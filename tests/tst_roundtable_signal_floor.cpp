// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_roundtable_signal_floor.cpp  (NereusSDR)
// =================================================================
//
// Roundtable plan Task 10 (R-RT-6, wire spec 6.5, design C3): a slice's
// signal-present decision. Present is 10 dB over the slice's own floor
// (the 10th percentile of the last 60 s of readings taken while not
// present), false for the first 10 s after a retune, and the floor holds
// while present, so a carrier held for minutes stays present.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 10).
//   2026-10-24: Task 11 fix round: a spread window's nearest-rank 10th
//               percentile. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#include "core/roundtable/RoundtableSignalFloor.h"

#include <QtTest/QtTest>

using namespace NereusSDR;

namespace {

constexpr qint64 kStartMs = 1000000;
constexpr qint64 kStepMs = 100;

// Feeds `dbm` every 100 ms over [from, to) and returns whether every
// reading gave `expect`.
bool feed(RoundtableSignalFloor& floor, double dbm, qint64 fromMs, qint64 toMs, bool expect)
{
    bool all = true;
    for (qint64 t = fromMs; t < toMs; t += kStepMs) {
        if (floor.update(dbm, t) != expect) {
            all = false;
        }
    }
    return all;
}

} // namespace

class TstRoundtableSignalFloor : public QObject {
    Q_OBJECT

private slots:
    void steadyFloorThenSignalIsPresentOnlyAfterWarmup();
    void carrierHeldFiveMinutesStaysPresent();
    void retuneClearsAndRestartsWarmup();
    void tenDecibelsIsTheEdge();
    void spreadWindowFloorIsTheNearestRankTenthPercentile();
};

void TstRoundtableSignalFloor::steadyFloorThenSignalIsPresentOnlyAfterWarmup()
{
    RoundtableSignalFloor floor;
    floor.retune(kStartMs);
    // During the warm-up neither the floor nor a signal is present.
    QVERIFY(feed(floor, -120.0, kStartMs, kStartMs + 5000, false));
    QVERIFY(!floor.update(-105.0, kStartMs + 5000));
    QVERIFY(feed(floor, -120.0, kStartMs + 5100, kStartMs + RoundtableSignalFloor::kWarmupMs, false));
    // After it, -105 dBm over a -120 dBm floor is present.
    QVERIFY(floor.update(-105.0, kStartMs + RoundtableSignalFloor::kWarmupMs));
    QVERIFY(floor.present());
    double dbm = 0.0;
    QVERIFY(floor.floor(&dbm));
    QCOMPARE(dbm, -120.0);
    // And the floor alone is not.
    QVERIFY(!floor.update(-120.0, kStartMs + RoundtableSignalFloor::kWarmupMs + kStepMs));
}

void TstRoundtableSignalFloor::carrierHeldFiveMinutesStaysPresent()
{
    RoundtableSignalFloor floor;
    floor.retune(kStartMs);
    const qint64 warm = kStartMs + RoundtableSignalFloor::kWarmupMs;
    QVERIFY(feed(floor, -120.0, kStartMs, warm + 20000, false));
    // A -100 dBm carrier for five minutes, far longer than the 60 s window.
    QVERIFY(feed(floor, -100.0, warm + 20000, warm + 20000 + 5 * 60 * 1000, true));
    double dbm = 0.0;
    QVERIFY(floor.floor(&dbm));
    QCOMPARE(dbm, -120.0);
}

void TstRoundtableSignalFloor::retuneClearsAndRestartsWarmup()
{
    RoundtableSignalFloor floor;
    floor.retune(kStartMs);
    const qint64 warm = kStartMs + RoundtableSignalFloor::kWarmupMs;
    QVERIFY(feed(floor, -120.0, kStartMs, warm, false));
    QVERIFY(floor.update(-105.0, warm));

    const qint64 retuneMs = warm + 1000;
    floor.retune(retuneMs);
    QVERIFY(!floor.present());
    QVERIFY(!floor.floor(nullptr));
    // The old floor is gone: the same signal waits out a new warm-up.
    QVERIFY(feed(floor, -105.0, retuneMs, retuneMs + RoundtableSignalFloor::kWarmupMs, false));
    // The new floor is -105 dBm itself, so it is not present over it.
    QVERIFY(!floor.update(-105.0, retuneMs + RoundtableSignalFloor::kWarmupMs));
    QVERIFY(floor.update(-90.0, retuneMs + RoundtableSignalFloor::kWarmupMs + kStepMs));
}

void TstRoundtableSignalFloor::tenDecibelsIsTheEdge()
{
    RoundtableSignalFloor floor;
    floor.retune(kStartMs);
    const qint64 warm = kStartMs + RoundtableSignalFloor::kWarmupMs;
    QVERIFY(feed(floor, -120.0, kStartMs, warm, false));
    QVERIFY(!floor.update(-110.5, warm));
    QVERIFY(floor.update(-110.0, warm + kStepMs));
}

void TstRoundtableSignalFloor::spreadWindowFloorIsTheNearestRankTenthPercentile()
{
    RoundtableSignalFloor floor;
    floor.retune(kStartMs);
    // 100 distinct readings, -130 to -31 dBm, fed out of order inside the
    // warm-up. The nearest-rank 10th percentile is the ceil(0.1 * 100) =
    // 10th smallest, -121 dBm. A min (-130), median (-81) or last-value
    // floor gives something else.
    for (int i = 0; i < 100; ++i) {
        const int step = (i * 37) % 100;
        QVERIFY(!floor.update(-130.0 + step, kStartMs + i * kStepMs));
    }
    double dbm = 0.0;
    QVERIFY(floor.floor(&dbm));
    QCOMPARE(dbm, -121.0);
    // 101 readings: ceil(10.1) = 11th smallest. Add one more at -31 dBm
    // (still in the warm-up), so the 11th smallest is -120 dBm.
    QVERIFY(!floor.update(-31.0, kStartMs + 100 * kStepMs - 1));
    QVERIFY(floor.floor(&dbm));
    QCOMPARE(dbm, -120.0);
}

QTEST_GUILESS_MAIN(TstRoundtableSignalFloor)
#include "tst_roundtable_signal_floor.moc"
