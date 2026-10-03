// no-port-check: NereusSDR-original. R-R3-48 / R-R3-25 the Core's station
// TCI server and the app's one TCI switch.
//
// The Core's server: where it listens (the station network facing the
// radio, nereusd.conf's override, and this computer), the switch and port
// it keeps in its own settings, and what a TCI app on the station network
// (the stand-in for the RF-Kit amplifier) receives from the Core's radio:
// the init burst saying receive-only, split_enable, and vfo: as the Core's
// slice moves. Band follow for the RF-Kit over that server. The window
// side: the one switch starts this window's server and asks the Core for
// the same, runs none of its own when the Core is on this computer, and
// the TCI page's "Also at the station" line.
//
// Loopback sockets only; synthetic interface entries stand in for the
// station network. No radio, amplifier or real Core is contacted.
// J.J. Boyd (KG4VCF), September 2026; AI-assisted via Anthropic Claude Code.
// 2026-09-25: iPhone app plan Task 73 (R-IOS-02, ruling 5.13): the Core's
// server reads every slice and changes only the station device's own. J.J.
// Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-25: iPhone app plan Task 35 (R-IOS-13, ruling 8.14): on a Core that
// allows remote transmit, the Core's own server still never keys, with
// transmit unheld or held by a device, and never releases a device's key.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-27: Parity Task 23 options, client records and remote controls,
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-09-30: fix wave (INFRA minor 1): the listener retry test waits for a
// retry's own log line instead of a fixed 2.5 s sleep. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// 2026-09-30: fix wave round 1: that test turns the TCI log category on
// through LogManager and puts it back the same way, so the process keeps
// LogManager's filter rules instead of an empty set. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
#include "MultiDeviceHarness.h"

#include <QtTest/QtTest>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QWebSocket>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/SliceOwnership.h"
#include "core/RfKitBandFollow.h"
#include "core/StationNetwork.h"
#include "core/StationTciController.h"
#include "core/TciServer.h"
#include "core/TciSwitch.h"
#include "core/session/IStationLink.h"
#include "core/session/SessionCommandDispatcher.h"
#include "models/RadioModel.h"
#include "models/RfKitModel.h"
#include "models/SliceModel.h"
#include "models/StationTciModel.h"
#include "gui/applets/TciApplet.h"
#include "gui/setup/CatNetworkSetupPages.h"

using namespace NereusSDR;
using BandFollow = TunerModel::BandFollow;

namespace {

quint16 freePort()
{
    QTcpServer reservation;
    if (!reservation.listen(QHostAddress::LocalHost, 0)) { return 0; }
    const quint16 port = reservation.serverPort();
    reservation.close();
    return port;
}

QNetworkAddressEntry entry(const char* ip, int prefix)
{
    QNetworkAddressEntry e;
    e.setIp(QHostAddress(QString::fromLatin1(ip)));
    e.setPrefixLength(prefix);
    return e;
}

// An app on the station network: every text frame the server sends it.
struct TciApp {
    QWebSocket socket;
    QStringList frames;
    explicit TciApp(quint16 port)
    {
        QObject::connect(&socket, &QWebSocket::textMessageReceived, &socket,
                         [this](const QString& text) {
            for (const QString& part : text.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
                frames.append(part.trimmed() + QLatin1Char(';'));
            }
        });
        socket.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port)));
    }
    bool has(const QString& frame) const { return frames.contains(frame); }
};

class FakeStationLink final : public IStationLink {
public:
    bool tciAvailable{true};
    bool coreHere{false};
    bool ready{true};
    bool serverVersion2{true};
    int stored{1};   // the Core has a stored station switch (-1: not known yet)
    int requests{0};
    bool requestedOn{false};
    quint16 requestedPort{0};
    int optionRequests{0};
    bool requestedExpert{true};
    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
    bool stationLinkReady() const override { return ready; }
    bool stationTciAvailable() const override { return ready && tciAvailable; }
    bool stationTciServerAvailable() const override
    { return stationTciAvailable() && serverVersion2; }
    bool coreServesTciOnThisComputer() const override { return coreHere; }
    int coreStationTciStored() const override { return stored; }
    CommandOutcome requestStationTci(bool on, quint16 port) override
    {
        ++requests;
        requestedOn = on;
        requestedPort = port;
        return {true, {}, quint32(100 + requests)};   // command ids 101, 102, ...
    }
    CommandOutcome requestStationTciOptions(bool expert, bool, bool, bool) override
    {
        ++optionRequests;
        requestedExpert = expert;
        return {true, {}, quint32(200 + optionRequests)};
    }
    // JJ's ruling of 2026-09-28 (stationTciSettingsVersion 1).
    bool settingsAvailable{true};
    QList<std::pair<QByteArray, QVariant>> settingRequests;
    bool stationTciSettingsAvailable() const override
    { return stationTciServerAvailable() && settingsAvailable; }
    CommandOutcome requestStationTciSetting(const QByteArray& name, const QVariant& value) override
    {
        settingRequests.append({name, value});
        return {true, {}, quint32(300 + settingRequests.size())};
    }
};

} // namespace

class StationTciServerTest : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // The station network is this computer's address on the radio's subnet.
    void stationAddressFacesTheRadio()
    {
        const QList<QNetworkAddressEntry> entries{
            entry("10.8.0.4", 24), entry("192.168.1.20", 24), entry("fe80::1", 64)};
        QCOMPARE(StationNetwork::addressFacing(QHostAddress(QStringLiteral("192.168.1.50")), entries),
                 QHostAddress(QStringLiteral("192.168.1.20")));
        QCOMPARE(StationNetwork::addressFacing(QHostAddress(QStringLiteral("10.8.0.99")), entries),
                 QHostAddress(QStringLiteral("10.8.0.4")));
        QVERIFY(StationNetwork::addressFacing(QHostAddress(QStringLiteral("172.16.0.9")), entries)
                    .isNull());
        QCOMPARE(StationNetwork::addressFacing(QHostAddress(QStringLiteral("::ffff:192.168.1.7")),
                                               entries),
                 QHostAddress(QStringLiteral("192.168.1.20")));
    }

    // Where the Core listens: the station network plus this computer; the
    // nereusd.conf override replaces the station choice; every address
    // covers this computer on its own.
    void listensOnTheStationNetworkAndThisComputer()
    {
        RadioModel model;
        StationTciModel state;
        StationTciController controller(&model, &state);
        const QHostAddress loopback(QHostAddress::LocalHost);
        controller.setInterfaceEntriesForTest({entry("192.168.1.20", 24)});
        QCOMPARE(controller.wantedAddresses(), QList<QHostAddress>{loopback});   // no radio yet
        controller.setRadioAddress(QHostAddress(QStringLiteral("192.168.1.50")));
        QCOMPARE(controller.wantedAddresses(),
                 (QList<QHostAddress>{QHostAddress(QStringLiteral("192.168.1.20")), loopback}));
        controller.setRadioAddress(QHostAddress(QStringLiteral("172.16.0.9")));
        QCOMPARE(controller.wantedAddresses(), QList<QHostAddress>{loopback});
        controller.setBindOverride(QStringLiteral("10.0.0.7"));
        QCOMPARE(controller.wantedAddresses(),
                 (QList<QHostAddress>{QHostAddress(QStringLiteral("10.0.0.7")), loopback}));
        controller.setBindOverride(QStringLiteral("0.0.0.0"));
        QCOMPARE(controller.wantedAddresses(),
                 QList<QHostAddress>{QHostAddress(QHostAddress::AnyIPv4)});
        controller.setBindOverride(QStringLiteral("127.0.0.1"));
        QCOMPARE(controller.wantedAddresses(), QList<QHostAddress>{loopback});
    }

    // The station's switch and port are saved on the Core and applied; a
    // restarted Core comes back as it was; off stops the server.
    void switchIsKeptOnTheCore()
    {
        const quint16 port = freePort();
        QVERIFY(port >= 1024);
        RadioModel model;
        StationTciModel state;
        {
            StationTciController controller(&model, &state);
            controller.setBindOverride(QStringLiteral("127.0.0.1"));
            controller.applySaved();
            QVERIFY(!state.enabled());
            QVERIFY(!state.listening());

            QString reason;
            QVERIFY(!controller.setEnabled(true, 80, &reason));
            QCOMPARE(reason, QStringLiteral("Choose a TCI port from 1024 to 65535."));
            QVERIFY(OperatorWording::isPlain(reason));
            QVERIFY(!state.enabled());

            QVERIFY(controller.setEnabled(true, port, &reason));
            QVERIFY(reason.isEmpty());
            QVERIFY(state.enabled());
            QVERIFY(state.listening());
            QCOMPARE(state.port(), int(port));
            QVERIFY(state.stationAddress().isEmpty());   // this computer only
            QCOMPARE(AppSettings::instance().value(StationTciController::enabledKey()).toString(),
                     QStringLiteral("True"));
            QCOMPARE(AppSettings::instance().value(StationTciController::portKey()).toString(),
                     QString::number(port));
            QVERIFY(controller.server()->stationReceiveOnly());
        }
        {
            StationTciController restarted(&model, &state);
            restarted.setBindOverride(QStringLiteral("127.0.0.1"));
            restarted.applySaved();
            QVERIFY(state.enabled());
            QVERIFY(state.listening());
            QCOMPARE(restarted.server()->port(), port);

            QString reason;
            QVERIFY(restarted.setEnabled(false, port, &reason));
            QVERIFY(!state.enabled());
            QVERIFY(!state.listening());
            QVERIFY(!restarted.server()->isRunning());
        }
    }

    // Parity Task 23: the four station options are kept by the Core;
    // each connected app has a stable record id and can be disconnected.
    void stationOptionsAndClientDisconnect()
    {
        const quint16 port = freePort();
        QVERIFY(port >= 1024);
        RadioModel model;
        StationTciModel state;
        StationTciController controller(&model, &state);
        controller.setBindOverride(QStringLiteral("127.0.0.1"));
        controller.setOptions(false, true, true, false);
        QVERIFY(!state.emulateExpertSdr3());
        QVERIFY(state.emulateSunSdr2Pro());
        QVERIFY(state.cwluBecomesCw());
        QVERIFY(!state.sendInitialState());
        QCOMPARE(AppSettings::instance().value(QStringLiteral("TciEmulateExpertSDR3Protocol"))
                     .toString(), QStringLiteral("False"));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("TciCwluBecomesCw")).toString(),
                 QStringLiteral("True"));
        QString reason;
        QVERIFY(controller.setEnabled(true, port, &reason));
        TciApp app(port);
        QTRY_COMPARE(state.clients().size(), 1);
        const StationTciClient client = state.clients().first();
        QVERIFY(!client.id.isEmpty());
        QVERIFY(!client.address.isEmpty());
        QCOMPARE(StationTciClient::fromFields(client.id, client.toFields()).value(), client);
        QVERIFY(!controller.disconnectClient(QStringLiteral("missing"), &reason));
        QCOMPARE(reason, StationTciController::unknownClientReason());
        QVERIFY(OperatorWording::isPlain(reason));
        QVERIFY(controller.disconnectClient(client.id, &reason));
        QVERIFY(reason.isEmpty());
        QTRY_COMPARE(state.clients().size(), 0);
        QVERIFY(controller.setEnabled(false, port, &reason));
    }

    // JJ's ruling of 2026-09-28 (stationTciSettingsVersion 1): the rest of
    // the TCI Server page's settings for the Core's server are the Core's,
    // at the page's defaults until changed, saved under the page's keys,
    // held to the page's ranges, and taken whole or not at all.
    void stationSettingsAreKeptHeldToTheirRangesAndPublished()
    {
        RadioModel model;
        StationTciModel state;
        StationTciController controller(&model, &state);
        controller.setBindOverride(QStringLiteral("127.0.0.1"));
        QString reason;
        QVERIFY(controller.setEnabled(false, freePort(), &reason));
        // The defaults the page and the server read.
        QCOMPARE(state.rateLimitMs(), 100);
        QVERIFY(!state.cwBecomesCwuAbove10mhz());
        QVERIFY(state.iqSwap());
        QVERIFY(!state.alwaysStreamIq());
        QCOMPARE(state.audioBlockSamples(), 2048);
        QCOMPARE(state.txChannel(), 2);
        QCOMPARE(state.rxSensorIntervalMs(), 200);
        QCOMPARE(state.txSensorIntervalMs(), 200);
        // The RX2 VFO options' defaults: TciProtocol.h's (Duplicate on).
        QCOMPARE(state.forgetRx2VfoBOnDisconnect(), kTciForgetRx2VfobDefault);
        QCOMPARE(state.useRx1VfoaForRx2Vfoa(), kTciUseRx1VfoaForRx2VfoaDefault);
        QCOMPARE(state.copyRx2VfobToVfoa(), kTciCopyRx2VfobToVfoaDefault);
        QVERIFY(state.copyRx2VfobToVfoa());

        QVERIFY(controller.setSettings({{QStringLiteral("iqSwap"), false},
                                        {QStringLiteral("audioBlockSamples"), 512},
                                        {QStringLiteral("txChannel"), 0},
                                        {QStringLiteral("rateLimitMs"), 0},
                                        {QStringLiteral("copyRx2VfobToVfoa"), false}},
                                       &reason));
        QVERIFY(reason.isEmpty());
        auto& settings = AppSettings::instance();
        QCOMPARE(settings.value(QStringLiteral("TciIqSwap")).toString(), QStringLiteral("False"));
        QCOMPARE(settings.value(QStringLiteral("TciAudioStreamSamples")).toString(),
                 QStringLiteral("512"));
        QCOMPARE(settings.value(QStringLiteral("TciTxChannel")).toString(), QStringLiteral("Left"));
        QCOMPARE(settings.value(QStringLiteral("TciRateLimitMs")).toString(), QStringLiteral("0"));
        QCOMPARE(settings.value(QStringLiteral("TciCopyRx2VfobToVfoa")).toString(),
                 QStringLiteral("False"));
        QVERIFY(!state.iqSwap());
        QCOMPARE(state.audioBlockSamples(), 512);
        QCOMPARE(state.txChannel(), 0);
        QCOMPARE(state.rateLimitMs(), 0);
        QVERIFY(!state.copyRx2VfobToVfoa());

        // Out of range, the wrong kind, or a name it does not have: refused
        // in plain words, and nothing of the request is kept.
        const QList<QVariantMap> refused{
            {{QStringLiteral("iqSwap"), true}, {QStringLiteral("audioBlockSamples"), 99}},
            {{QStringLiteral("rxSensorIntervalMs"), 1001}},
            {{QStringLiteral("txChannel"), 3}},
            {{QStringLiteral("iqSwap"), 1}},
            {{QStringLiteral("port"), 50001}}};
        for (const QVariantMap& changes : refused) {
            QVERIFY(!controller.setSettings(changes, &reason));
            QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        }
        QVERIFY(!state.iqSwap());
        QCOMPARE(state.audioBlockSamples(), 512);
        QCOMPARE(state.rxSensorIntervalMs(), 200);
        QCOMPARE(state.txChannel(), 0);

        // A value saved out of range reads back held to the range.
        settings.setValue(QStringLiteral("TciTxSensorIntervalMs"), QStringLiteral("5"));
        QVERIFY(controller.setSettings({{QStringLiteral("alwaysStreamIq"), true}}, &reason));
        QCOMPARE(state.txSensorIntervalMs(), 30);
        QVERIFY(state.alwaysStreamIq());
    }

    // ...and the Core's dispatcher takes them from a window or an app, one or
    // more at once, but not while the radio is on the air.
    void stationSettingsAreRefusedOnAir()
    {
        RadioModel model;
        model.enableStationTci(QStringLiteral("127.0.0.1"));
        auto* state = model.stationTciModel();
        SessionCommandDispatcher dispatcher(&model);
        QSignalSpy results(&dispatcher, &SessionCommandDispatcher::commandResultReady);
        model.transmitModel().setMox(true);
        dispatcher.dispatch(SessionMessages::commandInvoke("setStationTciSettings", 1,
            {{0, "iqSwap", MirrorWireKind::Bool, false}}));
        QCOMPARE(results.size(), 1);
        SessionMessage result = qvariant_cast<SessionMessage>(results.last().first());
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, RadioModel::onAirReason());
        QVERIFY(state->iqSwap());
        model.transmitModel().setMox(false);
        dispatcher.dispatch(SessionMessages::commandInvoke("setStationTciSettings", 2,
            {{0, "iqSwap", MirrorWireKind::Bool, false},
             {0, "txSensorIntervalMs", MirrorWireKind::Int64, qint64(500)}}));
        result = qvariant_cast<SessionMessage>(results.last().first());
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QVERIFY(!state->iqSwap());
        QCOMPARE(state->txSensorIntervalMs(), 500);
        // A bool where a number goes: not understood.
        dispatcher.dispatch(SessionMessages::commandInvoke("setStationTciSettings", 3,
            {{0, "txSensorIntervalMs", MirrorWireKind::Bool, true}}));
        result = qvariant_cast<SessionMessage>(results.last().first());
        QVERIFY(!result.accepted);
        QVERIFY(OperatorWording::isPlain(result.reason));
        QCOMPARE(state->txSensorIntervalMs(), 500);
    }

    // Exercise the actual Core dispatcher with simulated transmit state;
    // no radio or RF. A rejected disconnect must leave the client connected.
    void stationOptionsAndDisconnectAreRefusedOnAir()
    {
        RadioModel model;
        model.enableStationTci(QStringLiteral("127.0.0.1"));
        QString reason;
        const quint16 port = freePort();
        QVERIFY(model.setStationTciForStation(true, port, &reason));
        TciApp app(port);
        auto* state = model.stationTciModel();
        QTRY_COMPARE(state->clients().size(), 1);
        const QString id = state->clients().first().id;
        const bool original = state->emulateExpertSdr3();
        SessionCommandDispatcher dispatcher(&model);
        QSignalSpy results(&dispatcher, &SessionCommandDispatcher::commandResultReady);
        model.transmitModel().setMox(true);
        dispatcher.dispatch(SessionMessages::commandInvoke("setStationTciOptions", 1,
            {{0, "emulateExpertSdr3", MirrorWireKind::Bool, !original},
             {0, "emulateSunSdr2Pro", MirrorWireKind::Bool, false},
             {0, "cwluBecomesCw", MirrorWireKind::Bool, true},
             {0, "sendInitialState", MirrorWireKind::Bool, false}}));
        dispatcher.dispatch(SessionMessages::commandInvoke("disconnectStationTciClient", 2,
            {utf8("id", id)}));
        QCOMPARE(results.size(), 2);
        for (const auto& arguments : results) {
            const auto result = qvariant_cast<SessionMessage>(arguments.first());
            QVERIFY(!result.accepted);
            QCOMPARE(result.reason, RadioModel::onAirReason());
        }
        QCOMPARE(state->emulateExpertSdr3(), original);
        QCOMPARE(state->clients().size(), 1);
        QCOMPARE(app.socket.state(), QAbstractSocket::ConnectedState);
        model.transmitModel().setMox(false);
        dispatcher.dispatch(SessionMessages::commandInvoke("disconnectStationTciClient", 3,
            {utf8("id", id)}));
        QCOMPARE(results.size(), 3);
        QVERIFY(qvariant_cast<SessionMessage>(results.last().first()).accepted);
        QTRY_VERIFY(state->clients().isEmpty());
    }

    void remoteAppletAndSetupUseTheCoreControlPath()
    {
        const quint16 port = freePort();
        QVERIFY(port >= 1024);
        AppSettings::instance().setValue(QStringLiteral("TciServerPort"), QString::number(port));
        RadioModel window(RadioModel::Role::Remote);
        FakeStationLink link;
        window.attachStation(&link);
        TciServer local(&window);
        TciSwitch control(&local, &window);
        TciApplet applet(&local);
        applet.setStationContext(&control, &window);
        auto* enable = [&]() -> QPushButton* {
            for (QPushButton* button : applet.findChildren<QPushButton*>()) {
                if (button->text() == QStringLiteral("Enable Server")) { return button; }
            }
            return nullptr;
        }();
        QVERIFY(enable);
        enable->click();
        QCOMPARE(link.requests, 1);
        QVERIFY(link.requestedOn);
        QCOMPARE(link.requestedPort, port);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("TciServerEnabled")).toString(),
                 QStringLiteral("True"));
        QVERIFY(local.isRunning());
        local.stop();

        CatTciServerPage page;
        page.setRadioModel(&window);
        auto* coreOption = [&]() -> QCheckBox* {
            for (QCheckBox* box : page.findChildren<QCheckBox*>()) {
                if (box->text() == QStringLiteral("Emulate ExpertSDR3 protocol")
                    && box->parent() != nullptr
                    && box->parent()->objectName() == QStringLiteral("coreTciOptions")) {
                    return box;
                }
            }
            return nullptr;
        }();
        QVERIFY(coreOption);
        QVERIFY(coreOption->isEnabled());
        const QVariant localOption =
            AppSettings::instance().value(QStringLiteral("TciEmulateExpertSDR3Protocol"));
        coreOption->click();
        QCOMPARE(link.optionRequests, 1);
        QVERIFY(!link.requestedExpert);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("TciEmulateExpertSDR3Protocol")),
                 localOption);
        // JJ's ruling of 2026-09-28: the rest of the page's settings for
        // the Core's server go to the Core, never to this window's own keys.
        const auto coreSetting = [&page](const char* name) -> QWidget* {
            for (QWidget* widget : page.findChildren<QWidget*>()) {
                if (widget->property("nereusSetupId").toString()
                    == QStringLiteral("catNetwork.tciServer.core.%1").arg(QLatin1String(name))) {
                    return widget;
                }
            }
            return nullptr;
        };
        auto* iqSwap = qobject_cast<QCheckBox*>(coreSetting("iqSwap"));
        auto* block = qobject_cast<QSpinBox*>(coreSetting("audioBlockSamples"));
        auto* channel = qobject_cast<QComboBox*>(coreSetting("txChannel"));
        QVERIFY(iqSwap && block && channel);
        QVERIFY(iqSwap->isEnabled());
        const QVariant localSwap = AppSettings::instance().value(QStringLiteral("TciIqSwap"));
        iqSwap->click();
        block->setValue(1024);
        channel->setCurrentIndex(1);
        QCOMPARE(link.settingRequests.size(), 3);
        QCOMPARE(link.settingRequests.at(0).first, QByteArrayLiteral("iqSwap"));
        QCOMPARE(link.settingRequests.at(1),
                 std::make_pair(QByteArrayLiteral("audioBlockSamples"), QVariant(1024)));
        QCOMPARE(link.settingRequests.at(2),
                 std::make_pair(QByteArrayLiteral("txChannel"), QVariant(1)));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("TciIqSwap")), localSwap);
        // A Core that does not share them: shown disabled with the reason.
        link.settingsAvailable = false;
        window.reportStationLinkStateChanged();
        QVERIFY(!iqSwap->isEnabled());
        QCOMPARE(iqSwap->toolTip(), IStationLink::stationTciServerUnavailableReason());
        QVERIFY(coreOption->isEnabled());

        link.serverVersion2 = false;
        window.reportStationLinkStateChanged();
        QVERIFY(!coreOption->isEnabled());
        QVERIFY(OperatorWording::isPlain(IStationLink::stationTciServerUnavailableReason()));
    }

    // A TCI app at the station (the RF-Kit amplifier's stand-in) hears the
    // Core's radio: receive-only, split_enable and vfo as the slice moves.
    // Transmit over the Core's TCI is refused with a plain reason.
    void ampStandInFollowsTheCoresSlice()
    {
        const quint16 port = freePort();
        RadioModel station;
        const int sliceId = station.addSlice(QStringLiteral("pan-0"));
        SliceModel* slice = station.sliceById(sliceId);
        QVERIFY(slice);
        slice->setFrequency(7074000.0);
        station.enableStationTci(QStringLiteral("127.0.0.1"));
        QVERIFY(station.stationTciController());
        QString reason;
        QVERIFY(station.setStationTciForStation(true, port, &reason));
        QVERIFY(station.stationTciModel()->listening());

        TciApp amp(port);
        QTRY_VERIFY_WITH_TIMEOUT(amp.has(QStringLiteral("ready;")), 3000);
        QVERIFY(amp.has(QStringLiteral("receive_only:true;")));
        QVERIFY(amp.has(QStringLiteral("split_enable:0,false;")));
        QVERIFY(amp.has(QStringLiteral("tx_enable:0,false;")));
        QVERIFY(amp.has(QStringLiteral("vfo:0,0,7074000;")));

        slice->setFrequency(14074000.0);
        QTRY_VERIFY_WITH_TIMEOUT(amp.has(QStringLiteral("vfo:0,0,14074000;")), 3000);
        slice->setFrequency(21074000.0);
        QTRY_VERIFY_WITH_TIMEOUT(amp.has(QStringLiteral("vfo:0,0,21074000;")), 3000);
        // Another app's split set reaches the amp as split_enable too.
        amp.socket.sendTextMessage(QStringLiteral("split_enable:0,false;"));
        QTRY_VERIFY_WITH_TIMEOUT(amp.frames.count(QStringLiteral("split_enable:0,false;")) >= 2,
                                 3000);

        // Transmit waits for remote transmit: no MOX, the app hears
        // trx:0,false, and the reason is plain and off the wire.
        amp.frames.clear();
        amp.socket.sendTextMessage(QStringLiteral("trx:0,true;"));
        QTRY_VERIFY_WITH_TIMEOUT(amp.has(QStringLiteral("trx:0,false;")), 3000);
        QVERIFY(!station.mox());
        TciServer* server = station.stationTciController()->server();
        QCOMPARE(server->operatorNoticeReason(),
                 QString::fromLatin1(TciServer::kStationTransmitRefusedReason));
        QVERIFY(OperatorWording::isPlain(server->operatorNoticeReason()));
        for (const QString& frame : amp.frames) {
            QVERIFY2(!frame.contains(QStringLiteral("remote transmit")), qPrintable(frame));
        }
        QCOMPARE(server->activeTxClientCount(), 0);
        amp.socket.close();
    }

    // iPhone app plan Task 35 (the several-devices design, ruling 8.14; the
    // design's bench row 16): the Core's own TCI server stays receive-only
    // on a Core that allows remote transmit. A program through it never
    // keys, with transmit unheld or held by a device (keyed or not), makes
    // nobody the holder, and its trx:N,false never releases a device's key.
    void coresOwnServerNeverKeysHeldOrUnheld()
    {
        Core core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        QVERIFY(admitted(appA));
        const quint16 port = freePort();
        core.model->enableStationTci(QStringLiteral("127.0.0.1"));
        QString reason;
        QVERIFY(core.model->setStationTciForStation(true, port, &reason));
        TciApp app(port);
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("ready;")), 3000);
        QVERIFY(app.has(QStringLiteral("receive_only:true;")));
        MoxController* mox = core.model->moxController();
        TransmitHolder* holder = core.server->transmitHolder();

        // Unheld: no key, and nobody becomes the holder.
        app.frames.clear();
        app.socket.sendTextMessage(QStringLiteral("trx:0,true;"));
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("trx:0,false;")), 3000);
        QVERIFY(!mox->isMox());
        QCOMPARE(holder->state(), TransmitHolder::State::Unheld);

        // Held by a device, unkeyed: still no key.
        mox->setMox(true, keyerFor(a));
        mox->setMox(false, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QVERIFY(holder->isHeldBy(a.key.fingerprint()));
        app.frames.clear();
        app.socket.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("trx:0,false;")), 3000);
        QVERIFY(!mox->isMox());

        // Held and keyed by the device: the app's release ends nothing.
        mox->setMox(true, keyerFor(a));
        QVERIFY(mox->isMox());
        app.socket.sendTextMessage(QStringLiteral("trx:0,false;"));
        QTest::qWait(150);
        QVERIFY(mox->isMox());
        QCOMPARE(mox->currentKeyer().deviceId, a.key.fingerprint());
        QCOMPARE(core.model->stationTciController()->server()->activeTxClientCount(), 0);
        mox->setMox(false, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        app.socket.close();
    }

    // iPhone app Task 73 (the several-devices design, ruling 5.13): the
    // Core's own server reads every slice (trx:N is slice N), changes only
    // the station device's own (here, a slice it holds for an absent
    // device) and never keys.
    void coresServerChangesOnlyTheStationDevicesOwnSlices()
    {
        const quint16 port = freePort();
        RadioModel station;
        const int devicesSlice = station.addSlice(QStringLiteral("pan-0"));
        const int heldSlice = station.addSlice(QStringLiteral("pan-0"));
        QCOMPARE(devicesSlice, 0);
        QCOMPARE(heldSlice, 1);
        station.sliceById(0)->setFrequency(7074000.0);
        station.sliceById(1)->setFrequency(14074000.0);
        SliceOwnership* ownership = station.sliceOwnership();
        ownership->setOwner(0, QByteArray(32, '\x41'));
        ownership->hold(1, QByteArray(32, '\x42'));
        station.enableStationTci(QStringLiteral("127.0.0.1"));
        QString reason;
        QVERIFY(station.setStationTciForStation(true, port, &reason));

        TciApp app(port);
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("ready;")), 3000);
        // Reads every slice, as trx:N.
        QVERIFY(app.has(QStringLiteral("vfo:0,0,7074000;")));
        QVERIFY(app.has(QStringLiteral("vfo:1,0,14074000;")));
        QVERIFY(app.has(QStringLiteral("receive_only:true;")));

        // A device's slice: nothing changes, and the app hears the value
        // the slice holds.
        app.frames.clear();
        app.socket.sendTextMessage(QStringLiteral("vfo:0,0,7100000;"));
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("vfo:0,0,7074000;")), 3000);
        const DSPMode mode = station.sliceById(0)->dspMode();
        app.socket.sendTextMessage(QStringLiteral("modulation:0,am;"));
        app.socket.sendTextMessage(QStringLiteral("rx_mute:0,true;"));
        QTest::qWait(200);
        QCOMPARE(station.sliceById(0)->frequency(), 7074000.0);
        QCOMPARE(station.sliceById(0)->dspMode(), mode);
        QVERIFY(!station.sliceById(0)->muted());

        // The station device's own slice changes.
        app.socket.sendTextMessage(QStringLiteral("vfo:1,0,14100000;"));
        QTRY_COMPARE_WITH_TIMEOUT(station.sliceById(1)->frequency(), 14100000.0, 3000);

        // And it never keys.
        app.frames.clear();
        app.socket.sendTextMessage(QStringLiteral("trx:1,true;"));
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("trx:1,false;")), 3000);
        QVERIFY(!station.mox());
        app.socket.close();
    }

    // The three RX2 VFO options on the Core's own server. The Core has no
    // receiver map, so RX2 is on when slice 1 exists (trx:N is slice N),
    // not when the connection's active RX count says so. Thetis
    // TCIServer.cs:7256-7269 and 7295-7296 [v2.10.3.15]: slice 1 (RX2 VFO
    // B) goes out on channel 1, then copied to channel 0 unless replaced;
    // with Use RX1 VFO A, slice 0 goes out as receiver 1 channel 0 only.
    void stationRx2VfoOptions_data()
    {
        QTest::addColumn<bool>("copy");
        QTest::addColumn<bool>("forget");
        QTest::addColumn<bool>("useRx1");
        QTest::addColumn<QStringList>("slice1Lines");
        const QString b1 = QStringLiteral("vfo:1,1,14100000;");
        const QString b0 = QStringLiteral("vfo:1,0,14100000;");
        QTest::newRow("copy, keep channel 1") << true << false << false << QStringList{b1, b0};
        QTest::newRow("copy, forget channel 1") << true << true << false << QStringList{b0};
        QTest::newRow("no copy") << false << false << false << QStringList{b1};
        QTest::newRow("no copy, forget has no effect") << false << true << false << QStringList{b1};
        QTest::newRow("copy, use RX1 VFO A") << true << false << true << QStringList{b1, b0};
    }
    void stationRx2VfoOptions()
    {
        QFETCH(bool, copy);
        QFETCH(bool, forget);
        QFETCH(bool, useRx1);
        QFETCH(QStringList, slice1Lines);
        auto& s = AppSettings::instance();
        const auto flag = [](bool on) { return on ? QStringLiteral("True") : QStringLiteral("False"); };
        s.setValue(QStringLiteral("TciCopyRx2VfobToVfoa"), flag(copy));
        s.setValue(QStringLiteral("TciForgetRx2VfoBOnDisconnect"), flag(forget));
        s.setValue(QStringLiteral("TciUseRx1VfoaForRx2Vfoa"), flag(useRx1));

        const quint16 port = freePort();
        RadioModel station;
        QCOMPARE(station.addSlice(QStringLiteral("pan-0")), 0);
        QCOMPARE(station.addSlice(QStringLiteral("pan-0")), 1);
        station.sliceById(0)->setFrequency(7074000.0);
        station.sliceById(1)->setFrequency(14074000.0);
        SliceOwnership* ownership = station.sliceOwnership();
        ownership->hold(0, QByteArray(32, '\x42'));
        ownership->hold(1, QByteArray(32, '\x42'));
        station.enableStationTci(QStringLiteral("127.0.0.1"));
        QString reason;
        QVERIFY(station.setStationTciForStation(true, port, &reason));
        TciApp app(port);
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("ready;")), 3000);
        QTest::qWait(150);
        const auto vfoLines = [&app]() {
            QStringList lines;
            for (const QString& f : app.frames) {
                if (f.startsWith(QStringLiteral("vfo:"))) { lines.append(f); }
            }
            return lines;
        };

        // Slice 1 moves: RX2 VFO B's lines.
        app.frames.clear();
        station.sliceById(1)->setFrequency(14100000.0);
        QTRY_VERIFY_WITH_TIMEOUT(app.has(slice1Lines.last()), 3000);
        QTest::qWait(150);
        QCOMPARE(vfoLines(), slice1Lines);

        // Slice 0 moves: with Use RX1 VFO A it is receiver 1 channel 0 only.
        app.frames.clear();
        station.sliceById(0)->setFrequency(7100000.0);
        if (useRx1) {
            QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("vfo:1,0,7100000;")), 3000);
            QTest::qWait(150);
            QCOMPARE(vfoLines(), QStringList{QStringLiteral("vfo:1,0,7100000;")});
            // And a set of receiver 1 channel 0 tunes slice 0.
            app.socket.sendTextMessage(QStringLiteral("vfo:1,0,7110000;"));
            QTRY_COMPARE_WITH_TIMEOUT(station.sliceById(0)->frequency(), 7110000.0, 3000);
            QCOMPARE(station.sliceById(1)->frequency(), 14100000.0);
        } else {
            // With RX2 on, Thetis's VFO A handler sends channel 0 alone
            // (TCIServer.cs:7266-7269 [v2.10.3.15]).
            QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("vfo:0,0,7100000;")), 3000);
            QTest::qWait(150);
            QCOMPARE(vfoLines(), QStringList{QStringLiteral("vfo:0,0,7100000;")});
            // And vfo:0,1 is VFO B, RX2's: it tunes slice 1.
            app.socket.sendTextMessage(QStringLiteral("vfo:0,1,14120000;"));
            QTRY_COMPARE_WITH_TIMEOUT(station.sliceById(1)->frequency(), 14120000.0, 3000);
            QCOMPARE(station.sliceById(0)->frequency(), 7100000.0);
        }
        app.socket.close();
    }

    // A vfo set is gated on the slice it writes, not the receiver it names:
    // with Use RX1 VFO A on, vfo:1,0 writes slice 0, and a slice another
    // device owns (the phone's) is not retuned; the app hears its value.
    void stationRx1VfoaSetRespectsTheOwnerOfSlice0()
    {
        AppSettings::instance().setValue(QStringLiteral("TciUseRx1VfoaForRx2Vfoa"),
                                         QStringLiteral("True"));
        const quint16 port = freePort();
        RadioModel station;
        QCOMPARE(station.addSlice(QStringLiteral("pan-0")), 0);
        QCOMPARE(station.addSlice(QStringLiteral("pan-0")), 1);
        station.sliceById(0)->setFrequency(7074000.0);
        station.sliceById(1)->setFrequency(14074000.0);
        SliceOwnership* ownership = station.sliceOwnership();
        ownership->setOwner(0, QByteArray(32, '\x41'));
        ownership->hold(1, QByteArray(32, '\x42'));
        station.enableStationTci(QStringLiteral("127.0.0.1"));
        QString reason;
        QVERIFY(station.setStationTciForStation(true, port, &reason));
        TciApp app(port);
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("ready;")), 3000);
        QTest::qWait(150);

        app.frames.clear();
        app.socket.sendTextMessage(QStringLiteral("vfo:1,0,7100000;"));
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("vfo:1,0,7074000;")), 3000);
        QTest::qWait(150);
        QCOMPARE(station.sliceById(0)->frequency(), 7074000.0);
        QCOMPARE(station.sliceById(1)->frequency(), 14074000.0);

        // Slice 1 is the station device's: vfo:1,1 still tunes it.
        app.socket.sendTextMessage(QStringLiteral("vfo:1,1,14100000;"));
        QTRY_COMPARE_WITH_TIMEOUT(station.sliceById(1)->frequency(), 14100000.0, 3000);
        app.socket.close();
    }

    // rx_channel_enable on the Core's server (Thetis handleRxChannelEnable,
    // TCIServer.cs:6252-6291 [v2.10.3.15]): receiver 1 is slice 1, owned
    // by another device here, so a set changes nothing and answers what
    // slice 1 holds. Slice 0 is the station device's own.
    void stationRxChannelEnableRespectsTheOwnerOfSlice1()
    {
        const quint16 port = freePort();
        RadioModel station;
        QCOMPARE(station.addSlice(QStringLiteral("pan-0")), 0);
        QCOMPARE(station.addSlice(QStringLiteral("pan-0")), 1);
        SliceOwnership* ownership = station.sliceOwnership();
        ownership->hold(0, QByteArray(32, '\x42'));
        ownership->setOwner(1, QByteArray(32, '\x41'));
        station.enableStationTci(QStringLiteral("127.0.0.1"));
        QString reason;
        QVERIFY(station.setStationTciForStation(true, port, &reason));
        TciApp app(port);
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("ready;")), 3000);
        QTest::qWait(150);

        app.frames.clear();
        app.socket.sendTextMessage(QStringLiteral("rx_channel_enable:1,0,false;"));
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("rx_channel_enable:1,0,true;")), 3000);
        app.socket.sendTextMessage(QStringLiteral("rx_channel_enable:1,1,true;"));
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("rx_channel_enable:1,1,false;")), 3000);
        QTest::qWait(150);
        QVERIFY(!app.has(QStringLiteral("rx_channel_enable:1,0,false;")));
        QVERIFY(!app.has(QStringLiteral("rx_channel_enable:1,1,true;")));
        QVERIFY(station.sliceById(1) != nullptr);

        // Slice 0 is writable: its set is echoed.
        app.socket.sendTextMessage(QStringLiteral("rx_channel_enable:0,1,true;"));
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("rx_channel_enable:0,1,true;")), 3000);
        app.socket.close();
    }

    // rx_enable on the Core's server (Thetis handleRXEnable,
    // TCIServer.cs:4595-4629 [v2.10.3.15]): receiver 1 answers true while
    // slice 1 is there and false without it. A set sends nothing to any app
    // and opens or closes no slice.
    void stationRxEnableFollowsSlice1()
    {
        const quint16 port = freePort();
        RadioModel station;
        QCOMPARE(station.addSlice(QStringLiteral("pan-0")), 0);
        QCOMPARE(station.addSlice(QStringLiteral("pan-0")), 1);
        station.enableStationTci(QStringLiteral("127.0.0.1"));
        QString reason;
        QVERIFY(station.setStationTciForStation(true, port, &reason));
        TciApp app(port);
        TciApp other(port);
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("ready;")), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(other.has(QStringLiteral("ready;")), 3000);
        QTest::qWait(150);

        app.frames.clear();
        app.socket.sendTextMessage(QStringLiteral("rx_enable:1;"));
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("rx_enable:1,true;")), 3000);

        app.frames.clear();
        other.frames.clear();
        app.socket.sendTextMessage(QStringLiteral("rx_enable:1,false;"));
        app.socket.sendTextMessage(QStringLiteral("rx_enable:0,false;"));
        QTest::qWait(300);
        const auto rxEnableLines = [](const TciApp& a) {
            QStringList out;
            for (const QString& f : a.frames) {
                if (f.startsWith(QStringLiteral("rx_enable:"))) { out << f; }
            }
            return out;
        };
        QCOMPARE(rxEnableLines(app), QStringList{});
        QCOMPARE(rxEnableLines(other), QStringList{});
        QVERIFY(station.sliceById(1) != nullptr);

        station.removeSlice(1);
        QTRY_VERIFY_WITH_TIMEOUT(other.has(QStringLiteral("rx_enable:1,false;")), 3000);
        app.frames.clear();
        app.socket.sendTextMessage(QStringLiteral("rx_enable:1;"));
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("rx_enable:1,false;")), 3000);
        app.socket.close();
        other.socket.close();
    }

    // Thetis re-sends the RX2 lines when RX2 is turned on or off
    // (RX2EnabledChangedHandlers, TCIServer.cs:6741 and 842-847
    // [v2.10.3.15]): rx_enable:1 and tx_enable:1 only. On the Core RX2 is
    // slice 1, so adding and removing it sends them, once per change; the
    // Core's server refuses transmit, so tx_enable:1 stays false.
    void stationRx2LinesFollowSlice1()
    {
        const quint16 port = freePort();
        RadioModel station;
        QCOMPARE(station.addSlice(QStringLiteral("pan-0")), 0);
        station.enableStationTci(QStringLiteral("127.0.0.1"));
        QString reason;
        QVERIFY(station.setStationTciForStation(true, port, &reason));
        TciApp app(port);
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("ready;")), 3000);
        QTest::qWait(150);
        const auto rx2Lines = [&app]() {
            QStringList lines;
            for (const QString& f : app.frames) {
                if (f.startsWith(QStringLiteral("rx_enable:1,"))
                    || f.startsWith(QStringLiteral("tx_enable:1,"))
                    || f.startsWith(QStringLiteral("rx_channel_enable:1,"))
                    || f.startsWith(QStringLiteral("lock:1,"))) {
                    lines.append(f);
                }
            }
            return lines;
        };

        app.frames.clear();
        QCOMPARE(station.addSlice(QStringLiteral("pan-0")), 1);
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("tx_enable:1,false;")), 3000);
        QTest::qWait(150);
        QCOMPARE(rx2Lines(), (QStringList{QStringLiteral("rx_enable:1,true;"),
                                          QStringLiteral("tx_enable:1,false;")}));

        app.frames.clear();
        station.removeSlice(1);
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("rx_enable:1,false;")), 3000);
        QTest::qWait(150);
        QCOMPARE(rx2Lines(), (QStringList{QStringLiteral("rx_enable:1,false;"),
                                          QStringLiteral("tx_enable:1,false;")}));
        app.socket.close();
    }

    // A Core with one slice has RX2 off: a set of receiver 1 is ignored
    // (Thetis TCIServer.cs:3897-3899 [v2.10.3.15]), and slice 0 goes out on
    // both of its own channels even with every option on.
    void stationWithOneSliceHasRx2Off()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("TciCopyRx2VfobToVfoa"), QStringLiteral("True"));
        s.setValue(QStringLiteral("TciUseRx1VfoaForRx2Vfoa"), QStringLiteral("True"));
        const quint16 port = freePort();
        RadioModel station;
        QCOMPARE(station.addSlice(QStringLiteral("pan-0")), 0);
        station.sliceById(0)->setFrequency(7074000.0);
        station.sliceOwnership()->hold(0, QByteArray(32, '\x42'));
        station.enableStationTci(QStringLiteral("127.0.0.1"));
        QString reason;
        QVERIFY(station.setStationTciForStation(true, port, &reason));
        TciApp app(port);
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("ready;")), 3000);
        QTest::qWait(150);

        app.frames.clear();
        app.socket.sendTextMessage(QStringLiteral("vfo:1,0,14100000;"));
        app.socket.sendTextMessage(QStringLiteral("vfo:1,1,14100000;"));
        app.socket.sendTextMessage(QStringLiteral("vfo:0,0,7100000;"));
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("vfo:0,0,7100000;")), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("vfo:0,1,7100000;")), 3000);
        QTest::qWait(150);
        QCOMPARE(station.sliceById(0)->frequency(), 7100000.0);
        for (const QString& f : app.frames) {
            QVERIFY2(!f.startsWith(QStringLiteral("vfo:1,")), qPrintable(f));
        }
        app.socket.close();
    }

    // iPhone app Task 73 (ruling 5.11): TCI's per-slice broadcasts that
    // exist once per radio (digl_offset, digu_offset) follow the
    // station-level active slice. Two devices, each with its own active
    // slice: while A holds the station's slice (A holds transmit), B's
    // choice of its own slice does not redirect them; with nobody holding
    // transmit the most recent choice does.
    void perSliceBroadcastsFollowTheStationLevelActiveSlice()
    {
        const quint16 port = freePort();
        RadioModel station;
        QCOMPARE(station.addSlice(QStringLiteral("pan-0")), 0);
        QCOMPARE(station.addSlice(QStringLiteral("pan-0")), 1);
        const QByteArray a(32, '\x41');
        const QByteArray b(32, '\x42');
        SliceOwnership* ownership = station.sliceOwnership();
        ownership->setOwner(0, a);
        ownership->setOwner(1, b);
        station.setTransmitHolder(a);
        QVERIFY(station.setActiveSliceByIdFor(a, 0));
        QVERIFY(station.setActiveSliceByIdFor(b, 1));
        // Each device's own active slice; the station's is A's.
        QVERIFY(station.sliceById(0)->isActive());
        QVERIFY(station.sliceById(1)->isActive());
        QCOMPARE(station.activeSlice(), station.sliceById(0));

        station.enableStationTci(QStringLiteral("127.0.0.1"));
        QString reason;
        QVERIFY(station.setStationTciForStation(true, port, &reason));
        TciApp app(port);
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("ready;")), 3000);

        app.frames.clear();
        station.sliceById(1)->setDiglOffsetHz(1500);
        station.sliceById(0)->setDiglOffsetHz(900);
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("digl_offset:900;")), 3000);
        QVERIFY(!app.has(QStringLiteral("digl_offset:1500;")));

        // Nobody holds transmit: B's latest choice is the station's slice.
        station.setTransmitHolder(QByteArray());
        QVERIFY(station.setActiveSliceByIdFor(b, 1));
        QCOMPARE(station.activeSlice(), station.sliceById(1));
        app.frames.clear();
        station.sliceById(0)->setDiglOffsetHz(950);
        station.sliceById(1)->setDiglOffsetHz(1600);
        QTRY_VERIFY_WITH_TIMEOUT(app.has(QStringLiteral("digl_offset:1600;")), 3000);
        QVERIFY(!app.has(QStringLiteral("digl_offset:950;")));
        app.socket.close();
    }

    // R-R3-48: band follow over the Core's server. The amp's address
    // connected as an app: following. Before that: the address to enter
    // on the amp, or "this computer only". Switched off: off.
    void rfKitBandFollowOverTheCoresServer()
    {
        const quint16 port = freePort();
        RadioModel station;
        station.enableStationTci(QStringLiteral("127.0.0.1"));
        RfKitModel* rfKit = station.rfKitModel();
        QCOMPARE(rfKit->bandFollow(), BandFollow::Off);
        QVERIFY(OperatorWording::isPlain(rfKit->bandFollowText()));

        RfKitModel::StationConnectionState amp;
        amp.configuredHost = QStringLiteral("127.0.0.1");
        amp.configuredPort = 8080;
        rfKit->setStationConnectionState(amp);
        QString reason;
        QVERIFY(station.setStationTciForStation(true, port, &reason));
        QTRY_COMPARE(rfKit->bandFollow(), BandFollow::ThisComputerOnly);
        QVERIFY(OperatorWording::isPlain(rfKit->bandFollowText()));

        {
            TciApp app(port);
            QTRY_COMPARE_WITH_TIMEOUT(rfKit->bandFollow(), BandFollow::Following, 3000);
            QCOMPARE(rfKit->bandFollowText(), QStringLiteral("Band follow: following the radio"));
            QCOMPARE(rfKit->bandFollowPort(), int(port));
            app.socket.close();
            QTRY_COMPARE_WITH_TIMEOUT(rfKit->bandFollow(), BandFollow::ThisComputerOnly, 3000);
        }

        QVERIFY(station.setStationTciForStation(false, port, &reason));
        QTRY_COMPARE(rfKit->bandFollow(), BandFollow::Off);
    }

    // The address to enter on the amp, given where a server listens.
    void addressToEnterOnTheAmp()
    {
        const QHostAddress amp(QStringLiteral("192.168.1.60"));
        const QList<QNetworkAddressEntry> entries{entry("10.8.0.4", 24), entry("192.168.1.20", 24)};
        const QHostAddress loopback(QHostAddress::LocalHost);
        QCOMPARE(RfKitBandFollow::addressForAmp({QHostAddress(QStringLiteral("192.168.1.20")),
                                                 loopback}, amp, entries),
                 QHostAddress(QStringLiteral("192.168.1.20")));
        QCOMPARE(RfKitBandFollow::addressForAmp({QHostAddress(QHostAddress::AnyIPv4)}, amp, entries),
                 QHostAddress(QStringLiteral("192.168.1.20")));
        QVERIFY(RfKitBandFollow::addressForAmp({loopback}, amp, entries).isNull());

        RfKitModel model;
        model.setBandFollow(BandFollow::Waiting, QStringLiteral("192.168.1.20"), 50001);
        QCOMPARE(model.bandFollowText(),
                 QStringLiteral("Band follow: enter 192.168.1.20, port 50001 as the TCI server on "
                                "the amplifier."));
        QVERIFY(OperatorWording::isPlain(model.bandFollowText()));
    }

    // The window's one switch: its own server and the Core's; none of its
    // own when the Core is on this computer; off stops both; a port change
    // goes to the Core; an older Core leaves this window's server only.
    void oneSwitchDrivesBothServers()
    {
        const quint16 port = freePort();
        RadioModel window(RadioModel::Role::Remote);
        FakeStationLink link;
        window.attachStation(&link);
        TciServer local(&window);
        TciSwitch tci(&local, &window);
        const QHostAddress loopback(QHostAddress::LocalHost);

        tci.setSwitch(true, port, loopback);
        QVERIFY(local.isRunning());
        QCOMPARE(local.port(), port);
        QCOMPARE(link.requests, 1);
        QVERIFY(link.requestedOn);
        QCOMPARE(link.requestedPort, port);

        const quint16 other = freePort();
        tci.setPortOrBind(other, loopback);
        QVERIFY(local.isRunning());
        QCOMPARE(local.port(), other);
        QCOMPARE(link.requests, 2);
        QCOMPARE(link.requestedPort, other);

        // The Core turns out to be on this computer and serves TCI on this
        // port: one server, the Core's.
        link.coreHere = true;
        StationTciModel::State serving;
        serving.enabled = true;
        serving.listening = true;
        serving.port = other;
        window.stationTciModel()->setState(serving);
        window.reportStationLinkStateChanged();
        QVERIFY(!local.isRunning());
        QVERIFY(tci.coreServesThisComputer());

        tci.setSwitch(false, other, loopback);
        QVERIFY(!local.isRunning());
        QCOMPARE(link.requests, 3);
        QVERIFY(!link.requestedOn);

        tci.setSwitch(true, other, loopback);
        QVERIFY(!local.isRunning());
        QCOMPARE(link.requests, 4);
        QVERIFY(link.requestedOn);

        // Startup applies the switch here without telling the Core.
        link.coreHere = false;
        link.tciAvailable = false;   // an older Core
        tci.setSwitch(true, port, loopback, /*tellCore=*/false);
        QVERIFY(local.isRunning());
        QCOMPARE(link.requests, 4);
        tci.setSwitch(false, port, loopback);
        QVERIFY(!local.isRunning());
        QCOMPARE(link.requests, 4);
    }

    // Rework part 1 (R-R3-48, operator decision 2026-09-23: one TCI switch
    // and one port). A window connected to a Core on another computer shows
    // the Core's station switch and port: another window (or the phone)
    // changing them changes this window's switch, and this window's own
    // server follows it. The whole state is taken at once (the object's
    // properties arrive one at a time), so a first property never makes the
    // window restart on a stale port.
    void windowFollowsTheCoresSwitchOnAnotherComputer()
    {
        const quint16 port = freePort();
        const quint16 other = freePort();
        RadioModel window(RadioModel::Role::Remote);
        FakeStationLink link;
        window.attachStation(&link);
        TciServer local(&window);
        TciSwitch tci(&local, &window);
        QSignalSpy starts(&local, &TciServer::serverStarted);
        tci.setSwitch(true, port, QHostAddress(QHostAddress::LocalHost));
        QVERIFY(local.isRunning());
        QCOMPARE(link.requests, 1);

        StationTciModel* station = window.stationTciModel();
        station->applyStationValue("enabled", true);
        station->applyStationValue("port", int(port));
        station->applyStationValue("listening", true);
        QCoreApplication::processEvents();
        QVERIFY(local.isRunning());
        QCOMPARE(starts.count(), 1);

        // Another window moves the Core to another port: this window's
        // switch and server follow, once.
        station->applyStationValue("port", int(other));
        QCoreApplication::processEvents();
        QTRY_COMPARE(local.port(), other);
        QCOMPARE(tci.port(), other);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("TciServerPort")).toInt(), int(other));
        QCOMPARE(link.requests, 1);   // following never asks the Core

        // And turns the Core's switch off: this window's switch goes off.
        station->applyStationValue("enabled", false);
        station->applyStationValue("listening", false);
        QTRY_VERIFY(!local.isRunning());
        QVERIFY(!tci.switchOn());
        QCOMPARE(AppSettings::instance().value(QStringLiteral("TciServerEnabled")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(link.requests, 1);
    }

    // Rework part 1: on the Core's own computer the window runs no server
    // while connected (the Core's loopback listener serves apps here); its
    // switch still sets the Core's.
    void coreHereWindowRunsNoServer()
    {
        const quint16 port = freePort();
        RadioModel window(RadioModel::Role::Remote);
        FakeStationLink link;
        link.coreHere = true;
        window.attachStation(&link);
        TciServer local(&window);
        TciSwitch tci(&local, &window);
        tci.setSwitch(true, port, QHostAddress(QHostAddress::LocalHost));
        QVERIFY(!local.isRunning());
        QCOMPARE(link.requests, 1);
        QVERIFY(link.requestedOn);
        StationTciModel* station = window.stationTciModel();
        station->applyStationValue("enabled", true);
        station->applyStationValue("port", int(port));
        QCoreApplication::processEvents();
        QVERIFY(!local.isRunning());   // not listening yet: still none here
        tci.setSwitch(false, port, QHostAddress(QHostAddress::LocalHost));
        QCOMPARE(link.requests, 2);
        QVERIFY(!link.requestedOn);
        QVERIFY(!local.isRunning());
    }

    // Rework part 2 (R-R3-48): at connect the Core's stored switch wins;
    // this window's switch follows it and asks the Core for nothing.
    void coresStoredSwitchWinsAtConnect()
    {
        const quint16 port = freePort();
        const quint16 corePort = freePort();
        RadioModel window(RadioModel::Role::Remote);
        FakeStationLink link;
        link.ready = false;
        window.attachStation(&link);
        TciServer local(&window);
        TciSwitch tci(&local, &window);
        tci.setSwitch(true, port, QHostAddress(QHostAddress::LocalHost), /*tellCore=*/false);
        QVERIFY(local.isRunning());

        link.ready = true;
        window.reportStationLinkStateChanged();
        StationTciModel* station = window.stationTciModel();
        station->applyStationValue("enabled", false);
        station->applyStationValue("port", int(corePort));
        QTRY_VERIFY(!tci.switchOn());
        QCOMPARE(tci.port(), corePort);
        QVERIFY(!local.isRunning());
        QCOMPARE(link.requests, 0);
    }

    // Rework part 2: a Core with no stored station switch yet (the upgrade:
    // nereusd and this window on one computer with TCI on before) takes
    // this window's switch and port at connect. Its first state (its
    // defaults) does not turn this window's switch off meanwhile. A Core
    // whose settings have not arrived yet is not decided until they do.
    void windowSeedsACoreWithNoStoredSwitch()
    {
        const quint16 port = freePort();
        RadioModel window(RadioModel::Role::Remote);
        FakeStationLink link;
        link.ready = false;
        link.stored = -1;
        link.coreHere = true;
        window.attachStation(&link);
        TciServer local(&window);
        TciSwitch tci(&local, &window);
        tci.setSwitch(true, port, QHostAddress(QHostAddress::LocalHost), /*tellCore=*/false);

        link.ready = true;
        window.reportStationLinkStateChanged();
        StationTciModel* station = window.stationTciModel();
        station->applyStationValue("enabled", false);
        station->applyStationValue("port", int(StationTciController::kDefaultPort));
        QCoreApplication::processEvents();
        QCOMPARE(link.requests, 0);        // not known yet: nothing decided
        QVERIFY(tci.switchOn());

        link.stored = 0;                   // the Core's settings arrive
        window.reportStationSettingChanged(QString());
        QTRY_COMPARE(link.requests, 1);
        QVERIFY(link.requestedOn);
        QCOMPARE(link.requestedPort, port);
        QCoreApplication::processEvents();
        QVERIFY(tci.switchOn());           // the Core's defaults did not win
        station->applyStationValue("enabled", true);
        station->applyStationValue("port", int(port));
        station->applyStationValue("listening", true);
        QCoreApplication::processEvents();
        QVERIFY(tci.switchOn());
        QCOMPARE(tci.port(), port);
        QCOMPARE(link.requests, 1);
    }

    // Rework follow-up 1 (R-R3-48): a window that asked the Core for a
    // switch and port follows the Core again once that request is over,
    // whatever became of it: the link dropped before the Core's echo, the
    // Core refused it, or another window's change landed in the same turn
    // so the echo never matched.
    void windowFollowsAgainAfterItsRequestEnds()
    {
        const quint16 port = freePort();
        const quint16 other = freePort();
        const auto coreSays = [](RadioModel& window, bool on, quint16 p) {
            StationTciModel::State state;
            state.enabled = on;
            state.port = p;
            state.listening = on;
            window.stationTciModel()->setState(state);
            QCoreApplication::processEvents();
        };
        // The link drops before the echo.
        {
            RadioModel window(RadioModel::Role::Remote);
            FakeStationLink link;
            window.attachStation(&link);
            TciServer local(&window);
            TciSwitch tci(&local, &window);
            coreSays(window, false, port);
            tci.setSwitch(true, port, QHostAddress(QHostAddress::LocalHost));
            link.ready = false;
            window.reportStationLinkStateChanged();
            link.ready = true;
            window.reportStationLinkStateChanged();
            coreSays(window, false, other);
            QVERIFY(!tci.switchOn());
            QCOMPARE(tci.port(), other);
        }
        // The Core refuses the request (a hand-edited port below 1024).
        {
            RadioModel window(RadioModel::Role::Remote);
            FakeStationLink link;
            window.attachStation(&link);
            TciServer local(&window);
            TciSwitch tci(&local, &window);
            coreSays(window, false, port);
            tci.setSwitch(true, 900, QHostAddress(QHostAddress::LocalHost));
            window.reportStationAccessoryRefusal(QStringLiteral("tci"),
                QStringLiteral("Choose a TCI port from 1024 to 65535."), 101);
            window.reportStationCommandFinished(101, false,
                QStringLiteral("Choose a TCI port from 1024 to 65535."));
            QCoreApplication::processEvents();
            QVERIFY(!tci.switchOn());   // the Core's switch again
            QCOMPARE(tci.port(), port);
            coreSays(window, true, other);
            QVERIFY(tci.switchOn());
            QCOMPARE(tci.port(), other);
            local.stop();
        }
        // Another window's change lands in the same turn: the echo never
        // matches; the request's acceptance ends the wait.
        {
            RadioModel window(RadioModel::Role::Remote);
            FakeStationLink link;
            window.attachStation(&link);
            TciServer local(&window);
            TciSwitch tci(&local, &window);
            coreSays(window, false, port);
            tci.setSwitch(true, port, QHostAddress(QHostAddress::LocalHost));
            coreSays(window, true, other);
            QCOMPARE(tci.port(), port);   // still waiting for its own echo
            window.reportStationCommandFinished(101, true, QString());   // the Core accepted it
            QCoreApplication::processEvents();
            QVERIFY(tci.switchOn());
            QCOMPARE(tci.port(), other);
            local.stop();
        }
    }

    // Rework part 4 (R-R3-48): with the link to the Core down the switch
    // shows the last known state. On the Core's computer the window starts
    // no server (there is no radio here to serve); on another computer its
    // server follows the switch as before. The TCI page's line says
    // nothing about a Core it cannot reach.
    void linkDownKeepsTheLastSwitch()
    {
        const quint16 port = freePort();
        for (const bool coreHere : {true, false}) {
            RadioModel window(RadioModel::Role::Remote);
            FakeStationLink link;
            link.coreHere = coreHere;
            window.attachStation(&link);
            TciServer local(&window);
            TciSwitch tci(&local, &window);
            StationTciModel::State serving;
            serving.enabled = true;
            serving.listening = true;
            serving.port = port;
            window.stationTciModel()->setState(serving);
            tci.setSwitch(true, port, QHostAddress(QHostAddress::LocalHost), /*tellCore=*/false);
            QCoreApplication::processEvents();
            QCOMPARE(local.isRunning(), !coreHere);
            QVERIFY(!TciSwitch::stationLine(&window).isEmpty());

            link.ready = false;
            window.reportStationLinkStateChanged();
            QCoreApplication::processEvents();
            QVERIFY(tci.switchOn());
            QCOMPARE(tci.port(), port);
            QCOMPARE(local.isRunning(), !coreHere);
            QVERIFY(TciSwitch::stationLine(&window).isEmpty());
            local.stop();
        }
    }

    // Follow-up 1b: the Core retries a station listener that could not
    // start (another program had its port), with a plain reason while it
    // cannot listen, and listens once the port is free.
    void coreRetriesAFailedStationListener()
    {
        QTcpServer blocker;
        QVERIFY(blocker.listen(QHostAddress::LocalHost, 0));
        const quint16 port = blocker.serverPort();
        RadioModel model;
        StationTciModel state;
        StationTciController controller(&model, &state);
        controller.setBindOverride(QStringLiteral("127.0.0.1"));
        // Rework follow-up 2: the first failure is logged once, and the
        // retries not at all (warnings counted while it fails).
        // Each try, retries included, logs one debug line in nereus.tci
        // (quiet listen attempts); counting those shows a retry ran instead
        // of sleeping for one.
        static int s_listenWarnings = 0;
        static int s_listenAttempts = 0;
        s_listenWarnings = 0;
        s_listenAttempts = 0;
        static QtMessageHandler s_previous = nullptr;
        // Through LogManager, so its filter rules stay in force and only
        // this category changes; its setting lives in the in-memory
        // AppSettings that init() and cleanup() clear.
        auto& logs = LogManager::instance();
        const QString tciCategory = QStringLiteral("nereus.tci");
        const bool tciWasEnabled = logs.isEnabled(tciCategory);
        logs.setEnabled(tciCategory, true);
        s_previous = qInstallMessageHandler(
            [](QtMsgType type, const QMessageLogContext& context, const QString& text) {
                if (type == QtWarningMsg && text.contains(QStringLiteral("listen"))) {
                    ++s_listenWarnings;
                }
                if (type == QtDebugMsg && text.contains(QStringLiteral("failed to listen"))) {
                    ++s_listenAttempts;
                    return;
                }
                if (s_previous) {
                    s_previous(type, context, text);
                }
            });
        const auto restoreLogging = qScopeGuard([&logs, tciCategory, tciWasEnabled] {
            qInstallMessageHandler(s_previous);
            logs.setEnabled(tciCategory, tciWasEnabled);
        });
        QString reason;
        QVERIFY(controller.setEnabled(true, port, &reason));
        // The first try, then at least one retry.
        QTRY_VERIFY_WITH_TIMEOUT(s_listenAttempts >= 2, 10000);
        qInstallMessageHandler(s_previous);
        QCOMPARE(s_listenWarnings, 1);
        QVERIFY(state.enabled());
        QVERIFY(!state.listening());
        QCOMPARE(state.error(), StationTciController::blockedReason(
                                    port, {QHostAddress(QHostAddress::LocalHost)}));
        QVERIFY(OperatorWording::isPlain(state.error()));
        blocker.close();
        QTRY_VERIFY_WITH_TIMEOUT(state.listening(), 5000);
        QVERIFY(state.error().isEmpty());
        QVERIFY(controller.setEnabled(false, port, &reason));
    }

    // Rework part 3 (R-R3-48): the Core binds the station address and this
    // computer separately. A third program holds the port on this computer:
    // the station network is still served (the RF-Kit's band follow keeps
    // working), the object says which address is blocked, in plain words;
    // when the program lets go, the Core takes this computer too on its
    // next retry, with no stop and start of the server.
    void stationNetworkServedWhileThisComputerIsBlocked()
    {
        QString stationAddress;
        for (const QHostAddress& address : QNetworkInterface::allAddresses()) {
            if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
                stationAddress = address.toString();
                break;
            }
        }
        if (stationAddress.isEmpty()) {
            QSKIP("No non-loopback IPv4 address on this computer to stand in for the station.");
        }
        QTcpServer blocker;
        QVERIFY(blocker.listen(QHostAddress::LocalHost, 0));
        const quint16 port = blocker.serverPort();
        RadioModel model;
        StationTciModel state;
        StationTciController controller(&model, &state);
        controller.setBindOverride(stationAddress);
        QSignalSpy starts(controller.server(), &TciServer::serverStarted);
        QSignalSpy stops(controller.server(), &TciServer::serverStopped);
        QString reason;
        QVERIFY(controller.setEnabled(true, port, &reason));
        QVERIFY(state.listening());
        QCOMPARE(state.stationAddress(), stationAddress);
        QCOMPARE(state.error(), StationTciController::blockedReason(
                                    port, {QHostAddress(QHostAddress::LocalHost)}));
        QVERIFY(OperatorWording::isPlain(state.error()));
        // The RF-Kit's band follow is up on the station address.
        RfKitModel rfKit;
        RfKitModel::StationConnectionState ampState;
        ampState.configuredHost = stationAddress;
        ampState.configuredPort = 8080;
        rfKit.setStationConnectionState(ampState);
        RfKitBandFollow follow(&rfKit);
        follow.setServer(controller.server());
        QTRY_VERIFY(rfKit.bandFollow() != BandFollow::Off);
        QCOMPARE(rfKit.bandFollowAddress(), stationAddress);
        QWebSocket device;
        QStringList frames;
        connect(&device, &QWebSocket::textMessageReceived, &device,
                [&frames](const QString& text) { frames.append(text); });
        device.open(QUrl(QStringLiteral("ws://%1:%2").arg(stationAddress).arg(port)));
        QTRY_VERIFY(frames.join(QString()).contains(QStringLiteral("receive_only:true;")));
        device.close();

        blocker.close();
        QTRY_VERIFY_WITH_TIMEOUT(controller.server()->listenAddresses().contains(
                                     QHostAddress(QHostAddress::LocalHost)), 5000);
        QTRY_VERIFY(state.error().isEmpty());
        QCOMPARE(starts.count(), 1);
        QCOMPARE(stops.count(), 0);
        QVERIFY(controller.setEnabled(false, port, &reason));
    }

    // The TCI page's line, in user words.
    void stationLineReadsInUserWords()
    {
        RadioModel window(RadioModel::Role::Remote);
        FakeStationLink link;
        window.attachStation(&link);
        StationTciModel::State state;
        QVERIFY(TciSwitch::stationLine(&window).isEmpty());   // switch off at the Core
        state.enabled = true;
        state.port = 50001;
        window.stationTciModel()->setState(state);
        QCOMPARE(TciSwitch::stationLine(&window),
                 QStringLiteral("The Core's TCI server is not running."));
        state.listening = true;
        state.stationAddress = QStringLiteral("192.168.1.20");
        window.stationTciModel()->setState(state);
        QCOMPARE(TciSwitch::stationLine(&window),
                 QStringLiteral("Also at the Core: 192.168.1.20, port 50001"));
        link.coreHere = true;
        QCOMPARE(TciSwitch::stationLine(&window),
                 QStringLiteral("The Core on this computer serves TCI apps here, port 50001."));
        link.coreHere = false;
        link.tciAvailable = false;
        QVERIFY(TciSwitch::stationLine(&window).isEmpty());
        link.tciAvailable = true;
        for (const QString& line : {TciSwitch::stationLine(&window),
                                    StationTciModel::readOnlyReason()}) {
            QVERIFY2(OperatorWording::isPlain(line), qPrintable(line));
        }
        RadioModel local;
        QVERIFY(TciSwitch::stationLine(&local).isEmpty());
    }
};

QTEST_MAIN(StationTciServerTest)
#include "tst_station_tci_server.moc"
