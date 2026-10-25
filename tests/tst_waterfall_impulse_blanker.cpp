// =================================================================
// tests/tst_waterfall_impulse_blanker.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test.
//
// WaterfallImpulseBlanker, NB Blank's decision, on rows of 800 bins with a
// floor of -120 dB and +/-2 dB of noise, threshold 3 dB unless stated:
//   - a fence charger's pulse (every bin +10 dB) is blanked, alone and as a
//     train, and no row around it is;
//   - a loud station on 5 % of the bins (+40 dB) and a wide one on 25 %
//     (+30 dB) blank nothing, however long they last;
//   - a lasting rise of the floor is blanked for two rows, then accepted
//     while the baseline follows, then judged against the new floor;
//   - nothing is blanked before 8 rows of history;
//   - a row with no finite bin is not blanked and leaves the baseline as
//     it was, and a pulse with most bins not finite is judged on the rest;
//   - the threshold is the edge in dB at 6 dB.
// Modification history (NereusSDR):
//   2026-10-22  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (Claude Opus 5.5): new. Fix wave: the partial
//                 non-finite case is a pulse with -inf in 500 of 800 bins.
// =================================================================

#include <QtTest/QtTest>

#include <cmath>
#include <limits>
#include <random>
#include <vector>

#include "gui/WaterfallImpulseBlanker.h"

using namespace NereusSDR;
using Verdict = WaterfallImpulseBlanker::Verdict;

namespace {

constexpr int   kBins    = 800;
constexpr float kFloorDb = -120.0f;
constexpr float kNoiseDb = 2.0f;

// One row: the floor with uniform noise, every bin `riseDb` up, and bins
// [signalFirst, signalFirst + signalBins) `signalDb` over the floor.
class Rows {
public:
    std::vector<float> row(float riseDb = 0.0f, int signalFirst = 0, int signalBins = 0,
                           float signalDb = 0.0f)
    {
        std::vector<float> r(kBins);
        for (int i = 0; i < kBins; ++i) {
            r[std::size_t(i)] = kFloorDb + riseDb + m_noise(m_rng);
            if (i >= signalFirst && i < signalFirst + signalBins) {
                r[std::size_t(i)] += signalDb;
            }
        }
        return r;
    }

private:
    std::mt19937 m_rng{20261022u};
    std::uniform_real_distribution<float> m_noise{-kNoiseDb, kNoiseDb};
};

Verdict judge(WaterfallImpulseBlanker& b, const std::vector<float>& row, float thresholdDb = 3.0f)
{
    return b.judge(row.data(), row.size(), thresholdDb).verdict;
}

const char* name(Verdict v)
{
    switch (v) {
    case Verdict::Accepted:         return "Accepted";
    case Verdict::Blanked:          return "Blanked";
    case Verdict::AcceptedOverHold: return "AcceptedOverHold";
    case Verdict::NoFiniteBins:     return "NoFiniteBins";
    }
    return "?";
}

// True when `actual` is `expected`; otherwise says which row and why.
bool expect(Verdict actual, Verdict expected, int row)
{
    if (actual == expected) {
        return true;
    }
    qWarning("row %d: %s, expected %s", row, name(actual), name(expected));
    return false;
}

} // namespace

class TestWaterfallImpulseBlanker : public QObject {
    Q_OBJECT

private slots:
    // ── A fence charger's click: one row, every bin up ──────────────────
    void fencePulse_isBlanked_nextRowIsNot()
    {
        WaterfallImpulseBlanker b;
        Rows rows;
        for (int i = 0; i < 32; ++i) {
            QVERIFY(expect(judge(b, rows.row()), Verdict::Accepted, i));
        }
        QVERIFY(expect(judge(b, rows.row(10.0f)), Verdict::Blanked, 32));
        QVERIFY(expect(judge(b, rows.row()), Verdict::Accepted, 33));
    }

    // ── About once a second: a pulse every 20 rows ──────────────────────
    void fenceTrain_everyPulseBlanked_noOtherRow()
    {
        WaterfallImpulseBlanker b;
        Rows rows;
        int pulses = 0;
        for (int i = 0; i < 400; ++i) {
            const bool pulse = (i % 20) == 19;
            const Verdict v = judge(b, rows.row(pulse ? 10.0f : 0.0f));
            QVERIFY(expect(v, pulse ? Verdict::Blanked : Verdict::Accepted, i));
            pulses += pulse ? 1 : 0;
        }
        QCOMPARE(pulses, 20);
    }

    // ── A loud SSB station on 5 % of the bins (the 75 m freeze) ─────────
    void loudTalker_onFivePercent_blanksNothing()
    {
        WaterfallImpulseBlanker b;
        Rows rows;
        for (int i = 0; i < 32; ++i) {
            judge(b, rows.row());
        }
        for (int i = 0; i < 200; ++i) {
            QVERIFY(expect(judge(b, rows.row(0.0f, 380, 40, 40.0f)), Verdict::Accepted, 32 + i));
        }
    }

    // ── A wide loud signal on 25 % of the bins ──────────────────────────
    void wideLoudSignal_onAQuarter_blanksNothing()
    {
        WaterfallImpulseBlanker b;
        Rows rows;
        for (int i = 0; i < 32; ++i) {
            judge(b, rows.row());
        }
        for (int i = 0; i < 200; ++i) {
            QVERIFY(expect(judge(b, rows.row(0.0f, 300, 200, 30.0f)), Verdict::Accepted, 32 + i));
        }
    }

    // ── The floor rises 10 dB and stays (a band opening, preamp on) ─────
    //
    // Rows 40 and 41 are blanked (the hold limit is two). Row 42 is
    // accepted over the hold, and so is every burst row after it while the
    // baseline follows: the ring (32 accepted rows) has a median at the new
    // floor once 17 of them are risen rows, which rows 42 to 58 are. Row 59
    // is judged against the new floor and is no burst; nothing after it is
    // blanked.
    void sustainedRise_twoRowsBlanked_thenFollowed()
    {
        WaterfallImpulseBlanker b;
        Rows rows;
        for (int i = 0; i < 40; ++i) {
            QVERIFY(expect(judge(b, rows.row()), Verdict::Accepted, i));
        }
        int blanked = 0;
        for (int i = 40; i < 240; ++i) {
            const Verdict v = judge(b, rows.row(10.0f));
            const Verdict expected = i < 42 ? Verdict::Blanked
                                   : i <= 58 ? Verdict::AcceptedOverHold
                                   : Verdict::Accepted;
            QVERIFY(expect(v, expected, i));
            blanked += v == Verdict::Blanked ? 1 : 0;
        }
        QCOMPARE(blanked, 2);
    }

    // ── At most two rows in a row; a row that is no burst ends the run ──
    void holdLimit_isTwoRows_andARunEnds()
    {
        WaterfallImpulseBlanker b;
        Rows rows;
        for (int i = 0; i < 32; ++i) {
            judge(b, rows.row());
        }
        QCOMPARE(WaterfallImpulseBlanker::kMaxConsecutiveRows, 2);
        QVERIFY(expect(judge(b, rows.row(10.0f)), Verdict::Blanked, 0));
        QVERIFY(expect(judge(b, rows.row(10.0f)), Verdict::Blanked, 1));
        QCOMPARE(b.consecutiveBurstRows(), 2);
        QVERIFY(expect(judge(b, rows.row()), Verdict::Accepted, 2));
        QCOMPARE(b.consecutiveBurstRows(), 0);
        // A fresh run: two more blanked, the third accepted.
        QVERIFY(expect(judge(b, rows.row(10.0f)), Verdict::Blanked, 3));
        QVERIFY(expect(judge(b, rows.row(10.0f)), Verdict::Blanked, 4));
        QVERIFY(expect(judge(b, rows.row(10.0f)), Verdict::AcceptedOverHold, 5));
        // Reset forgets the run and the history.
        b.reset();
        QCOMPARE(b.consecutiveBurstRows(), 0);
        QCOMPARE(b.historyRows(), 0);
    }

    // ── Nothing is a burst before 8 rows of history ─────────────────────
    void minHistory_noBurstBeforeEightRows()
    {
        QCOMPARE(WaterfallImpulseBlanker::kMinRows, 8);
        for (int history = 0; history < 8; ++history) {
            WaterfallImpulseBlanker b;
            Rows rows;
            for (int i = 0; i < history; ++i) {
                judge(b, rows.row());
            }
            QVERIFY(expect(judge(b, rows.row(10.0f)), Verdict::Accepted, history));
        }
        WaterfallImpulseBlanker b;
        Rows rows;
        for (int i = 0; i < 8; ++i) {
            judge(b, rows.row());
        }
        QVERIFY(expect(judge(b, rows.row(10.0f)), Verdict::Blanked, 8));
    }

    // ── A row with no finite bin ────────────────────────────────────────
    void nonFiniteRow_notBlanked_baselineUnharmed()
    {
        WaterfallImpulseBlanker b;
        Rows rows;
        for (int i = 0; i < 20; ++i) {
            judge(b, rows.row());
        }
        const std::vector<float> nan(kBins, std::numeric_limits<float>::quiet_NaN());
        const std::vector<float> negInf(kBins, -std::numeric_limits<float>::infinity());
        QVERIFY(expect(judge(b, nan), Verdict::NoFiniteBins, 20));
        QVERIFY(expect(judge(b, negInf), Verdict::NoFiniteBins, 21));
        QCOMPARE(b.historyRows(), 20);
        QVERIFY(expect(judge(b, rows.row()), Verdict::Accepted, 22));
        // Still judged against the floor: a pulse is blanked.
        QVERIFY(expect(judge(b, rows.row(10.0f)), Verdict::Blanked, 23));
        QVERIFY(expect(judge(b, rows.row()), Verdict::Accepted, 24));

        // An empty row counts as no finite bin.
        QCOMPARE(b.judge(nullptr, 0, 3.0f).verdict, Verdict::NoFiniteBins);
    }

    // ── A pulse with most bins not finite is judged on the rest ─────────
    //
    // A +10 dB pulse with -inf in 500 of its 800 bins is still blanked.
    // Were the -inf bins counted, the row's median would be -inf and the
    // pulse would be drawn. Only -inf here, never NaN, so the filter is
    // what is under test, not how a selection orders NaN.
    void mostlyNonFinitePulse_judgedOnTheRest()
    {
        WaterfallImpulseBlanker b;
        Rows rows;
        for (int i = 0; i < 32; ++i) {
            judge(b, rows.row());
        }
        std::vector<float> mostlyEmptyPulse = rows.row(10.0f);
        int emptied = 0;
        for (int i = 0; i < kBins; ++i) {
            if (i % 8 < 5) {
                mostlyEmptyPulse[std::size_t(i)] = -std::numeric_limits<float>::infinity();
                ++emptied;
            }
        }
        QCOMPARE(emptied, 500);
        QVERIFY(expect(judge(b, mostlyEmptyPulse), Verdict::Blanked, 32));
        QVERIFY(expect(judge(b, rows.row()), Verdict::Accepted, 33));
    }

    // ── The threshold is the edge in dB ─────────────────────────────────
    void thresholdSix_fiveUnder_sevenOver()
    {
        WaterfallImpulseBlanker b;
        Rows rows;
        for (int i = 0; i < 32; ++i) {
            judge(b, rows.row(), 6.0f);
        }
        QVERIFY(expect(judge(b, rows.row(5.0f), 6.0f), Verdict::Accepted, 32));
        for (int i = 0; i < 32; ++i) {
            judge(b, rows.row(), 6.0f);
        }
        QVERIFY(expect(judge(b, rows.row(7.0f), 6.0f), Verdict::Blanked, 65));
    }
};

QTEST_GUILESS_MAIN(TestWaterfallImpulseBlanker)
#include "tst_waterfall_impulse_blanker.moc"
