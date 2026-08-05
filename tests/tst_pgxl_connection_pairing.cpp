// =================================================================
// tests/tst_pgxl_connection_pairing.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No AetherSDR equivalent (Tier 2 command surface
// is NereusSDR-only per design doc §2 and §6.4).
// Wire formats from FlexRadio PowerGenius Ethernet API wiki spec.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19  Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 Tests: amplifierCreate frame format, flexradioPair
//                 accept/reject via R-frame pairingResult signal.
// =================================================================

#include <QtTest/QtTest>
#include "core/PgxlConnection.h"

class PgxlConnectionPairingTest : public QObject {
    Q_OBJECT
private slots:
    void amplifierCreateEmitsExpectedFrame();
    void pairingSuccessEmitsPairingResultTrue();
    void pairingFailureEmitsPairingResultFalse();
};

void PgxlConnectionPairingTest::amplifierCreateEmitsExpectedFrame() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy frameSpy(&conn, &NereusSDR::PgxlConnection::testFrameWrittenForTesting);
    conn.amplifierCreate("NereusSDR-AA:BB:CC", "NereusSDR", "ANT1:PORTA,ANT2:PORTB");
    QCOMPARE(frameSpy.count(), 1);
    QString frame = frameSpy.takeFirst().at(0).toString();
    QVERIFY(frame.contains("amplifier create"));
    QVERIFY(frame.contains("model=NereusSDR"));
    QVERIFY(frame.contains("serial_num=NereusSDR-AA:BB:CC"));
    QVERIFY(frame.contains("ant=ANT1:PORTA,ANT2:PORTB"));
}

void PgxlConnectionPairingTest::pairingSuccessEmitsPairingResultTrue() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy resultSpy(&conn, &NereusSDR::PgxlConnection::pairingResult);
    conn.injectLineForTesting("V3.8.9");
    quint32 seq = conn.flexradioPair('A', "NereusSDR-AA:BB", "ANT1");
    conn.injectLineForTesting(QString("R%1|0|serial=NereusSDR-AA:BB txant=ANT1").arg(seq));
    QCOMPARE(resultSpy.count(), 1);
    QCOMPARE(resultSpy.takeFirst().at(0).toBool(), true);
}

void PgxlConnectionPairingTest::pairingFailureEmitsPairingResultFalse() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy resultSpy(&conn, &NereusSDR::PgxlConnection::pairingResult);
    conn.injectLineForTesting("V3.8.9");
    quint32 seq = conn.flexradioPair('A', "NereusSDR-AA:BB", "ANT1");
    conn.injectLineForTesting(QString("R%1|2|invalid serial format").arg(seq));
    QCOMPARE(resultSpy.count(), 1);
    QCOMPARE(resultSpy.takeFirst().at(0).toBool(), false);
}

QTEST_GUILESS_MAIN(PgxlConnectionPairingTest)
#include "tst_pgxl_connection_pairing.moc"
