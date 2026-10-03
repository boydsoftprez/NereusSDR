// =================================================================
// src/core/session/media/LibDataChannelMediaTransport.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R3 Task 1.
// See LibDataChannelMediaTransport.h for the boundary contract.
//
// Modification history (NereusSDR):
//   2026-09-25: iPhone app plan Task 36 (R-IOS-13): the microphone line, a
//               second audio m-line (mid "mic") receive-only at the Core,
//               offered only when asked. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 37 (R-IOS-13): the "tx" data channel
//               (unordered, never retransmitted) for the transmit
//               keepalive, created and taken only when asked, and the
//               receive-severed test seam. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 27 (R-IOS-16): the ICE settings of a
//               connection that came through the remote access service
//               (StartOptions::ice; see IceConfiguration.h for the pinned
//               libdatachannel and libjuice limits they follow). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: Task 27 follow-up: the far end's relay candidates are kept
//               for selectedPath(). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//
//   2026-09-27: iPhone app plan Task 29 step 2b (R-IOS-16, R-IOS-08): the
//               web relay's leg (RelayLeg) and its per-connection candidate
//               sources; the computer's own proxy settings (SystemProxy).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28: addendum G-127: a drain settles readiness before it
//               delivers what it collected, so a message that arrives with
//               the display channel's opening is reported after ready().
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: the opt-in ICE check log (IceDiagnostics, NEREUS_ICE_DIAG):
//               this peer's candidates, the ones it admits, its states and
//               its selected pair, redacted. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-29: direct media fix wave: a connection on its own candidate
//               source alone (IceConfiguration::onlySourceCandidates)
//               takes no signalled candidate and sends none of its own.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: ICE check log review fix: a local candidate is logged
//               where it is sent, or as not sent on the tunnel alone, and
//               the tunnel-only refusal of a remote candidate is logged.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: LINK minor 8: a connection's end (PeerFailed,
//               PeerClosed) is never dropped from a full event queue.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-09-30: TX stall lane: micRtpReceived carries heldUs, how long
//               the packet waited between its receipt in the transport
//               and its report, so the microphone buffer times it at
//               receipt. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-01: TX mic thread (JJ approved): with a sink installed
//               (setMicPacketSink), the microphone line's packets go from
//               the library's callback to a thread of the line's own
//               ("NereusMicRx"), which hands them to the sink, so a stall
//               of the owner's event loop never holds them; the "tx"
//               channel's messages carry their receipt (txReceived's
//               heldUs). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-01: TX diagnostics lane: the "media RTP timing" warning again
//               covers the microphone line, from the line's own thread: a
//               gap between the line's packet receipts, or a wait on the
//               line, over the same threshold, at most once a second. Log
//               only. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-10-01: TX diagnostics lane, review round: the line's warning is
//               MicLineTimingWatch's: a gap past 2 s is the line starting
//               again and neither warns nor takes the once-a-second slot;
//               the warning is logged after the batch is delivered. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: Control logging lane: rttMs() and the selected
//               pair's candidate transports. Logging only. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: Mic 48k lane (JJ's ruling: the phone microphone at 48
//               kbit/s): the microphone line's offer advertises
//               maxaveragebitrate=48000 (kMicLineMaxAverageBitrate), the
//               ceiling the phone's 48 kbit/s encoder must respect under
//               RFC 7587 section 6.1. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/session/media/LibDataChannelMediaTransport.h"
#include "core/session/CandidateSourceLease.h"
#include "core/session/IceDiagnostics.h"
#include "core/session/media/PcmAudioCodec.h"

#include <QDebug>
#include <QPointer>
#include <QThread>
#include <QTimer>

#include <rtc/rtc.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace NereusSDR {

namespace {

constexpr char kDisplayLabel[] = "display";
constexpr char kIqLabel[] = "iq";
// Task 37: the transmit keepalive's channel.
constexpr char kTxLabel[] = "tx";
constexpr char kAudioMid[] = "audio";
// Task 36: the microphone line and its a=ssrc cname.
constexpr char kMicMid[] = "mic";
constexpr char kMicStreamName[] = "nereus-microphone";
constexpr int kOpusPayloadType = 111;
constexpr std::size_t kMaxPendingEvents = 128;
// R-R3-21: a stall of about 480 ms releases about 14 display messages at
// once, all within one drain. The window decodes and plays them out on their
// timestamps, so the queue holds a whole burst (8 dropped the middle of the
// delta chain and cost a keyframe wait); the byte cap still bounds it.
constexpr std::size_t kMaxPendingDisplayMessages = 32;
constexpr std::size_t kMaxPendingDisplayBytes = 256 * 1024;
constexpr std::size_t kMaxPendingIqMessages = 8;
constexpr std::size_t kMaxPendingIqBytes =
    kMaxPendingIqMessages * IMediaTransport::kMaxIqMessageBytes;
// R-R3-43: every declared audio stream keeps the 256 ms of lossless cushion
// the one main stream had: 64 packets at 250 packets/s.
static_assert(IMediaTransport::kReceivedRtpPacketsPerStream * 1000
                      / (PcmAudioCodecConfig::kSampleRate / PcmAudioCodecConfig::kPacketFrames)
                  >= 256,
              "each audio stream's receive queue must hold 256 ms of lossless audio");
constexpr char kMainAudioStreamName[] = "nereus-mixed-stereo";
constexpr char kReceiverAudioStreamPrefix[] = "nereus-receiver-";
// R-R3-45: the headphones mix's a=ssrc cname.
constexpr char kHeadphonesAudioStreamName[] = "nereus-headphones-mix";
constexpr auto kRtpTimingWarningThreshold = std::chrono::milliseconds(80);
constexpr auto kRtpTimingWarningInterval = std::chrono::seconds(1);
static_assert(MicLineTimingWatch::kThreshold == kRtpTimingWarningThreshold
                  && MicLineTimingWatch::kInterval == kRtpTimingWarningInterval,
              "the microphone line warns as the owner does");

struct CallbackEvent {
    enum class Kind {
        Description,
        Candidate,
        Error,
        DisplayError,
        DisplayWritable,
        IqError,
        PeerClosed,
        PeerFailed,
        // Task 37: a message on the "tx" channel (in `first`).
        TxMessage,
        // Task 27: every local candidate has been reported.
        GatheringComplete,
    };

    Kind kind;
    std::string first;
    std::string second;
    // TX mic thread: when a TxMessage came off the network.
    std::chrono::steady_clock::time_point receivedAt{};
};

struct PendingRtpPacket {
    rtc::binary data;
    std::chrono::steady_clock::time_point receivedAt;
    // Task 36: arrived on the microphone line.
    bool mic = false;
};

// TX mic thread (JJ approved 2026-10-01): the microphone line's own thread.
// The library's callback queues the line's packets here instead of for the
// owner's 2 ms drain, and the thread hands them to the installed sink (the
// receiver, which decodes and fills the transmit feed), so a stall of the
// owner's event loop never holds the line. One thread per transport with a
// microphone line: a Core carries a line for each transmitting device, a
// handful at most, each thread asleep between packets, and the thread's
// life is the transport's start to stop, with nothing shared between
// connections to tear down.
struct MicLane {
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<PendingRtpPacket> packets;
    bool stopping = false;
    // Whether a sink is installed (read by the callback without sinkMutex).
    std::atomic<bool> hasSink{false};
    // Held while the sink runs and while it is replaced, so a replaced sink
    // is never called after setMicPacketSink() returns.
    std::mutex sinkMutex;
    IMediaTransport::MicPacketSink sink;
};

std::once_flag g_sctpSettingsOnce;
std::atomic<int> g_sctpSettingsApplications{0};
std::atomic<quint64> g_peersCreated{0};
std::atomic<quint64> g_peersCreatedBeforeSctpSettings{0};

struct CallbackBridge {
    std::mutex mutex;
    std::condition_variable displayReceiveGate;
    bool displayReceiveStalledForTest = false;
    bool cancelled = false;
    bool overflowReported = false;
    std::deque<CallbackEvent> events;
    std::deque<rtc::binary> displayMessages;
    std::size_t displayBytes = 0;
    std::deque<rtc::binary> iqMessages;
    std::size_t iqBytes = 0;
    bool iqFailed = false;
    bool iqFailureReported = false;
    std::deque<PendingRtpPacket> rtpPackets;
    // R-R3-43: kReceivedRtpPacketsPerStream for each declared audio stream.
    std::size_t rtpPacketCapacity =
        static_cast<std::size_t>(IMediaTransport::kReceivedRtpPacketsPerStream);
    std::chrono::steady_clock::time_point lastRtpReceipt;
    std::chrono::steady_clock::duration maxRtpCallbackGap {};
    std::size_t droppedRtpPackets = 0;
    std::shared_ptr<rtc::DataChannel> dataChannel;
    std::shared_ptr<rtc::Track> track;
    // Task 36: the answerer's microphone line, and the SSRC it declares on
    // it (0: no microphone line).
    std::shared_ptr<rtc::Track> micTrack;
    quint32 micSsrc = 0;
    bool dataChannelAssigned = false;
    bool trackAssigned = false;
    bool micTrackAssigned = false;
    // TX mic thread: the line's own queue, while it has one.
    std::shared_ptr<MicLane> micLane;
    // Task 37: whether this side takes a "tx" channel, and the answerer's
    // once it arrived.
    bool txChannelWanted = false;
    bool txChannelAssigned = false;
    std::shared_ptr<rtc::DataChannel> txChannel;
    bool iqChannelWanted = false;
    bool iqChannelAssigned = false;
    std::shared_ptr<rtc::DataChannel> iqChannel;
    // Task 37 test seam: drop everything that arrives.
    bool receiveSeveredForTest = false;
    std::atomic<quint64> receivedDisplayPayloadBytes{0};
    std::atomic<quint64> submittedDisplayPayloadBytes{0};
    std::atomic<quint64> receivedRtpBytes{0};
    std::atomic<quint64> submittedRtpBytes{0};
    std::atomic<quint64> displayMessagesDropped{0};
    std::atomic<quint64> receivedTxPayloadBytes{0};
    std::atomic<quint64> submittedTxPayloadBytes{0};
    std::atomic<quint64> receivedIqPayloadBytes{0};
    std::atomic<quint64> submittedIqPayloadBytes{0};
};

void queueEvent(const std::weak_ptr<CallbackBridge>& weak,
                CallbackEvent::Kind kind,
                std::string first = {}, std::string second = {})
{
    const auto bridge = weak.lock();
    if (!bridge) {
        return;
    }
    std::lock_guard lock(bridge->mutex);
    if (bridge->cancelled) {
        return;
    }
    // LINK minor 8: the connection's end (PeerFailed, PeerClosed) is never
    // dropped, however full the queue is: a lost end would leave the
    // session holding a media connection that is gone. A connection
    // reports each only a few times, so they stay bounded past the cap.
    const auto isLifecycle = [](CallbackEvent::Kind k) {
        return k == CallbackEvent::Kind::PeerFailed || k == CallbackEvent::Kind::PeerClosed;
    };
    if (isLifecycle(kind)) {
        bridge->events.push_back({kind, std::move(first), std::move(second)});
        return;
    }
    if (bridge->events.size() >= kMaxPendingEvents) {
        if (!bridge->overflowReported) {
            // Make room by the oldest event that is not the connection's
            // end.
            const auto oldest = std::find_if(
                bridge->events.begin(), bridge->events.end(),
                [&isLifecycle](const CallbackEvent& e) { return !isLifecycle(e.kind); });
            if (oldest != bridge->events.end()) {
                bridge->events.erase(oldest);
            }
            bridge->events.push_back({CallbackEvent::Kind::Error,
                                      "media callback queue overflow", {}});
            bridge->overflowReported = true;
        }
        return;
    }
    bridge->events.push_back({kind, std::move(first), std::move(second)});
}

void queueDisplay(const std::weak_ptr<CallbackBridge>& weak, rtc::binary data)
{
    const auto bridge = weak.lock();
    if (!bridge) {
        return;
    }
    std::unique_lock lock(bridge->mutex);
    bridge->displayReceiveGate.wait(lock, [&bridge] {
        return !bridge->displayReceiveStalledForTest || bridge->cancelled;
    });
    if (bridge->cancelled || bridge->receiveSeveredForTest) {
        return;
    }
    if (data.size() > static_cast<std::size_t>(IMediaTransport::kMaxDisplayMessageBytes)) {
        if (bridge->events.size() < kMaxPendingEvents) {
            bridge->events.push_back({CallbackEvent::Kind::Error,
                                      "oversized display message rejected", {}});
        }
        return;
    }
    bridge->receivedDisplayPayloadBytes.fetch_add(
        static_cast<quint64>(data.size()), std::memory_order_relaxed);
    while (!bridge->displayMessages.empty()
           && (bridge->displayMessages.size() >= kMaxPendingDisplayMessages
               || bridge->displayBytes + data.size() > kMaxPendingDisplayBytes)) {
        bridge->displayBytes -= bridge->displayMessages.front().size();
        bridge->displayMessages.pop_front();
        bridge->displayMessagesDropped.fetch_add(1, std::memory_order_relaxed);
    }
    bridge->displayBytes += data.size();
    bridge->displayMessages.push_back(std::move(data));
}

void queueRtp(const std::weak_ptr<CallbackBridge>& weak, rtc::binary data, bool mic)
{
    const auto bridge = weak.lock();
    if (!bridge) {
        return;
    }
    const auto receivedAt = std::chrono::steady_clock::now();
    std::lock_guard lock(bridge->mutex);
    if (bridge->cancelled || bridge->receiveSeveredForTest) {
        return;
    }
    if (data.size() < static_cast<std::size_t>(IMediaTransport::kMinRawRtpBytes)
        || data.size() > static_cast<std::size_t>(IMediaTransport::kMaxRawRtpBytes)) {
        if (bridge->events.size() < kMaxPendingEvents) {
            bridge->events.push_back({CallbackEvent::Kind::Error,
                                      "invalid raw RTP packet size rejected", {}});
        }
        return;
    }
    bridge->receivedRtpBytes.fetch_add(
        static_cast<quint64>(data.size()), std::memory_order_relaxed);
    if (bridge->lastRtpReceipt != std::chrono::steady_clock::time_point {}) {
        bridge->maxRtpCallbackGap = std::max(
            bridge->maxRtpCallbackGap, receivedAt - bridge->lastRtpReceipt);
    }
    bridge->lastRtpReceipt = receivedAt;
    // TX mic thread: with a sink installed, the microphone line goes to
    // its own thread, never through the owner's drain.
    if (mic && bridge->micLane && bridge->micLane->hasSink.load(std::memory_order_acquire)) {
        MicLane& lane = *bridge->micLane;
        {
            std::lock_guard laneLock(lane.mutex);
            if (lane.packets.size()
                >= static_cast<std::size_t>(IMediaTransport::kReceivedRtpPacketsPerStream)) {
                lane.packets.pop_front();
                ++bridge->droppedRtpPackets;
            }
            lane.packets.push_back({std::move(data), receivedAt, true});
        }
        lane.wake.notify_one();
        return;
    }
    if (bridge->rtpPackets.size() >= bridge->rtpPacketCapacity) {
        bridge->rtpPackets.pop_front();
        ++bridge->droppedRtpPackets;
    }
    bridge->rtpPackets.push_back({std::move(data), receivedAt, mic});
}

void bindDataChannel(const std::shared_ptr<rtc::DataChannel>& channel,
                     const std::weak_ptr<CallbackBridge>& weak)
{
    channel->onError([weak](std::string error) {
        queueEvent(weak, CallbackEvent::Kind::DisplayError, std::move(error));
    });
    channel->onClosed([weak] {
        queueEvent(weak, CallbackEvent::Kind::PeerClosed,
                   "display data channel closed");
    });
    // Threshold 0: libdatachannel calls this when the one message it held
    // for SCTP has gone (src/impl/channel.cpp:52-60), which is when the
    // display channel takes a new message again.
    channel->setBufferedAmountLowThreshold(0);
    channel->onBufferedAmountLow([weak] {
        queueEvent(weak, CallbackEvent::Kind::DisplayWritable);
    });
    channel->onMessage(
        [weak](rtc::binary data) { queueDisplay(weak, std::move(data)); },
        [weak](std::string) {
            queueEvent(weak, CallbackEvent::Kind::Error,
                       "text display message rejected");
        });
}

// Task 37: the "tx" channel. Its messages go through the event queue in
// arrival order. Its closing or an error on it ends nothing else: a lost
// keepalive channel shows at the Core as keepalives stopping, which is the
// safe direction.
void queueTx(const std::weak_ptr<CallbackBridge>& weak, rtc::binary data)
{
    const auto bridge = weak.lock();
    if (!bridge) {
        return;
    }
    // TX mic thread: stamped here, so the watchdog judges it by its receipt.
    const auto receivedAt = std::chrono::steady_clock::now();
    std::lock_guard lock(bridge->mutex);
    if (bridge->cancelled || bridge->receiveSeveredForTest || data.empty()
        || data.size() > static_cast<std::size_t>(IMediaTransport::kMaxTxMessageBytes)) {
        return;
    }
    bridge->receivedTxPayloadBytes.fetch_add(
        static_cast<quint64>(data.size()), std::memory_order_relaxed);
    if (bridge->events.size() >= kMaxPendingEvents) {
        return;
    }
    bridge->events.push_back({CallbackEvent::Kind::TxMessage,
                              std::string(reinterpret_cast<const char*>(data.data()), data.size()),
                              {}, receivedAt});
}

void queueIq(const std::weak_ptr<CallbackBridge>& weak, rtc::binary data)
{
    const auto bridge = weak.lock();
    if (!bridge) { return; }
    std::lock_guard lock(bridge->mutex);
    if (bridge->cancelled || bridge->receiveSeveredForTest || bridge->iqFailed) { return; }
    if (data.empty() || data.size() > static_cast<std::size_t>(IMediaTransport::kMaxIqMessageBytes)) {
        bridge->iqMessages.clear();
        bridge->iqBytes = 0;
        bridge->iqFailed = true;
        return;
    }
    bridge->receivedIqPayloadBytes.fetch_add(
        static_cast<quint64>(data.size()), std::memory_order_relaxed);
    if (bridge->iqMessages.size() >= kMaxPendingIqMessages
        || bridge->iqBytes + data.size() > kMaxPendingIqBytes) {
        bridge->iqMessages.clear();
        bridge->iqBytes = 0;
        bridge->iqFailed = true;
        return;
    }
    bridge->iqBytes += data.size();
    bridge->iqMessages.push_back(std::move(data));
}

void bindIqChannel(const std::shared_ptr<rtc::DataChannel>& channel,
                   const std::weak_ptr<CallbackBridge>& weak)
{
    channel->onError([weak](std::string reason) {
        queueEvent(weak, CallbackEvent::Kind::IqError, std::move(reason));
    });
    channel->onClosed([weak] {
        queueEvent(weak, CallbackEvent::Kind::IqError, "raw I/Q channel closed");
    });
    channel->onMessage(
        [weak](rtc::binary data) { queueIq(weak, std::move(data)); },
        [weak](std::string) {
            queueEvent(weak, CallbackEvent::Kind::IqError, "text raw I/Q message rejected");
        });
}

void bindTxChannel(const std::shared_ptr<rtc::DataChannel>& channel,
                   const std::weak_ptr<CallbackBridge>& weak)
{
    channel->onMessage(
        [weak](rtc::binary data) { queueTx(weak, std::move(data)); },
        [](std::string) {});
}

bool isUnorderedWithoutRetransmits(const rtc::Reliability& reliability)
{
    return reliability.unordered && reliability.maxRetransmits
        && *reliability.maxRetransmits == 0;
}

void bindTrack(const std::shared_ptr<rtc::Track>& track,
               const std::weak_ptr<CallbackBridge>& weak, bool mic = false)
{
    track->onError([weak](std::string error) {
        queueEvent(weak, CallbackEvent::Kind::Error, std::move(error));
    });
    track->onClosed([weak] {
        queueEvent(weak, CallbackEvent::Kind::PeerClosed, "RTP track closed");
    });
    track->onMessage(
        [weak, mic](rtc::binary data) { queueRtp(weak, std::move(data), mic); },
        [weak](std::string) {
            queueEvent(weak, CallbackEvent::Kind::Error,
                       "text RTP message rejected");
        });
}

QByteArray toByteArray(const rtc::binary& data)
{
    if (data.size() > static_cast<std::size_t>(std::numeric_limits<qsizetype>::max())) {
        return {};
    }
    return QByteArray(reinterpret_cast<const char*>(data.data()),
                      static_cast<qsizetype>(data.size()));
}

qint64 heldMicroseconds(std::chrono::steady_clock::time_point receivedAt)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now() - receivedAt)
        .count();
}

} // namespace

void MicLineTimingWatch::noteReceipt(Clock::time_point receivedAt)
{
    if (m_lastReceipt != Clock::time_point {}) {
        const Clock::duration gap = receivedAt - m_lastReceipt;
        // Past the idle bound the line is starting again: not a gap.
        if (gap <= kIdleBound) {
            m_largestGap = std::max(m_largestGap, gap);
        }
    }
    m_lastReceipt = receivedAt;
}

std::optional<MicLineTimingWatch::Warning> MicLineTimingWatch::endBatch(
    Clock::time_point firstReceivedAt, Clock::time_point takenAt, int packets)
{
    const Clock::duration laneWait = takenAt - firstReceivedAt;
    if (m_largestGap <= kThreshold && laneWait <= kThreshold) {
        return std::nullopt;
    }
    if (m_lastWarning != Clock::time_point {} && takenAt - m_lastWarning < kInterval) {
        return std::nullopt;
    }
    m_lastWarning = takenAt;
    Warning warning;
    warning.callbackGapMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(m_largestGap).count();
    warning.laneWaitMs = std::chrono::duration_cast<std::chrono::milliseconds>(laneWait).count();
    warning.batchPackets = packets;
    return warning;
}

namespace {

// TX mic thread: the line's thread, until the transport stops.
void runMicLane(const std::shared_ptr<MicLane>& lane)
{
    std::deque<PendingRtpPacket> batch;
    // TX diagnostics lane: the owner's "media RTP timing" warning no longer
    // sees the line's packets, so the line warns on its own thread
    // (MicLineTimingWatch), after the batch is delivered.
    MicLineTimingWatch timing;
    for (;;) {
        {
            std::unique_lock lock(lane->mutex);
            lane->wake.wait(lock, [&lane] { return lane->stopping || !lane->packets.empty(); });
            if (lane->stopping) {
                return;
            }
            batch.swap(lane->packets);
        }
        std::optional<MicLineTimingWatch::Warning> warning;
        if (!batch.empty()) {
            const auto takenAt = std::chrono::steady_clock::now();
            timing.beginBatch();
            for (const PendingRtpPacket& packet : batch) {
                timing.noteReceipt(packet.receivedAt);
            }
            warning = timing.endBatch(batch.front().receivedAt, takenAt,
                                      static_cast<int>(batch.size()));
        }
        {
            const std::lock_guard sinkLock(lane->sinkMutex);
            for (const PendingRtpPacket& packet : batch) {
                if (!lane->sink) {
                    break;
                }
                lane->sink(toByteArray(packet.data), heldMicroseconds(packet.receivedAt));
            }
        }
        batch.clear();
        if (warning) {
            qWarning().nospace() << "media RTP timing (microphone): callbackGapMs="
                                 << warning->callbackGapMs
                                 << " laneWaitMs=" << warning->laneWaitMs
                                 << " batchPackets=" << warning->batchPackets;
        }
    }
}

// R-R3-23: whether a description's audio m-line maps the lossless payload
// type to L16/48000/2 (RFC 3551 L16, 48 kHz, stereo). Task 36: or the
// m-line of another mid (the microphone line).
bool describesLosslessAudio(const rtc::Description& description,
                            const char* mid = kAudioMid)
{
    for (int index = 0; index < description.mediaCount(); ++index) {
        const auto entry = description.media(index);
        const rtc::Description::Media* const* media =
            std::get_if<const rtc::Description::Media*>(&entry);
        if (media == nullptr || *media == nullptr || (*media)->mid() != mid
            || !(*media)->hasPayloadType(PcmAudioCodecConfig::kPayloadType)) {
            continue;
        }
        const rtc::Description::Media::RtpMap* map =
            (*media)->rtpMap(PcmAudioCodecConfig::kPayloadType);
        return QString::fromStdString(map->format).compare(
                   QLatin1String("L16"), Qt::CaseInsensitive) == 0
            && map->clockRate == PcmAudioCodecConfig::kSampleRate
            && map->encParams == std::to_string(PcmAudioCodecConfig::kChannels);
    }
    return false;
}

quint32 rtpSsrc(const QByteArray& packet)
{
    return (static_cast<quint32>(static_cast<quint8>(packet.at(8))) << 24)
        | (static_cast<quint32>(static_cast<quint8>(packet.at(9))) << 16)
        | (static_cast<quint32>(static_cast<quint8>(packet.at(10))) << 8)
        | static_cast<quint32>(static_cast<quint8>(packet.at(11)));
}

// R-R3-43: the main stream or one of the declared receiver streams.
// R-R3-45: or the declared headphones mix (0 when none is declared).
bool isDeclaredAudioSsrc(quint32 ssrc, quint32 mainSsrc, const QList<quint32>& receiverSsrcs,
                         quint32 headphonesSsrc)
{
    return ssrc == mainSsrc || receiverSsrcs.contains(ssrc)
        || (headphonesSsrc != 0 && ssrc == headphonesSsrc);
}

// R-R3-43: at most kMaxReceiverAudioStreams, none zero, none the main SSRC,
// no repeats.
bool validReceiverAudioSsrcs(quint32 mainSsrc, const QList<quint32>& receiverSsrcs)
{
    if (receiverSsrcs.size() > IMediaTransport::kMaxReceiverAudioStreams) {
        return false;
    }
    for (qsizetype index = 0; index < receiverSsrcs.size(); ++index) {
        const quint32 ssrc = receiverSsrcs.at(index);
        if (ssrc == 0 || ssrc == mainSsrc || receiverSsrcs.indexOf(ssrc) != index) {
            return false;
        }
    }
    return true;
}

// R-R3-45: 0 (none), or an id that is neither the main stream's nor a
// receiver stream's.
bool validHeadphonesAudioSsrc(quint32 mainSsrc, const QList<quint32>& receiverSsrcs,
                              quint32 headphonesSsrc)
{
    return headphonesSsrc == 0
        || (headphonesSsrc != mainSsrc && !receiverSsrcs.contains(headphonesSsrc));
}

// Task 36: 0 (none), or an id that is none of the Core's own streams'.
bool validMicAudioSsrc(quint32 mainSsrc, const QList<quint32>& receiverSsrcs,
                       quint32 headphonesSsrc, quint32 micSsrc)
{
    return micSsrc == 0
        || (micSsrc != mainSsrc && !receiverSsrcs.contains(micSsrc)
            && micSsrc != headphonesSsrc);
}

} // namespace

bool applyMediaSctpSettingsOnce()
{
    bool applied = false;
    std::call_once(g_sctpSettingsOnce, [&applied] {
        rtc::SctpSettings settings;
        settings.sendBufferSize =
            static_cast<std::size_t>(IMediaTransport::kSctpSendBufferBytes);
        settings.recvBufferSize =
            static_cast<std::size_t>(IMediaTransport::kSctpReceiveBufferBytes);
        // Every other field stays unset, which keeps libdatachannel's own
        // defaults. Before global initialisation the library stores these
        // for its first peer; afterwards it applies them to new sockets
        // (libdatachannel v0.24.5 src/impl/init.cpp:105-111, 144-145).
        rtc::SetSctpSettings(std::move(settings));
        g_peersCreatedBeforeSctpSettings.store(
            g_peersCreated.load(std::memory_order_relaxed), std::memory_order_relaxed);
        g_sctpSettingsApplications.fetch_add(1, std::memory_order_relaxed);
        applied = true;
    });
    return applied;
}

MediaSctpSettingsRecord mediaSctpSettingsRecord()
{
    return {g_sctpSettingsApplications.load(std::memory_order_relaxed),
            g_peersCreatedBeforeSctpSettings.load(std::memory_order_relaxed),
            g_peersCreated.load(std::memory_order_relaxed)};
}

QString opusOfferFormatParameters(int targetBitrate)
{
    // RFC 7587 section 6.1: stereo, useinbandfec, maxaveragebitrate and
    // minptime describe what the AUTHOR of the description (here the Core)
    // prefers to RECEIVE; only the sprop-* parameters describe what the
    // author sends (sprop-stereo=1: the Core sends stereo). The Core's
    // offer is send-only and it receives no audio, so the receive
    // preferences are set to mirror what it sends, never aspirational:
    // stereo, useinbandfec omitted (default 0, as OpusAudioEncoder sets
    // OPUS_SET_INBAND_FEC(0)), and maxaveragebitrate equal to the
    // configured encoder target (R-R3-23). The encoder itself is reported to
    // the GUI by the minor-8 audio context, not by this line. Receiver
    // streams (R-R3-43) ride this m-line but run their own 48 kbit/s
    // (DaemonMediaController::kReceiverAudioOpusBitrate), each reported by
    // its receiver context; the line stays as it was so a window sees the
    // same offer as before, and no receiver of the Core's audio reads it
    // (the Core receives no audio). Tying the
    // receive preferences to the send target must be revisited when the
    // m-line becomes sendrecv (TX audio, R4): then they describe what the
    // Core really wants to receive.
    return QStringLiteral("minptime=10;maxaveragebitrate=%1;stereo=1;sprop-stereo=1")
        .arg(targetBitrate);
}

QString micLineOpusFormatParameters()
{
    // RFC 7587 section 6.1: these describe what the author of the offer
    // (the Core) prefers to receive on this line: mono (stereo=0), in-band
    // FEC, 10 ms minimum packet time, and maxaveragebitrate, the maximum
    // average bitrate the Core will receive, which the sender keeps under.
    // The ceiling is 48 kbit/s, the highest measured Opus profile and
    // JJ's ruling for the phone's microphone; a desktop remote window's
    // 24 kbit/s (RemoteMicConfig::kOpusBitrate) is within it. The
    // device's own choice is not known when the offer is written (it
    // comes in the `audio` control after the peer starts), so the line
    // carries the ceiling rather than the choice.
    return QStringLiteral("minptime=10;useinbandfec=1;stereo=0;maxaveragebitrate=%1")
        .arg(kMicLineMaxAverageBitrate);
}

namespace {

// A remote candidate, admitted or refused, to the opt-in ICE check log.
void logMediaRemoteCandidate(const QString& candidate, bool fromOwnedSource, bool admitted)
{
    if (!IceDiagnostics::enabled()) {
        return;
    }
    IceDiagnostics::logPath("media", QStringLiteral("remote candidate%1 %2: %3")
                                         .arg(fromOwnedSource ? QStringLiteral(" (tunnel)")
                                                              : QString(),
                                              admitted ? QStringLiteral("admitted")
                                                       : QStringLiteral("refused"),
                                              candidate));
}

} // namespace

struct LibDataChannelMediaTransport::Private {
    QTimer* drainTimer = nullptr;
    // TX mic thread: the microphone line's queue and thread, from start()
    // with a line to stop().
    std::shared_ptr<MicLane> micLane;
    std::unique_ptr<QThread> micThread;
    std::shared_ptr<CallbackBridge> bridge;
    std::shared_ptr<rtc::PeerConnection> peer;
    std::shared_ptr<rtc::DataChannel> display;
    std::shared_ptr<rtc::DataChannel> iq;
    // Task 37: the "tx" channel, null without one.
    std::shared_ptr<rtc::DataChannel> tx;
    std::shared_ptr<rtc::Track> audio;
    // Task 36: the microphone line (the offerer's receive-only track or the
    // answerer's send-only one), null without one.
    std::shared_ptr<rtc::Track> micAudio;
    quint32 micAudioSsrc = 0;
    Role role = Role::Answerer;
    quint32 localAudioSsrc = 0;
    // R-R3-43: the declared receiver audio streams' SSRCs, empty for today.
    QList<quint32> receiverAudioSsrcs;
    // R-R3-45: the declared headphones mix's SSRC, 0 for today.
    quint32 headphonesAudioSsrc = 0;
    CandidatePolicy candidatePolicy = CandidatePolicy::HostOnly;
    // Task 27: the ICE settings of a connection through the remote access
    // service, and where its gathering stands.
    std::optional<IceConfiguration> ice;
    QString connectionId;
    bool gatherRequested = false;
    bool gatheringStarted = false;
    QList<IceRelayServer> relays;
    bool started = false;
    bool ready = false;
    bool remoteDescriptionAccepted = false;
    bool remoteDescribesLossless = false;
    bool remoteDescribesMicLossless = false;
    int acceptedCandidates = 0;
    /// The far end's relay candidates, address and port (selectedPath()).
    QList<QPair<QString, quint16>> farEndRelays;
    QList<QPair<QString, quint16>> ownedShimEndpoints;
    // Task 29 step 2b: this connection's own candidate source on the media
    // lane (the web relay's leg, or the direct link's tunnel).
    std::shared_ptr<CandidateSourceLease> candidateSourceLease;
    // Its candidates before the remote description (an offerer gathers
    // first): the agent takes remote candidates only after it.
    QStringList pendingSourceCandidates;
    std::chrono::steady_clock::time_point lastRtpTimingWarning;
};

LibDataChannelMediaTransport::LibDataChannelMediaTransport(
    QObject* parent, CandidatePolicy candidatePolicy)
    : IMediaTransport(parent)
    , d(std::make_unique<Private>())
{
    d->candidatePolicy = candidatePolicy;
    d->drainTimer = new QTimer(this);
    d->drainTimer->setInterval(2);
    d->drainTimer->setTimerType(Qt::PreciseTimer);
    connect(d->drainTimer, &QTimer::timeout, this,
            &LibDataChannelMediaTransport::drainCallbacks);
}

LibDataChannelMediaTransport::~LibDataChannelMediaTransport()
{
    stopInternal(false);
}

bool LibDataChannelMediaTransport::start(const StartOptions& options)
{
    if (d->started || options.localAudioSsrc == 0
        || !validReceiverAudioSsrcs(options.localAudioSsrc, options.receiverAudioSsrcs)
        || !validHeadphonesAudioSsrc(options.localAudioSsrc, options.receiverAudioSsrcs,
                                     options.headphonesAudioSsrc)
        || !validMicAudioSsrc(options.localAudioSsrc, options.receiverAudioSsrcs,
                              options.headphonesAudioSsrc, options.micAudioSsrc)) {
        return false;
    }

    d->role = options.role;
    d->localAudioSsrc = options.localAudioSsrc;
    d->receiverAudioSsrcs = options.receiverAudioSsrcs;
    d->headphonesAudioSsrc = options.headphonesAudioSsrc;
    d->micAudioSsrc = options.micAudioSsrc;
    d->bridge = std::make_shared<CallbackBridge>();
    d->bridge->micSsrc = options.micAudioSsrc;
    if (options.micAudioSsrc != 0) {
        // TX mic thread: the line's thread runs from here to stop(); it
        // carries packets once a sink is installed.
        d->micLane = std::make_shared<MicLane>();
        d->bridge->micLane = d->micLane;
        const std::shared_ptr<MicLane> lane = d->micLane;
        d->micThread.reset(QThread::create([lane]() { runMicLane(lane); }));
        d->micThread->setObjectName(QStringLiteral("NereusMicRx"));
        d->micThread->start();
    }
    d->bridge->txChannelWanted = options.txChannel;
    d->bridge->iqChannelWanted = options.iqChannel;
    // Task 36: the microphone line's packets share the queue with one more
    // stream's worth of room.
    d->bridge->rtpPacketCapacity =
        static_cast<std::size_t>(kReceivedRtpPacketsPerStream)
        * static_cast<std::size_t>(1 + options.receiverAudioSsrcs.size()
                                   + (options.headphonesAudioSsrc != 0 ? 1 : 0)
                                   + (options.micAudioSsrc != 0 ? 1 : 0));
    const std::weak_ptr<CallbackBridge> weak = d->bridge;

    try {
        rtc::Configuration config;
        config.mtu = static_cast<std::size_t>(kConfiguredMtuBytes);
        // Task 27: through the remote access service, the MTU leaves room
        // for TURN's ChannelData header (IceConfiguration::kMtuBytes).
        if (options.ice) {
            config.mtu = static_cast<std::size_t>(IceConfiguration::kMtuBytes);
        }
        // libdatachannel applies this before delivering a complete SCTP
        // message, so the allocation is bounded before our callback runs.
        config.maxMessageSize = static_cast<std::size_t>(kMaxDisplayMessageBytes);
        config.forceMediaTransport = true;
        config.disableAutoNegotiation = true;
        config.enableIceTcp = false;
        config.iceServers.clear();
        d->ice = options.ice;
        d->connectionId = options.connectionId;
        if (options.ice && options.ice->hasCandidateSourceFactory()) {
            d->candidateSourceLease = CandidateSourceLease::create(*options.ice);
            config.iceTransportLifetime = d->candidateSourceLease;
        }
        d->gatherRequested = false;
        d->gatheringStarted = false;
        d->relays.clear();
        if (options.ice) {
            // One STUN server (libdatachannel picks one of its STUN servers
            // at random, so there is only ever one to pick), and no
            // gathering until the relay credentials are known: they are
            // fixed when gathering starts (IceConfiguration.h).
            if (const auto stun = options.ice->stunServer()) {
                config.iceServers.emplace_back(stun->host.toStdString(), stun->port);
            }
            config.disableAutoGathering = true;
            if (options.ice->relayKnown()) {
                d->gatherRequested = true;
                d->relays = options.ice->relayServers();
            }
        }

        applyMediaSctpSettingsOnce();
        d->peer = std::make_shared<rtc::PeerConnection>(std::move(config));
        g_peersCreated.fetch_add(1, std::memory_order_relaxed);
        d->peer->onLocalDescription([weak](rtc::Description description) {
            const std::string sdp = description.generateSdp();
            const std::string type = description.typeString();
            if (sdp.size() > static_cast<std::size_t>(IMediaTransport::kMaxDescriptionBytes)) {
                queueEvent(weak, CallbackEvent::Kind::Error,
                           "oversized local description rejected");
                return;
            }
            queueEvent(weak, CallbackEvent::Kind::Description, sdp, type);
        });
        d->peer->onLocalCandidate([weak](rtc::Candidate candidate) {
            const std::string value = candidate.candidate();
            const std::string mid = candidate.mid();
            if (value.size() > static_cast<std::size_t>(IMediaTransport::kMaxCandidateBytes)
                || mid.size() > static_cast<std::size_t>(IMediaTransport::kMaxCandidateMidBytes)) {
                queueEvent(weak, CallbackEvent::Kind::Error,
                           "oversized local candidate rejected");
                return;
            }
            // Logged where it is sent, or not (drainCallbacks).
            queueEvent(weak, CallbackEvent::Kind::Candidate, value, mid);
        });
        d->peer->onGatheringStateChange([weak](rtc::PeerConnection::GatheringState state) {
            if (state == rtc::PeerConnection::GatheringState::Complete) {
                queueEvent(weak, CallbackEvent::Kind::GatheringComplete, std::string());
            }
        });
        if (IceDiagnostics::enabled()) {
            d->peer->onIceStateChange([](rtc::PeerConnection::IceState state) {
                IceDiagnostics::logPath("media", QStringLiteral("ICE state %1")
                                                     .arg(IceDiagnostics::stateName(state)));
            });
        }
        d->peer->onStateChange([weak](rtc::PeerConnection::State state) {
            if (IceDiagnostics::enabled()) {
                IceDiagnostics::logPath("media", QStringLiteral("peer state %1")
                                                     .arg(IceDiagnostics::stateName(state)));
            }
            if (state == rtc::PeerConnection::State::Failed) {
                queueEvent(weak, CallbackEvent::Kind::PeerFailed,
                           "media peer connection failed");
            } else if (state == rtc::PeerConnection::State::Closed) {
                queueEvent(weak, CallbackEvent::Kind::PeerClosed,
                           "media peer connection closed");
            }
        });
        d->peer->onDataChannel([weak](std::shared_ptr<rtc::DataChannel> channel) {
            const rtc::Reliability reliability = channel->reliability();
            if (channel->label() == kIqLabel) {
                const auto bridge = weak.lock();
                bool take = false;
                if (bridge && !reliability.unordered && !reliability.maxRetransmits
                    && !reliability.maxPacketLifeTime) {
                    std::lock_guard lock(bridge->mutex);
                    take = !bridge->cancelled && bridge->iqChannelWanted
                        && !bridge->iqChannelAssigned;
                    if (take) {
                        bridge->iqChannelAssigned = true;
                        bridge->iqChannel = channel;
                    }
                }
                if (!take) {
                    channel->close();
                    queueEvent(weak, CallbackEvent::Kind::IqError,
                               "unexpected raw I/Q data channel rejected");
                    return;
                }
                bindIqChannel(channel, weak);
                return;
            }
            // Task 37: the "tx" channel, taken only by a side started with
            // it, unordered and never retransmitted.
            if (channel->label() == kTxLabel) {
                const auto bridge = weak.lock();
                bool take = false;
                if (bridge && isUnorderedWithoutRetransmits(reliability)) {
                    std::lock_guard lock(bridge->mutex);
                    take = !bridge->cancelled && bridge->txChannelWanted
                        && !bridge->txChannelAssigned;
                    if (take) {
                        bridge->txChannelAssigned = true;
                        bridge->txChannel = channel;
                    }
                }
                if (!take) {
                    channel->close();
                    queueEvent(weak, CallbackEvent::Kind::Error,
                               "unexpected tx data channel rejected");
                    return;
                }
                bindTxChannel(channel, weak);
                return;
            }
            if (channel->label() != kDisplayLabel || !reliability.unordered
                || !reliability.maxRetransmits || *reliability.maxRetransmits != 0) {
                channel->close();
                queueEvent(weak, CallbackEvent::Kind::Error,
                           "unexpected display data channel rejected");
                return;
            }
            bindDataChannel(channel, weak);
            const auto bridge = weak.lock();
            if (!bridge) {
                channel->resetCallbacks();
                channel->close();
                return;
            }
            bool reject = false;
            {
                std::lock_guard lock(bridge->mutex);
                reject = bridge->cancelled || bridge->dataChannelAssigned;
                if (!reject) {
                    bridge->dataChannelAssigned = true;
                    bridge->dataChannel = channel;
                }
            }
            if (reject) {
                channel->resetCallbacks();
                channel->close();
            }
        });
        d->peer->onTrack([weak](std::shared_ptr<rtc::Track> track) {
            rtc::Description::Media description = track->description();
            // Task 36: the microphone line, taken only by an answerer that
            // was started with one. libdatachannel calls this from inside
            // setRemoteDescription(), before the answer is written, so the
            // SSRC declared here goes out in the answer and the Core's
            // library routes the line's packets by it.
            const auto owner = weak.lock();
            const quint32 micSsrc = owner ? owner->micSsrc : 0;
            const bool mic = track->mid() == kMicMid && micSsrc != 0;
            if ((track->mid() != kAudioMid && !mic)
                || !description.hasPayloadType(kOpusPayloadType)) {
                track->close();
                queueEvent(weak, CallbackEvent::Kind::Error,
                           "unexpected media track rejected");
                return;
            }
            if (mic) {
                try {
                    description.addSSRC(micSsrc, kMicStreamName);
                    track->setDescription(std::move(description));
                } catch (const std::exception&) {
                    track->close();
                    queueEvent(weak, CallbackEvent::Kind::Error,
                               "microphone line rejected");
                    return;
                }
            }
            bindTrack(track, weak, mic);
            const auto bridge = weak.lock();
            if (!bridge) {
                track->resetCallbacks();
                track->close();
                return;
            }
            bool reject = false;
            {
                std::lock_guard lock(bridge->mutex);
                if (mic) {
                    reject = bridge->cancelled || bridge->micTrackAssigned;
                    if (!reject) {
                        bridge->micTrackAssigned = true;
                        bridge->micTrack = track;
                    }
                } else {
                    reject = bridge->cancelled || bridge->trackAssigned;
                    if (!reject) {
                        bridge->trackAssigned = true;
                        bridge->track = track;
                    }
                }
            }
            if (reject) {
                track->resetCallbacks();
                track->close();
            }
        });

        if (options.role == Role::Offerer) {
            rtc::DataChannelInit init;
            init.reliability.unordered = true;
            init.reliability.maxRetransmits = 0;
            d->display = d->peer->createDataChannel(kDisplayLabel, init);
            bindDataChannel(d->display, weak);
            if (options.iqChannel) {
                rtc::DataChannelInit iqInit;
                d->iq = d->peer->createDataChannel(kIqLabel, iqInit);
                bindIqChannel(d->iq, weak);
            }
            // Task 37: the keepalive's own channel, only when asked.
            if (options.txChannel) {
                d->tx = d->peer->createDataChannel(kTxLabel, init);
                bindTxChannel(d->tx, weak);
            }

            rtc::Description::Audio opus(kAudioMid,
                                         rtc::Description::Direction::SendOnly);
            opus.addOpusCodec(
                kOpusPayloadType,
                opusOfferFormatParameters(options.audioTargetBitrate)
                    .toStdString());
            if (options.offerLosslessAudio) {
                // R-R3-23: the lossless profile rides the same m-line and
                // SSRC as Opus; the payload type tells the two apart. Opus
                // stays first, the preferred format.
                opus.addAudioCodec(PcmAudioCodecConfig::kPayloadType,
                                   l16RtpMapEncoding());
            }
            opus.addSSRC(options.localAudioSsrc, kMainAudioStreamName);
            // R-R3-43: receiver streams ride the same m-line, each declared
            // by its own a=ssrc line after the main one, in list order. With
            // none asked for, the offer is today's.
            for (qsizetype index = 0; index < options.receiverAudioSsrcs.size(); ++index) {
                opus.addSSRC(options.receiverAudioSsrcs.at(index),
                             kReceiverAudioStreamPrefix + std::to_string(index));
            }
            // R-R3-45: the headphones mix, last, only when declared.
            if (options.headphonesAudioSsrc != 0) {
                opus.addSSRC(options.headphonesAudioSsrc, kHeadphonesAudioStreamName);
            }
            d->audio = d->peer->addTrack(opus);
            bindTrack(d->audio, weak);

            // Task 36: the microphone line, after the main one and only
            // when asked. The Core receives on it and declares no SSRC of
            // its own; the answer declares the microphone's.
            if (options.micAudioSsrc != 0) {
                rtc::Description::Audio mic(kMicMid,
                                            rtc::Description::Direction::RecvOnly);
                mic.addOpusCodec(kOpusPayloadType,
                                 micLineOpusFormatParameters().toStdString());
                if (options.offerLosslessAudio) {
                    mic.addAudioCodec(PcmAudioCodecConfig::kPayloadType,
                                      l16RtpMapEncoding());
                }
                d->micAudio = d->peer->addTrack(mic);
                bindTrack(d->micAudio, weak, /*mic=*/true);
            }
        }

        d->started = true;
        d->ready = false;
        d->remoteDescriptionAccepted = false;
        d->remoteDescribesLossless = false;
        d->remoteDescribesMicLossless = false;
        d->acceptedCandidates = 0;
        d->farEndRelays.clear();
        d->ownedShimEndpoints.clear();
        d->drainTimer->start();

        if (options.role == Role::Offerer) {
            d->peer->setLocalDescription(rtc::Description::Type::Offer);
            gatherIfReady();
        }
        return true;
    } catch (const std::exception& error) {
        const QString message = QString::fromUtf8(error.what());
        stopInternal(false);
        emit errorOccurred(message);
        return false;
    }
}

bool LibDataChannelMediaTransport::gatherCandidates(const QList<IceRelayServer>& relays)
{
    if (!d->started || !d->ice || d->gatherRequested) {
        return false;
    }
    d->gatherRequested = true;
    d->relays = d->ice->relayAllowed() ? relays : QList<IceRelayServer>();
    gatherIfReady();
    return true;
}

void LibDataChannelMediaTransport::gatherIfReady()
{
    if (!d->started || !d->peer || !d->ice || !d->gatherRequested || d->gatheringStarted
        || !d->peer->localDescription()) {
        return;
    }
    d->gatheringStarted = true;
    std::vector<rtc::IceServer> relays;
    for (const IceRelayServer& relay : std::as_const(d->relays)) {
        if (relays.size() >= static_cast<std::size_t>(IceConfiguration::kMaxRelayServers)) {
            break;
        }
        relays.emplace_back(relay.host.toStdString(), relay.port, relay.username.toStdString(),
                            relay.password.toStdString(), rtc::IceServer::RelayType::TurnUdp);
    }
    try {
        d->peer->gatherLocalCandidates(std::move(relays));
    } catch (const std::exception& error) {
        emit errorOccurred(QString::fromUtf8(error.what()));
    }
    // Task 29 step 2b (the step 2a review's Minor 10): the media agent gets
    // its own source on the media lane too.
    if (d->candidateSourceLease) {
        const QPointer<LibDataChannelMediaTransport> self(this);
        // An old ICE agent can outlive stop()/start() of this wrapper.
        const std::weak_ptr<CandidateSourceLease> expected = d->candidateSourceLease;
        d->candidateSourceLease->start(IceConfiguration::kMediaLane, d->connectionId,
                                       [self, expected](const QString& candidate) {
            const auto lease = expected.lock();
            if (!self || !lease || !self->d->started
                || self->d->candidateSourceLease != lease) {
                return;
            }
            if (!self->d->remoteDescriptionAccepted) {
                self->d->pendingSourceCandidates.append(candidate);
                return;
            }
            self->admitCandidate(candidate, QString(), true);
        });
    }
}

std::optional<qint64> LibDataChannelMediaTransport::rttMs() const
{
    // Control logging lane: the SCTP round-trip estimate, for the log only.
    if (thread() != QThread::currentThread() || !d->started || !d->peer) {
        return std::nullopt;
    }
    try {
        if (const auto rtt = d->peer->rtt()) {
            return static_cast<qint64>(rtt->count());
        }
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

std::optional<MediaIcePath> LibDataChannelMediaTransport::selectedPath() const
{
    if (thread() != QThread::currentThread() || !d->started || !d->peer) {
        return std::nullopt;
    }
    try {
        rtc::Candidate local;
        rtc::Candidate remote;
        if (!d->peer->getSelectedCandidatePair(&local, &remote)) {
            return std::nullopt;
        }
        const auto typeName = [](const rtc::Candidate& candidate) {
            switch (candidate.type()) {
            case rtc::Candidate::Type::Host:
                return QStringLiteral("host");
            case rtc::Candidate::Type::ServerReflexive:
                return QStringLiteral("srflx");
            case rtc::Candidate::Type::PeerReflexive:
                return QStringLiteral("prflx");
            case rtc::Candidate::Type::Relayed:
                return QStringLiteral("relay");
            default:
                return QString();
            }
        };
        const auto transportName = [](const rtc::Candidate& candidate) {
            switch (candidate.transportType()) {
            case rtc::Candidate::TransportType::Udp:
                return QStringLiteral("udp");
            case rtc::Candidate::TransportType::TcpActive:
                return QStringLiteral("tcp-active");
            case rtc::Candidate::TransportType::TcpPassive:
                return QStringLiteral("tcp-passive");
            case rtc::Candidate::TransportType::TcpSo:
                return QStringLiteral("tcp-so");
            case rtc::Candidate::TransportType::TcpUnknown:
                return QStringLiteral("tcp");
            default:
                return QString();
            }
        };
        MediaIcePath path;
        path.localType = typeName(local);
        path.remoteType = typeName(remote);
        // Control logging lane: the candidates' transports, for the log.
        path.localTransport = transportName(local);
        path.remoteTransport = transportName(remote);
        path.localAddress = QString::fromStdString(local.address().value_or(std::string()));
        path.localPort = local.port().value_or(0);
        path.remoteAddress = QString::fromStdString(remote.address().value_or(std::string()));
        path.remotePort = remote.port().value_or(0);
        path.farEndRelays = d->farEndRelays;
        const auto endpoint = MediaIcePath::loopbackEndpoint(path.remoteAddress, path.remotePort);
        path.ownedLoopbackShim = endpoint && d->ownedShimEndpoints.contains(*endpoint);
        if (path.ownedLoopbackShim && d->candidateSourceLease) {
            path.ownedSourcePath = d->candidateSourceLease->networkPathSnapshot();
        }
        return path;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

void LibDataChannelMediaTransport::stop()
{
    stopInternal(true);
}

bool LibDataChannelMediaTransport::setMicPacketSink(MicPacketSink sink)
{
    if (!d->micLane) {
        return false;
    }
    const std::lock_guard sinkLock(d->micLane->sinkMutex);
    d->micLane->hasSink.store(static_cast<bool>(sink), std::memory_order_release);
    d->micLane->sink = std::move(sink);
    return true;
}

void LibDataChannelMediaTransport::stopInternal(bool notify)
{
    if (!d->started && !d->peer && !d->bridge) {
        return;
    }

    const bool wasStarted = d->started;
    d->started = false;
    d->ready = false;
    d->localAudioSsrc = 0;
    d->receiverAudioSsrcs.clear();
    d->headphonesAudioSsrc = 0;
    d->micAudioSsrc = 0;
    d->remoteDescriptionAccepted = false;
    d->remoteDescribesLossless = false;
    d->remoteDescribesMicLossless = false;
    d->acceptedCandidates = 0;
    d->farEndRelays.clear();
    d->ownedShimEndpoints.clear();
    // The ICE transport retains the source/socket lease through its real
    // asynchronous teardown. Releasing this wrapper's copy must not release
    // the loopback port while the old ICE agent can still send.
    d->candidateSourceLease.reset();
    d->pendingSourceCandidates.clear();
    d->ice.reset();
    d->gatherRequested = false;
    d->gatheringStarted = false;
    d->relays.clear();
    d->drainTimer->stop();

    std::shared_ptr<rtc::DataChannel> pendingDisplay;
    std::shared_ptr<rtc::DataChannel> pendingIq;
    std::shared_ptr<rtc::DataChannel> pendingTx;
    std::shared_ptr<rtc::Track> pendingAudio;
    std::shared_ptr<rtc::Track> pendingMic;
    if (d->bridge) {
        std::lock_guard lock(d->bridge->mutex);
        d->bridge->cancelled = true;
        d->bridge->displayReceiveGate.notify_all();
        d->bridge->events.clear();
        d->bridge->displayMessages.clear();
        d->bridge->displayBytes = 0;
        d->bridge->iqMessages.clear();
        d->bridge->iqBytes = 0;
        d->bridge->rtpPackets.clear();
        d->bridge->micLane.reset();
        pendingDisplay = std::move(d->bridge->dataChannel);
        pendingIq = std::move(d->bridge->iqChannel);
        pendingTx = std::move(d->bridge->txChannel);
        pendingAudio = std::move(d->bridge->track);
        pendingMic = std::move(d->bridge->micTrack);
    }
    // TX mic thread: the line's thread ends here, and its sink is never
    // called again.
    if (d->micLane) {
        {
            std::lock_guard laneLock(d->micLane->mutex);
            d->micLane->stopping = true;
            d->micLane->packets.clear();
        }
        d->micLane->wake.notify_all();
        {
            const std::lock_guard sinkLock(d->micLane->sinkMutex);
            d->micLane->hasSink.store(false, std::memory_order_release);
            d->micLane->sink = nullptr;
        }
        if (d->micThread) {
            d->micThread->wait();
            d->micThread.reset();
        }
        d->micLane.reset();
    }

    if (pendingTx && pendingTx != d->tx) {
        pendingTx->resetCallbacks();
        pendingTx->close();
    }
    if (pendingDisplay && pendingDisplay != d->display) {
        pendingDisplay->resetCallbacks();
        pendingDisplay->close();
    }
    if (pendingIq && pendingIq != d->iq) {
        pendingIq->resetCallbacks();
        pendingIq->close();
    }
    if (pendingAudio && pendingAudio != d->audio) {
        pendingAudio->resetCallbacks();
        pendingAudio->close();
    }
    if (pendingMic && pendingMic != d->micAudio) {
        pendingMic->resetCallbacks();
        pendingMic->close();
    }

    if (d->display) {
        d->display->resetCallbacks();
        d->display->close();
    }
    if (d->iq) {
        d->iq->resetCallbacks();
        d->iq->close();
    }
    if (d->tx) {
        d->tx->resetCallbacks();
        d->tx->close();
    }
    if (d->audio) {
        d->audio->resetCallbacks();
        d->audio->close();
    }
    if (d->micAudio) {
        d->micAudio->resetCallbacks();
        d->micAudio->close();
    }
    if (d->peer) {
        d->peer->resetCallbacks();
        d->peer->close();
    }

    d->display.reset();
    d->iq.reset();
    d->tx.reset();
    d->audio.reset();
    d->micAudio.reset();
    d->peer.reset();
    d->bridge.reset();

    if (notify && wasStarted) {
        emit closed();
    }
}

bool LibDataChannelMediaTransport::acceptDescription(const QString& sdp,
                                                      const QString& type)
{
    if (!d->started || d->remoteDescriptionAccepted || !d->peer) {
        return false;
    }
    const QByteArray sdpBytes = sdp.toUtf8();
    const QByteArray typeBytes = type.toUtf8().toLower();
    if (sdpBytes.isEmpty() || sdpBytes.size() > kMaxDescriptionBytes
        || sdpBytes.contains('\0')) {
        return false;
    }
    const QByteArray expected = d->role == Role::Offerer
        ? QByteArrayLiteral("answer") : QByteArrayLiteral("offer");
    if (typeBytes != expected) {
        return false;
    }

    try {
        rtc::Description description(sdpBytes.toStdString(),
                                     typeBytes.toStdString());
        // Candidate admission is intentionally confined to acceptCandidate(),
        // where R3 applies the host-only policy and the shared count bound.
        // libdatachannel otherwise imports candidates embedded in SDP directly.
        if (!description.candidates().empty()) {
            return false;
        }
        const bool describesLossless = describesLosslessAudio(description);
        const bool describesMicLossless = describesLosslessAudio(description, kMicMid);
        d->peer->setRemoteDescription(std::move(description));
        d->remoteDescriptionAccepted = true;
        d->remoteDescribesLossless = describesLossless;
        d->remoteDescribesMicLossless = describesMicLossless;
        if (d->role == Role::Answerer) {
            d->peer->setLocalDescription(rtc::Description::Type::Answer);
            gatherIfReady();
        }
        for (const QString& candidate : std::exchange(d->pendingSourceCandidates, {})) {
            admitCandidate(candidate, QString(), true);
        }
        return true;
    } catch (const std::exception& error) {
        emit errorOccurred(QString::fromUtf8(error.what()));
        return false;
    }
}

bool LibDataChannelMediaTransport::acceptCandidate(const QString& candidate,
                                                    const QString& mid)
{
    return admitCandidate(candidate, mid, false);
}

bool LibDataChannelMediaTransport::admitCandidate(const QString& candidate,
                                                  const QString& mid, bool fromOwnedSource)
{
    if (!d->started || !d->peer || d->acceptedCandidates >= kMaxRemoteCandidates) {
        return false;
    }
    const QByteArray candidateBytes = candidate.toUtf8();
    const QByteArray midBytes = mid.toUtf8();
    // Task 27: a candidate through the remote access service names no mid
    // (the rendezvous document, section 5.2); the library takes the bundle's.
    if (candidateBytes.isEmpty() || candidateBytes.size() > kMaxCandidateBytes
        || (midBytes.isEmpty() && !d->ice) || midBytes.size() > kMaxCandidateMidBytes
        || candidateBytes.contains('\0') || midBytes.contains('\0')) {
        return false;
    }

    try {
        rtc::Candidate parsed(candidateBytes.toStdString(), midBytes.toStdString());
        if (d->ice && d->ice->onlySourceCandidates() && !fromOwnedSource) {
            // The fallback onto the tunnel: only the tunnel's own candidate.
            logMediaRemoteCandidate(candidate, fromOwnedSource, false);
            return false;
        }
        if (d->ice) {
            // Task 27: through the remote access service every candidate
            // type is used, the far end's relay ones only when this side
            // allows the relay (`relay = deny`: direct or nothing).
            if (!d->ice->acceptsRemoteCandidate(QString::fromUtf8(candidateBytes))) {
                logMediaRemoteCandidate(candidate, fromOwnedSource, false);
                return false;
            }
        } else if (d->candidatePolicy == CandidatePolicy::HostOnly
                   && parsed.type() != rtc::Candidate::Type::Host) {
            // R3 selects HostOnly. AnyIceType keeps the same transport
            // interface usable by a later approved STUN or relay
            // configuration.
            logMediaRemoteCandidate(candidate, fromOwnedSource, false);
            return false;
        }
        if (parsed.type() == rtc::Candidate::Type::Relayed) {
            // Task 27 follow-up: where the far end's relay is, so a remote
            // learned there as peer-reflexive still reads as relayed
            // (MediaIcePath::relayed()). A numeric address only: no lookup.
            rtc::Candidate relay = parsed;
            if (relay.resolve(rtc::Candidate::ResolveMode::Simple) && relay.address()
                && relay.port()) {
                d->farEndRelays.append(
                    qMakePair(QString::fromStdString(*relay.address()), *relay.port()));
            }
        }
        std::optional<QPair<QString, quint16>> ownedEndpoint;
        if (fromOwnedSource) {
            rtc::Candidate source = parsed;
            if (source.resolve(rtc::Candidate::ResolveMode::Simple)
                && source.address() && source.port()) {
                ownedEndpoint = MediaIcePath::loopbackEndpoint(
                    QString::fromStdString(*source.address()), *source.port());
            }
        }
        logMediaRemoteCandidate(candidate, fromOwnedSource, true);
        d->peer->addRemoteCandidate(std::move(parsed));
        ++d->acceptedCandidates;
        if (ownedEndpoint && !d->ownedShimEndpoints.contains(*ownedEndpoint)) {
            d->ownedShimEndpoints.append(*ownedEndpoint);
        }
        return true;
    } catch (const std::exception& error) {
        emit errorOccurred(QString::fromUtf8(error.what()));
        return false;
    }
}

bool LibDataChannelMediaTransport::sendDisplay(const QByteArray& message)
{
    const DisplaySendResult result = submitDisplay(message);
    return result == DisplaySendResult::Sent || result == DisplaySendResult::Queued;
}

IMediaTransport::DisplaySendResult
LibDataChannelMediaTransport::submitDisplay(const QByteArray& message)
{
    if (!d->ready || !d->display || message.isEmpty()
        || message.size() > kMaxDisplayMessageBytes) {
        return DisplaySendResult::Refused;
    }
    // Latest-value-wins: while the library still holds a message, a new one
    // is not taken, so at most one message ever waits behind SCTP.
    if (d->display->bufferedAmount() != 0) {
        return DisplaySendResult::Busy;
    }
    const std::shared_ptr<CallbackBridge> bridge = d->bridge;
    if (!bridge) {
        return DisplaySendResult::Refused;
    }
    bridge->submittedDisplayPayloadBytes.fetch_add(
        static_cast<quint64>(message.size()), std::memory_order_relaxed);
    try {
        // False means usrsctp had no room (its send buffer counts data not
        // yet acknowledged, usrsctp fec583d5 sctp_output.c:14081-14099), so
        // libdatachannel queued the message and sends it when room appears
        // (v0.24.5 src/impl/sctptransport.cpp:374-393).
        return d->display->send(
                   reinterpret_cast<const rtc::byte*>(message.constData()),
                   static_cast<std::size_t>(message.size()))
            ? DisplaySendResult::Sent : DisplaySendResult::Queued;
    } catch (const std::exception& error) {
        // R-R3-05: a display error is reported once, as a display error.
        emit displayErrorOccurred(QString::fromUtf8(error.what()));
        return DisplaySendResult::Refused;
    }
}

bool LibDataChannelMediaTransport::displayBusy() const
{
    return d->ready && d->display && d->display->bufferedAmount() != 0;
}

IMediaTransport::DisplaySendResult
LibDataChannelMediaTransport::submitIq(const QByteArray& message)
{
    if (!d->ready || !d->iq || !d->iq->isOpen() || message.isEmpty()
        || message.size() > kMaxIqMessageBytes) {
        return DisplaySendResult::Refused;
    }
    if (d->iq->bufferedAmount() != 0) { return DisplaySendResult::Busy; }
    const std::shared_ptr<CallbackBridge> bridge = d->bridge;
    if (!bridge) { return DisplaySendResult::Refused; }
    bridge->submittedIqPayloadBytes.fetch_add(
        static_cast<quint64>(message.size()), std::memory_order_relaxed);
    try {
        return d->iq->send(reinterpret_cast<const rtc::byte*>(message.constData()),
                           static_cast<std::size_t>(message.size()))
            ? DisplaySendResult::Sent : DisplaySendResult::Queued;
    } catch (const std::exception& error) {
        emit iqErrorOccurred(QString::fromUtf8(error.what()));
        return DisplaySendResult::Refused;
    }
}

bool LibDataChannelMediaTransport::iqBusy() const
{
    return d->iq && d->iq->bufferedAmount() != 0;
}

bool LibDataChannelMediaTransport::sendRtp(const QByteArray& packet)
{
    if (!d->ready || !d->audio || packet.size() < kMinRawRtpBytes
        || packet.size() > kMaxRawRtpBytes || d->audio->bufferedAmount() != 0
        || d->audio->maxMessageSize() < static_cast<std::size_t>(packet.size())
        || !isDeclaredAudioSsrc(rtpSsrc(packet), d->localAudioSsrc, d->receiverAudioSsrcs,
                                d->headphonesAudioSsrc)) {
        return false;
    }
    const std::shared_ptr<CallbackBridge> bridge = d->bridge;
    if (!bridge) {
        return false;
    }
    bridge->submittedRtpBytes.fetch_add(
        static_cast<quint64>(packet.size()), std::memory_order_relaxed);
    try {
        return d->audio->send(
            reinterpret_cast<const rtc::byte*>(packet.constData()),
            static_cast<std::size_t>(packet.size()));
    } catch (const std::exception& error) {
        emit errorOccurred(QString::fromUtf8(error.what()));
        return false;
    }
}

bool LibDataChannelMediaTransport::sendMicRtp(const QByteArray& packet)
{
    // Task 36: only an answerer sends on the microphone line, and only the
    // SSRC it declared there.
    // Not before the line's track is open: the transport reports ready
    // without waiting for it.
    if (!d->ready || !d->micAudio || !d->micAudio->isOpen() || d->role != Role::Answerer
        || d->micAudioSsrc == 0 || packet.size() < kMinRawRtpBytes || packet.size() > kMaxRawRtpBytes
        || rtpSsrc(packet) != d->micAudioSsrc || d->micAudio->bufferedAmount() != 0
        || d->micAudio->maxMessageSize() < static_cast<std::size_t>(packet.size())) {
        return false;
    }
    const std::shared_ptr<CallbackBridge> bridge = d->bridge;
    if (!bridge) {
        return false;
    }
    bridge->submittedRtpBytes.fetch_add(
        static_cast<quint64>(packet.size()), std::memory_order_relaxed);
    try {
        return d->micAudio->send(
            reinterpret_cast<const rtc::byte*>(packet.constData()),
            static_cast<std::size_t>(packet.size()));
    } catch (const std::exception& error) {
        emit errorOccurred(QString::fromUtf8(error.what()));
        return false;
    }
}

bool LibDataChannelMediaTransport::sendTx(const QByteArray& message)
{
    // Task 37: now or not at all. The channel never retransmits, so nothing
    // waits behind a lost message; a message still held by the library is
    // not joined by another (the next keepalive supersedes it anyway).
    if (!d->started || !d->tx || !d->tx->isOpen() || message.isEmpty()
        || message.size() > kMaxTxMessageBytes || d->tx->bufferedAmount() != 0) {
        return false;
    }
    const std::shared_ptr<CallbackBridge> bridge = d->bridge;
    if (!bridge) { return false; }
    bridge->submittedTxPayloadBytes.fetch_add(
        static_cast<quint64>(message.size()), std::memory_order_relaxed);
    try {
        d->tx->send(reinterpret_cast<const rtc::byte*>(message.constData()),
                    static_cast<std::size_t>(message.size()));
        return true;
    } catch (const std::exception& error) {
        qWarning().nospace() << "media tx channel send failed: " << error.what();
        return false;
    }
}

void LibDataChannelMediaTransport::setReceiveSeveredForTest(bool severed)
{
    const std::shared_ptr<CallbackBridge> bridge = d->bridge;
    if (!bridge) {
        return;
    }
    std::lock_guard lock(bridge->mutex);
    bridge->receiveSeveredForTest = severed;
}

bool LibDataChannelMediaTransport::micLosslessNegotiated() const
{
    if (!d->started || !d->peer || d->micAudioSsrc == 0 || !d->remoteDescribesMicLossless) {
        return false;
    }
    const std::optional<rtc::Description> local = d->peer->localDescription();
    return local && describesLosslessAudio(*local, kMicMid);
}

bool LibDataChannelMediaTransport::isReady() const
{
    return d->ready;
}

bool LibDataChannelMediaTransport::losslessAudioNegotiated() const
{
    if (!d->started || !d->peer || !d->remoteDescribesLossless) {
        return false;
    }
    // Both sides, not only the remote one: the offerer's own offer, or the
    // answer libdatachannel built from the offer.
    const std::optional<rtc::Description> local = d->peer->localDescription();
    return local && describesLosslessAudio(*local);
}

std::optional<MediaTransportTelemetry>
LibDataChannelMediaTransport::telemetry() const
{
    if (!d->started || !d->bridge) {
        return std::nullopt;
    }
    const std::shared_ptr<CallbackBridge> bridge = d->bridge;
    return MediaTransportTelemetry{
        bridge->receivedDisplayPayloadBytes.load(std::memory_order_relaxed),
        bridge->submittedDisplayPayloadBytes.load(std::memory_order_relaxed),
        bridge->receivedRtpBytes.load(std::memory_order_relaxed),
        bridge->submittedRtpBytes.load(std::memory_order_relaxed),
        bridge->displayMessagesDropped.load(std::memory_order_relaxed),
        bridge->receivedTxPayloadBytes.load(std::memory_order_relaxed),
        bridge->submittedTxPayloadBytes.load(std::memory_order_relaxed),
        bridge->receivedIqPayloadBytes.load(std::memory_order_relaxed),
        bridge->submittedIqPayloadBytes.load(std::memory_order_relaxed),
    };
}

void LibDataChannelMediaTransport::setDisplayReceiveStalledForTest(bool stalled)
{
    const std::shared_ptr<CallbackBridge> bridge = d->bridge;
    if (!bridge) {
        return;
    }
    std::lock_guard lock(bridge->mutex);
    bridge->displayReceiveStalledForTest = stalled;
    bridge->displayReceiveGate.notify_all();
}

void LibDataChannelMediaTransport::drainCallbacks()
{
    if (!d->started || !d->bridge) {
        return;
    }

    QPointer<LibDataChannelMediaTransport> self(this);
    const std::shared_ptr<CallbackBridge> bridge = d->bridge;
    const auto isCurrentGeneration = [&self, &bridge] {
        return self && self->d->started && self->d->bridge == bridge;
    };

    std::deque<CallbackEvent> events;
    std::deque<rtc::binary> displayMessages;
    std::deque<rtc::binary> iqMessages;
    std::deque<PendingRtpPacket> rtpPackets;
    std::chrono::steady_clock::duration maxRtpCallbackGap {};
    std::size_t droppedRtpPackets = 0;
    bool reportIqOverflow = false;
    std::shared_ptr<rtc::DataChannel> incomingDisplay;
    std::shared_ptr<rtc::DataChannel> incomingIq;
    std::shared_ptr<rtc::DataChannel> incomingTx;
    std::shared_ptr<rtc::Track> incomingAudio;
    std::shared_ptr<rtc::Track> incomingMic;
    {
        std::lock_guard lock(bridge->mutex);
        if (bridge->cancelled) {
            return;
        }
        events.swap(bridge->events);
        displayMessages.swap(bridge->displayMessages);
        bridge->displayBytes = 0;
        iqMessages.swap(bridge->iqMessages);
        bridge->iqBytes = 0;
        reportIqOverflow = bridge->iqFailed && !bridge->iqFailureReported;
        if (reportIqOverflow) { bridge->iqFailureReported = true; }
        rtpPackets.swap(bridge->rtpPackets);
        maxRtpCallbackGap = bridge->maxRtpCallbackGap;
        bridge->maxRtpCallbackGap = {};
        droppedRtpPackets = bridge->droppedRtpPackets;
        bridge->droppedRtpPackets = 0;
        incomingDisplay = std::move(bridge->dataChannel);
        incomingIq = std::move(bridge->iqChannel);
        incomingTx = std::move(bridge->txChannel);
        incomingAudio = std::move(bridge->track);
        incomingMic = std::move(bridge->micTrack);
    }

    if (!isCurrentGeneration()) {
        return;
    }
    if (reportIqOverflow) {
        emit iqErrorOccurred(QStringLiteral("raw I/Q receive queue overflow"));
        if (!isCurrentGeneration()) { return; }
    }
    if (!d->tx && incomingTx) {
        d->tx = std::move(incomingTx);
    }

    if (!d->display && incomingDisplay) {
        d->display = std::move(incomingDisplay);
    }
    if (!d->iq && incomingIq) {
        d->iq = std::move(incomingIq);
    }
    if (!d->audio && incomingAudio) {
        d->audio = std::move(incomingAudio);
    }
    if (!d->micAudio && incomingMic) {
        d->micAudio = std::move(incomingMic);
    }

    bool mustStop = false;
    for (CallbackEvent& event : events) {
        switch (event.kind) {
        case CallbackEvent::Kind::Description:
            emit localDescription(QString::fromStdString(event.first),
                                  QString::fromStdString(event.second));
            break;
        case CallbackEvent::Kind::Candidate:
            if (d->ice && d->ice->onlySourceCandidates()) {
                // The fallback onto the tunnel: this computer's addresses
                // stay with it, so the far end has no direct pair to try.
                if (IceDiagnostics::enabled()) {
                    IceDiagnostics::logPath(
                        "media", QStringLiteral("local candidate (not sent, tunnel only) %1")
                                     .arg(QString::fromStdString(event.first)));
                }
                break;
            }
            if (IceDiagnostics::enabled()) {
                IceDiagnostics::logPath("media", QStringLiteral("local candidate %1")
                                                     .arg(QString::fromStdString(event.first)));
            }
            emit localCandidate(QString::fromStdString(event.first),
                                QString::fromStdString(event.second));
            break;
        case CallbackEvent::Kind::Error:
            emit errorOccurred(QString::fromStdString(event.first));
            break;
        case CallbackEvent::Kind::DisplayError: {
            // Reported once: as a display error while this display channel
            // is current, otherwise as a plain error, as it always was.
            const QString text = QString::fromStdString(event.first);
            if (isCurrentGeneration()) {
                emit displayErrorOccurred(text);
            } else {
                emit errorOccurred(text);
            }
            break;
        }
        case CallbackEvent::Kind::DisplayWritable:
            emit displayWritable();
            break;
        case CallbackEvent::Kind::IqError:
            emit iqErrorOccurred(QString::fromStdString(event.first));
            break;
        case CallbackEvent::Kind::PeerFailed:
            // Terminal peer connectivity is a recovery decision for the
            // authenticated session owner.  Keep it typed: generic media
            // errors also describe malformed packets and local decoder/device
            // failures, none of which may redial the station.
            emit connectionFailed(QString::fromStdString(event.first));
            mustStop = true;
            break;
        case CallbackEvent::Kind::PeerClosed:
            mustStop = true;
            break;
        case CallbackEvent::Kind::TxMessage:
            emit txReceived(QByteArray(event.first.data(),
                                       static_cast<qsizetype>(event.first.size())),
                            heldMicroseconds(event.receivedAt));
            break;
        case CallbackEvent::Kind::GatheringComplete:
            emit gatheringComplete();
            break;
        }
        if (!isCurrentGeneration()) {
            return;
        }
    }

    if (mustStop) {
        stop();
        return;
    }

    // G-127: readiness is settled before anything this drain collected is
    // delivered. Under load one drain can find the display channel just
    // opened and the first message on it together; delivered first, the
    // message reached a transport that was not yet ready, so a reply to it
    // (the traversal echo) was refused and never sent.
    const bool nowReady = d->peer
        && d->peer->state() == rtc::PeerConnection::State::Connected
        && d->display && d->display->isOpen()
        && d->audio && d->audio->isOpen();
    if (nowReady && !d->ready) {
        d->ready = true;
        if (IceDiagnostics::enabled()) {
            IceDiagnostics::logSelectedPair("media", *d->peer);
        }
        emit ready();
        if (!isCurrentGeneration()) {
            return;
        }
    } else if (!nowReady) {
        d->ready = false;
    }

    for (const rtc::binary& message : displayMessages) {
        emit displayReceived(toByteArray(message));
        if (!isCurrentGeneration()) {
            return;
        }
    }
    for (const rtc::binary& message : iqMessages) {
        emit iqReceived(toByteArray(message));
        if (!isCurrentGeneration()) { return; }
    }
    if (!rtpPackets.empty()) {
        const auto drainedAt = std::chrono::steady_clock::now();
        const auto oldestRtpQueueAge = drainedAt - rtpPackets.front().receivedAt;
        if ((maxRtpCallbackGap > kRtpTimingWarningThreshold
             || oldestRtpQueueAge > kRtpTimingWarningThreshold)
            && (d->lastRtpTimingWarning == std::chrono::steady_clock::time_point {}
                || drainedAt - d->lastRtpTimingWarning >= kRtpTimingWarningInterval)) {
            d->lastRtpTimingWarning = drainedAt;
            qWarning().nospace()
                << "media RTP timing: callbackGapMs="
                << std::chrono::duration_cast<std::chrono::milliseconds>(maxRtpCallbackGap).count()
                << " ownerDrainAgeMs="
                << std::chrono::duration_cast<std::chrono::milliseconds>(oldestRtpQueueAge).count()
                << " batchPackets=" << rtpPackets.size()
                << " droppedPending=" << droppedRtpPackets;
        }
    }
    for (const PendingRtpPacket& packet : rtpPackets) {
        // Task 36: the microphone line's packets are reported apart.
        if (packet.mic) {
            // TX stall lane: the time since the library's callback took it
            // off the network, so the microphone buffer times it there.
            emit micRtpReceived(toByteArray(packet.data), heldMicroseconds(packet.receivedAt));
        } else {
            emit rtpReceived(toByteArray(packet.data));
        }
        if (!isCurrentGeneration()) {
            return;
        }
    }
}

} // namespace NereusSDR
