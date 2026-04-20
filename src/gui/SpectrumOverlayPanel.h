// src/gui/SpectrumOverlayPanel.h
// Left overlay button strip for SpectrumWidget.
// Ported from AetherSDR SpectrumOverlayMenu — same visual style, adapted
// for NereusSDR's OpenHPSDR/Thetis feature set.
//
// 10 buttons (68×22px, stacked vertically) + 5 flyout sub-panels.
// Positioned via move() as a child of the spectrum widget.

// =================================================================
// src/gui/SpectrumOverlayPanel.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Ported from AetherSDR `src/gui/SpectrumOverlayMenu.{h,cpp}`
//                 (left button strip + 5 flyout panels).
//   2026-04-20 — Phase 3O Sub-Phase 9 Task 9.2c (issue #70 fold-in):
//                 added setRadioModel() so the previously-disabled VAX Ch
//                 combo on the left-edge overlay is now wired bidirectionally
//                 to slice 0's vaxChannel() with echo prevention. IQ Ch
//                 stays feature-flagged off (design spec §6.7/§11.3 —
//                 audio/SendIqToVax stored-but-not-active). J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

#pragma once

#include <QWidget>
#include <QVector>

class QPushButton;
class QComboBox;
class QSlider;
class QLabel;
class QEvent;
class QMouseEvent;

namespace NereusSDR {

class RadioModel;

class SpectrumOverlayPanel : public QWidget {
    Q_OBJECT

public:
    explicit SpectrumOverlayPanel(QWidget* parent = nullptr);

    // Bind this overlay panel to a RadioModel. Enables the VAX Ch combo
    // and wires it bidirectionally to slice 0's vaxChannel(). Safe to
    // call multiple times — each rebind drops prior SliceModel connections.
    // The IQ Ch combo remains disabled (feature-flagged per design spec
    // §6.7/§11.3 — audio/SendIqToVax is stored-but-not-active).
    void setRadioModel(RadioModel* model);

    // Raise panel and all flyouts above siblings.
    void raiseAll();

signals:
    // Band flyout
    void bandSelected(const QString& bandName, double freqHz, const QString& mode);

    // DSP flyout toggles
    void nrToggled(bool on);
    void nbToggled(bool on);
    void anfToggled(bool on);
    void snbToggled(bool on);

    // Display flyout
    void wfColorGainChanged(int gain);
    void wfBlackLevelChanged(int level);
    void colorSchemeChanged(int scheme);

    // Zoom buttons
    void zoomSegment();
    void zoomBand();
    void zoomOut();
    void zoomIn();

    // Collapse
    void collapsed(bool isCollapsed);

    // Clarity adaptive tuning (Phase 3G-9c)
    void clarityRetuneRequested();

    // NYI placeholders
    void addRxClicked();
    void addTnfClicked();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

public:
    // Reposition zoom buttons to bottom-left of parent spectrum widget.
    // Must be called after parent resize.
    void repositionZoomButtons();

    // Phase 3G-9c: update the Clarity status badge.
    // active=true → green "C", paused=true → amber "C", both false → hidden.
    void setClarityStatus(bool active, bool paused);

private:
    // Layout
    void updateLayout();
    void toggle();
    void buildZoomButtons();

    // Flyout builders
    void buildBandFlyout();
    void buildAntFlyout();
    void buildDspFlyout();
    void buildDisplayFlyout();
    void buildVaxFlyout();

    // Flyout toggles
    void toggleBandFlyout();
    void toggleAntFlyout();
    void toggleDspFlyout();
    void toggleDisplayFlyout();
    void toggleVaxFlyout();

    // Auto-close helper
    void hideFlyout();

    // ── Main button strip ────────────────────────────────────────────────
    QPushButton*         m_collapseBtn{nullptr};
    QVector<QPushButton*> m_menuBtns;   // indices 0-8 (buttons 2-10)
    bool                 m_expanded{true};

    // ── Active flyout tracking (one visible at a time) ───────────────────
    QWidget*     m_activeFlyout{nullptr};
    QPushButton* m_activeButton{nullptr};

    // ── Band flyout ──────────────────────────────────────────────────────
    QWidget* m_bandFlyout{nullptr};

    // ── ANT flyout ───────────────────────────────────────────────────────
    QWidget*     m_antFlyout{nullptr};
    QComboBox*   m_rxAntCmb{nullptr};
    QComboBox*   m_txAntCmb{nullptr};
    QSlider*     m_rfGainSlider{nullptr};
    QLabel*      m_rfGainLabel{nullptr};
    QPushButton* m_wnbBtn{nullptr};

    // ── DSP flyout ───────────────────────────────────────────────────────
    QWidget*     m_dspFlyout{nullptr};
    QPushButton* m_nrBtn{nullptr};
    QPushButton* m_nbBtn{nullptr};
    QPushButton* m_snbBtn{nullptr};
    QPushButton* m_anfBtn{nullptr};
    QPushButton* m_binBtn{nullptr};
    QPushButton* m_mnfDspBtn{nullptr};
    QSlider*     m_nrSlider{nullptr};
    QLabel*      m_nrLabel{nullptr};
    QSlider*     m_nbSlider{nullptr};
    QLabel*      m_nbLabel{nullptr};

    // ── Display flyout ───────────────────────────────────────────────────
    QWidget*     m_displayFlyout{nullptr};
    QComboBox*   m_colorSchemeCmb{nullptr};
    QSlider*     m_wfGainSlider{nullptr};
    QLabel*      m_wfGainLabel{nullptr};
    QSlider*     m_wfBlackSlider{nullptr};
    QLabel*      m_wfBlackLabel{nullptr};
    QSlider*     m_fillAlphaSlider{nullptr};
    QLabel*      m_fillAlphaLabel{nullptr};
    QPushButton* m_fillColorBtn{nullptr};
    QPushButton* m_showGridBtn{nullptr};
    QPushButton* m_cursorFreqBtn{nullptr};
    QPushButton* m_heatMapBtn{nullptr};
    QPushButton* m_noiseFloorBtn{nullptr};
    QSlider*     m_noiseFloorSlider{nullptr};
    QLabel*      m_noiseFloorLabel{nullptr};
    QPushButton* m_weightedAvgBtn{nullptr};

    // ── Clarity badge + Re-tune (Phase 3G-9c) ────────────────────────────
    QLabel*      m_clarityBadge{nullptr};
    QPushButton* m_clarityRetuneBtn{nullptr};

    // ── VAX flyout ───────────────────────────────────────────────────────
    QWidget*   m_vaxFlyout{nullptr};
    QComboBox* m_vaxCmb{nullptr};
    QComboBox* m_vaxIqCmb{nullptr};

    // ── VAX model binding (Phase 3O Sub-Phase 9 Task 9.2c) ───────────────
    // m_vaxChannelConn stores the SliceModel::vaxChannelChanged → combo
    // connection so a rebind via setRadioModel() can explicitly drop the
    // prior subscription. m_updatingFromModel guards the echo path
    // (model → widget) from re-triggering the widget → model side.
    RadioModel*              m_radioModel{nullptr};
    QMetaObject::Connection  m_vaxChannelConn;
    bool                     m_updatingFromModel{false};

    // Seat (or re-seat) the Model→Widget subscription onto whichever
    // SliceModel currently occupies index 0. Called from setRadioModel()
    // at bind time AND from the sliceAdded/sliceRemoved listeners so a
    // late-arriving or reshuffled slice 0 rebinds cleanly.
    void bindToSliceZero();

    // ── Waterfall zoom buttons (bottom-left of spectrum widget) ──────────
    QWidget*     m_zoomStrip{nullptr};   // container for the 4 zoom buttons
    QPushButton* m_zoomSegBtn{nullptr};
    QPushButton* m_zoomBandBtn{nullptr};
    QPushButton* m_zoomOutBtn{nullptr};
    QPushButton* m_zoomInBtn{nullptr};
};

} // namespace NereusSDR
