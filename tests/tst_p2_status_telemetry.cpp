// =================================================================
// tests/tst_p2_status_telemetry.cpp  (NereusSDR)
// =================================================================
//
// Phase 3P-H Task 4: verifies P2RadioConnection::processHighPriorityStatus()
// extracts PA telemetry from the High-Priority status packet per Thetis
// network.c:711-748 [@501e3f5]:
//
//   bytes  2- 3 → exciter_power (AIN5)
//   bytes 10-11 → fwd_power     (AIN1)
//   bytes 18-19 → rev_power     (AIN2)
//   bytes 45-46 → supply_volts
//   bytes 51-52 → user_adc1     (AIN4 PA Amps)
//   bytes 53-54 → user_adc0     (AIN3 PA Volts)
//
// NereusSDR keeps the 4-byte sequence prefix in the QByteArray, so the
// effective offset in the buffer is N+4.
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

#include "core/P2RadioConnection.h"

using namespace NereusSDR;

namespace {

// Build a 60-byte High-Priority status packet with the given big-endian
// 16-bit values placed at the Thetis ReadBufp offsets (shifted by +4 to
// account for NereusSDR's leading sequence-number prefix).
QByteArray makeHighPriorityPacket(quint16 exciter, quint16 fwd, quint16 rev,
                                  quint16 supply,
                                  quint16 userAdc1, quint16 userAdc0)
{
    QByteArray pkt(60, '\0');
    auto* p = reinterpret_cast<quint8*>(pkt.data());

    auto put16 = [p](int off, quint16 v) {
        p[off]     = static_cast<quint8>((v >> 8) & 0xFF);
        p[off + 1] = static_cast<quint8>(v & 0xFF);
    };

    // Sequence number — anything non-zero is fine; the parser bumps m_ccSeqNo.
    put16(0, 0); put16(2, 1);

    // ReadBufp[N] → packet offset N+4
    put16(4 + 2,  exciter);   // bytes 2-3
    put16(4 + 10, fwd);       // bytes 10-11
    put16(4 + 18, rev);       // bytes 18-19
    put16(4 + 45, supply);    // bytes 45-46
    put16(4 + 51, userAdc1);  // bytes 51-52
    put16(4 + 53, userAdc0);  // bytes 53-54

    return pkt;
}

} // anonymous namespace

class TestP2StatusTelemetry : public QObject {
    Q_OBJECT
private slots:

    void parsesAllSixFields_fromKnownVector()
    {
        // Hand-derived test vector: each field gets a unique value so the
        // test catches any cross-wiring between fields.
        QByteArray pkt = makeHighPriorityPacket(
            /*exciter*/  0x0102,
            /*fwd*/      0x0304,
            /*rev*/      0x0506,
            /*supply*/   0x0708,
            /*userAdc1*/ 0x090A,
            /*userAdc0*/ 0x0B0C);

        P2RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &P2RadioConnection::paTelemetryUpdated);

        conn.processHighPriorityStatusForTest(pkt);

        QCOMPARE(spy.count(), 1);
        const auto& args = spy.takeFirst();
        // Signal: (fwdRaw, revRaw, exciterRaw, userAdc0Raw, userAdc1Raw, supplyRaw)
        QCOMPARE(args.at(0).value<quint16>(), quint16(0x0304));  // fwd
        QCOMPARE(args.at(1).value<quint16>(), quint16(0x0506));  // rev
        QCOMPARE(args.at(2).value<quint16>(), quint16(0x0102));  // exciter
        QCOMPARE(args.at(3).value<quint16>(), quint16(0x0B0C));  // user_adc0
        QCOMPARE(args.at(4).value<quint16>(), quint16(0x090A));  // user_adc1
        QCOMPARE(args.at(5).value<quint16>(), quint16(0x0708));  // supply
    }

    void parsesZeroFields_emitsAllZero()
    {
        // Idle packet (all telemetry bytes zero) — the parser should still
        // emit the signal with all-zero values; downstream handles the
        // 0 → "no reading" UX.
        QByteArray pkt = makeHighPriorityPacket(0, 0, 0, 0, 0, 0);

        P2RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &P2RadioConnection::paTelemetryUpdated);

        conn.processHighPriorityStatusForTest(pkt);

        QCOMPARE(spy.count(), 1);
        const auto& args = spy.takeFirst();
        for (int i = 0; i < 6; ++i) {
            QCOMPARE(args.at(i).value<quint16>(), quint16(0));
        }
    }

    void parsesMaxValue_preservesFullDynamicRange()
    {
        // Each user_adc field is 16 bits in the buffer (Thetis sign-extends
        // the 12-bit ADC into 16 bits — full 0xFFFF must round-trip).
        QByteArray pkt = makeHighPriorityPacket(
            /*exciter*/  0xFFFF,
            /*fwd*/      0xFFFF,
            /*rev*/      0xFFFF,
            /*supply*/   0xFFFF,
            /*userAdc1*/ 0xFFFF,
            /*userAdc0*/ 0xFFFF);

        P2RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &P2RadioConnection::paTelemetryUpdated);

        conn.processHighPriorityStatusForTest(pkt);

        QCOMPARE(spy.count(), 1);
        const auto& args = spy.takeFirst();
        for (int i = 0; i < 6; ++i) {
            QCOMPARE(args.at(i).value<quint16>(), quint16(0xFFFF));
        }
    }
};

QTEST_GUILESS_MAIN(TestP2StatusTelemetry)
#include "tst_p2_status_telemetry.moc"
