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
//   2026-10-02 — Native matched editor and paired typed-profile/session
//                 history by J.J. Boyd (KG4VCF), with OpenAI Codex.
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
#include "gui/widgets/EqEditHistory.h"
#include "models/TransmitModel.h"

#include <QApplication>
#include <QScreen>
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
#include <QDataStream>
#include <QIODevice>
#include <QScrollArea>
#include <QSlider>
#include <QAbstractSpinBox>
#include <QLineEdit>
#include <QKeyEvent>
#include <QScopedValueRollback>
#include <QScopeGuard>

#include <algorithm>
#include <cmath>
#include <vector>
#include <numeric>
#include <memory>

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

    m_history = new EqEditHistory(this);
    buildUi();
    wireSignals();
    seedWidgetsFromTransmitModel();
    rebaseEditHistory();
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
// Native approved editor: matched plots, shared band selector and controls,
// independent width/amount inputs, separate globals and collapsible Advanced.
// Retains the Thetis parameter ranges, editing semantics and measured overlay.
// ─────────────────────────────────────────────────────────────────────

void TxCfcDialog::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(6);
    auto button = [this](const QString& title, const char* name) {
        auto* result = new QPushButton(title, this);
        result->setObjectName(QString::fromLatin1(name));
        result->setAutoDefault(false);
        return result;
    };
    auto* title = new QLabel(tr("CFC compressor · Compression and tone shaping"), this);
    outer->addWidget(title);
    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(new QLabel(tr("Bands"), this));
    m_bandCountGroup = new QButtonGroup(this);
    m_bands5Radio = new QRadioButton(tr("5"), this);
    m_bands10Radio = new QRadioButton(tr("10"), this);
    m_bands18Radio = new QRadioButton(tr("18"), this);
    for (auto* radio : {m_bands5Radio, m_bands10Radio, m_bands18Radio}) {
        const int count = radio == m_bands5Radio ? 5 : radio == m_bands10Radio ? 10 : 18;
        radio->setAccessibleName(tr("%1 CFC bands").arg(count));
        m_bandCountGroup->addButton(radio, count);
        toolbar->addWidget(radio);
    }
    m_bands10Radio->setChecked(true);
    toolbar->addStretch();
    m_undoBtn = button(tr("Undo"), "TxCfcUndo");
    m_redoBtn = button(tr("Redo"), "TxCfcRedo");
    toolbar->addWidget(m_undoBtn);
    toolbar->addWidget(m_redoBtn);
    outer->addLayout(toolbar);
    m_invalidCurveGuidance = new QLabel(tr("The stored legacy frequencies do not span 1000 Hz. Choose another band count and Apply to reset both curves."), this);
    m_invalidCurveGuidance->setObjectName(QStringLiteral("TxCfcInvalidCurve"));
    m_invalidCurveGuidance->setWordWrap(true);
    outer->addWidget(m_invalidCurveGuidance);
    m_invalidCurveGuidance->hide();
    m_countNotice = new QWidget(this);
    auto* notice = new QHBoxLayout(m_countNotice);
    notice->setContentsMargins(0, 0, 0, 0);
    auto* noticeText = new QLabel(tr("Changing band count resets both curves."), m_countNotice);
    noticeText->setWordWrap(true);
    notice->addWidget(noticeText, 1);
    m_applyBandsBtn = button(tr("Apply"), "TxCfcApplyBands");
    m_cancelBandsBtn = button(tr("Cancel"), "TxCfcCancelBands");
    notice->addWidget(m_applyBandsBtn);
    notice->addWidget(m_cancelBandsBtn);
    outer->addWidget(m_countNotice);
    m_countNotice->hide();

    m_compWidget = new ParametricEqWidget(this);
    m_postEqWidget = new ParametricEqWidget(this);
    m_compWidget->setObjectName(QStringLiteral("TxCfcCompWidget"));
    m_postEqWidget->setObjectName(QStringLiteral("TxCfcEqWidget"));
    m_compWidget->setDbMin(kCompDbMin);
    m_compWidget->setDbMax(kCompDbMax);
    m_postEqWidget->setDbMin(kGainDbMin);
    m_postEqWidget->setDbMax(kGainDbMax);
    m_compWidget->setGlobalGainIsHorizLine(true);
    m_compWidget->setShowDotReadingsAsComp(true);
    m_compWidget->setYAxisStepDb(4);
    m_postEqWidget->setYAxisStepDb(12);
    for (auto* graph : {m_compWidget, m_postEqWidget}) {
        graph->setEditorPresentationEnabled(true);
        graph->setParametricEq(false);
        graph->setShowReadout(false);
        graph->setShowDotReadings(false);
        graph->setShowBandShading(true);
        graph->setShowAxisScales(true);
        graph->setMinimumHeight(80);
        graph->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    // Reserve the larger native axis/handle margins on both plots.
    const int left = qCeil(std::max(m_compWidget->plotRect().left(), m_postEqWidget->plotRect().left()));
    const int right = qCeil(std::max(m_compWidget->width() - m_compWidget->plotRect().right(),
                                    m_postEqWidget->width() - m_postEqWidget->plotRect().right()));
    m_compWidget->setMinimumPlotGutters(left, right);
    m_postEqWidget->setMinimumPlotGutters(left, right);
    outer->addWidget(new QLabel(tr("Compression · Configured curve / live measured compression bars"), this));
    outer->addWidget(m_compWidget, 1);
    outer->addWidget(new QLabel(tr("EQ after compression · Combined configured curve"), this));
    outer->addWidget(m_postEqWidget, 1);

    auto* selectorScroll = new QScrollArea(this);
    selectorScroll->setWidgetResizable(false);
    selectorScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Reserve both label lines plus the horizontal scrollbar at small sizes.
    selectorScroll->setFixedHeight(66);
    auto* selectorHost = new QWidget(selectorScroll);
    m_bandSelectors = new QHBoxLayout(selectorHost);
    m_bandSelectors->setContentsMargins(0, 0, 0, 0);
    m_bandSelectors->setSpacing(4);
    selectorScroll->setWidget(selectorHost);
    outer->addWidget(selectorScroll);

    auto* controlsScroll = new QScrollArea(this);
    controlsScroll->setWidgetResizable(true);
    controlsScroll->setMinimumHeight(70);
    // Give the two plots about 180 px each initially. Controls scroll on
    // laptop-height displays and shrink further when the window is smaller.
    controlsScroll->setMaximumHeight(220);
    auto* controls = new QWidget(controlsScroll);
    auto* controlLayout = new QVBoxLayout(controls);
    controlLayout->setContentsMargins(4, 4, 4, 4);
    controlLayout->setSpacing(5);
    controlsScroll->setWidget(controls);
    outer->addWidget(controlsScroll);
    auto spin = [controls](const char* name, double low, double high, int decimals, const QString& label) {
        auto* result = new QDoubleSpinBox(controls);
        result->setObjectName(QString::fromLatin1(name));
        result->setRange(low, high);
        result->setDecimals(decimals);
        result->setSingleStep(decimals == 2 ? .01 : .1);
        result->setKeyboardTracking(false);
        result->setAccessibleName(label);
        return result;
    };
    auto intSpin = [controls](const char* name, int low, int high, const QString& label) {
        auto* result = new QSpinBox(controls);
        result->setObjectName(QString::fromLatin1(name));
        result->setRange(low, high);
        result->setKeyboardTracking(false);
        result->setAccessibleName(label);
        return result;
    };
    auto* selection = new QHBoxLayout;
    m_selectedSummary = new QLabel(tr("Select a band to edit"), controls);
    selection->addWidget(m_selectedSummary, 1);
    auto* bandLabel = new QLabel(tr("Band"), controls);
    m_selectedBandSpin = intSpin("TxCfcSelectedBandSpin", 1, 10, tr("Selected band"));
    bandLabel->setBuddy(m_selectedBandSpin);
    selection->addWidget(bandLabel);
    selection->addWidget(m_selectedBandSpin);
    auto* freqLabel = new QLabel(tr("Frequency"), controls);
    m_freqSpin = intSpin("TxCfcFreqSpin", kFreqHzMin, kFreqHzMax, tr("Shared band frequency in Hz"));
    m_freqSpin->setSuffix(tr(" Hz"));
    freqLabel->setBuddy(m_freqSpin);
    selection->addWidget(freqLabel);
    selection->addWidget(m_freqSpin);
    controlLayout->addLayout(selection);
    m_compSpin = spin("TxCfcCompSpin", kCompDbMin, kCompDbMax, 1, tr("Compression amount in dB"));
    m_compQSpin = spin("TxCfcCompQSpin", kQMin, kQMax, 2, tr("Compression width Q"));
    m_gainSpin = spin("TxCfcGainSpin", kGainDbMin, kGainDbMax, 1, tr("EQ after compression gain in dB"));
    m_eqQSpin = spin("TxCfcEqQSpin", kQMin, kQMax, 2, tr("EQ after compression width Q"));
    m_compSpin->setSuffix(tr(" dB"));
    m_gainSpin->setSuffix(tr(" dB"));
    auto* amounts = new QGridLayout;
    amounts->setColumnStretch(0, 1);
    amounts->setColumnStretch(1, 1);
    for (int col = 0; col < 2; ++col) {
        auto* group = new QWidget(controls);
        auto* grid = new QGridLayout(group);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->addWidget(new QLabel(col == 0 ? tr("Compression") : tr("EQ after compression"), group), 0, 0, 1, 2);
        auto* amount = col == 0 ? m_compSpin : m_gainSpin;
        auto* width = col == 0 ? m_compQSpin : m_eqQSpin;
        auto* amountLabel = new QLabel(col == 0 ? tr("Amount") : tr("Gain"), group);
        auto* widthLabel = new QLabel(tr("Width (Q)"), group);
        amountLabel->setBuddy(amount);
        widthLabel->setBuddy(width);
        grid->addWidget(amountLabel, 1, 0);
        grid->addWidget(widthLabel, 1, 1);
        grid->addWidget(amount, 2, 0);
        grid->addWidget(width, 2, 1);
        auto* slider = new QSlider(Qt::Horizontal, group);
        slider->setRange(0, 1000);
        slider->setStyleSheet(Style::sliderHStyle());
        slider->setAccessibleName(col == 0 ? tr("Compression width, wider to narrower") : tr("EQ width, wider to narrower"));
        slider->setObjectName(col == 0 ? QStringLiteral("TxCfcCompQSlider") : QStringLiteral("TxCfcEqQSlider"));
        if (col == 0) { m_compQSlider = slider; } else { m_eqQSlider = slider; }
        grid->addWidget(slider, 3, 0, 1, 2);
        grid->addWidget(new QLabel(tr("Wider"), group), 4, 0);
        auto* narrower = new QLabel(tr("Narrower"), group);
        narrower->setAlignment(Qt::AlignRight);
        grid->addWidget(narrower, 4, 1);
        amounts->addWidget(group, 0, col);
    }
    controlLayout->addLayout(amounts);
    auto* qRow = new QHBoxLayout;
    m_useQFactorsChk = new QCheckBox(tr("Use Q Factors"), controls);
    qRow->addWidget(m_useQFactorsChk);
    m_qGuidance = new QLabel(tr("Enable Use Q Factors to adjust widths."), controls);
    m_qGuidance->setWordWrap(true);
    qRow->addWidget(m_qGuidance, 1);
    controlLayout->addLayout(qRow);
    auto* guidance = new QLabel(tr("Drag a point: frequency ↔ amount ↕ · Drag square handles: width"), controls);
    guidance->setWordWrap(true);
    controlLayout->addWidget(guidance);
    m_precompSpin = spin("TxCfcPrecompSpin", kPrecompDbMin, kPrecompDbMax, 1, tr("Global pre-compression in dB"));
    m_postEqGainSpin = spin("TxCfcPostEqGainSpin", kPostEqGainDbMin, kPostEqGainDbMax, 1, tr("Global post-EQ gain in dB"));
    m_precompSpin->setSuffix(tr(" dB"));
    m_postEqGainSpin->setSuffix(tr(" dB"));
    m_resetCompBtn = button(tr("Reset Compression"), "TxCfcResetComp");
    m_resetEqBtn = button(tr("Reset EQ"), "TxCfcResetEq");
    auto* globals = new QGridLayout;
    auto* preLabel = new QLabel(tr("Pre-compression"), controls);
    auto* postLabel = new QLabel(tr("Post-EQ gain"), controls);
    preLabel->setBuddy(m_precompSpin);
    postLabel->setBuddy(m_postEqGainSpin);
    globals->addWidget(preLabel, 0, 0);
    globals->addWidget(m_precompSpin, 0, 1);
    globals->addWidget(postLabel, 0, 2);
    globals->addWidget(m_postEqGainSpin, 0, 3);
    globals->addWidget(m_resetCompBtn, 1, 0, 1, 2);
    globals->addWidget(m_resetEqBtn, 1, 2, 1, 2);
    controlLayout->addLayout(globals);
    auto* advancedToggle = button(tr("Advanced ▸"), "TxCfcAdvanced");
    advancedToggle->setCheckable(true);
    controlLayout->addWidget(advancedToggle);
    auto* advanced = new QWidget(controls);
    auto* advancedLayout = new QGridLayout(advanced);
    advancedLayout->setContentsMargins(0, 0, 0, 0);
    m_lowSpin = intSpin("TxCfcLowSpin", kFreqHzMin, kFreqHzMax, tr("Curve range low frequency"));
    m_highSpin = intSpin("TxCfcHighSpin", kFreqHzMin, kFreqHzMax, tr("Curve range high frequency"));
    m_lowSpin->setSuffix(tr(" Hz"));
    m_highSpin->setSuffix(tr(" Hz"));
    advancedLayout->addWidget(new QLabel(tr("Curve range (rescales both curves)"), advanced), 0, 0, 1, 2);
    advancedLayout->addWidget(m_lowSpin, 1, 0);
    advancedLayout->addWidget(m_highSpin, 1, 1);
    m_liveUpdateChk = new QCheckBox(tr("Live Update"), advanced);
    m_logScaleChk = new QCheckBox(tr("Log scale"), advanced);
    advancedLayout->addWidget(m_liveUpdateChk, 2, 0);
    advancedLayout->addWidget(m_logScaleChk, 3, 0);
    m_ogGuideLink = button(tr("OG CFC Guide by W1AEX"), "TxCfcGuide");
    m_ogGuideLink->setCursor(Qt::PointingHandCursor);
    advancedLayout->addWidget(m_ogGuideLink, 3, 1);
    controlLayout->addWidget(advanced);
    advanced->hide();
    connect(advancedToggle, &QPushButton::toggled, this, [advanced, advancedToggle](bool on) {
        advanced->setVisible(on);
        advancedToggle->setText(on ? tr("Advanced ▾") : tr("Advanced ▸"));
    });
    controlLayout->addStretch();
    const QRect available = screen()->availableGeometry();
    resize(std::min(760, available.width() - 40), std::min(730, available.height() - 60));
}

void TxCfcDialog::wireSignals()
{
    connect(m_bandCountGroup, &QButtonGroup::idToggled, this, [this](int, bool checked) {
        if (checked && !m_ignoreUpdates) { onBandCountChanged(); }
    });
    connect(m_applyBandsBtn, &QPushButton::clicked, this, &TxCfcDialog::applyBandCount);
    connect(m_cancelBandsBtn, &QPushButton::clicked, this, &TxCfcDialog::cancelBandCount);
    connect(m_undoBtn, &QPushButton::clicked, this, &TxCfcDialog::undoEdit);
    connect(m_redoBtn, &QPushButton::clicked, this, &TxCfcDialog::redoEdit);
    connect(m_history, &EqEditHistory::availabilityChanged, this, [this](bool undo, bool redo) {
        m_undoBtn->setEnabled(undo);
        m_redoBtn->setEnabled(redo);
    });
    m_undoBtn->setEnabled(false);
    m_redoBtn->setEnabled(false);
    connect(m_lowSpin, qOverload<int>(&QSpinBox::valueChanged), this, &TxCfcDialog::onLowFreqChanged);
    connect(m_highSpin, qOverload<int>(&QSpinBox::valueChanged), this, &TxCfcDialog::onHighFreqChanged);
    connect(m_selectedBandSpin, qOverload<int>(&QSpinBox::valueChanged), this, &TxCfcDialog::onSelectedBandChanged);
    connect(m_freqSpin, qOverload<int>(&QSpinBox::valueChanged), this, &TxCfcDialog::onFreqSpinChanged);
    connect(m_precompSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &TxCfcDialog::onPrecompSpinChanged);
    connect(m_compSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &TxCfcDialog::onCompSpinChanged);
    connect(m_compQSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &TxCfcDialog::onCompQSpinChanged);
    connect(m_postEqGainSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &TxCfcDialog::onPostEqGainSpinChanged);
    connect(m_gainSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &TxCfcDialog::onGainSpinChanged);
    connect(m_eqQSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &TxCfcDialog::onEqQSpinChanged);
    connect(m_useQFactorsChk, &QCheckBox::toggled, this, &TxCfcDialog::onUseQFactorsToggled);
    connect(m_logScaleChk, &QCheckBox::toggled, this, &TxCfcDialog::onLogScaleToggled);
    connect(m_resetCompBtn, &QPushButton::clicked, this, &TxCfcDialog::onResetCompClicked);
    connect(m_resetEqBtn, &QPushButton::clicked, this, &TxCfcDialog::onResetEqClicked);
    connect(m_ogGuideLink, &QPushButton::clicked, this, &TxCfcDialog::onOgGuideClicked);
    for (QAbstractSpinBox* spin : QList<QAbstractSpinBox*>{m_freqSpin, m_lowSpin, m_highSpin,
                                  m_precompSpin, m_postEqGainSpin, m_compSpin, m_compQSpin, m_gainSpin, m_eqQSpin}) {
        spin->installEventFilter(this);
        spin->findChild<QLineEdit*>()->installEventFilter(this);
        connect(spin, &QAbstractSpinBox::editingFinished, this, [this, spin] {
            if (m_numericEditor == spin) { m_numericEditor = nullptr; finishEdit(); }
        });
    }
    for (auto* slider : {m_compQSlider, m_eqQSlider}) {
        slider->installEventFilter(this);
        connect(slider, &QSlider::sliderPressed, this, [this] { beginEdit(); m_sliderActive = true; });
        connect(slider, &QSlider::sliderReleased, this, [this] { m_sliderActive = false; finishEdit(); });
        connect(slider, &QSlider::valueChanged, this, [this, slider](int value) {
            if (m_ignoreUpdates) { return; }
            const double q = kQMin * std::pow(kQMax / kQMin, value / 1000.0);
            auto* widget = slider == m_compQSlider ? m_compWidget : m_postEqWidget;
            const int index = widget->selectedIndex();
            if (index < 0) { return; }
            const auto point = widget->points()[index];
            editSelectedPoint(widget, point.frequencyHz, point.gainDb, q);
        });
    }
    for (auto* widget : {m_compWidget, m_postEqWidget}) {
        connect(widget, &ParametricEqWidget::editStarted, this, [this] { beginEdit(); m_gestureActive = true; });
        connect(widget, &ParametricEqWidget::editFinished, this, [this] { m_gestureActive = false; finishEdit(); });
        connect(widget, &ParametricEqWidget::pointsChanged, this, [this, widget](bool dragging) {
            if (m_ignoreUpdates || m_updatingFromModel) { return; }
            syncPairedFrequencies(widget, widget == m_compWidget ? m_postEqWidget : m_compWidget);
            changed(dragging);
        });
        connect(widget, &ParametricEqWidget::globalGainChanged, this, [this](bool dragging) { changed(dragging); });
        connect(widget, &ParametricEqWidget::pointDataChanged, this, [this](int, int, double, double, double, bool) {
            if (!m_ignoreUpdates) { refreshControls(); }
        });
        connect(widget, &ParametricEqWidget::pointSelected, this, [this, widget](int, int id, double, double, double) {
            auto* target = widget == m_compWidget ? m_postEqWidget : m_compWidget;
            QSignalBlocker blocker(target);
            target->setSelectedIndex(target->getIndexFromBandId(id));
            refreshControls();
        });
        connect(widget, &ParametricEqWidget::pointUnselected, this, [this, widget](int, int, double, double, double) {
            if (m_ignoreUnselected || m_ignoreUpdates) { return; }
            auto* target = widget == m_compWidget ? m_postEqWidget : m_compWidget;
            QSignalBlocker blocker(target);
            target->setSelectedIndex(-1);
            refreshControls();
        });
    }
    if (m_tm) {
        connect(m_tm, &TransmitModel::cfcProfileChanged, this, [this](const CfcProfile&) { syncFromModel(); });
    }
}

CfcProfile TxCfcDialog::captureProfile() const
{
    CfcProfile profile;
    auto capture = [](const ParametricEqWidget* widget, CfcCurveState& curve) {
        widget->getPointsData(curve.frequenciesHz, curve.gainsDb, curve.q);
        curve.globalGainDb = widget->globalGainDb();
        curve.frequencyMinHz = widget->frequencyMinHz();
        curve.frequencyMaxHz = widget->frequencyMaxHz();
    };
    capture(m_compWidget, profile.compression);
    capture(m_postEqWidget, profile.postEq);
    profile.compression.useQ = m_compUseQ;
    profile.postEq.useQ = m_eqUseQ;
    return profile;
}

void TxCfcDialog::restoreProfile(const CfcProfile& profile)
{
    CfcProfile display = profile;
    if (!isValidCfcProfile(display)) {
        // Legacy profiles can store centers out of order. Sort both amounts
        // together for display without writing or moving any configured center.
        QVector<int> order(display.compression.frequenciesHz.size());
        std::iota(order.begin(), order.end(), 0);
        std::stable_sort(order.begin(), order.end(), [&display](int a, int b) {
            return display.compression.frequenciesHz[a] < display.compression.frequenciesHz[b];
        });
        auto sorted = [&order](CfcCurveState& curve) {
            const auto original = curve;
            for (int i = 0; i < order.size(); ++i) {
                curve.frequenciesHz[i] = original.frequenciesHz[order[i]];
                curve.gainsDb[i] = original.gainsDb[order[i]];
                curve.q[i] = original.q[order[i]];
            }
            curve.frequencyMinHz = curve.frequenciesHz.first();
            curve.frequencyMaxHz = curve.frequenciesHz.last();
        };
        sorted(display.compression); sorted(display.postEq);
        if (!isValidCfcProfile(display)) {
            m_curveAvailable = false;
            m_invalidLegacyProfile = profile;
            m_invalidLegacyBlob = m_tm ? m_tm->cfcParaEqData() : QString();
            const QScopedValueRollback<bool> guard(m_ignoreUpdates, true);
            m_compUseQ = profile.compression.useQ;
            m_eqUseQ = profile.postEq.useQ;
            // Hidden repair placeholders use the incoming fallback metadata,
            // never the previous configured curve. Raw legacy data stays in
            // the unavailable-state sidecar until an explicit count repair.
            auto placeholder = [this](ParametricEqWidget* widget, const CfcCurveState& curve) {
                ParametricEqWidget::EqJsonState state;
                state.bandCount = curve.frequenciesHz.size();
                state.frequencyMinHz = curve.frequencyMinHz;
                state.frequencyMaxHz = curve.frequencyMaxHz;
                state.globalGainDb = curve.globalGainDb;
                state.parametricEq = m_compUseQ && m_eqUseQ;
                for (int i = 0; i < state.bandCount; ++i) {
                    const double hz = state.frequencyMinHz + i *
                        (state.frequencyMaxHz - state.frequencyMinHz) / (state.bandCount - 1);
                    state.points.append({i + 1, QColor(), hz, 0.0, 4.0});
                }
                QSignalBlocker blocker(widget);
                widget->setEditorCurveState(state);
                widget->setSelectedIndex(-1);
            };
            placeholder(m_compWidget, profile.compression);
            placeholder(m_postEqWidget, profile.postEq);
            cancelBandCount();
            m_compWidget->hide(); m_postEqWidget->hide();
            m_invalidCurveGuidance->show();
            rebuildBandSelectors();
            refreshControls();
            return;
        }
    }
    m_curveAvailable = true;
    m_compWidget->show(); m_postEqWidget->show();
    m_invalidCurveGuidance->hide();
    const QScopedValueRollback<bool> guard(m_ignoreUpdates, true);
    m_compUseQ = display.compression.useQ;
    m_eqUseQ = display.postEq.useQ;
    auto load = [this](ParametricEqWidget* widget, const CfcCurveState& curve) {
        ParametricEqWidget::EqJsonState state;
        state.bandCount = curve.frequenciesHz.size();
        state.frequencyMinHz = curve.frequencyMinHz;
        state.frequencyMaxHz = curve.frequencyMaxHz;
        state.globalGainDb = curve.globalGainDb;
        state.parametricEq = m_compUseQ && m_eqUseQ;
        for (int i = 0; i < state.bandCount; ++i) {
            state.points.append({i + 1, QColor(), curve.frequenciesHz[i], curve.gainsDb[i], curve.q[i]});
        }
        QSignalBlocker blocker(widget);
        widget->setEditorCurveState(state);
    };
    load(m_compWidget, display.compression);
    load(m_postEqWidget, display.postEq);
    cancelBandCount();
    rebuildBandSelectors();
    refreshControls();
}

QByteArray TxCfcDialog::captureEditState() const
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << m_curveAvailable << m_compWidget->saveEditState() << m_postEqWidget->saveEditState() << m_compUseQ << m_eqUseQ;
    if (!m_curveAvailable) {
        stream << m_invalidLegacyProfile.compression.frequenciesHz << m_invalidLegacyProfile.compression.gainsDb
               << m_invalidLegacyProfile.postEq.gainsDb << m_invalidLegacyProfile.compression.globalGainDb
               << m_invalidLegacyProfile.postEq.globalGainDb << m_invalidLegacyBlob;
    }
    return result;
}

void TxCfcDialog::restoreEditState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_6_0);
    QByteArray comp, eq;
    bool compQ = false, eqQ = false, available = true;
    CfcProfile legacy;
    QString legacyBlob;
    stream >> available >> comp >> eq >> compQ >> eqQ;
    if (!available) {
        stream >> legacy.compression.frequenciesHz >> legacy.compression.gainsDb >> legacy.postEq.gainsDb
               >> legacy.compression.globalGainDb >> legacy.postEq.globalGainDb >> legacyBlob;
    }
    if (stream.status() != QDataStream::Ok || !stream.atEnd()
        || (!available && (legacy.compression.frequenciesHz.size() != 10
            || legacy.compression.gainsDb.size() != 10 || legacy.postEq.gainsDb.size() != 10))) { return; }
    {
        const QScopedValueRollback<bool> guard(m_ignoreUpdates, true);
        QSignalBlocker c(m_compWidget), e(m_postEqWidget);
        // Restore both exact runtime halves before sending the complete profile.
        if (!m_compWidget->restoreEditState(comp) || !m_postEqWidget->restoreEditState(eq)) { return; }
        m_compUseQ = compQ;
        m_eqUseQ = eqQ;
        const int id = m_selectionStates.value(state, -1);
        m_compWidget->setSelectedIndex(m_compWidget->getIndexFromBandId(id));
        m_postEqWidget->setSelectedIndex(m_postEqWidget->getIndexFromBandId(id));
    }
    m_curveAvailable = available;
    if (!available && m_tm) {
        // Narrow legacy exception: an invalid pre-edit profile cannot be sent
        // through the typed validator. Restore its original integers/blob in
        // one balanced aggregate batch, never fabricate a replacement curve.
        const QScopedValueRollback<bool> guard(m_updatingFromModel, true);
        m_tm->beginCfcProfileUpdate();
        {
            const auto batch = qScopeGuard([this] { m_tm->endCfcProfileUpdate(); });
            m_tm->setCfcPrecompDb(qRound(legacy.compression.globalGainDb));
            m_tm->setCfcPostEqGainDb(qRound(legacy.postEq.globalGainDb));
            for (int i = 0; i < 10; ++i) {
                m_tm->setCfcEqFreq(i, qRound(legacy.compression.frequenciesHz[i]));
                m_tm->setCfcCompression(i, qRound(legacy.compression.gainsDb[i]));
                m_tm->setCfcPostEqBandGain(i, qRound(legacy.postEq.gainsDb[i]));
            }
            m_tm->setCfcParaEqData(legacyBlob);
        }
        restoreProfile(m_tm->effectiveCfcProfile());
    } else {
        m_compWidget->show(); m_postEqWidget->show();
        m_invalidCurveGuidance->hide();
    }
    cancelBandCount();
    m_committedEditState = state;
    rebuildBandSelectors();
    refreshControls();
    if (available) { pushCfcProfileToModel(); }
}

void TxCfcDialog::beginEdit()
{
    if (m_ignoreUpdates || m_updatingFromModel) { return; }
    const QByteArray state = captureEditState();
    const int index = selectedIndex();
    m_selectionStates.insert(state, index >= 0 ? m_compWidget->points()[index].bandId : -1);
    m_history->beginEdit(state);
}

void TxCfcDialog::finishEdit()
{
    if (m_ignoreUpdates || m_updatingFromModel) { return; }
    const QByteArray state = captureEditState();
    const int index = selectedIndex();
    m_selectionStates.insert(state, index >= 0 ? m_compWidget->points()[index].bandId : -1);
    const bool edited = state != m_committedEditState;
    m_history->commitEdit(state);
    const auto retained = m_history->retainedStates();
    for (auto it = m_selectionStates.begin(); it != m_selectionStates.end();) {
        if (!retained.contains(it.key())) { it = m_selectionStates.erase(it); } else { ++it; }
    }
    m_committedEditState = state;
    if (edited || m_liveGestureWrote) { pushCfcProfileToModel(); }
    m_liveGestureWrote = false;
    refreshControls();
}

void TxCfcDialog::changed(bool dragging)
{
    if (m_ignoreUpdates || m_updatingFromModel) { return; }
    refreshControls();
    if (m_gestureActive || m_sliderActive || dragging) {
        if (m_liveUpdateChk->isChecked()) { pushCfcProfileToModel(); m_liveGestureWrote = true; }
    } else if (m_numericEditor) {
        pushCfcProfileToModel();
    } else {
        finishEdit();
    }
}

void TxCfcDialog::rebaseEditHistory()
{
    m_gestureActive = false;
    m_sliderActive = false;
    m_numericEditor = nullptr;
    m_liveGestureWrote = false;
    m_history->cancelEdit();
    m_selectionStates.clear();
    m_committedEditState = captureEditState();
    m_history->reset(m_committedEditState);
}

void TxCfcDialog::seedWidgetsFromTransmitModel()
{
    if (m_tm) { restoreProfile(m_tm->effectiveCfcProfile()); }
}

void TxCfcDialog::syncFromModel()
{
    if (!m_tm || m_updatingFromModel) { return; }
    const QScopedValueRollback<bool> guard(m_updatingFromModel, true);
    m_compWidget->cancelEditGesture();
    m_postEqWidget->cancelEditGesture();
    for (auto* slider : {m_compQSlider, m_eqQSlider}) {
        if (slider->isSliderDown()) {
            m_cancelledSlider = slider;
            QSignalBlocker blocker(slider);
            slider->setSliderDown(false);
        }
    }
    rebaseEditHistory();
    restoreProfile(m_tm->effectiveCfcProfile());
    rebaseEditHistory();
}

void TxCfcDialog::pushCfcProfileToModel()
{
    if (!m_tm || m_updatingFromModel || m_ignoreUpdates || !m_curveAvailable) { return; }
    const QScopedValueRollback<bool> guard(m_updatingFromModel, true);
    m_tm->setCfcProfile(captureProfile());
}

void TxCfcDialog::syncPairedFrequencies(ParametricEqWidget* source, ParametricEqWidget* target)
{
    ParametricEqWidget::EqJsonState state;
    state.bandCount = source->bandCount();
    state.frequencyMinHz = source->frequencyMinHz();
    state.frequencyMaxHz = source->frequencyMaxHz();
    state.globalGainDb = target->globalGainDb();
    state.parametricEq = m_compUseQ && m_eqUseQ;
    for (const auto& point : source->points()) {
        const int index = target->getIndexFromBandId(point.bandId);
        if (index < 0) { return; }
        auto partner = target->points()[index];
        partner.frequencyHz = point.frequencyHz;
        state.points.append(partner);
    }
    QSignalBlocker blocker(target);
    target->setEditorCurveState(state);
    const int index = source->selectedIndex();
    target->setSelectedIndex(index >= 0 ? target->getIndexFromBandId(source->points()[index].bandId) : -1);
}

int TxCfcDialog::selectedIndex() const { return m_compWidget->selectedIndex(); }
int TxCfcDialog::currentBandCount() const { return m_compWidget->bandCount(); }

void TxCfcDialog::rebuildBandSelectors()
{
    while (m_bandSelectors->count() > 0) {
        std::unique_ptr<QLayoutItem> item(m_bandSelectors->takeAt(0));
        if (item->widget()) {
            item->widget()->setObjectName(QString());
            item->widget()->hide();
            item->widget()->deleteLater();
        }
    }
    for (int i = 0; i < currentBandCount(); ++i) {
        auto* selector = new QPushButton(this);
        selector->setAutoDefault(false);
        selector->setCheckable(true);
        selector->setStyleSheet(Style::blueCheckedStyle());
        selector->setObjectName(QStringLiteral("TxCfcBand%1").arg(i + 1));
        selector->setMinimumWidth(86);
        m_bandSelectors->addWidget(selector);
        selector->show();
        connect(selector, &QPushButton::clicked, this, [this, selector] {
            onSelectedBandChanged(selector->property("bandId").toInt());
        });
    }
    m_bandSelectors->addStretch();
}

void TxCfcDialog::refreshControls()
{
    const QScopedValueRollback<bool> guard(m_ignoreUpdates, true);
    std::vector<QSignalBlocker> blockers;
    blockers.reserve(14);
    for (QObject* control : QList<QObject*>{m_selectedBandSpin, m_freqSpin, m_compSpin, m_gainSpin,
                            m_compQSpin, m_eqQSpin, m_precompSpin, m_postEqGainSpin, m_lowSpin, m_highSpin,
                            m_useQFactorsChk, m_logScaleChk, m_compQSlider, m_eqQSlider}) { blockers.emplace_back(control); }
    m_selectedBandSpin->setMaximum(currentBandCount());
    m_precompSpin->setValue(m_compWidget->globalGainDb());
    m_postEqGainSpin->setValue(m_postEqWidget->globalGainDb());
    m_lowSpin->setValue(qRound(m_compWidget->frequencyMinHz()));
    m_highSpin->setValue(qRound(m_compWidget->frequencyMaxHz()));
    m_useQFactorsChk->setChecked(m_compUseQ && m_eqUseQ);
    m_logScaleChk->setChecked(m_compWidget->logScale());
    const int index = selectedIndex();
    if (index >= 0) {
        const auto& comp = m_compWidget->points()[index];
        const auto& eq = m_postEqWidget->points()[index];
        m_selectedBandSpin->setValue(comp.bandId);
        m_freqSpin->setValue(qRound(comp.frequencyHz));
        m_compSpin->setValue(comp.gainDb);
        m_gainSpin->setValue(eq.gainDb);
        m_compQSpin->setValue(comp.q);
        m_eqQSpin->setValue(eq.q);
        m_compQSlider->setValue(qRound(1000 * std::log(comp.q / kQMin) / std::log(kQMax / kQMin)));
        m_eqQSlider->setValue(qRound(1000 * std::log(eq.q / kQMin) / std::log(kQMax / kQMin)));
        m_selectedSummary->setText(tr("Band %1 · %2 Hz").arg(comp.bandId).arg(comp.frequencyHz, 0, 'f', 0));
    } else { m_selectedSummary->setText(tr("Select a band to edit")); }

    for (int i = 0; i < currentBandCount() && i < m_bandSelectors->count(); ++i) {
        auto* button = qobject_cast<QPushButton*>(m_bandSelectors->itemAt(i)->widget());
        if (!button) { continue; }
        const double hz = m_curveAvailable ? m_compWidget->points()[i].frequencyHz
                                          : m_invalidLegacyProfile.compression.frequenciesHz[i];
        const int id = m_compWidget->points()[i].bandId;
        button->setText(tr("%1\n%2 Hz").arg(id).arg(hz, 0, 'f', 0));
        button->setObjectName(QStringLiteral("TxCfcBand%1").arg(id));
        button->setProperty("bandId", id);
        button->setToolTip(tr("Select band %1 at %2 Hz").arg(id).arg(hz, 0, 'f', 3));
        button->setAccessibleName(button->toolTip());
        button->setChecked(i == index);
        const auto lines = button->text().split('\n');
        int textWidth = 0;
        for (const auto& line : lines) { textWidth = std::max(textWidth, button->fontMetrics().horizontalAdvance(line)); }
        button->setMinimumWidth(std::max(86, textWidth + 22));
    }
    // Explicit content geometry prevents compression/overlap while a new
    // count's minimum hints propagate through the horizontal scroll area.
    m_bandSelectors->invalidate();
    m_bandSelectors->parentWidget()->setFixedSize(m_bandSelectors->sizeHint());
    m_bandSelectors->activate();
    updateSelectedRowEnable();
}

void TxCfcDialog::updateSelectedRowEnable()
{
    const bool selected = m_curveAvailable && selectedIndex() >= 0;
    m_selectedBandSpin->setEnabled(m_curveAvailable);
    m_precompSpin->setEnabled(m_curveAvailable);
    m_postEqGainSpin->setEnabled(m_curveAvailable);
    m_lowSpin->setEnabled(m_curveAvailable);
    m_highSpin->setEnabled(m_curveAvailable);
    m_useQFactorsChk->setEnabled(m_curveAvailable);
    m_resetCompBtn->setEnabled(m_curveAvailable);
    m_resetEqBtn->setEnabled(m_curveAvailable);
    for (int i = 0; i < currentBandCount() && i < m_bandSelectors->count(); ++i) {
        if (m_bandSelectors->itemAt(i)->widget()) { m_bandSelectors->itemAt(i)->widget()->setEnabled(m_curveAvailable); }
    }
    m_freqSpin->setEnabled(selected);
    m_compSpin->setEnabled(selected);
    m_gainSpin->setEnabled(selected);
    const bool q = selected && m_compUseQ && m_eqUseQ;
    m_compQSpin->setEnabled(q);
    m_eqQSpin->setEnabled(q);
    m_compQSlider->setEnabled(q);
    m_eqQSlider->setEnabled(q);
    m_qGuidance->setVisible(!(m_compUseQ && m_eqUseQ));
}
void TxCfcDialog::updateEditRowFromSelection(int) { refreshControls(); }

void TxCfcDialog::onBandCountChanged()
{
    m_pendingBandCount = m_bandCountGroup->checkedId();
    m_countNotice->setVisible(m_pendingBandCount != currentBandCount());
}

void TxCfcDialog::cancelBandCount()
{
    const QSignalBlocker blocker(m_bandCountGroup);
    m_bandCountGroup->button(currentBandCount())->setChecked(true);
    m_pendingBandCount = 0;
    m_countNotice->hide();
}

void TxCfcDialog::applyBandCount()
{
    if (m_pendingBandCount == 0 || m_pendingBandCount == currentBandCount()) { cancelBandCount(); return; }
    beginEdit();
    {
        QSignalBlocker c(m_compWidget), e(m_postEqWidget);
        m_compWidget->setBandCount(m_pendingBandCount);
        m_postEqWidget->setBandCount(m_pendingBandCount);
    }
    m_curveAvailable = true;
    m_compWidget->show(); m_postEqWidget->show();
    m_invalidCurveGuidance->hide();
    cancelBandCount();
    rebuildBandSelectors();
    finishEdit();
}

// From Thetis frmCFCConfig.cs:120-140 [v2.10.3.15] — shared rescale, 1000 Hz minimum spread.
void TxCfcDialog::onLowFreqChanged(int hz)
{
    if (m_ignoreUpdates) { return; }
    beginEdit();
    hz = std::min(hz, static_cast<int>(std::floor(m_compWidget->frequencyMaxHz() - kMinFreqSpreadHz)));
    { QSignalBlocker c(m_compWidget), e(m_postEqWidget);
      m_compWidget->setFrequencyMinHz(hz); m_postEqWidget->setFrequencyMinHz(hz); }
    changed();
}
void TxCfcDialog::onHighFreqChanged(int hz)
{
    if (m_ignoreUpdates) { return; }
    beginEdit();
    hz = std::max(hz, static_cast<int>(std::ceil(m_compWidget->frequencyMinHz() + kMinFreqSpreadHz)));
    { QSignalBlocker c(m_compWidget), e(m_postEqWidget);
      m_compWidget->setFrequencyMaxHz(hz); m_postEqWidget->setFrequencyMaxHz(hz); }
    changed();
}

void TxCfcDialog::onUseQFactorsToggled(bool on)
{
    if (m_ignoreUpdates) { return; }
    beginEdit();
    m_compUseQ = m_eqUseQ = on;
    { QSignalBlocker c(m_compWidget), e(m_postEqWidget);
      m_compWidget->setParametricEq(on); m_postEqWidget->setParametricEq(on); }
    finishEdit();
}

void TxCfcDialog::onLogScaleToggled(bool on)
{
    if (m_ignoreUpdates) { return; }
    { QSignalBlocker c(m_compWidget), e(m_postEqWidget);
      m_compWidget->setLogScale(on); m_postEqWidget->setLogScale(on); }
    // Presentation changes are not audio edits. Rebase the current snapshot
    // only when history is empty; later audio undo retains its exact graph config.
    m_committedEditState = captureEditState();
    if (!m_history->canUndo() && !m_history->canRedo()) { rebaseEditHistory(); }
}

void TxCfcDialog::resetCurve(ParametricEqWidget* widget)
{
    beginEdit();
    { QSignalBlocker blocker(widget);
      ParametricEqWidget::EqJsonState state;
      state.bandCount = widget->bandCount();
      state.frequencyMinHz = widget->frequencyMinHz();
      state.frequencyMaxHz = widget->frequencyMaxHz();
      state.parametricEq = widget->parametricEq();
      state.points = widget->points();
      for (auto& point : state.points) { point.gainDb = 0; point.q = 4; }
      widget->setEditorCurveState(state); }
    finishEdit();
}
void TxCfcDialog::onResetCompClicked() { resetCurve(m_compWidget); }
void TxCfcDialog::onResetEqClicked() { resetCurve(m_postEqWidget); }
void TxCfcDialog::onOgGuideClicked() { QDesktopServices::openUrl(QUrl(QString::fromUtf8(kOgGuideUrl))); }

void TxCfcDialog::onSelectedBandChanged(int oneBased)
{
    if (m_ignoreUpdates || !m_curveAvailable) { return; }
    { QSignalBlocker c(m_compWidget), e(m_postEqWidget);
      m_compWidget->setSelectedIndex(m_compWidget->getIndexFromBandId(oneBased));
      m_postEqWidget->setSelectedIndex(m_postEqWidget->getIndexFromBandId(oneBased)); }
    refreshControls();
}

void TxCfcDialog::editSelectedPoint(ParametricEqWidget* widget, double f, double g, double q)
{
    if (m_ignoreUpdates || widget->selectedIndex() < 0) { return; }
    if (!m_sliderActive && !m_numericEditor) { beginEdit(); }
    {
        QSignalBlocker blocker(widget);
        const int index = widget->selectedIndex();
        if (f == widget->points()[index].frequencyHz) {
            // Amount/width edits must not run the legacy frequency-spacing
            // sweep over exact centers loaded from a saved profile.
            ParametricEqWidget::EqJsonState state;
            state.bandCount = widget->bandCount();
            state.frequencyMinHz = widget->frequencyMinHz();
            state.frequencyMaxHz = widget->frequencyMaxHz();
            state.globalGainDb = widget->globalGainDb();
            state.parametricEq = widget->parametricEq();
            state.points = widget->points();
            state.points[index].gainDb = std::clamp(g, widget->dbMin(), widget->dbMax());
            state.points[index].q = std::clamp(q, widget->qMin(), widget->qMax());
            widget->setEditorCurveState(state);
        } else {
            widget->setPointData(index, f, g, q);
        }
    }
    syncPairedFrequencies(widget, widget == m_compWidget ? m_postEqWidget : m_compWidget);
    changed();
}
void TxCfcDialog::onFreqSpinChanged(int hz)
{
    const int index = selectedIndex();
    if (index < 0) { return; }
    const auto point = m_compWidget->points()[index];
    editSelectedPoint(m_compWidget, hz, point.gainDb, point.q);
}
void TxCfcDialog::onCompSpinChanged(double db)
{
    const int index = selectedIndex(); if (index < 0) { return; }
    const auto point = m_compWidget->points()[index];
    editSelectedPoint(m_compWidget, point.frequencyHz, db, point.q);
}
void TxCfcDialog::onCompQSpinChanged(double q)
{
    const int index = selectedIndex(); if (index < 0) { return; }
    const auto point = m_compWidget->points()[index];
    editSelectedPoint(m_compWidget, point.frequencyHz, point.gainDb, q);
}
void TxCfcDialog::onGainSpinChanged(double db)
{
    const int index = selectedIndex(); if (index < 0) { return; }
    const auto point = m_postEqWidget->points()[index];
    editSelectedPoint(m_postEqWidget, point.frequencyHz, db, point.q);
}
void TxCfcDialog::onEqQSpinChanged(double q)
{
    const int index = selectedIndex(); if (index < 0) { return; }
    const auto point = m_postEqWidget->points()[index];
    editSelectedPoint(m_postEqWidget, point.frequencyHz, point.gainDb, q);
}
void TxCfcDialog::onPrecompSpinChanged(double db)
{
    if (m_ignoreUpdates) { return; }
    if (!m_numericEditor) { beginEdit(); }
    { QSignalBlocker blocker(m_compWidget); m_compWidget->setGlobalGainDb(db); }
    changed();
}
void TxCfcDialog::onPostEqGainSpinChanged(double db)
{
    if (m_ignoreUpdates) { return; }
    if (!m_numericEditor) { beginEdit(); }
    { QSignalBlocker blocker(m_postEqWidget); m_postEqWidget->setGlobalGainDb(db); }
    changed();
}

bool TxCfcDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_cancelledSlider) {
        if (event->type() == QEvent::MouseButtonRelease) { m_cancelledSlider = nullptr; event->accept(); return true; }
        if (event->type() == QEvent::MouseMove) { event->accept(); return true; }
    }
    if (auto* editor = qobject_cast<QLineEdit*>(watched); editor && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        const bool undo = key->matches(QKeySequence::Undo);
        const bool redo = key->matches(QKeySequence::Redo);
        if ((undo && !editor->isUndoAvailable()) || (redo && !editor->isRedoAvailable())) {
            if (undo) { undoEdit(); } else { redoEdit(); }
            key->accept(); return true;
        }
    }
    auto* spin = qobject_cast<QAbstractSpinBox*>(watched);
    if (!spin && qobject_cast<QLineEdit*>(watched)) { spin = qobject_cast<QAbstractSpinBox*>(watched->parent()); }
    if (spin && event->type() == QEvent::FocusIn && !m_ignoreUpdates && !m_updatingFromModel) {
        if (m_numericEditor != spin) {
            if (m_numericEditor) { m_numericEditor = nullptr; finishEdit(); }
            beginEdit(); m_numericEditor = spin;
        }
    }
    if (qobject_cast<QSlider*>(watched) && event->type() == QEvent::MouseButtonPress) {
        beginEdit();
        m_sliderActive = true;
    }
    if (qobject_cast<QSlider*>(watched) && event->type() == QEvent::MouseButtonRelease) {
        // Groove clicks do not emit sliderReleased. Finish after native handling;
        // handle releases already finish through the slider's own signal.
        QTimer::singleShot(0, this, [this] {
            if (m_sliderActive) { m_sliderActive = false; finishEdit(); }
        });
    }
    // Width sliders live inside a scroll area; avoid accidental wheel edits.
    if (qobject_cast<QSlider*>(watched) && event->type() == QEvent::Wheel) { event->accept(); return true; }
    return QDialog::eventFilter(watched, event);
}

void TxCfcDialog::undoEdit()
{
    if (m_numericEditor) { m_numericEditor = nullptr; finishEdit(); }
    if (const auto state = m_history->undo()) { restoreEditState(*state); }
}
void TxCfcDialog::redoEdit()
{
    if (m_numericEditor) { m_numericEditor = nullptr; finishEdit(); }
    if (const auto state = m_history->redo()) { restoreEditState(*state); }
}
void TxCfcDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Undo) || event->matches(QKeySequence::Redo)) {
        // Focused text fields consume their standard undo before propagation.
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        if (editor && ((event->matches(QKeySequence::Undo) && editor->isUndoAvailable())
                    || (event->matches(QKeySequence::Redo) && editor->isRedoAvailable()))) {
            if (event->matches(QKeySequence::Undo)) { editor->undo(); } else { editor->redo(); }
        } else if (event->matches(QKeySequence::Undo)) { undoEdit(); } else { redoEdit(); }
        event->accept(); return;
    }
    QDialog::keyPressEvent(event);
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
