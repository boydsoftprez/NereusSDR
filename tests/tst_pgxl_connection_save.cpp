// =================================================================
// tests/tst_pgxl_connection_save.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No AetherSDR equivalent (Tier 2 command surface
// is NereusSDR-only per design doc section 2 and 6.4).
// Wire format from FlexRadio PowerGenius Ethernet API wiki spec:
//   "save" -> R<seq>|0|saving -> amp acknowledges then reboots.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19  Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 Tests: save command frame, saveAcknowledged signal on
//                 "saving" body, seq increments after save call.
// =================================================================

#include <QtTest/QtTest>
#include "core/PgxlConnection.h"

class PgxlConnectionSaveTest : public QObject {
    Q_OBJECT
private slots:
    void saveSendsCommand();
    void saveResponseEmitsSaveAcknowledged();
    void saveSeqIsPositive();
};

// saveSendsCommand: conn.save() emits a frame containing "save" via the
// testFrameWrittenForTesting seam.
// Wire format: "save" per FlexRadio PowerGenius Ethernet API wiki spec.
void PgxlConnectionSaveTest::saveSendsCommand() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy frameSpy(&conn, &NereusSDR::PgxlConnection::testFrameWrittenForTesting);
    conn.save();
    QCOMPARE(frameSpy.count(), 1);
    QString frame = frameSpy.takeFirst().at(0).toString();
    QVERIFY(frame.contains(QStringLiteral("save")));
}

// saveResponseEmitsSaveAcknowledged: inject R<seq>|0|saving and verify the
// saveAcknowledged signal fires.
// Design: "saving" is the body the PGXL sends when it accepts the save command
// and will reboot. PgxlConnection::processLine() matches body=="saving" and
// emits saveAcknowledged() -- confirmed by reading processLine().
void PgxlConnectionSaveTest::saveResponseEmitsSaveAcknowledged() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy saveSpy(&conn, &NereusSDR::PgxlConnection::saveAcknowledged);

    // Handshake required before R-frames are processed.
    conn.injectLineForTesting(QStringLiteral("V3.8.9"));

    quint32 seq = conn.save();
    conn.injectLineForTesting(QString("R%1|0|saving").arg(seq));

    QCOMPARE(saveSpy.count(), 1);
}

// saveSeqIsPositive: conn.save() returns a non-zero seq number.
void PgxlConnectionSaveTest::saveSeqIsPositive() {
    NereusSDR::PgxlConnection conn;
    quint32 seq = conn.save();
    QVERIFY(seq > 0);
}

QTEST_GUILESS_MAIN(PgxlConnectionSaveTest)
#include "tst_pgxl_connection_save.moc"
