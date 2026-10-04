// no-port-check: NereusSDR-original PS3 action/session regression tests.
// Modification history (NereusSDR):
//   2026-10-04 J.J. Boyd (KG4VCF): first media admission/rejoin Auto
//              convergence regressions. AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QPushButton>
#include <QLabel>
#include "gui/applets/PureSignalApplet.h"
#include <QTableWidget>
#include <limits>
#include <algorithm>
#include "core/AppSettings.h"
#include "core/PureSignal.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/dsp/DspAssetService.h"
#include "core/session/PureSignalSessionFacade.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/DisplayBudget.h"
#include "core/settings/SettingsProxy.h"
#include "models/RadioModel.h"
#include "models/PureSignalSettings.h"
#include "gui/PsaIndicatorWidget.h"
#include "gui/PsForm.h"
#include "gui/DspAssetDialog.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;

namespace {

struct AdmissionClient {
    RadioModel radio{RadioModel::Role::Remote};
    SettingsProxy proxy;
    StationClient client{&radio, &proxy};

    void join(StationServer& server, const QString& token, bool abortSnapshot = false)
    {
        auto coreEnd = std::make_unique<Test::LoopbackTransport>("station");
        auto guiEnd = std::make_unique<Test::LoopbackTransport>("gui");
        coreEnd->linkTo(guiEnd.get());
        if (abortSnapshot) {
            QObject::connect(coreEnd.get(), &Test::LoopbackTransport::outboundText,
                             coreEnd.get(), [end = coreEnd.get()](const QByteArray& wire) {
                if (QJsonDocument::fromJson(wire).object().value("type") == "capabilities") {
                    end->closeLink(QStringLiteral("snapshot delivery lost"));
                }
            });
        }
        client.startSession(guiEnd.release(), token);
        server.acceptTransport(coreEnd.release());
    }
};

#ifdef HAVE_WDSP
struct AdmissionCore {
    // The station owns the coordinator, and must retire it before WDSP closes.
    WdspEngine engine;
    RadioModel radio;
    PureSignal* coordinator{nullptr};
    bool ready{true};
    bool permitted{true};

    bool initialize(const QString& directory)
    {
        engine.setSynchronousInitForTest(true);
        if (!engine.initialize(directory + QLatin1Char('/'))) { return false; }
        TxChannel* tx = engine.createTxChannel(WdspEngine::kTxChannelId);
        if (!tx) { return false; }
        coordinator = radio.installPureSignalForTest(tx);
        coordinator->setTimersEnabled(false);
        coordinator->setOperationalReadinessPredicate([this] { return ready; });
        coordinator->setOperationalPermissionPredicate([this] { return permitted; });
        coordinator->initializeAutoCalPreference(true);
        radio.pureSignalSettings()->setRunCalibrationProcessing(true);
        return coordinator->applyAcceptedSettingsToEngine();
    }

    void tick()
    {
        int resetInfo[16] = {};
        // No IQ is supplied: the native engine stays at LRESET off the air.
        for (int count = 0; count < 8; ++count) {
            coordinator->processNewInfo(resetInfo);
        }
    }
};
#endif

} // namespace

class TestPs3Session : public QObject {
    Q_OBJECT
private slots:
    void init() { AppSettings::instance().clear(); }

    void localQueuedIdentityCannotBeReusedAndResetRetiresQueuedWork()
    {
        RadioModel radio;
        PureSignalSessionFacade* facade = radio.pureSignalFacade();
        QSignalSpy results(facade, &PureSignalSessionFacade::actionResult);
        const quint32 first = facade->requestAction(Ps3Action::OffReset);
        const quint32 second = facade->requestAction(Ps3Action::OffReset);
        QVERIFY(first != 0 && second != 0 && first != second);
        QCOMPARE(facade->executeAction(Ps3Action::OffReset, {}, first).phase,
                 Ps3ActionPhase::Failed);
        QTRY_COMPARE(results.size(), 2);
        QCOMPARE(results[0][0].toUInt(), first);
        QCOMPARE(qvariant_cast<Ps3ActionPhase>(results[0][1]), Ps3ActionPhase::Completed);
        facade->requestAction(Ps3Action::Single);
        facade->resetSession();
        QCoreApplication::processEvents();
        QCOMPARE(results.size(), 2);
    }

    void remoteCapabilityNeverOverridesRuntimeReadinessOrR4Gate()
    {
        RadioModel radio(RadioModel::Role::Remote);
        PureSignalSessionFacade* facade = radio.pureSignalFacade();
        facade->setRemoteCapabilities(true, true);
        QVERIFY(!facade->available());
        QVERIFY(!facade->canActuate());
        QVERIFY(facade->applyRemoteProperty("available", true));
        QVERIFY(facade->applyRemoteProperty("canActuate", true));
        QVERIFY(facade->canActuate());
        facade->setRemoteCapabilities(true, false);
        int requests = 0;
        facade->setRemoteRequestHandler([&requests](Ps3Action, const QVariantMap&) {
            return static_cast<quint32>(++requests);
        });
        QCOMPARE(facade->requestAction(Ps3Action::Single), 0u);
        QCOMPARE(facade->requestAction(Ps3Action::StartAutomatic), 0u);
        QCOMPARE(facade->requestAction(Ps3Action::SetTwoTone, {{"enabled", true}}), 0u);
        QCOMPARE(requests, 0);
        const quint32 id = facade->requestAction(Ps3Action::OffReset);
        QVERIFY(id != 0);
        QSignalSpy results(facade, &PureSignalSessionFacade::actionResult);
        facade->receiveRemoteActionResult(id, "ps3.single", Ps3ActionPhase::Completed, {}, {});
        QCOMPARE(results.size(), 0);
        facade->receiveRemoteActionResult(id, "ps3.off", Ps3ActionPhase::Pending, {}, {});
        QCOMPARE(results.size(), 1);
        facade->receiveRemoteActionResult(id, "ps3.off", Ps3ActionPhase::Completed, {}, {});
        QCOMPARE(results.size(), 2);
        facade->receiveRemoteActionResult(id, "ps3.off", Ps3ActionPhase::Completed, {}, {});
        QCOMPARE(results.size(), 2);
        const quint32 retired = facade->requestAction(Ps3Action::OffReset);
        facade->resetSession();
        facade->receiveRemoteActionResult(retired, "ps3.off", Ps3ActionPhase::Failed, "old", {});
        QCOMPARE(results.size(), 2);
        QVERIFY(!facade->available());
        QVERIFY(!facade->canActuate());
        QVERIFY(facade->lastActionError().isEmpty());
    }

    void explicitAutomaticResumesSavedIntentAndPendingStartsAreSerialized()
    {
#ifndef HAVE_WDSP
        QSKIP("requires WDSP");
#else
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        WdspEngine engine;
        engine.setSynchronousInitForTest(true);
        QVERIFY(engine.initialize(directory.path() + QLatin1Char('/')));
        TxChannel* tx = engine.createTxChannel(WdspEngine::kTxChannelId);
        QVERIFY(tx);
        PureSignalSettings settings;
        settings.initializeAutoCalPreference(true);
        PureSignal coordinator(&engine, tx, nullptr, nullptr, nullptr, nullptr);
        coordinator.setTimersEnabled(false);
        coordinator.setSettings(&settings);
        PureSignalSessionFacade facade(nullptr, &coordinator);
        QSignalSpy results(&facade, &PureSignalSessionFacade::actionResult);
        QCOMPARE(facade.executeAction(Ps3Action::StartAutomatic, {}, 1).phase,
                 Ps3ActionPhase::Pending);
        QTRY_VERIFY([&] { coordinator.pollTimerTick(); return coordinator.isPsEnabled(); }());
        QTRY_COMPARE(results.size(), 1);
        QCOMPARE(qvariant_cast<Ps3ActionPhase>(results.last()[1]), Ps3ActionPhase::Completed);

        facade.resetSession();
        QVERIFY(settings.autoCalEnabled());
        QTRY_VERIFY([&] { coordinator.pollTimerTick(); return !coordinator.isPsEnabled(); }());
        QCOMPARE(facade.executeAction(Ps3Action::StartAutomatic, {}, 2).phase,
                 Ps3ActionPhase::Pending);
        QTRY_VERIFY([&] { coordinator.pollTimerTick(); return coordinator.isPsEnabled(); }());
        QTRY_COMPARE(results.size(), 2);
        QCOMPARE(facade.executeAction(Ps3Action::Single, {}, 3).phase, Ps3ActionPhase::Pending);
        const Ps3ActionResult automatic = facade.executeAction(Ps3Action::StartAutomatic, {}, 4);
        QCOMPARE(automatic.phase, Ps3ActionPhase::Pending);
        QVERIFY(settings.autoCalEnabled());
        QVERIFY(std::any_of(results.begin(), results.end(), [](const QList<QVariant>& result) {
            return result[0].toUInt() == 3
                && qvariant_cast<Ps3ActionPhase>(result[1]) == Ps3ActionPhase::Failed;
        }));
        // Switching immediately after Single must not leave its transient
        // request to switch the command state back into Single later.
        int correctedInfo[16] = {};
        correctedInfo[14] = 1;
        for (int tick = 0; tick < 8; ++tick) {
            coordinator.processNewInfo(correctedInfo);
        }
        QVERIFY(coordinator.isPsEnabled());
        QTRY_VERIFY([&] {
            coordinator.pollTimerTick();
            return std::any_of(results.begin(), results.end(), [](const QList<QVariant>& result) {
                return result[0].toUInt() == 4
                    && qvariant_cast<Ps3ActionPhase>(result[1]) == Ps3ActionPhase::Completed;
            });
        }());
        QCOMPARE(facade.executeAction(Ps3Action::Single, {}, 5).phase, Ps3ActionPhase::Pending);
        QCOMPARE(facade.executeAction(Ps3Action::OffReset, {}, 6).phase, Ps3ActionPhase::Pending);
        QTRY_VERIFY([&] { coordinator.pollTimerTick(); return !coordinator.isPsEnabled(); }());
        // No TXA or feedback sample was sent, and the explicit Off retired Single.
        QVERIFY(std::any_of(results.begin(), results.end(), [](const QList<QVariant>& result) {
            return result[0].toUInt() == 5
                && qvariant_cast<Ps3ActionPhase>(result[1]) == Ps3ActionPhase::Failed;
        }));
#endif
    }

    void automaticReplacesSingleAtEveryCommandTransition()
    {
#ifndef HAVE_WDSP
        QSKIP("requires WDSP");
#else
        for (int singleTicks = 0; singleTicks < 4; ++singleTicks) {
            QTemporaryDir directory;
            WdspEngine engine;
            engine.setSynchronousInitForTest(true);
            QVERIFY(engine.initialize(directory.path() + QLatin1Char('/')));
            TxChannel* tx = engine.createTxChannel(WdspEngine::kTxChannelId);
            QVERIFY(tx);
            PureSignalSettings settings;
            PureSignal coordinator(&engine, tx, nullptr, nullptr, nullptr, nullptr);
            coordinator.setTimersEnabled(false);
            coordinator.setSettings(&settings);
            QVERIFY(coordinator.applyAcceptedSettingsToEngine());
            PureSignalSessionFacade facade(nullptr, &coordinator);
            QCOMPARE(facade.executeAction(Ps3Action::Single, {}, 1).phase,
                     Ps3ActionPhase::Pending);
            int info[16] = {};
            for (int tick = 0; tick < singleTicks; ++tick) {
                coordinator.processNewInfo(info);
            }
            QCOMPARE(facade.executeAction(Ps3Action::StartAutomatic, {}, 2).phase,
                     Ps3ActionPhase::Pending);
            info[14] = 1;
            for (int tick = 0; tick < 8; ++tick) {
                coordinator.processNewInfo(info);
            }
            QVERIFY2(coordinator.isPsEnabled(), qPrintable(
                QStringLiteral("Auto reverted to Single after %1 tick(s)").arg(singleTicks)));
        }
#endif
    }

    void firstMediaAdmissionResumesSavedAutomaticIntent_data()
    {
        QTest::addColumn<bool>("rejoin");
        QTest::newRow("first-join") << false;
        QTest::newRow("after-last-client-departure") << true;
    }

    void firstMediaAdmissionResumesSavedAutomaticIntent()
    {
#ifndef HAVE_WDSP
        QSKIP("requires WDSP");
#else
        QFETCH(bool, rejoin);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AdmissionCore core;
        QVERIFY(core.initialize(directory.path()));
        QVERIFY(core.coordinator->resumeAutomaticCalibrationPreference());
        core.tick();
        QVERIFY(core.coordinator->isPsEnabled());
        StationServer server(&core.radio, AppSettings::instance(),
                             Test::seedUpgradedCoreToken(directory.path()));
        server.setMediaEnabled(true);
        AdmissionClient first;
        if (rejoin) {
            first.join(server, server.token());
            QTRY_VERIFY(first.client.isHandshakeComplete());
            // Establish a running prior session independently of the bug.
            QVERIFY(core.coordinator->resumeAutomaticCalibrationPreference());
            core.tick();
            QVERIFY(core.coordinator->isPsEnabled());
            first.client.disconnectFromStation(QStringLiteral("test departure"));
            QTRY_VERIFY(server.mediaSessionEpochs().isEmpty());
            core.tick();
            QVERIFY(!core.coordinator->isPsEnabled());
            QVERIFY(core.radio.pureSignalSettings()->autoCalEnabled());
        }
        PureSignalSessionFacade* facade = core.radio.pureSignalFacade();
        const quint64 retiredGeneration = facade->displayGeneration();
        facade->setRemoteAmpViewSubscribed(true);
        AdmissionClient admitted;
        admitted.join(server, server.token());
        QTRY_VERIFY(admitted.client.isHandshakeComplete());
        QCOMPARE(server.mediaSessionEpochs().size(), 1);
        QVERIFY(facade->displayGeneration() > retiredGeneration);
        core.tick();
        QVERIFY(core.radio.pureSignalSettings()->autoCalEnabled());
        QVERIFY2(core.coordinator->isPsEnabled(),
                 "successful first media admission retained Auto but did not rearm it");
        QVERIFY(!core.radio.transmitModel().isMox());
        QVERIFY(!core.radio.twoToneController()->isActive());
        QVERIFY(!admitted.radio.pureSignal());
        QVERIFY(!facade->remoteAmpViewSubscribed());
        QVERIFY(!facade->displaySnapshot());

        AdmissionClient second;
        const quint64 runningGeneration = facade->displayGeneration();
        second.join(server, server.token());
        QTRY_VERIFY(second.client.isHandshakeComplete());
        QCOMPARE(server.mediaSessionEpochs().size(), 2);
        QCOMPARE(facade->displayGeneration(), runningGeneration);
        core.tick();
        QVERIFY(core.coordinator->isPsEnabled());
#endif
    }

    void firstMediaAdmissionHonorsAutomaticEligibility_data()
    {
        QTest::addColumn<QString>("gate");
        QTest::newRow("Auto-off") << QStringLiteral("auto");
        QTest::newRow("RunCal-off") << QStringLiteral("run");
        QTest::newRow("radio-unready") << QStringLiteral("ready");
        QTest::newRow("operation-not-permitted") << QStringLiteral("permission");
        QTest::newRow("parameters-not-applied") << QStringLiteral("parameters");
    }

    void firstMediaAdmissionHonorsAutomaticEligibility()
    {
#ifndef HAVE_WDSP
        QSKIP("requires WDSP");
#else
        QFETCH(QString, gate);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AdmissionCore core;
        QVERIFY(core.initialize(directory.path()));
        if (gate == "auto") { core.coordinator->initializeAutoCalPreference(false); }
        if (gate == "run") { core.radio.pureSignalSettings()->setRunCalibrationProcessing(false); }
        if (gate == "ready") { core.ready = false; }
        if (gate == "permission") { core.permitted = false; }
        PureSignalSettings unappliedSettings;
        if (gate == "parameters") {
            unappliedSettings.initializeAutoCalPreference(true);
            unappliedSettings.setRunCalibrationProcessing(true);
            core.coordinator->setSettings(&unappliedSettings);
        }
        StationServer server(&core.radio, AppSettings::instance(),
                             Test::seedUpgradedCoreToken(directory.path()));
        server.setMediaEnabled(true);
        AdmissionClient admitted;
        admitted.join(server, server.token());
        QTRY_VERIFY(admitted.client.isHandshakeComplete());
        QCOMPARE(server.mediaSessionEpochs().size(), 1);
        core.tick();
        QVERIFY(!core.coordinator->isPsEnabled());
        QVERIFY(!core.radio.transmitModel().isMox());
        QVERIFY(!core.radio.twoToneController()->isActive());
        // Return the station-owned settings before the temporary object leaves.
        core.coordinator->setSettings(core.radio.pureSignalSettings());
#endif
    }

    void failedMediaAuthenticationDoesNotResumeAutomatic_data()
    {
        QTest::addColumn<bool>("abortSnapshot");
        QTest::newRow("invalid-token") << false;
        QTest::newRow("lost-during-capabilities-snapshot") << true;
    }

    void failedMediaAuthenticationDoesNotResumeAutomatic()
    {
#ifndef HAVE_WDSP
        QSKIP("requires WDSP");
#else
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AdmissionCore core;
        QVERIFY(core.initialize(directory.path()));
        core.coordinator->retireSessionOperations();
        core.tick();
        QVERIFY(!core.coordinator->isPsEnabled());
        QVERIFY(core.radio.pureSignalSettings()->autoCalEnabled());
        StationServer server(&core.radio, AppSettings::instance(),
                             Test::seedUpgradedCoreToken(directory.path()));
        server.setMediaEnabled(true);
        const quint64 generation = core.radio.pureSignalFacade()->displayGeneration();
        QFETCH(bool, abortSnapshot);
        AdmissionClient rejected;
        QSignalSpy ended(&rejected.client, &StationClient::sessionEnded);
        rejected.join(server, abortSnapshot ? server.token() : QStringLiteral("invalid-test-token"),
                      abortSnapshot);
        QTRY_VERIFY(!ended.isEmpty());
        QVERIFY(!rejected.client.isHandshakeComplete());
        QVERIFY(server.mediaSessionEpochs().isEmpty());
        if (abortSnapshot) {
            QVERIFY(core.radio.pureSignalFacade()->displayGeneration() > generation);
        } else {
            QCOMPARE(core.radio.pureSignalFacade()->displayGeneration(), generation);
        }
        core.tick();
        QVERIFY(!core.coordinator->isPsEnabled());
        QVERIFY(!core.radio.transmitModel().isMox());
        QVERIFY(!core.radio.twoToneController()->isActive());
#endif
    }

    void telemetryAdmissionCallbackCanDestroyStationBeforeAutomaticResume()
    {
#ifndef HAVE_WDSP
        QSKIP("requires WDSP");
#else
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AdmissionCore core;
        QVERIFY(core.initialize(directory.path()));
        QVERIFY(core.coordinator->resumeAutomaticCalibrationPreference());
        core.tick();
        QVERIFY(core.coordinator->isPsEnabled());
        auto server = std::make_unique<StationServer>(&core.radio, AppSettings::instance(),
            Test::seedUpgradedCoreToken(directory.path()));
        server->setMediaEnabled(true);
        server->setTelemetryEnabled(true);
        const QPointer<StationServer> lifetime(server.get());
        bool callbackReturned = false;
        QObject::connect(server.get(), &StationServer::telemetrySessionStarted,
                         &core.radio, [&server, &callbackReturned](quint64) {
            // A direct subscriber may end the Core during admission.
            server.reset();
            callbackReturned = true;
        });
        AdmissionClient admitted;
        admitted.join(*server, server->token());
        QTRY_VERIFY(callbackReturned);
        QVERIFY(lifetime.isNull());
        QVERIFY(!server);
        core.tick();
        QVERIFY(!core.coordinator->isPsEnabled());
        QVERIFY(core.radio.pureSignalSettings()->autoCalEnabled());
        QVERIFY(!core.radio.transmitModel().isMox());
        QVERIFY(!core.radio.twoToneController()->isActive());
#endif
    }

    void displayRejectsRetiredMalformedAndHiddenFrames()
    {
        RadioModel radio(RadioModel::Role::Remote);
        PureSignalSessionFacade* facade = radio.pureSignalFacade();
        Ps3Snapshot frame;
        frame.channelId = 3;
        frame.sessionGeneration = facade->displayGeneration();
        frame.sequence = 1;
        frame.sampleCount = 1;
        frame.x = {0.5}; frame.ym = {0.4}; frame.yc = {1.0}; frame.ys = {0.0};
        QSignalSpy displayed(facade, &PureSignalSessionFacade::displaySnapshotReady);
        facade->receiveDisplaySnapshot(frame);
        QCOMPARE(displayed.size(), 0);
        facade->setAmpViewSubscribed(true);
        facade->receiveDisplaySnapshot(frame);
        QCOMPARE(displayed.size(), 1);
        facade->receiveDisplaySnapshot(frame);
        QCOMPARE(displayed.size(), 1);
        ++frame.sequence;
        frame.x[0] = std::numeric_limits<double>::quiet_NaN();
        facade->receiveDisplaySnapshot(frame);
        QCOMPARE(displayed.size(), 1);
        frame.x[0] = 0.5;
        facade->resetSession();
        facade->receiveDisplaySnapshot(frame);
        QCOMPARE(displayed.size(), 1);
        QVERIFY(!facade->displaySnapshot());
    }

    void remoteDisplaySubscriptionPublishesOnlyAcceptedStateChanges()
    {
        RadioModel radio;
        PureSignalSessionFacade* facade = radio.pureSignalFacade();
        QSignalSpy changes(facade,
                           &PureSignalSessionFacade::remoteAmpViewSubscriptionChanged);

        facade->setRemoteAmpViewSubscribed(true);
        QCOMPARE(changes.size(), 1);
        QCOMPARE(changes.constLast().constFirst().toBool(), true);
        facade->setRemoteAmpViewSubscribed(true);
        QCOMPARE(changes.size(), 1);

        facade->resetSession();
        QCOMPARE(changes.size(), 2);
        QCOMPARE(changes.constLast().constFirst().toBool(), false);
        facade->resetSession();
        QCOMPARE(changes.size(), 2);
    }

    void ps3BudgetAdmissionPrecedesFacadeMutation()
    {
#ifndef HAVE_WDSP
        QSKIP("requires PS3 display capability");
#else
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        RadioModel station;
        StationServer server(&station, AppSettings::instance(), NereusSDR::Test::seedUpgradedCoreToken(directory.path()));
        server.setMediaEnabled(true);
        DaemonMediaController controller(&server, &station);
        QVERIFY(server.setDisplayBudgetLimits({1, 1, 1}));

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        auto* coreEnd = new Test::LoopbackTransport("station");
        auto* guiEnd = new Test::LoopbackTransport("gui");
        coreEnd->linkTo(guiEnd);
        client.startSession(guiEnd, server.token());
        server.acceptTransport(coreEnd);
        QTRY_VERIFY(client.isHandshakeComplete());
        QTRY_VERIFY(client.remoteDisplayBudgetLimits().has_value());

        QSignalSpy replies(&client, &StationClient::commandResponse);
        const quint32 refusedId = client.requestPs3DisplaySubscription(true);
        QVERIFY(refusedId != 0);
        QTRY_VERIFY(!replies.isEmpty());
        SessionMessage reply = qvariant_cast<SessionMessage>(replies.constLast().constFirst());
        QCOMPARE(reply.commandId, refusedId);
        QVERIFY(!reply.accepted);
        QVERIFY(!station.pureSignalFacade()->remoteAmpViewSubscribed());

        const DisplayBudgetCharge charge = ps3DisplayCharge();
        QVERIFY(server.setDisplayBudgetLimits({charge.applicationBytesPerSecond, 1, 2}));
        QTRY_COMPARE(client.remoteDisplayBudgetLimits()->generation, quint32{2});
        const quint32 acceptedId = client.requestPs3DisplaySubscription(true);
        QVERIFY(acceptedId != 0);
        QTRY_VERIFY(replies.size() >= 2);
        reply = qvariant_cast<SessionMessage>(replies.constLast().constFirst());
        QCOMPARE(reply.commandId, acceptedId);
        QVERIFY(reply.accepted);
        QVERIFY(station.pureSignalFacade()->remoteAmpViewSubscribed());
        QTRY_VERIFY(client.remotePs3DisplaySubscribed());
#endif
    }

    void stationRejectsTwoToneEvenIfClientBypassesDisabledControls()
    {
        QTemporaryDir security;
        QVERIFY(security.isValid());
        RadioModel station;
        PureSignal* coordinator = station.installPureSignalForTest(nullptr);
        QSignalSpy started(coordinator, &PureSignal::calibrationStarted);
        StationServer server(&station, AppSettings::instance(), NereusSDR::Test::seedUpgradedCoreToken(security.path()));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        auto* coreEnd = new Test::LoopbackTransport("station");
        auto* guiEnd = new Test::LoopbackTransport("gui");
        coreEnd->linkTo(guiEnd);
        client.startSession(guiEnd, server.token());
        server.acceptTransport(coreEnd);
        QTRY_VERIFY(client.isHandshakeComplete());
        QVERIFY(!remote.pureSignalFacade()->canActuate());
        QSignalSpy replies(&client, &StationClient::commandResponse);
        // R-R3-49 (parity Task 7): arming (Single Cal, Automatic, Apply
        // current correction, Restore) is taken off the air from a window
        // offered transmitSettingsVersion 7 (tst_remote_puresignal_arming).
        // The two-tone test keys the radio: it stays refused.
        QVERIFY(client.invokeCommand("ps3.twoTone",
            {{0, "enabled", MirrorWireKind::Bool, true}}) != 0);
        QTRY_COMPARE(replies.size(), 1);
        const SessionMessage reply = qvariant_cast<SessionMessage>(replies.last()[0]);
        QVERIFY(!reply.accepted);
        // iPhone app plan Task 77 (ruling 8.3): the two-tone test is a key,
        // refused by this receive-only Core's transmit gate.
        QCOMPARE(reply.reason, QStringLiteral("This Core is set to receive only."));
        QVERIFY(!station.twoToneController()->isActive());
        QCOMPARE(started.size(), 0);
        QVERIFY(!coordinator->isPsEnabled());
        remote.pureSignalSettings()->setMoxDelaySeconds(0.4);
        QTRY_COMPARE(station.pureSignalSettings()->moxDelaySeconds(), 0.4);
        QCOMPARE(started.size(), 0);

        // The radio may arrive after the control session. Capability changes
        // must update the facade without another snapshot or action replay.
        StationCapabilities capabilities = server.buildCapabilities();
        capabilities.psAlgorithmVersion = 0;
        coreEnd->sendText(SessionMessages::encode(
            SessionMessages::capabilities(capabilities.toUpdates())));
        QTRY_VERIFY(!remote.pureSignalFacade()->available());
        capabilities.psAlgorithmVersion = 3;
        coreEnd->sendText(SessionMessages::encode(
            SessionMessages::capabilities(capabilities.toUpdates())));
        QTRY_VERIFY(remote.pureSignalFacade()->available());
        QVERIFY(!remote.pureSignalFacade()->canActuate());
        QCOMPARE(started.size(), 0);
    }

    void remoteCorrectionManagerRestoresOnlyOnACoreThatOffersArming()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        RadioModel station;
        RadioInfo info;
        info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:91");
        station.setLastRadioInfoForTest(info);
        station.setConnectionStateForTest(ConnectionState::Connected);
        station.dspAssets()->setRadioIdentity(info.macAddress);
        station.installPureSignalForTest(nullptr);
        StationServer server(&station, AppSettings::instance(), NereusSDR::Test::seedUpgradedCoreToken(directory.path()));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        auto* coreEnd = new Test::LoopbackTransport("station");
        auto* guiEnd = new Test::LoopbackTransport("gui");
        coreEnd->linkTo(guiEnd);
        client.startSession(guiEnd, server.token());
        server.acceptTransport(coreEnd);
        QTRY_VERIFY(client.isHandshakeComplete());
        QTRY_VERIFY(remote.pureSignalFacade()->available());
        QVERIFY(!remote.pureSignalFacade()->canActuate());
        QCOMPARE(remote.currentRadioMac(), info.macAddress);

        PsForm form(&remote, nullptr);
        auto* openManager = form.findChild<QPushButton*>(QStringLiteral("btnPSRestore"));
        QVERIFY(openManager);
        QTRY_VERIFY(openManager->isEnabled());
        openManager->click();
        auto* manager = form.findChild<DspAssetDialog*>();
        QVERIFY(manager);
        auto* importButton = manager->findChild<QPushButton*>(QStringLiteral("dspAssetImportButton"));
        auto* restore = manager->findChild<QPushButton*>(QStringLiteral("restoreCorrectionAssetButton"));
        auto* table = manager->findChild<QTableWidget*>(QStringLiteral("dspAssetTable"));
        QVERIFY(importButton && restore && table);
        QTRY_VERIFY(importButton->isEnabled());
        const QString fixturePath = QFINDTESTDATA("fixtures/dsp/ps3-v2-source-writer.txt");
        QFile fixture(fixturePath);
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        const QByteArray expected = fixture.readAll();
        QVERIFY(manager->importFile(fixturePath, QStringLiteral("Remote correction")));
        QTRY_COMPARE(table->rowCount(), 1);
        const QString id = manager->selectedAssetId();
        QVERIFY(!id.isEmpty());
        const QString exported = directory.filePath(QStringLiteral("exported.ps3"));
        auto* exportButton = manager->findChild<QPushButton*>(QStringLiteral("dspAssetExportButton"));
        QVERIFY(exportButton);
        // The row can arrive before the import's final refresh settles.
        // Follow the same readiness gate as the operator-facing action.
        QTRY_VERIFY(exportButton->isEnabled());
        QVERIFY(manager->exportAssetToFile(id, exported));
        QTRY_VERIFY(QFile::exists(exported));
        QFile result(exported);
        QVERIFY(result.open(QIODevice::ReadOnly));
        QCOMPARE(result.readAll(), expected);
        // R-R3-49 (parity Task 7): a Core at transmitSettingsVersion 7
        // takes a restore from this window off the air.
        QSignalSpy restoreRequested(manager, &DspAssetDialog::restoreCorrectionRequested);
        QTRY_VERIFY(restore->isEnabled());
        restore->click();
        QCOMPARE(restoreRequested.size(), 1);
        QCOMPARE(restoreRequested.last().at(0).toString(), id);
        // A Core below version 7 keeps restore with remote transmit.
        StationCapabilities capabilities = server.buildCapabilities();
        capabilities.transmitSettingsVersion = 6;
        coreEnd->sendText(SessionMessages::encode(
            SessionMessages::capabilities(capabilities.toUpdates())));
        QTRY_VERIFY(!restore->isEnabled());
        restore->click();
        QCOMPARE(restoreRequested.size(), 1);
        QCOMPARE(remote.pureSignalFacade()->requestAction(
            Ps3Action::RestoreCorrection, {{"assetId", id}}), 0u);
    }

    void compactRemoteReadoutsFollowCoreStatusAndClearOnDisconnect()
    {
        RadioModel source;
        QJsonObject status = QJsonDocument::fromJson(
            source.pureSignalFacade()->statusJson().toUtf8()).object();
        status["psEnabled"] = true;
        status["mox"] = true;
        status["correctionsApplied"] = true;
        status["feedbackLevel"] = 149;
        status["attemptedCalibrations"] = 7;
        status["successfulCalibrations"] = 5;
        status["engineState"] = 4;
        status["correctionSummaryValid"] = true;
        status["correctionGainAtPeak"] = 0.8125;
        status["correctionPhaseSpanDegrees"] = 3.25;
        RadioModel remote(RadioModel::Role::Remote);
        PureSignalSessionFacade* facade = remote.pureSignalFacade();
        facade->setRemoteCapabilities(true, false);
        facade->applyRemoteProperty("available", true);
        PsaIndicatorWidget indicator(&remote);
        PureSignalApplet applet(&remote);
        QVERIFY(facade->applyRemoteProperty("statusJson",
            QString::fromUtf8(QJsonDocument(status).toJson(QJsonDocument::Compact))));
        QVERIFY(!indicator.findChild<QLabel*>("lblCorrPeak"));
        QCOMPARE(indicator.findChildren<QLabel*>().size(), 1);
        QVERIFY(indicator.findChild<QLabel*>("lblPSFeedback"));
        QCOMPARE(indicator.fbText(), QStringLiteral("Feedback 149"));
        QCOMPARE(facade->statusSnapshot().correctionGainAtPeak, 0.8125);
        QLabel* correction = applet.findChild<QLabel*>("PsAppletCorrectionDbLabel");
        QVERIFY(correction);
        QCOMPARE(correction->text(), QStringLiteral("Correction: Applied"));
        QLabel* diagnostic = applet.findChild<QLabel*>("PsAppletStatusLabel");
        QVERIFY(diagnostic);
        QVERIFY(diagnostic->text().contains("Collect"));
        QLabel* calibrations = applet.findChild<QLabel*>("PsAppletIterationsLabel");
        QVERIFY(calibrations);
        QVERIFY(calibrations->text().contains("5 / 7"));
        // A new feedback level can arrive without another calibration attempt.
        status["feedbackLevel"] = 171;
        status["correctionsApplied"] = false;
        status["correctionSummaryValid"] = false;
        QVERIFY(facade->applyRemoteProperty("statusJson",
            QString::fromUtf8(QJsonDocument(status).toJson(QJsonDocument::Compact))));
        QCOMPARE(indicator.fbText(), QStringLiteral("Feedback 171"));
        QCOMPARE(correction->text(), QStringLiteral("Correction: Off"));
        facade->resetSession();
        QCOMPARE(indicator.fbText(), QStringLiteral("Feedback —"));
        QCOMPARE(correction->text(), QStringLiteral("Correction: Off"));
        QVERIFY(!remote.pureSignal());
    }

    void remoteIndicatorConsumesStatusWithoutALocalCoordinator()
    {
        RadioModel source;
        source.installPureSignalForTest(nullptr);
        QJsonObject status = QJsonDocument::fromJson(
            source.pureSignalFacade()->statusJson().toUtf8()).object();
        QVERIFY(!status.isEmpty());
        status["psEnabled"] = true;
        status["mox"] = true;
        status["feedbackLevel"] = 150;
        status["attemptedCalibrations"] = 1;
        status["successfulCalibrations"] = 1;
        status["correctionsApplied"] = true;
        status["txMonitorDdc"] = 3;
        status["feedbackDdc"] = 2;
        status["feedbackChannelId"] = 6;
        status["pumpActive"] = true;
        // Counters must survive beyond JSON's exact floating-point range.
        status["pairedBlocks"] = QStringLiteral("9007199254740993");
        RadioModel remote(RadioModel::Role::Remote);
        remote.pureSignalSettings()->initializeAutoCalPreference(true);
        PureSignalSessionFacade* facade = remote.pureSignalFacade();
        facade->setRemoteCapabilities(true, false);
        facade->applyRemoteProperty("available", true);
        PsaIndicatorWidget indicator(&remote);
        QVERIFY(facade->applyRemoteProperty("statusJson",
            QString::fromUtf8(QJsonDocument(status).toJson(QJsonDocument::Compact))));
        QCOMPARE(indicator.fbText(), QStringLiteral("Feedback 150"));
        QCOMPARE(indicator.psText(), QStringLiteral("PureSignal3"));
        const Ps3StatusSnapshot readback = facade->statusSnapshot();
        QCOMPARE(readback.txMonitorDdc, 3);
        QCOMPARE(readback.feedbackDdc, 2);
        QCOMPARE(readback.feedbackChannelId, 6);
        QVERIFY(readback.pumpActive);
        QCOMPARE(readback.pairedBlocks, std::uint64_t{9007199254740993ULL});
        QVERIFY(!remote.pureSignal());
        QVERIFY(!facade->canActuate());
        facade->resetSession();
        QCOMPARE(indicator.psText(), QStringLiteral("PureSignal3"));
    }
};

QTEST_MAIN(TestPs3Session)
#include "tst_ps3_session.moc"
