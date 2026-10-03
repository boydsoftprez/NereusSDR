#pragma once
// =================================================================
// src/core/session/RemoteTransmitClient.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. iPhone app plan, the desktop remote
// window's transmit (R-IOS-13, R-R3-42).
//
// A remote window keys the Core's radio with the transmit verbs (link
// section 18.6), never with its own MoxController: MOX (the TX applet's
// button and the container's), TUNE and two-tone send tx.key
// {trigger:"screen"}, tx.unkey {epoch}, tx.tune {on} and tx.twoTone {on};
// a program keying through the window's TCI server sends tx.key
// {trigger:"tci"} and its tx.unkey. Each goes out three times as the same
// command (one id), the copies rule; the Core acts on the first and
// answers every copy, so only the first answer per id counts here.
//
// The key's epoch comes from the Core's answer. A release names it; a
// release before the answer names 4294967295 (kReleaseAnyEpoch), which is
// never older than the device's live key, so a key accepted just before
// the release still stops. After the Core ends a key on its own (the
// mirrored `transmitting` falls), the next press is a new command.
//
// The window's microphone uplink follows two inputs from here: the key is
// down (a press not yet released, waiting for its answer or keyed) and the
// window holds transmit (its key is on at the Core). The window cannot see
// that it holds transmit unkeyed until the holder reaches it (Task 39's
// txState and Task 77); until then holding follows its own accepted key.
//
// Task 37 (R-IOS-13; remote design section 12.1): while this window has a
// key down or on, asked TUNE or two-tone on, or has VOX armed, it sends
// tx.keepalive {sequence, epoch} every 100 ms, so the Core's watchdog
// knows the link is alive. Each goes once: on the media connection's "tx"
// data channel when that is open (unordered, never retransmitted), and
// otherwise on the session. The sequence starts at 1 and rises by one with
// every keepalive, and never starts again while the link lasts; the epoch
// is the window's key's (from its answer), or 4294967295 before the answer
// and for a key that has none (VOX, TUNE, two-tone), which the Core counts
// as never older.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25 - Created for the desktop remote window's transmit
//                (R-IOS-13, R-R3-42). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 37 (R-IOS-13): the keepalive for the
//                Core's transmit watchdog. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave: M7 coreStopped. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: Transmit group fix wave 2, the re-review's minors:
//               holderTransferring true while keys are refused for a
//               transfer's reasons (a dropped holder's fence, a transfer
//               ended with MOX on); stopEpoch names the key a stop ended so
//               a newer key is never ended by it; VOX at the Core listens
//               only to the device that armed it; the window says why MOX
//               and TUNE wait while another device holds. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13):
//               setTunerTune. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: TX rulings (item 1): screenReleasePending and
//               tuneReleasePending, the operator's MOX or TUNE let go while
//               the Core's confirmation is on its way, so the next press
//               keys. J.J. Boyd (KG4VCF), with AI-assisted implementation
//               via Anthropic Claude Code.
//   2026-09-30: TX rulings review (I-1): the release memory is bounded,
//               forgotten on a failed release, and ignored while anything
//               else of this window keeps the radio on the air. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-10-01: Tune-ended lane: tunerTuneEnded, the Core's notice that
//               this window's Tuner Genius autotune ended before its
//               carrier keyed. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>

#include "core/session/MirrorSchema.h"

namespace NereusSDR {

class RemoteTransmitClient final : public QObject {
    Q_OBJECT

public:
    /// Sends `verb` with `arguments` kCopies times under one new command
    /// id and returns the id, or 0 when nothing could be sent.
    using Sender = std::function<quint32(const QByteArray& verb,
                                         const QList<MirrorUpdate>& arguments)>;

    /// The Core's answer to a key, for a program keying through the TCI
    /// server (TciServer::RemoteKeyAnswer's fields).
    struct Answer {
        bool accepted{false};
        quint32 epoch{0};
        QString reason;
        QString code;
        QString fix;
    };

    static constexpr int kCopies = 3;
    /// A release sent before the key's epoch is known.
    static constexpr quint32 kReleaseAnyEpoch = 0xFFFFFFFFu;
    static constexpr const char* kScreenTrigger = "screen";
    static constexpr const char* kProgramTrigger = "tci";
    /// Shown when a press finds no link to the Core.
    static constexpr const char* kNoLinkReason =
        "This computer is not connected to the Core, so it cannot transmit.";
    static constexpr const char* kReleaseFailedReason =
        "The transmit release could not be confirmed. Reconnect to the Core before transmitting again.";

    explicit RemoteTransmitClient(Sender sender, QObject* parent = nullptr);

    // ---- Task 37: the keepalive ----
    /// Sends one keepalive; true when it went.
    using KeepaliveSender = std::function<bool(quint64 sequence, quint32 epoch)>;
    /// How often a keepalive goes (RemoteTxWatchdog::kKeepaliveIntervalMs).
    static constexpr int kKeepaliveIntervalMs = 100;
    /// TX rulings (item 1): once the Core accepted a release and this
    /// window does not read it transmitting, how long a late `transmitting`
    /// from the key let go may still arrive: a few of the Core's delta
    /// flushes (StationServer::kDefaultDeltaFlushMs, 50 ms), after which
    /// the memory of the release is forgotten.
    static constexpr int kReleaseConfirmGraceMs = 250;
    /// The session's tx.keepalive (sent once, never three times).
    void setSessionKeepalive(KeepaliveSender sender) { m_sessionKeepalive = std::move(sender); }
    /// The media connection's "tx" data channel; tried first while no
    /// release waits for its primary-session delivery barrier. Unset, or
    /// false (no channel open), the session carries it.
    void setChannelKeepalive(KeepaliveSender sender) { m_channelKeepalive = std::move(sender); }
    /// Independent watch socket: one copy of this timer's sequence/epoch.
    /// Its result never replaces the existing channel-or-primary copy.
    void setAuxiliaryKeepalive(KeepaliveSender sender)
    {
        m_auxiliaryKeepalive = std::move(sender);
    }
    /// The Core's VOX is on (the window's mirrored transmit.voxEnabled).
    void setVoxArmed(bool armed);
    /// Keepalives are going out now.
    bool keepaliveRunning() const { return m_keepaliveTimer.isActive(); }
    /// The epoch the next keepalive names.
    quint32 keepaliveEpoch() const;
    /// Keepalives sent on the channel and on the session since the link
    /// came up.
    quint64 channelKeepalivesSent() const { return m_channelKeepalives; }
    quint64 sessionKeepalivesSent() const { return m_sessionKeepalives; }
    quint64 auxiliaryKeepalivesSent() const { return m_auxiliaryKeepalives; }
    /// One keepalive now (the timer's; public for tests).
    void keepaliveTick();

    /// The Core takes this window's keys: the session is up and the Core
    /// told it remoteTxVersion 1 or later. Going false forgets every key
    /// (the Core unkeys a device whose link drops, and never re-keys).
    void setAvailable(bool available);
    bool available() const { return m_available; }

    // ---- The operator's controls ("screen") ----
    /// MOX pressed (true) or released (false).
    void setScreenKey(bool down);
    void setTune(bool on);
    void setTwoTone(bool on);
    /// iPhone app plan Task 77: the Core's Tuner Genius autotune
    /// (tx.tunerTune {on}); the carrier it keys is this window's, watched
    /// like TUNE's.
    void setTunerTune(bool on);
    /// Tune-ended lane: the Core ended this window's Tuner Genius autotune
    /// before its carrier keyed (its notice tuneEnded). The TUNE asked for
    /// is over: the next press asks it on again, and its keepalives stop.
    void tunerTuneEnded();

    // ---- A program through this window's TCI server ("tci") ----
    void keyForProgram(std::function<void(const Answer&)> answer);
    void unkeyForProgram(quint32 epoch);

    // ---- From the link ----
    /// Every result of a transmit verb (the copies included).
    void commandFinished(quint32 commandId, const QByteArray& verb, bool accepted,
                         const QString& reason, const QList<MirrorUpdate>& values);
    /// The Core's real transmit state (RadioModel `transmitting`).
    void setCoreTransmitting(bool on);
    /// Fix wave M7: the Core recorded a stop (`txState`'s stopSerial moved)
    /// and says nothing is keyed now (`keyed`): a key of this window's that
    /// is on ended at the Core, even one so short the mirrored
    /// `transmitting` never rose. The next press is a new command.
    /// Fix wave 2 (the M7 race): `stopEpoch` (txState's stopEpoch) names
    /// the key the stop ended; a key of this window's with a newer epoch
    /// (pressed after the stop, before the stop's update arrived) is left
    /// on. With no epoch (0, an older Core) a key on at the Core now
    /// (`coreKeyed`) is not this stop's.
    void coreStopped(quint32 stopSerial, bool coreKeyed, quint32 stopEpoch = 0);

    /// The press is down (waiting for its answer, or keyed).
    bool micKeyDown() const;
    /// This window's key is on at the Core.
    bool holdsTransmit() const;
    /// The operator's MOX key is on or waiting.
    bool screenKeyDown() const { return m_screen.phase != Phase::Idle; }
    /// The epoch of this window's key, or 0.
    quint32 screenEpoch() const { return m_screen.epoch; }
    /// This window asked for TUNE on and has not asked it off since (nor
    /// been refused): its off must go even before the Core's TUNE shows.
    bool tuneAsked() const { return m_tuneAsked; }
    /// TX rulings (item 1): the operator let go of MOX (or TUNE) and the
    /// Core has not said it stopped: its `transmitting` may still read
    /// true, from this key, for a moment. The next press is a new key.
    /// It ends when the Core reads not transmitting (its `transmitting`
    /// going false, a key refused, a stop while not transmitting), when
    /// the release is refused or cannot be sent, or kReleaseConfirmGraceMs
    /// after the release or its acceptance, whatever the Core reads.
    /// Review I-1: never while a release failed (the sticky failure), and
    /// never while anything else of this window may keep the radio on the
    /// air (VOX armed, two-tone, a program key, TUNE for MOX, MOX for
    /// TUNE): then a press is the plain toggle, which unkeys.
    bool screenReleasePending() const;
    bool tuneReleasePending() const;

signals:
    /// The Core refused the operator's press (or a release), in its words.
    void refused(const QString& reason, const QString& code, const QString& fix);
    void micKeyDownChanged(bool down);
    void holdsTransmitChanged(bool holds);

private:
    enum class Phase { Idle, Waiting, On };
    struct Key {
        Phase phase{Phase::Idle};
        quint32 commandId{0};
        quint32 epoch{0};
        bool sawTransmitting{false};
    };
    enum class Kind { ScreenKey, ProgramKey, Release, Tune, TwoTone };
    struct Pending {
        Kind kind;
        QByteArray verb;
        bool releaseIntent;
    };

    quint32 send(const QByteArray& verb, const QList<MirrorUpdate>& arguments, Kind kind,
                 bool releaseIntent = false);
    void release(quint32 epoch);
    void reset();
    void publish();
    static quint32 epochOf(const QList<MirrorUpdate>& values);

    Sender m_sender;
    bool m_available{false};
    Key m_screen;
    Key m_program;
    std::function<void(const Answer&)> m_programAnswer;
    /// Commands still waiting for their first answer.
    QHash<quint32, Pending> m_pending;
    /// Every off with an assigned ID needs its own accepted Core result;
    /// the ID alone does not prove the transport sent it. A failed or
    /// refused off cannot be resolved by a different-mode off command.
    quint32 m_pendingReleases{0};
    quint32 m_releaseDispatches{0};
    bool m_releaseFailureSticky{false};
    bool m_releaseFailureNotified{false};
    quint64 m_sessionGeneration{0};
    bool m_tuneAsked{false};
    // TX rulings (item 1, review I-1).
    void forgetReleases();
    bool othersKeepTransmitting(bool forTune) const;
    // TX rulings (item 1).
    bool m_screenReleasePending{false};
    bool m_tuneReleasePending{false};
    // Task 37.
    bool m_twoToneAsked{false};
    bool m_voxArmed{false};
    KeepaliveSender m_sessionKeepalive;
    KeepaliveSender m_channelKeepalive;
    KeepaliveSender m_auxiliaryKeepalive;
    QTimer m_keepaliveTimer;
    // TX rulings (item 1).
    QTimer m_releaseGraceTimer;
    quint64 m_keepaliveSequence{0};
    quint64 m_channelKeepalives{0};
    quint64 m_sessionKeepalives{0};
    quint64 m_auxiliaryKeepalives{0};
    void refreshKeepalive();
    /// The Core's `transmitting` as last heard.
    quint32 m_coreStopSerial{0};   // fix wave M7
    bool m_coreTransmitting{false};
    bool m_publishedKeyDown{false};
    bool m_publishedHolds{false};
};

} // namespace NereusSDR
