#pragma once
// =================================================================
// src/core/session/media/RemoteMicReceiver.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original.
//
// iPhone app plan Task 36 (R-IOS-13; remote design sections 8.2 and 8.3;
// spec section 4.1): the microphone uplink at the Core.
//
// A remote device sends its microphone on the media connection's
// microphone line (mid "mic"): Opus mono 48 kHz in 20 ms frames with
// in-band FEC, or, from a desktop remote window whose link can carry it,
// the L16 format of PcmAudioCodec. Three pieces live here:
//
//   RemoteMicReceiver (the Core's event loop): RTP in, sequence order,
//     Opus or L16 decode, loss concealment (Opus PLC, and in-band FEC
//     from the next packet for the packet just before it), the key's
//     readiness wait and the starvation signal. Decoded audio goes into
//     the feed.
//   RemoteMicFeed (the boundary): a lock-free ring from the receiver to
//     the transmit pump, and on the pump's side the transmit jitter
//     buffer and WDSP rmatch (RemoteAudioRateMatcher) at a ratio the
//     buffer sets. R-IOS-13 (2026-09-27): the buffer adds no latency it
//     does not need. Its target is one packet plus a margin that starts
//     at 10 ms (30 ms for the phone's 20 ms packets), grows only with the
//     jitter it measures in the packets' arrivals, and eases back while
//     the link is steady, never past 120 ms. A standing excess
//     (after a stall, or queued past the radio's send ring) is shed only
//     in silence: a whole near-silent 64-frame block, deep in a pause, is
//     dropped before TX DSP, so the transmitted I/Q never splices. After
//     every change of use it gives the pump silence until it holds its
//     target, then audio. A stall that ran the buffer dry and then
//     delivers more than kStaleAfterStallMs at once is stale: the buffer
//     starts again at its target with the newest audio (LINK minor 12).
//   RemoteMicEncoder: the encoder a desktop remote window (and the tests)
//     send the line with, Opus mono 20 ms frames with in-band FEC.
//
// Threading: the receiver's control (start, stop, the key's wait,
// starvation) and the feed's setInUse run on the Core's event loop. A
// line's packets (RemoteMicReceiver::submit, which decodes and calls the
// feed's write) run on the thread that delivers them: the media
// transport's microphone thread on a real connection (TX mic thread, so a
// stall of the event loop never holds the line's audio), the event loop
// otherwise. The receiver's decoder state and the feed's writer side are
// each under a lock of their own, which the pump never takes. The feed's
// pump half (pull) runs on the transmit pump's thread and never locks,
// waits or allocates except when the feed changes use (the rate matcher is
// rebuilt then). No codec work runs on the pump (R-R3-06).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 36 (R-IOS-13), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave C2: setFeedWriter, one line
//               writes the transmitter's feed at a time. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: R-IOS-13, R-R3-42: the small adaptive transmit buffer,
//               shedding a standing excess only in silence (here and
//               past the radio's send ring), and the over's latency
//               figures. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-28: Load findings 2 (R-IOS-13): the key's 250 ms fill wait
//               runs from the line's first packet, with 1 s for that
//               packet to come, so a line's cold start does not refuse
//               every key. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: LINK minor 12 (TX audio): kStaleAfterStallMs and the
//               trim on resuming after a long stall. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-30: TX stall lane: each packet is timed at its receipt in the
//               transport, not when the Core's event loop hands it over,
//               so a stalled drain is not counted as network jitter
//               (submit's heldUs, write's heldFrames). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX stall lane, fix round 1: the buffer is timed by the
//               pump's drain again, as before (a stalled drain grows the
//               margin); a packet's wait at the Core is a measurement
//               only, in the over's figures (owner waits). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX mic thread (JJ approved): submit may run on the media
//               transport's microphone thread; the receiver's decoder
//               state and the feed's writer side take a lock each (never
//               the pump), the line's last audio is an atomic, and the
//               key's wait and starvation follow on the event loop. With
//               the line off the event loop, the buffer is timed at the
//               packet's receipt again (a6f832b90's timing). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX diagnostics lane: each packet carries its receipt gap
//               and its receipt minus RTP timestamp to the pump
//               (ArrivalTiming), and the feed places the over's first
//               underruns in time (Stats::underrunsPlaced). Measurement
//               only. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-10-01: Mic 48k lane: kOpusBitrate is the desktop remote window's
//               rate, within the line's 48 kbit/s offer; kSilencePeak
//               re-measured at the phone's 48 kbit/s full band and kept.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/AudioRingSpsc.h"

#include <QByteArray>
#include <QObject>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <vector>

namespace NereusSDR {

class RemoteAudioRateMatcher;

/// The microphone line's numbers (Task 36; R-IOS-13 2026-09-27 for the
/// buffer: small and adaptive, see RemoteMicFeed).
struct RemoteMicConfig {
    static constexpr int kSampleRate = 48'000;
    static constexpr int kFramesPerMs = kSampleRate / 1000;
    /// Opus, payload type 111, mono, 20 ms frames (the app's microphone
    /// encoder profile).
    static constexpr int kOpusPayloadType = 111;
    static constexpr int kOpusFrameSamples = 960;
    /// The bitrate a desktop remote window's encoder sends at
    /// (RemoteMicEncoder), within the line's offered maxaveragebitrate
    /// (LibDataChannelMediaTransport.h kMicLineMaxAverageBitrate, 48
    /// kbit/s, the ceiling set for the phone's microphone).
    static constexpr int kOpusBitrate = 24'000;
    /// The transmit pump's block (TxWorkerThread::kBlockFrames).
    static constexpr int kPumpBlockFrames = 64;

    // ---- The transmit jitter buffer (R-IOS-13, 2026-09-27) ----
    /// The margin: audio the buffer keeps past one packet, i.e. its fill
    /// just before the next packet lands. It starts here and never goes
    /// below: 10 ms covers the Core's own delivery of a packet and the
    /// pump's bursts without adding a fixed cushion.
    static constexpr int kMinMarginMs = 10;
    /// The smallest target, for the phone's 20 ms packets: one packet plus
    /// the margin. A key waits for the buffer to reach its target.
    static constexpr int kTargetDepthMs = 20 + kMinMarginMs;
    /// The ceiling: one packet plus the margin never passes this.
    static constexpr int kMaxDepthMs = 120;
    /// Growth follows measured jitter. Each arrival's delay is measured
    /// against its place in the stream on the pump's (the radio's) clock;
    /// the spread between the latest and the earliest of the last
    /// kJitterMemoryMs is the jitter, and the margin is kMinMarginMs plus
    /// it, at once when that is more; when it is less, the margin eases
    /// back kShrinkStepMs every kShrinkIntervalMs. A packet lost and rebuilt
    /// from the next one's FEC arrives with it, one packet late, and counts
    /// so. A delay the ceiling could not cover anyway (a stall) is ridden
    /// through and grows nothing.
    static constexpr int kJitterMemoryMs = 10'000;
    static constexpr int kShrinkIntervalMs = 1000;
    static constexpr int kShrinkStepMs = 5;
    /// The buffer is measured over windows (CoDel's rule: an excess is
    /// standing only when the lowest fill of a whole window, with that
    /// window's jitter added back, is above the margin; one that dips is
    /// only jitter).
    static constexpr int kWindowMs = 250;
    /// Excess or deficit under this is left alone.
    static constexpr int kReserveMs = 3;
    /// Silence. A 64-frame block counts as silent when its peak is under
    /// -40 dBFS, and it may be shed (or silence inserted before it) only
    /// after 20 ms of silence; speech blocks peak far above it (a splice
    /// at the threshold steps by at most 0.02 of full scale, 34 dB under
    /// speech peaks, and TX DSP filters it in band). Measured at the
    /// phone's Opus 48 kbit/s full band (mic 48k lane,
    /// tst_tx_leveler_remote_mic phonePausesStayUnderTheSilencePeak,
    /// speech peaking at -6 dBFS, a white full-band microphone floor,
    /// which is the worst case for the full-band path): the p99 pause
    /// block peaks at -52 dBFS over a -80 dBFS RMS floor, -48 over -60
    /// and -44 over -55, so pauses qualify for any floor up to about
    /// -55 dBFS. The threshold rests on these. The earlier 24 kbit/s
    /// figures (tx-latency report: -73 dBFS p50 over a -80 floor, -54
    /// p50 and -50 p99 over -60) sit about 6 dB above this harness's own
    /// 24 kbit/s results; the cause is unknown and the original harness
    /// is gone.
    static constexpr float kSilencePeak = 0.01f;
    static constexpr int kSilenceRunMs = 20;
    /// Clock matching: the ratio moves the buffer toward its margin at
    /// 50 ppm per ms of standing deviation, at most 500 ppm (under a cent
    /// of pitch; rmatch's own control ranges to 4 percent), so a sender's
    /// clock that runs fast or slow never needs silence to hold the buffer.
    static constexpr double kRatioPpmPerMs = 50.0;
    static constexpr double kMaxRatioPpm = 500.0;
    /// The lowest the radio's send ring normally gets over a window (P2:
    /// under 1.6 ms, measured in tst_tx_mic_latency; it holds under a
    /// 240-sample frame plus a 256-sample pump block at 192 kHz between
    /// sends). What stands past it for a whole window is excess the buffer
    /// sheds, a pump block at a time.
    static constexpr int kRingSlackMs = 2;

    static constexpr int kMinMarginFrames = kFramesPerMs * kMinMarginMs;
    static constexpr int kTargetDepthFrames = kFramesPerMs * kTargetDepthMs;
    static constexpr int kMaxDepthFrames = kFramesPerMs * kMaxDepthMs;
    /// A gap in the stream is concealed only up to this (a longer one
    /// inserts nothing: the buffer has run dry meanwhile anyway).
    static constexpr int kMaxConcealMs = 60;
    static constexpr int kMaxConcealFrames = kFramesPerMs * kMaxConcealMs;

    /// How long a keyed device's line may carry no audio before it counts
    /// as starved, and how long a key waits for the buffer to fill, counted
    /// from the line's first packet in the wait.
    static constexpr int kStarvationMs = 250;
    static constexpr int kReadyDeadlineMs = 250;
    /// Load findings 2 (R-IOS-13): how long a key waits for that first
    /// packet. A device starts its line when the key is pressed (a window
    /// opens its microphone only then), so the line's cold start (capture
    /// open, encode, transport) comes before any fill and is timed apart
    /// from it. A line that never delivers is refused at this bound, with
    /// the same reason; a key comes at most kLineStartDeadlineMs plus
    /// kReadyDeadlineMs after it arrives.
    static constexpr int kLineStartDeadlineMs = 1000;
    /// LINK minor 12: after a stall the line delivers what it held at
    /// once. Up to this much (the time a keyed line may go silent before
    /// it counts as starved) still plays whole and is shed in pauses; past
    /// it the audio is stale, and the buffer starts again from its target
    /// with the newest audio, so the radio never sends speech seconds
    /// late.
    static constexpr int kStaleAfterStallMs = kStarvationMs;
    static constexpr int kStaleAfterStallFrames = kFramesPerMs * kStaleAfterStallMs;
    /// TX stall lane: a packet that waited at the Core longer than this
    /// between its receipt and the feed (a stall of the thread delivering
    /// it) is counted in the over's figures. Log only.
    static constexpr int kLongOwnerWaitMs = 50;

    static_assert(kMaxDepthMs < kStarvationMs,
                  "the transmit jitter buffer is shorter than the starvation deadline");
    static_assert(kTargetDepthMs < kReadyDeadlineMs,
                  "a key can reach the smallest target before its deadline");
    static_assert(kMaxDepthMs < kStaleAfterStallMs,
                  "a stall's audio is stale only past what the buffer could hold");
    static_assert(kReadyDeadlineMs < kLineStartDeadlineMs,
                  "a line gets longer to start than to fill once started");
};

/// Whether an Opus payload carries in-band FEC for the frame before it
/// (opus_packet_has_lbrr). Opus codes it only for frames its voice detector
/// calls active.
bool opusPacketCarriesFec(const QByteArray& payload);

/// TX diagnostics lane: a packet's arrival as the line saw it, carried to
/// the pump with its audio for the over's figures only
/// (RemoteMicFeed::write).
struct RemoteMicArrivalTiming {
    static constexpr qint64 kNoOffset = std::numeric_limits<qint64>::min();
    /// Time since the line's previous packet was received, in us (-1: none
    /// before it).
    qint64 receiptGapUs{-1};
    /// Its receipt less its RTP timestamp's time, in us, on the line's own
    /// scale (only differences between packets mean anything).
    qint64 rtpOffsetUs{kNoOffset};
};

/// The boundary between the receiver and the transmit pump, and the transmit
/// jitter buffer (R-IOS-13, 2026-09-27).
///
/// The buffer keeps one packet plus a margin (RemoteMicConfig). Each pump
/// block it feeds rmatch only as much as the block needs, so the margin,
/// not rmatch's ring, is the delay; rmatch runs at a forced ratio that
/// matches the sender's clock to the radio's (at most 500 ppm).
///
/// It times each arrival against its place in the stream on the pump's
/// clock: the spread of those delays over the last 10 s is the jitter, and
/// the margin is 10 ms plus it, growing at once and easing back 5 ms a
/// second. A block with nothing to play (an underrun: rmatch fades out) is
/// followed by silence until the buffer holds its target again.
///
/// Every window (250 ms) it takes the lowest fill it saw, adds back the
/// jitter that window saw, and compares that with the margin: what stood
/// above it for the whole window is excess, and what stood in the radio's
/// send ring past its slack (the caller passes the ring's fill in) is
/// excess too. Both are shed only in silence: a whole block under -40
/// dBFS, after 20 ms of silence, is dropped before rmatch. Excess in the
/// buffer is shed by dropping a block within a pump block; excess past
/// the send ring by dropping a block and telling the pump to skip that
/// block (pullBlock returns Shed), so TX DSP never runs on it and the ring
/// drains by one block. A deficit is filled the same way, with a silent
/// block inserted. The I/Q the radio sends stays continuous: nothing is
/// ever cut after TX DSP. Nothing is spliced while DEXP's own timing runs
/// (its hold, decay or VOX turn-off), which it counts in the samples it
/// processes, so shedding never moves the VOX hang or the expander.
class RemoteMicFeed {
public:
    RemoteMicFeed();
    ~RemoteMicFeed();
    RemoteMicFeed(const RemoteMicFeed&) = delete;
    RemoteMicFeed& operator=(const RemoteMicFeed&) = delete;

    // ---- the writers' side: setInUse on the Core's event loop, write on
    // ---- the thread delivering a line's packets (under m_writerLock,
    // ---- which the pump never takes) ----

    /// Whether the pump takes its audio from this feed. Every change empties
    /// the feed: the audio that was waiting is dropped and the pump starts
    /// again from silence until the buffer holds its target. The margin is
    /// the link's and is kept.
    void setInUse(bool inUse);
    bool inUse() const { return m_inUse.load(std::memory_order_acquire); }
    /// Frames written since the feed last went in use. Any thread.
    qint64 framesSinceInUse() const
    {
        return m_framesSinceInUse.load(std::memory_order_acquire);
    }
    using ArrivalTiming = RemoteMicArrivalTiming;
    /// Mono 48 kHz, one packet a call. Refused (false) while not in use;
    /// audio that does not fit the pump's input ring is dropped and counted.
    /// `heldFrames` (TX stall lane): how long, in frames, the packet waited
    /// at the Core between its receipt in the transport and this write. The
    /// buffer times the packet that much earlier (at its receipt), and the
    /// wait goes into the over's figures (Stats::ownerWait*). `timing` (TX
    /// diagnostics lane) is measured only.
    bool write(const float* mono, int frames, int heldFrames = 0, ArrivalTiming timing = {});
    /// The buffer's target now (one packet plus the margin), in frames:
    /// what a key waits for. Any thread.
    int targetFrames() const { return m_targetFrames.load(std::memory_order_relaxed); }

    // ---- the transmit pump's thread ----

    enum class Pull {
        NotInUse,   ///< not in use: dst untouched, arrivals dropped
        Audio,      ///< dst holds the block (silence while filling)
        Shed,       ///< a silent block was shed for the send ring: skip
                    ///< this pump block entirely (dst untouched)
    };
    /// Called on every pump block. `frames` must be
    /// RemoteMicConfig::kPumpBlockFrames. `downstreamQueuedMs` is what the
    /// radio's send ring holds now (RadioConnection::txIqQueuedMs), or
    /// negative when unknown (then Shed is never returned). While
    /// `holdSplices` is true (DEXP's hold, decay or VOX turn-off is
    /// counting, TxChannel::dexpTimingRunning) nothing is shed or inserted.
    Pull pullBlock(float* dst, int frames, double downstreamQueuedMs,
                   bool holdSplices = false);
    /// pullBlock without a send ring: true while in use.
    bool pull(float* dst, int frames)
    {
        return pullBlock(dst, frames, -1.0) != Pull::NotInUse;
    }

    // ---- any thread ----

    struct Stats {
        /// Whether the pump has reached the target since the last change of
        /// use (or underrun) and is taking audio.
        bool started{false};
        /// Frames in the buffer (the jitter buffer and rmatch), as the pump
        /// last read it.
        int fillFrames{0};
        /// The ratio rmatch runs at (output per input).
        double ratio{1.0};
        /// Since the last change of use: times the buffer ran empty after it
        /// started, and times rmatch overflowed (never, while the buffer
        /// feeds it a block at a time).
        int underflows{0};
        int overflows{0};
        /// Frames the owner could not put in the pump's input ring.
        quint64 droppedFrames{0};
        /// Changes of use the pump has seen.
        quint64 changes{0};

        // R-IOS-13 (2026-09-27).
        /// The target (one packet plus the margin) and the margin now.
        int targetFrames{0};
        int marginFrames{0};
        /// Over the last time in use (kept after it ends, reset when the
        /// feed next goes in use), per pump block once playing: the delay
        /// the path added to the microphone (this buffer plus the radio's
        /// send ring when known), and the send ring alone.
        int blocks{0};
        double addedMeanMs{0.0};
        double addedMaxMs{0.0};
        /// Negative when the send ring was never known.
        double ringMeanMs{-1.0};
        double ringMaxMs{-1.0};
        /// Silence shed from the buffer, shed for the send ring (blocks the
        /// pump skipped), and inserted, in frames; margin growths; pump
        /// blocks in which DEXP's timing held every splice back.
        quint64 shedFrames{0};
        quint64 shedForRingFrames{0};
        quint64 insertedFrames{0};
        int grows{0};
        int heldBlocks{0};
        /// TX stall lane: how long the over's packets waited at the Core
        /// between their receipt in the transport and the feed (a stall of
        /// the Core's event loop), mean and longest in ms, and how many
        /// waited over kLongOwnerWaitMs. With the line on the transport's
        /// microphone thread these stay near 0 through a stall of the
        /// event loop.
        double ownerWaitMeanMs{0.0};
        double ownerWaitMaxMs{0.0};
        int ownerWaitsLong{0};
        /// TX diagnostics lane: the over's first underruns placed in time
        /// (kept after it ends, reset when the feed next goes in use).
        struct Underrun {
            /// When it came, in ms since the feed went in use (the key's
            /// wait for the buffer), and on the steady clock in us.
            double atLineMs{0.0};
            qint64 atSteadyUs{-1};
            /// How long until the buffer played again, in ms (the fade-out
            /// block and the silence after it); -1 while it has not.
            double silentMs{-1.0};
            /// Around it (the two windows before it, through to playing
            /// again): the largest gap between the line's packet receipts,
            /// and the latest a packet came against its RTP timestamp,
            /// measured from the over's earliest; -1 when none was known.
            double arrivalGapMs{-1.0};
            double lateMs{-1.0};
        };
        static constexpr int kMaxUnderrunsPlaced = 4;
        int underrunsPlacedCount{0};
        std::array<Underrun, kMaxUnderrunsPlaced> underrunsPlaced{};
    };
    Stats stats() const;

private:
    static constexpr std::size_t kInputRingBytes = 131072;  // 682 ms of mono float
    // TX stall lane: one record a write, in step with the input ring, so the
    // pump can measure each packet's wait at the Core. 511 records, more
    // than the input ring holds packets.
    // TX diagnostics lane: with the receipt gap and RTP offset, 32 bytes a
    // record, and the ring twice the size, so it still holds 511.
    struct Arrival {
        quint64 endBytes;   // m_writtenBytes after the write
        qint32 frames;
        qint32 heldFrames;
        qint64 receiptGapUs;
        qint64 rtpOffsetUs;
    };
    static constexpr std::size_t kArrivalRingBytes = 16384;
    static constexpr int kBufferFrames = 65536;              // 1.37 s: stall headroom
    static constexpr int kMatcherRingFrames = 1024;          // rmatch's ring, 21 ms

    void discardInputTo(quint64 bytes);
    void discardAllInput();
    // Pump.
    int drainInput();
    /// TX stall lane: takes the records of the writes the pump has now read
    /// whole. With `note`, what one drain found counts as one arrival,
    /// timed at its first write's receipt (noteArrival), and each write's
    /// wait at the Core goes into the over's figures; without, the records
    /// of discarded audio are only passed.
    void takeArrivals(bool note);
    bool headIsSilent() const;
    bool canSplice() const;
    void popBlock(float* mono);
    void dropBlock();
    /// LINK minor 12: drops the oldest audio, buffered and still in the
    /// input ring, down to the target (resuming after a long stall).
    void trimStaleToTarget();
    int currentPacketFrames() const;
    int currentTarget() const;
    int marginCeiling() const;
    void noteArrival(int frames, int heldFrames);
    /// TX diagnostics lane: one write's receipt gap and RTP offset into the
    /// window's and any open underrun's figures.
    void noteArrivalTiming(const Arrival& arrival);
    void openUnderrun();
    void closeUnderrun();
    void publishUnderrun(int index);
    bool memoryDelayMin(qint64* min) const;
    void endBlock();
    void endWindow();
    void resetWindow();
    void publishOver();

    // Writers' side: setInUse and write, serialized by m_writerLock (TX
    // mic thread: write runs on the line's delivering thread). inUse and
    // framesSinceInUse are read anywhere.
    std::mutex m_writerLock;
    std::atomic<bool> m_inUse{false};
    std::atomic<qint64> m_framesSinceInUse{0};
    quint64 m_writtenBytes{0};

    // Shared.
    AudioRingSpsc<kInputRingBytes> m_input;
    AudioRingSpsc<kArrivalRingBytes> m_arrivals;
    std::atomic<bool> m_inUseForPump{false};
    std::atomic<quint64> m_clearAtBytes{0};
    std::atomic<quint64> m_change{0};
    std::atomic<quint64> m_droppedFrames{0};
    std::atomic<int> m_packetFrames{RemoteMicConfig::kOpusFrameSamples};
    std::atomic<int> m_targetFrames{RemoteMicConfig::kTargetDepthFrames};
    std::atomic<bool> m_statsStarted{false};
    std::atomic<int> m_statsFill{0};
    std::atomic<double> m_statsRatio{1.0};
    std::atomic<int> m_statsUnderflows{0};
    std::atomic<int> m_statsOverflows{0};
    std::atomic<quint64> m_statsChanges{0};
    std::atomic<int> m_statsMargin{RemoteMicConfig::kMinMarginFrames};
    std::atomic<int> m_statsBlocks{0};
    std::atomic<double> m_statsAddedMeanMs{0.0};
    std::atomic<double> m_statsAddedMaxMs{0.0};
    std::atomic<double> m_statsRingMeanMs{-1.0};
    std::atomic<double> m_statsRingMaxMs{-1.0};
    std::atomic<quint64> m_statsShed{0};
    std::atomic<quint64> m_statsShedForRing{0};
    std::atomic<quint64> m_statsInserted{0};
    std::atomic<int> m_statsGrows{0};
    std::atomic<int> m_statsHeld{0};
    std::atomic<double> m_statsOwnerWaitMeanMs{0.0};
    std::atomic<double> m_statsOwnerWaitMaxMs{0.0};
    std::atomic<int> m_statsOwnerWaitsLong{0};
    // TX diagnostics lane: the placed underruns, published field by field
    // (log only; a reader may see one half-updated).
    std::array<std::atomic<double>, Stats::kMaxUnderrunsPlaced> m_statsUnderrunAtMs{};
    std::array<std::atomic<qint64>, Stats::kMaxUnderrunsPlaced> m_statsUnderrunSteadyUs{};
    std::array<std::atomic<double>, Stats::kMaxUnderrunsPlaced> m_statsUnderrunSilentMs{};
    std::array<std::atomic<double>, Stats::kMaxUnderrunsPlaced> m_statsUnderrunGapMs{};
    std::array<std::atomic<double>, Stats::kMaxUnderrunsPlaced> m_statsUnderrunLateMs{};
    std::atomic<int> m_statsUnderrunsPlaced{0};

    // Pump.
    std::unique_ptr<RemoteAudioRateMatcher> m_matcher;
    quint64 m_seenChange{0};
    quint64 m_readBytes{0};
    bool m_started{false};
    // LINK minor 12: the buffer ran dry while in use; the next start may
    // find a stall's held audio.
    bool m_resumingAfterUnderrun{false};
    std::vector<float> m_buffer;   // the jitter buffer, a ring of kBufferFrames
    int m_bufferHead{0};
    int m_bufferCount{0};
    int m_matcherFill{0};
    int m_marginFrames{RemoteMicConfig::kMinMarginFrames};
    double m_ratio{1.0};
    int m_silentRunFrames{0};
    int m_underflows{0};
    // Budgets for the current window, in frames.
    int m_shedBudget{0};
    int m_insertBudget{0};
    int m_ringShedBudget{0};
    // Time, in pump blocks.
    quint64 m_block{0};
    quint64 m_windowStart{0};
    quint64 m_lastShrink{0};
    int m_windowFloor{0};
    bool m_windowPlayed{false};
    bool m_windowUnderrun{false};
    // Arrival timing: each arrival's delay against its place in the
    // stream, in frames of the pump's clock (pump blocks x 64 less the
    // frames that arrived before it), the window's and the last
    // kJitterMemoryMs's lowest and highest.
    quint64 m_arrivedFrames{0};
    bool m_windowArrived{false};
    qint64 m_windowDelayMin{0};
    qint64 m_windowDelayMax{0};
    std::vector<qint64> m_delayMinMemory;
    std::vector<qint64> m_delayMaxMemory;
    int m_memoryIndex{0};
    double m_windowRingFloorMs{-1.0};
    // The over's figures.
    int m_overBlocks{0};
    double m_overAddedSumMs{0.0};
    double m_overAddedMaxMs{0.0};
    int m_overRingBlocks{0};
    double m_overRingSumMs{0.0};
    double m_overRingMaxMs{-1.0};
    quint64 m_overShed{0};
    quint64 m_overShedForRing{0};
    quint64 m_overInserted{0};
    int m_overGrows{0};
    int m_overHeld{0};
    int m_overOwnerWaits{0};
    qint64 m_overOwnerWaitSumFrames{0};
    int m_overOwnerWaitMaxFrames{0};
    int m_overOwnerWaitsLong{0};
    // TX diagnostics lane (pump): the block the feed last changed use at,
    // the over's arrivals and earliest RTP offset, the largest receipt gap
    // and lateness of this window and the one before, and the placed
    // underruns (the open one's index and start block, -1 for none).
    quint64 m_changeBlock{0};
    int m_overArrivals{0};
    qint64 m_overRtpOffsetMinUs{std::numeric_limits<qint64>::max()};
    qint64 m_windowGapMaxUs{-1};
    qint64 m_prevWindowGapMaxUs{-1};
    qint64 m_windowLateMaxUs{-1};
    qint64 m_prevWindowLateMaxUs{-1};
    std::array<Stats::Underrun, Stats::kMaxUnderrunsPlaced> m_underruns{};
    int m_underrunsPlaced{0};
    int m_openUnderrun{-1};
    quint64 m_openUnderrunBlock{0};
    std::vector<float> m_monoScratch;
    std::vector<float> m_stereoScratch;
};

/// Opus mono 48 kHz, 20 ms frames, 24 kbit/s, in-band FEC: the microphone
/// line as a desktop remote window sends it.
class RemoteMicEncoder {
public:
    RemoteMicEncoder();
    ~RemoteMicEncoder();
    RemoteMicEncoder(const RemoteMicEncoder&) = delete;
    RemoteMicEncoder& operator=(const RemoteMicEncoder&) = delete;

    bool isReady() const;
    /// One RTP packet from RemoteMicConfig::kOpusFrameSamples mono samples;
    /// empty on failure.
    QByteArray encode(const float* mono, quint16 sequence, quint32 timestamp,
                      quint32 ssrc);
    void reset();

private:
    struct State;
    std::unique_ptr<State> m_state;
};

/// The receiver of one media connection's microphone line.
class RemoteMicReceiver final : public QObject {
    Q_OBJECT

public:
    /// Monotonic milliseconds.
    using Clock = std::function<qint64()>;
    /// Runs `fire` once after `ms` (a QTimer by default; tests drive it).
    using Scheduler = std::function<void(int ms, std::function<void()> fire)>;

    struct Stats {
        quint64 accepted{0};
        /// Frames decoded from packets that arrived.
        quint64 decodedPackets{0};
        /// Lost packets rebuilt from the next packet's in-band FEC.
        quint64 recoveredPackets{0};
        /// Lost packets concealed (Opus PLC, including a packet whose next
        /// carried no FEC, or silence for L16).
        quint64 concealedPackets{0};
        /// Arrived behind the stream (reordered too late, or repeated).
        quint64 latePackets{0};
        /// Gaps longer than the buffer's target: nothing inserted.
        quint64 longGaps{0};
        /// Refused: another payload type, an undecodable payload, or L16
        /// the connection did not agree.
        quint64 rejectedPackets{0};
        /// Frames given to the feed while it was in use.
        quint64 framesWritten{0};
    };

    RemoteMicReceiver(RemoteMicFeed* feed, QObject* parent = nullptr, Clock clock = {},
                      Scheduler scheduler = {});
    ~RemoteMicReceiver() override;

    /// One connection's line: its SSRC, and whether it agreed the L16
    /// format. False when the decoder cannot be built.
    bool start(quint32 ssrc, bool losslessNegotiated);
    /// Ends the line; a key waiting on it is answered not ready.
    void stop();
    bool isRunning() const;
    quint32 ssrc() const;
    void setLosslessNegotiated(bool negotiated);

    /// One RTP packet from the line (MediaPeer::micRtpReceived, or the
    /// transport's microphone thread). `heldUs` (TX stall lane): how long
    /// it waited between its receipt in the transport and this call; the
    /// buffer times it at its receipt, and the over's figures count it.
    /// TX mic thread: any thread. The packet's audio reaches the feed in
    /// this call; what follows on the event loop (the key's wait, the end
    /// of a starvation) runs in it when called there, and is posted there
    /// otherwise.
    void submit(const QByteArray& packet, qint64 heldUs = 0);

    /// The key's wait (Task 36): calls done(true) as soon as the feed, in
    /// use, holds its target (RemoteMicFeed::targetFrames, 30 ms on a
    /// steady link), or done(false) when it has not within 250 ms of the
    /// line's first packet in the wait, or when no packet has come within
    /// 1 s (kLineStartDeadlineMs; load findings 2). The key never keys
    /// without the line's audio in the feed. A second wait replaces the
    /// first, which is never answered.
    void awaitReady(std::function<void(bool ready)> done);
    /// Ends a wait without answering it.
    void cancelWait();
    bool isWaiting() const { return static_cast<bool>(m_waitDone); }

    /// Fix wave C2: whether this line writes the transmitter's feed now.
    /// The Core has one writer at a time, the device the transmitter takes
    /// its audio from (RadioModel::remoteMicWriter()); every other line is
    /// decoded but dropped. True by default (one line on its own).
    void setFeedWriter(bool writer);
    bool isFeedWriter() const { return m_feedWriter.load(std::memory_order_acquire); }

    /// Whether a device holding transmit is keyed on this line's audio now.
    /// While it is, 250 ms without audio emits starved(true), and audio
    /// arriving again starved(false). Turning it off ends a starvation.
    void setWatching(bool watching);
    bool isWatching() const { return m_watching; }
    bool isStarved() const { return m_starved; }

    Stats stats() const;

signals:
    void starved(bool starved);

private:
    /// TX mic thread: the event loop's part of a packet (the end of a
    /// starvation, the next starvation check, the key's wait).
    void afterPacket(bool wroteAudio);
    void decodeOpus(const QByteArray& payload, int missing);
    void decodeL16(const QByteArray& packet, int missing);
    void writeAudio(const float* mono, int frames);
    void checkReady();
    void refuseWaitAfter(int ms, bool onlyBeforeFirstPacket);
    void scheduleStarvationCheck(int ms);
    void checkStarvation();
    qint64 now() const;

    struct Decoder;
    RemoteMicFeed* m_feed{nullptr};
    Clock m_clock;
    Scheduler m_scheduler;
    // TX mic thread: the line's state below, down to m_stats, is under
    // m_lineLock (submit may run on the transport's microphone thread).
    // Nothing is called or emitted while it is held.
    mutable std::mutex m_lineLock;
    std::unique_ptr<Decoder> m_decoder;
    bool m_running{false};
    bool m_lossless{false};
    quint32 m_ssrc{0};
    bool m_haveSequence{false};
    quint16 m_expectedSequence{0};
    int m_lastOpusFrames{RemoteMicConfig::kOpusFrameSamples};
    // TX stall lane: the packet being submitted waited this long, in frames
    // (measured only).
    int m_heldFrames{0};
    // TX diagnostics lane: the packet being submitted's arrival timing, and
    // what it is measured from: the last receipt (us on m_clock, -1: none),
    // the RTP timestamp unwrapped to frames, and the last offset.
    RemoteMicFeed::ArrivalTiming m_timing;
    qint64 m_lastReceiptUs{-1};
    bool m_haveRtpTimestamp{false};
    quint32 m_lastRtpTimestamp{0};
    qint64 m_rtpFrames{0};
    qint64 m_lastRtpOffsetUs{0};
    std::vector<float> m_pcm;
    Stats m_stats;

    std::function<void(bool)> m_waitDone;
    quint64 m_waitGeneration{0};
    // Load findings 2: whether the waiting key's line has delivered its
    // first packet (its fill deadline then runs), and when the wait began.
    bool m_waitLineStarted{false};
    qint64 m_waitStartedMs{0};

    std::atomic<bool> m_feedWriter{true};
    // The event loop's.
    bool m_watching{false};
    bool m_starved{false};
    bool m_starvationCheckPending{false};
    quint64 m_starvationGeneration{0};
    // TX mic thread: when the line's audio last came, on m_clock, set by
    // the delivering thread at the packet's receipt there.
    std::atomic<qint64> m_lastAudioMs{0};
};

} // namespace NereusSDR
