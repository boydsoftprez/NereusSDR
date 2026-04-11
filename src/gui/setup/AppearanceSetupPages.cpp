#include "AppearanceSetupPages.h"
#include "gui/StyleConstants.h"

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

namespace NereusSDR {

namespace {

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
        "QSpinBox { background: #1a2a3a; color: #c8d8e8;"
        "  border: 1px solid #203040; border-radius: 3px; padding: 1px 4px; }"
        "QSpinBox::up-button, QSpinBox::down-button { background: #203040; border: none; }"
        "QCheckBox { color: #c8d8e8; }"
        "QCheckBox::indicator { width: 14px; height: 14px; background: #1a2a3a;"
        "  border: 1px solid #203040; border-radius: 2px; }"
        "QCheckBox::indicator:checked { background: #00b4d8; border-color: #00b4d8; }"
        "QPushButton { background: #1a2a3a; color: #c8d8e8; border: 1px solid #203040;"
        "  border-radius: 3px; padding: 3px 12px; }"
        "QPushButton:hover { background: #203040; }"
        "QPushButton:pressed { background: #00b4d8; color: #0f0f1a; }"
    ));
}

// Build a color swatch placeholder label.
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
// ColorsThemePage
// ---------------------------------------------------------------------------

ColorsThemePage::ColorsThemePage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Colors & Theme"), model, parent)
{
    buildUI();
}

void ColorsThemePage::buildUI()
{
    applyDarkStyle(this);

    // --- Section: Background ---
    auto* bgGroup = new QGroupBox(QStringLiteral("Background"), this);
    auto* bgForm  = new QFormLayout(bgGroup);
    bgForm->setSpacing(6);

    m_bgColorLabel = makeColorSwatch(QStringLiteral("Background Color"), QStringLiteral("#0f0f1a"), bgGroup);
    bgForm->addRow(QStringLiteral("Background:"), m_bgColorLabel);

    m_textColorLabel = makeColorSwatch(QStringLiteral("Text Color"), QStringLiteral("#405060"), bgGroup);
    bgForm->addRow(QStringLiteral("Text:"), m_textColorLabel);

    contentLayout()->addWidget(bgGroup);

    // --- Section: Spectrum ---
    auto* specGroup = new QGroupBox(QStringLiteral("Spectrum"), this);
    auto* specForm  = new QFormLayout(specGroup);
    specForm->setSpacing(6);

    m_gridColorLabel = makeColorSwatch(QStringLiteral("Grid Color"), QStringLiteral("#203040"), specGroup);
    specForm->addRow(QStringLiteral("Grid:"), m_gridColorLabel);

    m_traceColorLabel = makeColorSwatch(QStringLiteral("Trace Color"), QStringLiteral("#1a4a6a"), specGroup);
    specForm->addRow(QStringLiteral("Trace:"), m_traceColorLabel);

    m_filterColorLabel = makeColorSwatch(QStringLiteral("Filter Color"), QStringLiteral("#003060"), specGroup);
    specForm->addRow(QStringLiteral("Filter:"), m_filterColorLabel);

    contentLayout()->addWidget(specGroup);

    // --- Section: Waterfall ---
    auto* wfGroup = new QGroupBox(QStringLiteral("Waterfall"), this);
    auto* wfForm  = new QFormLayout(wfGroup);
    wfForm->setSpacing(6);

    m_wfLowColorLabel = makeColorSwatch(QStringLiteral("Low Level Color"), QStringLiteral("#000030"), wfGroup);
    wfForm->addRow(QStringLiteral("Low Level:"), m_wfLowColorLabel);

    m_wfHighColorLabel = makeColorSwatch(QStringLiteral("High Level Color"), QStringLiteral("#ff8000"), wfGroup);
    wfForm->addRow(QStringLiteral("High Level:"), m_wfHighColorLabel);

    contentLayout()->addWidget(wfGroup);
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
    applyDarkStyle(this);

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
    applyDarkStyle(this);

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
    applyDarkStyle(this);

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
    applyDarkStyle(this);

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
