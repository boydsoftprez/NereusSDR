// no-port-check: NereusSDR-original. R-R3-48 RF-Kit band follow over TCI.
// J.J. Boyd (KG4VCF), September 2026; AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-48 rework: refresh when the server adds a listener.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#include "core/RfKitBandFollow.h"

#include "core/StationNetwork.h"
#include "core/TciServer.h"

#include <QTimer>
#include <QWebSocket>

namespace NereusSDR {

using BandFollow = RfKitModel::BandFollow;

RfKitBandFollow::RfKitBandFollow(RfKitModel* model, QObject* parent)
    : QObject(parent), m_model(model)
{
    if (model) {
        connect(model, &RfKitModel::stationConnectionChanged,
                this, &RfKitBandFollow::scheduleRefresh);
    }
}

void RfKitBandFollow::setServer(TciServer* server)
{
#ifdef HAVE_WEBSOCKETS
    if (m_server) {
        disconnect(m_server, nullptr, this, nullptr);
    }
    m_server = server;
    if (server) {
        connect(server, &TciServer::serverStarted, this, &RfKitBandFollow::scheduleRefresh);
        connect(server, &TciServer::serverStopped, this, &RfKitBandFollow::scheduleRefresh);
        connect(server, &TciServer::listenersChanged, this, &RfKitBandFollow::scheduleRefresh);
        connect(server, &TciServer::clientConnected, this, &RfKitBandFollow::scheduleRefresh);
        connect(server, &TciServer::clientDisconnected, this, &RfKitBandFollow::scheduleRefresh);
    }
#else
    Q_UNUSED(server);
#endif
    refresh();
}

void RfKitBandFollow::setInterfaceEntriesForTest(const QList<QNetworkAddressEntry>& entries)
{
    m_entriesForTest = entries;
    refresh();
}

void RfKitBandFollow::scheduleRefresh()
{
    // A client leaving is announced before the server forgets it; look
    // once the event loop has settled.
    if (m_refreshQueued) {
        return;
    }
    m_refreshQueued = true;
    QTimer::singleShot(0, this, [this] {
        m_refreshQueued = false;
        refresh();
    });
}

QHostAddress RfKitBandFollow::addressForAmp(const QList<QHostAddress>& listening,
                                            const QHostAddress& amp,
                                            const QList<QNetworkAddressEntry>& entries)
{
    QHostAddress chosen;
    bool everyAddress = false;
    for (const QHostAddress& raw : listening) {
        const QHostAddress address = StationNetwork::plainIpv4(raw);
        if (address.isLoopback()) {
            continue;
        }
        if (address == QHostAddress(QHostAddress::AnyIPv4)
            || address == QHostAddress(QHostAddress::AnyIPv6)
            || address == QHostAddress(QHostAddress::Any)) {
            everyAddress = true;
            continue;
        }
        // An IPv4 address reads best on the amp's touchscreen.
        if (chosen.isNull() || (chosen.protocol() != QAbstractSocket::IPv4Protocol
                                && address.protocol() == QAbstractSocket::IPv4Protocol)) {
            chosen = address;
        }
    }
    if (!chosen.isNull() || !everyAddress) {
        return chosen;
    }
    const QHostAddress facing = StationNetwork::addressFacing(amp, entries);
    if (!facing.isNull()) {
        return facing;
    }
    for (const QNetworkAddressEntry& entry : entries) {
        if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !entry.ip().isLoopback()) {
            return entry.ip();
        }
    }
    return {};
}

void RfKitBandFollow::refresh()
{
    if (!m_model) {
        return;
    }
#ifdef HAVE_WEBSOCKETS
    if (!m_server || !m_server->isRunning()) {
        m_model->setBandFollow(BandFollow::Off, {}, 0);
        return;
    }
    const int port = m_server->port();
    const QHostAddress amp = StationNetwork::plainIpv4(QHostAddress(m_model->configuredHost()));
    const QList<QNetworkAddressEntry> entries =
        m_entriesForTest ? *m_entriesForTest : StationNetwork::localEntries();
    const QHostAddress address = addressForAmp(m_server->listenAddresses(), amp, entries);
    const QString shown = address.isNull() ? QString() : address.toString();
    // The amp connected as a TCI app follows, whichever address it used.
    if (!amp.isNull()) {
        const auto clients = m_server->clients();
        for (auto it = clients.cbegin(); it != clients.cend(); ++it) {
            QWebSocket* socket = it.key();
            if (socket && socket->state() == QAbstractSocket::ConnectedState
                && StationNetwork::plainIpv4(socket->peerAddress()) == amp) {
                m_model->setBandFollow(BandFollow::Following, shown, port);
                return;
            }
        }
    }
    if (address.isNull()) {
        m_model->setBandFollow(BandFollow::ThisComputerOnly, {}, port);
        return;
    }
    m_model->setBandFollow(BandFollow::Waiting, shown, port);
#else
    m_model->setBandFollow(BandFollow::Off, {}, 0);
#endif
}

} // namespace NereusSDR
