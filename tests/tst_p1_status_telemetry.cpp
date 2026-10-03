// no-port-check: test harness citing Thetis networkproto1.c offsets as the
// behavioural spec it asserts against; contains no ported logic of its own.
// =================================================================
// tests/tst_p1_status_telemetry.cpp  (NereusSDR)
// =================================================================
//
// Phase 3P-H Task 4: verifies P1RadioConnection::parseEp6Frame() extracts
// PA telemetry (fwd / rev / exciter / user_adc0 / user_adc1 / supply_volts)
// from C&C status bytes per Thetis networkproto1.c:332-356 [@501e3f5].
//
// Test vectors are hand-derived from the Thetis switch statement on
// (ControlBytesIn[0] & 0xf8):
//   case 0x08 — exciter (C1-C2) + fwd (C3-C4)
//   case 0x10 — rev     (C1-C2) + user_adc0 (C3-C4)
//   case 0x18 — user_adc1 (C1-C2) + supply_volts (C3-C4)
//
// The test feeds a 1032-byte ep6 datagram with two subframes whose C0
// byte selects two of the three telemetry cases and asserts the emitted
// paTelemetryUpdated() signal carries the expected uint16 values.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-21 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted authoring via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QByteArray>

#include "core/P1RadioConnection.h"

using namespace NereusSDR;

namespace {

class DiagnosticsP1Connection : public P1RadioConnection {
public:
    void markConnected(quint16 port)
    {
        m_radioInfo.port = port;
        setState(ConnectionState::Connected);
    }
    void retire() { setState(ConnectionState::LinkLost); }
};

// Build a minimal valid 1032-byte ep6 datagram with both subframes' sync
// + the caller-supplied 5-byte C&C (C0..C4) for each subframe.  Sample
// data is left zero — the parser will still return successfully because
// the magic + sync words are present.
QByteArray makeEp6Frame(quint8 sub0[5], quint8 sub1[5])
{
    QByteArray pkt(1032, '\0');
    auto* p = reinterpret_cast<quint8*>(pkt.data());

    // Metis ep6 magic — networkproto1.c:326-327
    p[0] = 0xEF; p[1] = 0xFE; p[2] = 0x01; p[3] = 0x06;

    // Subframe 0 sync at [8..10] + C&C at [11..15]
    p[8] = 0x7F; p[9] = 0x7F; p[10] = 0x7F;
    for (int i = 0; i < 5; ++i) { p[11 + i] = sub0[i]; }

    // Subframe 1 sync at [520..522] + C&C at [523..527]
    p[520] = 0x7F; p[521] = 0x7F; p[522] = 0x7F;
    for (int i = 0; i < 5; ++i) { p[523 + i] = sub1[i]; }

    return pkt;
}

} // anonymous namespace

class TestP1StatusTelemetry : public QObject {
    Q_OBJECT
private slots:

    void parsedAdcStatusRidesExistingTelemetryReply()
    {
        DiagnosticsP1Connection conn;
        conn.init();
        conn.markConnected(41024);
        QSignalSpy replies(&conn, &RadioConnection::telemetryObservationReady);
        conn.collectTelemetryObservation(6);
        QCOMPARE(replies.count(), 1);
        QCOMPARE(replies.at(0).size(), 7); // one bounded owner-thread diagnostics value
        RadioDiagnosticsObservation before =
            qvariant_cast<RadioDiagnosticsObservation>(replies.takeFirst().at(6));
        QCOMPARE(before.radioUdpBasePort, std::optional<quint16>(41024));
        QVERIFY(!before.adcOverloads[0].known);

        quint8 positive[5] = {0x00, 0x01, 0, 0, 0};
        quint8 unrelated[5] = {0x08, 0, 0, 0, 0};
        QSignalSpy positiveSignals(&conn, &RadioConnection::adcOverflow);
        conn.parseEp6FrameForTest(makeEp6Frame(positive, unrelated));
        conn.collectTelemetryObservation(7);
        QCOMPARE(replies.count(), 1);
        const RadioDiagnosticsObservation set =
            qvariant_cast<RadioDiagnosticsObservation>(replies.takeFirst().at(6));
        QCOMPARE(set.radioUdpBasePort, std::optional<quint16>(41024));
        QVERIFY(set.adcOverloads[0].known);
        QVERIFY(set.adcOverloads[0].active);
        QCOMPARE(set.adcOverloads[0].eventsSinceConnection, 1);
        QVERIFY(set.adcOverloads[0].lastOverloadAgeMs);
        QVERIFY(!set.adcOverloads[1].known);
        QTest::qWait(15);
        conn.collectTelemetryObservation(71);
        const RadioDiagnosticsObservation aged =
            qvariant_cast<RadioDiagnosticsObservation>(replies.takeFirst().at(6));
        QVERIFY(*aged.adcOverloads[0].lastOverloadAgeMs >= 10);
        conn.parseEp6FrameForTest(makeEp6Frame(positive, unrelated));
        conn.collectTelemetryObservation(72);
        const RadioDiagnosticsObservation refreshed =
            qvariant_cast<RadioDiagnosticsObservation>(replies.takeFirst().at(6));
        QCOMPARE(refreshed.adcOverloads[0].eventsSinceConnection, 1);
        QVERIFY(*refreshed.adcOverloads[0].lastOverloadAgeMs
                < *aged.adcOverloads[0].lastOverloadAgeMs);
        QCOMPARE(positiveSignals.count(), 2); // existing positive signal remains

        quint8 multi[5] = {0x20, 0, 1, 1, 0};
        conn.parseEp6FrameForTest(makeEp6Frame(multi, unrelated));
        conn.parseEp6FrameForTest(makeEp6Frame(positive, unrelated));
        conn.collectTelemetryObservation(8);
        const RadioDiagnosticsObservation mixed =
            qvariant_cast<RadioDiagnosticsObservation>(replies.takeFirst().at(6));
        QCOMPARE(mixed.adcOverloads[0].eventsSinceConnection, 2); // clear -> set
        QCOMPARE(mixed.adcOverloads[1].eventsSinceConnection, 1);
        QCOMPARE(mixed.adcOverloads[2].eventsSinceConnection, 1);
        QVERIFY(mixed.adcOverloads[1].active); // 0x00 did not clear ADC1

        quint8 clear[5] = {0x00, 0, 0, 0, 0};
        conn.parseEp6FrameForTest(makeEp6Frame(clear, unrelated));
        conn.collectTelemetryObservation(9);
        const RadioDiagnosticsObservation cleared =
            qvariant_cast<RadioDiagnosticsObservation>(replies.takeFirst().at(6));
        QVERIFY(!cleared.adcOverloads[0].active);
        QCOMPARE(cleared.adcOverloads[0].eventsSinceConnection, 2);
        QVERIFY(cleared.adcOverloads[0].lastOverloadAgeMs);
        QVERIFY(cleared.adcOverloads[1].active);

        conn.retire();
        conn.markConnected(41025); // same QObject, new connection epoch
        conn.collectTelemetryObservation(10);
        const RadioDiagnosticsObservation reconnected =
            qvariant_cast<RadioDiagnosticsObservation>(replies.takeFirst().at(6));
        QCOMPARE(reconnected.radioUdpBasePort, std::optional<quint16>(41025));
        QVERIFY(!reconnected.adcOverloads[0].known);
        QVERIFY(!reconnected.adcOverloads[1].known);
    }

    // ── Case 0x08: exciter + fwd ──────────────────────────────────────────
    void parsesCase08_excitesAndFwd()
    {
        // C0 = 0x08 (case 0x08, MOX bit 0 = 0), C1=0x12 C2=0x34 → exciter=0x1234
        // C3=0xAB C4=0xCD → fwd_power=0xABCD
        quint8 sub0[5] = {0x08, 0x12, 0x34, 0xAB, 0xCD};
        // Subframe 1 carries C0=0x10 → rev=0x5678, user_adc0=0x9ABC
        quint8 sub1[5] = {0x10, 0x56, 0x78, 0x9A, 0xBC};
        QByteArray pkt = makeEp6Frame(sub0, sub1);

        P1RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &P1RadioConnection::paTelemetryUpdated);

        conn.parseEp6FrameForTest(pkt);

        QCOMPARE(spy.count(), 1);
        const auto& args = spy.takeFirst();
        // Signal: (fwdRaw, revRaw, exciterRaw, userAdc0Raw, userAdc1Raw, supplyRaw)
        QCOMPARE(args.at(0).value<quint16>(), quint16(0xABCD));  // fwd
        QCOMPARE(args.at(1).value<quint16>(), quint16(0x5678));  // rev
        QCOMPARE(args.at(2).value<quint16>(), quint16(0x1234));  // exciter
        QCOMPARE(args.at(3).value<quint16>(), quint16(0x9ABC));  // user_adc0
        QCOMPARE(args.at(4).value<quint16>(), quint16(0x0000));  // user_adc1 (not in this frame)
        QCOMPARE(args.at(5).value<quint16>(), quint16(0x0000));  // supply (not in this frame)
    }

    // ── Case 0x18: user_adc1 (PA Amps) + supply_volts ─────────────────────
    void parsesCase18_userAdc1AndSupply()
    {
        // Subframe 0: C0=0x18 → user_adc1=0xDEAD, supply=0xBEEF
        quint8 sub0[5] = {0x18, 0xDE, 0xAD, 0xBE, 0xEF};
        // Subframe 1: another 0x18 with different values; latch carries forward
        quint8 sub1[5] = {0x18, 0x11, 0x22, 0x33, 0x44};
        QByteArray pkt = makeEp6Frame(sub0, sub1);

        P1RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &P1RadioConnection::paTelemetryUpdated);

        conn.parseEp6FrameForTest(pkt);

        QCOMPARE(spy.count(), 1);
        const auto& args = spy.takeFirst();
        // Last-writer-wins: subframe 1 values should be in the emit.
        QCOMPARE(args.at(4).value<quint16>(), quint16(0x1122));  // user_adc1
        QCOMPARE(args.at(5).value<quint16>(), quint16(0x3344));  // supply
        // Other fields untouched (zero)
        QCOMPARE(args.at(0).value<quint16>(), quint16(0x0000));  // fwd
        QCOMPARE(args.at(1).value<quint16>(), quint16(0x0000));  // rev
        QCOMPARE(args.at(2).value<quint16>(), quint16(0x0000));  // exciter
        QCOMPARE(args.at(3).value<quint16>(), quint16(0x0000));  // user_adc0
    }

    // ── No telemetry case (C0=0x00) → no signal emitted ───────────────────
    void noEmit_whenC0IsAdcOverloadCase()
    {
        // C0 = 0x00 is the ADC-overload case (networkproto1.c:334).
        // It carries no PA telemetry, so paTelemetryUpdated() must NOT fire.
        quint8 sub0[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
        quint8 sub1[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
        QByteArray pkt = makeEp6Frame(sub0, sub1);

        P1RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &P1RadioConnection::paTelemetryUpdated);

        conn.parseEp6FrameForTest(pkt);

        QCOMPARE(spy.count(), 0);
    }

    // ── I2C response (C0 bit 7) is NOT treated as telemetry ───────────────
    void i2cResponseFrame_doesNotTriggerTelemetry()
    {
        // C0 bit 7 set → I2C read response (mi0bot networkproto1.c:478-480).
        // The telemetry switch must skip these subframes so the bytes don't
        // bleed into fwd/rev/etc.
        quint8 sub0[5] = {0x80, 0x12, 0x34, 0x56, 0x78};
        quint8 sub1[5] = {0x80, 0x9A, 0xBC, 0xDE, 0xF0};
        QByteArray pkt = makeEp6Frame(sub0, sub1);

        P1RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &P1RadioConnection::paTelemetryUpdated);

        conn.parseEp6FrameForTest(pkt);

        QCOMPARE(spy.count(), 0);
    }
};

QTEST_GUILESS_MAIN(TestP1StatusTelemetry)
#include "tst_p1_status_telemetry.moc"
