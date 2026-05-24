// =================================================================
// tests/tst_rfkit_radiomodel_enabled.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No upstream source file ported.
//
// Modification history (NereusSDR):
//   2026-05-24 -- Authored by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include "models/RadioModel.h"
#include "core/AppSettings.h"

class RfKitEnabledTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void defaultsFalse();
    void setterPersistsAndEmits();
    void getterReadsFromAppSettings();
    void exposesRf2ksConnection();
    void enablingTriggersConnect();
    void disablingTriggersDisconnect();
};

void RfKitEnabledTest::initTestCase() {
    NereusSDR::AppSettings::instance().remove(QStringLiteral("RfKit_Enabled"));
}

void RfKitEnabledTest::defaultsFalse() {
    NereusSDR::AppSettings::instance().remove(QStringLiteral("RfKit_Enabled"));
    NereusSDR::RadioModel m;
    QCOMPARE(m.rfKitEnabled(), false);
}

void RfKitEnabledTest::setterPersistsAndEmits() {
    NereusSDR::AppSettings::instance().remove(QStringLiteral("RfKit_Enabled"));
    NereusSDR::RadioModel m;
    QSignalSpy spy(&m, &NereusSDR::RadioModel::rfKitEnabledChanged);

    m.setRfKitEnabled(true);

    QCOMPARE(m.rfKitEnabled(), true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);
    QCOMPARE(NereusSDR::AppSettings::instance()
        .value(QStringLiteral("RfKit_Enabled")).toString(), QStringLiteral("True"));
}

void RfKitEnabledTest::getterReadsFromAppSettings() {
    NereusSDR::AppSettings::instance()
        .setValue(QStringLiteral("RfKit_Enabled"), QStringLiteral("True"));
    NereusSDR::RadioModel m;
    QCOMPARE(m.rfKitEnabled(), true);
}

void RfKitEnabledTest::exposesRf2ksConnection() {
    NereusSDR::RadioModel m;
    QVERIFY(m.rfKitConnection() != nullptr);
}

void RfKitEnabledTest::enablingTriggersConnect() {
    NereusSDR::AppSettings::instance().remove(QStringLiteral("RfKit_Enabled"));
    NereusSDR::AppSettings::instance()
        .setValue(QStringLiteral("RfKit_ManualIp"), QStringLiteral("127.0.0.1"));
    NereusSDR::AppSettings::instance()
        .setValue(QStringLiteral("RfKit_ManualPort"), QStringLiteral("12345"));
    NereusSDR::RadioModel m;
    m.setRfKitEnabled(true);
    QCOMPARE(m.rfKitConnection()->peerAddress(), QString("127.0.0.1"));
    QCOMPARE(m.rfKitConnection()->peerPort(),    quint16(12345));
}

void RfKitEnabledTest::disablingTriggersDisconnect() {
    NereusSDR::AppSettings::instance().remove(QStringLiteral("RfKit_Enabled"));
    NereusSDR::AppSettings::instance()
        .setValue(QStringLiteral("RfKit_ManualIp"), QStringLiteral("127.0.0.1"));
    NereusSDR::AppSettings::instance()
        .setValue(QStringLiteral("RfKit_ManualPort"), QStringLiteral("12345"));
    NereusSDR::RadioModel m;
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
