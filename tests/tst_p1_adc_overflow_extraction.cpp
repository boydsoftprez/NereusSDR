// no-port-check: test harness citing Thetis networkproto1.c offsets as the
// behavioural spec it asserts against; contains no ported logic of its own.
// =================================================================
// tests/tst_p1_adc_overflow_extraction.cpp  (NereusSDR)
// =================================================================
//
// Regression tests for issue #176 — the P1 EP6 status-frame parser was
// reading C0 bit 0 (which is mic_PTT) and emitting it as adcOverflow.
// That meant:
//   1. Closing the footswitch fired a false ADC-overload every frame
//      while held — the badge ramped yellow→red and tailed back through
//      yellow on release.
//   2. UI MOX (which never closes the PTT input pin) never saw any OVL
//      indication.
//   3. A genuine LT2208 saturation, reported by the radio in C1 bit 0,
//      was invisible — the parser was looking at the wrong byte.
//
// The correct extraction per Thetis is:
//
//   networkproto1.c:329 [v2.10.3.13]:
//     prn->ptt_in = ControlBytesIn[0] & 0x1;            // C0 bit 0
//
//   networkproto1.c:332-335 [v2.10.3.13]:
//     switch (ControlBytesIn[0] & 0xf8)
//       case 0x00:
//         prn->adc[0].adc_overload = ... ControlBytesIn[1] & 0x01;
//                                          // C1 bit 0 — type 0x00 only
//
//   networkproto1.c:353-355 [v2.10.3.13]:
//     case 0x20:
//       prn->adc[0].adc_overload = ... ControlBytesIn[1] & 1;
//       prn->adc[1].adc_overload = ... (ControlBytesIn[2] & 1) << 1;
//       prn->adc[2].adc_overload = ... (ControlBytesIn[3] & 1) << 2;
//
// This test feeds 1032-byte EP6 datagrams that exercise each of:
//   - PTT bit set, OVL bit clear  → adcOverflow MUST NOT fire
//   - OVL bit set in case 0x00    → adcOverflow(0) fires once
//   - C1 bit 0 in non-OVL case    → adcOverflow MUST NOT fire (gated by C0 type)
//   - Multi-ADC case 0x20         → adcOverflow(0), (1), (2) each fire
//   - PTT + OVL set together      → both signals fire independently
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-04 — Issue #176 regression — J.J. Boyd (KG4VCF),
//                 with AI-assisted authoring via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QByteArray>

#include "core/P1RadioConnection.h"

using namespace NereusSDR;

namespace {

// Build a 1032-byte EP6 datagram with both subframes' sync words +
// caller-supplied 5-byte C&C (C0..C4) per subframe.  Sample bytes are
// zero — parseEp6Frame() accepts the frame because magic + sync are
// present.  Layout per Thetis networkproto1.c:320-357 [v2.10.3.13].
QByteArray makeEp6Frame(const quint8 sub0[5], const quint8 sub1[5])
{
    QByteArray pkt(1032, '\0');
    auto* p = reinterpret_cast<quint8*>(pkt.data());

    // Metis EP6 magic — networkproto1.c:326-327 [v2.10.3.13]
    p[0] = 0xEF; p[1] = 0xFE; p[2] = 0x01; p[3] = 0x06;

    // Subframe 0: sync at [8..10], C&C at [11..15]
    p[8] = 0x7F; p[9] = 0x7F; p[10] = 0x7F;
    for (int i = 0; i < 5; ++i) { p[11 + i] = sub0[i]; }

    // Subframe 1: sync at [520..522], C&C at [523..527]
    p[520] = 0x7F; p[521] = 0x7F; p[522] = 0x7F;
    for (int i = 0; i < 5; ++i) { p[523 + i] = sub1[i]; }

    return pkt;
}

} // anonymous namespace

class TestP1AdcOverflowExtraction : public QObject {
    Q_OBJECT
private slots:

    // ── Headline regression for #176 ──────────────────────────────────────
    // PTT bit set in C0, no OVL bit anywhere → adcOverflow MUST NOT fire.
    //
    // Without the fix: the parser reads C0 bit 0 as the OVL source, so
    //   pressing the footswitch sends an unbroken stream of fake OVLs.
    //
    // From Thetis networkproto1.c:329 [v2.10.3.13]:
    //   prn->ptt_in = ControlBytesIn[0] & 0x1;            // C0 bit 0
    // From Thetis networkproto1.c:335 [v2.10.3.13]:
    //   prn->adc[0].adc_overload = ... ControlBytesIn[1] & 0x01;
    //   // only cleared by getAndResetADC_Overload() //[2.10.3.13]MW0LGE
    void pttHeldNoOverload_doesNotEmitAdcOverflow()
    {
        // Sub 0: C0 = 0x01 (type=0x00, ptt_in=1), C1 = 0x00 (no OVL)
        // Sub 1: C0 = 0x00 (no PTT), C1 = 0x00
        const quint8 sub0[5] = {0x01, 0x00, 0x00, 0x00, 0x00};
        const quint8 sub1[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
        const QByteArray pkt = makeEp6Frame(sub0, sub1);

        P1RadioConnection conn;
        conn.init();
        QSignalSpy ovlSpy(&conn, &P1RadioConnection::adcOverflow);
        QSignalSpy pttSpy(&conn, &P1RadioConnection::micPttFromRadio);

        conn.parseEp6FrameForTest(pkt);

        // PTT must reach MoxController so the footswitch still works.
        QCOMPARE(pttSpy.count(), 1);
        QCOMPARE(pttSpy.at(0).at(0).toBool(), true);
        // OVL must NOT fire — no overload was reported by the radio.
        QCOMPARE(ovlSpy.count(), 0);
    }

    // ── Real OVL in case 0x00 fires on the right ADC ───────────────────────
    // From Thetis networkproto1.c:332-335 [v2.10.3.13]:
    //   switch (ControlBytesIn[0] & 0xf8) case 0x00:
    //     prn->adc[0].adc_overload = ... ControlBytesIn[1] & 0x01;
    //   // only cleared by getAndResetADC_Overload() //[2.10.3.13]MW0LGE
    void overloadInCase00_emitsAdcOverflowZero()
    {
        // Sub 0: C0 = 0x00 (type=0x00, ptt=0), C1 = 0x01 (OVL bit set)
        // Sub 1: C0 = 0x00, C1 = 0x00 (quiet — only one report)
        const quint8 sub0[5] = {0x00, 0x01, 0x00, 0x00, 0x00};
        const quint8 sub1[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
        const QByteArray pkt = makeEp6Frame(sub0, sub1);

        P1RadioConnection conn;
        conn.init();
        QSignalSpy ovlSpy(&conn, &P1RadioConnection::adcOverflow);

        conn.parseEp6FrameForTest(pkt);

        QCOMPARE(ovlSpy.count(), 1);
        QCOMPARE(ovlSpy.at(0).at(0).toInt(), 0);
    }

    // ── C1 bit 0 in a non-OVL case must NOT fire OVL ───────────────────────
    // Type 0x08 carries exciter_power MSB in C1 — bit 0 of C1 there is the
    // top byte's LSB, NOT an overload report.  This test guards the type
    // gate so the fix can't over-fire.
    //
    // From Thetis networkproto1.c:339 [v2.10.3.13]:
    //   case 0x08:
    //     prn->tx[0].exciter_power = ((ControlBytesIn[1] << 8) ...) ...
    // (Adjacent upstream :335 OVL line carries //[2.10.3.13]MW0LGE — preserved
    //  in the case 0x00 body of the port; no OVL semantics in case 0x08.)
    void c1Bit0InCase08_doesNotEmitAdcOverflow()
    {
        // Sub 0: C0 = 0x08 (PA telemetry case), C1 = 0x01 (exciter MSB lsb)
        // Sub 1: C0 = 0x00, C1 = 0x00 (no OVL anywhere)
        const quint8 sub0[5] = {0x08, 0x01, 0x00, 0x00, 0x00};
        const quint8 sub1[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
        const QByteArray pkt = makeEp6Frame(sub0, sub1);

        P1RadioConnection conn;
        conn.init();
        QSignalSpy ovlSpy(&conn, &P1RadioConnection::adcOverflow);

        conn.parseEp6FrameForTest(pkt);

        QCOMPARE(ovlSpy.count(), 0);
    }

    // ── Multi-ADC case 0x20 fires ADC0 / ADC1 / ADC2 independently ─────────
    // From Thetis networkproto1.c:353-355 [v2.10.3.13]:
    //   case 0x20:
    //     prn->adc[0].adc_overload = ... ControlBytesIn[1] & 1;        //[2.10.3.13]MW0LGE
    //     prn->adc[1].adc_overload = ... (ControlBytesIn[2] & 1) << 1; //[2.10.3.13]MW0LGE
    //     prn->adc[2].adc_overload = ... (ControlBytesIn[3] & 1) << 2; //[2.10.3.13]MW0LGE
    void overloadInCase20_emitsAllThreeAdcs()
    {
        // Sub 0: C0 = 0x20 (multi-ADC OVL case), C1=C2=C3 each bit 0 set
        // Sub 1: C0 = 0x00 (quiet)
        const quint8 sub0[5] = {0x20, 0x01, 0x01, 0x01, 0x00};
        const quint8 sub1[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
        const QByteArray pkt = makeEp6Frame(sub0, sub1);

        P1RadioConnection conn;
        conn.init();
        QSignalSpy ovlSpy(&conn, &P1RadioConnection::adcOverflow);

        conn.parseEp6FrameForTest(pkt);

        // Three independent ADC reports in this single frame.  Order of
        // emission follows the Thetis switch body (ADC0 → ADC1 → ADC2).
        QCOMPARE(ovlSpy.count(), 3);
        QCOMPARE(ovlSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(ovlSpy.at(1).at(0).toInt(), 1);
        QCOMPARE(ovlSpy.at(2).at(0).toInt(), 2);
    }

    // ── PTT and OVL fire independently when both bits are set ──────────────
    // Confirms the two paths are wired to different bytes after the fix.
    void pttAndOverloadTogether_fireIndependently()
    {
        // Sub 0: C0 = 0x01 (ptt_in=1, type=0x00), C1 = 0x01 (OVL bit set)
        // Sub 1: C0 = 0x00, C1 = 0x00
        const quint8 sub0[5] = {0x01, 0x01, 0x00, 0x00, 0x00};
        const quint8 sub1[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
        const QByteArray pkt = makeEp6Frame(sub0, sub1);

        P1RadioConnection conn;
        conn.init();
        QSignalSpy ovlSpy(&conn, &P1RadioConnection::adcOverflow);
        QSignalSpy pttSpy(&conn, &P1RadioConnection::micPttFromRadio);

        conn.parseEp6FrameForTest(pkt);

        QCOMPARE(pttSpy.count(), 1);
        QCOMPARE(pttSpy.at(0).at(0).toBool(), true);
        QCOMPARE(ovlSpy.count(), 1);
        QCOMPARE(ovlSpy.at(0).at(0).toInt(), 0);
    }
};

QTEST_GUILESS_MAIN(TestP1AdcOverflowExtraction)
#include "tst_p1_adc_overflow_extraction.moc"
