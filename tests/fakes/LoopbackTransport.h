#pragma once
// tests/fakes/LoopbackTransport.h
//
// no-port-check: NereusSDR-original test fixture.
//
// Remote-daemon R2 Task 18: an in-process SessionTransport pair.
//
// Two of these link to each other; whatever one sends arrives at the
// other's textReceived(), on a LATER turn of the event loop rather than
// synchronously, so the ordering a real socket imposes is reproduced
// rather than papered over by direct calls.
//
// It exists for two things a real wss socket cannot give a test:
//
//   1. **Running the protocol without TLS.** Every handshake, ordering,
//      preemption and rate-limit assertion in tst_station_session.cpp runs
//      over this, so they stay meaningful on a Qt build where
//      QSslSocket::supportsSsl() is false and the wss slots QSKIP.
//   2. **A peer that goes silent without closing.** setAnswersPings(false)
//      makes this end stop producing pongs while the link stays nominally
//      up, which is what a laptop lid, a cell handoff or a NAT timeout
//      actually looks like -- and is unreproducible over a loopback
//      QWebSocket, whose Qt stack answers every ping automatically.
//
// Production code sees no difference: StationServer::acceptTransport() and
// StationClient::startSession() take a SessionTransport either way, and
// there is no test-only branch inside either class.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08  J.J. Boyd / KG4VCF  Remote daemon R2 Task 18: in-process
//                                    session transport fake. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 18: the certificate
//                                    the far end presents. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app plan Task 37: setSevered, a
//                                    path that goes dead without closing.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  iPhone app plan Task 29 fix wave:
//                                    setDropsOutgoing(), one direction
//                                    dead. AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Direct media fix wave:
//                                    setCarriesBinary(), a session link
//                                    without binary messages (a data
//                                    channel). AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  setHoldsOutgoing(), a slow link
//                                    that delivers late and in order;
//                                    setDropsOutgoing() loses pings and
//                                    pongs too. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-10-12  J.J. Boyd / KG4VCF  setRewritesIncoming(), the text a
//                                    far end of another version would
//                                    have sent. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-11-05  J.J. Boyd / KG4VCF  setMeasuresReceipt() and
//                                    arriveFromAnyThreadForTest(), an end
//                                    that reports a message's wait as a
//                                    data channel does. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include <functional>
#include <optional>
#include <utility>

#include <QByteArray>
#include <QList>
#include <QPointer>
#include <QString>

#include "core/session/SessionTransport.h"

namespace NereusSDR::Test {

class LoopbackTransport : public NereusSDR::SessionTransport {
    Q_OBJECT

public:
    explicit LoopbackTransport(const QString& description, QObject* parent = nullptr);

    /// Links the two ends. Call on ONE of them; it links both directions.
    void linkTo(LoopbackTransport* peer);

    // ---- SessionTransport ----
    void sendText(const QByteArray& wire) override;
    /// Task 29 step 2b: binary messages, queued as text is.
    bool sendBinary(const QByteArray& message) override;
    bool carriesBinary() const override { return m_carriesBinary; }
    void ping() override;
    void closeLink(const QString& reason) override;
    bool isOpen() const override;
    QString peerDescription() const override;
    QString peerAddress() const override { return m_peerAddress; }
    std::optional<qint64> deliveringMessageWaitUs() const override { return m_deliveringWaitUs; }
    QByteArray peerCertificateSha256() const override { return m_peerCertificateSha256; }

    // ---- Test controls ----

    /// What peerAddress() reports (the far end's address, as the station
    /// sees it). Default empty, as a relayed connection reports.
    void setPeerAddress(const QString& address) { m_peerAddress = address; }

    /// What carriesBinary() reports. Default true (a WebSocket); false
    /// stands for a session link that carries no binary messages, where
    /// the media tunnel cannot run.
    void setCarriesBinary(bool carries) { m_carriesBinary = carries; }

    /// What peerCertificateSha256() reports: the certificate the far end
    /// would have presented over TLS (a Core's, as its pin's 32 bytes).
    /// Default empty, as a link without TLS reports.
    void setPeerCertificateSha256(const QByteArray& sha256) { m_peerCertificateSha256 = sha256; }

    /// While false, a ping arriving at this end produces no pong, so the
    /// SENDER sees a peer that has gone silent without closing. Default
    /// true.
    void setAnswersPings(bool answers) { m_answersPings = answers; }
    bool answersPings() const { return m_answersPings; }

    /// Task 37: while true, the path through this end is dead without
    /// closing: nothing it sends leaves (text or ping) and nothing arrives
    /// at it. Neither end sees a close. Default false.
    void setSevered(bool severed) { m_severed = severed; }
    bool severed() const { return m_severed; }
    /// Task 29 fix wave: while true, what this end sends is lost (text,
    /// binary, its pings and its pongs) and what the far end sends still
    /// arrives (one direction dead). Default false.
    void setDropsOutgoing(bool drops) { m_dropsOutgoing = drops; }
    /// While true, what this end sends (text, binary, pings and pongs)
    /// waits, in order; false releases it in the order sent. A slow link:
    /// late, never lost. Default false.
    void setHoldsOutgoing(bool holds);
    /// Text arriving at this end goes through `rewrite` first; its result
    /// is what received() records and what this end's listener gets, and
    /// an empty result delivers nothing. For a far end of another version:
    /// the same messages without a value it does not send yet. An empty
    /// function, the default, delivers the text as sent.
    void setRewritesIncoming(std::function<QByteArray(const QByteArray&)> rewrite)
    {
        m_rewriteIncoming = std::move(rewrite);
    }
    /// While true, this end reports what a data channel session on a direct
    /// or TURN pair reports (over the web relay it would report about nothing):
    /// while a message is delivered, how long it waited since its receipt.
    /// Its receipt is when it was sent, so what a far end's
    /// setHoldsOutgoing() held counts as having waited here, behind a stalled
    /// thread, not on the link. Default false, as a WebSocket reports nothing.
    void setMeasuresReceipt(bool measures) { m_measuresReceipt = measures; }
    /// Safe from any thread: `wire` arrives at this end now and waits for
    /// this end's thread, as a data channel library's thread hands over a
    /// message while the owner's thread is stalled.
    void arriveFromAnyThreadForTest(const QByteArray& wire);
    LoopbackTransport* peerForTest() const { return m_peer; }

    /// Every wire message this end has received, in arrival order.
    QList<QByteArray> received() const { return m_received; }
    void clearReceived() { m_received.clear(); }

    /// The "type" field of each received message, in order -- what the
    /// ordering assertions compare against.
    QList<QByteArray> receivedKinds() const;

    int pingsSeen() const { return m_pingsSeen; }
    QString closeReason() const { return m_closeReason; }

signals:
    /// Synchronous test seam for a callback during a station send.
    void outboundText(const QByteArray& wire);

private:
    void deliver(const QByteArray& wire, qint64 receivedNs);
    void receivePing();
    void sendOrHold(std::function<void()> delivery);

    QString m_description;
    QPointer<LoopbackTransport> m_peer;
    bool m_open = true;
    bool m_answersPings = true;
    bool m_severed = false;
    bool m_dropsOutgoing = false;
    bool m_carriesBinary = true;
    bool m_holdsOutgoing = false;
    bool m_measuresReceipt = false;
    std::optional<qint64> m_deliveringWaitUs;
    QList<std::function<void()>> m_heldOutgoing;
    std::function<QByteArray(const QByteArray&)> m_rewriteIncoming;
    int m_pingsSeen = 0;
    QString m_closeReason;
    QString m_peerAddress;
    QByteArray m_peerCertificateSha256;
    QList<QByteArray> m_received;
};

} // namespace NereusSDR::Test
