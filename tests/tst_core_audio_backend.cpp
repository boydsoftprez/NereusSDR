// =================================================================
// tests/tst_core_audio_backend.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The Core Audio engine on the Mac
// (native audio plan Task 8; R-AUD-01, R-AUD-02, R-AUD-07, R-AUD-11,
// R-AUD-14, R-AUD-32): the transport mapping, the device list from a fake
// system (one entry per direction, states, defaults), the live default,
// the notices, the AUHAL channel maps, buffer frames and clock matcher,
// the routing of an open by UID, the test-run barrier on the real
// adapter, the capture clock, and the registry.
//
// A fake system only (FakeCoreAudioSystem).  The real adapter is made but
// never opens a device: its streams refuse in a test run, and the clock
// check reads clocks only.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 8 (R-AUD-01, R-AUD-02, R-AUD-07,
//               R-AUD-11, R-AUD-14, R-AUD-32). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: Task 8 fix (R-AUD-17): the capture clock follows a probe
//               clock that jumps an hour mid-stream within one callback.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: Task 8 merge (R-AUD-32): the real adapter lists nothing
//               in a test run. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QStandardPaths>

#include "core/audio/AudioBackendRegistry.h"
#include "core/audio/AudioDelayProbe.h"
#include "core/audio/AudioTestBarrier.h"
#include "core/audio/CoreAudioBackend.h"
#include "core/audio/CoreAudioInputStream.h"
#include "core/audio/CoreAudioOutputBus.h"
#include "core/audio/CoreAudioSystem.h"

#include "fakes/FakeCoreAudioSystem.h"

#ifdef Q_OS_MAC
#include <CoreAudio/HostTime.h>
#endif

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr std::uint32_t fourCc(const char (&code)[5])
{
    return (std::uint32_t(std::uint8_t(code[0])) << 24) | (std::uint32_t(std::uint8_t(code[1])) << 16)
           | (std::uint32_t(std::uint8_t(code[2])) << 8) | std::uint32_t(std::uint8_t(code[3]));
}

CoreAudioDeviceRecord record(std::uint32_t objectId, const QString& uid, const QString& name,
                             std::uint32_t transport, int outputs, int inputs)
{
    CoreAudioDeviceRecord r;
    r.objectId = objectId;
    r.uid = uid;
    r.name = name;
    r.transportType = transport;
    r.outputChannels = outputs;
    r.inputChannels = inputs;
    return r;
}

const QString kUsbUid = QStringLiteral("AppleUSBAudioEngine:Focus:10x8:1");
const QString kBuiltInUid = QStringLiteral("BuiltInSpeakerDevice");
const QString kMicUid = QStringLiteral("BuiltInMicrophoneDevice");

// A USB interface with 10 outputs and 8 inputs, the built-in speakers and
// the built-in mic.
struct Rig {
    FakeCoreAudioSystem* system = nullptr;   // owned by backend
    std::unique_ptr<CoreAudioBackend> backend;

    Rig()
    {
        auto fake = std::make_unique<FakeCoreAudioSystem>();
        system = fake.get();
        system->setRecords({record(40, kUsbUid, QStringLiteral("Focus 10x8"), fourCc("usb "), 10, 8),
                            record(50, kBuiltInUid, QStringLiteral("MacBook Pro Speakers"),
                                   fourCc("bltn"), 2, 0),
                            record(60, kMicUid, QStringLiteral("MacBook Pro Microphone"),
                                   fourCc("bltn"), 0, 1)});
        system->setDefault(AudioDeviceDirection::Output, 50);
        system->setDefault(AudioDeviceDirection::Input, 60);
        backend = std::make_unique<CoreAudioBackend>(std::move(fake));
    }
};

const AudioDeviceInfo* find(const QList<AudioDeviceInfo>& devices, const QString& id,
                            AudioDeviceDirection direction)
{
    for (const AudioDeviceInfo& info : devices) {
        if (info.id == id && info.direction == direction) {
            return &info;
        }
    }
    return nullptr;
}

// Scripted clocks for CoreAudioCaptureClock: host ticks are nanoseconds
// here, and the probe clock runs hours ahead of them, as on a Mac that
// has slept since boot.
std::atomic<std::int64_t> g_probeNs{0};
std::atomic<std::uint64_t> g_hostTicks{0};
std::int64_t scriptedProbeNow() { return g_probeNs.load(); }
std::uint64_t scriptedHostNow() { return g_hostTicks.load(); }
std::int64_t scriptedHostToNs(std::uint64_t ticks) { return static_cast<std::int64_t>(ticks); }

class NullSink final : public IAudioInputSink {
public:
    void onInput(const float*, int, int, std::int64_t) override {}
};

} // namespace

class TstCoreAudioBackend : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(QStandardPaths::isTestModeEnabled());
        QVERIFY(audioDevicesBarredForTestRun());
    }

    // R-AUD-14: AudioHardwareBase.h:607-624 (macOS SDK 27.0).
    void transportMapping()
    {
        QCOMPARE(coreAudioTransport(fourCc("bltn")), AudioTransport::BuiltIn);
        QCOMPARE(coreAudioTransport(fourCc("usb ")), AudioTransport::Usb);
        QCOMPARE(coreAudioTransport(fourCc("blue")), AudioTransport::Bluetooth);
        QCOMPARE(coreAudioTransport(fourCc("blea")), AudioTransport::Bluetooth);
        QCOMPARE(coreAudioTransport(fourCc("hdmi")), AudioTransport::Hdmi);
        QCOMPARE(coreAudioTransport(fourCc("dprt")), AudioTransport::Hdmi);
        QCOMPARE(coreAudioTransport(fourCc("virt")), AudioTransport::Virtual);
        QCOMPARE(coreAudioTransport(fourCc("pci ")), AudioTransport::Unknown);
        QCOMPARE(coreAudioTransport(fourCc("airp")), AudioTransport::Unknown);
        QCOMPARE(coreAudioTransport(fourCc("grup")), AudioTransport::Unknown);
        QCOMPARE(coreAudioTransport(0), AudioTransport::Unknown);
    }

    // One entry per direction a device has channels for.
    void enumerateGivesOneEntryPerDirection()
    {
        Rig rig;
        QVERIFY(rig.backend->running());
        QCOMPARE(rig.backend->id(), AudioBackendId::CoreAudio);
        const QList<AudioDeviceInfo> devices = rig.backend->enumerate();
        QCOMPARE(devices.size(), 4);

        const AudioDeviceInfo* usbOut = find(devices, kUsbUid, AudioDeviceDirection::Output);
        const AudioDeviceInfo* usbIn = find(devices, kUsbUid, AudioDeviceDirection::Input);
        QVERIFY(usbOut != nullptr);
        QVERIFY(usbIn != nullptr);
        QCOMPARE(usbOut->channelCount, 10);
        QCOMPARE(usbIn->channelCount, 8);
        QCOMPARE(usbOut->name, QStringLiteral("Focus 10x8"));
        QCOMPARE(usbOut->backend, AudioBackendId::CoreAudio);
        QCOMPARE(usbOut->transport, AudioTransport::Usb);
        QCOMPARE(usbOut->state, AudioDeviceState::Present);
        QVERIFY(!usbOut->isDefault);
        QVERIFY(!usbIn->isDefault);

        const AudioDeviceInfo* speakers = find(devices, kBuiltInUid, AudioDeviceDirection::Output);
        QVERIFY(speakers != nullptr);
        QCOMPARE(speakers->transport, AudioTransport::BuiltIn);
        QVERIFY(speakers->isDefault);
        QVERIFY(find(devices, kBuiltInUid, AudioDeviceDirection::Input) == nullptr);

        const AudioDeviceInfo* mic = find(devices, kMicUid, AudioDeviceDirection::Input);
        QVERIFY(mic != nullptr);
        QCOMPARE(mic->channelCount, 1);
        QVERIFY(mic->isDefault);
        QVERIFY(find(devices, kMicUid, AudioDeviceDirection::Output) == nullptr);
    }

    // R-AUD-11: hog mode held by another process reads InUse; our own hog
    // and none read Present; a device that is not alive reads NotConnected.
    void statesFollowHogAndAlive()
    {
        Rig rig;
        rig.system->setOwnPid(4242);
        CoreAudioDeviceRecord otherHog = record(70, QStringLiteral("hog-other"),
                                                QStringLiteral("Hogged"), fourCc("usb "), 2, 0);
        otherHog.hogPid = 777;
        CoreAudioDeviceRecord ownHog = record(71, QStringLiteral("hog-own"),
                                              QStringLiteral("Ours"), fourCc("usb "), 2, 0);
        ownHog.hogPid = 4242;
        CoreAudioDeviceRecord gone = record(72, QStringLiteral("gone"),
                                            QStringLiteral("Unplugged"), fourCc("usb "), 2, 2);
        gone.alive = false;
        CoreAudioDeviceRecord goneAndHogged = gone;
        goneAndHogged.objectId = 73;
        goneAndHogged.uid = QStringLiteral("gone-hogged");
        goneAndHogged.hogPid = 777;
        rig.system->setRecords({otherHog, ownHog, gone, goneAndHogged});

        const QList<AudioDeviceInfo> devices = rig.backend->enumerate();
        QCOMPARE(find(devices, QStringLiteral("hog-other"), AudioDeviceDirection::Output)->state,
                 AudioDeviceState::InUse);
        QCOMPARE(find(devices, QStringLiteral("hog-own"), AudioDeviceDirection::Output)->state,
                 AudioDeviceState::Present);
        QCOMPARE(find(devices, QStringLiteral("gone"), AudioDeviceDirection::Output)->state,
                 AudioDeviceState::NotConnected);
        QCOMPARE(find(devices, QStringLiteral("gone"), AudioDeviceDirection::Input)->state,
                 AudioDeviceState::NotConnected);
        QCOMPARE(find(devices, QStringLiteral("gone-hogged"), AudioDeviceDirection::Output)->state,
                 AudioDeviceState::NotConnected);
    }

    // The default is asked live, mapped to the UID, without a re-list
    // when the device is known; a new device is listed once to find it.
    void defaultDeviceIdIsLive()
    {
        Rig rig;
        rig.backend->enumerate();
        const int listed = rig.system->deviceCalls();
        QCOMPARE(rig.backend->defaultDeviceId(AudioDeviceDirection::Output),
                 std::optional<QString>(kBuiltInUid));
        QCOMPARE(rig.backend->defaultDeviceId(AudioDeviceDirection::Input),
                 std::optional<QString>(kMicUid));

        rig.system->setDefault(AudioDeviceDirection::Output, 40);
        QCOMPARE(rig.backend->defaultDeviceId(AudioDeviceDirection::Output),
                 std::optional<QString>(kUsbUid));
        QCOMPARE(rig.system->deviceCalls(), listed);

        QList<CoreAudioDeviceRecord> records = rig.system->devices();
        records.append(record(90, QStringLiteral("airpods-uid"), QStringLiteral("AirPods"),
                              fourCc("blue"), 2, 1));
        rig.system->setRecords(records);
        rig.system->setDefault(AudioDeviceDirection::Output, 90);
        const int before = rig.system->deviceCalls();
        QCOMPARE(rig.backend->defaultDeviceId(AudioDeviceDirection::Output),
                 std::optional<QString>(QStringLiteral("airpods-uid")));
        QCOMPARE(rig.system->deviceCalls(), before + 1);

        rig.system->setDefault(AudioDeviceDirection::Output, std::nullopt);
        QCOMPARE(rig.backend->defaultDeviceId(AudioDeviceDirection::Output), std::nullopt);
        rig.system->setDefault(AudioDeviceDirection::Output, 999);
        QCOMPARE(rig.backend->defaultDeviceId(AudioDeviceDirection::Output), std::nullopt);
    }

    // The system's notices reach the catalogue's sink unchanged.
    void noticesReachTheSink()
    {
        Rig rig;
        QVERIFY(!rig.system->hasNoticeSink());
        std::vector<AudioNotice> seen;
        rig.backend->setNoticeSink([&seen](AudioNotice notice) { seen.push_back(notice); });
        QVERIFY(rig.system->hasNoticeSink());
        rig.system->postNotice(AudioNotice::DevicesChanged);
        rig.system->postNotice(AudioNotice::DefaultOutputChanged);
        rig.system->postNotice(AudioNotice::DefaultInputChanged);
        QCOMPARE(seen, std::vector<AudioNotice>({AudioNotice::DevicesChanged,
                                                 AudioNotice::DefaultOutputChanged,
                                                 AudioNotice::DefaultInputChanged}));
    }

    // kAudioOutputUnitProperty_ChannelMap for an output and an input.
    void channelMaps()
    {
        QCOMPARE(coreAudioOutputChannelMap(10, AudioChannelPair{3, 2}),
                 std::vector<std::int32_t>({-1, -1, 0, 1, -1, -1, -1, -1, -1, -1}));
        QCOMPARE(coreAudioOutputChannelMap(2, AudioChannelPair{1, 2}),
                 std::vector<std::int32_t>({0, 1}));
        QCOMPARE(coreAudioOutputChannelMap(3, AudioChannelPair{3, 1}),
                 std::vector<std::int32_t>({-1, -1, 0}));
        QVERIFY(coreAudioOutputChannelMap(2, AudioChannelPair{2, 2}).empty());
        QVERIFY(coreAudioOutputChannelMap(10, AudioChannelPair{0, 2}).empty());
        QVERIFY(coreAudioOutputChannelMap(0, AudioChannelPair{1, 2}).empty());

        QCOMPARE(coreAudioInputChannelMap(8, AudioChannelPair{5, 2}),
                 std::vector<std::int32_t>({4, 5}));
        QCOMPARE(coreAudioInputChannelMap(1, AudioChannelPair{1, 1}),
                 std::vector<std::int32_t>({0, 0}));
        QVERIFY(coreAudioInputChannelMap(8, AudioChannelPair{8, 2}).empty());
        QVERIFY(coreAudioInputChannelMap(1, AudioChannelPair{1, 2}).empty());
    }

    // The request's frames, or 128 when 0, clamped to the device's range;
    // the matcher as the brief gives it.
    void bufferFramesAndMatcher()
    {
        QCOMPARE(kCoreAudioDefaultBufferFrames, 128);
        QCOMPARE(coreAudioBufferFrames(0, 14.0, 4096.0), 128);
        QCOMPARE(coreAudioBufferFrames(256, 14.0, 4096.0), 256);
        QCOMPARE(coreAudioBufferFrames(8, 14.0, 4096.0), 14);
        QCOMPARE(coreAudioBufferFrames(8192, 14.0, 4096.0), 4096);
        QCOMPARE(coreAudioBufferFrames(0, 256.0, 4096.0), 256);
        QCOMPARE(coreAudioBufferFrames(64, 0.0, 0.0), 64);   // no range read

        const DeviceRateMatcher::Config cfg = coreAudioMatcherConfig(44100, 256, 30);
        QCOMPARE(cfg.inRate, 48000);
        QCOMPARE(cfg.outRate, 44100);
        QCOMPARE(cfg.writeBlockFrames, 64);
        QCOMPARE(cfg.callbackFrames, 256);
        QCOMPARE(cfg.delayMs, 30);

        QCOMPARE(coreAudioFramesToNs(480, 48000.0), std::int64_t(10'000'000));
        QCOMPARE(coreAudioFramesToNs(480, 0.0), std::int64_t(0));
    }

    // An open goes to the record with the request's UID, or to the
    // system default when the id is empty; the pick reaches the input.
    void opensRouteByUid()
    {
        Rig rig;
        AudioStreamRequest out;
        out.deviceId = kUsbUid;
        out.pair = AudioChannelPair{3, 2};
        out.bufferFrames = 256;
        out.delayMs = 40;
        QVERIFY(rig.backend->createOutput(out) != nullptr);
        AudioStreamRequest defaultOut;
        QVERIFY(rig.backend->createOutput(defaultOut) != nullptr);
        const auto outputs = rig.system->outputs();
        QCOMPARE(outputs.size(), std::size_t(2));
        QCOMPARE(outputs[0].record.objectId, std::uint32_t(40));
        QCOMPARE(outputs[0].request.pair, (AudioChannelPair{3, 2}));
        QCOMPARE(outputs[0].request.bufferFrames, 256);
        QCOMPARE(outputs[0].request.delayMs, 40);
        QCOMPARE(outputs[0].request.direction, AudioDeviceDirection::Output);
        QCOMPARE(outputs[1].record.objectId, std::uint32_t(50));

        NullSink sink;
        AudioStreamRequest in;
        in.direction = AudioDeviceDirection::Input;
        in.deviceId = kUsbUid;
        in.pair = AudioChannelPair{5, 2};
        QVERIFY(rig.backend->createInput(in, MicChannelPick::Right, &sink) != nullptr);
        AudioStreamRequest defaultIn;
        QVERIFY(rig.backend->createInput(defaultIn, MicChannelPick::Left, &sink) != nullptr);
        const auto inputs = rig.system->inputs();
        QCOMPARE(inputs.size(), std::size_t(2));
        QCOMPARE(inputs[0].record.objectId, std::uint32_t(40));
        QCOMPARE(inputs[0].pick, MicChannelPick::Right);
        QCOMPARE(inputs[0].request.direction, AudioDeviceDirection::Input);
        QCOMPARE(inputs[1].record.objectId, std::uint32_t(60));

        // No such device, or no channels in that direction: nothing made.
        AudioStreamRequest missing;
        missing.deviceId = QStringLiteral("no-such-uid");
        QVERIFY(rig.backend->createOutput(missing) == nullptr);
        AudioStreamRequest micAsOutput;
        micAsOutput.deviceId = kMicUid;
        QVERIFY(rig.backend->createOutput(micAsOutput) == nullptr);
        AudioStreamRequest speakersAsInput;
        speakersAsInput.deviceId = kBuiltInUid;
        QVERIFY(rig.backend->createInput(speakersAsInput, MicChannelPick::Left, &sink) == nullptr);
        QCOMPARE(rig.system->outputs().size(), std::size_t(2));
        QCOMPARE(rig.system->inputs().size(), std::size_t(2));
    }

    // R-AUD-32: the real adapter's streams refuse to open in a test run.
    void realAdapterRefusesInATestRun()
    {
#ifdef Q_OS_MAC
        std::unique_ptr<ICoreAudioSystem> system = makeCoreAudioSystem();
        QVERIFY(system != nullptr);
        // It walks no device and reports no default in a test run.
        QVERIFY(system->devices().isEmpty());
        QCOMPARE(system->defaultDevice(AudioDeviceDirection::Output), std::nullopt);
        QCOMPARE(system->defaultDevice(AudioDeviceDirection::Input), std::nullopt);
        const CoreAudioDeviceRecord fakeDevice =
            record(0, QStringLiteral("never-opened"), QStringLiteral("Never opened"), fourCc("usb "), 2, 2);
        AudioStreamRequest request;
        request.deviceId = fakeDevice.uid;
        std::unique_ptr<IAudioBus> bus = system->createOutput(fakeDevice, request);
        QVERIFY(bus != nullptr);
        QVERIFY(bus->takesStereoMix());
        QCOMPARE(bus->audioWorkgroupDevice(), std::uint32_t(0));
        QVERIFY(!bus->open(AudioFormat{}));
        QVERIFY(!bus->isOpen());
        QCOMPARE(bus->errorString(), coreAudioTestRunError());

        NullSink sink;
        request.direction = AudioDeviceDirection::Input;
        std::unique_ptr<IAudioInputStream> input =
            system->createInput(fakeDevice, request, MicChannelPick::Left, &sink);
        QVERIFY(input != nullptr);
        QVERIFY(!input->open());
        QVERIFY(!input->isOpen());
        QCOMPARE(input->errorString(), coreAudioTestRunError());
#else
        QSKIP("The Core Audio adapter is the Mac's");
#endif
    }

    // R-AUD-07: a host time mapped with the offset the stream measures at
    // open lands on audioProbeNowNs()'s clock within 1 ms.  The raw
    // difference between the two clocks is logged, not asserted: it is
    // what the offset is for.  No device is opened.
    void captureClockAgreesWithTheProbe()
    {
#ifdef Q_OS_MAC
        constexpr std::int64_t kOneMsNs = 1'000'000;
        const std::int64_t offset = coreAudioHostClockOffsetNs();
        for (int i = 0; i < 100; ++i) {
            const std::int64_t before = audioProbeNowNs();
            const std::uint64_t host = AudioGetCurrentHostTime();
            const std::int64_t after = audioProbeNowNs();
            const std::int64_t mapped = coreAudioHostTimeToProbeNs(host, offset);
            QVERIFY2(mapped >= before - kOneMsNs && mapped <= after + kOneMsNs,
                     qPrintable(QStringLiteral("mapped %1 outside [%2, %3] by more than 1 ms")
                                    .arg(mapped).arg(before).arg(after)));
        }
        qInfo("Core Audio host clock minus probe clock: %lld ns", static_cast<long long>(-offset));
#else
        QSKIP("The Core Audio host clock is the Mac's");
#endif
    }

    // R-AUD-17: the Mac sleeps an hour with the mic open (the probe clock
    // jumps, the host clock does not).  The next callback's capture time
    // is on the new probe clock: no capture time keeps the old offset.
    void captureClockFollowsAJumpWithinOneCallback()
    {
        constexpr std::int64_t kHourNs = 3'600'000'000'000;
        constexpr std::int64_t kBlockNs = 10'000'000;      // a 480-frame block at 48 kHz
        constexpr std::int64_t kLatencyNs = 2'000'000;
        constexpr std::int64_t kHostDelayNs = 3'000'000;   // block stamp before the callback runs
        const CoreAudioCaptureClock clock{&scriptedProbeNow, &scriptedHostNow, &scriptedHostToNs};

        g_hostTicks = 1'000'000'000;
        g_probeNs = 13 * kHourNs + 1'000'000'000;
        auto callback = [&]() {
            const std::uint64_t stamp = g_hostTicks.load() - kHostDelayNs;
            return clock.captureNs(true, stamp, kLatencyNs);
        };
        auto advance = [](std::int64_t ns) {
            g_hostTicks += static_cast<std::uint64_t>(ns);
            g_probeNs += ns;
        };

        // Before the sleep: frame 0 is the stamp on the probe clock less
        // the latency, block after block.
        for (int i = 0; i < 5; ++i) {
            QCOMPARE(callback(), g_probeNs.load() - kHostDelayNs - kLatencyNs);
            advance(kBlockNs);
        }

        // The sleep: an hour on the probe clock only.
        g_probeNs += kHourNs;
        advance(kBlockNs);
        const std::int64_t first = callback();
        QCOMPARE(first, g_probeNs.load() - kHostDelayNs - kLatencyNs);
        advance(kBlockNs);
        QCOMPARE(callback() - first, kBlockNs);

        // A block with no valid host time is stamped now.
        QCOMPARE(clock.captureNs(false, 0, kLatencyNs), g_probeNs.load() - kLatencyNs);
        QCOMPARE(clock.offsetNs(), g_probeNs.load() - static_cast<std::int64_t>(g_hostTicks.load()));
    }

    // R-AUD-01, R-AUD-02: the Mac registers Core Audio only, for every
    // process, and it is the default engine.
    void registryOnTheMacIsCoreAudioOnly()
    {
#ifdef Q_OS_MAC
        const std::vector<AudioBackendContext> contexts = {
            AudioBackendContext{}, AudioBackendContext{true, false}, AudioBackendContext{false, true}};
        for (const AudioBackendContext& context : contexts) {
            const auto backends = makeSystemAudioBackends(context);
            QCOMPARE(backends.size(), std::size_t(1));
            QCOMPARE(backends.front()->id(), AudioBackendId::CoreAudio);
            QVERIFY(backends.front()->running());
            QCOMPARE(defaultAudioEngine(backends), AudioEngineKind::CoreAudio);
        }
#else
        QSKIP("The Mac's registry");
#endif
    }
};

QTEST_GUILESS_MAIN(TstCoreAudioBackend)
#include "tst_core_audio_backend.moc"
