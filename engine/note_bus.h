#pragma once
// The notes the converter plays, as events: from the audio thread (which makes them) to the editor (which draws them). One producer, one consumer, no locks.
#include <array>
#include <atomic>
#include <cstddef>

namespace gm {

struct NoteEvent {
    bool on = false;    // note on (true) or note off (false)
    int midi = 0;       // the note as it is sent (after the transposition)
    int velocity = 0;   // 1..127 for a note on
    int bend14 = -1;    // >= 0: not a note but a pitch wheel value (0..16383, 8192 at the centre), for the bend lane
};

class NoteBus {
public:
    // Audio thread. False when the editor is not draining the queue (the event is dropped: a drawing is not worth waiting for).
    bool push(const NoteEvent& e) {
        const std::size_t w = write_.load(std::memory_order_relaxed), n = (w + 1) % kSize;
        if (n == read_.load(std::memory_order_acquire)) return false;
        buf_[w] = e;
        write_.store(n, std::memory_order_release);
        return true;
    }
    // Editor thread.
    bool pop(NoteEvent& e) {
        const std::size_t r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire)) return false;
        e = buf_[r];
        read_.store((r + 1) % kSize, std::memory_order_release);
        return true;
    }

private:
    static constexpr std::size_t kSize = 1024;
    std::array<NoteEvent, kSize> buf_{};
    std::atomic<std::size_t> write_{0}, read_{0};
};

}  // namespace gm
