#include <QtTest/QtTest>
#include <QCheckBox>
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

QTEST_MAIN(RfKitPageMasterGateTest)
#include "tst_rfkit_page_master_gate.moc"
