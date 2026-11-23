// SAS MIDI Probe (see CMakeLists.txt): audio passes through untouched; every half second a note of a C major arpeggio is sent out as MIDI.
#include <juce_audio_processors/juce_audio_processors.h>

namespace {
class ProbeProcessor final : public juce::AudioProcessor {
public:
    ProbeProcessor() : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

    const juce::String getName() const override { return "SAS MIDI Probe"; }
    void prepareToPlay(double sampleRate, int) override {
        fs_ = sampleRate;
        position_ = 0;
        step_ = 0;
        playing_ = -1;
    }
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override {
        return l.getMainInputChannelSet() == l.getMainOutputChannelSet() && !l.getMainOutputChannelSet().isDisabled();
    }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override {
        midi.clear();  // whatever came in is not forwarded: the host should only see what the probe makes
        const int n = buffer.getNumSamples();
        const int half = int(fs_ * 0.5), noteLen = int(fs_ * 0.35);
        static constexpr int kNotes[8] = {60, 64, 67, 72, 67, 64, 60, 55};
        for (int i = 0; i < n; ++i, ++position_) {
            if (playing_ >= 0 && position_ % half == noteLen) {
                midi.addEvent(juce::MidiMessage::noteOff(1, playing_), i);
                playing_ = -1;
            }
            if (position_ % half == 0) {
                playing_ = kNotes[step_++ % 8];
                midi.addEvent(juce::MidiMessage::noteOn(1, playing_, juce::uint8(100)), i);
            }
        }
    }
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    double fs_ = 48000.0;
    juce::int64 position_ = 0;
    int step_ = 0, playing_ = -1;
};
}  // namespace

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ProbeProcessor(); }
