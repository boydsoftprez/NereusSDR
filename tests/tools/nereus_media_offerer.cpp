// no-port-check: NereusSDR-original.
// =================================================================
// tests/tools/nereus_media_offerer.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 10. The station side of the app's media interop
// tests (ios/scripts/interop-test.sh): runs the Core's own
// LibDataChannelMediaTransport as the offering peer, exactly as the Core
// runs it, and exchanges signalling with the test as JSON lines on stdin
// and stdout. On command it sends the link's media conformance vectors
// (tests/data/link/v1/media): the Opus RTP packets and the NSDC display
// frames, plus bursts for the receive-bound tests.
//
// Usage:
//   nereus_media_offerer --vectors <dir> [--wrong-fingerprint]
//   nereus_media_offerer --vectors <dir> --media-control
//
// --wrong-fingerprint changes one byte of the DTLS fingerprint in the offer
// it prints, so an answerer that checks fingerprints cannot connect.
//
// --media-control (iPhone app plan Task 11) plays the Core's side of media
// control instead: nothing starts until the app's `start` arrives, and the
// Core's own MediaPeer (the signalling bridge DaemonMediaController uses)
// then starts as the offerer under the app's connection id, deriving the
// audio SSRC from it as the Core does. Media control payloads travel both
// ways as {"type":"media-control","payload":{...}} lines. A `start` that
// declares `audioProfileVersion` gets the offer a Core with audio_lossless
// allowed writes (DaemonMediaController::handleStart): the L16 rtpmap at
// payload type 96 on the Opus m-line, written by the Core's own transport.
// An `audio` with `enabled` true is answered with the Core's own
// encodeRemoteAudioContext: the minor-7 eight keys, or with `profile` in
// the request the audio-profile shape (encoder, profile "opus"); then one
// Opus packet every 40 ms from the conformance vectors, as the Core's
// sender paces them. `enabled` false answers disabled and stops. The
// command {"type":"send-l16","count":n} sends n packets with payload type
// 96 on the same SSRC, one per 40 ms tick in place of that tick's Opus
// packet and in the stream's one sequence space (SRTP refuses a sequence
// that jumps back), which an app that asked for Opus must never decode. No display is sent in this mode. Every operation the
// app sends is reported as {"type":"op","op":...}.
//
// Lines it prints (one JSON object each):
//   {"type":"description","sdpType":"offer","sdp":...}
//   {"type":"candidate","candidate":...,"mid":...}
//   {"type":"ready"} {"type":"closed"} {"type":"failed","message":...}
//   {"type":"error","message":...} {"type":"refused","what":...}
//   {"type":"sent","what":...,"count":n}
//   {"type":"ssrc","main":n,"foreign":n}
// Lines it reads (--media-control: media-control and stop only):
//   {"type":"media-control","payload":{...}}
//   {"type":"description","sdpType":"answer","sdp":...}
//   {"type":"candidate","candidate":...,"mid":...}
//   {"type":"send-opus"} {"type":"send-display"}
//   {"type":"send-audio-burst","count":n}
//   {"type":"send-foreign-audio","count":n}
//   {"type":"send-display-burst","count":n,"bytes":n}
//   {"type":"stop"}
//
// =================================================================

#include "core/session/media/LibDataChannelMediaTransport.h"
#include "core/session/media/MediaPeer.h"
#include "core/session/media/PcmAudioCodec.h"
#include "core/session/media/RemoteAudioContext.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QSocketNotifier>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <cstdio>
#include <deque>
#include <unistd.h>

using namespace NereusSDR;

namespace {

constexpr int kOpusPayloadType = 111;
// One 20 ms Opus frame at 48 kHz, the RTP timestamp step between packets.
constexpr quint32 kTimestampStep = 960;
// XORed into the main SSRC to make the declared stream the test expects to
// be dropped. Any nonzero value that keeps the result nonzero will do.
constexpr quint32 kForeignSsrcMask = 0x5a5a5a5aU;
// --media-control: the Core's audio cadence, one 1920-sample (40 ms) Opus
// frame per packet, the RTP timestamp step between them, and where the
// stream starts.
constexpr int kAudioPacketIntervalMs = 40;
constexpr quint32 kAudioTimestampStep = 1920;
constexpr quint16 kAudioFirstSequence = 100;
// At most this many packets per audio request, so a test that never stops
// audio cannot leave the helper sending.
constexpr int kMaxAudioPackets = 750;
// The Opus profile the Core's audio context reports by default: 48 kHz
// stereo, 1920-sample frames, 48000 bit/s, fullband (20 kHz audio; R-R3-21).
constexpr int kOpusTargetBitrate = IMediaTransport::kDefaultAudioTargetBitrate;
constexpr int kOpusAudioBandwidthHz = 20000;


void emitLine(const QJsonObject& object)
{
    const QByteArray line = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(line.constData(), 1, static_cast<std::size_t>(line.size()), stdout);
    std::fflush(stdout);
}

void emitMessage(const QString& type, const QString& message)
{
    emitLine(QJsonObject{{QStringLiteral("type"), type},
                         {QStringLiteral("message"), message}});
}

quint32 ssrcOf(const QByteArray& packet)
{
    return (static_cast<quint32>(static_cast<quint8>(packet.at(8))) << 24)
        | (static_cast<quint32>(static_cast<quint8>(packet.at(9))) << 16)
        | (static_cast<quint32>(static_cast<quint8>(packet.at(10))) << 8)
        | static_cast<quint32>(static_cast<quint8>(packet.at(11)));
}

// The RTP payload of a packet with a plain 12-byte header and no CSRCs,
// extension or padding, which is how the Core writes them.
QByteArray payloadOf(const QByteArray& packet)
{
    return packet.mid(12);
}

QByteArray rtpPacket(quint32 ssrc, quint16 sequence, quint32 timestamp,
                     const QByteArray& payload)
{
    QByteArray packet;
    packet.reserve(12 + payload.size());
    packet.append(static_cast<char>(0x80));
    packet.append(static_cast<char>(kOpusPayloadType));
    packet.append(static_cast<char>(sequence >> 8));
    packet.append(static_cast<char>(sequence & 0xff));
    for (int shift = 24; shift >= 0; shift -= 8) {
        packet.append(static_cast<char>((timestamp >> shift) & 0xff));
    }
    for (int shift = 24; shift >= 0; shift -= 8) {
        packet.append(static_cast<char>((ssrc >> shift) & 0xff));
    }
    packet.append(payload);
    return packet;
}

// Changes the first hex digit of the offer's SHA-256 fingerprint.
QString withWrongFingerprint(const QString& sdp)
{
    const QString prefix = QStringLiteral("a=fingerprint:sha-256 ");
    const qsizetype at = sdp.indexOf(prefix);
    if (at < 0) {
        return sdp;
    }
    QString changed = sdp;
    const qsizetype digit = at + prefix.size();
    changed[digit] = changed.at(digit) == QLatin1Char('0') ? QLatin1Char('1') : QLatin1Char('0');
    return changed;
}

QByteArray readVectorFile(const QString& vectors, const QString& name)
{
    QFile file(QDir(vectors).filePath(name));
    if (!file.open(QIODevice::ReadOnly)) {
        emitMessage(QStringLiteral("error"), QStringLiteral("cannot read ") + file.fileName());
        return {};
    }
    return file.readAll();
}

// --media-control: the Core's side of media control, for the app's
// MediaControlClient. See the file comment.
class MediaControlStation final : public QObject {
public:
    explicit MediaControlStation(QString vectors)
        : m_vectors(std::move(vectors))
        , m_stdin(STDIN_FILENO, QSocketNotifier::Read)
        , m_peer(this)
    {
        connect(&m_stdin, &QSocketNotifier::activated, this, &MediaControlStation::readInput);
        connect(&m_peer, &MediaPeer::controlReady, this, [](const QJsonObject& control) {
            emitLine(QJsonObject{{QStringLiteral("type"), QStringLiteral("media-control")},
                                 {QStringLiteral("payload"), control}});
        });
        connect(&m_peer, &MediaPeer::ready, this, [this] {
            emitLine(QJsonObject{{QStringLiteral("type"), QStringLiteral("ready")},
                                 {QStringLiteral("lossless"), m_peer.losslessAudioNegotiated()}});
        });
        connect(&m_peer, &MediaPeer::closed, this, [] {
            emitLine(QJsonObject{{QStringLiteral("type"), QStringLiteral("closed")}});
        });
        connect(&m_peer, &MediaPeer::connectionFailed, this,
                [](const QString& message) { emitMessage(QStringLiteral("failed"), message); });
        connect(&m_peer, &MediaPeer::errorOccurred, this,
                [](const QString& message) { emitMessage(QStringLiteral("error"), message); });
        m_audioTimer.setInterval(kAudioPacketIntervalMs);
        connect(&m_audioTimer, &QTimer::timeout, this, &MediaControlStation::sendAudioPacket);
    }

    bool loadVectors()
    {
        for (int index = 1; index <= 4; ++index) {
            const QByteArray packet = readVectorFile(m_vectors, QStringLiteral("opus-%1.bin").arg(index));
            if (packet.size() < 12) {
                return false;
            }
            m_opusPayloads.append(payloadOf(packet));
        }
        return true;
    }

private:
    void readInput()
    {
        char buffer[65536];
        const ssize_t count = ::read(STDIN_FILENO, buffer, sizeof buffer);
        if (count <= 0) {
            stop();
            return;
        }
        m_input.append(buffer, static_cast<qsizetype>(count));
        qsizetype newline = m_input.indexOf('\n');
        while (newline >= 0) {
            const QByteArray line = m_input.left(newline);
            m_input.remove(0, newline + 1);
            if (!line.trimmed().isEmpty()) {
                handle(QJsonDocument::fromJson(line).object());
            }
            newline = m_input.indexOf('\n');
        }
    }

    void stop()
    {
        m_stdin.setEnabled(false);
        m_audioTimer.stop();
        m_peer.stop();
        QCoreApplication::quit();
    }

    void handle(const QJsonObject& command)
    {
        const QString type = command.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("stop")) {
            stop();
            return;
        }
        if (type == QStringLiteral("send-l16")) {
            m_l16Pending += command.value(QStringLiteral("count")).toInt();
            return;
        }
        if (type != QStringLiteral("media-control")) {
            emitMessage(QStringLiteral("refused"), type);
            return;
        }
        const QJsonObject payload = command.value(QStringLiteral("payload")).toObject();
        const QString op = payload.value(QStringLiteral("op")).toString();
        emitLine(QJsonObject{{QStringLiteral("type"), QStringLiteral("op")}, {QStringLiteral("op"), op}});
        if (op == QStringLiteral("start")) {
            handleStart(payload);
        } else if (op == QStringLiteral("description") || op == QStringLiteral("candidate")) {
            if (payload.value(QStringLiteral("connectionId")).toString() != m_connectionId
                || !m_peer.acceptControl(payload)) {
                emitMessage(QStringLiteral("refused"), op);
            }
        } else if (op == QStringLiteral("audio")) {
            handleAudio(payload);
        } else {
            // Display operations: this mode sends no display, so any is a
            // finding for the test, which the op line already reports.
            emitMessage(QStringLiteral("refused"), op);
        }
    }

    // DaemonMediaController::handleStart's shape: op and connectionId, and
    // perhaps audioProfileVersion, a whole number of at least 1, which (with
    // lossless allowed, as here) makes the offer carry L16.
    void handleStart(const QJsonObject& payload)
    {
        const QString connectionId = payload.value(QStringLiteral("connectionId")).toString();
        const bool declaresAudioProfile = payload.contains(QStringLiteral("audioProfileVersion"));
        const double version = payload.value(QStringLiteral("audioProfileVersion")).toDouble();
        const bool validDeclaration = !declaresAudioProfile
            || (payload.value(QStringLiteral("audioProfileVersion")).isDouble() && version >= 1
                && version == static_cast<double>(static_cast<qint64>(version)));
        if (payload.size() != (declaresAudioProfile ? 3 : 2) || !validDeclaration
            || !m_connectionId.isEmpty()
            || !m_peer.start(IMediaTransport::Role::Offerer, connectionId, kOpusTargetBitrate,
                             declaresAudioProfile)) {
            emitMessage(QStringLiteral("refused"), QStringLiteral("start"));
            return;
        }
        m_connectionId = connectionId;
        emitLine(QJsonObject{{QStringLiteral("type"), QStringLiteral("ssrc")},
                             {QStringLiteral("main"), static_cast<qint64>(m_peer.audioSsrc())}});
    }

    // DaemonMediaController::handleAudio's shapes: without a profile,
    // answered with the minor-7 audio context; with `profile` "opus", with
    // the audio-profile shape. Both are written by the Core's own
    // encodeRemoteAudioContext.
    void handleAudio(const QJsonObject& payload)
    {
        const double revision = payload.value(QStringLiteral("revision")).toDouble();
        const bool hasProfile = payload.contains(QStringLiteral("profile"));
        if (payload.size() != (hasProfile ? 5 : 4)
            || payload.value(QStringLiteral("connectionId")).toString() != m_connectionId
            || (hasProfile && payload.value(QStringLiteral("profile")) != QStringLiteral("opus"))
            || !payload.value(QStringLiteral("enabled")).isBool() || revision < 1
            || revision <= m_audioRevision) {
            emitMessage(QStringLiteral("refused"), QStringLiteral("audio"));
            return;
        }
        m_audioRevision = revision;
        const bool enabled = payload.value(QStringLiteral("enabled")).toBool();
        ++m_audioGeneration;
        m_audioTimer.stop();
        m_audioPacketsSent = 0;
        m_streamPackets = 0;
        RemoteAudioContextMessage context;
        context.connectionId = m_connectionId;
        context.revision = static_cast<quint32>(revision);
        context.generation = m_audioGeneration;
        context.enabled = enabled;
        context.ssrc = m_peer.audioSsrc();
        context.firstSequence = kAudioFirstSequence;
        context.firstTimestamp = 0;
        if (enabled) {
            OpusEncoderProfile encoder;
            encoder.sampleRate = OpusAudioCodecConfig::kSampleRate;
            encoder.channels = OpusAudioCodecConfig::kChannels;
            encoder.frameSamples = OpusAudioCodecConfig::kFrameSamples;
            encoder.targetBitrate = kOpusTargetBitrate;
            encoder.audioBandwidthHz = kOpusAudioBandwidthHz;
            context.encoder = encoder;
            context.profile = RemoteAudioProfile::Opus;
        } else {
            context.offReason = RemoteAudioOffReason::ClientDisabled;
        }
        emitLine(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("media-control")},
            {QStringLiteral("payload"),
             encodeRemoteAudioContext(context, /*detailNegotiated=*/hasProfile,
                                      /*profileNegotiated=*/hasProfile)}});
        if (enabled) {
            m_audioTimer.start();
        }
    }

    void sendAudioPacket()
    {
        if (!m_peer.isReady()) {
            return;
        }
        if (m_l16Pending > 0) {
            // Payload type 96 on the main SSRC, a zeroed L16 payload.
            QByteArray packet = rtpPacket(m_peer.audioSsrc(),
                                          static_cast<quint16>(kAudioFirstSequence + m_streamPackets),
                                          static_cast<quint32>(m_streamPackets) * kAudioTimestampStep,
                                          QByteArray(PcmAudioCodecConfig::kPayloadBytes, '\0'));
            packet[1] = static_cast<char>(PcmAudioCodecConfig::kPayloadType);
            if (m_peer.sendRtp(packet)) {
                --m_l16Pending;
                ++m_l16Sent;
                ++m_streamPackets;
                emitSent(QStringLiteral("l16"), m_l16Sent);
            }
            return;
        }
        if (m_audioPacketsSent >= kMaxAudioPackets) {
            m_audioTimer.stop();
            return;
        }
        const QByteArray packet = rtpPacket(
            m_peer.audioSsrc(), static_cast<quint16>(kAudioFirstSequence + m_streamPackets),
            static_cast<quint32>(m_streamPackets) * kAudioTimestampStep,
            m_opusPayloads.at(m_audioPacketsSent % m_opusPayloads.size()));
        if (m_peer.sendRtp(packet)) {
            ++m_audioPacketsSent;
            ++m_streamPackets;
            emitSent(QStringLiteral("audio"), m_audioPacketsSent);
        }
    }

    static void emitSent(const QString& what, int count)
    {
        emitLine(QJsonObject{{QStringLiteral("type"), QStringLiteral("sent")},
                             {QStringLiteral("what"), what},
                             {QStringLiteral("count"), count}});
    }

    QString m_vectors;
    QSocketNotifier m_stdin;
    MediaPeer m_peer;
    QTimer m_audioTimer;
    QList<QByteArray> m_opusPayloads;
    QByteArray m_input;
    QString m_connectionId;
    double m_audioRevision = 0;
    quint32 m_audioGeneration = 0;
    int m_audioPacketsSent = 0;
    // Every packet on the stream, Opus or payload type 96: the sequence and
    // timestamp offset from the context's start.
    int m_streamPackets = 0;
    int m_l16Pending = 0;
    int m_l16Sent = 0;
};

class Offerer final : public QObject {
public:
    Offerer(QString vectors, bool wrongFingerprint)
        : m_vectors(std::move(vectors))
        , m_wrongFingerprint(wrongFingerprint)
        , m_stdin(STDIN_FILENO, QSocketNotifier::Read)
    {
        connect(&m_stdin, &QSocketNotifier::activated, this, &Offerer::readInput);
        connect(&m_transport, &IMediaTransport::localDescription, this,
                [this](const QString& sdp, const QString& type) {
                    emitLine(QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("description")},
                        {QStringLiteral("sdpType"), type},
                        {QStringLiteral("sdp"), m_wrongFingerprint ? withWrongFingerprint(sdp) : sdp}});
                });
        connect(&m_transport, &IMediaTransport::localCandidate, this,
                [](const QString& candidate, const QString& mid) {
                    emitLine(QJsonObject{{QStringLiteral("type"), QStringLiteral("candidate")},
                                         {QStringLiteral("candidate"), candidate},
                                         {QStringLiteral("mid"), mid}});
                });
        connect(&m_transport, &IMediaTransport::ready, this, [] {
            emitLine(QJsonObject{{QStringLiteral("type"), QStringLiteral("ready")}});
        });
        connect(&m_transport, &IMediaTransport::closed, this, [] {
            emitLine(QJsonObject{{QStringLiteral("type"), QStringLiteral("closed")}});
        });
        connect(&m_transport, &IMediaTransport::connectionFailed, this,
                [](const QString& message) { emitMessage(QStringLiteral("failed"), message); });
        connect(&m_transport, &IMediaTransport::errorOccurred, this,
                [](const QString& message) { emitMessage(QStringLiteral("error"), message); });
        connect(&m_transport, &IMediaTransport::displayErrorOccurred, this,
                [](const QString& message) { emitMessage(QStringLiteral("error"), message); });
        connect(&m_transport, &IMediaTransport::displayWritable, this, &Offerer::pumpDisplay);
    }

    bool start()
    {
        m_opus.clear();
        for (int index = 1; index <= 4; ++index) {
            const QByteArray packet = readVector(QStringLiteral("opus-%1.bin").arg(index));
            if (packet.size() < 12) {
                return false;
            }
            m_opus.append(packet);
        }
        m_mainSsrc = ssrcOf(m_opus.first());
        m_foreignSsrc = m_mainSsrc ^ kForeignSsrcMask;
        emitLine(QJsonObject{{QStringLiteral("type"), QStringLiteral("ssrc")},
                             {QStringLiteral("main"), static_cast<qint64>(m_mainSsrc)},
                             {QStringLiteral("foreign"), static_cast<qint64>(m_foreignSsrc)}});

        IMediaTransport::StartOptions options{IMediaTransport::Role::Offerer, m_mainSsrc};
        // A second declared stream, so the Core's transport sends RTP under
        // an SSRC the app is told not to expect.
        options.receiverAudioSsrcs = {m_foreignSsrc};
        return m_transport.start(options);
    }

private:
    QByteArray readVector(const QString& name) const
    {
        QFile file(QDir(m_vectors).filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            emitMessage(QStringLiteral("error"), QStringLiteral("cannot read ") + file.fileName());
            return {};
        }
        return file.readAll();
    }

    void readInput()
    {
        char buffer[65536];
        const ssize_t count = ::read(STDIN_FILENO, buffer, sizeof buffer);
        if (count <= 0) {
            m_stdin.setEnabled(false);
            m_transport.stop();
            QCoreApplication::quit();
            return;
        }
        m_input.append(buffer, static_cast<qsizetype>(count));
        qsizetype newline = m_input.indexOf('\n');
        while (newline >= 0) {
            const QByteArray line = m_input.left(newline);
            m_input.remove(0, newline + 1);
            if (!line.trimmed().isEmpty()) {
                handle(QJsonDocument::fromJson(line).object());
            }
            newline = m_input.indexOf('\n');
        }
    }

    void handle(const QJsonObject& command)
    {
        const QString type = command.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("description")) {
            if (!m_transport.acceptDescription(command.value(QStringLiteral("sdp")).toString(),
                                               command.value(QStringLiteral("sdpType")).toString())) {
                emitMessage(QStringLiteral("refused"), QStringLiteral("description"));
            }
        } else if (type == QStringLiteral("candidate")) {
            if (!m_transport.acceptCandidate(command.value(QStringLiteral("candidate")).toString(),
                                             command.value(QStringLiteral("mid")).toString())) {
                emitMessage(QStringLiteral("refused"), QStringLiteral("candidate"));
            }
        } else if (type == QStringLiteral("send-opus")) {
            int sent = 0;
            for (const QByteArray& packet : m_opus) {
                sent += m_transport.sendRtp(packet) ? 1 : 0;
            }
            emitSent(QStringLiteral("opus"), sent);
        } else if (type == QStringLiteral("send-audio-burst")
                   || type == QStringLiteral("send-foreign-audio")) {
            const bool foreign = type == QStringLiteral("send-foreign-audio");
            const int count = command.value(QStringLiteral("count")).toInt();
            const QByteArray payload = payloadOf(m_opus.first());
            int sent = 0;
            for (int index = 0; index < count; ++index) {
                const QByteArray packet = rtpPacket(foreign ? m_foreignSsrc : m_mainSsrc,
                                                    static_cast<quint16>(index),
                                                    static_cast<quint32>(index) * kTimestampStep,
                                                    payload);
                sent += m_transport.sendRtp(packet) ? 1 : 0;
            }
            emitSent(type, sent);
        } else if (type == QStringLiteral("send-display")) {
            queueDisplay(QStringLiteral("display"),
                         {readVector(QStringLiteral("nsdc1-full.bin")),
                          readVector(QStringLiteral("nsdc1-delta.bin"))});
        } else if (type == QStringLiteral("send-display-burst")) {
            const int count = command.value(QStringLiteral("count")).toInt();
            const int bytes = command.value(QStringLiteral("bytes")).toInt();
            QList<QByteArray> messages;
            for (int index = 0; index < count; ++index) {
                // Each message starts with its index, big-endian, so the
                // test can tell which ones it kept.
                QByteArray message(qMax(bytes, 4), static_cast<char>(0x5a));
                for (int byte = 0; byte < 4; ++byte) {
                    message[byte] = static_cast<char>((index >> (24 - 8 * byte)) & 0xff);
                }
                messages.append(message);
            }
            queueDisplay(QStringLiteral("display-burst"), messages);
        } else if (type == QStringLiteral("stop")) {
            m_stdin.setEnabled(false);
            m_transport.stop();
            QCoreApplication::quit();
        } else {
            emitMessage(QStringLiteral("refused"), type);
        }
    }

    void emitSent(const QString& what, int count)
    {
        emitLine(QJsonObject{{QStringLiteral("type"), QStringLiteral("sent")},
                             {QStringLiteral("what"), what},
                             {QStringLiteral("count"), count}});
    }

    void queueDisplay(const QString& what, const QList<QByteArray>& messages)
    {
        for (const QByteArray& message : messages) {
            m_display.push_back({what, message});
        }
        m_displayLabel = what;
        pumpDisplay();
    }

    // Hands display messages to the transport the way the Core does: one
    // held by the library at most, the next after displayWritable().
    void pumpDisplay()
    {
        while (!m_display.empty()) {
            const IMediaTransport::DisplaySendResult result =
                m_transport.submitDisplay(m_display.front().message);
            if (result == IMediaTransport::DisplaySendResult::Busy) {
                return;
            }
            m_display.pop_front();
            if (result == IMediaTransport::DisplaySendResult::Refused) {
                emitMessage(QStringLiteral("error"), QStringLiteral("display message refused"));
            } else {
                ++m_displaySent;
            }
            if (result == IMediaTransport::DisplaySendResult::Queued) {
                if (m_display.empty()) {
                    finishDisplay();
                }
                return;
            }
        }
        finishDisplay();
    }

    void finishDisplay()
    {
        if (m_displaySent > 0 || !m_displayLabel.isEmpty()) {
            emitSent(m_displayLabel, m_displaySent);
        }
        m_displaySent = 0;
        m_displayLabel.clear();
    }

    struct PendingDisplay {
        QString what;
        QByteArray message;
    };

    QString m_vectors;
    bool m_wrongFingerprint = false;
    QSocketNotifier m_stdin;
    LibDataChannelMediaTransport m_transport;
    QList<QByteArray> m_opus;
    quint32 m_mainSsrc = 0;
    quint32 m_foreignSsrc = 0;
    QByteArray m_input;
    std::deque<PendingDisplay> m_display;
    QString m_displayLabel;
    int m_displaySent = 0;
};

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = QCoreApplication::arguments();
    const qsizetype vectorsAt = arguments.indexOf(QStringLiteral("--vectors"));
    if (vectorsAt < 0 || vectorsAt + 1 >= arguments.size()) {
        std::fprintf(stderr, "usage: nereus_media_offerer --vectors <dir> "
                             "[--wrong-fingerprint | --media-control]\n");
        return 2;
    }
    if (arguments.contains(QStringLiteral("--media-control"))) {
        MediaControlStation station(arguments.at(vectorsAt + 1));
        if (!station.loadVectors()) {
            emitMessage(QStringLiteral("error"), QStringLiteral("the Opus vectors could not be read"));
            return 1;
        }
        return QCoreApplication::exec();
    }
    Offerer offerer(arguments.at(vectorsAt + 1),
                    arguments.contains(QStringLiteral("--wrong-fingerprint")));
    if (!offerer.start()) {
        emitMessage(QStringLiteral("error"), QStringLiteral("the media transport did not start"));
        return 1;
    }
    return QCoreApplication::exec();
}
