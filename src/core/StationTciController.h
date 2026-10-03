// no-port-check: NereusSDR-original. R-R3-48 / R-R3-25 the Core's station TCI server.
// J.J. Boyd (KG4VCF), September 2026; AI-assisted via Anthropic Claude Code.
// 2026-09-27: Parity Task 23 (R-R3-48, R-R3-42, R-R3-49): the server's apps
// (the `tciClients` stream), disconnecting one, and its four options. J.J.
// Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#pragma once

#include "core/StationNetwork.h"
#include "models/StationTciModel.h"

#include <QHash>
#include <QHostAddress>
#include <QList>
#include <QNetworkAddressEntry>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <memory>
#include <optional>

namespace NereusSDR {

class RadioModel;
class TciServer;

// The Core runs the app's existing TciServer on its own radio model, so
// devices on the station network (the RF-Kit RF2K-S first) follow the
// Core's radio. One switch and port: the window's TCI switch sends them
// with the setStationTci command, and the Core keeps them in its own
// settings (StationTci_Enabled, StationTci_Port) across window sessions,
// other apps connecting, and restarts.
//
// Where it listens: the station network only, plus this computer (so apps
// on the Core's own computer reach it, and a window on the same computer
// needs no server of its own), by the one rule every station listener
// follows (StationNetwork::StationBind): nereusd.conf's station_bind
// (older name station_tci_bind); else this computer's address on the
// radio's subnet once the radio is known. Before either is known the
// server listens on this computer only. A bind to every address
// (0.0.0.0) is used as given and covers this computer too.
//
// The server transmits for no app until remote transmit (R-R3-25): see
// TciServer::setStationReceiveOnly.
//
// The station address and this computer are listened on separately (rework
// part 3): what binds is kept and served, and only an address another
// program holds is retried, with a bounded backoff (1, 2, 5, 10, then every
// 30 s) while the switch is on, without stopping and starting the server.
// The object says which address is blocked, in plain words, meanwhile, and
// the log has one line when it starts failing and one when it listens on
// everything again, not one per try (the server's own per-attempt lines are
// at debug level). So the RF-Kit keeps band follow while
// the station address is up, whatever holds this computer's port.
class StationTciController : public QObject {
    Q_OBJECT
public:
    /// The station switch's default port: the app's TCI default
    /// (CatTciServerPage's TciServerPort, 50001).
    static constexpr quint16 kDefaultPort = 50001;
    /// The settings keys the Core keeps its switch in.
    static QString enabledKey() { return QStringLiteral("StationTci_Enabled"); }
    static QString portKey() { return QStringLiteral("StationTci_Port"); }

    StationTciController(RadioModel* radio, StationTciModel* model, QObject* parent = nullptr);
    ~StationTciController() override;

    /// nereusd.conf station_bind (or station_tci_bind): empty chooses the
    /// station network from the radio's address.
    void setBindOverride(const QString& address);
    /// The radio's address: the station network is the subnet holding it.
    void setRadioAddress(const QHostAddress& radio);
    /// Test seam: this computer's address entries (default: the live ones).
    void setInterfaceEntriesForTest(const QList<QNetworkAddressEntry>& entries);

    /// Read the saved switch and port and apply them (the Core's start).
    void applySaved();
    /// The station's TCI switch and port, saved and applied. Refused (with
    /// a plain reason) for a port outside 1024 to 65535.
    bool setEnabled(bool enabled, int port, QString* reason);

    /// The plain reason the object carries while `blocked` addresses cannot
    /// be listened on (another program holds the port there).
    static QString blockedReason(quint16 port, const QList<QHostAddress>& blocked);
    /// The retry delays, ms; the last repeats.
    static constexpr int kRetryDelaysMs[] = {1000, 2000, 5000, 10000, 30000};

    /// The addresses the server listens on when on.
    QList<QHostAddress> wantedAddresses() const;

    TciServer* server() const;

    /// Parity Task 23 (stationTciVersion 2): the server's options, the
    /// Core's own TciEmulateExpertSDR3Protocol, TciEmulateSunSDR2Pro,
    /// TciCwluBecomesCw and TciSendInitialFrequencyStateOnConnect, saved
    /// and published. An app connecting later reads them in its init burst.
    void setOptions(bool emulateExpertSdr3, bool emulateSunSdr2Pro, bool cwluBecomesCw,
                    bool sendInitialState);
    /// JJ's ruling of 2026-09-28 (stationTciSettingsVersion 1): the rest of
    /// the TCI Server page's settings for this server, by property name
    /// (StationTciModel::settingsTable()), saved and published; the rate
    /// limit, "always stream IQ" and the TX channel reach the running server
    /// at once, the others as the server next reads them (the sensor
    /// intervals when an app connects). Every name must be known and
    /// every value in its range, or nothing changes and `reason` says why.
    bool setSettings(const QVariantMap& changes, QString* reason);
    /// Parity Task 23: closes the app the `tciClients` record `id` names.
    /// False, with a plain reason, when no app on the server has that id.
    bool disconnectClient(const QString& id, QString* reason);
    static QString unknownClientReason();

private:
    void apply();
    void publish();
    void publishClients();
    void resetRetry();

    QPointer<RadioModel> m_radio;
    QPointer<StationTciModel> m_model;
#ifdef HAVE_WEBSOCKETS
    std::unique_ptr<TciServer> m_server;
#endif
    StationNetwork::StationBind m_bind;
    bool m_enabled{false};
    quint16 m_port{kDefaultPort};
    QList<QHostAddress> m_listening;   // bound now
    QList<QHostAddress> m_wanted;      // the addresses this server is for
    QList<QHostAddress> m_blocked;     // wanted, not bound (retried)
    QString m_error;
    QTimer m_retryTimer;
    int m_retryStep{0};
    bool m_failing{false};
    // Parity Task 23: each connection's `tciClients` id, a rising number.
    QHash<const void*, QString> m_clientIds;
    quint64 m_nextClientId{1};
    bool m_clientsQueued{false};
};

} // namespace NereusSDR
