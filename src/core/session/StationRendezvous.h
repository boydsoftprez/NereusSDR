#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/StationRendezvous.h  (NereusSDR)
// =================================================================
//
// The Core's use of the remote access service (iPhone app plan Task 27,
// R-IOS-08 and R-IOS-16; the pairing design, sections 4.3 and 5.3): what
// DaemonApp starts beside its StationServer, in one place so a test runs
// exactly what nereusd runs.
//
//   - Registers the Core under the rendezvous id of its station identity
//     key (StationServer::stationIdentity(), never the TLS certificate's
//     key) with the first of the configured servers that answers, and keeps
//     it registered.
//   - Introductions: only a paired device (StationServer::deviceStore())
//     whose signature verifies is reported (RendezvousClient::introduced);
//     every other is dropped without a reply and counted.
//   - Plan Task 28: each reported introduction is answered with a control
//     connection (DataChannelTransport, the Core's own certificate in
//     DTLS): the STUN server chosen by this computer's address families
//     once the hello's names are resolved (an introduction that arrives
//     during the lookup waits for it), the relay host chosen the same way
//     once the credentials arrive (or no relay, when they have not come
//     within kCredentialsTimeoutMs of the answer), candidates both ways
//     through the service. When the channel opens it becomes one of
//     StationServer's connections through acceptIntroducedTransport(): the
//     same session as a WebSocket's, with that function's three rules (it
//     never pairs, since pairing through the service stays the mailbox's;
//     it signs in by device key only, never a token; and every introduced
//     connection still connecting shares one source's handshake cap). The
//     introduction is retired at once; one that fails or has not opened
//     within kAnswerDeadlineMs is retired and dropped.
//   - Holds a nameplate while the pairing window is open, gives it back
//     when it closes (after the mailbox of the pairing that closed it has
//     closed, since releasing a nameplate ends its mailbox), and hands the
//     number to the window, so the code the Core shows is the one a device
//     types from anywhere.
//   - A device opening the mailbox on that nameplate pairs by the code
//     through it: StationServer::acceptPairingMailbox() runs the same
//     exchange as on a direct connection.
//
// A session already running is never touched by anything here: the service
// can stop and restart while devices stay connected.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 28 (R-IOS-16): introductions answered
//               with a control connection. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Task 28 fix wave: connections handed over as introduced
//               (review Important 1 and 2), an unopened answer freed when
//               the device leaves, answers owned by std::unique_ptr (Minors
//               1 and 4). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: Task 28 tail (re-review Minor): the header names
//               acceptIntroducedTransport() and its three rules. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/session/IceConfiguration.h"
#include "core/session/RendezvousClient.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include <memory>
#include <unordered_map>

namespace NereusSDR {

class DataChannelTransport;
class StationServer;

class StationRendezvous : public QObject {
    Q_OBJECT

public:
    /// `server` must outlive this object. `servers` is the ordered list
    /// (RendezvousClient::serverUrls); `relayAllowed` nereusd.conf's
    /// `relay`.
    StationRendezvous(StationServer* server, const QList<QUrl>& servers, bool relayAllowed,
                      QObject* parent = nullptr);
    ~StationRendezvous() override;

    StationRendezvous(const StationRendezvous&) = delete;
    StationRendezvous& operator=(const StationRendezvous&) = delete;

    /// Registers, and follows the pairing window. False when the Core has
    /// no usable identity key or no server is configured.
    bool start();

    RendezvousClient* client() const { return m_client; }

    /// How long the Core waits for the relay credentials after answering
    /// with the relay allowed before it gathers without them. NereusSDR's
    /// own bound: the service sends them straight after the answer (the
    /// rendezvous document, section 6.3), so a few seconds is plenty and
    /// costs the connection little of its time.
    static constexpr int kCredentialsTimeoutMs = 5000;
    /// How long an answered introduction may take to open its control
    /// connection: credentials, gathering and the connectivity checks
    /// (IceConfiguration::kConnectDeadlineMs), plus the two lookups
    /// (IceConfiguration::kHostLookupTimeoutMs). Then it is retired and
    /// its connection dropped.
    static constexpr int kAnswerDeadlineMs = kCredentialsTimeoutMs
        + 2 * IceConfiguration::kHostLookupTimeoutMs + IceConfiguration::kConnectDeadlineMs;

    /// Test seam: while false, introductions are reported and nothing
    /// answers them here (a test that answers them itself). Default true.
    void setAnswersIntroductionsForTest(bool answers) { m_answersIntroductions = answers; }

    /// Test seams: the two bounds above.
    void setCredentialsTimeoutMs(int ms) { m_credentialsTimeoutMs = ms; }
    void setAnswerDeadlineMs(int ms) { m_answerDeadlineMs = ms; }

    /// Answered introductions whose connection has not opened yet.
    int pendingAnswers() const { return static_cast<int>(m_answers.size()); }

private:
    struct Answer;

    void followPairingWindow();
    void answerIntroduction(const RendezvousIntroduction& introduction);
    void finishAnswer(const QByteArray& id, bool opened);
    /// The answer for `id`, or null.
    Answer* answerFor(const QByteArray& id) const;

    QPointer<StationServer> m_server;
    RendezvousClient* m_client = nullptr;
    /// The window closed while a device's pairing mailbox was still open
    /// (the pairing that just claimed the Core): the nameplate goes back
    /// once that mailbox has closed, so the Core's last pairing message is
    /// not cut off by the release.
    bool m_releaseAfterMailbox = false;

    // Task 28.
    /// The STUN names of the current connection's hello, resolved: until
    /// then introductions wait in m_waiting.
    quint64 m_lookup = 0;
    bool m_stunResolved = false;
    HostFamilies m_stunFamilies;
    QList<RendezvousIntroduction> m_waiting;
    std::unordered_map<QByteArray, std::unique_ptr<Answer>> m_answers;
    int m_credentialsTimeoutMs = kCredentialsTimeoutMs;
    int m_answerDeadlineMs = kAnswerDeadlineMs;
    bool m_answersIntroductions = true;
};

} // namespace NereusSDR
