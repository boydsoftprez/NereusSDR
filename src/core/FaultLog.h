// =================================================================
// src/core/FaultLog.h  (NereusSDR)
// =================================================================
//
// NereusSDR-native ring buffer of the last 10 fault events per device,
// persisted to AppSettings as a JSON array. Consumed by the PGXL
// advanced setup page and any future fault-history surface.
//
// Design reference: docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-design.md §4.7
//
// AI tooling: Anthropic Claude Code.

#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace NereusSDR {

/// Single fault event captured when a PGXL (or TGXL) state transitions
/// to a value beginning with "FAULT".
struct FaultEvent {
    qint64  whenMs;          // epoch ms at capture time
    QString state;           // e.g. "FAULT" or "FAULT_PROTECT"
    float   fwdAtFaultW;     // forward power (watts) at fault transition
    float   swrAtFault;      // SWR at fault transition
    float   tempAtFaultC;    // temperature (Celsius) at fault transition
    QString likelyCause;     // heuristic: "SWR trip", "Overtemp", "Drive too high", "Unknown"
};

/// Ring buffer of the last 10 fault events for a single device, keyed by an
/// AppSettings device key (e.g. "PGXL_FaultHistory" or "TGXL_FaultHistory").
///
/// The buffer is newest-first: `events().first()` is the most recent fault.
/// Persistence uses AppSettings as a compact JSON string so no separate file
/// is needed.
class FaultLog : public QObject {
    Q_OBJECT
public:
    explicit FaultLog(const QString& deviceKey, QObject* parent = nullptr);

    /// Returns all events, newest first. Size is at most 10.
    QVector<FaultEvent> events() const;

    /// Prepend ev to the ring buffer. If size exceeds 10 after the prepend,
    /// the oldest entry (tail) is evicted. Persists and emits changed().
    void capture(const FaultEvent& ev);

    /// Remove all events and persist the empty state. Emits changed().
    void clear();

    /// Heuristic: derive a human-readable cause from the telemetry values
    /// recorded at fault time. Priority order: SWR -> temp -> fwd -> unknown.
    /// No instance state is needed, so this is a static helper.
    static QString likelyCauseFor(float fwd, float swr, float temp);

signals:
    void changed();

private:
    void load();
    void save() const;

    QString             m_deviceKey;
    QVector<FaultEvent> m_events;    // newest-first; capped at 10
};

}  // namespace NereusSDR
