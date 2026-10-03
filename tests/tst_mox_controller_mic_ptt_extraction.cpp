// =================================================================
// tests/tst_mox_controller_mic_ptt_extraction.cpp  (NereusSDR)
// =================================================================
//
// Phase 3M-1b Task H.5: verifies end-to-end mic_ptt extraction from P1/P2
// status frames and dispatch through MoxController.
//
// Two test prongs:
//
//   Prong A — RadioConnection-level extraction:
//     P1: parseEp6Frame() with C0 bit 0 set → micPttFromRadio(true)
//     P1: parseEp6Frame() with C0 bit 0 clear → micPttFromRadio(false)
//     P2: processHighPriorityStatus() with raw[4] bit 0 set → micPttFromRadio(true)
//     P2: processHighPriorityStatus() with raw[4] bit 0 clear → micPttFromRadio(false)
//
//   Prong B — End-to-end (status frame → MoxController):
//     Wire P1RadioConnection::micPttFromRadio → MoxController::onMicPttFromRadio.
//     Inject status frame with bit 0 set → MoxController engaged, PttMode::Mic.
//     Inject status frame with bit 0 clear → MoxController released.
//     Same for P2.
//
// Source references:
//   Thetis Project Files/Source/Console/console.cs:25426 [v2.10.3.13]:
//     PollPTT: bool mic_ptt = (dotdashptt & 0x01) != 0; // PTT from radio
//   Thetis Project Files/Source/ChannelMaster/networkproto1.c:329 [v2.10.3.13]:
//     prn->ptt_in = ControlBytesIn[0] & 0x1;
//   Thetis Project Files/Source/ChannelMaster/network.c:686-689 [v2.10.3.13]:
//     //Byte 0 - Bit [0] - PTT  1 = active, 0 = inactive
//     prn->ptt_in = prn->ReadBufp[0] & 0x1;
//   deskhpsdr/src/new_protocol.c:2525 [@120188f]:
//     radio_ptt = (buffer[4]) & 0x01;
//   Pre-code review §8.2 (vox_ptt dispatch context; mic_ptt bit-layout).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-28 — Phase 3M-1b Task H.5 — J.J. Boyd (KG4VCF),
//                with AI-assisted authoring via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file — no upstream Thetis port.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QByteArray>
#include <QCoreApplication>

#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/MoxController.h"
#include "core/PttMode.h"

using namespace NereusSDR;

namespace {

// ──────────────────────────────────────────────────────────────────────────
// Helper: build a 1032-byte EP6 datagram with specified C0 for both
// sub-frames.  Follows the layout from tst_p1_status_telemetry.cpp.
//
// Source: Thetis networkproto1.c:320-357 [@501e3f5] — EP6 frame layout:
//   [0..3]    = Metis header (0xEF 0xFE 0x01 0x06)
//   [4..7]    = zero padding
//   [8..10]   = sub-frame 0 sync (0x7F 0x7F 0x7F)
//   [11..15]  = sub-frame 0 C&C (C0..C4)
//   [520..522]= sub-frame 1 sync
//   [523..527]= sub-frame 1 C&C (C0..C4)
// ──────────────────────────────────────────────────────────────────────────
QByteArray makeEp6Frame(quint8 c0Sub0, quint8 c0Sub1)
{
    QByteArray pkt(1032, '\0');
    auto* p = reinterpret_cast<quint8*>(pkt.data());

    // Metis ep6 magic — networkproto1.c:326-327 [@501e3f5]
    p[0] = 0xEF; p[1] = 0xFE; p[2] = 0x01; p[3] = 0x06;

    // Sub-frame 0: sync at [8..10], C0 at [11]
    p[8] = 0x7F; p[9] = 0x7F; p[10] = 0x7F;
    p[11] = c0Sub0;  // C0 — bit 0 = ptt_in

    // Sub-frame 1: sync at [520..522], C0 at [523]
    p[520] = 0x7F; p[521] = 0x7F; p[522] = 0x7F;
    p[523] = c0Sub1;  // C0 — bit 0 = ptt_in

    return pkt;
}

// ──────────────────────────────────────────────────────────────────────────
// Helper: build a 60-byte P2 High-Priority status packet with bit 0 of
// raw[4] (ReadBufp[0]) set or cleared per the ptt argument.
//
// Source: Thetis network.c:531 [v2.10.3.13]: memcpy(bufp, readbuf+4, 56)
//   → ReadBufp[0] = raw[4] in NereusSDR.
// network.c:689 [v2.10.3.13]:
//   prn->ptt_in = prn->ReadBufp[0] & 0x1;
// ──────────────────────────────────────────────────────────────────────────
QByteArray makeHighPriorityPacket(bool ptt)
{
    QByteArray pkt(60, '\0');
    auto* p = reinterpret_cast<quint8*>(pkt.data());

    // Sequence number 1 at raw[0..3] (big-endian)
    p[3] = 0x01;

    // raw[4] = ReadBufp[0]: PTT/dot/dash byte.
    // Bit 0 = PTT per Thetis network.c:689 [v2.10.3.13].
    p[4] = ptt ? 0x01 : 0x00;

    return pkt;
}

// Convenience: pump the Qt event loop twice to drain queued signals.
void drainEvents()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}

// Convenience: make MoxController state machine synchronous.
void makeSync(MoxController& ctrl)
{
    ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
}

} // anonymous namespace

class TestMoxControllerMicPttExtraction : public QObject {
    Q_OBJECT

private slots:

    // ════════════════════════════════════════════════════════════════════════
    // § A — Prong A: P1 status-frame extraction
    //
    // Verifies P1RadioConnection::parseEp6Frame() emits micPttFromRadio(bool)
    // based on C0 bit 0 of the EP6 C&C sub-frame.
    //
    // Source: Thetis networkproto1.c:329 [v2.10.3.13]:
    //   prn->ptt_in = ControlBytesIn[0] & 0x1;
    // ════════════════════════════════════════════════════════════════════════

    // §A.1 — P1: C0 bit 0 set in sub-frame 0 → micPttFromRadio(true)
    void p1_c0Bit0Set_emitsMicPttTrue()
    {
        // C0 = 0x01: bit 0 = ptt_in=1.  Sub-frame 1 has C0=0x00 (ptt_in=0),
        // but OR semantics means any sub-frame with ptt_in=1 yields true.
        QByteArray pkt = makeEp6Frame(/*sub0 C0*/ 0x01, /*sub1 C0*/ 0x00);

        P1RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &P1RadioConnection::micPttFromRadio);

        conn.parseEp6FrameForTest(pkt);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    // §A.2 — P1: C0 bit 0 set in sub-frame 1 only → micPttFromRadio(true)
    //
    // OR logic: either sub-frame asserting ptt_in yields a pressed signal.
    void p1_c0Bit0SetInSub1Only_emitsMicPttTrue()
    {
        QByteArray pkt = makeEp6Frame(/*sub0 C0*/ 0x00, /*sub1 C0*/ 0x01);

        P1RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &P1RadioConnection::micPttFromRadio);

        conn.parseEp6FrameForTest(pkt);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    // §A.3 — P1: C0 bit 0 clear in both sub-frames → micPttFromRadio(false)
    void p1_c0Bit0Clear_emitsMicPttFalse()
    {
        // C0 = 0x00 in both sub-frames: ptt_in=0.
        QByteArray pkt = makeEp6Frame(/*sub0 C0*/ 0x00, /*sub1 C0*/ 0x00);

        P1RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &P1RadioConnection::micPttFromRadio);

        conn.parseEp6FrameForTest(pkt);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
    }

    // §A.4 — P1: ADC-overload bit (bit 0 in case 0x00 C1) does NOT affect
    //         micPttFromRadio — the PTT bit is in C0, not C1.
    //
    // C0 = 0x00 (bank 0 decode selector — NOT the PTT bit), C1 = 0x01
    // (ADC0 overflow).  The PTT bit is C0 bit 0, which is 0 here.
    // micPttFromRadio must be false.
    void p1_adcOverloadInC1_doesNotSetMicPtt()
    {
        QByteArray pkt(1032, '\0');
        auto* p = reinterpret_cast<quint8*>(pkt.data());
        p[0] = 0xEF; p[1] = 0xFE; p[2] = 0x01; p[3] = 0x06;
        p[8] = 0x7F; p[9] = 0x7F; p[10] = 0x7F;
        p[11] = 0x00;  // C0 = 0x00 (bank 0, ptt_in=0)
        p[12] = 0x01;  // C1 = 0x01 (ADC0 overflow — does NOT influence PTT)
        p[520] = 0x7F; p[521] = 0x7F; p[522] = 0x7F;
        p[523] = 0x00;  // sub-frame 1 C0 = 0x00

        P1RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &P1RadioConnection::micPttFromRadio);

        conn.parseEp6FrameForTest(pkt);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
    }

    // ════════════════════════════════════════════════════════════════════════
    // § B — Prong A: P2 status-frame extraction
    //
    // Verifies P2RadioConnection::processHighPriorityStatus() emits
    // micPttFromRadio(bool) based on raw[4] bit 0 (ReadBufp[0] bit 0).
    //
    // Source: Thetis network.c:686-689 [v2.10.3.13]:
    //   //Byte 0 - Bit [0] - PTT  1 = active, 0 = inactive
    //   prn->ptt_in = prn->ReadBufp[0] & 0x1;
    // ReadBufp[0] = raw[4] in NereusSDR (4-byte seq prefix, then
    // memcpy(bufp, readbuf+4, 56) per network.c:531 [v2.10.3.13]).
    // ════════════════════════════════════════════════════════════════════════

    // §B.1 — P2: raw[4] bit 0 set → micPttFromRadio(true)
    void p2_byte4Bit0Set_emitsMicPttTrue()
    {
        QByteArray pkt = makeHighPriorityPacket(/*ptt*/ true);

        P2RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &P2RadioConnection::micPttFromRadio);

        conn.processHighPriorityStatusForTest(pkt);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    // §B.2 — P2: raw[4] bit 0 clear → micPttFromRadio(false)
    void p2_byte4Bit0Clear_emitsMicPttFalse()
    {
        QByteArray pkt = makeHighPriorityPacket(/*ptt*/ false);

        P2RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &P2RadioConnection::micPttFromRadio);

        conn.processHighPriorityStatusForTest(pkt);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
    }

    // §B.3 — P2: raw[4] dot/dash bits (bits 1/2) do NOT set PTT
    //
    // Only bit 0 is PTT.  Bits 1/2 are Dot and Dash per network.c:690-691.
    void p2_dotDashBitsOnly_emitsMicPttFalse()
    {
        QByteArray pkt(60, '\0');
        auto* p = reinterpret_cast<quint8*>(pkt.data());
        p[3] = 0x01;   // seq = 1
        p[4] = 0x06;   // bits 1+2 = Dot+Dash, bit 0 = 0 (no PTT)

        P2RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &P2RadioConnection::micPttFromRadio);

        conn.processHighPriorityStatusForTest(pkt);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
    }

    // §B.4 — P2: ADC overload bits at raw[5] do NOT affect micPttFromRadio
    //
    // raw[4] bit 0 = 0 (no PTT); raw[5] bit 0 = 1 (ADC0 overload).
    // Confirms the byte-offset correction (raw[4]=PTT, raw[5]=ADC overload).
    void p2_adcOverloadInByte5_doesNotSetMicPtt()
    {
        QByteArray pkt(60, '\0');
        auto* p = reinterpret_cast<quint8*>(pkt.data());
        p[3] = 0x01;   // seq = 1
        p[4] = 0x00;   // PTT byte: bit 0 = 0 (no PTT)
        p[5] = 0x01;   // ADC overload byte: ADC0 overload (not PTT)

        P2RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &P2RadioConnection::micPttFromRadio);

        conn.processHighPriorityStatusForTest(pkt);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
    }

    // ════════════════════════════════════════════════════════════════════════
    // § C — Prong B: P1 end-to-end (status frame → MoxController)
    //
    // Wire P1RadioConnection::micPttFromRadio to MoxController::onMicPttFromRadio
    // via a direct connection (same thread in tests) and verify the MOX state
    // machine responds correctly.
    // ════════════════════════════════════════════════════════════════════════

    // §C.1 — P1 PTT press end-to-end: engaged MoxController with PttMode::Mic
    void p1_endToEnd_pttPress_engagesMox()
    {
        P1RadioConnection conn;
        conn.init();

        MoxController ctrl;
        makeSync(ctrl);

        // Wire: direct connection (both objects live on main thread in tests).
        connect(&conn, &P1RadioConnection::micPttFromRadio,
                &ctrl, &MoxController::onMicPttFromRadio,
                Qt::DirectConnection);

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        // Inject frame with PTT bit set in both sub-frames.
        QByteArray pkt = makeEp6Frame(/*sub0 C0*/ 0x01, /*sub1 C0*/ 0x01);
        conn.parseEp6FrameForTest(pkt);
        drainEvents();

        QCOMPARE(pttSpy.count(), 1);
        QCOMPARE(pttSpy.at(0).at(0).value<PttMode>(), PttMode::Mic);
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);

        QCOMPARE(moxSpy.count(), 1);
        QCOMPARE(moxSpy.at(0).at(0).toBool(), true);
        QVERIFY(ctrl.isMox());
    }

    // §C.2 — P1 PTT release end-to-end: MoxController released
    void p1_endToEnd_pttRelease_releasesMox()
    {
        P1RadioConnection conn;
        conn.init();

        MoxController ctrl;
        makeSync(ctrl);

        connect(&conn, &P1RadioConnection::micPttFromRadio,
                &ctrl, &MoxController::onMicPttFromRadio,
                Qt::DirectConnection);

        // Press first.
        QByteArray pktPressed = makeEp6Frame(0x01, 0x01);
        conn.parseEp6FrameForTest(pktPressed);
        drainEvents();

        QVERIFY(ctrl.isMox());

        // Now release.
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        QByteArray pktReleased = makeEp6Frame(0x00, 0x00);
        conn.parseEp6FrameForTest(pktReleased);
        drainEvents();

        QCOMPARE(moxSpy.count(), 1);
        QCOMPARE(moxSpy.at(0).at(0).toBool(), false);
        QVERIFY(!ctrl.isMox());
        // The unkey clears the PTT mode, as chkMOX_CheckedChanged2 does
        // (console.cs:29547 [v2.10.3.15]; receiver and transmit gaps plan,
        // Task 7).
        QCOMPARE(ctrl.pttMode(), PttMode::None);
    }

    // §C.3 — P1 repeated press is idempotent: no double-emit
    //
    // Unconditional emission means the frame-level PTT signal fires on every
    // EP6 frame while PTT is held.  MoxController must be idempotent:
    // second onMicPttFromRadio(true) → no new pttModeChanged, no new
    // moxStateChanged (state machine gate blocks re-entry).
    void p1_endToEnd_repeatedPress_idempotent()
    {
        P1RadioConnection conn;
        conn.init();

        MoxController ctrl;
        makeSync(ctrl);

        connect(&conn, &P1RadioConnection::micPttFromRadio,
                &ctrl, &MoxController::onMicPttFromRadio,
                Qt::DirectConnection);

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        QByteArray pkt = makeEp6Frame(0x01, 0x01);

        conn.parseEp6FrameForTest(pkt);
        drainEvents();

        QCOMPARE(pttSpy.count(), 1);
        QCOMPARE(moxSpy.count(), 1);

        // Second frame with same PTT state — no new signals.
        conn.parseEp6FrameForTest(pkt);
        drainEvents();

        QCOMPARE(pttSpy.count(), 1);  // no new pttModeChanged
        QCOMPARE(moxSpy.count(), 1);  // no new moxStateChanged
        QVERIFY(ctrl.isMox());
    }

    // ════════════════════════════════════════════════════════════════════════
    // § D — Prong B: P2 end-to-end (status frame → MoxController)
    // ════════════════════════════════════════════════════════════════════════

    // §D.1 — P2 PTT press end-to-end: engaged MoxController with PttMode::Mic
    void p2_endToEnd_pttPress_engagesMox()
    {
        P2RadioConnection conn;
        conn.init();

        MoxController ctrl;
        makeSync(ctrl);

        connect(&conn, &P2RadioConnection::micPttFromRadio,
                &ctrl, &MoxController::onMicPttFromRadio,
                Qt::DirectConnection);

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        QByteArray pkt = makeHighPriorityPacket(/*ptt*/ true);
        conn.processHighPriorityStatusForTest(pkt);
        drainEvents();

        QCOMPARE(pttSpy.count(), 1);
        QCOMPARE(pttSpy.at(0).at(0).value<PttMode>(), PttMode::Mic);
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);

        QCOMPARE(moxSpy.count(), 1);
        QCOMPARE(moxSpy.at(0).at(0).toBool(), true);
        QVERIFY(ctrl.isMox());
    }

    // §D.2 — P2 PTT release end-to-end: MoxController released
    void p2_endToEnd_pttRelease_releasesMox()
    {
        P2RadioConnection conn;
        conn.init();

        MoxController ctrl;
        makeSync(ctrl);

        connect(&conn, &P2RadioConnection::micPttFromRadio,
                &ctrl, &MoxController::onMicPttFromRadio,
                Qt::DirectConnection);

        // Press first.
        QByteArray pktPressed = makeHighPriorityPacket(true);
        conn.processHighPriorityStatusForTest(pktPressed);
        drainEvents();

        QVERIFY(ctrl.isMox());

        // Release.
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        QByteArray pktReleased = makeHighPriorityPacket(false);
        conn.processHighPriorityStatusForTest(pktReleased);
        drainEvents();

        QCOMPARE(moxSpy.count(), 1);
        QCOMPARE(moxSpy.at(0).at(0).toBool(), false);
        QVERIFY(!ctrl.isMox());
        // The unkey clears the PTT mode (Task 7).
        QCOMPARE(ctrl.pttMode(), PttMode::None);
    }

    // §D.3 — P2 repeated press is idempotent: no double-emit
    void p2_endToEnd_repeatedPress_idempotent()
    {
        P2RadioConnection conn;
        conn.init();

        MoxController ctrl;
        makeSync(ctrl);

        connect(&conn, &P2RadioConnection::micPttFromRadio,
                &ctrl, &MoxController::onMicPttFromRadio,
                Qt::DirectConnection);

        QSignalSpy pttSpy(&ctrl, &MoxController::pttModeChanged);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);

        QByteArray pkt = makeHighPriorityPacket(true);

        conn.processHighPriorityStatusForTest(pkt);
        drainEvents();

        QCOMPARE(pttSpy.count(), 1);
        QCOMPARE(moxSpy.count(), 1);

        conn.processHighPriorityStatusForTest(pkt);
        drainEvents();

        QCOMPARE(pttSpy.count(), 1);  // idempotent
        QCOMPARE(moxSpy.count(), 1);  // idempotent
        QVERIFY(ctrl.isMox());
    }

    // ════════════════════════════════════════════════════════════════════════
    // § E — Signal emitted unconditionally (not only on transition)
    //
    // The design choice is to emit on every frame, not only on state
    // transition.  This verifies that two frames with PTT=false both emit
    // the signal (twice).  MoxController idempotency handles the redundancy.
    // ════════════════════════════════════════════════════════════════════════

    // §E.1 — P1: consecutive false frames both emit micPttFromRadio
    void p1_consecutiveFalseFrames_emitsTwice()
    {
        P1RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &P1RadioConnection::micPttFromRadio);

        QByteArray pkt = makeEp6Frame(0x00, 0x00);

        conn.parseEp6FrameForTest(pkt);
        conn.parseEp6FrameForTest(pkt);

        // Unconditional emission: both frames fire the signal.
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
        QCOMPARE(spy.at(1).at(0).toBool(), false);
    }

    // §E.2 — P2: consecutive false packets both emit micPttFromRadio
    void p2_consecutiveFalsePackets_emitsTwice()
    {
        P2RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &P2RadioConnection::micPttFromRadio);

        QByteArray pkt0 = makeHighPriorityPacket(false);
        // Bump seq to avoid sequence-error log noise.
        {
            auto* p = reinterpret_cast<quint8*>(pkt0.data());
            p[3] = 0x01;
        }
        QByteArray pkt1 = makeHighPriorityPacket(false);
        {
            auto* p = reinterpret_cast<quint8*>(pkt1.data());
            p[3] = 0x02;
        }

        conn.processHighPriorityStatusForTest(pkt0);
        conn.processHighPriorityStatusForTest(pkt1);

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
        QCOMPARE(spy.at(1).at(0).toBool(), false);
    }
};

QTEST_MAIN(TestMoxControllerMicPttExtraction)
#include "tst_mox_controller_mic_ptt_extraction.moc"
