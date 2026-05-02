// =================================================================
// src/models/SliceModel.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
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
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

//=================================================================
// display.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley (W5WC)
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
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
// Transitions to directX and continual modifications Copyright (C) 2020-2025 Richard Samphire (MW0LGE)
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

#include "SliceModel.h"

#include "Band.h"
#include "core/AppSettings.h"
#include "core/accessories/AlexController.h"

#include <algorithm>

namespace NereusSDR {

SliceModel::SliceModel(QObject* parent)
    : QObject(parent)
{
}

SliceModel::SliceModel(int sliceId, QObject* parent)
    : QObject(parent)
    , m_sliceIndex(sliceId)
{
}

SliceModel::~SliceModel() = default;

// ---------------------------------------------------------------------------
// Frequency
// ---------------------------------------------------------------------------

void SliceModel::setFrequency(double freq)
{
    // 3G-10 S2.9: client-side lock guard. When locked, setFrequency is a
    // no-op — prevents accidental tuning. The hardware VFO is not changed.
    if (m_locked) { return; }
    if (!qFuzzyCompare(m_frequency, freq)) {
        m_frequency = freq;
        emit frequencyChanged(freq);
    }
}

// ---------------------------------------------------------------------------
// Demodulation mode
// ---------------------------------------------------------------------------

void SliceModel::setDspMode(DSPMode mode)
{
    bool modeChanged = (m_dspMode != mode);
    m_dspMode = mode;

    // Apply default filter for the new mode
    // From Thetis console.cs:5180-5575 — InitFilterPresets, F5 per mode
    auto [low, high] = defaultFilterForMode(mode);
    bool filterChanged = (m_filterLow != low || m_filterHigh != high);
    m_filterLow = low;
    m_filterHigh = high;

    if (modeChanged) {
        emit dspModeChanged(mode);
    }
    if (filterChanged) {
        emit this->filterChanged(m_filterLow, m_filterHigh);
    }
}

// ---------------------------------------------------------------------------
// Bandpass filter
// ---------------------------------------------------------------------------

void SliceModel::setFilterLow(int low)
{
    if (m_filterLow != low) {
        m_filterLow = low;
        emit filterChanged(m_filterLow, m_filterHigh);
    }
}

void SliceModel::setFilterHigh(int high)
{
    if (m_filterHigh != high) {
        m_filterHigh = high;
        emit filterChanged(m_filterLow, m_filterHigh);
    }
}

void SliceModel::setFilter(int low, int high)
{
    if (m_filterLow != low || m_filterHigh != high) {
        m_filterLow = low;
        m_filterHigh = high;
        emit filterChanged(m_filterLow, m_filterHigh);
    }
}

// ---------------------------------------------------------------------------
// AGC
// ---------------------------------------------------------------------------

void SliceModel::setAgcMode(AGCMode mode)
{
    if (m_agcMode != mode) {
        m_agcMode = mode;
        emit agcModeChanged(mode);
    }
}

// ---------------------------------------------------------------------------
// Tuning step
// ---------------------------------------------------------------------------

void SliceModel::setStepHz(int hz)
{
    if (m_stepHz != hz && hz > 0) {
        m_stepHz = hz;
        emit stepHzChanged(hz);
    }
}

// ---------------------------------------------------------------------------
// Gains
// ---------------------------------------------------------------------------

void SliceModel::setAfGain(int gain)
{
    gain = std::clamp(gain, 0, 100);
    if (m_afGain != gain) {
        m_afGain = gain;
        emit afGainChanged(gain);
    }
}

void SliceModel::setRfGain(int gain)
{
    gain = std::clamp(gain, 0, 100);
    if (m_rfGain != gain) {
        m_rfGain = gain;
        emit rfGainChanged(gain);
    }
}

// ---------------------------------------------------------------------------
// Antenna selection
// ---------------------------------------------------------------------------

void SliceModel::setRxAntenna(const QString& ant)
{
    if (m_rxAntenna != ant) {
        m_rxAntenna = ant;
        emit rxAntennaChanged(ant);
    }
}

void SliceModel::setTxAntenna(const QString& ant)
{
    if (m_txAntenna != ant) {
        m_txAntenna = ant;
        emit txAntennaChanged(ant);
    }
}

// Phase 3P-I-a T13 — reverse sync: AlexController write → slice cache refresh.
// Reads the current per-band RX and TX antenna values from AlexController
// and refreshes the slice's cached m_rxAntenna / m_txAntenna via the public
// setters so rxAntennaChanged / txAntennaChanged signals fire to VFO Flag
// and RxApplet. The write-back to AlexController via T12's RadioModel handler
// is idempotent — AlexController::setRxAnt/setTxAnt returns early (without
// emitting antennaChanged) when the stored value equals the new value
// (AlexController.cpp:95,107), so no signal loop occurs.
void SliceModel::refreshAntennasFromAlex(const AlexController& alex, Band band)
{
    const int rx = alex.rxAnt(band);   // 1..3
    const int tx = alex.txAnt(band);
    auto name = [](int n) {
        switch (n) {
            case 2:  return QStringLiteral("ANT2");
            case 3:  return QStringLiteral("ANT3");
            default: return QStringLiteral("ANT1");
        }
    };
    // Use the public setters so rxAntennaChanged / txAntennaChanged
    // signals fire — VFO Flag and RxApplet listen. The loop back to
    // AlexController via T12's handler is idempotent: AlexController's
    // setRxAnt/setTxAnt returns early on equal value, so no signal is
    // emitted.
    setRxAntenna(name(rx));
    setTxAntenna(name(tx));
}

// ---------------------------------------------------------------------------
// Slice state
// ---------------------------------------------------------------------------

void SliceModel::setActive(bool active)
{
    if (m_active != active) {
        m_active = active;
        emit activeChanged(active);
    }
}

void SliceModel::setTxSlice(bool tx)
{
    if (m_txSlice != tx) {
        m_txSlice = tx;
        emit txSliceChanged(tx);
    }
}

// ── Phase 3G-10 Stage 1 stubs (DSP state, Stage 2 wires to RxChannel) ──

void SliceModel::setLocked(bool v)
{
    if (m_locked != v) {
        m_locked = v;
        emit lockedChanged(v);
    }
}

void SliceModel::setMuted(bool v)
{
    if (m_muted != v) {
        m_muted = v;
        emit mutedChanged(v);
    }
}

void SliceModel::setAudioPan(double pan)
{
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_audioPan - pan)) {
        return;
    }
    m_audioPan = pan;
    emit audioPanChanged(pan);
}

void SliceModel::setSsqlEnabled(bool v)
{
    if (m_ssqlEnabled != v) {
        m_ssqlEnabled = v;
        emit ssqlEnabledChanged(v);
    }
}

void SliceModel::setSsqlThresh(double dB)
{
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_ssqlThresh - dB)) {
        return;
    }
    m_ssqlThresh = dB;
    emit ssqlThreshChanged(dB);
}

void SliceModel::setAmsqEnabled(bool v)
{
    if (m_amsqEnabled != v) {
        m_amsqEnabled = v;
        emit amsqEnabledChanged(v);
    }
}

void SliceModel::setAmsqThresh(double dB)
{
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_amsqThresh - dB)) {
        return;
    }
    m_amsqThresh = dB;
    emit amsqThreshChanged(dB);
}

void SliceModel::setFmsqEnabled(bool v)
{
    if (m_fmsqEnabled != v) {
        m_fmsqEnabled = v;
        emit fmsqEnabledChanged(v);
    }
}

void SliceModel::setFmsqThresh(double dB)
{
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_fmsqThresh - dB)) {
        return;
    }
    m_fmsqThresh = dB;
    emit fmsqThreshChanged(dB);
}

void SliceModel::setAgcThreshold(int dBu)
{
    if (m_agcThreshold != dBu) {
        m_agcThreshold = dBu;
        emit agcThresholdChanged(dBu);
    }
}

void SliceModel::setAgcHang(int ms)
{
    if (m_agcHang != ms) {
        m_agcHang = ms;
        emit agcHangChanged(ms);
    }
}

void SliceModel::setAgcSlope(int dB)
{
    if (m_agcSlope != dB) {
        m_agcSlope = dB;
        emit agcSlopeChanged(dB);
    }
}

void SliceModel::setAgcAttack(int ms)
{
    if (m_agcAttack != ms) {
        m_agcAttack = ms;
        emit agcAttackChanged(ms);
    }
}

void SliceModel::setAgcDecay(int ms)
{
    if (m_agcDecay != ms) {
        m_agcDecay = ms;
        emit agcDecayChanged(ms);
    }
}

void SliceModel::setAutoAgcEnabled(bool on)
{
    if (m_autoAgcEnabled != on) {
        m_autoAgcEnabled = on;
        emit autoAgcEnabledChanged(on);
    }
}

void SliceModel::setAutoAgcOffset(double dB)
{
    if (!qFuzzyCompare(m_autoAgcOffset, dB)) {
        m_autoAgcOffset = dB;
        emit autoAgcOffsetChanged(dB);
    }
}

void SliceModel::setAgcFixedGain(int dB)
{
    if (m_agcFixedGain != dB) {
        m_agcFixedGain = dB;
        emit agcFixedGainChanged(dB);
    }
}

void SliceModel::setAgcHangThreshold(int val)
{
    if (m_agcHangThreshold != val) {
        m_agcHangThreshold = val;
        emit agcHangThresholdChanged(val);
    }
}

void SliceModel::setAgcMaxGain(int dB)
{
    if (m_agcMaxGain != dB) {
        m_agcMaxGain = dB;
        emit agcMaxGainChanged(dB);
    }
}

void SliceModel::setRitEnabled(bool v)
{
    if (m_ritEnabled != v) {
        m_ritEnabled = v;
        emit ritEnabledChanged(v);
    }
}

void SliceModel::setRitHz(int hz)
{
    if (m_ritHz != hz) {
        m_ritHz = hz;
        emit ritHzChanged(hz);
    }
}

void SliceModel::setXitEnabled(bool v)
{
    if (m_xitEnabled != v) {
        m_xitEnabled = v;
        emit xitEnabledChanged(v);
    }
}

void SliceModel::setXitHz(int hz)
{
    if (m_xitHz != hz) {
        m_xitHz = hz;
        emit xitHzChanged(hz);
    }
}

void SliceModel::setNbMode(NereusSDR::NbMode v)
{
    if (v == m_nbMode) { return; }
    m_nbMode = v;
    emit nbModeChanged(v);
}

// setNbTuning / nbTuningChanged removed 2026-04-22 — per-slice NB tuning is
// not a Thetis concept. All NB tuning is global per DSPRX and lives inside
// NbFamily, seeded from Setup → DSP → NB/SNB. See SliceModel.h.

// --- NR setters (Sub-epic C-1) ---
// See Thetis console.cs:43297-43450 SelectNR() [v2.10.3.13].

void SliceModel::setActiveNr(NereusSDR::NrSlot slot)
{
    if (m_activeNr == slot) { return; }
    m_activeNr = slot;
    emit activeNrChanged(slot);
}

// NR1
void SliceModel::setNr1Taps(int v)
{
    if (m_nr1Taps == v) { return; }
    m_nr1Taps = v;
    emit nr1TapsChanged(v);
}
void SliceModel::setNr1Delay(int v)
{
    if (m_nr1Delay == v) { return; }
    m_nr1Delay = v;
    emit nr1DelayChanged(v);
}
void SliceModel::setNr1Gain(double v)
{
    if (qFuzzyCompare(m_nr1Gain, v)) { return; }
    m_nr1Gain = v;
    emit nr1GainChanged(v);
}
void SliceModel::setNr1Leakage(double v)
{
    if (qFuzzyCompare(m_nr1Leakage, v)) { return; }
    m_nr1Leakage = v;
    emit nr1LeakageChanged(v);
}
void SliceModel::setNr1Position(NereusSDR::NrPosition p)
{
    if (m_nr1Position == p) { return; }
    m_nr1Position = p;
    emit nr1PositionChanged(p);
}

// NR2
void SliceModel::setNr2GainMethod(NereusSDR::EmnrGainMethod v)
{
    if (m_nr2GainMethod == v) { return; }
    m_nr2GainMethod = v;
    emit nr2GainMethodChanged(v);
}
void SliceModel::setNr2NpeMethod(NereusSDR::EmnrNpeMethod v)
{
    if (m_nr2NpeMethod == v) { return; }
    m_nr2NpeMethod = v;
    emit nr2NpeMethodChanged(v);
}
void SliceModel::setNr2TrainT1(double v)
{
    if (qFuzzyCompare(m_nr2TrainT1, v)) { return; }
    m_nr2TrainT1 = v;
    emit nr2TrainT1Changed(v);
}
void SliceModel::setNr2TrainT2(double v)
{
    if (qFuzzyCompare(m_nr2TrainT2, v)) { return; }
    m_nr2TrainT2 = v;
    emit nr2TrainT2Changed(v);
}
void SliceModel::setNr2AeFilter(bool v)
{
    if (m_nr2AeFilter == v) { return; }
    m_nr2AeFilter = v;
    emit nr2AeFilterChanged(v);
}
void SliceModel::setNr2Position(NereusSDR::NrPosition p)
{
    if (m_nr2Position == p) { return; }
    m_nr2Position = p;
    emit nr2PositionChanged(p);
}
void SliceModel::setNr2Post2Run(bool v)
{
    if (m_nr2Post2Run == v) { return; }
    m_nr2Post2Run = v;
    emit nr2Post2RunChanged(v);
}
void SliceModel::setNr2Post2Level(double v)
{
    if (qFuzzyCompare(m_nr2Post2Level, v)) { return; }
    m_nr2Post2Level = v;
    emit nr2Post2LevelChanged(v);
}
void SliceModel::setNr2Post2Factor(double v)
{
    if (qFuzzyCompare(m_nr2Post2Factor, v)) { return; }
    m_nr2Post2Factor = v;
    emit nr2Post2FactorChanged(v);
}
void SliceModel::setNr2Post2Rate(double v)
{
    if (qFuzzyCompare(m_nr2Post2Rate, v)) { return; }
    m_nr2Post2Rate = v;
    emit nr2Post2RateChanged(v);
}
void SliceModel::setNr2Post2Taper(int v)
{
    if (m_nr2Post2Taper == v) { return; }
    m_nr2Post2Taper = v;
    emit nr2Post2TaperChanged(v);
}

// NR3
void SliceModel::setNr3Position(NereusSDR::NrPosition p)
{
    if (m_nr3Position == p) { return; }
    m_nr3Position = p;
    emit nr3PositionChanged(p);
}
void SliceModel::setNr3UseDefaultGain(bool v)
{
    if (m_nr3UseDefaultGain == v) { return; }
    m_nr3UseDefaultGain = v;
    emit nr3UseDefaultGainChanged(v);
}

// NR4
void SliceModel::setNr4Reduction(double v)
{
    if (qFuzzyCompare(m_nr4Reduction, v)) { return; }
    m_nr4Reduction = v;
    emit nr4ReductionChanged(v);
}
void SliceModel::setNr4Smoothing(double v)
{
    if (qFuzzyCompare(m_nr4Smoothing, v)) { return; }
    m_nr4Smoothing = v;
    emit nr4SmoothingChanged(v);
}
void SliceModel::setNr4Whitening(double v)
{
    if (qFuzzyCompare(m_nr4Whitening, v)) { return; }
    m_nr4Whitening = v;
    emit nr4WhiteningChanged(v);
}
void SliceModel::setNr4Rescale(double v)
{
    if (qFuzzyCompare(m_nr4Rescale, v)) { return; }
    m_nr4Rescale = v;
    emit nr4RescaleChanged(v);
}
void SliceModel::setNr4PostThresh(double v)
{
    if (qFuzzyCompare(m_nr4PostThresh, v)) { return; }
    m_nr4PostThresh = v;
    emit nr4PostThreshChanged(v);
}
void SliceModel::setNr4Algo(NereusSDR::SbnrAlgo v)
{
    if (m_nr4Algo == v) { return; }
    m_nr4Algo = v;
    emit nr4AlgoChanged(v);
}

// DFNR
void SliceModel::setDfnrAttenLimit(double v)
{
    if (qFuzzyCompare(m_dfnrAttenLimit, v)) { return; }
    m_dfnrAttenLimit = v;
    emit dfnrAttenLimitChanged(v);
}
void SliceModel::setDfnrPostFilterBeta(double v)
{
    if (qFuzzyCompare(m_dfnrPostFilterBeta, v)) { return; }
    m_dfnrPostFilterBeta = v;
    emit dfnrPostFilterBetaChanged(v);
}

// BNR + MNR
void SliceModel::setBnrStrength(double v)
{
    if (qFuzzyCompare(m_bnrStrength, v)) { return; }
    m_bnrStrength = v;
    emit bnrStrengthChanged(v);
}
void SliceModel::setMnrStrength(double v)
{
    if (qFuzzyCompare(m_mnrStrength, v)) { return; }
    m_mnrStrength = v;
    emit mnrStrengthChanged(v);
}
void SliceModel::setMnrOversub(double v)
{
    if (qFuzzyCompare(m_mnrOversub, v)) { return; }
    m_mnrOversub = v;
    emit mnrOversubChanged(v);
}
void SliceModel::setMnrFloor(double v)
{
    if (qFuzzyCompare(m_mnrFloor, v)) { return; }
    m_mnrFloor = v;
    emit mnrFloorChanged(v);
}
void SliceModel::setMnrAlpha(double v)
{
    if (qFuzzyCompare(m_mnrAlpha, v)) { return; }
    m_mnrAlpha = v;
    emit mnrAlphaChanged(v);
}
void SliceModel::setMnrBias(double v)
{
    if (qFuzzyCompare(m_mnrBias, v)) { return; }
    m_mnrBias = v;
    emit mnrBiasChanged(v);
}
void SliceModel::setMnrGsmooth(double v)
{
    if (qFuzzyCompare(m_mnrGsmooth, v)) { return; }
    m_mnrGsmooth = v;
    emit mnrGsmoothChanged(v);
}

void SliceModel::setSnbEnabled(bool v)
{
    if (m_snbEnabled != v) {
        m_snbEnabled = v;
        emit snbEnabledChanged(v);
    }
}

void SliceModel::setApfEnabled(bool v)
{
    if (m_apfEnabled != v) {
        m_apfEnabled = v;
        emit apfEnabledChanged(v);
    }
}

void SliceModel::setApfTuneHz(int hz)
{
    if (m_apfTuneHz != hz) {
        m_apfTuneHz = hz;
        emit apfTuneHzChanged(hz);
    }
}

void SliceModel::setBinauralEnabled(bool v)
{
    if (m_binauralEnabled != v) {
        m_binauralEnabled = v;
        emit binauralEnabledChanged(v);
    }
}

void SliceModel::setFmCtcssMode(int mode)
{
    if (m_fmCtcssMode != mode) {
        m_fmCtcssMode = mode;
        emit fmCtcssModeChanged(mode);
    }
}

void SliceModel::setFmCtcssValueHz(double hz)
{
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_fmCtcssValueHz - hz)) {
        return;
    }
    m_fmCtcssValueHz = hz;
    emit fmCtcssValueHzChanged(hz);
}

void SliceModel::setFmOffsetHz(int hz)
{
    if (m_fmOffsetHz != hz) {
        m_fmOffsetHz = hz;
        emit fmOffsetHzChanged(hz);
    }
}

void SliceModel::setFmTxMode(FmTxMode mode)
{
    if (m_fmTxMode == mode) { return; }
    m_fmTxMode = mode;
    emit fmTxModeChanged(mode);
}

void SliceModel::setFmReverse(bool v)
{
    if (m_fmReverse != v) {
        m_fmReverse = v;
        emit fmReverseChanged(v);
    }
}

void SliceModel::setDiglOffsetHz(int hz)
{
    if (m_diglOffsetHz == hz) { return; }
    m_diglOffsetHz = hz;
    emit diglOffsetHzChanged(hz);
}

void SliceModel::setDiguOffsetHz(int hz)
{
    if (m_diguOffsetHz == hz) { return; }
    m_diguOffsetHz = hz;
    emit diguOffsetHzChanged(hz);
}

void SliceModel::setRttyMarkHz(int hz)
{
    if (m_rttyMarkHz != hz) {
        m_rttyMarkHz = hz;
        emit rttyMarkHzChanged(hz);
    }
}

void SliceModel::setRttyShiftHz(int hz)
{
    if (m_rttyShiftHz != hz) {
        m_rttyShiftHz = hz;
        emit rttyShiftHzChanged(hz);
    }
}

// ---------------------------------------------------------------------------
// Per-mode default filter presets
// ---------------------------------------------------------------------------

// Porting from Thetis console.cs:5180-5575 — InitFilterPresets, F5 per mode.
//
// Filter low/high are in Hz relative to the carrier frequency.
// LSB: negative offsets (passband below carrier)
// USB: positive offsets (passband above carrier)
// AM/SAM/DSB: symmetric around carrier
// CW: centered on cw_pitch (600 Hz from Thetis display.cs:1023)
// DIGU: centered on digu_click_tune_offset (1500 Hz from Thetis console.cs:14636)
// DIGL: centered on -digl_click_tune_offset (-2210 Hz from Thetis console.cs:14671)
std::pair<int, int> SliceModel::defaultFilterForMode(DSPMode mode)
{
    // From Thetis display.cs:1023
    static constexpr int kCwPitch = 600;
    // From Thetis console.cs:14636
    static constexpr int kDiguOffset = 1500;
    // From Thetis console.cs:14671
    // Upstream inline attribution preserved verbatim:
    //   :14669  //reset preset filter's center frequency - W4TME
    static constexpr int kDiglOffset = 2210;

    switch (mode) {
    case DSPMode::LSB:
        // From Thetis console.cs:5207 — F5: -3000 to -100
        return {-3000, -100};
    case DSPMode::USB:
        // From Thetis console.cs:5249 — F5: 100 to 3000
        return {100, 3000};
    case DSPMode::DSB:
        // From Thetis console.cs:5543 — F5: -3300 to 3300
        return {-3300, 3300};
    case DSPMode::CWL:
        // From Thetis console.cs:5375 — F5: -(cw_pitch+200) to -(cw_pitch-200)
        return {-(kCwPitch + 200), -(kCwPitch - 200)};
    case DSPMode::CWU:
        // From Thetis console.cs:5417 — F5: (cw_pitch-200) to (cw_pitch+200)
        return {kCwPitch - 200, kCwPitch + 200};
    case DSPMode::FM:
        // FM filters are dynamic in Thetis (from deviation + high cut).
        // Default deviation=5000, so use ±8000 as reasonable default.
        // From Thetis console.cs:7559-7565
        // Upstream inline attribution preserved verbatim (console.cs:7560):
        //   int halfBw = (int)(radio.GetDSPRX(0, 0).RXFMDeviation + radio.GetDSPRX(0, 0).RXFMHighCut);  //[2.10.3.4]MW0LGE
        return {-8000, 8000};
    case DSPMode::AM:
        // From Thetis console.cs:5459 — F5: -5000 to 5000
        return {-5000, 5000};
    case DSPMode::DIGU:
        // From Thetis console.cs:5333 — F5: (offset-500) to (offset+500)
        return {kDiguOffset - 500, kDiguOffset + 500};
    case DSPMode::SPEC:
        // SPEC mode: passthrough, wide filter
        return {-5000, 5000};
    case DSPMode::DIGL:
        // From Thetis console.cs:5291 — F5: -(offset+500) to -(offset-500)
        return {-(kDiglOffset + 500), -(kDiglOffset - 500)};
    case DSPMode::SAM:
        // From Thetis console.cs:5501 — F5: -5000 to 5000
        return {-5000, 5000};
    case DSPMode::DRM:
        // DRM: wide filter similar to AM
        return {-5000, 5000};
    }
    // Fallback
    return {100, 3000};
}

// ---------------------------------------------------------------------------
// Mode name utilities
// ---------------------------------------------------------------------------

QString SliceModel::modeName(DSPMode mode)
{
    switch (mode) {
    case DSPMode::LSB:  return QStringLiteral("LSB");
    case DSPMode::USB:  return QStringLiteral("USB");
    case DSPMode::DSB:  return QStringLiteral("DSB");
    case DSPMode::CWL:  return QStringLiteral("CWL");
    case DSPMode::CWU:  return QStringLiteral("CWU");
    case DSPMode::FM:   return QStringLiteral("FM");
    case DSPMode::AM:   return QStringLiteral("AM");
    case DSPMode::DIGU: return QStringLiteral("DIGU");
    case DSPMode::SPEC: return QStringLiteral("SPEC");
    case DSPMode::DIGL: return QStringLiteral("DIGL");
    case DSPMode::SAM:  return QStringLiteral("SAM");
    case DSPMode::DRM:  return QStringLiteral("DRM");
    }
    return QStringLiteral("USB");
}

DSPMode SliceModel::modeFromName(const QString& name)
{
    if (name == QLatin1String("LSB"))  return DSPMode::LSB;
    if (name == QLatin1String("USB"))  return DSPMode::USB;
    if (name == QLatin1String("DSB"))  return DSPMode::DSB;
    if (name == QLatin1String("CWL"))  return DSPMode::CWL;
    if (name == QLatin1String("CWU"))  return DSPMode::CWU;
    if (name == QLatin1String("FM"))   return DSPMode::FM;
    if (name == QLatin1String("AM"))   return DSPMode::AM;
    if (name == QLatin1String("DIGU")) return DSPMode::DIGU;
    if (name == QLatin1String("SPEC")) return DSPMode::SPEC;
    if (name == QLatin1String("DIGL")) return DSPMode::DIGL;
    if (name == QLatin1String("SAM"))  return DSPMode::SAM;
    if (name == QLatin1String("DRM"))  return DSPMode::DRM;
    return DSPMode::USB;
}

// ---------------------------------------------------------------------------
// Per-slice-per-band persistence (Phase 3G-10 Stage 2 — S2.P)
// ---------------------------------------------------------------------------
//
// Key layout:
//   Per-band DSP: Slice<N>/Band<key>/<Field>   (varies by band)
//   Session state: Slice<N>/<Field>             (band-agnostic)
//
// <N> comes from m_sliceIndex. <key> comes from bandKeyName(band).

namespace {

// Build the per-band prefix string, e.g. "Slice0/Band20m/".
QString bandPrefix(int sliceIndex, Band band)
{
    return QStringLiteral("Slice%1/Band%2/")
               .arg(sliceIndex)
               .arg(bandKeyName(band));
}

// Build the session-state prefix string, e.g. "Slice0/".
QString slicePrefix(int sliceIndex)
{
    return QStringLiteral("Slice%1/").arg(sliceIndex);
}

// Boolean → AppSettings canonical string.
QString boolStr(bool v) { return v ? QStringLiteral("True") : QStringLiteral("False"); }

} // namespace

// DspMode is the sentinel: if present under the per-band namespace,
// the band is treated as visited. Alternatives (Frequency, FilterLow)
// are written by migrateLegacyKeys() even when the upstream VfoDspMode
// key was absent, so they'd report "visited" for a band the user has
// never intentionally configured on the mode side. Using DspMode matches
// the semantic we want for the #118 band-click handler: "has this band
// been configured, not just visited."
bool SliceModel::hasSettingsFor(Band band) const
{
    auto& s = AppSettings::instance();
    return s.contains(bandPrefix(m_sliceIndex, band) + QStringLiteral("DspMode"));
}

void SliceModel::saveToSettings(Band band)
{
    auto& s = AppSettings::instance();
    const QString bp = bandPrefix(m_sliceIndex, band);
    const QString sp = slicePrefix(m_sliceIndex);

    // ── Per-band DSP state ────────────────────────────────────────────────────
    s.setValue(bp + QStringLiteral("Frequency"),    m_frequency);
    s.setValue(bp + QStringLiteral("AgcThreshold"), m_agcThreshold);
    s.setValue(bp + QStringLiteral("AgcHang"),      m_agcHang);
    s.setValue(bp + QStringLiteral("AgcSlope"),     m_agcSlope);
    s.setValue(bp + QStringLiteral("AgcAttack"),    m_agcAttack);
    s.setValue(bp + QStringLiteral("AgcDecay"),     m_agcDecay);
    s.setValue(bp + QStringLiteral("AgcAutoEnabled"), m_autoAgcEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(bp + QStringLiteral("AgcAutoOffset"), m_autoAgcOffset);
    s.setValue(bp + QStringLiteral("AgcFixedGain"), m_agcFixedGain);
    s.setValue(bp + QStringLiteral("AgcHangThreshold"), m_agcHangThreshold);
    s.setValue(bp + QStringLiteral("AgcMaxGain"),   m_agcMaxGain);
    s.setValue(bp + QStringLiteral("FilterLow"),    m_filterLow);
    s.setValue(bp + QStringLiteral("FilterHigh"),   m_filterHigh);
    s.setValue(bp + QStringLiteral("DspMode"),      static_cast<int>(m_dspMode));
    s.setValue(bp + QStringLiteral("AgcMode"),      static_cast<int>(m_agcMode));
    s.setValue(bp + QStringLiteral("StepHz"),       m_stepHz);

    // Noise-blanker mode (per-band). Tri-state Off/NB/NB2 mirrors Thetis
    // chkNB state per-receiver. NB TUNING (threshold / tau / lag / lead) is
    // NOT per-band in Thetis and lives globally inside NbFamily — see
    // SliceModel.h for the 2026-04-22 removal note.
    s.setValue(bp + QStringLiteral("NbMode"), static_cast<int>(m_nbMode));

    // ── Session state (band-agnostic) ─────────────────────────────────────────
    // NR active slot + tuning — session-level only, no per-band suffix.
    // Per user directive Q10: no band suffix on NR keys.
    s.setValue(sp + QStringLiteral("NrActive"),        static_cast<int>(m_activeNr));
    // NR1
    s.setValue(sp + QStringLiteral("Nr1Taps"),         m_nr1Taps);
    s.setValue(sp + QStringLiteral("Nr1Delay"),        m_nr1Delay);
    s.setValue(sp + QStringLiteral("Nr1Gain"),         m_nr1Gain);
    s.setValue(sp + QStringLiteral("Nr1Leakage"),      m_nr1Leakage);
    s.setValue(sp + QStringLiteral("Nr1Position"),     static_cast<int>(m_nr1Position));
    // NR2
    s.setValue(sp + QStringLiteral("Nr2GainMethod"),   static_cast<int>(m_nr2GainMethod));
    s.setValue(sp + QStringLiteral("Nr2NpeMethod"),    static_cast<int>(m_nr2NpeMethod));
    s.setValue(sp + QStringLiteral("Nr2TrainT1"),      m_nr2TrainT1);
    s.setValue(sp + QStringLiteral("Nr2TrainT2"),      m_nr2TrainT2);
    s.setValue(sp + QStringLiteral("Nr2AeFilter"),     boolStr(m_nr2AeFilter));
    s.setValue(sp + QStringLiteral("Nr2Position"),     static_cast<int>(m_nr2Position));
    s.setValue(sp + QStringLiteral("Nr2Post2Run"),     boolStr(m_nr2Post2Run));
    s.setValue(sp + QStringLiteral("Nr2Post2Level"),   m_nr2Post2Level);
    s.setValue(sp + QStringLiteral("Nr2Post2Factor"),  m_nr2Post2Factor);
    s.setValue(sp + QStringLiteral("Nr2Post2Rate"),    m_nr2Post2Rate);
    s.setValue(sp + QStringLiteral("Nr2Post2Taper"),   m_nr2Post2Taper);
    // NR3
    s.setValue(sp + QStringLiteral("Nr3Position"),     static_cast<int>(m_nr3Position));
    s.setValue(sp + QStringLiteral("Nr3UseDefaultGain"), boolStr(m_nr3UseDefaultGain));
    // NR4
    s.setValue(sp + QStringLiteral("Nr4Reduction"),    m_nr4Reduction);
    s.setValue(sp + QStringLiteral("Nr4Smoothing"),    m_nr4Smoothing);
    s.setValue(sp + QStringLiteral("Nr4Whitening"),    m_nr4Whitening);
    s.setValue(sp + QStringLiteral("Nr4Rescale"),      m_nr4Rescale);
    s.setValue(sp + QStringLiteral("Nr4PostThresh"),   m_nr4PostThresh);
    s.setValue(sp + QStringLiteral("Nr4Algo"),         static_cast<int>(m_nr4Algo));
    // DFNR
    s.setValue(sp + QStringLiteral("DfnrAttenLimit"),     m_dfnrAttenLimit);
    s.setValue(sp + QStringLiteral("DfnrPostFilterBeta"), m_dfnrPostFilterBeta);
    // BNR + MNR
    s.setValue(sp + QStringLiteral("BnrStrength"),     m_bnrStrength);
    s.setValue(sp + QStringLiteral("MnrStrength"),     m_mnrStrength);
    s.setValue(sp + QStringLiteral("MnrOversub"),      m_mnrOversub);
    s.setValue(sp + QStringLiteral("MnrFloor"),        m_mnrFloor);
    s.setValue(sp + QStringLiteral("MnrAlpha"),        m_mnrAlpha);
    s.setValue(sp + QStringLiteral("MnrBias"),         m_mnrBias);
    s.setValue(sp + QStringLiteral("MnrGsmooth"),      m_mnrGsmooth);

    s.setValue(sp + QStringLiteral("SnbEnabled"), boolStr(m_snbEnabled));
    s.setValue(sp + QStringLiteral("Locked"),     boolStr(m_locked));
    s.setValue(sp + QStringLiteral("Muted"),      boolStr(m_muted));
    s.setValue(sp + QStringLiteral("RitEnabled"), boolStr(m_ritEnabled));
    s.setValue(sp + QStringLiteral("RitHz"),      m_ritHz);
    s.setValue(sp + QStringLiteral("XitEnabled"), boolStr(m_xitEnabled));
    s.setValue(sp + QStringLiteral("XitHz"),      m_xitHz);
    s.setValue(sp + QStringLiteral("AfGain"),     m_afGain);
    s.setValue(sp + QStringLiteral("RfGain"),     m_rfGain);
    s.setValue(sp + QStringLiteral("RxAntenna"),  m_rxAntenna);
    s.setValue(sp + QStringLiteral("TxAntenna"),  m_txAntenna);

    // Track the most recently saved band so RadioModel::loadSliceState() on
    // the next launch can land on the user's actual last-used frequency,
    // not the panadapter's 14.225 MHz default. Per-band Frequency keys
    // already store the per-band freq; this just records "which band was
    // active last." Read via SliceModel::loadLastBandFromSettings().
    s.setValue(sp + QStringLiteral("LastBand"), bandKeyName(band));
}

void SliceModel::restoreFromSettings(Band band)
{
    auto& s = AppSettings::instance();
    const QString bp = bandPrefix(m_sliceIndex, band);
    const QString sp = slicePrefix(m_sliceIndex);

    // ── Per-band DSP state ────────────────────────────────────────────────────
    // Each key: if absent, leave the current SliceModel default unchanged.

    if (s.contains(bp + QStringLiteral("Frequency"))) {
        setFrequency(s.value(bp + QStringLiteral("Frequency")).toDouble());
    }
    if (s.contains(bp + QStringLiteral("AgcThreshold"))) {
        setAgcThreshold(s.value(bp + QStringLiteral("AgcThreshold")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcHang"))) {
        setAgcHang(s.value(bp + QStringLiteral("AgcHang")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcSlope"))) {
        setAgcSlope(s.value(bp + QStringLiteral("AgcSlope")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcAttack"))) {
        setAgcAttack(s.value(bp + QStringLiteral("AgcAttack")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcDecay"))) {
        setAgcDecay(s.value(bp + QStringLiteral("AgcDecay")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcAutoEnabled"))) {
        setAutoAgcEnabled(s.value(bp + QStringLiteral("AgcAutoEnabled")).toString() == QLatin1String("True"));
    }
    if (s.contains(bp + QStringLiteral("AgcAutoOffset"))) {
        setAutoAgcOffset(s.value(bp + QStringLiteral("AgcAutoOffset")).toDouble());
    }
    if (s.contains(bp + QStringLiteral("AgcFixedGain"))) {
        setAgcFixedGain(s.value(bp + QStringLiteral("AgcFixedGain")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcHangThreshold"))) {
        setAgcHangThreshold(s.value(bp + QStringLiteral("AgcHangThreshold")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcMaxGain"))) {
        setAgcMaxGain(s.value(bp + QStringLiteral("AgcMaxGain")).toInt());
    }
    if (s.contains(bp + QStringLiteral("DspMode"))) {
        // Set mode WITHOUT applying the default filter — filter follows below.
        // We must update m_dspMode before reading FilterLow/FilterHigh so
        // the final setFilter call is not superseded by setDspMode's default.
        DSPMode mode = static_cast<DSPMode>(
            s.value(bp + QStringLiteral("DspMode")).toInt());
        // Directly assign mode without calling setDspMode() (which also
        // resets the filter). Emit the signal manually to keep observers in sync.
        if (m_dspMode != mode) {
            m_dspMode = mode;
            emit dspModeChanged(mode);
        }
    }
    if (s.contains(bp + QStringLiteral("FilterLow")) &&
        s.contains(bp + QStringLiteral("FilterHigh"))) {
        setFilter(s.value(bp + QStringLiteral("FilterLow")).toInt(),
                  s.value(bp + QStringLiteral("FilterHigh")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcMode"))) {
        setAgcMode(static_cast<AGCMode>(
            s.value(bp + QStringLiteral("AgcMode")).toInt()));
    }
    if (s.contains(bp + QStringLiteral("StepHz"))) {
        setStepHz(s.value(bp + QStringLiteral("StepHz")).toInt());
    }

    // Noise blanker mode (per-band). Tuning keys (NbThreshold/NbTauMs/
    // NbLeadMs/NbLagMs) from an earlier pre-2026-04-22 schema are ignored
    // if present — they'll be overwritten on next save. Per-band NB tuning
    // is not a Thetis concept.
    if (s.contains(bp + QStringLiteral("NbMode"))) {
        setNbMode(static_cast<NereusSDR::NbMode>(
            s.value(bp + QStringLiteral("NbMode")).toInt()));
    }

    // ── Session state (band-agnostic) ─────────────────────────────────────────
    // NR active slot + tuning (no per-band suffix, per user directive Q10).
    if (s.contains(sp + QStringLiteral("NrActive"))) {
        setActiveNr(static_cast<NereusSDR::NrSlot>(s.value(sp + QStringLiteral("NrActive")).toInt()));
    }
    // NR1
    if (s.contains(sp + QStringLiteral("Nr1Taps"))) {
        setNr1Taps(s.value(sp + QStringLiteral("Nr1Taps")).toInt());
    }
    if (s.contains(sp + QStringLiteral("Nr1Delay"))) {
        setNr1Delay(s.value(sp + QStringLiteral("Nr1Delay")).toInt());
    }
    if (s.contains(sp + QStringLiteral("Nr1Gain"))) {
        setNr1Gain(s.value(sp + QStringLiteral("Nr1Gain")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr1Leakage"))) {
        setNr1Leakage(s.value(sp + QStringLiteral("Nr1Leakage")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr1Position"))) {
        setNr1Position(static_cast<NereusSDR::NrPosition>(s.value(sp + QStringLiteral("Nr1Position")).toInt()));
    }
    // NR2
    if (s.contains(sp + QStringLiteral("Nr2GainMethod"))) {
        setNr2GainMethod(static_cast<NereusSDR::EmnrGainMethod>(s.value(sp + QStringLiteral("Nr2GainMethod")).toInt()));
    }
    if (s.contains(sp + QStringLiteral("Nr2NpeMethod"))) {
        setNr2NpeMethod(static_cast<NereusSDR::EmnrNpeMethod>(s.value(sp + QStringLiteral("Nr2NpeMethod")).toInt()));
    }
    if (s.contains(sp + QStringLiteral("Nr2TrainT1"))) {
        setNr2TrainT1(s.value(sp + QStringLiteral("Nr2TrainT1")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr2TrainT2"))) {
        setNr2TrainT2(s.value(sp + QStringLiteral("Nr2TrainT2")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr2AeFilter"))) {
        setNr2AeFilter(s.value(sp + QStringLiteral("Nr2AeFilter")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("Nr2Position"))) {
        setNr2Position(static_cast<NereusSDR::NrPosition>(s.value(sp + QStringLiteral("Nr2Position")).toInt()));
    }
    if (s.contains(sp + QStringLiteral("Nr2Post2Run"))) {
        setNr2Post2Run(s.value(sp + QStringLiteral("Nr2Post2Run")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("Nr2Post2Level"))) {
        setNr2Post2Level(s.value(sp + QStringLiteral("Nr2Post2Level")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr2Post2Factor"))) {
        setNr2Post2Factor(s.value(sp + QStringLiteral("Nr2Post2Factor")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr2Post2Rate"))) {
        setNr2Post2Rate(s.value(sp + QStringLiteral("Nr2Post2Rate")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr2Post2Taper"))) {
        setNr2Post2Taper(s.value(sp + QStringLiteral("Nr2Post2Taper")).toInt());
    }
    // NR3
    if (s.contains(sp + QStringLiteral("Nr3Position"))) {
        setNr3Position(static_cast<NereusSDR::NrPosition>(s.value(sp + QStringLiteral("Nr3Position")).toInt()));
    }
    if (s.contains(sp + QStringLiteral("Nr3UseDefaultGain"))) {
        setNr3UseDefaultGain(s.value(sp + QStringLiteral("Nr3UseDefaultGain")).toString() == QLatin1String("True"));
    }
    // NR4
    if (s.contains(sp + QStringLiteral("Nr4Reduction"))) {
        setNr4Reduction(s.value(sp + QStringLiteral("Nr4Reduction")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr4Smoothing"))) {
        setNr4Smoothing(s.value(sp + QStringLiteral("Nr4Smoothing")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr4Whitening"))) {
        setNr4Whitening(s.value(sp + QStringLiteral("Nr4Whitening")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr4Rescale"))) {
        setNr4Rescale(s.value(sp + QStringLiteral("Nr4Rescale")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr4PostThresh"))) {
        setNr4PostThresh(s.value(sp + QStringLiteral("Nr4PostThresh")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr4Algo"))) {
        setNr4Algo(static_cast<NereusSDR::SbnrAlgo>(s.value(sp + QStringLiteral("Nr4Algo")).toInt()));
    }
    // DFNR
    if (s.contains(sp + QStringLiteral("DfnrAttenLimit"))) {
        setDfnrAttenLimit(s.value(sp + QStringLiteral("DfnrAttenLimit")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("DfnrPostFilterBeta"))) {
        setDfnrPostFilterBeta(s.value(sp + QStringLiteral("DfnrPostFilterBeta")).toDouble());
    }
    // BNR + MNR
    if (s.contains(sp + QStringLiteral("BnrStrength"))) {
        setBnrStrength(s.value(sp + QStringLiteral("BnrStrength")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("MnrStrength"))) {
        setMnrStrength(s.value(sp + QStringLiteral("MnrStrength")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("MnrOversub"))) {
        setMnrOversub(s.value(sp + QStringLiteral("MnrOversub")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("MnrFloor"))) {
        setMnrFloor(s.value(sp + QStringLiteral("MnrFloor")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("MnrAlpha"))) {
        setMnrAlpha(s.value(sp + QStringLiteral("MnrAlpha")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("MnrBias"))) {
        setMnrBias(s.value(sp + QStringLiteral("MnrBias")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("MnrGsmooth"))) {
        setMnrGsmooth(s.value(sp + QStringLiteral("MnrGsmooth")).toDouble());
    }

    if (s.contains(sp + QStringLiteral("SnbEnabled"))) {
        setSnbEnabled(s.value(sp + QStringLiteral("SnbEnabled")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("Locked"))) {
        setLocked(s.value(sp + QStringLiteral("Locked")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("Muted"))) {
        setMuted(s.value(sp + QStringLiteral("Muted")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("RitEnabled"))) {
        setRitEnabled(s.value(sp + QStringLiteral("RitEnabled")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("RitHz"))) {
        setRitHz(s.value(sp + QStringLiteral("RitHz")).toInt());
    }
    if (s.contains(sp + QStringLiteral("XitEnabled"))) {
        setXitEnabled(s.value(sp + QStringLiteral("XitEnabled")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("XitHz"))) {
        setXitHz(s.value(sp + QStringLiteral("XitHz")).toInt());
    }
    if (s.contains(sp + QStringLiteral("AfGain"))) {
        setAfGain(s.value(sp + QStringLiteral("AfGain")).toInt());
    }
    if (s.contains(sp + QStringLiteral("RfGain"))) {
        setRfGain(s.value(sp + QStringLiteral("RfGain")).toInt());
    }
    if (s.contains(sp + QStringLiteral("RxAntenna"))) {
        setRxAntenna(s.value(sp + QStringLiteral("RxAntenna")).toString());
    }
    if (s.contains(sp + QStringLiteral("TxAntenna"))) {
        setTxAntenna(s.value(sp + QStringLiteral("TxAntenna")).toString());
    }
}

// One-shot migration of the legacy flat key format (VfoFrequency, VfoDspMode,
// etc.) to the new per-slice-per-band namespace. Called once at startup before
// restoreFromSettings(). If the legacy key is absent the function is a no-op.
void SliceModel::migrateLegacyKeys()
{
    auto& s = AppSettings::instance();

    if (!s.contains(QStringLiteral("VfoFrequency"))) {
        return; // Nothing to migrate.
    }

    // Derive the band from the persisted frequency.
    double freq = s.value(QStringLiteral("VfoFrequency"), 14225000.0).toDouble();
    Band band = bandFromFrequency(freq);

    // Slice 0 — the only slice that can have legacy data.
    const QString bp = bandPrefix(0, band);
    const QString sp = slicePrefix(0);

    // Per-band DSP — migrate each key that exists.
    s.setValue(bp + QStringLiteral("Frequency"), freq);

    if (s.contains(QStringLiteral("VfoDspMode"))) {
        s.setValue(bp + QStringLiteral("DspMode"),
                   s.value(QStringLiteral("VfoDspMode")));
    }
    if (s.contains(QStringLiteral("VfoFilterLow"))) {
        s.setValue(bp + QStringLiteral("FilterLow"),
                   s.value(QStringLiteral("VfoFilterLow")));
    }
    if (s.contains(QStringLiteral("VfoFilterHigh"))) {
        s.setValue(bp + QStringLiteral("FilterHigh"),
                   s.value(QStringLiteral("VfoFilterHigh")));
    }
    if (s.contains(QStringLiteral("VfoAgcMode"))) {
        s.setValue(bp + QStringLiteral("AgcMode"),
                   s.value(QStringLiteral("VfoAgcMode")));
    }
    if (s.contains(QStringLiteral("VfoStepHz"))) {
        s.setValue(bp + QStringLiteral("StepHz"),
                   s.value(QStringLiteral("VfoStepHz")));
    }

    // Session state — migrate each key that exists.
    if (s.contains(QStringLiteral("VfoAfGain"))) {
        s.setValue(sp + QStringLiteral("AfGain"),
                   s.value(QStringLiteral("VfoAfGain")));
    }
    if (s.contains(QStringLiteral("VfoRfGain"))) {
        s.setValue(sp + QStringLiteral("RfGain"),
                   s.value(QStringLiteral("VfoRfGain")));
    }
    if (s.contains(QStringLiteral("VfoRxAntenna"))) {
        s.setValue(sp + QStringLiteral("RxAntenna"),
                   s.value(QStringLiteral("VfoRxAntenna")));
    }
    if (s.contains(QStringLiteral("VfoTxAntenna"))) {
        s.setValue(sp + QStringLiteral("TxAntenna"),
                   s.value(QStringLiteral("VfoTxAntenna")));
    }

    // Remove all legacy flat keys.
    s.remove(QStringLiteral("VfoFrequency"));
    s.remove(QStringLiteral("VfoDspMode"));
    s.remove(QStringLiteral("VfoFilterLow"));
    s.remove(QStringLiteral("VfoFilterHigh"));
    s.remove(QStringLiteral("VfoAgcMode"));
    s.remove(QStringLiteral("VfoStepHz"));
    s.remove(QStringLiteral("VfoAfGain"));
    s.remove(QStringLiteral("VfoRfGain"));
    s.remove(QStringLiteral("VfoRxAntenna"));
    s.remove(QStringLiteral("VfoTxAntenna"));
}

// Reads Slice<N>/LastBand and parses it back to a Band. Returns
// std::nullopt on missing key (fresh install / pre-LastBand settings)
// or unparseable values. Static so RadioModel can call this before
// constructing any slice.
std::optional<Band> SliceModel::loadLastBandFromSettings(int sliceIndex)
{
    auto& s = AppSettings::instance();
    const QString key = slicePrefix(sliceIndex) + QStringLiteral("LastBand");
    if (!s.contains(key)) {
        return std::nullopt;
    }
    const QString name = s.value(key).toString();
    if (name.isEmpty()) {
        return std::nullopt;
    }
    // bandFromName handles both label form ("20m") and short form ("20"),
    // plus GEN/WWV/XVTR. Falls back to Band::GEN on unknown input — guard
    // against that so a corrupted key doesn't silently land on GEN.
    const Band parsed = bandFromName(name);
    if (parsed == Band::GEN && name != QLatin1String("GEN")) {
        return std::nullopt;
    }
    return parsed;
}

// ---------------------------------------------------------------------------
// Phase 3O — VAX routing
// ---------------------------------------------------------------------------

void SliceModel::setVaxChannel(int ch)
{
    // Clamp to valid range.
    if (ch < 0 || ch > 4) { ch = 0; }

    const int prev = m_vaxChannel.exchange(ch, std::memory_order_acq_rel);
    if (prev == ch) { return; }

    AppSettings::instance().setValue(
        slicePrefix(m_sliceIndex) + QStringLiteral("VaxChannel"),
        QString::number(ch));

    emit vaxChannelChanged(ch);
}

void SliceModel::loadFromSettings()
{
    auto& s = AppSettings::instance();

    // ── VAX channel (Phase 3O) ────────────────────────────────────────────────
    int vaxCh = s.value(
        slicePrefix(m_sliceIndex) + QStringLiteral("VaxChannel"), "0")
        .toString().toInt();
    if (vaxCh < 0 || vaxCh > 4) { vaxCh = 0; }  // spec §5.1: invalid values clamp to 0
    if (vaxCh != m_vaxChannel.load(std::memory_order_acquire)) {
        m_vaxChannel.store(vaxCh, std::memory_order_release);
        emit vaxChannelChanged(vaxCh);
    }

}

} // namespace NereusSDR
