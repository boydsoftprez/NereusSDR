// =================================================================
// src/gui/setup/AppearanceSetupPages.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.designer.cs (the colour buttons
//     each row corresponds to and the waterfall low colour tooltip text):
//     upstream source has no top-of-file GPL header, the setup.cs header
//     below covers its partial class
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - PR 677 review fix wave (I3): the Thetis header and
//                 provenance row for the setup.designer.cs references and
//                 the tooltip text taken from it; the widened port check
//                 found them. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

// --- From setup.cs ---
//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// --- From setup.designer.cs ---
// Upstream source 'Project Files/Source/Console/setup.designer.cs' has no top-of-file GPL header —
// project-level LICENSE applies (it is the designer half of the Setup class in setup.cs).

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
#include <memory>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// ColorsThemePage
// ---------------------------------------------------------------------------

ColorsThemePage::ColorsThemePage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Colors & Theme"), model, parent)
{
    buildUI();
}

QWidget* ColorsThemePage::takePictureColourRows(QWidget* parent)
{
    auto* rows = new QWidget(parent);
    rows->setObjectName(QStringLiteral("displayPictureColourRows"));
    auto* form = new QFormLayout(rows);
    form->setContentsMargins(0, 0, 0, 0);
    for (ColorSwatchButton* control : {m_traceColorBtn, m_traceFillColorBtn}) {
        auto* original = qobject_cast<QFormLayout*>(control->parentWidget()->layout());
        if (!original) { continue; }
        const QFormLayout::TakeRowResult taken = original->takeRow(control);
        const std::unique_ptr<QLayoutItem> labelItem(taken.labelItem);
        const std::unique_ptr<QLayoutItem> fieldItem(taken.fieldItem);
        if (!fieldItem || !fieldItem->widget()) { continue; }
        if (labelItem && labelItem->widget()) {
            form->addRow(labelItem->widget(), fieldItem->widget());
        } else {
            form->addRow(fieldItem->widget());
        }
    }
    m_traceColorBtn->setProperty("nereusSetupId", "display.spectrum.traceColor");
    m_traceFillColorBtn->setProperty("nereusSetupId", "display.spectrum.fillColor");
    return rows;
}

void ColorsThemePage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    // Helper: build a ColorSwatchButton for one colour default.
    // Display pill plan Task 21 (D26): a colour chosen here is the Setup
    // default for every pan. applyDisplayDefault stores it and moves each
    // pan that has no colour of its own; the button opens on the default,
    // not on the active pan's colour, and follows the default while the
    // page is open.
    auto makeBtn = [this](QWidget* parent, const QColor& defaultColor,
                          const QString& keyBase,
                          void (SpectrumWidget::*setter)(const QColor&)) {
        auto* btn = new ColorSwatchButton(defaultColor, parent);
        connect(btn, &ColorSwatchButton::colorChanged, this,
                [this, keyBase, setter](const QColor& c) {
            applyDisplayDefault(keyBase, c.name(QColor::HexArgb),
                                [setter, c](SpectrumWidget* pan) { (pan->*setter)(c); });
        });
        followDisplayDefault(keyBase, [this, btn, keyBase]() {
            const QColor c = QColor::fromString(displayDefaultText(keyBase));
            if (!c.isValid()) { return; }
            QSignalBlocker block(btn);
            btn->setColor(c);
        });
        return btn;
    };

    // --- Section: Spectrum ---
    auto* specGroup = new QGroupBox(QStringLiteral("Spectrum"), this);
    auto* specForm  = new QFormLayout(specGroup);
    specForm->setSpacing(6);

    // Trace Color and Fill Color — moved from Display → Spectrum Defaults
    // "Trace Colors" group.
    // Thetis: setup.designer.cs:3234 (clrbtnDataLine) / :3217 (clrbtnDataFill).
    // Display pill plan Task 21 (S13): two pickers, as Thetis has them. They
    // were one while SpectrumWidget shared one colour for line and fill.
    //
    // The trace has no colour of its own until one is chosen: it follows the
    // fill (SpectrumWidget::traceColor()). So the Trace button shows the
    // fill default until then, and moves with it.
    m_traceColorBtn = new ColorSwatchButton(QColor(0x00, 0xe5, 0xff), specGroup);
    m_traceColorBtn->setObjectName(QStringLiteral("appearanceTraceColorBtn"));
    m_traceColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.traceColor");
    m_traceColorBtn->setToolTip(QStringLiteral(
        "Click to choose the spectrum trace line color. "
        "Until you choose one, the trace uses the fill color."));
    connect(m_traceColorBtn, &ColorSwatchButton::colorChanged, this, [this](const QColor& c) {
        applyDisplayDefault(QStringLiteral("DisplayTraceColor"), c.name(QColor::HexArgb),
                            [c](SpectrumWidget* pan) { pan->setTraceColor(c); });
    });
    const auto showTraceDefault = [this]() {
        QColor c = QColor::fromString(displayDefaultText(QStringLiteral("DisplayTraceColor")));
        if (!c.isValid()) {
            c = QColor::fromString(displayDefaultText(QStringLiteral("DisplayFillColor")));
        }
        if (!c.isValid()) { return; }
        QSignalBlocker block(m_traceColorBtn);
        m_traceColorBtn->setColor(c);
    };
    followDisplayDefault(QStringLiteral("DisplayTraceColor"), showTraceDefault);
    specForm->addRow(QStringLiteral("Trace Color:"), m_traceColorBtn);

    m_traceFillColorBtn = makeBtn(specGroup,
        QColor(0x00, 0xe5, 0xff),
        QStringLiteral("DisplayFillColor"),
        &SpectrumWidget::setFillColor);
    m_traceFillColorBtn->setObjectName(QStringLiteral("appearanceFillColorBtn"));
    m_traceFillColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.traceFillColor");
    m_traceFillColorBtn->setToolTip(QStringLiteral(
        "Click to choose the spectrum fill color. "
        "The color picker lets you adjust alpha for the fill opacity."));  // a QColorDialog
    // The fill default also moves a Trace button that is following it.
    followDisplayDefault(QStringLiteral("DisplayFillColor"), [this, showTraceDefault]() {
        const QColor c = QColor::fromString(displayDefaultText(QStringLiteral("DisplayFillColor")));
        if (c.isValid()) {
            QSignalBlocker block(m_traceFillColorBtn);
            m_traceFillColorBtn->setColor(c);
        }
        showTraceDefault();
    });
    connect(m_traceFillColorBtn, &ColorSwatchButton::colorChanged, this,
            [showTraceDefault](const QColor&) { showTraceDefault(); });
    specForm->addRow(QStringLiteral("Fill Color:"), m_traceFillColorBtn);

    // Grid Color — moved from Display → Grid & Scales "Colors" group (G9).
    m_gridColorBtn = makeBtn(specGroup,
        QColor(255, 255, 255, 40),
        QStringLiteral("DisplayGridColor"),
        &SpectrumWidget::setGridColor);
    m_gridColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.gridColor");
    // Thetis: setup.designer.cs:3202 (clrbtnGrid) — rewritten
    // Thetis original: (none)
    m_gridColorBtn->setToolTip(QStringLiteral("Color of the major vertical grid lines on the panadapter."));
    specForm->addRow(QStringLiteral("Grid Color:"), m_gridColorBtn);

    // Grid Fine Color — moved from Display → Grid & Scales "Colors" group (G10).
    m_gridFineColorBtn = makeBtn(specGroup,
        QColor(255, 255, 255, 20),
        QStringLiteral("DisplayGridFineColor"),
        &SpectrumWidget::setGridFineColor);
    m_gridFineColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.gridFineColor");
    // Thetis: setup.designer.cs:3198 (clrbtnGridFine) — rewritten
    // Thetis original: (none)
    m_gridFineColorBtn->setToolTip(QStringLiteral("Color of the minor (fine) grid lines between major grid lines on the panadapter."));
    specForm->addRow(QStringLiteral("Grid Fine Color:"), m_gridFineColorBtn);

    // H-Grid Color — moved from Display → Grid & Scales "Colors" group (G11).
    m_hGridColorBtn = makeBtn(specGroup,
        QColor(255, 255, 255, 40),
        QStringLiteral("DisplayHGridColor"),
        &SpectrumWidget::setHGridColor);
    m_hGridColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.hGridColor");
    // Thetis: setup.designer.cs:3193 (clrbtnHGridColor) — rewritten
    // Thetis original: (none)
    m_hGridColorBtn->setToolTip(QStringLiteral("Color of the horizontal dB grid lines on the panadapter."));
    specForm->addRow(QStringLiteral("H-Grid Color:"), m_hGridColorBtn);

    // Grid Text Color — moved from Display → Grid & Scales "Colors" group (G12).
    m_gridTextColorBtn = makeBtn(specGroup,
        QColor(255, 255, 0),
        QStringLiteral("DisplayGridTextColor"),
        &SpectrumWidget::setGridTextColor);
    m_gridTextColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.gridTextColor");
    // Thetis: setup.designer.cs:3206 (clrbtnText) — rewritten
    // Thetis original: (none)
    m_gridTextColorBtn->setToolTip(QStringLiteral("Color of the frequency and dB labels drawn on the panadapter grid."));
    specForm->addRow(QStringLiteral("Grid Text Color:"), m_gridTextColorBtn);

    // Band Edge Color — moved from Display → Grid & Scales "Colors" group (G6).
    m_bandEdgeColorBtn = makeBtn(specGroup,
        QColor(255, 0, 0),
        QStringLiteral("DisplayBandEdgeColor"),
        &SpectrumWidget::setBandEdgeColor);
    m_bandEdgeColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.bandEdgeColor");
    // Thetis: setup.designer.cs:3232 (clrbtnBandEdge) — rewritten
    // Thetis original: (none)
    m_bandEdgeColorBtn->setToolTip(QStringLiteral("Color of the band edge markers drawn at the amateur band boundaries on the panadapter."));
    specForm->addRow(QStringLiteral("Band Edge Color:"), m_bandEdgeColorBtn);

    // RX Zero Line Color — moved from Display → Grid & Scales "Colors" group (G13).
    m_rxZeroLineColorBtn = makeBtn(specGroup,
        QColor(255, 0, 0),
        QStringLiteral("DisplayRxZeroLineColor"),
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
        QStringLiteral("DisplayTxZeroLineColor"),
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
        QStringLiteral("DisplayRxFilterColor"),
        &SpectrumWidget::setRxFilterColor);
    m_rxFilterColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.rxFilterColor");
    m_rxFilterColorBtn->setToolTip(QStringLiteral(
        "Click to choose the RX passband overlay color and opacity. "
        "Shown on the panadapter and waterfall slice band during receive."));
    specForm->addRow(QStringLiteral("RX Passband Color:"), m_rxFilterColorBtn);

    // TX Passband Color — Plan 4 D9b, already lived here.
    m_txFilterColorBtn = makeBtn(specGroup,
        QColor(255, 120, 60, 46),
        QStringLiteral("DisplayTxFilterColor"),
        &SpectrumWidget::setTxFilterColor);
    m_txFilterColorBtn->setProperty("nereusSetupId", "appearance.colorsTheme.txFilterColor");
    m_txFilterColorBtn->setToolTip(QStringLiteral(
        "Click to choose the TX passband overlay color and opacity. "
        "Shown on the panadapter and waterfall during MOX/TUNE."));
    specForm->addRow(QStringLiteral("TX Passband Color:"), m_txFilterColorBtn);

    addContent(specGroup);

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

    addContent(wfGroup);

    // Reset all colors to defaults — broadened from the Plan 4 D9c-3 button
    // that lived on Spectrum Defaults to cover all Colors & Theme entries.
    auto* resetRow = new QHBoxLayout;
    resetRow->addStretch(1);
    auto* resetBtn = new QPushButton(QStringLiteral("Reset all colors to defaults"), this);
    resetBtn->setProperty("nereusSetupId", "appearance.colorsTheme.resetColors");
    resetBtn->setToolTip(QStringLiteral(
        "Reset the nine Appearance colors above to factory defaults. "
        "Other display settings (FPS, averaging, thresholds, etc.) are not affected."));
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        const auto res = QMessageBox::question(
            this,
            QStringLiteral("Reset all colors to defaults"),
            QStringLiteral(
                "Reset the nine Appearance colors above to factory defaults?\n\n"
                "Custom colors set here will be discarded. "
                "Other display settings are not affected."),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (res != QMessageBox::Yes) { return; }
        resetColorsToDefaultsForTest();
    });
    resetRow->addWidget(resetBtn);
    resetRow->addStretch(1);
    addContent(resetRow);

    showDisplayDefaults();
}

void ColorsThemePage::resetColorsToDefaultsForTest()
{
    // Display pill plan Task 21 (D26): each factory colour is stored as the
    // Setup default and reaches every pan that has no colour of its own,
    // whether or not the button already showed it. The values are the ones
    // SpectrumWidget::resetDisplayColorsToDefaults() sets. The trace colour
    // is left as chosen: a chosen trace colour cannot be un-chosen.
    // Approved Display rebuild: trace, fill and background are picture
    // choices on Display's pages; Appearance resets only its nine colours.
    const auto reset = [this](const char* keyBase, const QColor& c,
                              void (SpectrumWidget::*setter)(const QColor&)) {
        applyDisplayDefault(QString::fromLatin1(keyBase), c.name(QColor::HexArgb),
                            [setter, c](SpectrumWidget* pan) { (pan->*setter)(c); });
    };
    reset("DisplayGridColor", QColor(255, 255, 255, 40), &SpectrumWidget::setGridColor);
    reset("DisplayGridFineColor", QColor(255, 255, 255, 20), &SpectrumWidget::setGridFineColor);
    reset("DisplayHGridColor", QColor(255, 255, 255, 40), &SpectrumWidget::setHGridColor);
    reset("DisplayGridTextColor", QColor(255, 255, 0), &SpectrumWidget::setGridTextColor);
    reset("DisplayBandEdgeColor", QColor(255, 0, 0), &SpectrumWidget::setBandEdgeColor);
    reset("DisplayRxZeroLineColor", QColor(255, 0, 0), &SpectrumWidget::setRxZeroLineColor);
    reset("DisplayTxZeroLineColor", QColor(255, 184, 0), &SpectrumWidget::setTxZeroLineColor);
    reset("DisplayRxFilterColor", QColor(0x00, 0xb4, 0xd8, 80), &SpectrumWidget::setRxFilterColor);
    reset("DisplayTxFilterColor", QColor(255, 120, 60, 46), &SpectrumWidget::setTxFilterColor);
    showDisplayDefaults();
    // waterfall low default (the hidden legacy control is outside these nine).
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
        // Added dials with no face of their own follow this face.
        SMeterSetupFace::instance().notify(face);
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

    addContent(smGroup);

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

    addContent(vfoGroup);
    // R-R3-49: the flag stores this setting but draws nothing with it yet,
    // so the group (its only control) is hidden until that is built.
    UnbuiltFeatures::hideUnlessBuilt(vfoGroup, UnbuiltFeature::SmallFilter);
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

    addContent(skinGroup);
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

    addContent(colGroup);
}

} // namespace NereusSDR
