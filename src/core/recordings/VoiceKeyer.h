// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original voice keyer. The repeat wait's range is
// the existing DvkApplet's (src/gui/applets/DvkApplet.cpp:108-109); no logic
// is ported.
// Modification history (NereusSDR):
// 2026-11-16  J.J. Boyd / KG4VCF; the voice keyer on the Core: M1 to M8
//             kept per station as 48 kHz 16-bit mono WAV, sent through the
//             on-air source with the normal processing, keying and unkeying
//             the radio by itself, with Repeat and its wait (spec 3, 6.1,
//             6.6, V3, section 7, A8, A9). AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; review fixes: only the sender's own
//             controls stop its message (ruling 8.5), the sender's wire id in
//             `sending`, the wait ends when the radio drops, imports convert
//             on the recordings worker, a link import's file goes with its
//             session. AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; a message on the air cannot be cleared,
//             imported into or replaced ("M<n> is on the air."), and a
//             Core's leftover import files go on load. AI tooling:
//             Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; fix round 2: a waiting repeat goes with
//             its sender's session (sessionEnded, the sender-alive check),
//             only the sender or the holder turns Repeat off under a
//             message (setRepeatFrom), handedToMicrophone, and a message in
//             use is never lost to a failed replace. AI tooling: Anthropic
//             Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; final review fix: kCwModeReason, a
//             message is refused in CW and ends if CW keys under it. AI
//             tooling: Anthropic Claude Code.
#pragma once

#include "core/MoxController.h"
#include "core/NereusCoreExport.h"
#include "core/recordings/RecordingPlayer.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

class QFile;
class QTimer;

namespace NereusSDR {

class OnAirSource;
class RecordingService;
class TransmitHolder;

// The Core's voice keyer (spec 6.6). Lives on the main (model) thread and is
// owned by RecordingService.
//
// Keying: send() is the only call that keys the radio. It keys as the device
// that asked (MoxController::setMox(bool, const KeyerIdentity&)), sends the
// message through OnAirSource::beginVoiceMessage, so the normal processing
// applies and none of the five on-air switches, and unkeys at the end. With
// Repeat on it then waits the set seconds and sends again, until stopped.
//
// What cuts it, within one transmit block (the message turns to silence at
// once and never hands the air to a microphone by itself). Ruling 8.5: only
// the sender's own controls, the radio's own PTT (which takes transmit,
// ruling 8.9) and a take stop a message; any other device's press or stop
// is refused and the message goes on.
//   - stop(), the Core's own; stopFrom() for a device, the sender's (or the
//     holder's) only;
//   - another request of the sender's that MOX accepts (its PTT, MOX or
//     TUNE press, its release): a press keeps the radio keyed and its
//     microphone goes out (PTT then keeps the mic on the air); a release
//     unkeys;
//   - the sender's own tx.key, once admitted (keyAdmittedFor): its key stays
//     and its microphone goes out;
//   - MOX going off for any other reason (Stop All TX, the link watchdog,
//     a take).
// VOX is audio, not a person: it cuts nothing. A press MOX refuses cuts
// nothing either. The radio's connection dropping ends a message and a wait
// between repeats (connectionLost).
//
// The transmit slice in CWL or CWU refuses send() and a repeat with
// kCwModeReason and nothing keys; a message going out ends and unkeys if
// the slice moves to CW or MOX runs as a CW key under it.
//
// Another device holding transmit refuses send() with the holder's words
// (TransmitHolder::keyRefusalFor) and nothing keys. Another device's message
// going out, or waiting to repeat, refuses it with kTransmittingReason.
class NEREUS_CORE_EXPORT VoiceKeyer : public QObject {
    Q_OBJECT

public:
    static constexpr int kVoiceKeyerSlots = 8;
    static constexpr int kVoiceClipMaxMs = 120000;
    // V3: the transmit input's rate, so a message goes out unresampled.
    static constexpr int kSampleRateHz = 48000;
    // The repeat wait, as the existing voice keyer panel offers it: its
    // slider runs 1 to 60 s and starts at 5 (DvkApplet.cpp:108-109).
    static constexpr int kRepeatDelayMinSec = 1;
    static constexpr int kRepeatDelayMaxSec = 60;
    static constexpr int kRepeatDelayDefaultSec = 5;
    // How often the end of a message and the end of a wait are looked for.
    static constexpr int kTickMs = 20;
    static constexpr int kNameMaxChars = 32;
    // An import larger than this is refused before any of it is kept: two
    // minutes of 96 kHz stereo 32-bit audio is about 92 MB, of 48 kHz
    // stereo 32-bit 46 MB.
    static constexpr qint64 kImportMaxBytes = 100LL * 1024 * 1024;
    // voiceKeyer.export and voiceKeyer.chunk move this much at once, as
    // recordings.export does (RecordingsFacade::kExportChunkBytes).
    static constexpr qint64 kChunkBytes = 196608;

    // Who sends, for the station's own window (a remote device by its name).
    static constexpr const char* kStationName = "Desktop";

    // What the operator sees.
    static constexpr const char* kTooLongReason = "Voice messages can be up to 2 minutes long.";
    static constexpr const char* kSlotRangeReason = "Voice messages are M1 to M8.";
    static constexpr const char* kUnreadableReason = "That file cannot be read as sound.";
    static constexpr const char* kTransmittingReason = "The radio is already transmitting.";
    static constexpr const char* kNoRadioReason = "Connect to a radio to send a voice message.";
    static constexpr const char* kNotKeyedReason = "The radio could not be keyed.";
    // A voice message in CWL or CWU would key a CW key, which never takes
    // the message: refused, and nothing keys.
    static constexpr const char* kCwModeReason = "Voice messages go out in voice modes. The transmit slice is in CW.";
    static constexpr const char* kCannotKeepReason = "The voice message cannot be kept on the Core.";
    static constexpr const char* kTooLargeReason = "That file is too large for a voice message.";
    static constexpr const char* kNoImportReason = "No voice message is being loaded.";
    static constexpr const char* kNotWholeReason = "The file did not arrive whole. Try again.";

    // Who asked: the radio keys as `keyer`; `name` is shown as who sends;
    // `wireId` is the device as the station link names it (`sending`'s
    // `deviceId`), so the sender's window keeps its keepalives going.
    struct Requester {
        KeyerIdentity keyer;
        QString name;
        QString wireId;
    };
    struct Message {
        int n = 0;
        QString name;
        qint64 lengthMs = 0;
        bool empty = true;
    };
    struct Sending {
        int n = 0;
        qint64 positionMs = 0;
        qint64 lengthMs = 0;
        qint64 waitingMs = 0;   // between repeats; 0 while it goes out
        QString by;
        QString deviceId;       // the sender's wireId
    };
    // An import's end, on this object's thread: true when Mn holds it,
    // otherwise why not.
    using ImportDone = std::function<void(bool ok, const QString& refusal)>;

    VoiceKeyer(RecordingService& service, MoxController* mox, OnAirSource* onAir,
               QObject* parent = nullptr);
    ~VoiceKeyer() override;   // stops a message going out and unkeys

    // Where M1 to M8 are kept: the Core's own disk, under its application
    // data, whatever disk recordings go to. A test points it elsewhere.
    static QString defaultFolder();
    void setFolder(const QString& folder);
    QString folder() const { return m_folder; }

    // The station's transmit holder (StationServer's); null without one.
    void setTransmitHolder(TransmitHolder* holder);
    // Whether a sender still has a live session (StationServer's); unset,
    // every sender is. A repeat whose sender has none never keys again.
    using SenderAlive = std::function<bool(const KeyerIdentity&)>;
    void setSenderAlive(SenderAlive alive);

    QList<Message> messages() const;
    bool repeat() const { return m_repeat; }
    int repeatDelaySec() const { return m_repeatDelaySec; }
    std::optional<Sending> sending() const;
    // The `voiceKeyer` part of the recordings document.
    QJsonObject documentJson() const;
    // 1 to 8, or 0 when all eight hold a message.
    int firstEmpty() const;
    // The kept file of Mn; empty when Mn is empty.
    QString messagePath(int n) const;

    // ---- Keying.
    bool send(int n, const Requester& who, QString* refusal);
    // The Core's own stop (teardown, the radio gone): whoever sent.
    void stop();
    // A device's Stop: the sender's, or the holder's after a take, stops
    // it; any other device is refused with the holder's words (ruling 8.5)
    // and the message goes on. Nothing going out is no refusal.
    bool stopFrom(const KeyerIdentity& who, TxRefusal* refusal);
    // `session` ended (its link dropped or it left): a repeat its sender
    // was waiting on is over, and nothing keys again for it.
    void sessionEnded(const QString& session);
    // RemoteKeying admitted `deviceId`'s tx.key: the sender's own (device
    // and session) hands the air from its message to its microphone.
    void keyAdmittedFor(const QByteArray& deviceId, const QString& session);
    // The radio's connection dropped: a message going out, or waiting to
    // repeat, is over, and nothing keys again on a reconnect.
    void connectionLost();

    // ---- The messages. None of these keys. A message on the air (going
    // out, or waiting to repeat) cannot be cleared, imported into or
    // replaced by anyone, its sender included: "M<n> is on the air." Stop
    // ends it; renaming it is allowed.
    bool rename(int n, const QString& name, QString* refusal);
    bool clear(int n, QString* refusal);
    // The Core's own: whoever sent, turning Repeat off ends a wait.
    bool setRepeat(bool on, int delaySec, QString* refusal);
    // A device's: turning Repeat off under a message (going out, or
    // waiting to repeat) is the sender's or the holder's only, as Stop is
    // (ruling 8.5); another device is refused with the holder's words in
    // `held` (and `refusal`), and nothing changes.
    bool setRepeatFrom(const KeyerIdentity& who, bool on, int delaySec, QString* refusal,
                       TxRefusal* held = nullptr);
    // Converts `path` to 48 kHz mono 16-bit WAV into Mn. A 48 kHz mono
    // 16-bit PCM WAV is kept as it is, so a saved message loads back the
    // same byte for byte. An empty `name` keeps Mn's name. The conversion
    // runs on the recordings worker thread, a few thousand frames a turn,
    // so this thread (MOX, the end of a message, the link) never waits on
    // it; `done` runs on this thread once Mn holds the message or it is
    // refused, on a later turn (at once only for a refused argument).
    void importFile(int n, const QString& path, const QString& name, ImportDone done);
    // Plays Mn on `who`'s own speaker, through the player (never keys).
    bool listen(int n, const ListenerId& who, QString* refusal);

    // ---- The station link's import and export, one import per owner.
    bool beginImport(const QString& owner, int n, const QString& name, qint64 size, QString* refusal);
    bool importChunk(const QString& owner, const QByteArray& bytes, QString* refusal);
    // The file is converted as importFile() does; `done` as there.
    void finishImport(const QString& owner, ImportDone done);
    // Drops `owner`'s import and its file (its session ended, or it began
    // another); a conversion already under way runs to its end.
    void cancelImport(const QString& owner);
    // Up to kChunkBytes of Mn's file from `offset`, and the file's size.
    bool exportChunk(int n, qint64 offset, QByteArray* chunk, qint64* size, QString* refusal);

    // ---- Test seams.
    using Clock = std::function<qint64()>;
    void setClockForTest(Clock clock);
    void tickForTest() { tick(); }

signals:
    void changed();
    // A repeat that could not key again for `deviceId` (the message has
    // stopped).
    void sendRefused(const QByteArray& deviceId, const QString& reason);
    // The sender's own press took the air from its message with the radio
    // still keyed for `deviceId`: its microphone goes out from here.
    void handedToMicrophone(const QByteArray& deviceId);

private:
    enum class Phase { Idle, Sending, Waiting };
    struct Conversion;   // one import's conversion; VoiceKeyer.cpp
    struct PendingImport {
        int n = 0;
        QString name;
        ImportDone done;
    };
    // One message going out: read by the transmit thread through the pull
    // OnAirSource calls, never locked.
    struct Clip {
        std::shared_ptr<const std::vector<float>> samples;
        std::atomic<qint64> position{0};
        std::atomic<bool> stopped{false};   // silence from the next block
        std::atomic<bool> ended{false};     // the last sample has gone out
    };
    struct Import {
        int n = 0;
        QString name;
        qint64 size = 0;
        qint64 received = 0;
        std::shared_ptr<QFile> file;   // QHash keeps values copyable
    };

    static std::function<int(float*, int)> pullFor(std::shared_ptr<Clip> clip);
    void ensureLoaded() const;
    void saveNames() const;
    qint64 now() const;
    QString pathFor(int n) const;
    static QString emptyReason(int n);
    static QString onAirReason(int n);
    bool isOnAir(int n) const;
    std::shared_ptr<const std::vector<float>> loadSamples(int n, QString* refusal) const;
    bool startSend(int n, const Requester& who, std::shared_ptr<const std::vector<float>> samples,
                   QString* refusal);
    TxRefusal holderRefusal(const KeyerIdentity& keyer) const;
    // Ruling 8.5: whether `who` may end the message on the air (its sender,
    // or the holder after a take); otherwise the holder's words.
    bool mayEnd(const KeyerIdentity& who, TxRefusal* refusal) const;
    // The recordings worker thread: one turn of `c`, then the next turn or
    // the end, posted back to `owner`.
    static void stepConversion(RecordingService* service, VoiceKeyer* owner, std::shared_ptr<Conversion> c);
    void finishConversion(const std::shared_ptr<Conversion>& c, bool ok);
    void tick();
    void endOfMessage();
    void resend();
    void cutClip();
    void endOnAirIfOurs();
    void becomeIdle();
    void handOverToPress();
    void unkeyIfOurs(const KeyerIdentity& keyer);
    void updateTimer();
    void onRequestAccepted(const KeyerIdentity& requester, bool requestedOn);
    void onMoxChanged(bool on);

    RecordingService& m_service;
    QPointer<MoxController> m_mox;
    OnAirSource* m_onAir = nullptr;
    QPointer<TransmitHolder> m_holder;
    SenderAlive m_senderAlive;
    QString m_folder;
    Clock m_clock;
    QTimer* m_timer = nullptr;

    mutable bool m_loaded = false;
    mutable std::array<QString, kVoiceKeyerSlots> m_names;
    mutable std::array<qint64, kVoiceKeyerSlots> m_lengths{};
    mutable std::array<bool, kVoiceKeyerSlots> m_filled{};
    bool m_repeat = false;
    int m_repeatDelaySec = kRepeatDelayDefaultSec;

    Phase m_phase = Phase::Idle;
    int m_n = 0;
    Requester m_who;
    KeyerIdentity m_keyer;   // as keyed, with this send's request tag
    std::shared_ptr<Clip> m_clip;
    std::shared_ptr<const std::vector<float>> m_samples;
    qint64 m_waitUntilMs = 0;
    quint64 m_nextTag = 0;
    bool m_keying = false;   // inside this object's own setMox(true)

    QHash<QString, Import> m_imports;
    // Each import's own number: its files' names (never two alike).
    quint64 m_nextImportId = 0;
    // Conversions under way (this thread's), with what each one ends in.
    std::vector<std::shared_ptr<Conversion>> m_conversions;
    QHash<quint64, PendingImport> m_pendingImports;
};

}  // namespace NereusSDR
