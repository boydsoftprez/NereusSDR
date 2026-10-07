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
//                                    desktop, the client side).
//                                    AI tooling: Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"
#include "SessionWait.h"

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTcpServer>

#include <memory>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/SliceOwnership.h"
#include "core/cat/CatControl.h"
#include "core/cat/CatService.h"
#include "core/cat/StationCatController.h"
#include "core/session/StationClient.h"
#include "core/settings/SettingsProxy.h"
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
    QTcpServer reservation;
    if (!reservation.listen(QHostAddress::LocalHost, 0)) { return 0; }
    const quint16 port = reservation.serverPort();
    reservation.close();
    return port;
}

template<class T> T* control(QWidget& page, const char* name)
{
    return page.findChild<T*>(QString::fromLatin1(name));
}

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
        NEREUS_TRY_VERIFY(control<QLabel>(page, "cat1Status")->text()
                          == StationCatController::channelRefusedReason());
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
