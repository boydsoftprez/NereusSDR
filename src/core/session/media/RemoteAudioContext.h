// =================================================================
// src/core/session/media/RemoteAudioContext.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The one wire codec Core and GUI share
// for the remote audio context, in the minor-7 and minor-8 shapes and the
// audio-profile shape (R-R3-23, audioProfileVersion 1), and for the
// receiver audio context (R-R3-43, receiverAudioVersion 1) and the
// headphones audio context (R-R3-45, headphonesMixVersion 1), and for the
// transmit monitor's route (remote-window parity Task 32,
// txMonitorAudioVersion 1); it holds no session identity or playback
// policy.
// =================================================================

#pragma once

#include "core/session/media/OpusAudioCodec.h"
#include "core/session/media/PcmAudioCodec.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QtGlobal>

#include <optional>

namespace NereusSDR {

/// Why Core is not sending audio, as a minor-8 audio context reports it.
/// SliceRemoved and ReceiverLimit (R-R3-43) occur only in a
/// receiver-audio-context, never in the main audio-context.
enum class RemoteAudioOffReason {
    ClientDisabled,
    MediaNotReady,
    RadioOffline,
    EncoderUnavailable,
    /// The receiver's slice is gone (removed, or never there).
    SliceRemoved,
    /// Core already sends kMaxReceiverAudioStreams receiver streams.
    ReceiverLimit,
    /// R-R3-45, only in a headphones-audio-context: no receiver is routed
    /// to the headphones, so there is no headphones mix to send.
    NoHeadphonesReceiver,
};

/// client-disabled, media-not-ready, radio-offline, encoder-unavailable,
/// slice-removed, receiver-limit or no-headphones-receiver.
QString remoteAudioOffReasonToWire(RemoteAudioOffReason reason);
/// One of the four main-context wire strings exactly (not slice-removed or
/// receiver-limit); nullopt for any other string and for any value that is
/// not a string.
std::optional<RemoteAudioOffReason> remoteAudioOffReasonFromWire(const QJsonValue& value);
/// R-R3-43: any of the six wire strings, as a receiver-audio-context carries.
std::optional<RemoteAudioOffReason> receiverAudioOffReasonFromWire(const QJsonValue& value);
/// R-R3-45: the four main-context strings or no-headphones-receiver, as a
/// headphones-audio-context carries.
std::optional<RemoteAudioOffReason> headphonesAudioOffReasonFromWire(const QJsonValue& value);

/// {"codec":"opus","sampleRate","channels","frameSamples","targetBitrate",
/// "audioBandwidthHz"}, every number an integral JSON number.
QJsonObject remoteAudioEncoderToJson(const OpusEncoderProfile& profile);
/// Accepts only that exact key set describing a profile this build can
/// decode: codec "opus", 48000 Hz, 2 channels, 1920-sample frames, a target
/// of 6000..510000 bit/s and one of the five Opus audio bandwidths.
std::optional<OpusEncoderProfile> remoteAudioEncoderFromJson(const QJsonValue& value);

// ---- Audio profile (R-R3-23, audioProfileVersion 1) ----

/// "opus" or "lossless": the `profile` a GUI asks for in its audio control
/// and the profile Core reports it is running in the audio context.
QString remoteAudioProfileToWire(RemoteAudioProfile profile);
/// One of the two wire strings exactly; nullopt otherwise.
std::optional<RemoteAudioProfile> remoteAudioProfileFromWire(const QJsonValue& value);

/// Why Core runs Opus although lossless was asked for. Machine codes for
/// the GUI to turn into plain words; never shown as they are.
enum class RemoteAudioProfileRefusal {
    /// The Core's own setting (nereusd.conf audio_lossless = deny).
    NotAllowed,
    /// This media connection cannot carry it: the lossless format was not
    /// agreed when the connection was set up, or the packetiser is missing.
    Unavailable,
};
/// lossless-not-allowed or lossless-unavailable.
QString remoteAudioProfileRefusalToWire(RemoteAudioProfileRefusal refusal);
std::optional<RemoteAudioProfileRefusal> remoteAudioProfileRefusalFromWire(
    const QJsonValue& value);

/// {"codec":"l16","sampleRate":48000,"channels":2,"frameSamples":192,
/// "bitsPerSample":16,"payloadType":96}, every number an integral JSON number.
QJsonObject remoteAudioL16EncoderToJson(const PcmEncoderProfile& profile);
/// Accepts only that exact key set with exactly those values (the one
/// lossless profile this build can play).
std::optional<PcmEncoderProfile> remoteAudioL16EncoderFromJson(const QJsonValue& value);

struct RemoteAudioContextMessage {
    QString connectionId;
    quint32 revision = 0;
    quint32 generation = 0;
    bool enabled = false;
    quint32 ssrc = 0;
    quint16 firstSequence = 0;
    quint32 firstTimestamp = 0;
    std::optional<OpusEncoderProfile> encoder;     // set only when enabled and detail negotiated
    std::optional<RemoteAudioOffReason> offReason; // set only when disabled and detail negotiated
    // The audio-profile shape only (profileNegotiated). `profile` is the one
    // Core runs (absent means Opus when encoding); when it is Lossless and
    // audio is on, `losslessEncoder` describes the packets and `encoder` is
    // unused. `profileRefusal` says why lossless was asked for and not given.
    std::optional<RemoteAudioProfile> profile;
    std::optional<PcmEncoderProfile> losslessEncoder;
    std::optional<RemoteAudioProfileRefusal> profileRefusal;
    // iPhone app plan Task 23 (audioQualityVersion 1), the main context
    // only: why the device's `opusBitrate` was not taken, in plain words
    // (empty when it was, or none was asked for). The running encoder,
    // which `encoder` reports, stays as it was.
    QString opusBitrateRefusal;
};

/// iPhone app plan Task 23: the plain reason an `opusBitrate` outside the
/// catalogue's `audio.opusProfiles` gets.
QString opusBitrateNotOfferedReason();

// detailNegotiated=false: exactly today's eight keys (op, connectionId,
// revision, generation, enabled, ssrc, firstSequence, firstTimestamp) with
// today's JSON number types; encoder/offReason are not written.
// detailNegotiated=true: those eight plus "encoder" (enabled) or "reason" (disabled).
// An enabled message with no encoder is written disabled with reason
// encoder-unavailable, the only shape a minor-8 GUI accepts for it.
// profileNegotiated=true (only with detailNegotiated; ignored otherwise), for
// a GUI that sent `profile` in its audio control: the detail shape plus
// "profile" always, the "l16" encoder shape when lossless audio is on, and
// "profileRefusal" when Opus runs because lossless was refused. With
// profileNegotiated=false the profile fields are never written, so an older
// GUI sees exactly the shape it parses today.
QJsonObject encodeRemoteAudioContext(const RemoteAudioContextMessage& message,
                                     bool detailNegotiated, bool profileNegotiated = false);

// Shape and field validation only (identity checks stay with the caller).
// Accepts exactly the shape selected by detailNegotiated and
// profileNegotiated; nullopt otherwise.
// All shapes take revision, generation and ssrc as integral 1..4294967295,
// as the GUI's parser always has, and firstSequence 0..65535 and
// firstTimestamp 0..4294967295. The profile shape also requires: "profile"
// opus or lossless; an enabled lossless context with the "l16" encoder and
// an enabled Opus one with the Opus encoder; "profileRefusal" only beside
// profile opus.
std::optional<RemoteAudioContextMessage> decodeRemoteAudioContext(const QJsonObject& payload,
                                                                  bool detailNegotiated,
                                                                  bool profileNegotiated = false);

// ---- Receiver audio (R-R3-43, receiverAudioVersion 1) ----

/// Core's answer to {op:"receiver-audio", connectionId, sliceId, revision,
/// enabled, profile}: the audio-profile shape of the audio context with op
/// "receiver-audio-context" and the slice id beside it:
/// {op, connectionId, sliceId, revision, generation, enabled, ssrc,
///  firstSequence, firstTimestamp, profile, encoder | reason
///  [, profileRefusal]}.
/// ssrc is the receiver stream id the packets carry; a disabled context
/// that holds no receiver stream id (receiver-limit, slice-removed, or one
/// that never started) carries ssrc, firstSequence and firstTimestamp 0.
/// The reason may be any of the six, slice-removed and receiver-limit
/// included. generation counts receiver contexts on their own; it is not
/// the main audio context's generation.
struct RemoteReceiverAudioContextMessage {
    int sliceId = -1;
    RemoteAudioContextMessage context;
};
QJsonObject encodeReceiverAudioContext(const RemoteReceiverAudioContextMessage& message);
/// Exactly that shape; sliceId an integral 0..2147483647; otherwise as
/// decodeRemoteAudioContext's profile shape, except the ssrc rule above.
std::optional<RemoteReceiverAudioContextMessage> decodeReceiverAudioContext(
    const QJsonObject& payload);

// ---- Headphones mix (R-R3-45, headphonesMixVersion 1) ----

/// Core's answer to {op:"headphones-audio", connectionId, revision, enabled,
/// profile}, and its notice whenever the headphones mix starts, stops or
/// changes: the audio-profile shape of the audio context with op
/// "headphones-audio-context":
/// {op, connectionId, revision, generation, enabled, ssrc, firstSequence,
///  firstTimestamp, profile, encoder | reason [, profileRefusal]}.
/// ssrc is always the connection's headphones mix id. revision is the
/// newest headphones-audio request; generation counts headphones contexts
/// on their own. The reason is one of the four main reasons or
/// no-headphones-receiver.
QJsonObject encodeHeadphonesAudioContext(const RemoteAudioContextMessage& message);
/// Exactly that shape; otherwise as decodeRemoteAudioContext's profile shape.
std::optional<RemoteAudioContextMessage> decodeHeadphonesAudioContext(const QJsonObject& payload);

// ---- Transmit monitor (remote-window parity Task 32, txMonitorAudioVersion 1) ----

/// Where a device wants the transmit monitor (MON) while it holds transmit
/// and is on the air: its main stream, its headphones stream, or nowhere.
enum class TxMonitorRoute { None, Speakers, Headphones };
/// "none", "speakers" or "headphones".
QString txMonitorRouteToWire(TxMonitorRoute route);
/// Exactly one of the three strings; nullopt for any other value.
std::optional<TxMonitorRoute> txMonitorRouteFromWire(const QJsonValue& value);

/// The request {op:"monitor-audio", connectionId, revision, route}, and the
/// Core's one answer to it, {op:"monitor-audio-context", connectionId,
/// revision, route}: the request's revision and the route the Core applies
/// for the device (headphones becomes speakers for a device whose start did
/// not declare headphonesMixVersion, whose main stream carries it then).
struct MonitorAudioMessage {
    QString connectionId;
    quint32 revision = 0;
    TxMonitorRoute route = TxMonitorRoute::None;
};
QJsonObject encodeMonitorAudioRequest(const MonitorAudioMessage& message);
QJsonObject encodeMonitorAudioContext(const MonitorAudioMessage& message);
/// Exactly the request's four keys, op "monitor-audio", a string
/// connectionId, a nonzero integral uint32 revision and a known route.
std::optional<MonitorAudioMessage> decodeMonitorAudioRequest(const QJsonObject& payload);
/// Exactly the context's four keys, under op "monitor-audio-context", as
/// the request is checked.
std::optional<MonitorAudioMessage> decodeMonitorAudioContext(const QJsonObject& payload);

} // namespace NereusSDR
