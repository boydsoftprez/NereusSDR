// no-port-check: NereusSDR-original test coverage.
// =================================================================
// tests/tst_ssb_mod_monitor.cpp  (NereusSDR)
// =================================================================
//
// The SSB Mod Monitor around its analyzer:
//
//   - RadioModel measures only while keyed in USB or LSB: the TX tap is on
//     the transmit channel, and the feedback feed open, only then. DIGU,
//     DIGL, AM, CW and FM feed nothing.
//   - PA FB reads PureSignal's feedback samples, never a receiver's, and
//     only while PureSignal runs.
//   - The Core's publisher sends only while keyed in USB or LSB and watched,
//     removes the record after, and RESET clears the analyzer.
//   - The record round-trips and refuses bad input.
//   - The applet does not read ON after the samples stop, says why PA FB is
//     off, and a panel that is not shown measures nothing and watches nothing.
//
// The in-process loopback only. No radio is keyed and nothing reaches a
// device: the MoxController is keyed with its pre-check lifted, and the
// transmit channel is never driven.
//
//   cmake --build build --target tst_ssb_mod_monitor
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_ssb_mod_monitor$' \
//       --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05  Corrections for review findings. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/PureSignal.h"
#include "core/SsbModulationAnalyzer.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/session/PureSignalSessionFacade.h"
#include "core/session/RecordStream.h"
#include "core/session/SessionMessages.h"
#include "core/session/SsbModMonitorPublisher.h"
#include "core/session/SsbModMonitorRecord.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/QuietRadioConnection.h"
#include "fakes/RemoteWindowHarness.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/applets/SsbModMonitorApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <chrono>
#include <cmath>
#include <memory>
#include <vector>

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;
using NereusSDR::Test::RemoteWindowHarness;

Q_DECLARE_METATYPE(NereusSDR::DSPMode)

namespace {

constexpr int kFs = 48000;

// The desktop's words, literally, so a change to them fails here; the phone
// carries its own copy of the first.
const QString kPaFeedbackOffWords = QStringLiteral(
    "PA FB reads PureSignal's feedback. PureSignal is off, or this radio does not have it.");
const QString kPaFeedbackOnWords = QStringLiteral(
    "Measure the PA output through PureSignal's feedback.");

QVector<float> tone(int frames, float level)
{
    QVector<float> iq(frames * 2);
    for (int i = 0; i < frames; ++i) {
        iq[2 * i] = level;
        iq[2 * i + 1] = 0.0f;
    }
    return iq;
}

void feedSteady(SsbModulationAnalyzer& a, double seconds, float level)
{
    a.startClockForTest(std::chrono::milliseconds(1000000));
    const QVector<float> block = tone(48, level);
    for (int ms = 0; ms < static_cast<int>(seconds * 1000.0); ++ms) {
        a.pushIq(block.constData(), 48);
        a.advanceClockForTest(std::chrono::milliseconds(1));
    }
}

std::unique_ptr<RadioModel> makeCore()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::HermesLite);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:42");
    info.name = QStringLiteral("Bench HL2");
    info.boardType = HPSDRHW::HermesLite;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    return model;
}

SliceModel* txSlice(RadioModel& core)
{
    SliceModel* slice = core.txBoundSlice();
    return slice != nullptr ? slice : core.slices().value(0);
}

bool keyCore(RadioModel& core)
{
    MoxController* mox = core.moxController();
    if (mox == nullptr) {
        return false;
    }
    QSignalSpy walked(mox, &MoxController::moxStateChanged);
    mox->setMoxCheck({});
    mox->setMox(true);
    return (walked.count() > 0 || walked.wait(2000)) && core.isTransmitting();
}

void unkeyCore(RadioModel& core)
{
    if (MoxController* mox = core.moxController()) {
        mox->setMox(false);
    }
}

// Clears the model's channel pointer before the channel goes.
struct InjectedChannel {
    explicit InjectedChannel(RadioModel& m) : model(m) { model.injectTxChannelForTest(&tx); }
    ~InjectedChannel() { model.injectTxChannelForTest(nullptr); }
    RadioModel& model;
    TxChannel tx{WdspEngine::kTxChannelId};
};

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
    RecordStream* stream(int source) const
    {
        return server->recordStreamForTest(SsbModMonitorRecord::streamName(source));
    }
    SsbModMonitorPublisher* publisher() const { return server->ssbModMonitorPublisherForTest(); }

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

QJsonObject goodFields()
{
    SsbModulationAnalyzer a;
    a.setSampleRate(kFs);
    feedSteady(a, 0.6, 0.5f);
    return SsbModMonitorRecord::toFields(a.snapshot(), 1234);
}

} // namespace

class TstSsbModMonitor : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(m_securityDir.isValid());
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("ssb-mod-monitor")));
    }

    void init() { QVERIFY(RemoteWindowHarness::clearIsolatedProfile()); }

    void cleanupTestCase() { QVERIFY(RemoteWindowHarness::removeIsolatedProfile()); }

    // ── The record ────────────────────────────────────────────────────────

    void theRecordRoundTrips()
    {
        SsbModulationAnalyzer a;
        a.setSampleRate(kFs);
        feedSteady(a, 0.6, 0.5f);
        const SsbModulationAnalyzer::Snapshot s = a.snapshot();
        QVERIFY(s.signalPresent);
        QVERIFY(s.crestDb.has_value());
        QVERIFY(s.scope.size() > static_cast<std::size_t>(SsbModMonitorRecord::kWireScopePoints));
        const QJsonObject fields = SsbModMonitorRecord::toFields(s, 1234);
        QCOMPARE(fields.value(QStringLiteral("atMs")).toDouble(), 1234.0);
        const auto back = SsbModMonitorRecord::fromFields(fields);
        QVERIFY(back.has_value());
        QCOMPARE(back->peakPct, s.peakPct);
        QCOMPARE(back->avgPct, s.avgPct);
        QCOMPARE(back->peakHoldPct, s.peakHoldPct);
        QCOMPARE(back->peakDbfs, s.peakDbfs);
        QCOMPARE(back->avgDbfs, s.avgDbfs);
        QCOMPARE(back->peakHoldDbfs, s.peakHoldDbfs);
        QCOMPARE(*back->crestDb, *s.crestDb);
        QCOMPARE(back->signalPresent, s.signalPresent);
        // Bounded, and the largest point survives.
        QVERIFY(back->scope.size() <= static_cast<std::size_t>(SsbModMonitorRecord::kWireScopePoints));
        QVERIFY(!back->scope.empty());
        QVERIFY(std::fabs(*std::max_element(back->scope.begin(), back->scope.end()) - 50.0f) <= 0.05f);
        QCOMPARE(SsbModMonitorRecord::sourceOfStream(QStringLiteral("txSsbModulation")), 0);
        QCOMPARE(SsbModMonitorRecord::sourceOfStream(QStringLiteral("txSsbModulationFeedback")), 1);
        QCOMPARE(SsbModMonitorRecord::sourceOfStream(QStringLiteral("spots")), -1);
    }

    void noCrestValueStaysNoValueOnTheWire()
    {
        SsbModulationAnalyzer::Snapshot s;
        QVERIFY(!s.crestDb.has_value());
        const QJsonObject fields = SsbModMonitorRecord::toFields(s, 1);
        QVERIFY(!fields.contains(QStringLiteral("crestDb")));
        const auto back = SsbModMonitorRecord::fromFields(fields);
        QVERIFY(back.has_value());
        QVERIFY(!back->crestDb.has_value());
    }

    void aSnapshotBeyondTheRangesStillDecodes()
    {
        SsbModulationAnalyzer::Snapshot s;
        s.peakPct = 1e9;
        s.avgDbfs = -700.0;
        s.peakHoldDbfs = 500.0;
        s.crestDb = 1e6;
        const auto back = SsbModMonitorRecord::fromFields(SsbModMonitorRecord::toFields(s, 1));
        QVERIFY(back.has_value());
        QCOMPARE(back->peakPct, SsbModMonitorRecord::kMaxPct);
        QCOMPARE(back->avgDbfs, SsbModMonitorRecord::kMinDbfs);
        QCOMPARE(back->peakHoldDbfs, SsbModMonitorRecord::kMaxDbfs);
        QCOMPARE(*back->crestDb, SsbModMonitorRecord::kMaxCrestDb);
    }

    void badFieldsAreNoRecord_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("clock as text") << "atMs" << QJsonValue(QStringLiteral("1"));
        QTest::newRow("clock negative") << "atMs" << QJsonValue(-1.0);
        QTest::newRow("present as a number") << "signalPresent" << QJsonValue(1);
        QTest::newRow("peak as text") << "peakPct" << QJsonValue(QStringLiteral("9"));
        QTest::newRow("peak negative") << "peakPct" << QJsonValue(-0.1);
        QTest::newRow("peak huge") << "peakPct" << QJsonValue(1e9);
        QTest::newRow("average negative") << "avgPct" << QJsonValue(-1.0);
        QTest::newRow("hold huge") << "peakHoldPct" << QJsonValue(4000.0);
        QTest::newRow("peak dB too low") << "peakDbfs" << QJsonValue(-121.0);
        QTest::newRow("average dB too high") << "avgDbfs" << QJsonValue(72.0);
        QTest::newRow("hold dB too high") << "peakHoldDbfs" << QJsonValue(1000.0);
        QTest::newRow("crest as text") << "crestDb" << QJsonValue(QStringLiteral("3"));
        QTest::newRow("crest huge") << "crestDb" << QJsonValue(1000.0);
        QTest::newRow("rate negative") << "scopeRateHz" << QJsonValue(-1);
        QTest::newRow("rate huge") << "scopeRateHz" << QJsonValue(2000000);
        QTest::newRow("rate not whole") << "scopeRateHz" << QJsonValue(8000.5);
        QTest::newRow("scope too long") << "scopePctTenths"
            << QJsonValue(QString::fromLatin1(
                   QByteArray(2 * (SsbModMonitorRecord::kWireScopePoints + 1), '\0').toBase64()));
        QTest::newRow("scope odd") << "scopePctTenths"
            << QJsonValue(QString::fromLatin1(QByteArray(3, '\0').toBase64()));
        QTest::newRow("scope negative") << "scopePctTenths"
            << QJsonValue(QString::fromLatin1(QByteArray::fromHex("ffff").toBase64()));
        QTest::newRow("scope not base64") << "scopePctTenths" << QJsonValue(QStringLiteral("***"));
    }

    void badFieldsAreNoRecord()
    {
        QFETCH(QString, key);
        QFETCH(QJsonValue, value);
        QJsonObject fields = goodFields();
        QVERIFY(SsbModMonitorRecord::fromFields(fields).has_value());
        fields.insert(key, value);
        QVERIFY(!SsbModMonitorRecord::fromFields(fields).has_value());
    }

    void aMissingFieldIsNoRecord_data()
    {
        QTest::addColumn<QString>("key");
        for (const char* key : {"atMs", "peakPct", "avgPct", "peakHoldPct", "peakDbfs", "avgDbfs",
                                "peakHoldDbfs", "signalPresent", "scopeRateHz", "scopePctTenths"}) {
            QTest::newRow(key) << QString::fromLatin1(key);
        }
    }

    void aMissingFieldIsNoRecord()
    {
        QFETCH(QString, key);
        QJsonObject fields = goodFields();
        fields.remove(key);
        QVERIFY(!SsbModMonitorRecord::fromFields(fields).has_value());
    }

    // ── RadioModel gating ─────────────────────────────────────────────────

    void theTxTapFollowsOneDefinition_data()
    {
        QTest::addColumn<DSPMode>("mode");
        QTest::addColumn<bool>("measured");
        QTest::newRow("USB") << DSPMode::USB << true;
        QTest::newRow("LSB") << DSPMode::LSB << true;
        QTest::newRow("AM") << DSPMode::AM << false;
        QTest::newRow("SAM") << DSPMode::SAM << false;
        QTest::newRow("DSB") << DSPMode::DSB << false;
        QTest::newRow("CWU") << DSPMode::CWU << false;
        QTest::newRow("CWL") << DSPMode::CWL << false;
        QTest::newRow("FM") << DSPMode::FM << false;
        QTest::newRow("DIGU") << DSPMode::DIGU << false;
        QTest::newRow("DIGL") << DSPMode::DIGL << false;
    }

    void theTxTapFollowsOneDefinition()
    {
        QFETCH(DSPMode, mode);
        QFETCH(bool, measured);
        std::unique_ptr<RadioModel> core = makeCore();
        InjectedChannel channel(*core);
        txSlice(*core)->setDspMode(mode);
        core->setSsbModTxTapEnabled(true);
        // Unkeyed: not attached in any mode.
        QVERIFY(!core->ssbTxActive());
        QCOMPARE(channel.tx.ssbModulationTapForTest(), nullptr);

        QVERIFY(keyCore(*core));
        QCOMPARE(core->ssbTxActive(), measured);
        QCOMPARE(channel.tx.ssbModulationTapForTest(),
                 measured ? core->ssbModulationAnalyzer(0) : nullptr);

        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());
        QCOMPARE(channel.tx.ssbModulationTapForTest(), nullptr);
        QVERIFY(!core->ssbTxActive());
    }

    void theTxTapFollowsAModeChangeDuringTheKey()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        InjectedChannel channel(*core);
        txSlice(*core)->setDspMode(DSPMode::USB);
        core->setSsbModTxTapEnabled(true);
        QVERIFY(keyCore(*core));
        QCOMPARE(channel.tx.ssbModulationTapForTest(), core->ssbModulationAnalyzer(0));
        txSlice(*core)->setDspMode(DSPMode::AM);
        QCOMPARE(channel.tx.ssbModulationTapForTest(), nullptr);
        txSlice(*core)->setDspMode(DSPMode::LSB);
        QCOMPARE(channel.tx.ssbModulationTapForTest(), core->ssbModulationAnalyzer(0));
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());
    }

    void noWantedTapMeansNoTapWhileKeyed()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        InjectedChannel channel(*core);
        txSlice(*core)->setDspMode(DSPMode::USB);
        // Off until a shown panel or a watching device asks.
        QVERIFY(!core->ssbModTxTapEnabled());
        QVERIFY(keyCore(*core));
        QCOMPARE(channel.tx.ssbModulationTapForTest(), nullptr);
        core->setSsbModTxTapEnabled(true);
        QCOMPARE(channel.tx.ssbModulationTapForTest(), core->ssbModulationAnalyzer(0));
        core->setSsbModTxTapEnabled(false);
        QCOMPARE(channel.tx.ssbModulationTapForTest(), nullptr);
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());
    }

    void aReceiverIsNeverReadAsPaFeedback()
    {
        // The review's finding: stream 1 is a logical receiver. With
        // PureSignal off, or on, its samples must not reach the analyzer.
        std::unique_ptr<RadioModel> core = makeCore();
        txSlice(*core)->setDspMode(DSPMode::USB);
        core->setSsbModFeedbackWanted(true);
        QVERIFY(keyCore(*core));
        const QVector<float> samples = tone(256, 0.5f);
        for (int stream = 0; stream < 5; ++stream) {
            core->forkIqToTapsForTest(stream, samples);
        }
        QCOMPARE(core->ssbModulationAnalyzer(1)->snapshot().framesSeen, std::uint64_t(0));

        PureSignal* ps = core->installPureSignalForTest(nullptr);
        ps->setPsEnabledForTest(true);
        for (int stream = 0; stream < 5; ++stream) {
            core->forkIqToTapsForTest(stream, samples);
        }
        QCOMPARE(core->ssbModulationAnalyzer(1)->snapshot().framesSeen, std::uint64_t(0));
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());
    }

    void feedbackNeedsPureSignalKeyAndWant()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        txSlice(*core)->setDspMode(DSPMode::USB);
        SsbModulationAnalyzer* fb = core->ssbModulationAnalyzer(1);
        const QVector<float> samples = tone(256, 0.5f);
        const auto fed = [&]() {
            const std::uint64_t before = fb->snapshot().framesSeen;
            core->feedSsbModFeedbackForTest(samples);
            return fb->snapshot().framesSeen > before;
        };

        // PureSignal absent.
        core->setSsbModFeedbackWanted(true);
        QVERIFY(!core->ssbModFeedbackInUse());
        QVERIFY(keyCore(*core));
        QVERIFY(!fed());
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());

        // PureSignal present and off.
        PureSignal* ps = core->installPureSignalForTest(nullptr);
        QVERIFY(!core->ssbModFeedbackInUse());
        QVERIFY(keyCore(*core));
        QVERIFY(!fed());

        // Turned on during the key: it starts, at PureSignal's rate.
        QSignalSpy inUse(core.get(), &RadioModel::ssbModFeedbackInUseChanged);
        ps->setPsEnabledForTest(true);
        QCOMPARE(inUse.count(), 1);
        QVERIFY(core->ssbModFeedbackInUse());
        QVERIFY(fed());

        // Another mode, then not wanted, then off: each stops it.
        txSlice(*core)->setDspMode(DSPMode::AM);
        QVERIFY(!fed());
        txSlice(*core)->setDspMode(DSPMode::LSB);
        QVERIFY(fed());
        core->setSsbModFeedbackWanted(false);
        QVERIFY(!fed());
        core->setSsbModFeedbackWanted(true);
        QVERIFY(fed());
        ps->setPsEnabledForTest(false);
        QVERIFY(!fed());
        ps->setPsEnabledForTest(true);
        QVERIFY(fed());

        // Unkeyed, with PureSignal on.
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());
        QVERIFY(!fed());
    }

    void feedbackIsMeasuredAtTheRateItArrivesAt_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<int>("protocol");
        QTest::addColumn<int>("wireRate");
        QTest::addColumn<int>("expected");
        const int hl2 = static_cast<int>(HPSDRHW::HermesLite);
        const int hermes = static_cast<int>(HPSDRHW::Hermes);
        const int saturn = static_cast<int>(HPSDRHW::Saturn);
        // Protocol 1 has one wire rate for every DDC, the pair included.
        QTest::newRow("HL2 P1 48k") << hl2 << 1 << 48000 << 48000;
        QTest::newRow("HL2 P1 384k") << hl2 << 1 << 384000 << 384000;
        QTest::newRow("Hermes P1 96k") << hermes << 1 << 96000 << 96000;
        // Protocol 2 runs the pair at 192 kHz whatever the receivers do.
        QTest::newRow("Saturn P2 48k") << saturn << 2 << 48000 << 192000;
        QTest::newRow("Saturn P2 384k") << saturn << 2 << 384000 << 192000;
    }

    void feedbackIsMeasuredAtTheRateItArrivesAt()
    {
        QFETCH(int, board);
        QFETCH(int, protocol);
        QFETCH(int, wireRate);
        QFETCH(int, expected);
        std::unique_ptr<RadioModel> core = makeCore();
        core->setBoardForTest(static_cast<HPSDRHW>(board));
        RadioInfo info;
        info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:42");
        info.boardType = static_cast<HPSDRHW>(board);
        info.protocol = protocol == 2 ? ProtocolVersion::Protocol2 : ProtocolVersion::Protocol1;
        core->setLastRadioInfoForTest(info);
        core->setConnectionRateForTest(wireRate, 1);
        txSlice(*core)->setDspMode(DSPMode::USB);
        PureSignal* ps = core->installPureSignalForTest(nullptr);
        ps->setPsEnabledForTest(true);
        core->setSsbModFeedbackWanted(true);
        QVERIFY(keyCore(*core));
        QCOMPARE(core->ssbModulationAnalyzer(1)->sampleRate(), expected);
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());
    }

    void feedbackComesThroughTheConnectionsPairedSignal()
    {
        // Outlives the model, which holds it.
        NereusSDR::Test::QuietRadioConnection connection;
        std::unique_ptr<RadioModel> core = makeCore();
        txSlice(*core)->setDspMode(DSPMode::USB);
        PureSignal* ps = core->installPureSignalForTest(nullptr);
        ps->setPsEnabledForTest(true);
        core->setSsbModFeedbackWanted(true);
        core->injectConnectionForTest(&connection);
        core->wireSsbModFeedbackForTest();
        core->setConnectionRateForTest(48000, 1);
        QVERIFY(keyCore(*core));

        // The first of the pair is the feedback, the second the reference.
        const QVector<float> feedback = tone(4800, 0.5f);
        const QVector<float> reference = tone(4800, 0.1f);
        emit connection.psPairedIqDataReceived(2, feedback, 3, reference);
        const SsbModulationAnalyzer::Snapshot snap = core->ssbModulationAnalyzer(1)->snapshot();
        QCOMPARE(snap.framesSeen, std::uint64_t(4800));
        QVERIFY2(std::fabs(snap.peakPct - 50.0) < 0.5, qPrintable(QString::number(snap.peakPct)));

        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());
        emit connection.psPairedIqDataReceived(2, feedback, 3, reference);
        QCOMPARE(core->ssbModulationAnalyzer(1)->snapshot().framesSeen, std::uint64_t(4800));
        core->injectConnectionForTest(nullptr);
    }

    void eachOverStartsFresh()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        txSlice(*core)->setDspMode(DSPMode::USB);
        SsbModulationAnalyzer* tx = core->ssbModulationAnalyzer(0);
        QVERIFY(keyCore(*core));
        feedSteady(*tx, 0.3, 0.5f);
        QVERIFY(tx->snapshot().peakHoldPct > 49.0);
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());
        // The hold survives the unkey to finish its decay, and goes at the
        // next key.
        QVERIFY(tx->snapshot().peakHoldPct > 49.0);
        QVERIFY(keyCore(*core));
        QCOMPARE(tx->snapshot().peakHoldPct, 0.0);
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());
    }

    // ── The publisher ─────────────────────────────────────────────────────

    void aHostedCoreLeavesTheLocalPanelMeasuring()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        InjectedChannel channel(*core);
        txSlice(*core)->setDspMode(DSPMode::USB);
        PureSignal* ps = core->installPureSignalForTest(nullptr);
        ps->setPsEnabledForTest(true);
        SsbModMonitorApplet local(core.get());
        local.show();
        QVERIFY(core->ssbModTxTapEnabled());

        auto session = std::make_unique<Session>(core.get(), m_securityDir.path(), this);
        // Hosting starts: the shown panel keeps measuring.
        QVERIFY(core->ssbModTxTapEnabled());
        QVERIFY(session->connect());
        session->publisher()->tickForTest();
        QVERIFY(core->ssbModTxTapEnabled());
        QVERIFY(keyCore(*core));
        session->publisher()->tickForTest();
        QCOMPARE(channel.tx.ssbModulationTapForTest(), core->ssbModulationAnalyzer(0));
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());

        // The panel on PA FB while a device watches TX I/Q: both stay on,
        // through the publisher's reads.
        local.setSource(SsbModMonitorApplet::Source::PaFeedback);
        QVERIFY(!core->ssbModTxTapEnabled());
        auto device = std::make_unique<SsbModMonitorApplet>(&session->window);
        // Both panels read one saved choice here; the device's is its own.
        device->setSource(SsbModMonitorApplet::Source::TxIq);
        device->show();
        QTRY_COMPARE(session->stream(0)->subscriberCount(), 1);
        QVERIFY(keyCore(*core));
        QTRY_VERIFY(session->publisher()->sourceOpen(0));
        session->publisher()->tickForTest();
        session->publisher()->tickForTest();
        QVERIFY(core->ssbModFeedbackWanted());
        QVERIFY(core->ssbModTxTapEnabled());
        QCOMPARE(channel.tx.ssbModulationTapForTest(), core->ssbModulationAnalyzer(0));

        // The device leaves: only what the panel wants stays.
        device->hide();
        QTRY_COMPARE(session->stream(0)->subscriberCount(), 0);
        QTRY_VERIFY(!session->publisher()->sourceOpen(0));
        QVERIFY(core->ssbModFeedbackWanted());
        QVERIFY(!core->ssbModTxTapEnabled());
        QCOMPARE(channel.tx.ssbModulationTapForTest(), nullptr);
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());
        device.reset();

        // A panel that is hidden stops only its own wants.
        local.hide();
        QVERIFY(!core->ssbModFeedbackWanted());
        session.reset();
        QVERIFY(!core->ssbModTxTapEnabled());
    }

    void aPublisherOpeningMidOverLeavesThePanelAlone()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        txSlice(*core)->setDspMode(DSPMode::USB);
        Session s(core.get(), m_securityDir.path(), this);
        QVERIFY(s.connect());
        SsbModMonitorApplet local(core.get());
        local.show();
        QVERIFY(keyCore(*core));
        SsbModulationAnalyzer* tx = core->ssbModulationAnalyzer(0);
        feedSteady(*tx, 0.3, 0.8f);
        const std::uint64_t frames = tx->snapshot().framesSeen;
        QVERIFY(frames > 0);

        SsbModMonitorApplet device(&s.window);
        device.show();
        QTRY_VERIFY(s.publisher()->sourceOpen(0));
        s.publisher()->tickForTest();
        const SsbModulationAnalyzer::Snapshot after = tx->snapshot();
        QVERIFY2(after.peakHoldPct > 79.0, qPrintable(QString::number(after.peakHoldPct)));
        QCOMPARE(after.framesSeen, frames);
        QVERIFY(!after.scope.empty());
        QVERIFY(after.signalPresent);
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());
    }

    // The panel restarts only its own window: a watching device reads the
    // same hold.
    void aLocalPanelShowingOrChangingSourceLeavesTheHoldAlone()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        SsbModMonitorApplet local(core.get());
        SsbModulationAnalyzer* tx = core->ssbModulationAnalyzer(0);
        SsbModulationAnalyzer* fb = core->ssbModulationAnalyzer(1);
        feedSteady(*tx, 0.3, 0.8f);
        feedSteady(*fb, 0.3, 0.8f);
        const auto held = [](SsbModulationAnalyzer* a) { return a->snapshot().peakHoldPct > 79.0; };

        local.show();
        QVERIFY(held(tx));
        QVERIFY(held(fb));
        local.setSource(SsbModMonitorApplet::Source::PaFeedback);
        QVERIFY(held(tx));
        QVERIFY(held(fb));
        local.setSource(SsbModMonitorApplet::Source::TxIq);
        QVERIFY(held(tx));
        QVERIFY(held(fb));
        local.hide();
        local.show();
        QVERIFY(held(tx));
        QVERIFY(held(fb));
    }

    void aDeviceChangingSourceLeavesThePanelsHoldAlone()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        txSlice(*core)->setDspMode(DSPMode::USB);
        PureSignal* ps = core->installPureSignalForTest(nullptr);
        ps->setPsEnabledForTest(true);
        Session s(core.get(), m_securityDir.path(), this);
        QVERIFY(s.connect());
        SsbModMonitorApplet local(core.get());
        local.show();
        SsbModMonitorApplet device(&s.window);
        device.setSource(SsbModMonitorApplet::Source::PaFeedback);
        device.show();
        QTRY_COMPARE(s.stream(1)->subscriberCount(), 1);
        QVERIFY(keyCore(*core));
        SsbModulationAnalyzer* tx = core->ssbModulationAnalyzer(0);
        feedSteady(*tx, 0.3, 0.8f);

        device.setSource(SsbModMonitorApplet::Source::TxIq);
        QTRY_COMPARE(s.stream(0)->subscriberCount(), 1);
        QVERIFY2(tx->snapshot().peakHoldPct > 79.0, qPrintable(QString::number(tx->snapshot().peakHoldPct)));
        for (const QByteArray& wire : s.coreEnd->received()) {
            QVERIFY2(!wire.contains("txSsbModMonitor.reset"), wire.constData());
        }
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());
    }

    void aPublisherLeavingTakesItsWantsWithIt()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        txSlice(*core)->setDspMode(DSPMode::USB);
        auto session = std::make_unique<Session>(core.get(), m_securityDir.path(), this);
        QVERIFY(session->connect());
        auto device = std::make_unique<SsbModMonitorApplet>(&session->window);
        device->show();
        QTRY_COMPARE(session->stream(0)->subscriberCount(), 1);
        QVERIFY(keyCore(*core));
        QTRY_VERIFY(session->publisher()->sourceOpen(0));
        QVERIFY(core->ssbModTxTapEnabled());
        device.reset();
        session.reset();
        QVERIFY(!core->ssbModTxTapEnabled());
        QVERIFY(!core->ssbModFeedbackWanted());
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());
    }

    void thePublisherSendsOnlyKeyedInUsbOrLsbAndWatched()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        InjectedChannel channel(*core);
        txSlice(*core)->setDspMode(DSPMode::USB);
        Session s(core.get(), m_securityDir.path(), this);
        QCOMPARE(s.server->txSsbModMonitorVersion(), 1);
        QVERIFY(s.connect());
        QCOMPARE(s.client->capabilities().txSsbModMonitorVersion, 1);
        QVERIFY(s.publisher() != nullptr);

        // Nobody watching, keyed or not: nothing runs.
        QVERIFY(!s.publisher()->publishing());
        QVERIFY(keyCore(*core));
        s.publisher()->tickForTest();
        QVERIFY(!s.publisher()->sourceOpen(0));
        QCOMPARE(channel.tx.ssbModulationTapForTest(), nullptr);
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());

        // Watching, unkeyed: no analyzer and no record.
        SsbModMonitorApplet remote(&s.window);
        remote.show();
        QTRY_COMPARE(s.stream(0)->subscriberCount(), 1);
        QTRY_VERIFY(s.publisher()->publishing());
        s.publisher()->tickForTest();
        QVERIFY(!s.publisher()->sourceOpen(0));
        QVERIFY(!s.stream(0)->contains(QLatin1String(SsbModMonitorRecord::kRecordId)));

        // Keyed in USB: the tap, the record, and the window's copy.
        QVERIFY(keyCore(*core));
        QTRY_VERIFY(s.publisher()->sourceOpen(0));
        QCOMPARE(channel.tx.ssbModulationTapForTest(), core->ssbModulationAnalyzer(0));
        feedSteady(*core->ssbModulationAnalyzer(0), 0.3, 0.5f);
        QTRY_VERIFY([&]() {
            const auto snap = s.window.stationSsbModMonitorSnapshot(0);
            return snap && snap->signalPresent;
        }());
        remote.tickForTest();
        QVERIFY(remote.lampTextForTest() != QStringLiteral("NO RF"));

        // Another mode: closed, and the copy goes.
        txSlice(*core)->setDspMode(DSPMode::AM);
        QTRY_VERIFY(!s.publisher()->sourceOpen(0));
        QCOMPARE(channel.tx.ssbModulationTapForTest(), nullptr);
        QTRY_VERIFY(!s.window.stationSsbModMonitorSnapshot(0).has_value());
        remote.tickForTest();
        QCOMPARE(remote.lampTextForTest(), QStringLiteral("NO RF"));
        txSlice(*core)->setDspMode(DSPMode::LSB);
        QTRY_VERIFY(s.publisher()->sourceOpen(0));

        // The unkey: closed, and the copy goes.
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());
        QTRY_VERIFY(!s.publisher()->sourceOpen(0));
        QTRY_VERIFY(!s.window.stationSsbModMonitorSnapshot(0).has_value());
        QVERIFY(!s.stream(0)->contains(QLatin1String(SsbModMonitorRecord::kRecordId)));
        QCOMPARE(channel.tx.ssbModulationTapForTest(), nullptr);

        // The last watcher leaves.
        remote.hide();
        QTRY_COMPARE(s.stream(0)->subscriberCount(), 0);
        QTRY_VERIFY(!s.publisher()->publishing());
    }

    void thePublisherSendsFeedbackOnlyWithPureSignal()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        txSlice(*core)->setDspMode(DSPMode::USB);
        PureSignal* ps = core->installPureSignalForTest(nullptr);
        Session s(core.get(), m_securityDir.path(), this);
        QVERIFY(s.connect());
        SsbModMonitorApplet remote(&s.window);
        remote.setSource(SsbModMonitorApplet::Source::PaFeedback);
        remote.show();
        QTRY_COMPARE(s.stream(1)->subscriberCount(), 1);
        QCOMPARE(s.stream(0)->subscriberCount(), 0);

        QVERIFY(keyCore(*core));
        s.publisher()->tickForTest();
        QVERIFY(!s.publisher()->sourceOpen(1));
        QVERIFY(!core->ssbModFeedbackWanted());
        QVERIFY(!s.stream(1)->contains(QLatin1String(SsbModMonitorRecord::kRecordId)));

        ps->setPsEnabledForTest(true);
        QTRY_VERIFY(s.publisher()->sourceOpen(1));
        QVERIFY(core->ssbModFeedbackWanted());

        ps->setPsEnabledForTest(false);
        QTRY_VERIFY(!s.publisher()->sourceOpen(1));
        QVERIFY(!core->ssbModFeedbackWanted());
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());
    }

    void resetClearsTheCoresAnalyzer()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        txSlice(*core)->setDspMode(DSPMode::USB);
        Session s(core.get(), m_securityDir.path(), this);
        QVERIFY(s.connect());
        SsbModMonitorApplet remote(&s.window);
        remote.show();
        QTRY_COMPARE(s.stream(0)->subscriberCount(), 1);
        QVERIFY(keyCore(*core));
        QTRY_VERIFY(s.publisher()->sourceOpen(0));
        feedSteady(*core->ssbModulationAnalyzer(0), 0.3, 0.8f);
        QTRY_VERIFY([&]() {
            const auto snap = s.window.stationSsbModMonitorSnapshot(0);
            return snap && snap->peakHoldPct > 70.0;
        }());

        remote.resetPeaks();
        QTRY_VERIFY([&]() {
            const auto snap = s.window.stationSsbModMonitorSnapshot(0);
            return snap && !snap->signalPresent && snap->peakHoldPct == 0.0;
        }());

        // A source the Core does not have is refused.
        QVERIFY(!s.publisher()->resetSource(5));
        const StationClient::CommandOutcome refused = s.client->requestSsbModMonitorReset(5);
        QVERIFY(!refused.sent);
        QVERIFY(OperatorWording::isPlain(refused.reason));
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());
    }

    // ── The applet ────────────────────────────────────────────────────────

    void theLampDoesNotReadOnAfterTheSamplesStop()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        SsbModMonitorApplet applet(core.get());
        SsbModulationAnalyzer* tx = core->ssbModulationAnalyzer(0);
        tx->setSampleRate(kFs);
        feedSteady(*tx, 0.5, 0.5f);
        applet.tickForTest();
        QCOMPARE(applet.lampTextForTest(), QStringLiteral("ON"));
        QVERIFY(applet.crestTextForTest() != QStringLiteral("--"));
        QVERIFY(applet.scopePointsForTest() >= 2);

        tx->advanceClockForTest(std::chrono::milliseconds(
            static_cast<int>(SsbModulationAnalyzer::kInputStaleMs) + 1));
        applet.tickForTest();
        QCOMPARE(applet.lampTextForTest(), QStringLiteral("NO RF"));
        QCOMPARE(applet.crestTextForTest(), QStringLiteral("--"));
        QCOMPARE(applet.scopePointsForTest(), std::size_t(0));
    }

    void aLeftoverScopeIsNotASignal()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        SsbModMonitorApplet applet(core.get());
        SsbModulationAnalyzer::Snapshot s;
        s.scope.assign(4000, 50.0f);
        applet.applySnapshotForTest(s);
        QCOMPARE(applet.lampTextForTest(), QStringLiteral("NO RF"));
    }

    void crestShowsNoValueWhileTheOverGoesOn()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        SsbModMonitorApplet applet(core.get());
        SsbModulationAnalyzer::Snapshot s;
        s.signalPresent = true;
        s.peakHoldPct = 50.0;
        s.peakHoldDbfs = -6.0;
        applet.applySnapshotForTest(s);
        QCOMPARE(applet.lampTextForTest(), QStringLiteral("ON"));
        QCOMPARE(applet.crestTextForTest(), QStringLiteral("--"));
        s.crestDb = 3.01;
        applet.applySnapshotForTest(s);
        QCOMPARE(applet.crestTextForTest(), QStringLiteral("3.0"));
    }

    void paFeedbackIsDisabledWithItsReasonLocally()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        SsbModMonitorApplet applet(core.get());
        applet.show();
        QVERIFY(!applet.paFeedbackEnabledForTest());
        QCOMPARE(applet.paFeedbackToolTipForTest(), kPaFeedbackOffWords);
        QVERIFY(OperatorWording::isPlain(SsbModMonitorApplet::paFeedbackOffReason()));

        PureSignal* ps = core->installPureSignalForTest(nullptr);
        QVERIFY(!applet.paFeedbackEnabledForTest());
        ps->setPsEnabledForTest(true);
        QVERIFY(applet.paFeedbackEnabledForTest());
        QCOMPARE(applet.paFeedbackToolTipForTest(), kPaFeedbackOnWords);
        ps->setPsEnabledForTest(false);
        QVERIFY(!applet.paFeedbackEnabledForTest());
    }

    void paFeedbackIsDisabledWithItsReasonRemotely()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        Session s(core.get(), m_securityDir.path(), this);
        QVERIFY(s.connect());
        SsbModMonitorApplet applet(&s.window);
        QTRY_VERIFY(applet.monitorAvailableForTest());
        QVERIFY(!applet.paFeedbackEnabledForTest());
        QCOMPARE(applet.paFeedbackToolTipForTest(), kPaFeedbackOffWords);

        QJsonObject status = QJsonDocument::fromJson(
            core->pureSignalFacade()->statusJson().toUtf8()).object();
        status.insert(QStringLiteral("psEnabled"), true);
        QVERIFY(s.window.pureSignalFacade()->applyRemoteProperty(
            "statusJson", QString::fromUtf8(QJsonDocument(status).toJson(QJsonDocument::Compact))));
        QVERIFY(s.window.ssbModFeedbackInUse());
        QVERIFY(applet.paFeedbackEnabledForTest());
        QCOMPARE(applet.paFeedbackToolTipForTest(), kPaFeedbackOnWords);

        status.insert(QStringLiteral("psEnabled"), false);
        QVERIFY(s.window.pureSignalFacade()->applyRemoteProperty(
            "statusJson", QString::fromUtf8(QJsonDocument(status).toJson(QJsonDocument::Compact))));
        QVERIFY(!applet.paFeedbackEnabledForTest());
        QCOMPARE(applet.paFeedbackToolTipForTest(), kPaFeedbackOffWords);
    }

    void thePanelStartsOffInTheWindowsList()
    {
        // MainWindow registers the panel with this value.
        QVERIFY(!SsbModMonitorApplet::kShownByDefault);
    }

    void aPanelNotShownMeasuresNothingLocally()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        SsbModMonitorApplet applet(core.get());
        QVERIFY(!core->ssbModTxTapEnabled());
        QVERIFY(!core->ssbModFeedbackWanted());
        applet.show();
        QVERIFY(core->ssbModTxTapEnabled());
        QVERIFY(!core->ssbModFeedbackWanted());
        applet.setSource(SsbModMonitorApplet::Source::PaFeedback);
        QVERIFY(!core->ssbModTxTapEnabled());
        QVERIFY(core->ssbModFeedbackWanted());
        applet.hide();
        QVERIFY(!core->ssbModTxTapEnabled());
        QVERIFY(!core->ssbModFeedbackWanted());
    }

    void aPanelNotShownWatchesNothingOnTheCore()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        txSlice(*core)->setDspMode(DSPMode::USB);
        Session s(core.get(), m_securityDir.path(), this);
        QVERIFY(s.connect());
        SsbModMonitorApplet remote(&s.window);
        QTRY_VERIFY(remote.monitorAvailableForTest());
        // A full round trip on the link, so a subscribe would have landed.
        QVERIFY(keyCore(*core));
        QTRY_VERIFY(s.window.isTransmitting());
        for (int source = 0; source < 2; ++source) {
            QCOMPARE(s.stream(source)->subscriberCount(), 0);
        }
        QVERIFY(!s.publisher()->publishing());
        QVERIFY(!core->ssbModTxTapEnabled());
        QVERIFY(!core->ssbModFeedbackWanted());
        for (const QByteArray& wire : s.coreEnd->received()) {
            QVERIFY2(!wire.contains("txSsbModulation") && !wire.contains("txSsbModMonitor"),
                     wire.constData());
        }
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());
    }

    void belowTheCapabilityTheAppletIsDisabled()
    {
        RadioModel lonely{RadioModel::Role::Remote};
        SsbModMonitorApplet unlinked(&lonely);
        QVERIFY(!unlinked.monitorAvailableForTest());
        QCOMPARE(unlinked.unavailableReasonForTest(), SsbModMonitorApplet::notConnectedReason());
        QCOMPARE(unlinked.lampTextForTest(), QStringLiteral("--"));

        std::unique_ptr<RadioModel> core = makeCore();
        Session s(core.get(), m_securityDir.path(), this);
        QVERIFY(s.connect());
        SsbModMonitorApplet remote(&s.window);
        remote.show();
        QTRY_VERIFY(remote.monitorAvailableForTest());
        QTRY_COMPARE(s.stream(0)->subscriberCount(), 1);

        StationCapabilities caps = s.client->capabilities();
        caps.txSsbModMonitorVersion = 0;
        s.coreEnd->sendText(
            SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));
        QTRY_VERIFY(!remote.monitorAvailableForTest());
        QCOMPARE(remote.unavailableReasonForTest(), IStationLink::ssbModMonitorUnavailableReason());
        QVERIFY(OperatorWording::isPlain(remote.unavailableReasonForTest()));
        QCOMPARE(remote.lampTextForTest(), QStringLiteral("--"));
        remote.tickForTest();
        QCOMPARE(remote.lampTextForTest(), QStringLiteral("--"));
        QTRY_COMPARE(s.stream(0)->subscriberCount(), 0);
    }

private:
    QTemporaryDir m_securityDir;
};

QTEST_MAIN(TstSsbModMonitor)
#include "tst_ssb_mod_monitor.moc"
