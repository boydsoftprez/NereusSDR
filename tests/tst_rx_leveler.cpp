// no-port-check: NereusSDR-original test coverage.
// =================================================================
// tests/tst_rx_leveler.cpp  (NereusSDR)
// =================================================================
//
// The receive leveler (core/dsp/RxLeveler) and its station check
// (core/dsp/RxStationGate), on synthetic voice nets: speech, natural and
// heavily processed, at several strengths over band noise, through a peak
// AGC to the level the audio has when it reaches AudioEngine
// (tests/RxLevelerSynth.h).
//
// NEREUS_RXL_MATRIX=1 prints the measurement the Auto values were chosen
// from: every over's speech level before and after, the hiss in every gap,
// the loudest moment at the start of every over, the spread of levels;
// and the weak-station table (tests/RxWeakStationModel.h), the station
// check by the S-meter alone against the S-meter with the panadapter bins.
//
// Modification history (NereusSDR):
//   2026-11-14 — First version of the RX Leveler tests by Brian Sutton
//                (W4KZO), AI-assisted via xAI Grok.
//   2026-11-14 — Rewritten for the new engine and station check. J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code.
//   2026-11-15 — Weak stations: the panadapter check, the lower bars to
//                stay present, the larger Auto boost. J.J. Boyd (KG4VCF),
//                with AI-assisted transformation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "RxLevelerSynth.h"
#include "RxWeakStationModel.h"

#include <cstdio>
#include <cstring>
#include <random>

using namespace NereusSDR;
using namespace RxlSynth;

namespace {

// A net: overs of different stations with gaps of hiss between them, some
// long, some back to back.
std::vector<Segment> standardNet()
{
    return {
        {4, false},
        {10, true, false, 45, 11},   // strong, natural
        {6, false},
        {10, true, true, 45, 12},    // strong, processed
        {6, false},
        {10, true, false, 30, 13},   // weak, natural
        {6, false},
        {10, true, true, 45, 14},    // processed again, after a gap
        {0.4, false},
        {10, true, false, 32, 15},   // weak, back to back
        {0.4, false},
        {10, true, true, 45, 16},    // processed, back to back after a weak one
        {8, false},
    };
}

// The loudest 50 ms between `from` and `to`, dBFS RMS.
double loudest50ms(const std::vector<double>& x, int from, int to)
{
    const int w = static_cast<int>(0.05 * kRate);
    double m = -200.0;
    for (int i = from; i + w <= to; i += w) {
        m = std::max(m, rmsDb(x, i, i + w));
    }
    return m;
}

int blockOf(const Run& r, int sample)
{
    return std::min(static_cast<int>(r.gainDb.size()) - 1, sample / r.blockFrames);
}

void printMatrix(const char* title, const std::vector<Segment>& segs, double hissDb,
                 int block)
{
    const Net n = buildNet(segs, hissDb, 77);
    const Run r = runLeveler(n, autoParams(), block);
    std::printf("== %s (hiss %.0f dBFS, block %d) ==\n", title, hissDb, block);
    double minBefore = 1e9;
    double maxBefore = -1e9;
    double minAfter = 1e9;
    double maxAfter = -1e9;
    double peakIn = 0.0;
    double peakOut = 0.0;
    for (size_t i = 0; i < r.audio.size(); ++i) {
        peakIn = std::max(peakIn, std::abs(n.audio[i]));
        peakOut = std::max(peakOut, std::abs(r.audio[i]));
    }
    for (const auto& [a, b] : n.overs) {
        const int settled = a + static_cast<int>(2.0 * kRate);
        const double before = activeLevelDb(n.voice, settled, b);
        const double after = activeLevelDb(r.voice, settled, b);
        std::printf("  over %5.1f s: speech before %6.1f after %6.1f (gain at end %+5.1f)"
                    "  loudest 50 ms in its first second before %6.1f after %6.1f\n",
                    a / kRate, before, after, r.gainDb[static_cast<size_t>(blockOf(r, b - 1))],
                    loudest50ms(n.audio, a, a + static_cast<int>(kRate)),
                    loudest50ms(r.audio, a, a + static_cast<int>(kRate)));
        minBefore = std::min(minBefore, before);
        maxBefore = std::max(maxBefore, before);
        minAfter = std::min(minAfter, after);
        maxAfter = std::max(maxAfter, after);
    }
    for (const auto& [a, b] : n.gaps) {
        double maxGain = -1e9;
        for (int i = blockOf(r, a); i < blockOf(r, b); ++i) {
            maxGain = std::max(maxGain, r.gainDb[static_cast<size_t>(i)]);
        }
        const int last4 = std::max(a, b - static_cast<int>(4.0 * kRate));
        std::printf("  gap  %5.1f s: hiss before %6.1f after %6.1f, last 4 s before %6.1f"
                    " after %6.1f, most gain in the gap %+5.1f\n",
                    a / kRate, rmsDb(n.audio, a, b), rmsDb(r.audio, a, b),
                    rmsDb(n.audio, last4, b), rmsDb(r.audio, last4, b), maxGain);
    }
    if (!n.overs.empty()) {
        std::printf("  spread of speech levels: before %.1f dB, after %.1f dB\n",
                    maxBefore - minBefore, maxAfter - minAfter);
    }
    // The synthetic AGC has no delay line, so it overshoots at an onset;
    // the leveler holds back only what it adds.
    std::printf("  peak in %.3f, out %.3f\n", peakIn, peakOut);
}

std::vector<float> randomBlock(int frames, std::uint32_t seed, float amp)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> u(-amp, amp);
    std::vector<float> x(static_cast<size_t>(frames) * 2);
    for (float& v : x) {
        v = u(rng);
    }
    return x;
}

}  // namespace

class TestRxLeveler : public QObject {
    Q_OBJECT

private slots:
    // ── The engine ────────────────────────────────────────────────────────

    // Off touches nothing, whatever ran before.
    void offIsBitIdentical()
    {
        RxLeveler lev;
        RxLevelerParams on = autoParams();
        on.stationPresent = true;
        for (int i = 0; i < 200; ++i) {
            std::vector<float> x = randomBlock(1024, 1 + static_cast<std::uint32_t>(i), 0.02f);
            lev.process(x.data(), 1024, kRate, on);
        }
        QVERIFY(lev.gainDb() > 1.0);   // it was doing something

        const RxLevelerParams off;
        for (int i = 0; i < 20; ++i) {
            const std::vector<float> in = randomBlock(1024, 500 + static_cast<std::uint32_t>(i), 0.9f);
            std::vector<float> x = in;
            lev.process(x.data(), 1024, kRate, off);
            QVERIFY(std::memcmp(x.data(), in.data(), in.size() * sizeof(float)) == 0);
        }
        QCOMPARE(lev.gainDb(), 0.0);
        QCOMPARE(lev.limiterDb(), 0.0);
    }

    // One word to the audio thread, every field back as it went in, and
    // out-of-range values clamped to the operator ranges.
    void paramsPackAndUnpack()
    {
        RxLevelerParams p = autoParams();
        p.stationPresent = true;
        p.nearOverload = true;
        QCOMPARE(RxLevelerParams::unpack(p.pack()), p);

        RxLevelerParams lo;
        lo.on = true;
        lo.targetDb = RxLeveler::kTargetDbMin;
        lo.maxBoostDb = RxLeveler::kMaxBoostDbMin;
        lo.maxCutDb = RxLeveler::kMaxCutDbMin;
        lo.attackMs = RxLeveler::kAttackMsMin;
        lo.releaseMs = RxLeveler::kReleaseMsMin;
        QCOMPARE(RxLevelerParams::unpack(lo.pack()), lo);
        RxLevelerParams hi = lo;
        hi.targetDb = RxLeveler::kTargetDbMax;
        hi.maxBoostDb = RxLeveler::kMaxBoostDbMax;
        hi.maxCutDb = RxLeveler::kMaxCutDbMax;
        hi.attackMs = RxLeveler::kAttackMsMax;
        hi.releaseMs = RxLeveler::kReleaseMsMax;
        QCOMPARE(RxLevelerParams::unpack(hi.pack()), hi);

        RxLevelerParams wild = lo;
        wild.targetDb = 40;
        wild.maxBoostDb = -5;
        wild.maxCutDb = 99;
        wild.attackMs = 1;
        wild.releaseMs = 100000;
        const RxLevelerParams back = RxLevelerParams::unpack(wild.pack());
        QCOMPARE(back.targetDb, RxLeveler::kTargetDbMax);
        QCOMPARE(back.maxBoostDb, RxLeveler::kMaxBoostDbMin);
        QCOMPARE(back.maxCutDb, RxLeveler::kMaxCutDbMax);
        QCOMPARE(back.attackMs, RxLeveler::kAttackMsMin);
        QCOMPARE(back.releaseMs, RxLeveler::kReleaseMsMax);

        // Every Auto value sits inside its range.
        QVERIFY(RxLeveler::kAutoTargetDb >= RxLeveler::kTargetDbMin
                && RxLeveler::kAutoTargetDb <= RxLeveler::kTargetDbMax);
        QVERIFY(RxLeveler::kAutoMaxBoostDb >= RxLeveler::kMaxBoostDbMin
                && RxLeveler::kAutoMaxBoostDb <= RxLeveler::kMaxBoostDbMax);
        QVERIFY(RxLeveler::kAutoMaxCutDb >= RxLeveler::kMaxCutDbMin
                && RxLeveler::kAutoMaxCutDb <= RxLeveler::kMaxCutDbMax);
        QVERIFY(RxLeveler::kAutoAttackMs >= RxLeveler::kAttackMsMin
                && RxLeveler::kAutoAttackMs <= RxLeveler::kAttackMsMax);
        QVERIFY(RxLeveler::kAutoReleaseMs >= RxLeveler::kReleaseMsMin
                && RxLeveler::kAutoReleaseMs <= RxLeveler::kReleaseMsMax);
    }

    void voiceModesOnly()
    {
        for (DSPMode m : {DSPMode::LSB, DSPMode::USB, DSPMode::DSB, DSPMode::AM, DSPMode::SAM}) {
            QVERIFY(RxLeveler::modeAllows(m));
        }
        for (DSPMode m : {DSPMode::CWL, DSPMode::CWU, DSPMode::FM, DSPMode::DIGU,
                          DSPMode::DIGL, DSPMode::SPEC, DSPMode::DRM}) {
            QVERIFY(!RxLeveler::modeAllows(m));
        }
    }

    // A minute of band noise with no station: the leveler stays at unity
    // and the hiss comes out exactly as it went in.
    void noiseIsNotLifted()
    {
        for (double hiss : {-30.0, -40.0}) {
            const Net n = buildNet({{60, false}}, hiss, 31);
            const Run r = runLeveler(n, autoParams());
            for (double g : r.gainDb) {
                QCOMPARE(g, 0.0);
            }
            for (size_t i = 0; i < n.audio.size(); ++i) {
                QCOMPARE(r.audio[i], static_cast<double>(static_cast<float>(n.audio[i])));
            }
        }
    }

    // Between overs the gain climbs no more than 1 dB above where the over
    // left it (it may keep moving for the first 0.3 s, which it cannot tell
    // from a gap between words), and by two and a half seconds into a gap
    // the hiss is back to plain. (Two seconds with the old +10 dB Auto
    // boost: a weak station lifted +20 dB takes about half a second longer
    // to let go.)
    void gapsAreNotLifted()
    {
        for (double hiss : {-30.0, -40.0}) {
            for (int block : {256, 1024}) {
                const Net n = buildNet(standardNet(), hiss, 77);
                const Run r = runLeveler(n, autoParams(), block);
                for (const auto& [a, b] : n.gaps) {
                    const int first = blockOf(r, a);
                    const double atStart = first > 0 ? r.gainDb[static_cast<size_t>(first - 1)] : 0.0;
                    for (int i = first; i < blockOf(r, b); ++i) {
                        QVERIFY2(r.gainDb[static_cast<size_t>(i)] <= std::max(atStart, 0.0) + 1.0,
                                 qPrintable(QStringLiteral("gap at %1 s rose to %2 dB")
                                                .arg(a / kRate).arg(r.gainDb[static_cast<size_t>(i)])));
                    }
                    if (b - a >= static_cast<int>(4.0 * kRate)) {
                        const int from = a + static_cast<int>(2.5 * kRate);
                        const double plain = rmsDb(n.audio, from, b);
                        const double leveled = rmsDb(r.audio, from, b);
                        QVERIFY2(leveled <= plain + 0.5,
                                 qPrintable(QStringLiteral("gap at %1 s: hiss %2 dB, plain %3 dB")
                                                .arg(a / kRate).arg(leveled).arg(plain)));
                    }
                }
            }
        }
    }

    // A loud station after a gap, or straight after a weak one the gain was
    // up for, starts no louder than it would without the leveler; a weak
    // one may come up, but its start stays under the boost headroom.
    void noBlastWhenALoudStationFollows()
    {
        const std::vector<Segment> segs = standardNet();
        for (double hiss : {-30.0, -40.0}) {
            for (int block : {256, 1024}) {
                const Net n = buildNet(segs, hiss, 77);
                const Run r = runLeveler(n, autoParams(), block);
                size_t over = 0;
                for (const Segment& s : segs) {
                    if (!s.station) {
                        continue;
                    }
                    const auto [a, b] = n.overs[over++];
                    const int end = a + static_cast<int>(kRate);
                    const double plain = loudest50ms(n.audio, a, end);
                    const double leveled = loudest50ms(r.audio, a, end);
                    const bool loud = s.processed || s.snrDb >= 40.0;
                    const double limit = loud
                        ? plain + 0.5
                        : std::max(plain + 0.5,
                                   RxLeveler::kAutoTargetDb + RxLeveler::kBoostHeadroomDb);
                    QVERIFY2(leveled <= limit,
                             qPrintable(QStringLiteral("over at %1 s starts at %2 dB, plain %3 dB")
                                            .arg(a / kRate).arg(leveled).arg(plain)));
                }
            }
        }
    }

    // What it is for: weak stations come up, heavily processed ones come
    // down, a natural strong voice is left about where it was, and the
    // spread of levels across the net shrinks.
    void levelsTheNet()
    {
        const Net n = buildNet(standardNet(), -30.0, 77);
        const Run r = runLeveler(n, autoParams());
        const std::vector<Segment> segs = standardNet();
        double minBefore = 1e9;
        double maxBefore = -1e9;
        double minAfter = 1e9;
        double maxAfter = -1e9;
        size_t over = 0;
        for (const Segment& s : segs) {
            if (!s.station) {
                continue;
            }
            const auto [a, b] = n.overs[over++];
            const int settled = a + static_cast<int>(2.0 * kRate);
            const double before = activeLevelDb(n.voice, settled, b);
            const double after = activeLevelDb(r.voice, settled, b);
            const double change = after - before;
            if (s.processed) {
                QVERIFY2(change < -1.0, qPrintable(QStringLiteral("processed: %1 dB").arg(change)));
                QVERIFY(change >= -RxLeveler::kAutoMaxCutDb - 0.5);
            } else if (s.snrDb < 40.0) {
                QVERIFY2(change > 2.5, qPrintable(QStringLiteral("weak: %1 dB").arg(change)));
                QVERIFY(change <= RxLeveler::kAutoMaxBoostDb + 0.5);
            } else {
                QVERIFY2(std::abs(change) < 2.0, qPrintable(QStringLiteral("natural: %1 dB").arg(change)));
            }
            minBefore = std::min(minBefore, before);
            maxBefore = std::max(maxBefore, before);
            minAfter = std::min(minAfter, after);
            maxAfter = std::max(maxAfter, after);
        }
        QVERIFY2(maxAfter - minAfter < maxBefore - minBefore - 3.0,
                 qPrintable(QStringLiteral("spread %1 dB -> %2 dB")
                                .arg(maxBefore - minBefore).arg(maxAfter - minAfter)));
    }

    // Near ADC overload the leveler may still cut, never lift.
    void noBoostNearOverload()
    {
        const Net n = buildNet({{2, false}, {20, true, false, 25, 3}}, -30.0, 41);
        RxLevelerParams p = autoParams();
        p.stationPresent = true;
        p.nearOverload = true;
        const Run r = runLeveler(n, p, 1024, /*useGate=*/false);
        for (double g : r.gainDb) {
            QVERIFY(g <= 0.0);
        }
        // The same station without the overload is lifted, so the check
        // above means something.
        p.nearOverload = false;
        const Run lifted = runLeveler(n, p, 1024, false);
        QVERIFY(lifted.gainDb.back() > 3.0);
    }

    // Lifting never takes a peak past -1 dBFS, even when a burst arrives
    // with the gain all the way up; and at or below unity the limiter
    // leaves the audio's own peaks alone.
    void ceilingHolds()
    {
        RxLeveler lev;
        RxLevelerParams p = autoParams();
        p.stationPresent = true;
        p.maxBoostDb = RxLeveler::kMaxBoostDbMax;
        p.targetDb = RxLeveler::kTargetDbMax;
        // A quiet voice gets the gain up.
        const std::vector<double> quiet = speech(10.0, 5, false);
        std::vector<float> buf(2048);
        for (size_t pos = 0; pos + 1024 <= quiet.size(); pos += 1024) {
            for (int i = 0; i < 1024; ++i) {
                buf[static_cast<size_t>(2 * i)] = buf[static_cast<size_t>(2 * i + 1)]
                    = static_cast<float>(0.03 * quiet[pos + static_cast<size_t>(i)]);
            }
            lev.process(buf.data(), 1024, kRate, p);
        }
        QVERIFY(lev.gainDb() > 10.0);
        // Then a loud burst mid-block.
        for (int b = 0; b < 20; ++b) {
            for (int i = 0; i < 1024; ++i) {
                const double v = (b == 0 && i < 500) ? 0.0
                    : 0.7 * std::sin(2.0 * kPi * 700.0 * (b * 1024 + i) / kRate);
                buf[static_cast<size_t>(2 * i)] = buf[static_cast<size_t>(2 * i + 1)] = static_cast<float>(v);
            }
            lev.process(buf.data(), 1024, kRate, p);
            for (float v : buf) {
                QVERIFY2(std::abs(v) <= RxLeveler::kCeiling + 1e-6,
                         qPrintable(QStringLiteral("block %1: %2").arg(b).arg(v)));
            }
        }

        // Unity, a peak above the ceiling: untouched.
        RxLeveler unity;
        RxLevelerParams none = autoParams();
        const std::vector<float> in = randomBlock(1024, 9, 0.98f);
        std::vector<float> x = in;
        unity.process(x.data(), 1024, kRate, none);
        QVERIFY(std::memcmp(x.data(), in.data(), in.size() * sizeof(float)) == 0);
    }

    // ── The station check ─────────────────────────────────────────────────

    void gateNoiseIsNoStation()
    {
        RxStationGate gate;
        std::mt19937 rng(3);
        std::normal_distribution<double> wob(0.0, 1.0);
        for (int i = 0; i < 600; ++i) {
            RxStationReading r;
            r.signalDbm = -120.0 + wob(rng);
            // A single noise crash now and then.
            if (i % 50 == 25) {
                r.signalDbm += 15.0;
            }
            r.panFloorDbm = -137.0;
            r.panFloorValid = true;
            r.adcPeakDbfs = -30.0;
            gate.update(r, 0.1);
            QVERIFY2(!gate.stationPresent(), qPrintable(QStringLiteral("reading %1").arg(i)));
        }
        QVERIFY(gate.anchorLearned());
        QVERIFY(!gate.nearOverload());
    }

    void gateStationShowsAndHolds()
    {
        RxStationGate gate;
        RxStationReading r;
        r.panFloorDbm = -137.0;
        r.panFloorValid = true;
        r.adcPeakDbfs = -30.0;
        r.signalDbm = -120.0;
        for (int i = 0; i < 30; ++i) {
            gate.update(r, 0.1);
        }
        r.signalDbm = -105.0;
        gate.update(r, 0.1);
        QVERIFY(!gate.stationPresent());          // one reading is not enough
        gate.update(r, 0.1);
        QVERIFY(gate.stationPresent());
        // A pause: present through the hold, then gone.
        r.signalDbm = -120.0;
        double held = 0.0;
        while (gate.stationPresent() && held < 10.0) {
            gate.update(r, 0.1);
            held += 0.1;
        }
        QVERIFY(held >= RxStationGate::kPresentHoldSec - 0.15);
        QVERIFY(held <= RxStationGate::kPresentHoldSec + 0.15);
    }

    // An AM carrier sits on frequency for a long over. With the panadapter
    // floor known, the quiet level stays on the band's noise and the
    // station stays present; with no panadapter floor the quiet level
    // creeps up to the carrier and it fades out.
    void gateLongCarrierStaysWithPanadapterFloor()
    {
        for (bool pan : {true, false}) {
            RxStationGate gate;
            RxStationReading r;
            r.panFloorDbm = -137.0;
            r.panFloorValid = pan;
            r.adcPeakDbfs = -30.0;
            r.signalDbm = -120.0;
            for (int i = 0; i < 40; ++i) {
                gate.update(r, 0.1);
            }
            r.signalDbm = -100.0;
            for (int i = 0; i < 600; ++i) {
                gate.update(r, 0.1);
            }
            QCOMPARE(gate.stationPresent(), pan);
        }
    }

    void gateNearOverloadHolds()
    {
        RxStationGate gate;
        RxStationReading r;
        r.signalDbm = -120.0;
        r.adcPeakDbfs = -30.0;
        gate.update(r, 0.1);
        QVERIFY(!gate.nearOverload());
        r.adcPeakDbfs = -2.0;
        gate.update(r, 0.1);
        QVERIFY(gate.nearOverload());
        r.adcPeakDbfs = -30.0;
        gate.reset();                               // a retune keeps it
        QVERIFY(gate.nearOverload());
        for (int i = 0; i < 19; ++i) {
            gate.update(r, 0.1);
        }
        QVERIFY(gate.nearOverload());
        gate.update(r, 0.15);
        QVERIFY(!gate.nearOverload());
        r.adcOverload = true;                       // the radio's own flag
        gate.update(r, 0.1);
        QVERIFY(gate.nearOverload());
    }

    void gateNoReadingAndReset()
    {
        RxStationGate gate;
        RxStationReading r;
        r.signalDbm = -120.0;
        for (int i = 0; i < 10; ++i) {
            gate.update(r, 0.1);
        }
        r.signalDbm = -100.0;
        gate.update(r, 0.1);
        gate.update(r, 0.1);
        QVERIFY(gate.stationPresent());
        r.signalDbm = -400.0;                       // radio down
        gate.update(r, 0.1);
        QVERIFY(!gate.stationPresent());
        r.signalDbm = -100.0;
        gate.update(r, 0.1);
        gate.update(r, 0.1);
        QVERIFY(gate.stationPresent());
        gate.reset();
        QVERIFY(!gate.stationPresent());
        QCOMPARE(gate.quietLevelDbm(), RxStationGate::kNoReadingDbm);
        QVERIFY(!gate.anchorLearned());
    }

    // ── Weak stations ─────────────────────────────────────────────────────

    // The panadapter check on band noise alone: under the bar to enter, and
    // under the bar to stay, frame after frame (a second of frames per
    // reading, so the worst of many readings is seen).
    void passbandNoiseIsNoVoice()
    {
        using WeakStation::detail::kFftSize;
        using WeakStation::detail::fft;
        constexpr double kTwoPi = 2.0 * WeakStation::detail::kPi;
        std::mt19937 g(11);
        std::normal_distribution<double> n01(0.0, 1.0);
        RxPassbandVoice voice;
        std::vector<std::complex<double>> buf(kFftSize);
        std::vector<float> bins(kFftSize);
        double worst = 0.0;
        for (int tick = 0; tick < 300; ++tick) {
            for (int f = 0; f < 3; ++f) {
                for (int i = 0; i < kFftSize; ++i) {
                    const double x = kTwoPi * i / (kFftSize - 1);
                    const double w = 0.35875 - 0.48829 * std::cos(x) + 0.14128 * std::cos(2 * x)
                        - 0.01168 * std::cos(3 * x);
                    buf[static_cast<size_t>(i)] = std::complex<double>(n01(g), n01(g)) * w;
                }
                fft(buf);
                for (int i = 0; i < kFftSize; ++i) {
                    bins[static_cast<size_t>(i)] = static_cast<float>(
                        10.0 * std::log10(std::norm(buf[static_cast<size_t>((i + kFftSize / 2) % kFftSize)])));
                }
                voice.addFrame(bins.data(), kFftSize, kFftSize / 2 + 3, kFftSize / 2 + 62, 0.033);
            }
            worst = std::max(worst, voice.take());
        }
        QVERIFY2(worst < RxStationGate::kVoiceStayShare,
                 qPrintable(QStringLiteral("noise share %1").arg(worst)));
        QCOMPARE(voice.take(), -1.0);   // no frames since: no reading
    }

    // A carrier in the passband counts at first, then stops counting once
    // it has been steady for kSteadySec; a noise crash across the band
    // raises the reference with it and does not count.
    void passbandSteadyCarrierAndCrash()
    {
        constexpr int kBins = 1024;
        constexpr int lo = 515;
        constexpr int hi = 575;
        std::vector<float> flat(kBins, -140.0f);
        RxPassbandVoice voice;
        std::vector<float> carrier = flat;
        for (int i = 540; i < 545; ++i) {
            carrier[static_cast<size_t>(i)] = -100.0f;
        }
        voice.addFrame(carrier.data(), kBins, lo, hi, 0.033);
        QVERIFY(voice.take() >= RxStationGate::kVoiceEnterShare);
        for (int f = 0; f < 70; ++f) {
            voice.addFrame(carrier.data(), kBins, lo, hi, 0.033);
        }
        voice.take();
        voice.addFrame(carrier.data(), kBins, lo, hi, 0.033);
        QCOMPARE(voice.take(), 0.0);
        std::vector<float> crash(kBins, -110.0f);
        voice.addFrame(crash.data(), kBins, lo, hi, 0.033);
        QCOMPARE(voice.take(), 0.0);
        // Too few bins in the passband: no reading.
        voice.addFrame(carrier.data(), kBins, lo, lo + RxPassbandVoice::kMinBins - 1, 0.033);
        QCOMPARE(voice.take(), -1.0);
    }

    // The bins bring a station in at the higher bar and keep it at the
    // lower one; under the lower one it goes after the hold.
    void gateBinsEnterAndStay()
    {
        RxStationGate gate;
        RxStationReading r;
        r.signalDbm = -120.0;
        r.adcPeakDbfs = -30.0;
        for (int i = 0; i < 30; ++i) {
            gate.update(r, 0.1);
        }
        const double between = 0.5 * (RxStationGate::kVoiceEnterShare + RxStationGate::kVoiceStayShare);
        r.passbandVoice = between;
        for (int i = 0; i < 10; ++i) {
            gate.update(r, 0.1);
        }
        QVERIFY(!gate.stationPresent());           // under the bar to enter
        r.passbandVoice = RxStationGate::kVoiceEnterShare;
        gate.update(r, 0.1);
        QVERIFY(!gate.stationPresent());           // one reading is not enough
        gate.update(r, 0.1);
        QVERIFY(gate.stationPresent());
        r.passbandVoice = between;
        for (int i = 0; i < 100; ++i) {
            gate.update(r, 0.1);
        }
        QVERIFY(gate.stationPresent());            // kept at the lower bar
        r.passbandVoice = 0.0;
        for (int i = 0; i < 30; ++i) {
            gate.update(r, 0.1);
        }
        QVERIFY(!gate.stationPresent());
    }

    // JJ's bench: weak stations got no lift. A 30 s over from a station 3,
    // 6, 9, 12 and 20 dB over the band's noise, Auto settings, 47 Hz and
    // 94 Hz panadapter bins: counted present through the over, lifted to
    // the target (every one of these needs less than the Auto maximum),
    // and the hiss before and after the over neither counted nor lifted.
    void weakStationsAreHeardAndLifted()
    {
        RxLevelerParams autoParams;
        autoParams.targetDb = RxLeveler::kAutoTargetDb;
        autoParams.maxBoostDb = RxLeveler::kAutoMaxBoostDb;
        autoParams.maxCutDb = RxLeveler::kAutoMaxCutDb;
        autoParams.attackMs = RxLeveler::kAutoAttackMs;
        autoParams.releaseMs = RxLeveler::kAutoReleaseMs;
        for (double binHz : {47.0, 94.0}) {
            for (double snr : {3.0, 6.0, 9.0, 12.0, 20.0}) {
                const WeakStation::Result r = WeakStation::run(snr, -30.0, binHz, autoParams);
                const QString what = QStringLiteral("%1 dB, %2 Hz bins: present %3, late gain %4, need %5, "
                                                    "hiss present %6, hiss gain %7")
                    .arg(snr).arg(binHz).arg(r.presentShare).arg(r.gainLateDb).arg(r.needDb)
                    .arg(r.gapPresentShare).arg(r.gapGainMaxDb);
                QVERIFY2(r.needDb < RxLeveler::kAutoMaxBoostDb, qPrintable(what));
                QVERIFY2(r.presentShare >= 0.9, qPrintable(what));
                QVERIFY2(std::abs(r.gainLateDb - r.needDb) <= 2.5, qPrintable(what));
                QCOMPARE(r.gapPresentShare, 0.0);
                QVERIFY2(r.gapGainMaxDb < 1.0, qPrintable(what));
            }
        }
    }

    // The S-meter alone, as before the panadapter check: the 3 and 6 dB
    // stations are not heard. Kept so the table shows what changed.
    void weakStationsMeterAloneMissed()
    {
        RxLevelerParams autoParams;
        autoParams.targetDb = RxLeveler::kAutoTargetDb;
        autoParams.maxBoostDb = RxLeveler::kAutoMaxBoostDb;
        for (double snr : {3.0, 6.0}) {
            const WeakStation::Result r = WeakStation::run(snr, -30.0, 47.0, autoParams, /*useBins=*/false);
            QVERIFY2(r.presentShare < 0.2, qPrintable(QStringLiteral("%1 dB: %2").arg(snr).arg(r.presentShare)));
        }
    }

    void weakStationMatrix()
    {
        if (qEnvironmentVariableIntValue("NEREUS_RXL_MATRIX") != 1) {
            QSKIP("set NEREUS_RXL_MATRIX=1 to print the measurement");
        }
        for (double hissDb : {-30.0, -40.0}) {
            for (int boost : {10, 20, 25}) {
                for (bool bins : {false, true}) {
                    std::printf("\nhiss %.0f dBFS, maximum boost %d dB, %s\n", hissDb, boost,
                                bins ? "S-meter and panadapter" : "S-meter alone");
                    for (double snr : {3.0, 6.0, 9.0, 12.0, 15.0, 20.0}) {
                        RxLevelerParams p;
                        p.targetDb = RxLeveler::kAutoTargetDb;
                        p.maxBoostDb = boost;
                        p.maxCutDb = RxLeveler::kAutoMaxCutDb;
                        p.attackMs = RxLeveler::kAutoAttackMs;
                        p.releaseMs = RxLeveler::kAutoReleaseMs;
                        const WeakStation::Result r = WeakStation::run(snr, hissDb, 47.0, p, bins);
                        std::printf("  %4.0f dB over the noise: present %3.0f%%, lift %+5.1f dB (needs %+5.1f);"
                                    " hiss: present %3.0f%%, most lift %+4.1f dB\n",
                                    snr, 100.0 * r.presentShare, r.gainLateDb, r.needDb,
                                    100.0 * r.gapPresentShare, r.gapGainMaxDb);
                    }
                }
            }
        }
    }

    // ── The measurement ───────────────────────────────────────────────────

    void matrix()
    {
        if (qEnvironmentVariableIntValue("NEREUS_RXL_MATRIX") != 1) {
            QSKIP("set NEREUS_RXL_MATRIX=1 to print the measurement");
        }
        printMatrix("net", standardNet(), -30.0, 1024);
        printMatrix("net", standardNet(), -40.0, 1024);
        printMatrix("net, small blocks", standardNet(), -30.0, 256);
        printMatrix("noise only", {{60, false}}, -30.0, 1024);
    }
};

QTEST_APPLESS_MAIN(TestRxLeveler)
#include "tst_rx_leveler.moc"
