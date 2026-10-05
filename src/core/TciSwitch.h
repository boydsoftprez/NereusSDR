// no-port-check: NereusSDR-original. R-R3-48 the app's one TCI switch.
// J.J. Boyd (KG4VCF), September 2026; AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-48 rework: one switch and one port with the Core's
// station server (operator decision 2026-09-23); the handover removed.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#pragma once

#include "core/NereusCoreExport.h"
#include <QHostAddress>
#include <QObject>
#include <QPointer>

#include <optional>

namespace NereusSDR {

class RadioModel;
class TciServer;

// The app's one TCI switch and port (Setup > CAT & Network > TCI Server).
//
// Operator decision of 2026-09-23: one TCI switch and one port. In a window
// connected to a Core that runs a station TCI server (stationTciVersion 1)
// the switch and port are the Core's: the window shows the Core's station
// switch and port (following a change made by another window or the phone,
// and keeping this computer's TciServerEnabled / TciServerPort settings in
// step), and changing them here sets the Core's (setStationTci). The Core
// keeps its own copy, so its server keeps running for the amp when this
// window closes or another app connects.
//
// At connect the Core's stored switch wins and this window's switch follows
// it. A Core that has no stored station switch yet (the upgrade: a Core and
// this window on one computer with TCI on before) takes this window's
// switch and port instead.
//
// This window's own server follows the switch only when the Core runs on
// another computer. On the Core's own computer the window runs no server:
// the Core's server also listens on this computer and serves apps here.
// Without a Core that offers a station server (a local window, an older
// Core) the window's own server follows the switch as it always has.
//
// With the link down the switch shows the last known state; on the Core's
// computer the window still starts no server (there is no radio here to
// serve), and on another computer its server follows the switch.
class NEREUS_CORE_EXPORT TciSwitch : public QObject {
    Q_OBJECT
public:
    TciSwitch(TciServer* local, RadioModel* model, QObject* parent = nullptr);

    /// The switch changed (or the window started): apply it here and, when
    /// `tellCore`, set the Core's to the same.
    void setSwitch(bool on, quint16 port, const QHostAddress& bindAddress, bool tellCore = true);
    /// The port or bind address changed: restart this window's server if
    /// it runs, and give the Core the new port.
    void setPortOrBind(quint16 port, const QHostAddress& bindAddress);
    /// The link to the Core changed: start or stop this window's server as
    /// the placement rule says.
    void reevaluate();

    bool switchOn() const { return m_on; }
    quint16 port() const { return m_port; }
    /// The Core is on this computer (kept while the link is down).
    bool coreServesThisComputer() const;
    /// Connected to a Core that runs a station TCI server this switch sets.
    bool coreHasStationServer() const;

    /// R-R3-48: the TCI page's line about the Core's station server, in a
    /// remote window connected to a Core that runs one: "Also at the
    /// station: <address>, port <port>". Empty when there is nothing to say
    /// (and while the link is down).
    static QString stationLine(const RadioModel* model);

signals:
    /// A request to the Core could not be sent; `reason` in plain words.
    void stationRequestFailed(const QString& reason);
    /// The switch now shows the Core's switch and port (another window or
    /// the phone changed them); this computer's settings already say so.
    void switchFollowedCore(bool on, quint16 port);

private:
    void applyLocal();
    void tellCore();
    void onStationTciChanged();
    void followCore();
    void syncAtConnect();
    void endRequest();

    QPointer<TciServer> m_local;
    QPointer<RadioModel> m_model;
    bool m_on{false};
    quint16 m_port{50001};
    QHostAddress m_bind{QHostAddress::LocalHost};
    // What this window last asked the Core for; the Core's state is not
    // followed until it says the same (its answer arrives a property at a
    // time), so the switch never flips back to the Core's old value.
    // Cleared when that request is over (accepted or refused), when the
    // link changes and at each new connection, so a request whose echo
    // never matches cannot stop the window following the Core.
    std::optional<std::pair<bool, quint16>> m_asked;
    quint32 m_askedCommandId{0};
    bool m_linkUp{false};   // the link as last seen (for its changes)
    // The Core's object is read once its whole change has arrived.
    bool m_followQueued{false};
    // This connection's start was settled: the Core's stored switch wins,
    // or this window's seeded a Core that had none. Until then nothing is
    // followed.
    bool m_synced{false};
};

} // namespace NereusSDR
