// no-port-check: test for the Anvelina DX Outputs group on Setup > Hardware
// Config > OC Outputs (OcOutputsTab), NereusSDR-original.
//
// Modification history (NereusSDR):
//   2026-10-26 - Created: visibility by model, box / All on / All off to
//                the model's wire byte, model to UI sync without write-back,
//                stopped vs running (J.J. Boyd KG4VCF, AI-assisted: Claude
//                Code).
//   2026-10-26 - Fix wave: a name saves when its edit finishes; a remote
//                window on a Core below radioHardwareVersion 14 keeps the
//                group disabled with its reason; the ANAN_G2 row is named
//                for its model (J.J. Boyd KG4VCF, AI-assisted: Claude Code).

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>

#include "core/AnvelinaDxOutputs.h"
#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/ConnectionState.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"
#include "core/session/IStationLink.h"
#include "gui/setup/HardwarePage.h"
#include "gui/setup/hardware/OcOutputsTab.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

const QString kMac = QStringLiteral("AA:BB:CC:DD:0E:01");
const QString kOtherMac = QStringLiteral("AA:BB:CC:DD:0E:02");

QString dxKey(const QString& mac, int out, const char* field)
{
    return QStringLiteral("hardware/%1/oc/dx/out%2/%3").arg(mac).arg(out).arg(QLatin1String(field));
}

RadioInfo radioWithMac(const QString& mac)
{
    RadioInfo info;
    info.macAddress = mac;
    return info;
}

// Poses the model as `model` and tells the tab the radio, as
// HardwarePage::onCurrentRadioChanged does.
void pose(RadioModel& radio, OcOutputsTab& tab, HPSDRModel model, const QString& mac = kMac)
{
    radio.setHpsdrModelForTest(model);
    tab.populate(radioWithMac(mac), *radio.hardwareProfile().caps);
}

} // namespace

class TstOcOutputsDxGroup : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
    }
    void init()
    {
        AppSettings::instance().clearHardwareValues(kMac);
        AppSettings::instance().clearHardwareValues(kOtherMac);
    }
    void cleanup()
    {
        AppSettings::instance().clearHardwareValues(kMac);
        AppSettings::instance().clearHardwareValues(kOtherMac);
    }

    void shownOnAnAnvelinaOnly_data()
    {
        QTest::addColumn<int>("model");
        QTest::addColumn<bool>("shown");
        QTest::newRow("ANAN7000D") << int(HPSDRModel::ANAN7000D) << false;
        QTest::newRow("ANAN_G2 (Saturn)") << int(HPSDRModel::ANAN_G2) << false;
        QTest::newRow("ANVELINAPRO3") << int(HPSDRModel::ANVELINAPRO3) << true;
    }
    void shownOnAnAnvelinaOnly()
    {
        QFETCH(int, model);
        QFETCH(bool, shown);
        RadioModel radio;
        OcOutputsTab tab(&radio);
        pose(radio, tab, static_cast<HPSDRModel>(model));
        QCOMPARE(tab.dxGroupVisibleForTest(), shown);
    }

    void aBoxSwitchesItsOutput()
    {
        RadioModel radio;
        OcOutputsTab tab(&radio);
        pose(radio, tab, HPSDRModel::ANVELINAPRO3);
        radio.setConnectionStateForTest(ConnectionState::Connected);

        QCheckBox* out9 = tab.dxCheckboxForTest(9);
        QVERIFY(out9 != nullptr);
        QCOMPARE(out9->text(), QStringLiteral("DX OUT 9"));
        QVERIFY(out9->isEnabled());
        out9->click();
        QVERIFY(radio.anvelinaDxOutputs().outputOn(9));
        QCOMPARE(int(radio.anvelinaDxOutputs().wireByte()), 0x08);
        QCOMPARE(AppSettings::instance().value(dxKey(kMac, 9, "enabled")).toString(),
                 QStringLiteral("True"));

        tab.dxAllOnButtonForTest()->click();
        QCOMPARE(int(radio.anvelinaDxOutputs().wireByte()), 0x1E);
        for (int out = 7; out <= 10; ++out) {
            QVERIFY(tab.dxCheckboxForTest(out)->isChecked());
        }
        tab.dxAllOffButtonForTest()->click();
        QCOMPARE(int(radio.anvelinaDxOutputs().wireByte()), 0x00);
        for (int out = 7; out <= 10; ++out) {
            QVERIFY(!tab.dxCheckboxForTest(out)->isChecked());
        }
    }

    void aNameIsTheModels()
    {
        RadioModel radio;
        OcOutputsTab tab(&radio);
        pose(radio, tab, HPSDRModel::ANVELINAPRO3);

        QLineEdit* name = tab.dxNameFieldForTest(7);
        QVERIFY(name != nullptr);
        QCOMPARE(name->placeholderText(), QStringLiteral("Name"));
        QCOMPARE(name->toolTip(), QStringLiteral("Your name for this output, shown here only"));
        name->setFocus();
        QTest::keyClicks(name, QStringLiteral("ANT Switch"));
        // Not saved per keystroke: in a remote window each save is a Core
        // write and an "ocDx" reload.
        QVERIFY(radio.anvelinaDxOutputs().label(7).isEmpty());
        QVERIFY(!AppSettings::instance().contains(dxKey(kMac, 7, "label")));
        // A model change mid-edit (another box) leaves the typing alone.
        radio.anvelinaDxOutputsMutable().setOutputOn(8, true);
        QCOMPARE(name->text(), QStringLiteral("ANT Switch"));
        QTest::keyClick(name, Qt::Key_Return);   // the edit finishes
        QCOMPARE(radio.anvelinaDxOutputs().label(7), QStringLiteral("ANT Switch"));
        QCOMPARE(AppSettings::instance().value(dxKey(kMac, 7, "label")).toString(),
                 QStringLiteral("ANT Switch"));
    }

    void anotherRadiosStateFillsTheBoxesWithoutWritingBack()
    {
        AppSettings& s = AppSettings::instance();
        s.setValue(dxKey(kOtherMac, 8, "enabled"), QStringLiteral("True"));
        s.setValue(dxKey(kOtherMac, 10, "label"), QStringLiteral("Band Data LATCH"));

        RadioModel radio;
        OcOutputsTab tab(&radio);
        pose(radio, tab, HPSDRModel::ANVELINAPRO3);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        tab.dxCheckboxForTest(9)->click();   // the first radio: out 9 on
        QCOMPARE(int(radio.anvelinaDxOutputs().wireByte()), 0x08);

        QSignalSpy toggled8(tab.dxCheckboxForTest(8), &QCheckBox::toggled);
        QSignalSpy toggled9(tab.dxCheckboxForTest(9), &QCheckBox::toggled);
        QSignalSpy changed(&radio.anvelinaDxOutputsMutable(), &AnvelinaDxOutputs::changed);

        AnvelinaDxOutputs& dx = radio.anvelinaDxOutputsMutable();
        dx.setMacAddress(kOtherMac);
        dx.load();

        QVERIFY(tab.dxCheckboxForTest(8)->isChecked());
        QVERIFY(!tab.dxCheckboxForTest(9)->isChecked());
        QCOMPARE(tab.dxNameFieldForTest(10)->text(), QStringLiteral("Band Data LATCH"));
        // Nothing echoed back: the boxes moved with their signals blocked,
        // the model announced the load once, and the first radio keeps out 9.
        QCOMPARE(toggled8.count(), 0);
        QCOMPARE(toggled9.count(), 0);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(int(dx.wireByte()), 0x04);
        QCOMPARE(s.value(dxKey(kMac, 9, "enabled")).toString(), QStringLiteral("True"));
        QVERIFY(!s.contains(dxKey(kOtherMac, 9, "enabled")));
    }

    void stoppedHoldsTheBoxesAndSaysWhy()
    {
        RadioModel radio;
        OcOutputsTab tab(&radio);
        pose(radio, tab, HPSDRModel::ANVELINAPRO3);
        const QString held = QStringLiteral("The radio holds the DX outputs off while it is stopped.");

        // Not connected: stopped.
        QVERIFY(!radio.isConnected());
        for (int out = 7; out <= 10; ++out) {
            QVERIFY(!tab.dxCheckboxForTest(out)->isEnabled());
            QCOMPARE(tab.dxCheckboxForTest(out)->toolTip(), held);
            QVERIFY(tab.dxNameFieldForTest(out)->isEnabled());
        }
        QVERIFY(!tab.dxAllOnButtonForTest()->isEnabled());
        QVERIFY(!tab.dxAllOffButtonForTest()->isEnabled());
        QCOMPARE(tab.dxAllOnButtonForTest()->toolTip(), held);
        QCOMPARE(tab.dxAllOffButtonForTest()->toolTip(), held);
        QVERIFY(tab.dxReasonVisibleForTest());

        // Running.
        radio.setConnectionStateForTest(ConnectionState::Connected);
        for (int out = 7; out <= 10; ++out) {
            QVERIFY(tab.dxCheckboxForTest(out)->isEnabled());
            QCOMPARE(tab.dxCheckboxForTest(out)->toolTip(),
                     QStringLiteral("Switches USEROUT%1 on the Anvelina's DX connector.").arg(out));
            QVERIFY(tab.dxNameFieldForTest(out)->isEnabled());
        }
        QVERIFY(tab.dxAllOnButtonForTest()->isEnabled());
        QVERIFY(tab.dxAllOffButtonForTest()->isEnabled());
        QVERIFY(!tab.dxReasonVisibleForTest());

        // The link drops: stopped again.
        radio.setConnectionStateForTest(ConnectionState::LinkLost);
        QVERIFY(!tab.dxCheckboxForTest(7)->isEnabled());
        QVERIFY(tab.dxReasonVisibleForTest());
    }

    // radioHardwareVersion 14: a Core below it reloads only the OC matrix
    // for an oc/ key, so a remote window's DX group would switch nothing.
    // The group is shown, disabled with its reason (names too: they are
    // Core writes), and that reason wins over the stopped one.
    void olderCoreKeepsTheGroupClosedWithItsReason()
    {
        RadioModel remote(RadioModel::Role::Remote);
        remote.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        HardwarePage page(&remote);
        auto* tab = page.findChild<OcOutputsTab*>();
        QVERIFY(tab != nullptr);
        tab->populate(radioWithMac(kMac), *remote.hardwareProfile().caps);
        QVERIFY(tab->dxGroupVisibleForTest());
        const QString reason = IStationLink::dxOutputsUnavailableReason();
        for (int out = 7; out <= 10; ++out) {
            QVERIFY(!tab->dxCheckboxForTest(out)->isEnabled());
            QCOMPARE(tab->dxCheckboxForTest(out)->toolTip(), reason);
            QVERIFY(!tab->dxNameFieldForTest(out)->isEnabled());
            QCOMPARE(tab->dxNameFieldForTest(out)->toolTip(), reason);
        }
        QVERIFY(!tab->dxAllOnButtonForTest()->isEnabled());
        QCOMPARE(tab->dxAllOnButtonForTest()->toolTip(), reason);
        QCOMPARE(tab->dxAllOffButtonForTest()->toolTip(), reason);
        QVERIFY(!tab->dxReasonVisibleForTest());
    }

    // The gate opens again when the Core offers it: running, everything is
    // live; stopped, the stopped reason comes back.
    void availableAgain_followsRunning()
    {
        RadioModel radio;
        OcOutputsTab tab(&radio);
        pose(radio, tab, HPSDRModel::ANVELINAPRO3);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        const QString reason = IStationLink::dxOutputsUnavailableReason();

        tab.setDxOutputsAvailable(false, reason);
        QVERIFY(!tab.dxCheckboxForTest(7)->isEnabled());
        QCOMPARE(tab.dxCheckboxForTest(7)->toolTip(), reason);
        QVERIFY(!tab.dxNameFieldForTest(7)->isEnabled());

        tab.setDxOutputsAvailable(true, QString());
        QVERIFY(tab.dxCheckboxForTest(7)->isEnabled());
        QVERIFY(tab.dxNameFieldForTest(7)->isEnabled());
        QCOMPARE(tab.dxNameFieldForTest(7)->toolTip(),
                 QStringLiteral("Your name for this output, shown here only"));
        QVERIFY(tab.dxAllOnButtonForTest()->isEnabled());

        radio.setConnectionStateForTest(ConnectionState::Disconnected);
        QCOMPARE(tab.dxCheckboxForTest(7)->toolTip(),
                 QStringLiteral("The radio holds the DX outputs off while it is stopped."));
        QVERIFY(tab.dxNameFieldForTest(7)->isEnabled());
        QVERIFY(tab.dxReasonVisibleForTest());
    }
};

QTEST_MAIN(TstOcOutputsDxGroup)
#include "tst_oc_outputs_dx_group.moc"
