// no-port-check: NereusSDR-original. Remote TGXL Peripherals UI coverage.
// 2026-09-23: R-R3-47 / R-R3-22: the Power Genius and RF-Kit applets read
// the Core's `amplifier` and `rfkit` objects in a remote window (filled on
// attach, updated, false and zero shown, stale on Core loss, no accessory
// socket opened) and the same objects in-process in a local window. J.J.
// Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-47 / R-R3-22 / R-R3-25: a remote window connects,
// disconnects and configures the Core's Power Genius through the Core (the
// Peripherals row and the Power Genius tab), opening no socket of its own;
// a receive-only Core refuses the amp's operate and the tuner's operate,
// bypass and antenna writes, changing nothing and sending nothing. J.J.
// Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-47 / R-R3-48 / R-R3-22 / R-R3-25: a remote window
// switches, connects and disconnects the Core's RF-Kit through the Core (the
// RF-Kit page and the applet), sees its tuner, antenna and band-follow rows,
// and a raw write of the RF-Kit switch is refused in plain words; the Power
// Genius's band-follow line, local and remote; the one TCI switch turns the
// Core's station TCI server on and off, which keeps running when the window
// goes and another connects. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
// Claude Code.
// 2026-09-24: R-R3-47 / R-R3-22: a remote window's Advanced pages are views
// of the Core's records (they open no accessory connection): faults raised
// on the Core appear without a reconnect and are cleared through the Core,
// the counters are the Core's and not the window's, the output limit is
// set through the Core and its alert reaches the window, the antenna names
// and tune memory are the Core's; the interlock policy is changed from a
// remote window, applied and enforced on the Core, and shown by every
// window; an older Core leaves the interlock section saying why. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-47 / R-R3-22: every control on a remote window's Power
// Genius and Tuner Genius Advanced pages works through the Core: a fake
// amp and tuner on the Core record the bytes, which are the ones a local
// window's page sends; network changes and Save & Reboot ask first; the
// device's answers, its values and the Core's refusals (on their own route)
// show on the page; an older Core leaves the controls saying why. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-22 / R-R3-47: the Power Genius and RF-Kit applets'
// Disconnect and Reconnect send the Core's verbs from a remote window, show
// the Core's connection as it changes and a refusal's plain reason, and
// say why when the Core does not offer them; a local window is unchanged.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-49 / R-R3-47: a remote window's Tuner Genius applet
// switches the Core's tuner antenna and OPERATE through the Core
// (remoteTgxlControlVersion 2), follows the Core's report and its transmit
// state, keeps TUNE with remote transmit, and stays greyed on an older
// Core. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-49 fix wave: the Core keys through its MoxController (a
// MOX click and the radio's PTT input) and the window greys ANT and OPERATE
// from the radio's `transmitting`; a raw write of it is refused. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-49 (parity Task 1): the applet's "on the air" is
// RadioModel::isCoreOnAir(); the Core's two-tone test and its TUNE grey ANT
// and OPERATE in the window too. J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code.
// 2026-09-25: R-R3-49 (parity Task 8): a remote window's relay bars move
// the Core's tuner relays (the bars follow the tuner), Recall tune memory
// and Open TGXL Advanced work there, the Advanced and Interlock entries
// open their 4O3A tab, the Peripherals row scans the Core's network and
// keeps a typed address on the Core, and Copy diagnostics copies the Core's
// connection. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-25: R-R3-49 (parity Task 9): a remote window's Power Genius
// OPERATE (applet and 4O3A tab) asks the Core and follows the amp's report,
// waits on the air; the Peripherals row scans for the amp and keeps its
// address on the Core; Copy diagnostics copies the Core's amp connection.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-25: R-R3-49 (parity Task 10): a remote window's RF-Kit OPERATE and
// ANT 1 to 4 (applet) and "Set amp to TCI mode" (page) ask the Core, follow
// the amp's report and wait on the air; Save keeps a changed Host and Port
// on the Core without dialling; Copy diagnostics and Live diagnostics show
// the Core's counts; a local window's Live diagnostics shows the same
// readings. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-25: R-R3-49 (parity mini-round, the operator's rulings a to c):
// Scan LAN, Host, Port and Save are taken on the air in a remote window; a
// local window's "Set amp to TCI mode" waits on the air; a local click
// refused as the radio unkeys shows the remote window's reason. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-26: iPhone app plan Task 77 fix round 3 (R-IOS-02, R-IOS-03,
// R-IOS-13): the Power Genius's OPERATE waits while the Tuner Genius tunes
// in both windows, and a faulted amp is sent standby. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// 2026-09-26: iPhone app plan Task 77 fix round 4: an amp whose operate=1
// is unconfirmed is sent standby from both local buttons. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-29: load finding: the nothing-happens checks wait for the link's
// own flush bound (SessionWait.h), not a number picked by hand. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.

#include <QtTest>

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cmath>
#include <memory>

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QTabWidget>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocket>
#include <QMenu>
#include <QRadioButton>
#include <QSlider>
#include <QTimer>
#include <QApplication>
#include <QDialog>
#include <QTableWidget>
#include <QWheelEvent>

#include "OperatorWording.h"
#include "core/PgxlConnection.h"
#include "core/ConnectionDiagnostics.h"
#include "core/FaultLog.h"
#include "core/TuneMemoryStore.h"
#include "core/TxInterlockPolicy.h"
#include "core/TgxlConnection.h"
#include "core/SmartSdrApiListener.h"
#include "core/StationPgxlController.h"
#include "core/StationTgxlController.h"
#include "core/LanDiscovery.h"
#include "core/MoxController.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/AppSettings.h"
#include "core/Rf2ksConnection.h"
#include "core/session/IStationLink.h"
#include "core/session/PureSignalSessionFacade.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceStore.h"
#include "core/settings/SettingsProxy.h"
#include "gui/applets/AmpApplet.h"
#include "gui/applets/Rf2ksApplet.h"
#include "gui/applets/TunerApplet.h"
#include "gui/setup/CatNetworkSetupPages.h"
#include "gui/setup/FourO3APage.h"
#include "gui/setup/RfKitPage.h"
#include "gui/setup/PgxlAdvancedPage.h"
#include "gui/setup/PgxlInterlockPage.h"
#include "gui/setup/TgxlAdvancedPage.h"
#include "gui/SetupDialog.h"
#include "gui/LanScanDialog.h"
#include "gui/RelayBar.h"
#include "gui/PgxlSaveRebootDialog.h"
#include "models/AccessorySettingsModel.h"
#include "models/AccessoryDataModel.h"
#include "models/AmplifierModel.h"
#include "models/RadioModel.h"
#include "models/RfKitModel.h"
#include "models/StationTciModel.h"
#include "core/StationTciController.h"
#include "core/TciServer.h"
#include "core/TciSwitch.h"
#include "gui/OperatorReasonText.h"
#include "models/TunerModel.h"

#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "SessionWait.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

class RecordingTgxlLink final : public IStationLink {
public:
    bool available{true};
    bool fourO3AAvailable{false};
    int configureCalls{0};
    int disconnectCalls{0};
    int fourO3ACalls{0};
    bool requestedFourO3AEnabled{false};
    CommandOutcome fourO3AOutcome{true, {}};
    QString configuredHost;
    quint16 configuredPort{0};

    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }

    CommandOutcome requestConfigureTgxl(const QString& host, quint16 port) override
    {
        ++configureCalls;
        configuredHost = host;
        configuredPort = port;
        return {true, {}};
    }

    CommandOutcome requestDisconnectTgxl() override
    {
        ++disconnectCalls;
        return {true, {}};
    }

    bool remoteTgxlConfigAvailable() const override { return available; }

    // R-R3-49: the Tuner Genius switches (remoteTgxlControlVersion 2, and
    // 3 when the Core applies OPERATE whole). Each request is recorded.
    bool tgxlControl{false};
    bool tgxlWhole{false};
    QStringList tgxlRequests;
    bool tgxlControlAvailable() const override { return tgxlControl; }
    // Task 77: the Core runs TUNE for this window (tx.tunerTune).
    bool autotune{false};
    bool tgxlAutotuneAvailable() const override { return autotune; }
    bool tgxlOperateAppliesWhole() const override { return tgxlControl && tgxlWhole; }
    CommandOutcome requestTgxlAntenna(int port) override
    {
        if (!tgxlControl) { return IStationLink::requestTgxlAntenna(port); }
        tgxlRequests.append(QStringLiteral("antenna %1").arg(port));
        return {true, {}};
    }
    CommandOutcome requestTgxlOperate(bool on) override
    {
        if (!tgxlControl) { return IStationLink::requestTgxlOperate(on); }
        tgxlRequests.append(QStringLiteral("operate %1").arg(on ? 1 : 0));
        return {true, {}};
    }
    CommandOutcome requestTgxlBypass(bool on) override
    {
        if (!tgxlControl) { return IStationLink::requestTgxlBypass(on); }
        tgxlRequests.append(QStringLiteral("bypass %1").arg(on ? 1 : 0));
        return {true, {}};
    }
    // Group B fix wave (M1): the saved address (remoteTgxlControlVersion 4).
    bool tgxlFull{false};
    int tgxlAddressCalls{0};
    bool tgxlFullControlAvailable() const override { return tgxlControl && tgxlFull; }
    CommandOutcome requestTgxlAddress(const QString& host, int port) override
    {
        if (!tgxlFullControlAvailable()) { return IStationLink::requestTgxlAddress(host, port); }
        ++tgxlAddressCalls;
        return {true, {}};
    }

    CommandOutcome requestFourO3AEnabled(bool enabled) override
    {
        ++fourO3ACalls;
        requestedFourO3AEnabled = enabled;
        return fourO3AOutcome;
    }
    bool remoteFourO3AControlAvailable() const override { return fourO3AAvailable; }

    // R-R3-47: the link's state for the amplifier and RF-Kit readings.
    bool linkReady{false};
    bool amplifierStatus{false};
    bool rfKitStatus{false};
    bool stationLinkReady() const override { return linkReady; }
    bool remoteAmplifierStatusAvailable() const override { return amplifierStatus; }
    bool remoteRfKitStatusAvailable() const override { return rfKitStatus; }

    // R-R3-47: the Power Genius commands.
    bool pgxlAvailable{false};
    CommandOutcome pgxlOutcome{true, {}};
    int pgxlConfigureCalls{0};
    int pgxlDisconnectCalls{0};
    int pgxlSettingsCalls{0};
    QString pgxlHost;
    quint16 pgxlPort{0};
    bool pgxlAutoReconnect{true};
    int pgxlKeepaliveSec{0};
    int pgxlPingSec{-1};
    bool remotePgxlControlAvailable() const override { return pgxlAvailable; }
    // R-R3-22 fix wave: each sent command gets its own id, as StationClient
    // gives it, so an applet can tell its own command's result apart.
    quint32 nextCommandId{100};
    quint32 lastPgxlCommandId{0};
    quint32 lastRfKitCommandId{0};
    CommandOutcome stamp(CommandOutcome outcome, quint32& last)
    {
        if (outcome.sent) {
            outcome.commandId = nextCommandId++;
            last = outcome.commandId;
        }
        return outcome;
    }
    CommandOutcome requestConfigurePgxl(const QString& host, quint16 port) override
    {
        ++pgxlConfigureCalls;
        pgxlHost = host;
        pgxlPort = port;
        return stamp(pgxlOutcome, lastPgxlCommandId);
    }
    CommandOutcome requestDisconnectPgxl() override
    {
        ++pgxlDisconnectCalls;
        return stamp(pgxlOutcome, lastPgxlCommandId);
    }
    CommandOutcome requestPgxlConnectionSettings(bool autoReconnect, int keepaliveSec,
                                                 int pingSec) override
    {
        ++pgxlSettingsCalls;
        pgxlAutoReconnect = autoReconnect;
        pgxlKeepaliveSec = keepaliveSec;
        pgxlPingSec = pingSec;
        return {true, {}};
    }

    // R-R3-22: the RF-Kit commands, and what a request answers with.
    bool rfKitAvailable{false};
    int rfKitConfigureCalls{0};
    int rfKitDisconnectCalls{0};
    QString rfKitHost;
    quint16 rfKitPort{0};
    CommandOutcome accessoryOutcome{true, {}};
    bool remoteRfKitControlAvailable() const override { return rfKitAvailable; }
    CommandOutcome requestConfigureRfKit(const QString& host, quint16 port) override
    {
        ++rfKitConfigureCalls;
        rfKitHost = host;
        rfKitPort = port;
        return stamp(accessoryOutcome, lastRfKitCommandId);
    }
    CommandOutcome requestDisconnectRfKit() override
    {
        ++rfKitDisconnectCalls;
        return stamp(accessoryOutcome, lastRfKitCommandId);
    }
};

// Status lines and REST replies as the repository's parser tests carry
// them (tst_pgxl_connection_parse, tst_rf2ks_connection_parse); the
// transmit frame uses the same keys, 60 dBm (1000 W) and -24.5 dB (1.13).
QMap<QString, QString> pgxlFrame(const char* line)
{
    QMap<QString, QString> kvs;
    const QString body = QString::fromLatin1(line);
    for (const QString& part : body.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        const int eq = part.indexOf(QLatin1Char('='));
        if (eq > 0) {
            kvs.insert(part.left(eq), part.mid(eq + 1));
        }
    }
    return kvs;
}
constexpr const char* kOperate = "state=OPERATE temp=42.5 vac=240 fwd=1480.0 swr=2.1";
constexpr const char* kTransmit = "state=TRANSMIT_A peakfwd=60.0 swr=-24.5 id=22.5";
constexpr const char* kStandby = "state=STANDBY peakfwd=60.0 swr=-24.5 id=0.0";
constexpr const char* kRfKitPower =
    R"({"temperature":{"value":27.0,"unit":"°C"},"voltage":{"value":52.7,"unit":"V"},"current":{"value":0.0,"unit":"A"},"forward":{"value":850,"max_value":1200,"unit":"W"},"reflected":{"value":3,"max_value":20,"unit":"W"},"swr":{"value":1.4,"max_value":2.1,"unit":""}})";
constexpr const char* kRfKitIdle =
    R"({"temperature":{"value":0.0,"unit":"°C"},"voltage":{"value":0.0,"unit":"V"},"current":{"value":0.0,"unit":"A"},"forward":{"value":0,"max_value":0,"unit":"W"},"reflected":{"value":0,"max_value":0,"unit":"W"},"swr":{"value":1.0,"max_value":1.0,"unit":""}})";

RfKitPowerSnapshot rfKitPower(int forwardW, float swr, float tempC, float volts, float amps)
{
    RfKitPowerSnapshot snap;
    snap.forwardW = forwardW;
    snap.swr = swr;
    snap.temperatureC = tempC;
    snap.voltageV = volts;
    snap.currentA = amps;
    return snap;
}

// R-R3-47: the RF-Kit's REST interface on the loopback (the /info body is
// tst_rf2ks_connection_parse's). R-R3-49 (parity Task 10): a PUT changes
// what the next GET reports, as the amp does, and is recorded with its body.
class FakeRfKit : public QTcpServer {
public:
    FakeRfKit()
    {
        listen(QHostAddress::LocalHost, 0);
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* sock = nextPendingConnection()) {
                connect(sock, &QTcpSocket::readyRead, this, [this, sock] { serve(sock); });
            }
        });
    }
    int requests{0};
    QStringList lines;    // "GET /info", "POST /error/reset": what reached the amp
    QStringList writes;   // "PUT /operate-mode {...}": the changes, with their bodies
    QString operateMode{QStringLiteral("STANDBY")};
    int activeAntenna{1};
    QString operationalInterface{QStringLiteral("UDP")};

private:
    void serve(QTcpSocket* sock)
    {
        QByteArray& buffer = m_buffers[sock];
        buffer += sock->readAll();
        const int headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) { return; }
        int length = 0;
        for (const QByteArray& line : buffer.left(headerEnd).split('\n')) {
            if (line.toLower().startsWith("content-length:")) {
                length = line.mid(15).trimmed().toInt();
            }
        }
        if (buffer.size() < headerEnd + 4 + length) { return; }
        const QByteArray req = buffer;
        m_buffers.remove(sock);
        const int sp = req.indexOf(' ') + 1;
        const QByteArray verb = req.left(sp - 1);
        const QByteArray path = req.mid(sp, req.indexOf(' ', sp) - sp);
        const QByteArray payload = req.mid(headerEnd + 4, length);
        ++requests;
        lines.append(QString::fromLatin1(req.left(sp) + path));
        if (verb == "PUT") {
            writes.append(QString::fromLatin1(verb + ' ' + path + ' ') + QString::fromUtf8(payload));
            const QJsonObject o = QJsonDocument::fromJson(payload).object();
            if (path == "/operate-mode") {
                operateMode = o.value(QStringLiteral("operate_mode")).toString();
            } else if (path == "/antennas/active") {
                activeAntenna = o.value(QStringLiteral("number")).toInt();
            } else if (path == "/operational-interface") {
                operationalInterface = o.value(QStringLiteral("operational_interface")).toString();
            }
        }
        QByteArray body = "{}";
        if (verb != "GET") {
            body.clear();
        } else if (path == "/info") {
            body = R"({"device":"RF2K-S","software_version":{"GUI":200,"controller":267},"custom_device_name":"KG4VCF"})";
        } else if (path == "/operate-mode") {
            body = QJsonDocument(QJsonObject{{QStringLiteral("operate_mode"), operateMode}})
                       .toJson(QJsonDocument::Compact);
        } else if (path == "/antennas/active") {
            body = QJsonDocument(QJsonObject{{QStringLiteral("type"), QStringLiteral("INTERNAL")},
                                             {QStringLiteral("number"), activeAntenna}})
                       .toJson(QJsonDocument::Compact);
        } else if (path == "/operational-interface") {
            body = QJsonDocument(QJsonObject{
                       {QStringLiteral("operational_interface"), operationalInterface},
                       {QStringLiteral("error"), QString()}})
                       .toJson(QJsonDocument::Compact);
        }
        sock->write("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                    + QByteArray::number(body.size()) + "\r\n\r\n" + body);
        sock->flush();
        sock->disconnectFromHost();
    }
    QHash<QTcpSocket*, QByteArray> m_buffers;
};

// A loopback stand-in for the Power Genius or Tuner Genius: records every
// command line it receives (the bytes on the wire) and answers on request.
class FakeGenius : public QObject {
public:
    QTcpServer server;
    QPointer<QTcpSocket> peer;
    QStringList commands;          // the command part of each C<seq>|<command> line
    QList<quint32> sequences;
    QByteArray pending;

    bool listen() { return server.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return server.serverPort(); }
    bool accept()
    {
        if (!QTest::qWaitFor([this] { return server.hasPendingConnections(); }, 3000)) {
            return false;
        }
        peer = server.nextPendingConnection();
        connect(peer.data(), &QTcpSocket::readyRead, this, [this] { read(); });
        return true;
    }
    void read()
    {
        pending += peer->readAll();
        qsizetype nl = 0;
        while ((nl = pending.indexOf('\n')) >= 0) {
            const QString line = QString::fromUtf8(pending.left(nl)).trimmed();
            pending.remove(0, nl + 1);
            const qsizetype bar = line.indexOf(QLatin1Char('|'));
            if (line.startsWith(QLatin1Char('C')) && bar > 1) {
                sequences.append(line.mid(1, bar - 1).toUInt());
                commands.append(line.mid(bar + 1));
            }
        }
    }
    void send(const QString& line)
    {
        peer->write(line.toUtf8() + '\n');
        peer->flush();
    }
    /// Index of the first `command` received at or after `from`; -1 if none
    /// arrives in time.
    int waitFor(const QString& command, int from = 0)
    {
        int found = -1;
        (void)QTest::qWaitFor([&] {   // -1 when it never arrives
            for (int i = from; i < commands.size(); ++i) {
                if (commands.at(i) == command) {
                    found = i;
                    return true;
                }
            }
            return false;
        }, 3000);
        return found;
    }
    void reply(int index, const QString& codeAndBody)
    {
        send(QStringLiteral("R%1|%2").arg(sequences.at(index)).arg(codeAndBody));
    }
    /// The commands that change or read the device's own settings, from
    /// `from` on (status polls, keepalive and pairing left out).
    QStringList settingsCommands(int from) const
    {
        QStringList out;
        for (int i = from; i < commands.size(); ++i) {
            const QString& c = commands.at(i);
            if (c.startsWith(QLatin1String("setup ")) || c.startsWith(QLatin1String("ifconf "))
                || c == QLatin1String("save")) {
                if (out.isEmpty() || out.last() != c) {   // one per change
                    out.append(c);
                }
            }
        }
        return out;
    }
};

quint16 freeLoopbackPort()
{
    QTcpServer reservation;
    if (!reservation.listen(QHostAddress::LocalHost, 0)) { return 0; }
    const quint16 port = reservation.serverPort();
    reservation.close();
    return port;
}

TunerModel::StationConnectionState state(TunerModel::ConnectionPhase phase,
                                         const QString& host = {},
                                         quint16 port = 0,
                                         const QString& error = {})
{
    TunerModel::StationConnectionState result;
    result.phase = phase;
    result.configuredHost = host;
    result.configuredPort = port;
    result.error = error;
    return result;
}

// R-R3-22: the applet menu's Disconnect or Reconnect item.
QAction* connectionToggle(QMenu* menu)
{
    for (QAction* a : menu->actions()) {
        if (a->text() == QStringLiteral("Connect") || a->text() == QStringLiteral("Disconnect")
            || a->text() == QStringLiteral("Cancel")) {
            return a;
        }
    }
    return nullptr;
}

} // namespace

class RemotePeripheralsTest : public QObject {
    Q_OBJECT

private slots:
    // Rework: this binary's own settings file, since some tests use the
    // process's AppSettings as the Core's store (and save it).
    void initTestCase()
    {
        AppSettings::setProfileOverride(
            QStringLiteral("tst-remote-peripherals-%1").arg(QCoreApplication::applicationPid()));
    }
    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
        QDir().rmdir(QFileInfo(path).absolutePath());
    }
    void remoteTgxlDraftUsesStationLinkAndSurvivesUnrelatedSnapshots();
    void unknownCapabilityKeepsRemoteTgxlInert();
    void remoteParentPageExposesOnlyStationBackedControls();
    void remoteMasterShowsPendingAndRefusalWithoutLocalActivation();
    void coreTunerErrorsAreShownInUserWords();
    void remoteAmpAndRfKitAppletsFollowTheCore();
    void remoteAppletsSayWhyReadingsAreNotLive();
    void localAppletsShowTheSameValuesAsBefore();
    void remoteAppletsConnectAndDisconnectThroughTheCore();
    void remotePgxlRowAndTabUseTheStationLink();
    void remoteWindowSetsUpThePgxlThroughTheCore();
    void receiveOnlyCoreRefusesTunerAndAmpOperation();
    void remoteWindowSetsUpTheRfKitThroughTheCore();
    void rawRfKitSwitchWriteIsRefused();
    void pgxlBandFollowLineLocalAndRemote();
    void oneTciSwitchDrivesTheCoresStationServer();
    void coreHereServesThisComputersApps();
    void upgradeKeepsTciOnTheCoresComputer();
    void coresStoredSwitchWinsOverTheLink();
    void connectRuleReadsTheCurrentCore();
    void tciPortIsSentWhenEditingFinishes();
    void localPagesAskBeforeNetworkSettings();
    // R-R3-47 / R-R3-22
    void remoteWindowShowsTheCoresRecords();
    void remoteWindowChangesTheInterlockOnTheCore();
    void olderCoreLeavesTheInterlockSayingWhy();
    void remoteWindowChangesTheAmpsOwnSettingsThroughTheCore();
    void remoteWindowChangesTheTunersOwnSettingsThroughTheCore();
    void olderCoreLeavesTheDeviceSettingsSayingWhy();
    void accessoryRefusalsNeverReachTheSliceToast();
    void remoteRfKitPageWorksEveryControl();
    void olderCoreLeavesTheRfKitSettingsSayingWhy();
    void refusalClaimsEndWithTheLink();
    void ampAppletRefusalShownOnTheAppletIsNotToasted();
    void remoteWindowSwitchesTheTunerThroughTheCore();
    void remoteWindowMovesTheTunerRelaysThroughTheCore();
    void remoteWindowScansAndKeepsTheTunerAddressOnTheCore();
    void remoteTunerMenuRecallsOpensAdvancedAndCopiesTheCore();
    void advancedAndInterlockEntriesOpenTheirFourO3ATab();
    void olderCoreLeavesTheTunerSwitchesGreyed();
    void aWindowThatMayTransmitNeverFallsBackToItsOwnTuner();
    void remoteTuneWaitsWhileTheCoresTuneIsOn();
    void aNameSavedWithSpacesStillTakesEdits();
    void operateFromStandbyIsOneRequestOnACoreThatAppliesItWhole();
    void remoteWindowOperatesTheAmpThroughTheCore();
    void remoteWindowScansAndKeepsTheAmpAddressOnTheCore();
    void localPowerGeniusTabOperatesThisComputersAmp();
    void remoteWindowOperatesTheRfKitThroughTheCore();
    void olderCoreLeavesTheRfKitSwitchesGreyed();
    void localRfKitPageShowsTheRemoteReadings();
    void pageThatOutlivesItsModelSendsNothing();
    void localWindowAmpAndTunerSwitchesWaitOnTheAir();
    void localClickRefusedAsTheRadioUnkeysSaysWhy();
    void ampOperateWaitsForTheTunerAndAFaultedAmpGoesToStandby();
};

void RemotePeripheralsTest::remoteParentPageExposesOnlyStationBackedControls()
{
    RadioModel model(RadioModel::Role::Remote);
    RecordingTgxlLink link;
    model.attachStation(&link);
    FourO3APage page(&model);
    auto* peripherals = page.findChild<PeripheralsPage*>();
    QVERIFY(peripherals);
    QVERIFY(peripherals->isEnabled());
    // R-R3-47: the Advanced pages are views of the Core's records; they
    // open no connection to an accessory and send it nothing.
    QVERIFY(page.findChild<PgxlAdvancedPage*>());
    QVERIFY(page.findChild<TgxlAdvancedPage*>());
    QVERIFY(page.findChild<PgxlInterlockPage*>());
    QVERIFY(!model.pgxlConnection()->isConnected());
    QVERIFY(!model.tgxlConnection()->isConnected());
    QVERIFY(model.pgxlConnection()->peerAddress().isEmpty());
    // R-R3-47: the Power Genius tab has check boxes of its own now.
    auto* master = page.findChild<QCheckBox*>(QStringLiteral("fourO3AMasterToggle"));
    QVERIFY(master);
    QVERIFY(!master->isEnabled());
    QVERIFY(master->toolTip().contains(QStringLiteral("Core")));
    QVERIFY(QMetaObject::invokeMethod(&page, "onMasterToggled", Qt::DirectConnection,
                                      Q_ARG(bool, true)));
    model.setFourO3AEnabled(true);
    QVERIFY(!model.smartSdrListener()->isListening());

    SetupDialog dialog(&model);
    dialog.selectPage(QStringLiteral("4O3A"));
    auto* actualPage = dialog.findChild<FourO3APage*>();
    QVERIFY(actualPage);
    QVERIFY(actualPage->isEnabled());
    QVERIFY(actualPage->findChild<PeripheralsPage*>()->isEnabled());
}

void RemotePeripheralsTest::remoteMasterShowsPendingAndRefusalWithoutLocalActivation()
{
    RadioModel model(RadioModel::Role::Remote);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:99");
    model.setLastRadioInfoForTest(info);
    model.setConnectionStateForTest(ConnectionState::Connected);
    RecordingTgxlLink link;
    link.fourO3AAvailable = true;
    model.attachStation(&link);

    FourO3APage page(&model);
    auto* master = page.findChild<QCheckBox*>(QStringLiteral("fourO3AMasterToggle"));
    auto* status = page.findChild<QLabel*>(QStringLiteral("fourO3AListenerStatus"));
    QVERIFY(master && status);
    QVERIFY(master->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(&page, "onMasterToggled", Qt::DirectConnection,
                                      Q_ARG(bool, true)));
    QCOMPARE(link.fourO3ACalls, 1);
    QVERIFY(link.requestedFourO3AEnabled);
    QVERIFY(!master->isChecked());
    QVERIFY(!master->isEnabled());
    QVERIFY(master->text().contains(QStringLiteral("pending")));
    QVERIFY(!model.smartSdrListener()->isListening());

    model.reportStationFourO3ACommandFinished(false, QStringLiteral("Core refused test request"));
    QVERIFY(master->isEnabled());
    QVERIFY(!master->text().contains(QStringLiteral("pending")));
    QVERIFY(status->text().contains(QStringLiteral("refused test request")));
    // R-R3-21: a refusal in the Core's own terms is shown in user words.
    model.reportStationFourO3ACommandFinished(
        false, QStringLiteral("Remote 4O3A control requires a newer station protocol."));
    QVERIFY2(status->text().contains(QStringLiteral("Update this app to control 4O3A on this Core.")),
             qPrintable(status->text()));
    QVERIFY(OperatorWording::isPlain(status->text()));
    QVERIFY(OperatorWording::isPlain(master->toolTip()));
    model.reportStationFourO3ACommandFinished(false, QStringLiteral("Core refused test request"));
    QVERIFY(QMetaObject::invokeMethod(&page, "onMasterToggled", Qt::DirectConnection,
                                     Q_ARG(bool, true)));
    QVERIFY(!master->isEnabled());
    link.fourO3AAvailable = false;
    model.reportStationLinkStateChanged();
    QVERIFY(!master->text().contains(QStringLiteral("pending")));
    link.fourO3AAvailable = true;
    model.reportStationLinkStateChanged();
    QVERIFY(master->isEnabled());
    NEREUS_TRY_VERIFY(!status->text().contains(QStringLiteral("refused test request")));
}

void RemotePeripheralsTest::remoteTgxlDraftUsesStationLinkAndSurvivesUnrelatedSnapshots()
{
    RadioModel model(RadioModel::Role::Remote);
    RecordingTgxlLink link;
    model.attachStation(&link);

    PeripheralsPage page(&model);
    auto* host = page.findChild<QLineEdit*>(QStringLiteral("tgxlHostEdit"));
    auto* port = page.findChild<QSpinBox*>(QStringLiteral("tgxlPortSpin"));
    auto* connect = page.findChild<QPushButton*>(QStringLiteral("tgxlConnectButton"));
    auto* scan = page.findChild<QPushButton*>(QStringLiteral("tgxlScanButton"));
    auto* status = page.findChild<QLabel*>(QStringLiteral("tgxlStatusLabel"));
    QVERIFY(host && port && connect && scan && status);
    QVERIFY(!scan->isEnabled());

    QSignalSpy tgxlFrames(model.tgxlConnection(), &TgxlConnection::testFrameWrittenForTesting);
    QSignalSpy pgxlFrames(model.pgxlConnection(), &PgxlConnection::testFrameWrittenForTesting);
    host->setText(QStringLiteral("draft.station.example"));
    port->setValue(9021);

    // A phase/error snapshot changes neither Core's configured endpoint nor
    // the operator's unsent draft. It does make the actionable error visible.
    model.tunerModel()->setStationConnectionState(
        state(TunerModel::ConnectionPhase::Error, {}, 0,
              QStringLiteral("wrong device")));
    QCOMPARE(host->text(), QStringLiteral("draft.station.example"));
    QCOMPARE(port->value(), 9021);
    QVERIFY(status->text().contains(QStringLiteral("wrong device")));
    QCOMPARE(connect->text(), QStringLiteral("Connect"));

    // The draft goes to the typed station-link seam exactly once; no Mac-local
    // socket or LAN path is used.
    QVERIFY(QMetaObject::invokeMethod(&page, "onConnect", Qt::DirectConnection,
                                      Q_ARG(int, 0)));
    QCOMPARE(link.disconnectCalls, 0);
    QCOMPARE(link.configureCalls, 1);
    QCOMPARE(link.configuredHost, QStringLiteral("draft.station.example"));
    QCOMPARE(link.configuredPort, quint16{9021});
    QCOMPARE(tgxlFrames.count(), 0);
    QCOMPARE(pgxlFrames.count(), 0);

    // An active snapshot changes the same action to Core-side cancellation
    // without replacing the still-unacknowledged draft.
    model.tunerModel()->setStationConnectionState(
        state(TunerModel::ConnectionPhase::Retrying, {}, 0,
              QStringLiteral("wrong device")));
    QCOMPARE(host->text(), QStringLiteral("draft.station.example"));
    QCOMPARE(port->value(), 9021);
    QCOMPARE(connect->text(), QStringLiteral("Cancel"));
    QVERIFY(status->text().contains(QStringLiteral("wrong device")));
    QVERIFY(QMetaObject::invokeMethod(&page, "onConnect", Qt::DirectConnection,
                                      Q_ARG(int, 0)));
    QCOMPARE(link.disconnectCalls, 1);

    // Once Core reports a different configured endpoint, it owns the visible
    // fields. A later snapshot remains the sole source of that confirmation.
    model.tunerModel()->setStationConnectionState(
        state(TunerModel::ConnectionPhase::Disconnected,
              QStringLiteral("core.station.example"), 9010));
    QCOMPARE(host->text(), QStringLiteral("core.station.example"));
    QCOMPARE(port->value(), 9010);
    QCOMPARE(connect->text(), QStringLiteral("Connect"));
    // A connected snapshot turns the same UI action into a Core-side
    // disconnect, without changing the configured endpoint locally.
    model.tunerModel()->setStationConnectionState(
        state(TunerModel::ConnectionPhase::Connected,
              QStringLiteral("core.station.example"), 9010));
    QCOMPARE(connect->text(), QStringLiteral("Disconnect"));
    QVERIFY(QMetaObject::invokeMethod(&page, "onConnect", Qt::DirectConnection,
                                      Q_ARG(int, 0)));
    QCOMPARE(link.disconnectCalls, 2);
    QCOMPARE(tgxlFrames.count(), 0);
    QCOMPARE(pgxlFrames.count(), 0);

    // A remote model has no connected radio MAC, so typing did not create a
    // local per-radio peripheral setting either.
    QCOMPARE(model.peripheralValue(QStringLiteral("TGXL_ManualIp"),
                                   QStringLiteral("not-written")),
             QStringLiteral("not-written"));
}

void RemotePeripheralsTest::unknownCapabilityKeepsRemoteTgxlInert()
{
    RadioModel model(RadioModel::Role::Remote);
    RecordingTgxlLink link;
    link.available = false;
    model.attachStation(&link);

    PeripheralsPage page(&model);
    auto* connect = page.findChild<QPushButton*>(QStringLiteral("tgxlConnectButton"));
    auto* status = page.findChild<QLabel*>(QStringLiteral("tgxlStatusLabel"));
    QVERIFY(connect && status);
    QVERIFY(!connect->isEnabled());
    QCOMPARE(status->text(),
             QStringLiteral("This Core does not offer Tuner Genius XL control to this app."));
    QVERIFY2(OperatorWording::isPlain(status->text()), qPrintable(status->text()));

    // Capability negotiation can complete without a radio connection-state
    // change; the dedicated station-link signal must refresh this row.
    link.available = true;
    model.reportStationLinkStateChanged();
    QVERIFY(connect->isEnabled());

    link.available = false;
    model.reportStationLinkStateChanged();
    QVERIFY(QMetaObject::invokeMethod(&page, "onConnect", Qt::DirectConnection,
                                      Q_ARG(int, 0)));
    QCOMPARE(link.configureCalls, 0);
    QCOMPARE(link.disconnectCalls, 0);
}

// R-R3-17, R-R3-21: the Core's Tuner Genius XL errors reach the row in user
// words; the Core's own text (here as StationTgxlController and
// TgxlConnection word it) is unchanged on the wire and kept in the log.
void RemotePeripheralsTest::coreTunerErrorsAreShownInUserWords()
{
    RadioModel model(RadioModel::Role::Remote);
    RecordingTgxlLink link;
    model.attachStation(&link);

    PeripheralsPage page(&model);
    auto* status = page.findChild<QLabel*>(QStringLiteral("tgxlStatusLabel"));
    QVERIFY(status);

    const QString identity = QStringLiteral(
        "Expected TunerGenius/TunerGeniusXL at the connected endpoint; observed PowerGeniusXL "
        "(serial 1234-5678).");
    for (const auto phase : {TunerModel::ConnectionPhase::Error,
                             TunerModel::ConnectionPhase::Retrying}) {
        model.tunerModel()->setStationConnectionState(state(phase, {}, 0, identity));
        QCOMPARE(model.tunerModel()->connectionError(), identity);
        QVERIFY2(status->text().contains(QStringLiteral(
                     "The device at this address is not a Tuner Genius. Check the tuner's "
                     "address and port.")),
                 qPrintable(status->text()));
        QVERIFY2(OperatorWording::isPlain(status->text()), qPrintable(status->text()));
    }

    for (const QString& raw :
         {QStringLiteral("No matching TGXL discovery announcement for 192.0.2.40:9010. Check the "
                         "tuner address, port and station LAN discovery."),
          QStringLiteral("TGXL identity serial mismatch: expected 1234-5678, observed 8765-4321"),
          QStringLiteral("TGXL identity was rejected by station discovery"),
          QStringLiteral("TGXL native info failed with code 3"),
          QStringLiteral("TGXL native info omitted a nonempty serial"),
          QStringLiteral("TGXL native identity timed out"),
          QStringLiteral("TGXL discovery approval timed out for serial 1234-5678")}) {
        model.tunerModel()->setStationConnectionState(
            state(TunerModel::ConnectionPhase::Error, {}, 0, raw));
        QVERIFY2(!status->text().contains(raw), qPrintable(status->text()));
        QVERIFY2(!status->text().contains(QStringLiteral("TGXL")), qPrintable(status->text()));
        QVERIFY2(OperatorWording::isPlain(status->text()), qPrintable(status->text()));
    }

    // An error with no words says where the reason is, not "Error: ".
    model.tunerModel()->setStationConnectionState(
        state(TunerModel::ConnectionPhase::Error, {}, 0, {}));
    QCOMPARE(status->text(), QStringLiteral("Error: The reason is in the log."));
}

// R-R3-47 / R-R3-22: a remote window's Power Genius and RF-Kit applets read
// the Core's objects over the in-process loopback: filled on attach,
// updated, false and zero shown as they are, stale when the Core is lost,
// and no accessory socket or request from this window.
void RemotePeripheralsTest::remoteAmpAndRfKitAppletsFollowTheCore()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RadioModel station;
    station.enableStationAccessoryIdentity();
    station.amplifierModel()->applyStatusFrame(pgxlFrame(kOperate));
    station.rfKitModel()->applyInfo(QStringLiteral("RF2K-S"), QStringLiteral("G200C267"),
                                    QStringLiteral("KG4VCF"));
    station.rfKitModel()->applyPower(rfKitPower(850, 1.4f, 27.0f, 52.7f, 0.0f));
    station.rfKitModel()->applyOperateMode(QStringLiteral("OPERATE"));
    TunerModel::StationConnectionState up;
    up.configuredHost = QStringLiteral("192.0.2.41");
    up.configuredPort = 8080;
    up.phase = TunerModel::ConnectionPhase::Connected;
    up.deviceModel = QStringLiteral("RF2K-S");
    up.deviceVersion = QStringLiteral("G200C267");
    up.deviceNickname = QStringLiteral("KG4VCF");
    station.rfKitModel()->setStationConnectionState(up);
    station.rfKitModel()->applyPower(rfKitPower(850, 1.4f, 27.0f, 52.7f, 0.0f));
    AppSettings stationSettings(dir.filePath(QStringLiteral("station.settings")));
    StationServer server(&station, stationSettings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));

    RadioModel window(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&window, &proxy);
    AmpApplet amp(&window);
    Rf2ksApplet rfKit(&window);
    QSignalSpy pgxlFrames(window.pgxlConnection(), &PgxlConnection::testFrameWrittenForTesting);

    // Not attached yet: nothing live to show.
    QVERIFY(amp.staleIndicatorVisibleForTesting());
    QVERIFY(rfKit.staleIndicatorVisibleForTesting());
    QVERIFY(!amp.operateButtonShownForTesting());

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    QVERIFY(completed.wait(5000) || !completed.isEmpty());
    QVERIFY(client.remoteAmplifierStatusAvailable());
    QVERIFY(client.remoteRfKitStatusAvailable());

    // Filled on attach.
    NEREUS_TRY_COMPARE(amp.tempGaugeValueForTesting(), 42.5);
    QVERIFY(amp.operateButtonShownForTesting());
    QCOMPARE(amp.operateButtonTextForTesting(), QStringLiteral("OPERATE"));
    QCOMPARE(amp.powerLabelTextForTesting(), QStringLiteral("Volts: 240V\u00A0\u00A0Amps: 0.0A"));
    QCOMPARE(amp.fwdGaugeValueForTesting(), 0.0);
    QVERIFY(!amp.staleIndicatorVisibleForTesting());
    NEREUS_TRY_COMPARE(rfKit.fwdGaugeValueForTesting(), 850);
    QVERIFY(rfKit.connectedStateForTesting());
    QCOMPARE(rfKit.operateButtonTextForTesting(), QStringLiteral("OPERATE"));
    QCOMPARE(rfKit.nicknameLabelTextForTesting(), QStringLiteral("KG4VCF  G200C267"));
    QCOMPARE(rfKit.telemetryStripTextForTesting(),
             QStringLiteral("Fwd 850 W  SWR 1.40  53 V  0.0 A"));
    QVERIFY(!rfKit.staleIndicatorVisibleForTesting());

    // Updated as the Core's amps report.
    station.amplifierModel()->applyStatusFrame(pgxlFrame(kTransmit));
    NEREUS_TRY_VERIFY(std::abs(amp.fwdGaugeValueForTesting() - 1000.0) < 1e-3);
    QVERIFY(std::abs(amp.swrGaugeValueForTesting() - 1.12668) < 1e-4);
    QCOMPARE(amp.powerLabelTextForTesting(), QStringLiteral("Volts: 240V\u00A0\u00A0Amps: 22.5A"));

    // False and zero, shown as they are.
    station.amplifierModel()->applyStatusFrame(pgxlFrame(kStandby));
    station.rfKitModel()->applyOperateMode(QStringLiteral("STANDBY"));
    station.rfKitModel()->applyPower(rfKitPower(0, 1.0f, 0.0f, 0.0f, 0.0f));
    NEREUS_TRY_COMPARE(amp.operateButtonTextForTesting(), QStringLiteral("STANDBY"));
    NEREUS_TRY_COMPARE(amp.fwdGaugeValueForTesting(), 0.0);
    QCOMPARE(amp.swrGaugeValueForTesting(), 1.0);
    QCOMPARE(amp.powerLabelTextForTesting(), QStringLiteral("Volts: 240V\u00A0\u00A0Amps: 0.0A"));
    NEREUS_TRY_COMPARE(rfKit.operateButtonTextForTesting(), QStringLiteral("STANDBY"));
    NEREUS_TRY_COMPARE(rfKit.fwdGaugeValueForTesting(), 0);
    QCOMPARE(rfKit.telemetryStripTextForTesting(),
             QStringLiteral("Fwd 0 W  SWR 1.00  0 V  0.0 A"));

    // The Core is lost: the last readings stay, marked stale.
    stationEnd->closeLink(QStringLiteral("test: Core lost"));
    NEREUS_TRY_VERIFY(amp.staleIndicatorVisibleForTesting());
    NEREUS_TRY_VERIFY(rfKit.staleIndicatorVisibleForTesting());
    QCOMPARE(amp.staleIndicatorTextForTesting(),
             QStringLiteral("Core disconnected. Power Genius readings are stale."));
    QCOMPARE(rfKit.staleIndicatorTextForTesting(),
             QStringLiteral("Core disconnected. RF-Kit readings are stale."));
    QVERIFY(OperatorWording::isPlain(amp.staleIndicatorTextForTesting()));
    QVERIFY(OperatorWording::isPlain(rfKit.staleIndicatorTextForTesting()));
    QCOMPARE(amp.operateButtonTextForTesting(), QStringLiteral("STANDBY"));

    // The window opened no accessory connection of its own.
    QCOMPARE(pgxlFrames.count(), 0);
    QVERIFY(!window.pgxlConnection()->isConnected());
    QVERIFY(!window.rfKitConnection()->isConnected());
    QCOMPARE(window.rfKitConnection()->testInFlightReplyCount(), 0);
    QVERIFY(!window.rfKitConnection()->testPollActive());
    QCOMPARE(window.rfKitConnection()->pollsSucceeded() + window.rfKitConnection()->pollsFailed(), 0);
}

// R-R3-47: with the link up but an older Core, the applets say the Core
// does not report the amp; every line is in user words.
void RemotePeripheralsTest::remoteAppletsSayWhyReadingsAreNotLive()
{
    RadioModel model(RadioModel::Role::Remote);
    RecordingTgxlLink link;
    model.attachStation(&link);
    AmpApplet amp(&model);
    Rf2ksApplet rfKit(&model);
    QVERIFY(amp.staleIndicatorVisibleForTesting());
    QVERIFY(amp.staleIndicatorTextForTesting().contains(QStringLiteral("stale")));

    link.linkReady = true;
    model.reportStationLinkStateChanged();
    QCOMPARE(amp.staleIndicatorTextForTesting(),
             QStringLiteral("This Core does not report its Power Genius to this app. "
                            "Updating the Core may help."));
    QCOMPARE(rfKit.staleIndicatorTextForTesting(),
             QStringLiteral("This Core does not report its RF-Kit amplifier to this app. "
                            "Updating the Core may help."));
    for (const QString& text : {amp.staleIndicatorTextForTesting(),
                                rfKit.staleIndicatorTextForTesting(),
                                AmplifierModel::readOnlyReason(),
                                RfKitModel::readOnlyReason()}) {
        QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
    }

    link.amplifierStatus = true;
    link.rfKitStatus = true;
    model.reportStationLinkStateChanged();
    QVERIFY(!amp.staleIndicatorVisibleForTesting());
    QVERIFY(!rfKit.staleIndicatorVisibleForTesting());
}

// R-R3-47: a local window shows what it showed before, now through the
// same AmplifierModel and RfKitModel the Core mirrors, fed by this
// computer's own connections. No stale line locally.
void RemotePeripheralsTest::localAppletsShowTheSameValuesAsBefore()
{
    RadioModel model;
    AmpApplet amp(&model);
    Rf2ksApplet rfKit(&model);
    QVERIFY(!amp.staleIndicatorVisibleForTesting());
    QVERIFY(!rfKit.staleIndicatorVisibleForTesting());
    QVERIFY(!amp.operateButtonShownForTesting());

    PgxlConnection* pgxl = model.pgxlConnection();
    pgxl->injectLineForTesting(QStringLiteral("R1|0|") + QString::fromLatin1(kOperate));
    QCOMPARE(amp.tempGaugeValueForTesting(), 42.5);
    QCOMPARE(amp.operateButtonTextForTesting(), QStringLiteral("OPERATE"));
    QCOMPARE(amp.fwdGaugeValueForTesting(), 0.0);  // latched peak not shown
    pgxl->injectLineForTesting(QStringLiteral("S0|status ") + QString::fromLatin1(kTransmit));
    QVERIFY(std::abs(amp.fwdGaugeValueForTesting() - 1000.0) < 1e-3);
    QVERIFY(std::abs(amp.swrGaugeValueForTesting() - 1.12668) < 1e-4);
    QCOMPARE(amp.powerLabelTextForTesting(), QStringLiteral("Volts: 240V\u00A0\u00A0Amps: 22.5A"));
    pgxl->injectLineForTesting(QStringLiteral("S0|status ") + QString::fromLatin1(kStandby));
    QCOMPARE(amp.fwdGaugeValueForTesting(), 0.0);
    QCOMPARE(amp.swrGaugeValueForTesting(), 1.0);
    QCOMPARE(amp.operateButtonTextForTesting(), QStringLiteral("STANDBY"));
    pgxl->injectLineForTesting(QStringLiteral("R2|0|nickname=ShackAmp fan=auto meffa=off led=65"));
    QCOMPARE(amp.meffLabelTextForTesting(), QStringLiteral("MEffA:\u00A0\u00A0\u00A0off"));

    Rf2ksConnection* conn = model.rfKitConnection();
    conn->injectJsonForTesting(QStringLiteral("/power"), kRfKitPower);
    conn->injectJsonForTesting(QStringLiteral("/operate-mode"), R"({"operate_mode":"OPERATE"})");
    QCOMPARE(rfKit.fwdGaugeValueForTesting(), 850);
    QVERIFY(std::abs(rfKit.swrGaugeValueForTesting() - 1.4f) < 1e-4f);
    QCOMPARE(rfKit.tempGaugeValueForTesting(), 27.0f);
    QCOMPARE(rfKit.telemetryStripTextForTesting(),
             QStringLiteral("Fwd 850 W  SWR 1.40  53 V  0.0 A"));
    QCOMPARE(rfKit.operateButtonTextForTesting(), QStringLiteral("OPERATE"));
    conn->injectJsonForTesting(QStringLiteral("/power"), kRfKitIdle);
    QCOMPARE(rfKit.fwdGaugeValueForTesting(), 0);
    QVERIFY(!rfKit.staleIndicatorVisibleForTesting());
}

// R-R3-22 / R-R3-47: a remote window's Power Genius and RF-Kit applets send
// Disconnect and Reconnect to the Core (the Core owns both connections),
// follow the Core's phase on their own line, show a refusal's plain reason,
// and say why when the Core does not offer them. The window's own
// connections stay closed and the local toggle signal is never raised.
void RemotePeripheralsTest::remoteAppletsConnectAndDisconnectThroughTheCore()
{
    RadioModel model(RadioModel::Role::Remote);
    RecordingTgxlLink link;
    model.attachStation(&link);
    AmpApplet amp(&model);
    Rf2ksApplet rfKit(&model);
    QSignalSpy ampLocalToggles(&amp, &AmpApplet::connectionToggleRequested);
    QSignalSpy rfKitLocalToggles(&rfKit, &Rf2ksApplet::connectionToggleRequested);
    const auto checkToggle = [](QMenu* menu, const QString& text, bool enabled,
                                const QString& tooltip) {
        QAction* toggle = connectionToggle(menu);
        QVERIFY(toggle != nullptr);
        QCOMPARE(toggle->text(), text);
        QCOMPARE(toggle->isEnabled(), enabled);
        // An action with no tooltip of its own shows its text.
        QCOMPARE(toggle->toolTip(), tooltip.isEmpty() ? text : tooltip);
        QVERIFY2(OperatorWording::isPlain(toggle->toolTip()), qPrintable(toggle->toolTip()));
    };

    // The link is still coming up: the items wait for it.
    {
        std::unique_ptr<QMenu> ampMenu(amp.buildContextMenuForTesting());
        std::unique_ptr<QMenu> rfKitMenu(rfKit.buildContextMenuForTesting());
        checkToggle(ampMenu.get(), QStringLiteral("Connect"), false,
                    QStringLiteral("Waiting for the Core to connect."));
        checkToggle(rfKitMenu.get(), QStringLiteral("Connect"), false,
                    QStringLiteral("Waiting for the Core to connect."));
    }
    QVERIFY(amp.connectionLineTextForTesting().isEmpty());

    // An older Core reports the amps but offers no setup: the items say why.
    link.linkReady = true;
    link.amplifierStatus = true;
    link.rfKitStatus = true;
    model.reportStationLinkStateChanged();
    {
        std::unique_ptr<QMenu> ampMenu(amp.buildContextMenuForTesting());
        std::unique_ptr<QMenu> rfKitMenu(rfKit.buildContextMenuForTesting());
        checkToggle(ampMenu.get(), QStringLiteral("Connect"), false,
                    QStringLiteral("This Core does not offer Power Genius XL control to this app."));
        checkToggle(rfKitMenu.get(), QStringLiteral("Connect"), false,
                    QStringLiteral("This Core does not offer RF-Kit amplifier setup to this app."));
        connectionToggle(ampMenu.get())->trigger();
        connectionToggle(rfKitMenu.get())->trigger();
    }
    QCOMPARE(link.pgxlConfigureCalls + link.pgxlDisconnectCalls, 0);
    QCOMPARE(link.rfKitConfigureCalls + link.rfKitDisconnectCalls, 0);

    // A Core that offers setup, with no address saved yet.
    link.pgxlAvailable = true;
    link.rfKitAvailable = true;
    model.amplifierModel()->setStationConnectionState(
        state(TunerModel::ConnectionPhase::Disconnected));
    model.rfKitModel()->setStationConnectionState(
        state(TunerModel::ConnectionPhase::Disconnected));
    model.reportStationLinkStateChanged();
    QCOMPARE(amp.connectionLineTextForTesting(), QStringLiteral("Disconnected"));
    QCOMPARE(rfKit.connectionLineTextForTesting(), QStringLiteral("Disconnected"));
    {
        std::unique_ptr<QMenu> ampMenu(amp.buildContextMenuForTesting());
        std::unique_ptr<QMenu> rfKitMenu(rfKit.buildContextMenuForTesting());
        checkToggle(ampMenu.get(), QStringLiteral("Connect"), false,
                    QStringLiteral("Enter the Power Genius address in Setup first."));
        checkToggle(rfKitMenu.get(), QStringLiteral("Connect"), false,
                    QStringLiteral("Enter the RF-Kit amplifier's address in Setup first."));
    }

    // Connect asks the Core to dial its saved address.
    model.amplifierModel()->setStationConnectionState(
        state(TunerModel::ConnectionPhase::Disconnected, QStringLiteral("192.0.2.40"), 9008));
    model.rfKitModel()->setStationConnectionState(
        state(TunerModel::ConnectionPhase::Disconnected, QStringLiteral("192.0.2.41"), 8080));
    {
        std::unique_ptr<QMenu> ampMenu(amp.buildContextMenuForTesting());
        std::unique_ptr<QMenu> rfKitMenu(rfKit.buildContextMenuForTesting());
        checkToggle(ampMenu.get(), QStringLiteral("Connect"), true, QString());
        checkToggle(rfKitMenu.get(), QStringLiteral("Connect"), true, QString());
        connectionToggle(ampMenu.get())->trigger();
        connectionToggle(rfKitMenu.get())->trigger();
    }
    QCOMPARE(link.pgxlConfigureCalls, 1);
    QCOMPARE(link.pgxlHost, QStringLiteral("192.0.2.40"));
    QCOMPARE(link.pgxlPort, quint16{9008});
    QCOMPARE(link.rfKitConfigureCalls, 1);
    QCOMPARE(link.rfKitHost, QStringLiteral("192.0.2.41"));
    QCOMPARE(link.rfKitPort, quint16{8080});

    // The Core refuses another request (a Setup page's, another id): not
    // the applets' business.
    const QString otherRefusal = QStringLiteral("Choose an output limit from 100 to 2000 W.");
    model.reportStationAccessoryRefusal(QStringLiteral("pgxl"), otherRefusal);
    model.reportStationCommandFinished(7, false, otherRefusal);
    QCOMPARE(amp.connectionLineTextForTesting(), QStringLiteral("Disconnected"));
    QCOMPARE(rfKit.connectionLineTextForTesting(), QStringLiteral("Disconnected"));

    // The Core refuses the applets' own: each shows the plain reason.
    const QString pgxlRefusal = QStringLiteral("Enable 4O3A on Core before connecting the PGXL.");
    const QString rfKitRefusal =
        QStringLiteral("Turn on the RF-Kit amplifier on the Core before connecting it.");
    model.reportStationAccessoryRefusal(QStringLiteral("pgxl"), pgxlRefusal);
    model.reportStationCommandFinished(link.lastPgxlCommandId, false, pgxlRefusal);
    model.reportStationAccessoryRefusal(QStringLiteral("rfkit"), rfKitRefusal);
    model.reportStationCommandFinished(link.lastRfKitCommandId, false, rfKitRefusal);
    QCOMPARE(amp.connectionLineTextForTesting(), OperatorReasonText::forDisplay(pgxlRefusal));
    QCOMPARE(rfKit.connectionLineTextForTesting(), OperatorReasonText::forDisplay(rfKitRefusal));
    QVERIFY2(OperatorWording::isPlain(amp.connectionLineTextForTesting()),
             qPrintable(amp.connectionLineTextForTesting()));
    QVERIFY2(OperatorWording::isPlain(rfKit.connectionLineTextForTesting()),
             qPrintable(rfKit.connectionLineTextForTesting()));

    // Accepted: the Core's phase replaces the refusal as it moves, and a
    // refusal of another page's request does not land on the applet.
    model.amplifierModel()->setStationConnectionState(
        state(TunerModel::ConnectionPhase::Identifying, QStringLiteral("192.0.2.40"), 9008));
    model.rfKitModel()->setStationConnectionState(
        state(TunerModel::ConnectionPhase::Connecting, QStringLiteral("192.0.2.41"), 8080));
    QCOMPARE(amp.connectionLineTextForTesting(), QStringLiteral("Identifying device"));
    QCOMPARE(rfKit.connectionLineTextForTesting(), QStringLiteral("Connecting at the Core"));
    model.reportStationAccessoryRefusal(QStringLiteral("pgxl"),
                                        QStringLiteral("Choose an output limit from 100 to 2000 W."));
    QCOMPARE(amp.connectionLineTextForTesting(), QStringLiteral("Identifying device"));

    // While the Core is trying, the item says Cancel (the word the
    // Peripherals row uses) and cancels the attempt.
    {
        std::unique_ptr<QMenu> ampMenu(amp.buildContextMenuForTesting());
        std::unique_ptr<QMenu> rfKitMenu(rfKit.buildContextMenuForTesting());
        checkToggle(ampMenu.get(), QStringLiteral("Cancel"), true, QString());
        checkToggle(rfKitMenu.get(), QStringLiteral("Cancel"), true, QString());
        connectionToggle(ampMenu.get())->trigger();
        connectionToggle(rfKitMenu.get())->trigger();
    }
    QCOMPARE(link.pgxlDisconnectCalls, 1);
    QCOMPARE(link.rfKitDisconnectCalls, 1);

    // The Core takes the applets' requests with no phase change yet: their
    // commands are done, so a later refusal of an unrelated request (a
    // Setup page's) does not land on the applets' lines.
    model.reportStationCommandFinished(link.lastPgxlCommandId, true, QString());
    model.reportStationCommandFinished(link.lastRfKitCommandId, true, QString());
    model.reportStationAccessoryRefusal(QStringLiteral("pgxl"), otherRefusal);
    model.reportStationCommandFinished(link.nextCommandId + 50, false, otherRefusal);
    model.reportStationAccessoryRefusal(QStringLiteral("rfkit"), otherRefusal);
    model.reportStationCommandFinished(link.nextCommandId + 51, false, otherRefusal);
    QCOMPARE(amp.connectionLineTextForTesting(), QStringLiteral("Identifying device"));
    QCOMPARE(rfKit.connectionLineTextForTesting(), QStringLiteral("Connecting at the Core"));

    // Connected, then a drop the Core retries, in user words.
    model.amplifierModel()->setStationConnectionState(
        state(TunerModel::ConnectionPhase::Connected, QStringLiteral("192.0.2.40"), 9008));
    QCOMPARE(amp.connectionLineTextForTesting(), QStringLiteral("Connected"));
    model.rfKitModel()->setStationConnectionState(
        state(TunerModel::ConnectionPhase::Retrying, QStringLiteral("192.0.2.41"), 8080,
              QStringLiteral("Connection refused")));
    QVERIFY(rfKit.connectionLineTextForTesting().startsWith(QStringLiteral("Retrying at the Core")));
    QVERIFY2(OperatorWording::isPlain(rfKit.connectionLineTextForTesting()),
             qPrintable(rfKit.connectionLineTextForTesting()));
    {
        // Connected: Disconnect. Retrying: Cancel again.
        std::unique_ptr<QMenu> ampMenu(amp.buildContextMenuForTesting());
        std::unique_ptr<QMenu> rfKitMenu(rfKit.buildContextMenuForTesting());
        checkToggle(ampMenu.get(), QStringLiteral("Disconnect"), true, QString());
        checkToggle(rfKitMenu.get(), QStringLiteral("Cancel"), true, QString());
        connectionToggle(ampMenu.get())->trigger();
    }
    QCOMPARE(link.pgxlDisconnectCalls, 2);

    // A request this computer could not send shows why at once.
    link.pgxlOutcome = {false, QStringLiteral("The station does not support remote PGXL configuration.")};
    link.accessoryOutcome = {false, QStringLiteral("This Core does not offer RF-Kit amplifier setup to this app.")};
    {
        std::unique_ptr<QMenu> ampMenu(amp.buildContextMenuForTesting());
        std::unique_ptr<QMenu> rfKitMenu(rfKit.buildContextMenuForTesting());
        connectionToggle(ampMenu.get())->trigger();
        connectionToggle(rfKitMenu.get())->trigger();
    }
    QCOMPARE(amp.connectionLineTextForTesting(),
             OperatorReasonText::forDisplay(QStringLiteral(
                 "The station does not support remote PGXL configuration.")));
    QCOMPARE(rfKit.connectionLineTextForTesting(),
             QStringLiteral("This Core does not offer RF-Kit amplifier setup to this app."));
    QVERIFY2(OperatorWording::isPlain(amp.connectionLineTextForTesting()),
             qPrintable(amp.connectionLineTextForTesting()));

    // The Core is lost: the stale line speaks and the connection line goes.
    link.linkReady = false;
    model.reportStationLinkStateChanged();
    QVERIFY(amp.staleIndicatorVisibleForTesting());
    QVERIFY(amp.connectionLineTextForTesting().isEmpty());
    QVERIFY(rfKit.connectionLineTextForTesting().isEmpty());

    // Nothing went through this computer's own connections.
    QCOMPARE(ampLocalToggles.count(), 0);
    QCOMPARE(rfKitLocalToggles.count(), 0);
    QVERIFY(!model.pgxlConnection()->isConnected());
    QCOMPARE(model.pgxlConnection()->socketAttemptToken(), quint64(0));
    QVERIFY(!model.rfKitConnection()->isConnected());
    QVERIFY(model.rfKitConnection()->peerAddress().isEmpty());

    // A local window: no connection line, and the toggle is the local one.
    RadioModel local;
    AmpApplet localAmp(&local);
    Rf2ksApplet localRfKit(&local);
    QSignalSpy localAmpToggles(&localAmp, &AmpApplet::connectionToggleRequested);
    QSignalSpy localRfKitToggles(&localRfKit, &Rf2ksApplet::connectionToggleRequested);
    QVERIFY(localAmp.connectionLineTextForTesting().isEmpty());
    QVERIFY(localRfKit.connectionLineTextForTesting().isEmpty());
    {
        std::unique_ptr<QMenu> ampMenu(localAmp.buildContextMenuForTesting());
        std::unique_ptr<QMenu> rfKitMenu(localRfKit.buildContextMenuForTesting());
        checkToggle(ampMenu.get(), QStringLiteral("Connect"), true, QString());
        checkToggle(rfKitMenu.get(), QStringLiteral("Connect"), true, QString());
        connectionToggle(ampMenu.get())->trigger();
        connectionToggle(rfKitMenu.get())->trigger();
    }
    QCOMPARE(localAmpToggles.count(), 1);
    QCOMPARE(localRfKitToggles.count(), 1);
}

// R-R3-47: the remote Power Genius row and tab ask the Core (typed link
// requests) and show the Core's state; no socket of this window's own.
void RemotePeripheralsTest::remotePgxlRowAndTabUseTheStationLink()
{
    RadioModel model(RadioModel::Role::Remote);
    RecordingTgxlLink link;
    model.attachStation(&link);
    FourO3APage page(&model);
    auto* host = page.findChild<QLineEdit*>(QStringLiteral("pgxlHostEdit"));
    auto* port = page.findChild<QSpinBox*>(QStringLiteral("pgxlPortSpin"));
    auto* rowConnect = page.findChild<QPushButton*>(QStringLiteral("pgxlConnectButton"));
    auto* scan = page.findChild<QPushButton*>(QStringLiteral("pgxlScanButton"));
    auto* status = page.findChild<QLabel*>(QStringLiteral("pgxlStatusLabel"));
    auto* peripherals = page.findChild<PeripheralsPage*>();
    auto* tabStatus = page.findChild<QLabel*>(QStringLiteral("remotePgxlStatus"));
    auto* tabConnect = page.findChild<QPushButton*>(QStringLiteral("remotePgxlConnectButton"));
    auto* operate = page.findChild<QPushButton*>(QStringLiteral("remotePgxlOperateButton"));
    auto* apply = page.findChild<QPushButton*>(QStringLiteral("remotePgxlApplySettings"));
    auto* keepalive = page.findChild<QSpinBox*>(QStringLiteral("remotePgxlKeepalive"));
    auto* ping = page.findChild<QSpinBox*>(QStringLiteral("remotePgxlPing"));
    auto* autoReconnect = page.findChild<QCheckBox*>(QStringLiteral("remotePgxlAutoReconnect"));
    auto* tabs = page.findChild<QTabWidget*>();
    QVERIFY(host && port && rowConnect && scan && status && peripherals && tabStatus
            && tabConnect && operate && apply && keepalive && ping && autoReconnect && tabs);
    QSignalSpy pgxlFrames(model.pgxlConnection(), &PgxlConnection::testFrameWrittenForTesting);

    // An older Core: nothing to press, and it says why in user words.
    QVERIFY(!rowConnect->isEnabled());
    QCOMPARE(status->text(),
             QStringLiteral("This Core does not offer Power Genius XL control to this app."));
    QVERIFY(!tabs->isTabEnabled(1));
    QVERIFY(OperatorWording::isPlain(status->text()));

    link.pgxlAvailable = true;
    model.reportStationLinkStateChanged();
    QVERIFY(rowConnect->isEnabled());
    QVERIFY(!scan->isEnabled());
    QVERIFY(tabs->isTabEnabled(1));
    QVERIFY(!operate->isEnabled());
    QCOMPARE(operate->toolTip(), OperatorReasonText::forDisplay(
                                     AmplifierModel::receiveOnlyOperateReason()));
    QVERIFY(OperatorWording::isPlain(operate->toolTip()));

    // Connect sends the draft to the Core; nothing local.
    host->setText(QStringLiteral("amp.station.example"));
    port->setValue(9018);
    QVERIFY(QMetaObject::invokeMethod(peripherals, "onConnect", Qt::DirectConnection,
                                      Q_ARG(int, 1)));
    QCOMPARE(link.pgxlConfigureCalls, 1);
    QCOMPARE(link.pgxlHost, QStringLiteral("amp.station.example"));
    QCOMPARE(link.pgxlPort, quint16{9018});
    QCOMPARE(link.configureCalls, 0);  // not the tuner's command

    // The Core's state arrives; an identifying amp can be cancelled.
    TunerModel::StationConnectionState identifying =
        state(TunerModel::ConnectionPhase::Identifying, QStringLiteral("amp.station.example"), 9018);
    model.amplifierModel()->setStationConnectionState(identifying);
    QCOMPARE(rowConnect->text(), QStringLiteral("Cancel"));
    QCOMPARE(status->text(), QStringLiteral("Identifying device"));
    QCOMPARE(tabConnect->text(), QStringLiteral("Cancel"));
    QVERIFY(QMetaObject::invokeMethod(peripherals, "onConnect", Qt::DirectConnection,
                                      Q_ARG(int, 1)));
    QCOMPARE(link.pgxlDisconnectCalls, 1);

    // Connected, with the Core's identity.
    TunerModel::StationConnectionState up =
        state(TunerModel::ConnectionPhase::Connected, QStringLiteral("amp.station.example"), 9018);
    up.deviceModel = QStringLiteral("PowerGeniusXL");
    up.deviceSerial = QStringLiteral("10-200/24-0046");
    up.deviceVersion = QStringLiteral("3.8.9");
    model.amplifierModel()->setStationConnectionState(up);
    QCOMPARE(rowConnect->text(), QStringLiteral("Disconnect"));
    QCOMPARE(status->text(), QStringLiteral("Connected: PowerGeniusXL 10-200/24-0046"));
    auto* identity = page.findChild<QLabel*>(QStringLiteral("remotePgxlIdentity"));
    QVERIFY(identity->text().contains(QStringLiteral("10-200/24-0046")));
    QVERIFY(QMetaObject::invokeMethod(&page, "onRemotePgxlConnectClicked", Qt::DirectConnection));
    QCOMPARE(link.pgxlDisconnectCalls, 2);

    // A refusal in the Core's words reaches the row in user words.
    TunerModel::StationConnectionState wrong =
        state(TunerModel::ConnectionPhase::Error, QStringLiteral("amp.station.example"), 9018,
              QStringLiteral("Expected PowerGeniusXL at the connected endpoint; observed "
                             "TunerGenius (serial 241288-1)."));
    model.amplifierModel()->setStationConnectionState(wrong);
    QVERIFY2(status->text().contains(QStringLiteral(
                 "The device at this address is not a Power Genius.")), qPrintable(status->text()));
    QVERIFY2(OperatorWording::isPlain(status->text()), qPrintable(status->text()));
    QVERIFY2(OperatorWording::isPlain(tabStatus->text()), qPrintable(tabStatus->text()));
    for (const QString& raw :
         {QStringLiteral("No matching PGXL discovery announcement for 192.0.2.40:9008. Check the "
                         "amplifier address, port and station LAN discovery."),
          QStringLiteral("PGXL identity serial mismatch: expected 1, observed 2"),
          QStringLiteral("PGXL native identity timed out"),
          QStringLiteral("PGXL native info omitted a nonempty serial"),
          QStringLiteral("PGXL native info failed with code 3"),
          QStringLiteral("PGXL discovery approval timed out for serial 1"),
          QStringLiteral("The station does not support remote PGXL configuration.")}) {
        const QString shown = OperatorReasonText::forDisplay(raw);
        QVERIFY2(shown != raw || !raw.contains(QStringLiteral("PGXL")), qPrintable(raw));
        QVERIFY2(OperatorWording::isPlain(shown), qPrintable(shown));
    }

    // Connection settings go to the Core as one request.
    autoReconnect->setChecked(false);
    keepalive->setValue(45);
    ping->setValue(0);
    QVERIFY(QMetaObject::invokeMethod(&page, "onRemotePgxlApplySettingsClicked",
                                      Qt::DirectConnection));
    QCOMPARE(link.pgxlSettingsCalls, 1);
    QVERIFY(!link.pgxlAutoReconnect);
    QCOMPARE(link.pgxlKeepaliveSec, 45);
    QCOMPARE(link.pgxlPingSec, 0);

    QCOMPARE(pgxlFrames.count(), 0);
    QVERIFY(!model.pgxlConnection()->isConnected());
    QCOMPARE(model.pgxlConnection()->socketAttemptToken(), quint64(0));
}

// R-R3-47 / R-R3-22: end to end over the in-process loopback. The window
// asks; the Core dials the amp (a loopback stand-in), identifies it, pairs
// it; the window follows the Core's `amplifier` object and opens nothing.
void RemotePeripheralsTest::remoteWindowSetsUpThePgxlThroughTheCore()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings::instance().setValue(QStringLiteral("PeripheralsMigrationDone"),
                                     QStringLiteral("True"));
    QTcpServer amp;
    QVERIFY(amp.listen(QHostAddress::LocalHost, 0));
    RadioModel station;
    station.enableStationAccessoryIdentity();
    RadioInfo radio;
    radio.macAddress = QStringLiteral("aa:bb:cc:dd:ee:73");
    station.setLastRadioInfoForTest(radio);
    station.setConnectionStateForTest(ConnectionState::Connected);
    station.smartSdrListener()->setListenEndpointForTesting(QHostAddress::LocalHost, 0);
    station.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("True"));
    AppSettings stationSettings(dir.filePath(QStringLiteral("station.settings")));
    StationServer server(&station, stationSettings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
    QSignalSpy stationFrames(station.pgxlConnection(), &PgxlConnection::testFrameWrittenForTesting);

    RadioModel window(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&window, &proxy);
    window.attachStation(&client);
    PeripheralsPage page(&window);
    auto* host = page.findChild<QLineEdit*>(QStringLiteral("pgxlHostEdit"));
    auto* port = page.findChild<QSpinBox*>(QStringLiteral("pgxlPortSpin"));
    auto* connectButton = page.findChild<QPushButton*>(QStringLiteral("pgxlConnectButton"));
    auto* status = page.findChild<QLabel*>(QStringLiteral("pgxlStatusLabel"));
    QVERIFY(host && port && connectButton && status);
    AmpApplet applet(&window);
    QSignalSpy windowFrames(window.pgxlConnection(), &PgxlConnection::testFrameWrittenForTesting);

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    QVERIFY(completed.wait(5000) || !completed.isEmpty());
    NEREUS_TRY_VERIFY(client.remotePgxlControlAvailable());
    window.reportStationLinkStateChanged();
    NEREUS_TRY_VERIFY(connectButton->isEnabled());

    // Connect: the Core dials, and the window follows its phase.
    host->setText(QStringLiteral("127.0.0.1"));
    port->setValue(amp.serverPort());
    QVERIFY(QMetaObject::invokeMethod(&page, "onConnect", Qt::DirectConnection, Q_ARG(int, 1)));
    NEREUS_TRY_VERIFY(amp.hasPendingConnections());
    QTcpSocket* peer = amp.nextPendingConnection();
    peer->write("V3.8.9\n");
    peer->flush();
    NEREUS_TRY_COMPARE(window.amplifierModel()->connectionPhase(),
                 AmplifierModel::ConnectionPhase::Identifying);
    QCOMPARE(status->text(), QStringLiteral("Identifying device"));
    quint32 infoSeq = 0;
    NEREUS_TRY_VERIFY([&] {
        for (const auto& row : stationFrames) {
            const QString frame = row.first().toString();
            if (frame.endsWith(QStringLiteral("|info"))) {
                infoSeq = frame.mid(1, frame.indexOf(QLatin1Char('|')) - 1).toUInt();
            }
        }
        return infoSeq != 0;
    }());
    peer->write(QStringLiteral("R%1|0|serial=10-200/24-0046  version=3.8.9 protocol=1.0 mains=240\n")
                    .arg(infoSeq).toUtf8());
    peer->flush();
    auto* controller = station.findChild<StationPgxlController*>();
    QVERIFY(controller);
    NEREUS_TRY_VERIFY(controller->findChild<LanDiscovery*>());
    controller->findChild<LanDiscovery*>()->injectDatagramForTesting(
        QStringLiteral("PowerGeniusXL ip=127.0.0.1 v=3.8.9 serial=10-200/24-0046 nickname=PowerGeniusXL"),
        amp.serverPort());
    NEREUS_TRY_COMPARE(window.amplifierModel()->connectionPhase(),
                 AmplifierModel::ConnectionPhase::Connected);
    NEREUS_TRY_COMPARE(status->text(), QStringLiteral("Connected: PowerGeniusXL 10-200/24-0046"));
    QCOMPARE(window.amplifierModel()->configuredPort(), int(amp.serverPort()));
    QCOMPARE(connectButton->text(), QStringLiteral("Disconnect"));

    // R-R3-22: the applet's Disconnect and Reconnect ask the Core too.
    QCOMPARE(applet.connectionLineTextForTesting(), QStringLiteral("Connected"));
    {
        std::unique_ptr<QMenu> menu(applet.buildContextMenuForTesting());
        QAction* toggle = connectionToggle(menu.get());
        QVERIFY(toggle && toggle->isEnabled());
        QCOMPARE(toggle->text(), QStringLiteral("Disconnect"));
        toggle->trigger();
    }
    NEREUS_TRY_COMPARE(window.amplifierModel()->connectionPhase(),
                 AmplifierModel::ConnectionPhase::Disconnected);
    NEREUS_TRY_VERIFY(!station.pgxlConnection()->isConnected());
    QCOMPARE(applet.connectionLineTextForTesting(), QStringLiteral("Disconnected"));
    {
        std::unique_ptr<QMenu> menu(applet.buildContextMenuForTesting());
        QAction* toggle = connectionToggle(menu.get());
        QVERIFY(toggle && toggle->isEnabled());
        QCOMPARE(toggle->text(), QStringLiteral("Connect"));
        toggle->trigger();
    }
    NEREUS_TRY_VERIFY(amp.hasPendingConnections());   // the Core dials its saved address
    QTcpSocket* again = amp.nextPendingConnection();
    again->write("V3.8.9\n");
    again->flush();
    NEREUS_TRY_COMPARE(window.amplifierModel()->connectionPhase(),
                 AmplifierModel::ConnectionPhase::Identifying);
    QCOMPARE(applet.connectionLineTextForTesting(), QStringLiteral("Identifying device"));

    // Configure: the settings command reaches the Core and applies there.
    const auto settingsOutcome = client.requestPgxlConnectionSettings(true, 40, 0);
    QVERIFY(settingsOutcome.sent);
    NEREUS_TRY_COMPARE(AppSettings::instance().value(QStringLiteral("PGXL_KeepaliveSec")).toString(),
                 QStringLiteral("40"));

    // Disconnect (here, cancelling the second attempt): the Core closes
    // it; the window shows it.
    QVERIFY(QMetaObject::invokeMethod(&page, "onConnect", Qt::DirectConnection, Q_ARG(int, 1)));
    NEREUS_TRY_COMPARE(window.amplifierModel()->connectionPhase(),
                 AmplifierModel::ConnectionPhase::Disconnected);
    QVERIFY(!station.pgxlConnection()->isConnected());
    QCOMPARE(status->text(), QStringLiteral("Disconnected"));
    QCOMPARE(applet.connectionLineTextForTesting(), QStringLiteral("Disconnected"));

    // The window opened no connection of its own.
    QCOMPARE(windowFrames.count(), 0);
    QCOMPARE(window.pgxlConnection()->socketAttemptToken(), quint64(0));
    stationEnd->closeLink(QStringLiteral("test done"));
    AppSettings::instance().clear();
}

// R-R3-25: a receive-only Core refuses a window's write to the tuner's
// operate, bypass or antenna and to the amp's operate, with one plain
// reason; nothing changes on the Core and nothing reaches the tuner.
void RemotePeripheralsTest::receiveOnlyCoreRefusesTunerAndAmpOperation()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RadioModel station;
    station.setReceiveOnlyStationPolicy(true);
    // The tuner is connected on the Core (offline parser seam): a write that
    // got through would send `operate=`, `bypass=` or `activate ant=`.
    station.tgxlConnection()->injectLineForTesting(QStringLiteral("V1.2.17"));
    QVERIFY(station.tgxlConnection()->isConnected());
    station.tgxlConnection()->injectLineForTesting(
        QStringLiteral("S0|state operate=0 bypass=0 antA=1 one_by_three=1"));
    QSignalSpy tunerFrames(station.tgxlConnection(), &TgxlConnection::testFrameWrittenForTesting);
    QSignalSpy ampFrames(station.pgxlConnection(), &PgxlConnection::testFrameWrittenForTesting);
    const bool operateBefore = station.tunerModel()->isOperate();
    const bool bypassBefore = station.tunerModel()->isBypass();
    const int antennaBefore = station.tunerModel()->antennaA();
    AppSettings stationSettings(dir.filePath(QStringLiteral("station.settings")));
    StationServer server(&station, stationSettings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));

    auto* core = new LoopbackTransport(QStringLiteral("core"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("raw-gui"), this);
    core->linkTo(peer);
    server.acceptTransport(core);
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("guard-test"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    const auto messages = [peer] {
        QList<SessionMessage> list;
        for (const QByteArray& wire : peer->received()) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)) { list.append(message); }
        }
        return list;
    };
    NEREUS_TRY_VERIFY([&] {
        for (const SessionMessage& m : messages()) {
            if (m.kind == SessionMessageKind::SnapshotComplete) { return true; }
        }
        return false;
    }());

    peer->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
        "tuner", {MirrorUpdate{0, "isOperate", MirrorWireKind::Bool, QVariant(!operateBefore)}}, 11)));
    peer->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
        "tuner", {MirrorUpdate{0, "isBypass", MirrorWireKind::Bool, QVariant(!bypassBefore)}}, 12)));
    peer->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
        "tuner", {MirrorUpdate{0, "antennaA", MirrorWireKind::Int64, QVariant(qint64(3))}}, 13)));
    peer->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
        "amplifier", {MirrorUpdate{0, "operate", MirrorWireKind::Bool, QVariant(true)}}, 14)));
    QList<SessionPropertyResult> results;
    NEREUS_TRY_VERIFY([&] {
        results.clear();
        for (const SessionMessage& m : messages()) {
            if (m.kind == SessionMessageKind::PropertyResult) {
                results.append(m.propertyResults);
            }
        }
        return results.size() >= 3;
    }());
    // The amp object is offered only on a Core that owns its accessories;
    // this Core does not, so only the three tuner results are certain.
    NereusSDR::Test::settleSession();
    int tunerRefusals = 0;
    for (const SessionPropertyResult& result : results) {
        QVERIFY(!result.accepted);
        if (result.property != "operate") {
            QCOMPARE(result.reason, AmplifierModel::receiveOnlyOperateReason());
            ++tunerRefusals;
        }
    }
    QCOMPARE(tunerRefusals, 3);
    QVERIFY(OperatorWording::isPlain(AmplifierModel::receiveOnlyOperateReason()));
    // R-R3-49: the Core's own 1 Hz status poll (TgxlConnection::pollStatus)
    // is not the window reaching the tuner. On a busy computer this case
    // ran past its first second and the poll failed it; it is waited for
    // here, so every run has one, and every frame but that poll counts.
    const auto framesButThePoll = [](const QSignalSpy& frames) {
        int count = 0;
        for (const QList<QVariant>& frame : frames) {
            if (!frame.at(0).toString().endsWith(QLatin1String("|status"))) {
                ++count;
            }
        }
        return count;
    };
    NEREUS_TRY_VERIFY_WITH_TIMEOUT(framesButThePoll(tunerFrames) < tunerFrames.count(), 5000);
    QCOMPARE(framesButThePoll(tunerFrames), 0);
    QCOMPARE(framesButThePoll(ampFrames), 0);
    QCOMPARE(station.tunerModel()->isOperate(), operateBefore);
    QCOMPARE(station.tunerModel()->isBypass(), bypassBefore);
    QCOMPARE(station.tunerModel()->antennaA(), antennaBefore);
    core->closeLink(QStringLiteral("test done"));
}

// R-R3-47 / R-R3-48: the RF-Kit page and applet in a remote window ask the
// Core, which switches, identifies, connects and disconnects its amp; the
// window sees the rows and the band-follow line, and dials nothing.
void RemotePeripheralsTest::remoteWindowSetsUpTheRfKitThroughTheCore()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings::instance().setValue(QStringLiteral("PeripheralsMigrationDone"),
                                     QStringLiteral("True"));
    AppSettings::instance().setValue(QStringLiteral("RfKit_PollIntervalMs"), QStringLiteral("5000"));
    FakeRfKit amp;
    RadioModel station;
    station.enableStationAccessoryIdentity();
    station.setReceiveOnlyStationPolicy(true);
    RadioInfo radio;
    radio.macAddress = QStringLiteral("aa:bb:cc:dd:ee:83");
    station.setLastRadioInfoForTest(radio);
    station.setConnectionStateForTest(ConnectionState::Connected);
    AppSettings stationSettings(dir.filePath(QStringLiteral("station.settings")));
    StationServer server(&station, stationSettings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));

    RadioModel window(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&window, &proxy);
    window.attachStation(&client);
    RfKitPage page(&window);
    Rf2ksApplet applet(&window);
    QCheckBox* master = page.masterCheckboxForTesting();
    QVERIFY(master);
    QVERIFY(!master->isEnabled());   // no Core yet

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    QVERIFY(completed.wait(5000) || !completed.isEmpty());
    NEREUS_TRY_VERIFY(client.remoteRfKitControlAvailable());
    window.reportStationLinkStateChanged();
    NEREUS_TRY_VERIFY(master->isEnabled());
    QVERIFY(!window.rfKitEnabled());
    QVERIFY(!page.detailTabIsEnabledForTesting());

    // Switch it on at the Core.
    master->setChecked(true);
    NEREUS_TRY_VERIFY(station.rfKitEnabled());
    NEREUS_TRY_VERIFY(window.rfKitEnabled());
    QVERIFY(master->isChecked());
    NEREUS_TRY_VERIFY(page.detailTabIsEnabledForTesting());

    // Connect: the Core identifies the amp and admits it.
    page.setHostForTesting(QStringLiteral("127.0.0.1"));
    page.setPortForTesting(amp.serverPort());
    page.testConnectionButtonForTesting()->click();
    NEREUS_TRY_COMPARE_WITH_TIMEOUT(window.rfKitModel()->connectionPhase(),
                              RfKitModel::ConnectionPhase::Connected, 5000);
    QCOMPARE(window.rfKitModel()->deviceModel(), QStringLiteral("RF2K-S"));
    QCOMPARE(window.rfKitModel()->configuredPort(), int(amp.serverPort()));
    QCOMPARE(station.peripheralValue(QStringLiteral("RfKit_ManualIp")), QStringLiteral("127.0.0.1"));
    NEREUS_TRY_VERIFY(page.liveStatusTextForTesting().contains(QStringLiteral("Connected")));
    QVERIFY(OperatorWording::isPlain(page.liveStatusTextForTesting()));
    NEREUS_TRY_VERIFY(applet.connectedStateForTesting());

    // The rows the Core's amp reports reach the window's applet.
    station.rfKitConnection()->injectJsonForTesting(QStringLiteral("/power"), kRfKitPower);
    station.rfKitConnection()->injectJsonForTesting(QStringLiteral("/tuner"),
        R"({"mode":"AUTO","setup":"LC","L":{"value":1200,"unit":"nH"},"C":{"value":345,"unit":"pF"},"tuned_frequency":{"value":3891,"unit":"kHz"},"segment_size":{"value":9,"unit":"kHz"}})");
    station.rfKitConnection()->injectJsonForTesting(QStringLiteral("/antennas"),
        R"({"antennas":[{"type":"INTERNAL","number":1,"state":"AVAILABLE"},{"type":"INTERNAL","number":2,"state":"ACTIVE"},{"type":"INTERNAL","number":3,"state":"AVAILABLE"},{"type":"INTERNAL","number":4,"state":"AVAILABLE"}]})");
    station.rfKitConnection()->injectJsonForTesting(QStringLiteral("/antennas/active"),
                                                   R"({"type":"INTERNAL","number":2})");
    NEREUS_TRY_COMPARE(applet.tunerStatusTextForTesting(), QStringLiteral("TUNED 3.891 MHz (LC)"));
    NEREUS_TRY_VERIFY(applet.antennaButtonIsActiveForTesting(2));
    QVERIFY(!applet.antennaButtonIsActiveForTesting(1));
    // R-R3-49 (parity Task 10): off the air the Core switches its amp's
    // antenna for this window (they waited for remote transmit before).
    QVERIFY(applet.antennaButtonIsEnabledForTesting(2));

    // Band follow: the Core runs no station TCI server here, so it is off.
    QCOMPARE(applet.bandFollowTextForTesting(), window.rfKitModel()->bandFollowText());
    QCOMPARE(page.bandFollowTextForTesting(), window.rfKitModel()->bandFollowText());
    QVERIFY(page.bandFollowTextForTesting().startsWith(QStringLiteral("Band follow: off")));

    // R-R3-22: the applet's Disconnect and Reconnect ask the Core.
    QCOMPARE(applet.connectionLineTextForTesting(), QStringLiteral("Connected"));
    {
        std::unique_ptr<QMenu> menu(applet.buildContextMenuForTesting());
        QAction* toggle = connectionToggle(menu.get());
        QVERIFY(toggle && toggle->isEnabled());
        QCOMPARE(toggle->text(), QStringLiteral("Disconnect"));
        toggle->trigger();
    }
    NEREUS_TRY_COMPARE(window.rfKitModel()->connectionPhase(),
                 RfKitModel::ConnectionPhase::Disconnected);
    NEREUS_TRY_VERIFY(!station.rfKitConnection()->isConnected());
    QCOMPARE(applet.connectionLineTextForTesting(), QStringLiteral("Disconnected"));
    {
        std::unique_ptr<QMenu> menu(applet.buildContextMenuForTesting());
        QAction* toggle = connectionToggle(menu.get());
        QVERIFY(toggle && toggle->isEnabled());
        QCOMPARE(toggle->text(), QStringLiteral("Connect"));
        toggle->trigger();
    }
    NEREUS_TRY_COMPARE_WITH_TIMEOUT(window.rfKitModel()->connectionPhase(),
                              RfKitModel::ConnectionPhase::Connected, 5000);
    NEREUS_TRY_COMPARE(applet.connectionLineTextForTesting(), QStringLiteral("Connected"));

    // Disconnect from the page.
    page.disconnectButtonForTesting()->click();
    NEREUS_TRY_COMPARE(window.rfKitModel()->connectionPhase(),
                 RfKitModel::ConnectionPhase::Disconnected);
    QVERIFY(!station.rfKitConnection()->isConnected());

    // Switch it off.
    master->setChecked(false);
    NEREUS_TRY_VERIFY(!station.rfKitEnabled());
    NEREUS_TRY_VERIFY(!window.rfKitEnabled());
    NEREUS_TRY_COMPARE(window.rfKitModel()->connectionPhase(), RfKitModel::ConnectionPhase::Disabled);

    // R-R3-22: the Core refuses the applet's Reconnect while its switch is
    // off; the applet shows the reason in plain words.
    {
        std::unique_ptr<QMenu> menu(applet.buildContextMenuForTesting());
        QAction* toggle = connectionToggle(menu.get());
        QVERIFY(toggle && toggle->isEnabled());
        QCOMPARE(toggle->text(), QStringLiteral("Connect"));
        toggle->trigger();
    }
    NEREUS_TRY_COMPARE(applet.connectionLineTextForTesting(),
                 OperatorReasonText::forDisplay(QStringLiteral(
                     "Turn on the RF-Kit amplifier on the Core before connecting it.")));
    QVERIFY2(OperatorWording::isPlain(applet.connectionLineTextForTesting()),
             qPrintable(applet.connectionLineTextForTesting()));
    QVERIFY(!station.rfKitConnection()->isConnected());

    // The window opened no connection of its own.
    QVERIFY(!window.rfKitConnection()->isConnected());
    QCOMPARE(window.rfKitConnection()->pollsSucceeded() + window.rfKitConnection()->pollsFailed(), 0);
    QVERIFY(window.rfKitConnection()->peerAddress().isEmpty());
    stationEnd->closeLink(QStringLiteral("test done"));
    AppSettings::instance().clear();
}

// R-R3-47: an app that writes the RF-Kit switch as a raw value (every app
// before this one) is refused in plain words; the Core's switch stays.
void RemotePeripheralsTest::rawRfKitSwitchWriteIsRefused()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings::instance().setValue(QStringLiteral("PeripheralsMigrationDone"),
                                     QStringLiteral("True"));
    RadioModel station;
    station.enableStationAccessoryIdentity();
    RadioInfo radio;
    radio.macAddress = QStringLiteral("aa:bb:cc:dd:ee:84");
    station.setLastRadioInfoForTest(radio);
    station.setConnectionStateForTest(ConnectionState::Connected);
    QVERIFY(!station.rfKitEnabled());
    AppSettings stationSettings(dir.filePath(QStringLiteral("station.settings")));
    StationServer server(&station, stationSettings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));

    auto* core = new LoopbackTransport(QStringLiteral("core"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("raw-gui"), this);
    core->linkTo(peer);
    server.acceptTransport(core);
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("switch-test"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    const auto messages = [peer] {
        QList<SessionMessage> list;
        for (const QByteArray& wire : peer->received()) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)) { list.append(message); }
        }
        return list;
    };
    NEREUS_TRY_VERIFY([&] {
        for (const SessionMessage& m : messages()) {
            if (m.kind == SessionMessageKind::SnapshotComplete) { return true; }
        }
        return false;
    }());
    peer->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
        "radio", {MirrorUpdate{0, "rfKitEnabled", MirrorWireKind::Bool, QVariant(true)}}, 21)));
    SessionPropertyResult result;
    NEREUS_TRY_VERIFY([&] {
        for (const SessionMessage& m : messages()) {
            if (m.kind == SessionMessageKind::PropertyResult && !m.propertyResults.isEmpty()) {
                result = m.propertyResults.first();
                return true;
            }
        }
        return false;
    }());
    QVERIFY(!result.accepted);
    QCOMPARE(result.reason,
             QStringLiteral("Update this app to turn the RF-Kit amplifier on or off on this Core."));
    QVERIFY(OperatorWording::isPlain(result.reason));
    QVERIFY(!station.rfKitEnabled());

    // R-R3-49: the Core's transmit state is its own report; a raw write
    // never keys and is refused like any other Core reading.
    peer->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
        "radio", {MirrorUpdate{0, "transmitting", MirrorWireKind::Bool, QVariant(true)}}, 22)));
    SessionPropertyResult transmitResult;
    NEREUS_TRY_VERIFY([&] {
        for (const SessionMessage& m : messages()) {
            if (m.kind == SessionMessageKind::PropertyResult && m.writeId == 22
                && !m.propertyResults.isEmpty()) {
                transmitResult = m.propertyResults.first();
                return true;
            }
        }
        return false;
    }());
    QVERIFY(!transmitResult.accepted);
    QCOMPARE(transmitResult.reason,
             QStringLiteral("The Core sets this itself; it cannot be changed from here."));
    QVERIFY(!station.isTransmitting());
    QVERIFY(!station.moxController()->isMox());

    // The command is what changes it.
    peer->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
        "setRfKitEnabled", 31, {MirrorUpdate{0, "enabled", MirrorWireKind::Bool, QVariant(true)}})));
    NEREUS_TRY_VERIFY(station.rfKitEnabled());
    core->closeLink(QStringLiteral("test done"));
    AppSettings::instance().clear();
}

// R-R3-48: the Power Genius's band-follow line on the applet and the 4O3A
// page follows its pairing, in a local window and from the Core.
void RemotePeripheralsTest::pgxlBandFollowLineLocalAndRemote()
{
    // The amp's own reports drive the state: connected, paired, dropped.
    // (A bare connection and model, so no pairing request is sent.)
    {
        PgxlConnection conn;
        AmplifierModel amp;
        amp.bindConnection(&conn);
        QCOMPARE(amp.bandFollow(), TunerModel::BandFollow::Off);
        emit conn.connected();
        QCOMPARE(amp.bandFollow(), TunerModel::BandFollow::Waiting);
        emit conn.pairingResult(false, QStringLiteral("R12|1|"));
        QCOMPARE(amp.bandFollow(), TunerModel::BandFollow::Waiting);
        emit conn.pairingResult(true, QString());
        QCOMPARE(amp.bandFollow(), TunerModel::BandFollow::Following);
        emit conn.disconnected();
        QCOMPARE(amp.bandFollow(), TunerModel::BandFollow::Off);
    }

    // A local window's applet and 4O3A page show the line.
    RadioModel local;
    AmpApplet localApplet(&local);
    FourO3APage localPage(&local);
    auto* localLine = localPage.findChild<QLabel*>(QStringLiteral("pgxlBandFollowLabel"));
    QVERIFY(localLine);
    QCOMPARE(localApplet.bandFollowTextForTesting(),
             QStringLiteral("Band follow: off while the Power Genius is not connected."));
    local.amplifierModel()->setBandFollow(TunerModel::BandFollow::Waiting);
    QCOMPARE(localApplet.bandFollowTextForTesting(),
             QStringLiteral("Band follow: waiting for the Power Genius to pair with the radio."));
    local.amplifierModel()->setBandFollow(TunerModel::BandFollow::Following);
    QCOMPARE(localApplet.bandFollowTextForTesting(),
             QStringLiteral("Band follow: following the radio"));
    QCOMPARE(localLine->text(), QStringLiteral("Band follow: following the radio"));
    for (const QString& text : {localApplet.bandFollowTextForTesting(), localLine->text()}) {
        QVERIFY(OperatorWording::isPlain(text));
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RadioModel station;
    station.enableStationAccessoryIdentity();
    AppSettings stationSettings(dir.filePath(QStringLiteral("station.settings")));
    StationServer server(&station, stationSettings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
    RadioModel window(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&window, &proxy);
    window.attachStation(&client);
    AmpApplet remoteApplet(&window);
    FourO3APage remotePage(&window);
    auto* remoteLine = remotePage.findChild<QLabel*>(QStringLiteral("pgxlBandFollowLabel"));
    QVERIFY(remoteLine);
    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    QVERIFY(completed.wait(5000) || !completed.isEmpty());
    station.amplifierModel()->setBandFollow(TunerModel::BandFollow::Following);
    NEREUS_TRY_COMPARE(window.amplifierModel()->bandFollow(), TunerModel::BandFollow::Following);
    QCOMPARE(remoteApplet.bandFollowTextForTesting(),
             QStringLiteral("Band follow: following the radio"));
    QCOMPARE(remoteLine->text(), QStringLiteral("Band follow: following the radio"));
    station.amplifierModel()->setBandFollow(TunerModel::BandFollow::Waiting);
    NEREUS_TRY_COMPARE(remoteApplet.bandFollowTextForTesting(),
                 QStringLiteral("Band follow: waiting for the Power Genius to pair with the radio."));
    stationEnd->closeLink(QStringLiteral("test done"));
}

// R-R3-48: the app's one TCI switch turns the Core's station server on and
// off. The Core keeps it when the window goes and when another connects.
// On the same computer as the Core, the window runs no server of its own
// and apps here reach the Core's.
void RemotePeripheralsTest::oneTciSwitchDrivesTheCoresStationServer()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const quint16 port = freeLoopbackPort();
    RadioModel station;
    station.enableStationTci(QStringLiteral("127.0.0.1"));
    AppSettings stationSettings(dir.filePath(QStringLiteral("station.settings")));
    // The Core's own settings store: where its station TCI switch is kept
    // (StationTciController) and what the window's settings snapshot reads.
    StationServer server(&station, AppSettings::instance(), NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
    // Version 2 includes the Core's TCI client record stream and options.
    QCOMPARE(server.stationTciVersion(), 2);

    auto window = std::make_unique<RadioModel>(RadioModel::Role::Remote);
    SettingsProxy proxy;
    auto client = std::make_unique<StationClient>(window.get(), &proxy);
    window->attachStation(client.get());
    client->setCoreOnThisComputerForTest(true);
    auto local = std::make_unique<TciServer>(window.get());
    auto tci = std::make_unique<TciSwitch>(local.get(), window.get());
    CatTciServerPage page;
    page.setRadioModel(window.get());
    QVERIFY(page.stationLineForTesting().isEmpty());

    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);
    QSignalSpy completed(client.get(), &StationClient::handshakeComplete);
    client->startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    QVERIFY(completed.wait(5000) || !completed.isEmpty());
    NEREUS_TRY_VERIFY(client->stationTciAvailable());
    QCOMPARE(client->capabilities().stationTciVersion, 2);
    QVERIFY(client->stationTciServerAvailable());
    window->reportStationLinkStateChanged();

    // On: the Core listens on this port; this window runs none of its own.
    tci->setSwitch(true, port, QHostAddress(QHostAddress::LocalHost));
    NEREUS_TRY_VERIFY(station.stationTciModel()->listening());
    QCOMPARE(station.stationTciModel()->port(), int(port));
    QVERIFY(!local->isRunning());
    NEREUS_TRY_VERIFY(window->stationTciModel()->listening());
    QCOMPARE(page.stationLineForTesting(),
             QStringLiteral("The Core on this computer serves TCI apps here, port %1.").arg(port));
    QVERIFY(OperatorWording::isPlain(page.stationLineForTesting()));
    {
        QWebSocket app;
        QStringList frames;
        connect(&app, &QWebSocket::textMessageReceived, &app,
                [&frames](const QString& text) { frames.append(text); });
        app.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port)));
        NEREUS_TRY_VERIFY(frames.join(QString()).contains(QStringLiteral("receive_only:true;")));
        app.close();
    }

    // The window goes: the Core keeps its switch.
    stationEnd->closeLink(QStringLiteral("window closed"));
    tci.reset();
    local.reset();
    client.reset();
    window.reset();
    NereusSDR::Test::settleSession();
    QVERIFY(station.stationTciModel()->enabled());
    QVERIFY(station.stationTciModel()->listening());

    // Another app connects: the Core's switch is unchanged, and that app
    // sees it; its own switch turns both off.
    RadioModel second(RadioModel::Role::Remote);
    SettingsProxy secondProxy;
    StationClient secondClient(&second, &secondProxy);
    second.attachStation(&secondClient);
    TciServer secondLocal(&second);
    TciSwitch secondSwitch(&secondLocal, &second);
    auto* stationEnd2 = new LoopbackTransport(QStringLiteral("station-end-2"), this);
    auto* clientEnd2 = new LoopbackTransport(QStringLiteral("client-end-2"), this);
    stationEnd2->linkTo(clientEnd2);
    QSignalSpy completed2(&secondClient, &StationClient::handshakeComplete);
    secondClient.startSession(clientEnd2, server.token());
    server.acceptTransport(stationEnd2);
    QVERIFY(completed2.wait(5000) || !completed2.isEmpty());
    NEREUS_TRY_VERIFY(second.stationTciModel()->listening());
    QVERIFY(station.stationTciModel()->listening());
    NEREUS_TRY_VERIFY(secondClient.stationTciAvailable());

    // Rework part 1: the second window's switch shows the Core's.
    NEREUS_TRY_VERIFY(secondSwitch.switchOn());
    QCOMPARE(secondSwitch.port(), port);
    secondSwitch.setSwitch(false, port, QHostAddress(QHostAddress::LocalHost));
    NEREUS_TRY_VERIFY(!station.stationTciModel()->enabled());
    QVERIFY(!station.stationTciModel()->listening());
    QVERIFY(!secondLocal.isRunning());
    NEREUS_TRY_VERIFY(!second.stationTciModel()->listening());
    stationEnd2->closeLink(QStringLiteral("test done"));
    AppSettings::instance().clear();
}

// Rework part 1 (R-R3-48, one switch and one port): a window on the
// Core's computer runs no TCI server of its own while connected. The phone
// (or another window) turns the Core's station switch on: the Core serves
// apps on this computer and on the station network (a real address of
// this computer stands in for the station network when there is one), and
// this window's switch shows the Core's.
void RemotePeripheralsTest::coreHereServesThisComputersApps()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const quint16 port = freeLoopbackPort();
    QString stationAddress;
    for (const QHostAddress& address : QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
            stationAddress = address.toString();
            break;
        }
    }
    // The Core keeps its station switch (off), so it wins at connect.
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("StationTci_Enabled"), QStringLiteral("False"));
    AppSettings::instance().setValue(QStringLiteral("StationTci_Port"), QString::number(port));
    RadioModel station;
    station.enableStationTci(stationAddress.isEmpty() ? QStringLiteral("127.0.0.1")
                                                      : stationAddress);
    AppSettings stationSettings(dir.filePath(QStringLiteral("station.settings")));
    // The Core's own settings store: where its station TCI switch is kept
    // (StationTciController) and what the window's settings snapshot reads.
    StationServer server(&station, AppSettings::instance(), NereusSDR::Test::seedUpgradedCoreToken(dir.path()));

    RadioModel window(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&window, &proxy);
    window.attachStation(&client);
    client.setCoreOnThisComputerForTest(true);
    TciServer local(&window);
    TciSwitch tci(&local, &window);
    AppSettings::instance().setValue(QStringLiteral("TciServerEnabled"), QStringLiteral("False"));
    CatTciServerPage page;
    page.setRadioModel(&window);
    QVERIFY(!page.switchOnForTesting());
    // The window's switch at start (before any link): off here.
    tci.setSwitch(false, port, QHostAddress(QHostAddress::LocalHost), /*tellCore=*/false);
    QVERIFY(!local.isRunning());
    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
    stationEnd->linkTo(clientEnd);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server.token());
    server.acceptTransport(stationEnd);
    QVERIFY(completed.wait(5000) || !completed.isEmpty());
    NEREUS_TRY_VERIFY(client.stationTciAvailable());
    window.reportStationLinkStateChanged();
    NEREUS_TRY_VERIFY(!local.isRunning());   // connected on the Core's computer: none here

    // The phone turns the Core's switch on: the Core serves.
    QString reason;
    QVERIFY(station.setStationTciForStation(true, port, &reason));
    NEREUS_TRY_VERIFY_WITH_TIMEOUT(station.stationTciModel()->listening(), 5000);
    NEREUS_TRY_VERIFY(window.stationTciModel()->listening());
    QVERIFY(station.stationTciModel()->error().isEmpty());
    QVERIFY(!local.isRunning());
    NEREUS_TRY_VERIFY(tci.switchOn());
    QCOMPARE(tci.port(), port);
    // The TCI page shows the Core's switch and port.
    NEREUS_TRY_VERIFY(page.switchOnForTesting());
    QCOMPARE(page.portForTesting(), int(port));
    const auto servedAt = [](const QString& address, quint16 appPort) {
        QWebSocket app;
        QStringList frames;
        QObject::connect(&app, &QWebSocket::textMessageReceived, &app,
                         [&frames](const QString& text) { frames.append(text); });
        app.open(QUrl(QStringLiteral("ws://%1:%2").arg(address).arg(appPort)));
        const bool ok = QTest::qWaitFor([&] {
            return frames.join(QString()).contains(QStringLiteral("receive_only:true;"));
        }, 5000);
        app.close();
        return ok;
    };
    QVERIFY(servedAt(QStringLiteral("127.0.0.1"), port));   // an app on this computer
    if (!stationAddress.isEmpty()) {
        QVERIFY(servedAt(stationAddress, port));             // a device at the station
    }
    stationEnd->closeLink(QStringLiteral("test done"));
    QVERIFY(station.setStationTciForStation(false, port, &reason));
    AppSettings::instance().clear();
}

namespace {

// A Core with a station TCI server on this computer (loopback only) and a
// window on it whose TCI switch was on before it connected.
struct TciCoreAndWindow {
    QTemporaryDir dir;
    RadioModel station;
    AppSettings stationSettings;
    std::unique_ptr<StationServer> server;
    RadioModel window{RadioModel::Role::Remote};
    SettingsProxy proxy;
    StationClient client{&window, &proxy};
    TciServer local{&window};
    TciSwitch tci{&local, &window};
    TciCoreAndWindow() : stationSettings(dir.filePath(QStringLiteral("station.settings")))
    {
        window.attachStation(&client);
        client.setCoreOnThisComputerForTest(true);
    }
    // The Core's own settings store (AppSettings::instance()), where its
    // station TCI switch is kept and what the window's snapshot reads.
    void start()
    {
        station.enableStationTci(QStringLiteral("127.0.0.1"));   // reads the Core's switch
        server = std::make_unique<StationServer>(&station, AppSettings::instance(), NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
    }
    bool connect(QObject* owner)
    {
        auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), owner);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), owner);
        stationEnd->linkTo(clientEnd);
        QSignalSpy completed(&client, &StationClient::handshakeComplete);
        client.startSession(clientEnd, server->token());
        server->acceptTransport(stationEnd);
        return completed.wait(5000) || !completed.isEmpty();
    }
};

bool tciAppServed(quint16 port)
{
    QWebSocket app;
    QStringList frames;
    QObject::connect(&app, &QWebSocket::textMessageReceived, &app,
                     [&frames](const QString& text) { frames.append(text); });
    app.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port)));
    const bool ok = QTest::qWaitFor([&] {
        return frames.join(QString()).contains(QStringLiteral("receive_only:true;"));
    }, 5000);
    app.close();
    return ok;
}

} // namespace

// Rework part 2 (R-R3-48): nereusd and this window on one computer with TCI
// on before the upgrade: the Core has no stored station switch, so the
// window's switch seeds it at connect and apps here keep TCI (the Core's).
void RemotePeripheralsTest::upgradeKeepsTciOnTheCoresComputer()
{
    AppSettings::instance().clear();
    TciCoreAndWindow cw;
    cw.start();
    const quint16 port = freeLoopbackPort();
    cw.tci.setSwitch(true, port, QHostAddress(QHostAddress::LocalHost), /*tellCore=*/false);
    QVERIFY(cw.local.isRunning());   // before the link: as before
    QVERIFY(!AppSettings::instance().contains(QStringLiteral("StationTci_Enabled")));
    QVERIFY(cw.connect(this));
    NEREUS_TRY_VERIFY(cw.client.stationTciAvailable());
    cw.window.reportStationLinkStateChanged();
    NEREUS_TRY_VERIFY_WITH_TIMEOUT(cw.station.stationTciModel()->listening(), 5000);
    QCOMPARE(cw.station.stationTciModel()->port(), int(port));
    NEREUS_TRY_VERIFY(!cw.local.isRunning());
    QVERIFY(cw.tci.switchOn());
    QVERIFY(tciAppServed(port));
    QString reason;
    QVERIFY(cw.station.setStationTciForStation(false, port, &reason));
    AppSettings::instance().clear();
}

// Rework part 2: a Core that already keeps a station switch (off here)
// wins at connect; the window's switch follows it and the Core is not
// changed.
void RemotePeripheralsTest::coresStoredSwitchWinsOverTheLink()
{
    AppSettings::instance().clear();
    TciCoreAndWindow cw;
    AppSettings::instance().setValue(QStringLiteral("StationTci_Enabled"), QStringLiteral("False"));
    AppSettings::instance().setValue(QStringLiteral("StationTci_Port"), QStringLiteral("50001"));
    cw.start();
    const quint16 port = freeLoopbackPort();
    cw.tci.setSwitch(true, port, QHostAddress(QHostAddress::LocalHost), /*tellCore=*/false);
    QVERIFY(cw.connect(this));
    NEREUS_TRY_VERIFY(cw.client.stationTciAvailable());
    cw.window.reportStationLinkStateChanged();
    NEREUS_TRY_VERIFY(!cw.tci.switchOn());
    QCOMPARE(cw.tci.port(), quint16(50001));
    QVERIFY(!cw.local.isRunning());
    NereusSDR::Test::settleSession();
    QVERIFY(!cw.station.stationTciModel()->enabled());
    QVERIFY(!cw.station.stationTciModel()->listening());
    AppSettings::instance().clear();
}

// Rework follow-up 4 (R-R3-48): moving to another Core in the same
// process, the connect-time rule reads that Core's settings, not the last
// Core's: whether a Core keeps a TCI switch is unknown between links.
void RemotePeripheralsTest::connectRuleReadsTheCurrentCore()
{
    AppSettings::instance().clear();
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings::instance().setValue(QStringLiteral("StationTci_Enabled"), QStringLiteral("False"));
    RadioModel coreA;   // keeps a switch (the process's settings are its store)
    StationServer serverA(&coreA, AppSettings::instance(), NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
    RadioModel coreB;   // keeps none
    AppSettings storeB(dir.filePath(QStringLiteral("b.settings")));
    StationServer serverB(&coreB, storeB, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));

    RadioModel window(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&window, &proxy);
    window.attachStation(&client);
    const auto linkTo = [&](StationServer& server, const QString& name) {
        auto* stationEnd = new LoopbackTransport(name + QStringLiteral("-station"), this);
        auto* clientEnd = new LoopbackTransport(name + QStringLiteral("-client"), this);
        stationEnd->linkTo(clientEnd);
        QSignalSpy completed(&client, &StationClient::handshakeComplete);
        client.startSession(clientEnd, server.token());
        server.acceptTransport(stationEnd);
        [&] { QVERIFY(completed.wait(5000) || !completed.isEmpty()); }();
        return stationEnd;
    };
    LoopbackTransport* endA = linkTo(serverA, QStringLiteral("a"));
    NEREUS_TRY_COMPARE(client.coreStationTciStored(), 1);
    endA->closeLink(QStringLiteral("moving to another Core"));
    NEREUS_TRY_COMPARE(client.coreStationTciStored(), -1);
    LoopbackTransport* endB = linkTo(serverB, QStringLiteral("b"));
    NEREUS_TRY_COMPARE(client.coreStationTciStored(), 0);
    endB->closeLink(QStringLiteral("test done"));
    AppSettings::instance().clear();
}

namespace {

// R-R3-47: a Core that owns its accessories and a remote window on it, over
// the in-process loopback.
struct CoreAndWindow {
    QTemporaryDir dir;
    QTemporaryDir keyDir;
    RadioModel station;
    AppSettings stationSettings;
    StationServer server;
    RadioModel window{RadioModel::Role::Remote};
    SettingsProxy proxy;
    StationClient client{&window, &proxy};
    bool pairedTransmitter{false};
    CoreAndWindow()
        : stationSettings(dir.filePath(QStringLiteral("station.settings")))
        , server((prepareStation(station), &station), stationSettings,
                 NereusSDR::Test::seedUpgradedCoreToken(dir.path()))
    {
        window.attachStation(&client);
    }
    static void prepareStation(RadioModel& core)
    {
        AppSettings::instance().setValue(QStringLiteral("PeripheralsMigrationDone"),
                                         QStringLiteral("True"));
        core.enableStationAccessoryIdentity();
        core.setReceiveOnlyStationPolicy(true);
        RadioInfo radio;
        radio.macAddress = QStringLiteral("aa:bb:cc:dd:ee:47");
        core.setLastRadioInfoForTest(radio);
        core.setConnectionStateForTest(ConnectionState::Connected);
    }
    bool pairTransmitWindow()
    {
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        if (!key->isValid()) { return false; }
        PairedDevice record;
        record.id = key->fingerprint();
        record.publicKeySpki = key->publicKeySpki();
        record.name = QStringLiteral("Desktop");
        record.kind = QStringLiteral("computer");
        if (!server.deviceStore()->add(record)) { return false; }
        client.setDeviceIdentity(key, record.name);
        server.setRemoteTransmitAllowed(true);
        pairedTransmitter = true;
        return true;
    }
    LoopbackTransport* connect(QObject* owner)
    {
        auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), owner);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), owner);
        stationEnd->linkTo(clientEnd);
        if (pairedTransmitter) {
            QString pin = server.certificateFingerprint();
            pin.remove(QLatin1Char(':'));
            clientEnd->setPeerCertificateSha256(QByteArray::fromHex(pin.toLatin1()));
            client.startSession(clientEnd, {}, {}, server.stationIdentity().fingerprint());
        } else {
            client.startSession(clientEnd, server.token());
        }
        server.acceptTransport(stationEnd);
        return stationEnd;
    }
};

} // namespace

// R-R3-47 / R-R3-22: a remote window's Power Genius and Tuner Genius pages
// show what the Core keeps, live, and change it only through the Core.
void RemotePeripheralsTest::remoteWindowShowsTheCoresRecords()
{
    AppSettings::instance().clear();
    CoreAndWindow cw;
    RadioModel& station = cw.station;
    RadioModel& window = cw.window;
    PgxlAdvancedPage pgxlPage(&window);
    TgxlAdvancedPage tgxlPage(&window);
    QVERIFY(OperatorWording::isPlain(pgxlPage.remoteNoteForTesting()));
    QVERIFY(!pgxlPage.powerCapCheckForTesting()->isEnabled());   // no Core yet

    LoopbackTransport* stationEnd = cw.connect(this);
    NEREUS_TRY_VERIFY(cw.client.accessoryDataAvailable());
    // 2 from parity Task 10 (the RF-Kit's connection counts).
    QCOMPARE(cw.server.accessoryDataVersion(), 3);
    window.reportStationLinkStateChanged();
    NEREUS_TRY_VERIFY(pgxlPage.powerCapCheckForTesting()->isEnabled());
    QVERIFY(OperatorWording::isPlain(pgxlPage.remoteNoteForTesting()));

    // A Power Genius fault on the Core appears without a reconnect, with
    // its time, device and plain words. (The Core's amp connection takes
    // status lines only from an admitted amp, so the fault is recorded as
    // RadioModel::onPgxlStatus records it, with the captured readings.)
    station.pgxlFaultLog()->capture(FaultEvent{QDateTime::currentMSecsSinceEpoch(),
                                               QStringLiteral("FAULT"), 1820.0f, 2.85f, 78.0f,
                                               FaultLog::likelyCauseFor(1820.0f, 2.85f, 78.0f)});
    QCOMPARE(station.pgxlFaultLog()->events().size(), 1);
    NEREUS_TRY_COMPARE(window.pgxlFaultLog()->events().size(), 1);
    const FaultEvent pgxlFault = window.pgxlFaultLog()->events().first();
    QCOMPARE(pgxlFault.device, QStringLiteral("pgxl"));
    QCOMPARE(pgxlFault.whenMs, station.pgxlFaultLog()->events().first().whenMs);
    QVERIFY(pgxlFault.text.startsWith(QStringLiteral("The Power Genius reported a fault.")));
    QCOMPARE(pgxlPage.faultRowCountForTesting(), 1);
    QCOMPARE(pgxlPage.faultTextForTesting(0), pgxlFault.text);
    QVERIFY(OperatorWording::isPlain(pgxlPage.faultTextForTesting(0)));

    // An RF-Kit fault (the amp's interface error) and a Tuner Genius one.
    station.rfKitConnection()->injectJsonForTesting(
        QStringLiteral("/operational-interface"),
        R"({"operational_interface":"UDP","error":"CAT timeout"})");
    NEREUS_TRY_COMPARE(window.rfkitFaultLog()->events().size(), 1);
    QCOMPARE(window.rfkitFaultLog()->events().first().detail, QStringLiteral("CAT timeout"));
    QVERIFY(OperatorWording::isPlain(window.rfkitFaultLog()->events().first().text));
    station.tgxlFaultLog()->captureNotice(QStringLiteral("link"),
                                          QStringLiteral("The Tuner Genius stopped answering."),
                                          QString());
    NEREUS_TRY_COMPARE(tgxlPage.faultRowCountForTesting(), 1);
    QCOMPARE(tgxlPage.faultTextForTesting(0), QStringLiteral("The Tuner Genius stopped answering."));
    QVERIFY(window.accessoryDataModel()->faultRevision() >= 3);

    // Cleared from the remote page: the Core's history goes, and the window's.
    pgxlPage.clearFaultsButtonForTesting()->click();
    NEREUS_TRY_VERIFY(station.pgxlFaultLog()->events().isEmpty());
    NEREUS_TRY_COMPARE(pgxlPage.faultRowCountForTesting(), 0);
    tgxlPage.clearFaultsButtonForTesting()->click();
    NEREUS_TRY_VERIFY(station.tgxlFaultLog()->events().isEmpty());
    NEREUS_TRY_COMPARE(tgxlPage.faultRowCountForTesting(), 0);
    QCOMPARE(station.rfkitFaultLog()->events().size(), 1);   // only the one asked for

    // The counters are the Core's: a retry the Core's amp connection counts
    // shows here; one on the window's own (idle) connection does not count.
    emit window.pgxlConnection()->reconnectAttempt(1, 1000);
    window.pgxlDiagnostics()->testFlushCoalesceTimer();
    QCOMPARE(pgxlPage.reconnectCountTextForTesting(), QStringLiteral("0"));
    emit station.pgxlConnection()->reconnectAttempt(1, 1000);
    emit station.pgxlConnection()->reconnectAttempt(2, 2000);
    station.pgxlDiagnostics()->testFlushCoalesceTimer();
    NEREUS_TRY_COMPARE(pgxlPage.reconnectCountTextForTesting(), QStringLiteral("2"));
    QCOMPARE(window.accessoryDataModel()->pgxlReconnectCount(), 2);
    emit station.tgxlConnection()->reconnectAttempt(1, 1000);
    station.tgxlDiagnostics()->testFlushCoalesceTimer();
    NEREUS_TRY_COMPARE(tgxlPage.reconnectCountTextForTesting(), QStringLiteral("1"));

    // The output limit is set through the Core; the Core raises the alert
    // and the window sees it.
    pgxlPage.powerCapSpinForTesting()->setValue(800);   // limit off: not sent yet
    pgxlPage.powerCapCheckForTesting()->setChecked(true);
    NEREUS_TRY_VERIFY(station.accessoryDataModel()->powerCapEnabled());
    QCOMPARE(station.accessoryDataModel()->powerCapW(), 800);
    NEREUS_TRY_VERIFY(window.accessoryDataModel()->powerCapEnabled());
    // The amp's peak forward power as the Core reads it (onPgxlStatus).
    emit station.ampMetersChanged(1000.0f, 1.13f);
    NEREUS_TRY_COMPARE(window.accessoryDataModel()->powerCapAlertCount(), 1);
    QVERIFY(window.accessoryDataModel()->powerCapExceeded());
    QCOMPARE(window.accessoryDataModel()->powerCapAlertText(),
             QStringLiteral("Power Genius output 1000 W is above the 800 W limit."));

    // Antenna names and tune memory are the Core's. (A window's own edit
    // reaches the Core as a station setting; the Core then publishes it.)
    AppSettings::instance().setValue(QStringLiteral("TGXL_Ant1_Label"), QStringLiteral("Dipole"));
    station.applyRemoteAccessorySetting(QStringLiteral("TGXL_Ant1_Label"));
    NEREUS_TRY_COMPARE(window.accessoryDataModel()->tgxlAntenna1Label(), QStringLiteral("Dipole"));
    NEREUS_TRY_COMPARE(tgxlPage.antennaLabelForTesting(1), QStringLiteral("Dipole"));
    station.tuneMemoryStore()->store(TuneMemory{2, Band::Band40m, 10, 20, 30, 1790000000000});
    NEREUS_TRY_COMPARE(window.tuneMemoryStore()->listAll().size(), 1);
    QCOMPARE(window.tuneMemoryStore()->listAll().first().band, Band::Band40m);
    QCOMPARE(tgxlPage.tuneMemoryRowCountForTesting(), 1);

    // The window opened no accessory connection of its own.
    QVERIFY(!window.pgxlConnection()->isConnected());
    QVERIFY(!window.tgxlConnection()->isConnected());
    QVERIFY(!window.rfKitConnection()->isConnected());
    stationEnd->closeLink(QStringLiteral("test done"));
    AppSettings::instance().clear();
}

// R-R3-47 / R-R3-22: the interlock policy changed from a remote window takes
// effect on the Core at once, where the refusal to transmit happens; the
// window that changed it and a window that connects later show it; a
// request the Core cannot take changes nothing and says why.
void RemotePeripheralsTest::remoteWindowChangesTheInterlockOnTheCore()
{
    AppSettings::instance().clear();
    CoreAndWindow cw;
    RadioModel& station = cw.station;
    PgxlInterlockPage page(&cw.window);
    QVERIFY(!page.modeComboForTesting()->isEnabled());
    LoopbackTransport* stationEnd = cw.connect(this);
    NEREUS_TRY_VERIFY(cw.client.accessoryDataAvailable());
    cw.window.reportStationLinkStateChanged();
    NEREUS_TRY_VERIFY(page.modeComboForTesting()->isEnabled());
    QVERIFY(OperatorWording::isPlain(page.remoteNoteForTesting()));
    QVERIFY(station.txInterlockPolicy()->evaluateTxRequest(true, false, 1.0f));

    page.modeComboForTesting()->setCurrentIndex(2);   // Block
    NEREUS_TRY_COMPARE(station.txInterlockPolicy()->mode(), TxInterlockPolicy::Block);
    QCOMPARE(AppSettings::instance().value(QStringLiteral("PGXL_TxInterlockMode")).toString(),
             QStringLiteral("Block"));
    // The Core refuses to transmit now; the window's copy is the Core's.
    QVERIFY(!station.txInterlockPolicy()->evaluateTxRequest(true, false, 1.0f));
    NEREUS_TRY_COMPARE(cw.window.txInterlockPolicy()->mode(), TxInterlockPolicy::Block);
    QCOMPARE(cw.window.accessoryDataModel()->interlockMode(),
             AccessoryDataModel::InterlockMode::Block);
    QCOMPARE(page.modeComboForTesting()->currentIndex(), 2);

    page.swrGateCheckboxForTesting()->setChecked(true);
    page.graceSpinboxForTesting()->setValue(1250);
    page.swrGateMaxSpinboxForTesting()->setValue(2.4);
    NEREUS_TRY_COMPARE(station.txInterlockPolicy()->graceMs(), 1250);
    NEREUS_TRY_VERIFY(qFuzzyCompare(station.txInterlockPolicy()->swrGateMax(), 2.4f));
    QVERIFY(station.txInterlockPolicy()->swrGateEnabled());
    NEREUS_TRY_COMPARE(cw.window.txInterlockPolicy()->graceMs(), 1250);

    // A request the Core cannot take: nothing changes; the window says why,
    // as an accessory refusal (L1), never as a slice one.
    QSignalSpy sliceToast(&cw.window, &RadioModel::sliceAddRejected);
    QSignalSpy refused(&cw.window, &RadioModel::accessoryRequestRefused);
    QVERIFY(cw.client.requestTxInterlockPolicy(1, 99999, false, 2.0).sent);
    NEREUS_TRY_COMPARE(refused.count(), 1);
    QCOMPARE(refused.first().at(0).toString(), QStringLiteral("interlock"));
    QVERIFY(OperatorWording::isPlain(refused.first().at(1).toString()));
    QCOMPARE(sliceToast.count(), 0);
    QCOMPARE(station.txInterlockPolicy()->mode(), TxInterlockPolicy::Block);
    QCOMPARE(page.modeComboForTesting()->currentIndex(), 2);

    // A window that connects later shows the Core's policy.
    stationEnd->closeLink(QStringLiteral("first window gone"));
    RadioModel second(RadioModel::Role::Remote);
    SettingsProxy secondProxy;
    StationClient secondClient(&second, &secondProxy);
    second.attachStation(&secondClient);
    PgxlInterlockPage secondPage(&second);
    auto* stationEnd2 = new LoopbackTransport(QStringLiteral("station-end-2"), this);
    auto* clientEnd2 = new LoopbackTransport(QStringLiteral("client-end-2"), this);
    stationEnd2->linkTo(clientEnd2);
    secondClient.startSession(clientEnd2, cw.server.token());
    cw.server.acceptTransport(stationEnd2);
    NEREUS_TRY_COMPARE(second.txInterlockPolicy()->mode(), TxInterlockPolicy::Block);
    QCOMPARE(second.txInterlockPolicy()->graceMs(), 1250);
    NEREUS_TRY_COMPARE(secondPage.modeComboForTesting()->currentIndex(), 2);
    QCOMPARE(secondPage.graceSpinboxForTesting()->value(), 1250);
    stationEnd2->closeLink(QStringLiteral("test done"));
    AppSettings::instance().clear();
}

// R-R3-47: a Core that does not share its interlock (an older Core) leaves
// the section showing, unchangeable, with the reason in user words.
void RemotePeripheralsTest::olderCoreLeavesTheInterlockSayingWhy()
{
    RadioModel model(RadioModel::Role::Remote);
    RecordingTgxlLink link;
    link.linkReady = true;
    model.attachStation(&link);
    PgxlInterlockPage page(&model);
    QVERIFY(!page.modeComboForTesting()->isEnabled());
    QVERIFY(!page.graceSpinboxForTesting()->isEnabled());
    QVERIFY(page.remoteNoteForTesting().contains(QStringLiteral("does not share")));
    QVERIFY(OperatorWording::isPlain(page.remoteNoteForTesting()));
    PgxlAdvancedPage advanced(&model);
    QVERIFY(!advanced.powerCapCheckForTesting()->isEnabled());
    QVERIFY(!advanced.clearFaultsButtonForTesting()->isEnabled());
    QVERIFY(OperatorWording::isPlain(advanced.remoteNoteForTesting()));
}

namespace {

// Accept the modal dialog a local page opens (Save & Reboot).
void acceptNextModal()
{
    QTimer::singleShot(0, [] {
        if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
            dialog->accept();
        }
    });
}

void editName(QLineEdit* edit, const QString& name)
{
    edit->setText(name);
    edit->setModified(true);
    emit edit->editingFinished();
}

// The Core's Power Genius, admitted: the V banner, the captured `info`
// reply and the captured discovery announcement.
bool admitCoreAmp(RadioModel& station, FakeGenius& amp)
{
    QString reason;
    if (!station.configurePgxlForStation(QStringLiteral("127.0.0.1"), amp.port(), &reason)
        || !amp.accept()) {
        return false;
    }
    amp.send(QStringLiteral("V3.8.9"));
    const int info = amp.waitFor(QStringLiteral("info"));
    if (info < 0) {
        return false;
    }
    amp.reply(info, QStringLiteral("0|serial=10-200/24-0046  version=3.8.9 protocol=1.0 mains=240"));
    auto* controller = station.findChild<StationPgxlController*>();
    if (!controller
        || !QTest::qWaitFor([&] { return controller->findChild<LanDiscovery*>() != nullptr; },
                            3000)) {
        return false;
    }
    controller->findChild<LanDiscovery*>()->injectDatagramForTesting(
        QStringLiteral("PowerGeniusXL ip=127.0.0.1 v=3.8.9 serial=10-200/24-0046 "
                       "nickname=PowerGeniusXL"),
        amp.port());
    return QTest::qWaitFor([&] { return station.pgxlConnection()->isConnected(); }, 3000);
}

// The Core's Tuner Genius, admitted likewise.
bool admitCoreTuner(RadioModel& station, FakeGenius& tuner)
{
    QString reason;
    if (!station.configureTgxlForStation(QStringLiteral("127.0.0.1"), tuner.port(), &reason)
        || !tuner.accept()) {
        return false;
    }
    tuner.send(QStringLiteral("V1.2.17"));
    const int info = tuner.waitFor(QStringLiteral("info"));
    if (info < 0) {
        return false;
    }
    tuner.reply(info, QStringLiteral("0|info serial=241288-1 version=1.2.17 "
                                     "nickname=Tuner_Genius_XL 3way=1"));
    auto* controller = station.findChild<StationTgxlController*>();
    if (!controller
        || !QTest::qWaitFor([&] { return controller->findChild<LanDiscovery*>() != nullptr; },
                            3000)) {
        return false;
    }
    controller->findChild<LanDiscovery*>()->injectDatagramForTesting(
        QStringLiteral("TunerGeniusXL ip=127.0.0.1 v=1.2.17 serial=241288-1 "
                       "nickname=Tuner_Genius_XL"),
        tuner.port());
    return QTest::qWaitFor([&] { return station.tgxlConnection()->isConnected(); }, 3000);
}

} // namespace

// R-R3-47 / R-R3-22: every control on a remote window's Power Genius
// Advanced page works through the Core. The same clicks on a remote page
// and on a local window's page reach their amps as the same commands; the
// remote page asks before network changes and Save & Reboot, and sends
// nothing on a no; the amp's answers and values, and the Core's refusals,
// show on the page in plain words; the window opens no connection to the amp
// and nothing operates it.
void RemotePeripheralsTest::remoteWindowChangesTheAmpsOwnSettingsThroughTheCore()
{
    AppSettings::instance().clear();
    CoreAndWindow cw;
    RadioModel& station = cw.station;
    RadioModel& window = cw.window;
    station.smartSdrListener()->setListenEndpointForTesting(QHostAddress::LocalHost, 0);
    station.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("True"));
    FakeGenius amp;
    QVERIFY(amp.listen());
    PgxlAdvancedPage page(&window);
    bool yes = true;
    QStringList asked;
    page.setConfirmationForTesting([&](const QString& title, const QString& text) {
        asked.append(title + QLatin1Char('|') + text);
        return yes;
    });
    // A local window's page, on its own amp, for the same clicks.
    FakeGenius localAmp;
    QVERIFY(localAmp.listen());
    RadioModel local;
    PgxlAdvancedPage localPage(&local);
    QStringList localAsked;   // the local page asks the same question
    localPage.setConfirmationForTesting([&](const QString& title, const QString& text) {
        localAsked.append(title + QLatin1Char('|') + text);
        return true;
    });

    LoopbackTransport* stationEnd = cw.connect(this);
    NEREUS_TRY_VERIFY(cw.client.pgxlDeviceSettingsAvailable());
    window.reportStationLinkStateChanged();
    QVERIFY(page.nicknameEditForTesting()->isEnabled());
    QVERIFY(page.pairAttemptCheckForTesting()->isEnabled());
    // The Core has no amp yet: Apply, Revert and Save & Reboot wait for it,
    // and a request says why in the Core's words.
    QVERIFY(!page.applyNetworkButtonForTesting()->isEnabled());
    QVERIFY(!page.revertButtonForTesting()->isEnabled());
    QSignalSpy offlineRefused(&window, &RadioModel::accessoryRequestRefused);
    page.show();   // rework part 5: a refusal counts as shown only on a visible page
    editName(page.nicknameEditForTesting(), QStringLiteral("Offline"));
    NEREUS_TRY_COMPARE(page.deviceAnswerForTesting(),
                 QStringLiteral("The Core is not connected to the Power Genius."));
    // Follow-up 3: shown on the page that sent it, so not toasted too.
    QCOMPARE(offlineRefused.count(), 1);
    QCOMPARE(offlineRefused.first().size(), 3);
    QVERIFY(offlineRefused.first().at(2).toBool());
    QVERIFY(OperatorWording::isPlain(page.deviceAnswerForTesting()));

    QVERIFY(admitCoreAmp(station, amp));
    NEREUS_TRY_VERIFY(page.applyNetworkButtonForTesting()->isEnabled());
    NEREUS_TRY_COMPARE(page.firmwareTextForTesting(), QStringLiteral("3.8.9"));
    local.pgxlConnection()->connectToPgxl(QStringLiteral("127.0.0.1"), localAmp.port());
    QVERIFY(localAmp.accept());
    localAmp.send(QStringLiteral("V3.8.9"));
    // The local page reads the amp's settings when it connects.
    QVERIFY(localAmp.waitFor(QStringLiteral("ifconf read")) >= 0);

    // I5: DHCP off with no address is not sent from either page, and the
    // remote page asks nothing; both say why in the same words.
    {
        const int remoteBefore = amp.commands.size();
        const int localBefore = localAmp.commands.size();
        const qsizetype askedBefore = asked.size();
        for (PgxlAdvancedPage* p : {&page, &localPage}) {
            p->dhcpCheckForTesting()->setChecked(false);
            p->ipEditForTesting()->clear();
            p->netmaskEditForTesting()->clear();
            p->gatewayEditForTesting()->clear();
            p->applyNetworkButtonForTesting()->click();
            QCOMPARE(p->networkProblemForTesting(),
                     QStringLiteral("Without DHCP, enter an address and a netmask."));
            QVERIFY(OperatorWording::isPlain(p->networkProblemForTesting()));
            // Follow-up 7: the subnet's broadcast address is refused too.
            p->ipEditForTesting()->setText(QStringLiteral("192.168.1.255"));
            p->netmaskEditForTesting()->setText(QStringLiteral("255.255.255.0"));
            p->applyNetworkButtonForTesting()->click();
            QCOMPARE(p->networkProblemForTesting(),
                     QStringLiteral("Enter an address the device can use on your network."));
            p->ipEditForTesting()->clear();
            p->netmaskEditForTesting()->clear();
        }
        QCOMPARE(asked.size(), askedBefore);
        NereusSDR::Test::settleSession();
        for (int i = remoteBefore; i < amp.commands.size(); ++i) {
            QVERIFY2(!amp.commands.at(i).startsWith(QStringLiteral("ifconf address")),
                     qPrintable(amp.commands.at(i)));
        }
        for (int i = localBefore; i < localAmp.commands.size(); ++i) {
            QVERIFY2(!localAmp.commands.at(i).startsWith(QStringLiteral("ifconf address")),
                     qPrintable(localAmp.commands.at(i)));
        }
    }

    // ---- The same clicks on both pages reach the amps as the same commands.
    const int remoteMark = amp.commands.size();
    const int localMark = localAmp.commands.size();
    const auto drive = [](PgxlAdvancedPage& p) {
        editName(p.nicknameEditForTesting(), QStringLiteral("Shack_PGXL"));
        p.biasClassAForTesting()->click();
        p.fanModeComboForTesting()->setCurrentText(QStringLiteral("Quiet"));
        p.ledSliderForTesting()->setValue(40);
        p.dhcpCheckForTesting()->setChecked(false);
        p.ipEditForTesting()->setText(QStringLiteral("192.168.1.50"));
        p.netmaskEditForTesting()->setText(QStringLiteral("255.255.255.0"));
        p.gatewayEditForTesting()->setText(QStringLiteral("192.168.1.1"));
        p.applyNetworkButtonForTesting()->click();
        p.revertButtonForTesting()->click();
        p.ledSliderForTesting()->setValue(50);
        QVERIFY(p.saveAndRebootButtonForTesting()->isEnabled());
        acceptNextModal();
        p.saveAndRebootButtonForTesting()->click();
    };
    drive(page);
    drive(localPage);
    QVERIFY(amp.waitFor(QStringLiteral("save"), remoteMark) >= 0);
    QVERIFY(localAmp.waitFor(QStringLiteral("save"), localMark) >= 0);
    const QStringList expected{
        QStringLiteral("setup nickname=Shack_PGXL"),
        QStringLiteral("setup bias=a"),
        QStringLiteral("setup fan=quiet"),
        QStringLiteral("setup led=40"),
        QStringLiteral("ifconf address=192.168.1.50 netmask=255.255.255.0 "
                       "gateway=192.168.1.1 dhcp=false"),
        QStringLiteral("setup read"),
        QStringLiteral("ifconf read"),
        QStringLiteral("setup led=50"),
        QStringLiteral("save"),
    };
    QCOMPARE(localAmp.settingsCommands(localMark), expected);
    QCOMPARE(amp.settingsCommands(remoteMark), expected);
    // One request per click on the remote page (the local page sends the
    // bias twice, once per radio button).
    QCOMPARE(amp.commands.mid(remoteMark).count(QStringLiteral("setup bias=a")), 1);
    // The remote page asked first (M4: in words true in a remote window,
    // with no Scan LAN, which a remote window does not offer), and the
    // local page asked the same (operator decision 2026-09-24).
    for (const QString& text : {PgxlAdvancedPage::networkQuestionText(),
                                TgxlAdvancedPage::networkQuestionText()}) {
        QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
        QVERIFY(!text.contains(QStringLiteral("Scan LAN")));
        QVERIFY(!text.contains(QStringLiteral("host")));
    }
    QCOMPARE(asked, (QStringList{
        QStringLiteral("Apply Network Settings|") + PgxlAdvancedPage::networkQuestionText(),
        QStringLiteral("Save & Reboot PGXL|") + PgxlSaveRebootDialog::message()}));
    QCOMPARE(localAsked, QStringList{QStringLiteral("Apply Network Settings|")
                                     + PgxlAdvancedPage::networkQuestionText()});

    // ---- A no sends nothing.
    yes = false;
    const int declineMark = amp.commands.size();
    page.applyNetworkButtonForTesting()->click();
    page.ledSliderForTesting()->setValue(60);
    QVERIFY(amp.waitFor(QStringLiteral("setup led=60"), declineMark) >= 0);
    page.saveAndRebootButtonForTesting()->click();
    QCOMPARE(asked.size(), 4);
    NereusSDR::Test::settleSession();
    QCOMPARE(amp.settingsCommands(declineMark), QStringList{QStringLiteral("setup led=60")});
    yes = true;

    // ---- The amp's answers and values show on the page.
    int at = -1;
    const int answerMark = amp.commands.size();
    editName(page.nicknameEditForTesting(), QStringLiteral("Remote_Amp"));
    at = amp.waitFor(QStringLiteral("setup nickname=Remote_Amp"), answerMark);
    QVERIFY(at >= 0);
    NEREUS_TRY_COMPARE(page.deviceAnswerForTesting(),
                 QStringLiteral("Sent to the Power Genius. Waiting for its answer."));
    amp.reply(at, QStringLiteral("0|"));
    NEREUS_TRY_COMPARE(page.deviceAnswerForTesting(),
                 QStringLiteral("The Power Genius took the new name."));
    QVERIFY(OperatorWording::isPlain(page.deviceAnswerForTesting()));
    QCOMPARE(window.accessorySettingsModel()->pgxlNickname(), QStringLiteral("Remote_Amp"));
    editName(page.nicknameEditForTesting(), QStringLiteral("Refused"));
    at = amp.waitFor(QStringLiteral("setup nickname=Refused"), answerMark);
    QVERIFY(at >= 0);
    // M9: a refusal. 50000015 is the code a real Power Genius sent when it
    // refused `amplifier create` (bench note of 2026-05-21 in
    // RadioModel.cpp, above the PGXL_PairModel read); the design doc
    // (2026-05-18-pgxl-tgxl-and-analog-smeter-design.md section 6.1, from
    // the FlexRadio wiki) says only that non-zero is a failure. The amp's
    // refusal of a `setup` command itself has not been observed.
    amp.reply(at, QStringLiteral("50000015|"));
    NEREUS_TRY_COMPARE(page.deviceAnswerForTesting(),
                 QStringLiteral("The Power Genius did not take the new name."));
    page.revertButtonForTesting()->click();
    const int setupAt = amp.waitFor(QStringLiteral("setup read"), answerMark);
    const int ifconfAt = amp.waitFor(QStringLiteral("ifconf read"), answerMark);
    QVERIFY(setupAt >= 0 && ifconfAt >= 0);
    // M9: unobserved reply shapes (see tst_station_pgxl_controller's
    // Revert: the design doc's section 6.4 `setup read` reply carries no
    // `bias=`; `dhcp=`/`ip=` are the local TGXL page's parser keys, where the
    // design doc documents `address=` and `dhcp=false`). Pending hardware.
    amp.reply(setupAt, QStringLiteral("0|nickname=Amp2 bias=classab fan=continuous led=90"));
    amp.reply(ifconfAt, QStringLiteral("0|dhcp=0 ip=10.0.0.5 netmask=255.0.0.0 gateway=10.0.0.1"));
    NEREUS_TRY_COMPARE(page.ipEditForTesting()->text(), QStringLiteral("10.0.0.5"));
    QCOMPARE(page.nicknameEditForTesting()->text(), QStringLiteral("Amp2"));
    QVERIFY(!page.biasClassAForTesting()->isChecked());
    QCOMPARE(page.fanModeComboForTesting()->currentText(), QStringLiteral("Continuous"));
    QCOMPARE(page.ledSliderForTesting()->value(), 90);
    QCOMPARE(page.netmaskEditForTesting()->text(), QStringLiteral("255.0.0.0"));
    QCOMPARE(page.gatewayEditForTesting()->text(), QStringLiteral("10.0.0.1"));
    QVERIFY(!page.dhcpCheckForTesting()->isChecked());
    QCOMPARE(page.deviceAnswerForTesting(),
             QStringLiteral("The Power Genius sent its network settings."));

    // ---- A Core refusal reaches the page on its own route, not the slice
    // toast, and changes nothing.
    QSignalSpy sliceToast(&window, &RadioModel::sliceAddRejected);
    QSignalSpy refused(&window, &RadioModel::accessoryRequestRefused);
    const int refuseMark = amp.commands.size();
    QVERIFY(cw.client.requestPgxlNetwork(false, QStringLiteral("999.1.1.1"), QString(),
                                         QString()).sent);
    NEREUS_TRY_COMPARE(refused.count(), 1);
    QCOMPARE(refused.first().first().toString(), QStringLiteral("pgxl"));
    QCOMPARE(sliceToast.count(), 0);
    QCOMPARE(page.deviceAnswerForTesting(),
             QStringLiteral("Enter each address as four numbers from 0 to 255 separated by dots."));
    QVERIFY(OperatorWording::isPlain(page.deviceAnswerForTesting()));
    QCOMPARE(page.ipEditForTesting()->text(), QStringLiteral("10.0.0.5"));
    QCOMPARE(amp.settingsCommands(refuseMark), QStringList{});

    // ---- Pairing settings are the station's: the window writes them.
    page.pairAttemptCheckForTesting()->setChecked(false);
    QCOMPARE(AppSettings::instance().value(QStringLiteral("PGXL_PairAttempt")).toString(),
             QStringLiteral("False"));

    // Nothing operated the amp; the window opened no connection to it.
    for (const QString& command : amp.commands) {
        QVERIFY2(!command.contains(QStringLiteral("operate")), qPrintable(command));
    }
    QVERIFY(!station.amplifierModel()->operate());
    QCOMPARE(window.pgxlConnection()->socketAttemptToken(), quint64(0));
    local.pgxlConnection()->disconnect();
    stationEnd->closeLink(QStringLiteral("test done"));
    AppSettings::instance().clear();
}

// R-R3-47 / R-R3-22: every control on a remote window's Tuner Genius
// Advanced page works through the Core, as for the Power Genius.
void RemotePeripheralsTest::remoteWindowChangesTheTunersOwnSettingsThroughTheCore()
{
    AppSettings::instance().clear();
    CoreAndWindow cw;
    RadioModel& station = cw.station;
    RadioModel& window = cw.window;
    station.smartSdrListener()->setListenEndpointForTesting(QHostAddress::LocalHost, 0);
    station.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("True"));
    FakeGenius tuner;
    QVERIFY(tuner.listen());
    TgxlAdvancedPage page(&window);
    bool yes = true;
    QStringList asked;
    page.setConfirmationForTesting([&](const QString& title, const QString& text) {
        asked.append(title + QLatin1Char('|') + text);
        return yes;
    });
    FakeGenius localTuner;
    QVERIFY(localTuner.listen());
    RadioModel local;
    TgxlAdvancedPage localPage(&local);
    QStringList localAsked;   // the local page asks the same question
    localPage.setConfirmationForTesting([&](const QString& title, const QString& text) {
        localAsked.append(title + QLatin1Char('|') + text);
        return true;
    });

    LoopbackTransport* stationEnd = cw.connect(this);
    NEREUS_TRY_VERIFY(cw.client.tgxlDeviceSettingsAvailable());
    window.reportStationLinkStateChanged();
    QVERIFY(page.nicknameEditForTesting()->isEnabled());
    QVERIFY(!page.applyNetworkButtonForTesting()->isEnabled());

    QVERIFY(admitCoreTuner(station, tuner));
    NEREUS_TRY_VERIFY(page.applyNetworkButtonForTesting()->isEnabled());
    NEREUS_TRY_COMPARE(page.firmwareTextForTesting(), QStringLiteral("1.2.17"));
    QCOMPARE(page.variantTextForTesting(), QStringLiteral("3x1"));
    local.tgxlConnection()->connectToTgxl(QStringLiteral("127.0.0.1"), localTuner.port());
    QVERIFY(localTuner.accept());
    localTuner.send(QStringLiteral("V1.2.17"));
    QVERIFY(localTuner.waitFor(QStringLiteral("ifconf read")) >= 0);

    // I5: DHCP off with no address is not sent from either page, and the
    // remote page asks nothing; both say why in the same words.
    {
        const int remoteBefore = tuner.commands.size();
        const int localBefore = localTuner.commands.size();
        const qsizetype askedBefore = asked.size();
        for (TgxlAdvancedPage* p : {&page, &localPage}) {
            p->dhcpCheckForTesting()->setChecked(false);
            p->ipEditForTesting()->clear();
            p->netmaskEditForTesting()->clear();
            p->gatewayEditForTesting()->clear();
            p->applyNetworkButtonForTesting()->click();
            QCOMPARE(p->networkProblemForTesting(),
                     QStringLiteral("Without DHCP, enter an address and a netmask."));
            QVERIFY(OperatorWording::isPlain(p->networkProblemForTesting()));
            // Follow-up 7: the subnet's broadcast address is refused too.
            p->ipEditForTesting()->setText(QStringLiteral("192.168.1.255"));
            p->netmaskEditForTesting()->setText(QStringLiteral("255.255.255.0"));
            p->applyNetworkButtonForTesting()->click();
            QCOMPARE(p->networkProblemForTesting(),
                     QStringLiteral("Enter an address the device can use on your network."));
            p->ipEditForTesting()->clear();
            p->netmaskEditForTesting()->clear();
        }
        QCOMPARE(asked.size(), askedBefore);
        NereusSDR::Test::settleSession();
        for (int i = remoteBefore; i < tuner.commands.size(); ++i) {
            QVERIFY2(!tuner.commands.at(i).startsWith(QStringLiteral("ifconf address")),
                     qPrintable(tuner.commands.at(i)));
        }
        for (int i = localBefore; i < localTuner.commands.size(); ++i) {
            QVERIFY2(!localTuner.commands.at(i).startsWith(QStringLiteral("ifconf address")),
                     qPrintable(localTuner.commands.at(i)));
        }
    }

    const int remoteMark = tuner.commands.size();
    const int localMark = localTuner.commands.size();
    const auto drive = [](TgxlAdvancedPage& p) {
        editName(p.nicknameEditForTesting(), QStringLiteral("Shack_Tuner"));
        p.dhcpCheckForTesting()->setChecked(true);
        p.applyNetworkButtonForTesting()->click();
        QVERIFY(p.saveAndRebootButtonForTesting()->isEnabled());
        acceptNextModal();
        p.saveAndRebootButtonForTesting()->click();
        p.revertButtonForTesting()->click();
    };
    drive(page);
    drive(localPage);
    QVERIFY(tuner.waitFor(QStringLiteral("ifconf read"), remoteMark) >= 0);
    QVERIFY(localTuner.waitFor(QStringLiteral("ifconf read"), localMark) >= 0);
    const QStringList expected{
        QStringLiteral("setup nickname=Shack_Tuner"),
        QStringLiteral("ifconf address= netmask= gateway= dhcp=true"),
        QStringLiteral("save"),
        QStringLiteral("setup read"),
        QStringLiteral("ifconf read"),
    };
    QCOMPARE(localTuner.settingsCommands(localMark), expected);
    QCOMPARE(tuner.settingsCommands(remoteMark), expected);
    QCOMPARE(asked, (QStringList{
        QStringLiteral("Apply Network Settings|") + TgxlAdvancedPage::networkQuestionText(),
        QStringLiteral("Save & Reboot TGXL|") + PgxlSaveRebootDialog::message()}));

    // A no sends nothing.
    yes = false;
    const int declineMark = tuner.commands.size();
    page.applyNetworkButtonForTesting()->click();
    NereusSDR::Test::settleSession();
    QCOMPARE(tuner.settingsCommands(declineMark), QStringList{});
    yes = true;

    // The tuner's values and answers.
    const int readAt = tuner.waitFor(QStringLiteral("setup read"), remoteMark);
    const int ifconfAt = tuner.waitFor(QStringLiteral("ifconf read"), remoteMark);
    // M9: `nickname=` as TgxlAdvancedPage::onSetupResponse reads it, the
    // value the captured `info` reply carries (captures/flex-tgxl-direct-
    // NOTES.md); `dhcp=`/`ip=` as TgxlAdvancedPage::onIfconfResponse reads
    // them. The tuner's `setup read` and `ifconf read` replies were never
    // captured (the capture shows the `ifconf read` request only):
    // unobserved, pending hardware.
    tuner.reply(readAt, QStringLiteral("0|nickname=Tuner_Genius_XL"));
    tuner.reply(ifconfAt, QStringLiteral("0|dhcp=1 ip=192.168.1.60 netmask=255.255.255.0 "
                                         "gateway=192.168.1.1"));
    NEREUS_TRY_COMPARE(page.ipEditForTesting()->text(), QStringLiteral("192.168.1.60"));
    QCOMPARE(page.nicknameEditForTesting()->text(), QStringLiteral("Tuner_Genius_XL"));
    QVERIFY(page.dhcpCheckForTesting()->isChecked());
    QVERIFY(!page.ipEditForTesting()->isEnabled());   // DHCP gates the fields
    const int saveAt = tuner.waitFor(QStringLiteral("save"), remoteMark);
    tuner.reply(saveAt, QStringLiteral("0|saving"));
    NEREUS_TRY_COMPARE(page.deviceAnswerForTesting(),
                 QStringLiteral("The Tuner Genius is saving its settings and restarting."));
    QVERIFY(OperatorWording::isPlain(page.deviceAnswerForTesting()));

    // A Core refusal on its own route.
    QSignalSpy refused(&window, &RadioModel::accessoryRequestRefused);
    QVERIFY(cw.client.requestTgxlName(QStringLiteral("Bad\tname")).sent);
    NEREUS_TRY_COMPARE(refused.count(), 1);
    QCOMPARE(refused.first().first().toString(), QStringLiteral("tgxl"));
    QCOMPARE(page.deviceAnswerForTesting(),
             QStringLiteral("Enter a name without line breaks or tabs."));

    // The Core loses the tuner: Apply and Revert wait for it again.
    QString reason;
    QVERIFY(station.disconnectTgxlForStation(&reason));
    NEREUS_TRY_VERIFY(!page.applyNetworkButtonForTesting()->isEnabled());
    QVERIFY(!page.revertButtonForTesting()->isEnabled());

    for (const QString& command : tuner.commands) {
        QVERIFY2(!command.contains(QStringLiteral("operate"))
                     && !command.contains(QStringLiteral("bypass")), qPrintable(command));
    }
    QCOMPARE(window.tgxlConnection()->socketAttemptToken(), quint64(0));
    local.tgxlConnection()->disconnect();
    stationEnd->closeLink(QStringLiteral("test done"));
    AppSettings::instance().clear();
}

// R-R3-47 / R-R3-22: a Core that does not offer the devices' own settings
// (an older Core) leaves those controls unchangeable, saying why in plain
// words; the station settings (pairing) still reach it.
void RemotePeripheralsTest::olderCoreLeavesTheDeviceSettingsSayingWhy()
{
    RadioModel model(RadioModel::Role::Remote);
    RecordingTgxlLink link;
    link.linkReady = true;
    model.attachStation(&link);
    PgxlAdvancedPage pgxl(&model);
    QVERIFY(!pgxl.nicknameEditForTesting()->isEnabled());
    QVERIFY(!pgxl.biasClassAForTesting()->isEnabled());
    QVERIFY(!pgxl.fanModeComboForTesting()->isEnabled());
    QVERIFY(!pgxl.ledSliderForTesting()->isEnabled());
    QVERIFY(!pgxl.dhcpCheckForTesting()->isEnabled());
    QVERIFY(!pgxl.ipEditForTesting()->isEnabled());
    QVERIFY(!pgxl.applyNetworkButtonForTesting()->isEnabled());
    QVERIFY(!pgxl.revertButtonForTesting()->isEnabled());
    QVERIFY(!pgxl.saveAndRebootButtonForTesting()->isEnabled());
    QVERIFY(pgxl.pairAttemptCheckForTesting()->isEnabled());
    QCOMPARE(pgxl.deviceAnswerForTesting(), IStationLink::pgxlDeviceSettingsUnavailableReason());
    QVERIFY(OperatorWording::isPlain(pgxl.deviceAnswerForTesting()));
    TgxlAdvancedPage tgxl(&model);
    QVERIFY(!tgxl.nicknameEditForTesting()->isEnabled());
    QVERIFY(!tgxl.dhcpCheckForTesting()->isEnabled());
    QVERIFY(!tgxl.applyNetworkButtonForTesting()->isEnabled());
    QVERIFY(!tgxl.revertButtonForTesting()->isEnabled());
    QCOMPARE(tgxl.deviceAnswerForTesting(), IStationLink::tgxlDeviceSettingsUnavailableReason());
    QVERIFY(OperatorWording::isPlain(tgxl.deviceAnswerForTesting()));
    // A request is not sent: the link says why.
    const IStationLink::CommandOutcome outcome = link.requestPgxlSaveAndRestart();
    QVERIFY(!outcome.sent);
    QVERIFY(OperatorWording::isPlain(outcome.reason));
}

// L1 (R-R3-47, R-R3-22, R-R3-48): the Core's refusals of the accessory
// requests reach the window as accessory refusals, keyed by what they were
// about, and never as a slice refusal.
void RemotePeripheralsTest::accessoryRefusalsNeverReachTheSliceToast()
{
    AppSettings::instance().clear();
    CoreAndWindow cw;
    LoopbackTransport* stationEnd = cw.connect(this);
    NEREUS_TRY_VERIFY(cw.client.accessoryDataAvailable());
    NEREUS_TRY_VERIFY(cw.client.pgxlDeviceSettingsAvailable());
    QSignalSpy sliceToast(&cw.window, &RadioModel::sliceAddRejected);
    QSignalSpy refused(&cw.window, &RadioModel::accessoryRequestRefused);

    struct Case { const char* device; std::function<IStationLink::CommandOutcome()> send; };
    StationClient& c = cw.client;
    const QList<Case> cases{
        {"pgxl", [&] { return c.requestPgxlPowerCap(true, 5); }},
        {"pgxl", [&] { return c.requestPgxlName(QStringLiteral("Amp")); }},   // no amp on the Core
        {"tgxl", [&] { return c.requestTgxlSaveAndRestart(); }},                 // no tuner
        {"interlock", [&] { return c.requestTxInterlockPolicy(9, 0, false, 2.0); }},
        {"faults", [&] { return c.requestClearAccessoryFaults(QStringLiteral("amp")); }},
    };
    for (const Case& one : cases) {
        refused.clear();
        QVERIFY(one.send().sent);
        NEREUS_TRY_COMPARE(refused.count(), 1);
        QCOMPARE(refused.first().at(0).toString(), QString::fromLatin1(one.device));
        // Follow-up 3: sent by no page, so MainWindow toasts it.
        QCOMPARE(refused.first().size(), 3);
        QVERIFY(!refused.first().at(2).toBool());
        QVERIFY(OperatorWording::isPlain(
            OperatorReasonText::forDisplay(refused.first().at(1).toString())));
    }
    NereusSDR::Test::settleSession();
    QCOMPARE(sliceToast.count(), 0);
    stationEnd->closeLink(QStringLiteral("test done"));
    AppSettings::instance().clear();
}

// I4 (R-R3-47): every control on a remote window's RF-Kit page works
// through the Core. Auto-reconnect, poll interval and the antenna names
// are the station's settings (the Core applies them at once); Reset amp
// error reaches the Core's amp as the request the local page's button
// sends to its own amp (the fakes record both).
void RemotePeripheralsTest::remoteRfKitPageWorksEveryControl()
{
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("RfKit_PollIntervalMs"),
                                     QStringLiteral("5000"));
    CoreAndWindow cw;
    RadioModel& station = cw.station;
    RadioModel& window = cw.window;
    FakeRfKit amp;
    RfKitPage page(&window);
    LoopbackTransport* stationEnd = cw.connect(this);
    NEREUS_TRY_VERIFY(cw.client.rfKitSettingsAvailable());
    window.reportStationLinkStateChanged();
    QString reason;
    QVERIFY(station.setRfKitEnabledForStation(true, &reason));
    NEREUS_TRY_VERIFY(page.detailTabIsEnabledForTesting());
    for (QWidget* w : std::initializer_list<QWidget*>{
             page.autoReconnectForTesting(), page.pollIntervalForTesting(),
             page.saveButtonForTesting(), page.resetErrorButtonForTesting(),
             page.antennaLabelEditForTesting(1), page.antennaLabelEditForTesting(4)}) {
        QVERIFY(w->isEnabled());
    }
    QCOMPARE(page.pollIntervalForTesting()->value(), 5000);

    // Reset amp error with no amp at the Core: the Core's words, nothing sent.
    QSignalSpy sliceToast(&window, &RadioModel::sliceAddRejected);
    QSignalSpy accessoryRefused(&window, &RadioModel::accessoryRequestRefused);
    // Rework part 5: Setup closed (the page hidden) when the answer comes:
    // the refusal is toasted, not lost.
    page.hide();
    page.resetErrorButtonForTesting()->click();
    NEREUS_TRY_COMPARE(accessoryRefused.count(), 1);
    QCOMPARE(accessoryRefused.first().size(), 3);
    QVERIFY(!accessoryRefused.first().at(2).toBool());
    // A page gone before the answer: toasted too.
    {
        auto gone = std::make_unique<RfKitPage>(&window);
        gone->show();
        gone->resetErrorButtonForTesting()->click();
    }
    NEREUS_TRY_COMPARE(accessoryRefused.count(), 2);
    QVERIFY(!accessoryRefused.at(1).at(2).toBool());
    // Follow-up 3: the visible page that sent it shows it, so it is not
    // toasted too.
    page.show();
    page.resetErrorButtonForTesting()->click();
    NEREUS_TRY_COMPARE(accessoryRefused.count(), 3);
    QVERIFY(accessoryRefused.at(2).at(2).toBool());
    NEREUS_TRY_VERIFY(page.liveStatusTextForTesting().contains(
        QStringLiteral("The Core is not connected to the RF-Kit amplifier.")));
    QVERIFY(OperatorWording::isPlain(page.liveStatusTextForTesting()));
    QCOMPARE(sliceToast.count(), 0);

    // The Core admits its amp; the window's Reset reaches it.
    QVERIFY(station.configureRfKitForStation(QStringLiteral("127.0.0.1"), amp.serverPort(),
                                             &reason));
    NEREUS_TRY_COMPARE_WITH_TIMEOUT(window.rfKitModel()->connectionPhase(),
                              RfKitModel::ConnectionPhase::Connected, 5000);
    const int remoteMark = amp.lines.size();
    page.resetErrorButtonForTesting()->click();
    NEREUS_TRY_VERIFY(amp.lines.mid(remoteMark).contains(QStringLiteral("POST /error/reset")));

    // A local window's page on its own amp: the same request.
    FakeRfKit localAmp;
    RadioModel local;
    RadioInfo radio;
    radio.macAddress = QStringLiteral("aa:bb:cc:dd:ee:48");
    local.setLastRadioInfoForTest(radio);
    local.setConnectionStateForTest(ConnectionState::Connected);
    local.setRfKitEnabled(true);
    RfKitPage localPage(&local);
    QVERIFY(localPage.detailTabIsEnabledForTesting());
    local.rfKitConnection()->connectToAmp(QStringLiteral("127.0.0.1"), localAmp.serverPort());
    NEREUS_TRY_VERIFY_WITH_TIMEOUT(local.rfKitConnection()->isConnected(), 5000);
    const int localMark = localAmp.lines.size();
    localPage.resetErrorButtonForTesting()->click();
    NEREUS_TRY_VERIFY(localAmp.lines.mid(localMark).contains(QStringLiteral("POST /error/reset")));
    local.rfKitConnection()->disconnect();

    // Save: the station's settings, sent over the link through the
    // window's settings proxy (follow-up 5: nothing here writes the Core's
    // store or calls its apply by hand), then applied by the Core at once.
    NEREUS_TRY_VERIFY(cw.proxy.ready());
    AppSettings::instance().setRemoteBackend(&cw.proxy);
    const auto restoreBackend = qScopeGuard([] {
        AppSettings::instance().setRemoteBackend(nullptr);
    });
    page.autoReconnectForTesting()->setChecked(false);
    page.pollIntervalForTesting()->setValue(2500);
    page.antennaLabelEditForTesting(1)->setText(QStringLiteral("Beam"));
    page.antennaLabelEditForTesting(4)->setText(QStringLiteral("Loop"));
    page.saveButtonForTesting()->click();
    NEREUS_TRY_COMPARE(cw.stationSettings.value(QStringLiteral("RfKit_PollIntervalMs")).toString(),
                 QStringLiteral("2500"));
    QCOMPARE(cw.stationSettings.value(QStringLiteral("RfKit_AutoReconnect")).toString(),
             QStringLiteral("False"));
    QCOMPARE(cw.stationSettings.value(QStringLiteral("RfKit_Ant1_Label")).toString(),
             QStringLiteral("Beam"));
    NEREUS_TRY_COMPARE(station.rfKitConnection()->pollIntervalMs(), 2500);
    QVERIFY(!station.rfKitConnection()->autoReconnect());
    NEREUS_TRY_COMPARE(window.accessoryDataModel()->rfkitAntennaLabels().value(0),
                 QStringLiteral("Beam"));
    QCOMPARE(window.accessoryDataModel()->rfkitAntennaLabels().value(3), QStringLiteral("Loop"));

    // Follow-up 6: another window changes them on the Core: this page
    // follows.
    cw.stationSettings.setValue(QStringLiteral("RfKit_PollIntervalMs"), QStringLiteral("3000"));
    cw.stationSettings.setValue(QStringLiteral("RfKit_AutoReconnect"), QStringLiteral("True"));
    NEREUS_TRY_COMPARE(page.pollIntervalForTesting()->value(), 3000);
    NEREUS_TRY_VERIFY(page.autoReconnectForTesting()->isChecked());

    // Rework part 6: an unsaved edit is not overwritten by the Core's
    // settings; untouched fields still follow.
    page.pollIntervalForTesting()->setValue(1234);
    page.antennaLabelEditForTesting(2)->setText(QString());
    QTest::keyClicks(page.antennaLabelEditForTesting(2), QStringLiteral("Wire"));
    cw.stationSettings.setValue(QStringLiteral("RfKit_AutoReconnect"), QStringLiteral("False"));
    cw.stationSettings.setValue(QStringLiteral("RfKit_PollIntervalMs"), QStringLiteral("4000"));
    cw.stationSettings.setValue(QStringLiteral("RfKit_Ant2_Label"), QStringLiteral("Core"));
    NEREUS_TRY_VERIFY(!page.autoReconnectForTesting()->isChecked());
    NereusSDR::Test::settleSession();
    QCOMPARE(page.pollIntervalForTesting()->value(), 1234);
    QCOMPARE(page.antennaLabelEditForTesting(2)->text(), QStringLiteral("Wire"));

    // Rework follow-up 3: a Save whose writes never reach the Core (the
    // settings link not ready: they are dropped) keeps the marks, so the
    // Core's settings arriving later do not overwrite the operator's
    // values; a Save the Core takes (its settings echo) clears them.
    cw.proxy.setReady(false);
    page.saveButtonForTesting()->click();   // poll 1234, ANT 2 "Wire": dropped
    NereusSDR::Test::settleSession();
    QCOMPARE(cw.stationSettings.value(QStringLiteral("RfKit_PollIntervalMs")).toString(),
             QStringLiteral("4000"));
    cw.proxy.setReady(true);
    cw.stationSettings.setValue(QStringLiteral("RfKit_PollIntervalMs"), QStringLiteral("4500"));
    NereusSDR::Test::settleSession();
    QCOMPARE(page.pollIntervalForTesting()->value(), 1234);
    page.saveButtonForTesting()->click();   // taken this time
    NEREUS_TRY_COMPARE(cw.stationSettings.value(QStringLiteral("RfKit_PollIntervalMs")).toString(),
                 QStringLiteral("1234"));
    cw.stationSettings.setValue(QStringLiteral("RfKit_PollIntervalMs"), QStringLiteral("4600"));
    NEREUS_TRY_COMPARE(page.pollIntervalForTesting()->value(), 4600);

    // Nothing but reads and the reset reached the Core's amp; the window
    // opened no connection of its own.
    for (const QString& line : amp.lines) {
        QVERIFY2(line.startsWith(QStringLiteral("GET "))
                     || line == QStringLiteral("POST /error/reset"), qPrintable(line));
    }
    QVERIFY(!window.rfKitConnection()->isConnected());
    QVERIFY(window.rfKitConnection()->peerAddress().isEmpty());
    stationEnd->closeLink(QStringLiteral("test done"));
    AppSettings::instance().clear();
}

// I4: a Core without remoteRfKitControlVersion 3 leaves the page's settings,
// names and Reset amp error unchangeable, and says why in plain words.
void RemotePeripheralsTest::olderCoreLeavesTheRfKitSettingsSayingWhy()
{
    AppSettings::instance().clear();
    struct OlderCore final : IStationLink {
        CommandOutcome requestAddSlice(const QString&) override { return {}; }
        CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
        CommandOutcome requestRemoveSlice(int) override { return {}; }
        CommandOutcome requestActiveSlice(int) override { return {}; }
        CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
        bool stationLinkReady() const override { return true; }
        bool remoteRfKitControlAvailable() const override { return true; }
    } link;
    RadioModel model(RadioModel::Role::Remote);
    model.attachStation(&link);
    RfKitPage page(&model);
    model.reportStationLinkStateChanged();
    for (QWidget* w : std::initializer_list<QWidget*>{
             page.autoReconnectForTesting(), page.pollIntervalForTesting(),
             page.saveButtonForTesting(), page.resetErrorButtonForTesting(),
             page.antennaLabelEditForTesting(2)}) {
        QVERIFY(!w->isEnabled());
        QVERIFY(OperatorWording::isPlain(w->toolTip()));
    }
    const IStationLink::CommandOutcome outcome = link.requestResetRfKitError();
    QVERIFY(!outcome.sent);
    QVERIFY(OperatorWording::isPlain(outcome.reason));
    AppSettings::instance().clear();
}

// Rework part 5 (R-R3-47): a page's claim on a request's refusal ends when
// the link to the Core drops; a refusal arriving later is toasted.
void RemotePeripheralsTest::refusalClaimsEndWithTheLink()
{
    struct Link final : IStationLink {
        bool ready{true};
        CommandOutcome requestAddSlice(const QString&) override { return {}; }
        CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
        CommandOutcome requestRemoveSlice(int) override { return {}; }
        CommandOutcome requestActiveSlice(int) override { return {}; }
        CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
        bool stationLinkReady() const override { return ready; }
    } link;
    RadioModel window(RadioModel::Role::Remote);
    window.attachStation(&link);
    RfKitPage page(&window);
    page.show();
    QSignalSpy refused(&window, &RadioModel::accessoryRequestRefused);
    window.noteAccessoryRequestShownOnPage(7, &page);
    window.reportStationAccessoryRefusal(QStringLiteral("rfkit"), QStringLiteral("No."), 7);
    QVERIFY(refused.last().at(2).toBool());      // claimed and visible
    window.noteAccessoryRequestShownOnPage(8, &page);
    link.ready = false;
    window.reportStationLinkStateChanged();
    window.reportStationAccessoryRefusal(QStringLiteral("rfkit"), QStringLiteral("No."), 8);
    QVERIFY(!refused.last().at(2).toBool());     // the claim ended with the link
}

// R-R3-21 / R-R3-23: an amp applet's own Connect refused by the Core
// shows on the applet's line, so it is not toasted too; the same refusal
// is toasted when the applet is not on screen to show it.
void RemotePeripheralsTest::ampAppletRefusalShownOnTheAppletIsNotToasted()
{
    RadioModel model(RadioModel::Role::Remote);
    RecordingTgxlLink link;
    link.linkReady = true;
    link.amplifierStatus = true;
    link.rfKitStatus = true;
    link.pgxlAvailable = true;
    link.rfKitAvailable = true;
    model.attachStation(&link);
    AmpApplet amp(&model);
    Rf2ksApplet rfKit(&model);
    model.amplifierModel()->setStationConnectionState(
        state(TunerModel::ConnectionPhase::Disconnected, QStringLiteral("192.0.2.40"), 9008));
    model.rfKitModel()->setStationConnectionState(
        state(TunerModel::ConnectionPhase::Disconnected, QStringLiteral("192.0.2.41"), 8080));
    model.reportStationLinkStateChanged();
    QSignalSpy refused(&model, &RadioModel::accessoryRequestRefused);
    const QString pgxlRefusal = QStringLiteral("Enable 4O3A on Core before connecting the PGXL.");
    const QString rfKitRefusal =
        QStringLiteral("Turn on the RF-Kit amplifier on the Core before connecting it.");
    const auto sendBoth = [&] {
        std::unique_ptr<QMenu> ampMenu(amp.buildContextMenuForTesting());
        std::unique_ptr<QMenu> rfKitMenu(rfKit.buildContextMenuForTesting());
        QAction* ampToggle = connectionToggle(ampMenu.get());
        QAction* rfKitToggle = connectionToggle(rfKitMenu.get());
        QVERIFY(ampToggle && rfKitToggle);
        ampToggle->trigger();
        rfKitToggle->trigger();
    };
    const auto refuseBoth = [&] {
        model.reportStationAccessoryRefusal(QStringLiteral("pgxl"), pgxlRefusal,
                                            link.lastPgxlCommandId);
        model.reportStationCommandFinished(link.lastPgxlCommandId, false, pgxlRefusal);
        model.reportStationAccessoryRefusal(QStringLiteral("rfkit"), rfKitRefusal,
                                            link.lastRfKitCommandId);
        model.reportStationCommandFinished(link.lastRfKitCommandId, false, rfKitRefusal);
    };

    // On screen: the applets show the refusals, so neither is toasted.
    amp.show();
    rfKit.show();
    sendBoth();
    QCOMPARE(link.pgxlConfigureCalls, 1);
    QCOMPARE(link.rfKitConfigureCalls, 1);
    refuseBoth();
    QCOMPARE(refused.count(), 2);
    QVERIFY(refused.at(0).at(2).toBool());
    QVERIFY(refused.at(1).at(2).toBool());
    QCOMPARE(amp.connectionLineTextForTesting(), OperatorReasonText::forDisplay(pgxlRefusal));
    QCOMPARE(rfKit.connectionLineTextForTesting(), OperatorReasonText::forDisplay(rfKitRefusal));

    // Hidden before the answer came: nobody saw the line, so it is toasted.
    sendBoth();
    amp.hide();
    rfKit.hide();
    refuseBoth();
    QCOMPARE(refused.count(), 4);
    QVERIFY(!refused.at(2).at(2).toBool());
    QVERIFY(!refused.at(3).at(2).toBool());
}

// Rework follow-up 5 (R-R3-48): typing a TCI port sends it (to this
// window's server and the Core's) once, when editing finishes, not for
// every keystroke.
void RemotePeripheralsTest::tciPortIsSentWhenEditingFinishes()
{
    AppSettings::instance().clear();
    CatTciServerPage page;
    page.show();
    QSpinBox* spin = page.portSpinForTesting();
    QVERIFY(spin);
    QSignalSpy sent(&page, &CatTciServerPage::tciServerBindOrPortChanged);
    spin->setFocus();
    spin->selectAll();
    QTest::keyClicks(spin, QStringLiteral("50002"));
    QCOMPARE(sent.count(), 0);
    QTest::keyClick(spin, Qt::Key_Return);
    NEREUS_TRY_COMPARE(sent.count(), 1);
    QCOMPARE(sent.first().at(1).toUInt(), 50002u);
    AppSettings::instance().clear();
}

// Operator decision 2026-09-24 (R-R3-47, R-R3-22): a local window's Power
// Genius and Tuner Genius pages ask the same plain question before
// applying network settings as the remote pages; a no sends nothing, a yes
// sends the same ifconf line as before.
void RemotePeripheralsTest::localPagesAskBeforeNetworkSettings()
{
    AppSettings::instance().clear();
    const QString ifconf = QStringLiteral("ifconf address=192.168.1.50 netmask=255.255.255.0 "
                                          "gateway=192.168.1.1 dhcp=false");
    const auto fill = [](auto& page) {
        page.dhcpCheckForTesting()->setChecked(false);
        page.ipEditForTesting()->setText(QStringLiteral("192.168.1.50"));
        page.netmaskEditForTesting()->setText(QStringLiteral("255.255.255.0"));
        page.gatewayEditForTesting()->setText(QStringLiteral("192.168.1.1"));
    };
    {
        FakeGenius amp;
        QVERIFY(amp.listen());
        RadioModel local;
        PgxlAdvancedPage page(&local);
        bool yes = false;
        QStringList asked;
        page.setConfirmationForTesting([&](const QString& title, const QString& text) {
            asked.append(title + QLatin1Char('|') + text);
            return yes;
        });
        local.pgxlConnection()->connectToPgxl(QStringLiteral("127.0.0.1"), amp.port());
        QVERIFY(amp.accept());
        amp.send(QStringLiteral("V3.8.9"));
        QVERIFY(amp.waitFor(QStringLiteral("ifconf read")) >= 0);
        fill(page);
        const int mark = amp.commands.size();
        page.applyNetworkButtonForTesting()->click();
        NereusSDR::Test::settleSession();
        QCOMPARE(amp.settingsCommands(mark), QStringList{});
        QCOMPARE(asked, QStringList{QStringLiteral("Apply Network Settings|")
                                    + PgxlAdvancedPage::networkQuestionText()});
        yes = true;
        page.applyNetworkButtonForTesting()->click();
        QVERIFY(amp.waitFor(ifconf, mark) >= 0);
    }
    {
        FakeGenius tuner;
        QVERIFY(tuner.listen());
        RadioModel local;
        TgxlAdvancedPage page(&local);
        bool yes = false;
        QStringList asked;
        page.setConfirmationForTesting([&](const QString& title, const QString& text) {
            asked.append(title + QLatin1Char('|') + text);
            return yes;
        });
        local.tgxlConnection()->connectToTgxl(QStringLiteral("127.0.0.1"), tuner.port());
        QVERIFY(tuner.accept());
        tuner.send(QStringLiteral("V1.2.17"));
        QVERIFY(tuner.waitFor(QStringLiteral("ifconf read")) >= 0);
        fill(page);
        const int mark = tuner.commands.size();
        page.applyNetworkButtonForTesting()->click();
        NereusSDR::Test::settleSession();
        QCOMPARE(tuner.settingsCommands(mark), QStringList{});
        QCOMPARE(asked, QStringList{QStringLiteral("Apply Network Settings|")
                                    + TgxlAdvancedPage::networkQuestionText()});
        yes = true;
        page.applyNetworkButtonForTesting()->click();
        QVERIFY(tuner.waitFor(ifconf, mark) >= 0);
    }
    AppSettings::instance().clear();
}

// R-R3-49 / R-R3-47 (remoteTgxlControlVersion 2): in a remote window the
// Tuner Genius applet's ANT 1/2/3 and OPERATE switch the Core's tuner
// through the Core (the fake tuner records the local applet's own lines),
// from a paired window allowed to transmit; the buttons follow the tuner's report, not
// the click; while the radio is on the air they are disabled with the
// reason, and a request sent anyway is refused with it and reaches nothing.
// TUNE is kept outside this accessory-control case.
void RemotePeripheralsTest::remoteWindowSwitchesTheTunerThroughTheCore()
{
    AppSettings::instance().clear();
    CoreAndWindow cw;
    QVERIFY(cw.pairTransmitWindow());
    RadioModel& station = cw.station;
    RadioModel& window = cw.window;
    station.smartSdrListener()->setListenEndpointForTesting(QHostAddress::LocalHost, 0);
    station.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("True"));
    FakeGenius tuner;
    QVERIFY(tuner.listen());
    TunerApplet applet(&window, window.tunerModel());
    const QString transmitReason =
        QStringLiteral("Remote transmit controls are not available from this Core.");
    applet.setTransmitPermitted(false, transmitReason);   // do not key in this fixture
    QSignalSpy refused(&window, &RadioModel::accessoryRequestRefused);

    LoopbackTransport* stationEnd = cw.connect(this);
    NEREUS_TRY_VERIFY(cw.client.tgxlControlAvailable());
    window.reportStationLinkStateChanged();
    QVERIFY(admitCoreTuner(station, tuner));
    NEREUS_TRY_VERIFY(cw.client.capabilities().txPermitted);
    QVERIFY(!station.receiveOnlyStationPolicy());
    tuner.send(QStringLiteral("S0|state one_by_three=1 antA=1 operate=0 bypass=0"));
    NEREUS_TRY_VERIFY(window.tunerModel()->hasAntennaSwitch());
    NEREUS_TRY_COMPARE(window.tunerModel()->antennaA(), 1);

    for (int port = 1; port <= 3; ++port) {
        QVERIFY(applet.antennaButtonForTesting(port)->isEnabled());
        QVERIFY(applet.antennaButtonForTesting(port)->toolTip().isEmpty());
    }
    QVERIFY(applet.operateButtonForTesting()->isEnabled());
    QVERIFY(!applet.tuneButtonForTesting()->isEnabled());
    QCOMPARE(applet.tuneButtonForTesting()->toolTip(), transmitReason);

    // ANT 3: the Core's tuner gets the local applet's line; the window's
    // antenna moves only when the tuner reports it.
    const int mark = tuner.commands.size();
    applet.antennaButtonForTesting(3)->click();
    QVERIFY(tuner.waitFor(QStringLiteral("activate ant=3"), mark) >= 0);
    QCOMPARE(window.tunerModel()->antennaA(), 1);
    tuner.send(QStringLiteral("S0|state antA=3"));
    NEREUS_TRY_COMPARE(window.tunerModel()->antennaA(), 3);

    // OPERATE from STANDBY: bypass off, then operate on, as a local click.
    // This Core (remoteTgxlControlVersion 3) takes it as one setTgxlOperate
    // and sends both lines itself, so bypass=0 reaches the tuner once.
    QVERIFY(cw.client.tgxlOperateAppliesWhole());
    const int operateMark = tuner.commands.size();
    QCOMPARE(applet.operateButtonForTesting()->text(), QStringLiteral("STANDBY"));
    applet.operateButtonForTesting()->click();
    const int bypassOff = tuner.waitFor(QStringLiteral("bypass=0"), operateMark);
    const int operateOn = tuner.waitFor(QStringLiteral("operate=1"), operateMark);
    QVERIFY(bypassOff >= 0 && operateOn > bypassOff);
    NereusSDR::Test::settleSession();
    QCOMPARE(tuner.commands.mid(operateMark).count(QStringLiteral("bypass=0")), 1);
    QCOMPARE(applet.operateButtonForTesting()->text(), QStringLiteral("STANDBY"));
    tuner.send(QStringLiteral("S0|state operate=1 bypass=0"));
    NEREUS_TRY_COMPARE(applet.operateButtonForTesting()->text(), QStringLiteral("OPERATE"));
    // OPERATE to BYPASS.
    const int bypassMark = tuner.commands.size();
    applet.operateButtonForTesting()->click();
    QVERIFY(tuner.waitFor(QStringLiteral("bypass=1"), bypassMark) >= 0);
    QVERIFY(refused.isEmpty());

    // On the air: disabled, with the reason; a request sent anyway is
    // refused with it and nothing reaches the tuner. The Core keys through
    // its MoxController, as its MOX button and a hardware PTT do: first a
    // MOX click, then the radio's own PTT input.
    MoxController* const mox = station.moxController();
    QVERIFY(mox);
    // Test-only logical keying: there is no connected radio or RF path.
    QVERIFY(!mox->isMox());
    mox->setMoxCheck({});
    const auto expectOnAir = [&] {
        for (int port = 1; port <= 3; ++port) {
            NEREUS_TRY_VERIFY(!applet.antennaButtonForTesting(port)->isEnabled());
            QCOMPARE(applet.antennaButtonForTesting(port)->toolTip(), TunerApplet::onAirReason());
        }
        QVERIFY(!applet.operateButtonForTesting()->isEnabled());
        QCOMPARE(applet.operateButtonForTesting()->toolTip(), TunerApplet::onAirReason());
    };
    const auto expectOffAir = [&] {
        NEREUS_TRY_VERIFY(applet.antennaButtonForTesting(2)->isEnabled());
        QVERIFY(applet.antennaButtonForTesting(2)->toolTip().isEmpty());
        QVERIFY(applet.operateButtonForTesting()->isEnabled());
        QVERIFY(!applet.tuneButtonForTesting()->isEnabled());
    };
    for (int keying = 0; keying < 2; ++keying) {
        if (keying == 0) { mox->setMox(true); } else { mox->onMicPttFromRadio(true); }
        QVERIFY(mox->isMox());
        // The Core never writes its transmit model's MOX latch from here.
        QVERIFY(!station.transmitModel().isMox());
        QVERIFY(station.isTransmitting());
        NEREUS_TRY_VERIFY(window.isTransmitting());
        expectOnAir();
        if (QTest::currentTestFailed()) { return; }
        QVERIFY(OperatorWording::isPlain(TunerApplet::onAirReason()));
        const int airMark = tuner.commands.size();
        const int refusedBefore = refused.count();
        QVERIFY(cw.client.requestTgxlAntenna(2).sent);
        NEREUS_TRY_COMPARE(refused.count(), refusedBefore + 1);
        QCOMPARE(refused.last().at(0).toString(), QStringLiteral("tgxl"));
        QCOMPARE(refused.last().at(1).toString(), TunerApplet::onAirReason());
        NereusSDR::Test::settleSession();
        for (int i = airMark; i < tuner.commands.size(); ++i) {
            QVERIFY2(!tuner.commands.at(i).startsWith(QStringLiteral("activate")),
                     qPrintable(tuner.commands.at(i)));
        }
        if (keying == 0) { mox->setMox(false); } else { mox->onMicPttFromRadio(false); }
        NEREUS_TRY_VERIFY(!station.isTransmitting());
        NEREUS_TRY_VERIFY(!window.isTransmitting());
        expectOffAir();
        if (QTest::currentTestFailed()) { return; }
    }

    // R-R3-49 (parity Task 1): the Core's TUNE and its two-tone test put
    // the radio on the air too; the window hears both (isCoreOnAir).
    station.transmitModel().setTune(true);
    NEREUS_TRY_VERIFY(window.isCoreOnAir());
    expectOnAir();
    if (QTest::currentTestFailed()) { return; }
    station.transmitModel().setTune(false);
    NEREUS_TRY_VERIFY(!window.isCoreOnAir());
    expectOffAir();
    if (QTest::currentTestFailed()) { return; }
    {
        TxChannel tx(/*channelId=*/1);
        TwoToneController* const twoTone = station.twoToneController();
        QVERIFY(twoTone);
        twoTone->setTxChannel(&tx);
        twoTone->setSettleDelaysMs(0, 0);
        twoTone->setActive(true);
        NEREUS_TRY_VERIFY(twoTone->isActive());
        NEREUS_TRY_VERIFY(window.pureSignalFacade()->twoToneOn());
        expectOnAir();
        if (QTest::currentTestFailed()) { return; }
        twoTone->setActive(false);
        NEREUS_TRY_VERIFY(!twoTone->isActive());
        NEREUS_TRY_VERIFY(mox->state() == MoxState::Rx);
        twoTone->setTxChannel(nullptr);
    }
    NEREUS_TRY_VERIFY(!window.isCoreOnAir());
    expectOffAir();
    if (QTest::currentTestFailed()) { return; }

    // The link gone: back to the transmit gate, with its reason.
    stationEnd->closeLink(QStringLiteral("test: Core lost"));
    NEREUS_TRY_VERIFY(!cw.client.tgxlControlAvailable());
    window.reportStationLinkStateChanged();
    NEREUS_TRY_VERIFY(!applet.antennaButtonForTesting(1)->isEnabled());
    QCOMPARE(applet.antennaButtonForTesting(1)->toolTip(), transmitReason);
}

// R-R3-49: a Core below remoteTgxlControlVersion 2 leaves ANT and OPERATE
// greyed with the transmit reason, and a click sends nothing.
void RemotePeripheralsTest::olderCoreLeavesTheTunerSwitchesGreyed()
{
    RadioModel model(RadioModel::Role::Remote);
    RecordingTgxlLink link;
    link.linkReady = true;
    model.attachStation(&link);
    TunerApplet applet(&model, model.tunerModel());
    const QString transmitReason =
        QStringLiteral("Remote transmit controls are not available from this Core.");
    applet.setTransmitPermitted(false, transmitReason);
    for (int port = 1; port <= 3; ++port) {
        QVERIFY(!applet.antennaButtonForTesting(port)->isEnabled());
        QCOMPARE(applet.antennaButtonForTesting(port)->toolTip(), transmitReason);
    }
    QVERIFY(!applet.operateButtonForTesting()->isEnabled());
    QCOMPARE(applet.operateButtonForTesting()->toolTip(), transmitReason);
    const IStationLink::CommandOutcome outcome = link.requestTgxlAntenna(2);
    QVERIFY(!outcome.sent);
    QCOMPARE(outcome.reason, IStationLink::tgxlControlUnavailableReason());
    QVERIFY(OperatorWording::isPlain(outcome.reason));
    model.detachStation();
}

// GUI-I4 (fix wave): a remote window that may transmit, on a Core that
// does not run the tuner for this app (no remoteTgxlControlVersion 2, no
// tx.tunerTune), leaves TUNE, ANT, OPERATE and the relay bars disabled with
// the reason. Before, they were live and acted on this computer's own Tuner
// Genius. A Core that switches it but does not move relays (version below
// 4) leaves only the relay bars disabled, with their own reason.
void RemotePeripheralsTest::aWindowThatMayTransmitNeverFallsBackToItsOwnTuner()
{
    RadioModel model(RadioModel::Role::Remote);
    RecordingTgxlLink link;
    link.linkReady = true;
    model.attachStation(&link);
    TunerApplet applet(&model, model.tunerModel());
    applet.setTransmitPermitted(true, QString());
    model.reportStationLinkStateChanged();

    QVERIFY(!applet.tuneButtonForTesting()->isEnabled());
    QCOMPARE(applet.tuneButtonForTesting()->toolTip(), TunerApplet::noRemoteTuneReason());
    for (int port = 1; port <= 3; ++port) {
        QVERIFY(!applet.antennaButtonForTesting(port)->isEnabled());
        QCOMPARE(applet.antennaButtonForTesting(port)->toolTip(),
                 TunerApplet::noRemoteTunerReason());
    }
    QVERIFY(!applet.operateButtonForTesting()->isEnabled());
    QCOMPARE(applet.operateButtonForTesting()->toolTip(), TunerApplet::noRemoteTunerReason());
    for (int relay = 0; relay < 3; ++relay) {
        QVERIFY(!applet.relayBarForTesting(relay)->isScrollEnabled());
        QCOMPARE(applet.relayBarForTesting(relay)->toolTip(), TunerApplet::noRemoteTunerReason());
    }
    for (const QString& reason : {TunerApplet::noRemoteTuneReason(),
                                  TunerApplet::noRemoteTunerReason(),
                                  TunerApplet::noRemoteRelayReason()}) {
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
    }

    // Switching through the Core, relays not: only the bars wait.
    link.tgxlControl = true;
    model.reportStationLinkStateChanged();
    applet.setTransmitPermitted(true, QString());
    QVERIFY(applet.antennaButtonForTesting(1)->isEnabled());
    QVERIFY(applet.operateButtonForTesting()->isEnabled());
    for (int relay = 0; relay < 3; ++relay) {
        QVERIFY(!applet.relayBarForTesting(relay)->isScrollEnabled());
        QCOMPARE(applet.relayBarForTesting(relay)->toolTip(), TunerApplet::noRemoteRelayReason());
    }
    QVERIFY(link.tgxlRequests.isEmpty());
    model.detachStation();
}

// Fix round 1 (minor 4): a name saved with spaces before names were one
// word is offered with underscores, so the box accepts it and an edit is
// saved. Before, the box held text its validator never accepted, so
// editingFinished never fired and edits went nowhere.
void RemotePeripheralsTest::aNameSavedWithSpacesStillTakesEdits()
{
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("PGXL_Nickname"), QStringLiteral("Shack PGXL"));
    AppSettings::instance().setValue(QStringLiteral("TGXL_Nickname"), QStringLiteral("Shack Tuner"));
    const auto cleanup = qScopeGuard([] { AppSettings::instance().clear(); });
    RadioModel local;
    PgxlAdvancedPage amp(&local);
    TgxlAdvancedPage tuner(&local);
    const auto check = [](QLineEdit* edit, const QString& offered, const QString& key) {
        QCOMPARE(edit->text(), offered);
        QVERIFY(edit->hasAcceptableInput());
        QSignalSpy finished(edit, &QLineEdit::editingFinished);
        QTest::keyClick(edit, Qt::Key_End);
        QTest::keyClicks(edit, QStringLiteral("2"));
        QTest::keyClick(edit, Qt::Key_Return);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(AppSettings::instance().value(key).toString(), offered + QStringLiteral("2"));
    };
    check(amp.nicknameEditForTesting(), QStringLiteral("Shack_PGXL"),
          QStringLiteral("PGXL_Nickname"));
    if (QTest::currentTestFailed()) { return; }
    check(tuner.nicknameEditForTesting(), QStringLiteral("Shack_Tuner"),
          QStringLiteral("TGXL_Nickname"));
}

// Fix round 1 (minor 1): in a remote window on a Core that runs TUNE for
// it, the Core's TUN on leaves TUNE disabled with the on-air reason, and a
// click that gets through anyway starts nothing here (no local cycle, no
// request) and puts the reason back.
void RemotePeripheralsTest::remoteTuneWaitsWhileTheCoresTuneIsOn()
{
    RadioModel model(RadioModel::Role::Remote);
    RecordingTgxlLink link;
    link.linkReady = true;
    link.tgxlControl = true;
    link.autotune = true;
    model.attachStation(&link);
    TunerApplet applet(&model, model.tunerModel());
    applet.setTransmitPermitted(true, QString());
    model.reportStationLinkStateChanged();
    QVERIFY(applet.tuneButtonForTesting()->isEnabled());

    model.transmitModel().setTune(true);   // the Core's TUN, mirrored
    NEREUS_TRY_VERIFY(model.isCoreOnAir());
    NEREUS_TRY_VERIFY(!applet.tuneButtonForTesting()->isEnabled());
    QCOMPARE(applet.tuneButtonForTesting()->toolTip(), TunerApplet::onAirReason());

    applet.tuneButtonForTesting()->setEnabled(true);   // gets through anyway
    emit applet.tuneButtonForTesting()->clicked();
    QVERIFY(!model.isTgxlAutotuneInProgress());
    QVERIFY(!model.isTune());
    QVERIFY(link.tgxlRequests.isEmpty());
    QVERIFY(!applet.tuneButtonForTesting()->isEnabled());
    QCOMPARE(applet.tuneButtonForTesting()->toolTip(), TunerApplet::onAirReason());

    model.transmitModel().setTune(false);
    NEREUS_TRY_VERIFY(applet.tuneButtonForTesting()->isEnabled());
    model.detachStation();
}

// R-R3-49 fix wave: from STANDBY, one OPERATE click is one setTgxlOperate
// on a Core that applies it whole (remoteTgxlControlVersion 3), and bypass
// off then operate on for a Core at 2. The other steps of the cycle are
// one request either way.
void RemotePeripheralsTest::operateFromStandbyIsOneRequestOnACoreThatAppliesItWhole()
{
    for (const bool whole : {true, false}) {
        RadioModel model(RadioModel::Role::Remote);
        RecordingTgxlLink link;
        link.linkReady = true;
        link.tgxlControl = true;
        link.tgxlWhole = whole;
        model.attachStation(&link);
        TunerApplet applet(&model, model.tunerModel());
        applet.setTransmitPermitted(
            false, QStringLiteral("Remote transmit controls are not available from this Core."));
        model.reportStationLinkStateChanged();
        QVERIFY(applet.operateButtonForTesting()->isEnabled());
        QVERIFY(!model.tunerModel()->isOperate());
        applet.operateButtonForTesting()->click();
        const QStringList expected = whole
            ? QStringList{QStringLiteral("operate 1")}
            : QStringList{QStringLiteral("bypass 0"), QStringLiteral("operate 1")};
        QCOMPARE(link.tgxlRequests, expected);
        model.detachStation();
    }
}

namespace {
void wheel(QWidget* widget, int delta)
{
    QWheelEvent event(QPointF(4, 4), QPointF(4, 4), QPoint(), QPoint(0, delta), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(widget, &event);
}

int relayCommandCount(const FakeGenius& tuner)
{
    int count = 0;
    for (const QString& c : tuner.commands) {
        if (c.startsWith(QLatin1String("tune relay="))) { ++count; }
    }
    return count;
}
} // namespace

// R-R3-49 (parity Task 8, remoteTgxlControlVersion 4): a wheel nudge on C1,
// L or C2 in a remote window moves the Core's tuner relay (the fake tuner
// records the local applet's own line), on a receive-only Core too; the
// bar follows the tuner's relayC1/relayL/relayC2, not the wheel; on the
// air scrolling is off with the reason, and a request sent anyway is
// refused with it and reaches nothing.
void RemotePeripheralsTest::remoteWindowMovesTheTunerRelaysThroughTheCore()
{
    AppSettings::instance().clear();
    CoreAndWindow cw;
    RadioModel& station = cw.station;
    RadioModel& window = cw.window;
    station.smartSdrListener()->setListenEndpointForTesting(QHostAddress::LocalHost, 0);
    station.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("True"));
    FakeGenius tuner;
    QVERIFY(tuner.listen());
    TunerApplet applet(&window, window.tunerModel());
    const QString transmitReason =
        QStringLiteral("Remote transmit controls are not available from this Core.");
    applet.setTransmitPermitted(false, transmitReason);   // as MainWindow does
    QSignalSpy refused(&window, &RadioModel::accessoryRequestRefused);

    // Before the Core admits a tuner the bars do not scroll.
    cw.connect(this);
    NEREUS_TRY_VERIFY(cw.client.tgxlFullControlAvailable());
    window.reportStationLinkStateChanged();
    QVERIFY(!applet.relayBarForTesting(0)->isScrollEnabled());
    QVERIFY(admitCoreTuner(station, tuner));
    QVERIFY(station.receiveOnlyStationPolicy());
    NEREUS_TRY_VERIFY(window.tunerModel()->hasDirectConnection());
    for (int relay = 0; relay < 3; ++relay) {
        NEREUS_TRY_VERIFY(applet.relayBarForTesting(relay)->isScrollEnabled());
        QVERIFY(applet.relayBarForTesting(relay)->toolTip().isEmpty());
    }
    QVERIFY(!applet.tuneButtonForTesting()->isEnabled());

    // C1 up, L down, C2 up: the Core's tuner gets each line; the bar moves
    // only when the tuner reports it.
    int mark = tuner.commands.size();
    wheel(applet.relayBarForTesting(0), 120);
    QVERIFY(tuner.waitFor(QStringLiteral("tune relay=0 move=1"), mark) >= 0);
    QCOMPARE(applet.relayBarForTesting(0)->value(), 0);
    tuner.send(QStringLiteral("S0|state relayC1=42 relayL=17 relayC2=3"));
    NEREUS_TRY_COMPARE(window.tunerModel()->relayC1(), 42);
    NEREUS_TRY_COMPARE(applet.relayBarForTesting(0)->value(), 42);
    QCOMPARE(applet.relayBarForTesting(1)->value(), 17);
    mark = tuner.commands.size();
    wheel(applet.relayBarForTesting(1), -120);
    QVERIFY(tuner.waitFor(QStringLiteral("tune relay=1 move=-1"), mark) >= 0);
    wheel(applet.relayBarForTesting(2), 120);
    QVERIFY(tuner.waitFor(QStringLiteral("tune relay=2 move=1"), mark) >= 0);
    QCOMPARE(applet.relayBarForTesting(1)->value(), 17);
    QVERIFY(refused.isEmpty());
    QVERIFY(!station.isTransmitting());
    QVERIFY(!station.transmitModel().isTune());

    // On the air (the Core's MoxController, a MOX click then the radio's
    // own PTT input, its receive-only pre-check lifted as in the switching
    // test): scrolling is off with the reason, and a request sent anyway is
    // refused and reaches nothing.
    MoxController* const mox = station.moxController();
    QVERIFY(mox);
    mox->setMoxCheck({});
    for (int keying = 0; keying < 2; ++keying) {
        if (keying == 0) { mox->setMox(true); } else { mox->onMicPttFromRadio(true); }
        NEREUS_TRY_VERIFY(window.isCoreOnAir());
        for (int relay = 0; relay < 3; ++relay) {
            NEREUS_TRY_VERIFY(!applet.relayBarForTesting(relay)->isScrollEnabled());
            QCOMPARE(applet.relayBarForTesting(relay)->toolTip(), TunerApplet::onAirReason());
        }
        const int before = relayCommandCount(tuner);
        wheel(applet.relayBarForTesting(0), 120);
        const int refusedBefore = refused.count();
        QVERIFY(cw.client.requestTgxlRelayMove(0, 1).sent);
        NEREUS_TRY_COMPARE(refused.count(), refusedBefore + 1);
        QCOMPARE(refused.last().at(0).toString(), QStringLiteral("tgxl"));
        QCOMPARE(refused.last().at(1).toString(), TunerApplet::onAirReason());
        NereusSDR::Test::settleSession();
        QCOMPARE(relayCommandCount(tuner), before);
        if (keying == 0) { mox->setMox(false); } else { mox->onMicPttFromRadio(false); }
        NEREUS_TRY_VERIFY(!window.isCoreOnAir());
        NEREUS_TRY_VERIFY(applet.relayBarForTesting(0)->isScrollEnabled());
        QVERIFY(applet.relayBarForTesting(0)->toolTip().isEmpty());
    }
    // A bad request is not understood, in the Core's words.
    const int refusedBefore = refused.count();
    QVERIFY(cw.client.requestTgxlRelayMove(3, 1).sent);
    NEREUS_TRY_COMPARE(refused.count(), refusedBefore + 1);
    QCOMPARE(refused.last().at(1).toString(),
             QStringLiteral("The request to move a Tuner Genius relay was not understood."));
    QVERIFY(!station.transmitModel().isTune());
}

// R-R3-49 (parity Task 8): Setup > 4O3A > Peripherals in a remote window.
// Scan LAN lists what the Core hears and a pick fills Host and Port; a Host
// or Port typed without Connect reaches the Core when editing finishes,
// and when Setup closes with an unsent edit; the Core keeps it for the next
// window; nothing is dialled. On the air, and on an older Core, the scan
// and the fields wait with their reasons.
void RemotePeripheralsTest::remoteWindowScansAndKeepsTheTunerAddressOnTheCore()
{
    {   // An older Core: Scan LAN stays off with its reason; a typed edit
        // is kept for Connect, as before.
        RadioModel model(RadioModel::Role::Remote);
        RecordingTgxlLink link;
        link.linkReady = true;
        link.available = true;
        link.tgxlControl = true;
        model.attachStation(&link);
        PeripheralsPage page(&model);
        model.reportStationLinkStateChanged();
        auto* scan = page.findChild<QPushButton*>(QStringLiteral("tgxlScanButton"));
        QVERIFY(scan);
        QVERIFY(!scan->isEnabled());
        QVERIFY(OperatorWording::isPlain(scan->toolTip()));
        QVERIFY(OperatorWording::isPlain(IStationLink::tgxlFullControlUnavailableReason()));
        QVERIFY(!link.requestTgxlLanScan().sent);
        model.detachStation();
    }

    AppSettings::instance().clear();
    CoreAndWindow cw;
    RadioModel& station = cw.station;
    RadioModel& window = cw.window;
    station.smartSdrListener()->setListenEndpointForTesting(QHostAddress::LocalHost, 0);
    station.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("True"));
    station.setTgxlLanScanWindowMsForTest(150);
    QSignalSpy refused(&window, &RadioModel::accessoryRequestRefused);
    // Nothing dials the tuner: the Core stays switched off or disconnected.
    const auto notDialled = [&station] {
        const auto phase = station.tunerModel()->connectionPhase();
        return (phase == TunerModel::ConnectionPhase::Disabled
                || phase == TunerModel::ConnectionPhase::Disconnected)
            && !station.tgxlConnection()->isConnected();
    };
    cw.connect(this);
    NEREUS_TRY_VERIFY(cw.client.tgxlFullControlAvailable());
    window.reportStationLinkStateChanged();

    SetupDialog dialog(&window);
    dialog.show();
    QVERIFY(dialog.selectNavigationTarget(QStringLiteral("peripherals")));
    auto* page = dialog.findChild<PeripheralsPage*>();
    QVERIFY(page);
    auto* host = page->findChild<QLineEdit*>(QStringLiteral("tgxlHostEdit"));
    auto* port = page->findChild<QSpinBox*>(QStringLiteral("tgxlPortSpin"));
    auto* scan = page->findChild<QPushButton*>(QStringLiteral("tgxlScanButton"));
    QVERIFY(host && port && scan);
    NEREUS_TRY_VERIFY(scan->isEnabled());
    QVERIFY(OperatorWording::isPlain(scan->toolTip()));
    QVERIFY(host->isEnabled());

    // Scan LAN: the Core listens; the dialog lists what it heard.
    scan->click();
    auto* scanDialog = page->findChild<LanScanDialog*>(QStringLiteral("tgxlCoreScanDialog"));
    QVERIFY(scanDialog);
    LanDiscovery* coreScan = nullptr;
    NEREUS_TRY_VERIFY((coreScan = station.findChild<LanDiscovery*>(QStringLiteral("tgxlLanScan")))
                != nullptr);
    coreScan->injectDatagramForTesting(
        QStringLiteral("TunerGeniusXL ip=192.0.2.44 v=1.2.17 serial=9911-2 nickname=Shack_TGXL"),
        9010);
    NEREUS_TRY_VERIFY(scanDialog->rowCountForTesting() >= 1);
    QVERIFY(OperatorWording::isPlain(scanDialog->statusTextForTesting()));
    auto* table = scanDialog->findChild<QTableWidget*>();
    QVERIFY(table);
    int row = -1;
    for (int i = 0; i < table->rowCount(); ++i) {
        if (table->item(i, 1)->text() == QStringLiteral("192.0.2.44")) { row = i; }
    }
    QVERIFY(row >= 0);
    QCOMPARE(table->item(row, 2)->text(), QStringLiteral("9010"));
    QCOMPARE(table->item(row, 4)->text(), QStringLiteral("9911-2"));
    // A pick fills Host and Port and is kept on the Core, not dialled.
    scanDialog->pickRowForTesting(row);
    QCOMPARE(host->text(), QStringLiteral("192.0.2.44"));
    QCOMPARE(port->value(), 9010);
    NEREUS_TRY_COMPARE(station.peripheralValue(QStringLiteral("TGXL_ManualIp")),
                 QStringLiteral("192.0.2.44"));
    NEREUS_TRY_COMPARE(window.tunerModel()->configuredHost(), QStringLiteral("192.0.2.44"));
    QVERIFY(notDialled());

    // A Host typed without Connect reaches the Core when editing finishes.
    host->clear();
    QTest::keyClicks(host, QStringLiteral("192.0.2.77"));
    emit host->editingFinished();
    NEREUS_TRY_COMPARE(station.peripheralValue(QStringLiteral("TGXL_ManualIp")),
                 QStringLiteral("192.0.2.77"));
    // A Port changed and left unsent reaches the Core when Setup closes.
    port->setValue(9055);
    QCOMPARE(station.peripheralValue(QStringLiteral("TGXL_ManualPort")), QStringLiteral("9010"));
    dialog.close();
    NEREUS_TRY_COMPARE(station.peripheralValue(QStringLiteral("TGXL_ManualPort")),
                 QStringLiteral("9055"));
    NEREUS_TRY_COMPARE(window.tunerModel()->configuredPort(), 9055);
    QVERIFY(refused.isEmpty());
    QVERIFY(notDialled());

    // The Core keeps it: a later window reads it from the Core.
    {
        PeripheralsPage later(&window);
        auto* laterHost = later.findChild<QLineEdit*>(QStringLiteral("tgxlHostEdit"));
        auto* laterPort = later.findChild<QSpinBox*>(QStringLiteral("tgxlPortSpin"));
        QCOMPARE(laterHost->text(), QStringLiteral("192.0.2.77"));
        QCOMPARE(laterPort->value(), 9055);
    }

    // Parity mini-round (rulings a and b): on the air, Scan LAN only
    // listens and the address is only saved, so both stay live, as a local
    // window's do, and the Core takes them.
    PeripheralsPage onAirPage(&window);
    window.reportStationLinkStateChanged();
    auto* onAirScan = onAirPage.findChild<QPushButton*>(QStringLiteral("tgxlScanButton"));
    auto* onAirHost = onAirPage.findChild<QLineEdit*>(QStringLiteral("tgxlHostEdit"));
    NEREUS_TRY_VERIFY(onAirScan->isEnabled());
    const QString offAirScanTip = onAirScan->toolTip();
    MoxController* const mox = station.moxController();
    QVERIFY(mox);
    mox->setMoxCheck({});
    mox->setMox(true);
    NEREUS_TRY_VERIFY(window.isCoreOnAir());
    QVERIFY(onAirScan->isEnabled());
    QCOMPARE(onAirScan->toolTip(), offAirScanTip);
    QVERIFY(onAirHost->isEnabled());
    QVERIFY(onAirHost->toolTip() != RadioModel::onAirReason());
    const int refusedBefore = refused.count();
    QSignalSpy scanned(&window, &RadioModel::stationTgxlLanScanFinished);
    QVERIFY(cw.client.requestTgxlLanScan().sent);
    QVERIFY(cw.client.requestTgxlAddress(QStringLiteral("192.0.2.88"), 9010).sent);
    NEREUS_TRY_COMPARE(station.peripheralValue(QStringLiteral("TGXL_ManualIp")),
                 QStringLiteral("192.0.2.88"));
    NEREUS_TRY_COMPARE(scanned.count(), 1);
    QVERIFY(scanned.last().at(1).toBool());
    QCOMPARE(refused.count(), refusedBefore);
    QVERIFY(window.isCoreOnAir());
    mox->setMox(false);
    NEREUS_TRY_VERIFY(!window.isCoreOnAir());
    QVERIFY(onAirScan->isEnabled());
    QVERIFY(onAirHost->isEnabled());

    // Group B fix wave (I1): a blank Host, as the tooltip says, is kept on
    // the Core as a local window's blank Host is, and stops auto-connect.
    onAirPage.show();
    const int refusedBeforeBlank = refused.count();
    onAirHost->selectAll();
    QTest::keyClick(onAirHost, Qt::Key_Backspace);
    QVERIFY(onAirHost->text().isEmpty());
    emit onAirHost->editingFinished();
    NEREUS_TRY_VERIFY(station.peripheralValue(QStringLiteral("TGXL_ManualIp")).isEmpty());
    NEREUS_TRY_VERIFY(window.tunerModel()->configuredHost().isEmpty());
    QCOMPARE(refused.count(), refusedBeforeBlank);
    station.applyPeripheralsForTest();
    NereusSDR::Test::settleSession();
    QVERIFY(notDialled());
}

// R-R3-49 (parity Task 8): in a remote window, right-click > Recall tune
// memory copies the stored values into the bars and sends nothing, Open
// TGXL Advanced opens the Tuner Genius tab, and Copy diagnostics copies the
// Core's connection (the mirrored tuner and its counters on
// accessoryData), not this computer's idle socket.
void RemotePeripheralsTest::remoteTunerMenuRecallsOpensAdvancedAndCopiesTheCore()
{
    AppSettings::instance().clear();
    CoreAndWindow cw;
    RadioModel& station = cw.station;
    RadioModel& window = cw.window;
    station.smartSdrListener()->setListenEndpointForTesting(QHostAddress::LocalHost, 0);
    station.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("True"));
    FakeGenius tuner;
    QVERIFY(tuner.listen());
    TunerApplet applet(&window, window.tunerModel(), nullptr, window.tuneMemoryStore());
    applet.setTransmitPermitted(
        false, QStringLiteral("Remote transmit controls are not available from this Core."));
    cw.connect(this);
    NEREUS_TRY_VERIFY(cw.client.tgxlFullControlAvailable());
    window.reportStationLinkStateChanged();
    QVERIFY(admitCoreTuner(station, tuner));
    NEREUS_TRY_VERIFY(window.tunerModel()->hasDirectConnection());

    const auto action = [](QMenu* menu, const QString& text) -> QAction* {
        for (QAction* a : menu->actions()) {
            if (a->text() == text) { return a; }
        }
        return nullptr;
    };
    std::unique_ptr<QMenu> menu(applet.buildContextMenuForTesting());
    QAction* advanced = action(menu.get(), QStringLiteral("Open TGXL Advanced..."));
    QAction* recall = action(menu.get(), QStringLiteral("Recall tune memory"));
    QVERIFY(advanced && recall);
    QVERIFY(advanced->isEnabled());
    QVERIFY(recall->isEnabled());

    QSignalSpy navigation(&applet, &TunerApplet::navigationRequested);
    advanced->trigger();
    QCOMPARE(navigation.count(), 1);
    QCOMPARE(navigation.first().first().toString(), QStringLiteral("tgxlAdvanced"));

    // Recall: the stored values into the bars; nothing reaches the tuner.
    TuneMemory mem{};
    mem.antenna = 1;
    mem.band = Band::Band20m;
    mem.c1 = 11;
    mem.l = 22;
    mem.c2 = 33;
    window.tuneMemoryStore()->store(mem);
    applet.testSetCurrentBandAndAntenna(Band::Band20m, 1);
    const int mark = tuner.commands.size();
    recall->trigger();
    QCOMPARE(applet.relayBarForTesting(0)->value(), 11);
    QCOMPARE(applet.relayBarForTesting(1)->value(), 22);
    QCOMPARE(applet.relayBarForTesting(2)->value(), 33);
    NereusSDR::Test::settleSession();
    for (int i = mark; i < tuner.commands.size(); ++i) {
        QVERIFY2(tuner.commands.at(i) == QStringLiteral("status")
                     || tuner.commands.at(i).startsWith(QStringLiteral("keepalive"))
                     || tuner.commands.at(i).startsWith(QStringLiteral("ping")),
                 qPrintable(tuner.commands.at(i)));
    }

    // Copy diagnostics: the Core's connection, not this computer's.
    QVERIFY(!window.tgxlConnection()->isConnected());
    NEREUS_TRY_VERIFY(window.accessoryDataModel()->tgxlFramesIn() > 0);
    const QString text = TunerApplet::coreDiagnosticsText(&window);
    QVERIFY2(text.contains(QStringLiteral("Connected: Yes")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("Serial: 241288-1")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("Lines in/out: %1 / %2")
                               .arg(window.accessoryDataModel()->tgxlFramesIn())
                               .arg(window.accessoryDataModel()->tgxlFramesOut())),
             qPrintable(text));
    QVERIFY(!station.transmitModel().isTune());
}

// R-R3-49 (parity Task 8): Open PGXL Advanced, Open TGXL Advanced and the
// PGXL Interlock entry open CAT & Network > 4O3A at their own tab, not
// Setup's first page, in local and remote windows.
void RemotePeripheralsTest::advancedAndInterlockEntriesOpenTheirFourO3ATab()
{
    RadioModel local;
    RadioModel remote(RadioModel::Role::Remote);
    RecordingTgxlLink link;
    link.linkReady = true;
    remote.attachStation(&link);
    for (RadioModel* model : {&local, &remote}) {
        const struct { const char* key; FourO3APage::Tab tab; } targets[] = {
            {"pgxlAdvanced", FourO3APage::Tab::PowerGenius},
            {"tgxlAdvanced", FourO3APage::Tab::TunerGenius},
            {"pgxlInterlock", FourO3APage::Tab::General},
            {"peripherals", FourO3APage::Tab::General},
        };
        for (const auto& target : targets) {
            SetupDialog dialog(model);
            QVERIFY(dialog.findChild<FourO3APage*>() == nullptr);   // not the first page
            QVERIFY(dialog.selectNavigationTarget(QString::fromLatin1(target.key)));
            auto* page = dialog.findChild<FourO3APage*>();
            QVERIFY2(page, target.key);
            QCOMPARE(page->currentTabForTesting(), target.tab);
            if (target.tab == FourO3APage::Tab::General) {
                QVERIFY(page->findChild<PgxlInterlockPage*>());
            }
        }
        SetupDialog dialog(model);
        QVERIFY(!dialog.selectNavigationTarget(QStringLiteral("noSuchPage")));
    }
    remote.detachStation();
}

// R-R3-49 (parity Task 9): the Power Genius applet's OPERATE and the 4O3A
// page's Power Genius tab Operate in a remote window ask the Core, which
// sends the local applet's line to its amp; the buttons follow the amp's
// report. On the air both wait with the reason and a request sent anyway
// is refused with nothing reaching the amp. Copy diagnostics copies the
// Core's connection. An older Core leaves OPERATE greyed with its reason.
void RemotePeripheralsTest::remoteWindowOperatesTheAmpThroughTheCore()
{
    {   // An older Core: OPERATE stays greyed with the older reason.
        RadioModel model(RadioModel::Role::Remote);
        RecordingTgxlLink link;
        link.linkReady = true;
        link.pgxlAvailable = true;
        model.attachStation(&link);
        AmpApplet applet(&model);
        model.reportStationLinkStateChanged();
        QVERIFY(!applet.operateButtonEnabledForTesting());
        QCOMPARE(applet.operateButtonToolTipForTesting(), AmpApplet::remoteUnavailableReason());
        QVERIFY(!link.requestPgxlOperate(true).sent);
        QVERIFY(OperatorWording::isPlain(IStationLink::pgxlFullControlUnavailableReason()));
        model.detachStation();
    }

    AppSettings::instance().clear();
    CoreAndWindow cw;
    QVERIFY(cw.pairTransmitWindow());
    RadioModel& station = cw.station;
    RadioModel& window = cw.window;
    station.smartSdrListener()->setListenEndpointForTesting(QHostAddress::LocalHost, 0);
    station.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("True"));
    FakeGenius amp;
    QVERIFY(amp.listen());
    AmpApplet applet(&window);
    QSignalSpy localToggles(&applet, &AmpApplet::operateToggled);
    FourO3APage page(&window);
    auto* tabOperate = page.findChild<QPushButton*>(QStringLiteral("remotePgxlOperateButton"));
    QVERIFY(tabOperate);
    QSignalSpy refused(&window, &RadioModel::accessoryRequestRefused);
    QSignalSpy sliceRefused(&window, &RadioModel::sliceAddRejected);
    const auto operateLines = [&amp] {
        return amp.commands.filter(QRegularExpression(QStringLiteral("^operate")));
    };

    cw.connect(this);
    NEREUS_TRY_VERIFY(cw.client.pgxlFullControlAvailable());
    window.reportStationLinkStateChanged();
    // Before the Core admits an amp: both wait, saying why.
    const QString notConnected = QStringLiteral("The Core is not connected to the Power Genius.");
    QVERIFY(!applet.operateButtonEnabledForTesting());
    QCOMPARE(applet.operateButtonToolTipForTesting(), notConnected);
    QVERIFY(!tabOperate->isEnabled());
    QCOMPARE(tabOperate->toolTip(), notConnected);
    QVERIFY(OperatorWording::isPlain(notConnected));

    QVERIFY(admitCoreAmp(station, amp));
    NEREUS_TRY_VERIFY(cw.client.capabilities().txPermitted);
    QVERIFY(!station.receiveOnlyStationPolicy());
    amp.send(QStringLiteral("S0|status state=STANDBY"));
    NEREUS_TRY_COMPARE(window.amplifierModel()->deviceState(), QStringLiteral("STANDBY"));
    NEREUS_TRY_VERIFY(applet.operateButtonEnabledForTesting());
    QCOMPARE(applet.operateButtonTextForTesting(), QStringLiteral("STANDBY"));
    QVERIFY(applet.operateButtonToolTipForTesting().isEmpty());
    NEREUS_TRY_VERIFY(tabOperate->isEnabled());
    QCOMPARE(tabOperate->text(), QStringLiteral("Operate"));
    QVERIFY(OperatorWording::isPlain(tabOperate->toolTip()));

    // The applet's OPERATE: the Core's amp gets the local applet's line;
    // the button follows the amp's report, not the click.
    applet.clickOperateForTesting();
    QVERIFY(amp.waitFor(QStringLiteral("operate=1")) >= 0);
    QCOMPARE(applet.operateButtonTextForTesting(), QStringLiteral("STANDBY"));
    amp.send(QStringLiteral("S0|status state=OPERATE"));
    NEREUS_TRY_COMPARE(applet.operateButtonTextForTesting(), QStringLiteral("OPERATE"));
    NEREUS_TRY_COMPARE(tabOperate->text(), QStringLiteral("Standby"));
    // The tab's Operate: standby.
    tabOperate->click();
    QVERIFY(amp.waitFor(QStringLiteral("operate=0")) >= 0);
    amp.send(QStringLiteral("S0|status state=STANDBY"));
    NEREUS_TRY_COMPARE(applet.operateButtonTextForTesting(), QStringLiteral("STANDBY"));
    NEREUS_TRY_COMPARE(tabOperate->text(), QStringLiteral("Operate"));
    QCOMPARE(operateLines(), (QStringList{QStringLiteral("operate=1"),
                                          QStringLiteral("operate=0")}));
    QVERIFY(refused.isEmpty());
    QCOMPARE(localToggles.count(), 0);   // never this computer's connection
    QVERIFY(!window.pgxlConnection()->isConnected());
    QVERIFY(!station.isTransmitting());

    // On the air (a MOX click, then the radio's own PTT input, through the
    // Core's MoxController with test-only logical keying): both
    // wait with the reason; a request sent anyway reaches nothing.
    MoxController* const mox = station.moxController();
    QVERIFY(mox);
    mox->setMoxCheck({});
    for (int keying = 0; keying < 2; ++keying) {
        if (keying == 0) { mox->setMox(true); } else { mox->onMicPttFromRadio(true); }
        NEREUS_TRY_VERIFY(window.isCoreOnAir());
        NEREUS_TRY_VERIFY(!applet.operateButtonEnabledForTesting());
        QCOMPARE(applet.operateButtonToolTipForTesting(), RadioModel::onAirReason());
        NEREUS_TRY_VERIFY(!tabOperate->isEnabled());
        QCOMPARE(tabOperate->toolTip(), RadioModel::onAirReason());
        applet.clickOperateForTesting();
        QVERIFY(QMetaObject::invokeMethod(&page, "onRemotePgxlOperateClicked",
                                          Qt::DirectConnection));
        const int refusedBefore = refused.count();
        QVERIFY(cw.client.requestPgxlOperate(true).sent);
        NEREUS_TRY_COMPARE(refused.count(), refusedBefore + 1);
        QCOMPARE(refused.last().at(0).toString(), QStringLiteral("pgxl"));
        QCOMPARE(refused.last().at(1).toString(), RadioModel::onAirReason());
        NereusSDR::Test::settleSession();
        QCOMPARE(refused.count(), refusedBefore + 1);
        QVERIFY(sliceRefused.isEmpty());
        QCOMPARE(operateLines().size(), 2);
        if (keying == 0) { mox->setMox(false); } else { mox->onMicPttFromRadio(false); }
        NEREUS_TRY_VERIFY(!window.isCoreOnAir());
        NEREUS_TRY_VERIFY(applet.operateButtonEnabledForTesting());
        NEREUS_TRY_VERIFY(tabOperate->isEnabled());
    }

    // Copy diagnostics: the Core's connection, not this computer's.
    NEREUS_TRY_VERIFY(window.accessoryDataModel()->pgxlFramesIn() > 0);
    const QString text = AmpApplet::coreDiagnosticsText(&window);
    QVERIFY2(text.contains(QStringLiteral("Connected: Yes")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("Serial: 10-200/24-0046")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("State: STANDBY")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("Lines in/out: %1 / %2")
                               .arg(window.accessoryDataModel()->pgxlFramesIn())
                               .arg(window.accessoryDataModel()->pgxlFramesOut())),
             qPrintable(text));
    QVERIFY(!station.transmitModel().isTune());
}

// R-R3-49 (parity Task 9): Setup > 4O3A > Peripherals' Power Genius row in
// a remote window scans the Core's network and keeps a typed address on
// the Core, as the Tuner Genius row does; nothing is dialled.
void RemotePeripheralsTest::remoteWindowScansAndKeepsTheAmpAddressOnTheCore()
{
    {   // An older Core: Scan LAN stays off with its reason.
        RadioModel model(RadioModel::Role::Remote);
        RecordingTgxlLink link;
        link.linkReady = true;
        link.pgxlAvailable = true;
        model.attachStation(&link);
        PeripheralsPage page(&model);
        model.reportStationLinkStateChanged();
        auto* scan = page.findChild<QPushButton*>(QStringLiteral("pgxlScanButton"));
        QVERIFY(scan);
        QVERIFY(!scan->isEnabled());
        QVERIFY(OperatorWording::isPlain(scan->toolTip()));
        QVERIFY(!link.requestPgxlLanScan().sent);
        QVERIFY(!link.requestPgxlAddress(QStringLiteral("192.0.2.9"), 9008).sent);
        model.detachStation();
    }

    AppSettings::instance().clear();
    CoreAndWindow cw;
    RadioModel& station = cw.station;
    RadioModel& window = cw.window;
    station.smartSdrListener()->setListenEndpointForTesting(QHostAddress::LocalHost, 0);
    station.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("True"));
    station.setPgxlLanScanWindowMsForTest(150);
    QSignalSpy refused(&window, &RadioModel::accessoryRequestRefused);
    const quint64 token = station.pgxlConnection()->socketAttemptToken();
    const auto notDialled = [&station, token] {
        return !station.pgxlConnection()->isConnected()
            && station.pgxlConnection()->socketAttemptToken() == token;
    };
    cw.connect(this);
    NEREUS_TRY_VERIFY(cw.client.pgxlFullControlAvailable());
    window.reportStationLinkStateChanged();

    SetupDialog dialog(&window);
    dialog.show();
    QVERIFY(dialog.selectNavigationTarget(QStringLiteral("peripherals")));
    auto* page = dialog.findChild<PeripheralsPage*>();
    QVERIFY(page);
    auto* host = page->findChild<QLineEdit*>(QStringLiteral("pgxlHostEdit"));
    auto* port = page->findChild<QSpinBox*>(QStringLiteral("pgxlPortSpin"));
    auto* scan = page->findChild<QPushButton*>(QStringLiteral("pgxlScanButton"));
    QVERIFY(host && port && scan);
    NEREUS_TRY_VERIFY(scan->isEnabled());
    QVERIFY(OperatorWording::isPlain(scan->toolTip()));
    QVERIFY(host->isEnabled());

    // Scan LAN: the Core listens; the dialog lists the Power Genius it heard.
    scan->click();
    auto* scanDialog = page->findChild<LanScanDialog*>(QStringLiteral("pgxlCoreScanDialog"));
    QVERIFY(scanDialog);
    QVERIFY(OperatorWording::isPlain(scanDialog->statusTextForTesting()));
    LanDiscovery* coreScan = nullptr;
    NEREUS_TRY_VERIFY((coreScan = station.findChild<LanDiscovery*>(QStringLiteral("pgxlLanScan")))
                != nullptr);
    coreScan->injectDatagramForTesting(
        QStringLiteral("PowerGeniusXL ip=192.0.2.45 v=3.8.9 serial=5501-7 nickname=Shack_Amp"),
        9008);
    coreScan->injectDatagramForTesting(
        QStringLiteral("TunerGeniusXL ip=192.0.2.44 v=1.2.17 serial=9911-2 nickname=Tuner"),
        9010);
    NEREUS_TRY_VERIFY(scanDialog->rowCountForTesting() >= 1);
    auto* table = scanDialog->findChild<QTableWidget*>();
    QVERIFY(table);
    int row = -1;
    for (int i = 0; i < table->rowCount(); ++i) {
        QVERIFY(table->item(i, 1)->text() != QStringLiteral("192.0.2.44"));   // not the tuner
        if (table->item(i, 1)->text() == QStringLiteral("192.0.2.45")) { row = i; }
    }
    QVERIFY(row >= 0);
    QCOMPARE(table->item(row, 2)->text(), QStringLiteral("9008"));
    QCOMPARE(table->item(row, 4)->text(), QStringLiteral("5501-7"));
    scanDialog->pickRowForTesting(row);
    QCOMPARE(host->text(), QStringLiteral("192.0.2.45"));
    QCOMPARE(port->value(), 9008);
    NEREUS_TRY_COMPARE(station.peripheralValue(QStringLiteral("PGXL_ManualIp")),
                 QStringLiteral("192.0.2.45"));
    NEREUS_TRY_COMPARE(window.amplifierModel()->configuredHost(), QStringLiteral("192.0.2.45"));
    QVERIFY(notDialled());

    // A Host typed without Connect reaches the Core when editing finishes.
    host->clear();
    QTest::keyClicks(host, QStringLiteral("192.0.2.77"));
    emit host->editingFinished();
    NEREUS_TRY_COMPARE(station.peripheralValue(QStringLiteral("PGXL_ManualIp")),
                 QStringLiteral("192.0.2.77"));
    // A Port changed and left unsent reaches the Core when Setup closes.
    port->setValue(9055);
    QCOMPARE(station.peripheralValue(QStringLiteral("PGXL_ManualPort")), QStringLiteral("9008"));
    dialog.close();
    NEREUS_TRY_COMPARE(station.peripheralValue(QStringLiteral("PGXL_ManualPort")),
                 QStringLiteral("9055"));
    NEREUS_TRY_COMPARE(window.amplifierModel()->configuredPort(), 9055);
    QVERIFY(refused.isEmpty());
    QVERIFY(notDialled());

    // The Core keeps it: a later window reads it from the Core.
    {
        PeripheralsPage later(&window);
        QCOMPARE(later.findChild<QLineEdit*>(QStringLiteral("pgxlHostEdit"))->text(),
                 QStringLiteral("192.0.2.77"));
        QCOMPARE(later.findChild<QSpinBox*>(QStringLiteral("pgxlPortSpin"))->value(), 9055);
    }

    // Parity mini-round (rulings a and b): on the air, Scan LAN only
    // listens and the address is only saved, so both stay live, as a local
    // window's do, and the Core takes them without dialling.
    PeripheralsPage onAirPage(&window);
    window.reportStationLinkStateChanged();
    auto* onAirScan = onAirPage.findChild<QPushButton*>(QStringLiteral("pgxlScanButton"));
    auto* onAirHost = onAirPage.findChild<QLineEdit*>(QStringLiteral("pgxlHostEdit"));
    NEREUS_TRY_VERIFY(onAirScan->isEnabled());
    const QString offAirScanTip = onAirScan->toolTip();
    MoxController* const mox = station.moxController();
    QVERIFY(mox);
    mox->setMoxCheck({});
    mox->setMox(true);
    NEREUS_TRY_VERIFY(window.isCoreOnAir());
    QVERIFY(onAirScan->isEnabled());
    QCOMPARE(onAirScan->toolTip(), offAirScanTip);
    QVERIFY(onAirHost->isEnabled());
    QVERIFY(onAirHost->toolTip() != RadioModel::onAirReason());
    const int refusedBefore = refused.count();
    QSignalSpy scanned(&window, &RadioModel::stationPgxlLanScanFinished);
    QVERIFY(cw.client.requestPgxlLanScan().sent);
    QVERIFY(cw.client.requestPgxlAddress(QStringLiteral("192.0.2.88"), 9008).sent);
    NEREUS_TRY_COMPARE(station.peripheralValue(QStringLiteral("PGXL_ManualIp")),
                 QStringLiteral("192.0.2.88"));
    NEREUS_TRY_COMPARE(scanned.count(), 1);
    QVERIFY(scanned.last().at(1).toBool());
    QCOMPARE(refused.count(), refusedBefore);
    QVERIFY(window.isCoreOnAir());
    mox->setMox(false);
    NEREUS_TRY_VERIFY(!window.isCoreOnAir());
    QVERIFY(onAirScan->isEnabled());
    QVERIFY(onAirHost->isEnabled());
    QVERIFY(notDialled());

    // Group B fix wave (I1): a blank Host, as the tooltip says, is kept on
    // the Core as a local window's blank Host is, and stops auto-connect.
    onAirPage.show();
    const int refusedBeforeBlank = refused.count();
    onAirHost->selectAll();
    QTest::keyClick(onAirHost, Qt::Key_Backspace);
    QVERIFY(onAirHost->text().isEmpty());
    emit onAirHost->editingFinished();
    NEREUS_TRY_VERIFY(station.peripheralValue(QStringLiteral("PGXL_ManualIp")).isEmpty());
    NEREUS_TRY_VERIFY(window.amplifierModel()->configuredHost().isEmpty());
    QCOMPARE(refused.count(), refusedBeforeBlank);
    station.applyPeripheralsForTest();
    NereusSDR::Test::settleSession();
    QVERIFY(notDialled());
}

// R-R3-49 (parity Task 9, operator amendment 2026-09-25): a local window's
// Setup > CAT & Network > 4O3A > PowerGenius XL tab has an Operate button
// beside the state badge, as a remote window's tab does. It sends the
// local applet's own line (operate=1 or operate=0) through this computer's
// PgxlConnection, reads Operate or Standby from the amp's report, and is
// disabled with a plain reason while the amp is not connected. Group B fix
// wave (M5): it also waits on the air, as the local applet's OPERATE does
// (localWindowAmpAndTunerSwitchesWaitOnTheAir covers that).
void RemotePeripheralsTest::localPowerGeniusTabOperatesThisComputersAmp()
{
    AppSettings::instance().clear();
    FakeGenius amp;
    QVERIFY(amp.listen());
    RadioModel local;
    PgxlAdvancedPage page(&local);
    QPushButton* operate = page.operateButtonForTesting();
    QVERIFY(operate);
    const QString notConnected = QStringLiteral("The Power Genius is not connected.");
    QVERIFY(!operate->isEnabled());
    QCOMPARE(operate->toolTip(), notConnected);
    QVERIFY(OperatorWording::isPlain(notConnected));
    const auto operateLines = [&amp] {
        return amp.commands.filter(QRegularExpression(QStringLiteral("^operate")));
    };

    local.pgxlConnection()->connectToPgxl(QStringLiteral("127.0.0.1"), amp.port());
    QVERIFY(amp.accept());
    amp.send(QStringLiteral("V3.8.9"));
    NEREUS_TRY_VERIFY(local.pgxlConnection()->isConnected());
    amp.send(QStringLiteral("S0|status state=STANDBY"));
    NEREUS_TRY_VERIFY(operate->isEnabled());
    QCOMPARE(operate->text(), QStringLiteral("Operate"));
    QVERIFY(OperatorWording::isPlain(operate->toolTip()));

    // Operate: the applet's line; the button follows the amp's report.
    operate->click();
    QVERIFY(amp.waitFor(QStringLiteral("operate=1")) >= 0);
    // Task 77 fix round 4 (Minor 3): until the amp reports operate, the tab
    // offers Standby (the way out of an amp that never gets there).
    NEREUS_TRY_COMPARE(operate->text(), QStringLiteral("Standby"));
    amp.send(QStringLiteral("S0|status state=OPERATE"));
    NEREUS_TRY_COMPARE(operate->text(), QStringLiteral("Standby"));
    QVERIFY(OperatorWording::isPlain(operate->toolTip()));
    // Standby.
    operate->click();
    QVERIFY(amp.waitFor(QStringLiteral("operate=0")) >= 0);
    amp.send(QStringLiteral("S0|status state=STANDBY"));
    NEREUS_TRY_COMPARE(operate->text(), QStringLiteral("Operate"));
    QCOMPARE(operateLines(), (QStringList{QStringLiteral("operate=1"),
                                          QStringLiteral("operate=0")}));
    QVERIFY(!local.isTransmitting());

    // The amp goes away: disabled with the reason again.
    amp.peer->disconnectFromHost();
    NEREUS_TRY_VERIFY(!local.pgxlConnection()->isConnected());
    NEREUS_TRY_VERIFY(!operate->isEnabled());
    QCOMPARE(operate->toolTip(), notConnected);
    local.pgxlConnection()->disconnect();
}

// R-R3-49 (parity Task 10): a remote window's RF-Kit applet OPERATE and
// ANT 1 to 4, and its RF-Kit page's "Set amp to TCI mode", switch the
// Core's amp (the local applet's and page's own REST requests) off the air
// and follow the amp's report; Save keeps a changed Host and Port on the
// Core without dialling; Copy diagnostics and Live diagnostics show the
// Core's counts. On the air they wait with the reason and nothing reaches
// the amp. The window never uses this computer's own connection.
void RemotePeripheralsTest::remoteWindowOperatesTheRfKitThroughTheCore()
{
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("RfKit_PollIntervalMs"),
                                     QStringLiteral("250"));
    CoreAndWindow cw;
    QVERIFY(cw.pairTransmitWindow());
    RadioModel& station = cw.station;
    RadioModel& window = cw.window;
    FakeRfKit amp;
    Rf2ksApplet applet(&window);
    QSignalSpy localOperate(&applet, &Rf2ksApplet::operateToggled);
    QSignalSpy localAntenna(&applet, &Rf2ksApplet::antennaRequested);
    RfKitPage page(&window);
    page.show();
    QSignalSpy refused(&window, &RadioModel::accessoryRequestRefused);

    LoopbackTransport* stationEnd = cw.connect(this);
    NEREUS_TRY_VERIFY(cw.client.rfKitFullControlAvailable());
    QVERIFY(cw.client.rfKitCountersAvailable());
    window.reportStationLinkStateChanged();
    QString reason;
    QVERIFY(station.setRfKitEnabledForStation(true, &reason));
    NEREUS_TRY_VERIFY(page.detailTabIsEnabledForTesting());
    // Before the Core admits an amp: they wait, saying why.
    const QString notConnected =
        QStringLiteral("The Core is not connected to the RF-Kit amplifier.");
    QVERIFY(OperatorWording::isPlain(notConnected));
    QVERIFY(!applet.operateButtonEnabledForTesting());
    QCOMPARE(applet.operateButtonToolTipForTesting(), notConnected);
    QVERIFY(!applet.antennaButtonIsEnabledForTesting(1));
    QCOMPARE(applet.antennaButtonToolTipForTesting(1), notConnected);
    QVERIFY(!page.setTciButtonForTesting()->isEnabled());
    QCOMPARE(page.setTciButtonForTesting()->toolTip(), notConnected);

    QVERIFY(station.configureRfKitForStation(QStringLiteral("127.0.0.1"), amp.serverPort(),
                                             &reason));
    NEREUS_TRY_COMPARE_WITH_TIMEOUT(window.rfKitModel()->connectionPhase(),
                              RfKitModel::ConnectionPhase::Connected, 5000);
    NEREUS_TRY_VERIFY(cw.client.capabilities().txPermitted);
    QVERIFY(!station.receiveOnlyStationPolicy());
    // The amp lists its antennas: 1 and 2 usable, 3 disabled, 4 not fitted.
    station.rfKitConnection()->injectJsonForTesting(QStringLiteral("/antennas"), QByteArray(
        R"({"antennas":[{"type":"INTERNAL","number":1,"state":"ACTIVE"},)"
        R"({"type":"INTERNAL","number":2,"state":"AVAILABLE"},)"
        R"({"type":"INTERNAL","number":3,"state":"DISABLED"}]})"));
    NEREUS_TRY_COMPARE(window.rfKitModel()->antennaPresentMask(), 0x7);
    NEREUS_TRY_VERIFY(applet.operateButtonEnabledForTesting());
    QVERIFY(applet.operateButtonToolTipForTesting().isEmpty());
    NEREUS_TRY_VERIFY(applet.antennaButtonIsEnabledForTesting(2));
    QVERIFY(applet.antennaButtonIsEnabledForTesting(1));
    const QString unavailable =
        QStringLiteral("This antenna is not available on the RF-Kit amplifier.");
    QVERIFY(!applet.antennaButtonIsEnabledForTesting(3));
    QCOMPARE(applet.antennaButtonToolTipForTesting(3), unavailable);
    QVERIFY(!applet.antennaButtonIsEnabledForTesting(4));
    QVERIFY(OperatorWording::isPlain(unavailable));
    NEREUS_TRY_VERIFY(page.setTciButtonForTesting()->isEnabled());
    QVERIFY(OperatorWording::isPlain(page.setTciButtonForTesting()->toolTip()));

    // B1.7: OPERATE switches the Core's amp; the button follows the amp's
    // report, not the click.
    NEREUS_TRY_COMPARE(applet.operateButtonTextForTesting(), QStringLiteral("STANDBY"));
    applet.clickOperateButtonForTesting();
    NEREUS_TRY_COMPARE(amp.writes, QStringList{
        QStringLiteral(R"(PUT /operate-mode {"operate_mode":"OPERATE"})")});
    NEREUS_TRY_COMPARE(applet.operateButtonTextForTesting(), QStringLiteral("OPERATE"));
    QVERIFY(window.rfKitModel()->operate());
    applet.clickOperateButtonForTesting();
    NEREUS_TRY_COMPARE(amp.writes.size(), 2);
    QCOMPARE(amp.writes.at(1), QStringLiteral(R"(PUT /operate-mode {"operate_mode":"STANDBY"})"));
    NEREUS_TRY_COMPARE(applet.operateButtonTextForTesting(), QStringLiteral("STANDBY"));

    // B1.8: ANT 2 switches the Core's amp and lights when the amp says so.
    applet.clickAntennaButtonForTesting(2);
    NEREUS_TRY_COMPARE(amp.writes.size(), 3);
    QCOMPARE(amp.writes.at(2),
             QStringLiteral(R"(PUT /antennas/active {"number":2,"type":"INTERNAL"})"));
    NEREUS_TRY_COMPARE(window.rfKitModel()->activeAntennaNumber(), 2);
    NEREUS_TRY_VERIFY(applet.antennaButtonIsActiveForTesting(2));
    QVERIFY(!applet.antennaButtonIsActiveForTesting(1));

    // B1.9: Set amp to TCI mode from the page; the amp reports TCI after.
    NEREUS_TRY_COMPARE(window.rfKitModel()->operationalInterface(), QStringLiteral("UDP"));
    page.setTciButtonForTesting()->click();
    NEREUS_TRY_COMPARE(amp.writes.size(), 4);
    QCOMPARE(amp.writes.at(3), QStringLiteral(
        R"(PUT /operational-interface {"operational_interface":"TCI"})"));
    NEREUS_TRY_COMPARE(window.rfKitModel()->operationalInterface(), QStringLiteral("TCI"));
    QVERIFY(refused.isEmpty());
    QCOMPARE(localOperate.count(), 0);   // never this computer's connection
    QCOMPARE(localAntenna.count(), 0);

    // B1.11: Host and Port with Save are kept on the Core, nothing dialled.
    page.hostEditForTesting()->setText(QStringLiteral("192.0.2.77"));
    page.portSpinForTesting()->setValue(8099);
    page.saveButtonForTesting()->click();
    NEREUS_TRY_COMPARE(station.peripheralValue(QStringLiteral("RfKit_ManualIp")),
                 QStringLiteral("192.0.2.77"));
    QCOMPARE(station.peripheralValue(QStringLiteral("RfKit_ManualPort")), QStringLiteral("8099"));
    QVERIFY(station.rfKitConnection()->isConnected());
    QCOMPARE(station.rfKitConnection()->peerAddress(), QStringLiteral("127.0.0.1"));
    QVERIFY(refused.isEmpty());

    // B1.12: the Core's connection counts in Live diagnostics and Copy
    // diagnostics.
    NEREUS_TRY_VERIFY_WITH_TIMEOUT(window.accessoryDataModel()->rfkitPollsOk() > 0, 3000);
    NEREUS_TRY_VERIFY(window.accessoryDataModel()->rfkitConnectedSinceMs() > 0);
    NEREUS_TRY_VERIFY(page.diagnosticsTextForTesting().contains(
        QStringLiteral("Polls: %1 OK").arg(window.accessoryDataModel()->rfkitPollsOk())));
    QVERIFY(page.diagnosticsTextForTesting().contains(QStringLiteral("Connected since")));
    QVERIFY(!page.diagnosticsTextForTesting().contains(QStringLiteral("Connected since --")));
    const QString text = Rf2ksApplet::coreDiagnosticsText(&window);
    QVERIFY2(text.contains(QStringLiteral("Connected: Yes")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("Host: 127.0.0.1:%1").arg(amp.serverPort())),
             qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("Interface: TCI")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("Polls OK/failed: %1/%2")
                               .arg(window.accessoryDataModel()->rfkitPollsOk())
                               .arg(window.accessoryDataModel()->rfkitPollsFailed())),
             qPrintable(text));
    // Group B fix wave (M7): the amp's response time, as a local window
    // shows it (the Core's average over its last ten polls).
    QVERIFY(cw.client.rfKitResponseTimeAvailable());
    for (int i = 0; i < 10; ++i) { station.rfKitConnection()->testMarkPollSuccess(250); }
    NEREUS_TRY_VERIFY(window.accessoryDataModel()->rfkitRttAvgMs() > 0);
    NEREUS_TRY_VERIFY2(page.diagnosticsTextForTesting().contains(
                     QStringLiteral("RTT %1 ms avg")
                         .arg(window.accessoryDataModel()->rfkitRttAvgMs())),
                 qPrintable(page.diagnosticsTextForTesting()));
    const QString withRtt = Rf2ksApplet::coreDiagnosticsText(&window);
    QVERIFY2(withRtt.contains(QStringLiteral("RTT avg: %1 ms")
                                  .arg(window.accessoryDataModel()->rfkitRttAvgMs())),
             qPrintable(withRtt));

    // On the air (a MOX click, through the Core's MoxController with its
    // test-only logical keying): the switches (OPERATE, the antennas
    // and TCI mode) wait with the reason, and a request sent anyway
    // reaches nothing. Parity mini-round (rulings a and b): Host, Port and
    // Save only save, so they stay live, as a local window's do, and the
    // Core keeps the address without dialling.
    MoxController* const mox = station.moxController();
    QVERIFY(mox);
    mox->setMoxCheck({});
    mox->setMox(true);
    NEREUS_TRY_VERIFY(window.isCoreOnAir());
    NEREUS_TRY_VERIFY(!applet.operateButtonEnabledForTesting());
    QCOMPARE(applet.operateButtonToolTipForTesting(), RadioModel::onAirReason());
    QVERIFY(!applet.antennaButtonIsEnabledForTesting(1));
    QCOMPARE(applet.antennaButtonToolTipForTesting(1), RadioModel::onAirReason());
    NEREUS_TRY_VERIFY(!page.setTciButtonForTesting()->isEnabled());
    QCOMPARE(page.setTciButtonForTesting()->toolTip(), RadioModel::onAirReason());
    QVERIFY(page.hostEditForTesting()->isEnabled());
    QVERIFY(page.hostEditForTesting()->toolTip() != RadioModel::onAirReason());
    QVERIFY(page.saveButtonForTesting()->isEnabled());
    applet.clickOperateButtonForTesting();
    applet.clickAntennaButtonForTesting(1);
    const int refusedBefore = refused.count();
    QVERIFY(cw.client.requestRfKitOperate(true).sent);
    NEREUS_TRY_COMPARE(refused.count(), refusedBefore + 1);
    QCOMPARE(refused.last().at(0).toString(), QStringLiteral("rfkit"));
    QCOMPARE(refused.last().at(1).toString(), RadioModel::onAirReason());
    QVERIFY(cw.client.requestRfKitTciMode().sent);
    NEREUS_TRY_COMPARE(refused.count(), refusedBefore + 2);
    QCOMPARE(refused.last().at(1).toString(), RadioModel::onAirReason());
    page.hostEditForTesting()->setText(QStringLiteral("192.0.2.78"));
    page.saveButtonForTesting()->click();
    NEREUS_TRY_COMPARE(station.peripheralValue(QStringLiteral("RfKit_ManualIp")),
                 QStringLiteral("192.0.2.78"));
    NereusSDR::Test::settleSession();
    QCOMPARE(refused.count(), refusedBefore + 2);
    QCOMPARE(amp.writes.size(), 4);
    QVERIFY(window.isCoreOnAir());
    mox->setMox(false);
    NEREUS_TRY_VERIFY(!window.isCoreOnAir());
    NEREUS_TRY_VERIFY(applet.operateButtonEnabledForTesting());
    NEREUS_TRY_VERIFY(page.setTciButtonForTesting()->isEnabled());
    QVERIFY(page.hostEditForTesting()->isEnabled());

    // Group B fix wave (I1): a blank Host with Save is kept on the Core, as
    // a local Save keeps it, and stops auto-connect; the running
    // connection is left alone.
    const int refusedBeforeBlank = refused.count();
    page.hostEditForTesting()->clear();
    page.saveButtonForTesting()->click();
    NEREUS_TRY_VERIFY(station.peripheralValue(QStringLiteral("RfKit_ManualIp")).isEmpty());
    QCOMPARE(station.peripheralValue(QStringLiteral("RfKit_ManualPort")), QStringLiteral("8099"));
    NereusSDR::Test::settleSession();
    QCOMPARE(refused.count(), refusedBeforeBlank);
    QVERIFY(station.rfKitConnection()->isConnected());

    // Nothing keyed; the window opened no connection of its own.
    QVERIFY(!station.transmitModel().isTune());
    QVERIFY(!window.rfKitConnection()->isConnected());
    QVERIFY(window.rfKitConnection()->peerAddress().isEmpty());
    stationEnd->closeLink(QStringLiteral("test done"));
    AppSettings::instance().clear();
}

// R-R3-49 (parity Task 10): a Core below remoteRfKitControlVersion 4 leaves
// OPERATE, the antennas and TCI mode greyed with the older reasons, and the
// window asks nothing.
void RemotePeripheralsTest::olderCoreLeavesTheRfKitSwitchesGreyed()
{
    AppSettings::instance().clear();
    struct OlderCore final : IStationLink {
        CommandOutcome requestAddSlice(const QString&) override { return {}; }
        CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
        CommandOutcome requestRemoveSlice(int) override { return {}; }
        CommandOutcome requestActiveSlice(int) override { return {}; }
        CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
        bool stationLinkReady() const override { return true; }
        bool remoteRfKitStatusAvailable() const override { return true; }
        bool remoteRfKitControlAvailable() const override { return true; }
        bool rfKitSettingsAvailable() const override { return true; }
    } link;
    RadioModel model(RadioModel::Role::Remote);
    model.attachStation(&link);
    Rf2ksApplet applet(&model);
    RfKitPage page(&model);
    model.reportStationLinkStateChanged();
    QVERIFY(!applet.operateButtonEnabledForTesting());
    QCOMPARE(applet.operateButtonToolTipForTesting(), AmpApplet::remoteUnavailableReason());
    for (int n = 1; n <= 4; ++n) {
        QVERIFY(!applet.antennaButtonIsEnabledForTesting(n));
        QCOMPARE(applet.antennaButtonToolTipForTesting(n), AmpApplet::remoteUnavailableReason());
    }
    QVERIFY(!page.setTciButtonForTesting()->isEnabled());
    QVERIFY(OperatorWording::isPlain(page.setTciButtonForTesting()->toolTip()));
    QVERIFY(page.diagnosticsTextForTesting().contains(QStringLiteral("The Core keeps")));
    for (const IStationLink::CommandOutcome& outcome :
         {link.requestRfKitOperate(true), link.requestRfKitAntenna(2),
          link.requestRfKitTciMode(), link.requestRfKitAddress(QStringLiteral("192.0.2.9"), 80)}) {
        QVERIFY(!outcome.sent);
        QCOMPARE(outcome.reason, IStationLink::rfKitFullControlUnavailableReason());
    }
    QVERIFY(OperatorWording::isPlain(IStationLink::rfKitFullControlUnavailableReason()));
    model.detachStation();
    AppSettings::instance().clear();
}

// R-R3-49 (parity Task 10, both ways): a local window's RF-Kit page shows the
// connected-since and last-poll readings a remote window shows from the
// Core, and its "Set amp to TCI mode" still sends this computer's own
// request.
void RemotePeripheralsTest::localRfKitPageShowsTheRemoteReadings()
{
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("PeripheralsMigrationDone"),
                                     QStringLiteral("True"));
    FakeRfKit amp;
    RadioModel local;
    RadioInfo radio;
    radio.macAddress = QStringLiteral("aa:bb:cc:dd:ee:4a");
    local.setLastRadioInfoForTest(radio);
    local.setConnectionStateForTest(ConnectionState::Connected);
    local.setRfKitEnabled(true);
    RfKitPage page(&local);
    QVERIFY(page.setTciButtonForTesting()->isEnabled());
    local.rfKitConnection()->connectToAmp(QStringLiteral("127.0.0.1"), amp.serverPort());
    NEREUS_TRY_VERIFY_WITH_TIMEOUT(local.rfKitConnection()->isConnected(), 5000);
    NEREUS_TRY_VERIFY_WITH_TIMEOUT(page.diagnosticsTextForTesting().contains(QStringLiteral("Connected since")),
                             3000);
    QVERIFY(page.diagnosticsTextForTesting().contains(QStringLiteral("Last poll")));
    QVERIFY(page.diagnosticsTextForTesting().contains(QStringLiteral("RTT")));
    QVERIFY(!page.diagnosticsTextForTesting().contains(QStringLiteral("Connected since --")));
    page.setTciButtonForTesting()->click();
    NEREUS_TRY_VERIFY(amp.writes.contains(QStringLiteral(
        R"(PUT /operational-interface {"operational_interface":"TCI"})")));
    local.rfKitConnection()->disconnect();
    AppSettings::instance().clear();
}

// Group B fix wave (M1): RadioModel is MainWindow's first child, so at quit
// it goes before a Setup dialog that is still open. A remote window's
// Peripherals page with an unsent Tuner Genius address then sends
// nothing from its destructor: the model it would ask is gone.
void RemotePeripheralsTest::pageThatOutlivesItsModelSendsNothing()
{
    RecordingTgxlLink link;
    link.linkReady = true;
    link.available = true;
    link.tgxlControl = true;
    link.tgxlFull = true;
    auto model = std::make_unique<RadioModel>(RadioModel::Role::Remote);
    model->attachStation(&link);
    auto page = std::make_unique<PeripheralsPage>(model.get());
    model->reportStationLinkStateChanged();
    auto* host = page->findChild<QLineEdit*>(QStringLiteral("tgxlHostEdit"));
    QVERIFY(host);
    QTest::keyClicks(host, QStringLiteral("192.0.2.5"));
    QVERIFY(host->text().contains(QStringLiteral("192.0.2.5")));

    model.reset();
    QVERIFY(!page->hasModelForTest());
    page.reset();
    QCOMPARE(link.tgxlAddressCalls, 0);
}

// Group B fix wave (M5, the operator's ruling 2026-09-25, "block them in
// both windows"): a local window's Power Genius OPERATE (the applet and the
// PowerGenius XL tab), RF-Kit OPERATE and antennas, and Tuner Genius relays
// and switches follow the remote window's rule. While the radio is on the
// air each is disabled with RadioModel::onAirReason(), and a press that
// gets through anyway sends nothing; off the air each works as before.
void RemotePeripheralsTest::localWindowAmpAndTunerSwitchesWaitOnTheAir()
{
    AppSettings::instance().clear();
    const QString onAir = RadioModel::onAirReason();
    QVERIFY(OperatorWording::isPlain(onAir));
    const auto pressDisabled = [](QPushButton* button) {
        button->setEnabled(true);   // a press that gets through anyway
        button->click();
    };

    // The Power Genius and the RF-Kit on this computer.
    FakeGenius amp;
    QVERIFY(amp.listen());
    RadioModel local;
    AmpApplet ampApplet(&local);
    PgxlAdvancedPage tab(&local);
    Rf2ksApplet rfKit(&local);
    QSignalSpy ampOperate(&ampApplet, &AmpApplet::operateToggled);
    QSignalSpy rfKitOperate(&rfKit, &Rf2ksApplet::operateToggled);
    QSignalSpy rfKitAntenna(&rfKit, &Rf2ksApplet::antennaRequested);
    local.pgxlConnection()->connectToPgxl(QStringLiteral("127.0.0.1"), amp.port());
    QVERIFY(amp.accept());
    amp.send(QStringLiteral("V3.8.9"));
    NEREUS_TRY_VERIFY(local.pgxlConnection()->isConnected());
    amp.send(QStringLiteral("S0|status state=STANDBY"));
    QPushButton* tabOperate = tab.operateButtonForTesting();
    NEREUS_TRY_VERIFY(tabOperate->isEnabled());
    QVERIFY(ampApplet.operateButtonEnabledForTesting());
    QVERIFY(rfKit.operateButtonEnabledForTesting());
    QVERIFY(rfKit.antennaButtonIsEnabledForTesting(1));
    QPushButton* ampButton = nullptr;
    QPushButton* rfKitButton = nullptr;
    for (QPushButton* b : ampApplet.findChildren<QPushButton*>()) {
        if (b->text() == QStringLiteral("OPERATE")) { ampButton = b; }
    }
    for (QPushButton* b : rfKit.findChildren<QPushButton*>()) {
        if (b->text() == QStringLiteral("STANDBY")) { rfKitButton = b; }
    }
    QVERIFY(ampButton && rfKitButton);
    QPushButton* rfKitAnt1 = nullptr;
    for (QPushButton* b : rfKit.findChildren<QPushButton*>()) {
        if (b->text() == QStringLiteral("ANT 1")) { rfKitAnt1 = b; }
    }
    QVERIFY(rfKitAnt1);

    MoxController* const mox = local.moxController();
    QVERIFY(mox);
    mox->setMoxCheck({});
    mox->setMox(true);
    NEREUS_TRY_VERIFY(local.isCoreOnAir());
    NEREUS_TRY_VERIFY(!ampApplet.operateButtonEnabledForTesting());
    QCOMPARE(ampApplet.operateButtonToolTipForTesting(), onAir);
    NEREUS_TRY_VERIFY(!tabOperate->isEnabled());
    QCOMPARE(tabOperate->toolTip(), onAir);
    QVERIFY(!rfKit.operateButtonEnabledForTesting());
    QCOMPARE(rfKit.operateButtonToolTipForTesting(), onAir);
    for (int n = 1; n <= 4; ++n) {
        QVERIFY(!rfKit.antennaButtonIsEnabledForTesting(n));
        QCOMPARE(rfKit.antennaButtonToolTipForTesting(n), onAir);
    }
    const int ampLines = amp.commands.filter(QRegularExpression(QStringLiteral("^operate"))).size();
    pressDisabled(ampButton);
    pressDisabled(tabOperate);
    pressDisabled(rfKitButton);
    pressDisabled(rfKitAnt1);
    NereusSDR::Test::settleSession();
    QCOMPARE(ampOperate.count(), 0);
    QCOMPARE(rfKitOperate.count(), 0);
    QCOMPARE(rfKitAntenna.count(), 0);
    QCOMPARE(amp.commands.filter(QRegularExpression(QStringLiteral("^operate"))).size(), ampLines);
    // The press put each back the way the rule has it.
    QVERIFY(!ampApplet.operateButtonEnabledForTesting());
    QVERIFY(!tabOperate->isEnabled());
    QVERIFY(!rfKit.operateButtonEnabledForTesting());
    QVERIFY(!rfKit.antennaButtonIsEnabledForTesting(1));

    mox->setMox(false);
    NEREUS_TRY_VERIFY(!local.isCoreOnAir());
    NEREUS_TRY_VERIFY(ampApplet.operateButtonEnabledForTesting());
    QVERIFY(ampApplet.operateButtonToolTipForTesting().isEmpty());
    NEREUS_TRY_VERIFY(tabOperate->isEnabled());
    QVERIFY(rfKit.operateButtonEnabledForTesting());
    QVERIFY(rfKit.operateButtonToolTipForTesting().isEmpty());
    QVERIFY(rfKit.antennaButtonIsEnabledForTesting(1));
    NEREUS_TRY_VERIFY(!local.stationOnAirRefusal(nullptr));
    ampApplet.clickOperateForTesting();
    QCOMPARE(ampOperate.count(), 1);
    tabOperate->click();
    QVERIFY(amp.waitFor(QStringLiteral("operate=1")) >= 0);
    rfKit.clickOperateButtonForTesting();
    QCOMPARE(rfKitOperate.count(), 1);
    rfKit.clickAntennaButtonForTesting(1);
    QCOMPARE(rfKitAntenna.count(), 1);
    local.pgxlConnection()->disconnect();

    // The Tuner Genius on this computer: the Core model is a local window's
    // model with its own tuner connection.
    CoreAndWindow cw;
    RadioModel& station = cw.station;
    station.smartSdrListener()->setListenEndpointForTesting(QHostAddress::LocalHost, 0);
    station.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("True"));
    FakeGenius tuner;
    QVERIFY(tuner.listen());
    TunerApplet applet(&station, station.tunerModel());
    // A local window with transmit, as MainWindow sets it (the Core model
    // here is receive-only for its session server).
    applet.setTransmitPermitted(true, QString());
    QVERIFY(admitCoreTuner(station, tuner));
    for (int relay = 0; relay < 3; ++relay) {
        NEREUS_TRY_VERIFY(applet.relayBarForTesting(relay)->isScrollEnabled());
    }
    QVERIFY(applet.operateButtonForTesting()->isEnabled());
    // Task 77 fix round 2: TUNE (it switches the Power Genius to standby
    // first) follows the same on-air rule as OPERATE and ANT.
    QVERIFY(applet.tuneButtonForTesting()->isEnabled());
    MoxController* const coreMox = station.moxController();
    coreMox->setMoxCheck({});
    coreMox->setMox(true);
    NEREUS_TRY_VERIFY(station.isCoreOnAir());
    NEREUS_TRY_VERIFY(!applet.tuneButtonForTesting()->isEnabled());
    QCOMPARE(applet.tuneButtonForTesting()->toolTip(), onAir);
    for (int relay = 0; relay < 3; ++relay) {
        NEREUS_TRY_VERIFY(!applet.relayBarForTesting(relay)->isScrollEnabled());
        QCOMPARE(applet.relayBarForTesting(relay)->toolTip(), onAir);
    }
    QVERIFY(!applet.operateButtonForTesting()->isEnabled());
    QCOMPARE(applet.operateButtonForTesting()->toolTip(), onAir);
    for (int port = 1; port <= 3; ++port) {
        QVERIFY(!applet.antennaButtonForTesting(port)->isEnabled());
        QCOMPARE(applet.antennaButtonForTesting(port)->toolTip(), onAir);
    }
    // What the tuner is asked to do, less the connection's own status and
    // info polls.
    const auto switchCommands = [&tuner](int from) {
        QStringList out;
        for (const QString& c : tuner.commands.mid(from)) {
            if (c != QLatin1String("status") && c != QLatin1String("info")) { out << c; }
        }
        return out;
    };
    int mark = tuner.commands.size();
    applet.relayBarForTesting(0)->setScrollEnabled(true);   // gets through anyway
    wheel(applet.relayBarForTesting(0), 120);
    pressDisabled(applet.operateButtonForTesting());
    pressDisabled(applet.antennaButtonForTesting(2));
    NereusSDR::Test::settleSession();
    QVERIFY2(switchCommands(mark).isEmpty(), qPrintable(switchCommands(mark).join(u',')));

    coreMox->setMox(false);
    NEREUS_TRY_VERIFY(!station.isCoreOnAir());
    NEREUS_TRY_VERIFY(!station.stationOnAirRefusal(nullptr));
    for (int relay = 0; relay < 3; ++relay) {
        NEREUS_TRY_VERIFY(applet.relayBarForTesting(relay)->isScrollEnabled());
        QVERIFY(applet.relayBarForTesting(relay)->toolTip().isEmpty());
    }
    QVERIFY(applet.operateButtonForTesting()->isEnabled());
    NEREUS_TRY_VERIFY(applet.tuneButtonForTesting()->isEnabled());
    QVERIFY(applet.tuneButtonForTesting()->toolTip().isEmpty());
    mark = tuner.commands.size();
    wheel(applet.relayBarForTesting(0), 120);
    QVERIFY(tuner.waitFor(QStringLiteral("tune relay=0 move=1"), mark) >= 0);
    QVERIFY(!station.transmitModel().isTune());
}

// Parity mini-round (the operator's rulings a and c, 2026-09-25). A local
// window's "Set amp to TCI mode" switches the amp, so it waits on the air
// as the other amp and tuner switches do. And a local click that the
// Core's own rule refuses while the window already shows the radio off
// the air (isCoreOnAir() false, stationOnAirRefusal() still true, as in
// the hand-back to receive; the transmit model's MOX latch stands in for
// it here) is never a silent drop: it goes out on accessoryRequestRefused,
// which MainWindow shows, with the words a remote window gets from its
// Core. Nothing reaches the amp or the tuner.
void RemotePeripheralsTest::localClickRefusedAsTheRadioUnkeysSaysWhy()
{
    AppSettings::instance().clear();
    const QString onAir = RadioModel::onAirReason();
    FakeGenius amp;
    QVERIFY(amp.listen());
    RadioModel local;
    AmpApplet ampApplet(&local);
    PgxlAdvancedPage tab(&local);
    Rf2ksApplet rfKit(&local);
    RfKitPage rfKitPage(&local);
    TunerApplet tuner(&local, local.tunerModel());
    tuner.setTransmitPermitted(true, QString());
    QSignalSpy refused(&local, &RadioModel::accessoryRequestRefused);
    QSignalSpy ampOperate(&ampApplet, &AmpApplet::operateToggled);
    QSignalSpy rfKitOperate(&rfKit, &Rf2ksApplet::operateToggled);
    QSignalSpy rfKitAntenna(&rfKit, &Rf2ksApplet::antennaRequested);
    local.pgxlConnection()->connectToPgxl(QStringLiteral("127.0.0.1"), amp.port());
    QVERIFY(amp.accept());
    amp.send(QStringLiteral("V3.8.9"));
    NEREUS_TRY_VERIFY(local.pgxlConnection()->isConnected());
    amp.send(QStringLiteral("S0|status state=STANDBY"));
    QPushButton* tabOperate = tab.operateButtonForTesting();
    NEREUS_TRY_VERIFY(tabOperate->isEnabled());

    // Ruling a: the local TCI mode button waits on the air with the reason.
    QPushButton* tci = rfKitPage.setTciButtonForTesting();
    QVERIFY(tci->isEnabledTo(tci->parentWidget()));
    MoxController* const mox = local.moxController();
    QVERIFY(mox);
    mox->setMoxCheck({});
    mox->setMox(true);
    NEREUS_TRY_VERIFY(local.isCoreOnAir());
    NEREUS_TRY_VERIFY(!tci->isEnabledTo(tci->parentWidget()));
    QCOMPARE(tci->toolTip(), onAir);
    mox->setMox(false);
    NEREUS_TRY_VERIFY(!local.isCoreOnAir());
    NEREUS_TRY_VERIFY(tci->isEnabledTo(tci->parentWidget()));
    QVERIFY(tci->toolTip().isEmpty());
    NEREUS_TRY_VERIFY(!local.stationOnAirRefusal(nullptr));
    QCOMPARE(refused.count(), 0);

    // Ruling c: the window shows the radio off the air, the Core's rule
    // still refuses.
    local.transmitModel().setMox(true);
    QVERIFY(!local.isCoreOnAir());
    QVERIFY(local.stationOnAirRefusal(nullptr));
    QVERIFY(ampApplet.operateButtonEnabledForTesting());
    QVERIFY(tabOperate->isEnabled());
    QVERIFY(rfKit.operateButtonEnabledForTesting());
    QVERIFY(tci->isEnabledTo(tci->parentWidget()));
    const int ampLines = amp.commands.filter(QRegularExpression(QStringLiteral("^operate"))).size();
    const auto expectReason = [&](int count, const QString& device) {
        QCOMPARE(refused.count(), count);
        QCOMPARE(refused.last().at(0).toString(), device);
        QCOMPARE(refused.last().at(1).toString(), onAir);
        QCOMPARE(refused.last().at(2).toBool(), false);
    };
    ampApplet.clickOperateForTesting();
    expectReason(1, QStringLiteral("pgxl"));
    tabOperate->click();
    expectReason(2, QStringLiteral("pgxl"));
    rfKit.clickOperateButtonForTesting();
    expectReason(3, QStringLiteral("rfkit"));
    rfKit.clickAntennaButtonForTesting(1);
    expectReason(4, QStringLiteral("rfkit"));
    emit tci->clicked();   // its tab waits for a radio; the button's own click
    expectReason(5, QStringLiteral("rfkit"));
    QPushButton* tunerAnt = tuner.antennaButtonForTesting(2);
    tunerAnt->setEnabled(true);
    tunerAnt->click();
    expectReason(6, QStringLiteral("tgxl"));
    tuner.operateButtonForTesting()->setEnabled(true);
    tuner.operateButtonForTesting()->click();
    expectReason(7, QStringLiteral("tgxl"));
    tuner.relayBarForTesting(0)->setScrollEnabled(true);
    wheel(tuner.relayBarForTesting(0), 120);
    expectReason(8, QStringLiteral("tgxl"));
    NereusSDR::Test::settleSession();
    QCOMPARE(ampOperate.count(), 0);
    QCOMPARE(rfKitOperate.count(), 0);
    QCOMPARE(rfKitAntenna.count(), 0);
    QCOMPARE(amp.commands.filter(QRegularExpression(QStringLiteral("^operate"))).size(), ampLines);

    // Off the air again: the same clicks go ahead with nothing refused.
    local.transmitModel().setMox(false);
    QVERIFY(!local.stationOnAirRefusal(nullptr));
    ampApplet.clickOperateForTesting();
    QCOMPARE(ampOperate.count(), 1);
    rfKit.clickOperateButtonForTesting();
    QCOMPARE(rfKitOperate.count(), 1);
    QCOMPARE(refused.count(), 8);
    local.pgxlConnection()->disconnect();
}

// iPhone app plan Task 77 fix round 3 (R-IOS-02, R-IOS-03, R-IOS-13): the
// Power Genius's OPERATE and STANDBY wait while the Tuner Genius tunes, in
// a local window (the applet, its 4O3A tab, a press that gets through
// anyway refused with the words) and in a remote window (the Core's tuner,
// and the Core refusing a request sent anyway); an amp in FAULT is sent
// standby from both local buttons.
void RemotePeripheralsTest::ampOperateWaitsForTheTunerAndAFaultedAmpGoesToStandby()
{
    const QString tuning = RadioModel::tunerTuningReason();
    QVERIFY(OperatorWording::isPlain(tuning));
    {
        AppSettings::instance().clear();
        FakeGenius amp;
        QVERIFY(amp.listen());
        RadioModel local;
        AmpApplet applet(&local);
        PgxlAdvancedPage tab(&local);
        QSignalSpy toggles(&applet, &AmpApplet::operateToggled);
        QSignalSpy refused(&local, &RadioModel::accessoryRequestRefused);
        const auto operateLines = [&amp] {
            return amp.commands.filter(QRegularExpression(QStringLiteral("^operate")));
        };
        local.pgxlConnection()->connectToPgxl(QStringLiteral("127.0.0.1"), amp.port());
        QVERIFY(amp.accept());
        amp.send(QStringLiteral("V3.8.9"));
        NEREUS_TRY_VERIFY(local.pgxlConnection()->isConnected());
        amp.send(QStringLiteral("S0|status state=STANDBY"));
        QPushButton* tabOperate = tab.operateButtonForTesting();
        NEREUS_TRY_VERIFY(tabOperate->isEnabled());
        QVERIFY(applet.operateButtonEnabledForTesting());
        NEREUS_TRY_COMPARE(applet.operateButtonTextForTesting(), QStringLiteral("STANDBY"));
        QPushButton* ampButton = nullptr;
        for (QPushButton* b : applet.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("STANDBY")) { ampButton = b; }
        }
        QVERIFY(ampButton);

        // The tuner reports its sweep: both wait, saying why.
        local.tgxlConnection()->injectLineForTesting(QStringLiteral("V1.2.17"));
        local.tgxlConnection()->injectLineForTesting(QStringLiteral("S0|state tuning=1"));
        NEREUS_TRY_VERIFY(local.pgxlSwitchWaitsForTuner());
        NEREUS_TRY_VERIFY(!applet.operateButtonEnabledForTesting());
        QCOMPARE(applet.operateButtonToolTipForTesting(), tuning);
        QVERIFY(!tabOperate->isEnabled());
        QCOMPARE(tabOperate->toolTip(), tuning);
        // A press that gets through anyway: refused with the words.
        for (QPushButton* button : {ampButton, tabOperate}) {
            button->setEnabled(true);
            button->click();
            QCOMPARE(refused.count(), 1);
            QCOMPARE(refused.last().at(1).toString(), tuning);
            refused.clear();
        }
        QCOMPARE(toggles.count(), 0);
        NereusSDR::Test::settleSession();
        QVERIFY(operateLines().isEmpty());
        local.tgxlConnection()->injectLineForTesting(QStringLiteral("S0|state tuning=0"));
        NEREUS_TRY_VERIFY(applet.operateButtonEnabledForTesting());
        NEREUS_TRY_VERIFY(tabOperate->isEnabled());

        // A fault: the applet's button reads STANDBY and sends standby;
        // the tab offers Standby and sends operate=0.
        amp.send(QStringLiteral("S0|status state=FAULT"));
        NEREUS_TRY_COMPARE(local.amplifierModel()->state(), AmplifierModel::State::Fault);
        QCOMPARE(applet.operateButtonTextForTesting(), QStringLiteral("STANDBY"));
        QVERIFY(OperatorWording::isPlain(applet.operateButtonToolTipForTesting()));
        QVERIFY(!applet.operateButtonToolTipForTesting().isEmpty());
        applet.clickOperateForTesting();
        QCOMPARE(toggles.count(), 1);
        QCOMPARE(toggles.last().at(0).toBool(), false);
        NEREUS_TRY_COMPARE(tabOperate->text(), QStringLiteral("Standby"));
        QVERIFY(OperatorWording::isPlain(tabOperate->toolTip()));
        tabOperate->click();
        QVERIFY(amp.waitFor(QStringLiteral("operate=0")) >= 0);
        QCOMPARE(operateLines(), QStringList{QStringLiteral("operate=0")});
        amp.send(QStringLiteral("S0|status state=STANDBY"));
        NEREUS_TRY_VERIFY(!local.ampChangingOver());

        // Round 4 (Minor 3): an amp that took operate=1 and keeps reporting
        // standby: both buttons now send standby, which the next STANDBY
        // report confirms, ending the wait.
        NEREUS_TRY_COMPARE(tabOperate->text(), QStringLiteral("Operate"));
        tabOperate->click();
        QVERIFY(amp.waitFor(QStringLiteral("operate=1")) >= 0);
        QVERIFY(local.ampOperateUnconfirmed());
        amp.send(QStringLiteral("S0|status state=STANDBY"));
        NEREUS_TRY_COMPARE(tabOperate->text(), QStringLiteral("Standby"));
        QVERIFY(OperatorWording::isPlain(tabOperate->toolTip()));
        const int togglesBefore = toggles.count();
        applet.clickOperateForTesting();
        QCOMPARE(toggles.count(), togglesBefore + 1);
        QCOMPARE(toggles.last().at(0).toBool(), false);
        tabOperate->click();
        NEREUS_TRY_COMPARE(operateLines().size(), 3);
        QCOMPARE(operateLines().last(), QStringLiteral("operate=0"));
        amp.send(QStringLiteral("S0|status state=STANDBY"));
        NEREUS_TRY_VERIFY(!local.ampChangingOver());
        NEREUS_TRY_COMPARE(tabOperate->text(), QStringLiteral("Operate"));
    }

    // A remote window: the Core's tuner sweeping greys OPERATE with the
    // words, and a request sent anyway is refused by the Core with them.
    AppSettings::instance().clear();
    CoreAndWindow cw;
    QVERIFY(cw.pairTransmitWindow());
    RadioModel& station = cw.station;
    RadioModel& window = cw.window;
    station.smartSdrListener()->setListenEndpointForTesting(QHostAddress::LocalHost, 0);
    station.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("True"));
    FakeGenius amp;
    QVERIFY(amp.listen());
    AmpApplet applet(&window);
    FourO3APage page(&window);
    auto* tabOperate = page.findChild<QPushButton*>(QStringLiteral("remotePgxlOperateButton"));
    QVERIFY(tabOperate);
    QSignalSpy refused(&window, &RadioModel::accessoryRequestRefused);
    QSignalSpy sliceRefused(&window, &RadioModel::sliceAddRejected);
    cw.connect(this);
    NEREUS_TRY_VERIFY(cw.client.pgxlFullControlAvailable());
    NEREUS_TRY_VERIFY(cw.client.capabilities().txPermitted);
    window.reportStationLinkStateChanged();
    QVERIFY(admitCoreAmp(station, amp));
    amp.send(QStringLiteral("S0|status state=STANDBY"));
    NEREUS_TRY_VERIFY(applet.operateButtonEnabledForTesting());
    station.tunerModel()->applyStationValue(QByteArrayLiteral("isTuning"), true);
    NEREUS_TRY_VERIFY(window.tunerModel()->isTuning());
    NEREUS_TRY_VERIFY(!applet.operateButtonEnabledForTesting());
    QCOMPARE(applet.operateButtonToolTipForTesting(), tuning);
    NEREUS_TRY_VERIFY(!tabOperate->isEnabled());
    QCOMPARE(tabOperate->toolTip(), tuning);
    QVERIFY(cw.client.requestPgxlOperate(true).sent);
    NEREUS_TRY_COMPARE(refused.count(), 1);
    QCOMPARE(refused.last().at(0).toString(), QStringLiteral("pgxl"));
    QCOMPARE(refused.last().at(1).toString(), tuning);
    NereusSDR::Test::settleSession();
    QCOMPARE(refused.count(), 1);
    QVERIFY(sliceRefused.isEmpty());
    QVERIFY(amp.commands.filter(QRegularExpression(QStringLiteral("^operate"))).isEmpty());
    station.tunerModel()->applyStationValue(QByteArrayLiteral("isTuning"), false);
    NEREUS_TRY_VERIFY(applet.operateButtonEnabledForTesting());
    NEREUS_TRY_VERIFY(tabOperate->isEnabled());
    // A fault: the remote tab offers Standby.
    amp.send(QStringLiteral("S0|status state=FAULT"));
    NEREUS_TRY_COMPARE(tabOperate->text(), QStringLiteral("Standby"));
    QVERIFY(OperatorWording::isPlain(tabOperate->toolTip()));
}

QTEST_MAIN(RemotePeripheralsTest)
#include "tst_remote_peripherals.moc"
