// =================================================================
// tests/tst_pgxl_connection_setup.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No AetherSDR equivalent (Tier 2 command surface
// is NereusSDR-only per design doc section 2 and 6.4).
// Wire formats from FlexRadio PowerGenius Ethernet API wiki spec.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19  Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 Tests: readSetup frame format, writeSetup kv building,
//                 R-frame parse routing through statusUpdated, seq tracking.
// =================================================================

#include <QtTest/QtTest>
#include "core/PgxlConnection.h"

class PgxlConnectionSetupTest : public QObject {
    Q_OBJECT
private slots:
    void readSetupSendsCommand();
    void writeSetupBuildsKvCommand();
    void writeSetupRefusesAFieldThatWouldSplit();
    void anOldNameIsOfferedAsOneWord();
    void parsesSetupResponse();
    void seqIncrementsBetweenCalls();
};

// readSetupSendsCommand: conn.readSetup() returns a positive seq and emits a
// frame containing "setup read" via the testFrameWrittenForTesting seam.
void PgxlConnectionSetupTest::readSetupSendsCommand() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy frameSpy(&conn, &NereusSDR::PgxlConnection::testFrameWrittenForTesting);
    quint32 seq = conn.readSetup();
    QVERIFY(seq > 0);
    QCOMPARE(frameSpy.count(), 1);
    QString frame = frameSpy.takeFirst().at(0).toString();
    QVERIFY(frame.contains(QStringLiteral("setup read")));
}

// writeSetupBuildsKvCommand: conn.writeSetup({...}) emits a frame containing
// "setup" and all supplied k=v pairs (order-independent).
// Wire format: "setup nickname=ShackAmp fan=Quiet" per FlexRadio wiki spec.
void PgxlConnectionSetupTest::writeSetupBuildsKvCommand() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy frameSpy(&conn, &NereusSDR::PgxlConnection::testFrameWrittenForTesting);
    QMap<QString,QString> fields;
    fields.insert(QStringLiteral("nickname"), QStringLiteral("ShackAmp"));
    fields.insert(QStringLiteral("fan"),      QStringLiteral("Quiet"));
    quint32 seq = conn.writeSetup(fields);
    QVERIFY(seq > 0);
    QCOMPARE(frameSpy.count(), 1);
    QString frame = frameSpy.takeFirst().at(0).toString();
    QVERIFY(frame.contains(QStringLiteral("setup")));
    QVERIFY(frame.contains(QStringLiteral("nickname=ShackAmp")));
    QVERIFY(frame.contains(QStringLiteral("fan=Quiet")));
}

// RD-I11: a value with a space or '=' would become extra fields on the
// amp's `setup` line ("Shack bias=a" sets the bias). Nothing is sent.
// Fix round 1 (minor 4): a name saved with spaces is offered as one word.
void PgxlConnectionSetupTest::anOldNameIsOfferedAsOneWord() {
    using NereusSDR::PgxlConnection;
    QCOMPARE(PgxlConnection::asSetupToken(QStringLiteral(" Shack PGXL ")),
             QStringLiteral("Shack_PGXL"));
    QCOMPARE(PgxlConnection::asSetupToken(QStringLiteral("Shack  bias=a")),
             QStringLiteral("Shack_bias_a"));
    QCOMPARE(PgxlConnection::asSetupToken(QStringLiteral("Shack_Amp")),
             QStringLiteral("Shack_Amp"));
    QCOMPARE(PgxlConnection::asSetupToken(QString()), QString());
    QVERIFY(PgxlConnection::isSetupToken(
        PgxlConnection::asSetupToken(QStringLiteral("a = b\tc"))));
}

void PgxlConnectionSetupTest::writeSetupRefusesAFieldThatWouldSplit() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy frameSpy(&conn, &NereusSDR::PgxlConnection::testFrameWrittenForTesting);
    for (const QString& bad : {QStringLiteral("Shack bias=a"), QStringLiteral("Shack PGXL"),
                               QStringLiteral("a=b"), QStringLiteral("tab\there")}) {
        QCOMPARE(conn.writeSetup({{QStringLiteral("nickname"), bad}}), quint32(0));
    }
    QCOMPARE(conn.writeSetup({{QStringLiteral("nick name"), QStringLiteral("Amp")}}), quint32(0));
    QCOMPARE(frameSpy.count(), 0);
    QVERIFY(NereusSDR::PgxlConnection::isSetupToken(QStringLiteral("Shack_PGXL")));
    QVERIFY(!NereusSDR::PgxlConnection::isSetupToken(QStringLiteral("Shack PGXL")));
}

// parsesSetupResponse: inject a synthetic R-frame whose body contains setup kv
// pairs and verify statusUpdated fires with the parsed map.
// PgxlConnection emits statusUpdated (not a dedicated setupResponse) for all
// kv bodies in R-frames -- confirmed by reading processLine() in PgxlConnection.cpp.
void PgxlConnectionSetupTest::parsesSetupResponse() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy statusSpy(&conn, &NereusSDR::PgxlConnection::statusUpdated);

    // Handshake required before R-frames are processed.
    conn.injectLineForTesting(QStringLiteral("V3.8.9"));

    quint32 seq = conn.readSetup();
    conn.injectLineForTesting(
        QString("R%1|0|nickname=ShackAmp fan=Quiet bias=ClassA led=80").arg(seq));

    QCOMPARE(statusSpy.count(), 1);
    auto kvs = statusSpy.takeFirst().at(0).value<QMap<QString,QString>>();
    QCOMPARE(kvs.value(QStringLiteral("nickname")), QStringLiteral("ShackAmp"));
    QCOMPARE(kvs.value(QStringLiteral("fan")),      QStringLiteral("Quiet"));
    QCOMPARE(kvs.value(QStringLiteral("bias")),     QStringLiteral("ClassA"));
    QCOMPARE(kvs.value(QStringLiteral("led")),      QStringLiteral("80"));
}

// seqIncrementsBetweenCalls: a second readSetup() returns a seq > the first.
// Guards against accidental seq regression.
void PgxlConnectionSetupTest::seqIncrementsBetweenCalls() {
    NereusSDR::PgxlConnection conn;
    quint32 seq1 = conn.readSetup();
    quint32 seq2 = conn.readSetup();
    QVERIFY(seq2 > seq1);
}

QTEST_GUILESS_MAIN(PgxlConnectionSetupTest)
#include "tst_pgxl_connection_setup.moc"
