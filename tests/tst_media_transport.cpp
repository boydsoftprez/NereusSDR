// =================================================================
// tests/tst_media_transport.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R3 Task 1.
//
// 2026-09-24: labelled `realtime` (fast-test-loop rules): each case does a
// real encrypted loopback handshake inside a fixed 10 s wait, which misses at
// load 20-30; a failed case prints the load average. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
//
// 2026-09-25: iPhone app plan Task 36 (R-IOS-13): the microphone line (a
// second audio m-line, mid "mic", receive-only at the offerer) is offered
// only when asked, keeps today's offer otherwise, and carries the
// answerer's microphone to the offerer alone. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
//
// 2026-09-28: addendum G-127: ready() precedes a message that reaches the
// answerer together with its display channel's opening, and a reply from
// that message's handler is taken. J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code.
//
// 2026-09-30: TX stall lane: a microphone packet reports how long it
// waited between its receipt and its report, so a stalled owner thread is
// not taken for the link's jitter. J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code.
//
// 2026-10-01: TX mic thread: with a sink installed, the microphone line is
// delivered by its own thread, so a stalled owner thread (80 to 420 ms,
// speech-like audio, a real pump) leaves the line whole; delivered by the
// owner instead, the same stalls run the buffer dry. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
//
// 2026-10-01: TX diagnostics lane: the microphone line's own thread warns
// of a long gap between its packets ("media RTP timing (microphone)"), and
// stays quiet at the phone's 20 ms pacing. J.J. Boyd (KG4VCF), AI-assisted
// via Anthropic Claude Code.
//
// 2026-10-01: TX diagnostics lane, review round: MicLineTimingWatch replayed
// (20 ms pacing, a line idle between keys, a 300 ms gap in the next second,
// the once-a-second limit), and the loopback check no longer depends on
// real-time pacing. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
// Code.
//
// 2026-10-01: Mic 48k lane: the microphone line's offer advertises
// maxaveragebitrate=48000. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
// Claude Code.
//
// =================================================================

#include "RealtimeTestLoad.h"

#include "core/session/media/LibDataChannelMediaTransport.h"
#include "core/session/RelayLeg.h"
#include "core/session/media/PcmAudioCodec.h"
#include "core/session/media/OpusAudioCodec.h"
#include "core/session/media/RemoteIqCodec.h"
#include "core/session/media/RemoteMicReceiver.h"

#include <QElapsedTimer>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSignalSpy>
#include <QThread>
#include <QTimer>
#include <QtTest>
#include <QtEndian>

#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <numbers>
#include <functional>
#include <optional>
#include <thread>

using namespace NereusSDR;

namespace {

constexpr quint32 kTestAudioSsrc = 0x4e523301U;

class RetainedCandidateSource final : public IceConfiguration::CandidateSource {
public:
    void start(std::function<void(const QString&)> add) override { m_add = std::move(add); }
    void stop() override { m_add = {}; }
    bool ready() const { return static_cast<bool>(m_add); }
    std::function<void(const QString&)> callback() const { return m_add; }

private:
    std::function<void(const QString&)> m_add;
};
// R-R3-43: four receiver stream SSRCs, distinct from the main one.
const QList<quint32> kTestReceiverSsrcs{0x4e523311U, 0x4e523322U, 0x4e523333U, 0x4e523344U};

// The Opus parameters every Core offered before R-R3-23, verbatim: an old
// Core that a new GUI must still answer.
constexpr const char* kOldCoreOpusParameters =
    "minptime=10;maxaveragebitrate=96000;stereo=1;sprop-stereo=1;useinbandfec=1";

// The single Opus a=fmtp line of an offer, without its prefix.
QString opusFormatLine(const QString& sdp)
{
    const QString prefix = QStringLiteral("a=fmtp:111 ");
    QStringList found;
    for (const QString& line : sdp.split(QRegularExpression(QStringLiteral("\\r?\\n")))) {
        if (line.startsWith(prefix)) {
            found << line.mid(prefix.size());
        }
    }
    return found.size() == 1 ? found.constFirst() : QString();
}
// The audio section of a description: its m= line and every a=rtpmap and
// a=fmtp line, in order. Fingerprints, ICE credentials and SSRC lines differ
// per run; these do not, so they are the golden part of an offer.
QStringList audioFormatLines(const QString& sdp)
{
    QStringList lines;
    bool inAudio = false;
    for (const QString& line : sdp.split(QRegularExpression(QStringLiteral("\\r?\\n")))) {
        if (line.startsWith(QLatin1String("m="))) {
            inAudio = line.startsWith(QLatin1String("m=audio"));
            if (inAudio) { lines << line; }
            continue;
        }
        if (inAudio && (line.startsWith(QLatin1String("a=rtpmap:"))
                        || line.startsWith(QLatin1String("a=fmtp:")))) {
            lines << line;
        }
    }
    return lines;
}

// The a=ssrc lines of a description's audio section, in order.
QStringList audioSsrcLines(const QString& sdp)
{
    QStringList lines;
    bool inAudio = false;
    for (const QString& line : sdp.split(QRegularExpression(QStringLiteral("\\r?\\n")))) {
        if (line.startsWith(QLatin1String("m="))) {
            inAudio = line.startsWith(QLatin1String("m=audio"));
            continue;
        }
        if (inAudio && line.startsWith(QLatin1String("a=ssrc:"))) {
            lines << line;
        }
    }
    return lines;
}

// A description's lines with its per-run values (session id, DTLS
// fingerprint, ICE credentials) replaced by fixed placeholders, so two runs
// of the same code compare byte for byte everywhere else.
QStringList stableDescriptionLines(const QString& sdp)
{
    static const QRegularExpression perRun(QStringLiteral(
        "^(o=rtc |a=fingerprint:sha-256 |a=ice-ufrag:|a=ice-pwd:)\\S+"));
    QStringList lines;
    for (QString line : sdp.split(QStringLiteral("\r\n"))) {
        line.replace(perRun, QStringLiteral("\\1<per-run>"));
        lines << line;
    }
    return lines;
}

// A description without the a=ssrc lines of the given SSRCs: what a peer
// that never declared them would have offered.
QString withoutSsrcLines(const QString& sdp, const QList<quint32>& ssrcs)
{
    QStringList kept;
    const QStringList lines = sdp.split(QStringLiteral("\r\n"));
    for (const QString& line : lines) {
        bool drop = false;
        for (const quint32 ssrc : ssrcs) {
            if (line.startsWith(QStringLiteral("a=ssrc:%1 ").arg(ssrc))) {
                drop = true;
            }
        }
        if (!drop) {
            kept << line;
        }
    }
    return kept.join(QStringLiteral("\r\n"));
}

// Waits, without running the Qt event loop (so the owner's drain timer
// cannot fire), until the transport's library threads have received
// `bytes` RTP bytes in all.
bool waitForReceivedRtpBytes(const LibDataChannelMediaTransport& transport, quint64 bytes)
{
    QElapsedTimer waited;
    waited.start();
    while (waited.elapsed() < 5000) {
        const std::optional<MediaTransportTelemetry> traffic = transport.telemetry();
        if (traffic && traffic->receivedRtpBytes >= bytes) {
            return traffic->receivedRtpBytes == bytes;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

// Task 36: the lines of the media section whose a=mid is `mid`, from its
// m= line to the next m= line.
QStringList mediaSectionLines(const QString& sdp, const QString& mid)
{
    QStringList section;
    QStringList current;
    const QStringList lines = sdp.split(QRegularExpression(QStringLiteral("\\r?\\n")));
    const auto flush = [&section, &current, &mid] {
        if (current.contains(QStringLiteral("a=mid:%1").arg(mid))) {
            section = current;
        }
        current.clear();
    };
    for (const QString& line : lines) {
        if (line.startsWith(QLatin1String("m="))) {
            flush();
        }
        if (!line.isEmpty()) {
            current << line;
        }
    }
    flush();
    return section;
}

// Task 36: a description without the microphone line's section, and with the
// groups it joins taken out again: what a peer that never asked for the
// line would have written.
QString withoutMicSection(const QString& sdp)
{
    const QStringList mic = mediaSectionLines(sdp, QStringLiteral("mic"));
    // The section is contiguous: take it out by position.
    QStringList all = sdp.split(QStringLiteral("\r\n"));
    if (!mic.isEmpty()) {
        for (int start = 0; start + mic.size() <= all.size(); ++start) {
            if (all.mid(start, mic.size()) == mic) {
                all.remove(start, mic.size());
                break;
            }
        }
    }
    for (QString& line : all) {
        // The bundle and lip-sync groups name the line too.
        if (line.startsWith(QLatin1String("a=group:"))) {
            line.replace(QStringLiteral(" mic"), QString());
        }
    }
    return all.join(QStringLiteral("\r\n"));
}

// Today's whole Core offer, per-run values aside (golden), for the main SSRC.
QStringList todaysOfferGolden(quint32 mainSsrc)
{
    return {
        QStringLiteral("v=0"),
        QStringLiteral("o=rtc <per-run> 0 IN IP4 127.0.0.1"),
        QStringLiteral("s=-"),
        QStringLiteral("t=0 0"),
        QStringLiteral("a=group:BUNDLE audio 0"),
        QStringLiteral("a=group:LS audio"),
        QStringLiteral("a=msid-semantic:WMS *"),
        QStringLiteral("a=ice-options:ice2,trickle"),
        QStringLiteral("a=fingerprint:sha-256 <per-run>"),
        QStringLiteral("m=audio 9 UDP/TLS/RTP/SAVPF 111"),
        QStringLiteral("c=IN IP4 0.0.0.0"),
        QStringLiteral("a=mid:audio"),
        QStringLiteral("a=sendonly"),
        QStringLiteral("a=ssrc:%1 cname:nereus-mixed-stereo").arg(mainSsrc),
        QStringLiteral("a=rtcp-mux"),
        QStringLiteral("a=rtpmap:111 opus/48000/2"),
        QStringLiteral("a=fmtp:111 minptime=10;maxaveragebitrate=48000;stereo=1;sprop-stereo=1"),
        QStringLiteral("a=setup:actpass"),
        QStringLiteral("a=ice-ufrag:<per-run>"),
        QStringLiteral("a=ice-pwd:<per-run>"),
        QStringLiteral("m=application 9 UDP/DTLS/SCTP webrtc-datachannel"),
        QStringLiteral("c=IN IP4 0.0.0.0"),
        QStringLiteral("a=mid:0"),
        QStringLiteral("a=sendrecv"),
        QStringLiteral("a=sctp-port:5000"),
        QStringLiteral("a=max-message-size:65536"),
        QStringLiteral("a=setup:actpass"),
        QStringLiteral("a=ice-ufrag:<per-run>"),
        QStringLiteral("a=ice-pwd:<per-run>"),
        QString(),
    };
}

// Task 36: the microphone line's SSRC in these tests.
constexpr quint32 kTestMicSsrc = 0x4e523366U;

// Set only in the child process that checks when SCTP settings are applied.
constexpr const char* kFirstPeerChildVariable = "NEREUS_TST_MEDIA_TRANSPORT_FIRST_PEER";

} // namespace

class TestMediaTransport : public QObject {
    Q_OBJECT

private slots:
    void cleanup() { NereusSDR::RealtimeTestLoad::printLoadAverageIfFailed(); }

    // Observes a fresh child process, so its place in the list does not
    // matter and it can run any number of times.
    void sctpSettingsAppliedOnceBeforeFirstPeer();
    void encryptedPeersCarryDisplayAndRtp();
    void stalledReceiverRefusesDisplayInsteadOfQueueing();
    void heldDisplayMessageIsTakenAndSignalsWritable();
    void queuedDisplayOverflowDropsOldestAndCountsIt();
    void heldBurstAfterAStallIsDeliveredWhole();
    void telemetryCountsValidatedTrafficAndResets();
    void boundedInputsRefuseBeforeTransport();
    void stopCancelsOldCallbacksAndRecreates();
    void signalRestartDropsRemainingOldGenerationMedia();
    void deletionFromReceivedSignalIsSafe();
    void readyPrecedesMessagesThatArriveWithIt();
    void offerDescribesTheRealEncoder_data();
    void offerDescribesTheRealEncoder();
    void answererAcceptsNewAndOldCoreOffers_data();
    void answererAcceptsNewAndOldCoreOffers();
    void preconditionRefusalsReportNoError();
    void losslessRtpMapIsOfferedOnlyWhenAsked();
    void answerKeepsTheLosslessRtpMap_data();
    void answerKeepsTheLosslessRtpMap();
    void ordinaryMediaHostOnLoopbackIsNotAnOwnedShim();
    void retiredMediaSourceCannotMarkARestartedPeer();
    void losslessAudioCrossesRealEncryptedLoopback();
    void undeclaredStreamIsDeliveredByTheOneAudioLine();
    void receiverStreamsAreDeclaredOnlyWhenAsked();
    void declaredReceiverStreamsCrossBesideTheMainOne();
    void receiveQueueHoldsEveryDeclaredStream_data();
    void receiveQueueHoldsEveryDeclaredStream();
    void receiverStreamPreconditionsRefuseSilently();
    void headphonesMixIsDeclaredLastAndCrossesBesideTheOthers();
    void micLineIsOfferedOnlyWhenAsked();
    void micLineCarriesTheAnswerersMicrophoneAlone();
    void micPacketsReportTheirWaitForTheOwnerThread();
    void aStalledOwnerLeavesTheMicrophoneLineWhole();
    void micLineWarnsOfALongGapFromItsOwnThread();
    void micLineTimingTakesAnIdleGapAsTheLineStarting();
    void micLineCarriesLosslessWhenOffered();
    void micSsrcPreconditionsRefuseSilently();
    void dedicatedIqChannelPreservesOrder();
    void dedicatedTxAndIqTelemetryCountsPayloadsAndResets();
    void iqTelemetryCountsValidArrivalBeforeReceiveQueueFails();

private:
    static void wireExchange(LibDataChannelMediaTransport& offerer,
                             LibDataChannelMediaTransport& answerer,
                             QString* offerOut, QString* answerOut);
    static void wire(LibDataChannelMediaTransport& offerer,
                     LibDataChannelMediaTransport& answerer);
    static void startPair(LibDataChannelMediaTransport& offerer,
                          LibDataChannelMediaTransport& answerer);
    static QByteArray rtpPacket(quint16 sequence,
                                qsizetype size = 15,
                                quint32 ssrc = kTestAudioSsrc);
};

void TestMediaTransport::wire(LibDataChannelMediaTransport& offerer,
                              LibDataChannelMediaTransport& answerer)
{
    connect(&offerer, &IMediaTransport::localDescription, &answerer,
            [&answerer](const QString& sdp, const QString& type) {
                QVERIFY2(answerer.acceptDescription(sdp, type),
                         "answerer rejected offer");
            });
    connect(&answerer, &IMediaTransport::localDescription, &offerer,
            [&offerer](const QString& sdp, const QString& type) {
                QVERIFY2(offerer.acceptDescription(sdp, type),
                         "offerer rejected answer");
            });
    connect(&offerer, &IMediaTransport::localCandidate, &answerer,
            [&answerer](const QString& candidate, const QString& mid) {
                QVERIFY2(answerer.acceptCandidate(candidate, mid),
                         "answerer rejected host candidate");
            });
    connect(&answerer, &IMediaTransport::localCandidate, &offerer,
            [&offerer](const QString& candidate, const QString& mid) {
                QVERIFY2(offerer.acceptCandidate(candidate, mid),
                         "offerer rejected host candidate");
            });
}

void TestMediaTransport::startPair(LibDataChannelMediaTransport& offerer,
                                   LibDataChannelMediaTransport& answerer)
{
    wire(offerer, answerer);
    QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
    QSignalSpy answerReady(&answerer, &IMediaTransport::ready);
    QVERIFY(answerer.start({IMediaTransport::Role::Answerer,
                            kTestAudioSsrc}));
    QVERIFY(offerer.start({IMediaTransport::Role::Offerer,
                           kTestAudioSsrc}));
    QTRY_COMPARE_WITH_TIMEOUT(offerReady.count(), 1, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(answerReady.count(), 1, 10000);
    QVERIFY(offerer.isReady());
    QVERIFY(answerer.isReady());
}

void TestMediaTransport::dedicatedIqChannelPreservesOrder()
{
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    wire(offerer, answerer);
    IMediaTransport::StartOptions offered{IMediaTransport::Role::Offerer, kTestAudioSsrc};
    IMediaTransport::StartOptions answered{IMediaTransport::Role::Answerer, kTestAudioSsrc};
    offered.iqChannel = true;
    answered.iqChannel = true;
    QSignalSpy offeredReady(&offerer, &IMediaTransport::ready);
    QSignalSpy answeredReady(&answerer, &IMediaTransport::ready);
    QSignalSpy received(&answerer, &IMediaTransport::iqReceived);
    QSignalSpy iqErrors(&answerer, &IMediaTransport::iqErrorOccurred);
    QVERIFY(answerer.start(answered));
    QVERIFY(offerer.start(offered));
    QTRY_COMPARE_WITH_TIMEOUT(offeredReady.size(), 1, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(answeredReady.size(), 1, 10000);
    for (quint32 sequence = 0; sequence < 128; ++sequence) {
        const auto frame = RemoteIqCodec::encode(1, 9, sequence,
                                                  {float(sequence), -float(sequence)});
        QVERIFY(frame);
        QElapsedTimer deadline;
        deadline.start();
        while (offerer.iqBusy() && deadline.elapsed() < 1000) { QTest::qWait(1); }
        QVERIFY(!offerer.iqBusy());
        const auto result = offerer.submitIq(*frame);
        QVERIFY(result == IMediaTransport::DisplaySendResult::Sent
                || result == IMediaTransport::DisplaySendResult::Queued);
        // The receiver deliberately holds at most eight callbacks. Drain
        // each accepted message before offering the next; a separate queue
        // overflow case proves its failure signal.
        QTRY_COMPARE_WITH_TIMEOUT(received.size(), int(sequence) + 1, 5000);
    }
    QCOMPARE(iqErrors.size(), 0);
    for (int index = 0; index < received.size(); ++index) {
        const auto decoded = RemoteIqCodec::decode(received.at(index).at(0).toByteArray());
        QVERIFY(decoded);
        QCOMPARE(decoded->sequence, quint32(index));
        QCOMPARE(decoded->generation, 9u);
    }
    offerer.stop();
    answerer.stop();
}

void TestMediaTransport::dedicatedTxAndIqTelemetryCountsPayloadsAndResets()
{
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    wire(offerer, answerer);
    IMediaTransport::StartOptions offered{IMediaTransport::Role::Offerer, kTestAudioSsrc};
    IMediaTransport::StartOptions answered{IMediaTransport::Role::Answerer, kTestAudioSsrc};
    offered.txChannel = answered.txChannel = true;
    offered.iqChannel = answered.iqChannel = true;
    QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
    QSignalSpy answerReady(&answerer, &IMediaTransport::ready);
    QSignalSpy txReceived(&answerer, &IMediaTransport::txReceived);
    QSignalSpy iqReceived(&answerer, &IMediaTransport::iqReceived);
    QVERIFY(answerer.start(answered));
    QVERIFY(offerer.start(offered));
    QTRY_COMPARE_WITH_TIMEOUT(offerReady.size(), 1, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(answerReady.size(), 1, 10000);

    const QByteArray tx = QByteArrayLiteral("watch-keepalive");
    const auto iq = RemoteIqCodec::encode(1, 9, 1, {0.25f, -0.25f});
    QVERIFY(iq);
    QVERIFY(offerer.sendTx(tx));
    const auto iqResult = offerer.submitIq(*iq);
    QVERIFY(iqResult == IMediaTransport::DisplaySendResult::Sent
            || iqResult == IMediaTransport::DisplaySendResult::Queued);
    QTRY_COMPARE_WITH_TIMEOUT(txReceived.size(), 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(iqReceived.size(), 1, 5000);
    QCOMPARE(txReceived.constFirst().constFirst().toByteArray(), tx);
    QCOMPARE(iqReceived.constFirst().constFirst().toByteArray(), *iq);
    const auto submitted = offerer.telemetry();
    const auto received = answerer.telemetry();
    QVERIFY(submitted && received);
    QCOMPARE(submitted->submittedTxPayloadBytes, quint64(tx.size()));
    QCOMPARE(submitted->submittedIqPayloadBytes, quint64(iq->size()));
    QCOMPARE(received->receivedTxPayloadBytes, quint64(tx.size()));
    QCOMPARE(received->receivedIqPayloadBytes, quint64(iq->size()));
    QCOMPARE(submitted->receivedTxPayloadBytes, quint64(0));
    QCOMPARE(received->submittedIqPayloadBytes, quint64(0));

    QVERIFY(!offerer.sendTx({}));
    QVERIFY(!offerer.sendTx(QByteArray(IMediaTransport::kMaxTxMessageBytes + 1, 'x')));
    QCOMPARE(offerer.submitIq({}), IMediaTransport::DisplaySendResult::Refused);
    QCOMPARE(offerer.submitIq(QByteArray(IMediaTransport::kMaxIqMessageBytes + 1, 'x')),
             IMediaTransport::DisplaySendResult::Refused);
    QCOMPARE(offerer.telemetry()->submittedTxPayloadBytes, submitted->submittedTxPayloadBytes);
    QCOMPARE(offerer.telemetry()->submittedIqPayloadBytes, submitted->submittedIqPayloadBytes);

    offerer.stop();
    answerer.stop();
    QVERIFY(!offerer.telemetry() && !answerer.telemetry());
    QSignalSpy offerReadyAgain(&offerer, &IMediaTransport::ready);
    QSignalSpy answerReadyAgain(&answerer, &IMediaTransport::ready);
    QVERIFY(answerer.start(answered));
    QVERIFY(offerer.start(offered));
    QTRY_COMPARE_WITH_TIMEOUT(offerReadyAgain.size(), 1, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(answerReadyAgain.size(), 1, 10000);
    QCOMPARE(offerer.telemetry()->submittedTxPayloadBytes, quint64(0));
    QCOMPARE(offerer.telemetry()->submittedIqPayloadBytes, quint64(0));
    QCOMPARE(answerer.telemetry()->receivedTxPayloadBytes, quint64(0));
    QCOMPARE(answerer.telemetry()->receivedIqPayloadBytes, quint64(0));
    offerer.stop();
    answerer.stop();
}

void TestMediaTransport::iqTelemetryCountsValidArrivalBeforeReceiveQueueFails()
{
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    wire(offerer, answerer);
    IMediaTransport::StartOptions offered{IMediaTransport::Role::Offerer, kTestAudioSsrc};
    IMediaTransport::StartOptions answered{IMediaTransport::Role::Answerer, kTestAudioSsrc};
    offered.iqChannel = answered.iqChannel = true;
    QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
    QSignalSpy answerReady(&answerer, &IMediaTransport::ready);
    QSignalSpy iqReceived(&answerer, &IMediaTransport::iqReceived);
    QSignalSpy iqErrors(&answerer, &IMediaTransport::iqErrorOccurred);
    QVERIFY(answerer.start(answered));
    QVERIFY(offerer.start(offered));
    QTRY_COMPARE_WITH_TIMEOUT(offerReady.size(), 1, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(answerReady.size(), 1, 10000);

    // With no Qt drain, the ninth valid frame exceeds the existing eight-
    // message receive queue. The callback counter still measures its arrival.
    quint64 bytes = 0;
    for (quint32 sequence = 0; sequence < 9; ++sequence) {
        const auto frame = RemoteIqCodec::encode(1, 9, sequence, {0.25f, -0.25f});
        QVERIFY(frame);
        QElapsedTimer waited;
        waited.start();
        while (offerer.iqBusy() && waited.elapsed() < 5000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        QVERIFY(!offerer.iqBusy());
        const auto result = offerer.submitIq(*frame);
        QVERIFY(result == IMediaTransport::DisplaySendResult::Sent
                || result == IMediaTransport::DisplaySendResult::Queued);
        bytes += quint64(frame->size());
    }
    QElapsedTimer receivedWait;
    receivedWait.start();
    while (answerer.telemetry()->receivedIqPayloadBytes < bytes
           && receivedWait.elapsed() < 5000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    QCOMPARE(offerer.telemetry()->submittedIqPayloadBytes, bytes);
    QCOMPARE(answerer.telemetry()->receivedIqPayloadBytes, bytes);
    QCOMPARE(iqReceived.size(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(iqErrors.size(), 1, 5000);
    QCOMPARE(iqReceived.size(), 0);
    offerer.stop();
    answerer.stop();
}

QByteArray TestMediaTransport::rtpPacket(quint16 sequence, qsizetype size,
                                         quint32 ssrc)
{
    QByteArray packet(size, char(0xa5));
    packet[0] = char(0x80);
    packet[1] = char(111);
    packet[2] = char(sequence >> 8);
    packet[3] = char(sequence & 0xff);
    packet[8] = char(ssrc >> 24);
    packet[9] = char(ssrc >> 16);
    packet[10] = char(ssrc >> 8);
    packet[11] = char(ssrc);
    return packet;
}

void TestMediaTransport::sctpSettingsAppliedOnceBeforeFirstPeer()
{
    // The settings are process-wide and applied once, so only a process in
    // which no peer has existed yet can show when they were applied. Run the
    // check in a fresh copy of this test binary running only this slot.
    if (!qEnvironmentVariableIsSet(kFirstPeerChildVariable)) {
        QProcess child;
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QString::fromLatin1(kFirstPeerChildVariable), QStringLiteral("1"));
        child.setProcessEnvironment(environment);
        child.setProcessChannelMode(QProcess::MergedChannels);
        child.start(QCoreApplication::applicationFilePath(),
                    {QStringLiteral("sctpSettingsAppliedOnceBeforeFirstPeer")});
        QVERIFY2(child.waitForStarted(10'000), qPrintable(child.errorString()));
        QVERIFY2(child.waitForFinished(60'000), "first-peer child did not finish");
        const QByteArray output = child.readAll();
        QVERIFY2(child.exitStatus() == QProcess::NormalExit && child.exitCode() == 0,
                 output.constData());
        QVERIFY2(output.contains("PASS   : TestMediaTransport::"
                                 "sctpSettingsAppliedOnceBeforeFirstPeer()"),
                 output.constData());
        return;
    }

    const MediaSctpSettingsRecord before = mediaSctpSettingsRecord();
    QCOMPARE(before.applications, 0);
    QCOMPARE(before.peersCreated, quint64(0));

    {
        LibDataChannelMediaTransport offerer;
        LibDataChannelMediaTransport answerer;
        startPair(offerer, answerer);
        const MediaSctpSettingsRecord first = mediaSctpSettingsRecord();
        QCOMPARE(first.applications, 1);
        QCOMPARE(first.peersCreatedBeforeApplication, quint64(0));
        QCOMPARE(first.peersCreated, quint64(2));
        offerer.stop();
        answerer.stop();
    }

    // Later calls and later peers never apply the settings again.
    QVERIFY(!applyMediaSctpSettingsOnce());
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    startPair(offerer, answerer);
    const MediaSctpSettingsRecord second = mediaSctpSettingsRecord();
    QCOMPARE(second.applications, 1);
    QCOMPARE(second.peersCreatedBeforeApplication, quint64(0));
    QCOMPARE(second.peersCreated, quint64(4));
    offerer.stop();
    answerer.stop();
}

void TestMediaTransport::encryptedPeersCarryDisplayAndRtp()
{
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    QSignalSpy offerDescriptions(&offerer, &IMediaTransport::localDescription);
    QSignalSpy answerDescriptions(&answerer, &IMediaTransport::localDescription);
    QSignalSpy displayReceived(&answerer, &IMediaTransport::displayReceived);
    QSignalSpy rtpReceived(&answerer, &IMediaTransport::rtpReceived);
    startPair(offerer, answerer);

    QCOMPARE(offerDescriptions.count(), 1);
    QCOMPARE(answerDescriptions.count(), 1);
    QVERIFY(offerDescriptions.at(0).at(0).toString().contains("a=fingerprint:"));
    QVERIFY(answerDescriptions.at(0).at(0).toString().contains("a=fingerprint:"));
    QVERIFY(offerDescriptions.at(0).at(0).toString().contains("m=application"));
    QVERIFY(offerDescriptions.at(0).at(0).toString().contains("m=audio"));
    QVERIFY(offerDescriptions.at(0).at(0).toString().contains("stereo=1"));
    QVERIFY(offerDescriptions.at(0).at(0).toString().contains(
        QStringLiteral("a=ssrc:%1").arg(kTestAudioSsrc)));
    QVERIFY(!offerDescriptions.at(0).at(0).toString().contains(
        QStringLiteral("a=candidate:"), Qt::CaseInsensitive));
    QVERIFY(!answerDescriptions.at(0).at(0).toString().contains(
        QStringLiteral("a=candidate:"), Qt::CaseInsensitive));

    const QList<QByteArray> displayMessages{
        QByteArrayLiteral("pan-0-display"),
        QByteArrayLiteral("pan-1-display"),
        QByteArrayLiteral("pan-2-display"),
        QByteArrayLiteral("pan-3-display"),
    };
    const QByteArray rtp = rtpPacket(7);
    for (const QByteArray& message : displayMessages) {
        QVERIFY(offerer.sendDisplay(message));
    }
    QVERIFY(offerer.sendRtp(rtp));
    QTRY_COMPARE_WITH_TIMEOUT(displayReceived.count(), displayMessages.size(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(rtpReceived.count(), 1, 5000);
    for (qsizetype i = 0; i < displayMessages.size(); ++i) {
        QCOMPARE(displayReceived.at(i).at(0).toByteArray(), displayMessages.at(i));
    }
    QCOMPARE(rtpReceived.at(0).at(0).toByteArray(), rtp);

    const QByteArray boundaryDisplay(
        IMediaTransport::kMaxDisplayMessageBytes, char(0x5a));
    const QByteArray boundaryRtp = rtpPacket(
        8, IMediaTransport::kMaxRawRtpBytes);
    QVERIFY(offerer.sendDisplay(boundaryDisplay));
    QVERIFY(offerer.sendRtp(boundaryRtp));
    QTRY_COMPARE_WITH_TIMEOUT(displayReceived.count(),
                              displayMessages.size() + 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(rtpReceived.count(), 2, 5000);
    QCOMPARE(displayReceived.last().at(0).toByteArray(), boundaryDisplay);
    QCOMPARE(rtpReceived.last().at(0).toByteArray(), boundaryRtp);
    QVERIFY(!offerer.sendRtp(rtpPacket(9, 15, kTestAudioSsrc + 1)));

    offerer.stop();
    answerer.stop();
}

void TestMediaTransport::stalledReceiverRefusesDisplayInsteadOfQueueing()
{
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    QSignalSpy displayReceived(&answerer, &IMediaTransport::displayReceived);
    startPair(offerer, answerer);

    // The largest spectrum frame: 4096/4096 points plus a 768-point 3D row.
    constexpr qsizetype kFrameBytes = 9361;
    const QByteArray frame(kFrameBytes, char(0x3c));
    answerer.setDisplayReceiveStalledForTest(true);

    // Offer frames until a full second passes in which the sender took
    // nothing: no frame went to SCTP (Sent) and none was held by the
    // library (Queued). Either one restarts the second.
    int sent = 0;
    int queued = 0;
    int refused = 0;
    QElapsedTimer sinceTaken;
    sinceTaken.start();
    QElapsedTimer overall;
    overall.start();
    while (sinceTaken.elapsed() < 1000 && overall.elapsed() < 60'000) {
        const IMediaTransport::DisplaySendResult result = offerer.submitDisplay(frame);
        if (result == IMediaTransport::DisplaySendResult::Sent) {
            ++sent;
            sinceTaken.restart();
        } else if (result == IMediaTransport::DisplaySendResult::Queued) {
            ++queued;
            sinceTaken.restart();
        } else {
            ++refused;
        }
        QTest::qWait(1);
    }
    QVERIFY2(overall.elapsed() < 60'000, "the sender never stopped taking frames");
    QVERIFY(sent > 0);
    QVERIFY(refused > 0);

    // The SCTP buffers (64 KiB send, 128 KiB receive) and the one message
    // the library holds do not add up to a whole-path limit, so the bound
    // is the measurement: the whole path took exactly 26 frames (243,386
    // bytes) on every run, 2026-09-23. The margin is one frame, for when
    // the receiver's window update lands. libdatachannel's 1 MiB defaults
    // let about 1.4 MB pile up here.
    const quint64 submitted = offerer.telemetry()->submittedDisplayPayloadBytes;
    constexpr quint64 kMeasuredFrames = 26;
    constexpr quint64 kMarginFrames = 1;
    const quint64 bound = (kMeasuredFrames + kMarginFrames) * quint64(kFrameBytes);
    QCOMPARE(submitted, quint64(sent + queued) * quint64(kFrameBytes));
    QVERIFY2(submitted <= bound,
             qPrintable(QStringLiteral("sender took %1 bytes in %2 frames (%3 held by "
                                       "the library), bound %4")
                            .arg(submitted).arg(sent + queued).arg(queued).arg(bound)));

    // The writable event marks the library's held frame leaving its queue.
    // Repeated send attempts in QTRY can queue another frame before the
    // assertion observes recovery. Wait for the event, then submit once.
    QSignalSpy writable(&offerer, &IMediaTransport::displayWritable);
    answerer.setDisplayReceiveStalledForTest(false);
    QTRY_VERIFY_WITH_TIMEOUT(!writable.isEmpty(), 10'000);
    QVERIFY(!offerer.displayBusy());
    QVERIFY(offerer.sendDisplay(frame));
    QTRY_VERIFY_WITH_TIMEOUT(!displayReceived.isEmpty(), 10'000);
    for (const QList<QVariant>& arguments : displayReceived) {
        QCOMPARE(arguments.at(0).toByteArray(), frame);
    }

    offerer.stop();
    answerer.stop();
}

// R-R3-03/R-R3-05: usrsctp takes a message only while it fits beside the
// data not yet acknowledged, and its send buffer is the 64 KiB maximum
// message. A second large message sent before the first is acknowledged is
// taken by libdatachannel and held (Queued), not lost; a third is Busy; the
// display channel reports writable once the held message has gone, and the
// receiver gets both messages.
void TestMediaTransport::heldDisplayMessageIsTakenAndSignalsWritable()
{
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    QSignalSpy displayReceived(&answerer, &IMediaTransport::displayReceived);
    QSignalSpy writable(&offerer, &IMediaTransport::displayWritable);
    startPair(offerer, answerer);

    constexpr qsizetype kChunkBytes = 49'216; // one PureSignal display chunk
    const QByteArray first(kChunkBytes, char(0x11));
    const QByteArray second(kChunkBytes, char(0x22));
    const QByteArray third(kChunkBytes, char(0x33));
    QCOMPARE(offerer.submitDisplay(first), IMediaTransport::DisplaySendResult::Sent);
    QCOMPARE(offerer.submitDisplay(second), IMediaTransport::DisplaySendResult::Queued);
    QVERIFY(offerer.displayBusy());
    QCOMPARE(offerer.submitDisplay(third), IMediaTransport::DisplaySendResult::Busy);

    QTRY_VERIFY_WITH_TIMEOUT(!writable.isEmpty(), 10'000);
    QVERIFY(!offerer.displayBusy());
    QTRY_COMPARE_WITH_TIMEOUT(displayReceived.count(), 2, 10'000);
    QCOMPARE(displayReceived.at(0).at(0).toByteArray(), first);
    QCOMPARE(displayReceived.at(1).at(0).toByteArray(), second);
    // Busy was not taken: nothing more arrives.
    QTest::qWait(200);
    QCOMPARE(displayReceived.count(), 2);
    QCOMPARE(offerer.telemetry()->submittedDisplayPayloadBytes, quint64(2 * kChunkBytes));

    offerer.stop();
    answerer.stop();
}

void TestMediaTransport::queuedDisplayOverflowDropsOldestAndCountsIt()
{
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    QSignalSpy displayReceived(&answerer, &IMediaTransport::displayReceived);
    startPair(offerer, answerer);

    QList<QByteArray> sent;
    quint64 sentBytes = 0;
    for (int i = 0; i < 33; ++i) {
        sent.append(QStringLiteral("queued-display-%1").arg(i).toUtf8());
        sentBytes += quint64(sent.last().size());
        QVERIFY(offerer.sendDisplay(sent.last()));
    }
    // Keep this thread out of its event loop, so no drain runs, until the
    // library has delivered all 33 messages into the receive queue.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (answerer.telemetry()->receivedDisplayPayloadBytes < sentBytes
           && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    QCOMPARE(answerer.telemetry()->receivedDisplayPayloadBytes, sentBytes);
    QCOMPARE(answerer.telemetry()->displayMessagesDropped, quint64(1));

    // One drain hands over the newest 32; the 32-message rule dropped
    // exactly one, and received bytes still count all 33 arrivals.
    QTRY_COMPARE_WITH_TIMEOUT(displayReceived.count(), 32, 5000);
    QTest::qWait(50);
    QCOMPARE(displayReceived.count(), 32);
    QSet<QByteArray> delivered;
    for (const QList<QVariant>& arguments : displayReceived) {
        delivered.insert(arguments.at(0).toByteArray());
    }
    QCOMPARE(delivered.size(), 32);
    for (const QByteArray& message : delivered) {
        QVERIFY(sent.contains(message));
    }
    QCOMPARE(answerer.telemetry()->displayMessagesDropped, quint64(1));
    QCOMPARE(answerer.telemetry()->receivedDisplayPayloadBytes, sentBytes);
    QCOMPARE(offerer.telemetry()->displayMessagesDropped, quint64(0));

    offerer.stop();
    answerer.stop();
}

// R-R3-21: a link stall holds display messages in SCTP, then releases them
// back to back into one drain. A 460 ms stall at 30 frames a second is 14
// messages; every one reaches the window, none is dropped.
void TestMediaTransport::heldBurstAfterAStallIsDeliveredWhole()
{
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    QSignalSpy displayReceived(&answerer, &IMediaTransport::displayReceived);
    startPair(offerer, answerer);

    // The largest delta the operator's Core logged: 2150 bytes, 3 fragments.
    constexpr qsizetype kDeltaBytes = 2150;
    constexpr int kHeld = 14;
    answerer.setDisplayReceiveStalledForTest(true);
    QList<QByteArray> sent;
    quint64 sentBytes = 0;
    for (int i = 0; i < kHeld; ++i) {
        QByteArray message(kDeltaBytes, char('a' + i));
        sent.append(message);
        sentBytes += quint64(message.size());
        QVERIFY(offerer.sendDisplay(message));
    }
    QTest::qWait(100);
    // Release, then keep this thread out of its event loop until the whole
    // burst sits in the receive queue, so it arrives in one drain.
    answerer.setDisplayReceiveStalledForTest(false);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (answerer.telemetry()->receivedDisplayPayloadBytes < sentBytes
           && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    QCOMPARE(answerer.telemetry()->receivedDisplayPayloadBytes, sentBytes);
    QTRY_COMPARE_WITH_TIMEOUT(displayReceived.count(), kHeld, 5000);
    QTest::qWait(50);
    QCOMPARE(displayReceived.count(), kHeld);
    QCOMPARE(answerer.telemetry()->displayMessagesDropped, quint64(0));
    for (int i = 0; i < kHeld; ++i) {
        QCOMPARE(displayReceived.at(i).at(0).toByteArray(), sent.at(i));
    }

    offerer.stop();
    answerer.stop();
}

void TestMediaTransport::telemetryCountsValidatedTrafficAndResets()
{
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    QSignalSpy displayReceived(&answerer, &IMediaTransport::displayReceived);
    QSignalSpy rtpReceived(&answerer, &IMediaTransport::rtpReceived);
    startPair(offerer, answerer);

    QVERIFY(offerer.telemetry().has_value());
    QVERIFY(answerer.telemetry().has_value());
    const auto countersAreZero = [](const MediaTransportTelemetry& telemetry) {
        return telemetry.receivedDisplayPayloadBytes == 0
            && telemetry.submittedDisplayPayloadBytes == 0
            && telemetry.displayMessagesDropped == 0
            && telemetry.receivedRtpBytes == 0
            && telemetry.submittedRtpBytes == 0;
    };
    QVERIFY(countersAreZero(*offerer.telemetry()));
    QVERIFY(countersAreZero(*answerer.telemetry()));

    const QByteArray display = QByteArrayLiteral("observed-display-payload");
    const QByteArray rtp = rtpPacket(40, 37);
    QVERIFY(offerer.sendDisplay(display));
    QTRY_COMPARE_WITH_TIMEOUT(displayReceived.size(), 1, 5000);
    QVERIFY(offerer.sendRtp(rtp));
    QTRY_COMPARE_WITH_TIMEOUT(rtpReceived.size(), 1, 5000);

    const auto trafficArrived = [&answerer, &display, &rtp] {
        const auto snapshot = answerer.telemetry();
        return snapshot
            && snapshot->receivedDisplayPayloadBytes
                == static_cast<quint64>(display.size())
            && snapshot->receivedRtpBytes == static_cast<quint64>(rtp.size());
    };
    QTRY_VERIFY_WITH_TIMEOUT(trafficArrived(), 5000);
    const MediaTransportTelemetry submitted = *offerer.telemetry();
    QCOMPARE(submitted.submittedDisplayPayloadBytes,
             static_cast<quint64>(display.size()));
    QCOMPARE(submitted.submittedRtpBytes, static_cast<quint64>(rtp.size()));
    QCOMPARE(submitted.receivedDisplayPayloadBytes, quint64(0));
    QCOMPARE(submitted.receivedRtpBytes, quint64(0));

    // These fail the adapter's own preflight and never become submissions.
    QVERIFY(!offerer.sendDisplay(QByteArray{}));
    QVERIFY(!offerer.sendDisplay(QByteArray(
        IMediaTransport::kMaxDisplayMessageBytes + 1, 'x')));
    QVERIFY(!offerer.sendRtp(QByteArray(
        IMediaTransport::kMinRawRtpBytes - 1, 'x')));
    QVERIFY(!offerer.sendRtp(QByteArray(
        IMediaTransport::kMaxRawRtpBytes + 1, 'x')));
    QVERIFY(!offerer.sendRtp(rtpPacket(41, 15, kTestAudioSsrc + 1)));
    const MediaTransportTelemetry afterInvalid = *offerer.telemetry();
    QCOMPARE(afterInvalid.receivedDisplayPayloadBytes,
             submitted.receivedDisplayPayloadBytes);
    QCOMPARE(afterInvalid.submittedDisplayPayloadBytes,
             submitted.submittedDisplayPayloadBytes);
    QCOMPARE(afterInvalid.receivedRtpBytes, submitted.receivedRtpBytes);
    QCOMPARE(afterInvalid.submittedRtpBytes, submitted.submittedRtpBytes);

    offerer.stop();
    answerer.stop();
    QVERIFY(!offerer.telemetry().has_value());
    QVERIFY(!answerer.telemetry().has_value());

    // A new start owns a new callback bridge and therefore fresh counters.
    QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
    QSignalSpy answerReady(&answerer, &IMediaTransport::ready);
    QVERIFY(answerer.start({IMediaTransport::Role::Answerer,
                            kTestAudioSsrc}));
    QVERIFY(offerer.start({IMediaTransport::Role::Offerer,
                           kTestAudioSsrc}));
    QVERIFY(offerer.telemetry().has_value());
    QVERIFY(answerer.telemetry().has_value());
    QVERIFY(countersAreZero(*offerer.telemetry()));
    QVERIFY(countersAreZero(*answerer.telemetry()));
    QTRY_COMPARE_WITH_TIMEOUT(offerReady.size(), 1, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(answerReady.size(), 1, 10000);

    offerer.stop();
    answerer.stop();
}

void TestMediaTransport::boundedInputsRefuseBeforeTransport()
{
    LibDataChannelMediaTransport transport;
    QVERIFY(transport.start({IMediaTransport::Role::Answerer,
                             kTestAudioSsrc}));

    LibDataChannelMediaTransport offerer;
    QSignalSpy offerDescriptions(&offerer, &IMediaTransport::localDescription);
    QVERIFY(offerer.start({IMediaTransport::Role::Offerer,
                           kTestAudioSsrc}));
    QTRY_COMPARE_WITH_TIMEOUT(offerDescriptions.count(), 1, 5000);
    const QString candidateFreeOffer =
        offerDescriptions.at(0).at(0).toString();
    QVERIFY(!candidateFreeOffer.contains(QStringLiteral("a=candidate:"),
                                         Qt::CaseInsensitive));

    QVERIFY(!transport.acceptDescription(
        QString(IMediaTransport::kMaxDescriptionBytes + 1, QLatin1Char('x')),
        QStringLiteral("offer")));
    QVERIFY(!transport.acceptDescription(QStringLiteral("v=0"),
                                         QStringLiteral("answer")));

    QString embeddedRelay = candidateFreeOffer;
    if (!embeddedRelay.endsWith(QLatin1Char('\n'))) {
        embeddedRelay.append(QStringLiteral("\r\n"));
    }
    embeddedRelay.append(QStringLiteral(
        "a=candidate:1 1 UDP 1 192.0.2.1 5000 typ relay\r\n"));
    QVERIFY(!transport.acceptDescription(embeddedRelay,
                                         QStringLiteral("offer")));

    QString overBudget = candidateFreeOffer;
    if (!overBudget.endsWith(QLatin1Char('\n'))) {
        overBudget.append(QStringLiteral("\r\n"));
    }
    for (int i = 0; i <= IMediaTransport::kMaxRemoteCandidates; ++i) {
        overBudget.append(QStringLiteral(
            "a=candidate:%1 1 UDP 2122260223 192.0.2.%2 %3 typ host\r\n")
                              .arg(i + 1)
                              .arg(i + 1)
                              .arg(5000 + i));
    }
    QVERIFY(!transport.acceptDescription(overBudget,
                                         QStringLiteral("offer")));
    QVERIFY(!transport.acceptCandidate(
        QString(IMediaTransport::kMaxCandidateBytes + 1, QLatin1Char('x')),
        QStringLiteral("0")));
    QVERIFY(!transport.acceptCandidate(
        QStringLiteral("1 1 UDP 1 192.0.2.1 5000 typ relay"),
        QStringLiteral("0")));
    QVERIFY(!transport.sendDisplay(QByteArray(
        IMediaTransport::kMaxDisplayMessageBytes + 1, 'x')));
    QVERIFY(!transport.sendRtp(QByteArray(
        IMediaTransport::kMaxRawRtpBytes + 1, 'x')));
    QVERIFY(!transport.sendRtp(QByteArray(
        IMediaTransport::kMinRawRtpBytes - 1, 'x')));

    transport.stop();
    transport.stop();
    offerer.stop();
    QVERIFY(!transport.isReady());
}

void TestMediaTransport::stopCancelsOldCallbacksAndRecreates()
{
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    startPair(offerer, answerer);

    QSignalSpy offerClosed(&offerer, &IMediaTransport::closed);
    QSignalSpy answerClosed(&answerer, &IMediaTransport::closed);
    QSignalSpy lateDisplay(&answerer, &IMediaTransport::displayReceived);
    QSignalSpy lateRtp(&answerer, &IMediaTransport::rtpReceived);
    QVERIFY(offerer.sendDisplay(QByteArrayLiteral("queued-before-stop")));
    QVERIFY(offerer.sendRtp(rtpPacket(9)));
    offerer.stop();
    answerer.stop();
    QCOMPARE(offerClosed.count(), 1);
    QCOMPARE(answerClosed.count(), 1);
    QTest::qWait(100);
    QCOMPARE(lateDisplay.count(), 0);
    QCOMPARE(lateRtp.count(), 0);

    disconnect(&offerer, nullptr, &answerer, nullptr);
    disconnect(&answerer, nullptr, &offerer, nullptr);
    startPair(offerer, answerer);

    QSignalSpy displayReceived(&answerer, &IMediaTransport::displayReceived);
    QSignalSpy rtpReceived(&answerer, &IMediaTransport::rtpReceived);
    const QByteArray display("R3-recreated-display");
    const QByteArray rtp = rtpPacket(8);
    QVERIFY(offerer.sendDisplay(display));
    QVERIFY(offerer.sendRtp(rtp));
    QTRY_COMPARE_WITH_TIMEOUT(displayReceived.count(), 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(rtpReceived.count(), 1, 5000);
    QCOMPARE(displayReceived.at(0).at(0).toByteArray(), display);
    QCOMPARE(rtpReceived.at(0).at(0).toByteArray(), rtp);
}

void TestMediaTransport::signalRestartDropsRemainingOldGenerationMedia()
{
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    startPair(offerer, answerer);

    int displaysReceived = 0;
    int rtpReceived = 0;
    connect(&answerer, &IMediaTransport::displayReceived, &answerer,
            [&answerer, &displaysReceived](const QByteArray&) {
                ++displaysReceived;
                if (displaysReceived == 1) {
                    answerer.stop();
                    QVERIFY(answerer.start({IMediaTransport::Role::Answerer,
                                            kTestAudioSsrc}));
                }
            });
    connect(&answerer, &IMediaTransport::rtpReceived, &answerer,
            [&rtpReceived](const QByteArray&) { ++rtpReceived; });

    QVERIFY(offerer.sendDisplay(QByteArrayLiteral("old-display-0")));
    QVERIFY(offerer.sendDisplay(QByteArrayLiteral("old-display-1")));
    QVERIFY(offerer.sendDisplay(QByteArrayLiteral("old-display-2")));
    QVERIFY(offerer.sendRtp(rtpPacket(10)));
    QTRY_COMPARE_WITH_TIMEOUT(displaysReceived, 1, 5000);
    QTest::qWait(100);
    QCOMPARE(displaysReceived, 1);
    QCOMPARE(rtpReceived, 0);

    offerer.stop();
    answerer.stop();
}

void TestMediaTransport::deletionFromReceivedSignalIsSafe()
{
    LibDataChannelMediaTransport offerer;
    QPointer<LibDataChannelMediaTransport> answerer =
        new LibDataChannelMediaTransport;
    startPair(offerer, *answerer);

    connect(answerer, &IMediaTransport::displayReceived, answerer,
            [&answerer](const QByteArray&) { delete answerer.data(); });
    QVERIFY(offerer.sendDisplay(QByteArrayLiteral("delete-receiver")));
    QVERIFY(offerer.sendDisplay(QByteArrayLiteral("must-not-follow-delete")));
    QVERIFY(offerer.sendRtp(rtpPacket(11)));
    QTRY_VERIFY_WITH_TIMEOUT(answerer.isNull(), 5000);

    offerer.stop();
}


// G-127: a message can reach the answerer's library before the answerer's
// owner thread has seen its display channel open. Its next drain then finds
// both. The answerer reports ready first, so whoever handles the message
// (the traversal echo, or any reply) finds the transport ready and can send.
// The answerer's one drain timer is held from the end of its gathering until
// the offerer's message has arrived, which is the interleaving the traversal
// harness met under load.
void TestMediaTransport::readyPrecedesMessagesThatArriveWithIt()
{
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    wire(offerer, answerer);
    const QList<QTimer*> drainTimers =
        answerer.findChildren<QTimer*>(QString(), Qt::FindDirectChildrenOnly);
    QCOMPARE(drainTimers.size(), 1);
    QTimer* const answererDrain = drainTimers.constFirst();

    QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
    QSignalSpy answerReady(&answerer, &IMediaTransport::ready);
    QSignalSpy echoes(&offerer, &IMediaTransport::displayReceived);
    bool held = false;
    connect(&answerer, &IMediaTransport::gatheringComplete, &answerer,
            [answererDrain, &held] {
                answererDrain->stop();
                held = true;
            });
    QStringList order;
    bool readyAtReceipt = false;
    std::optional<IMediaTransport::DisplaySendResult> echoResult;
    connect(&answerer, &IMediaTransport::ready, &answerer,
            [&order] { order << QStringLiteral("ready"); });
    connect(&answerer, &IMediaTransport::displayReceived, &answerer,
            [&answerer, &order, &readyAtReceipt, &echoResult](const QByteArray& message) {
                order << QStringLiteral("display");
                readyAtReceipt = answerer.isReady();
                echoResult = answerer.submitDisplay(message);
            });

    QVERIFY(answerer.start({IMediaTransport::Role::Answerer, kTestAudioSsrc}));
    QVERIFY(offerer.start({IMediaTransport::Role::Offerer, kTestAudioSsrc}));
    QTRY_VERIFY_WITH_TIMEOUT(held, 10000);
    // The offerer becomes ready without the answerer's owner thread: its
    // display channel opens on the answerer library's acknowledgement.
    QTRY_COMPARE_WITH_TIMEOUT(offerReady.count(), 1, 10000);
    QCOMPARE(answerReady.count(), 0);

    // The traversal helper's payload: 60000 bytes, many SCTP fragments.
    QByteArray message(60000, Qt::Uninitialized);
    for (qsizetype index = 0; index < message.size(); ++index) {
        message[index] = static_cast<char>(index * 31 + 7);
    }
    QVERIFY(offerer.sendDisplay(message));
    QTRY_COMPARE_WITH_TIMEOUT(answerer.telemetry()->receivedDisplayPayloadBytes,
                              quint64(message.size()), 10000);
    QCOMPARE(answerReady.count(), 0);
    QVERIFY(order.isEmpty());

    answererDrain->start();
    QTRY_COMPARE_WITH_TIMEOUT(order.size(), 2, 10000);
    QCOMPARE(order, (QStringList{QStringLiteral("ready"), QStringLiteral("display")}));
    QVERIFY(readyAtReceipt);
    QVERIFY(echoResult.has_value());
    QVERIFY(*echoResult == IMediaTransport::DisplaySendResult::Sent
            || *echoResult == IMediaTransport::DisplaySendResult::Queued);
    QTRY_COMPARE_WITH_TIMEOUT(echoes.count(), 1, 10000);
    QCOMPARE(echoes.at(0).at(0).toByteArray(), message);

    offerer.stop();
    answerer.stop();
}


// R-R3-23 and RFC 7587 section 6.1: the Core's send-only offer describes the
// encoder it runs. No in-band FEC (the encoder has it off) and no average
// bitrate ceiling above the configured target; stereo and minptime stay.
void TestMediaTransport::offerDescribesTheRealEncoder_data()
{
    QTest::addColumn<int>("bitrate");
    QTest::addColumn<bool>("defaulted");
    QTest::newRow("default") << 48000 << true; // R-R3-21
    QTest::newRow("24000") << 24000 << false;
    QTest::newRow("48000") << 48000 << false;
}

void TestMediaTransport::offerDescribesTheRealEncoder()
{
    QFETCH(int, bitrate);
    QFETCH(bool, defaulted);
    LibDataChannelMediaTransport offerer;
    QSignalSpy descriptions(&offerer, &IMediaTransport::localDescription);
    IMediaTransport::StartOptions options{IMediaTransport::Role::Offerer, kTestAudioSsrc};
    if (!defaulted) {
        options.audioTargetBitrate = bitrate;
    }
    QVERIFY(offerer.start(options));
    QTRY_COMPARE_WITH_TIMEOUT(descriptions.count(), 1, 5000);
    const QString offer = descriptions.at(0).at(0).toString();
    QCOMPARE(descriptions.at(0).at(1).toString(), QStringLiteral("offer"));

    const QString expected =
        QStringLiteral("minptime=10;maxaveragebitrate=%1;stereo=1;sprop-stereo=1").arg(bitrate);
    QCOMPARE(opusFormatLine(offer), expected);
    QCOMPARE(opusOfferFormatParameters(bitrate), expected);
    QVERIFY(!offer.contains(QStringLiteral("useinbandfec"), Qt::CaseInsensitive));
    QVERIFY(offer.contains(QStringLiteral("a=rtpmap:111 opus/48000/2")));
    QVERIFY(offer.contains(QStringLiteral("a=sendonly")));
    offerer.stop();
}

// The GUI answers this Core's offer and an old Core's offer alike: the real
// answerer negotiates, becomes ready and receives the audio track's RTP.
void TestMediaTransport::answererAcceptsNewAndOldCoreOffers_data()
{
    QTest::addColumn<int>("bitrate");
    QTest::addColumn<bool>("oldCore");
    QTest::newRow("new-24000") << 24000 << false;
    QTest::newRow("new-48000") << 48000 << false;
    QTest::newRow("old-core") << 24000 << true;
}

void TestMediaTransport::answererAcceptsNewAndOldCoreOffers()
{
    QFETCH(int, bitrate);
    QFETCH(bool, oldCore);
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;

    QString deliveredOffer;
    connect(&offerer, &IMediaTransport::localDescription, &answerer,
            [&answerer, &deliveredOffer, oldCore](const QString& sdp, const QString& type) {
                QString offer = sdp;
                if (oldCore) {
                    const QString current = opusFormatLine(sdp);
                    QVERIFY(!current.isEmpty());
                    offer.replace(QStringLiteral("a=fmtp:111 ") + current,
                                  QStringLiteral("a=fmtp:111 ")
                                      + QLatin1String(kOldCoreOpusParameters));
                }
                deliveredOffer = offer;
                QVERIFY2(answerer.acceptDescription(offer, type), "answerer rejected offer");
            });
    connect(&answerer, &IMediaTransport::localDescription, &offerer,
            [&offerer](const QString& sdp, const QString& type) {
                QVERIFY2(offerer.acceptDescription(sdp, type), "offerer rejected answer");
            });
    connect(&offerer, &IMediaTransport::localCandidate, &answerer,
            [&answerer](const QString& candidate, const QString& mid) {
                QVERIFY2(answerer.acceptCandidate(candidate, mid),
                         "answerer rejected host candidate");
            });
    connect(&answerer, &IMediaTransport::localCandidate, &offerer,
            [&offerer](const QString& candidate, const QString& mid) {
                QVERIFY2(offerer.acceptCandidate(candidate, mid),
                         "offerer rejected host candidate");
            });

    QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
    QSignalSpy answerReady(&answerer, &IMediaTransport::ready);
    QSignalSpy answerErrors(&answerer, &IMediaTransport::errorOccurred);
    QVERIFY(answerer.start({IMediaTransport::Role::Answerer, kTestAudioSsrc}));
    QVERIFY(offerer.start({IMediaTransport::Role::Offerer, kTestAudioSsrc, bitrate}));
    QTRY_COMPARE_WITH_TIMEOUT(offerReady.count(), 1, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(answerReady.count(), 1, 10000);
    QCOMPARE(opusFormatLine(deliveredOffer),
             oldCore ? QString::fromLatin1(kOldCoreOpusParameters)
                     : opusOfferFormatParameters(bitrate));

    QSignalSpy rtpReceived(&answerer, &IMediaTransport::rtpReceived);
    const QByteArray rtp = rtpPacket(21);
    QVERIFY(offerer.sendRtp(rtp));
    QTRY_COMPARE_WITH_TIMEOUT(rtpReceived.count(), 1, 5000);
    QCOMPARE(rtpReceived.at(0).at(0).toByteArray(), rtp);
    QCOMPARE(answerErrors.count(), 0);
    offerer.stop();
    answerer.stop();
}

// R-R3-28, amended 2026-09-23. MediaPeer tells a transient refusal (the peer
// could not be built: an error, then false) from a permanent one (false with
// no error). The real transport's precondition refusals must stay silent.
void TestMediaTransport::preconditionRefusalsReportNoError()
{
    LibDataChannelMediaTransport transport;
    QSignalSpy errors(&transport, &IMediaTransport::errorOccurred);
    // An SSRC of zero.
    QVERIFY(!transport.start({IMediaTransport::Role::Answerer, 0}));
    QCOMPARE(errors.size(), 0);
    // Already started.
    QVERIFY(transport.start({IMediaTransport::Role::Answerer, kTestAudioSsrc}));
    QVERIFY(!transport.start({IMediaTransport::Role::Answerer, kTestAudioSsrc}));
    QCOMPARE(errors.size(), 0);
    transport.stop();
}

void TestMediaTransport::wireExchange(LibDataChannelMediaTransport& offerer,
                                      LibDataChannelMediaTransport& answerer,
                                      QString* offerOut, QString* answerOut)
{
    connect(&offerer, &IMediaTransport::localDescription, &answerer,
            [&answerer, offerOut](const QString& sdp, const QString& type) {
                *offerOut = sdp;
                QVERIFY2(answerer.acceptDescription(sdp, type), "answerer rejected offer");
            });
    connect(&answerer, &IMediaTransport::localDescription, &offerer,
            [&offerer, answerOut](const QString& sdp, const QString& type) {
                *answerOut = sdp;
                QVERIFY2(offerer.acceptDescription(sdp, type), "offerer rejected answer");
            });
    connect(&offerer, &IMediaTransport::localCandidate, &answerer,
            [&answerer](const QString& candidate, const QString& mid) {
                QVERIFY2(answerer.acceptCandidate(candidate, mid),
                         "answerer rejected host candidate");
            });
    connect(&answerer, &IMediaTransport::localCandidate, &offerer,
            [&offerer](const QString& candidate, const QString& mid) {
                QVERIFY2(offerer.acceptCandidate(candidate, mid),
                         "offerer rejected host candidate");
            });
}

// R-R3-23: a GUI that did not ask for lossless receives exactly today's
// audio description (golden); one that asked gets the L16 rtpmap added to
// the same m-line, after Opus, with no fmtp of its own.
void TestMediaTransport::losslessRtpMapIsOfferedOnlyWhenAsked()
{
    const QStringList todaysAudio{
        QStringLiteral("m=audio 9 UDP/TLS/RTP/SAVPF 111"),
        QStringLiteral("a=rtpmap:111 opus/48000/2"),
        QStringLiteral("a=fmtp:111 minptime=10;maxaveragebitrate=48000;stereo=1;sprop-stereo=1"),
    };
    for (const bool lossless : {false, true}) {
        LibDataChannelMediaTransport offerer;
        QSignalSpy descriptions(&offerer, &IMediaTransport::localDescription);
        IMediaTransport::StartOptions options{IMediaTransport::Role::Offerer, kTestAudioSsrc};
        options.offerLosslessAudio = lossless;
        QVERIFY(offerer.start(options));
        QTRY_COMPARE_WITH_TIMEOUT(descriptions.count(), 1, 5000);
        const QString offer = descriptions.at(0).at(0).toString();
        if (!lossless) {
            QCOMPARE(audioFormatLines(offer), todaysAudio);
            QVERIFY(!offer.contains(QStringLiteral("L16"), Qt::CaseInsensitive));
        } else {
            // The m= line's format order is the preference: Opus first.
            // libdatachannel writes the rtpmap lines in payload type order.
            QCOMPARE(audioFormatLines(offer), (QStringList{
                QStringLiteral("m=audio 9 UDP/TLS/RTP/SAVPF 111 96"),
                QStringLiteral("a=rtpmap:96 L16/48000/2"),
                todaysAudio.at(1), todaysAudio.at(2)}));
        }
        // One m=audio line either way, and nothing negotiated before an answer.
        QCOMPARE(offer.count(QStringLiteral("m=audio")), 1);
        QVERIFY(!offerer.losslessAudioNegotiated());
        offerer.stop();
    }
}

void TestMediaTransport::answerKeepsTheLosslessRtpMap_data()
{
    QTest::addColumn<bool>("lossless");
    QTest::newRow("today") << false;
    QTest::newRow("lossless") << true;
}

// Established against the real library, not assumed: libdatachannel's
// answer to an offer carrying L16 keeps the rtpmap (it reciprocates the
// offered media), so both ends agree lossless audio may flow. Without the
// offer flag neither end does, and the answer is today's.
void TestMediaTransport::answerKeepsTheLosslessRtpMap()
{
    QFETCH(bool, lossless);
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    QString offer;
    QString answer;
    wireExchange(offerer, answerer, &offer, &answer);
    QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
    QSignalSpy answerReady(&answerer, &IMediaTransport::ready);
    QSignalSpy answerErrors(&answerer, &IMediaTransport::errorOccurred);
    QVERIFY(answerer.start({IMediaTransport::Role::Answerer, kTestAudioSsrc}));
    IMediaTransport::StartOptions options{IMediaTransport::Role::Offerer, kTestAudioSsrc};
    options.offerLosslessAudio = lossless;
    QVERIFY(offerer.start(options));
    QTRY_COMPARE_WITH_TIMEOUT(offerReady.count(), 1, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(answerReady.count(), 1, 10000);

    const QStringList answerAudio = audioFormatLines(answer);
    QVERIFY2(!answerAudio.isEmpty(), qPrintable(answer));
    QCOMPARE(answerAudio.contains(QStringLiteral("a=rtpmap:96 L16/48000/2")), lossless);
    QVERIFY(answerAudio.contains(QStringLiteral("a=rtpmap:111 opus/48000/2")));
    QVERIFY(answer.contains(QStringLiteral("a=recvonly")));
    QCOMPARE(offerer.losslessAudioNegotiated(), lossless);
    QCOMPARE(answerer.losslessAudioNegotiated(), lossless);

    // Both payload types cross the same track.
    QSignalSpy rtpReceived(&answerer, &IMediaTransport::rtpReceived);
    const QByteArray opus = rtpPacket(30);
    const QByteArray l16 = PcmAudioPacketiser{}.encode(
        QVector<float>(PcmAudioCodecConfig::kPacketFrames * 2, 0.5f), 31, 1920,
        kTestAudioSsrc).packet;
    QVERIFY(offerer.sendRtp(opus));
    QVERIFY(offerer.sendRtp(l16));
    QTRY_COMPARE_WITH_TIMEOUT(rtpReceived.count(), 2, 5000);
    QCOMPARE(rtpReceived.at(0).at(0).toByteArray(), opus);
    QCOMPARE(rtpReceived.at(1).at(0).toByteArray(), l16);
    QCOMPARE(answerErrors.count(), 0);
    offerer.stop();
    answerer.stop();
    QVERIFY(!offerer.losslessAudioNegotiated());
    QVERIFY(!answerer.losslessAudioNegotiated());
}

void TestMediaTransport::ordinaryMediaHostOnLoopbackIsNotAnOwnedShim()
{
    LibDataChannelMediaTransport offerer(nullptr, LibDataChannelMediaTransport::CandidatePolicy::AnyIceType);
    LibDataChannelMediaTransport answerer(nullptr, LibDataChannelMediaTransport::CandidatePolicy::AnyIceType);
    QStringList offeredCandidates;
    QStringList answeredCandidates;
    bool descriptionsAccepted = true;
    connect(&offerer, &IMediaTransport::localDescription, &answerer,
            [&](const QString& sdp, const QString& type) {
                descriptionsAccepted &= answerer.acceptDescription(sdp, type);
            });
    connect(&answerer, &IMediaTransport::localDescription, &offerer,
            [&](const QString& sdp, const QString& type) {
                descriptionsAccepted &= offerer.acceptDescription(sdp, type);
            });
    connect(&offerer, &IMediaTransport::localCandidate, &offerer,
            [&](const QString& candidate, const QString&) { offeredCandidates.append(candidate); });
    connect(&answerer, &IMediaTransport::localCandidate, &answerer,
            [&](const QString& candidate, const QString&) { answeredCandidates.append(candidate); });
    const auto loopbackFor = [](const QStringList& gathered) {
        for (const QString& line : gathered) {
            const QStringList fields = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (fields.size() < 8 || !fields.at(4).contains(QLatin1Char('.'))
                || fields.at(7) != QLatin1String("host")) {
                continue;
            }
            bool parsed = false;
            const int port = fields.at(5).toInt(&parsed);
            if (parsed && port > 0 && port <= 65535) {
                // The source is absent; a remote peer may call itself
                // wsrelay without making this a locally owned shim.
                return RelayLeg::candidateLine(IceConfiguration::kMediaLane,
                                               static_cast<quint16>(port));
            }
        }
        return QString();
    };
    IceConfiguration ice = IceConfiguration::throughRendezvous({}, true, {}, {});
    ice.setRelay(std::nullopt, 1);
    IMediaTransport::StartOptions device;
    device.role = IMediaTransport::Role::Answerer;
    device.localAudioSsrc = kTestAudioSsrc;
    device.ice = ice;
    IMediaTransport::StartOptions core = device;
    core.role = IMediaTransport::Role::Offerer;
    QVERIFY(answerer.start(device));
    QVERIFY(offerer.start(core));
    QTRY_VERIFY_WITH_TIMEOUT(descriptionsAccepted
        && !loopbackFor(offeredCandidates).isEmpty()
        && !loopbackFor(answeredCandidates).isEmpty(), 10000);
    QVERIFY(offerer.acceptCandidate(loopbackFor(answeredCandidates), {}));
    QVERIFY(answerer.acceptCandidate(loopbackFor(offeredCandidates), {}));
    QTRY_VERIFY_WITH_TIMEOUT(offerer.isReady() && answerer.isReady(), 10000);
    QVERIFY(offerer.selectedPath());
    QVERIFY(answerer.selectedPath());
    QVERIFY(MediaIcePath::loopbackEndpoint(offerer.selectedPath()->remoteAddress,
                                           offerer.selectedPath()->remotePort));
    QVERIFY(MediaIcePath::loopbackEndpoint(answerer.selectedPath()->remoteAddress,
                                           answerer.selectedPath()->remotePort));
    QVERIFY(!offerer.selectedPath()->viaLoopbackShim());
    QVERIFY(!answerer.selectedPath()->viaLoopbackShim());
    const auto route = offerer.selectedPath()->networkPathSnapshot();
    QVERIFY(route);
    QCOMPARE(route->kind, NetworkPathSnapshot::Kind::Direct);
    QCOMPARE(route->carrier, NetworkPathSnapshot::Carrier::Ice);
    QCOMPARE(route->endpoints, NetworkPathSnapshot::Endpoints::IceCandidates);
    QVERIFY(route->localPort != 0);
    QVERIFY(route->remotePort != 0);
}

void TestMediaTransport::retiredMediaSourceCannotMarkARestartedPeer()
{
    LibDataChannelMediaTransport offerer(nullptr, LibDataChannelMediaTransport::CandidatePolicy::AnyIceType);
    const auto oldSource = std::make_shared<RetainedCandidateSource>();
    IceConfiguration firstIce = IceConfiguration::throughRendezvous({}, true, {}, {});
    firstIce.setRelay(std::nullopt, 1);
    firstIce.setCandidateSourceFactory(
        [oldSource](int lane, const QString&, bool) -> std::shared_ptr<IceConfiguration::CandidateSource> {
            return lane == IceConfiguration::kMediaLane ? oldSource : nullptr;
        }, false);
    IMediaTransport::StartOptions first;
    first.role = IMediaTransport::Role::Offerer;
    first.localAudioSsrc = kTestAudioSsrc;
    first.ice = firstIce;
    QVERIFY(offerer.start(first));
    QTRY_VERIFY_WITH_TIMEOUT(oldSource->ready(), 10000);
    const auto staleCallback = oldSource->callback();
    QVERIFY(staleCallback);
    offerer.stop();
    QVERIFY(!offerer.selectedPath());
    // A callback already queued by the old source can run after stop.
    staleCallback(RelayLeg::candidateLine(IceConfiguration::kMediaLane, 65000));

    LibDataChannelMediaTransport answerer(nullptr, LibDataChannelMediaTransport::CandidatePolicy::AnyIceType);
    QStringList offeredCandidates;
    QStringList answeredCandidates;
    QString pendingAnswer;
    bool descriptionAccepted = true;
    connect(&offerer, &IMediaTransport::localDescription, &answerer,
            [&](const QString& sdp, const QString& type) {
                descriptionAccepted &= answerer.acceptDescription(sdp, type);
            });
    connect(&answerer, &IMediaTransport::localDescription, &offerer,
            [&](const QString& sdp, const QString& type) {
                QCOMPARE(type, QStringLiteral("answer"));
                pendingAnswer = sdp;
            });
    connect(&offerer, &IMediaTransport::localCandidate, &offerer,
            [&](const QString& candidate, const QString&) { offeredCandidates.append(candidate); });
    connect(&answerer, &IMediaTransport::localCandidate, &answerer,
            [&](const QString& candidate, const QString&) { answeredCandidates.append(candidate); });
    const auto loopbackFor = [](const QStringList& gathered) {
        for (const QString& line : gathered) {
            const QStringList fields = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (fields.size() < 8 || !fields.at(4).contains(QLatin1Char('.'))
                || fields.at(7) != QLatin1String("host")) {
                continue;
            }
            bool parsed = false;
            const int port = fields.at(5).toInt(&parsed);
            if (parsed && port > 0 && port <= 65535) {
                return RelayLeg::candidateLine(IceConfiguration::kMediaLane,
                                               static_cast<quint16>(port));
            }
        }
        return QString();
    };
    IceConfiguration freshIce = IceConfiguration::throughRendezvous({}, true, {}, {});
    freshIce.setRelay(std::nullopt, 1);
    IMediaTransport::StartOptions device;
    device.role = IMediaTransport::Role::Answerer;
    device.localAudioSsrc = kTestAudioSsrc;
    device.ice = freshIce;
    IMediaTransport::StartOptions core = device;
    core.role = IMediaTransport::Role::Offerer;
    QVERIFY(answerer.start(device));
    QVERIFY(offerer.start(core));
    QTRY_VERIFY_WITH_TIMEOUT(descriptionAccepted && !pendingAnswer.isEmpty()
        && !loopbackFor(offeredCandidates).isEmpty()
        && !loopbackFor(answeredCandidates).isEmpty(), 10000);
    const QString selectedCandidate = loopbackFor(answeredCandidates);
    // The exact old callback arriving in a new generation must not turn
    // the ordinary incoming host candidate into an owned source endpoint.
    staleCallback(selectedCandidate);
    QVERIFY(offerer.acceptDescription(pendingAnswer, QStringLiteral("answer")));
    QVERIFY(offerer.acceptCandidate(selectedCandidate, {}));
    QVERIFY(answerer.acceptCandidate(loopbackFor(offeredCandidates), {}));
    QTRY_VERIFY_WITH_TIMEOUT(offerer.isReady() && answerer.isReady(), 10000);
    QVERIFY(offerer.selectedPath());
    QVERIFY(MediaIcePath::loopbackEndpoint(offerer.selectedPath()->remoteAddress,
                                           offerer.selectedPath()->remotePort));
    QVERIFY(!offerer.selectedPath()->viaLoopbackShim());
    const auto route = offerer.selectedPath()->networkPathSnapshot();
    QVERIFY(route);
    QCOMPARE(route->kind, NetworkPathSnapshot::Kind::Direct);
    QCOMPARE(route->carrier, NetworkPathSnapshot::Carrier::Ice);
}

// R-R3-23 acceptance: real DTLS/SRTP between two real peers carries the
// lossless stream at its own rate, 48 kHz x 2 x 16 bit = 1.536 Mbit/s of
// payload, sent as the Core sends it: ten packets back to back per 40 ms
// capture block. Every packet arrives intact and in order.
void TestMediaTransport::losslessAudioCrossesRealEncryptedLoopback()
{
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    QString offer;
    QString answer;
    wireExchange(offerer, answerer, &offer, &answer);
    QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
    QSignalSpy answerReady(&answerer, &IMediaTransport::ready);
    QVERIFY(answerer.start({IMediaTransport::Role::Answerer, kTestAudioSsrc}));
    IMediaTransport::StartOptions options{IMediaTransport::Role::Offerer, kTestAudioSsrc};
    options.offerLosslessAudio = true;
    QVERIFY(offerer.start(options));
    QTRY_COMPARE_WITH_TIMEOUT(offerReady.count(), 1, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(answerReady.count(), 1, 10000);
    QVERIFY(offerer.losslessAudioNegotiated());
    QVERIFY(offerer.selectedPath());
    QVERIFY(answerer.selectedPath());
    QVERIFY(!offerer.selectedPath()->viaLoopbackShim());
    QVERIFY(!answerer.selectedPath()->viaLoopbackShim());

    QList<QByteArray> received;
    connect(&answerer, &IMediaTransport::rtpReceived, this,
            [&received](const QByteArray& packet) { received.append(packet); });

    constexpr int kBlocks = 50; // two seconds of audio
    constexpr int kPackets = kBlocks * PcmAudioCodecConfig::kPacketsPerBlock;
    QVector<float> block(PcmAudioCodecConfig::kBlockFrames * 2);
    for (int index = 0; index < block.size(); ++index) {
        block[index] = static_cast<float>(std::sin(0.01 * index)) * 0.8f;
    }
    const PcmAudioPacketiser packetiser;
    QList<QByteArray> sent;
    int accepted = 0;
    QElapsedTimer wall;
    wall.start();
    for (int b = 0; b < kBlocks; ++b) {
        // Hold the Core's cadence: block b is due at b * 40 ms.
        while (wall.elapsed() < b * 40) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        }
        const QList<QByteArray> packets = packetiser.packetiseBlock(
            block, static_cast<quint16>(b * 10), static_cast<quint32>(b * 1920),
            kTestAudioSsrc);
        QCOMPARE(packets.size(), 10);
        for (const QByteArray& packet : packets) {
            sent.append(packet);
            accepted += offerer.sendRtp(packet) ? 1 : 0;
        }
    }
    const qint64 sendMs = wall.elapsed();
    QTRY_COMPARE_WITH_TIMEOUT(received.size(), kPackets, 10000);
    const qint64 receiveMs = wall.elapsed();
    QCOMPARE(accepted, kPackets);
    QCOMPARE(received, sent);

    quint64 payloadBytes = 0;
    for (const QByteArray& packet : received) {
        const PcmRtpDecodeResult decoded = decodeL16Rtp(packet, kTestAudioSsrc);
        QCOMPARE(decoded.status, OpusAudioCodecStatus::Accepted);
        payloadBytes += PcmAudioCodecConfig::kPayloadBytes;
    }
    // Media time, from the RTP clock: exactly the lossless payload rate.
    const double mediaSeconds = static_cast<double>(kPackets)
        * PcmAudioCodecConfig::kPacketFrames / PcmAudioCodecConfig::kSampleRate;
    const double payloadBitsPerSecond = payloadBytes * 8.0 / mediaSeconds;
    QCOMPARE(payloadBitsPerSecond, 1'536'000.0);
    // Wall time, for the record: the whole stream was carried in about its
    // own duration.
    const double wallMbps = payloadBytes * 8.0 / (receiveMs / 1000.0) / 1e6;
    qInfo("lossless loopback: %d packets, %llu payload bytes, sent over %lld ms, "
          "all received by %lld ms, %.3f Mbit/s payload by wall clock",
          kPackets, static_cast<unsigned long long>(payloadBytes),
          static_cast<long long>(sendMs), static_cast<long long>(receiveMs), wallMbps);
    const std::optional<MediaTransportTelemetry> traffic = answerer.telemetry();
    QVERIFY(traffic.has_value());
    QCOMPARE(traffic->receivedRtpBytes,
             quint64(kPackets) * quint64(PcmAudioCodecConfig::kRtpPacketBytes));
    offerer.stop();
    answerer.stop();
}

// R-R3-43, observed against the real library before anything was built on
// it: libdatachannel v0.24.5 hands every RTP packet to the one audio track
// when a connection has a single media line
// (src/impl/peerconnection.cpp:562-566), whatever its SSRC and whether or not
// any description declared it. An undeclared stream is therefore DELIVERED to
// the transport, not dropped; refusing it is MediaPeer's job (its receive
// filter), as it has been for the main stream. The declared-streams design
// does not depend on this: the Core declares every receiver SSRC it may send.
void TestMediaTransport::undeclaredStreamIsDeliveredByTheOneAudioLine()
{
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    QString deliveredOffer;
    connect(&offerer, &IMediaTransport::localDescription, &answerer,
            [&answerer, &deliveredOffer](const QString& sdp, const QString& type) {
                // The answerer never sees the receiver streams declared.
                deliveredOffer = withoutSsrcLines(sdp, kTestReceiverSsrcs);
                QVERIFY2(answerer.acceptDescription(deliveredOffer, type),
                         "answerer rejected offer");
            });
    connect(&answerer, &IMediaTransport::localDescription, &offerer,
            [&offerer](const QString& sdp, const QString& type) {
                QVERIFY2(offerer.acceptDescription(sdp, type), "offerer rejected answer");
            });
    connect(&offerer, &IMediaTransport::localCandidate, &answerer,
            [&answerer](const QString& candidate, const QString& mid) {
                QVERIFY2(answerer.acceptCandidate(candidate, mid),
                         "answerer rejected host candidate");
            });
    connect(&answerer, &IMediaTransport::localCandidate, &offerer,
            [&offerer](const QString& candidate, const QString& mid) {
                QVERIFY2(offerer.acceptCandidate(candidate, mid),
                         "offerer rejected host candidate");
            });
    QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
    QSignalSpy answerReady(&answerer, &IMediaTransport::ready);
    QSignalSpy answerErrors(&answerer, &IMediaTransport::errorOccurred);
    QVERIFY(answerer.start({IMediaTransport::Role::Answerer, kTestAudioSsrc}));
    IMediaTransport::StartOptions options{IMediaTransport::Role::Offerer, kTestAudioSsrc};
    options.receiverAudioSsrcs = kTestReceiverSsrcs;
    QVERIFY(offerer.start(options));
    QTRY_COMPARE_WITH_TIMEOUT(offerReady.count(), 1, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(answerReady.count(), 1, 10000);
    QCOMPARE(audioSsrcLines(deliveredOffer),
             QStringList{QStringLiteral("a=ssrc:%1 cname:nereus-mixed-stereo")
                             .arg(kTestAudioSsrc)});

    QSignalSpy rtpReceived(&answerer, &IMediaTransport::rtpReceived);
    const QByteArray main = rtpPacket(40);
    const QByteArray undeclared = rtpPacket(41, 15, kTestReceiverSsrcs.at(1));
    const QByteArray mainAfter = rtpPacket(42);
    QVERIFY(offerer.sendRtp(main));
    QVERIFY(offerer.sendRtp(undeclared));
    QVERIFY(offerer.sendRtp(mainAfter));
    QTRY_COMPARE_WITH_TIMEOUT(rtpReceived.count(), 3, 5000);
    QCOMPARE(rtpReceived.at(0).at(0).toByteArray(), main);
    QCOMPARE(rtpReceived.at(1).at(0).toByteArray(), undeclared);
    QCOMPARE(rtpReceived.at(2).at(0).toByteArray(), mainAfter);
    QCOMPARE(answerErrors.count(), 0);
    qInfo("undeclared SSRC 0x%08x: delivered by libdatachannel to the one audio track",
          kTestReceiverSsrcs.at(1));
    offerer.stop();
    answerer.stop();
}

// R-R3-43: without receiver streams the offer and the answer are today's,
// byte for byte in every audio line (m=, rtpmap, fmtp and the one a=ssrc
// line). With them, the same audio line gains one a=ssrc line per receiver
// stream after the main one, and nothing else changes on either side.
void TestMediaTransport::receiverStreamsAreDeclaredOnlyWhenAsked()
{
    const QStringList todaysAudio{
        QStringLiteral("m=audio 9 UDP/TLS/RTP/SAVPF 111"),
        QStringLiteral("a=rtpmap:111 opus/48000/2"),
        QStringLiteral("a=fmtp:111 minptime=10;maxaveragebitrate=48000;stereo=1;sprop-stereo=1"),
    };
    const QString todaysSsrcLine =
        QStringLiteral("a=ssrc:%1 cname:nereus-mixed-stereo").arg(kTestAudioSsrc);
    // Today's whole offer, per-run values aside (golden).
    const QStringList todaysOffer{
        QStringLiteral("v=0"),
        QStringLiteral("o=rtc <per-run> 0 IN IP4 127.0.0.1"),
        QStringLiteral("s=-"),
        QStringLiteral("t=0 0"),
        QStringLiteral("a=group:BUNDLE audio 0"),
        QStringLiteral("a=group:LS audio"),
        QStringLiteral("a=msid-semantic:WMS *"),
        QStringLiteral("a=ice-options:ice2,trickle"),
        QStringLiteral("a=fingerprint:sha-256 <per-run>"),
        todaysAudio.at(0),
        QStringLiteral("c=IN IP4 0.0.0.0"),
        QStringLiteral("a=mid:audio"),
        QStringLiteral("a=sendonly"),
        todaysSsrcLine,
        QStringLiteral("a=rtcp-mux"),
        todaysAudio.at(1),
        todaysAudio.at(2),
        QStringLiteral("a=setup:actpass"),
        QStringLiteral("a=ice-ufrag:<per-run>"),
        QStringLiteral("a=ice-pwd:<per-run>"),
        QStringLiteral("m=application 9 UDP/DTLS/SCTP webrtc-datachannel"),
        QStringLiteral("c=IN IP4 0.0.0.0"),
        QStringLiteral("a=mid:0"),
        QStringLiteral("a=sendrecv"),
        QStringLiteral("a=sctp-port:5000"),
        QStringLiteral("a=max-message-size:65536"),
        QStringLiteral("a=setup:actpass"),
        QStringLiteral("a=ice-ufrag:<per-run>"),
        QStringLiteral("a=ice-pwd:<per-run>"),
        QString(),
    };
    QStringList stableAnswer[2];
    QStringList answerSsrcLines[2];
    QStringList answerAudio[2];
    for (const bool receivers : {false, true}) {
        LibDataChannelMediaTransport offerer;
        LibDataChannelMediaTransport answerer;
        QString offer;
        QString answer;
        wireExchange(offerer, answerer, &offer, &answer);
        QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
        QSignalSpy answerReady(&answerer, &IMediaTransport::ready);
        IMediaTransport::StartOptions answerOptions{IMediaTransport::Role::Answerer,
                                                    kTestAudioSsrc};
        IMediaTransport::StartOptions offerOptions{IMediaTransport::Role::Offerer,
                                                   kTestAudioSsrc};
        if (receivers) {
            answerOptions.receiverAudioSsrcs = kTestReceiverSsrcs;
            offerOptions.receiverAudioSsrcs = kTestReceiverSsrcs;
        }
        QVERIFY(answerer.start(answerOptions));
        QVERIFY(offerer.start(offerOptions));
        QTRY_COMPARE_WITH_TIMEOUT(offerReady.count(), 1, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(answerReady.count(), 1, 10000);

        QCOMPARE(audioFormatLines(offer), todaysAudio);
        QCOMPARE(offer.count(QStringLiteral("m=audio")), 1);
        if (!receivers) {
            QCOMPARE(audioSsrcLines(offer), QStringList{todaysSsrcLine});
            QCOMPARE(stableDescriptionLines(offer), todaysOffer);
        } else {
            QStringList expected{todaysSsrcLine};
            for (qsizetype index = 0; index < kTestReceiverSsrcs.size(); ++index) {
                expected << QStringLiteral("a=ssrc:%1 cname:nereus-receiver-%2")
                                .arg(kTestReceiverSsrcs.at(index))
                                .arg(index);
            }
            QCOMPARE(audioSsrcLines(offer), expected);
            // Today's offer with the receiver lines after the main one.
            QStringList withReceivers = todaysOffer;
            withReceivers.insert(withReceivers.indexOf(todaysSsrcLine) + 1,
                                 QStringList(expected.mid(1)).join(QStringLiteral("\r\n")));
            QCOMPARE(stableDescriptionLines(offer).join(QStringLiteral("\r\n")),
                     withReceivers.join(QStringLiteral("\r\n")));
        }
        stableAnswer[receivers ? 1 : 0] = stableDescriptionLines(answer);
        answerSsrcLines[receivers ? 1 : 0] = audioSsrcLines(answer);
        answerAudio[receivers ? 1 : 0] = audioFormatLines(answer);
        offerer.stop();
        answerer.stop();
    }
    // The answer does not change with the option.
    QCOMPARE(stableAnswer[1], stableAnswer[0]);
    QCOMPARE(answerSsrcLines[1], answerSsrcLines[0]);
    QCOMPARE(answerAudio[1], answerAudio[0]);
    QVERIFY(!answerAudio[0].isEmpty());
}

// R-R3-43: a declared receiver stream arrives intact and in order beside the
// main one, lossless included; the send filter takes exactly the declared set.
void TestMediaTransport::declaredReceiverStreamsCrossBesideTheMainOne()
{
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    QString offer;
    QString answer;
    wireExchange(offerer, answerer, &offer, &answer);
    QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
    QSignalSpy answerReady(&answerer, &IMediaTransport::ready);
    QSignalSpy answerErrors(&answerer, &IMediaTransport::errorOccurred);
    IMediaTransport::StartOptions answerOptions{IMediaTransport::Role::Answerer, kTestAudioSsrc};
    answerOptions.receiverAudioSsrcs = kTestReceiverSsrcs;
    IMediaTransport::StartOptions offerOptions{IMediaTransport::Role::Offerer, kTestAudioSsrc};
    offerOptions.offerLosslessAudio = true;
    offerOptions.receiverAudioSsrcs = kTestReceiverSsrcs;
    QVERIFY(answerer.start(answerOptions));
    QVERIFY(offerer.start(offerOptions));
    QTRY_COMPARE_WITH_TIMEOUT(offerReady.count(), 1, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(answerReady.count(), 1, 10000);
    QVERIFY(offerer.losslessAudioNegotiated());

    QList<QByteArray> received;
    connect(&answerer, &IMediaTransport::rtpReceived, this,
            [&received](const QByteArray& packet) { received.append(packet); });

    QVector<float> block(PcmAudioCodecConfig::kBlockFrames * 2);
    for (int index = 0; index < block.size(); ++index) {
        block[index] = static_cast<float>(std::sin(0.02 * index)) * 0.6f;
    }
    const PcmAudioPacketiser packetiser;
    const QList<QByteArray> mainPackets =
        packetiser.packetiseBlock(block, 100, 0, kTestAudioSsrc);
    const QList<QByteArray> receiverPackets =
        packetiser.packetiseBlock(block, 500, 0, kTestReceiverSsrcs.at(2));
    QList<QByteArray> sent;
    for (qsizetype index = 0; index < mainPackets.size(); ++index) {
        sent << mainPackets.at(index) << receiverPackets.at(index);
    }
    // One Opus-shaped packet on every other declared receiver stream.
    sent << rtpPacket(7, 15, kTestReceiverSsrcs.at(0))
         << rtpPacket(8, 15, kTestReceiverSsrcs.at(1))
         << rtpPacket(9, 15, kTestReceiverSsrcs.at(3));
    for (const QByteArray& packet : sent) {
        QVERIFY(offerer.sendRtp(packet));
    }
    // Not declared: refused before the library.
    QVERIFY(!offerer.sendRtp(rtpPacket(10, 15, kTestAudioSsrc + 0x100)));
    QTRY_COMPARE_WITH_TIMEOUT(received.size(), sent.size(), 5000);
    QCOMPARE(received, sent);
    for (qsizetype index = 0; index < receiverPackets.size(); ++index) {
        const PcmRtpDecodeResult decoded =
            decodeL16Rtp(received.at(2 * index + 1), kTestReceiverSsrcs.at(2));
        QCOMPARE(decoded.status, OpusAudioCodecStatus::Accepted);
    }
    QCOMPARE(answerErrors.count(), 0);
    offerer.stop();
    answerer.stop();
}

void TestMediaTransport::receiveQueueHoldsEveryDeclaredStream_data()
{
    QTest::addColumn<int>("receivers");
    QTest::addColumn<bool>("overflow");
    QTest::newRow("main-only") << 0 << false;
    QTest::newRow("main-only-overflow") << 0 << true;
    QTest::newRow("four-receivers") << 4 << false;
    QTest::newRow("four-receivers-overflow") << 4 << true;
}

// R-R3-43, R-R3-05: the receive queue between two drains holds 256 ms of
// lossless audio for the main stream and every declared receiver stream,
// computed here from the lossless packet rate, and stays bounded: one packet
// more drops the oldest. Without receiver streams it is today's queue.
void TestMediaTransport::receiveQueueHoldsEveryDeclaredStream()
{
    QFETCH(int, receivers);
    QFETCH(bool, overflow);
    const QList<quint32> receiverSsrcs = kTestReceiverSsrcs.mid(0, receivers);
    constexpr int kLosslessPacketsPerSecond =
        PcmAudioCodecConfig::kSampleRate / PcmAudioCodecConfig::kPacketFrames;
    const int streams = 1 + receivers;
    const int cushionPackets = streams * (256 * kLosslessPacketsPerSecond / 1000);
    QCOMPARE(cushionPackets, receivers == 0 ? 64 : 320);

    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    QString offer;
    QString answer;
    wireExchange(offerer, answerer, &offer, &answer);
    QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
    QSignalSpy answerReady(&answerer, &IMediaTransport::ready);
    IMediaTransport::StartOptions answerOptions{IMediaTransport::Role::Answerer, kTestAudioSsrc};
    answerOptions.receiverAudioSsrcs = receiverSsrcs;
    IMediaTransport::StartOptions offerOptions{IMediaTransport::Role::Offerer, kTestAudioSsrc};
    offerOptions.receiverAudioSsrcs = receiverSsrcs;
    QVERIFY(answerer.start(answerOptions));
    QVERIFY(offerer.start(offerOptions));
    QTRY_COMPARE_WITH_TIMEOUT(offerReady.count(), 1, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(answerReady.count(), 1, 10000);

    QList<QByteArray> received;
    connect(&answerer, &IMediaTransport::rtpReceived, this,
            [&received](const QByteArray& packet) { received.append(packet); });

    // From here the event loop does not run, so nothing drains: every packet
    // waits in the receive queue. Streams take turns, as the Core sends them.
    const int packets = cushionPackets + (overflow ? 1 : 0);
    QList<QByteArray> sent;
    quint64 bytes = 0;
    constexpr int kChunk = 32;
    for (int index = 0; index < packets; ++index) {
        const quint32 ssrc = index % streams == 0 ? kTestAudioSsrc
                                                  : receiverSsrcs.at(index % streams - 1);
        const QByteArray packet = rtpPacket(static_cast<quint16>(index), 15, ssrc);
        QVERIFY(offerer.sendRtp(packet));
        sent << packet;
        bytes += static_cast<quint64>(packet.size());
        // Small bursts, each fully received before the next, so the host's
        // UDP socket buffer never decides what is lost.
        if ((index + 1) % kChunk == 0 || index + 1 == packets) {
            QVERIFY(waitForReceivedRtpBytes(answerer, bytes));
        }
    }
    QVERIFY(received.isEmpty());
    // The first drain then finds the oldest packet older than the 80 ms
    // timing threshold and says so once, with the packets it dropped.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression(QStringLiteral(
                             "^media RTP timing: .* batchPackets=%1 droppedPending=%2$")
                                                .arg(cushionPackets)
                                                .arg(overflow ? 1 : 0)));
    QTRY_COMPARE_WITH_TIMEOUT(received.size(), cushionPackets, 5000);
    QTest::qWait(20);
    QCOMPARE(received.size(), cushionPackets);
    QCOMPARE(received, sent.mid(overflow ? 1 : 0));
    offerer.stop();
    answerer.stop();
}

// R-R3-43: a receiver SSRC of zero, the main SSRC, a repeat or more than
// four receivers is a precondition refusal: false, no error, not started.
void TestMediaTransport::receiverStreamPreconditionsRefuseSilently()
{
    const QList<QList<quint32>> refused{
        {0x11U, 0U},
        {0x11U, kTestAudioSsrc},
        {0x11U, 0x22U, 0x11U},
        {0x11U, 0x22U, 0x33U, 0x44U, 0x55U},
    };
    for (const QList<quint32>& receivers : refused) {
        for (const IMediaTransport::Role role :
             {IMediaTransport::Role::Offerer, IMediaTransport::Role::Answerer}) {
            LibDataChannelMediaTransport transport;
            QSignalSpy errors(&transport, &IMediaTransport::errorOccurred);
            QSignalSpy descriptions(&transport, &IMediaTransport::localDescription);
            IMediaTransport::StartOptions options{role, kTestAudioSsrc};
            options.receiverAudioSsrcs = receivers;
            QVERIFY(!transport.start(options));
            QCOMPARE(errors.size(), 0);
            QVERIFY(!transport.telemetry().has_value());
            QTest::qWait(10);
            QCOMPARE(descriptions.size(), 0);
        }
    }
    // Four distinct receivers are accepted.
    LibDataChannelMediaTransport transport;
    IMediaTransport::StartOptions options{IMediaTransport::Role::Answerer, kTestAudioSsrc};
    options.receiverAudioSsrcs = kTestReceiverSsrcs;
    QVERIFY(transport.start(options));
    transport.stop();
}

// R-R3-45: the headphones mix is one more a=ssrc line on the same audio
// line, after the receiver streams, and crosses intact beside them. Only a
// headphones SSRC distinct from the main and receiver SSRCs is accepted.
void TestMediaTransport::headphonesMixIsDeclaredLastAndCrossesBesideTheOthers()
{
    constexpr quint32 kHeadphones = 0x4e523355U;
    for (const quint32 bad : {kTestAudioSsrc, kTestReceiverSsrcs.at(2)}) {
        LibDataChannelMediaTransport transport;
        QSignalSpy errors(&transport, &IMediaTransport::errorOccurred);
        IMediaTransport::StartOptions options{IMediaTransport::Role::Offerer, kTestAudioSsrc};
        options.receiverAudioSsrcs = kTestReceiverSsrcs;
        options.headphonesAudioSsrc = bad;
        QVERIFY(!transport.start(options));
        QCOMPARE(errors.size(), 0);
    }

    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    QString offer;
    QString answer;
    wireExchange(offerer, answerer, &offer, &answer);
    QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
    QSignalSpy answerReady(&answerer, &IMediaTransport::ready);
    QSignalSpy answerErrors(&answerer, &IMediaTransport::errorOccurred);
    IMediaTransport::StartOptions answerOptions{IMediaTransport::Role::Answerer, kTestAudioSsrc};
    answerOptions.receiverAudioSsrcs = kTestReceiverSsrcs;
    answerOptions.headphonesAudioSsrc = kHeadphones;
    IMediaTransport::StartOptions offerOptions{IMediaTransport::Role::Offerer, kTestAudioSsrc};
    offerOptions.receiverAudioSsrcs = kTestReceiverSsrcs;
    offerOptions.headphonesAudioSsrc = kHeadphones;
    QVERIFY(answerer.start(answerOptions));
    QVERIFY(offerer.start(offerOptions));
    QTRY_COMPARE_WITH_TIMEOUT(offerReady.count(), 1, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(answerReady.count(), 1, 10000);

    QStringList expected{QStringLiteral("a=ssrc:%1 cname:nereus-mixed-stereo").arg(kTestAudioSsrc)};
    for (qsizetype index = 0; index < kTestReceiverSsrcs.size(); ++index) {
        expected << QStringLiteral("a=ssrc:%1 cname:nereus-receiver-%2")
                        .arg(kTestReceiverSsrcs.at(index)).arg(index);
    }
    expected << QStringLiteral("a=ssrc:%1 cname:nereus-headphones-mix").arg(kHeadphones);
    QCOMPARE(audioSsrcLines(offer), expected);
    QCOMPARE(offer.count(QStringLiteral("m=audio")), 1);

    QList<QByteArray> received;
    connect(&answerer, &IMediaTransport::rtpReceived, this,
            [&received](const QByteArray& packet) { received.append(packet); });
    const QList<QByteArray> sent{rtpPacket(1, 15, kTestAudioSsrc),
                                 rtpPacket(2, 15, kHeadphones),
                                 rtpPacket(3, 15, kTestReceiverSsrcs.at(0)),
                                 rtpPacket(4, 15, kHeadphones)};
    for (const QByteArray& packet : sent) {
        QVERIFY(offerer.sendRtp(packet));
    }
    QVERIFY(!offerer.sendRtp(rtpPacket(5, 15, kHeadphones + 1)));
    QTRY_COMPARE_WITH_TIMEOUT(received.size(), sent.size(), 5000);
    QCOMPARE(received, sent);
    QCOMPARE(answerErrors.count(), 0);
    offerer.stop();
    answerer.stop();
}

// Task 36: older peers get exactly today's offer (golden). A peer that asked
// for the microphone line gets one more audio section, mid "mic",
// receive-only at the Core, carrying Opus with the line's parameters and no
// SSRC of the Core's; everything else in the offer is today's. The answer
// takes the line send-only and declares the microphone's SSRC on it.
void TestMediaTransport::micLineIsOfferedOnlyWhenAsked()
{
    const QStringList golden = todaysOfferGolden(kTestAudioSsrc);
    QStringList answers[2];
    for (const bool mic : {false, true}) {
        LibDataChannelMediaTransport offerer;
        LibDataChannelMediaTransport answerer;
        QString offer;
        QString answer;
        wireExchange(offerer, answerer, &offer, &answer);
        QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
        QSignalSpy answerReady(&answerer, &IMediaTransport::ready);
        QSignalSpy answerErrors(&answerer, &IMediaTransport::errorOccurred);
        QSignalSpy offerErrors(&offerer, &IMediaTransport::errorOccurred);
        IMediaTransport::StartOptions answerOptions{IMediaTransport::Role::Answerer,
                                                    kTestAudioSsrc};
        IMediaTransport::StartOptions offerOptions{IMediaTransport::Role::Offerer,
                                                   kTestAudioSsrc};
        if (mic) {
            answerOptions.micAudioSsrc = kTestMicSsrc;
            offerOptions.micAudioSsrc = kTestMicSsrc;
        }
        QVERIFY(answerer.start(answerOptions));
        QVERIFY(offerer.start(offerOptions));
        QTRY_COMPARE_WITH_TIMEOUT(offerReady.count(), 1, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(answerReady.count(), 1, 10000);
        QCOMPARE(answerErrors.count(), 0);
        QCOMPARE(offerErrors.count(), 0);

        if (!mic) {
            QCOMPARE(stableDescriptionLines(offer), golden);
            QVERIFY(mediaSectionLines(offer, QStringLiteral("mic")).isEmpty());
            QVERIFY(mediaSectionLines(answer, QStringLiteral("mic")).isEmpty());
        } else {
            QCOMPARE(offer.count(QStringLiteral("m=audio")), 2);
            const QStringList micOffer = mediaSectionLines(offer, QStringLiteral("mic"));
            QVERIFY2(!micOffer.isEmpty(), qPrintable(offer));
            QCOMPARE(micOffer.constFirst(), QStringLiteral("m=audio 9 UDP/TLS/RTP/SAVPF 111"));
            QVERIFY(micOffer.contains(QStringLiteral("a=recvonly")));
            QVERIFY(micOffer.contains(QStringLiteral("a=rtpmap:111 opus/48000/2")));
            QVERIFY(micOffer.contains(QStringLiteral(
                "a=fmtp:111 minptime=10;useinbandfec=1;stereo=0;maxaveragebitrate=48000")));
            for (const QString& line : micOffer) {
                QVERIFY2(!line.startsWith(QLatin1String("a=ssrc:")), qPrintable(line));
                QVERIFY2(!line.contains(QLatin1String("L16")), qPrintable(line));
            }
            // Everything else is today's offer.
            QCOMPARE(stableDescriptionLines(withoutMicSection(offer)), golden);

            const QStringList micAnswer = mediaSectionLines(answer, QStringLiteral("mic"));
            QVERIFY2(!micAnswer.isEmpty(), qPrintable(answer));
            QVERIFY(micAnswer.contains(QStringLiteral("a=sendonly")));
            QVERIFY(micAnswer.contains(
                QStringLiteral("a=ssrc:%1 cname:nereus-microphone").arg(kTestMicSsrc)));
        }
        answers[mic ? 1 : 0] = stableDescriptionLines(withoutMicSection(answer));
        offerer.stop();
        answerer.stop();
    }
    // The answer's other sections do not change with the line.
    QCOMPARE(answers[1], answers[0]);
}

// TX stall lane: the owner thread drains received packets on a 2 ms timer,
// so a packet that arrives while it is busy waits. Each microphone packet
// reports that wait (from the library's callback to the report): never
// more than the time since it was sent, and most of a 150 ms stall of the
// owner thread when one comes between its receipt and its report.
void TestMediaTransport::micPacketsReportTheirWaitForTheOwnerThread()
{
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    QString offer;
    QString answer;
    wireExchange(offerer, answerer, &offer, &answer);
    QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
    QSignalSpy answerReady(&answerer, &IMediaTransport::ready);
    IMediaTransport::StartOptions answerOptions{IMediaTransport::Role::Answerer, kTestAudioSsrc};
    answerOptions.micAudioSsrc = kTestMicSsrc;
    IMediaTransport::StartOptions offerOptions{IMediaTransport::Role::Offerer, kTestAudioSsrc};
    offerOptions.micAudioSsrc = kTestMicSsrc;
    QVERIFY(answerer.start(answerOptions));
    QVERIFY(offerer.start(offerOptions));
    QTRY_COMPARE_WITH_TIMEOUT(offerReady.count(), 1, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(answerReady.count(), 1, 10000);

    QElapsedTimer sinceSent;
    QList<qint64> heldUs;
    QList<qint64> sinceSentUs;
    connect(&offerer, &IMediaTransport::micRtpReceived, this,
            [&heldUs, &sinceSentUs, &sinceSent](const QByteArray&, qint64 held) {
                heldUs.append(held);
                sinceSentUs.append(sinceSent.nsecsElapsed() / 1000);
            });

    constexpr auto kStall = std::chrono::milliseconds(150);
    constexpr int kAttempts = 3;
    qint64 longestHeldUs = 0;
    for (int attempt = 0; attempt < kAttempts; ++attempt) {
        const int before = static_cast<int>(heldUs.size());
        sinceSent.start();
        QVERIFY(answerer.sendMicRtp(rtpPacket(static_cast<quint16>(attempt + 1), 40,
                                              kTestMicSsrc)));
        // The owner thread (this one) is busy: nothing is drained.
        std::this_thread::sleep_for(kStall);
        QTRY_COMPARE_WITH_TIMEOUT(static_cast<int>(heldUs.size()), before + 1, 5000);
        const qint64 held = heldUs.at(before);
        QVERIFY2(held >= 0, qPrintable(QString::number(held)));
        QVERIFY2(held <= sinceSentUs.at(before),
                 qPrintable(QStringLiteral("held %1 us, sent %2 us ago")
                                .arg(held).arg(sinceSentUs.at(before))));
        longestHeldUs = std::max(longestHeldUs, held);
    }
    qInfo().noquote() << QStringLiteral("owner stall of %1 ms: the longest reported wait %2 ms")
                             .arg(kStall.count())
                             .arg(static_cast<double>(longestHeldUs) / 1000.0, 0, 'f', 1);
    QVERIFY2(longestHeldUs >= 100'000, qPrintable(QString::number(longestHeldUs)));
}

namespace {

// TX diagnostics lane: the "media RTP timing (microphone)" warnings, with
// the name of the thread each came from.
std::mutex g_micTimingLock;
QStringList g_micTimingMessages;
QStringList g_micTimingThreads;
QtMessageHandler g_previousHandler = nullptr;

void captureMicTiming(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (message.startsWith(QStringLiteral("media RTP timing (microphone)"))) {
        const std::lock_guard lock(g_micTimingLock);
        g_micTimingMessages << message;
        g_micTimingThreads << QThread::currentThread()->objectName();
        return;
    }
    if (g_previousHandler != nullptr) {
        g_previousHandler(type, context, message);
    }
}

int micTimingCount()
{
    const std::lock_guard lock(g_micTimingLock);
    return static_cast<int>(g_micTimingMessages.size());
}

} // namespace

// TX diagnostics lane: with a sink installed the line's packets bypass the
// owner's drain, and with it the owner's "media RTP timing" warning. The
// line's own thread warns instead, from "NereusMicRx". Two packets 1.1 s
// apart: past the once-a-second limit, under the idle bound, so the second
// warns whatever the lane's other timing (MicLineTimingWatch's replayed
// test covers the pacing).
void TestMediaTransport::micLineWarnsOfALongGapFromItsOwnThread()
{
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    QString offer;
    QString answer;
    wireExchange(offerer, answerer, &offer, &answer);
    QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
    QSignalSpy answerReady(&answerer, &IMediaTransport::ready);
    IMediaTransport::StartOptions answerOptions{IMediaTransport::Role::Answerer, kTestAudioSsrc};
    answerOptions.micAudioSsrc = kTestMicSsrc;
    IMediaTransport::StartOptions offerOptions{IMediaTransport::Role::Offerer, kTestAudioSsrc};
    offerOptions.micAudioSsrc = kTestMicSsrc;
    QVERIFY(answerer.start(answerOptions));
    QVERIFY(offerer.start(offerOptions));
    QTRY_COMPARE_WITH_TIMEOUT(offerReady.count(), 1, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(answerReady.count(), 1, 10000);

    std::atomic<int> delivered{0};
    QVERIFY(offerer.setMicPacketSink(
        [&delivered](const QByteArray&, qint64) { delivered.fetch_add(1); }));
    {
        const std::lock_guard lock(g_micTimingLock);
        g_micTimingMessages.clear();
        g_micTimingThreads.clear();
    }
    g_previousHandler = qInstallMessageHandler(captureMicTiming);
    struct RestoreHandler {
        ~RestoreHandler() { qInstallMessageHandler(g_previousHandler); }
    } restore;

    QVERIFY(answerer.sendMicRtp(rtpPacket(1, 40, kTestMicSsrc)));
    QTRY_COMPARE_WITH_TIMEOUT(delivered.load(), 1, 5000);
    QTest::qWait(1100);
    QVERIFY(answerer.sendMicRtp(rtpPacket(2, 40, kTestMicSsrc)));
    QTRY_COMPARE_WITH_TIMEOUT(delivered.load(), 2, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(micTimingCount() >= 1, 5000);

    QStringList messages;
    QStringList threads;
    {
        const std::lock_guard lock(g_micTimingLock);
        messages = g_micTimingMessages;
        threads = g_micTimingThreads;
    }
    qInfo().noquote() << QStringLiteral("mic line warnings: ") + messages.join(QStringLiteral(" | "));
    QVERIFY(messages.size() <= 2);
    for (const QString& thread : std::as_const(threads)) {
        QCOMPARE(thread, QStringLiteral("NereusMicRx"));
    }
    const QRegularExpressionMatch gap =
        QRegularExpression(QStringLiteral("callbackGapMs=(\\d+) ")).match(messages.last());
    QVERIFY2(gap.hasMatch(), qPrintable(messages.last()));
    QVERIFY2(gap.captured(1).toInt() >= 1000, qPrintable(messages.last()));
}

// TX diagnostics lane: the line's warning, replayed. The phone's 20 ms
// pacing never warns; a line idle between keys (5 s) resuming does not
// warn and leaves the once-a-second slot alone, so a 300 ms gap half a
// second later warns; a second gap inside that second does not.
void TestMediaTransport::micLineTimingTakesAnIdleGapAsTheLineStarting()
{
    using Clock = MicLineTimingWatch::Clock;
    using std::chrono::milliseconds;
    MicLineTimingWatch watch;
    const Clock::time_point origin = Clock::now();
    const auto at = [origin](qint64 ms) { return origin + milliseconds(ms); };
    // One packet a batch, taken 1 ms after its receipt.
    const auto packet = [&watch, &at](qint64 ms) {
        watch.beginBatch();
        watch.noteReceipt(at(ms));
        return watch.endBatch(at(ms), at(ms + 1), 1);
    };

    qint64 ms = 0;
    for (; ms <= 1000; ms += 20) {
        QVERIFY2(!packet(ms), qPrintable(QStringLiteral("warned at %1 ms").arg(ms)));
    }
    // Unkey; the line is idle for 5 s; the next key's packets.
    ms += 5000;
    QVERIFY(!packet(ms));
    for (int k = 0; k < 10; ++k) {
        ms += 20;
        QVERIFY(!packet(ms));
    }
    // A 300 ms gap, inside the second after the line started again.
    ms += 300;
    const std::optional<MicLineTimingWatch::Warning> warned = packet(ms);
    QVERIFY(warned.has_value());
    QCOMPARE(warned->callbackGapMs, qint64(300));
    QCOMPARE(warned->laneWaitMs, qint64(1));
    QCOMPARE(warned->batchPackets, 1);
    // Another inside the same second: held back by the limit.
    ms += 200;
    QVERIFY(!packet(ms));
    // A late lane, a second on: warns for the wait.
    ms += 1000;
    watch.beginBatch();
    watch.noteReceipt(at(ms));
    watch.noteReceipt(at(ms + 20));
    const std::optional<MicLineTimingWatch::Warning> late =
        watch.endBatch(at(ms), at(ms + 120), 2);
    QVERIFY(late.has_value());
    QCOMPARE(late->laneWaitMs, qint64(120));
    QCOMPARE(late->batchPackets, 2);
}

namespace {

// Speech-like test audio, as tst_remote_mic_receiver's: a 300 ms word of
// tone, then 200 ms of near silence.
float micSpeechSample(qint64 n)
{
    constexpr qint64 kWordFrames = 48 * 300;
    constexpr qint64 kCycleFrames = 48 * 500;
    if (n % kCycleFrames < kWordFrames) {
        return 0.3f * static_cast<float>(
            std::sin(2.0 * std::numbers::pi * 1000.0 * static_cast<double>(n) / 48000.0));
    }
    quint32 h = static_cast<quint32>(n) * 2654435761U;
    h ^= h >> 15;
    h *= 2246822519U;
    h ^= h >> 13;
    return 3.0e-4f * (static_cast<float>(h & 0xffffU) / 32768.0f - 1.0f);
}

struct StalledOwnerRun {
    int underruns{-1};
    bool sinkSet{false};
    int sent{0};
    int delivered{0};
    int ownerReports{0};
    QString sinkThread;
    double ownerWaitMaxMs{0.0};
    int postRefused{0};
    int sendRefused{0};
    bool ownerExposed{false};
};

} // namespace

// Owner-isolation fixture: one pausable sample clock coordinates the phone's
// 20 ms packets and the feed's 64-frame pulls. Source/TestPhone scheduling
// pauses that clock; real transport delivery remains independent. The owner
// stalls seven times while the source and pump continue, so only owner-routed
// delivery runs the buffer dry. Independent source starvation and radio clocks
// remain separate receiver/latency test contracts.
void TestMediaTransport::aStalledOwnerLeavesTheMicrophoneLineWhole()
{
    constexpr int kFrames = RemoteMicConfig::kOpusFrameSamples;
    constexpr int kBlock = RemoteMicConfig::kPumpBlockFrames;
    constexpr int kWarmupMs = 1000;
    const std::vector<int> stallsMs{80, 120, 200, 300, 420, 90, 150};
    constexpr int kBetweenStallsMs = 500;
    // Repeat a two-second clip until the consumer stops. The owner can
    // spend longer in qWait under load, so its nominal elapsed time cannot
    // determine when the microphone stops sending.
    constexpr int packets = 100;
    std::vector<QByteArray> encoded;
    {
        RemoteMicEncoder encoder;
        QVERIFY(encoder.isReady());
        std::vector<float> frame(kFrames);
        for (int k = 0; k < packets; ++k) {
            for (int i = 0; i < kFrames; ++i) {
                frame[static_cast<size_t>(i)] =
                    micSpeechSample(static_cast<qint64>(k) * kFrames + i);
            }
            const QByteArray packet = encoder.encode(
                frame.data(), static_cast<quint16>(k + 1),
                static_cast<quint32>(k * kFrames), kTestMicSsrc);
            QVERIFY(packet.size() >= OpusAudioCodecConfig::kRtpHeaderBytes);
            encoded.push_back(packet);
        }
    }

    const auto runOnce = [&](bool lineThread) {
        StalledOwnerRun run;
        RemoteMicFeed feed;
        RemoteMicReceiver receiver(&feed);
        if (!receiver.start(kTestMicSsrc, false)) {
            return run;
        }
        feed.setInUse(true);

        // The phone: its transport lives on a thread of its own.
        QThread phoneThread;
        phoneThread.setObjectName(QStringLiteral("TestPhone"));
        phoneThread.start();
        QObject phoneContext;
        phoneContext.moveToThread(&phoneThread);
        std::unique_ptr<LibDataChannelMediaTransport> answerer;
        const auto onPhone = [&phoneContext](auto work) {
            QMetaObject::invokeMethod(&phoneContext, work, Qt::BlockingQueuedConnection);
        };
        onPhone([&answerer]() { answerer = std::make_unique<LibDataChannelMediaTransport>(); });

        LibDataChannelMediaTransport offerer;
        wire(offerer, *answerer);
        QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
        QSignalSpy offerErrors(&offerer, &IMediaTransport::errorOccurred);
        std::atomic<bool> answerReady{false};
        connect(answerer.get(), &IMediaTransport::ready, &phoneContext,
                [&answerReady]() { answerReady.store(true); });
        bool answerStarted = false;
        onPhone([&]() {
            IMediaTransport::StartOptions options{IMediaTransport::Role::Answerer,
                                                  kTestAudioSsrc};
            options.micAudioSsrc = kTestMicSsrc;
            answerStarted = answerer->start(options);
        });
        IMediaTransport::StartOptions offerOptions{IMediaTransport::Role::Offerer,
                                                   kTestAudioSsrc};
        offerOptions.micAudioSsrc = kTestMicSsrc;
        const bool offerStarted = answerStarted && offerer.start(offerOptions);
        QElapsedTimer waitReady;
        waitReady.start();
        while (offerStarted && (offerReady.count() == 0 || !answerReady.load())
               && waitReady.elapsed() < 10000) {
            QTest::qWait(10);
        }

        std::atomic<int> sent{0};
        std::atomic<int> delivered{0};
        std::mutex sinkThreadLock;
        if (lineThread) {
            run.sinkSet = offerer.setMicPacketSink(
                [&receiver, &delivered, &run, &sinkThreadLock](const QByteArray& packet,
                                                                qint64 heldUs) {
                    if (delivered.fetch_add(1) == 0) {
                        const std::lock_guard lock(sinkThreadLock);
                        run.sinkThread = QThread::currentThread()->objectName();
                    }
                    receiver.submit(packet, heldUs);
                });
        }
        connect(&offerer, &IMediaTransport::micRtpReceived, this,
                [&receiver, &run](const QByteArray& packet, qint64 heldUs) {
                    ++run.ownerReports;
                    // Without a sink the owner delivers, as before.
                    if (!run.sinkSet) {
                        receiver.submit(packet, heldUs);
                    }
                });

        if (offerStarted && offerReady.count() == 1 && answerReady.load()) {
            using Clock = std::chrono::steady_clock;
            std::atomic<bool> stop{false};
            // A source pause must freeze the sample clock before it consumes
            // the previous packet's lead. Wait only for TestPhone's send call,
            // never for receipt, decode, fill or owner processing.
            std::atomic<int> underrunsAtEnd{-1};
            std::atomic<qint64> pumpBlocks{0};
            std::thread coordinator([&]() {
                std::vector<float> out(kBlock);
                Clock::time_point deadline = Clock::now();
                quint64 packetIndex = 0;
                for (qint64 tick = 0; !stop.load(); ++tick) {
                    std::this_thread::sleep_until(deadline);
                    if (stop.load()) {
                        break;
                    }
                    const auto interval = ((tick + 1) * kBlock * 1000000 / 48000)
                                        - (tick * kBlock * 1000000 / 48000);
                    const Clock::time_point wake = Clock::now();
                    // Preserve ordinary sub-tick wake jitter. After a missed
                    // whole tick, rebase instead of replaying overdue pulls.
                    Clock::time_point base = wake - deadline >= std::chrono::microseconds(interval)
                                                ? wake : deadline;
                    if (tick % (kFrames / kBlock) == 0) {
                        const Clock::time_point workStart = Clock::now();
                        QByteArray packet = encoded[static_cast<size_t>(packetIndex % packets)];
                        // Reusing the audio must not restart the RTP stream.
                        qToBigEndian<quint16>(static_cast<quint16>(packetIndex + 1), packet.data() + 2);
                        qToBigEndian<quint32>(static_cast<quint32>(packetIndex * kFrames), packet.data() + 4);
                        const bool posted = QMetaObject::invokeMethod(
                            &phoneContext, [&]() {
                                const bool accepted = answerer->sendMicRtp(packet);
                                sent.fetch_add(accepted ? 1 : 0);
                                run.sendRefused += accepted ? 0 : 1;
                            }, Qt::BlockingQueuedConnection);
                        run.postRefused += posted ? 0 : 1;
                        base += Clock::now() - workStart;
                        ++packetIndex;
                    }
                    // Pull through startup and underflow as the ordinary pump
                    // does. No decoded-supply condition gates clock progress.
                    feed.pullBlock(out.data(), kBlock, -1.0);
                    pumpBlocks.fetch_add(1);
                    deadline = base + std::chrono::microseconds(interval);
                }
            });

            // The owner thread: works, then stalls.
            QTest::qWait(kWarmupMs);
            const int underrunsAtWarmup = feed.stats().underflows;
            for (const int stall : stallsMs) {
                const int sentBefore = sent.load();
                const qint64 pulledBefore = pumpBlocks.load();
                std::this_thread::sleep_for(std::chrono::milliseconds(stall));
                // Prove both sides did more work than the maximum buffer can
                // hold during at least one owner stall, independent of fill.
                run.ownerExposed = run.ownerExposed
                    || ((sent.load() - sentBefore) * kFrames > RemoteMicConfig::kMaxDepthFrames
                        && (pumpBlocks.load() - pulledBefore) * kBlock > RemoteMicConfig::kMaxDepthFrames);
                QTest::qWait(kBetweenStallsMs);
            }
            underrunsAtEnd.store(feed.stats().underflows - underrunsAtWarmup);
            stop.store(true);
            coordinator.join();
            run.underruns = underrunsAtEnd.load();
            // What the phone sent reaches the Core (a packet or two may
            // still be in flight).
            QTest::qWait(100);
            run.ownerWaitMaxMs = feed.stats().ownerWaitMaxMs;
        }
        run.delivered = delivered.load();
        offerer.stop();
        onPhone([&run, &sent]() { run.sent = sent.load(); });
        onPhone([&answerer]() { answerer.reset(); });
        phoneThread.quit();
        phoneThread.wait();
        if (offerErrors.count() != 0) {
            run.underruns = -1;
        }
        return run;
    };

    const StalledOwnerRun owner = runOnce(false);
    const StalledOwnerRun line = runOnce(true);
    qInfo().noquote()
        << QStringLiteral("seven owner stalls of 80-420 ms: delivered by the owner, %1 "
                          "underruns (longest wait %2 ms); by the line's thread, %3 underruns "
                          "(longest wait %4 ms, %5 packets on \"%6\")")
               .arg(owner.underruns)
               .arg(owner.ownerWaitMaxMs, 0, 'f', 1)
               .arg(line.underruns)
               .arg(line.ownerWaitMaxMs, 0, 'f', 1)
               .arg(line.delivered)
               .arg(line.sinkThread);
    QCOMPARE(owner.postRefused, 0);
    QCOMPARE(owner.sendRefused, 0);
    QCOMPARE(line.postRefused, 0);
    QCOMPARE(line.sendRefused, 0);
    QVERIFY(owner.ownerExposed);
    QVERIFY(line.ownerExposed);
    // Delivered by the owner: the stalls reach the buffer and run it dry.
    QVERIFY2(owner.underruns >= 1, qPrintable(QString::number(owner.underruns)));
    QVERIFY(owner.ownerWaitMaxMs >= 80.0);
    // Delivered by the line's own thread: the line stays whole, every
    // packet comes by that thread, and nothing reaches the owner's signal.
    QCOMPARE(line.underruns, 0);
    QVERIFY(line.sinkSet);
    QCOMPARE(line.sinkThread, QStringLiteral("NereusMicRx"));
    QVERIFY2(line.sent > 0 && line.delivered == line.sent,
             qPrintable(QStringLiteral("%1 of %2").arg(line.delivered).arg(line.sent)));
    QCOMPARE(line.ownerReports, 0);
}

// Task 36: the answerer's microphone crosses to the offerer on the
// microphone line alone, and the Core's audio still crosses the other way on
// the main line alone. Each side's send filter takes only its own line's
// SSRC.
void TestMediaTransport::micLineCarriesTheAnswerersMicrophoneAlone()
{
    LibDataChannelMediaTransport offerer;
    LibDataChannelMediaTransport answerer;
    QString offer;
    QString answer;
    wireExchange(offerer, answerer, &offer, &answer);
    QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
    QSignalSpy answerReady(&answerer, &IMediaTransport::ready);
    QSignalSpy offerErrors(&offerer, &IMediaTransport::errorOccurred);
    QSignalSpy answerErrors(&answerer, &IMediaTransport::errorOccurred);
    IMediaTransport::StartOptions answerOptions{IMediaTransport::Role::Answerer, kTestAudioSsrc};
    answerOptions.micAudioSsrc = kTestMicSsrc;
    IMediaTransport::StartOptions offerOptions{IMediaTransport::Role::Offerer, kTestAudioSsrc};
    offerOptions.micAudioSsrc = kTestMicSsrc;
    QVERIFY(answerer.start(answerOptions));
    QVERIFY(offerer.start(offerOptions));
    QTRY_COMPARE_WITH_TIMEOUT(offerReady.count(), 1, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(answerReady.count(), 1, 10000);

    QList<QByteArray> offererMain;
    QList<QByteArray> offererMic;
    QList<QByteArray> answererMain;
    QList<QByteArray> answererMic;
    connect(&offerer, &IMediaTransport::rtpReceived, this,
            [&offererMain](const QByteArray& packet) { offererMain.append(packet); });
    connect(&offerer, &IMediaTransport::micRtpReceived, this,
            [&offererMic](const QByteArray& packet) { offererMic.append(packet); });
    connect(&answerer, &IMediaTransport::rtpReceived, this,
            [&answererMain](const QByteArray& packet) { answererMain.append(packet); });
    connect(&answerer, &IMediaTransport::micRtpReceived, this,
            [&answererMic](const QByteArray& packet) { answererMic.append(packet); });

    QList<QByteArray> micSent;
    for (quint16 sequence = 1; sequence <= 20; ++sequence) {
        micSent << rtpPacket(sequence, 40, kTestMicSsrc);
    }
    for (const QByteArray& packet : micSent) {
        QVERIFY(answerer.sendMicRtp(packet));
    }
    const QList<QByteArray> mainSent{rtpPacket(100, 30, kTestAudioSsrc),
                                     rtpPacket(101, 30, kTestAudioSsrc)};
    for (const QByteArray& packet : mainSent) {
        QVERIFY(offerer.sendRtp(packet));
    }
    // Each line's filter: the wrong SSRC, or the wrong side, is refused
    // before the library.
    QVERIFY(!answerer.sendMicRtp(rtpPacket(21, 40, kTestMicSsrc + 1)));
    QVERIFY(!answerer.sendMicRtp(rtpPacket(22, 40, kTestAudioSsrc)));
    QVERIFY(!answerer.sendRtp(rtpPacket(23, 40, kTestMicSsrc)));
    QVERIFY(!offerer.sendMicRtp(rtpPacket(24, 40, kTestMicSsrc)));
    QVERIFY(!offerer.sendRtp(rtpPacket(25, 40, kTestMicSsrc)));

    QTRY_COMPARE_WITH_TIMEOUT(offererMic.size(), micSent.size(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(answererMain.size(), mainSent.size(), 5000);
    QTest::qWait(50);
    QCOMPARE(offererMic, micSent);
    QCOMPARE(answererMain, mainSent);
    QVERIFY(offererMain.isEmpty());
    QVERIFY(answererMic.isEmpty());
    QCOMPARE(offerErrors.count(), 0);
    QCOMPARE(answerErrors.count(), 0);
    offerer.stop();
    answerer.stop();
}

// Task 36: with the lossless profile offered (as for the main line), the
// microphone line carries the L16 rtpmap too, both ends agree it, and an L16
// microphone packet crosses intact. Without it the line is Opus only.
void TestMediaTransport::micLineCarriesLosslessWhenOffered()
{
    for (const bool lossless : {false, true}) {
        LibDataChannelMediaTransport offerer;
        LibDataChannelMediaTransport answerer;
        QString offer;
        QString answer;
        wireExchange(offerer, answerer, &offer, &answer);
        QSignalSpy offerReady(&offerer, &IMediaTransport::ready);
        QSignalSpy answerReady(&answerer, &IMediaTransport::ready);
        IMediaTransport::StartOptions answerOptions{IMediaTransport::Role::Answerer,
                                                    kTestAudioSsrc};
        answerOptions.micAudioSsrc = kTestMicSsrc;
        IMediaTransport::StartOptions offerOptions{IMediaTransport::Role::Offerer,
                                                   kTestAudioSsrc};
        offerOptions.micAudioSsrc = kTestMicSsrc;
        offerOptions.offerLosslessAudio = lossless;
        QVERIFY(answerer.start(answerOptions));
        QVERIFY(offerer.start(offerOptions));
        QTRY_COMPARE_WITH_TIMEOUT(offerReady.count(), 1, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(answerReady.count(), 1, 10000);

        const QStringList micOffer = mediaSectionLines(offer, QStringLiteral("mic"));
        QVERIFY2(!micOffer.isEmpty(), qPrintable(offer));
        QCOMPARE(micOffer.contains(QStringLiteral("a=rtpmap:96 L16/48000/2")), lossless);
        QCOMPARE(micOffer.constFirst(),
                 lossless ? QStringLiteral("m=audio 9 UDP/TLS/RTP/SAVPF 111 96")
                          : QStringLiteral("m=audio 9 UDP/TLS/RTP/SAVPF 111"));
        QCOMPARE(offerer.micLosslessNegotiated(), lossless);
        QCOMPARE(answerer.micLosslessNegotiated(), lossless);
        QCOMPARE(offerer.losslessAudioNegotiated(), lossless);

        if (lossless) {
            QList<QByteArray> received;
            connect(&offerer, &IMediaTransport::micRtpReceived, this,
                    [&received](const QByteArray& packet) { received.append(packet); });
            const QByteArray l16 = PcmAudioPacketiser{}.encode(
                QVector<float>(PcmAudioCodecConfig::kPacketFrames * 2, 0.25f), 7, 1344,
                kTestMicSsrc).packet;
            QVERIFY(answerer.sendMicRtp(l16));
            QTRY_COMPARE_WITH_TIMEOUT(received.size(), 1, 5000);
            QCOMPARE(received.constFirst(), l16);
        }
        offerer.stop();
        answerer.stop();
        QVERIFY(!offerer.micLosslessNegotiated());
        QVERIFY(!answerer.micLosslessNegotiated());
    }
}

// Task 36: a microphone SSRC equal to the main, a receiver or the headphones
// SSRC is a precondition refusal: false, no error, not started.
void TestMediaTransport::micSsrcPreconditionsRefuseSilently()
{
    constexpr quint32 kHeadphones = 0x4e523355U;
    for (const quint32 bad : {kTestAudioSsrc, kTestReceiverSsrcs.at(1), kHeadphones}) {
        for (const IMediaTransport::Role role :
             {IMediaTransport::Role::Offerer, IMediaTransport::Role::Answerer}) {
            LibDataChannelMediaTransport transport;
            QSignalSpy errors(&transport, &IMediaTransport::errorOccurred);
            IMediaTransport::StartOptions options{role, kTestAudioSsrc};
            options.receiverAudioSsrcs = kTestReceiverSsrcs;
            options.headphonesAudioSsrc = kHeadphones;
            options.micAudioSsrc = bad;
            QVERIFY(!transport.start(options));
            QCOMPARE(errors.size(), 0);
            QVERIFY(!transport.telemetry().has_value());
        }
    }
    LibDataChannelMediaTransport transport;
    IMediaTransport::StartOptions options{IMediaTransport::Role::Offerer, kTestAudioSsrc};
    options.micAudioSsrc = kTestMicSsrc;
    QVERIFY(transport.start(options));
    transport.stop();
}

QTEST_GUILESS_MAIN(TestMediaTransport)
#include "tst_media_transport.moc"
