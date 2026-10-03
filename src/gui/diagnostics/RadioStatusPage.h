// =================================================================
// src/gui/diagnostics/RadioStatusPage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Diagnostics → Radio Status dashboard.
// Thetis surfaces these readouts piecemeal across Front Console /
// PA Settings / main meter / etc. NereusSDR consolidates them into
// a single tab backed by Phase 3P-H Task 1 models (RadioStatus,
// SettingsHygiene) + Phase 3P-E HermesLiteBandwidthMonitor.
//
// No direct Thetis port at this layer; data shapes ported in Task 1.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-20 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-26 - R-R3-32 (remote-window parity Task 14): the Connection
//                Quality figures from RadioModel::hl2LinkFigures(), the
//                Core's HL2 link in a remote window and said so;
//                unavailable, never 0, when absent. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "gui/SetupPage.h"

#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QFrame>
#include <QListWidget>
#include <QTimer>
#include <QElapsedTimer>

namespace NereusSDR {

class RadioStatus;
class SettingsHygiene;
class HermesLiteBandwidthMonitor;

// RadioStatusPage — the Diagnostics → Radio Status dashboard.
//
// Layout: top status bar (4 columns) + 5 cards in a 3+2 grid:
//   Row 1: PA Status | Forward/Reflected/SWR | PTT Source
//   Row 2: Connection Quality summary | Settings Hygiene warnings
//
// The page holds a non-owning RadioModel* so it can access
// radioStatus(), bwMonitor(), and settingsHygiene() accessors.
// It wires signals to update UI in real time.
class RadioStatusPage : public SetupPage {
    Q_OBJECT

public:
    void setStationSettingsAvailable(bool available, const QString& reason) override;
    explicit RadioStatusPage(RadioModel* model = nullptr, QWidget* parent = nullptr);

private slots:
    void onPaTemperatureChanged(double celsius);
    void onPaCurrentChanged(double amps);
    // R-R3-32 / R-R3-46 (parity Task 6): the PA card from
    // RadioModel::paReadings() (this window's radio, or the Core's), an
    // absent reading shown as unavailable.
    void refreshPaReadings();
    void onPowerChanged(double forward, double reflected, double swr);
    // The power readouts from the model's RadioStatus as it stands now.
    void refreshPower();
    void onPttChanged();
    void onIssuesChanged();
    void onUptimeTick();
    void onBwPollTick();

private:
    void buildStatusBar(QFrame* bar);
    void buildPaStatusCard(QFrame* card);
    void buildPowerCard(QFrame* card);
    void buildPttCard(QFrame* card);
    void buildConnectionCard(QFrame* card);
    void buildHygieneCard(QFrame* card);

    void refreshPttPills();
    void refreshHygieneRows();

    RadioModel* m_model{nullptr};

    // ── Status bar labels ─────────────────────────────────────────────────
    QLabel* m_radioLabel{nullptr};
    QLabel* m_uptimeLabel{nullptr};
    QLabel* m_firmwareLabel{nullptr};
    QLabel* m_modeLabel{nullptr};

    // ── PA Status card ────────────────────────────────────────────────────
    QLabel*       m_paTemperatureLabel{nullptr};
    QLabel*       m_paCurrentLabel{nullptr};
    QLabel*       m_paVoltageLabel{nullptr};
    QLabel*       m_paTitleLabel{nullptr};
    QProgressBar* m_paTempBar{nullptr};
    QProgressBar* m_paCurrentBar{nullptr};

    // Last published °C value, cached so the °C / °F toggle can re-format
    // the label without waiting for the next telemetry sample.  The
    // progress bar always renders against the °C-canonical scale; only
    // the text label converts.
    double m_paTempLastCelsius{0.0};

    // ── Power card ────────────────────────────────────────────────────────
    QLabel*       m_forwardLabel{nullptr};
    QLabel*       m_reflectedLabel{nullptr};
    QLabel*       m_swrLabel{nullptr};
    QProgressBar* m_forwardBar{nullptr};
    QProgressBar* m_swrBar{nullptr};

    // ── PTT card ──────────────────────────────────────────────────────────
    QLabel*      m_pttActiveLabel{nullptr};
    QListWidget* m_pttHistoryList{nullptr};
    // PTT source pill buttons (not checkable — visual only)
    QPushButton* m_pttPills[7]{};

    // ── Connection quality card ───────────────────────────────────────────
    QLabel* m_bwEp6Label{nullptr};
    QLabel* m_bwEp2Label{nullptr};
    QLabel* m_bwThrottleLabel{nullptr};
    QLabel* m_bwSeqGapLabel{nullptr};
    // R-R3-32 (parity Task 14): says "from the Core" in a remote window.
    QLabel* m_connTitleLabel{nullptr};

    // ── Settings hygiene card ─────────────────────────────────────────────
    QListWidget*  m_issueList{nullptr};
    QPushButton*  m_repairBtn{nullptr};
    QPushButton*  m_forgetBtn{nullptr};

    // ── Timers ────────────────────────────────────────────────────────────
    QTimer        m_uptimeTimer;
    QTimer        m_bwPollTimer;
};

} // namespace NereusSDR
