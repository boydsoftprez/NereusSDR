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
// 2026-10-06 - Export the class from the Windows Core DLL so the GUI and tests
//              can use its signals. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-06 - Port the 30 s quiet-client drop (checkClientCommInterval).
//              J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

#pragma once
#include "core/NereusCoreExport.h"
#include <QObject>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QHash>
namespace NereusSDR {
class NEREUS_CORE_EXPORT CatTcpTransport : public QObject {
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
    // From Thetis CAT/TCPIPcatServer.cs:90-91 [v2.10.3.15]: checkClientCommInterval every 30000 ms.
    static constexpr int kThetisIdleCheckIntervalMs = 30000;
    // Applies to sessions attached afterwards; 0 never drops a quiet client.
    void setIdleCheckInterval(int ms) { m_idleCheckIntervalMs = ms > 0 ? ms : 0; }
    int idleCheckInterval() const { return m_idleCheckIntervalMs; }
signals:
    void clientAccepted(QTcpSocket*);
    void bytesReceived(quint64, QByteArray);
    void closeRequested(quint64);
    void clientCountChanged(int);
private:
    void acceptClients();
    void readBytes(quint64);
    void checkClientCommInterval(quint64);
    void stopIdleWatch(quint64);
    // Thetis keeps last/current send and receive times; a flag per interval is equivalent.
    struct IdleWatch { QPointer<QTimer> timer; bool received{false}; bool sent{false}; };
    QTcpServer m_server;
    QHash<quint64, QPointer<QTcpSocket>> m_sockets;
    QHash<quint64, IdleWatch> m_idle;
    int m_idleCheckIntervalMs{0};
};
} // namespace NereusSDR
