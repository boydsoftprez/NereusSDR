// no-port-check: test-only. Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// =================================================================
// tests/tst_level_calibration_station.cpp  (NereusSDR)
// =================================================================
//
// Level Cal as a Core procedure: the Core runs Thetis CalibrateLevel
// (console.cs:9856-10232 [v2.10.3.15]) through LevelCalibrationService,
// a paired device starts and cancels it over the session with the
// startLevelCalibration and cancelLevelCalibration verbs
// (radioHardwareVersion 12), and radio's levelCalRunning, levelCalPercent,
// levelCalMessage and levelCalSucceeded carry its progress to a peer that
// declared levelCalibration 1. A remote window's RadioModel makes the same
// calls a local one does.
//
// The receiver is a fake meter and spectrum (FakeLevelCalibrationHost.h)
// put in place of the Core's own receiver; every wait is zero or short.
// The on-air case sets the transmit state only. Nothing here keys a radio
// or touches a device.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29 - Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30 - Level Cal 2: another device's slice is refused, the
//                device's own slice runs; a paired remote window names
//                its own active slice; rx2AttenuatorVersion reaches
//                only a peer that declared it. J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30 - Radio codec lane: radioMicVersion, after
//                rx2AttenuatorVersion and before coreBuildInfo, only to a
//                peer that declared radioMic. J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "FakeLevelCalibrationHost.h"
#include "core/BuildIdentity.h"
#include "core/LevelCalibrationService.h"
#include "core/StepAttenuatorController.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/session/StationClient.h"
#include "core/settings/SettingsProxy.h"

using namespace NereusSDR::LevelCalTest;

namespace {

const QString kPairedOnly = QStringLiteral("Calibrate the receive level from a paired device.");
const QString kUnread = QStringLiteral("The Core could not read this request.");
const QString kNoSlice = QStringLiteral("The slice to calibrate is not open.");
const QString kFinished = QStringLiteral("Level calibration finished.");
const QString kCanceled = QStringLiteral("Level calibration was canceled.");
const QString kRadioOff = QStringLiteral("Turn the radio on before calibrating the receive level.");

QHash<QByteArray, int> withLevelCal()
{
    QHash<QByteArray, int> features = kHolder;
    features.insert(QByteArrayLiteral("levelCalibration"), 1);
    return features;
}

QList<MirrorUpdate> startArgs(double levelDbm = -50.0, double hz = kCentre + 1000.0,
                              qint64 sliceId = 0)
{
    return {f64("levelDbm", levelDbm), f64("frequencyHz", hz), int64("sliceId", sliceId)};
}

bool accepted(const QJsonObject& result)
{
    return result.value(QStringLiteral("accepted")).toBool(false);
}

QString reason(const QJsonObject& result)
{
    return result.value(QStringLiteral("reason")).toString();
}

// The Core's service on the fake receiver. `holdMs` keeps the run going
// that long at its last wait, so a test can see it running.
LevelCalibrationService* fakeReceiver(Core& core, FakeHost& fake, int holdMs = 0)
{
    LevelCalibrationService* service = core.model->levelCalibrationServiceForTest();
    if (service == nullptr) {
        return nullptr;
    }
    service->setHostForTest(&fake);
    LevelCalibrationRun::Timings timings = instant();
    timings.finalSettleMs = holdMs;
    service->setTimingsForTest(timings);
    return service;
}

} // namespace

class TstLevelCalibrationStation : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // A paired device starts the run; the Core runs it and every step of
    // it reaches the device.
    void pairedDevice_startRunsAndReportsProgress()
    {
        Core core;
        FakeHost fake;
        QVERIFY(fakeReceiver(core, fake, 300) != nullptr);
        Device a;
        core.pair(a);
        LoopbackTransport* app = core.signIn(a, withLevelCal());
        QVERIFY(admitted(app));
        QCOMPARE(latest(app->received(), QStringLiteral("radio"),
                        QStringLiteral("levelCalRunning")).toBool(true), false);

        const QJsonObject started =
            core.invoke(app, "startLevelCalibration", startArgs());
        QVERIFY2(accepted(started), qPrintable(reason(started)));
        QTRY_COMPARE(latest(app->received(), QStringLiteral("radio"),
                            QStringLiteral("levelCalRunning")).toBool(false), true);
        QTRY_COMPARE(latest(app->received(), QStringLiteral("radio"),
                            QStringLiteral("levelCalRunning")).toBool(true), false);
        QTRY_COMPARE(latest(app->received(), QStringLiteral("radio"),
                            QStringLiteral("levelCalSucceeded")).toBool(false), true);
        QCOMPARE(latest(app->received(), QStringLiteral("radio"),
                        QStringLiteral("levelCalMessage")).toString(), kFinished);
        QCOMPARE(latest(app->received(), QStringLiteral("radio"),
                        QStringLiteral("levelCalPercent")).toInteger(), 100);
        QVERIFY(core.model->levelCalSucceeded());
        // The run measured and put the receiver back.
        QVERIFY(fake.meterReads > 0);
        QCOMPARE(fake.vfoHz, 14200000.0);
        QCOMPARE(fake.mode, DSPMode::USB);
    }

    // A window signed in with the pairing token cannot start it.
    void tokenSession_refused()
    {
        Core core(/*upgradedWithToken=*/true);
        FakeHost fake;
        QVERIFY(fakeReceiver(core, fake) != nullptr);
        LoopbackTransport* token = core.tokenSignIn(withLevelCal());
        QVERIFY(admitted(token));
        QVERIFY2(OperatorWording::isPlain(kPairedOnly), qPrintable(kPairedOnly));
        const QJsonObject refused = core.invoke(token, "startLevelCalibration", startArgs());
        QVERIFY(!accepted(refused));
        QCOMPARE(reason(refused), kPairedOnly);
        QVERIFY(fake.log.isEmpty());
        QVERIFY(!core.model->levelCalRunning());
        // Cancel only stops a run, so it is taken from anyone.
        QVERIFY(accepted(core.invoke(token, "cancelLevelCalibration")));
    }

    // On the air: the Core's on-air refusal, as for its other radio verbs.
    // State only: nothing is keyed.
    void onAir_refused()
    {
        Core core;
        FakeHost fake;
        QVERIFY(fakeReceiver(core, fake) != nullptr);
        Device a;
        core.pair(a);
        LoopbackTransport* app = core.signIn(a, withLevelCal());
        QVERIFY(admitted(app));
        core.model->transmitModel().setMox(true);
        const QJsonObject refused = core.invoke(app, "startLevelCalibration", startArgs());
        QVERIFY(!accepted(refused));
        QCOMPARE(reason(refused), RadioModel::onAirReason());
        QVERIFY(fake.log.isEmpty());
        core.model->transmitModel().setMox(false);
    }

    // No live radio: the run's own refusal comes back as the result.
    void radioOff_refused()
    {
        Core core;
        FakeHost fake;
        fake.live = false;
        QVERIFY(fakeReceiver(core, fake) != nullptr);
        Device a;
        core.pair(a);
        LoopbackTransport* app = core.signIn(a, withLevelCal());
        QVERIFY(admitted(app));
        const QJsonObject refused = core.invoke(app, "startLevelCalibration", startArgs());
        QVERIFY(!accepted(refused));
        QCOMPARE(reason(refused), kRadioOff);
        QVERIFY(fake.log.isEmpty());
    }

    void unreadableOrUnknownSlice_refused()
    {
        Core core;
        FakeHost fake;
        QVERIFY(fakeReceiver(core, fake) != nullptr);
        Device a;
        core.pair(a);
        LoopbackTransport* app = core.signIn(a, withLevelCal());
        QVERIFY(admitted(app));
        const QJsonObject missing = core.invoke(
            app, "startLevelCalibration", {f64("levelDbm", -50.0), int64("sliceId", 0)});
        QCOMPARE(reason(missing), kUnread);
        const QJsonObject wrongKind = core.invoke(
            app, "startLevelCalibration",
            {int64("levelDbm", -50), f64("frequencyHz", kCentre), int64("sliceId", 0)});
        QCOMPARE(reason(wrongKind), kUnread);
        const QJsonObject cancelArgs =
            core.invoke(app, "cancelLevelCalibration", {int64("sliceId", 0)});
        QCOMPARE(reason(cancelArgs), kUnread);
        QVERIFY2(OperatorWording::isPlain(kNoSlice), qPrintable(kNoSlice));
        const QJsonObject noSlice =
            core.invoke(app, "startLevelCalibration", startArgs(-50.0, kCentre + 1000.0, 7));
        QVERIFY(!accepted(noSlice));
        QCOMPARE(reason(noSlice), kNoSlice);
        QVERIFY(fake.log.isEmpty());
    }

    // Level Cal 2: a run retunes its slice and switches the preamp, so a
    // device calibrates a slice it controls and no other. Another device's
    // slice, named or reached as the Core's active slice (-1), is refused
    // with the ownership words and nothing moves; its own slice runs.
    void anotherDevicesSlice_refused_ownSliceRuns()
    {
        Core core;
        FakeHost fake;
        QVERIFY(fakeReceiver(core, fake) != nullptr);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, withLevelCal());
        LoopbackTransport* appB = core.signIn(b, withLevelCal());
        QVERIFY(admitted(appA) && admitted(appB));
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, a.key.fingerprint());
        const QStringList bKeys = heldKeys(appB, QStringLiteral("slice:"));
        QCOMPARE(bKeys.size(), 1);
        const int bSlice = bKeys.first().mid(6).toInt();
        QVERIFY(bSlice != 0);
        const QString belongsToA = ownedElsewhere(QStringLiteral("iPhone"));

        const QJsonObject named =
            core.invoke(appB, "startLevelCalibration", startArgs(-50.0, kCentre + 1000.0, 0));
        QVERIFY(!accepted(named));
        QCOMPARE(reason(named), belongsToA);

        core.model->setActiveSliceById(0);
        QCOMPARE(core.model->activeSlice()->sliceIndex(), 0);
        const QJsonObject active =
            core.invoke(appB, "startLevelCalibration", startArgs(-50.0, kCentre + 1000.0, -1));
        QVERIFY(!accepted(active));
        QCOMPARE(reason(active), belongsToA);
        QVERIFY(fake.log.isEmpty());
        QVERIFY(!core.model->levelCalRunning());

        const QJsonObject own =
            core.invoke(appB, "startLevelCalibration", startArgs(-50.0, kCentre + 1000.0, bSlice));
        QVERIFY2(accepted(own), qPrintable(reason(own)));
        QTRY_COMPARE(latest(appB->received(), QStringLiteral("radio"),
                            QStringLiteral("levelCalSucceeded")).toBool(false), true);
        QVERIFY(fake.meterReads > 0);
        QVERIFY(!fake.log.isEmpty());
    }

    // Cancel stops the run and puts everything back (Thetis closing the
    // progress window); with nothing running it is accepted and does
    // nothing.
    void cancel_stopsAndRestores()
    {
        Core core;
        FakeHost fake;
        QVERIFY(fakeReceiver(core, fake, 60000) != nullptr);
        Device a;
        core.pair(a);
        LoopbackTransport* app = core.signIn(a, withLevelCal());
        QVERIFY(admitted(app));
        QVERIFY(accepted(core.invoke(app, "cancelLevelCalibration")));
        QVERIFY(accepted(core.invoke(app, "startLevelCalibration", startArgs())));
        QTRY_VERIFY(core.model->levelCalRunning());
        const QJsonObject again = core.invoke(app, "startLevelCalibration", startArgs());
        QVERIFY(!accepted(again));
        QVERIFY(reason(again).contains(QStringLiteral("already")));
        QVERIFY(accepted(core.invoke(app, "cancelLevelCalibration")));
        QTRY_COMPARE(latest(app->received(), QStringLiteral("radio"),
                            QStringLiteral("levelCalMessage")).toString(), kCanceled);
        QCOMPARE(latest(app->received(), QStringLiteral("radio"),
                        QStringLiteral("levelCalRunning")).toBool(true), false);
        QCOMPARE(latest(app->received(), QStringLiteral("radio"),
                        QStringLiteral("levelCalSucceeded")).toBool(true), false);
        QCOMPARE(fake.vfoHz, 14200000.0);
        QCOMPARE(fake.ritOn, true);
        QCOMPARE(fake.ritHz, 250);
        QCOMPARE(fake.phoneBuffer, 4096);
        QCOMPARE(fake.preamp1, PreampMode::Minus20);
    }

    // The progress properties go only to a peer that declared the feature.
    void progress_onlyToADeclaringPeer()
    {
        Core core;
        Device a;
        core.pair(a);
        LoopbackTransport* app = core.signIn(a, kHolder);
        QVERIFY(admitted(app));
        // The snapshot reached this peer, without the run's properties.
        QVERIFY(!app->received().isEmpty());
        for (const QByteArray& wire : app->received()) {
            QVERIFY2(!wire.contains("\"levelCal"), wire.constData());
        }
    }

    // A local window's calls run on its own model.
    void localModel_runsItsOwnService()
    {
        RadioModel model;
        QVERIFY(model.levelCalibrationRunAvailable());
        // No radio connected: the run's own refusal, synchronously.
        QCOMPARE(model.requestStartLevelCalibration(-50.0f, kCentre, -1),
                 QStringLiteral("Open a slice before calibrating the receive level."));
        model.addSlice(QStringLiteral("pan-0"));
        QCOMPARE(model.requestStartLevelCalibration(-50.0f, kCentre, -1), kRadioOff);
        QVERIFY(!model.levelCalRunning());
        // A remote-only apply is refused on a local model.
        QVERIFY(!model.applyStationLevelCalValue("levelCalRunning", true));
    }

    // A remote window: the same calls go to its Core, the progress comes
    // back, and a refusal the Core sends later arrives as a signal.
    void remoteWindow_roundTrip()
    {
        Core core(/*upgradedWithToken=*/true);
        core.server->setTokenSessionsMayChangeRadioForTest(true);
        // The Core's step attenuator, so it offers its radio's hardware
        // verbs (StationServer::radioHardwareVersion).
        StepAttenuatorController stepAtt;
        core.model->setStepAttController(&stepAtt);
        const auto unbind = qScopeGuard([&core]() { core.model->setStepAttController(nullptr); });
        FakeHost fake;
        QVERIFY(fakeReceiver(core, fake) != nullptr);

        RadioModel remote(RadioModel::Role::Remote);
        QVERIFY(!remote.levelCalibrationRunAvailable());
        QCOMPARE(remote.requestStartLevelCalibration(-50.0f, kCentre + 1000.0, 0),
                 QStringLiteral("Not connected to a Core, so the level calibration was not sent."));
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        QSignalSpy completed(&client, &StationClient::handshakeComplete);
        auto* stationEnd = new LoopbackTransport(QStringLiteral("cal-station"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("cal-client"), this);
        stationEnd->linkTo(clientEnd);
        client.startSession(clientEnd, core.server->token());
        core.server->acceptTransport(stationEnd);
        QTRY_COMPARE(completed.count(), 1);
        QTRY_VERIFY(remote.levelCalibrationRunAvailable());

        QSignalSpy state(&remote, &RadioModel::levelCalStateChanged);
        QCOMPARE(remote.requestStartLevelCalibration(-50.0f, kCentre + 1000.0, 0), QString());
        QTRY_VERIFY(remote.levelCalSucceeded());
        QCOMPARE(remote.levelCalMessage(), kFinished);
        QCOMPARE(remote.levelCalPercent(), 100);
        QVERIFY(!remote.levelCalRunning());
        QVERIFY(state.count() > 0);

        // The Core refuses after the window sent it.
        fake.live = false;
        QSignalSpy refused(&remote, &RadioModel::levelCalibrationRefused);
        QCOMPARE(remote.requestStartLevelCalibration(-50.0f, kCentre + 1000.0, 0), QString());
        QTRY_COMPARE(refused.count(), 1);
        QCOMPARE(refused.first().at(0).toString(), kRadioOff);
        QVERIFY(remote.requestCancelLevelCalibration().isEmpty());

        // The session ends: nothing is known until the next one.
        remote.clearStationLevelCal();
        QVERIFY(!remote.levelCalSucceeded());
        QVERIFY(remote.levelCalMessage().isEmpty());
        QCOMPARE(remote.levelCalPercent(), 0);
    }

    // Level Cal 2: rx2AttenuatorVersion 1 reaches a peer that declared
    // rx2Attenuator, as the last entry before coreBuildInfo; a peer that
    // did not is sent none. The entry reads back.
    void rx2AttenuatorVersion_onlyToADeclaringPeer()
    {
        const QString savedVersion = QCoreApplication::applicationVersion();
        const auto restore = qScopeGuard([&savedVersion]() {
            QCoreApplication::setApplicationVersion(savedVersion);
        });
        QCoreApplication::setApplicationVersion(QStringLiteral("0.5.2"));
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        QHash<QByteArray, int> asks = kHolder;
        asks.insert(QByteArrayLiteral("rx2Attenuator"), 1);
        asks.insert(QByteArrayLiteral("coreBuildInfo"), 1);
        LoopbackTransport* appA = core.signIn(a, asks);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA) && admitted(appB));
        QCOMPARE(capability(appA->received(), QStringLiteral("rx2AttenuatorVersion")), 1);
        QVERIFY(!capability(appB->received(), QStringLiteral("rx2AttenuatorVersion")).has_value());
        const QJsonArray caps = firstOfType(appA->received(), QStringLiteral("capabilities"))
                                    .value(QStringLiteral("properties")).toArray();
        QVERIFY(caps.size() >= 2);
        QCOMPARE(caps.at(caps.size() - 2).toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("rx2AttenuatorVersion"));
        QCOMPARE(caps.last().toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("coreBuildInfo"));

        StationCapabilities sent;
        sent.rx2AttenuatorVersion = 1;
        QCOMPARE(StationCapabilities::fromUpdates(sent.toUpdates()).rx2AttenuatorVersion, 1);
        QCOMPARE(StationCapabilities::fromUpdates(StationCapabilities{}.toUpdates())
                     .rx2AttenuatorVersion, 0);
    }

    // Radio codec lane: radioMicVersion 1 (the catalogue's board.radioMic
    // and radioMicNote) reaches a peer that declared radioMic, after
    // rx2AttenuatorVersion and before coreBuildInfo, which stays last; a
    // peer that did not is sent none. The entry reads back.
    void radioMicVersion_onlyToADeclaringPeer()
    {
        const QString savedVersion = QCoreApplication::applicationVersion();
        const auto restore = qScopeGuard([&savedVersion]() {
            QCoreApplication::setApplicationVersion(savedVersion);
        });
        QCoreApplication::setApplicationVersion(QStringLiteral("0.5.2"));
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        QHash<QByteArray, int> asks = kHolder;
        asks.insert(QByteArrayLiteral("rx2Attenuator"), 1);
        asks.insert(QByteArrayLiteral("radioMic"), 1);
        asks.insert(QByteArrayLiteral("coreBuildInfo"), 1);
        LoopbackTransport* appA = core.signIn(a, asks);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA) && admitted(appB));
        QCOMPARE(capability(appA->received(), QStringLiteral("radioMicVersion")), 1);
        QVERIFY(!capability(appB->received(), QStringLiteral("radioMicVersion")).has_value());
        const QJsonArray caps = firstOfType(appA->received(), QStringLiteral("capabilities"))
                                    .value(QStringLiteral("properties")).toArray();
        QVERIFY(caps.size() >= 3);
        QCOMPARE(caps.at(caps.size() - 3).toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("rx2AttenuatorVersion"));
        QCOMPARE(caps.at(caps.size() - 2).toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("radioMicVersion"));
        QCOMPARE(caps.last().toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("coreBuildInfo"));

        StationCapabilities sent;
        sent.radioMicVersion = 1;
        QCOMPARE(StationCapabilities::fromUpdates(sent.toUpdates()).radioMicVersion, 1);
        QCOMPARE(StationCapabilities::fromUpdates(StationCapabilities{}.toUpdates())
                     .radioMicVersion, 0);
    }

    // Level Cal 2 (remote parity): a paired remote window's Start names
    // its own active slice, as the phone does. With the station's active
    // slice another device's, the run goes on the window's own slice
    // instead of being refused (-1 would have named the other device's).
    void remoteWindow_namesItsOwnSlice()
    {
        Core core;
        StepAttenuatorController stepAtt;
        core.model->setStepAttController(&stepAtt);
        const auto unbind = qScopeGuard([&core]() { core.model->setStepAttController(nullptr); });
        FakeHost fake;
        QVERIFY(fakeReceiver(core, fake) != nullptr);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, withLevelCal());
        QVERIFY(admitted(appA));
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, a.key.fingerprint());

        QTemporaryDir keyDir;
        const auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        PairedDevice record;
        record.id = key->fingerprint();
        record.publicKeySpki = key->publicKeySpki();
        record.name = QStringLiteral("Shack MacBook");
        record.kind = QStringLiteral("computer");
        QVERIFY(core.server->deviceStore()->add(record));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        client.setDeviceIdentity(key, QStringLiteral("Shack MacBook"), QStringLiteral("MacBook"));
        auto* stationEnd = new LoopbackTransport(QStringLiteral("station"), this);
        auto* windowEnd = new LoopbackTransport(QStringLiteral("window"), this);
        stationEnd->setPeerAddress(QStringLiteral("192.0.2.30"));
        windowEnd->setPeerCertificateSha256(core.certSha256());
        stationEnd->linkTo(windowEnd);
        client.startSession(windowEnd, QString(), QString(),
                            core.server->stationIdentity().fingerprint());
        core.server->acceptTransport(stationEnd);
        QVERIFY(QTest::qWaitFor([&client]() { return client.stationLinkReady(); }, 5000));
        QTRY_VERIFY(remote.levelCalibrationRunAvailable());

        const QList<int> own = core.model->sliceOwnership()->ownedBy(key->fingerprint());
        QCOMPARE(own.size(), 1);
        QVERIFY(own.first() != 0);
        QTRY_VERIFY(remote.activeSlice() != nullptr
                    && remote.activeSlice()->sliceIndex() == own.first());
        core.model->setActiveSliceById(0);
        QCOMPARE(core.model->activeSlice()->sliceIndex(), 0);

        QSignalSpy refused(&remote, &RadioModel::levelCalibrationRefused);
        QCOMPARE(remote.requestStartLevelCalibration(-50.0f, kCentre + 1000.0, -1), QString());
        QTRY_VERIFY(remote.levelCalSucceeded());
        QCOMPARE(refused.count(), 0);
        QVERIFY(fake.meterReads > 0);
        // The window sent its own slice's id.
        QList<QJsonObject> starts;
        for (const QJsonObject& o : ofType(stationEnd->received(), QStringLiteral("command.invoke"))) {
            if (o.value(QStringLiteral("verb")).toString() == QStringLiteral("startLevelCalibration")) {
                starts.append(o);
            }
        }
        QCOMPARE(starts.size(), 1);
        qint64 sentSlice = -2;
        for (const QJsonValue& arg : starts.first().value(QStringLiteral("args")).toArray()) {
            if (arg.toObject().value(QStringLiteral("name")).toString() == QStringLiteral("sliceId")) {
                sentSlice = arg.toObject().value(QStringLiteral("value")).toInteger();
            }
        }
        QCOMPARE(sentSlice, qint64(own.first()));
    }
};

QTEST_GUILESS_MAIN(TstLevelCalibrationStation)
#include "tst_level_calibration_station.moc"
