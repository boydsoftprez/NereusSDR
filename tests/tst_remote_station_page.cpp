// no-port-check: NereusSDR-original Remote Access presentation contract.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gui/setup/RemoteStationPage.h"
#include "gui/GuiDesktopStationRuntime.h"
#include "core/session/RemoteDevicesState.h"
#include "gui/multidevice/ConnectedDevicesList.h"
#include <QTreeWidget>

#include <QCheckBox>
#include <QDir>
#include <QEvent>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

using NereusSDR::RemoteStationPage;

class RemoteStationPageTest : public QObject {
    Q_OBJECT
private:
    static RemoteStationPage::State ready()
    {
        RemoteStationPage::State state;
        state.available = true;
        state.runCore = true;
        state.stationName = QStringLiteral("My station");
        state.reachabilityText = QStringLiteral("Found by devices on this network.");
        state.keyBackupPath = QStringLiteral("/Users/operator/.config/nereus/station.key");
        state.devices = {{QByteArray("device-a"), QStringLiteral("Phone A"),
                          QStringLiteral("Monday"), QStringLiteral("Today"), true}};
        return state;
    }
    static QPushButton* button(RemoteStationPage& page, const char* name)
    {
        const QList<QPushButton*> matches = page.findChildren<QPushButton*>(QString::fromLatin1(name));
        Q_ASSERT(!matches.isEmpty());
        return matches.constLast();
    }
    static QCheckBox* check(RemoteStationPage& page, const char* name)
    {
        QCheckBox* result = page.findChild<QCheckBox*>(QString::fromLatin1(name));
        Q_ASSERT(result);
        return result;
    }
    static void screenshot(RemoteStationPage& page, const QString& file)
    {
        const QString directory = qEnvironmentVariable("NEREUS_REMOTE_PAGE_SHOTS");
        if (directory.isEmpty()) { return; }
        QVERIFY(QDir().mkpath(directory));
        page.resize(760, 760);
        page.show();
        QCoreApplication::processEvents();
        QVERIFY(page.grab().save(directory + QLatin1Char('/') + file));
    }
private slots:
    void connectionsStayAvailableOutsideHostingAuthority()
    {
        RemoteStationPage page;
        auto* connections = button(page, "remoteStationConnections");
        auto* section = qobject_cast<QGroupBox*>(connections->parentWidget());
        QVERIFY(section);
        QCOMPARE(section->title(), QStringLiteral("This window"));
        QSignalSpy requests(&page, &RemoteStationPage::connectionsRequested);
        QSignalSpy run(&page, &RemoteStationPage::runCoreRequested);
        QSignalSpy keep(&page, &RemoteStationPage::keepRunningRequested);

        auto state = ready();
        state.available = false;
        state.runCore = false;
        page.setState(state);
        QVERIFY(!check(page, "remoteAccessRunCore")->isEnabled());
        QVERIFY(connections->isEnabled());
        connections->click();

        state.available = true;
        state.busy = true;
        page.setState(state);
        QVERIFY(connections->isEnabled());
        connections->click();

        state.busy = false;
        state.transmitting = true;
        page.setState(state);
        QVERIFY(!check(page, "remoteAccessRunCore")->isEnabled());
        QVERIFY(connections->isEnabled());
        connections->click();

        QCOMPARE(requests.size(), 3);
        QCOMPARE(run.size(), 0);
        QCOMPARE(keep.size(), 0);
    }

    void actionsStayAuthoritative()
    {
        RemoteStationPage page;
        RemoteStationPage::State state = ready();
        QSignalSpy run(&page, &RemoteStationPage::runCoreRequested);
        QSignalSpy keep(&page, &RemoteStationPage::keepRunningRequested);
        QSignalSpy start(&page, &RemoteStationPage::startWithComputerRequested);
        QSignalSpy rename(&page, &RemoteStationPage::renameRequested);
        QSignalSpy revoke(&page, &RemoteStationPage::revokeRequested);
        QSignalSpy add(&page, &RemoteStationPage::addDeviceRequested);
        QSignalSpy backup(&page, &RemoteStationPage::keyBackupAcknowledgedRequested);
        page.setState(state);
        QCOMPARE(run.size() + keep.size() + start.size() + rename.size()
                 + revoke.size() + add.size() + backup.size(), 0);
        check(page, "remoteAccessRunCore")->click();
        QCOMPARE(run.takeFirst().at(0).toBool(), false);
        QVERIFY(check(page, "remoteAccessRunCore")->isChecked());
        QVERIFY(page.state().runCore);
        check(page, "remoteAccessKeepRunning")->click();
        QCOMPARE(keep.takeFirst().at(0).toBool(), true);
        QVERIFY(!page.state().keepRunning);
        QVERIFY(!check(page, "remoteAccessKeepRunning")->isChecked());
        check(page, "remoteAccessStartWithComputer")->click();
        QCOMPARE(start.takeFirst().at(0).toBool(), true);
        QVERIFY(!page.state().startWithComputer);
        QVERIFY(!check(page, "remoteAccessStartWithComputer")->isChecked());
        button(page, "remoteAccessAddDevice")->click();
        QCOMPARE(add.size(), 1);
        button(page, "remoteAccessBackupAcknowledged")->click();
        QCOMPARE(backup.size(), 1);
        QPushButton* revokeButton = button(page, "remoteAccessRevoke");
        revokeButton->click();
        QCOMPARE(revoke.takeFirst().at(0).toByteArray(), QByteArray("device-a"));
        QTimer::singleShot(0, [] {
            QInputDialog* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
            Q_ASSERT(dialog);
            dialog->setTextValue(QStringLiteral("  New station  "));
            dialog->accept();
        });
        button(page, "remoteAccessRename")->click();
        QCOMPARE(rename.takeFirst().at(0).toString(), QStringLiteral("New station"));
        QCOMPARE(page.state().stationName, QStringLiteral("My station"));
        QTimer::singleShot(0, [] {
            QInputDialog* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
            Q_ASSERT(dialog);
            dialog->reject();
        });
        button(page, "remoteAccessRename")->click();
        QCOMPARE(rename.size(), 0);
        state.stationName = QStringLiteral("New station");
        state.keepRunning = true;
        state.startWithComputer = true;
        page.setState(state);
        QCOMPARE(run.size() + keep.size() + start.size() + rename.size()
                 + revoke.size() + add.size() + backup.size(), 2);
        QVERIFY(check(page, "remoteAccessKeepRunning")->isChecked());
        QVERIFY(check(page, "remoteAccessStartWithComputer")->isChecked());
    }
    void gatesAndReplacement()
    {
        RemoteStationPage page;
        QSignalSpy revoke(&page, &RemoteStationPage::revokeRequested);
        QSignalSpy run(&page, &RemoteStationPage::runCoreRequested);
        RemoteStationPage::State state = ready();
        page.setState(state);
        QPushButton* stale = button(page, "remoteAccessRevoke");
        state.devices = {{QByteArray("device-b"), QStringLiteral("Phone B"),
                          QStringLiteral("Yesterday"), QStringLiteral("Today"), false}};
        page.setState(state);
        stale->click();
        QCOMPARE(revoke.size(), 0);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QPushButton* current = button(page, "remoteAccessRevoke");
        QVERIFY(!current->isEnabled());
        QVERIFY(!current->accessibleDescription().isEmpty());
        state.devices[0].revocable = true;
        page.setState(state);
        button(page, "remoteAccessRevoke")->click();
        QCOMPARE(revoke.takeFirst().at(0).toByteArray(), QByteArray("device-b"));
        state.available = false;
        page.setState(state);
        QVERIFY(!check(page, "remoteAccessRunCore")->isEnabled());
        QVERIFY(!button(page, "remoteAccessAddDevice")->isEnabled());
        QVERIFY(!button(page, "remoteAccessRevoke")->isEnabled());
        QVERIFY(page.findChild<QLabel*>("remoteAccessReason")->text().contains("local radio"));
        state.available = true;
        state.busy = true;
        page.setState(state);
        QVERIFY(!check(page, "remoteAccessRunCore")->isEnabled());
        state.busy = false;
        state.transmitting = true;
        page.setState(state);
        QVERIFY(!check(page, "remoteAccessRunCore")->isEnabled());
        state.transmitting = false;
        state.runCore = false;
        page.setState(state);
        QVERIFY(check(page, "remoteAccessRunCore")->isEnabled());
        QVERIFY(!check(page, "remoteAccessKeepRunning")->isEnabled());
        QVERIFY(!check(page, "remoteAccessStartWithComputer")->isEnabled());
        QVERIFY(!button(page, "remoteAccessRename")->isEnabled());
        check(page, "remoteAccessRunCore")->click();
        QCOMPARE(run.takeFirst().at(0).toBool(), true);
        QVERIFY(!page.state().runCore);
    }
    void renameCannotOutliveSetupPermission()
    {
        RemoteStationPage page;
        auto state = ready();
        page.setState(state);
        QSignalSpy rename(&page, &RemoteStationPage::renameRequested);
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            state.transmitting = true;
            page.setState(state);
            dialog->setTextValue(QStringLiteral("Rename while transmitting"));
            dialog->accept();
        });
        button(page, "remoteAccessRename")->click();
        QCOMPARE(rename.size(), 0);
    }
    // Slice control plan Task 8b: removing a computer that joined with the
    // pairing token asks first, in plain words that say what stopping the
    // token does, and goes ahead only when the operator agrees.
    void removingAComputerThatJoinedWithTheTokenAsksFirst()
    {
        RemoteStationPage page;
        RemoteStationPage::State state = ready();
        RemoteStationPage::Device joined;
        joined.id = QByteArray("mac-radxa");
        joined.name = QStringLiteral("MacBook-Pro (radxa_5c_r3)");
        joined.pairedText = QStringLiteral("September 25, 2026");
        joined.lastSeenText = QStringLiteral("Today");
        joined.removalStopsPairingToken = true;
        state.devices.append(joined);
        page.setState(state);
        QString asked;
        QString goAheadWords;
        bool answer = false;
        page.setConfirmation([&](const QString& title, const QString& text, const QString& goAhead) {
            asked = title + QLatin1Char('\n') + text;
            goAheadWords = goAhead;
            return answer;
        });
        QSignalSpy revoke(&page, &RemoteStationPage::revokeRequested);
        QSignalSpy stopping(&page, &RemoteStationPage::revokeStoppingPairingTokenRequested);

        button(page, "remoteAccessRevoke")->click();
        QVERIFY(asked.contains(QStringLiteral("MacBook-Pro (radxa_5c_r3)")));
        QVERIFY(asked.contains(QStringLiteral("stops accepting")));
        QVERIFY(asked.contains(QStringLiteral("Paired devices keep working")));
        QVERIFY(asked.contains(QStringLiteral("This is permanent")));
        QVERIFY(goAheadWords.contains(QStringLiteral("Pairing Token")));
        QCOMPARE(stopping.size(), 0);
        QCOMPARE(revoke.size(), 0);

        answer = true;
        button(page, "remoteAccessRevoke")->click();
        QCOMPARE(stopping.size(), 1);
        QCOMPARE(stopping.takeFirst().at(0).toByteArray(), QByteArray("mac-radxa"));
        QCOMPARE(revoke.size(), 0);

        // The last way in: disabled with the Core's own reason.
        state.devices = {joined};
        state.devices.first().revocable = false;
        state.devices.first().revokeReason =
            QStringLiteral("Pair another device first, or reset this Core from its own computer.");
        page.setState(state);
        QPushButton* last = button(page, "remoteAccessRevoke");
        QVERIFY(!last->isEnabled());
        QCOMPARE(last->toolTip(), state.devices.first().revokeReason);
    }

    void renderStates()
    {
        RemoteStationPage page;
        RemoteStationPage::State state;
        page.setState(state);
        screenshot(page, QStringLiteral("01-off.png"));
        state = ready();
        state.stationName = QStringLiteral("The extremely long station name for a desktop radio in the northern workshop");
        state.keyBackupPath = QStringLiteral("/Users/operator/Documents/Very Long Folder Name/Nereus SDR/Profiles/Workshop Station/station-identity-key.pem");
        state.pairingOpen = true;
        state.pairingCode = QStringLiteral("amber-pine-harbor");
        state.devices.clear();
        page.setState(state);
        screenshot(page, QStringLiteral("02-on-pairing.png"));
        state.pairingOpen = false;
        state.devices = {{QByteArray("phone"), QStringLiteral("A very long phone name from the main desk"),
                          QStringLiteral("September 27, 2026"), QStringLiteral("Today"), true},
                         {QByteArray("tablet"), QStringLiteral("Workshop tablet"),
                          QStringLiteral("September 20, 2026"), QStringLiteral("Yesterday"), false}};
        page.setState(state);
        screenshot(page, QStringLiteral("03-two-devices.png"));
        state.keyBackupAcknowledged = true;
        page.setState(state);
        screenshot(page, QStringLiteral("04-backup-acknowledged.png"));
        // iPhone app plan Tasks 49 and 78 item 8: the reach line as the
        // runtime words it for a Core that is fully reachable.
        NereusSDR::StationReach reach;
        reach.listening = true;
        reach.bonjourAvailable = true;
        reach.bonjourActive = true;
        reach.serviceConfigured = true;
        reach.serviceRegistered = true;
        reach.serviceHost = QStringLiteral("rv.nereussdr.com");
        state.reachabilityText = NereusSDR::GuiDesktopStationRuntime::reachText(QString(), 50055, reach);
        page.setState(state);
        screenshot(page, QStringLiteral("05-reach.png"));
        // Task 78 item 8: who is connected now, this desktop first.
        QCOMPARE(page.connectedList()->emptyLabel()->text(),
                 QStringLiteral("Run a Core on this computer to see who is connected."));
        NereusSDR::RemoteDevicesState devices;
        NereusSDR::MirrorUpdate list;
        list.name = QByteArrayLiteral("listJson");
        list.value = QStringLiteral(
            "[{\"deviceId\":\"host\",\"name\":\"Shack Mac mini\",\"shortName\":\"Mac mini\","
            "\"hostsCore\":true,\"state\":\"listening\",\"connectedForSeconds\":5400,"
            "\"listeningOn\":[{\"sliceId\":0,\"letter\":\"A\",\"band\":5}]},"
            "{\"deviceId\":\"phone\",\"name\":\"Jo's iPhone\",\"shortName\":\"iPhone\","
            "\"state\":\"transmitting\",\"holdsTransmit\":true,\"connectedForSeconds\":900,"
            "\"transmittingForSeconds\":75,\"transmittingOn\":{\"sliceId\":1,\"letter\":\"B\","
            "\"band\":5}}]");
        NereusSDR::MirrorUpdate limit;
        limit.name = QByteArrayLiteral("deviceLimit");
        limit.value = 4;
        devices.setSelfDeviceId(QStringLiteral("host"));
        devices.applyObject(QByteArrayLiteral("connectedDevices"), {list, limit});
        page.setConnectedDevices(&devices);
        QCOMPARE(page.connectedList()->tree()->topLevelItemCount(), 2);
        QVERIFY(page.connectedList()->tree()->topLevelItem(0)->text(0).endsWith(
            QStringLiteral("(this window)")));
        screenshot(page, QStringLiteral("06-connected-now.png"));
        page.setConnectedDevices(nullptr);
        // Task 8b: two profiles on one computer, told apart.
        RemoteStationPage::Device profiled{QByteArray("mac-radxa"),
                                           QStringLiteral("MacBook-Pro (radxa_5c_r3)"),
                                           QStringLiteral("September 25, 2026"),
                                           QStringLiteral("Today"), true};
        profiled.removalStopsPairingToken = true;
        state.devices = {{QByteArray("mac"), QStringLiteral("MacBook-Pro"),
                          QStringLiteral("September 28, 2026"), QStringLiteral("Today"), true},
                         profiled};
        page.setState(state);
        screenshot(page, QStringLiteral("07-two-profiles.png"));
    }
};
QTEST_MAIN(RemoteStationPageTest)
#include "tst_remote_station_page.moc"
