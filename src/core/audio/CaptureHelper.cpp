// =================================================================
// src/core/audio/CaptureHelper.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Control loop of the
// nereus-audio-capture helper process; no Thetis logic.  The input opens
// through the saved engine's backend (the older drivers through
// PortAudioBus), and its callback writes the clock matcher in the shared
// ring the window made (R-AUD-17).
//
// Modification history (NereusSDR):
//   2026-10-08: native audio plan Task 1 (V-HW-8): ProbeEnable turns the
//               audio delay probe's detector on the input callback on or
//               off; its hits go to the window as ProbeHit records.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 7 fix (R-AUD-06): Pa_Initialize and
//               Pa_Terminate hold PortAudioLibrary's lock, as every
//               PortAudio call outside a callback does. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan early-review fix wave (R-AUD-02, bug 1):
//               the mic opens on its saved host API (driverApi), mapped
//               under PortAudioLibrary's lock.  J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 13 (R-AUD-17, R-AUD-18): the mic
//               opens on the saved engine, and the input callback writes
//               the clock matcher in a shared-memory ring and posts its
//               wake; Pcm records and the 10 ms pump are gone.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/CaptureHelper.h"

#include "core/AudioDeviceConfig.h"
#include "core/IAudioBus.h"
#include "core/LogCategories.h"
#include "core/MacMicPermission.h"
#include "core/audio/AudioBackendRegistry.h"
#include "core/audio/AudioDelayProbe.h"
#include "core/audio/AudioDeviceMatching.h"
#include "core/audio/AudioTestBarrier.h"
#include "core/audio/CaptureProtocol.h"
#include "core/audio/CaptureShm.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/IAudioEngineBackend.h"
#include "core/audio/MatcherRing.h"
#include "core/audio/PortAudioBus.h"
#include "core/audio/PortAudioLibrary.h"

#if defined(Q_OS_WIN)
#include "core/audio/IAudioStreamHost.h"
#include "core/audio/WasapiInputStreamWin.h"
#include "core/audio/WasapiPolicy.h"
#endif

#include <QCoreApplication>
#include <QString>

#include <portaudio.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
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

int captureHostApiIndex(const QString& driverApi, int savedHostApiIndex,
                        const QVector<QPair<int, QString>>& hostApis)
{
    if (!driverApi.isEmpty()) {
        for (const QPair<int, QString>& api : hostApis) {
            if (api.second == driverApi) {
                return api.first;
            }
        }
    }
    return savedHostApiIndex;
}

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

constexpr auto kTickInterval = std::chrono::milliseconds(10);
constexpr auto kInputLostAfter = std::chrono::milliseconds(500);
// More queued parent commands than this is a misbehaving parent; the
// supervisor never has more than a handful outstanding.
constexpr std::size_t kMaxQueuedCommands = 64;
// The clock matcher's fixed write block (xvarsamp's size), as every
// device matcher uses.
constexpr int kMatcherWriteBlockFrames = 64;
constexpr int kMinCallbackFrames = 64;

// What a stream's event sink saw (posted from a device thread).
enum StreamEventCode : int { kEventNone = 0, kEventLost = 1, kEventBusy = 2, kEventReset = 3 };

struct ParentCommand {
    P::RecordType type = P::RecordType::Shutdown;
    P::Configure configure;          // Configure only
    P::AttachRing attach;            // AttachRing only
    quint32 generation = 0;          // Open / Stop / AttachRing
    bool probeEnabled = false;       // ProbeEnable
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

// Parses one parent record.  Anything but a valid Configure / AttachRing /
// Open / Stop / Shutdown / ProbeEnable is a protocol error.
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
    case P::RecordType::AttachRing: {
        const auto attach = P::decodeAttachRing(record.payload);
        if (!attach) {
            return std::nullopt;
        }
        command.attach = *attach;
        command.generation = attach->generation;
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
    case P::RecordType::ProbeEnable: {
        const auto enabled = P::decodeProbeEnable(record.payload);
        if (!enabled) {
            return std::nullopt;
        }
        command.probeEnabled = *enabled;
        return command;
    }
    case P::RecordType::Hello:
    case P::RecordType::Status:
    case P::RecordType::Pcm:
    case P::RecordType::ProbeHit:
    case P::RecordType::RingAttached:
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

// R-AUD-17: the input callback's whole job.  It writes the picked stereo
// into the clock matcher built in the shared ring, runs the audio delay
// probe's detector (V-HW-8) on the picked channel while the probe is on,
// and posts the wake.  No lock, no allocation, no Qt call; the wake post
// is its only system call.  It skips every block until a matcher is
// published, so the stream may start before the matcher exists.
class InputTap final : public IAudioInputSink, public PortAudioBus::InputBlockHook {
public:
    void setProbeEnabled(bool enabled) { m_probeEnabled.store(enabled, std::memory_order_release); }

    // Older drivers only, before the stream opens: which device channels
    // make the mic (the native engines pick in their own callback).
    void setPick(MicChannelPick pick, int firstChannel)
    {
        m_pick = pick;
        m_firstIndex = std::max(0, firstChannel - 1);
    }

    // The callback uses these from its next block on.
    void publish(DeviceRateMatcher* matcher, CaptureShmRegion* region,
                 AudioDelayProbeDetector* detector)
    {
        m_region.store(region, std::memory_order_release);
        m_detector.store(detector, std::memory_order_release);
        m_matcher.store(matcher, std::memory_order_release);
    }

    // Only while no callback runs (the stream is closed).
    void clear()
    {
        m_matcher.store(nullptr, std::memory_order_release);
        m_detector.store(nullptr, std::memory_order_release);
        m_region.store(nullptr, std::memory_order_release);
        while (takeHit()) {
        }
    }

    // IAudioInputSink: the native engines, stereo after the pick.
    void onInput(const float* stereo, int frames, int sampleRate,
                 std::int64_t captureNsOfFrame0) override
    {
        DeviceRateMatcher* const matcher = m_matcher.load(std::memory_order_acquire);
        if (matcher == nullptr || stereo == nullptr || frames <= 0) {
            return;
        }
        matcher->write(stereo, frames, audioProbeNowNs());
        probe(stereo, frames, captureNsOfFrame0);
        static_cast<void>(sampleRate);
        m_region.load(std::memory_order_acquire)->postWake();
    }

    // PortAudioBus::InputBlockHook: the older drivers, the device's own
    // interleaved block; picked here into stereo, in fixed chunks.
    void onInputBlock(const float* interleaved, int frames, int channels, int sampleRate,
                      std::int64_t captureNsOfFrame0) override
    {
        DeviceRateMatcher* const matcher = m_matcher.load(std::memory_order_acquire);
        if (matcher == nullptr || interleaved == nullptr || frames <= 0 || channels < 1
            || sampleRate < 1) {
            return;
        }
        const int a = std::min(m_firstIndex, channels - 1);
        const int b = std::min(a + 1, channels - 1);
        for (int offset = 0; offset < frames; offset += kChunkFrames) {
            const int count = std::min(kChunkFrames, frames - offset);
            for (int f = 0; f < count; ++f) {
                const float* frame =
                    interleaved + static_cast<std::ptrdiff_t>(offset + f) * channels;
                float value = frame[a];
                if (m_pick == MicChannelPick::Right) {
                    value = frame[b];
                } else if (m_pick == MicChannelPick::Both) {
                    value = 0.5f * (frame[a] + frame[b]);
                }
                m_stereo[static_cast<std::size_t>(2 * f + 0)] = value;
                m_stereo[static_cast<std::size_t>(2 * f + 1)] = value;
            }
            matcher->write(m_stereo.data(), count, audioProbeNowNs());
            const std::int64_t chunkNs =
                captureNsOfFrame0
                + static_cast<std::int64_t>(std::llround(static_cast<double>(offset) * 1e9
                                                         / static_cast<double>(sampleRate)));
            probe(m_stereo.data(), count, chunkNs);
        }
        m_region.load(std::memory_order_acquire)->postWake();
    }

    // Main thread only.
    std::optional<std::int64_t> takeHit()
    {
        const std::uint64_t tail = m_tail.load(std::memory_order_relaxed);
        if (tail == m_head.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        const std::int64_t hit = m_hits[static_cast<std::size_t>(tail % kHitSlots)];
        m_tail.store(tail + 1, std::memory_order_release);
        return hit;
    }

private:
    static constexpr int kChunkFrames = 256;
    static constexpr std::size_t kHitSlots = 16;

    void probe(const float* stereo, int frames, std::int64_t captureNs)
    {
        if (!m_probeEnabled.load(std::memory_order_acquire)) {
            return;
        }
        AudioDelayProbeDetector* const detector = m_detector.load(std::memory_order_acquire);
        if (detector == nullptr) {
            return;
        }
        const auto hit = detector->processStereo(stereo, frames, captureNs);
        if (hit) {
            pushHit(*hit);
        }
    }

    void pushHit(std::int64_t hit)
    {
        const std::uint64_t head = m_head.load(std::memory_order_relaxed);
        if (head - m_tail.load(std::memory_order_acquire) >= kHitSlots) {
            return;                  // the main thread is behind; a click a second never fills this
        }
        m_hits[static_cast<std::size_t>(head % kHitSlots)] = hit;
        m_head.store(head + 1, std::memory_order_release);
    }

    std::atomic<bool> m_probeEnabled{false};
    std::atomic<DeviceRateMatcher*> m_matcher{nullptr};
    std::atomic<CaptureShmRegion*> m_region{nullptr};
    std::atomic<AudioDelayProbeDetector*> m_detector{nullptr};
    MicChannelPick m_pick = MicChannelPick::Left;
    int m_firstIndex = 0;
    std::array<float, kChunkFrames * 2> m_stereo{};
    std::array<std::int64_t, kHitSlots> m_hits{};
    std::atomic<std::uint64_t> m_head{0};
    std::atomic<std::uint64_t> m_tail{0};
};

int streamEventCode(AudioStreamEvent::Kind kind)
{
    switch (kind) {
    case AudioStreamEvent::Kind::DeviceLost:
        return kEventLost;
    case AudioStreamEvent::Kind::DeviceBusy:
        return kEventBusy;
    case AudioStreamEvent::Kind::FormatChanged:
    case AudioStreamEvent::Kind::ResetRequested:
        return kEventReset;
    }
    return kEventLost;
}

class Helper {
public:
    explicit Helper(std::shared_ptr<CommandQueue> queue) : m_queue(std::move(queue)) {}

    int run()
    {
        m_nextTick = Clock::now() + kTickInterval;
        for (;;) {
            std::deque<ParentCommand> batch;
            {
                std::unique_lock<std::mutex> lock(m_queue->mutex);
                const auto hasWork = [this]() { return !m_queue->commands.empty(); };
                m_queue->wake.wait_until(lock, m_nextTick, hasWork);
                batch.swap(m_queue->commands);
            }
            for (const ParentCommand& command : batch) {
                if (command.type == P::RecordType::Shutdown) {
                    shutdown();
                    return 0;
                }
                execute(command);
            }
            // The engines' streams and device systems are QObjects of this
            // thread: their queued events (a PipeWire stream's error, a
            // sound server's reconnect timer) run here, between commands.
            if (QCoreApplication::instance() != nullptr) {
                QCoreApplication::processEvents();
            }
            const auto now = Clock::now();
            if (now >= m_nextTick) {
                if (isOpen()) {
                    tick();
                }
                m_nextTick += kTickInterval;
                if (m_nextTick < now) {
                    m_nextTick = now + kTickInterval;
                }
            }
        }
    }

private:
    bool isOpen() const { return m_bus != nullptr || m_stream != nullptr; }

    void execute(const ParentCommand& command)
    {
        switch (command.type) {
        case P::RecordType::Configure:
            configure(command.configure);
            break;
        case P::RecordType::AttachRing:
            if (command.generation != m_generation) {
                qCInfo(lcAudio) << "capture helper: ignoring AttachRing for generation"
                                << command.generation << "(current" << m_generation << ")";
                break;
            }
            attachRing(command.attach);
            break;
        case P::RecordType::Open:
            if (command.generation != m_generation) {
                qCInfo(lcAudio) << "capture helper: ignoring Open for generation"
                                << command.generation << "(current" << m_generation << ")";
                break;
            }
            open();
            break;
        case P::RecordType::ProbeEnable:
            qCInfo(lcAudio) << "capture helper: audio delay probe"
                            << (command.probeEnabled ? "on" : "off");
            m_tap.setProbeEnabled(command.probeEnabled);
            break;
        case P::RecordType::Stop:
            if (command.generation != m_generation) {
                qCInfo(lcAudio) << "capture helper: ignoring Stop for generation"
                                << command.generation << "(current" << m_generation << ")";
                break;
            }
            closeInput();
            sendStatus(P::HelperState::Stopped);
            break;
        default:
            break;
        }
    }

    void configure(const P::Configure& configure)
    {
        if (isOpen()) {
            qCInfo(lcAudio) << "capture helper: reconfigured, closing generation" << m_generation;
        }
        closeInput();
        m_region.reset();
        m_ringUsed = false;
        m_generation = configure.generation;
        m_device = configure.device;
    }

    // R-AUD-17: the window's region and wake for this generation.
    void attachRing(const P::AttachRing& attach)
    {
        closeInput();
        m_region.reset();
        m_ringUsed = false;
        m_region = CaptureShmRegion::attach({attach.memory, attach.wake},
                                            static_cast<std::size_t>(attach.bytes));
        if (!m_region) {
            sendFailure(P::FailReason::Internal,
                        QStringLiteral("could not attach the shared ring"));
            return;
        }
        m_ringInRate = attach.inRate;
        write(P::encodeRingAttached({m_generation}));
    }

    void open()
    {
        if (m_generation == 0) {
            return;
        }
        if (isOpen()) {
            qCInfo(lcAudio) << "capture helper: generation" << m_generation << "already open";
            return;
        }
        closeInput();

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

        // R-R3-21, R-AUD-32: a test run never makes an engine backend,
        // initialises PortAudio or opens a real device, the same decision
        // AudioEngine makes. The named device is looked up on the test's
        // list (setCaptureHelperTestDevices), so a missing device still
        // fails as one, with the bus's own words.
        if (PortAudioBus::portAudioBarredForTestRun() || audioDevicesBarredForTestRun()) {
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

        if (!m_region) {
            sendFailure(P::FailReason::Internal,
                        QStringLiteral("no shared ring for this generation"));
            return;
        }
        if (m_ringUsed) {
            // The window's reader may still be attached to the ring's
            // header; it is never rebuilt under it.
            sendFailure(P::FailReason::Internal,
                        QStringLiteral("the shared ring of this generation was used"));
            return;
        }

        // A config with no engine (only before the migration) is the older
        // drivers, as AudioEngine::makeBus treats it.
        const AudioEngineKind engine = m_device.engine.value_or(AudioEngineKind::PortAudio);
        if (engine == AudioEngineKind::PortAudio) {
            openOlderDriver();
        } else {
            openNative(engine);
        }
    }

    IAudioEngineBackend* backendFor(AudioEngineKind engine)
    {
        if (!m_backendsMade) {
            AudioBackendContext context;
            context.helper = true;
            m_backends = makeSystemAudioBackends(context);
            m_backendsMade = true;
        }
        const AudioBackendId id = audioBackendFor(engine);
        for (const std::shared_ptr<IAudioEngineBackend>& backend : m_backends) {
            if (backend && backend->id() == id) {
                return backend.get();
            }
        }
        return nullptr;
    }

    // R-AUD-17: the input through the registry's backend for the saved
    // engine, matched with the saved identity (R-AUD-04).  The helper never
    // picks a device itself (R-AUD-09, R-AUD-14).
    void openNative(AudioEngineKind engine)
    {
        IAudioEngineBackend* backend = backendFor(engine);
        if (backend == nullptr) {
            sendFailure(P::FailReason::OpenFailed,
                        audioEngineLabel(engine) + QStringLiteral(" is not available here"));
            return;
        }
        if (!backend->running()) {
            sendFailure(P::FailReason::OpenFailed,
                        audioEngineLabel(engine) + QStringLiteral(" is not running"));
            return;
        }
        QList<AudioDeviceInfo> inputs;
        for (const AudioDeviceInfo& info : backend->enumerate()) {
            if (info.direction == AudioDeviceDirection::Input) {
                inputs.append(info);
            }
        }

        AudioStreamRequest request;
        request.direction = AudioDeviceDirection::Input;
        std::optional<AudioDeviceInfo> device;
        if (m_device.isPlatformDefault()) {
            for (const AudioDeviceInfo& info : inputs) {
                if (info.isDefault) {
                    device = info;
                    break;
                }
            }
            if (!device && inputs.isEmpty()) {
                sendFailure(P::FailReason::DeviceNotFound, QStringLiteral("No input device found"));
                return;
            }
            // R-AUD-13, R-AUD-14: the platform default is never a Bluetooth
            // mic; one opens only when it is picked by name.
            if (device && device->transport == AudioTransport::Bluetooth) {
                sendFailure(P::FailReason::DeviceNotFound,
                            QStringLiteral("the system default input is a Bluetooth mic, "
                                           "which opens only when picked by name"));
                return;
            }
            request.deviceId = device ? device->id : QString();
        } else {
            const std::optional<AudioDeviceMatch> match = matchSavedAudioDevice(m_device, inputs);
            if (!match) {
                const QString name = m_device.deviceName.trimmed().isEmpty()
                                         ? m_device.deviceId
                                         : m_device.deviceName.trimmed();
                sendFailure(P::FailReason::DeviceNotFound,
                            QStringLiteral("device-not-found: ") + name);
                return;
            }
            device = match->device;
            request.deviceId = device->id;
        }

        const int deviceChannels = device ? std::max(1, device->channelCount) : 2;
        request.pair.firstChannel = std::clamp(m_device.firstChannel, 1, deviceChannels);
        request.pair.channelCount = (deviceChannels - request.pair.firstChannel + 1 >= 2) ? 2 : 1;
        request.sampleRate = m_device.sampleRate;
        // R-AUD-16: Windows audio, shared runs at the engine's smallest
        // period (0); the other engines keep the saved buffer size.
        request.bufferFrames = engine == AudioEngineKind::WindowsShared
                                   ? 0
                                   : std::max(0, m_device.bufferSamples);
        request.delayMs = std::max(0, m_device.delayMs);
        request.exclusive = engine == AudioEngineKind::WindowsExclusive;

        std::unique_ptr<IAudioInputStream> stream =
            backend->createInput(request, m_device.micChannel, &m_tap);
        if (!stream) {
            sendFailure(P::FailReason::OpenFailed,
                        audioEngineLabel(engine) + QStringLiteral(" has no input for this device"));
            return;
        }
        // Before open(), so a busy device reported during the open is seen
        // (an engine may post DeviceBusy from inside open()).
        m_streamEvent.store(kEventNone, std::memory_order_release);
        std::atomic<int>* const events = &m_streamEvent;
        stream->setStreamEventSink([events](const AudioStreamEvent& event) {
            int expected = kEventNone;
            events->compare_exchange_strong(expected, streamEventCode(event.kind),
                                            std::memory_order_acq_rel);
        });
        const bool opened = stream->open();
        bool busy = m_streamEvent.load(std::memory_order_acquire) == kEventBusy;
#if defined(Q_OS_WIN)
        if (const auto* wasapi = dynamic_cast<const WasapiInputStreamWin*>(stream.get())) {
            busy = busy || wasapiOpenResult(wasapi->lastOpenResult()) == AudioOpenResult::InUse;
        }
#endif
        if (!opened) {
            const QString why = stream->errorString();
            qCWarning(lcAudio) << "capture helper: open failed:" << why;
            sendFailure(busy ? P::FailReason::DeviceInUse : P::FailReason::OpenFailed,
                        why.isEmpty() ? QStringLiteral("the input did not open") : why);
            return;
        }
        if (busy) {
            m_streamEvent.store(kEventNone, std::memory_order_release);
        }
        const int rate = stream->sampleRate();
        const std::optional<std::int64_t> latencyNs = stream->inputLatencyNs();
        const int latencyUs = latencyNs && *latencyNs > 0
                                  ? static_cast<int>(std::min<std::int64_t>(
                                      *latencyNs / 1000, P::kMaxLatencyUs))
                                  : 0;
        m_stream = std::move(stream);
        finishOpen(rate, request.pair.channelCount, device ? device->name : QString(), latencyUs,
                   request.bufferFrames);
    }

    // The older drivers: PortAudio through PortAudioBus, as before, with
    // the device's own block delivered to the tap.
    void openOlderDriver()
    {
        if (!m_paInitialized) {
            std::lock_guard<std::recursive_mutex> paLock(PortAudioLibrary::mutex());
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
        AudioFormat format;
        format.sampleRate = P::kSampleRate;
        format.channels = 1;
        format.sample = AudioFormat::Sample::Float32;
        m_tap.setPick(m_device.micChannel, m_device.firstChannel);
        bool opened = false;
        {
            // R-AUD-02 (bug 1): the saved host API's index, looked up and
            // opened under one hold of the library lock so the numbering
            // cannot change in between.
            std::lock_guard<std::recursive_mutex> paLock(PortAudioLibrary::mutex());
            QVector<QPair<int, QString>> listed;
            for (const PortAudioBus::HostApiInfo& api : PortAudioBus::hostApis()) {
                listed.append({api.index, api.name});
            }
            PortAudioConfig config;
            config.direction = AudioDirection::Input;
            config.hostApiIndex =
                captureHostApiIndex(m_device.driverApi, m_device.hostApiIndex, listed);
            config.deviceName = m_device.deviceName;
            config.bufferSamples = m_device.bufferSamples;
            config.exclusiveMode = m_device.exclusiveMode;
            bus->setConfig(config);
            bus->setStrictInputDevice(true);
            bus->setInputBlockHook(&m_tap);
            opened = bus->open(format);
        }

        if (!opened) {
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
        const int rate = bus->openedNativeRate();
        const int channels = bus->openedStreamChannels();
        const QString name = bus->openedDeviceName();
        m_bus = std::move(bus);
        finishOpen(rate, channels, name, 0, m_device.bufferSamples);
    }

    // Builds the matcher in the shared ring at the stream's own rate,
    // publishes it to the callback and reports Ready.
    void finishOpen(int rate, int channels, const QString& deviceName, int latencyUs,
                    int bufferFrames)
    {
        DeviceRateMatcher::Config config;
        config.inRate = rate;
        config.outRate = P::kSampleRate;
        config.writeBlockFrames = kMatcherWriteBlockFrames;
        config.callbackFrames = std::clamp(bufferFrames > 0 ? bufferFrames : m_device.bufferSamples,
                                           kMinCallbackFrames, P::kMaxBufferFrames);
        config.delayMs = std::max(0, m_device.delayMs);
        const std::size_t needed = DeviceRateMatcher::ringBytes(config);
        if (needed == 0 || needed > m_region->size() || rate < m_ringInRate) {
            closeInput();
            sendFailure(P::FailReason::Internal,
                        QStringLiteral("the shared ring does not fit the input at %1 Hz").arg(rate));
            return;
        }
        m_ringUsed = true;
        m_matcher = std::make_unique<DeviceRateMatcher>(config, m_region->data(), m_region->size());
        if (!m_matcher->valid()) {
            closeInput();
            sendFailure(P::FailReason::Internal,
                        QStringLiteral("the clock matcher cannot run at %1 Hz").arg(rate));
            return;
        }

        P::Status ready;
        ready.generation = m_generation;
        ready.state = P::HelperState::Ready;
        ready.actualDevice = clampText(deviceName);
        ready.nativeRate = rate;
        ready.nativeChannels = channels;
        ready.latencyUs = latencyUs;
        ready.bufferFrames = config.callbackFrames;
        const QByteArray record = P::encodeStatus(ready);
        if (record.isEmpty()) {
            closeInput();
            sendFailure(P::FailReason::Internal,
                        QStringLiteral("opened stream reports an unsupported format: %1 Hz, %2 channels")
                            .arg(ready.nativeRate)
                            .arg(ready.nativeChannels));
            return;
        }
        qCInfo(lcAudio) << "capture helper: generation" << m_generation << "ready on"
                        << ready.actualDevice << ready.nativeRate << "Hz"
                        << ready.nativeChannels << "ch";

        // V-HW-8: the detector at the stream's own rate.
        m_detector = std::make_unique<AudioDelayProbeDetector>(rate);
        m_tap.publish(m_matcher.get(), m_region.get(), m_detector.get());
        m_lastWriteNs = 0;
        m_lastProgress = Clock::now();
        m_nextTick = m_lastProgress + kTickInterval;
        write(record);
    }

    void tick()
    {
        while (const auto hit = m_tap.takeHit()) {
            write(P::encodeProbeHit(*hit));
        }
        const int event = m_streamEvent.exchange(kEventNone, std::memory_order_acq_rel);
        if (event == kEventBusy) {
            qCWarning(lcAudio) << "capture helper: the input is in use by another program";
            closeInput();
            sendFailure(P::FailReason::DeviceInUse,
                        QStringLiteral("the input is in use by another program"));
            return;
        }
        if (event == kEventLost || event == kEventReset) {
            qCWarning(lcAudio) << "capture helper: the input went away on generation"
                               << m_generation;
            closeInput();
            sendFailure(P::FailReason::InputLost,
                        event == kEventLost ? QStringLiteral("the input device went away")
                                            : QStringLiteral("the input device was reset"));
            return;
        }
        // No block written for 500 ms: the input is lost.
        const auto now = Clock::now();
        const std::int64_t lastWrite = (m_matcher && m_matcher->ring() != nullptr)
            ? m_matcher->ring()->lastWriteNs.load(std::memory_order_acquire)
            : 0;
        if (lastWrite != m_lastWriteNs) {
            m_lastWriteNs = lastWrite;
            m_lastProgress = now;
        } else if (now - m_lastProgress >= kInputLostAfter) {
            qCWarning(lcAudio) << "capture helper: no input for 500 ms on generation"
                               << m_generation;
            closeInput();
            sendFailure(P::FailReason::InputLost,
                        QStringLiteral("the input produced no audio for 500 ms"));
        }
    }

    void closeInput()
    {
        if (m_stream) {
            m_stream->close();
            m_stream.reset();
        }
        if (m_bus) {
            m_bus->close();
            m_bus.reset();
        }
        // No callback runs now; the matcher, the detector and any unsent
        // hit go with it.  The ring's memory stays mapped for the window.
        m_tap.clear();
        m_detector.reset();
        m_matcher.reset();
        m_streamEvent.store(kEventNone, std::memory_order_release);
    }

    void shutdown()
    {
        closeInput();
        m_region.reset();
        if (m_paInitialized) {
            std::lock_guard<std::recursive_mutex> paLock(PortAudioLibrary::mutex());
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
    // Declared before the streams so they, and their callbacks, go first.
    bool m_backendsMade = false;
    std::vector<std::shared_ptr<IAudioEngineBackend>> m_backends;
    InputTap m_tap;
    std::unique_ptr<CaptureShmRegion> m_region;
    int m_ringInRate = 0;
    bool m_ringUsed = false;
    std::unique_ptr<DeviceRateMatcher> m_matcher;
    std::unique_ptr<AudioDelayProbeDetector> m_detector;
    std::atomic<int> m_streamEvent{kEventNone};
    std::unique_ptr<PortAudioBus> m_bus;
    std::unique_ptr<IAudioInputStream> m_stream;
    std::int64_t m_lastWriteNs = 0;
    Clock::time_point m_lastProgress;
    Clock::time_point m_nextTick;
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
