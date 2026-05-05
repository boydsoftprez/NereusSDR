// =================================================================
// src/gui/setup/DspSetupPages.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

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

#include "DspSetupPages.h"

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/RadioConnection.h"
#include "core/wdsp_api.h"
#include "models/RadioModel.h"
#include "core/WdspEngine.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace NereusSDR {

// ─────────────────────────────────────────────────────────────────────────────
// Helper: disable every child widget inside a group box (NYI guard).
// ─────────────────────────────────────────────────────────────────────────────
static void disableGroup(QGroupBox* grp)
{
    grp->setEnabled(false);
}

// ══════════════════════════════════════════════════════════════════════════════
// AgcAlcSetupPage
// ══════════════════════════════════════════════════════════════════════════════
//
// From Thetis setup.cs — tabDSP / tabPageAGC controls:
//   comboAGCMode, udAGCAttack, udAGCDecay, udAGCHang, tbAGCSlope,
//   udAGCMaxGain, tbAGCHangThreshold, udALCDecay, udALCMaxGain,
//   chkLevelerEnable, udLevelerThreshold, udLevelerDecay
//
AgcAlcSetupPage::AgcAlcSetupPage(RadioModel* model, QWidget* parent)
    : SetupPage("AGC/ALC", model, parent)
{
    SliceModel* slice = model->activeSlice();
    if (!slice) {
        // No active slice (disconnected) — show disabled placeholder
        QGroupBox* grp = addSection("RX1 AGC");
        disableGroup(grp);
        return;
    }

    // ── RX1 AGC ──────────────────────────────────────────────────────────────
    QGroupBox* agcGrp = addSection("RX1 AGC");
    QVBoxLayout* agcLay = qobject_cast<QVBoxLayout*>(agcGrp->layout());

    m_agcModeCombo = new QComboBox;
    m_agcModeCombo->addItems({"Off", "Long", "Slow", "Med", "Fast", "Custom"});
    m_agcModeCombo->setCurrentIndex(static_cast<int>(slice->agcMode()));
    // From Thetis v2.10.3.13 console.resx:4555 — comboAGC.ToolTip
    m_agcModeCombo->setToolTip(QStringLiteral("Automatic Gain Control Mode Setting"));
    addLabeledCombo(agcLay, "Mode", m_agcModeCombo);

    m_agcAttack = new QSpinBox;
    m_agcAttack->setRange(1, 1000);
    m_agcAttack->setSuffix(" ms");
    m_agcAttack->setValue(slice->agcAttack());
    addLabeledSpinner(agcLay, "Attack", m_agcAttack);

    m_agcDecay = new QSpinBox;
    m_agcDecay->setRange(1, 5000);
    m_agcDecay->setSuffix(" ms");
    m_agcDecay->setValue(slice->agcDecay());
    // From Thetis v2.10.3.13 setup.designer.cs:39390 — udDSPAGCDecay.ToolTip
    m_agcDecay->setToolTip(QStringLiteral("Time-constant to increase signal amplitude after strong signal"));
    addLabeledSpinner(agcLay, "Decay", m_agcDecay);

    m_agcHang = new QSpinBox;
    m_agcHang->setRange(10, 5000);
    m_agcHang->setSuffix(" ms");
    m_agcHang->setValue(slice->agcHang());
    // From Thetis v2.10.3.13 setup.designer.cs:39294 — udDSPAGCHangTime.ToolTip
    m_agcHang->setToolTip(QStringLiteral("Time to hold constant gain after strong signal"));
    addLabeledSpinner(agcLay, "Hang", m_agcHang);

    m_agcSlope = new QSlider(Qt::Horizontal);
    m_agcSlope->setRange(0, 20);
    m_agcSlope->setValue(slice->agcSlope() / 10);
    // From Thetis v2.10.3.13 setup.designer.cs:39358 — udDSPAGCSlope.ToolTip
    m_agcSlope->setToolTip(QStringLiteral("Gain difference for weak and strong signals"));
    addLabeledSlider(agcLay, "Slope", m_agcSlope);

    m_agcMaxGain = new QSpinBox;
    m_agcMaxGain->setRange(-20, 120);
    m_agcMaxGain->setSuffix(" dB");
    m_agcMaxGain->setValue(slice->agcMaxGain());
    // From Thetis v2.10.3.13 setup.designer.cs:39325 — udDSPAGCMaxGaindB.ToolTip
    m_agcMaxGain->setToolTip(QStringLiteral("Threshold AGC: no gain over this Max Gain is applied, irrespective of signal weakness"));
    addLabeledSpinner(agcLay, "Max Gain", m_agcMaxGain);

    m_agcFixedGain = new QSpinBox;
    m_agcFixedGain->setRange(-20, 120);
    m_agcFixedGain->setSuffix(" dB");
    m_agcFixedGain->setValue(slice->agcFixedGain());
    // From Thetis v2.10.3.13 setup.designer.cs:39448 — udDSPAGCFixedGaindB.ToolTip
    m_agcFixedGain->setToolTip(QStringLiteral("Gain when AGC is disabled"));
    addLabeledSpinner(agcLay, "Fixed Gain", m_agcFixedGain);

    m_agcHangThresh = new QSlider(Qt::Horizontal);
    m_agcHangThresh->setRange(0, 100);
    m_agcHangThresh->setValue(slice->agcHangThreshold());
    // From Thetis v2.10.3.13 setup.designer.cs:39250 — tbDSPAGCHangThreshold.ToolTip
    m_agcHangThresh->setToolTip(QStringLiteral("Level at which the 'hang' function is engaged"));
    addLabeledSlider(agcLay, "Hang Threshold", m_agcHangThresh);

    // ── Wire AGC controls to SliceModel ──────────────────────────────────────

    connect(m_agcModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [slice](int idx) {
        slice->setAgcMode(static_cast<AGCMode>(idx));
    });
    connect(m_agcModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        updateCustomGating(static_cast<AGCMode>(idx));
    });

    connect(m_agcAttack, QOverload<int>::of(&QSpinBox::valueChanged),
            slice, &SliceModel::setAgcAttack);

    connect(m_agcDecay, QOverload<int>::of(&QSpinBox::valueChanged),
            slice, &SliceModel::setAgcDecay);

    connect(m_agcHang, QOverload<int>::of(&QSpinBox::valueChanged),
            slice, &SliceModel::setAgcHang);

    connect(m_agcSlope, &QSlider::valueChanged,
            this, [slice](int val) {
        slice->setAgcSlope(val * 10);  // UI 0-20, WDSP gets ×10
    });

    connect(m_agcMaxGain, QOverload<int>::of(&QSpinBox::valueChanged),
            slice, &SliceModel::setAgcMaxGain);

    connect(m_agcFixedGain, QOverload<int>::of(&QSpinBox::valueChanged),
            slice, &SliceModel::setAgcFixedGain);

    connect(m_agcHangThresh, &QSlider::valueChanged,
            slice, &SliceModel::setAgcHangThreshold);

    // ── Auto AGC ─────────────────────────────────────────────────────────────
    QGroupBox* autoAgcGrp = addSection("Auto AGC");
    QVBoxLayout* autoAgcLay = qobject_cast<QVBoxLayout*>(autoAgcGrp->layout());

    m_autoAgcChk = new QCheckBox("Auto AGC RX1");
    m_autoAgcChk->setChecked(slice->autoAgcEnabled());
    // From Thetis v2.10.3.13 setup.designer.cs:38679 — chkAutoAGCRX1.ToolTip
    m_autoAgcChk->setToolTip(QStringLiteral("Automatically adjust AGC based on Noise Floor"));
    autoAgcLay->addWidget(m_autoAgcChk);

    m_autoAgcOffset = new QSpinBox;
    m_autoAgcOffset->setRange(-60, 60);
    m_autoAgcOffset->setSuffix(" dB");
    m_autoAgcOffset->setValue(static_cast<int>(slice->autoAgcOffset()));
    // From Thetis v2.10.3.13 setup.designer.cs:38649 — udRX1AutoAGCOffset.ToolTip
    m_autoAgcOffset->setToolTip(QStringLiteral("dB shift from noise floor"));
    addLabeledSpinner(autoAgcLay, "± Offset", m_autoAgcOffset);

    connect(m_autoAgcChk, &QCheckBox::toggled,
            slice, &SliceModel::setAutoAgcEnabled);

    connect(m_autoAgcOffset, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [slice](int val) {
        slice->setAutoAgcOffset(static_cast<double>(val));
    });

    // ── Custom-mode gating ───────────────────────────────────────────────────
    // From Thetis v2.10.3.13 setup.cs:5046-5076 — CustomRXAGCEnabled
    updateCustomGating(slice->agcMode());

    // ══════════════════════════════════════════════════════════════════════════
    // ── TX Leveler ────────────────────────────────────────────────────────────
    // ══════════════════════════════════════════════════════════════════════════
    //
    // Phase 3M-3a-i Batch 2 (Task D): replaces the prior disabled "Leveler"
    // NYI stub.  Wires Setup → DSP → AGC/ALC → TX Leveler controls
    // bidirectionally to TransmitModel TX Leveler properties (added in
    // Batch 1 Task C).  Range / default / tooltip text all mirror Thetis
    // grpDSPLeveler at setup.Designer.cs:38683-38791 [v2.10.3.13].
    //
    TransmitModel& tx = model->transmitModel();

    QGroupBox* txLevGrp = addSection("TX Leveler");
    QVBoxLayout* txLevLay = qobject_cast<QVBoxLayout*>(txLevGrp->layout());

    m_txLevelerOnChk = new QCheckBox("Enable");
    m_txLevelerOnChk->setChecked(tx.txLevelerOn());
    // From Thetis setup.Designer.cs:38707 [v2.10.3.13] — chkDSPLevelerEnabled tooltip.
    m_txLevelerOnChk->setToolTip(
        QStringLiteral("Adjust gain if transmit audio increases/decreases"));
    txLevLay->addWidget(m_txLevelerOnChk);

    m_txLevelerTopSpin = new QSpinBox;
    m_txLevelerTopSpin->setRange(TransmitModel::kTxLevelerMaxGainDbMin,
                                 TransmitModel::kTxLevelerMaxGainDbMax);
    m_txLevelerTopSpin->setSuffix(" dB");
    m_txLevelerTopSpin->setValue(tx.txLevelerMaxGain());
    // From Thetis setup.Designer.cs:38732-38733 [v2.10.3.13] — udDSPLevelerThreshold tooltip.
    m_txLevelerTopSpin->setToolTip(
        QStringLiteral("This provides for a 'threshold' ALC. Irrespective of how weak "
                       "the input is, no gain over this max is applied."));
    addLabeledSpinner(txLevLay, "Max Gain", m_txLevelerTopSpin);

    m_txLevelerDecaySpin = new QSpinBox;
    m_txLevelerDecaySpin->setRange(TransmitModel::kTxLevelerDecayMsMin,
                                   TransmitModel::kTxLevelerDecayMsMax);
    m_txLevelerDecaySpin->setSuffix(" ms");
    m_txLevelerDecaySpin->setValue(tx.txLevelerDecay());
    // From Thetis setup.Designer.cs:38764-38765 [v2.10.3.13] — udDSPLevelerDecay tooltip.
    m_txLevelerDecaySpin->setToolTip(
        QStringLiteral("Decay time-constant in ms.  Note that this is a time-constant "
                       "for an exponential curve, not an absolute time."));
    addLabeledSpinner(txLevLay, "Decay", m_txLevelerDecaySpin);

    // Wire TX Leveler controls bidirectionally to TransmitModel.
    connect(m_txLevelerOnChk, &QCheckBox::toggled,
            &tx, &TransmitModel::setTxLevelerOn);
    connect(&tx, &TransmitModel::txLevelerOnChanged,
            m_txLevelerOnChk, [this](bool on) {
        QSignalBlocker b(m_txLevelerOnChk);
        m_txLevelerOnChk->setChecked(on);
    });

    connect(m_txLevelerTopSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            &tx, &TransmitModel::setTxLevelerMaxGain);
    connect(&tx, &TransmitModel::txLevelerMaxGainChanged,
            m_txLevelerTopSpin, [this](int dB) {
        QSignalBlocker b(m_txLevelerTopSpin);
        m_txLevelerTopSpin->setValue(dB);
    });

    connect(m_txLevelerDecaySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            &tx, &TransmitModel::setTxLevelerDecay);
    connect(&tx, &TransmitModel::txLevelerDecayChanged,
            m_txLevelerDecaySpin, [this](int ms) {
        QSignalBlocker b(m_txLevelerDecaySpin);
        m_txLevelerDecaySpin->setValue(ms);
    });

    // ══════════════════════════════════════════════════════════════════════════
    // ── TX ALC ────────────────────────────────────────────────────────────────
    // ══════════════════════════════════════════════════════════════════════════
    //
    // Phase 3M-3a-i Batch 2 (Task D): replaces the prior disabled "ALC" NYI
    // stub.  No "Enable" checkbox — ALC Run is locked-on per Thetis schema
    // (chkALCEnabled is absent from setup.Designer.cs grpDSPALC; WdspEngine
    // boot calls SetTXAALCSt(1) at WdspEngine.cpp:438).  Range / default /
    // tooltip text all mirror Thetis grpDSPALC at setup.Designer.cs:
    // 38793-38866 [v2.10.3.13].
    //
    QGroupBox* txAlcGrp = addSection("TX ALC");
    QVBoxLayout* txAlcLay = qobject_cast<QVBoxLayout*>(txAlcGrp->layout());

    m_txAlcMaxGainSpin = new QSpinBox;
    m_txAlcMaxGainSpin->setRange(TransmitModel::kTxAlcMaxGainDbMin,
                                 TransmitModel::kTxAlcMaxGainDbMax);
    m_txAlcMaxGainSpin->setSuffix(" dB");
    m_txAlcMaxGainSpin->setValue(tx.txAlcMaxGain());
    // From Thetis setup.Designer.cs:38828 [v2.10.3.13] — udDSPALCMaximumGain tooltip.
    m_txAlcMaxGainSpin->setToolTip(
        QStringLiteral("Maximum gain to apply before ALC limiting"));
    addLabeledSpinner(txAlcLay, "Max Gain", m_txAlcMaxGainSpin);

    m_txAlcDecaySpin = new QSpinBox;
    m_txAlcDecaySpin->setRange(TransmitModel::kTxAlcDecayMsMin,
                               TransmitModel::kTxAlcDecayMsMax);
    m_txAlcDecaySpin->setSuffix(" ms");
    m_txAlcDecaySpin->setValue(tx.txAlcDecay());
    // From Thetis setup.Designer.cs:38858-38859 [v2.10.3.13] — udDSPALCDecay tooltip.
    m_txAlcDecaySpin->setToolTip(
        QStringLiteral("Decay time-constant in ms.  Note that this is a time-constant "
                       "for an exponential curve, not an absolute time."));
    addLabeledSpinner(txAlcLay, "Decay", m_txAlcDecaySpin);

    // Wire TX ALC controls bidirectionally to TransmitModel.
    connect(m_txAlcMaxGainSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            &tx, &TransmitModel::setTxAlcMaxGain);
    connect(&tx, &TransmitModel::txAlcMaxGainChanged,
            m_txAlcMaxGainSpin, [this](int dB) {
        QSignalBlocker b(m_txAlcMaxGainSpin);
        m_txAlcMaxGainSpin->setValue(dB);
    });

    connect(m_txAlcDecaySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            &tx, &TransmitModel::setTxAlcDecay);
    connect(&tx, &TransmitModel::txAlcDecayChanged,
            m_txAlcDecaySpin, [this](int ms) {
        QSignalBlocker b(m_txAlcDecaySpin);
        m_txAlcDecaySpin->setValue(ms);
    });
}

// From Thetis v2.10.3.13 setup.cs:5046-5076 — CustomRXAGCEnabled
void AgcAlcSetupPage::updateCustomGating(AGCMode mode)
{
    const bool custom = (mode == AGCMode::Custom);
    m_agcDecay->setEnabled(custom);
    m_agcHang->setEnabled(custom);
}

// ══════════════════════════════════════════════════════════════════════════════
// NrAnfSetupPage
// ══════════════════════════════════════════════════════════════════════════════
//
// Porting from Thetis setup.designer.cs NR/ANF controls [v2.10.3.13]:
//
//   NR1 tab: udDSPNRTaps (16-1024), udDSPNRDelay (1-256),
//            tbDSPNRGain (0-999), tbDSPNRLeak (0-999),
//            rdoDSPNRPreAGC / rdoDSPNRPostAGC
//   NR2 tab: rdoEMNRGainMethod 0-3 (Linear/Log/Gamma/Trained),
//            rdoEMNRNPEMethod 0-2 (OSMS/MMSE/NSTAT),
//            udDSPEMNRTrainT1 (−5.0..5.0 step 0.1),
//            udDSPEMNRTrainT2 (0.0..2.0 step 0.05),
//            chkDSPEMNRAEFilter, rdoDSPEMNRPreAGC / rdoDSPEMNRPostAGC,
//            Noise Post Proc group: chkDSPEMNRPost2Run,
//              udDSPEMNRPost2Level/Factor/Rate, udDSPEMNRPost2Taper
//   NR3 tab: rdoDSPNR3PreAGC / rdoDSPNR3PostAGC,
//            chkRXANR3FixedGain, model selector (global)
//   NR4 tab: udDSPSBNRreduction (0-20 step 0.1), udDSPSBNRsmooth (0-100),
//            udDSPSBNRwhiten (0-100), udDSPSBNRrescale (0-12 step 0.1),
//            udDSPSBNRsnrthresh (-10..10 step 0.5), rdoSBNR1/2/3
//   DFNR tab: (AetherSDR native, not Thetis)
//   MNR tab:  (AetherSDR native, macOS only, not Thetis)
//   ANF tab:  chkANFEnable (Thetis chkDSPANFEnable)
//
// Slice-tracking policy: binds to model->activeSlice() at construction
// (simple pattern matching AgcAlcSetupPage). Slice-switch requires
// close+reopen of Setup dialog. Full dynamic rebind deferred to Task 21.
//
NrAnfSetupPage::NrAnfSetupPage(RadioModel* model, QWidget* parent)
    : SetupPage("NR/ANF", model, parent)
{
    // Embed QTabWidget directly in the inherited contentLayout().
    // This is identical to HardwarePage's pattern (HardwarePage.cpp:90-95).
    auto* tabs = new QTabWidget(this);
    m_tabs = tabs;  // remember for selectSubtab()
    tabs->setTabPosition(QTabWidget::North);
    tabs->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #304050; background: #0f0f1a; }"
        "QTabBar::tab { background: #1a2a3a; color: #8aa8c0; padding: 4px 10px; "
        "               border: 1px solid #304050; border-bottom: none; border-radius: 3px 3px 0 0; }"
        "QTabBar::tab:selected { background: #0f0f1a; color: #c8d8e8; }"
        "QTabBar::tab:hover { background: #203040; }");
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    // Remove the trailing stretch that SetupPage adds in its ctor
    // (SetupPage.cpp:90 — m_contentLayout->addStretch(1)). That stretch
    // would otherwise compete with our tabs widget for vertical space
    // (50/50 split when both have stretch=1), leaving dead space below
    // the tab pane. With the stretch removed and our tabs getting
    // stretch=1 + Expanding policy, the tab pane fills all available
    // vertical space in the Setup dialog.
    {
        const int last = contentLayout()->count() - 1;
        if (auto* s = contentLayout()->itemAt(last); s && s->spacerItem()) {
            delete contentLayout()->takeAt(last);
        }
    }
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    contentLayout()->addWidget(tabs, /*stretch=*/1);

    SliceModel* slice = model ? model->activeSlice() : nullptr;

    // Helper: build a tab-page widget with a QVBoxLayout + scroll.
    // Returns {outer QWidget (used as tab), inner QVBoxLayout (for sections)}.
    auto makeTab = [](QTabWidget* tw, const QString& name)
        -> std::pair<QWidget*, QVBoxLayout*>
    {
        auto* page = new QWidget;
        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setStyleSheet("QScrollArea { background: #0f0f1a; border: none; }");
        page->setStyleSheet("QWidget { background: #0f0f1a; }");
        auto* vlay = new QVBoxLayout(page);
        vlay->setContentsMargins(8, 8, 8, 8);
        vlay->setSpacing(6);
        scroll->setWidget(page);
        tw->addTab(scroll, name);
        return {page, vlay};
    };

    // Helper: add a titled group box to a tab VBoxLayout.
    // Returns the group's inner QVBoxLayout.
    auto makeGroup = [](QVBoxLayout* parent, const QString& title) -> QVBoxLayout*
    {
        static const QString kGrpStyle =
            "QGroupBox { border: 1px solid #304050; border-radius: 4px; "
            "margin-top: 8px; padding-top: 12px; font-weight: bold; color: #8aa8c0; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }";
        auto* grp = new QGroupBox(title);
        grp->setStyleSheet(kGrpStyle);
        auto* lay = new QVBoxLayout(grp);
        lay->setContentsMargins(8, 4, 8, 8);
        lay->setSpacing(4);
        parent->addWidget(grp);
        return lay;
    };

    // Shared label/control style constants (mirror SetupPage::makeLabeledRow).
    static const QString kLbl = "QLabel { color: #c8d8e8; font-size: 12px; }";
    static const QString kCombo =
        "QComboBox { background: #1a2a3a; border: 1px solid #304050; "
        "border-radius: 3px; color: #c8d8e8; font-size: 12px; padding: 2px 4px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #1a2a3a; color: #c8d8e8; "
        "selection-background-color: #00b4d8; }";
    static const QString kSlider =
        "QSlider::groove:horizontal { background: #1a2a3a; height: 4px; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #00b4d8; width: 12px; height: 12px; "
        "border-radius: 6px; margin: -4px 0; }";
    static const QString kInfoLbl =
        "QLabel { color: #5a8aaa; font-size: 12px; font-style: italic; }";

    // Label style used inside lambdas (non-static copy so it can be captured).
    const QString lblStyle = kLbl;

    // Helper: add a labeled row (150px label + stretch control) into a QVBoxLayout.
    auto addRow = [lblStyle](QVBoxLayout* vl, const QString& labelText, QWidget* ctrl)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        auto* lbl = new QLabel(labelText);
        lbl->setStyleSheet(lblStyle);
        lbl->setFixedWidth(150);
        row->addWidget(lbl);
        row->addWidget(ctrl, 1);
        vl->addLayout(row);
    };

    // addSliderRow — horizontal QSlider + live value readout label on the right.
    // For integer-valued controls.
    // Returns {slider, valueLabel}.
    auto addSliderRow = [](QVBoxLayout* parent, const QString& labelText,
                           int minimum, int maximum, int defaultValue,
                           const QString& tooltip = QString(),
                           const QString& suffix = QString())
        -> std::pair<QSlider*, QLabel*>
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);

        auto* label = new QLabel(labelText);
        label->setStyleSheet("QLabel { color: #8aa8c0; font-size: 12px; }");
        label->setFixedWidth(80);
        row->addWidget(label);

        auto* slider = new QSlider(Qt::Horizontal);
        slider->setRange(minimum, maximum);
        slider->setValue(defaultValue);
        slider->setStyleSheet(
            "QSlider::groove:horizontal { background: #1a2a3a; height: 4px; border-radius: 2px; }"
            "QSlider::handle:horizontal { background: #00b4d8; width: 12px; height: 12px; "
            "border-radius: 6px; margin: -4px 0; }");
        if (!tooltip.isEmpty()) { slider->setToolTip(tooltip); }
        row->addWidget(slider, /*stretch=*/1);

        auto* value = new QLabel(QString::number(defaultValue) + suffix);
        value->setStyleSheet("QLabel { color: #c8d8e8; font-size: 12px; font-weight: bold; }");
        value->setFixedWidth(48);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(value);

        QObject::connect(slider, &QSlider::valueChanged, value,
            [value, suffix](int v) { value->setText(QString::number(v) + suffix); });

        parent->addLayout(row);
        return {slider, value};
    };

    // addDoubleSliderRow — QSlider + label for double-valued controls.
    // Uses an integer-backed QSlider with a scale factor (1/step) to represent
    // fractional values. Returns {slider, valueLabel, scale}.
    auto addDoubleSliderRow = [](QVBoxLayout* parent, const QString& labelText,
                                 double minimum, double maximum, double defaultValue,
                                 double step, int decimals,
                                 const QString& tooltip = QString(),
                                     const QString& suffix = QString())
        -> std::tuple<QSlider*, QLabel*, double>
    {
        const double scale = 1.0 / step;   // e.g. step=0.1 → scale=10
        auto* row = new QHBoxLayout;
        row->setSpacing(8);

        auto* label = new QLabel(labelText);
        label->setStyleSheet("QLabel { color: #8aa8c0; font-size: 12px; }");
        label->setFixedWidth(80);
        row->addWidget(label);

        auto* slider = new QSlider(Qt::Horizontal);
        slider->setRange(static_cast<int>(minimum * scale),
                         static_cast<int>(maximum * scale));
        slider->setValue(static_cast<int>(defaultValue * scale));
        slider->setStyleSheet(
            "QSlider::groove:horizontal { background: #1a2a3a; height: 4px; border-radius: 2px; }"
            "QSlider::handle:horizontal { background: #00b4d8; width: 12px; height: 12px; "
            "border-radius: 6px; margin: -4px 0; }");
        if (!tooltip.isEmpty()) { slider->setToolTip(tooltip); }
        row->addWidget(slider, 1);

        auto* value = new QLabel;
        value->setStyleSheet("QLabel { color: #c8d8e8; font-size: 12px; font-weight: bold; }");
        value->setFixedWidth(56);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        value->setText(QString::number(defaultValue, 'f', decimals) + suffix);
        row->addWidget(value);

        QObject::connect(slider, &QSlider::valueChanged, value,
            [value, scale, decimals, suffix](int v) {
                value->setText(QString::number(v / scale, 'f', decimals) + suffix);
            });

        parent->addLayout(row);
        return {slider, value, scale};
    };

    // Helper: add a pair of radio buttons as a Position row.
    // Returns {preRadio, postRadio}.
    auto addPositionRow = [lblStyle](QVBoxLayout* vl) -> std::pair<QRadioButton*, QRadioButton*>
    {
        auto* preRdo  = new QRadioButton("Pre-AGC");
        auto* postRdo = new QRadioButton("Post-AGC");
        preRdo->setStyleSheet(
            "QRadioButton { color: #c8d8e8; font-size: 12px; }"
            "QRadioButton::indicator { width: 14px; height: 14px; }"
            "QRadioButton::indicator:unchecked { border: 2px solid #304050; "
            "border-radius: 7px; background: #1a2a3a; }"
            "QRadioButton::indicator:checked { border: 2px solid #00b4d8; "
            "border-radius: 7px; background: #00b4d8; }");
        postRdo->setStyleSheet(preRdo->styleSheet());
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        auto* lbl = new QLabel("Position");
        lbl->setStyleSheet(lblStyle);
        lbl->setFixedWidth(150);
        row->addWidget(lbl);
        row->addWidget(preRdo);
        row->addWidget(postRdo);
        row->addStretch(1);
        vl->addLayout(row);
        return {preRdo, postRdo};
    };

    // ── NR1 tab ───────────────────────────────────────────────────────────────
    // From Thetis setup.designer.cs NR1 (ANR) group [v2.10.3.13].
    {
        auto [tabPage, tabLay] = makeTab(tabs, "NR1");
        Q_UNUSED(tabPage)
        QVBoxLayout* grpLay = makeGroup(tabLay, "NR1 (LMS)");

        // Taps — udDSPNRTaps: 16-1024, default 64
        // From Thetis setup.designer.cs — udDSPNRTaps.ToolTip [v2.10.3.13]
        auto [taps, tapsVal] = addSliderRow(grpLay, "Taps", 16, 1024,
            slice ? slice->nr1Taps() : 64,
            tr("LMS filter length (number of taps). Longer = more suppression, "
               "more latency. Range 16-1024."));

        // Delay — udDSPNRDelay: 1-256, default 16
        auto [delay, delayVal] = addSliderRow(grpLay, "Delay", 1, 256,
            slice ? slice->nr1Delay() : 16,
            tr("LMS adaptive delay in samples. Separates desired signal "
               "from correlated noise."));

        // Gain — tbDSPNRGain: UI range 0-999 → WDSP domain = UI × 1e-6
        // From Thetis setup.cs NR1 gain rescaling [v2.10.3.13].
        // Reverse-scale from WDSP domain: WDSP_val = UI / 1e6 → UI = WDSP_val × 1e6
        // Clamped to 999 (slider max) — WDSP default 16e-4 = 1600 overflows range.
        const int gainDefault = slice
            ? std::min(999, static_cast<int>(slice->nr1Gain() * 1e6))
            : 999;
        auto [gain, gainVal] = addSliderRow(grpLay, "Gain", 0, 999,
            gainDefault,
            tr("LMS adaptation rate (gain). UI units × 1e-6 = WDSP domain value. "
               "Default 999 (clamped from 16e-4 WDSP)."));

        // Leakage — tbDSPNRLeak: UI range 0-999 → WDSP domain = UI × 1e-3
        // From Thetis setup.cs NR1 leakage rescaling [v2.10.3.13].
        // Reverse-scale: WDSP_val = UI / 1e3 → UI = WDSP_val × 1e3
        // Default slice value is 10e-7; scaled UI = 10e-7 × 1e3 ≈ 0
        auto [leak, leakVal] = addSliderRow(grpLay, "Leak", 0, 999,
            slice ? static_cast<int>(slice->nr1Leakage() * 1e3) : 0,
            tr("LMS leakage factor. UI units × 1e-3 = WDSP domain value. "
               "Default 0 (= 10e-7 WDSP)."));

        // Position radio
        auto [preRdo, postRdo] = addPositionRow(grpLay);
        const bool isPost = !slice || (slice->nr1Position() == NrPosition::PostAgc);
        preRdo->setChecked(!isPost);
        postRdo->setChecked(isPost);

        tabLay->addStretch(1);

        // ── Wire NR1 controls → SliceModel ──────────────────────────────────
        if (slice) {
            connect(taps, &QSlider::valueChanged,
                    slice, &SliceModel::setNr1Taps);

            connect(delay, &QSlider::valueChanged,
                    slice, &SliceModel::setNr1Delay);

            connect(gain, &QSlider::valueChanged,
                    slice, [slice](int v) {
                // UI × 1e-6 → WDSP domain (matching VfoWidget Task 8 scaling).
                slice->setNr1Gain(static_cast<double>(v) * 1e-6);
            });

            connect(leak, &QSlider::valueChanged,
                    slice, [slice](int v) {
                // UI × 1e-3 → WDSP domain (matching VfoWidget Task 8 scaling).
                slice->setNr1Leakage(static_cast<double>(v) * 1e-3);
            });

            connect(preRdo, &QRadioButton::toggled, slice, [slice](bool checked) {
                if (checked) { slice->setNr1Position(NrPosition::PreAgc); }
            });
            connect(postRdo, &QRadioButton::toggled, slice, [slice](bool checked) {
                if (checked) { slice->setNr1Position(NrPosition::PostAgc); }
            });

            // ── Model → UI (bi-directional sync) ────────────────────────────
            connect(slice, &SliceModel::nr1TapsChanged, taps, [taps](int v) {
                QSignalBlocker b(taps); taps->setValue(v);
            });
            connect(slice, &SliceModel::nr1DelayChanged, delay, [delay](int v) {
                QSignalBlocker b(delay); delay->setValue(v);
            });
            connect(slice, &SliceModel::nr1GainChanged, gain, [gain](double v) {
                QSignalBlocker b(gain);
                gain->setValue(std::min(999, static_cast<int>(v * 1e6)));
            });
            connect(slice, &SliceModel::nr1LeakageChanged, leak, [leak](double v) {
                QSignalBlocker b(leak); leak->setValue(static_cast<int>(v * 1e3));
            });
            connect(slice, &SliceModel::nr1PositionChanged, preRdo,
                    [preRdo, postRdo](NrPosition p) {
                QSignalBlocker b1(preRdo), b2(postRdo);
                preRdo->setChecked(p == NrPosition::PreAgc);
                postRdo->setChecked(p == NrPosition::PostAgc);
            });
        }
    }

    // ── NR2 tab ───────────────────────────────────────────────────────────────
    // From Thetis setup.designer.cs NR2 (EMNR) group [v2.10.3.13].
    {
        auto [tabPage, tabLay] = makeTab(tabs, "NR2");
        Q_UNUSED(tabPage)

        // ── Gain Method ─────────────────────────────────────────────────────
        QVBoxLayout* gmGrp = makeGroup(tabLay, "Gain Method");
        const QStringList gmLabels = {"Linear", "Log", "Gamma", "Trained"};
        QVector<QRadioButton*> gmRdos;
        {
            for (int i = 0; i < gmLabels.size(); ++i) {
                auto* rdo = new QRadioButton(gmLabels[i]);
                rdo->setStyleSheet(
                    "QRadioButton { color: #c8d8e8; font-size: 12px; }"
                    "QRadioButton::indicator { width: 14px; height: 14px; }"
                    "QRadioButton::indicator:unchecked { border: 2px solid #304050; "
                    "border-radius: 7px; background: #1a2a3a; }"
                    "QRadioButton::indicator:checked { border: 2px solid #00b4d8; "
                    "border-radius: 7px; background: #00b4d8; }");
                gmGrp->addWidget(rdo);
                gmRdos.append(rdo);
            }
            const int gmIdx = slice ? static_cast<int>(slice->nr2GainMethod()) : 2; // Gamma default
            if (gmIdx >= 0 && gmIdx < gmRdos.size()) { gmRdos[gmIdx]->setChecked(true); }
        }

        // ── NPE Method ──────────────────────────────────────────────────────
        QVBoxLayout* npeGrp = makeGroup(tabLay, "NPE Method");
        const QStringList npeLabels = {"OSMS", "MMSE", "NSTAT"};
        QVector<QRadioButton*> npeRdos;
        {
            for (const QString& lbl : npeLabels) {
                auto* rdo = new QRadioButton(lbl);
                rdo->setStyleSheet(gmRdos[0]->styleSheet());
                npeGrp->addWidget(rdo);
                npeRdos.append(rdo);
            }
            const int npeIdx = slice ? static_cast<int>(slice->nr2NpeMethod()) : 0; // OSMS default
            if (npeIdx >= 0 && npeIdx < npeRdos.size()) { npeRdos[npeIdx]->setChecked(true); }
        }

        // ── Training / Filter ────────────────────────────────────────────────
        QVBoxLayout* tfGrp = makeGroup(tabLay, "Training / Filter");

        // T1 — udDSPEMNRTrainT1: −5.0 .. 5.0 step 0.1, default −0.5
        auto [t1, t1Val, t1Scale] = addDoubleSliderRow(tfGrp, "T1",
            -5.0, 5.0, slice ? slice->nr2TrainT1() : -0.5,
            0.1, 1,
            tr("EMNR Zeta threshold (T1). Controls the asymmetry of the "
               "noise estimator. Range -5.0 to +5.0."));

        // T2 — udDSPEMNRTrainT2: 0.0 .. 2.0 step 0.05, default 0.20
        auto [t2, t2Val, t2Scale] = addDoubleSliderRow(tfGrp, "T2",
            0.0, 2.0, slice ? slice->nr2TrainT2() : 0.20,
            0.05, 2,
            tr("EMNR T2 training parameter. Range 0.0 to 2.0."));

        // AE Filter checkbox
        auto* aeChk = new QCheckBox("AE Filter");
        aeChk->setChecked(slice ? slice->nr2AeFilter() : true);
        aeChk->setToolTip(tr("Enable EMNR Acoustic Echo (AE) filter stage."));
        tfGrp->addWidget(aeChk);

        // Position radio
        auto [preRdo, postRdo] = addPositionRow(tfGrp);
        {
            const bool isPost = !slice || (slice->nr2Position() == NrPosition::PostAgc);
            preRdo->setChecked(!isPost);
            postRdo->setChecked(isPost);
        }

        // ── Noise Post-Proc ─────────────────────────────────────────────────
        QVBoxLayout* ppGrp = makeGroup(tabLay, "Noise Post-Proc");

        auto* ppRun = new QCheckBox("Enable");
        ppRun->setChecked(slice ? slice->nr2Post2Run() : false);
        ppRun->setToolTip(tr("Enable EMNR Noise Post-Processing cascade."));
        ppGrp->addWidget(ppRun);

        // Level — udDSPEMNRPost2Level: 0-100 step 1, default 15.0
        auto [ppLevel, ppLevelVal, ppLevelScale] = addDoubleSliderRow(ppGrp, "Level",
            0.0, 100.0, slice ? slice->nr2Post2Level() : 15.0,
            1.0, 0);

        // Factor — udDSPEMNRPost2Factor: 0-100 step 1, default 15.0
        auto [ppFactor, ppFactorVal, ppFactorScale] = addDoubleSliderRow(ppGrp, "Factor",
            0.0, 100.0, slice ? slice->nr2Post2Factor() : 15.0,
            1.0, 0);

        // Rate — udDSPEMNRPost2Rate: 0-100 step 0.1, default 5.0
        auto [ppRate, ppRateVal, ppRateScale] = addDoubleSliderRow(ppGrp, "Rate",
            0.0, 100.0, slice ? slice->nr2Post2Rate() : 5.0,
            0.1, 1);

        // Taper — udDSPEMNRPost2Taper (integer): 0-100, default 12
        auto [ppTaper, ppTaperVal] = addSliderRow(ppGrp, "Taper", 0, 100,
            slice ? slice->nr2Post2Taper() : 12);

        tabLay->addStretch(1);

        // ── Wire NR2 controls → SliceModel ──────────────────────────────────
        if (slice) {
            // Gain Method radios → slice
            for (int i = 0; i < gmRdos.size(); ++i) {
                connect(gmRdos[i], &QRadioButton::toggled, slice,
                        [slice, i](bool checked) {
                    if (checked) {
                        slice->setNr2GainMethod(static_cast<EmnrGainMethod>(i));
                    }
                });
            }
            // NPE Method radios → slice
            for (int i = 0; i < npeRdos.size(); ++i) {
                connect(npeRdos[i], &QRadioButton::toggled, slice,
                        [slice, i](bool checked) {
                    if (checked) {
                        slice->setNr2NpeMethod(static_cast<EmnrNpeMethod>(i));
                    }
                });
            }

            connect(t1, &QSlider::valueChanged, slice, [slice, t1Scale](int v) {
                slice->setNr2TrainT1(v / t1Scale);
            });
            connect(t2, &QSlider::valueChanged, slice, [slice, t2Scale](int v) {
                slice->setNr2TrainT2(v / t2Scale);
            });
            connect(aeChk, &QCheckBox::toggled, slice, &SliceModel::setNr2AeFilter);

            connect(preRdo, &QRadioButton::toggled, slice, [slice](bool checked) {
                if (checked) { slice->setNr2Position(NrPosition::PreAgc); }
            });
            connect(postRdo, &QRadioButton::toggled, slice, [slice](bool checked) {
                if (checked) { slice->setNr2Position(NrPosition::PostAgc); }
            });

            connect(ppRun, &QCheckBox::toggled, slice, &SliceModel::setNr2Post2Run);
            connect(ppLevel, &QSlider::valueChanged, slice, [slice, ppLevelScale](int v) {
                slice->setNr2Post2Level(v / ppLevelScale);
            });
            connect(ppFactor, &QSlider::valueChanged, slice, [slice, ppFactorScale](int v) {
                slice->setNr2Post2Factor(v / ppFactorScale);
            });
            connect(ppRate, &QSlider::valueChanged, slice, [slice, ppRateScale](int v) {
                slice->setNr2Post2Rate(v / ppRateScale);
            });
            connect(ppTaper, &QSlider::valueChanged,
                    slice, &SliceModel::setNr2Post2Taper);

            // ── Model → UI (bi-directional sync) ────────────────────────────
            connect(slice, &SliceModel::nr2GainMethodChanged,
                    [gmRdos](EmnrGainMethod v) {
                const int idx = static_cast<int>(v);
                if (idx >= 0 && idx < gmRdos.size()) {
                    QSignalBlocker b(gmRdos[idx]);
                    gmRdos[idx]->setChecked(true);
                }
            });
            connect(slice, &SliceModel::nr2NpeMethodChanged,
                    [npeRdos](EmnrNpeMethod v) {
                const int idx = static_cast<int>(v);
                if (idx >= 0 && idx < npeRdos.size()) {
                    QSignalBlocker b(npeRdos[idx]);
                    npeRdos[idx]->setChecked(true);
                }
            });
            connect(slice, &SliceModel::nr2TrainT1Changed, t1, [t1, t1Scale](double v) {
                QSignalBlocker b(t1); t1->setValue(static_cast<int>(v * t1Scale));
            });
            connect(slice, &SliceModel::nr2TrainT2Changed, t2, [t2, t2Scale](double v) {
                QSignalBlocker b(t2); t2->setValue(static_cast<int>(v * t2Scale));
            });
            connect(slice, &SliceModel::nr2AeFilterChanged, aeChk, [aeChk](bool v) {
                QSignalBlocker b(aeChk); aeChk->setChecked(v);
            });
            connect(slice, &SliceModel::nr2PositionChanged, preRdo,
                    [preRdo, postRdo](NrPosition p) {
                QSignalBlocker b1(preRdo), b2(postRdo);
                preRdo->setChecked(p == NrPosition::PreAgc);
                postRdo->setChecked(p == NrPosition::PostAgc);
            });
            connect(slice, &SliceModel::nr2Post2RunChanged, ppRun, [ppRun](bool v) {
                QSignalBlocker b(ppRun); ppRun->setChecked(v);
            });
            connect(slice, &SliceModel::nr2Post2LevelChanged, ppLevel,
                    [ppLevel, ppLevelScale](double v) {
                QSignalBlocker b(ppLevel); ppLevel->setValue(static_cast<int>(v * ppLevelScale));
            });
            connect(slice, &SliceModel::nr2Post2FactorChanged, ppFactor,
                    [ppFactor, ppFactorScale](double v) {
                QSignalBlocker b(ppFactor); ppFactor->setValue(static_cast<int>(v * ppFactorScale));
            });
            connect(slice, &SliceModel::nr2Post2RateChanged, ppRate,
                    [ppRate, ppRateScale](double v) {
                QSignalBlocker b(ppRate); ppRate->setValue(static_cast<int>(v * ppRateScale));
            });
            connect(slice, &SliceModel::nr2Post2TaperChanged, ppTaper, [ppTaper](int v) {
                QSignalBlocker b(ppTaper); ppTaper->setValue(v);
            });
        }
    }

    // ── NR3 tab ───────────────────────────────────────────────────────────────
    // From Thetis setup.cs NR3 (RNNR) controls [v2.10.3.13].
    {
        auto [tabPage, tabLay] = makeTab(tabs, "NR3");
        Q_UNUSED(tabPage)

        QVBoxLayout* grpLay = makeGroup(tabLay, "NR3 (RNNoise)");

        // Position radio
        auto [preRdo, postRdo] = addPositionRow(grpLay);
        {
            const bool isPost = !slice || (slice->nr3Position() == NrPosition::PostAgc);
            preRdo->setChecked(!isPost);
            postRdo->setChecked(isPost);
        }

        // "Use fixed gain for input samples" — chkRXANR3FixedGain [v2.10.3.13]
        auto* fixedGainChk = new QCheckBox("Use fixed gain for input samples");
        fixedGainChk->setChecked(slice ? slice->nr3UseDefaultGain() : true);
        // Tooltip source: Thetis setup.cs:35460 chkRXANR3FixedGain [v2.10.3.13]
        fixedGainChk->setToolTip(tr("Use a fixed (rather than adaptive) input sample gain."));
        grpLay->addWidget(fixedGainChk);

        // Model selector group — GLOBAL (not per-slice), stored in AppSettings
        QVBoxLayout* mdlGrp = makeGroup(tabLay, "RNNoise Model (Global)");

        auto* modelLabel = new QLabel;
        modelLabel->setStyleSheet("QLabel { color: #00c8ff; font-size: 12px; }");
        {
            const QString saved = AppSettings::instance().value("Nr3ModelPath", "").toString();
            modelLabel->setText(saved.isEmpty() ? "Default (large)" : QFileInfo(saved).baseName());
        }
        mdlGrp->addWidget(modelLabel);

        auto* btnRow = new QHBoxLayout;
        auto* useModelBtn = new QPushButton("Use Model...");
        useModelBtn->setStyleSheet(
            "QPushButton { background: #1a2a3a; border: 1px solid #304050; "
            "border-radius: 3px; color: #c8d8e8; font-size: 12px; padding: 3px 10px; }"
            "QPushButton:hover { background: #203040; }"
            "QPushButton:pressed { background: #00b4d8; color: #0f0f1a; }");
        auto* defBtn = new QPushButton("Default");
        defBtn->setStyleSheet(useModelBtn->styleSheet());
        btnRow->addWidget(useModelBtn);
        btnRow->addWidget(defBtn);
        btnRow->addStretch(1);
        mdlGrp->addLayout(btnRow);

        tabLay->addStretch(1);

        // ── Wire NR3 controls → SliceModel / global ──────────────────────────
        connect(useModelBtn, &QPushButton::clicked, this, [this, modelLabel]() {
            const QString path = QFileDialog::getOpenFileName(
                this, tr("Choose RNNoise Model"), QString(), tr("Model Files (*.bin *.rnnn);;All files (*)"));
            if (path.isEmpty()) { return; }
            AppSettings::instance().setValue("Nr3ModelPath", path);
            RNNRloadModel(path.toStdString().c_str());
            modelLabel->setText(QFileInfo(path).baseName());
        });

        connect(defBtn, &QPushButton::clicked, this, [modelLabel]() {
            AppSettings::instance().setValue("Nr3ModelPath", "");
            RNNRloadModel("");
            modelLabel->setText("Default (large)");
        });

        if (slice) {
            connect(preRdo, &QRadioButton::toggled, slice, [slice](bool checked) {
                if (checked) { slice->setNr3Position(NrPosition::PreAgc); }
            });
            connect(postRdo, &QRadioButton::toggled, slice, [slice](bool checked) {
                if (checked) { slice->setNr3Position(NrPosition::PostAgc); }
            });
            connect(fixedGainChk, &QCheckBox::toggled, slice, &SliceModel::setNr3UseDefaultGain);

            // ── Model → UI ────────────────────────────────────────────────
            connect(slice, &SliceModel::nr3PositionChanged, preRdo,
                    [preRdo, postRdo](NrPosition p) {
                QSignalBlocker b1(preRdo), b2(postRdo);
                preRdo->setChecked(p == NrPosition::PreAgc);
                postRdo->setChecked(p == NrPosition::PostAgc);
            });
            connect(slice, &SliceModel::nr3UseDefaultGainChanged, fixedGainChk,
                    [fixedGainChk](bool v) {
                QSignalBlocker b(fixedGainChk); fixedGainChk->setChecked(v);
            });
        }
    }

    // ── NR4 tab ───────────────────────────────────────────────────────────────
    // From Thetis setup.cs NR4 (SBNR / SpecBleach) controls [v2.10.3.13].
    {
        auto [tabPage, tabLay] = makeTab(tabs, "NR4");
        Q_UNUSED(tabPage)

        QVBoxLayout* grpLay = makeGroup(tabLay, "NR4 (SpecBleach)");

        // Reduction — udDSPSBNRreduction: 0-20 step 0.1, default 10.0
        // Tooltip source: Thetis setup.cs udDSPSBNRreduction [v2.10.3.13]
        auto [reduction, reductionVal, reductionScale] = addDoubleSliderRow(grpLay, "Reduction",
            0.0, 20.0, slice ? slice->nr4Reduction() : 10.0,
            0.1, 1,
            tr("Spectral reduction amount in dB. Range 0-20, step 0.1."),
            " dB");

        // Smoothing — udDSPSBNRsmooth: 0-100 step 1, default 65.0
        // Tooltip source: Thetis setup.cs udDSPSBNRsmooth [v2.10.3.13]
        auto [smoothing, smoothingVal, smoothingScale] = addDoubleSliderRow(grpLay, "Smoothing",
            0.0, 100.0, slice ? slice->nr4Smoothing() : 65.0,
            1.0, 0,
            tr("Spectral smoothing factor. Range 0-100."),
            " %");

        // Whitening — udDSPSBNRwhiten: 0-100 step 1, default 2.0
        // Tooltip source: Thetis setup.cs udDSPSBNRwhiten [v2.10.3.13]
        auto [whitening, whiteningVal, whiteningScale] = addDoubleSliderRow(grpLay, "Whitening",
            0.0, 100.0, slice ? slice->nr4Whitening() : 2.0,
            1.0, 0,
            tr("Spectral whitening factor. Range 0-100."),
            " %");

        // Rescale — udDSPSBNRrescale: 0-12 step 0.1, default 2.0
        // Tooltip source: Thetis setup.cs udDSPSBNRrescale [v2.10.3.13]
        auto [rescale, rescaleVal, rescaleScale] = addDoubleSliderRow(grpLay, "Rescale",
            0.0, 12.0, slice ? slice->nr4Rescale() : 2.0,
            0.1, 1,
            tr("Output rescale factor. Range 0-12, step 0.1."),
            " dB");

        // SNR Threshold — udDSPSBNRsnrthresh: -10..10 step 0.5, default -10.0
        // Tooltip source: Thetis setup.cs udDSPSBNRsnrthresh [v2.10.3.13]
        auto [snrThresh, snrThreshVal, snrThreshScale] = addDoubleSliderRow(grpLay, "SNR Thresh",
            -10.0, 10.0, slice ? slice->nr4PostThresh() : -10.0,
            0.5, 1,
            tr("Post-processing SNR threshold. Range -10 to +10 dB, step 0.5."),
            " dB");

        // Algorithm radio — rdoSBNR1/2/3 [v2.10.3.13]
        const QString rdoStyle =
            "QRadioButton { color: #c8d8e8; font-size: 12px; }"
            "QRadioButton::indicator { width: 14px; height: 14px; }"
            "QRadioButton::indicator:unchecked { border: 2px solid #304050; "
            "border-radius: 7px; background: #1a2a3a; }"
            "QRadioButton::indicator:checked { border: 2px solid #00b4d8; "
            "border-radius: 7px; background: #00b4d8; }";

        auto* algo1 = new QRadioButton("Algo 1");
        auto* algo2 = new QRadioButton("Algo 2");
        auto* algo3 = new QRadioButton("Algo 3");
        algo1->setStyleSheet(rdoStyle);
        algo2->setStyleSheet(rdoStyle);
        algo3->setStyleSheet(rdoStyle);

        {
            const int algoIdx = slice ? static_cast<int>(slice->nr4Algo()) : 1; // Algo2 default
            if (algoIdx == 0) { algo1->setChecked(true); }
            else if (algoIdx == 2) { algo3->setChecked(true); }
            else { algo2->setChecked(true); }
        }

        auto* algoRow = new QHBoxLayout;
        auto* algoLbl = new QLabel("Algorithm");
        algoLbl->setStyleSheet(kLbl);
        algoLbl->setFixedWidth(150);
        algoRow->addWidget(algoLbl);
        algoRow->addWidget(algo1);
        algoRow->addWidget(algo2);
        algoRow->addWidget(algo3);
        algoRow->addStretch(1);
        grpLay->addLayout(algoRow);

        tabLay->addStretch(1);

        // ── Wire NR4 controls → SliceModel ──────────────────────────────────
        if (slice) {
            connect(reduction, &QSlider::valueChanged, slice, [slice, reductionScale](int v) {
                slice->setNr4Reduction(v / reductionScale);
            });
            connect(smoothing, &QSlider::valueChanged, slice, [slice, smoothingScale](int v) {
                slice->setNr4Smoothing(v / smoothingScale);
            });
            connect(whitening, &QSlider::valueChanged, slice, [slice, whiteningScale](int v) {
                slice->setNr4Whitening(v / whiteningScale);
            });
            connect(rescale, &QSlider::valueChanged, slice, [slice, rescaleScale](int v) {
                slice->setNr4Rescale(v / rescaleScale);
            });
            connect(snrThresh, &QSlider::valueChanged, slice, [slice, snrThreshScale](int v) {
                slice->setNr4PostThresh(v / snrThreshScale);
            });

            connect(algo1, &QRadioButton::toggled, slice, [slice](bool checked) {
                if (checked) { slice->setNr4Algo(SbnrAlgo::Algo1); }
            });
            connect(algo2, &QRadioButton::toggled, slice, [slice](bool checked) {
                if (checked) { slice->setNr4Algo(SbnrAlgo::Algo2); }
            });
            connect(algo3, &QRadioButton::toggled, slice, [slice](bool checked) {
                if (checked) { slice->setNr4Algo(SbnrAlgo::Algo3); }
            });

            // ── Model → UI ────────────────────────────────────────────────
            connect(slice, &SliceModel::nr4ReductionChanged, reduction,
                    [reduction, reductionScale](double v) {
                QSignalBlocker b(reduction); reduction->setValue(static_cast<int>(v * reductionScale));
            });
            connect(slice, &SliceModel::nr4SmoothingChanged, smoothing,
                    [smoothing, smoothingScale](double v) {
                QSignalBlocker b(smoothing); smoothing->setValue(static_cast<int>(v * smoothingScale));
            });
            connect(slice, &SliceModel::nr4WhiteningChanged, whitening,
                    [whitening, whiteningScale](double v) {
                QSignalBlocker b(whitening); whitening->setValue(static_cast<int>(v * whiteningScale));
            });
            connect(slice, &SliceModel::nr4RescaleChanged, rescale,
                    [rescale, rescaleScale](double v) {
                QSignalBlocker b(rescale); rescale->setValue(static_cast<int>(v * rescaleScale));
            });
            connect(slice, &SliceModel::nr4PostThreshChanged, snrThresh,
                    [snrThresh, snrThreshScale](double v) {
                QSignalBlocker b(snrThresh); snrThresh->setValue(static_cast<int>(v * snrThreshScale));
            });
            connect(slice, &SliceModel::nr4AlgoChanged, algo1,
                    [algo1, algo2, algo3](SbnrAlgo v) {
                QSignalBlocker b1(algo1), b2(algo2), b3(algo3);
                algo1->setChecked(v == SbnrAlgo::Algo1);
                algo2->setChecked(v == SbnrAlgo::Algo2);
                algo3->setChecked(v == SbnrAlgo::Algo3);
            });
        }
    }

    // ── DFNR tab ──────────────────────────────────────────────────────────────
    // AetherSDR-native DeepFilter NR (not in Thetis). HAVE_DFNR guards WDSP
    // integration; UI is always shown so users can understand the feature gate.
    {
        auto [tabPage, tabLay] = makeTab(tabs, "DFNR");
        Q_UNUSED(tabPage)

        QVBoxLayout* grpLay = makeGroup(tabLay, "DeepFilter NR");

#ifndef HAVE_DFNR
        // Feature not compiled in — show a helpful note and gray out the group.
        auto* note = new QLabel(
            "DFNR is not enabled in this build.\n"
            "Run ./setup-deepfilter.sh and rebuild to enable DeepFilter NR.");
        note->setStyleSheet(kInfoLbl);
        note->setWordWrap(true);
        grpLay->addWidget(note);
        grpLay->parentWidget()->setEnabled(false);
#endif

        // Attenuation Limit (0-100 dB) — use the shared addSliderRow helper
        // so the style matches NR1/NR2/NR4 (label | slider | value label).
        auto [attenSl, attenVal] = addSliderRow(
            grpLay, "Attenuation Limit", 0, 100,
            slice ? static_cast<int>(slice->dfnrAttenLimit()) : 100,
            tr("Maximum noise attenuation in dB (0 = bypass, 100 = maximum). "
               "Default 100. Higher values suppress more noise but may clip speech peaks."),
            " dB");

        // Post-Filter Beta (0.00-0.30 step 0.01) — use the double helper.
        auto [betaSl, betaVal, betaScale] = addDoubleSliderRow(
            grpLay, "Post-Filter Beta", 0.0, 0.30,
            slice ? slice->dfnrPostFilterBeta() : 0.0, 0.01, 2,
            tr("Post-filter aggressiveness (0 = disabled, 0.30 = maximum). Default 0. "
               "Higher values reduce residual musical-noise artifacts but may "
               "over-attenuate consonants."),
            QString());
        Q_UNUSED(attenVal); Q_UNUSED(betaVal);

        tabLay->addStretch(1);

        // ── Wire DFNR controls → SliceModel ─────────────────────────────────
        if (slice) {
            connect(attenSl, &QSlider::valueChanged, slice, [slice](int v) {
                slice->setDfnrAttenLimit(static_cast<double>(v));
            });
            connect(betaSl, &QSlider::valueChanged, slice, [slice, betaScale](int v) {
                slice->setDfnrPostFilterBeta(static_cast<double>(v) / betaScale);
            });

            // ── Model → UI ────────────────────────────────────────────────
            connect(slice, &SliceModel::dfnrAttenLimitChanged, attenSl, [attenSl](double v) {
                QSignalBlocker b(attenSl); attenSl->setValue(static_cast<int>(v));
            });
            connect(slice, &SliceModel::dfnrPostFilterBetaChanged, betaSl, [betaSl, betaScale](double v) {
                QSignalBlocker b(betaSl); betaSl->setValue(static_cast<int>(v * betaScale));
            });
        }
    }

    // ── MNR tab ───────────────────────────────────────────────────────────────
    // AetherSDR-native macOS noise reduction (not in Thetis). Always shown
    // so Windows/Linux users see the feature exists; controls grayed out on
    // non-Apple builds.
    {
        auto [tabPage, tabLay] = makeTab(tabs, "MNR");
        Q_UNUSED(tabPage)

        QVBoxLayout* grpLay = makeGroup(tabLay, "macOS NR (MNR)");

#ifndef Q_OS_APPLE
        auto* note = new QLabel(
            "MNR uses Apple Accelerate and is only available on macOS.\n"
            "On this platform the MNR controls are inactive.");
        note->setStyleSheet(kInfoLbl);
        note->setWordWrap(true);
        grpLay->addWidget(note);
#endif

        // Full 6-knob tuning surface matching the VFO right-click MNR popup
        // (see VfoWidget::showMnrPopup). Ranges + factory defaults identical.

        auto [strSl, strVal] = addSliderRow(
            grpLay, "Strength", 0, 200,
            slice ? static_cast<int>(slice->mnrStrength() * 100.0) : 100,
            tr("Dry/wet blend. 0%% = bypass (filter runs but output = input), "
               "100%% = full NR, 200%% = over-drive (phase-flip, destructive). "
               "Default 100%%."),
            QStringLiteral("%"));
        Q_UNUSED(strVal);

        auto [oversubSl, oversubVal] = addSliderRow(
            grpLay, "Aggressiveness", 1, 1000,
            slice ? static_cast<int>(slice->mnrOversub()) : 6,
            tr("MMSE-Wiener oversubtraction factor. Higher = more attenuation "
               "on low-SNR bins; 1 = gentle, 6 = default, 20+ = underwater."),
            QString());
        Q_UNUSED(oversubVal);

        auto [floorSl, floorVal] = addSliderRow(
            grpLay, "Floor", 0, 2000,
            slice ? static_cast<int>(slice->mnrFloor() * 1000.0) : 50,
            tr("Minimum Wiener gain per bin (x0.001). 0 = silence, "
               "50 = -26 dB (default), 1000 = 0 dB, 2000 = amplify."),
            QStringLiteral("m"));
        Q_UNUSED(floorVal);

        auto [alphaSl, alphaVal] = addSliderRow(
            grpLay, "Alpha", 0, 100,
            slice ? static_cast<int>(slice->mnrAlpha() * 100.0) : 92,
            tr("Decision-directed smoothing (x0.01). 0 = no smoothing "
               "(chattery), 92 = Ephraim-Malah classic (default), "
               "100 = frozen prior SNR."),
            QString());
        Q_UNUSED(alphaVal);

        auto [biasSl, biasVal] = addSliderRow(
            grpLay, "Bias", 0, 100,
            slice ? static_cast<int>(slice->mnrBias() * 10.0) : 15,
            tr("Min-statistics noise-floor bias (x0.1). <10 = underestimate "
               "noise (less NR), 15 = default, >30 = overestimate (erodes signal). "
               "Nudge up if NR is weak, down if it eats speech."),
            QString());
        Q_UNUSED(biasVal);

        auto [gsmoothSl, gsmoothVal] = addSliderRow(
            grpLay, "Gsmooth", 0, 100,
            slice ? static_cast<int>(slice->mnrGsmooth() * 100.0) : 70,
            tr("Temporal gain smoothing (x0.01). 0 = instant (musical noise), "
               "70 = balanced (default), 100 = frozen gain."),
            QString());
        Q_UNUSED(gsmoothVal);

#ifndef Q_OS_APPLE
        strSl->setEnabled(false);
        oversubSl->setEnabled(false);
        floorSl->setEnabled(false);
        alphaSl->setEnabled(false);
        biasSl->setEnabled(false);
        gsmoothSl->setEnabled(false);
#endif

        tabLay->addStretch(1);

        // ── Wire MNR controls → SliceModel ──────────────────────────────────
#ifdef Q_OS_APPLE
        if (slice) {
            connect(strSl, &QSlider::valueChanged, slice, [slice](int v) {
                slice->setMnrStrength(static_cast<double>(v) / 100.0);
            });
            connect(slice, &SliceModel::mnrStrengthChanged, strSl, [strSl](double v) {
                QSignalBlocker b(strSl); strSl->setValue(static_cast<int>(v * 100.0));
            });

            connect(oversubSl, &QSlider::valueChanged, slice, [slice](int v) {
                slice->setMnrOversub(static_cast<double>(v));
            });
            connect(slice, &SliceModel::mnrOversubChanged, oversubSl, [oversubSl](double v) {
                QSignalBlocker b(oversubSl); oversubSl->setValue(static_cast<int>(v));
            });

            connect(floorSl, &QSlider::valueChanged, slice, [slice](int v) {
                slice->setMnrFloor(static_cast<double>(v) * 0.001);
            });
            connect(slice, &SliceModel::mnrFloorChanged, floorSl, [floorSl](double v) {
                QSignalBlocker b(floorSl); floorSl->setValue(static_cast<int>(v * 1000.0));
            });

            connect(alphaSl, &QSlider::valueChanged, slice, [slice](int v) {
                slice->setMnrAlpha(static_cast<double>(v) * 0.01);
            });
            connect(slice, &SliceModel::mnrAlphaChanged, alphaSl, [alphaSl](double v) {
                QSignalBlocker b(alphaSl); alphaSl->setValue(static_cast<int>(v * 100.0));
            });

            connect(biasSl, &QSlider::valueChanged, slice, [slice](int v) {
                slice->setMnrBias(static_cast<double>(v) * 0.1);
            });
            connect(slice, &SliceModel::mnrBiasChanged, biasSl, [biasSl](double v) {
                QSignalBlocker b(biasSl); biasSl->setValue(static_cast<int>(v * 10.0));
            });

            connect(gsmoothSl, &QSlider::valueChanged, slice, [slice](int v) {
                slice->setMnrGsmooth(static_cast<double>(v) * 0.01);
            });
            connect(slice, &SliceModel::mnrGsmoothChanged, gsmoothSl, [gsmoothSl](double v) {
                QSignalBlocker b(gsmoothSl); gsmoothSl->setValue(static_cast<int>(v * 100.0));
            });
        }
#endif
    }

    // ── ANF tab ───────────────────────────────────────────────────────────────
    // From Thetis setup.designer.cs — chkDSPANFEnable [v2.10.3.13].
    // Advanced ANF tuning (Taps/Delay/Gain/Leakage) is not yet in SliceModel;
    // deferred to a future phase. This tab wires the Enable toggle only.
    {
        auto [tabPage, tabLay] = makeTab(tabs, "ANF");
        Q_UNUSED(tabPage)

        QVBoxLayout* grpLay = makeGroup(tabLay, "Adaptive Noise Filter");

        // Note: NrSlot::Off means ANF is included as part of the NR selector
        // but ANF itself doesn't have a dedicated NrSlot — in Thetis ANF is a
        // parallel stage enabled independently of the NR slot selection.
        // For now expose an informational note and a placeholder Enable toggle
        // that will be wired once SliceModel gains anfEnabled.
        auto* note = new QLabel(
            "ANF is available via the VFO popup ANF button.\n"
            "Advanced tuning (Taps, Delay, Gain, Leakage) will be added in a future phase.\n"
            "Enable toggle below mirrors the VFO ANF button state (not yet wired).");
        note->setStyleSheet(kInfoLbl);
        note->setWordWrap(true);
        grpLay->addWidget(note);

        // Placeholder Enable toggle — from Thetis chkDSPANFEnable [v2.10.3.13].
        // NYI: SliceModel does not yet expose anfEnabled / setAnfEnabled.
        // Deferred to the same phase that adds Taps/Delay/Gain/Leakage to SliceModel.
        auto* anfEnableChk = new QCheckBox("Enable ANF");
        anfEnableChk->setChecked(false);
        // Tooltip source: Thetis setup.designer.cs chkDSPANFEnable [v2.10.3.13]
        anfEnableChk->setToolTip(tr("Enable Adaptive Notch Filter. Full ANF tuning (Taps/Delay/"
                                    "Gain/Leakage) coming in a future phase."));
        anfEnableChk->setEnabled(false);  // NYI until SliceModel has anfEnabled
        grpLay->addWidget(anfEnableChk);

        tabLay->addStretch(1);
        // No wiring: SliceModel::anfEnabled does not yet exist (Task 17 scope).
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// NbSnbSetupPage
// ══════════════════════════════════════════════════════════════════════════════
//
// From Thetis setup.cs — tabDSP / tabPageNoiseBlanker controls:
//   tbNB1Threshold, comboNB1Mode, tbNB2Threshold,
//   tbSNBK1, tbSNBK2, udSNBOutputBW
//
NbSnbSetupPage::NbSnbSetupPage(RadioModel* model, QWidget* parent)
    : SetupPage("NB/SNB", model, parent)
{
    // Sliders with inline live-value labels. Tooltips use Thetis's own user-
    // facing text (from setup.designer.cs ToolTip attributes [v2.10.3.13]).
    // Ranges / defaults mirror Thetis NumericUpDown widgets byte-for-byte.
    auto& s = AppSettings::instance();

    // Gate: the WDSP SetEXT* / SetRXASNBA* setters dereference
    // panb[0]/pnob[0]/channels[0] — if no RX channel has been created
    // (user opened Setup dialog before connecting to a radio), calling
    // them will null-deref inside WDSP. The slider value is still
    // persisted via AppSettings and seeded into NbFamily at channel
    // create time, so "Setup → change before connect" still takes effect
    // on next connect. This just stops the crash.
    // (Codex review #120, P1 — 2026-04-23.)
    auto channelReady = [this]() -> bool {
        auto* rm = this->model();
        if (!rm || !rm->wdspEngine()) { return false; }
        return rm->wdspEngine()->rxChannel(0) != nullptr;
    };

    // Helper: integer slider, live value label showing "value / max" with unit.
    // Returns the slider so caller can wire valueChanged.
    auto addIntSlider = [this](QVBoxLayout* parent, const QString& label,
                               int minVal, int maxVal, int value,
                               const QString& unitSuffix,
                               const QString& tooltip) -> QSlider*
    {
        auto* sl = new QSlider(Qt::Horizontal);
        sl->setRange(minVal, maxVal);
        sl->setValue(value);
        sl->setToolTip(tooltip);
        auto* valLbl = new QLabel;
        valLbl->setMinimumWidth(80);
        valLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto renderInt = [valLbl, maxVal, unitSuffix](int v) {
            valLbl->setText(QStringLiteral("%1 / %2%3")
                .arg(v).arg(maxVal).arg(unitSuffix));
        };
        renderInt(value);
        QObject::connect(sl, &QSlider::valueChanged, valLbl, renderInt);
        this->addLabeledSlider(parent, label, sl, valLbl);
        return sl;
    };

    // Helper: "decimal" slider. Slider uses integer internally at the chosen
    // scale (e.g. ×100 for 0.01 step); value label renders at the real scale
    // with the given unit. Returns {slider, scaleDivisor}.
    auto addScaledSlider = [this](QVBoxLayout* parent, const QString& label,
                                  int sliderMin, int sliderMax, int sliderValue,
                                  double divisor, int decimals,
                                  const QString& unitSuffix,
                                  const QString& tooltip) -> QSlider*
    {
        auto* sl = new QSlider(Qt::Horizontal);
        sl->setRange(sliderMin, sliderMax);
        sl->setValue(sliderValue);
        sl->setToolTip(tooltip);
        auto* valLbl = new QLabel;
        valLbl->setMinimumWidth(110);
        valLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        const double maxReal = sliderMax / divisor;
        auto render = [valLbl, decimals, divisor, maxReal, unitSuffix](int v) {
            valLbl->setText(QStringLiteral("%1 / %2%3")
                .arg(v / divisor, 0, 'f', decimals)
                .arg(maxReal, 0, 'f', decimals)
                .arg(unitSuffix));
        };
        render(sliderValue);
        QObject::connect(sl, &QSlider::valueChanged, valLbl, render);
        this->addLabeledSlider(parent, label, sl, valLbl);
        return sl;
    };

    // ── NB1 ───────────────────────────────────────────────────────────────────
    // Thetis grpDSPNB (setup.designer.cs:44399-44604 [v2.10.3.13]).
    QGroupBox* nb1Grp = addSection("NB1");
    QVBoxLayout* nb1Lay = qobject_cast<QVBoxLayout*>(nb1Grp->layout());

    // Threshold — udDSPNB: 1-1000, default 30. WDSP threshold = 0.165 × value.
    QSlider* nb1Thresh = addIntSlider(nb1Lay, tr("Threshold"),
        1, 1000,
        s.value(QStringLiteral("NbDefaultThresholdSlider"), 30).toInt(),
        QString{},
        tr("Controls the detection threshold for impulse noise.\n"
           "Lower = more aggressive (blanks weaker impulses too).\n"
           "Higher = more conservative (only strong clicks get blanked)."));
    connect(nb1Thresh, &QSlider::valueChanged, [channelReady](int v) {
        AppSettings::instance().setValue(QStringLiteral("NbDefaultThresholdSlider"), v);
        if (channelReady()) { SetEXTANBThreshold(0, 0.165 * static_cast<double>(v)); }
    });

    // Transition — udDSPNBTransition: 0.01-2.00 ms, step 0.01, default 0.01.
    // Slider internal: 1-200 (×100 scale). Label shows "X.XX / 2.00 ms".
    QSlider* nb1Trans = addScaledSlider(nb1Lay, tr("Transition"),
        1, 200,
        s.value(QStringLiteral("NbDefaultTransition"), 1).toInt(),
        100.0, 2, tr(" ms"),
        tr("Time to decrease/increase to/from zero amplitude around an\n"
           "impulse. Controls how gradually the blanker fades in and out\n"
           "— very short = crisp click; longer = gentler but audible."));
    connect(nb1Trans, &QSlider::valueChanged, [channelReady](int v) {
        AppSettings::instance().setValue(QStringLiteral("NbDefaultTransition"), v);
        // v/100 ms → × 0.001 s = v * 1e-5 s.
        if (channelReady()) { SetEXTANBTau(0, static_cast<double>(v) * 1e-5); }
    });

    // Lead — udDSPNBLead: 0.01-2.00 ms, default 0.01.
    QSlider* nb1Lead = addScaledSlider(nb1Lay, tr("Lead"),
        1, 200,
        s.value(QStringLiteral("NbDefaultLead"), 1).toInt(),
        100.0, 2, tr(" ms"),
        tr("Time at zero amplitude BEFORE the detected impulse. Blanks\n"
           "the leading edge of the click that the detector would\n"
           "otherwise miss. Raise slightly if clicks still get through."));
    connect(nb1Lead, &QSlider::valueChanged, [channelReady](int v) {
        AppSettings::instance().setValue(QStringLiteral("NbDefaultLead"), v);
        if (channelReady()) { SetEXTANBAdvtime(0, static_cast<double>(v) * 1e-5); }
    });

    // Lag — udDSPNBLag: 0.01-2.00 ms, default 0.01.
    QSlider* nb1Lag = addScaledSlider(nb1Lay, tr("Lag"),
        1, 200,
        s.value(QStringLiteral("NbDefaultLag"), 1).toInt(),
        100.0, 2, tr(" ms"),
        tr("Time to remain at zero amplitude AFTER the impulse. Blanks\n"
           "the decay tail of the click. Raise this if pops still have\n"
           "an audible ringing after the initial transient."));
    connect(nb1Lag, &QSlider::valueChanged, [channelReady](int v) {
        AppSettings::instance().setValue(QStringLiteral("NbDefaultLag"), v);
        if (channelReady()) { SetEXTANBHangtime(0, static_cast<double>(v) * 1e-5); }
    });

    // NB2 Mode — Thetis comboDSPNOBmode.
    auto* nb1Mode = new QComboBox;
    // Item text matches Thetis comboDSPNOBmode verbatim (setup.designer.cs:44434 [v2.10.3.13]).
    nb1Mode->addItems({tr("Zero"), tr("Sample && Hold"), tr("Mean-Hold"),
                       tr("Hold && Sample"), tr("Linear Interpolate")});
    nb1Mode->setCurrentIndex(s.value(QStringLiteral("Nb2DefaultMode"), 0).toInt());
    nb1Mode->setToolTip(tr(
        "Method used to fill in the blanked samples when NB2 triggers.\n"
        "Zero silences the impulse entirely; the other modes synthesise\n"
        "a replacement waveform from surrounding samples to reduce\n"
        "audible artifacts on voice peaks."));
    addLabeledCombo(nb1Lay, "NB2 Mode", nb1Mode);
    connect(nb1Mode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [channelReady](int v) {
        AppSettings::instance().setValue(QStringLiteral("Nb2DefaultMode"), v);
        if (channelReady()) { SetEXTNOBMode(0, v); }
    });

    // ── NB2 Threshold — intentionally absent (Thetis parity) ─────────────────
    // Thetis has no NB2 threshold UI. NB2 runs at cmaster.c:68 [v2.10.3.13]
    // hardcoded default of 30.0.

    // ── SNB ───────────────────────────────────────────────────────────────────
    // Thetis grpDSPSNB (setup.designer.cs:44280-44398 [v2.10.3.13]).
    QGroupBox* snbGrp = addSection("SNB");
    QVBoxLayout* snbLay = qobject_cast<QVBoxLayout*>(snbGrp->layout());

    // Threshold 1 — udDSPSNBThresh1: 2.0-20.0, step 0.1, default 8.0.
    // Slider internal: 20-200 (×10 scale).
    QSlider* snbK1 = addScaledSlider(snbLay, tr("Threshold 1"),
        20, 200,
        qRound(s.value(QStringLiteral("SnbDefaultK1"), 8.0).toDouble() * 10.0),
        10.0, 1, QString{},
        tr("Multiple of the running noise power at which a sample is\n"
           "flagged as a candidate outlier. Lower = more aggressive\n"
           "first-pass detection; higher = miss weaker noise."));
    connect(snbK1, &QSlider::valueChanged, [channelReady](int v) {
        const double real = static_cast<double>(v) / 10.0;
        AppSettings::instance().setValue(QStringLiteral("SnbDefaultK1"), real);
        if (channelReady()) { SetRXASNBAk1(0, real); }
    });

    // Threshold 2 — udDSPSNBThresh2: 4.0-60.0, step 0.1, default 20.0.
    // Slider internal: 40-600 (×10 scale).
    QSlider* snbK2 = addScaledSlider(snbLay, tr("Threshold 2"),
        40, 600,
        qRound(s.value(QStringLiteral("SnbDefaultK2"), 20.0).toDouble() * 10.0),
        10.0, 1, QString{},
        tr("Multiplier applied to the final detection threshold — confirms\n"
           "candidates from Threshold 1 as real noise outliers. Lower =\n"
           "more aggressive overall blanking; higher = fewer false triggers\n"
           "on genuine voice peaks."));
    connect(snbK2, &QSlider::valueChanged, [channelReady](int v) {
        const double real = static_cast<double>(v) / 10.0;
        AppSettings::instance().setValue(QStringLiteral("SnbDefaultK2"), real);
        if (channelReady()) { SetRXASNBAk2(0, real); }
    });

    // SNB Output Bandwidth — NOT in Thetis Setup page. Thetis sets it
    // automatically per mode in rxa.cs:112-124. Kept as a NereusSDR-native
    // global override.
    QSlider* snbOutBw = addIntSlider(snbLay, tr("Output Bandwidth"),
        100, 96000,
        s.value(QStringLiteral("SnbDefaultOutputBW"), 6000).toInt(),
        tr(" Hz"),
        tr("Width of the audio band SNB operates on, centred on zero.\n"
           "Smaller = focuses the blanker on the active passband;\n"
           "larger = covers wider modes (FM, DRM). Default 6000 Hz\n"
           "covers SSB + AM comfortably."));
    connect(snbOutBw, &QSlider::valueChanged, [channelReady](int v) {
        AppSettings::instance().setValue(QStringLiteral("SnbDefaultOutputBW"), v);
        const double half = static_cast<double>(v) / 2.0;
        if (channelReady()) { SetRXASNBAOutputBandwidth(0, -half, half); }
    });
}

// ══════════════════════════════════════════════════════════════════════════════
// CwSetupPage
// ══════════════════════════════════════════════════════════════════════════════
//
// From Thetis setup.cs — tabDSP / tabPageCW controls:
//   comboCWKeyerMode, udCWKeyerWeight, tbCWLetterSpacing, tbCWDotDashRatio,
//   udCWSemiBreakInDelay, tbCWSidetoneVolume,
//   chkAPFEnable, udAPFFreq, udAPFBandwidth, tbAPFGain
//
CwSetupPage::CwSetupPage(RadioModel* model, QWidget* parent)
    : SetupPage("CW", model, parent)
{
    // ── Keyer ─────────────────────────────────────────────────────────────────
    QGroupBox* keyerGrp = addSection("Keyer");
    QVBoxLayout* keyerLay = qobject_cast<QVBoxLayout*>(keyerGrp->layout());

    auto* keyerMode = new QComboBox;
    keyerMode->addItems({"Iambic A", "Iambic B", "Bug", "Straight"});
    addLabeledCombo(keyerLay, "Mode", keyerMode);

    auto* keyerWeight = new QSpinBox;
    keyerWeight->setRange(10, 90);
    addLabeledSpinner(keyerLay, "Weight", keyerWeight);

    auto* letterSpacing = new QSlider(Qt::Horizontal);
    letterSpacing->setRange(0, 100);
    addLabeledSlider(keyerLay, "Letter Spacing", letterSpacing);

    auto* dotDashRatio = new QSlider(Qt::Horizontal);
    dotDashRatio->setRange(100, 500);
    addLabeledSlider(keyerLay, "Dot-Dash Ratio", dotDashRatio);

    disableGroup(keyerGrp);

    // ── Timing ────────────────────────────────────────────────────────────────
    QGroupBox* timingGrp = addSection("Timing");
    QVBoxLayout* timingLay = qobject_cast<QVBoxLayout*>(timingGrp->layout());

    auto* semiDelay = new QSlider(Qt::Horizontal);
    semiDelay->setRange(0, 2000);
    addLabeledSlider(timingLay, "Semi-Break-In Delay (ms)", semiDelay);

    // P1 full-parity §4.2 — Sidetone Volume row is wrapped in a single
    // QWidget so we can visibility-gate the whole row (label + slider) on
    // BoardCapabilities.hasSidetoneGenerator via setHasSidetoneGenerator().
    // We bypass the addLabeledSlider() helper here because it stitches the
    // label and slider into a QHBoxLayout that's added to timingLay
    // directly — there's no row-level QWidget to toggle.  Default:
    // hidden until a board with hasSidetoneGenerator=true connects.
    //
    // Thetis upstream comparison (setup.cs [v2.10.3.13 @501e3f51]
    // chkSideTones / chkDSPKeyerSidetone[_software]):  Thetis does NOT
    // board-gate its sidetone controls — they're always visible and
    // mutually-exclusive via the toggle logic at setup.cs:8823-8854.
    // This visibility gate is NereusSDR-specific use of the populated
    // hasSidetoneGenerator flag, not a port of an upstream gate.
    m_sidetoneRow = new QWidget;
    auto* sidetoneRowLay = new QHBoxLayout(m_sidetoneRow);
    sidetoneRowLay->setContentsMargins(0, 0, 0, 0);
    sidetoneRowLay->setSpacing(8);
    auto* sidetoneLbl = new QLabel("Sidetone Volume");
    sidetoneLbl->setFixedWidth(150);
    // Match SetupPage::makeLabeledRow label styling (kLabelStyle in
    // SetupPage.cpp).  Inlined here because the helper is file-scope.
    sidetoneLbl->setStyleSheet(QStringLiteral(
        "QLabel { color: #c8d8e8; font-size: 12px; }"));
    auto* sidetoneVol = new QSlider(Qt::Horizontal);
    sidetoneVol->setRange(0, 100);
    sidetoneVol->setStyleSheet(
        "QSlider::groove:horizontal { background: #1a2a3a; height: 4px; "
        "border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #00b4d8; width: 12px; "
        "height: 12px; border-radius: 6px; margin: -4px 0; }");
    sidetoneRowLay->addWidget(sidetoneLbl);
    sidetoneRowLay->addWidget(sidetoneVol, 1);
    timingLay->addWidget(m_sidetoneRow);
    m_sidetoneRow->setVisible(false);

    disableGroup(timingGrp);

    // ── APF ───────────────────────────────────────────────────────────────────
    QGroupBox* apfGrp = addSection("APF");
    QVBoxLayout* apfLay = qobject_cast<QVBoxLayout*>(apfGrp->layout());

    auto* apfEnable = new QPushButton("Enable");
    addLabeledToggle(apfLay, "Enable", apfEnable);

    auto* apfCenter = new QSlider(Qt::Horizontal);
    apfCenter->setRange(200, 3000);
    addLabeledSlider(apfLay, "Center Freq", apfCenter);

    auto* apfBw = new QSlider(Qt::Horizontal);
    apfBw->setRange(10, 500);
    addLabeledSlider(apfLay, "Bandwidth", apfBw);

    auto* apfGain = new QSlider(Qt::Horizontal);
    apfGain->setRange(0, 100);
    addLabeledSlider(apfLay, "Gain", apfGain);

    disableGroup(apfGrp);

    // P1 full-parity §4.2 — subscribe to model so the Sidetone Volume row
    // visibility reflects the connected board's hasSidetoneGenerator flag
    // automatically.  SetupDialog is constructed lazily on every Setup
    // menu click (see SetupDialog.cpp) so this page can't be poked from
    // MainWindow's connectionStateChanged slot — instead it tracks model
    // signals itself, mirroring HardwarePage's same-pattern wiring at
    // HardwarePage.cpp:144-153.
    if (model) {
        connect(model, &RadioModel::currentRadioChanged,
                this, [this](const NereusSDR::RadioInfo& info) {
            const auto& caps = NereusSDR::BoardCapsTable::forBoard(info.boardType);
            setHasSidetoneGenerator(caps.hasSidetoneGenerator);
        });
        connect(model, &RadioModel::connectionStateChanged,
                this, [this](NereusSDR::ConnectionState s) {
            if (s != NereusSDR::ConnectionState::Connected) {
                // On any non-Connected transition (Disconnected /
                // Connecting / Disconnecting / Error), hide the row so
                // the user doesn't see a useless control between
                // sessions.
                setHasSidetoneGenerator(false);
            }
        });
        // Apply current state — the dialog may have been opened while a
        // radio is already connected.
        if (model->isConnected() && model->connection()) {
            const auto& caps = NereusSDR::BoardCapsTable::forBoard(
                model->connection()->radioInfo().boardType);
            setHasSidetoneGenerator(caps.hasSidetoneGenerator);
        }
    }
}

void CwSetupPage::setHasSidetoneGenerator(bool on)
{
    if (m_sidetoneRow != nullptr) {
        m_sidetoneRow->setVisible(on);
    }
}

bool CwSetupPage::sidetoneRowVisibleForTest() const
{
    return m_sidetoneRow != nullptr && !m_sidetoneRow->isHidden();
}

// ══════════════════════════════════════════════════════════════════════════════
// AmSamSetupPage
// ══════════════════════════════════════════════════════════════════════════════
//
// From Thetis setup.cs — tabDSP / tabPageAMSAM controls:
//   tbAMTXCarrierLevel, comboSAMFadeLevel, comboSAMDSBMode,
//   tbAMSquelchThreshold, udAMSquelchMaxTail
//
AmSamSetupPage::AmSamSetupPage(RadioModel* model, QWidget* parent)
    : SetupPage("AM/SAM", model, parent)
{
    // ── AM TX ─────────────────────────────────────────────────────────────────
    QGroupBox* amGrp = addSection("AM TX");
    QVBoxLayout* amLay = qobject_cast<QVBoxLayout*>(amGrp->layout());

    auto* carrierLevel = new QSlider(Qt::Horizontal);
    carrierLevel->setRange(0, 100);
    addLabeledSlider(amLay, "Carrier Level", carrierLevel);

    disableGroup(amGrp);

    // ── SAM ───────────────────────────────────────────────────────────────────
    QGroupBox* samGrp = addSection("SAM");
    QVBoxLayout* samLay = qobject_cast<QVBoxLayout*>(samGrp->layout());

    auto* fadeLevel = new QComboBox;
    fadeLevel->addItems({"0", "1", "2", "3", "4", "5"});
    addLabeledCombo(samLay, "Fade Level", fadeLevel);

    auto* dsbMode = new QComboBox;
    dsbMode->addItems({"LSB", "USB", "Both"});
    addLabeledCombo(samLay, "DSB Mode", dsbMode);

    disableGroup(samGrp);

    // ── Squelch ───────────────────────────────────────────────────────────────
    QGroupBox* sqGrp = addSection("Squelch");
    QVBoxLayout* sqLay = qobject_cast<QVBoxLayout*>(sqGrp->layout());

    auto* sqThresh = new QSlider(Qt::Horizontal);
    sqThresh->setRange(-160, 0);
    addLabeledSlider(sqLay, "AM Squelch Threshold", sqThresh);

    auto* sqMaxTail = new QSpinBox;
    sqMaxTail->setRange(1, 1000);
    sqMaxTail->setSuffix(" ms");
    addLabeledSpinner(sqLay, "Max Tail", sqMaxTail);

    disableGroup(sqGrp);
}

// ══════════════════════════════════════════════════════════════════════════════
// FmSetupPage
// ══════════════════════════════════════════════════════════════════════════════
//
// From Thetis setup.cs — tabDSP / tabPageFM controls:
//   comboFMDeviation, tbFMSquelchThreshold, chkFMDeEmphasis,
//   comboFMTXDeviation, tbFMMicGain, comboFMTXEmphasisPosition
//
FmSetupPage::FmSetupPage(RadioModel* model, QWidget* parent)
    : SetupPage("FM", model, parent)
{
    // ── RX ────────────────────────────────────────────────────────────────────
    QGroupBox* rxGrp = addSection("RX");
    QVBoxLayout* rxLay = qobject_cast<QVBoxLayout*>(rxGrp->layout());

    auto* rxDeviation = new QComboBox;
    rxDeviation->addItems({"5k", "2.5k"});
    addLabeledCombo(rxLay, "Deviation", rxDeviation);

    auto* squelchThresh = new QSlider(Qt::Horizontal);
    squelchThresh->setRange(-160, 0);
    addLabeledSlider(rxLay, "Squelch Threshold", squelchThresh);

    auto* deEmphasis = new QPushButton("Enable");
    addLabeledToggle(rxLay, "De-Emphasis", deEmphasis);

    disableGroup(rxGrp);

    // ── TX ────────────────────────────────────────────────────────────────────
    QGroupBox* txGrp = addSection("TX");
    QVBoxLayout* txLay = qobject_cast<QVBoxLayout*>(txGrp->layout());

    auto* txDeviation = new QComboBox;
    txDeviation->addItems({"5k", "2.5k"});
    addLabeledCombo(txLay, "Deviation", txDeviation);

    auto* micGain = new QSlider(Qt::Horizontal);
    micGain->setRange(0, 100);
    addLabeledSlider(txLay, "Mic Gain", micGain);

    auto* emphasisPos = new QComboBox;
    emphasisPos->addItems({"Pre-EQ", "Post-EQ"});
    addLabeledCombo(txLay, "Emphasis Position", emphasisPos);

    disableGroup(txGrp);
}

// ══════════════════════════════════════════════════════════════════════════════
// (VoxDexpSetupPage placeholder removed in 3M-3a-iii Task 16 — the wired
//  page lives at Setup → Transmit → DEXP/VOX (DexpVoxPage from Task 14).
//  The old stub had 4 disabled placeholder sliders and never wired to
//  anything; two leaves with similar names under different categories was
//  confusing, so the stub was retired.)
// ══════════════════════════════════════════════════════════════════════════════

// ══════════════════════════════════════════════════════════════════════════════
// CfcSetupPage
// ══════════════════════════════════════════════════════════════════════════════
//
// Phase 3M-3a-ii Batch 5 (Task E): replaces the prior 30-line placeholder with
// a fully-wired three-group page that mirrors Thetis tpDSPCFC tab layout 1:1.
// Bidirectional binding to TransmitModel via QSignalBlocker guards prevents
// feedback loops; ranges / defaults / tooltip text all match the Thetis
// designer file.
//
// Source-first cites:
//   Phase Rotator group  — setup.Designer.cs:46162-46280 [v2.10.3.13]
//   CFC ranges           — TransmitModel kCfc* constants (Batch 2 schema)
//   CESSB→CPDR gate note — wdsp/TXA.c:843-851 [v2.10.3.13] (bp2 ladder)
//
CfcSetupPage::CfcSetupPage(RadioModel* model, QWidget* parent)
    : SetupPage("CFC", model, parent)
{
    if (!model) {
        // No model — show disabled placeholder (consistent with AgcAlcSetupPage).
        QGroupBox* grp = addSection("Phase Rotator");
        disableGroup(grp);
        return;
    }

    TransmitModel& tx = model->transmitModel();

    // ══════════════════════════════════════════════════════════════════════════
    // ── Group 1: Phase Rotator ────────────────────────────────────────────────
    // ══════════════════════════════════════════════════════════════════════════
    //
    // From Thetis setup.Designer.cs:46162-46280 [v2.10.3.13] — grpPhRot.
    // Four controls: chkPHROTEnable / udPhRotFreq / udPHROTStages /
    // chkPHROTReverse.  Tooltip strings copied verbatim from designer file.
    //
    QGroupBox* phRotGrp = addSection("Phase Rotator");
    QVBoxLayout* phRotLay = qobject_cast<QVBoxLayout*>(phRotGrp->layout());

    m_phRotEnableChk = new QCheckBox("Enable");
    m_phRotEnableChk->setObjectName(QStringLiteral("chkPHROTEnable"));
    m_phRotEnableChk->setChecked(tx.phaseRotatorEnabled());
    // From Thetis setup.Designer.cs:46281 [v2.10.3.13] — chkPHROTEnable tooltip.
    m_phRotEnableChk->setToolTip(QStringLiteral("Turn the phase rotator on or off"));
    phRotLay->addWidget(m_phRotEnableChk);

    m_phRotFreqSpin = new QSpinBox;
    m_phRotFreqSpin->setObjectName(QStringLiteral("udPhRotFreq"));
    m_phRotFreqSpin->setRange(TransmitModel::kPhaseRotatorFreqHzMin,
                              TransmitModel::kPhaseRotatorFreqHzMax);
    m_phRotFreqSpin->setSuffix(" Hz");
    m_phRotFreqSpin->setValue(tx.phaseRotatorFreqHz());
    // From Thetis setup.Designer.cs:46264 [v2.10.3.13] — udPhRotFreq tooltip.
    m_phRotFreqSpin->setToolTip(
        QStringLiteral("Set rotation frequency in Hz. (default 338)"));
    addLabeledSpinner(phRotLay, "FREQ", m_phRotFreqSpin);

    m_phRotStagesSpin = new QSpinBox;
    m_phRotStagesSpin->setObjectName(QStringLiteral("udPHROTStages"));
    m_phRotStagesSpin->setRange(TransmitModel::kPhaseRotatorStagesMin,
                                TransmitModel::kPhaseRotatorStagesMax);
    m_phRotStagesSpin->setValue(tx.phaseRotatorStages());
    // From Thetis setup.Designer.cs:46223 [v2.10.3.13] — udPHROTStages tooltip.
    m_phRotStagesSpin->setToolTip(
        QStringLiteral("Choose the number of rotation stages. Larger values = "
                       "smoother (default 8)"));
    addLabeledSpinner(phRotLay, "STAGES", m_phRotStagesSpin);

    m_phRotReverseChk = new QCheckBox("Reverse Phase");
    m_phRotReverseChk->setObjectName(QStringLiteral("chkPHROTReverse"));
    m_phRotReverseChk->setChecked(tx.phaseReverseEnabled());
    // From Thetis setup.Designer.cs:46186 [v2.10.3.13] — chkPHROTReverse tooltip.
    m_phRotReverseChk->setToolTip(QStringLiteral("Reverses the phase"));
    phRotLay->addWidget(m_phRotReverseChk);

    // ── Wiring: PhRot widgets ↔ TransmitModel ────────────────────────────────
    connect(m_phRotEnableChk, &QCheckBox::toggled,
            &tx, &TransmitModel::setPhaseRotatorEnabled);
    connect(&tx, &TransmitModel::phaseRotatorEnabledChanged,
            m_phRotEnableChk, [this](bool on) {
        QSignalBlocker b(m_phRotEnableChk);
        m_phRotEnableChk->setChecked(on);
    });

    connect(m_phRotFreqSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            &tx, &TransmitModel::setPhaseRotatorFreqHz);
    connect(&tx, &TransmitModel::phaseRotatorFreqHzChanged,
            m_phRotFreqSpin, [this](int hz) {
        QSignalBlocker b(m_phRotFreqSpin);
        m_phRotFreqSpin->setValue(hz);
    });

    connect(m_phRotStagesSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            &tx, &TransmitModel::setPhaseRotatorStages);
    connect(&tx, &TransmitModel::phaseRotatorStagesChanged,
            m_phRotStagesSpin, [this](int n) {
        QSignalBlocker b(m_phRotStagesSpin);
        m_phRotStagesSpin->setValue(n);
    });

    connect(m_phRotReverseChk, &QCheckBox::toggled,
            &tx, &TransmitModel::setPhaseReverseEnabled);
    connect(&tx, &TransmitModel::phaseReverseEnabledChanged,
            m_phRotReverseChk, [this](bool on) {
        QSignalBlocker b(m_phRotReverseChk);
        m_phRotReverseChk->setChecked(on);
    });

    // ══════════════════════════════════════════════════════════════════════════
    // ── Group 2: CFC (Continuous Frequency Compressor) ────────────────────────
    // ══════════════════════════════════════════════════════════════════════════
    //
    // Global CFC controls: Enable / Post-EQ Enable / PreComp dB / PostEqGain dB.
    // Per-band CFC editing (10-band ParaEQ + compression sliders) lives in
    // TxCfcDialog (Batch 6) — the [Configure CFC bands…] button below emits
    // openCfcDialogRequested for the parent SetupDialog to route.
    //
    QGroupBox* cfcGrp = addSection("CFC");
    QVBoxLayout* cfcLay = qobject_cast<QVBoxLayout*>(cfcGrp->layout());

    m_cfcEnableChk = new QCheckBox("Enable");
    m_cfcEnableChk->setObjectName(QStringLiteral("chkCFCEnable"));
    m_cfcEnableChk->setChecked(tx.cfcEnabled());
    m_cfcEnableChk->setToolTip(QStringLiteral(
        "Enable the Continuous Frequency Compressor (10-band)"));
    cfcLay->addWidget(m_cfcEnableChk);

    m_cfcPostEqEnableChk = new QCheckBox("Post-EQ Enable");
    m_cfcPostEqEnableChk->setObjectName(QStringLiteral("chkCFCPeqEnable"));
    m_cfcPostEqEnableChk->setChecked(tx.cfcPostEqEnabled());
    m_cfcPostEqEnableChk->setToolTip(QStringLiteral(
        "Enable the Post-EQ stage applied after CFC compression"));
    cfcLay->addWidget(m_cfcPostEqEnableChk);

    m_cfcPrecompSpin = new QSpinBox;
    m_cfcPrecompSpin->setObjectName(QStringLiteral("udCFCPreComp"));
    m_cfcPrecompSpin->setRange(TransmitModel::kCfcPrecompDbMin,
                               TransmitModel::kCfcPrecompDbMax);
    m_cfcPrecompSpin->setSuffix(" dB");
    m_cfcPrecompSpin->setValue(tx.cfcPrecompDb());
    m_cfcPrecompSpin->setToolTip(QStringLiteral(
        "Global pre-compression gain (0–16 dB) applied before per-band "
        "compression"));
    addLabeledSpinner(cfcLay, "Pre-Comp", m_cfcPrecompSpin);

    m_cfcPostEqGainSpin = new QSpinBox;
    m_cfcPostEqGainSpin->setObjectName(QStringLiteral("udCFCPostEqGain"));
    m_cfcPostEqGainSpin->setRange(TransmitModel::kCfcPostEqGainDbMin,
                                  TransmitModel::kCfcPostEqGainDbMax);
    m_cfcPostEqGainSpin->setSuffix(" dB");
    m_cfcPostEqGainSpin->setValue(tx.cfcPostEqGainDb());
    m_cfcPostEqGainSpin->setToolTip(QStringLiteral(
        "Global Post-EQ make-up gain (-24 .. +24 dB)"));
    addLabeledSpinner(cfcLay, "Post-EQ Gain", m_cfcPostEqGainSpin);

    m_cfcBandsBtn = new QPushButton("Configure CFC bands…");
    m_cfcBandsBtn->setObjectName(QStringLiteral("btnCFCBandsConfigure"));
    m_cfcBandsBtn->setAutoDefault(false);
    m_cfcBandsBtn->setToolTip(QStringLiteral(
        "Open the per-band CFC editor (10-band ParaEQ + compression sliders)"));
    cfcLay->addWidget(m_cfcBandsBtn);

    // ── Wiring: CFC widgets ↔ TransmitModel ──────────────────────────────────
    connect(m_cfcEnableChk, &QCheckBox::toggled,
            &tx, &TransmitModel::setCfcEnabled);
    connect(&tx, &TransmitModel::cfcEnabledChanged,
            m_cfcEnableChk, [this](bool on) {
        QSignalBlocker b(m_cfcEnableChk);
        m_cfcEnableChk->setChecked(on);
    });

    connect(m_cfcPostEqEnableChk, &QCheckBox::toggled,
            &tx, &TransmitModel::setCfcPostEqEnabled);
    connect(&tx, &TransmitModel::cfcPostEqEnabledChanged,
            m_cfcPostEqEnableChk, [this](bool on) {
        QSignalBlocker b(m_cfcPostEqEnableChk);
        m_cfcPostEqEnableChk->setChecked(on);
    });

    connect(m_cfcPrecompSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            &tx, &TransmitModel::setCfcPrecompDb);
    connect(&tx, &TransmitModel::cfcPrecompDbChanged,
            m_cfcPrecompSpin, [this](int dB) {
        QSignalBlocker b(m_cfcPrecompSpin);
        m_cfcPrecompSpin->setValue(dB);
    });

    connect(m_cfcPostEqGainSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            &tx, &TransmitModel::setCfcPostEqGainDb);
    connect(&tx, &TransmitModel::cfcPostEqGainDbChanged,
            m_cfcPostEqGainSpin, [this](int dB) {
        QSignalBlocker b(m_cfcPostEqGainSpin);
        m_cfcPostEqGainSpin->setValue(dB);
    });

    // [Configure CFC bands…] — Batch 6 wires the parent SetupDialog to route
    // this signal to the modeless TxCfcDialog.
    connect(m_cfcBandsBtn, &QPushButton::clicked,
            this, &CfcSetupPage::openCfcDialogRequested);

    // ══════════════════════════════════════════════════════════════════════════
    // ── Group 3: CESSB (Controlled-Envelope SSB) ──────────────────────────────
    // ══════════════════════════════════════════════════════════════════════════
    //
    // CESSB only takes effect when the speech compressor (CPDR) is enabled —
    // see wdsp/TXA.c:843-851 [v2.10.3.13]:
    //
    //     if (txa[channel].compressor.p->run)
    //     {
    //         CalcBandpassFilter (txa[channel].bp1.p, ...);
    //         txa[channel].bp1.p->run = 1;
    //         if (txa[channel].osctrl.p->run)
    //         {
    //             CalcBandpassFilter (txa[channel].bp2.p, ...);
    //             txa[channel].bp2.p->run = 1;
    //         }
    //     }
    //
    // i.e. CESSB's bp2 stage only activates when both compressor AND osctrl
    // (CESSB) run-flags are set; flipping CESSB without CPDR is a no-op.
    //
    QGroupBox* cessbGrp = addSection("CESSB");
    QVBoxLayout* cessbLay = qobject_cast<QVBoxLayout*>(cessbGrp->layout());

    m_cessbEnableChk = new QCheckBox("Enable");
    m_cessbEnableChk->setObjectName(QStringLiteral("chkCESSBEnable"));
    m_cessbEnableChk->setChecked(tx.cessbOn());
    m_cessbEnableChk->setToolTip(QStringLiteral(
        "Enable Controlled-Envelope SSB (acts only when CPDR is enabled)"));
    cessbLay->addWidget(m_cessbEnableChk);

    auto* cessbNote = new QLabel(QStringLiteral(
        "CESSB only acts when CPDR is enabled (engages bp2 in TXA "
        "bandpass chain)."));
    cessbNote->setObjectName(QStringLiteral("lblCESSBNote"));
    cessbNote->setWordWrap(true);
    cessbNote->setStyleSheet(QStringLiteral(
        "QLabel { color: #8aa8c0; font-size: 11px; font-style: italic; }"));
    cessbLay->addWidget(cessbNote);

    // ── Wiring: CESSB widget ↔ TransmitModel ─────────────────────────────────
    connect(m_cessbEnableChk, &QCheckBox::toggled,
            &tx, &TransmitModel::setCessbOn);
    connect(&tx, &TransmitModel::cessbOnChanged,
            m_cessbEnableChk, [this](bool on) {
        QSignalBlocker b(m_cessbEnableChk);
        m_cessbEnableChk->setChecked(on);
    });
}

// ══════════════════════════════════════════════════════════════════════════════
// MnfSetupPage
// ══════════════════════════════════════════════════════════════════════════════
//
// From Thetis setup.cs — tabDSP / tabPageMNF controls:
//   chkMNFAutoIncrease, comboMNFWindow, lstNotches (display)
//
MnfSetupPage::MnfSetupPage(RadioModel* model, QWidget* parent)
    : SetupPage("MNF", model, parent)
{
    // ── Manual Notch ──────────────────────────────────────────────────────────
    QGroupBox* mnfGrp = addSection("Manual Notch");
    QVBoxLayout* mnfLay = qobject_cast<QVBoxLayout*>(mnfGrp->layout());

    auto* autoIncrease = new QPushButton("Enable");
    addLabeledToggle(mnfLay, "Auto-Increase", autoIncrease);

    auto* windowCombo = new QComboBox;
    windowCombo->addItems({"Blackman-Harris", "Hann", "Flat-Top"});
    addLabeledCombo(mnfLay, "Window", windowCombo);

    auto* notchList = new QLabel("(no notches)");
    addLabeledLabel(mnfLay, "Notch List", notchList);

    disableGroup(mnfGrp);
}

// ══════════════════════════════════════════════════════════════════════════════
// NrAnfSetupPage::selectSubtab
// ══════════════════════════════════════════════════════════════════════════════
// Used by MainWindow's openNrSetupRequested handler to deep-link the "More
// Settings…" popup button into the correct sub-tab. Tab order mirrors the
// constructor's addTab calls.
//
void NrAnfSetupPage::selectSubtab(NrSlot slot)
{
    if (!m_tabs) { return; }
    // Map NrSlot → QTabWidget tab label. Must match the labels passed to
    // makeTab(tabs, <name>) in the ctor.
    QString target;
    switch (slot) {
        case NrSlot::NR1:  target = QStringLiteral("NR1");  break;
        case NrSlot::NR2:  target = QStringLiteral("NR2");  break;
        case NrSlot::NR3:  target = QStringLiteral("NR3");  break;
        case NrSlot::NR4:  target = QStringLiteral("NR4");  break;
        case NrSlot::DFNR: target = QStringLiteral("DFNR"); break;
        case NrSlot::MNR:  target = QStringLiteral("MNR");  break;
        case NrSlot::BNR:
        case NrSlot::Off:  return;  // no dedicated sub-tab
    }
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (m_tabs->tabText(i) == target) {
            m_tabs->setCurrentIndex(i);
            return;
        }
    }
}

} // namespace NereusSDR
