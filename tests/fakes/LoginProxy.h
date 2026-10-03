#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// tests/fakes/LoginProxy.h  (NereusSDR)
// =================================================================
//
// An HTTP proxy on this computer that demands a login from everyone: it
// answers every request, CONNECT included, with 407 Proxy Authentication
// Required and a Basic challenge, and closes. A test points
// the station's WebSockets at it with useAsSystemProxy() (SystemProxy's
// test seam) to stand in for a network whose proxy needs a user name and
// password, which NereusSDR cannot give; the seam is cleared again when
// the proxy goes. requests() counts the requests it refused.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QNetworkProxy>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>

#include <optional>

#include "core/session/SystemProxy.h"

namespace NereusSDR::Test {

class LoginProxy : public QObject {
public:
    explicit LoginProxy(QObject* parent = nullptr)
        : QObject(parent)
    {
        QObject::connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = m_server.nextPendingConnection()) {
                socket->setParent(this);
                QObject::connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    QByteArray& request = m_pending[socket];
                    request += socket->readAll();
                    if (!request.contains("\r\n\r\n")) {
                        return;
                    }
                    ++m_requests;
                    m_pending.remove(socket);
                    socket->write("HTTP/1.1 407 Proxy Authentication Required\r\n"
                                  "Proxy-Authenticate: Basic realm=\"office\"\r\n"
                                  "Content-Length: 0\r\n"
                                  "Connection: close\r\n\r\n");
                    socket->disconnectFromHost();
                });
                QObject::connect(socket, &QTcpSocket::disconnected, socket,
                                 &QObject::deleteLater);
            }
        });
    }

    ~LoginProxy() override
    {
        if (m_inUse) {
            NereusSDR::SystemProxy::setProxyForTest(std::nullopt);
        }
    }

    bool listen() { return m_server.listen(QHostAddress::LocalHost, 0); }

    /// Every WebSocket SystemProxy::forUrl names a proxy for goes here.
    void useAsSystemProxy()
    {
        m_inUse = true;
        NereusSDR::SystemProxy::setProxyForTest(proxy());
    }

    QNetworkProxy proxy() const
    {
        return QNetworkProxy(QNetworkProxy::HttpProxy, QStringLiteral("127.0.0.1"),
                             m_server.serverPort());
    }

    int requests() const { return m_requests; }

private:
    QTcpServer m_server;
    QHash<QTcpSocket*, QByteArray> m_pending;
    int m_requests = 0;
    bool m_inUse = false;
};

} // namespace NereusSDR::Test
