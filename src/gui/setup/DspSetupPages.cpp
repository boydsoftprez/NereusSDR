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
    // ── RX1 AGC ──────────────────────────────────────────────────────────────
    QGroupBox* agcGrp = addSection("RX1 AGC");
    QVBoxLayout* agcLay = qobject_cast<QVBoxLayout*>(agcGrp->layout());

    auto* agcMode = new QComboBox;
    agcMode->addItems({"Off", "Long", "Slow", "Med", "Fast", "Custom"});
    addLabeledCombo(agcLay, "Mode", agcMode);

    auto* agcAttack = new QSpinBox;
    agcAttack->setRange(1, 1000);
    agcAttack->setSuffix(" ms");
    addLabeledSpinner(agcLay, "Attack", agcAttack);

    auto* agcDecay = new QSpinBox;
    agcDecay->setRange(1, 5000);
    agcDecay->setSuffix(" ms");
    addLabeledSpinner(agcLay, "Decay", agcDecay);

    auto* agcHang = new QSpinBox;
    agcHang->setRange(0, 5000);
    agcHang->setSuffix(" ms");
    addLabeledSpinner(agcLay, "Hang", agcHang);

    auto* agcSlope = new QSlider(Qt::Horizontal);
    agcSlope->setRange(0, 10);
    addLabeledSlider(agcLay, "Slope", agcSlope);

    auto* agcMaxGain = new QSpinBox;
    agcMaxGain->setRange(0, 120);
    agcMaxGain->setSuffix(" dB");
    addLabeledSpinner(agcLay, "Max Gain", agcMaxGain);

    auto* agcHangThresh = new QSlider(Qt::Horizontal);
    agcHangThresh->setRange(0, 100);
    addLabeledSlider(agcLay, "Hang Threshold", agcHangThresh);

    disableGroup(agcGrp);

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
    // ── NB1 ───────────────────────────────────────────────────────────────────
    QGroupBox* nb1Grp = addSection("NB1");
    QVBoxLayout* nb1Lay = qobject_cast<QVBoxLayout*>(nb1Grp->layout());

    auto* nb1Thresh = new QSlider(Qt::Horizontal);
    nb1Thresh->setRange(0, 100);
    addLabeledSlider(nb1Lay, "Threshold", nb1Thresh);

    auto* nb1Mode = new QComboBox;
    nb1Mode->addItems({"Zero", "Gate", "Interpolate"});
    addLabeledCombo(nb1Lay, "Mode", nb1Mode);

    disableGroup(nb1Grp);

    // ── NB2 ───────────────────────────────────────────────────────────────────
    QGroupBox* nb2Grp = addSection("NB2");
    QVBoxLayout* nb2Lay = qobject_cast<QVBoxLayout*>(nb2Grp->layout());

    auto* nb2Thresh = new QSlider(Qt::Horizontal);
    nb2Thresh->setRange(0, 100);
    addLabeledSlider(nb2Lay, "Threshold", nb2Thresh);

    disableGroup(nb2Grp);

    // ── SNB ───────────────────────────────────────────────────────────────────
    QGroupBox* snbGrp = addSection("SNB");
    QVBoxLayout* snbLay = qobject_cast<QVBoxLayout*>(snbGrp->layout());

    auto* snbK1 = new QSlider(Qt::Horizontal);
    snbK1->setRange(0, 100);
    addLabeledSlider(snbLay, "K1", snbK1);

    auto* snbK2 = new QSlider(Qt::Horizontal);
    snbK2->setRange(0, 100);
    addLabeledSlider(snbLay, "K2", snbK2);

    auto* snbOutBw = new QSpinBox;
    snbOutBw->setRange(100, 96000);
    snbOutBw->setSuffix(" Hz");
    addLabeledSpinner(snbLay, "Output Bandwidth", snbOutBw);

    disableGroup(snbGrp);
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
