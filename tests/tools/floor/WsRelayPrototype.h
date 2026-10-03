#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// tests/tools/floor/WsRelayPrototype.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 29 Step 1 (R-IOS-16): option (E) of the relay floor,
// prototyped far enough to measure. Not product code: it lives with the
// measurement tool (nereus_floor_probe) only.
//
// Each end opens a WebSocket to the relay role of the rendezvous service
// (the prototype's is tests/tools/floor_relay_prototype.py) and names the
// same pairing token; the service forwards each binary frame to the other
// end's socket. At each end a loopback UDP socket stands between the
// WebSocket and the ICE agent, so ICE, DTLS, SCTP and SRTP run end to end
// unchanged and the relay only ever sees their ciphertext:
//
//   device: its agent is given 127.0.0.1:<this socket> as a remote host
//           candidate; datagrams from the agent go out as frames, frames
//           in go back to the address the agent last sent from.
//   Core:   frames in go to 127.0.0.1:<the agent's port> (setAgentPort),
//           where the agent learns this socket as a peer-reflexive
//           candidate; its answers go back out as frames.
//
// A dropped WebSocket reconnects at once under the same token; the
// loopback addresses do not change, so ICE does not notice beyond the
// datagrams lost meanwhile.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QUrl>

QT_BEGIN_NAMESPACE
class QUdpSocket;
class QWebSocket;
QT_END_NAMESPACE

namespace NereusSDR::FloorPrototype {

class WsRelayPrototype : public QObject {
    Q_OBJECT

public:
    enum class Role { Device, Core };

    WsRelayPrototype(QUrl relayUrl, QString token, Role role, QObject* parent = nullptr);
    ~WsRelayPrototype() override;

    /// Binds the loopback socket and opens the WebSocket; the loopback
    /// port, 0 on failure.
    quint16 start();
    /// Core: where the ICE agent listens on loopback.
    void setAgentPort(quint16 port);
    /// Aborts the WebSocket's TCP connection and reconnects at once.
    void resetConnection();

    quint64 framesOut() const { return m_framesOut; }
    quint64 framesIn() const { return m_framesIn; }
    int connections() const { return m_connections; }

signals:
    void opened();

private:
    void open();
    void onDatagrams();

    QUrl m_url;
    QString m_token;
    Role m_role;
    QUdpSocket* m_udp = nullptr;
    QWebSocket* m_socket = nullptr;
    quint16 m_agentPort = 0;
    QHostAddress m_agentAddress {QHostAddress::LocalHost};
    quint64 m_framesOut = 0;
    quint64 m_framesIn = 0;
    int m_connections = 0;
};

} // namespace NereusSDR::FloorPrototype
