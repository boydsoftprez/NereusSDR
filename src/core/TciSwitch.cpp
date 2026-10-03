// no-port-check: NereusSDR-original. R-R3-48 the app's one TCI switch.
// J.J. Boyd (KG4VCF), September 2026; AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-48 rework: one switch and one port with the Core's
// station server (operator decision 2026-09-23); the handover removed.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-48 rework follow-up: a request's wait ends with its
// answer, a link change or a new connection. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
#include "core/TciSwitch.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/TciServer.h"
#include "core/session/IStationLink.h"
#include "models/RadioModel.h"
#include "models/StationTciModel.h"

namespace NereusSDR {

TciSwitch::TciSwitch(TciServer* local, RadioModel* model, QObject* parent)
    : QObject(parent), m_local(local), m_model(model)
{
    if (model) {
        connect(model, &RadioModel::stationLinkStateChanged, this, &TciSwitch::reevaluate);
        // Rework follow-up 1: the Core answered this window's request.
        connect(model, &RadioModel::stationCommandFinished, this,
                [this](quint32 commandId, bool, const QString&) {
            if (commandId != 0 && commandId == m_askedCommandId) {
                endRequest();
                onStationTciChanged();
            }
        });
        // The Core's settings arriving tells whether it keeps a switch.
        connect(model, &RadioModel::stationSettingChanged, this, [this](const QString& key) {
            if (key.isEmpty() || key.startsWith(QLatin1String("StationTci_"))) {
                syncAtConnect();
                onStationTciChanged();
            }
        });
        if (StationTciModel* station = model->stationTciModel()) {
            connect(station, &StationTciModel::stateChanged,
                    this, &TciSwitch::onStationTciChanged);
        }
    }
}

void TciSwitch::onStationTciChanged()
{
    // A change arrives one property at a time; read the whole state once
    // this turn of the event loop has delivered it. Posted at the first
    // property, so it runs before anything posted later for the same change
    // (the TCI page reads this computer's settings after it).
    if (!m_followQueued) {
        m_followQueued = true;
        QMetaObject::invokeMethod(this, [this] {
            m_followQueued = false;
            followCore();
        }, Qt::QueuedConnection);
    }
}

void TciSwitch::endRequest()
{
    m_asked.reset();
    m_askedCommandId = 0;
}

void TciSwitch::syncAtConnect()
{
    // Rework follow-up 1: the link going down or coming up (a new
    // connection) ends any request in flight: its echo may never come.
    // Repeated reports of the same link state do not.
    const bool up = coreHasStationServer();
    if (up != m_linkUp) {
        m_linkUp = up;
        endRequest();
        m_synced = false;   // a new connection settles again
    }
    if (!up) {
        return;
    }
    if (m_synced) {
        return;
    }
    const int stored = m_model->stationLink()->coreStationTciStored();
    if (stored < 0) {
        return;   // the Core's settings have not arrived yet
    }
    m_synced = true;
    if (stored == 0) {
        // The Core has no station switch yet: this window's seeds it.
        qCInfo(lcTci) << "The Core keeps no TCI switch yet; giving it this window's"
                      << (m_on ? "on" : "off") << "port" << m_port;
        tellCore();
    }
}

void TciSwitch::followCore()
{
    syncAtConnect();
    if (!coreHasStationServer() || !m_synced) {
        return;
    }
    const StationTciModel* station = m_model->stationTciModel();
    if (!station || station->port() <= 0) {
        return;   // the Core's object has not arrived yet
    }
    const bool on = station->enabled();
    const quint16 port = static_cast<quint16>(station->port());
    if (m_asked) {
        if (m_asked->first != on || m_asked->second != port) {
            return;   // the Core has not taken this window's request yet
        }
        endRequest();
    }
    if (on == m_on && port == m_port) {
        return;
    }
    m_on = on;
    m_port = port;
    // This computer's own switch and port show the Core's (the TCI page
    // reads them), so a later window start or a lost link shows the last
    // known state.
    auto& settings = AppSettings::instance();
    settings.setValue(QStringLiteral("TciServerEnabled"),
                      on ? QStringLiteral("True") : QStringLiteral("False"));
    settings.setValue(QStringLiteral("TciServerPort"), QString::number(port));
#ifdef HAVE_WEBSOCKETS
    if (m_local && m_local->isRunning() && m_local->port() != m_port) {
        m_local->stop();
    }
#endif
    applyLocal();
    emit switchFollowedCore(on, port);
}

bool TciSwitch::coreHasStationServer() const
{
    const IStationLink* link = m_model ? m_model->stationLink() : nullptr;
    return m_model && m_model->role() == RadioModel::Role::Remote && link
        && link->stationTciAvailable();
}

bool TciSwitch::coreServesThisComputer() const
{
    const IStationLink* link = m_model ? m_model->stationLink() : nullptr;
    return m_model && m_model->role() == RadioModel::Role::Remote && link
        && link->coreServesTciOnThisComputer();
}

QString TciSwitch::stationLine(const RadioModel* model)
{
    if (!model || model->role() != RadioModel::Role::Remote) {
        return {};
    }
    const IStationLink* link = model->stationLink();
    const StationTciModel* station = model->stationTciModel();
    // Rework part 4: nothing to say about a Core the link cannot reach.
    if (!link || !link->stationLinkReady()) {
        return {};
    }
    if (!station
        || (!link->coreServesTciOnThisComputer() && !link->stationTciAvailable())) {
        return {};
    }
    if (!station->enabled()) {
        return {};
    }
    if (!station->listening()) {
        return QStringLiteral("The Core's TCI server is not running.");
    }
    if (link->coreServesTciOnThisComputer()) {
        return QStringLiteral("The Core on this computer serves TCI apps here, port %1.")
            .arg(station->port());
    }
    if (station->stationAddress().isEmpty()) {
        return QStringLiteral("Also at the Core, port %1, for apps on the Core's computer.")
            .arg(station->port());
    }
    return QStringLiteral("Also at the Core: %1, port %2")
        .arg(station->stationAddress()).arg(station->port());
}

void TciSwitch::setSwitch(bool on, quint16 port, const QHostAddress& bindAddress, bool tell)
{
    m_on = on;
    m_port = port;
    m_bind = bindAddress;
    if (tell) {
        tellCore();
    }
    applyLocal();
}

void TciSwitch::setPortOrBind(quint16 port, const QHostAddress& bindAddress)
{
    const bool portChanged = port != m_port;
    m_port = port;
    m_bind = bindAddress;
#ifdef HAVE_WEBSOCKETS
    if (m_local && m_local->isRunning()) {
        m_local->stop();
    }
#endif
    if (portChanged) {
        tellCore();
    }
    applyLocal();
}

void TciSwitch::reevaluate()
{
    syncAtConnect();
    applyLocal();
    onStationTciChanged();
}

void TciSwitch::applyLocal()
{
#ifdef HAVE_WEBSOCKETS
    if (!m_local) {
        return;
    }
    // On the Core's own computer the Core's server serves apps here; with
    // the link down there is no radio here to serve either, so the window
    // starts none (rework part 4). On another computer the window's server
    // follows the switch, link or not.
    const bool wanted = m_on && !coreServesThisComputer();
    if (wanted && !m_local->isRunning()) {
        m_local->start(m_bind, m_port);
    } else if (!wanted && m_local->isRunning()) {
        qCInfo(lcTci) << "The Core on this computer serves TCI here;"
                      << "this window runs no TCI server of its own";
        m_local->stop();
    }
#endif
}

void TciSwitch::tellCore()
{
    if (!coreHasStationServer()) {
        return; // An older Core (or no link): this window's server only.
    }
    // The link is up (this request goes over it): note it now, so its
    // first report later is not taken as a new connection ending this wait.
    if (!m_linkUp) {
        m_linkUp = true;
        m_synced = false;
    }
    IStationLink* link = m_model->stationLink();
    const auto outcome = link->requestStationTci(m_on, m_port);
    if (!outcome.sent) {
        emit stationRequestFailed(outcome.reason);
        return;
    }
    m_asked = std::make_pair(m_on, m_port);
    m_askedCommandId = outcome.commandId;
}

} // namespace NereusSDR
