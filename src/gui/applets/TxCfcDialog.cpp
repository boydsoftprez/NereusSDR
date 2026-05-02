// =================================================================
// src/gui/applets/TxCfcDialog.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/frmCFCConfig.{cs,Designer.cs}
//   [v2.10.3.13] — per-band CFC editor (Continuous Frequency
//   Compressor).  Original licence reproduced below.
//
// Layout 1:1 with frmCFCConfig.Designer.cs [v2.10.3.13]:
//   - Two ParametricEqWidget instances stacked vertically (compression
//     curve on top, post-EQ curve on bottom).
//   - Top + middle edit rows operating on the currently selected band.
//   - Right column: 5/10/18-band radios, freq-range spinboxes,
//     three checkboxes (Use Q Factors / Live Update / Log scale),
//     two reset buttons, OG CFC Guide LinkLabel.
//
// Cross-sync, live-update gating, hide-on-close, and 50ms bar chart
// timer all match the Thetis behavior.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — Phase 3M-3a-ii Batch 6 (Task A): created by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-04-30 — Phase 3M-3a-ii follow-up sub-PR Batch 8: full
//                 Thetis-verbatim rewrite by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-04-30 — Phase 3M-3a-ii follow-up sub-PR style fix: added a
//                 dialog-level QSS block in the constructor so default
//                 Qt6 widgets (spinboxes, combos, radios, checkboxes,
//                 group-boxes, labels, push-buttons) pick up the
//                 project's dark theme.  Without this, every control on
//                 this dialog rendered with the system default look, which
//                 read as dark-on-dark against the project's #0f0f1a
//                 dialog background and made the controls effectively
//                 invisible during bench test.  J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
// =================================================================

//=================================================================
// frmCFCConfig.cs
//=================================================================
//  frmCFCConfig.cs
//
// This file is part of a program that implements a Software-Defined Radio.
//
// This code/file can be found on GitHub : https://github.com/ramdor/Thetis
//
// Copyright (C) 2020-2026 Richard Samphire MW0LGE
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// The author can be reached by email at
//
// mw0lge@grange-lane.co.uk
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
//============================================================================================//

#include "TxCfcDialog.h"

#include "core/TxChannel.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/ParametricEqWidget.h"
#include "models/TransmitModel.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

// Defaults sourced byte-for-byte from frmCFCConfig.Designer.cs [v2.10.3.13].
// nudCFC_f Maximum=20000 (line 267-271).
constexpr int    kFreqHzMin           = 0;
constexpr int    kFreqHzMax           = 20000;
// nudCFC_precomp Maximum=16, Minimum=0 (line 408-417).
constexpr double kPrecompDbMin        = 0.0;
constexpr double kPrecompDbMax        = 16.0;
// nudCFC_posteqgain Maximum=24, Minimum=-24 (line 337-346).
constexpr double kPostEqGainDbMin     = -24.0;
constexpr double kPostEqGainDbMax     =  24.0;
// nudCFC_c Maximum=16, Minimum=0 (line 217-226).
constexpr double kCompDbMin           = 0.0;
constexpr double kCompDbMax           = 16.0;
// nudCFC_gain Maximum=24, Minimum=-24 (line 564-573).
constexpr double kGainDbMin           = -24.0;
constexpr double kGainDbMax           =  24.0;
// nudCFC_q / nudCFC_cq Maximum=20, Minimum=0.2 (line 597-617).
constexpr double kQMin                = 0.2;
constexpr double kQMax                = 20.0;
// nudCFC_selected_band Maximum=10 default, Minimum=1, Value=10 (line 360-385).
constexpr int    kSelectedBandMin     = 1;
constexpr int    kSelectedBandDefault = 10;

// Default min/max freqs match the widget defaults (frmCFCConfig.cs:89-99 calls
// ucCFC_comp.GetDefaults(...,10) which returns minHz=0, maxHz=4000 per
// ucParametricEq.cs:1107-1131 [v2.10.3.13]).
constexpr double kDefaultMinHz        = 0.0;
constexpr double kDefaultMaxHz        = 4000.0;

// frmCFCConfig.cs:122-138 [v2.10.3.13] — Low/High spinboxes enforce a
// minimum 1 kHz spread when the user types a value that would invert them.
constexpr int    kMinFreqSpreadHz     = 1000;

// OG CFC Guide URL — verbatim from frmCFCConfig.cs:600 [v2.10.3.13].
constexpr const char* kOgGuideUrl =
    "https://www.w1aex.com/anan/CFC_Audio_Tools/CFC_Audio_Tools.html";

// Bar chart timer interval (ms) — frmCFCConfig.cs:447 [v2.10.3.13]: 50 ms.
constexpr int    kBarChartTimerMs     = 50;

} // namespace

// ─────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────

TxCfcDialog::TxCfcDialog(TransmitModel* tm,
                         TxChannel* tx,
                         QWidget* parent)
    : QDialog(parent)
    , m_tm(tm)
    , m_tx(tx)
{
    setWindowTitle(tr("CFC Config"));  // Matches frmCFCConfig.Designer.cs:760 [v2.10.3.13]
    setObjectName(QStringLiteral("TxCfcDialog"));
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose, false);

    // Dialog-level QSS — without this, default Qt6 widgets (spinboxes,
    // combos, radios, checkboxes, group-boxes, labels) render with the
    // system default theme inside an otherwise-dark dialog, producing the
    // dark-on-dark "invisible boxes" reported during the 3M-3a-ii
    // follow-up bench test.  The block layers on the project's
    // StyleConstants helpers via QSS selectors; per-widget setStyleSheet
    // calls (OG Guide hyperlink button below) keep their own specificity.
    setStyleSheet(QString::fromLatin1(NereusSDR::Style::kPageStyle)
                  + QString::fromLatin1(NereusSDR::Style::kGroupBoxStyle)
                  + QString::fromLatin1(NereusSDR::Style::kSpinBoxStyle)
                  + NereusSDR::Style::doubleSpinBoxStyle()
                  + QString::fromLatin1(NereusSDR::Style::kComboStyle)
                  + QString::fromLatin1(NereusSDR::Style::kCheckBoxStyle)
                  + QString::fromLatin1(NereusSDR::Style::kRadioButtonStyle)
                  + QString::fromLatin1(NereusSDR::Style::kButtonStyle));

    buildUi();
    wireSignals();
    seedWidgetsFromTransmitModel();
    syncFromModel();
    updateSelectedRowEnable();

    // Bar chart timer.  Started in showEvent, stopped in hideEvent.
    m_barChartTimer = new QTimer(this);
    m_barChartTimer->setInterval(kBarChartTimerMs);
    connect(m_barChartTimer, &QTimer::timeout, this, &TxCfcDialog::onBarChartTick);
}

TxCfcDialog::~TxCfcDialog() = default;

void TxCfcDialog::setTxChannel(TxChannel* tx)
{
    m_tx = tx;
}

// ─────────────────────────────────────────────────────────────────────
// UI build-out — 1:1 with frmCFCConfig.Designer.cs [v2.10.3.13].
//
// Hybrid layout choice:
//   Thetis frmCFCConfig is absolutely positioned (Forms Designer). For
//   the Qt port we use nested QHBox/QVBox/QGrid layouts so the dialog
//   reflows under different system fonts / locales.  Group-box headings
//   replace the loose label-and-spinbox clusters Thetis uses; the
//   intent ("here are the per-band edits", "here are the global
//   settings") is preserved.  Within each layout the visual order
//   (Selected band → f → Pre-Comp → Comp → Q on the top edit row, etc.)
//   matches Thetis's left-to-right tab order.
// ─────────────────────────────────────────────────────────────────────

void TxCfcDialog::buildUi()
{
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(8);

    // ── Left column: edit rows + two parametric EQ widgets ───────────────
    auto* leftCol = new QVBoxLayout;
    leftCol->setSpacing(6);

    // ── Top edit row (above ucCFC_comp) ──────────────────────────────────
    // From Thetis frmCFCConfig.Designer.cs:30-65 [v2.10.3.13] — labels
    // labelTS667 ("#"), labelTS664 ("f"), labelTS662 ("Pre-Comp"),
    // labelTS672 ("Comp"), labelTS673 ("dB"), labelTS1 ("Q").
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);

        row->addWidget(new QLabel(tr("#"), this));
        m_selectedBandSpin = new QSpinBox(this);
        m_selectedBandSpin->setObjectName(QStringLiteral("TxCfcSelectedBandSpin"));
        m_selectedBandSpin->setRange(kSelectedBandMin, kSelectedBandDefault);
        m_selectedBandSpin->setValue(kSelectedBandDefault);
        m_selectedBandSpin->setReadOnly(true);  // Designer.cs:377
        m_selectedBandSpin->setToolTip(tr(
            "Currently selected band number (read-only — click a point on "
            "either curve to change)."));
        row->addWidget(m_selectedBandSpin);

        row->addWidget(new QLabel(tr("f"), this));
        m_freqSpin = new QSpinBox(this);
        m_freqSpin->setObjectName(QStringLiteral("TxCfcFreqSpin"));
        m_freqSpin->setRange(kFreqHzMin, kFreqHzMax);
        m_freqSpin->setSuffix(QStringLiteral(" Hz"));
        m_freqSpin->setToolTip(tr(
            "Center frequency of the selected band (0–20000 Hz)."));
        row->addWidget(m_freqSpin);

        row->addWidget(new QLabel(tr("Pre-Comp"), this));
        m_precompSpin = new QDoubleSpinBox(this);
        m_precompSpin->setObjectName(QStringLiteral("TxCfcPrecompSpin"));
        m_precompSpin->setDecimals(1);  // Designer.cs:401
        m_precompSpin->setSingleStep(0.1);
        m_precompSpin->setRange(kPrecompDbMin, kPrecompDbMax);
        m_precompSpin->setSuffix(QStringLiteral(" dB"));
        m_precompSpin->setToolTip(tr(
            "Global pre-compression gain (0–16 dB) applied before "
            "per-band compression."));
        row->addWidget(m_precompSpin);

        row->addWidget(new QLabel(tr("Comp"), this));
        m_compSpin = new QDoubleSpinBox(this);
        m_compSpin->setObjectName(QStringLiteral("TxCfcCompSpin"));
        m_compSpin->setDecimals(1);  // Designer.cs:210
        m_compSpin->setSingleStep(0.1);
        m_compSpin->setRange(kCompDbMin, kCompDbMax);
        m_compSpin->setSuffix(QStringLiteral(" dB"));
        m_compSpin->setToolTip(tr(
            "Compression amount (0–16 dB) for the selected band."));
        row->addWidget(m_compSpin);

        row->addWidget(new QLabel(tr("Q"), this));
        m_compQSpin = new QDoubleSpinBox(this);
        m_compQSpin->setObjectName(QStringLiteral("TxCfcCompQSpin"));
        m_compQSpin->setDecimals(2);  // Designer.cs:114
        m_compQSpin->setSingleStep(0.01);
        m_compQSpin->setRange(kQMin, kQMax);
        m_compQSpin->setToolTip(tr(
            "Q factor for the selected band's compression curve."));
        row->addWidget(m_compQSpin);

        row->addStretch(1);
        leftCol->addLayout(row);
    }

    // ── ucCFC_comp (compression curve + bar chart) ───────────────────────
    // From Thetis frmCFCConfig.Designer.cs:155-196 [v2.10.3.13].  Settings:
    //   - DbMax=16, DbMin=0, FrequencyMaxHz=4000, FrequencyMinHz=0
    //   - GlobalGainIsHorizLine=true (the pre-comp scalar draws as a
    //     horizontal line at GlobalGainDb)
    //   - ShowDotReadingsAsComp=true (per-point readout in dB-comp form)
    //   - ParametricEQ=true (Q factors enabled)
    //   - YAxisStepDb=2
    m_compWidget = new ParametricEqWidget(this);
    m_compWidget->setObjectName(QStringLiteral("TxCfcCompWidget"));
    m_compWidget->setDbMax(16.0);
    m_compWidget->setDbMin(0.0);
    m_compWidget->setFrequencyMaxHz(kDefaultMaxHz);
    m_compWidget->setFrequencyMinHz(kDefaultMinHz);
    m_compWidget->setGlobalGainIsHorizLine(true);
    m_compWidget->setShowDotReadingsAsComp(true);
    m_compWidget->setShowDotReadings(true);
    m_compWidget->setShowReadout(false);
    m_compWidget->setShowAxisScales(true);
    m_compWidget->setShowBandShading(true);
    m_compWidget->setUsePerBandColours(true);
    m_compWidget->setParametricEq(true);
    m_compWidget->setYAxisStepDb(2.0);
    m_compWidget->setMinimumSize(509, 320);  // Designer.cs:188 Size

    leftCol->addWidget(m_compWidget, 1);

    // ── Middle edit row (between widgets) ────────────────────────────────
    // From Thetis frmCFCConfig.Designer.cs:30-65 [v2.10.3.13] — labels
    // labelTS671 ("Post-EQ"), labelTS670 ("dB"), labelTS666 ("Gain"),
    // labelTS663 ("dB"), labelTS665 ("Q").
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);

        row->addWidget(new QLabel(tr("Post-EQ"), this));
        m_postEqGainSpin = new QDoubleSpinBox(this);
        m_postEqGainSpin->setObjectName(QStringLiteral("TxCfcPostEqGainSpin"));
        m_postEqGainSpin->setDecimals(1);  // Designer.cs:330
        m_postEqGainSpin->setSingleStep(0.1);
        m_postEqGainSpin->setRange(kPostEqGainDbMin, kPostEqGainDbMax);
        m_postEqGainSpin->setSuffix(QStringLiteral(" dB"));
        m_postEqGainSpin->setToolTip(tr(
            "Global Post-EQ make-up gain (-24 to +24 dB)."));
        row->addWidget(m_postEqGainSpin);

        row->addSpacing(20);

        row->addWidget(new QLabel(tr("Gain"), this));
        m_gainSpin = new QDoubleSpinBox(this);
        m_gainSpin->setObjectName(QStringLiteral("TxCfcGainSpin"));
        m_gainSpin->setDecimals(1);  // Designer.cs:557
        m_gainSpin->setSingleStep(0.1);
        m_gainSpin->setRange(kGainDbMin, kGainDbMax);
        m_gainSpin->setSuffix(QStringLiteral(" dB"));
        m_gainSpin->setToolTip(tr(
            "Post-EQ band gain (-24 to +24 dB) for the selected band."));
        row->addWidget(m_gainSpin);

        row->addWidget(new QLabel(tr("Q"), this));
        m_eqQSpin = new QDoubleSpinBox(this);
        m_eqQSpin->setObjectName(QStringLiteral("TxCfcEqQSpin"));
        m_eqQSpin->setDecimals(2);  // Designer.cs:597
        m_eqQSpin->setSingleStep(0.01);
        m_eqQSpin->setRange(kQMin, kQMax);
        m_eqQSpin->setToolTip(tr(
            "Q factor for the selected band's post-EQ curve."));
        row->addWidget(m_eqQSpin);

        row->addStretch(1);
        leftCol->addLayout(row);
    }

    // ── ucCFC_eq (post-EQ curve) ─────────────────────────────────────────
    // From Thetis frmCFCConfig.Designer.cs:665-703 [v2.10.3.13].  Settings:
    //   - DbMax=24, DbMin=-24, FrequencyMaxHz=4000, FrequencyMinHz=0
    //   - GlobalGainIsHorizLine omitted (default false — gain handle on
    //     LHS gutter rather than horizontal line)
    //   - ShowDotReadings=true, ShowDotReadingsAsComp=false
    //   - ParametricEQ=true (Q factors enabled)
    m_postEqWidget = new ParametricEqWidget(this);
    m_postEqWidget->setObjectName(QStringLiteral("TxCfcPostEqWidget"));
    m_postEqWidget->setDbMax(24.0);
    m_postEqWidget->setDbMin(-24.0);
    m_postEqWidget->setFrequencyMaxHz(kDefaultMaxHz);
    m_postEqWidget->setFrequencyMinHz(kDefaultMinHz);
    m_postEqWidget->setShowDotReadings(true);
    m_postEqWidget->setShowReadout(false);
    m_postEqWidget->setShowAxisScales(true);
    m_postEqWidget->setShowBandShading(true);
    m_postEqWidget->setUsePerBandColours(true);
    m_postEqWidget->setParametricEq(true);
    m_postEqWidget->setMinimumSize(509, 320);  // Designer.cs:696 Size

    leftCol->addWidget(m_postEqWidget, 1);

    outer->addLayout(leftCol, 1);

    // ── Right column: band-count radios + freq range + checkboxes +
    //                  reset buttons + OG CFC Guide ──────────────────────
    auto* rightCol = new QVBoxLayout;
    rightCol->setSpacing(6);

    // Band-count radios — Designer.cs:470-553.  Default 10 (line 474).
    {
        auto* grp = new QGroupBox(tr("Bands"), this);
        auto* col = new QVBoxLayout(grp);
        col->setContentsMargins(8, 14, 8, 8);
        col->setSpacing(2);

        m_bands5Radio  = new QRadioButton(tr("5-band"),  grp);
        m_bands5Radio->setObjectName(QStringLiteral("TxCfcBands5Radio"));
        m_bands5Radio->setToolTip(tr(
            "Switch CFC layout to 5 bands.  Resets per-band points."));

        m_bands10Radio = new QRadioButton(tr("10-band"), grp);
        m_bands10Radio->setObjectName(QStringLiteral("TxCfcBands10Radio"));
        m_bands10Radio->setChecked(true);
        m_bands10Radio->setToolTip(tr(
            "Switch CFC layout to 10 bands (default).  Resets per-band points."));

        m_bands18Radio = new QRadioButton(tr("18-band"), grp);
        m_bands18Radio->setObjectName(QStringLiteral("TxCfcBands18Radio"));
        m_bands18Radio->setToolTip(tr(
            "Switch CFC layout to 18 bands.  Resets per-band points."));

        m_bandCountGroup = new QButtonGroup(grp);
        m_bandCountGroup->addButton(m_bands5Radio,   5);
        m_bandCountGroup->addButton(m_bands10Radio, 10);
        m_bandCountGroup->addButton(m_bands18Radio, 18);

        col->addWidget(m_bands5Radio);
        col->addWidget(m_bands10Radio);
        col->addWidget(m_bands18Radio);
        rightCol->addWidget(grp);
    }

    // Freq range — Designer.cs:440-468 udCFC_low + Designer.cs:625-653
    // udCFC_high.  Defaults: low=0, high=16000 (line 463/648).
    {
        auto* grp = new QGroupBox(tr("Freq Range"), this);
        auto* g = new QGridLayout(grp);
        g->setContentsMargins(8, 14, 8, 8);
        g->setHorizontalSpacing(6);
        g->setVerticalSpacing(4);

        g->addWidget(new QLabel(tr("Low"),  grp), 0, 0);
        m_lowSpin = new QSpinBox(grp);
        m_lowSpin->setObjectName(QStringLiteral("TxCfcLowSpin"));
        m_lowSpin->setRange(kFreqHzMin, kFreqHzMax);
        m_lowSpin->setSuffix(QStringLiteral(" Hz"));
        m_lowSpin->setValue(static_cast<int>(kDefaultMinHz));
        m_lowSpin->setToolTip(tr(
            "Lower edge of the visible CFC freq range (Hz).  Must be at "
            "least 1000 Hz below High."));
        g->addWidget(m_lowSpin, 0, 1);

        g->addWidget(new QLabel(tr("High"), grp), 1, 0);
        m_highSpin = new QSpinBox(grp);
        m_highSpin->setObjectName(QStringLiteral("TxCfcHighSpin"));
        m_highSpin->setRange(kFreqHzMin, kFreqHzMax);
        m_highSpin->setSuffix(QStringLiteral(" Hz"));
        m_highSpin->setValue(static_cast<int>(kDefaultMaxHz));
        m_highSpin->setToolTip(tr(
            "Upper edge of the visible CFC freq range (Hz).  Must be at "
            "least 1000 Hz above Low."));
        g->addWidget(m_highSpin, 1, 1);

        rightCol->addWidget(grp);
    }

    // Checkboxes — Designer.cs:90-100, 484-496, 238-247.
    {
        m_useQFactorsChk = new QCheckBox(tr("Use Q Factors"), this);
        m_useQFactorsChk->setObjectName(QStringLiteral("TxCfcUseQFactorsChk"));
        m_useQFactorsChk->setChecked(true);  // Designer.cs:487 default
        m_useQFactorsChk->setToolTip(tr(
            "Use Q factors per band when computing the CFC profile.  When off, "
            "the Q columns are ignored and the curve degenerates to flat-band."));
        rightCol->addWidget(m_useQFactorsChk);

        m_liveUpdateChk = new QCheckBox(tr("Live Update"), this);
        m_liveUpdateChk->setObjectName(QStringLiteral("TxCfcLiveUpdateChk"));
        m_liveUpdateChk->setChecked(false);  // Designer.cs default unchecked
        m_liveUpdateChk->setToolTip(tr(
            "Push CFC values through to the radio while dragging points.  "
            "When off, the WDSP profile is updated only on mouse-release."));
        rightCol->addWidget(m_liveUpdateChk);

        m_logScaleChk = new QCheckBox(tr("Log scale"), this);
        m_logScaleChk->setObjectName(QStringLiteral("TxCfcLogScaleChk"));
        m_logScaleChk->setChecked(false);
        m_logScaleChk->setToolTip(tr(
            "Render both curves on a log frequency axis."));
        rightCol->addWidget(m_logScaleChk);
    }

    rightCol->addSpacing(8);

    // Reset Comp + Reset EQ buttons — Designer.cs:142-153 (btnResetEQ
    // located near top of EQ widget) + 508-519 (btnResetComp at top of
    // Comp widget).
    {
        m_resetCompBtn = new QPushButton(tr("Reset Comp"), this);
        m_resetCompBtn->setObjectName(QStringLiteral("TxCfcResetCompBtn"));
        m_resetCompBtn->setToolTip(tr(
            "Reset the compression curve to default (flat) values."));
        rightCol->addWidget(m_resetCompBtn);

        m_resetEqBtn = new QPushButton(tr("Reset EQ"), this);
        m_resetEqBtn->setObjectName(QStringLiteral("TxCfcResetEqBtn"));
        m_resetEqBtn->setToolTip(tr(
            "Reset the post-EQ curve to default (flat) values."));
        rightCol->addWidget(m_resetEqBtn);
    }

    rightCol->addStretch(1);

    // OG CFC Guide LinkLabel — Designer.cs:78-88.  Verbatim text:
    //   "OG CFC Guide\r\nby W1AEX"
    // (newline between the two lines).  We use a flat QPushButton styled
    // as a hyperlink — Qt has no exact LinkLabel equivalent.
    {
        m_ogGuideLink = new QPushButton(tr("OG CFC Guide\nby W1AEX"), this);
        m_ogGuideLink->setObjectName(QStringLiteral("TxCfcOgGuideLink"));
        m_ogGuideLink->setFlat(true);
        m_ogGuideLink->setCursor(Qt::PointingHandCursor);
        m_ogGuideLink->setStyleSheet(QStringLiteral(
            "QPushButton { color: palette(link); text-decoration: underline; }"));
        m_ogGuideLink->setToolTip(tr(
            "Opens the W1AEX CFC tuning guide in your default web browser."));
        rightCol->addWidget(m_ogGuideLink, 0, Qt::AlignRight);
    }

    outer->addLayout(rightCol, 0);

    // Designer.cs:719 ClientSize=624,717.  Use as a sensible minimum.
    setMinimumSize(620, 700);
}

// ─────────────────────────────────────────────────────────────────────
// Signal wiring
// ─────────────────────────────────────────────────────────────────────

void TxCfcDialog::wireSignals()
{
    // ── Right-column controls ───────────────────────────────────────────
    connect(m_bandCountGroup, &QButtonGroup::idToggled,
            this, [this](int /*id*/, bool checked) {
        if (checked) onBandCountChanged();
    });
    connect(m_lowSpin,  qOverload<int>(&QSpinBox::valueChanged),
            this, &TxCfcDialog::onLowFreqChanged);
    connect(m_highSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &TxCfcDialog::onHighFreqChanged);
    connect(m_useQFactorsChk, &QCheckBox::toggled,
            this, &TxCfcDialog::onUseQFactorsToggled);
    connect(m_logScaleChk,    &QCheckBox::toggled,
            this, &TxCfcDialog::onLogScaleToggled);
    connect(m_resetCompBtn, &QPushButton::clicked,
            this, &TxCfcDialog::onResetCompClicked);
    connect(m_resetEqBtn,   &QPushButton::clicked,
            this, &TxCfcDialog::onResetEqClicked);
    connect(m_ogGuideLink,  &QPushButton::clicked,
            this, &TxCfcDialog::onOgGuideClicked);

    // ── Top edit row ─────────────────────────────────────────────────────
    connect(m_selectedBandSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &TxCfcDialog::onSelectedBandChanged);
    connect(m_freqSpin,    qOverload<int>(&QSpinBox::valueChanged),
            this, &TxCfcDialog::onFreqSpinChanged);
    connect(m_precompSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TxCfcDialog::onPrecompSpinChanged);
    connect(m_compSpin,    qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TxCfcDialog::onCompSpinChanged);
    connect(m_compQSpin,   qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TxCfcDialog::onCompQSpinChanged);

    // ── Middle edit row ──────────────────────────────────────────────────
    connect(m_postEqGainSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TxCfcDialog::onPostEqGainSpinChanged);
    connect(m_gainSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TxCfcDialog::onGainSpinChanged);
    connect(m_eqQSpin,  qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TxCfcDialog::onEqQSpinChanged);

    // ── Comp widget events ───────────────────────────────────────────────
    connect(m_compWidget, &ParametricEqWidget::pointsChanged,
            this, &TxCfcDialog::onCompPointsChanged);
    connect(m_compWidget, &ParametricEqWidget::globalGainChanged,
            this, &TxCfcDialog::onCompGlobalGainChanged);
    connect(m_compWidget, &ParametricEqWidget::pointDataChanged,
            this, &TxCfcDialog::onCompPointDataChanged);
    connect(m_compWidget, &ParametricEqWidget::pointSelected,
            this, &TxCfcDialog::onCompPointSelected);
    connect(m_compWidget, &ParametricEqWidget::pointUnselected,
            this, &TxCfcDialog::onCompPointUnselected);

    // ── Post-EQ widget events ────────────────────────────────────────────
    connect(m_postEqWidget, &ParametricEqWidget::pointsChanged,
            this, &TxCfcDialog::onEqPointsChanged);
    connect(m_postEqWidget, &ParametricEqWidget::globalGainChanged,
            this, &TxCfcDialog::onEqGlobalGainChanged);
    connect(m_postEqWidget, &ParametricEqWidget::pointDataChanged,
            this, &TxCfcDialog::onEqPointDataChanged);
    connect(m_postEqWidget, &ParametricEqWidget::pointSelected,
            this, &TxCfcDialog::onEqPointSelected);
    connect(m_postEqWidget, &ParametricEqWidget::pointUnselected,
            this, &TxCfcDialog::onEqPointUnselected);

    // ── TM → UI sync ─────────────────────────────────────────────────────
    if (m_tm) {
        connect(m_tm.data(), &TransmitModel::cfcPrecompDbChanged,
                this, &TxCfcDialog::syncFromModel);
        connect(m_tm.data(), &TransmitModel::cfcPostEqGainDbChanged,
                this, &TxCfcDialog::syncFromModel);
        connect(m_tm.data(), &TransmitModel::cfcEqFreqChanged,
                this, &TxCfcDialog::syncFromModel);
        connect(m_tm.data(), &TransmitModel::cfcCompressionChanged,
                this, &TxCfcDialog::syncFromModel);
        connect(m_tm.data(), &TransmitModel::cfcPostEqBandGainChanged,
                this, &TxCfcDialog::syncFromModel);
    }
}

// ─────────────────────────────────────────────────────────────────────
// Initial seed: copy TransmitModel CFC arrays + globals into both widgets.
// Q factors default to widget defaults (4.0) since TM doesn't store Q.
// ─────────────────────────────────────────────────────────────────────

void TxCfcDialog::seedWidgetsFromTransmitModel()
{
    if (!m_tm) { return; }

    // Pull TM defaults — TransmitModel.h:1337 [v2.10.3.13] defaults:
    //   m_cfcEqFreqHz       = {0, 125, 250, 500, 1000, 2000, 3000, 4000, 5000, 10000};
    //   m_cfcCompressionDb  = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
    //   m_cfcPostEqBandGainDb = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    QVector<double> freqs(10);
    QVector<double> compGains(10);
    QVector<double> compQ(10, 4.0);
    QVector<double> eqGains(10);
    QVector<double> eqQ(10, 4.0);
    double maxFreqInTm = 0.0;
    double minFreqInTm = static_cast<double>(kFreqHzMax);
    for (int i = 0; i < 10; ++i) {
        const double f = static_cast<double>(m_tm->cfcEqFreq(i));
        freqs[i]     = f;
        compGains[i] = static_cast<double>(m_tm->cfcCompression(i));
        eqGains[i]   = static_cast<double>(m_tm->cfcPostEqBandGain(i));
        if (f > maxFreqInTm) { maxFreqInTm = f; }
        if (f < minFreqInTm) { minFreqInTm = f; }
    }

    // Widen the freq envelope so the highest TM freq fits.  Thetis's
    // dialog default is 0..4000 Hz (frmCFCConfig.cs:89-99 [v2.10.3.13])
    // but the TXProfile defaults extend to 10000 Hz — Thetis only honors
    // those when a saved profile loads via ConfigData (cs:509-575).  For
    // NereusSDR the TM IS the source of truth on construction so we
    // expand the envelope to cover whatever's in TM, then push the
    // resulting min/max into the Low/High spinboxes.
    const double seedMinHz = std::min(kDefaultMinHz, minFreqInTm);
    const double seedMaxHz = std::max(kDefaultMaxHz, maxFreqInTm);

    QSignalBlocker bComp(m_compWidget);
    QSignalBlocker bEq(m_postEqWidget);
    QSignalBlocker bLow(m_lowSpin);
    QSignalBlocker bHigh(m_highSpin);

    m_compWidget->setBandCount(10);
    m_compWidget->setFrequencyMinHz(seedMinHz);
    m_compWidget->setFrequencyMaxHz(seedMaxHz);
    m_compWidget->setPointsData(freqs, compGains, compQ);
    m_compWidget->setGlobalGainDb(static_cast<double>(m_tm->cfcPrecompDb()));

    m_postEqWidget->setBandCount(10);
    m_postEqWidget->setFrequencyMinHz(seedMinHz);
    m_postEqWidget->setFrequencyMaxHz(seedMaxHz);
    m_postEqWidget->setPointsData(freqs, eqGains, eqQ);
    m_postEqWidget->setGlobalGainDb(static_cast<double>(m_tm->cfcPostEqGainDb()));

    // Sync the Low/High spinboxes to match the seeded envelope.
    m_lowSpin->setValue(static_cast<int>(seedMinHz));
    m_highSpin->setValue(static_cast<int>(seedMaxHz));
}

// ─────────────────────────────────────────────────────────────────────
// Selected-index helpers — From Thetis frmCFCConfig.cs:307-315 [v2.10.3.13].
// ─────────────────────────────────────────────────────────────────────

int TxCfcDialog::selectedIndex() const
{
    if (m_compWidget && m_compWidget->selectedIndex() != -1) {
        return m_compWidget->selectedIndex();
    }
    if (m_postEqWidget && m_postEqWidget->selectedIndex() != -1) {
        return m_postEqWidget->selectedIndex();
    }
    return -1;
}

// From Thetis frmCFCConfig.cs:316-332 [v2.10.3.13] — updateSelected.
// Enables/disables the per-band edit controls based on whether a band is
// currently selected.
void TxCfcDialog::updateSelectedRowEnable()
{
    const bool enable = (selectedIndex() != -1);
    m_freqSpin->setEnabled(enable);
    m_compSpin->setEnabled(enable);
    m_compQSpin->setEnabled(enable);
    m_gainSpin->setEnabled(enable);
    m_eqQSpin->setEnabled(enable);
    // Pre-Comp / Post-EQ Gain are global (always enabled).
}

// Pull the selected band's freq/gain/q values into the edit-row spinboxes.
// Echo-guarded via m_ignoreUpdates (mirrors Thetis _ignore_udpates).
void TxCfcDialog::updateEditRowFromSelection(int index)
{
    if (!m_compWidget || !m_postEqWidget) { return; }
    if (index < 0 || index >= m_compWidget->points().size() ||
                     index >= m_postEqWidget->points().size()) {
        return;
    }

    double cf = 0.0, cg = 0.0, cq = 0.0;
    double ef = 0.0, eg = 0.0, eq = 0.0;
    m_compWidget->getPointData(index, cf, cg, cq);
    m_postEqWidget->getPointData(index, ef, eg, eq);

    m_ignoreUpdates = true;
    {
        QSignalBlocker b(m_selectedBandSpin);
        m_selectedBandSpin->setValue(index + 1);
    }
    {
        QSignalBlocker b(m_freqSpin);
        m_freqSpin->setValue(static_cast<int>(std::round(cf)));
    }
    {
        QSignalBlocker b(m_compSpin);
        m_compSpin->setValue(cg);
    }
    {
        QSignalBlocker b(m_compQSpin);
        m_compQSpin->setValue(cq);
    }
    {
        QSignalBlocker b(m_gainSpin);
        m_gainSpin->setValue(eg);
    }
    {
        QSignalBlocker b(m_eqQSpin);
        m_eqQSpin->setValue(eq);
    }
    m_ignoreUpdates = false;
}

// ─────────────────────────────────────────────────────────────────────
// Push current widget state back into TransmitModel.  This is the
// NereusSDR equivalent of Thetis's setCFCProfile WDSP-direct push:
// frmCFCConfig.cs:333-392 [v2.10.3.13] writes through to WDSP, but
// NereusSDR routes everything through TransmitModel which dispatches
// to TxChannel via its existing per-property change handlers (wired in
// 3M-3a-ii Batch 2 via RadioModel).
// ─────────────────────────────────────────────────────────────────────

void TxCfcDialog::pushCfcProfileToModel()
{
    if (!m_tm || !m_compWidget || !m_postEqWidget) { return; }

    QVector<double> cf, cg, cq, ef, eg, eq;
    m_compWidget->getPointsData(cf, cg, cq);
    m_postEqWidget->getPointsData(ef, eg, eq);

    if (cf.size() != 10 || ef.size() != 10) {
        // Non-10-band layouts (5-band / 18-band) don't fit TM's fixed
        // 10-element arrays.  Profile push for those layouts is gated
        // until the TM array width grows (separate follow-up; matches
        // Thetis radCFC_5/18 which still calls setCFCProfile but has
        // the variable-length WDSP API).  For now we just sync the
        // GlobalGainDb scalars and skip the per-band push.
        m_updatingFromModel = true;
        m_tm->setCfcPrecompDb(static_cast<int>(std::round(m_compWidget->globalGainDb())));
        m_tm->setCfcPostEqGainDb(static_cast<int>(std::round(m_postEqWidget->globalGainDb())));
        m_updatingFromModel = false;
        return;
    }

    m_updatingFromModel = true;
    m_tm->setCfcPrecompDb(static_cast<int>(std::round(m_compWidget->globalGainDb())));
    m_tm->setCfcPostEqGainDb(static_cast<int>(std::round(m_postEqWidget->globalGainDb())));
    for (int i = 0; i < 10; ++i) {
        m_tm->setCfcEqFreq        (i, static_cast<int>(std::round(cf[i])));
        m_tm->setCfcCompression   (i, static_cast<int>(std::round(cg[i])));
        m_tm->setCfcPostEqBandGain(i, static_cast<int>(std::round(eg[i])));
    }
    m_updatingFromModel = false;
}

// ─────────────────────────────────────────────────────────────────────
// Right-column control slots
// ─────────────────────────────────────────────────────────────────────

// From Thetis frmCFCConfig.cs:108-118 [v2.10.3.13] — radCFC_bands_CheckedChanged.
void TxCfcDialog::onBandCountChanged()
{
    int bands = currentBandCount();

    m_selectedBandSpin->setMaximum(bands);

    QSignalBlocker bComp(m_compWidget);
    QSignalBlocker bEq(m_postEqWidget);
    m_compWidget->setBandCount(bands);
    m_postEqWidget->setBandCount(bands);

    updateSelectedRowEnable();
}

// From Thetis frmCFCConfig.cs:120-129 [v2.10.3.13] — udCFC_low_ValueChanged.
void TxCfcDialog::onLowFreqChanged(int hz)
{
    if (hz > m_highSpin->value() - kMinFreqSpreadHz) {
        QSignalBlocker b(m_lowSpin);
        m_lowSpin->setValue(m_highSpin->value() - kMinFreqSpreadHz);
        return;
    }
    m_compWidget->setFrequencyMinHz  (static_cast<double>(hz));
    m_postEqWidget->setFrequencyMinHz(static_cast<double>(hz));
}

// From Thetis frmCFCConfig.cs:131-140 [v2.10.3.13] — udCFC_high_ValueChanged.
void TxCfcDialog::onHighFreqChanged(int hz)
{
    if (hz < m_lowSpin->value() + kMinFreqSpreadHz) {
        QSignalBlocker b(m_highSpin);
        m_highSpin->setValue(m_lowSpin->value() + kMinFreqSpreadHz);
        return;
    }
    m_compWidget->setFrequencyMaxHz  (static_cast<double>(hz));
    m_postEqWidget->setFrequencyMaxHz(static_cast<double>(hz));
}

int TxCfcDialog::currentBandCount() const
{
    if (m_bands5Radio  && m_bands5Radio->isChecked())  return 5;
    if (m_bands18Radio && m_bands18Radio->isChecked()) return 18;
    return 10;
}

// From Thetis frmCFCConfig.cs:484-490 [v2.10.3.13] — chkCFC_UseQFactors.
void TxCfcDialog::onUseQFactorsToggled(bool on)
{
    m_compWidget->setParametricEq(on);
    m_postEqWidget->setParametricEq(on);
    pushCfcProfileToModel();
}

// From Thetis frmCFCConfig.cs:603-607 [v2.10.3.13] — chkLogScale.
void TxCfcDialog::onLogScaleToggled(bool on)
{
    m_compWidget->setLogScale(on);
    m_postEqWidget->setLogScale(on);
}

// From Thetis frmCFCConfig.cs:451-456 [v2.10.3.13] — btnResetComp_Click.
//
// Note: NereusSDR's ParametricEqWidget does not expose a public ResetPoints()
// (Thetis ucParametricEq.cs:1041-1046).  The same effect is achieved by
// re-seeding via setPointsData with the default flat profile (gain=0, q=4)
// across the current frequency span.  See Task 5 review for the
// intentional decision to keep resetPointsDefault private.
void TxCfcDialog::onResetCompClicked()
{
    if (!m_compWidget) { return; }
    const int bands = currentBandCount();
    QVector<double> f(bands), g(bands, 0.0), q(bands, 4.0);
    const double minHz = m_compWidget->frequencyMinHz();
    const double maxHz = m_compWidget->frequencyMaxHz();
    const double span = (maxHz > minHz) ? (maxHz - minHz) : 1.0;
    for (int i = 0; i < bands; ++i) {
        const double t = (bands > 1) ? double(i) / double(bands - 1) : 0.0;
        f[i] = minHz + t * span;
    }
    {
        QSignalBlocker b(m_compWidget);
        m_compWidget->setSelectedIndex(-1);
        m_compWidget->setGlobalGainDb(0.0);
        m_compWidget->setPointsData(f, g, q);
    }
    pushCfcProfileToModel();
    syncFromModel();
    updateSelectedRowEnable();
}

// From Thetis frmCFCConfig.cs:458-463 [v2.10.3.13] — btnResetEQ_Click.
void TxCfcDialog::onResetEqClicked()
{
    if (!m_postEqWidget) { return; }
    const int bands = currentBandCount();
    QVector<double> f(bands), g(bands, 0.0), q(bands, 4.0);
    const double minHz = m_postEqWidget->frequencyMinHz();
    const double maxHz = m_postEqWidget->frequencyMaxHz();
    const double span = (maxHz > minHz) ? (maxHz - minHz) : 1.0;
    for (int i = 0; i < bands; ++i) {
        const double t = (bands > 1) ? double(i) / double(bands - 1) : 0.0;
        f[i] = minHz + t * span;
    }
    {
        QSignalBlocker b(m_postEqWidget);
        m_postEqWidget->setSelectedIndex(-1);
        m_postEqWidget->setGlobalGainDb(0.0);
        m_postEqWidget->setPointsData(f, g, q);
    }
    pushCfcProfileToModel();
    syncFromModel();
    updateSelectedRowEnable();
}

// From Thetis frmCFCConfig.cs:598-601 [v2.10.3.13] — lblOGGuide_LinkClicked.
void TxCfcDialog::onOgGuideClicked()
{
    QDesktopServices::openUrl(QUrl(QString::fromUtf8(kOgGuideUrl)));
}

// ─────────────────────────────────────────────────────────────────────
// Top edit row slots
// ─────────────────────────────────────────────────────────────────────

// From Thetis frmCFCConfig.cs:465-475 [v2.10.3.13] — nudCFC_selected_band_ValueChanged.
void TxCfcDialog::onSelectedBandChanged(int oneBased)
{
    if (m_ignoreUpdates) return;

    m_ignoreUnselected = true;
    {
        QSignalBlocker bComp(m_compWidget);
        QSignalBlocker bEq(m_postEqWidget);
        m_compWidget->setSelectedIndex(oneBased - 1);
        m_postEqWidget->setSelectedIndex(m_compWidget->selectedIndex());
    }
    m_ignoreUnselected = false;

    updateEditRowFromSelection(m_compWidget->selectedIndex());
    updateSelectedRowEnable();
}

// From Thetis frmCFCConfig.cs:142-150 [v2.10.3.13] — nudCFC_f_ValueChanged.
void TxCfcDialog::onFreqSpinChanged(int hz)
{
    if (m_ignoreUpdates) return;
    if (!m_compWidget) return;

    const int index = m_compWidget->selectedIndex();
    if (index < 0) return;

    double f = 0.0, g = 0.0, q = 0.0;
    m_compWidget->getPointData(index, f, g, q);
    f = static_cast<double>(hz);
    {
        QSignalBlocker b(m_compWidget);
        m_compWidget->setPointData(index, f, g, q);
    }
    // Cross-sync: the post-EQ widget shares per-band frequency.
    if (m_postEqWidget && index < m_postEqWidget->points().size()) {
        const int bandId = m_compWidget->points().at(index).bandId;
        QSignalBlocker b(m_postEqWidget);
        m_postEqWidget->setPointHz(bandId, f, false);
    }
    pushCfcProfileToModel();
}

// From Thetis frmCFCConfig.cs:152-157 [v2.10.3.13] — nudCFC_precomp_ValueChanged.
void TxCfcDialog::onPrecompSpinChanged(double db)
{
    if (m_ignoreUpdates) return;
    if (!m_compWidget) return;
    {
        QSignalBlocker b(m_compWidget);
        m_compWidget->setGlobalGainDb(db);
    }
    if (m_tm && !m_updatingFromModel) {
        m_updatingFromModel = true;
        m_tm->setCfcPrecompDb(static_cast<int>(std::round(db)));
        m_updatingFromModel = false;
    }
}

// From Thetis frmCFCConfig.cs:159-167 [v2.10.3.13] — nudCFC_c_ValueChanged.
void TxCfcDialog::onCompSpinChanged(double db)
{
    if (m_ignoreUpdates) return;
    if (!m_compWidget) return;

    const int index = m_compWidget->selectedIndex();
    if (index < 0) return;

    double f = 0.0, g = 0.0, q = 0.0;
    m_compWidget->getPointData(index, f, g, q);
    g = db;
    {
        QSignalBlocker b(m_compWidget);
        m_compWidget->setPointData(index, f, g, q);
    }
    if (m_tm && !m_updatingFromModel) {
        m_updatingFromModel = true;
        m_tm->setCfcCompression(index, static_cast<int>(std::round(g)));
        m_updatingFromModel = false;
    }
}

// From Thetis frmCFCConfig.cs:196-204 [v2.10.3.13] — nudCFC_cq_ValueChanged.
void TxCfcDialog::onCompQSpinChanged(double q)
{
    if (m_ignoreUpdates) return;
    if (!m_compWidget) return;

    const int index = m_compWidget->selectedIndex();
    if (index < 0) return;

    double f = 0.0, g = 0.0, oldQ = 0.0;
    m_compWidget->getPointData(index, f, g, oldQ);
    {
        QSignalBlocker b(m_compWidget);
        m_compWidget->setPointData(index, f, g, q);
    }
    pushCfcProfileToModel();
}

// ─────────────────────────────────────────────────────────────────────
// Middle edit row slots
// ─────────────────────────────────────────────────────────────────────

// From Thetis frmCFCConfig.cs:169-174 [v2.10.3.13] — nudCFC_posteqgain_ValueChanged.
void TxCfcDialog::onPostEqGainSpinChanged(double db)
{
    if (m_ignoreUpdates) return;
    if (!m_postEqWidget) return;
    {
        QSignalBlocker b(m_postEqWidget);
        m_postEqWidget->setGlobalGainDb(db);
    }
    if (m_tm && !m_updatingFromModel) {
        m_updatingFromModel = true;
        m_tm->setCfcPostEqGainDb(static_cast<int>(std::round(db)));
        m_updatingFromModel = false;
    }
}

// From Thetis frmCFCConfig.cs:176-184 [v2.10.3.13] — nudCFC_gain_ValueChanged.
void TxCfcDialog::onGainSpinChanged(double db)
{
    if (m_ignoreUpdates) return;
    if (!m_postEqWidget) return;

    const int index = m_postEqWidget->selectedIndex();
    if (index < 0) return;

    double f = 0.0, g = 0.0, q = 0.0;
    m_postEqWidget->getPointData(index, f, g, q);
    g = db;
    {
        QSignalBlocker b(m_postEqWidget);
        m_postEqWidget->setPointData(index, f, g, q);
    }
    if (m_tm && !m_updatingFromModel) {
        m_updatingFromModel = true;
        m_tm->setCfcPostEqBandGain(index, static_cast<int>(std::round(g)));
        m_updatingFromModel = false;
    }
}

// From Thetis frmCFCConfig.cs:186-194 [v2.10.3.13] — nudCFC_q_ValueChanged.
void TxCfcDialog::onEqQSpinChanged(double q)
{
    if (m_ignoreUpdates) return;
    if (!m_postEqWidget) return;

    const int index = m_postEqWidget->selectedIndex();
    if (index < 0) return;

    double f = 0.0, g = 0.0, oldQ = 0.0;
    m_postEqWidget->getPointData(index, f, g, oldQ);
    {
        QSignalBlocker b(m_postEqWidget);
        m_postEqWidget->setPointData(index, f, g, q);
    }
    pushCfcProfileToModel();
}

// ─────────────────────────────────────────────────────────────────────
// Comp widget event handlers
// ─────────────────────────────────────────────────────────────────────

// From Thetis frmCFCConfig.cs:234-238 [v2.10.3.13] — ucCFC_comp_PointsChanged.
void TxCfcDialog::onCompPointsChanged(bool isDragging)
{
    if (isDragging) return;
    pushCfcProfileToModel();
}

// From Thetis frmCFCConfig.cs:206-216 [v2.10.3.13] — ucCFC_comp_GlobalGainChanged.
void TxCfcDialog::onCompGlobalGainChanged(bool isDragging)
{
    const bool live = m_liveUpdateChk && m_liveUpdateChk->isChecked();
    if (!isDragging || live) {
        pushCfcProfileToModel();
    } else {
        // just_text: don't push to WDSP, just update edit-row text.
        // Mirror Thetis's setCFCProfile(-1, true) — UI text update only.
        m_ignoreUpdates = true;
        {
            QSignalBlocker b(m_precompSpin);
            m_precompSpin->setValue(m_compWidget->globalGainDb());
        }
        m_ignoreUpdates = false;
    }
}

// From Thetis frmCFCConfig.cs:218-232 [v2.10.3.13] — ucCFC_comp_PointDataChanged.
void TxCfcDialog::onCompPointDataChanged(int index, int bandId,
                                          double frequencyHz,
                                          double /*gainDb*/, double /*q*/,
                                          bool isDragging)
{
    // Cross-sync: push the same band's frequency to the post-EQ widget +
    // mirror the selection.
    if (m_postEqWidget) {
        QSignalBlocker b(m_postEqWidget);
        m_postEqWidget->setPointHz(bandId, frequencyHz, isDragging);
        const int eqIndex = m_postEqWidget->getIndexFromBandId(bandId);
        m_postEqWidget->setSelectedIndex(eqIndex);
    }

    // Update the edit-row spinboxes from the new point data.
    updateEditRowFromSelection(index);

    const bool live = m_liveUpdateChk && m_liveUpdateChk->isChecked();
    if (!isDragging || live) {
        pushCfcProfileToModel();
    }
    // (drag-without-live: just update text, defer push to release.)
}

// From Thetis frmCFCConfig.cs:240-246 [v2.10.3.13] — ucCFC_comp_PointSelected.
void TxCfcDialog::onCompPointSelected(int index, int bandId,
                                       double /*frequencyHz*/,
                                       double /*gainDb*/, double /*q*/)
{
    if (m_postEqWidget) {
        QSignalBlocker b(m_postEqWidget);
        const int eqIndex = m_postEqWidget->getIndexFromBandId(bandId);
        m_postEqWidget->setSelectedIndex(eqIndex);
    }
    updateEditRowFromSelection(index);
    updateSelectedRowEnable();
}

// From Thetis frmCFCConfig.cs:248-255 [v2.10.3.13] — ucCFC_comp_PointUnselected.
void TxCfcDialog::onCompPointUnselected(int /*index*/, int /*bandId*/,
                                         double /*frequencyHz*/,
                                         double /*gainDb*/, double /*q*/)
{
    if (m_ignoreUnselected) return;

    if (m_postEqWidget) {
        QSignalBlocker b(m_postEqWidget);
        m_postEqWidget->setSelectedIndex(-1);
    }
    updateSelectedRowEnable();
}

// ─────────────────────────────────────────────────────────────────────
// Post-EQ widget event handlers (mirror comp handlers)
// ─────────────────────────────────────────────────────────────────────

// From Thetis frmCFCConfig.cs:285-289 [v2.10.3.13] — ucCFC_eq_PointsChanged.
void TxCfcDialog::onEqPointsChanged(bool isDragging)
{
    if (isDragging) return;
    pushCfcProfileToModel();
}

// From Thetis frmCFCConfig.cs:257-267 [v2.10.3.13] — ucCFC_eq_GlobalGainChanged.
void TxCfcDialog::onEqGlobalGainChanged(bool isDragging)
{
    const bool live = m_liveUpdateChk && m_liveUpdateChk->isChecked();
    if (!isDragging || live) {
        pushCfcProfileToModel();
    } else {
        m_ignoreUpdates = true;
        {
            QSignalBlocker b(m_postEqGainSpin);
            m_postEqGainSpin->setValue(m_postEqWidget->globalGainDb());
        }
        m_ignoreUpdates = false;
    }
}

// From Thetis frmCFCConfig.cs:269-283 [v2.10.3.13] — ucCFC_eq_PointDataChanged.
void TxCfcDialog::onEqPointDataChanged(int index, int bandId,
                                        double frequencyHz,
                                        double /*gainDb*/, double /*q*/,
                                        bool isDragging)
{
    if (m_compWidget) {
        QSignalBlocker b(m_compWidget);
        m_compWidget->setPointHz(bandId, frequencyHz, isDragging);
        const int compIndex = m_compWidget->getIndexFromBandId(bandId);
        m_compWidget->setSelectedIndex(compIndex);
    }

    updateEditRowFromSelection(index);

    const bool live = m_liveUpdateChk && m_liveUpdateChk->isChecked();
    if (!isDragging || live) {
        pushCfcProfileToModel();
    }
}

// From Thetis frmCFCConfig.cs:291-297 [v2.10.3.13] — ucCFC_eq_PointSelected.
void TxCfcDialog::onEqPointSelected(int index, int bandId,
                                     double /*frequencyHz*/,
                                     double /*gainDb*/, double /*q*/)
{
    if (m_compWidget) {
        QSignalBlocker b(m_compWidget);
        const int compIndex = m_compWidget->getIndexFromBandId(bandId);
        m_compWidget->setSelectedIndex(compIndex);
    }
    updateEditRowFromSelection(index);
    updateSelectedRowEnable();
}

// From Thetis frmCFCConfig.cs:299-306 [v2.10.3.13] — ucCFC_eq_PointUnselected.
void TxCfcDialog::onEqPointUnselected(int /*index*/, int /*bandId*/,
                                       double /*frequencyHz*/,
                                       double /*gainDb*/, double /*q*/)
{
    if (m_ignoreUnselected) return;

    if (m_compWidget) {
        QSignalBlocker b(m_compWidget);
        m_compWidget->setSelectedIndex(-1);
    }
    updateSelectedRowEnable();
}

// ─────────────────────────────────────────────────────────────────────
// Model → UI sync (echo-guarded)
// ─────────────────────────────────────────────────────────────────────

void TxCfcDialog::syncFromModel()
{
    if (!m_tm || !m_compWidget || !m_postEqWidget) { return; }

    // Avoid re-entrant model writes during an in-flight pushCfcProfileToModel.
    if (m_updatingFromModel) { return; }

    m_updatingFromModel = true;

    // Pre-comp + post-EQ gain scalars → top edit row + widgets.
    {
        QSignalBlocker b(m_precompSpin);
        m_precompSpin->setValue(static_cast<double>(m_tm->cfcPrecompDb()));
    }
    {
        QSignalBlocker b(m_postEqGainSpin);
        m_postEqGainSpin->setValue(static_cast<double>(m_tm->cfcPostEqGainDb()));
    }

    // Per-band freq / comp / post-EQ → both widgets (preserving current Q).
    // We can only push to widgets that have the matching band count — TM
    // always carries 10 entries, so the 10-band layout round-trips natively;
    // 5/18 layouts are handled by user reset (band-radio change reseeds via
    // setBandCount, which calls resetPointsDefault internally).
    if (m_compWidget->bandCount() == 10 && m_postEqWidget->bandCount() == 10) {
        // Find the max TM freq so we can widen the envelope if needed
        // (mirrors seedWidgetsFromTransmitModel logic).
        double maxFreqInTm = 0.0;
        for (int i = 0; i < 10; ++i) {
            const double f = static_cast<double>(m_tm->cfcEqFreq(i));
            if (f > maxFreqInTm) { maxFreqInTm = f; }
        }
        if (maxFreqInTm > m_compWidget->frequencyMaxHz()) {
            QSignalBlocker bComp(m_compWidget);
            QSignalBlocker bEq(m_postEqWidget);
            QSignalBlocker bHigh(m_highSpin);
            m_compWidget->setFrequencyMaxHz(maxFreqInTm);
            m_postEqWidget->setFrequencyMaxHz(maxFreqInTm);
            m_highSpin->setValue(static_cast<int>(maxFreqInTm));
        }
    }
    if (m_compWidget->bandCount() == 10) {
        QVector<double> cf(10), cg(10), cq(10);
        m_compWidget->getPointsData(cf, cg, cq);
        for (int i = 0; i < 10; ++i) {
            cf[i] = static_cast<double>(m_tm->cfcEqFreq(i));
            cg[i] = static_cast<double>(m_tm->cfcCompression(i));
        }
        QSignalBlocker b(m_compWidget);
        m_compWidget->setPointsData(cf, cg, cq);
        m_compWidget->setGlobalGainDb(static_cast<double>(m_tm->cfcPrecompDb()));
    }
    if (m_postEqWidget->bandCount() == 10) {
        QVector<double> ef(10), eg(10), eq(10);
        m_postEqWidget->getPointsData(ef, eg, eq);
        for (int i = 0; i < 10; ++i) {
            ef[i] = static_cast<double>(m_tm->cfcEqFreq(i));
            eg[i] = static_cast<double>(m_tm->cfcPostEqBandGain(i));
        }
        QSignalBlocker b(m_postEqWidget);
        m_postEqWidget->setPointsData(ef, eg, eq);
        m_postEqWidget->setGlobalGainDb(static_cast<double>(m_tm->cfcPostEqGainDb()));
    }

    // If a band is currently selected, refresh its edit-row spinboxes.
    if (selectedIndex() != -1) {
        m_updatingFromModel = false;
        updateEditRowFromSelection(selectedIndex());
        return;
    }

    m_updatingFromModel = false;
}

// ─────────────────────────────────────────────────────────────────────
// 50 ms bar chart timer
//
// From Thetis frmCFCConfig.cs:394-431 [v2.10.3.13] — timerTick.
//
// Bin → bar mapping:
//   binsPerHz = kCfcDisplayBinCount / 48000
//   startIdx  = (int)(FrequencyMinHz * binsPerHz)
//   endIdx    = (int)(FrequencyMaxHz * binsPerHz)
//   slice     = bins[startIdx..endIdx]  (1 sample per bin)
//   widget->drawBarChartData(slice)
//
// The widget renders one bar per data point; the comp widget's internal
// hit-test maps mouse-x to data-index via the same FrequencyMin/Max axis,
// so each bar lines up with its corresponding frequency on the curve.
// ─────────────────────────────────────────────────────────────────────

void TxCfcDialog::onBarChartTick()
{
    if (m_barChartBusy) return;
    if (!m_tx || !m_compWidget) return;

    m_barChartBusy = true;

    static double bins[TxChannel::kCfcDisplayBinCount] = {};
    const bool ready = m_tx->getCfcDisplayCompression(
        bins, TxChannel::kCfcDisplayBinCount);

    if (ready) {
        const double startHz = m_compWidget->frequencyMinHz();
        const double stopHz  = m_compWidget->frequencyMaxHz();
        const double binsPerHz = static_cast<double>(TxChannel::kCfcDisplayBinCount) /
                                 TxChannel::kCfcDisplaySampleRateHz;
        int startIdx = static_cast<int>(startHz * binsPerHz);
        int endIdx   = static_cast<int>(stopHz  * binsPerHz);

        // Clamp to valid range.
        if (startIdx < 0) startIdx = 0;
        if (endIdx   < 0) endIdx   = 0;
        if (startIdx >= TxChannel::kCfcDisplayBinCount) {
            startIdx = TxChannel::kCfcDisplayBinCount - 1;
        }
        if (endIdx   >= TxChannel::kCfcDisplayBinCount) {
            endIdx   = TxChannel::kCfcDisplayBinCount - 1;
        }

        const int len = endIdx - startIdx + 1;
        if (len > 0) {
            QVector<double> slice(len);
            for (int i = 0; i < len; ++i) {
                slice[i] = bins[startIdx + i];
            }
            m_compWidget->drawBarChartData(slice);
        }
    }

    m_barChartBusy = false;
}

// ─────────────────────────────────────────────────────────────────────
// Show / hide / close — From Thetis frmCFCConfig.cs:433-449 + 477-482 [v2.10.3.13].
// ─────────────────────────────────────────────────────────────────────

void TxCfcDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (m_barChartTimer && !m_barChartTimer->isActive()) {
        m_barChartTimer->start();
    }
}

void TxCfcDialog::hideEvent(QHideEvent* event)
{
    QDialog::hideEvent(event);
    if (m_barChartTimer) {
        m_barChartTimer->stop();
    }
    // Clear the bar chart so the next show isn't littered with stale data
    // (mirrors Thetis frmCFCConfig.cs:1052-1057 [v2.10.3.13] empty-array
    // path).  drawBarChartData with an empty vector resets the widget's
    // bar chart state.
    if (m_compWidget) {
        m_compWidget->drawBarChartData(QVector<double>{});
    }
}

void TxCfcDialog::closeEvent(QCloseEvent* event)
{
    // From Thetis frmCFCConfig.cs:477-482 [v2.10.3.13] — cancel the close
    // and hide instead.  TxApplet keeps the dialog instance alive for
    // fast re-show.
    event->ignore();
    hide();
}

} // namespace NereusSDR
