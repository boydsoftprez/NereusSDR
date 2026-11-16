// no-port-check: NereusSDR-original test helper.
#pragma once

// =================================================================
// tests/RxWeakStationModel.h  (NereusSDR)
// =================================================================
//
// A weak SSB station on a quiet band, end to end through the receive
// leveler's station check and the leveler: 12 s of band noise, one 30 s
// over, then 18 s of noise again. The station talks in syllables
// (170 ms on, 80 ms off, a 0.6 s breath every 4 s) with a pitch of 95 to
// 145 Hz, its harmonics from 200 to 2900 Hz falling 6 dB an octave above
// 500 Hz.
//
// `snrDb` is the station's power over the noise in the receiver's
// passband (2.8 kHz), while it talks: the S-meter's view. The three
// readings the gate takes are built from it:
//   * the S-meter average (noise plus the station, smoothed over 0.1 s,
//     read ten times a second with 0.7 dB of jitter);
//   * the panadapter: complex band noise plus the station's harmonics,
//     4-term Blackman-Harris window, 1024-point FFT every 33 ms, bins
//     `binHz` wide (47 Hz is a 4096-point display at 192 kHz);
//   * the audio after the AGC: hiss at `hissDb`, the voice `snrDb` over
//     it while it talks.
//
// `Band` adds what an empty frequency brings besides steady noise, the
// same in all three readings: static crashes (bursts of wideband noise,
// 5 to 40 ms, decaying), fading of the band noise, a steady carrier in
// the passband, a swell of the noise floor (up over 3 s, held 2 s, back
// down over 3 s, every 15 s from 4 s in). With `snrDb` at or below
// kNoStation nobody talks at all.
//
// Modification history (NereusSDR):
//   2026-11-15 — New. J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Anthropic Claude Code.
//   2026-11-16 — Band: static crashes, fading noise, a carrier, a swell of
//                the noise floor, and a run with no station. J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Anthropic Claude Code.
// =================================================================

#include "core/dsp/RxLeveler.h"
#include "core/dsp/RxPassbandVoice.h"
#include "core/dsp/RxStationGate.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <random>
#include <vector>

namespace NereusSDR::WeakStation {

// No station: `snrDb` at or below this.
constexpr double kNoStation = -100.0;

// What the band brings besides steady noise.
struct Band {
    double crashesPerSec{0.0};
    double crashDb{20.0};        // a crash's peak power over the noise
    double fadeDb{0.0};          // the noise level wanders this far each way
    double carrierDb{-999.0};    // a steady carrier's power over the passband noise
    double swellDb{0.0};         // the noise swells this far and back, every 15 s
};

struct Result {
    double presentShare{0.0};   // of the over's blocks, station counted present
    double gainAvgDb{0.0};      // leveler gain over the over
    double gainLateDb{0.0};     // ... from 3 s in (settled)
    double gainMaxDb{-99.0};
    double needDb{0.0};         // lift that puts the voice on the target
    double gapPresentShare{0.0};  // of the noise-only blocks (before, and 3 s after;
                                  // the whole run from 2 s with no station)
    double gapGainMaxDb{-99.0};   // most gain on the hiss in those blocks
};

namespace detail {

constexpr double kPi = 3.14159265358979323846;
constexpr int kFftSize = 1024;
constexpr double kFrameSec = 0.033;
constexpr double kOverStart = 12.0;
constexpr double kOverEnd = 42.0;
constexpr double kRunSec = 60.0;
constexpr double kPassLowHz = 100.0;
constexpr double kPassHighHz = 2900.0;

inline bool talking(double t)
{
    if (t < kOverStart || t >= kOverEnd) {
        return false;
    }
    const double ts = t - kOverStart;
    return std::fmod(ts, 4.0) < 3.4 && std::fmod(ts, 0.25) < 0.17;
}

// The band's noise power at `t`, relative to steady: the fading times
// (1 + the crashes then).
struct BandNoise {
    std::vector<std::pair<double, double>> crashes;   // start s, duration s
    std::vector<double> crashPeak;                    // power over the noise
    double fadeDb{0.0};
    double swellDb{0.0};

    double swell(double t) const
    {
        if (swellDb == 0.0 || t < 4.0) {
            return 1.0;
        }
        const double ts = std::fmod(t - 4.0, 15.0);
        double up = 0.0;
        if (ts < 3.0) {
            up = 0.5 - 0.5 * std::cos(kPi * ts / 3.0);
        } else if (ts < 5.0) {
            up = 1.0;
        } else if (ts < 8.0) {
            up = 0.5 + 0.5 * std::cos(kPi * (ts - 5.0) / 3.0);
        }
        return std::pow(10.0, swellDb * up / 10.0);
    }

    double at(double t) const
    {
        const double fade = fadeDb == 0.0
            ? 1.0
            : std::pow(10.0, fadeDb * (0.6 * std::sin(2.0 * kPi * 0.05 * t)
                                       + 0.4 * std::sin(2.0 * kPi * 0.13 * t + 1.0)) / 10.0);
        // The crashes start in time order and none lasts past 4 x 40 ms.
        double crash = 0.0;
        auto it = std::upper_bound(crashes.begin(), crashes.end(), t,
                                   [](double v, const std::pair<double, double>& c) { return v < c.first; });
        while (it != crashes.begin()) {
            --it;
            const double d = t - it->first;
            if (d >= 0.16) {
                break;
            }
            if (d < 4.0 * it->second) {
                crash += crashPeak[static_cast<size_t>(it - crashes.begin())]
                    * std::exp(-2.0 * d / it->second);
            }
        }
        return fade * swell(t) * (1.0 + crash);
    }
};

// One syllable's pitch: fixed per syllable, from the syllable's number.
inline double pitchHz(double t, std::mt19937& g, std::vector<double>& pitches)
{
    const int syllable = static_cast<int>(std::floor((t - kOverStart) / 0.25));
    while (static_cast<int>(pitches.size()) <= std::max(syllable, 0)) {
        pitches.push_back(std::uniform_real_distribution<double>(95.0, 145.0)(g));
    }
    return pitches[static_cast<size_t>(std::max(syllable, 0))];
}

inline void fft(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; (j & bit) != 0; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(a[i], a[j]);
        }
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const std::complex<double> w = std::polar(1.0, -2.0 * kPi / static_cast<double>(len));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> wk(1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k) {
                const std::complex<double> u = a[i + k];
                const std::complex<double> v = a[i + k + len / 2] * wk;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                wk *= w;
            }
        }
    }
}

}  // namespace detail

// `base` carries the leveler's settings (target, boost, cut, times); its
// on, stationPresent and nearOverload are set here. `useBins` false runs
// the gate on the S-meter alone, as before the panadapter check.
inline Result run(double snrDb, double hissDb, double binHz, const RxLevelerParams& base,
                  bool useBins = true, unsigned seed = 7, const Band& band = {})
{
    using namespace detail;
    std::mt19937 g(seed);
    std::normal_distribution<double> n01(0.0, 1.0);
    std::vector<double> pitches;
    const bool station = snrDb > kNoStation;

    BandNoise noise;
    noise.fadeDb = band.fadeDb;
    noise.swellDb = band.swellDb;
    if (band.crashesPerSec > 0.0) {
        std::mt19937 gc(seed * 31U + 5U);
        std::exponential_distribution<double> gap(band.crashesPerSec);
        std::uniform_real_distribution<double> u01(0.0, 1.0);
        for (double t = gap(gc); t < kRunSec; t += gap(gc)) {
            noise.crashes.push_back({t, 0.005 + 0.035 * u01(gc)});
            noise.crashPeak.push_back(std::pow(10.0, (band.crashDb + 6.0 * (u01(gc) - 0.5)) / 10.0));
        }
    }
    const bool plainNoise = noise.crashes.empty() && band.fadeDb == 0.0 && band.swellDb == 0.0;

    RxLeveler leveler;
    RxStationGate gate;
    RxPassbandVoice voice;
    RxLevelerParams p = base;
    p.on = true;

    // Panadapter: complex noise of unit power per sample; the passband
    // holds kPass / fs of it, and the station snrDb over that.
    const double fsIq = binHz * kFftSize;
    const double passNoise = (kPassHighHz - kPassLowHz) / fsIq;
    const double voicePower = station ? passNoise * std::pow(10.0, snrDb / 10.0) : 0.0;
    const double carrierAmp = band.carrierDb > -900.0
        ? std::sqrt(passNoise * std::pow(10.0, band.carrierDb / 10.0)) : 0.0;
    constexpr double kCarrierHz = 1000.0;
    const int loBin = kFftSize / 2 + static_cast<int>(std::ceil(kPassLowHz / binHz));
    const int hiBin = kFftSize / 2 + static_cast<int>(std::floor(kPassHighHz / binHz)) + 1;
    std::vector<double> window(kFftSize);
    for (int i = 0; i < kFftSize; ++i) {
        const double x = 2.0 * kPi * i / (kFftSize - 1);
        window[static_cast<size_t>(i)] = 0.35875 - 0.48829 * std::cos(x) + 0.14128 * std::cos(2 * x)
            - 0.01168 * std::cos(3 * x);
    }
    std::vector<double> harmonicPhase(64);
    for (double& ph : harmonicPhase) {
        ph = std::uniform_real_distribution<double>(0.0, 2.0 * kPi)(g);
    }
    std::vector<std::complex<double>> buf(kFftSize);
    std::vector<float> bins(kFftSize);

    // Audio, S-meter.
    constexpr int kFs = 48000;
    constexpr int kBlock = 480;
    constexpr double kBlockSec = static_cast<double>(kBlock) / kFs;
    const double hissAmp = std::pow(10.0, hissDb / 20.0);
    const double voiceAmp = station ? std::pow(10.0, (hissDb + snrDb) / 20.0) : 0.0;
    const double carrierAudioAmp = band.carrierDb > -900.0
        ? std::pow(10.0, (hissDb + band.carrierDb) / 20.0) * std::sqrt(2.0) : 0.0;
    const double noiseDbm = -120.0;
    double meter = noiseDbm;
    double sinceTick = 0.0;
    double sinceFrame = 0.0;
    std::vector<float> audio(static_cast<size_t>(kBlock) * 2);

    int overBlocks = 0;
    int presentBlocks = 0;
    int lateBlocks = 0;
    int gapBlocks = 0;
    int gapPresent = 0;
    Result r;
    double gainSum = 0.0;
    double lateSum = 0.0;
    const int blocks = static_cast<int>(kRunSec / kBlockSec);
    for (int b = 0; b < blocks; ++b) {
        const double t = b * kBlockSec;
        const bool talk = station && talking(t);

        // S-meter: noise plus the station's power, smoothed over 0.1 s.
        const double inst = std::pow(10.0, noiseDbm / 10.0) * noise.at(t)
            + (talk ? std::pow(10.0, (noiseDbm + snrDb) / 10.0) : 0.0)
            + (carrierAmp > 0.0 ? std::pow(10.0, (noiseDbm + band.carrierDb) / 10.0) : 0.0);
        meter += (1.0 - std::exp(-kBlockSec / 0.1)) * (10.0 * std::log10(inst) - meter);

        // Panadapter frame.
        sinceFrame += kBlockSec;
        if (useBins && sinceFrame >= kFrameSec - 1e-9) {
            const double f0 = pitchHz(t, g, pitches);
            int harmonics = 0;
            double weightSum = 0.0;
            for (int k = 1; k < 64; ++k) {
                const double f = k * f0;
                if (f >= 200.0 && f <= kPassHighHz) {
                    const double a = f < 500.0 ? 1.0 : 500.0 / f;
                    weightSum += a * a;
                    ++harmonics;
                }
            }
            for (int i = 0; i < kFftSize; ++i) {
                const double ti = t + static_cast<double>(i) / fsIq;
                const double a0 = plainNoise ? std::sqrt(0.5) : std::sqrt(0.5 * noise.at(ti));
                std::complex<double> s(n01(g) * a0, n01(g) * a0);
                if (carrierAmp > 0.0) {
                    s += std::polar(carrierAmp, 2.0 * kPi * kCarrierHz * ti);
                }
                if (station && talking(ti) && harmonics > 0) {
                    for (int k = 1; k < 64; ++k) {
                        const double f = k * f0;
                        if (f < 200.0 || f > kPassHighHz) {
                            continue;
                        }
                        const double a = (f < 500.0 ? 1.0 : 500.0 / f)
                            * std::sqrt(voicePower / weightSum);
                        s += std::polar(a, 2.0 * kPi * f * ti + harmonicPhase[static_cast<size_t>(k)]);
                    }
                }
                buf[static_cast<size_t>(i)] = s * window[static_cast<size_t>(i)];
            }
            fft(buf);
            for (int i = 0; i < kFftSize; ++i) {
                const std::complex<double> c = buf[static_cast<size_t>((i + kFftSize / 2) % kFftSize)];
                bins[static_cast<size_t>(i)] = static_cast<float>(10.0 * std::log10(std::norm(c) + 1e-30));
            }
            voice.addFrame(bins.data(), kFftSize, loBin, hiBin, sinceFrame);
            sinceFrame = 0.0;
        }

        // Gate, ten times a second.
        sinceTick += kBlockSec;
        if (sinceTick >= 0.1 - 1e-9) {
            RxStationReading reading;
            reading.signalDbm = meter + 0.7 * n01(g);
            reading.panFloorDbm = -150.0 + 0.3 * n01(g);
            reading.panFloorValid = true;
            reading.adcPeakDbfs = -40.0;
            reading.passbandVoice = useBins ? voice.take() : -1.0;
            gate.update(reading, sinceTick);
            sinceTick = 0.0;
        }
        p.stationPresent = gate.stationPresent();
        p.nearOverload = gate.nearOverload();

        for (int i = 0; i < kBlock; ++i) {
            const double ti = t + static_cast<double>(i) / kFs;
            const double h = plainNoise ? hissAmp : hissAmp * std::sqrt(noise.at(ti));
            const double s = h * n01(g) + (talk ? voiceAmp * n01(g) : 0.0)
                + carrierAudioAmp * std::sin(2.0 * kPi * kCarrierHz * ti);
            audio[static_cast<size_t>(2 * i)] = audio[static_cast<size_t>(2 * i + 1)] = static_cast<float>(s);
        }
        leveler.process(audio.data(), kBlock, kFs, p);

        const double gain = leveler.gainDb();
        if (station && t >= kOverStart && t < kOverEnd) {
            ++overBlocks;
            presentBlocks += p.stationPresent ? 1 : 0;
            gainSum += gain;
            r.gainMaxDb = std::max(r.gainMaxDb, gain);
            if (t >= kOverStart + 3.0) {
                lateSum += gain;
                ++lateBlocks;
            }
        } else if (t >= 2.0 && (!station || t < kOverStart || t >= kOverEnd + 3.0)) {
            // The hiss: before the over (after 2 s of learning), and from 3 s
            // after it (the station check's 2.5 s hold through pauses).
            ++gapBlocks;
            gapPresent += p.stationPresent ? 1 : 0;
            r.gapGainMaxDb = std::max(r.gapGainMaxDb, gain);
        }
    }
    r.presentShare = overBlocks > 0 ? static_cast<double>(presentBlocks) / overBlocks : 0.0;
    r.gainAvgDb = overBlocks > 0 ? gainSum / overBlocks : 0.0;
    r.gainLateDb = lateBlocks > 0 ? lateSum / lateBlocks : 0.0;
    r.gapPresentShare = static_cast<double>(gapPresent) / gapBlocks;
    // The voice alone (hiss out), over the syllables: what the leveler
    // aims at the target.
    const double voiceLoudDb = hissDb + snrDb;
    r.needDb = static_cast<double>(base.targetDb) - voiceLoudDb;
    return r;
}

}  // namespace NereusSDR::WeakStation
