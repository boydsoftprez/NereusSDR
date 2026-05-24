#include <QtTest/QtTest>
#include "core/Rf2ksConnection.h"

using namespace NereusSDR;

class Rf2ksConnectionReconnectTest : public QObject {
    Q_OBJECT
private slots:
    void backoffSequenceFollowsSchedule();
    void successResetsBackoff();
};

void Rf2ksConnectionReconnectTest::backoffSequenceFollowsSchedule() {
    Rf2ksConnection conn;
    conn.testForceBackoffSequence();
    QCOMPARE(conn.testCurrentBackoffMs(), 1000);
    conn.testForceBackoffSequence();
    QCOMPARE(conn.testCurrentBackoffMs(), 2000);
    conn.testForceBackoffSequence();
    QCOMPARE(conn.testCurrentBackoffMs(), 4000);
    conn.testForceBackoffSequence();
    QCOMPARE(conn.testCurrentBackoffMs(), 8000);
    for (int i = 0; i < 5; ++i) { conn.testForceBackoffSequence(); }
    QVERIFY(conn.testCurrentBackoffMs() <= 60000);
}

void Rf2ksConnectionReconnectTest::successResetsBackoff() {
    Rf2ksConnection conn;
    conn.testForceBackoffSequence();
    conn.testForceBackoffSequence();
    conn.testForceBackoffSequence();
    QVERIFY(conn.testCurrentBackoffMs() > 1000);
    conn.testForceBackoffReset();
    QCOMPARE(conn.testCurrentBackoffMs(), 1000);
}

QTEST_MAIN(Rf2ksConnectionReconnectTest)
#include "tst_rf2ks_connection_reconnect.moc"
