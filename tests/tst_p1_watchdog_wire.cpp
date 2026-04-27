// no-port-check: test-only -- HL2 firmware file names and deskhpsdr source
// paths appear only in source-cite comments that document which upstream line
// each assertion verifies.  No Thetis or deskhpsdr logic is ported here;
// this file is NereusSDR-original.
//
// Wire-byte snapshot tests for the RUNSTOP packet watchdog bit (3M-1a Task E.5).
//
// Watchdog bit position: RUNSTOP packet byte 3 (pkt[3]), bit 7 (0x80).
// Semantic INVERTED vs the boolean flag:
//   m_watchdogEnabled == true  -> pkt[3] bit 7 = 0  (watchdog NOT disabled)
//   m_watchdogEnabled == false -> pkt[3] bit 7 = 1  (watchdog disabled)
//
// Primary source cite (HL2 firmware -- the receiver of this packet):
//   Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:200-203
//     RUNSTOP: begin
//       run_next = eth_data[0];
//       wide_spectrum_next = eth_data[1];
//       runstop_watchdog_valid = 1'b1;
//     end
//
//   Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:399-400
//     end else if (runstop_watchdog_valid) begin
//       watchdog_disable <= eth_data[7]; // Bit 7 can be used to disable watchdog
//     end
//
// deskhpsdr reference (deskhpsdr/src/old_protocol.c:3811 [@120188f]):
//   buffer[3] = command;  // 0x01 start or 0x00 stop -- bit 7 never set
//   deskhpsdr has no user-configurable watchdog disable.  It implicitly keeps
//   bit 7 = 0 (watchdog enabled) on every RUNSTOP packet.  NereusSDR matches
//   this as the default (m_watchdogEnabled{true} -> bit 7 = 0).
//
// Thetis call-site (setup.cs:17986 [v2.10.3.13]):
//   NetworkIO.SetWatchdogTimer(Convert.ToInt32(chkNetworkWDT.Checked));
//   chkNetworkWDT.Checked == true (enabled) -> passes 1 -> bit 7 = 0 (not disabled).
//
// RUNSTOP packet layout (64 bytes):
//   pkt[0] = 0xEF  (Metis magic byte 0)
//   pkt[1] = 0xFE  (Metis magic byte 1)
//   pkt[2] = 0x04  (RUNSTOP command type)
//   pkt[3] = run_bits | watchdog_disable_bit
//     run_bits: 0x01 = start IQ only; 0x02 = start IQ+mic; 0x00 = stop
//     watchdog_disable_bit: 0x80 if watchdog disabled, 0x00 if enabled
//   pkt[4..63] = 0x00 (padding)
//
// Note on co-existence of bits in pkt[3]:
//   eth_data[0] = run (bit 0), eth_data[1] = wide_spectrum (bit 1),
//   eth_data[7] = watchdog_disable (bit 7).  These are independent and OR'd
//   together.  Bits 2..6 are unused.
#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"

using namespace NereusSDR;

class TestP1WatchdogWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: watchdog bit is CLEAR on start packet ──────────────
    // A freshly constructed P1RadioConnection has m_watchdogEnabled = true
    // (3M-1a E.5 default -- see RadioConnection.h comment for rationale).
    // Therefore pkt[3] bit 7 must be 0 (watchdog NOT disabled).
    //
    // Source: Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:399-400
    //   watchdog_disable <= eth_data[7]; // 0 = watchdog active (not disabled)
    // deskhpsdr: buffer[3] = command -- bit 7 = 0 implicitly.
    void defaultState_watchdogBitIsClear_startPacket() {
        P1RadioConnection conn;
        // Default: m_watchdogEnabled = true -> bit 7 = 0
        const QByteArray pkt = conn.metisStartPacketForTest(false);
        QCOMPARE(pkt.size(), 64);
        // pkt[3] bit 7 must be 0 (watchdog enabled = bit NOT set).
        // Source: Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:399-400
        QCOMPARE(int(quint8(pkt[3]) & 0x80), 0);
    }

    // ── 2. Default state: watchdog bit is CLEAR on stop packet ───────────────
    // Same default state assertion for the stop packet.
    // deskhpsdr/src/old_protocol.c:3811 [@120188f]:
    //   buffer[3] = command; // 0x00 stop -- bit 7 = 0 implicitly
    void defaultState_watchdogBitIsClear_stopPacket() {
        P1RadioConnection conn;
        const QByteArray pkt = conn.metisStopPacketForTest();
        QCOMPARE(pkt.size(), 64);
        QCOMPARE(int(quint8(pkt[3]) & 0x80), 0);
    }

    // ── 3. setWatchdogEnabled(false) -> pkt[3] bit 7 = 1 (disabled) ──────────
    // When the watchdog is disabled, bit 7 must be 1.
    // Source: Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:400
    //   watchdog_disable <= eth_data[7]; // 1 = watchdog disabled
    void setWatchdogDisabled_startPacket_bit7IsSet() {
        P1RadioConnection conn;
        conn.setWatchdogEnabled(false);

        const QByteArray pkt = conn.metisStartPacketForTest(false);
        QCOMPARE(int(quint8(pkt[3]) & 0x80), 0x80);
    }

    // ── 4. setWatchdogEnabled(false) -> stop packet bit 7 = 1 ────────────────
    void setWatchdogDisabled_stopPacket_bit7IsSet() {
        P1RadioConnection conn;
        conn.setWatchdogEnabled(false);

        const QByteArray pkt = conn.metisStopPacketForTest();
        QCOMPARE(int(quint8(pkt[3]) & 0x80), 0x80);
    }

    // ── 5. setWatchdogEnabled(true) after false -> bit 7 cleared ─────────────
    // Round-trip: disable then re-enable; bit 7 must return to 0.
    void setWatchdogEnabled_afterDisable_bit7Cleared() {
        P1RadioConnection conn;
        conn.setWatchdogEnabled(false);
        conn.setWatchdogEnabled(true);

        const QByteArray pkt = conn.metisStartPacketForTest(false);
        QCOMPARE(int(quint8(pkt[3]) & 0x80), 0);
    }

    // ── 6. Round-trip: true -> false -> true ─────────────────────────────────
    void roundTrip_enabledToDisabledToEnabled() {
        P1RadioConnection conn;
        // Default true: bit 7 = 0
        QCOMPARE(int(quint8(conn.metisStartPacketForTest(false)[3]) & 0x80), 0);

        conn.setWatchdogEnabled(false);
        // Disabled: bit 7 = 1
        QCOMPARE(int(quint8(conn.metisStartPacketForTest(false)[3]) & 0x80), 0x80);

        conn.setWatchdogEnabled(true);
        // Re-enabled: bit 7 = 0
        QCOMPARE(int(quint8(conn.metisStartPacketForTest(false)[3]) & 0x80), 0);
    }

    // ── 7. Idempotency: repeated same-value call doesn't crash ────────────────
    void idempotent_disabledTwice_doesNotCrash() {
        P1RadioConnection conn;
        conn.setWatchdogEnabled(false);
        conn.setWatchdogEnabled(false);  // second call -- no-op via guard
        QCOMPARE(int(quint8(conn.metisStartPacketForTest(false)[3]) & 0x80), 0x80);
    }

    // ── 8. HL2 cross-check: run bit (eth_data[0]) and watchdog bit
    //       (eth_data[7]) co-exist independently in pkt[3] ─────────────────────
    //
    // Source: Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:200-203
    //   run_next = eth_data[0];          // bit 0
    //   wide_spectrum_next = eth_data[1]; // bit 1
    //   watchdog_disable <= eth_data[7]; // bit 7 -- separate from run
    //
    // With watchdog disabled: pkt[3] has both bit 0 (run=1) and bit 7 (0x80).
    // These bits are independent. Verify both are set simultaneously.
    void hl2FirmwareCrossCheck_runAndWatchdogBitsCoexist() {
        P1RadioConnection conn;
        conn.setWatchdogEnabled(false);  // watchdog disabled -> bit 7 = 1

        // Start IQ-only packet: run bit 0 = 1 (from 0x01), bit 7 = 1 (from 0x80)
        const QByteArray pkt = conn.metisStartPacketForTest(false);

        // bit 0 (run) must be set
        // Source: dsopenhpsdr1.v:201 -- run_next = eth_data[0]
        QCOMPARE(int(quint8(pkt[3]) & 0x01), 0x01);

        // bit 7 (watchdog_disable) must also be set
        // Source: dsopenhpsdr1.v:400 -- watchdog_disable <= eth_data[7]
        QCOMPARE(int(quint8(pkt[3]) & 0x80), 0x80);

        // Combined: pkt[3] = 0x81 (run=1, watchdog disabled)
        QCOMPARE(int(quint8(pkt[3])), 0x81);
    }

    // ── 9. sendMetisStart(true) + watchdog disabled -> pkt[3] = 0x82 ──────────
    // IQ+mic mode (iqAndMic=true): run bits = 0x02 (bit 1 set for wide_spectrum).
    // Watchdog disabled: bit 7 = 0x80.
    // Expected: pkt[3] = 0x82 | 0x80 = ... wait: 0x02 | 0x80 = 0x82.
    //
    // Source: Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:202
    //   wide_spectrum_next = eth_data[1]; // bit 1 -- set when iqAndMic=true (0x02)
    void startIqMic_watchdogDisabled_pkt3Is0x82() {
        P1RadioConnection conn;
        conn.setWatchdogEnabled(false);

        const QByteArray pkt = conn.metisStartPacketForTest(true);  // IQ+mic
        // 0x02 (IQ+mic run) | 0x80 (watchdog disabled) = 0x82
        QCOMPARE(int(quint8(pkt[3])), 0x82);
    }

    // ── 10. sendMetisStop() + watchdog disabled -> pkt[3] = 0x80 ─────────────
    // Stop packet (run = 0x00), watchdog disabled (bit 7 = 0x80).
    // Expected: pkt[3] = 0x00 | 0x80 = 0x80.
    //
    // Source: Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:200-203
    //   run_next = eth_data[0]; // 0 = stop
    // deskhpsdr/src/old_protocol.c:3811 [@120188f]:
    //   buffer[3] = command; // 0x00 stop
    void stopPacket_watchdogDisabled_pkt3Is0x80() {
        P1RadioConnection conn;
        conn.setWatchdogEnabled(false);

        const QByteArray pkt = conn.metisStopPacketForTest();
        // 0x00 (stop) | 0x80 (watchdog disabled) = 0x80
        QCOMPARE(int(quint8(pkt[3])), 0x80);
    }

    // ── 11. Packet header bytes are always correct (regression guard) ─────────
    // pkt[0..2] must always be 0xEF 0xFE 0x04 regardless of watchdog state.
    // Source: deskhpsdr/src/old_protocol.c:3808-3810 [@120188f]:
    //   buffer[0] = 0xEF; buffer[1] = 0xFE; buffer[2] = 0x04;
    void packetHeader_isAlwaysCorrect() {
        P1RadioConnection conn;
        conn.setWatchdogEnabled(false);

        const QByteArray startPkt = conn.metisStartPacketForTest(false);
        QCOMPARE(quint8(startPkt[0]), quint8(0xEF));
        QCOMPARE(quint8(startPkt[1]), quint8(0xFE));
        QCOMPARE(quint8(startPkt[2]), quint8(0x04));

        const QByteArray stopPkt = conn.metisStopPacketForTest();
        QCOMPARE(quint8(stopPkt[0]), quint8(0xEF));
        QCOMPARE(quint8(stopPkt[1]), quint8(0xFE));
        QCOMPARE(quint8(stopPkt[2]), quint8(0x04));
    }

    // ── 12. Packet padding bytes 4..63 are always zero ───────────────────────
    // Source: deskhpsdr/src/old_protocol.c:3814-3817 [@120188f]:
    //   for (i = 4; i < 64; i++) { buffer[i] = 0x00; }
    void paddingBytes_areAllZero() {
        P1RadioConnection conn;
        conn.setWatchdogEnabled(false);

        const QByteArray pkt = conn.metisStartPacketForTest(false);
        QCOMPARE(pkt.size(), 64);
        for (int i = 4; i < 64; ++i) {
            QCOMPARE(int(quint8(pkt[i])), 0);
        }
    }
};

QTEST_APPLESS_MAIN(TestP1WatchdogWire)
#include "tst_p1_watchdog_wire.moc"
