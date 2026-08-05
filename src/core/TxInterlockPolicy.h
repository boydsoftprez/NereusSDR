// =================================================================
// src/core/TxInterlockPolicy.h  (NereusSDR)
// =================================================================
//
// NereusSDR-native operator-toggled policy that gates TX based on PGXL
// amplifier state. Hooked into MoxController::onTxRequested.
//
// Three modes:
//   Disabled -- never interferes with TX (default; matches AetherSDR behavior).
//   Warn     -- allows TX but emits warned() so the UI can toast the operator.
//   Block    -- prevents TX and emits denied().
//
// An optional SWR gate (swrGateEnabled + swrGateMax) adds a second check:
// when enabled, high-SWR conditions trigger the same Warn/Block action.
//
// The graceMs field suppresses the SWR gate check for <graceMs> milliseconds
// after the amplifier transitions to OPERATE. This avoids false interlock
// trips during the amplifier warm-up transient before the SWR reading
// stabilises. m_ampLastOperateMs is set by onAmpStateChanged on the
// not-in-operate -> in-operate edge and consulted in evaluateTxRequest.
//
// Design reference: docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-design.md §4.9
//
// AI tooling: Anthropic Claude Code.

#pragma once

#include <QObject>
#include <QString>

namespace NereusSDR {

class TxInterlockPolicy : public QObject {
    Q_OBJECT
public:
    enum Mode { Disabled, Warn, Block };
    Q_ENUM(Mode)

    Q_PROPERTY(Mode  mode           READ mode           WRITE setMode           NOTIFY changed)
    Q_PROPERTY(int   graceMs        READ graceMs        WRITE setGraceMs        NOTIFY changed)
    Q_PROPERTY(bool  swrGateEnabled READ swrGateEnabled WRITE setSwrGateEnabled NOTIFY changed)
    Q_PROPERTY(float swrGateMax     READ swrGateMax     WRITE setSwrGateMax     NOTIFY changed)

    explicit TxInterlockPolicy(QObject* parent = nullptr);

    Mode  mode()           const { return m_mode; }
    int   graceMs()        const { return m_graceMs; }
    bool  swrGateEnabled() const { return m_swrGateEnabled; }
    float swrGateMax()     const { return m_swrGateMax; }

    /// Evaluate whether a TX request should be allowed.
    ///
    /// Returns true if TX may proceed. When the result is false (Block mode
    /// only), denied() has been emitted. When true but a condition is
    /// abnormal (Warn mode), warned() has been emitted.
    ///
    /// Decision tree (in priority order):
    ///   1. Disabled mode    -> always returns true, no signals emitted.
    ///   2. Amp present but not in OPERATE -> Warn: return true + warned();
    ///                                        Block: return false + denied().
    ///   3. SWR gate enabled AND currentSwr > swrGateMax
    ///                       -> same Warn/Block action as above.
    ///   4. Otherwise        -> returns true.
    bool evaluateTxRequest(bool ampPresent, bool ampInOperate, float currentSwr);

public slots:
    void setMode(Mode m);
    void setGraceMs(int ms);
    void setSwrGateEnabled(bool on);
    void setSwrGateMax(float x);

    // Phase 3P-II review fix I2: update the OPERATE transition timestamp so
    // the grace period gate in evaluateTxRequest can suppress the SWR check
    // during the window immediately after the amplifier enters OPERATE.
    // Called by MoxController::onAmpStateChanged which is already wired from
    // RadioModel amplifierChanged / ampStateChanged lambdas.
    void onAmpStateChanged(bool ampPresent, bool ampInOperate);

signals:
    void warned(const QString& reason);
    void denied(const QString& reason);
    void changed();

private:
    Mode  m_mode{Disabled};
    int   m_graceMs{3000};
    bool  m_swrGateEnabled{false};
    float m_swrGateMax{3.0f};

    // Timestamp (epoch ms) of the last not-in-operate -> in-operate transition.
    // Set by onAmpStateChanged; consulted by evaluateTxRequest to suppress the
    // SWR gate check during the grace window.
    qint64 m_ampLastOperateMs{0};
    // Previous operate state tracked to detect rising edge only.
    bool   m_prevAmpInOperate{false};
};

}  // namespace NereusSDR
