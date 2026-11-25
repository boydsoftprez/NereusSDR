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
//   2026-11-16 — Nobody talking: static, fading noise and a carrier with
//                no station are neither counted nor lifted, and a weak
//                station through static still is. J.J. Boyd (KG4VCF), with
//                AI-assisted transformation via Anthropic Claude Code.
//   2026-11-16 — A neighbor overlapping one edge: the measuring band, the
//                edge bins left out, the level steady while the neighbor
//                keys, the hold after a turn-down. J.J. Boyd (KG4VCF),
//                with AI-assisted transformation via Anthropic Claude Code.
//   2026-11-22 - Review fixes: the trimmed reading of a panned or 90 degree
//                pair agrees with the whole block, the measuring band and
//                the bins' edges follow the mode, the reset count rides in
//                the word, the hold is timed from when it is armed and a
//                pause's let-go arms none, std::numbers::pi. J.J. Boyd
//                (KG4VCF), AI-assisted via Claude Code.
//   2026-11-22 - Re-review: hiss after an over is not lifted with a
//                station still said present, a start over lets the gain go
//                without a step (a filter drag too), a pause with the
//                station there from the start. J.J. Boyd (KG4VCF),
//                AI-assisted via Claude Code.
//   2026-11-25 - The nets run on both sets of voices, a weak station's
//                settled gain is checked against what it needs when its peaks
//                stand at full scale, and its lift when they have room; a word
//                tail when the station check comes late lifts nothing.
//                J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "RxLevelerSynth.h"
#include "RxWeakStationModel.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <numbers>
#include <random>

using namespace NereusSDR;
using namespace RxlSynth;

namespace {

// Sets the synthetic voices (RxlSynth::voices) and puts them back.
struct UseVoices {
    explicit UseVoices(Voices v) { voices = v; }
    ~UseVoices() { voices = Voices::A; }
};

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

// The gain over the second half of an over (its median).
double settledGainDb(const Run& r, int a, int b)
{
    std::vector<double> gains(r.gainDb.begin() + blockOf(r, a + (b - a) / 2),
                              r.gainDb.begin() + blockOf(r, b));
    std::nth_element(gains.begin(), gains.begin() + gains.size() / 2, gains.end());
    return gains[gains.size() / 2];
}

// How far the leveled peaks pass the ceiling: kCeiling, or a block's own
// peak where it arrived higher.
double worstOverCeiling(const std::vector<double>& in, const Run& r)
{
    double worst = -1.0;
    const size_t block = static_cast<size_t>(r.blockFrames);
    for (size_t i = 0; i < in.size(); i += block) {
        double peakIn = 0.0;
        double peakOut = 0.0;
        for (size_t k = i; k < std::min(i + block, in.size()); ++k) {
            peakIn = std::max(peakIn, std::abs(in[k]));
            peakOut = std::max(peakOut, std::abs(r.audio[k]));
        }
        worst = std::max(worst, peakOut - std::max(RxLeveler::kCeiling, peakIn));
    }
    return worst;
}

}  // namespace

class TestRxLeveler : public QObject {
    Q_OBJECT

private:
    static void voiceSets()
    {
        QTest::addColumn<int>("set");
        QTest::newRow("voices A") << static_cast<int>(Voices::A);
        QTest::newRow("voices B") << static_cast<int>(Voices::B);
    }

private slots:
    // ── The engine ────────────────────────────────────────────────────────

    // Off touches nothing, whatever ran before.
    void offIsBitIdentical()
    {
        RxLeveler lev;
        RxLevelerParams on = autoParams();
        on.stationPresent = true;
        for (int i = 0; i < 200; ++i) {
            // Words with gaps between them, so the hiss is learned first.
            const float level = i % 20 < 10 ? 0.02f : 0.001f;
            std::vector<float> x = randomBlock(1024, 1 + static_cast<std::uint32_t>(i), level);
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
        p.resetCount = 21;
        RxLevelerParams back = RxLevelerParams::unpack(p.pack());
        RxLevelerParams::unpackBand(p.packBand(), back);
        QCOMPARE(back, p);
        // The reset count has its five bits to itself and goes round.
        for (int count : {0, 1, RxLevelerParams::kResetCountMask}) {
            RxLevelerParams c = p;
            c.resetCount = count;
            const RxLevelerParams cb = RxLevelerParams::unpack(c.pack());
            QCOMPARE(cb.resetCount, count);
            QCOMPARE(cb.on, c.on);
            QCOMPARE(cb.stationPresent, c.stationPresent);
            QCOMPARE(cb.nearOverload, c.nearOverload);
            QCOMPARE(cb.targetDb, c.targetDb);
        }
        RxLevelerParams wrap = p;
        wrap.resetCount = RxLevelerParams::kResetCountMask + 1;
        QCOMPARE(RxLevelerParams::unpack(wrap.pack()).resetCount, 0);

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
        back = RxLevelerParams::unpack(wild.pack());
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
    void gapsAreNotLifted_data() { voiceSets(); }
    void gapsAreNotLifted()
    {
        QFETCH(int, set);
        const UseVoices use(static_cast<Voices>(set));
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
    void noBlastWhenALoudStationFollows_data() { voiceSets(); }
    void noBlastWhenALoudStationFollows()
    {
        QFETCH(int, set);
        const UseVoices use(static_cast<Voices>(set));
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

    // The station check can come after a first word has ended. The 50 ms
    // level is still up from it while the block is bare hiss; that is no
    // voice, and nothing is lifted until one is heard.
    void aWordTailIsNotTheStation()
    {
        for (int block : {256, 1024}) {
            std::vector<double> x = bandNoise(static_cast<int>(6 * kRate), 5);
            for (double& v : x) {
                v *= 0.01;                          // hiss at -40 dBFS
            }
            for (int i = static_cast<int>(3.0 * kRate); i < static_cast<int>(3.2 * kRate); ++i) {
                x[static_cast<size_t>(i)] *= 20.0;  // a word at -14
            }
            RxLeveler lev;
            RxLevelerParams p = autoParams();
            std::vector<float> buf(static_cast<size_t>(block) * 2);
            for (int pos = 0; pos + block <= static_cast<int>(x.size()); pos += block) {
                for (int i = 0; i < block; ++i) {
                    buf[static_cast<size_t>(2 * i)] = static_cast<float>(x[static_cast<size_t>(pos + i)]);
                    buf[static_cast<size_t>(2 * i + 1)] = buf[static_cast<size_t>(2 * i)];
                }
                p.stationPresent = pos >= static_cast<int>(3.23 * kRate);
                lev.process(buf.data(), block, kRate, p);
                QVERIFY2(lev.gainDb() <= 0.1,
                         qPrintable(QStringLiteral("%1 s: %2 dB").arg(pos / kRate).arg(lev.gainDb())));
            }
        }
    }

    // What it is for: weak stations come up, heavily processed ones come
    // down, a natural strong voice is left about where it was, and the
    // spread of levels across the net shrinks.
    void levelsTheNet_data() { voiceSets(); }
    void levelsTheNet()
    {
        QFETCH(int, set);
        const UseVoices use(static_cast<Voices>(set));
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
                // Its peaks already stand at full scale after the AGC, so the
                // limiter holds them and the lift shows in the gain, which
                // settles at what the station needs. A weak station with
                // room is weakStationWithRoomComesUp.
                const double gain = settledGainDb(r, a, b);
                const double need = RxLeveler::kAutoTargetDb - before;
                qInfo("full-scale weak over at %.1f s: needs %.2f dB, gain %.2f dB, change %.2f dB",
                      a / kRate, need, gain, change);
                QVERIFY2(std::abs(gain - need) < 1.5,
                         qPrintable(QStringLiteral("weak: needs %1 dB, gets %2 dB").arg(need).arg(gain)));
                const double pastCeiling = worstOverCeiling(n.audio, r);
                qInfo("output peak over the ceiling: %.6f", pastCeiling);
                QVERIFY2(pastCeiling <= 1e-4, qPrintable(QStringLiteral("peak over the ceiling by %1").arg(pastCeiling)));
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

    // The same weak stations 10 dB lower at the leveler, their peaks well
    // under full scale: the lift is heard, with room for all of it.
    void weakStationWithRoomComesUp_data() { voiceSets(); }
    void weakStationWithRoomComesUp()
    {
        QFETCH(int, set);
        const UseVoices use(static_cast<Voices>(set));
        const std::vector<Segment> segs = standardNet();
        const Net n = buildNet(segs, -40.0, 77);
        const Run r = runLeveler(n, autoParams());
        size_t index = 0;
        for (const Segment& s : segs) {
            if (!s.station) {
                continue;
            }
            const auto [a, b] = n.overs[index++];
            if (s.processed || s.snrDb >= 40.0) {
                continue;
            }
            const int settled = a + static_cast<int>(2.0 * kRate);
            double peak = 0.0;
            for (int i = settled; i < b; ++i) {
                peak = std::max(peak, std::abs(n.audio[static_cast<size_t>(i)]));
            }
            const double peakDb = 20.0 * std::log10(std::max(peak, 1e-12));
            const double before = activeLevelDb(n.voice, settled, b);
            const double change = activeLevelDb(r.voice, settled, b) - before;
            qInfo("weak over with room at %.1f s: peak %.2f dBFS, needs %.2f dB, gain %.2f dB, change %.2f dB",
                  a / kRate, peakDb, RxLeveler::kAutoTargetDb - before, settledGainDb(r, a, b), change);
            // The room is real: 2.5 dB of lift and more under the ceiling.
            QVERIFY2(peakDb < -6.0, qPrintable(QStringLiteral("peak %1 dBFS").arg(peakDb)));
            QVERIFY2(change > 2.5, qPrintable(QStringLiteral("weak: %1 dB").arg(change)));
        }
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
        // No panadapter reading: the S-meter must stay up kMeterSustainSec.
        r.signalDbm = -105.0;
        for (int i = 0; i < 4; ++i) {
            gate.update(r, 0.1);
            QVERIFY(!gate.stationPresent());      // not long enough yet
        }
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
        for (int i = 0; i < 5; ++i) {
            gate.update(r, 0.1);
        }
        QVERIFY(gate.stationPresent());
        r.signalDbm = -400.0;                       // radio down
        gate.update(r, 0.1);
        QVERIFY(!gate.stationPresent());
        r.signalDbm = -100.0;
        for (int i = 0; i < 5; ++i) {
            gate.update(r, 0.1);
        }
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

    // ── Nobody talking ────────────────────────────────────────────────────

    // JJ on the air, 2026-11-16: the lift sometimes climbed to about +20 dB
    // with no station talking. Static crashes jumped the S-meter, the
    // station check took that for a station, and each crash renewed the
    // hold; with the band noise fading up a few dB the leveler then read
    // the hiss as a quiet voice and lifted it.
    //
    // No panadapter reading: a crash holds the S-meter up for less than
    // kMeterSustainSec, so crash after crash brings no station in.
    void gateMeterCrashesAreNoStation()
    {
        RxStationGate gate;
        RxStationReading r;
        r.adcPeakDbfs = -30.0;
        for (int i = 0; i < 600; ++i) {
            // A crash every second, holding the meter up 0.3 s.
            r.signalDbm = (i % 10) < 3 ? -105.0 : -120.0;
            gate.update(r, 0.1);
            QVERIFY2(!gate.stationPresent(), qPrintable(QStringLiteral("reading %1").arg(i)));
        }
    }

    // With the panadapter running its bins decide: the S-meter well up for
    // seconds (noise fading up, a burst of static) while the bins show
    // nothing brings no station in, and does not keep one either.
    void gateMeterAloneIsNoStationWithPanadapter()
    {
        RxStationGate gate;
        RxStationReading r;
        r.adcPeakDbfs = -30.0;
        r.signalDbm = -120.0;
        r.passbandVoice = 0.0;
        for (int i = 0; i < 30; ++i) {
            gate.update(r, 0.1);
        }
        r.signalDbm = -108.0;
        for (int i = 0; i < 50; ++i) {
            gate.update(r, 0.1);
            QVERIFY2(!gate.stationPresent(), qPrintable(QStringLiteral("reading %1").arg(i)));
        }
        // A station shows in the bins: present. The bins fall silent while
        // the S-meter stays up: gone after the hold.
        r.passbandVoice = RxStationGate::kVoiceEnterShare;
        gate.update(r, 0.1);
        gate.update(r, 0.1);
        QVERIFY(gate.stationPresent());
        r.passbandVoice = 0.0;
        for (int i = 0; i < 30; ++i) {
            gate.update(r, 0.1);
        }
        QVERIFY(!gate.stationPresent());
    }

    // End to end, with nobody on the frequency: steady noise, static
    // crashes, dense static, fading noise with crashes, a steady carrier,
    // the noise floor swelling several dB and back. With the panadapter the
    // station check never says a station is there and the lift never
    // rises. Without it (S-meter only) the same holds, except under dense
    // static (three loud crashes a second) and a swell of the floor, which
    // the S-meter cannot tell from a voice.
    void bandNoiseAloneIsNotLifted()
    {
        RxLevelerParams autoParams;
        autoParams.targetDb = RxLeveler::kAutoTargetDb;
        autoParams.maxBoostDb = RxLeveler::kAutoMaxBoostDb;
        autoParams.maxCutDb = RxLeveler::kAutoMaxCutDb;
        autoParams.attackMs = RxLeveler::kAutoAttackMs;
        autoParams.releaseMs = RxLeveler::kAutoReleaseMs;
        struct Case {
            const char* name;
            WeakStation::Band band;
            bool meterAloneToo;
        };
        const Case cases[] = {
            {"steady noise", {}, true},
            {"crashes 1/s 20 dB", {1.0, 20.0, 0.0}, true},
            {"crashes 3/s 25 dB", {3.0, 25.0, 0.0}, false},
            {"fading 4 dB, crashes 1/s 15 dB", {1.0, 15.0, 4.0}, true},
            {"fading 4 dB", {0.0, 20.0, 4.0}, true},
            {"carrier 10 dB", {0.0, 20.0, 0.0, 10.0}, true},
            // JJ: the climb seemed to line up with the noise floor surging
            // and falling. The S-meter cannot tell a swell of the floor
            // from a station; the bins can.
            {"noise floor swells 6 dB", {0.0, 20.0, 0.0, -999.0, 6.0}, false},
            {"noise floor swells 10 dB", {0.0, 20.0, 0.0, -999.0, 10.0}, false},
        };
        for (const Case& c : cases) {
            for (unsigned seed : {7U, 8U, 9U}) {
                for (bool bins : {true, false}) {
                    if (!bins && !c.meterAloneToo) {
                        continue;
                    }
                    const WeakStation::Result r = WeakStation::run(
                        WeakStation::kNoStation, -30.0, 47.0, autoParams, bins, seed, c.band);
                    const QString what = QStringLiteral("%1, seed %2, %3: present %4, most gain %5")
                        .arg(QLatin1String(c.name)).arg(seed)
                        .arg(bins ? QStringLiteral("panadapter") : QStringLiteral("S-meter only"))
                        .arg(r.gapPresentShare).arg(r.gapGainMaxDb);
                    QVERIFY2(r.gapPresentShare == 0.0, qPrintable(what));
                    QVERIFY2(r.gapGainMaxDb < 1.0, qPrintable(what));
                }
            }
        }
    }

    // The 2026-11-15 complaint must not come back: a weak station 3 and
    // 6 dB over the noise, with static crashes, is still heard through its
    // over and lifted to within 3 dB of what it needs, and the static
    // after the over is not taken for a station.
    void weakStationsThroughStaticAreLifted()
    {
        RxLevelerParams autoParams;
        autoParams.targetDb = RxLeveler::kAutoTargetDb;
        autoParams.maxBoostDb = RxLeveler::kAutoMaxBoostDb;
        autoParams.maxCutDb = RxLeveler::kAutoMaxCutDb;
        autoParams.attackMs = RxLeveler::kAutoAttackMs;
        autoParams.releaseMs = RxLeveler::kAutoReleaseMs;
        for (double snr : {3.0, 6.0}) {
            for (unsigned seed : {7U, 8U}) {
                const WeakStation::Result r = WeakStation::run(
                    snr, -30.0, 47.0, autoParams, true, seed, {1.0, 20.0, 0.0});
                const QString what = QStringLiteral("%1 dB, seed %2: present %3, late gain %4, need %5, "
                                                    "static present %6")
                    .arg(snr).arg(seed).arg(r.presentShare).arg(r.gainLateDb).arg(r.needDb)
                    .arg(r.gapPresentShare);
                QVERIFY2(r.presentShare >= 0.9, qPrintable(what));
                QVERIFY2(std::abs(r.gainLateDb - r.needDb) <= 3.0, qPrintable(what));
                QCOMPARE(r.gapPresentShare, 0.0);
            }
        }
    }

    // ── A neighbor overlapping one edge (JJ on the air, 2026-11-16) ──────

    // The loudness is read without the filter's outer edge: kEdgeShare of
    // its width at each edge away from the carrier, as audio frequencies.
    void measureBandFollowsTheFilter()
    {
        int lo = -1;
        int hi = -1;
        RxLeveler::measureBand(DSPMode::USB, 100, 2900, lo, hi);     // USB: the top 420 Hz off
        QCOMPARE(lo, 100);
        QCOMPARE(hi, 2480);
        RxLeveler::measureBand(DSPMode::LSB, -2900, -100, lo, hi);   // LSB: mirrored, the same audio
        QCOMPARE(lo, 100);
        QCOMPARE(hi, 2480);
        RxLeveler::measureBand(DSPMode::AM, -5000, 5000, lo, hi);    // AM: folded, outer edges trimmed
        QCOMPARE(lo, 0);
        QCOMPARE(hi, 3500);
        RxLeveler::measureBand(DSPMode::SAM, -5000, 5000, lo, hi);
        QCOMPARE(lo, 0);
        QCOMPARE(hi, 3500);
        RxLeveler::measureBand(DSPMode::DSB, -3000, 3000, lo, hi);
        QCOMPARE(lo, 0);
        QCOMPARE(hi, 2100);
        RxLeveler::measureBand(DSPMode::USB, 2900, 100, lo, hi);     // no filter: the whole block
        QCOMPARE(lo, 0);
        QCOMPARE(hi, 0);
        // A USB or LSB filter crossing the carrier: the sideband is the
        // mode's, so only the far edge is trimmed (450 Hz of 3000), not the
        // folded band the filter's sign gave (nothing left: the whole block).
        RxLeveler::measureBand(DSPMode::USB, -100, 2900, lo, hi);
        QCOMPARE(lo, 0);
        QCOMPARE(hi, 2450);
        RxLeveler::measureBand(DSPMode::LSB, -2900, 100, lo, hi);
        QCOMPARE(lo, 0);
        QCOMPARE(hi, 2450);
        bool trimLow = false;
        bool trimHigh = false;
        RxPassbandVoice::outerEdges(DSPMode::USB, 100, 2900, trimLow, trimHigh);
        QVERIFY(!trimLow && trimHigh);
        RxPassbandVoice::outerEdges(DSPMode::LSB, -2900, -100, trimLow, trimHigh);
        QVERIFY(trimLow && !trimHigh);
        RxPassbandVoice::outerEdges(DSPMode::AM, -5000, 5000, trimLow, trimHigh);
        QVERIFY(trimLow && trimHigh);
        RxPassbandVoice::outerEdges(DSPMode::USB, -100, 2900, trimLow, trimHigh);   // far edge only
        QVERIFY(!trimLow && trimHigh);
        RxPassbandVoice::outerEdges(DSPMode::LSB, -2900, 100, trimLow, trimHigh);
        QVERIFY(trimLow && !trimHigh);
        RxLevelerParams p;
        p.measureLowHz = 100;
        p.measureHighHz = 2480;
        RxLevelerParams back;
        RxLevelerParams::unpackBand(p.packBand(), back);
        QCOMPARE(back.measureLowHz, 100);
        QCOMPARE(back.measureHighHz, 2480);
    }

    // The trimmed reading of a receiver whose sides differ (panned hard to
    // one side; a BIN pair 90 degrees apart) agrees with the whole-block
    // reading within kMeasureOffsetDb, as a centred one does. Summing the
    // two sides before the filter read them 3 dB low, which gave a panned
    // or BIN receiver extra lift and a low no-blast headroom reading.
    void trimmedReadingMatchesWholeBlockWhenSidesDiffer_data()
    {
        QTest::addColumn<int>("pair");
        QTest::newRow("centred") << 0;
        QTest::newRow("hard pan left") << 1;
        QTest::newRow("hard pan right") << 2;
        QTest::newRow("90 degrees apart") << 3;
    }
    void trimmedReadingMatchesWholeBlockWhenSidesDiffer()
    {
        QFETCH(int, pair);
        constexpr int kBlock = 480;
        constexpr double kTwoPi = 2.0 * std::numbers::pi;
        const auto loudness = [pair](bool trimmed) {
            RxLevelerParams p = autoParams();
            p.stationPresent = true;
            p.measureLowHz = 0;
            p.measureHighHz = 0;
            if (trimmed) {
                RxLeveler::measureBand(DSPMode::USB, 100, 2900, p.measureLowHz, p.measureHighHz);
            }
            RxLeveler lev;
            std::vector<float> x(kBlock * 2);
            const int blocks = static_cast<int>(5.0 * kRate) / kBlock;
            for (int b = 0; b < blocks; ++b) {
                // A second of hiss before the station is said to be there.
                p.stationPresent = b * kBlock >= static_cast<int>(kRate);
                for (int k = 0; k < kBlock; ++k) {
                    const double t = (b * kBlock + k) / kRate;
                    // A voice-band pair of tones well inside the band, in
                    // syllables with gaps, over a small hiss tone.
                    const double ph1 = kTwoPi * 700.0 * t;
                    const double ph2 = kTwoPi * 1300.0 * t;
                    const double on = p.stationPresent && std::fmod(t, 0.25) < 0.17 ? 0.05 : 0.0;
                    const double hiss = 0.002 * std::sin(kTwoPi * 731.0 * t);
                    const double a = on * (std::sin(ph1) + 0.6 * std::sin(ph2)) + hiss;
                    const double q = on * (std::cos(ph1) + 0.6 * std::cos(ph2)) + hiss;
                    double left = a;
                    double right = a;
                    if (pair == 1) {
                        right = 0.0;
                    } else if (pair == 2) {
                        left = 0.0;
                    } else if (pair == 3) {
                        right = q;
                    }
                    x[2 * k] = static_cast<float>(left);
                    x[2 * k + 1] = static_cast<float>(right);
                }
                lev.process(x.data(), kBlock, kRate, p);
            }
            return lev.loudnessDb();
        };
        const double whole = loudness(false);
        const double trimmed = loudness(true);
        QVERIFY2(whole > RxLeveler::kSilenceDbfs, qPrintable(QStringLiteral("whole %1").arg(whole)));
        QVERIFY2(std::abs(trimmed - whole) <= RxLeveler::kMeasureOffsetDb + 0.25,
                 qPrintable(QStringLiteral("trimmed %1 dBFS, whole block %2 dBFS")
                                .arg(trimmed).arg(whole)));
    }

    // The trimmed reading is used only while the panadapter decides: the
    // gate says so for a bins reading within kPanadapterStaleSec.
    void gatePanadapterRunning()
    {
        RxStationGate gate;
        QVERIFY(!gate.panadapterRunning());
        RxStationReading r;
        r.signalDbm = -120.0;
        r.passbandVoice = 0.0;
        gate.update(r, 0.1);
        QVERIFY(gate.panadapterRunning());
        r.passbandVoice = -1.0;
        for (int i = 0; i < 9; ++i) {
            gate.update(r, 0.1);
        }
        QVERIFY(gate.panadapterRunning());      // 0.9 s: a slow display
        gate.update(r, 0.1);
        gate.update(r, 0.1);
        QVERIFY(!gate.panadapterRunning());
        r.passbandVoice = 0.0;
        gate.update(r, 0.1);
        gate.reset();
        QVERIFY(!gate.panadapterRunning());
    }

    // A neighbor filling the outer edge's bins is not a station; the same
    // signal inside the filter is.
    void passbandEdgeNeighborIsNoVoice()
    {
        constexpr int kBins = 1024;
        constexpr int lo = 515;
        constexpr int hi = 575;     // 60 bins, the top 9 left out (USB)
        const std::vector<float> flat(kBins, -140.0f);
        RxPassbandVoice voice;
        std::vector<float> edge = flat;
        for (int i = hi - 9; i < hi; ++i) {
            edge[static_cast<size_t>(i)] = -100.0f;
        }
        voice.addFrame(edge.data(), kBins, lo, hi, 0.033, false, true);
        QCOMPARE(voice.take(), 0.0);
        voice.addFrame(edge.data(), kBins, lo, hi, 0.033);     // untrimmed: counted
        QVERIFY(voice.take() >= RxStationGate::kVoiceEnterShare);
        std::vector<float> inside = flat;
        for (int i = 540; i < 549; ++i) {
            inside[static_cast<size_t>(i)] = -100.0f;
        }
        voice.addFrame(inside.data(), kBins, lo, hi, 0.033, false, true);
        QVERIFY(voice.take() >= RxStationGate::kVoiceEnterShare);
    }

    // A steady voice in a 100-2900 Hz filter and a louder neighbor in its
    // top 300 Hz keying 3 s on, 2 s off. Read without the outer edge, the
    // level holds still; read on the whole block (as before) it rode up
    // and down with the neighbor.
    void edgeNeighborDoesNotRideTheLevel()
    {
        const auto swing = [](bool trimmed) {
            constexpr int kBlock = 480;
            constexpr double kTwoPi = 2.0 * std::numbers::pi;
            RxLevelerParams p = autoParams();
            p.stationPresent = true;
            p.measureLowHz = 0;
            p.measureHighHz = 0;
            if (trimmed) {
                RxLeveler::measureBand(DSPMode::USB, 100, 2900, p.measureLowHz, p.measureHighHz);
            }
            RxLeveler lev;
            std::vector<float> x(kBlock * 2);
            std::vector<double> gains;
            const int blocks = static_cast<int>(40.0 * kRate) / kBlock;
            for (int b = 0; b < blocks; ++b) {
                for (int k = 0; k < kBlock; ++k) {
                    const double t = (b * kBlock + k) / kRate;
                    // Wanted: a quiet voice, syllables at 1 kHz and 1.4 kHz.
                    const bool syl = std::fmod(t, 0.25) < 0.17;
                    double v = syl ? 0.03 * (std::sin(kTwoPi * 1000.0 * t)
                                             + 0.5 * std::sin(kTwoPi * 1400.0 * t)) : 0.0;
                    // Neighbor: 15 dB louder, 2.65-2.85 kHz, 3 s on, 2 s off.
                    if (std::fmod(t, 5.0) < 3.0 && std::fmod(t + 0.1, 0.25) < 0.17) {
                        v += 0.17 * (std::sin(kTwoPi * 2650.0 * t) + std::sin(kTwoPi * 2850.0 * t));
                    }
                    v += 0.002 * std::sin(kTwoPi * 731.0 * t + 3.0 * std::sin(kTwoPi * 17.0 * t));
                    x[2 * k] = x[2 * k + 1] = static_cast<float>(v);
                }
                lev.process(x.data(), kBlock, kRate, p);
                if (b * kBlock >= 10.0 * kRate) {
                    gains.push_back(lev.gainDb());
                }
            }
            std::sort(gains.begin(), gains.end());
            return gains[gains.size() * 95 / 100] - gains[gains.size() * 5 / 100];
        };
        const double whole = swing(false);
        const double trimmed = swing(true);
        QVERIFY2(whole > 3.0, qPrintable(QStringLiteral("whole-block swing %1 dB").arg(whole)));
        QVERIFY2(trimmed < 1.5 && trimmed < whole / 3.0,
                 qPrintable(QStringLiteral("trimmed swing %1 dB, whole-block %2 dB")
                                .arg(trimmed).arg(whole)));
    }

    // After a turn-down for a louder voice the level holds kRiseHoldSec,
    // timed from when the hold is armed (the last turn-down that added up
    // to kTurnDownDb), before it rises for a quieter one.
    void riseHoldsAfterATurnDown()
    {
        constexpr int kBlock = 480;
        constexpr double kTwoPi = 2.0 * std::numbers::pi;
        constexpr double kDt = kBlock / kRate;
        RxLevelerParams p = autoParams();
        p.stationPresent = true;
        RxLeveler lev;
        std::vector<float> x(kBlock * 2);
        std::vector<double> gains;
        std::vector<double> holds;
        const int blocks = static_cast<int>(16.0 * kRate) / kBlock;
        int armed = -1;     // the block that last armed the hold
        double previousHold = 0.0;
        for (int b = 0; b < blocks; ++b) {
            const double t0 = b * kDt;
            // 6 s at a quiet level, 2 s loud, then quiet again.
            const double amp = (t0 >= 6.0 && t0 < 8.0) ? 0.5 : 0.03;
            for (int k = 0; k < kBlock; ++k) {
                const double t = (b * kBlock + k) / kRate;
                const double v = std::fmod(t, 0.25) < 0.17 ? amp * std::sin(kTwoPi * 1000.0 * t) : 0.0;
                x[2 * k] = x[2 * k + 1] = static_cast<float>(v + 0.002 * std::sin(kTwoPi * 731.0 * t));
            }
            lev.process(x.data(), kBlock, kRate, p);
            gains.push_back(lev.gainDb());
            holds.push_back(lev.riseHoldLeftSec());
            if (lev.riseHoldLeftSec() > previousHold + 1e-9) {
                armed = b;
                QCOMPARE(lev.riseHoldLeftSec(), RxLeveler::kRiseHoldSec);
            }
            previousHold = lev.riseHoldLeftSec();
        }
        QVERIFY2(armed >= 0, "no turn-down armed the hold");
        const double armedAt = (armed + 1) * kDt;     // the end of the arming block
        QVERIFY2(armedAt >= 6.0 && armedAt < 9.0,
                 qPrintable(QStringLiteral("armed at %1 s").arg(armedAt)));
        const size_t atSwitch = static_cast<size_t>(std::lround(8.0 / kDt)) - 1;
        QVERIFY2(gains[atSwitch] < 0.0,
                 qPrintable(QStringLiteral("at the switch %1").arg(gains[atSwitch])));
        // From the arming on, no rise until the hold has run out...
        const int holdBlocks = static_cast<int>(std::lround(RxLeveler::kRiseHoldSec / kDt));
        for (int b = armed + 1; b <= armed + holdBlocks && b < blocks; ++b) {
            QVERIFY2(gains[static_cast<size_t>(b)] <= gains[static_cast<size_t>(b - 1)] + 1e-9,
                     qPrintable(QStringLiteral("rose %1 s after the hold was armed")
                                    .arg((b + 1) * kDt - armedAt)));
        }
        // ... which is kRiseHoldSec after it was armed, to the block.
        int ended = -1;
        for (int b = armed + 1; b < blocks && ended < 0; ++b) {
            if (holds[static_cast<size_t>(b)] <= 0.0) {
                ended = b;
            }
        }
        QVERIFY(ended > 0);
        QVERIFY2(std::abs((ended + 1) * kDt - armedAt - RxLeveler::kRiseHoldSec) <= kDt + 1e-9,
                 qPrintable(QStringLiteral("held %1 s").arg((ended + 1) * kDt - armedAt)));
        // And then it rises for the quieter voice.
        const double gainHeld = gains[static_cast<size_t>(ended)];
        QVERIFY2(gains.back() > gainHeld + 6.0,
                 qPrintable(QStringLiteral("after %1, held %2").arg(gains.back()).arg(gainHeld)));
    }

    // The let-go in a pause (the gain back toward unity with no voice) is
    // not a turn-down for a louder voice: it arms no hold, and the gain
    // rises again as soon as the voice is back. The same with the hiss
    // learned before the station, and with the station there from the
    // first block (the hiss is learned from the gaps between words).
    void aPausesLetGoArmsNoHold_data()
    {
        QTest::addColumn<double>("leadIn");
        QTest::newRow("hiss heard first") << 2.0;
        QTest::newRow("station present from the start") << 0.0;
    }
    void aPausesLetGoArmsNoHold()
    {
        QFETCH(double, leadIn);
        constexpr int kBlock = 480;
        constexpr double kTwoPi = 2.0 * std::numbers::pi;
        constexpr double kDt = kBlock / kRate;
        RxLevelerParams p = autoParams();
        p.stationPresent = true;
        RxLeveler lev;
        std::vector<float> x(kBlock * 2);
        double gainBeforePause = 0.0;
        double gainAtResume = 0.0;
        double gainAfterResume = 0.0;
        double mostHold = 0.0;
        const double pauseAt = leadIn + 6.0;
        const double resumeAt = pauseAt + 3.0;
        const int blocks = static_cast<int>((resumeAt + 3.0) * kRate) / kBlock;
        for (int b = 0; b < blocks; ++b) {
            const double t0 = b * kDt;
            // The lead-in's hiss with nobody there, a quiet voice 6 s, a 3 s
            // pause (the station still there), the voice again.
            p.stationPresent = t0 >= leadIn;
            const bool pause = t0 < leadIn || (t0 >= pauseAt && t0 < resumeAt);
            for (int k = 0; k < kBlock; ++k) {
                const double t = (b * kBlock + k) / kRate;
                const double v = !pause && std::fmod(t, 0.25) < 0.17
                    ? 0.03 * std::sin(kTwoPi * 1000.0 * t) : 0.0;
                x[2 * k] = x[2 * k + 1] = static_cast<float>(v + 0.002 * std::sin(kTwoPi * 731.0 * t));
            }
            lev.process(x.data(), kBlock, kRate, p);
            if (t0 >= pauseAt && t0 < resumeAt + kDt) {
                mostHold = std::max(mostHold, lev.riseHoldLeftSec());
            }
            if (std::abs(t0 - (pauseAt - kDt)) < kDt / 2) {
                gainBeforePause = lev.gainDb();
            } else if (std::abs(t0 - (resumeAt - kDt)) < kDt / 2) {
                gainAtResume = lev.gainDb();
            } else if (std::abs(t0 - (resumeAt + 1.5)) < kDt / 2) {
                gainAfterResume = lev.gainDb();
            }
        }
        QVERIFY2(gainBeforePause > 6.0, qPrintable(QStringLiteral("before the pause %1").arg(gainBeforePause)));
        QVERIFY2(gainAtResume < gainBeforePause - RxLeveler::kTurnDownDb,
                 qPrintable(QStringLiteral("at resume %1, before %2").arg(gainAtResume).arg(gainBeforePause)));
        QCOMPARE(mostHold, 0.0);
        QVERIFY2(gainAfterResume > gainAtResume + 3.0,
                 qPrintable(QStringLiteral("after resume %1, at resume %2")
                                .arg(gainAfterResume).arg(gainAtResume)));
    }

    // A quiet voice lifted, then the slice transmits (resetForTransmit) and
    // unkeys with the station check still saying a station is there (its
    // hold, or the S-meter frozen at key-down): plain hiss is not lifted.
    // The hiss learned before the over is kept, so it is not a voice.
    void hissAfterAnOverIsNotLifted()
    {
        constexpr int kBlock = 480;
        constexpr double kTwoPi = 2.0 * std::numbers::pi;
        constexpr double kDt = kBlock / kRate;
        RxLevelerParams p = autoParams();
        p.stationPresent = true;
        RxLeveler lev;
        std::vector<float> x(kBlock * 2);
        const auto block = [&](int b, bool voice) {
            for (int k = 0; k < kBlock; ++k) {
                const double t = (b * kBlock + k) / kRate;
                const double v = voice && std::fmod(t, 0.25) < 0.17
                    ? 0.03 * std::sin(kTwoPi * 1000.0 * t) : 0.0;
                x[2 * k] = x[2 * k + 1] = static_cast<float>(v + 0.002 * std::sin(kTwoPi * 731.0 * t));
            }
            lev.process(x.data(), kBlock, kRate, p);
        };
        const int voiceBlocks = static_cast<int>(6.0 / kDt);
        for (int b = 0; b < voiceBlocks; ++b) {
            block(b, true);
        }
        QVERIFY2(lev.gainDb() > 6.0, qPrintable(QString::number(lev.gainDb())));
        lev.resetForTransmit();
        QCOMPARE(lev.gainDb(), 0.0);
        double most = 0.0;
        for (int b = 0; b < static_cast<int>(3.0 / kDt); ++b) {
            block(voiceBlocks + b, false);
            most = std::max(most, lev.gainDb());
        }
        QVERIFY2(most < 0.5, qPrintable(QStringLiteral("most lift on the hiss %1 dB").arg(most)));
    }

    // startOver (a retune, a filter or mode change, a new receiver) lets
    // the gain go back to unity at the leveler's own rates, never in one
    // block, and forgets what it learned.
    void startOverLetsTheGainGoWithoutAStep()
    {
        constexpr int kBlock = 480;
        constexpr double kTwoPi = 2.0 * std::numbers::pi;
        constexpr double kDt = kBlock / kRate;
        RxLevelerParams p = autoParams();
        p.stationPresent = true;
        RxLeveler lev;
        std::vector<float> x(kBlock * 2);
        const auto block = [&](int b, bool voice) {
            for (int k = 0; k < kBlock; ++k) {
                const double t = (b * kBlock + k) / kRate;
                const double v = voice && std::fmod(t, 0.25) < 0.17
                    ? 0.03 * std::sin(kTwoPi * 1000.0 * t) : 0.0;
                x[2 * k] = x[2 * k + 1] = static_cast<float>(v + 0.002 * std::sin(kTwoPi * 731.0 * t));
            }
            lev.process(x.data(), kBlock, kRate, p);
        };
        const int voiceBlocks = static_cast<int>(6.0 / kDt);
        for (int b = 0; b < voiceBlocks; ++b) {
            block(b, true);
        }
        const double before = lev.gainDb();
        QVERIFY(before > 6.0);
        lev.startOver();
        QCOMPARE(lev.loudnessDb(), RxLeveler::kSilenceDbfs);
        QCOMPARE(lev.gainDb(), before);
        // The most one block may move toward unity at the release rate.
        double last = before;
        for (int b = 0; b < static_cast<int>(3.0 / kDt); ++b) {
            block(voiceBlocks + b, false);
            const double releaseStep =
                last * (1.0 - std::exp(-kDt * 1000.0 / RxLeveler::kAutoReleaseMs));
            QVERIFY2(last - lev.gainDb() <= releaseStep + 1e-4,
                     qPrintable(QStringLiteral("stepped %1 dB in one block").arg(last - lev.gainDb())));
            QVERIFY(lev.gainDb() <= last + 1e-4);
            last = lev.gainDb();
        }
        QVERIFY2(last < 0.5, qPrintable(QStringLiteral("still %1 dB after 3 s").arg(last)));
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
