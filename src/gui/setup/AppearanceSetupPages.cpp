#include "AppearanceSetupPages.h"
#include "gui/ColorSwatchButton.h"
#include "gui/SMeterWidget.h"
#include "gui/SpectrumWidget.h"
#include "gui/StyleConstants.h"
#include "gui/UnbuiltFeatures.h"
#include "core/AppSettings.h"
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
#include <QSignalBlocker>

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
    m_traceFillColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.traceFillColor");
    // Thetis: setup.designer.cs:3234 (clrbtnDataLine) / :3217 (clrbtnDataFill) — collapsed into
    // one picker because SpectrumWidget currently shares one colour for line and fill.
    m_traceFillColorBtn->setToolTip(QStringLiteral(
        "Click to choose the spectrum trace line and fill color. "
        "The color picker lets you adjust alpha for the fill opacity."));  // a QColorDialog
    specForm->addRow(QStringLiteral("Trace & Fill Color:"), m_traceFillColorBtn);

    // Grid Color — moved from Display → Grid & Scales "Colors" group (G9).
    m_gridColorBtn = makeBtn(specGroup,
        QColor(255, 255, 255, 40),
        [](SpectrumWidget* w){ return w->gridColor(); },
        &SpectrumWidget::setGridColor);
    m_gridColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.gridColor");
    // Thetis: setup.designer.cs:3202 (clrbtnGrid) — rewritten
    // Thetis original: (none)
    m_gridColorBtn->setToolTip(QStringLiteral("Color of the major vertical grid lines on the panadapter."));
    specForm->addRow(QStringLiteral("Grid Color:"), m_gridColorBtn);

    // Grid Fine Color — moved from Display → Grid & Scales "Colors" group (G10).
    m_gridFineColorBtn = makeBtn(specGroup,
        QColor(255, 255, 255, 20),
        [](SpectrumWidget* w){ return w->gridFineColor(); },
        &SpectrumWidget::setGridFineColor);
    m_gridFineColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.gridFineColor");
    // Thetis: setup.designer.cs:3198 (clrbtnGridFine) — rewritten
    // Thetis original: (none)
    m_gridFineColorBtn->setToolTip(QStringLiteral("Color of the minor (fine) grid lines between major grid lines on the panadapter."));
    specForm->addRow(QStringLiteral("Grid Fine Color:"), m_gridFineColorBtn);

    // H-Grid Color — moved from Display → Grid & Scales "Colors" group (G11).
    m_hGridColorBtn = makeBtn(specGroup,
        QColor(255, 255, 255, 40),
        [](SpectrumWidget* w){ return w->hGridColor(); },
        &SpectrumWidget::setHGridColor);
    m_hGridColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.hGridColor");
    // Thetis: setup.designer.cs:3193 (clrbtnHGridColor) — rewritten
    // Thetis original: (none)
    m_hGridColorBtn->setToolTip(QStringLiteral("Color of the horizontal dB grid lines on the panadapter."));
    specForm->addRow(QStringLiteral("H-Grid Color:"), m_hGridColorBtn);

    // Grid Text Color — moved from Display → Grid & Scales "Colors" group (G12).
    m_gridTextColorBtn = makeBtn(specGroup,
        QColor(255, 255, 0),
        [](SpectrumWidget* w){ return w->gridTextColor(); },
        &SpectrumWidget::setGridTextColor);
    m_gridTextColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.gridTextColor");
    // Thetis: setup.designer.cs:3206 (clrbtnText) — rewritten
    // Thetis original: (none)
    m_gridTextColorBtn->setToolTip(QStringLiteral("Color of the frequency and dB labels drawn on the panadapter grid."));
    specForm->addRow(QStringLiteral("Grid Text Color:"), m_gridTextColorBtn);

    // Band Edge Color — moved from Display → Grid & Scales "Colors" group (G6).
    m_bandEdgeColorBtn = makeBtn(specGroup,
        QColor(255, 0, 0),
        [](SpectrumWidget* w){ return w->bandEdgeColor(); },
        &SpectrumWidget::setBandEdgeColor);
    m_bandEdgeColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.bandEdgeColor");
    // Thetis: setup.designer.cs:3232 (clrbtnBandEdge) — rewritten
    // Thetis original: (none)
    m_bandEdgeColorBtn->setToolTip(QStringLiteral("Color of the band edge markers drawn at the amateur band boundaries on the panadapter."));
    specForm->addRow(QStringLiteral("Band Edge Color:"), m_bandEdgeColorBtn);

    // RX Zero Line Color — moved from Display → Grid & Scales "Colors" group (G13).
    m_rxZeroLineColorBtn = makeBtn(specGroup,
        QColor(255, 0, 0),
        [](SpectrumWidget* w){ return w->rxZeroLineColor(); },
        &SpectrumWidget::setRxZeroLineColor);
    m_rxZeroLineColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.rxZeroLineColor");
    // Thetis: setup.designer.cs:3204 (clrbtnZeroLine) — rewritten; split per Plan 4 D9c-1
    // Thetis original: (none)
    m_rxZeroLineColorBtn->setToolTip(QStringLiteral(
        "Color of the RX zero line (0 dBm marker) drawn on the panadapter "
        "when Show zero line is checked."));
    specForm->addRow(QStringLiteral("RX Zero Line Color:"), m_rxZeroLineColorBtn);

    // TX Zero Line Color — moved from Display → Grid & Scales "Colors" group (G13 TX).
    m_txZeroLineColorBtn = makeBtn(specGroup,
        QColor(255, 184, 0),
        [](SpectrumWidget* w){ return w->txZeroLineColor(); },
        &SpectrumWidget::setTxZeroLineColor);
    m_txZeroLineColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.txZeroLineColor");
    // NereusSDR Plan 4 D9c-1 — no Thetis equivalent (NereusSDR-original).
    m_txZeroLineColorBtn->setToolTip(QStringLiteral(
        "Color of the TX zero line drawn on the panadapter and waterfall "
        "at the TX center frequency when transmitting (MOX active)."));
    specForm->addRow(QStringLiteral("TX Zero Line Color:"), m_txZeroLineColorBtn);

    // RX Passband Color — Plan 4 D9b, already lived here.
    m_rxFilterColorBtn = makeBtn(specGroup,
        QColor(0x00, 0xb4, 0xd8, 80),
        [](SpectrumWidget* w){ return w->rxFilterColor(); },
        &SpectrumWidget::setRxFilterColor);
    m_rxFilterColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.rxFilterColor");
    m_rxFilterColorBtn->setToolTip(QStringLiteral(
        "Click to choose the RX passband overlay color and opacity. "
        "Shown on the panadapter and waterfall slice band during receive."));
    specForm->addRow(QStringLiteral("RX Passband Color:"), m_rxFilterColorBtn);

    // TX Passband Color — Plan 4 D9b, already lived here.
    m_txFilterColorBtn = makeBtn(specGroup,
        QColor(255, 120, 60, 46),
        [](SpectrumWidget* w){ return w->txFilterColor(); },
        &SpectrumWidget::setTxFilterColor);
    m_txFilterColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.txFilterColor");
    m_txFilterColorBtn->setToolTip(QStringLiteral(
        "Click to choose the TX passband overlay color and opacity. "
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
    UnbuiltFeatures::hideRowUnlessBuilt(m_wfLowColorBtn,
                                       UnbuiltFeature::WaterfallLowColor, wfForm);

    contentLayout()->addWidget(wfGroup);

    // Reset all colors to defaults — broadened from the Plan 4 D9c-3 button
    // that lived on Spectrum Defaults to cover all Colors & Theme entries.
    auto* resetRow = new QHBoxLayout;
    resetRow->addStretch(1);
    auto* resetBtn = new QPushButton(QStringLiteral("Reset all colors to defaults"), this);
    resetBtn->setProperty("nereusSetupId", "appearance.colorsTheme.resetColors");
    resetBtn->setToolTip(QStringLiteral(
        "Reset all spectrum and waterfall colors to factory defaults. "
        "Other display settings (FPS, averaging, thresholds, etc.) are not affected."));
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        const auto res = QMessageBox::question(
            this,
            QStringLiteral("Reset all colors to defaults"),
            QStringLiteral(
                "Reset all spectrum and waterfall colors to factory defaults?\n\n"
                "Custom colors set here will be discarded. "
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

void MeterStylesPage::reloadSMeterSettings()
{
    using Face = SMeterWidget::FaceStyle;
    const auto& s = AppSettings::instance();
    if (m_faceCombo) {
        const Face face = SMeterWidget::faceStyleFromKey(
            s.value(QStringLiteral("SMeter_FaceStyle"),
                    SMeterWidget::faceStyleKey(Face::AgedCream)).toString());
        QSignalBlocker block(m_faceCombo);
        m_faceCombo->setCurrentIndex(m_faceCombo->findData(static_cast<int>(face)));
    }
    if (m_peakHoldToggle) {
        QSignalBlocker block(m_peakHoldToggle);
        m_peakHoldToggle->setChecked(
            s.value(QStringLiteral("PeakHoldEnabled"), QStringLiteral("True")).toString()
            == QStringLiteral("True"));
    }
    if (m_decayRateCombo) {
        const int at = m_decayRateCombo->findData(
            s.value(QStringLiteral("PeakDecayRate"), QStringLiteral("Medium")).toString());
        QSignalBlocker block(m_decayRateCombo);
        m_decayRateCombo->setCurrentIndex(at >= 0 ? at : 1);
    }
}

void MeterStylesPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    // --- Section: S-Meter ---
    auto* smGroup = new QGroupBox(QStringLiteral("S-Meter"), this);
    auto* smForm  = new QFormLayout(smGroup);
    smForm->setSpacing(6);

    // R-R3-21: the analog S-meter settings, the ones its right-click
    // menu holds (Meter Face, Peak Hold > Enabled, Peak Hold > Decay),
    // saved under its keys. The group was three greyed placeholders; the
    // S-meter has faces rather than the Arc / Bar / Digital types it
    // listed, so the first row picks the face.
    const auto& s0 = AppSettings::instance();
    using Face = SMeterWidget::FaceStyle;

    m_faceCombo = new QComboBox(smGroup);
    m_faceCombo->setObjectName(QStringLiteral("sMeterFaceCombo"));
    m_faceCombo->setProperty("nereusSetupId", "appearance.meterStyles.face");
    for (int i = 0; i <= static_cast<int>(Face::Classic); ++i) {
        m_faceCombo->addItem(SMeterWidget::faceStyleLabel(static_cast<Face>(i)), i);
    }
    const Face savedFace = SMeterWidget::faceStyleFromKey(
        s0.value(QStringLiteral("SMeter_FaceStyle"),
                 SMeterWidget::faceStyleKey(Face::AgedCream)).toString());
    m_faceCombo->setCurrentIndex(m_faceCombo->findData(static_cast<int>(savedFace)));
    m_faceCombo->setToolTip(QStringLiteral("The S-meter's face"));
    smForm->addRow(QStringLiteral("Face:"), m_faceCombo);

    m_peakHoldToggle = new QCheckBox(QStringLiteral("Peak hold"), smGroup);
    m_peakHoldToggle->setObjectName(QStringLiteral("sMeterPeakHoldCheck"));
    m_peakHoldToggle->setProperty("nereusSetupId", "appearance.meterStyles.peakHold");
    m_peakHoldToggle->setChecked(
        s0.value(QStringLiteral("PeakHoldEnabled"), QStringLiteral("True")).toString()
        == QStringLiteral("True"));
    m_peakHoldToggle->setToolTip(QStringLiteral("Hold the S-meter's peak reading"));
    smForm->addRow(QString(), m_peakHoldToggle);

    m_decayRateCombo = new QComboBox(smGroup);
    m_decayRateCombo->setObjectName(QStringLiteral("sMeterDecayCombo"));
    m_decayRateCombo->setProperty("nereusSetupId", "appearance.meterStyles.peakDecay");
    // SMeterWidget::setPeakDecayRate: 20 / 10 / 5 dB/s.
    m_decayRateCombo->addItem(QStringLiteral("Fast (20 dB/s)"), QStringLiteral("Fast"));
    m_decayRateCombo->addItem(QStringLiteral("Medium (10 dB/s)"), QStringLiteral("Medium"));
    m_decayRateCombo->addItem(QStringLiteral("Slow (5 dB/s)"), QStringLiteral("Slow"));
    {
        const int at = m_decayRateCombo->findData(
            s0.value(QStringLiteral("PeakDecayRate"), QStringLiteral("Medium")).toString());
        m_decayRateCombo->setCurrentIndex(at >= 0 ? at : 1);
    }
    m_decayRateCombo->setToolTip(QStringLiteral("How fast the held peak falls back"));
    smForm->addRow(QStringLiteral("Decay Rate:"), m_decayRateCombo);

    connect(m_faceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
        const int face = m_faceCombo->itemData(index).toInt();
        AppSettings::instance().setValue(QStringLiteral("SMeter_FaceStyle"),
                                         SMeterWidget::faceStyleKey(static_cast<Face>(face)));
        emit sMeterFaceChanged(face);
    });
    connect(m_peakHoldToggle, &QCheckBox::toggled, this, [this](bool on) {
        AppSettings::instance().setValue(QStringLiteral("PeakHoldEnabled"),
                                         on ? QStringLiteral("True") : QStringLiteral("False"));
        emit sMeterPeakHoldChanged(on);
    });
    connect(m_decayRateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
        const QString rate = m_decayRateCombo->itemData(index).toString();
        AppSettings::instance().setValue(QStringLiteral("PeakDecayRate"), rate);
        emit sMeterPeakDecayChanged(rate);
    });

    contentLayout()->addWidget(smGroup);

    // --- Section: VFO Flag ---
    auto* vfoGroup = new QGroupBox(QStringLiteral("VFO Flag"), this);
    vfoGroup->setObjectName(QStringLiteral("appearanceVfoFlagGroup"));
    auto* vfoLayout = new QVBoxLayout(vfoGroup);

    m_smallModeFilterToggle = new QCheckBox(
        QStringLiteral("Small filter display on VFO flag"), vfoGroup);
    m_smallModeFilterToggle->setToolTip(
        QStringLiteral("Compact filter readout under the VFO frequency."));

    vfoLayout->addWidget(m_smallModeFilterToggle);

    // Persist setting
    auto& s = AppSettings::instance();
    m_smallModeFilterToggle->setChecked(
        s.value(QStringLiteral("AppearanceSmallModeFilterOnVfos"), QStringLiteral("False")).toString() == QStringLiteral("True"));

    connect(m_smallModeFilterToggle, &QCheckBox::toggled, this,
        [](bool v) {
            AppSettings::instance().setValue(
                QStringLiteral("AppearanceSmallModeFilterOnVfos"),
                v ? QStringLiteral("True") : QStringLiteral("False"));
            // TODO(future): wire to VfoWidget::setSmallFilterMode(v) for live-apply
            // once VfoWidget has a clean accessor path from MainWindow or SetupDialog
        });

    contentLayout()->addWidget(vfoGroup);
    // R-R3-49: the flag stores this setting but draws nothing with it yet,
    // so the group (its only control) is hidden until that is built.
    UnbuiltFeatures::hideUnlessBuilt(vfoGroup, UnbuiltFeature::SmallFilter);
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
        QStringLiteral("No skins are loaded."), skinGroup);
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
    m_loadBtn->setToolTip(QStringLiteral("Load a skin"));
    m_loadBtn->setAutoDefault(false);
    btnRow->addWidget(m_loadBtn);

    m_saveBtn = new QPushButton(QStringLiteral("Save"), skinGroup);
    m_saveBtn->setEnabled(false);  // NYI
    m_saveBtn->setToolTip(QStringLiteral("Save the current look as a skin"));
    m_saveBtn->setAutoDefault(false);
    btnRow->addWidget(m_saveBtn);

    m_importBtn = new QPushButton(QStringLiteral("Import..."), skinGroup);
    m_importBtn->setEnabled(false);  // NYI
    m_importBtn->setToolTip(QStringLiteral("Importing a skin made for Thetis is not available in this version."));
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
    m_widthSpin->setToolTip(QStringLiteral("Width of the collapsible panel"));
    colForm->addRow(QStringLiteral("Width:"), m_widthSpin);

    m_heightSpin = new QSpinBox(colGroup);
    m_heightSpin->setRange(50, 1000);
    m_heightSpin->setValue(200);
    m_heightSpin->setSuffix(QStringLiteral(" px"));
    m_heightSpin->setEnabled(false);  // NYI
    m_heightSpin->setToolTip(QStringLiteral("Height of the collapsible panel"));
    colForm->addRow(QStringLiteral("Height:"), m_heightSpin);

    m_enableToggle = new QCheckBox(QStringLiteral("Enable collapsible display"), colGroup);
    m_enableToggle->setEnabled(false);  // NYI
    m_enableToggle->setToolTip(QStringLiteral("Let the spectrum section collapse"));
    colForm->addRow(QString(), m_enableToggle);

    contentLayout()->addWidget(colGroup);
    contentLayout()->addStretch();
}

} // namespace NereusSDR
