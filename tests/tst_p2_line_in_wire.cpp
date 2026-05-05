// no-port-check: test-only — deskhpsdr file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No deskhpsdr logic is ported here; this file is NereusSDR-original.
//
// Wire-byte snapshot tests for P2RadioConnection::setLineIn() (3M-1b Task G.2).
//
// line_in bit position: CmdTx buffer byte 50, bit 0 (mask 0x01).
// Semantic: 1 = line in active (no polarity inversion).
//
// Source cite:
//   deskhpsdr/src/new_protocol.c:1480-1482 [@120188f]
//     if (mic_linein) {
//       transmit_specific_buffer[50] |= 0x01;
//     }
//
// Note: P2 bit position (bit 0 = 0x01) differs from P1 bit position
// (bit 1 = 0x02 in C2 of bank 10). Both mean "line in active = 1".
//
// CmdTx buffer layout (60 bytes, relevant bytes only):
//   byte  4: numDac
//   byte 50: mic control byte (m_mic.micControl)
//             bit 0 (0x01): line_in  ← G.2
//             bit 1 (0x02): mic_boost
//             bit 2 (0x04): mic_ptt_disabled (inverted — future G task)
//             bit 3 (0x08): mic_tip_ring
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

class TestP2LineInWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: byte 50 bit 0 is clear ─────────────────────────
    // A freshly constructed P2RadioConnection has m_lineIn = false, so
    // CmdTx byte 50 bit 0 must be 0.
    // Source: deskhpsdr/src/new_protocol.c:1478 [@120188f]
    //   transmit_specific_buffer[50] = 0;  // cleared before setting bits
    void defaultState_byte50Bit0IsClear() {
        P2RadioConnection conn;
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x01), 0);
    }

    // ── 2. setLineIn(true) → byte 50 bit 0 set ───────────────────────────
    // After setLineIn(true), CmdTx byte 50 bit 0 must be 1.
    // Source: deskhpsdr/src/new_protocol.c:1480-1482 [@120188f]
    //   if (mic_linein) { transmit_specific_buffer[50] |= 0x01; }
    void setLineInTrue_byte50Bit0IsSet() {
        P2RadioConnection conn;
        conn.setLineIn(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x01), 0x01);
    }

    // ── 3. setLineIn(false) → byte 50 bit 0 cleared ──────────────────────
    void setLineInFalse_byte50Bit0IsCleared() {
        P2RadioConnection conn;
        conn.setLineIn(true);
        conn.setLineIn(false);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x01), 0);
    }

    // ── 4. Round-trip: true → false → true ───────────────────────────────
    void roundTrip_trueToFalseToTrue() {
        P2RadioConnection conn;

        conn.setLineIn(true);
        quint8 buf1[60] = {};
        conn.composeCmdTxForTest(buf1);
        QCOMPARE(int(buf1[50] & 0x01), 0x01);

        conn.setLineIn(false);
        quint8 buf2[60] = {};
        conn.composeCmdTxForTest(buf2);
        QCOMPARE(int(buf2[50] & 0x01), 0);

        conn.setLineIn(true);
        quint8 buf3[60] = {};
        conn.composeCmdTxForTest(buf3);
        QCOMPARE(int(buf3[50] & 0x01), 0x01);
    }

    // ── 5. setLineIn(true) does NOT touch byte-50 bit 1 (mic_boost) ──────
    // Cross-bit guard: line_in (bit 0 = 0x01) must not collide with
    // mic_boost (bit 1 = 0x02). Both bits are independent.
    // Source: deskhpsdr/src/new_protocol.c:1480-1486 [@120188f]
    void micBoostBit_unaffectedByLineIn() {
        P2RadioConnection conn;
        conn.setLineIn(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        // Bit 0 (line_in) must be 1.
        QCOMPARE(int(buf[50] & 0x01), 0x01);
        // Bit 1 (mic_boost) must be 0 — not set by setLineIn.
        QCOMPARE(int(buf[50] & 0x02), 0);
    }

    // ── 6. Bit 5 (G.6) set by default; bit 2 (G.5, post issue #182) clear; ──
    //      bits 3-4,6-7 unaffected.
    // Post issue #182: bit 2 (0x04) CLEAR by default (m_micPTTDisabled=false →
    //   PTT enabled at firmware on wire; matches Thetis console.cs:19757
    //   [v2.10.3.13+501e3f51] private bool mic_ptt_disabled = false).
    // After G.6: bit 5 (0x20) SET by default (m_micXlr=true → XLR selected).
    // setLineIn must not change bits 2, 5, or 3-4, 6-7.
    // Source: deskhpsdr/src/new_protocol.c:1480-1482 [@120188f]
    void byte50UpperBits_unaffectedByLineIn() {
        P2RadioConnection conn;
        conn.setLineIn(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        // Post issue #182: only bit 5 (0x20) is set by default.
        // Bits 2,3,4,6,7 (0xDC) must be 0 — not set by setLineIn or defaults.
        QCOMPARE(int(buf[50] & 0xDC), 0);
        QCOMPARE(int(buf[50] & 0x04), 0);
        QCOMPARE(int(buf[50] & 0x20), 0x20);
    }

    // ── 7. Idempotent: setLineIn(true) twice → bit remains 1 ─────────────
    // Repeated calls with the same value must not clear the bit.
    void idempotency_setLineInTrueTwice_bitStays1() {
        P2RadioConnection conn;
        conn.setLineIn(true);
        conn.setLineIn(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x01), 0x01);
    }

    // ── 8. Codec path (OrionMKII): line_in bit lands at byte 50 bit 0 ────
    // setBoardForTest(OrionMKII) installs P2CodecOrionMkII which reads
    // ctx.p2MicControl for byte 50.  setLineIn must set the same bit
    // in m_mic.micControl so the codec path agrees with the legacy path.
    // Source: deskhpsdr/src/new_protocol.c:1480-1482 [@120188f]
    void codecPath_orionMkII_lineInBitLandsAtByte50Bit0() {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::OrionMKII);

        conn.setLineIn(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x01), 0x01);

        conn.setLineIn(false);
        memset(buf, 0, sizeof(buf));
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x01), 0);
    }

    // ── 9. Codec path (Saturn/ANAN-G2): same line_in behaviour ───────────
    // Saturn is the primary target board for P2.  Verify identical behaviour.
    void codecPath_saturn_lineInBitLandsAtByte50Bit0() {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);

        conn.setLineIn(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x01), 0x01);

        conn.setLineIn(false);
        memset(buf, 0, sizeof(buf));
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x01), 0);
    }

    // ── 10. Byte 51 (line_in gain) unaffected by setLineIn ───────────────
    // Byte 51 = line_in gain (m_mic.lineInGain).  Must stay at its
    // default-encoded value when only the line_in enable bit changes.
    // Source: deskhpsdr/src/new_protocol.c:1502 [@120188f]
    //   transmit_specific_buffer[51] = (int)((linein_gain + 34.0) * 0.6739 + 0.5);
    void byte51_unaffectedByLineIn() {
        P2RadioConnection conn;
        conn.setLineIn(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        // Compare byte 51 with default state (no line_in).
        quint8 buf_off[60] = {};
        P2RadioConnection conn2;
        conn2.composeCmdTxForTest(buf_off);
        QCOMPARE(int(buf[51]), int(buf_off[51]));
    }
};

QTEST_APPLESS_MAIN(TestP2LineInWire)
#include "tst_p2_line_in_wire.moc"
