#pragma once
// Puts the two converters together. The one-note converter answers first (30 to 40 ms after the pluck) with the note it hears; the chord converter answers
// later (80 to 100 ms) with every note of the chord. In Hybrid mode:
//  - the first note goes out at once. When the chord converter has the same note it is confirmed and from then on that converter decides when it ends;
//  - the notes only the chord converter has are added;
//  - a note of the one-note converter that the chord converter never confirms, while it did hear other notes of that attack, is taken back (a wrong note
//    or a wrong octave does not hang for the whole time the chord rings);
//  - the pitch bend of the one-note converter is passed on only while a single note sounds (a bend moves every note of a channel).
// Mono passes the one-note converter through; Poly uses the chord converter only.
#include <array>
#include <vector>

#include "basic_pitch.h"
#include "mono_transcriber.h"

namespace gm {

class NoteMerger {
public:
    using Event = MonoTranscriber::Event;
    enum class Mode { Mono, Poly, Hybrid };

    void setMode(Mode m) { mode_ = m; }
    Mode mode() const { return mode_; }
    void setPolyAvailable(bool ok) { polyOk_ = ok; }
    // Mode as it really works: Poly and Hybrid fall back to Mono when the chord converter could not be loaded.
    Mode effective() const { return polyOk_ ? mode_ : Mode::Mono; }

    void reset() {
        for (auto& n : notes_) n = {};
        bend_ = 8192;
        polyOnAt_ = chordAt_ = -1000000000;
        onsets_.fill(-1000000000);
        onsetIdx_ = 0;
    }
    int sounding() const {
        int c = 0;
        for (const auto& n : notes_) c += n.on ? 1 : 0;
        return c;
    }

    // An event of the one-note converter at absolute sample `at` (offset: its sample within the block).
    void onMono(const Event& e, long long at, std::vector<Event>& out) {
        if (effective() == Mode::Poly) return;
        auto& n = notes_[std::size_t(e.note) & 127];
        switch (e.type) {
            case Event::NoteOn:
                if (!n.on) {
                    n = {true, true, false, at};
                    out.push_back(e);
                }
                break;
            case Event::NoteOff:
                if (n.on && (effective() == Mode::Mono || (n.mono && !n.confirmed))) emitOff(e.note, e.offset, out);
                if (effective() == Mode::Mono) bend_ = 8192;
                break;
            case Event::Bend:
                if (effective() == Mode::Mono || sounding() <= 1) {
                    bend_ = e.bend14;
                    out.push_back(e);
                }
                break;
        }
    }

    // An event of the chord converter, applied at `offset` of the block.
    void onPoly(const PolyEvent& e, long long at, int offset, std::vector<Event>& out) {
        if (effective() == Mode::Mono) return;
        auto& n = notes_[std::size_t(e.note) & 127];
        if (e.on) {
            polyOnAt_ = at;
            if (at - onsets_[onsetIdx_] < (long long)(0.12 * fs_)) chordAt_ = at;  // this and the three before it, all within 120 ms
            onsets_[onsetIdx_] = at;
            onsetIdx_ = (onsetIdx_ + 1) % 3;
            if (n.on) {
                if (n.mono && !n.confirmed && at - n.at < 8000) { n.confirmed = true; return; }  // the same note, now confirmed
                emitOff(e.note, offset, out);                                                  // the same string plucked again
            }
            if (bend_ != 8192 && sounding() >= 1) {  // a second note: the bend of the first must not move it
                bend_ = 8192;
                out.push_back({Event::Bend, 0, 0, 8192, offset});
            }
            n = {true, false, true, at};
            out.push_back({Event::NoteOn, e.note, e.velocity, 8192, offset, e.onsetHost});
        } else if (n.on && n.confirmed) {
            emitOff(e.note, offset, out);
        }
    }

    // Once per block: takes back the notes of the one-note converter that the chord converter did not confirm.
    void update(long long at, int offset, std::vector<Event>& out) {
        if (effective() != Mode::Hybrid) return;
        for (int m = 0; m < 128; ++m) {
            auto& n = notes_[std::size_t(m)];
            if (n.on && n.mono && !n.confirmed && polyOnAt_ >= n.at && at - n.at > ghostSamples_) emitOff(m, offset, out);
        }
    }
    void setSampleRate(double fs) {
        fs_ = fs;
        ghostSamples_ = (long long)(0.2 * fs);
    }
    // How clean the pitch reading of the one-note converter must be to send a note now (Hybrid). While chords are being played (the chord converter has
    // just found four onsets within 120 ms) a single reading is more likely a muddy mix of several strings, so only a very clean one passes.
    double clarityNeeded(long long at) const {
        if (effective() != Mode::Hybrid) return 0.0;
        return at - chordAt_ < (long long)(chordHoldS_ * fs_) ? chordClarity_ : soloClarity_;
    }
    void setClarities(double solo, double chord, double holdSeconds) {
        soloClarity_ = solo;
        chordClarity_ = chord;
        chordHoldS_ = holdSeconds;
    }

private:
    struct Note { bool on = false, mono = false, confirmed = false; long long at = 0; };

    void emitOff(int note, int offset, std::vector<Event>& out) {
        notes_[std::size_t(note) & 127] = {};
        out.push_back({Event::NoteOff, note, 0, 8192, offset});
        if (bend_ != 8192 && sounding() == 0) {
            bend_ = 8192;
            out.push_back({Event::Bend, note, 0, 8192, offset});
        }
    }

    Mode mode_ = Mode::Hybrid;
    bool polyOk_ = false;
    std::array<Note, 128> notes_{};
    int bend_ = 8192;
    long long polyOnAt_ = -1000000000, ghostSamples_ = 9600, chordAt_ = -1000000000;
    std::array<long long, 3> onsets_{{-1000000000, -1000000000, -1000000000}};
    int onsetIdx_ = 0;
    double fs_ = 48000.0, soloClarity_ = 0.0, chordClarity_ = 0.97, chordHoldS_ = 1.0;
};

}  // namespace gm
