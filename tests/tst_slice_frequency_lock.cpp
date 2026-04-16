// tst_slice_frequency_lock.cpp
//
// Verifies SliceModel frequency lock behavior (S2.9).
//
// When locked=true, setFrequency() is a no-op:
//   - frequency value does not change
//   - frequencyChanged signal is not emitted
// When locked=false, setFrequency() works normally.
//
// Client-side guard only — does not communicate with hardware.

#include <QtTest/QtTest>
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceFrequencyLock : public QObject {
    Q_OBJECT

private slots:
    // ── Normal operation (unlocked) ───────────────────────────────────────────

    void setFrequencyWorksWhenUnlocked() {
        SliceModel s;
        s.setLocked(false);
        s.setFrequency(7100000.0);
        QCOMPARE(s.frequency(), 7100000.0);
    }

    void setFrequencyEmitsSignalWhenUnlocked() {
        SliceModel s;
        s.setLocked(false);
        QSignalSpy spy(&s, &SliceModel::frequencyChanged);
        // Use a frequency that differs from the default (14225000.0)
        s.setFrequency(7100000.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toDouble(), 7100000.0);
    }

    // ── Lock guard ────────────────────────────────────────────────────────────

    void setFrequencyIsNoOpWhenLocked() {
        SliceModel s;
        s.setFrequency(14000000.0);
        s.setLocked(true);
        s.setFrequency(21000000.0);
        // Value must not change
        QCOMPARE(s.frequency(), 14000000.0);
    }

    void setFrequencyDoesNotEmitSignalWhenLocked() {
        SliceModel s;
        s.setFrequency(14000000.0);
        s.setLocked(true);
        QSignalSpy spy(&s, &SliceModel::frequencyChanged);
        s.setFrequency(21000000.0);
        // Signal must not fire
        QCOMPARE(spy.count(), 0);
    }

    void setFrequencyResumesAfterUnlock() {
        SliceModel s;
        s.setFrequency(14000000.0);
        s.setLocked(true);
        s.setFrequency(21000000.0); // blocked
        s.setLocked(false);
        s.setFrequency(21000000.0); // now allowed
        QCOMPARE(s.frequency(), 21000000.0);
    }

    void setFrequencyEmitsSignalAfterUnlock() {
        SliceModel s;
        s.setFrequency(7000000.0);
        s.setLocked(true);
        QSignalSpy spy(&s, &SliceModel::frequencyChanged);
        s.setFrequency(14000000.0); // blocked
        QCOMPARE(spy.count(), 0);
        s.setLocked(false);
        s.setFrequency(14000000.0); // allowed
        QCOMPARE(spy.count(), 1);
    }

    // ── Lock toggle signal guards ─────────────────────────────────────────────

    void setLockedEmitsLockedChanged() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::lockedChanged);
        s.setLocked(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void setLockedNoSignalOnSameValue() {
        SliceModel s;
        s.setLocked(true);
        QSignalSpy spy(&s, &SliceModel::lockedChanged);
        s.setLocked(true); // same value — no signal
        QCOMPARE(spy.count(), 0);
    }

    // ── Default state ─────────────────────────────────────────────────────────

    void defaultLockedIsFalse() {
        SliceModel s;
        QVERIFY(!s.locked());
    }

    void setFrequencyWorksFromDefaultState() {
        SliceModel s;
        // Default locked=false, so setFrequency must work immediately
        s.setFrequency(3700000.0);
        QCOMPARE(s.frequency(), 3700000.0);
    }
};

QTEST_MAIN(TestSliceFrequencyLock)
#include "tst_slice_frequency_lock.moc"
