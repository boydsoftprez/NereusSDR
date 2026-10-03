// =================================================================
// tests/tst_mic_line_phone_answer.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test.
//
// Mic 48k lane: the Core's real offerer (LibDataChannelMediaTransport)
// against an answerer that writes its microphone line the way the phone
// does. The phone runs libdatachannel through its C API; this answerer
// makes the same calls through the C++ API, in the same order
// (NereusKit MediaPeer.swift setRemoteDescription, :443-482 on
// claude/iphone-audioquality at bf72cdb30):
//
//   setRemoteDescription(offer); the offered microphone track's own
//   description (rtcGetTrackDescription, track->description()) goes
//   through microphoneAnswer (MediaPeer.swift:1016-1046), and the result
//   is added as a track (rtcAddTrack, RtcBridge.swift:311-316); then
//   setLocalDescription(answer) with auto negotiation off, and the
//   answer's candidates are taken out before it is sent
//   (withoutCandidates, MediaPeer.swift:990-1000).
//
// microphoneAnswer keeps the offered Opus fmtp line, so the phone's answer
// echoes the Core's maxaveragebitrate; its tests pin the lines it writes
// (MicrophoneLosslessTests.swift:15-35), and its interop run against this
// offerer sees Opus 111 first and L16 96 beside it
// (MediaControlInteropTests.swift:100-115).
//
// Two rows. "opus-48k": the offer advertises maxaveragebitrate=48000, the
// phone-shaped answer is accepted, and Opus from the phone's encoder at
// 48 kbit/s (OpusEncoder.swift:58-73) crosses and the Core's receiver
// decodes it. "lossless": with the lossless format offered, the
// phone-shaped answer keeps L16 at 96, the Core grants lossless on the
// microphone line (micLosslessNegotiated), and the phone's 4 ms L16 packets
// (mono in both channels, as the desktop sends them) cross byte for byte
// and the Core's receiver decodes them to the level sent.
//
// Labelled realtime: a real encrypted loopback handshake inside a fixed
// wait, as tst_media_transport.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-01: original test for NereusSDR by J.J. Boyd (KG4VCF), Mic 48k
//               lane, with AI-assisted implementation via Anthropic Claude
//               Code. Fix wave: the phone encoder's create and settings
//               are checked.
// =================================================================

#include "RealtimeTestLoad.h"

#include "core/session/media/LibDataChannelMediaTransport.h"
#include "core/session/media/OpusAudioCodec.h"
#include "core/session/media/PcmAudioCodec.h"
#include "core/session/media/RemoteMicReceiver.h"

#include <QRegularExpression>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>

#include <rtc/rtc.hpp>

#include <opus.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <numbers>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr quint32 kAudioSsrc = 0x4e523301U;
constexpr quint32 kMicSsrc = 0x6d696331U;
constexpr int kOpusPayloadType = 111;
constexpr int kPackets = 25;

QStringList splitLines(const QString& text)
{
    QStringList lines;
    for (const QString& line : text.split(QRegularExpression(QStringLiteral("\r\n|\n")))) {
        if (!line.isEmpty()) {
            lines << line;
        }
    }
    return lines;
}

// One media section of a description, by its a=mid.
QStringList mediaSection(const QString& sdp, const QString& mid)
{
    QStringList section;
    QStringList current;
    const auto flush = [&] {
        if (current.contains(QStringLiteral("a=mid:") + mid)) {
            section = current;
        }
        current.clear();
    };
    for (const QString& line : splitLines(sdp)) {
        if (line.startsWith(QLatin1String("m="))) {
            flush();
        }
        if (!current.isEmpty() || line.startsWith(QLatin1String("m="))) {
            current << line;
        }
    }
    flush();
    return section;
}

// MediaPeer.describesL16 (MediaPeer.swift:1050-1055).
bool describesL16(const QString& media)
{
    for (const QString& line : splitLines(media)) {
        if (line.toLower().startsWith(QStringLiteral("a=rtpmap:%1 l16/48000/2")
                                          .arg(PcmAudioCodecConfig::kPayloadType))) {
            return true;
        }
    }
    return false;
}

// MediaPeer.describesOpus (MediaPeer.swift:1057-1062).
bool describesOpus(const QString& media)
{
    for (const QString& line : splitLines(media)) {
        if (line.toLower().startsWith(QStringLiteral("a=rtpmap:%1 opus/48000")
                                          .arg(kOpusPayloadType))) {
            return true;
        }
    }
    return false;
}

// MediaPeer.microphoneAnswer (MediaPeer.swift:1016-1046), line for line:
// send-only, Opus, L16 at 96 beside it when the offer carries it, the
// offered lines kept for those payload types only, and
// a=ssrc:<ssrc> cname:nereus-microphone. Empty when the offered line
// carries no Opus.
QString phoneMicrophoneAnswer(const QString& offered, quint32 ssrc)
{
    const QString opus = QString::number(kOpusPayloadType);
    const QString l16 = QString::number(PcmAudioCodecConfig::kPayloadType);
    std::set<QString> kept{opus};
    if (describesL16(offered)) {
        kept.insert(l16);
    }
    QStringList lines = splitLines(offered);
    if (lines.isEmpty() || !lines.constFirst().startsWith(QLatin1String("m="))
        || !describesOpus(offered)) {
        return {};
    }
    QStringList fields = lines.constFirst().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (fields.size() < 4) {
        return {};
    }
    fields = fields.mid(0, 3);
    fields << opus;
    if (kept.size() > 1) {
        fields << l16;
    }
    lines[0] = fields.join(QLatin1Char(' '));
    const QStringList directions{QStringLiteral("a=sendrecv"), QStringLiteral("a=recvonly"),
                                 QStringLiteral("a=sendonly"), QStringLiteral("a=inactive")};
    QStringList filtered;
    for (const QString& line : lines) {
        if (directions.contains(line) || line.startsWith(QLatin1String("a=ssrc:"))
            || line.startsWith(QLatin1String("a=msid"))) {
            continue;
        }
        bool keep = true;
        for (const QLatin1String prefix : {QLatin1String("a=rtpmap:"), QLatin1String("a=fmtp:"),
                                           QLatin1String("a=rtcp-fb:")}) {
            if (line.startsWith(prefix)) {
                const QString type = line.mid(prefix.size()).section(QLatin1Char(' '), 0, 0);
                keep = kept.count(type) > 0;
                break;
            }
        }
        if (keep) {
            filtered << line;
        }
    }
    qsizetype midIndex = 0;
    for (qsizetype i = 0; i < filtered.size(); ++i) {
        if (filtered.at(i).startsWith(QLatin1String("a=mid:"))) {
            midIndex = i;
            break;
        }
    }
    filtered.insert(midIndex + 1, QStringLiteral("a=sendonly"));
    filtered << QStringLiteral("a=ssrc:%1 cname:nereus-microphone").arg(ssrc);
    return filtered.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n");
}

// MediaPeer.withoutCandidates (MediaPeer.swift:990-1000).
QString withoutCandidates(const QString& sdp)
{
    QStringList kept;
    for (const QString& line : sdp.split(QStringLiteral("\r\n"))) {
        if (!line.startsWith(QLatin1String("a=candidate:"))
            && !line.startsWith(QLatin1String("a=end-of-candidates"))) {
            kept << line;
        }
    }
    return kept.join(QStringLiteral("\r\n"));
}

// The phone's side of the media connection, on libdatachannel.
class PhoneAnswerer {
public:
    PhoneAnswerer()
    {
        rtc::Configuration config;
        // MediaPeer.Configuration's default (MediaPeer.swift:85).
        config.disableAutoNegotiation = true;
        m_peer = std::make_shared<rtc::PeerConnection>(config);
        m_peer->onLocalDescription([this](rtc::Description description) {
            const std::lock_guard lock(m_mutex);
            m_answer = QString::fromStdString(std::string(description));
        });
        m_peer->onLocalCandidate([this](rtc::Candidate candidate) {
            const std::lock_guard lock(m_mutex);
            m_candidates.append({QString::fromStdString(candidate.candidate()),
                                 QString::fromStdString(candidate.mid())});
        });
        m_peer->onTrack([this](std::shared_ptr<rtc::Track> track) {
            const std::lock_guard lock(m_mutex);
            m_offeredTracks[track->mid()] = std::move(track);
        });
        m_peer->onDataChannel([this](std::shared_ptr<rtc::DataChannel> channel) {
            const std::lock_guard lock(m_mutex);
            m_channels.push_back(std::move(channel));
        });
    }

    ~PhoneAnswerer()
    {
        if (m_peer) {
            m_peer->close();
        }
    }

    // MediaPeer.setRemoteDescription and describeMicrophoneLine. Returns
    // the microphone line's answer, empty when the line was rejected.
    QString takeOffer(const QString& offer, quint32 micSsrc)
    {
        m_peer->setRemoteDescription(rtc::Description(offer.toStdString(), "offer"));
        std::shared_ptr<rtc::Track> offeredMic;
        {
            const std::lock_guard lock(m_mutex);
            const auto it = m_offeredTracks.find("mic");
            if (it != m_offeredTracks.end()) {
                offeredMic = it->second;
            }
        }
        if (!offeredMic) {
            return {};
        }
        const QString offered = QString::fromStdString(std::string(offeredMic->description()));
        const QString answer = phoneMicrophoneAnswer(offered, micSsrc);
        if (answer.isEmpty()) {
            return {};
        }
        m_mic = m_peer->addTrack(rtc::Description::Media(answer.toStdString()));
        m_peer->setLocalDescription(rtc::Description::Type::Answer);
        return answer;
    }

    void addRemoteCandidate(const QString& candidate, const QString& mid)
    {
        m_peer->addRemoteCandidate(rtc::Candidate(candidate.toStdString(), mid.toStdString()));
    }

    QString answer() const
    {
        const std::lock_guard lock(m_mutex);
        return m_answer;
    }

    QList<QPair<QString, QString>> takeCandidates()
    {
        const std::lock_guard lock(m_mutex);
        return std::exchange(m_candidates, {});
    }

    bool micOpen() const { return m_mic && m_mic->isOpen(); }

    bool sendMic(const QByteArray& packet)
    {
        return m_mic
            && m_mic->send(reinterpret_cast<const std::byte*>(packet.constData()),
                           static_cast<size_t>(packet.size()));
    }

private:
    mutable std::mutex m_mutex;
    std::shared_ptr<rtc::PeerConnection> m_peer;
    std::map<std::string, std::shared_ptr<rtc::Track>> m_offeredTracks;
    std::vector<std::shared_ptr<rtc::DataChannel>> m_channels;
    std::shared_ptr<rtc::Track> m_mic;
    QString m_answer;
    QList<QPair<QString, QString>> m_candidates;
};

// The phone's microphone encoder (OpusEncoder.swift:58-73): mono, 48 kHz,
// 20 ms frames, VOIP, 48000 bit/s constrained VBR, in-band FEC for 10 %
// loss, no DTX, complexity 9, voice.
class PhoneOpusEncoder {
public:
    PhoneOpusEncoder()
    {
        int error = OPUS_OK;
        m_encoder = opus_encoder_create(RemoteMicConfig::kSampleRate, 1, OPUS_APPLICATION_VOIP,
                                        &error);
        if (m_encoder == nullptr || error != OPUS_OK) {
            return;
        }
        // Every setting must take, as the phone's encoder throws when one
        // does not (OpusEncoder.swift).
        const int settings[] = {
            opus_encoder_ctl(m_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE)),
            opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(48000)),
            opus_encoder_ctl(m_encoder, OPUS_SET_VBR(1)),
            opus_encoder_ctl(m_encoder, OPUS_SET_VBR_CONSTRAINT(1)),
            opus_encoder_ctl(m_encoder, OPUS_SET_INBAND_FEC(1)),
            opus_encoder_ctl(m_encoder, OPUS_SET_PACKET_LOSS_PERC(10)),
            opus_encoder_ctl(m_encoder, OPUS_SET_DTX(0)),
            opus_encoder_ctl(m_encoder, OPUS_SET_COMPLEXITY(9)),
        };
        m_ready = std::all_of(std::begin(settings), std::end(settings),
                              [](int result) { return result == OPUS_OK; });
    }

    bool isReady() const { return m_ready; }
    ~PhoneOpusEncoder()
    {
        if (m_encoder != nullptr) {
            opus_encoder_destroy(m_encoder);
        }
    }
    PhoneOpusEncoder(const PhoneOpusEncoder&) = delete;
    PhoneOpusEncoder& operator=(const PhoneOpusEncoder&) = delete;

    QByteArray packet(const float* mono, quint16 sequence, quint32 timestamp)
    {
        std::vector<unsigned char> payload(1500);
        const opus_int32 bytes =
            opus_encode_float(m_encoder, mono, RemoteMicConfig::kOpusFrameSamples, payload.data(),
                              static_cast<opus_int32>(payload.size()));
        if (bytes <= 0) {
            return {};
        }
        return buildAudioRtp(kOpusPayloadType, sequence, timestamp, kMicSsrc,
                             QByteArray(reinterpret_cast<const char*>(payload.data()), bytes));
    }

private:
    OpusEncoder* m_encoder{nullptr};
    bool m_ready{false};
};

} // namespace

class TestMicLinePhoneAnswer : public QObject {
    Q_OBJECT

private slots:
    void cleanup() { NereusSDR::RealtimeTestLoad::printLoadAverageIfFailed(); }
    void phoneAnswerIsGrantedAndDecoded_data();
    void phoneAnswerIsGrantedAndDecoded();
};

void TestMicLinePhoneAnswer::phoneAnswerIsGrantedAndDecoded_data()
{
    QTest::addColumn<bool>("lossless");
    QTest::newRow("opus-48k") << false;
    QTest::newRow("lossless") << true;
}

void TestMicLinePhoneAnswer::phoneAnswerIsGrantedAndDecoded()
{
    QFETCH(bool, lossless);
    LibDataChannelMediaTransport offerer;
    PhoneAnswerer phone;
    QString offer;
    QList<QPair<QString, QString>> offerCandidates;
    bool offerTaken = false;
    bool answerAccepted = false;
    QSignalSpy ready(&offerer, &IMediaTransport::ready);
    QSignalSpy errors(&offerer, &IMediaTransport::errorOccurred);
    connect(&offerer, &IMediaTransport::localDescription, this,
            [&offer](const QString& sdp, const QString&) { offer = sdp; });
    connect(&offerer, &IMediaTransport::localCandidate, this,
            [&](const QString& candidate, const QString& mid) {
                if (offerTaken) {
                    phone.addRemoteCandidate(candidate, mid);
                } else {
                    offerCandidates.append({candidate, mid});
                }
            });
    QList<QByteArray> received;
    connect(&offerer, &IMediaTransport::micRtpReceived, this,
            [&received](const QByteArray& packet) { received.append(packet); });
    // The phone's candidates reach the Core once its answer is in.
    QTimer relay;
    relay.setInterval(5);
    connect(&relay, &QTimer::timeout, this, [&] {
        if (!answerAccepted) {
            return;
        }
        for (const auto& [candidate, mid] : phone.takeCandidates()) {
            QVERIFY2(offerer.acceptCandidate(candidate, mid), qPrintable(candidate));
        }
    });
    relay.start();

    IMediaTransport::StartOptions options{IMediaTransport::Role::Offerer, kAudioSsrc};
    options.micAudioSsrc = kMicSsrc;
    options.offerLosslessAudio = lossless;
    QVERIFY(offerer.start(options));
    QTRY_VERIFY_WITH_TIMEOUT(!offer.isEmpty(), 10000);

    // The offer's microphone line advertises the 48 kbit/s ceiling.
    const QStringList micOffer = mediaSection(offer, QStringLiteral("mic"));
    QVERIFY2(!micOffer.isEmpty(), qPrintable(offer));
    QVERIFY2(micOffer.contains(QStringLiteral(
                 "a=fmtp:111 minptime=10;useinbandfec=1;stereo=0;maxaveragebitrate=48000")),
             qPrintable(micOffer.join(QLatin1Char('\n'))));
    QCOMPARE(micOffer.contains(QStringLiteral("a=rtpmap:96 L16/48000/2")), lossless);

    const QString micAnswer = phone.takeOffer(offer, kMicSsrc);
    QVERIFY2(!micAnswer.isEmpty(), "the phone rejected the microphone line");
    offerTaken = true;
    for (const auto& [candidate, mid] : std::exchange(offerCandidates, {})) {
        phone.addRemoteCandidate(candidate, mid);
    }
    QTRY_VERIFY_WITH_TIMEOUT(!phone.answer().isEmpty(), 10000);
    const QString answer = withoutCandidates(phone.answer());

    // The answer's microphone line as the phone writes it: send-only, its
    // SSRC, the Core's Opus fmtp echoed, and L16 at 96 when offered.
    const QStringList micAnswerLines = mediaSection(answer, QStringLiteral("mic"));
    QVERIFY2(!micAnswerLines.isEmpty(), qPrintable(answer));
    QCOMPARE(micAnswerLines.constFirst(),
             lossless ? QStringLiteral("m=audio 9 UDP/TLS/RTP/SAVPF 111 96")
                      : QStringLiteral("m=audio 9 UDP/TLS/RTP/SAVPF 111"));
    QVERIFY(micAnswerLines.contains(QStringLiteral("a=sendonly")));
    QVERIFY(micAnswerLines.contains(
        QStringLiteral("a=ssrc:%1 cname:nereus-microphone").arg(kMicSsrc)));
    QVERIFY2(micAnswerLines.contains(QStringLiteral(
                 "a=fmtp:111 minptime=10;useinbandfec=1;stereo=0;maxaveragebitrate=48000")),
             qPrintable(micAnswerLines.join(QLatin1Char('\n'))));
    QCOMPARE(micAnswerLines.contains(QStringLiteral("a=rtpmap:96 L16/48000/2")), lossless);

    QVERIFY2(offerer.acceptDescription(answer, QStringLiteral("answer")), qPrintable(answer));
    answerAccepted = true;
    QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(phone.micOpen(), 10000);
    QCOMPARE(errors.count(), 0);
    QCOMPARE(offerer.micLosslessNegotiated(), lossless);

    // The phone's microphone: 25 packets of a 440 Hz tone at 0.25, L16 in
    // 4 ms packets with the mono sample in both channels, or Opus in 20 ms
    // packets from the phone's encoder.
    QList<QByteArray> sent;
    if (lossless) {
        constexpr int kFrames = PcmAudioCodecConfig::kPacketFrames;
        for (int k = 0; k < kPackets; ++k) {
            QVector<float> stereo(kFrames * 2, 0.25f);
            sent << PcmAudioPacketiser{}
                        .encode(stereo, static_cast<quint16>(k + 1),
                                static_cast<quint32>(k * kFrames), kMicSsrc)
                        .packet;
        }
    } else {
        PhoneOpusEncoder encoder;
        QVERIFY(encoder.isReady());
        constexpr int kFrames = RemoteMicConfig::kOpusFrameSamples;
        std::vector<float> frame(kFrames);
        for (int k = 0; k < kPackets; ++k) {
            for (int n = 0; n < kFrames; ++n) {
                const double t = double(k * kFrames + n) / RemoteMicConfig::kSampleRate;
                frame[static_cast<size_t>(n)] =
                    static_cast<float>(0.25 * std::sin(2.0 * std::numbers::pi * 440.0 * t));
            }
            const QByteArray packet = encoder.packet(frame.data(), static_cast<quint16>(k + 1),
                                                     static_cast<quint32>(k * kFrames));
            QVERIFY(!packet.isEmpty());
            sent << packet;
        }
    }
    for (const QByteArray& packet : sent) {
        QVERIFY(phone.sendMic(packet));
    }
    QTRY_COMPARE_WITH_TIMEOUT(received.size(), sent.size(), 5000);
    QCOMPARE(received, sent);

    // The Core's receiver, told what the line agreed, decodes every packet.
    RemoteMicFeed feed;
    RemoteMicReceiver receiver(&feed);
    QVERIFY(receiver.start(kMicSsrc, offerer.micLosslessNegotiated()));
    feed.setInUse(true);
    for (const QByteArray& packet : received) {
        receiver.submit(packet);
    }
    const RemoteMicReceiver::Stats stats = receiver.stats();
    QCOMPARE(stats.rejectedPackets, quint64(0));
    QCOMPARE(stats.decodedPackets, quint64(kPackets));
    std::vector<float> block(RemoteMicConfig::kPumpBlockFrames);
    float peak = 0.0f;
    bool heard = false;
    for (int i = 0; i < 200; ++i) {
        if (!feed.pull(block.data(), RemoteMicConfig::kPumpBlockFrames)) {
            break;
        }
        for (const float v : block) {
            peak = std::max(peak, std::abs(v));
        }
        heard = heard || std::abs(block[10] - 0.25f) < 0.01f;
    }
    if (lossless) {
        // Lossless: the level sent reaches the transmitter's feed (its rate
        // matcher between, as tst_remote_mic_receiver reads it).
        QVERIFY2(heard, qPrintable(QString::number(peak)));
    } else {
        QVERIFY2(peak > 0.15f && peak < 0.35f, qPrintable(QString::number(peak)));
    }
    receiver.stop();
    relay.stop();
    offerer.stop();
}

QTEST_GUILESS_MAIN(TestMicLinePhoneAnswer)
#include "tst_mic_line_phone_answer.moc"
