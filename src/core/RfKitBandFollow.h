// no-port-check: NereusSDR-original. R-R3-48 RF-Kit band follow over TCI.
// J.J. Boyd (KG4VCF), September 2026; AI-assisted via Anthropic Claude Code.
#pragma once

#include "models/RfKitModel.h"

#include <QHostAddress>
#include <QList>
#include <QNetworkAddressEntry>
#include <QObject>
#include <QPointer>

#include <optional>

namespace NereusSDR {

class TciServer;

// Whether the RF-Kit RF2K-S follows the radio's band, published into an
// RfKitModel. The amp follows as a TCI client of a TciServer: the Core's
// station server on the Core, the window's own server in a local window.
//
//   - the server is off: Off;
//   - a TCI app is connected from the amp's address: Following;
//   - it listens only on its own computer (loopback): ThisComputerOnly;
//   - otherwise: Waiting, with the address and port to enter on the amp
//     (the server's network address, or this computer's address facing
//     the amp when the server listens on every address).
//
// A host name for the amp never matches an app's address, so such an amp
// reads Waiting even while it follows; the address line is still right.
class RfKitBandFollow : public QObject {
    Q_OBJECT
public:
    explicit RfKitBandFollow(RfKitModel* model, QObject* parent = nullptr);

    /// The server the amp connects to (nullptr: none, band follow Off).
    void setServer(TciServer* server);
    /// Test seam: this computer's address entries (default: the live ones).
    void setInterfaceEntriesForTest(const QList<QNetworkAddressEntry>& entries);

    /// Recompute now (the server's and the model's changes also do).
    void refresh();

    /// The address to enter on the amp, given where the server listens.
    /// Null when the server listens only on its own computer.
    static QHostAddress addressForAmp(const QList<QHostAddress>& listening,
                                      const QHostAddress& amp,
                                      const QList<QNetworkAddressEntry>& entries);

private:
    void scheduleRefresh();

    QPointer<RfKitModel> m_model;
    QPointer<TciServer> m_server;
    std::optional<QList<QNetworkAddressEntry>> m_entriesForTest;
    bool m_refreshQueued{false};
};

} // namespace NereusSDR
