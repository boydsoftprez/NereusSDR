// =================================================================
// tests/tst_media_peer.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R3 Task 1.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: iPhone app plan Task 28 (R-IOS-16): the ICE settings of a
//               session through the remote access service reach the media
//               transport's start. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-10-01: TX mic thread fix round 2: a microphone sink's rejection
//               posted before stop() or a restart is not reported after it.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/media/MediaPeer.h"
#include "fakes/DataChannelStartupEvidence.h"

#include <QCryptographicHash>
#include <QPointer>
#include <QSet>
#include <QSignalSpy>
#include <QThread>
#include <QUuid>
#include <QtTest>

#include <stdexcept>
#include <thread>

using namespace NereusSDR;

namespace {

constexpr char kConnectionA[] = "11111111-2222-4333-8444-555555555555";
constexpr char kConnectionB[] = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee";

QJsonObject descriptionControl(const QString& connectionId,
                               const QString& sdp = QStringLiteral("v=0"),
                               const QString& type = QStringLiteral("offer"))
{
    return {
        {QStringLiteral("op"), QStringLiteral("description")},
        {QStringLiteral("connectionId"), connectionId},
        {QStringLiteral("sdp"), sdp},
        {QStringLiteral("type"), type},
    };
}

QJsonObject candidateControl(const QString& connectionId,
                             const QString& candidate,
                             const QString& mid = QStringLiteral("0"))
{
    return {
        {QStringLiteral("op"), QStringLiteral("candidate")},
        {QStringLiteral("connectionId"), connectionId},
        {QStringLiteral("candidate"), candidate},
        {QStringLiteral("mid"), mid},
    };
}

// Load findings 4: both real peers' ready within the same 10 s as before;
// when it does not come, the failure says which stage the ICE, DTLS and
// SCTP handshakes reached (libdatachannel's own stage lines, captured for
// this wait), not only that it timed out.
QString waitForBothReady(const QSignalSpy& offerReady, const QSignalSpy& answerReady,
                         const NereusSDR::Test::DataChannelStartupEvidence& evidence)
{
    constexpr int kReadyBoundMs = 10000;
    if (QTest::qWaitFor([&]() { return offerReady.size() >= 1 && answerReady.size() >= 1; },
                        kReadyBoundMs)) {
        return {};
    }
    return QStringLiteral("not ready within %1 ms (offerer %2, answerer %3), last stage %4\n%5")
        .arg(kReadyBoundMs).arg(offerReady.size()).arg(answerReady.size())
        .arg(evidence.lastLibraryStage(), evidence.diagnostic());
}

QByteArray rtpPacket(quint16 sequence, quint32 ssrc)
{
    QByteArray packet(15, '\0');
    packet[0] = char(0x80);
    packet[1] = char(111);
    packet[2] = char(sequence >> 8);
    packet[3] = char(sequence & 0xff);
    packet[8] = char(ssrc >> 24);
    packet[9] = char(ssrc >> 16);
    packet[10] = char(ssrc >> 8);
    packet[11] = char(ssrc);
    packet[12] = char(0xf8);
    packet[13] = char(0xff);
    packet[14] = char(0xfe);
    return packet;
}

class FakeTransport : public IMediaTransport {
public:
    explicit FakeTransport(QObject* parent = nullptr) : IMediaTransport(parent) {}

    bool start(const StartOptions& options) override
    {
        started = startSucceeds;
        startedRole = options.role;
        startedSsrc = options.localAudioSsrc;
        startedReceiverSsrcs = options.receiverAudioSsrcs;
        startedHeadphonesSsrc = options.headphonesAudioSsrc;
        startedIce = options.ice;
        return started;
    }

    void stop() override
    {
        started = false;
        readyState = false;
    }

    bool acceptDescription(const QString& sdp, const QString& type) override
    {
        descriptions.push_back({sdp, type});
        return acceptDescriptionSucceeds;
    }

    bool acceptCandidate(const QString& candidate, const QString& mid) override
    {
        candidates.push_back({candidate, mid});
        return acceptCandidateSucceeds;
    }

    bool sendDisplay(const QByteArray& message) override
    {
        sentDisplay.push_back(message);
        return readyState;
    }

    bool sendRtp(const QByteArray& packet) override
    {
        sentRtp.push_back(packet);
        return readyState;
    }

    bool isReady() const override { return readyState; }

    bool setMicPacketSink(MicPacketSink sink) override
    {
        micSink = std::move(sink);
        return true;
    }

    void fireLocalDescription(const QString& sdp, const QString& type)
    {
        emit localDescription(sdp, type);
    }

    void fireLocalCandidate(const QString& candidate, const QString& mid)
    {
        emit localCandidate(candidate, mid);
    }

    void fireDisplay(const QByteArray& message) { emit displayReceived(message); }
    void fireRtp(const QByteArray& packet) { emit rtpReceived(packet); }
    void fireConnectionFailed(const QString& reason) { emit connectionFailed(reason); }
    void fireGenericError(const QString& reason) { emit errorOccurred(reason); }

    void fireReady()
    {
        readyState = true;
        emit ready();
    }

    bool startSucceeds = true;
    bool acceptDescriptionSucceeds = true;
    bool acceptCandidateSucceeds = true;
    bool started = false;
    bool readyState = false;
    Role startedRole = Role::Answerer;
    quint32 startedSsrc = 0;
    QList<quint32> startedReceiverSsrcs;
    quint32 startedHeadphonesSsrc = 0;
    std::optional<IceConfiguration> startedIce;
    QList<QPair<QString, QString>> descriptions;
    QList<QPair<QString, QString>> candidates;
    QList<QByteArray> sentDisplay;
    QList<QByteArray> sentRtp;
    MicPacketSink micSink;
};

class ObservableFakeTransport final : public FakeTransport {
public:
    using FakeTransport::FakeTransport;

    std::optional<MediaTransportTelemetry> telemetry() const override
    {
        return observedTelemetry;
    }

    std::optional<MediaTransportTelemetry> observedTelemetry;
};

} // namespace

class TestMediaPeer : public QObject {
    Q_OBJECT

private slots:
    void strictControlBuffersCandidatesAndEnforcesCap();
    void outboundControlIsScopedAndBounded();
    void oldQueuedCallbacksCannotEnterNewGeneration();
    void terminalConnectionFailureIsTypedAndGenerationScoped();
    void telemetryDefaultsUnsupportedAndRejectsStaleGeneration();
    void signalHandlersMayRestartOrDeletePeer();
    void realPeersExchangeQueuedControlAndDirectMedia();
    void startRefusalsAreTyped();
    void receiverSsrcsAreDerivedAndDistinct();
    void receiverStreamsFollowTheStartOption();
    void headphonesMixFollowsTheStartOption();
    void iceSettingsReachTheTransport();
    void realPeersCarryDeclaredReceiverStreams();
    void aMicSinkRejectionIsReportedOnlyForItsOwnStart();
};

void TestMediaPeer::terminalConnectionFailureIsTypedAndGenerationScoped()
{
    QList<QPointer<FakeTransport>> transports;
    MediaPeer peer(nullptr, [&transports](QObject* parent) -> IMediaTransport* {
        auto* transport = new FakeTransport(parent);
        transports.push_back(transport);
        return transport;
    });
    QSignalSpy failures(&peer, &MediaPeer::connectionFailed);
    QSignalSpy errors(&peer, &MediaPeer::errorOccurred);

    QVERIFY(peer.start(IMediaTransport::Role::Answerer, QLatin1String(kConnectionA)));
    QVERIFY(transports.constLast());
    transports.constLast()->fireGenericError(QStringLiteral("invalid media packet"));
    QCOMPARE(errors.size(), 1);
    QCOMPARE(failures.size(), 0);

    transports.constLast()->fireConnectionFailed(QStringLiteral("media peer connection failed"));
    QCOMPARE(failures.size(), 1);
    QCOMPARE(failures.constFirst().constFirst().toString(),
             QStringLiteral("media peer connection failed"));
    QCOMPARE(errors.size(), 1);

    QPointer<FakeTransport> stale = transports.constLast();
    peer.stop();
    QVERIFY(peer.start(IMediaTransport::Role::Answerer, QLatin1String(kConnectionB)));
    QVERIFY(stale);
    stale->fireConnectionFailed(QStringLiteral("stale peer failed"));
    QCOMPARE(failures.size(), 1);
}

void TestMediaPeer::telemetryDefaultsUnsupportedAndRejectsStaleGeneration()
{
    QPointer<FakeTransport> unsupported;
    MediaPeer unsupportedPeer(
        nullptr, [&unsupported](QObject* parent) -> IMediaTransport* {
            unsupported = new FakeTransport(parent);
            return unsupported;
        });
    QVERIFY(unsupportedPeer.start(IMediaTransport::Role::Answerer,
                                  QLatin1String(kConnectionA)));
    QVERIFY(unsupported);
    QVERIFY(!unsupportedPeer.telemetry().has_value());

    QList<QPointer<ObservableFakeTransport>> transports;
    MediaPeer peer(nullptr, [&transports](QObject* parent) -> IMediaTransport* {
        auto* transport = new ObservableFakeTransport(parent);
        transports.push_back(transport);
        return transport;
    });

    QVERIFY(peer.start(IMediaTransport::Role::Answerer,
                       QLatin1String(kConnectionA)));
    ObservableFakeTransport* first = transports.constLast();
    QVERIFY(!peer.telemetry().has_value());
    first->observedTelemetry = MediaTransportTelemetry{11, 12, 13, 14};
    const auto firstSnapshot = peer.telemetry();
    QVERIFY(firstSnapshot.has_value());
    QVERIFY(firstSnapshot->generation != 0);
    QCOMPARE(firstSnapshot->traffic.receivedDisplayPayloadBytes, quint64(11));
    QCOMPARE(firstSnapshot->traffic.submittedDisplayPayloadBytes, quint64(12));
    QCOMPARE(firstSnapshot->traffic.receivedRtpBytes, quint64(13));
    QCOMPARE(firstSnapshot->traffic.submittedRtpBytes, quint64(14));

    peer.stop();
    QVERIFY(!peer.telemetry().has_value());
    QVERIFY(peer.start(IMediaTransport::Role::Answerer,
                       QLatin1String(kConnectionB)));
    ObservableFakeTransport* second = transports.constLast();
    QVERIFY(second != first);
    first->observedTelemetry = MediaTransportTelemetry{91, 92, 93, 94};
    QVERIFY(!peer.telemetry().has_value());

    second->observedTelemetry = MediaTransportTelemetry{21, 22, 23, 24};
    const auto secondSnapshot = peer.telemetry();
    QVERIFY(secondSnapshot.has_value());
    QVERIFY(secondSnapshot->generation != firstSnapshot->generation);
    QCOMPARE(secondSnapshot->traffic.receivedDisplayPayloadBytes, quint64(21));
    QCOMPARE(secondSnapshot->traffic.submittedDisplayPayloadBytes, quint64(22));
    QCOMPARE(secondSnapshot->traffic.receivedRtpBytes, quint64(23));
    QCOMPARE(secondSnapshot->traffic.submittedRtpBytes, quint64(24));
}

void TestMediaPeer::strictControlBuffersCandidatesAndEnforcesCap()
{
    QPointer<FakeTransport> transport;
    MediaPeer peer(nullptr, [&transport](QObject* parent) -> IMediaTransport* {
        transport = new FakeTransport(parent);
        return transport;
    });

    QVERIFY(!peer.start(IMediaTransport::Role::Answerer,
                        QStringLiteral("not-a-uuid")));
    QVERIFY(peer.start(IMediaTransport::Role::Answerer,
                       QLatin1String(kConnectionA)));
    QCOMPARE(peer.connectionId(), QLatin1String(kConnectionA));
    QCOMPARE(peer.audioSsrc(), quint32(0xf46d8502U));
    QVERIFY(transport);
    QCOMPARE(transport->startedSsrc, peer.audioSsrc());

    QVERIFY(!peer.acceptControl(candidateControl(
        QLatin1String(kConnectionB), QStringLiteral("stale"))));
    QJsonObject unknown{
        {QStringLiteral("op"), QStringLiteral("modelMutation")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionA)},
    };
    QVERIFY(!peer.acceptControl(unknown));
    QJsonObject extra = descriptionControl(QLatin1String(kConnectionA));
    extra.insert(QStringLiteral("model"), QStringLiteral("forbidden"));
    QVERIFY(!peer.acceptControl(extra));
    QVERIFY(!peer.acceptControl(descriptionControl(
        QLatin1String(kConnectionA), QStringLiteral("v=0"),
        QStringLiteral("answer"))));
    QVERIFY(!peer.acceptControl(descriptionControl(
        QLatin1String(kConnectionA),
        QString(IMediaTransport::kMaxDescriptionBytes + 1, QLatin1Char('x')))));

    QVERIFY(peer.acceptControl(candidateControl(
        QLatin1String(kConnectionA), QStringLiteral("candidate-0"))));
    QVERIFY(peer.acceptControl(candidateControl(
        QLatin1String(kConnectionA), QStringLiteral("candidate-1"))));
    QCOMPARE(transport->candidates.size(), 0);

    QVERIFY(peer.acceptControl(descriptionControl(QLatin1String(kConnectionA))));
    QCOMPARE(transport->descriptions.size(), 1);
    QCOMPARE(transport->candidates.size(), 2);
    QCOMPARE(transport->candidates.at(0).first, QStringLiteral("candidate-0"));
    QCOMPARE(transport->candidates.at(1).first, QStringLiteral("candidate-1"));
    QVERIFY(!peer.acceptControl(descriptionControl(QLatin1String(kConnectionA))));

    for (int i = 2; i < IMediaTransport::kMaxRemoteCandidates; ++i) {
        QVERIFY(peer.acceptControl(candidateControl(
            QLatin1String(kConnectionA), QStringLiteral("candidate-%1").arg(i))));
    }
    QCOMPARE(transport->candidates.size(), IMediaTransport::kMaxRemoteCandidates);
    QVERIFY(!peer.acceptControl(candidateControl(
        QLatin1String(kConnectionA), QStringLiteral("candidate-over-cap"))));
}

void TestMediaPeer::outboundControlIsScopedAndBounded()
{
    QPointer<FakeTransport> transport;
    MediaPeer peer(nullptr, [&transport](QObject* parent) -> IMediaTransport* {
        transport = new FakeTransport(parent);
        return transport;
    });
    QSignalSpy controls(&peer, &MediaPeer::controlReady);
    QSignalSpy errors(&peer, &MediaPeer::errorOccurred);
    QVERIFY(peer.start(IMediaTransport::Role::Offerer,
                       QLatin1String(kConnectionA)));

    transport->fireLocalDescription(QStringLiteral("v=0\r\n"),
                                    QStringLiteral("offer"));
    transport->fireLocalCandidate(QStringLiteral("host-candidate"),
                                  QStringLiteral("audio"));
    QCOMPARE(controls.size(), 2);
    const QJsonObject description = controls.at(0).at(0).toJsonObject();
    QCOMPARE(description.size(), 4);
    QCOMPARE(description.value(QStringLiteral("op")).toString(),
             QStringLiteral("description"));
    QCOMPARE(description.value(QStringLiteral("connectionId")).toString(),
             QLatin1String(kConnectionA));
    const QJsonObject candidate = controls.at(1).at(0).toJsonObject();
    QCOMPARE(candidate.size(), 4);
    QCOMPARE(candidate.value(QStringLiteral("op")).toString(),
             QStringLiteral("candidate"));
    QCOMPARE(candidate.value(QStringLiteral("connectionId")).toString(),
             QLatin1String(kConnectionA));

    transport->fireLocalDescription(
        QString(IMediaTransport::kMaxDescriptionBytes + 1, QLatin1Char('x')),
        QStringLiteral("offer"));
    transport->fireLocalCandidate(
        QString(IMediaTransport::kMaxCandidateBytes + 1, QLatin1Char('x')),
        QStringLiteral("audio"));
    transport->fireLocalDescription(QStringLiteral("v=0"),
                                    QStringLiteral("answer"));
    QCOMPARE(controls.size(), 2);
    QCOMPARE(errors.size(), 3);
}

void TestMediaPeer::oldQueuedCallbacksCannotEnterNewGeneration()
{
    QList<QPointer<FakeTransport>> transports;
    MediaPeer peer(nullptr, [&transports](QObject* parent) -> IMediaTransport* {
        auto* transport = new FakeTransport(parent);
        transports.push_back(transport);
        return transport;
    });
    QSignalSpy controls(&peer, &MediaPeer::controlReady);
    QSignalSpy displays(&peer, &MediaPeer::displayReceived);
    QSignalSpy ready(&peer, &MediaPeer::ready);
    bool callbackOnOwnerThread = false;
    connect(&peer, &MediaPeer::controlReady, &peer,
            [&peer, &callbackOnOwnerThread](const QJsonObject&) {
                callbackOnOwnerThread = QThread::currentThread() == peer.thread();
            });
    QVERIFY(peer.start(IMediaTransport::Role::Offerer,
                       QLatin1String(kConnectionA)));
    const quint32 firstSsrc = peer.audioSsrc();
    FakeTransport* oldTransport = transports.at(0);

    std::thread worker([oldTransport] {
        oldTransport->fireLocalDescription(QStringLiteral("v=0"),
                                           QStringLiteral("offer"));
        oldTransport->fireDisplay(QByteArrayLiteral("old-display"));
        oldTransport->fireReady();
    });
    worker.join();

    peer.stop();
    QVERIFY(peer.start(IMediaTransport::Role::Offerer,
                       QLatin1String(kConnectionB)));
    QCOMPARE(peer.audioSsrc(), quint32(0xd2a6ade1U));
    QVERIFY(peer.audioSsrc() != firstSsrc);
    QCoreApplication::processEvents();
    QCOMPARE(controls.size(), 0);
    QCOMPARE(displays.size(), 0);
    QCOMPARE(ready.size(), 0);

    FakeTransport* currentTransport = transports.at(1);
    std::thread currentWorker([currentTransport] {
        currentTransport->fireLocalDescription(QStringLiteral("v=0"),
                                               QStringLiteral("offer"));
    });
    currentWorker.join();
    QTRY_COMPARE_WITH_TIMEOUT(controls.size(), 1, 5000);
    QVERIFY(callbackOnOwnerThread);
    currentTransport->fireReady();
    QCOMPARE(controls.at(0).at(0).toJsonObject()
                 .value(QStringLiteral("connectionId")).toString(),
             QLatin1String(kConnectionB));
    QCOMPARE(ready.size(), 1);
    QVERIFY(peer.isReady());

    const QByteArray display("new-display");
    const QByteArray rtp = rtpPacket(12, peer.audioSsrc());
    QVERIFY(peer.sendDisplay(display));
    QVERIFY(peer.sendRtp(rtp));
    QVERIFY(!peer.sendRtp(rtpPacket(13, peer.audioSsrc() + 1)));
    QVERIFY(!peer.sendDisplay(QByteArray(
        IMediaTransport::kMaxDisplayMessageBytes + 1, 'x')));
    QVERIFY(!peer.sendRtp(QByteArray(
        IMediaTransport::kMaxRawRtpBytes + 1, 'x')));
    QCOMPARE(currentTransport->sentDisplay, QList<QByteArray>{display});
    QCOMPARE(currentTransport->sentRtp, QList<QByteArray>{rtp});
}

void TestMediaPeer::signalHandlersMayRestartOrDeletePeer()
{
    QList<QPointer<FakeTransport>> transports;
    MediaPeer peer(nullptr, [&transports](QObject* parent) -> IMediaTransport* {
        auto* transport = new FakeTransport(parent);
        transports.push_back(transport);
        return transport;
    });
    QVERIFY(peer.start(IMediaTransport::Role::Offerer,
                       QLatin1String(kConnectionA)));
    connect(&peer, &MediaPeer::controlReady, &peer,
            [&peer](const QJsonObject&) {
                peer.stop();
                QVERIFY(peer.start(IMediaTransport::Role::Offerer,
                                   QLatin1String(kConnectionB)));
            });
    transports.at(0)->fireLocalDescription(QStringLiteral("v=0"),
                                           QStringLiteral("offer"));
    QCOMPARE(peer.connectionId(), QLatin1String(kConnectionB));

    QPointer<MediaPeer> deletedPeer;
    QPointer<FakeTransport> deletedTransport;
    deletedPeer = new MediaPeer(
        nullptr, [&deletedTransport](QObject* parent) -> IMediaTransport* {
            deletedTransport = new FakeTransport(parent);
            return deletedTransport;
        });
    QVERIFY(deletedPeer->start(IMediaTransport::Role::Offerer,
                               QLatin1String(kConnectionA)));
    connect(deletedPeer, &MediaPeer::controlReady, deletedPeer,
            [&deletedPeer](const QJsonObject&) { delete deletedPeer.data(); });
    deletedTransport->fireLocalDescription(QStringLiteral("v=0"),
                                           QStringLiteral("offer"));
    QVERIFY(deletedPeer.isNull());
}

void TestMediaPeer::realPeersExchangeQueuedControlAndDirectMedia()
{
    MediaPeer offerer;
    MediaPeer answerer;
    bool controlRejected = false;
    connect(&offerer, &MediaPeer::controlReady, &answerer,
            [&answerer, &controlRejected](const QJsonObject& control) {
                if (!answerer.acceptControl(control)) {
                    controlRejected = true;
                }
            }, Qt::QueuedConnection);
    connect(&answerer, &MediaPeer::controlReady, &offerer,
            [&offerer, &controlRejected](const QJsonObject& control) {
                if (!offerer.acceptControl(control)) {
                    controlRejected = true;
                }
            }, Qt::QueuedConnection);

    QSignalSpy offerReady(&offerer, &MediaPeer::ready);
    QSignalSpy answerReady(&answerer, &MediaPeer::ready);
    QSignalSpy offerControls(&offerer, &MediaPeer::controlReady);
    QSignalSpy displayReceived(&answerer, &MediaPeer::displayReceived);
    QSignalSpy rtpReceived(&answerer, &MediaPeer::rtpReceived);
    const NereusSDR::Test::DataChannelStartupEvidence evidence;
    QVERIFY(answerer.start(IMediaTransport::Role::Answerer,
                           QLatin1String(kConnectionA)));
    QVERIFY(offerer.start(IMediaTransport::Role::Offerer,
                          QLatin1String(kConnectionA)));
    const QString notReady = waitForBothReady(offerReady, answerReady, evidence);
    QVERIFY2(notReady.isEmpty(), qPrintable(notReady));
    QCOMPARE(offerReady.size(), 1);
    QCOMPARE(answerReady.size(), 1);
    QVERIFY(!controlRejected);

    const QByteArray display("media-peer-display");
    QCOMPARE(offerer.audioSsrc(), answerer.audioSsrc());
    QVERIFY(offerer.audioSsrc() != 0);
    bool foundExpectedSsrc = false;
    for (const QList<QVariant>& arguments : offerControls) {
        const QJsonObject control = arguments.at(0).toJsonObject();
        if (control.value(QStringLiteral("op")).toString()
            == QStringLiteral("description")) {
            foundExpectedSsrc = control.value(QStringLiteral("sdp")).toString()
                .contains(QStringLiteral("a=ssrc:%1").arg(offerer.audioSsrc()));
        }
    }
    QVERIFY(foundExpectedSsrc);
    const QByteArray rtp = rtpPacket(13, offerer.audioSsrc());
    QVERIFY(offerer.sendDisplay(display));
    QVERIFY(offerer.sendRtp(rtp));
    QTRY_COMPARE_WITH_TIMEOUT(displayReceived.size(), 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(rtpReceived.size(), 1, 5000);
    QCOMPARE(displayReceived.at(0).at(0).toByteArray(), display);
    QCOMPARE(rtpReceived.at(0).at(0).toByteArray(), rtp);
    const auto offerTelemetry = offerer.telemetry();
    QVERIFY(offerTelemetry.has_value());
    QCOMPARE(offerTelemetry->traffic.submittedDisplayPayloadBytes,
             static_cast<quint64>(display.size()));
    QCOMPARE(offerTelemetry->traffic.submittedRtpBytes,
             static_cast<quint64>(rtp.size()));
    const auto answerTrafficArrived = [&answerer, &display, &rtp] {
        const auto snapshot = answerer.telemetry();
        return snapshot
            && snapshot->traffic.receivedDisplayPayloadBytes
                == static_cast<quint64>(display.size())
            && snapshot->traffic.receivedRtpBytes
                == static_cast<quint64>(rtp.size());
    };
    QTRY_VERIFY_WITH_TIMEOUT(answerTrafficArrived(), 5000);
}

// R-R3-28, amended 2026-09-23. Only a transport that could not be built is
// worth retrying; the controller reads which refusal it was from here.
void TestMediaPeer::startRefusalsAreTyped()
{
    using Refusal = MediaPeer::StartRefusal;
    QList<QPointer<FakeTransport>> transports;
    bool succeeds = true;
    MediaPeer peer(nullptr, [&](QObject* parent) -> IMediaTransport* {
        auto* transport = new FakeTransport(parent);
        transport->startSucceeds = succeeds;
        transports.push_back(transport);
        return transport;
    });
    QCOMPARE(peer.lastStartRefusal(), Refusal::None);

    // Preconditions: a non-canonical connection id, an already started peer.
    QVERIFY(!peer.start(IMediaTransport::Role::Answerer, QStringLiteral("not-an-id")));
    QCOMPARE(peer.lastStartRefusal(), Refusal::Precondition);
    QVERIFY(peer.start(IMediaTransport::Role::Answerer, QLatin1String(kConnectionA)));
    QCOMPARE(peer.lastStartRefusal(), Refusal::None);
    QVERIFY(!peer.start(IMediaTransport::Role::Answerer, QLatin1String(kConnectionB)));
    QCOMPARE(peer.lastStartRefusal(), Refusal::Precondition);
    peer.stop();

    // The transport refuses without reporting an error: a precondition of
    // its own, such as an SSRC of zero. Permanent.
    succeeds = false;
    QVERIFY(!peer.start(IMediaTransport::Role::Answerer, QLatin1String(kConnectionA)));
    QCOMPARE(peer.lastStartRefusal(), Refusal::TransportRefused);

    // A later successful start clears the record.
    succeeds = true;
    QVERIFY(peer.start(IMediaTransport::Role::Answerer, QLatin1String(kConnectionA)));
    QCOMPARE(peer.lastStartRefusal(), Refusal::None);
    peer.stop();

    // The transport reports an error and refuses: its peer could not be
    // built (LibDataChannelMediaTransport::start's catch). Transient.
    class BuildFailing final : public FakeTransport {
    public:
        using FakeTransport::FakeTransport;
        bool start(const StartOptions&) override
        {
            emit errorOccurred(QStringLiteral("could not create the peer connection"));
            return false;
        }
    };
    MediaPeer buildFailing(nullptr, [](QObject* parent) -> IMediaTransport* {
        return new BuildFailing(parent);
    });
    QSignalSpy buildErrors(&buildFailing, &MediaPeer::errorOccurred);
    QVERIFY(!buildFailing.start(IMediaTransport::Role::Answerer, QLatin1String(kConnectionA)));
    QCOMPARE(buildFailing.lastStartRefusal(), Refusal::TransportConstructionFailed);
    QCOMPARE(buildErrors.size(), 1);

    // The factory throws: the transport could not be built. Transient.
    MediaPeer throwing(nullptr, [](QObject*) -> IMediaTransport* {
        throw std::runtime_error("no transport");
    });
    QVERIFY(!throwing.start(IMediaTransport::Role::Answerer, QLatin1String(kConnectionA)));
    QCOMPARE(throwing.lastStartRefusal(), Refusal::TransportConstructionFailed);

    // The factory returns no transport. Permanent.
    MediaPeer empty(nullptr, [](QObject*) -> IMediaTransport* { return nullptr; });
    QVERIFY(!empty.start(IMediaTransport::Role::Answerer, QLatin1String(kConnectionA)));
    QCOMPARE(empty.lastStartRefusal(), Refusal::InvalidTransport);
}

// R-R3-43: receiver n's SSRC is the first four big-endian bytes of
// SHA-256("NereusSDR/media-receiver-ssrc/v1:<n>:" + connection id), distinct
// from the main SSRC, from each other and from zero, and the same on both
// peers.
void TestMediaPeer::receiverSsrcsAreDerivedAndDistinct()
{
    for (const char* connection : {kConnectionA, kConnectionB}) {
        const QString connectionId = QLatin1String(connection);
        const QList<quint32> ssrcs = MediaPeer::receiverAudioSsrcsForConnection(connectionId);
        QCOMPARE(ssrcs.size(), IMediaTransport::kMaxReceiverAudioStreams);
        QCOMPARE(MediaPeer::receiverAudioSsrcsForConnection(connectionId), ssrcs);

        MediaPeer peer(nullptr, [](QObject* parent) -> IMediaTransport* {
            return new FakeTransport(parent);
        });
        QVERIFY(peer.start(IMediaTransport::Role::Answerer, connectionId));
        QSet<quint32> distinct{peer.audioSsrc()};
        for (int receiver = 0; receiver < ssrcs.size(); ++receiver) {
            const QByteArray digest = QCryptographicHash::hash(
                QByteArray("NereusSDR/media-receiver-ssrc/v1:")
                    + QByteArray::number(receiver) + ':' + connectionId.toUtf8(),
                QCryptographicHash::Sha256);
            const quint32 derived = (quint32(quint8(digest[0])) << 24)
                | (quint32(quint8(digest[1])) << 16)
                | (quint32(quint8(digest[2])) << 8) | quint32(quint8(digest[3]));
            // No collision in these connections, so the digest stands as is.
            QCOMPARE(ssrcs.at(receiver), derived);
            QVERIFY(ssrcs.at(receiver) != 0);
            distinct.insert(ssrcs.at(receiver));
        }
        QCOMPARE(distinct.size(), 1 + IMediaTransport::kMaxReceiverAudioStreams);
    }
    QVERIFY(MediaPeer::receiverAudioSsrcsForConnection(QLatin1String(kConnectionA))
            != MediaPeer::receiverAudioSsrcsForConnection(QLatin1String(kConnectionB)));
}

// R-R3-43: without the option the transport is asked for nothing new and
// only the main SSRC is sent or accepted, as today. With it, the transport
// gets this connection's four receiver SSRCs and exactly that set passes
// both filters; anything else is refused and reported, as always.
void TestMediaPeer::receiverStreamsFollowTheStartOption()
{
    QList<QPointer<FakeTransport>> transports;
    MediaPeer peer(nullptr, [&transports](QObject* parent) -> IMediaTransport* {
        auto* transport = new FakeTransport(parent);
        transports.push_back(transport);
        return transport;
    });
    QSignalSpy received(&peer, &MediaPeer::rtpReceived);
    QSignalSpy errors(&peer, &MediaPeer::errorOccurred);
    const QList<quint32> receivers =
        MediaPeer::receiverAudioSsrcsForConnection(QLatin1String(kConnectionA));

    // Today.
    QVERIFY(peer.start(IMediaTransport::Role::Answerer, QLatin1String(kConnectionA)));
    FakeTransport* today = transports.constLast();
    QVERIFY(today->startedReceiverSsrcs.isEmpty());
    QVERIFY(peer.receiverAudioSsrcs().isEmpty());
    today->fireReady();
    QVERIFY(peer.sendRtp(rtpPacket(1, peer.audioSsrc())));
    QVERIFY(!peer.sendRtp(rtpPacket(2, receivers.at(0))));
    QCOMPARE(today->sentRtp.size(), 1);
    today->fireRtp(rtpPacket(3, receivers.at(0)));
    QCOMPARE(received.size(), 0);
    QCOMPARE(errors.size(), 1);
    QCOMPARE(errors.constLast().constFirst().toString(),
             QStringLiteral("invalid raw RTP packet rejected"));
    peer.stop();

    // Asked for.
    QVERIFY(peer.start(IMediaTransport::Role::Answerer, QLatin1String(kConnectionA),
                       IMediaTransport::kDefaultAudioTargetBitrate, false, true));
    FakeTransport* asked = transports.constLast();
    QCOMPARE(asked->startedSsrc, peer.audioSsrc());
    QCOMPARE(asked->startedReceiverSsrcs, receivers);
    QCOMPARE(peer.receiverAudioSsrcs(), receivers);
    asked->fireReady();
    QList<QByteArray> sent{rtpPacket(10, peer.audioSsrc())};
    for (const quint32 ssrc : receivers) {
        sent << rtpPacket(static_cast<quint16>(11 + sent.size()), ssrc);
    }
    for (const QByteArray& packet : sent) {
        QVERIFY(peer.sendRtp(packet));
        asked->fireRtp(packet);
    }
    QCOMPARE(asked->sentRtp, sent);
    QCOMPARE(received.size(), sent.size());
    for (qsizetype index = 0; index < sent.size(); ++index) {
        QCOMPARE(received.at(index).at(0).toByteArray(), sent.at(index));
    }
    QCOMPARE(errors.size(), 1);

    // Undeclared: the connection B receiver ids and a neighbour of the main id.
    const QList<quint32> undeclared{
        MediaPeer::receiverAudioSsrcsForConnection(QLatin1String(kConnectionB)).at(0),
        peer.audioSsrc() + 1,
    };
    for (const quint32 ssrc : undeclared) {
        QVERIFY(!receivers.contains(ssrc));
        QVERIFY(!peer.sendRtp(rtpPacket(30, ssrc)));
        asked->fireRtp(rtpPacket(31, ssrc));
    }
    QCOMPARE(asked->sentRtp.size(), sent.size());
    QCOMPARE(received.size(), sent.size());
    QCOMPARE(errors.size(), 1 + undeclared.size());
    QCOMPARE(errors.constLast().constFirst().toString(),
             QStringLiteral("invalid raw RTP packet rejected"));

    // Stopping forgets the set.
    peer.stop();
    QVERIFY(peer.receiverAudioSsrcs().isEmpty());
}

// R-R3-43 over real peers: when both ask, a receiver stream crosses beside
// the main one and the offer declares it. When the Core declares receiver
// streams but the GUI did not ask, the GUI refuses and reports them.
void TestMediaPeer::realPeersCarryDeclaredReceiverStreams()
{
    for (const bool answererAsks : {true, false}) {
        MediaPeer offerer;
        MediaPeer answerer;
        connect(&offerer, &MediaPeer::controlReady, &answerer,
                [&answerer](const QJsonObject& control) {
                    QVERIFY(answerer.acceptControl(control));
                }, Qt::QueuedConnection);
        connect(&answerer, &MediaPeer::controlReady, &offerer,
                [&offerer](const QJsonObject& control) {
                    QVERIFY(offerer.acceptControl(control));
                }, Qt::QueuedConnection);
        QSignalSpy offerReady(&offerer, &MediaPeer::ready);
        QSignalSpy answerReady(&answerer, &MediaPeer::ready);
        QSignalSpy offerControls(&offerer, &MediaPeer::controlReady);
        QSignalSpy rtpReceived(&answerer, &MediaPeer::rtpReceived);
        QSignalSpy answerErrors(&answerer, &MediaPeer::errorOccurred);
        const NereusSDR::Test::DataChannelStartupEvidence evidence;
        QVERIFY(answerer.start(IMediaTransport::Role::Answerer, QLatin1String(kConnectionA),
                               IMediaTransport::kDefaultAudioTargetBitrate, false,
                               answererAsks));
        QVERIFY(offerer.start(IMediaTransport::Role::Offerer, QLatin1String(kConnectionA),
                              IMediaTransport::kDefaultAudioTargetBitrate, false, true));
        const QString notReady = waitForBothReady(offerReady, answerReady, evidence);
        QVERIFY2(notReady.isEmpty(), qPrintable(notReady));
        QCOMPARE(offerReady.size(), 1);
        QCOMPARE(answerReady.size(), 1);

        const QList<quint32> receivers = offerer.receiverAudioSsrcs();
        QCOMPARE(receivers.size(), IMediaTransport::kMaxReceiverAudioStreams);
        QCOMPARE(answerer.receiverAudioSsrcs(),
                 answererAsks ? receivers : QList<quint32>{});
        QString offer;
        for (const QList<QVariant>& arguments : offerControls) {
            const QJsonObject control = arguments.at(0).toJsonObject();
            if (control.value(QStringLiteral("op")).toString() == QStringLiteral("description")) {
                offer = control.value(QStringLiteral("sdp")).toString();
            }
        }
        for (int receiver = 0; receiver < receivers.size(); ++receiver) {
            QVERIFY(offer.contains(QStringLiteral("a=ssrc:%1 cname:nereus-receiver-%2")
                                       .arg(receivers.at(receiver))
                                       .arg(receiver)));
        }

        const QByteArray main = rtpPacket(50, offerer.audioSsrc());
        const QByteArray second = rtpPacket(51, receivers.at(1));
        const QByteArray mainAfter = rtpPacket(52, offerer.audioSsrc());
        QVERIFY(offerer.sendRtp(main));
        QVERIFY(offerer.sendRtp(second));
        QVERIFY(offerer.sendRtp(mainAfter));
        if (answererAsks) {
            QTRY_COMPARE_WITH_TIMEOUT(rtpReceived.size(), 3, 5000);
            QCOMPARE(rtpReceived.at(1).at(0).toByteArray(), second);
            QCOMPARE(answerErrors.size(), 0);
        } else {
            QTRY_COMPARE_WITH_TIMEOUT(rtpReceived.size(), 2, 5000);
            QTRY_COMPARE_WITH_TIMEOUT(answerErrors.size(), 1, 5000);
            QCOMPARE(answerErrors.constFirst().constFirst().toString(),
                     QStringLiteral("invalid raw RTP packet rejected"));
            QCOMPARE(rtpReceived.at(1).at(0).toByteArray(), mainAfter);
        }
        QCOMPARE(rtpReceived.at(0).at(0).toByteArray(), main);
    }
}

// R-R3-45: the headphones mix SSRC is the first four big-endian bytes of
// SHA-256("NereusSDR/media-headphones-ssrc/v1:" + connection id), distinct
// from the main and every receiver SSRC. Without the start option the
// transport is asked for nothing and the id is refused both ways; with it,
// it passes both filters, receiver streams or not.
void TestMediaPeer::headphonesMixFollowsTheStartOption()
{
    for (const char* connection : {kConnectionA, kConnectionB}) {
        const QString connectionId = QLatin1String(connection);
        const quint32 headphones = MediaPeer::headphonesAudioSsrcForConnection(connectionId);
        const QByteArray digest = QCryptographicHash::hash(
            QByteArray("NereusSDR/media-headphones-ssrc/v1:") + connectionId.toUtf8(),
            QCryptographicHash::Sha256);
        const quint32 derived = (quint32(quint8(digest[0])) << 24)
            | (quint32(quint8(digest[1])) << 16)
            | (quint32(quint8(digest[2])) << 8) | quint32(quint8(digest[3]));
        QCOMPARE(headphones, derived);
        QVERIFY(headphones != 0);
        QVERIFY(!MediaPeer::receiverAudioSsrcsForConnection(connectionId).contains(headphones));
    }

    QList<QPointer<FakeTransport>> transports;
    MediaPeer peer(nullptr, [&transports](QObject* parent) -> IMediaTransport* {
        auto* transport = new FakeTransport(parent);
        transports.push_back(transport);
        return transport;
    });
    QSignalSpy received(&peer, &MediaPeer::rtpReceived);
    QSignalSpy errors(&peer, &MediaPeer::errorOccurred);
    const quint32 headphones =
        MediaPeer::headphonesAudioSsrcForConnection(QLatin1String(kConnectionA));

    // Today.
    QVERIFY(peer.start(IMediaTransport::Role::Answerer, QLatin1String(kConnectionA)));
    FakeTransport* today = transports.constLast();
    QCOMPARE(today->startedHeadphonesSsrc, quint32{0});
    QCOMPARE(peer.headphonesAudioSsrc(), quint32{0});
    today->fireReady();
    QVERIFY(!peer.sendRtp(rtpPacket(1, headphones)));
    today->fireRtp(rtpPacket(2, headphones));
    QCOMPARE(received.size(), 0);
    QCOMPARE(errors.size(), 1);
    peer.stop();

    // Asked for, without receiver streams.
    QVERIFY(peer.start(IMediaTransport::Role::Answerer, QLatin1String(kConnectionA),
                       IMediaTransport::kDefaultAudioTargetBitrate, false, false, true));
    FakeTransport* asked = transports.constLast();
    QCOMPARE(asked->startedHeadphonesSsrc, headphones);
    QVERIFY(asked->startedReceiverSsrcs.isEmpty());
    QCOMPARE(peer.headphonesAudioSsrc(), headphones);
    asked->fireReady();
    QVERIFY(peer.sendRtp(rtpPacket(3, headphones)));
    asked->fireRtp(rtpPacket(4, headphones));
    QCOMPARE(received.size(), 1);
    QCOMPARE(errors.size(), 1);
    peer.stop();
    QCOMPARE(peer.headphonesAudioSsrc(), quint32{0});
}

// iPhone app plan Task 28 (R-IOS-16): a session through the remote access
// service starts its media transport with the session's ICE settings (the
// same STUN server and relay); without them, host candidates only.
void TestMediaPeer::iceSettingsReachTheTransport()
{
    QList<QPointer<FakeTransport>> transports;
    MediaPeer peer(nullptr, [&transports](QObject* parent) -> IMediaTransport* {
        auto* transport = new FakeTransport(parent);
        transports.push_back(transport);
        return transport;
    });
    QVERIFY(!peer.usesIce());
    QVERIFY(peer.start(IMediaTransport::Role::Answerer, QLatin1String(kConnectionA)));
    QVERIFY(!transports.constLast()->startedIce.has_value());
    peer.stop();

    IceConfiguration ice = IceConfiguration::throughRendezvous(
        {QStringLiteral("stun:192.0.2.1:3478")}, true, AddressFamilies{}, HostFamilies{});
    RendezvousWire::Turn turn;
    turn.urls = {QStringLiteral("turn:192.0.2.1:3478?transport=udp")};
    turn.username = QStringLiteral("1800086400:aaaaaaaaaaaaaaaaaaaaaaaaaa");
    turn.password = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QCOMPARE(ice.setRelay(turn, 1), 1);
    // Task 29 step 2b: ICE with no server of its own (the media tunnel's
    // settings) gathers at once; the service's STUN and relay take longer.
    peer.setIceConfiguration(IceConfiguration::throughRendezvous(
        {}, false, AddressFamilies{}, HostFamilies{}));
    QVERIFY(peer.usesIce());
    QVERIFY(!peer.gathersFromServers());
    peer.setIceConfiguration(ice);
    QVERIFY(peer.usesIce());
    QVERIFY(peer.gathersFromServers());
    QVERIFY(peer.start(IMediaTransport::Role::Answerer, QLatin1String(kConnectionA)));
    const std::optional<IceConfiguration> started = transports.constLast()->startedIce;
    QVERIFY(started.has_value());
    QCOMPARE(started->stunServer()->host, QStringLiteral("192.0.2.1"));
    QCOMPARE(started->relayServers().size(), 1);
    QVERIFY(started->relayKnown());
    peer.stop();

    peer.setIceConfiguration(std::nullopt);
    QVERIFY(peer.start(IMediaTransport::Role::Answerer, QLatin1String(kConnectionA)));
    QVERIFY(!transports.constLast()->startedIce.has_value());
    peer.stop();
}

// TX mic thread fix round 2: the sink (on the transport's thread) refuses a
// packet off the line's SSRC and posts the report to the owner. Reported
// while its start is current; dropped when stop() or a restart came between.
void TestMediaPeer::aMicSinkRejectionIsReportedOnlyForItsOwnStart()
{
    QList<QPointer<FakeTransport>> transports;
    MediaPeer peer(nullptr, [&transports](QObject* parent) -> IMediaTransport* {
        auto* transport = new FakeTransport(parent);
        transports.push_back(transport);
        return transport;
    });
    QSignalSpy errors(&peer, &MediaPeer::errorOccurred);
    int delivered = 0;
    QVERIFY(peer.setMicPacketSink([&delivered](const QByteArray&, qint64) { ++delivered; }) == false);
    const auto startWithLine = [&peer]() {
        return peer.start(IMediaTransport::Role::Answerer, QLatin1String(kConnectionA),
                          IMediaTransport::kDefaultAudioTargetBitrate, false, false, false, true);
    };
    const QByteArray wrongSsrc = rtpPacket(1, 0x01020304u);
    const quint32 micSsrc = MediaPeer::micAudioSsrcForConnection(QLatin1String(kConnectionA));
    QVERIFY(micSsrc != 0x01020304u);

    // Current: delivered or reported.
    QVERIFY(startWithLine());
    QVERIFY(transports.constLast()->micSink);
    transports.constLast()->micSink(rtpPacket(1, micSsrc), 0);
    QCOMPARE(delivered, 1);
    transports.constLast()->micSink(wrongSsrc, 0);
    QCOMPARE(errors.size(), 0);   // posted, not emitted on the transport's thread
    QCoreApplication::processEvents();
    QCOMPARE(errors.size(), 1);

    // Posted before stop(): dropped.
    IMediaTransport::MicPacketSink stale = transports.constLast()->micSink;
    stale(wrongSsrc, 0);
    peer.stop();
    QCoreApplication::processEvents();
    QCOMPARE(errors.size(), 1);

    // Posted before a restart: dropped; the new start's own report is not.
    QVERIFY(startWithLine());
    IMediaTransport::MicPacketSink first = transports.constLast()->micSink;
    first(wrongSsrc, 0);
    peer.stop();
    QVERIFY(startWithLine());
    QCoreApplication::processEvents();
    QCOMPARE(errors.size(), 1);
    transports.constLast()->micSink(wrongSsrc, 0);
    QCoreApplication::processEvents();
    QCOMPARE(errors.size(), 2);
    peer.stop();
}

QTEST_GUILESS_MAIN(TestMediaPeer)
#include "tst_media_peer.moc"
