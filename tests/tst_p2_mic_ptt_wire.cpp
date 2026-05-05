// no-port-check: test-only — deskhpsdr file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No deskhpsdr logic is ported here; this file is NereusSDR-original.
//
// Wire-byte snapshot tests for P2RadioConnection::setMicPTTDisabled() (3M-1b
// Task G.5; renamed from setMicPTT for issue #182 to match Thetis storage
// name MicPTTDisabled / mic_ptt_disabled).
//
// mic_ptt bit position: CmdTx buffer byte 50, bit 2 (mask 0x04).
// Direct polarity (matches Thetis byte-for-byte):
//   setMicPTTDisabled(false) → bit 2 CLEAR (PTT enabled at firmware, default)
//   setMicPTTDisabled(true)  → bit 2 SET   (PTT disabled at firmware)
//
// Source cite:
//   Thetis console.cs:19757-19766 [v2.10.3.13+501e3f51]
//     private bool mic_ptt_disabled = false;        // default PTT enabled
//     public bool MicPTTDisabled {
//         set {
//             mic_ptt_disabled = value;
//             NetworkIO.SetMicPTT(Convert.ToInt32(value));
//         }
//     }
//   deskhpsdr/src/new_protocol.c:1488-1490 [@120188f]
//     if (mic_ptt_enabled == 0) { transmit_specific_buffer[50] |= 0x04; }
//   (deskhpsdr's mic_ptt_enabled is the inverse of mic_ptt_disabled; both
//    collapse to the same wire-bit-set-when-disabled convention.)
//
// CmdTx buffer layout (60 bytes, relevant bytes only):
//   byte 50: mic control byte (m_mic.micControl)
//             bit 0 (0x01): line_in       (G.2)
//             bit 1 (0x02): mic_boost     (G.1)
//             bit 2 (0x04): mic_ptt_disabled (direct, issue #182)
//             bit 3 (0x08): mic_ptt_tip_bias_ring (INVERTED — G.3)
//             bit 4 (0x10): mic_bias      (G.4)
//             bit 5 (0x20): mic_xlr (Saturn/XLR only)
//   byte 51: line_in gain
//   bytes 57-59: TX step attenuators
//
// Test seam: composeCmdTxForTest() in P2RadioConnection.h (NEREUS_BUILD_TESTS)
// exposes the CmdTx buffer composition without needing a live socket.
#include <QtTest/QtTest>
#include "core/P2RadioConnection.h"

using namespace NereusSDR;

class TestP2MicPttWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: byte 50 bit 2 is CLEAR (PTT enabled at firmware) ───
    // m_micPTTDisabled defaults false (matches Thetis console.cs:19757), so
    // CmdTx byte 50 bit 2 must be 0 (firmware honors mic-jack PTT).
    //
    // Pre-fix (issue #182): MicState::micControl{0x24} shipped with bit 2 SET
    // out of the box, which orphaned the mic-jack PTT line on every Protocol 2
    // OrionMKII / Saturn family board. Default now matches Thetis exactly.
    void defaultState_byte50Bit2IsClear() {
        P2RadioConnection conn;
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x04), 0);
    }

    // ── 2. setMicPTTDisabled(true) → byte 50 bit 2 SET ───────────────────────
    // disabled=true → wire bit 2 = 1 (firmware ignores mic-jack PTT line).
    // Source: Thetis console.cs:19764 [v2.10.3.13+501e3f51]
    //   NetworkIO.SetMicPTT(Convert.ToInt32(value));   // value=true → 1
    void setMicPTTDisabledTrue_byte50Bit2IsSet() {
        P2RadioConnection conn;
        conn.setMicPTTDisabled(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x04), 0x04);
    }

    // ── 3. setMicPTTDisabled(false) → byte 50 bit 2 CLEAR ────────────────────
    // disabled=false → wire bit 2 = 0 (firmware honors mic-jack PTT line).
    void setMicPTTDisabledFalse_byte50Bit2IsClear() {
        P2RadioConnection conn;
        conn.setMicPTTDisabled(true);   // disable first
        conn.setMicPTTDisabled(false);  // then re-enable
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x04), 0);
    }

    // ── 4. Round-trip: false → true → false ──────────────────────────────────
    // Now that micControl{0x20} matches m_micPTTDisabled{false} (issue #182),
    // the default starts in a consistent "PTT enabled" wire state.
    void roundTrip_falseToTrueToFalse() {
        P2RadioConnection conn;

        // Start from default. Bit 2 should be clear.
        quint8 buf0[60] = {};
        conn.composeCmdTxForTest(buf0);
        QCOMPARE(int(buf0[50] & 0x04), 0);

        conn.setMicPTTDisabled(true);
        quint8 buf1[60] = {};
        conn.composeCmdTxForTest(buf1);
        QCOMPARE(int(buf1[50] & 0x04), 0x04);

        conn.setMicPTTDisabled(false);
        quint8 buf2[60] = {};
        conn.composeCmdTxForTest(buf2);
        QCOMPARE(int(buf2[50] & 0x04), 0);
    }

    // ── 5. setMicPTTDisabled(true) does NOT touch byte-50 bits 0-1 ──────────
    // Cross-bit guard: mic_ptt (bit 2 = 0x04) must not collide with
    // line_in (bit 0 = 0x01) or mic_boost (bit 1 = 0x02).
    void micBoostAndLineInBits_unaffectedByMicPTTDisabled() {
        P2RadioConnection conn;
        conn.setMicPTTDisabled(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x04), 0x04);
        QCOMPARE(int(buf[50] & 0x01), 0);
        QCOMPARE(int(buf[50] & 0x02), 0);
    }

    // ── 6. setMicPTTDisabled(true) does NOT touch byte-50 bit 3 (G.3) ────────
    // Cross-bit guard for G.3: mic_trs (bit 3 = 0x08) must stay at its
    // default value when only mic_ptt changes.
    void setMicPTTDisabledTrue_doesNotTouchBit3() {
        P2RadioConnection conn;
        conn.setMicPTTDisabled(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        // Bit 3 (mic_ptt_tip_bias_ring): m_micTipRing=true default → inverted → 0.
        QCOMPARE(int(buf[50] & 0x08), 0);
    }

    // ── 7. Idempotent: setMicPTTDisabled(true) twice → bit stays 1 ──────────
    void idempotency_setMicPTTDisabledTrueTwice_bitStays1() {
        P2RadioConnection conn;
        conn.setMicPTTDisabled(true);
        conn.setMicPTTDisabled(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x04), 0x04);
    }

    // ── 8. Codec path (OrionMKII): mic_ptt bit lands at byte 50 bit 2 ────────
    // setBoardForTest(OrionMKII) installs P2CodecOrionMkII which reads
    // ctx.p2MicControl for byte 50.  setMicPTTDisabled must set the same bit
    // in m_mic.micControl so the codec path agrees with the legacy path.
    void codecPath_orionMkII_micPttBitLandsAtByte50Bit2() {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::OrionMKII);

        conn.setMicPTTDisabled(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x04), 0x04);

        conn.setMicPTTDisabled(false);
        memset(buf, 0, sizeof(buf));
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x04), 0);
    }

    // ── 9. Codec path (Saturn/ANAN-G2): same mic_ptt behaviour ──────────────
    void codecPath_saturn_micPttBitLandsAtByte50Bit2() {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);

        conn.setMicPTTDisabled(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x04), 0x04);

        conn.setMicPTTDisabled(false);
        memset(buf, 0, sizeof(buf));
        conn.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x04), 0);
    }

    // ── 10. Byte 51 (line_in gain) unaffected by setMicPTTDisabled ──────────
    // Byte 51 = line_in gain (m_mic.lineInGain). Must stay at its
    // default-encoded value when only the mic_ptt bit changes.
    void byte51_unaffectedByMicPTTDisabled() {
        P2RadioConnection conn;
        conn.setMicPTTDisabled(true);
        quint8 buf[60] = {};
        conn.composeCmdTxForTest(buf);
        // Compare byte 51 with default state (no mic_ptt change).
        quint8 buf_off[60] = {};
        P2RadioConnection conn2;
        conn2.composeCmdTxForTest(buf_off);
        QCOMPARE(int(buf[51]), int(buf_off[51]));
    }
};

QTEST_APPLESS_MAIN(TestP2MicPttWire)
#include "tst_p2_mic_ptt_wire.moc"
