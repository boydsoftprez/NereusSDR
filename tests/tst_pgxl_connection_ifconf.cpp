// =================================================================
// tests/tst_pgxl_connection_ifconf.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No AetherSDR equivalent (Tier 2 command surface
// is NereusSDR-only per design doc section 2 and 6.4).
// Wire formats from FlexRadio PowerGenius Ethernet API wiki spec.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19  Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 Tests: readIfconf frame format, writeIfconf kv building,
//                 R-frame ifconf response parse routing through statusUpdated.
// =================================================================

#include <QtTest/QtTest>
#include "core/PgxlConnection.h"

class PgxlConnectionIfconfTest : public QObject {
    Q_OBJECT
private slots:
    void readIfconfSendsCommand();
    void writeIfconfBuildsCommandWithIpFields();
    void parsesIfconfResponse();
};

// readIfconfSendsCommand: conn.readIfconf() returns a positive seq and emits
// a frame containing "ifconf read" via the testFrameWrittenForTesting seam.
void PgxlConnectionIfconfTest::readIfconfSendsCommand() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy frameSpy(&conn, &NereusSDR::PgxlConnection::testFrameWrittenForTesting);
    quint32 seq = conn.readIfconf();
    QVERIFY(seq > 0);
    QCOMPARE(frameSpy.count(), 1);
    QString frame = frameSpy.takeFirst().at(0).toString();
    QVERIFY(frame.contains(QStringLiteral("ifconf read")));
}

// writeIfconfBuildsCommandWithIpFields: conn.writeIfconf(...) emits a frame
// containing "ifconf" and all four IP fields in the expected format.
// Wire format: "ifconf address=<ip> netmask=<mask> gateway=<gw> dhcp=<true|false>"
// per FlexRadio PowerGenius Ethernet API wiki spec (design section 6.4).
void PgxlConnectionIfconfTest::writeIfconfBuildsCommandWithIpFields() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy frameSpy(&conn, &NereusSDR::PgxlConnection::testFrameWrittenForTesting);
    quint32 seq = conn.writeIfconf(
        QStringLiteral("192.168.1.43"),
        QStringLiteral("255.255.255.0"),
        QStringLiteral("192.168.1.1"),
        false);
    QVERIFY(seq > 0);
    QCOMPARE(frameSpy.count(), 1);
    QString frame = frameSpy.takeFirst().at(0).toString();
    QVERIFY(frame.contains(QStringLiteral("ifconf")));
    QVERIFY(frame.contains(QStringLiteral("address=192.168.1.43")));
    QVERIFY(frame.contains(QStringLiteral("netmask=255.255.255.0")));
    QVERIFY(frame.contains(QStringLiteral("gateway=192.168.1.1")));
    QVERIFY(frame.contains(QStringLiteral("dhcp=false")));
}

// parsesIfconfResponse: inject a synthetic R-frame with IP config fields and
// verify statusUpdated fires with all four keys parsed correctly.
// PgxlConnection emits statusUpdated (not a dedicated ifconfResponse) for all
// kv bodies in R-frames -- confirmed by reading processLine() in PgxlConnection.cpp.
void PgxlConnectionIfconfTest::parsesIfconfResponse() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy statusSpy(&conn, &NereusSDR::PgxlConnection::statusUpdated);

    // Handshake required before R-frames are processed.
    conn.injectLineForTesting(QStringLiteral("V3.8.9"));

    quint32 seq = conn.readIfconf();
    conn.injectLineForTesting(
        QString("R%1|0|address=192.168.1.43 netmask=255.255.255.0 gateway=192.168.1.1 dhcp=0")
            .arg(seq));

    QCOMPARE(statusSpy.count(), 1);
    auto kvs = statusSpy.takeFirst().at(0).value<QMap<QString,QString>>();
    QCOMPARE(kvs.value(QStringLiteral("address")),  QStringLiteral("192.168.1.43"));
    QCOMPARE(kvs.value(QStringLiteral("netmask")), QStringLiteral("255.255.255.0"));
    QCOMPARE(kvs.value(QStringLiteral("gateway")), QStringLiteral("192.168.1.1"));
    QCOMPARE(kvs.value(QStringLiteral("dhcp")),    QStringLiteral("0"));
}

QTEST_GUILESS_MAIN(PgxlConnectionIfconfTest)
#include "tst_pgxl_connection_ifconf.moc"
