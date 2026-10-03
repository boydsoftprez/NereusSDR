// no-port-check: NereusSDR-original receive telemetry presentation regression.
#include <QtTest>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>

#include "gui/applets/RxApplet.h"
#include "gui/MainWindow.h"
#include "core/NoiseFloorTracker.h"
#include "gui/widgets/FilterPolicyDialog.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {
QLabel* floorLabel(QWidget& widget)
{
    for (auto* label : widget.findChildren<QLabel*>()) {
        if (label->text().startsWith(QStringLiteral("NF "))) { return label; }
    }
    return nullptr;
}
}

class TestRemoteReceiveIndicators : public QObject {
    Q_OBJECT
private slots:
    void invalidFloorDoesNotDisplayAnOldMeasurement()
    {
        RadioModel radio(RadioModel::Role::Remote);
        RxApplet applet(nullptr, &radio);
        VfoWidget flag;
        applet.updateAgcAutoVisuals(true, -104.0f, 10.0, true);
        flag.updateAgcAutoVisuals(true, -104.0f, 10.0, true);
        QLabel* appletFloor = floorLabel(applet);
        QLabel* flagFloor = floorLabel(flag);
        QVERIFY(appletFloor && flagFloor);
        QVERIFY(appletFloor->text().contains(QStringLiteral("-104")));
        QCOMPARE(flagFloor->text(), appletFloor->text());
        applet.updateAgcAutoVisuals(true, -104.0f, 10.0, false);
        flag.updateAgcAutoVisuals(true, -104.0f, 10.0, false);
        QCOMPARE(appletFloor->text(), QStringLiteral("NF awaiting measurement"));
        QCOMPARE(flagFloor->text(), appletFloor->text());
        applet.updateAgcAutoVisuals(false, -104.0f, 10.0, false);
        flag.updateAgcAutoVisuals(false, -104.0f, 10.0, false);
        QVERIFY(appletFloor->isHidden());
        QVERIFY(flagFloor->isHidden());
    }

    void productionBindingKeepsFlagsPerSliceAndAppletOnActiveSlice()
    {
        RadioModel radio(RadioModel::Role::Remote);
        radio.setStationConnectionState(ConnectionState::Connected);
        radio.addSliceWithStationId(7);
        radio.addSliceWithStationId(23);
        auto* first = radio.sliceById(7);
        auto* second = radio.sliceById(23);
        QVERIFY(first && second);
        first->setAutoAgcEnabled(true);
        second->setAutoAgcEnabled(true);
        first->setStationAutoAgcNoiseFloor(-113.0, true, 1);
        second->setStationAutoAgcNoiseFloor(-87.0, true, 2);
        radio.applyStationActiveSlice(7);
        NoiseFloorTracker localFloor;
        for (int i = 0; i < 70; ++i) { localFloor.feed(QVector<float>(32, -40.0f), 33.0f); }
        radio.setNoiseFloorTracker(&localFloor);
        RxApplet applet(first, &radio);
        VfoWidget firstFlag, secondFlag;
        MainWindow::wireAutoAgcVisuals(&radio, first, &firstFlag, &applet);
        MainWindow::wireAutoAgcVisuals(&radio, second, &secondFlag, &applet);
        auto* firstLabel = floorLabel(firstFlag);
        auto* secondLabel = floorLabel(secondFlag);
        auto* appletLabel = floorLabel(applet);
        QVERIFY(firstLabel && secondLabel && appletLabel);
        QVERIFY(firstLabel->text().contains("-113"));
        QVERIFY(secondLabel->text().contains("-87"));
        QCOMPARE(appletLabel->text(), firstLabel->text());
        radio.applyStationActiveSlice(23);
        QCOMPARE(appletLabel->text(), secondLabel->text());
        first->setStationAutoAgcNoiseFloor(-109.0, true, 1);
        QVERIFY(firstLabel->text().contains("-109"));
        QVERIFY(appletLabel->text().contains("-87"));
        second->setStationAutoAgcNoiseFloor(-85.0, true, 2);
        QVERIFY(appletLabel->text().contains("-85"));
        radio.setStationConnectionState(ConnectionState::Disconnected);
        QCOMPARE(appletLabel->text(), QStringLiteral("NF awaiting measurement"));
        QCOMPARE(firstLabel->text(), appletLabel->text());
        QCOMPARE(secondLabel->text(), appletLabel->text());
        radio.setNoiseFloorTracker(nullptr);
    }

    void unavailableCoreFilterDialogDoesNotPresentRetainedState()
    {
        RadioModel remote(RadioModel::Role::Remote);
        remote.setStationConnectionState(ConnectionState::Connected);
        QVERIFY(remote.applyStationFilterValue("rxFilter1Mode", 2));
        QVERIFY(remote.applyStationFilterValue("rxFilter1Effective", 1));
        QVERIFY(remote.applyStationFilterValue("rxFilter1Band", 0));
        QVERIFY(remote.applyStationFilterValue("rxFilter1Reason", "retained Core override"));
        QVERIFY(!remote.filterChainStateAvailable(1));
        remote.setStationFilterSnapshotReady();
        QVERIFY(remote.filterChainStateAvailable(1));
        remote.clearStationFilterState();
        FilterPolicyDialog dialog(1, &remote);
        bool foundUnavailable = false;
        for (auto* label : dialog.findChildren<QLabel*>()) {
            QVERIFY(!label->text().contains("retained Core override"));
            foundUnavailable |= label->text().contains("not available");
        }
        QVERIFY(foundUnavailable);
        for (auto* button : dialog.findChildren<QRadioButton*>()) {
            QVERIFY(!button->isEnabled());
        }
    }

    void remoteFilterDialogUsesCoreStateAndCannotEditLocalPolicy()
    {
        RadioModel remote(RadioModel::Role::Remote);
        auto& alex = remote.alexControllerMutable();
        AlexController::AlexAdcState station;
        station.mode = AlexController::BpfMode::ForceBypass;
        station.effective = AlexController::BpfEffective::Bypass;
        station.reasonText = QStringLiteral("Core chain 1 override");
        FilterPolicyDialog dialog(1, &alex, nullptr, &station);
        bool foundReason = false;
        for (auto* label : dialog.findChildren<QLabel*>()) {
            foundReason |= label->text().contains(station.reasonText);
        }
        QVERIFY(foundReason);
        for (auto* button : dialog.findChildren<QRadioButton*>()) {
            QVERIFY(!button->isEnabled());
            if (button->text().startsWith(QStringLiteral("Force bypass"))) {
                QVERIFY(button->isChecked());
            }
        }
        for (auto* button : dialog.findChildren<QPushButton*>()) {
            if (button->text() == QStringLiteral("Close")) { button->click(); }
        }
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(alex.bpfMode(1), AlexController::BpfMode::Auto);
    }
};
QTEST_MAIN(TestRemoteReceiveIndicators)
#include "tst_remote_receive_indicators.moc"
