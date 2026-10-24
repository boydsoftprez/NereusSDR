// no-port-check: NereusSDR-original. No upstream logic is ported here.
// =================================================================
// tests/tst_core_parity.cpp  (NereusSDR)
// =================================================================
//
// The guard for the two ways NereusSDR assembles a Core. The headless Core
// (nereusd, DaemonApp::start) and the desktop Core (a desktop window with
// DesktopCore/Run on: MainWindow, GuiSessionCoordinator and
// DesktopStationController) each set up the radio model's optional helpers
// by hand, so they drift apart. This test stands up one, then the other, in
// one process, signs the same desktop client in to each over the in-process
// loopback, and compares what each one serves:
//
//   * every entry of the wire "capabilities" message the client receives
//     (keys "cap.<name>"), and
//   * a list of facts read from the Core's own objects (which helpers the
//     radio model was given, which record streams the server holds).
//
// A difference fails the test unless it is listed in kExpectedDifferences
// below, either as on purpose (the two listed values are then asserted
// exactly) or as a known gap (the row reports XFAIL today and fails once the
// gap is closed: the fix deletes its rows).
//
// What this test does not cover:
//   * Values that only arrive once a radio streams samples (the auto AGC
//     noise floor was a gap of that kind). Both Cores here have a primed
//     board and no radio.
//   * Which thread does the work on each Core.
//   * The property lists of the mirrored objects each Core sends.
//
// Loopback only: no radio, no rendezvous or relay server, no audio device
// beyond what each Core opens for itself in the test sandbox.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-11 - Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-10-22, J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code:
//     test ports come from unusedLoopbackPort() (UnusedLoopbackPort.h), below
//     the kernel's ephemeral range, so a parallel test cannot take one between
//     the pick and the listen.
// =================================================================

#include "UnusedLoopbackPort.h"
#include <QtTest/QtTest>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QSet>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QVariant>

#include <chrono>
#include <cstring>

#include "core/AppSettings.h"
#include "core/CoreInit.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"
#include "core/StepAttenuatorController.h"
#include "core/daemon/DaemonApp.h"
#include "core/daemon/DaemonConfig.h"
#include "core/security/CertificateStore.h"
#include "core/session/ModMonitorRecord.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/TransmitStateFacade.h"
#include "core/settings/SettingsProxy.h"
#include "core/station/StationHandover.h"
#include "gui/DesktopStationController.h"
#include "gui/GuiDesktopStationRuntime.h"
#include "gui/GuiSessionCoordinator.h"
#include "gui/MainWindow.h"
#include "gui/StationServiceManager.h"
#include "models/RadioModel.h"

#include "fakes/LoopbackTransport.h"
#include "fakes/MainWindowTestSettings.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

// The same radio on both Cores: two ADCs, PureSignal and Diversity offered.
constexpr HPSDRHW kBoard = HPSDRHW::Angelia;
const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:42");
const QString kRadioName = QStringLiteral("Parity bench radio");
constexpr int kHandshakeTimeoutMs = 10000;

// What one Core serves, read the same way from both.
struct CoreFacts {
    QJsonObject capabilities;         // the wire "capabilities" message a signed-in desktop client receives
    QMap<QString, QVariant> assembly; // facts read from the Core's own objects
};

enum class Kind { OnPurpose, KnownGap };

struct Expected {
    const char* key;      // "cap.vaxVersion", "stepAtt.debouncedSave", ...
    Kind kind;
    const char* headless; // the value as text, "" when the key is absent
    const char* desktop;
    const char* why;      // plain words
    const char* where;    // OnPurpose: where it is written. KnownGap: who is fixing it.
};

// Every difference between the two Cores that is allowed today. A fix for a
// known gap deletes its rows; a new difference made on purpose adds a row
// that says where the decision is written.
const char* const kTciWhy =
    "Signed-in clients of a desktop Core get no view or control of the Core's TCI server.";
const char* const kTciWho = "Design question with the maintainer; owner: core parity audit session.";

const Expected kExpectedDifferences[] = {
    {"cap.coreSpeakerVersion", Kind::OnPurpose, "1", "",
     "A desktop that hosts a Core plays through its own window's speakers. The Core speaker is "
     "for a Core with no window.",
     "src/core/session/StationServer.cpp:9814 (peerGetsCoreSpeaker)"},
    {"coreSpeaker.host", Kind::OnPurpose, "true", "false",
     "A desktop that hosts a Core plays through its own window's speakers. The Core speaker is "
     "for a Core with no window.",
     "src/core/daemon/DaemonApp.cpp:340 (setCoreSpeakerHost) and "
     "src/core/session/StationServer.cpp:9814 (peerGetsCoreSpeaker)"},
    {"cap.vaxVersion", Kind::OnPurpose, "0", "1",
     "VAX devices belong to a desktop. The headless Core publishes none: outputs there would be "
     "devices nothing on the Core's computer feeds.",
     "src/core/daemon/DaemonApp.cpp:315 (setVaxOutputsAllowed) and "
     "src/core/session/StationServer.cpp:13996 (vaxVersion)"},

    {"cap.stationTciVersion", Kind::KnownGap, "2", "0", kTciWhy, kTciWho},
    {"cap.stationTciSettingsVersion", Kind::KnownGap, "1", "", kTciWhy, kTciWho},
    {"stationTci.controller", Kind::KnownGap, "true", "false", kTciWhy, kTciWho},

    {"pcCapture.allowed", Kind::OnPurpose, "false", "true",
     "The headless Core has nobody at its computer to speak, so it never opens a microphone. A "
     "desktop Core's own window may use its computer's microphone.",
     "src/core/daemon/DaemonApp.cpp:307 (setPcCaptureAllowed) and "
     "src/models/RadioModel.h:1106 (setPcCaptureAllowed)"},
};

const Expected* expectedFor(const QString& key)
{
    for (const Expected& row : kExpectedDifferences) {
        if (key == QLatin1String(row.key)) {
            return &row;
        }
    }
    return nullptr;
}

// One value as the text the table holds. A missing key is "".
QString textOf(const QJsonValue& value)
{
    switch (value.type()) {
    case QJsonValue::Undefined:
        return QString();
    case QJsonValue::Null:
        return QStringLiteral("null");
    case QJsonValue::Bool:
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    case QJsonValue::Double:
        return QString::number(value.toDouble(), 'g', 15);
    case QJsonValue::String:
        return value.toString().isEmpty() ? QStringLiteral("(empty)") : value.toString();
    case QJsonValue::Array:
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    case QJsonValue::Object:
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    return QString();
}

// The first wire message of `type` this end received, or an empty object.
QJsonObject firstOfType(const QList<QByteArray>& received, const QString& type)
{
    for (const QByteArray& wire : received) {
        const QJsonObject message = QJsonDocument::fromJson(wire).object();
        if (message.value(QStringLiteral("type")).toString() == type) {
            return message;
        }
    }
    return QJsonObject{};
}

// The capabilities message as "cap.<name>" -> text: one row per entry of
// its `properties` list, and one per other top-level field.
QMap<QString, QString> flattenCapabilities(const QJsonObject& capabilities)
{
    QMap<QString, QString> out;
    for (auto it = capabilities.constBegin(); it != capabilities.constEnd(); ++it) {
        if (it.key() == QStringLiteral("type") || it.key() == QStringLiteral("properties")) {
            continue;
        }
        out.insert(QStringLiteral("cap.") + it.key(), textOf(it.value()));
    }
    for (const QJsonValue& entry : capabilities.value(QStringLiteral("properties")).toArray()) {
        QJsonObject property = entry.toObject();
        QString key = QStringLiteral("cap.") + property.take(QStringLiteral("name")).toString();
        QString text = textOf(property.take(QStringLiteral("value")));
        // Nearly every entry is one whole number at ordinal 0. Anything
        // else shows in the key or the text, so it is compared too.
        const int ordinal = property.take(QStringLiteral("ordinal")).toInt();
        if (ordinal != 0) {
            key += QStringLiteral("[%1]").arg(ordinal);
        }
        if (property.value(QStringLiteral("kind")).toString() == QStringLiteral("i64")) {
            property.remove(QStringLiteral("kind"));
        }
        if (!property.isEmpty()) {
            text += QLatin1Char(' ')
                    + QString::fromUtf8(QJsonDocument(property).toJson(QJsonDocument::Compact));
        }
        out.insert(key, text);
    }
    return out;
}

QMap<QString, QString> flatten(const CoreFacts& facts)
{
    QMap<QString, QString> out = flattenCapabilities(facts.capabilities);
    for (auto it = facts.assembly.constBegin(); it != facts.assembly.constEnd(); ++it) {
        out.insert(it.key(), textOf(QJsonValue::fromVariant(it.value())));
    }
    return out;
}

quint16 freeLoopbackPort()
{
    return unusedLoopbackPort();
}

// The radio both Cores describe, beyond the board each was primed with.
void primeRadio(RadioModel* model)
{
    RadioInfo info;
    info.macAddress = kMac;
    info.name = kRadioName;
    info.boardType = kBoard;
    model->setLastRadioInfoForTest(info);
}

// The facts read from a Core's own objects. Public getters only, the same
// on both sides.
QMap<QString, QVariant> assemblyOf(RadioModel* model, StationServer* server)
{
    QMap<QString, QVariant> facts;
    const StepAttenuatorController* stepAtt = model->stepAttController();
    facts.insert(QStringLiteral("stepAtt.exists"), stepAtt != nullptr);
    if (stepAtt != nullptr) {
        facts.insert(QStringLiteral("stepAtt.debouncedSave"), stepAtt->debouncedSaveEnabled());
        facts.insert(QStringLiteral("stepAtt.bandRestoreToRadio"), stepAtt->bandRestoreToRadio());
    }
    facts.insert(QStringLiteral("stationTci.controller"), model->stationTciController() != nullptr);
    facts.insert(QStringLiteral("stationCat.controller"), model->stationCatController() != nullptr);
    facts.insert(QStringLiteral("rotor.controller"), model->stationRotorController() != nullptr);
    facts.insert(QStringLiteral("coreSpeaker.host"), model->coreSpeakerHost());
    facts.insert(QStringLiteral("accessoryIdentity"), model->stationAccessoryIdentityEnabled());
    facts.insert(QStringLiteral("meters.pump"), model->sliceMeterPump() != nullptr);
    facts.insert(QStringLiteral("txDisplay.feed"), model->txDisplayFeed() != nullptr);
    facts.insert(QStringLiteral("txAnalyzer"), model->txAnalyzer() != nullptr);
    facts.insert(QStringLiteral("receiveOnlyPolicy"), model->receiveOnlyStationPolicy());
    facts.insert(QStringLiteral("pcCapture.allowed"), model->pcCaptureAllowed());
    const QString streams[] = {
        QStringLiteral("coreLog"),
        QStringLiteral("catLog"),
        QStringLiteral("spots"),
        QStringLiteral("freedvStations"),
        QString::fromLatin1(ModMonitorRecord::kTxStream),
        QString::fromLatin1(ModMonitorRecord::kFeedbackStream),
        QString::fromLatin1(TransmitState::kCfcStream),
    };
    for (const QString& stream : streams) {
        facts.insert(QStringLiteral("recordStream.") + stream,
                     server->recordStreamForTest(stream) != nullptr);
    }
    return facts;
}

// Signs one real desktop client in to `server` over a loopback pair and
// reads what the Core serves. False, with `error` set, when the client
// never finished signing in.
bool snapshot(RadioModel* model, StationServer* server, CoreFacts* out, QString* error)
{
    QObject owner;
    RadioModel window{RadioModel::Role::Remote};
    SettingsProxy proxy;
    StationClient client{&window, &proxy};
    auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), &owner);
    auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), &owner);
    stationEnd->linkTo(clientEnd);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    client.startSession(clientEnd, server->token());
    server->acceptTransport(stationEnd);
    if (!QTest::qWaitFor([&completed]() { return completed.count() >= 1; }, kHandshakeTimeoutMs)) {
        *error = QStringLiteral("the client never finished signing in (messages received: %1)")
                     .arg(QString::fromUtf8(clientEnd->receivedKinds().join(", ")));
        return false;
    }
    out->capabilities = firstOfType(clientEnd->received(), QStringLiteral("capabilities"));
    if (out->capabilities.isEmpty()) {
        *error = QStringLiteral("the client signed in but was sent no capabilities message");
        return false;
    }
    out->assembly = assemblyOf(model, server);
    return true;
}

} // namespace

class TstCoreParity : public QObject {
    Q_OBJECT

private:
    CoreFacts m_headless;
    CoreFacts m_desktop;
    QMap<QString, QString> m_headlessFlat;
    QMap<QString, QString> m_desktopFlat;

    QString profileDirectory() const
    {
        return QFileInfo(AppSettings::instance().filePath()).absolutePath();
    }

    // Each Core starts from the same empty settings, with an upgraded
    // Core's pairing token in the profile so the client can sign in.
    void startFromNothing()
    {
        QVERIFY(!AppSettings::instance().remoteBackend());
        // Both programs migrate the settings before they build a Core
        // (CoreInit::initialize, once per process). Emptying the settings
        // must not undo that for the second Core.
        const QString schemaKey = QStringLiteral("SettingsSchemaVersion");
        const QVariant schema = AppSettings::instance().value(schemaKey);
        AppSettings::instance().clear();
        if (schema.isValid()) {
            AppSettings::instance().setValue(schemaKey, schema);
        }
        Test::markAudioFirstRunDone();
        QVERIFY(AppSettings::instance().save());
        Test::seedUpgradedCoreToken(CertificateStore::defaultDirectory());
        RadioDiscovery::clearHoldOffForTest();
        RadioDiscovery discovery;
        discovery.holdOffScans(std::chrono::minutes{5});
    }

    void snapshotHeadlessCore()
    {
        startFromNothing();
        if (QTest::currentTestFailed()) {
            return;
        }
        DaemonConfig cfg = DaemonConfig::defaults();
        cfg.sliceCount = 1;
        cfg.statusPage = false;
        cfg.rendezvousServers = {};
        cfg.remoteBind = QStringLiteral("127.0.0.1");
        cfg.remotePort = static_cast<int>(freeLoopbackPort());
        QVERIFY2(cfg.remotePort != 0, "no free loopback port for the headless Core");

        DaemonApp app;
        app.primeBoardForTest(kBoard, kMac);
        QVERIFY2(app.start(cfg), "the headless Core did not start");
        StationServer* server = app.stationServer();
        RadioModel* model = app.radioModelForTest();
        QString error;
        bool ok = server != nullptr && model != nullptr;
        if (!ok) {
            error = QStringLiteral("the headless Core has no station server of its own");
        } else {
            primeRadio(model);
            ok = snapshot(model, server, &m_headless, &error);
        }
        app.stop();
        QVERIFY2(ok, qPrintable(QStringLiteral("Headless Core: ") + error));
    }

    void snapshotDesktopCore()
    {
        startFromNothing();
        if (QTest::currentTestFailed()) {
            return;
        }
        AppSettings& settings = AppSettings::instance();
        StationHandover ownership(AppSettings::profileOverride());
        QString error;
        QVERIFY2(ownership.acquire(0, &error), qPrintable(error));
        const quint16 port = freeLoopbackPort();
        QVERIFY2(port != 0, "no free loopback port for the desktop Core");
        QFile config(profileDirectory() + QStringLiteral("/station.conf"));
        QVERIFY(config.open(QIODevice::WriteOnly));
        const QByteArray configBytes = QStringLiteral("remote_bind = 127.0.0.1\nremote_port = %1\n"
            "status_page = off\nrendezvous_servers =\n").arg(port).toUtf8();
        QCOMPARE(config.write(configBytes), configBytes.size());
        config.close();
        settings.setValue(QStringLiteral("DesktopCore/Run"), true);
        QTemporaryDir serviceHome;
        QVERIFY(serviceHome.isValid());
        StationServiceOptions options;
        options.homeDirectory = serviceHome.path();
        options.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };

        GuiSessionCoordinator sessions;
        QVERIFY(sessions.configureDesktopStation(AppSettings::profileOverride(),
                                                 ownership.ownsProfile(), options));
        QVERIFY2(sessions.replace({}, false, &error), qPrintable(error));
        GuiDesktopStationRuntime* runtime = sessions.desktopRuntime();
        bool ok = runtime != nullptr && runtime->controller()->enabled()
                  && runtime->controller()->server() != nullptr && sessions.window() != nullptr;
        if (!ok) {
            error = QStringLiteral("the desktop window is not hosting a Core with a station server");
        } else {
            RadioModel* model = sessions.window()->radioModel();
            model->setBoardForTest(kBoard);
            primeRadio(model);
            ok = snapshot(model, runtime->controller()->server(), &m_desktop, &error);
        }
        sessions.shutdown();
        QVERIFY2(ok, qPrintable(QStringLiteral("Desktop Core: ") + error));
    }

private slots:
    // The two Cores, one after the other, never both alive at once.
    void initTestCase()
    {
        AppSettings::setProfileOverride(
            QStringLiteral("core-parity-%1").arg(QCoreApplication::applicationPid()));
        QVERIFY(CoreInit::initialize());
        snapshotHeadlessCore();
        if (QTest::currentTestFailed()) {
            return;
        }
        snapshotDesktopCore();
        if (QTest::currentTestFailed()) {
            return;
        }
        m_headlessFlat = flatten(m_headless);
        m_desktopFlat = flatten(m_desktop);
    }

    void cleanupTestCase()
    {
        RadioDiscovery::clearHoldOffForTest();
        const QString path = AppSettings::instance().filePath();
        QFile::remove(profileDirectory() + QStringLiteral("/station.conf"));
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // One row per key either Core serves.
    void bothCoresServeTheSame_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QString>("headless");
        QTest::addColumn<QString>("desktop");
        QSet<QString> seen;
        for (const QMap<QString, QString>* side : {&m_headlessFlat, &m_desktopFlat}) {
            for (auto it = side->constBegin(); it != side->constEnd(); ++it) {
                if (seen.contains(it.key())) {
                    continue;
                }
                seen.insert(it.key());
                QTest::newRow(qPrintable(it.key()))
                    << it.key() << m_headlessFlat.value(it.key()) << m_desktopFlat.value(it.key());
            }
        }
    }

    void bothCoresServeTheSame()
    {
        QFETCH(QString, key);
        QFETCH(QString, headless);
        QFETCH(QString, desktop);
        const Expected* expected = expectedFor(key);
        if (expected == nullptr) {
            QVERIFY2(headless == desktop,
                     qPrintable(QStringLiteral(
                         "The two Cores differ on '%1': the headless Core has '%2', the desktop "
                         "Core has '%3' (empty means the key is absent). Add the helper to the "
                         "other Core, or list the difference in kExpectedDifferences with the "
                         "reason.").arg(key, headless, desktop)));
            return;
        }
        if (expected->kind == Kind::KnownGap) {
            // XFAIL while the gap is open. Once it is closed the row fails
            // until the fix deletes it from the table.
            QVERIFY2(headless != desktop,
                     qPrintable(QStringLiteral(
                         "The two Cores now agree on '%1' (both have '%2'), but "
                         "kExpectedDifferences still lists it as a known gap. Delete the row.")
                         .arg(key, headless)));
            QEXPECT_FAIL("", expected->why, Continue);
            QCOMPARE(headless, desktop);
            return;
        }
        QVERIFY2(headless == QLatin1String(expected->headless)
                     && desktop == QLatin1String(expected->desktop),
                 qPrintable(QStringLiteral(
                     "'%1' is listed in kExpectedDifferences as different on purpose (headless "
                     "'%2', desktop '%3'), but the headless Core has '%4' and the desktop Core has "
                     "'%5'. Correct the row, or delete it if the two Cores now agree.")
                     .arg(key, QLatin1String(expected->headless), QLatin1String(expected->desktop),
                          headless, desktop)));
    }

    // A row of the table that no longer describes the two Cores cannot hide.
    void everyListedDifferenceIsReal_data()
    {
        QTest::addColumn<int>("index");
        int index = 0;
        for (const Expected& row : kExpectedDifferences) {
            QTest::newRow(row.key) << index;
            ++index;
        }
    }

    void everyListedDifferenceIsReal()
    {
        QFETCH(int, index);
        const Expected& row = kExpectedDifferences[index];
        const QString key = QLatin1String(row.key);
        QVERIFY2(std::strlen(row.why) > 0 && std::strlen(row.where) > 0,
                 qPrintable(QStringLiteral("The kExpectedDifferences row for '%1' must say why "
                                           "the two Cores differ and where that is written or "
                                           "who is fixing it.").arg(key)));
        QVERIFY2(m_headlessFlat.contains(key) || m_desktopFlat.contains(key),
                 qPrintable(QStringLiteral("Stale row: kExpectedDifferences lists '%1', which "
                                           "neither Core serves. Delete the row.").arg(key)));
        QVERIFY2(std::strcmp(row.headless, row.desktop) != 0,
                 qPrintable(QStringLiteral("The kExpectedDifferences row for '%1' lists the same "
                                           "value for both Cores, so it is not a difference. "
                                           "Delete the row.").arg(key)));
        if (row.kind == Kind::OnPurpose) {
            return; // bothCoresServeTheSame asserts the two listed values.
        }
        const QString headless = m_headlessFlat.value(key);
        const QString desktop = m_desktopFlat.value(key);
        if (headless == desktop) {
            return; // The gap is closed: bothCoresServeTheSame reports it.
        }
        QVERIFY2(headless == QLatin1String(row.headless) && desktop == QLatin1String(row.desktop),
                 qPrintable(QStringLiteral(
                     "The known gap '%1' is listed as headless '%2', desktop '%3', but the "
                     "headless Core has '%4' and the desktop Core has '%5'. Correct the row.")
                     .arg(key, QLatin1String(row.headless), QLatin1String(row.desktop), headless,
                          desktop)));
    }
};

QTEST_MAIN(TstCoreParity)
#include "tst_core_parity.moc"
