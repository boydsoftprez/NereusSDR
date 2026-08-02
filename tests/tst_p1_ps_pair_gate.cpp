// no-port-check: NereusSDR-original regression test. The upstream file names
// below appear only inside source-cite comments that record where each
// expected DDC index comes from; no upstream logic is ported here.
//
// =================================================================
// tests/tst_p1_ps_pair_gate.cpp  (NereusSDR)
// =================================================================
//
// Regression: the PureSignal paired-IQ emit must stay shut during ordinary RX.
//
// P1RadioConnection::parseEp6Frame gates `emit psPairedIqDataReceived` on
// `m_psFbDdc >= 0 && m_psTxMonDdc >= 0`, latched out of PsDdcConfig by
// applyPsDdcConfig. PsDdcConfig used to default those two fields to 0 and 1,
// and no branch outside the PS-MOX one assigns them, so the gate stood open on
// every board from the first DDC config onward: two QVector copies
// materialised and one signal emitted per EP6 frame, up to roughly 5000 times
// a second at 192 kHz, every one of them dropped by PsccPump's `!m_active`
// guard. Measured on a live HL2 at 201,400 samples/sec with PureSignal
// switched off.
//
// The fix is the sentinel: PsDdcConfig defaults both indices to -1, and each
// codec's PS-MOX branch assigns the real pair explicitly. 0 and 1 remain
// expressible, and remain what the nddc=2 family and every Saturn-class P2
// branch emit.
//
// Expected HL2 pair when PureSignal IS running, from mi0bot
// networkproto1.c:383-387 [v2.10.3.13-beta2] (`twist(spr, 2, 3, 1)`) and
// mi0bot console.cs:8757-8762 [v2.10.3.13-beta2] GetDDC (psrx = 2, pstx = 3).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-01: PS pairing gate regression. J.J. Boyd (KG4VCF),
//               with AI-assisted authoring via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QByteArray>
#include <QVector>

#include "core/HpsdrModel.h"
#include "core/P1RadioConnection.h"
#include "core/RadioConnection.h"
#include "core/codec/CodecContext.h"
#include "core/codec/P1CodecHl2.h"

using namespace NereusSDR;

namespace {

// A 1032-byte EP6 datagram that parseEp6Frame accepts: Metis magic plus both
// subframe sync words. Sample bytes stay zero, which is enough here because
// the parser pushes one I/Q pair per slot per subframe regardless of content,
// so every per-RX vector up to activeRxCount comes back non-empty.
// Layout per Thetis networkproto1.c:320-357 [v2.10.3.13].
QByteArray makeEp6Frame()
{
    QByteArray pkt(1032, '\0');
    auto* p = reinterpret_cast<quint8*>(pkt.data());

    // Metis EP6 magic, networkproto1.c:326-327 [v2.10.3.13]
    p[0] = 0xEF; p[1] = 0xFE; p[2] = 0x01; p[3] = 0x06;
    // Subframe 0 sync at [8..10], subframe 1 sync at [520..522]
    p[8]   = 0x7F; p[9]   = 0x7F; p[10]  = 0x7F;
    p[520] = 0x7F; p[521] = 0x7F; p[522] = 0x7F;

    return pkt;
}

// The HL2 config for a given operator state, straight out of the production
// codec rather than hand-built, so the assertions below pin the real branch.
PsDdcConfig hl2Config(bool psEnabled, bool moxState)
{
    P1CodecHl2 codec;
    return codec.applyPureSignalDdcConfig(
        HPSDRModel::HERMESLITE,
        psEnabled,
        /*diversityEnabled=*/false,
        moxState,
        /*rx1Rate=*/192000,
        /*rx2Rate=*/192000,
        /*rx2Enabled=*/false,
        /*adcCtrl1=*/0,
        /*adcCtrl2=*/0);
}

} // anonymous namespace

class TestP1PsPairGate : public QObject {
    Q_OBJECT
private slots:

    void initTestCase()
    {
        // QSignalSpy has to resolve every parameter type to record a call.
        qRegisterMetaType<QVector<float>>("QVector<float>");
    }

    // ── The headline defect ──────────────────────────────────────────────
    //
    // PureSignal off, receiving. The connection has a DDC config (it always
    // does: ReceiverManager republishes on every rate, RX2 and model change),
    // and that config carries no PS pair, so no paired signal may be emitted.
    void puresignalOff_receiving_emitsNoPairedIq()
    {
        P1RadioConnection conn;
        conn.init();
        conn.applyPsDdcConfig(hl2Config(/*psEnabled=*/false, /*moxState=*/false));

        QSignalSpy spy(&conn, &RadioConnection::psPairedIqDataReceived);
        QVERIFY(spy.isValid());

        const QByteArray frame = makeEp6Frame();
        for (int i = 0; i < 5; ++i) {
            conn.parseEp6FrameForTest(frame);
        }

        QCOMPARE(spy.count(), 0);
    }

    // Keyed up with PureSignal off is still not a PS pair. The HL2 branch for
    // (mox, !diversity, !ps) is the plain single-DDC one, mi0bot
    // console.cs:8444-8457 [v2.10.3.13-beta2].
    void puresignalOff_transmitting_emitsNoPairedIq()
    {
        P1RadioConnection conn;
        conn.init();
        conn.applyPsDdcConfig(hl2Config(/*psEnabled=*/false, /*moxState=*/true));

        QSignalSpy spy(&conn, &RadioConnection::psPairedIqDataReceived);
        QVERIFY(spy.isValid());

        conn.parseEp6FrameForTest(makeEp6Frame());

        QCOMPARE(spy.count(), 0);
    }

    // The other half of the contract: a codec that really does configure a
    // pair must still get one paired emit per frame, on the DDC indices it
    // asked for. HL2 PS-MOX is DDC2 feedback + DDC3 TX monitor.
    void puresignalMox_emitsThePairTheCodecConfigured()
    {
        const PsDdcConfig ps = hl2Config(/*psEnabled=*/true, /*moxState=*/true);
        QCOMPARE(ps.psFbDdc,  2);
        QCOMPARE(ps.txMonDdc, 3);

        P1RadioConnection conn;
        conn.init();
        conn.applyPsDdcConfig(ps);

        QSignalSpy spy(&conn, &RadioConnection::psPairedIqDataReceived);
        QVERIFY(spy.isValid());

        conn.parseEp6FrameForTest(makeEp6Frame());

        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.first();
        QCOMPARE(args.at(0).toInt(), 2);   // psFbDdc
        QCOMPARE(args.at(2).toInt(), 3);   // txMonDdc
        QVERIFY(!args.at(1).value<QVector<float>>().isEmpty());
        QVERIFY(!args.at(3).value<QVector<float>>().isEmpty());
    }

    // Unkeying has to shut the gate again. A latch that only ever opens would
    // leave the pairing running for the rest of the session after the first
    // PureSignal transmission.
    void leavingPuresignalMox_closesTheGateAgain()
    {
        P1RadioConnection conn;
        conn.init();
        conn.applyPsDdcConfig(hl2Config(/*psEnabled=*/true, /*moxState=*/true));

        QSignalSpy spy(&conn, &RadioConnection::psPairedIqDataReceived);
        QVERIFY(spy.isValid());

        conn.parseEp6FrameForTest(makeEp6Frame());
        QCOMPARE(spy.count(), 1);

        // Back to receive with PureSignal still switched on at the operator's
        // master toggle: no MOX, so no PS pair.
        conn.applyPsDdcConfig(hl2Config(/*psEnabled=*/true, /*moxState=*/false));
        spy.clear();

        for (int i = 0; i < 5; ++i) {
            conn.parseEp6FrameForTest(makeEp6Frame());
        }

        QCOMPARE(spy.count(), 0);
    }
};

QTEST_GUILESS_MAIN(TestP1PsPairGate)
#include "tst_p1_ps_pair_gate.moc"
