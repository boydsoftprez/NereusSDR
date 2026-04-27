// no-port-check: test-only — deskhpsdr file names and control.v appear
// only in source-cite comments that document which upstream line each assertion
// verifies.  No Thetis or deskhpsdr logic is ported here; this file is
// NereusSDR-original.
//
// Wire-byte snapshot tests for P1RadioConnection::setTrxRelay() (3M-1a Task E.4).
//
// T/R relay bit position: C3 byte (bank 10, frame-bank C0=0x12), bit 7 (0x80).
// Semantic INVERTED vs MOX: 1 = T/R relay DISABLED (PA bypass / RX-only protect),
//                           0 = T/R relay ENGAGED (normal TX path through relay).
//
// Primary source cite:
//   deskhpsdr/src/old_protocol.c:2909-2910 [@120188f]
//     if (txband->disablePA || !pa_enabled) {
//         output_buffer[C3] |= 0x80; // disable Alex T/R relay
//     }
//
// Bank 10 C0 address: 0x12 (see networkproto1.c:578-591 / deskhpsdr case 3).
// Byte layout for bank 10 (C0=0x12):
//   out[0] = 0x12 (C0 — address 0x12 ORed with MOX bit 0)
//   out[1] = TX drive level (0-255)
//   out[2] = mic/line-in flags (default 0x40 in 3M-1a)
//   out[3] = Alex HPF bits | T/R relay disable bit (C3)
//   out[4] = Alex LPF bits (C4)
//
// HL2 note (informational — no HL2 FW runs in tests):
//   HL2 firmware control.v:211-214 decodes cmd_addr==6'h09:
//     pa_enable  <= cmd_data[19];  // bit 19 of 32-bit payload
//     tr_disable <= cmd_data[18];  // bit 18
//   deskhpsdr clears C2/C3/C4 entirely for HL2 (old_protocol.c:2964-2966
//   [@120188f]) and uses C2 bit 3 for PA enable — the Alex T/R relay bit
//   (C3 bit 7) is NOT decoded by HL2 firmware.  These tests exercise the
//   Alex board path only.
//
// Latent bug fixed (3M-1a E.4):
//   Prior to this task, case 10 wrote (m_paEnabled ? 0x80 : 0).  That is
//   INVERTED — it asserted "disable relay" when PA was enabled.  The bug was
//   latent from 3M-0 because no prior task exercised live TX I/Q on EP2.
#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;

class TestP1TrxRelayWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: relay bit is SET (relay disengaged / PA protect) ──
    // A freshly constructed P1RadioConnection has m_trxRelay = false (base
    // class default), so C3 bit 7 must be 1 (relay disabled).
    // Source: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f]
    //   if (txband->disablePA || !pa_enabled)
    //       output_buffer[C3] |= 0x80; // disable Alex T/R relay
    // In the default (no TX) state, PA is not enabled, so bit 7 = 1.
    void defaultState_relayBitIsSet() {
        P1RadioConnection conn;
        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(bank10.size(), 5);
        // C3 = bank10[3].  Bit 7 (0x80) = 1 means relay disabled.
        // Default: m_trxRelay = false → bit 7 should be 1.
        QCOMPARE(int(quint8(bank10[3]) & 0x80), 0x80);
    }

    // ── 2. setTrxRelay(true) → C3 bit 7 CLEARED (relay engaged) ───────────
    // When the relay is engaged (normal TX path), bit 7 must be 0.
    // Source: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f] — the bit is
    // written only when disablePA || !pa_enabled; in normal TX it stays 0.
    void setTrxRelayTrue_c3Bit7IsCleared() {
        P1RadioConnection conn;
        conn.setTrxRelay(true);  // engage relay

        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(bank10.size(), 5);
        // Bit 7 must be 0 (relay engaged, normal TX path).
        QCOMPARE(int(quint8(bank10[3]) & 0x80), 0);
    }

    // ── 3. setTrxRelay(false) → C3 bit 7 SET (relay disengaged) ───────────
    // After setTrxRelay(false) following a setTrxRelay(true), bit 7 must be 1.
    void setTrxRelayFalse_c3Bit7IsSet() {
        P1RadioConnection conn;
        conn.setTrxRelay(true);
        conn.setTrxRelay(false);  // disengage relay

        const QByteArray bank10 = conn.captureBank10ForTest();
        // Bit 7 must be 1 (relay disengaged / PA bypass).
        QCOMPARE(int(quint8(bank10[3]) & 0x80), 0x80);
    }

    // ── 4. Bank 10 C0 address bits must be 0x12 ─────────────────────────────
    // Bank 10's C0 byte carries address 0x12 (bits 7..1).  After setTrxRelay,
    // only bit 7 of C3 changes — C0 (out[0]) must still have address 0x12.
    // Source: networkproto1.c:578-591 / deskhpsdr old_protocol.c:2895 [@120188f]
    //   output_buffer[C0] = 0x12;
    void setTrxRelayTrue_c0AddressIs0x12() {
        P1RadioConnection conn;
        conn.setTrxRelay(true);

        const QByteArray bank10 = conn.captureBank10ForTest();
        // C0 = bank10[0].  Upper bits (7..1) encode address 0x12; bit 0 = MOX.
        // MOX defaults to false, so C0 should be exactly 0x12.
        QCOMPARE(int(quint8(bank10[0]) & 0xFE), 0x12);
    }

    // ── 5. Flush flag: setTrxRelay sets m_forceBank10Next ───────────────────
    // setTrxRelay() must set m_forceBank10Next (Codex P2: safety effect fires
    // BEFORE the idempotent guard, so every call schedules a bank-10 flush).
    void setTrxRelay_setsForceBank10Next() {
        P1RadioConnection conn;

        // Initially the flush flag must be false.
        QCOMPARE(conn.forceBank10NextForTest(), false);

        conn.setTrxRelay(true);
        QCOMPARE(conn.forceBank10NextForTest(), true);
    }

    // ── 6. Repeated call with same value still sets the flush flag ───────────
    // Codex P2: even when the stored value doesn't change, the safety effect
    // (force bank-10 frame) must still fire.
    void setTrxRelayRepeat_stillSetsForceFlag() {
        P1RadioConnection conn;
        conn.setTrxRelay(true);

        // Re-call setTrxRelay(true) — the flag should be set again.
        conn.setTrxRelay(true);
        QCOMPARE(conn.forceBank10NextForTest(), true);
    }

    // ── 7. Idempotency: setTrxRelay(true) twice doesn't crash ───────────────
    // The second call with the same value must not crash or assert.
    void setTrxRelayTwice_doesNotCrash() {
        P1RadioConnection conn;
        conn.setTrxRelay(true);
        conn.setTrxRelay(true);

        const QByteArray bank10 = conn.captureBank10ForTest();
        // Relay engaged → bit 7 = 0.
        QCOMPARE(int(quint8(bank10[3]) & 0x80), 0);
    }

    // ── 8. Round-trip: engaged → disengaged → engaged ───────────────────────
    // Verify that three successive state transitions produce correct bit values.
    void roundTrip_trueToFalseToTrue() {
        P1RadioConnection conn;

        conn.setTrxRelay(true);
        QCOMPARE(int(quint8(conn.captureBank10ForTest()[3]) & 0x80), 0);    // engaged

        conn.setTrxRelay(false);
        QCOMPARE(int(quint8(conn.captureBank10ForTest()[3]) & 0x80), 0x80); // disengaged

        conn.setTrxRelay(true);
        QCOMPARE(int(quint8(conn.captureBank10ForTest()[3]) & 0x80), 0);    // engaged again
    }

    // ── 9. Alex HPF bits are preserved when relay state changes ─────────────
    // C3 is: m_alexHpfBits | relay-disable-bit.  Setting the relay state must
    // not overwrite the HPF bits.  Since m_alexHpfBits is 0 at construction,
    // this test verifies bits 6..0 remain 0 when only relay state changes.
    // (Full HPF bit interaction is tested in tst_p1_codec_standard.cpp.)
    void alexHpfBitsPreserved_bitsZeroToSixAreZero() {
        P1RadioConnection conn;

        conn.setTrxRelay(false);  // relay disabled → bit 7 = 1
        const QByteArray bank10 = conn.captureBank10ForTest();
        // Bits 6..0 of C3 should be 0 (HPF bits all zero at init).
        QCOMPARE(int(quint8(bank10[3]) & 0x7F), 0);

        conn.setTrxRelay(true);   // relay engaged → bit 7 = 0
        const QByteArray bank10b = conn.captureBank10ForTest();
        // Bits 6..0 still 0.
        QCOMPARE(int(quint8(bank10b[3]) & 0x7F), 0);
    }

    // ── 10. HL2 firmware cross-check (informational) ─────────────────────────
    // HL2 control.v:211-214 decodes cmd_addr==6'h09 (which maps to C0=0x12,
    // i.e. address = 0x12 / 2 = 9 = 6'h09):
    //   pa_enable  <= cmd_data[19];   // C2 bit 3 in deskhpsdr's HL2 path
    //   tr_disable <= cmd_data[18];   // C2 bit 2 in deskhpsdr's HL2 path
    // deskhpsdr clears C2/C3/C4 for HL2 (old_protocol.c:2964-2966 [@120188f])
    // and writes C2 |= 0x08 for PA enable — it does NOT set C3 bit 7 for HL2.
    // Therefore the Alex T/R relay bit (C3 bit 7) is only relevant for
    // Alex-equipped boards.  This test documents the boundary by confirming
    // bank 10's C0 address (0x12) matches the cmd_addr (6'h09) HL2 expects.
    void hl2FirmwareCrossCheck_addressMatchesCmdAddr() {
        P1RadioConnection conn;

        const QByteArray bank10 = conn.captureBank10ForTest();
        // C0 bits 7..1 = address = 0x12.  Divide by 2 → 9 = 0x09 = 6'h09.
        // This confirms bank 10 maps to cmd_addr 6'h09 in HL2 firmware,
        // the same address HL2 uses for pa_enable (cmd_data[19]) and
        // tr_disable (cmd_data[18]).  HL2 does not decode C3 bit 7 from this
        // bank; it clears C3 and uses C2 bit 3 instead.
        const int address = (quint8(bank10[0]) & 0xFE) >> 1;  // strip MOX bit
        QCOMPARE(address, 0x09);
    }

    // ── 11. Flush flag is cleared before state: true → false transition ──────
    // Verify that the Codex P2 pattern (set flush flag, THEN check state) means
    // the flag is still true after a false→false no-op call (guard returns early
    // but flush flag was already set before the guard).
    void setTrxRelayFalseOnFalse_flushFlagStillSet() {
        P1RadioConnection conn;
        // Default state is false.  Call setTrxRelay(false) — state doesn't
        // change, but Codex P2 means the flush flag was set before the guard.
        conn.setTrxRelay(false);
        QCOMPARE(conn.forceBank10NextForTest(), true);
    }

    // ── 12. Codec path (Standard): setTrxRelay(true) → C3 bit 7 CLEARED ─────
    // When m_codec is set (production path via applyBoardQuirks), composeCcForBank
    // dispatches through P1CodecStandard::bank10, which must now read ctx.trxRelay
    // (not ctx.paEnabled).  This test drives the codec path by calling
    // setBoardForTest(HPSDRHW::HermesII) which sets m_codec = P1CodecStandard.
    //
    // E.4 bug: without this fix, the codec path wrote (ctx.paEnabled ? 0x80 : 0),
    // ignoring ctx.trxRelay entirely.  Production connections (m_codec != null) would
    // never clear C3 bit 7 on setTrxRelay(true), leaving the relay disengaged mid-TX.
    //
    // Source: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f]
    void setTrxRelayTrue_codecPath_c3Bit7Cleared() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);   // → P1CodecStandard; m_codec != null
        conn.setTrxRelay(true);                     // engage relay → bit 7 must be 0

        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(bank10.size(), 5);
        // C3 bit 7 (0x80) must be 0: relay engaged, normal TX path.
        QCOMPARE(int(quint8(bank10[3]) & 0x80), 0);
    }

    // ── 13. Codec path (Standard): setTrxRelay(false) → C3 bit 7 SET ────────
    // After disengaging the relay on the Standard codec path, C3 bit 7 must be 1.
    // Source: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f]
    void setTrxRelayFalse_codecPath_c3Bit7Set() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);   // → P1CodecStandard; m_codec != null
        conn.setTrxRelay(true);
        conn.setTrxRelay(false);                    // disengage relay → bit 7 must be 1

        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(bank10.size(), 5);
        // C3 bit 7 (0x80) must be 1: relay disabled / PA bypass.
        QCOMPARE(int(quint8(bank10[3]) & 0x80), 0x80);
    }

    // ── 14. Codec path (HL2): setTrxRelay(true) → C3 bit 7 CLEARED ──────────
    // HL2 firmware (control.v:211-214) does NOT decode C3 bit 7 — it uses C2
    // bit 3 for PA enable instead (deskhpsdr old_protocol.c:2964-2966 [@120188f]).
    // The P1CodecHl2::bank10 path still writes ctx.trxRelay into C3 bit 7 for
    // correctness (the HL2 FW ignores it).  Verify the bit is correctly set/cleared
    // so HL2 output matches the Standard codec contract on that byte.
    // Source: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f]
    void setTrxRelayTrue_hl2CodecPath_c3Bit7Cleared() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);  // → P1CodecHl2; m_codec != null
        conn.setTrxRelay(true);                      // engage relay → bit 7 must be 0

        const QByteArray bank10 = conn.captureBank10ForTest();
        QCOMPARE(bank10.size(), 5);
        // C3 bit 7 (0x80) must be 0 (relay engaged), even though HL2 FW ignores it.
        QCOMPARE(int(quint8(bank10[3]) & 0x80), 0);
    }
};

QTEST_APPLESS_MAIN(TestP1TrxRelayWire)
#include "tst_p1_trx_relay_wire.moc"
