// no-port-check: NereusSDR-original. A connected desktop's CAT pages, applet
// and log window set up and show the Core's CAT.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tests/tst_cat_remote_setup.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port.
//
// A remote window (RadioModel::Role::Remote) connected to a real Core over
// loopback: its CAT pages, the CAT applet and the CAT log window read the
// Core's CAT through RadioModel::catControl() and change it through the
// stationCat commands. Loopback only; no radio.
//
// The design: docs/architecture/2026-10-07-remote-cat-setup-plan.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07  J.J. Boyd / KG4VCF  Created (CAT setup from a connected
//                                    desktop, the client side). Review
//                                    fixes: rebinds, test replies, signal
//                                    split, kept settings, log history,
//                                    refusals, device refresh, test gaps.
//                                    AI tooling: Claude Code.
//   2026-10-12  J.J. Boyd / KG4VCF  A refused change shows the rule it
//                                    broke. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-10-15  J.J. Boyd / KG4VCF  The CAT pages survive the Core's
//                                    echoes with accessibility on. AI-
//                                    assisted via Anthropic Claude Code.
//   2026-10-22, J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code:
//     test ports come from unusedLoopbackPort() (UnusedLoopbackPort.h), below
//     the kernel's ephemeral range, so a parallel test cannot take one between
//     the pick and the listen.
// =================================================================

#include "UnusedLoopbackPort.h"
#include "MultiDeviceHarness.h"
#include "SessionWait.h"

#include <QtTest/QtTest>
#include <QAbstractItemView>
#include <QAccessible>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformaccessibility.h>
#include <qpa/qplatformintegration.h>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTcpServer>
#include <QTcpSocket>

#include <memory>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/SliceOwnership.h"
#include "core/cat/CatControl.h"
#include "core/cat/CatService.h"
#include "core/cat/StationCatController.h"
#include "core/session/RecordStream.h"
#include "core/session/StationClient.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/MainWindowTestSettings.h"
#include "gui/MainWindow.h"
#include "gui/applets/CatApplet.h"
#include "gui/setup/CatLogWindow.h"
#include "gui/setup/CatNetworkSetupPages.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/StationCatModel.h"

using namespace NereusSDR;

namespace {

const QString kConnect = QStringLiteral("Connect to the Core to set up its CAT.");

quint16 freePort()
{
    return unusedLoopbackPort();
}

template<class T> T* control(QWidget& page, const char* name)
{
    return page.findChild<T*>(QString::fromLatin1(name));
}

// Does to a selector's list what the macOS accessibility bridge does when it
// drops its table rows (Qt 6.11 qcocoaaccessibilityelement.mm,
// removeElementsFromCache): each cell's interface is deleted while the list
// view's QAccessibleTable still caches its id. Returns the cells dropped.
int dropCellsAsTheMacBridgeDoes(QComboBox* box)
{
    QAccessibleInterface* table = QAccessible::queryAccessibleInterface(box->view());
    if (!table || !table->tableInterface()) { return 0; }
    QList<QAccessible::Id> cells;
    for (int i = 0; i < table->childCount(); ++i) {
        // A cell already dropped stays cached as a stale id and reads null.
        if (QAccessibleInterface* cell = table->child(i)) { cells.push_back(QAccessible::uniqueId(cell)); }
    }
    for (const QAccessible::Id id : cells) { QAccessible::deleteAccessibleInterface(id); }
    return int(cells.size());
}

// Turns the platform's accessibility on or off. Qt 6.11's
// QAccessible::setActive does that; Qt 6.8's (Linux CI) only tells the
// activation observers and leaves QAccessible::isActive() false, so there
// the platform's own switch is set, as Qt's tests do.
void setAccessibilityActive(bool active)
{
    QAccessible::setActive(active);
    if (QAccessible::isActive() == active) {
        return;
    }
    if (QPlatformAccessibility* platform = QGuiApplicationPrivate::platformIntegration()->accessibility()) {
        platform->setActive(active);
    }
}

// Accessibility on for one test, as VoiceOver or an accessibility query
// turns it on in the app; off again after.
struct AccessibilityOn {
    AccessibilityOn() { setAccessibilityActive(true); }
    ~AccessibilityOn() { setAccessibilityActive(false); }
    AccessibilityOn(const AccessibilityOn&) = delete;
    AccessibilityOn& operator=(const AccessibilityOn&) = delete;
};

// A remote window connected to a real Core over loopback.
struct Session {
    std::unique_ptr<Core> core;
    std::unique_ptr<RadioModel> window;
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;

    bool open(QObject* owner)
    {
        // The window's settings are migrated, as CoreInit leaves them.
        AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"),
                                         QStringLiteral("7"));
        core = std::make_unique<Core>(true);
        core->settings->setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
        core->model->catService()->startConfigured();
        window = std::make_unique<RadioModel>(RadioModel::Role::Remote);
        client = std::make_unique<StationClient>(window.get(), &proxy);
        window->attachStation(client.get());
        auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), owner);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), owner);
        stationEnd->linkTo(clientEnd);
        QSignalSpy completed(client.get(), &StationClient::handshakeComplete);
        client->startSession(clientEnd, core->server->token());
        core->server->acceptTransport(stationEnd);
        if (!completed.wait(5000) && completed.isEmpty()) { return false; }
        if (!QTest::qWaitFor([this] {
                return client->stationCatAvailable()
                    && !window->stationCatModel()->channelObject(1).isEmpty();
            }, 5000)) {
            return false;
        }
        // The window's CAT reads each delta once all of it is in: let it.
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
        return window->catControl() != nullptr && window->catControl()->available();
    }

    ~Session()
    {
        if (window) { window->detachStation(); }
        client.reset();
        window.reset();
        core.reset();
    }

    CatService& coreCat() const { return *core->model->catService(); }

    // The link drops and a new session opens with the same Core.
    bool reconnect(QObject* owner)
    {
        client->disconnectFromStation(QStringLiteral("test reconnect"));
        QCoreApplication::processEvents();
        auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end-again"), owner);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end-again"), owner);
        stationEnd->linkTo(clientEnd);
        QSignalSpy completed(client.get(), &StationClient::handshakeComplete);
        client->startSession(clientEnd, core->server->token());
        core->server->acceptTransport(stationEnd);
        if (!completed.wait(5000) && completed.isEmpty()) { return false; }
        return QTest::qWaitFor([this] { return window->catControl()->available(); }, 5000);
    }
};

} // namespace

class TstCatRemoteSetup : public QObject {
    Q_OBJECT

private slots:
    void init() { clearCat(); }
    void cleanup() { clearCat(); }

    // A remote window's TCP page changes channel 1's port and enables it;
    // the Core's CatService has it and listens, and the page shows it.
    void tcpPageChangesAndEnablesTheCoresChannel()
    {
        Session session;
        QVERIFY(session.open(this));
        CatTcpIpPage page(session.window.get());
        auto* enabled = control<QCheckBox>(page, "cat1Enabled");
        auto* port = control<QSpinBox>(page, "cat1Port");
        QVERIFY(enabled && port);
        QVERIFY(enabled->isEnabled());
        QVERIFY(port->isEnabled());
        const quint16 chosen = freePort();
        QVERIFY(chosen > 0);
        port->setValue(chosen);
        NEREUS_TRY_VERIFY(session.coreCat().channelConfig(1).tcpPort == int(chosen));
        NEREUS_TRY_VERIFY(session.window->catControl()->channelConfig(1).tcpPort == int(chosen));
        enabled->setChecked(true);
        NEREUS_TRY_VERIFY(session.coreCat().channelConfig(1).tcpEnabled);
        NEREUS_TRY_VERIFY(session.coreCat().isListening(1));
        NEREUS_TRY_VERIFY(control<QLabel>(page, "cat1Status")->text().contains(QString::number(chosen)));
        QVERIFY(control<QLabel>(page, "cat1Status")->text().contains(QStringLiteral("Listening")));
        QVERIFY(enabled->isChecked());
    }

    // A slice chosen in a remote window binds the Core's live slice.
    void sliceChosenRemotelyBindsTheCoresLiveSlice()
    {
        Session session;
        QVERIFY(session.open(this));
        const int second = session.core->model->addSlice(QStringLiteral("pan-0"));
        QVERIFY(second >= 0);
        NEREUS_TRY_VERIFY(session.window->sliceById(second) != nullptr);
        CatTcpIpPage page(session.window.get());
        auto* primary = control<QComboBox>(page, "cat1Primary");
        QVERIFY(primary);
        NEREUS_TRY_VERIFY(primary->findData(second) >= 0);
        QVERIFY(primary->currentData().toInt() != second);
        primary->setCurrentIndex(primary->findData(second));
        NEREUS_TRY_VERIFY(session.coreCat().channelConfig(1).binding.primarySliceId == second);
        QCOMPARE(session.coreCat().channelConfig(1).binding.primaryIncarnation,
                 session.core->model->sliceOwnership()->incarnation(second));
        NEREUS_TRY_VERIFY(primary->currentData().toInt() == second);
        QVERIFY(!primary->currentText().contains(QStringLiteral("Invalid")));
    }

    // The options page changes one of the Core's global settings.
    void optionsPageChangesTheCoresGlobalSetting()
    {
        Session session;
        QVERIFY(session.open(this));
        CatOptionsSetupPage options(session.window.get());
        auto* identity = control<QComboBox>(options, "catRigIdentity");
        auto* welcome = control<QCheckBox>(options, "catWelcome");
        QVERIFY(identity && welcome);
        QVERIFY(identity->isEnabled());
        identity->setCurrentText(QStringLiteral("TS-480"));
        NEREUS_TRY_VERIFY(session.coreCat().globalConfig().rigIdentity == QStringLiteral("TS-480"));
        welcome->setChecked(true);
        NEREUS_TRY_VERIFY(session.coreCat().globalConfig().sendWelcome);
        QCOMPARE(session.coreCat().globalConfig().rigIdentity, QStringLiteral("TS-480"));
    }

    // A change the Core refuses shows the Core's reason, and the page
    // shows the Core's settings again.
    void refusedChangeShowsTheCoresReason()
    {
        Session session;
        QVERIFY(session.open(this));
        CatTcpIpPage page(session.window.get());
        auto* enabled = control<QCheckBox>(page, "cat1Enabled");
        auto* port = control<QSpinBox>(page, "cat1Port");
        port->setValue(0);
        NEREUS_TRY_VERIFY(session.coreCat().channelConfig(1).tcpPort == 0);
        NEREUS_TRY_VERIFY(session.window->catControl()->channelConfig(1).tcpPort == 0);
        enabled->setChecked(true);
        // A listener turned on with no port: the Core says which rule.
        NEREUS_TRY_VERIFY(control<QLabel>(page, "cat1Status")->text()
                          == QStringLiteral("Invalid CAT listener address or port."));
        QVERIFY(!enabled->isChecked());
        QVERIFY(!session.coreCat().channelConfig(1).tcpEnabled);
    }

    // The CAT tester's reply comes back from the Core.
    void testerReplyComesBackFromTheCore()
    {
        Session session;
        QVERIFY(session.open(this));
        CatOptionsSetupPage options(session.window.get());
        auto* command = control<QLineEdit>(options, "catTesterCommand");
        auto* send = control<QPushButton>(options, "catTesterSend");
        auto* reply = control<QLabel>(options, "catTesterReply");
        QVERIFY(command && send && reply);
        QVERIFY(send->isEnabled());
        command->setText(QStringLiteral("ID;"));
        send->click();
        NEREUS_TRY_VERIFY(reply->text() == QStringLiteral("ID019;"));
        command->setText(QStringLiteral("TX;"));
        send->click();
        NEREUS_TRY_VERIFY(reply->text() == QStringLiteral("?;"));
        QVERIFY(control<QPushButton>(options, "catShowLog")->isEnabled());
    }

    // The log window fills from the Core's `catLog` stream while it is
    // open, and the window leaves the stream when it closes.
    void logWindowFillsFromTheStream()
    {
        Session session;
        QVERIFY(session.open(this));
        RecordStream* stream = session.core->server->recordStreamForTest(QStringLiteral("catLog"));
        QVERIFY(stream);
        QCOMPARE(stream->subscriberCount(), 0);
        auto log = std::make_unique<CatLogWindow>(session.window->catControl());
        NEREUS_TRY_VERIFY(stream->subscriberCount() == 1);
        auto* text = control<QPlainTextEdit>(*log, "catLogText");
        QVERIFY(text);
        QCOMPARE(session.coreCat().testCommand(1, "id;"), QByteArray("ID019;"));
        NEREUS_TRY_VERIFY(text->toPlainText().contains(QStringLiteral("in bytes=3  id;  [hex 69 64 3b]")));
        NEREUS_TRY_VERIFY(text->toPlainText().contains(
            QStringLiteral("out bytes=6  ID019;  [hex 49 44 30 31 39 3b]")));
        log.reset();
        NEREUS_TRY_VERIFY(stream->subscriberCount() == 0);
    }

    // The applet's TCP button switches the Core's CAT1 listener.
    void appletCatOneButtonWorksRemotely()
    {
        Session session;
        QVERIFY(session.open(this));
        CatEndpointConfig one = session.coreCat().channelConfig(1);
        one.tcpPort = freePort();
        QVERIFY(one.tcpPort > 0);
        QVERIFY(session.coreCat().reconfigureChannel(1, one));
        NEREUS_TRY_VERIFY(session.window->catControl()->channelConfig(1).tcpPort == one.tcpPort);
        CatApplet applet(session.window.get());
        auto* tcp = applet.findChild<QPushButton*>(QStringLiteral("catTcpButton"));
        QVERIFY(tcp);
        QVERIFY(tcp->isEnabled());
        QVERIFY(!tcp->isChecked());
        tcp->click();
        NEREUS_TRY_VERIFY(session.coreCat().channelConfig(1).tcpEnabled);
        NEREUS_TRY_VERIFY(session.coreCat().isListening(1));
        NEREUS_TRY_VERIFY(applet.findChild<QLabel*>(QStringLiteral("catTcpLed1"))
                              ->toolTip().contains(QStringLiteral("Listening")));
        QVERIFY(tcp->isChecked());
    }

    // With the link down every control is disabled and says why.
    void linkDownDisablesControlsWithTheReason()
    {
        RadioModel unlinked(RadioModel::Role::Remote);
        QVERIFY(unlinked.catControl());
        QVERIFY(!unlinked.catControl()->available());
        QCOMPARE(unlinked.catControl()->unavailableReason(), kConnect);
        QVERIFY(OperatorWording::isPlain(kConnect));
        CatTcpIpPage idle(&unlinked);
        QVERIFY(!control<QCheckBox>(idle, "cat1Enabled")->isEnabled());
        QCOMPARE(control<QCheckBox>(idle, "cat1Enabled")->toolTip(), kConnect);

        Session session;
        QVERIFY(session.open(this));
        CatTcpIpPage page(session.window.get());
        CatOptionsSetupPage options(session.window.get());
        CatPttSetupPage ptt(session.window.get());
        CatApplet applet(session.window.get());
        auto* enabled = control<QCheckBox>(page, "cat1Enabled");
        QVERIFY(enabled->isEnabled());
        session.window->detachStation();
        session.window->reportStationLinkStateChanged();
        QVERIFY(!session.window->catControl()->available());
        QVERIFY(!enabled->isEnabled());
        QCOMPARE(enabled->toolTip(), kConnect);
        QVERIFY(!control<QSpinBox>(page, "cat1Port")->isEnabled());
        QVERIFY(!control<QComboBox>(options, "catRigIdentity")->isEnabled());
        QCOMPARE(control<QComboBox>(options, "catRigIdentity")->toolTip(), kConnect);
        QVERIFY(!control<QPushButton>(options, "catTesterSend")->isEnabled());
        QVERIFY(!control<QCheckBox>(ptt, "catPttEnabled")->isEnabled());
        auto* tcp = applet.findChild<QPushButton*>(QStringLiteral("catTcpButton"));
        QVERIFY(!tcp->isEnabled());
        QCOMPARE(tcp->toolTip(), kConnect);
        bool explained = false;
        for (QLabel* label : page.findChildren<QLabel*>()) {
            explained = explained || label->text() == kConnect;
        }
        QVERIFY(explained);
    }

    // Finding 1: a slice closed and opened again with the same id, picked
    // in a remote window, binds the Core's live slice again.
    void sliceOpenedAgainWithItsIdRebindsWhenPicked()
    {
        Session session;
        QVERIFY(session.open(this));
        CatControl* cat = session.window->catControl();
        const int second = session.core->model->addSlice(QStringLiteral("pan-0"));
        QVERIFY(second >= 0);
        NEREUS_TRY_VERIFY(session.window->sliceById(second) != nullptr);
        CatTcpIpPage page(session.window.get());
        auto* primary = control<QComboBox>(page, "cat1Primary");
        auto* secondary = control<QComboBox>(page, "cat1Secondary");
        QVERIFY(primary && secondary);
        NEREUS_TRY_VERIFY(primary->findData(second) >= 0 && secondary->findData(second) >= 0);
        primary->setCurrentIndex(primary->findData(second));
        NEREUS_TRY_VERIFY(session.coreCat().channelConfig(1).binding.primarySliceId == second);
        NEREUS_TRY_VERIFY(secondary->findData(second) >= 0);
        secondary->setCurrentIndex(secondary->findData(second));
        NEREUS_TRY_VERIFY(session.coreCat().channelConfig(1).binding.secondarySliceId == second);
        NEREUS_TRY_VERIFY(cat->channelStatus(1).primaryValid && cat->channelStatus(1).secondaryValid
                          && cat->channelConfig(1).binding.secondarySliceId == second);

        session.core->model->removeSlice(second);
        NEREUS_TRY_VERIFY(!cat->channelStatus(1).primaryValid && !cat->channelStatus(1).secondaryValid);
        QCOMPARE(session.core->model->addSlice(QStringLiteral("pan-0")), second);
        NEREUS_TRY_VERIFY(session.window->sliceById(second) != nullptr);
        NEREUS_TRY_VERIFY(primary->currentText().startsWith(QStringLiteral("Invalid binding")));
        NEREUS_TRY_VERIFY(secondary->currentText().startsWith(QStringLiteral("Invalid binding")));
        // Still invalid on the Core: nothing was picked.
        QVERIFY(!cat->channelStatus(1).primaryValid);

        primary->setCurrentIndex(primary->findData(second));
        NEREUS_TRY_VERIFY(cat->channelStatus(1).primaryValid);
        QCOMPARE(session.coreCat().channelConfig(1).binding.primaryIncarnation,
                 session.core->model->sliceOwnership()->incarnation(second));
        // The secondary binding was not picked: it stays invalid.
        QVERIFY(!cat->channelStatus(1).secondaryValid);
        NEREUS_TRY_VERIFY(secondary->currentText().startsWith(QStringLiteral("Invalid binding")));
        secondary->setCurrentIndex(secondary->findData(second));
        NEREUS_TRY_VERIFY(cat->channelStatus(1).secondaryValid);
        QCOMPARE(session.coreCat().channelConfig(1).binding.secondaryIncarnation.value_or(0),
                 session.core->model->sliceOwnership()->incarnation(second));
        NEREUS_TRY_VERIFY(!primary->currentText().startsWith(QStringLiteral("Invalid binding"))
                          && !secondary->currentText().startsWith(QStringLiteral("Invalid binding")));
    }

    // Finding 2: two test commands sent at once (their changes reach the
    // window in one flush) each get their own reply.
    void twoTestsAtOnceEachGetTheirReply()
    {
        Session session;
        QVERIFY(session.open(this));
        CatControl* cat = session.window->catControl();
        QList<QByteArray> replies{QByteArray(), QByteArray()};
        QList<bool> ran{false, false};
        int answers = 0;
        cat->testCommand(1, "ID;", [&](bool done, const QByteArray& reply, const QString&) {
            ran[0] = done; replies[0] = reply; ++answers;
        });
        cat->testCommand(1, "TX;", [&](bool done, const QByteArray& reply, const QString&) {
            ran[1] = done; replies[1] = reply; ++answers;
        });
        NEREUS_TRY_VERIFY(answers == 2);
        QVERIFY(ran[0] && ran[1]);
        QCOMPARE(replies[0], QByteArray("ID019;"));
        QCOMPARE(replies[1], QByteArray("?;"));
        // Other windows still read the last test from the mirror.
        NEREUS_TRY_VERIFY(session.window->stationCatModel()->lastTestObject()
                              .value(QStringLiteral("reply")).toString() == QStringLiteral("?;"));
    }

    // Finding 4: text typed in an Options field and not finished survives
    // a CAT client connecting and a channel's change, in a local window.
    void typedOptionSurvivesChannelChangesLocally()
    {
        RadioModel model;
        model.addSlice();
        CatService& service = *model.catService();
        service.startConfigured();
        CatOptionsSetupPage options(&model);
        auto* serialNumber = control<QLineEdit>(options, "catSerialNumber");
        QVERIFY(serialNumber);
        serialNumber->setText(QStringLiteral("typed"));
        CatEndpointConfig one = service.channelConfig(1);
        one.tcpPort = freePort();
        one.tcpEnabled = true;
        one.tcpBindAddress = QStringLiteral("127.0.0.1");
        QVERIFY(service.reconfigureChannel(1, one));
        QVERIFY(service.isListening(1));
        QCOMPARE(serialNumber->text(), QStringLiteral("typed"));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, quint16(one.tcpPort));
        NEREUS_TRY_VERIFY(service.clientCount(1) == 1);
        QCOMPARE(serialNumber->text(), QStringLiteral("typed"));
    }

    // Finding 4: the same in a connected desktop.
    void typedOptionSurvivesChannelChangesRemotely()
    {
        Session session;
        QVERIFY(session.open(this));
        CatControl* cat = session.window->catControl();
        CatOptionsSetupPage options(session.window.get());
        auto* serialNumber = control<QLineEdit>(options, "catSerialNumber");
        QVERIFY(serialNumber);
        serialNumber->setText(QStringLiteral("typed"));
        CatEndpointConfig one = session.coreCat().channelConfig(1);
        one.tcpPort = freePort();
        one.tcpEnabled = true;
        one.tcpBindAddress = QStringLiteral("127.0.0.1");
        QVERIFY(session.coreCat().reconfigureChannel(1, one));
        NEREUS_TRY_VERIFY(cat->channelStatus(1).tcp == QStringLiteral("Listening"));
        QCOMPARE(serialNumber->text(), QStringLiteral("typed"));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, quint16(one.tcpPort));
        NEREUS_TRY_VERIFY(cat->channelStatus(1).tcpClients == 1);
        QCOMPARE(serialNumber->text(), QStringLiteral("typed"));
    }

    // Finding 6: a log window opened again shows the lines from before.
    void logWindowOpenedAgainShowsEarlierLines()
    {
        Session session;
        QVERIFY(session.open(this));
        RecordStream* stream = session.core->server->recordStreamForTest(QStringLiteral("catLog"));
        QVERIFY(stream);
        const QString line = QStringLiteral("in bytes=3  id;  [hex 69 64 3b]");
        auto log = std::make_unique<CatLogWindow>(session.window->catControl());
        NEREUS_TRY_VERIFY(stream->subscriberCount() == 1);
        QCOMPARE(session.coreCat().testCommand(1, "id;"), QByteArray("ID019;"));
        NEREUS_TRY_VERIFY(control<QPlainTextEdit>(*log, "catLogText")->toPlainText().contains(line));
        log.reset();
        NEREUS_TRY_VERIFY(stream->subscriberCount() == 0);
        log = std::make_unique<CatLogWindow>(session.window->catControl());
        auto* text = control<QPlainTextEdit>(*log, "catLogText");
        NEREUS_TRY_VERIFY(text->toPlainText().contains(line));
        QCOMPARE(text->toPlainText().count(line), 1);
    }

    // Finding 7: a test command the Core refuses is said on the page only.
    void refusedTestIsShownOnThePage()
    {
        Session session;
        QVERIFY(session.open(this));
        CatOptionsSetupPage options(session.window.get());
        options.show();
        auto* channel = control<QComboBox>(options, "catTesterChannel");
        auto* send = control<QPushButton>(options, "catTesterSend");
        auto* reply = control<QLabel>(options, "catTesterReply");
        QVERIFY(channel && send && reply);
        channel->addItem(QStringLiteral("5"));
        channel->setCurrentText(QStringLiteral("5"));
        QSignalSpy refused(session.window.get(), &RadioModel::accessoryRequestRefused);
        send->click();
        NEREUS_TRY_VERIFY(refused.count() == 1);
        QVERIFY(refused.at(0).at(2).toBool());
        NEREUS_TRY_VERIFY(reply->text() == refused.at(0).at(1).toString());
    }

    // Finding 8: a device path typed and not finished survives the device
    // list being filled again.
    void typedDevicePathSurvivesARefresh()
    {
        Session session;
        QVERIFY(session.open(this));
        CatSerialPortsPage page(session.window.get());
        auto* device = control<QComboBox>(page, "cat1Device");
        QVERIFY(device && device->lineEdit());
        device->lineEdit()->setText(QStringLiteral("/dev/typed"));
        QJsonObject platform = session.window->stationCatModel()->platformObject();
        platform.insert(QStringLiteral("serial"), true);
        platform.insert(QStringLiteral("serialDevices"),
                        QJsonArray{QStringLiteral("/dev/one"), QStringLiteral("/dev/two")});
        session.window->stationCatModel()->setPlatform(StationCatModel::toText(platform));
        NEREUS_TRY_VERIFY(device->findText(QStringLiteral("/dev/two")) >= 0);
        QCOMPARE(device->currentText(), QStringLiteral("/dev/typed"));
    }

    // Finding 5: an accepted change that makes no change leaves nothing
    // kept; one that does is kept until the Core's change arrives.
    void acceptedChangesLeaveNothingKept()
    {
        Session session;
        QVERIFY(session.open(this));
        auto* cat = qobject_cast<RemoteCatControl*>(session.window->catControl());
        QVERIFY(cat);
        int answers = 0;
        int keptAtAnswer = -1;
        cat->reconfigureGlobal(cat->globalConfig(), [&](bool accepted, const QString&) {
            QVERIFY(accepted);
            keptAtAnswer = cat->unconfirmedCount();
            ++answers;
        });
        QCOMPARE(cat->unconfirmedCount(), 1);
        NEREUS_TRY_VERIFY(answers == 1);
        QCOMPARE(keptAtAnswer, 0);
        QCOMPARE(cat->unconfirmedCount(), 0);

        CatEndpointConfig one = cat->channelConfig(1);
        one.serialBaud = one.serialBaud == 9600 ? 19200 : 9600;
        cat->reconfigureChannel(1, one, [&](bool accepted, const QString&) {
            QVERIFY(accepted);
            keptAtAnswer = cat->unconfirmedCount();
            ++answers;
        });
        // Until the Core's change arrives the window shows what was sent.
        QCOMPARE(cat->channelConfig(1).serialBaud, one.serialBaud);
        NEREUS_TRY_VERIFY(answers == 2);
        QCOMPARE(keptAtAnswer, 1);
        NEREUS_TRY_VERIFY(cat->unconfirmedCount() == 0);
        QCOMPARE(cat->channelConfig(1).serialBaud, one.serialBaud);
        QCOMPARE(session.coreCat().channelConfig(1).serialBaud, one.serialBaud);
    }

    // Finding 5: a change the Core refuses, or one left unanswered when the
    // link drops, is not kept.
    void refusedOrUnansweredChangesAreNotKept()
    {
        Session session;
        QVERIFY(session.open(this));
        auto* cat = qobject_cast<RemoteCatControl*>(session.window->catControl());
        QVERIFY(cat);
        const int port = cat->channelConfig(1).tcpPort;
        CatEndpointConfig refused = cat->channelConfig(1);
        refused.tcpEnabled = true;
        refused.tcpPort = 0;
        QSignalSpy configChanged(cat, &CatControl::channelConfigChanged);
        bool answered = false;
        cat->reconfigureChannel(1, refused, [&](bool accepted, const QString&) {
            QVERIFY(!accepted);
            answered = true;
        });
        QCOMPARE(cat->channelConfig(1).tcpPort, 0);
        NEREUS_TRY_VERIFY(answered);
        QCOMPARE(cat->unconfirmedCount(), 0);
        QCOMPARE(cat->channelConfig(1).tcpPort, port);
        // The pages show the Core's settings again.
        QVERIFY(!configChanged.isEmpty());
        QCOMPARE(configChanged.last().at(0).toInt(), 1);

        CatGlobalConfig global = cat->globalConfig();
        global.rigIdentity = global.rigIdentity == QStringLiteral("TS-480")
            ? QStringLiteral("TS-50S") : QStringLiteral("TS-480");
        cat->reconfigureGlobal(global, {});
        QCOMPARE(cat->unconfirmedCount(), 1);
        session.window->detachStation();
        session.window->reportStationLinkStateChanged();
        QCOMPARE(cat->unconfirmedCount(), 0);
        QVERIFY(cat->globalConfig().rigIdentity != global.rigIdentity);
    }

    // Finding 5: one delta carrying two properties shows both new values
    // the first time either is told.
    void deltaWithTwoPropertiesShowsBothAtOnce()
    {
        Session session;
        QVERIFY(session.open(this));
        CatControl* cat = session.window->catControl();
        CatEndpointConfig one = session.coreCat().channelConfig(1);
        one.serialBaud = one.serialBaud == 9600 ? 19200 : 9600;
        CatGlobalConfig global = session.coreCat().globalConfig();
        global.rigIdentity = global.rigIdentity == QStringLiteral("TS-480")
            ? QStringLiteral("TS-50S") : QStringLiteral("TS-480");
        QList<QPair<int, QString>> seen;
        const auto look = [&]() {
            seen.append({cat->channelConfig(1).serialBaud, cat->globalConfig().rigIdentity});
        };
        connect(cat, &CatControl::globalConfigChanged, this, look);
        connect(cat, &CatControl::channelConfigChanged, this, look);
        QSignalSpy status(cat, &CatControl::channelStatusChanged);
        QSignalSpy ptt(cat, &CatControl::pttStateChanged);
        // Both in one turn of the Core's loop: one flush.
        QVERIFY(session.coreCat().reconfigureChannel(1, one));
        QVERIFY(session.coreCat().reconfigureGlobal(global));
        NEREUS_TRY_VERIFY(seen.size() >= 2);
        QCOMPARE(seen.first().first, one.serialBaud);
        QCOMPARE(seen.first().second, global.rigIdentity);
        // Only the kinds that changed are told.
        QVERIFY(status.isEmpty());
        QVERIFY(ptt.isEmpty());
        disconnect(cat, nullptr, this, nullptr);
    }

    // Finding 9: the log window follows the Core's log again after a
    // reconnect, without showing earlier lines twice.
    void logFollowsAgainAfterAReconnect()
    {
        Session session;
        QVERIFY(session.open(this));
        RecordStream* stream = session.core->server->recordStreamForTest(QStringLiteral("catLog"));
        QVERIFY(stream);
        CatLogWindow log(session.window->catControl());
        auto* text = control<QPlainTextEdit>(log, "catLogText");
        NEREUS_TRY_VERIFY(stream->subscriberCount() == 1);
        const QString before = QStringLiteral("in bytes=3  id;  [hex 69 64 3b]");
        QCOMPARE(session.coreCat().testCommand(1, "id;"), QByteArray("ID019;"));
        NEREUS_TRY_VERIFY(text->toPlainText().contains(before));
        QVERIFY(session.reconnect(this));
        NEREUS_TRY_VERIFY(stream->subscriberCount() == 1);
        const QString after = QStringLiteral("in bytes=3  fa;  [hex 66 61 3b]");
        session.coreCat().testCommand(1, "fa;");
        NEREUS_TRY_VERIFY(text->toPlainText().contains(after));
        QCOMPARE(text->toPlainText().count(before), 1);
        QCOMPARE(text->toPlainText().count(after), 1);
    }

    // Finding 9: the serial devices, mark and space parity, 1.5 stop bits
    // and PTYs a remote window offers are the Core's.
    void platformChoicesComeFromTheCore()
    {
        Session session;
        QVERIFY(session.open(this));
        CatSerialPortsPage serial(session.window.get());
        CatTcpIpPage tcp(session.window.get());
        CatPttSetupPage ptt(session.window.get());
        const auto publish = [&session](bool serialPorts, bool pty, bool markSpace, bool oneAndHalf,
                                        const QStringList& devices) {
            session.core->model->stationCatModel()->setPlatform(StationCatModel::toText(QJsonObject{
                {QStringLiteral("serial"), serialPorts},
                {QStringLiteral("pty"), pty},
                {QStringLiteral("markSpaceParity"), markSpace},
                {QStringLiteral("oneAndHalfStop"), oneAndHalf},
                {QStringLiteral("serialDevices"), QJsonArray::fromStringList(devices)}}));
        };
        const auto itemEnabled = [](QComboBox* box, const QString& text) {
            auto* items = qobject_cast<QStandardItemModel*>(box->model());
            const int index = box->findText(text);
            return items && index >= 0 && items->item(index)->isEnabled();
        };
        auto* device = control<QComboBox>(serial, "cat1Device");
        auto* parity = control<QComboBox>(serial, "cat1Parity");
        auto* stops = control<QComboBox>(serial, "cat1Stops");
        auto* pty = control<QCheckBox>(tcp, "cat1Pty");
        auto* pttParity = control<QComboBox>(ptt, "catPttParity");
        QVERIFY(device && parity && stops && pty && pttParity);

        publish(true, false, false, false, {QStringLiteral("/dev/core-a"), QStringLiteral("/dev/core-b")});
        NEREUS_TRY_VERIFY(device->findText(QStringLiteral("/dev/core-b")) >= 0);
        QVERIFY(device->findText(QStringLiteral("/dev/core-a")) >= 0);
        QVERIFY(!itemEnabled(parity, QStringLiteral("Mark")));
        QVERIFY(!itemEnabled(parity, QStringLiteral("Space")));
        QVERIFY(!itemEnabled(stops, QStringLiteral("1.5")));
        QVERIFY(!itemEnabled(pttParity, QStringLiteral("Mark")));
        QVERIFY(!pty->isEnabled());
        QCOMPARE(pty->toolTip(), QStringLiteral(
            "The Core's computer has no native PTYs: they are available only on macOS and Linux."));

        publish(true, true, true, true, {QStringLiteral("/dev/core-c")});
        NEREUS_TRY_VERIFY(device->findText(QStringLiteral("/dev/core-c")) >= 0);
        QVERIFY(device->findText(QStringLiteral("/dev/core-a")) < 0);
        QVERIFY(itemEnabled(parity, QStringLiteral("Mark")));
        QVERIFY(itemEnabled(parity, QStringLiteral("Space")));
        QVERIFY(itemEnabled(stops, QStringLiteral("1.5")));
        QVERIFY(itemEnabled(pttParity, QStringLiteral("Mark")));
        QVERIFY(pty->isEnabled());

        // No serial ports on the Core: the serial channels and input PTT are off, with the reason.
        publish(false, true, true, true, {});
        NEREUS_TRY_VERIFY(!device->isEnabled());
        bool explained = false;
        for (QGroupBox* group : serial.findChildren<QGroupBox*>()) {
            explained = explained
                || group->toolTip() == QStringLiteral("The Core was built without serial port support.");
        }
        QVERIFY(explained);
        QVERIFY(!control<QCheckBox>(ptt, "catPttEnabled")->isEnabled());
    }

    // Finding 9: "Invalid binding" in a remote window follows the Core's
    // primaryValid and secondaryValid.
    void invalidBindingFollowsTheCore()
    {
        Session session;
        QVERIFY(session.open(this));
        CatTcpIpPage page(session.window.get());
        auto* primary = control<QComboBox>(page, "cat1Primary");
        auto* secondary = control<QComboBox>(page, "cat1Secondary");
        QVERIFY(primary && secondary);
        StationCatModel* core = session.core->model->stationCatModel();
        const auto publish = [core](bool primaryValid, bool secondaryValid) {
            QJsonObject channel = core->channelObject(1);
            QJsonObject config = channel.value(QStringLiteral("config")).toObject();
            config.insert(QStringLiteral("primarySliceId"), 0);
            config.insert(QStringLiteral("secondarySliceId"), 0);
            channel.insert(QStringLiteral("config"), config);
            channel.insert(QStringLiteral("primaryValid"), primaryValid);
            channel.insert(QStringLiteral("secondaryValid"), secondaryValid);
            core->setChannel(1, StationCatModel::toText(channel));
        };
        const QString invalid = QStringLiteral("Invalid binding (ID 0)");
        publish(false, true);
        NEREUS_TRY_VERIFY(primary->currentText() == invalid);
        QCOMPARE(secondary->currentText(), QStringLiteral("Slice ID 0"));
        auto* items = qobject_cast<QStandardItemModel*>(primary->model());
        QVERIFY(items && !items->item(primary->currentIndex())->isEnabled());
        QCOMPARE(primary->toolTip(), QStringLiteral("The slice this channel controlled was closed. Pick another slice."));
        publish(true, false);
        NEREUS_TRY_VERIFY(secondary->currentText() == invalid);
        QCOMPARE(primary->currentText(), QStringLiteral("Slice ID 0"));
        publish(true, true);
        NEREUS_TRY_VERIFY(secondary->currentText() == QStringLiteral("Slice ID 0"));
        QCOMPARE(primary->currentText(), QStringLiteral("Slice ID 0"));
    }

    // Finding 9: the status bar's CAT text and details in a remote window.
    void statusBarShowsTheCoresCat()
    {
        Session session;
        QVERIFY(session.open(this));
        CatControl* cat = session.window->catControl();
        CatIndicator indicator = cat->indicator();
        QCOMPARE(indicator.text, QStringLiteral("Off"));
        QCOMPARE(indicator.details.first(), QStringLiteral("CAT on the Core's computer:"));
        QCOMPARE(indicator.details.size(), 5);
        QVERIFY(indicator.details.at(1).startsWith(QStringLiteral("CAT1: ")));
        QSignalSpy status(cat, &CatControl::channelStatusChanged);
        CatEndpointConfig one = session.coreCat().channelConfig(1);
        one.tcpPort = freePort();
        one.tcpEnabled = true;
        one.tcpBindAddress = QStringLiteral("127.0.0.1");
        QVERIFY(session.coreCat().reconfigureChannel(1, one));
        NEREUS_TRY_VERIFY(cat->indicator().text == QStringLiteral("On (0)"));
        // The status bar is told.
        NEREUS_TRY_VERIFY(!status.isEmpty());
        QCOMPARE(cat->indicator().details.at(1),
                 QStringLiteral("CAT1: %1").arg(session.coreCat().channelState(1)));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, quint16(one.tcpPort));
        NEREUS_TRY_VERIFY(cat->indicator().text == QStringLiteral("On (1)"));
        client.abort();
        session.window->detachStation();
        session.window->reportStationLinkStateChanged();
        indicator = cat->indicator();
        QCOMPARE(indicator.text, QStringLiteral("Off"));
        QCOMPARE(indicator.details, (QStringList{QStringLiteral("CAT on the Core's computer:"), kConnect}));
        QVERIFY(OperatorWording::isPlain(indicator.details.join(QLatin1Char(' '))));
    }

    // Finding 9: MainWindow's "Show CAT Log" opens one log window and
    // shows it again. A remote MainWindow cannot be built in a unit test;
    // this is the local window, which keeps its log window when closed.
    void mainWindowShowsTheCatLog()
    {
        Test::markAudioFirstRunDone();
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        QVERIFY(QMetaObject::invokeMethod(&window, "showCatLog"));
        auto* log = window.findChild<CatLogWindow*>();
        QVERIFY(log);
        QVERIFY(log->isVisible());
        QVERIFY(!log->testAttribute(Qt::WA_DeleteOnClose));
        log->close();
        QVERIFY(QMetaObject::invokeMethod(&window, "showCatLog"));
        QCOMPARE(window.findChildren<CatLogWindow*>().size(), 1);
        QCOMPARE(window.findChild<CatLogWindow*>(), log);
        QVERIFY(log->isVisible());
    }

    // 2026-10-15 crashes: with accessibility on, each change made on the
    // TCP/IP CAT page came back from the Core and the window crashed
    // emptying the VFO slice selectors (QAccessibleTable::modelChange read
    // a cell the macOS bridge had deleted). The page survives the echoes,
    // a slice opened on the Core and an invalid binding, and still shows
    // each one.
    void tcpPageSurvivesEchoesWithAccessibilityOn()
    {
        Session session;
        QVERIFY(session.open(this));
        CatTcpIpPage page(session.window.get());
        auto* primary = control<QComboBox>(page, "cat1Primary");
        auto* port = control<QSpinBox>(page, "cat1Port");
        auto* rigAddress = control<QLineEdit>(page, "cat1RigctldAddress");
        QVERIFY(primary && port && rigAddress);
        const AccessibilityOn accessibility;
        QVERIFY(QAccessible::isActive());
        const auto dropAllCells = [&page] {
            int dropped = 0;
            for (QComboBox* box : page.findChildren<QComboBox*>()) {
                if (box->objectName().endsWith(QStringLiteral("Primary"))
                    || box->objectName().endsWith(QStringLiteral("Secondary"))) {
                    dropped += dropCellsAsTheMacBridgeDoes(box);
                }
            }
            return dropped;
        };
        QVERIFY(dropAllCells() > 0);

        // Each change comes back from the Core and fills the page again.
        for (int round = 0; round < 3; ++round) {
            const quint16 chosen = freePort();
            QVERIFY(chosen > 0);
            port->setValue(chosen);
            NEREUS_TRY_VERIFY(session.window->catControl()->channelConfig(1).tcpPort == int(chosen));
            QCoreApplication::processEvents();
            QCOMPARE(port->value(), int(chosen));
            dropAllCells();
        }
        const QString address = QStringLiteral("127.0.0.1");
        rigAddress->setText(address);
        emit rigAddress->editingFinished();
        NEREUS_TRY_VERIFY(session.window->catControl()->channelConfig(1).rigctldBindAddress == address);
        QCoreApplication::processEvents();
        QCOMPARE(rigAddress->text(), address);
        dropAllCells();

        // A slice opened on the Core lists one more slice.
        const int second = session.core->model->addSlice(QStringLiteral("pan-0"));
        QVERIFY(second >= 0);
        NEREUS_TRY_VERIFY(primary->findData(second) >= 0);
        QCOMPARE(primary->currentData().toInt(), session.window->catControl()->channelConfig(1).binding.primarySliceId);
        dropAllCells();

        // An invalid binding adds its own entry, disabled.
        StationCatModel* core = session.core->model->stationCatModel();
        QJsonObject channel = core->channelObject(1);
        QJsonObject config = channel.value(QStringLiteral("config")).toObject();
        config.insert(QStringLiteral("primarySliceId"), 0);
        channel.insert(QStringLiteral("config"), config);
        channel.insert(QStringLiteral("primaryValid"), false);
        core->setChannel(1, StationCatModel::toText(channel));
        NEREUS_TRY_VERIFY(primary->currentText() == QStringLiteral("Invalid binding (ID 0)"));
        auto* items = qobject_cast<QStandardItemModel*>(primary->model());
        QVERIFY(items && !items->item(primary->currentIndex())->isEnabled());
        dropAllCells();
        channel.insert(QStringLiteral("primaryValid"), true);
        core->setChannel(1, StationCatModel::toText(channel));
        NEREUS_TRY_VERIFY(primary->currentText() == QStringLiteral("Slice ID 0"));
        QVERIFY(items->item(primary->currentIndex())->isEnabled());
        QVERIFY(primary->findText(QStringLiteral("Invalid binding (ID 0)")) < 0);
    }

    // The serial page's device list, filled again from the Core with
    // accessibility on, is not emptied row by row either.
    void serialDeviceListSurvivesAccessibilityOn()
    {
        Session session;
        QVERIFY(session.open(this));
        CatSerialPortsPage page(session.window.get());
        auto* device = control<QComboBox>(page, "cat1Device");
        QVERIFY(device);
        const auto publish = [&session](const QStringList& devices) {
            QJsonObject platform = session.window->stationCatModel()->platformObject();
            platform.insert(QStringLiteral("serial"), true);
            platform.insert(QStringLiteral("serialDevices"), QJsonArray::fromStringList(devices));
            session.core->model->stationCatModel()->setPlatform(StationCatModel::toText(platform));
        };
        publish({QStringLiteral("/dev/one"), QStringLiteral("/dev/two")});
        NEREUS_TRY_VERIFY(device->findText(QStringLiteral("/dev/two")) >= 0);
        const AccessibilityOn accessibility;
        QVERIFY(dropCellsAsTheMacBridgeDoes(device) > 0);
        publish({QStringLiteral("/dev/three")});
        NEREUS_TRY_VERIFY(device->findText(QStringLiteral("/dev/three")) >= 0);
        QVERIFY(device->findText(QStringLiteral("/dev/one")) < 0);

        // A device typed on the page comes back from the Core and is added to the list.
        QVERIFY(dropCellsAsTheMacBridgeDoes(device) > 0);
        const QString typed = QStringLiteral("/dev/typed");
        device->lineEdit()->setText(typed);
        emit device->lineEdit()->editingFinished();
        NEREUS_TRY_VERIFY(session.window->catControl()->channelConfig(1).serialDevice == typed);
        NEREUS_TRY_VERIFY(device->findText(typed) >= 0);
        QCOMPARE(device->currentText(), typed);
    }

private:
    static void clearCat()
    {
        for (const QString& key : AppSettings::instance().allKeys()) {
            if (key.startsWith(QStringLiteral("Cat/"))) {
                AppSettings::instance().remove(key);
            }
        }
    }
};

QTEST_MAIN(TstCatRemoteSetup)
#include "tst_cat_remote_setup.moc"
