// no-port-check: NereusSDR-original test helper.
#pragma once

// Synthetic voice-net audio for the receive leveler's tests and its
// measurement matrix (tst_rx_leveler, NEREUS_RXL_MATRIX=1).
//
// A station is synthetic speech (voiced syllables with formants, words,
// phrase pauses), natural or heavily processed, at some strength over
// band noise. A simple peak AGC with WDSP's shape (fast attack, hang,
// then decay; a gain ceiling that stands in for AGC-T) brings it to the
// level the receive audio has when it reaches AudioEngine. The S-meter
// reading the leveler's gate uses is the passband power before the AGC,
// averaged over 100 ms like WDSP's (rxa.c create_meter, 0.100).
//
// Everything is deterministic from the seed.

#include "core/dsp/RxLeveler.h"
#include "core/dsp/RxStationGate.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace RxlSynth {

constexpr double kRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;

struct Biquad {
    double b0{1}, b1{0}, b2{0}, a1{0}, a2{0};
    double z1{0}, z2{0};
    double run(double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    static Biquad lowpass(double f, double q)
    {
        Biquad b;
        const double w = 2 * kPi * f / kRate, c = std::cos(w), s = std::sin(w) / (2 * q);
        const double a0 = 1 + s;
        b.b0 = (1 - c) / 2 / a0; b.b1 = (1 - c) / a0; b.b2 = b.b0;
        b.a1 = -2 * c / a0; b.a2 = (1 - s) / a0;
        return b;
    }
    static Biquad highpass(double f, double q)
    {
        Biquad b;
        const double w = 2 * kPi * f / kRate, c = std::cos(w), s = std::sin(w) / (2 * q);
        const double a0 = 1 + s;
        b.b0 = (1 + c) / 2 / a0; b.b1 = -(1 + c) / a0; b.b2 = b.b0;
        b.a1 = -2 * c / a0; b.a2 = (1 - s) / a0;
        return b;
    }
    static Biquad bandpass(double f, double q)
    {
        Biquad b;
        const double w = 2 * kPi * f / kRate, c = std::cos(w), s = std::sin(w) / (2 * q);
        const double a0 = 1 + s;
        b.b0 = s / a0; b.b1 = 0; b.b2 = -s / a0;
        b.a1 = -2 * c / a0; b.a2 = (1 - s) / a0;
        return b;
    }
};

// SSB passband, 300-2700 Hz.
struct Passband {
    Biquad hp1 = Biquad::highpass(300, 0.707), hp2 = Biquad::highpass(300, 0.707);
    Biquad lp1 = Biquad::lowpass(2700, 0.707), lp2 = Biquad::lowpass(2700, 0.707);
    double run(double x) { return lp2.run(lp1.run(hp2.run(hp1.run(x)))); }
};

// Unit-RMS band noise.
inline std::vector<double> bandNoise(int n, std::uint32_t seed)
{
    std::mt19937 rng(seed);
    std::normal_distribution<double> g(0.0, 1.0);
    Passband pb;
    std::vector<double> out(static_cast<std::size_t>(n));
    double acc = 0.0;
    for (int i = 0; i < n; ++i) {
        out[static_cast<std::size_t>(i)] = pb.run(g(rng));
        acc += out[static_cast<std::size_t>(i)] * out[static_cast<std::size_t>(i)];
    }
    const double rms = std::sqrt(acc / std::max(1, n));
    for (double& v : out) { v /= rms; }
    return out;
}

// Synthetic speech, `seconds` long, unit peak. Natural speech varies its
// syllables by several dB; `processed` runs it through a fast compressor
// and a soft clipper, the way a heavily processed transmitter sounds.
inline std::vector<double> speech(double seconds, std::uint32_t seed, bool processed)
{
    const int n = static_cast<int>(seconds * kRate);
    std::vector<double> out(static_cast<std::size_t>(n), 0.0);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> u(0.0, 1.0);
    std::normal_distribution<double> g(0.0, 1.0);
    const double vowels[][2] = {{730, 1090}, {270, 2290}, {530, 1840}, {660, 1720},
                                {300, 870},  {640, 1190}, {490, 1350}};
    int pos = 0;
    double f0 = 120.0, phase = 0.0;
    while (pos < n) {
        const int words = 3 + static_cast<int>(u(rng) * 5);           // a phrase
        for (int w = 0; w < words && pos < n; ++w) {
            const int syl = 1 + static_cast<int>(u(rng) * 3);
            for (int s = 0; s < syl && pos < n; ++s) {
                const int len = static_cast<int>((0.12 + 0.13 * u(rng)) * kRate);
                const double ampDb = std::clamp(g(rng) * 5.0, -14.0, 6.0);
                const double amp = std::pow(10.0, ampDb / 20.0);
                const auto& v = vowels[static_cast<int>(u(rng) * 7) % 7];
                Biquad f1 = Biquad::bandpass(v[0], 5.0), f2 = Biquad::bandpass(v[1], 8.0);
                Biquad f3 = Biquad::bandpass(2600, 10.0);
                // A short consonant first: a noise burst.
                const int cons = static_cast<int>(0.03 * kRate);
                Biquad ch = Biquad::bandpass(2200, 1.5);
                for (int i = 0; i < cons && pos < n; ++i, ++pos) {
                    out[static_cast<std::size_t>(pos)] += 0.12 * amp * ch.run(g(rng));
                }
                for (int i = 0; i < len && pos < n; ++i, ++pos) {
                    f0 = std::clamp(f0 + g(rng) * 0.05, 95.0, 160.0);
                    phase += f0 / kRate;
                    double pulse = 0.0;
                    if (phase >= 1.0) { phase -= 1.0; pulse = 1.0; }
                    const double src = pulse * 40.0 + g(rng) * 0.02;
                    const double env = std::sin(kPi * static_cast<double>(i) / len);
                    const double y = f1.run(src) + 0.6 * f2.run(src) + 0.25 * f3.run(src);
                    out[static_cast<std::size_t>(pos)] += amp * env * y;
                }
                pos += static_cast<int>((0.02 + 0.04 * u(rng)) * kRate);
            }
            pos += static_cast<int>((0.08 + 0.17 * u(rng)) * kRate);
        }
        pos += static_cast<int>((0.35 + 0.55 * u(rng)) * kRate);
    }
    Passband pb;
    for (double& v : out) { v = pb.run(v); }
    if (processed) {
        // Fast compressor (1 ms attack, 80 ms release, 20:1 above -40 dB of
        // the peak), then a clipper driven 8 dB into its knee and the
        // passband again: about 9 dB from peak to speech level, against
        // about 18 dB for the natural voice.
        double peak = 0.0;
        for (double v : out) { peak = std::max(peak, std::abs(v)); }
        const double thr = peak * std::pow(10.0, -40.0 / 20.0);
        double env = 0.0;
        const double att = 1.0 - std::exp(-1.0 / (0.001 * kRate));
        const double rel = 1.0 - std::exp(-1.0 / (0.080 * kRate));
        const double drive = std::pow(10.0, 8.0 / 20.0);
        for (double& v : out) {
            const double a = std::abs(v);
            env += (a > env ? att : rel) * (a - env);
            double gain = 1.0;
            if (env > thr) {
                const double overDb = 20.0 * std::log10(env / thr);
                gain = std::pow(10.0, -(overDb * (1.0 - 1.0 / 20.0)) / 20.0);
            }
            v = std::clamp(v * gain / thr * drive, -1.0, 1.0);
        }
        Passband pb2;
        for (double& v : out) { v = pb2.run(v); }
    }
    double peak = 0.0;
    for (double v : out) { peak = std::max(peak, std::abs(v)); }
    if (peak > 0.0) { for (double& v : out) { v /= peak; } }
    return out;
}

// RMS over the parts of `x` within 20 dB of its loudest 50 ms, in dBFS:
// the speech level, not counting the pauses.
inline double activeLevelDb(const std::vector<double>& x, int from, int to)
{
    const int win = static_cast<int>(0.05 * kRate);
    std::vector<double> p;
    for (int i = std::max(0, from); i + win <= std::min<int>(to, static_cast<int>(x.size())); i += win) {
        double s = 0.0;
        for (int k = 0; k < win; ++k) { s += x[static_cast<std::size_t>(i + k)] * x[static_cast<std::size_t>(i + k)]; }
        p.push_back(s / win);
    }
    if (p.empty()) { return -200.0; }
    const double mx = *std::max_element(p.begin(), p.end());
    double s = 0.0; int c = 0;
    for (double v : p) { if (v > mx * 0.01) { s += v; ++c; } }
    return 10.0 * std::log10(std::max(s / std::max(c, 1), 1e-20));
}

inline double rmsDb(const std::vector<double>& x, int from, int to)
{
    double s = 0.0; int c = 0;
    for (int i = std::max(0, from); i < std::min<int>(to, static_cast<int>(x.size())); ++i) {
        s += x[static_cast<std::size_t>(i)] * x[static_cast<std::size_t>(i)]; ++c;
    }
    return 10.0 * std::log10(std::max(s / std::max(c, 1), 1e-20));
}

// A peak AGC with WDSP's shape: 1 ms attack, 250 ms hang, then a 250 ms
// decay, aiming peaks at 0.98. maxGainDb stands in for AGC-T: a signal
// weaker than that gain can lift comes out quieter.
struct Agc {
    double maxGain;
    double env{0.0};
    double hang{0.0};
    double lastGain{1.0};
    explicit Agc(double maxGainDb) : maxGain(std::pow(10.0, maxGainDb / 20.0)) {}
    double run(double x)
    {
        const double a = std::abs(x);
        if (a > env) {
            env += (1.0 - std::exp(-1.0 / (0.001 * kRate))) * (a - env);
            hang = 0.25 * kRate;
        } else if (hang > 0) {
            hang -= 1;
        } else {
            env += (1.0 - std::exp(-1.0 / (0.25 * kRate))) * (a - env);
        }
        const double g = std::min(maxGain, 0.98 / std::max(env, 1e-12));
        lastGain = g;
        return x * g;
    }
};

// One over or one gap of a net.
struct Segment {
    double seconds{0.0};
    bool   station{false};
    bool   processed{false};
    double snrDb{30.0};        // speech peak power over noise power (passband)
    std::uint32_t seed{1};
};

struct Net {
    std::vector<double> audio;        // after the AGC, before the leveler
    std::vector<double> voice;        // the voice alone, at the AGC's gain
    std::vector<double> sMeterDbm;    // one reading per 100 ms
    std::vector<double> panFloorDbm;  // one per 100 ms
    std::vector<std::pair<int, int>> overs;  // sample ranges of each over
    std::vector<std::pair<int, int>> gaps;
};

// Noise power on the S-meter: -120 dBm in the passband; the panadapter
// floor reads 17 dB lower (a narrow bin), plus a small wobble.
// noiseOutDb is the hiss level the AGC lets through with no station, in
// dBFS RMS: where the operator's AGC-T sits.
inline Net buildNet(const std::vector<Segment>& segs, double noiseOutDb,
                    std::uint32_t noiseSeed = 99)
{
    const double noiseRms = 1e-3;
    const double agcMaxGainDb = noiseOutDb - 20.0 * std::log10(noiseRms);
    int total = 0;
    for (const auto& s : segs) { total += static_cast<int>(s.seconds * kRate); }
    Net net;
    std::vector<double> rf = bandNoise(total, noiseSeed);
    for (double& v : rf) { v *= noiseRms; }
    std::vector<double> voiceIn(static_cast<std::size_t>(total), 0.0);
    int pos = 0;
    for (const auto& s : segs) {
        const int len = static_cast<int>(s.seconds * kRate);
        if (s.station) {
            const std::vector<double> sp = speech(s.seconds, s.seed, s.processed);
            const double amp = noiseRms * std::pow(10.0, s.snrDb / 20.0);
            for (int i = 0; i < len; ++i) {
                const double v = amp * sp[static_cast<std::size_t>(i)];
                rf[static_cast<std::size_t>(pos + i)] += v;
                voiceIn[static_cast<std::size_t>(pos + i)] = v;
            }
            net.overs.push_back({pos, pos + len});
        } else {
            net.gaps.push_back({pos, pos + len});
        }
        pos += len;
    }
    // S-meter: 100 ms average of passband power, read every 100 ms.
    const double noisePow = noiseRms * noiseRms;
    const double avg = 1.0 - std::exp(-1.0 / (0.1 * kRate));
    double m = noisePow;
    const int tick = static_cast<int>(0.1 * kRate);
    std::mt19937 rng(noiseSeed + 7);
    std::normal_distribution<double> wob(0.0, 0.3);
    for (int i = 0; i < total; ++i) {
        m += avg * (rf[static_cast<std::size_t>(i)] * rf[static_cast<std::size_t>(i)] - m);
        if ((i + 1) % tick == 0) {
            net.sMeterDbm.push_back(-120.0 + 10.0 * std::log10(m / noisePow));
            net.panFloorDbm.push_back(-137.0 + wob(rng));
        }
    }
    Agc agc(agcMaxGainDb);
    net.audio.resize(static_cast<std::size_t>(total));
    net.voice.resize(static_cast<std::size_t>(total));
    for (int i = 0; i < total; ++i) {
        const std::size_t k = static_cast<std::size_t>(i);
        net.audio[k] = agc.run(rf[k]);
        net.voice[k] = voiceIn[k] * agc.lastGain;
    }
    return net;
}

// Runs `net` through the gate and the leveler the way AudioEngine and
// RadioModel do: the gate reads the S-meter every 100 ms and its verdict
// reaches the audio from the next block on; the audio goes in blocks of
// `blockFrames`. Returns the leveled audio (mono) and, per block, the gain.
struct Run {
    std::vector<double> audio;
    std::vector<double> voice;        // the voice alone, at the leveler's gain
    std::vector<double> gainDb;       // per block
    std::vector<bool>   present;      // per block
    int blockFrames{0};
};

inline Run runLeveler(const Net& net, NereusSDR::RxLevelerParams params,
                      int blockFrames = 1024, bool useGate = true)
{
    Run r;
    r.blockFrames = blockFrames;
    r.audio = net.audio;
    r.voice = net.voice;
    NereusSDR::RxLeveler lev;
    NereusSDR::RxStationGate gate;
    const int tick = static_cast<int>(0.1 * kRate);
    std::vector<float> buf(static_cast<std::size_t>(blockFrames) * 2);
    int nextTick = tick;
    std::size_t tickIndex = 0;
    const int total = static_cast<int>(net.audio.size());
    for (int pos = 0; pos < total; pos += blockFrames) {
        const int n = std::min(blockFrames, total - pos);
        while (pos >= nextTick && tickIndex < net.sMeterDbm.size()) {
            NereusSDR::RxStationReading rd;
            rd.signalDbm = net.sMeterDbm[tickIndex];
            rd.panFloorDbm = net.panFloorDbm[tickIndex];
            rd.panFloorValid = true;
            rd.adcPeakDbfs = -30.0;
            gate.update(rd, 0.1);
            ++tickIndex;
            nextTick += tick;
        }
        NereusSDR::RxLevelerParams p = params;
        if (useGate) {
            p.stationPresent = gate.stationPresent();
            p.nearOverload = gate.nearOverload();
        }
        for (int i = 0; i < n; ++i) {
            buf[static_cast<std::size_t>(2 * i)] = static_cast<float>(net.audio[static_cast<std::size_t>(pos + i)]);
            buf[static_cast<std::size_t>(2 * i + 1)] = buf[static_cast<std::size_t>(2 * i)];
        }
        lev.process(buf.data(), n, kRate, p);
        for (int i = 0; i < n; ++i) {
            r.audio[static_cast<std::size_t>(pos + i)] = buf[static_cast<std::size_t>(2 * i)];
        }
        const double g = std::pow(10.0, (lev.gainDb() + lev.limiterDb()) / 20.0);
        for (int i = 0; i < n; ++i) { r.voice[static_cast<std::size_t>(pos + i)] *= g; }
        r.gainDb.push_back(lev.gainDb());
        r.present.push_back(p.stationPresent);
    }
    return r;
}

inline NereusSDR::RxLevelerParams autoParams()
{
    using NereusSDR::RxLeveler;
    NereusSDR::RxLevelerParams p;
    p.on = true;
    p.targetDb = RxLeveler::kAutoTargetDb;
    p.maxBoostDb = RxLeveler::kAutoMaxBoostDb;
    p.maxCutDb = RxLeveler::kAutoMaxCutDb;
    p.attackMs = RxLeveler::kAutoAttackMs;
    p.releaseMs = RxLeveler::kAutoReleaseMs;
    return p;
}

}  // namespace RxlSynth
