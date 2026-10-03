#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/RendezvousDialer.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 28 (R-IOS-16): the desktop's side of a control
// connection through the remote access service. It asks the service to
// introduce this computer, a paired device, to the Core registered under
// the Core's rendezvous id, and makes the ICE connection that follows,
// carrying the control channel (DataChannelTransport):
//
//   1. connects to the first server that answers (RendezvousClient);
//   2. resolves the hello's STUN names and chooses the STUN server by this
//      computer's address families (IceConfiguration), then makes its offer
//      and introduces itself, signing with its device key;
//   3. takes the Core's answer, resolves the relay names in it, chooses the
//      relay host the same way and gathers; candidates go both ways through
//      the service;
//   4. once the channel opens, hands the transport over (ready()) and
//      leaves the service: the session runs over the connection, and the
//      service stays out of it (the rendezvous document, section 6.3).
//
// A service that does not answer, a Core that is not there, a connection
// that fails or has not opened within kDialDeadlineMs ends the attempt
// with failed(), in plain words. The introduction ending (the Core left the
// service, it expired) or this computer's connection to the service ending
// fails the attempt at once while the Core's answer or the end of its
// candidates has not come. Once both have, this computer holds everything
// the service would carry from the Core, ICE can finish without the
// service, and the deadline runs. The certificate is not checked here: the
// session checks the one the Core presents in DTLS against the one its
// identity key binds, at the same gate as a WebSocket's (StationClient).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: Task 28 tail (re-review Minor): an introduction or service
//               connection that ends after the Core's answer leaves the
//               attempt to its deadline. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 (R-IOS-16): an attempt without the
//               relay (an upgrade, a Core with the relay off), and a Core
//               that never answers its introduction ends after
//               kAnswerDeadlineMs in plain words (link section 21.1). J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 step 2b (R-IOS-16, R-IOS-08): the
//               web relay's leg (RelayLeg) and its per-connection candidate
//               sources; the computer's own proxy settings (SystemProxy).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/IceConfiguration.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <memory>
#include <optional>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace NereusSDR {

class ClientDeviceIdentity;
class DataChannelTransport;
class RendezvousClient;
class RelayLeg;

class RendezvousDialer : public QObject {
    Q_OBJECT

public:
    /// The whole attempt: the service's hello and the two lookups, then
    /// gathering and the connectivity checks (IceConfiguration::
    /// kConnectDeadlineMs).
    static constexpr int kDialDeadlineMs = 10000 + 2 * IceConfiguration::kHostLookupTimeoutMs
                                           + IceConfiguration::kConnectDeadlineMs;
    /// iPhone app plan Task 29 (link section 21.1): how long after its
    /// introduce this computer waits for the Core's answer. A Core answers
    /// once its STUN names are resolved (IceConfiguration::
    /// kHostLookupTimeoutMs at most) and the service relays it at once, so
    /// a Core silent this long does not answer introductions at all (one
    /// older than controlChannelVersion). NereusSDR's own bound: the lookup
    /// and 7 s for the service's round trips.
    static constexpr int kAnswerDeadlineMs = IceConfiguration::kHostLookupTimeoutMs + 7000;
    /// The words for that Core (link section 21.1).
    static constexpr const char* kCoreTooOldReason =
        "This Core can't be reached through the internet service. Updating the Core may help.";
    /// Task 29 fix wave (review Minor 5): the words when the Core's last
    /// session said it answers through the service (controlChannelVersion
    /// 1), so it is slow or offline, not too old.
    static constexpr const char* kCoreDidNotAnswerReason =
        "The Core did not answer through the internet service. Check that it is on and "
        "online.";

    explicit RendezvousDialer(QObject* parent = nullptr);
    ~RendezvousDialer() override;

    RendezvousDialer(const RendezvousDialer&) = delete;
    RendezvousDialer& operator=(const RendezvousDialer&) = delete;

    /// Starts an attempt: `servers` in order (RendezvousClient::serverUrls),
    /// the Core's rendezvous id, and this computer's device key. Once only.
    void dial(const QList<QUrl>& servers, const QString& stationId,
              std::shared_ptr<const ClientDeviceIdentity> device);
    /// Stops the attempt; nothing more is signalled.
    void cancel();

    /// Test seam: the attempt's bound.
    void setDialDeadlineMs(int ms) { m_deadlineMs = ms; }
    /// Test seam: kAnswerDeadlineMs.
    void setAnswerDeadlineMs(int ms) { m_answerDeadlineMs = ms; }
    /// iPhone app plan Task 29: false gathers no relay candidate and
    /// accepts none of the Core's (an upgrade, which looks for a path
    /// without the relay; a Core with the relay turned off). Default true.
    /// Before dial().
    void setAllowRelay(bool allow) { m_allowRelay = allow; }
    /// Task 29 fix wave (review Minor 5): the Core's last session declared
    /// controlChannelVersion 1, so a Core that does not answer is not too
    /// old. Before dial().
    void setCoreAnswersIntroductions(bool answers) { m_coreAnswersIntroductions = answers; }
    /// The attempt ended because the Core never answered its introduction
    /// (kCoreTooOldReason, or kCoreDidNotAnswerReason for a Core known to
    /// answer).
    bool coreDidNotAnswer() const { return m_coreDidNotAnswer; }
    /// It never answered and nothing says it can: probably an older Core.
    bool coreTooOld() const { return m_coreDidNotAnswer && !m_coreAnswersIntroductions; }
    /// The Core answered with relay credentials.
    bool relayOffered() const { return m_relayOffered; }
    /// The Core's answer arrived.
    bool answered() const { return m_answered; }
    /// The service sent a relay grant for this introduction: the Core
    /// allowed the relay (rendezvous section 12.1).
    bool relayGranted() const { return m_relayGranted; }

    /// The ICE settings of the connection, the relay included once known.
    std::optional<IceConfiguration> iceConfiguration() const { return m_ice; }
    /// Task 29 step 2b: this attempt's leg to the web relay (null without
    /// the relay), and how it ended, if it has.
    std::shared_ptr<RelayLeg> relayLeg() const { return m_leg; }
    QString webRelayEndCode() const { return m_legEndCode; }
    QString webRelayEndWords() const { return m_legEndWords; }

signals:
    /// The control channel opened. The receiver takes `transport` (it has
    /// no parent) and runs the session over it.
    void ready(NereusSDR::DataChannelTransport* transport);
    /// The attempt ended without a connection. `reason` is plain words.
    void failed(const QString& reason);
    /// Task 29 step 2b: the web relay ended this attempt's leg (section
    /// 12.4's code and words).
    void webRelayEnded(const QString& code, const QString& words);

private:
    void fail(const QString& reason);
    void startOffer();
    /// The introduction or the connection to the service ended: fails at
    /// once unless the Core's answer and the end of its candidates have
    /// both come, when the attempt carries on to its deadline.
    void serviceGone(const QString& why);

    RendezvousClient* m_client = nullptr;
    QPointer<DataChannelTransport> m_transport;
    QTimer* m_deadline = nullptr;
    std::shared_ptr<const ClientDeviceIdentity> m_device;
    QString m_stationId;
    std::optional<IceConfiguration> m_ice;
    int m_deadlineMs = kDialDeadlineMs;
    int m_answerDeadlineMs = kAnswerDeadlineMs;
    QTimer* m_answerDeadline = nullptr;
    bool m_allowRelay = true;
    bool m_coreDidNotAnswer = false;
    bool m_coreAnswersIntroductions = false;
    bool m_relayOffered = false;
    bool m_relayGranted = false;
    std::shared_ptr<RelayLeg> m_leg;
    QString m_legEndCode;
    QString m_legEndWords;
    bool m_started = false;
    bool m_done = false;
    bool m_answered = false;
    /// The Core's end of candidates (an empty candidate) came.
    bool m_coreCandidatesEnded = false;
};

} // namespace NereusSDR
