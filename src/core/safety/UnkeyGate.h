// no-port-check: NereusSDR-original.
// =================================================================
// src/core/safety/UnkeyGate.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 34 (R-IOS-03; remote design section 12.2, "remote TX
// handoff needs an unkey-confirmed gate"): unkey the transmitter the normal
// way and say when it reached receive.
//
// unkey(reason, context, done) releases the key (the owner's unkey
// function: TUNE and two-tone end their own way, any other key through
// MoxController::setMox(false)) and calls done once:
//   - Confirmed: MOX reached receive (MoxController::rxReady, or already in
//     receive when asked);
//   - TimedOut: 2000 ms passed without it. Before done runs, the emergency
//     stop (RadioModel::stopTransmitNow, Task 33) has been applied, so the
//     transmitter is off either way.
// Nothing is called when `context` is gone (a null context never goes).
//
// Timing uses an injected scheduler, never a sleep: a test takes the
// 2000 ms timeout into its own hands.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 34 (R-IOS-03), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================
#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>
#include <memory>

namespace NereusSDR {

class MoxController;

enum class UnkeyOutcome {
    Confirmed,  ///< MOX reached receive.
    TimedOut,   ///< 2000 ms without it; the emergency stop has been applied.
};

class UnkeyGate : public QObject {
    Q_OBJECT

public:
    /// How long the gate waits for MOX to reach receive.
    static constexpr int kConfirmTimeoutMs = 2000;

    /// Releases the key now (the normal unkey).
    using UnkeyFn = std::function<void()>;
    /// The emergency stop, with its reason (RadioModel::stopTransmitNow).
    using StopNowFn = std::function<void(const QString& reason)>;
    /// Runs `fire` once after `ms`, unless `context` is gone first.
    using Scheduler = std::function<void(int ms, QObject* context, std::function<void()> fire)>;

    UnkeyGate(MoxController* mox, UnkeyFn unkey, StopNowFn stopNow, QObject* parent = nullptr);
    ~UnkeyGate() override;

    /// Test seam: the timer. The default is QTimer::singleShot.
    void setScheduler(Scheduler scheduler);

    void unkey(const QString& reason, QObject* context, std::function<void(UnkeyOutcome)> done);

    /// Unkeys asked for and not yet answered.
    int pendingCount() const;

private:
    struct Pending;
    void settle(const std::shared_ptr<Pending>& pending, UnkeyOutcome outcome);
    bool inReceive() const;

    QPointer<MoxController> m_mox;
    UnkeyFn m_unkey;
    StopNowFn m_stopNow;
    Scheduler m_scheduler;
    QList<std::shared_ptr<Pending>> m_pending;
};

} // namespace NereusSDR
