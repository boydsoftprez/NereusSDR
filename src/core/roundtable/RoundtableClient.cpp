// no-port-check: NereusSDR-original.
// =================================================================
// src/core/roundtable/RoundtableClient.cpp  (NereusSDR)
// =================================================================
//
// See RoundtableClient.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 10: R-RT-6, R-RT-12,
//               R-RT-13, R-RT-14 Core side).
//   2026-10-24: Roundtable plan Task 11 fix round: syncWatches() keeps
//               the live reader watching its slices' rooms; an archive
//               action waits for the station connection when that is
//               wanted, and the station connection waits for the archive
//               connection to finish (3.1: never two at once). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-24: Roundtable plan Task 15: setStationInfo() starts the
//               station connection again with the new callsign or grid.
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include "core/roundtable/RoundtableClient.h"

#include "core/security/StationIdentity.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTimer>
#include <QWebSocket>

#include <algorithm>
#include <chrono>
#include <cmath>

Q_LOGGING_CATEGORY(lcRoundtable, "nereus.roundtable")

namespace NereusSDR {

namespace {

#ifdef NEREUSSDR_VERSION
const QString kSoftwareVersion = QStringLiteral(NEREUSSDR_VERSION);
#else
const QString kSoftwareVersion = QStringLiteral("0");
#endif

// Section 2.4's close codes a Core acts on.
constexpr int kCloseSignIn = 4001;
constexpr int kCloseProtocol = 4002;
constexpr int kCloseReplaced = 4003;
constexpr int kCloseRevoked = 4004;

// Section 6.5: what a reading's dbm may be (5.2).
constexpr double kMinDbm = -160.0;
constexpr double kMaxDbm = 94.0;
constexpr qint64 kMaxHz = 99999999999LL;
constexpr int kMaxFilterHz = 1000000;
constexpr int kMaxSliceId = 255;

bool isLoopbackHost(const QString& host)
{
    if (host.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    const QHostAddress address(host);
    return !address.isNull() && address.isLoopback();
}

bool usableServerUrl(const QUrl& url)
{
    if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty()) {
        return false;
    }
    return url.scheme() == QLatin1String("wss")
        || (url.scheme() == QLatin1String("ws") && isLoopbackHost(url.host()));
}

bool sameSlices(const QList<RoundtableSliceState>& a, const QList<RoundtableSliceState>& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (qsizetype i = 0; i < a.size(); ++i) {
        if (a[i].id != b[i].id || a[i].hz != b[i].hz || a[i].mode != b[i].mode
            || a[i].filterHz != b[i].filterHz || a[i].keyed != b[i].keyed) {
            return false;
        }
    }
    return true;
}

QString shortId(const QString& id)
{
    return id.left(6);
}

} // namespace

const QList<int>& RoundtableClient::reconnectDelaysMs()
{
    static const QList<int> delays{1000, 2000, 5000, 10000, 30000};
    return delays;
}

QUrl RoundtableClient::defaultServerUrl()
{
    return QUrl(QStringLiteral("wss://roundtable.nereussdr.com/v1/station"));
}

QString RoundtableClient::profileDirectoryFor(const QString& settingsFilePath)
{
    return QDir::cleanPath(QFileInfo(QFileInfo(settingsFilePath).absolutePath()).absoluteFilePath());
}

RoundtableClient::RoundtableClient(QObject* parent)
    : QObject(parent)
    , m_serverUrl(defaultServerUrl())
    , m_software(QStringLiteral("NereusSDR"))
    , m_softwareVersion(kSoftwareVersion)
{
    m_tick = new QTimer(this);
    m_tick->setInterval(kTickMs);
    connect(m_tick, &QTimer::timeout, this, &RoundtableClient::serviceTimers);
}

RoundtableClient::~RoundtableClient()
{
    m_started = false;
    closeLink(m_main, true);
    closeLink(m_archive, true);
}

// ----------------------------------------------------------------- settings

void RoundtableClient::setServerUrl(const QUrl& url)
{
    if (!usableServerUrl(url)) {
        qCWarning(lcRoundtable) << "Not a Roundtable server address (wss://, or ws:// to this computer):"
                                << url.host();
        return;
    }
    if (url == m_serverUrl) {
        return;
    }
    m_serverUrl = url;
    clearOffline();
    if (m_main.socket) {
        closeLink(m_main, true);
    }
    reconcile();
}

QUrl RoundtableClient::liveUrl() const
{
    QUrl url = m_serverUrl;
    QString path = url.path();
    if (path.endsWith(QLatin1String("/station"))) {
        path.chop(static_cast<int>(qstrlen("station")));
        path.append(QLatin1String("live"));
    } else {
        path = QStringLiteral("/v1/live");
    }
    url.setPath(path);
    return url;
}

void RoundtableClient::setIdentity(const StationIdentity* identity)
{
    if (identity == m_identity && !m_identityProvider) {
        return;
    }
    m_identity = identity;
    m_identityProvider = {};
    clearOffline();
    reconcile();
}

void RoundtableClient::setIdentityProvider(IdentityProvider provider)
{
    m_identityProvider = std::move(provider);
    m_identity = nullptr;
    clearOffline();
    reconcile();
}

const StationIdentity* RoundtableClient::identity() const
{
    if (m_identityProvider) {
        return m_identityProvider();
    }
    return m_identity;
}

void RoundtableClient::setStationInfo(const QString& callsign, const QString& grid)
{
    const QString call = callsign.trimmed().toUpper();
    const QString square = RoundtableWire::gridToSend(grid);
    if (call == m_callsign && square == m_grid) {
        return;
    }
    m_callsign = call;
    m_grid = square;
    m_presenceDirty = true;
    clearOffline();
    if (m_main.socket && m_main.leg == Leg::Station) {
        // A new callsign or grid while signed in: the station connection
        // starts again, so the hello and presence carry the new station.
        // reconcile() goes offline with the reason when the server would
        // refuse the callsign.
        closeLink(m_main, true);
        m_attempt = 0;
        m_reconnectDueMs = -1;
    }
    reconcile();
}

void RoundtableClient::setSlices(const QList<RoundtableSliceState>& slices)
{
    const qint64 now = monoNow();
    const QList<RoundtableSliceState> before = presenceSlices();
    QSet<int> ids;
    for (const RoundtableSliceState& slice : slices) {
        ids.insert(slice.id);
        auto previous = std::find_if(m_slices.cbegin(), m_slices.cend(),
                                     [&slice](const RoundtableSliceState& s) { return s.id == slice.id; });
        // A new frequency or mode starts the floor again (6.5).
        if (previous != m_slices.cend() && (previous->hz != slice.hz || previous->mode != slice.mode)) {
            m_meters[slice.id].floor.retune(now);
        }
        // A reading taken before the key goes down is never sent after it.
        if (slice.keyed) {
            const auto meter = m_meters.find(slice.id);
            if (meter != m_meters.end()) {
                meter->sentStampNs = meter->stampNs;
            }
        }
    }
    for (auto it = m_meters.begin(); it != m_meters.end();) {
        if (!ids.contains(it.key())) {
            it = m_meters.erase(it);
        } else {
            ++it;
        }
    }
    m_slices = slices;
    if (!sameSlices(before, presenceSlices())) {
        m_presenceDirty = true;
    }
    syncWatches();
}

void RoundtableClient::pushMeter(int sliceId, double dbm, qint64 meterReadingNs)
{
    const auto slice = std::find_if(m_slices.cbegin(), m_slices.cend(),
                                    [sliceId](const RoundtableSliceState& s) { return s.id == sliceId; });
    // No reading while keyed (6.5), and none for a slice not listed.
    if (slice == m_slices.cend() || slice->keyed) {
        return;
    }
    Meter& meter = m_meters[sliceId];
    if (meterReadingNs <= meter.stampNs) {
        return;
    }
    meter.stampNs = meterReadingNs;
    meter.dbm = dbm;
    meter.wallMs = wallNow();
    if (std::isfinite(dbm)) {
        meter.present = meter.floor.update(dbm, monoNow());
    }
}

void RoundtableClient::setAtRadio(bool atRadio)
{
    if (atRadio == m_atRadio) {
        return;
    }
    m_atRadio = atRadio;
    m_presenceDirty = true;
    reconcile();
    emit statusTextChanged();
}

void RoundtableClient::setHidden(bool hidden)
{
    if (hidden == m_hidden) {
        return;
    }
    m_hidden = hidden;
    reconcile();
    emit statusTextChanged();
}

void RoundtableClient::setFirstRunAnswered(bool answered)
{
    if (answered == m_answered) {
        return;
    }
    m_answered = answered;
    reconcile();
    emit statusTextChanged();
}

void RoundtableClient::setStopWhenAway(bool stop)
{
    if (stop == m_stopWhenAway) {
        return;
    }
    m_stopWhenAway = stop;
    reconcile();
    emit statusTextChanged();
}

void RoundtableClient::start()
{
    m_started = true;
    clearOffline();
    m_attempt = 0;
    m_reconnectDueMs = -1;
    m_tick->start();
    // Opened from the event loop, so whoever starts the client can finish
    // setting it up (at radio, slices) in the same turn first.
    if (!m_openQueued) {
        m_openQueued = true;
        QMetaObject::invokeMethod(this, [this] {
            m_openQueued = false;
            reconcile();
        }, Qt::QueuedConnection);
    }
}

void RoundtableClient::stop()
{
    m_started = false;
    m_reconnectDueMs = -1;
    closeLink(m_main, true);
    closeLink(m_archive, true);
    m_mainQueue.clear();
    m_archiveQueue.clear();
    m_archiveUnanswered = 0;
    m_tick->stop();
    m_offline = false;
    m_offlineReason.clear();
    setState(LinkState::Off);
}

// ----------------------------------------------------------------- clocks

void RoundtableClient::setClocksForTest(Clock wallMs, Clock monotonicMs)
{
    m_wallClock = std::move(wallMs);
    m_monoClock = std::move(monotonicMs);
}

qint64 RoundtableClient::wallNow() const
{
    return m_wallClock ? m_wallClock() : QDateTime::currentMSecsSinceEpoch();
}

qint64 RoundtableClient::monoNow() const
{
    if (m_monoClock) {
        return m_monoClock();
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// ----------------------------------------------------------------- links

RoundtableClient::Want RoundtableClient::wanted() const
{
    if (!m_started || m_offline) {
        return Want::None;
    }
    // D25 and 10.2: until the first-run question is answered, while
    // hidden, and while nobody is at the radio when the operator chose to
    // stop then, nothing about the station is sent.
    const bool station = m_answered && !m_hidden && !(m_stopWhenAway && !m_atRadio);
    return station ? Want::Station : Want::Live;
}

bool RoundtableClient::stationLive() const
{
    return m_main.socket && m_main.leg == Leg::Station && m_main.signedIn;
}

void RoundtableClient::reconcile()
{
    const Want want = wanted();
    if (want == Want::None) {
        if (m_main.socket) {
            closeLink(m_main, true);
        }
        if (!m_started) {
            setState(LinkState::Off);
        }
        return;
    }
    const Leg leg = want == Want::Station ? Leg::Station : Leg::Live;
    if (m_main.socket && m_main.leg == leg) {
        return;
    }
    if (leg == Leg::Station && m_archive.socket) {
        // 3.1: a second connection of this station closes the older one
        // with 4003. The archive's unsent actions go on the station
        // connection instead, and the station connection opens once the
        // archive has its answers (finishArchiveIfAnswered,
        // onArchiveClosed); the live reader carries on until then.
        m_mainQueue.append(m_archiveQueue);
        m_archiveQueue.clear();
        if (m_archiveUnanswered > 0) {
            return;
        }
        closeLink(m_archive, true);
    }
    if (m_main.socket) {
        // Going hidden (or away with Stop) from live: `hidden` is the last
        // thing the station sends (5.3.14), then the live reader opens.
        if (m_main.leg == Leg::Station && m_main.signedIn) {
            send(m_main, RoundtableWire::hidden());
        }
        closeLink(m_main, true);
        m_attempt = 0;
        m_reconnectDueMs = -1;
    } else if (m_reconnectDueMs >= 0) {
        // Waiting out a retry; serviceTimers() opens it when due.
        return;
    }
    if (leg == Leg::Station) {
        if (!RoundtableWire::isCallsign(m_callsign)) {
            goOffline(tr("Roundtable needs a callsign of 3 to 16 letters, digits and /."));
            return;
        }
        const StationIdentity* key = identity();
        if (key == nullptr || !key->isValid()) {
            goOffline(tr("This station's key could not be loaded, so Roundtable can't sign in."));
            return;
        }
    }
    openLink(m_main, leg);
}

void RoundtableClient::openLink(Link& link, Leg leg)
{
    closeLink(link, false);
    link.leg = leg;
    link.url = leg == Leg::Live ? liveUrl() : m_serverUrl;
    link.generation = ++m_nextGeneration;
    link.open = false;
    link.signedIn = false;
    link.errorCode.clear();
    link.errorReason.clear();
    link.retryAfterMs = 0;
    link.openedMs = monoNow();
    link.lastFrameMs = link.openedMs;
    link.lastPingMs = link.openedMs;
    const bool main = &link == &m_main;
    if (main) {
        m_roomsGen = 0;
        m_awaitingRooms = false;
        m_roomGens.clear();
        m_awaitingRoom.clear();
        m_listedRooms.clear();
        m_watched.clear();
        if (leg == Leg::Station) {
            m_presenceSent = false;
            m_sentSlices.clear();
        }
        setState(m_attempt == 0 ? LinkState::Connecting : LinkState::Reconnecting);
    }
    // A test run never reaches a server off this computer (every test
    // binary runs in QStandardPaths test mode).
    if (QStandardPaths::isTestModeEnabled() && !isLoopbackHost(link.url.host())) {
        qCInfo(lcRoundtable) << "Test run: not contacting a Roundtable server off this computer";
        const Leg which = leg;
        const quint64 generation = link.generation;
        QMetaObject::invokeMethod(this, [this, which, generation] {
            onDisconnected(which, generation);
        }, Qt::QueuedConnection);
        return;
    }
    auto* socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    socket->setMaxAllowedIncomingMessageSize(static_cast<quint64>(RoundtableWire::kMaxIncomingBytes));
    socket->setMaxAllowedIncomingFrameSize(static_cast<quint64>(RoundtableWire::kMaxIncomingBytes));
    link.socket = socket;
    const quint64 generation = link.generation;
    connect(socket, &QWebSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, &QWebSocket::connected, this,
            [this, leg, generation] { onConnected(leg, generation); });
    connect(socket, &QWebSocket::disconnected, this,
            [this, leg, generation] { onDisconnected(leg, generation); });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(socket, &QWebSocket::errorOccurred, this,
#else
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this,
#endif
            [this, leg, generation](QAbstractSocket::SocketError) {
        // A refused or unreachable server may never say disconnected.
        QMetaObject::invokeMethod(this, [this, leg, generation] {
            onDisconnected(leg, generation);
        }, Qt::QueuedConnection);
    });
    connect(socket, &QWebSocket::textMessageReceived, this,
            [this, leg, generation](const QString& text) { onText(leg, generation, text); });
    connect(socket, &QWebSocket::binaryMessageReceived, this, [this, leg, generation] {
        Link& l = leg == Leg::Archive ? m_archive : m_main;
        if (l.generation == generation) {
            l.lastFrameMs = monoNow();
            qCWarning(lcRoundtable) << "Roundtable sent a binary message; ignored";
        }
    });
    connect(socket, &QWebSocket::pong, this, [this, leg, generation] {
        Link& l = leg == Leg::Archive ? m_archive : m_main;
        if (l.generation == generation) {
            l.lastFrameMs = monoNow();
        }
    });
    qCInfo(lcRoundtable) << "Connecting to Roundtable" << link.url.host()
                         << (leg == Leg::Live ? "(read only)" : leg == Leg::Archive ? "(archive)" : "");
    socket->open(link.url);
}

void RoundtableClient::closeLink(Link& link, bool polite)
{
    link.generation = ++m_nextGeneration;
    link.open = false;
    link.signedIn = false;
    if (!link.socket) {
        return;
    }
    QWebSocket* socket = link.socket.data();
    link.socket.clear();
    disconnect(socket, nullptr, this, nullptr);
    if (polite && socket->state() == QAbstractSocket::ConnectedState) {
        socket->close(QWebSocketProtocol::CloseCodeNormal);
        // Deleted on disconnected (connected at creation), or here if the
        // peer never answers the close.
        QTimer::singleShot(5000, socket, &QObject::deleteLater);
    } else {
        socket->abort();
        socket->deleteLater();
    }
}

void RoundtableClient::onConnected(Leg leg, quint64 generation)
{
    Link& link = leg == Leg::Archive ? m_archive : m_main;
    if (link.generation != generation || !link.socket) {
        return;
    }
    link.open = true;
    link.lastFrameMs = monoNow();
    link.lastPingMs = link.lastFrameMs;
    if (leg == Leg::Live) {
        // 7.8: the band list, all bands, and the rooms being read.
        send(link, RoundtableWire::watchBand(QStringLiteral("all")));
        m_watched.clear();
        syncWatches();
        setState(LinkState::Hidden);
        emit statusTextChanged();
    }
    // A station waits for `challenge` (3.1).
}

void RoundtableClient::onDisconnected(Leg leg, quint64 generation)
{
    Link& link = leg == Leg::Archive ? m_archive : m_main;
    if (link.generation != generation) {
        return;
    }
    int code = 0;
    if (link.socket) {
        code = static_cast<int>(link.socket->closeCode());
    }
    closeLink(link, false);
    if (leg == Leg::Archive) {
        onArchiveClosed();
    } else {
        onMainClosed(code);
    }
}

void RoundtableClient::onMainClosed(int closeCode)
{
    if (!m_started) {
        return;
    }
    // 2.4: these need the operator, or mean another connection of this
    // station is live; never a reconnect loop.
    QString reason;
    if (m_main.leg == Leg::Station) {
        if (closeCode == kCloseSignIn) {
            reason = m_main.errorReason.isEmpty()
                ? tr("Roundtable did not accept this station's sign-in.")
                : m_main.errorReason;
        } else if (closeCode == kCloseReplaced) {
            reason = tr("This station signed in to Roundtable on another connection.");
        } else if (closeCode == kCloseRevoked) {
            reason = tr("Roundtable no longer accepts this station's key.");
        }
    }
    if (closeCode == kCloseProtocol) {
        reason = tr("Roundtable closed the connection after a message it could not read. "
                    "Updating NereusSDR may help.");
    }
    if (!reason.isEmpty()) {
        qCWarning(lcRoundtable) << "Roundtable closed the connection with" << closeCode
                                << "; not retrying";
        goOffline(reason);
        return;
    }
    // 4005, 1000, 1001, 1013 and the rest: the normal backoff, at least as
    // long as a cap's `retryAfterMs` (9.1).
    scheduleReconnect(m_main.retryAfterMs);
}

void RoundtableClient::scheduleReconnect(qint64 atLeastMs)
{
    const QList<int>& delays = reconnectDelaysMs();
    const int step = std::min<int>(m_attempt, static_cast<int>(delays.size()) - 1);
    const qint64 delay = std::max<qint64>(delays.at(step), atLeastMs);
    ++m_attempt;
    m_reconnectDueMs = monoNow() + delay;
    qCInfo(lcRoundtable) << "Roundtable connection lost; trying again in" << delay << "ms";
    setState(LinkState::Reconnecting);
    emit statusTextChanged();
}

void RoundtableClient::goOffline(const QString& reason)
{
    m_offline = true;
    m_offlineReason = reason;
    m_reconnectDueMs = -1;
    closeLink(m_main, true);
    setState(LinkState::Offline);
    emit statusTextChanged();
}

void RoundtableClient::clearOffline()
{
    if (!m_offline) {
        return;
    }
    m_offline = false;
    m_offlineReason.clear();
    m_attempt = 0;
}

void RoundtableClient::setState(LinkState state)
{
    if (state == m_state) {
        return;
    }
    m_state = state;
    emit linkStateChanged(state);
    emit statusTextChanged();
}

QString RoundtableClient::statusText() const
{
    switch (m_state) {
    case LinkState::Off:
        return {};
    case LinkState::Connecting:
        return tr("Connecting to Roundtable");
    case LinkState::Reconnecting:
        return tr("Lost Roundtable; trying again");
    case LinkState::Live:
        return tr("Live as %1").arg(m_callsign);
    case LinkState::Hidden:
        if (!m_answered) {
            return tr("Reading only, until someone chooses Go live");
        }
        if (m_hidden) {
            return tr("Hidden: reading only, nothing sent about this station");
        }
        return tr("Nobody at the radio: reading only, nothing sent about this station");
    case LinkState::Offline:
        return m_offlineReason;
    }
    return {};
}

bool RoundtableClient::send(Link& link, const QJsonObject& message)
{
    if (!link.socket) {
        return false;
    }
    const RoundtableWire::Direction direction = link.leg == Leg::Live
        ? RoundtableWire::Direction::LiveToServer
        : RoundtableWire::Direction::StationToServer;
    const QByteArray bytes = RoundtableWire::encode(message);
    const QString text = QString::fromUtf8(bytes);
    if (bytes.size() > RoundtableWire::kMaxOutgoingBytes) {
        qCWarning(lcRoundtable) << "Not sent: a" << message.value(QStringLiteral("type")).toString()
                                << "over the server's size limit";
        return false;
    }
    // Never a message the server would close the connection for (5.1).
    const RoundtableWire::Decoded check = RoundtableWire::decode(text, direction);
    if (!check.ok) {
        qCWarning(lcRoundtable).noquote() << "Not sent: a" << message.value(QStringLiteral("type")).toString()
                                          << "the server would refuse:" << check.error;
        return false;
    }
    link.socket->sendTextMessage(text);
    return true;
}

// ----------------------------------------------------------------- receive

void RoundtableClient::onText(Leg leg, quint64 generation, const QString& text)
{
    Link& link = leg == Leg::Archive ? m_archive : m_main;
    if (link.generation != generation) {
        return;
    }
    link.lastFrameMs = monoNow();
    const RoundtableWire::Direction direction = leg == Leg::Live
        ? RoundtableWire::Direction::ServerToLive
        : RoundtableWire::Direction::ServerToStation;
    const RoundtableWire::Decoded decoded = RoundtableWire::decode(text, direction);
    if (!decoded.ok) {
        // 5.1: logged and ignored at a station or live reader.
        qCWarning(lcRoundtable).noquote() << "Ignored a Roundtable message:" << decoded.error;
        return;
    }
    if (leg == Leg::Station) {
        handleStation(decoded.message);
    } else if (leg == Leg::Live) {
        handleLive(decoded.message);
    } else {
        handleArchive(decoded.message);
    }
}

void RoundtableClient::answerChallenge(Link& link, const QJsonObject& message)
{
    if (link.signedIn) {
        return;
    }
    bool ok = false;
    const QByteArray nonce =
        RoundtableWire::fromBase64Url(message.value(QStringLiteral("nonce")).toString(), &ok);
    const StationIdentity* key = identity();
    if (!ok || key == nullptr || !key->isValid()) {
        qCWarning(lcRoundtable) << "Roundtable sign-in not possible: no station key";
        if (&link == &m_main) {
            goOffline(tr("This station's key could not be loaded, so Roundtable can't sign in."));
        } else {
            closeLink(link, true);
            onArchiveClosed();
        }
        return;
    }
    // 4.3: the host as the station connected to it.
    const QByteArray transcript =
        RoundtableWire::helloTranscript(RoundtableWire::signingHost(link.url), nonce);
    const QByteArray signature = key->sign(transcript);
    send(link, RoundtableWire::hello(m_software, m_softwareVersion, key->publicKeySpki(), signature));
}

void RoundtableClient::handleError(Link& link, const QJsonObject& message)
{
    const QString code = message.value(QStringLiteral("code")).toString();
    const QString reason = message.value(QStringLiteral("reason")).toString();
    const QJsonValue about = message.value(QStringLiteral("about"));
    const qint64 retry = message.value(QStringLiteral("retryAfterMs")).toInteger();
    link.errorCode = code;
    link.errorReason = reason;
    if (code == QLatin1String("rate")) {
        if (about.isNull()) {
            // A connection cap, before close 1013 (3.1).
            link.retryAfterMs = retry;
        } else if (about.toString() == QLatin1String("presence")) {
            // 5.3.2: send the current presence again after that time.
            m_presenceNotBeforeMs = monoNow() + retry;
            m_presenceDirty = true;
        }
    }
    qCInfo(lcRoundtable) << "Roundtable refused" << about.toString() << "with" << code;
    emit errorReceived(code, reason);
}

void RoundtableClient::handleStation(const QJsonObject& message)
{
    const QString type = message.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("challenge")) {
        answerChallenge(m_main, message);
        return;
    }
    if (type == QLatin1String("error")) {
        handleError(m_main, message);
        return;
    }
    if (type == QLatin1String("welcome")) {
        m_main.signedIn = true;
        m_attempt = 0;
        m_stationId = message.value(QStringLiteral("station")).toString();
        qCInfo(lcRoundtable) << "Signed in to Roundtable as" << shortId(m_stationId);
        m_presenceDirty = true;
        m_presenceSent = false;
        m_presenceNotBeforeMs = 0;
        m_sentSlices.clear();
        setState(LinkState::Live);
        emit statusTextChanged();
        for (const QJsonValue& mention : message.value(QStringLiteral("mentions")).toArray()) {
            const QJsonObject m = mention.toObject();
            emitMention(m.value(QStringLiteral("room")).toString(), m.value(QStringLiteral("message")).toObject());
        }
        // Presence first, so readings follow a presence that lists them.
        sendPresenceIfDue(monoNow());
        for (const QString& room : std::as_const(m_reads)) {
            send(m_main, RoundtableWire::read(room));
        }
        flushQueuedArchive(m_main, m_mainQueue);
        return;
    }
    if (!m_main.signedIn) {
        return;
    }
    if (type == QLatin1String("posted")) {
        const bool ok = message.value(QStringLiteral("ok")).toBool();
        const QString clientId = message.value(QStringLiteral("clientId")).toString();
        emit postResult(clientId, ok,
                        ok ? QString::number(message.value(QStringLiteral("msg")).toInteger())
                           : message.value(QStringLiteral("reason")).toString());
    } else if (type == QLatin1String("mention")) {
        emitMention(message.value(QStringLiteral("room")).toString(),
                    message.value(QStringLiteral("message")).toObject());
    } else if (type == QLatin1String("notice")) {
        emit noticeReceived(message.value(QStringLiteral("text")).toString());
    } else if (type == QLatin1String("weblinkOk")) {
        emit webLinkReady(QUrl(message.value(QStringLiteral("url")).toString()));
    } else {
        handleViews(type, message);
    }
}

void RoundtableClient::handleLive(const QJsonObject& message)
{
    const QString type = message.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("error")) {
        handleError(m_main, message);
        return;
    }
    if (type == QLatin1String("rooms")) {
        // The server answered: this reader is up, so the ladder restarts.
        m_attempt = 0;
    }
    handleViews(type, message);
}

void RoundtableClient::handleViews(const QString& type, const QJsonObject& message)
{
    const qint64 gen = message.value(QStringLiteral("gen")).toInteger();
    if (type == QLatin1String("rooms")) {
        m_roomsGen = gen;
        m_awaitingRooms = false;
        m_listedRooms.clear();
        for (const QJsonValue& value : message.value(QStringLiteral("rooms")).toArray()) {
            const QJsonObject summary = value.toObject();
            m_listedRooms.insert(summary.value(QStringLiteral("id")).toString(),
                                 RoundtableWire::ListedRoom{
                                     summary.value(QStringLiteral("family")).toString(),
                                     summary.value(QStringLiteral("hz")).toInteger()});
        }
        emit roomsReceived(message);
        syncWatches();
    } else if (type == QLatin1String("roomsDelta")) {
        if (m_awaitingRooms) {
            return;
        }
        if (gen != m_roomsGen + 1) {
            // 6.11: a gap; ask for the whole band list again.
            m_awaitingRooms = true;
            send(m_main, RoundtableWire::resync(QString()));
            return;
        }
        m_roomsGen = gen;
        for (const QString& key : {QStringLiteral("add"), QStringLiteral("change")}) {
            for (const QJsonValue& value : message.value(key).toArray()) {
                const QJsonObject summary = value.toObject();
                m_listedRooms.insert(summary.value(QStringLiteral("id")).toString(),
                                     RoundtableWire::ListedRoom{
                                         summary.value(QStringLiteral("family")).toString(),
                                         summary.value(QStringLiteral("hz")).toInteger()});
            }
        }
        for (const QJsonValue& value : message.value(QStringLiteral("remove")).toArray()) {
            m_listedRooms.remove(value.toString());
        }
        emit roomsDeltaReceived(message);
        syncWatches();
    } else if (type == QLatin1String("room")) {
        const QString id = message.value(QStringLiteral("id")).toString();
        m_roomGens.insert(id, gen);
        m_awaitingRoom.remove(id);
        emit roomReceived(message);
        if (m_main.leg == Leg::Live) {
            scanForMentions(id, message.value(QStringLiteral("messages")).toArray());
        }
    } else if (type == QLatin1String("delta")) {
        const QString id = message.value(QStringLiteral("room")).toString();
        if (m_awaitingRoom.contains(id)) {
            return;
        }
        const qint64 last = m_roomGens.value(id, -1);
        if (last < 0 || gen != last + 1) {
            m_awaitingRoom.insert(id);
            send(m_main, RoundtableWire::resync(id));
            return;
        }
        if (message.value(QStringLiteral("closed")).toBool()) {
            m_roomGens.remove(id);
        } else {
            m_roomGens.insert(id, gen);
        }
        emit roomDeltaReceived(message);
        if (m_main.leg == Leg::Live) {
            scanForMentions(id, message.value(QStringLiteral("messages")).toArray());
        }
    }
    // Any other kind this side knows is not for a view; ignored.
}

void RoundtableClient::scanForMentions(const QString& roomId, const QJsonArray& messages)
{
    // A live reader gets no `mention`, so a hidden Core finds its own in
    // the rooms it reads (6.10): a word with this station's base callsign.
    const QString mine = RoundtableWire::baseCallsign(m_callsign);
    if (mine.isEmpty()) {
        return;
    }
    for (const QJsonValue& value : messages) {
        const QJsonObject message = value.toObject();
        if (message.value(QStringLiteral("state")).toString() != QLatin1String("shown")) {
            continue;
        }
        if (RoundtableWire::baseCallsign(message.value(QStringLiteral("callsign")).toString()) == mine) {
            continue;  // this station's own message
        }
        if (RoundtableWire::mentionedBaseCallsigns(message.value(QStringLiteral("text")).toString())
                .contains(mine)) {
            emitMention(roomId, message);
        }
    }
}

void RoundtableClient::emitMention(const QString& roomId, const QJsonObject& message)
{
    const qint64 id = message.value(QStringLiteral("id")).toInteger();
    if (id <= 0 || m_mentionIds.contains(id)) {
        return;
    }
    if (message.value(QStringLiteral("t")).toInteger() <= m_mentionsSeenMs) {
        return;
    }
    m_mentionIds.insert(id);
    emit mentionReceived(QJsonObject{{QStringLiteral("type"), QStringLiteral("mention")},
                                     {QStringLiteral("room"), roomId},
                                     {QStringLiteral("message"), message}});
}

// ----------------------------------------------------------------- sending

QList<RoundtableSliceState> RoundtableClient::presenceSlices() const
{
    QList<RoundtableSliceState> out;
    QSet<int> ids;
    for (const RoundtableSliceState& slice : m_slices) {
        // 6.1: a mode with no family is left out; 5.2's ranges hold.
        if (RoundtableWire::familyOfMode(slice.mode).isEmpty() || slice.id < 0 || slice.id > kMaxSliceId
            || slice.hz < 1 || slice.hz > kMaxHz || ids.contains(slice.id)) {
            continue;
        }
        RoundtableSliceState listed = slice;
        listed.filterHz = std::clamp(slice.filterHz, 1, kMaxFilterHz);
        out.append(listed);
        ids.insert(slice.id);
        if (out.size() == RoundtableWire::kMaxSlices) {
            break;
        }
    }
    return out;
}

void RoundtableClient::sendPresenceIfDue(qint64 now)
{
    if (!stationLive()) {
        return;
    }
    const qint64 since = now - m_lastPresenceMs;
    if (m_presenceSent && !m_presenceDirty && since < kPresenceRepeatMs) {
        return;
    }
    // 6.5: at least 1100 ms apart, and not before a refusal's retry time.
    if (m_presenceSent && since < kPresenceSpacingMs) {
        return;
    }
    if (now < m_presenceNotBeforeMs) {
        return;
    }
    const QList<RoundtableSliceState> slices = presenceSlices();
    if (!send(m_main, RoundtableWire::presence(m_callsign, m_grid, m_atRadio, slices))) {
        return;
    }
    m_presenceSent = true;
    m_presenceDirty = false;
    m_lastPresenceMs = now;
    m_sentSlices.clear();
    for (const RoundtableSliceState& slice : slices) {
        m_sentSlices.insert(slice.id, slice.keyed);
    }
}

void RoundtableClient::sendReadingsIfDue(qint64 now)
{
    if (!stationLive() || !m_presenceSent) {
        return;
    }
    for (const RoundtableSliceState& slice : std::as_const(m_slices)) {
        // A slice the server's latest presence lists, not keyed there or
        // now (5.3.3 refuses the others).
        const auto sent = m_sentSlices.constFind(slice.id);
        if (sent == m_sentSlices.constEnd() || sent.value() || slice.keyed) {
            continue;
        }
        const auto found = m_meters.find(slice.id);
        if (found == m_meters.end()) {
            continue;
        }
        Meter& meter = found.value();
        if (meter.stampNs < 0 || meter.stampNs <= meter.sentStampNs) {
            continue;  // no new reading since the last one sent
        }
        if (meter.sentOnce && now - meter.lastSentMs < kReadingIntervalMs) {
            continue;
        }
        if (!std::isfinite(meter.dbm) || meter.dbm < kMinDbm || meter.dbm > kMaxDbm) {
            continue;
        }
        if (send(m_main, RoundtableWire::reading(slice.id, meter.dbm, meter.wallMs, meter.present))) {
            meter.sentStampNs = meter.stampNs;
            meter.lastSentMs = now;
            meter.sentOnce = true;
        }
    }
}

void RoundtableClient::serviceTimers()
{
    const qint64 now = monoNow();
    if (m_started && !m_offline && !m_main.socket && m_reconnectDueMs >= 0 && now >= m_reconnectDueMs) {
        m_reconnectDueMs = -1;
        reconcile();
    }
    for (Link* link : {&m_main, &m_archive}) {
        if (!link->socket) {
            continue;
        }
        if (now - link->lastFrameMs >= kSilenceMs) {
            // 2.3: nothing for 45 s; the connection is lost.
            qCInfo(lcRoundtable) << "No frame from Roundtable for 45 s; reconnecting";
            const Leg leg = link->leg;
            closeLink(*link, false);
            if (leg == Leg::Archive) {
                onArchiveClosed();
            } else {
                onMainClosed(0);
            }
            continue;
        }
        if (link->open && now - link->lastPingMs >= kPingIntervalMs) {
            link->lastPingMs = now;
            link->socket->ping();
        }
    }
    if (m_archive.socket && now - m_archive.openedMs >= kArchiveTimeoutMs) {
        closeLink(m_archive, true);
        onArchiveClosed();
    }
    sendPresenceIfDue(now);
    sendReadingsIfDue(now);
}

// ----------------------------------------------------------------- actions

void RoundtableClient::post(const QString& roomId, const QString& text, const QString& clientId)
{
    if (!stationLive()) {
        emit postResult(clientId, false, tr("Roundtable is not connected, so this can't be posted."));
        return;
    }
    if (!send(m_main, RoundtableWire::post(roomId, text, clientId))) {
        emit postResult(clientId, false, tr("This message can't be sent."));
    }
}

void RoundtableClient::deleteMessage(const QString& messageId)
{
    bool ok = false;
    const qint64 id = messageId.toLongLong(&ok);
    if (!ok || !stationLive()) {
        qCInfo(lcRoundtable) << "Delete not sent: not live, or not a message id";
        return;
    }
    send(m_main, RoundtableWire::deleteMessage(id));
}

void RoundtableClient::nameTurn(const QString& turnId, const QString& name)
{
    bool ok = false;
    const qint64 id = turnId.toLongLong(&ok);
    if (!ok || !stationLive()) {
        qCInfo(lcRoundtable) << "Name not sent: not live, or not a turn id";
        return;
    }
    send(m_main, RoundtableWire::name(id, name.trimmed().toUpper()));
}

void RoundtableClient::report(const QString& kind, const QString& target, const QString& reason)
{
    if (!stationLive()) {
        qCInfo(lcRoundtable) << "Report not sent: not live";
        return;
    }
    bool ok = false;
    if (kind == QLatin1String("message")) {
        const qint64 id = target.toLongLong(&ok);
        if (ok) {
            send(m_main, RoundtableWire::reportMessage(id, reason));
        }
    } else if (kind == QLatin1String("turn")) {
        const qint64 id = target.toLongLong(&ok);
        if (ok) {
            send(m_main, RoundtableWire::reportTurn(id, reason));
        }
    } else if (kind == QLatin1String("station")) {
        const qsizetype slash = target.indexOf(QLatin1Char('/'));
        if (slash > 0) {
            ok = true;
            send(m_main, RoundtableWire::reportStation(target.left(slash), target.mid(slash + 1), reason));
        }
    }
    if (!ok) {
        qCWarning(lcRoundtable) << "Report not sent: not a target for" << kind;
    }
}

void RoundtableClient::readRoom(const QString& roomId)
{
    if (!m_reads.contains(roomId)) {
        m_reads.append(roomId);
    }
    if (stationLive()) {
        send(m_main, RoundtableWire::read(roomId));
    } else {
        syncWatches();
    }
}

void RoundtableClient::unreadRoom(const QString& roomId)
{
    m_reads.removeAll(roomId);
    if (stationLive()) {
        send(m_main, RoundtableWire::unread(roomId));
    } else {
        syncWatches();
    }
}

void RoundtableClient::markMentionsSeen(qint64 upToMs)
{
    m_mentionsSeenMs = std::max(m_mentionsSeenMs, upToMs);
    // Read live, it stays here: nothing about the station is sent.
    if (stationLive()) {
        send(m_main, RoundtableWire::mentionsSeen(upToMs));
    }
}

void RoundtableClient::requestWebLink()
{
    runArchive(ArchiveAction::WebLink);
}

void RoundtableClient::deleteArchive()
{
    runArchive(ArchiveAction::DeleteArchive);
}

void RoundtableClient::sendArchiveAction(Link& link, ArchiveAction action)
{
    if (action == ArchiveAction::WebLink) {
        send(link, RoundtableWire::weblink());
    } else {
        send(link, RoundtableWire::deleteArchive(QLatin1String(RoundtableWire::kDeleteArchiveWords)));
    }
}

void RoundtableClient::runArchive(ArchiveAction action)
{
    if (stationLive()) {
        sendArchiveAction(m_main, action);
        return;
    }
    if (wanted() == Want::Station) {
        // The station connection is wanted (signing in, waiting out a
        // retry, or waiting for an archive connection to finish): the
        // action goes on it once welcomed, never on a second connection of
        // this station (3.1 closes the older with 4003).
        m_mainQueue.append(action);
        if (!m_main.socket && m_reconnectDueMs < 0) {
            reconcile();
        }
        return;
    }
    if (m_main.socket && m_main.leg == Leg::Station) {
        // Signing in now; sent once welcomed.
        m_mainQueue.append(action);
        return;
    }
    // 7.8 and 10.2: hidden or unanswered, a brief station connection that
    // sends `hello` and then only this.
    const StationIdentity* key = identity();
    if (key == nullptr || !key->isValid()) {
        emit errorReceived(QStringLiteral("no-key"),
                           tr("This station's key could not be loaded, so Roundtable can't sign in."));
        return;
    }
    m_archiveQueue.append(action);
    if (!m_archive.socket) {
        m_archiveErrored = false;
        m_archiveUnanswered = 0;
        openLink(m_archive, Leg::Archive);
    } else if (m_archive.signedIn) {
        flushQueuedArchive(m_archive, m_archiveQueue);
    }
}

void RoundtableClient::flushQueuedArchive(Link& link, QList<ArchiveAction>& queue)
{
    const QList<ArchiveAction> actions = queue;
    queue.clear();
    for (const ArchiveAction action : actions) {
        sendArchiveAction(link, action);
        if (&link == &m_archive) {
            ++m_archiveUnanswered;
        }
    }
}

void RoundtableClient::handleArchive(const QJsonObject& message)
{
    const QString type = message.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("challenge")) {
        answerChallenge(m_archive, message);
    } else if (type == QLatin1String("welcome")) {
        m_archive.signedIn = true;
        flushQueuedArchive(m_archive, m_archiveQueue);
    } else if (type == QLatin1String("weblinkOk")) {
        emit webLinkReady(QUrl(message.value(QStringLiteral("url")).toString()));
        --m_archiveUnanswered;
        finishArchiveIfAnswered();
    } else if (type == QLatin1String("notice")) {
        emit noticeReceived(message.value(QStringLiteral("text")).toString());
        if (message.value(QStringLiteral("code")).toString() == QLatin1String("archive-deleted")) {
            --m_archiveUnanswered;
            finishArchiveIfAnswered();
        }
    } else if (type == QLatin1String("error")) {
        m_archiveErrored = true;
        handleError(m_archive, message);
        const QString about = message.value(QStringLiteral("about")).toString();
        if (about == QLatin1String("weblink") || about == QLatin1String("deleteArchive")) {
            --m_archiveUnanswered;
            finishArchiveIfAnswered();
        }
    }
    // rooms and the rest: the live reader keeps the views.
}

void RoundtableClient::finishArchiveIfAnswered()
{
    if (m_archiveUnanswered <= 0 && m_archiveQueue.isEmpty()) {
        m_archiveUnanswered = 0;
        closeLink(m_archive, true);
        // A station connection that waited for it opens now.
        if (m_started) {
            reconcile();
        }
    }
}

void RoundtableClient::onArchiveClosed()
{
    const bool unanswered = m_archiveUnanswered > 0 || !m_archiveQueue.isEmpty();
    m_archiveUnanswered = 0;
    m_archiveQueue.clear();
    if (unanswered && !m_archiveErrored) {
        emit errorReceived(QStringLiteral("unreachable"),
                           tr("Roundtable could not be reached. Try again shortly."));
    }
    if (m_started) {
        reconcile();
    }
}

void RoundtableClient::placeSlices()
{
    // 6.3, as the Core's state places them (RoundtableState::replace).
    QHash<int, QString> next;
    for (const RoundtableSliceState& slice : std::as_const(m_slices)) {
        const QString room =
            RoundtableWire::placeSlice(slice, m_sliceRoomIds.value(slice.id), m_listedRooms);
        if (!room.isEmpty()) {
            next.insert(slice.id, room);
        }
    }
    m_sliceRoomIds = next;
}

void RoundtableClient::syncWatches()
{
    placeSlices();
    if (!m_main.socket || m_main.leg != Leg::Live || !m_main.open) {
        return;
    }
    // 7.8: the live reader watches the rooms it shows: the rooms read,
    // newest first, then each slice's room, at most four.
    QStringList wanted;
    for (auto it = m_reads.crbegin(); it != m_reads.crend(); ++it) {
        if (!wanted.contains(*it)) {
            wanted.append(*it);
        }
    }
    QList<int> ids = m_sliceRoomIds.keys();
    std::sort(ids.begin(), ids.end());
    for (const int id : std::as_const(ids)) {
        const QString room = m_sliceRoomIds.value(id);
        if (!wanted.contains(room)) {
            wanted.append(room);
        }
    }
    if (wanted.size() > kMaxWatched) {
        wanted = wanted.mid(0, kMaxWatched);
    }
    for (const QString& room : std::as_const(m_watched)) {
        if (!wanted.contains(room)) {
            send(m_main, RoundtableWire::unwatch(room));
        }
    }
    for (const QString& room : std::as_const(wanted)) {
        if (!m_watched.contains(room)) {
            send(m_main, RoundtableWire::watch(room));
        }
    }
    m_watched = wanted;
}

} // namespace NereusSDR
