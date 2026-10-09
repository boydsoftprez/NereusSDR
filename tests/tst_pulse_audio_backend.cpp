// =================================================================
// tests/tst_pulse_audio_backend.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test.  The PulseAudio engine's
// contract (native audio plan Task 11: R-AUD-01, R-AUD-02, R-AUD-07,
// R-AUD-14, R-AUD-31, R-AUD-32) with a fake system: sinks and sources to
// devices, monitors, pairs, the stream a device opens, the subscribe
// notices, the matcher output's fill, the selection the backend reports,
// the registry order and the test-run barrier, and (R-AUD-03) the system
// coming back after its server restarts, over a fake connection.  No sound
// server is reached: the real adapter never connects in a test run.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 11. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-09: Task 11 fix round 1 (R-AUD-03): reconnect cases over a
//               fake connection and server. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================
#ifdef NEREUS_HAVE_PULSEAUDIO

#include <QtTest/QtTest>

#include "core/audio/AudioBackendRegistry.h"
#include "core/audio/AudioDeviceCatalog.h"
#include "core/audio/AudioTestBarrier.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/PulseAudioBackend.h"
#include "core/audio/PulseAudioBus.h"
#include "core/audio/PulseAudioSystem.h"
#include "fakes/FakeAudioEngineBackend.h"
#include "fakes/FakeMatcherAudioBus.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace NereusSDR;

namespace {

const QStringList kStereoMap{QStringLiteral("front-left"), QStringLiteral("front-right")};

QStringList auxMap(int count)
{
    QStringList map;
    for (int i = 0; i < count; ++i) {
        map.append(QStringLiteral("aux%1").arg(i));
    }
    return map;
}

PulseDeviceRecord record(const QString& name, const QString& description, bool isSink,
                         const QStringList& map, const QString& bus = QString(),
                         bool isMonitor = false)
{
    PulseDeviceRecord r;
    r.name = name;
    r.description = description;
    r.isSink = isSink;
    r.isMonitor = isMonitor;
    r.channelMap = map;
    r.busProperty = bus;
    return r;
}

class FakeInputStream final : public IAudioInputStream {
public:
    bool open() override { return false; }
    void close() override {}
    bool isOpen() const override { return false; }
    QString errorString() const override { return {}; }
    int sampleRate() const override { return 48000; }
    std::optional<std::int64_t> inputLatencyNs() const override { return std::nullopt; }
    void setStreamEventSink(std::function<void(const AudioStreamEvent&)>) override {}
};

class FakeInputSink final : public IAudioInputSink {
public:
    void onInput(const float*, int, int, std::int64_t) override {}
};

// The system seam's fake: the test sets devices, defaults, the server's
// name and the running flag, posts notices and reads what each open was
// asked.
class FakePulseAudioSystem final : public IPulseAudioSystem {
public:
    struct OutputCall {
        PulseDeviceRecord device;
        AudioStreamRequest request;
    };
    struct InputCall {
        PulseDeviceRecord device;
        AudioStreamRequest request;
        MicChannelPick pick = MicChannelPick::Left;
        IAudioInputSink* sink = nullptr;
    };

    bool running() override { return isRunning.load(); }
    std::optional<QString> serverName() override { return name; }
    QList<PulseDeviceRecord> devices() override { return records; }
    QString defaultName(AudioDeviceDirection direction) override
    {
        return direction == AudioDeviceDirection::Output ? defaultSink : defaultSource;
    }
    void setNoticeSink(std::function<void(AudioNotice)> sink) override
    {
        std::lock_guard<std::mutex> lock(sinkMutex);
        noticeSink = std::move(sink);
    }
    std::unique_ptr<IAudioBus> createOutput(const PulseDeviceRecord& device,
                                            const AudioStreamRequest& request) override
    {
        outputs.push_back({device, request});
        return std::make_unique<FakeMatcherAudioBus>(request);
    }
    std::unique_ptr<IAudioInputStream> createInput(const PulseDeviceRecord& device,
                                                   const AudioStreamRequest& request,
                                                   MicChannelPick pick,
                                                   IAudioInputSink* sink) override
    {
        inputs.push_back({device, request, pick, sink});
        return std::make_unique<FakeInputStream>();
    }

    void post(AudioNotice notice)
    {
        std::function<void(AudioNotice)> sink;
        {
            std::lock_guard<std::mutex> lock(sinkMutex);
            sink = noticeSink;
        }
        if (sink) {
            sink(notice);
        }
    }

    std::atomic<bool> isRunning{true};
    std::optional<QString> name = QStringLiteral("pulseaudio");
    QList<PulseDeviceRecord> records;
    QString defaultSink;
    QString defaultSource;
    std::vector<OutputCall> outputs;
    std::vector<InputCall> inputs;
    std::mutex sinkMutex;
    std::function<void(AudioNotice)> noticeSink;
};

QList<PulseDeviceRecord> desktopRecords()
{
    PulseDeviceRecord speakers = record(QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo"),
                                        QStringLiteral("Built-in Audio Analog Stereo"), true,
                                        kStereoMap, QStringLiteral("pci"));
    speakers.alsaCard = 0;
    speakers.alsaDevice = 0;
    PulseDeviceRecord mic = record(QStringLiteral("alsa_input.pci-0000_00_1f.3.analog-stereo"),
                                   QStringLiteral("Built-in Audio Analog Stereo"), false,
                                   kStereoMap, QStringLiteral("pci"));
    mic.alsaCard = 0;
    mic.alsaDevice = 6;
    const PulseDeviceRecord monitor = record(
        QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo.monitor"),
        QStringLiteral("Monitor of Built-in Audio Analog Stereo"), false, kStereoMap,
        QStringLiteral("pci"), true);
    const PulseDeviceRecord headset = record(QStringLiteral("bluez_sink.AA_BB_CC_DD_EE_FF.a2dp_sink"),
                                             QStringLiteral("Headset"), true, kStereoMap,
                                             QStringLiteral("bluetooth"));
    PulseDeviceRecord ifaceOut = record(QStringLiteral("alsa_output.usb-Focusrite.multichannel-output"),
                                        QStringLiteral("Scarlett 18i8 Multichannel"), true, auxMap(10),
                                        QStringLiteral("usb"));
    ifaceOut.alsaCard = 2;
    ifaceOut.alsaDevice = 0;
    const PulseDeviceRecord ifaceIn = record(QStringLiteral("alsa_input.usb-Focusrite.multichannel-input"),
                                             QStringLiteral("Scarlett 18i8 Multichannel"), false,
                                             auxMap(8), QStringLiteral("usb"));
    const PulseDeviceRecord mono = record(QStringLiteral("alsa_input.usb-mic.mono-fallback"),
                                          QStringLiteral("USB Mic"), false,
                                          {QStringLiteral("mono")}, QStringLiteral("usb"));
    return {speakers, mic, monitor, headset, ifaceOut, ifaceIn, mono};
}

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

struct NoticeLog {
    std::mutex mutex;
    QList<AudioNotice> notices;
    std::function<void(AudioNotice)> sink()
    {
        return [this](AudioNotice notice) {
            std::lock_guard<std::mutex> lock(mutex);
            notices.append(notice);
        };
    }
    QList<AudioNotice> take()
    {
        std::lock_guard<std::mutex> lock(mutex);
        QList<AudioNotice> out = notices;
        notices.clear();
        return out;
    }
};


// A connection seam fake: running or not when made, listing the desktop's
// records while it runs; lose() is the server going away, as the real
// connection's context state callback does it: not running, the state
// cleared (one DevicesChanged), then the lost handler.
class FakePulseConnection final : public IPulseConnection {
public:
    explicit FakePulseConnection(bool up) : isRunning(up)
    {
        if (up) {
            records = desktopRecords();
            serverState.serverInfo(QStringLiteral("pulseaudio"),
                                   QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo"),
                                   QStringLiteral("alsa_input.pci-0000_00_1f.3.analog-stereo"));
        }
    }

    bool running() const override { return isRunning.load(); }
    PulseServerState& state() override { return serverState; }
    QList<PulseDeviceRecord> devices() override
    {
        return running() ? records : QList<PulseDeviceRecord>{};
    }
    void setLostHandler(std::function<void()> handler) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        lostHandler = std::move(handler);
    }
    std::unique_ptr<IAudioBus> createOutput(const PulseDeviceRecord&,
                                            const AudioStreamRequest& request) override
    {
        opens.fetch_add(1);
        return std::make_unique<FakeMatcherAudioBus>(request);
    }
    std::unique_ptr<IAudioInputStream> createInput(const PulseDeviceRecord&,
                                                   const AudioStreamRequest&, MicChannelPick,
                                                   IAudioInputSink*) override
    {
        opens.fetch_add(1);
        return std::make_unique<FakeInputStream>();
    }

    void lose()
    {
        isRunning.store(false);
        serverState.clear();
        std::function<void()> handler;
        {
            std::lock_guard<std::mutex> lock(mutex);
            handler = lostHandler;
        }
        if (handler) {
            handler();
        }
    }

    bool hasLostHandler()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return bool(lostHandler);
    }

    std::atomic<bool> isRunning;
    std::atomic<int> opens{0};
    PulseServerState serverState;
    QList<PulseDeviceRecord> records;
    std::mutex mutex;
    std::function<void()> lostHandler;
};

// A desktop whose server the test starts and stops.  hold() makes every
// try wait at a gate until release(), as a server that is up but hangs
// before it answers; no case sleeps for it.
struct FakePulseServer {
    std::atomic<bool> up{true};
    std::atomic<int> tries{0};
    std::atomic<int> heldTries{0};
    std::atomic<int> inFlight{0};
    std::atomic<int> maxInFlight{0};
    std::mutex gateMutex;
    std::condition_variable gate;
    bool holding = false;
    std::mutex mutex;
    std::vector<std::shared_ptr<FakePulseConnection>> made;

    // A try still on its worker uses this server: let it go and wait.
    ~FakePulseServer()
    {
        release();
        while (inFlight.load() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void hold()
    {
        std::lock_guard<std::mutex> lock(gateMutex);
        holding = true;
    }
    void release()
    {
        {
            std::lock_guard<std::mutex> lock(gateMutex);
            holding = false;
        }
        gate.notify_all();
    }

    PulseConnector connector()
    {
        return [this]() -> std::shared_ptr<IPulseConnection> {
            tries.fetch_add(1);
            const int now = inFlight.fetch_add(1) + 1;
            int seen = maxInFlight.load();
            while (now > seen && !maxInFlight.compare_exchange_weak(seen, now)) {
            }
            {
                std::unique_lock<std::mutex> lock(gateMutex);
                if (holding) {
                    heldTries.fetch_add(1);
                    gate.wait(lock, [this] { return !holding; });
                }
            }
            auto connection = std::make_shared<FakePulseConnection>(up.load());
            {
                std::lock_guard<std::mutex> lock(mutex);
                made.push_back(connection);
            }
            inFlight.fetch_sub(1);
            return connection;
        };
    }

    std::shared_ptr<FakePulseConnection> last()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return made.empty() ? nullptr : made.back();
    }
};

// Short in the test so the waits stay short; the real interval is
// kPulseReconnectIntervalMs.
constexpr int kTestRetryMs = 20;

// The system's own log lines, expected where a case makes them.
void expectLost()
{
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("PulseAudio went away")));
}
void expectBack()
{
    QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral("PulseAudio answers again")));
}
void expectNotRunning()
{
    QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral("PulseAudio is not running")));
}

int countOf(const QList<AudioNotice>& notices, AudioNotice notice)
{
    return int(std::count(notices.begin(), notices.end(), notice));
}

} // namespace

class TestPulseAudioBackend : public QObject {
    Q_OBJECT

private slots:
    // Sinks are outputs and sources inputs; monitors are not listed; the
    // name is the saved DeviceId and the description the display name;
    // device.bus gives Bluetooth and USB; ALSA carries its card; the map
    // gives the channel count.
    void recordsBecomeDevices()
    {
        auto system = std::make_unique<FakePulseAudioSystem>();
        system->records = desktopRecords();
        system->defaultSink = QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo");
        system->defaultSource = QStringLiteral("alsa_input.usb-Focusrite.multichannel-input");
        PulseAudioBackend backend(std::move(system));

        QCOMPARE(backend.id(), AudioBackendId::PulseAudio);
        const QList<AudioDeviceInfo> devices = backend.enumerate();
        QCOMPARE(devices.size(), 6);   // every record but the monitor

        const AudioDeviceInfo* speakers = find(devices,
            QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo"), AudioDeviceDirection::Output);
        QVERIFY(speakers != nullptr);
        QCOMPARE(speakers->backend, AudioBackendId::PulseAudio);
        QCOMPARE(speakers->name, QStringLiteral("Built-in Audio Analog Stereo"));
        QCOMPARE(speakers->channelCount, 2);
        QCOMPARE(speakers->alsaCard, 0);
        QCOMPARE(speakers->alsaDevice, 0);
        QCOMPARE(speakers->transport, AudioTransport::Unknown);
        QVERIFY(speakers->isDefault);
        QVERIFY(find(devices, speakers->id, AudioDeviceDirection::Input) == nullptr);

        const AudioDeviceInfo* mic = find(devices,
            QStringLiteral("alsa_input.pci-0000_00_1f.3.analog-stereo"), AudioDeviceDirection::Input);
        QVERIFY(mic != nullptr);
        QCOMPARE(mic->alsaDevice, 6);
        QVERIFY(!mic->isDefault);

        QVERIFY(find(devices, QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo.monitor"),
                     AudioDeviceDirection::Input) == nullptr);

        const AudioDeviceInfo* headset = find(devices,
            QStringLiteral("bluez_sink.AA_BB_CC_DD_EE_FF.a2dp_sink"), AudioDeviceDirection::Output);
        QVERIFY(headset != nullptr);
        QCOMPARE(headset->transport, AudioTransport::Bluetooth);
        QCOMPARE(headset->alsaCard, -1);

        const AudioDeviceInfo* ifaceOut = find(devices,
            QStringLiteral("alsa_output.usb-Focusrite.multichannel-output"), AudioDeviceDirection::Output);
        QVERIFY(ifaceOut != nullptr);
        QCOMPARE(ifaceOut->transport, AudioTransport::Usb);
        QCOMPARE(ifaceOut->channelCount, 10);
        QCOMPARE(ifaceOut->alsaCard, 2);
        const AudioDeviceInfo* ifaceIn = find(devices,
            QStringLiteral("alsa_input.usb-Focusrite.multichannel-input"), AudioDeviceDirection::Input);
        QVERIFY(ifaceIn != nullptr);
        QCOMPARE(ifaceIn->channelCount, 8);
        QVERIFY(ifaceIn->isDefault);

        const AudioDeviceInfo* mono = find(devices, QStringLiteral("alsa_input.usb-mic.mono-fallback"),
                                           AudioDeviceDirection::Input);
        QVERIFY(mono != nullptr);
        QCOMPARE(mono->channelCount, 1);
    }

    // R-AUD-07: pairs only where the map lists more than two channels.
    void pairsOnlyPastTwoChannels()
    {
        const QList<AudioDeviceInfo> devices = pulseDevicesFromRecords(
            {record(QStringLiteral("pro"), QStringLiteral("Interface"), true, auxMap(10)),
             record(QStringLiteral("stereo"), QStringLiteral("Stereo"), true, kStereoMap),
             record(QStringLiteral("bare"), QString(), true, {})},
            {}, {});
        QCOMPARE(devices.size(), 3);
        const QList<AudioChannelPair> pairs = audioChannelPairs(devices.at(0).channelCount);
        QCOMPARE(pairs.size(), 5);
        QCOMPARE(pairs.at(1), (AudioChannelPair{3, 2}));
        QCOMPARE(audioChannelPairs(devices.at(1).channelCount).size(), 1);
        // A device that reports no map plays as stereo, named by its name.
        QCOMPARE(devices.at(2).channelCount, 2);
        QCOMPARE(devices.at(2).name, QStringLiteral("bare"));
        QCOMPARE(audioChannelPairs(devices.at(2).channelCount).size(), 1);
    }

    void recordFromProperties()
    {
        const PulseDeviceRecord r = pulseDeviceRecordFrom(
            QStringLiteral("alsa_output.x"), QStringLiteral("Desk"), true, false, kStereoMap,
            {{QStringLiteral("device.bus"), QStringLiteral("usb")},
             {QStringLiteral("alsa.card"), QStringLiteral("1")},
             {QStringLiteral("alsa.device"), QStringLiteral("3")},
             {QStringLiteral("device.api"), QStringLiteral("alsa")}});
        QCOMPARE(r.name, QStringLiteral("alsa_output.x"));
        QCOMPARE(r.description, QStringLiteral("Desk"));
        QVERIFY(r.isSink);
        QVERIFY(!r.isMonitor);
        QCOMPARE(r.channelMap, kStereoMap);
        QCOMPARE(r.busProperty, QStringLiteral("usb"));
        QCOMPARE(r.alsaCard, 1);
        QCOMPARE(r.alsaDevice, 3);
        QVERIFY(pulseDeviceIsListed(r));

        const PulseDeviceRecord bare = pulseDeviceRecordFrom(
            QStringLiteral("bluez_source.x"), QString(), false, true, kStereoMap,
            {{QStringLiteral("alsa.card"), QStringLiteral("not a number")}});
        QCOMPARE(bare.alsaCard, -1);
        QCOMPARE(bare.alsaDevice, -1);
        QVERIFY(bare.busProperty.isEmpty());
        QVERIFY(!pulseDeviceIsListed(bare));   // a monitor
        QVERIFY(!pulseDeviceIsListed(PulseDeviceRecord{}));   // no name
    }

    void defaultsAndNoticesPassThrough()
    {
        auto owned = std::make_unique<FakePulseAudioSystem>();
        FakePulseAudioSystem* system = owned.get();
        PulseAudioBackend backend(std::move(owned));

        QCOMPARE(backend.defaultDeviceId(AudioDeviceDirection::Output), std::nullopt);
        system->defaultSink = QStringLiteral("sink-a");
        system->defaultSource = QStringLiteral("source-b");
        QCOMPARE(backend.defaultDeviceId(AudioDeviceDirection::Output),
                 std::optional<QString>(QStringLiteral("sink-a")));
        QCOMPARE(backend.defaultDeviceId(AudioDeviceDirection::Input),
                 std::optional<QString>(QStringLiteral("source-b")));

        NoticeLog log;
        backend.setNoticeSink(log.sink());
        system->post(AudioNotice::DevicesChanged);
        system->post(AudioNotice::DefaultOutputChanged);
        system->post(AudioNotice::DefaultInputChanged);
        QCOMPARE(log.take(), (QList<AudioNotice>{AudioNotice::DevicesChanged,
                                                 AudioNotice::DefaultOutputChanged,
                                                 AudioNotice::DefaultInputChanged}));
    }

    // The saved DeviceId is the name; an empty one opens the server
    // default's device; a device PulseAudio does not list (or a monitor, or
    // the wrong direction) opens nothing.
    void opensResolveTheirDevice()
    {
        auto owned = std::make_unique<FakePulseAudioSystem>();
        FakePulseAudioSystem* system = owned.get();
        system->records = desktopRecords();
        system->defaultSink = QStringLiteral("bluez_sink.AA_BB_CC_DD_EE_FF.a2dp_sink");
        PulseAudioBackend backend(std::move(owned));

        AudioStreamRequest named;
        named.deviceId = QStringLiteral("alsa_output.usb-Focusrite.multichannel-output");
        named.pair = AudioChannelPair{3, 2};
        QVERIFY(backend.createOutput(named) != nullptr);
        QCOMPARE(system->outputs.back().device.name, named.deviceId);
        QCOMPARE(system->outputs.back().request.pair, (AudioChannelPair{3, 2}));

        QVERIFY(backend.createOutput(AudioStreamRequest{}) != nullptr);
        QCOMPARE(system->outputs.back().device.name,
                 QStringLiteral("bluez_sink.AA_BB_CC_DD_EE_FF.a2dp_sink"));

        // No default known: an empty record, which follows the server default.
        system->defaultSink.clear();
        QVERIFY(backend.createOutput(AudioStreamRequest{}) != nullptr);
        QVERIFY(system->outputs.back().device.name.isEmpty());

        const std::size_t outputsBefore = system->outputs.size();
        AudioStreamRequest missing;
        missing.deviceId = QStringLiteral("gone");
        QTest::ignoreMessage(QtWarningMsg, "PulseAudio does not list the output \"gone\"");
        QVERIFY(backend.createOutput(missing) == nullptr);
        AudioStreamRequest wrongWay;
        wrongWay.deviceId = QStringLiteral("alsa_input.pci-0000_00_1f.3.analog-stereo");
        QTest::ignoreMessage(QtWarningMsg,
                             "PulseAudio does not list the output \"alsa_input.pci-0000_00_1f.3.analog-stereo\"");
        QVERIFY(backend.createOutput(wrongWay) == nullptr);
        QCOMPARE(system->outputs.size(), outputsBefore);

        FakeInputSink sink;
        AudioStreamRequest mic;
        mic.direction = AudioDeviceDirection::Input;
        mic.deviceId = QStringLiteral("alsa_input.pci-0000_00_1f.3.analog-stereo");
        QVERIFY(backend.createInput(mic, MicChannelPick::Right, &sink) != nullptr);
        QCOMPARE(system->inputs.back().device.name, mic.deviceId);
        QCOMPARE(system->inputs.back().pick, MicChannelPick::Right);
        QCOMPARE(system->inputs.back().sink, &sink);

        AudioStreamRequest monitor = mic;
        monitor.deviceId = QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo.monitor");
        QTest::ignoreMessage(QtWarningMsg,
                             "PulseAudio does not list the input \"alsa_output.pci-0000_00_1f.3.analog-stereo.monitor\"");
        QVERIFY(backend.createInput(monitor, MicChannelPick::Left, &sink) == nullptr);
    }

    // The stream a device opens: buffer frames to tlength and minreq (128
    // by default), the device's own map with no remix past two channels,
    // a named device never moved.
    void streamConfig()
    {
        const PulseDeviceRecord iface = record(QStringLiteral("alsa_output.usb-Focusrite.multichannel-output"),
                                               QStringLiteral("Scarlett"), true, auxMap(10));
        AudioStreamRequest request;
        request.pair = AudioChannelPair{3, 2};
        PulseStreamConfig config = pulseStreamConfig(iface, request);
        QCOMPARE(config.direction, AudioDeviceDirection::Output);
        QCOMPARE(config.deviceName, iface.name);
        QCOMPARE(config.rate, 48000);
        QCOMPARE(config.channels, 10);
        QCOMPARE(config.channelMap, auxMap(10));
        QVERIFY(config.noRemix);
        QVERIFY(config.dontMove);
        QCOMPARE(config.bufferFrames, kPulseDefaultBufferFrames);
        QCOMPARE(kPulseDefaultBufferFrames, 128);
        QCOMPARE(config.tlengthBytes, std::uint32_t(128 * 10 * sizeof(float)));
        QCOMPARE(config.minreqBytes, std::uint32_t(128 * 10 * sizeof(float)));
        QCOMPARE(config.fragsizeBytes, std::uint32_t(0));
        QCOMPARE(config.pair, (AudioChannelPair{3, 2}));

        request.bufferFrames = 256;
        request.sampleRate = 96000;
        config = pulseStreamConfig(record(QStringLiteral("s"), QString(), true, kStereoMap), request);
        QCOMPARE(config.channels, 2);
        QVERIFY(config.channelMap.isEmpty());
        QVERIFY(!config.noRemix);
        QCOMPARE(config.rate, 96000);
        QCOMPARE(config.tlengthBytes, std::uint32_t(256 * 2 * sizeof(float)));
        // A pair past the stream's channels plays on the first pair.
        QCOMPARE(config.pair, (AudioChannelPair{1, 2}));

        // The server default: may move with it.
        config = pulseStreamConfig(PulseDeviceRecord{}, AudioStreamRequest{});
        QVERIFY(config.deviceName.isEmpty());
        QVERIFY(!config.dontMove);
        QCOMPARE(config.channels, 2);

        AudioStreamRequest input;
        input.direction = AudioDeviceDirection::Input;
        config = pulseStreamConfig(record(QStringLiteral("m"), QString(), false,
                                          {QStringLiteral("mono")}),
                                   input);
        QCOMPARE(config.channels, 1);
        QCOMPARE(config.fragsizeBytes, std::uint32_t(128 * sizeof(float)));
        QCOMPARE(config.tlengthBytes, std::uint32_t(0));
        QCOMPARE(config.pair, (AudioChannelPair{1, 1}));
    }

    void fillWritesThePairOnly()
    {
        DeviceRateMatcher matcher(DeviceRateMatcher::Config{});
        QVERIFY(matcher.valid());
        MatcherReader reader = matcher.makeReader();
        constexpr int kWriteFrames = 64;
        constexpr int kFillFrames = 128;
        constexpr int kChannels = 10;
        std::vector<float> in(2 * kWriteFrames);
        for (int i = 0; i < kWriteFrames; ++i) {
            in[std::size_t(2 * i)] = 0.5f;
            in[std::size_t(2 * i + 1)] = -0.25f;
        }
        std::vector<float> scratch(2 * kPulseChunkFrames);
        std::vector<float> out(std::size_t(kFillFrames * kChannels), 1.0f);
        for (int i = 0; i < 80; ++i) {
            matcher.write(in.data(), kWriteFrames, std::int64_t(i) * 1'333'333LL);
            if (i % 2 == 1) {
                std::fill(out.begin(), out.end(), 1.0f);
                // A scratch smaller than the cycle reads in pieces.
                pulseFillFromMatcher(reader, scratch.data(), 48, out.data(), kFillFrames, kChannels,
                                     AudioChannelPair{3, 2});
            }
        }
        for (int f = 0; f < kFillFrames; ++f) {
            for (int c = 0; c < kChannels; ++c) {
                if (c == 2 || c == 3) {
                    continue;
                }
                QCOMPARE(out[std::size_t(f * kChannels + c)], 0.0f);
            }
        }
        const int last = (kFillFrames - 1) * kChannels;
        QVERIFY2(std::abs(out[std::size_t(last + 2)] - 0.5f) < 0.01f,
                 qPrintable(QString::number(out[std::size_t(last + 2)])));
        QVERIFY2(std::abs(out[std::size_t(last + 3)] + 0.25f) < 0.01f,
                 qPrintable(QString::number(out[std::size_t(last + 3)])));

        // No reader: silence on every channel.
        MatcherReader none;
        std::fill(out.begin(), out.end(), 1.0f);
        pulseFillFromMatcher(none, scratch.data(), kPulseChunkFrames, out.data(), kFillFrames,
                             kChannels, AudioChannelPair{3, 2});
        QVERIFY(std::all_of(out.begin(), out.end(), [](float v) { return v == 0.0f; }));
    }

    // The subscribe callback's logic: a sink or source that comes or goes
    // posts DevicesChanged; a server event asks for the server's info, and
    // a changed default sink or source in that answer posts its notice.
    void subscribeNotices()
    {
        PulseServerState state;
        NoticeLog log;
        state.setNoticeSink(log.sink());

        QVERIFY(!state.subscriptionEvent(PulseFacility::Sink, PulseEventType::New));
        QVERIFY(!state.subscriptionEvent(PulseFacility::Source, PulseEventType::Remove));
        QCOMPARE(log.take(), (QList<AudioNotice>{AudioNotice::DevicesChanged,
                                                 AudioNotice::DevicesChanged}));
        // A volume change changes no list.
        QVERIFY(!state.subscriptionEvent(PulseFacility::Sink, PulseEventType::Change));
        QVERIFY(!state.subscriptionEvent(PulseFacility::Other, PulseEventType::New));
        QVERIFY(log.take().isEmpty());

        QVERIFY(state.subscriptionEvent(PulseFacility::Server, PulseEventType::Change));
        QVERIFY(log.take().isEmpty());   // the answer posts, not the event

        // The first answer only records.
        QCOMPARE(state.serverName(), std::nullopt);
        state.serverInfo(QStringLiteral("pulseaudio"), QStringLiteral("sink-a"), QStringLiteral("source-a"));
        QVERIFY(log.take().isEmpty());
        QCOMPARE(state.serverName(), std::optional<QString>(QStringLiteral("pulseaudio")));
        QCOMPARE(state.defaultName(AudioDeviceDirection::Output), QStringLiteral("sink-a"));
        QCOMPARE(state.defaultName(AudioDeviceDirection::Input), QStringLiteral("source-a"));

        state.serverInfo(QStringLiteral("pulseaudio"), QStringLiteral("sink-a"), QStringLiteral("source-a"));
        QVERIFY(log.take().isEmpty());
        state.serverInfo(QStringLiteral("pulseaudio"), QStringLiteral("sink-b"), QStringLiteral("source-a"));
        QCOMPARE(log.take(), QList<AudioNotice>{AudioNotice::DefaultOutputChanged});
        state.serverInfo(QStringLiteral("pulseaudio"), QStringLiteral("sink-b"), QStringLiteral("source-b"));
        QCOMPARE(log.take(), QList<AudioNotice>{AudioNotice::DefaultInputChanged});
        QCOMPARE(state.defaultName(AudioDeviceDirection::Output), QStringLiteral("sink-b"));

        // The server goes away.
        state.clear();
        QCOMPARE(log.take(), QList<AudioNotice>{AudioNotice::DevicesChanged});
        QCOMPARE(state.serverName(), std::nullopt);
        QVERIFY(state.defaultName(AudioDeviceDirection::Output).isEmpty());
    }

    // running() reports the selection when the registry gives one, so the
    // catalogue shows the engine as not running (R-AUD-01) and the default
    // engine passes it by (R-AUD-02).
    void selectionDecidesRunning()
    {
        bool selected = false;
        auto owned = std::make_unique<FakePulseAudioSystem>();
        owned->records = desktopRecords();
        auto backend = std::make_shared<PulseAudioBackend>(std::move(owned),
                                                           [&selected] { return selected; });
        QVERIFY(!backend->running());   // the server answers, but is not selected
        QCOMPARE(defaultAudioEngine({backend}), AudioEngineKind::PortAudio);

        AudioDeviceCatalog catalogue({backend});
        catalogue.start();
        QCOMPARE(catalogue.backends(), QList<AudioBackendId>{AudioBackendId::PulseAudio});
        QVERIFY(!catalogue.backendRunning(AudioBackendId::PulseAudio));
        QVERIFY(catalogue.devices(AudioBackendId::PulseAudio, AudioDeviceDirection::Output).isEmpty());
        catalogue.stop();

        selected = true;
        QVERIFY(backend->running());
        QCOMPARE(defaultAudioEngine({backend}), AudioEngineKind::PulseAudio);

        // Without a selection: whether the server answers.
        auto stopped = std::make_unique<FakePulseAudioSystem>();
        stopped->isRunning.store(false);
        PulseAudioBackend plain(std::move(stopped));
        QVERIFY(!plain.running());

        // Selected (a forced value) but no server answers: not running
        // (R-AUD-03), so the default falls through to the older drivers.
        auto down = std::make_unique<FakePulseAudioSystem>();
        down->isRunning.store(false);
        auto forced = std::make_shared<PulseAudioBackend>(std::move(down), [] { return true; });
        QVERIFY(!forced->running());
        QCOMPARE(defaultAudioEngine({forced}), AudioEngineKind::PortAudio);
    }

    // R-AUD-02 on Linux: PipeWire, else PulseAudio, else PortAudio.
    void defaultEngineOrder()
    {
        auto pipeWire = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PipeWire);
        bool pulseSelected = true;
        auto pulse = std::make_shared<PulseAudioBackend>(std::make_unique<FakePulseAudioSystem>(),
                                                         [&pulseSelected] { return pulseSelected; });
        auto older = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PortAudio);
        older->setRunning(true);
        const std::vector<std::shared_ptr<IAudioEngineBackend>> backends{pipeWire, pulse, older};

        pipeWire->setRunning(true);
        QCOMPARE(defaultAudioEngine(backends), AudioEngineKind::PipeWire);
        pipeWire->setRunning(false);
        QCOMPARE(defaultAudioEngine(backends), AudioEngineKind::PulseAudio);
        pulseSelected = false;
        QCOMPARE(defaultAudioEngine(backends), AudioEngineKind::PortAudio);
    }

    // R-AUD-32: in a test run the real adapter never connects and every
    // open fails; its output still takes the stereo mix.
    void realAdapterIsBarredInATestRun()
    {
        QVERIFY(audioDevicesBarredForTestRun());
        std::unique_ptr<IPulseAudioSystem> system = makePulseAudioSystem();
        QVERIFY(!system->running());
        // Nor does it retry.
        auto* reconnecting = dynamic_cast<ReconnectingPulseAudioSystem*>(system.get());
        QVERIFY(reconnecting != nullptr);
        QVERIFY(!reconnecting->retrying());
        QCOMPARE(system->serverName(), std::nullopt);
        QVERIFY(system->devices().isEmpty());
        QVERIFY(system->defaultName(AudioDeviceDirection::Output).isEmpty());

        const PulseDeviceRecord stereo = record(QStringLiteral("alsa_output.analog-stereo"),
                                                QStringLiteral("Built-in"), true, kStereoMap);
        std::unique_ptr<IAudioBus> bus = system->createOutput(stereo, AudioStreamRequest{});
        QVERIFY(bus != nullptr);
        QVERIFY(bus->takesStereoMix());
        QVERIFY(!bus->open(AudioFormat{}));
        QVERIFY(!bus->isOpen());
        QVERIFY(!bus->errorString().isEmpty());
        QCOMPARE(bus->push(nullptr, 0), qint64(0));
        QVERIFY(bus->fadedOut());
        QCOMPARE(bus->backendName(), QStringLiteral("PulseAudio"));

        FakeInputSink sink;
        AudioStreamRequest micRequest;
        micRequest.direction = AudioDeviceDirection::Input;
        std::unique_ptr<IAudioInputStream> mic =
            system->createInput(stereo, micRequest, MicChannelPick::Both, &sink);
        QVERIFY(mic != nullptr);
        QVERIFY(!mic->open());
        QVERIFY(!mic->isOpen());
        QVERIFY(!mic->errorString().isEmpty());

        PulseAudioBackend backend(makePulseAudioSystem());
        QVERIFY(!backend.running());
        QVERIFY(backend.enumerate().isEmpty());
    }

    // R-AUD-03: the server goes away and comes back.  The list empties with
    // one DevicesChanged and the engine is not running, even when forced;
    // the system tries again on a timer on its own thread (never a busy
    // loop) while the server stays down; when it answers the devices are
    // listed again, DevicesChanged and the default notices let the
    // supervisor reopen them, with no restart.
    void reconnectsWhenTheServerReturns()
    {
        FakePulseServer server;
        auto owned = std::make_unique<ReconnectingPulseAudioSystem>(server.connector(), kTestRetryMs);
        ReconnectingPulseAudioSystem* system = owned.get();
        PulseAudioBackend backend(std::move(owned), [] { return true; });   // forced
        NoticeLog log;
        backend.setNoticeSink(log.sink());

        QVERIFY(backend.running());
        QVERIFY(!system->retrying());
        QCOMPARE(system->serverName(), std::optional<QString>(QStringLiteral("pulseaudio")));
        QCOMPARE(backend.enumerate().size(), desktopRecords().size() - 1);   // less the monitor
        QCOMPARE(server.tries.load(), 1);

        // A stream open on the first connection when the server goes.
        std::shared_ptr<FakePulseConnection> first = server.last();
        std::unique_ptr<IAudioBus> playing = backend.createOutput(AudioStreamRequest{});
        QVERIFY(playing != nullptr);
        QCOMPARE(first->opens.load(), 1);

        // The server stops: as the real context state callback does, off
        // this thread.
        server.up.store(false);
        expectLost();
        std::thread mainloopThread([first] { first->lose(); });
        mainloopThread.join();
        QVERIFY(!backend.running());
        QVERIFY(backend.enumerate().isEmpty());
        QCOMPARE(system->serverName(), std::nullopt);
        QVERIFY(system->defaultName(AudioDeviceDirection::Output).isEmpty());
        QCOMPARE(countOf(log.take(), AudioNotice::DevicesChanged), 1);

        // The retry is queued to this thread, then keeps trying on the
        // timer while the server stays down.
        QElapsedTimer down;
        down.start();
        QTRY_VERIFY(server.tries.load() >= 4);
        const qint64 downMs = down.elapsed();
        QVERIFY(system->retrying());
        QVERIFY(!backend.running());
        QVERIFY(backend.enumerate().isEmpty());
        QVERIFY(log.take().isEmpty());   // a failed try posts nothing
        // A timer, not a loop: no more tries than the interval allows (a
        // late timer under load only makes fewer).
        QVERIFY2(server.tries.load() <= 2 + int(down.elapsed() / kTestRetryMs) + 1,
                 qPrintable(QStringLiteral("%1 tries in %2 ms").arg(server.tries.load()).arg(downMs)));
        QCOMPARE(server.maxInFlight.load(), 1);

        // The server answers again.
        expectBack();
        server.up.store(true);
        QTRY_VERIFY(backend.running());
        QVERIFY(!system->retrying());
        QCOMPARE(backend.enumerate().size(), desktopRecords().size() - 1);
        QCOMPARE(system->serverName(), std::optional<QString>(QStringLiteral("pulseaudio")));
        QCOMPARE(system->defaultName(AudioDeviceDirection::Output),
                 QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo"));
        const QList<AudioNotice> notices = log.take();
        QCOMPARE(countOf(notices, AudioNotice::DevicesChanged), 1);
        QCOMPARE(countOf(notices, AudioNotice::DefaultOutputChanged), 1);
        QCOMPARE(countOf(notices, AudioNotice::DefaultInputChanged), 1);

        // The old connection is detached: it posts nothing and its loss no
        // longer reaches the system; its stream still holds it.
        QVERIFY(!first->hasLostHandler());
        first->serverState.subscriptionEvent(PulseFacility::Sink, PulseEventType::New);
        QVERIFY(log.take().isEmpty());

        // Opens go to the new connection.
        std::shared_ptr<FakePulseConnection> second = server.last();
        QVERIFY(second != first);
        QVERIFY(backend.createOutput(AudioStreamRequest{}) != nullptr);
        QCOMPARE(second->opens.load(), 1);
        QCOMPARE(first->opens.load(), 1);

        // Lost again: the cycle repeats.
        const int before = server.tries.load();
        server.up.store(false);
        expectLost();
        second->lose();
        QTRY_VERIFY(server.tries.load() > before);
        expectBack();
        server.up.store(true);
        QTRY_VERIFY(backend.running());
    }

    // No server at start: the system keeps trying and lists the devices
    // once it answers.
    void connectsWhenTheServerStartsLate()
    {
        FakePulseServer server;
        server.up.store(false);
        expectNotRunning();
        ReconnectingPulseAudioSystem system(server.connector(), kTestRetryMs);
        QVERIFY(!system.running());
        QCOMPARE(system.serverName(), std::nullopt);
        QVERIFY(system.retrying());
        QTRY_VERIFY(server.tries.load() >= 3);
        QVERIFY(!system.running());
        expectBack();
        server.up.store(true);
        QTRY_VERIFY(system.running());
        QCOMPARE(system.devices().size(), desktopRecords().size());
    }

    // stop() during a pending retry cancels it at once and the connector
    // is never called again; a late loss from the old connection does
    // nothing.
    void stopDuringARetryDoesNotReconnect()
    {
        FakePulseServer server;
        server.up.store(false);
        expectNotRunning();
        ReconnectingPulseAudioSystem system(server.connector(), kTestRetryMs);
        QTRY_VERIFY(server.tries.load() >= 2);
        QVERIFY(system.retrying());

        QElapsedTimer timer;
        timer.start();
        system.stop();
        const qint64 stopMs = timer.elapsed();
        QVERIFY2(stopMs < kPulseReconnectIntervalMs, qPrintable(QString::number(stopMs)));
        QVERIFY(!system.retrying());

        // The server comes back: nothing reconnects.
        const int after = server.tries.load();
        server.up.store(true);
        QTest::qWait(kTestRetryMs * 5);
        QCOMPARE(server.tries.load(), after);
        QVERIFY(!system.running());
        QVERIFY(!server.last()->hasLostHandler());
        server.last()->lose();   // detached: nothing happens
        QTest::qWait(kTestRetryMs * 2);
        QCOMPARE(server.tries.load(), after);
        system.stop();   // a second stop() is harmless
    }

    // A try against a server that hangs runs on a worker: this thread stays
    // free, one try runs at a time, and the try's connection is adopted
    // here when it ends.
    void aHungTryLeavesTheThreadFree()
    {
        FakePulseServer server;
        server.up.store(false);
        expectNotRunning();
        ReconnectingPulseAudioSystem system(server.connector(), kTestRetryMs);
        server.hold();
        server.up.store(true);
        QTRY_VERIFY(server.heldTries.load() >= 1);
        const int triesAtHold = server.tries.load();

        // This thread's event loop runs while the try hangs.
        bool fired = false;
        QTimer::singleShot(0, [&fired] { fired = true; });
        QTRY_VERIFY_WITH_TIMEOUT(fired, 100);
        QTest::qWait(kTestRetryMs * 5);   // timers fire; no second try starts
        QCOMPARE(server.inFlight.load(), 1);
        QCOMPARE(server.tries.load(), triesAtHold);
        QVERIFY(!system.running());
        QVERIFY(system.retrying());

        expectBack();
        server.release();
        QTRY_VERIFY(system.running());
        QCOMPARE(server.maxInFlight.load(), 1);
        QCOMPARE(system.devices().size(), desktopRecords().size());
        QVERIFY(server.last()->hasLostHandler());
    }

    // stop() during a hung try returns without waiting for it; the try's
    // connection, when it ends, is discarded, never adopted.  Destroying
    // the system during a hung try is the same.
    void stopDuringAHungTryDiscardsIt()
    {
        FakePulseServer server;
        server.up.store(false);
        expectNotRunning();
        auto system = std::make_unique<ReconnectingPulseAudioSystem>(server.connector(),
                                                                     kTestRetryMs);
        NoticeLog log;
        system->setNoticeSink(log.sink());
        server.hold();
        server.up.store(true);
        QTRY_VERIFY(server.heldTries.load() >= 1);

        QElapsedTimer timer;
        timer.start();
        system->stop();
        const qint64 stopMs = timer.elapsed();
        QVERIFY2(stopMs < kPulseReconnectIntervalMs, qPrintable(QString::number(stopMs)));
        QVERIFY(!system->retrying());
        QCOMPARE(server.inFlight.load(), 1);   // still hung: never waited for

        server.release();
        QTRY_VERIFY(server.inFlight.load() == 0);
        QTest::qWait(kTestRetryMs * 3);   // a report, were one queued, would run here
        QVERIFY(!system->running());
        QVERIFY(system->devices().isEmpty());
        std::shared_ptr<FakePulseConnection> late = server.last();
        QVERIFY(late->running());
        QVERIFY(!late->hasLostHandler());
        late->serverState.subscriptionEvent(PulseFacility::Source, PulseEventType::New);
        QVERIFY(log.take().isEmpty());
        const int triesAfter = server.tries.load();
        QTest::qWait(kTestRetryMs * 3);
        QCOMPARE(server.tries.load(), triesAfter);

        // Destroyed while a try hangs on its worker.
        server.up.store(false);
        expectNotRunning();
        auto doomed = std::make_unique<ReconnectingPulseAudioSystem>(server.connector(),
                                                                     kTestRetryMs);
        const int heldBefore = server.heldTries.load();
        server.hold();
        server.up.store(true);
        QTRY_VERIFY(server.heldTries.load() > heldBefore);
        timer.restart();
        doomed.reset();
        QVERIFY2(timer.elapsed() < kPulseReconnectIntervalMs,
                 qPrintable(QString::number(timer.elapsed())));
        server.release();
        QTRY_VERIFY(server.inFlight.load() == 0);
        QTest::qWait(kTestRetryMs * 3);   // the late report finds no system
    }

    // R-AUD-01 on Linux desktops: PipeWire then PulseAudio then the older
    // drivers, both native backends always registered; outside the Core
    // only.  In a test run neither answers, so the default stays PortAudio.
    void registryOrderOnLinux()
    {
        const auto window = makeSystemAudioBackends(AudioBackendContext{});
        QList<AudioBackendId> ids;
        for (const auto& backend : window) {
            ids.append(backend->id());
        }
#ifdef NEREUS_HAVE_PIPEWIRE
        const QList<AudioBackendId> expected{AudioBackendId::PipeWire,
                                                   AudioBackendId::PulseAudio,
                                                   AudioBackendId::PortAudio};
#else
        const QList<AudioBackendId> expected{AudioBackendId::PulseAudio,
                                                   AudioBackendId::PortAudio};
#endif
        QCOMPARE(ids, expected);
        for (std::size_t i = 0; i + 1 < window.size(); ++i) {
            QVERIFY(!window.at(i)->running());
        }
        QCOMPARE(defaultAudioEngine(window), AudioEngineKind::PortAudio);

        AudioBackendContext helper;
        helper.helper = true;
        QCOMPARE(makeSystemAudioBackends(helper).size(), std::size_t(expected.size()));

        AudioBackendContext daemon;
        daemon.daemon = true;
        const auto core = makeSystemAudioBackends(daemon);
        QCOMPARE(core.size(), std::size_t(1));
        QCOMPARE(core.front()->id(), AudioBackendId::PortAudio);
    }
};

QTEST_MAIN(TestPulseAudioBackend)
#include "tst_pulse_audio_backend.moc"

#else
int main() { return 0; }   // Built only with libpulse; this keeps a stray build quiet.
#endif
