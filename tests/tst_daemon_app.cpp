// tests/tst_daemon_app.cpp
//
// R1 Task 10 -- DaemonApp connects a headless nereusd process to a radio
// and creates min(cfg.sliceCount, connected-board-maxSlices) slices,
// using RadioModel's own addSlice() rather than the GUI-only
// addSliceOnPan(). Before this class existed, a headless daemon that
// called RadioModel::connectToRadio() got Slice A and nothing else --
// every other slice-creation call site is wired from MainWindow.
//
// This test primes the RadioModel via DaemonApp::primeBoardForTest(),
// NOT a real RadioModel::connectToRadio() round trip (whether against
// real hardware or a P1FakeRadio loopback fake). Found while writing
// this test: RadioModel::connectToRadio() contains a synchronous nested
// QEventLoop that blocks the calling thread until WdspEngine finishes
// generating FFTW wisdom (RadioModel.cpp: "Block here while the wisdom
// worker finishes, pumping the Qt event loop") -- deliberate, existing
// behaviour (it is how the GUI shows a wisdom progress dialog on a cold
// first connect; see CLAUDE.md's "First run generates FFTW wisdom
// (~15 min)"), not something this task introduces. Measured directly
// while writing this test: over 5 minutes on a cold cache, at which
// point QtTest's own per-function watchdog aborted the process rather
// than connectToRadio() ever returning within a usable test budget. No
// test anywhere in this suite calls RadioModel::connectToRadio() for
// exactly this reason (grep tests/*.cpp for ".connectToRadio(" --
// every hit goes through P1RadioConnection/P2RadioConnection directly,
// never through RadioModel). Every RadioModel-level test instead primes
// board state via RadioModel::setBoardForTest() + configureStreamPool(),
// e.g. tst_p1_hl2_rx2_wiring.cpp's second_live_stream_enables_rx2_end_to_end();
// primeBoardForTest() puts DaemonApp through the identical two calls.
//
// The "connects to the radio" half of DaemonApp::start() (production
// discovery + RadioModel::connectToRadio()) is exercised by manual
// verification instead (see task-10-report.md), not by an automated
// test, for the same reason.
//
// Uses QTEST_MAIN (not APPLESS_MAIN): RadioModel's construction touches
// Qt machinery (timers, WdspEngine, AudioEngine) that wants a
// QCoreApplication, matching every other RadioModel-constructing test.
//
// Modification history (NereusSDR):
//   2026-09-20: cover DaemonApp's RadioModel teardown state relay,
//               by J.J. Boyd (KG4VCF), with AI-assisted implementation
//               via OpenAI Codex.
//   2026-09-23: cover the display load governor's wiring (R-R3-08,
//               R-R3-37, R-R3-40), by J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-23: the Core's attenuator range, its mirrored `stepAtt`
//               object and the debounced save (R-R3-46, R-R3-11), by J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-23: nereusd publishes no VAX device (R-R3-44), by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-23: cover remote_bind "::" taking IPv4 and IPv6, by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-25: a Core restart keeps each slice's owner or held-for mark,
//               and a manifest from before owners restores slices with no
//               owner (iPhone app plan Task 73, R-IOS-02), by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: the config file's sample_rate_hz is a starting value only;
//               a rate already saved for the radio wins at start (R-R3-49),
//               by J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.

#include <QtTest/QtTest>

#include <QHostAddress>
#include <QScopeGuard>
#include <QSslSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <utility>

#include "core/AudioEngine.h"
#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/MoxController.h"
#include "core/RadioDiscovery.h"
#include "core/SampleRateCatalog.h"
#include "core/StepAttenuatorController.h"
#include "core/StepAttenuatorFacade.h"
#define private public
#include "core/daemon/DaemonApp.h"
#include "core/station/StationHost.h"
#undef private
#include "core/daemon/DaemonConfig.h"
#include "core/daemon/DisplayLoadInputs.h"
#include "core/session/media/DisplayLoadGovernor.h"
#include "core/session/StationServer.h"
#include "core/security/StationIdentity.h"
#include "core/session/StationLanAnnouncer.h"
#include "core/session/DnsSdAdvertiser.h"
#include "core/session/StationDevicesFacade.h"
#include "core/security/PairingWindow.h"
#include "core/security/StationLabel.h"
#include "core/AppSettings.h"
#include "core/ReceiveLayoutStore.h"
#include "core/SliceOwnership.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {
// iPhone app Task 12 put the Core's listener on by default (TCP 47910 on
// every interface). A test Core opens no listener unless the test asks for
// one on a port of its own.
NereusSDR::DaemonConfig testCoreConfig()
{
    NereusSDR::DaemonConfig config = NereusSDR::DaemonConfig::defaults();
    config.remotePort = 0;
    // iPhone app Task 17: nor its status page (on by default beside the
    // listener, TCP 47911), which listenerConfig() below would otherwise
    // open on a shared port.
    config.statusPage = false;
    return config;
}
} // namespace

namespace {

// A loopback listener on a port the OS just said was free, so a parallel
// ctest shard cannot collide with it.
DaemonConfig listenerConfig()
{
    DaemonConfig cfg = testCoreConfig();
    QTcpServer probe;
    if (probe.listen(QHostAddress::LocalHost, 0)) {
        cfg.remotePort = static_cast<int>(probe.serverPort());
        probe.close();
    }
    cfg.remoteBind = QStringLiteral("127.0.0.1");
    return cfg;
}

} // namespace

class TstDaemonApp : public QObject {
    Q_OBJECT
private slots:
    // R-R3-49: the config file's sample_rate_hz is a starting value only.
    // It seeds hardware/<mac>/radioInfo/sampleRate when the radio has no
    // saved rate; a saved rate (from any window, or an earlier seed) wins.
    void configRateDoesNotOverrideASavedRate()
    {
        const QString mac = QStringLiteral("aa:bb:cc:dd:49:01");
        const QString key = QStringLiteral("radioInfo/sampleRate");
        AppSettings& settings = AppSettings::instance();
        const auto forget = qScopeGuard([&settings, mac] {
            settings.clearHardwareValues(mac);
            settings.save();
        });
        settings.setHardwareValue(mac, key, 768000);
        DaemonConfig cfg = testCoreConfig();
        cfg.sampleRateHz = 192000;
        cfg.sampleRateExplicit = true;
        QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral(
            "using the saved sample rate 768000 for this radio \\(the config "
            "file's 192000 is only a starting value\\)")));
        DaemonApp app;
        app.applyConfigToSettings(cfg, mac);
        QCOMPARE(settings.hardwareValue(mac, key).toInt(), 768000);
    }

    void configRateSeedsWhenNoRateIsSaved()
    {
        const QString mac = QStringLiteral("aa:bb:cc:dd:49:02");
        const QString key = QStringLiteral("radioInfo/sampleRate");
        AppSettings& settings = AppSettings::instance();
        settings.clearHardwareValues(mac);
        const auto forget = qScopeGuard([&settings, mac] {
            settings.clearHardwareValues(mac);
            settings.save();
        });
        DaemonConfig cfg = testCoreConfig();
        cfg.sampleRateHz = 192000;
        cfg.sampleRateExplicit = true;
        QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral(
            "seeded the sample rate from the config file: 192000")));
        DaemonApp app;
        app.applyConfigToSettings(cfg, mac);
        QCOMPARE(settings.hardwareValue(mac, key).toInt(), 192000);
    }

    void noConfigRateLeavesTheSavedRate()
    {
        const QString mac = QStringLiteral("aa:bb:cc:dd:49:03");
        const QString key = QStringLiteral("radioInfo/sampleRate");
        AppSettings& settings = AppSettings::instance();
        const auto forget = qScopeGuard([&settings, mac] {
            settings.clearHardwareValues(mac);
            settings.save();
        });
        settings.setHardwareValue(mac, key, 384000);
        const DaemonConfig cfg = testCoreConfig();
        QVERIFY(!cfg.sampleRateExplicit);
        DaemonApp app;
        app.applyConfigToSettings(cfg, mac);
        QCOMPARE(settings.hardwareValue(mac, key).toInt(), 384000);
    }

    // A saved rate the board cannot run still falls back to the board
    // default at connect (resolveSampleRate), as it did before; the config
    // file does not paper over it.
    void anUnsupportedSavedRateStillFallsBack()
    {
        const QString mac = QStringLiteral("aa:bb:cc:dd:49:04");
        const QString key = QStringLiteral("radioInfo/sampleRate");
        AppSettings& settings = AppSettings::instance();
        const auto forget = qScopeGuard([&settings, mac] {
            settings.clearHardwareValues(mac);
            settings.save();
        });
        settings.setHardwareValue(mac, key, 1536000);
        DaemonConfig cfg = testCoreConfig();
        cfg.sampleRateHz = 96000;
        cfg.sampleRateExplicit = true;
        QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral(
            "using the saved sample rate 1536000")));
        DaemonApp app;
        app.applyConfigToSettings(cfg, mac);
        const BoardCapabilities& caps = BoardCapsTable::forModel(HPSDRModel::HERMESLITE);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral(".*")));
        QCOMPARE(resolveSampleRate(settings, mac, ProtocolVersion::Protocol1, caps,
                                   HPSDRModel::HERMESLITE),
                 192000);
    }
    // G-16: the config file's audio_device is a starting value too, like
    // sample_rate_hz above. It seeds audio/Speakers/DeviceName only when no
    // speaker choice is saved; a saved choice (a window's pick, including
    // an explicit "platform default" saved as an empty name) wins.
    void configAudioDeviceDoesNotOverrideASavedSpeaker()
    {
        const QString key = QStringLiteral("audio/Speakers/DeviceName");
        AppSettings& settings = AppSettings::instance();
        const auto forget = qScopeGuard([&settings, key] {
            settings.remove(key);
            settings.save();
        });
        settings.setValue(key, QStringLiteral("USB Audio CODEC"));
        DaemonConfig cfg = testCoreConfig();
        cfg.audioDevice = QStringLiteral("Built-in Output");
        QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral(
            "using the saved speaker device \"USB Audio CODEC\" \\(the config "
            "file's \"Built-in Output\" is only a starting value\\)")));
        DaemonApp app;
        app.applyConfigToSettings(cfg, QString());
        QCOMPARE(settings.value(key).toString(), QStringLiteral("USB Audio CODEC"));
    }

    void configAudioDeviceKeepsASavedPlatformDefault()
    {
        const QString key = QStringLiteral("audio/Speakers/DeviceName");
        AppSettings& settings = AppSettings::instance();
        const auto forget = qScopeGuard([&settings, key] {
            settings.remove(key);
            settings.save();
        });
        settings.setValue(key, QString());
        DaemonConfig cfg = testCoreConfig();
        cfg.audioDevice = QStringLiteral("Built-in Output");
        QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral(
            "using the saved speaker device \"\" \\(the config file's")));
        DaemonApp app;
        app.applyConfigToSettings(cfg, QString());
        QVERIFY(settings.contains(key));
        QCOMPARE(settings.value(key).toString(), QString());
    }

    void configAudioDeviceSeedsWhenNoSpeakerIsSaved()
    {
        const QString key = QStringLiteral("audio/Speakers/DeviceName");
        AppSettings& settings = AppSettings::instance();
        settings.remove(key);
        const auto forget = qScopeGuard([&settings, key] {
            settings.remove(key);
            settings.save();
        });
        DaemonConfig cfg = testCoreConfig();
        cfg.audioDevice = QStringLiteral("Built-in Output");
        QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral(
            "seeded the speaker device from the config file: \"Built-in Output\"")));
        DaemonApp app;
        app.applyConfigToSettings(cfg, QString());
        QCOMPARE(settings.value(key).toString(), QStringLiteral("Built-in Output"));
    }

    void noConfigAudioDeviceLeavesTheSpeakerUnset()
    {
        const QString key = QStringLiteral("audio/Speakers/DeviceName");
        AppSettings& settings = AppSettings::instance();
        settings.remove(key);
        const auto forget = qScopeGuard([&settings, key] {
            settings.remove(key);
            settings.save();
        });
        const DaemonConfig cfg = testCoreConfig();
        QVERIFY(cfg.audioDevice.isEmpty());
        DaemonApp app;
        app.applyConfigToSettings(cfg, QString());
        QVERIFY(!settings.contains(key));
    }

    // R-R3-44: nereusd publishes no VAX device on the Core host, receive
    // or transmit. Its engine refuses them before anything connects (the
    // engine-level proof that start() then opens none is in
    // tst_remote_vax_feeder, aCoreOpensNoVaxDeviceOfEitherDirection).
    void publishesNoVaxDevices()
    {
        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(app.start(testCoreConfig()));
        AudioEngine* const engine = app.m_radioModel->audioEngine();
        QVERIFY(!engine->vaxOutputsAllowed());
        // Even with devices on offer, none is made.
        int asked = 0;
        engine->setVaxBusFactoryForTest([&asked](int) -> std::unique_ptr<IAudioBus> {
            ++asked;
            return nullptr;
        });
        engine->openVaxOutputs();
        engine->setVaxEnabled(1, true);
        QCOMPARE(asked, 0);
        for (int channel = 1; channel <= 4; ++channel) {
            QVERIFY(!engine->isVaxBusOpen(channel));
        }
        engine->setVaxBusFactoryForTest({});
        app.stop();
    }

    void malformedDisplayLimitsCannotReplaceARunningDaemon()
    {
        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(app.start(testCoreConfig()));
        RadioModel* const running = app.m_radioModel.get();
        QVERIFY(running);
        DaemonConfig invalid = testCoreConfig();
        invalid.displayApplicationBytesPerSecond = 1000;
        QVERIFY(!app.start(invalid));
        QCOMPARE(app.m_radioModel.get(), running);
        app.stop();
    }

    // iPhone app plan Task 34: remote_transmit deny installs the persistent
    // receive-only policy before station startup, as every Core did before;
    // allow (the default) leaves transmit to the station transmit gate.
    void installsReceiveOnlyPolicyBeforeStationStartup()
    {
        DaemonConfig cfg = testCoreConfig();
        cfg.sliceCount = 1;
        cfg.remoteTransmitAllowed = false;

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(app.start(cfg));
        QVERIFY(app.m_radioModel != nullptr);
        QVERIFY2(app.m_radioModel->receiveOnlyStationPolicy(),
                 "nereusd constructed a hardware-owning RadioModel without "
                 "the persistent receive-only policy remote_transmit deny asks for");
        app.stop();

        DaemonApp allowing;
        allowing.primeBoardForTest(HPSDRHW::HermesLite);
        DaemonConfig allow = testCoreConfig();
        allow.sliceCount = 1;
        QVERIFY(allow.remoteTransmitAllowed);
        QVERIFY(allowing.start(allow));
        QVERIFY(!allowing.m_radioModel->receiveOnlyStationPolicy());
        allowing.stop();
    }

    // The whole point of the task: a headless start must create the
    // configured number of slices, not just Slice A. HermesLite's
    // BoardCapabilities row (BoardCapabilities.cpp kHermesLite) sets
    // maxSlices = 5, so 3 is well within the SKU's real capacity and
    // must come back exactly, not clamped.
    // iPhone app Task 73 (ruling 5.3): a Core restart keeps each slice's
    // owner, or the device it is held for. Nobody is on a Core that has just
    // started, so a device's slice comes back held for it by the station
    // device (ruling 5.2 step 1); the next save writes the marks back as
    // they are. (The HL2 here holds two slices; the next test restores a
    // slice with no owner.)
    void aRestartKeepsOwnersAndHeldForMarks()
    {
        const QString mac = QStringLiteral("AA:BB:CC:DD:EE:73");
        const QByteArray phone(32, '\x31');
        const QByteArray tablet(32, '\x32');
        const QList<ReceiveSliceState> layout{
            {0, QStringLiteral("pan-0"), 7074000.0, DSPMode::USB, phone, {}},
            {1, QStringLiteral("pan-0"), 7075000.0, DSPMode::USB, SliceOwnership::stationDevice(),
             tablet},
        };
        QVERIFY(ReceiveLayoutStore::stage(AppSettings::instance(), mac, layout));
        // Saved and removed on disk too: this test's settings file is kept
        // between runs.
        const auto forget = qScopeGuard([&mac] {
            AppSettings::instance().remove(QStringLiteral("hardware/%1/receiveLayout")
                                               .arg(AppSettings::normalizedRadioMac(mac)));
            AppSettings::instance().save();
        });
        {
            DaemonApp app;
            app.primeBoardForTest(HPSDRHW::HermesLite, mac);
            QVERIFY(app.start(testCoreConfig()));
            QCOMPARE(app.sliceCount(), 2);
            const SliceOwnership* ownership = app.m_radioModel->sliceOwnership();
            QCOMPARE(ownership->mark(0).owner, SliceOwnership::stationDevice());
            QCOMPARE(ownership->mark(0).heldFor, phone);
            QCOMPARE(ownership->mark(1).owner, SliceOwnership::stationDevice());
            QCOMPARE(ownership->mark(1).heldFor, tablet);
            app.stop();
        }
        const ReceiveLayoutStore::LoadResult saved =
            ReceiveLayoutStore::load(AppSettings::instance(), mac);
        QCOMPARE(static_cast<int>(saved.state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Loaded));
        QCOMPARE(saved.slices.size(), 2);
        QCOMPARE(saved.slices.at(0).owner, SliceOwnership::stationDevice());
        QCOMPARE(saved.slices.at(0).heldFor, phone);
        QCOMPARE(saved.slices.at(1).heldFor, tablet);
    }

    // A manifest from before owners: every slice restored with no owner.
    void aManifestFromBeforeOwnersRestoresSlicesWithNoOwner()
    {
        const QString mac = QStringLiteral("AA:BB:CC:DD:EE:74");
        const QString normalized = AppSettings::normalizedRadioMac(mac);
        AppSettings::instance().setHardwareValue(
            normalized, QStringLiteral("receiveLayout"),
            QStringLiteral(R"({"version":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":7074000,"dspMode":1},{"id":1,"panKey":"pan-0","frequencyHz":7075000,"dspMode":1}]})"));
        const auto forget = qScopeGuard([&normalized] {
            AppSettings::instance().remove(QStringLiteral("hardware/%1/receiveLayout").arg(normalized));
            AppSettings::instance().save();
        });
        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite, mac);
        QVERIFY(app.start(testCoreConfig()));
        QCOMPARE(app.sliceCount(), 2);
        const SliceOwnership* ownership = app.m_radioModel->sliceOwnership();
        QCOMPARE(ownership->unowned(), (QList<int>{0, 1}));
        app.stop();
    }

    void createsConfiguredSliceCount()
    {
        DaemonConfig cfg = testCoreConfig();
        cfg.sliceCount = 3;

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);

        QVERIFY(app.start(cfg));
        QCOMPARE(app.sliceCount(), 3);

        app.stop();
    }

    // Fix round 1, Finding 1: mintFftEndpoints() populating m_topology
    // is not enough by itself -- it has to reach RadioModel's own live
    // FFTRouter (RadioModel::fftRouter()) via FftTopology::applyTo(),
    // the same way MainWindow::rebuildFftRouting() ends with
    // m_topology.applyTo(*router). Before the fix, m_topology was
    // subscribed to but never pushed anywhere, so the router never knew
    // about any of it.
    //
    // A fresh DaemonApp's endpoint-id counter starts at 0, so with
    // sliceCount = 1 the single slice created gets "daemon-ep-0" and
    // binds to stream 0 (the allocator's first placement for a slice
    // with no existing occupant to share with).
    void fftRouterReflectsSubscriptions()
    {
        DaemonConfig cfg = testCoreConfig();
        cfg.sliceCount = 1;

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);

        QVERIFY(app.start(cfg));
        QCOMPARE(app.sliceCount(), 1);

        const QList<int> mapped =
            app.fftRouterMappingsForTest(QStringLiteral("daemon-ep-0"));
        QCOMPARE(mapped.size(), 1);
        QCOMPARE(mapped.first(), 0);

        // Fix round 2, Finding 1 (reopened): exercise stop()'s ACTUAL
        // FFT-topology teardown step (clearFftTopologyForTest() runs the
        // exact same clearFftTopology() stop() calls) WHILE the
        // RadioModel and its FFTRouter are still alive, so this
        // assertion can actually fail if the removal regresses. Querying
        // only AFTER a full stop() cannot tell "the router was cleared"
        // apart from "the router no longer exists" -- stop() destroys
        // the RadioModel, and the FFTRouter Qt-parented to it, in the
        // very next step -- which is exactly how the round 1 fix's
        // broken stop() (m_topology = FftTopology{}; before
        // publishFftTopology(), which silently discarded
        // FftTopology's own record of what it had last pushed and so
        // removed nothing) passed this same test unnoticed.
        app.clearFftTopologyForTest();
        QVERIFY(app.fftRouterMappingsForTest(QStringLiteral("daemon-ep-0")).isEmpty());

        app.stop();

        // The RadioModel (and the FFTRouter it owned) is gone after the
        // full stop(); the observable contract is that DaemonApp reports
        // no mapping for anything, rather than a test reaching into a
        // dangling router pointer.
        QVERIFY(app.fftRouterMappingsForTest(QStringLiteral("daemon-ep-0")).isEmpty());
    }

    // HermesLite's maxSlices is 5 -- the request must clamp DOWN to the
    // board's real capability, not silently create 99 slices.
    void clampsSliceCountToBoardCapability()
    {
        DaemonConfig cfg = testCoreConfig();
        cfg.sliceCount = 99;

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);

        QVERIFY(app.start(cfg));
        QVERIFY(app.sliceCount() >= 1);
        QVERIFY(app.sliceCount() <= 5);   // no supported SKU exceeds 5

        app.stop();
    }

    void stopIsSafeWithoutStart()
    {
        DaemonApp app;
        app.stop();          // must not crash
        QCOMPARE(app.sliceCount(), 0);
    }

    // DaemonApp owns RadioModel with a unique_ptr. unique_ptr::reset() clears
    // that pointer before it deletes the old object, so a connection-state
    // relay emitted while the model tears down cannot query m_radioModel.
    // This takes the identical ownership shape explicitly, then emits the
    // source signal while the old model is still alive. The relay must use
    // the state argument, which is already the authoritative value.
    void connectionStateRelayDoesNotDereferenceReleasedModel()
    {
        DaemonConfig cfg = testCoreConfig();

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(app.start(cfg));

        std::unique_ptr<RadioModel> releasedModel = std::move(app.m_radioModel);
        QVERIFY(releasedModel != nullptr);
        const auto restoreModel = qScopeGuard([&app, &releasedModel]() {
            app.m_radioModel = std::move(releasedModel);
        });

        QSignalSpy connectedSpy(&app, &DaemonApp::radioConnected);
        emit releasedModel->connectionStateChanged(ConnectionState::Disconnected);
        QCOMPARE(connectedSpy.count(), 1);
        QCOMPARE(connectedSpy.takeFirst().at(0).toBool(), false);
    }

    void restartIsClean()
    {
        DaemonConfig cfg = testCoreConfig();
        cfg.sliceCount = 2;

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);

        QVERIFY(app.start(cfg));
        app.stop();
        // Carry-forward from the coordinator's dispatch: stop() must
        // leave sliceCount() == 0 BEFORE the next start(), not just
        // "eventually" after it. Checked explicitly rather than only
        // inferred from the post-restart count below.
        QCOMPARE(app.sliceCount(), 0);

        QVERIFY(app.start(cfg));
        QCOMPARE(app.sliceCount(), 2);   // not 4

        app.stop();
    }

    void headlessControllerUsesSaturnDefaultsAndCalibration()
    {
        DaemonConfig cfg = testCoreConfig();
        cfg.sliceCount = 1;

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::Saturn,
                              QStringLiteral("02:00:00:00:00:91"));
        QVERIFY(app.start(cfg));

        StepAttenuatorController* const controller =
            app.m_stepAttController.get();
        QVERIFY(controller != nullptr);
        QCOMPARE(app.m_radioModel->stepAttController(), controller);
        QVERIFY(controller->settingsLoaded());

        const auto& caps = app.m_radioModel->boardCapabilities();
        QCOMPARE(controller->minAttenuation(), caps.attenuator.minDb);
        QCOMPARE(controller->maxAttenuation(), caps.attenuator.maxDb);
        QCOMPARE(controller->hasStepAttenuatorCal(),
                 caps.hasStepAttenuatorCal);
        QVERIFY(!controller->isHpsdrBoard());

        // This synthetic MAC has no persisted step-att/preamp keys. Preserve
        // the controller's existing defaults: step ATT enabled at 0 dB.
        // ANAN-G2/Saturn's Thetis factory calibration is -4.476 dB, so the
        // live RadioModel offset must now be that value rather than the
        // preamp-Off branch's +15.524 dB.
        QVERIFY(controller->stepAttEnabled());
        QCOMPARE(controller->attenuatorDb(), 0);
        QCOMPARE(app.m_radioModel->rxMeterOffsetDb(),
                 static_cast<double>(-4.476f));

        app.stop();
        QVERIFY(app.m_stepAttController == nullptr);
    }

    void headlessControllerTracksTxBandModeAndMox()
    {
        DaemonConfig cfg = testCoreConfig();
        cfg.sliceCount = 1;
        // Task 34: receive-only, so the admission below still refuses.
        cfg.remoteTransmitAllowed = false;

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::Saturn);
        QVERIFY(app.start(cfg));

        StepAttenuatorController* const controller =
            app.m_stepAttController.get();
        SliceModel* const txSlice = app.m_radioModel->txBoundSlice();
        QVERIFY(controller != nullptr);
        QVERIFY(txSlice != nullptr);

        controller->setTxAttenuationForBand(Band::Band80m, 7);
        controller->setTxAttenuationForBand(Band::Band20m, 11);
        txSlice->setFrequency(3'830'000.0);
        txSlice->setDspMode(DSPMode::LSB);
        QCOMPARE(controller->attOnTxValue(), 7);
        QCOMPARE(controller->currentDspMode(), DSPMode::LSB);

        // Prove the daemon owns the desktop-equivalent MOX connection without
        // bypassing its receive-only policy. hardwareFlipped is the
        // authoritative controller notification DaemonApp wires to the step
        // attenuator; emitting it directly here isolates that signal/slot
        // projection from MOX admission. With PS active, the ordinary 7 dB TX
        // value is selected, then the synthetic RX notification restores 0 dB.
        controller->setPsActive(true);
        MoxController* const mox = app.m_radioModel->moxController();
        QVERIFY(mox != nullptr);
        emit mox->hardwareFlipped(true);
        QTRY_COMPARE(controller->attenuatorDb(), 7);
        emit mox->hardwareFlipped(false);
        QTRY_COMPARE(controller->attenuatorDb(), 0);

        // The real admission path remains receive-only: it rejects before
        // state advance and must not disturb the restored RX attenuation.
        QSignalSpy rejected(mox, &MoxController::moxRejected);
        mox->setMox(true);
        QCOMPARE(rejected.count(), 1);
        QVERIFY(!mox->isMox());
        QCOMPARE(controller->attenuatorDb(), 0);

        app.stop();
    }

    // R-R3-46: the Core's range is the one the local RX applet gives the
    // board (BoardCapsTable::stepAttMaxDb): an ANAN-100D (Angelia, Alex
    // filters) reaches 61 dB, not the board row's 31. The mirrored object
    // follows the Core's controller, and the Core saves an edit soon after.
    void anan100dReachesSixtyOneDbAndTheObjectFollowsTheController()
    {
        DaemonConfig cfg = testCoreConfig();
        cfg.sliceCount = 1;

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::Angelia, QStringLiteral("02:00:00:00:00:46"));
        QVERIFY(app.start(cfg));
        StepAttenuatorController* const controller = app.m_stepAttController.get();
        StepAttenuatorFacade* const facade = app.m_radioModel->stepAttFacade();
        QVERIFY(controller != nullptr);
        QVERIFY(app.m_radioModel->boardCapabilities().hasAlexFilters);
        QCOMPARE(controller->minAttenuation(), 0);
        QCOMPARE(controller->maxAttenuation(), 61);
        QVERIFY(controller->debouncedSaveEnabled());
        QCOMPARE(facade->controller(), controller);
        QCOMPARE(facade->minDb(), 0);
        QCOMPARE(facade->maxDb(), 61);

        facade->setAttenuationDb(45);
        QCOMPARE(controller->attenuatorDb(), 45);
        QCOMPARE(facade->attenuationDb(), 45);
        QVERIFY(facade->settleReason("attenuationDb").isEmpty());
        QVERIFY(controller->savePending());

        facade->setAttenuationDb(70);
        QCOMPARE(controller->attenuatorDb(), 61);
        QCOMPARE(facade->attenuationDb(), 61);
        QCOMPARE(facade->settleReason("attenuationDb"),
                 QStringLiteral("This radio's attenuator goes from 0 to 61 dB."));

        // A change made on the Core itself reaches the object too.
        controller->setAttenuation(12);
        QCOMPARE(facade->attenuationDb(), 12);

        app.stop();
    }

    // R-R3-46: a board without Alex keeps its own row. The Hermes Lite 2
    // runs -28..31 dB (its BoardCapabilities row, which carries the upstream
    // citation) and has no Adaptive auto-attenuate.
    void hermesLite2KeepsItsSignedRangeAndClassicAutoAttenuate()
    {
        DaemonConfig cfg = testCoreConfig();
        cfg.sliceCount = 1;

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite, QStringLiteral("02:00:00:00:00:47"));
        QVERIFY(app.start(cfg));
        StepAttenuatorController* const controller = app.m_stepAttController.get();
        StepAttenuatorFacade* const facade = app.m_radioModel->stepAttFacade();
        QCOMPARE(controller->minAttenuation(), -28);
        QCOMPARE(controller->maxAttenuation(), 31);
        QCOMPARE(facade->minDb(), -28);
        QCOMPARE(facade->maxDb(), 31);

        facade->setAutoAttMode(static_cast<int>(AutoAttMode::Adaptive));
        QCOMPARE(controller->autoAttMode(), AutoAttMode::Classic);
        QCOMPARE(facade->autoAttMode(), static_cast<int>(AutoAttMode::Classic));
        QCOMPARE(facade->settleReason("autoAttMode"),
                 QStringLiteral("This radio offers only Classic auto-attenuate."));

        // A Saturn stays at its row's 31 dB: it is not one of the Alex boards.
        app.stop();
        DaemonApp saturn;
        saturn.primeBoardForTest(HPSDRHW::Saturn, QStringLiteral("02:00:00:00:00:48"));
        QVERIFY(saturn.start(cfg));
        QCOMPARE(saturn.m_stepAttController->maxAttenuation(), 31);
        saturn.stop();
    }

    // R-R3-46: a band change on the Core restores that band's attenuation,
    // and the mirrored object follows it.
    void coreBandChangeRestoresTheBandsAttenuation()
    {
        DaemonConfig cfg = testCoreConfig();
        cfg.sliceCount = 1;

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::Angelia, QStringLiteral("02:00:00:00:00:49"));
        QVERIFY(app.start(cfg));
        StepAttenuatorFacade* const facade = app.m_radioModel->stepAttFacade();
        SliceModel* const txSlice = app.m_radioModel->txBoundSlice();
        QVERIFY(txSlice != nullptr);

        txSlice->setFrequency(7'100'000.0);
        facade->setAttenuationDb(20);
        txSlice->setFrequency(14'200'000.0);
        facade->setAttenuationDb(5);
        QCOMPARE(facade->attenuationDb(), 5);
        txSlice->setFrequency(7'150'000.0);
        QCOMPARE(facade->attenuationDb(), 20);
        txSlice->setFrequency(14'100'000.0);
        QCOMPARE(facade->attenuationDb(), 5);

        app.stop();
    }

    // R-R3-46 fix wave (Thetis parity; the cite is at RadioModel::
    // followReceiveSliceWithStepAttenuator): the Core's attenuator follows
    // slice A's receive band (Thetis rx1_band), not the transmit slice's. Transmitting on slice B at 20 m while
    // listening on slice A at 40 m keeps 40 m's attenuator.
    void coreAttenuatorFollowsSliceAInACrossBandSplit()
    {
        DaemonConfig cfg = testCoreConfig();
        cfg.sliceCount = 2;

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::Angelia, QStringLiteral("02:00:00:00:00:4A"));
        QVERIFY(app.start(cfg));
        RadioModel* const model = app.m_radioModel.get();
        StepAttenuatorFacade* const facade = model->stepAttFacade();
        StepAttenuatorController* const controller = app.m_stepAttController.get();
        SliceModel* const sliceA = model->sliceById(0);
        SliceModel* const sliceB = model->sliceById(1);
        QVERIFY(sliceA != nullptr && sliceB != nullptr);
        QCOMPARE(model->txBoundSlice(), sliceA);

        sliceA->setFrequency(14'200'000.0);
        facade->setAttenuationDb(5);          // 20 m's memory
        sliceA->setFrequency(7'100'000.0);
        facade->setAttenuationDb(20);         // 40 m's

        sliceB->setFrequency(14'250'000.0);
        QVERIFY(model->requestTxHandoffToSlice(1));
        QTRY_COMPARE(model->txBoundSlice(), sliceB);
        QCOMPARE(controller->currentBand(), Band::Band40m);
        QCOMPARE(facade->attenuationDb(), 20);
        QCOMPARE(controller->txBand(), Band::Band20m);

        sliceB->setFrequency(21'200'000.0);
        QCOMPARE(facade->attenuationDb(), 20);
        sliceA->setFrequency(14'100'000.0);
        QCOMPARE(facade->attenuationDb(), 5);

        app.stop();
    }

    void replacementSliceUsesStableIdForControllerWiring()
    {
        DaemonConfig cfg = testCoreConfig();
        cfg.sliceCount = 2;

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::Saturn);
        QVERIFY(app.start(cfg));

        RadioModel* const model = app.m_radioModel.get();
        StepAttenuatorController* const controller =
            app.m_stepAttController.get();
        QVERIFY(model != nullptr);
        QVERIFY(controller != nullptr);
        QVERIFY(model->sliceById(0) != nullptr);
        QVERIFY(model->sliceById(1) != nullptr);

        // Remove A, leaving B at list position 0, then recreate A. sliceAdded
        // carries stable id 0 while the replacement is list position 1. A
        // positional slices.at(0) lookup would silently wire B a second time.
        model->removeSlice(0);
        QVERIFY(model->sliceById(0) == nullptr);
        QCOMPARE(model->addSlice(), 0);
        SliceModel* const replacement = model->sliceById(0);
        QVERIFY(replacement != nullptr);
        QCOMPARE(model->slices().indexOf(replacement), 1);

        QVERIFY(model->requestTxHandoffToSlice(0));
        controller->setTxAttenuationForBand(Band::Band80m, 8);
        replacement->setFrequency(3'830'000.0);
        replacement->setDspMode(DSPMode::LSB);
        QCOMPARE(controller->attOnTxValue(), 8);
        QCOMPARE(controller->currentDspMode(), DSPMode::LSB);

        app.stop();
    }

    // ── Remote Daemon R2 Task 18 ─────────────────────────────────────────
    //
    // Until this landed, NOTHING in the tree constructed a StationServer:
    // grep for it across src/core/daemon, src/main.cpp and
    // src/core/CoreInit.cpp returned nothing, so nereusd never listened
    // and never printed the pairing banner. Task 18 step 4 is phrased as
    // daemon behaviour and its stated purpose is that without it the
    // acceptance run cannot authenticate, so it belongs here rather than
    // to the GUI-gating task.

    void remoteListenerIsNotStartedByDefault()
    {
        // iPhone app Task 12: a Core with no remote_port line listens on
        // 47910 on every interface, IPv4 and IPv6 (tst_daemon_config holds
        // the parsing); that is the one listener a test does not open for
        // real, so the address mapping is checked here instead.
        QCOMPARE(DaemonConfig::defaults().remotePort, 47910);
        QVERIFY(DaemonConfig::defaults().remoteBind.isEmpty());
        QCOMPARE(DaemonApp::listenerAddressFor(QString()), QHostAddress(QHostAddress::Any));
        QCOMPARE(DaemonApp::listenerAddressFor(QStringLiteral("127.0.0.1")),
                 QHostAddress(QHostAddress::LocalHost));
        QVERIFY(DaemonApp::listenerAddressFor(QStringLiteral("not-an-address")).isNull());

        // remote_port = 0 still opens no port at all.
        DaemonConfig cfg = testCoreConfig();
        QCOMPARE(cfg.remotePort, 0);

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(app.start(cfg));
        QVERIFY2(app.stationServer() == nullptr,
                 "remote_port = 0 must not bring up a network listener");
        app.stop();
    }

    void configuredRemotePortBringsUpAListener()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend, so a wss listener cannot bind. "
                  "The listener is wss-only by design (parent design section 10.5).");
        }

        DaemonConfig cfg = testCoreConfig();
        cfg.remotePort = 0;
        // Port 0 means "disabled" in the config, so an ephemeral port has
        // to be requested explicitly. Bind loopback and let the OS pick by
        // asking for a high port; a fixed port would collide with a
        // parallel ctest shard.
        QTcpServer probe;
        QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
        const quint16 freePort = probe.serverPort();
        probe.close();
        cfg.remotePort = static_cast<int>(freePort);
        cfg.remoteBind = QStringLiteral("127.0.0.1");
        // Synthetic limits verify the real config consumer, not board capacity.
        cfg.displayApplicationBytesPerSecond = 2000000;
        cfg.spectrumSampleUnitsPerSecond = 1000000;

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(app.start(cfg));

        StationServer* server = app.stationServer();
        QVERIFY2(server != nullptr, "remote_port was set but no StationServer exists");
        QVERIFY2(server->isListening(), qPrintable(server->lastError()));
        QCOMPARE(server->serverPort(), freePort);
        QCOMPARE(server->serverAddress(), QHostAddress(QHostAddress::LocalHost));
        QVERIFY(app.m_stationHost->m_stationAnnouncer);
        QVERIFY(!app.m_stationHost->m_stationAnnouncer->isActive()); // loopback never advertises LAN reachability.
        QSignalSpy listening(server, &StationServer::listeningChanged);
        server->close();
        QCOMPARE(listening.count(), 1);
        QCOMPARE(listening.first().first().toBool(), false);
        QVERIFY(server->serverAddress().isNull());
        QVERIFY(!app.m_stationHost->m_stationAnnouncer->isActive());
        QVERIFY(server->listen(QHostAddress::LocalHost, freePort));
        QCOMPARE(listening.count(), 2);
        QCOMPARE(listening.last().first().toBool(), true);
        QVERIFY(server->listen(QHostAddress::LocalHost, freePort));
        QCOMPARE(listening.count(), 2); // idempotent listen cannot restart announcements.
        QVERIFY(!app.m_stationHost->m_stationAnnouncer->isActive());
        QVERIFY(server->displayBudgetLimits());
        QCOMPARE(server->displayBudgetLimits()->applicationBytesPerSecond, quint64{2000000});
        QCOMPARE(server->displayBudgetLimits()->spectrumSampleUnitsPerSecond, quint64{1000000});
        QVERIFY(server->buildCapabilities().displayBudget);

        // Step 4's other half, as of iPhone app Task 12: the Core has its
        // identity key and its certificate pin. (That a new Core creates no
        // pairing token is tst_station_session's: this sandbox profile is
        // kept between runs, so it may hold one from an earlier run.)
        QVERIFY(server->stationIdentity().isValid());
        QVERIFY(!server->certificateFingerprint().isEmpty());

        // And it is torn down with the daemon rather than outliving the
        // RadioModel its mirror holds QPointers into.
        app.stop();
        QVERIFY(app.stationServer() == nullptr);
    }

    // iPhone app Task 16 (D36, R-IOS-16): the Core builds a schema-2
    // announcement and a Bonjour record from its identity, label, claimed
    // state and pairing window, and rebuilds both on a rename. A listener on
    // loopback only sends neither: nothing here reaches a network.
    void discoveryFollowsRenameAndNeverLeavesLoopback()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend, so a wss listener cannot bind.");
        }
        AppSettings& settings = AppSettings::instance();
        const QString labelKey = QString::fromLatin1(StationLabel::kSettingsKey);
        const QVariant storedLabel = settings.value(labelKey);
        const auto restore = qScopeGuard([&settings, labelKey, storedLabel] {
            if (storedLabel.isValid()) {
                settings.setValue(labelKey, storedLabel);
            } else {
                settings.remove(labelKey);
            }
            settings.save();
        });

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(app.start(listenerConfig()));
        StationServer* const server = app.stationServer();
        QVERIFY(server && server->isListening());
        StationDevicesFacade* const devices = server->devicesFacade();
        QVERIFY(devices);

        const StationLanAnnouncement built = app.stationAnnouncementForTest();
        QCOMPARE(built.schema, kStationLanAnnouncementSchema);
        QCOMPARE(built.controlPort, server->serverPort());
        QCOMPARE(built.identity, server->stationIdentity().fingerprint());
        QCOMPARE(built.claimed, devices->claimed());
        QCOMPARE(built.label, devices->stationLabel());
        QCOMPARE(built.pairing, DaemonApp::stationLanPairingFor(*server));
        // iPhone app plan Task 25 (R-IOS-16): the radio state, in the
        // announcement and in Bonjour's record, follows the Core's wait for
        // a radio choice.
        RadioModel* const model = app.radioModelForTest();
        QVERIFY(model);
        model->setStationRadioWaiting(QString());
        QCOMPARE(app.stationAnnouncementForTest().radio,
                 std::optional<StationLanRadio>(StationLanRadio::Offline));
        model->setStationRadioWaiting(QStringLiteral("Two radios are on this network."));
        QCOMPARE(app.stationAnnouncementForTest().radio,
                 std::optional<StationLanRadio>(StationLanRadio::Waiting));
        model->setStationRadioWaiting(QString());
        QCOMPARE(app.stationAnnouncementForTest().radio,
                 std::optional<StationLanRadio>(StationLanRadio::Offline));
        QString error;
        QVERIFY2(!encodeStationLanAnnouncement(built, &error).isEmpty(), qPrintable(error));
        // Loopback: neither the announcement nor Bonjour goes out.
        QVERIFY(!app.stationAnnouncedForTest());
        QVERIFY(!app.dnsSdAdvertisedForTest());
        QCOMPARE(app.dnsSdRecordForTest(), DnsSdRecord{});

        // station.rename: the next announcement carries the new label.
        const QString renamed = QStringLiteral("KG4VCF/task16-%1").arg(
            QCoreApplication::applicationPid() % 10000);
        QVERIFY(devices->rename(renamed).accepted);
        QCOMPARE(devices->stationLabel(), renamed);
        QCOMPARE(app.stationAnnouncementForTest().label, renamed);
        QCOMPARE(app.stationAnnouncementForTest().displayName(), renamed);
        QVERIFY(!app.stationAnnouncedForTest());
        QVERIFY(!app.dnsSdAdvertisedForTest());

        // The pairing window reads as the pairing field.
        const PairingWindow* window = server->pairingWindow();
        QVERIFY(window);
        const StationLanPairing pairing = DaemonApp::stationLanPairingFor(*server);
        switch (window->state()) {
        case PairingWindow::State::OpenUnclaimed:
            QCOMPARE(pairing, server->pairingLanClickAllowed() ? StationLanPairing::Click
                                                               : StationLanPairing::Code);
            break;
        case PairingWindow::State::OpenReopened:
            QCOMPARE(pairing, StationLanPairing::Code);
            break;
        case PairingWindow::State::ClosedClaimed:
        case PairingWindow::State::ClosedUnclaimed:
            QCOMPARE(pairing, StationLanPairing::Closed);
            break;
        }
        server->setPairingLanClickAllowed(false);
        QVERIFY(DaemonApp::stationLanPairingFor(*server) != StationLanPairing::Click);
        server->setPairingLanClickAllowed(true);

        app.stop();
        QCOMPARE(app.stationAnnouncementForTest(), StationLanAnnouncement{});
    }

    // R-R3-08/37/40 (final review M1): the display load governor's wiring.
    // With display_adaptive on and no limits configured, the Core holds the
    // computed ceiling, and only an app that knows the budget reason is put
    // in budget mode (no peer here, so none is); off, nothing is advertised
    // and no governor runs.
    void computedCeilingOnlyWithAdaptationOn()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend, so a wss listener cannot bind.");
        }
        {
            DaemonApp app;
            app.primeBoardForTest(HPSDRHW::HermesLite);
            QVERIFY(app.start(listenerConfig()));
            StationServer* const server = app.stationServer();
            QVERIFY(server);
            QCOMPARE(server->configuredDisplayBudgetLimits(),
                     std::optional<DisplayBudgetLimits>(DisplayLoadGovernor::computedCeiling()));
            QVERIFY(!server->displayBudgetLimits()); // no minor-11 app attached
            QVERIFY(!server->buildCapabilities().displayBudget);
            QVERIFY(app.m_stationHost->m_displayGovernor);
            QVERIFY(app.m_stationHost->m_displayGovernorTimer);
            QVERIFY(!app.m_stationHost->m_displayGovernorTimer->isActive()); // no media session yet
            app.stop();
        }
        {
            DaemonConfig cfg = listenerConfig();
            cfg.displayAdaptive = false;
            DaemonApp app;
            app.primeBoardForTest(HPSDRHW::HermesLite);
            QVERIFY(app.start(cfg));
            StationServer* const server = app.stationServer();
            QVERIFY(server);
            QVERIFY(!server->configuredDisplayBudgetLimits());
            QVERIFY(!server->displayBudgetLimits());
            QVERIFY(!app.m_stationHost->m_displayGovernor);
            QVERIFY(!app.m_stationHost->m_displayGovernorTimer);
            app.stop();
        }
    }

    // The governor measures busy receivers only (idle is not a measurement),
    // steps the budget through StationServer, and the session's end puts the
    // ceiling back. Uses the configured pair as the ceiling so the budget is
    // in force without an attached app.
    void governorStepsOnMeasuredLoadAndResetsAtSessionEnd()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend, so a wss listener cannot bind.");
        }
        const DisplayBudgetLimits ceiling = DisplayLoadGovernor::computedCeiling();
        DaemonConfig cfg = listenerConfig();
        cfg.displayApplicationBytesPerSecond = ceiling.applicationBytesPerSecond;
        cfg.spectrumSampleUnitsPerSecond = ceiling.spectrumSampleUnitsPerSecond;
        qint64 now = 0;
        bool idle = true;
        const DisplayBudgetCharge pan = spectrumDisplayCost(1024, 30, false)->charge;
        const DisplayBudgetCharge accepted = *sumDisplayCharges({pan, pan, pan, pan});
        DaemonApp app;
        app.setDisplayLoadSourcesForTest(
            [&idle] {
                DisplayLoadInputs inputs;
                ReceiverDspLoad load;
                load.load = 0.95;
                load.idle = idle;
                inputs.receivers.append({0, load});
                return inputs;
            },
            [&now] { return now; }, [&accepted] { return accepted; });
        app.primeBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(app.start(cfg));
        StationServer* const server = app.stationServer();
        QVERIFY(server);
        QCOMPARE(server->displayBudgetLimits()->generation, quint32{1});

        // Idle receivers: nothing measured, nothing changes.
        for (now = 0; now <= 30'000; now += 500) {
            app.m_stationHost->evaluateDisplayLoad();
        }
        QCOMPARE(server->displayBudgetLimits()->generation, quint32{1});
        QCOMPARE(server->displayBudgetReason(), DisplayBudgetReason::None);

        // Busy: one step after the busy hold, in force at StationServer and
        // in the governor.
        idle = false;
        const qint64 start = now;
        for (; now < start + DisplayLoadGovernor::kBusyHoldMs; now += 500) {
            app.m_stationHost->evaluateDisplayLoad();
            QCOMPARE(server->displayBudgetLimits()->generation, quint32{1});
        }
        app.m_stationHost->evaluateDisplayLoad();
        QCOMPARE(server->displayBudgetLimits()->generation, quint32{2});
        QCOMPARE(server->displayBudgetReason(), DisplayBudgetReason::CoreBusy);
        QVERIFY(server->displayBudgetLimits()->spectrumSampleUnitsPerSecond
                < accepted.spectrumSampleUnitsPerSecond);
        QCOMPARE(app.m_stationHost->m_displayGovernor->steps(), 1);

        // The media session ends: back to the ceiling, reason cleared.
        emit server->mediaSessionEnded(1);
        QCOMPARE(server->displayBudgetLimits()->generation, quint32{3});
        QCOMPARE(server->displayBudgetLimits()->spectrumSampleUnitsPerSecond,
                 ceiling.spectrumSampleUnitsPerSecond);
        QCOMPARE(server->displayBudgetReason(), DisplayBudgetReason::None);
        QCOMPARE(app.m_stationHost->m_displayGovernor->steps(), 0);
        app.stop();
    }

    // Final review M5: a step StationServer refuses is not in force in the
    // governor, and the next proposal follows the published generation.
    void aRefusedStepIsNotTakenAndTheNextFollowsThePublishedGeneration()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend, so a wss listener cannot bind.");
        }
        const DisplayBudgetLimits ceiling = DisplayLoadGovernor::computedCeiling();
        DaemonConfig cfg = listenerConfig();
        cfg.displayApplicationBytesPerSecond = ceiling.applicationBytesPerSecond;
        cfg.spectrumSampleUnitsPerSecond = ceiling.spectrumSampleUnitsPerSecond;
        qint64 now = 0;
        const DisplayBudgetCharge pan = spectrumDisplayCost(1024, 30, false)->charge;
        const DisplayBudgetCharge accepted = *sumDisplayCharges({pan, pan, pan, pan});
        DaemonApp app;
        app.setDisplayLoadSourcesForTest(
            [] {
                DisplayLoadInputs inputs;
                ReceiverDspLoad load;
                load.load = 0.95;
                inputs.receivers.append({0, load});
                return inputs;
            },
            [&now] { return now; }, [&accepted] { return accepted; });
        app.primeBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(app.start(cfg));
        StationServer* const server = app.stationServer();
        QVERIFY(server);
        // Something else published generation 10 behind the governor's back.
        QVERIFY(server->setDisplayBudgetLimits(
            DisplayBudgetLimits{ceiling.applicationBytesPerSecond,
                                ceiling.spectrumSampleUnitsPerSecond, 10}));
        for (now = 0; now <= DisplayLoadGovernor::kBusyHoldMs; now += 500) {
            app.m_stationHost->evaluateDisplayLoad();
        }
        // The proposal at generation 2 was refused: nothing in force.
        QCOMPARE(server->displayBudgetLimits()->generation, quint32{10});
        QCOMPARE(app.m_stationHost->m_displayGovernor->steps(), 0);
        QVERIFY(!app.m_stationHost->m_displayGovernor->settling());
        // Busy on: the next proposal is generation 11 and is taken.
        for (; now <= 2 * DisplayLoadGovernor::kBusyHoldMs + 500; now += 500) {
            app.m_stationHost->evaluateDisplayLoad();
        }
        QCOMPARE(server->displayBudgetLimits()->generation, quint32{11});
        QCOMPARE(server->displayBudgetReason(), DisplayBudgetReason::CoreBusy);
        QCOMPARE(app.m_stationHost->m_displayGovernor->steps(), 1);
        QCOMPARE(app.m_stationHost->m_displayGovernor->limits(), *server->displayBudgetLimits());
        app.stop();
    }

    void anyAddressBindTakesIpv4AndIpv6()
    {
        // remote_bind = "::" is how an operator asks for every address. Qt
        // binds a parsed "::" IPv6-only, which left a LAN's IPv4 clients
        // refused (seen on the Pi 4 bench, 2026-09-23). One listener must
        // take both families.
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend, so a wss listener cannot bind. "
                  "The listener is wss-only by design (parent design section 10.5).");
        }
        QTcpServer ipv6Probe;
        const bool haveIpv6 = ipv6Probe.listen(QHostAddress::LocalHostIPv6, 0);
        ipv6Probe.close();

        QTcpServer probe;
        QVERIFY(probe.listen(QHostAddress::Any, 0));
        const quint16 freePort = probe.serverPort();
        probe.close();

        DaemonConfig cfg = DaemonConfig::defaults();
        cfg.remotePort = static_cast<int>(freePort);
        cfg.remoteBind = QStringLiteral("::");
        // Lane B (iPhone app Task 17): no status page here, which would
        // otherwise share TCP 47911 with every other test.
        cfg.statusPage = false;

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(app.start(cfg));
        StationServer* server = app.stationServer();
        QVERIFY2(server != nullptr, "remote_port was set but no StationServer exists");
        QVERIFY2(server->isListening(), qPrintable(server->lastError()));

        QTcpSocket ipv4;
        ipv4.connectToHost(QHostAddress::LocalHost, freePort);
        QVERIFY2(ipv4.waitForConnected(5000),
                 qPrintable(QStringLiteral("IPv4 client refused by the \"::\" listener: %1")
                                .arg(ipv4.errorString())));
        ipv4.abort();

        if (!haveIpv6) {
            app.stop();
            QSKIP("This host has no IPv6 loopback; the IPv4 half passed.");
        }
        QCOMPARE(server->serverAddress(), QHostAddress(QHostAddress::Any));
        QTcpSocket ipv6;
        ipv6.connectToHost(QHostAddress::LocalHostIPv6, freePort);
        QVERIFY2(ipv6.waitForConnected(5000),
                 qPrintable(QStringLiteral("IPv6 client refused by the \"::\" listener: %1")
                                .arg(ipv6.errorString())));
        ipv6.abort();
        app.stop();
    }

    void occupiedRemotePortRecoversWithoutRecreatingStationState()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend, so the wss retry cannot bind.");
        }

        QTcpServer blocker;
        QVERIFY(blocker.listen(QHostAddress::LocalHost, 0));
        const quint16 port = blocker.serverPort();

        DaemonConfig cfg = testCoreConfig();
        cfg.remoteBind = QStringLiteral("127.0.0.1");
        cfg.remotePort = static_cast<int>(port);
        cfg.sliceCount = 2;

        DaemonApp app;
        app.setStationListenRetryIntervalsForTest(10, 30);
        app.primeBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(app.start(cfg));

        StationServer* const server = app.stationServer();
        RadioModel* const model = app.m_radioModel.get();
        DaemonMediaHub* const media = app.m_stationHost->m_mediaHub.get();
        QVERIFY(server != nullptr);
        QVERIFY(model != nullptr);
        QVERIFY(media != nullptr);
        QVERIFY(!server->isListening());
        QVERIFY(app.stationListenerRetryPending());
        QCOMPARE(app.stationListenAttemptCountForTest(), 1);

        const QByteArray identity = server->stationIdentity().fingerprint();
        QVERIFY(!identity.isEmpty());
        const QList<SliceModel*> slices = model->slices();

        // Keep the port occupied through several retries. The compressed
        // schedule must progress 10 -> 20 -> 30 ms and remain capped there;
        // no production-duration sleep is needed to observe the backoff.
        QTRY_VERIFY_WITH_TIMEOUT(app.stationListenAttemptCountForTest() >= 3, 500);
        QCOMPARE(app.m_stationHost->m_stationListenRetryTimer->interval(), 30);
        QCOMPARE(app.m_stationHost->m_stationListenNextDelayMs, 30);
        blocker.close();

        QTRY_VERIFY_WITH_TIMEOUT(app.stationListenerReady(), 1000);
        QCOMPARE(app.stationServer(), server);
        QCOMPARE(app.m_radioModel.get(), model);
        QCOMPARE(app.m_stationHost->m_mediaHub.get(), media);
        QCOMPARE(server->stationIdentity().fingerprint(), identity);
        QVERIFY(model->slices() == slices);
        QCOMPARE(server->serverPort(), port);
        QVERIFY(!app.stationListenerRetryPending());
        QVERIFY(app.stationListenAttemptCountForTest() >= 4);

        app.stop();
    }

    void stopDuringListenerBackoffCannotBindLater()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend, so the wss retry cannot bind.");
        }

        QTcpServer blocker;
        QVERIFY(blocker.listen(QHostAddress::LocalHost, 0));
        const quint16 port = blocker.serverPort();

        DaemonConfig cfg = testCoreConfig();
        cfg.remoteBind = QStringLiteral("127.0.0.1");
        cfg.remotePort = static_cast<int>(port);

        DaemonApp app;
        app.setStationListenRetryIntervalsForTest(10, 20);
        app.primeBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(app.start(cfg));
        QVERIFY(app.stationListenerRetryPending());

        app.stop();
        QVERIFY(!app.stationListenerRetryPending());
        QVERIFY(app.stationServer() == nullptr);
        blocker.close();

        // Let several compressed backoff periods pass. This waits for the
        // absence of a late callback; listener recovery itself is always
        // awaited with QTRY in the positive tests.
        QTest::qWait(80);
        QVERIFY(app.stationServer() == nullptr);
        QTcpServer claimant;
        QVERIFY(claimant.listen(QHostAddress::LocalHost, port));
    }

    void restartDiscardsOldListenerRetryTarget()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend, so the wss retry cannot bind.");
        }

        QTcpServer oldBlocker;
        QTcpServer newBlocker;
        QVERIFY(oldBlocker.listen(QHostAddress::LocalHost, 0));
        QVERIFY(newBlocker.listen(QHostAddress::LocalHost, 0));
        const quint16 oldPort = oldBlocker.serverPort();
        const quint16 newPort = newBlocker.serverPort();
        QVERIFY(oldPort != newPort);

        DaemonConfig cfg = testCoreConfig();
        cfg.remoteBind = QStringLiteral("127.0.0.1");
        cfg.remotePort = static_cast<int>(oldPort);

        DaemonApp app;
        app.setStationListenRetryIntervalsForTest(10, 30);
        app.primeBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(app.start(cfg));
        QVERIFY(app.stationListenerRetryPending());
        app.stop();

        cfg.remotePort = static_cast<int>(newPort);
        QVERIFY(app.start(cfg));
        QVERIFY(app.stationListenerRetryPending());
        QCOMPARE(app.stationListenAttemptCountForTest(), 1);
        newBlocker.close();

        QTRY_VERIFY_WITH_TIMEOUT(app.stationListenerReady(), 1000);
        QCOMPARE(app.stationServer()->serverPort(), newPort);
        QVERIFY(oldBlocker.isListening());
        QCOMPARE(oldBlocker.serverPort(), oldPort);

        app.stop();
    }

    void disabledAndInvalidRemoteConfigNeverScheduleListenerRetry()
    {
        DaemonApp app;
        app.setStationListenRetryIntervalsForTest(10, 30);
        app.primeBoardForTest(HPSDRHW::HermesLite);

        DaemonConfig cfg = testCoreConfig();
        cfg.remotePort = 0;
        QVERIFY(app.start(cfg));
        QVERIFY(app.stationServer() == nullptr);
        QVERIFY(!app.stationListenerReady());
        QVERIFY(!app.stationListenerRetryPending());
        QCOMPARE(app.stationListenAttemptCountForTest(), 0);
        app.stop();

        cfg.remoteBind = QStringLiteral("127.0.0.1");
        cfg.remotePort = -1;
        QVERIFY(app.start(cfg));
        QVERIFY(app.stationServer() == nullptr);
        QVERIFY(!app.stationListenerRetryPending());
        QCOMPARE(app.stationListenAttemptCountForTest(), 0);
        app.stop();

        cfg.remotePort = 65536;
        QVERIFY(app.start(cfg));
        QVERIFY(app.stationServer() == nullptr);
        QVERIFY(!app.stationListenerRetryPending());
        QCOMPARE(app.stationListenAttemptCountForTest(), 0);
        app.stop();

        cfg.remotePort = 4711;
        cfg.remoteBind = QStringLiteral("not-an-address");
        QVERIFY(app.start(cfg));
        QVERIFY(app.stationServer() == nullptr);
        QVERIFY(!app.stationListenerReady());
        QVERIFY(!app.stationListenerRetryPending());
        QCOMPARE(app.stationListenAttemptCountForTest(), 0);
        app.stop();
    }

    void invalidRemoteBindIsLoggedRatherThanFatal()
    {
        DaemonConfig cfg = testCoreConfig();
        cfg.remotePort = 4711;
        cfg.remoteBind = QStringLiteral("not-an-address");

        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);
        // A daemon that still demodulates locally is more useful than one
        // that refuses to boot over a mistyped config line.
        QVERIFY(app.start(cfg));
        QVERIFY(app.stationServer() == nullptr);
        QCOMPARE(app.sliceCount(), cfg.sliceCount);
        app.stop();
    }
};

QTEST_MAIN(TstDaemonApp)
#include "tst_daemon_app.moc"//   2026-09-24: Part C fix wave (R1-I2): a reopened pairing window
//               lasts 10 minutes, five burned codes in a row close any window,
//               and reopening starts afresh. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.

