// src/gui/SpectrumOverlayPanel.h
// Left overlay button strip for SpectrumWidget.
// Ported from AetherSDR SpectrumOverlayMenu — same visual style, adapted
// for NereusSDR's OpenHPSDR/Thetis feature set.
//
// 10 buttons (68×22px, stacked vertically) + 5 flyout sub-panels.
// Positioned via move() as a child of the spectrum widget.
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

class SpectrumOverlayPanel : public QWidget {
    Q_OBJECT

public:
    explicit SpectrumOverlayPanel(QWidget* parent = nullptr);

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
    void buildDaxFlyout();

    // Flyout toggles
    void toggleBandFlyout();
    void toggleAntFlyout();
    void toggleDspFlyout();
    void toggleDisplayFlyout();
    void toggleDaxFlyout();

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

    // ── DAX flyout ───────────────────────────────────────────────────────
    QWidget*   m_daxFlyout{nullptr};
    QComboBox* m_daxCmb{nullptr};
    QComboBox* m_daxIqCmb{nullptr};

    // ── Waterfall zoom buttons (bottom-left of spectrum widget) ──────────
    QWidget*     m_zoomStrip{nullptr};   // container for the 4 zoom buttons
    QPushButton* m_zoomSegBtn{nullptr};
    QPushButton* m_zoomBandBtn{nullptr};
    QPushButton* m_zoomOutBtn{nullptr};
    QPushButton* m_zoomInBtn{nullptr};
};

} // namespace NereusSDR
