#include <QtTest/QtTest>
#include <QCheckBox>
#include <QPushButton>
#include "gui/setup/RfKitPage.h"
#include "models/RadioModel.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class RfKitPageMasterGateTest : public QObject {
    Q_OBJECT
private slots:
    void cleanup();
    void masterCheckboxReflectsModel();
    void togglingCheckboxFlipsModel();
    void detailTabGreysWhenMasterOff();
    void hostPortInputsPersist();
    void antennaLabelsRoundTrip();
    void testConnectionButtonPresent();
};

void RfKitPageMasterGateTest::cleanup() {
    AppSettings::instance().remove(QStringLiteral("RfKit_Enabled"));
}

void RfKitPageMasterGateTest::masterCheckboxReflectsModel() {
    AppSettings::instance().setValue(QStringLiteral("RfKit_Enabled"),
                                     QStringLiteral("True"));
    RadioModel m;
    RfKitPage page(&m);
    QVERIFY(page.masterCheckboxForTesting()->isChecked());
}

void RfKitPageMasterGateTest::togglingCheckboxFlipsModel() {
    RadioModel m;
    RfKitPage page(&m);
    page.masterCheckboxForTesting()->setChecked(true);
    QCOMPARE(m.rfKitEnabled(), true);
    page.masterCheckboxForTesting()->setChecked(false);
    QCOMPARE(m.rfKitEnabled(), false);
}

void RfKitPageMasterGateTest::detailTabGreysWhenMasterOff() {
    RadioModel m;
    RfKitPage page(&m);
    page.masterCheckboxForTesting()->setChecked(true);
    QVERIFY(page.detailTabIsEnabledForTesting());
    page.masterCheckboxForTesting()->setChecked(false);
    QVERIFY(!page.detailTabIsEnabledForTesting());
}

void RfKitPageMasterGateTest::hostPortInputsPersist() {
    AppSettings::instance().remove(QStringLiteral("RfKit_ManualIp"));
    RadioModel m;
    RfKitPage page(&m);
    page.setHostForTesting("10.0.0.5");
    page.setPortForTesting(8080);
    page.clickSaveForTesting();
    QCOMPARE(AppSettings::instance()
        .value(QStringLiteral("RfKit_ManualIp")).toString(),
        QString("10.0.0.5"));
}

void RfKitPageMasterGateTest::antennaLabelsRoundTrip() {
    RadioModel m;
    RfKitPage page(&m);
    page.setAntennaLabelForTesting(1, "80m dipole");
    page.setAntennaLabelForTesting(2, "20m beam");
    page.clickSaveForTesting();
    QCOMPARE(AppSettings::instance()
        .value(QStringLiteral("RfKit_Ant1_Label")).toString(),
        QString("80m dipole"));
}

void RfKitPageMasterGateTest::testConnectionButtonPresent() {
    RadioModel m;
    RfKitPage page(&m);
    QVERIFY(page.testConnectionButtonForTesting() != nullptr);
}

QTEST_MAIN(RfKitPageMasterGateTest)
#include "tst_rfkit_page_master_gate.moc"
