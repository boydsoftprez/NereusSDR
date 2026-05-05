// no-port-check: test-only — deskhpsdr file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No deskhpsdr logic is ported here; this file is NereusSDR-original.
//
// Wire-byte snapshot tests for P2RadioConnection::setMicBoost() (3M-1b Task G.1).
//
// mic_boost bit position: CmdTx buffer byte 50, bit 1 (mask 0x02).
// Semantic: 1 = boost on (no polarity inversion).
//
// Source cite:
//   deskhpsdr/src/new_protocol.c:1484-1486 [@120188f]
//     if (mic_boost) {
//       transmit_specific_buffer[50] |= 0x02;
//     }
//
// Note: P2 bit position (bit 1 = 0x02) differs from P1 bit position
// (bit 0 = 0x01 in C2 of bank 10). Both mean "boost on = 1".
//
// CmdTx buffer layout (60 bytes, relevant bytes only):
//   byte  4: numDac
//   byte 50: mic control byte (m_mic.micControl)
//             bit 0 (0x01): line_in
//             bit 1 (0x02): mic_boost  ← G.1
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

class TestP2MicBoostWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: byte 50 bit 1 is clear ─────────────────────────
    // A freshly constructed P2RadioConnection has m_micBoost = false, so
    // CmdTx byte 50 bit 1 must be 0.
    // Source: deskhpsdr/src/new_protocol.c:1480 [@120188f]
    //   transmit_specific_buffer[50] = 0;  // cleared before setting bits
    void defaultState_byte50Bit1IsClear() {
        P2RadioConnection conn;
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x02), 0);
    }

    // ── 2. setMicBoost(true) → byte 50 bit 1 set ─────────────────────────
    // After setMicBoost(true), CmdTx byte 50 bit 1 must be 1.
    // Source: deskhpsdr/src/new_protocol.c:1484-1486 [@120188f]
    //   if (mic_boost) { transmit_specific_buffer[50] |= 0x02; }
    void setMicBoostTrue_byte50Bit1IsSet() {
        P2RadioConnection conn;
        conn.setMicBoost(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x02), 0x02);
    }

    // ── 3. setMicBoost(false) → byte 50 bit 1 cleared ────────────────────
    void setMicBoostFalse_byte50Bit1IsCleared() {
        P2RadioConnection conn;
        conn.setMicBoost(true);
        conn.setMicBoost(false);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x02), 0);
    }

    // ── 4. Round-trip: true → false → true ───────────────────────────────
    void roundTrip_trueToFalseToTrue() {
        P2RadioConnection conn;

        conn.setMicBoost(true);
        quint8 buf1[60] = {};
        conn.composeCmdTxForTest(buf1);
        QCOMPARE(int(buf1[50] & 0x02), 0x02);

        conn.setMicBoost(false);
        quint8 buf2[60] = {};
        conn.composeCmdTxForTest(buf2);
        QCOMPARE(int(buf2[50] & 0x02), 0);

        conn.setMicBoost(true);
        quint8 buf3[60] = {};
        conn.composeCmdTxForTest(buf3);
        QCOMPARE(int(buf3[50] & 0x02), 0x02);
    }

    // ── 5. Bit 0 (line_in) unaffected by mic_boost ───────────────────────
    // m_mic.micControl bit 0 = line_in; it must stay 0 when only mic_boost
    // changes.
    // Source: deskhpsdr/src/new_protocol.c:1480-1481 [@120188f]
    //   if (mic_linein) { transmit_specific_buffer[50] |= 0x01; }
    void lineInBit_unaffectedByMicBoost() {
        P2RadioConnection conn;
        conn.setMicBoost(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        // Bit 0 (line_in) must be 0 — not set by setMicBoost.
        QCOMPARE(int(buf[50] & 0x01), 0);
        // Bit 1 (mic_boost) must be 1.
        QCOMPARE(int(buf[50] & 0x02), 0x02);
    }

    // ── 6. Bit 5 (G.6) set by default; bit 2 (G.5, post issue #182) clear; ──
    //      bits 3-4,6-7 unaffected.
    // Post issue #182: bit 2 (0x04) CLEAR by default (m_micPTTDisabled=false →
    //   PTT enabled at firmware on wire; matches Thetis console.cs:19757
    //   [v2.10.3.13+501e3f51] private bool mic_ptt_disabled = false).
    // After G.6: bit 5 (0x20) SET by default (m_micXlr=true → XLR selected).
    // setMicBoost must not change bits 2, 5, or 3-4, 6-7.
    // Source: deskhpsdr/src/new_protocol.c:1484-1486 [@120188f]
    void byte50UpperBits_unaffectedByMicBoost() {
        P2RadioConnection conn;
        conn.setMicBoost(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        // Post issue #182: only bit 5 (0x20) is set by default.
        // Bits 2,3,4,6,7 (0xDC) must be 0 — not set by setMicBoost or defaults.
        QCOMPARE(int(buf[50] & 0xDC), 0);
        QCOMPARE(int(buf[50] & 0x04), 0);
        QCOMPARE(int(buf[50] & 0x20), 0x20);
    }

    // ── 7. Idempotent: setMicBoost(true) twice → bit remains 1 ───────────
    // Repeated calls with the same value must not clear the bit.
    void idempotency_setMicBoostTrueTwice_bitStays1() {
        P2RadioConnection conn;
        conn.setMicBoost(true);
        conn.setMicBoost(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x02), 0x02);
    }

    // ── 8. Codec path (OrionMKII): mic_boost bit lands at byte 50 bit 1 ──
    // setBoardForTest(OrionMKII) installs P2CodecOrionMkII which reads
    // ctx.p2MicControl for byte 50.  setMicBoost must set the same bit
    // in m_mic.micControl so the codec path agrees with the legacy path.
    // Source: deskhpsdr/src/new_protocol.c:1484-1486 [@120188f]
    void codecPath_orionMkII_micBoostBitLandsAtByte50Bit1() {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::OrionMKII);

        conn.setMicBoost(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x02), 0x02);

        conn.setMicBoost(false);
        memset(buf, 0, sizeof(buf));
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x02), 0);
    }

    // ── 9. Codec path (Saturn/ANAN-G2): same mic_boost behaviour ─────────
    // Saturn is the primary target board for P2.  Verify identical behaviour.
    void codecPath_saturn_micBoostBitLandsAtByte50Bit1() {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);

        conn.setMicBoost(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x02), 0x02);

        conn.setMicBoost(false);
        memset(buf, 0, sizeof(buf));
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x02), 0);
    }

    // ── 10. Byte 51 (line_in gain) unaffected by setMicBoost ─────────────
    // Byte 51 = line_in gain (m_mic.lineInGain).  Must stay 0 when only
    // mic_boost changes.
    // Source: deskhpsdr/src/new_protocol.c:1502 [@120188f]
    //   transmit_specific_buffer[51] = (int)((linein_gain + 34.0) * 0.6739 + 0.5);
    void byte51_unaffectedByMicBoost() {
        P2RadioConnection conn;
        conn.setMicBoost(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        // lineInGain = 0 (default) → byte 51 should be the encoded default value.
        // At lineInGain = 0: (0 + 34.0) * 0.6739 + 0.5 = 23.41 → 23 (0x17).
        // We only assert byte 51 is unaffected by mic_boost (no change from default).
        // Default state test: byte 51 value with mic_boost=false vs mic_boost=true.
        quint8 buf_off[60] = {};
        P2RadioConnection conn2;
        conn2.composeCmdTxForTest(buf_off);
        QCOMPARE(int(buf[51]), int(buf_off[51]));
    }
};

QTEST_APPLESS_MAIN(TestP2MicBoostWire)
#include "tst_p2_mic_boost_wire.moc"
