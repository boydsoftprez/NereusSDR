#pragma once

#include "gui/SetupPage.h"

class QSlider;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QLabel;
class QPushButton;

namespace NereusSDR {

class ColorSwatchButton;

// ---------------------------------------------------------------------------
// Appearance > Colors & Theme
// ---------------------------------------------------------------------------
class ColorsThemePage : public SetupPage {
    Q_OBJECT
public:
    explicit ColorsThemePage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: Spectrum — functional colour pickers (live, wired to SpectrumWidget).
    ColorSwatchButton* m_traceFillColorBtn{nullptr};   // → setFillColor
    ColorSwatchButton* m_gridColorBtn{nullptr};        // → setGridColor       (G9)
    ColorSwatchButton* m_gridFineColorBtn{nullptr};    // → setGridFineColor   (G10)
    ColorSwatchButton* m_hGridColorBtn{nullptr};       // → setHGridColor      (G11)
    ColorSwatchButton* m_gridTextColorBtn{nullptr};    // → setGridTextColor   (G12)
    ColorSwatchButton* m_bandEdgeColorBtn{nullptr};    // → setBandEdgeColor   (G6)
    ColorSwatchButton* m_rxZeroLineColorBtn{nullptr};  // → setRxZeroLineColor (G13)
    ColorSwatchButton* m_txZeroLineColorBtn{nullptr};  // → setTxZeroLineColor (G13 TX)
    // Plan 4 D9b — RX + TX passband overlay colour pickers (live).
    ColorSwatchButton* m_rxFilterColorBtn{nullptr};    // → setRxFilterColor
    ColorSwatchButton* m_txFilterColorBtn{nullptr};    // → setTxFilterColor

    // Section: Waterfall
    ColorSwatchButton* m_wfLowColorBtn{nullptr};       // → waterfall low colour (W10)
};

// ---------------------------------------------------------------------------
// Appearance > Meter Styles
// ---------------------------------------------------------------------------
class MeterStylesPage : public SetupPage {
    Q_OBJECT
public:
    explicit MeterStylesPage(RadioModel* model, QWidget* parent = nullptr);

    // R-R3-21: shows the S-meter's saved face, peak hold and decay again
    // (a right-click change on the S-meter while this page is open).
    void reloadSMeterSettings();

signals:
    // R-R3-21: the S-meter settings its right-click menu holds. The page
    // saves them under the S-meter's own keys; MainWindow applies them to
    // the S-meter on screen (SetupDialog forwards these).
    void sMeterFaceChanged(int faceStyle);           // SMeterWidget::FaceStyle
    void sMeterPeakHoldChanged(bool enabled);
    void sMeterPeakDecayChanged(const QString& rate); // "Fast" / "Medium" / "Slow"

private:
    void buildUI();

    // Section: S-Meter
    QComboBox* m_faceCombo{nullptr};       // SMeterWidget faces
    QCheckBox* m_peakHoldToggle{nullptr};
    QComboBox* m_decayRateCombo{nullptr};  // Fast / Medium / Slow

    // Section: VFO Flag
    QCheckBox* m_smallModeFilterToggle{nullptr};
};

// ---------------------------------------------------------------------------
// Appearance > Skins
// ---------------------------------------------------------------------------
class SkinsPage : public SetupPage {
    Q_OBJECT
public:
    explicit SkinsPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: Skins
    QLabel*      m_skinListLabel{nullptr};  // placeholder for future list widget
    QPushButton* m_loadBtn{nullptr};
    QPushButton* m_saveBtn{nullptr};
    QPushButton* m_importBtn{nullptr};
};

// ---------------------------------------------------------------------------
// Appearance > Collapsible Display
// ---------------------------------------------------------------------------
class CollapsibleDisplayPage : public SetupPage {
    Q_OBJECT
public:
    explicit CollapsibleDisplayPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: Collapsible
    QSpinBox*  m_widthSpin{nullptr};
    QSpinBox*  m_heightSpin{nullptr};
    QCheckBox* m_enableToggle{nullptr};
};

} // namespace NereusSDR
