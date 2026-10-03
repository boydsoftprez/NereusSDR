// =================================================================
// tests/fakes/FakeCaptureChild.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Scripted capture-helper peer for
// process supervision tests; see FakeCaptureChild.h for the scenarios.
// =================================================================

#include "FakeCaptureChild.h"

#include "core/audio/CaptureHelper.h"
#include "core/audio/CaptureProtocol.h"

#include <QCoreApplication>
#include <QFile>
#include <QtEndian>

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
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
constexpr double kTwoPi = 6.283185307179586;

enum class Scenario {
    Ready, HangOpen, NoHello, PermissionThenReady, CrashAfterReady,
    Malformed, Oversize, InputLost, IgnoreStop, Stale
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
    return std::nullopt;
}

struct Command {
    P::RecordType type = P::RecordType::Shutdown;
    quint32 generation = 0;
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
            } else if (record->type == P::RecordType::Open || record->type == P::RecordType::Stop) {
                const auto decoded = P::decodeCommand(record->payload);
                if (!decoded) {
                    std::_Exit(0);
                }
                command.generation = decoded->generation;
            } else if (record->type != P::RecordType::Shutdown) {
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

void sendStatus(quint32 generation, P::HelperState state,
                P::FailReason reason = P::FailReason::None, const QString& detail = {})
{
    P::Status status;
    status.generation = generation;
    status.state = state;
    status.reason = reason;
    status.detail = detail;
    if (state == P::HelperState::Ready) {
        status.actualDevice = QStringLiteral("Fake microphone");
        status.nativeRate = P::kSampleRate;
        status.nativeChannels = 1;
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

class Fake {
public:
    Fake(Scenario scenario, std::shared_ptr<Queue> queue)
        : m_scenario(scenario), m_queue(std::move(queue))
    {
        m_ignoring = (scenario == Scenario::NoHello);
        m_tone.resize(P::kHelperPcmFrames);
    }

    int run()
    {
        if (m_scenario != Scenario::NoHello) {
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
            m_streaming = false;
            m_readyAt.reset();
            break;
        case P::RecordType::Open:
            if (command.generation == m_generation) {
                open();
            }
            break;
        case P::RecordType::Stop:
            if (command.generation == m_generation && m_scenario != Scenario::IgnoreStop) {
                m_streaming = false;
                m_readyAt.reset();
                sendStatus(m_generation, P::HelperState::Stopped);
            }
            break;
        default:
            break;
        }
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
            startStreaming(stale);
            return;
        }
        case Scenario::Oversize:
            sendStatus(m_generation, P::HelperState::Opening);
            sendStatus(m_generation, P::HelperState::Ready);
            send(oversizePcmRecord(m_generation));
            return;
        default:
            sendStatus(m_generation, P::HelperState::Opening);
            sendStatus(m_generation, P::HelperState::Ready);
            startStreaming(m_generation);
            return;
        }
    }

    void startStreaming(quint32 pcmGeneration)
    {
        m_streaming = true;
        m_pcmGeneration = pcmGeneration;
        m_framePosition = 0;
        m_streamStart = Clock::now();
        m_nextPcm = m_streamStart;
    }

    void tick()
    {
        const auto now = Clock::now();
        if (m_readyAt && now >= *m_readyAt) {
            m_readyAt.reset();
            sendStatus(m_generation, P::HelperState::Opening);
            sendStatus(m_generation, P::HelperState::Ready);
            startStreaming(m_generation);
        }
        if (!m_streaming) {
            return;
        }
        while (m_streaming && now >= m_nextPcm) {
            sendTone();
            m_nextPcm += kTick;
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
        for (int i = 0; i < P::kHelperPcmFrames; ++i) {
            const double n = static_cast<double>(m_framePosition + static_cast<quint64>(i));
            m_tone[static_cast<std::size_t>(i)] =
                static_cast<float>(0.5 * std::sin(kTwoPi * 1000.0 * n / P::kSampleRate));
        }
        const auto sentNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                Clock::now().time_since_epoch()).count();
        send(P::encodePcm(m_pcmGeneration, m_framePosition, static_cast<quint64>(sentNs),
                          m_tone.data(), P::kHelperPcmFrames));
        m_framePosition += static_cast<quint64>(P::kHelperPcmFrames);
    }

    Scenario m_scenario;
    std::shared_ptr<Queue> m_queue;
    bool m_ignoring = false;
    quint32 m_generation = 0;
    quint32 m_pcmGeneration = 0;
    bool m_streaming = false;
    quint64 m_framePosition = 0;
    Clock::time_point m_streamStart;
    Clock::time_point m_nextPcm;
    std::optional<Clock::time_point> m_readyAt;
    std::vector<float> m_tone;
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
