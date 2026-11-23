#pragma once
// Strumline: guitar to MIDI. This is the shell (phase 1 of docs/PLAN_GUITAR_MIDI.md): the parameters, the MIDI output, the monitor synth, the level meter and
// the interface are real; the converter that listens to the guitar is not written yet, so the notes come from a built-in test script (the TEST button).
#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "monitor_synth.h"
#include "mono_transcriber.h"
#include "note_bus.h"
#include "note_merger.h"
#include "poly_worker.h"

namespace ids {
inline constexpr const char* mode = "mode";            // 0 Mono, 1 Poly, 2 Hybrid
inline constexpr const char* sens = "sens";            // dBFS the input must reach to count as playing
inline constexpr const char* lookahead = "lookahead";  // ms of the future the converter may look at: latency against precision
inline constexpr const char* velocity = "velocity";    // 0 Soft, 1 Normal, 2 Hard
inline constexpr const char* channel = "channel";      // MIDI channel 1..16
inline constexpr const char* transpose = "transpose";  // semitones
inline constexpr const char* bend = "bend";            // 0 Off, 1 Channel, 2 MPE (MPE is not written yet: it sends like Channel)
inline constexpr const char* thru = "thru";            // the guitar goes on to the output (so Strumline can sit first in a chain)
inline constexpr const char* synth = "synth";          // the monitor synth is heard
inline constexpr const char* wave = "wave";            // 0 Sine, 1 Saw, 2 Square
inline constexpr const char* level = "level";          // dB of the monitor synth
inline constexpr const char* perf = "perf";            // 0 Fast (the chord model runs every 35 ms), 1 Saver (every 46 ms: a third less CPU, 8 ms later)
}  // namespace ids

class StrumlineProcessor final : public juce::AudioProcessor {
public:
    StrumlineProcessor();

    const juce::String getName() const override { return "Strumline"; }
    void prepareToPlay(double sampleRate, int maxBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layout) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // For the editor.
    gm::NoteBus& noteBus() { return bus_; }
    float inputLevelDb() const { return levelDb_.load(); }
    // The average latency of the notes the converter sent (ms, from the start of the sound to the note-on); 0 until there is one.
    float latencyMs() const { return mono_.latencyMs(); }
    int voices() const { return voices_.load(); }
    // True when the chord converter (ONNX Runtime and the model) is loaded; otherwise Poly and Hybrid play as Mono.
    bool chordsAvailable() const { return poly_.running(); }
    // Milliseconds the last run of the chord model took (0 until it has run).
    float chordModelMs() const { return poly_.lastRunMs(); }
    // Milliseconds from the start of a chord note to its note-on (the model run included), smoothed; 0 until there is one.
    float chordLatencyMs() const { return chordLatency_.load(); }
    // How long the chord model has run in total (ms): the editor turns the difference over time into a load figure.
    double chordBusyMs() const { return poly_.busyMs(); }
    // The pitch of the note the one-note converter holds, in cents away from the note (0 when it holds none); for the strings.
    float centsNow() const { return cents_.load(); }
    // All notes off now, and every converter starts again (a hung note in the synth).
    void panic() { panic_.store(true); }
    // The built-in test script (an arpeggio and a chord, nine seconds, in a loop): lets the whole chain (MIDI out, monitor synth, drawing) be tried
    // without the converter. Not part of the saved state.
    void setTestRunning(bool on) { test_.store(on); }
    bool testRunning() const { return test_.load(); }

private:
    struct TestNote { double start, length; int midi; };
    static const std::array<TestNote, 13>& testScript();
    static constexpr double kTestLoopSeconds = 9.0;

    void emit(juce::MidiBuffer& midi, bool on, int note, int velocity, int offset);
    void allNotesOff(juce::MidiBuffer& midi, int offset);

    double fs_ = 48000.0;
    gm::MonitorSynth synth_;
    gm::MonoTranscriber mono_;
    gm::PolyWorker poly_;
    gm::NoteMerger merger_;
    std::vector<gm::MonoTranscriber::Event> merged_;
    long long blockStart_ = 0;
    double polyRate_ = 0.0;
    gm::NoteMerger::Mode lastMode_ = gm::NoteMerger::Mode::Hybrid;
    std::vector<gm::MonoTranscriber::Event> events_;
    std::vector<float> mix_;
    std::array<int, 128> outOf_{};  // the note that was sent for each note on, so its note-off matches even if the transposition changes
    gm::NoteBus bus_;
    std::atomic<float> levelDb_{-120.0f};
    std::atomic<int> voices_{0};
    std::atomic<bool> test_{false}, panic_{false};
    std::atomic<float> chordLatency_{0.0f}, cents_{0.0f};
    bool testWasOn_ = false;
    juce::int64 testPos_ = 0;
    std::array<bool, 128> held_{};
    std::vector<float> scratch_;
};
