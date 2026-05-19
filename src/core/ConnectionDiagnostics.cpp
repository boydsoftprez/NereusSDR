// =================================================================
// src/core/ConnectionDiagnostics.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-native lightweight QObject that aggregates runtime
// connection metrics from a bound PgxlConnection or TgxlConnection.
//
// Design reference:
//   docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-design.md
//   section 4.6.
//
// AI tooling: Anthropic Claude Code.
// =================================================================

#include "ConnectionDiagnostics.h"
#include "PgxlConnection.h"
#include "TgxlConnection.h"

namespace NereusSDR {

ConnectionDiagnostics::ConnectionDiagnostics(QObject* parent)
    : QObject(parent)
{
    m_coalesceTimer.setInterval(1000);
    m_coalesceTimer.setSingleShot(false);
    connect(&m_coalesceTimer, &QTimer::timeout,
            this, &ConnectionDiagnostics::onCoalesceTick);
    m_coalesceTimer.start();
}

// ---------------------------------------------------------------------------
// uptimeMs -- computed live so callers get a fresh value on each read.
// ---------------------------------------------------------------------------
qint64 ConnectionDiagnostics::uptimeMs() const
{
    if (m_connectedSinceMs == 0) {
        return 0;
    }
    return QDateTime::currentMSecsSinceEpoch() - m_connectedSinceMs;
}

// ---------------------------------------------------------------------------
// resetCounters -- zero all accumulated counters on rebind.
// ---------------------------------------------------------------------------
void ConnectionDiagnostics::resetCounters()
{
    m_connectedSinceMs = 0;
    m_lastRttMs        = 0;
    m_keepaliveMissed  = 0;
    m_reconnectCount   = 0;
    m_lastFrameMs      = 0;
    m_faultsSession    = 0;
    m_framesIn         = 0;
    m_framesOut        = 0;
    m_bytesIn          = 0;
    m_bytesOut         = 0;
}

// ---------------------------------------------------------------------------
// disconnectPrior -- tear down all QMetaObject::Connection handles
//                    so we stop listening to the previously bound object.
// ---------------------------------------------------------------------------
void ConnectionDiagnostics::disconnectPrior()
{
    QObject::disconnect(m_connConnected);
    QObject::disconnect(m_connDisconnected);
    QObject::disconnect(m_connPong);
    QObject::disconnect(m_connPingTimeout);
    QObject::disconnect(m_connReconnect);
    QObject::disconnect(m_connStatus);

    m_pgxl = nullptr;
    m_tgxl = nullptr;
}

// ---------------------------------------------------------------------------
// markDirty -- sets the dirty flag; changed() fires on the next 1 Hz tick.
// ---------------------------------------------------------------------------
void ConnectionDiagnostics::markDirty()
{
    m_dirty = true;
}

// ---------------------------------------------------------------------------
// bindTo(PgxlConnection*)
//
// framesIn / bytesIn are tracked via signals rather than polling the TCP
// byte counter so that unit tests using injectLineForTesting() (which
// bypasses the socket read path) still produce meaningful counts.
//
// Each high-level received frame emits one of:
//   connected()      -- version banner received (1 line in)
//   statusUpdated()  -- R-frame or S-frame received (1 line in)
// framesOut counts each sendCommand() write via testFrameWrittenForTesting.
// bytesIn / bytesOut approximate from poll-readable counters on tick.
// ---------------------------------------------------------------------------
void ConnectionDiagnostics::bindTo(PgxlConnection* c)
{
    disconnectPrior();
    resetCounters();

    if (!c) {
        return;
    }

    m_pgxl = c;

    // If already connected at bind time, capture the existing uptime anchor
    // and seed in-flight frame counts.
    if (c->isConnected()) {
        m_connectedSinceMs = c->connectedSinceMs();
        // Seed byte counters from existing TCP data; frame counters start
        // from 0 since we are joining mid-session (not double-counting).
        m_bytesIn  = c->bytesIn();
        m_bytesOut = c->bytesOut();
        markDirty();
    }

    // connected() fires when the version banner is parsed: counts as 1 frame in.
    m_connConnected = connect(c, &PgxlConnection::connected, this, [this, c]() {
        m_connectedSinceMs = c->connectedSinceMs();
        ++m_framesIn;
        m_lastFrameMs = QDateTime::currentMSecsSinceEpoch();
        markDirty();
    });

    m_connDisconnected = connect(c, &PgxlConnection::disconnected, this, [this]() {
        m_connectedSinceMs = 0;
        markDirty();
    });

    m_connPong = connect(c, &PgxlConnection::pongReceived,
                         this, [this](quint32 /*seq*/, qint64 rttMs, const QString& /*tag*/) {
        m_lastRttMs   = rttMs;
        m_lastFrameMs = QDateTime::currentMSecsSinceEpoch();
        markDirty();
    });

    m_connPingTimeout = connect(c, &PgxlConnection::pingTimedOut,
                                this, [this](quint32 /*seq*/) {
        ++m_keepaliveMissed;
        markDirty();
    });

    m_connReconnect = connect(c, &PgxlConnection::reconnectAttempt,
                              this, [this](int /*n*/, int /*ms*/) {
        ++m_reconnectCount;
        markDirty();
    });

    // statusUpdated fires on every R-frame or S-frame: counts as 1 frame in.
    // Also detect FAULT transitions for PGXL.
    m_connStatus = connect(c, &PgxlConnection::statusUpdated,
                           this, [this](const QMap<QString, QString>& kvs) {
        ++m_framesIn;
        m_lastFrameMs = QDateTime::currentMSecsSinceEpoch();
        const QString state = kvs.value(QStringLiteral("state"));
        if (state.startsWith(QStringLiteral("FAULT"))) {
            ++m_faultsSession;
        }
        markDirty();
    });
}

// ---------------------------------------------------------------------------
// bindTo(TgxlConnection*)
//
// Same signal-driven framesIn counting as PGXL. TGXL has no FAULT taxonomy
// (design §4.6, §6.2) so faultsSession stays 0.
// ---------------------------------------------------------------------------
void ConnectionDiagnostics::bindTo(TgxlConnection* c)
{
    disconnectPrior();
    resetCounters();

    if (!c) {
        return;
    }

    m_tgxl = c;

    if (c->isConnected()) {
        m_connectedSinceMs = c->connectedSinceMs();
        m_bytesIn  = c->bytesIn();
        m_bytesOut = c->bytesOut();
        markDirty();
    }

    m_connConnected = connect(c, &TgxlConnection::connected, this, [this, c]() {
        m_connectedSinceMs = c->connectedSinceMs();
        ++m_framesIn;
        m_lastFrameMs = QDateTime::currentMSecsSinceEpoch();
        markDirty();
    });

    m_connDisconnected = connect(c, &TgxlConnection::disconnected, this, [this]() {
        m_connectedSinceMs = 0;
        markDirty();
    });

    m_connPong = connect(c, &TgxlConnection::pongReceived,
                         this, [this](quint32 /*seq*/, qint64 rttMs, const QString& /*tag*/) {
        m_lastRttMs   = rttMs;
        m_lastFrameMs = QDateTime::currentMSecsSinceEpoch();
        markDirty();
    });

    m_connPingTimeout = connect(c, &TgxlConnection::pingTimedOut,
                                this, [this](quint32 /*seq*/) {
        ++m_keepaliveMissed;
        markDirty();
    });

    m_connReconnect = connect(c, &TgxlConnection::reconnectAttempt,
                              this, [this](int /*n*/, int /*ms*/) {
        ++m_reconnectCount;
        markDirty();
    });

    // TGXL has both stateUpdated and statusUpdated; count each as a frame in.
    // No FAULT taxonomy for TGXL (design §4.6, §6.2): skip faultsSession.
    m_connStatus = connect(c, &TgxlConnection::statusUpdated,
                           this, [this](const QMap<QString, QString>& /*kvs*/) {
        ++m_framesIn;
        m_lastFrameMs = QDateTime::currentMSecsSinceEpoch();
        markDirty();
    });
}

// ---------------------------------------------------------------------------
// onCoalesceTick -- fires at 1 Hz; polls byte counters from the bound
//                   connection (byte counters track the actual TCP bytes),
//                   then emits changed() if anything changed since last tick.
// ---------------------------------------------------------------------------
void ConnectionDiagnostics::onCoalesceTick()
{
    if (m_pgxl) {
        m_bytesIn  = m_pgxl->bytesIn();
        m_bytesOut = m_pgxl->bytesOut();
        m_framesOut = m_pgxl->framesOut();
    } else if (m_tgxl) {
        m_bytesIn  = m_tgxl->bytesIn();
        m_bytesOut = m_tgxl->bytesOut();
        m_framesOut = m_tgxl->framesOut();
    }

    if (m_dirty) {
        emit changed();
        m_dirty = false;
    }
}

// ---------------------------------------------------------------------------
// testFlushCoalesceTimer -- test seam: pull latest metrics + force-emit
//                           changed() immediately regardless of dirty state.
// ---------------------------------------------------------------------------
void ConnectionDiagnostics::testFlushCoalesceTimer()
{
    // Run the normal tick first so polled counters (bytesIn/Out, framesOut)
    // are up to date.
    onCoalesceTick();
    // Force-emit so QSignalSpy always observes a signal even when nothing
    // was dirty since the last natural tick.
    if (!m_dirty) {
        emit changed();
    }
    m_dirty = false;
}

}  // namespace NereusSDR
