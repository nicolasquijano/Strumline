#pragma once
// The converter for one note at a time: it listens to the guitar and says when a note starts (with its velocity), how the pitch bends and when it ends.
//
//  - The input is brought down to about 24 kHz (a box average and a low pass: the harmonics above 6 kHz do not matter for the pitch).
//  - Every 4 ms the level of the last 5 ms is measured. An onset is a rise of more than 8 dB over the level of 10 to 40 ms ago, above the sensitivity.
//  - After an onset the pitch is looked for in the audio since the onset, as soon as there are two and a half periods of the lowest note it could be: a
//    high note is heard 10 ms after it starts, an open low E after 35 ms. The note is sent when three readings in a row agree and the window holds three
//    periods of what they say, so the latency of every note is as short as the physics of its pitch allows.
//  - While the note sounds the pitch is followed (a bend of up to the bend range), and the note ends when the level drops below the sensitivity or a new
//    onset comes.
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

#include "pitch_detector.h"

namespace gm {

class MonoTranscriber {
public:
    struct Event {
        enum Type { NoteOn, NoteOff, Bend } type;
        int note = 0;
        int velocity = 0;
        int bend14 = 8192;  // pitch wheel, 0..16383 with 8192 at the centre
        int offset = 0;     // the sample of the block it belongs to
        long long onsetAt = -1;  // for a note of the chord converter: the host sample where the note started (the note itself is sent later); -1 when it is the moment of the event
    };

    void prepare(double hostRate) {
        factor_ = std::max(1, int(std::lround(hostRate / 24000.0)));
        rate_ = hostRate / double(factor_);
        hop_ = std::max(16, int(std::lround(rate_ * 0.004)));
        fastLen_ = std::max(16, int(std::lround(rate_ * 0.005)));
        ring_.assign(kRing, 0.0f);
        reset();
    }
    void reset() {
        std::fill(ring_.begin(), ring_.end(), 0.0f);
        write_ = 0;
        acc_ = 0.0;
        count_ = 0;
        lp_ = 0.0;
        sinceHop_ = 0;
        history_.fill(-120.0f);
        state_ = State::Idle;
        holdoff_ = 0;
        latencyMs_.store(0.0f);
    }

    void setSensitivityDb(float db) { sensDb_ = db; }
    void setBendRange(float semitones) { bendRange_ = std::max(0.5f, semitones); }
    void setReferenceA(double hz) { refA_ = hz; }
    void setKeyThreshold(double k) { detector_.keyThreshold = k; }
    void setRiseDb(float db) { riseDb_ = db; }
    // A new note is only sent when the pitch reading is this clean (0..1); a chord reads as a muddy pitch, a single note as a clean one.
    void setMinClarity(double c) { minClarity_ = c; }
    void setHoldoffMs(float ms) { holdoffMs_ = ms; }

    // Average latency (ms) of the notes sent so far: from the start of the sound to the note-on (0 until there is one).
    float latencyMs() const { return latencyMs_.load(); }
    bool noteSounding() const { return state_ == State::Note; }

    // `n` samples of the guitar (host rate). The events come out in time order, with the sample they belong to.
    void process(const float* in, int n, std::vector<Event>& out) {
        for (int i = 0; i < n; ++i) {
            acc_ += double(in[i]);
            if (++count_ < factor_) continue;
            const double v = acc_ / double(factor_);
            acc_ = 0.0;
            count_ = 0;
            lp_ += 0.7 * (v - lp_);
            ring_[write_ & (kRing - 1)] = float(lp_);
            ++write_;
            if (++sinceHop_ >= hop_) {
                sinceHop_ = 0;
                step(i, out);
            }
        }
    }

private:
    enum class State { Idle, Attack, Note };
    static constexpr std::size_t kRing = 8192;

    float fastDb() const {
        double e = 0.0;
        const std::size_t n = std::size_t(fastLen_);
        for (std::size_t k = 0; k < n; ++k) {
            const double x = ring_[(write_ - 1 - k) & (kRing - 1)];
            e += x * x;
        }
        return float(10.0 * std::log10(e / double(n) + 1e-12));
    }
    double noteHz(int midi) const { return refA_ * std::pow(2.0, double(midi - 69) / 12.0); }
    // The latest `n` decimated samples, oldest first, into the scratch buffer.
    const float* latest(std::size_t n) {
        scratch_.resize(n);
        for (std::size_t k = 0; k < n; ++k) scratch_[k] = ring_[(write_ - n + k) & (kRing - 1)];
        return scratch_.data();
    }
    double silenceRms() const { return std::pow(10.0, (double(sensDb_) - 12.0) / 20.0); }

    void sendOff(int offset, std::vector<Event>& out) {
        out.push_back({Event::NoteOff, note_, 0, 8192, offset});
        if (bendSent_ != 8192) out.push_back({Event::Bend, note_, 0, 8192, offset});
        bendSent_ = 8192;
        state_ = State::Idle;
    }

    void step(int offset, std::vector<Event>& out) {
        const float fast = fastDb();
        // The level of 10 to 40 ms ago (hops 3..10 back) is what a rise is measured against.
        float before = 0.0f;
        before = *std::min_element(history_.begin() + 2, history_.begin() + 10);
        const bool rise = fast > sensDb_ && fast - before > riseDb_;
        if (holdoff_ > 0) --holdoff_;
        for (int k = int(history_.size()) - 1; k > 0; --k) history_[std::size_t(k)] = history_[std::size_t(k - 1)];
        history_[0] = fast;

        if (rise && holdoff_ == 0) {
            if (state_ == State::Note) sendOff(offset, out);  // a new pluck ends the note that was sounding
            state_ = State::Attack;
            onsetAt_ = write_ - std::size_t(fastLen_ + hop_);  // the sound started about one fast window ago
            peakDb_ = fast;
            stable_ = 0;
            lastMidi_ = -1;
            holdoff_ = int(std::lround(0.001 * holdoffMs_ / (double(hop_) / rate_)));
            attackHops_ = 0;
            return;
        }

        if (state_ == State::Attack) {
            peakDb_ = std::max(peakDb_, fast);
            ++attackHops_;
            const std::size_t win = std::min<std::size_t>(write_ - onsetAt_, kRing - 1);
            if (attackHops_ > int(0.25 / (double(hop_) / rate_)) || fast < sensDb_ - 6.0f) {  // no pitch found in a quarter of a second: it was not a note
                state_ = State::Idle;
                return;
            }
            if (win < 192) return;
            const double minHz = std::max(60.0, 2.5 * rate_ / double(win));  // the lowest note that fits two and a half times in what has sounded
            const auto r = detector_.detect(latest(win), win, rate_, minHz, 1400.0, silenceRms());
            if (!r.valid || r.clarity < minClarity_) { stable_ = 0; return; }
            const int midi = int(std::lround(12.0 * std::log2(r.hz / refA_) + 69.0));
            stable_ = (midi == lastMidi_) ? stable_ + 1 : 1;
            lastMidi_ = midi;
            const bool periods = double(win) >= 3.0 * rate_ / r.hz;
            if (stable_ >= 3 && periods && midi >= 28 && midi <= 100) {
                note_ = midi;
                prevCents_ = 0.0;
                candMidi_ = -1;
                candCount_ = 0;
                // Velocity from how loud the attack was: from the sensitivity (soft) to -6 dBFS (hard).
                const float t = juce_clamp((peakDb_ - sensDb_) / std::max(1.0f, -6.0f - sensDb_), 0.0f, 1.0f);
                const int velocity = int(std::lround(20.0f + 107.0f * t));
                lastVelocity_ = velocity;
                out.push_back({Event::NoteOn, note_, velocity, 8192, offset});
                state_ = State::Note;
                lowCount_ = 0;
                bendSent_ = 8192;
                bendSmooth_ = 0.0f;
                const double ms = 1000.0 * double(write_ - onsetAt_) / rate_;
                const float old = latencyMs_.load();
                latencyMs_.store(old <= 0.0f ? float(ms) : old + 0.2f * (float(ms) - old));
            }
            return;
        }

        if (state_ == State::Note) {
            if (fast < sensDb_ - 3.0f) {
                if (++lowCount_ * hop_ >= int(rate_ * 0.020)) sendOff(offset, out);
                return;
            }
            lowCount_ = 0;
            // Follow the pitch: a window of at least three periods of the note.
            const double f0 = noteHz(note_);
            const std::size_t win = std::min<std::size_t>(std::max<std::size_t>(512, std::size_t(3.2 * rate_ / f0)), kRing - 1);
            const auto r = detector_.detect(latest(win), win, rate_, std::max(55.0, 0.7 * f0), 1400.0, silenceRms());
            if (!r.valid) return;
            const double cents = 1200.0 * std::log2(r.hz / f0);
            // A new note without a new pluck (a hammer-on, a pull-off, a fast line where the level hardly rises): the pitch jumps at once to another note and stays.
            const int midiNow = int(std::lround(12.0 * std::log2(r.hz / refA_) + 69.0));
            if (midiNow != note_ && std::abs(cents) > 60.0) {
                if (midiNow == candMidi_) {
                    ++candCount_;
                } else {
                    candMidi_ = midiNow;
                    candCount_ = 1;
                    candAbrupt_ = std::abs(cents - prevCents_) > 60.0;  // a bend moves little by little; a new note arrives in one step
                }
                prevCents_ = cents;
                if (candCount_ >= 3 && candAbrupt_ && midiNow >= 28 && midiNow <= 100) {
                    sendOff(offset, out);
                    note_ = midiNow;
                    out.push_back({Event::NoteOn, note_, lastVelocity_, 8192, offset});
                    state_ = State::Note;
                    lowCount_ = 0;
                    bendSent_ = 8192;
                    bendSmooth_ = 0.0f;
                    prevCents_ = 0.0;
                    candMidi_ = -1;
                    candCount_ = 0;
                    holdoff_ = int(std::lround(0.001 * holdoffMs_ / (double(hop_) / rate_)));
                }
                if (std::abs(cents) > 100.0 * bendRange_) return;
            } else {
                candMidi_ = -1;
                candCount_ = 0;
                prevCents_ = cents;
            }
            if (std::abs(cents) > 100.0 * bendRange_) return;  // an octave error or another note: not a bend
            bendSmooth_ += 0.5f * (float(cents) - bendSmooth_);
            const int value = int(std::lround(8192.0 + 8191.0 * double(bendSmooth_) / (100.0 * bendRange_)));
            const int v = std::clamp(value, 0, 16383);
            if (std::abs(v - bendSent_) >= 48) {
                bendSent_ = v;
                out.push_back({Event::Bend, note_, 0, v, offset});
            }
        }
    }

    static float juce_clamp(float v, float lo, float hi) { return std::min(hi, std::max(lo, v)); }

    PitchDetector detector_;
    std::vector<float> ring_, scratch_;
    std::array<float, 16> history_{};
    std::size_t write_ = 0, onsetAt_ = 0;
    double rate_ = 24000.0, acc_ = 0.0, lp_ = 0.0, refA_ = 440.0;
    int factor_ = 2, count_ = 0, hop_ = 96, fastLen_ = 120, sinceHop_ = 0, holdoff_ = 0, attackHops_ = 0, stable_ = 0, lastMidi_ = -1, note_ = 0, lowCount_ = 0,
        bendSent_ = 8192;
    float sensDb_ = -54.0f, bendRange_ = 2.0f, peakDb_ = -120.0f, bendSmooth_ = 0.0f, riseDb_ = 8.0f, holdoffMs_ = 80.0f;
    double prevCents_ = 0.0, minClarity_ = 0.0;
    int candMidi_ = -1, candCount_ = 0, lastVelocity_ = 100;
    bool candAbrupt_ = false;
    State state_ = State::Idle;
    std::atomic<float> latencyMs_{0.0f};
};

}  // namespace gm
