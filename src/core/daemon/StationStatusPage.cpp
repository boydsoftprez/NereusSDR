// no-port-check: NereusSDR-original.
// =================================================================
// src/core/daemon/StationStatusPage.cpp  (NereusSDR)
// =================================================================
// See StationStatusPage.h.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: Part C fix wave (R1-I2): a reopened pairing window
//               lasts 10 minutes, five burned codes in a row close any window,
//               and reopening starts afresh. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: Part C fix wave (R2-I1): the Host check accepts
//               only this computer's own names, whole. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: the first-start notice gives the sudo form a packaged
//               Core answers (R-IOS-08, R-R3-26). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/daemon/StationStatusPage.h"

#include "core/LogCategories.h"
#include "core/security/DeviceStore.h"
#include "core/security/PairingWindow.h"
#include "core/session/StationServer.h"

#include <QHostInfo>
#include <QNetworkInterface>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <memory>

namespace NereusSDR {

namespace {

QByteArray response(int status, const QByteArray& reason, const QByteArray& body)
{
    // No caching anywhere: the code changes, and a claimed Core's page must
    // never come back out of a cache with the code still on it. The
    // content policy allows the inline style and nothing else: no script,
    // no form, no frame, no outside request.
    QByteArray head = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason + "\r\n";
    head += "Content-Type: text/html; charset=utf-8\r\n";
    head += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    head += "Cache-Control: no-store\r\n";
    head += "Content-Security-Policy: default-src 'none'; style-src 'unsafe-inline'; "
            "form-action 'none'; frame-ancestors 'none'\r\n";
    head += "X-Content-Type-Options: nosniff\r\n";
    head += "Referrer-Policy: no-referrer\r\n";
    head += "Connection: close\r\n\r\n";
    return head + body;
}

QByteArray notFound()
{
    return response(404, "Not Found",
                    QByteArrayLiteral("<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
                                      "<title>Not found</title></head><body>"
                                      "<p>Not found.</p></body></html>"));
}

} // namespace

StationStatusPage::StationStatusPage(Sources sources, QObject* parent)
    : QObject(parent)
    , m_sources(std::move(sources))
    , m_peerCheck([](const QHostAddress& peer) {
        return StationServer::isOnDirectNetwork(peer.toString());
    })
{
}

StationStatusPage::~StationStatusPage() = default;

void StationStatusPage::setPeerCheck(PeerCheck check)
{
    if (check) {
        m_peerCheck = std::move(check);
    }
}

bool StationStatusPage::listen(const QHostAddress& address, quint16 port)
{
    close();
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &StationStatusPage::onNewConnection);
    if (!m_server->listen(address, port)) {
        m_lastError = m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return false;
    }
    m_lastError.clear();
    return true;
}

void StationStatusPage::close()
{
    if (m_server != nullptr) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
}

bool StationStatusPage::isListening() const
{
    return m_server != nullptr && m_server->isListening();
}

quint16 StationStatusPage::serverPort() const
{
    return isListening() ? m_server->serverPort() : 0;
}

QHostAddress StationStatusPage::serverAddress() const
{
    return isListening() ? m_server->serverAddress() : QHostAddress();
}

void StationStatusPage::onNewConnection()
{
    while (m_server != nullptr && m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();
        if (socket == nullptr) {
            break;
        }
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        // A peer that is not on one of this computer's own networks gets no
        // page and no reply: the connection closes before a byte is read.
        if (!m_peerCheck(socket->peerAddress())) {
            socket->abort();
            socket->deleteLater();
            continue;
        }
        serve(socket);
    }
}

void StationStatusPage::serve(QTcpSocket* socket)
{
    QPointer<QTcpSocket> guarded(socket);
    QTimer::singleShot(kRequestTimeoutMs, socket, [guarded]() {
        if (guarded) {
            guarded->abort();
            guarded->deleteLater();
        }
    });
    auto buffer = std::make_shared<QByteArray>();
    connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer]() {
        buffer->append(socket->readAll());
        const qsizetype end = buffer->indexOf("\r\n\r\n");
        if (end < 0) {
            if (buffer->size() > kMaxRequestBytes) {
                socket->abort();
                socket->deleteLater();
            }
            return;
        }
        const QList<QByteArray> lines = buffer->left(end).split('\n');
        const QList<QByteArray> request = lines.value(0).trimmed().split(' ');
        QString host;
        bool hostSeen = false;
        for (qsizetype i = 1; i < lines.size(); ++i) {
            const QByteArray line = lines.at(i).trimmed();
            const qsizetype colon = line.indexOf(':');
            if (colon > 0 && line.left(colon).trimmed().toLower() == "host") {
                host = QString::fromLatin1(line.mid(colon + 1).trimmed());
                hostSeen = true;
            }
        }
        disconnect(socket, &QTcpSocket::readyRead, socket, nullptr);
        // GET / only: every other method or path is not found, and nothing
        // here acts on anything a request carries.
        const bool answer = request.size() == 3 && request.at(0) == "GET"
                            && request.at(1) == "/" && request.at(2).startsWith("HTTP/1.")
                            && (!hostSeen || isAcceptedHost(host));
        socket->write(answer ? response(200, "OK", renderPage().toUtf8()) : notFound());
        socket->disconnectFromHost();
    });
}

bool StationStatusPage::showsCode() const
{
    StationServer* server = m_sources.server ? m_sources.server() : nullptr;
    if (server == nullptr || server->pairingWindow() == nullptr) {
        return false;
    }
    // Both, not either: the window's own state and the device store's.
    // A reopened window on a claimed Core never puts its code here.
    return server->pairingWindow()->state() == PairingWindow::State::OpenUnclaimed
           && !server->deviceStore()->isClaimed()
           && !server->pairingWindow()->currentCode().isEmpty();
}

QString StationStatusPage::renderPage() const
{
    const QString label = m_sources.label ? m_sources.label() : QString();
    const StationRadioStatus radio = m_sources.radio ? m_sources.radio() : StationRadioStatus{};
    StationServer* server = m_sources.server ? m_sources.server() : nullptr;

    QString radioLine;
    if (radio.connected) {
        QStringList parts;
        if (!radio.model.isEmpty()) {
            parts << radio.model;
        }
        if (!radio.name.isEmpty() && radio.name != radio.model) {
            parts << radio.name;
        }
        radioLine = parts.isEmpty() ? QStringLiteral("Radio: connected.")
                                    : QStringLiteral("Radio: %1, connected.")
                                          .arg(parts.join(QStringLiteral(", ")).toHtmlEscaped());
    } else {
        radioLine = QStringLiteral("Radio: off.");
    }

    QString pairing;
    if (server == nullptr) {
        pairing = QStringLiteral("<p>Remote access is off on this Core.</p>");
    } else if (server->deviceStore()->isClaimed()) {
        pairing = QStringLiteral("<p>A device has paired with this Core.</p>");
    } else if (showsCode()) {
        pairing = QStringLiteral(
                      "<p>No device is paired with this Core.</p>"
                      "<p class=\"code\">Pairing code: <strong>%1</strong></p>"
                      "<p>Type this code into the NereusSDR app on your phone or computer to "
                      "pair it with this Core.</p>")
                      .arg(server->pairingWindow()->currentCode().toHtmlEscaped());
    } else if (server->pairingWindow() != nullptr
               && server->pairingWindow()->state() == PairingWindow::State::ClosedUnclaimed) {
        pairing = QStringLiteral("<p>No device is paired with this Core.</p>"
                                 "<p>Pairing closed after too many wrong pairing codes. Run "
                                 "nereusd pairing open on this Core's computer to open it "
                                 "again.</p>");
    } else {
        pairing = QStringLiteral("<p>No device is paired with this Core.</p>"
                                 "<p>A new pairing code appears here shortly.</p>");
    }

    const QString title = label.isEmpty() ? QStringLiteral("NereusSDR Core")
                                          : label.toHtmlEscaped();
    return QStringLiteral(
               "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">"
               "<meta http-equiv=\"refresh\" content=\"%1\">"
               "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
               "<title>%2</title>"
               "<style>body{font-family:sans-serif;margin:1.5em;max-width:40em}"
               ".code{font-size:1.4em}</style></head><body>"
               "<h1>%2</h1><p>NereusSDR Core</p><p>%3</p>%4"
               "<p><small>This page updates every %1 seconds.</small></p>"
               "</body></html>")
        .arg(QString::number(kRefreshSeconds), title, radioLine, pairing);
}

QString StationStatusPage::addressForOperator(const QHostAddress& bound, quint16 port)
{
    QHostAddress address = bound;
    if (bound == QHostAddress(QHostAddress::Any) || bound == QHostAddress(QHostAddress::AnyIPv4)
        || bound == QHostAddress(QHostAddress::AnyIPv6)) {
        address = QHostAddress();
        const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface& interface : interfaces) {
            const auto flags = interface.flags();
            if (!flags.testFlag(QNetworkInterface::IsUp)
                || !flags.testFlag(QNetworkInterface::IsRunning)
                || flags.testFlag(QNetworkInterface::IsLoopBack)) {
                continue;
            }
            for (const QNetworkAddressEntry& entry : interface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol
                    && !entry.ip().isLoopback() && !entry.ip().isLinkLocal()) {
                    address = entry.ip();
                    break;
                }
            }
            if (!address.isNull()) {
                break;
            }
        }
    }
    if (address.isNull() || address.isLoopback()) {
        return {};
    }
    const QString host = address.protocol() == QAbstractSocket::IPv6Protocol
                             ? QStringLiteral("[%1]").arg(address.toString())
                             : address.toString();
    return QStringLiteral("http://%1:%2/").arg(host).arg(port);
}

QString StationStatusPage::formatFirstStartNotice(const QString& label, const QString& pageAddress)
{
    QString text = QStringLiteral("\n  This Core: %1\n").arg(label);
    if (!pageAddress.isEmpty()) {
        text += QStringLiteral("  Status page: %1 (open it in a browser on this network)\n")
                    .arg(pageAddress);
    }
    text += QStringLiteral(
        "  Manage it with nereusd status, nereusd pairing show and nereusd devices\n"
        "  (on a packaged Core: sudo nereusd status, and so on).\n");
    return text;
}

bool StationStatusPage::isAcceptedHost(const QString& header)
{
    QString host = header.trimmed();
    if (host.isEmpty()) {
        return false;
    }
    if (host.startsWith(QLatin1Char('['))) {
        const qsizetype close = host.indexOf(QLatin1Char(']'));
        if (close < 0) {
            return false;
        }
        return !QHostAddress(host.mid(1, close - 1)).isNull();
    }
    if (host.count(QLatin1Char(':')) == 1) {
        host = host.left(host.indexOf(QLatin1Char(':')));
    }
    if (!QHostAddress(host).isNull()) {
        return true;
    }
    // Fix wave R2-I1: only this computer's own names, whole. A name that
    // merely starts with it (<own>.attacker.example) is what a DNS
    // rebinding page would use to read the code, since this check is the
    // page's only defence against one.
    const QString own = QHostInfo::localHostName();
    if (own.isEmpty()) {
        return false;
    }
    QStringList names{own, own + QStringLiteral(".local")};
    const QString domain = QHostInfo::localDomainName();
    if (!domain.isEmpty()) {
        names << own + QLatin1Char('.') + domain;
    }
    for (const QString& name : std::as_const(names)) {
        if (host.compare(name, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace NereusSDR
