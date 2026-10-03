// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/RendezvousMailboxTransport.cpp  (NereusSDR)
// =================================================================
//
// See RendezvousMailboxTransport.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/session/RendezvousMailboxTransport.h"

#include "core/session/RendezvousClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

namespace NereusSDR {

namespace {
Q_LOGGING_CATEGORY(lcMailbox, "nereussdr.rendezvous")
}

RendezvousMailboxTransport::RendezvousMailboxTransport(RendezvousClient* client, QObject* parent)
    : SessionTransport(parent)
    , m_client(client)
{
    if (!m_client || !m_client->isMailboxOpen()) {
        m_open = false;
        return;
    }
    connect(m_client, &RendezvousClient::mailboxReceived, this, [this](const QString& body) {
        if (m_open) {
            emit textReceived(body.toUtf8());
        }
    });
    connect(m_client, &RendezvousClient::mailboxClosed, this, [this] { finish(); });
    connect(m_client, &RendezvousClient::connectionLost, this, [this] { finish(); });
    connect(m_client, &QObject::destroyed, this, [this] { finish(); });
}

RendezvousMailboxTransport::~RendezvousMailboxTransport() = default;

bool RendezvousMailboxTransport::isPairingMessage(const QByteArray& wire)
{
    const QJsonDocument document = QJsonDocument::fromJson(wire);
    if (!document.isObject()) {
        return false;
    }
    return document.object().value(QStringLiteral("type")).toString().startsWith(
        QLatin1String("pair."));
}

void RendezvousMailboxTransport::sendText(const QByteArray& wire)
{
    if (!m_open || !m_client) {
        return;
    }
    if (!isPairingMessage(wire)) {
        // A hello, a session end: a mailbox carries pairing only.
        return;
    }
    if (!m_client->sendMailbox(QString::fromUtf8(wire))) {
        qCWarning(lcMailbox) << "A pairing message could not be sent through the mailbox";
    }
}

void RendezvousMailboxTransport::ping()
{
    if (m_open) {
        emit pongReceived();
    }
}

void RendezvousMailboxTransport::closeLink(const QString& reason)
{
    Q_UNUSED(reason);
    if (!m_open) {
        return;
    }
    if (m_client) {
        m_client->closeMailbox();
    }
    finish();
}

bool RendezvousMailboxTransport::isOpen() const
{
    return m_open;
}

QString RendezvousMailboxTransport::peerDescription() const
{
    return QStringLiteral("a device pairing through the remote access service");
}

void RendezvousMailboxTransport::finish()
{
    if (!m_open) {
        return;
    }
    m_open = false;
    if (m_client) {
        disconnect(m_client, nullptr, this, nullptr);
    }
    // Queued, as a socket's close is: whoever called closeLink() finishes
    // what it was doing before it hears of the close.
    QMetaObject::invokeMethod(this, [this] { emit closed(); }, Qt::QueuedConnection);
}

} // namespace NereusSDR
