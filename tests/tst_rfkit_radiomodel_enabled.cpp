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
//   2026-09-24 -- R-R3-47: rfKitEnabled is the Core's switch, Core to
//                 window only: no raw write, a remote window holds the
//                 Core's value and never switches it itself, and on the
//                 Core the switch runs the amp through StationRfKitController.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include "models/RadioModel.h"
#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"   // RadioInfo
#include "core/RadioConnection.h"  // ConnectionState
#include "core/StationRfKitController.h"
#include "models/RfKitModel.h"
#include <QMetaProperty>
#include <QTcpServer>

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
    void currentRadioMacIsEmptyWhileOffline();
    void currentRadioMacReturnsMacWhileConnected();
    void switchHasNoRawWrite();
    void remoteWindowHoldsTheCoresSwitch();
    void coreRunsTheSwitchThroughItsController();

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

// Codex review [P2] on PR #291.  currentRadioMac() is the gate both
// RfKitPage and FourO3APage use to decide whether the peripherals UI is
// live ("Editing peripherals for <radio>" vs "Connect to a radio...",
// and whether the RF2K-S detail tab is interactive).
//
// m_lastRadioInfo is retained across a disconnect on purpose, so the
// accessor kept returning the previous radio's MAC while nothing was
// connected -- letting the operator edit, and even start, peripherals
// scoped to a radio that had gone away.  The header contract already
// said "returns m_lastRadioInfo.macAddress when connected, empty
// otherwise"; the implementation had drifted from its own documentation.
//
// Gated on m_connectionState rather than isConnected(): the latter needs
// a live RadioConnection object, which these tests deliberately do not
// stand up.
void RfKitEnabledTest::currentRadioMacIsEmptyWhileOffline()
{
    NereusSDR::RadioModel m;
    primeConnectedRadio(m, QStringLiteral("aa:bb:cc:dd:ee:42"));
    QCOMPARE(m.currentRadioMac(), QStringLiteral("aa:bb:cc:dd:ee:42"));

    // Radio goes away.  m_lastRadioInfo is deliberately still populated.
    m.setConnectionStateForTest(NereusSDR::ConnectionState::Disconnected);

    QVERIFY2(m.currentRadioMac().isEmpty(),
             "currentRadioMac() still reported the previous radio's MAC "
             "while offline; the peripherals UI gates on this and would "
             "stay live for a radio that is gone");
}

void RfKitEnabledTest::currentRadioMacReturnsMacWhileConnected()
{
    NereusSDR::RadioModel m;
    primeConnectedRadio(m, QStringLiteral("aa:bb:cc:dd:ee:43"));

    // The normal case must keep working -- a gate that always denies
    // would pass the test above while breaking the whole peripherals UI.
    QCOMPARE(m.currentRadioMac(), QStringLiteral("aa:bb:cc:dd:ee:43"));
}

void RfKitEnabledTest::switchHasNoRawWrite()
{
    const QMetaObject& mo = NereusSDR::RadioModel::staticMetaObject;
    const int index = mo.indexOfProperty("rfKitEnabled");
    QVERIFY(index >= 0);
    QVERIFY(!mo.property(index).isWritable());
}

void RfKitEnabledTest::remoteWindowHoldsTheCoresSwitch()
{
    NereusSDR::RadioModel window(NereusSDR::RadioModel::Role::Remote);
    QSignalSpy spy(&window, &NereusSDR::RadioModel::rfKitEnabledChanged);
    QVERIFY(!window.rfKitEnabled());
    QVERIFY(window.applyMirroredValue("rfKitEnabled", QVariant(true)).isEmpty());
    QVERIFY(window.rfKitEnabled());
    QCOMPARE(spy.count(), 1);
    QVERIFY(!window.applyMirroredValue("rfKitEnabled", QVariant(QStringLiteral("yes"))).isEmpty());

    // The window never switches the Core's amp itself.
    QTest::ignoreMessage(QtWarningMsg,
        "setRfKitEnabled ignored in a remote window; the Core owns the RF-Kit switch");
    window.setRfKitEnabled(false);
    QVERIFY(window.rfKitEnabled());
    QCOMPARE(spy.count(), 1);
    QVERIFY(!window.rfKitConnection()->isConnected());
    QVERIFY(window.rfKitConnection()->peerAddress().isEmpty());
}

void RfKitEnabledTest::coreRunsTheSwitchThroughItsController()
{
    QTcpServer reservation;
    QVERIFY(reservation.listen(QHostAddress::LocalHost, 0));
    const quint16 closed = reservation.serverPort();
    reservation.close();

    NereusSDR::RadioModel core;
    core.enableStationAccessoryIdentity();
    QVERIFY(core.stationRfKitController() != nullptr);
    QVERIFY(core.rfKitConnection()->identityAdmissionRequired());
    primeConnectedRadio(core, QStringLiteral("aa:bb:cc:dd:ee:44"));
    core.setPeripheralValue(QStringLiteral("RfKit_ManualIp"), QStringLiteral("127.0.0.1"));
    core.setPeripheralValue(QStringLiteral("RfKit_ManualPort"), QString::number(closed));
    using Phase = NereusSDR::RfKitModel::ConnectionPhase;

    core.setRfKitEnabled(true);
    QCOMPARE(core.rfKitModel()->connectionPhase(), Phase::Connecting);
    QCOMPARE(core.rfKitModel()->configuredHost(), QStringLiteral("127.0.0.1"));
    core.setRfKitEnabled(false);
    QCOMPARE(core.rfKitModel()->connectionPhase(), Phase::Disabled);
    QVERIFY(!core.rfKitConnection()->reconnectPending());
    QVERIFY(!core.rfKitConnection()->isConnected());
}

QTEST_MAIN(RfKitEnabledTest)
#include "tst_rfkit_radiomodel_enabled.moc"
