// =================================================================
// tests/tst_rfkit_page_master_gate.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No upstream source file ported.
//
// Modification history (NereusSDR):
//   2026-05-24 -- Authored by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-05-26 -- Per-radio peripherals refactor: RfKitPage now drives
//                 RfKit_* via RadioModel::peripheralValue, which writes
//                 under hardware/<mac>/peripherals/.  Tests prime a
//                 fake connection so the page can resolve a MAC scope.
// =================================================================

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QPushButton>
#include "gui/setup/RfKitPage.h"
#include "models/RadioModel.h"
#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"   // RadioInfo
#include "core/RadioConnection.h"  // ConnectionState

using namespace NereusSDR;

class RfKitPageMasterGateTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanup();
    void masterCheckboxReflectsModel();
    void togglingCheckboxFlipsModel();
    void detailTabGreysWhenMasterOff();
    void hostPortInputsPersist();
    void antennaLabelsRoundTrip();
    void testConnectionButtonPresent();

private:
    static void primeConnectedRadio(RadioModel& m,
                                    const QString& mac =
                                        QStringLiteral("aa:bb:cc:dd:ee:02"));
};

void RfKitPageMasterGateTest::primeConnectedRadio(RadioModel& m,
                                                  const QString& mac)
{
    RadioInfo info;
    info.macAddress = mac;
    m.setLastRadioInfoForTest(info);
    m.setConnectionStateForTest(ConnectionState::Connected);
    m.applyPeripheralsForTest();
}

void RfKitPageMasterGateTest::initTestCase() {
    AppSettings::instance().clear();
}

void RfKitPageMasterGateTest::cleanup() {
    AppSettings::instance().clear();
}

void RfKitPageMasterGateTest::masterCheckboxReflectsModel() {
    RadioModel m;
    primeConnectedRadio(m);
    AppSettings::instance().setHardwareValue(
        m.currentRadioMac(),
        QStringLiteral("peripherals/RfKit_Enabled"),
        QStringLiteral("True"));
    RfKitPage page(&m);
    QVERIFY(page.masterCheckboxForTesting()->isChecked());
}

void RfKitPageMasterGateTest::togglingCheckboxFlipsModel() {
    RadioModel m;
    primeConnectedRadio(m);
    RfKitPage page(&m);
    page.masterCheckboxForTesting()->setChecked(true);
    QCOMPARE(m.rfKitEnabled(), true);
    page.masterCheckboxForTesting()->setChecked(false);
    QCOMPARE(m.rfKitEnabled(), false);
}

void RfKitPageMasterGateTest::detailTabGreysWhenMasterOff() {
    RadioModel m;
    primeConnectedRadio(m);
    RfKitPage page(&m);
    page.masterCheckboxForTesting()->setChecked(true);
    QVERIFY(page.detailTabIsEnabledForTesting());
    page.masterCheckboxForTesting()->setChecked(false);
    QVERIFY(!page.detailTabIsEnabledForTesting());
}

void RfKitPageMasterGateTest::hostPortInputsPersist() {
    RadioModel m;
    primeConnectedRadio(m);
    RfKitPage page(&m);
    page.setHostForTesting("10.0.0.5");
    page.setPortForTesting(8080);
    page.clickSaveForTesting();
    QCOMPARE(AppSettings::instance()
        .hardwareValue(m.currentRadioMac(),
                       QStringLiteral("peripherals/RfKit_ManualIp"))
        .toString(),
        QString("10.0.0.5"));
}

void RfKitPageMasterGateTest::antennaLabelsRoundTrip() {
    RadioModel m;
    primeConnectedRadio(m);
    RfKitPage page(&m);
    page.setAntennaLabelForTesting(1, "80m dipole");
    page.setAntennaLabelForTesting(2, "20m beam");
    page.clickSaveForTesting();
    // Antenna labels stay GLOBAL (operator preference, not per-radio).
    QCOMPARE(AppSettings::instance()
        .value(QStringLiteral("RfKit_Ant1_Label")).toString(),
        QString("80m dipole"));
}

void RfKitPageMasterGateTest::testConnectionButtonPresent() {
    RadioModel m;
    primeConnectedRadio(m);
    RfKitPage page(&m);
    QVERIFY(page.testConnectionButtonForTesting() != nullptr);
}

QTEST_MAIN(RfKitPageMasterGateTest)
#include "tst_rfkit_page_master_gate.moc"
