// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 19.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "gui/hotkeys/DeckLink.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/HotKeyStateFeed.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSaveFile>

#include <algorithm>
#include <cmath>
#include <utility>

namespace NereusSDR {

namespace {

const QString& enabledKey()
{
    static const QString kKey = QStringLiteral("HotKeys/DeckLink/Enabled");
    return kKey;
}

// How long a live answer may take, as StationControlSocket::listen waits for its own.
constexpr int kProbeMs = 500;

#if !defined(Q_OS_WIN)
// The longest name this link will try: sockaddr_un on macOS holds 104 bytes with the null.
constexpr qsizetype kMaxSocketPathBytes = 103;
#endif

// First 12 hex characters of the SHA-256 of the settings file's path, so each profile
// has its own link and a plugin host that can't read AppSettings can still find it.
QString profileHash()
{
    const QByteArray path = AppSettings::instance().filePath().toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(path, QCryptographicHash::Sha256).toHex().left(12));
}

// What QLocalServer and QLocalSocket take. On Windows that's the bare pipe name.
QString listenNameFor(const QString& hash)
{
#if defined(Q_OS_WIN)
    return QStringLiteral("nereussdr-deck-") + hash;
#else
    return QDir(DeckLink::deckFolder()).filePath(hash + QStringLiteral(".sock"));
#endif
}

QString discoveryPathFor(const QString& hash)
{
    return QDir(DeckLink::deckFolder()).filePath(hash + QStringLiteral(".json"));
}

QByteArray toLine(const QJsonObject& message)
{
    return QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n';
}

QByteArray stateLine(const QString& id, bool on)
{
    return toLine({{QStringLiteral("type"), QStringLiteral("state")}, {QStringLiteral("id"), id}, {QStringLiteral("on"), on}});
}

QString gestureName(HotKeyGesture gesture)
{
    switch (gesture) {
    case HotKeyGesture::Press:
        return QStringLiteral("press");
    case HotKeyGesture::Hold:
        return QStringLiteral("hold");
    case HotKeyGesture::Turn:
        return QStringLiteral("turn");
    }
    return QString();
}

QByteArray helloLine()
{
    QJsonArray actions;
    for (const HotKeyAction& action : HotKeyActionList::all()) {
        actions.append(QJsonObject{{QStringLiteral("id"), action.id},
                                   {QStringLiteral("name"), action.name},
                                   {QStringLiteral("group"), action.group},
                                   {QStringLiteral("gesture"), gestureName(action.gesture)},
                                   {QStringLiteral("transmits"), action.transmits}});
    }
    return toLine({{QStringLiteral("type"), QStringLiteral("hello")},
                   {QStringLiteral("protocol"), DeckLink::kProtocolVersion},
                   {QStringLiteral("actions"), actions}});
}

// False for a pin that isn't Slice A to Slice D. No pin at all is fine, and leaves *pin null.
bool readPin(const QJsonObject& message, QChar* pin)
{
    const auto value = message.constFind(QStringLiteral("pin"));
    if (value == message.constEnd()) {
        return true;
    }
    const QString text = value->toString();
    if (text.size() != 1 || text.at(0) < QLatin1Char('A') || text.at(0) > QLatin1Char('D')) {
        return false;
    }
    *pin = text.at(0);
    return true;
}

} // namespace

DeckLink::DeckLink(HotKeyDispatcher* dispatcher, HotKeyStateFeed* feed, QObject* parent)
    : QObject(parent)
    , m_dispatcher(dispatcher)
    , m_feed(feed)
{
    if (feed != nullptr) {
        connect(feed, &HotKeyStateFeed::stateChanged, this, &DeckLink::onStateChanged);
        connect(feed, &HotKeyStateFeed::displayChanged, this, &DeckLink::onDisplayChanged);
    }
}

DeckLink::~DeckLink()
{
    stopListening();
}

bool DeckLink::savedEnabled()
{
    return AppSettings::instance().value(enabledKey()).toString() != QLatin1String("False");
}

bool DeckLink::setEnabled(bool on)
{
    if (on != savedEnabled()) {
        AppSettings& settings = AppSettings::instance();
        settings.setValue(enabledKey(), on ? QStringLiteral("True") : QStringLiteral("False"));
        settings.save();
    }
    if (!on) {
        m_lastError.clear();
        stopListening();
        return true;
    }
    if (isListening()) {
        return true;
    }
    if (startListening()) {
        m_lastError.clear();
        return true;
    }
    qCWarning(lcHotKeys) << "Deck link not open:" << m_lastError;
    return false;
}

bool DeckLink::isListening() const
{
    return m_server != nullptr;
}

QString DeckLink::socketPath() const
{
#if defined(Q_OS_WIN)
    return QStringLiteral("\\\\.\\pipe\\") + listenNameFor(profileHash());
#else
    return listenNameFor(profileHash());
#endif
}

QString DeckLink::deckFolder()
{
    // A test's own home where it has one: on Windows no environment variable moves
    // QDir::homePath(), and a test run must not write to the real home.
    const QString testHome = qEnvironmentVariable("NEREUS_TEST_HOME");
    return QDir(testHome.isEmpty() ? QDir::homePath() : testHome).filePath(QStringLiteral(".nereussdr/deck"));
}

QList<QString> DeckLink::connectedPlugins() const
{
    QList<QString> names;
    for (const Connection& connection : m_connections) {
        if (!connection.plugin.isEmpty()) {
            names.append(connection.plugin);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool DeckLink::startListening()
{
    const QString hash = profileHash();
    const QString name = listenNameFor(hash);
    const QString folder = deckFolder();

#if !defined(Q_OS_WIN)
    if (QFile::encodeName(name).size() > kMaxSocketPathBytes) {
        m_lastError = QStringLiteral("The path to your home folder is too long for a deck plugin connection.");
        return false;
    }
#endif
    if (!QDir().mkpath(folder)) {
        m_lastError = QStringLiteral("Could not make the folder for deck plugins: %1").arg(folder);
        return false;
    }
#if !defined(Q_OS_WIN)
    // Every time, and not only when it's made: this is what keeps other users out of the
    // discovery file as well as the socket.
    if (!QFile::setPermissions(folder, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
        m_lastError = QStringLiteral("Could not make the folder for deck plugins private: %1").arg(folder);
        return false;
    }
#endif

    // As StationControlSocket::listen: one answering here keeps its socket, and a file
    // left by one that stopped is replaced. On Windows two servers can share a pipe and
    // listen won't report it, so this is the only test there.
    {
        QLocalSocket probe;
        probe.connectToServer(name);
        if (probe.waitForConnected(kProbeMs)) {
            probe.abort();
            m_lastError = QStringLiteral("Another copy of NereusSDR with these settings already has the deck plugin connection.");
            return false;
        }
    }
    QLocalServer::removeServer(name);

    auto server = std::make_unique<QLocalServer>();
    // Owner-only, inside a folder that is too.
    server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(server.get(), &QLocalServer::newConnection, this, &DeckLink::acceptConnections);
    if (!server->listen(name)) {
        m_lastError = QStringLiteral("Could not open the deck plugin connection: %1").arg(server->errorString());
        return false;
    }

    const QString discovery = discoveryPathFor(hash);
    if (!writeDiscoveryFile(discovery)) {
        m_lastError = QStringLiteral("Could not write the file deck plugins look for: %1").arg(discovery);
        server->close();
        QLocalServer::removeServer(name);
        return false;
    }
    m_server = std::move(server);
    m_listenName = name;
    m_discoveryPath = discovery;
    return true;
}

bool DeckLink::writeDiscoveryFile(const QString& path)
{
    const QJsonObject content{{QStringLiteral("protocol"), kProtocolVersion},
                              {QStringLiteral("path"), socketPath()},
                              {QStringLiteral("pid"), QCoreApplication::applicationPid()},
                              {QStringLiteral("profile"), AppSettings::profileOverride()}};
    // Written beside its name and moved there, so a plugin never reads half of it. The
    // name it's written under doesn't end in .json, so a plugin doesn't pick it up.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(QJsonDocument(content).toJson(QJsonDocument::Compact));
    return file.commit();
}

void DeckLink::stopListening()
{
    // Nothing of another copy's is touched: only what this one made is named here.
    if (m_server != nullptr) {
        QFile::remove(m_discoveryPath);
        m_server->close();
        m_server.reset();
        QLocalServer::removeServer(m_listenName);
    }
    const QList<QLocalSocket*> sockets = m_connections.keys();
    for (QLocalSocket* const socket : sockets) {
        dropConnection(socket);
    }
}

void DeckLink::acceptConnections()
{
    while (m_server != nullptr && m_server->hasPendingConnections()) {
        QLocalSocket* const socket = m_server->nextPendingConnection();
        if (socket == nullptr) {
            break;
        }
        // Accepted sockets outlive the listener until they're dropped.
        socket->setParent(this);
        m_connections.insert(socket, Connection{QString::number(++m_lastConnectionId), QString(), QByteArray()});
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { readFrom(socket); });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] { dropConnection(socket); });
        connect(socket, &QLocalSocket::errorOccurred, this, [this, socket](QLocalSocket::LocalSocketError) {
            if (socket->state() != QLocalSocket::ConnectedState) {
                dropConnection(socket);
            }
        });
        // It may have spoken, or gone, before these were connected.
        if (socket->bytesAvailable() > 0) {
            readFrom(socket);
        }
        if (socket->state() != QLocalSocket::ConnectedState) {
            dropConnection(socket);
        }
    }
}

void DeckLink::readFrom(QLocalSocket* socket)
{
    auto connection = m_connections.find(socket);
    if (connection == m_connections.end()) {
        return;     // dropped: what it still had to say can't start anything now
    }
    connection->buffer += socket->readAll();
    ++m_readDepth;
    // Looked up afresh each time: running a line can end this connection or any other.
    while ((connection = m_connections.find(socket)) != m_connections.end()) {
        const qsizetype end = connection->buffer.indexOf('\n');
        const qsizetype length = end < 0 ? connection->buffer.size() : end;
        if (length > kMaxLineBytes) {
            qCWarning(lcHotKeys) << "Closing a deck plugin connection: a line went over" << kMaxLineBytes << "bytes";
            dropConnection(socket);
            break;
        }
        if (end < 0) {
            break;      // the rest of this line comes with a later read
        }
        const QByteArray line = connection->buffer.left(end);
        connection->buffer.remove(0, end + 1);
        handleLine(socket, line);
    }
    // Back out of the line that started this, and past what the socket was doing when it
    // said readyRead, so now a socket dropped along the way can go.
    if (--m_readDepth == 0) {
        for (QLocalSocket* const dropped : std::as_const(m_droppedWhileReading)) {
            dropped->deleteLater();
        }
        m_droppedWhileReading.clear();
    }
}

void DeckLink::handleLine(QLocalSocket* socket, const QByteArray& line)
{
    if (line.trimmed().isEmpty()) {
        return;
    }
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(line, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        qCWarning(lcHotKeys) << "Ignored a deck plugin line that isn't a JSON object, of" << line.size() << "bytes";
        return;
    }
    const QJsonObject message = document.object();
    const QString type = message.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("hello")) {
        handleHello(socket, message);
        return;
    }
    const auto connection = m_connections.constFind(socket);
    if (connection == m_connections.constEnd()) {
        return;
    }
    if (connection->plugin.isEmpty()) {
        qCWarning(lcHotKeys) << "Ignored a deck plugin message before its hello:" << type;
        return;
    }
    if (type == QLatin1String("press")) {
        handlePress(socket, message);
    } else if (type == QLatin1String("release")) {
        handleRelease(socket, message);
    } else if (type == QLatin1String("turn")) {
        handleTurn(socket, message);
    } else {
        qCWarning(lcHotKeys) << "Ignored a deck plugin message of unknown type:" << type;
    }
}

void DeckLink::handleHello(QLocalSocket* socket, const QJsonObject& message)
{
    const QString plugin = message.value(QStringLiteral("plugin")).toString();
    const auto connection = m_connections.find(socket);
    if (connection == m_connections.end()) {
        return;
    }
    if (plugin.isEmpty()) {
        qCWarning(lcHotKeys) << "Ignored a deck plugin hello with no plugin name";
        return;
    }
    connection->plugin = plugin;

    writeLine(socket, helloLine());
    if (m_feed != nullptr) {
        // What it should light now: it has heard nothing until this.
        const QStringList ids = HotKeyStateFeed::actionIds();
        for (const QString& id : ids) {
            if (m_connections.contains(socket)) {
                writeLine(socket, stateLine(id, m_feed->isOn(id)));
            }
        }
        if (m_connections.contains(socket)) {
            writeLine(socket, displayLine());
        }
    }
    emit pluginsChanged();
    noteHostsWithDevices();
}

void DeckLink::handlePress(QLocalSocket* socket, const QJsonObject& message)
{
    const QString id = message.value(QStringLiteral("id")).toString();
    if (HotKeyActionList::find(id) == nullptr) {
        qCWarning(lcHotKeys) << "Ignored a deck plugin press of an unknown action:" << id;
        return;
    }
    QChar pin;
    if (!readPin(message, &pin)) {
        qCWarning(lcHotKeys) << "Ignored a deck plugin press with a bad slice:" << id;
        return;
    }
    if (m_dispatcher == nullptr) {
        return;
    }
    const HotKeySource source{HotKeySourceKind::Deck, m_connections.value(socket).id};
    m_dispatcher->press(id, source, pin);
    // Running an action can spin the event loop (a dialog, say). If the connection ended
    // in there, its drop found nothing to release, and now nothing else ever will.
    if (m_dispatcher != nullptr && !m_connections.contains(socket)) {
        m_dispatcher->releaseAll(source);
    }
}

void DeckLink::handleRelease(QLocalSocket* socket, const QJsonObject& message)
{
    const QString id = message.value(QStringLiteral("id")).toString();
    if (HotKeyActionList::find(id) == nullptr) {
        qCWarning(lcHotKeys) << "Ignored a deck plugin release of an unknown action:" << id;
        return;
    }
    // Any pin is left unread: a hold belongs to its source and letting go must always work.
    if (m_dispatcher != nullptr) {
        m_dispatcher->release(id, HotKeySource{HotKeySourceKind::Deck, m_connections.value(socket).id});
    }
}

void DeckLink::handleTurn(QLocalSocket* socket, const QJsonObject& message)
{
    const QString idUp = message.value(QStringLiteral("idUp")).toString();
    const QString idDown = message.value(QStringLiteral("idDown")).toString();
    if (HotKeyActionList::find(idUp) == nullptr || HotKeyActionList::find(idDown) == nullptr) {
        qCWarning(lcHotKeys) << "Ignored a deck plugin turn of an unknown action:" << idUp << idDown;
        return;
    }
    const double steps = message.value(QStringLiteral("steps")).toDouble(std::nan(""));
    if (!std::isfinite(steps) || std::trunc(steps) != steps || std::abs(steps) > kMaxTurnSteps) {
        qCWarning(lcHotKeys) << "Ignored a deck plugin turn with bad steps:" << idUp << idDown;
        return;
    }
    QChar pin;
    if (!readPin(message, &pin)) {
        qCWarning(lcHotKeys) << "Ignored a deck plugin turn with a bad slice:" << idUp << idDown;
        return;
    }
    if (m_dispatcher != nullptr) {
        m_dispatcher->turn(idUp, idDown, static_cast<int>(steps),
                           HotKeySource{HotKeySourceKind::Deck, m_connections.value(socket).id}, pin);
    }
}

// The one way a connection ends, whatever ended it. Takes it out of the table first, so
// nothing it still has to say can run, and whatever the release sets off can't come back.
void DeckLink::dropConnection(QLocalSocket* socket)
{
    const auto connection = m_connections.constFind(socket);
    if (connection == m_connections.constEnd()) {
        return;
    }
    const QString id = connection->id;
    const bool named = !connection->plugin.isEmpty();
    m_connections.erase(connection);

    socket->disconnect(this);
    if (m_dispatcher != nullptr) {
        m_dispatcher->releaseAll(HotKeySource{HotKeySourceKind::Deck, id});
    }
    socket->abort();
    // Running a line can spin an event loop, and a deleteLater made in there runs in there.
    // That would delete a socket still emitting readyRead further up the stack.
    if (m_readDepth > 0) {
        m_droppedWhileReading.append(socket);
    } else {
        socket->deleteLater();
    }
    if (named) {
        emit pluginsChanged();
    }
    noteHostsWithDevices();
}

void DeckLink::writeLine(QLocalSocket* socket, const QByteArray& line)
{
    socket->write(line);
    if (socket->bytesToWrite() > kMaxUnsentBytes) {
        qCWarning(lcHotKeys) << "Closing a deck plugin connection: it has stopped reading";
        dropConnection(socket);
    }
}

void DeckLink::broadcast(const QByteArray& line)
{
    // A copy: a write can end a connection.
    const QList<QLocalSocket*> sockets = m_connections.keys();
    for (QLocalSocket* const socket : sockets) {
        const auto connection = m_connections.constFind(socket);
        if (connection != m_connections.constEnd() && !connection->plugin.isEmpty()) {
            writeLine(socket, line);
        }
    }
}

void DeckLink::onStateChanged(const QString& id, bool on)
{
    broadcast(stateLine(id, on));
}

void DeckLink::onDisplayChanged()
{
    // The feed also says so for what isn't on this message, such as the radio connecting.
    const QByteArray line = displayLine();
    if (line != m_lastDisplayLine) {
        m_lastDisplayLine = line;
        broadcast(line);
    }
}

QByteArray DeckLink::displayLine() const
{
    const HotKeyStateFeed::Display display = m_feed != nullptr ? m_feed->display() : HotKeyStateFeed::Display{};
    return toLine({{QStringLiteral("type"), QStringLiteral("display")},
                   {QStringLiteral("frequency"), display.frequency},
                   {QStringLiteral("mode"), display.mode},
                   {QStringLiteral("filter"), display.filter},
                   {QStringLiteral("slice"), display.slice.isNull() ? QString() : QString(display.slice)}});
}

// Elgato's app and Ulanzi Studio each take their device for themselves, so one press could
// otherwise arrive twice: once from the plugin and once from this window's own driver.
void DeckLink::noteHostsWithDevices()
{
    const auto isConnected = [this](const QLatin1String& plugin) {
        return std::any_of(m_connections.cbegin(), m_connections.cend(),
                           [plugin](const Connection& connection) { return connection.plugin == plugin; });
    };
    const bool elgato = isConnected(QLatin1String("elgato"));
    if (elgato != m_elgatoConnected) {
        m_elgatoConnected = elgato;
        emit elgatoConnectionChanged(elgato);
    }
    const bool ulanzi = isConnected(QLatin1String("ulanzi"));
    if (ulanzi != m_ulanziConnected) {
        m_ulanziConnected = ulanzi;
        emit ulanziConnectionChanged(ulanzi);
    }
}

} // namespace NereusSDR
