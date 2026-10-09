// no-port-check: test-only, NereusSDR-original. No upstream logic is
// ported here.
//
// =================================================================
// tests/tst_core_speaker_model.cpp  (NereusSDR)
// =================================================================
//
// The Core speaker on the Core (native audio plan Task 21; R-AUD-25,
// R-AUD-28, R-AUD-30, D23, D31; V-SW-9 Core half):
//   - the four JSON forms round-trip, and a form with an extra or a
//     missing key, or a wrong type, reads as nothing;
//   - the speakers role's status maps to the Core speaker's state;
//   - on the Core, radio's coreSpeaker properties are the engine's master
//     level and mute and the speakers role: 50 with nothing saved, a level
//     saved as the linear "0.720", the mute as "True", a card pick saved
//     with its engine and reopened, the Core's default as an empty id;
//   - the master level and mute act on the speakers alone: every tap
//     carries the same samples at 0 and muted as at 100 and unmuted;
//   - a box that starts into a desktop waits for a pick, opening no card
//     and saving nothing, until a window picks one.
//
// Fake engines only (FakeAudioEngineBackend, FakeAudioBus).  No device is
// touched.
//
// Modification history (NereusSDR):
//   2026-10-09 - New test (native audio plan Task 21). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - Task 16 fix round: the list fills before a radio connects
//                and the audio_device seed still applies. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QStandardPaths>

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/audio/CoreSpeakerJson.h"
#include "core/audio/IAudioDeviceCatalog.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"
#include "fakes/FakeAudioEngineBackend.h"

#include <memory>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr int kWaitMs = 5000;
constexpr int kFrames = 64;

const QString kUsbId = QStringLiteral("Device,0");
const QString kUsbName = QStringLiteral("USB Audio Device");
const QString kBuiltInId = QStringLiteral("PCH,0");
const QString kBuiltInName = QStringLiteral("Built-in Audio");

QString saved(const char* key)
{
    return AppSettings::instance()
        .value(QStringLiteral("audio/") + QLatin1String(key), QStringLiteral("<unset>"))
        .toString();
}

AudioDeviceInfo card(const QString& id, const QString& name)
{
    AudioDeviceInfo info;
    info.backend = AudioBackendId::AlsaDirect;
    info.direction = AudioDeviceDirection::Output;
    info.id = id;
    info.name = name;
    return info;
}

// The Core's engine on a fake ALSA direct engine with two cards, the
// built-in one the default.  VAX outputs are not under test here.
struct CoreRig {
    std::shared_ptr<FakeAudioEngineBackend> alsa =
        std::make_shared<FakeAudioEngineBackend>(AudioBackendId::AlsaDirect);
    RadioModel radio;
    AudioEngine* engine = nullptr;

    CoreRig()
    {
        alsa->setDevices({card(kUsbId, kUsbName), card(kBuiltInId, kBuiltInName)});
        alsa->setDefault(AudioDeviceDirection::Output, kBuiltInId);
        engine = radio.audioEngine();
        engine->setVaxOutputsAllowed(false);
        engine->setAudioBackendsForTest({alsa});
    }

    CoreSpeakerState state() const
    {
        return coreSpeakerStateFromJson(radio.coreSpeakerState()).value_or(CoreSpeakerState{});
    }

    QList<CoreSpeakerCard> cards() const
    {
        return coreSpeakerDevicesFromJson(radio.coreSpeakerDevices()).value_or(QList<CoreSpeakerCard>{});
    }

    int speakerOpens() const
    {
        int opens = 0;
        for (const AudioStreamRequest& request : alsa->outputRequests()) {
            if (request.direction == AudioDeviceDirection::Output) {
                ++opens;
            }
        }
        return opens;
    }

    QString lastOpenedId() const
    {
        const std::vector<AudioStreamRequest> requests = alsa->outputRequests();
        return requests.empty() ? QStringLiteral("<none>") : requests.back().deviceId;
    }
};

AudioRoleStatus status(AudioRoleState state, AudioRoleReason reason = AudioRoleReason::None,
                       const QString& chosen = {}, const QString& playing = {})
{
    AudioRoleStatus s;
    s.state = state;
    s.reason = reason;
    s.chosenName = chosen;
    s.playingName = playing;
    return s;
}

std::vector<float> stereoBlock(float left, float right)
{
    std::vector<float> block(std::size_t(kFrames) * 2);
    for (int i = 0; i < kFrames; ++i) {
        block[std::size_t(2 * i)] = left;
        block[std::size_t(2 * i + 1)] = right;
    }
    return block;
}

class RecordingTap final : public MasterMixAudioTap {
public:
    void consume(const float* samples, int frames, int) noexcept override
    {
        received.insert(received.end(), samples, samples + frames * 2);
    }
    std::vector<float> received;
};

class RecordingSliceTap final : public SliceAudioTap {
public:
    void consume(const float* samples, int frames, int) noexcept override
    {
        received.insert(received.end(), samples, samples + frames * 2);
    }
    void skip(int frames, int) noexcept override { skipped += frames; }
    std::vector<float> received;
    int skipped = 0;
};

// A Core with one streaming slice at unity AF, the speakers on a fake
// 48 kHz stereo bus, the mix ramps cut to one frame, and every tap the
// Core feeds a window or the radio installed.
struct TapRig {
    RadioModel radio;
    AudioEngine* engine = nullptr;
    FakeAudioBus* speakers = nullptr;
    int slice = -1;
    int ownerSlot = -1;
    RecordingTap master;
    RecordingTap owner;
    RecordingTap radioOut;
    RecordingSliceTap sliceTap;

    TapRig()
    {
        engine = radio.audioEngine();
        auto bus = std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
        AudioFormat format;
        format.sampleRate = 48000;
        format.channels = 2;
        format.sample = AudioFormat::Sample::Float32;
        bus->open(format);
        speakers = bus.get();
        engine->setSpeakersBusForTest(std::move(bus));
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        engine->masterMixForTest().setRampFrames(1);
        engine->masterMixForTest().setSlewUpFrames(0);
        slice = radio.addSlice();
        engine->setSliceStreaming(slice, true);
        radio.sliceById(slice)->setAfGain(100);
        engine->setRadioSpeakerVolume(0.80f);

        engine->setMasterMixAudioTap(&master);
        ownerSlot = engine->acquireOwnerMix();
        engine->setOwnerMixSliceMask(ownerSlot, 1u << slice);
        engine->setOwnerMixAudioTap(ownerSlot, &owner);
        engine->setRadioOutputTap(&radioOut);
        engine->setSliceAudioTap(slice, &sliceTap);
    }

    ~TapRig()
    {
        engine->clearSliceAudioTap(&sliceTap);
        engine->clearRadioOutputTap(&radioOut);
        engine->releaseOwnerMix(ownerSlot);
        engine->clearMasterMixAudioTap(&master);
    }

    void feed(int blocks)
    {
        const std::vector<float> block = stereoBlock(0.5f, 0.25f);
        for (int i = 0; i < blocks; ++i) {
            engine->rxBlockReady(slice, block.data(), kFrames);
        }
    }
};

} // namespace

class TstCoreSpeakerModel : public QObject {
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

    // ── JSON ───────────────────────────────────────────────────────────

    void json_stateRoundTripsAndIsStrict()
    {
        const CoreSpeakerStateKind kinds[] = {
            CoreSpeakerStateKind::Playing, CoreSpeakerStateKind::NotConnected,
            CoreSpeakerStateKind::InUse, CoreSpeakerStateKind::NoCard,
            CoreSpeakerStateKind::WaitingForPick};
        for (const CoreSpeakerStateKind kind : kinds) {
            CoreSpeakerState state;
            state.kind = kind;
            state.playingName = kBuiltInName;
            state.chosenName = kUsbName;
            state.desktop = kind == CoreSpeakerStateKind::WaitingForPick;
            const std::optional<CoreSpeakerState> back =
                coreSpeakerStateFromJson(coreSpeakerStateToJson(state));
            QVERIFY(back.has_value());
            QCOMPARE(*back, state);
        }
        QCOMPARE(coreSpeakerStateToJson(CoreSpeakerState{}),
                 QStringLiteral(R"({"chosen":"","desktop":false,"playing":"","state":"noCard"})"));

        QVERIFY(!coreSpeakerStateFromJson(
            QStringLiteral(R"({"chosen":"","desktop":false,"playing":"","state":"noCard","x":1})")));
        QVERIFY(!coreSpeakerStateFromJson(
            QStringLiteral(R"({"chosen":"","desktop":false,"state":"noCard"})")));
        QVERIFY(!coreSpeakerStateFromJson(
            QStringLiteral(R"({"chosen":"","desktop":0,"playing":"","state":"noCard"})")));
        QVERIFY(!coreSpeakerStateFromJson(
            QStringLiteral(R"({"chosen":"","desktop":false,"playing":"","state":"loud"})")));
        QVERIFY(!coreSpeakerStateFromJson(QStringLiteral("[]")));
        QVERIFY(!coreSpeakerStateFromJson(QStringLiteral("not json")));
    }

    void json_devicesRoundTripAndAreStrict()
    {
        const QList<CoreSpeakerCard> cards = {
            CoreSpeakerCard{kUsbId, kUsbName, AudioDeviceState::Present},
            CoreSpeakerCard{kBuiltInId, kBuiltInName, AudioDeviceState::InUse},
            CoreSpeakerCard{QStringLiteral("Gone,0"), QStringLiteral("Gone"),
                            AudioDeviceState::NotConnected}};
        const std::optional<QList<CoreSpeakerCard>> back =
            coreSpeakerDevicesFromJson(coreSpeakerDevicesToJson(cards));
        QVERIFY(back.has_value());
        QCOMPARE(*back, cards);
        QCOMPARE(coreSpeakerDevicesToJson({}), QStringLiteral("[]"));
        QVERIFY(coreSpeakerDevicesFromJson(QStringLiteral("[]")).has_value());
        QCOMPARE(coreSpeakerDevicesToJson({cards.first()}),
                 QStringLiteral(R"([{"id":"Device,0","name":"USB Audio Device","state":"present"}])"));

        QVERIFY(!coreSpeakerDevicesFromJson(
            QStringLiteral(R"([{"id":"a","name":"b","state":"present","extra":true}])")));
        QVERIFY(!coreSpeakerDevicesFromJson(QStringLiteral(R"([{"id":"a","name":"b"}])")));
        QVERIFY(!coreSpeakerDevicesFromJson(
            QStringLiteral(R"([{"id":"a","name":"b","state":"gone"}])")));
        QVERIFY(!coreSpeakerDevicesFromJson(QStringLiteral(R"([{"id":1,"name":"b","state":"present"}])")));
        QVERIFY(!coreSpeakerDevicesFromJson(QStringLiteral(R"(["a"])")));
        QVERIFY(!coreSpeakerDevicesFromJson(QStringLiteral(R"({"id":"a"})")));
    }

    void json_deviceRoundTripsAndIsStrict()
    {
        const std::optional<QPair<QString, QString>> usb =
            coreSpeakerDeviceFromJson(coreSpeakerDeviceToJson(kUsbId, kUsbName));
        QVERIFY(usb.has_value());
        QCOMPARE(usb->first, kUsbId);
        QCOMPARE(usb->second, kUsbName);
        QCOMPARE(coreSpeakerDeviceToJson(QString(), QString()),
                 QStringLiteral(R"({"id":"","name":""})"));
        const std::optional<QPair<QString, QString>> core =
            coreSpeakerDeviceFromJson(QStringLiteral(R"({"id":"","name":""})"));
        QVERIFY(core.has_value());
        QVERIFY(core->first.isEmpty() && core->second.isEmpty());

        QVERIFY(!coreSpeakerDeviceFromJson(QStringLiteral(R"({"id":"","name":"","x":""})")));
        QVERIFY(!coreSpeakerDeviceFromJson(QStringLiteral(R"({"id":""})")));
        QVERIFY(!coreSpeakerDeviceFromJson(QStringLiteral(R"({"id":"","name":3})")));
        QVERIFY(!coreSpeakerDeviceFromJson(QString()));
    }

    void json_detailsRoundTripAndAreStrict()
    {
        CoreSpeakerDetails details;
        details.bufferFrames = 256;
        details.delayMs = 10;
        details.sampleRate = 96000;
        details.negotiated = QStringLiteral("96000 Hz · 2 ch · 256 samples");
        details.delayNowMs = 12.5;
        const std::optional<CoreSpeakerDetails> back =
            coreSpeakerDetailsFromJson(coreSpeakerDetailsToJson(details));
        QVERIFY(back.has_value());
        QCOMPARE(*back, details);
        QCOMPARE(coreSpeakerDetailsFromJson(coreSpeakerDetailsToJson(CoreSpeakerDetails{})).value(),
                 CoreSpeakerDetails{});

        QVERIFY(!coreSpeakerDetailsFromJson(QStringLiteral(
            R"({"bufferFrames":256,"delayMs":10,"delayNowMs":-1,"negotiated":"","sampleRate":48000,"x":0})")));
        QVERIFY(!coreSpeakerDetailsFromJson(QStringLiteral(
            R"({"bufferFrames":256,"delayMs":10,"negotiated":"","sampleRate":48000})")));
        QVERIFY(!coreSpeakerDetailsFromJson(QStringLiteral(
            R"({"bufferFrames":256.5,"delayMs":10,"delayNowMs":-1,"negotiated":"","sampleRate":48000})")));
        QVERIFY(!coreSpeakerDetailsFromJson(QStringLiteral(
            R"({"bufferFrames":"256","delayMs":10,"delayNowMs":-1,"negotiated":"","sampleRate":48000})")));
        QVERIFY(!coreSpeakerDetailsFromJson(QStringLiteral(
            R"({"bufferFrames":256,"delayMs":10,"delayNowMs":-1,"negotiated":5,"sampleRate":48000})")));
    }

    void json_stateFromRoleStatus()
    {
        CoreSpeakerState s = coreSpeakerStateFor(
            status(AudioRoleState::Playing, AudioRoleReason::None, kUsbName, kUsbName), false);
        QCOMPARE(s.kind, CoreSpeakerStateKind::Playing);
        QCOMPARE(s.playingName, kUsbName);
        QCOMPARE(s.chosenName, kUsbName);

        s = coreSpeakerStateFor(status(AudioRoleState::PlayingOnDefault,
                                       AudioRoleReason::NotConnected, kUsbName, kBuiltInName),
                                false);
        QCOMPARE(s.kind, CoreSpeakerStateKind::NotConnected);
        QCOMPARE(s.playingName, kBuiltInName);
        QCOMPARE(s.chosenName, kUsbName);

        s = coreSpeakerStateFor(status(AudioRoleState::PlayingOnDefault, AudioRoleReason::InUse,
                                       kUsbName, kBuiltInName),
                                false);
        QCOMPARE(s.kind, CoreSpeakerStateKind::InUse);
        QCOMPARE(s.playingName, kBuiltInName);

        s = coreSpeakerStateFor(status(AudioRoleState::Silent, AudioRoleReason::NoDevice), false);
        QCOMPARE(s.kind, CoreSpeakerStateKind::NoCard);
        QVERIFY(s.playingName.isEmpty());

        s = coreSpeakerStateFor(status(AudioRoleState::Silent, AudioRoleReason::NotConnected,
                                       kUsbName),
                                false);
        QCOMPARE(s.kind, CoreSpeakerStateKind::NotConnected);
        QVERIFY(s.playingName.isEmpty());
        QCOMPARE(s.chosenName, kUsbName);

        s = coreSpeakerStateFor(status(AudioRoleState::WaitingForPick), true);
        QCOMPARE(s.kind, CoreSpeakerStateKind::WaitingForPick);
        QVERIFY(s.desktop);
    }

    // ── The Core's binding ─────────────────────────────────────────────

    // Settled call 29: 50 when no level was ever saved.
    void host_noSavedLevelIsFifty()
    {
        CoreRig rig;
        rig.radio.setCoreSpeakerHost(true);
        QVERIFY(rig.radio.coreSpeakerHost());
        QVERIFY(rig.radio.coreSpeakerAvailable());
        QVERIFY(rig.radio.coreSpeakerUnavailableReason().isEmpty());
        QCOMPARE(rig.radio.coreSpeakerVolume(), 50);
        QCOMPARE(rig.engine->volume(), 0.5f);
        QVERIFY(!rig.radio.coreSpeakerMuted());
        QVERIFY(!rig.engine->masterMuted());
    }

    // A saved level and mute load into the engine.
    void host_loadsTheSavedLevelAndMute()
    {
        AppSettings::instance().setValue(QStringLiteral("audio/Master/Volume"), QStringLiteral("0.250"));
        AppSettings::instance().setValue(QStringLiteral("audio/Master/Muted"), QStringLiteral("True"));
        CoreRig rig;
        rig.radio.setCoreSpeakerHost(true);
        QCOMPARE(rig.radio.coreSpeakerVolume(), 25);
        QCOMPARE(rig.engine->volume(), 0.25f);
        QVERIFY(rig.radio.coreSpeakerMuted());
        QVERIFY(rig.engine->masterMuted());
    }

    void host_levelAndMuteSaveAndReachTheEngine()
    {
        CoreRig rig;
        rig.radio.setCoreSpeakerHost(true);
        QSignalSpy volume(&rig.radio, &RadioModel::coreSpeakerVolumeChanged);
        QSignalSpy muted(&rig.radio, &RadioModel::coreSpeakerMutedChanged);

        rig.radio.setCoreSpeakerVolume(72);
        QCOMPARE(rig.radio.coreSpeakerVolume(), 72);
        QCOMPARE(saved("Master/Volume"), QStringLiteral("0.720"));
        QCOMPARE(rig.engine->volume(), 0.72f);
        QCOMPARE(volume.count(), 1);

        rig.radio.setCoreSpeakerMuted(true);
        QVERIFY(rig.radio.coreSpeakerMuted());
        QCOMPARE(saved("Master/Muted"), QStringLiteral("True"));
        QVERIFY(rig.engine->masterMuted());
        QCOMPARE(muted.count(), 1);

        // Out of range clamps; the same value again is no change.
        rig.radio.setCoreSpeakerVolume(140);
        QCOMPARE(rig.radio.coreSpeakerVolume(), 100);
        QCOMPARE(saved("Master/Volume"), QStringLiteral("1.000"));
        rig.radio.setCoreSpeakerVolume(100);
        QCOMPARE(volume.count(), 2);

        // Another path that moves the engine's level is followed.
        rig.engine->setVolume(0.3f);
        QCOMPARE(rig.radio.coreSpeakerVolume(), 30);
        rig.engine->setMasterMuted(false);
        QVERIFY(!rig.radio.coreSpeakerMuted());
    }

    // A model that is not the Core's speaker host changes nothing.
    void notHost_settersDoNothing()
    {
        CoreRig rig;
        QVERIFY(!rig.radio.coreSpeakerAvailable());
        QVERIFY(!rig.radio.coreSpeakerNeedsNewerCore());
        rig.radio.setCoreSpeakerVolume(72);
        rig.radio.setCoreSpeakerMuted(true);
        rig.radio.setCoreSpeakerDevice(coreSpeakerDeviceToJson(kUsbId, kUsbName));
        QCOMPARE(rig.radio.coreSpeakerVolume(), 50);
        QVERIFY(!rig.radio.coreSpeakerMuted());
        QCOMPARE(saved("Master/Volume"), QStringLiteral("<unset>"));
        QCOMPARE(saved("Speakers/DeviceId"), QStringLiteral("<unset>"));
        QCOMPARE(rig.speakerOpens(), 0);
    }

    // D23: a card pick is saved with its engine and the speakers reopen
    // on it; the list is the Core's cards.
    void host_cardPickSavesAndReopens()
    {
        CoreRig rig;
        rig.radio.setCoreSpeakerHost(true);
        rig.engine->start();
        QTRY_VERIFY_WITH_TIMEOUT(rig.speakerOpens() >= 1, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(rig.state().kind, CoreSpeakerStateKind::Playing, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(rig.cards().size(), 2, kWaitMs);
        QCOMPARE(rig.cards().at(0), (CoreSpeakerCard{kUsbId, kUsbName, AudioDeviceState::Present}));
        QCOMPARE(rig.cards().at(1),
                 (CoreSpeakerCard{kBuiltInId, kBuiltInName, AudioDeviceState::Present}));
        QCOMPARE(rig.radio.coreSpeakerDevice(), QStringLiteral(R"({"id":"","name":""})"));
        QVERIFY(!rig.state().desktop);

        const int opensBefore = rig.speakerOpens();
        QSignalSpy device(&rig.radio, &RadioModel::coreSpeakerDeviceChanged);
        rig.radio.setCoreSpeakerDevice(QStringLiteral(R"({"id":"Device,0","name":"USB Audio Device"})"));
        QCOMPARE(saved("Speakers/Engine"), QStringLiteral("AlsaDirect"));
        QCOMPARE(saved("Speakers/DeviceId"), kUsbId);
        QCOMPARE(saved("Speakers/DeviceName"), kUsbName);
        QTRY_VERIFY_WITH_TIMEOUT(rig.speakerOpens() > opensBefore, kWaitMs);
        QCOMPARE(rig.lastOpenedId(), kUsbId);
        QCOMPARE(rig.radio.coreSpeakerDevice(), coreSpeakerDeviceToJson(kUsbId, kUsbName));
        QCOMPARE(device.count(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(rig.state().kind, CoreSpeakerStateKind::Playing, kWaitMs);
        QCOMPARE(rig.state().chosenName, kUsbName);

        // The Core's default: an empty id, saved as such, the default card.
        const int opensAtUsb = rig.speakerOpens();
        rig.radio.setCoreSpeakerDevice(QStringLiteral(R"({"id":"","name":""})"));
        QCOMPARE(saved("Speakers/DeviceId"), QString());
        QCOMPARE(saved("Speakers/DeviceName"), QString());
        QCOMPARE(saved("Speakers/Engine"), QStringLiteral("AlsaDirect"));
        QTRY_VERIFY_WITH_TIMEOUT(rig.speakerOpens() > opensAtUsb, kWaitMs);
        QCOMPARE(rig.radio.coreSpeakerDevice(), QStringLiteral(R"({"id":"","name":""})"));
        QTRY_COMPARE_WITH_TIMEOUT(rig.state().kind, CoreSpeakerStateKind::Playing, kWaitMs);
        QVERIFY(rig.state().chosenName.isEmpty());

        // A form that does not parse changes nothing.
        const int opensAtDefault = rig.speakerOpens();
        rig.radio.setCoreSpeakerDevice(QStringLiteral(R"({"id":"Device,0"})"));
        QCOMPARE(saved("Speakers/DeviceId"), QString());
        QCOMPARE(rig.speakerOpens(), opensAtDefault);
    }

    // D23: the chosen card unplugged stays in the list, not connected.
    void host_aMissingChosenCardStaysListed()
    {
        CoreRig rig;
        rig.radio.setCoreSpeakerHost(true);
        rig.engine->start();
        rig.radio.setCoreSpeakerDevice(coreSpeakerDeviceToJson(kUsbId, kUsbName));
        QTRY_COMPARE_WITH_TIMEOUT(rig.cards().size(), 2, kWaitMs);
        rig.alsa->removeDevice(kUsbId, AudioDeviceDirection::Output);
        rig.alsa->postNotice(AudioNotice::DevicesChanged);
        QTRY_VERIFY_WITH_TIMEOUT(rig.cards().size() == 2
                                     && rig.cards().last().id == kUsbId
                                     && rig.cards().last().state == AudioDeviceState::NotConnected,
                                 kWaitMs);
        QCOMPARE(rig.cards().first().id, kBuiltInId);
    }

    // The buffer, delay setting and rate Setup offers are saved and the
    // speakers reopen with them; a value it does not offer is left alone.
    void host_detailsSaveTheOfferedValues()
    {
        CoreRig rig;
        rig.radio.setCoreSpeakerHost(true);
        rig.engine->start();
        QTRY_VERIFY_WITH_TIMEOUT(rig.speakerOpens() >= 1, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(rig.state().kind, CoreSpeakerStateKind::Playing, kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(
            !coreSpeakerDetailsFromJson(rig.radio.coreSpeakerDetails()).value().negotiated.isEmpty(),
            kWaitMs);

        const int opensBefore = rig.speakerOpens();
        CoreSpeakerDetails asked;
        asked.bufferFrames = 256;
        asked.delayMs = 10;
        asked.sampleRate = 96000;
        rig.radio.setCoreSpeakerDetails(coreSpeakerDetailsToJson(asked));
        QCOMPARE(saved("Speakers/BufferSamples"), QStringLiteral("256"));
        QCOMPARE(saved("Speakers/DelayMs"), QStringLiteral("10"));
        QCOMPARE(saved("Speakers/SampleRate"), QStringLiteral("96000"));
        QTRY_VERIFY_WITH_TIMEOUT(rig.speakerOpens() > opensBefore, kWaitMs);
        const AudioStreamRequest reopened = rig.alsa->outputRequests().back();
        QCOMPARE(reopened.sampleRate, 96000);
        QCOMPARE(reopened.bufferFrames, 256);
        QCOMPARE(reopened.delayMs, 10);
        QTRY_VERIFY_WITH_TIMEOUT(
            coreSpeakerDetailsFromJson(rig.radio.coreSpeakerDetails()).value().sampleRate == 96000,
            kWaitMs);
        const CoreSpeakerDetails now = coreSpeakerDetailsFromJson(rig.radio.coreSpeakerDetails()).value();
        QCOMPARE(now.bufferFrames, 256);
        QCOMPARE(now.delayMs, 10);

        // 100 frames and 7 ms are not offered: nothing changes.
        const int opensAfter = rig.speakerOpens();
        CoreSpeakerDetails odd = asked;
        odd.bufferFrames = 100;
        odd.delayMs = 7;
        odd.sampleRate = 12345;
        rig.radio.setCoreSpeakerDetails(coreSpeakerDetailsToJson(odd));
        QCOMPARE(saved("Speakers/BufferSamples"), QStringLiteral("256"));
        QCOMPARE(saved("Speakers/DelayMs"), QStringLiteral("10"));
        QCOMPARE(saved("Speakers/SampleRate"), QStringLiteral("96000"));
        QTest::qWait(100);
        QCOMPARE(rig.speakerOpens(), opensAfter);
    }

    // ── R-AUD-28 ───────────────────────────────────────────────────────

    // The Core speaker's level and mute act on the Core's speakers alone:
    // the station's program, an owner's mix, a slice's audio and the
    // radio's output carry the same samples at 0 and muted as at 100 and
    // unmuted.
    void masterLevelAndMuteLeaveEveryTapAlone()
    {
        TapRig loud;
        loud.radio.setCoreSpeakerHost(true);
        loud.radio.setCoreSpeakerVolume(100);
        loud.radio.setCoreSpeakerMuted(false);
        QCOMPARE(loud.engine->volume(), 1.0f);
        QVERIFY(!loud.engine->masterMuted());

        TapRig quiet;
        quiet.radio.setCoreSpeakerHost(true);
        quiet.radio.setCoreSpeakerVolume(0);
        quiet.radio.setCoreSpeakerMuted(true);
        QCOMPARE(quiet.engine->volume(), 0.0f);
        QVERIFY(quiet.engine->masterMuted());

        const int quietPushesBefore = quiet.speakers->pushCount();
        loud.feed(4);
        quiet.feed(4);

        QCOMPARE(loud.master.received.size(), std::size_t(4 * kFrames * 2));
        QVERIFY(loud.master.received.back() != 0.0f);
        QCOMPARE(quiet.master.received, loud.master.received);
        QCOMPARE(loud.owner.received.size(), std::size_t(4 * kFrames * 2));
        QCOMPARE(quiet.owner.received, loud.owner.received);
        QCOMPARE(loud.sliceTap.received.size(), std::size_t(4 * kFrames * 2));
        QCOMPARE(quiet.sliceTap.received, loud.sliceTap.received);
        QCOMPARE(loud.radioOut.received.size(), std::size_t(4 * kFrames * 2));
        QVERIFY(loud.radioOut.received.back() != 0.0f);
        QCOMPARE(quiet.radioOut.received, loud.radioOut.received);

        // The speakers alone hear the difference.
        QVERIFY(loud.speakers->pushCount() > 0);
        QCOMPARE(quiet.speakers->pushCount(), quietPushesBefore);
    }

    // ── The desktop rule (R-AUD-30, D31) ───────────────────────────────

    // A box that starts into a desktop, with nothing saved and no
    // audio_device, waits for a pick: "(none)" in effect but not saved, no
    // card opened, waitingForPick with desktop true.  A pick then plays,
    // and the box still reports its desktop.
    void desktop_waitsForAPick()
    {
        CoreRig rig;
        rig.radio.setCoreSpeakerDesktop(/*desktop=*/true, /*configNamesDevice=*/false);
        rig.radio.setCoreSpeakerHost(true);
        rig.engine->start();
        QTRY_COMPARE_WITH_TIMEOUT(rig.state().kind, CoreSpeakerStateKind::WaitingForPick, kWaitMs);
        QVERIFY(rig.state().desktop);
        QCOMPARE(rig.engine->roleStatus(AudioRole::Speakers).state, AudioRoleState::WaitingForPick);
        QVERIFY(rig.engine->speakersWaitForPick());
        QCOMPARE(rig.radio.coreSpeakerDevice(), QStringLiteral(R"j({"id":"(none)","name":""})j"));
        // The list is there to pick from.
        QTRY_COMPARE_WITH_TIMEOUT(rig.cards().size(), 2, kWaitMs);
        QTest::qWait(200);
        QCOMPARE(rig.speakerOpens(), 0);
        QCOMPARE(saved("Speakers/DeviceId"), QStringLiteral("<unset>"));
        QCOMPARE(saved("Speakers/Engine"), QStringLiteral("<unset>"));

        rig.radio.setCoreSpeakerDevice(coreSpeakerDeviceToJson(kUsbId, kUsbName));
        QVERIFY(!rig.engine->speakersWaitForPick());
        QCOMPARE(saved("Speakers/DeviceId"), kUsbId);
        QCOMPARE(saved("Speakers/Engine"), QStringLiteral("AlsaDirect"));
        QTRY_COMPARE_WITH_TIMEOUT(rig.speakerOpens(), 1, kWaitMs);
        QCOMPARE(rig.lastOpenedId(), kUsbId);
        QTRY_COMPARE_WITH_TIMEOUT(rig.state().kind, CoreSpeakerStateKind::Playing, kWaitMs);
        QVERIFY(rig.state().desktop);
        QCOMPARE(rig.radio.coreSpeakerDevice(), coreSpeakerDeviceToJson(kUsbId, kUsbName));
    }

    // Settled call 12: while waiting, "(none)" picked is a pick too: it is
    // saved, the wait ends, and the speakers stay off.
    void desktop_noneIsAPick()
    {
        CoreRig rig;
        rig.radio.setCoreSpeakerDesktop(true, false);
        rig.radio.setCoreSpeakerHost(true);
        rig.engine->start();
        QTRY_COMPARE_WITH_TIMEOUT(rig.state().kind, CoreSpeakerStateKind::WaitingForPick, kWaitMs);
        rig.radio.setCoreSpeakerDevice(QStringLiteral(R"j({"id":"(none)","name":""})j"));
        QVERIFY(!rig.engine->speakersWaitForPick());
        QCOMPARE(saved("Speakers/DeviceId"), QStringLiteral("(none)"));
        QCOMPARE(saved("Speakers/Engine"), QStringLiteral("AlsaDirect"));
        QTRY_COMPARE_WITH_TIMEOUT(rig.state().kind, CoreSpeakerStateKind::NoCard, kWaitMs);
        QCOMPARE(rig.engine->roleStatus(AudioRole::Speakers).state, AudioRoleState::Off);
        QCOMPARE(rig.speakerOpens(), 0);
    }

    // While waiting, details are saved without an engine, so the box still
    // waits at its next start, and nothing opens.
    void desktop_detailsWhileWaitingOpenNothing()
    {
        CoreRig rig;
        rig.radio.setCoreSpeakerDesktop(true, false);
        rig.radio.setCoreSpeakerHost(true);
        rig.engine->start();
        QTRY_COMPARE_WITH_TIMEOUT(rig.state().kind, CoreSpeakerStateKind::WaitingForPick, kWaitMs);
        CoreSpeakerDetails asked;
        asked.bufferFrames = 512;
        asked.sampleRate = 48000;
        rig.radio.setCoreSpeakerDetails(coreSpeakerDetailsToJson(asked));
        QCOMPARE(saved("Speakers/BufferSamples"), QStringLiteral("512"));
        QCOMPARE(saved("Speakers/Engine"), QStringLiteral("<unset>"));
        QCOMPARE(saved("Speakers/DeviceId"), QStringLiteral("<unset>"));
        QTest::qWait(200);
        QCOMPARE(rig.speakerOpens(), 0);
        QCOMPARE(rig.state().kind, CoreSpeakerStateKind::WaitingForPick);
    }

    // A saved pick, or the config file's audio_device (settled call 11),
    // means no wait on a desktop box.
    void desktop_aSavedPickOrAudioDeviceMeansNoWait()
    {
        {
            AudioDeviceConfig cfg;
            cfg.engine = AudioEngineKind::AlsaDirect;
            cfg.deviceId = kUsbId;
            cfg.deviceName = kUsbName;
            cfg.saveToSettings(QStringLiteral("audio/Speakers"));
            CoreRig rig;
            rig.radio.setCoreSpeakerDesktop(true, false);
            rig.radio.setCoreSpeakerHost(true);
            QVERIFY(!rig.engine->speakersWaitForPick());
            rig.engine->start();
            QTRY_COMPARE_WITH_TIMEOUT(rig.speakerOpens(), 1, kWaitMs);
            QCOMPARE(rig.lastOpenedId(), kUsbId);
            QTRY_COMPARE_WITH_TIMEOUT(rig.state().kind, CoreSpeakerStateKind::Playing, kWaitMs);
            QVERIFY(rig.state().desktop);
        }
        AppSettings::instance().clear();
        {
            CoreRig rig;
            rig.radio.setCoreSpeakerDesktop(true, /*configNamesDevice=*/true);
            rig.radio.setCoreSpeakerHost(true);
            QVERIFY(!rig.engine->speakersWaitForPick());
            rig.engine->start();
            QTRY_VERIFY_WITH_TIMEOUT(rig.speakerOpens() >= 1, kWaitMs);
            QTRY_COMPARE_WITH_TIMEOUT(rig.state().kind, CoreSpeakerStateKind::Playing, kWaitMs);
        }
    }

    // A box without a desktop opens the Core's default card at start.
    // Task 16 fix round: the Core speaker list fills before a radio
    // connects, opening nothing, and the daemon's later audio_device seed
    // (DaemonApp::applyConfigToSettings) still applies at start.
    void listFillsBeforeStartAndTheSeedStillApplies()
    {
        CoreRig rig;
        rig.radio.setCoreSpeakerDesktop(false, true);
        rig.radio.setCoreSpeakerHost(true);
        QCOMPARE(rig.cards().size(), 2);
        QCOMPARE(rig.speakerOpens(), 0);
        auto& s = AppSettings::instance();
        QVERIFY(!s.contains(QStringLiteral("audio/Speakers/DeviceName")));
        QVERIFY(!s.contains(QStringLiteral("audio/Speakers/Engine")));
        s.setValue(QStringLiteral("audio/Speakers/DeviceName"), kUsbName);
        rig.engine->start();
        QTRY_COMPARE_WITH_TIMEOUT(rig.speakerOpens(), 1, kWaitMs);
        QCOMPARE(rig.lastOpenedId(), kUsbId);
    }

    void noDesktop_opensTheDefaultAtStart()
    {
        CoreRig rig;
        rig.radio.setCoreSpeakerDesktop(false, false);
        rig.radio.setCoreSpeakerHost(true);
        QVERIFY(!rig.engine->speakersWaitForPick());
        rig.engine->start();
        QTRY_COMPARE_WITH_TIMEOUT(rig.speakerOpens(), 1, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(rig.state().kind, CoreSpeakerStateKind::Playing, kWaitMs);
        QCOMPARE(rig.state().playingName, kBuiltInName);
        QVERIFY(!rig.state().desktop);
        QCOMPARE(saved("Speakers/DeviceId"), QStringLiteral("<unset>"));
    }
};

QTEST_MAIN(TstCoreSpeakerModel)
#include "tst_core_speaker_model.moc"
