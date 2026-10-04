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

#include "CatTcpTransport.h"
#include "core/LogCategories.h"
#include <utility>
namespace NereusSDR {
namespace {
// Nereus read scheduling/bounds; no upstream arbitrary-size allocation.
constexpr qint64 kReadChunkBytes = 4096;
constexpr qint64 kMaximumReadBufferBytes = 64 * 1024;
}
CatTcpTransport::CatTcpTransport(QObject* parent) : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &CatTcpTransport::acceptClients);
}
CatTcpTransport::~CatTcpTransport() { stop(); }
bool CatTcpTransport::start(const QHostAddress& address, quint16 port)
{
    // From Thetis CAT/TCPIPcatServer.cs:505-535 [v2.10.3.15]. Qt event-loop adaptation.
    if (m_server.isListening()) { return false; }
    if (!m_server.listen(address, port)) {
        qCWarning(lcCat) << "CAT TCP bind failed:" << m_server.errorString(); return false;
    }
    return true;
}
void CatTcpTransport::acceptClients()
{
    const QPointer<CatTcpTransport> self(this);
    while (m_server.hasPendingConnections()) {
        QTcpSocket* socket = m_server.nextPendingConnection();
        if (!socket) { continue; }
        socket->setParent(this); socket->setReadBufferSize(kMaximumReadBufferBytes);
        const QPointer<QTcpSocket> guard(socket);
        emit clientAccepted(socket);
        if (!self) { return; }
        if (guard && !m_sockets.values().contains(guard)) {
            socket->setParent(nullptr); socket->deleteLater(); socket->abort();
            if (!self) { return; }
        }
        if (!m_server.isListening()) { return; }
    }
}
bool CatTcpTransport::attachSession(quint64 id, QTcpSocket* socket)
{
    if (!id || !socket || !m_server.isListening() || m_sockets.contains(id)) { return false; }
    m_sockets.insert(id, socket);
    connect(socket, &QTcpSocket::readyRead, this, [this, id] { readBytes(id); });
    connect(socket, &QTcpSocket::disconnected, this, [this, id] { emit closeRequested(id); });
    connect(socket, &QTcpSocket::errorOccurred, this, [this, id](QAbstractSocket::SocketError) {
        const QPointer<QTcpSocket> socket = m_sockets.value(id);
        if (socket) { qCWarning(lcCat) << "CAT TCP client error:" << socket->errorString(); }
        emit closeRequested(id);
    });
    const QPointer<CatTcpTransport> self(this);
    emit clientCountChanged(m_sockets.size());
    if (!self || m_sockets.value(id) != socket) { return false; }
    if (socket->bytesAvailable()) { readBytes(id); }
    return self && m_sockets.contains(id);
}
void CatTcpTransport::readBytes(quint64 id)
{
    // From Thetis CAT/TCPIPcatServer.cs:107-120 [v2.10.3.15]. Bounded asynchronous read.
    const QPointer<CatTcpTransport> self(this);
    const QPointer<QTcpSocket> socket = m_sockets.value(id);
    while (socket && socket->bytesAvailable() > 0) {
        const QByteArray bytes = socket->read(kReadChunkBytes);
        if (!self || m_sockets.value(id) != socket) { return; }
        if (bytes.isEmpty()) { break; }
        emit bytesReceived(id, bytes);
        if (!self || m_sockets.value(id) != socket) { return; }
    }
}
bool CatTcpTransport::writeBytes(quint64 id, const QByteArray& bytes)
{
    // From Thetis CAT/TCPIPcatServer.cs:358-380 [v2.10.3.15]. Checked/bounded Qt queue.
    const QPointer<QTcpSocket> socket = m_sockets.value(id);
    if (!socket) { return false; }
    if (bytes.size() > kMaximumOutputBytes || socket->bytesToWrite() > kMaximumPendingBytes - bytes.size()
        || socket->state() != QAbstractSocket::ConnectedState) {
        qCWarning(lcCat) << "CAT TCP output limit or closed socket";
        emit closeRequested(id); return false;
    }
    const QPointer<CatTcpTransport> self(this);
    const qint64 written = socket->write(bytes);
    if (!self || !socket || m_sockets.value(id) != socket) { return false; }
    if (written != bytes.size()) { emit closeRequested(id); return false; }
    return true;
}
void CatTcpTransport::closeSession(quint64 id)
{
    const QPointer<QTcpSocket> socket = m_sockets.take(id);
    if (!socket) { return; }
    const QPointer<CatTcpTransport> self(this);
    socket->disconnect(this);
    // abort emits synchronously: parent destruction must not delete its active socket.
    socket->setParent(nullptr); socket->deleteLater(); socket->abort();
    if (!self) { return; }
    emit clientCountChanged(m_sockets.size());
}
void CatTcpTransport::stop()
{
    // Caller cancels all claims before close. Detach old clients before callbacks.
    m_server.close();
    const auto sockets = std::exchange(m_sockets, {});
    for (const QPointer<QTcpSocket>& socket : sockets) {
        if (socket) { socket->disconnect(this); socket->setParent(nullptr); socket->deleteLater(); }
    }
    // Entire old snapshot is independent of this before any callback can restart/delete it.
    for (const QPointer<QTcpSocket>& socket : sockets) {
        if (socket) { socket->abort(); }
    }
}
} // namespace NereusSDR
