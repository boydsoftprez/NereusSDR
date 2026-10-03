// no-port-check: NereusSDR-original lifecycle acceptance tests for the PS3
// settings/coordinator boundary and stale-session pump retirement.

#include <QtTest>

#include "core/PsccPump.h"
#include "core/PureSignal.h"
#include "core/TxAnalyzer.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/dsp/DspAssetValidation.h"
#include "core/wdsp_api.h"
#include "models/PureSignalSettings.h"

#ifdef HAVE_WDSP
extern "C" void pscc(int channel, int size, double* tx, double* rx);
#endif

#include <QTemporaryDir>
#include <QFile>
#include <QElapsedTimer>

using namespace NereusSDR;

#ifdef HAVE_WDSP
namespace {
struct NativePsFixture {
    std::vector<double> tx;
    std::vector<double> rx;
    std::array<double, 512> txAudio{};
    std::array<double, 512> txOutput{};

    NativePsFixture() : tx(2 * 4096), rx(2 * 4096)
    {
        for (int bin = 0; bin < 16; ++bin) {
            const double amplitude = (static_cast<double>(bin) + 0.5) / 16.0;
            for (int sample = 0; sample < 256; ++sample) {
                const int index = bin * 256 + sample;
                tx[2 * index] = amplitude;
                rx[2 * index] = amplitude;
            }
        }
    }

    void pump(int channel)
    {
        ::pscc(channel, 4096, tx.data(), rx.data());
        pumpTxOnly(channel);
    }

    void pumpTxOnly(int channel)
    {
        for (int i = 0; i < 8; ++i) {
            int error = 0;
            ::fexchange0(channel, txAudio.data(), txOutput.data(), &error);
            QCOMPARE(error, 0);
        }
    }
};

bool restoreNativeCorrection(TxChannel* tx, NativePsFixture* fixture,
                             const QString& path)
{
    tx->setRunning(true);
    const auto token = tx->psRestoreCorr(path);
    if (!token) {
        return false;
    }
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < 8000) {
        fixture->pumpTxOnly(tx->channelId());
        const auto status =
            tx->psFileOperationStatus(Ps3FileOperationKind::Restore);
        const auto correction = tx->psCorrectionState();
        if (status && !status->pending
            && status->generation == *token
            && status->result == Ps3FileOperationResult::Success
            && correction && correction->run && !correction->busy) {
            return true;
        }
        QTest::qWait(1);
    }
    return false;
}
}
#endif

class TestPs3Lifecycle final : public QObject {
    Q_OBJECT

private slots:
    void hydrationBypassesEditGateWithoutDirtyingOrActuating();
    void editGateRejectsNormalChangesButAllowsDiagnosticReadback();
    void deniedOperationalPreferenceRemainsDesiredOnly();
    void calibrationAttemptsUseInfoSeven();
    void retiringPumpDropsLatePairedPacket();
    void rapidSingleThenOffDoesNotReactivateCalibration();
    void realWdspOffAcknowledgesWithRunCalIntentFalse();
    void realWdspFileFailureCompletesExactGeneration();
    void realWdspOffCancelsPendingRestoreBeforeRamp();
    void realTxHydrationPersistsIntentWithoutActuation();
    void retiredSingleDoesNotReplayWhenRunCalIsLaterEnabled();
    void realWdspRepeatedCreateDestroyKeepsPsAbiValid();
};

void TestPs3Lifecycle::hydrationBypassesEditGateWithoutDirtyingOrActuating()
{
    PureSignalSettings settings;
    settings.setEditGate([](QString* reason) {
        if (reason) *reason = QStringLiteral("receive-only");
        return false;
    });
    PureSignal coordinator(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    coordinator.setSettings(&settings);
    coordinator.setOperationalPermissionPredicate([] { return false; });
    coordinator.setOperationalReadinessPredicate([] { return false; });

    QSignalSpy writes(&settings, &PureSignalSettings::configurationChanged);
    QSignalSpy rejects(&settings, &PureSignalSettings::editRejected);
    coordinator.initializeAutoCalPreference(true);

    QVERIFY(settings.autoCalEnabled());
    QVERIFY(coordinator.isAutoCalEnabled());
    QCOMPARE(writes.count(), 0);
    QCOMPARE(rejects.count(), 0);
    QVERIFY(!coordinator.resumeAutomaticCalibrationPreference());
    QVERIFY(!coordinator.applyAcceptedSettingsToEngine());
    QVERIFY(!coordinator.applyCurrentCorrection());
}

void TestPs3Lifecycle::editGateRejectsNormalChangesButAllowsDiagnosticReadback()
{
    PureSignalSettings settings;
    settings.setEditGate([](QString* reason) {
        if (reason) *reason = QStringLiteral("station cannot author settings");
        return false;
    });
    QSignalSpy rejected(&settings, &PureSignalSettings::editRejected);
    QSignalSpy dirty(&settings, &PureSignalSettings::configurationChanged);

    settings.setQuickAttenuate(true);
    QVERIFY(!settings.quickAttenuate());
    QCOMPARE(rejected.count(), 1);
    QCOMPARE(dirty.count(), 0);

    QVERIFY(settings.applyStationDiagnostic(
        QByteArray("lastLoadError"),
        QVariant(QStringLiteral("remote persistence failed"))));
    QCOMPARE(settings.lastLoadError(),
             QStringLiteral("remote persistence failed"));
    QCOMPARE(dirty.count(), 0);
    QVERIFY(!settings.applyStationDiagnostic(QByteArray("autoCalEnabled"),
                                              QVariant(true)));
    QVERIFY(!settings.autoCalEnabled());
}

void TestPs3Lifecycle::deniedOperationalPreferenceRemainsDesiredOnly()
{
    PureSignalSettings settings;
    PureSignal coordinator(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    coordinator.setSettings(&settings);
    coordinator.setOperationalPermissionPredicate([] { return false; });

    QSignalSpy desiredChanged(&coordinator,
                              &PureSignal::autoCalEnabledChanged);
    coordinator.setAutoCalEnabled(true);

    QVERIFY(settings.autoCalEnabled());
    QCOMPARE(desiredChanged.count(), 1);
    QVERIFY(!coordinator.canActuate());
    QVERIFY(!coordinator.resumeAutomaticCalibrationPreference());
    QVERIFY(!coordinator.beginRestoreCorrections(QStringLiteral("x.ps3")));
    QVERIFY(!coordinator.beginSaveCorrections(QStringLiteral("x.ps3")));
}

void TestPs3Lifecycle::calibrationAttemptsUseInfoSeven()
{
    PureSignal coordinator(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    coordinator.setTimersEnabled(false);
    QSignalSpy successes(&coordinator, &PureSignal::calibrationCountChanged);
    QSignalSpy attempts(&coordinator, &PureSignal::calibrationAttemptsChanged);

    int info[16] = {};
    info[5] = 3;
    info[7] = 9;
    coordinator.processNewInfo(info);

    QCOMPARE(coordinator.calibrationCount(), 3);
    QCOMPARE(coordinator.calibrationAttempts(), 9);
    QCOMPARE(successes.count(), 1);
    QCOMPARE(attempts.count(), 1);

    // A success-only change must not masquerade as a new attempt.
    info[5] = 4;
    coordinator.processNewInfo(info);
    QCOMPARE(successes.count(), 2);
    QCOMPARE(attempts.count(), 1);
}

void TestPs3Lifecycle::retiringPumpDropsLatePairedPacket()
{
    PsccPump pump;
    pump.setTxChannelId(42);
    pump.setActive(true, /*txMonDdc=*/1, /*psFbDdc=*/0);
    pump.setSkipPsccForTests(true);
    pump.retireSession();

    QCOMPARE(pump.txChannelId(), -1);
    QVERIFY(!pump.isActive());
    const QVector<float> samples(8, 0.25f);
    pump.onPsPairedIqData(/*psFbDdc=*/0, samples,
                          /*txMonDdc=*/1, samples);
    QCOMPARE(pump.totalBlocksPumped(), qint64{0});
    QCOMPARE(pump.lastPsccArgsForTests().callCount, qint64{0});
}

void TestPs3Lifecycle::rapidSingleThenOffDoesNotReactivateCalibration()
{
    TxChannel tx(WdspEngine::kTxChannelId);
    PureSignal coordinator(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
    coordinator.setTimersEnabled(false);
    coordinator.setCorrectionStateForTest(Ps3CorrectionState{false, false});
    QSignalSpy started(&coordinator, &PureSignal::calibrationStarted);
    QSignalSpy enabled(&coordinator, &PureSignal::psEnabledChanged);

    coordinator.singleCalibrate();
    QCOMPARE(started.count(), 1);
    coordinator.reset();

    int resetInfo[16] = {};
    for (int i = 0; i < 6; ++i) {
        coordinator.processNewInfo(resetInfo);
    }
    QCOMPARE(started.count(), 1);
    QVERIFY(!coordinator.isPsEnabled());
    for (const QList<QVariant>& transition : enabled) {
        QVERIFY(!transition.at(0).toBool());
    }
}

void TestPs3Lifecycle::realWdspOffAcknowledgesWithRunCalIntentFalse()
{
#ifndef HAVE_WDSP
    QSKIP("requires wdsp_static");
#else
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    TxAnalyzer analyzer;
    WdspEngine engine;
    engine.setSynchronousInitForTest(true);
    QVERIFY(engine.initialize(directory.path() + QLatin1Char('/')));
    analyzer.setFftSize(4096);
    analyzer.setBlockSize(WdspEngine::kTxDspBufferSize);
    analyzer.start();
    analyzer.stop();
    TxChannel* tx = engine.createTxChannel(WdspEngine::kTxChannelId);
    QVERIFY(tx);

    PureSignalSettings settings;
    settings.setRunCalibrationProcessing(false);
    PureSignal coordinator(&engine, tx, nullptr, nullptr, nullptr, nullptr);
    coordinator.setTimersEnabled(false);
    coordinator.setSettings(&settings);
    coordinator.setOperationalPermissionPredicate([] { return true; });
    coordinator.setOperationalReadinessPredicate([] { return true; });
    QVERIFY(coordinator.applyAcceptedSettingsToEngine());
    QCOMPARE(tx->psRunCal(), std::optional<bool>(false));
    QVERIFY(!tx->psCorrectionAvailable().value_or(true));
    QVERIFY(!coordinator.applyCurrentCorrection());
    QSignalSpy calibrationStarted(&coordinator, &PureSignal::calibrationStarted);
    coordinator.singleCalibrate();
    QCOMPARE(calibrationStarted.count(), 0);
    coordinator.setAutoCalEnabled(true);
    QVERIFY(settings.autoCalEnabled());
    QVERIFY(!coordinator.resumeAutomaticCalibrationPreference());
    QCOMPARE(tx->psRunCal(), std::optional<bool>(false));
    QVERIFY(!coordinator.isPsEnabled());

    const QString seedPath =
        QFINDTESTDATA("fixtures/dsp/ps3-v2-source-writer.txt");
    QVERIFY(!seedPath.isEmpty());
    QFile seedFile(seedPath);
    QVERIFY(seedFile.open(QIODevice::ReadOnly));
    const auto seedValidation =
        DspAssetValidation::validatePs3Correction(seedFile.readAll());
    QVERIFY2(seedValidation.accepted, qPrintable(seedValidation.error));

    NativePsFixture fixture;
    QVERIFY2(restoreNativeCorrection(tx, &fixture, seedPath),
             "failed to install a validated correction through native restore");
    auto correction = tx->psCorrectionState();
    QVERIFY(correction && correction->run && !correction->busy);

    const QString nativePath = directory.path() + QStringLiteral("/native.ps3");
    QSignalSpy fileCompleted(&coordinator, &PureSignal::fileOperationCompleted);
    const auto saveToken = coordinator.beginSaveCorrections(nativePath);
    QVERIFY(saveToken.has_value());
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        fixture.pump(tx->channelId());
        coordinator.pollTimerTick();
        return fileCompleted.count() == 1;
    }(), 3000);
    auto saveStatus = tx->psFileOperationStatus(Ps3FileOperationKind::Save);
    QVERIFY(saveStatus.has_value());
    QCOMPARE(saveStatus->generation, saveToken->nativeCompletionGeneration);
    QCOMPARE(saveStatus->result, Ps3FileOperationResult::Success);
    QFile nativeFile(nativePath);
    QVERIFY(nativeFile.open(QIODevice::ReadOnly));
    const auto validation = DspAssetValidation::validatePs3Correction(nativeFile.readAll());
    QVERIFY2(validation.accepted, qPrintable(validation.error));
    fileCompleted.clear();

    // Reproduce the hard case: the persisted run-cal intent is false
    // immediately before Off starts while a real IQC correction is active.
    tx->setPSRunCal(0);
    QCOMPARE(tx->psRunCal(), std::optional<bool>(false));

    // Reproduce production teardown ordering. No pscc/fexchange call is
    // allowed after TXA stops; Off must still publish a real native ack.
    tx->setPSMox(false);
    tx->setRunning(false);
    coordinator.reset();
    coordinator.pollTimerTick();
    correction = tx->psCorrectionState();
    QVERIFY(correction.has_value());
    QVERIFY(!correction->run);
    QVERIFY(!correction->busy);
    QCOMPARE(tx->psRunCal(), std::optional<bool>(false));
    QVERIFY(!coordinator.isPsEnabled());

    // The quiescent stop retained all current curves. Reapply must start IQC
    // itself and finish through TXA audio without requiring pscc feedback.
    QVERIFY(tx->psCorrectionAvailable().value_or(false));
    tx->setRunning(true);
    QVERIFY(coordinator.applyCurrentCorrection());
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        fixture.pumpTxOnly(tx->channelId());
        coordinator.pollTimerTick();
        correction = tx->psCorrectionState();
        return correction && correction->run && !correction->busy;
    }(), 3000);

    // Active stop also bypasses pscc: RequestPSCorrectionStop enters IQC END
    // directly and TXA audio alone supplies the ramp-complete acknowledgement.
    coordinator.reset();
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        fixture.pumpTxOnly(tx->channelId());
        coordinator.pollTimerTick();
        correction = tx->psCorrectionState();
        return correction && !correction->run && !correction->busy;
    }(), 3000);
    QVERIFY(coordinator.applyCurrentCorrection());
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        fixture.pumpTxOnly(tx->channelId());
        coordinator.pollTimerTick();
        correction = tx->psCorrectionState();
        return correction && correction->run && !correction->busy;
    }(), 3000);

    const auto restoreToken = coordinator.beginRestoreCorrections(nativePath);
    QVERIFY(restoreToken.has_value());
    std::optional<Ps3FileOperationStatus> restoreStatus;
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        fixture.pumpTxOnly(tx->channelId());
        restoreStatus = tx->psFileOperationStatus(Ps3FileOperationKind::Restore);
        return restoreStatus && !restoreStatus->pending;
    }(), 3000);
    QVERIFY(restoreStatus.has_value());
    QCOMPARE(restoreStatus->generation,
             restoreToken->nativeCompletionGeneration);
    QCOMPARE(restoreStatus->result, Ps3FileOperationResult::Success);
    // Let the coordinator observe the successful restore, then immediately
    // supersede its ready-to-apply state with Off before another state tick.
    coordinator.pollTimerTick();
    tx->setPSMox(false);
    tx->setRunning(false);
    coordinator.reset();
    coordinator.pollTimerTick();
    correction = tx->psCorrectionState();
    QVERIFY(correction && !correction->run && !correction->busy);
    QVERIFY(!coordinator.isPsEnabled());

    coordinator.setTxChannel(nullptr);
    engine.destroyTxChannel(WdspEngine::kTxChannelId);
#endif
}

void TestPs3Lifecycle::realWdspFileFailureCompletesExactGeneration()
{
#ifndef HAVE_WDSP
    QSKIP("requires wdsp_static");
#else
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    WdspEngine engine;
    engine.setSynchronousInitForTest(true);
    QVERIFY(engine.initialize(directory.path() + QLatin1Char('/')));
    TxChannel* tx = engine.createTxChannel(WdspEngine::kTxChannelId);
    QVERIFY(tx);

    PureSignal coordinator(&engine, tx, nullptr, nullptr, nullptr, nullptr);
    coordinator.setTimersEnabled(false);
    coordinator.setSessionGeneration(73);
    QSignalSpy completed(&coordinator, &PureSignal::fileOperationCompleted);
    const QString impossible = directory.path()
        + QStringLiteral("/missing/never-created/correction.ps3");
    const auto token = coordinator.beginSaveCorrections(impossible);
    QVERIFY(token.has_value());
    QCOMPARE(token->sessionGeneration, std::uint64_t{73});

    QTRY_VERIFY_WITH_TIMEOUT([&] {
        coordinator.pollTimerTick();
        return completed.count() == 1;
    }(), 3000);
    const auto status = tx->psFileOperationStatus(Ps3FileOperationKind::Save);
    QVERIFY(status.has_value());
    QVERIFY(!status->pending);
    QCOMPARE(status->generation, token->nativeCompletionGeneration);
    QCOMPARE(status->result, Ps3FileOperationResult::Failure);
    const QList<QVariant> args = completed.takeFirst();
    QCOMPARE(args.at(0).toInt(), static_cast<int>(Ps3FileOperationKind::Save));
    QCOMPARE(args.at(1).toInt(), static_cast<int>(Ps3FileOperationResult::Failure));
    QCOMPARE(args.at(2).toULongLong(), quint64{73});
    QCOMPARE(args.at(3).toULongLong(),
             static_cast<quint64>(token->nativeCompletionGeneration));

    coordinator.setTxChannel(nullptr);
    engine.destroyTxChannel(WdspEngine::kTxChannelId);
#endif
}

void TestPs3Lifecycle::realWdspOffCancelsPendingRestoreBeforeRamp()
{
#ifndef HAVE_WDSP
    QSKIP("requires wdsp_static");
#else
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    WdspEngine engine;
    engine.setSynchronousInitForTest(true);
    QVERIFY(engine.initialize(directory.path() + QLatin1Char('/')));
    TxChannel* tx = engine.createTxChannel(WdspEngine::kTxChannelId);
    QVERIFY(tx);

    const QString validFixturePath =
        QFINDTESTDATA("fixtures/dsp/ps3-v2-source-writer.txt");
    QVERIFY(!validFixturePath.isEmpty());
    QFile validFixture(validFixturePath);
    QVERIFY(validFixture.open(QIODevice::ReadOnly));
    const QByteArray validBytes = validFixture.readAll();
    QVERIFY(DspAssetValidation::validatePs3Correction(validBytes).accepted);

    PureSignal coordinator(&engine, tx, nullptr, nullptr, nullptr, nullptr);
    coordinator.setTimersEnabled(false);
    coordinator.setOperationalPermissionPredicate([] { return true; });
    coordinator.setOperationalReadinessPredicate([] { return true; });
    coordinator.setSessionGeneration(44);
    QSignalSpy retired(&coordinator, &PureSignal::fileOperationRetired);
    const auto token = coordinator.beginRestoreCorrections(validFixturePath);
    QVERIFY(token.has_value());

    // TXA remains stopped. The worker can parse the valid correction and
    // enter IQC BEGIN, but no audio frames exist to finish the ramp. This is
    // the dangerous post-parse state Off must retire without applying later.
    std::optional<Ps3FileOperationStatus> status;
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        status = tx->psFileOperationStatus(Ps3FileOperationKind::Restore);
        const auto correction = tx->psCorrectionState();
        return status && status->pending
            && correction && correction->run && correction->busy;
    }(), 3000);

    coordinator.reset();
    QCOMPARE(retired.count(), 1);

    QTRY_VERIFY_WITH_TIMEOUT([&] {
        status = tx->psFileOperationStatus(Ps3FileOperationKind::Restore);
        return status && !status->pending;
    }(), 3000);
    QCOMPARE(status->generation, token->nativeCompletionGeneration);
    QCOMPARE(status->result, Ps3FileOperationResult::Cancelled);
    QVERIFY(!tx->cancelPsFileOperation(Ps3FileOperationKind::Restore));
    const auto correction = tx->psCorrectionState();
    QVERIFY(correction.has_value());
    QVERIFY(!correction->run);
    QVERIFY(!correction->busy);

    coordinator.setTxChannel(nullptr);
    engine.destroyTxChannel(WdspEngine::kTxChannelId);
#endif
}

void TestPs3Lifecycle::realTxHydrationPersistsIntentWithoutActuation()
{
#ifndef HAVE_WDSP
    QSKIP("requires wdsp_static");
#else
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    WdspEngine engine;
    engine.setSynchronousInitForTest(true);
    QVERIFY(engine.initialize(directory.path() + QLatin1Char('/')));
    TxChannel* tx = engine.createTxChannel(WdspEngine::kTxChannelId);
    QVERIFY(tx);

    PureSignalSettings settings;
    PureSignal coordinator(&engine, tx, nullptr, nullptr, nullptr, nullptr);
    coordinator.setTimersEnabled(false);
    coordinator.setSettings(&settings);
    coordinator.setOperationalPermissionPredicate([] { return true; });
    coordinator.setOperationalReadinessPredicate([] { return true; });
    QVERIFY(coordinator.applyAcceptedSettingsToEngine());
    const double appliedDelayBefore = coordinator.appliedTxDelayNs();
    const double nativePeakBefore = tx->getPSHWPeak();
    const auto runCalBefore = tx->psRunCal();
    QVERIFY(runCalBefore.has_value());

    coordinator.beginSettingsHydration();
    settings.setAutoCalEnabled(true);
    settings.setRunCalibrationProcessing(false);
    settings.setMoxDelaySeconds(0.8);
    settings.setLoopDelaySeconds(1.7);
    settings.setRequestedTxDelayNs(930.0);
    settings.setHardwarePeakOverride(0.42);
    settings.setHardwarePeakOverrideEnabled(true);
    coordinator.endSettingsHydration();

    QVERIFY(settings.autoCalEnabled());
    QVERIFY(!settings.runCalibrationProcessing());
    QCOMPARE(settings.moxDelaySeconds(), 0.8);
    QCOMPARE(settings.loopDelaySeconds(), 1.7);
    QCOMPARE(settings.requestedTxDelayNs(), 930.0);
    QVERIFY(settings.hardwarePeakOverrideEnabled());
    QCOMPARE(settings.hardwarePeakOverride(), 0.42);

    QCOMPARE(tx->psRunCal(), runCalBefore);
    QCOMPARE(coordinator.appliedTxDelayNs(), appliedDelayBefore);
    QCOMPARE(tx->getPSHWPeak(), nativePeakBefore);
    QVERIFY(!coordinator.isPsEnabled());
    const auto correction = tx->psCorrectionState();
    QVERIFY(correction.has_value());
    QVERIFY(!correction->run);
    QVERIFY(!correction->busy);

    coordinator.setTxChannel(nullptr);
    engine.destroyTxChannel(WdspEngine::kTxChannelId);
#endif
}

void TestPs3Lifecycle::retiredSingleDoesNotReplayWhenRunCalIsLaterEnabled()
{
#ifndef HAVE_WDSP
    QSKIP("requires wdsp_static");
#else
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    WdspEngine engine;
    engine.setSynchronousInitForTest(true);
    QVERIFY(engine.initialize(directory.path() + QLatin1Char('/')));
    TxChannel* tx = engine.createTxChannel(WdspEngine::kTxChannelId);
    QVERIFY(tx);

    PureSignalSettings settings;
    settings.setRunCalibrationProcessing(true);
    PureSignal coordinator(&engine, tx, nullptr, nullptr, nullptr, nullptr);
    coordinator.setTimersEnabled(false);
    coordinator.setSettings(&settings);
    coordinator.setOperationalPermissionPredicate([] { return true; });
    coordinator.setOperationalReadinessPredicate([] { return true; });
    QVERIFY(coordinator.applyAcceptedSettingsToEngine());
    QSignalSpy started(&coordinator, &PureSignal::calibrationStarted);

    coordinator.singleCalibrate();
    QCOMPARE(started.count(), 1);
    settings.setRunCalibrationProcessing(false);
    QVERIFY(!settings.runCalibrationProcessing());
    coordinator.initializeAutoCalPreference(true);
    QVERIFY(settings.autoCalEnabled());
    coordinator.retireSessionOperations();
    QVERIFY(settings.autoCalEnabled());

    coordinator.beginSettingsHydration();
    settings.setRunCalibrationProcessing(true);
    coordinator.endSettingsHydration();
    QVERIFY(settings.runCalibrationProcessing());

    int resetInfo[16] = {};
    resetInfo[15] = 0; // native calcc LRESET
    coordinator.processNewInfo(resetInfo);
    coordinator.processNewInfo(resetInfo);
    coordinator.processNewInfo(resetInfo);
    QCOMPARE(started.count(), 1);
    QVERIFY(settings.autoCalEnabled());
    QVERIFY(!coordinator.isPsEnabled());
    const auto correction = tx->psCorrectionState();
    QVERIFY(correction.has_value());
    QVERIFY(!correction->run);
    QVERIFY(!correction->busy);

    coordinator.setTxChannel(nullptr);
    engine.destroyTxChannel(WdspEngine::kTxChannelId);
#endif
}

void TestPs3Lifecycle::realWdspRepeatedCreateDestroyKeepsPsAbiValid()
{
#ifndef HAVE_WDSP
    QSKIP("requires wdsp_static");
#else
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    WdspEngine engine;
    engine.setSynchronousInitForTest(true);
    QVERIFY(engine.initialize(directory.path() + QLatin1Char('/')));

    for (int cycle = 0; cycle < 8; ++cycle) {
        TxChannel* tx = engine.createTxChannel(WdspEngine::kTxChannelId);
        QVERIFY2(tx, qPrintable(QStringLiteral("create cycle %1").arg(cycle)));
        QVERIFY(tx->psRunCal().has_value());
        QVERIFY(tx->psCorrectionState().has_value());
        QVERIFY(tx->psFileOperationStatus(Ps3FileOperationKind::Save).has_value());
        QVERIFY(tx->psFileOperationStatus(Ps3FileOperationKind::Restore).has_value());
        engine.destroyTxChannel(WdspEngine::kTxChannelId);
        QVERIFY(!engine.txChannel(WdspEngine::kTxChannelId));
    }
#endif
}

QTEST_GUILESS_MAIN(TestPs3Lifecycle)
#include "tst_ps3_lifecycle.moc"
