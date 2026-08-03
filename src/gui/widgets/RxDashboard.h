#pragma once

#include <QChar>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QVBoxLayout;

namespace NereusSDR {

class SliceModel;
class StatusBadge;

// RxDashboard — dense glance dashboard for the ACTIVE slice's RX state.
//
// Renders a single dense row: slice tag, mode, filter, AGC, NR, NB, APF,
// SQL. No per-badge borders, no BadgePair stacking.
//
// The frequency display lived here in earlier revisions but moved to the
// VFO Flag (2026-04-30) — there's no need to repeat it in the chrome.
//
// Layout decisions (fold/drop under width pressure) no longer live here.
// ChromeBarController (Task A8) owns folding and calls badgeForRung() to
// register each foldable badge individually at its rung. Mode and filter
// never fold, so they are not on the ladder.
//
// Task A8 fix round 1: the on*Changed handlers below no longer call
// setVisible() on the pills directly. ChromeBarController::addItem's
// contract requires it to be the sole writer of a registered widget's
// visibility; a pill also directly setVisible()'d from here would desync
// from the controller's next relayout(), which force-asserts visibility
// for every registered item on any rung change with no knowledge of
// DSP-active state. Each handler instead emits badgeAvailabilityChanged,
// which MainWindow forwards to ChromeBarController::setItemAvailable (and
// reports the badge's new width via setNaturalWidth, since
// StatusBadge::setLabel changes its own minimum width live).
//
// Signal-name notes (verified against SliceModel.h 2026-04-30):
//   - dspModeChanged(DSPMode)         — typed enum
//   - filterChanged(int low, int high) — two params
//   - agcModeChanged(AGCMode)         — typed enum
//   - activeNrChanged(NrSlot)         — NrSlot enum (Off/NR1/NR2/...MNR)
//   - nbModeChanged(NbMode)           — tri-state Off/NB/NB2
//   - apfEnabledChanged(bool)         — Auto-Notch Filter (closest to ANF)
//   - ssqlEnabled/amsqEnabled/fmsqEnabled — per-mode squelch; ssql as indicator
//
// Phase 3Q Sub-PR-5/6 (E.1 + F.1) + 2026-04-30 dashboard polish +
// 2026-08-02 bottom-banner cleanup (Task A5): follows the active slice
// instead of slice 0, dense single row replaces the 3-stage responsive
// ladder. See docs/architecture/2026-08-02-bottom-banner-and-pan-menu-design.md §4.2.
class RxDashboard : public QWidget {
    Q_OBJECT

public:
    explicit RxDashboard(QWidget* parent = nullptr);

    void bindSlice(SliceModel* slice);
    SliceModel* slice() const noexcept { return m_slice; }

    /// Slice this dashboard is describing. Prepended to the row so a
    /// multi-pan operator can tell which slice the readings belong to.
    void setSliceLetter(QChar letter);
    QChar sliceLetter() const noexcept { return m_sliceLetter; }

    /// Mode badge text, for tests and for the overflow tooltip.
    QString modeText() const;

    /// Badge that folds at this rung, or nullptr if the rung is not ours.
    /// 5 SQL, 6 APF, 7 NB, 8 NR, 9 AGC. Mode and filter never fold.
    StatusBadge* badgeForRung(int rung) const;

signals:
    /// A pill's DSP-active state (and/or its content, hence its width)
    /// just changed. rung matches badgeForRung's mapping (5 SQL .. 9 AGC).
    /// available is the badge's new should-show state; AGC has no "off"
    /// state and always reports true, purely so its width is re-checked
    /// too (StatusBadge::setLabel changes minimum width live). The
    /// receiver (MainWindow) is expected to resolve the widget via
    /// badgeForRung(rung) and forward both facts to
    /// ChromeBarController::setItemAvailable / setNaturalWidth.
    void badgeAvailabilityChanged(int rung, bool available);

private slots:
    void onModeChanged(int mode);
    void onFilterChanged(int low, int high);
    void onAgcChanged(int agcMode);
    void onNrChanged(int nrSlot);
    void onNbChanged(int nbMode);
    void onApfChanged(bool active);
    void onSsqlChanged(bool active);

private:
    void buildUi();

    QChar        m_sliceLetter{QLatin1Char('A')};
    QLabel*      m_sliceTag{nullptr};
    SliceModel*  m_slice{nullptr};
    StatusBadge* m_modeBadge{nullptr};
    StatusBadge* m_filterBadge{nullptr};
    StatusBadge* m_agcBadge{nullptr};
    StatusBadge* m_nrBadge{nullptr};
    StatusBadge* m_nbBadge{nullptr};
    StatusBadge* m_apfBadge{nullptr};
    StatusBadge* m_sqlBadge{nullptr};
};

} // namespace NereusSDR
