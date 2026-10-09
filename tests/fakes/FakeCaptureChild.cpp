// =================================================================
// tests/fakes/FakeCaptureChild.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Scripted capture-helper peer for
// process supervision tests; see FakeCaptureChild.h for the scenarios.
//
// Modification history (NereusSDR):
//   2026-10-08: native audio plan Task 1 (V-HW-8): probe and version-1
//               scenarios. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-09: native audio plan Task 13 (R-AUD-17): AttachRing, and the
//               tone through a clock matcher in the shared ring.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 15 (R-AUD-19): every scenario
//               answers the ASIO records for one fake driver.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 16 fix round 2 (R-AUD-18): Ready
//               names the configured device, as the real helper names the
//               device it opened; the busy-while-marked scenario.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 20 round 3 (R-AUD-24): the
//               busy-while-marked-then-pending scenario. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "FakeCaptureChild.h"

#include "core/audio/AudioDelayProbe.h"
#include "core/audio/CaptureHelper.h"
#include "core/audio/CaptureProtocol.h"
#include "core/audio/CaptureShm.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/MatcherRing.h"

#include <QCoreApplication>
#include <QFile>
#include <QtEndian>

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace NereusSDR::Test {

namespace {

namespace P = NereusSDR::CaptureProtocol;
using Clock = std::chrono::steady_clock;

constexpr auto kTick = std::chrono::milliseconds(10);
constexpr auto kPermissionHold = std::chrono::milliseconds(300);
constexpr auto kPcmBeforeEvent = std::chrono::milliseconds(100);
constexpr auto kProbeHitEvery = std::chrono::milliseconds(50);
constexpr int kProbeEnabledBeforeReadyExit = 4;
constexpr double kTwoPi = 6.283185307179586;
constexpr int kToneFrames = 480;

enum class Scenario {
    Ready, HangOpen, NoHello, PermissionThenReady, CrashAfterReady,
    Malformed, Oversize, InputLost, IgnoreStop, Stale, Probe, Version1,
    PcmRecord, Busy, BusyWhileMarked, BusyWhileMarkedThenPending, BadRing
};

std::optional<Scenario> scenarioFromName(const QString& name)
{
    if (name == QLatin1String("ready")) { return Scenario::Ready; }
    if (name == QLatin1String("hang-open")) { return Scenario::HangOpen; }
    if (name == QLatin1String("no-hello")) { return Scenario::NoHello; }
    if (name == QLatin1String("permission-then-ready")) { return Scenario::PermissionThenReady; }
    if (name == QLatin1String("crash-after-ready")) { return Scenario::CrashAfterReady; }
    if (name == QLatin1String("malformed")) { return Scenario::Malformed; }
    if (name == QLatin1String("oversize")) { return Scenario::Oversize; }
    if (name == QLatin1String("input-lost")) { return Scenario::InputLost; }
    if (name == QLatin1String("ignore-stop")) { return Scenario::IgnoreStop; }
    if (name == QLatin1String("stale")) { return Scenario::Stale; }
    if (name == QLatin1String("probe")) { return Scenario::Probe; }
    if (name == QLatin1String("version-1")) { return Scenario::Version1; }
    if (name == QLatin1String("pcm-record")) { return Scenario::PcmRecord; }
    if (name == QLatin1String("busy")) { return Scenario::Busy; }
    if (name == QLatin1String("busy-while-marked")) { return Scenario::BusyWhileMarked; }
    if (name == QLatin1String("busy-while-marked-then-pending")) {
        return Scenario::BusyWhileMarkedThenPending;
    }
    if (name == QLatin1String("bad-ring")) { return Scenario::BadRing; }
    return std::nullopt;
}

struct Command {
    P::RecordType type = P::RecordType::Shutdown;
    quint32 generation = 0;
    bool probeEnabled = false;
    CaptureShmNames ring;          // AttachRing
    qint64 ringBytes = 0;
    QString asioDriver;            // AsioDescribe
    P::AsioOpen asioOpen;          // AsioOpen
    QString deviceName;            // Configure: the chosen device, empty for the default
};

struct Queue {
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<Command> commands;
};

void readParent(std::shared_ptr<Queue> queue)
{
    P::RecordReader reader;
    std::vector<char> buffer(16 * 1024);
    for (;;) {
        const qint64 got = CaptureHelperIo::readInput(buffer.data(),
                                                      static_cast<qint64>(buffer.size()));
        if (got <= 0) {
            std::_Exit(0);
        }
        reader.append(buffer.data(), got);
        while (auto record = reader.next()) {
            Command command;
            command.type = record->type;
            if (record->type == P::RecordType::Configure) {
                const auto configure = P::decodeConfigure(record->payload);
                if (!configure) {
                    std::_Exit(0);
                }
                command.generation = configure->generation;
                command.deviceName = configure->device.deviceName;
            } else if (record->type == P::RecordType::Open || record->type == P::RecordType::Stop) {
                const auto decoded = P::decodeCommand(record->payload);
                if (!decoded) {
                    std::_Exit(0);
                }
                command.generation = decoded->generation;
            } else if (record->type == P::RecordType::AttachRing) {
                const auto attach = P::decodeAttachRing(record->payload);
                if (!attach) {
                    std::_Exit(0);
                }
                command.generation = attach->generation;
                command.ring = {attach->memory, attach->wake};
                command.ringBytes = attach->bytes;
            } else if (record->type == P::RecordType::ProbeEnable) {
                const auto enabled = P::decodeProbeEnable(record->payload);
                if (!enabled) {
                    std::_Exit(0);
                }
                command.probeEnabled = *enabled;
            } else if (record->type == P::RecordType::AsioDescribe) {
                const auto describe = P::decodeAsioDescribe(record->payload);
                if (!describe) {
                    std::_Exit(0);
                }
                command.asioDriver = describe->driver;
            } else if (record->type == P::RecordType::AsioOpen) {
                const auto open = P::decodeAsioOpen(record->payload);
                if (!open) {
                    std::_Exit(0);
                }
                command.asioOpen = *open;
            } else if (record->type != P::RecordType::Shutdown
                       && record->type != P::RecordType::AsioControlPanel) {
                std::_Exit(0);
            }
            std::lock_guard<std::mutex> lock(queue->mutex);
            queue->commands.push_back(command);
            queue->wake.notify_one();
        }
        if (reader.error() != P::RecordReader::Error::None) {
            std::_Exit(0);
        }
    }
}

void send(const QByteArray& record)
{
    if (!CaptureHelperIo::writeRecord(record)) {
        std::_Exit(0);
    }
}

// The name a Ready reports: the configured device, as the real helper
// reports the device it opened, or "Fake microphone" for the default.
QString& readyDeviceName()
{
    static QString name;
    return name;
}

void sendStatus(quint32 generation, P::HelperState state,
                P::FailReason reason = P::FailReason::None, const QString& detail = {})
{
    P::Status status;
    status.generation = generation;
    status.state = state;
    status.reason = reason;
    status.detail = detail;
    if (state == P::HelperState::Ready) {
        status.actualDevice = readyDeviceName().isEmpty() ? QStringLiteral("Fake microphone")
                                                          : readyDeviceName();
        status.nativeRate = P::kSampleRate;
        status.nativeChannels = 1;
        status.latencyUs = 1500;
        status.bufferFrames = kToneFrames;
    }
    send(P::encodeStatus(status));
}

// A PCM record whose header claims one frame more than the contract allows.
QByteArray oversizePcmRecord(quint32 generation)
{
    constexpr quint32 kFrames = P::kMaxPcmFrames + 1;
    const quint32 payloadBytes = P::kPcmHeaderBytes + kFrames * 4;
    QByteArray record;
    record.reserve(P::kHeaderBytes + static_cast<int>(payloadBytes));
    record.append("NCAP", 4);
    record.append(static_cast<char>(P::kVersion));
    record.append(static_cast<char>(P::RecordType::Pcm));
    record.append('\0');
    record.append('\0');
    char word[8];
    qToLittleEndian<quint32>(payloadBytes, word);
    record.append(word, 4);
    qToLittleEndian<quint32>(generation, word);
    record.append(word, 4);
    qToLittleEndian<quint32>(kFrames, word);
    record.append(word, 4);
    qToLittleEndian<quint64>(0, word);
    record.append(word, 8);
    record.append(word, 8);
    record.append(QByteArray(static_cast<int>(kFrames) * 4, '\0'));
    return record;
}

// A Hello as a protocol 1 helper writes it: version 1 header, protocol 1.
QByteArray version1HelloRecord()
{
    const QByteArray json = QByteArrayLiteral("{\"build\":\"old\",\"pid\":")
                            + QByteArray::number(QCoreApplication::applicationPid())
                            + QByteArrayLiteral(",\"protocol\":1}");
    QByteArray record;
    record.append("NCAP", 4);
    record.append(static_cast<char>(1));
    record.append(static_cast<char>(P::RecordType::Hello));
    record.append('\0');
    record.append('\0');
    char word[4];
    qToLittleEndian<quint32>(static_cast<quint32>(json.size()), word);
    record.append(word, 4);
    record.append(json);
    return record;
}

class Fake {
public:
    Fake(Scenario scenario, std::shared_ptr<Queue> queue)
        : m_scenario(scenario), m_queue(std::move(queue))
    {
        m_ignoring = (scenario == Scenario::NoHello);
        m_tone.resize(static_cast<std::size_t>(kToneFrames) * 2);
    }

    int run()
    {
        if (m_scenario == Scenario::Version1) {
            send(version1HelloRecord());
        } else if (m_scenario != Scenario::NoHello) {
            P::Hello hello;
            hello.protocol = P::kVersion;
            hello.pid = QCoreApplication::applicationPid();
            hello.build = QStringLiteral("fake-capture-child");
            send(P::encodeHello(hello));
        }
        if (m_scenario == Scenario::Malformed) {
            send(QByteArray("XCAP\x01\x02\x00\x00\x00\x00\x00\x00", 12));
        }

        for (;;) {
            std::deque<Command> batch;
            {
                std::unique_lock<std::mutex> lock(m_queue->mutex);
                m_queue->wake.wait_for(lock, kTick,
                                       [this]() { return !m_queue->commands.empty(); });
                batch.swap(m_queue->commands);
            }
            for (const Command& command : batch) {
                if (m_ignoring) {
                    continue;
                }
                if (command.type == P::RecordType::Shutdown) {
                    return 0;
                }
                handle(command);
            }
            tick();
        }
    }

private:
    void handle(const Command& command)
    {
        switch (command.type) {
        case P::RecordType::Configure:
            m_generation = command.generation;
            readyDeviceName() = command.deviceName;
            m_streaming = false;
            m_readySent = false;
            m_readyAt.reset();
            m_matcher.reset();
            break;
        case P::RecordType::AttachRing:
            m_matcher.reset();
            m_region = CaptureShmRegion::attach(command.ring,
                                                static_cast<std::size_t>(command.ringBytes));
            if (!m_region) {
                sendStatus(command.generation, P::HelperState::Failed, P::FailReason::Internal,
                           QStringLiteral("fake could not attach the ring"));
                break;
            }
            send(P::encodeRingAttached(P::Command{command.generation}));
            break;
        case P::RecordType::ProbeEnable:
            if (m_scenario != Scenario::Probe) {
                break;
            }
            if (command.probeEnabled && !m_readySent) {
                std::_Exit(kProbeEnabledBeforeReadyExit);
            }
            m_probeOn = command.probeEnabled;
            if (m_probeOn) {
                ++m_probeEnables;
                m_probeHits = 0;
                m_nextProbeHit = Clock::now();
            }
            break;
        case P::RecordType::Open:
            if (command.generation == m_generation) {
                open();
            }
            break;
        case P::RecordType::AsioDescribe:
            describeAsio(command.asioDriver);
            break;
        case P::RecordType::AsioOpen:
            openAsio(command.asioOpen);
            break;
        case P::RecordType::Stop:
            if (command.generation == m_generation && m_scenario != Scenario::IgnoreStop) {
                m_streaming = false;
                m_matcher.reset();
                m_readySent = false;
                m_probeOn = false;
                m_readyAt.reset();
                sendStatus(m_generation, P::HelperState::Stopped);
            }
            break;
        default:
            break;
        }
    }

    // Task 15: one fake ASIO driver, "Fake ASIO" (4 out, 2 in).  Opening
    // it answers running at 256 frames and 48 kHz; no uses answers closed.
    static AsioDriverCaps fakeAsioCaps()
    {
        AsioDriverCaps caps;
        caps.name = QStringLiteral("Fake ASIO");
        caps.outputChannels = 4;
        caps.inputChannels = 2;
        caps.sampleType = AsioSampleType::Float32Lsb;
        caps.minBufferFrames = 64;
        caps.maxBufferFrames = 2048;
        caps.preferredBufferFrames = 256;
        caps.granularity = -1;
        caps.sampleRates = {44100.0, 48000.0};
        caps.currentRate = 48000.0;
        caps.inputLatencyFrames = 300;
        caps.outputLatencyFrames = 400;
        return caps;
    }

    static void describeAsio(const QString& driver)
    {
        P::AsioCapsRecord caps;
        caps.drivers = {QStringLiteral("Fake ASIO")};
        caps.driver = driver;
        if (driver == QLatin1String("Fake ASIO")) {
            caps.caps = fakeAsioCaps();
        }
        send(P::encodeAsioCaps(caps));
    }

    static void openAsio(const P::AsioOpen& open)
    {
        P::AsioState state;
        state.serial = open.serial;
        state.driver = open.driver;
        if (open.uses.isEmpty()) {
            state.state = P::AsioStateKind::Closed;
        } else {
            state.state = P::AsioStateKind::Running;
            state.bufferFrames = 256;
            state.rate = 48000.0;
            state.inputLatencyFrames = 300;
            state.outputLatencyFrames = 400;
        }
        send(P::encodeAsioState(state));
    }

    // hang-open only: once the Open is answered and every later command is
    // ignored, creates <dir>/<pid> when NEREUS_FAKE_CAPTURE_HANG_DIR names a
    // directory, so a test can tell that the helper is now really hanging
    // (before that, a Shutdown still ends it).
    static void markHanging()
    {
        const QString dir = qEnvironmentVariable("NEREUS_FAKE_CAPTURE_HANG_DIR");
        if (dir.isEmpty()) {
            return;
        }
        QFile marker(dir + QLatin1Char('/')
                     + QString::number(QCoreApplication::applicationPid()));
        if (marker.open(QIODevice::WriteOnly)) {
            marker.close();
        }
    }

    void open()
    {
        switch (m_scenario) {
        case Scenario::HangOpen:
            sendStatus(m_generation, P::HelperState::Opening);
            m_ignoring = true;
            markHanging();
            return;
        case Scenario::PermissionThenReady:
            sendStatus(m_generation, P::HelperState::Permission);
            m_readyAt = Clock::now() + kPermissionHold;
            return;
        case Scenario::Stale: {
            sendStatus(m_generation, P::HelperState::Opening);
            const quint32 stale = (m_generation == 1) ? std::numeric_limits<quint32>::max()
                                                      : m_generation - 1;
            sendStatus(stale, P::HelperState::Ready);
            startStreaming();
            return;
        }
        case Scenario::BusyWhileMarkedThenPending:
            if (!QFile::exists(qEnvironmentVariable("NEREUS_FAKE_CAPTURE_BUSY_FILE"))) {
                // The engine's retry: Opening, then no answer.
                sendStatus(m_generation, P::HelperState::Opening);
                return;
            }
            sendStatus(m_generation, P::HelperState::Opening);
            sendStatus(m_generation, P::HelperState::Failed, P::FailReason::DeviceInUse,
                       QStringLiteral("fake device held by another program"));
            return;
        case Scenario::BusyWhileMarked:
            if (!QFile::exists(qEnvironmentVariable("NEREUS_FAKE_CAPTURE_BUSY_FILE"))) {
                sendStatus(m_generation, P::HelperState::Opening);
                sendStatus(m_generation, P::HelperState::Ready);
                m_readySent = true;
                startStreaming();
                return;
            }
            [[fallthrough]];
        case Scenario::Busy:
            sendStatus(m_generation, P::HelperState::Opening);
            sendStatus(m_generation, P::HelperState::Failed, P::FailReason::DeviceInUse,
                       QStringLiteral("fake device held by another program"));
            return;
        case Scenario::BadRing:
            sendStatus(m_generation, P::HelperState::Opening);
            sendStatus(m_generation, P::HelperState::Ready);
            if (m_region) {
                std::memset(m_region->data(), 0, sizeof(MatcherRingHeader));
                m_region->postWake();
            }
            return;
        case Scenario::PcmRecord:
            sendStatus(m_generation, P::HelperState::Opening);
            sendStatus(m_generation, P::HelperState::Ready);
            m_readySent = true;
            startStreaming();
            send(P::encodePcm(m_generation, 0, 0, m_tone.data(), P::kHelperPcmFrames));
            return;
        case Scenario::Oversize:
            sendStatus(m_generation, P::HelperState::Opening);
            sendStatus(m_generation, P::HelperState::Ready);
            send(oversizePcmRecord(m_generation));
            return;
        default:
            sendStatus(m_generation, P::HelperState::Opening);
            sendStatus(m_generation, P::HelperState::Ready);
            m_readySent = true;
            startStreaming();
            return;
        }
    }

    // Builds the clock matcher in the attached region.  Without a region
    // the fake keeps its timeline (the scripted events after 100 ms) but
    // writes nothing.
    void startStreaming()
    {
        m_streaming = true;
        m_framePosition = 0;
        m_streamStart = Clock::now();
        m_nextPcm = m_streamStart;
        if (!m_region) {
            return;
        }
        DeviceRateMatcher::Config config;
        config.inRate = P::kSampleRate;
        config.outRate = P::kSampleRate;
        config.writeBlockFrames = 64;
        config.callbackFrames = kToneFrames;
        config.delayMs = 0;
        m_matcher = std::make_unique<DeviceRateMatcher>(config, m_region->data(), m_region->size());
        if (!m_matcher->valid()) {
            m_matcher.reset();
        }
    }

    void tick()
    {
        const auto now = Clock::now();
        if (m_readyAt && now >= *m_readyAt) {
            m_readyAt.reset();
            sendStatus(m_generation, P::HelperState::Opening);
            sendStatus(m_generation, P::HelperState::Ready);
            startStreaming();
        }
        if (!m_streaming) {
            return;
        }
        while (m_streaming && now >= m_nextPcm) {
            sendTone();
            m_nextPcm += kTick;
        }
        while (m_probeOn && now >= m_nextProbeHit) {
            send(P::encodeProbeHit(1000 * static_cast<std::int64_t>(m_probeEnables)
                                   + m_probeHits));
            ++m_probeHits;
            m_nextProbeHit += kProbeHitEvery;
        }
        if (now - m_streamStart >= kPcmBeforeEvent) {
            if (m_scenario == Scenario::CrashAfterReady) {
                std::_Exit(3);
            }
            if (m_scenario == Scenario::InputLost) {
                m_streaming = false;
                sendStatus(m_generation, P::HelperState::Failed, P::FailReason::InputLost,
                           QStringLiteral("fake input stopped"));
            }
        }
    }

    void sendTone()
    {
        if (!m_matcher) {
            return;
        }
        for (int i = 0; i < kToneFrames; ++i) {
            const double n = static_cast<double>(m_framePosition + static_cast<quint64>(i));
            const auto value =
                static_cast<float>(0.5 * std::sin(kTwoPi * 1000.0 * n / P::kSampleRate));
            m_tone[static_cast<std::size_t>(2 * i)] = value;
            m_tone[static_cast<std::size_t>(2 * i + 1)] = value;
        }
        m_matcher->write(m_tone.data(), kToneFrames, audioProbeNowNs());
        m_region->postWake();
        m_framePosition += static_cast<quint64>(kToneFrames);
    }

    Scenario m_scenario;
    std::shared_ptr<Queue> m_queue;
    bool m_ignoring = false;
    quint32 m_generation = 0;
    std::unique_ptr<CaptureShmRegion> m_region;
    std::unique_ptr<DeviceRateMatcher> m_matcher;   // built in m_region
    bool m_streaming = false;
    quint64 m_framePosition = 0;
    Clock::time_point m_streamStart;
    Clock::time_point m_nextPcm;
    std::optional<Clock::time_point> m_readyAt;
    std::vector<float> m_tone;
    bool m_readySent = false;
    bool m_probeOn = false;
    int m_probeEnables = 0;
    std::int64_t m_probeHits = 0;
    Clock::time_point m_nextProbeHit;
};

} // namespace

int runFakeCaptureChild(const QString& scenarioName)
{
    const auto scenario = scenarioFromName(scenarioName);
    if (!scenario) {
        return 2;
    }
    CaptureHelperIo::prepareStdio();
    auto queue = std::make_shared<Queue>();
    std::thread(readParent, queue).detach();
    Fake fake(*scenario, queue);
    return fake.run();
}

} // namespace NereusSDR::Test
