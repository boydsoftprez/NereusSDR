// =================================================================
// tools/nereus-media-probe.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote daemon R3 live display probe.
//
// A bounded, headless developer probe for the authenticated station-to-media
// path. Credentials are read only from a private JSON file and are never
// printed or accepted as command-line values.
//
// =================================================================

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/CoreInit.h"
#include "core/FFTEngine.h"
#include "core/RadioConnection.h"
#include "core/session/RemoteStationOptions.h"
#include "core/session/StationClient.h"
#include "core/session/media/DisplayCodec.h"
#include "core/session/media/MediaPeer.h"
#include "core/session/media/OpusAudioCodec.h"
#include "core/session/media/RemoteAudioContext.h"
#include "core/session/media/SpectrumEndpoint.h"
#include "core/settings/SettingsProxy.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QUuid>
#include <QtEndian>

#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <initializer_list>

using namespace NereusSDR;

namespace {

constexpr quint32 kEndpointId = 1;
constexpr quint32 kRevision = 1;
constexpr int kDefaultSeconds = 15;
constexpr int kMaximumSeconds = 300;
constexpr int kMinimumAcceptedFrames = 5;
constexpr int kMinimumDistinctFrames = 3;
constexpr int kMinimumWaterfallAdvances = 2;

struct StationCredentials {
    QUrl url;
    QString token;
    QString fingerprint;
};

bool jsonNumber(const QJsonObject& object, const char* key, double low,
                double high, double& value, bool integer = false)
{
    const QJsonValue json = object.value(QLatin1String(key));
    if (!json.isDouble()) {
        return false;
    }
    value = json.toDouble();
    return std::isfinite(value) && value >= low && value <= high
        && (!integer || std::floor(value) == value);
}

bool jsonUint32(const QJsonObject& object, const char* key, quint32& value)
{
    double parsed = 0.0;
    if (!jsonNumber(object, key, 1,
                    std::numeric_limits<quint32>::max(), parsed, true)) {
        return false;
    }
    value = static_cast<quint32>(parsed);
    return true;
}

QString opusStatusName(OpusAudioCodecStatus status)
{
    switch (status) {
    case OpusAudioCodecStatus::Accepted: return QStringLiteral("accepted");
    case OpusAudioCodecStatus::Concealed: return QStringLiteral("concealed");
    case OpusAudioCodecStatus::InvalidInput: return QStringLiteral("invalidInput");
    case OpusAudioCodecStatus::EncodeFailed: return QStringLiteral("encodeFailed");
    case OpusAudioCodecStatus::DecodeFailed: return QStringLiteral("decodeFailed");
    case OpusAudioCodecStatus::MalformedRtp: return QStringLiteral("malformedRtp");
    case OpusAudioCodecStatus::UnexpectedSsrc: return QStringLiteral("unexpectedSsrc");
    case OpusAudioCodecStatus::Oversized: return QStringLiteral("oversized");
    }
    return QStringLiteral("unknown");
}

QString reasonName(DisplayCodecReason reason)
{
    switch (reason) {
    case DisplayCodecReason::None: return QStringLiteral("none");
    case DisplayCodecReason::InvalidInput: return QStringLiteral("invalidInput");
    case DisplayCodecReason::NoHistory: return QStringLiteral("noHistory");
    case DisplayCodecReason::SequenceGap: return QStringLiteral("sequenceGap");
    case DisplayCodecReason::StaleSequence: return QStringLiteral("staleSequence");
    case DisplayCodecReason::OldContext: return QStringLiteral("oldContext");
    case DisplayCodecReason::ContextMismatch: return QStringLiteral("contextMismatch");
    case DisplayCodecReason::BadMagic: return QStringLiteral("badMagic");
    case DisplayCodecReason::UnsupportedVersion: return QStringLiteral("unsupportedVersion");
    case DisplayCodecReason::UnknownFlags: return QStringLiteral("unknownFlags");
    case DisplayCodecReason::Truncated: return QStringLiteral("truncated");
    case DisplayCodecReason::Oversized: return QStringLiteral("oversized");
    case DisplayCodecReason::Malformed: return QStringLiteral("malformed");
    }
    return QStringLiteral("unknown");
}

bool loadCredentials(const QString& path, StationCredentials& credentials,
                     QString& failure)
{
    QFileInfo info(path);
    if (!info.exists() || !info.isFile() || info.isSymLink()) {
        failure = QStringLiteral("stationFileInvalid");
        return false;
    }
#ifdef Q_OS_UNIX
    const QFileDevice::Permissions publicBits =
        QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
        | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
    if ((info.permissions() & publicBits) != 0) {
        failure = QStringLiteral("stationFileNotPrivate");
        return false;
    }
#endif
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > 16 * 1024) {
        failure = QStringLiteral("stationFileUnreadable");
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        failure = QStringLiteral("stationFileMalformed");
        return false;
    }
    const QJsonObject object = document.object();
    if (object.size() != 3 || !object.value(QStringLiteral("url")).isString()
        || !object.value(QStringLiteral("token")).isString()
        || !object.value(QStringLiteral("fingerprint")).isString()) {
        failure = QStringLiteral("stationFileSchema");
        return false;
    }
    const QString urlText = object.value(QStringLiteral("url")).toString();
    credentials.token = object.value(QStringLiteral("token")).toString();
    credentials.fingerprint = object.value(QStringLiteral("fingerprint"))
                                  .toString().toUpper();
    QString urlFailure;
    if (!RemoteStationOptions::isValidStationUrl(urlText, &urlFailure)
        || !urlText.startsWith(QStringLiteral("wss://"), Qt::CaseInsensitive)) {
        failure = QStringLiteral("stationUrlInvalid");
        return false;
    }
    credentials.url = QUrl(urlText);
    static const QRegularExpression fingerprintPattern(
        QStringLiteral("^(?:[0-9A-F]{2}:){31}[0-9A-F]{2}$"));
    if (credentials.token.isEmpty() || credentials.token.size() > 4096
        || credentials.token.contains(QChar::Null)
        || !fingerprintPattern.match(credentials.fingerprint).hasMatch()) {
        failure = QStringLiteral("stationCredentialsInvalid");
        return false;
    }
    return true;
}

QByteArray frameContentHash(const DisplayCodecFrame& frame)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const auto addPlane = [&hash](const QVector<float>& values) {
        hash.addData(QByteArrayView(
            reinterpret_cast<const char*>(values.constData()),
            values.size() * qsizetype(sizeof(float))));
    };
    addPlane(frame.traceDbm);
    addPlane(frame.waterfallDbm);
    addPlane(frame.wideDbm);
    return hash.result();
}

class MediaProbe final : public QObject {
public:
    MediaProbe(QCoreApplication* application, StationClient* client,
               RadioModel* model, int durationSeconds, bool audioRequested,
               QObject* parent = nullptr)
        : QObject(parent)
        , m_application(application)
        , m_client(client)
        , m_model(model)
        , m_durationSeconds(durationSeconds)
        , m_audioRequested(audioRequested)
    {
        m_clock.start();
        m_timeout.setSingleShot(true);
        m_timeout.setInterval(durationSeconds * 1000);
        connect(&m_timeout, &QTimer::timeout, this, [this] { finish(); });
        m_slicePoll.setInterval(100);
        connect(&m_slicePoll, &QTimer::timeout, this,
                [this] { subscribeFirstLiveSlice(); });

        connect(client, &StationClient::handshakeComplete, this,
                [this] { startMedia(); });
        connect(client, &StationClient::mediaControlReceived, this,
                [this](const QJsonObject& payload, quint32 epoch) {
                    receiveControl(payload, epoch);
                });
        connect(client, &StationClient::mediaSessionEnded, this,
                [this](quint32 epoch) {
                    if (!m_finished && epoch == m_epoch) {
                        fail(QStringLiteral("mediaSessionEnded"));
                    }
                });
        connect(client, &StationClient::sessionEnded, this,
                [this](const QString&) {
                    if (!m_finished) {
                        fail(QStringLiteral("controlSessionEnded"));
                    }
                });
    }

    void begin(const StationCredentials& credentials)
    {
        m_timeout.start();
        m_client->connectToStation(credentials.url, credentials.token,
                                   credentials.fingerprint);
    }

private:
    void startMedia()
    {
        if (m_finished || !m_client->mediaAvailable()) {
            fail(QStringLiteral("mediaUnavailable"));
            return;
        }
        m_epoch = m_client->sessionEpoch();
        m_connectionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        auto* peer = new MediaPeer(this);
        m_peer = peer;
        connect(peer, &MediaPeer::controlReady, this,
                [this, peer](const QJsonObject& payload) {
                    if (isCurrent(peer)) {
                        send(payload);
                    }
                });
        connect(peer, &MediaPeer::displayReceived, this,
                [this, peer](const QByteArray& packet) {
                    if (isCurrent(peer)) {
                        receiveDisplay(packet);
                    }
                });
        connect(peer, &MediaPeer::rtpReceived, this,
                [this, peer](const QByteArray& packet) {
                    if (isCurrent(peer)) {
                        receiveAudio(packet);
                    }
                });
        connect(peer, &MediaPeer::ready, this, [this, peer] {
            if (!isCurrent(peer)) {
                return;
            }
            m_mediaReady = true;
            subscribeFirstLiveSlice();
            requestAudio();
            if (!m_subscribed) {
                m_slicePoll.start();
            }
        });
        connect(peer, &MediaPeer::closed, this, [this, peer] {
            if (isCurrent(peer)) {
                fail(QStringLiteral("mediaPeerClosed"));
            }
        });
        connect(peer, &MediaPeer::errorOccurred, this,
                [this, peer](const QString&) {
                    if (isCurrent(peer)) {
                        fail(QStringLiteral("mediaPeerError"));
                    }
                });
        if (!peer->start(IMediaTransport::Role::Answerer, m_connectionId)) {
            fail(QStringLiteral("mediaPeerStartFailed"));
            return;
        }
        if (!send({{QStringLiteral("op"), QStringLiteral("start")}})) {
            fail(QStringLiteral("mediaStartSendFailed"));
        }
    }

    bool isCurrent(const MediaPeer* peer) const
    {
        return !m_finished && m_peer == peer && m_client->mediaAvailable()
            && m_client->sessionEpoch() == m_epoch;
    }

    bool send(QJsonObject payload)
    {
        if (m_finished || m_connectionId.isEmpty()) {
            return false;
        }
        payload.insert(QStringLiteral("connectionId"), m_connectionId);
        return m_client->sendMediaControl(payload, m_epoch);
    }

    void subscribeFirstLiveSlice()
    {
        if (m_finished || m_subscribed || !m_mediaReady) {
            return;
        }
        SliceModel* selected = nullptr;
        for (SliceModel* slice : m_model->slices()) {
            if (slice && slice->streamIndex() >= 0 && slice->sampleRateHz() > 0
                && std::isfinite(slice->frequency())) {
                selected = slice;
                break;
            }
        }
        if (!selected) {
            return;
        }
        const double spanHz = double(selected->sampleRateHz()) / 4.0;
        QJsonObject subscription{
            {QStringLiteral("op"), QStringLiteral("subscribe")},
            {QStringLiteral("endpointId"), double(kEndpointId)},
            {QStringLiteral("revision"), double(kRevision)},
            {QStringLiteral("sliceId"), selected->sliceIndex()},
            {QStringLiteral("tier"), QStringLiteral("wide")},
            {QStringLiteral("fftSize"), 4096},
            {QStringLiteral("windowType"), int(WindowFunction::BlackmanHarris4)},
            {QStringLiteral("centreHz"), selected->frequency()},
            {QStringLiteral("spanHz"), spanHz},
            {QStringLiteral("pixels"), 1024},
            {QStringLiteral("fps"), 15},
            {QStringLiteral("framesPerLine"), 1},
            {QStringLiteral("trace"), QJsonObject{
                 {QStringLiteral("detector"), int(SpectrumDetectorMode::Peak)},
                 {QStringLiteral("averageMode"), 0},
                 {QStringLiteral("averageAlpha"), 0.0}}},
            {QStringLiteral("waterfall"), QJsonObject{
                 {QStringLiteral("detector"), int(SpectrumDetectorMode::RMS)},
                 {QStringLiteral("averageMode"), 0},
                 {QStringLiteral("averageAlpha"), 0.0}}},
            {QStringLiteral("minDbm"), -180.0},
            {QStringLiteral("maxDbm"), 0.0},
            {QStringLiteral("wideSpanFactor"), 4.0},
        };
        if (!send(subscription)) {
            fail(QStringLiteral("subscribeSendFailed"));
            return;
        }
        m_subscribed = true;
        m_slicePoll.stop();
    }

    void requestAudio()
    {
        if (!m_audioRequested || m_audioControlSent || m_finished || !m_mediaReady) {
            return;
        }
        if (!m_audioDecoder.isReady()) {
            fail(QStringLiteral("audioDecoderUnavailable"));
            return;
        }
        if (!send({{QStringLiteral("op"), QStringLiteral("audio")},
                   {QStringLiteral("revision"), double(kRevision)},
                   {QStringLiteral("enabled"), true}})) {
            fail(QStringLiteral("audioRequestSendFailed"));
            return;
        }
        m_audioControlSent = true;
    }

    bool acceptAudioContext(const QJsonObject& payload)
    {
        // The shared wire codec, in the shape this session negotiated
        // (minor 8 adds the encoder profile or the off reason), then this
        // probe's identity checks.
        const std::optional<RemoteAudioContextMessage> context = decodeRemoteAudioContext(
            payload, m_client->remoteAudioStatusAvailable());
        if (!m_audioRequested || !context || context->connectionId != m_connectionId
            || context->revision != kRevision || !m_peer
            || context->ssrc != m_peer->audioSsrc()) {
            return false;
        }
        const quint32 generation = context->generation;
        if (m_audioGeneration != 0
            && (generation == m_audioGeneration
                || quint32(generation - m_audioGeneration) >= 0x80000000U)) {
            return false;
        }
        m_audioGeneration = generation;
        m_audioSsrc = context->ssrc;
        m_audioFirstSequence = context->firstSequence;
        m_audioFirstTimestamp = context->firstTimestamp;
        // The answerer's ready callback can precede the offerer's. Core
        // then reports disabled until its own peer is ready, followed by a
        // newer enabled context. Keep waiting within the bounded probe.
        m_audioContextAccepted = context->enabled;
        m_audioHavePrevious = false;
        m_audioDecoder.reset();
        return true;
    }

    void receiveAudio(const QByteArray& packet)
    {
        ++m_audioPackets;
        m_audioBytes += static_cast<quint64>(packet.size());
        if (!m_audioContextAccepted) {
            ++m_audioPreContextPackets;
            return;
        }
        const auto header = inspectOpusRtp(packet, m_audioSsrc);
        if (header.status != OpusAudioCodecStatus::Accepted) {
            ++m_audioRejected;
            ++m_audioStatusCounts[opusStatusName(header.status)];
            return;
        }
        // Reject retired context data before it can change decoder history.
        if (static_cast<qint32>(header.timestamp - m_audioFirstTimestamp) < 0) {
            ++m_audioRejected;
            ++m_audioStatusCounts[QStringLiteral("oldTimestamp")];
            return;
        }
        const OpusRtpDecodeResult decoded = m_audioDecoder.decodeRtp(packet, m_audioSsrc);
        ++m_audioStatusCounts[opusStatusName(decoded.status)];
        if (decoded.status != OpusAudioCodecStatus::Accepted) {
            ++m_audioRejected;
            return;
        }
        ++m_audioDecoded;
        m_audioChannels = decoded.packetInfo.channels;
        m_audioBandwidth = decoded.packetInfo.bandwidth;
        if (m_audioHavePrevious) {
            if (decoded.sequence != static_cast<quint16>(m_audioPreviousSequence + 1)) {
                ++m_audioSequenceDiscontinuities;
            }
            if (decoded.timestamp != m_audioPreviousTimestamp + OpusAudioCodecConfig::kFrameSamples) {
                ++m_audioTimestampDiscontinuities;
            }
        } else if (decoded.sequence != m_audioFirstSequence
                   || decoded.timestamp != m_audioFirstTimestamp) {
            ++m_audioContextDiscontinuities;
        }
        m_audioPreviousSequence = decoded.sequence;
        m_audioPreviousTimestamp = decoded.timestamp;
        m_audioHavePrevious = true;

        double leftSquared = 0.0;
        double rightSquared = 0.0;
        for (int index = 0; index < decoded.pcmInterleaved.size(); index += 2) {
            const float left = decoded.pcmInterleaved.at(index);
            const float right = decoded.pcmInterleaved.at(index + 1);
            if (!std::isfinite(left) || !std::isfinite(right)) {
                m_audioPcmFinite = false;
                ++m_audioRejected;
                return;
            }
            leftSquared += static_cast<double>(left) * left;
            rightSquared += static_cast<double>(right) * right;
        }
        m_audioPcmFrames += static_cast<quint64>(decoded.pcmInterleaved.size() / 2);
        m_audioLeftSquared += leftSquared;
        m_audioRightSquared += rightSquared;
    }

    void receiveControl(const QJsonObject& payload, quint32 epoch)
    {
        if (m_finished || epoch != m_epoch || !m_peer
            || payload.value(QStringLiteral("connectionId")).toString()
                != m_connectionId) {
            return;
        }
        const QString op = payload.value(QStringLiteral("op")).toString();
        if (op == QLatin1String("description") || op == QLatin1String("candidate")) {
            if (!m_peer->acceptControl(payload)) {
                fail(QStringLiteral("mediaControlRejected"));
            }
            return;
        }
        if (op == QLatin1String("rejected")) {
            fail(QStringLiteral("subscriptionRejected"));
            return;
        }
        if (op == QLatin1String("audio-context")) {
            if (!m_audioRequested) {
                return;
            }
            if (!acceptAudioContext(payload)) {
                fail(QStringLiteral("audioContextInvalid"));
            }
            return;
        }
        if (op != QLatin1String("context") || payload.size() != 19
            || payload.value(QStringLiteral("connectionId")).toString()
                != m_connectionId) {
            return;
        }

        quint32 endpointId = 0;
        quint32 revision = 0;
        DisplayCodecContext codec;
        double sourceStream = 0.0;
        double sourceCentre = 0.0;
        double sampleRate = 0.0;
        double centre = 0.0;
        double span = 0.0;
        double wideCentre = 0.0;
        double wideSpan = 0.0;
        double traceSamples = 0.0;
        double waterfallSamples = 0.0;
        double wideSamples = 0.0;
        double minDbm = 0.0;
        double maxDbm = 0.0;
        double fps = 0.0;
        double framesPerLine = 0.0;
        if (!jsonUint32(payload, "endpointId", endpointId)
            || endpointId != kEndpointId
            || !jsonUint32(payload, "revision", revision)
            || revision != kRevision
            || !jsonUint32(payload, "contextGeneration", codec.contextGeneration)
            || !jsonNumber(payload, "sourceStream", 0, 255, sourceStream, true)
            || !jsonNumber(payload, "sourceCentreHz", 0, 1.0e12, sourceCentre)
            || !jsonNumber(payload, "sampleRateHz", 1, 1.0e8, sampleRate)
            || !jsonNumber(payload, "centreHz", 0, 1.0e12, centre)
            || !jsonNumber(payload, "spanHz", 0.000001, sampleRate, span)
            || !jsonNumber(payload, "wideCentreHz", 0, 1.0e12, wideCentre)
            || !jsonNumber(payload, "wideSpanHz", 0, sampleRate, wideSpan)
            || !jsonNumber(payload, "traceSamples", 1,
                           SpectrumEndpoint::kMaxPixels, traceSamples, true)
            || !jsonNumber(payload, "waterfallSamples", 1,
                           SpectrumEndpoint::kMaxPixels, waterfallSamples, true)
            || !jsonNumber(payload, "wideSamples", 0,
                           SpectrumEndpoint::kMaxWideSamples, wideSamples, true)
            || !jsonNumber(payload, "minDbm", -400, 100, minDbm)
            || !jsonNumber(payload, "maxDbm", -400, 100, maxDbm)
            || minDbm >= maxDbm
            || !jsonNumber(payload, "fps", 1, 60, fps, true)
            || !jsonNumber(payload, "framesPerLine", 1, 10000,
                           framesPerLine, true)
            || (wideSamples == 0) != (wideSpan == 0)) {
            fail(QStringLiteral("contextInvalid"));
            return;
        }
        codec.endpointId = endpointId;
        codec.traceSamples = quint16(traceSamples);
        codec.waterfallSamples = quint16(waterfallSamples);
        codec.wideSamples = quint16(wideSamples);
        codec.minDbm = float(minDbm);
        codec.maxDbm = float(maxDbm);
        m_codecContext = codec;
        m_contextCentreHz = centre;
        m_contextSpanHz = span;
        m_sourceCentreHz = sourceCentre;
        m_sampleRateHz = sampleRate;
        m_contextAccepted = true;
        m_decoder.reset();
        requestKeyframe();
    }

    void requestKeyframe()
    {
        if (!m_contextAccepted) {
            return;
        }
        const qint64 now = m_clock.elapsed();
        if (now - m_lastKeyframeMs < 200) {
            return;
        }
        m_lastKeyframeMs = now;
        send({{QStringLiteral("op"), QStringLiteral("keyframe")},
              {QStringLiteral("endpointId"), double(kEndpointId)},
              {QStringLiteral("contextGeneration"),
               double(m_codecContext.contextGeneration)}});
    }

    void receiveDisplay(const QByteArray& packet)
    {
        ++m_receivedFrames;
        m_receivedEncodedBytes += quint64(packet.size());
        if (!m_contextAccepted || packet.size() < 42
            || packet.size() > DisplayCodecEncoder::kMaxEncodedBytes
            || packet.first(4) != QByteArrayLiteral("NSDC")
            || qFromBigEndian<quint32>(packet.constData() + 8) != kEndpointId
            || qFromBigEndian<quint32>(packet.constData() + 12)
                != m_codecContext.contextGeneration) {
            ++m_rejected;
            ++m_reasonCounts[QStringLiteral("envelope")];
            return;
        }
        const DisplayCodecDecodeResult decoded = m_decoder.decode(packet);
        ++m_reasonCounts[reasonName(decoded.reason)];
        switch (decoded.disposition) {
        case DisplayCodecDisposition::Accepted:
            ++m_accepted;
            if (decoded.frame.waterfallAdvance) {
                ++m_waterfallAdvances;
            }
            if (!decoded.frame.wideDbm.isEmpty()) {
                ++m_wideFrames;
            }
            m_distinctFrames.insert(frameContentHash(decoded.frame));
            break;
        case DisplayCodecDisposition::NeedKeyframe:
            ++m_needKeyframe;
            requestKeyframe();
            break;
        case DisplayCodecDisposition::Rejected:
            ++m_rejected;
            break;
        }
    }

    void fail(const QString& code)
    {
        if (m_failure.isEmpty()) {
            m_failure = code;
        }
        finish();
    }

    void finish()
    {
        if (m_finished) {
            return;
        }
        m_finished = true;
        m_timeout.stop();
        m_slicePoll.stop();
        const bool passed = m_failure.isEmpty() && m_contextAccepted
            && m_accepted >= kMinimumAcceptedFrames
            && m_distinctFrames.size() >= kMinimumDistinctFrames
            && m_waterfallAdvances >= kMinimumWaterfallAdvances
            && m_wideFrames > 0
            && (!m_audioRequested || (m_audioContextAccepted && m_audioPackets > 0
                && m_audioDecoded > 0 && m_audioPcmFinite && m_audioPcmFrames > 0));
        if (!passed && m_failure.isEmpty()) {
            m_failure = m_audioRequested ? QStringLiteral("insufficientAudioOrDisplayMedia")
                                         : QStringLiteral("insufficientChangingFrames");
        }

        if (m_subscribed) {
            QJsonObject unsubscribe{
                {QStringLiteral("op"), QStringLiteral("unsubscribe")},
                {QStringLiteral("connectionId"), m_connectionId},
                {QStringLiteral("endpointId"), double(kEndpointId)},
            };
            m_client->sendMediaControl(unsubscribe, m_epoch);
        }
        if (m_peer) {
            disconnect(m_peer, nullptr, this, nullptr);
            m_peer->stop();
            m_peer->deleteLater();
            m_peer = nullptr;
        }
        m_client->disconnectFromStation(QStringLiteral("media probe complete"));

        QJsonObject dispositions{
            {QStringLiteral("accepted"), double(m_accepted)},
            {QStringLiteral("needKeyframe"), double(m_needKeyframe)},
            {QStringLiteral("rejected"), double(m_rejected)},
        };
        QJsonObject reasons;
        for (auto it = m_reasonCounts.cbegin(); it != m_reasonCounts.cend(); ++it) {
            reasons.insert(it.key(), double(it.value()));
        }
        QJsonObject context{
            {QStringLiteral("centreHz"), m_contextCentreHz},
            {QStringLiteral("spanHz"), m_contextSpanHz},
            {QStringLiteral("sourceCentreHz"), m_sourceCentreHz},
            {QStringLiteral("sampleRateHz"), m_sampleRateHz},
            {QStringLiteral("traceSamples"), m_codecContext.traceSamples},
            {QStringLiteral("waterfallSamples"), m_codecContext.waterfallSamples},
            {QStringLiteral("wideSamples"), m_codecContext.wideSamples},
            {QStringLiteral("contextGeneration"),
             double(m_codecContext.contextGeneration)},
        };
        QJsonObject audio;
        if (m_audioRequested) {
            QJsonObject statuses;
            for (auto it = m_audioStatusCounts.cbegin(); it != m_audioStatusCounts.cend(); ++it) {
                statuses.insert(it.key(), double(it.value()));
            }
            const double leftRms = m_audioPcmFrames
                ? std::sqrt(m_audioLeftSquared / double(m_audioPcmFrames)) : 0.0;
            const double rightRms = m_audioPcmFrames
                ? std::sqrt(m_audioRightSquared / double(m_audioPcmFrames)) : 0.0;
            audio = {{QStringLiteral("contextAccepted"), m_audioContextAccepted},
                     {QStringLiteral("contextGeneration"), double(m_audioGeneration)},
                     {QStringLiteral("packetCount"), double(m_audioPackets)},
                     {QStringLiteral("actualReceivedBytes"), double(m_audioBytes)},
                     {QStringLiteral("decodedPackets"), double(m_audioDecoded)},
                     {QStringLiteral("rejectedPackets"), double(m_audioRejected)},
                     {QStringLiteral("preContextPackets"), double(m_audioPreContextPackets)},
                     {QStringLiteral("sequenceDiscontinuities"), double(m_audioSequenceDiscontinuities)},
                     {QStringLiteral("timestampDiscontinuities"), double(m_audioTimestampDiscontinuities)},
                     {QStringLiteral("contextDiscontinuities"), double(m_audioContextDiscontinuities)},
                     {QStringLiteral("channels"), m_audioChannels},
                     {QStringLiteral("bandwidth"), m_audioBandwidth},
                     {QStringLiteral("pcmFinite"), m_audioPcmFinite},
                     {QStringLiteral("pcmFrames"), double(m_audioPcmFrames)},
                     {QStringLiteral("leftRms"), leftRms},
                     {QStringLiteral("rightRms"), rightRms},
                     {QStringLiteral("decodeStatuses"), statuses}};
        }
        QJsonObject report{
            {QStringLiteral("status"), passed ? QStringLiteral("ok")
                                               : QStringLiteral("failed")},
            {QStringLiteral("failure"), m_failure},
            {QStringLiteral("durationSeconds"), m_durationSeconds},
            {QStringLiteral("receivedFrames"), double(m_receivedFrames)},
            {QStringLiteral("distinctFrames"), double(m_distinctFrames.size())},
            {QStringLiteral("waterfallAdvances"), double(m_waterfallAdvances)},
            {QStringLiteral("wideFrames"), double(m_wideFrames)},
            {QStringLiteral("context"), context},
            {QStringLiteral("codecDispositions"), dispositions},
            {QStringLiteral("codecReasons"), reasons},
            {QStringLiteral("actualReceivedEncodedBytes"),
             double(m_receivedEncodedBytes)},
        };
        if (m_audioRequested) { report.insert(QStringLiteral("audio"), audio); }
        const QByteArray output = QJsonDocument(report).toJson(QJsonDocument::Compact)
            + '\n';
        std::fwrite(output.constData(), 1, size_t(output.size()), stdout);
        std::fflush(stdout);
        m_application->exit(passed ? 0 : 1);
    }

    QCoreApplication* m_application = nullptr;
    StationClient* m_client = nullptr;
    RadioModel* m_model = nullptr;
    QPointer<MediaPeer> m_peer;
    QTimer m_timeout;
    QTimer m_slicePoll;
    QElapsedTimer m_clock;
    DisplayCodecDecoder m_decoder;
    OpusAudioDecoder m_audioDecoder;
    DisplayCodecContext m_codecContext;
    QSet<QByteArray> m_distinctFrames;
    QMap<QString, quint64> m_reasonCounts;
    QMap<QString, quint64> m_audioStatusCounts;
    QString m_connectionId;
    QString m_failure;
    quint32 m_epoch = 0;
    quint32 m_audioGeneration = 0;
    quint32 m_audioSsrc = 0;
    quint32 m_audioFirstTimestamp = 0;
    quint32 m_audioPreviousTimestamp = 0;
    quint16 m_audioFirstSequence = 0;
    quint16 m_audioPreviousSequence = 0;
    qint64 m_lastKeyframeMs = -1000;
    quint64 m_receivedEncodedBytes = 0;
    quint64 m_receivedFrames = 0;
    quint64 m_accepted = 0;
    quint64 m_needKeyframe = 0;
    quint64 m_rejected = 0;
    quint64 m_waterfallAdvances = 0;
    quint64 m_wideFrames = 0;
    quint64 m_audioBytes = 0;
    quint64 m_audioPackets = 0;
    quint64 m_audioDecoded = 0;
    quint64 m_audioRejected = 0;
    quint64 m_audioPreContextPackets = 0;
    quint64 m_audioSequenceDiscontinuities = 0;
    quint64 m_audioTimestampDiscontinuities = 0;
    quint64 m_audioContextDiscontinuities = 0;
    quint64 m_audioPcmFrames = 0;
    double m_audioLeftSquared = 0.0;
    double m_audioRightSquared = 0.0;
    double m_contextCentreHz = 0.0;
    double m_contextSpanHz = 0.0;
    double m_sourceCentreHz = 0.0;
    double m_sampleRateHz = 0.0;
    int m_audioChannels = 0;
    int m_audioBandwidth = 0;
    int m_durationSeconds = kDefaultSeconds;
    bool m_mediaReady = false;
    bool m_subscribed = false;
    bool m_contextAccepted = false;
    bool m_audioRequested = false;
    bool m_audioControlSent = false;
    bool m_audioContextAccepted = false;
    bool m_audioHavePrevious = false;
    bool m_audioPcmFinite = true;
    bool m_finished = false;
};

void writeStartupFailure(const QString& failure)
{
    const QByteArray output = QJsonDocument(QJsonObject{
        {QStringLiteral("status"), QStringLiteral("failed")},
        {QStringLiteral("failure"), failure},
    }).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(output.constData(), 1, size_t(output.size()), stdout);
    std::fflush(stdout);
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("nereus-media-probe"));
    application.setApplicationVersion(QStringLiteral(NEREUSSDR_VERSION));
    application.setOrganizationName(QStringLiteral("NereusSDR"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Bounded headless probe for encrypted remote display media."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption stationFileOption(
        QStringLiteral("station-file"),
        QStringLiteral("Private JSON file containing url, token, and fingerprint."),
        QStringLiteral("path"));
    QCommandLineOption secondsOption(
        QStringLiteral("seconds"),
        QStringLiteral("Bounded capture duration, 1 through 300 seconds."),
        QStringLiteral("seconds"), QString::number(kDefaultSeconds));
    QCommandLineOption audioOption(
        QStringLiteral("audio"),
        QStringLiteral("Also request and validate decrypted 48 kHz stereo Opus RTP."));
    parser.addOption(stationFileOption);
    parser.addOption(secondsOption);
    parser.addOption(audioOption);
    parser.process(application);

    bool durationValid = false;
    const int durationSeconds = parser.value(secondsOption).toInt(&durationValid);
    if (!parser.isSet(stationFileOption) || !durationValid
        || durationSeconds < 1 || durationSeconds > kMaximumSeconds) {
        writeStartupFailure(QStringLiteral("argumentsInvalid"));
        return 2;
    }
    StationCredentials credentials;
    QString failure;
    if (!loadCredentials(parser.value(stationFileOption), credentials, failure)) {
        writeStartupFailure(failure);
        return 2;
    }

    const QString profile = QStringLiteral("media-probe-%1")
                                .arg(QCoreApplication::applicationPid());
    AppSettings::setProfileOverride(profile);
    if (!CoreInit::initialize(profile)) {
        writeStartupFailure(QStringLiteral("coreInitializationFailed"));
        return 2;
    }
    qRegisterMetaType<RadioConnectionError>();
    qRegisterMetaType<AudioDeviceConfig>();

    int result = 2;
    {
        SettingsProxy settingsProxy;
        AppSettings::instance().setRemoteBackend(&settingsProxy);
        RadioModel remoteModel(RadioModel::Role::Remote);
        StationClient client(&remoteModel, &settingsProxy);
        MediaProbe probe(&application, &client, &remoteModel, durationSeconds,
                         parser.isSet(audioOption));
        probe.begin(credentials);
        credentials.token.fill(QChar::Null);
        result = application.exec();
        AppSettings::instance().setRemoteBackend(nullptr);
    }
    CoreInit::shutdown();
    QDir(AppSettings::resolveConfigDir(profile)).removeRecursively();
    return result;
}
