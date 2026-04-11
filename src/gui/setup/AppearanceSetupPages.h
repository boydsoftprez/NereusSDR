#pragma once

#include "gui/SetupPage.h"

class QSlider;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QLabel;
class QPushButton;

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Appearance > Colors & Theme
// ---------------------------------------------------------------------------
class ColorsThemePage : public SetupPage {
    Q_OBJECT
public:
    explicit ColorsThemePage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: Background
    QLabel* m_bgColorLabel{nullptr};    // color swatch placeholder
    QLabel* m_textColorLabel{nullptr};  // color swatch placeholder

    // Section: Spectrum
    QLabel* m_gridColorLabel{nullptr};   // color swatch placeholder
    QLabel* m_traceColorLabel{nullptr};  // color swatch placeholder
    QLabel* m_filterColorLabel{nullptr}; // color swatch placeholder

    // Section: Waterfall
    QLabel* m_wfLowColorLabel{nullptr};  // color swatch placeholder
    QLabel* m_wfHighColorLabel{nullptr}; // color swatch placeholder
};

// ---------------------------------------------------------------------------
// Appearance > Meter Styles
// ---------------------------------------------------------------------------
class MeterStylesPage : public SetupPage {
    Q_OBJECT
public:
    explicit MeterStylesPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: S-Meter
    QComboBox* m_typeCombo{nullptr};       // Arc/Bar/Digital
    QCheckBox* m_peakHoldToggle{nullptr};
    QSlider*   m_decayRateSlider{nullptr};
};

// ---------------------------------------------------------------------------
// Appearance > Gradients
// ---------------------------------------------------------------------------
class GradientsPage : public SetupPage {
    Q_OBJECT
public:
    explicit GradientsPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: Waterfall Gradient
    QLabel*    m_gradientEditorLabel{nullptr}; // placeholder for future editor
    QComboBox* m_presetCombo{nullptr};
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
