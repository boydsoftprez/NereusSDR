// =================================================================
// src/core/ConnectionDiagnostics.h  (NereusSDR)
// =================================================================
//
// NereusSDR-native lightweight QObject that aggregates runtime
// connection metrics from a bound PgxlConnection or TgxlConnection.
// Intended consumer: Setup -> <Device> Advanced page (Phase 4).
//
// All 10 Q_PROPERTYs share a single NOTIFY changed() signal that is
// coalesced at 1 Hz so the diagnostics grid does not repaint per
// telemetry frame.
//
// Design reference:
//   docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-design.md
//   section 4.6.
//
// AI tooling: Anthropic Claude Code.
// =================================================================

#pragma once

#include <QObject>
#include <QTimer>
#include <QDateTime>

namespace NereusSDR {

class PgxlConnection;
class TgxlConnection;

class ConnectionDiagnostics : public QObject {
    Q_OBJECT

    Q_PROPERTY(qint64  uptimeMs        READ uptimeMs        NOTIFY changed)
    Q_PROPERTY(qint64  lastRttMs       READ lastRttMs       NOTIFY changed)
    Q_PROPERTY(int     keepaliveMissed READ keepaliveMissed NOTIFY changed)
    Q_PROPERTY(int     reconnectCount  READ reconnectCount  NOTIFY changed)
    Q_PROPERTY(quint64 framesIn        READ framesIn        NOTIFY changed)
    Q_PROPERTY(quint64 framesOut       READ framesOut       NOTIFY changed)
    Q_PROPERTY(quint64 bytesIn         READ bytesIn         NOTIFY changed)
    Q_PROPERTY(quint64 bytesOut        READ bytesOut        NOTIFY changed)
    Q_PROPERTY(qint64  lastFrameMs     READ lastFrameMs     NOTIFY changed)
    Q_PROPERTY(int     faultsSession   READ faultsSession   NOTIFY changed)

public:
    explicit ConnectionDiagnostics(QObject* parent = nullptr);

    // Bind to a PGXL connection. Disconnects any prior binding first.
    // All prior accumulated counters (keepaliveMissed, reconnectCount,
    // faultsSession) are reset to 0 on rebind.
    void bindTo(PgxlConnection* c);

    // Bind to a TGXL connection. Disconnects any prior binding first.
    // Same reset semantics as bindTo(PgxlConnection*).
    void bindTo(TgxlConnection* c);

    // Returns ms since the connection was established, or 0 if disconnected.
    qint64  uptimeMs()        const;
    qint64  lastRttMs()       const noexcept { return m_lastRttMs; }
    int     keepaliveMissed() const noexcept { return m_keepaliveMissed; }
    int     reconnectCount()  const noexcept { return m_reconnectCount; }
    quint64 framesIn()        const noexcept { return m_framesIn; }
    quint64 framesOut()       const noexcept { return m_framesOut; }
    quint64 bytesIn()         const noexcept { return m_bytesIn; }
    quint64 bytesOut()        const noexcept { return m_bytesOut; }
    qint64  lastFrameMs()     const noexcept { return m_lastFrameMs; }
    int     faultsSession()   const noexcept { return m_faultsSession; }

    // Test seam: flush the 1 Hz coalesce timer immediately and emit changed().
    // Used by unit tests so they don't need to advance the event loop 1 second.
    void testFlushCoalesceTimer();

signals:
    void changed();

private slots:
    void onCoalesceTick();

private:
    void markDirty();          // sets m_dirty = true; changed() fires at next 1 Hz tick
    void resetCounters();      // zero out all accumulated counters on rebind
    void disconnectPrior();    // QObject::disconnect prior binding QMetaObject connections

    QTimer  m_coalesceTimer;
    bool    m_dirty{false};

    // Accumulated / signal-driven counters.
    qint64  m_connectedSinceMs{0};   // 0 means disconnected
    qint64  m_lastRttMs{0};
    int     m_keepaliveMissed{0};
    int     m_reconnectCount{0};
    qint64  m_lastFrameMs{0};
    int     m_faultsSession{0};

    // Polled from the bound connection on each 1 Hz tick.
    quint64 m_framesIn{0};
    quint64 m_framesOut{0};
    quint64 m_bytesIn{0};
    quint64 m_bytesOut{0};

    // Bound connection pointers (at most one non-null at a time).
    PgxlConnection* m_pgxl{nullptr};
    TgxlConnection* m_tgxl{nullptr};

    // Tracks QMetaObject::Connection handles so we can disconnect on rebind.
    QMetaObject::Connection m_connConnected;
    QMetaObject::Connection m_connDisconnected;
    QMetaObject::Connection m_connPong;
    QMetaObject::Connection m_connPingTimeout;
    QMetaObject::Connection m_connReconnect;
    QMetaObject::Connection m_connStatus;
};

}  // namespace NereusSDR
