// =================================================================
// src/core/audio/CaptureHelper.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Control loop of the
// nereus-audio-capture helper process; no Thetis logic.  The native input
// path is the existing PortAudioBus (native-rate open plus its resampler);
// this file only drives it and frames its output.
// =================================================================

#include "core/audio/CaptureHelper.h"

#include "core/AudioDeviceConfig.h"
#include "core/IAudioBus.h"
#include "core/LogCategories.h"
#include "core/MacMicPermission.h"
#include "core/audio/CaptureProtocol.h"
#include "core/audio/PortAudioBus.h"

#include <QCoreApplication>
#include <QString>

#include <portaudio.h>

#include <chrono>
#include <cerrno>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#else
#include <csignal>
#include <unistd.h>
#endif

namespace NereusSDR {

namespace CaptureHelperIo {

namespace {

int s_protocolFd = 1;

std::mutex& writeMutex()
{
    static std::mutex mutex;
    return mutex;
}

} // namespace

void prepareStdio()
{
    std::fflush(stdout);
#ifdef Q_OS_WIN
    _setmode(_fileno(stdin), _O_BINARY);
    const int protocolFd = _dup(1);
    if (protocolFd >= 0) {
        _setmode(protocolFd, _O_BINARY);
        s_protocolFd = protocolFd;
        _dup2(2, 1);
    } else {
        _setmode(1, _O_BINARY);
    }
#else
    // A parent that exits closes our stdout; report that as a failed write
    // instead of dying on SIGPIPE mid-record.
    std::signal(SIGPIPE, SIG_IGN);
    const int protocolFd = ::dup(1);
    if (protocolFd >= 0) {
        s_protocolFd = protocolFd;
        ::dup2(2, 1);
    }
#endif
}

bool writeRecord(const QByteArray& record)
{
    if (record.isEmpty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(writeMutex());
    const char* data = record.constData();
    qint64 remaining = record.size();
    while (remaining > 0) {
#ifdef Q_OS_WIN
        const int written = _write(s_protocolFd, data, static_cast<unsigned int>(remaining));
#else
        const ssize_t written = ::write(s_protocolFd, data, static_cast<size_t>(remaining));
#endif
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (written == 0) {
            return false;
        }
        data += written;
        remaining -= written;
    }
    return true;
}

qint64 readInput(char* buffer, qint64 size)
{
    for (;;) {
#ifdef Q_OS_WIN
        const int got = _read(0, buffer, static_cast<unsigned int>(size));
#else
        const ssize_t got = ::read(0, buffer, static_cast<size_t>(size));
#endif
        if (got < 0 && errno == EINTR) {
            continue;
        }
        return static_cast<qint64>(got);
    }
}

} // namespace CaptureHelperIo

namespace {

namespace P = CaptureProtocol;
using Clock = std::chrono::steady_clock;

// R-R3-21: setCaptureHelperTestDevices. Written once before the helper
// starts its threads, read only by the main thread's open().
QStringList& testRunDevices()
{
    static QStringList names;
    return names;
}

constexpr auto kPumpInterval = std::chrono::milliseconds(10);
constexpr auto kInputLostAfter = std::chrono::milliseconds(500);
// More queued parent commands than this is a misbehaving parent; the
// supervisor never has more than a handful outstanding.
constexpr std::size_t kMaxQueuedCommands = 64;
// The PortAudioBus capture ring is 100 ms of stereo floats; one pull
// drains at most that much.
constexpr int kPullFloats = 4800 * 2;

struct ParentCommand {
    P::RecordType type = P::RecordType::Shutdown;
    P::Configure configure;          // Configure only
    quint32 generation = 0;          // Open / Stop
};

struct CommandQueue {
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<ParentCommand> commands;
};

[[noreturn]] void exitNow(const char* why)
{
    std::fprintf(stderr, "nereus-audio-capture: exiting: %s\n", why);
    std::fflush(stderr);
    std::_Exit(0);
}

// Parses one parent record.  Anything but a valid Configure / Open / Stop
// / Shutdown is a protocol error.
std::optional<ParentCommand> parseCommand(const P::Record& record)
{
    ParentCommand command;
    command.type = record.type;
    switch (record.type) {
    case P::RecordType::Configure: {
        const auto configure = P::decodeConfigure(record.payload);
        if (!configure) {
            return std::nullopt;
        }
        command.configure = *configure;
        command.generation = configure->generation;
        return command;
    }
    case P::RecordType::Open:
    case P::RecordType::Stop: {
        const auto decoded = P::decodeCommand(record.payload);
        if (!decoded) {
            return std::nullopt;
        }
        command.generation = decoded->generation;
        return command;
    }
    case P::RecordType::Shutdown:
        if (record.payload.trimmed() != QByteArrayLiteral("{}")) {
            return std::nullopt;
        }
        return command;
    case P::RecordType::Hello:
    case P::RecordType::Status:
    case P::RecordType::Pcm:
        break;
    }
    return std::nullopt;
}

// Stdin watchdog.  Owns nothing the main thread destroys (the queue is
// shared), so it may outlive runCaptureHelper() while blocked in read().
void readParent(std::shared_ptr<CommandQueue> queue)
{
    P::RecordReader reader;
    std::vector<char> buffer(64 * 1024);
    for (;;) {
        const qint64 got = CaptureHelperIo::readInput(buffer.data(),
                                                      static_cast<qint64>(buffer.size()));
        if (got <= 0) {
            exitNow(got == 0 ? "parent closed the control pipe" : "control pipe read error");
        }
        reader.append(buffer.data(), got);
        while (auto record = reader.next()) {
            const auto command = parseCommand(*record);
            if (!command) {
                exitNow("protocol error in a parent record");
            }
            std::lock_guard<std::mutex> lock(queue->mutex);
            if (queue->commands.size() >= kMaxQueuedCommands) {
                exitNow("too many queued parent records");
            }
            queue->commands.push_back(*command);
            queue->wake.notify_one();
        }
        if (reader.error() != P::RecordReader::Error::None) {
            exitNow("malformed record from the parent");
        }
    }
}

QString clampText(const QString& text)
{
    return text.left(P::kMaxStringChars);
}

quint64 monotonicNs()
{
    return static_cast<quint64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                    Clock::now().time_since_epoch()).count());
}

class Helper {
public:
    explicit Helper(std::shared_ptr<CommandQueue> queue) : m_queue(std::move(queue))
    {
        m_pullScratch.resize(kPullFloats);
        m_pending.reserve(kPullFloats + P::kHelperPcmFrames);
    }

    int run()
    {
        for (;;) {
            std::deque<ParentCommand> batch;
            {
                std::unique_lock<std::mutex> lock(m_queue->mutex);
                const auto hasWork = [this]() { return !m_queue->commands.empty(); };
                if (isPumping()) {
                    m_queue->wake.wait_until(lock, m_nextPump, hasWork);
                } else {
                    m_queue->wake.wait(lock, hasWork);
                }
                batch.swap(m_queue->commands);
            }
            for (const ParentCommand& command : batch) {
                if (command.type == P::RecordType::Shutdown) {
                    shutdown();
                    return 0;
                }
                execute(command);
            }
            if (isPumping() && Clock::now() >= m_nextPump) {
                pump();
                m_nextPump += kPumpInterval;
                // Never try to catch up a stall tick by tick; the bus ring
                // already holds what arrived meanwhile.
                const auto now = Clock::now();
                if (m_nextPump < now) {
                    m_nextPump = now + kPumpInterval;
                }
            }
        }
    }

private:
    bool isPumping() const { return m_bus && m_bus->isOpen(); }

    void execute(const ParentCommand& command)
    {
        switch (command.type) {
        case P::RecordType::Configure:
            configure(command.configure);
            break;
        case P::RecordType::Open:
            if (command.generation != m_generation) {
                qCInfo(lcAudio) << "capture helper: ignoring Open for generation"
                                << command.generation << "(current" << m_generation << ")";
                break;
            }
            open();
            break;
        case P::RecordType::Stop:
            if (command.generation != m_generation) {
                qCInfo(lcAudio) << "capture helper: ignoring Stop for generation"
                                << command.generation << "(current" << m_generation << ")";
                break;
            }
            closeBus();
            sendStatus(P::HelperState::Stopped);
            break;
        default:
            break;
        }
    }

    void configure(const P::Configure& configure)
    {
        if (m_bus) {
            qCInfo(lcAudio) << "capture helper: reconfigured, closing generation" << m_generation;
            closeBus();
        }
        m_generation = configure.generation;
        m_device = configure.device;
    }

    void open()
    {
        if (m_generation == 0) {
            return;
        }
        if (isPumping()) {
            qCInfo(lcAudio) << "capture helper: generation" << m_generation << "already open";
            return;
        }
        closeBus();

        MicPermission permission = microphonePermissionStatus();
        if (permission == MicPermission::Undetermined) {
            sendStatus(P::HelperState::Permission);
            permission = requestMicrophonePermissionAndWait();
        }
        if (permission == MicPermission::Denied) {
            sendFailure(P::FailReason::PermissionDenied,
                        QStringLiteral("microphone access is denied for this application"));
            return;
        }

        sendStatus(P::HelperState::Opening);

        // R-R3-21: a test run never initialises PortAudio or opens a real
        // device, the same decision AudioEngine makes. The named device is
        // looked up on the test's list (setCaptureHelperTestDevices), so a
        // missing device still fails as one, with the bus's own words.
        if (PortAudioBus::portAudioBarredForTestRun()) {
            const QString name = m_device.deviceName.trimmed();
            const QStringList& listed = testRunDevices();
            if (name.isEmpty() ? listed.isEmpty() : !listed.contains(name)) {
                sendFailure(P::FailReason::DeviceNotFound,
                            name.isEmpty() ? QStringLiteral("No input device found")
                                           : QStringLiteral("device-not-found: ") + name);
            } else {
                sendFailure(P::FailReason::OpenFailed,
                            QStringLiteral("a test run opens no audio device"));
            }
            return;
        }

        if (!m_paInitialized) {
            const PaError err = Pa_Initialize();
            if (err != paNoError) {
                sendFailure(P::FailReason::OpenFailed,
                            QStringLiteral("Pa_Initialize: ")
                                + QString::fromUtf8(Pa_GetErrorText(err)));
                return;
            }
            m_paInitialized = true;
        }

        auto bus = std::make_unique<PortAudioBus>();
        PortAudioConfig config;
        config.direction = AudioDirection::Input;
        config.hostApiIndex = m_device.hostApiIndex;
        config.deviceName = m_device.deviceName;
        config.bufferSamples = m_device.bufferSamples;
        config.exclusiveMode = m_device.exclusiveMode;
        bus->setConfig(config);
        bus->setStrictInputDevice(true);

        AudioFormat format;
        format.sampleRate = P::kSampleRate;
        format.channels = 1;
        format.sample = AudioFormat::Sample::Float32;

        if (!bus->open(format)) {
            P::FailReason reason = P::FailReason::OpenFailed;
            switch (bus->lastOpenFailure()) {
            case PortAudioBus::OpenFailure::DeviceNotFound:
                reason = P::FailReason::DeviceNotFound;
                break;
            case PortAudioBus::OpenFailure::StartFailed:
                reason = P::FailReason::StartFailed;
                break;
            case PortAudioBus::OpenFailure::OpenFailed:
            case PortAudioBus::OpenFailure::None:
                reason = P::FailReason::OpenFailed;
                break;
            }
            qCWarning(lcAudio) << "capture helper: open failed:" << bus->errorString();
            sendFailure(reason, bus->errorString());
            return;
        }

        P::Status ready;
        ready.generation = m_generation;
        ready.state = P::HelperState::Ready;
        ready.actualDevice = clampText(bus->openedDeviceName());
        ready.nativeRate = bus->openedNativeRate();
        ready.nativeChannels = bus->openedStreamChannels();
        const QByteArray record = P::encodeStatus(ready);
        if (record.isEmpty()) {
            bus->close();
            sendFailure(P::FailReason::Internal,
                        QStringLiteral("opened stream reports an unsupported format: %1 Hz, %2 channels")
                            .arg(ready.nativeRate)
                            .arg(ready.nativeChannels));
            return;
        }
        qCInfo(lcAudio) << "capture helper: generation" << m_generation << "ready on"
                        << ready.actualDevice << ready.nativeRate << "Hz"
                        << ready.nativeChannels << "ch";

        m_bus = std::move(bus);
        m_framePosition = 0;
        m_pending.clear();
        m_lastInput = Clock::now();
        m_nextPump = m_lastInput + kPumpInterval;
        write(record);
    }

    void pump()
    {
        const qint64 bytes = m_bus->pull(reinterpret_cast<char*>(m_pullScratch.data()),
                                         static_cast<qint64>(m_pullScratch.size() * sizeof(float)));
        const int floats = static_cast<int>(bytes / static_cast<qint64>(sizeof(float)));
        const auto now = Clock::now();
        if (floats > 0) {
            m_lastInput = now;
            for (int i = 0; i < floats; ++i) {
                const float sample = m_pullScratch[static_cast<std::size_t>(i)];
                m_pending.push_back(std::isfinite(sample) ? sample : 0.0f);
            }
        }

        std::size_t offset = 0;
        while (m_pending.size() - offset >= static_cast<std::size_t>(P::kHelperPcmFrames)) {
            const QByteArray record = P::encodePcm(m_generation, m_framePosition, monotonicNs(),
                                                   m_pending.data() + offset,
                                                   P::kHelperPcmFrames);
            if (record.isEmpty()) {
                closeBus();
                sendFailure(P::FailReason::Internal, QStringLiteral("could not frame captured audio"));
                return;
            }
            write(record);
            m_framePosition += static_cast<quint64>(P::kHelperPcmFrames);
            offset += static_cast<std::size_t>(P::kHelperPcmFrames);
        }
        if (offset > 0) {
            m_pending.erase(m_pending.begin(),
                            m_pending.begin() + static_cast<std::ptrdiff_t>(offset));
        }

        if (now - m_lastInput >= kInputLostAfter) {
            qCWarning(lcAudio) << "capture helper: no input for 500 ms on generation"
                               << m_generation;
            closeBus();
            sendFailure(P::FailReason::InputLost,
                        QStringLiteral("the input produced no audio for 500 ms"));
        }
    }

    void closeBus()
    {
        if (m_bus) {
            m_bus->close();
            m_bus.reset();
        }
        m_pending.clear();
    }

    void shutdown()
    {
        closeBus();
        if (m_paInitialized) {
            Pa_Terminate();
            m_paInitialized = false;
        }
    }

    void sendStatus(P::HelperState state)
    {
        P::Status status;
        status.generation = m_generation;
        status.state = state;
        write(P::encodeStatus(status));
    }

    void sendFailure(P::FailReason reason, const QString& detail)
    {
        P::Status status;
        status.generation = m_generation;
        status.state = P::HelperState::Failed;
        status.reason = reason;
        status.detail = clampText(detail);
        write(P::encodeStatus(status));
    }

    static void write(const QByteArray& record)
    {
        if (record.isEmpty()) {
            qCWarning(lcAudio) << "capture helper: dropped a record that failed to encode";
            return;
        }
        if (!CaptureHelperIo::writeRecord(record)) {
            exitNow("parent closed the data pipe");
        }
    }

    std::shared_ptr<CommandQueue> m_queue;
    quint32 m_generation = 0;
    AudioDeviceConfig m_device;
    bool m_paInitialized = false;
    std::unique_ptr<PortAudioBus> m_bus;
    quint64 m_framePosition = 0;
    Clock::time_point m_lastInput;
    Clock::time_point m_nextPump;
    std::vector<float> m_pullScratch;
    std::vector<float> m_pending;
};

} // namespace

void setCaptureHelperTestDevices(const QStringList& names)
{
    testRunDevices() = names;
}

int runCaptureHelper(int argc, char** argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    CaptureHelperIo::prepareStdio();

    P::Hello hello;
    hello.protocol = P::kVersion;
    hello.pid = QCoreApplication::applicationPid();
    hello.build = QStringLiteral(NEREUSSDR_VERSION);
    if (!CaptureHelperIo::writeRecord(P::encodeHello(hello))) {
        exitNow("could not send hello");
    }

    auto queue = std::make_shared<CommandQueue>();
    // Detached on purpose: the watchdog must be able to end the process
    // while the main thread is stuck in a native call, and it may still be
    // blocked in read() when Shutdown returns from here.
    std::thread(readParent, queue).detach();

    Helper helper(queue);
    const int code = helper.run();
    std::fflush(stderr);
    return code;
}

} // namespace NereusSDR
