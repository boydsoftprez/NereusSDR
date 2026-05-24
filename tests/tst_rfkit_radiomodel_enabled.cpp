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

QTEST_MAIN(RfKitEnabledTest)
#include "tst_rfkit_radiomodel_enabled.moc"
