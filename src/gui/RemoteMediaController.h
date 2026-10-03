#pragma once
#include <QVector>
// no-port-check: NereusSDR-original. Remote daemon R3 receive display wiring.
//
// Modification history (NereusSDR):
//   2026-09-29: startReplacement takes whether the replace carries a
//               folded session move, recorded before it is sent. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: holdAudioRestartForTest and audioRestartStepHeldForTest;
//               endWaitingFallback.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: kDirectMediaSilenceFallbackMs 3000 -> 5000 ms, as JJ
//               ruled. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-29: direct media: audioRestartPendingForTest, so a test can see an
//               audio restart waiting on its backoff step. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: direct media fix wave: the silence fallback runs once per
//               silence, only while the window wants audio, onto the
//               tunnel alone, and a fallback that brings no media back
//               asks for recovery (ReplaceKind). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29: the direct media ladder: upgradeToDirectConnection (a
//               direct-only replace while media rides the tunnel, on the
//               PathRacer::kUpgradeRetryMs steps, never keyed or with VOX
//               armed) and the no-packets fallback
//               (kDirectMediaSilenceFallbackMs back to the tunnel).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 36 (R-IOS-13): the microphone uplink.
//               The window captures the microphone chosen in Audio > Devices
//               through the capture helper and sends it on the media
//               connection's microphone line while it transmits or has VOX
//               armed; a program keying through its TCI server is sent in
//               place of the microphone. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-25: iPhone app plan, desktop remote transmit (R-IOS-13): the
//               uplink follows the window's transmit client and the Core's
//               mirrored VOX by itself. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-26: transmit group fix wave: I4 setTransmitHolder fed from
//               txState's holder; M6 the microphone streams unkeyed only for
//               VOX this window armed. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-26: transmit group fix wave 2 (M8): micLineOpen and
//               micLineChanged, so VOX shows disabled with its reason while
//               this computer has no microphone line to the Core. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: parity Task 28 (R-R3-49, A11, R-IOS-13): a Core at
//               txDisplayVersion 1 is told at media start that this window
//               takes its transmit display; each subscribe carries the pan's
//               transmit window (txMinDbm, txMaxDbm), and a context the Core
//               marks `transmit` and its frames are handed on
//               (transmitContextReceived, transmitFrameReceived) instead of
//               being drawn as receive; drawing them is Task 29. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: parity Task 29 (A11, R-R3-49): setPanTransmitting holds a
//               transmitting pan's receive frames while keyed and says when
//               the Core sends no transmit display; refreshTransmitView asks
//               again at once for the transmitting pan's moved view. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: parity Tasks 27-29 fix wave (R-R3-49): heldTransmitContext,
//               the Core's transmit context for a pan as it stands, so a
//               context that beat the window's own rise is not lost. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: R-R3-21 / R-R3-08: the spectrum and waterfall presented on
//               the audio's playout clock (displayClockVersion 1), gap rows
//               blended or repeated, and the display counters. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 28 (R-IOS-16): the connection stage of
//               a session through the remote access service is longer by
//               the gathering bound. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: parity Task 31 (A11, R-R3-49): display duplex. A Core at
//               txDisplayVersion 3 is told 3 at media start, and while this
//               window's DUP is on every subscribe carries `duplex` true, so
//               the Core keeps the transmitting pan's receive frames while
//               keyed. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-09-27: parity Task 32 (R-IOS-13, R-R3-49): the transmit monitor. A
//               Core at txMonitorAudioVersion 1 is told it at media start
//               and sent this window's MON output as monitor-audio, on
//               change and on each media connection. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 (R-IOS-16): replaceConnection() and,
//               in its fix wave, a replacement kept pending until it can
//               start. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.

#include "core/session/media/DisplayBudget.h"
#include "core/session/media/DisplayCodec.h"
#include "core/session/media/IReceiverPcmSink.h"
#include "core/session/media/MediaPeer.h"
#include "core/session/NetworkPathSnapshot.h"
#include "core/session/media/RemoteAudioContext.h"
#include "core/session/media/RemoteAudioReceiver.h"
#include "core/session/media/RemoteSpectrumContext.h"
#include "gui/PanStatusText.h"
#include "gui/RemoteAudioStatus.h"
#include <QHash>
#include <QObject>
#include <QSet>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace NereusSDR {
class RemoteTciAudioStage;
class StationClient;
class RadioModel;
class PanadapterStack;
class SpectrumWidget;

/// R-R3-21 / R-R3-08: this window's display counters for its media session
/// (every pan together), for the diagnostics line.
struct RemoteDisplayTelemetry {
    /// Lost display messages that began a wait for a keyframe.
    quint64 keyframeWaits = 0;
    /// Keyframe requests sent to the Core.
    quint64 keyframeRequests = 0;
    /// Waterfall rows blended across a gap, and repeated while waiting.
    quint64 rowsBlended = 0;
    quint64 rowsRepeated = 0;
    /// The largest time between two display messages of one pan arriving,
    /// in the last 10 s (any pan).
    std::optional<double> largestArrivalGapMs;
    /// Decoded frames the presenter dropped because its queue was full, and
    /// rows the pans' waterfalls dropped for the same reason (never expected).
    quint64 itemsDropped = 0;
    quint64 rowsDropped = 0;
    /// How far behind the Core's capture the display is presented now (the
    /// audio's delay, or the last known one), or nothing while each frame is
    /// drawn on arrival (an older Core, or no clock echo yet).
    std::optional<double> displayDelayMs;
};

/// Owns the GUI's media session and one bounded subscription per logical pan.
/// Layout reparenting does not retire a pan; removing it from the stack does.
/// Display data never passes through the control/property mirror.
class RemoteMediaController final : public QObject {
    Q_OBJECT
public:
    using AllocationClock = std::function<qint64()>;

    /// R-R3-28. How long a started media session may take to become ready
    /// before it counts as a media failure and enters the authenticated
    /// retry. Two stages, each on a precise single-shot timer:
    ///
    /// Stage one, kMediaDescriptionDeadlineMs: Core's media description
    /// must arrive within one control heartbeat interval
    /// (StationClient::kDefaultHeartbeatIntervalMs, 20,000 ms). Core sends
    /// it as soon as its peer starts; nothing in the peer library runs on
    /// this side before it arrives, so a Core that never answers is found
    /// in about 20 s rather than after the whole library chain.
    ///
    /// Stage two, kMediaConnectDeadlineMs, restarted when the description
    /// is accepted: it must outlast the slowest failure the pinned peer
    /// library reports by itself, so the library's own typed reason wins
    /// whenever one comes. That worst case is its three connection stages
    /// running one after another, the first two succeeding only just short
    /// of their limits and the last failing at its own:
    ///   - ICE: 39,500 ms, the libjuice connectivity timer
    ///     (_deps/nereus_libjuice-src/src/agent.h:43 [@3c40a354]
    ///     ICE_PAC_TIMEOUT, armed at agent.c:2603 [@3c40a354], reported as
    ///     failed at agent.c:1224-1226 [@3c40a354]).
    ///   - DTLS: 31,000 ms, libdatachannel v0.24.5 with OpenSSL: a 1 s
    ///     retransmit timer doubling until the next wait would exceed 30 s,
    ///     so 1+2+4+8+16 s (src/impl/dtlstransport.cpp:1024-1031), then
    ///     "DTLS handshake failed" (dtlstransport.cpp:1007-1008).
    ///   - SCTP: 35,000 ms, usrsctp INIT with a 1 s initial RTO capped at
    ///     10 s and 5 retransmissions, so 1+2+4+8+10+10 s
    ///     (src/impl/sctptransport.cpp:127-142; this build keeps those
    ///     defaults, LibDataChannelMediaTransport sets only buffer sizes).
    /// That is 105,500 ms, which the library's report always comes before.
    /// The whole bound, kMediaEstablishmentDeadlineMs, is the two stages'
    /// sum, 125,500 ms. The .cpp checks each derivation.
    ///
    /// iPhone app plan Task 28 (R-IOS-16): for a session through the remote
    /// access service, whose media connection gathers STUN and relay
    /// candidates before the library's stages run, stage two is longer by
    /// the gathering bound (IceConfiguration::kGatheringDeadlineMs, 23.5 s).
    ///
    /// The deadline only ever fires on a live control link: if control
    /// dies first (the heartbeat declares Core dead after two missed
    /// pongs, 40 to 60 s), the session ends, media is stopped with it and
    /// the heartbeat's own retry runs instead.
    static constexpr int kMediaDescriptionDeadlineMs = 20'000;
    static constexpr int kMediaConnectDeadlineMs = 105'500;
    static constexpr int kMediaEstablishmentDeadlineMs =
        kMediaDescriptionDeadlineMs + kMediaConnectDeadlineMs;

    RemoteMediaController(StationClient* client, RadioModel* model,
                          PanadapterStack* stack, QObject* parent = nullptr,
                          MediaPeer::TransportFactory factory = {},
                          AllocationClock allocationClock = {},
                          int allocationAckTimeoutMs = 10'000,
                          int descriptionDeadlineMs = kMediaDescriptionDeadlineMs,
                          int connectDeadlineMs = kMediaConnectDeadlineMs);
    ~RemoteMediaController() override;

    quint64 receivedDisplayFrames() const;
    int activeEndpointCount() const;
    // One remote mini analyzer endpoint per wanted receiver slice. Callers
    // fan accepted frames to every visible container that owns the slice.
    void setMiniDisplaySlices(const QSet<int>& sliceIds);
    std::optional<MediaPeerTelemetry> trafficTelemetry() const;
    /// Display updates this computer received but discarded, oldest first,
    /// because newer ones arrived before it could show them. Zero without a
    /// media session; each session starts from zero.
    quint64 displayMessagesDropped() const;
    /// R-R3-21 / R-R3-08: the display counters and the current delay.
    RemoteDisplayTelemetry displayTelemetry() const;
    /// R-R3-21 / R-R3-08: this Core stamps display frames and clock echoes
    /// from one clock (displayClockVersion 1 or later, with audio clock
    /// probes). Then the display is presented on the audio's clock and clock
    /// probes run while the display does; without it each frame is drawn on
    /// arrival, as before.
    bool displayClockNegotiated() const;
    RemoteAudioReceiverTelemetry audioTelemetry() const;
    /// The audio context most recently accepted from Core. Its encoder and
    /// off reason are present only when audioDetailNegotiated().
    std::optional<RemoteAudioContextMessage> acceptedAudioContext() const; // nullopt before the first accepted context and after stop()
    /// Core and this GUI agreed the minor-8 audio-context detail.
    bool audioDetailNegotiated() const; // d->client && d->client->remoteAudioStatusAvailable()
    /// Core and this GUI agreed the minor-9 spectrum grant report.
    bool spectrumGrantNegotiated() const; // d->client && d->client->spectrumGrantAvailable()
    /// This computer's remote audio status. It is recomputed whenever
    /// something it depends on changes; audioStatusChanged() fires only when
    /// the value does.
    RemoteAudioStatus audioStatus() const;
    /// Why a pan's display is below what it asked for (R-R3-08, R-R3-37):
    /// CoreBusy when the Core lowered its display budget because its
    /// computer is busy; None for a pan at its requested quality, a pan the
    /// Core did not give a reason for, or an unknown pan. Set on every
    /// budget replan with the pan's status line.
    DisplayBudgetReason panDisplayBudgetReason(const QString& panId) const;

    /// R-R3-23: the AppSettings key (stored on this computer, never on the
    /// Core) holding the remote audio choice, "Opus" or "Lossless".
    static constexpr const char* kAudioProfileSettingKey = "RemoteAudioProfile";
    /// The operator's remote audio choice, read from this computer's
    /// settings at construction and replayed on every connection.
    RemoteAudioProfile audioProfileChoice() const;
    /// Core and this GUI can use the choice: the minor-8 audio detail and a
    /// Core advertising audioProfileVersion 1 or later. Without it the GUI
    /// sends exactly today's media start and audio controls.
    bool audioProfileNegotiated() const;
    /// R-R3-35: this Core answers audio clock probes (audioClockVersion 1 or
    /// later). Without it no probe is sent and no delay is measured.
    bool audioClockNegotiated() const;
    /// R-R3-43: this Core can send a receiver's audio on its own stream:
    /// audioProfileNegotiated() and a Core advertising receiverAudioVersion
    /// 1 or later. Only then does the media start carry
    /// receiverAudioVersion and a receiver-audio request go out; otherwise
    /// the controls on the wire are exactly today's.
    bool receiverAudioNegotiated() const;
    /// R-R3-43: the stop reason a consumer gets from a Core that cannot send
    /// a receiver's audio. Plain words, shown as it is.
    static constexpr const char* kReceiverAudioUnavailableReason =
        "This Core cannot send a receiver's audio.";
    /// R-R3-43: an app on this computer (TCI, VAX) wants the Core's slice
    /// `sliceId` audio, without a speaker. Reference-counted per slice: the
    /// first sink asks the Core for the slice's stream, later sinks share
    /// it. The stream follows the one audio quality choice and runs while
    /// the speakers are muted. The sink stays registered across media
    /// reconnects until released; see IReceiverPcmSink for what it is told.
    /// Adding a sink that is already registered for the slice does nothing.
    /// GUI thread only.
    std::shared_ptr<RemoteTciAudioStage> requestReceiverAudio(int sliceId,
                                                               IReceiverPcmSink* sink);
    /// R-R3-43: undoes one requestReceiverAudio(). When it returns, `sink`
    /// is not called again for this slice; the last sink's release asks the
    /// Core to stop the stream. GUI thread only.
    void releaseReceiverAudio(int sliceId, IReceiverPcmSink* sink);
    bool remoteIqNegotiated() const;
    void requestRawIq(int sliceId);
    void releaseRawIq(int sliceId);
    /// R-R3-43: each wanted receiver stream's measured health, by slice id.
    QHash<int, RemoteAudioReceiverTelemetry> receiverAudioTelemetry() const;
    /// R-R3-45: this Core can send the headphones mix on its own stream:
    /// audioProfileNegotiated() and a Core advertising headphonesMixVersion
    /// 1 or later. Only then does the media start carry
    /// headphonesMixVersion (the Core then sends the speakers' mix alone on
    /// the main stream) and a headphones-audio request go out; otherwise
    /// the controls on the wire are exactly today's.
    bool headphonesMixNegotiated() const;
    /// Parity Task 28 (R-R3-49, A11): this window's media start told a Core
    /// at txDisplayVersion 1 that it takes the transmit display: its
    /// subscribes carry the transmit window and its contexts `transmit`.
    bool txDisplayNegotiated() const;
    /// Parity Task 29 (A11): the pan shows the transmit display while the
    /// Core is keyed (MoxDisplayController's rise and fall). While it does,
    /// the pan's receive frames are decoded but not drawn, so the receiver
    /// hearing its own transmitter never reaches the trace or the waterfall.
    /// With `displayMissing` (a Core below txDisplayVersion 1) the pan's
    /// status line says the Core does not send its transmit display.
    void setPanTransmitting(const QString& panId, bool transmitting, bool displayMissing);
    bool isPanTransmitting(const QString& panId) const;
    /// Parity Task 29: the transmitting pan's view moved while keyed; ask
    /// the Core again now rather than on the next planner pass.
    void refreshTransmitView();
    /// Parity Task 31 (A11): this window's display duplex (DUP), as
    /// MoxDisplayController applies it. On a Core at txDisplayVersion 3 (told
    /// 3 at media start), every subscribe carries `duplex` true while it is
    /// on, and a change asks the Core again at once. Below 3 nothing is sent.
    void setDisplayDuplex(bool on);
    bool displayDuplex() const;
    /// Whether this media start told the Core txDisplayVersion 3, so its
    /// subscribes may carry `duplex`.
    bool displayDuplexNegotiated() const;
    /// The Core's transmit context for the pan while it sends one (the
    /// newest accepted context marked `transmit`); none once a receive
    /// context replaced it. Media and transmit state travel on different
    /// channels, so the context can land before the window's own rise.
    std::optional<SpectrumContextMessage> heldTransmitContext(const QString& panId) const;
    /// Parity Task 32 (R-IOS-13, R-R3-49): this window's media start told a
    /// Core at txMonitorAudioVersion 1 that it takes the transmit monitor.
    bool txMonitorAudioNegotiated() const;
    /// Parity Task 32: where this window wants MON while it holds transmit
    /// (its MON output choice). Sent to the Core as monitor-audio at once
    /// with a media session that negotiated it, and again on each new media
    /// connection. Nothing is sent to an older Core. Speakers by default.
    void setTxMonitorRoute(TxMonitorRoute route);
    TxMonitorRoute txMonitorRoute() const;
    /// Parity Task 32: the monitor-audio-context most recently accepted (the
    /// route the Core applies), or empty before the first and after stop().
    std::optional<MonitorAudioMessage> acceptedMonitorContext() const;
    /// R-R3-45: why a receiver routed to the headphones is not heard, for
    /// the slice flags; empty when nothing is wrong on the Core's side or
    /// this computer's headphones device. Plain words, shown as they are.
    /// (No headphones set up on this computer is the flag's own notice.)
    QString headphonesProblem() const;
    /// R-R3-45: headphonesProblem() from a Core that cannot send the
    /// headphones mix.
    static constexpr const char* kHeadphonesMixUnavailableReason =
        "This Core cannot send audio for the headphones, so this receiver plays on "
        "the speakers.";
    /// R-R3-45: headphonesProblem() when the Core could not start the mix.
    static constexpr const char* kHeadphonesCoreCouldNotStart =
        "The Core could not start the audio for the headphones.";
    /// R-R3-45: headphonesProblem() after a headphones device fault on this
    /// computer, in the operator's words (the toast says the same).
    static QString headphonesFaultText(RemoteAudioReceiver::Fault fault);
    /// R-R3-45: the headphones mix's playback health on this computer.
    RemoteAudioReceiverTelemetry headphonesTelemetry() const;
    /// R-R3-45: the headphones-audio-context most recently accepted, or
    /// empty before the first and after stop().
    std::optional<RemoteAudioContextMessage> acceptedHeadphonesContext() const;
    /// R-R3-35: the measured audio delay now. measurable follows
    /// audioClockNegotiated() while a media session exists; estimate is
    /// present only while audio plays, echoes arrive (the newest younger
    /// than AudioClockEstimator::kEchoStaleNs) and the Core's capture
    /// belongs to the audio context being played.
    RemoteAudioDelayReport audioDelay() const;
    /// R-R3-35: how often a clock probe goes out while audio plays.
    static constexpr int kClockProbeIntervalMs = 1000;

    // ── iPhone app plan Task 36 (R-IOS-13): the microphone uplink ────────
    //
    // With a Core that takes this computer's microphone (micLineNegotiated)
    // the media start carries remoteTxVersion and the connection gets a
    // microphone line. The uplink runs (the capture helper open on the
    // microphone chosen in Audio > Devices, packets sent) while this window
    // holds transmit, its own key is down, or it has VOX armed (the Core's
    // VOX on, and this session permitted to transmit), and never otherwise:
    // no capture, no packet. Opus mono 20 ms frames; the lossless format
    // instead while the operator chose lossless audio and the line agreed
    // it. A program keying through this window's TCI server is sent in
    // place of the microphone while its audio comes.

    /// This Core takes this computer's microphone: it told this session
    /// remoteTxVersion 1 or later (it does so only for a hello declaring
    /// remoteTx). Without it the media start is today's.
    bool micLineNegotiated() const;
    /// Whether this window holds transmit, and whether its own key is down
    /// (its transmit button, or a program keying through its TCI server).
    /// Followed from the window's transmit client (StationClient's
    /// RemoteTransmitClient) from construction on.
    void setHoldsTransmit(bool holds);
    void setMicKeyDown(bool down);
    /// Whether the Core's VOX is on for this window: followed from the
    /// mirrored transmit.voxEnabled from construction on. The uplink then
    /// runs while this session is permitted to transmit.
    void setVoxArmed(bool armed);
    /// The uplink runs now.
    bool micUplinkRunning() const;
    /// Fix wave 2 (M8): this computer's microphone line to the Core is
    /// open (the media connection is ready and carries it), so VOX armed
    /// here can hear this computer. micLineChanged() follows it.
    bool micLineOpen() const;
    /// Packets sent on this media connection's microphone line.
    quint64 micPacketsSent() const;
    /// A program's transmit audio (TciServer::RemoteTransmit::audio): the
    /// left channel, at any rate (resampled to 48 kHz), sent in place of
    /// the microphone while it keeps coming.
    void pushProgramAudio(const float* samples, int frames, int channels, int sampleRateHz);
    /// How often the uplink moves captured audio to the line.
    static constexpr int kMicPumpIntervalMs = 10;
    /// How long after the last program audio the microphone is heard again.
    static constexpr int kProgramAudioHoldMs = 200;
    /// R-R3-37: how long a pan in budget mode may wait for the Core's first
    /// answer before it says "Waiting for the Core". A Core that answers
    /// within this (the usual case at session start) never flashes the line.
    static constexpr int kPanWaitingGraceMs = 2000;
    /// The display planner runs this often while a media session is live
    /// (and on every budget change and allocation answer).
    static constexpr int kPlannerIntervalMs = 100;
    /// Fix wave 3 (the several-devices re-review's Minor 3): how long pans
    /// must keep their widths before a resize that made them wider asks the
    /// Core again for what the operator wants: two of the planner's ticks
    /// (kPlannerIntervalMs), so a drag, whose steps come far faster, asks
    /// once when it stops, not at every step.
    static constexpr int kResizeSettleMs = 2 * kPlannerIntervalMs;
    /// R-R3-37: what the pan named `panId` was last told about its remote
    /// display, including its zoom-detail limit. The pan paints
    /// buildPanStatusText() of this.
    PanDisplayState panDisplayState(const QString& panId) const;

public slots:
    /// Ask Core for audio again: a new request, enabled per mute and radio
    /// state like every request. A no-op without a media session or while
    /// muted on this computer. It never clears a playback problem by itself.
    void retryAudio();
    /// Stores the choice on this computer and, with a media session, asks
    /// Core for it at once. Choosing again also ends an earlier fallback to
    /// Opus and starts a new link trial.
    void setAudioProfileChoice(NereusSDR::RemoteAudioProfile profile);
    /// Fix wave 3 (the several-devices design, ruling 9.3, asking again
    /// around the transmit holder): who holds transmit, as the Core's
    /// holder notification says (Task 34's `txState`: `holderEpoch`, which
    /// moves with every change of holder, a release included, and
    /// `holderAway`). A change of either asks again for the displays the
    /// operator wants: this device became the present holder (rule 1 gives
    /// a holder its whole request only once it asks for it), the holder
    /// changed, or a holder left or went away (the others' equal shares,
    /// rule 3, come back only once they ask). These are operator events and
    /// rule 1 is not symmetric, so asking on them never loops. The same
    /// values again ask nothing. Epoch 0, not away, is the unheld start.
    ///
    /// The client's `txState` (`holderEpoch`, `holderAway`, fix wave I4)
    /// calls it whenever the Core's holder changes.
    void setTransmitHolder(quint64 holderEpoch, bool holderAway);
    /// iPhone app plan Task 29 (R-IOS-16; the remote media control
    /// document, "Replacing the media connection"): asks the Core for a new
    /// media connection beside the current one, which takes over without a
    /// gap once audio runs on both. What the session calls when it moved to
    /// a better path (StationClient::pathChanged). False when it cannot now:
    /// no ready media, a Core without mediaReplaceVersion, one already
    /// under way, or this window keyed, VOX armed or the Core on the air.
    bool replaceConnection();
    /// The direct media ladder: asks a Core with mediaDirectVersion for a
    /// direct-only replacement (STUN and host candidates, no tunnel or
    /// relay; the replace names "mediaDirectVersion": 1). False as for
    /// replaceConnection, and also while this window is keyed, has VOX
    /// armed or the Core is on the air. A refused one is not retried until
    /// the next step of its schedule.
    bool upgradeToDirectConnection();
    /// The direct media ladder: the delay of the next direct-only replace
    /// armed while media rides the tunnel (a PathRacer::kUpgradeRetryMs
    /// step), or -1 when none is armed.
    int directUpgradeDelayMs() const;
    /// The direct media ladder: one step of the direct-only schedule (what
    /// its timer runs; public so tests drive it without waiting).
    void runDirectUpgradeStep();
    /// The direct media ladder: the no-packets fallback's check (what the
    /// stall timer runs; public so tests drive it with an injected clock).
    void checkMediaSilence();

public:
    /// Test only: whether an audio restart is waiting on its backoff step
    /// (so a test can order a media move against it; no production caller).
    bool audioRestartPendingForTest() const;
    /// Test only: while held, an audio restart's backoff step that comes
    /// due waits instead of running; releasing runs it (through the same
    /// fence as ever). No production caller.
    void holdAudioRestartForTest(bool held);
    /// Test only: whether a backoff step came due while held.
    bool audioRestartStepHeldForTest() const;
    /// iPhone app plan Task 29 fix wave (review Important 1): media follows
    /// every move of the session. A move marks a replacement pending; it
    /// starts as soon as it can (the media connection ready, unkeyed, VOX
    /// disarmed, the Core back on receive), retried every
    /// kReplaceRetryMs while pending, and again after the Core refused one
    /// because it was transmitting (DaemonMediaController::
    /// kReplaceTransmittingReason; no other refusal re-arms it). A new
    /// connection that cannot start is tried kMaxReplaceRearms times a
    /// move.
    static constexpr int kReplaceRetryMs = 500;
    /// Task 29 step 2b (fast failure detection): on a path through a relay
    /// or the WebSocket tunnel, audio that stops coming for this long while
    /// it plays is a dead media connection: it is started again at once
    /// (recoveryRequested), rather than after ICE's 30 s consent check.
    static constexpr int kMediaStallMs = 3000;
    static constexpr int kMaxReplaceRearms = 3;
    /// The direct media ladder: on a direct path (not the tunnel or a
    /// relay), no audio or display packet for this long while control still
    /// runs moves media back to the tunnel alone (a three-field replace
    /// whose connection offers only the tunnel's candidate), and the
    /// direct-only schedule starts over. It runs once per silence; if media
    /// has not returned this long after that replace finishes, the window
    /// asks for recovery (recoveryRequested) instead of replacing again.
    /// 5000 ms by JJ's ruling (2026-09-29): long enough that a short gap on
    /// a working direct path does not move media.
    static constexpr int kDirectMediaSilenceFallbackMs = 5000;
    bool replacePending() const;
    /// iPhone app plan Task 29: the media connection's id now, and whether
    /// a replacement is under way (for the window's diagnostics and tests).
    QString mediaConnectionId() const;
    /// The active primary peer only, fenced to this client's current session.
    std::optional<NetworkPathSnapshot> currentNetworkPath() const;
    bool replacingConnection() const;
    /// Copies of audio packets dropped while two connections carried them.
    quint64 duplicateAudioDropped() const;

signals:
    void networkPathChanged();
    void rawIqBlock(int sliceId, int sampleRateHz, const QVector<float>& samples);
    void rawIqRate(int sliceId, int sampleRateHz);
    void rawIqUnavailable(int sliceId, const QString& reason);
    void recoveryRequested(quint32 expectedEpoch, const QString& reason);
    void errorOccurred(const QString& reason);
    void displayFrameReceived(quint32 endpointId);
    /// Parity Task 28: the Core switched the pan to its transmit display
    /// (context `transmit` true): the transmit analyzer's view, centred on
    /// the carrier. The pan's receive context stands; drawing the transmit
    /// display is the MOX display controller's (Task 29).
    void transmitContextReceived(const QString& panId,
                                 const NereusSDR::SpectrumContextMessage& context);
    /// Parity Task 28: one transmit display frame for the pan, decoded.
    void transmitFrameReceived(const QString& panId, const NereusSDR::DisplayCodecFrame& frame);
    void miniDisplayFrame(int sliceId, const QVector<float>& traceDbm,
                          const QVector<float>& waterfallDbm, double centreHz,
                          double spanHz, bool transmit, bool waterfallAdvance);
    void miniDisplayUnavailable(int sliceId);
    /// Once per accepted audio context, after playback was started or
    /// stopped for it. A malformed or stale context emits nothing.
    void audioContextAccepted();
    void audioStatusChanged();
    /// R-R3-45: headphonesProblem() changed.
    void headphonesProblemChanged(const QString& problem);
    /// Fix wave 2 (M8): micLineOpen() changed.
    void micLineChanged(bool open);

private:
    struct Private;
    std::unique_ptr<Private> d;
    void start();
    void stop();
    void requestRecovery(quint32 expectedEpoch, const QString& reason);
    void settleWithoutRetry(quint32 expectedEpoch, const QString& reason);
    void refreshSubscriptions();
    void refreshBudgetSubscriptions();
    void refreshLegacyMiniSubscriptions();
    // Parity Task 32: the monitor-audio request and its answer.
    void requestMonitorAudio();
    void sendIqRequest(int sliceId, bool enabled);
    void receiveIqContext(const QJsonObject& payload);
    void receiveIqFrame(const QByteArray& message);
    void failIqStream(int sliceId, const QString& reason);
    void receiveMonitorAudioContext(const QJsonObject& payload);
    /// Parity Task 18 (B3.5): a widget in `keepHistory` is taking the
    /// display of another slice on the same receiver, so its drawn
    /// waterfall, rewind history and 3D stack stay; only the live frame
    /// is dropped until the new display arrives.
    bool retireSubscriptions(const QList<quint32>& endpointIds,
                             const QSet<SpectrumWidget*>& keepHistory = {});
    /// Fix wave 2 (Important 2): unsubscribes an endpoint the Core refused
    /// (never accepted) after its binding is dropped, the next revision
    /// after `lastRevision`. False when the session ended while sending.
    bool sendRefusedRelease(quint32 endpointId, quint32 lastRevision);
    void receiveAllocationResult(const QJsonObject& payload);
    void setPanStatus(const QString& panId, const PanDisplayState& status);
    PanDisplayState statusWithGrant(const QString& panId, PanDisplayState status) const;
    void refreshPanGrantStatus(const QString& panId);
    PanDisplayState perPanRefusalStatus(const QString& panId) const;
    void refreshCtunState();
    // R-R3-18/19: a C-Tune centre gesture from one pan, at most one request
    // in flight per stream, never a zoom on a stream other receivers share.
    void requestCentreFromGesture(quint32 endpointId, double centreHz);
    void finishCentreRequest(int sliceId, quint64 streamEpoch, bool accepted);
    void receiveControl(const QJsonObject& payload, quint32 epoch);
    void receiveDisplay(const QByteArray& packet);
    void receiveDisplayExtras(const QByteArray& packet);
    void reportDisplayDrops();
    void requestKeyframe(quint32 endpointId);
    void requestAudio();
    void sendReceiverAudioRequest(int sliceId, bool enabled);
    void requestWantedReceiverAudio();
    void receiveReceiverAudioContext(const QJsonObject& payload);
    void notifyReceiverStopped(int sliceId, const QString& reason);
    void onReceiverRestart(int sliceId, RemoteAudioReceiver* receiver, const QString& reason,
                           RemoteAudioReceiver::Fault fault);
    void onReceiverError(int sliceId, RemoteAudioReceiver* receiver, const QString& reason,
                         RemoteAudioReceiver::Fault fault);
    void reconcileLinkTrial();
    // R-R3-45: the headphones mix.
    bool headphonesWanted() const;
    void requestHeadphonesAudio();
    void receiveHeadphonesAudioContext(const QJsonObject& payload);
    void onHeadphonesRestart(const QString& reason, RemoteAudioReceiver::Fault fault);
    void onHeadphonesError(const QString& reason, RemoteAudioReceiver::Fault fault);
    void setHeadphonesProblem(const QString& problem);
    void refreshAudioStatus();
    void checkLosslessLink();
    void fallBackToOpus(const QString& cause);
    void sendClockProbe();
    void reconcileClockProbe();
    // R-R3-21 / R-R3-08: display presentation on the audio's clock.
    std::optional<qint64> displayMapNs();
    void presentDueDisplay();
    void scheduleDisplayPresentation(std::optional<qint64> mapNs);
    // Task 36: the microphone uplink.
    bool micUplinkWanted() const;
    void reconcileMicUplink();
    // Fix wave 2 (M8): emits micLineChanged when micLineOpen() moved.
    void noteMicLine();
    /// Sends the whole packets `pending` holds and keeps the rest.
    void sendMicAudio(std::vector<float>& pending);
    void receiveClockEcho(const QJsonObject& payload, qint64 receivedNs);
    bool send(QJsonObject payload);
    // iPhone app plan Task 29: a peer's callbacks as the current peer, one
    // audio packet from any peer to its stream, and the replacement's end.
    void connectPeer(MediaPeer* peer, quint32 epoch);
    void routeRtp(const QByteArray& packet, const MediaPeer* from);
    void deliverRtp(const QByteArray& packet);
    void receiveReplacementControl(const QJsonObject& payload);
    void promoteReplacement();
    void dropReplacement(const QString& why);
    // The direct media ladder: a plain replace, a direct-only one, and the
    // silence fallback's plain replace onto the tunnel alone.
    enum class ReplaceKind { Normal, Direct, TunnelFallback };
    // carriesFoldedMove: the replace carries a session move folded into a
    // waiting fallback; it is recorded before the replace is sent, so a
    // refusal that arrives during the send keeps the move.
    bool startReplacement(ReplaceKind kind, bool carriesFoldedMove = false);
    // Media arrived while a refused fallback waits to be retried.
    void endWaitingFallback();
    void updateDirectUpgrade(bool viaTunnel);
    void markReplacePending();
    void tryPendingReplace();
    std::optional<IceConfiguration> mediaIceConfiguration();
    void retireOldPeer();
};
} // namespace NereusSDR
