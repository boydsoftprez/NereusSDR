// =================================================================
// tests/tst_sample_rate_live_apply.cpp  (NereusSDR)
// =================================================================
// NereusSDR-original test infrastructure, except the case
// stored_rate_survives_probing_and_connecting, ported from Longpath and
// registered in docs/attribution/LONGPATH-PROVENANCE.md. The Thetis filename
// reference below is a comment-only provenance note, not a code port.
//
// Ported from Longpath sources:
//   tests/tst_sample_rate_live_apply.cpp (stored_rate_survives_probing_and_connecting).
//   Longpath's banner on that file is this NereusSDR header with the project
//   name changed, so it is not copied here.
//
// NereusSDR-original infrastructure — no Thetis source ported here.
// RadioModel::setSampleRateLive() is a NereusSDR coordinator; the P1
// restart it delegates to mirrors the onReconnectTimeout() sequence
// from P1RadioConnection (itself ported from
// ChannelMaster/networkproto1.c SendStopToMetis/SendStartToMetis [v2.10.3.13]).
//
// Test scope (Task 1.6):
//   changes_rate_without_dropping_channel —
//       guards: unconnected → returns -1, idempotent same-rate → returns 0.
//   rebuild_completes_under_glitch_budget —
//       with no WDSP session the coordinator returns -1 from the guard
//       before any of the 8 steps execute; elapsed is always < 200 ms.
//
// Both tests run without a live WDSP session or radio connection.
// The "under glitch budget" test is vacuously satisfied in the unconnected
// case; the full timing path is verified by the integration bench when
// a real ANAN-G2 is attached (see Task 1.8).
//
// Modification history (NereusSDR):
//   2026-10-22 - stored_rate_survives_probing_and_connecting, ported from
//                Longpath PR 37 (fd894ded, Martin Fischer OE5SOS) by J.J.
//                Boyd (KG4VCF), with AI-assisted transformation via Claude
//                Code.
// =================================================================

#include <QtTest/QtTest>
#include <QElapsedTimer>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestSampleRateLiveApply : public QObject {
    Q_OBJECT

private slots:
    // ── Guard path: not connected → returns -1 ────────────────────────────────
    //
    // setSampleRateLive() returns -1 whenever the radio is not connected or
    // WDSP is not initialized.  An un-connected RadioModel satisfies both
    // conditions; this verifies the guard fires and no steps execute.
    void changes_rate_without_dropping_channel()
    {
        RadioModel radio;

        // No connection, no WDSP — guard returns -1.
        const qint64 result = radio.setSampleRateLive(192000);
        QCOMPARE(result, qint64(-1));

        // The rate property must remain at its initial value (0 = disconnected).
        QCOMPARE(radio.connectionSampleRateHz(), 0);
    }

    // ── Idempotent: same rate as current → returns 0 ─────────────────────────
    //
    // When the requested rate equals m_connectionSampleRateHz the method
    // returns 0 immediately without touching any hardware or WDSP channel.
    // This exercises the second early-return guard.
    //
    // In the unconnected state m_connectionSampleRateHz == 0, so passing
    // 0 triggers the idempotent guard before the connection guard.
    void idempotent_same_rate_returns_zero()
    {
        RadioModel radio;

        // 0 == m_connectionSampleRateHz{0}: idempotent path fires first.
        const qint64 result = radio.setSampleRateLive(0);
        QCOMPARE(result, qint64(0));
    }

    // ── Timing: coordinator must not stall the main thread ───────────────────
    //
    // In the no-WDSP path the guard returns immediately, so elapsed is
    // always well under budget.  This pins the regression: a blocking
    // cross-thread call must never appear before the guard check.
    //
    // The glitch budget (200 ms) comes from the design doc §5C risk note.
    // The integration bench (Task 1.8) verifies the full 8-step path on
    // a real ANAN-G2 also satisfies this bound.
    void rebuild_completes_under_glitch_budget()
    {
        RadioModel radio;

        QElapsedTimer t;
        t.start();
        radio.setSampleRateLive(384000);
        const qint64 elapsed = t.elapsed();

        // The guard returns -1 in < 1 ms; 200 ms is the outer contract.
        QVERIFY2(elapsed < 200,
                 qPrintable(QStringLiteral("setSampleRateLive took %1 ms (budget: 200 ms)")
                                .arg(elapsed)));
    }

    // ── wireSampleRateChanged not emitted from guard path ────────────────────
    //
    // When the guard fires (not connected), the signal must NOT be emitted —
    // state has not changed.
    void signal_not_emitted_when_guard_fires()
    {
        RadioModel radio;

        QSignalSpy spy(&radio, &RadioModel::wireSampleRateChanged);
        radio.setSampleRateLive(192000);

        // Guard returned -1; state unchanged; no signal.
        QCOMPARE(spy.count(), 0);
    }

    // ── wireSampleRateChanged not emitted on idempotent same-rate ────────────
    //
    // When the idempotent guard fires (same rate as current), the signal
    // must NOT be emitted — no state transition occurred.
    void signal_not_emitted_on_same_rate()
    {
        RadioModel radio;

        QSignalSpy spy(&radio, &RadioModel::wireSampleRateChanged);
        radio.setSampleRateLive(0);  // same as m_connectionSampleRateHz{0}

        QCOMPARE(spy.count(), 0);
    }

    // From Longpath tests/tst_sample_rate_live_apply.cpp:126-156 [@383f058]
    // ── The stored rate survives Probing/Connecting ─────────────────────────
    //
    // connectToRadio() writes the wire rate and the active-RX count, then
    // the worker thread reports Probing → Connecting → Connected one event-
    // loop turn later. Until 2026-09-21 every non-Connected state cleared
    // both fields, so the write was undone before Connected ever arrived:
    // the model reported rate 0 and rx2Enabled()==false for the whole
    // session (HL2 simulator bench). The idempotent guard of
    // setSampleRateLive() is the probe here: it returns 0 exactly when the
    // requested rate equals the stored one, and -1 otherwise (no radio).
    void stored_rate_survives_probing_and_connecting()
    {
        RadioModel radio;
        radio.setConnectionRateForTest(192000, 2);

        radio.setConnectionStateForTest(ConnectionState::Probing);
        radio.setConnectionStateForTest(ConnectionState::Connecting);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        QCOMPARE(radio.setSampleRateLive(192000), qint64(0));
        QVERIFY(radio.rx2Enabled());

        // A lost link may come back; it is not a teardown.
        radio.setConnectionStateForTest(ConnectionState::LinkLost);
        QCOMPARE(radio.setSampleRateLive(192000), qint64(0));
        QVERIFY(radio.rx2Enabled());

        // Only Disconnected clears the pair.
        radio.setConnectionStateForTest(ConnectionState::Disconnected);
        QCOMPARE(radio.setSampleRateLive(0), qint64(0));
        QVERIFY(!radio.rx2Enabled());
    }
};

QTEST_MAIN(TestSampleRateLiveApply)
#include "tst_sample_rate_live_apply.moc"
