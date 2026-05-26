// =================================================================
// tests/tst_rfkit_radiomodel_enabled.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No upstream source file ported.
//
// Modification history (NereusSDR):
//   2026-05-24 -- Authored by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-05-26 -- Per-radio peripherals refactor: setRfKitEnabled now
//                 writes per-MAC under hardware/<mac>/peripherals/.
//                 Tests pin a MAC via setLastRadioInfoForTest and drive
//                 the Connected state via setConnectionStateForTest.
// =================================================================

#include <QtTest/QtTest>
#include "models/RadioModel.h"
#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"   // RadioInfo
#include "core/RadioConnection.h"  // ConnectionState

class RfKitEnabledTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanup();
    void defaultsFalse();
    void setterPersistsAndEmits();
    void getterReadsFromAppSettings();
    void exposesRf2ksConnection();
    void enablingTriggersConnect();
    void disablingTriggersDisconnect();

private:
    static void primeConnectedRadio(NereusSDR::RadioModel& m,
                                    const QString& mac =
                                        QStringLiteral("aa:bb:cc:dd:ee:01"));
};

void RfKitEnabledTest::primeConnectedRadio(NereusSDR::RadioModel& m,
                                           const QString& mac)
{
    NereusSDR::RadioInfo info;
    info.macAddress = mac;
    m.setLastRadioInfoForTest(info);
    m.setConnectionStateForTest(NereusSDR::ConnectionState::Connected);
    // Drive applyPeripheralsForCurrentMac() so the one-shot migration
    // sentinel ("PeripheralsMigrationDone") flips to True; otherwise the
    // first test seeds globals via setRfKitEnabled which (before migration
    // runs) would not produce the expected hardware/<mac>/peripherals/
    // entries.  In production the same hook fires from the Connected
    // arm of onConnectionStateChanged.
    m.applyPeripheralsForTest();
}

void RfKitEnabledTest::initTestCase() {
    NereusSDR::AppSettings::instance().clear();
}

void RfKitEnabledTest::cleanup() {
    // Each test runs in its own RadioModel/AppSettings sandbox; wipe between.
    NereusSDR::AppSettings::instance().clear();
}

void RfKitEnabledTest::defaultsFalse() {
    NereusSDR::RadioModel m;
    primeConnectedRadio(m);
    QCOMPARE(m.rfKitEnabled(), false);
}

void RfKitEnabledTest::setterPersistsAndEmits() {
    NereusSDR::RadioModel m;
    primeConnectedRadio(m);
    QSignalSpy spy(&m, &NereusSDR::RadioModel::rfKitEnabledChanged);

    m.setRfKitEnabled(true);

    QCOMPARE(m.rfKitEnabled(), true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);
    QCOMPARE(NereusSDR::AppSettings::instance()
        .hardwareValue(m.currentRadioMac(),
                       QStringLiteral("peripherals/RfKit_Enabled"))
        .toString(),
        QStringLiteral("True"));
}

void RfKitEnabledTest::getterReadsFromAppSettings() {
    NereusSDR::RadioModel m;
    primeConnectedRadio(m);
    NereusSDR::AppSettings::instance().setHardwareValue(
        m.currentRadioMac(),
        QStringLiteral("peripherals/RfKit_Enabled"),
        QStringLiteral("True"));
    QCOMPARE(m.rfKitEnabled(), true);
}

void RfKitEnabledTest::exposesRf2ksConnection() {
    NereusSDR::RadioModel m;
    QVERIFY(m.rfKitConnection() != nullptr);
}

void RfKitEnabledTest::enablingTriggersConnect() {
    NereusSDR::RadioModel m;
    primeConnectedRadio(m);
    const QString mac = m.currentRadioMac();
    NereusSDR::AppSettings::instance().setHardwareValue(
        mac,
        QStringLiteral("peripherals/RfKit_ManualIp"),
        QStringLiteral("127.0.0.1"));
    NereusSDR::AppSettings::instance().setHardwareValue(
        mac,
        QStringLiteral("peripherals/RfKit_ManualPort"),
        QStringLiteral("12345"));
    m.setRfKitEnabled(true);
    QCOMPARE(m.rfKitConnection()->peerAddress(), QString("127.0.0.1"));
    QCOMPARE(m.rfKitConnection()->peerPort(),    quint16(12345));
}

void RfKitEnabledTest::disablingTriggersDisconnect() {
    NereusSDR::RadioModel m;
    primeConnectedRadio(m);
    const QString mac = m.currentRadioMac();
    NereusSDR::AppSettings::instance().setHardwareValue(
        mac,
        QStringLiteral("peripherals/RfKit_ManualIp"),
        QStringLiteral("127.0.0.1"));
    NereusSDR::AppSettings::instance().setHardwareValue(
        mac,
        QStringLiteral("peripherals/RfKit_ManualPort"),
        QStringLiteral("12345"));
    // connectToAmp stores host/port but m_connected stays false until an HTTP
    // reply arrives (no real server here).  Force the connected flag so that
    // the subsequent disconnect() actually emits disconnected().
    m.setRfKitEnabled(true);
    m.rfKitConnection()->testForceConnectedForTesting();
    QSignalSpy disSpy(m.rfKitConnection(), &NereusSDR::Rf2ksConnection::disconnected);
    m.setRfKitEnabled(false);
    QCOMPARE(disSpy.count(), 1);
}

QTEST_MAIN(RfKitEnabledTest)
#include "tst_rfkit_radiomodel_enabled.moc"
