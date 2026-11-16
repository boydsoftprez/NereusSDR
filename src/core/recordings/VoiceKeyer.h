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
// once and never hands the air to a microphone by itself):
//   - stop();
//   - any other request MOX accepts (a PTT, MOX or TUNE press, a release):
//     a press that keys keeps the radio keyed and its microphone goes out
//     (PTT then keeps the mic on the air); a release unkeys;
//   - a press MOX refuses (another device's PTT or MOX): the radio unkeys;
//   - a device's own tx.key (pressFrom): its key stays and its microphone
//     goes out; any other device's: the radio unkeys;
//   - MOX going off for any other reason (Stop All TX, the link watchdog).
// VOX is audio, not a person: it cuts nothing.
//
// Another device holding transmit refuses send() with the holder's words
// (TransmitHolder::keyRefusalFor) and nothing keys.
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
    static constexpr const char* kCannotKeepReason = "The voice message cannot be kept on the Core.";
    static constexpr const char* kTooLargeReason = "That file is too large for a voice message.";
    static constexpr const char* kNoImportReason = "No voice message is being loaded.";
    static constexpr const char* kNotWholeReason = "The file did not arrive whole. Try again.";

    // Who asked: the radio keys as `keyer`; `name` is shown as who sends.
    struct Requester {
        KeyerIdentity keyer;
        QString name;
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
    };

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
    void stop();
    // A device's own transmit press on the station link (RemoteKeying's
    // tx.key or another start): a voice key from the device and session
    // that sent hands the air to its microphone; anything else unkeys.
    void pressFrom(const QByteArray& deviceId, const QString& session, bool voiceKey);

    // ---- The messages. None of these keys.
    bool rename(int n, const QString& name, QString* refusal);
    bool clear(int n, QString* refusal);
    bool setRepeat(bool on, int delaySec, QString* refusal);
    // Converts `path` to 48 kHz mono 16-bit WAV into Mn. A 48 kHz mono
    // 16-bit PCM WAV is kept as it is, so a saved message loads back the
    // same byte for byte. An empty `name` keeps Mn's name.
    bool importFile(int n, const QString& path, const QString& name, QString* refusal);
    // Plays Mn on `who`'s own speaker, through the player (never keys).
    bool listen(int n, const ListenerId& who, QString* refusal);

    // ---- The station link's import and export, one import per owner.
    bool beginImport(const QString& owner, int n, const QString& name, qint64 size, QString* refusal);
    bool importChunk(const QString& owner, const QByteArray& bytes, QString* refusal);
    bool finishImport(const QString& owner, QString* refusal);
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

private:
    enum class Phase { Idle, Sending, Waiting };
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
    std::shared_ptr<const std::vector<float>> loadSamples(int n, QString* refusal) const;
    bool startSend(int n, const Requester& who, std::shared_ptr<const std::vector<float>> samples,
                   QString* refusal);
    TxRefusal holderRefusal(const KeyerIdentity& keyer) const;
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
    void onMoxRefused();
    void onMoxChanged(bool on);

    RecordingService& m_service;
    QPointer<MoxController> m_mox;
    OnAirSource* m_onAir = nullptr;
    QPointer<TransmitHolder> m_holder;
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
};

}  // namespace NereusSDR
