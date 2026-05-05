// no-port-check: test-only — deskhpsdr file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No deskhpsdr logic is ported here; this file is NereusSDR-original.
//
// Wire-byte snapshot tests for P2RadioConnection::setMicXlr() (3M-1b Task G.6).
//
// mic_xlr bit position: CmdTx buffer byte 50, bit 5 (mask 0x20).
// POLARITY: 1 = XLR jack selected (no inversion — parameter maps directly to wire bit).
// Default true — Saturn G2 ships with XLR-enabled config (m_micXlr{true}).
//
// Source cite:
//   deskhpsdr/src/new_protocol.c:1500-1502 [@120188f]
//     if (mic_input_xlr) {
//       transmit_specific_buffer[50] |= 0x20;
//     }
//     // Saturn G2 only
//
//   P1 implementation is STORAGE-ONLY — see tst_p1_mic_xlr_storage.cpp.
//   P1 case-10 and case-11 C&C bytes are UNCHANGED regardless of m_micXlr value.
//   "Saturn G2 P2-only feature; P1 hardware has no XLR jack."
//
// CmdTx buffer layout (60 bytes, relevant bytes only):
//   byte 50: mic control byte (m_mic.micControl)
//             bit 0 (0x01): line_in       (G.2)
//             bit 1 (0x02): mic_boost     (G.1)
//             bit 2 (0x04): mic_ptt_disabled (INVERTED — G.5)
//             bit 3 (0x08): mic_ptt_tip_bias_ring (INVERTED — G.3)
//             bit 4 (0x10): mic_bias      (G.4)
//             bit 5 (0x20): mic_xlr (Saturn/XLR only — G.6, this test)
//   byte 51: line_in gain
//   bytes 57-59: TX step attenuators
//
// MicState::micControl default 0x24:
//   bit 2 (0x04): PTT disabled (m_micPTT=false, polarity-inverted → wire bit 1)
//   bit 5 (0x20): XLR selected (m_micXlr=true, no inversion → wire bit 1)
//   Updated from 0x04 in G.6 — reflects both defaults at construction.
//
// Test seam: composeCmdTxForTest() in P2RadioConnection.h (NEREUS_BUILD_TESTS)
// exposes the CmdTx buffer composition without needing a live socket.
#include <QtTest/QtTest>
#include "core/P2RadioConnection.h"

using namespace NereusSDR;

class TestP2MicXlrWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: byte 50 bit 5 is SET (XLR selected by default) ─────
    // m_micXlr defaults true → wire bit 5 = 1 (XLR jack selected).
    // No inversion: true → 1 on wire.
    // MicState::micControl default is 0x24 (bit 2 + bit 5 both set).
    // Source: deskhpsdr/src/new_protocol.c:1500-1502 [@120188f]
    void defaultState_byte50Bit5IsSet() {
        P2RadioConnection conn;
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        // Default m_micXlr=true → wire bit 5 = 1 (XLR selected).
        QCOMPARE(int(buf[50] & 0x20), 0x20);
    }

    // ── 2. setMicXlr(true) → byte 50 bit 5 set ───────────────────────────────
    // xlrJack=true → XLR selected → bit 5 = 1.
    // Source: deskhpsdr/src/new_protocol.c:1500-1502 [@120188f]
    //   if (mic_input_xlr) { transmit_specific_buffer[50] |= 0x20; }
    void setMicXlrTrue_byte50Bit5IsSet() {
        P2RadioConnection conn;
        conn.setMicXlr(false);  // first clear
        conn.setMicXlr(true);   // then re-enable
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x20), 0x20);
    }

    // ── 3. setMicXlr(false) → byte 50 bit 5 clear ────────────────────────────
    void setMicXlrFalse_byte50Bit5IsClear() {
        P2RadioConnection conn;
        conn.setMicXlr(false);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x20), 0);
    }

    // ── 4. Round-trip: false → true ───────────────────────────────────────────
    void roundTrip_falseToTrue_bitSetsAgain() {
        P2RadioConnection conn;

        conn.setMicXlr(false);
        quint8 buf1[60] = {};
        conn.composeCmdTxForTest(buf1);
        QCOMPARE(int(buf1[50] & 0x20), 0);

        conn.setMicXlr(true);
        quint8 buf2[60] = {};
        conn.composeCmdTxForTest(buf2);
        QCOMPARE(int(buf2[50] & 0x20), 0x20);
    }

    // ── 5. Cross-bit guard: setMicXlr(true) does NOT touch bits 0-4 ─────────
    // Bit 5 (0x20) must not collide with G.1-G.5 bits (0x1F).
    // All five lower bits must remain at their default state.
    // G.1 (mic_boost=false → 0), G.2 (line_in=false → 0),
    // G.5 (mic_ptt_disabled=false → bit 2 CLEAR — micControl{0x20} default
    //   post issue #182), G.3 (tip-ring=true → bit 3 CLEAR),
    // G.4 (mic_bias=false → bit 4 CLEAR).
    // Source: deskhpsdr/src/new_protocol.c:1480-1500 [@120188f]
    void setMicXlrTrue_doesNotTouchBits0to4() {
        P2RadioConnection conn;
        conn.setMicXlr(false);  // clear XLR to isolate lower bits
        conn.setMicXlr(true);   // set XLR — lower bits must stay unchanged
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x20), 0x20);
        QCOMPARE(int(buf[50] & 0x02), 0);
        QCOMPARE(int(buf[50] & 0x01), 0);
        // Bit 2 (mic_ptt_disabled, default false → PTT enabled) must be clear.
        QCOMPARE(int(buf[50] & 0x04), 0);
        QCOMPARE(int(buf[50] & 0x08), 0);
        QCOMPARE(int(buf[50] & 0x10), 0);
    }

    // ── 6. Cross-bit guard: setMicXlr(false) does NOT touch bits 0-4 ────────
    void setMicXlrFalse_doesNotTouchBits0to4() {
        P2RadioConnection conn;
        conn.setMicXlr(false);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x20), 0);
        // Bit 2 (mic_ptt_disabled, default false → PTT enabled) must be clear.
        QCOMPARE(int(buf[50] & 0x04), 0);
        QCOMPARE(int(buf[50] & 0x01), 0);
        QCOMPARE(int(buf[50] & 0x02), 0);
        QCOMPARE(int(buf[50] & 0x08), 0);
        QCOMPARE(int(buf[50] & 0x10), 0);
    }

    // ── 7. Idempotent: setMicXlr(true) twice → bit stays 1 ──────────────────
    void idempotency_setMicXlrTrueTwice_bitStays1() {
        P2RadioConnection conn;
        conn.setMicXlr(true);   // re-set the already-true default
        conn.setMicXlr(true);   // second call — idempotent guard fires
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x20), 0x20);
    }

    // ── 8. Codec path (OrionMKII): mic_xlr bit lands at byte 50 bit 5 ────────
    // setBoardForTest(OrionMKII) installs P2CodecOrionMkII which reads
    // ctx.p2MicControl for byte 50. setMicXlr must set the same bit
    // in m_mic.micControl so the codec path agrees with the legacy path.
    // Source: deskhpsdr/src/new_protocol.c:1500-1502 [@120188f]
    void codecPath_orionMkII_micXlrBitLandsAtByte50Bit5() {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::OrionMKII);

        conn.setMicXlr(false);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x20), 0);

        conn.setMicXlr(true);
        memset(buf, 0, sizeof(buf));
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x20), 0x20);
    }

    // ── 9. Codec path (Saturn/ANAN-G2): same mic_xlr behaviour ─────────────
    // P2-only feature is on the Saturn-family hardware; codec path must
    // also show the correct bit.
    // Source: deskhpsdr/src/new_protocol.c:1500-1502 [@120188f] — Saturn G2 only.
    void codecPath_saturn_micXlrBitLandsAtByte50Bit5() {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);

        conn.setMicXlr(false);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x20), 0);

        conn.setMicXlr(true);
        memset(buf, 0, sizeof(buf));
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x20), 0x20);
    }

    // ── 10. Byte 51 (line_in gain) unaffected by setMicXlr ──────────────────
    // Byte 51 = line_in gain (m_mic.lineInGain). Must stay at its
    // default-encoded value when only the mic_xlr bit changes.
    void byte51_unaffectedByMicXlr() {
        P2RadioConnection conn;
        conn.setMicXlr(false);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        // Compare byte 51 with default state (no xlr change).
        quint8 buf_default[60] = {};
        P2RadioConnection conn2;
        conn2.composeCmdTxForTest(buf_default);
        QCOMPARE(int(buf[51]), int(buf_default[51]));
    }
};

QTEST_APPLESS_MAIN(TestP2MicXlrWire)
#include "tst_p2_mic_xlr_wire.moc"
