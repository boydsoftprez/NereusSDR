// no-port-check: NereusSDR-original.
// =================================================================
// src/core/daemon/StationControlSocket.cpp  (NereusSDR)
// =================================================================
// See StationControlSocket.h.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: candidatePathsFor() and the no-answer text (R-IOS-08,
//               R-R3-26). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include "core/daemon/StationControlSocket.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/daemon/DaemonConfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <QTimer>

#include <memory>

namespace NereusSDR {

namespace {

QByteArray encodeReply(const StationControlReply& reply)
{
    return QJsonDocument(QJsonObject{{QStringLiteral("ok"), reply.ok},
                                     {QStringLiteral("text"), reply.text}})
               .toJson(QJsonDocument::Compact)
           + '\n';
}

QStringList& packagedHomesOverride()
{
    static QStringList homes;
    return homes;
}

bool& packagedHomesOverridden()
{
    static bool overridden = false;
    return overridden;
}

// `tried` names every place the command looked: the one socket it found,
// or all of them when none was there.
StationControlReply notReached(const QStringList& tried, QLocalSocket::LocalSocketError error)
{
    const QString where = tried.join(QStringLiteral(" and "));
    if (error == QLocalSocket::SocketAccessError) {
        return {false, QStringLiteral("This account may not manage the Core at %1. Run the "
                                      "command with sudo, for example sudo nereusd status.")
                           .arg(where)};
    }
    return {false, QStringLiteral("No Core answered. Looked for it at %1. Check that nereusd is "
                                  "running (systemctl status nereusd on a packaged Core). A "
                                  "packaged Core answers the command with sudo and nothing "
                                  "else, for example sudo nereusd status. A Core you started "
                                  "yourself answers the command run from the same account, with "
                                  "the same --config and --profile it was started with.")
                       .arg(where)};
}

StationControlReply requestAt(const QString& path, const QStringList& args, int timeoutMs,
                              const QStringList& tried)
{
    QElapsedTimer deadline;
    deadline.start();
    const auto remaining = [&] { return qMax(0, timeoutMs - int(deadline.elapsed())); };
    QLocalSocket socket;
    socket.connectToServer(path);
    if (!socket.waitForConnected(remaining())) {
        return notReached(tried, socket.error());
    }
    const QByteArray line = QJsonDocument(QJsonObject{{QStringLiteral("args"),
                                                       QJsonArray::fromStringList(args)}})
                                .toJson(QJsonDocument::Compact)
                            + '\n';
    socket.write(line);
    if (!socket.waitForBytesWritten(remaining())) {
        return notReached(tried, socket.error());
    }
    QByteArray received;
    while (!received.contains('\n') && received.size() <= StationControlSocket::kMaxReplyBytes) {
        if (socket.bytesAvailable() == 0 && !socket.waitForReadyRead(remaining())) {
            break;
        }
        received += socket.readAll();
    }
    const QJsonDocument doc = QJsonDocument::fromJson(received.left(received.indexOf('\n')));
    if (!doc.isObject()) {
        return {false, QStringLiteral("The Core at %1 did not answer that command.").arg(path)};
    }
    return {doc.object().value(QStringLiteral("ok")).toBool(false),
            doc.object().value(QStringLiteral("text")).toString()};
}

} // namespace

StationControlSocket::StationControlSocket(Handler handler, QObject* parent,
                                           AsyncHandler asyncHandler)
    : QObject(parent)
    , m_handler(std::move(handler))
    , m_asyncHandler(std::move(asyncHandler))
{
}

StationControlSocket::~StationControlSocket()
{
    close();
}

QString StationControlSocket::directoryFor(const DaemonConfig& config, const QString& profile)
{
    if (!config.stateDirectory.isEmpty()) {
        return QDir::cleanPath(config.stateDirectory);
    }
    return AppSettings::resolveConfigDir(profile);
}

QString StationControlSocket::socketPathFor(const DaemonConfig& config, const QString& profile)
{
    return QDir(directoryFor(config, profile)).filePath(QString::fromLatin1(kSocketName));
}

QStringList StationControlSocket::packagedHomes()
{
    if (packagedHomesOverridden()) {
        return packagedHomesOverride();
    }
#ifdef Q_OS_LINUX
    // packaging/nereusd.service.in: StateDirectory=nereusd with
    // DynamicUser=yes and Environment=HOME=/var/lib/nereusd. systemd keeps
    // a DynamicUser's state in /var/lib/private/nereusd and links
    // /var/lib/nereusd to it.
    return {QStringLiteral("/var/lib/nereusd"), QStringLiteral("/var/lib/private/nereusd")};
#else
    return {};
#endif
}

void StationControlSocket::setPackagedHomesForTest(const QStringList& homes)
{
    packagedHomesOverride() = homes;
    packagedHomesOverridden() = true;
}

QStringList StationControlSocket::candidatePathsFor(const DaemonConfig& config,
                                                    const QString& profile)
{
    return candidatePathsFor(config, profile, packagedHomes());
}

QStringList StationControlSocket::candidatePathsFor(const DaemonConfig& config,
                                                    const QString& profile,
                                                    const QStringList& homes)
{
    QStringList paths{socketPathFor(config, profile)};
    if (!config.stateDirectory.isEmpty()) {
        return paths;
    }
    // The profile's directory as AppSettings::resolveConfigDir() makes it
    // on Linux ($HOME/.config/NereusSDR, then profiles/<name> for a valid
    // name), under the packaged Core's HOME instead of this command's.
    for (const QString& home : homes) {
        QString directory = QDir(home).filePath(QStringLiteral(".config/NereusSDR"));
        if (AppSettings::isValidProfileName(profile)) {
            directory += QStringLiteral("/profiles/") + profile;
        }
        const QString path = QDir::cleanPath(
            QDir(directory).filePath(QString::fromLatin1(kSocketName)));
        if (!paths.contains(path)) {
            paths << path;
        }
    }
    return paths;
}

bool StationControlSocket::listen(const QString& path)
{
    close();
    m_path = path;
    const QString directory = QFileInfo(path).absolutePath();
    if (!QDir(directory).exists()) {
        if (!QDir().mkpath(directory)) {
            m_lastError = QStringLiteral("could not create %1").arg(directory);
            return false;
        }
        QFile::setPermissions(directory, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                             | QFileDevice::ExeOwner);
    }
    if (QFileInfo::exists(path)) {
        // Another Core answering here keeps its socket; a file left by one
        // that stopped is replaced.
        QLocalSocket probe;
        probe.connectToServer(path);
        if (probe.waitForConnected(500)) {
            probe.abort();
            m_lastError = QStringLiteral("another Core is already answering at %1").arg(path);
            return false;
        }
        QLocalServer::removeServer(path);
    }
    m_server = new QLocalServer(this);
    // Owner-only: Qt binds in a private directory, sets the mode, then
    // moves the socket to its name, so it never exists more widely open.
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_server, &QLocalServer::newConnection, this, &StationControlSocket::onNewConnection);
    if (!m_server->listen(path)) {
        m_lastError = m_server->errorString();
        // A local socket's name has a short limit (about 100 bytes, less
        // the private directory Qt binds in first); a deep profile
        // directory can pass it.
        if (QFile::encodeName(path).size() > kLongPathBytes) {
            m_lastError += QStringLiteral(" (the path may be too long for a local socket; "
                                          "set state_directory to a shorter directory)");
        }
        delete m_server;
        m_server = nullptr;
        return false;
    }
    m_lastError.clear();
    return true;
}

void StationControlSocket::close()
{
    if (m_server != nullptr) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
        QLocalServer::removeServer(m_path);
    }
}

bool StationControlSocket::isListening() const
{
    return m_server != nullptr && m_server->isListening();
}

void StationControlSocket::onNewConnection()
{
    while (m_server != nullptr && m_server->hasPendingConnections()) {
        QLocalSocket* socket = m_server->nextPendingConnection();
        if (socket == nullptr) {
            break;
        }
        // QLocalServer owns pending sockets by default. A release closes
        // and destroys the listener before dropping station.lock, but its
        // accepted reply socket must outlive that listener.
        socket->setParent(this);
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        serve(socket);
    }
}

void StationControlSocket::serve(QLocalSocket* socket)
{
    QPointer<QLocalSocket> guarded(socket);
    QTimer* const parseDeadline = new QTimer(socket);
    parseDeadline->setSingleShot(true);
    connect(parseDeadline, &QTimer::timeout, socket, [guarded]() {
        if (guarded) { guarded->abort(); }
    });
    parseDeadline->start(kRequestTimeoutMs);
    auto buffer = std::make_shared<QByteArray>();
    connect(socket, &QLocalSocket::readyRead, socket, [this, socket, buffer, parseDeadline]() {
        buffer->append(socket->readAll());
        const qsizetype end = buffer->indexOf('\n');
        if (end < 0) {
            if (buffer->size() > kMaxRequestBytes) {
                socket->abort();
                socket->deleteLater();
            }
            return;
        }
        disconnect(socket, &QLocalSocket::readyRead, socket, nullptr);
        parseDeadline->stop();
        StationControlReply reply;
        const QJsonDocument doc = QJsonDocument::fromJson(buffer->left(end));
        const QJsonValue args = doc.object().value(QStringLiteral("args"));
        if (end > kMaxRequestBytes || !doc.isObject() || !args.isArray()) {
            reply = {false, QStringLiteral("The Core could not read that command.")};
        } else {
            QStringList list;
            for (const QJsonValue& value : args.toArray()) {
                list << value.toString();
            }
            if (m_asyncHandler && m_asyncHandler(list, socket)) { return; }
            reply = m_handler ? m_handler(list) : StationControlReply{};
        }
        sendReply(socket, reply);
    });
}

bool StationControlSocket::sendReply(QLocalSocket* socket, const StationControlReply& reply)
{
    if (!socket || socket->state() != QLocalSocket::ConnectedState) { return false; }
    const QByteArray encoded = encodeReply(reply);
    if (socket->write(encoded) != encoded.size()) { return false; }
    socket->flush();
    socket->disconnectFromServer();
    return true;
}

StationControlReply StationControlSocket::request(const QString& path, const QStringList& args,
                                                  int timeoutMs)
{
    return requestAt(path, args, timeoutMs, {path});
}

StationControlReply StationControlSocket::request(const QStringList& paths,
                                                  const QStringList& args, int timeoutMs)
{
    for (const QString& path : paths) {
        if (QFileInfo::exists(path)) {
            return requestAt(path, args, timeoutMs, {path});
        }
    }
    return requestAt(paths.value(0), args, timeoutMs, paths);
}

} // namespace NereusSDR
