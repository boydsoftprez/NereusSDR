// =================================================================
// src/core/session/media/MediaPeer.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R3 Task 1.
// See MediaPeer.h for the ownership boundary.
//
// Modification history (NereusSDR):
//   2026-09-25: iPhone app plan Task 36 (R-IOS-13): the microphone line
//               and its SSRC. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-25: iPhone app plan Task 37 (R-IOS-13): the "tx" data channel
//               with the line. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//
//   2026-09-26: iPhone app plan Task 28 (R-IOS-16): the session's ICE
//               settings passed to the media transport. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: LINK minor 14: the private part is held by unique_ptr.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-09-30: TX stall lane: micRtpReceived carries heldUs, how long
//               the packet waited between its receipt in the transport
//               and its report, so the microphone buffer times it at
//               receipt. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-01: TX mic thread (JJ approved): setMicPacketSink, the
//               microphone line delivered on the transport's own thread
//               with this peer's checks; txReceived carries heldUs.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX mic thread fix round 2: the microphone sink's rejection
//               is reported only while its start is still current, as on
//               the owner's path. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-01: Control logging lane: rttMs(). Logging only. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/media/MediaPeer.h"

#include "core/session/media/LibDataChannelMediaTransport.h"
#include "core/session/media/OpusAudioCodec.h"

#include <QCryptographicHash>
#include <QJsonValue>
#include <QList>
#include <QMetaObject>
#include <QPair>
#include <QPointer>
#include <QUuid>

#include <initializer_list>
#include <utility>

namespace NereusSDR {

// IMediaTransport names the default audio target instead of including the
// codec for it; the two must agree (R-R3-23).
static_assert(IMediaTransport::kDefaultAudioTargetBitrate == OpusAudioCodecConfig{}.bitrate,
              "the transport's default audio target must be the encoder's default");

namespace {

constexpr char kDescriptionOp[] = "description";
constexpr char kCandidateOp[] = "candidate";

struct PendingCandidate {
    QString candidate;
    QString mid;
};

bool hasExactKeys(const QJsonObject& object,
                  const std::initializer_list<QString>& keys)
{
    if (object.size() != static_cast<qsizetype>(keys.size())) {
        return false;
    }
    for (const QString& key : keys) {
        if (!object.contains(key)) {
            return false;
        }
    }
    return true;
}

bool isCanonicalConnectionId(const QString& connectionId)
{
    if (connectionId.size() != 36) {
        return false;
    }
    const QUuid uuid = QUuid::fromString(connectionId);
    return !uuid.isNull()
        && uuid.toString(QUuid::WithoutBraces) == connectionId;
}

bool isBoundedString(const QJsonValue& value, qsizetype maxBytes,
                     bool allowEmpty = false)
{
    if (!value.isString()) {
        return false;
    }
    const QByteArray bytes = value.toString().toUtf8();
    return (allowEmpty || !bytes.isEmpty())
        && bytes.size() <= maxBytes && !bytes.contains('\0');
}

bool isDescriptionType(const QString& type)
{
    return type == QLatin1String("offer") || type == QLatin1String("answer");
}

// The first four bytes of SHA-256(identity), big-endian.
quint32 digestSsrc(const QByteArray& identity)
{
    const QByteArray digest = QCryptographicHash::hash(
        identity, QCryptographicHash::Sha256);
    const auto* bytes = reinterpret_cast<const unsigned char*>(
        digest.constData());
    return (static_cast<quint32>(bytes[0]) << 24)
        | (static_cast<quint32>(bytes[1]) << 16)
        | (static_cast<quint32>(bytes[2]) << 8)
        | static_cast<quint32>(bytes[3]);
}

quint32 audioSsrcForConnection(const QString& connectionId)
{
    QByteArray identity("NereusSDR/media-audio-ssrc/v1:");
    identity.append(connectionId.toUtf8());
    const quint32 derived = digestSsrc(identity);
    return derived == 0 ? 1 : derived;
}

quint32 rtpSsrc(const QByteArray& packet)
{
    return (static_cast<quint32>(static_cast<quint8>(packet.at(8))) << 24)
        | (static_cast<quint32>(static_cast<quint8>(packet.at(9))) << 16)
        | (static_cast<quint32>(static_cast<quint8>(packet.at(10))) << 8)
        | static_cast<quint32>(static_cast<quint8>(packet.at(11)));
}

} // namespace

struct MediaPeer::Private {
    TransportFactory factory;
    // Task 28: the session's ICE settings through the service.
    std::optional<IceConfiguration> ice;
    QPointer<IMediaTransport> transport;
    QList<PendingCandidate> pendingCandidates;
    QString connectionId;
    IMediaTransport::Role role = IMediaTransport::Role::Answerer;
    quint32 audioSsrc = 0;
    // R-R3-43: empty unless receiver audio streams were asked for.
    QList<quint32> receiverAudioSsrcs;
    // R-R3-45: 0 unless the headphones mix was asked for.
    quint32 headphonesAudioSsrc = 0;
    // Task 36: 0 unless the microphone line was asked for.
    quint32 micAudioSsrc = 0;
    quint64 generation = 0;
    // TX mic thread: the microphone line's sink, installed on each start.
    IMediaTransport::MicPacketSink micSink;
    int remoteCandidateControls = 0;
    int localCandidateControls = 0;
    bool started = false;
    bool remoteDescriptionAccepted = false;
    bool ready = false;
    StartRefusal startRefusal = StartRefusal::None;
    // While the transport's start() runs: whether it reported an error.
    bool transportStarting = false;
    bool transportStartError = false;
};

MediaPeer::MediaPeer(QObject* parent, TransportFactory factory)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->factory = std::move(factory);
    if (!d->factory) {
        d->factory = [](QObject* owner) -> IMediaTransport* {
            return new LibDataChannelMediaTransport(owner);
        };
    }
}

MediaPeer::~MediaPeer()
{
    stopInternal(false);
}

bool MediaPeer::start(IMediaTransport::Role role, const QString& connectionId,
                      int audioTargetBitrate, bool offerLosslessAudio,
                      bool receiverAudioStreams, bool headphonesMixStream, bool micLine,
                      bool iqChannel)
{
    d->startRefusal = StartRefusal::None;
    if (d->started || !isCanonicalConnectionId(connectionId)) {
        d->startRefusal = StartRefusal::Precondition;
        return false;
    }

    IMediaTransport* transport = nullptr;
    try {
        transport = d->factory(this);
    } catch (...) {
        d->startRefusal = StartRefusal::TransportConstructionFailed;
        emit errorOccurred(QStringLiteral("media transport factory failed"));
        return false;
    }
    if (!transport || transport->thread() != thread()) {
        if (transport && !transport->parent()) {
            transport->deleteLater();
        }
        d->startRefusal = StartRefusal::InvalidTransport;
        emit errorOccurred(QStringLiteral("media transport factory returned an invalid object"));
        return false;
    }
    if (transport->parent() != this) {
        transport->setParent(this);
    }

    ++d->generation;
    const quint64 generation = d->generation;
    d->transport = transport;
    d->connectionId = connectionId;
    d->role = role;
    d->audioSsrc = audioSsrcForConnection(connectionId);
    d->receiverAudioSsrcs = receiverAudioStreams
        ? receiverAudioSsrcsForConnection(connectionId) : QList<quint32>{};
    d->headphonesAudioSsrc = headphonesMixStream
        ? headphonesAudioSsrcForConnection(connectionId) : 0;
    d->micAudioSsrc = micLine ? micAudioSsrcForConnection(connectionId) : 0;
    d->remoteCandidateControls = 0;
    d->localCandidateControls = 0;
    d->remoteDescriptionAccepted = false;
    d->pendingCandidates.clear();
    d->ready = false;
    d->started = true;

    QPointer<MediaPeer> self(this);
    QPointer<IMediaTransport> guardedTransport(transport);
    const auto isCurrentGeneration = [self, guardedTransport, generation] {
        return self && guardedTransport
            && self->isCurrent(guardedTransport, generation);
    };

    connect(transport, &IMediaTransport::localDescription, this,
            [self, isCurrentGeneration]
            (const QString& sdp, const QString& type) {
                if (!isCurrentGeneration()) {
                    return;
                }
                const QByteArray sdpBytes = sdp.toUtf8();
                const QString expectedType = self->d->role
                    == IMediaTransport::Role::Offerer
                    ? QStringLiteral("offer") : QStringLiteral("answer");
                if (sdpBytes.isEmpty()
                    || sdpBytes.size() > IMediaTransport::kMaxDescriptionBytes
                    || sdpBytes.contains('\0') || type != expectedType) {
                    emit self->errorOccurred(
                        QStringLiteral("invalid local media description rejected"));
                    return;
                }
                QJsonObject control{
                    {QStringLiteral("op"), QLatin1String(kDescriptionOp)},
                    {QStringLiteral("connectionId"), self->d->connectionId},
                    {QStringLiteral("sdp"), sdp},
                    {QStringLiteral("type"), type},
                };
                emit self->controlReady(control);
            });
    connect(transport, &IMediaTransport::localCandidate, this,
            [self, isCurrentGeneration]
            (const QString& candidate, const QString& mid) {
                if (!isCurrentGeneration()) {
                    return;
                }
                const QByteArray candidateBytes = candidate.toUtf8();
                const QByteArray midBytes = mid.toUtf8();
                if (candidateBytes.isEmpty()
                    || candidateBytes.size() > IMediaTransport::kMaxCandidateBytes
                    || midBytes.isEmpty()
                    || midBytes.size() > IMediaTransport::kMaxCandidateMidBytes
                    || candidateBytes.contains('\0') || midBytes.contains('\0')
                    || self->d->localCandidateControls
                        >= IMediaTransport::kMaxRemoteCandidates) {
                    emit self->errorOccurred(
                        QStringLiteral("invalid local media candidate rejected"));
                    return;
                }
                ++self->d->localCandidateControls;
                QJsonObject control{
                    {QStringLiteral("op"), QLatin1String(kCandidateOp)},
                    {QStringLiteral("connectionId"), self->d->connectionId},
                    {QStringLiteral("candidate"), candidate},
                    {QStringLiteral("mid"), mid},
                };
                emit self->controlReady(control);
            });
    connect(transport, &IMediaTransport::displayReceived, this,
            [self, isCurrentGeneration](const QByteArray& message) {
                if (!isCurrentGeneration()) {
                    return;
                }
                if (message.isEmpty()
                    || message.size() > IMediaTransport::kMaxDisplayMessageBytes) {
                    emit self->errorOccurred(
                        QStringLiteral("invalid display message rejected"));
                    return;
                }
                emit self->displayReceived(message);
            });
    connect(transport, &IMediaTransport::iqReceived, this,
            [self, isCurrentGeneration](const QByteArray& message) {
                if (isCurrentGeneration() && !message.isEmpty()
                    && message.size() <= IMediaTransport::kMaxIqMessageBytes) {
                    emit self->iqReceived(message);
                }
            });
    connect(transport, &IMediaTransport::iqErrorOccurred, this,
            [self, isCurrentGeneration](const QString& reason) {
                if (isCurrentGeneration()) { emit self->iqErrorOccurred(reason); }
            });
    connect(transport, &IMediaTransport::rtpReceived, this,
            [self, isCurrentGeneration](const QByteArray& packet) {
                if (!isCurrentGeneration()) {
                    return;
                }
                // R-R3-43: the main stream or a declared receiver stream;
                // R-R3-45: or the declared headphones mix. Anything else is
                // refused and reported, as always.
                if (packet.size() < IMediaTransport::kMinRawRtpBytes
                    || packet.size() > IMediaTransport::kMaxRawRtpBytes
                    || !self->isDeclaredAudioSsrc(rtpSsrc(packet))) {
                    emit self->errorOccurred(
                        QStringLiteral("invalid raw RTP packet rejected"));
                    return;
                }
                emit self->rtpReceived(packet);
            });
    // Task 36: the microphone line carries its one SSRC; anything else on it
    // is refused and reported, as on the main line.
    connect(transport, &IMediaTransport::micRtpReceived, this,
            [self, isCurrentGeneration](const QByteArray& packet, qint64 heldUs) {
                if (!isCurrentGeneration()) {
                    return;
                }
                if (packet.size() < IMediaTransport::kMinRawRtpBytes
                    || packet.size() > IMediaTransport::kMaxRawRtpBytes
                    || self->d->micAudioSsrc == 0
                    || rtpSsrc(packet) != self->d->micAudioSsrc) {
                    emit self->errorOccurred(
                        QStringLiteral("invalid microphone RTP packet rejected"));
                    return;
                }
                emit self->micRtpReceived(packet, heldUs);
            });
    // Task 37: the "tx" channel's messages, bounded.
    connect(transport, &IMediaTransport::txReceived, this,
            [self, isCurrentGeneration](const QByteArray& message, qint64 heldUs) {
                if (!isCurrentGeneration() || self->d->micAudioSsrc == 0 || message.isEmpty()
                    || message.size() > IMediaTransport::kMaxTxMessageBytes) {
                    return;
                }
                emit self->txReceived(message, heldUs);
            });
    connect(transport, &IMediaTransport::ready, this,
            [self, isCurrentGeneration] {
                if (!isCurrentGeneration() || self->d->ready) {
                    return;
                }
                self->d->ready = true;
                emit self->ready();
            });
    connect(transport, &IMediaTransport::closed, this,
            [self, isCurrentGeneration] {
                if (isCurrentGeneration()) {
                    self->stop();
                }
            });
    connect(transport, &IMediaTransport::connectionFailed, this,
            [self, isCurrentGeneration](const QString& message) {
                if (isCurrentGeneration()) {
                    emit self->connectionFailed(message);
                }
            });
    connect(transport, &IMediaTransport::errorOccurred, this,
            [self, isCurrentGeneration](const QString& message) {
                if (isCurrentGeneration()) {
                    if (self->d->transportStarting) {
                        self->d->transportStartError = true;
                    }
                    emit self->errorOccurred(message);
                }
            });
    connect(transport, &IMediaTransport::displayErrorOccurred, this,
            [self, isCurrentGeneration](const QString& message) {
                if (isCurrentGeneration()) {
                    emit self->displayErrorOccurred(message);
                }
            });
    connect(transport, &IMediaTransport::displayWritable, this,
            [self, isCurrentGeneration] {
                if (isCurrentGeneration()) {
                    emit self->displayWritable();
                }
            });

    d->transportStarting = true;
    d->transportStartError = false;
    IMediaTransport::StartOptions options{role, d->audioSsrc, audioTargetBitrate};
    options.connectionId = connectionId;
    options.offerLosslessAudio = offerLosslessAudio;
    options.receiverAudioSsrcs = d->receiverAudioSsrcs;
    options.headphonesAudioSsrc = d->headphonesAudioSsrc;
    options.micAudioSsrc = d->micAudioSsrc;
    options.txChannel = d->micAudioSsrc != 0;
    options.iqChannel = iqChannel;
    options.ice = d->ice;
    const bool backendStarted = transport->start(options);
    if (!self) {
        return false;
    }
    d->transportStarting = false;
    if (!self->isCurrent(transport, generation)) {
        return false;
    }
    if (!backendStarted) {
        // The IMediaTransport contract: a refusal that reports an error
        // means the backend could not be built (LibDataChannelMediaTransport
        // start()'s catch); one without an error is a precondition refusal.
        d->startRefusal = d->transportStartError
            ? StartRefusal::TransportConstructionFailed : StartRefusal::TransportRefused;
        stopInternal(false);
        return false;
    }
    if (d->micSink) {
        installMicSink();
    }
    return true;
}

bool MediaPeer::setMicPacketSink(IMediaTransport::MicPacketSink sink)
{
    d->micSink = std::move(sink);
    return installMicSink();
}

bool MediaPeer::installMicSink()
{
    if (!d->started || !d->transport || d->micAudioSsrc == 0) {
        return false;
    }
    if (!d->micSink) {
        d->transport->setMicPacketSink({});
        return false;
    }
    // On the transport's thread: only this start's values, copied in. A
    // rejection is reported on the owner's thread (this peer outlives the
    // transport's thread: stop() ends it first), and only while this start
    // is still current: a report posted before stop() or a restart is
    // dropped, as on the owner's path.
    const quint32 ssrc = d->micAudioSsrc;
    MediaPeer* const peer = this;
    const IMediaTransport* const transport = d->transport;
    const quint64 generation = d->generation;
    IMediaTransport::MicPacketSink sink = d->micSink;
    return d->transport->setMicPacketSink(
        [ssrc, peer, transport, generation, sink](const QByteArray& packet, qint64 heldUs) {
            if (packet.size() < IMediaTransport::kMinRawRtpBytes
                || packet.size() > IMediaTransport::kMaxRawRtpBytes || rtpSsrc(packet) != ssrc) {
                QMetaObject::invokeMethod(
                    peer,
                    [peer, transport, generation]() {
                        if (!peer->isCurrent(transport, generation)) {
                            return;
                        }
                        emit peer->errorOccurred(
                            QStringLiteral("invalid microphone RTP packet rejected"));
                    },
                    Qt::QueuedConnection);
                return;
            }
            sink(packet, heldUs);
        });
}

MediaPeer::StartRefusal MediaPeer::lastStartRefusal() const
{
    return d->startRefusal;
}

void MediaPeer::setIceConfiguration(const std::optional<IceConfiguration>& ice)
{
    d->ice = ice;
}

bool MediaPeer::usesIce() const
{
    return d->ice.has_value();
}

bool MediaPeer::gathersFromServers() const
{
    return d->ice.has_value() && (d->ice->stunServer().has_value()
                                  || !d->ice->relayServers().isEmpty());
}

void MediaPeer::stop()
{
    stopInternal(true);
}

void MediaPeer::stopInternal(bool notify)
{
    if (!d->started && !d->transport) {
        return;
    }

    const bool wasStarted = d->started;
    ++d->generation;
    d->started = false;
    d->ready = false;
    d->remoteDescriptionAccepted = false;
    d->remoteCandidateControls = 0;
    d->localCandidateControls = 0;
    d->pendingCandidates.clear();
    d->connectionId.clear();
    d->audioSsrc = 0;
    d->receiverAudioSsrcs.clear();
    d->headphonesAudioSsrc = 0;
    d->micAudioSsrc = 0;

    QPointer<IMediaTransport> transport = d->transport;
    d->transport.clear();
    if (transport) {
        disconnect(transport, nullptr, this, nullptr);
        transport->stop();
        transport->deleteLater();
    }

    if (notify && wasStarted) {
        emit closed();
    }
}

bool MediaPeer::acceptControl(const QJsonObject& control)
{
    if (!d->started || !d->transport
        || !control.value(QStringLiteral("op")).isString()
        || !control.value(QStringLiteral("connectionId")).isString()
        || control.value(QStringLiteral("connectionId")).toString()
            != d->connectionId) {
        return false;
    }

    const QString op = control.value(QStringLiteral("op")).toString();
    if (op == QLatin1String(kDescriptionOp)) {
        const QString expectedType = d->role == IMediaTransport::Role::Offerer
            ? QStringLiteral("answer") : QStringLiteral("offer");
        if (!hasExactKeys(control,
                          {QStringLiteral("op"), QStringLiteral("connectionId"),
                           QStringLiteral("sdp"), QStringLiteral("type")})
            || d->remoteDescriptionAccepted
            || !isBoundedString(control.value(QStringLiteral("sdp")),
                                IMediaTransport::kMaxDescriptionBytes)
            || !isBoundedString(control.value(QStringLiteral("type")), 6)
            || !isDescriptionType(
                control.value(QStringLiteral("type")).toString())
            || control.value(QStringLiteral("type")).toString()
                != expectedType) {
            return false;
        }

        const quint64 generation = d->generation;
        QPointer<MediaPeer> self(this);
        IMediaTransport* transport = d->transport;
        const bool accepted = transport->acceptDescription(
            control.value(QStringLiteral("sdp")).toString(),
            control.value(QStringLiteral("type")).toString());
        if (!self || !self->isCurrent(transport, generation) || !accepted) {
            return false;
        }
        d->remoteDescriptionAccepted = true;

        const QList<PendingCandidate> pending = std::move(d->pendingCandidates);
        for (const PendingCandidate& candidate : pending) {
            if (!transport->acceptCandidate(candidate.candidate, candidate.mid)) {
                if (self && self->isCurrent(transport, generation)) {
                    emit self->errorOccurred(
                        QStringLiteral("buffered media candidate rejected"));
                }
                return false;
            }
            if (!self || !self->isCurrent(transport, generation)) {
                return false;
            }
        }
        return true;
    }

    if (op == QLatin1String(kCandidateOp)) {
        if (!hasExactKeys(control,
                          {QStringLiteral("op"), QStringLiteral("connectionId"),
                           QStringLiteral("candidate"), QStringLiteral("mid")})
            || !isBoundedString(control.value(QStringLiteral("candidate")),
                                IMediaTransport::kMaxCandidateBytes)
            || !isBoundedString(control.value(QStringLiteral("mid")),
                                IMediaTransport::kMaxCandidateMidBytes)
            || d->remoteCandidateControls
                >= IMediaTransport::kMaxRemoteCandidates) {
            return false;
        }

        ++d->remoteCandidateControls;
        PendingCandidate candidate{
            control.value(QStringLiteral("candidate")).toString(),
            control.value(QStringLiteral("mid")).toString(),
        };
        if (!d->remoteDescriptionAccepted) {
            d->pendingCandidates.push_back(std::move(candidate));
            return true;
        }
        const quint64 generation = d->generation;
        QPointer<MediaPeer> self(this);
        IMediaTransport* transport = d->transport;
        const bool accepted = transport->acceptCandidate(candidate.candidate,
                                                          candidate.mid);
        return self && self->isCurrent(transport, generation) && accepted;
    }

    return false;
}

bool MediaPeer::sendDisplay(const QByteArray& message)
{
    return d->started && d->transport && !message.isEmpty()
        && message.size() <= IMediaTransport::kMaxDisplayMessageBytes
        && d->transport->sendDisplay(message);
}

IMediaTransport::DisplaySendResult MediaPeer::submitDisplay(const QByteArray& message)
{
    if (!d->started || !d->transport || message.isEmpty()
        || message.size() > IMediaTransport::kMaxDisplayMessageBytes) {
        return IMediaTransport::DisplaySendResult::Refused;
    }
    return d->transport->submitDisplay(message);
}

IMediaTransport::DisplaySendResult MediaPeer::submitIq(const QByteArray& message)
{
    if (!d->started || !d->transport || message.isEmpty()
        || message.size() > IMediaTransport::kMaxIqMessageBytes) {
        return IMediaTransport::DisplaySendResult::Refused;
    }
    return d->transport->submitIq(message);
}

bool MediaPeer::iqBusy() const
{
    return d->started && d->transport && d->transport->iqBusy();
}

bool MediaPeer::displayBusy() const
{
    return d->started && d->transport && d->transport->displayBusy();
}

bool MediaPeer::sendRtp(const QByteArray& packet)
{
    return d->started && d->transport
        && packet.size() >= IMediaTransport::kMinRawRtpBytes
        && packet.size() <= IMediaTransport::kMaxRawRtpBytes
        && isDeclaredAudioSsrc(rtpSsrc(packet))
        && d->transport->sendRtp(packet);
}

bool MediaPeer::sendMicRtp(const QByteArray& packet)
{
    return d->started && d->transport && d->micAudioSsrc != 0
        && packet.size() >= IMediaTransport::kMinRawRtpBytes
        && packet.size() <= IMediaTransport::kMaxRawRtpBytes
        && rtpSsrc(packet) == d->micAudioSsrc
        && d->transport->sendMicRtp(packet);
}

bool MediaPeer::sendTx(const QByteArray& message)
{
    return d->started && d->transport && d->micAudioSsrc != 0 && d->transport->sendTx(message);
}

std::optional<MediaIcePath> MediaPeer::selectedPath() const
{
    return d->transport ? d->transport->selectedPath() : std::nullopt;
}

std::optional<qint64> MediaPeer::rttMs() const
{
    return d->transport ? d->transport->rttMs() : std::nullopt;
}

bool MediaPeer::isReady() const
{
    return d->started && d->ready && d->transport
        && d->transport->isReady();
}

bool MediaPeer::losslessAudioNegotiated() const
{
    return d->started && d->transport && d->transport->losslessAudioNegotiated();
}

bool MediaPeer::micLosslessNegotiated() const
{
    return d->started && d->transport && d->micAudioSsrc != 0
        && d->transport->micLosslessNegotiated();
}

QString MediaPeer::connectionId() const
{
    return d->connectionId;
}

quint32 MediaPeer::audioSsrc() const
{
    return d->audioSsrc;
}

QList<quint32> MediaPeer::receiverAudioSsrcs() const
{
    return d->receiverAudioSsrcs;
}

QList<quint32> MediaPeer::receiverAudioSsrcsForConnection(const QString& connectionId)
{
    const quint32 mainSsrc = audioSsrcForConnection(connectionId);
    QList<quint32> ssrcs;
    ssrcs.reserve(IMediaTransport::kMaxReceiverAudioStreams);
    for (int receiver = 0; receiver < IMediaTransport::kMaxReceiverAudioStreams; ++receiver) {
        QByteArray identity("NereusSDR/media-receiver-ssrc/v1:");
        identity.append(QByteArray::number(receiver));
        identity.append(':');
        identity.append(connectionId.toUtf8());
        quint32 ssrc = digestSsrc(identity);
        while (ssrc == 0 || ssrc == mainSsrc || ssrcs.contains(ssrc)) {
            ++ssrc;
        }
        ssrcs.append(ssrc);
    }
    return ssrcs;
}

quint32 MediaPeer::headphonesAudioSsrc() const
{
    return d->headphonesAudioSsrc;
}

quint32 MediaPeer::headphonesAudioSsrcForConnection(const QString& connectionId)
{
    // Distinct from the main stream and from every receiver stream id,
    // declared or not, so the set is the same whichever streams a
    // connection declares.
    const quint32 mainSsrc = audioSsrcForConnection(connectionId);
    const QList<quint32> receivers = receiverAudioSsrcsForConnection(connectionId);
    QByteArray identity("NereusSDR/media-headphones-ssrc/v1:");
    identity.append(connectionId.toUtf8());
    quint32 ssrc = digestSsrc(identity);
    while (ssrc == 0 || ssrc == mainSsrc || receivers.contains(ssrc)) {
        ++ssrc;
    }
    return ssrc;
}

quint32 MediaPeer::micAudioSsrc() const
{
    return d->micAudioSsrc;
}

quint32 MediaPeer::micAudioSsrcForConnection(const QString& connectionId)
{
    // Distinct from every stream id the Core sends on, declared or not, so
    // the set is the same whichever streams a connection declares.
    const quint32 mainSsrc = audioSsrcForConnection(connectionId);
    const QList<quint32> receivers = receiverAudioSsrcsForConnection(connectionId);
    const quint32 headphones = headphonesAudioSsrcForConnection(connectionId);
    QByteArray identity("NereusSDR/media-mic-ssrc/v1:");
    identity.append(connectionId.toUtf8());
    quint32 ssrc = digestSsrc(identity);
    while (ssrc == 0 || ssrc == mainSsrc || receivers.contains(ssrc) || ssrc == headphones) {
        ++ssrc;
    }
    return ssrc;
}

bool MediaPeer::isDeclaredAudioSsrc(quint32 ssrc) const
{
    return ssrc != 0
        && (ssrc == d->audioSsrc || d->receiverAudioSsrcs.contains(ssrc)
            || ssrc == d->headphonesAudioSsrc);
}

std::optional<MediaPeerTelemetry> MediaPeer::telemetry() const
{
    if (!d->started || !d->transport) {
        return std::nullopt;
    }
    IMediaTransport* const transport = d->transport.data();
    const quint64 generation = d->generation;
    const std::optional<MediaTransportTelemetry> traffic = transport->telemetry();
    if (!traffic || !isCurrent(transport, generation)) {
        return std::nullopt;
    }
    return MediaPeerTelemetry{generation, *traffic};
}

bool MediaPeer::isCurrent(const IMediaTransport* transport,
                          quint64 generation) const
{
    return d->started && d->transport == transport
        && d->generation == generation;
}

} // namespace NereusSDR
