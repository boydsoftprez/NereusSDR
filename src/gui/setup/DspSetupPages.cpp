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
#include "core/wdsp_api.h"
#include "models/RadioModel.h"
#include "core/WdspEngine.h"
#include "models/SliceModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
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

    // ── ALC ──────────────────────────────────────────────────────────────────
    QGroupBox* alcGrp = addSection("ALC");
    QVBoxLayout* alcLay = qobject_cast<QVBoxLayout*>(alcGrp->layout());

    auto* alcDecay = new QSpinBox;
    alcDecay->setRange(1, 5000);
    alcDecay->setSuffix(" ms");
    addLabeledSpinner(alcLay, "Decay", alcDecay);

    auto* alcMaxGain = new QSpinBox;
    alcMaxGain->setRange(0, 120);
    alcMaxGain->setSuffix(" dB");
    addLabeledSpinner(alcLay, "Max Gain", alcMaxGain);

    disableGroup(alcGrp);

    // ── Leveler ───────────────────────────────────────────────────────────────
    QGroupBox* levGrp = addSection("Leveler");
    QVBoxLayout* levLay = qobject_cast<QVBoxLayout*>(levGrp->layout());

    auto* levEnable = new QPushButton("Enable");
    addLabeledToggle(levLay, "Enable", levEnable);

    auto* levThresh = new QSpinBox;
    levThresh->setRange(-20, 20);
    levThresh->setSuffix(" dB");
    addLabeledSpinner(levLay, "Threshold", levThresh);

    auto* levDecay = new QSpinBox;
    levDecay->setRange(1, 5000);
    levDecay->setSuffix(" ms");
    addLabeledSpinner(levLay, "Decay", levDecay);

    disableGroup(levGrp);
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
// From Thetis setup.cs — tabDSP / tabPageNoise controls:
//   comboNRAlgorithm, udNRTaps, udNRDelay, tbNRGain, tbNRLeakage,
//   comboNRPosition, chkANFEnable, udANFTaps, udANFDelay, tbANFGain,
//   tbANFLeakage, comboANFPosition, comboEMNRGainMethod, comboEMNRNPEMethod
//
NrAnfSetupPage::NrAnfSetupPage(RadioModel* model, QWidget* parent)
    : SetupPage("NR/ANF", model, parent)
{
    // ── Noise Reduction ───────────────────────────────────────────────────────
    QGroupBox* nrGrp = addSection("Noise Reduction");
    QVBoxLayout* nrLay = qobject_cast<QVBoxLayout*>(nrGrp->layout());

    auto* nrAlgo = new QComboBox;
    nrAlgo->addItems({"LMS", "EMNR", "RNN", "SpecBleach"});
    addLabeledCombo(nrLay, "Algorithm", nrAlgo);

    auto* nrTaps = new QSpinBox;
    nrTaps->setRange(16, 1024);
    addLabeledSpinner(nrLay, "Taps", nrTaps);

    auto* nrDelay = new QSpinBox;
    nrDelay->setRange(1, 256);
    addLabeledSpinner(nrLay, "Delay", nrDelay);

    auto* nrGain = new QSlider(Qt::Horizontal);
    nrGain->setRange(0, 1000);
    addLabeledSlider(nrLay, "Gain", nrGain);

    auto* nrLeakage = new QSlider(Qt::Horizontal);
    nrLeakage->setRange(0, 1000);
    addLabeledSlider(nrLay, "Leakage", nrLeakage);

    auto* nrPos = new QComboBox;
    nrPos->addItems({"Pre-AGC", "Post-AGC"});
    addLabeledCombo(nrLay, "Position", nrPos);

    disableGroup(nrGrp);

    // ── ANF ───────────────────────────────────────────────────────────────────
    QGroupBox* anfGrp = addSection("ANF");
    QVBoxLayout* anfLay = qobject_cast<QVBoxLayout*>(anfGrp->layout());

    auto* anfEnable = new QPushButton("Enable");
    addLabeledToggle(anfLay, "Enable", anfEnable);

    auto* anfTaps = new QSpinBox;
    anfTaps->setRange(16, 1024);
    addLabeledSpinner(anfLay, "Taps", anfTaps);

    auto* anfDelay = new QSpinBox;
    anfDelay->setRange(1, 256);
    addLabeledSpinner(anfLay, "Delay", anfDelay);

    auto* anfGain = new QSlider(Qt::Horizontal);
    anfGain->setRange(0, 1000);
    addLabeledSlider(anfLay, "Gain", anfGain);

    auto* anfLeakage = new QSlider(Qt::Horizontal);
    anfLeakage->setRange(0, 1000);
    addLabeledSlider(anfLay, "Leakage", anfLeakage);

    auto* anfPos = new QComboBox;
    anfPos->addItems({"Pre-AGC", "Post-AGC"});
    addLabeledCombo(anfLay, "Position", anfPos);

    disableGroup(anfGrp);

    // ── EMNR ─────────────────────────────────────────────────────────────────
    QGroupBox* emnrGrp = addSection("EMNR");
    QVBoxLayout* emnrLay = qobject_cast<QVBoxLayout*>(emnrGrp->layout());

    auto* emnrGainMethod = new QComboBox;
    emnrGainMethod->addItems({"0", "1", "2"});
    addLabeledCombo(emnrLay, "Gain Method", emnrGainMethod);

    auto* emnrNpeMethod = new QComboBox;
    emnrNpeMethod->addItems({"0", "1", "2"});
    addLabeledCombo(emnrLay, "NPE Method", emnrNpeMethod);

    disableGroup(emnrGrp);
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

    auto* sidetoneVol = new QSlider(Qt::Horizontal);
    sidetoneVol->setRange(0, 100);
    addLabeledSlider(timingLay, "Sidetone Volume", sidetoneVol);

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
// VoxDexpSetupPage
// ══════════════════════════════════════════════════════════════════════════════
//
// From Thetis setup.cs — tabDSP / tabPageVOX controls:
//   tbVOXThreshold, tbVOXGain, tbVOXDelay, tbAntiVOXGain,
//   tbDEXPThreshold, tbDEXPAttack, tbDEXPRelease
//
VoxDexpSetupPage::VoxDexpSetupPage(RadioModel* model, QWidget* parent)
    : SetupPage("VOX/DEXP", model, parent)
{
    // ── VOX ───────────────────────────────────────────────────────────────────
    QGroupBox* voxGrp = addSection("VOX");
    QVBoxLayout* voxLay = qobject_cast<QVBoxLayout*>(voxGrp->layout());

    auto* voxThresh = new QSlider(Qt::Horizontal);
    voxThresh->setRange(0, 100);
    addLabeledSlider(voxLay, "Threshold", voxThresh);

    auto* voxGain = new QSlider(Qt::Horizontal);
    voxGain->setRange(0, 100);
    addLabeledSlider(voxLay, "Gain", voxGain);

    auto* voxDelay = new QSlider(Qt::Horizontal);
    voxDelay->setRange(0, 2000);
    addLabeledSlider(voxLay, "Delay (ms)", voxDelay);

    auto* antiVoxGain = new QSlider(Qt::Horizontal);
    antiVoxGain->setRange(0, 100);
    addLabeledSlider(voxLay, "Anti-VOX Gain", antiVoxGain);

    disableGroup(voxGrp);

    // ── DEXP ──────────────────────────────────────────────────────────────────
    QGroupBox* dexpGrp = addSection("DEXP");
    QVBoxLayout* dexpLay = qobject_cast<QVBoxLayout*>(dexpGrp->layout());

    auto* dexpThresh = new QSlider(Qt::Horizontal);
    dexpThresh->setRange(0, 100);
    addLabeledSlider(dexpLay, "Threshold", dexpThresh);

    auto* dexpAttack = new QSlider(Qt::Horizontal);
    dexpAttack->setRange(0, 2000);
    addLabeledSlider(dexpLay, "Attack (ms)", dexpAttack);

    auto* dexpRelease = new QSlider(Qt::Horizontal);
    dexpRelease->setRange(0, 2000);
    addLabeledSlider(dexpLay, "Release (ms)", dexpRelease);

    disableGroup(dexpGrp);
}

// ══════════════════════════════════════════════════════════════════════════════
// CfcSetupPage
// ══════════════════════════════════════════════════════════════════════════════
//
// From Thetis setup.cs — tabDSP / tabPageCFC controls:
//   chkCFCEnable, comboCFCPosition, chkCFCPreCompEQ, lblCFCProfile (display)
//
CfcSetupPage::CfcSetupPage(RadioModel* model, QWidget* parent)
    : SetupPage("CFC", model, parent)
{
    // ── CFC ───────────────────────────────────────────────────────────────────
    QGroupBox* cfcGrp = addSection("CFC");
    QVBoxLayout* cfcLay = qobject_cast<QVBoxLayout*>(cfcGrp->layout());

    auto* cfcEnable = new QPushButton("Enable");
    addLabeledToggle(cfcLay, "Enable", cfcEnable);

    auto* cfcPos = new QComboBox;
    cfcPos->addItems({"Pre", "Post"});
    addLabeledCombo(cfcLay, "Position", cfcPos);

    auto* cfcPreComp = new QPushButton("Enable");
    addLabeledToggle(cfcLay, "Pre-Comp EQ", cfcPreComp);

    disableGroup(cfcGrp);

    // ── Profile ───────────────────────────────────────────────────────────────
    QGroupBox* profGrp = addSection("Profile");
    QVBoxLayout* profLay = qobject_cast<QVBoxLayout*>(profGrp->layout());

    auto* profileLabel = new QLabel("(no profile loaded)");
    addLabeledLabel(profLay, "Profile", profileLabel);

    disableGroup(profGrp);
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

} // namespace NereusSDR
