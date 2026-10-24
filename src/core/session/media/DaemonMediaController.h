#pragma once
#include "core/session/media/RemoteIqIngress.h"
#include "core/session/media/WaveformCodec.h"
// =================================================================
// src/core/session/media/DaemonMediaController.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. Session-owned daemon display media
// coordination; it contains neither GUI nor radio control policy.
//
// Modification history (NereusSDR):
//   2026-10-23: American spelling in the keyframe every frame comment.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-22: NSDC v2 test seams (setEndpointMaximumFrameBytesForTest,
//               setEncodedFrameObserverForTest and the observer) build only
//               with NEREUS_BUILD_TESTS. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-22: NSDC v2 sender: a connection whose start agreed version 2
//               sends every display frame as NSDC v2, a split frame's two
//               parts in one sender turn, row part first; the diagnostics
//               count split frames and refused second parts. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-22: NSDC v2 negotiation: a start may declare
//               displayCodecVersion 2; displayCodecVersion() is the
//               version the connection agreed. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-22: Bench setting DisplayKeyframeEveryFrame (JJ approved
//               2026-10-22, option C of the waterfall loss study): every
//               display frame goes as a keyframe; the display diagnostics
//               count frames, keyframes and bytes sent. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-16: sendWaveformsOnStationThread: the RADE waveforms stay on
//               the station thread while the display thread sends the
//               displays. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-16: DaemonSharedSpectrum::keyEdge takes the display lock;
//               m_displayKeyed is written under it. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-16: readDisplayExtrasForTest. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-15: display send thread (JJ approved 2026-10-15): with a media
//               peer whose transport sends displays from any thread, a
//               controller's frames are taken in and its displays sent by
//               its DaemonSharedSpectrum's display thread, under that
//               spectrum's display lock; the main thread takes the lock
//               whenever it runs controller code. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-11: R-DISP-RETUNE (third wave): a start may declare
//               spectrumPictureVersion. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-10: R-AUD-SEND: receive audio is encoded and sent by a thread
//               of its own (DaemonAudioSendThread), through AudioSendRoute,
//               one per sender; nothing of it runs on this controller's
//               thread. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-01: TX diagnostics lane: unkeyEventLines, one line for each
//               placed microphone underrun, the first radio ran dry and
//               each catch-up burst. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-01: TX stall lane, fix round 1: only the keyer's controller
//               reports the unkey. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-01: TX mic thread (JJ approved): MicRoute, the microphone
//               line's packets delivered from the transport's own thread.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: TX stall lane: the unkey line follows MoxController (its
//               moxChanging and moxStateChanged), so it prints on every
//               unkey; TransmitModel::moxChanged only saw the
//               no-controller fallback. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-29: the direct media ladder: m_currentRouted and
//               m_replacementRouted. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29: iPhone app plan Task 23 (R-IOS-09, audioQualityVersion 1):
//               a device's own Opus bitrate. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 2: ownsSlice
//               split into controlsSlice, hearsSlice and seesSlice. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 (R-IOS-16): the media `replace`
//               operation: a second peer beside the current one, audio on
//               both across the move, displays on a keyframe, the old one
//               retired. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-27: at each unkey, one info line with the microphone line's
//               statistics and the transmit I/Q send path's counters
//               (R-IOS-13, R-R3-42). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 36 (R-IOS-13): the microphone line.
//               A start carrying remoteTxVersion gets it; its receiver
//               feeds RadioModel's remote microphone ring; keys wait on it
//               (RemoteKeying::MicUplink); VOX armed and starvation follow
//               the device. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-25 - iPhone app plan Task 76 (R-IOS-31; the several-devices
//                design, rulings 9.1 to 9.4): one controller per admitted
//                session (DaemonMediaHub), each bound to its session's
//                media epoch; a receiver's FFT shared between every
//                controller watching it (DaemonSharedSpectrum); each
//                device's audio from its own owner mix; displays and
//                receiver streams only for the device's own slices; the
//                PureSignal display only to its subscriber. J.J. Boyd
//                (KG4VCF), with AI-assisted implementation via Anthropic
//                Claude Code.
//   2026-09-26: Transmit group fix wave C2: each controller opens and
//               closes only its own device's line and writes the feed
//               only while it is the writer; the hub routes the keying's
//               view of the lines by device; VOX another device armed is
//               never this one's. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: Parity Task 28 (R-R3-49, A11, R-IOS-13): the transmit
//               display. A media peer that declared txDisplayVersion gets
//               the transmit analyzer's view, from RadioModel's
//               TxDisplayFeed, on each endpoint of the transmitting pan
//               while the Core is keyed. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Parity Tasks 27-29 fix wave (R-R3-49): the transmitting
//               pan is the transmit slice's pan recorded at the keyed rise
//               and kept until the fall, as the window records it, so a
//               slice or binding that moves while keyed cannot move the
//               transmit display to another pan. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: Parity Task 31 (A11, R-R3-49): display duplex. A peer that
//               declared txDisplayVersion 3 may add `duplex` to a
//               subscribe; such an endpoint is no viewer of the transmit
//               display and keeps its receive frames while keyed, and its
//               device's DUP reaches RadioModel for noise blanking. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27: Parity Task 32 (R-IOS-13, R-R3-49): the transmit monitor.
//               A peer that declares txMonitorAudioVersion sends
//               monitor-audio; while its device holds transmit and MON is
//               on, MON rides in its main or headphones stream. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 6: the owner mix also sums the
//               slices this device listens to, at its own listen level.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX mic thread fix round 2: the unkey line carries the
//               over's longest "tx" keepalive wait. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-01: Control logging lane: the media connection's
//               selected pair at selection and on change, and its rtt in
//               the periodic display diagnostics line. Logging only. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-15: txReceiptHook(epoch) for the remote
//               transmit cut guard. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/NoiseFloorEstimator.h"
#include "core/RadioConnection.h"
#include "core/session/media/DaemonAudioSender.h"
#include "core/session/media/DaemonAudioSendThread.h"
#include "core/session/media/DaemonDisplaySendThread.h"
#include "core/session/media/DaemonSpectrumSource.h"
#include "core/session/media/DisplayBudget.h"
#include "core/session/media/DisplayCodec.h"
#include "core/session/media/DisplayExtras.h"
#include "core/session/media/MediaPeer.h"
#include "core/session/media/RemoteAudioContext.h"
#include "core/session/media/RemoteMicReceiver.h"
#include "core/session/media/SpectrumEndpoint.h"
#include "core/spectrum/WidebandSpectrumCache.h"
#include "core/session/RemoteKeying.h"

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QMap>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <map>
#include <mutex>
#include <optional>

namespace NereusSDR {

class RadioModel;
class SliceModel;
class StationServer;
class DaemonMediaController;
class TxDisplayFeed;
enum class ConnectionState;

/// iPhone app Task 76 (ruling 9.1): the spectrum engines every media
/// controller on the Core draws from, so a receiver's FFT is shared between
/// everyone watching it. Each engine runs at the largest size and the
/// highest rate any controller's endpoints on it were granted; each frame
/// is taken once and handed to every controller. Lives on the station
/// thread with its controllers.
///
/// Display send thread (2026-10-15): while any of its controllers sends
/// from it (DaemonMediaController::syncDisplayThread), the frames are taken
/// in and the displays sent by this spectrum's display thread, not the
/// station thread. Every controller's state is then under displayLock().
class DaemonSharedSpectrum final : public QObject {
    Q_OBJECT
public:
    struct SourceRuntime {
        DaemonSpectrumSourceConfig config;
        bool configured{false};
    };

    explicit DaemonSharedSpectrum(RadioModel* radioModel, QObject* parent = nullptr);
    ~DaemonSharedSpectrum() override;

    DaemonSpectrumSource& source() { return m_source; }
    const DaemonSpectrumSource& source() const { return m_source; }
    QMap<MediaSourceKey, SourceRuntime>& runtimes() { return m_runtimes; }
    const QMap<MediaSourceKey, SourceRuntime>& runtimes() const { return m_runtimes; }
    /// The controllers drawing from these engines, this one included.
    QList<DaemonMediaController*> members() const;
    void join(DaemonMediaController* controller);
    void leave(DaemonMediaController* controller);
    /// A MOX edge, told by every controller that hears it: the engines'
    /// FFT input is cleared once for the edge
    /// (DaemonSpectrumSource::resetInputHistoryAll), however many
    /// controllers share them. Takes the display lock.
    void keyEdge(bool keyed);

    /// The lock over every member's display state (DaemonDisplayLock).
    const std::shared_ptr<DaemonDisplayLock>& displayLock() const { return m_displayLock; }
    /// Whether the calling thread is this spectrum's display thread.
    bool onDisplayThread() const { return m_displayThread.onThisThread(); }
    /// The display thread runs one turn of that member's sender soon, as
    /// its timer would. Any thread.
    void askDisplayTurn(DaemonMediaController* member);
    /// Starts the display thread, or idles it, after a member moved onto it
    /// or off it. The station thread, under the display lock.
    void syncDisplayThread();
    /// Drops the frames every member holds for the station thread from
    /// `key`, whose engine was just stopped: one started again under the
    /// same key counts its generations from one again. Under the display
    /// lock.
    void forgetHeldFrames(const MediaSourceKey& key);

private:
    void onFrameAvailable(MediaSourceKey key);
    /// The station thread: `key`'s waiting frame, taken once and handed
    /// whole to every member (onSourceFrame). Under the display lock.
    void takeInOnStationThread(const MediaSourceKey& key);
    /// The display thread: every frame waiting in the engines, taken once
    /// and handed to every member. Under the display lock.
    void takeInOnDisplayThread();
    /// One pass of the display thread (DaemonDisplaySendThread::Pass).
    void displayPass(bool timedTurn, bool askedTurn);
    /// The release hook: each member on the display thread copies the
    /// station thread's values its turns read.
    void refreshMemberSnapshots();

    DaemonSpectrumSource m_source;
    QMap<MediaSourceKey, SourceRuntime> m_runtimes;
    /// Written under the display lock; the display thread reads it there.
    QList<QPointer<DaemonMediaController>> m_members;
    // The last edge the engines were cleared for; empty before the first.
    // Written under the display lock.
    std::optional<bool> m_lastKeyEdge;
    const std::shared_ptr<DaemonDisplayLock> m_displayLock{
        std::make_shared<DaemonDisplayLock>()};
    /// Whether any member sends from the display thread (station thread).
    bool m_displayThreadUsed{false};
    DaemonDisplaySendThread m_displayThread;
};

/// Read-only diagnostics for the most recent active daemon audio context.
/// A successful send means the media transport accepted the RTP packet; it is
/// not evidence of network delivery. For every context,
/// attempts = accepted + rejected + inFlight + unresolvedAtRetirement.
/// Unresolved sends were interrupted by retirement and are not packet loss.
///
/// R-AUD-SEND: the sends are made by the sender thread and counted there
/// (AudioSendRoute). An attempt is a packet a ready transport was asked to
/// take; rejected is that transport's own refusal, a transport fault to a
/// reader. A packet that met a transport which was not ready (stopping,
/// stopped, gone) is not an attempt and is counted nowhere, as when the
/// controller's thread sent. inFlight is the sends that thread had begun
/// and whose transport had not yet answered when this was read: 0 or 1 in a
/// periodic reading.
struct DaemonAudioDiagnostics {
    quint32 contextGeneration = 0;
    quint32 revision = 0;
    bool activeContext = false;
    qint64 elapsedMs = 0;
    DaemonAudioSenderTelemetry sender;
    std::uint64_t sendAttempts = 0;
    std::uint64_t sendAccepted = 0;
    std::uint64_t sendRejected = 0;
    std::uint64_t sendInFlight = 0;
    /// R-AUD-SEND: always 0 now. It counted sends the retirement of a
    /// context cut short on the controller's thread; the context is now
    /// retired only after the sender's stop(), which returns once the send
    /// under way has its answer. Kept so the log line and readers keep
    /// their shape.
    std::uint64_t sendUnresolvedAtRetirement = 0;
};

/// Real display traffic for the current media peer (R-R3-03, R-R3-05).
/// Sizes are spectrum frames the transport took, including one the library
/// queued; fragments are SCTP DATA chunks, ceil(bytes /
/// IMediaTransport::kSctpDataPayloadBytes). A refusal is a display message
/// (spectrum frame or PureSignal chunk) offered to the transport and not
/// taken: a refused spectrum frame is dropped, a PureSignal chunk refused
/// only because the channel was busy is offered again, never having been
/// sent. queuedLate counts messages the library took but held until SCTP
/// had room; each is still sent once. Transport errors are errors on the
/// display channel only. Every count starts again with each new media peer.
struct DaemonDisplayDiagnostics {
    quint32 displayMaxKeyframeBytes = 0;
    quint32 displayMaxDeltaBytes = 0;
    quint32 displayMaxFragments = 0;
    quint64 displaySendRefusals = 0;
    quint64 displayTransportErrors = 0;
    quint64 displayQueuedLate = 0;
    /// R-R3-21: keyframes the window asked for (after a lost display
    /// message), and those refused by the five-a-second limit.
    quint64 displayKeyframeRequests = 0;
    quint64 displayKeyframeRequestsRefused = 0;
    /// Spectrum frames (receive and transmit displays) the transport took,
    /// those of them that were keyframes, and their bytes all together:
    /// the line's mean frame size is displayBytesSent / displayFramesSent.
    quint64 displayFramesSent = 0;
    quint64 displayKeyframesSent = 0;
    quint64 displayBytesSent = 0;
    /// Whether the bench setting DisplayKeyframeEveryFrame was on for this
    /// media peer (every display frame sent as a keyframe).
    bool displayKeyframeEveryFrame = false;
    /// Frames a newer frame replaced before they were sent, by where they
    /// waited; each place holds one frame. At the source: frames an FFT
    /// engine finished for a source one of this peer's pans is sending
    /// from, which the Core's main thread had not taken when the next one
    /// came. One per source frame, whatever the number of pans on it. At a
    /// pan: frames a pan held and would have sent, which no sender turn
    /// had taken when the next one came (no turn ran, the turn went to
    /// another pan, the channel was busy, or the display budget had no
    /// room). One per pan. A frame the pan's own pacing would have dropped
    /// is not counted.
    quint64 displaySourceFramesReplaced = 0;
    quint64 displayPanFramesReplaced = 0;
    /// The longest time between two turns of the display sender's timer
    /// while this peer was ready, in microseconds; -1 before two turns.
    /// The turns are kDisplaySenderIntervalMs apart when the Core's main
    /// thread is free, and a turn sends one display message at most, except
    /// that an NSDC v2 split frame sends its two parts in one turn.
    qint64 displaySenderGapMaxUs = -1;
    /// NSDC v2 only: frames sent in two parts (their row part taken), and
    /// second parts (trace parts) the transport did not take after it took
    /// the row part. On v2, displayFramesSent counts frames, not messages,
    /// displayBytesSent the bytes of every message taken, and
    /// displayMaxDeltaBytes the largest single message; no keyframes.
    quint64 displaySplitFrames = 0;
    quint64 displaySecondPartRefusals = 0;

    bool operator==(const DaemonDisplayDiagnostics&) const = default;
};

/// The periodic journal line for these diagnostics, e.g.
/// "largestKeyframe=2977 bytes/4 fragments largestDelta=... ".
QString daemonDisplayDiagnosticsLine(const DaemonDisplayDiagnostics& diagnostics);

/// Bench setting (JJ approved 2026-10-22, option C of the waterfall loss
/// study): with "True" in the Core's own AppSettings, every frame of every
/// display stream goes as a keyframe (absolute planes, NSDC v1 unchanged),
/// so each row a window receives decodes alone. Default off. Read once
/// when the Core starts its station host (StationHost::start); a Core-only
/// key the settings proxy never carries (SettingsScope OperatorLocal).
inline constexpr char kDisplayKeyframeEveryFrameSettingsKey[] = "DisplayKeyframeEveryFrame";

/// Owns one authenticated daemon media session: strict control validation,
/// actual RadioModel I/Q to bounded source, per-endpoint reduction/codec and
/// one-at-a-time media sends. The caller owns StationServer and RadioModel.
class DaemonMediaController final : public QObject {
    Q_OBJECT
public:
    /// iPhone app plan Task 29 (R-IOS-16; the remote media control
    /// document, "Replacing the media connection"): after a replacement
    /// peer is ready, how long every audio stream goes out on both peers
    /// before the old one stops sending. NereusSDR's own bound: long enough
    /// for the app's new peer to be carrying audio (its DTLS done a round
    /// trip after the Core's) over any path's delay, short enough that the
    /// doubled audio costs little.
    static constexpr int kReplaceOverlapMs = 1000;
    /// How long the old peer still takes microphone packets and "tx"
    /// messages after that, for what the app sent it before it heard the
    /// replacement was done (at least one trip over the slower path).
    static constexpr int kReplaceDrainMs = 2000;
    /// The refusal of a replacement while the radio is on the air (MOX not
    /// idle); a device tries again once it is back on receive.
    static constexpr const char* kReplaceTransmittingReason =
        "The Core did not move audio and display: the radio is transmitting.";
    /// Monotonic nanoseconds, never negative. Besides display pacing it is
    /// the Core's audio clock (R-R3-35): clock-echo times and the capture
    /// times of audio blocks, which the DSP thread reads, so an injected
    /// clock must be safe to call from any thread. Without one it is the
    /// producer clock (DaemonSpectrumSource::monotonicNowNs), the one
    /// display frames are stamped from (R-R3-21, displayClockVersion 1).
    using MonotonicClock = std::function<qint64()>;
    /// A controller on its own: it serves one media session at a time (the
    /// first that starts while it has none live), owns its spectrum engines
    /// and installs the Core's PureSignal display gate itself. Tests and a
    /// Core with a single controller use it.
    explicit DaemonMediaController(StationServer* server, RadioModel* radioModel,
                                   QObject* parent = nullptr,
                                   MediaPeer::TransportFactory peerFactory = {},
                                   MonotonicClock monotonicClock = {});
    /// iPhone app Task 76: a controller for the one media session `epoch`
    /// names, drawing from `spectrum` with the others (DaemonMediaHub makes
    /// these). It starts at once when that session's media is available and
    /// never serves another session.
    DaemonMediaController(StationServer* server, RadioModel* radioModel,
                          quint64 boundEpoch,
                          std::shared_ptr<DaemonSharedSpectrum> spectrum,
                          QObject* parent = nullptr,
                          MediaPeer::TransportFactory peerFactory = {},
                          MonotonicClock monotonicClock = {});
    ~DaemonMediaController() override;

    /// The media session this controller serves now (0 with none).
    quint64 sessionEpoch() const noexcept { return m_epoch; }
    /// The spectrum engines this controller draws from (Task 76: shared
    /// with every other controller of the same hub).
    DaemonSharedSpectrum* sharedSpectrum() const noexcept { return m_shared.get(); }
    /// Task 76: the owner mix of AudioEngine this controller's session
    /// hears (-1 while none is held), and its slices.
    int ownerMixSlot() const noexcept { return m_ownerMix; }
    /// Task 76: this controller's own display charge (its spectrum
    /// endpoints, plus the PureSignal display when its session is the
    /// subscriber). DaemonMediaHub sums them for the governor.
    DisplayBudgetCharge ownDisplayCharge() const;
    /// Fix wave I5 (ruling 9.3): this controller's display demand, the
    /// charges its displays asked for as subscribed (at the requested
    /// frame rate and the requested pixels clamped to the source bins its
    /// window can carry, fix wave 3; before the budget clamps them), a
    /// display refused for the budget included, until it is closed. The
    /// PureSignal display is not in it (the split charges it to its
    /// subscriber).
    DisplayBudgetCharge displayDemand() const;
    /// Fix wave 2 after the several-devices re-review (Important 2): how
    /// long a subscription refused for the budget still counts in this
    /// device's request once the refusal is sent, unless the client asks
    /// for that endpoint again (subscribe or unsubscribe) first. The
    /// refusal follows the capabilities carrying the share its request
    /// produced (the budget generation it was sent); a client that still
    /// wants the display re-plans at once (the desktop's planner runs on
    /// every capabilities change and every 100 ms) and subscribes again
    /// inside its share, replacing the refused request. The hold is the
    /// app's own allocation acknowledgement timeout
    /// (kDisplayAllocationAckTimeoutMs, 10 s): the longest the app waits
    /// for the Core's answer on a slow link, so a client's re-ask within
    /// the same round trip is never missed, while a display the client
    /// dropped without saying so stops cutting the other devices then.
    static constexpr int kRefusedDisplayDemandHoldMs = kDisplayAllocationAckTimeoutMs;
    /// Tests only: a shorter hold. Applies to refusals from now on.
    void setRefusedDisplayDemandHoldMsForTest(int ms) { m_refusedDemandHoldMs = ms; }
    /// Task 76 (ruling 9.3 item 4): the PureSignal display goes to this
    /// controller's session.
    bool ps3DisplayHere() const;
    /// Task 76: the PureSignal display gate for this controller's session.
    bool admitPs3DisplayForSession(bool enabled, QString* refusal)
    {
        return admitPs3Display(enabled, refusal);
    }

    /// Small read-only lifecycle telemetry for daemon diagnostics and core
    /// integration tests. Endpoint internals remain session-private.
    int activeEndpointCount() const;
    int activeSourceCount() const;
    /// Test hook (slice control plan Task 4): every audio sender this
    /// session runs now (the speakers' mix, the headphones mix, each
    /// receiver stream's), so a test can see none was remade.
    QList<const DaemonAudioSender*> audioSendersForTest() const;
#ifdef NEREUS_BUILD_TESTS
    /// Test seam, the thread that feeds the audio engine only, no
    /// production caller: every sender's capture bridge hands over a block
    /// a busy sender thread kept it from queueing, as the DSP thread's next
    /// callback does (DaemonAudioSource::handOverPendingForTest). A test
    /// that feeds a fixed number of blocks calls it after the last.
    void handOverPendingAudioForTest();
#endif
    DaemonAudioDiagnostics audioDiagnostics() const;
    /// The Opus target, bit/s, for the speakers' mix and the headphones mix
    /// this controller sends (R-R3-23: nereusd's audio_bitrate). Applies to
    /// the next media peer and audio sender it creates, so DaemonApp sets it
    /// before the listener opens. Default is the encoder's own default
    /// target, 48 kbit/s fullband (R-R3-21). Receiver streams do not follow it: see
    /// kReceiverAudioOpusBitrate.
    void setAudioTargetBitrate(int bitsPerSecond);
    /// R-R3-43, R-R3-44: the Opus target, bit/s, of every receiver stream
    /// (the audio VAX and TCI apps decode) whenever it is compressed: when
    /// Opus is the choice and when lossless is refused or falls back. The
    /// 48 kbit/s fullband profile (bandwidthForBitrate()), whatever
    /// audio_bitrate says: the operator's decision of 2026-09-24 after the
    /// FT8 measurement in docs/architecture/2026-09-20-remote-daemon-r3-
    /// verification/digital-modes-over-opus.md (confirming run: of the 177
    /// files the untouched audio decoded, 24 kbit/s lost 15 and 48 kbit/s
    /// lost 4; each also decoded 2 files the untouched audio missed, so
    /// 164 and 175 decodes in all, net 13 and 2 fewer). The receiver
    /// context's encoder object reports it, so a window never assumes it.
    static constexpr int kReceiverAudioOpusBitrate = 48'000;
    int audioTargetBitrate() const noexcept { return m_audioTargetBitrate; }
    /// iPhone app plan Task 23: the Opus bitrate the main audio stream is
    /// coded at for this device: its own `opusBitrate` when it asked for a
    /// measured one, else audioTargetBitrate().
    int audioStreamBitrate() const noexcept
    {
        return m_audioRequestedBitrate > 0 ? m_audioRequestedBitrate : m_audioTargetBitrate;
    }
    /// R-R3-23: whether a GUI may switch audio to the lossless profile
    /// (nereusd.conf audio_lossless; default allow). With false a request
    /// is refused as lossless-not-allowed and Opus keeps running, and no
    /// media offer carries the lossless format. Applies to the next media
    /// peer and request.
    void setAudioLosslessAllowed(bool allowed) { m_audioLosslessAllowed = allowed; }
    bool audioLosslessAllowed() const noexcept { return m_audioLosslessAllowed; }
    /// Bench setting kDisplayKeyframeEveryFrameSettingsKey: every display
    /// frame (receive and transmit) is encoded as a keyframe. Takes effect
    /// on the next frame encoded. A keyframe request from the window is
    /// still counted and honored; it never sends a second frame. An NSDC
    /// v2 connection has no keyframes, so it changes nothing there.
    void setDisplayKeyframeEveryFrame(bool on);
    bool displayKeyframeEveryFrame() const noexcept { return m_displayKeyframeEveryFrame.load(); }
    /// Display codec v2 (NSDC v2): the display codec version this
    /// connection agreed in its media start: 2 when the start carried
    /// displayCodecVersion 2, else 1. A replace keeps it; only the next
    /// start sets it again.
    quint32 displayCodecVersion() const noexcept;
    /// The profile the Core's audio runs for the current peer (R-R3-23).
    RemoteAudioProfile audioProfile() const noexcept { return m_audioActiveProfile; }
    /// R-R3-43: receiver streams sending now (enabled receiver contexts).
    int activeReceiverAudioStreamCount() const;
    /// R-R3-43: the profile slice `sliceId`'s receiver stream runs, or empty
    /// while that stream is not sending.
    std::optional<RemoteAudioProfile> receiverAudioProfile(int sliceId) const;
    /// R-R3-45: the headphones mix is sending now (an enabled headphones
    /// context), and the profile it runs; empty while it is not sending.
    bool headphonesMixSending() const;
    std::optional<RemoteAudioProfile> headphonesMixProfile() const;
    /// Parity Task 32: where this device's owner mix carries the transmit
    /// monitor now (none unless the device declared txMonitorAudioVersion,
    /// asked for a route, holds transmit and MON is on).
    TxMonitorRoute txMonitorRoute() const noexcept { return m_monitorApplied; }
    DaemonDisplayDiagnostics displayDiagnostics() const;
    /// Display send thread: whether this controller's displays leave on its
    /// hub's display thread now (its peer is ready and gives a display sink)
    /// rather than on its own thread.
    bool displaysSentFromThread() const;
    /// R-R3-21: the Core's media clock now (displayNowNs): the clock of
    /// clock-echo times and audio capture, and, without an injected clock,
    /// the producer clock display frames are stamped from.
    qint64 mediaClockNowNs() const { return displayNowNs(); }
    /// What Core granted a live spectrum endpoint: FFT size and tier after
    /// the largest-size and shared-engine rules, and pixels after the source
    /// bin rule (R-R3-01, R-R3-08). Empty for an unknown endpoint.
    std::optional<SpectrumGrant> spectrumGrant(quint32 endpointId) const;
    /// The frame rate Core configured on the engine that feeds a live
    /// spectrum endpoint (R-R3-01, R-R3-08). Empty for an unknown endpoint.
    std::optional<int> spectrumSourceFps(quint32 endpointId) const;
    /// iPhone app follow-up (R-IOS-27): the averaging constants/depths a live
    /// spectrum endpoint's trace and waterfall planes run with, after
    /// display extras' averageTimeMs and waterfallAverageTimeMs. Empty for
    /// an unknown endpoint.
    struct SpectrumAveraging {
        double traceAlpha{0.0};
        double waterfallAlpha{0.0};
        int traceWindowFrames{SpectrumAvenger::kMinWindowFrames};
        int waterfallWindowFrames{SpectrumAvenger::kMinWindowFrames};
    };
    std::optional<SpectrumAveraging> spectrumAveraging(quint32 endpointId) const;
    /// R-IOS-27, R-IOS-06: a live endpoint's display extras, for tests that
    /// drive its computations directly (nullptr for an unknown endpoint or
    /// one that asked for none).
    DisplayExtrasProcessor* displayExtrasForTest(quint32 endpointId);
    /// Runs read on that endpoint's display extras under the display lock,
    /// so the display send thread does not run them meanwhile. False, and
    /// read is not run, for an unknown endpoint or one that asked for none.
    bool readDisplayExtrasForTest(quint32 endpointId,
                                  const std::function<void(const DisplayExtrasProcessor&)>& read);
#ifdef NEREUS_BUILD_TESTS
    /// NSDC v2 tests: sets a live endpoint's admitted frame bytes (its
    /// displayCost.maximumFrameBytes) under the display lock. False for an
    /// unknown endpoint.
    bool setEndpointMaximumFrameBytesForTest(quint32 endpointId, quint32 bytes);
    /// NSDC v2 tests: called with every display frame just before it is
    /// encoded (either version), under the display lock, on the thread that
    /// sends the displays. Empty clears it.
    void setEncodedFrameObserverForTest(std::function<void(const DisplayCodecFrame&)> observer);
#endif
    /// Whether that engine's transforms follow its frame rate: true only
    /// while the display budget is lowered because the Core is busy
    /// (R-R3-08, R-R3-40). Empty for an unknown endpoint.
    std::optional<bool> spectrumSourceTransformsFollowFrameRate(quint32 endpointId) const;
    /// Parity Task 17 (R-R3-01): the decimation that endpoint's engine runs
    /// at (FFTEngine::decimation). Empty for an unknown endpoint or one
    /// whose engine has not started.
    std::optional<int> spectrumSourceDecimation(quint32 endpointId) const;
    /// Display traffic accepted now: every live spectrum endpoint's charge
    /// plus PureSignal's display while it is subscribed (R-R3-08, R-R3-37).
    /// What the display load governor scales when the Core is busy.
    DisplayBudgetCharge acceptedDisplayCharge() const;

    /// iPhone app plan Task 36 (R-IOS-13): the media connection's microphone
    /// line receiver, while its start carried remoteTxVersion; null
    /// otherwise. Its starved(bool) is the Core's starvation signal.
    /// Parity Task 28 (R-R3-49, A11): whether this session's media start
    /// declared txDisplayVersion, and whether an endpoint is showing the
    /// transmit display now (a viewer of RadioModel's TxDisplayFeed).
    bool txDisplayNegotiated() const noexcept { return m_txDisplayNegotiated; }
    bool transmitDisplayActive(quint32 endpointId) const;
    /// Parity Task 31 (A11): whether an endpoint's subscribe carried
    /// `duplex` true, and the device this controller told RadioModel has
    /// DUP on (empty for none).
    bool endpointDuplex(quint32 endpointId) const;
    QByteArray displayDuplexDevice() const { return m_duplexDevice; }
    /// The calibration the endpoint's last receive frame was given
    /// (RadioModel::rxMeterOffsetDb, or keyedDisplayOffsetDb(true) for a
    /// `duplex` endpoint on the transmitting pan while keyed).
    double endpointDisplayOffsetDb(quint32 endpointId) const;

    RemoteMicReceiver* micReceiver() const { return m_micReceiver.get(); }
    /// R-IOS-13, R-R3-42: the text of the line logged at each unkey (log
    /// only, never shown to a device). `feed` is null when the Core has no
    /// remote microphone feed. `keepaliveWaitMaxUs` is the over's longest
    /// wait at the Core of a "tx" keepalive, or -1 when none came.
    static QString unkeyStatsLine(const QByteArray& deviceId, const RemoteMicReceiver::Stats& rx,
                                  const RemoteMicFeed::Stats* feed,
                                  const RadioConnection::TxSendStats& send,
                                  qint64 keepaliveWaitMaxUs = -1);
    /// TX diagnostics lane: the lines logged after it, one for each placed
    /// microphone underrun (RemoteMicFeed::Stats::underrunsPlaced), the
    /// first time the radio ran dry and each placed catch-up burst
    /// (RadioConnection::TxSendStats), at most 9. Log only.
    static QStringList unkeyEventLines(const QByteArray& deviceId,
                                       const RemoteMicFeed::Stats* feed,
                                       const RadioConnection::TxSendStats& send);
    /// Task 36: the keying's view of the line (RemoteKeying::setMicUplink;
    /// a controller on its own installs it on the Core's RemoteKeying, and
    /// DaemonMediaHub installs one that routes by device, fix wave C2).
    RemoteKeying::MicUplink micUplink();
    /// Fix wave C2: the device this controller's microphone line is for
    /// (empty without a line), whether it carries the line now, and its
    /// key's wait on the line's buffer.
    QByteArray micDeviceId() const { return m_micDeviceId; }
    bool carriesMicFor(const QByteArray& deviceId) const;
    void primeMic(std::function<void(bool)> done);
    void endMicPriming();

private:
    struct EndpointEntry;
    struct AllocationRecord {
        QJsonObject request;
        quint32 revision{0};
        bool accepted{false};
        bool explicitlyRetired{false};
        QString reason;
    };
    using SourceRuntime = DaemonSharedSpectrum::SourceRuntime;
    /// R-R3-43: one slice's receiver audio request and, while it holds a
    /// receiver stream id, the sender capturing that slice.
public:
    struct AudioSendRoute;

private:
    struct ReceiverAudioStream {
        quint32 revision{0};
        bool desiredEnabled{false};
        RemoteAudioProfile requestedProfile{RemoteAudioProfile::Opus};
        RemoteAudioProfile activeProfile{RemoteAudioProfile::Opus};
        std::optional<RemoteAudioProfileRefusal> profileRefusal;
        /// Index into MediaPeer::receiverAudioSsrcs(), or -1 with none.
        int streamIndex{-1};
        bool sending{false};
        std::unique_ptr<DaemonAudioSender> sender;
        /// R-AUD-SEND: where `sender`'s packets go; made and dropped with it.
        std::shared_ptr<AudioSendRoute> route;
    };
    using WaveformKey = std::pair<int, WaveformKind>;
    struct WaveformStream {
        bool wanted = false;
        bool sending = false;
        quint32 revision = 0;
        quint64 sourceGeneration = 0;
        quint64 deliveryGeneration = 0;
        quint64 packetSerial = 0;
        quint64 droppedFrames = 0;
        int sampleRate = 0;
        WaveformBatch pending;
        int segmentIndex = 0;
        int segmentOffset = 0;
        QString reason;
    };
    bool handleWaveformStream(const QJsonObject& control);
    bool waveformAuthorized(const WaveformKey& key) const;
    void reconcileWaveforms();
    void clearWaveforms(bool notify = true, const QString& reason = {}, qint64 retryAtNs = -1);
    void sendWaveformContext(const WaveformKey& key, const WaveformStream& stream);
    void acceptWaveformBatch(int sliceId, WaveformKind kind, quint64 source, WaveformBatchPtr batch);
    bool trySendWaveform(MediaPeer* peer, quint64 epoch, qint64 nowNs);
    quint64 waveformBytesPerSecond() const;
    static quint64 waveformCharge(int rate);
    bool m_waveformNegotiated = false;
    bool m_waveformFailed = false;
    qint64 m_waveformRetryAtNs = -1;
    bool m_reconcilingWaveforms = false;
    quint64 m_nextWaveformGeneration = 0;
    std::map<WaveformKey, WaveformStream> m_waveformStreams;
    QByteArray m_waveformPacket;
    WaveformKey m_waveformPacketKey{-1, WaveformKind::RawMicrophone};
    int m_waveformPacketFrames = 0;
    bool m_waveformPacketDebited = false;
    qint64 m_waveformBlockedAtNs = -1;
    struct IqStream {
        quint32 revision = 0;
        quint32 generation = 0;
        quint32 sequence = 0;
        bool desired = false;
        bool sending = false;
        int streamIndex = -1;
        int sampleRate = 0;
        quint64 bytesPerSecond = 0;
        std::shared_ptr<RemoteIqIngress> ingress;
        QMetaObject::Connection tap;
        QByteArray pending;
        bool pendingDebited = false;
        qint64 blockedAtNs = -1;
        bool budgetRefused = false;
        quint64 lastBudgetRefusalRevision = 0;
        /// Display send thread: the end the display thread found for this
        /// stream, which the station thread carries out (failIqStream).
        struct Failure {
            QString reason;
            bool budgetRefused = false;
        };
        std::optional<Failure> failOnMain;
    };
    /// R-R3-45: the headphones mix for a GUI that declared
    /// headphonesMixVersion: its latest request and, while it runs, the
    /// sender capturing AudioEngine's headphones-mix tap. The RTP timeline
    /// of the one headphones stream id continues across contexts.
    struct HeadphonesAudioStream {
        quint32 revision{0};
        int encoderBitrate{0};
        bool desiredEnabled{false};
        RemoteAudioProfile requestedProfile{RemoteAudioProfile::Opus};
        RemoteAudioProfile activeProfile{RemoteAudioProfile::Opus};
        std::optional<RemoteAudioProfileRefusal> profileRefusal;
        bool sending{false};
        /// The reason in the last context sent, or empty after an enabled
        /// one (fix wave: a radio drop says only what changed).
        std::optional<RemoteAudioOffReason> lastOffReason;
        quint16 nextSequence{1};
        quint32 nextTimestamp{0};
        std::unique_ptr<DaemonAudioSender> sender;
        /// R-AUD-SEND: where `sender`'s packets go; made and dropped with it.
        std::shared_ptr<AudioSendRoute> route;
    };
public:
    /// R-AUD-SEND: one sender's way out. Public only so its test can drive
    /// it; see m_audioRoute below for the contract.
    struct AudioSendRoute {
        /// Guards every member but the counters. Taken by deliver() only to
        /// copy out where one packet goes, never across a send, and by the
        /// controller's thread only to copy new values in (never while it
        /// holds another lock). So neither thread waits here for a send.
        std::mutex mutex;
        std::shared_ptr<MediaRtpSink> current;
        QHash<quint32, quint32> rewrite;
        /// While a replacement is ready: its sink, and the two peers' audio
        /// SSRCs in stream order (audioSsrcsOf).
        std::shared_ptr<MediaRtpSink> replacement;
        QList<quint32> replacementFrom;
        QList<quint32> replacementTo;
        /// Sends to the current peer since resetCounters(): written by
        /// deliver() on the sender thread, read without the lock. attempts is
        /// raised before the transport is asked and one of the others after
        /// it answers, so a reader takes accepted and rejected first. A
        /// transport that answers "not ready" was asked nothing: the attempt
        /// is taken back and none of the three counts it.
        std::atomic<std::uint64_t> attempts{0};
        std::atomic<std::uint64_t> accepted{0};
        std::atomic<std::uint64_t> rejected{0};

        /// The sender thread. Sends `packet` to the current peer with its
        /// SSRC as that peer's, and during a replacement to the new peer
        /// too, with the same sequence number and timestamp and the new
        /// peer's SSRC. Nothing, and nothing counted, while closed.
        void deliver(const QByteArray& packet);
        /// The controller's thread, while the route's sender is stopped.
        void resetCounters();
        /// The controller's thread: nothing is sent from here on.
        void close();
    };

private:
    struct AdmittedAudioProfile {
        RemoteAudioProfile active{RemoteAudioProfile::Opus};
        std::optional<RemoteAudioProfileRefusal> refusal;
    };

    void wireUp();
    void onSessionStarted(quint64 epoch);
    void onSessionEnded(quint64 epoch);
    void onControl(const QJsonObject& control, quint64 epoch);
    friend class DaemonSharedSpectrum;
    /// One frame of a shared engine, taken once by DaemonSharedSpectrum.
    void onSourceFrame(const DaemonSpectrumFrame& frame);
    /// Task 76: the owner mix follows the session's own slices.
    void acquireOwnerMix();
    void releaseOwnerMix();
    void refreshOwnerMixMask();
    /// Slice control plan Task 2 (SliceAccessPolicy): whether this
    /// session's device controls, may hear, or may see `sliceId`.
    bool controlsSlice(int sliceId) const;
    bool hearsSlice(int sliceId) const;
    bool seesSlice(int sliceId) const;
    /// Every endpoint on `key` of every controller sharing the engines.
    template <typename Fn>
    void forEachSharedEndpoint(const MediaSourceKey& key, Fn&& fn);
    bool anySharedEndpointOn(const MediaSourceKey& key) const;
    void onWidebandSourceChanged(int adc);
    bool reconcileWidebandDemand(EndpointEntry& entry);
    std::optional<WidebandDisplayContext> widebandContext(const EndpointEntry& entry) const;
    void onSendTick();
    /// A turn of the display sender's timer: notes the time since the
    /// turn before for the display diagnostics, then onSendTick(). With
    /// the display thread sending, sendWaveformsOnStationThread() only.
    void onSendTimerTurn();
    /// The waveform streams' part of a sender turn (a due retry, then up
    /// to 16 messages), on the station thread while the display thread
    /// sends the displays: the waveform channel sends only on its owner's
    /// thread.
    void sendWaveformsOnStationThread();
    /// Whether the frame `entry` holds is one a sender turn would send:
    /// its context is out and its own pacing would take the frame.
    bool heldFrameWouldBeSent(const EndpointEntry& entry) const;

    // ── Display send thread (2026-10-15) ────────────────────────────────
    // While this controller's current media peer is ready and its transport
    // sends displays from any thread (MediaPeer::displaySink), the frames
    // are taken in and the displays sent by the shared spectrum's display
    // thread, never this controller's own. Every piece of this controller
    // the display thread reads or writes is then under the display lock:
    // the station thread takes it at every entry into controller code
    // (displayGuard), the display thread for each pass. What only the
    // station thread may do (a control message, a configure, ending a raw
    // I/Q stream, reading RadioModel or StationServer) the display thread
    // leaves to it as one queued call, reading the station thread's values
    // from a copy (DisplaySnapshot). The receive "tx" keepalives and the
    // microphone line take no lock: they touch nothing the display thread
    // does. The transmit display's keyed edge does take it:
    // MoxController::moxStateChanged reaches recordTransmitPan directly,
    // so a key or unkey on the station thread can wait out one display
    // pass, as it waited out a send turn on that thread before. So does
    // the display extras' key edge, whose copy the lock's release
    // refreshes. The RADE waveforms stay on the station thread, under the
    // lock (sendWaveformsOnStationThread).
    /// Holds the display lock for a scope.
    DaemonDisplayLockGuard displayGuard() const { return DaemonDisplayLockGuard(m_displayLock); }
    bool onDisplayThread() const { return m_shared->onDisplayThread(); }
    /// Logs once for each place that runs without the display lock, which
    /// only an entry missing its displayGuard() does, and, when
    /// `stationThreadOnly`, for each that runs on the display thread.
    void warnIfDisplayLockNotHeld(const char* where, bool stationThreadOnly) const;
    /// Moves this controller onto the display thread or off it, wherever
    /// the current peer changes or becomes ready (syncAudioRoutes).
    void syncDisplayThread();
    /// The station thread, while on the display thread: copies the values
    /// the display thread reads. At every release of the display lock by
    /// the station thread (DaemonSharedSpectrum's release hook), so also
    /// after each turn of this controller's timer.
    void refreshDisplaySnapshot();
    /// The display thread: one source frame, as onSourceFrame() takes it,
    /// with what needs the station thread queued to it.
    void takeInOnDisplayThread(const DaemonSpectrumFrame& frame);
    /// The display thread: one turn of the sender. A timed turn is the
    /// timer's own and is noted for the display diagnostics; an asked one
    /// is displayWritable's.
    void displayThreadTurn(bool timedTurn, bool askedTurn);
    /// Display diagnostics: the time since the sender's previous turn.
    void noteSenderTurn(bool peerReady);
    /// Queues runQueuedFrameWork() to the station thread once.
    void queueFrameWork();
    void runQueuedFrameWork();
    /// The station thread: what a frame the display thread took in needs
    /// of it (configures, then the noise floors due).
    void mainWorkForFrame(const DaemonSpectrumFrame& frame);
    /// The noise-floor messages due on the endpoints of `frame`'s source.
    /// False when the session or peer changed meanwhile, or this
    /// controller went: the caller returns at once.
    bool sendDueNoiseFloors(const DaemonSpectrumFrame& frame, double stationOffsetDb);
    /// Keeps the newest frame of its source for the station thread.
    void holdFrameForMain(const DaemonSpectrumFrame& frame);
    /// Ends raw I/Q stream `sliceId` with `reason`. On the display thread
    /// it is recorded and left to the station thread.
    void failIqStream(int sliceId, const QString& reason, bool budgetRefused);
    /// The current peer's display sends: through m_displaySink while this
    /// controller is on the display thread, else through the peer.
    bool peerDisplayReady() const;
    bool peerDisplayBusy(MediaPeer* peer) const;
    IMediaTransport::DisplaySendResult peerSubmitDisplay(MediaPeer* peer,
                                                         const QByteArray& message);
    /// NSDC v2: what became of one frame's messages.
    enum class DisplayV2Send {
        PeerGone, // the peer or epoch changed during a send; touch nothing
        NotTaken, // the first message was not taken: the frame is dropped
        Taken,    // the first message (the row, or the whole frame) was taken
    };
    /// NSDC v2: offers a frame's messages in order in this one turn, the
    /// row part first; after a message is not taken nothing more is
    /// offered. Records the diagnostics. Pacing and the admitted cost were
    /// checked by the caller against encoding.totalBytes().
    DisplayV2Send submitDisplayCodecV2(MediaPeer* peer, quint64 epoch,
                                       const DisplayCodecV2Encoding& encoding);
    bool peerIqBusy(MediaPeer* peer) const;
    IMediaTransport::DisplaySendResult peerSubmitIq(MediaPeer* peer, const QByteArray& message);
    void onStreamGeometryChanged(int streamIndex, double centreHz, int sampleRateHz);
    void onStreamBindingsChanged(int streamIndex, const QVector<int>& sliceIds);
    void onSliceRemoved(int sliceId);
    /// Fix wave after the several-devices group review: retires this
    /// session's display endpoints on `sliceId` with the slice-removed
    /// reason. False when the peer or session changed while doing so.
    bool retireSliceDisplays(int sliceId);
    void onRadioConnectionStateChanged(ConnectionState state);

    bool handleStart(const QJsonObject& control);
    bool handleSubscribe(const QJsonObject& control);
    bool handleUnsubscribe(const QJsonObject& control);
    bool handleKeyframe(const QJsonObject& control);
    /// R-IOS-27, R-IOS-06 (displayExtrasVersion 2): {op:"clarity-retune",
    /// connectionId, endpointId}. Clarity's Re-tune for that endpoint, what
    /// the desktop's Re-tune button does for its pan. Nothing is sent when
    /// it runs; a refusal is a `rejected` naming the endpoint with
    /// revision 0. A request of another shape is ignored.
    bool handleClarityRetune(const QJsonObject& control);
    bool handleAudio(const QJsonObject& control);
    /// R-R3-43: {op:"receiver-audio", connectionId, sliceId, revision,
    /// enabled, profile}, only from a GUI that declared receiverAudioVersion
    /// in its start; anything else is ignored. Answered with a
    /// receiver-audio-context; never touches the main audio context.
    bool handleReceiverAudio(const QJsonObject& control);
    bool handleIqStream(const QJsonObject& control);
    void reconcileIqStream(int sliceId);
    void reconcileWantedIq();
    void reconcileIqBudget();
    bool iqFitsCurrentShare() const;
    void stopIqStream(IqStream& stream);
    void sendIqContext(int sliceId, const IqStream& stream, const QString& reason);
    bool trySendIq(MediaPeer* peer, quint64 epoch, qint64 nowNs);
    quint64 iqBytesPerSecond() const;
    /// R-R3-45: {op:"headphones-audio", connectionId, revision, enabled,
    /// profile}, only from a GUI that declared headphonesMixVersion in its
    /// start; anything else is ignored. Answered with a
    /// headphones-audio-context; never touches the main audio context.
    bool handleHeadphonesAudio(const QJsonObject& control);
    /// Parity Task 32: {op:"monitor-audio", connectionId, revision, route},
    /// only from a GUI that declared txMonitorAudioVersion in its start;
    /// anything else (another shape, a stale revision, another connection)
    /// is ignored. Answered with one monitor-audio-context carrying the
    /// route as applied. Taken on and off the air: it only picks which of
    /// this device's streams carries MON, and reaches no device.
    bool handleMonitorAudio(const QJsonObject& control);
    /// The route this device asked for, as it applies here: headphones is
    /// the main stream for a device that did not declare the headphones mix.
    TxMonitorRoute appliedMonitorRoute() const;
    /// Recomputes where MON goes for this device (holder, MON, the route)
    /// and hands it to the owner mix; the headphones mix follows when MON
    /// comes onto or off it.
    void refreshTxMonitor();
    /// Whether this device's headphones mix has anything to carry: a
    /// receiver of its own on the headphones, or MON routed there.
    bool headphonesMixNeeded() const;
    /// R-R3-35: answers {op:"clock-probe", connectionId, id, t0} with
    /// {op:"clock-echo", connectionId, id, t0, t1, t2, generation,
    /// rtpTimestamp, capturedNs}. t1 is the Core clock on entry to
    /// onControl(), t2 just before the reply. generation is the running
    /// audio context and rtpTimestamp/capturedNs the newest captured block's
    /// end (DaemonAudioSenderTelemetry::captureTimestamp/captureNs); all
    /// three are 0 when no audio context is capturing.
    bool handleClockProbe(const QJsonObject& control, qint64 receivedNs);
    // Parity Task 28: the transmit display.
    void wireTxDisplayFeed();
    void reconcileTransmitDisplay();
    bool endpointOnTransmitPan(const EndpointEntry& entry) const;
    /// Records the transmit slice's pan at the rise (keyed true) and
    /// forgets it at the fall.
    void recordTransmitPan(bool keyed);
    // Parity Task 31: this session's device's DUP (any endpoint subscribed
    // with `duplex` true) to RadioModel::setDeviceDisplayDuplex.
    void refreshDeviceDisplayDuplex();
    std::optional<QJsonObject> transmitContextFor(const EndpointEntry& entry) const;
    void onTransmitPlane(const QVector<float>& dbm, bool waterfall, bool mini = false);
    bool trySendTransmitFrame(quint32 endpointId, MediaPeer* peer, quint64 epoch,
                              qint64 nowNs);
    // Task 36: the microphone line.
    void startMicLine(MediaPeer* peer);
    void stopMicLine();
    void refreshMicVoxArmed();
    void refreshMicWatching();
    // R-IOS-13, R-R3-42: at each unkey, one info line with this line's
    // microphone statistics and the transmit I/Q send path's counters.
    // TX stall lane: the microphone figures are taken as the unkey starts
    // (MoxController::moxChanging, before the feed leaves use and its
    // underrun count resets), and the line is logged when the walk ends
    // (moxStateChanged(false)), with the send path's counters then; a key
    // that cuts the walk short logs it first. Only the controller whose
    // device holds the key on its line takes the figures (fix round 1).
    void snapshotUnkeyStats();
    void logUnkeyStats();
    bool acceptPeerControl(const QJsonObject& control);
    // iPhone app plan Task 29 (R-IOS-16; the media document, "Replacing
    // the media connection"): a new peer beside the current one, both
    // carrying audio for kReplaceOverlapMs, then the new one takes over.
    bool handleReplace(const QJsonObject& control);
    /// Task 29 step 2b: the ICE settings a media peer of this session gets.
    std::optional<IceConfiguration> mediaIceConfiguration();
    void onReplacementReady();
    void finishReplacement();
    void failReplacement(const QString& reason);
    /// Drops a replacement under way and a retired peer still draining.
    void clearReplacement();
    /// Wires `peer` as the current peer (handleStart's callbacks).
    void wireCurrentPeer(MediaPeer* peer, const QString& connectionId);
    /// A peer's audio SSRCs in stream order: main, receiver 0 to 3,
    /// headphones.
    static QList<quint32> audioSsrcsOf(const MediaPeer* peer);
    /// The radio is idle (MoxController in Rx).
    bool radioIdleForReplace() const;

    void clearSession();
    /// Drops the media peer while the session stays (the Core dropped it,
    /// or the device started media on a new connection), and splits the
    /// display budget again when the peer had asked for displays.
    void retirePeerKeepingSession();
    void clearProduction();
    QList<quint32> endpointIds() const;
    void removeEndpoint(quint32 endpointId, bool retainOperation = true);
    /// `sourceUpdate`, when given, says what an accepted reconcile did to a
    /// source that was already running (R-DISP-RETUNE): relabeled through
    /// a retune, or anything else. A caller holding an endpoint outside
    /// m_endpoints uses it to carry that endpoint over a relabel too.
    bool reconcileSource(const MediaSourceKey& key,
                         DaemonSpectrumSourceUpdate* sourceUpdate = nullptr);
    void releaseSourceIfUnused(const MediaSourceKey& key);
    void rebalanceSourceAfterDeparture(const MediaSourceKey& key);
    void configureEndpointFromFrame(EndpointEntry& endpoint,
                                    const DaemonSpectrumFrame& frame);
    void sendContext(EndpointEntry& endpoint);
    std::optional<float> fullSourceNoiseFloor(const DaemonSpectrumFrame& sourceFrame,
                                               double stationOffsetDb);
    void sendRejected(const QString& connectionId, quint32 endpointId,
                      quint32 revision, const QString& reason);
    void sendAllocationResult(const QString& connectionId, quint32 endpointId,
                              quint32 revision, bool accepted, const QString& reason);
    bool rejectAllocation(const QJsonObject& control, quint32 endpointId,
                          quint32 revision, const QString& reason,
                          bool remember = true);
    bool displayBudgetWireAvailable() const;
    bool displayPacingRequired() const;
    /// R-R3-08/40: the budget in force is lowered because the Core is busy,
    /// so every source's transforms follow its frame rate.
    bool coreBusyLimitsSources() const;
    qint64 displayNowNs() const;
    void beginDisplayBudgetIfNeeded();
    void refreshDisplayBudgetPacer();
    DisplayBudgetCharge currentSpectrumCharge() const;
    std::optional<DisplayBudgetCharge> proposedSpectrumCharge(
        quint32 endpointId, const DisplayBudgetCharge& replacement) const;
    bool spectrumAdmissionFits(quint32 endpointId,
                               const DisplayBudgetCharge& replacement) const;
    bool admitPs3Display(bool enabled, QString* refusal);
    void onRemoteAmpViewSubscriptionChanged(bool subscribed);
    void rememberNonliveOperation(quint32 endpointId, const AllocationRecord& record);
    void forgetNonliveOperation(quint32 endpointId);
    void clearAllocationIdentity();
    void promoteLatestPs3Frame();
    bool trySendPs3(MediaPeer* peer, quint64 epoch, qint64 nowNs);
    bool trySendSpectrum(MediaPeer* peer, quint64 epoch, qint64 nowNs);
    /// iPhone app Task 20 (R-IOS-27): sends one endpoint's waiting NSDX
    /// datagram, the extras of the frame it last sent. True when it tried.
    bool trySendDisplayExtras(MediaPeer* peer, quint64 epoch, qint64 nowNs);
    DisplayExtrasInputs displayExtrasInputs(const EndpointEntry& entry, double binWidthHz,
                                            qint64 nowNs) const;
    /// The key state the display extras see: m_displayKeyed, or
    /// TransmitModel's flag when the Core has no MoxController.
    bool displayKeyed() const;
    void reconcileAudio();
    void stopAudioCapture();
    /// `reason` travels only in a disabled context, and only to a peer that
    /// agreed the minor-8 detail; an enabled one carries the encoder profile.
    void sendAudioContext(bool enabled, RemoteAudioOffReason reason);
    void beginAudioDiagnostics(quint32 contextGeneration);
    void finalizeAudioDiagnostics();
    void maybeLogAudioDiagnostics(bool final);
    DaemonAudioDiagnostics snapshotAudioDiagnostics() const;
    void resetAudioSession();
    /// Admits the requested profile against the Core setting and the peer's
    /// negotiated formats, setting the active profile and any refusal.
    void admitAudioProfile();
    /// The profile rules on their own: lossless when requested, allowed by
    /// the Core and carried by this media connection (or not yet known,
    /// before the peer is ready); otherwise Opus and why.
    AdmittedAudioProfile admitProfile(RemoteAudioProfile requested) const;
    void reconcileReceiverAudio(int sliceId);
    void stopReceiverAudioCapture(ReceiverAudioStream& stream);
    void releaseReceiverStreamIndex(ReceiverAudioStream& stream);
    void retireReceiverSender(ReceiverAudioStream& stream);
    void sendReceiverAudioContext(int sliceId, const ReceiverAudioStream& stream,
                                  bool enabled, RemoteAudioOffReason reason);
    void resetReceiverAudioSession();
    quint32 nextReceiverContextGeneration();
    // R-R3-45: the headphones mix.
    void reconcileHeadphonesAudio();
    /// Why the headphones mix cannot run now, in reconcile order; empty
    /// when it can.
    std::optional<RemoteAudioOffReason> headphonesBlockedBy() const;
    void stopHeadphonesAudioCapture();
    void sendHeadphonesAudioContext(bool enabled, RemoteAudioOffReason reason);
    void resetHeadphonesAudioSession();
    quint32 nextHeadphonesContextGeneration();
    /// Some slice on this Core plays on the headphones now.
    bool anySliceOnHeadphones() const;
    void watchSliceOutputRoute(SliceModel* slice);
    void onOutputRoutesChanged();
    void recordDisplaySent(const QByteArray& spectrumFrame, bool keyframe);
    void onMediaTransportError(const QString& message);
    void onMediaPeerError(const QString& message);
    void logDisplayDiagnostics(bool final);
    /// Control logging lane: the media connection's selected pair, when
    /// first known and on every change (logging only).
    void logMediaPath();
    bool sendControl(const QJsonObject& payload) const;
    quint32 nextContextGeneration();

    QPointer<StationServer> m_server;
    QPointer<RadioModel> m_radioModel;
    /// Task 76: the session this controller alone serves (0: one session at
    /// a time, whichever starts while it has none).
    quint64 m_boundEpoch{0};
    std::shared_ptr<DaemonSharedSpectrum> m_shared;
    DaemonSpectrumSource& m_source;
    QMap<MediaSourceKey, SourceRuntime>& m_sources;
    int m_ownerMix{-1};
    // Slice control plan Task 6: follows this device's listen levels while
    // it holds an owner mix.
    QMetaObject::Connection m_listenLevelConnection;
    /// coreBusyLimitsSources() as last applied to the sources.
    bool m_transformsFollowFrameRate = false;
    // The Core is keyed, as its displays see it: set in the
    // MoxController::moxStateChanged slot, so it turns on when the walk
    // into transmit ends and off when the walk out ends. Written under the
    // display lock and read on the main thread only; the display thread
    // reads its copy (EndpointEntry::snapshotSlice).
    bool m_displayKeyed = false;
    NoiseFloorEstimator m_noiseFloorEstimator;
    MediaPeer::TransportFactory m_peerFactory;
    MonotonicClock m_monotonicClock;
    std::unique_ptr<MediaPeer> m_peer;
    // iPhone app plan Task 29: a replacement peer under way, whether it is
    // ready, the old peer draining after it took over, and the SSRC maps
    // (a packet stamped with an earlier peer's SSRC, as a sender started
    // before a replacement stamps it, to the current peer's; a microphone
    // packet from a newer peer to the receiver's).
    std::unique_ptr<MediaPeer> m_replacement;
    QString m_replacementId;
    bool m_replacementReady{false};
    std::unique_ptr<MediaPeer> m_retiring;
    QTimer m_replaceOverlapTimer;
    QTimer m_replaceConnectTimer;
    QTimer m_retireDrainTimer;
    QHash<quint32, quint32> m_sendSsrcRewrite;
    QHash<quint32, quint32> m_micSsrcRewrite;
    // TX mic thread (JJ approved 2026-10-01): where the microphone line's
    // packets go, from the transport's own thread or the event loop: this
    // controller's receiver and SSRC rewrites, copied in under its lock
    // whenever either changes (syncMicRoute). A packet in delivery holds
    // the lock, so a receiver taken out of the route is never used again.
    struct MicRoute {
        std::mutex mutex;
        RemoteMicReceiver* receiver{nullptr};
        QHash<quint32, quint32> rewrite;
        void deliver(const QByteArray& packet, qint64 heldUs);
    };
    std::shared_ptr<MicRoute> m_micRoute{std::make_shared<MicRoute>()};
    void syncMicRoute();
    IMediaTransport::MicPacketSink micRouteSink() const;
    // Remote transmit cut off the main thread: the hook a peer of media
    // epoch `epoch` reports its "tx" receipts to (empty without a server).
    IMediaTransport::TxReceiptHook txReceiptHook(quint64 epoch) const;
    // R-AUD-SEND: the mirror of MicRoute for receive audio. m_sendThread
    // drains every sender this controller runs; a sender hands each packet
    // to its own route's deliver(), on that thread. The route holds what
    // sending needs, copied in under its lock by this controller's thread
    // whenever any of it changes (syncAudioRoutes): the current peer's sink
    // and SSRC rewrite, and while a replacement is ready its sink and SSRC
    // mapping. deliver() never calls back into this controller. A packet in
    // delivery holds its own reference to the sinks it copied out, not the
    // lock, so a sync never waits for a send: the one packet under way may
    // still go to a sink the sync just took out. A sink is valid for as long
    // as anything holds it and answers "not ready" once its peer stopped.
    // What ends a sender's sends for certain is its stop().
    void attachAudioSender(DaemonAudioSender* sender,
                           const std::shared_ptr<AudioSendRoute>& route);
    /// Stops `sender`, closes `route` and takes the sender from the thread:
    /// when this returns the thread never touches the sender again.
    void detachAudioSender(DaemonAudioSender* sender,
                           const std::shared_ptr<AudioSendRoute>& route);
    void syncAudioRoutes();
    // The media connections already warned about for having no audio send
    // off the transport's own thread: said once for each, not at every sync.
    QSet<QString> m_noAudioSinkWarned;
    std::shared_ptr<AudioSendRoute> m_audioRoute{std::make_shared<AudioSendRoute>()};
    DaemonAudioSendThread m_sendThread;
    // What the current peer's start negotiated, which a replacement keeps.
    bool m_startOfferedLossless{false};
    /// Task 29 step 2b: the media start declared the tunnel.
    bool m_startTunnel = false;
    bool m_startRelayRouting = false;
    /// Direct media ladder: whether the connection in use (and the one
    /// replacing it) was made with media routing (the tunnel or a routed
    /// relay). An unrouted shim leg is an older relay leg that cannot move.
    bool m_currentRouted = false;
    bool m_replacementRouted = false;
    bool m_startMicLine{false};
    // NSDC v2: the display codec version the last start agreed (2 or 1);
    // written on the main thread, read by whichever thread sends the
    // displays. A replace carries no start, so it keeps this.
    std::atomic<quint32> m_displayCodecVersion{1};
    std::unique_ptr<DaemonAudioSender> m_audioSender;
    int m_audioTargetBitrate{OpusAudioCodecConfig{}.bitrate};
    // R-R3-23 lossless audio, per media peer. The GUI declares it understands
    // the profile in its media start (the offer then carries L16) and in its
    // audio control (contexts then carry the profile shape).
    bool m_audioLosslessAllowed{true};
    // Bench setting DisplayKeyframeEveryFrame; written on the main thread,
    // read by whichever thread sends the displays.
    std::atomic<bool> m_displayKeyframeEveryFrame{false};
#ifdef NEREUS_BUILD_TESTS
    // NSDC v2 tests (setEncodedFrameObserverForTest); under the display lock.
    std::function<void(const DisplayCodecFrame&)> m_encodedFrameObserver;
#endif
    bool m_audioProfileNegotiated{false};
    RemoteAudioProfile m_audioRequestedProfile{RemoteAudioProfile::Opus};
    RemoteAudioProfile m_audioActiveProfile{RemoteAudioProfile::Opus};
    std::optional<RemoteAudioProfileRefusal> m_audioProfileRefusal;
    // iPhone app plan Task 23 (audioQualityVersion 1), per media peer (so
    // per device): the Opus bitrate this device asked for (0: none, the
    // Core's audio_bitrate), the bitrate the running sender was built with,
    // and why the latest request's bitrate was not taken.
    int m_audioRequestedBitrate{0};
    int m_audioSenderBitrate{0};
    QString m_audioBitrateRefusal;
    // R-R3-43 receiver audio, per media peer. Requests are honoured only
    // when the GUI declared receiverAudioVersion at start (the offer then
    // declares the receiver stream ids). Entries outlive their streams so a
    // stale revision stays refused; only slices that existed are entered.
    bool m_receiverAudioNegotiated{false};
    // Parity Task 28: this session's start declared txDisplayVersion.
    bool m_txDisplayNegotiated{false};
    bool m_miniDisplayNegotiated{false};
    // R-DISP-RETUNE: this session's start declared spectrumPictureVersion,
    // so each of its spectrum contexts carries pictureGeneration.
    bool m_spectrumPictureNegotiated{false};
    // Parity Task 31: the version it declared (3 and above: `duplex`), and
    // the device whose DUP this controller last told RadioModel is on.
    quint32 m_txDisplayDeclared{0};
    QByteArray m_duplexDevice;
    QPointer<TxDisplayFeed> m_txFeed;
    QMetaObject::Connection m_txArbiterConnection;
    /// The transmitting pan, recorded at the keyed rise and used until the
    /// fall (the window's MoxDisplayController records the same pan): its
    /// key, or the transmit slice's id when that slice had no pan key.
    bool m_txRiseRecorded{false};
    QString m_txRisePanKey;
    int m_txRiseSliceId{-1};
    /// The current peer's connection failed (ICE consent lost, DTLS failed)
    /// before it closed: which reason the app is told.
    bool m_peerLost{false};
    std::map<int, ReceiverAudioStream> m_receiverStreams;
    bool m_iqNegotiated{false};
    std::map<int, IqStream> m_iqStreams;
    quint32 m_nextIqGeneration{0};
    quint64 m_iqBudgetChangeRevision{0};
    bool m_iqBudgetRecheckScheduled{false};
    /// Per receiver stream id: the slice holding it (-1 free) and the next
    /// RTP sequence and timestamp, so each id's timeline continues across
    /// contexts as the main stream's does.
    std::array<int, IMediaTransport::kMaxReceiverAudioStreams> m_receiverStreamSlice{};
    std::array<quint16, IMediaTransport::kMaxReceiverAudioStreams> m_receiverNextSequence{};
    std::array<quint32, IMediaTransport::kMaxReceiverAudioStreams> m_receiverNextTimestamp{};
    quint32 m_nextReceiverContextGeneration{0};
    // R-R3-45 headphones mix, per media peer. Requests are honoured only
    // when the GUI declared headphonesMixVersion at start (the offer then
    // declares the headphones stream id, and the main stream carries the
    // speakers' mix alone). m_headphonesRouted is anySliceOnHeadphones() as
    // last acted on.
    bool m_headphonesMixNegotiated{false};
    HeadphonesAudioStream m_headphones;
    quint32 m_nextHeadphonesContextGeneration{0};
    bool m_headphonesRouted{false};
    // Parity Task 32: the transmit monitor, per media peer. Requests are
    // honoured only when the GUI declared txMonitorAudioVersion at start.
    bool m_txMonitorNegotiated{false};
    quint32 m_monitorRevision{0};
    TxMonitorRoute m_monitorRoute{TxMonitorRoute::None};
    TxMonitorRoute m_monitorApplied{TxMonitorRoute::None};
    // Task 36: the microphone line of the current media peer, and the
    // device it is for.
    std::unique_ptr<RemoteMicReceiver> m_micReceiver;
    QByteArray m_micDeviceId;
    // TX stall lane: the microphone figures of the unkey in progress.
    struct UnkeySnapshot {
        QByteArray deviceId;
        RemoteMicReceiver::Stats rx;
        RemoteMicFeed::Stats feed;
        bool haveFeed{false};
        qint64 keepaliveWaitMaxUs{-1};
    };
    std::optional<UnkeySnapshot> m_unkeySnapshot;
    // TX mic thread fix round 2: the over's longest wait at the Core of a
    // "tx" keepalive (-1: none yet), for the unkey line.
    qint64 m_overKeepaliveWaitMaxUs{-1};
    void noteKeepaliveWait(qint64 heldUs);
    std::map<quint32, EndpointEntry> m_endpoints;
    QTimer m_sendTimer;
    QTimer m_audioDiagnosticsTimer;
    DisplayBudgetPacer m_displayPacer;
    bool m_displayPacerInitialized{false};
    quint64 m_lastSessionEpoch{0};
    quint64 m_epoch{0};
    quint32 m_nextContextGeneration{0};
    quint32 m_audioRevision{0};
    quint16 m_audioNextSequence{1};
    quint32 m_audioNextTimestamp{0};
    bool m_audioDesiredEnabled{false};
    DaemonAudioDiagnostics m_audioDiagnostics;
    QElapsedTimer m_audioDiagnosticsClock;
    // R-R3-21: paces the refused-keyframe log line.
    QElapsedTimer m_keyframeRefusalLog;
    qint64 m_audioDiagnosticsLastLogMs{0};
    QTimer m_displayDiagnosticsTimer;
    /// Control logging lane: the selected pair last logged for this
    /// peer's session, and how often a keepalive may look at it.
    QString m_mediaPathLogged;
    QElapsedTimer m_mediaPathChecked;
    DaemonDisplayDiagnostics m_displayDiagnostics;
    DaemonDisplayDiagnostics m_displayDiagnosticsLogged;
    /// When the display sender's timer last turned with this peer ready
    /// (displayNowNs), and the longest gap between two such turns since
    /// the last display diagnostics line (-1: none), which that line
    /// carries and takes.
    std::optional<qint64> m_lastSenderTurnNs;
    qint64 m_senderGapMaxSinceLoggedUs{-1};
    // Display send thread (see displayGuard()). The lock is the shared
    // spectrum's, held here too so a guard outlives neither.
    std::shared_ptr<DaemonDisplayLock> m_displayLock;
    /// Whether this controller sends from the display thread, and the
    /// current peer's display sink while it does.
    bool m_displayThreadMode{false};
    std::shared_ptr<MediaDisplaySink> m_displaySink;
    /// The peer that sink is from. Only compared with m_peer, never
    /// dereferenced, so a sink kept past its peer is never used for the
    /// next one (peerDisplayReady).
    MediaPeer* m_displaySinkPeer{nullptr};
    /// The display channel took its held message: one turn soon.
    std::atomic<bool> m_displayTurnAsked{false};
    /// The station thread's values a display thread turn reads.
    struct DisplaySnapshot {
        bool sending{false};
        bool pacingRequired{false};
        bool ps3Here{false};
        bool iqFits{false};
        bool keyedNow{false};
        double duplexOffsetDb{0.0};
        QHash<int, double> offsetByStream;
        std::array<std::optional<WidebandSpectrumFrame>, WidebandSpectrumCache::kMaxSources>
            widebandByAdc;
    };
    DisplaySnapshot m_displaySnapshot;
    /// Frames the display thread took in for the station thread: one per
    /// source whose endpoints wait on a configure or a noise floor, and,
    /// while this controller is not on the display thread, every frame.
    QMap<MediaSourceKey, DaemonSpectrumFrame> m_framesForMain;
    QMap<MediaSourceKey, DaemonSpectrumFrame> m_pendingSourceFrames;
    bool m_frameWorkQueued{false};
    /// Diagnostics: the places warnIfDisplayLockNotHeld() has logged.
    mutable std::mutex m_unguardedWarnedMutex;
    mutable QSet<QByteArray> m_unguardedWarned;
    QSet<QString> m_loggedTransportErrorKinds;
    int m_roundRobinCursor{0};
    QList<QByteArray> m_ps3CurrentChunks;
    QList<QByteArray> m_ps3LatestChunks;
    bool m_ps3CurrentAttempted{false};
    bool m_lastDisplayAttemptWasPs3{false};
    quint32 m_endpointHighWater{0};
    /// Fix wave I5: each display's requested charge, by endpoint id.
    std::map<quint32, DisplayBudgetCharge> m_displayDemand;
    /// Fix wave 2 (Important 2): a subscription refused for the budget
    /// keeps its request in m_displayDemand only until the client asks for
    /// that endpoint again (subscribe or unsubscribe) or the hold runs
    /// out; then the endpoint asks for what it asked before the refusal
    /// (`before`: its live display's request, or nothing).
    struct RefusedDemand {
        std::optional<DisplayBudgetCharge> before;
        qint64 endsAtNs{0};
    };
    std::map<quint32, RefusedDemand> m_refusedDemand;
    QTimer* m_refusedDemandTimer{nullptr};
    int m_refusedDemandHoldMs{kRefusedDisplayDemandHoldMs};
    /// Records `endpointId`'s refused request, held until kept, replaced
    /// or run out.
    void holdRefusedDemand(quint32 endpointId, std::optional<DisplayBudgetCharge> before);
    /// Ends every refused request whose hold ran out, and re-arms the timer.
    void endRefusedDemands();
    /// Sets (or, with nullopt, forgets) one display's demand and has the
    /// Core split its budget again when the demand changed.
    void setDisplayDemand(quint32 endpointId, std::optional<DisplayBudgetCharge> demand);
    std::map<quint32, AllocationRecord> m_nonliveOperations;
    QList<quint32> m_nonliveOperationOrder;
};

/// iPhone app Task 76 (ruling 9.1): one DaemonMediaController per admitted
/// session. Makes a controller bound to each media session as it starts and
/// retires it when that session ends; all of them share one set of spectrum
/// engines. Installs the Core's PureSignal display gate (the subscribing
/// session's controller answers it) and turns display budget enforcement
/// on. A hosting desktop's own window draws locally and has no controller.
class DaemonMediaHub final : public QObject {
    Q_OBJECT
public:
    explicit DaemonMediaHub(StationServer* server, RadioModel* radioModel,
                            QObject* parent = nullptr,
                            MediaPeer::TransportFactory peerFactory = {},
                            DaemonMediaController::MonotonicClock monotonicClock = {});
    ~DaemonMediaHub() override;

    /// Applied to every controller now and later (R-R3-23).
    void setAudioTargetBitrate(int bitsPerSecond);
    void setAudioLosslessAllowed(bool allowed);
    /// Bench setting kDisplayKeyframeEveryFrameSettingsKey, applied to
    /// every controller now and later.
    void setDisplayKeyframeEveryFrame(bool on);
    bool displayKeyframeEveryFrame() const noexcept { return m_displayKeyframeEveryFrame; }
    /// The controller for a media session, or null.
    DaemonMediaController* controllerFor(quint64 epoch) const;
    QList<DaemonMediaController*> controllers() const;
    int controllerCount() const { return static_cast<int>(m_controllers.size()); }
    /// Display traffic accepted now across every controller, PureSignal's
    /// display counted once: what the display load governor scales.
    DisplayBudgetCharge acceptedDisplayCharge() const;
    DaemonAudioDiagnostics audioDiagnostics(quint64 epoch) const;
    const std::shared_ptr<DaemonSharedSpectrum>& sharedSpectrum() const { return m_spectrum; }

private:
    void onSessionStarted(quint64 epoch);
    void onSessionEnded(quint64 epoch);

    QPointer<StationServer> m_server;
    QPointer<RadioModel> m_radioModel;
    MediaPeer::TransportFactory m_peerFactory;
    DaemonMediaController::MonotonicClock m_monotonicClock;
    std::shared_ptr<DaemonSharedSpectrum> m_spectrum;
    std::map<quint64, std::unique_ptr<DaemonMediaController>> m_controllers;
    std::optional<int> m_audioTargetBitrate;
    std::optional<bool> m_audioLosslessAllowed;
    bool m_displayKeyframeEveryFrame{false};
};

} // namespace NereusSDR
