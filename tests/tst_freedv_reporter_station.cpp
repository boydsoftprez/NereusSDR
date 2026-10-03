// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_freedv_reporter_station.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 22 and remote-window parity Task 20 (R-IOS-26,
// R-R3-49; acceptance B7.3 and B7.4): FreeDV Reporter at the Core.
//
//   - The Core registers from its own settings, with no window: its
//     StationCallsign (when no Spot Hub identity is set), grid square and
//     message. Its label never appears in anything it sends.
//   - The Core's list reaches a remote window as the freedvStations stream,
//     with the Core's distance and heading, the station's transmit and
//     receive state and when its message last changed.
//   - A window's status message, QSY request and "Hide my station" reach
//     the Core, which sends them; a QSY request goes to the named station;
//     a refusal comes back in plain words.
//   - FreeDV Reporter's state is in `spotSources`, the same shape as the
//     other sources: off with the plain reason when it could not start.
//   - The listed frequency follows the Core's RADE slice, and a switch into
//     RADE shows the station; "Hide my station" keeps it hidden.
//   - Each freedvStations record carries the station's band
//     (stationFreedvVersion 2), the band the desktop's own lookup gives its
//     frequency, as a remote window's band filter reads it.
//   - In a real remote window, the FreeDV tab and the FreeDV Reporter
//     dialog wait for the Core, then list its stations with distance and
//     heading at once, with no Save & Propagate.
//
// A fake Socket.IO server on 127.0.0.1 only; nothing reaches
// qso.freedv.org. No RF, no audio device.
//
//   cmake --build build --target tst_freedv_reporter_station
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build \
//       -R '^tst_freedv_reporter_station$' --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-28: each station's band on its record (R-IOS-26,
//               stationFreedvVersion 2). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29: a remote window's VFO flag follows the Core's RADE sync,
//               SNR and offset (radeStatus 1). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QAbstractItemModel>
#include <QCheckBox>
#include <QLabel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableView>
#include <QTemporaryDir>
#include <QWebSocket>
#include <QWebSocketServer>

#include "core/AppSettings.h"
#include "core/FreeDVRadeReporterBridge.h"
#include "core/FreeDVReporterClient.h"
#include "core/FreeDVStation.h"
#include "core/SpotSourceHost.h"
#include "core/security/StationLabel.h"
#include "core/session/RecordStream.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/RemoteWindowHarness.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/FreeDVReporterDialog.h"
#include "gui/SpotHubDialog.h"
#include "gui/widgets/VfoWidget.h"
#include "models/Band.h"
#include "models/FreeDVStationModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;
using NereusSDR::Test::RemoteWindowHarness;

namespace {

const QString kLabelSuffix = QStringLiteral("shack-pi-secret");

// A Socket.IO server as qso.freedv.org speaks it (Engine.IO v4 open,
// Socket.IO connect ACK, events), on 127.0.0.1. It records every text the
// client sends.
class FakeReporterServer : public QObject {
public:
    FakeReporterServer()
        : m_server(QStringLiteral("fake-freedv-reporter"), QWebSocketServer::NonSecureMode)
    {
        QObject::connect(&m_server, &QWebSocketServer::newConnection, this, [this]() {
            while (QWebSocket* socket = m_server.nextPendingConnection()) {
                socket->setParent(this);
                m_socket = socket;
                QObject::connect(socket, &QWebSocket::textMessageReceived, this,
                                 [this](const QString& text) { onText(text); });
                // Engine.IO open.
                socket->sendTextMessage(QStringLiteral(
                    "0{\"sid\":\"eio\",\"upgrades\":[],\"pingInterval\":25000,"
                    "\"pingTimeout\":20000}"));
            }
        });
    }
    bool listen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    QString url() const
    {
        return QStringLiteral("ws://127.0.0.1:%1/socket.io/?EIO=4&transport=websocket")
            .arg(m_server.serverPort());
    }
    // Every event the client sent, as [name, payload].
    QList<QJsonArray> events() const
    {
        QList<QJsonArray> out;
        for (const QString& text : m_received) {
            if (text.startsWith(QStringLiteral("42"))) {
                out.append(QJsonDocument::fromJson(text.mid(2).toUtf8()).array());
            }
        }
        return out;
    }
    QList<QJsonArray> eventsNamed(const QString& name) const
    {
        QList<QJsonArray> out;
        for (const QJsonArray& e : events()) {
            if (!e.isEmpty() && e.at(0).toString() == name) {
                out.append(e);
            }
        }
        return out;
    }
    QJsonObject auth() const
    {
        for (const QString& text : m_received) {
            if (text.startsWith(QStringLiteral("40"))) {
                return QJsonDocument::fromJson(text.mid(2).toUtf8()).object();
            }
        }
        return {};
    }
    const QStringList& received() const { return m_received; }
    // A Socket.IO event from the server.
    void push(const QString& name, const QJsonObject& data)
    {
        if (m_socket) {
            m_socket->sendTextMessage(
                QStringLiteral("42")
                + QString::fromUtf8(QJsonDocument(QJsonArray{name, data})
                                        .toJson(QJsonDocument::Compact)));
        }
    }

private:
    void onText(const QString& text)
    {
        m_received.append(text);
        if (text.startsWith(QStringLiteral("40")) && m_socket) {
            m_socket->sendTextMessage(QStringLiteral("40{\"sid\":\"core-session\"}"));
        }
    }

    QWebSocketServer m_server;
    QPointer<QWebSocket> m_socket;
    QStringList m_received;
};

// The Core's own settings: its callsign and label, no Spot Hub identity.
void setCoreIdentity(const QString& serverUrl)
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("StationCallsign"), QStringLiteral("KG4VCF"));
    s.setValue(QString::fromLatin1(StationLabel::kSettingsKey),
               QStringLiteral("KG4VCF/") + kLabelSuffix);
    s.setValue(QStringLiteral("User/GridSquare"), QStringLiteral("EM73"));
    s.setValue(QStringLiteral("FreeDvReporter/Message"), QStringLiteral("QRV from the shack"));
    s.setValue(QStringLiteral("FreeDvReporter/ServerUrl"), serverUrl);
    s.setValue(QStringLiteral("FreeDvAutoStart"), QStringLiteral("True"));
}

std::unique_ptr<RadioModel> makeCore()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::HermesLite);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:22");
    info.name = QStringLiteral("Bench HL2");
    info.boardType = HPSDRHW::HermesLite;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    return model;
}

// A remote window's VFO flag for a slice.
VfoWidget* remoteFlag(MainWindow& window, int sliceId)
{
    for (VfoWidget* flag : window.findChildren<VfoWidget*>()) {
        if (flag->sliceIndex() == sliceId) {
            return flag;
        }
    }
    return nullptr;
}

QJsonObject station(const QString& sid, const QString& call, const QString& grid)
{
    return QJsonObject{{QStringLiteral("sid"), sid},
                       {QStringLiteral("callsign"), call},
                       {QStringLiteral("grid_square"), grid},
                       {QStringLiteral("version"), QStringLiteral("FreeDV 2.0")},
                       {QStringLiteral("rx_only"), false},
                       {QStringLiteral("last_update"), QStringLiteral("2026-09-27T12:00:00Z")}};
}

struct Session {
    Session(RadioModel* core, const QString& securityDir, QObject* parent)
        : settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")))
        , coreModel(core)
    {
        server = std::make_unique<StationServer>(
            coreModel, settings, NereusSDR::Test::seedUpgradedCoreToken(securityDir));
        client = std::make_unique<StationClient>(&window, &proxy);
        coreEnd = new LoopbackTransport(QStringLiteral("station-end"), parent);
        windowEnd = new LoopbackTransport(QStringLiteral("client-end"), parent);
        coreEnd->linkTo(windowEnd);
    }
    bool connect()
    {
        QSignalSpy completed(client.get(), &StationClient::handshakeComplete);
        client->startSession(windowEnd, server->token());
        server->acceptTransport(coreEnd);
        return completed.wait(5000) || completed.count() == 1;
    }

    QTemporaryDir settingsDir;
    AppSettings settings;
    RadioModel* coreModel = nullptr;
    std::unique_ptr<StationServer> server;
    RadioModel window{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    LoopbackTransport* coreEnd = nullptr;
    LoopbackTransport* windowEnd = nullptr;
};

} // namespace

class TstFreedvReporterStation : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // The reporter's log file lands in the test's own location.
        QStandardPaths::setTestModeEnabled(true);
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(m_securityDir.isValid());
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("freedv-station")));
    }

    void init() { QVERIFY(RemoteWindowHarness::clearIsolatedProfile()); }

    void cleanupTestCase() { QVERIFY(RemoteWindowHarness::removeIsolatedProfile()); }

    // B7.3, R-IOS-26: the Core registers with no window, from its own
    // settings, and its label never leaves it.
    void theCoreRegistersWithItsCallsignAndNeverItsLabel()
    {
        FakeReporterServer fake;
        QVERIFY(fake.listen());
        setCoreIdentity(fake.url());
        std::unique_ptr<RadioModel> core = makeCore();
        SpotSourceHost* host = core->spotSourceHost();
        core->restoreStationSpotSources();
        QTRY_VERIFY_WITH_TIMEOUT(core->freeDvReporter()->isConnected(), 5000);
        QCOMPARE(host->freedvReporterState(), SpotSourceHost::kConnected);

        const QJsonObject auth = fake.auth();
        QCOMPARE(auth.value(QStringLiteral("role")).toString(), QStringLiteral("report"));
        QCOMPARE(auth.value(QStringLiteral("callsign")).toString(), QStringLiteral("KG4VCF"));
        QCOMPARE(auth.value(QStringLiteral("grid_square")).toString(), QStringLiteral("EM73"));
        QTRY_VERIFY(!fake.eventsNamed(QStringLiteral("message_update")).isEmpty()
                    || !fake.eventsNamed(QStringLiteral("hide_self")).isEmpty());

        // Everything a station sends while it runs: its message, a QSY
        // request, hiding and showing.
        fake.push(QStringLiteral("new_connection"),
                  station(QStringLiteral("sid-w1aw"), QStringLiteral("W1AW"),
                          QStringLiteral("FN31")));
        QTRY_COMPARE(core->freeDvStationModel()->stationCount(), 1);
        QString reason;
        QVERIFY(host->setFreedvMessage(QStringLiteral("CQ from the Core"), &reason));
        QVERIFY(host->sendFreedvQsy(QStringLiteral("W1AW"), 14236000, &reason));
        QVERIFY(host->setFreedvHidden(true, &reason));
        QVERIFY(host->setFreedvHidden(false, &reason));
        core->sliceById(0)->setDspMode(DSPMode::RADE_U);
        QTRY_VERIFY(!fake.eventsNamed(QStringLiteral("show_self")).isEmpty());
        QTRY_VERIFY(!fake.eventsNamed(QStringLiteral("qsy_request")).isEmpty());

        const QString label = QStringLiteral("KG4VCF/") + kLabelSuffix;
        QVERIFY(fake.received().size() > 3);
        for (const QString& text : fake.received()) {
            QVERIFY2(!text.contains(kLabelSuffix, Qt::CaseInsensitive), qPrintable(text));
            QVERIFY2(!text.contains(label, Qt::CaseInsensitive), qPrintable(text));
        }
    }

    // The state in `spotSources`: off with the plain reason when there is no
    // identity to register with.
    void withNoIdentityTheCoreSaysWhy()
    {
        AppSettings::instance().setValue(QStringLiteral("FreeDvAutoStart"),
                                         QStringLiteral("True"));
        std::unique_ptr<RadioModel> core = makeCore();
        core->restoreStationSpotSources();
        SpotSourceHost* host = core->spotSourceHost();
        QCOMPARE(host->freedvReporterState(), SpotSourceHost::kOff);
        QCOMPARE(host->freedvReporterText(),
                 QStringLiteral("Enter your callsign and grid square in Spot Hub first."));
        QVERIFY(!core->freeDvReporter()->isConnected());
        QString reason;
        QVERIFY(!host->connectSource(SpotSourceHost::kFreedvReporter, &reason));
        QCOMPARE(reason, QStringLiteral("Enter your callsign and grid square in Spot Hub first."));
    }

    // B7.3: the Core's list in a remote window, with the Core's distance and
    // heading, transmit and receive state and the message's change time.
    void theCoresListReachesAWindow()
    {
        FakeReporterServer fake;
        QVERIFY(fake.listen());
        setCoreIdentity(fake.url());
        std::unique_ptr<RadioModel> core = makeCore();
        core->freeDvStationModel()->setClockForTest([]() { return qint64(1790000000000); });
        core->restoreStationSpotSources();
        QTRY_VERIFY_WITH_TIMEOUT(core->freeDvReporter()->isConnected(), 5000);
        fake.push(QStringLiteral("new_connection"),
                  station(QStringLiteral("sid-w1aw"), QStringLiteral("W1AW"),
                          QStringLiteral("FN31")));
        QTRY_COMPARE(core->freeDvStationModel()->stationCount(), 1);

        Session s(core.get(), m_securityDir.path(), this);
        QCOMPARE(s.server->stationFreedvVersion(), 2);
        QVERIFY(s.connect());
        QCOMPARE(s.client->capabilities().stationFreedvVersion, 2);
        QVERIFY(s.client->stationFreedvAvailable());
        FreeDVStationModel* windowList = s.window.freeDvStationModel();
        QVERIFY(!windowList->computesDistance());

        // The backlog, with the Core's distance and heading (EM73 to FN31).
        QTRY_COMPARE(windowList->stationCount(), 1);
        FreeDVStation w1aw = windowList->stationBySid(QStringLiteral("sid-w1aw"));
        QCOMPARE(w1aw.callsign, QStringLiteral("W1AW"));
        QVERIFY(w1aw.distanceKm > 1000.0);
        QCOMPARE(w1aw.distanceKm,
                 core->freeDvStationModel()->stationBySid(QStringLiteral("sid-w1aw")).distanceKm);
        QVERIFY(!w1aw.headingCardinal.isEmpty());
        // The window's own grid never replaces the Core's numbers.
        windowList->setOurGridSquare(QStringLiteral("JO01"));
        QCOMPARE(windowList->stationBySid(QStringLiteral("sid-w1aw")).distanceKm,
                 w1aw.distanceKm);

        // Frequency, transmitting, a receive report and a message.
        fake.push(QStringLiteral("freq_change"),
                  QJsonObject{{QStringLiteral("sid"), QStringLiteral("sid-w1aw")},
                              {QStringLiteral("freq"), 14236000},
                              {QStringLiteral("last_update"),
                               QStringLiteral("2026-09-27T12:00:05Z")}});
        fake.push(QStringLiteral("tx_report"),
                  QJsonObject{{QStringLiteral("sid"), QStringLiteral("sid-w1aw")},
                              {QStringLiteral("mode"), QStringLiteral("RADEV1")},
                              {QStringLiteral("transmitting"), true}});
        fake.push(QStringLiteral("rx_report"),
                  QJsonObject{{QStringLiteral("sid"), QStringLiteral("sid-w1aw")},
                              {QStringLiteral("callsign"), QStringLiteral("VK5DGR")},
                              {QStringLiteral("mode"), QStringLiteral("RADEV1")},
                              {QStringLiteral("snr"), 7}});
        fake.push(QStringLiteral("message_update"),
                  QJsonObject{{QStringLiteral("sid"), QStringLiteral("sid-w1aw")},
                              {QStringLiteral("message"), QStringLiteral("QRV 20 m")}});
        QTRY_COMPARE(windowList->stationBySid(QStringLiteral("sid-w1aw")).userMessage,
                     QStringLiteral("QRV 20 m"));
        w1aw = windowList->stationBySid(QStringLiteral("sid-w1aw"));
        QCOMPARE(w1aw.frequencyHz, quint64(14236000));
        QVERIFY(w1aw.transmitting);
        QCOMPARE(w1aw.txMode, QStringLiteral("RADEV1"));
        QCOMPARE(w1aw.lastRxCallsign, QStringLiteral("VK5DGR"));
        QCOMPARE(w1aw.snrVal, 7);
        QVERIFY(w1aw.lastRxDate.isValid());
        QCOMPARE(windowList->messageChangedAtMs(QStringLiteral("sid-w1aw")),
                 qint64(1790000000000));

        // The record as the phone reads it.
        RecordStream* stream = s.server->recordStreamForTest(QStringLiteral("freedvStations"));
        QVERIFY(stream);
        const QList<RecordUpsert> records = stream->newest(stream->capacity());
        QCOMPARE(records.size(), 1);
        const QJsonObject f = records.constFirst().fields;
        QCOMPARE(records.constFirst().id, QStringLiteral("sid-w1aw"));
        QCOMPARE(f.value(QStringLiteral("transmitting")).toBool(), true);
        QCOMPARE(f.value(QStringLiteral("receivingFrom")).toString(), QStringLiteral("VK5DGR"));
        QCOMPARE(f.value(QStringLiteral("messageChangedAtMs")).toDouble(), 1790000000000.0);
        QCOMPARE(f.value(QStringLiteral("frequencyHz")).toDouble(), 14236000.0);
        QCOMPARE(f.value(QStringLiteral("gridSquare")).toString(), QStringLiteral("FN31"));
        // stationFreedvVersion 2: the band, as the window's own band filter
        // finds it for the frequency it was given.
        QCOMPARE(f.value(QStringLiteral("band")).toInt(-1), static_cast<int>(Band::Band20m));
        QCOMPARE(f.value(QStringLiteral("band")).toInt(-1),
                 static_cast<int>(bandFromFrequency(static_cast<double>(w1aw.frequencyHz))));

        // A station that leaves, and the Core's list starting again.
        fake.push(QStringLiteral("remove_connection"),
                  QJsonObject{{QStringLiteral("sid"), QStringLiteral("sid-w1aw")}});
        QTRY_COMPARE(windowList->stationCount(), 0);
        fake.push(QStringLiteral("new_connection"),
                  station(QStringLiteral("sid-k6aq"), QStringLiteral("K6AQ"),
                          QStringLiteral("CM87")));
        QTRY_COMPARE(windowList->stationCount(), 1);
        core->freeDvReporter()->stopConnection();
        QTRY_COMPARE(windowList->stationCount(), 0);

        // The session ends: the window's list is no longer the Core's.
        fake.push(QStringLiteral("new_connection"),
                  station(QStringLiteral("sid-k6aq"), QStringLiteral("K6AQ"),
                          QStringLiteral("CM87")));
        s.window.clearStationRecords();
        QCOMPARE(windowList->stationCount(), 0);
    }

    // stationFreedvVersion 2 (R-IOS-26): a station's record carries the band
    // the desktop's own Band::bandFromFrequency gives its frequency, numbered
    // as the spots record numbers its band, and none while its frequency is
    // not known (the desktop's band filter lists such a station under All
    // only). Band edges, the gaps between bands, WWV and 2 m.
    void aStationsRecordCarriesTheDesktopsBand_data()
    {
        QTest::addColumn<qint64>("hz");
        QTest::addColumn<int>("expected");  // -1: no band

        const auto row = [](const char* name, qint64 hz, int expected) {
            QTest::newRow(name) << hz << expected;
        };
        const auto b = [](Band band) { return static_cast<int>(band); };
        row("not known", 0, -1);
        row("below 160 m", 1799999, b(Band::GEN));
        row("160 m low edge", 1800000, b(Band::Band160m));
        row("160 m high edge", 2000000, b(Band::Band160m));
        row("above 160 m", 2000001, b(Band::GEN));
        row("gap 160 to 80 m", 3000000, b(Band::GEN));
        row("80 m low edge", 3500000, b(Band::Band80m));
        row("80 m high edge", 4000000, b(Band::Band80m));
        row("60 m low edge", 5330000, b(Band::Band60m));
        row("60 m high edge", 5410000, b(Band::Band60m));
        row("above 60 m", 5410001, b(Band::GEN));
        row("WWV 5 MHz", 5000000, b(Band::WWV));
        row("40 m RADE calling", 7177000, b(Band::Band40m));
        row("40 m high edge", 7300000, b(Band::Band40m));
        row("above 40 m", 7300001, b(Band::GEN));
        row("WWV 10 MHz", 10000000, b(Band::WWV));
        row("30 m high edge", 10150000, b(Band::Band30m));
        row("20 m RADE calling", 14236000, b(Band::Band20m));
        row("above 20 m", 14350001, b(Band::GEN));
        row("17 m low edge", 18068000, b(Band::Band17m));
        row("15 m high edge", 21450000, b(Band::Band15m));
        row("12 m low edge", 24890000, b(Band::Band12m));
        row("10 m high edge", 29700000, b(Band::Band10m));
        row("gap 10 to 6 m", 40000000, b(Band::GEN));
        row("6 m low edge", 50000000, b(Band::Band6m));
        row("6 m high edge", 54000000, b(Band::Band6m));
        row("2 m", 144500000, b(Band::Band2m));
    }

    void aStationsRecordCarriesTheDesktopsBand()
    {
        QFETCH(qint64, hz);
        QFETCH(int, expected);
        FreeDVStation info;
        info.sid = QStringLiteral("sid-band");
        info.callsign = QStringLiteral("W1AW");
        info.frequencyHz = static_cast<quint64>(hz);
        const QJsonObject f = FreeDVStationModel::recordFields(info, 0);
        if (expected < 0) {
            QVERIFY(!f.contains(QStringLiteral("band")));
            return;
        }
        QVERIFY(f.value(QStringLiteral("band")).isDouble());
        QCOMPARE(f.value(QStringLiteral("band")).toInt(), expected);
        // The desktop's band for the frequency a remote window is given.
        const FreeDVStation windowCopy =
            FreeDVStationModel::stationFromRecord(info.sid, f);
        QCOMPARE(static_cast<int>(bandFromFrequency(static_cast<double>(windowCopy.frequencyHz))),
                 expected);
    }

    // A window's message, QSY request and "Hide my station" reach the Core.
    void theWindowsRequestsReachTheCore()
    {
        FakeReporterServer fake;
        QVERIFY(fake.listen());
        setCoreIdentity(fake.url());
        std::unique_ptr<RadioModel> core = makeCore();
        core->restoreStationSpotSources();
        QTRY_VERIFY_WITH_TIMEOUT(core->freeDvReporter()->isConnected(), 5000);
        fake.push(QStringLiteral("new_connection"),
                  station(QStringLiteral("sid-w1aw"), QStringLiteral("W1AW"),
                          QStringLiteral("FN31")));
        fake.push(QStringLiteral("new_connection"),
                  station(QStringLiteral("sid-k6aq"), QStringLiteral("K6AQ"),
                          QStringLiteral("CM87")));
        QTRY_COMPARE(core->freeDvStationModel()->stationCount(), 2);

        Session s(core.get(), m_securityDir.path(), this);
        QVERIFY(s.connect());
        SpotSourceHost* windowHost = s.window.spotSourceHost();
        QTRY_COMPARE(windowHost->freedvReporterState(), SpotSourceHost::kConnected);
        QSignalSpy refused(windowHost, &SpotSourceHost::sourceRefused);

        // The status message.
        windowHost->sendFreedvMessage(QStringLiteral("QRV 14.236"));
        QTRY_VERIFY(([&] {
            const auto events = fake.eventsNamed(QStringLiteral("message_update"));
            return std::any_of(events.cbegin(), events.cend(),
                                [](const QJsonArray& e) {
            return e.at(1).toObject().value(QStringLiteral("message")).toString()
                == QStringLiteral("QRV 14.236");
            });
        }()));

        // A QSY request goes to the named station, by its session.
        windowHost->requestFreedvQsy(QStringLiteral("k6aq"), 14236000);
        QTRY_COMPARE(fake.eventsNamed(QStringLiteral("qsy_request")).size(), 1);
        const QJsonObject qsy =
            fake.eventsNamed(QStringLiteral("qsy_request")).constFirst().at(1).toObject();
        QCOMPARE(qsy.value(QStringLiteral("dest_sid")).toString(), QStringLiteral("sid-k6aq"));
        QCOMPARE(qsy.value(QStringLiteral("frequency")).toInteger(), qint64(14236000));

        // One that is not listed is refused, in plain words, to the window.
        windowHost->requestFreedvQsy(QStringLiteral("K9ZZZ"), 14236000);
        QTRY_COMPARE(refused.count(), 1);
        QCOMPARE(refused.constFirst().at(0).toString(), SpotSourceHost::kFreedvReporter);
        QCOMPARE(refused.constFirst().at(1).toString(),
                 QStringLiteral("K9ZZZ is not on FreeDV Reporter now."));

        // "Hide my station": the Core hides it, saves it, and every window
        // sees it.
        const int hidesBefore = fake.eventsNamed(QStringLiteral("hide_self")).size();
        core->sliceById(0)->setDspMode(DSPMode::RADE_U);
        QTRY_VERIFY(!fake.eventsNamed(QStringLiteral("show_self")).isEmpty());
        windowHost->hideFreedvStation(true);
        QTRY_VERIFY(core->spotSourceHost()->freedvReporterHidden());
        QTRY_VERIFY(windowHost->freedvReporterHidden());
        QTRY_COMPARE(fake.eventsNamed(QStringLiteral("hide_self")).size(), hidesBefore + 1);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("FreeDvReporter/Hidden")).toString(),
                 QStringLiteral("True"));
        QVERIFY(core->freeDvReporter()->isHiddenFromView());

        // Start and Stop from the window.
        windowHost->stopFreedvReporter();
        QTRY_COMPARE(core->spotSourceHost()->freedvReporterState(), SpotSourceHost::kOff);
        QTRY_COMPARE(windowHost->freedvReporterState(), SpotSourceHost::kOff);
        QVERIFY(!core->freeDvReporter()->isConnected());
        windowHost->startFreedvReporter();
        QTRY_VERIFY_WITH_TIMEOUT(core->freeDvReporter()->isConnected(), 5000);
        QTRY_COMPARE(windowHost->freedvReporterState(), SpotSourceHost::kConnected);
    }

    // A window's requests to a Core that does not run FreeDV Reporter are
    // refused before they are sent.
    void anOlderCoreIsNotAsked()
    {
        RadioModel window{RadioModel::Role::Remote};
        SpotSourceHost* host = window.spotSourceHost();
        QSignalSpy refused(host, &SpotSourceHost::sourceRefused);
        host->sendFreedvMessage(QStringLiteral("hello"));
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.constFirst().at(1).toString(),
                 QStringLiteral("Not connected to the Core, so the FreeDV Reporter request was "
                                "not sent."));
        QVERIFY(!window.freeDvReporter()->isConnected());
        QCOMPARE(IStationLink::stationFreedvUnavailableReason(),
                 QStringLiteral("This Core does not run FreeDV Reporter for this app. Updating "
                                "the Core may help."));
    }

    // B7.4: the listed frequency follows the Core's RADE slice, a switch into
    // RADE shows the station, and "Hide my station" keeps it hidden.
    void theStationShowsWhenItsListedSliceIsInRade()
    {
        FakeReporterServer fake;
        QVERIFY(fake.listen());
        setCoreIdentity(fake.url());
        std::unique_ptr<RadioModel> core = makeCore();
        core->sliceById(0)->setFrequency(14074000.0);
        core->addSlice(QStringLiteral("pan-0"));
        SliceModel* rade = core->sliceById(1);
        QVERIFY(rade);
        rade->setFrequency(7177000.0);
        core->restoreStationSpotSources();
        QTRY_VERIFY_WITH_TIMEOUT(core->freeDvReporter()->isConnected(), 5000);
        // Neither slice in RADE: listed on the station's slice, hidden.
        QTRY_VERIFY(core->freeDvReporter()->isHiddenFromView());
        QTRY_VERIFY(!fake.eventsNamed(QStringLiteral("hide_self")).isEmpty());

        // Slice B into RADE: its frequency is listed and the station shows.
        rade->setDspMode(DSPMode::RADE_U);
        QVERIFY(!core->freeDvReporter()->isHiddenFromView());
        QCOMPARE(core->freedvWantedFrequencyHzForTest(), quint64(7177000));
        QTRY_VERIFY(!fake.eventsNamed(QStringLiteral("show_self")).isEmpty());
        QTRY_VERIFY(([&] {
            const auto events = fake.eventsNamed(QStringLiteral("freq_change"));
            return std::any_of(events.cbegin(), events.cend(),
                                [](const QJsonArray& e) {
            return e.at(1).toObject().value(QStringLiteral("freq")).toInteger() == 7177000;
            });
        }()));

        // "Hide my station" wins over RADE, and lets go again.
        QString reason;
        QVERIFY(core->spotSourceHost()->setFreedvHidden(true, &reason));
        QVERIFY(core->freeDvReporter()->isHiddenFromView());
        rade->setFrequency(7178000.0);
        QVERIFY(core->freeDvReporter()->isHiddenFromView());
        QVERIFY(core->spotSourceHost()->setFreedvHidden(false, &reason));
        QVERIFY(!core->freeDvReporter()->isHiddenFromView());

        // Out of RADE: hidden again.
        rade->setDspMode(DSPMode::USB);
        QVERIFY(core->freeDvReporter()->isHiddenFromView());
        QCOMPARE(core->freedvWantedFrequencyHzForTest(), quint64(14074000));
    }

    // B7.3 in a real remote window: the FreeDV tab and the dialog wait for
    // the Core, then list its stations with distance and heading at once.
    void aRemoteWindowListsTheCoresStationsAtOnce()
    {
        RemoteWindowHarness h;
        QVERIFY(h.start());
        // The Core's own grid (read from its settings at its start).
        h.station().freeDvStationModel()->setOurGridSquare(QStringLiteral("EM73"));
        FreeDVStation w1aw;
        w1aw.sid = QStringLiteral("sid-w1aw");
        w1aw.callsign = QStringLiteral("W1AW");
        w1aw.gridSquare = QStringLiteral("FN31");
        w1aw.frequencyHz = 14236000;
        w1aw.status = QStringLiteral("Active");
        h.station().freeDvStationModel()->onStationAdded(w1aw.sid, w1aw);

        QAction* spotHub = h.menuAction(QStringLiteral("&Tools"), QStringLiteral("Spot &Hub..."));
        QAction* reporter =
            h.menuAction(QStringLiteral("&Tools"), QStringLiteral("&FreeDV Reporter..."));
        QVERIFY(spotHub && reporter);
        spotHub->trigger();
        reporter->trigger();
        auto* hub = h.window()->findChild<SpotHubDialog*>();
        auto* dialog = h.window()->findChild<FreeDVReporterDialog*>();
        QVERIFY(hub && dialog);
        auto* start = hub->findChild<QPushButton*>(QStringLiteral("freedvStartBtn"));
        auto* hide = hub->findChild<QCheckBox*>(QStringLiteral("freedvHideFromViewChk"));
        auto* send = dialog->findChild<QPushButton*>(QStringLiteral("msgSendButton"));
        QVERIFY(start && hide && send);
        const QString why = QStringLiteral("Connect to the Core to change these.");
        QVERIFY(!start->isEnabled());
        QCOMPARE(start->toolTip(), why);
        QVERIFY(!send->isEnabled());
        QCOMPARE(send->toolTip(), why);

        QAction* connect = h.menuAction(QStringLiteral("&Radio"), QStringLiteral("&Connect"));
        QVERIFY(connect && connect->isEnabled());
        connect->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(h.client()->isHandshakeComplete(), 10000);
        QTRY_VERIFY(start->isEnabled());
        QTRY_VERIFY(send->isEnabled());
        QVERIFY(hide->isEnabled());

        // Listed at once, with the Core's distance and heading.
        FreeDVStationModel* windowList = h.remoteModel()->freeDvStationModel();
        QTRY_COMPARE(windowList->stationCount(), 1);
        QVERIFY(windowList->stationBySid(QStringLiteral("sid-w1aw")).distanceKm > 1000.0);
        auto* table = dialog->findChild<QTableView*>();
        QVERIFY(table && table->model());
        QTRY_COMPARE(table->model()->rowCount(), 1);
        const QString distance =
            table->model()->index(0, kDistanceCol).data(Qt::DisplayRole).toString();
        QVERIFY2(distance.trimmed() != QStringLiteral("-") && !distance.isEmpty(),
                 qPrintable(distance));

        // "Hide my station" from the window is the Core's.
        hide->setChecked(true);
        QTRY_VERIFY(h.station().spotSourceHost()->freedvReporterHidden());
    }

    // A remote window's VFO flag shows the Core's RADE status as a local
    // window's does: the lock and SNR, the offset after the SNR (kept when
    // only the SNR changes, since the Core sends a value only when it
    // moves), and the hollow circle when the decoder loses sync. The
    // window never reports to FreeDV Reporter itself; the Core does.
    void aRemoteFlagFollowsTheCoresRadeStatus()
    {
        RemoteWindowHarness h;
        QVERIFY(h.start());
        SliceModel* coreSlice = h.station().sliceById(0);
        QVERIFY(coreSlice != nullptr);
        coreSlice->setDspMode(DSPMode::RADE_U);

        QAction* connect = h.menuAction(QStringLiteral("&Radio"), QStringLiteral("&Connect"));
        QVERIFY(connect && connect->isEnabled());
        connect->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(h.client()->isHandshakeComplete(), 10000);
        QTRY_VERIFY(h.remoteModel()->sliceById(0) != nullptr);
        QTRY_COMPARE(h.remoteModel()->sliceById(0)->dspMode(), DSPMode::RADE_U);
        VfoWidget* flag = nullptr;
        QTRY_VERIFY((flag = remoteFlag(*h.window(), 0)) != nullptr);
        QLabel* label = flag->snrLabelForTest();
        QVERIFY(label != nullptr);

        coreSlice->setRadeSynced(true);
        coreSlice->setSnrDb(12.0);
        coreSlice->setRadeFreqOffsetHz(25.0);
        QTRY_VERIFY2(label->text().contains(QStringLiteral("12dB +25Hz")),
                     qPrintable(label->text()));
        QVERIFY(label->text().contains(QStringLiteral("●")));

        // Only the SNR moves: the offset stays on the flag.
        coreSlice->setSnrDb(9.0);
        QTRY_VERIFY2(label->text().contains(QStringLiteral("9dB +25Hz")),
                     qPrintable(label->text()));

        // A new offset alone replaces the old one.
        coreSlice->setRadeFreqOffsetHz(-40.0);
        QTRY_VERIFY2(label->text().contains(QStringLiteral("9dB -40Hz")),
                     qPrintable(label->text()));

        // Loss of sync.
        coreSlice->setRadeSynced(false);
        QTRY_VERIFY2(label->text().contains(QStringLiteral("○"))
                         && label->text().endsWith(QStringLiteral(" ---")),
                     qPrintable(label->text()));

        // The lock comes back at the same SNR and offset: the Core sends
        // only the sync, and the flag shows the lock with both again.
        coreSlice->setRadeSynced(true);
        QTRY_VERIFY2(label->text().contains(QStringLiteral("9dB -40Hz")),
                     qPrintable(label->text()));
        QVERIFY(label->text().contains(QStringLiteral("●")));

        FreeDVRadeReporterBridge* windowReporter = h.remoteModel()->radeReporterBridgeForTest();
        QVERIFY(windowReporter != nullptr);
        QVERIFY(!windowReporter->syncedForTest());
    }

private:
    QTemporaryDir m_securityDir;
};

QTEST_MAIN(TstFreedvReporterStation)
#include "tst_freedv_reporter_station.moc"
