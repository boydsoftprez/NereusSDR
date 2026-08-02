// src/gui/SpectrumOverlayPanel.h
// Left overlay button strip for SpectrumWidget.
// Ported from AetherSDR SpectrumOverlayMenu — same visual style, adapted
// for NereusSDR's OpenHPSDR/Thetis feature set.
//
// 8 buttons (68×22px, stacked vertically) + 4 flyout sub-panels.
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
//                 to the resolved pan slice's vaxChannel() with echo prevention. IQ Ch
//                 stays feature-flagged off (design spec §6.7/§11.3 —
//                 audio/SendIqToVax stored-but-not-active). J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

#pragma once

#include <QWidget>
#include <QVector>
#include <functional>

class QPushButton;
class QComboBox;
class QSlider;
class QLabel;
class QEvent;
class QMouseEvent;

namespace NereusSDR {

struct BoardCapabilities;
class RadioModel;
class SliceModel;

class SpectrumOverlayPanel : public QWidget {
    Q_OBJECT

public:
    explicit SpectrumOverlayPanel(QWidget* parent = nullptr);

    /// The panadapter this strip is drawn on. A control rendered on a pan acts
    /// on THAT pan -- the id travels with the signals rather than the consumer
    /// resolving an implicit "active" pan, so what a visible button does never
    /// depends on hidden state. Same shape as AetherSDR's SpectrumOverlayMenu,
    /// which carries m_panId and emits addRxClicked(m_panId)
    /// (SpectrumOverlayMenu.cpp:315 [@c6481cbf]).
    void setPanId(const QString& panId) { m_panId = panId; }
    QString panId() const { return m_panId; }

    // Bind this overlay panel to a RadioModel. Enables the VAX Ch combo
    // and wires it bidirectionally to the resolved pan slice. Safe to
    // call multiple times — each rebind drops prior SliceModel connections.
    // The IQ Ch combo remains disabled (feature-flagged per design spec
    // §6.7/§11.3 — audio/SendIqToVax is stored-but-not-active).
    void setRadioModel(RadioModel* model);

    using SliceResolver = std::function<SliceModel*()>;
    void setSliceResolver(SliceResolver resolver);
    void bindToPanSlice();

    // Raise panel and all flyouts above siblings.
    void raiseAll();

public slots:
    // Phase 3P-I-a T18 — repopulate antenna combos from caps and hide
    // both RX/TX rows on boards without Alex (HL2/Atlas). Also reseeds
    // the combo's current value from the resolved slice so the label matches the
    // new port list (e.g. a persisted ANT3 preserves after reconnect).
    void setBoardCapabilities(const NereusSDR::BoardCapabilities& caps);

signals:
    // Band flyout
    void bandSelected(const QString& bandName, double freqHz, const QString& mode);

    // Display flyout
    void wfColorGainChanged(int gain);
    void wfBlackLevelChanged(int level);
    void colorSchemeChanged(int scheme);
    void cursorFreqVisibleChanged(bool on);  // B8 Task 21
    void fillColorChanged(const QColor& color); // B8 Task 22
    void fillAlphaChanged(float alpha);  // 0.0..1.0  B8 fix-up
    void openSetupRequested(const QString& page); // B8 Task 24

    // Zoom buttons
    void zoomSegment();
    void zoomBand();
    void zoomOut();
    void zoomIn();

    // Collapse
    void collapsed(bool isCollapsed);

    // Clarity adaptive tuning (Phase 3G-9c)
    void clarityRetuneRequested();

    /// Add an RX slice on the pan this strip belongs to. Carries the pan id so
    /// the consumer never has to guess which pan the operator meant.
    void addRxClicked(const QString& panId);

    /// Add a notch on the pan this strip belongs to. Carries the pan id for
    /// the same reason addRxClicked does: a control rendered on a pan acts on
    /// THAT pan, never on whichever pan is implicitly "active".
    ///
    /// A pure signal. The notch centre is composed by NotchModel and the add
    /// is issued by MainWindow, so no DSP logic lands in this file.
    void addTnfClicked(const QString& panId);

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
    /// Which panadapter this strip is drawn on; see setPanId.
    QString m_panId;

    // Layout
    void updateLayout();
    void toggle();
    void buildZoomButtons();

    // Flyout builders
    void buildBandFlyout();
    void buildAntFlyout();
    void buildDisplayFlyout();
    void buildVaxFlyout();

    // Flyout toggles
    void toggleBandFlyout();
    void toggleAntFlyout();
    void toggleDisplayFlyout();
    void toggleVaxFlyout();

    // Auto-close helper
    void hideFlyout();

    // ── Main button strip ────────────────────────────────────────────────
    QPushButton*         m_collapseBtn{nullptr};
    QVector<QPushButton*> m_menuBtns;   // indices 0-6 (buttons 2-8)
    bool                 m_expanded{true};

    // ── Active flyout tracking (one visible at a time) ───────────────────
    QWidget*     m_activeFlyout{nullptr};
    QPushButton* m_activeButton{nullptr};

    // ── Band flyout ──────────────────────────────────────────────────────
    QWidget* m_bandFlyout{nullptr};

    // ── ANT flyout ───────────────────────────────────────────────────────
    QWidget*     m_antFlyout{nullptr};
    // Phase 3P-I-a T18 — row wrappers so setBoardCapabilities can
    // setVisible(false) on both the label and the combo together.
    QWidget*     m_rxAntRow{nullptr};
    QWidget*     m_txAntRow{nullptr};
    QComboBox*   m_rxAntCmb{nullptr};
    QComboBox*   m_txAntCmb{nullptr};
    // Stored so the widget→model connection ordering inside setRadioModel
    // can replicate the per-pan rebind pattern used for VAX.
    QMetaObject::Connection m_rxAntConn;
    QMetaObject::Connection m_txAntConn;
    QSlider*     m_rfGainSlider{nullptr};
    QLabel*      m_rfGainLabel{nullptr};
    QPushButton* m_wnbBtn{nullptr};

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
    // m_heatMapBtn / m_noiseFloorBtn / m_noiseFloorSlider / m_noiseFloorLabel /
    // m_weightedAvgBtn removed B8 Task 23 — unbacked theatre controls.

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
    SliceResolver            m_sliceResolver;
    QMetaObject::Connection  m_vaxChannelConn;
    bool                     m_updatingFromModel{false};

    SliceModel* resolvedSlice() const;

    // ── Waterfall zoom buttons (bottom-left of spectrum widget) ──────────
    QWidget*     m_zoomStrip{nullptr};   // container for the 4 zoom buttons
    QPushButton* m_zoomSegBtn{nullptr};
    QPushButton* m_zoomBandBtn{nullptr};
    QPushButton* m_zoomOutBtn{nullptr};
    QPushButton* m_zoomInBtn{nullptr};
};

} // namespace NereusSDR
