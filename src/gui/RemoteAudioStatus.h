// =================================================================
// src/gui/RemoteAudioStatus.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The GUI's persistent remote audio
// status (R-R3-23): the status value, the pure state derivation, the rule
// that clears a recorded playback failure, and the plain-English wording
// the Core connection panel and title bar show.  It owns no session,
// device, receiver or timer; RemoteMediaController feeds it.  It also holds
// the lossless link trial's rule (RemoteAudioLinkTrial), as pure logic.
// =================================================================

#pragma once

#include "core/session/media/OpusAudioCodec.h"
#include "core/session/media/PcmAudioCodec.h"
#include "core/session/media/RemoteAudioContext.h"
#include "core/session/media/RemoteAudioReceiver.h"
#include "gui/AudioClockEstimator.h"
#include "gui/RemoteReceiverAudioNote.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QtGlobal>

#include <deque>
#include <map>
#include <optional>
#include <vector>

namespace NereusSDR {

/// R-R3-23: why the Core runs Opus although this computer chose Lossless.
enum class RemoteAudioQualityReason {
    CoreCannotSend,        // the Core has no lossless audio (an older Core)
    CoreNotAllowed,        // the Core refused: its setting denies lossless
    ConnectionUnavailable, // the Core refused: this connection did not agree it
    NetworkTooSlow,        // this computer's link trial failed
};

/// R-R3-43: one receiver's audio stream to apps on this computer (TCI,
/// VAX), apart from the speakers' mix.
struct RemoteReceiverAudioStatus {
    enum class State {
        Waiting,   // an app asked; the Core has not started it yet
        Receiving, // the Core sends it and this computer receives it
        Stopped,   // stopped, with stopReason
    };
    int sliceId = -1;
    State state = State::Waiting;
    /// The Core's wire reason or this computer's sentence; shown through
    /// OperatorReasonText::forDisplay(). Set only when Stopped.
    QString stopReason;
    /// What the Core runs for this receiver; absent before it says.
    std::optional<RemoteAudioProfile> runningProfile;
    /// The Opus encoder the Core reports for this receiver while it runs
    /// Opus (its real rate, R-R3-43); absent for lossless or before it says.
    std::optional<OpusEncoderProfile> encoder;
    friend bool operator==(const RemoteReceiverAudioStatus&,
                           const RemoteReceiverAudioStatus&) = default;
};

struct RemoteAudioStatus {
    enum class State {
        NotConnected,     // no remote media session
        WaitingForAudio,  // requested, awaiting Core or the media link
        MutedHere,        // master mute on this computer
        RadioOffline,     // station radio not connected
        CoreCouldNotStart,// Core reported encoder-unavailable
        Starting,         // Core sending, speaker progress not yet seen
        Playing,          // speaker consuming audio now
        Reconnecting,     // interruption, automatic retry scheduled
        PlaybackProblem,   // local output/decoder failure, persists
    };
    State state = State::NotConnected;
    bool detailNegotiated = false;               // Core reports codec detail
    std::optional<OpusEncoderProfile> encoder;   // current accepted context only
    QString selectedOutput;                      // selected speakers device name, "System default" when default
    std::optional<RemoteAudioReceiver::Fault> problem; // persistent local fault
    bool retryAvailable = false;
    // R-R3-23 audio quality. chosenProfile is the operator's choice, stored
    // on this computer. profileChoiceAvailable: this Core and this computer
    // agreed the lossless profile. runningProfile is what the Core reports
    // it runs (Opus for a Core that reports no profile); absent before the
    // first accepted context. losslessEncoder is set only while lossless
    // audio is on. qualityReason says why Lossless was chosen and Opus runs.
    RemoteAudioProfile chosenProfile = RemoteAudioProfile::Opus;
    bool profileChoiceAvailable = false;
    std::optional<RemoteAudioProfile> runningProfile;
    std::optional<PcmEncoderProfile> losslessEncoder;
    std::optional<RemoteAudioQualityReason> qualityReason;
    // R-R3-43: every receiver an app on this computer listens to, by slice
    // id. They follow the one audio quality choice and the one link trial;
    // the speakers' mute does not stop them.
    QList<RemoteReceiverAudioStatus> receivers;
    friend bool operator==(const RemoteAudioStatus&, const RemoteAudioStatus&) = default;
};

struct RemoteAudioStatusInputs {
    bool mediaSession = false;
    bool muted = false;
    bool radioConnected = false;
    std::optional<RemoteAudioContextMessage> context;
    bool receiverRunning = false;
    bool playing = false;       // running, decoded > 0, device progress younger than 500 ms
    bool restarting = false;
    std::optional<RemoteAudioReceiver::Fault> problem;
};

/// First match wins: no media session, muted here, a persistent local
/// problem, the station radio offline (this GUI's mirror of it, or Core's
/// reason), Core's encoder unavailable, an automatic retry, no enabled
/// context yet, a receiver not yet running, the speaker consuming audio,
/// and otherwise starting.
RemoteAudioStatus::State deriveRemoteAudioState(const RemoteAudioStatusInputs& in);
QString remoteAudioHeadline(RemoteAudioStatus::State state);   // panel status words
QString remoteAudioBannerWord(RemoteAudioStatus::State state); // title bar words
/// The operator's wording for a playback fault. Only the local output and
/// decoder faults become persistent problems; every interruption the
/// receiver recovers from by itself reads "Audio was interrupted."
QString remoteAudioProblemText(RemoteAudioReceiver::Fault fault);
/// Core's reported encoder settings, labelled as a target; "Not reported by
/// this Core" when the detail was not negotiated, and "Audio is off" when it
/// was but the current context carries no encoder. Lossless reads "Lossless
/// stereo, 16-bit, 1536 kbit/s, 4 ms packets".
QString remoteAudioCodecText(const RemoteAudioStatus& status);
/// "Opus" or "Lossless".
QString remoteAudioProfileName(RemoteAudioProfile profile);
/// The operator's sentence for a quality reason, e.g. "This Core does not
/// allow lossless audio."
QString remoteAudioQualityReasonText(RemoteAudioQualityReason reason);
/// The "Audio quality" value: the profile the Core runs, or, before it
/// reports one, the choice with "(chosen)".
QString remoteAudioQualityText(const RemoteAudioStatus& status);
/// R-R3-43 / R-R3-44: whether the receiver streams that feed apps on this
/// computer (VAX, TCI) are Opus rather than lossless. They follow the one
/// quality choice and its fallback, so: false with no remote media; true
/// when Lossless was chosen but Opus runs (qualityReason); otherwise what a
/// running receiver stream uses, else what the Core runs for the speakers,
/// else the choice itself.
bool remoteReceiverAudioIsCompressed(const RemoteAudioStatus& status);

/// R-R3-43 / R-R3-44: which note Setup > Audio > VAX shows about the
/// receiver streams (see RemoteReceiverAudioNote.h). None whenever
/// receiverAudioNegotiated is false: the Core sends no receiver streams, so
/// VAX is not fed from it.
RemoteReceiverAudioNote remoteReceiverAudioNote(const RemoteAudioStatus& status,
                                                bool receiverAudioNegotiated);

/// The Core connection panel's "Remote audio" section, one line per item
/// joined with '\n': headline, problem (only when status.problem is set),
/// the audio quality and its reason (only when the Core offers the choice
/// or Lossless was chosen, so an older Core's section reads as before),
/// codec, output, then four measurement lines (arrival jitter, missing
/// packets, gaps filled, speaker buffer) each showing "not measured" (or
/// "none received" for missing packets) until playback has a value. The
/// four measurement lines are omitted together when status.state is
/// NotConnected, MutedHere or RadioOffline. Numbers are integers; jitter and
/// the speaker buffer are rounded.
/// R-R3-35: the measured audio delay as this computer can report it.
/// measurable: this Core answers clock probes (audioClockVersion), so the
/// delay can be measured on this connection; an older Core cannot, and
/// every place that shows the delay then reads exactly as before.
struct RemoteAudioDelayReport {
    bool measurable = false;
    std::optional<AudioDelayEstimate> estimate;
    bool operator==(const RemoteAudioDelayReport&) const = default;
};
/// "85 ms ± 1 ms", adding ", not counting the speaker device" when the
/// device's own latency is unknown. The ± is the accuracy; the value is
/// never half a round trip.
QString remoteAudioDelayText(const AudioDelayEstimate& estimate);
/// The delivery delay (Core to this computer's player), "62 ms ± 1 ms";
/// empty when not measured.
QString remoteAudioDeliveryText(const AudioDelayEstimate& estimate);
/// With delay.measurable and a health section, a line "Audio delay: ..."
/// (or "Audio delay: not measured") follows the speaker buffer line.
/// R-R3-43: then, for each of status.receivers, a line "Receiver B for
/// apps: ..." with its state (and the quality it runs, or why it stopped),
/// and while it is receiving a line with its measured arrival jitter,
/// missing packets and gaps filled, from receiverPlayback by slice id. With
/// no receivers the text is exactly as before.
QString formatRemoteAudioDetails(const RemoteAudioStatus& status,
                                 const RemoteAudioReceiverTelemetry& playback,
                                 const RemoteAudioDelayReport& delay = {},
                                 const QHash<int, RemoteAudioReceiverTelemetry>& receiverPlayback = {});
/// R-R3-43: the receiver line's words for one stream, e.g. "Receiving,
/// Lossless" or "Stopped. This receiver was removed on the Core."
QString remoteReceiverAudioStateText(const RemoteReceiverAudioStatus& receiver);

/// The identity a persistent playback failure is recorded against: the
/// session (epoch and connection), the accepted audio context it happened
/// in, and the receiver generation that was playing, or trying to play, it.
struct RemoteAudioFailure {
    RemoteAudioReceiver::Fault fault = RemoteAudioReceiver::Fault::SpeakerOpenFailed;
    quint32 epoch = 0;
    QString connectionId;
    quint32 contextGeneration = 0;
    quint64 receiverGeneration = 0;
};

/// Matching recovery, the only thing that clears a recorded failure: in the
/// same epoch and connection, an accepted context newer than the failed one
/// (wrap-aware), played by a running receiver of a later generation whose
/// speaker has consumed audio. Mute, unmute, a device change, a disabled
/// context, a restart or time alone never satisfy it.
bool remoteAudioFailureRecovered(const RemoteAudioFailure& failure, quint32 epoch,
                                 const QString& connectionId,
                                 const std::optional<RemoteAudioContextMessage>& context,
                                 const RemoteAudioReceiverTelemetry& playback);

/// R-R3-23: this computer's own check that the network carries lossless
/// audio, owned by the app because the Core cannot see what arrives here.
/// Lossless is about 1.6 Mbit/s at 250 packets a second, where Opus is 24
/// to 48 kbit/s at 25; a link that cannot carry it loses packets (each one
/// 4 ms of silence), and a queue that cannot drain restarts. Opus, whose
/// concealment hides isolated loss and whose rate fits almost any link,
/// is the better sound on such a link.
///
/// The trial begins when lossless playback begins. Its first window closes
/// after kWindowMs; later windows follow back to back while lossless plays.
/// Loss in a window is the larger of missing packets over expected packets
/// and filled gaps over played packets.
class RemoteAudioLinkTrial {
public:
    /// About 5 s: some 1250 lossless packets, enough that 2% (25 packets) is
    /// a real pattern and not one unlucky burst, yet short enough that an
    /// unusable link falls back before the operator gives up on it.
    static constexpr qint64 kWindowMs = 5'000;
    /// The first window fails above 2% loss: 25 gaps of 4 ms in 5 s, one
    /// every 200 ms, which is audible and costs digital-mode decodes the
    /// lossless choice was meant to win.
    static constexpr double kTrialLossLimit = 0.02;
    /// After that, only sustained loss fails: every one of three windows in
    /// a row (15 s) above 1%. One bad window, a passing burst of other
    /// traffic, never forces Opus on a link that carries lossless otherwise.
    static constexpr double kSustainedLossLimit = 0.01;
    static constexpr int kSustainedWindows = 3;
    /// Interruptions of the stream: a restart for no packets, and (R-R3-21)
    /// an arrival burst, a stream gap or a stall the receiver now rides
    /// through on this computer (its linkInterruptions, read from each
    /// sample, at most one a sample). Any one during the first window fails
    /// it, because a link that stalls within 5 s cannot carry the stream;
    /// later, the second within kRestartSpanMs fails, one being a passing
    /// event.
    static constexpr int kTrialRestartLimit = 1;
    static constexpr int kSustainedRestartLimit = 2;
    static constexpr qint64 kRestartSpanMs = 60'000;

    enum class Verdict { Continue, Failed };

    /// R-R3-43: one lossless stream's sample. `stream` tells the streams
    /// apart (the speakers' mix and each receiver stream).
    struct StreamSample {
        int stream = 0;
        RemoteAudioReceiverTelemetry playback;
    };

    void begin(qint64 nowMs);
    void end();
    bool active() const { return m_active; }
    /// A playback sample. Counters are per receiver generation; a new
    /// generation starts from zero. Samples from a stopped receiver are
    /// ignored (the restart itself goes to noteInterruption()).
    Verdict observe(qint64 nowMs, const RemoteAudioReceiverTelemetry& playback);
    /// R-R3-43: a sample of every lossless stream at once. Each stream's
    /// counters are kept apart (its own generation and base) and summed
    /// into the one window, so the trial judges the link by all the
    /// lossless audio it carries. A stream missing from a sample is
    /// forgotten; if it comes back it counts from zero.
    Verdict observe(qint64 nowMs, const std::vector<StreamSample>& streams);
    /// One restart the receiver asked for because audio arrived badly.
    Verdict noteInterruption(qint64 nowMs);
    /// The loss of the last closed window, for the log; absent before one.
    std::optional<double> lastWindowLoss() const { return m_lastWindowLoss; }
    /// R-R3-21: the last Failed verdict came from interruptions, not loss.
    bool failedOnInterruptions() const { return m_failedOnInterruptions; }

private:
    struct Counters {
        quint64 expected = 0;
        quint64 missing = 0;
        quint64 concealed = 0;
        quint64 played = 0;
        quint64 interruptions = 0; // R-R3-21: linkInterruptions
    };
    struct StreamBase {
        quint64 generation = 0;
        Counters base;
    };
    bool m_active = false;
    qint64 m_windowStartMs = 0;
    int m_closedWindows = 0;
    int m_badWindows = 0;
    std::map<int, StreamBase> m_streams;
    Counters m_window;
    std::deque<qint64> m_interruptions;
    std::optional<double> m_lastWindowLoss;
    bool m_failedOnInterruptions = false;
};

} // namespace NereusSDR
