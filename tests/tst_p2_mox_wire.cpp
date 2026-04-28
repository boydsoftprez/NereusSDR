// no-port-check: test-only — deskhpsdr and Thetis file names appear only in
// source-cite comments that document which upstream line each assertion verifies.
// No Thetis or deskhpsdr logic is ported here; this file is NereusSDR-original.
//
// Wire-byte snapshot tests for P2RadioConnection::setMox() (3M-1a Task E.7).
//
// MOX bit position: CmdHighPriority byte 4, bit 1 (0x02).
// Semantic: 1 = MOX on (transmitting), 0 = MOX off (receiving).
// Run bit (bit 0 = 0x01) is independent of MOX and must not be disturbed.
//
// Source cite:
//   deskhpsdr/src/new_protocol.c:739-762 [@120188f]
//     high_priority_buffer_to_radio[4] = P2running;        // bit 0 = run
//     if (xmit) { high_priority_buffer_to_radio[4] |= 0x02; }  // bit 1 = MOX
//
// Latent bug fixed in 3M-1a E.7:
//   Before this fix, setMox(true) set m_tx[0].pttOut (rear-panel PTT-out relay)
//   instead of m_mox.  The composeCmdHighPriority*() functions used pttOut << 1
//   for byte 4 bit 1.  Because pttOut was never changed by the radio (only by
//   rear-panel relay events, which are future 3M-3 work), the MOX bit never
//   asserted on the wire.
//
// Test coverage:
//   1.  Default state: byte 4 bit 1 = 0.
//   2.  setMox(true) → byte 4 bit 1 = 1.
//   3.  setMox(false) → byte 4 bit 1 = 0.
//   4.  Round-trip true→false→true.
//   5.  Run bit (bit 0) preserved when MOX toggles.
//   6.  Bits 2-7 of byte 4 unaffected by MOX.
//   7.  Codec path (setBoardForTest(OrionMKII)) same result as legacy.
//   8.  Codec path (setBoardForTest(Saturn)) same result.
//   9.  Idempotency: setMox(true) twice → bit still 1.
//  10.  Codec path: byte 5 CW bits unaffected by setMox.
#include <QtTest/QtTest>
#include "core/P2RadioConnection.h"

using namespace NereusSDR;

class TestP2MoxWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: byte 4 bit 1 is clear ──────────────────────────────
    // A freshly constructed P2RadioConnection has m_mox=false, so CmdHighPriority
    // byte 4 bit 1 must be 0.
    // Source: deskhpsdr/src/new_protocol.c:739 [@120188f]
    //   high_priority_buffer_to_radio[4] = P2running;  (P2running=0 before start)
    void defaultState_moxBitIsClear() {
        P2RadioConnection conn;
        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        // Byte 4 bit 1 (0x02) = MOX. Must be 0 (not transmitting).
        QCOMPARE(int(buf[4] & 0x02), 0);
    }

    // ── 2. setMox(true) → byte 4 bit 1 set ──────────────────────────────────
    // After setMox(true), the MOX bit (0x02) in byte 4 must be 1.
    // Source: deskhpsdr/src/new_protocol.c:761 [@120188f]
    //   high_priority_buffer_to_radio[4] |= 0x02;
    void setMoxTrue_byte4Bit1IsSet() {
        P2RadioConnection conn;
        conn.setMox(true);
        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        QCOMPARE(int(buf[4] & 0x02), 0x02);
    }

    // ── 3. setMox(false) → byte 4 bit 1 cleared ──────────────────────────────
    void setMoxFalse_byte4Bit1IsCleared() {
        P2RadioConnection conn;
        conn.setMox(true);
        conn.setMox(false);
        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        QCOMPARE(int(buf[4] & 0x02), 0);
    }

    // ── 4. Round-trip true→false→true: bit tracks state faithfully ───────────
    void roundTrip_trueToFalseToTrue() {
        P2RadioConnection conn;

        conn.setMox(true);
        quint8 buf1[1444] = {};
        conn.composeCmdHighPriorityForTest(buf1);
        QCOMPARE(int(buf1[4] & 0x02), 0x02);

        conn.setMox(false);
        quint8 buf2[1444] = {};
        conn.composeCmdHighPriorityForTest(buf2);
        QCOMPARE(int(buf2[4] & 0x02), 0);

        conn.setMox(true);
        quint8 buf3[1444] = {};
        conn.composeCmdHighPriorityForTest(buf3);
        QCOMPARE(int(buf3[4] & 0x02), 0x02);
    }

    // ── 5. Run bit (bit 0) unaffected by MOX toggle ──────────────────────────
    // m_running is false in test state (no live socket), so bit 0 = 0.
    // setMox must not disturb bit 0.
    // Source: deskhpsdr/src/new_protocol.c:739 [@120188f]
    //   high_priority_buffer_to_radio[4] = P2running;  // bit 0 = run, independent of MOX
    void runBit_unaffectedByMox() {
        P2RadioConnection conn;
        // Without setBoardForTest the legacy path runs; m_running=false → bit 0=0.
        conn.setMox(true);
        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        // Bit 0 must be 0 (m_running=false).
        QCOMPARE(int(buf[4] & 0x01), 0);
        // Bit 1 must be 1 (MOX=true).
        QCOMPARE(int(buf[4] & 0x02), 0x02);
    }

    // ── 6. Bits 2-7 of byte 4 must be 0 with setMox(true) ───────────────────
    // Only bits 0 (run) and 1 (MOX) are defined for byte 4.  All others = 0.
    // Source: deskhpsdr/src/new_protocol.c:739-762 [@120188f] — only bits 0,1 set.
    void byte4UpperBits_unaffectedByMox() {
        P2RadioConnection conn;
        conn.setMox(true);
        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        // Bits 2-7 must be 0.
        QCOMPARE(int(buf[4] & 0xFC), 0);
    }

    // ── 7. Codec path (OrionMKII): MOX bit lands at byte 4 bit 1 ────────────
    // setBoardForTest(OrionMKII) installs P2CodecOrionMkII; composeCmdHighPriority
    // delegates to it.  The codec must produce the same byte-4 result as the
    // legacy path.  This is the PRODUCTION dispatch path for OrionMKII-class boards.
    // Source: P2CodecOrionMkII::composeCmdHighPriority line 138 — uses ctx.p2PttOut
    //   which buildCodecContext() now populates from m_mox (not m_tx[0].pttOut).
    void codecPath_orionMkII_moxBitLandsAtByte4Bit1() {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::OrionMKII);

        conn.setMox(true);
        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        QCOMPARE(int(buf[4] & 0x02), 0x02);

        conn.setMox(false);
        memset(buf, 0, sizeof(buf));
        conn.composeCmdHighPriorityForTest(buf);
        QCOMPARE(int(buf[4] & 0x02), 0);
    }

    // ── 8. Codec path (Saturn/ANAN-G2): same MOX bit behaviour ──────────────
    // P2CodecSaturn inherits composeCmdHighPriority from P2CodecOrionMkII, so
    // the MOX byte must be identical.  The ANAN-G2 is the primary target board.
    void codecPath_saturn_moxBitLandsAtByte4Bit1() {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);

        conn.setMox(true);
        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        QCOMPARE(int(buf[4] & 0x02), 0x02);

        conn.setMox(false);
        memset(buf, 0, sizeof(buf));
        conn.composeCmdHighPriorityForTest(buf);
        QCOMPARE(int(buf[4] & 0x02), 0);
    }

    // ── 9. Idempotency: setMox(true) twice → bit remains 1 ──────────────────
    // Codex P2: repeated calls with the same value must not clear the bit.
    void idempotency_setMoxTrueTwice_bitStays1() {
        P2RadioConnection conn;
        conn.setMox(true);
        conn.setMox(true);
        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        QCOMPARE(int(buf[4] & 0x02), 0x02);
    }

    // ── 10. Codec path: byte 5 (CW keying) unaffected by setMox ─────────────
    // Byte 5 = dash<<2 | dot<<1 | cwx.  Must stay 0x00 when only MOX changes.
    // Source: deskhpsdr/src/new_protocol.c:766-768 [@120188f]
    //   high_priority_buffer_to_radio[5] = (dash << 2 | dot << 1 | cwx);
    void codecPath_byte5_unaffectedByMox() {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::OrionMKII);
        conn.setMox(true);
        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        // CW bits not set — cwx/dot/dash all 0 by default.
        QCOMPARE(int(buf[5]), 0);
    }
};

QTEST_APPLESS_MAIN(TestP2MoxWire)
#include "tst_p2_mox_wire.moc"
