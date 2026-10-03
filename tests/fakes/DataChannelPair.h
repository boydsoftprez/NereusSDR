#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// tests/fakes/DataChannelPair.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 28 (R-IOS-16): two DataChannelTransports on this
// computer, joined directly (host candidates, no service): the device's
// Offerer and the Core's Answerer, the Answerer presenting the certificate
// it is given. What tst_data_channel_transport and the session conformance
// runner's data-channel mode run the control channel over.
//
// DataChannelBridge puts such a pair between a LoopbackTransport the
// conformance runner plays a client through and the StationServer, so every
// fixture runs over a real DTLS and SCTP connection unchanged: what the
// client's LoopbackTransport sends crosses the data channel, what the Core
// sends comes back the same way, and a close at either end closes the
// other. settled() says whether both ends have handled everything the other
// sent, which the runner waits for wherever it waits for the event loop.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-28: Load findings 2: waitForOpen() ends at a failure and
//               failureReasons() names it. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>

#include "core/session/DataChannelTransport.h"
#include "LoopbackTransport.h"

namespace NereusSDR::Test {

struct DataChannelPairStart {
    bool answererStarted = false;
    bool offererStarted = false;
};

struct DataChannelPairEvent {
    enum class Side { Offerer, Answerer };
    enum class Kind {
        DescriptionEmitted, DescriptionAccepted,
        CandidateEmitted, CandidateAccepted,
        GatheringComplete, Opened, Failed,
    };
    Side side;
    Kind kind;
    bool accepted = false; // meaningful only for DescriptionAccepted/CandidateAccepted
};

using DataChannelPairObserver = std::function<void(DataChannelPairEvent)>;

/// Joins `offerer` and `answerer` directly and starts both. False when
/// either refuses to start.
inline bool startDataChannelPair(DataChannelTransport* offerer, DataChannelTransport* answerer,
                                 quint64 offererCap, quint64 answererCap,
                                 const QString& certificatePemPath,
                                 const QString& privateKeyPemPath,
                                 DataChannelPairStart* result = nullptr,
                                 DataChannelPairObserver observer = {})
{
    QObject::connect(offerer, &DataChannelTransport::localDescription, answerer,
                     [answerer, observer](const QString& sdp, const QString& type) {
        if (observer) observer({DataChannelPairEvent::Side::Offerer,
                                DataChannelPairEvent::Kind::DescriptionEmitted});
        const bool accepted = answerer->acceptDescription(sdp, type);
        if (observer) observer({DataChannelPairEvent::Side::Answerer,
                                DataChannelPairEvent::Kind::DescriptionAccepted, accepted});
    });
    QObject::connect(answerer, &DataChannelTransport::localDescription, offerer,
                     [offerer, observer](const QString& sdp, const QString& type) {
        if (observer) observer({DataChannelPairEvent::Side::Answerer,
                                DataChannelPairEvent::Kind::DescriptionEmitted});
        const bool accepted = offerer->acceptDescription(sdp, type);
        if (observer) observer({DataChannelPairEvent::Side::Offerer,
                                DataChannelPairEvent::Kind::DescriptionAccepted, accepted});
    });
    QObject::connect(offerer, &DataChannelTransport::localCandidate, answerer,
                     [answerer, observer](const QString& candidate) {
        if (observer) observer({DataChannelPairEvent::Side::Offerer,
                                DataChannelPairEvent::Kind::CandidateEmitted});
        const bool accepted = answerer->acceptCandidate(candidate);
        if (observer) observer({DataChannelPairEvent::Side::Answerer,
                                DataChannelPairEvent::Kind::CandidateAccepted, accepted});
    });
    QObject::connect(answerer, &DataChannelTransport::localCandidate, offerer,
                     [offerer, observer](const QString& candidate) {
        if (observer) observer({DataChannelPairEvent::Side::Answerer,
                                DataChannelPairEvent::Kind::CandidateEmitted});
        const bool accepted = offerer->acceptCandidate(candidate);
        if (observer) observer({DataChannelPairEvent::Side::Offerer,
                                DataChannelPairEvent::Kind::CandidateAccepted, accepted});
    });
    if (observer) {
        const auto observeState = [observer](DataChannelTransport* transport,
                                            DataChannelPairEvent::Side side) {
            QObject::connect(transport, &DataChannelTransport::gatheringComplete, transport,
                             [observer, side] {
                observer({side, DataChannelPairEvent::Kind::GatheringComplete});
            });
            QObject::connect(transport, &DataChannelTransport::opened, transport,
                             [observer, side] { observer({side, DataChannelPairEvent::Kind::Opened}); });
            QObject::connect(transport, &DataChannelTransport::failed, transport,
                             [observer, side](const QString&) {
                observer({side, DataChannelPairEvent::Kind::Failed});
            });
        };
        observeState(offerer, DataChannelPairEvent::Side::Offerer);
        observeState(answerer, DataChannelPairEvent::Side::Answerer);
    }
    DataChannelTransport::Options answer;
    answer.role = DataChannelTransport::Role::Answerer;
    answer.maxIncomingBytes = answererCap;
    answer.certificatePemPath = certificatePemPath;
    answer.privateKeyPemPath = privateKeyPemPath;
    DataChannelTransport::Options offer;
    offer.role = DataChannelTransport::Role::Offerer;
    offer.maxIncomingBytes = offererCap;
    const bool answererStarted = answerer->start(answer);
    const bool offererStarted = answererStarted && offerer->start(offer);
    if (result) {
        result->answererStarted = answererStarted;
        result->offererStarted = offererStarted;
    }
    return answererStarted && offererStarted;
}

/// Both ends have handled everything the other sent: every message
/// delivered, every ping seen, every pong handled.
inline bool dataChannelPairSettled(const DataChannelTransport& a, const DataChannelTransport& b)
{
    const DataChannelTransport::Counts x = a.countsForTest();
    const DataChannelTransport::Counts y = b.countsForTest();
    return x.messagesSent == y.messagesDelivered && y.messagesSent == x.messagesDelivered
        && x.pingsSent == y.pingsReceived && y.pingsSent == x.pingsReceived
        && x.pongsSent == y.pongsReceived && y.pongsSent == x.pongsReceived;
}

/// Runs the event loop until `done` holds or `ms` pass.
template <typename Done>
bool waitFor(Done done, int ms)
{
    const QDeadlineTimer deadline(ms);
    while (!done() && !deadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return done();
}

class DataChannelBridge : public QObject {
public:
    /// `client` is the transport the runner plays; `server`'s end is handed
    /// to `accept` once the channel opens. The Answerer presents the
    /// certificate at the two paths.
    template <typename Accept>
    DataChannelBridge(LoopbackTransport* client, quint64 stationCap, quint64 clientCap,
                      const QString& certificatePemPath, const QString& privateKeyPemPath,
                      Accept accept, QObject* parent = nullptr,
                      DataChannelPairObserver observer = {})
        : QObject(parent)
        , m_client(client)
    {
        m_near = new LoopbackTransport(QStringLiteral("data-channel-bridge"), this);
        m_near->linkTo(client);
        m_offerer = new DataChannelTransport(this);
        m_answerer = new DataChannelTransport();
        // As a LoopbackTransport reports by default: no address of the
        // client's own (DataChannelTransport::setPeerAddressForTest()).
        m_answerer->setPeerAddressForTest(QString());
        QObject::connect(m_near, &SessionTransport::textReceived, this,
                         [this](const QByteArray& wire) { m_offerer->sendText(wire); });
        QObject::connect(m_offerer, &SessionTransport::textReceived, this,
                         [this](const QByteArray& wire) { m_near->sendText(wire); });
        QObject::connect(m_near, &SessionTransport::closed, this,
                         [this]() { m_offerer->closeLink(QStringLiteral("client closed")); });
        QObject::connect(m_offerer, &SessionTransport::closed, this, [this]() {
            m_near->closeLink(QStringLiteral("control channel closed"));
        });
        QObject::connect(m_offerer, &DataChannelTransport::failed, this, [this]() {
            m_near->closeLink(QStringLiteral("control channel failed"));
        });
        QObject::connect(m_offerer, &DataChannelTransport::opened, this,
                         [this]() { m_offererOpened = true; });
        QObject::connect(m_offerer, &DataChannelTransport::failed, this,
                         [this](const QString& reason) {
            m_offererFailed = true;
            m_offererReason = structuralReason(reason);
        });
        QObject::connect(m_answerer, &DataChannelTransport::opened, this, [this, accept]() {
            m_answererOpened = true;
            accept(m_answerer);
        });
        QObject::connect(m_answerer, &DataChannelTransport::failed, this,
                         [this](const QString& reason) {
            m_answererFailed = true;
            m_answererReason = structuralReason(reason);
        });
        m_started = startDataChannelPair(m_offerer, m_answerer, clientCap, stationCap,
                                         certificatePemPath, privateKeyPemPath, &m_start,
                                         std::move(observer));
    }

    ~DataChannelBridge() override
    {
        // Owned by the server once accepted; otherwise ours.
        if (m_answerer && m_answerer->parent() == nullptr) {
            delete m_answerer.data();
        }
    }

    bool started() const { return m_started; }
    /// Both ends opened (the station may have closed its end again at
    /// once, as it does a connection past its limit).
    bool opened() const { return m_offererOpened && m_answererOpened; }
    /// Either end reported a failure before or after opening.
    bool failed() const { return m_offererFailed || m_answererFailed; }

    /// Load findings 2: runs the event loop until both ends open, either
    /// end fails, or `ms` pass. A failure ends the wait at once, so it is
    /// reported with its reason rather than as a silent timeout.
    bool waitForOpen(int ms)
    {
        waitFor([this] { return opened() || failed(); }, ms);
        return opened();
    }

    /// Load findings 2: each failed end's reason, as the transport gave it
    /// when it is one of the transport's own sentences, else "library
    /// error" (a library message may carry anything).
    QString failureReasons() const
    {
        QStringList out;
        if (m_answererFailed) {
            out << QStringLiteral("answerer: %1").arg(m_answererReason);
        }
        if (m_offererFailed) {
            out << QStringLiteral("offerer: %1").arg(m_offererReason);
        }
        return out.join(QStringLiteral("; "));
    }

    /// Structural status only: never include SDP, candidate, key, or device identity.
    QString openDiagnostic() const
    {
        return QStringLiteral("start(answerer=%1,offerer=%2) opened(answerer=%3,offerer=%4) "
                              "failed(answerer=%5,offerer=%6)")
            .arg(int(m_start.answererStarted))
            .arg(int(m_start.offererStarted))
            .arg(int(m_answererOpened))
            .arg(int(m_offererOpened))
            .arg(int(m_answererFailed))
            .arg(int(m_offererFailed));
    }

    /// Everything each end sent has been handled at the other, a close at
    /// either end has reached the other, and the client's choice about
    /// answering pings is the offerer's.
    bool settled()
    {
        if (m_client) {
            m_offerer->setAnswersPingsForTest(m_client->answersPings());
        }
        // A close crossing from one end to the other is not settled yet.
        const bool offererOpen = m_offerer->isOpen();
        const bool answererOpen = m_answerer && m_answerer->isOpen();
        if (offererOpen != answererOpen) {
            return false;
        }
        return !offererOpen || dataChannelPairSettled(*m_offerer, *m_answerer);
    }

    DataChannelTransport* offerer() const { return m_offerer; }
    DataChannelTransport* answerer() const { return m_answerer; }

private:
    QPointer<LoopbackTransport> m_client;
    LoopbackTransport* m_near = nullptr;
    DataChannelTransport* m_offerer = nullptr;
    QPointer<DataChannelTransport> m_answerer;
    bool m_started = false;
    DataChannelPairStart m_start;
    bool m_offererOpened = false;
    bool m_answererOpened = false;
    bool m_offererFailed = false;
    bool m_answererFailed = false;
    QString m_offererReason;
    QString m_answererReason;

    static QString structuralReason(const QString& reason)
    {
        // DataChannelTransport's own failure sentences.
        static const QStringList known{
            QStringLiteral("the control connection could not be made"),
            QStringLiteral("the control connection closed"),
            QStringLiteral("the control connection closed before it opened"),
            QStringLiteral("the control channel closed"),
            QStringLiteral("an unexpected data channel was refused"),
            QStringLiteral("oversized local description"),
        };
        return known.contains(reason) ? reason : QStringLiteral("library error");
    }
};

} // namespace NereusSDR::Test
