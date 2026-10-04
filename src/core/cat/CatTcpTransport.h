//=================================================================
// MW0LGE 2022
//=================================================================

// inspiration from https://www.codeproject.com/Articles/5733/A-TCP-IP-Server-written-in-C
//

// Ported from Thetis Project Files/Source/Console/CAT/TCPIPcatServer.cs
// Upstream source has an author/inspiration notice; project-level GNU General Public License applies.
// Modification history (NereusSDR):
// 2026-10-04 - Native event-loop CAT adaptation by J.J. Boyd (KG4VCF),
//              AI-assisted via OpenAI Codex.

#pragma once
#include <QObject>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
namespace NereusSDR {
class CatTcpTransport : public QObject {
    Q_OBJECT
public:
    explicit CatTcpTransport(QObject* parent = nullptr);
    ~CatTcpTransport() override;
    bool start(const QHostAddress&, quint16 port);
    void stop();
    bool attachSession(quint64, QTcpSocket*);
    bool writeBytes(quint64, const QByteArray&);
    void closeSession(quint64);
    bool isListening() const { return m_server.isListening(); }
    QHostAddress boundAddress() const { return m_server.serverAddress(); }
    quint16 boundPort() const { return m_server.serverPort(); }
    int clientCount() const { return m_sockets.size(); }
    QString errorString() const { return m_server.errorString(); }
    // Nereus bounds: independent of catalogue request and answer widths.
    static constexpr qsizetype kMaximumOutputBytes = 64 * 1024;
    static constexpr qint64 kMaximumPendingBytes = 256 * 1024;
signals:
    void clientAccepted(QTcpSocket*);
    void bytesReceived(quint64, QByteArray);
    void closeRequested(quint64);
    void clientCountChanged(int);
private:
    void acceptClients();
    void readBytes(quint64);
    QTcpServer m_server;
    QHash<quint64, QPointer<QTcpSocket>> m_sockets;
};
} // namespace NereusSDR
