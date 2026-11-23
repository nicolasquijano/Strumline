// strumline_eval <folder> [seconds] [mono|...]: runs the converter on every <id>.wav of the folder (made by tools/extract_guitarset.py) and compares the note-ons
// with the notes the player played (<id>.txt): precision, recall and F of the onsets (the same MIDI note, within 50 ms), how many of the misses are an octave
// off, and the latency of the notes it got right (from the real onset to the note-on, without anything the host adds).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include <juce_audio_utils/juce_audio_utils.h>

#include "basic_pitch.h"
#include "mono_transcriber.h"
#include "note_merger.h"
#if STRUMLINE_POLY
#include "nmp_model.h"
#endif

namespace {
struct Ref { double on, off; int midi; };

std::vector<Ref> readRefs(const juce::File& f, double seconds) {
    std::vector<Ref> v;
    juce::StringArray lines;
    f.readLines(lines);
    for (const auto& l : lines) {
        const auto t = juce::StringArray::fromTokens(l, " ", "");
        if (t.size() == 3 && t[0].getDoubleValue() < seconds) v.push_back({t[0].getDoubleValue(), t[1].getDoubleValue(), t[2].getIntValue()});
    }
    return v;
}
}  // namespace

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::printf("usage: strumline_eval <folder with .wav and .txt> [seconds]\n"); return 1; }
    const juce::File dir(argv[1]);
    const double seconds = argc > 2 ? std::atof(argv[2]) : 15.0;
    const float sens = argc > 3 ? float(std::atof(argv[3])) : -54.0f;
    const double key = argc > 4 ? std::atof(argv[4]) : 0.6;
    const float rise = argc > 5 ? float(std::atof(argv[5])) : 8.0f, hold = argc > 6 ? float(std::atof(argv[6])) : 80.0f;
    // argv[7]: mono | poly | hybrid (default hybrid); argv[8]: look-ahead of the chord converter in frames of 11.6 ms (default 6); argv[9]: onset threshold.
    const std::string modeName = argc > 7 ? argv[7] : "hybrid";
    const gm::NoteMerger::Mode mode = modeName == "mono" ? gm::NoteMerger::Mode::Mono : modeName == "poly" ? gm::NoteMerger::Mode::Poly : gm::NoteMerger::Mode::Hybrid;
    const int lookFrames = argc > 8 ? std::atoi(argv[8]) : 6;
    const float onsetThr = argc > 9 ? float(std::atof(argv[9])) : 0.5f;
    const int hopFrames = argc > 12 ? std::atoi(argv[12]) : 3;
    const int idleHop = argc > 13 ? std::atoi(argv[13]) : 8;
    const float burst = argc > 14 ? float(std::atof(argv[14])) : 0.3f, riseDb = argc > 15 ? float(std::atof(argv[15])) : 4.0f;
    const double clarity = argc > 10 ? std::atof(argv[10]) : 0.0, clarityChord = argc > 11 ? std::atof(argv[11]) : 0.97;
    gm::PolyTranscriber poly;
    gm::NoteMerger merger;
    bool polyOk = false;
#if STRUMLINE_POLY
    polyOk = poly.load(gm::kNmpModel, gm::kNmpModelSize, 4);
#endif
    std::printf("mode %s (chord converter %s), look-ahead %d frames\n", modeName.c_str(), polyOk ? "loaded" : "NOT available", lookFrames);
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    int tp = 0, nOn = 0, nRef = 0, octave = 0, missNone = 0;
    std::map<int, int> intervals;
    std::vector<double> lat;
    long long runsTotal = 0;
    double audioSeconds = 0.0;
    for (const auto& wav : dir.findChildFiles(juce::File::findFiles, false, "*.wav")) {
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(wav));
        if (!reader) continue;
        const double fs = reader->sampleRate;
        const int total = int(std::min<juce::int64>(reader->lengthInSamples, juce::int64(seconds * fs)));
        juce::AudioBuffer<float> audio(1, total);
        reader->read(&audio, 0, total, 0, true, false);

        gm::MonoTranscriber t;
        t.prepare(fs);
        t.setSensitivityDb(sens);
        t.setKeyThreshold(key);
        t.setRiseDb(rise);
        t.setHoldoffMs(hold);
        poly.prepare(fs);
        poly.setSensitivityDb(sens);
        poly.setLookaheadFrames(lookFrames);
        poly.setOnsetThreshold(onsetThr);
        poly.setHopFrames(hopFrames);
        poly.setIdle(idleHop, burst, riseDb);
        merger.reset();
        merger.setSampleRate(fs);
        merger.setMode(mode);
        merger.setClarities(clarity, clarityChord, 1.0);
        merger.setPolyAvailable(polyOk);
        std::vector<gm::MonoTranscriber::Event> ev, all, merged;
        std::vector<gm::PolyEvent> pev;
        std::vector<double> times, match;  // when each note was sent; and the moment it is compared with the real onset (where the chord converter says it began)
        for (int s = 0; s < total; s += 512) {
            const int n = std::min(512, total - s);
            ev.clear();
            t.setMinClarity(merger.clarityNeeded(s));
            t.process(audio.getReadPointer(0) + s, n, ev);
            merged.clear();
            for (auto& e : ev) merger.onMono(e, s + e.offset, merged);
            pev.clear();
            if (polyOk && mode != gm::NoteMerger::Mode::Mono) poly.feed(audio.getReadPointer(0) + s, n, pev);
            for (auto& e : pev) merger.onPoly(e, e.hostSample, std::clamp(int(e.hostSample - s), 0, n - 1), merged);
            merger.update(s + n, n - 1, merged);
            for (auto& e : merged) { all.push_back(e); times.push_back(double(s + e.offset) / fs); match.push_back(e.onsetAt >= 0 ? double(e.onsetAt) / fs : times.back()); }
        }
        runsTotal = poly.runs();
        audioSeconds += double(total) / fs;
        const auto refs = readRefs(wav.withFileExtension("txt"), seconds);
        std::vector<bool> used(refs.size(), false);
        int fileTp = 0, fileOn = 0, fileOct = 0;
        for (std::size_t k = 0; k < all.size(); ++k) {
            if (all[k].type != gm::MonoTranscriber::Event::NoteOn) continue;
            ++fileOn;
            // A note counts when it is the right one and starts from 50 ms before to 120 ms after the real onset (for the one-note converter the start is when it answers; the chord converter says where the note began, as the tables in the plan do). The latency is always when it was sent.
            int best = -1;
            double bd = 1e9;
            for (std::size_t i = 0; i < refs.size(); ++i) {
                if (used[i] || refs[i].midi != all[k].note) continue;
                const double d = match[k] - refs[i].on;
                if (d >= -0.05 && d <= (all[k].onsetAt >= 0 ? 0.05 : 0.12) && std::abs(d) < bd) { bd = std::abs(d); best = int(i); }
            }
            if (best >= 0) { used[std::size_t(best)] = true; ++fileTp; lat.push_back((times[k] - refs[std::size_t(best)].on) * 1000.0); }
            else {  // not matched: is it the right moment with the wrong octave?
                for (std::size_t i = 0; i < refs.size(); ++i)
                    if (!used[i] && std::abs(refs[i].midi - all[k].note) % 12 == 0 && refs[i].midi != all[k].note && match[k] - refs[i].on >= -0.05 && match[k] - refs[i].on <= (all[k].onsetAt >= 0 ? 0.05 : 0.12)) { ++fileOct; break; }
            }
        }
        // The notes that were not found: nothing was sent near their onset, or a note of another pitch was (the interval is kept).
        for (std::size_t i = 0; i < refs.size(); ++i) {
            if (used[i]) continue;
            bool seen = false;
            int interval = 0;
            for (std::size_t k = 0; k < all.size() && !seen; ++k) {
                if (all[k].type != gm::MonoTranscriber::Event::NoteOn) continue;
                const double d = match[k] - refs[i].on;
                if (d >= -0.05 && d <= (all[k].onsetAt >= 0 ? 0.05 : 0.12)) { seen = true; interval = all[k].note - refs[i].midi; }
            }
            if (seen) ++intervals[interval]; else ++missNone;
        }
        std::printf("%-26s notes %3d  sent %3d  right %3d  octave errors %2d  F %.2f  latency %.0f ms\n", wav.getFileNameWithoutExtension().toRawUTF8(), int(refs.size()), fileOn, fileTp,
                    fileOct, 2.0 * fileTp / std::max(1, fileOn + int(refs.size())), t.latencyMs());
        tp += fileTp; nOn += fileOn; nRef += int(refs.size()); octave += fileOct;
    }
    std::sort(lat.begin(), lat.end());
    const double p = double(tp) / std::max(1, nOn), r = double(tp) / std::max(1, nRef);
    std::printf("\nTOTAL  precision %.2f  recall %.2f  F %.2f  octave errors %d of %d wrong  latency median %.0f ms  p90 %.0f ms\n", p, r, 2 * p * r / std::max(1e-9, p + r), octave, nOn - tp,
                lat.empty() ? 0.0 : lat[lat.size() / 2], lat.empty() ? 0.0 : lat[std::min(lat.size() - 1, std::size_t(double(lat.size()) * 0.9))]);
    std::printf("model runs: %lld in %.0f s of audio = %.1f per second (each is about 30 ms of one core)\n", runsTotal, audioSeconds, double(runsTotal) / std::max(1.0, audioSeconds));
    std::printf("not found: %d with no note-on near their onset; with another pitch (interval in semitones: count):", missNone);
    for (const auto& kv : intervals) if (kv.second >= 3) std::printf("  %+d: %d", kv.first, kv.second);
    std::printf("\n");
    return 0;
}
