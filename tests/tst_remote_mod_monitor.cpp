// no-port-check: NereusSDR-original test coverage.
// =================================================================
// tests/tst_remote_mod_monitor.cpp  (NereusSDR)
// =================================================================
//
// R-IOS-13 / R-R3-49 (iPhone plan Task 39 row A10; txModMonitorVersion 1):
// the AM Mod Monitor in a remote window.
//
//   - A remote window's applet, watching the Core's txAmModulation stream,
//     shows on a keyed AM carrier at 50 % and at 100 % modulation the same
//     peaks, holds, carrier and lamps as a local window's applet fed the
//     same I/Q.
//   - With no subscriber the Core runs no reads and feeds no analyzer; a
//     subscriber alone starts the reads but not the analyzer; keyed in AM
//     the TX tap is attached; a mode outside AM, SAM and DSB, the unkey or
//     the last unsubscribe detach it and the window's copy goes. The PA
//     feedback stream turns the feedback fork on the same way.
//   - RESET in a remote window clears the Core's analyzer; a source the
//     Core does not have is refused.
//   - A window's ModMon/FbStream reaches the Core's feedback analyzer.
//   - Below the capability (or with no Core) the applet, docked or popped
//     out, is disabled with the reason, never "NO CARRIER".
//   - The record's codec round-trips and bounds the scope.
//
// The in-process loopback only. No radio is keyed and nothing reaches a
// device: the Core's MoxController is keyed with its receive-only
// pre-check lifted, its model has no radio connection or transmit channel,
// and the Core's TX analyzer is fed the I/Q the TX tap would hand it.
//
//   cmake --build build --target tst_remote_mod_monitor
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_remote_mod_monitor$' \
//       --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (R-IOS-13, R-R3-49).
// =================================================================

#include <QtTest>

#include <QLabel>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "OperatorWording.h"
#include "core/AmModulationAnalyzer.h"
#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/session/ModMonitorPublisher.h"
#include "core/session/ModMonitorRecord.h"
#include "core/session/RecordStream.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/RemoteWindowHarness.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/ModMonitorApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <cmath>
#include <memory>
#include <vector>

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;
using NereusSDR::Test::RemoteWindowHarness;

namespace {

constexpr int kFs = 48000;

// AM as WDSP's ammod makes it: carrier c plus c * m * audio on the
// envelope, a 1 kHz tone, over whole blocks of 64 frames (the TX tap's).
std::vector<float> makeAm(double seconds, double modulation, double carrier = 0.4)
{
    const int n = static_cast<int>(seconds * kFs) / 64 * 64;
    std::vector<float> iq(static_cast<std::size_t>(n) * 2);
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / kFs;
        const double env = carrier * (1.0 + modulation * std::sin(2.0 * M_PI * 1000.0 * t));
        iq[2 * i + 0] = static_cast<float>(env * 0.8);
        iq[2 * i + 1] = static_cast<float>(env * 0.6);
    }
    return iq;
}

void feed(AmModulationAnalyzer& a, const std::vector<float>& iq)
{
    const int frames = static_cast<int>(iq.size() / 2);
    for (int off = 0; off + 64 <= frames; off += 64) {
        a.pushIq(iq.data() + 2 * off, 64);
    }
}

std::unique_ptr<RadioModel> makeCore()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::HermesLite);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:39");
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
        return server->recordStreamForTest(ModMonitorRecord::streamName(source));
    }
    ModMonitorPublisher* publisher() const { return server->modMonitorPublisherForTest(); }

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

// Keys the Core's MoxController with its receive-only pre-check lifted. No
// radio: the model has no connection and no transmit channel.
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

} // namespace

class TstRemoteModMonitor : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(m_securityDir.isValid());
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("remote-mod-monitor")));
    }

    void init() { QVERIFY(RemoteWindowHarness::clearIsolatedProfile()); }

    void cleanupTestCase() { QVERIFY(RemoteWindowHarness::removeIsolatedProfile()); }

    void theRecordRoundTrips()
    {
        AmModulationAnalyzer a;
        a.setSampleRate(kFs);
        feed(a, makeAm(0.6, 0.5));
        const AmModulationAnalyzer::Snapshot s = a.snapshot();
        QVERIFY(s.carrierPresent);
        QVERIFY(s.scope.size() > static_cast<std::size_t>(ModMonitorRecord::kWireScopePoints));
        const QJsonObject fields = ModMonitorRecord::toFields(s, 1234);
        QCOMPARE(fields.value(QStringLiteral("atMs")).toDouble(), 1234.0);
        const auto back = ModMonitorRecord::fromFields(fields);
        QVERIFY(back.has_value());
        QCOMPARE(back->posPeakPct, s.posPeakPct);
        QCOMPARE(back->negPeakPct, s.negPeakPct);
        QCOMPARE(back->posHoldPct, s.posHoldPct);
        QCOMPARE(back->negHoldPct, s.negHoldPct);
        QCOMPARE(back->carrierLevel, s.carrierLevel);
        QCOMPARE(back->carrierDbfs, s.carrierDbfs);
        QCOMPARE(back->carrierPresent, s.carrierPresent);
        QCOMPARE(back->carrierLow, s.carrierLow);
        QCOMPARE(back->carrierHigh, s.carrierHigh);
        // Bounded, and the largest excursion survives the reduction.
        QVERIFY(back->scope.size() <= static_cast<std::size_t>(ModMonitorRecord::kWireScopePoints));
        QVERIFY(!back->scope.empty());
        const auto maxOf = [](const std::vector<float>& v) {
            return *std::max_element(v.begin(), v.end());
        };
        QVERIFY(std::fabs(maxOf(back->scope) - maxOf(s.scope)) <= 0.05f);
        // A missing or mistyped field is no record.
        QJsonObject broken = fields;
        broken.insert(QStringLiteral("carrierPresent"), 1);
        QVERIFY(!ModMonitorRecord::fromFields(broken).has_value());
        broken = fields;
        broken.remove(QStringLiteral("posHoldPct"));
        QVERIFY(!ModMonitorRecord::fromFields(broken).has_value());
        broken = fields;
        broken.insert(QStringLiteral("scopePctTenths"),
                      QString::fromLatin1(QByteArray(2 * (ModMonitorRecord::kWireScopePoints + 1), '\0').toBase64()));
        QVERIFY(!ModMonitorRecord::fromFields(broken).has_value());
        QCOMPARE(ModMonitorRecord::sourceOfStream(QStringLiteral("txAmModulation")), 0);
        QCOMPARE(ModMonitorRecord::sourceOfStream(QStringLiteral("txAmModulationFeedback")), 1);
        QCOMPARE(ModMonitorRecord::sourceOfStream(QStringLiteral("spots")), -1);
    }

    void aKeyedCarrierShowsTheSameInBothWindows_data()
    {
        QTest::addColumn<double>("modulation");
        QTest::newRow("50 percent") << 0.5;
        QTest::newRow("100 percent") << 1.0;
    }

    void aKeyedCarrierShowsTheSameInBothWindows()
    {
        QFETCH(double, modulation);
        std::unique_ptr<RadioModel> core = makeCore();
        txSlice(*core)->setDspMode(DSPMode::AM);
        Session s(core.get(), m_securityDir.path(), this);
        QCOMPARE(s.server->txModMonitorVersion(), 1);
        QVERIFY(s.connect());
        QCOMPARE(s.client->capabilities().txModMonitorVersion, 1);

        ModMonitorApplet remote(&s.window);
        remote.show();
        QVERIFY(remote.monitorAvailableForTest());
        QTRY_COMPARE(s.stream(0)->subscriberCount(), 1);
        QCOMPARE(s.stream(1)->subscriberCount(), 0);

        QVERIFY(keyCore(*core));
        QTRY_VERIFY(s.publisher()->sourceOpen(0));
        QVERIFY(!s.publisher()->sourceOpen(1));
        QVERIFY(core->amModTxTapEnabled());

        // The same I/Q into the Core's TX analyzer (as its TX tap would
        // hand it) and into a local window's.
        RadioModel local;
        ModMonitorApplet localApplet(&local);
        const std::vector<float> iq = makeAm(0.6, modulation);
        feed(*core->amModulationAnalyzer(0), iq);
        feed(*local.amModulationAnalyzer(0), iq);
        localApplet.tickForTest();
        const ModMonitorApplet::DisplayForTest want = localApplet.displayForTest();
        QCOMPARE(want.lampText, QStringLiteral("CARRIER OK"));
        QVERIFY2(std::fabs(want.posBar - modulation * 100.0) < 2.0, qPrintable(want.posText));
        QVERIFY2(std::fabs(want.negBar - modulation * 100.0) < 2.0, qPrintable(want.negText));

        QTRY_VERIFY_WITH_TIMEOUT(
            [&]() {
                remote.tickForTest();
                return remote.displayForTest() == want;
            }(),
            3000);
        const ModMonitorApplet::DisplayForTest got = remote.displayForTest();
        QCOMPARE(got.posText, want.posText);
        QCOMPARE(got.negText, want.negText);
        QCOMPARE(got.asymText, want.asymText);
        QCOMPARE(got.carrierText, want.carrierText);
        QCOMPARE(got.lampText, want.lampText);
        QCOMPARE(got.posLit, want.posLit);
        QCOMPARE(got.negLit, want.negLit);
        // 100 %: the negative flasher (95 % by default) lights in both.
        QCOMPARE(got.negLit, modulation >= 1.0);
        QVERIFY(remote.scopePointsForTest() > 0);
        QVERIFY(remote.scopePointsForTest()
                <= static_cast<std::size_t>(ModMonitorRecord::kWireScopePoints));

        unkeyCore(*core);
        QTRY_VERIFY(!s.publisher()->sourceOpen(0));
        QTRY_VERIFY(!s.window.stationModMonitorSnapshot(0).has_value());
        QVERIFY(!core->amModTxTapEnabled());
    }

    void noWatcherCostsTheCoreNothing()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        txSlice(*core)->setDspMode(DSPMode::AM);
        Session s(core.get(), m_securityDir.path(), this);
        QVERIFY(s.connect());
        QVERIFY(s.publisher() != nullptr);

        // Nobody watching: no reads, no analyzer, keyed or not.
        QVERIFY(!s.publisher()->publishing());
        QVERIFY(!core->amModTxTapEnabled());
        QVERIFY(keyCore(*core));
        QTest::qWait(150);
        QVERIFY(!s.publisher()->publishing());
        QVERIFY(!s.publisher()->sourceOpen(0));
        QVERIFY(!core->amModTxTapEnabled());
        QVERIFY(!core->amModFeedbackWanted());
        unkeyCore(*core);
        QTRY_VERIFY(!core->isTransmitting());

        // Watching, unkeyed: the reads run, the analyzer is not fed.
        ModMonitorApplet remote(&s.window);
        remote.show();
        QTRY_VERIFY(s.publisher()->publishing());
        QTest::qWait(100);
        QVERIFY(!s.publisher()->sourceOpen(0));
        QVERIFY(!core->amModTxTapEnabled());

        // Keyed in AM: the tap is attached. In USB: detached, and the
        // window's copy goes.
        QVERIFY(keyCore(*core));
        QTRY_VERIFY(core->amModTxTapEnabled());
        feed(*core->amModulationAnalyzer(0), makeAm(0.3, 0.5));
        QTRY_VERIFY(s.window.stationModMonitorSnapshot(0).has_value());
        txSlice(*core)->setDspMode(DSPMode::USB);
        QTRY_VERIFY(!core->amModTxTapEnabled());
        QTRY_VERIFY(!s.window.stationModMonitorSnapshot(0).has_value());
        // SAM and DSB are measured too.
        txSlice(*core)->setDspMode(DSPMode::SAM);
        QTRY_VERIFY(core->amModTxTapEnabled());
        txSlice(*core)->setDspMode(DSPMode::DSB);
        QTest::qWait(100);
        QVERIFY(core->amModTxTapEnabled());

        // The PA feedback source turns the feedback fork on instead.
        remote.setSource(ModMonitorApplet::Source::PaFeedback);
        QTRY_COMPARE(s.stream(1)->subscriberCount(), 1);
        QTRY_COMPARE(s.stream(0)->subscriberCount(), 0);
        QTRY_VERIFY(core->amModFeedbackWanted());
        QTRY_VERIFY(!core->amModTxTapEnabled());

        // The last watcher leaves: nothing runs.
        remote.hide();
        QTRY_COMPARE(s.stream(1)->subscriberCount(), 0);
        QTRY_VERIFY(!s.publisher()->publishing());
        QVERIFY(!core->amModFeedbackWanted());
        QVERIFY(!core->amModTxTapEnabled());
        unkeyCore(*core);
    }

    void resetClearsTheCoresAnalyzer()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        txSlice(*core)->setDspMode(DSPMode::AM);
        Session s(core.get(), m_securityDir.path(), this);
        QVERIFY(s.connect());
        ModMonitorApplet remote(&s.window);
        remote.show();
        QTRY_COMPARE(s.stream(0)->subscriberCount(), 1);
        QVERIFY(keyCore(*core));
        QTRY_VERIFY(s.publisher()->sourceOpen(0));
        feed(*core->amModulationAnalyzer(0), makeAm(0.6, 0.8));
        QTRY_VERIFY([&]() {
            const auto snap = s.window.stationModMonitorSnapshot(0);
            return snap && snap->posHoldPct > 50.0;
        }());

        remote.resetPeaks();
        // The Core's analyzer starts again: no carrier until it hears one.
        QTRY_VERIFY([&]() {
            const auto snap = s.window.stationModMonitorSnapshot(0);
            return snap && !snap->carrierPresent && snap->posHoldPct == 0.0;
        }());

        // A source the Core does not have is refused.
        const StationClient::CommandOutcome refused = s.client->requestModMonitorReset(5);
        QVERIFY(!refused.sent);
        QVERIFY(OperatorWording::isPlain(refused.reason));
        s.windowEnd->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
            "txModMonitor.reset", 901,
            {MirrorUpdate{0, "source", MirrorWireKind::Int64, QVariant(qlonglong(5))}})));
        QTRY_VERIFY([&]() {
            for (const QByteArray& wire : s.windowEnd->received()) {
                SessionMessage m;
                if (SessionMessages::decode(wire, &m) && m.kind == SessionMessageKind::CommandResult
                    && m.commandId == 901) {
                    return !m.accepted && m.reason == QStringLiteral("The Core could not read this request.");
                }
            }
            return false;
        }());
        s.windowEnd->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
            "txModMonitor.reset", 902,
            {MirrorUpdate{0, "source", MirrorWireKind::Int64,
                          QVariant(qlonglong(4294967296LL))}})));
        QTRY_VERIFY([&]() {
            for (const QByteArray& wire : s.windowEnd->received()) {
                SessionMessage m;
                if (SessionMessages::decode(wire, &m) && m.kind == SessionMessageKind::CommandResult
                    && m.commandId == 902) {
                    return !m.accepted;
                }
            }
            return false;
        }());
        unkeyCore(*core);
    }

    void aWindowsFeedbackReceiverReachesTheCore()
    {
        std::unique_ptr<RadioModel> core = makeCore();
        Session s(core.get(), m_securityDir.path(), this);
        QVERIFY(s.connect());
        QCOMPARE(core->amModFeedbackStream(), 1);
        s.windowEnd->sendText(SessionMessages::encode(SessionMessages::settingsWrite(
            QStringLiteral("ModMon/FbStream"), QStringLiteral("3"), QStringLiteral("test"))));
        QTRY_COMPARE(core->amModFeedbackStream(), 3);
        s.windowEnd->sendText(SessionMessages::encode(
            SessionMessages::settingsRemove(QStringLiteral("ModMon/FbStream"))));
        QTRY_COMPARE(core->amModFeedbackStream(), 1);
    }

    void belowTheCapabilityTheAppletIsDisabled()
    {
        // No Core yet.
        RadioModel lonely{RadioModel::Role::Remote};
        ModMonitorApplet unlinked(&lonely);
        QVERIFY(!unlinked.monitorAvailableForTest());
        QCOMPARE(unlinked.unavailableReasonForTest(), ModMonitorApplet::notConnectedReason());
        QVERIFY(unlinked.carrierLampTextForTest() != QStringLiteral("NO CARRIER"));

        std::unique_ptr<RadioModel> core = makeCore();
        Session s(core.get(), m_securityDir.path(), this);
        QVERIFY(s.connect());
        AppletPanelWidget panel;
        auto* remote = new ModMonitorApplet(&s.window);
        panel.insertApplet(0, remote);
        panel.show();
        QTRY_VERIFY(remote->monitorAvailableForTest());

        // The Core says it does not send them.
        StationCapabilities caps = s.client->capabilities();
        caps.txModMonitorVersion = 0;
        s.coreEnd->sendText(
            SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));
        QTRY_VERIFY(!remote->monitorAvailableForTest());
        const QString reason = QStringLiteral(
            "This Core does not send the modulation monitor. Updating the Core may help.");
        QCOMPARE(remote->unavailableReasonForTest(), reason);
        QVERIFY(OperatorWording::isPlain(reason));
        QCOMPARE(remote->carrierLampTextForTest(), QStringLiteral("--"));
        remote->tickForTest();
        QCOMPARE(remote->carrierLampTextForTest(), QStringLiteral("--"));
        QCOMPARE(remote->displayForTest().posText, QStringLiteral("--"));
        // Nothing is watched on a Core that does not send it.
        QTRY_COMPARE(s.stream(0)->subscriberCount(), 0);

        // Popped out, the same.
        panel.floatApplet(remote);
        QVERIFY(qobject_cast<AppletFloatingWindow*>(remote->window()) != nullptr);
        QVERIFY(!remote->monitorAvailableForTest());
        const QList<QLabel*> labels = remote->findChildren<QLabel*>();
        bool reasonShown = false;
        for (const QLabel* label : labels) {
            reasonShown = reasonShown || (label->text() == reason && label->isVisible());
        }
        QVERIFY(reasonShown);
        panel.dockApplet(remote);
        QCoreApplication::processEvents();
    }

private:
    QTemporaryDir m_securityDir;
};

QTEST_MAIN(TstRemoteModMonitor)
#include "tst_remote_mod_monitor.moc"
