#include "StrumlineProcessor.h"

#include <algorithm>
#include <cmath>

#include "StrumlineEditor.h"

namespace {
constexpr int kVersion = 1;

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout() {
    using P = juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add(std::make_unique<juce::AudioParameterChoice>(P{ids::mode, kVersion}, "Mode", juce::StringArray{"Mono", "Poly", "Hybrid"}, 2));
    l.add(std::make_unique<juce::AudioParameterFloat>(P{ids::sens, kVersion}, "Sensitivity", juce::NormalisableRange<float>(-75.0f, -35.0f, 1.0f), -54.0f,
                                                      juce::AudioParameterFloatAttributes().withLabel("dB")));
    l.add(std::make_unique<juce::AudioParameterFloat>(P{ids::lookahead, kVersion}, "Look-ahead", juce::NormalisableRange<float>(0.0f, 120.0f, 5.0f), 60.0f,
                                                      juce::AudioParameterFloatAttributes().withLabel("ms")));
    l.add(std::make_unique<juce::AudioParameterChoice>(P{ids::velocity, kVersion}, "Velocity", juce::StringArray{"Soft", "Normal", "Hard"}, 1));
    l.add(std::make_unique<juce::AudioParameterInt>(P{ids::channel, kVersion}, "MIDI channel", 1, 16, 1));
    l.add(std::make_unique<juce::AudioParameterInt>(P{ids::transpose, kVersion}, "Transpose", -24, 24, 0));
    l.add(std::make_unique<juce::AudioParameterChoice>(P{ids::bend, kVersion}, "Pitch bend", juce::StringArray{"Off", "Channel", "MPE"}, 1));
    l.add(std::make_unique<juce::AudioParameterBool>(P{ids::thru, kVersion}, "Guitar through", true));
    l.add(std::make_unique<juce::AudioParameterBool>(P{ids::synth, kVersion}, "Monitor synth", true));
    l.add(std::make_unique<juce::AudioParameterChoice>(P{ids::wave, kVersion}, "Synth wave", juce::StringArray{"Sine", "Saw", "Square"}, 1));
    l.add(std::make_unique<juce::AudioParameterFloat>(P{ids::level, kVersion}, "Synth level", juce::NormalisableRange<float>(-40.0f, 0.0f, 1.0f), -12.0f,
                                                      juce::AudioParameterFloatAttributes().withLabel("dB")));
    l.add(std::make_unique<juce::AudioParameterChoice>(P{ids::perf, kVersion}, "Chord rate", juce::StringArray{"Fast", "Saver"}, 0));
    return l;
}
}  // namespace

StrumlineProcessor::StrumlineProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STRUMLINE", makeLayout()) {}

const std::array<StrumlineProcessor::TestNote, 13>& StrumlineProcessor::testScript() {
    // Eight single notes of an A minor arpeggio (0 to 4 s), then a chord of five notes (4.5 to 7 s).
    static const std::array<TestNote, 13> s = {{{0.0, 0.40, 57}, {0.5, 0.40, 60}, {1.0, 0.40, 64}, {1.5, 0.40, 69}, {2.0, 0.40, 64}, {2.5, 0.40, 60},
                                                {3.0, 0.40, 57}, {3.5, 0.80, 52}, {4.5, 2.50, 40}, {4.5, 2.50, 47}, {4.5, 2.50, 52}, {4.5, 2.50, 56},
                                                {4.5, 2.50, 59}}};
    return s;
}

void StrumlineProcessor::prepareToPlay(double sampleRate, int maxBlock) {
    fs_ = sampleRate;
    synth_.prepare(sampleRate);
    scratch_.assign(std::size_t(std::max(1, maxBlock)), 0.0f);
    mix_.assign(std::size_t(std::max(1, maxBlock)), 0.0f);
    events_.reserve(256);
    mono_.prepare(sampleRate);
    mono_.setKeyThreshold(0.6);  // measured on GuitarSet: 0.9 picks the lower octave too often (docs/PLAN_GUITAR_MIDI.md)
    // The chord converter: loads ONNX Runtime and the model (not again when only the block size changed). Without them the plug-in plays as Mono.
    if (!poly_.running() || polyRate_ != sampleRate) {
        poly_.start(sampleRate);
        polyRate_ = sampleRate;
    } else {
        poly_.requestReset();
    }
    merger_.setSampleRate(sampleRate);
    merger_.setPolyAvailable(poly_.running());
    merger_.reset();
    merged_.reserve(256);
    blockStart_ = 0;
    outOf_.fill(0);
    testPos_ = 0;
    testWasOn_ = false;
    held_.fill(false);
    voices_.store(0);
}

bool StrumlineProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainInputChannelSet() == l.getMainOutputChannelSet() && !l.getMainOutputChannelSet().isDisabled() && l.getMainOutputChannelSet().size() <= 2;
}

void StrumlineProcessor::emit(juce::MidiBuffer& midi, bool on, int note, int velocity, int offset) {
    const int shift = int(apvts.getRawParameterValue(ids::transpose)->load());
    const int out = on ? juce::jlimit(0, 127, note + shift) : (outOf_[std::size_t(note)] > 0 ? outOf_[std::size_t(note)] : juce::jlimit(0, 127, note + shift));
    if (on) outOf_[std::size_t(note)] = out;
    const int ch = juce::jlimit(1, 16, int(apvts.getRawParameterValue(ids::channel)->load()));
    if (on) {
        // The velocity curve: Soft lifts the quiet notes, Hard asks for more.
        const float curve[3] = {0.7f, 1.0f, 1.4f};
        const int shape = juce::jlimit(0, 2, int(apvts.getRawParameterValue(ids::velocity)->load() + 0.5f));
        const int v = juce::jlimit(1, 127, int(std::lround(127.0 * std::pow(float(velocity) / 127.0f, curve[shape]))));
        midi.addEvent(juce::MidiMessage::noteOn(ch, out, juce::uint8(v)), offset);
        synth_.noteOn(out, v);
        held_[std::size_t(out)] = true;
        bus_.push({true, out, v});
    } else {
        midi.addEvent(juce::MidiMessage::noteOff(ch, out), offset);
        synth_.noteOff(out);
        held_[std::size_t(out)] = false;
        bus_.push({false, out, 0});
    }
}

void StrumlineProcessor::allNotesOff(juce::MidiBuffer& midi, int offset) {
    const int ch = juce::jlimit(1, 16, int(apvts.getRawParameterValue(ids::channel)->load()));
    for (int n = 0; n < 128; ++n)
        if (held_[std::size_t(n)]) {
            midi.addEvent(juce::MidiMessage::noteOff(ch, n), offset);
            bus_.push({false, n, 0});
            held_[std::size_t(n)] = false;
        }
    midi.addEvent(juce::MidiMessage::allNotesOff(ch), offset);
    synth_.allOff();
}

void StrumlineProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const int n = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();

    // The level of the input, for the meter.
    double e = 0.0;
    for (int c = 0; c < channels; ++c) {
        const float* x = buffer.getReadPointer(c);
        for (int i = 0; i < n; ++i) e += double(x[i]) * x[i];
    }
    const float db = float(10.0 * std::log10(e / double(std::max(1, n * std::max(1, channels))) + 1e-12));
    levelDb_.store(db);

    // The guitar for the converter (before THRU can silence the buffer): the mix of the channels.
    if (std::size_t(n) > mix_.size()) mix_.assign(std::size_t(n), 0.0f);
    for (int i = 0; i < n; ++i) {
        float s = 0.0f;
        for (int c = 0; c < channels; ++c) s += buffer.getReadPointer(c)[i];
        mix_[std::size_t(i)] = channels > 0 ? s / float(channels) : 0.0f;
    }

    if (apvts.getRawParameterValue(ids::thru)->load() < 0.5f)
        buffer.clear();

    // The notes. For now only the test script makes them.
    const bool test = test_.load();
    const auto mode = gm::NoteMerger::Mode(juce::jlimit(0, 2, int(apvts.getRawParameterValue(ids::mode)->load() + 0.5f)));
    if (panic_.exchange(false) || testWasOn_ != test || lastMode_ != mode) {  // the built-in script and the converter take turns (and so do the modes): whatever sounded is switched off
        allNotesOff(midi, 0);
        testPos_ = 0;
        mono_.reset();
        merger_.reset();
        poly_.requestReset();
    }
    testWasOn_ = test;
    lastMode_ = mode;

    synth_.setWave(int(apvts.getRawParameterValue(ids::wave)->load() + 0.5f));
    const bool synthOn = apvts.getRawParameterValue(ids::synth)->load() > 0.5f;
    const float gain = synthOn ? std::pow(10.0f, apvts.getRawParameterValue(ids::level)->load() / 20.0f) : 0.0f;
    if (std::size_t(n) > scratch_.size()) scratch_.assign(std::size_t(n), 0.0f);
    std::fill(scratch_.begin(), scratch_.begin() + n, 0.0f);

    int done = 0;
    auto renderTo = [&](int upTo) {
        if (upTo > done) synth_.render(scratch_.data() + done, upTo - done, gain);
        done = std::max(done, upTo);
    };
    if (test) {
        const juce::int64 loop = juce::int64(kTestLoopSeconds * fs_);
        for (int i = 0; i < n; ++i, ++testPos_) {
            const juce::int64 pos = testPos_ % loop;
            for (const auto& t : testScript()) {
                const juce::int64 start = juce::int64(t.start * fs_), end = start + juce::int64(t.length * fs_);
                if (pos == start) { renderTo(i); emit(midi, true, t.midi, 100, i); }
                if (pos == end) { renderTo(i); emit(midi, false, t.midi, 0, i); }
            }
        }
    }
    if (!test) {
        // The converters: the one-note converter here, the chord converter on its own thread; the merger decides what is sent (Mono, Poly or Hybrid).
        const float sens = apvts.getRawParameterValue(ids::sens)->load();
        mono_.setSensitivityDb(sens);
        merger_.setMode(mode);
        mono_.setMinClarity(merger_.clarityNeeded(blockStart_));
        poly_.setSensitivityDb(sens);
        poly_.setLookaheadFrames(int(std::lround(apvts.getRawParameterValue(ids::lookahead)->load() / 11.61f)));
        poly_.setActive(mode != gm::NoteMerger::Mode::Mono);
        poly_.setHopFrames(apvts.getRawParameterValue(ids::perf)->load() > 0.5f ? 4 : 3);
        poly_.push(mix_.data(), n);
        events_.clear();
        mono_.process(mix_.data(), n, events_);
        merged_.clear();
        for (const auto& e : events_) merger_.onMono(e, blockStart_ + e.offset, merged_);
        for (gm::PolyEvent pe; poly_.pop(pe);) {  // the chord converter answers after the block it heard
            merger_.onPoly(pe, blockStart_, 0, merged_);
            if (pe.on) {
                const float ms = float(1000.0 * double(pe.hostSample - pe.onsetHost) / fs_) + poly_.lastRunMs();
                const float old = chordLatency_.load();
                chordLatency_.store(old <= 0.0f ? ms : old + 0.2f * (ms - old));
            }
        }
        merger_.update(blockStart_ + n, n - 1, merged_);
        const bool bendOn = apvts.getRawParameterValue(ids::bend)->load() > 0.5f;
        const int ch = juce::jlimit(1, 16, int(apvts.getRawParameterValue(ids::channel)->load()));
        std::stable_sort(merged_.begin(), merged_.end(), [](const auto& a, const auto& b) { return a.offset < b.offset; });
        for (const auto& e : merged_) {
            renderTo(e.offset);
            switch (e.type) {
                case gm::MonoTranscriber::Event::NoteOn: emit(midi, true, e.note, e.velocity, e.offset); break;
                case gm::MonoTranscriber::Event::NoteOff:
                    emit(midi, false, e.note, 0, e.offset);
                    synth_.setBend(0.0f);
                    cents_.store(0.0f);
                    break;
                case gm::MonoTranscriber::Event::Bend:
                    if (bendOn) midi.addEvent(juce::MidiMessage::pitchWheel(ch, e.bend14), e.offset);
                    synth_.setBend(float(e.bend14 - 8192) / 8191.0f * 2.0f);  // the converter's bend range is two semitones
                    cents_.store(float(e.bend14 - 8192) / 8191.0f * 200.0f);
                    bus_.push({false, 0, 0, e.bend14});
                    break;
            }
        }
    }
    renderTo(n);
    blockStart_ += n;
    for (int c = 0; c < channels; ++c) juce::FloatVectorOperations::add(buffer.getWritePointer(c), scratch_.data(), n);

    int count = 0;
    for (bool h : held_) count += h ? 1 : 0;
    voices_.store(count);
}

juce::AudioProcessorEditor* StrumlineProcessor::createEditor() { return new StrumlineEditor(*this); }

void StrumlineProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void StrumlineProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new StrumlineProcessor(); }
