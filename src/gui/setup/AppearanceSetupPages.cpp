#include "AppearanceSetupPages.h"
#include "gui/ColorSwatchButton.h"
#include "gui/SpectrumWidget.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMessageBox>

#include <functional>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// ColorsThemePage
// ---------------------------------------------------------------------------

ColorsThemePage::ColorsThemePage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Colors & Theme"), model, parent)
{
    buildUI();
}

void ColorsThemePage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    SpectrumWidget* sw = model() ? model()->spectrumWidget() : nullptr;

    // Helper: build a functional ColorSwatchButton wired to a SpectrumWidget setter.
    auto makeBtn = [this, sw](QWidget* parent, const QColor& defaultColor,
                              std::function<QColor(SpectrumWidget*)> getter,
                              void (SpectrumWidget::*setter)(const QColor&)) {
        const QColor initial = (sw && getter) ? getter(sw) : defaultColor;
        auto* btn = new ColorSwatchButton(initial, parent);
        connect(btn, &ColorSwatchButton::colorChanged, this,
                [this, setter](const QColor& c) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                (w->*setter)(c);
            }
        });
        return btn;
    };

    // --- Section: Spectrum ---
    auto* specGroup = new QGroupBox(QStringLiteral("Spectrum"), this);
    auto* specForm  = new QFormLayout(specGroup);
    specForm->setSpacing(6);

    // Trace & Fill Color — moved from Display → Spectrum Defaults "Trace Colors" group.
    m_traceFillColorBtn = makeBtn(specGroup,
        QColor(0x00, 0xe5, 0xff),
        [](SpectrumWidget* w){ return w->fillColor(); },
        &SpectrumWidget::setFillColor);
    // Thetis: setup.designer.cs:3234 (clrbtnDataLine) / :3217 (clrbtnDataFill) — collapsed into
    // one picker because SpectrumWidget currently shares one colour for line and fill.
    m_traceFillColorBtn->setToolTip(QStringLiteral(
        "Click to choose the spectrum trace line and fill colour. "
        "QColorDialog lets you adjust alpha for the fill opacity."));
    specForm->addRow(QStringLiteral("Trace & Fill Color:"), m_traceFillColorBtn);

    // Grid Color — moved from Display → Grid & Scales "Colors" group (G9).
    m_gridColorBtn = makeBtn(specGroup,
        QColor(255, 255, 255, 40),
        [](SpectrumWidget* w){ return w->gridColor(); },
        &SpectrumWidget::setGridColor);
    // Thetis: setup.designer.cs:3202 (clrbtnGrid) — rewritten
    // Thetis original: (none)
    m_gridColorBtn->setToolTip(QStringLiteral("Colour of the major vertical grid lines on the panadapter."));
    specForm->addRow(QStringLiteral("Grid Color:"), m_gridColorBtn);

    // Grid Fine Color — moved from Display → Grid & Scales "Colors" group (G10).
    m_gridFineColorBtn = makeBtn(specGroup,
        QColor(255, 255, 255, 20),
        [](SpectrumWidget* w){ return w->gridFineColor(); },
        &SpectrumWidget::setGridFineColor);
    // Thetis: setup.designer.cs:3198 (clrbtnGridFine) — rewritten
    // Thetis original: (none)
    m_gridFineColorBtn->setToolTip(QStringLiteral("Colour of the minor (fine) grid lines between major grid lines on the panadapter."));
    specForm->addRow(QStringLiteral("Grid Fine Color:"), m_gridFineColorBtn);

    // H-Grid Color — moved from Display → Grid & Scales "Colors" group (G11).
    m_hGridColorBtn = makeBtn(specGroup,
        QColor(255, 255, 255, 40),
        [](SpectrumWidget* w){ return w->hGridColor(); },
        &SpectrumWidget::setHGridColor);
    // Thetis: setup.designer.cs:3193 (clrbtnHGridColor) — rewritten
    // Thetis original: (none)
    m_hGridColorBtn->setToolTip(QStringLiteral("Colour of the horizontal dB grid lines on the panadapter."));
    specForm->addRow(QStringLiteral("H-Grid Color:"), m_hGridColorBtn);

    // Grid Text Color — moved from Display → Grid & Scales "Colors" group (G12).
    m_gridTextColorBtn = makeBtn(specGroup,
        QColor(255, 255, 0),
        [](SpectrumWidget* w){ return w->gridTextColor(); },
        &SpectrumWidget::setGridTextColor);
    // Thetis: setup.designer.cs:3206 (clrbtnText) — rewritten
    // Thetis original: (none)
    m_gridTextColorBtn->setToolTip(QStringLiteral("Colour of the frequency and dB labels drawn on the panadapter grid."));
    specForm->addRow(QStringLiteral("Grid Text Color:"), m_gridTextColorBtn);

    // Band Edge Color — moved from Display → Grid & Scales "Colors" group (G6).
    m_bandEdgeColorBtn = makeBtn(specGroup,
        QColor(255, 0, 0),
        [](SpectrumWidget* w){ return w->bandEdgeColor(); },
        &SpectrumWidget::setBandEdgeColor);
    // Thetis: setup.designer.cs:3232 (clrbtnBandEdge) — rewritten
    // Thetis original: (none)
    m_bandEdgeColorBtn->setToolTip(QStringLiteral("Colour of the band edge markers drawn at the amateur band boundaries on the panadapter."));
    specForm->addRow(QStringLiteral("Band Edge Color:"), m_bandEdgeColorBtn);

    // RX Zero Line Color — moved from Display → Grid & Scales "Colors" group (G13).
    m_rxZeroLineColorBtn = makeBtn(specGroup,
        QColor(255, 0, 0),
        [](SpectrumWidget* w){ return w->rxZeroLineColor(); },
        &SpectrumWidget::setRxZeroLineColor);
    // Thetis: setup.designer.cs:3204 (clrbtnZeroLine) — rewritten; split per Plan 4 D9c-1
    // Thetis original: (none)
    m_rxZeroLineColorBtn->setToolTip(QStringLiteral(
        "Colour of the RX zero line (0 dBm marker) drawn on the panadapter "
        "when Show zero line is checked."));
    specForm->addRow(QStringLiteral("RX Zero Line Color:"), m_rxZeroLineColorBtn);

    // TX Zero Line Color — moved from Display → Grid & Scales "Colors" group (G13 TX).
    m_txZeroLineColorBtn = makeBtn(specGroup,
        QColor(255, 184, 0),
        [](SpectrumWidget* w){ return w->txZeroLineColor(); },
        &SpectrumWidget::setTxZeroLineColor);
    // NereusSDR Plan 4 D9c-1 — no Thetis equivalent (NereusSDR-original).
    m_txZeroLineColorBtn->setToolTip(QStringLiteral(
        "Colour of the TX zero line drawn on the panadapter and waterfall "
        "at the TX centre frequency when transmitting (MOX active)."));
    specForm->addRow(QStringLiteral("TX Zero Line Color:"), m_txZeroLineColorBtn);

    // RX Passband Color — Plan 4 D9b, already lived here.
    m_rxFilterColorBtn = makeBtn(specGroup,
        QColor(0x00, 0xb4, 0xd8, 80),
        [](SpectrumWidget* w){ return w->rxFilterColor(); },
        &SpectrumWidget::setRxFilterColor);
    m_rxFilterColorBtn->setToolTip(QStringLiteral(
        "Click to choose the RX passband overlay colour and opacity. "
        "Shown on the panadapter and waterfall slice band during receive."));
    specForm->addRow(QStringLiteral("RX Passband Color:"), m_rxFilterColorBtn);

    // TX Passband Color — Plan 4 D9b, already lived here.
    m_txFilterColorBtn = makeBtn(specGroup,
        QColor(255, 120, 60, 46),
        [](SpectrumWidget* w){ return w->txFilterColor(); },
        &SpectrumWidget::setTxFilterColor);
    m_txFilterColorBtn->setToolTip(QStringLiteral(
        "Click to choose the TX passband overlay colour and opacity. "
        "Shown on the panadapter and waterfall during MOX/TUNE."));
    specForm->addRow(QStringLiteral("TX Passband Color:"), m_txFilterColorBtn);

    contentLayout()->addWidget(specGroup);

    // --- Section: Waterfall ---
    auto* wfGroup = new QGroupBox(QStringLiteral("Waterfall"), this);
    auto* wfForm  = new QFormLayout(wfGroup);
    wfForm->setSpacing(6);

    // Low Level Color — moved from Display → Waterfall Defaults "Levels" group (W10).
    // SpectrumWidget uses gradient tables; the custom colour is persisted for future
    // Custom-scheme wiring but does not rebuild the gradient live yet.
    m_wfLowColorBtn = new ColorSwatchButton(QColor(Qt::black), wfGroup);
    // Thetis: setup.designer.cs:34176 (clrbtnWaterfallLow)
    m_wfLowColorBtn->setToolTip(QStringLiteral("The Color to use when the signal level is at or below the low level set above."));
    connect(m_wfLowColorBtn, &ColorSwatchButton::colorChanged,
            this, [](const QColor&) { /* stored via AppSettings on save */ });
    wfForm->addRow(QStringLiteral("Low Level Color:"), m_wfLowColorBtn);

    contentLayout()->addWidget(wfGroup);

    // Reset all colors to defaults — broadened from the Plan 4 D9c-3 button
    // that lived on Spectrum Defaults to cover all Colors & Theme entries.
    auto* resetRow = new QHBoxLayout;
    resetRow->addStretch(1);
    auto* resetBtn = new QPushButton(QStringLiteral("Reset all colors to defaults"), this);
    resetBtn->setToolTip(QStringLiteral(
        "Reset all spectrum and waterfall colours to factory defaults. "
        "Other display settings (FPS, averaging, thresholds, etc.) are not affected."));
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        const auto res = QMessageBox::question(
            this,
            QStringLiteral("Reset all colors to defaults"),
            QStringLiteral(
                "Reset all spectrum and waterfall colours to factory defaults?\n\n"
                "Custom colours set here will be discarded. "
                "Other display settings are not affected."),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (res != QMessageBox::Yes) { return; }
        auto* w = model() ? model()->spectrumWidget() : nullptr;
        if (!w) { return; }
        w->resetDisplayColorsToDefaults();
        // Reload all swatches from the model so the buttons reflect the reset values.
        m_traceFillColorBtn->setColor(w->fillColor());
        m_gridColorBtn->setColor(w->gridColor());
        m_gridFineColorBtn->setColor(w->gridFineColor());
        m_hGridColorBtn->setColor(w->hGridColor());
        m_gridTextColorBtn->setColor(w->gridTextColor());
        m_bandEdgeColorBtn->setColor(w->bandEdgeColor());
        m_rxZeroLineColorBtn->setColor(w->rxZeroLineColor());
        m_txZeroLineColorBtn->setColor(w->txZeroLineColor());
        m_rxFilterColorBtn->setColor(w->rxFilterColor());
        m_txFilterColorBtn->setColor(w->txFilterColor());
        m_wfLowColorBtn->setColor(QColor(Qt::black));   // waterfall low default
    });
    resetRow->addWidget(resetBtn);
    resetRow->addStretch(1);
    contentLayout()->addLayout(resetRow);

    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// MeterStylesPage
// ---------------------------------------------------------------------------

MeterStylesPage::MeterStylesPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Meter Styles"), model, parent)
{
    buildUI();
}

void MeterStylesPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    // --- Section: S-Meter ---
    auto* smGroup = new QGroupBox(QStringLiteral("S-Meter"), this);
    auto* smForm  = new QFormLayout(smGroup);
    smForm->setSpacing(6);

    m_typeCombo = new QComboBox(smGroup);
    m_typeCombo->addItems({QStringLiteral("Arc"), QStringLiteral("Bar"),
                           QStringLiteral("Digital")});
    m_typeCombo->setEnabled(false);  // NYI
    m_typeCombo->setToolTip(QStringLiteral("S-Meter display type — not yet implemented"));
    smForm->addRow(QStringLiteral("Type:"), m_typeCombo);

    m_peakHoldToggle = new QCheckBox(QStringLiteral("Peak hold"), smGroup);
    m_peakHoldToggle->setEnabled(false);  // NYI
    m_peakHoldToggle->setToolTip(QStringLiteral("S-Meter peak hold — not yet implemented"));
    smForm->addRow(QString(), m_peakHoldToggle);

    m_decayRateSlider = new QSlider(Qt::Horizontal, smGroup);
    m_decayRateSlider->setRange(1, 100);
    m_decayRateSlider->setValue(20);
    m_decayRateSlider->setEnabled(false);  // NYI
    m_decayRateSlider->setToolTip(QStringLiteral("S-Meter peak decay rate — not yet implemented"));
    smForm->addRow(QStringLiteral("Decay Rate:"), m_decayRateSlider);

    contentLayout()->addWidget(smGroup);
    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// GradientsPage
// ---------------------------------------------------------------------------

GradientsPage::GradientsPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Gradients"), model, parent)
{
    buildUI();
}

void GradientsPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    // --- Section: Waterfall Gradient ---
    auto* gradGroup = new QGroupBox(QStringLiteral("Waterfall Gradient"), this);
    auto* gradForm  = new QFormLayout(gradGroup);
    gradForm->setSpacing(6);

    m_gradientEditorLabel = new QLabel(
        QStringLiteral("(Gradient editor — not yet implemented)"), gradGroup);
    m_gradientEditorLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-style: italic;"
        " background: #1a2a3a; border: 1px solid #203040;"
        " border-radius: 3px; padding: 8px; }"));
    m_gradientEditorLabel->setMinimumHeight(60);
    m_gradientEditorLabel->setEnabled(false);
    m_gradientEditorLabel->setAlignment(Qt::AlignCenter);
    gradForm->addRow(QStringLiteral("Editor:"), m_gradientEditorLabel);

    m_presetCombo = new QComboBox(gradGroup);
    m_presetCombo->addItems({QStringLiteral("Enhanced"), QStringLiteral("Grayscale"),
                             QStringLiteral("Spectrum"), QStringLiteral("Fire"),
                             QStringLiteral("Ice")});
    m_presetCombo->setEnabled(false);  // NYI
    m_presetCombo->setToolTip(QStringLiteral("Waterfall gradient preset — not yet implemented"));
    gradForm->addRow(QStringLiteral("Preset:"), m_presetCombo);

    contentLayout()->addWidget(gradGroup);
    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// SkinsPage
// ---------------------------------------------------------------------------

SkinsPage::SkinsPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Skins"), model, parent)
{
    buildUI();
}

void SkinsPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    // --- Section: Skins ---
    auto* skinGroup = new QGroupBox(QStringLiteral("Skins"), this);
    auto* skinLayout = new QVBoxLayout(skinGroup);
    skinLayout->setSpacing(6);

    m_skinListLabel = new QLabel(
        QStringLiteral("(Skin list — not yet implemented. Phase 3H.)"), skinGroup);
    m_skinListLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-style: italic;"
        " background: #1a2a3a; border: 1px solid #203040;"
        " border-radius: 3px; padding: 8px; }"));
    m_skinListLabel->setMinimumHeight(120);
    m_skinListLabel->setEnabled(false);
    m_skinListLabel->setAlignment(Qt::AlignCenter);
    skinLayout->addWidget(m_skinListLabel);

    auto* btnRow = new QHBoxLayout();
    m_loadBtn = new QPushButton(QStringLiteral("Load"), skinGroup);
    m_loadBtn->setEnabled(false);  // NYI
    m_loadBtn->setToolTip(QStringLiteral("Load skin — not yet implemented (Phase 3H)"));
    m_loadBtn->setAutoDefault(false);
    btnRow->addWidget(m_loadBtn);

    m_saveBtn = new QPushButton(QStringLiteral("Save"), skinGroup);
    m_saveBtn->setEnabled(false);  // NYI
    m_saveBtn->setToolTip(QStringLiteral("Save skin — not yet implemented (Phase 3H)"));
    m_saveBtn->setAutoDefault(false);
    btnRow->addWidget(m_saveBtn);

    m_importBtn = new QPushButton(QStringLiteral("Import..."), skinGroup);
    m_importBtn->setEnabled(false);  // NYI
    m_importBtn->setToolTip(QStringLiteral("Import Thetis-format skin — not yet implemented (Phase 3H)"));
    m_importBtn->setAutoDefault(false);
    btnRow->addWidget(m_importBtn);

    btnRow->addStretch();
    skinLayout->addLayout(btnRow);

    contentLayout()->addWidget(skinGroup);
    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// CollapsibleDisplayPage
// ---------------------------------------------------------------------------

CollapsibleDisplayPage::CollapsibleDisplayPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Collapsible Display"), model, parent)
{
    buildUI();
}

void CollapsibleDisplayPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    // --- Section: Collapsible ---
    auto* colGroup = new QGroupBox(QStringLiteral("Collapsible"), this);
    auto* colForm  = new QFormLayout(colGroup);
    colForm->setSpacing(6);

    m_widthSpin = new QSpinBox(colGroup);
    m_widthSpin->setRange(100, 2000);
    m_widthSpin->setValue(400);
    m_widthSpin->setSuffix(QStringLiteral(" px"));
    m_widthSpin->setEnabled(false);  // NYI
    m_widthSpin->setToolTip(QStringLiteral("Collapsible panel width — not yet implemented"));
    colForm->addRow(QStringLiteral("Width:"), m_widthSpin);

    m_heightSpin = new QSpinBox(colGroup);
    m_heightSpin->setRange(50, 1000);
    m_heightSpin->setValue(200);
    m_heightSpin->setSuffix(QStringLiteral(" px"));
    m_heightSpin->setEnabled(false);  // NYI
    m_heightSpin->setToolTip(QStringLiteral("Collapsible panel height — not yet implemented"));
    colForm->addRow(QStringLiteral("Height:"), m_heightSpin);

    m_enableToggle = new QCheckBox(QStringLiteral("Enable collapsible display"), colGroup);
    m_enableToggle->setEnabled(false);  // NYI
    m_enableToggle->setToolTip(QStringLiteral("Enable collapsible spectrum section — not yet implemented"));
    colForm->addRow(QString(), m_enableToggle);

    contentLayout()->addWidget(colGroup);
    contentLayout()->addStretch();
}

} // namespace NereusSDR
