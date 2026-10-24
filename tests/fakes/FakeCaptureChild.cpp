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
//   2026-10-09: native audio plan final fix wave (V-HW-8): the probe
//               scenario marks a disable with a "Probe off" AsioCaps
//               record.  J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-11: pcm-record builds no ring, so the window cannot be Ready
//               before it reads the record.  Three scenarios act on a
//               ProbeEnable instead of a clock (pcm-record-, crash- and
//               input-lost-on-probe-enable), so a test that turns the
//               probe on once it has seen Ready fixes the order.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11: permission-while-marked holds Permission until the test
//               removes a file, in place of a set time.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-12: generation-tagged open-step diagnostic regression.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-16: Warm is answered with Warmed (the persistent helper);
//               the hang-warm, warm-while-marked and exit-after-warm
//               scenarios.  J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-16: a Warm that names a bundle is answered as a stored Voice
//               Isolation made Standard.  J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-23: every exit goes through CaptureHelperIo::exitProcessNow,
//               which on Windows runs no DLL static destructor.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "FakeCaptureChild.h"

#include "core/audio/AudioDelayProbe.h"
#include "core/audio/CaptureHelper.h"
#include "core/audio/CaptureProtocol.h"
#include "core/audio/CaptureShm.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/MatcherRing.h"

#include <cstdio>
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
    Ready, HangOpen, HangOpenAfterStep, HangOpenAfterTaggedSteps, NoHello, PermissionThenReady, CrashAfterReady,
    Malformed, Oversize, InputLost, IgnoreStop, Stale, Probe, Version1,
    PcmRecord, PcmRecordOnProbeEnable, CrashOnProbeEnable, InputLostOnProbeEnable,
    Busy, BusyWhileMarked, BusyWhileMarkedThenPending, BadRing,
    PermissionWhileMarked, HangWarm, WarmWhileMarked, ExitAfterWarm, WarmWrongMicMode
};

std::optional<Scenario> scenarioFromName(const QString& name)
{
    if (name == QLatin1String("ready")) { return Scenario::Ready; }
    if (name == QLatin1String("hang-open")) { return Scenario::HangOpen; }
    if (name == QLatin1String("hang-open-after-step")) { return Scenario::HangOpenAfterStep; }
    if (name == QLatin1String("hang-open-after-tagged-steps")) { return Scenario::HangOpenAfterTaggedSteps; }
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
    if (name == QLatin1String("pcm-record-on-probe-enable")) {
        return Scenario::PcmRecordOnProbeEnable;
    }
    if (name == QLatin1String("crash-on-probe-enable")) {
        return Scenario::CrashOnProbeEnable;
    }
    if (name == QLatin1String("input-lost-on-probe-enable")) {
        return Scenario::InputLostOnProbeEnable;
    }
    if (name == QLatin1String("busy")) { return Scenario::Busy; }
    if (name == QLatin1String("busy-while-marked")) { return Scenario::BusyWhileMarked; }
    if (name == QLatin1String("busy-while-marked-then-pending")) {
        return Scenario::BusyWhileMarkedThenPending;
    }
    if (name == QLatin1String("bad-ring")) { return Scenario::BadRing; }
    if (name == QLatin1String("permission-while-marked")) {
        return Scenario::PermissionWhileMarked;
    }
    if (name == QLatin1String("hang-warm")) { return Scenario::HangWarm; }
    if (name == QLatin1String("warm-while-marked")) { return Scenario::WarmWhileMarked; }
    if (name == QLatin1String("exit-after-warm")) { return Scenario::ExitAfterWarm; }
    if (name == QLatin1String("warm-wrong-mic-mode")) { return Scenario::WarmWrongMicMode; }
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
    QString micModeBundle;         // Warm
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
            CaptureHelperIo::exitProcessNow(0);
        }
        reader.append(buffer.data(), got);
        while (auto record = reader.next()) {
            Command command;
            command.type = record->type;
            if (record->type == P::RecordType::Configure) {
                const auto configure = P::decodeConfigure(record->payload);
                if (!configure) {
                    CaptureHelperIo::exitProcessNow(0);
                }
                command.generation = configure->generation;
                command.deviceName = configure->device.deviceName;
            } else if (record->type == P::RecordType::Open || record->type == P::RecordType::Stop) {
                const auto decoded = P::decodeCommand(record->payload);
                if (!decoded) {
                    CaptureHelperIo::exitProcessNow(0);
                }
                command.generation = decoded->generation;
            } else if (record->type == P::RecordType::AttachRing) {
                const auto attach = P::decodeAttachRing(record->payload);
                if (!attach) {
                    CaptureHelperIo::exitProcessNow(0);
                }
                command.generation = attach->generation;
                command.ring = {attach->memory, attach->wake};
                command.ringBytes = attach->bytes;
            } else if (record->type == P::RecordType::ProbeEnable) {
                const auto enabled = P::decodeProbeEnable(record->payload);
                if (!enabled) {
                    CaptureHelperIo::exitProcessNow(0);
                }
                command.probeEnabled = *enabled;
            } else if (record->type == P::RecordType::AsioDescribe) {
                const auto describe = P::decodeAsioDescribe(record->payload);
                if (!describe) {
                    CaptureHelperIo::exitProcessNow(0);
                }
                command.asioDriver = describe->driver;
            } else if (record->type == P::RecordType::AsioOpen) {
                const auto open = P::decodeAsioOpen(record->payload);
                if (!open) {
                    CaptureHelperIo::exitProcessNow(0);
                }
                command.asioOpen = *open;
            } else if (record->type == P::RecordType::Warm) {
                const auto warm = P::decodeWarm(record->payload);
                if (!warm) {
                    CaptureHelperIo::exitProcessNow(0);
                }
                command.micModeBundle = warm->micModeBundle;
            } else if (record->type != P::RecordType::Shutdown
                       && record->type != P::RecordType::AsioControlPanel) {
                CaptureHelperIo::exitProcessNow(0);
            }
            std::lock_guard<std::mutex> lock(queue->mutex);
            queue->commands.push_back(command);
            queue->wake.notify_one();
        }
        if (reader.error() != P::RecordReader::Error::None) {
            CaptureHelperIo::exitProcessNow(0);
        }
    }
}

void send(const QByteArray& record)
{
    if (!CaptureHelperIo::writeRecord(record)) {
        CaptureHelperIo::exitProcessNow(0);
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
                // warm-while-marked: the helper's commands run in order,
                // so everything after a held Warm waits behind it.
                if (m_warmHeld) {
                    m_afterWarm.push_back(command);
                    continue;
                }
                if (command.type == P::RecordType::Shutdown) {
                    return 0;
                }
                handle(command);
            }
            tick();
            while (!m_warmHeld && !m_ignoring && !m_afterWarm.empty()) {
                const Command command = m_afterWarm.front();
                m_afterWarm.pop_front();
                if (command.type == P::RecordType::Shutdown) {
                    return 0;
                }
                handle(command);
            }
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
            m_permissionHeld = false;
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
            if (actsOnProbeEnable()) {
                // A test turns the probe on once it has seen Ready and the
                // window passes that on at once, so the act always follows
                // a published Ready; nothing waits a fixed time for it.
                if (command.probeEnabled) {
                    actOnProbeEnable();
                }
                break;
            }
            if (m_scenario != Scenario::Probe) {
                break;
            }
            if (command.probeEnabled && !m_readySent) {
                CaptureHelperIo::exitProcessNow(kProbeEnabledBeforeReadyExit);
            }
            m_probeOn = command.probeEnabled;
            if (m_probeOn) {
                ++m_probeEnables;
                m_probeHits = 0;
                m_nextProbeHit = Clock::now();
            } else {
                // The test's mark that the disable reached the helper: hits
                // stop here, so nothing waits a fixed time for them.
                P::AsioCapsRecord off;
                off.drivers = {QStringLiteral("Fake ASIO")};
                off.driver = QStringLiteral("Probe off");
                send(P::encodeAsioCaps(off));
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
        case P::RecordType::Warm:
            if (m_scenario == Scenario::HangWarm) {
                m_ignoring = true;
                markHanging();
                break;
            }
            if (m_scenario == Scenario::WarmWhileMarked && warmMarked()) {
                m_warmHeld = true;
                m_heldMicModeBundle = command.micModeBundle;
                markHanging();
                break;
            }
            sendWarmed(command.micModeBundle);
            if (m_scenario == Scenario::ExitAfterWarm) {
                CaptureHelperIo::exitProcessNow(3);
            }
            break;
        case P::RecordType::Stop:
            if (command.generation == m_generation && m_scenario != Scenario::IgnoreStop) {
                m_streaming = false;
                m_matcher.reset();
                m_readySent = false;
                m_probeOn = false;
                m_readyAt.reset();
                m_permissionHeld = false;
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

    void sendWarmed(const QString& micModeBundle)
    {
        P::Warmed warmed;
        warmed.inputs = 1;
        warmed.startNs = audioProbeNowNs();
        warmed.listedNs = warmed.startNs;
        // warm-wrong-mic-mode answers the question its Warm did not ask.
        const bool asked = !micModeBundle.isEmpty();
        if (asked != (m_scenario == Scenario::WarmWrongMicMode)) {
            warmed.micMode = MicModeResult{MicModeOutcome::MadeStandard, kMicModeVoiceIsolation};
        }
        send(P::encodeWarmed(warmed));
    }

    // warm-while-marked: the hold lasts while this file exists.
    static bool warmMarked()
    {
        return QFile::exists(qEnvironmentVariable("NEREUS_FAKE_CAPTURE_WARM_FILE"));
    }

    // hang-open and hang-warm: once every later command is ignored, and
    // warm-while-marked: once its Warm is held, creates <dir>/<pid> when
    // NEREUS_FAKE_CAPTURE_HANG_DIR names a directory, so a test can tell
    // that the helper is now really hanging (before that, a Shutdown still
    // ends it).
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
        case Scenario::HangOpenAfterStep:
            sendStatus(m_generation, P::HelperState::Opening);
            std::fprintf(stderr, "nereus-audio-capture: open step: devices listed at 12 ms\n");
            std::fflush(stderr);
            m_ignoring = true;
            return;
        case Scenario::HangOpenAfterTaggedSteps: {
            sendStatus(m_generation, P::HelperState::Opening);
            const long long pid = QCoreApplication::applicationPid();
            std::fprintf(stderr, "nereus-audio-capture: open step: [pid=%lld generation=%u] current query at 12 ms\n",
                         pid, static_cast<unsigned int>(m_generation));
            std::fprintf(stderr, "nereus-audio-capture: open step: [pid=%lld generation=%u] stale generation at 20 ms\n",
                         pid, static_cast<unsigned int>(m_generation - 1));
            std::fprintf(stderr, "nereus-audio-capture: open step: [pid=%lld generation=%u] wrong process at 30 ms\n",
                         pid + 1, static_cast<unsigned int>(m_generation));
            std::fprintf(stderr, "nereus-audio-capture: open step: [pid=invalid generation=invalid] malformed tag at 40 ms\n");
            std::fflush(stderr);
            m_ignoring = true;
            return;
        }
        case Scenario::PermissionThenReady:
            sendStatus(m_generation, P::HelperState::Permission);
            m_readyAt = Clock::now() + kPermissionHold;
            return;
        case Scenario::PermissionWhileMarked:
            sendStatus(m_generation, P::HelperState::Permission);
            m_permissionHeld = true;
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
            // No ring is built and no wake is posted: the window's Ready
            // needs the ring's first wake, so it reads the record first
            // however the pipe splits the two.
            sendStatus(m_generation, P::HelperState::Opening);
            sendStatus(m_generation, P::HelperState::Ready);
            m_readySent = true;
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

    bool actsOnProbeEnable() const
    {
        return m_scenario == Scenario::PcmRecordOnProbeEnable
            || m_scenario == Scenario::CrashOnProbeEnable
            || m_scenario == Scenario::InputLostOnProbeEnable;
    }

    void actOnProbeEnable()
    {
        if (m_scenario == Scenario::PcmRecordOnProbeEnable) {
            send(P::encodePcm(m_generation, 0, 0, m_tone.data(), P::kHelperPcmFrames));
        } else if (m_scenario == Scenario::CrashOnProbeEnable) {
            CaptureHelperIo::exitProcessNow(3);
        } else if (m_scenario == Scenario::InputLostOnProbeEnable) {
            m_streaming = false;
            sendStatus(m_generation, P::HelperState::Failed, P::FailReason::InputLost,
                       QStringLiteral("fake input stopped"));
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
        // warm-while-marked: likewise for the warm-up.
        if (m_warmHeld && !warmMarked()) {
            m_warmHeld = false;
            sendWarmed(m_heldMicModeBundle);
        }
        // permission-while-marked: the test ends the hold by removing the
        // file, so the hold is as long as the test needs and no longer.
        if (m_permissionHeld
            && !QFile::exists(qEnvironmentVariable("NEREUS_FAKE_CAPTURE_PERMISSION_FILE"))) {
            m_permissionHeld = false;
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
                CaptureHelperIo::exitProcessNow(3);
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
    bool m_permissionHeld = false;                  // permission-while-marked
    bool m_warmHeld = false;                        // warm-while-marked
    QString m_heldMicModeBundle;                    // the held Warm's bundle
    std::deque<Command> m_afterWarm;                // commands behind the held Warm
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
