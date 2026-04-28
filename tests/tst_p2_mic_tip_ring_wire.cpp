// no-port-check: test-only — deskhpsdr file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No deskhpsdr logic is ported here; this file is NereusSDR-original.
//
// Wire-byte snapshot tests for P2RadioConnection::setMicTipRing() (3M-1b Task G.3).
//
// mic_ptt_tip_bias_ring bit position: CmdTx buffer byte 50, bit 3 (mask 0x08).
// POLARITY INVERSION: parameter tipHot = true means "Tip is mic" (intuitive).
// Wire bit is INVERTED: 0 = Tip-is-mic (tipHot=true), 1 = Tip-is-BIAS (tipHot=false).
//
// Source cite:
//   deskhpsdr/src/new_protocol.c:1492-1494 [@120188f]
//     if (mic_ptt_tip_bias_ring) {
//       transmit_specific_buffer[50] |= 0x08;
//     }
//
// Note: P2 bit position (bit 3 = 0x08) differs from P1 bit position
// (bit 4 = 0x10 in C1 of bank 11). Both carry the same inverted semantics.
//
// CmdTx buffer layout (60 bytes, relevant bytes only):
//   byte 50: mic control byte (m_mic.micControl)
//             bit 0 (0x01): line_in  (G.2)
//             bit 1 (0x02): mic_boost (G.1)
//             bit 2 (0x04): mic_ptt_disabled (inverted — future G task)
//             bit 3 (0x08): mic_ptt_tip_bias_ring (INVERTED — G.3)
//             bit 4 (0x10): mic_bias
//             bit 5 (0x20): mic_xlr (Saturn/XLR only)
//   byte 51: line_in gain
//   bytes 57-59: TX step attenuators
//
// Test seam: composeCmdTxForTest() in P2RadioConnection.h (NEREUS_BUILD_TESTS)
// exposes the CmdTx buffer composition without needing a live socket.
#include <QtTest/QtTest>
#include "core/P2RadioConnection.h"

using namespace NereusSDR;

class TestP2MicTipRingWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: byte 50 bit 3 is CLEAR (tip-is-mic, default tipHot=true) ──
    // m_micTipRing defaults true (Tip is mic), so CmdTx byte 50 bit 3 must be 0.
    // Source: deskhpsdr/src/new_protocol.c:1492-1494 [@120188f]
    void defaultState_byte50Bit3IsClear() {
        P2RadioConnection conn;
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x08), 0);
    }

    // ── 2. setMicTipRing(false) [tip-is-bias] → byte 50 bit 3 set ────────────
    // tipHot = false → Tip is BIAS/PTT → mic_ptt_tip_bias_ring = 1 → bit 3 = 1.
    // Source: deskhpsdr/src/new_protocol.c:1492-1494 [@120188f]
    //   if (mic_ptt_tip_bias_ring) { transmit_specific_buffer[50] |= 0x08; }
    void setMicTipRingFalse_byte50Bit3IsSet() {
        P2RadioConnection conn;
        conn.setMicTipRing(false);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x08), 0x08);
    }

    // ── 3. setMicTipRing(true) [tip-is-mic] → byte 50 bit 3 clear ────────────
    void setMicTipRingTrue_byte50Bit3IsClear() {
        P2RadioConnection conn;
        conn.setMicTipRing(false);  // set first
        conn.setMicTipRing(true);   // then clear
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x08), 0);
    }

    // ── 4. Round-trip: false → true → false ──────────────────────────────────
    void roundTrip_falseToTrueToFalse() {
        P2RadioConnection conn;

        conn.setMicTipRing(false);
        quint8 buf1[60] = {};
        conn.composeCmdTxForTest(buf1);
        QCOMPARE(int(buf1[50] & 0x08), 0x08);

        conn.setMicTipRing(true);
        quint8 buf2[60] = {};
        conn.composeCmdTxForTest(buf2);
        QCOMPARE(int(buf2[50] & 0x08), 0);

        conn.setMicTipRing(false);
        quint8 buf3[60] = {};
        conn.composeCmdTxForTest(buf3);
        QCOMPARE(int(buf3[50] & 0x08), 0x08);
    }

    // ── 5. setMicTipRing(false) does NOT touch byte-50 bits 0-2 (G.1/G.2 bits) ──
    // Cross-bit guard: mic_trs (bit 3 = 0x08) must not collide with
    // line_in (bit 0 = 0x01) or mic_boost (bit 1 = 0x02).
    // Source: deskhpsdr/src/new_protocol.c:1480-1494 [@120188f]
    void micBoostAndLineInBits_unaffectedByMicTipRing() {
        P2RadioConnection conn;
        conn.setMicTipRing(false);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        // Bit 3 (mic_trs) must be 1.
        QCOMPARE(int(buf[50] & 0x08), 0x08);
        // Bits 0 (line_in) and 1 (mic_boost) must be 0.
        QCOMPARE(int(buf[50] & 0x01), 0);
        QCOMPARE(int(buf[50] & 0x02), 0);
    }

    // ── 6. Bits 4-7 of byte 50 unaffected by setMicTipRing ───────────────────
    // Only bit 3 (mic_trs) changes; bits 4-7 must remain 0 in default state.
    // Source: deskhpsdr/src/new_protocol.c:1492-1502 [@120188f]
    void byte50UpperBits_unaffectedByMicTipRing() {
        P2RadioConnection conn;
        conn.setMicTipRing(false);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        // Bits 4-7 must be 0 (only bit 3 set).
        QCOMPARE(int(buf[50] & 0xF0), 0);
    }

    // ── 7. Idempotent: setMicTipRing(false) twice → bit remains 1 ────────────
    // Repeated calls with the same value must not clear the bit.
    void idempotency_setMicTipRingFalseTwice_bitStays1() {
        P2RadioConnection conn;
        conn.setMicTipRing(false);
        conn.setMicTipRing(false);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x08), 0x08);
    }

    // ── 8. Codec path (OrionMKII): mic_trs bit lands at byte 50 bit 3 ────────
    // setBoardForTest(OrionMKII) installs P2CodecOrionMkII which reads
    // ctx.p2MicControl for byte 50.  setMicTipRing must set the same bit
    // in m_mic.micControl so the codec path agrees with the legacy path.
    // Source: deskhpsdr/src/new_protocol.c:1492-1494 [@120188f]
    void codecPath_orionMkII_micTipRingBitLandsAtByte50Bit3() {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::OrionMKII);

        conn.setMicTipRing(false);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x08), 0x08);

        conn.setMicTipRing(true);
        memset(buf, 0, sizeof(buf));
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x08), 0);
    }

    // ── 9. Codec path (Saturn/ANAN-G2): same mic_trs behaviour ──────────────
    void codecPath_saturn_micTipRingBitLandsAtByte50Bit3() {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);

        conn.setMicTipRing(false);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x08), 0x08);

        conn.setMicTipRing(true);
        memset(buf, 0, sizeof(buf));
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x08), 0);
    }

    // ── 10. Byte 51 (line_in gain) unaffected by setMicTipRing ──────────────
    // Byte 51 = line_in gain (m_mic.lineInGain).  Must stay at its
    // default-encoded value when only the mic_trs bit changes.
    void byte51_unaffectedByMicTipRing() {
        P2RadioConnection conn;
        conn.setMicTipRing(false);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        // Compare byte 51 with default state (no tip_ring set).
        quint8 buf_off[60] = {};
        P2RadioConnection conn2;
        conn2.composeCmdTxForTest(buf_off);
        QCOMPARE(int(buf[51]), int(buf_off[51]));
    }
};

QTEST_APPLESS_MAIN(TestP2MicTipRingWire)
#include "tst_p2_mic_tip_ring_wire.moc"
