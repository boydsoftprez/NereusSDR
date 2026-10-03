// =================================================================
// src/gui/setup/DspSetupPages.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: Match NR2/NR4 controls and defaults to Thetis v2.10.3.15.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//               (R-IOS-06, R-IOS-27).
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-05-04 — Issue #175 Wave 1: dropped misplaced AM TX / Carrier
//                 Level stub from AmSamSetupPage (control belongs at
//                 Thetis grpTXAM on tpTransmit, not the DSP/AM tab).
//   2026-09-23 - R-R3-21: AgcAlcSetupPage TX Leveler and TX ALC groups
//                 follow the remote transmit permission. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-23 - R-R3-21 / R-R3-09: the TNF page's row commit sends a
//                 remote window's centre and width to the Core as one move.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-23 - R-R3-46: the CW sidetone row reads the Core's radio
//                 at build in a remote window. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-24 - R-R3-49: controls whose feature is not built yet are
//                 hidden through UnbuiltFeatures: the CW keyer and timing
//                 groups, the APF bandwidth and gain, the SAM group, the AM
//                 squelch maximum tail, the FM receive deviation and
//                 de-emphasis. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-24 - iPhone app follow-up (R-IOS-06): the AM and FM squelch
//                 threshold ranges come from ControlRanges.h, which the
//                 Core's catalogue reads too. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 4): AGC/ALC's TX Leveler and TX ALC
//                 groups and every CFC page setting follow the transmit
//                 settings gate at version 4 (the Core mirrors them)
//                 instead of the transmit permission. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-49 / R-R3-21 (parity Task 16): NR/ANF's DFNR and MNR
//                 tabs follow what the station can run (the Core's in a
//                 remote window), disabled with the plain reason; TNF's
//                 minimum notch width is the Core's in a remote window.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-49 (trunk merge of parity Tasks 16 to 18): the DFNR
//                 and MNR tabs read RadioModel::nrCannotRunReason (the one
//                 source, DspAssetService) and follow its signals.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - R-IOS-06, R-IOS-27: the NR1 tab reads ControlRanges.h,
//                 its ranges and defaults corrected to Thetis's NR
//                 spinboxes (taps 1-1024, delay 1-1023, gain and leak
//                 1-1000, defaults 64 / 16 / 100 / 100) with Thetis's
//                 tooltips. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-27 - R-IOS-06, R-IOS-27: the MNR tab reads ControlRanges.h;
//                 with no slice it starts at MacNRFilter's DEF_* values
//                 (Aggressiveness 4, Bias 1.2), where it showed 6 and 1.5.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-49 / R-IOS-18: Setup description version 15 ids on
//                 the NR3 model, NNR limit and Diagnostics, APF Center Freq
//                 and Visual Notch. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
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
#include "core/ControlRanges.h"
#include "core/RadioConnection.h"
#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "core/dsp/DspAssetService.h"
#include "core/session/IStationLink.h"
#include "core/wdsp_api.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "gui/widgets/NnrControls.h"
#include "gui/DspAssetDialog.h"
#include "gui/UnbuiltFeatures.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>
#include <functional>
#include <memory>

namespace NereusSDR {

void DspReceiverSelection::setSelector(std::function<SliceModel*()> selector,
                                       std::function<bool()> requiresOwnedReceiver)
{
    m_selector = std::move(selector);
    m_requiresOwnedReceiver = std::move(requiresOwnedReceiver);
    m_lastSelected = m_selector ? m_selector() : nullptr;
    m_lastRestricted = restrictsReceiver();
    emit changed();
}

bool DspReceiverSelection::restrictsReceiver() const
{
    return m_requiresOwnedReceiver && m_requiresOwnedReceiver();
}

SliceModel* DspReceiverSelection::selected(RadioModel* model) const
{
    return m_selector ? m_selector() : (model ? model->activeSlice() : nullptr);
}

void DspReceiverSelection::notifyChanged(RadioModel* model)
{
    SliceModel* now = selected(model);
    const bool restricted = restrictsReceiver();
    if (now == m_lastSelected && restricted == m_lastRestricted) { return; }
    m_lastSelected = now;
    m_lastRestricted = restricted;
    emit changed();
}

ReceiverDspSetupPage::ReceiverDspSetupPage(const QString& title, RadioModel* model,
                                           QWidget* parent, DspReceiverSelection* selection)
    : SetupPage(title, model, parent), m_selection(selection)
{}

SliceModel* ReceiverDspSetupPage::selectedSlice()
{
    return m_selection ? m_selection->selected(model())
                       : (model() ? model()->activeSlice() : nullptr);
}

void ReceiverDspSetupPage::watchReceiverSelection(std::function<void()> update)
{
    if (model()) {
        connect(model(), &RadioModel::activeSliceChanged, this,
                [update](int) { update(); });
    }
    if (m_selection) {
        connect(m_selection, &DspReceiverSelection::changed, this,
                [update] { update(); });
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: disable every child widget inside a group box (NYI guard).
// ─────────────────────────────────────────────────────────────────────────────
static void disableGroup(QGroupBox* grp)
{
    grp->setEnabled(false);
}

using SliceBindings = QList<QMetaObject::Connection>;

// R-R3-21: keeps Setup controls bound to the active slice while the page is
// open. bind(slice) makes the connections for one slice (slice is null when
// there is none) and returns them; when the active slice changes they are
// dropped and bind() runs again for the new one.
static void bindToActiveSlice(QObject* owner, RadioModel* model,
                              DspReceiverSelection* selection,
                              std::function<SliceBindings(SliceModel*)> bind)
{
    auto held = std::make_shared<SliceBindings>();
    const auto rebind = [held, bind = std::move(bind)](SliceModel* slice) {
        for (const QMetaObject::Connection& c : std::as_const(*held)) {
            QObject::disconnect(c);
        }
        *held = bind(slice);
    };
    const auto current = [model, selection] {
        return selection ? selection->selected(model)
                         : (model ? model->activeSlice() : nullptr);
    };
    rebind(current());
    if (model) {
        QObject::connect(model, &RadioModel::activeSliceChanged, owner,
                         [current, rebind](int) { rebind(current()); });
    }
    if (selection) {
        QObject::connect(selection, &DspReceiverSelection::changed, owner,
                         [current, rebind] { rebind(current()); });
    }
}

// With no slice a control is off and says why; with one it is on again.
static void setSliceAvailable(QWidget* w, bool available, const QString& tip = QString())
{
    w->setEnabled(available);
    w->setToolTip(available ? tip : QStringLiteral("Connect to a radio to change this."));
}

// R-R3-21: a squelch threshold slider (dB) bound both ways to one setting of
// the active slice; with no slice it says why it is off.
static void bindSquelchThreshold(QObject* owner, RadioModel* model,
                                 DspReceiverSelection* selection, QSlider* slider,
                                 QLabel* value,
                                 double (SliceModel::*getter)() const,
                                 void (SliceModel::*setter)(double),
                                 void (SliceModel::*changed)(double))
{
    bindToActiveSlice(owner, model, selection, [=](SliceModel* slice) {
        SliceBindings conns;
        setSliceAvailable(slider, slice != nullptr);
        if (!slice) { return conns; }
        const auto show = [slider, value](double dB) {
            QSignalBlocker block(slider);
            slider->setValue(static_cast<int>(std::lround(dB)));
            value->setText(QStringLiteral("%1 dB").arg(slider->value()));
        };
        show((slice->*getter)());
        conns << QObject::connect(slider, &QSlider::valueChanged, slice,
                                  [slice, setter, value](int dB) {
            value->setText(QStringLiteral("%1 dB").arg(dB));
            (slice->*setter)(static_cast<double>(dB));
        });
        conns << QObject::connect(slice, changed, slider, show);
        return conns;
    });
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
AgcAlcSetupPage::AgcAlcSetupPage(RadioModel* model, QWidget* parent,
                                 DspReceiverSelection* selection)
    : ReceiverDspSetupPage("AGC/ALC", model, parent, selection)
{
    rebuildForActiveSlice();
    watchReceiverSelection([this] { rebuildForActiveSlice(); });
}

void AgcAlcSetupPage::rebuildForActiveSlice()
{
    // Drop the old receiver widgets and their slice connections before a
    // new selection can receive a pending gesture (same contract as NR/ANF).
    QVBoxLayout* layout = contentLayout();
    while (layout->count() > 0) {
        QLayoutItem* item = layout->takeAt(0);
        delete item->widget();
        delete item;
    }
    m_txLevelerGrp = nullptr;
    m_txAlcGrp = nullptr;
    RadioModel* model = this->model();
    SliceModel* slice = selectedSlice();
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
    m_agcModeCombo->setProperty("nereusSetupId", "dsp.agcAlc.agcMode");
    m_agcModeCombo->addItems({"Off", "Long", "Slow", "Med", "Fast", "Custom"});
    m_agcModeCombo->setCurrentIndex(static_cast<int>(slice->agcMode()));
    // From Thetis v2.10.3.13 console.resx:4555 — comboAGC.ToolTip
    m_agcModeCombo->setToolTip(QStringLiteral("Automatic Gain Control Mode Setting"));
    addLabeledCombo(agcLay, "Mode", m_agcModeCombo);

    m_agcAttack = new QSpinBox;
    m_agcAttack->setProperty("nereusSetupId", "dsp.agcAlc.agcAttack");
    m_agcAttack->setRange(1, 1000);
    m_agcAttack->setSuffix(" ms");
    m_agcAttack->setValue(slice->agcAttack());
    addLabeledSpinner(agcLay, "Attack", m_agcAttack);

    m_agcDecay = new QSpinBox;
    m_agcDecay->setProperty("nereusSetupId", "dsp.agcAlc.agcDecay");
    m_agcDecay->setRange(1, 5000);
    m_agcDecay->setSuffix(" ms");
    m_agcDecay->setValue(slice->agcDecay());
    // From Thetis v2.10.3.13 setup.designer.cs:39390 — udDSPAGCDecay.ToolTip
    m_agcDecay->setToolTip(QStringLiteral("Time-constant to increase signal amplitude after strong signal"));
    addLabeledSpinner(agcLay, "Decay", m_agcDecay);

    m_agcHang = new QSpinBox;
    m_agcHang->setProperty("nereusSetupId", "dsp.agcAlc.agcHang");
    m_agcHang->setRange(10, 5000);
    m_agcHang->setSuffix(" ms");
    m_agcHang->setValue(slice->agcHang());
    // From Thetis v2.10.3.13 setup.designer.cs:39294 — udDSPAGCHangTime.ToolTip
    m_agcHang->setToolTip(QStringLiteral("Time to hold constant gain after strong signal"));
    addLabeledSpinner(agcLay, "Hang", m_agcHang);

    m_agcSlope = new QSlider(Qt::Horizontal);
    m_agcSlope->setProperty("nereusSetupId", "dsp.agcAlc.agcSlope");
    m_agcSlope->setProperty("nereusSetupScale", 0.1);
    m_agcSlope->setRange(0, 20);
    m_agcSlope->setValue(slice->agcSlope() / 10);
    // From Thetis v2.10.3.13 setup.designer.cs:39358 — udDSPAGCSlope.ToolTip
    m_agcSlope->setToolTip(QStringLiteral("Gain difference for weak and strong signals"));
    addLabeledSlider(agcLay, "Slope", m_agcSlope);

    m_agcMaxGain = new QSpinBox;
    m_agcMaxGain->setProperty("nereusSetupId", "dsp.agcAlc.agcMaxGain");
    m_agcMaxGain->setRange(-20, 120);
    m_agcMaxGain->setSuffix(" dB");
    m_agcMaxGain->setValue(slice->agcMaxGain());
    // From Thetis v2.10.3.13 setup.designer.cs:39325 — udDSPAGCMaxGaindB.ToolTip
    m_agcMaxGain->setToolTip(QStringLiteral("Threshold AGC: no gain over this Max Gain is applied, irrespective of signal weakness"));
    addLabeledSpinner(agcLay, "Max Gain", m_agcMaxGain);

    m_agcFixedGain = new QSpinBox;
    m_agcFixedGain->setProperty("nereusSetupId", "dsp.agcAlc.agcFixedGain");
    m_agcFixedGain->setRange(-20, 120);
    m_agcFixedGain->setSuffix(" dB");
    m_agcFixedGain->setValue(slice->agcFixedGain());
    // From Thetis v2.10.3.13 setup.designer.cs:39448 — udDSPAGCFixedGaindB.ToolTip
    m_agcFixedGain->setToolTip(QStringLiteral("Gain when AGC is disabled"));
    addLabeledSpinner(agcLay, "Fixed Gain", m_agcFixedGain);

    m_agcHangThresh = new QSlider(Qt::Horizontal);
    m_agcHangThresh->setProperty("nereusSetupId", "dsp.agcAlc.agcHangThreshold");
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
    m_autoAgcChk->setProperty("nereusSetupId", "dsp.agcAlc.autoAgcEnabled");
    m_autoAgcChk->setChecked(slice->autoAgcEnabled());
    // From Thetis v2.10.3.13 setup.designer.cs:38679 — chkAutoAGCRX1.ToolTip
    m_autoAgcChk->setToolTip(QStringLiteral("Automatically adjust AGC based on Noise Floor"));
    autoAgcLay->addWidget(m_autoAgcChk);

    m_autoAgcOffset = new QSpinBox;
    m_autoAgcOffset->setProperty("nereusSetupId", "dsp.agcAlc.autoAgcOffset");
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
    m_txLevelerGrp = txLevGrp;
    QVBoxLayout* txLevLay = qobject_cast<QVBoxLayout*>(txLevGrp->layout());

    m_txLevelerOnChk = new QCheckBox("Enable");
    m_txLevelerOnChk->setProperty("nereusSetupId", "dsp.agcAlc.txLevelerOn");
    m_txLevelerOnChk->setChecked(tx.txLevelerOn());
    // From Thetis setup.Designer.cs:38707 [v2.10.3.13] — chkDSPLevelerEnabled tooltip.
    m_txLevelerOnChk->setToolTip(
        QStringLiteral("Adjust gain if transmit audio increases/decreases"));
    txLevLay->addWidget(m_txLevelerOnChk);

    m_txLevelerTopSpin = new QSpinBox;
    m_txLevelerTopSpin->setProperty("nereusSetupId", "dsp.agcAlc.txLevelerMaxGain");
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
    m_txLevelerDecaySpin->setProperty("nereusSetupId", "dsp.agcAlc.txLevelerDecay");
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
    m_txAlcGrp = txAlcGrp;
    QVBoxLayout* txAlcLay = qobject_cast<QVBoxLayout*>(txAlcGrp->layout());

    m_txAlcMaxGainSpin = new QSpinBox;
    m_txAlcMaxGainSpin->setProperty("nereusSetupId", "dsp.agcAlc.txAlcMaxGain");
    m_txAlcMaxGainSpin->setRange(TransmitModel::kTxAlcMaxGainDbMin,
                                 TransmitModel::kTxAlcMaxGainDbMax);
    m_txAlcMaxGainSpin->setSuffix(" dB");
    m_txAlcMaxGainSpin->setValue(tx.txAlcMaxGain());
    // From Thetis setup.Designer.cs:38828 [v2.10.3.13] — udDSPALCMaximumGain tooltip.
    m_txAlcMaxGainSpin->setToolTip(
        QStringLiteral("Maximum gain to apply before ALC limiting"));
    addLabeledSpinner(txAlcLay, "Max Gain", m_txAlcMaxGainSpin);

    m_txAlcDecaySpin = new QSpinBox;
    m_txAlcDecaySpin->setProperty("nereusSetupId", "dsp.agcAlc.txAlcDecay");
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

    // R-R3-49 (parity Task 4): the Core mirrors the TX Leveler and TX ALC
    // settings (transmitSettingsVersion 4). In a remote window both groups
    // start closed until SetupDialog pushes that gate, then change the
    // Core's values while its radio is off the air.
    if (!model->ownsLocalDsp()) {
        setTransmitSettingsPermittedAt(4,
            m_txSettingsGateKnown && m_txSettingsPermitted, m_txSettingsReason);
    }
}

void AgcAlcSetupPage::setTransmitSettingsPermittedAt(int version, bool permitted,
                                                     const QString& reason)
{
    if (version != 4) {
        return;
    }
    m_txSettingsGateKnown = true;
    m_txSettingsPermitted = permitted;
    m_txSettingsReason = reason;
    gateTransmitControls({m_txLevelerGrp, m_txAlcGrp}, permitted,
        reason.isEmpty() ? IStationLink::transmitSettingsUnavailableReason() : reason);
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
// Each active-slice change reconstructs these tab controls from the new
// slice's values. Destroying the prior widgets disconnects their edits and
// readbacks, so an in-progress gesture cannot retarget another receiver.
//
NrAnfSetupPage::NrAnfSetupPage(RadioModel* model, QWidget* parent,
                               DspReceiverSelection* selection)
    : ReceiverDspSetupPage("NR/ANF", model, parent, selection)
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

    rebuildForActiveSlice();
    watchReceiverSelection([this] { rebuildForActiveSlice(); });
}

void NrAnfSetupPage::rebuildForActiveSlice()
{
    RadioModel* model = this->model();
    QTabWidget* tabs = m_tabs;
    const int oldTab = tabs->currentIndex();
    m_nnrControls = nullptr;
    while (tabs->count() != 0) {
        QWidget* old = tabs->widget(0);
        tabs->removeTab(0);
        delete old;
    }
    SliceModel* slice = selectedSlice();

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

    const auto nrDoubleRow = [&](QVBoxLayout* layout, const ControlRanges::NrControl& control,
                                  double value, const QString& tooltip = QString()) {
        return addDoubleSliderRow(layout, QString::fromUtf8(control.label),
            control.min * control.scale, control.max * control.scale, value,
            control.step * control.scale, control.decimals, tooltip,
            QString::fromUtf8(control.suffix));
    };

    // ── NR1 tab ───────────────────────────────────────────────────────────────
    // From Thetis setup.designer.cs NR1 (ANR) group [v2.10.3.13].
    {
        auto [tabPage, tabLay] = makeTab(tabs, "NR1");
        Q_UNUSED(tabPage)
        QVBoxLayout* grpLay = makeGroup(tabLay, "NR1 (LMS)");

        // Taps, Delay, Gain and Leak: Thetis's NR spinboxes (ranges,
        // defaults and the gain x 1e-6 / leak x 1e-3 conversion) from
        // ControlRanges.h, the table the VFO flag's NR1 quick controls read.
        // Tooltips from Thetis setup.designer.cs:43440-43551 [v2.10.3.15].
        using namespace ControlRanges;
        const auto nr1Row = [&](const NrControl& control, double current,
                                const QString& tooltip) {
            return addSliderRow(grpLay, QString::fromUtf8(control.label),
                                static_cast<int>(control.min), static_cast<int>(control.max),
                                nrSliderFromValue(control, current), tooltip);
        };
        auto [taps, tapsVal] = nr1Row(kNr1Taps, slice ? slice->nr1Taps() : kNr1Taps.defaultValue,
            tr("Determines the length of the NR computed filter."));
        taps->setProperty("nereusSetupId", "dsp.nrAnf.nr1Taps");
        auto [delay, delayVal] = nr1Row(kNr1Delay,
            slice ? slice->nr1Delay() : kNr1Delay.defaultValue,
            tr("Determines how far back you look in the signal before you begin to "
               "compute a coherent signal enhancement filter."));
        delay->setProperty("nereusSetupId", "dsp.nrAnf.nr1Delay");
        auto [gain, gainVal] = nr1Row(kNr1Gain, slice ? slice->nr1Gain() : kNr1Gain.defaultValue,
            tr("Determines the adaptation rate of the filter."));
        gain->setProperty("nereusSetupId", "dsp.nrAnf.nr1Gain");
        gain->setProperty("nereusSetupScale", 1000000.0);
        auto [leak, leakVal] = nr1Row(kNr1Leak,
            slice ? slice->nr1Leakage() : kNr1Leak.defaultValue,
            tr("Determines the adaptation rate of the filter."));
        leak->setProperty("nereusSetupId", "dsp.nrAnf.nr1Leakage");
        leak->setProperty("nereusSetupScale", 1000.0);

        // Position radio
        auto [preRdo, postRdo] = addPositionRow(grpLay);
        auto* choice = new QButtonGroup(grpLay->parentWidget());
        choice->setProperty("nereusSetupId", "dsp.nrAnf.nr1Position");
        choice->addButton(preRdo, 0);
        choice->addButton(postRdo, 1);
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
                // Slider x 1e-6 to the WDSP domain, as Thetis converts it.
                slice->setNr1Gain(ControlRanges::nrValueFromSlider(ControlRanges::kNr1Gain, v));
            });

            connect(leak, &QSlider::valueChanged,
                    slice, [slice](int v) {
                // Slider x 1e-3 to the WDSP domain, as Thetis converts it.
                slice->setNr1Leakage(ControlRanges::nrValueFromSlider(ControlRanges::kNr1Leak, v));
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
                gain->setValue(ControlRanges::nrSliderFromValue(ControlRanges::kNr1Gain, v));
            });
            connect(slice, &SliceModel::nr1LeakageChanged, leak, [leak](double v) {
                QSignalBlocker b(leak);
                leak->setValue(ControlRanges::nrSliderFromValue(ControlRanges::kNr1Leak, v));
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
            auto* gmChoice = new QButtonGroup(gmGrp->parentWidget());
            gmChoice->setProperty("nereusSetupId", "dsp.nrAnf.nr2GainMethod");
            for (int i = 0; i < gmRdos.size(); ++i) { gmChoice->addButton(gmRdos[i], i); }
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
            auto* npeChoice = new QButtonGroup(npeGrp->parentWidget());
            npeChoice->setProperty("nereusSetupId", "dsp.nrAnf.nr2NpeMethod");
            for (int i = 0; i < npeRdos.size(); ++i) { npeChoice->addButton(npeRdos[i], i); }
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
        t1->setProperty("nereusSetupId", "dsp.nrAnf.nr2TrainT1");
        t1->setProperty("nereusSetupScale", 10.0);

        // T2 — udDSPEMNRTrainT2: 0.0 .. 2.0 step 0.05, default 0.20
        auto [t2, t2Val, t2Scale] = addDoubleSliderRow(tfGrp, "T2",
            0.0, 2.0, slice ? slice->nr2TrainT2() : 0.20,
            0.05, 2,
            tr("EMNR T2 training parameter. Range 0.0 to 2.0."));
        t2->setProperty("nereusSetupId", "dsp.nrAnf.nr2TrainT2");
        t2->setProperty("nereusSetupScale", 20.0);

        // AE Filter checkbox
        auto* aeChk = new QCheckBox("AE Filter");
        aeChk->setProperty("nereusSetupId", "dsp.nrAnf.nr2AeFilter");
        aeChk->setChecked(slice ? slice->nr2AeFilter() : true);
        aeChk->setToolTip(tr("Enable EMNR Acoustic Echo (AE) filter stage."));
        tfGrp->addWidget(aeChk);

        // Position radio
        auto [preRdo, postRdo] = addPositionRow(tfGrp);
        auto* choice = new QButtonGroup(tfGrp->parentWidget());
        choice->setProperty("nereusSetupId", "dsp.nrAnf.nr2Position");
        choice->addButton(preRdo, 0);
        choice->addButton(postRdo, 1);
        {
            const bool isPost = !slice || (slice->nr2Position() == NrPosition::PostAgc);
            preRdo->setChecked(!isPost);
            postRdo->setChecked(isPost);
        }

        // ── Noise Post-Proc ─────────────────────────────────────────────────
        QVBoxLayout* ppGrp = makeGroup(tabLay, "Noise Post-Proc");

        auto* ppRun = new QCheckBox("Enable");
        ppRun->setProperty("nereusSetupId", "dsp.nrAnf.nr2Post2Run");
        ppRun->setChecked(slice ? slice->nr2Post2Run() : false);
        ppRun->setToolTip(tr("Enable EMNR Noise Post-Processing cascade."));
        ppGrp->addWidget(ppRun);

        // Level — udDSPEMNRPost2Level: 0-100 step 1, default 15.0
        auto [ppLevel, ppLevelVal, ppLevelScale] = addDoubleSliderRow(ppGrp, "Level",
            0.0, 100.0, slice ? slice->nr2Post2Level() : 15.0,
            1.0, 0);
        ppLevel->setProperty("nereusSetupId", "dsp.nrAnf.nr2Post2Level");
        ppLevel->setProperty("nereusSetupScale", 1.0);

        // From Thetis setup.designer.cs:43019-43158 [v2.10.3.15]:
        // Factor and Rate both span 0..100 in 0.1 steps.
        using namespace ControlRanges;
        auto [ppFactor, ppFactorVal, ppFactorScale] = nrDoubleRow(ppGrp, kNr2Post2Factor,
            slice ? slice->nr2Post2Factor() : kNr2Post2Factor.defaultValue);
        ppFactor->setProperty("nereusSetupId", "dsp.nrAnf.nr2Post2Factor");
        ppFactor->setProperty("nereusSetupScale", 10.0);
        auto [ppRate, ppRateVal, ppRateScale] = nrDoubleRow(ppGrp, kNr2Post2Rate,
            slice ? slice->nr2Post2Rate() : kNr2Post2Rate.defaultValue);
        ppRate->setProperty("nereusSetupId", "dsp.nrAnf.nr2Post2Rate");
        ppRate->setProperty("nereusSetupScale", 10.0);

        // Taper — udDSPEMNRPost2Taper (integer): 0-100, default 12
        auto [ppTaper, ppTaperVal] = addSliderRow(ppGrp, "Taper", 0, 100,
            slice ? slice->nr2Post2Taper() : 12);
        ppTaper->setProperty("nereusSetupId", "dsp.nrAnf.nr2Post2Taper");

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
            connect(slice, &SliceModel::nr2GainMethodChanged, gmGrp->parentWidget(),
                    [gmRdos](EmnrGainMethod v) {
                const int idx = static_cast<int>(v);
                if (idx >= 0 && idx < gmRdos.size()) {
                    QSignalBlocker b(gmRdos[idx]);
                    gmRdos[idx]->setChecked(true);
                }
            });
            connect(slice, &SliceModel::nr2NpeMethodChanged, npeGrp->parentWidget(),
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
        auto* choice = new QButtonGroup(grpLay->parentWidget());
        choice->setProperty("nereusSetupId", "dsp.nrAnf.nr3Position");
        choice->addButton(preRdo, 0);
        choice->addButton(postRdo, 1);
        {
            const bool isPost = !slice || (slice->nr3Position() == NrPosition::PostAgc);
            preRdo->setChecked(!isPost);
            postRdo->setChecked(isPost);
        }

        // "Use fixed gain for input samples" — chkRXANR3FixedGain [v2.10.3.13]
        auto* fixedGainChk = new QCheckBox("Use fixed gain for input samples");
        fixedGainChk->setProperty("nereusSetupId", "dsp.nrAnf.nr3UseDefaultGain");
        fixedGainChk->setChecked(slice ? slice->nr3UseDefaultGain() : true);
        // Tooltip source: Thetis setup.cs:35460 chkRXANR3FixedGain [v2.10.3.13]
        fixedGainChk->setToolTip(tr("Use a fixed (rather than adaptive) input sample gain."));
        grpLay->addWidget(fixedGainChk);

        // Model selector group: GLOBAL (not per-slice). R-R3-21: the Core
        // owns the NR3 model; this chooses among the Core's models (bundled
        // large/small plus any added) in local and remote windows alike.
        QVBoxLayout* mdlGrp = makeGroup(tabLay, "RNNoise Model (Global)");
        auto* nr3Picker = new Nr3ModelPicker(model, model ? model->dspAssets() : nullptr);
        if (auto* modelsBtn = nr3Picker->findChild<QPushButton*>(QStringLiteral("nr3ModelsButton"))) {
            modelsBtn->setStyleSheet(
                "QPushButton { background: #1a2a3a; border: 1px solid #304050; "
                "border-radius: 3px; color: #c8d8e8; font-size: 12px; padding: 3px 10px; }"
                "QPushButton:hover { background: #203040; }"
                "QPushButton:pressed { background: #00b4d8; color: #0f0f1a; }");
        }
        if (auto* statusLbl = nr3Picker->findChild<QLabel*>(QStringLiteral("nr3ModelStatusLabel"))) {
            statusLbl->setStyleSheet("QLabel { color: #00c8ff; font-size: 12px; }");
            statusLbl->setProperty("nereusSetupId", "dsp.nrAnf.nr3ModelStatus");
        }
        // Setup description version 15: the Core's NR3 model choice.
        if (auto* modelCombo = nr3Picker->findChild<QComboBox*>(QStringLiteral("nr3ModelCombo"))) {
            modelCombo->setProperty("nereusSetupId", "dsp.nrAnf.nr3Model");
            modelCombo->setAccessibleName(QStringLiteral("Model"));
        }
        mdlGrp->addWidget(nr3Picker);

        tabLay->addStretch(1);

        // ── Wire NR3 controls → SliceModel ───────────────────────────────────
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

        // From Thetis setup.designer.cs:42186-42412 [v2.10.3.15].
        // Use the popup/catalogue's ranges, increments and defaults.
        using namespace ControlRanges;
        auto [reduction, reductionVal, reductionScale] = nrDoubleRow(grpLay, kNr4Reduction,
            slice ? slice->nr4Reduction() : kNr4Reduction.defaultValue,
            tr("Spectral reduction amount in dB. Range 0-20, step 1."));
        reduction->setProperty("nereusSetupId", "dsp.nrAnf.nr4Reduction");
        reduction->setProperty("nereusSetupScale", 1.0);
        auto [smoothing, smoothingVal, smoothingScale] = nrDoubleRow(grpLay, kNr4Smoothing,
            slice ? slice->nr4Smoothing() : kNr4Smoothing.defaultValue,
            tr("Spectral smoothing factor. Range 0-100."));
        smoothing->setProperty("nereusSetupId", "dsp.nrAnf.nr4Smoothing");
        smoothing->setProperty("nereusSetupScale", 1.0);
        auto [whitening, whiteningVal, whiteningScale] = nrDoubleRow(grpLay, kNr4Whitening,
            slice ? slice->nr4Whitening() : kNr4Whitening.defaultValue,
            tr("Spectral whitening factor. Range 0-100."));
        whitening->setProperty("nereusSetupId", "dsp.nrAnf.nr4Whitening");
        whitening->setProperty("nereusSetupScale", 1.0);
        auto [rescale, rescaleVal, rescaleScale] = nrDoubleRow(grpLay, kNr4Rescale,
            slice ? slice->nr4Rescale() : kNr4Rescale.defaultValue,
            tr("Output rescale factor. Range 0-12, step 1."));
        rescale->setProperty("nereusSetupId", "dsp.nrAnf.nr4Rescale");
        rescale->setProperty("nereusSetupScale", 1.0);
        auto [snrThresh, snrThreshVal, snrThreshScale] = nrDoubleRow(grpLay, kNr4PostThresh,
            slice ? slice->nr4PostThresh() : kNr4PostThresh.defaultValue,
            tr("Post-processing SNR threshold. Range -10 to +10 dB, step 1."));
        snrThresh->setProperty("nereusSetupId", "dsp.nrAnf.nr4PostThresh");
        snrThresh->setProperty("nereusSetupScale", 1.0);

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
            const int algoIdx = slice ? static_cast<int>(slice->nr4Algo())
                                      : static_cast<int>(kNr4Algo.defaultValue); // Algo 1
            if (algoIdx == 0) { algo1->setChecked(true); }
            else if (algoIdx == 2) { algo3->setChecked(true); }
            else { algo2->setChecked(true); }
        }

        auto* algoChoice = new QButtonGroup(grpLay->parentWidget());
        algoChoice->setProperty("nereusSetupId", "dsp.nrAnf.nr4Algo");
        algoChoice->addButton(algo1, 0);
        algoChoice->addButton(algo2, 1);
        algoChoice->addButton(algo3, 2);
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

        QVBoxLayout* grpLay = makeGroup(tabLay, "DeepFilter NR");

        // Parity Task 16 (R-R3-49, R-R3-21): offered by what the station
        // can run (the Core's in a remote window, whatever this computer's
        // build), never hidden; while it cannot, the group is disabled and
        // the note says why.
        auto* dfnrNote = new QLabel(QString(), tabPage);
        dfnrNote->setObjectName(QStringLiteral("dfnrUnavailableNote"));
        dfnrNote->setStyleSheet(kInfoLbl);
        dfnrNote->setWordWrap(true);
        tabLay->insertWidget(0, dfnrNote);
        QWidget* dfnrGroup = grpLay->parentWidget();
        const auto applyDfnr = [model, dfnrNote, dfnrGroup]() {
            const QString reason = model ? model->nrCannotRunReason(NrSlot::DFNR)
                                         : RadioModel::nrCannotRunInThisBuildReason(
                                               NrSlot::DFNR);
            dfnrNote->setText(reason);
            dfnrNote->setVisible(!reason.isEmpty());
            dfnrGroup->setEnabled(reason.isEmpty());
        };
        applyDfnr();
        if (model) {
            // Trunk merge: the one availability source, DspAssetService,
            // and whether an older Core says at all.
            if (model->dspAssets()) {
                connect(model->dspAssets(), &DspAssetService::dfnrAvailabilityChanged, dfnrGroup, applyDfnr);
            }
            connect(model, &RadioModel::nrAvailabilityChanged, dfnrGroup, applyDfnr);
        }

        // Attenuation Limit (0-100 dB) — use the shared addSliderRow helper
        // so the style matches NR1/NR2/NR4 (label | slider | value label).
        auto [attenSl, attenVal] = addSliderRow(
            grpLay, "Attenuation Limit", 0, 100,
            slice ? static_cast<int>(slice->dfnrAttenLimit()) : 100,
            tr("Maximum noise attenuation in dB (0 = bypass, 100 = maximum). "
               "Default 100. Higher values suppress more noise but may clip speech peaks."),
            " dB");
        attenSl->setProperty("nereusSetupId", "dsp.nrAnf.dfnrAttenLimit");
        attenSl->setProperty("nereusSetupScale", 1.0);

        // Post-Filter Beta (0.00-0.30 step 0.01) — use the double helper.
        auto [betaSl, betaVal, betaScale] = addDoubleSliderRow(
            grpLay, "Post-Filter Beta", 0.0, 0.30,
            slice ? slice->dfnrPostFilterBeta() : 0.0, 0.01, 2,
            tr("Post-filter aggressiveness (0 = disabled, 0.30 = maximum). Default 0. "
               "Higher values reduce residual musical-noise artifacts but may "
               "over-attenuate consonants."),
            QString());
        betaSl->setProperty("nereusSetupId", "dsp.nrAnf.dfnrPostFilterBeta");
        betaSl->setProperty("nereusSetupScale", 100.0);
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
    // AetherSDR-native macOS noise reduction (not in Thetis). Always shown.
    // Parity Task 16 (R-R3-49, R-R3-21): its controls follow what the
    // station can run, the Core's in a remote window (a Linux window on a
    // Mac Core tunes the Core's MNR), disabled with the plain reason while
    // it cannot.
    {
        auto [tabPage, tabLay] = makeTab(tabs, "MNR");

        QVBoxLayout* grpLay = makeGroup(tabLay, "macOS NR (MNR)");

        auto* mnrNote = new QLabel(QString(), tabPage);
        mnrNote->setObjectName(QStringLiteral("mnrUnavailableNote"));
        mnrNote->setStyleSheet(kInfoLbl);
        mnrNote->setWordWrap(true);
        tabLay->insertWidget(0, mnrNote);

        // Full 6-knob tuning surface matching the VFO right-click MNR popup
        // (see VfoWidget::showMnrPopup). Ranges + factory defaults identical.

        using namespace ControlRanges;
        const auto mnrRow = [&](const NrControl& control, double current,
                                const QString& tooltip) {
            return addSliderRow(grpLay, QString::fromUtf8(control.label),
                                static_cast<int>(control.min), static_cast<int>(control.max),
                                nrSliderFromValue(control, current), tooltip,
                                QString::fromUtf8(control.suffix));
        };
        auto [strSl, strVal] = mnrRow(
            kMnrStrength, slice ? slice->mnrStrength() : kMnrStrength.defaultValue,
            tr("Dry/wet blend. 0%% = bypass (filter runs but output = input), "
               "100%% = full NR, 200%% = over-drive (phase-flip, destructive). "
               "Default 100%%."));
        strSl->setProperty("nereusSetupId", "dsp.nrAnf.mnrStrength");
        strSl->setProperty("nereusSetupScale", 100.0);
        Q_UNUSED(strVal);

        auto [oversubSl, oversubVal] = mnrRow(
            kMnrOversub, slice ? slice->mnrOversub() : kMnrOversub.defaultValue,
            tr("MMSE-Wiener oversubtraction factor. Higher = more attenuation "
               "on low-SNR bins; 1 = gentle, 4 = default, 20+ = underwater."));
        oversubSl->setProperty("nereusSetupId", "dsp.nrAnf.mnrOversub");
        oversubSl->setProperty("nereusSetupScale", 1.0);
        Q_UNUSED(oversubVal);

        auto [floorSl, floorVal] = mnrRow(
            kMnrFloor, slice ? slice->mnrFloor() : kMnrFloor.defaultValue,
            tr("Minimum Wiener gain per bin (x0.001). 0 = silence, "
               "50 = -26 dB (default), 1000 = 0 dB, 2000 = amplify."));
        floorSl->setProperty("nereusSetupId", "dsp.nrAnf.mnrFloor");
        floorSl->setProperty("nereusSetupScale", 1000.0);
        Q_UNUSED(floorVal);

        auto [alphaSl, alphaVal] = mnrRow(
            kMnrAlpha, slice ? slice->mnrAlpha() : kMnrAlpha.defaultValue,
            tr("Decision-directed smoothing (x0.01). 0 = no smoothing "
               "(chattery), 92 = Ephraim-Malah classic (default), "
               "100 = frozen prior SNR."));
        alphaSl->setProperty("nereusSetupId", "dsp.nrAnf.mnrAlpha");
        alphaSl->setProperty("nereusSetupScale", 100.0);
        Q_UNUSED(alphaVal);

        auto [biasSl, biasVal] = mnrRow(
            kMnrBias, slice ? slice->mnrBias() : kMnrBias.defaultValue,
            tr("Min-statistics noise-floor bias (x0.1). <10 = underestimate "
               "noise (less NR), 12 = default, >30 = overestimate (erodes signal). "
               "Nudge up if NR is weak, down if it eats speech."));
        biasSl->setProperty("nereusSetupId", "dsp.nrAnf.mnrBias");
        biasSl->setProperty("nereusSetupScale", 10.0);
        Q_UNUSED(biasVal);

        auto [gsmoothSl, gsmoothVal] = mnrRow(
            kMnrGsmooth, slice ? slice->mnrGsmooth() : kMnrGsmooth.defaultValue,
            tr("Temporal gain smoothing (x0.01). 0 = instant (musical noise), "
               "70 = balanced (default), 100 = frozen gain."));
        gsmoothSl->setProperty("nereusSetupId", "dsp.nrAnf.mnrGsmooth");
        gsmoothSl->setProperty("nereusSetupScale", 100.0);
        Q_UNUSED(gsmoothVal);

        QWidget* mnrGroup = grpLay->parentWidget();
        const auto applyMnr = [model, mnrNote, mnrGroup]() {
            const QString reason = model ? model->nrCannotRunReason(NrSlot::MNR)
                                         : RadioModel::nrCannotRunInThisBuildReason(
                                               NrSlot::MNR);
            mnrNote->setText(reason);
            mnrNote->setVisible(!reason.isEmpty());
            mnrGroup->setEnabled(reason.isEmpty());
        };
        applyMnr();
        if (model) {
            // Trunk merge: the one availability source, DspAssetService,
            // and whether an older Core says at all.
            if (model->dspAssets()) {
                connect(model->dspAssets(), &DspAssetService::mnrAvailabilityChanged, mnrGroup, applyMnr);
            }
            connect(model, &RadioModel::nrAvailabilityChanged, mnrGroup, applyMnr);
        }

        tabLay->addStretch(1);

        // ── Wire MNR controls → SliceModel ──────────────────────────────────
        // Parity Task 16: wired on every build; the slice's MNR settings
        // reach the Core that runs it (a remote window) or stay with the
        // slice (a computer that cannot run it, with the group disabled).
        if (slice) {
            connect(strSl, &QSlider::valueChanged, slice, [slice](int v) {
                slice->setMnrStrength(ControlRanges::nrValueFromSlider(ControlRanges::kMnrStrength, v));
            });
            connect(slice, &SliceModel::mnrStrengthChanged, strSl, [strSl](double v) {
                QSignalBlocker b(strSl); strSl->setValue(ControlRanges::nrSliderFromValue(ControlRanges::kMnrStrength, v));
            });

            connect(oversubSl, &QSlider::valueChanged, slice, [slice](int v) {
                slice->setMnrOversub(static_cast<double>(v));
            });
            connect(slice, &SliceModel::mnrOversubChanged, oversubSl, [oversubSl](double v) {
                QSignalBlocker b(oversubSl); oversubSl->setValue(ControlRanges::nrSliderFromValue(ControlRanges::kMnrOversub, v));
            });

            connect(floorSl, &QSlider::valueChanged, slice, [slice](int v) {
                slice->setMnrFloor(ControlRanges::nrValueFromSlider(ControlRanges::kMnrFloor, v));
            });
            connect(slice, &SliceModel::mnrFloorChanged, floorSl, [floorSl](double v) {
                QSignalBlocker b(floorSl); floorSl->setValue(ControlRanges::nrSliderFromValue(ControlRanges::kMnrFloor, v));
            });

            connect(alphaSl, &QSlider::valueChanged, slice, [slice](int v) {
                slice->setMnrAlpha(ControlRanges::nrValueFromSlider(ControlRanges::kMnrAlpha, v));
            });
            connect(slice, &SliceModel::mnrAlphaChanged, alphaSl, [alphaSl](double v) {
                QSignalBlocker b(alphaSl); alphaSl->setValue(ControlRanges::nrSliderFromValue(ControlRanges::kMnrAlpha, v));
            });

            connect(biasSl, &QSlider::valueChanged, slice, [slice](int v) {
                slice->setMnrBias(ControlRanges::nrValueFromSlider(ControlRanges::kMnrBias, v));
            });
            connect(slice, &SliceModel::mnrBiasChanged, biasSl, [biasSl](double v) {
                QSignalBlocker b(biasSl); biasSl->setValue(ControlRanges::nrSliderFromValue(ControlRanges::kMnrBias, v));
            });

            connect(gsmoothSl, &QSlider::valueChanged, slice, [slice](int v) {
                slice->setMnrGsmooth(ControlRanges::nrValueFromSlider(ControlRanges::kMnrGsmooth, v));
            });
            connect(slice, &SliceModel::mnrGsmoothChanged, gsmoothSl, [gsmoothSl](double v) {
                QSignalBlocker b(gsmoothSl); gsmoothSl->setValue(ControlRanges::nrSliderFromValue(ControlRanges::kMnrGsmooth, v));
            });
        }
    }

    // ── ANF tab ───────────────────────────────────────────────────────────────
    // NNR uses the same editor in the quick popup and in Setup. This tab's
    // existing scroll area supplies scrolling for the full control set.
    {
        auto [tabPage, tabLay] = makeTab(tabs, "NNR");
        m_nnrControls = new NnrControls(model, slice, NnrControls::Presentation::Full,
                                        tabPage);
        connect(m_nnrControls, &NnrControls::openModelsRequested, this,
                [this, model](int sliceId) {
            if (!model->sliceById(sliceId)) {
                return;
            }
            auto* dialog = new DspAssetDialog(model, DspAssetKind::NnrModel, this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->show();
        });
        const std::pair<const char*, const char*> nnrIds[] = {
            {"nnrModelCombo", "nnrModelSlot"}, {"nnrMaskFloorSpin", "nnrMaskFloorDb"},
            {"nnrPositionCombo", "nnrPosition"}, {"nnrAlphaSpin", "nnrAlpha"},
            {"nnrAlphaKneeSpin", "nnrAlphaKneeDb"}, {"nnrTauSpin", "nnrTauSeconds"},
            {"nnrMaxGainSpin", "nnrMaxGainDb"}, {"nnrAttackSpin", "nnrAttackMs"},
            {"nnrReleaseSpin", "nnrReleaseMs"}, {"nnrResetButton", "nnrResetTuning"},
            // Setup description version 15: the limit row and Diagnostics.
            {"nnrLimitNotice", "nnrLimit"}, {"nnrTryAgainButton", "nnrTryAgain"},
            {"nnrTestModeCombo", "nnrTestMode"}, {"nnrOutputModeCombo", "nnrOutputMode"},
            {"nnrApplyDiagnosticButton", "nnrApplyDiagnostics"}
        };
        for (const auto& [objectName, property] : nnrIds) {
            if (auto* control = m_nnrControls->findChild<QWidget*>(QLatin1String(objectName))) {
                control->setProperty("nereusSetupId", QStringLiteral("dsp.nrAnf.")
                    + QLatin1String(property));
            }
        }
        // Each Diagnostics line shows several of the described readouts.
        const std::pair<const char*, QStringList> nnrReadouts[] = {
            {"nnrRuntimeReadback", {QStringLiteral("dsp.nrAnf.nnrAvailable"),
                                    QStringLiteral("dsp.nrAnf.nnrReady"),
                                    QStringLiteral("dsp.nrAnf.nnrRunning")}},
            {"nnrModelsReadback", {QStringLiteral("dsp.nrAnf.nnrStandardAvailable"),
                                   QStringLiteral("dsp.nrAnf.nnrPremiumAvailable"),
                                   QStringLiteral("dsp.nrAnf.nnrActualModelSlot")}},
            {"nnrRateLatencyReadback", {QStringLiteral("dsp.nrAnf.nnrDspRateHz"),
                                        QStringLiteral("dsp.nrAnf.nnrNetworkRateHz"),
                                        QStringLiteral("dsp.nrAnf.nnrRateSupported"),
                                        QStringLiteral("dsp.nrAnf.nnrDelaySamples"),
                                        QStringLiteral("dsp.nrAnf.nnrLatencyMs")}},
            {"nnrSourceReadback", {QStringLiteral("dsp.nrAnf.nnrModelSource")}},
            {"nnrStatusReadback", {QStringLiteral("dsp.nrAnf.nnrProfilingAvailable"),
                                   QStringLiteral("dsp.nrAnf.nnrStatus")}}};
        for (const auto& [objectName, ids] : nnrReadouts) {
            if (auto* label = m_nnrControls->findChild<QLabel*>(QLatin1String(objectName))) {
                label->setProperty("nereusSetupIds", ids);
            }
        }
        tabLay->addWidget(m_nnrControls);
        tabLay->addStretch(1);
    }

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
        auto* note = new QLabel(
            "This switch and the VFO flag's ANF button are the same setting.");
        note->setStyleSheet(kInfoLbl);
        note->setWordWrap(true);
        grpLay->addWidget(note);

        // From Thetis chkDSPANFEnable [v2.10.3.13]. R-R3-21: bound to the
        // active slice's anfEnabled, the setting the VFO flag's ANF button
        // sets (SliceModel::setAnfEnabled); it was a greyed placeholder.
        auto* anfEnableChk = new QCheckBox("Enable ANF");
        anfEnableChk->setProperty("nereusSetupId", "dsp.nrAnf.anfEnabled");
        anfEnableChk->setObjectName(QStringLiteral("anfEnableCheck"));
        const QString anfTip = tr("Enable Adaptive Notch Filter.");
        setSliceAvailable(anfEnableChk, slice != nullptr, anfTip);
        if (slice) {
            anfEnableChk->setChecked(slice->anfEnabled());
            connect(anfEnableChk, &QCheckBox::toggled,
                    slice, &SliceModel::setAnfEnabled);
            connect(slice, &SliceModel::anfEnabledChanged, anfEnableChk,
                    [anfEnableChk](bool on) {
                QSignalBlocker block(anfEnableChk);
                anfEnableChk->setChecked(on);
            });
        }
        grpLay->addWidget(anfEnableChk);

        tabLay->addStretch(1);
    }
    tabs->setEnabled(slice != nullptr);
    if (oldTab >= 0 && oldTab < tabs->count()) {
        tabs->setCurrentIndex(oldTab);
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
NbSnbSetupPage::NbSnbSetupPage(RadioModel* model, QWidget* parent,
                               DspReceiverSelection* selection)
    : ReceiverDspSetupPage("NB/SNB", model, parent, selection)
{
    // Sliders with inline live-value labels. Tooltips use Thetis's own user-
    // facing text (from setup.designer.cs ToolTip attributes [v2.10.3.13]).
    // Ranges / defaults mirror Thetis NumericUpDown widgets byte-for-byte.
    //
    // Every control here used to write AppSettings plus a raw WDSP call with
    // a hardcoded channel 0 (SetEXTANB* / SetEXTNOBMode / SetRXASNBA*), so
    // tuning the blanker always acted on receiver A whichever receiver the
    // operator was working. That bypass did not show up in Sub-Epic J's
    // rxChannel() audit because it reached WDSP by a different route. The
    // eight knobs now live on SliceModel and RadioModel pushes each to
    // rxChannel(slice->sliceIndex()), the same path every other per-slice DSP
    // setting takes, which is also why the old null-channel crash gate is
    // gone: RadioModel's push resolves the channel and no-ops when there
    // isn't one.
    //
    // Slice-tracking policy: Setup is not attached to any flag, so the rule
    // is that it targets the active slice, and it follows a change of active
    // slice while open (R-R3-21, bindToActiveSlice at the end). `slice` here
    // only seeds the controls' first values.
    SliceModel* slice = selectedSlice();

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

    // Tell the operator the truth about what these five reach. WDSP keeps one
    // noise blanker per RECEIVER, not per slice: SetEXTANB* writes panb[id]
    // and SetEXTNOBMode writes pnob[id] (Thetis wdsp/nob.c:376-423 +
    // wdsp/nobII.c:658-663 [v2.10.3.15]), the single ANB / NOB members of
    // struct _rcvr (cmaster.h:74-82). Two receivers sharing one DDC window
    // therefore cannot have independent blankers, so NereusSDR mirrors these
    // across co-hosted slices rather than pretending otherwise. Saying so here
    // beats letting the operator discover it by watching another receiver's
    // settings move.
    {
        auto* nb1Note = new QLabel(
            tr("Shared by every receiver on the same DDC. Adjusting these "
               "changes the blanker for all of them."));
        nb1Note->setWordWrap(true);
        nb1Note->setStyleSheet(QStringLiteral("color: #8aa8c0; font-size: 11px;"));
        nb1Lay->addWidget(nb1Note);
    }

    // Threshold — udDSPNB: 1-1000, default 30. WDSP threshold = 0.165 × value,
    // applied by RadioModel on the way to SetEXTANBThreshold.
    QSlider* nb1Thresh = addIntSlider(nb1Lay, tr("Threshold"),
        1, 1000,
        slice ? slice->nb1Threshold() : 30,
        QString{},
        tr("Controls the detection threshold for impulse noise.\n"
           "Lower = more aggressive (blanks weaker impulses too).\n"
           "Higher = more conservative (only strong clicks get blanked)."));
    nb1Thresh->setProperty("nereusSetupId", "dsp.nbSnb.nb1Threshold");
    nb1Thresh->setObjectName(QStringLiteral("nb1ThresholdSlider"));

    // Transition — udDSPNBTransition: 0.01-2.00 ms, step 0.01, default 0.01.
    // Slider internal: 1-200 (×100 scale). Label shows "X.XX / 2.00 ms".
    QSlider* nb1Trans = addScaledSlider(nb1Lay, tr("Transition"),
        1, 200,
        slice ? qRound(slice->nb1TransitionMs() * 100.0) : 1,
        100.0, 2, tr(" ms"),
        tr("Time to decrease/increase to/from zero amplitude around an\n"
           "impulse. Controls how gradually the blanker fades in and out:\n"
           "very short = crisp click; longer = gentler but audible."));
    nb1Trans->setProperty("nereusSetupId", "dsp.nbSnb.nb1TransitionMs");
    nb1Trans->setProperty("nereusSetupScale", 100.0);

    // Lead — udDSPNBLead: 0.01-2.00 ms, default 0.01.
    QSlider* nb1Lead = addScaledSlider(nb1Lay, tr("Lead"),
        1, 200,
        slice ? qRound(slice->nb1LeadMs() * 100.0) : 1,
        100.0, 2, tr(" ms"),
        tr("Time at zero amplitude BEFORE the detected impulse. Blanks\n"
           "the leading edge of the click that the detector would\n"
           "otherwise miss. Raise slightly if clicks still get through."));
    nb1Lead->setProperty("nereusSetupId", "dsp.nbSnb.nb1LeadMs");
    nb1Lead->setProperty("nereusSetupScale", 100.0);

    // Lag — udDSPNBLag: 0.01-2.00 ms, default 0.01.
    QSlider* nb1Lag = addScaledSlider(nb1Lay, tr("Lag"),
        1, 200,
        slice ? qRound(slice->nb1LagMs() * 100.0) : 1,
        100.0, 2, tr(" ms"),
        tr("Time to remain at zero amplitude AFTER the impulse. Blanks\n"
           "the decay tail of the click. Raise this if pops still have\n"
           "an audible ringing after the initial transient."));
    nb1Lag->setProperty("nereusSetupId", "dsp.nbSnb.nb1LagMs");
    nb1Lag->setProperty("nereusSetupScale", 100.0);

    // NB2 Mode — Thetis comboDSPNOBmode.
    auto* nb1Mode = new QComboBox;
    nb1Mode->setProperty("nereusSetupId", "dsp.nbSnb.nb2Mode");
    // Item text matches Thetis comboDSPNOBmode verbatim (setup.designer.cs:44434 [v2.10.3.13]).
    nb1Mode->addItems({tr("Zero"), tr("Sample && Hold"), tr("Mean-Hold"),
                       tr("Hold && Sample"), tr("Linear Interpolate")});
    nb1Mode->setCurrentIndex(slice ? slice->nb2Mode() : 0);
    nb1Mode->setToolTip(tr(
        "Method used to fill in the blanked samples when NB2 triggers.\n"
        "Zero silences the impulse entirely; the other modes synthesise\n"
        "a replacement waveform from surrounding samples to reduce\n"
        "audible artifacts on voice peaks."));
    addLabeledCombo(nb1Lay, "NB2 Mode", nb1Mode);

    // ── NB2 Threshold — intentionally absent (Thetis parity) ─────────────────
    // Thetis has no NB2 threshold UI. NB2 runs at cmaster.c:68 [v2.10.3.13]
    // hardcoded default of 30.0.

    // ── SNB ───────────────────────────────────────────────────────────────────
    // Thetis grpDSPSNB (setup.designer.cs:44280-44398 [v2.10.3.13]).
    QGroupBox* snbGrp = addSection("SNB");
    QVBoxLayout* snbLay = qobject_cast<QVBoxLayout*>(snbGrp->layout());

    // The other half of the same story, and the reason the NB1 note above is
    // worth stating: SNB genuinely IS per receiver. SetRXASNBA* writes
    // rxa[channel].snba (Thetis wdsp/snb.c:621-670 [v2.10.3.15]), one per WDSP
    // channel, and NereusSDR gives every slice its own channel.
    {
        auto* snbNote = new QLabel(
            tr("Applies to the selected receiver only."));
        snbNote->setWordWrap(true);
        snbNote->setStyleSheet(QStringLiteral("color: #8aa8c0; font-size: 11px;"));
        snbLay->addWidget(snbNote);
    }

    // Threshold 1 — udDSPSNBThresh1: 2.0-20.0, step 0.1, default 8.0.
    // Slider internal: 20-200 (×10 scale).
    QSlider* snbK1 = addScaledSlider(snbLay, tr("Threshold 1"),
        20, 200,
        qRound((slice ? slice->snbK1() : 8.0) * 10.0),
        10.0, 1, QString{},
        tr("Multiple of the running noise power at which a sample is\n"
           "flagged as a candidate outlier. Lower = more aggressive\n"
           "first-pass detection; higher = miss weaker noise."));
    snbK1->setProperty("nereusSetupId", "dsp.nbSnb.snbK1");
    snbK1->setProperty("nereusSetupScale", 10.0);

    // Threshold 2 — udDSPSNBThresh2: 4.0-60.0, step 0.1, default 20.0.
    // Slider internal: 40-600 (×10 scale).
    QSlider* snbK2 = addScaledSlider(snbLay, tr("Threshold 2"),
        40, 600,
        qRound((slice ? slice->snbK2() : 20.0) * 10.0),
        10.0, 1, QString{},
        tr("Multiplier applied to the final detection threshold. It confirms\n"
           "candidates from Threshold 1 as real noise outliers. Lower =\n"
           "more aggressive overall blanking; higher = fewer false triggers\n"
           "on genuine voice peaks."));
    snbK2->setProperty("nereusSetupId", "dsp.nbSnb.snbK2");
    snbK2->setProperty("nereusSetupScale", 10.0);

    // SNB Output Bandwidth — NOT in Thetis Setup page. Thetis sets it
    // automatically per mode in rxa.cs:112-124. Kept as a NereusSDR-native
    // global override.
    QSlider* snbOutBw = addIntSlider(snbLay, tr("Output Bandwidth"),
        100, 96000,
        slice ? slice->snbOutputBandwidthHz() : 6000,
        tr(" Hz"),
        tr("Width of the audio band SNB operates on, centered on zero.\n"
           "Smaller = focuses the blanker on the active passband;\n"
           "larger = covers wider modes (FM, DRM). Default 6000 Hz\n"
           "covers SSB + AM comfortably."));
    snbOutBw->setProperty("nereusSetupId", "dsp.nbSnb.snbOutputBandwidthHz");

    // R-R3-21: the controls act on the active slice and follow it while
    // Setup is open (bindToActiveSlice). Every connection is owned by the
    // slice, so removing it while the page is open leaves nothing pointing
    // at it (the page used to keep a raw pointer to the slice it was built
    // with). No slice yet means Setup was opened before any receiver
    // existed: the controls have nothing to address, so they are disabled
    // rather than silently dropping the operator's adjustments.
    const QString noSliceTip = tr("Connect to a radio to tune the noise blanker.");
    bindToActiveSlice(this, model, receiverSelection(), [=](SliceModel* s) {
        SliceBindings conns;
        nb1Grp->setEnabled(s != nullptr);
        snbGrp->setEnabled(s != nullptr);
        nb1Grp->setToolTip(s ? QString() : noSliceTip);
        snbGrp->setToolTip(s ? QString() : noSliceTip);
        if (!s) { return conns; }

        // Show the slice's values (the value labels follow), then connect.
        auto showing = std::make_shared<bool>(false);
        const auto show = [=]() {
            *showing = true;
            nb1Thresh->setValue(s->nb1Threshold());
            nb1Trans->setValue(qRound(s->nb1TransitionMs() * 100.0));
            nb1Lead->setValue(qRound(s->nb1LeadMs() * 100.0));
            nb1Lag->setValue(qRound(s->nb1LagMs() * 100.0));
            nb1Mode->setCurrentIndex(s->nb2Mode());
            snbK1->setValue(qRound(s->snbK1() * 10.0));
            snbK2->setValue(qRound(s->snbK2() * 10.0));
            snbOutBw->setValue(s->snbOutputBandwidthHz());
            *showing = false;
        };
        show();

        // Control -> slice. The sliders hold x100 / x10 integers; the model
        // stores real values and RadioModel converts on the way to WDSP.
        const auto toSlice = [&conns, s, showing](QSlider* sl, auto apply) {
            conns << connect(sl, &QSlider::valueChanged, s, [s, showing, apply](int v) {
                if (!*showing) { apply(s, v); }
            });
        };
        toSlice(nb1Thresh, [](SliceModel* m, int v) { m->setNb1Threshold(v); });
        toSlice(nb1Trans, [](SliceModel* m, int v) { m->setNb1TransitionMs(v / 100.0); });
        toSlice(nb1Lead, [](SliceModel* m, int v) { m->setNb1LeadMs(v / 100.0); });
        toSlice(nb1Lag, [](SliceModel* m, int v) { m->setNb1LagMs(v / 100.0); });
        toSlice(snbK1, [](SliceModel* m, int v) { m->setSnbK1(v / 10.0); });
        toSlice(snbK2, [](SliceModel* m, int v) { m->setSnbK2(v / 10.0); });
        toSlice(snbOutBw, [](SliceModel* m, int v) { m->setSnbOutputBandwidthHz(v); });
        conns << connect(nb1Mode, QOverload<int>::of(&QComboBox::currentIndexChanged), s,
                         [s, showing](int v) {
            if (!*showing) { s->setNb2Mode(v); }
        });

        // Slice -> controls (the VFO flag or a remote change).
        for (auto signal : {&SliceModel::nb1ThresholdChanged, &SliceModel::nb2ModeChanged,
                            &SliceModel::snbOutputBandwidthHzChanged}) {
            conns << connect(s, signal, nb1Grp, show);
        }
        for (auto signal : {&SliceModel::nb1TransitionMsChanged, &SliceModel::nb1LeadMsChanged,
                            &SliceModel::nb1LagMsChanged, &SliceModel::snbK1Changed,
                            &SliceModel::snbK2Changed}) {
            conns << connect(s, signal, nb1Grp, show);
        }
        return conns;
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
CwSetupPage::CwSetupPage(RadioModel* model, QWidget* parent,
                         DspReceiverSelection* selection)
    : ReceiverDspSetupPage("CW", model, parent, selection)
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
    // R-R3-49: the CW keyer settings are hidden until CWX and CW transmit
    // are built.
    keyerGrp->setObjectName(QStringLiteral("cwKeyerGroup"));
    UnbuiltFeatures::hideUnlessBuilt(keyerGrp, UnbuiltFeature::Cwx);

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
    // Thetis upstream comparison (setup.cs [v2.10.3.13+501e3f51]
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
    // R-R3-49: break-in delay and sidetone are keyer settings too.
    timingGrp->setObjectName(QStringLiteral("cwTimingGroup"));
    UnbuiltFeatures::hideUnlessBuilt(timingGrp, UnbuiltFeature::Cwx);

    // ── APF ───────────────────────────────────────────────────────────────────
    QGroupBox* apfGrp = addSection("APF");
    QVBoxLayout* apfLay = qobject_cast<QVBoxLayout*>(apfGrp->layout());

    auto* apfEnable = new QPushButton("Enable");
    apfEnable->setProperty("nereusSetupId", "dsp.cw.apfEnabled");
    apfEnable->setCheckable(true);
    apfEnable->setObjectName(QStringLiteral("apfEnableButton"));
    addLabeledToggle(apfLay, "Enable", apfEnable);

    // R-R3-21: Enable and Center Freq are the active slice's APF, the VFO
    // flag's APF button and tune slider (SliceModel apfEnabled /
    // apfTuneHz). The centre is the CW pitch plus the slice's tune offset,
    // as RadioModel sends it: From Thetis setup.cs:17071 [v2.10.3.13] --
    // freq = CWPitch + tuneOffset, with RadioModel's pitch
    // (RadioModel::kApfCwPitchHz). The range is the flag slider's
    // -500..+500 Hz around it.
    static constexpr int kApfPitchHz = RadioModel::kApfCwPitchHz;
    auto* apfCenter = new QSlider(Qt::Horizontal);
    apfCenter->setObjectName(QStringLiteral("apfCenterSlider"));
    apfCenter->setProperty("nereusSetupId", "dsp.cw.apfCenter");
    apfCenter->setRange(kApfPitchHz - 500, kApfPitchHz + 500);
    auto* apfCenterValue = new QLabel;
    addLabeledSlider(apfLay, "Center Freq", apfCenter, apfCenterValue);

    // No slice setting holds these two yet; they stay as they were.
    auto* apfBw = new QSlider(Qt::Horizontal);
    apfBw->setRange(10, 500);
    apfBw->setEnabled(false);
    apfBw->setObjectName(QStringLiteral("apfBandwidthSlider"));
    // R-R3-49: hidden until a slice setting holds them.
    UnbuiltFeatures::hideLayoutUnlessBuilt(addLabeledSlider(apfLay, "Bandwidth", apfBw),
                                           UnbuiltFeature::ApfParams);

    auto* apfGain = new QSlider(Qt::Horizontal);
    apfGain->setRange(0, 100);
    apfGain->setEnabled(false);
    apfGain->setObjectName(QStringLiteral("apfGainSlider"));
    UnbuiltFeatures::hideLayoutUnlessBuilt(addLabeledSlider(apfLay, "Gain", apfGain),
                                           UnbuiltFeature::ApfParams);

    bindToActiveSlice(this, model, receiverSelection(),
                      [apfEnable, apfCenter, apfCenterValue](SliceModel* s) {
        SliceBindings conns;
        setSliceAvailable(apfEnable, s != nullptr);
        setSliceAvailable(apfCenter, s != nullptr);
        if (!s) { return conns; }
        {
            QSignalBlocker blockEnable(apfEnable);
            QSignalBlocker blockCenter(apfCenter);
            apfEnable->setChecked(s->apfEnabled());
            apfCenter->setValue(kApfPitchHz + s->apfTuneHz());
        }
        apfCenterValue->setText(QStringLiteral("%1 Hz").arg(apfCenter->value()));
        conns << connect(apfEnable, &QPushButton::toggled, s, &SliceModel::setApfEnabled);
        conns << connect(apfCenter, &QSlider::valueChanged, s, [s, apfCenterValue](int hz) {
            apfCenterValue->setText(QStringLiteral("%1 Hz").arg(hz));
            s->setApfTuneHz(hz - kApfPitchHz);
        });
        conns << connect(s, &SliceModel::apfEnabledChanged, apfEnable, [apfEnable](bool on) {
            QSignalBlocker block(apfEnable);
            apfEnable->setChecked(on);
        });
        conns << connect(s, &SliceModel::apfTuneHzChanged, apfCenter,
                         [apfCenter, apfCenterValue](int tune) {
            QSignalBlocker block(apfCenter);
            apfCenter->setValue(kApfPitchHz + tune);
            apfCenterValue->setText(QStringLiteral("%1 Hz").arg(apfCenter->value()));
        });
        return conns;
    });

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
        } else if (!model->ownsLocalDsp()
                   && !model->currentRadioInfo().macAddress.isEmpty()) {
            // R-R3-46 (carried): a remote window has no connection of its
            // own; the Core's radio, once known, is the model's stored
            // radio info, and currentRadioChanged fired before this page
            // was built.
            const auto& caps = NereusSDR::BoardCapsTable::forBoard(
                model->currentRadioInfo().boardType);
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
//   comboSAMFadeLevel, comboSAMDSBMode,
//   tbAMSquelchThreshold, udAMSquelchMaxTail
//
// Issue #175 Wave 1 follow-up — the disabled "AM TX / Carrier Level"
// stub that previously lived here was a misplaced surface.  The control
// belongs at Thetis grpTXAM on tpTransmit (mi0bot setup.designer.cs:
// 47710-47711 [v2.10.3.13-beta2]), not on the DSP/AM tab.  NOTE: the
// Thetis DSP/AM tab DOES host a different TX-side group, grpAMTX (Tx
// USB/LSB/DSB sideband select radios), at mi0bot setup.designer.cs:
// 40200, 40326-40336 [v2.10.3.13-beta2].  NereusSDR is missing this
// group entirely; tracked as a separate follow-up issue, not this PR.
//
AmSamSetupPage::AmSamSetupPage(RadioModel* model, QWidget* parent,
                               DspReceiverSelection* selection)
    : ReceiverDspSetupPage("AM/SAM", model, parent, selection)
{
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
    // R-R3-49: the synchronous AM options are hidden until they are built.
    samGrp->setObjectName(QStringLiteral("samGroup"));
    UnbuiltFeatures::hideUnlessBuilt(samGrp, UnbuiltFeature::Sam);

    // ── Squelch ───────────────────────────────────────────────────────────────
    QGroupBox* sqGrp = addSection("Squelch");
    QVBoxLayout* sqLay = qobject_cast<QVBoxLayout*>(sqGrp->layout());

    // R-R3-21: the active slice's AM squelch threshold (SliceModel
    // amsqThresh, dB, sent to WDSP SetRXAAMSQThreshold by RadioModel).
    auto* sqThresh = new QSlider(Qt::Horizontal);
    sqThresh->setProperty("nereusSetupId", "dsp.amSam.amsqThresh");
    sqThresh->setObjectName(QStringLiteral("amSquelchThresholdSlider"));
    sqThresh->setRange(ControlRanges::kAmsqThreshMinDb, ControlRanges::kAmsqThreshMaxDb);
    sqThresh->setSingleStep(ControlRanges::kAmsqThreshStepDb);
    auto* sqThreshValue = new QLabel;
    addLabeledSlider(sqLay, "AM Squelch Threshold", sqThresh, sqThreshValue);
    bindSquelchThreshold(this, model, receiverSelection(), sqThresh, sqThreshValue,
                         &SliceModel::amsqThresh, &SliceModel::setAmsqThresh,
                         &SliceModel::amsqThreshChanged);

    // No slice setting holds the tail yet; it stays as it was.
    auto* sqMaxTail = new QSpinBox;
    sqMaxTail->setRange(1, 1000);
    sqMaxTail->setSuffix(" ms");
    sqMaxTail->setEnabled(false);
    sqMaxTail->setObjectName(QStringLiteral("amSquelchMaxTailSpin"));
    // R-R3-49: hidden until a slice setting holds it.
    UnbuiltFeatures::hideLayoutUnlessBuilt(addLabeledSpinner(sqLay, "Max Tail", sqMaxTail),
                                           UnbuiltFeature::AmSquelchTail);
}

// ══════════════════════════════════════════════════════════════════════════════
// FmSetupPage
// ══════════════════════════════════════════════════════════════════════════════
//
// From Thetis setup.cs — tabDSP / tabPageFM controls:
//   comboFMDeviation, tbFMSquelchThreshold, chkFMDeEmphasis,
//   comboFMTXDeviation, tbFMMicGain, comboFMTXEmphasisPosition
//
FmSetupPage::FmSetupPage(RadioModel* model, QWidget* parent,
                         DspReceiverSelection* selection)
    : ReceiverDspSetupPage("FM", model, parent, selection)
{
    // ── RX ────────────────────────────────────────────────────────────────────
    QGroupBox* rxGrp = addSection("RX");
    QVBoxLayout* rxLay = qobject_cast<QVBoxLayout*>(rxGrp->layout());

    auto* rxDeviation = new QComboBox;
    rxDeviation->addItems({"5k", "2.5k"});
    rxDeviation->setObjectName(QStringLiteral("fmRxDeviationCombo"));
    // R-R3-49: hidden until a slice setting holds it.
    UnbuiltFeatures::hideLayoutUnlessBuilt(addLabeledCombo(rxLay, "Deviation", rxDeviation),
                                           UnbuiltFeature::FmDeviation);

    // R-R3-21: the active slice's FM squelch threshold (SliceModel
    // fmsqThresh, dB; RxChannel::setFmsqThresh converts it for WDSP).
    auto* squelchThresh = new QSlider(Qt::Horizontal);
    squelchThresh->setProperty("nereusSetupId", "dsp.fm.fmsqThresh");
    squelchThresh->setObjectName(QStringLiteral("fmSquelchThresholdSlider"));
    squelchThresh->setRange(ControlRanges::kFmsqThreshMinDb, ControlRanges::kFmsqThreshMaxDb);
    squelchThresh->setSingleStep(ControlRanges::kFmsqThreshStepDb);
    auto* squelchThreshValue = new QLabel;
    addLabeledSlider(rxLay, "Squelch Threshold", squelchThresh, squelchThreshValue);
    bindSquelchThreshold(this, model, receiverSelection(), squelchThresh,
                         squelchThreshValue, &SliceModel::fmsqThresh,
                         &SliceModel::setFmsqThresh, &SliceModel::fmsqThreshChanged);

    // No slice setting holds these two yet; they stay as they were.
    rxDeviation->setEnabled(false);
    auto* deEmphasis = new QPushButton("Enable");
    deEmphasis->setEnabled(false);
    deEmphasis->setObjectName(QStringLiteral("fmDeEmphasisButton"));
    UnbuiltFeatures::hideLayoutUnlessBuilt(addLabeledToggle(rxLay, "De-Emphasis", deEmphasis),
                                           UnbuiltFeature::FmDeviation);

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
    // R-R3-49 (fm-tx): hidden until FM transmit is built.
    txGrp->setObjectName(QStringLiteral("fmTxGroup"));
    UnbuiltFeatures::hideUnlessBuilt(txGrp, UnbuiltFeature::FmTransmit);
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
    m_phRotEnableChk->setProperty("nereusSetupId", "dsp.cfc.phaseRotatorEnabled");
    m_phRotEnableChk->setObjectName(QStringLiteral("chkPHROTEnable"));
    m_phRotEnableChk->setChecked(tx.phaseRotatorEnabled());
    // From Thetis setup.Designer.cs:46281 [v2.10.3.13] — chkPHROTEnable tooltip.
    m_phRotEnableChk->setToolTip(QStringLiteral("Turn the phase rotator on or off"));
    phRotLay->addWidget(m_phRotEnableChk);

    m_phRotFreqSpin = new QSpinBox;
    m_phRotFreqSpin->setProperty("nereusSetupId", "dsp.cfc.phaseRotatorFreqHz");
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
    m_phRotStagesSpin->setProperty("nereusSetupId", "dsp.cfc.phaseRotatorStages");
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
    m_phRotReverseChk->setProperty("nereusSetupId", "dsp.cfc.phaseReverseEnabled");
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
    m_cfcEnableChk->setProperty("nereusSetupId", "dsp.cfc.cfcEnabled");
    m_cfcEnableChk->setObjectName(QStringLiteral("chkCFCEnable"));
    m_cfcEnableChk->setChecked(tx.cfcEnabled());
    m_cfcEnableChk->setToolTip(QStringLiteral(
        "Enable the Continuous Frequency Compressor (10-band)"));
    cfcLay->addWidget(m_cfcEnableChk);

    m_cfcPostEqEnableChk = new QCheckBox("Post-EQ Enable");
    m_cfcPostEqEnableChk->setProperty("nereusSetupId", "dsp.cfc.cfcPostEqEnabled");
    m_cfcPostEqEnableChk->setObjectName(QStringLiteral("chkCFCPeqEnable"));
    m_cfcPostEqEnableChk->setChecked(tx.cfcPostEqEnabled());
    m_cfcPostEqEnableChk->setToolTip(QStringLiteral(
        "Enable the Post-EQ stage applied after CFC compression"));
    cfcLay->addWidget(m_cfcPostEqEnableChk);

    m_cfcPrecompSpin = new QSpinBox;
    m_cfcPrecompSpin->setProperty("nereusSetupId", "dsp.cfc.cfcPrecompDb");
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
    m_cfcPostEqGainSpin->setProperty("nereusSetupId", "dsp.cfc.cfcPostEqGainDb");
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
    // Setup description version 19 publishes this editor as dsp.cfc.bands;
    // the row's tooltip is this one.
    m_cfcBandsBtn->setProperty("nereusSetupId", QStringLiteral("dsp.cfc.bands"));
    m_cfcBandsBtn->setToolTip(QStringLiteral(
        "Open the per-band CFC editor: 5, 10 or 18 bands of compression and post-EQ."));
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
    m_cessbEnableChk->setProperty("nereusSetupId", "dsp.cfc.cessbOn");
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

    // R-R3-49 (parity Task 4): every setting here is on the Core's
    // `transmit` object (transmitSettingsVersion 4). In a remote window they
    // start closed until SetupDialog pushes that gate. [Configure CFC
    // bands] stays live: the dialog shows why it is greyed.
    if (!model->ownsLocalDsp()) {
        setTransmitSettingsPermittedAt(4, false, QString());
    }
}

void CfcSetupPage::setTransmitSettingsPermittedAt(int version, bool permitted,
                                                  const QString& reason)
{
    if (version != 4) {
        return;
    }
    gateTransmitControls({m_phRotEnableChk, m_phRotReverseChk, m_phRotFreqSpin,
                          m_phRotStagesSpin, m_cfcEnableChk, m_cfcPostEqEnableChk,
                          m_cfcPrecompSpin, m_cfcPostEqGainSpin, m_cessbEnableChk},
        permitted,
        reason.isEmpty() ? IStationLink::transmitSettingsUnavailableReason() : reason);
}

// ── MNF editor ranges ─────────────────────────────────────────────────────────
// The centre bounds and the width ceiling belong to NotchModel, which does the
// actual clamping; only the two the model has no equivalent for are declared
// here.
// From Thetis setup.designer.cs:44334-44338 [v2.10.3.15] — udMNFWidth.Minimum.
static constexpr double kMnfWidthMinHz = 0.0;
// From Thetis setup.designer.cs:44323-44327 [v2.10.3.15] — udMNFWidth.Increment.
static constexpr double kMnfWidthStepHz = 1.0;

// ══════════════════════════════════════════════════════════════════════════════
// MnfSetupPage
// ══════════════════════════════════════════════════════════════════════════════
//
// Setup → DSP → MNF. The tab keeps Thetis's name (setup.designer.cs:44141
// [v2.10.3.15], this.tpDSPMNF.Text = "MNF") and the group box keeps Thetis's
// caption (:44165, grpDSPMNF.Text = "Multi Notch Filter"); everything
// operator-facing outside Settings says TNF.
//
// grpDSPMNF's control set is at setup.designer.cs:44145-44159 [v2.10.3.15].
// This page keeps those object names and their verbatim tooltips but replaces
// the upstream one-notch-at-a-time shape (udMNFNotch index spinner plus
// Add / Edit / Enter / Cancel modal buttons) with a table that edits every
// notch in place.
//
// The prior placeholder carried a "Window" combo. No such control exists
// upstream (there is no comboMNFWindow anywhere in Thetis v2.10.3.15) and the
// bandpass window is out of scope, so it is dropped rather than wired.
MnfSetupPage::MnfSetupPage(RadioModel* model, QWidget* parent,
                           DspReceiverSelection* selection)
    : ReceiverDspSetupPage("TNF", model, parent, selection)
{
    // Thetis captions this group "Multi Notch Filter" and names the tab
    // MNF (setup.designer.cs:44165, :44141 [v2.10.3.15]). NereusSDR says TNF
    // everywhere instead (maintainer decision, 2026-08-02): Thetis is itself
    // split, MNF on the Setup tab and chkTNF on the console, and carrying
    // that split through meant the same feature had two names on screen at
    // once. Deliberate naming divergence; the control objectNames below stay
    // on Thetis's spellings because they are the source-first mapping.
    QGroupBox* mnfGrp = addSection(QStringLiteral("Tunable Notch Filter"));
    QVBoxLayout* mnfLay = qobject_cast<QVBoxLayout*>(mnfGrp->layout());

    if (!model || !model->notchModel() || !mnfLay) {
        disableGroup(mnfGrp);
        return;
    }

    NotchModel* nm = model->notchModel();

    // ── Notch table ──────────────────────────────────────────────────────────
    m_notchTable = new QTableWidget(0, 4, mnfGrp);
    m_notchTable->setObjectName(QStringLiteral("tblMNFNotches"));
    m_notchTable->setProperty("nereusSetupId", "dsp.tnf.list");
    // Column captions follow Thetis lblMNFFreq / lblMNFWidth / chkMNFActive
    // (setup.designer.cs:44308, :44298, :44260 [v2.10.3.15]); the frequency
    // column is Hz here where upstream is MHz.
    m_notchTable->setHorizontalHeaderLabels({
        QStringLiteral("Center Frequency (Hz)"),
        QStringLiteral("Width (Hz)"),
        QStringLiteral("Active"),
        QString()
    });
    m_notchTable->setStyleSheet(QStringLiteral(
        "QTableWidget { background: #131326; color: #c8d8e8; "
        "  gridline-color: #304050; border: 1px solid #304050; }"
        "QTableWidget::item { padding: 2px 4px; }"
        "QTableWidget::item:selected { background: #1a3050; }"
        "QHeaderView::section { background: #1a2030; color: #8aa8c0; "
        "  border: 1px solid #304050; padding: 4px; }"));
    m_notchTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_notchTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_notchTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_notchTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_notchTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_notchTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_notchTable->setColumnWidth(1, 110);
    m_notchTable->setColumnWidth(2, 60);
    m_notchTable->setColumnWidth(3, 80);
    m_notchTable->verticalHeader()->setVisible(false);
    m_notchTable->setMinimumHeight(160);
    mnfLay->addWidget(m_notchTable);

    // ── Add ──────────────────────────────────────────────────────────────────
    static const QString kMnfButtonStyle = QStringLiteral(
        "QPushButton { background: #203040; color: #c8d8e8; border: 1px solid #304050; "
        "  border-radius: 3px; padding: 4px 10px; }"
        "QPushButton:hover { background: #2a4060; }"
        "QPushButton:pressed { background: #1a2840; }");

    m_addBtn = new QPushButton(QStringLiteral("Add"), mnfGrp);
    m_addBtn->setObjectName(QStringLiteral("btnMNFAdd"));
    m_addBtn->setProperty("nereusSetupId", "dsp.tnf.add");
    // From Thetis setup.designer.cs:44286 [v2.10.3.15] — btnMNFAdd tooltip.
    m_addBtn->setToolTip(QStringLiteral("Add a notch"));
    m_addBtn->setStyleSheet(kMnfButtonStyle);

    auto* addRow = new QHBoxLayout;
    addRow->setContentsMargins(0, 0, 0, 0);
    addRow->setSpacing(8);
    addRow->addWidget(m_addBtn);
    addRow->addStretch();
    mnfLay->addLayout(addRow);

    connect(m_addBtn, &QPushButton::clicked, this, [this] {
        // SetupPage::model(), qualified: the ctor parameter of the same name
        // is still in scope inside this lambda and would shadow the accessor.
        RadioModel* rm = SetupPage::model();
        if (!rm || !rm->notchModel()) { return; }
        SliceModel* slice = selectedSlice();
        if (!slice) { return; }
        // Thetis splits this into two clicks: btnMNFAdd opens an empty row and
        // btnVFOFreq fills it from VFOA ("Enter the Frequency from VFOA",
        // setup.designer.cs:44190 [v2.10.3.15]). One gesture here, because the
        // table edits in place.
        endAdminEdit();
        // Through RadioModel so the width is clamped to what this slice's
        // filter can realise, exactly as the panadapter and +TNF routes do.
        // A bare addNotch here stored the 200 Hz default even where the live
        // minimum is 400 Hz (nc 1024), and WDSP widened it silently while this
        // very table kept showing 200. Codex review of PR #313.
        rm->addNotchForSlice(slice, slice->frequency(),
                             NotchModel::kDefaultNotchWidthHz);
    });

    // ── Minimum notch width ──────────────────────────────────────────────────
    // Narrowest notch the current bandpass can realise: WDSP min_notch_width
    // (third_party/wdsp/src/nbp.c:82-96), read through RXANBPGetMinNotchWidth
    // (nbp.c:594). NereusSDR-original control; Thetis pushes the same value
    // out of UpdateMinimumNotchWidthRX (console.cs:48787-48818 [v2.10.3.15])
    // to its notch popup rather than to the Setup tab.
    m_minWidthLbl = new QLabel(QStringLiteral("--"), mnfGrp);
    m_minWidthLbl->setObjectName(QStringLiteral("lblMNFMinWidth"));
    m_minWidthLbl->setProperty("nereusSetupId", "dsp.tnf.minNotchWidthHz");
    m_minWidthLbl->setToolTip(QStringLiteral(
        "Narrowest notch the current bandpass filter can realise"));
    addLabeledLabel(mnfLay, QStringLiteral("Minimum Notch Width"), m_minWidthLbl);

    // The readout follows the active slice's channel, so it has to re-resolve
    // when the operator changes which slice that is.
    watchReceiverSelection([this] {
        m_addBtn->setEnabled(selectedSlice() != nullptr);
        refreshMinNotchWidth();
    });
    m_addBtn->setEnabled(selectedSlice() != nullptr);

    // ── Auto-increase ────────────────────────────────────────────────────────
    // From Thetis setup.designer.cs:44204 [v2.10.3.15] — chkMNFAutoIncrease.Text.
    m_autoIncreaseChk = new QCheckBox(
        QStringLiteral("Auto-Increase width (if needed) to achieve >100dB attenuation"),
        mnfGrp);
    m_autoIncreaseChk->setObjectName(QStringLiteral("chkMNFAutoIncrease"));
    m_autoIncreaseChk->setProperty("nereusSetupId", "dsp.tnf.autoIncrease");
    m_autoIncreaseChk->setChecked(nm->autoIncrease());
    // From Thetis setup.designer.cs:44205 [v2.10.3.15] — chkMNFAutoIncrease tooltip.
    m_autoIncreaseChk->setToolTip(QStringLiteral(
        "The notch width will be increased if needed to ensure >100dB of attenuation"));
    mnfLay->addWidget(m_autoIncreaseChk);

    // Thetis fans the flag straight to three fixed channel ids from the Setup
    // form (setup.cs:17925-17931 [v2.10.3.15], chkMNFAutoIncrease_CheckedChanged
    // → WDSP.RXANBPSetAutoIncrease ×3). NereusSDR routes it through NotchModel
    // so RadioModel's fan-out reaches every open slice channel instead.
    connect(m_autoIncreaseChk, &QCheckBox::toggled, nm, &NotchModel::setAutoIncrease);
    connect(nm, &NotchModel::autoIncreaseChanged, m_autoIncreaseChk, [this](bool on) {
        QSignalBlocker b(m_autoIncreaseChk);
        m_autoIncreaseChk->setChecked(on);
    });

    // ── Visual notch ─────────────────────────────────────────────────────────
    // From Thetis setup.designer.cs:44167-44179 [v2.10.3.15] — chkVisualNotch,
    // the last control in grpDSPMNF (:44145). Caption (:44175-44176) and
    // tooltip (:44177) copied verbatim. The designer makes no `Checked`
    // assignment, so Windows Forms leaves it unchecked; NotchModel's default
    // false matches.
    m_visualNotchChk = new QCheckBox(
        QStringLiteral("Visual approximation of notch (NOTE: this is not 100% "
                       "representation of the active notch)"),
        mnfGrp);
    m_visualNotchChk->setObjectName(QStringLiteral("chkVisualNotch"));
    m_visualNotchChk->setProperty("nereusSetupId", "dsp.tnf.visualNotch");
    m_visualNotchChk->setChecked(nm->visualEnabled());
    m_visualNotchChk->setToolTip(QStringLiteral(
        "This is a simple approximation and does not accurately represent the notch"));
    mnfLay->addWidget(m_visualNotchChk);

    // From Thetis setup.cs:24376-24380 [v2.10.3.15] —
    // chkVisualNotch_CheckedChanged sets Display.ShowVisualNotch AND
    // MiniSpec.ShowVisualNotch. NereusSDR has no mini-spectrum surface, so
    // only the panadapter half is ported; the fan-out to every pan hangs off
    // NotchModel::visualEnabledChanged rather than off this widget.
    connect(m_visualNotchChk, &QCheckBox::toggled, nm, &NotchModel::setVisualEnabled);
    connect(nm, &NotchModel::visualEnabledChanged, m_visualNotchChk, [this](bool on) {
        QSignalBlocker b(m_visualNotchChk);
        m_visualNotchChk->setChecked(on);
    });

    // ── Wiring: notch list ↔ table ───────────────────────────────────────────
    // Structural changes go through a queued rebuild; a direct connection would
    // let removeNotch() delete the row Delete button from inside that button's
    // own clicked() emission.
    connect(nm, &NotchModel::notchAdded, this,
            [this](int) { rebuildTable(); }, Qt::QueuedConnection);
    connect(nm, &NotchModel::notchRemoved, this,
            [this](int, int) { rebuildTable(); }, Qt::QueuedConnection);
    connect(nm, &NotchModel::notchesReset, this,
            &MnfSetupPage::rebuildTable, Qt::QueuedConnection);

    // Value-only changes refresh the row in place. Direct, not queued: it
    // rewrites the existing editors instead of replacing them, so it is safe
    // even when it lands inside a spin box's own editingFinished.
    connect(nm, &NotchModel::notchChanged, this, &MnfSetupPage::refreshRow);

    rebuildTable();
    refreshMinNotchWidth();
}

// ── MnfSetupPage::rebuildTable ────────────────────────────────────────────────

void MnfSetupPage::rebuildTable()
{
    RadioModel* rm = model();
    if (!m_notchTable || !rm || !rm->notchModel()) { return; }

    static const QString kMnfEditorStyle = QStringLiteral(
        "QDoubleSpinBox { background: #1a2030; color: #c8d8e8; "
        "  border: 1px solid #304050; border-radius: 2px; padding: 1px; }"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button "
        "  { background: #202838; width: 14px; }");
    static const QString kMnfRowButtonStyle = QStringLiteral(
        "QPushButton { background: #203040; color: #c8d8e8; border: 1px solid #304050; "
        "  border-radius: 2px; padding: 1px 6px; font-size: 11px; }"
        "QPushButton:hover { background: #2a4060; }");

    // setRowCount() destroys the outgoing cell widgets; a focused spin box
    // being destroyed emits editingFinished on its way out, so the guard has
    // to be up before the row count moves.
    m_rebuilding = true;

    const QList<Notch>& notches = rm->notchModel()->notches();
    const int count = static_cast<int>(notches.size());

    m_rowIds.clear();
    m_rowIds.reserve(count);
    m_notchTable->setRowCount(count);

    for (int row = 0; row < count; ++row) {
        const Notch& n = notches.at(row);
        const int id = n.id;
        m_rowIds.append(id);

        // Col 0: centre frequency. Thetis's udMNFFreq is MHz over 0..1000000
        // (setup.designer.cs:44352-44369 [v2.10.3.15]); this column is Hz, so
        // the editor takes the bounds NotchModel itself constrains to.
        auto* freqSpin = new QDoubleSpinBox(m_notchTable);
        freqSpin->setObjectName(QStringLiteral("udMNFFreq"));
        freqSpin->setProperty("nereusSetupId", "dsp.tnf.list.centreHz");
        freqSpin->setDecimals(0);
        freqSpin->setRange(NotchModel::kMinNotchCentreHz,
                           NotchModel::kMaxNotchCentreHz);
        freqSpin->setSingleStep(1.0);
        freqSpin->setValue(n.centerHz);
        freqSpin->setStyleSheet(kMnfEditorStyle);
        // From Thetis setup.designer.cs:44374 [v2.10.3.15] — udMNFFreq tooltip.
        freqSpin->setToolTip(QStringLiteral("Center frequency of the notch"));
        // Connected after the value is pushed, so construction cannot read as
        // an operator edit and raise the lock.
        connect(freqSpin, &QDoubleSpinBox::valueChanged, this,
                [this](double) { beginAdminEdit(); });
        connect(freqSpin, &QDoubleSpinBox::editingFinished, this,
                [this, id] { commitRow(id); });
        m_notchTable->setCellWidget(row, 0, freqSpin);

        // Col 1: width.
        auto* widthSpin = new QDoubleSpinBox(m_notchTable);
        widthSpin->setObjectName(QStringLiteral("udMNFWidth"));
        widthSpin->setProperty("nereusSetupId", "dsp.tnf.list.widthHz");
        widthSpin->setDecimals(0);
        widthSpin->setRange(kMnfWidthMinHz, NotchModel::kMaxNotchWidthHz);
        widthSpin->setSingleStep(kMnfWidthStepHz);
        widthSpin->setValue(n.widthHz);
        widthSpin->setStyleSheet(kMnfEditorStyle);
        // From Thetis setup.designer.cs:44343 [v2.10.3.15] — udMNFWidth tooltip
        // (upstream spelling preserved verbatim).
        widthSpin->setToolTip(QStringLiteral("Bandwdith of the notch"));
        connect(widthSpin, &QDoubleSpinBox::valueChanged, this,
                [this](double) { beginAdminEdit(); });
        connect(widthSpin, &QDoubleSpinBox::editingFinished, this,
                [this, id] { commitRow(id); });
        m_notchTable->setCellWidget(row, 1, widthSpin);

        // Col 2: active.
        auto* activeChk = new QCheckBox(m_notchTable);
        activeChk->setObjectName(QStringLiteral("chkMNFActive"));
        activeChk->setProperty("nereusSetupId", "dsp.tnf.list.active");
        activeChk->setChecked(n.active);
        // From Thetis setup.designer.cs:44261 [v2.10.3.15] — chkMNFActive tooltip.
        activeChk->setToolTip(QStringLiteral("Checked if the notch is active"));
        connect(activeChk, &QCheckBox::toggled, this, [this, id](bool on) {
            if (m_rebuilding) { return; }
            RadioModel* r = model();
            if (!r || !r->notchModel()) { return; }
            endAdminEdit();
            r->notchModel()->setActive(id, on);
        });
        m_notchTable->setCellWidget(row, 2, activeChk);

        // Col 3: delete.
        auto* delBtn = new QPushButton(QStringLiteral("Delete"), m_notchTable);
        delBtn->setObjectName(QStringLiteral("btnMNFDelete"));
        delBtn->setProperty("nereusSetupId", "dsp.tnf.list.delete");
        // From Thetis setup.designer.cs:44219 [v2.10.3.15] — btnMNFDelete tooltip.
        delBtn->setToolTip(QStringLiteral("Delete the current notch index"));
        delBtn->setStyleSheet(kMnfRowButtonStyle);
        connect(delBtn, &QPushButton::clicked, this, [this, id] {
            RadioModel* r = model();
            if (!r || !r->notchModel()) { return; }
            endAdminEdit();
            r->notchModel()->removeNotch(id);
        });
        m_notchTable->setCellWidget(row, 3, delBtn);

        m_notchTable->setRowHeight(row, 26);
    }

    m_rebuilding = false;
}

// ── MnfSetupPage::refreshRow ──────────────────────────────────────────────────

void MnfSetupPage::refreshRow(int notchId)
{
    RadioModel* rm = model();
    if (!m_notchTable || !rm || !rm->notchModel()) { return; }

    const int row = m_rowIds.indexOf(notchId);
    if (row < 0) { return; }

    const Notch* n = rm->notchModel()->notchById(notchId);
    if (!n) { return; }

    m_rebuilding = true;
    if (auto* freqSpin = qobject_cast<QDoubleSpinBox*>(m_notchTable->cellWidget(row, 0))) {
        QSignalBlocker b(freqSpin);
        freqSpin->setValue(n->centerHz);
    }
    if (auto* widthSpin = qobject_cast<QDoubleSpinBox*>(m_notchTable->cellWidget(row, 1))) {
        QSignalBlocker b(widthSpin);
        widthSpin->setValue(n->widthHz);
    }
    if (auto* activeChk = qobject_cast<QCheckBox*>(m_notchTable->cellWidget(row, 2))) {
        QSignalBlocker b(activeChk);
        activeChk->setChecked(n->active);
    }
    m_rebuilding = false;
}

// ── MnfSetupPage::commitRow ───────────────────────────────────────────────────

void MnfSetupPage::commitRow(int notchId)
{
    if (m_rebuilding) { return; }

    RadioModel* rm = model();
    if (!m_notchTable || !rm || !rm->notchModel()) { return; }

    const int row = m_rowIds.indexOf(notchId);
    if (row < 0) { return; }

    auto* freqSpin  = qobject_cast<QDoubleSpinBox*>(m_notchTable->cellWidget(row, 0));
    auto* widthSpin = qobject_cast<QDoubleSpinBox*>(m_notchTable->cellWidget(row, 1));
    if (!freqSpin || !widthSpin) { return; }

    // Lock down first, exactly as Thetis's ENTER does: btnMNFEnter_Click
    // (setup.cs:17738 [v2.10.3.15]) sets AddActive false at :17744 before
    // RXANBPAddNotch at :17749-17751, and EditActive false at :17759 before
    // RXANBPEditNotch at :17766-17768. NotchModel's mutators are shared with
    // the panadapter path and reject writes while adminBusy is set
    // (console.cs:40009, :40079 [v2.10.3.15]), so this ordering is required
    // for the page's own write to land at all.
    endAdminEdit();

    NotchModel* nm = rm->notchModel();
    nm->setCenter(notchId, freqSpin->value());
    nm->setWidth(notchId, widthSpin->value());
    // R-R3-21: on a remote window the two halves travel to the Core as one
    // notch.move, sent now rather than at the end of a drag window. Nothing
    // is held locally, so this is a no-op there.
    nm->flushPendingMoves();
}

// ── MnfSetupPage::beginAdminEdit ──────────────────────────────────────────────

void MnfSetupPage::beginAdminEdit()
{
    if (m_rebuilding) { return; }
    RadioModel* rm = model();
    if (!rm || !rm->notchModel()) { return; }
    // Thetis raises the flag on btnMNFAdd (AddActive = true, setup.cs:17679
    // [v2.10.3.15]) and btnMNFEdit (EditActive = true, :17718), and every
    // console-side notch mutator bails on it (console.cs:40009, 40079, 40125,
    // 40161, 40200, 40224, 40315 [v2.10.3.15]). An in-place table edit is the
    // same window: it opens on the first value change and closes when the row
    // commits.
    rm->notchModel()->setAdminBusy(true);
}

// ── MnfSetupPage::endAdminEdit ────────────────────────────────────────────────

void MnfSetupPage::endAdminEdit()
{
    RadioModel* rm = model();
    if (!rm || !rm->notchModel()) { return; }
    // Thetis clears the flag before it writes: btnMNFEnter_Click
    // (setup.cs:17738 [v2.10.3.15]) sets `AddActive = false` at :17744 and
    // only then calls WDSP.RXANBPAddNotch at :17749-17751, and likewise sets
    // `EditActive = false` at :17759 before WDSP.RXANBPEditNotch at
    // :17766-17768. NotchAdminBusy is AddActive | EditActive
    // (:17728-17735), so clearing both is what unlocks the write.
    rm->notchModel()->setAdminBusy(false);
}

// ── MnfSetupPage::refreshMinNotchWidth ────────────────────────────────────────

void MnfSetupPage::refreshMinNotchWidth()
{
    if (!m_minWidthLbl) { return; }

    // Whichever channel was being followed is no longer necessarily the right
    // one; re-arm below against the channel actually resolved this pass.
    disconnect(m_minWidthConn);

    RadioModel* rm = model();
    if (!rm) {
        m_minWidthLbl->setText(QStringLiteral("--"));
        return;
    }

    // R-R3-49 (parity Task 16): a remote window has no channel; the active
    // slice carries the Core's channel's minimum (dspInfoVersion 1).
    if (rm->role() == RadioModel::Role::Remote) {
        static const QString kOwnTip = QStringLiteral(
            "Narrowest notch the current bandpass filter can realise");
        const auto show = [this, rm](double minWidthHz) {
            if (!m_minWidthLbl) { return; }
            const bool known = rm->stationDspInfoVersion() >= 1 && minWidthHz > 0.0;
            m_minWidthLbl->setText(known ? QStringLiteral("%1 Hz").arg(minWidthHz, 0, 'f', 1)
                                         : QStringLiteral("--"));
            m_minWidthLbl->setToolTip(rm->stationDspInfoVersion() >= 1
                ? kOwnTip
                : QStringLiteral("This Core does not say how narrow a notch it can make. "
                                 "Updating the Core may help."));
        };
        SliceModel* slice = selectedSlice();
        if (!slice) {
            show(0.0);
            return;
        }
        m_minWidthConn = connect(slice, &SliceModel::minNotchWidthHzChanged, this, show);
        show(slice->minNotchWidthHz());
        return;
    }

    // Thetis surfaces this per-RX (console.cs:48787-48818,
    // UpdateMinimumNotchWidthRX [v2.10.3.15]). NereusSDR's notch list is
    // global, so the readout follows the active slice's channel and falls
    // back to the first pooled channel before any slice exists.
    SliceModel* slice = selectedSlice();
    if (!slice && receiverSelection() && receiverSelection()->restrictsReceiver()) {
        m_minWidthLbl->setText(QStringLiteral("--"));
        return;
    }
    const int sliceIndex = slice ? slice->sliceIndex()
                               : WdspEngine::kFirstSliceChannelId;
    RxChannel* ch = rm->rxChannelForSlice(sliceIndex);
    if (!ch) {
        m_minWidthLbl->setText(QStringLiteral("--"));
        return;
    }

    m_minWidthConn = connect(ch, &RxChannel::minNotchWidthChanged, this,
                             [this](double minWidthHz) {
        if (!m_minWidthLbl) { return; }
        m_minWidthLbl->setText(QStringLiteral("%1 Hz").arg(minWidthHz, 0, 'f', 1));
    });

    m_minWidthLbl->setText(
        QStringLiteral("%1 Hz").arg(ch->minNotchWidthHz(), 0, 'f', 1));
}

// ── MnfSetupPage::showEvent ───────────────────────────────────────────────────

void MnfSetupPage::showEvent(QShowEvent* event)
{
    SetupPage::showEvent(event);
    // The channel this readout follows may not have existed when the page was
    // built (WDSP initialises asynchronously, and the slice pool opens on
    // connect), so re-resolve on every visit as well as on the signal.
    refreshMinNotchWidth();
    rebuildTable();
}

// ══════════════════════════════════════════════════════════════════════════════
// NrAnfSetupPage::selectSubtab
// ══════════════════════════════════════════════════════════════════════════════
// Used by MainWindow's openNrSetupRequested handler to deep-link the "More
// Settings…" popup button into the correct sub-tab. Tab order mirrors the
// constructor's addTab calls.
//
void NrAnfSetupPage::selectSubtab(NrSlot slot, int openerSliceId)
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
        case NrSlot::NNR:
            target = QStringLiteral("NNR");
            if (m_nnrControls && model()) {
                SliceModel* opener = selectedSlice();
                if ((!receiverSelection() || !receiverSelection()->restrictsReceiver())
                    && openerSliceId >= 0) {
                    opener = model()->sliceById(openerSliceId);
                }
                m_nnrControls->bindSlice(opener);
            }
            break;
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
