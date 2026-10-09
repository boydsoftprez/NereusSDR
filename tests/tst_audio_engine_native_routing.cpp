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
                        QStringLiteral("Desk headphones"), QStringLiteral("Desk headphones"),
                        kCoreAudioApi),
             deviceInfo(AudioBackendId::PortAudio, AudioDeviceDirection::Output,
                        QStringLiteral("Built-in speakers"), QStringLiteral("Built-in speakers"),
                        kCoreAudioApi),
             deviceInfo(AudioBackendId::PortAudio, AudioDeviceDirection::Input,
                        QStringLiteral("Desk mic"), QStringLiteral("Desk mic"), kCoreAudioApi)});
        older->setDefault(AudioDeviceDirection::Output, QStringLiteral("Built-in speakers"));
        older->setDefault(AudioDeviceDirection::Input, QStringLiteral("Desk mic"));
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

        // The registry builds the older drivers here, until the native
        // engine tasks add theirs.
        const auto system = makeSystemAudioBackends(AudioBackendContext{});
        QVERIFY(!system.empty());
        QCOMPARE(system.back()->id(), AudioBackendId::PortAudio);
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
                                                  QStringLiteral("Built-in speakers"),
                                                  QStringLiteral("Built-in speakers"),
                                                  kCoreAudioApi));
        QCOMPARE(rig.older->outputRequests().size(), std::size_t(1));
        QCOMPARE(rig.older->outputRequests().front().deviceId, QStringLiteral("Built-in speakers"));
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
        savedChoice(AudioEngineKind::PortAudio, QStringLiteral("Desk headphones"),
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
        savedChoice(AudioEngineKind::PortAudio, QStringLiteral("Desk headphones"),
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

    // The PC mic role: the matched config goes to the capture helper, and
    // its Ready completes the open.
    void micRoleOpensThroughTheCaptureHelper()
    {
        Rig rig;
        savedChoice(AudioEngineKind::PortAudio, QStringLiteral("Desk mic"),
                    QStringLiteral("Desk mic"), kCoreAudioApi)
            .saveToSettings(QStringLiteral("audio/TxInput"));
        rig.build(fakeHelper(QStringLiteral("ready")));
        rig.engine->start();
        // Nothing captures until someone demands it.
        QCOMPARE(rig.engine->roleStatus(AudioRole::TxInput).state, AudioRoleState::Silent);
        CaptureSupervisor::Lease lease =
            rig.engine->acquireCaptureDemand(CaptureSupervisor::Demand::TestMic);
        QTRY_COMPARE_WITH_TIMEOUT(rig.engine->roleStatus(AudioRole::TxInput).state,
                                  AudioRoleState::Playing, 10000);
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
        savedChoice(AudioEngineKind::PortAudio, QStringLiteral("Desk mic"),
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
