// =================================================================
// tests/tst_pipewire_device_backend.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test.  The PipeWire engine's contract
// (native audio plan Task 10: R-AUD-01, R-AUD-02, R-AUD-03, R-AUD-07,
// R-AUD-14, R-AUD-32) with a fake system: nodes to devices, pairs, the
// stream a pair opens, the registry listener's notices, the matcher
// output's fill, the not-running engine, reconnecting when the daemon
// returns and the test-run barrier.  No
// sound server is reached: the real adapter never connects in a test run.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 10. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-09: reconnect cases (Task 10 fix round 1). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================
#ifdef NEREUS_HAVE_PIPEWIRE

#include <QtTest/QtTest>

#include <pipewire/keys.h>
#include <pipewire/pipewire.h>

#include "core/audio/AudioBackendRegistry.h"
#include "core/audio/AudioDeviceCatalog.h"
#include "core/audio/AudioTestBarrier.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/PipeWireDeviceBackend.h"
#include "core/audio/PipeWireDeviceSystem.h"
#include "core/audio/PipeWireStream.h"
#include "fakes/FakeMatcherAudioBus.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace NereusSDR;

namespace {

PipeWireNodeRecord node(std::uint32_t id, const QString& name, const QString& description,
                        const QString& mediaClass, const QString& api, const QStringList& positions)
{
    PipeWireNodeRecord record;
    record.id = id;
    record.nodeName = name;
    record.description = description;
    record.mediaClass = mediaClass;
    record.deviceApi = api;
    record.positions = positions;
    return record;
}

QStringList auxPositions(int count)
{
    QStringList positions;
    for (int i = 0; i < count; ++i) {
        positions.append(QStringLiteral("AUX%1").arg(i));
    }
    return positions;
}

const QStringList kStereo{QStringLiteral("FL"), QStringLiteral("FR")};

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

// The system seam's fake: the test sets nodes, defaults and the running
// flag, posts notices and reads what each open was asked.
class FakePipeWireDeviceSystem final : public IPipeWireDeviceSystem {
public:
    struct OutputCall {
        PipeWireNodeRecord node;
        AudioStreamRequest request;
    };
    struct InputCall {
        PipeWireNodeRecord node;
        AudioStreamRequest request;
        MicChannelPick pick = MicChannelPick::Left;
        IAudioInputSink* sink = nullptr;
    };

    bool running() override { return isRunning.load(); }
    QList<PipeWireNodeRecord> nodes() override { return nodeList; }
    QString defaultNodeName(AudioDeviceDirection direction) override
    {
        return direction == AudioDeviceDirection::Output ? defaultSink : defaultSource;
    }
    void setNoticeSink(std::function<void(AudioNotice)> sink) override
    {
        std::lock_guard<std::mutex> lock(sinkMutex);
        noticeSink = std::move(sink);
    }
    std::unique_ptr<IAudioBus> createOutput(const PipeWireNodeRecord& n,
                                            const AudioStreamRequest& request) override
    {
        outputs.push_back({n, request});
        return std::make_unique<FakeMatcherAudioBus>(request);
    }
    std::unique_ptr<IAudioInputStream> createInput(const PipeWireNodeRecord& n,
                                                   const AudioStreamRequest& request,
                                                   MicChannelPick pick,
                                                   IAudioInputSink* sink) override
    {
        inputs.push_back({n, request, pick, sink});
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
    QList<PipeWireNodeRecord> nodeList;
    QString defaultSink;
    QString defaultSource;
    std::vector<OutputCall> outputs;
    std::vector<InputCall> inputs;
    std::mutex sinkMutex;
    std::function<void(AudioNotice)> noticeSink;
};

QList<PipeWireNodeRecord> desktopNodes()
{
    PipeWireNodeRecord speakers = node(31, QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo"),
                                       QStringLiteral("Built-in Audio Analog Stereo"),
                                       QStringLiteral("Audio/Sink"), QStringLiteral("alsa"), kStereo);
    speakers.alsaCard = 0;
    speakers.alsaDevice = 0;
    PipeWireNodeRecord mic = node(32, QStringLiteral("alsa_input.pci-0000_00_1f.3.analog-stereo"),
                                  QStringLiteral("Built-in Audio Analog Stereo"),
                                  QStringLiteral("Audio/Source"), QStringLiteral("alsa"), kStereo);
    mic.alsaCard = 0;
    mic.alsaDevice = 6;
    PipeWireNodeRecord headset = node(40, QStringLiteral("bluez_output.AA_BB_CC_DD_EE_FF.1"),
                                      QStringLiteral("Headset"), QStringLiteral("Audio/Sink"),
                                      QStringLiteral("bluez5"), kStereo);
    PipeWireNodeRecord iface = node(50, QStringLiteral("alsa_card.usb-Focusrite.pro-audio"),
                                    QStringLiteral("Scarlett 18i8"), QStringLiteral("Audio/Duplex"),
                                    QStringLiteral("alsa"), auxPositions(10));
    iface.alsaCard = 2;
    iface.alsaDevice = 0;
    const PipeWireNodeRecord monitor = node(60, QStringLiteral("alsa_output.pci.analog-stereo.monitor"),
                                            QStringLiteral("Monitor of Built-in"),
                                            QStringLiteral("Audio/Source"), QStringLiteral("alsa"),
                                            kStereo);
    const PipeWireNodeRecord stream = node(70, QStringLiteral("Firefox"), QStringLiteral("Firefox"),
                                           QStringLiteral("Stream/Output/Audio"), QString(), kStereo);
    return {speakers, mic, headset, iface, monitor, stream};
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

QHash<QString, QString> sinkProps(const QString& name, const QString& description)
{
    return {{QStringLiteral("node.name"), name},
            {QStringLiteral("node.description"), description},
            {QStringLiteral("media.class"), QStringLiteral("Audio/Sink")},
            {QStringLiteral("device.api"), QStringLiteral("alsa")},
            {QStringLiteral("audio.position"), QStringLiteral("FL,FR")}};
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

// A connection seam fake: running or not when made, its directory filled
// by the test, lose() as the daemon going away.
class FakePipeWireConnection final : public IPipeWireConnection {
public:
    explicit FakePipeWireConnection(bool up) : isRunning(up) {}

    bool running() const override { return isRunning.load(); }
    PipeWireNodeDirectory& directory() override { return dir; }
    void setLostHandler(std::function<void()> handler) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        lostHandler = std::move(handler);
    }
    std::unique_ptr<IAudioBus> createOutput(const PipeWireNodeRecord&,
                                            const AudioStreamRequest& request) override
    {
        return std::make_unique<FakeMatcherAudioBus>(request);
    }
    std::unique_ptr<IAudioInputStream> createInput(const PipeWireNodeRecord&,
                                                   const AudioStreamRequest&, MicChannelPick,
                                                   IAudioInputSink*) override
    {
        return std::make_unique<FakeInputStream>();
    }

    // As the real connection does on -EPIPE, from another thread.
    void lose()
    {
        isRunning.store(false);
        dir.clear();
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
    PipeWireNodeDirectory dir;
    std::mutex mutex;
    std::function<void()> lostHandler;
};

// A desktop whose daemon the test starts and stops.  Every connection it
// makes while up lists a sink and a source.
struct FakeServer {
    std::atomic<bool> up{true};
    std::atomic<int> tries{0};
    std::mutex mutex;
    std::vector<std::shared_ptr<FakePipeWireConnection>> made;

    PipeWireConnector connector()
    {
        return [this]() -> std::shared_ptr<IPipeWireConnection> {
            tries.fetch_add(1);
            auto connection = std::make_shared<FakePipeWireConnection>(up.load());
            if (connection->running()) {
                connection->dir.nodeProperties(31, sinkProps(QStringLiteral("alsa_output.analog-stereo"),
                                                             QStringLiteral("Built-in")));
                QHash<QString, QString> source = sinkProps(QStringLiteral("alsa_input.analog-stereo"),
                                                           QStringLiteral("Built-in Mic"));
                source.insert(QStringLiteral("media.class"), QStringLiteral("Audio/Source"));
                connection->dir.nodeProperties(32, source);
            }
            std::lock_guard<std::mutex> lock(mutex);
            made.push_back(connection);
            return connection;
        };
    }

    std::shared_ptr<FakePipeWireConnection> last()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return made.empty() ? nullptr : made.back();
    }
};

// Short in the test so the waits stay short; the real interval is
// kPipeWireReconnectIntervalMs.
constexpr int kTestRetryMs = 20;

// The system's own log lines, expected where a case makes them.
void expectLost()
{
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("PipeWire went away")));
}
void expectBack()
{
    QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral("PipeWire answers again")));
}
void expectNotRunning()
{
    QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral("PipeWire is not running")));
}

QString prop(pw_properties* p, const char* key)
{
    const char* value = pw_properties_get(p, key);
    return value ? QString::fromUtf8(value) : QString();
}

} // namespace

class TestPipeWireDeviceBackend : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { pw_init(nullptr, nullptr); }
    void cleanupTestCase() { pw_deinit(); }

    // Sinks are outputs, sources inputs, duplex both; monitors and program
    // streams are not listed; bluez5 is Bluetooth; ALSA carries its card.
    void nodesBecomeDevices()
    {
        auto system = std::make_unique<FakePipeWireDeviceSystem>();
        system->nodeList = desktopNodes();
        system->defaultSink = QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo");
        system->defaultSource = QStringLiteral("alsa_card.usb-Focusrite.pro-audio");
        PipeWireDeviceBackend backend(std::move(system));

        QCOMPARE(backend.id(), AudioBackendId::PipeWire);
        const QList<AudioDeviceInfo> devices = backend.enumerate();
        QCOMPARE(devices.size(), 5);   // speakers, mic, headset, interface twice

        const AudioDeviceInfo* speakers = find(devices,
            QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo"), AudioDeviceDirection::Output);
        QVERIFY(speakers != nullptr);
        QCOMPARE(speakers->backend, AudioBackendId::PipeWire);
        QCOMPARE(speakers->name, QStringLiteral("Built-in Audio Analog Stereo"));
        QCOMPARE(speakers->channelCount, 2);
        QCOMPARE(speakers->alsaCard, 0);
        QCOMPARE(speakers->alsaDevice, 0);
        QVERIFY(speakers->isDefault);
        QCOMPARE(speakers->transport, AudioTransport::Unknown);
        QVERIFY(find(devices, speakers->id, AudioDeviceDirection::Input) == nullptr);

        const AudioDeviceInfo* mic = find(devices,
            QStringLiteral("alsa_input.pci-0000_00_1f.3.analog-stereo"), AudioDeviceDirection::Input);
        QVERIFY(mic != nullptr);
        QCOMPARE(mic->alsaDevice, 6);
        QVERIFY(!mic->isDefault);

        const AudioDeviceInfo* headset = find(devices,
            QStringLiteral("bluez_output.AA_BB_CC_DD_EE_FF.1"), AudioDeviceDirection::Output);
        QVERIFY(headset != nullptr);
        QCOMPARE(headset->transport, AudioTransport::Bluetooth);
        QCOMPARE(headset->alsaCard, -1);

        const QString ifaceId = QStringLiteral("alsa_card.usb-Focusrite.pro-audio");
        const AudioDeviceInfo* ifaceOut = find(devices, ifaceId, AudioDeviceDirection::Output);
        const AudioDeviceInfo* ifaceIn = find(devices, ifaceId, AudioDeviceDirection::Input);
        QVERIFY(ifaceOut != nullptr);
        QVERIFY(ifaceIn != nullptr);
        QCOMPARE(ifaceOut->channelCount, 10);
        QCOMPARE(ifaceIn->channelCount, 10);
        QVERIFY(!ifaceOut->isDefault);
        QVERIFY(ifaceIn->isDefault);
        QCOMPARE(ifaceOut->alsaCard, 2);

        QVERIFY(find(devices, QStringLiteral("alsa_output.pci.analog-stereo.monitor"),
                     AudioDeviceDirection::Input) == nullptr);
        QVERIFY(find(devices, QStringLiteral("Firefox"), AudioDeviceDirection::Output) == nullptr);
    }

    // R-AUD-07: AUX0 to AUX9 gives ten channels, offered as five pairs.
    void auxNodeOffersItsPairs()
    {
        const QList<AudioDeviceInfo> devices = pipeWireDevicesFromNodes(
            {node(5, QStringLiteral("pro"), QStringLiteral("Interface"), QStringLiteral("Audio/Sink"),
                  QStringLiteral("alsa"), auxPositions(10))},
            {}, {});
        QCOMPARE(devices.size(), 1);
        QCOMPARE(devices.front().channelCount, 10);
        const QList<AudioChannelPair> pairs = audioChannelPairs(devices.front().channelCount);
        QCOMPARE(pairs.size(), 5);
        QCOMPARE(pairs.at(1), (AudioChannelPair{3, 2}));
        // A node that reports no positions plays as stereo.
        const QList<AudioDeviceInfo> bare = pipeWireDevicesFromNodes(
            {node(6, QStringLiteral("bare"), QString(), QStringLiteral("Audio/Sink"), QString(), {})},
            {}, {});
        QCOMPARE(bare.front().channelCount, 2);
        QCOMPARE(bare.front().name, QStringLiteral("bare"));
    }

    void defaultsAndNoticesPassThrough()
    {
        auto owned = std::make_unique<FakePipeWireDeviceSystem>();
        FakePipeWireDeviceSystem* system = owned.get();
        PipeWireDeviceBackend backend(std::move(owned));

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

    // The saved DeviceId is the node.name; an empty one opens the system
    // default's node; a node PipeWire does not list opens nothing.
    void opensResolveTheirNode()
    {
        auto owned = std::make_unique<FakePipeWireDeviceSystem>();
        FakePipeWireDeviceSystem* system = owned.get();
        system->nodeList = desktopNodes();
        system->defaultSink = QStringLiteral("bluez_output.AA_BB_CC_DD_EE_FF.1");
        system->defaultSource = QStringLiteral("alsa_input.pci-0000_00_1f.3.analog-stereo");
        PipeWireDeviceBackend backend(std::move(owned));

        AudioStreamRequest request;
        request.deviceId = QStringLiteral("alsa_card.usb-Focusrite.pro-audio");
        request.pair = AudioChannelPair{3, 2};
        std::unique_ptr<IAudioBus> bus = backend.createOutput(request);
        QVERIFY(bus != nullptr);
        QCOMPARE(system->outputs.size(), std::size_t(1));
        QCOMPARE(system->outputs.back().node.id, std::uint32_t(50));
        QCOMPARE(system->outputs.back().request.pair, (AudioChannelPair{3, 2}));

        request.deviceId.clear();
        QVERIFY(backend.createOutput(request) != nullptr);
        QCOMPARE(system->outputs.back().node.nodeName, QStringLiteral("bluez_output.AA_BB_CC_DD_EE_FF.1"));

        // The default is not listed: the stream follows the system default.
        system->defaultSink = QStringLiteral("gone");
        QVERIFY(backend.createOutput(request) != nullptr);
        QVERIFY(system->outputs.back().node.nodeName.isEmpty());

        // A source is not an output.
        request.deviceId = QStringLiteral("alsa_input.pci-0000_00_1f.3.analog-stereo");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("does not list the output")));
        QVERIFY(backend.createOutput(request) == nullptr);
        QCOMPARE(system->outputs.size(), std::size_t(3));

        FakeInputSink sink;
        AudioStreamRequest micRequest;
        micRequest.direction = AudioDeviceDirection::Input;
        micRequest.deviceId = QStringLiteral("alsa_card.usb-Focusrite.pro-audio");
        micRequest.pair = AudioChannelPair{5, 2};
        QVERIFY(backend.createInput(micRequest, MicChannelPick::Right, &sink) != nullptr);
        QCOMPARE(system->inputs.size(), std::size_t(1));
        QCOMPARE(system->inputs.back().node.id, std::uint32_t(50));
        QCOMPARE(system->inputs.back().pick, MicChannelPick::Right);
        QVERIFY(system->inputs.back().sink == &sink);

        micRequest.deviceId.clear();
        QVERIFY(backend.createInput(micRequest, MicChannelPick::Left, &sink) != nullptr);
        QCOMPARE(system->inputs.back().node.id, std::uint32_t(32));

        micRequest.deviceId = QStringLiteral("bluez_output.AA_BB_CC_DD_EE_FF.1");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("does not list the input")));
        QVERIFY(backend.createInput(micRequest, MicChannelPick::Left, &sink) == nullptr);
        QCOMPARE(system->inputs.size(), std::size_t(2));
    }

    // R-AUD-07: pair 3-4 of a ten-channel node opens all ten positions,
    // without remix, on the node; node.latency is the buffer (design
    // choice 11); a stereo node opens as today.
    void pairStreamConfig()
    {
        const PipeWireNodeRecord iface = node(50, QStringLiteral("alsa_card.usb-Focusrite.pro-audio"),
                                              QStringLiteral("Scarlett"), QStringLiteral("Audio/Duplex"),
                                              QStringLiteral("alsa"), auxPositions(10));
        AudioStreamRequest request;
        request.deviceId = iface.nodeName;
        request.pair = AudioChannelPair{3, 2};
        const StreamConfig cfg = pipeWireDeviceStreamConfig(iface, request, AudioDeviceDirection::Output);
        QCOMPARE(cfg.direction, StreamConfig::Output);
        QCOMPARE(cfg.channels, 10u);
        QCOMPARE(cfg.audioPosition, auxPositions(10));
        QVERIFY(cfg.dontRemix);
        QCOMPARE(cfg.targetNodeName, iface.nodeName);
        QCOMPARE(cfg.pair, (AudioChannelPair{3, 2}));
        QCOMPARE(cfg.quantum, 128u);
        QCOMPARE(cfg.rate, 48000u);

        pw_properties* p = configToProperties(cfg);
        QCOMPARE(prop(p, PW_KEY_TARGET_OBJECT), iface.nodeName);
        QCOMPARE(prop(p, "stream.dont-remix"), QStringLiteral("true"));
        QCOMPARE(prop(p, "audio.position"),
                 QStringLiteral("AUX0,AUX1,AUX2,AUX3,AUX4,AUX5,AUX6,AUX7,AUX8,AUX9"));
        QCOMPARE(prop(p, PW_KEY_AUDIO_CHANNELS), QStringLiteral("10"));
        QCOMPARE(prop(p, PW_KEY_NODE_LATENCY), QStringLiteral("128/48000"));
        QCOMPARE(prop(p, PW_KEY_MEDIA_CLASS), QStringLiteral("Stream/Output/Audio"));
        pw_properties_free(p);

        const PipeWireNodeRecord stereo = node(31, QStringLiteral("alsa_output.analog-stereo"),
                                               QStringLiteral("Built-in"), QStringLiteral("Audio/Sink"),
                                               QStringLiteral("alsa"), kStereo);
        request.deviceId = stereo.nodeName;
        request.pair = AudioChannelPair{1, 2};
        request.bufferFrames = 256;
        const StreamConfig two = pipeWireDeviceStreamConfig(stereo, request, AudioDeviceDirection::Output);
        QCOMPARE(two.channels, 2u);
        QVERIFY(two.audioPosition.isEmpty());
        QVERIFY(!two.dontRemix);
        QCOMPARE(two.quantum, 256u);
        p = configToProperties(two);
        QVERIFY(pw_properties_get(p, "stream.dont-remix") == nullptr);
        QCOMPARE(prop(p, "audio.position"), QStringLiteral("FL,FR"));
        QCOMPARE(prop(p, PW_KEY_NODE_LATENCY), QStringLiteral("256/48000"));
        pw_properties_free(p);

        // An input on the interface's pair 5-6.
        request.deviceId = iface.nodeName;
        request.pair = AudioChannelPair{5, 2};
        request.bufferFrames = 0;
        const StreamConfig in = pipeWireDeviceStreamConfig(iface, request, AudioDeviceDirection::Input);
        QCOMPARE(in.direction, StreamConfig::Input);
        QCOMPARE(in.mediaClass, QStringLiteral("Stream/Input/Audio"));
        QCOMPARE(in.channels, 10u);
        QVERIFY(in.dontRemix);
        QCOMPARE(in.pair, (AudioChannelPair{5, 2}));

        // The system default: no target, stereo.
        const StreamConfig def = pipeWireDeviceStreamConfig(PipeWireNodeRecord{}, AudioStreamRequest{},
                                                            AudioDeviceDirection::Output);
        QVERIFY(def.targetNodeName.isEmpty());
        QCOMPARE(def.channels, 2u);
        p = configToProperties(def);
        QVERIFY(pw_properties_get(p, PW_KEY_TARGET_OBJECT) == nullptr);
        pw_properties_free(p);
    }

    // The matcher output mode's fill: the pair carries the stereo, every
    // other position is exactly zero; a stereo stream is plain stereo.
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
        std::vector<float> scratch(2 * kPipeWireDeviceChunkFrames);
        std::vector<float> out(std::size_t(kFillFrames * kChannels), 1.0f);
        for (int i = 0; i < 80; ++i) {
            matcher.write(in.data(), kWriteFrames, std::int64_t(i) * 1'333'333LL);
            if (i % 2 == 1) {
                std::fill(out.begin(), out.end(), 1.0f);
                // A scratch smaller than the cycle reads in pieces.
                pipeWireFillFromMatcher(reader, scratch.data(), 48, out.data(), kFillFrames,
                                        kChannels, AudioChannelPair{3, 2});
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
        pipeWireFillFromMatcher(none, scratch.data(), kPipeWireDeviceChunkFrames, out.data(),
                                kFillFrames, kChannels, AudioChannelPair{3, 2});
        QVERIFY(std::all_of(out.begin(), out.end(), [](float v) { return v == 0.0f; }));
    }

    void recordFromProperties()
    {
        QHash<QString, QString> props = sinkProps(QStringLiteral("alsa_output.x"), QStringLiteral("Desk"));
        props.insert(QStringLiteral("api.alsa.pcm.card"), QStringLiteral("1"));
        props.insert(QStringLiteral("api.alsa.pcm.device"), QStringLiteral("3"));
        std::optional<PipeWireNodeRecord> record = pipeWireNodeRecordFromProperties(7, props);
        QVERIFY(record.has_value());
        QCOMPARE(record->id, std::uint32_t(7));
        QCOMPARE(record->nodeName, QStringLiteral("alsa_output.x"));
        QCOMPARE(record->description, QStringLiteral("Desk"));
        QCOMPARE(record->mediaClass, QStringLiteral("Audio/Sink"));
        QCOMPARE(record->deviceApi, QStringLiteral("alsa"));
        QCOMPARE(record->positions, kStereo);
        QCOMPARE(record->alsaCard, 1);
        QCOMPARE(record->alsaDevice, 3);

        props.insert(QStringLiteral("audio.position"), QStringLiteral("[ AUX0, AUX1, AUX2 ]"));
        record = pipeWireNodeRecordFromProperties(7, props);
        QCOMPARE(record->positions, auxPositions(3));

        props.remove(QStringLiteral("audio.position"));
        props.insert(QStringLiteral("audio.channels"), QStringLiteral("4"));
        record = pipeWireNodeRecordFromProperties(7, props);
        QCOMPARE(record->positions, auxPositions(4));
        props.insert(QStringLiteral("audio.channels"), QStringLiteral("1"));
        record = pipeWireNodeRecordFromProperties(7, props);
        QCOMPARE(record->positions, QStringList{QStringLiteral("MONO")});

        props.remove(QStringLiteral("node.name"));
        QVERIFY(!pipeWireNodeRecordFromProperties(7, props).has_value());

        QCOMPARE(pipeWireMetadataDefaultName(QStringLiteral("{\"name\":\"alsa_output.x\"}")),
                 QStringLiteral("alsa_output.x"));
        QVERIFY(pipeWireMetadataDefaultName(QStringLiteral("alsa_output.x")).isEmpty());
        QVERIFY(pipeWireMetadataDefaultName(QString()).isEmpty());
    }

    // R-AUD-03: the registry listener posts DevicesChanged when a device
    // node comes, changes or goes, and the default notices when the
    // "default" metadata's defaults change; nothing else posts.
    void directoryPostsNotices()
    {
        PipeWireNodeDirectory directory;
        NoticeLog log;
        directory.setNoticeSink(log.sink());

        directory.nodeProperties(31, sinkProps(QStringLiteral("alsa_output.a"), QStringLiteral("Desk")));
        QCOMPARE(log.take(), QList<AudioNotice>{AudioNotice::DevicesChanged});
        QCOMPARE(directory.nodes().size(), 1);
        // The same properties again (a node info that changed nothing listed).
        directory.nodeProperties(31, sinkProps(QStringLiteral("alsa_output.a"), QStringLiteral("Desk")));
        QVERIFY(log.take().isEmpty());
        directory.nodeProperties(31, sinkProps(QStringLiteral("alsa_output.a"), QStringLiteral("Desk 2")));
        QCOMPARE(log.take(), QList<AudioNotice>{AudioNotice::DevicesChanged});
        QCOMPARE(directory.nodes().front().description, QStringLiteral("Desk 2"));

        // A program stream and a monitor are not device nodes.
        QHash<QString, QString> stream = sinkProps(QStringLiteral("Firefox"), QStringLiteral("Firefox"));
        stream.insert(QStringLiteral("media.class"), QStringLiteral("Stream/Output/Audio"));
        directory.nodeProperties(70, stream);
        QHash<QString, QString> monitor = sinkProps(QStringLiteral("alsa_output.a.monitor"), QString());
        monitor.insert(QStringLiteral("media.class"), QStringLiteral("Audio/Source"));
        directory.nodeProperties(71, monitor);
        QVERIFY(log.take().isEmpty());
        directory.nodeRemoved(70);
        QVERIFY(log.take().isEmpty());
        QCOMPARE(directory.nodes().size(), 1);

        directory.nodeProperties(32, sinkProps(QStringLiteral("alsa_output.b"), QStringLiteral("USB")));
        QCOMPARE(log.take(), QList<AudioNotice>{AudioNotice::DevicesChanged});
        directory.nodeRemoved(31);
        QCOMPARE(log.take(), QList<AudioNotice>{AudioNotice::DevicesChanged});
        QCOMPARE(directory.nodes().size(), 1);
        QCOMPARE(directory.nodes().front().nodeName, QStringLiteral("alsa_output.b"));

        // The defaults.
        directory.metadataProperty(0, QStringLiteral("default.audio.sink"), false,
                                   QStringLiteral("{\"name\":\"alsa_output.b\"}"));
        QCOMPARE(log.take(), QList<AudioNotice>{AudioNotice::DefaultOutputChanged});
        QCOMPARE(directory.defaultNodeName(AudioDeviceDirection::Output), QStringLiteral("alsa_output.b"));
        directory.metadataProperty(0, QStringLiteral("default.audio.sink"), false,
                                   QStringLiteral("{\"name\":\"alsa_output.b\"}"));
        QVERIFY(log.take().isEmpty());
        directory.metadataProperty(0, QStringLiteral("default.audio.source"), false,
                                   QStringLiteral("{\"name\":\"alsa_input.c\"}"));
        QCOMPARE(log.take(), QList<AudioNotice>{AudioNotice::DefaultInputChanged});
        QCOMPARE(directory.defaultNodeName(AudioDeviceDirection::Input), QStringLiteral("alsa_input.c"));
        // Another subject, and a key that is not a default, post nothing.
        directory.metadataProperty(42, QStringLiteral("default.audio.sink"), false,
                                   QStringLiteral("{\"name\":\"other\"}"));
        directory.metadataProperty(0, QStringLiteral("default.configured.audio.sink"), false,
                                   QStringLiteral("{\"name\":\"other\"}"));
        QVERIFY(log.take().isEmpty());
        // Every property cleared.
        directory.metadataProperty(0, QString(), true, QString());
        QCOMPARE(log.take(), (QList<AudioNotice>{AudioNotice::DefaultOutputChanged,
                                                 AudioNotice::DefaultInputChanged}));
        QVERIFY(directory.defaultNodeName(AudioDeviceDirection::Output).isEmpty());

        // The daemon went away.
        directory.clear();
        QCOMPARE(log.take(), QList<AudioNotice>{AudioNotice::DevicesChanged});
        QVERIFY(directory.nodes().isEmpty());
        directory.clear();
        QVERIFY(log.take().isEmpty());
    }

    // No daemon: the catalogue lists PipeWire as not running, with no
    // devices, and the default engine stays the older drivers.
    void notRunningIsListedAsNotRunning()
    {
        auto owned = std::make_unique<FakePipeWireDeviceSystem>();
        owned->isRunning.store(false);
        owned->nodeList = desktopNodes();
        auto backend = std::make_shared<PipeWireDeviceBackend>(std::move(owned));
        QVERIFY(!backend->running());
        QCOMPARE(defaultAudioEngine({backend}), AudioEngineKind::PortAudio);

        AudioDeviceCatalog catalogue({backend});
        catalogue.start();
        QCOMPARE(catalogue.backends(), QList<AudioBackendId>{AudioBackendId::PipeWire});
        QVERIFY(!catalogue.backendRunning(AudioBackendId::PipeWire));
        QVERIFY(catalogue.devices(AudioBackendId::PipeWire, AudioDeviceDirection::Output).isEmpty());
        catalogue.stop();

        auto runningSystem = std::make_unique<FakePipeWireDeviceSystem>();
        runningSystem->nodeList = desktopNodes();
        auto runningBackend = std::make_shared<PipeWireDeviceBackend>(std::move(runningSystem));
        QCOMPARE(defaultAudioEngine({runningBackend}), AudioEngineKind::PipeWire);
    }

    // R-AUD-32: in a test run the real adapter never connects and every
    // open fails; its output still takes the stereo mix.
    void realAdapterIsBarredInATestRun()
    {
        QVERIFY(audioDevicesBarredForTestRun());
        std::unique_ptr<IPipeWireDeviceSystem> system = makePipeWireDeviceSystem();
        QVERIFY(!system->running());
        QVERIFY(system->nodes().isEmpty());
        QVERIFY(system->defaultNodeName(AudioDeviceDirection::Output).isEmpty());

        const PipeWireNodeRecord stereo = node(31, QStringLiteral("alsa_output.analog-stereo"),
                                               QStringLiteral("Built-in"), QStringLiteral("Audio/Sink"),
                                               QStringLiteral("alsa"), kStereo);
        std::unique_ptr<IAudioBus> bus = system->createOutput(stereo, AudioStreamRequest{});
        QVERIFY(bus != nullptr);
        QVERIFY(bus->takesStereoMix());
        QVERIFY(!bus->open(AudioFormat{}));
        QVERIFY(!bus->isOpen());
        QVERIFY(!bus->errorString().isEmpty());
        QCOMPARE(bus->push(nullptr, 0), qint64(0));
        QVERIFY(bus->fadedOut());

        FakeInputSink sink;
        AudioStreamRequest micRequest;
        micRequest.direction = AudioDeviceDirection::Input;
        std::unique_ptr<IAudioInputStream> mic =
            system->createInput(stereo, micRequest, MicChannelPick::Both, &sink);
        QVERIFY(mic != nullptr);
        QVERIFY(!mic->open());
        QVERIFY(!mic->isOpen());

        PipeWireDeviceBackend backend(makePipeWireDeviceSystem());
        QVERIFY(!backend.running());
        QVERIFY(backend.enumerate().isEmpty());

        // Nor does it retry.
        auto* reconnecting = dynamic_cast<ReconnectingPipeWireDeviceSystem*>(system.get());
        QVERIFY(reconnecting != nullptr);
        QVERIFY(!reconnecting->retrying());
    }

    // R-AUD-03: the daemon goes away and comes back.  The list empties and
    // the engine is not running; the system tries again on its own thread
    // on a timer (never a busy loop); when the daemon answers the devices
    // are listed again and DevicesChanged lets the supervisor reopen them,
    // with no restart.
    void reconnectsWhenTheServerReturns()
    {
        FakeServer server;
        auto owned = std::make_unique<ReconnectingPipeWireDeviceSystem>(server.connector(), kTestRetryMs);
        ReconnectingPipeWireDeviceSystem* system = owned.get();
        PipeWireDeviceBackend backend(std::move(owned));
        NoticeLog log;
        backend.setNoticeSink(log.sink());

        QVERIFY(backend.running());
        QVERIFY(!system->retrying());
        QCOMPARE(backend.enumerate().size(), 2);
        QCOMPARE(server.tries.load(), 1);

        // The daemon stops: as the real connection does, off this thread.
        server.up.store(false);
        std::shared_ptr<FakePipeWireConnection> first = server.last();
        expectLost();
        std::thread pwThread([first] { first->lose(); });
        pwThread.join();
        QVERIFY(!backend.running());
        QVERIFY(backend.enumerate().isEmpty());
        QVERIFY(log.take().contains(AudioNotice::DevicesChanged));

        // The retry is queued to this thread, then tries on the timer.
        QElapsedTimer down;
        down.start();
        QTRY_VERIFY(server.tries.load() >= 3);
        const qint64 downMs = down.elapsed();
        QVERIFY(system->retrying());
        QVERIFY(!backend.running());
        QVERIFY(backend.enumerate().isEmpty());
        // A timer, not a loop: no more tries than the interval allows
        // (a late timer under load only makes fewer).
        QVERIFY2(server.tries.load() <= 2 + int(down.elapsed() / kTestRetryMs) + 1,
                 qPrintable(QStringLiteral("%1 tries in %2 ms").arg(server.tries.load()).arg(downMs)));

        // The daemon answers again.
        expectBack();
        server.up.store(true);
        QTRY_VERIFY(backend.running());
        QVERIFY(!system->retrying());
        const QList<AudioDeviceInfo> back = backend.enumerate();
        QCOMPARE(back.size(), 2);
        QVERIFY(find(back, QStringLiteral("alsa_output.analog-stereo"), AudioDeviceDirection::Output));
        QVERIFY(find(back, QStringLiteral("alsa_input.analog-stereo"), AudioDeviceDirection::Input));
        const QList<AudioNotice> notices = log.take();
        QVERIFY(notices.contains(AudioNotice::DevicesChanged));
        QVERIFY(notices.contains(AudioNotice::DefaultOutputChanged));
        QVERIFY(notices.contains(AudioNotice::DefaultInputChanged));

        // The old connection is detached: it posts nothing and its loss
        // no longer reaches the system.
        QVERIFY(!first->hasLostHandler());
        first->dir.nodeProperties(99, sinkProps(QStringLiteral("stale"), QStringLiteral("Stale")));
        QVERIFY(log.take().isEmpty());

        // Opens go to the new connection.
        QVERIFY(backend.createOutput(AudioStreamRequest{}) != nullptr);

        // Lost again: the cycle repeats.
        const int before = server.tries.load();
        server.up.store(false);
        expectLost();
        server.last()->lose();
        QTRY_VERIFY(server.tries.load() > before);
        expectBack();
        server.up.store(true);
        QTRY_VERIFY(backend.running());
    }

    // No daemon at start: the system keeps trying and lists the devices
    // once it answers.
    void connectsWhenTheServerStartsLate()
    {
        FakeServer server;
        server.up.store(false);
        expectNotRunning();
        ReconnectingPipeWireDeviceSystem system(server.connector(), kTestRetryMs);
        QVERIFY(!system.running());
        QVERIFY(system.retrying());
        expectBack();
        server.up.store(true);
        QTRY_VERIFY(system.running());
        QCOMPARE(system.nodes().size(), 2);
    }

    // stop() during a retry cancels it at once and the connector is never
    // called again; a late loss from the old connection does nothing.
    void stopDuringARetryReturnsPromptly()
    {
        FakeServer server;
        server.up.store(false);
        expectNotRunning();
        ReconnectingPipeWireDeviceSystem system(server.connector(), kTestRetryMs);
        QTRY_VERIFY(server.tries.load() >= 2);
        QVERIFY(system.retrying());

        QElapsedTimer timer;
        timer.start();
        system.stop();
        const qint64 stopMs = timer.elapsed();
        // It never waits on the daemon: well inside one retry interval of
        // the real adapter.
        QVERIFY2(stopMs < kPipeWireReconnectIntervalMs, qPrintable(QString::number(stopMs)));
        QVERIFY(!system.retrying());

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

    // R-AUD-01 on Linux: PipeWire ahead of the older drivers, outside the
    // Core.  In a test run it is not running, so the default stays PortAudio.
    void registryOrderOnLinux()
    {
        const auto window = makeSystemAudioBackends(AudioBackendContext{});
        QCOMPARE(window.size(), std::size_t(2));
        QCOMPARE(window.front()->id(), AudioBackendId::PipeWire);
        QCOMPARE(window.back()->id(), AudioBackendId::PortAudio);
        QVERIFY(!window.front()->running());
        QCOMPARE(defaultAudioEngine(window), AudioEngineKind::PortAudio);

        AudioBackendContext helper;
        helper.helper = true;
        QCOMPARE(makeSystemAudioBackends(helper).front()->id(), AudioBackendId::PipeWire);

        AudioBackendContext daemon;
        daemon.daemon = true;
        const auto core = makeSystemAudioBackends(daemon);
        QCOMPARE(core.size(), std::size_t(1));
        QCOMPARE(core.front()->id(), AudioBackendId::PortAudio);
    }
};

QTEST_MAIN(TestPipeWireDeviceBackend)
#include "tst_pipewire_device_backend.moc"

#else
int main() { return 0; }   // Compiles cleanly on hosts without libpipewire.
#endif
