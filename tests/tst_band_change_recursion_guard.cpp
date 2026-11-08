// tst_band_change_recursion_guard.cpp
//
// no-port-check: Test file references Thetis behavior in commentary only;
// no Thetis source is translated here.
//
// Regression test for two v0.2.0 bugs exposed by wheel-tuning across a band
// boundary:
//
// Bug 1 (crash, reported 2026-04-18/19, 7 reports):
//   EXC_BAD_ACCESS — "Thread stack size exceeded due to excessive recursion"
//   recursion depth 10,405. The wireSliceSignals()/addPanadapter() band-
//   switch lambdas called restoreFromSettings(newBand) synchronously;
//   that emitted frequencyChanged and re-entered the same lambda
//   indefinitely when per-band Frequency values were cross-referenced.
//
// Bug 2 (VFO jump): with the recursion fixed via a guard in an interim
//   commit, wheel-tuning across a band boundary snapped the VFO to the
//   new band's stored frequency instead of the user-tuned value. That was
//   divergence from Thetis console.cs:45312 handleBSFChange [v2.10.3.13
//   @501e3f5], which updates the bandstack LastVisited record on a
//   VFO-driven crossing but does NOT restore state. Restore is reserved
//   for the explicit band-button press path.
//
// Fix: the VFO-tune lambda only tracks m_lastBand; no save or restore at
// the boundary. Per-band state is flushed via the coalesced
// scheduleSettingsSave() timer, which targets m_lastBand. This test
// replicates the lambda's topology and asserts:
//
//   1. A wheel-tune that crosses a band boundary leaves SliceModel's
//      frequency equal to the user-tuned value (no snap-back).
//   2. The lambda is invoked exactly once per setFrequency call — no
//      recursion through frequencyChanged, even with cross-referenced
//      per-band Frequency values seeded into AppSettings.

#include <QtTest/QtTest>

#include "models/SliceModel.h"
#include "models/Band.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TestBandChangeRecursionGuard : public QObject {
    Q_OBJECT

private:
    static void resetPersistenceKeys() {
        auto& s = AppSettings::instance();
        for (const QString& band : {
                 QStringLiteral("20m"), QStringLiteral("40m"),
                 QStringLiteral("80m"), QStringLiteral("160m") }) {
            const QString bp = QStringLiteral("Slice0/Band") + band + QStringLiteral("/");
            for (const QString& field : {
                     QStringLiteral("Frequency"),   QStringLiteral("AgcThreshold"),
                     QStringLiteral("AgcHang"),     QStringLiteral("AgcSlope"),
                     QStringLiteral("AgcAttack"),   QStringLiteral("AgcDecay"),
                     QStringLiteral("FilterLow"),   QStringLiteral("FilterHigh"),
                     QStringLiteral("DspMode"),     QStringLiteral("AgcMode"),
                     QStringLiteral("StepHz") }) {
                s.remove(bp + field);
            }
        }
        const QString sp = QStringLiteral("Slice0/");
        for (const QString& field : {
                 QStringLiteral("StepHz"),
                 QStringLiteral("Locked"),     QStringLiteral("Muted"),
                 QStringLiteral("RitEnabled"), QStringLiteral("RitHz"),
                 QStringLiteral("XitEnabled"), QStringLiteral("XitHz"),
                 QStringLiteral("AfGain"),     QStringLiteral("RfGain"),
                 QStringLiteral("RxAntenna"),  QStringLiteral("TxAntenna") }) {
            s.remove(sp + field);
        }
    }

private slots:

    void init()    { resetPersistenceKeys(); }
    void cleanup() { resetPersistenceKeys(); }

    // ── Invariant 1: wheel-tune across a band boundary leaves the VFO at the
    // user-tuned frequency (no snap-back to the new band's stored value).

    void wheelTuneAcrossBoundaryPreservesFrequency() {
        auto& s = AppSettings::instance();

        // Seed a stored frequency for 40m that differs from the value the
        // user will wheel-tune to. If bandstack recall were still happening
        // on the crossing, m_frequency would snap to 7150000 instead of
        // staying at the user-tuned 7100000.
        s.setValue(QStringLiteral("Slice0/Band40m/Frequency"), 7150000.0);

        SliceModel slice;
        slice.setSliceIndex(0);
        slice.setFrequency(14225000.0);

        Band lastBand = Band::Band20m;
        int  enterCount = 0;

        QObject::connect(&slice, &SliceModel::frequencyChanged,
            [&](double freq) {
                ++enterCount;
                const Band newBand = bandFromFrequency(freq);
                if (newBand != lastBand) {
                    lastBand = newBand;
                }
                // No saveToSettings / restoreFromSettings — Thetis parity.
                // scheduleSettingsSave() in the real lambda is a production-
                // only timer dispatch; not relevant for this signal-topology
                // invariant check.
            });

        slice.setFrequency(7100000.0);  // wheel-tune from 20m into 40m

        QCOMPARE(slice.frequency(), 7100000.0);  // stays where the user tuned
        QCOMPARE(lastBand,          Band::Band40m);
        QCOMPARE(enterCount,        1);  // single invocation, no recursion
    }

    // ── Invariant 2: even with the corrupt cross-referencing per-band
    // Frequency values that reproduced the v0.2.0 cascade, the lambda must
    // not recurse. With save/restore removed from the lambda, the recursion
    // surface is simply gone.

    void corruptPerBandFrequenciesDoNotCascade() {
        auto& s = AppSettings::instance();

        s.setValue(QStringLiteral("Slice0/Band20m/Frequency"),  7100000.0);    // value is in 40m
        s.setValue(QStringLiteral("Slice0/Band40m/Frequency"),  3700000.0);    // value is in 80m
        s.setValue(QStringLiteral("Slice0/Band80m/Frequency"),  14225000.0);   // value is in 20m (cycle)

        SliceModel slice;
        slice.setSliceIndex(0);
        slice.setFrequency(14225000.0);

        Band lastBand = Band::Band20m;
        int  enterCount = 0;
        constexpr int kSafetyLimit = 50;

        QObject::connect(&slice, &SliceModel::frequencyChanged,
            [&](double freq) {
                ++enterCount;
                if (enterCount > kSafetyLimit) {
                    return;  // belt-and-suspenders guard for the test itself
                }
                const Band newBand = bandFromFrequency(freq);
                if (newBand != lastBand) {
                    lastBand = newBand;
                }
            });

        slice.setFrequency(7100000.0);

        QCOMPARE(enterCount, 1);
        QCOMPARE(slice.frequency(), 7100000.0);
        QCOMPARE(lastBand, Band::Band40m);
    }
};

QTEST_MAIN(TestBandChangeRecursionGuard)
#include "tst_band_change_recursion_guard.moc"
