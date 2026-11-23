// strumline_check: runs the processor without a host and checks what it sends: the MIDI notes of the test script (with the transposition, the channel and the
// velocity curve), the monitor synth, the through switch and the all-notes-off when the test stops. Prints PASS or FAIL for each; exit code 1 on any failure.
#include <cmath>
#include <ctime>
#include <cstdio>
#include <chrono>
#include <map>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <juce_audio_utils/juce_audio_utils.h>

#include "I18n.h"
#include "StrumlineProcessor.h"

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

struct Run {
    int on = 0, off = 0, allOff = 0, channel = 0, firstNote = -1, firstVelocity = -1, lastOnNote = -1;
    double rmsOut = 0.0;
    int blocks = 0;
};

void setParam(StrumlineProcessor& p, const char* id, float value) {
    auto* prm = p.apvts.getParameter(id);
    prm->setValueNotifyingHost(prm->convertTo0to1(value));
}

// `seconds` of a 110 Hz sine at 0.1 through the processor; `silentInput` feeds zeros (to hear only the synth).
Run run(StrumlineProcessor& p, double seconds, bool silentInput) {
    Run r;
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    double e = 0.0;
    long n = 0, phase = 0;
    for (int b = 0; b < int(seconds * 48000.0 / 512.0); ++b, ++r.blocks) {
        for (int i = 0; i < 512; ++i, ++phase) {
            const float s = silentInput ? 0.0f : 0.1f * std::sin(2.0f * 3.14159265f * 110.0f * float(phase) / 48000.0f);
            buf.setSample(0, i, s);
            buf.setSample(1, i, s);
        }
        p.processBlock(buf, midi);
        for (const auto meta : midi) {
            const auto m = meta.getMessage();
            if (m.isNoteOn()) {
                ++r.on;
                r.channel = m.getChannel();
                if (r.firstNote < 0) { r.firstNote = m.getNoteNumber(); r.firstVelocity = m.getVelocity(); }
                r.lastOnNote = m.getNoteNumber();
            } else if (m.isNoteOff()) {
                ++r.off;
            } else if (m.isAllNotesOff()) {
                ++r.allOff;
            }
        }
        for (int i = 0; i < 512; ++i) { e += double(buf.getSample(0, i)) * buf.getSample(0, i); ++n; }
    }
    r.rmsOut = std::sqrt(e / double(std::max(1L, n)));
    return r;
}
}  // namespace

// A plucked string (Karplus-Strong, 48 kHz), roughly guitar-like.
std::vector<float> pluck(int midi, double seconds, unsigned seed) {
    const double f = 440.0 * std::pow(2.0, (midi - 69) / 12.0);
    const int period = int(std::lround(48000.0 / f)), n = int(seconds * 48000.0);
    std::vector<float> buf(static_cast<std::size_t>(period)), out(static_cast<std::size_t>(n));
    unsigned s = seed * 2654435761u + 1u;
    for (auto& b : buf) { s = s * 1664525u + 1013904223u; b = float(s >> 8) / float(1u << 23) - 1.0f; }
    for (int i = 0; i < n; ++i) {
        out[std::size_t(i)] = buf[std::size_t(i % period)];
        buf[std::size_t(i % period)] = 0.4985f * (buf[std::size_t(i % period)] + buf[std::size_t((i + 1) % period)]);
    }
    return out;
}

int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    {
        // The languages: every table translates the same texts, a missing text falls back to English, and the system language is mapped.
        using i18n::Language;
        const auto spanish = i18n::translationCount(Language::Spanish);
        bool same = spanish > 40;
        for (auto l : {Language::Portuguese, Language::Italian, Language::Chinese, Language::French, Language::German, Language::Japanese, Language::Russian, Language::Korean})
            same = same && i18n::translationCount(l) == spanish;
        check(same, "the nine languages translate the same number of texts");
        i18n::useForSession(Language::Spanish);
        check(tr("PANIC") == juce::String::fromUTF8("P\xc3\x81NICO"), "Spanish: PANIC is PÁNICO");
        check(tr("a text that is not in any table") == "a text that is not in any table", "a text without a translation stays in English");
        i18n::useForSession(Language::Japanese);
        check(tr("PANIC") != "PANIC" && tr("PANIC").isNotEmpty(), "Japanese: PANIC is translated");
        i18n::useForSession(Language::English);
        check(tr("PANIC") == "PANIC", "English: the text is the one in the source");
        check(i18n::fromSystemCode("es_AR") == Language::Spanish && i18n::fromSystemCode("pt-BR") == Language::Portuguese && i18n::fromSystemCode("zh_CN") == Language::Chinese &&
                  i18n::fromSystemCode("ko") == Language::Korean && i18n::fromSystemCode("sv_SE") == Language::English,
              "system language codes map to the interface languages (English for the rest)");
    }
    {
        StrumlineProcessor p;
        p.setPlayConfigDetails(2, 2, 48000.0, 512);
        p.prepareToPlay(48000.0, 512);
        check(p.producesMidi() && !p.acceptsMidi(), "the processor produces MIDI and takes none");

        setParam(p, ids::synth, 0.0f);  // to hear only the guitar that goes through
        auto idle = run(p, 1.0, false);
        check(idle.on == 1 && idle.firstNote == 45, "the converter hears a 110 Hz sine as one A2 (45)");
        check(std::abs(idle.rmsOut - 0.1 / std::sqrt(2.0)) < 0.002, "the guitar goes through untouched (thru on)");

        setParam(p, ids::thru, 0.0f);
        check(run(p, 0.5, false).rmsOut < 1e-6, "thru off: silence in the output");
        setParam(p, ids::thru, 1.0f);
        setParam(p, ids::synth, 1.0f);

        p.setTestRunning(true);
        auto r = run(p, 9.0, true);  // one whole loop of the script: 8 single notes and a chord of 5
        check(r.on == 13 && r.off >= 13, "the test script plays 13 notes and ends all of them");
        check(r.channel == 1 && r.firstNote == 57 && r.firstVelocity == 100, "channel 1, first note A3 (57) at velocity 100");
        check(r.rmsOut > 0.005, "the monitor synth is heard (silent input)");
        check(p.voices() == 0, "no voice left sounding at the end of the loop");

        setParam(p, ids::synth, 0.0f);
        check(run(p, 5.0, true).rmsOut < 1e-6, "synth off: the notes go out as MIDI but nothing is heard");
        setParam(p, ids::synth, 1.0f);

        auto restart = [&] {  // stops and starts the script: it begins again at its first note
            p.setTestRunning(false);
            run(p, 0.1, true);
            p.setTestRunning(true);
        };
        setParam(p, ids::channel, 5.0f);
        setParam(p, ids::transpose, 12.0f);
        setParam(p, ids::velocity, 0.0f);  // soft
        restart();
        auto t = run(p, 1.0, true);
        check(t.channel == 5 && t.firstNote == 69, "channel 5 and transposed an octave up (57 becomes 69)");
        check(t.firstVelocity > 100, "the soft velocity curve lifts the velocity (100 becomes more)");
        setParam(p, ids::velocity, 2.0f);  // hard
        restart();
        auto h = run(p, 1.0, true);
        check(h.on > 0 && h.firstVelocity < 100, "the hard velocity curve asks for more (100 becomes less)");
        setParam(p, ids::velocity, 1.0f);

        // Stop in the middle of the chord (the loop is 9 s long and the chord starts at 4.5 s): everything must be switched off.
        restart();
        run(p, 5.0, true);
        check(p.voices() == 5, "the chord of five notes is sounding");
        p.setTestRunning(false);
        auto s = run(p, 0.2, true);
        check(s.allOff == 1, "stopping the test sends all-notes-off");
        check(p.voices() == 0, "...and no voice is left");
    }
    {
        // The converter on the six open strings of a guitar (Karplus-Strong plucks, one a second), then on the noise of an interface.
        StrumlineProcessor p;
        p.setPlayConfigDetails(2, 2, 48000.0, 512);
        p.prepareToPlay(48000.0, 512);
        setParam(p, ids::mode, 0.0f);  // this test is about the one-note converter
        const int strings[6] = {40, 45, 50, 55, 59, 64};
        std::vector<float> signal(std::size_t(48000 * 8), 0.0f);
        for (int k = 0; k < 6; ++k) {
            const auto s = pluck(strings[k], 0.9, unsigned(k + 1));
            for (std::size_t i = 0; i < s.size(); ++i) signal[std::size_t(48000 * (k + 1)) + i] += 0.3f * s[i];
        }
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        std::vector<std::pair<long, int>> ons;
        for (std::size_t b = 0; b + 512 <= signal.size(); b += 512) {
            for (int i = 0; i < 512; ++i) { buf.setSample(0, i, signal[b + std::size_t(i)]); buf.setSample(1, i, signal[b + std::size_t(i)]); }
            p.processBlock(buf, midi);
            for (const auto meta : midi) if (meta.getMessage().isNoteOn()) ons.push_back({long(b) + meta.samplePosition, meta.getMessage().getNoteNumber()});
        }
        bool ok = ons.size() == 6;
        double worst = 0.0;
        for (std::size_t k = 0; k < ons.size() && k < 6; ++k) {
            const double ms = 1000.0 * double(ons[k].first - 48000L * long(k + 1)) / 48000.0;
            worst = std::max(worst, ms);
            ok = ok && ons[k].second == strings[k];
            std::printf("     string %zu: note %d, note-on %.0f ms after the pluck\n", k + 1, ons[k].second, ms);
        }
        std::printf("     notes sent: %zu\n", ons.size());
        check(ok, "the six open strings come out as E2 A2 D3 G3 B3 E4, one note each");
        check(worst < 70.0, "every note-on comes less than 70 ms after the pluck");
        check(p.latencyMs() > 0.0f && p.latencyMs() < 70.0f, "the latency the plug-in reports is measured and below 70 ms");

        StrumlineProcessor q;
        q.setPlayConfigDetails(2, 2, 48000.0, 512);
        q.prepareToPlay(48000.0, 512);
        setParam(q, ids::mode, 2.0f);  // the noise must not make notes in Hybrid either (the chord model is running)
        long noise = 0;
        unsigned s = 12345u;
        for (int b = 0; b < 200; ++b) {
            for (int i = 0; i < 512; ++i) { s = s * 1664525u + 1013904223u; const float x = 2e-4f * (float(s >> 8) / float(1u << 23) - 1.0f); buf.setSample(0, i, x); buf.setSample(1, i, x); }
            q.processBlock(buf, midi);
            for (const auto meta : midi) if (meta.getMessage().isNoteOn()) ++noise;
        }
        check(noise == 0, "the noise of an interface (-80 dBFS) plays no notes");
    }
    {
        // Chords: a strummed open E major (six Karplus-Strong strings, 15 ms apart) through the chord converter, paced at the speed of the audio because the
        // converter runs on its own thread. Poly must find the chord by itself; Hybrid too (the first string goes out fast, the rest arrive later).
        const int chord[6] = {40, 47, 52, 56, 59, 64};
        std::vector<float> signal(std::size_t(48000 * 3), 0.0f);
        for (int k = 0; k < 6; ++k) {
            const auto s = pluck(chord[k], 1.4, unsigned(k + 11));
            for (std::size_t i = 0; i < s.size(); ++i) signal[std::size_t(48000 / 2 + k * 720) + i] += 0.12f * s[i];
        }
        auto cpuSeconds = [] {
#if defined(_WIN32)
            FILETIME c, e, k, u;
            GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
            const auto sum = [](const FILETIME& t) { return double((ULONGLONG(t.dwHighDateTime) << 32) | t.dwLowDateTime) * 1e-7; };
            return sum(k) + sum(u);
#else
            return double(std::clock()) / double(CLOCKS_PER_SEC);  // on Linux and macOS this is the CPU time of the whole process
#endif
        };
        for (const int mode : {1, 2}) {
            const double cpu0 = cpuSeconds();
            const auto wall0 = std::chrono::steady_clock::now();
            StrumlineProcessor p;
            p.setPlayConfigDetails(2, 2, 48000.0, 512);
            p.prepareToPlay(48000.0, 512);
            if (mode == 1) check(p.chordsAvailable(), "the chord converter is loaded (ONNX Runtime and the model are there)");
            setParam(p, ids::mode, float(mode));
            juce::AudioBuffer<float> buf(2, 512);
            juce::MidiBuffer midi;
            std::map<int, long> first;
            int extra = 0;
            for (std::size_t b = 0; b + 512 <= signal.size(); b += 512) {
                for (int i = 0; i < 512; ++i) { buf.setSample(0, i, signal[b + std::size_t(i)]); buf.setSample(1, i, signal[b + std::size_t(i)]); }
                p.processBlock(buf, midi);
                for (const auto meta : midi)
                    if (meta.getMessage().isNoteOn()) {
                        const int n = meta.getMessage().getNoteNumber();
                        if (!first.count(n)) first[n] = long(b) + meta.samplePosition;
                        else ++extra;
                    }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            const double cpu = cpuSeconds() - cpu0, wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall0).count();
            std::printf("     CPU of the whole process during the chord test: %.0f%% of one core (%.2f s of CPU in %.2f s; the chord converter runs only while there is sound)\n", 100.0 * cpu / wall, cpu, wall);
            int found = 0, wrong = 0;
            for (const auto& kv : first) {
                bool inChord = false;
                for (int c : chord) inChord = inChord || c == kv.first;
                found += inChord ? 1 : 0;
                wrong += inChord ? 0 : 1;
            }
            std::printf("     %s: %d of 6 notes of the chord, %d other notes, %d repeats; chord model run %.1f ms\n", mode == 1 ? "Poly" : "Hybrid", found, wrong, extra, double(p.chordModelMs()));
            check(found >= 5 && wrong <= 1, mode == 1 ? "Poly: a strummed E major comes out as (at least) five of its six notes" : "Hybrid: the same chord, found as well");
            double worst = 0.0;
            for (int c : chord) if (first.count(c)) worst = std::max(worst, 1000.0 * double(first[c] - 24000L) / 48000.0);
            check(worst < 400.0, "every note of the chord arrives within 400 ms of the first pluck");
        }
    }
    std::printf("%s\n", failures == 0 ? "ALL PASSED" : "SOME FAILED");
    return failures == 0 ? 0 : 1;
}
