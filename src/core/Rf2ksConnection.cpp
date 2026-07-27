// =================================================================
// src/core/Rf2ksConnection.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native. No upstream port.
//   2026-05-24  Initial implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Patterns mirror src/core/PgxlConnection.{h,cpp}
//                 (which is itself an AetherSDR port); wire format is REST
//                 not C/R/S/V text.
// =================================================================
#include "Rf2ksConnection.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QtGlobal>

namespace NereusSDR {

namespace {

RfKitTunerSnapshot::Mode parseTunerMode(const QString& s) {
    if (s == QStringLiteral("BYPASS"))      { return RfKitTunerSnapshot::Mode::Bypass; }
    if (s == QStringLiteral("MANUAL"))      { return RfKitTunerSnapshot::Mode::Manual; }
    if (s == QStringLiteral("AUTO_TUNING")) { return RfKitTunerSnapshot::Mode::AutoTuning; }
    if (s == QStringLiteral("AUTO"))        { return RfKitTunerSnapshot::Mode::Auto; }
    return RfKitTunerSnapshot::Mode::Unknown;
}

RfKitAntenna::State parseAntennaState(const QString& s) {
    if (s == QStringLiteral("ACTIVE"))   { return RfKitAntenna::State::Active; }
    if (s == QStringLiteral("DISABLED")) { return RfKitAntenna::State::Disabled; }
    return RfKitAntenna::State::Available;
}

RfKitAntenna::Type parseAntennaType(const QString& s) {
    return (s == QStringLiteral("EXTERNAL"))
        ? RfKitAntenna::Type::External
        : RfKitAntenna::Type::Internal;
}

} // namespace

Rf2ksConnection::Rf2ksConnection(QObject* parent)
    : QObject(parent),
      m_nam(std::make_unique<QNetworkAccessManager>())
{
    qRegisterMetaType<RfKitPowerSnapshot>("NereusSDR::RfKitPowerSnapshot");
    qRegisterMetaType<RfKitTunerSnapshot>("NereusSDR::RfKitTunerSnapshot");
    qRegisterMetaType<RfKitAntenna>("NereusSDR::RfKitAntenna");
    qRegisterMetaType<QList<RfKitAntenna>>("QList<NereusSDR::RfKitAntenna>");

    // Own the reconnect timer so disconnect() can cancel a pending retry.
    // Review blocker [P1] on PR #291: scheduleReconnect() previously called
    // m_reconnectTimer.singleShot(...), which is the STATIC
    // QTimer::singleShot invoked through an instance.  That compiles and
    // reads as if it arms this timer, but it creates a detached one-shot,
    // so m_reconnectTimer.stop() in disconnect() was a no-op and a queued
    // reconnect could fire after the operator disabled RF-Kit.
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout,
            this, &Rf2ksConnection::onReconnectTimeout);
}

Rf2ksConnection::~Rf2ksConnection() = default;

void Rf2ksConnection::injectJsonForTesting(const QString& path, const QByteArray& json)
{
    handleResponse(path, json);
}

void Rf2ksConnection::handleResponse(const QString& path, const QByteArray& body)
{
    if (path == QStringLiteral("/info"))                  { parseInfo(body);                 return; }
    if (path == QStringLiteral("/power"))                 { parsePower(body);                return; }
    if (path == QStringLiteral("/tuner"))                 { parseTuner(body);                return; }
    if (path == QStringLiteral("/antennas"))              { parseAntennas(body);             return; }
    if (path == QStringLiteral("/antennas/active"))       { parseActiveAntenna(body);        return; }
    if (path == QStringLiteral("/operate-mode"))          { parseOperateMode(body);          return; }
    if (path == QStringLiteral("/operational-interface")) { parseOperationalInterface(body); return; }
    if (path == QStringLiteral("/data"))                  { parseData(body);                 return; }
}

void Rf2ksConnection::parseInfo(const QByteArray& body)
{
    const QJsonObject o = QJsonDocument::fromJson(body).object();
    m_deviceName = o.value(QStringLiteral("custom_device_name")).toString();
    const QJsonObject sv = o.value(QStringLiteral("software_version")).toObject();
    const int gui = sv.value(QStringLiteral("GUI")).toInt();
    const int ctl = sv.value(QStringLiteral("controller")).toInt();
    m_softwareVersion = QStringLiteral("G%1C%2").arg(gui).arg(ctl);
    emit infoUpdated(o.value(QStringLiteral("device")).toString(),
                     m_softwareVersion, m_deviceName);
}

void Rf2ksConnection::parsePower(const QByteArray& body)
{
    const QJsonObject o = QJsonDocument::fromJson(body).object();
    RfKitPowerSnapshot snap;
    snap.forwardW      = o.value(QStringLiteral("forward")).toObject()
                          .value(QStringLiteral("value")).toInt();
    snap.forwardMaxW   = o.value(QStringLiteral("forward")).toObject()
                          .value(QStringLiteral("max_value")).toInt();
    snap.reflectedW    = o.value(QStringLiteral("reflected")).toObject()
                          .value(QStringLiteral("value")).toInt();
    snap.reflectedMaxW = o.value(QStringLiteral("reflected")).toObject()
                          .value(QStringLiteral("max_value")).toInt();
    snap.swr           = static_cast<float>(o.value(QStringLiteral("swr"))
                          .toObject().value(QStringLiteral("value")).toDouble());
    snap.swrMax        = static_cast<float>(o.value(QStringLiteral("swr"))
                          .toObject().value(QStringLiteral("max_value")).toDouble());
    snap.temperatureC  = static_cast<float>(o.value(QStringLiteral("temperature"))
                          .toObject().value(QStringLiteral("value")).toDouble());
    snap.voltageV      = static_cast<float>(o.value(QStringLiteral("voltage"))
                          .toObject().value(QStringLiteral("value")).toDouble());
    snap.currentA      = static_cast<float>(o.value(QStringLiteral("current"))
                          .toObject().value(QStringLiteral("value")).toDouble());
    m_lastPower = snap;
    emit powerUpdated(snap);
}

void Rf2ksConnection::parseTuner(const QByteArray& body)
{
    const QJsonObject o = QJsonDocument::fromJson(body).object();
    RfKitTunerSnapshot snap;
    snap.mode              = parseTunerMode(o.value(QStringLiteral("mode")).toString());
    snap.setup             = o.value(QStringLiteral("setup")).toString();
    snap.lValuenH          = o.value(QStringLiteral("L")).toObject()
                              .value(QStringLiteral("value")).toInt();
    snap.cValuepF          = o.value(QStringLiteral("C")).toObject()
                              .value(QStringLiteral("value")).toInt();
    snap.tunedFrequencyKHz = o.value(QStringLiteral("tuned_frequency")).toObject()
                              .value(QStringLiteral("value")).toInt();
    snap.segmentSizeKHz    = o.value(QStringLiteral("segment_size")).toObject()
                              .value(QStringLiteral("value")).toInt();
    m_lastTuner = snap;
    emit tunerUpdated(snap);
}

void Rf2ksConnection::parseAntennas(const QByteArray& body)
{
    const QJsonArray arr = QJsonDocument::fromJson(body).object()
                            .value(QStringLiteral("antennas")).toArray();
    QList<RfKitAntenna> list;
    for (const auto& v : arr) {
        const QJsonObject o = v.toObject();
        RfKitAntenna a;
        a.type   = parseAntennaType(o.value(QStringLiteral("type")).toString());
        a.number = o.value(QStringLiteral("number")).toInt();
        a.state  = parseAntennaState(o.value(QStringLiteral("state")).toString());
        list.push_back(a);
    }
    m_antennas = list;
    emit antennasUpdated(list);
}

void Rf2ksConnection::parseActiveAntenna(const QByteArray& body)
{
    const QJsonObject o = QJsonDocument::fromJson(body).object();
    RfKitAntenna a;
    a.type   = parseAntennaType(o.value(QStringLiteral("type")).toString());
    a.number = o.value(QStringLiteral("number")).toInt();
    a.state  = RfKitAntenna::State::Active;
    m_active = a;
    emit activeAntennaUpdated(a);
}

void Rf2ksConnection::parseOperateMode(const QByteArray& body)
{
    const QString mode = QJsonDocument::fromJson(body).object()
                          .value(QStringLiteral("operate_mode")).toString();
    m_operateMode = mode;
    emit operateModeUpdated(mode);
}

void Rf2ksConnection::parseOperationalInterface(const QByteArray& body)
{
    const QJsonObject o = QJsonDocument::fromJson(body).object();
    m_opIfx           = o.value(QStringLiteral("operational_interface")).toString();
    m_opIfxErrorField = o.value(QStringLiteral("error")).toString();
    emit operationalInterfaceUpdated(m_opIfx, m_opIfxErrorField);
}

void Rf2ksConnection::parseData(const QByteArray& body)
{
    const QJsonObject o = QJsonDocument::fromJson(body).object();
    const int bandM      = o.value(QStringLiteral("band")).toObject()
                            .value(QStringLiteral("value")).toInt();
    const int freqKHz    = static_cast<int>(o.value(QStringLiteral("frequency"))
                            .toObject().value(QStringLiteral("value")).toDouble());
    const QString status = o.value(QStringLiteral("status")).toString();
    emit dataUpdated(bandM, freqKHz, status);
}

void Rf2ksConnection::connectToAmp(const QString& host, quint16 port)
{
    m_host = host;
    m_port = port;
    m_consecutiveFailures = 0;
    m_reconnectBackoffMs = 500;  // doubled to 1000 ms on first scheduleReconnect()

    connect(&m_pollTimer, &QTimer::timeout, this, &Rf2ksConnection::pollOnce,
            Qt::UniqueConnection);
    // 2026-05-25 KG4VCF bench fix: pollOnce now staggers one path per
    // tick (was: 6 parallel GETs per tick).  Run the timer at one-sixth
    // of m_pollIntervalMs so each path is still sampled at the same
    // overall rate, but the reply burst on the main thread is spread
    // out instead of producing a visible ~1 Hz hitch in the waterfall.
    m_pollTimer.start(qMax(1, m_pollIntervalMs / 6));

    // Probe /info immediately to establish connection.
    issueGet(QStringLiteral("/info"));
}

void Rf2ksConnection::disconnect()
{
    m_pollTimer.stop();
    m_reconnectTimer.stop();
    if (m_connected) {
        m_connected = false;
        emit disconnected();
    }
}

void Rf2ksConnection::setPollIntervalMs(int ms)
{
    m_pollIntervalMs = qBound(250, ms, 5000);
    if (m_pollTimer.isActive()) {
        // See connectToAmp(): the timer fires per-path, not per-cycle,
        // so the interval is one-sixth of the configured cycle period.
        m_pollTimer.start(qMax(1, m_pollIntervalMs / 6));
    }
}

void Rf2ksConnection::pollOnce()
{
    // 2026-05-25 KG4VCF bench fix: stagger the 6 hot-path GETs across
    // the poll interval instead of firing them all at once.
    //
    // Original behavior: 6 parallel GETs every 1000 ms.  All 6 QNetworkReply
    // finished callbacks deliver back to the main thread within a few ms;
    // the resulting paint-event starvation produced a visible ~1 Hz hitch
    // in the waterfall scroll (bench report: "still seems to have about a
    // 1 second hitch in the flow").
    //
    // New behavior: pollOnce fires ONE path per tick, round-robin.  The
    // timer interval is divided by 6 so each path still gets sampled at
    // the same overall rate.  Replies arrive spread out -> no
    // main-thread spike.
    //
    // The slow-rotation /antennas + /info refresh now also rides this
    // schedule (counter wraps every 60 hot-path ticks).
    static const QStringList kHotPaths{
        QStringLiteral("/power"),
        QStringLiteral("/tuner"),
        QStringLiteral("/data"),
        QStringLiteral("/antennas/active"),
        QStringLiteral("/operate-mode"),
        QStringLiteral("/operational-interface"),
    };
    static int hotIdx = 0;
    static int slowTick = 0;
    issueGet(kHotPaths.at(hotIdx));
    hotIdx = (hotIdx + 1) % kHotPaths.size();
    // Slow rotation: once per full hot-path cycle (~ once per poll
    // interval), advance the slow-tick counter and refresh /antennas
    // + /info every 10 cycles.
    if (hotIdx == 0) {
        if (++slowTick % 10 == 0) {
            issueGet(QStringLiteral("/antennas"));
            issueGet(QStringLiteral("/info"));
        }
    }
}

void Rf2ksConnection::issueGet(const QString& path)
{
    if (m_host.isEmpty()) {
        return;
    }
    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(m_host);
    url.setPort(m_port);
    url.setPath(path);
    QNetworkRequest req(url);
    // Disable HTTP keep-alive on the polling path.  Without this, QNAM caches
    // the TCP connection for ~120 s; when the amp closes a kept-alive socket
    // after its idle timeout, Qt's macOS CFSocket source can fire into a
    // QSocketNotifier whose private has already been torn down, crashing in
    // qt_mac_socket_callback -> QCoreApplication::sendEvent with a wild
    // receiver pointer.  Forcing connection close after each reply costs ~1
    // ms on LAN and eliminates the stale-socket race.
    req.setRawHeader("Connection", "close");
    auto* reply = m_nam->get(req);
    reply->setProperty("rfkitPath", path);
    reply->setProperty("startedMs", QDateTime::currentMSecsSinceEpoch());
    connect(reply, &QNetworkReply::finished,
            this, &Rf2ksConnection::onReplyFinished);
}

void Rf2ksConnection::onReplyFinished()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    const QString path    = reply->property("rfkitPath").toString();
    const qint64 started  = reply->property("startedMs").toLongLong();
    const bool isWrite    = reply->property("rfkitIsWrite").toBool();
    const int rttMs       = static_cast<int>(QDateTime::currentMSecsSinceEpoch() - started);

    if (reply->error() != QNetworkReply::NoError) {
        markPollFailure();
        reply->deleteLater();
        return;
    }
    const QByteArray body = reply->readAll();
    reply->deleteLater();

    // Review blocker [P2] on PR #291: only GET replies carry state.  The amp
    // answers a write with Content-Length: 0, and QJsonDocument::fromJson("")
    // yields an empty object -- so parsing a write ack as state published
    // operateModeUpdated("") and antenna number 0, clobbering the cached
    // values with the reply to the very command that had just changed them.
    // A write ack is still proof of liveness, so it keeps feeding
    // markPollSuccess() and the connected transition below.
    if (!isWrite) {
        handleResponse(path, body);
    }
    markPollSuccess(rttMs);

    if (!m_connected) {
        m_connected = true;
        m_connectedSinceMs = QDateTime::currentMSecsSinceEpoch();
        emit connected();
    }
}

void Rf2ksConnection::markPollSuccess(int rttMs)
{
    m_pollsSucceeded++;
    m_consecutiveFailures = 0;
    m_reconnectBackoffMs = 500;  // doubled to 1000 ms on next scheduleReconnect()
    m_lastPollMs = QDateTime::currentMSecsSinceEpoch();
    m_rttAvgMs = (m_rttAvgMs * 9 + rttMs) / 10;
}

void Rf2ksConnection::markPollFailure()
{
    m_pollsFailed++;
    m_consecutiveFailures++;
    if (m_consecutiveFailures >= 3 && m_connected) {
        m_connected = false;
        // Stop polling the amp we just declared down.  Without this the
        // poll timer kept firing at the configured cadence against a dead
        // endpoint, which made the exponential backoff decorative -- the
        // retry schedule stretched to 60 s while the poller carried on
        // hammering every few hundred ms.  onReconnectTimeout() restarts
        // the timer when a probe succeeds.  Codex review, PR #291.
        m_pollTimer.stop();
        emit disconnected();
        scheduleReconnect();
    }
}

void Rf2ksConnection::scheduleReconnect()
{
    // Review blocker [P2] on PR #291: the operator's "Auto-reconnect" choice
    // was persisted by RfKitPage and never consulted here, so the connection
    // retried unconditionally regardless of the checkbox.
    if (!m_autoReconnect) {
        return;
    }
    m_reconnectAttempts++;
    // Double first, then schedule - so the member always holds the delay
    // that was just used.  testCurrentBackoffMs() reads this value and the
    // test verifies the 1 s / 2 s / 4 s / 8 s ... 60 s sequence.
    m_reconnectBackoffMs = qMin(m_reconnectBackoffMs * 2, 60000);
    m_reconnectTimer.start(m_reconnectBackoffMs);
}

void Rf2ksConnection::onReconnectTimeout()
{
    // Re-issue /info as the connection probe; success path will set
    // m_connected back to true via onReplyFinished.
    issueGet(QStringLiteral("/info"));
    if (!m_pollTimer.isActive() && !m_host.isEmpty()) {
        // See connectToAmp(): timer fires per-path, one-sixth of cycle.
        m_pollTimer.start(qMax(1, m_pollIntervalMs / 6));
    }
}

void Rf2ksConnection::testForceBackoffSequence()
{
    scheduleReconnect();
    m_reconnectTimer.stop();   // cancel actual reconnect; we only wanted the math
}

void Rf2ksConnection::testForceBackoffReset()
{
    m_reconnectBackoffMs = 1000;
    m_consecutiveFailures = 0;
}

// Control / IO verbs (Task 5).

void Rf2ksConnection::setActiveAntenna(RfKitAntenna::Type type, int number)
{
    const QString typeStr = (type == RfKitAntenna::Type::Internal)
                              ? QStringLiteral("INTERNAL")
                              : QStringLiteral("EXTERNAL");
    const QJsonObject o{
        { QStringLiteral("type"),   typeStr },
        { QStringLiteral("number"), number  },
    };
    issuePut(QStringLiteral("/antennas/active"),
             QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void Rf2ksConnection::setOperateMode(const QString& mode)
{
    const QJsonObject o{ { QStringLiteral("operate_mode"), mode } };
    issuePut(QStringLiteral("/operate-mode"),
             QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void Rf2ksConnection::setOperationalInterface(const QString& iface)
{
    const QJsonObject o{ { QStringLiteral("operational_interface"), iface } };
    issuePut(QStringLiteral("/operational-interface"),
             QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void Rf2ksConnection::resetError()
{
    issuePost(QStringLiteral("/error/reset"));
}

void Rf2ksConnection::issuePut(const QString& path, const QByteArray& body)
{
    if (m_host.isEmpty()) {
        return;
    }
    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(m_host);
    url.setPort(m_port);
    url.setPath(path);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));
    // See issueGet() for the rationale on disabling HTTP keep-alive.
    req.setRawHeader("Connection", "close");
    auto* reply = m_nam->sendCustomRequest(req, "PUT", body);
    reply->setProperty("rfkitPath", path);
    reply->setProperty("startedMs", QDateTime::currentMSecsSinceEpoch());
    // See onReplyFinished(): write acks carry no state body and must not be
    // fed to handleResponse().
    reply->setProperty("rfkitIsWrite", true);
    connect(reply, &QNetworkReply::finished,
            this, &Rf2ksConnection::onReplyFinished);
}

void Rf2ksConnection::issuePost(const QString& path)
{
    if (m_host.isEmpty()) {
        return;
    }
    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(m_host);
    url.setPort(m_port);
    url.setPath(path);
    QNetworkRequest req(url);
    // See issueGet() for the rationale on disabling HTTP keep-alive.
    req.setRawHeader("Connection", "close");
    auto* reply = m_nam->post(req, QByteArray());
    reply->setProperty("rfkitPath", path);
    reply->setProperty("startedMs", QDateTime::currentMSecsSinceEpoch());
    // See onReplyFinished(): write acks carry no state body and must not be
    // fed to handleResponse().
    reply->setProperty("rfkitIsWrite", true);
    connect(reply, &QNetworkReply::finished,
            this, &Rf2ksConnection::onReplyFinished);
}

} // namespace NereusSDR
