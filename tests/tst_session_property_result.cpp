// no-port-check: NereusSDR-original accepted-value and ordering integration tests.
#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QFile>
#include "core/AppSettings.h"
#include "core/CfcProfile.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "core/settings/SettingsScope.h"
#include "core/settings/SettingsProxyServer.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/PureSignalSettings.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using Test::LoopbackTransport;

namespace {
class HoldingTransport : public LoopbackTransport {
public:
    explicit HoldingTransport() : LoopbackTransport(QStringLiteral("station")) {}
    bool hold = false;
    bool advertisePropertyResults = true;
    QList<QByteArray> held;
    void sendText(const QByteArray& wire) override
    {
        SessionMessage message;
        if (!advertisePropertyResults && SessionMessages::decode(wire, &message)
            && message.kind == SessionMessageKind::Capabilities) {
            for (auto& update : message.updates) {
                if (update.name == "propertyResultVersion") {
                    update.value = 0;
                }
            }
            LoopbackTransport::sendText(SessionMessages::encode(message));
            return;
        }
        if (hold && SessionMessages::decode(wire, &message)
            && message.kind == SessionMessageKind::PropertyResult) {
            held.append(wire);
            return;
        }
        LoopbackTransport::sendText(wire);
    }
    void releaseFirst() { LoopbackTransport::sendText(held.takeFirst()); }
};
// PropertyWrite messages among what a transport's outboundText spy saw.
int propertyWrites(const QSignalSpy& sent)
{
    int count = 0;
    for (const auto& args : sent) {
        SessionMessage message;
        if (SessionMessages::decode(args.at(0).toByteArray(), &message)
            && message.kind == SessionMessageKind::PropertyWrite) {
            ++count;
        }
    }
    return count;
}
// Whether a Delta among what a transport's textReceived spy saw carries
// `name`.
bool deltaCarries(const QSignalSpy& received, const QByteArray& name)
{
    for (const auto& args : received) {
        SessionMessage message;
        if (!SessionMessages::decode(args.at(0).toByteArray(), &message)
            || message.kind != SessionMessageKind::Delta) {
            continue;
        }
        for (const auto& update : message.updates) {
            if (update.name == name) {
                return true;
            }
        }
    }
    return false;
}
// A Core with one USB slice and a window on it, write flush paused.
struct SliceSession {
    QTemporaryDir security;
    RadioModel station;
    SliceModel* coreSlice = nullptr;
    std::unique_ptr<StationServer> server;
    RadioModel gui{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    LoopbackTransport* guiEnd = nullptr;
    SliceModel* slice = nullptr;
};
// A Core and a window on its TransmitModel, write flush paused.
struct TxSession {
    QTemporaryDir security;
    RadioModel station;
    std::unique_ptr<StationServer> server;
    RadioModel gui{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    LoopbackTransport* guiEnd = nullptr;
    TransmitModel* coreTx = nullptr;
    TransmitModel* windowTx = nullptr;
};
// The CFC curve for the Core's ten-band values, with the given
// pre-compression: a curve that decodes.
QString cfcCurve(const TransmitModel& tx, double precompDb)
{
    std::array<int, 10> frequencyHz{};
    std::array<int, 10> compressionDb{};
    std::array<int, 10> postEqGainDb{};
    for (int i = 0; i < 10; ++i) {
        const auto k = static_cast<std::size_t>(i);
        frequencyHz[k] = tx.cfcEqFreq(i);
        compressionDb[k] = tx.cfcCompression(i);
        postEqGainDb[k] = tx.cfcPostEqBandGain(i);
    }
    CfcProfile::Profile profile = CfcProfile::legacyProfile(
        frequencyHz, compressionDb, postEqGainDb, tx.cfcPrecompDb(), tx.cfcPostEqGainDb());
    profile.precompDb = precompDb;
    return CfcProfile::encode(profile);
}
bool cfcCurveDecodes(const QString& blob)
{
    CfcProfile::Profile profile;
    return CfcProfile::decode(blob, profile);
}
// Filter-edge settings writes (a "...FilterLow"/"...FilterHigh" key)
// among what a transport's outboundText spy saw, as key -> value.
QList<QPair<QString, QString>> filterSettingsWrites(const QSignalSpy& sent)
{
    QList<QPair<QString, QString>> writes;
    for (const auto& args : sent) {
        SessionMessage message;
        if (SessionMessages::decode(args.at(0).toByteArray(), &message)
            && message.kind == SessionMessageKind::SettingsWrite
            && message.objectKey.contains("/Filter") && !message.updates.isEmpty()) {
            writes.append({QString::fromUtf8(message.objectKey),
                           message.updates.first().value.toString()});
        }
    }
    return writes;
}
// Installs a SettingsProxy as AppSettings' remote backend for one scope,
// the way a remote window runs; always removed again.
class RemoteBackendScope {
public:
    explicit RemoteBackendScope(SettingsProxy* proxy) { AppSettings::instance().setRemoteBackend(proxy); }
    ~RemoteBackendScope() { AppSettings::instance().setRemoteBackend(nullptr); }
    RemoteBackendScope(const RemoteBackendScope&) = delete;
    RemoteBackendScope& operator=(const RemoteBackendScope&) = delete;
};
// The per-(band, mode) LastFilter key prefix SliceModel::setDspMode uses.
QString lastFilterPrefix(const SliceModel* slice, DSPMode mode)
{
    return QStringLiteral("Slice%1/Band%2/Mode%3/")
        .arg(slice->sliceIndex())
        .arg(bandKeyName(bandFromFrequency(slice->frequency())))
        .arg(SliceModel::modeName(mode));
}
MirrorUpdate real(const char* name, double value)
{
    return {0, name, MirrorWireKind::Float64, value};
}
}

class TestSessionPropertyResult : public QObject {
    Q_OBJECT
private:
    void startSliceSession(SliceSession& s, DSPMode startMode = DSPMode::USB)
    {
        QVERIFY(s.security.isValid());
        s.station.setBoardForTest(HPSDRHW::HermesLite);
        RadioInfo info;
        info.macAddress = "AA:BB:CC:DD:EE:01";
        info.boardType = HPSDRHW::HermesLite;
        s.station.setLastRadioInfoForTest(info);
        const int id = s.station.addSlice();
        s.coreSlice = s.station.sliceById(id);
        s.coreSlice->setDspMode(startMode);
        s.server = std::make_unique<StationServer>(
            &s.station, AppSettings::instance(),
            NereusSDR::Test::seedUpgradedCoreToken(s.security.path()));
        s.client = std::make_unique<StationClient>(&s.gui, &s.proxy);
        auto* coreEnd = new HoldingTransport;
        s.guiEnd = new LoopbackTransport("gui");
        coreEnd->linkTo(s.guiEnd);
        s.client->startSession(s.guiEnd, s.server->token());
        s.server->acceptTransport(coreEnd);
        QTRY_VERIFY(s.client->isHandshakeComplete());
        s.slice = s.gui.sliceById(id);
        QVERIFY(s.slice);
        QCOMPARE(s.slice->dspMode(), startMode);
        s.client->pauseWriteFlushForTest();
    }

    void startTxSession(TxSession& s)
    {
        QVERIFY(s.security.isValid());
        s.station.setBoardForTest(HPSDRHW::HermesLite);
        RadioInfo info;
        info.macAddress = "AA:BB:CC:DD:EE:01";
        info.boardType = HPSDRHW::HermesLite;
        s.station.setLastRadioInfoForTest(info);
        s.station.addSlice();
        s.coreTx = &s.station.transmitModel();
        s.server = std::make_unique<StationServer>(
            &s.station, AppSettings::instance(),
            NereusSDR::Test::seedUpgradedCoreToken(s.security.path()));
        s.client = std::make_unique<StationClient>(&s.gui, &s.proxy);
        auto* coreEnd = new HoldingTransport;
        s.guiEnd = new LoopbackTransport("gui");
        coreEnd->linkTo(s.guiEnd);
        s.client->startSession(s.guiEnd, s.server->token());
        s.server->acceptTransport(coreEnd);
        QTRY_VERIFY(s.client->isHandshakeComplete());
        s.windowTx = &s.gui.transmitModel();
        QCOMPARE(s.windowTx->cfcParaEqData(), s.coreTx->cfcParaEqData());
        QCOMPARE(s.windowTx->cfcPrecompDb(), s.coreTx->cfcPrecompDb());
        s.client->pauseWriteFlushForTest();
    }

private slots:
    void initTestCase()
    {
        // This test deliberately reloads its atomic settings file. Other
        // executables run in parallel and share the default Qt test sandbox.
        AppSettings::setProfileOverride(QStringLiteral("property-result-%1")
            .arg(QCoreApplication::applicationPid()));
    }
    void init() { AppSettings::instance().clear(); }
    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    void codecPreservesReadbackAndRejectsFractionalSequences()
    {
        const SessionPropertyResult value{"nnrAlpha", false, "Model unavailable", true, real("nnrAlpha", 1.25)};
        const auto source = SessionMessages::propertyResult("slice:7", 4294967295u, {value});
        SessionMessage decoded;
        QVERIFY(SessionMessages::decode(SessionMessages::encode(source), &decoded));
        QCOMPARE(decoded.writeId, 4294967295u);
        QCOMPARE(decoded.propertyResults.size(), 1);
        QCOMPARE(decoded.propertyResults[0].value.value.toDouble(), 1.25);
        QVERIFY(!decoded.propertyResults[0].accepted);
        for (double invalid : {-1.0, 0.0, 1.5, 4294967296.0}) {
            auto json = QJsonDocument::fromJson(SessionMessages::encode(source)).object();
            json["writeId"] = invalid;
            QVERIFY(!SessionMessages::decode(QJsonDocument(json).toJson(), &decoded));
        }
        auto json = QJsonDocument::fromJson(SessionMessages::encode(source)).object();
        QJsonArray duplicated = json["results"].toArray();
        duplicated.append(duplicated.first());
        json["results"] = duplicated;
        QVERIFY(!SessionMessages::decode(QJsonDocument(json).toJson(), &decoded));
        QVERIFY(SessionMessages::decode(SessionMessages::encode(
            SessionMessages::propertyWrite("slice:7", {real("nnrAlpha", 1.5)})), &decoded));
        QCOMPARE(decoded.writeId, 0u); // Older peers remain decodable.
    }

    void delayedResultCannotReplaceANewerEditAndRefusalReturnsCoreState()
    {
        QTemporaryDir security;
        QVERIFY(security.isValid());
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        RadioInfo info;
        info.macAddress = "AA:BB:CC:DD:EE:01";
        info.boardType = HPSDRHW::HermesLite;
        station.setLastRadioInfoForTest(info);
        const int id = station.addSlice();
        auto* coreSlice = station.sliceById(id);
        StationServer server(&station, AppSettings::instance(), NereusSDR::Test::seedUpgradedCoreToken(security.path()));
        RadioModel gui(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&gui, &proxy);
        auto* coreEnd = new HoldingTransport;
        auto* guiEnd = new LoopbackTransport("gui");
        coreEnd->linkTo(guiEnd);
        client.startSession(guiEnd, server.token());
        server.acceptTransport(coreEnd);
        QTRY_VERIFY(client.isHandshakeComplete());
        auto* slice = gui.sliceById(id);
        QVERIFY(slice);
        coreEnd->hold = true;
        slice->setNnrAlpha(1.75);
        QTRY_COMPARE(coreEnd->held.size(), 1);
        QCOMPARE(coreSlice->nnrAlpha(), 1.75);
        slice->setNnrAlpha(2.25);
        coreEnd->releaseFirst();
        QTRY_COMPARE(coreSlice->nnrAlpha(), 2.25);
        QCOMPARE(slice->nnrAlpha(), 2.25);
        QTRY_COMPARE(coreEnd->held.size(), 1);
        coreEnd->releaseFirst();
        QTRY_COMPARE(slice->nnrAlpha(), 2.25);

        coreSlice->setNnrSettingsApplier([](const NnrSettings&, QString* reason) -> std::optional<NnrSettings> {
            *reason = "Injected unavailable model";
            return std::nullopt;
        });
        coreEnd->hold = false;
        QSignalSpy completed(&client, &StationClient::propertyWriteCompleted);
        slice->setNnrAlpha(3.0);
        QTRY_COMPARE(slice->nnrAlpha(), 2.25);
        QTRY_VERIFY(!completed.isEmpty());
        QVERIFY(!completed.last().at(3).toBool());
        QVERIFY(!slice->nnrLastError().isEmpty());
        station.flushPendingSettingsSave();
        QCOMPARE(AppSettings::instance().value(coreSlice->nnrSettingsPrefix() + "NnrAlpha").toDouble(), 2.25);
    }

    void protocolMinorAloneDoesNotEnablePropertyResults()
    {
        QTemporaryDir security;
        QVERIFY(security.isValid());
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        const int id = station.addSlice();
        StationServer server(&station, AppSettings::instance(), NereusSDR::Test::seedUpgradedCoreToken(security.path()));
        RadioModel gui(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&gui, &proxy);
        auto* coreEnd = new HoldingTransport;
        coreEnd->advertisePropertyResults = false;
        auto* guiEnd = new LoopbackTransport("gui");
        coreEnd->linkTo(guiEnd);
        client.startSession(guiEnd, server.token());
        server.acceptTransport(coreEnd);
        QTRY_VERIFY(client.isHandshakeComplete());
        QVERIFY(!client.nnrControlAvailable());
        auto* slice = gui.sliceById(id);
        QVERIFY(slice);
        slice->setAfGain(37);
        QTRY_COMPARE(station.sliceById(id)->afGain(), 37);
        station.sliceById(id)->setAfGain(54);
        QTRY_COMPARE(slice->afGain(), 54);
    }

    void acceptedRemoteSettingsSurviveCoreModelRestartAndStaleGuiHydration()
    {
        QTemporaryDir security;
        QVERIFY(security.isValid());
        auto& settings = AppSettings::instance();
        RadioInfo info;
        info.macAddress = "AA:BB:CC:DD:EE:09";
        info.boardType = HPSDRHW::HermesLite;
        settings.setLastConnected(info.macAddress);

        for (int epoch = 0; epoch < 2; ++epoch) {
            if (epoch == 1) {
                // Recreate Core's settings/model boundary from its atomic file,
                // after the first station and GUI have both been destroyed.
                settings.clear();
                settings.load();
            }
            RadioModel station;
            station.setBoardForTest(info.boardType);
            station.setLastRadioInfoForTest(info);
            station.setConnectionStateForTest(ConnectionState::Connected);
            const int aId = station.addSlice();
            const int bId = station.addSlice();
            QVERIFY(aId >= 0 && bId >= 0 && aId != bId);
            auto* coreA = station.sliceById(aId);
            auto* coreB = station.sliceById(bId);
            StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(security.path()));
            RadioModel gui(RadioModel::Role::Remote);
            // Stale client preferences must lose to the startup snapshot.
            gui.pureSignalSettings()->setMoxDelaySeconds(0.9);
            gui.pureSignalSettings()->setLoopDelaySeconds(80.0);
            SettingsProxy proxy;
            StationClient client(&gui, &proxy);
            auto* coreEnd = new LoopbackTransport("station");
            auto* guiEnd = new LoopbackTransport("gui");
            coreEnd->linkTo(guiEnd);
            client.startSession(guiEnd, server.token());
            server.acceptTransport(coreEnd);
            QTRY_VERIFY(client.isHandshakeComplete());
            auto* a = gui.sliceById(aId);
            auto* b = gui.sliceById(bId);
            QVERIFY(a && b);
            if (epoch == 0) {
                a->setNnrAlpha(1.75);
                b->setNnrAlpha(2.25);
                b->setNnrReleaseMs(83.5);
                gui.pureSignalSettings()->setMoxDelaySeconds(0.4);
                gui.pureSignalSettings()->setLoopDelaySeconds(17.25);
                gui.pureSignalSettings()->setAutoCalEnabled(true);
                QTRY_COMPARE(coreA->nnrAlpha(), 1.75);
                QTRY_COMPARE(coreB->nnrAlpha(), 2.25);
                QTRY_COMPARE(coreB->nnrReleaseMs(), 83.5);
                QTRY_COMPARE(station.pureSignalSettings()->moxDelaySeconds(), 0.4);
                QTRY_COMPARE(station.pureSignalSettings()->loopDelaySeconds(), 17.25);
                QTRY_VERIFY(station.pureSignalSettings()->autoCalEnabled());
                coreB->setNnrSettingsApplier([](const NnrSettings&, QString* reason)
                    -> std::optional<NnrSettings> {
                    *reason = "Injected unavailable model";
                    return std::nullopt;
                });
                b->setNnrAlpha(3.75);
                QTRY_COMPARE(b->nnrAlpha(), 2.25);
                // This is the same synchronous flush used by daemon shutdown;
                // no debounce timer or dialog close is needed to keep the edit.
                station.flushPendingSettingsSave();
                QVERIFY(station.settingsSaveError().isEmpty());
            } else {
                QCOMPARE(coreA->nnrAlpha(), 1.75);
                QCOMPARE(coreB->nnrAlpha(), 2.25);
                QCOMPARE(coreB->nnrReleaseMs(), 83.5);
                QCOMPARE(a->nnrAlpha(), 1.75);
                QCOMPARE(b->nnrAlpha(), 2.25);
                QCOMPARE(b->nnrReleaseMs(), 83.5);
                QCOMPARE(station.pureSignalSettings()->moxDelaySeconds(), 0.4);
                QCOMPARE(station.pureSignalSettings()->loopDelaySeconds(), 17.25);
                QCOMPARE(gui.pureSignalSettings()->moxDelaySeconds(), 0.4);
                QCOMPARE(gui.pureSignalSettings()->loopDelaySeconds(), 17.25);
                QVERIFY(gui.pureSignalSettings()->autoCalEnabled());
                for (const auto& wire : coreEnd->received()) {
                    SessionMessage message;
                    QVERIFY(SessionMessages::decode(wire, &message));
                    // Record subscriptions and the automatic settings
                    // validation only read Core state. Neither can hydrate
                    // stale client preferences back into the Core.
                    QVERIFY2(message.kind != SessionMessageKind::CommandInvoke
                                 || message.commandVerb == "records.subscribe"
                                 || message.commandVerb == "station.validateSettings",
                             message.commandVerb.constData());
                    QVERIFY(message.kind != SessionMessageKind::PropertyWrite);
                }
            }
            client.disconnectFromStation("Restart fixture complete");
        }
    }

    void rawSettingsCannotBypassNormalModelValidation()
    {
        auto& settings = AppSettings::instance();
        const QString key = "hardware/AA:BB:CC:DD:EE:01/slices/7/nnr/NnrAlpha";
        settings.setValue(key, 1.75);
        SettingsProxyServer server(settings);
        const auto result = server.applyInboundWrite(key, 99.0, "test");
        QVERIFY(!result.accepted);
        QVERIFY2(result.reason.contains("their own controls"), qPrintable(result.reason));
        QCOMPARE(settings.value(key).toDouble(), 1.75);
    }

    // Inbound sibling lane, fix round 1: a Core mode change lands while
    // the operator's filter edge is still waiting for the window's flush.
    // Thetis keeps filter edges per mode (SetRX1Mode loads the new mode's
    // own LastFilter, console.cs:34513 [v2.10.3.15]), so the unsent edge
    // from the old mode is dropped: the window shows the new mode's
    // filter and sends no write for it.
    void coreModeChangeDropsTheOperatorsUnsentFilterEdge()
    {
        QTemporaryDir security;
        QVERIFY(security.isValid());
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        RadioInfo info;
        info.macAddress = "AA:BB:CC:DD:EE:01";
        info.boardType = HPSDRHW::HermesLite;
        station.setLastRadioInfoForTest(info);
        const int id = station.addSlice();
        auto* coreSlice = station.sliceById(id);
        coreSlice->setDspMode(DSPMode::USB);
        StationServer server(&station, AppSettings::instance(), NereusSDR::Test::seedUpgradedCoreToken(security.path()));
        RadioModel gui(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&gui, &proxy);
        auto* coreEnd = new HoldingTransport;
        auto* guiEnd = new LoopbackTransport("gui");
        coreEnd->linkTo(guiEnd);
        client.startSession(guiEnd, server.token());
        server.acceptTransport(coreEnd);
        QTRY_VERIFY(client.isHandshakeComplete());
        auto* slice = gui.sliceById(id);
        QVERIFY(slice);
        QCOMPARE(slice->dspMode(), DSPMode::USB);
        client.pauseWriteFlushForTest();

        // The operator's edge, not yet sent.
        const int operatorLow = slice->filterLow() + 170;
        slice->setFilterLow(operatorLow);
        QCOMPARE(slice->filterLow(), operatorLow);

        // The Core changes mode before the window's next flush.
        coreSlice->setDspMode(DSPMode::CWU);
        // The "differs" guard relies on the Core and the window sharing one settings store in-process.
        QVERIFY(coreSlice->filterLow() != operatorLow);
        const int coreLow = coreSlice->filterLow();
        const int coreHigh = coreSlice->filterHigh();
        QTRY_COMPARE(slice->dspMode(), DSPMode::CWU);

        // The window shows the new mode's filter, not the old mode's edge.
        QCOMPARE(slice->filterLow(), coreLow);
        QCOMPARE(slice->filterHigh(), coreHigh);

        // And the flush sends no write for it: the edit was dropped.
        QSignalSpy sent(guiEnd, &LoopbackTransport::outboundText);
        client.flushWritesForTest();
        QCOMPARE(propertyWrites(sent), 0);
        QCOMPARE(coreSlice->dspMode(), DSPMode::CWU);
        QCOMPARE(coreSlice->filterLow(), coreLow);
        QCOMPARE(coreSlice->filterHigh(), coreHigh);
    }

    // The same side effect on an edge already SENT and not yet answered.
    // The two edges share filterChanged, so the operator's next edge
    // re-reads both: the one in flight must still be the operator's, not
    // the edge the Core's mode change left on the window.
    void coreModeChangeKeepsTheOperatorsUnansweredFilterEdge()
    {
        QTemporaryDir security;
        QVERIFY(security.isValid());
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        RadioInfo info;
        info.macAddress = "AA:BB:CC:DD:EE:01";
        info.boardType = HPSDRHW::HermesLite;
        station.setLastRadioInfoForTest(info);
        const int id = station.addSlice();
        auto* coreSlice = station.sliceById(id);
        coreSlice->setDspMode(DSPMode::USB);
        StationServer server(&station, AppSettings::instance(), NereusSDR::Test::seedUpgradedCoreToken(security.path()));
        RadioModel gui(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&gui, &proxy);
        auto* coreEnd = new HoldingTransport;
        auto* guiEnd = new LoopbackTransport("gui");
        coreEnd->linkTo(guiEnd);
        client.startSession(guiEnd, server.token());
        server.acceptTransport(coreEnd);
        QTRY_VERIFY(client.isHandshakeComplete());
        auto* slice = gui.sliceById(id);
        QVERIFY(slice);
        QCOMPARE(slice->dspMode(), DSPMode::USB);
        client.pauseWriteFlushForTest();

        // The operator's edge leaves the window and is still on its way.
        const int operatorLow = slice->filterLow() + 170;
        slice->setFilterLow(operatorLow);
        guiEnd->setHoldsOutgoing(true);
        client.flushWritesForTest();

        // The Core changes mode first; its delta reaches the window.
        coreSlice->setDspMode(DSPMode::CWU);
        // The "differs" guard relies on the Core and the window sharing one settings store in-process.
        QVERIFY(coreSlice->filterLow() != operatorLow);
        QTRY_COMPARE(slice->dspMode(), DSPMode::CWU);

        // The operator's next edge re-reads both edges.
        const int operatorHigh = slice->filterHigh() + 230;
        slice->setFilterHigh(operatorHigh);
        guiEnd->setHoldsOutgoing(false);
        client.flushWritesForTest();

        QTRY_COMPARE(coreSlice->filterHigh(), operatorHigh);
        QCOMPARE(coreSlice->filterLow(), operatorLow);
        QCOMPARE(coreSlice->dspMode(), DSPMode::CWU);
        QCOMPARE(slice->filterLow(), operatorLow);
        QCOMPARE(slice->filterHigh(), operatorHigh);
    }

    // Another row: TransmitModel::setLineInBoost sets lineInGain to the
    // boost's index. A Core boost change landing before the window sends
    // the operator's own line-in gain is a change from elsewhere, so the
    // unsent gain is dropped (fix round 1): the window shows the gain the
    // Core's boost set and sends no write for it.
    void coreLineInBoostDropsTheOperatorsUnsentLineInGain()
    {
        QTemporaryDir security;
        QVERIFY(security.isValid());
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        RadioInfo info;
        info.macAddress = "AA:BB:CC:DD:EE:01";
        info.boardType = HPSDRHW::HermesLite;
        station.setLastRadioInfoForTest(info);
        station.addSlice();
        TransmitModel& coreTx = station.transmitModel();
        coreTx.setLineInBoost(0.0);
        StationServer server(&station, AppSettings::instance(), NereusSDR::Test::seedUpgradedCoreToken(security.path()));
        RadioModel gui(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&gui, &proxy);
        auto* coreEnd = new HoldingTransport;
        auto* guiEnd = new LoopbackTransport("gui");
        coreEnd->linkTo(guiEnd);
        client.startSession(guiEnd, server.token());
        server.acceptTransport(coreEnd);
        QTRY_VERIFY(client.isHandshakeComplete());
        TransmitModel& windowTx = gui.transmitModel();
        QCOMPARE(windowTx.lineInGain(), coreTx.lineInGain());
        client.pauseWriteFlushForTest();

        // The operator's gain, not yet sent.
        const int operatorGain = 7;
        QVERIFY(windowTx.lineInGain() != operatorGain);
        windowTx.setLineInGain(operatorGain);

        // The Core's boost changes before the window's next flush.
        coreTx.setLineInBoost(6.0);
        QVERIFY(coreTx.lineInGain() != operatorGain);
        const int coreGain = coreTx.lineInGain();
        QTRY_COMPARE(windowTx.lineInBoost(), 6.0);

        // The window shows the Core's gain, not the operator's.
        QCOMPARE(windowTx.lineInGain(), coreGain);

        // And the flush sends no write for it.
        QSignalSpy sent(guiEnd, &LoopbackTransport::outboundText);
        client.flushWritesForTest();
        QCOMPARE(propertyWrites(sent), 0);
        QCOMPARE(coreTx.lineInGain(), coreGain);
        QCOMPARE(coreTx.lineInBoost(), 6.0);
    }

    // Fix round 2, I1: the operator drags, the drag is sent (W1, still on
    // its way), drags again (unsent), and then a Core mode change cancels
    // the unsent drag. The window falls back to W1, whose answer applies,
    // so the window ends where the Core is. The Core never sends its
    // writer a correction of its own write.
    void coreModeChangeAfterAReEditEndsWhereTheCoreIs()
    {
        SliceSession s;
        startSliceSession(s);
        if (QTest::currentTestFailed()) { return; }

        const int firstLow = s.slice->filterLow() + 170;
        s.slice->setFilterLow(firstLow);
        s.guiEnd->setHoldsOutgoing(true);
        s.client->flushWritesForTest();
        s.slice->setFilterLow(firstLow + 50);

        s.coreSlice->setDspMode(DSPMode::CWU);
        QTRY_COMPARE(s.slice->dspMode(), DSPMode::CWU);
        // W1 is what the window now shows and still holds.
        QCOMPARE(s.slice->filterLow(), firstLow);

        s.guiEnd->clearReceived();
        s.guiEnd->setHoldsOutgoing(false);
        QTRY_COMPARE(s.coreSlice->filterLow(), firstLow);
        QTRY_VERIFY(s.guiEnd->receivedKinds().contains("property.result"));
        QCOMPARE(s.slice->filterLow(), s.coreSlice->filterLow());
        QCOMPARE(s.slice->filterHigh(), s.coreSlice->filterHigh());

        QSignalSpy sent(s.guiEnd, &LoopbackTransport::outboundText);
        s.client->flushWritesForTest();
        QCOMPARE(propertyWrites(sent), 0);
    }

    // Fix round 2, I3: the delta that cancels the unsent edge carries an
    // edge of its own that is not the window's CW memory. That value is
    // what the window shows.
    void coreModeChangeShowsTheDeltasOwnFilterEdge()
    {
        SliceSession s;
        startSliceSession(s);
        if (QTest::currentTestFailed()) { return; }

        const int operatorLow = s.slice->filterLow() + 170;
        s.slice->setFilterLow(operatorLow);

        // Mode and edge change before the Core's flush: one delta.
        s.coreSlice->setDspMode(DSPMode::CWU);
        const int memoryLow = s.coreSlice->filterLow();
        const int coreLow = memoryLow - 60;
        QVERIFY(coreLow != operatorLow);
        s.coreSlice->setFilterLow(coreLow);
        QTRY_COMPARE(s.slice->dspMode(), DSPMode::CWU);

        QCOMPARE(s.slice->filterLow(), coreLow);
        QCOMPARE(s.slice->filterHigh(), s.coreSlice->filterHigh());
        QSignalSpy sent(s.guiEnd, &LoopbackTransport::outboundText);
        s.client->flushWritesForTest();
        QCOMPARE(propertyWrites(sent), 0);
        QCOMPARE(s.coreSlice->filterLow(), coreLow);
    }

    // Fix round 2 I2, made production in round 3: the window runs with
    // the SettingsProxy as its AppSettings backend, so it reads the
    // Core's LastFilter memory as a remote window does. The Core's CW
    // memory is its USB edges, so its mode delta carries no edges and the
    // window's own side effect lands on the same edges. The unsent edge
    // is still cancelled (console.cs:34513 [v2.10.3.15]): the window
    // shows the Core's edges, sends no write, and no settings.write
    // carries the cancelled edge to the Core.
    void coreModeChangeWithoutEdgesShowsTheCoresEdges()
    {
        SliceSession s;
        startSliceSession(s);
        if (QTest::currentTestFailed()) { return; }
        auto& settings = AppSettings::instance();
        const QString cw = lastFilterPrefix(s.coreSlice, DSPMode::CWU);
        QCOMPARE(classifySettingsKey(cw + QStringLiteral("FilterLow")), SettingsScope::Station);
        const int usbLow = s.coreSlice->filterLow();
        const int usbHigh = s.coreSlice->filterHigh();
        // The Core's CW memory is its USB edges; the window learns it
        // through the proxy.
        settings.setValue(cw + QStringLiteral("FilterLow"), usbLow);
        settings.setValue(cw + QStringLiteral("FilterHigh"), usbHigh);
        QTRY_COMPARE(s.proxy.value(cw + QStringLiteral("FilterHigh"), QVariant()).toInt(), usbHigh);

        QSignalSpy sent(s.guiEnd, &LoopbackTransport::outboundText);
        const int operatorLow = usbLow + 170;
        s.slice->setFilterLow(operatorLow);

        s.coreSlice->setDspMode(DSPMode::CWU);
        QCOMPARE(s.coreSlice->filterLow(), usbLow);
        QCOMPARE(s.coreSlice->filterHigh(), usbHigh);
        // What the window sends waits until the backend is the Core's
        // own store again (one process plays both ends).
        s.guiEnd->setHoldsOutgoing(true);
        {
            RemoteBackendScope remote(&s.proxy);
            QTRY_COMPARE(s.slice->dspMode(), DSPMode::CWU);
            QCOMPARE(s.slice->filterLow(), usbLow);
            QCOMPARE(s.slice->filterHigh(), usbHigh);
            s.client->flushWritesForTest();
        }
        QCOMPARE(propertyWrites(sent), 0);
        const auto writes = filterSettingsWrites(sent);
        QVERIFY(!writes.isEmpty());
        const QString usbLowKey = lastFilterPrefix(s.slice, DSPMode::USB) + QStringLiteral("FilterLow");
        bool usbLowWritten = false;
        for (const auto& write : writes) {
            QVERIFY2(write.second != QString::number(operatorLow),
                     qPrintable(write.first + QStringLiteral(" = ") + write.second));
            if (write.first.endsWith(usbLowKey)) {
                QCOMPARE(write.second, QString::number(usbLow));
                usbLowWritten = true;
            }
        }
        // The USB memory keeps the Core's low edge, not the cancelled one.
        QVERIFY(usbLowWritten);
        s.guiEnd->setHoldsOutgoing(false);
        QCOMPARE(s.coreSlice->filterLow(), usbLow);
    }

    // Round 3 reproduction 1, one shared store: USB to CWU with equal
    // edge memories. The delta carries only dspMode and nothing on the
    // window moves, but the unsent edge is still cancelled and never sent.
    void equalEdgeMemoriesStillDropTheUnsentEdge()
    {
        SliceSession s;
        startSliceSession(s);
        if (QTest::currentTestFailed()) { return; }
        auto& settings = AppSettings::instance();
        const QString cw = lastFilterPrefix(s.coreSlice, DSPMode::CWU);
        const int usbLow = s.coreSlice->filterLow();
        const int usbHigh = s.coreSlice->filterHigh();
        settings.setValue(cw + QStringLiteral("FilterLow"), usbLow);
        settings.setValue(cw + QStringLiteral("FilterHigh"), usbHigh);

        s.slice->setFilterLow(usbLow + 170);
        s.coreSlice->setDspMode(DSPMode::CWU);
        QTRY_COMPARE(s.slice->dspMode(), DSPMode::CWU);

        QCOMPARE(s.slice->filterLow(), usbLow);
        QCOMPARE(s.slice->filterHigh(), usbHigh);
        QSignalSpy sent(s.guiEnd, &LoopbackTransport::outboundText);
        s.client->flushWritesForTest();
        QCOMPARE(propertyWrites(sent), 0);
        QCOMPARE(s.coreSlice->filterLow(), usbLow);
    }

    // Round 3 reproduction 2: AM to SAM, both on their default edges. The
    // edges do not move, and the unsent AM edge is still dropped.
    void amToSamDropsTheUnsentEdge()
    {
        SliceSession s;
        startSliceSession(s, DSPMode::AM);
        if (QTest::currentTestFailed()) { return; }
        const int amLow = s.coreSlice->filterLow();
        const int amHigh = s.coreSlice->filterHigh();

        s.slice->setFilterLow(amLow + 1000);
        s.coreSlice->setDspMode(DSPMode::SAM);
        QCOMPARE(s.coreSlice->filterLow(), amLow);
        QCOMPARE(s.coreSlice->filterHigh(), amHigh);
        QTRY_COMPARE(s.slice->dspMode(), DSPMode::SAM);

        QCOMPARE(s.slice->filterLow(), amLow);
        QCOMPARE(s.slice->filterHigh(), amHigh);
        QSignalSpy sent(s.guiEnd, &LoopbackTransport::outboundText);
        s.client->flushWritesForTest();
        QCOMPARE(propertyWrites(sent), 0);
        QCOMPARE(s.coreSlice->filterLow(), amLow);
    }

    // Fix round 2, Minor 1: the operator moved the low edge only. The
    // window's CW memory keeps the old high edge, so the high hold does
    // not move, but it is behind the same notifier as the cancelled low
    // edge and the delta carries the Core's high: it is cancelled too,
    // and the old mode's high is not sent into the new mode.
    void coreModeChangeCancelsTheUnmovedSiblingEdge()
    {
        SliceSession s;
        startSliceSession(s);
        if (QTest::currentTestFailed()) { return; }
        auto& settings = AppSettings::instance();
        const QString cw = lastFilterPrefix(s.coreSlice, DSPMode::CWU);
        const int usbLow = s.coreSlice->filterLow();
        const int usbHigh = s.coreSlice->filterHigh();
        const int cwLow = usbLow + 100;
        const int cwHigh = usbHigh + 200;
        settings.setValue(cw + QStringLiteral("FilterLow"), cwLow);
        settings.setValue(cw + QStringLiteral("FilterHigh"), cwHigh);

        s.slice->setFilterLow(usbLow + 170);

        s.coreSlice->setDspMode(DSPMode::CWU);
        QCOMPARE(s.coreSlice->filterLow(), cwLow);
        QCOMPARE(s.coreSlice->filterHigh(), cwHigh);
        // The window's own CW memory keeps the USB high edge.
        settings.setValue(cw + QStringLiteral("FilterLow"), usbLow + 300);
        settings.setValue(cw + QStringLiteral("FilterHigh"), usbHigh);
        QTRY_COMPARE(s.slice->dspMode(), DSPMode::CWU);

        QCOMPARE(s.slice->filterLow(), cwLow);
        QCOMPARE(s.slice->filterHigh(), cwHigh);
        QSignalSpy sent(s.guiEnd, &LoopbackTransport::outboundText);
        s.client->flushWritesForTest();
        QCOMPARE(propertyWrites(sent), 0);
        QCOMPARE(s.coreSlice->filterHigh(), cwHigh);
    }
    // Fix round 4: a delta that repeats the current mode changes nothing
    // the edges depend on, so it cancels nothing; the operator's unsent
    // edge stays on the window and is sent.
    void repeatedModeKeepsAndSendsTheUnsentEdge()
    {
        SliceSession s;
        startSliceSession(s);
        if (QTest::currentTestFailed()) { return; }
        const int usbLow = s.coreSlice->filterLow();
        const int operatorLow = usbLow + 170;
        s.slice->setFilterLow(operatorLow);

        // Away and back before the Core's next flush: its delta carries
        // dspMode at USB, the mode the window already shows.
        QSignalSpy received(s.guiEnd, &LoopbackTransport::textReceived);
        s.coreSlice->setDspMode(DSPMode::LSB);
        s.coreSlice->setDspMode(DSPMode::USB);
        QTRY_VERIFY(deltaCarries(received, "dspMode"));
        QCOMPARE(s.slice->dspMode(), DSPMode::USB);

        QCOMPARE(s.slice->filterLow(), operatorLow);
        QSignalSpy sent(s.guiEnd, &LoopbackTransport::outboundText);
        s.client->flushWritesForTest();
        QVERIFY(propertyWrites(sent) >= 1);
        QTRY_COMPARE(s.coreSlice->filterLow(), operatorLow);
        QCOMPARE(s.coreSlice->dspMode(), DSPMode::USB);
    }

    // Fix round 4: every class and property name in the delta cancel
    // rules resolves in the mirror schema. A name that does not is
    // skipped when a delta applies, so its rule would be dead.
    void deltaCancelRuleNamesResolve()
    {
        const QList<QByteArray> unresolved = StationClient::unresolvedDeltaCancelRuleNames();
        QVERIFY2(unresolved.isEmpty(), unresolved.join(", ").constData());
    }

    // Fix round 4, the curve-to-scalars row: the window holds the ten-band
    // values (its curve does not decode) and has an unsent pre-compression
    // edit. A curve that decodes arrives from elsewhere and defines the
    // scalars: the edit is cancelled, the window shows the Core's value,
    // and nothing is sent.
    void coreCfcCurveDropsTheOperatorsUnsentScalar()
    {
        TxSession s;
        startTxSession(s);
        if (QTest::currentTestFailed()) { return; }
        QVERIFY(!cfcCurveDecodes(s.windowTx->cfcParaEqData()));
        const int startPrecomp = s.coreTx->cfcPrecompDb();
        const int operatorPrecomp = startPrecomp + 3;
        const int corePrecomp = startPrecomp + 7;
        s.windowTx->setCfcPrecompDb(operatorPrecomp);
        QCOMPARE(s.windowTx->cfcPrecompDb(), operatorPrecomp);

        const QString curve = cfcCurve(*s.coreTx, corePrecomp);
        QVERIFY(cfcCurveDecodes(curve));
        QSignalSpy received(s.guiEnd, &LoopbackTransport::textReceived);
        s.coreTx->setCfcParaEqData(curve);
        QCOMPARE(s.coreTx->cfcPrecompDb(), corePrecomp);
        QTRY_VERIFY(deltaCarries(received, "cfcParaEqData"));

        QCOMPARE(s.windowTx->cfcParaEqData(), curve);
        QCOMPARE(s.windowTx->cfcPrecompDb(), corePrecomp);
        QSignalSpy sent(s.guiEnd, &LoopbackTransport::outboundText);
        s.client->flushWritesForTest();
        QCOMPARE(propertyWrites(sent), 0);
        QCOMPARE(s.coreTx->cfcPrecompDb(), corePrecomp);
        QCOMPARE(s.coreTx->cfcParaEqData(), curve);
    }

    // Fix round 4: a curve that does not decode projects nothing onto the
    // scalars (TransmitModel::setCfcParaEqData), so it does not redefine
    // them. The operator's unsent pre-compression stays and is sent.
    void undecodableCfcCurveKeepsTheOperatorsUnsentScalar()
    {
        TxSession s;
        startTxSession(s);
        if (QTest::currentTestFailed()) { return; }
        QVERIFY(!cfcCurveDecodes(s.windowTx->cfcParaEqData()));
        const int startPrecomp = s.coreTx->cfcPrecompDb();
        const int operatorPrecomp = startPrecomp + 3;
        s.windowTx->setCfcPrecompDb(operatorPrecomp);

        const QString opaque = QStringLiteral("not-a-cfc-curve");
        QVERIFY(!cfcCurveDecodes(opaque));
        QSignalSpy received(s.guiEnd, &LoopbackTransport::textReceived);
        s.coreTx->setCfcParaEqData(opaque);
        QCOMPARE(s.coreTx->cfcPrecompDb(), startPrecomp);
        QTRY_VERIFY(deltaCarries(received, "cfcParaEqData"));
        QCOMPARE(s.windowTx->cfcParaEqData(), opaque);

        QCOMPARE(s.windowTx->cfcPrecompDb(), operatorPrecomp);
        QSignalSpy sent(s.guiEnd, &LoopbackTransport::outboundText);
        s.client->flushWritesForTest();
        QCOMPARE(propertyWrites(sent), 1);
        QTRY_COMPARE(s.coreTx->cfcPrecompDb(), operatorPrecomp);
        QCOMPARE(s.coreTx->cfcParaEqData(), opaque);
    }

    // Fix round 4, the five scalar-to-curve rows: both ends hold a curve
    // that decodes, and the operator has an unsent curve-only edit (a Q,
    // which leaves every rounded scalar as it was). A scalar changes
    // elsewhere and re-encodes the Core's curve: the edit is cancelled,
    // the window shows the Core's curve, and nothing is sent.
    void coreCfcScalarDropsTheOperatorsUnsentCurve_data()
    {
        QTest::addColumn<QByteArray>("scalar");
        QTest::newRow("cfcPrecompDb") << QByteArray("cfcPrecompDb");
        QTest::newRow("cfcPostEqGainDb") << QByteArray("cfcPostEqGainDb");
        QTest::newRow("cfcEqFreqJson") << QByteArray("cfcEqFreqJson");
        QTest::newRow("cfcCompressionJson") << QByteArray("cfcCompressionJson");
        QTest::newRow("cfcPostEqBandGainJson") << QByteArray("cfcPostEqBandGainJson");
    }
    void coreCfcScalarDropsTheOperatorsUnsentCurve()
    {
        QFETCH(QByteArray, scalar);
        TxSession s;
        startTxSession(s);
        if (QTest::currentTestFailed()) { return; }
        const QString start = cfcCurve(*s.coreTx, s.coreTx->cfcPrecompDb());
        s.coreTx->setCfcParaEqData(start);
        QTRY_COMPARE(s.windowTx->cfcParaEqData(), start);
        // That curve was the Core's own change, applied: nothing pending.
        {
            QSignalSpy idle(s.guiEnd, &LoopbackTransport::outboundText);
            s.client->flushWritesForTest();
            QCOMPARE(propertyWrites(idle), 0);
        }

        // The operator's curve-only edit, not yet sent.
        CfcProfile::Profile edited;
        QVERIFY(CfcProfile::decode(start, edited));
        QCOMPARE(static_cast<int>(edited.qg.size()), 10);
        edited.qg[0] = edited.qg[0] + 1.0;
        const QString operatorCurve = CfcProfile::encode(edited);
        QVERIFY(operatorCurve != start);
        const QString scalarsBefore = s.windowTx->cfcCompressionJson() + s.windowTx->cfcEqFreqJson()
            + s.windowTx->cfcPostEqBandGainJson()
            + QString::number(s.windowTx->cfcPrecompDb())
            + QString::number(s.windowTx->cfcPostEqGainDb());
        s.windowTx->setCfcParaEqData(operatorCurve);
        QCOMPARE(s.windowTx->cfcParaEqData(), operatorCurve);
        QCOMPARE(s.windowTx->cfcCompressionJson() + s.windowTx->cfcEqFreqJson()
                     + s.windowTx->cfcPostEqBandGainJson()
                     + QString::number(s.windowTx->cfcPrecompDb())
                     + QString::number(s.windowTx->cfcPostEqGainDb()),
                 scalarsBefore);

        // The scalar changes elsewhere before the window's next flush.
        QSignalSpy received(s.guiEnd, &LoopbackTransport::textReceived);
        TransmitModel& core = *s.coreTx;
        if (scalar == "cfcPrecompDb") {
            core.setCfcPrecompDb(core.cfcPrecompDb() + 4);
        } else if (scalar == "cfcPostEqGainDb") {
            core.setCfcPostEqGainDb(core.cfcPostEqGainDb() + 4);
        } else if (scalar == "cfcEqFreqJson") {
            core.setCfcEqFreq(5, core.cfcEqFreq(5) + 37);
        } else if (scalar == "cfcCompressionJson") {
            core.setCfcCompression(3, core.cfcCompression(3) + 2);
        } else {
            core.setCfcPostEqBandGain(3, core.cfcPostEqBandGain(3) + 2);
        }
        const QString coreCurve = core.cfcParaEqData();
        QVERIFY(coreCurve != start);
        QVERIFY(coreCurve != operatorCurve);
        QTRY_VERIFY(deltaCarries(received, scalar));

        QCOMPARE(s.windowTx->cfcParaEqData(), coreCurve);
        QCOMPARE(s.windowTx->property(scalar.constData()), core.property(scalar.constData()));
        QSignalSpy sent(s.guiEnd, &LoopbackTransport::outboundText);
        s.client->flushWritesForTest();
        QCOMPARE(propertyWrites(sent), 0);
        QCOMPARE(core.cfcParaEqData(), coreCurve);
    }
};

QTEST_MAIN(TestSessionPropertyResult)
#include "tst_session_property_result.moc"
