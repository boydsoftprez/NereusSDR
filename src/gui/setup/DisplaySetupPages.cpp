#include "DisplaySetupPages.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Helpers (file-local)
// ---------------------------------------------------------------------------

namespace {

// Apply the project-wide dark-theme stylesheet to a widget.
void applyDarkStyle(QWidget* w)
{
    w->setStyleSheet(QStringLiteral(
        "QGroupBox { color: #8090a0; font-size: 11px;"
        "  border: 1px solid #203040; border-radius: 4px;"
        "  margin-top: 8px; padding-top: 4px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QLabel { color: #c8d8e8; }"
        "QComboBox { background: #1a2a3a; color: #c8d8e8; border: 1px solid #203040;"
        "  border-radius: 3px; padding: 2px 6px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #1a2a3a; color: #c8d8e8;"
        "  selection-background-color: #00b4d8; }"
        "QSlider::groove:horizontal { background: #1a2a3a; height: 4px; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -4px 0;"
        "  border-radius: 6px; }"
        "QSlider::sub-page:horizontal { background: #00b4d8; border-radius: 2px; }"
        "QSpinBox, QDoubleSpinBox { background: #1a2a3a; color: #c8d8e8;"
        "  border: 1px solid #203040; border-radius: 3px; padding: 1px 4px; }"
        "QSpinBox::up-button, QSpinBox::down-button,"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button"
        "  { background: #203040; border: none; }"
        "QCheckBox { color: #c8d8e8; }"
        "QCheckBox::indicator { width: 14px; height: 14px; background: #1a2a3a;"
        "  border: 1px solid #203040; border-radius: 2px; }"
        "QCheckBox::indicator:checked { background: #00b4d8; border-color: #00b4d8; }"
    ));
}

// Build a color swatch placeholder label (NYI — no color picker yet).
QLabel* makeColorSwatch(const QString& label, const QString& hexColor, QWidget* parent)
{
    auto* lbl = new QLabel(QStringLiteral("  %1  ").arg(label), parent);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: #c8d8e8; border: 1px solid #203040;"
        " border-radius: 3px; padding: 2px 6px; }").arg(hexColor));
    lbl->setFixedHeight(24);
    lbl->setEnabled(false);  // NYI
    lbl->setToolTip(QStringLiteral("Color picker — not yet implemented"));
    return lbl;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// SpectrumDefaultsPage
// ---------------------------------------------------------------------------

SpectrumDefaultsPage::SpectrumDefaultsPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Spectrum Defaults"), model, parent)
{
    buildUI();
}

void SpectrumDefaultsPage::buildUI()
{
    applyDarkStyle(this);

    // --- Section: FFT ---
    auto* fftGroup = new QGroupBox(QStringLiteral("FFT"), this);
    auto* fftForm  = new QFormLayout(fftGroup);
    fftForm->setSpacing(6);

    m_fftSizeCombo = new QComboBox(fftGroup);
    m_fftSizeCombo->addItems({QStringLiteral("1024"), QStringLiteral("2048"),
                              QStringLiteral("4096"), QStringLiteral("8192"),
                              QStringLiteral("16384")});
    m_fftSizeCombo->setCurrentText(QStringLiteral("4096"));
    m_fftSizeCombo->setEnabled(false);  // NYI
    m_fftSizeCombo->setToolTip(QStringLiteral("Default FFT size — not yet implemented"));
    fftForm->addRow(QStringLiteral("FFT Size:"), m_fftSizeCombo);

    m_windowCombo = new QComboBox(fftGroup);
    m_windowCombo->addItems({QStringLiteral("Blackman-Harris"), QStringLiteral("Hann"),
                             QStringLiteral("Hamming"),         QStringLiteral("Flat-Top")});
    m_windowCombo->setEnabled(false);  // NYI
    m_windowCombo->setToolTip(QStringLiteral("FFT window function — not yet implemented"));
    fftForm->addRow(QStringLiteral("Window:"), m_windowCombo);

    contentLayout()->addWidget(fftGroup);

    // --- Section: Rendering ---
    auto* renderGroup = new QGroupBox(QStringLiteral("Rendering"), this);
    auto* renderForm  = new QFormLayout(renderGroup);
    renderForm->setSpacing(6);

    m_fpSlider = new QSlider(Qt::Horizontal, renderGroup);
    m_fpSlider->setRange(10, 60);
    m_fpSlider->setValue(30);
    m_fpSlider->setEnabled(false);  // NYI
    m_fpSlider->setToolTip(QStringLiteral("Spectrum update rate (FPS) — not yet implemented"));
    renderForm->addRow(QStringLiteral("FPS (10–60):"), m_fpSlider);

    m_averagingCombo = new QComboBox(renderGroup);
    m_averagingCombo->addItems({QStringLiteral("None"), QStringLiteral("Weighted"),
                                QStringLiteral("Logarithmic")});
    m_averagingCombo->setEnabled(false);  // NYI
    m_averagingCombo->setToolTip(QStringLiteral("Spectrum averaging mode — not yet implemented"));
    renderForm->addRow(QStringLiteral("Averaging:"), m_averagingCombo);

    m_fillToggle = new QCheckBox(QStringLiteral("Fill under trace"), renderGroup);
    m_fillToggle->setEnabled(false);  // NYI
    m_fillToggle->setToolTip(QStringLiteral("Fill spectrum trace area — not yet implemented"));
    renderForm->addRow(QString(), m_fillToggle);

    m_fillAlphaSlider = new QSlider(Qt::Horizontal, renderGroup);
    m_fillAlphaSlider->setRange(0, 100);
    m_fillAlphaSlider->setValue(30);
    m_fillAlphaSlider->setEnabled(false);  // NYI
    m_fillAlphaSlider->setToolTip(QStringLiteral("Fill opacity (0–100) — not yet implemented"));
    renderForm->addRow(QStringLiteral("Fill Alpha:"), m_fillAlphaSlider);

    m_lineWidthSlider = new QSlider(Qt::Horizontal, renderGroup);
    m_lineWidthSlider->setRange(1, 3);
    m_lineWidthSlider->setValue(1);
    m_lineWidthSlider->setEnabled(false);  // NYI
    m_lineWidthSlider->setToolTip(QStringLiteral("Trace line width (1–3 px) — not yet implemented"));
    renderForm->addRow(QStringLiteral("Line Width:"), m_lineWidthSlider);

    contentLayout()->addWidget(renderGroup);

    // --- Section: Calibration ---
    auto* calGroup = new QGroupBox(QStringLiteral("Calibration"), this);
    auto* calForm  = new QFormLayout(calGroup);
    calForm->setSpacing(6);

    m_calOffsetSpin = new QDoubleSpinBox(calGroup);
    m_calOffsetSpin->setRange(-30.0, 30.0);
    m_calOffsetSpin->setSingleStep(0.5);
    m_calOffsetSpin->setSuffix(QStringLiteral(" dBm"));
    m_calOffsetSpin->setValue(0.0);
    m_calOffsetSpin->setEnabled(false);  // NYI
    m_calOffsetSpin->setToolTip(QStringLiteral("Display calibration offset — not yet implemented"));
    calForm->addRow(QStringLiteral("Cal Offset:"), m_calOffsetSpin);

    m_peakHoldToggle = new QCheckBox(QStringLiteral("Peak hold"), calGroup);
    m_peakHoldToggle->setEnabled(false);  // NYI
    m_peakHoldToggle->setToolTip(QStringLiteral("Hold spectrum peaks — not yet implemented"));
    calForm->addRow(QString(), m_peakHoldToggle);

    m_peakHoldDelaySpin = new QSpinBox(calGroup);
    m_peakHoldDelaySpin->setRange(100, 10000);
    m_peakHoldDelaySpin->setSingleStep(100);
    m_peakHoldDelaySpin->setSuffix(QStringLiteral(" ms"));
    m_peakHoldDelaySpin->setValue(2000);
    m_peakHoldDelaySpin->setEnabled(false);  // NYI
    m_peakHoldDelaySpin->setToolTip(QStringLiteral("Peak hold decay delay — not yet implemented"));
    calForm->addRow(QStringLiteral("Peak Delay:"), m_peakHoldDelaySpin);

    contentLayout()->addWidget(calGroup);
    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// WaterfallDefaultsPage
// ---------------------------------------------------------------------------

WaterfallDefaultsPage::WaterfallDefaultsPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Waterfall Defaults"), model, parent)
{
    buildUI();
}

void WaterfallDefaultsPage::buildUI()
{
    applyDarkStyle(this);

    // --- Section: Levels ---
    auto* levGroup = new QGroupBox(QStringLiteral("Levels"), this);
    auto* levForm  = new QFormLayout(levGroup);
    levForm->setSpacing(6);

    m_highThresholdSlider = new QSlider(Qt::Horizontal, levGroup);
    m_highThresholdSlider->setRange(-200, 0);
    m_highThresholdSlider->setValue(-40);
    m_highThresholdSlider->setEnabled(false);  // NYI
    m_highThresholdSlider->setToolTip(QStringLiteral("Waterfall high threshold — not yet implemented"));
    levForm->addRow(QStringLiteral("High Threshold:"), m_highThresholdSlider);

    m_lowThresholdSlider = new QSlider(Qt::Horizontal, levGroup);
    m_lowThresholdSlider->setRange(-200, 0);
    m_lowThresholdSlider->setValue(-130);
    m_lowThresholdSlider->setEnabled(false);  // NYI
    m_lowThresholdSlider->setToolTip(QStringLiteral("Waterfall low threshold — not yet implemented"));
    levForm->addRow(QStringLiteral("Low Threshold:"), m_lowThresholdSlider);

    m_agcToggle = new QCheckBox(QStringLiteral("AGC"), levGroup);
    m_agcToggle->setEnabled(false);  // NYI
    m_agcToggle->setToolTip(QStringLiteral("Waterfall AGC — not yet implemented"));
    levForm->addRow(QString(), m_agcToggle);

    contentLayout()->addWidget(levGroup);

    // --- Section: Display ---
    auto* dispGroup = new QGroupBox(QStringLiteral("Display"), this);
    auto* dispForm  = new QFormLayout(dispGroup);
    dispForm->setSpacing(6);

    m_updatePeriodSlider = new QSlider(Qt::Horizontal, dispGroup);
    m_updatePeriodSlider->setRange(10, 500);
    m_updatePeriodSlider->setValue(50);
    m_updatePeriodSlider->setEnabled(false);  // NYI
    m_updatePeriodSlider->setToolTip(QStringLiteral("Waterfall update period (ms) — not yet implemented"));
    dispForm->addRow(QStringLiteral("Update Period (ms):"), m_updatePeriodSlider);

    m_reverseToggle = new QCheckBox(QStringLiteral("Reverse scroll"), dispGroup);
    m_reverseToggle->setEnabled(false);  // NYI
    m_reverseToggle->setToolTip(QStringLiteral("Reverse waterfall scroll direction — not yet implemented"));
    dispForm->addRow(QString(), m_reverseToggle);

    m_opacitySlider = new QSlider(Qt::Horizontal, dispGroup);
    m_opacitySlider->setRange(0, 100);
    m_opacitySlider->setValue(100);
    m_opacitySlider->setEnabled(false);  // NYI
    m_opacitySlider->setToolTip(QStringLiteral("Waterfall opacity — not yet implemented"));
    dispForm->addRow(QStringLiteral("Opacity:"), m_opacitySlider);

    m_colorSchemeCombo = new QComboBox(dispGroup);
    m_colorSchemeCombo->addItems({QStringLiteral("Enhanced"), QStringLiteral("Grayscale"),
                                  QStringLiteral("Spectrum")});
    m_colorSchemeCombo->setEnabled(false);  // NYI
    m_colorSchemeCombo->setToolTip(QStringLiteral("Waterfall color scheme — not yet implemented"));
    dispForm->addRow(QStringLiteral("Color Scheme:"), m_colorSchemeCombo);

    contentLayout()->addWidget(dispGroup);

    // --- Section: Time ---
    auto* timeGroup = new QGroupBox(QStringLiteral("Time"), this);
    auto* timeForm  = new QFormLayout(timeGroup);
    timeForm->setSpacing(6);

    m_timestampPosCombo = new QComboBox(timeGroup);
    m_timestampPosCombo->addItems({QStringLiteral("None"), QStringLiteral("Left"),
                                   QStringLiteral("Right")});
    m_timestampPosCombo->setEnabled(false);  // NYI
    m_timestampPosCombo->setToolTip(QStringLiteral("Waterfall timestamp position — not yet implemented"));
    timeForm->addRow(QStringLiteral("Timestamp Position:"), m_timestampPosCombo);

    m_timestampModeCombo = new QComboBox(timeGroup);
    m_timestampModeCombo->addItems({QStringLiteral("UTC"), QStringLiteral("Local")});
    m_timestampModeCombo->setEnabled(false);  // NYI
    m_timestampModeCombo->setToolTip(QStringLiteral("Timestamp time zone — not yet implemented"));
    timeForm->addRow(QStringLiteral("Timestamp Mode:"), m_timestampModeCombo);

    contentLayout()->addWidget(timeGroup);
    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// GridScalesPage
// ---------------------------------------------------------------------------

GridScalesPage::GridScalesPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Grid & Scales"), model, parent)
{
    buildUI();
}

void GridScalesPage::buildUI()
{
    applyDarkStyle(this);

    // --- Section: Grid ---
    auto* gridGroup = new QGroupBox(QStringLiteral("Grid"), this);
    auto* gridForm  = new QFormLayout(gridGroup);
    gridForm->setSpacing(6);

    m_gridToggle = new QCheckBox(QStringLiteral("Show grid"), gridGroup);
    m_gridToggle->setChecked(true);
    m_gridToggle->setEnabled(false);  // NYI
    m_gridToggle->setToolTip(QStringLiteral("Show spectrum grid lines — not yet implemented"));
    gridForm->addRow(QString(), m_gridToggle);

    m_dbMaxSpin = new QSpinBox(gridGroup);
    m_dbMaxSpin->setRange(-200, 0);
    m_dbMaxSpin->setValue(-20);
    m_dbMaxSpin->setSuffix(QStringLiteral(" dB"));
    m_dbMaxSpin->setEnabled(false);  // NYI
    m_dbMaxSpin->setToolTip(QStringLiteral("Grid dB maximum — not yet implemented"));
    gridForm->addRow(QStringLiteral("dB Max:"), m_dbMaxSpin);

    m_dbMinSpin = new QSpinBox(gridGroup);
    m_dbMinSpin->setRange(-200, 0);
    m_dbMinSpin->setValue(-160);
    m_dbMinSpin->setSuffix(QStringLiteral(" dB"));
    m_dbMinSpin->setEnabled(false);  // NYI
    m_dbMinSpin->setToolTip(QStringLiteral("Grid dB minimum — not yet implemented"));
    gridForm->addRow(QStringLiteral("dB Min:"), m_dbMinSpin);

    m_dbStepSpin = new QSpinBox(gridGroup);
    m_dbStepSpin->setRange(1, 40);
    m_dbStepSpin->setValue(10);
    m_dbStepSpin->setSuffix(QStringLiteral(" dB"));
    m_dbStepSpin->setEnabled(false);  // NYI
    m_dbStepSpin->setToolTip(QStringLiteral("Grid dB step size — not yet implemented"));
    gridForm->addRow(QStringLiteral("dB Step:"), m_dbStepSpin);

    contentLayout()->addWidget(gridGroup);

    // --- Section: Labels ---
    auto* lblGroup = new QGroupBox(QStringLiteral("Labels"), this);
    auto* lblForm  = new QFormLayout(lblGroup);
    lblForm->setSpacing(6);

    m_freqLabelAlignCombo = new QComboBox(lblGroup);
    m_freqLabelAlignCombo->addItems({QStringLiteral("Left"), QStringLiteral("Center")});
    m_freqLabelAlignCombo->setEnabled(false);  // NYI
    m_freqLabelAlignCombo->setToolTip(QStringLiteral("Frequency label alignment — not yet implemented"));
    lblForm->addRow(QStringLiteral("Freq Label Align:"), m_freqLabelAlignCombo);

    m_bandEdgeColorLabel = makeColorSwatch(QStringLiteral("Band Edge Color"), QStringLiteral("#1a4a1a"), lblGroup);
    lblForm->addRow(QStringLiteral("Band Edge Color:"), m_bandEdgeColorLabel);

    m_zeroLineToggle = new QCheckBox(QStringLiteral("Show zero line"), lblGroup);
    m_zeroLineToggle->setEnabled(false);  // NYI
    m_zeroLineToggle->setToolTip(QStringLiteral("Show zero dB reference line — not yet implemented"));
    lblForm->addRow(QString(), m_zeroLineToggle);

    m_showFpsToggle = new QCheckBox(QStringLiteral("Show FPS overlay"), lblGroup);
    m_showFpsToggle->setEnabled(false);  // NYI
    m_showFpsToggle->setToolTip(QStringLiteral("Show FPS counter overlay — not yet implemented"));
    lblForm->addRow(QString(), m_showFpsToggle);

    contentLayout()->addWidget(lblGroup);
    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// Rx2DisplayPage
// ---------------------------------------------------------------------------

Rx2DisplayPage::Rx2DisplayPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("RX2 Display"), model, parent)
{
    buildUI();
}

void Rx2DisplayPage::buildUI()
{
    applyDarkStyle(this);

    // --- Section: RX2 Spectrum ---
    auto* specGroup = new QGroupBox(QStringLiteral("RX2 Spectrum"), this);
    auto* specForm  = new QFormLayout(specGroup);
    specForm->setSpacing(6);

    m_dbMaxSpin = new QSpinBox(specGroup);
    m_dbMaxSpin->setRange(-200, 0);
    m_dbMaxSpin->setValue(-20);
    m_dbMaxSpin->setSuffix(QStringLiteral(" dB"));
    m_dbMaxSpin->setEnabled(false);  // NYI
    m_dbMaxSpin->setToolTip(QStringLiteral("RX2 spectrum dB max — not yet implemented"));
    specForm->addRow(QStringLiteral("dB Max:"), m_dbMaxSpin);

    m_dbMinSpin = new QSpinBox(specGroup);
    m_dbMinSpin->setRange(-200, 0);
    m_dbMinSpin->setValue(-160);
    m_dbMinSpin->setSuffix(QStringLiteral(" dB"));
    m_dbMinSpin->setEnabled(false);  // NYI
    m_dbMinSpin->setToolTip(QStringLiteral("RX2 spectrum dB min — not yet implemented"));
    specForm->addRow(QStringLiteral("dB Min:"), m_dbMinSpin);

    m_colorSchemeCombo = new QComboBox(specGroup);
    m_colorSchemeCombo->addItems({QStringLiteral("Enhanced"), QStringLiteral("Grayscale"),
                                  QStringLiteral("Spectrum")});
    m_colorSchemeCombo->setEnabled(false);  // NYI
    m_colorSchemeCombo->setToolTip(QStringLiteral("RX2 waterfall color scheme — not yet implemented"));
    specForm->addRow(QStringLiteral("Color Scheme:"), m_colorSchemeCombo);

    contentLayout()->addWidget(specGroup);

    // --- Section: RX2 Waterfall ---
    auto* wfGroup = new QGroupBox(QStringLiteral("RX2 Waterfall"), this);
    auto* wfForm  = new QFormLayout(wfGroup);
    wfForm->setSpacing(6);

    m_highThresholdSlider = new QSlider(Qt::Horizontal, wfGroup);
    m_highThresholdSlider->setRange(-200, 0);
    m_highThresholdSlider->setValue(-40);
    m_highThresholdSlider->setEnabled(false);  // NYI
    m_highThresholdSlider->setToolTip(QStringLiteral("RX2 waterfall high threshold — not yet implemented"));
    wfForm->addRow(QStringLiteral("High Threshold:"), m_highThresholdSlider);

    m_lowThresholdSlider = new QSlider(Qt::Horizontal, wfGroup);
    m_lowThresholdSlider->setRange(-200, 0);
    m_lowThresholdSlider->setValue(-130);
    m_lowThresholdSlider->setEnabled(false);  // NYI
    m_lowThresholdSlider->setToolTip(QStringLiteral("RX2 waterfall low threshold — not yet implemented"));
    wfForm->addRow(QStringLiteral("Low Threshold:"), m_lowThresholdSlider);

    contentLayout()->addWidget(wfGroup);
    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// TxDisplayPage
// ---------------------------------------------------------------------------

TxDisplayPage::TxDisplayPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("TX Display"), model, parent)
{
    buildUI();
}

void TxDisplayPage::buildUI()
{
    applyDarkStyle(this);

    // --- Section: TX Spectrum ---
    auto* specGroup = new QGroupBox(QStringLiteral("TX Spectrum"), this);
    auto* specForm  = new QFormLayout(specGroup);
    specForm->setSpacing(6);

    m_bgColorLabel = makeColorSwatch(QStringLiteral("Background Color"), QStringLiteral("#0a0a14"), specGroup);
    specForm->addRow(QStringLiteral("Background:"), m_bgColorLabel);

    m_gridColorLabel = makeColorSwatch(QStringLiteral("Grid Color"), QStringLiteral("#203040"), specGroup);
    specForm->addRow(QStringLiteral("Grid Color:"), m_gridColorLabel);

    m_lineWidthSlider = new QSlider(Qt::Horizontal, specGroup);
    m_lineWidthSlider->setRange(1, 3);
    m_lineWidthSlider->setValue(1);
    m_lineWidthSlider->setEnabled(false);  // NYI
    m_lineWidthSlider->setToolTip(QStringLiteral("TX trace line width (1–3 px) — not yet implemented"));
    specForm->addRow(QStringLiteral("Line Width:"), m_lineWidthSlider);

    m_calOffsetSpin = new QDoubleSpinBox(specGroup);
    m_calOffsetSpin->setRange(-30.0, 30.0);
    m_calOffsetSpin->setSingleStep(0.5);
    m_calOffsetSpin->setSuffix(QStringLiteral(" dBm"));
    m_calOffsetSpin->setValue(0.0);
    m_calOffsetSpin->setEnabled(false);  // NYI
    m_calOffsetSpin->setToolTip(QStringLiteral("TX calibration offset — not yet implemented"));
    specForm->addRow(QStringLiteral("Cal Offset:"), m_calOffsetSpin);

    contentLayout()->addWidget(specGroup);
    contentLayout()->addStretch();
}

} // namespace NereusSDR
