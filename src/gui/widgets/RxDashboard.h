#pragma once

#include <QSet>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QVBoxLayout;

namespace NereusSDR {

class SliceModel;
class StatusBadge;

// RxDashboard — dense glance dashboard for RX1 state.
//
// Renders: RX1 label + frequency stack, then mode/filter/AGC always-on
// badges, then active-only badges (NR / NB / ANF / SQL). Disabled
// features don't render; layout flexes within the host QHBoxLayout.
//
// Signal-name notes (verified against SliceModel.h 2026-04-30):
//   - frequencyChanged(double) — double Hz, not qint64
//   - dspModeChanged(DSPMode)  — typed enum
//   - filterChanged(int low, int high) — two params
//   - agcModeChanged(AGCMode)  — typed enum
//   - activeNrChanged(NrSlot)  — NrSlot enum (Off/NR1/NR2/..MNR)
//   - nbModeChanged(NbMode)    — tri-state Off/NB/NB2
//   - apfEnabledChanged(bool)  — Auto-Notch Filter (closest to ANF)
//   - ssqlEnabled/amsqEnabled/fmsqEnabled — per-mode squelch; use ssql as indicator
//
// Phase 3Q Sub-PR-5/6 (E.1 + F.1).
class RxDashboard : public QWidget {
    Q_OBJECT

public:
    explicit RxDashboard(QWidget* parent = nullptr);

    void bindSlice(SliceModel* slice);
    SliceModel* slice() const noexcept { return m_slice; }

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onFrequencyChanged(double hz);
    void onModeChanged(int mode);
    void onFilterChanged(int low, int high);
    void onAgcChanged(int agcMode);
    void onNrChanged(int nrSlot);
    void onNbChanged(int nbMode);
    void onApfChanged(bool active);
    void onSsqlChanged(bool active);

private:
    void buildUi();
    void reapplyDropPriority();

    QSet<StatusBadge*> m_droppedBadges;   // badges hidden by resize, not by feature-off

    SliceModel*  m_slice{nullptr};

    QLabel*      m_rxLabel{nullptr};
    QLabel*      m_freqLabel{nullptr};
    StatusBadge* m_modeBadge{nullptr};
    StatusBadge* m_filterBadge{nullptr};
    StatusBadge* m_agcBadge{nullptr};
    StatusBadge* m_nrBadge{nullptr};
    StatusBadge* m_nbBadge{nullptr};
    StatusBadge* m_apfBadge{nullptr};
    StatusBadge* m_sqlBadge{nullptr};
};

} // namespace NereusSDR
