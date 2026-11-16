// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original on-air source. Thetis is cited for
// behavior only (clsAudioRecordPlayback.cs [v2.10.3.15]); no logic is ported.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; the on-air source: while a listener that
//             is playing a recording keys the radio, the transmit input is
//             that playback from where it plays, and the five processing
//             switches apply until it stops going out (spec 3, 6.1, 6.3,
//             V5, V6, V7, A6, A7). AI tooling: Anthropic Claude Code.
#pragma once

#include "core/NereusCoreExport.h"
#include "core/recordings/RecordingPlayer.h"

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QString>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

namespace NereusSDR {

class RecordingService;
class TransmitModel;
class TxChannel;

// What the transmitter sends in place of the microphone while a recording
// goes on the air. Owned by RecordingService; TxWorkerThread reads it at the
// top of every transmit block (pull), ahead of TCI and every microphone.
//
// Thetis sends the playback while MOX is on from the position it is at
// (clsAudioRecordPlayback.cs:3993 GetPlayBuffer [v2.10.3.15]) and turns off
// TX EQ, compressor, CFC, phase rotator and leveler while it plays
// (clsAudioRecordPlayback.cs:270-283, 533-543 [v2.10.3.15]). Here a
// listener's playback goes out only while that listener keys (V6), and each
// of the five stays on when its "use while it goes out" setting says so.
//
// Threads: pull() and active() on the transmit worker (any thread for
// active()); everything else on the main (model) thread.
class NEREUS_CORE_EXPORT OnAirSource {
public:
    enum class Kind { None, Playback, VoiceMessage };

    explicit OnAirSource(RecordingService& service);
    ~OnAirSource();   // ends whatever is on the air

    OnAirSource(const OnAirSource&) = delete;
    OnAirSource& operator=(const OnAirSource&) = delete;

    // The transmit chain the processing switches act on: the TX channel as
    // it is at the moment (null while there is none) and the model they are
    // restored from. The switches call TxChannel only; TransmitModel is read,
    // never written.
    void setTransmitChain(std::function<TxChannel*()> channel, TransmitModel* model);

    // Audio thread: `frames` mono 48 kHz samples to send in place of the
    // microphone, returning true; false to use the normal microphone path
    // (nothing on the air, or what was on the air has ended). No lock, no
    // wait, no allocation.
    bool pull(float* mono48k, int frames) noexcept;
    Kind active() const noexcept;   // std::atomic inside

    // Main thread.
    // On a key by `who` while its recording plays: its playback goes on the
    // air from where it is, its speakers stop getting it, and the switches
    // apply. Nothing happens when `who` plays nothing.
    void beginPlayback(const ListenerId& who);
    // A voice keyer message (Task 11): `pullClip` writes up to `frames`
    // samples of the message and returns how many it wrote; fewer than asked
    // means the message ended there, and 0 that it is over. It is called on
    // the audio thread and must not lock, wait or allocate. The message gets
    // the normal processing: no switch applies. While a message is on the
    // air, the next one replaces it with no block between them (Task 11's
    // voice keyer returns silence after its end, so the air never falls to
    // a microphone until it unkeys or hands over).
    void beginVoiceMessage(const QString& slotId, std::function<int(float*, int)> pullClip);
    // Stops sending; a playback goes back to its speakers from where it got
    // to, and the switches return to what TransmitModel says. Again: nothing.
    void end();

    // Whose playback is on the air; empty when none is.
    std::optional<ListenerId> playbackListener() const;
    // The voice keyer message on the air; empty when none is.
    QString voiceMessageSlot() const { return m_voiceSlot; }

private:
    // The five switches (spec 6.5, V7).
    enum Switch : unsigned { kTxEq = 1u, kCompressor = 2u, kLeveler = 4u, kCfc = 8u, kPhaseRotator = 16u };

    void holdSwitches();
    void assertHeld() const;
    void releaseSwitches();
    void waitForPulls() const;

    RecordingService& m_service;
    std::function<TxChannel*()> m_channel;
    TransmitModel* m_model = nullptr;

    std::atomic<Kind> m_kind{Kind::None};
    std::atomic<int> m_playChannel{-1};
    std::atomic<std::function<int(float*, int)>*> m_clip{nullptr};
    mutable std::atomic<int> m_pullsInFlight{0};

    // Main thread.
    std::optional<ListenerId> m_listener;
    QString m_voiceSlot;
    std::unique_ptr<std::function<int(float*, int)>> m_clipOwner;
    unsigned m_held = 0;
    QObject m_context;   // the connections that keep the held switches off
    QList<QMetaObject::Connection> m_holdConnections;
};

}  // namespace NereusSDR
