#pragma once
// The converter for chords: Basic Pitch (Spotify, Apache 2.0; the model is engine/models/nmp.onnx) run through ONNX Runtime (MIT), plus a note tracker that
// turns its onset and note probabilities into note-ons and note-offs as they arrive.
//
//  - The guitar (host rate) is brought to 22 050 Hz. Every few frames (35 ms) the last 2 s go through the model. The model looks about 10 frames
//    (116 ms) into the future of each frame, so a frame is answered only when `lookahead` frames of audio exist after it (the live trade: 6 frames = 70 ms
//    is almost as good as the full context, fewer is much worse; docs/PLAN_GUITAR_MIDI.md, section 8).
//  - An onset is a peak of the onset probability (>= 0.5) with the note still active a frame later. A note ends when its note probability stays low.
//  - Nothing is run while the guitar is silent (the model costs 15 to 30 ms of one core per run).
//  - ONNX Runtime is loaded by its full path from next to this module (onnxruntime.dll in the folder of the plug-in or the program): Windows has its own
//    older copy in System32 and must not get the chance to pick that one. Without the DLL (or the model) `load()` is false and everything else still works.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#ifndef STRUMLINE_POLY
#define STRUMLINE_POLY 0
#endif

#if STRUMLINE_POLY
#define ORT_API_MANUAL_INIT
#include <filesystem>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <onnxruntime_cxx_api.h>
#endif

namespace gm {

// A note of the chord converter. `hostSample` is where the host's audio was when it was decided (the notes arrive after their onset, never before).
struct PolyEvent {
    bool on = false;
    int note = 0;
    int velocity = 0;
    long long hostSample = 0;
    long long onsetHost = 0;  // where the note started (the frame it was found in), at the host rate; before hostSample by the look-ahead
};

// Streaming sample-rate converter to 22 050 Hz: a windowed sinc (16 zero crossings of the output rate on each side) read from a table.
class Resampler22k {
public:
    static constexpr double kRate = 22050.0;

    void prepare(double hostRate) {
        ratio_ = hostRate / kRate;
        const double cut = std::min(0.5, 0.45 / std::max(1.0, ratio_));  // cycles per input sample; just under the output Nyquist
        half_ = std::max(8.0, 16.0 * std::max(1.0, ratio_));
        const double pi = 3.14159265358979323846;
        table_.assign(std::size_t(kTable) + 2, 0.0f);
        for (int i = 0; i <= kTable + 1; ++i) {
            const double x = double(i) / double(kTable) * half_;
            const double s = x < 1e-9 ? 2.0 * cut : std::sin(2.0 * pi * cut * x) / (pi * x);
            const double w = 0.5 + 0.5 * std::cos(pi * x / half_);  // Hann
            table_[std::size_t(i)] = float(s * w);
        }
        reset();
    }
    void reset() {
        in_.clear();
        base_ = 0;
        pos_ = 0.0;
    }
    double ratio() const { return ratio_; }
    // Appends the output samples that can be computed now.
    void process(const float* x, int n, std::vector<float>& out) {
        in_.insert(in_.end(), x, x + n);
        const long long end = base_ + (long long)in_.size();
        while (pos_ + half_ < double(end)) {
            const long long c = (long long)std::floor(pos_);
            const long long a = c - (long long)std::ceil(half_) + 1, b = c + (long long)std::ceil(half_);
            double acc = 0.0;
            for (long long k = a; k <= b; ++k) {
                const double d = std::abs(double(k) - pos_);
                if (d >= half_) continue;
                const double t = d / half_ * double(kTable);
                const int i = int(t);
                const float w = table_[std::size_t(i)] + (table_[std::size_t(i) + 1] - table_[std::size_t(i)]) * float(t - double(i));
                const long long idx = k - base_;
                if (idx >= 0) acc += double(in_[std::size_t(idx)]) * w;
            }
            out.push_back(float(acc));
            pos_ += ratio_;
        }
        const long long keepFrom = (long long)std::floor(pos_ - half_) - 1;
        if (keepFrom - base_ > 4096) {
            in_.erase(in_.begin(), in_.begin() + std::ptrdiff_t(keepFrom - base_));
            base_ = keepFrom;
        }
    }

private:
    static constexpr int kTable = 4096;
    double ratio_ = 2.0, half_ = 32.0, pos_ = 0.0;
    long long base_ = 0;
    std::vector<float> in_, table_;
    std::vector<float> dummy_;
};

#if STRUMLINE_POLY
// Basic Pitch through ONNX Runtime.
class BasicPitchModel {
public:
    static constexpr int kSamples = 43844, kFrames = 172, kBins = 88;

#if defined(_WIN32)
    static constexpr const char* kLibraryName = "onnxruntime.dll";
#elif defined(__APPLE__)
    static constexpr const char* kLibraryName = "libonnxruntime.dylib";
#else
    static constexpr const char* kLibraryName = "libonnxruntime.so";
#endif

    // `libDir`: the folder that holds ONNX Runtime's shared library. False (and nothing loaded) when the library, the model or the session is not usable.
    bool load(const unsigned char* model, std::size_t size, int threads, const std::filesystem::path& libDir) {
        try {
            if (!ready_) {
                using GetApiBase = const OrtApiBase*(ORT_API_CALL*)();
#if defined(_WIN32)
                const HMODULE lib = LoadLibraryExW((libDir / kLibraryName).c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
                if (!lib) return false;
                const auto getBase = reinterpret_cast<GetApiBase>(GetProcAddress(lib, "OrtGetApiBase"));
#else
                void* lib = dlopen((libDir / kLibraryName).string().c_str(), RTLD_NOW | RTLD_LOCAL);
                if (!lib) return false;
                const auto getBase = reinterpret_cast<GetApiBase>(dlsym(lib, "OrtGetApiBase"));
#endif
                if (!getBase) return false;
                const OrtApi* api = getBase()->GetApi(ORT_API_VERSION);
                if (!api) return false;
                Ort::InitApi(api);
            }
            env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_ERROR, "strumline");
            Ort::SessionOptions opts;
            opts.SetIntraOpNumThreads(threads);
            opts.SetInterOpNumThreads(1);
            opts.AddConfigEntry("session.intra_op.allow_spinning", "0");  // do not burn a core while waiting for the next window
            opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
            session_ = std::make_unique<Ort::Session>(*env_, model, size, opts);
            Ort::AllocatorWithDefaultOptions alloc;
            inName_ = session_->GetInputNameAllocated(0, alloc).get();
            std::array<std::string, 3> names;
            for (std::size_t i = 0; i < 3; ++i) names[i] = session_->GetOutputNameAllocated(i, alloc).get();
            // The outputs of the TensorFlow model, by name: ...:2 is the onset, ...:1 the note activity, ...:0 the pitch contour (264 bins).
            for (const auto& n : names) {
                if (n.size() > 2 && n.back() == '2') onsetName_ = n;
                else if (n.size() > 2 && n.back() == '1') noteName_ = n;
                else contourName_ = n;
            }
            ready_ = !onsetName_.empty() && !noteName_.empty() && !contourName_.empty();
        } catch (...) {
            session_.reset();
            ready_ = false;
        }
        return ready_;
    }
    bool ready() const { return ready_; }

    // `audio`: kSamples samples at 22 050 Hz. `onset` and `note`: kFrames * kBins probabilities each.
    bool run(const float* audio, float* onset, float* note) {
        if (!ready_) return false;
        try {
            const Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
            const std::array<int64_t, 3> shape{1, kSamples, 1};
            Ort::Value in = Ort::Value::CreateTensor<float>(mem, const_cast<float*>(audio), std::size_t(kSamples), shape.data(), shape.size());
            const char* inNames[] = {inName_.c_str()};
            const char* outNames[] = {onsetName_.c_str(), noteName_.c_str(), contourName_.c_str()};
            auto out = session_->Run(Ort::RunOptions{nullptr}, inNames, &in, 1, outNames, 3);
            std::copy_n(out[0].GetTensorData<float>(), std::size_t(kFrames) * kBins, onset);
            std::copy_n(out[1].GetTensorData<float>(), std::size_t(kFrames) * kBins, note);
            return true;
        } catch (...) {
            return false;
        }
    }

    // The folder of the plug-in (or program) this code is part of: the shared library sits next to it (in a macOS bundle, Contents/MacOS).
    static std::filesystem::path moduleDir() {
#if defined(_WIN32)
        HMODULE h = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&BasicPitchModel::moduleDir), &h);
        wchar_t buf[2048];
        const DWORD n = GetModuleFileNameW(h, buf, DWORD(std::size(buf)));
        return std::filesystem::path(std::wstring(buf, n)).parent_path();
#else
        Dl_info info{};
        if (dladdr(reinterpret_cast<const void*>(&BasicPitchModel::moduleDir), &info) && info.dli_fname) return std::filesystem::path(info.dli_fname).parent_path();
        return ".";
#endif
    }

private:
    bool ready_ = false;
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::string inName_, onsetName_, noteName_, contourName_;
};
#else
class BasicPitchModel {
public:
    static constexpr int kSamples = 43844, kFrames = 172, kBins = 88;
    bool load(const unsigned char*, std::size_t, int, const std::filesystem::path&) { return false; }
    bool ready() const { return false; }
    bool run(const float*, float*, float*) { return false; }
    static std::filesystem::path moduleDir() { return "."; }
};
#endif

// The chord converter: audio in (host rate), note-ons and note-offs out.
class PolyTranscriber {
public:
    static constexpr int kHop = 256, kMaxVoices = 6, kLowest = 21;

    bool load(const unsigned char* model, std::size_t size, int threads = 2) { return model_.load(model, size, threads, BasicPitchModel::moduleDir()); }
    bool ready() const { return model_.ready(); }

    void prepare(double hostRate) {
        resampler_.prepare(hostRate);
        hist_.assign(kHistory, 0.0f);
        blockDb_.assign(kBlocks, -120.0f);
        onset_.assign(std::size_t(BasicPitchModel::kFrames) * BasicPitchModel::kBins, 0.0f);
        note_ = onset_;
        window_.assign(BasicPitchModel::kSamples, 0.0f);
        reset();
    }
    void reset() {
        resampler_.reset();
        std::fill(hist_.begin(), hist_.end(), 0.0f);
        std::fill(blockDb_.begin(), blockDb_.end(), -120.0f);
        n22_ = 0;
        blockE_ = 0.0;
        blockN_ = 0;
        lastLoud_ = lastRise_ = -1000000;
        lastRunFrame_ = -1000;
        next_ = INT32_MIN;
        prevOn_.fill(0.0f);
        lastOn_.fill(-1000);
        low_.fill(0);
        active_.fill(false);
        age_.fill(0);
        hostPos_ = 0;
    }

    void setSensitivityDb(float db) { sensDb_ = db; }
    void setLookaheadFrames(int f) { lookahead_ = std::clamp(f, 2, 15); }
    void setOnsetThreshold(float t) { onsetThr_ = t; }
    // How often the model runs, in frames of 11.6 ms: a shorter hop answers sooner and costs proportionally more CPU.
    void setHopFrames(int f) { hopFrames_ = std::clamp(f, 1, 8); }
    // Between plucks (nothing new for `burstSeconds`) the model runs only every `idle` frames, just to see the notes end: this is what saves the CPU.
    void setIdle(int idleHopFrames, float burstSeconds, float riseDb) {
        idleHopFrames_ = std::clamp(idleHopFrames, 1, 16);
        burstSeconds_ = burstSeconds;
        riseDb_ = riseDb;
    }
    void setNoteOffThreshold(float t) { offThr_ = t; }
    // Milliseconds the last model run took (for the CPU figure).
    float lastRunMs() const { return runMs_; }
    long long runs() const { return runs_; }

    // `n` samples of the guitar at the host rate. The events decided meanwhile are appended to `out`.
    void feed(const float* x, int n, std::vector<PolyEvent>& out) {
        if (!model_.ready()) { hostPos_ += n; return; }
        scratch_.clear();
        resampler_.process(x, n, scratch_);
        hostPos_ += n;
        for (const float s : scratch_) {
            hist_[std::size_t(n22_) & (kHistory - 1)] = s;
            ++n22_;
            blockE_ += double(s) * s;
            if (++blockN_ == kHop) {
                const float db = float(10.0 * std::log10(blockE_ / double(kHop) + 1e-12));
                blockDb_[std::size_t(n22_ / kHop - 1) & (kBlocks - 1)] = db;
                if (db > sensDb_ - 6.0f) lastLoud_ = n22_;
                // A rise of the level over the quietest of the blocks 3 to 8 back: something was plucked. The model then runs at full rate for a while.
                {
                    const int b = int(n22_ / kHop) - 1;
                    float before = 0.0f;
                    before = blockDb(b - 3);
                    for (int k = 4; k <= 8; ++k) before = std::min(before, blockDb(b - k));
                    if (db > sensDb_ && db - before > riseDb_) lastRise_ = n22_;
                }
                blockE_ = 0.0;
                blockN_ = 0;
                maybeRun(out);
            }
        }
    }

private:
    static constexpr std::size_t kHistory = 1 << 16, kBlocks = 1 << 10;

    float blockDb(int block) const { return block < 0 ? -120.0f : blockDb_[std::size_t(block) & (kBlocks - 1)]; }
    void releaseAll(std::vector<PolyEvent>& out, long long at) {
        for (int p = 0; p < 88; ++p)
            if (active_[std::size_t(p)]) {
                out.push_back({false, p + kLowest, 0, at});
                active_[std::size_t(p)] = false;
            }
    }

    void maybeRun(std::vector<PolyEvent>& out) {
        const int newest = int((n22_ - 68) / kHop);  // the last frame whose whole window of the model fits in what has been heard
        const bool busy = n22_ - lastRise_ < (long long)(burstSeconds_ * Resampler22k::kRate);
        if (newest - lastRunFrame_ < (busy ? hopFrames_ : idleHopFrames_)) return;
        lastRunFrame_ = newest;
        const long long at = (long long)std::llround(double(n22_) * resampler_.ratio());
        if (next_ == INT32_MIN) next_ = newest - lookahead_;
        // Silence: nothing sounds, so nothing runs; whatever was held ends.
        if (n22_ - lastLoud_ > (long long)(0.8 * Resampler22k::kRate)) {
            releaseAll(out, at);
            next_ = newest - lookahead_;
            prevOn_.fill(0.0f);
            return;
        }
        const long long start = (long long)(newest - (BasicPitchModel::kFrames - 1)) * kHop;
        for (int i = 0; i < BasicPitchModel::kSamples; ++i) {
            const long long s = start + i;
            window_[std::size_t(i)] = s < 0 || s >= n22_ ? 0.0f : hist_[std::size_t(s) & (kHistory - 1)];
        }
        const auto t0 = std::chrono::steady_clock::now();
        const bool ok = model_.run(window_.data(), onset_.data(), note_.data());
        runMs_ = float(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
        ++runs_;
        if (!ok) return;
        const int first = newest - (BasicPitchModel::kFrames - 1);
        const int last = newest - lookahead_ - 1;  // a frame is a peak only when the next one is known
        for (int f = std::max(next_, first + 1); f <= last; ++f) frame(f, f - first, at, out);
        next_ = std::max(next_, last + 1);
    }

    void frame(int f, int k, long long at, std::vector<PolyEvent>& out) {
        const float* on = &onset_[std::size_t(k) * BasicPitchModel::kBins];
        const float* onNext = &onset_[std::size_t(k + 1) * BasicPitchModel::kBins];
        const float* onPrev = &onset_[std::size_t(k - 1) * BasicPitchModel::kBins];
        const float* nt = &note_[std::size_t(k) * BasicPitchModel::kBins];
        const float* ntNext = &note_[std::size_t(k + 1) * BasicPitchModel::kBins];
        // The level of the attack, for the velocity and as a floor: the loudest 256-sample block around the frame.
        float peak = -120.0f;
        for (int b = f - 1; b <= f + 2; ++b) peak = std::max(peak, blockDb(b));
        for (int p = 0; p < 88; ++p) {
            const float o = on[p];
            const bool peakNow = o >= onsetThr_ && o >= onPrev[p] && o > onNext[p];
            if (peakNow && f - lastOn_[std::size_t(p)] >= 4 && (ntNext[p] >= 0.3f || o >= 0.8f) && peak > sensDb_) {
                lastOn_[std::size_t(p)] = f;
                if (active_[std::size_t(p)]) out.push_back({false, p + kLowest, 0, at});  // the same string again
                else if (countActive() >= kMaxVoices) dropOldest(out, at);
                const float t = std::clamp((peak - sensDb_) / std::max(1.0f, -6.0f - sensDb_), 0.0f, 1.0f);
                out.push_back({true, p + kLowest, int(std::lround(20.0f + 107.0f * t)), at, (long long)std::llround(double(f) * kHop * resampler_.ratio())});
                active_[std::size_t(p)] = true;
                age_[std::size_t(p)] = f;
                low_[std::size_t(p)] = 0;
            } else if (active_[std::size_t(p)]) {
                if (nt[p] < offThr_) {
                    if (++low_[std::size_t(p)] >= 2) {
                        out.push_back({false, p + kLowest, 0, at});
                        active_[std::size_t(p)] = false;
                    }
                } else {
                    low_[std::size_t(p)] = 0;
                }
            }
            prevOn_[std::size_t(p)] = o;
        }
    }
    int countActive() const {
        int c = 0;
        for (const bool a : active_) c += a ? 1 : 0;
        return c;
    }
    void dropOldest(std::vector<PolyEvent>& out, long long at) {
        int oldest = -1;
        for (int p = 0; p < 88; ++p)
            if (active_[std::size_t(p)] && (oldest < 0 || age_[std::size_t(p)] < age_[std::size_t(oldest)])) oldest = p;
        if (oldest >= 0) {
            out.push_back({false, oldest + kLowest, 0, at});
            active_[std::size_t(oldest)] = false;
        }
    }

    BasicPitchModel model_;
    Resampler22k resampler_;
    std::vector<float> hist_, blockDb_, onset_, note_, window_, scratch_;
    std::array<float, 88> prevOn_{};
    std::array<int, 88> lastOn_{}, low_{}, age_{};
    std::array<bool, 88> active_{};
    long long n22_ = 0, lastRise_ = -1000000, lastLoud_ = -1000000, hostPos_ = 0, runs_ = 0;
    double blockE_ = 0.0;
    int blockN_ = 0, lastRunFrame_ = -1000, next_ = INT32_MIN, lookahead_ = 5, hopFrames_ = 3, idleHopFrames_ = 12;
    float sensDb_ = -54.0f, onsetThr_ = 0.5f, offThr_ = 0.2f, runMs_ = 0.0f, burstSeconds_ = 0.25f, riseDb_ = 4.0f;
};

}  // namespace gm
