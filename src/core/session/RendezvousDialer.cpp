// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/RendezvousDialer.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 28 (R-IOS-16). See RendezvousDialer.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: Task 28 fix wave (review Minor 1): an ended introduction
//               or a lost service connection ends the attempt at once.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-27: Task 28 tail (re-review Minor): at once only while the
//               Core's answer or the end of its candidates is missing; once
//               both came the deadline runs, as ICE can finish without the
//               service. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 (R-IOS-16): the answer deadline and
//               plain words for an older Core, a dial without the relay.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 step 2b (R-IOS-16, R-IOS-08): the
//               web relay's leg (RelayLeg) and its per-connection candidate
//               sources; the computer's own proxy settings (SystemProxy).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/RendezvousDialer.h"

#include "core/security/ClientDeviceIdentity.h"
#include "core/session/DataChannelTransport.h"
#include "core/session/RendezvousClient.h"
#include "core/session/RelayLeg.h"
#include "core/session/StationClient.h"

#include <QLoggingCategory>
#include <QTimer>

Q_LOGGING_CATEGORY(lcRendezvousDialer, "nereus.session.rendezvousdialer")

namespace NereusSDR {

namespace {

// Plain words for the operator (OperatorWording::isPlain); where they came
// from is in the log.
constexpr const char* kNotReached = "The Core could not be reached from here.";
constexpr const char* kNoDeviceKey = "This computer has no key to connect with.";

} // namespace

RendezvousDialer::RendezvousDialer(QObject* parent)
    : QObject(parent)
{
    m_deadline = new QTimer(this);
    m_deadline->setSingleShot(true);
    connect(m_deadline, &QTimer::timeout, this, [this] {
        qCInfo(lcRendezvousDialer) << "The connection through the remote access service did "
                                      "not open in time";
        fail(QString::fromLatin1(kNotReached));
    });
    // iPhone app plan Task 29 (link section 21.1): a Core that has not
    // answered its introduction by now never will.
    m_answerDeadline = new QTimer(this);
    m_answerDeadline->setSingleShot(true);
    connect(m_answerDeadline, &QTimer::timeout, this, [this] {
        if (m_answered) {
            return;
        }
        qCInfo(lcRendezvousDialer) << "The Core did not answer its introduction";
        m_coreDidNotAnswer = true;
        fail(QString::fromLatin1(m_coreAnswersIntroductions ? kCoreDidNotAnswerReason
                                                            : kCoreTooOldReason));
    });
}

RendezvousDialer::~RendezvousDialer()
{
    cancel();
}

void RendezvousDialer::dial(const QList<QUrl>& servers, const QString& stationId,
                            std::shared_ptr<const ClientDeviceIdentity> device)
{
    if (m_started) {
        return;
    }
    m_started = true;
    m_stationId = stationId;
    m_device = std::move(device);
    if (!m_device || !m_device->isValid()) {
        QTimer::singleShot(0, this, [this] { fail(QString::fromLatin1(kNoDeviceKey)); });
        return;
    }
    m_client = new RendezvousClient(this);
    m_client->setServers(servers);
    m_client->setWatchRelayEnabled(true);
    connect(m_client, &RendezvousClient::connected, this, [this] {
        // The STUN server by this computer's families, once the names are
        // resolved (IceConfiguration).
        IceConfiguration::resolveHostFamilies(
            IceConfiguration::hostNames(m_client->stunUrls()), this,
            [this](const HostFamilies& families) {
                if (m_done || m_transport) {
                    return;
                }
                // Task 29: without the relay, this end gathers none and
                // takes none of the Core's relay candidates.
                m_ice = IceConfiguration::throughRendezvous(
                    m_client->stunUrls(), m_allowRelay,
                    IceConfiguration::localAddressFamilies(), families);
                // Task 29 step 2b (rendezvous section 12): the web relay's
                // leg, when the relay is allowed; it opens when the grant
                // comes, and its lanes join this connection's ICE and,
                // later, the session's media.
                if (m_allowRelay) {
                    m_leg = RelayLeg::create();
                    if (m_leg) {
                        m_ice->setCandidateSourceFactory(RelayLeg::factoryFor(m_leg),
                                                         /*needsRelay=*/true);
                        connect(m_leg.get(), &RelayLeg::ended, this,
                                [this](const QString& code, const QString& words) {
                            m_legEndCode = code;
                            m_legEndWords = words;
                            emit webRelayEnded(code, words);
                        });
                    }
                }
                startOffer();
            });
    });
    connect(m_client, &RendezvousClient::unreachable, this,
            [this](const QString& reason) { fail(reason); });
    // Re-review: a relay grant means the Core allowed the relay (rendezvous
    // section 12.1), whatever the answer's `turn` says.
    connect(m_client, &RendezvousClient::relayGrantReceived, this, [this](const QByteArray&) {
        m_relayGranted = true;
        // Task 29 step 2b: the leg opens at once (section 12.1).
        const std::optional<RendezvousWire::RelayGrant> grant = m_client->relayGrant();
        if (m_leg && grant) {
            if (m_transport && !grant->watchToken.isEmpty()) {
                m_transport->setWatchRelayGrant(
                    {QUrl(grant->url), grant->watchToken, grant->expires, m_leg});
            }
            m_leg->open(QUrl(grant->url), grant->token);
        }
    });
    connect(m_client, &RendezvousClient::answerReceived, this,
            [this](const QString& sdp, bool offered, const RendezvousWire::Turn& turn) {
        if (m_done || !m_transport || m_answered) {
            return;
        }
        m_answered = true;
        m_answerDeadline->stop();
        m_relayOffered = offered;
        if (!m_transport->acceptDescription(sdp, QStringLiteral("answer"))) {
            qCInfo(lcRendezvousDialer) << "The Core's answer could not be used";
            fail(QString::fromLatin1(kNotReached));
            return;
        }
        const std::optional<RendezvousWire::Turn> relay =
            offered ? std::optional<RendezvousWire::Turn>(turn) : std::nullopt;
        // The relay host by this computer's families: its names first.
        IceConfiguration::resolveHostFamilies(
            relay ? IceConfiguration::hostNames(relay->urls) : QStringList(), this,
            [this, relay](const HostFamilies& families) {
                if (m_done || !m_transport || !m_ice) {
                    return;
                }
                m_ice->addHostFamilies(families);
                m_ice->setRelay(relay, 1);
                m_transport->gatherCandidates(*m_ice);
            });
    });
    connect(m_client, &RendezvousClient::candidateReceived, this,
            [this](const QByteArray&, const QString& candidate) {
        if (m_done) {
            return;
        }
        if (candidate.isEmpty()) {
            m_coreCandidatesEnded = true;
            return;
        }
        if (m_transport) {
            m_transport->acceptCandidate(candidate);
        }
    });
    // Task 28 fix wave (review Minor 1), narrowed in the Task 28 tail: once
    // the introduction has ended (the Core left the service, it expired, or
    // this computer's connection to the service was lost) no answer or
    // candidate can come any more.
    connect(m_client, &RendezvousClient::introductionEnded, this,
            [this](const QByteArray&, const QString& code) {
        serviceGone(QStringLiteral("the introduction ended (%1)").arg(code));
    });
    connect(m_client, &RendezvousClient::connectionLost, this, [this] {
        serviceGone(QStringLiteral("the connection to the remote access service ended"));
    });
    connect(m_client, &RendezvousClient::serviceError, this,
            [](const QString& code, const QString&, qint64) {
        qCInfo(lcRendezvousDialer) << "The remote access service answered" << code;
    });
    m_deadline->start(m_deadlineMs);
    m_client->connectToService();
}

void RendezvousDialer::serviceGone(const QString& why)
{
    if (m_done) {
        return;
    }
    if (m_answered && m_coreCandidatesEnded) {
        // Everything the service carries from the Core has come: the
        // connectivity checks run between the two ends without it (the
        // Core learns this computer's address from the checks), so the
        // attempt carries on to its deadline.
        qCInfo(lcRendezvousDialer) << "After the Core's answer and candidates," << why
                                   << "; the connection can still open";
        return;
    }
    // What the connection still needed from the Core cannot come any more:
    // end now rather than at the deadline, so the next try goes through the
    // service again.
    qCInfo(lcRendezvousDialer) << "Before the Core's answer and candidates came," << why;
    fail(QString::fromLatin1(kNotReached));
}

void RendezvousDialer::startOffer()
{
    auto* transport = new DataChannelTransport(this);
    m_transport = transport;
    connect(transport, &DataChannelTransport::localDescription, this,
            [this](const QString& sdp, const QString&) {
        if (m_done) {
            return;
        }
        const std::shared_ptr<const ClientDeviceIdentity> device = m_device;
        m_client->introduce(m_stationId, device->publicKeySpki(),
                            [device](const QByteArray& message) { return device->sign(message); },
                            sdp);
        if (!m_answered && m_answerDeadlineMs > 0) {
            m_answerDeadline->start(m_answerDeadlineMs);
        }
    });
    connect(transport, &DataChannelTransport::localCandidate, this,
            [this](const QString& candidate) {
        if (!m_done) {
            m_client->sendCandidate(candidate);
        }
    });
    connect(transport, &DataChannelTransport::gatheringComplete, this, [this] {
        if (!m_done) {
            m_client->sendCandidate(QString());
        }
    });
    connect(transport, &DataChannelTransport::failed, this, [this](const QString& reason) {
        qCInfo(lcRendezvousDialer) << "The connection through the remote access service failed:"
                                   << reason;
        fail(QString::fromLatin1(kNotReached));
    });
    connect(transport, &DataChannelTransport::opened, this, [this, transport] {
        if (m_done) {
            return;
        }
        m_done = true;
        m_deadline->stop();
        transport->disconnect(this);
        transport->setParent(nullptr);
        m_transport = nullptr;
        // The session runs over the connection from here; the service has
        // nothing more to do in it.
        m_client->stop();
        emit ready(transport);
    });
    DataChannelTransport::Options options;
    options.role = DataChannelTransport::Role::Offerer;
    options.maxIncomingBytes = StationClient::kMaxIncomingMessageBytes;
    options.ice = m_ice;
    if (!transport->start(options)) {
        fail(QString::fromLatin1(kNotReached));
    }
}

void RendezvousDialer::fail(const QString& reason)
{
    if (m_done) {
        return;
    }
    m_done = true;
    m_deadline->stop();
    m_answerDeadline->stop();
    if (m_transport) {
        m_transport->disconnect(this);
        m_transport->closeLink(QStringLiteral("not connected"));
        m_transport->deleteLater();
        m_transport = nullptr;
    }
    if (m_client != nullptr) {
        m_client->stop();
    }
    emit failed(reason);
}

void RendezvousDialer::cancel()
{
    if (m_done) {
        return;
    }
    m_done = true;
    m_deadline->stop();
    m_answerDeadline->stop();
    if (m_transport) {
        m_transport->disconnect(this);
        m_transport->closeLink(QStringLiteral("cancelled"));
        m_transport->deleteLater();
        m_transport = nullptr;
    }
    if (m_client != nullptr) {
        m_client->stop();
    }
}

} // namespace NereusSDR
