// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/models/RoundtableHub.cpp  (Longpath)
// =================================================================
//
// Siehe RoundtableHub.h.
//
// Modification history (Longpath):
//   2026-11-14 - Angelegt fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude. modeOf() und die
//                Reihenfolge in sliceStates() aus Upstream NereusSDR
//                RadioModel::roundtableModeOf / pushRoundtableSlices
//                (src/models/RadioModel.cpp @ 9036ffc04).
// =================================================================
// no-port-check: NereusSDR-original, in Longpath uebernommen und angepasst.

#include "models/RoundtableHub.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/MoxController.h"
#include "core/roundtable/RoundtableClient.h"
#include "core/roundtable/RoundtableKey.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/SpotModel.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QUrl>

#include <cmath>

namespace Longpath {

namespace {

const QString kSource = QStringLiteral("Roundtable");

QString setting(const char* key, const QString& fallback = QString())
{
    return AppSettings::instance().value(QLatin1String(key), fallback).toString();
}

// Die Familie (Protokoll 6.1), wie die Spot-Liste eine Betriebsart zeigt.
QString spotModeOf(const QString& family)
{
    static const QHash<QString, QString> modes{
        {QStringLiteral("usb"), QStringLiteral("USB")},   {QStringLiteral("lsb"), QStringLiteral("LSB")},
        {QStringLiteral("cw"), QStringLiteral("CW")},     {QStringLiteral("am"), QStringLiteral("AM")},
        {QStringLiteral("fm"), QStringLiteral("FM")},     {QStringLiteral("digu"), QStringLiteral("DIGU")},
        {QStringLiteral("digl"), QStringLiteral("DIGL")}, {QStringLiteral("fdvu"), QStringLiteral("RADE")},
        {QStringLiteral("fdvl"), QStringLiteral("RADE")},
    };
    return modes.value(family, family.toUpper());
}

} // namespace

RoundtableHub::RoundtableHub(QObject* parent)
    : QObject(parent)
    , m_client(std::make_unique<RoundtableClient>())
{
    // Der Schluessel wird erst gebraucht, wenn eine Verbindung der
    // Station aufgeht — vorher entsteht keine Datei.
    m_client->setIdentityProvider([this]() { return key(); });
    connect(m_client.get(), &RoundtableClient::roomsReceived, this, &RoundtableHub::onRooms);
    connect(m_client.get(), &RoundtableClient::roomsDeltaReceived, this,
            &RoundtableHub::onRoomsDelta);
    connect(m_client.get(), &RoundtableClient::linkStateChanged, this,
            &RoundtableHub::onLinkStateChanged);
    connect(m_client.get(), &RoundtableClient::statusTextChanged, this,
            &RoundtableHub::statusChanged);

    // Die Raumliste meldet nur Aenderungen; ohne Auffrischen liefen die
    // Spots einer ruhigen Station nach kSpotLifetimeSec ab.
    m_refresh.setInterval(kSpotRefreshMs);
    connect(&m_refresh, &QTimer::timeout, this, [this]() { syncSpots(true); });
}

RoundtableHub::~RoundtableHub()
{
    // Erst den Client (er schickt beim Abbau noch), dann den Schluessel.
    m_client.reset();
}

// ----------------------------------------------------------------- radio

void RoundtableHub::attachRadio(RadioModel* radio)
{
    m_radio = radio;
    if (radio == nullptr) {
        return;
    }
    for (SliceModel* slice : radio->slices()) {
        watchSlice(slice);
    }
    connect(radio, &RadioModel::sliceAdded, this, [this](int) {
        if (m_radio) {
            for (SliceModel* slice : m_radio->slices()) {
                watchSlice(slice);
            }
        }
        pushSlices();
    });
    connect(radio, &RadioModel::sliceRemoved, this, &RoundtableHub::pushSlices);
    connect(radio, &RadioModel::activeSliceChanged, this, &RoundtableHub::pushSlices);
    if (MoxController* mox = radio->moxController()) {
        // Das Argument, nicht isMox(): das Signal kommt am Ende des
        // Umschaltens, und nur sein Wert ist dann sicher der neue.
        connect(mox, &MoxController::moxStateChanged, this, [this](bool on) {
            m_keyed = on;
            pushSlices();
        });
    }
    pushSlices();
}

void RoundtableHub::watchSlice(SliceModel* slice)
{
    if (slice == nullptr) {
        return;
    }
    connect(slice, &SliceModel::frequencyChanged, this, &RoundtableHub::pushSlices,
            Qt::UniqueConnection);
    connect(slice, &SliceModel::dspModeChanged, this, &RoundtableHub::pushSlices,
            Qt::UniqueConnection);
    connect(slice, &SliceModel::filterChanged, this, &RoundtableHub::pushSlices,
            Qt::UniqueConnection);
    connect(slice, &SliceModel::txSliceChanged, this, &RoundtableHub::pushSlices,
            Qt::UniqueConnection);
}

QString RoundtableHub::modeOf(DSPMode mode)
{
    // Upstream RadioModel.cpp roundtableModeOf [@9036ffc04]; RADE2 gibt es
    // bei uns nicht.
    switch (mode) {
    case DSPMode::LSB:
        return QStringLiteral("LSB");
    case DSPMode::USB:
        return QStringLiteral("USB");
    case DSPMode::DIGL:
        return QStringLiteral("DIGL");
    case DSPMode::DIGU:
        return QStringLiteral("DIGU");
    case DSPMode::AM:
        return QStringLiteral("AM");
    case DSPMode::SAM:
        return QStringLiteral("SAM");
    case DSPMode::FM:
        return QStringLiteral("FM");
    case DSPMode::CWL:
        return QStringLiteral("CWL");
    case DSPMode::CWU:
        return QStringLiteral("CWU");
    case DSPMode::RADE_U:
        return QStringLiteral("FDVU");
    case DSPMode::RADE_L:
        return QStringLiteral("FDVL");
    case DSPMode::DSB:
    case DSPMode::SPEC:
    case DSPMode::DRM:
        break;
    }
    return {};
}

QList<RoundtableSliceState> RoundtableHub::sliceStates(const QList<SliceModel*>& slices,
                                                       SliceModel* active, SliceModel* keyed)
{
    QList<RoundtableSliceState> states;
    const auto add = [&states, keyed](SliceModel* slice) {
        // Ein fremder Empfaenger (KiwiSDR, SunSDR ueber TCI) ist nicht
        // diese Station — er wird nicht gemeldet.
        if (slice == nullptr || slice->suppressAutoStreamBinding()) {
            return;
        }
        RoundtableSliceState state;
        state.id = slice->sliceIndex();
        state.hz = std::llround(slice->frequency());
        state.mode = modeOf(slice->dspMode());
        state.filterHz = qAbs(slice->filterHigh() - slice->filterLow());
        state.keyed = slice == keyed;
        states.append(state);
    };
    // Upstream: die aktive Scheibe zuerst.
    add(active);
    for (SliceModel* slice : slices) {
        if (slice != active) {
            add(slice);
        }
    }
    return states;
}

void RoundtableHub::pushSlices()
{
    if (!m_radio) {
        return;
    }
    SliceModel* keyed = m_keyed ? m_radio->txBoundSlice() : nullptr;
    m_client->setSlices(sliceStates(m_radio->slices(), m_radio->activeSlice(), keyed));
}

// ----------------------------------------------------------------- settings

bool RoundtableHub::enabled() const
{
    return setting("Roundtable/AutoStart", QStringLiteral("True")) == QLatin1String("True");
}

bool RoundtableHub::answered() const
{
    return !setting("Roundtable/FirstRunAnswer").isEmpty();
}

bool RoundtableHub::hidden() const
{
    return setting("Roundtable/Hidden", QStringLiteral("False")) == QLatin1String("True");
}

QString RoundtableHub::callsign() const
{
    QString call = setting("Roundtable/Callsign").trimmed();
    if (call.isEmpty()) {
        call = AppSettings::stationCallsign().trimmed();
    }
    return call.toUpper();
}

QString RoundtableHub::grid() const
{
    QString grid = setting("Roundtable/GridSquare").trimmed();
    if (grid.isEmpty()) {
        grid = AppSettings::stationGridSquare().trimmed();
    }
    return grid;
}

QString RoundtableHub::statusText() const
{
    if (!enabled()) {
        return tr("Off");
    }
    if (!answered()) {
        return tr("Waiting for your answer: nothing is sent until then");
    }
    return m_client->statusText();
}

void RoundtableHub::reloadSettings()
{
    const QString url = setting("Roundtable/ServerUrl").trimmed();
    m_client->setServerUrl(url.isEmpty() ? RoundtableClient::defaultServerUrl() : QUrl(url));
    m_client->setStationInfo(callsign(), grid());
    m_client->setStopWhenAway(setting("Roundtable/AwayChoice", QStringLiteral("Report"))
                              == QLatin1String("Stop"));
    // Erst verborgen, dann die Antwort: wer "verborgen" antwortet, geht
    // nie ueber eine Verbindung der Station (wie Upstreams answerFirstRun).
    m_client->setHidden(hidden());
    m_client->setFirstRunAnswered(answered());
    if (enabled()) {
        const RoundtableClient::LinkState link = m_client->linkState();
        if (link == RoundtableClient::LinkState::Off
            || link == RoundtableClient::LinkState::Offline) {
            m_client->start();
        }
    } else {
        m_client->stop();
        clearStations();
    }
    emit statusChanged();
}

void RoundtableHub::answerFirstRun(bool live)
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("Roundtable/Hidden"),
               live ? QStringLiteral("False") : QStringLiteral("True"));
    s.setValue(QStringLiteral("Roundtable/FirstRunAnswer"),
               live ? QStringLiteral("Live") : QStringLiteral("Hidden"));
    s.save();
    qCInfo(lcSpots) << "Roundtable: Erstfrage beantwortet," << (live ? "live" : "verborgen");
    reloadSettings();
}

void RoundtableHub::setHidden(bool hide)
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("Roundtable/Hidden"),
               hide ? QStringLiteral("True") : QStringLiteral("False"));
    s.save();
    reloadSettings();
}

void RoundtableHub::setEnabled(bool on)
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("Roundtable/AutoStart"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    s.save();
    reloadSettings();
}

// ----------------------------------------------------------------- key

QString RoundtableHub::keyPath() const
{
    if (!m_keyPath.isEmpty()) {
        return m_keyPath;
    }
    return QFileInfo(AppSettings::instance().filePath()).absolutePath()
           + QStringLiteral("/roundtable-identity.pem");
}

const RoundtableKey* RoundtableHub::key()
{
    if (!m_keyTried) {
        m_keyTried = true;
        QString error;
        m_key = std::make_unique<RoundtableKey>(RoundtableKey::loadOrCreate(keyPath(), &error));
        if (!m_key->isValid()) {
            qCWarning(lcSpots) << "Roundtable: Stationsschluessel nicht geladen:" << error;
        }
    }
    return m_key.get();
}

// ----------------------------------------------------------------- rooms

void RoundtableHub::onRooms(const QJsonObject& full)
{
    m_rooms.clear();
    for (const QJsonValue& v : full.value(QStringLiteral("rooms")).toArray()) {
        const QJsonObject room = v.toObject();
        m_rooms.insert(room.value(QStringLiteral("id")).toString(), room);
    }
    rebuildStations();
}

void RoundtableHub::onRoomsDelta(const QJsonObject& delta)
{
    for (const char* list : {"add", "change"}) {
        for (const QJsonValue& v : delta.value(QLatin1String(list)).toArray()) {
            const QJsonObject room = v.toObject();
            m_rooms.insert(room.value(QStringLiteral("id")).toString(), room);
        }
    }
    for (const QJsonValue& v : delta.value(QStringLiteral("remove")).toArray()) {
        m_rooms.remove(v.toString());
    }
    rebuildStations();
}

void RoundtableHub::onLinkStateChanged()
{
    using S = RoundtableClient::LinkState;
    const S state = m_client->linkState();
    // Ohne Verbindung keine Marken: was dort stand, ist nicht mehr sicher.
    if (state != S::Live && state != S::Hidden) {
        clearStations();
        m_refresh.stop();
    } else if (!m_refresh.isActive()) {
        m_refresh.start();
    }
    emit statusChanged();
}

void RoundtableHub::rebuildStations()
{
    const QString own = RoundtableWire::baseCallsign(callsign());
    QHash<QString, Station> next;
    for (const QJsonObject& room : std::as_const(m_rooms)) {
        for (const QJsonValue& v : room.value(QStringLiteral("stations")).toArray()) {
            const QJsonObject st = v.toObject();
            Station s;
            s.callsign = st.value(QStringLiteral("callsign")).toString().trimmed().toUpper();
            if (s.callsign.isEmpty()) {
                continue;
            }
            // Die eigene Station steht nicht als Spot da.
            if (!own.isEmpty() && RoundtableWire::baseCallsign(s.callsign) == own) {
                continue;
            }
            s.key = st.value(QStringLiteral("sourceRoom")).toString() + QLatin1Char('/')
                    + st.value(QStringLiteral("ref")).toString();
            s.hz = st.value(QStringLiteral("hz")).toInteger();
            s.family = st.value(QStringLiteral("family")).toString();
            s.keyed = st.value(QStringLiteral("keyed")).toBool();
            s.away = st.value(QStringLiteral("away")).toBool();
            s.joining = st.value(QStringLiteral("joining")).toBool();
            if (s.hz <= 0) {
                continue;
            }
            next.insert(s.key, s);
        }
    }
    m_stations = next;
    syncSpots(false);
    emit stationsChanged();
}

void RoundtableHub::clearStations()
{
    m_rooms.clear();
    if (m_stations.isEmpty() && m_spotIndex.isEmpty()) {
        return;
    }
    m_stations.clear();
    syncSpots(false);
    emit stationsChanged();
}

void RoundtableHub::syncSpots(bool /*refreshAll*/)
{
    if (!m_spots) {
        return;
    }
    const auto removeOurs = [this](int idx) {
        // Nur entfernen, was noch unser ist: eine Cluster-Meldung derselben
        // Station auf demselben kHz teilt sich den Index (SpotModel-Dedup).
        const auto it = m_spots->spots().constFind(idx);
        if (it != m_spots->spots().constEnd() && it->source == kSource) {
            m_spots->removeSpot(idx);
        }
    };
    for (auto it = m_spotIndex.begin(); it != m_spotIndex.end();) {
        if (!m_stations.contains(it.key())) {
            removeOurs(it.value());
            it = m_spotIndex.erase(it);
        } else {
            ++it;
        }
    }
    const QString color = setting("Roundtable/SpotColor", m_defaultSpotColor);
    for (const Station& s : std::as_const(m_stations)) {
        const double mhz = double(s.hz) / 1.0e6;
        const int idx = m_spots->dedupIndexFor(s.callsign, mhz);
        const auto old = m_spotIndex.constFind(s.key);
        if (old != m_spotIndex.constEnd() && old.value() != idx) {
            removeOurs(old.value());
        }
        QStringList notes{kSource};
        if (s.keyed) {
            notes << tr("on the air");
        }
        if (s.away) {
            notes << tr("away");
        }
        if (s.joining) {
            notes << tr("joining");
        }
        QMap<QString, QString> kvs;
        kvs[QStringLiteral("callsign")] = s.callsign;
        kvs[QStringLiteral("rx_freq")] = QString::number(mhz, 'f', 6);
        kvs[QStringLiteral("tx_freq")] = QString::number(mhz, 'f', 6);
        kvs[QStringLiteral("mode")] = spotModeOf(s.family);
        kvs[QStringLiteral("source")] = kSource;
        kvs[QStringLiteral("spotter_callsign")] = kSource;
        kvs[QStringLiteral("comment")] = notes.join(QStringLiteral(" · "));
        kvs[QStringLiteral("timestamp")] = QString::number(QDateTime::currentSecsSinceEpoch());
        kvs[QStringLiteral("lifetime_seconds")] = QString::number(kSpotLifetimeSec);
        kvs[QStringLiteral("color")] = color;
        m_spots->applySpotStatus(idx, kvs);
        m_spotIndex.insert(s.key, idx);
    }
}

void RoundtableHub::attachSpots(SpotModel* spots)
{
    m_spots = spots;
    syncSpots(true);
}

} // namespace Longpath
