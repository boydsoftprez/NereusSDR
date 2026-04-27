// no-port-check: test-only — deskhpsdr and Thetis file names appear only in
// source-cite comments that document which upstream line each assertion verifies.
// No Thetis or deskhpsdr logic is ported here; this file is NereusSDR-original.
//
// Wire-byte snapshot tests for P2RadioConnection::setTxDrive() (3M-1a Task E.7).
//
// Drive level position: CmdHighPriority byte 345.
// Range: 0-255 (linear; 0 = off, 255 = full power).
//
// Source cite:
//   deskhpsdr/src/new_protocol.c:864-876 [@120188f]
//     int power = 0;
//     if ((txfreq >= txband->frequencyMin && txfreq <= txband->frequencyMax)
//         || tx_out_of_band_allowed) {
//       power = transmitter->drive_level;
//     }
//     high_priority_buffer_to_radio[345] = power & 0xFF;
//
// Out-of-band gate:
//   NereusSDR applies the same gate at compose time using bandFromFrequency().
//   If the TX frequency maps to Band::GEN or Band::WWV (outside ham bands),
//   byte 345 is zeroed regardless of the stored driveLevel.
//   BandPlanGuard does not zero driveLevel upstream; the gate is in compose.
//   tx_out_of_band_allowed is not yet wired in NereusSDR.
//
// Test coverage:
//   1.  Default state: byte 345 = 0.
//   2.  setTxDrive(50) → byte 345 = 50.
//   3.  setTxDrive(255) → byte 345 = 255.
//   4.  Clamp high: setTxDrive(300) → byte 345 = 255.
//   5.  Clamp low: setTxDrive(-10) → byte 345 = 0.
//   6.  Codec path (OrionMKII): same value as legacy.
//   7.  Codec path (Saturn): same value.
//   8.  Out-of-band gate: TX freq in ham band → drive passes.
//   9.  Out-of-band gate: TX freq in Band::WWV → byte 345 = 0.
//  10.  Byte 345 round-trip: set, read, update, read.
#include <QtTest/QtTest>
#include "core/P2RadioConnection.h"

using namespace NereusSDR;

class TestP2DriveLevelWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: byte 345 = 0 ───────────────────────────────────────
    // A freshly constructed P2RadioConnection has driveLevel=0.
    // Source: deskhpsdr/src/new_protocol.c:864 [@120188f]
    //   int power = 0;  (default, before TX gate check)
    void defaultState_byte345IsZero() {
        P2RadioConnection conn;
        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        QCOMPARE(int(buf[345]), 0);
    }

    // ── 2. setTxDrive(50) → byte 345 = 50 ────────────────────────────────────
    // TX frequency defaults to 3865000 Hz (40m band — in-band).
    // Source: deskhpsdr/src/new_protocol.c:876 [@120188f]
    //   high_priority_buffer_to_radio[345] = power & 0xFF;
    void setTxDrive50_byte345Is50() {
        P2RadioConnection conn;
        conn.setTxFrequency(3865000ULL);  // 40m — in-band
        conn.setTxDrive(50);
        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        QCOMPARE(int(buf[345]), 50);
    }

    // ── 3. setTxDrive(255) → byte 345 = 255 ──────────────────────────────────
    void setTxDriveMax_byte345Is255() {
        P2RadioConnection conn;
        conn.setTxFrequency(14200000ULL);  // 20m — in-band
        conn.setTxDrive(255);
        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        QCOMPARE(int(buf[345]), 255);
    }

    // ── 4. Clamp high: setTxDrive(300) → byte 345 = 255 ─────────────────────
    // Drive level is clamped to [0, 255] by qBound in setTxDrive.
    void setTxDriveOverflow_clampedTo255() {
        P2RadioConnection conn;
        conn.setTxFrequency(14200000ULL);  // 20m — in-band
        conn.setTxDrive(300);
        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        QCOMPARE(int(buf[345]), 255);
    }

    // ── 5. Clamp low: setTxDrive(-10) → byte 345 = 0 ────────────────────────
    void setTxDriveNegative_clampedTo0() {
        P2RadioConnection conn;
        conn.setTxFrequency(14200000ULL);  // 20m — in-band
        conn.setTxDrive(-10);
        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        QCOMPARE(int(buf[345]), 0);
    }

    // ── 6. Codec path (OrionMKII): same drive level as legacy path ───────────
    // setBoardForTest(OrionMKII) installs P2CodecOrionMkII; both paths must
    // produce the same byte 345 value.  This exercises the PRODUCTION dispatch.
    // Source: P2CodecOrionMkII::composeCmdHighPriority:160 — buf[345] = ctx.p2DriveLevel
    void codecPath_orionMkII_driveLevelMatchesLegacy() {
        P2RadioConnection legacy;
        legacy.setTxFrequency(7150000ULL);  // 40m — in-band
        legacy.setTxDrive(120);
        quint8 legacyBuf[1444] = {};
        legacy.composeCmdHighPriorityForTest(legacyBuf);

        P2RadioConnection codec;
        codec.setBoardForTest(HPSDRHW::OrionMKII);
        codec.setTxFrequency(7150000ULL);
        codec.setTxDrive(120);
        quint8 codecBuf[1444] = {};
        codec.composeCmdHighPriorityForTest(codecBuf);

        QCOMPARE(int(codecBuf[345]), int(legacyBuf[345]));
        QCOMPARE(int(codecBuf[345]), 120);
    }

    // ── 7. Codec path (Saturn/ANAN-G2): same value ───────────────────────────
    void codecPath_saturn_driveLevelPassesThrough() {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.setTxFrequency(14200000ULL);  // 20m — in-band
        conn.setTxDrive(75);
        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        QCOMPARE(int(buf[345]), 75);
    }

    // ── 8. Out-of-band gate: TX freq in ham band → drive passes ──────────────
    // A TX frequency inside a recognised ham band (e.g. 20m) lets driveLevel
    // pass through to byte 345 unchanged.
    // Source: deskhpsdr/src/new_protocol.c:872-873 [@120188f]
    //   if ((txfreq >= txband->frequencyMin && txfreq <= txband->frequencyMax)
    //       || tx_out_of_band_allowed) { power = transmitter->drive_level; }
    void outOfBandGate_inBandFreq_drivePasses() {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::OrionMKII);
        conn.setTxFrequency(14200000ULL);  // 20m — IARU Region 2 in-band
        conn.setTxDrive(100);
        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        QCOMPARE(int(buf[345]), 100);
    }

    // ── 9. Out-of-band gate: TX freq in Band::WWV → byte 345 = 0 ────────────
    // 10 MHz is one of the WWV discrete centers (bandFromFrequency returns
    // Band::WWV, not Band::GEN).  WWV is not a ham band, so the gate zeros
    // byte 345 regardless of driveLevel.
    // Source: deskhpsdr/src/new_protocol.c:864,876 [@120188f]
    //   int power = 0; ... high_priority_buffer_to_radio[345] = power & 0xFF;
    // NereusSDR translation: txBand == WWV → power = 0.
    void outOfBandGate_genBandFreq_driveZeroed() {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::OrionMKII);
        // 10 MHz is a WWV discrete center (bandFromFrequency returns Band::WWV).
        conn.setTxFrequency(10000000ULL);  // 10 MHz — Band::WWV (WWV-time-signal frequency, not a ham band; gate zeros it)
        conn.setTxDrive(100);
        quint8 buf[1444] = {};
        conn.composeCmdHighPriorityForTest(buf);
        // Out-of-band gate must zero the drive level.
        QCOMPARE(int(buf[345]), 0);
    }

    // ── 10. Round-trip: setTxDrive, read, update, read ───────────────────────
    // Two successive setTxDrive calls must each be independently visible.
    void roundTrip_driveLevelUpdates() {
        P2RadioConnection conn;
        conn.setTxFrequency(21200000ULL);  // 15m — in-band

        conn.setTxDrive(80);
        quint8 buf1[1444] = {};
        conn.composeCmdHighPriorityForTest(buf1);
        QCOMPARE(int(buf1[345]), 80);

        conn.setTxDrive(160);
        quint8 buf2[1444] = {};
        conn.composeCmdHighPriorityForTest(buf2);
        QCOMPARE(int(buf2[345]), 160);
    }
};

QTEST_APPLESS_MAIN(TestP2DriveLevelWire)
#include "tst_p2_drive_level_wire.moc"
