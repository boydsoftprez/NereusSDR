// =================================================================
// tests/tst_audio_engine_native_routing.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  AudioEngine with engine backends,
// the device catalogue and the stream supervisor (native audio plan
// Task 7; R-AUD-02, R-AUD-06, R-AUD-15, R-AUD-34): the engine a role
// opens on follows its saved Engine, "(platform default)" uses the
// default engine, the role status reaches roleStatusChanged, the delay
// readout is per role, Rescan closes and reopens only the roles on older
// drivers, a remote window's speakers open through the same backends,
// and the PC mic role follows the capture helper.
//
// Fake engines only (FakeAudioEngineBackend); the PC mic is the scripted
// fake helper (this binary re-run with --fake-capture-child).  No device
// is touched.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 7 (R-AUD-02, R-AUD-06, R-AUD-15,
//               R-AUD-34). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-09: native audio plan Task 8 (R-AUD-01, R-AUD-02): the Mac's
//               registry is Core Audio only. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 8 (R-AUD-18): the speakers
//               workgroup generation and the DSP thread's rejoin. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: early-review fix wave (R-AUD-02, R-AUD-06, R-AUD-08,
//               R-AUD-16): a failed open leaves the playing bus playing,
//               reopening the same device closes first, Windows shared
//               asks for the smallest period, Rescan waits out a large
//               callback, and the first mic demand opens the helper once.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QStandardPaths>

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/audio/AudioBackendRegistry.h"
#include "core/audio/CaptureSupervisor.h"
#include "core/audio/IAudioDeviceCatalog.h"
#include "core/audio/PortAudioBackend.h"
#include "core/audio/RealtimeAudioPriority.h"
#include "models/RxDspWorker.h"

#include "fakes/FakeAudioEngineBackend.h"
#include "fakes/FakeCaptureChild.h"

#include <cstring>
#include <memory>
#include <optional>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr int kWaitMs = 5000;

const QString kCoreAudioApi = QStringLiteral("Core Audio");

// An older-drivers device's id, as PortAudioBackend makes it.
QString paId(const QString& name)
{
    return portAudioDeviceId(kCoreAudioApi, name);
}

AudioDeviceInfo deviceInfo(AudioBackendId backend, AudioDeviceDirection direction,
                           const QString& id, const QString& name,
                           const QString& hostApi = {})
{
    AudioDeviceInfo info;
    info.backend = backend;
    info.direction = direction;
    info.id = id;
    info.name = name;
    info.hostApi = hostApi;
    return info;
}

AudioDeviceConfig savedChoice(AudioEngineKind engine, const QString& id, const QString& name,
                              const QString& hostApi = {})
{
    AudioDeviceConfig cfg;
    cfg.engine = engine;
    cfg.deviceId = id;
    cfg.deviceName = name;
    cfg.driverApi = hostApi;
    return cfg;
}

CaptureSupervisor::Options fakeHelper(const QString& scenario)
{
    CaptureSupervisor::Options options;
    options.program = QCoreApplication::applicationFilePath();
    options.arguments = {QStringLiteral("--fake-capture-child"), scenario};
    return options;
}

// A native engine (CoreAudio here) and the older drivers (PortAudio).
// The engine reads the saved choices when it is built, so build() comes
// after the test saves them.
struct Rig {
    std::shared_ptr<FakeAudioEngineBackend> native =
        std::make_shared<FakeAudioEngineBackend>(AudioBackendId::CoreAudio);
    std::shared_ptr<FakeAudioEngineBackend> older =
        std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PortAudio);
    std::unique_ptr<AudioEngine> engine;

    Rig()
    {
        native->setDevices({deviceInfo(AudioBackendId::CoreAudio, AudioDeviceDirection::Output,
                                       QStringLiteral("desk-uid"), QStringLiteral("Desk speakers")),
                            deviceInfo(AudioBackendId::CoreAudio, AudioDeviceDirection::Output,
                                       QStringLiteral("built-in-uid"),
                                       QStringLiteral("Built-in speakers"))});
        native->setDefault(AudioDeviceDirection::Output, QStringLiteral("built-in-uid"));
        older->setTakesStereoMix(false);
        older->setDevices(
            {deviceInfo(AudioBackendId::PortAudio, AudioDeviceDirection::Output,
                        paId(QStringLiteral("Desk headphones")), QStringLiteral("Desk headphones"),
                        kCoreAudioApi),
             deviceInfo(AudioBackendId::PortAudio, AudioDeviceDirection::Output,
                        paId(QStringLiteral("Built-in speakers")), QStringLiteral("Built-in speakers"),
                        kCoreAudioApi),
             deviceInfo(AudioBackendId::PortAudio, AudioDeviceDirection::Input,
                        paId(QStringLiteral("Desk mic")), QStringLiteral("Desk mic"), kCoreAudioApi)});
        older->setDefault(AudioDeviceDirection::Output, paId(QStringLiteral("Built-in speakers")));
        older->setDefault(AudioDeviceDirection::Input, paId(QStringLiteral("Desk mic")));
    }

    // VAX outputs are not under test here: none is published.
    void build(std::optional<CaptureSupervisor::Options> helper = std::nullopt)
    {
        engine = std::make_unique<AudioEngine>();
        engine->setVaxOutputsAllowed(false);
        if (helper) {
            engine->setCaptureSupervisorOptionsForTest(*helper);
        }
        engine->setAudioBackendsForTest({native, older});
    }
};

} // namespace

class TstAudioEngineNativeRouting : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(QStandardPaths::isTestModeEnabled());
    }

    void init()
    {
        AppSettings::instance().clear();
    }

    // R-AUD-02: the first native engine registered and running, else
    // PortAudio; ASIO is never the default.
    void defaultEngineIsTheFirstRunningNativeEngine()
    {
        auto older = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PortAudio);
        auto asio = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::Asio);
        auto wasapi = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::Wasapi);
        auto pipewire = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PipeWire);
        QCOMPARE(defaultAudioEngine({older}), AudioEngineKind::PortAudio);
        QCOMPARE(defaultAudioEngine({older, asio}), AudioEngineKind::PortAudio);
        QCOMPARE(defaultAudioEngine({asio, wasapi, older}), AudioEngineKind::WindowsShared);
        pipewire->setRunning(false);
        QCOMPARE(defaultAudioEngine({pipewire, older}), AudioEngineKind::PortAudio);
        pipewire->setRunning(true);
        QCOMPARE(defaultAudioEngine({pipewire, older}), AudioEngineKind::PipeWire);

        // R-AUD-01: the Mac registers Core Audio only; the other systems
        // build the older drivers last, until their engine tasks add theirs.
        const auto system = makeSystemAudioBackends(AudioBackendContext{});
        QVERIFY(!system.empty());
#ifdef Q_OS_MAC
        QCOMPARE(system.size(), std::size_t(1));
        QCOMPARE(system.front()->id(), AudioBackendId::CoreAudio);
        QCOMPARE(defaultAudioEngine(system), AudioEngineKind::CoreAudio);
#else
        QCOMPARE(system.back()->id(), AudioBackendId::PortAudio);
#endif
    }

    // R-AUD-18: every speakers open on a device bumps the workgroup
    // generation and stores the device's workgroup id: the saved device,
    // the fall-back to the default, and the return.
    void speakersOpenBumpsTheWorkgroupGeneration()
    {
        Rig rig;
        rig.native->setWorkgroupDevice(QStringLiteral("desk-uid"), 71);
        rig.native->setWorkgroupDevice(QString(), 72);
        savedChoice(AudioEngineKind::CoreAudio, QStringLiteral("desk-uid"),
                    QStringLiteral("Desk speakers"))
            .saveToSettings(QStringLiteral("audio/Speakers"));
        rig.build();
        QCOMPARE(rig.engine->speakersWorkgroupGeneration(), std::uint32_t(0));
        QCOMPARE(rig.engine->speakersWorkgroupDevice(), std::uint32_t(0));
        rig.engine->start();
        QCOMPARE(rig.native->outputRequests().size(), std::size_t(1));
        QCOMPARE(rig.engine->speakersWorkgroupGeneration(), std::uint32_t(1));
        QCOMPARE(rig.engine->speakersWorkgroupDevice(), std::uint32_t(71));

        // The device goes away: the speakers fall back to the default.
        FakeMatcherAudioBus* desk = rig.native->lastOutput();
        QVERIFY(desk != nullptr);
        AudioStreamEvent lost;
        lost.kind = AudioStreamEvent::Kind::DeviceLost;
        desk->emitEventForTest(lost);
        QTRY_COMPARE_WITH_TIMEOUT(rig.engine->roleStatus(AudioRole::Speakers).state,
                                  AudioRoleState::PlayingOnDefault, kWaitMs);
        QCOMPARE(rig.native->outputRequests().size(), std::size_t(2));
        QCOMPARE(rig.engine->speakersWorkgroupGeneration(), std::uint32_t(2));
        QCOMPARE(rig.engine->speakersWorkgroupDevice(), std::uint32_t(72));

        // The chosen device is chosen again: the speakers return to it.
        rig.engine->setSpeakersConfig(savedChoice(AudioEngineKind::CoreAudio,
                                                  QStringLiteral("built-in-uid"),
                                                  QStringLiteral("Built-in speakers")));
        rig.engine->setSpeakersConfig(savedChoice(AudioEngineKind::CoreAudio,
                                                  QStringLiteral("desk-uid"),
                                                  QStringLiteral("Desk speakers")));
        QTRY_COMPARE_WITH_TIMEOUT(rig.engine->roleStatus(AudioRole::Speakers).state,
                                  AudioRoleState::Playing, kWaitMs);
        QCOMPARE(rig.native->outputRequests().back().deviceId, QStringLiteral("desk-uid"));
        const auto opens = static_cast<std::uint32_t>(rig.native->outputRequests().size());
        QCOMPARE(rig.engine->speakersWorkgroupGeneration(), opens);
        QCOMPARE(rig.engine->speakersWorkgroupDevice(), std::uint32_t(71));
        rig.engine->stop();
    }

    // R-AUD-18: the DSP thread rejoins once per change of the generation,
    // with the stored device, and never on a batch with no change.
    void dspThreadRejoinsOncePerChange()
    {
        Rig rig;
        rig.native->setWorkgroupDevice(QStringLiteral("desk-uid"), 71);
        rig.native->setWorkgroupDevice(QStringLiteral("built-in-uid"), 73);
        savedChoice(AudioEngineKind::CoreAudio, QStringLiteral("desk-uid"),
                    QStringLiteral("Desk speakers"))
            .saveToSettings(QStringLiteral("audio/Speakers"));
        rig.build();

        RxDspWorker worker;
        worker.setEngines(nullptr, rig.engine.get());
        std::vector<std::uint32_t> rejoins;
        worker.setWorkgroupRejoinForTest([&rejoins](AudioPriorityToken*, std::uint32_t device) {
            rejoins.push_back(device);
            return true;
        });
        const QVector<float> empty;

        // No speakers open yet: nothing to follow.
        worker.processIqBatch(0, empty);
        QVERIFY(rejoins.empty());

        rig.engine->start();
        worker.processIqBatch(0, empty);
        worker.processIqBatch(0, empty);
        QCOMPARE(rejoins, std::vector<std::uint32_t>({71}));

        rig.engine->setSpeakersConfig(savedChoice(AudioEngineKind::CoreAudio,
                                                  QStringLiteral("built-in-uid"),
                                                  QStringLiteral("Built-in speakers")));
        QCOMPARE(rig.native->outputRequests().back().deviceId, QStringLiteral("built-in-uid"));
        worker.processIqBatch(0, empty);
        worker.processIqBatch(0, empty);
        QCOMPARE(rejoins, std::vector<std::uint32_t>({71, 73}));
        rig.engine->stop();
    }

    // The saved Engine decides which backend opens the role.
    void roleOpensOnItsSavedEngine()
    {
        Rig rig;
        savedChoice(AudioEngineKind::CoreAudio, QStringLiteral("desk-uid"),
                    QStringLiteral("Desk speakers"))
            .saveToSettings(QStringLiteral("audio/Speakers"));
        rig.build();
        rig.engine->start();

        QCOMPARE(rig.native->outputRequests().size(), std::size_t(1));
        QCOMPARE(rig.native->outputRequests().front().deviceId, QStringLiteral("desk-uid"));
        QVERIFY(rig.older->outputRequests().empty());
        const AudioRoleStatus status = rig.engine->roleStatus(AudioRole::Speakers);
        QCOMPARE(status.state, AudioRoleState::Playing);
        QCOMPARE(status.playingName, QStringLiteral("Desk speakers"));

        // A Setup change to an older-drivers device opens it on PortAudio,
        // on its host API.
        rig.engine->setSpeakersConfig(savedChoice(AudioEngineKind::PortAudio,
                                                  paId(QStringLiteral("Built-in speakers")),
                                                  QStringLiteral("Built-in speakers"),
                                                  kCoreAudioApi));
        QCOMPARE(rig.older->outputRequests().size(), std::size_t(1));
        QCOMPARE(rig.older->outputRequests().front().deviceId, paId(QStringLiteral("Built-in speakers")));
        QCOMPARE(rig.older->outputRequests().front().hostApi, kCoreAudioApi);
        QCOMPARE(rig.native->outputRequests().size(), std::size_t(1));
        QCOMPARE(rig.engine->roleStatus(AudioRole::Speakers).state, AudioRoleState::Playing);
        rig.engine->stop();
    }

    // R-AUD-02: speakers on "(platform default)" with no saved engine
    // play on the default engine's system default; with that engine not
    // running, PortAudio is the default engine.
    void platformDefaultUsesTheDefaultEngine()
    {
        {
            Rig rig;
            rig.build();
            rig.engine->start();
            QCOMPARE(rig.engine->defaultEngine(), AudioEngineKind::CoreAudio);
            QCOMPARE(rig.native->outputRequests().size(), std::size_t(1));
            QVERIFY(rig.native->outputRequests().front().deviceId.isEmpty());
            QVERIFY(rig.older->outputRequests().empty());
            const AudioRoleStatus status = rig.engine->roleStatus(AudioRole::Speakers);
            QCOMPARE(status.state, AudioRoleState::Playing);
            QCOMPARE(status.playingName, QStringLiteral("Built-in speakers"));
            QCOMPARE(status.chosen.engine, std::optional<AudioEngineKind>(AudioEngineKind::CoreAudio));
            // VAX on this computer keeps its own buses: the VAX roles are
            // the supervisor's on Windows only.
#if !defined(Q_OS_WIN)
            QCOMPARE(rig.engine->roleStatus(AudioRole::Vax1).state, AudioRoleState::Off);
#endif
            rig.engine->stop();
        }
        {
            Rig rig;
            rig.native->setRunning(false);
            rig.build();
            rig.engine->start();
            QCOMPARE(rig.engine->defaultEngine(), AudioEngineKind::PortAudio);
            QVERIFY(rig.native->outputRequests().empty());
            QCOMPARE(rig.older->outputRequests().size(), std::size_t(1));
            QVERIFY(rig.older->outputRequests().front().deviceId.isEmpty());
            rig.engine->stop();
        }
    }

    // The supervisor's status reaches roleStatusChanged, and a stream event
    // from the bus's device thread reaches the supervisor on this thread.
    void roleStatusFollowsTheStream()
    {
        Rig rig;
        savedChoice(AudioEngineKind::CoreAudio, QStringLiteral("desk-uid"),
                    QStringLiteral("Desk speakers"))
            .saveToSettings(QStringLiteral("audio/Speakers"));
        rig.build();
        QSignalSpy statuses(rig.engine.get(), &AudioEngine::roleStatusChanged);
        rig.engine->start();
        bool sawPlaying = false;
        for (const QList<QVariant>& args : statuses) {
            if (args.at(0).value<AudioRole>() == AudioRole::Speakers
                && args.at(1).value<AudioRoleStatus>().state == AudioRoleState::Playing) {
                sawPlaying = true;
            }
        }
        QVERIFY(sawPlaying);

        FakeMatcherAudioBus* desk = rig.native->lastOutput();
        QVERIFY(desk != nullptr);
        AudioStreamEvent lost;
        lost.kind = AudioStreamEvent::Kind::DeviceLost;
        desk->emitEventForTest(lost);
        // Posted, never handled inside the device callback.
        QCOMPARE(rig.engine->roleStatus(AudioRole::Speakers).state, AudioRoleState::Playing);
        QTRY_COMPARE_WITH_TIMEOUT(rig.engine->roleStatus(AudioRole::Speakers).state,
                                  AudioRoleState::PlayingOnDefault, kWaitMs);
        QCOMPARE(rig.engine->roleStatus(AudioRole::Speakers).reason, AudioRoleReason::NotConnected);
        QCOMPARE(rig.native->outputRequests().size(), std::size_t(2));
        QVERIFY(rig.native->outputRequests().back().deviceId.isEmpty());
        bool sawFallback = false;
        for (const QList<QVariant>& args : statuses) {
            if (args.at(0).value<AudioRole>() == AudioRole::Speakers
                && args.at(1).value<AudioRoleStatus>().state == AudioRoleState::PlayingOnDefault) {
                sawFallback = true;
            }
        }
        QVERIFY(sawFallback);
        rig.engine->stop();
    }

    // R-AUD-15: the delay readout is each role's own bus.
    void delayPartsArePerRole()
    {
        Rig rig;
        savedChoice(AudioEngineKind::PortAudio, paId(QStringLiteral("Desk headphones")),
                    QStringLiteral("Desk headphones"), kCoreAudioApi)
            .saveToSettings(QStringLiteral("audio/Headphones"));
        rig.build();
        rig.engine->start();
        QVERIFY(rig.engine->delayParts(AudioRole::Speakers).matcherFillMs >= 0.0);
        QCOMPARE(rig.engine->delayParts(AudioRole::Headphones).matcherFillMs, -1.0);  // closed
        QCOMPARE(rig.engine->delayParts(AudioRole::TxInput).matcherFillMs, -1.0);

        rig.engine->setHeadphonesEnabled(true);
        QVERIFY(rig.engine->headphonesAvailable());
        QCOMPARE(rig.engine->roleStatus(AudioRole::Headphones).state, AudioRoleState::Playing);
        // Open, on a bus with no clock matcher.
        QCOMPARE(rig.engine->delayParts(AudioRole::Headphones).matcherFillMs, -1.0);
        QVERIFY(rig.engine->delayParts(AudioRole::Speakers).matcherFillMs >= 0.0);

        rig.engine->setHeadphonesEnabled(false);
        QVERIFY(!rig.engine->headphonesAvailable());
        QCOMPARE(rig.engine->roleStatus(AudioRole::Headphones).state, AudioRoleState::Off);
        rig.engine->stop();
    }

    // R-AUD-06: Rescan fades and closes the roles on older drivers, has
    // PortAudio list again and reopens them; a role on a native engine is
    // never closed.
    void rescanTouchesOnlyOlderDriverRoles()
    {
        Rig rig;
        savedChoice(AudioEngineKind::CoreAudio, QStringLiteral("desk-uid"),
                    QStringLiteral("Desk speakers"))
            .saveToSettings(QStringLiteral("audio/Speakers"));
        savedChoice(AudioEngineKind::PortAudio, paId(QStringLiteral("Desk headphones")),
                    QStringLiteral("Desk headphones"), kCoreAudioApi)
            .saveToSettings(QStringLiteral("audio/Headphones"));
        AppSettings::instance().setValue(QStringLiteral("audio/Headphones/Enabled"),
                                         QStringLiteral("True"));
        rig.build();
        rig.engine->setHeadphonesEnabled(true);
        rig.engine->start();
        QCOMPARE(rig.native->outputRequests().size(), std::size_t(1));
        QCOMPARE(rig.older->outputRequests().size(), std::size_t(1));
        FakeMatcherAudioBus* desk = rig.native->lastOutput();
        QVERIFY(desk != nullptr && desk->isOpen());

        QSignalSpy rescanned(rig.engine->catalogue(), &IAudioDeviceCatalog::olderDriversRescanned);
        rig.engine->rescanOlderDrivers();
        QCOMPARE(rig.older->fadeRequests(), 1);
        QVERIFY(!rig.engine->headphonesAvailable());   // closed until the new list is in
        QVERIFY(rescanned.wait(kWaitMs));
        QTRY_COMPARE_WITH_TIMEOUT(rig.older->outputRequests().size(), std::size_t(2), kWaitMs);
        QVERIFY(rig.engine->headphonesAvailable());
        QCOMPARE(rig.older->rescanCount(), 1);
        QCOMPARE(rig.engine->roleStatus(AudioRole::Headphones).state, AudioRoleState::Playing);

        // The native role was never touched.
        QCOMPARE(rig.native->rescanCount(), 0);
        QCOMPARE(rig.native->fadeRequests(), 0);
        QCOMPARE(rig.native->outputRequests().size(), std::size_t(1));
        QVERIFY(rig.native->lastOutput() == desk);
        QVERIFY(desk->isOpen());
        QCOMPARE(rig.engine->roleStatus(AudioRole::Speakers).state, AudioRoleState::Playing);
        rig.engine->stop();
    }

    // R-AUD-34: a remote window's engine (no radio, never started) opens
    // its speakers through the same backends, and remote playback reaches
    // that bus's clock matcher.
    void remoteWindowSpeakersUseTheBackends()
    {
        Rig rig;
        rig.build();
        QString error;
        QVERIFY2(rig.engine->beginRemotePlayback(&error), qPrintable(error));
        QCOMPARE(rig.native->outputRequests().size(), std::size_t(1));
        QVERIFY(rig.engine->remotePlaybackIntoMatcher());
        FakeMatcherAudioBus* speakers = rig.native->lastOutput();
        QVERIFY(speakers != nullptr);

        QVector<float> block(2 * 480, 0.25f);
        QVERIFY(rig.engine->writeRemotePlayback(block));
        QCOMPARE(speakers->pushCount(), 1);
        QVERIFY(speakers->matcher() != nullptr);
        QVERIFY(speakers->matcher()->fillFrames() > 0.0);
        rig.engine->endRemotePlayback();
        rig.engine->stop();
    }

    // Bug 1 on the older drivers: a saved DirectSound ID opens the
    // DirectSound device, never the MME device of the same name listed
    // before it, and with DirectSound gone it never opens the MME one.
    void savedDirectSoundIdNeverOpensTheMmeDevice()
    {
        const QString realtek = QStringLiteral("Speakers (Realtek(R) Audio)");
        const QString mme = QStringLiteral("MME");
        const QString ds = QStringLiteral("Windows DirectSound");
        const QString mmeId = portAudioDeviceId(mme, realtek);
        const QString dsId = portAudioDeviceId(ds, realtek);
        const AudioDeviceInfo mmeEntry =
            deviceInfo(AudioBackendId::PortAudio, AudioDeviceDirection::Output, mmeId, realtek, mme);
        const AudioDeviceInfo dsEntry =
            deviceInfo(AudioBackendId::PortAudio, AudioDeviceDirection::Output, dsId, realtek, ds);
        AudioDeviceConfig choice = savedChoice(AudioEngineKind::PortAudio, dsId, realtek);
        {
            Rig rig;
            rig.older->setDevices({mmeEntry, dsEntry});
            choice.saveToSettings(QStringLiteral("audio/Speakers"));
            rig.build();
            rig.engine->start();
            QCOMPARE(rig.older->outputRequests().size(), std::size_t(1));
            QCOMPARE(rig.older->outputRequests().front().deviceId, dsId);
            QCOMPARE(rig.engine->roleStatus(AudioRole::Speakers).state, AudioRoleState::Playing);
            rig.engine->stop();
        }
        AppSettings::instance().clear();
        {
            Rig rig;
            rig.older->setDevices({mmeEntry});
            choice.saveToSettings(QStringLiteral("audio/Speakers"));
            rig.build();
            rig.engine->start();
            for (const AudioStreamRequest& request : rig.older->outputRequests()) {
                QVERIFY(request.deviceId != mmeId);
            }
            QCOMPARE(rig.engine->roleStatus(AudioRole::Speakers).reason, AudioRoleReason::NotConnected);
            rig.engine->stop();
        }
    }

    // A choice equal to the current one opens nothing again, on every role:
    // no click and no delay at start.
    void equalChoiceNeverReopens()
    {
        Rig rig;
        const AudioDeviceConfig speakers = savedChoice(
            AudioEngineKind::CoreAudio, QStringLiteral("desk-uid"), QStringLiteral("Desk speakers"));
        speakers.saveToSettings(QStringLiteral("audio/Speakers"));
        const AudioDeviceConfig headphones = savedChoice(
            AudioEngineKind::PortAudio, paId(QStringLiteral("Desk headphones")),
            QStringLiteral("Desk headphones"), kCoreAudioApi);
        headphones.saveToSettings(QStringLiteral("audio/Headphones"));
        AppSettings::instance().setValue(QStringLiteral("audio/Headphones/Enabled"),
                                         QStringLiteral("True"));
        const AudioDeviceConfig mic = savedChoice(AudioEngineKind::PortAudio, paId(QStringLiteral("Desk mic")),
                                                  QStringLiteral("Desk mic"), kCoreAudioApi);
        mic.saveToSettings(QStringLiteral("audio/TxInput"));
        rig.build(fakeHelper(QStringLiteral("ready")));
        rig.engine->start();
        QCOMPARE(rig.native->outputRequests().size(), std::size_t(1));
        QCOMPARE(rig.older->outputRequests().size(), std::size_t(1));
        const CaptureSupervisor::Status micBefore = rig.engine->captureStatus();

        rig.engine->setSpeakersConfig(speakers);
        rig.engine->setHeadphonesConfig(headphones);
        rig.engine->setHeadphonesEnabled(true);
        rig.engine->setTxInputConfig(mic);
        QCoreApplication::processEvents();
        QCOMPARE(rig.native->outputRequests().size(), std::size_t(1));
        QCOMPARE(rig.older->outputRequests().size(), std::size_t(1));
        QCOMPARE(rig.engine->captureStatus().generation, micBefore.generation);
        QCOMPARE(rig.engine->captureStatus().state, micBefore.state);
        rig.engine->stop();
    }

    // The first choice given before the devices are up is the one opened:
    // once, never the saved device first.
    void firstChoiceOpensOnce()
    {
        Rig rig;
        savedChoice(AudioEngineKind::CoreAudio, QStringLiteral("built-in-uid"),
                    QStringLiteral("Built-in speakers"))
            .saveToSettings(QStringLiteral("audio/Speakers"));
        rig.build();
        rig.engine->setSpeakersConfig(savedChoice(AudioEngineKind::CoreAudio, QStringLiteral("desk-uid"),
                                                  QStringLiteral("Desk speakers")));
        rig.engine->start();
        QCOMPARE(rig.native->outputRequests().size(), std::size_t(1));
        QCOMPARE(rig.native->outputRequests().front().deviceId, QStringLiteral("desk-uid"));
        rig.engine->stop();
    }

    // An idle mic whose device is not listed reads Silent, not connected.
    void idleMicNotListedReadsSilent()
    {
        Rig rig;
        savedChoice(AudioEngineKind::PortAudio, paId(QStringLiteral("Gone mic")),
                    QStringLiteral("Gone mic"), kCoreAudioApi)
            .saveToSettings(QStringLiteral("audio/TxInput"));
        rig.build(fakeHelper(QStringLiteral("ready")));
        rig.engine->start();
        const AudioRoleStatus status = rig.engine->roleStatus(AudioRole::TxInput);
        QCOMPARE(status.state, AudioRoleState::Silent);
        QCOMPARE(status.reason, AudioRoleReason::NotConnected);
        QVERIFY(status.playingName.isEmpty());
        rig.engine->stop();
    }

    // The PC mic role: the matched config goes to the capture helper, and
    // its Ready completes the open.
    void micRoleOpensThroughTheCaptureHelper()
    {
        Rig rig;
        savedChoice(AudioEngineKind::PortAudio, paId(QStringLiteral("Desk mic")),
                    QStringLiteral("Desk mic"), kCoreAudioApi)
            .saveToSettings(QStringLiteral("audio/TxInput"));
        rig.build(fakeHelper(QStringLiteral("ready")));
        rig.engine->start();
        // Nothing captures until someone demands it; the listed mic reads
        // Playing on itself meanwhile.
        QCOMPARE(rig.engine->roleStatus(AudioRole::TxInput).state, AudioRoleState::Playing);
        QCOMPARE(rig.engine->roleStatus(AudioRole::TxInput).playingName, QStringLiteral("Desk mic"));
        QCOMPARE(rig.engine->captureStatus().state, CaptureSupervisor::Status::State::Closed);
        CaptureSupervisor::Lease lease =
            rig.engine->acquireCaptureDemand(CaptureSupervisor::Demand::TestMic);
        QTRY_COMPARE_WITH_TIMEOUT(rig.engine->captureStatus().state,
                                  CaptureSupervisor::Status::State::Ready, 10000);
        QCOMPARE(rig.engine->roleStatus(AudioRole::TxInput).state, AudioRoleState::Playing);
        QCOMPARE(rig.engine->roleStatus(AudioRole::TxInput).playingName, QStringLiteral("Desk mic"));
        QCOMPARE(rig.engine->captureStatus().configuredDevice, QStringLiteral("Desk mic"));
        lease.release();
        rig.engine->stop();
    }

    // Ready, then the helper's input stops (InputLost): the chosen mic is
    // lost and the role goes silent, never onto another mic.
    void micRoleLostInputGoesSilent()
    {
        Rig rig;
        savedChoice(AudioEngineKind::PortAudio, paId(QStringLiteral("Desk mic")),
                    QStringLiteral("Desk mic"), kCoreAudioApi)
            .saveToSettings(QStringLiteral("audio/TxInput"));
        rig.build(fakeHelper(QStringLiteral("input-lost")));
        bool playing = false;
        bool lostAfterPlaying = false;
        connect(rig.engine.get(), &AudioEngine::roleStatusChanged, this,
                [&](AudioRole role, const AudioRoleStatus& status) {
                    if (role != AudioRole::TxInput) {
                        return;
                    }
                    if (status.state == AudioRoleState::Playing) {
                        playing = true;
                    } else if (playing && status.state == AudioRoleState::Silent
                               && status.reason == AudioRoleReason::NotConnected) {
                        lostAfterPlaying = true;
                    }
                });
        // The scripted helper's failure is the case under test.
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("helper failed generation")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("failed: InputLost")));
        rig.engine->start();
        CaptureSupervisor::Lease lease =
            rig.engine->acquireCaptureDemand(CaptureSupervisor::Demand::TestMic);
        QTRY_VERIFY_WITH_TIMEOUT(lostAfterPlaying, 10000);
        lease.release();
        rig.engine->stop();
    }

    // R-AUD-08 (C1): the chosen device fails to open while the system
    // default plays; each retry's failed open leaves the default's bus
    // open and in the slot, and the role still reads PlayingOnDefault.
    void failedRetryKeepsTheDefaultPlaying()
    {
        Rig rig;
        rig.native->setFailingOutputs({QStringLiteral("desk-uid")});
        savedChoice(AudioEngineKind::CoreAudio, QStringLiteral("desk-uid"),
                    QStringLiteral("Desk speakers"))
            .saveToSettings(QStringLiteral("audio/Speakers"));
        rig.build();
        // The failed opens are the case under test.
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("could not open")));
        for (int i = 0; i < 2; ++i) {
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("open failed")));
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("did not open")));
        }
        rig.engine->start();
        QCOMPARE(rig.native->outputRequests().size(), std::size_t(2));
        QCOMPARE(rig.native->outputRequests().at(0).deviceId, QStringLiteral("desk-uid"));
        QVERIFY(rig.native->outputRequests().at(1).deviceId.isEmpty());
        QCOMPARE(rig.engine->roleStatus(AudioRole::Speakers).state, AudioRoleState::PlayingOnDefault);
        FakeMatcherAudioBus* def = rig.native->lastOutput();
        QVERIFY(def != nullptr && rig.native->outputAlive(1));

        // The retry (250 ms) tries the chosen device again, and fails.
        QTRY_VERIFY_WITH_TIMEOUT(rig.native->outputRequests().size() >= std::size_t(3), kWaitMs);
        QCOMPARE(rig.native->outputRequests().at(2).deviceId, QStringLiteral("desk-uid"));
        QVERIFY(!rig.native->outputAlive(2));
        QVERIFY(rig.native->outputAlive(1));
        QVERIFY(def->isOpen());
        QCOMPARE(rig.native->aliveOutputs(), 1);
        // Still the bus the speakers play on: the readout is its matcher.
        QVERIFY(rig.engine->delayParts(AudioRole::Speakers).matcherFillMs >= 0.0);
        QCOMPARE(rig.engine->roleStatus(AudioRole::Speakers).state, AudioRoleState::PlayingOnDefault);
        QCOMPARE(rig.engine->roleStatus(AudioRole::Speakers).playingName,
                 QStringLiteral("Built-in speakers"));
        rig.engine->stop();
    }

    // R-AUD-08, R-AUD-12 (C1): the system default moves while the role
    // plays on it, and the new default cannot open: the old default's
    // bus keeps playing.
    void failedOpenOnADefaultMoveKeepsTheOldDefault()
    {
        Rig rig;
        rig.build();
        rig.engine->start();
        QCOMPARE(rig.native->outputRequests().size(), std::size_t(1));
        QCOMPARE(rig.engine->roleStatus(AudioRole::Speakers).state, AudioRoleState::Playing);

        rig.native->setFailingOutputs({QString()});
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("open failed")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("did not open")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("could not open")));
        rig.native->setDefault(AudioDeviceDirection::Output, QStringLiteral("desk-uid"));
        rig.native->postNotice(AudioNotice::DefaultOutputChanged);
        QTRY_VERIFY_WITH_TIMEOUT(rig.native->outputRequests().size() >= std::size_t(2), kWaitMs);
        QVERIFY(!rig.native->outputAlive(1));
        QVERIFY(rig.native->outputAlive(0));
        QVERIFY(rig.native->lastOutput() != nullptr);
        QVERIFY(rig.engine->delayParts(AudioRole::Speakers).matcherFillMs >= 0.0);
        rig.engine->stop();
    }

    // C1: reopening the same device closes the old stream first (a device
    // may not open twice); another device opens before the old one closes.
    // When that reopen fails the role is closed, and the supervisor falls
    // back to the system default.
    void reopenOfTheSameDeviceClosesFirst()
    {
        std::vector<int> aliveAtCreate;   // outlives the rig's hook
        Rig rig;
        savedChoice(AudioEngineKind::CoreAudio, QStringLiteral("desk-uid"),
                    QStringLiteral("Desk speakers"))
            .saveToSettings(QStringLiteral("audio/Speakers"));
        rig.build();
        rig.native->setOutputCreatedHook([&](const AudioStreamRequest&) {
            aliveAtCreate.push_back(rig.native->aliveOutputs());
        });
        rig.engine->start();
        QCOMPARE(aliveAtCreate, std::vector<int>({1}));

        // Another device: the new bus opens while the old one still plays.
        rig.engine->setSpeakersConfig(savedChoice(AudioEngineKind::CoreAudio,
                                                  QStringLiteral("built-in-uid"),
                                                  QStringLiteral("Built-in speakers")));
        QCOMPARE(aliveAtCreate, std::vector<int>({1, 2}));
        QCOMPARE(rig.native->aliveOutputs(), 1);

        // The same device again (a format change): closed first.
        AudioStreamEvent changed;
        changed.kind = AudioStreamEvent::Kind::FormatChanged;
        rig.native->lastOutput()->emitEventForTest(changed);
        QTRY_COMPARE_WITH_TIMEOUT(aliveAtCreate.size(), std::size_t(3), kWaitMs);
        QCOMPARE(aliveAtCreate.back(), 1);
        QCOMPARE(rig.engine->roleStatus(AudioRole::Speakers).state, AudioRoleState::Playing);

        // The same device again, and it fails: the role reads closed and
        // falls back to the system default.
        rig.native->setFailingOutputs({QStringLiteral("built-in-uid")});
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("open failed")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("did not open")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("could not open")));
        rig.native->lastOutput()->emitEventForTest(changed);
        QTRY_COMPARE_WITH_TIMEOUT(aliveAtCreate.size(), std::size_t(5), kWaitMs);
        QCOMPARE(aliveAtCreate.at(3), 1);
        QVERIFY(rig.native->outputRequests().back().deviceId.isEmpty());
        QCOMPARE(rig.native->aliveOutputs(), 1);
        QCOMPARE(rig.engine->roleStatus(AudioRole::Speakers).state, AudioRoleState::PlayingOnDefault);
        rig.native->setOutputCreatedHook({});
        rig.engine->stop();
    }

    // R-AUD-16 (W1): Windows audio, shared asks for the engine's smallest
    // period (bufferFrames 0); the other engines keep the saved size.
    void windowsSharedAsksForTheSmallestPeriod()
    {
        auto wasapi = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::Wasapi);
        wasapi->setDevices({deviceInfo(AudioBackendId::Wasapi, AudioDeviceDirection::Output,
                                       QStringLiteral("wasapi-desk"), QStringLiteral("Desk speakers"))});
        wasapi->setDefault(AudioDeviceDirection::Output, QStringLiteral("wasapi-desk"));
        auto older = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PortAudio);
        older->setTakesStereoMix(false);
        older->setDevices({deviceInfo(AudioBackendId::PortAudio, AudioDeviceDirection::Output,
                                      paId(QStringLiteral("Desk headphones")),
                                      QStringLiteral("Desk headphones"), kCoreAudioApi)});

        AudioDeviceConfig speakers = savedChoice(AudioEngineKind::WindowsShared,
                                                 QStringLiteral("wasapi-desk"),
                                                 QStringLiteral("Desk speakers"));
        speakers.bufferSamples = 128;
        speakers.saveToSettings(QStringLiteral("audio/Speakers"));
        AudioDeviceConfig headphones = savedChoice(AudioEngineKind::PortAudio,
                                                   paId(QStringLiteral("Desk headphones")),
                                                   QStringLiteral("Desk headphones"), kCoreAudioApi);
        headphones.bufferSamples = 128;
        headphones.saveToSettings(QStringLiteral("audio/Headphones"));
        AppSettings::instance().setValue(QStringLiteral("audio/Headphones/Enabled"),
                                         QStringLiteral("True"));

        AudioEngine engine;
        engine.setVaxOutputsAllowed(false);
        engine.setAudioBackendsForTest({wasapi, older});
        engine.setHeadphonesEnabled(true);
        engine.start();
        QCOMPARE(wasapi->outputRequests().size(), std::size_t(1));
        QCOMPARE(wasapi->outputRequests().front().bufferFrames, 0);
        QVERIFY(!wasapi->outputRequests().front().exclusive);
        QCOMPARE(older->outputRequests().size(), std::size_t(1));
        QCOMPARE(older->outputRequests().front().bufferFrames, 128);

        // Windows audio, exclusive keeps the saved size.
        AudioDeviceConfig exclusive = speakers;
        exclusive.engine = AudioEngineKind::WindowsExclusive;
        engine.setSpeakersConfig(exclusive);
        QCOMPARE(wasapi->outputRequests().size(), std::size_t(2));
        QCOMPARE(wasapi->outputRequests().back().bufferFrames, 128);
        QVERIFY(wasapi->outputRequests().back().exclusive);
        engine.stop();
    }

    // R-AUD-06 (M3): Rescan waits out a fade longer than kRescanFadeMs
    // when the stream's callback is larger: 2048 frames at 48 kHz is
    // 42.7 ms, so a 30 ms fade ends before the bus is closed.
    void rescanWaitsOutALargeCallback()
    {
        Rig rig;
        rig.older->setTakesStereoMix(true);
        rig.older->setCallbackFrames(2048);
        rig.older->setFadeTimeMs(30);
        savedChoice(AudioEngineKind::PortAudio, paId(QStringLiteral("Desk headphones")),
                    QStringLiteral("Desk headphones"), kCoreAudioApi)
            .saveToSettings(QStringLiteral("audio/Headphones"));
        AppSettings::instance().setValue(QStringLiteral("audio/Headphones/Enabled"),
                                         QStringLiteral("True"));
        rig.build();
        rig.engine->setHeadphonesEnabled(true);
        rig.engine->start();
        QCOMPARE(rig.older->outputRequests().size(), std::size_t(1));
        QCOMPARE(rig.older->outputRequests().front().sampleRate, 48000);

        QSignalSpy rescanned(rig.engine->catalogue(), &IAudioDeviceCatalog::olderDriversRescanned);
        rig.engine->rescanOlderDrivers();
        QCOMPARE(rig.older->fadeRequests(), 1);
        QVERIFY(!rig.older->outputAlive(0));
        QCOMPARE(rig.older->closedUnfaded(), 0);
        QVERIFY(rescanned.wait(kWaitMs));
        QTRY_COMPARE_WITH_TIMEOUT(rig.older->outputRequests().size(), std::size_t(2), kWaitMs);
        rig.engine->stop();
    }

    // M2: a capture demand before the radio starts (Test Mic) opens the
    // helper once, on the matched mic, never the saved choice and then the
    // matched one.  A mic saved before engines (no Engine, no DeviceId)
    // matches to a config that differs from the saved one.
    void firstMicDemandOpensTheHelperOnce()
    {
        // Declared before the rig: the helper's last statuses arrive while
        // the engine is destroyed.
        std::vector<quint32> generations;
        Rig rig;
        rig.native->addDevice(deviceInfo(AudioBackendId::CoreAudio, AudioDeviceDirection::Input,
                                         QStringLiteral("desk-mic-uid"), QStringLiteral("Desk mic")));
        AudioDeviceConfig legacy;
        legacy.deviceName = QStringLiteral("Desk mic");
        legacy.driverApi = kCoreAudioApi;
        legacy.saveToSettings(QStringLiteral("audio/TxInput"));
        rig.build(fakeHelper(QStringLiteral("ready")));
        connect(rig.engine.get(), &AudioEngine::captureStatusChanged, this,
                [&](const CaptureSupervisor::Status& status) {
                    if (status.generation == 0) {
                        return;
                    }
                    if (generations.empty() || generations.back() != status.generation) {
                        generations.push_back(status.generation);
                    }
                });
        CaptureSupervisor::Lease lease =
            rig.engine->acquireCaptureDemand(CaptureSupervisor::Demand::TestMic);
        QTRY_COMPARE_WITH_TIMEOUT(rig.engine->captureStatus().state,
                                  CaptureSupervisor::Status::State::Ready, 10000);
        const quint32 first = rig.engine->captureStatus().generation;
        rig.engine->start();
        QCoreApplication::processEvents();
        QCOMPARE(rig.engine->captureStatus().generation, first);
        QCOMPARE(rig.engine->captureStatus().state, CaptureSupervisor::Status::State::Ready);
        QCOMPARE(rig.engine->roleStatus(AudioRole::TxInput).state, AudioRoleState::Playing);
        QCOMPARE(generations, std::vector<quint32>({first}));
        lease.release();
        rig.engine->stop();
    }
};

int main(int argc, char* argv[])
{
    if (argc > 2 && std::strcmp(argv[1], "--fake-capture-child") == 0) {
        return NereusSDR::Test::runFakeCaptureChild(QString::fromLocal8Bit(argv[2]));
    }
    QCoreApplication app(argc, argv);
    TstAudioEngineNativeRouting test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_audio_engine_native_routing.moc"
