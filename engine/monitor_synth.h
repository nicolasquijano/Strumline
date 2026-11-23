#pragma once
// A small polyphonic synth to hear what the converter plays without loading another instrument: six voices, sine / saw / square, a short attack and a
// release. It is a monitor, not an instrument (the saw and the square are not band-limited).
#include <array>
#include <cmath>

namespace gm {

class MonitorSynth {
public:
    enum Wave { Sine = 0, Saw = 1, Square = 2 };

    void prepare(double fs) {
        fs_ = fs;
        attack_ = float(1.0 - std::exp(-1.0 / (0.004 * fs)));
        release_ = float(1.0 - std::exp(-1.0 / (0.10 * fs)));
        allOff();
    }
    void setWave(int w) { wave_ = w; }
    void setBend(float semitones) { bend_ = semitones; }  // pitch wheel, in semitones, for every voice

    void noteOn(int midi, int velocity) {
        int pick = -1;
        unsigned long oldest = ~0ul;
        for (int i = 0; i < kVoices; ++i) {
            if (!v_[i].active) { pick = i; break; }
            if (v_[i].age < oldest) { oldest = v_[i].age; pick = i; }
        }
        Voice& v = v_[pick];
        v.active = true;
        v.gate = true;
        v.midi = midi;
        v.phase = 0.0;
        v.env = 0.0f;
        v.amp = float(velocity) / 127.0f;
        v.age = ++counter_;
    }
    void noteOff(int midi) {
        for (auto& v : v_)
            if (v.active && v.gate && v.midi == midi) v.gate = false;
    }
    void allOff() {
        for (auto& v : v_) v = Voice{};
    }

    // Adds `n` samples of the voices to `out`, scaled by `gain`.
    void render(float* out, int n, float gain) {
        for (auto& v : v_) {
            if (!v.active) continue;
            const double inc = 440.0 * std::pow(2.0, (double(v.midi - 69) + double(bend_)) / 12.0) / fs_;
            for (int i = 0; i < n; ++i) {
                v.env += ((v.gate ? 1.0f : 0.0f) - v.env) * (v.gate ? attack_ : release_);
                v.phase += inc;
                if (v.phase >= 1.0) v.phase -= 1.0;
                float s;
                switch (wave_) {
                    case Saw: s = float(2.0 * v.phase - 1.0); break;
                    case Square: s = v.phase < 0.5 ? 1.0f : -1.0f; break;
                    default: s = float(std::sin(6.283185307179586 * v.phase)); break;
                }
                out[i] += gain * 0.25f * v.amp * v.env * s;
            }
            if (!v.gate && v.env < 0.0005f) v.active = false;
        }
    }

private:
    static constexpr int kVoices = 6;
    struct Voice {
        bool active = false, gate = false;
        int midi = 0;
        double phase = 0.0;
        float env = 0.0f, amp = 0.0f;
        unsigned long age = 0;
    };
    std::array<Voice, kVoices> v_{};
    double fs_ = 48000.0;
    float attack_ = 0.01f, release_ = 0.001f;
    int wave_ = Saw;
    float bend_ = 0.0f;
    unsigned long counter_ = 0;
};

}  // namespace gm
