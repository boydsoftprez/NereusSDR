// no-port-check: test-only — deskhpsdr file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No deskhpsdr logic is ported here; this file is NereusSDR-original.
//
// Wire-byte snapshot tests for P2RadioConnection::setMicBias() (3M-1b Task G.4).
//
// mic_bias_enabled bit position: CmdTx buffer byte 50, bit 4 (mask 0x10).
// Polarity: 1 = bias on (no inversion — parameter maps directly to wire bit).
//
// Source cite:
//   deskhpsdr/src/new_protocol.c:1496-1498 [@120188f]
//     if (mic_bias_enabled) {
//       transmit_specific_buffer[50] |= 0x10;
//     }
//
// Note: P2 bit position (bit 4 = 0x10) differs from P1 bit position
// (bit 5 = 0x20 in C1 of bank 11). Both carry polarity 1=on (no inversion).
//
// CmdTx buffer layout (60 bytes, relevant bytes only):
//   byte 50: mic control byte (m_mic.micControl)
//             bit 0 (0x01): line_in  (G.2)
//             bit 1 (0x02): mic_boost (G.1)
//             bit 2 (0x04): mic_ptt_disabled (inverted — future G task)
//             bit 3 (0x08): mic_ptt_tip_bias_ring (INVERTED — G.3)
//             bit 4 (0x10): mic_bias (G.4 — no inversion)
//             bit 5 (0x20): mic_xlr (Saturn/XLR only)
//   byte 51: line_in gain
//
// Test seam: composeCmdTxForTest() in P2RadioConnection.h (NEREUS_BUILD_TESTS)
// exposes the CmdTx buffer composition without needing a live socket.
#include <QtTest/QtTest>
#include "core/P2RadioConnection.h"

using namespace NereusSDR;

class TestP2MicBiasWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: byte 50 bit 4 is CLEAR (bias off by default) ───────
    // m_micBias defaults false (bias off), so CmdTx byte 50 bit 4 must be 0.
    // Source: deskhpsdr/src/new_protocol.c:1496-1498 [@120188f]
    void defaultState_byte50Bit4IsClear() {
        P2RadioConnection conn;
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x10), 0);
    }

    // ── 2. setMicBias(true) [bias on] → byte 50 bit 4 SET ────────────────────
    // on = true → bias enabled → mic_bias_enabled = 1 → bit 4 = 1.
    // Source: deskhpsdr/src/new_protocol.c:1496-1498 [@120188f]
    //   if (mic_bias_enabled) { transmit_specific_buffer[50] |= 0x10; }
    void setMicBiasTrue_byte50Bit4IsSet() {
        P2RadioConnection conn;
        conn.setMicBias(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x10), 0x10);
    }

    // ── 3. setMicBias(false) [bias off] → byte 50 bit 4 clear ────────────────
    void setMicBiasFalse_byte50Bit4IsClear() {
        P2RadioConnection conn;
        conn.setMicBias(true);   // set first
        conn.setMicBias(false);  // then clear
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x10), 0);
    }

    // ── 4. Round-trip: true → false → true ───────────────────────────────────
    void roundTrip_trueFalseTrue() {
        P2RadioConnection conn;

        conn.setMicBias(true);
        quint8 buf1[60] = {};
        conn.composeCmdTxForTest(buf1);
        QCOMPARE(int(buf1[50] & 0x10), 0x10);

        conn.setMicBias(false);
        quint8 buf2[60] = {};
        conn.composeCmdTxForTest(buf2);
        QCOMPARE(int(buf2[50] & 0x10), 0);

        conn.setMicBias(true);
        quint8 buf3[60] = {};
        conn.composeCmdTxForTest(buf3);
        QCOMPARE(int(buf3[50] & 0x10), 0x10);
    }

    // ── 5. setMicBias(true) does NOT touch byte-50 bits 0-3 (G.1/G.2/G.3) ───
    // Cross-bit guard: mic_bias (bit 4 = 0x10) must not collide with
    // line_in (bit 0 = 0x01), mic_boost (bit 1 = 0x02), or
    // mic_trs (bit 3 = 0x08, G.3).
    // Source: deskhpsdr/src/new_protocol.c:1480-1498 [@120188f]
    void setMicBiasTrue_doesNotTouchLowerBits() {
        P2RadioConnection conn;
        conn.setMicBias(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        // Bit 4 (mic_bias) must be 1.
        QCOMPARE(int(buf[50] & 0x10), 0x10);
        // Bit 0 (line_in) must be 0; bit 1 (mic_boost) keeps its default.
        QCOMPARE(int(buf[50] & 0x01), 0);
        // mic_boost stays at its default, on (Thetis console.cs:13259 mic_boost = true).
        QCOMPARE(int(buf[50] & 0x02), 0x02);
        // Bit 3 (mic_trs, G.3, default tipHot=true → inverted → 0) must be 0.
        QCOMPARE(int(buf[50] & 0x08), 0);
    }

    // ── 6. Bits 6-7 of byte 50 unaffected by setMicBias; bit 5 is XLR default ─
    // After G.6: bit 5 (0x20) is SET by default (m_micXlr=true, XLR selected).
    // setMicBias must not change bit 5 (G.6 XLR) or bits 6-7.
    // Only bit 4 (mic_bias) changes; bits 6-7 (0xC0) must remain 0.
    // Source: deskhpsdr/src/new_protocol.c:1496-1502 [@120188f]
    void byte50UpperBits_unaffectedByMicBias() {
        P2RadioConnection conn;
        conn.setMicBias(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        // After G.6: bit 5 (0x20) is on by default (XLR selected default).
        // Bits 6-7 (0xC0) must be 0 — not set by setMicBias or G.6 defaults.
        QCOMPARE(int(buf[50] & 0xC0), 0);
        // Bit 5 (mic_xlr, default true) must be set.
        QCOMPARE(int(buf[50] & 0x20), 0x20);
    }

    // ── 7. Idempotent: setMicBias(true) twice → bit remains 1 ────────────────
    // Repeated calls with the same value must not clear the bit.
    void idempotency_setMicBiasTrueTwice_bitStays1() {
        P2RadioConnection conn;
        conn.setMicBias(true);
        conn.setMicBias(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x10), 0x10);
    }

    // ── 8. Codec path (OrionMKII): mic_bias bit lands at byte 50 bit 4 ───────
    // setBoardForTest(OrionMKII) installs P2CodecOrionMkII which reads
    // ctx.p2MicControl for byte 50.  setMicBias must set the same bit
    // in m_mic.micControl so the codec path agrees with the legacy path.
    void codecPath_orionMkII_micBiasBitLandsAtByte50Bit4() {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::OrionMKII);

        conn.setMicBias(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x10), 0x10);

        conn.setMicBias(false);
        memset(buf, 0, sizeof(buf));
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x10), 0);
    }

    // ── 9. Codec path (Saturn/ANAN-G2): same mic_bias behaviour ──────────────
    void codecPath_saturn_micBiasBitLandsAtByte50Bit4() {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);

        conn.setMicBias(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x10), 0x10);

        conn.setMicBias(false);
        memset(buf, 0, sizeof(buf));
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x10), 0);
    }

    // ── 10. Byte 51 (line_in gain) unaffected by setMicBias ─────────────────
    // Byte 51 = line_in gain (m_mic.lineInGain).  Must stay at its
    // default-encoded value when only the mic_bias bit changes.
    void byte51_unaffectedByMicBias() {
        P2RadioConnection conn;
        conn.setMicBias(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        // Compare byte 51 with default state (no mic_bias set).
        quint8 buf_off[60] = {};
        P2RadioConnection conn2;
        conn2.composeCmdTxForTest(buf_off);
        QCOMPARE(int(buf[51]), int(buf_off[51]));
    }
};

QTEST_APPLESS_MAIN(TestP2MicBiasWire)
#include "tst_p2_mic_bias_wire.moc"
