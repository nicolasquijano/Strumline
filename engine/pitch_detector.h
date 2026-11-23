#pragma once
// Pitch detection of one note: the McLeod method (the normalised square difference function). The first key maximum above a fraction of the highest one
// is the period, which avoids the octave errors that plain autocorrelation makes on a guitar's weak low fundamental; a parabola through the peak refines it
// to a fraction of a sample. The same detector as the tuner of Schematic Amp Sim.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace gm {

struct PitchResult {
    bool valid = false;
    double hz = 0.0;
    double clarity = 0.0;  // 0..1: how periodic the signal is (a clean note is above 0.9)
};

class PitchDetector {
public:
    // `x`: `n` samples at `rate`. The window has to hold about two and a half periods of the lowest note wanted (`minHz`).
    PitchResult detect(const float* x, std::size_t n, double rate, double minHz, double maxHz, double silenceRms) {
        PitchResult r;
        if (n < 128 || rate <= 0.0) return r;
        double energy = 0.0;
        for (std::size_t i = 0; i < n; ++i) energy += double(x[i]) * x[i];
        if (std::sqrt(energy / double(n)) < silenceRms) return r;

        const std::size_t maxLag = std::min<std::size_t>(std::size_t(rate / minHz) + 2, n / 2);
        const std::size_t minLag = std::max<std::size_t>(2, std::size_t(rate / maxHz));
        if (maxLag <= minLag + 2) return r;

        nsdf_.assign(maxLag + 1, 0.0);
        const std::size_t w = n - maxLag;  // the same window for every lag, so the values are comparable
        double m0 = 0.0;
        for (std::size_t j = 0; j < w; ++j) m0 += double(x[j]) * x[j];
        double mLag = m0;
        for (std::size_t t = 0; t <= maxLag; ++t) {
            double acf = 0.0;
            for (std::size_t j = 0; j < w; ++j) acf += double(x[j]) * x[j + t];
            if (t > 0) mLag += double(x[w - 1 + t]) * x[w - 1 + t] - double(x[t - 1]) * x[t - 1];  // slide the second window
            nsdf_[t] = (m0 + mLag) > 1e-18 ? 2.0 * acf / (m0 + mLag) : 0.0;
        }

        // Key maxima: the highest point between a positive-going and the next negative-going zero crossing.
        peaks_.clear();
        std::size_t t = 1;
        while (t < maxLag && nsdf_[t] > 0.0) ++t;  // skip the lobe around lag 0
        while (t < maxLag) {
            while (t < maxLag && nsdf_[t] <= 0.0) ++t;
            std::size_t best = t;
            while (t < maxLag && nsdf_[t] > 0.0) {
                if (nsdf_[t] > nsdf_[best]) best = t;
                ++t;
            }
            if (best >= minLag && best < maxLag && nsdf_[best] > 0.0 && best > 0 && nsdf_[best] >= nsdf_[best - 1] && nsdf_[best] >= nsdf_[best + 1]) {
                const double a = nsdf_[best - 1], b = nsdf_[best], c = nsdf_[best + 1];
                const double den = a - 2.0 * b + c;
                const double shift = std::abs(den) > 1e-12 ? 0.5 * (a - c) / den : 0.0;
                peaks_.push_back({double(best) + shift, b - 0.25 * (a - c) * shift});
            }
        }
        if (peaks_.empty()) return r;
        double top = 0.0;
        for (const auto& p : peaks_) top = std::max(top, p.value);
        for (const auto& p : peaks_) {
            if (p.value >= keyThreshold * top) {  // the first one close to the best: the fundamental, not a subharmonic
                r.valid = top > kMinClarity;
                r.hz = rate / p.pos;
                r.clarity = p.value;
                break;
            }
        }
        return r;
    }

    // The first key maximum at least `keyThreshold` of the highest one is the period. Higher favours the longer periods (lower notes, octave errors down),
    // lower favours the shorter ones.
    double keyThreshold = 0.9;
    static constexpr double kMinClarity = 0.75;

private:
    struct Peak { double pos, value; };
    std::vector<double> nsdf_;
    std::vector<Peak> peaks_;
};

}  // namespace gm
