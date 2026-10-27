// =================================================================
// src/gui/meters/MeterItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/common.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Colours at their defaults follow the workspace look when
//                 drawn (ItemColourLook.h); saved colours are unchanged.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Final review fix wave (I10): a needle applies its saved
//                 attack ratio rising and decay ratio falling, and draws
//                 with its saved stroke width; one saved without them
//                 keeps 0.3 both ways and the 2 px pen. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - advanceFade: a stage-disabled item (setStageDisabled,
//                 Thetis clsMeterItem.Disabled) steps toward 48 whatever its
//                 fade options; never for a foreign session's entry.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - mmioVariableCount: HISTORY reads 2 (axis 0, then axis 1),
//                 as Thetis clsHistoryItem's per-axis MMIO does. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - mmioVariableCount: ROTATOR reads 2 (AZ, then ELE), as
//                 Thetis MeterVariables does. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Several data source (MMIO) variables per item
//                 (setMmioBinding(index, ...)) and mmioVariableCount(), the
//                 port of Thetis MeterVariables. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Fade on RX / Fade on TX on every item: setTransmitting,
//                 fade flags, fadeValue() and advanceFade() (Thetis fade(),
//                 via ContentFade::nextValue), setForeignSession; a virtual
//                 clearHistory() (Thetis clsMeterItem.ClearHistory).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - BAR building block: Segments and Solid styles ported from
//                 renderHBar, a peak hold marker separate from the live
//                 marker, marker stroke width, ignore history after
//                 clearHistory(), power limit normalisation for calibrated
//                 power bars, and the clsBarItem new-item defaults. Saved
//                 bars keep their look. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - NEEDLE building block: shadow and peak hold switches, a
//                 peak hold colour (peak needle on a calibrated face), the
//                 history arc ported from renderNeedle, ignore history after
//                 clearHistory(), the power limit (normalise to 100 W) and
//                 the clsNeedleItem new-item defaults. paint() and
//                 emitVertices() share one needle geometry, so the CPU
//                 needle follows a calibrated face too. Saved needles keep
//                 their look. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-25 - VHF S9 rule per slice: setAboveS9Frequency() /
//                 aboveS9Frequency(), a formatValue overload taking the
//                 flag (S9 = -93 dBm at or above 30 MHz, kS9DbmBelowS9Frequency
//                 / kS9DbmAboveS9Frequency from common.cs); BarItem, TextItem
//                 and NeedleItem S-unit text and calibration use it.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - Per-item update interval (acceptsUpdate); new items take
//                 kDefaultUpdateIntervalMs. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-26 - TEXT, SCALE and IMAGE building blocks: TextItem font
//                 family, italic, centring and the %value%, %valueWithType%
//                 (ReadingUnits) and %group% tokens (renderText); ImageItem
//                 dark mode and -dark/-small/-large variants (renderImage);
//                 clsText, clsScaleItem and clsImage new-item defaults.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - New items take Thetis defaults: applyNewItemDefaults()
//                 hook (no-op in the base), SolidColourItem override. Saved
//                 items keep their look. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-12 - Shared accessory display readings and independent legal
//                 marker / display scale by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-04 — Selected RX source identity and RX-only presentation reset by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Composite reading/replay/cadence contracts by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
//   2026-09-23: R-R3-13: no reading (isNoMeterReading) shows "--" in
//                 formatValue, TextItem, BarItem value/peak text and the
//                 NeedleItem readouts; bars and needles rest at the scale
//                 minimum and drop their peak. J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-09-23: R-R3-13 fix wave: no reading applies only to
//                 receive-signal bindings (isReceiveSignalBinding); a TX
//                 meter keeps WDSP's -400 zero-power floor as a number.
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-09-25 - R-R3-32 (remote-window parity Task 6): the hardware
//                 telemetry bindings treat the sentinel as no reading
//                 (isHardwareTelemetryBinding). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview data colours (JJ's decision B):
//                 NeedleItem's default background reads
//                 WorkspacePreview::dataBackground(), the role of the
//                 container around its meter or the user's colour for that
//                 container. Ink unchanged. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-26 - ImageItem: an empty image path clears the image without a
//                 file read. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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

// Modification history (NereusSDR):
//   2026-10-12 - Apply the approved workspace-only 11 px text minimum.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Keep overflowing regular scale endpoint glyphs within the
//                 meter canvas in both looks, preserving ticks and fonts.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-26 - User text in the record (image path, text label and suffix, needle source label) is joined, not substituted
//                 by chained QString::arg, so a "%1" or "%20" in it survives.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

// --- From common.cs ---
//=================================================================
// common.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2012  FlexRadio Systems
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
// You may contact us via email at: gpl@flexradio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    4616 W. Howard Lane  Suite 1-150
//    Austin, TX 78728
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

#include "core/ControlRanges.h"
#include "MeterItem.h"
#include "ItemColourLook.h"
#include "gui/styles/WorkspacePreview.h"
#include "MeterPoller.h"  // MeterBinding namespace
#include "AboveS9Frequency.h"
#include "gui/containers/ContentFade.h"

#include <QPainter>
#include <QMouseEvent>
#include <QFont>
#include <QFontMetrics>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <cmath>
#include <algorithm>
#include <QtMath>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// readingName — ported verbatim from Thetis MeterManager.cs:2258-2318
// ReadingName() switch. Used by ScaleItem::ShowType to label bar-row
// presets with their canonical Thetis names (e.g. "Signal Max FFT Bin").
// ---------------------------------------------------------------------------
QString readingName(int bindingId)
{
    switch (bindingId) {
        // RX
        case MeterBinding::SignalPeak:   return QStringLiteral("Signal Peak");
        case MeterBinding::SignalAvg:    return QStringLiteral("Signal Average");
        case MeterBinding::AdcPeak:      return QStringLiteral("ADC Peak");
        case MeterBinding::AdcAvg:       return QStringLiteral("ADC Average");
        case MeterBinding::AgcGain:      return QStringLiteral("AGC Gain");
        case MeterBinding::AgcPeak:      return QStringLiteral("AGC Peak");
        case MeterBinding::AgcAvg:       return QStringLiteral("AGC Average");
        case MeterBinding::SignalMaxBin: return QStringLiteral("Signal Max FFT Bin");
        case MeterBinding::PbSnr:        return QStringLiteral("Estimated PBSNR");
        // TX
        case MeterBinding::TxPower:        return QStringLiteral("Power");
        case MeterBinding::TxReversePower: return QStringLiteral("Reverse Power");
        case MeterBinding::TxSwr:          return QStringLiteral("SWR");
        case MeterBinding::TxMic:          return QStringLiteral("MIC");
        case MeterBinding::TxComp:         return QStringLiteral("Compression");
        case MeterBinding::TxAlc:          return QStringLiteral("ALC");
        case MeterBinding::TxEq:           return QStringLiteral("EQ");
        case MeterBinding::TxLeveler:      return QStringLiteral("Leveler");
        case MeterBinding::TxLevelerGain:  return QStringLiteral("Leveler Gain");
        // Thetis has no ReadingName for ALC_GAIN specifically; ALC_G is
        // "ALC Compression". NereusSDR splits these two signals into
        // separate bindings so we use the natural label for ALC Gain.
        case MeterBinding::TxAlcGain:      return QStringLiteral("ALC Gain");
        case MeterBinding::TxAlcGroup:     return QStringLiteral("ALC Group");
        case MeterBinding::TxCfc:          return QStringLiteral("CFC Compression Average");
        case MeterBinding::TxMicPeak:      return QStringLiteral("Mic Peak");
        case MeterBinding::TxAlcPeak:      return QStringLiteral("ALC Peak");
        case MeterBinding::TxCompPeak:     return QStringLiteral("COMP Peak");
        case MeterBinding::TxEqPeak:       return QStringLiteral("EQ Peak");
        case MeterBinding::TxLevelerPeak:  return QStringLiteral("Leveler Peak");
        case MeterBinding::TxCfcPeak:      return QStringLiteral("CFC Peak");
        case MeterBinding::TxCfcGain:      return QStringLiteral("CFC Compression");
        // Hardware
        case MeterBinding::HwVolts:       return QStringLiteral("Volts");
        case MeterBinding::HwAmps:        return QStringLiteral("Amps");
        case MeterBinding::HwTemperature: return QStringLiteral("Temperature");
        // Rotator
        case MeterBinding::RotatorAz:  return QStringLiteral("Azimuth");
        case MeterBinding::RotatorEle: return QStringLiteral("Elevation");
    }
    return QString();
}


bool isReceiveSignalBinding(int bindingId)
{
    return (bindingId >= MeterBinding::SignalPeak && bindingId <= MeterBinding::AgcAvg)
        || bindingId == MeterBinding::SignalMaxBin;
}

bool isSourceClampedPeakBinding(int bindingId)
{
    return bindingId >= MeterBinding::TxMicPeak && bindingId <= MeterBinding::TxCfcPeak;
}

bool isHardwareTelemetryBinding(int bindingId)
{
    return bindingId >= MeterBinding::HwVolts && bindingId <= MeterBinding::HwTemperature;
}

QString MeterItem::noReadingText(MeterUnit unit)
{
    return unit == MeterUnit::uV ? QStringLiteral("-- µV") : QStringLiteral("--");
}

// ---------------------------------------------------------------------------
// MeterItem::formatValue: Task 3.2 unit-mode fan-out helper
//
// Converts a raw signal-level dBm value to the user-selected display unit.
//
// S:   IARU S-meter scale: S9 = -73 dBm at HF, each S unit = 6 dB.
//      S0..S9 are shown as "S0".."S9".  Above S9: "S9+10", "S9+20" …
//      Clamps to [0..19] to stay finite (S9+60 = S19 internally).
//      Derived from Thetis console.cs radSReading / getMeterData dBm→S path
//      [v2.10.3.13].
// dBm: plain numeric; decimal flag controls .1 vs integer rounding.
// uV:  µV at 50 Ω: V = sqrt(P * R), P = 10^(dBm/10) * 1e-3.
//      Derived from Thetis Common.UVfromDBM which uses (dBm + 107) / 20
//      (a -107 dBm = 1 µV at 50 Ω simplification).  The full physics formula
//      is preserved here for accuracy; both agree to <1% across the ham-band
//      signal range.
// ---------------------------------------------------------------------------
QString MeterItem::formatValue(float dBm, MeterUnit unit, bool decimal)
{
    return formatValue(dBm, unit, decimal, false);
}

// aboveS9: the item's slice is at or above the S9 frequency, where S9 is
// -93 dBm instead of -73 dBm.
// From Thetis common.cs:881-887 [v2.10.3.15] (GetSMeterUnits)
QString MeterItem::formatValue(float dBm, MeterUnit unit, bool decimal, bool aboveS9)
{
    switch (unit) {
        case MeterUnit::S: {
            // IARU S-meter: S9 = -73 dBm, 6 dB/S-unit
            // From Thetis console.cs getMeterData dBm→S conversion [v2.10.3.13]
            const float diff = dBm - s9Dbm(aboveS9);
            const int sUnits = qBound(0, qRound(9.0f + diff / 6.0f), 19);
            if (sUnits <= 9) {
                return QStringLiteral("S%1").arg(sUnits);
            } else {
                const int over = (sUnits - 9) * 6;  // S9+10, S9+20, etc.
                return QStringLiteral("S9+%1").arg(over);
            }
        }
        case MeterUnit::dBm:
            return decimal ? QString::number(static_cast<double>(dBm), 'f', 1)
                           : QString::number(qRound(dBm));
        case MeterUnit::uV: {
            // µV at 50Ω: V = sqrt(P * R), P = 10^(dBm/10) * 1e-3
            // From Thetis Common.UVfromDBM [v2.10.3.13]
            const double powerW = std::pow(10.0, static_cast<double>(dBm) / 10.0) * 1e-3;
            const double volts  = std::sqrt(powerW * 50.0);
            const double uV     = volts * 1e6;
            if (uV < 1.0) {
                return QString::number(uV, 'f', decimal ? 2 : 0) + QStringLiteral(" µV");
            }
            return QString::number(uV, 'f', decimal ? 1 : 0) + QStringLiteral(" µV");
        }
    }
    return QString::number(static_cast<double>(dBm), 'f', 1);
}

// ---------------------------------------------------------------------------
// MeterItem base — serialize / deserialize
// Format: x|y|w|h|bindingId|zOrder
// ---------------------------------------------------------------------------

void MeterItem::applyNewItemDefaults()
{
    // kDefaultUpdateIntervalMs: Thetis's nudMeterItemUpdateRate default.
    setUpdateIntervalMs(kDefaultUpdateIntervalMs);
}

void MeterItem::setColourUserSet(const QString& field, bool picked)
{
    if (picked) {
        m_userColours.insert(field);
    } else {
        m_userColours.remove(field);
    }
}

QStringList MeterItem::userColourFields() const
{
    QStringList fields(m_userColours.cbegin(), m_userColours.cend());
    fields.sort();
    return fields;
}

void MeterItem::setUserColourFields(const QStringList& fields)
{
    m_userColours = QSet<QString>(fields.cbegin(), fields.cend());
    m_userColours.remove(QString());
}

void MeterItem::setUpdateIntervalMs(int ms)
{
    m_updateIntervalMs = ms > 0 ? ms : 0;
    m_lastAcceptedUpdateMs = -1;
}

bool MeterItem::acceptsUpdate(qint64 monotonicMs)
{
    if (m_updateIntervalMs <= 0) { return true; }
    // A clock that moved backwards (a new time base) starts a new interval.
    if (m_lastAcceptedUpdateMs >= 0 && monotonicMs >= m_lastAcceptedUpdateMs
        && monotonicMs - m_lastAcceptedUpdateMs < m_updateIntervalMs) {
        return false;
    }
    m_lastAcceptedUpdateMs = monotonicMs;
    return true;
}

void MeterItem::setMmioBinding(int index, const QUuid& guid, const QString& variableName)
{
    if (index < 0 || index >= kMaxMmioVariables) { return; }
    if (index == 0) { setMmioBinding(guid, variableName); return; }
    if (m_extraMmio.size() < index) { m_extraMmio.resize(index); }
    m_extraMmio[index - 1] = {guid, variableName};
    // Trailing unset variables are dropped, so an item that never had a
    // second variable compares and saves as it did before.
    while (!m_extraMmio.isEmpty() && m_extraMmio.constLast().guid.isNull()
           && m_extraMmio.constLast().variable.isEmpty()) {
        m_extraMmio.removeLast();
    }
}

QUuid MeterItem::mmioGuid(int index) const
{
    if (index == 0) { return m_mmioGuid; }
    if (index < 1 || index > m_extraMmio.size()) { return {}; }
    return m_extraMmio.at(index - 1).guid;
}

QString MeterItem::mmioVariable(int index) const
{
    if (index == 0) { return m_mmioVariable; }
    if (index < 1 || index > m_extraMmio.size()) { return {}; }
    return m_extraMmio.at(index - 1).variable;
}

bool MeterItem::hasMmioBinding(int index) const
{
    const QString variable = mmioVariable(index);
    return !mmioGuid(index).isNull() && !variable.isEmpty()
        && variable != QLatin1String("--DEFAULT--");
}

QString MeterItem::mmioSourceKey(int index) const
{
    return mmioGuid(index).toString(QUuid::WithoutBraces) + QChar(0) + mmioVariable(index);
}

bool MeterItem::hasAnyMmioBinding() const
{
    for (int index = 0; index < kMaxMmioVariables; ++index) {
        if (hasMmioBinding(index)) { return true; }
    }
    return false;
}

// From Thetis MeterManager.cs:22684-22738 [v2.10.3.15] — MeterVariables,
// keyed by the container item type that builds each Thetis MeterType.
// Types Thetis does not list (and the building blocks) read one variable.
int MeterItem::mmioVariableCount(const QString& typeId)
{
    // number of variables
    static const QHash<QString, int> kVariables{
        {QStringLiteral("meter.adc"), 2},          // MeterType.ADC
        {QStringLiteral("meter.agc"), 2},          // MeterType.AGC
        {QStringLiteral("meter.mic"), 2},          // MeterType.MIC
        {QStringLiteral("meter.alc"), 2},          // MeterType.ALC
        {QStringLiteral("meter.eq"), 2},           // MeterType.EQ
        {QStringLiteral("meter.leveler"), 2},      // MeterType.LEVELER
        {QStringLiteral("meter.comp"), 2},         // MeterType.COMP
        //case MeterType.CPDR: break;
        {QStringLiteral("meter.cfc"), 2},          // MeterType.CFC
        {QStringLiteral("meter.ananMulti"), 7},    // MeterType.ANANMM
        {QStringLiteral("meter.crossNeedle"), 2},  // MeterType.CROSS
        {QStringLiteral("meter.vfoDisplay"), 0},   // MeterType.VFO_DISPLAY
        {QStringLiteral("meter.clock"), 0},        // MeterType.CLOCK
        {QStringLiteral("meter.spacer"), 0},       // MeterType.SPACER
        {QStringLiteral("ROTATOR"), 2},            // MeterType.ROTATOR (AZ, ELE)
        {QStringLiteral("meter.historyGraph"), 2}, // MeterType.HISTORY
        {QStringLiteral("HISTORY"), 2},            // clsHistoryItem axis 0 and axis 1 (MeterManager.cs:26952-26955 [v2.10.3.15])
        {QStringLiteral("meter.customBar"), 2},    // MeterType.CUSTOM_METER_BAR
    };
    return kVariables.value(typeId, 1);
}

// From Thetis MeterManager.cs:33159-33160 [v2.10.3.15]: the update
// interval, at most 500 ms. NereusSDR steps each item on its own interval
// (Thetis uses the container's quickest).
int MeterItem::fadeIntervalMs() const
{
    constexpr int kMaxFadeIntervalMs = 500;
    const int interval = m_updateIntervalMs > 0 ? m_updateIntervalMs : kDefaultUpdateIntervalMs;
    return std::min(interval, kMaxFadeIntervalMs);
}

bool MeterItem::advanceFade(qint64 monotonicMs)
{
    // A clock that moved backwards (a new time base), or the first call,
    // only takes the time.
    if (m_lastFadeStepMs < 0 || monotonicMs < m_lastFadeStepMs) {
        m_lastFadeStepMs = monotonicMs;
        return false;
    }
    const int interval = fadeIntervalMs();
    // A foreign session's entry does not follow this window's transmit.
    const bool tx = m_transmitting && !m_foreignSession;
    // From Thetis MeterManager.cs:33156-33157,33166 [v2.10.3.15]: a
    // Disabled item dims whatever its fade flags, which ContentFade::nextValue
    // takes as both flags true. Not for a foreign session's entry.
    // [2.10.1.0] MW0LGE
    const bool disabled = m_stageDisabled && !m_foreignSession;
    const bool rx = disabled || fadeOnRx();
    const bool txFade = disabled || fadeOnTx();
    bool changed = false;
    while (monotonicMs - m_lastFadeStepMs >= interval) {
        const int next = ContentFade::nextValue(m_fadeValue, rx, txFade, tx, interval);
        if (next == m_fadeValue) {
            // At rest: the next change counts from now.
            m_lastFadeStepMs = monotonicMs;
            break;
        }
        m_fadeValue = next;
        m_lastFadeStepMs += interval;
        changed = true;
    }
    return changed;
}

void MeterItem::resetForTxTransition(bool inTx)
{
    if (!inTx && bindingId() >= MeterBinding::TxPower && bindingId() < MeterBinding::HwVolts) {
        if (BarItem* bar = qobject_cast<BarItem*>(this)) { bar->clearSmoothing(0.0); }
    }
}

void MeterItem::resetRxSource()
{
    if (hasMmioBinding()) { return; }
    for (int binding : readingBindings()) {
        if (binding >= MeterBinding::SignalPeak && binding <= MeterBinding::PbSnr) {
            pushBindingValue(binding, kNoMeterReadingDbm);
        }
    }
}

void MeterItem::setPowerScale(int watts)
{
    setPowerScale(watts, watts * ControlRanges::kRfPowerGaugeHeadroom);
}

void MeterItem::setPowerScale(int limitWatts, double topWatts)
{
    if (limitWatts <= 0 || topWatts <= 0.0) { return; }
    const double red = limitWatts;
    const double top = topWatts;
    if (objectName() == QStringLiteral("PowerBar")) {
        if (BarItem* bar = qobject_cast<BarItem*>(this)) { bar->setRange(0.0, top); bar->setRedThreshold(red); }
    } else if (objectName() == QStringLiteral("PowerScale")) {
        if (ScaleItem* scale = qobject_cast<ScaleItem*>(this)) { scale->setRange(0.0, top); scale->setMajorTicks(limitWatts <= 10 || (limitWatts == 1500 && top == 2000.0) ? 5 : 7); }
    }
}

QString MeterItem::serialize() const
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(static_cast<double>(m_x))
        .arg(static_cast<double>(m_y))
        .arg(static_cast<double>(m_w))
        .arg(static_cast<double>(m_h))
        .arg(m_bindingId)
        .arg(m_zOrder);
}

bool MeterItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 6) {
        return false;
    }

    bool ok = true;
    const float x  = parts[0].toFloat(&ok);  if (!ok) { return false; }
    const float y  = parts[1].toFloat(&ok);  if (!ok) { return false; }
    const float w  = parts[2].toFloat(&ok);  if (!ok) { return false; }
    const float h  = parts[3].toFloat(&ok);  if (!ok) { return false; }
    const int bid  = parts[4].toInt(&ok);    if (!ok) { return false; }
    const int zo   = parts[5].toInt(&ok);    if (!ok) { return false; }

    setRect(x, y, w, h);
    setBindingId(bid);
    setZOrder(zo);
    return true;
}

// ---------------------------------------------------------------------------
// MeterItem base — stacked-row layout (Phase 3G-9 post-revert)
// ---------------------------------------------------------------------------
//
// Thetis-parity stack layout. Each row's effective normalized y/h
// is computed from:
//
//   slotHNorm = slotHeightPx / widgetHeightPx
//   slotYNorm = bandTop + stackSlot * slotHNorm
//   m_y       = slotYNorm + m_slotLocalY * slotHNorm
//   m_h       = m_slotLocalH * slotHNorm
//
// The caller is responsible for choosing slotHeightPx — in the
// normal NereusSDR path that's
// `max(kNormalRowHNorm * widgetHeightPx, kMinRowPx)` (Thetis's
// `_fHeight = 0.05f` with a pixel floor so rows stay readable when
// the container is unusually small). bandTop is derived per-reflow
// from the composite sitting above the stack — typically 0 when no
// composite is present, or the composite's max y+h otherwise.
// Rows with slotYNorm + m_h > 1.0 overflow the widget bottom and
// clip naturally at paint time, matching Thetis Default Multimeter
// container behaviour when more rows are added than fit.

void MeterItem::layoutInStackSlot(int widgetHeightPx, int slotHeightPx,
                                   float bandTop)
{
    if (m_stackSlot < 0 || widgetHeightPx <= 0 || slotHeightPx <= 0) {
        return;
    }
    const float slotHNorm = static_cast<float>(slotHeightPx)
                          / static_cast<float>(widgetHeightPx);
    const float slotYNorm = bandTop
                          + static_cast<float>(m_stackSlot) * slotHNorm;
    m_y = slotYNorm + m_slotLocalY * slotHNorm;
    m_h = m_slotLocalH * slotHNorm;
}

// ---------------------------------------------------------------------------
// MeterItem base — mouse interaction (Phase 3G-5)
// ---------------------------------------------------------------------------

bool MeterItem::hitTest(const QPointF& pos, int widgetW, int widgetH) const
{
    return pixelRect(widgetW, widgetH).contains(pos.toPoint());
}

bool MeterItem::handleMousePress(QMouseEvent* event, int widgetW, int widgetH)
{
    Q_UNUSED(event); Q_UNUSED(widgetW); Q_UNUSED(widgetH);
    return false;
}

bool MeterItem::handleMouseRelease(QMouseEvent* event, int widgetW, int widgetH)
{
    Q_UNUSED(event); Q_UNUSED(widgetW); Q_UNUSED(widgetH);
    return false;
}

bool MeterItem::handleMouseMove(QMouseEvent* event, int widgetW, int widgetH)
{
    Q_UNUSED(event); Q_UNUSED(widgetW); Q_UNUSED(widgetH);
    return false;
}

bool MeterItem::handleWheel(QWheelEvent* event, int widgetW, int widgetH)
{
    Q_UNUSED(event); Q_UNUSED(widgetW); Q_UNUSED(widgetH);
    return false;
}

// ---------------------------------------------------------------------------
// Helper: build the base fields string (indices 1-6) for concrete types.
// Concrete types prepend their prefix and append type-specific fields.
// ---------------------------------------------------------------------------
static QString baseFields(const MeterItem& item)
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(static_cast<double>(item.x()))
        .arg(static_cast<double>(item.y()))
        .arg(static_cast<double>(item.itemWidth()))
        .arg(static_cast<double>(item.itemHeight()))
        .arg(item.bindingId())
        .arg(item.zOrder());
}

// Helper: parse base fields from indices 1..6 of parts (index 0 is the prefix).
static bool parseBaseFields(MeterItem& item, const QStringList& parts)
{
    if (parts.size() < 7) {
        return false;
    }
    // Re-join indices 1..6 and delegate to the base deserialize
    const QString base = QStringList(parts.mid(1, 6)).join(QLatin1Char('|'));
    return item.MeterItem::deserialize(base);
}

// ---------------------------------------------------------------------------
// SolidColourItem
// Format: SOLID|x|y|w|h|bindingId|zOrder|#aarrggbb
// ---------------------------------------------------------------------------

namespace {
// New-item defaults (clsSolidColour constructor).
struct SolidThetisDefaults {
    QRgb colour;
};
// From Thetis MeterManager.cs:7663 [v2.10.3.15] (_colour = System.Drawing.Color.Black)
constexpr SolidThetisDefaults kSolidThetisDefaults{qRgb(0, 0, 0)};
} // namespace

void SolidColourItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    m_colour = QColor::fromRgb(kSolidThetisDefaults.colour);
}

void SolidColourItem::paint(QPainter& p, int widgetW, int widgetH)
{
    p.fillRect(pixelRect(widgetW, widgetH), ItemColourLook::effective("Solid.colour", m_colour, this));
}

QString SolidColourItem::serialize() const
{
    return QStringLiteral("SOLID|%1|%2")
        .arg(baseFields(*this))
        .arg(m_colour.name(QColor::HexArgb));
}

bool SolidColourItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 8 || parts[0] != QLatin1String("SOLID")) {
        return false;
    }
    if (!parseBaseFields(*this, parts)) {
        return false;
    }
    m_colour = QColor(parts[7]);
    return m_colour.isValid();
}

// ---------------------------------------------------------------------------
// ImageItem
// Format: IMAGE|x|y|w|h|bindingId|zOrder|path[|darkMode(0/1)]
// ---------------------------------------------------------------------------

void ImageItem::setImagePath(const QString& path)
{
    m_imagePath = path;
    // An empty path (a new item) has no image; QImage::load would warn.
    // An empty path is no image; it never reaches a file API.
    // From Thetis MeterManager.cs:40639 [v2.10.3.15]
    if (path.isEmpty()) {
        m_image = QImage();
    } else {
        m_image.load(path);
    }
    m_loadedPath = path;
}

namespace {
// meter.png + "-dark" -> meter-dark.png, in the chosen file's folder.
QString imageVariantName(const QString& path, const QString& suffix)
{
    const QFileInfo info(path);
    const QString ext = info.suffix();
    QString name = info.completeBaseName() + suffix;
    if (!ext.isEmpty()) {
        name += QLatin1Char('.') + ext;
    }
    return info.dir().filePath(name);
}
} // namespace

// From Thetis MeterManager.cs:40641-40650 [v2.10.3.15] (renderImage)
QString ImageItem::variantPath(const QString& basePath, bool darkMode, double diagonalPx)
{
    if (basePath.isEmpty()) {
        return basePath;
    }
    QString image = basePath;
    if (darkMode) {
        const QString key = imageVariantName(image, QStringLiteral("-dark"));
        if (QFileInfo::exists(key)) {
            image = key;
        }
    }

    QString sizeKey;
    if (diagonalPx <= kImageSmallDiagonalPx) {
        sizeKey = imageVariantName(image, QStringLiteral("-small"));
    } else if (diagonalPx >= kImageLargeDiagonalPx) {
        sizeKey = imageVariantName(image, QStringLiteral("-large"));
    }

    if (!sizeKey.isEmpty() && QFileInfo::exists(sizeKey)) {
        image = sizeKey; // with size
    }
    return image;
}

void ImageItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    if (!m_imagePath.isEmpty()) {
        const double diagonal = std::sqrt(static_cast<double>(rect.width()) * rect.width()
                                          + static_cast<double>(rect.height()) * rect.height());
        const QString wanted = variantPath(m_imagePath, m_darkMode, diagonal);
        if (wanted != m_loadedPath) {
            m_image.load(wanted);
            m_loadedPath = wanted;
        }
    }
    if (m_image.isNull()) {
        return;
    }
    p.drawImage(rect, m_image);
}

QString ImageItem::serialize() const
{
    // Joined, not chained .arg(): a path holding "%1" must stay as written.
    return QStringList{QStringLiteral("IMAGE"), baseFields(*this), m_imagePath,
                       QString::number(m_darkMode ? 1 : 0)}
        .join(QLatin1Char('|'));
}

bool ImageItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 8 || parts[0] != QLatin1String("IMAGE")) {
        return false;
    }
    if (!parseBaseFields(*this, parts)) {
        return false;
    }
    setImagePath(parts[7]);
    // Appended field; records without it are not dark.
    m_darkMode = parts.size() >= 9 && parts[8] == QLatin1String("1");
    return true;
}

void ImageItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    m_darkMode = kImageThetisDefaults.darkMode;
}

// ---------------------------------------------------------------------------
// BarItem
// Format (fields 0-14 legacy, 15-19 Edge, 20-22 Phase A1, 23-25 A2,
// 26-29 A3, 30 A4, 31 E2):
//   BAR|x|y|w|h|bindingId|zOrder|orientation|minVal|maxVal|
//       barColor|barRedColor|redThreshold|attackRatio|decayRatio|
//       barStyle|edgeBgColor|edgeLowColor|edgeHighColor|edgeAvgColor|
//       showValue|showPeakValue|fontColour|
//       showHistory|historyColour|historyDurationMs|
//       showMarker|markerColour|peakHoldMarkerColour|peakHoldDecayRatio|
//       scaleCalibration|peakFontColour
// ---------------------------------------------------------------------------

// Override setValue() for exponential smoothing.
// From Thetis MeterManager.cs — attack when rising, decay when falling.
// ---------------------------------------------------------------------------
// BarItem — Phase A4 ScaleCalibration (non-linear value → X map).
// From Thetis clsBarItem (MeterManager.cs:20192 field, 21547-21549 addSMeterBar
// 3-point S-meter calibration, 21638-21640 AddADCMaxMag 3-point ADC curve).
// ---------------------------------------------------------------------------
namespace {
QString barStyleName(BarItem::BarStyle style)
{
    switch (style) {
        case BarItem::BarStyle::Edge:     return QStringLiteral("Edge");
        case BarItem::BarStyle::Line:     return QStringLiteral("Line");
        case BarItem::BarStyle::Segments: return QStringLiteral("Segments");
        case BarItem::BarStyle::Solid:    return QStringLiteral("Solid");
        case BarItem::BarStyle::Filled:   break;
    }
    return QStringLiteral("Filled");
}
} // namespace

void BarItem::addScaleCalibration(double value, float normalizedX)
{
    m_scaleCalibration.insert(value, normalizedX);
}

float BarItem::valueToNormalizedX(double value) const
{
    // Empty calibration: fall back to linear minVal..maxVal mapping. This
    // preserves pre-A4 behavior for every Filled/Edge preset.
    if (m_scaleCalibration.isEmpty()) {
        const double range = m_maxVal - m_minVal;
        if (range <= 0.0) { return 0.0f; }
        const double frac = std::clamp((value - m_minVal) / range, 0.0, 1.0);
        return static_cast<float>(frac);
    }

    // A signal-strength reading on a slice at or above the S9 frequency is
    // raised 20 dB, so the calibrated S9 point (-73) reads -93 dBm.
    // From Thetis MeterManager.cs:41049-41052 [v2.10.3.15] (getPerc)
    value = aboveS9Adjusted(m_bindingId, value, m_aboveS9Frequency);
    // A calibrated forward or reverse power bar reads against its power
    // limit. From Thetis MeterManager.cs:41053-41056 [v2.10.3.15] (getPerc;
    // MaxPower only for PWR / REVERSE_PWR, MeterManager.cs:28292)
    if ((m_bindingId == MeterBinding::TxPower || m_bindingId == MeterBinding::TxReversePower)
        && m_powerLimit > 0.0) {
        value *= 100.0 / m_powerLimit;
    }

    // Clamp below the first waypoint.
    auto itFirst = m_scaleCalibration.constBegin();
    if (value <= itFirst.key()) {
        return itFirst.value();
    }
    // Clamp above the last waypoint.
    auto itLast = std::prev(m_scaleCalibration.constEnd());
    if (value >= itLast.key()) {
        return itLast.value();
    }

    // Piecewise-linear interpolation between adjacent waypoints.
    auto hi = m_scaleCalibration.upperBound(value);
    auto lo = std::prev(hi);
    const double k0 = lo.key();
    const double k1 = hi.key();
    const float  x0 = lo.value();
    const float  x1 = hi.value();
    const double span = k1 - k0;
    if (span <= 0.0) { return x0; }
    const double t = (value - k0) / span;
    return static_cast<float>(x0 + (x1 - x0) * t);
}

BarItem::SegmentGeometry BarItem::segmentGeometry(float w, float h)
{
    // From Thetis MeterManager.cs:37458-37470 [v2.10.3.15]
    float fBarHeight = h * 0.85f; // same as base line % in render scale

    int segmentBlockSize = static_cast<int>(w * 0.02f);
    if (segmentBlockSize < 7) { segmentBlockSize = 7; } // minimum 7 pixels
    const int segmentGapSize = static_cast<int>(static_cast<float>(segmentBlockSize) * 0.2f);
    if (segmentBlockSize < 2) { segmentBlockSize = 2; } // minimum 2 pixels

    const int segmentStep = segmentBlockSize + segmentGapSize; // step, to include block + gap
    return SegmentGeometry{segmentBlockSize, segmentGapSize, segmentStep, fBarHeight};
}

void BarItem::setIgnoreHistoryMs(int ms)
{
    m_ignoreHistoryMs = std::clamp(ms, kMinBarIgnoreHistoryMs, kMaxBarIgnoreHistoryMs);
}

void BarItem::setPowerLimit(double watts)
{
    // From Thetis MeterManager.cs:7136-7137 [v2.10.3.15]: below 0.1 recovers
    // (Thetis takes CurrentPowerRating; NereusSDR takes the setup default).
    // [original inline comment from MeterManager.cs:7137] // incase of 0 recovery from db
    if (!(watts >= kMinBarPowerLimitW)) {
        m_powerLimit = kDefaultBarPowerLimitW;
        return;
    }
    m_powerLimit = std::min(watts, kMaxBarPowerLimitW);
}

void BarItem::clearHistory()
{
    // From Thetis MeterManager.cs:21450-21457 [v2.10.3.15] (ClearHistory)
    m_history.clear();
    // With no history Thetis's peak (MaxHistory) is the live value.
    m_peakValue = m_smoothedValue;
    const int interval = updateIntervalMs() > 0 ? updateIntervalMs() : kDefaultUpdateIntervalMs;
    m_ignoringNext = m_ignoreHistoryMs / interval;  // ignore next N readings to make up 2 second of ignore
}

void BarItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    const BarItemThetisDefaults& d = kBarItemThetisDefaults;
    m_historyDurationMs    = d.historyDurationMs;
    m_ignoreHistoryMs      = d.ignoreHistoryMs;
    m_showHistory          = d.showHistory;
    m_showValue            = d.showValue;
    m_showPeakValue        = d.showPeakValue;
    m_barStyle             = d.style;
    m_barColor             = QColor::fromRgb(d.colour);
    m_barRedColor          = QColor::fromRgb(d.colourHigh);
    m_markerColour         = QColor::fromRgb(d.markerColour);
    m_peakHoldMarkerColour = QColor::fromRgb(d.peakHoldMarkerColour);
    m_peakFontColour       = QColor::fromRgb(d.peakValueColour);
    m_strokeWidth          = d.strokeWidth;
    m_peakHold             = d.peakHold;
    m_showMarker           = d.showMarker;
}

void BarItem::setValue(double v)
{
    MeterItem::setValue(v);
    // NereusSDR (R-R3-13): no reading.  The bar falls straight to the scale
    // minimum, the peak and the history trail clear (so the peak text shows
    // "--" and no stale marker remains), and the next real value starts
    // afresh from the minimum.
    if (isNoReading(v)) {
        m_smoothedValue = m_minVal;
        m_peakValue = -std::numeric_limits<double>::infinity();
        m_history.clear();
        return;
    }
    if (v > m_smoothedValue) {
        m_smoothedValue = static_cast<double>(m_attackRatio) * v
                        + (1.0 - static_cast<double>(m_attackRatio)) * m_smoothedValue;
    } else {
        m_smoothedValue = static_cast<double>(m_decayRatio) * v
                        + (1.0 - static_cast<double>(m_decayRatio)) * m_smoothedValue;
    }
    // After clearHistory() the next readings move the bar but not the
    // history or the peak. From Thetis MeterManager.cs:21367-21378 [v2.10.3.15]
    if (m_ignoringNext > 0) {
        --m_ignoringNext;
        return;
    }
    // Phase A1/A3 — peak-hold tracking. When PeakHoldDecayRatio == 0 the
    // peak sticks at the highest value ever seen (A1 behavior). When > 0,
    // the peak decays geometrically toward the live value on every frame
    // where v < peak, matching Thetis clsBarItem peak-hold marker semantics
    // (MeterManager.cs:21541, 21544).
    if (v > m_peakValue || !std::isfinite(m_peakValue)) {
        m_peakValue = v;
    } else if (m_peakHoldDecayRatio > 0.0f) {
        const double r = static_cast<double>(m_peakHoldDecayRatio);
        m_peakValue = m_peakValue + (v - m_peakValue) * r;
    }

    // Phase A2 — history ring buffer. Thetis clsBarItem pushes samples at
    // UpdateInterval and keeps (HistoryDuration / UpdateInterval) entries.
    // Without a poll-clock reference at setValue time we bound the buffer
    // by a conservative upper limit (HistoryDurationMs at 100 Hz = 400).
    if (m_showHistory) {
        m_history.append(v);
        const int maxSamples = qMax(2, m_historyDurationMs / 10);
        while (m_history.size() > maxSamples) {
            m_history.removeFirst();
        }
    }
}

QString BarItem::valueText() const
{
    // NereusSDR (R-R3-13): "--" with no reading.
    if (isNoReading(m_value) || !std::isfinite(m_smoothedValue)) {
        return QStringLiteral("--");
    }
    // Task 3.2: unit-mode fan-out; signal-level bars route through
    // formatValue so the MultimeterPage unit combo drives the readout.
    return formatValue(static_cast<float>(m_smoothedValue), m_unitMode, m_showDecimal,
                       m_aboveS9Frequency);
}

QString BarItem::peakValueText() const
{
    return std::isfinite(m_peakValue)
        // Task 3.2: peak value also routed through formatValue
        ? formatValue(static_cast<float>(m_peakValue), m_unitMode, m_showDecimal,
                      m_aboveS9Frequency)
        : QStringLiteral("--");
}

void BarItem::paint(QPainter& p, int widgetW, int widgetH)
{
    // Dispatch to Edge renderer if configured
    // From Thetis console.cs:12612-12678
    if (m_barStyle == BarStyle::Edge) {
        paintEdge(p, widgetW, widgetH);
        return;
    }

    const QRect rect = pixelRect(widgetW, widgetH);
    const QColor& fillColor = (m_smoothedValue >= m_redThreshold)
                                  ? ItemColourLook::effective("Bar.barRedColor", m_barRedColor, this) : ItemColourLook::effective("Bar.barColor", m_barColor, this);

    // Bar body. Uses valueToNormalizedX() so both the linear min..max
    // fallback (empty calibration) and the Phase A4 piecewise-linear
    // calibration route through one code path. Only Horizontal
    // orientation supports the Phase A extensions; Vertical keeps its
    // pre-A1 behavior because no Thetis preset uses it.
    if (m_orientation == Orientation::Vertical) {
        const double range = m_maxVal - m_minVal;
        const double frac  = (range > 0.0)
            ? std::clamp((m_smoothedValue - m_minVal) / range, 0.0, 1.0)
            : 0.0;
        const int fillH = static_cast<int>(frac * rect.height());
        p.fillRect(QRect(rect.left(), rect.bottom() - fillH,
                         rect.width(), fillH),
                   fillColor);
        return;
    }

    const float fracLive = valueToNormalizedX(m_smoothedValue);
    const int fillW = static_cast<int>(fracLive * rect.width());

    // Phase A2 — history trailing fill. Drawn FIRST so the live bar
    // overlays it. Each sample produces one vertical line at its own
    // calibrated X, with full-height alpha-blended stroke using
    // m_historyColour. Older samples fade via the alpha in the colour
    // itself; a stronger trail effect will come with VBO work later.
    if (m_showHistory && !m_history.isEmpty()) {
        p.setPen(QPen(ItemColourLook::effective("Bar.historyColour", m_historyColour, this), 1));
        for (double v : m_history) {
            const float hx = valueToNormalizedX(v);
            const int px = rect.left() + static_cast<int>(hx * rect.width());
            p.drawLine(px, rect.top(), px, rect.bottom());
        }
    }

    // Phase A4 — Line style draws only a thin baseline at the bottom
    // of the bar rect instead of a filled quad. Matches Thetis
    // BarStyle.Line where the "bar" is really a baseline + markers +
    // labels composition.
    if (m_barStyle == BarStyle::Segments || m_barStyle == BarStyle::Solid) {
        paintSegmentsOrSolid(p, rect, fracLive);
    } else if (m_barStyle == BarStyle::Line) {
        p.setPen(QPen(fillColor, 2));
        const int baselineY = rect.bottom() - 2;
        p.drawLine(rect.left(), baselineY,
                   rect.left() + fillW, baselineY);
    } else {
        // Default Filled bar (existing pre-A behavior, now routed
        // through valueToNormalizedX so a calibrated Filled bar also
        // works if anyone wants it).
        p.fillRect(QRect(rect.left(), rect.top(),
                         fillW, rect.height()),
                   fillColor);
    }

    // Phase A3 — live value marker + peak-hold marker (Thetis
    // MeterManager.cs:21543-21544). Thin vertical lines at the live
    // smoothed X and the separate peak X. Each has its own switch
    // (ShowMarker, PeakHold; MeterManager.cs:37600-37606 [v2.10.3.15]) and
    // both use StrokeWidth. The live marker is drawn first, as NereusSDR
    // always has, so a saved bar paints as before.
    if (m_showMarker) {
        p.setPen(QPen(ItemColourLook::effective("Bar.markerColour", m_markerColour, this), static_cast<double>(m_strokeWidth)));
        const int liveX = rect.left() + static_cast<int>(fracLive * rect.width());
        p.drawLine(liveX, rect.top(), liveX, rect.bottom());
    }
    if (peakHold() && std::isfinite(m_peakValue)) {
        p.setPen(QPen(ItemColourLook::effective("Bar.peakHoldMarkerColour", m_peakHoldMarkerColour, this), static_cast<double>(m_strokeWidth)));
        const float fracPeak = valueToNormalizedX(m_peakValue);
        const int peakX = rect.left() + static_cast<int>(fracPeak * rect.width());
        p.drawLine(peakX, rect.top(), peakX, rect.bottom());
    }

    // Phase A1 / E — ShowValue / ShowPeakValue text labels. ShowValue
    // is drawn top-left in m_fontColour (Thetis default white), and
    // ShowPeakValue top-right in peakFontColour() (Thetis default red
    // — per Setup → Appearance → Meters/Gadgets Peak Value swatch).
    // Both use a small proportional bold font. Idle ShowValue is
    // skipped; idle ShowPeakValue renders "--" as a placeholder so
    // parked bindings (TX-only readings while RX active, etc.) still
    // show a slot where the peak text lives.  NereusSDR (R-R3-13): with
    // no reading ShowValue renders "--" too.
    if (m_showValue || m_showPeakValue) {
        QFont font = WorkspacePreview::readableFont(p.font(), p.device());
        const int fontPx = qMax(9, rect.height() / 3);
        font.setPixelSize(WorkspacePreview::readableTextPixelSize(fontPx));
        font.setBold(true);
        p.setFont(font);
        const QFontMetrics fm(font);

        if (m_showValue
            && (std::isfinite(m_smoothedValue) || isNoReading(m_value))) {
            p.setPen(ItemColourLook::effective("Bar.fontColour", m_fontColour, this));
            const QString s = valueText();
            p.drawText(rect.left() + 2,
                       rect.top() + fm.ascent(),
                       s);
        }
        if (m_showPeakValue) {
            const QString s = peakValueText();
            p.setPen(ItemColourLook::effective("Bar.peakFontColour", peakFontColour(), this));
            const int w = fm.horizontalAdvance(s);
            p.drawText(rect.right() - w - 2,
                       rect.top() + fm.ascent(),
                       s);
        }
    }
}

// Segments and SolidFilled bodies.
// From Thetis MeterManager.cs:37458-37598 [v2.10.3.15] (renderHBar). The
// low colour is barColor(), the high colour barRedColor(); the high
// transition is redThreshold() on the bar's scale (Thetis HighPoint).
void BarItem::paintSegmentsOrSolid(QPainter& p, const QRect& rect, float fracLive)
{
    const QColor barColour = ItemColourLook::effective("Bar.barColor", m_barColor, this);
    const QColor barRedColour = ItemColourLook::effective("Bar.barRedColor", m_barRedColor, this);
    const float x = static_cast<float>(rect.left());
    const float y = static_cast<float>(rect.top());
    const float w = static_cast<float>(rect.width());
    const float h = static_cast<float>(rect.height());
    const float xPos = x + fracLive * w;

    const SegmentGeometry g = segmentGeometry(w, h);
    const float segmentBlockSize = static_cast<float>(g.blockSize);
    const float segmentStep = static_cast<float>(g.step);
    const float fBarHeight = g.barHeight;

    float fHighXPosTransition = std::numeric_limits<float>::max();
    const double scaleTop = m_scaleCalibration.isEmpty()
        ? m_maxVal : std::prev(m_scaleCalibration.constEnd()).key();
    if (m_redThreshold < scaleTop) {
        // From Thetis MeterManager.cs:37482 [v2.10.3.15]
        fHighXPosTransition = x + (w * valueToNormalizedX(m_redThreshold));
    }

    const auto fill = [&p, y, fBarHeight](float left, float width, const QColor& c) {
        if (width > 0.0f) {
            p.fillRect(QRectF(left, y, width, fBarHeight), c);
        }
    };

    if (m_barStyle == BarStyle::Solid) {
        // From Thetis MeterManager.cs:37532-37547 [v2.10.3.15]
        const float fEnd = xPos < fHighXPosTransition ? xPos : fHighXPosTransition;
        fill(x, fEnd - x, barColour);
        if (fEnd < xPos) // the area in the high range
        {
            // complete the bar
            fill(fEnd, xPos - fEnd, barRedColour);
        }
        return;
    }

    // Segments. From Thetis MeterManager.cs:37553-37597 [v2.10.3.15]
    float i;
    const float fEnd = xPos < fHighXPosTransition ? xPos : fHighXPosTransition;

    for (i = x; i < fEnd - segmentStep; i += segmentStep) {
        fill(i, segmentBlockSize, barColour);
    }

    // complete the end sliver up to the high transition
    if (i < fEnd) {
        fill(i, fEnd - i, barColour);
    }

    if (xPos > fHighXPosTransition) // the area in the high range
    {
        // sliver to complete block at high transition
        if (i < fEnd) {
            fill(fEnd, i + segmentBlockSize - fEnd, barRedColour);
        }

        float j;
        for (j = i + segmentStep; j < xPos - segmentStep; j += segmentStep) {
            fill(j, segmentBlockSize, barRedColour);
        }

        // complete the end sliver
        if (j < xPos) {
            fill(j, xPos - j, barRedColour);
        }
    }
}

void BarItem::emitVertices(QVector<float>& verts, int widgetW, int widgetH)
{
    Q_UNUSED(widgetW); Q_UNUSED(widgetH);

    const double range = m_maxVal - m_minVal;
    const double frac  = (range > 0.0)
        ? std::clamp((m_smoothedValue - m_minVal) / range, 0.0, 1.0)
        : 0.0;

    // Convert normalized item rect to NDC
    const float ndcL = m_x * 2.0f - 1.0f;
    const float ndcR = (m_x + m_w) * 2.0f - 1.0f;
    const float ndcT = 1.0f - m_y * 2.0f;
    const float ndcB = 1.0f - (m_y + m_h) * 2.0f;

    const QColor& fillColor = (m_smoothedValue >= m_redThreshold) ? ItemColourLook::effective("Bar.barRedColor", m_barRedColor, this) : ItemColourLook::effective("Bar.barColor", m_barColor, this);
    const float r = static_cast<float>(fillColor.redF());
    const float g = static_cast<float>(fillColor.greenF());
    const float b = static_cast<float>(fillColor.blueF());
    const float a = static_cast<float>(fillColor.alphaF());

    // Compute filled quad corners
    float qL, qR, qT, qB;
    if (m_orientation == Orientation::Horizontal) {
        qL = ndcL;
        qR = ndcL + static_cast<float>(frac) * (ndcR - ndcL);
        qT = ndcT;
        qB = ndcB;
    } else {
        // Vertical: fill from bottom
        qL = ndcL;
        qR = ndcR;
        qB = ndcB;
        qT = ndcB + static_cast<float>(frac) * (ndcT - ndcB);
    }

    // Emit 6 vertices (2 triangles) for Triangles topology.
    // This avoids degenerate strip artifacts when multiple bars share the VBO.
    // Triangle 1: TL, BL, TR
    verts << qL << qT << r << g << b << a;
    verts << qL << qB << r << g << b << a;
    verts << qR << qT << r << g << b << a;
    // Triangle 2: TR, BL, BR
    verts << qR << qT << r << g << b << a;
    verts << qL << qB << r << g << b << a;
    verts << qR << qB << r << g << b << a;
}

// ---------------------------------------------------------------------------
// BarItem::paintEdge()
// Edge rendering mode: draws a single vertical indicator line instead of
// a filled bar. From Thetis console.cs:23589-23624
// ---------------------------------------------------------------------------
void BarItem::paintEdge(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);

    // Background outline (from Thetis console.cs:23589)
    const QColor edgeBg = ItemColourLook::effective("Bar.edgeBackgroundColor", m_edgeBgColor, this);
    const QColor edgeAvg = ItemColourLook::effective("Bar.edgeAvgColor", m_edgeAvgColor, this);
    p.setPen(QPen(edgeBg, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect);

    // Calculate meter position
    const double range = m_maxVal - m_minVal;
    const double frac = (range > 0.0)
        ? std::clamp((m_smoothedValue - m_minVal) / range, 0.0, 1.0)
        : 0.0;
    const int pixelX = rect.left() + static_cast<int>(frac * rect.width());

    // Shadow color: midpoint blend of indicator and background
    // From Thetis console.cs:23605-23608
    const QColor shadowColor = QColor(
        (edgeAvg.red()   + edgeBg.red())   / 2,
        (edgeAvg.green() + edgeBg.green()) / 2,
        (edgeAvg.blue()  + edgeBg.blue())  / 2
    );

    // Three vertical lines: shadow-center-shadow
    // From Thetis console.cs:23622-23624
    p.setPen(QPen(shadowColor, 1));
    p.drawLine(pixelX - 1, rect.top() + 1, pixelX - 1, rect.bottom() - 1);
    p.drawLine(pixelX + 1, rect.top() + 1, pixelX + 1, rect.bottom() - 1);

    p.setPen(QPen(edgeAvg, 1));
    p.drawLine(pixelX, rect.top() + 1, pixelX, rect.bottom() - 1);
}

QString BarItem::serialize() const
{
    // Fields 0-14:  BAR|x|y|w|h|bindingId|zOrder|orientation|min|max|
    //               barColor|barRedColor|redThreshold|attack|decay
    // Fields 15-19: barStyle|edgeBgColor|edgeLowColor|edgeHighColor|edgeAvgColor
    // Fields 20-22: showValue|showPeakValue|fontColour           (Phase A1)
    QString base = QStringLiteral("BAR|%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11|%12|%13|%14")
        .arg(baseFields(*this))
        .arg(m_orientation == Orientation::Horizontal ? QStringLiteral("H") : QStringLiteral("V"))
        .arg(m_minVal)
        .arg(m_maxVal)
        .arg(m_barColor.name(QColor::HexArgb))
        .arg(m_barRedColor.name(QColor::HexArgb))
        .arg(m_redThreshold)
        .arg(static_cast<double>(m_attackRatio))
        .arg(static_cast<double>(m_decayRatio))
        .arg(barStyleName(m_barStyle))
        .arg(m_edgeBgColor.name(QColor::HexArgb))
        .arg(m_edgeLowColor.name(QColor::HexArgb))
        .arg(m_edgeHighColor.name(QColor::HexArgb))
        .arg(m_edgeAvgColor.name(QColor::HexArgb));
    // Append-only tail (Phase A1). Legacy readers stop at field 19 and
    // these get the defaults — see tail-tolerant parse below.
    base += QLatin1Char('|') + (m_showValue     ? QStringLiteral("1") : QStringLiteral("0"));
    base += QLatin1Char('|') + (m_showPeakValue ? QStringLiteral("1") : QStringLiteral("0"));
    base += QLatin1Char('|') + m_fontColour.name(QColor::HexArgb);
    // Phase A2 tail: showHistory | historyColour | historyDurationMs
    base += QLatin1Char('|') + (m_showHistory ? QStringLiteral("1") : QStringLiteral("0"));
    base += QLatin1Char('|') + m_historyColour.name(QColor::HexArgb);
    base += QLatin1Char('|') + QString::number(m_historyDurationMs);
    // Phase A3 tail: showMarker | markerColour | peakHoldMarkerColour | peakHoldDecayRatio
    base += QLatin1Char('|') + (m_showMarker ? QStringLiteral("1") : QStringLiteral("0"));
    base += QLatin1Char('|') + m_markerColour.name(QColor::HexArgb);
    base += QLatin1Char('|') + m_peakHoldMarkerColour.name(QColor::HexArgb);
    base += QLatin1Char('|') + QString::number(static_cast<double>(m_peakHoldDecayRatio));
    // Phase A4 tail: scaleCalibration (semicolon-separated value=x pairs).
    // Empty-calibration BarItems emit an empty string, keeping the parser
    // identical for pre-A4 saves. The barStyle field (index 15) is already
    // serialized above — the Line value just joins Filled/Edge in the
    // existing string slot, so no new slot is needed for the enum itself.
    QStringList calParts;
    for (auto it = m_scaleCalibration.constBegin();
         it != m_scaleCalibration.constEnd(); ++it) {
        calParts << QStringLiteral("%1=%2")
                        .arg(it.key())
                        .arg(static_cast<double>(it.value()));
    }
    base += QLatin1Char('|') + calParts.join(QLatin1Char(';'));
    // Phase E2 tail: peakFontColour (field 31). Invalid QColor means
    // "fall back to m_fontColour" at render time — emit an empty slot
    // to preserve that sentinel across round-trip. Legacy readers stop
    // at field 30 and keep their default invalid m_peakFontColour.
    base += QLatin1Char('|');
    if (m_peakFontColour.isValid()) {
        base += m_peakFontColour.name(QColor::HexArgb);
    }
    // Tail fields 32-35: peakHold | ignoreHistoryMs | powerLimit | strokeWidth.
    // Readers before these stop at field 31.
    base += QLatin1Char('|') + (peakHold() ? QStringLiteral("1") : QStringLiteral("0"));
    base += QLatin1Char('|') + QString::number(m_ignoreHistoryMs);
    base += QLatin1Char('|') + QString::number(m_powerLimit);
    base += QLatin1Char('|') + QString::number(static_cast<double>(m_strokeWidth));
    return base;
}

bool BarItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    // Legacy format: 15 fields (indices 0-14), new format: 20 fields (adds Edge style fields)
    if (parts.size() < 15 || parts[0] != QLatin1String("BAR")) {
        return false;
    }
    if (!parseBaseFields(*this, parts)) {
        return false;
    }

    m_orientation = (parts[7] == QLatin1String("H")) ? Orientation::Horizontal : Orientation::Vertical;

    bool ok = true;
    m_minVal       = parts[8].toDouble(&ok);  if (!ok) { return false; }
    m_maxVal       = parts[9].toDouble(&ok);  if (!ok) { return false; }
    m_barColor     = QColor(parts[10]);       if (!m_barColor.isValid()) { return false; }
    m_barRedColor  = QColor(parts[11]);       if (!m_barRedColor.isValid()) { return false; }
    m_redThreshold = parts[12].toDouble(&ok); if (!ok) { return false; }
    m_attackRatio  = parts[13].toFloat(&ok);  if (!ok) { return false; }
    m_decayRatio   = parts[14].toFloat(&ok);  if (!ok) { return false; }

    // Optional Edge mode fields (new format, 20 fields total)
    // From Thetis console.cs:12612-12678
    if (parts.size() >= 20) {
        // Phase A4: BarStyle is now a 3-way enum (Filled, Edge, Line)
        if (parts[15] == QLatin1String("Edge")) {
            m_barStyle = BarStyle::Edge;
        } else if (parts[15] == QLatin1String("Line")) {
            m_barStyle = BarStyle::Line;
        } else if (parts[15] == QLatin1String("Segments")) {
            m_barStyle = BarStyle::Segments;
        } else if (parts[15] == QLatin1String("Solid")) {
            m_barStyle = BarStyle::Solid;
        } else {
            m_barStyle = BarStyle::Filled;
        }
        QColor ec1 = QColor(parts[16]); if (ec1.isValid()) { m_edgeBgColor   = ec1; }
        QColor ec2 = QColor(parts[17]); if (ec2.isValid()) { m_edgeLowColor  = ec2; }
        QColor ec3 = QColor(parts[18]); if (ec3.isValid()) { m_edgeHighColor = ec3; }
        QColor ec4 = QColor(parts[19]); if (ec4.isValid()) { m_edgeAvgColor  = ec4; }
    }

    // Phase A1 — ShowValue / ShowPeakValue / FontColour (append-only tail)
    if (parts.size() >= 21) { m_showValue     = (parts[20] == QLatin1String("1")); }
    if (parts.size() >= 22) { m_showPeakValue = (parts[21] == QLatin1String("1")); }
    if (parts.size() >= 23) {
        QColor fc = QColor(parts[22]);
        if (fc.isValid()) { m_fontColour = fc; }
    }

    // Phase A2 — ShowHistory / HistoryColour / HistoryDurationMs
    if (parts.size() >= 24) { m_showHistory = (parts[23] == QLatin1String("1")); }
    if (parts.size() >= 25) {
        QColor hc = QColor(parts[24]);
        if (hc.isValid()) { m_historyColour = hc; }
    }
    if (parts.size() >= 26) {
        bool okDur = true;
        int dur = parts[25].toInt(&okDur);
        if (okDur) { m_historyDurationMs = dur; }
    }

    // Phase A3 — ShowMarker / MarkerColour / PeakHoldMarkerColour / PeakHoldDecayRatio
    if (parts.size() >= 27) { m_showMarker = (parts[26] == QLatin1String("1")); }
    if (parts.size() >= 28) {
        QColor mc = QColor(parts[27]);
        if (mc.isValid()) { m_markerColour = mc; }
    }
    if (parts.size() >= 29) {
        QColor pc = QColor(parts[28]);
        if (pc.isValid()) { m_peakHoldMarkerColour = pc; }
    }
    if (parts.size() >= 30) {
        bool okR = true;
        float r = parts[29].toFloat(&okR);
        if (okR) { m_peakHoldDecayRatio = r; }
    }

    // Phase A4 — ScaleCalibration (semicolon-separated value=x pairs).
    if (parts.size() >= 31) {
        m_scaleCalibration.clear();
        const QString& calStr = parts[30];
        if (!calStr.isEmpty()) {
            const QStringList calParts =
                calStr.split(QLatin1Char(';'), Qt::SkipEmptyParts);
            for (const QString& p : calParts) {
                const int eq = p.indexOf(QLatin1Char('='));
                if (eq <= 0) { continue; }
                bool okK = true;
                bool okV = true;
                const double k = p.left(eq).toDouble(&okK);
                const float  v = p.mid(eq + 1).toFloat(&okV);
                if (okK && okV) {
                    m_scaleCalibration.insert(k, v);
                }
            }
        }
    }

    // Phase E2 — PeakFontColour (append-only, field 31). Empty slot
    // means "leave invalid" so peakFontColour() falls back to
    // m_fontColour at render time; a populated hex string restores
    // the explicit override the user picked in the editor.
    if (parts.size() >= 32) {
        const QString& pfcStr = parts[31];
        if (!pfcStr.isEmpty()) {
            QColor pfc(pfcStr);
            if (pfc.isValid()) { m_peakFontColour = pfc; }
        }
    }

    // Fields 32-35. A record without them keeps the legacy look: the
    // peak hold marker follows showMarker, nothing is ignored, the power
    // limit is 100 W (no scaling) and markers are 2 px.
    m_peakHold.reset();
    if (parts.size() >= 33
        && (parts[32] == QLatin1String("1") || parts[32] == QLatin1String("0"))) {
        m_peakHold = (parts[32] == QLatin1String("1"));
    }
    if (parts.size() >= 34) {
        bool okI = true;
        const int ms = parts[33].toInt(&okI);
        if (okI) { setIgnoreHistoryMs(ms); }
    }
    if (parts.size() >= 35) {
        bool okP = true;
        const double watts = parts[34].toDouble(&okP);
        if (okP) { setPowerLimit(watts); }
    }
    if (parts.size() >= 36) {
        bool okS = true;
        const float sw = parts[35].toFloat(&okS);
        if (okS) { setStrokeWidth(sw); }
    }

    return true;
}

// ---------------------------------------------------------------------------
// ScaleItem
// Format: SCALE|x|y|w|h|bindingId|zOrder|orientation|minVal|maxVal|
//               majorTicks|minorTicks|tickColor|labelColor|fontSize
// ---------------------------------------------------------------------------

void ScaleItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    const double range = m_maxVal - m_minVal;

    // Phase B2 — ShowType centered title.
    // From Thetis MeterManager.cs:31879-31886. Thetis draws the title
    // above the scale rect in the container background area. NereusSDR
    // draws it inside the top ~25% of the ScaleItem's own rect so the
    // item owns its title space; presets leave room at the top when
    // building the row (Phase D1).
    if (m_showType) {
        const QString title = readingName(m_bindingId);
        if (!title.isEmpty()) {
            const int titleHeight = qMax(8, rect.height() / 3);
            QFont titleFont = WorkspacePreview::readableFont(p.font(), p.device());
            titleFont.setPixelSize(WorkspacePreview::readableTextPixelSize(titleHeight));
            titleFont.setBold(true);
            p.setFont(titleFont);
            p.setPen(ItemColourLook::effective("Scale.titleColour", m_titleColour, this));
            const QRect titleRect(rect.left(),
                                  rect.top(),
                                  rect.width(),
                                  titleHeight);
            p.drawText(titleRect, Qt::AlignCenter, title);
        }
    }

    // Phase B3 — if the scale is in GeneralScale mode, render the
    // two-tone Thetis-style baseline + ticks instead of the NereusSDR
    // evenly-spaced renderer below. Ported from MeterManager.cs:32338-32423
    // generalScale(). Text labels (per-tick numeric values) are drawn
    // using the existing QFontMetrics-based positioning; colours come
    // from m_lowColour / m_highColour / m_labelColor.
    if (m_scaleStyle == ScaleStyle::GeneralScale) {
        const float rx = static_cast<float>(rect.left());
        const float ry = static_cast<float>(rect.top());
        const float rw = static_cast<float>(rect.width());
        const float rh = static_cast<float>(rect.height());

        // lowToHighPoint — the x-fraction at which the two-tone split
        // happens. Falls back to the even (lowLongTicks-1)/(totalTicks-1)
        // distribution when m_centrePerc < 0 (Thetis default).
        float lowToHighPoint =
            static_cast<float>(m_lowLongTicks - 1)
                / static_cast<float>(m_lowLongTicks + m_highLongTicks - 1);
        if (m_centrePerc >= 0.0f) {
            lowToHighPoint = m_centrePerc;
        }

        // Per-segment spacing. `lowSpacing` is the distance between
        // consecutive long ticks in the low half; `highSpacing` is the
        // same for the high half but measured over (w - lowSpan - w*0.01)
        // so the high baseline stops at x + w*0.99 — matches Thetis.
        const float lowSpacing =
            (rw * lowToHighPoint) / static_cast<float>(m_lowLongTicks - 1);
        const float lowSpan = lowSpacing * static_cast<float>(m_lowLongTicks - 1);
        const float highSpacing =
            ((rw - lowSpan) - (rw * 0.01f))
                / static_cast<float>(m_highLongTicks);

        const float fLineBaseY = ry + (rh * 0.85f);

        // Baseline horizontal line, split at end of low segment.
        p.setPen(QPen(ItemColourLook::effective("Scale.lowColour", m_lowColour, this), 2));
        p.drawLine(QPointF(rx, fLineBaseY),
                   QPointF(rx + lowSpan, fLineBaseY));
        p.setPen(QPen(ItemColourLook::effective("Scale.highColour", m_highColour, this), 2));
        p.drawLine(QPointF(rx + lowSpan, fLineBaseY),
                   QPointF(rx + rw * 0.99f, fLineBaseY));

        // Pick a font for the per-tick labels.
        const int dynFontPx = qMax(m_fontSize, rect.height() / 4);
        QFont font = WorkspacePreview::readableFont(p.font(), p.device());
        font.setPixelSize(WorkspacePreview::readableTextPixelSize(dynFontPx));
        p.setFont(font);
        const QFontMetrics fm(font);

        // Short-row guard: when ShowType eats the top third of the rect
        // and the rect is under ~40 px, shrink the tick row by half so
        // the major-tick tops don't collide with the title text.
        // NereusSDR-original polish — Thetis renders its title outside
        // the scale rect so doesn't hit this case.
        const float tickScale =
            (m_showType && rh < 40.0f) ? 0.5f : 1.0f;
        const float majorTickFrac = 0.30f * tickScale;
        const float minorTickFrac = 0.15f * tickScale;

        auto drawTick = [&](float tickX, bool isMajor, const QColor& colour) {
            const float topY = fLineBaseY
                             - (rh * (isMajor ? majorTickFrac : minorTickFrac));
            p.setPen(QPen(colour, 2));
            p.drawLine(QPointF(tickX, fLineBaseY), QPointF(tickX, topY));
        };

        auto drawLabel = [&](float tickX, const QString& text,
                             const QColor& colour, bool rightAligned) {
            const float topY = fLineBaseY - (rh * majorTickFrac);
            const int w = fm.horizontalAdvance(text);
            const int h = fm.height();
            float lx = rightAligned
                ? (rx + rw - w)
                : (tickX - w / 2.0f);
            p.setPen(colour);
            p.drawText(QPointF(lx, topY - (h - fm.ascent())), text);
        };

        // Low segment ticks + labels.
        for (int i = 1; i < m_lowLongTicks; ++i) {
            // Short tick offset half a space to the left of the long one
            // (Thetis line 32373).
            const float shortX = rx + (static_cast<float>(i) * lowSpacing)
                                    - (lowSpacing * 0.5f);
            drawTick(shortX, /*isMajor*/ false, ItemColourLook::effective("Scale.lowColour", m_lowColour, this));

            const float longX = rx + (static_cast<float>(i) * lowSpacing);
            drawTick(longX, /*isMajor*/ true, ItemColourLook::effective("Scale.lowColour", m_lowColour, this));

            const int val = m_lowStartNumber + i * m_lowIncrement;
            drawLabel(longX, QString::number(val), ItemColourLook::effective("Scale.labelColor", m_labelColor, this),
                      /*rightAligned*/ false);
        }

        // High segment ticks + labels.
        for (int i = 1; i < m_highLongTicks + 1; ++i) {
            const float shortX = rx + lowSpan
                               + (static_cast<float>(i) * highSpacing)
                               - (highSpacing * 0.5f);
            drawTick(shortX, /*isMajor*/ false, ItemColourLook::effective("Scale.highColour", m_highColour, this));

            const float longX = rx + lowSpan
                              + (static_cast<float>(i) * highSpacing);
            drawTick(longX, /*isMajor*/ true, ItemColourLook::effective("Scale.highColour", m_highColour, this));

            const int val = (m_highEndNumber - (m_highLongTicks * m_highIncrement))
                          + i * m_highIncrement;
            const bool isLast = (i == m_highLongTicks);
            drawLabel(longX, QString::number(val), ItemColourLook::effective("Scale.labelColor", m_labelColor, this),
                      /*rightAligned*/ isLast);
        }

        return;
    }

    if (range <= 0.0 || m_majorTicks < 2) {
        return;
    }

    // Dynamic pixel-based font sizing (matches NeedleItem pattern).
    // Labels occupy bottom 2/3 of rect; scale font to fit, m_fontSize is floor.
    const int pixH = rect.height();
    const int dynFontSize = qMax(m_fontSize, pixH / 3);

    QFont font = WorkspacePreview::readableFont(p.font(), p.device());
    font.setPixelSize(WorkspacePreview::readableTextPixelSize(dynFontSize));
    p.setFont(font);

    if (m_orientation == Orientation::Horizontal) {
        // Major ticks with labels — ticks go downward from top, labels below
        for (int i = 0; i < m_majorTicks; ++i) {
            const double frac = static_cast<double>(i) / (m_majorTicks - 1);
            const int x = rect.left() + static_cast<int>(frac * rect.width());

            p.setPen(ItemColourLook::effective("Scale.tickColor", m_tickColor, this));
            p.drawLine(x, rect.top(), x, rect.top() + rect.height() / 3);

            const double val = m_minVal + frac * range;
            const QString label = QString::number(val, 'f', 0);
            p.setPen(ItemColourLook::effective("Scale.labelColor", m_labelColor, this));
            const QFontMetrics fm(font);
            const int labelW = fm.horizontalAdvance(label);
            int labelX = x - labelW / 2;
            if (i == 0 || i == m_majorTicks - 1) {
                // Endpoint ticks stay at their calibrated position. Move only
                // an overflowing endpoint's glyphs; interior labels keep their
                // original centering and the font remains exactly the same.
                const QFontMetricsF inkMetrics(font, p.device());
                const QRectF ink = inkMetrics.tightBoundingRect(label);
                if (i == 0 && labelX + ink.left() < 0) {
                    labelX = static_cast<int>(std::ceil(-ink.left()));
                }
                if (i == m_majorTicks - 1 && labelX + ink.right() > widgetW) {
                    labelX = static_cast<int>(std::floor(widgetW - ink.right()));
                }
            }
            p.drawText(labelX, rect.top() + rect.height() / 3 + fm.ascent(), label);

            // Minor ticks (between major ticks, except after last major)
            if (i < m_majorTicks - 1 && m_minorTicks > 0) {
                const double nextFrac = static_cast<double>(i + 1) / (m_majorTicks - 1);
                for (int j = 1; j <= m_minorTicks; ++j) {
                    const double minorFrac = frac + (nextFrac - frac) * (static_cast<double>(j) / (m_minorTicks + 1));
                    const int mx = rect.left() + static_cast<int>(minorFrac * rect.width());
                    p.setPen(ItemColourLook::effective("Scale.tickColor", m_tickColor, this));
                    p.drawLine(mx, rect.top(), mx, rect.top() + rect.height() / 6);
                }
            }
        }
    } else {
        // Vertical — ticks go left from right edge, labels to the left
        for (int i = 0; i < m_majorTicks; ++i) {
            const double frac = static_cast<double>(i) / (m_majorTicks - 1);
            // Top = maxVal, bottom = minVal for vertical orientation
            const int y = rect.top() + static_cast<int>((1.0 - frac) * rect.height());

            p.setPen(ItemColourLook::effective("Scale.tickColor", m_tickColor, this));
            p.drawLine(rect.right(), y, rect.right() - rect.width() / 3, y);

            const double val = m_minVal + frac * range;
            const QString label = QString::number(val, 'f', 0);
            p.setPen(ItemColourLook::effective("Scale.labelColor", m_labelColor, this));
            const QFontMetrics fm(font);
            const int labelW = fm.horizontalAdvance(label);
            const int labelX = rect.right() - rect.width() / 3 - labelW - 2;
            p.drawText(labelX, y + fm.ascent() / 2, label);

            // Minor ticks
            if (i < m_majorTicks - 1 && m_minorTicks > 0) {
                const double nextFrac = static_cast<double>(i + 1) / (m_majorTicks - 1);
                for (int j = 1; j <= m_minorTicks; ++j) {
                    const double minorFrac = frac + (nextFrac - frac) * (static_cast<double>(j) / (m_minorTicks + 1));
                    const int my = rect.top() + static_cast<int>((1.0 - minorFrac) * rect.height());
                    p.setPen(ItemColourLook::effective("Scale.tickColor", m_tickColor, this));
                    p.drawLine(rect.right(), my, rect.right() - rect.width() / 6, my);
                }
            }
        }
    }
}

QString ScaleItem::serialize() const
{
    QString base = QStringLiteral("SCALE|%1|%2|%3|%4|%5|%6|%7|%8|%9")
        .arg(baseFields(*this))
        .arg(m_orientation == Orientation::Horizontal ? QStringLiteral("H") : QStringLiteral("V"))
        .arg(m_minVal)
        .arg(m_maxVal)
        .arg(m_majorTicks)
        .arg(m_minorTicks)
        .arg(m_tickColor.name(QColor::HexArgb))
        .arg(m_labelColor.name(QColor::HexArgb))
        .arg(m_fontSize);
    // Phase B2 tail (append-only): showType | titleColour
    base += QLatin1Char('|') + (m_showType ? QStringLiteral("1") : QStringLiteral("0"));
    base += QLatin1Char('|') + m_titleColour.name(QColor::HexArgb);
    // Phase B3 tail (append-only): scaleStyle | lowColour | highColour |
    //   lowLongTicks | highLongTicks | lowStartNumber | highEndNumber |
    //   lowIncrement | highIncrement | centrePerc
    base += QLatin1Char('|') + (m_scaleStyle == ScaleStyle::GeneralScale
                                    ? QStringLiteral("GS")
                                    : QStringLiteral("L"));
    base += QLatin1Char('|') + m_lowColour.name(QColor::HexArgb);
    base += QLatin1Char('|') + m_highColour.name(QColor::HexArgb);
    base += QLatin1Char('|') + QString::number(m_lowLongTicks);
    base += QLatin1Char('|') + QString::number(m_highLongTicks);
    base += QLatin1Char('|') + QString::number(m_lowStartNumber);
    base += QLatin1Char('|') + QString::number(m_highEndNumber);
    base += QLatin1Char('|') + QString::number(m_lowIncrement);
    base += QLatin1Char('|') + QString::number(m_highIncrement);
    base += QLatin1Char('|') + QString::number(static_cast<double>(m_centrePerc));
    return base;
}

bool ScaleItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 15 || parts[0] != QLatin1String("SCALE")) {
        return false;
    }
    if (!parseBaseFields(*this, parts)) {
        return false;
    }

    m_orientation = (parts[7] == QLatin1String("H")) ? Orientation::Horizontal : Orientation::Vertical;

    bool ok = true;
    m_minVal     = parts[8].toDouble(&ok);  if (!ok) { return false; }
    m_maxVal     = parts[9].toDouble(&ok);  if (!ok) { return false; }
    m_majorTicks = parts[10].toInt(&ok);    if (!ok) { return false; }
    m_minorTicks = parts[11].toInt(&ok);    if (!ok) { return false; }
    m_tickColor  = QColor(parts[12]);       if (!m_tickColor.isValid()) { return false; }
    m_labelColor = QColor(parts[13]);       if (!m_labelColor.isValid()) { return false; }
    m_fontSize   = parts[14].toInt(&ok);    if (!ok) { return false; }

    // Phase B2 — ShowType / TitleColour (append-only tail)
    if (parts.size() >= 16) { m_showType = (parts[15] == QLatin1String("1")); }
    if (parts.size() >= 17) {
        QColor tc = QColor(parts[16]);
        if (tc.isValid()) { m_titleColour = tc; }
    }

    // Phase B3 — GeneralScale parameters (append-only tail)
    if (parts.size() >= 18) {
        m_scaleStyle = (parts[17] == QLatin1String("GS"))
                           ? ScaleStyle::GeneralScale
                           : ScaleStyle::Linear;
    }
    if (parts.size() >= 19) {
        QColor c = QColor(parts[18]);
        if (c.isValid()) { m_lowColour = c; }
    }
    if (parts.size() >= 20) {
        QColor c = QColor(parts[19]);
        if (c.isValid()) { m_highColour = c; }
    }
    {
        bool ok2 = true;
        if (parts.size() >= 21) { int v = parts[20].toInt(&ok2); if (ok2) { m_lowLongTicks = v; } }
        if (parts.size() >= 22) { int v = parts[21].toInt(&ok2); if (ok2) { m_highLongTicks = v; } }
        if (parts.size() >= 23) { int v = parts[22].toInt(&ok2); if (ok2) { m_lowStartNumber = v; } }
        if (parts.size() >= 24) { int v = parts[23].toInt(&ok2); if (ok2) { m_highEndNumber = v; } }
        if (parts.size() >= 25) { int v = parts[24].toInt(&ok2); if (ok2) { m_lowIncrement = v; } }
        if (parts.size() >= 26) { int v = parts[25].toInt(&ok2); if (ok2) { m_highIncrement = v; } }
        if (parts.size() >= 27) { float v = parts[26].toFloat(&ok2); if (ok2) { m_centrePerc = v; } }
    }

    return true;
}

void ScaleItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    const ScaleItemThetisDefaults& d = kScaleThetisDefaults;
    m_lowColour   = QColor::fromRgb(d.lowColour);
    m_highColour  = QColor::fromRgb(d.highColour);
    m_titleColour = QColor::fromRgb(d.titleColour);
    m_fontSize    = d.fontSize;
}

// ---------------------------------------------------------------------------
// TextItem
// Format: TEXT|x|y|w|h|bindingId|zOrder|label|textColor|fontSize|bold(0/1)|suffix|decimals
//         [|fontFamily|italic(0/1)|centred(0/1)]
// ---------------------------------------------------------------------------

// Thetis ReadingUnits for the readings a NereusSDR binding carries.
// From Thetis MeterManager.cs:2322-2373 [v2.10.3.15] (ReadingUnits)
QString TextItem::readingUnits(int bindingId)
{
    switch (bindingId) {
        case MeterBinding::SignalPeak:     return QStringLiteral("dBm");   // SIGNAL_STRENGTH
        case MeterBinding::SignalAvg:      return QStringLiteral("dBm");   // AVG_SIGNAL_STRENGTH
        case MeterBinding::AdcPeak:        return QStringLiteral("dBFS");  // ADC_PK
        case MeterBinding::AdcAvg:         return QStringLiteral("dBFS");  // ADC_AV
        case MeterBinding::AgcGain:        return QStringLiteral("dB");    // AGC_GAIN
        case MeterBinding::AgcPeak:        return QStringLiteral("dB");    // AGC_PK
        case MeterBinding::AgcAvg:         return QStringLiteral("dB");    // AGC_AV
        case MeterBinding::SignalMaxBin:   return QStringLiteral("dBm");   // SIGNAL_MAX_BIN
        case MeterBinding::PbSnr:          return QStringLiteral("dB");    // ESTIMATED_PBSNR
        case MeterBinding::TxPower:        return QStringLiteral("W");     // PWR
        case MeterBinding::TxReversePower: return QStringLiteral("W");     // REVERSE_PWR
        case MeterBinding::TxSwr:          return QStringLiteral(":1");    // SWR
        case MeterBinding::TxMic:          return QStringLiteral("dB");    // MIC
        case MeterBinding::TxComp:         return QStringLiteral("dB");    // COMP
        case MeterBinding::TxAlc:          return QStringLiteral("dB");    // ALC
        case MeterBinding::TxEq:           return QStringLiteral("dB");    // EQ
        case MeterBinding::TxLeveler:      return QStringLiteral("dB");    // LEVELER
        case MeterBinding::TxLevelerGain:  return QStringLiteral("dB");    // LVL_G
        case MeterBinding::TxAlcGain:      return QStringLiteral("dB");    // ALC_G
        case MeterBinding::TxAlcGroup:     return QStringLiteral("dB");    // ALC_GROUP
        case MeterBinding::TxCfc:          return QStringLiteral("dB");    // CFC_AV
        case MeterBinding::TxCfcGain:      return QStringLiteral("dB");    // CFC_G
        case MeterBinding::TxMicPeak:      return QStringLiteral("dB");    // MIC_PK
        case MeterBinding::TxAlcPeak:      return QStringLiteral("dB");    // ALC_PK
        case MeterBinding::TxCompPeak:     return QStringLiteral("dB");    // COMP_PK
        case MeterBinding::TxEqPeak:       return QStringLiteral("dB");    // EQ_PK
        case MeterBinding::TxLevelerPeak:  return QStringLiteral("dB");    // LEVELER_PK
        case MeterBinding::TxCfcPeak:      return QStringLiteral("dB");    // CFC_PK
        case MeterBinding::HwVolts:        return QStringLiteral("V");     // VOLTS
        case MeterBinding::HwAmps:         return QStringLiteral("A");     // AMPS
        case MeterBinding::RotatorAz:      return QStringLiteral("\u00B0"); // AZ
        case MeterBinding::RotatorEle:     return QStringLiteral("\u00B0"); // ELE
    }
    // NereusSDR: Thetis returns the Reading enum's name for a reading with
    // no unit; a binding with no Thetis reading (temperature) shows none.
    return QString();
}

QString TextItem::displayText() const
{
    // Thetis clsText tokens: %value% and %valueWithType% replace the text
    // with the reading (updateReadingText, Text), %group% with the display
    // group's name (renderText).
    // From Thetis MeterManager.cs:22273-22290, 22305-22318 [v2.10.3.15]
    if (m_label == valueToken() || m_label == valueWithTypeToken()) {
        const QString units = m_label == valueWithTypeToken() ? readingUnits(m_bindingId) : QString();
        // NereusSDR (R-R3-13): no reading shows "--".
        if (isNoReading(m_value)) {
            return QStringLiteral("--") + units;
        }
        return QString::number(m_value, 'f', 1) + units;
    }
    // From Thetis MeterManager.cs:37370-37381 [v2.10.3.15] (renderText)
    if (m_label.toLower() == groupToken()) {
        return m_displayGroupText;
    }

    QString text;
    if (isNoReading(m_value)) {
        // NereusSDR (R-R3-13): no reading shows "--" with the unit the
        // readout shows today.  A signal-level readout in S or uV mode
        // takes formatValue's no-reading form ("--" / "-- µV").
        if (m_suffix == QStringLiteral(" dBm") &&
            (m_unitMode == MeterUnit::S || m_unitMode == MeterUnit::uV))
        {
            text = noReadingText(m_unitMode);
        } else {
            text = QStringLiteral("--") + m_suffix;
        }
    } else if (m_bindingId >= 0 && m_value < m_minValidValue && !m_idleText.isEmpty()) {
        text = m_idleText;
    } else if (m_bindingId >= 0) {
        // Task 3.2 — unit-mode fan-out: when this TextItem carries a signal-
        // level dBm readout (detected by the canonical " dBm" suffix), route
        // through formatValue so the MultimeterPage unit combo drives the
        // display.  Non-signal items (SWR "1.5", voltage "13.3 V", etc.) keep
        // their own suffix/decimals path unchanged.
        if (m_suffix == QStringLiteral(" dBm") &&
            (m_unitMode == MeterUnit::S || m_unitMode == MeterUnit::uV))
        {
            text = formatValue(static_cast<float>(m_value), m_unitMode, m_showDecimal,
                               m_aboveS9Frequency);
        } else {
            // Honor m_showDecimal for dBm mode — matches Thetis
            // chkDisplayMeterShowDecimal [v2.10.3.13]
            const int dec = (m_suffix == QStringLiteral(" dBm") && !m_showDecimal)
                            ? 0 : m_decimals;
            text = QString::number(m_value, 'f', dec) + m_suffix;
        }
    } else {
        text = m_label;
    }
    return text;
}

void TextItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);

    // Dynamic pixel-based font sizing (matches NeedleItem pattern).
    // Scale with rect height, m_fontSize is the minimum floor.
    const int pixH = rect.height();
    const int minimumFontSize = WorkspacePreview::readableTextPixelSize(m_fontSize);
    int dynFontSize = qMax(minimumFontSize, static_cast<int>(pixH * 0.7f));

    QFont font = WorkspacePreview::readableFont(p.font(), p.device());
    if (!m_fontFamily.isEmpty()) {
        font.setFamily(m_fontFamily);
    }
    font.setBold(m_bold);
    font.setItalic(m_italic);

    const QString text = displayText();

    // Shrink font if text overflows rect width
    font.setPixelSize(WorkspacePreview::readableTextPixelSize(dynFontSize));
    while (dynFontSize > minimumFontSize) {
        const QFontMetrics fm(font);
        if (fm.horizontalAdvance(text) <= rect.width()) {
            break;
        }
        --dynFontSize;
        font.setPixelSize(WorkspacePreview::readableTextPixelSize(dynFontSize));
    }

    p.setFont(font);
    p.setPen(ItemColourLook::effective("Text.textColor", m_textColor, this));
    // From Thetis MeterManager.cs:37383-37384 [v2.10.3.15] (renderText):
    // centred about the item's middle, else from its top left.
    const Qt::Alignment align = m_centred ? Qt::Alignment(Qt::AlignCenter)
                                          : (Qt::AlignLeft | Qt::AlignTop);
    p.drawText(rect, align, text);
}

QString TextItem::serialize() const
{
    // Built in steps: QString::arg would substitute %1-style markers that a
    // label or suffix contains.
    QString out = QStringLiteral("TEXT|") + baseFields(*this);
    out += QLatin1Char('|') + m_label;
    out += QLatin1Char('|') + m_textColor.name(QColor::HexArgb);
    out += QLatin1Char('|') + QString::number(m_fontSize);
    out += QLatin1Char('|') + QString::number(m_bold ? 1 : 0);
    out += QLatin1Char('|') + m_suffix;
    out += QLatin1Char('|') + QString::number(m_decimals);
    // Appended tail: fontFamily | italic | centred
    out += QLatin1Char('|') + m_fontFamily;
    out += QLatin1Char('|') + QString::number(m_italic ? 1 : 0);
    out += QLatin1Char('|') + QString::number(m_centred ? 1 : 0);
    return out;
}

bool TextItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 13 || parts[0] != QLatin1String("TEXT")) {
        return false;
    }
    if (!parseBaseFields(*this, parts)) {
        return false;
    }

    m_label     = parts[7];
    m_textColor = QColor(parts[8]);
    if (!m_textColor.isValid()) { return false; }

    bool ok = true;
    m_fontSize  = parts[9].toInt(&ok);   if (!ok) { return false; }
    m_bold      = parts[10].toInt(&ok) != 0; if (!ok) { return false; }
    m_suffix    = parts[11];
    m_decimals  = parts[12].toInt(&ok); if (!ok) { return false; }

    // Appended tail; records without it keep the application font, upright
    // and centred, as they were drawn before.
    m_fontFamily = parts.size() >= 14 ? parts[13] : QString();
    m_italic     = parts.size() >= 15 && parts[14] == QLatin1String("1");
    m_centred    = parts.size() < 16 || parts[15] != QLatin1String("0");

    return true;
}

void TextItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    const TextItemThetisDefaults& d = kTextThetisDefaults;
    m_fontFamily = QString::fromLatin1(d.fontFamily);
    m_bold       = d.bold;
    m_italic     = d.italic;
    m_textColor  = QColor::fromRgb(d.colour);
    m_centred    = d.centred;
    m_fontSize   = d.fontSize;
}

// ---------------------------------------------------------------------------
// NeedleItem — Arc-style S-meter needle
// Ported from AetherSDR src/gui/SMeterWidget.cpp
// ---------------------------------------------------------------------------

NeedleItem::NeedleItem(QObject* parent)
    : MeterItem(parent)
{
}

void NeedleItem::setHistoryDuration(int ms)
{
    m_historyDuration = std::clamp(ms, kMinBarHistoryDurationMs, kMaxBarHistoryDurationMs);
}

void NeedleItem::setIgnoreHistoryMs(int ms)
{
    m_ignoreHistoryMs = std::clamp(ms, kMinBarIgnoreHistoryMs, kMaxBarIgnoreHistoryMs);
}

// The reading interval the history is counted in: the item's update
// interval, or Thetis's 100 ms default for an item that takes every poll.
int NeedleItem::historyIntervalMs() const
{
    return updateIntervalMs() > 0 ? updateIntervalMs() : kDefaultUpdateIntervalMs;
}

void NeedleItem::clearHistory()
{
    // From Thetis MeterManager.cs:22131-22139 [v2.10.3.15] (ClearHistory)
    m_history.clear();
    m_ignoringNext = m_ignoreHistoryMs / historyIntervalMs();  // ignore next N readings to make up 2 second of ignore
}

// From Thetis MeterManager.cs:22109-22130 [v2.10.3.15] (MinHistory, MaxHistory)
float NeedleItem::minHistory() const
{
    if (m_history.isEmpty()) { return m_smoothedDbm; }
    return *std::min_element(m_history.cbegin(), m_history.cend());
}

float NeedleItem::maxHistory() const
{
    if (m_history.isEmpty()) { return m_smoothedDbm; }
    return *std::max_element(m_history.cbegin(), m_history.cend());
}

float NeedleItem::peakValue() const
{
    // Thetis draws the peak hold needle at MaxHistory
    // (MeterManager.cs:40800-40801 [v2.10.3.15]); a saved needle without
    // the setting keeps the AetherSDR decaying peak.
    return m_peakHold.has_value() ? maxHistory() : m_peakDbm;
}

void NeedleItem::setMaxPower(float p)
{
    // From Thetis MeterManager.cs:7132-7139 [v2.10.3.15]: below 0.1 recovers
    // (Thetis takes CurrentPowerRating; NereusSDR takes the setup default).
    // [original inline comment from MeterManager.cs:7137] // incase of 0 recovery from db
    if (!(static_cast<double>(p) >= kMinBarPowerLimitW)) {
        m_maxPower = static_cast<float>(kDefaultBarPowerLimitW);
        return;
    }
    m_maxPower = static_cast<float>(std::min(static_cast<double>(p), kMaxBarPowerLimitW));
}

// From Thetis MeterManager.cs:41047-41057 [v2.10.3.15] (getPerc)
float NeedleItem::scaleValue(float value) const
{
    if (m_bindingId == MeterBinding::SignalPeak || m_bindingId == MeterBinding::SignalAvg) {
        return static_cast<float>(aboveS9Adjusted(m_bindingId, value, m_aboveS9Frequency));
    }
    if (m_normaliseTo100W && m_maxPower > 0.0f) {
        return value * (100.0f / m_maxPower);
    }
    return value;
}

// A calibrated power face reads value * 100 / limit, so the needle travels
// over limit / 100 times the calibration keys before it pins.
float NeedleItem::rawCalibrationMin() const
{
    if (m_scaleCalibration.isEmpty()) { return kS0Dbm; }
    return rawFromScale(m_scaleCalibration.firstKey());
}

float NeedleItem::rawCalibrationMax() const
{
    if (m_scaleCalibration.isEmpty()) { return kMaxDbm; }
    return rawFromScale(m_scaleCalibration.lastKey());
}

// The inverse of the power limit step in scaleValue(). The S9 rule moves a
// signal reading, not the needle's range, so a signal face is unchanged.
float NeedleItem::rawFromScale(float key) const
{
    const bool signal = m_bindingId == MeterBinding::SignalPeak
                     || m_bindingId == MeterBinding::SignalAvg;
    if (!signal && m_normaliseTo100W && m_maxPower > 0.0f) {
        return key * (m_maxPower / 100.0f);
    }
    return key;
}

void NeedleItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    const NeedleItemThetisDefaults& d = kNeedleThetisDefaults;
    m_shadow          = d.shadow;
    m_peakHold        = d.peakHold;
    m_peakHoldColour  = QColor::fromRgb(d.peakHoldColour);
    m_historyEnabled  = d.showHistory;
    m_historyDuration = d.historyDurationMs;
    m_ignoreHistoryMs = d.ignoreHistoryMs;
    m_needleColor     = QColor::fromRgb(d.colour);
    m_strokeWidth     = d.strokeWidth;
    m_attackRatio     = d.attackRatio;
    m_decayRatio      = d.decayRatio;
    m_historyColor    = QColor::fromRgba(d.historyColour);
}

NeedleItem::NeedleGeometry NeedleItem::needleGeometry(int widgetW, int widgetH) const
{
    const QRect rect = meterRect(widgetW, widgetH);
    NeedleGeometry g;
    g.calibrated = !m_scaleCalibration.isEmpty();
    g.left = static_cast<float>(rect.left());
    g.top  = static_cast<float>(rect.top());
    g.pw   = static_cast<float>(rect.width());
    g.ph   = static_cast<float>(rect.height());
    if (g.calibrated) {
        // Pivot = m_needleOffset * rect (normalized to pixels).
        g.pivotX = g.left + g.pw * static_cast<float>(m_needleOffset.x());
        g.pivotY = g.top  + g.ph * static_cast<float>(m_needleOffset.y());
    } else {
        // From AetherSDR: cx = width*0.5, radius = width*0.85
        // cy = height + radius - height*0.65
        g.arcCx = g.left + g.pw * 0.5f;
        g.arcRadius = g.pw * kRadiusRatio;
        g.arcCy = g.top + g.ph + g.arcRadius - g.ph * kCenterYRatio;
        // From AetherSDR: needleCy = height + 6.0f (pivot below widget bottom)
        g.pivotX = g.arcCx;
        g.pivotY = g.top + g.ph + 6.0f;
    }
    return g;
}

QPointF NeedleItem::tipFor(const NeedleGeometry& g, float value) const
{
    if (g.calibrated) {
        // Tip: calibrated normalized position for the value, then apply
        // per-axis radius ratio and length factor as an offset from the
        // pivot. A signal-strength face moves 20 dB at or above the S9
        // frequency; a power face reads against its power limit.
        // From Thetis MeterManager.cs:41047-41057 [v2.10.3.15] (getPerc)
        const QPointF tipNorm = calibratedPosition(scaleValue(value));
        const float dxN = (static_cast<float>(tipNorm.x()) - static_cast<float>(m_needleOffset.x()))
                        * static_cast<float>(m_radiusRatio.x()) * m_lengthFactor;
        const float dyN = (static_cast<float>(tipNorm.y()) - static_cast<float>(m_needleOffset.y()))
                        * static_cast<float>(m_radiusRatio.y()) * m_lengthFactor;
        return QPointF(g.pivotX + g.pw * dxN, g.pivotY + g.ph * dyN);
    }
    const float frac = arcFraction(value);
    const float angleRad = qDegreesToRadians(kArcEndDeg - frac * (kArcEndDeg - kArcStartDeg));
    // From AetherSDR: needle tip at radius + 14
    const float tipDist = g.arcRadius + 14.0f;
    return QPointF(g.arcCx + tipDist * std::cos(angleRad),
                   g.arcCy - tipDist * std::sin(angleRad));
}

// The filled history arc: from the pivot out to the MinHistory tip, along
// the tips to the MaxHistory tip, and back. Thetis sweeps an elliptical arc
// at needle length; NereusSDR follows the needle's own tips, so the arc
// matches a calibrated face's path.
// From Thetis MeterManager.cs:40785-40851 [v2.10.3.15] (renderNeedle)
QVector<QPointF> NeedleItem::historyOutline(const NeedleGeometry& g) const
{
    QVector<QPointF> pts;
    if (!m_historyEnabled || m_history.isEmpty() || m_historyColor.alpha() == 0) {
        return pts;
    }
    constexpr int kSteps = 24;
    const float lo = minHistory();
    const float hi = maxHistory();
    pts.reserve(kSteps + 2);
    pts.append(QPointF(g.pivotX, g.pivotY));
    for (int i = 0; i <= kSteps; ++i) {
        const float v = lo + (hi - lo) * static_cast<float>(i) / static_cast<float>(kSteps);
        pts.append(tipFor(g, v));
    }
    return pts;
}

QPointF NeedleItem::needleTipAt(float value, int widgetW, int widgetH) const
{
    return tipFor(needleGeometry(widgetW, widgetH), value);
}

QPointF NeedleItem::needlePivot(int widgetW, int widgetH) const
{
    const NeedleGeometry g = needleGeometry(widgetW, widgetH);
    return QPointF(g.pivotX, g.pivotY);
}

// From AetherSDR sizeHint 280x140: lock to 2:1 aspect ratio.
// Centers the meter drawing area within the item's pixel rect.
QRect NeedleItem::meterRect(int widgetW, int widgetH) const
{
    // Use the full pixel rect — no aspect-ratio letterboxing.
    // The arc geometry already adapts via effectiveW = min(pw, ph * kTargetAspect)
    // in each paint method, so the needle scales properly while the background
    // and text labels fill the full available width.
    return pixelRect(widgetW, widgetH);
}

bool NeedleItem::participatesIn(Layer layer) const
{
    // NeedleItem renders across all 4 pipeline layers
    Q_UNUSED(layer);
    return true;
}

// From AetherSDR SMeterWidget.cpp setLevel() — SMOOTH_ALPHA = 0.3
void NeedleItem::setValue(double v)
{
    MeterItem::setValue(v);

    // NereusSDR (R-R3-13): no reading.  The needle rests at the scale
    // minimum (S0, or the first calibration point for a calibrated face)
    // and the peak marker clears; the readouts show "--".
    if (isNoReading(v)) {
        m_smoothedDbm = rawCalibrationMin();
        m_peakDbm = m_smoothedDbm;
        m_peakHoldCounter = 0;
        m_peakResetCounter = 0;
        m_history.clear();
        return;
    }

    const float val = static_cast<float>(v);

    // Exponential smoothing with the needle's saved attack and decay
    // ratios; a needle saved without them keeps 0.3 both ways (AetherSDR
    // SMOOTH_ALPHA, kSmoothAlpha).
    // From Thetis MeterManager.cs:22012-22015 [v2.10.3.15] (clsNeedleItem.Update):
    //   if (reading > Value)
    //      Value = (reading * AttackRatio) + (Value * (1f - AttackRatio));
    //   else
    //       Value = (reading * DecayRatio) + (Value * (1f - DecayRatio));
    const float ratio = val > m_smoothedDbm ? m_attackRatio : m_decayRatio;
    m_smoothedDbm = (val * ratio) + (m_smoothedDbm * (1.0f - ratio));

    // Clamp to the appropriate value range. Calibrated needles
    // (ANANMM Volts/Amps/Power/SWR/Compression/ALC) use the
    // calibration map's key range — they're in native units (volts,
    // amps, watts, etc.), NOT dBm. Clamping a 13V reading to the
    // [-127, -13] dBm range mangles it to -13, then
    // calibratedPosition() looks up -13 in a {10, 12.5, 15} map and
    // returns the first calibration point — the symptom was every
    // calibrated needle drawing as a near-horizontal line at the
    // offset row.
    // A power face reads against its power limit, so its range scales too.
    m_smoothedDbm = std::clamp(m_smoothedDbm, rawCalibrationMin(), rawCalibrationMax());

    // Peak tracking (from AetherSDR Medium preset: 500ms hold, 10 dB/sec decay)
    if (m_smoothedDbm > m_peakDbm) {
        m_peakDbm = m_smoothedDbm;
        m_peakHoldCounter = kPeakHoldFrames;
    } else if (m_peakHoldCounter > 0) {
        --m_peakHoldCounter;
    } else {
        m_peakDbm -= kPeakDecayPerFrame;
        if (m_peakDbm < m_smoothedDbm) {
            m_peakDbm = m_smoothedDbm;
        }
    }

    // Hard reset every 10 seconds (from AetherSDR m_peakReset = 10000ms)
    ++m_peakResetCounter;
    if (m_peakResetCounter >= kPeakResetFrames) {
        m_peakDbm = m_smoothedDbm;
        m_peakResetCounter = 0;
    }

    // From Thetis MeterManager.cs:22018-22029 [v2.10.3.15]
    if (m_ignoringNext <= 0) {
        // signal history
        m_history.append(m_smoothedDbm); // adds to end of the list
        // A duration under the interval keeps one reading.
        // From Thetis MeterManager.cs:22106 [v2.10.3.15]
        const int keep = std::max(1, m_historyDuration / historyIntervalMs());
        const int numberToRemove = static_cast<int>(m_history.size()) - keep;
        // the list is sized based on delay
        if (numberToRemove > 0) { m_history.remove(0, numberToRemove); } // remove the oldest, the head of the list
    } else {
        --m_ignoringNext;
    }
}

// From AetherSDR SMeterWidget.cpp dbmToFraction()
// S0-S9 occupies left 60% of arc; S9-S9+60 occupies right 40%
float NeedleItem::dbmToFraction(float dbm) const
{
    const float clamped = std::clamp(dbm, kS0Dbm, kMaxDbm);
    if (clamped <= kS9Dbm) {
        return 0.6f * (clamped - kS0Dbm) / (kS9Dbm - kS0Dbm);
    }
    return 0.6f + 0.4f * (clamped - kS9Dbm) / (kMaxDbm - kS9Dbm);
}

// The built-in arc is an S-unit scale with S9 at kS9Dbm; at or above the S9
// frequency a reading moves 20 dB up it so -93 dBm sits on S9, as the S-unit
// text does (sUnitsText). The tick labels stay where they are.
// From Thetis MeterManager.cs:41049-41052 [v2.10.3.15] (getPerc)
float NeedleItem::arcFraction(float dbm) const
{
    return dbmToFraction(dbm + kS9Dbm - s9Dbm(m_aboveS9Frequency));
}

// Interpolate needle position from scale calibration map.
// From Thetis MeterManager.cs calibration point interpolation.
// Finds the two calibration points bracketing `value` and linearly
// interpolates the (x,y) position between them.
QPointF NeedleItem::calibratedPosition(float value) const
{
    if (m_scaleCalibration.isEmpty()) {
        return QPointF(0.5, 0.5);
    }

    // Use QMap's ordered iteration to find bracketing keys
    auto it = m_scaleCalibration.lowerBound(value);

    // Value is at or beyond the last calibration point
    if (it == m_scaleCalibration.end()) {
        return (--it).value();
    }

    // Value is at or before the first calibration point
    if (it == m_scaleCalibration.begin()) {
        return it.value();
    }

    // Exact match
    if (it.key() == value) {
        return it.value();
    }

    // Interpolate between the two bracketing points
    auto upper = it;
    auto lower = --it;
    const float range = upper.key() - lower.key();
    if (range <= 0.0f) {
        return lower.value();
    }
    const float t = (value - lower.key()) / range;
    const QPointF& p0 = lower.value();
    const QPointF& p1 = upper.value();
    return QPointF(p0.x() + t * (p1.x() - p0.x()),
                   p0.y() + t * (p1.y() - p0.y()));
}

// From AetherSDR SMeterWidget.cpp sUnitsText()
QString NeedleItem::sUnitsText(float dbm) const
{
    // The scale below keeps S9 at kS9Dbm; at or above the S9 frequency the
    // reading moves 20 dB up it so -93 dBm reads S9.
    // From Thetis common.cs:881-887 [v2.10.3.15] (GetSMeterUnits)
    dbm += kS9Dbm - s9Dbm(m_aboveS9Frequency);
    if (dbm <= kS0Dbm) {
        return QStringLiteral("S0");
    }
    if (dbm <= kS9Dbm) {
        const int sUnit = qBound(0, qRound((dbm - kS0Dbm) / kDbPerS), 9);
        return QStringLiteral("S%1").arg(sUnit);
    }
    const int over = qRound(dbm - kS9Dbm);
    return QStringLiteral("S9+%1").arg(over);
}

QString NeedleItem::sUnitsReadout() const
{
    // NereusSDR (R-R3-13): "--" with no reading.
    if (isNoReading(m_value)) {
        return QStringLiteral("--");
    }
    return sUnitsText(m_smoothedDbm);
}

QString NeedleItem::valueReadout() const
{
    if (m_unitMode == MeterUnit::dBm) {
        // NereusSDR (R-R3-13): "-- dBm" with no reading, as the S-meter does.
        if (isNoReading(m_value)) {
            return QStringLiteral("-- dBm");
        }
        return QString::number(static_cast<int>(std::round(m_smoothedDbm)))
               + QStringLiteral(" dBm");
    }
    // S / uV: noReadingText() with no reading, formatValue() otherwise.
    return isNoReading(m_value) ? noReadingText(m_unitMode)
                                : formatValue(m_smoothedDbm, m_unitMode, m_showDecimal,
                                              m_aboveS9Frequency);
}

void NeedleItem::paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    switch (layer) {
    case Layer::Background:     paintBackground(p, widgetW, widgetH); break;
    case Layer::OverlayStatic:  paintOverlayStatic(p, widgetW, widgetH); break;
    case Layer::OverlayDynamic: paintOverlayDynamic(p, widgetW, widgetH); break;
    default: break; // Geometry handled by emitVertices()
    }
    p.restore();
}

void NeedleItem::paint(QPainter& p, int widgetW, int widgetH)
{
    // CPU fallback: render all layers in a single QPainter pass
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    paintBackground(p, widgetW, widgetH);
    paintOverlayStatic(p, widgetW, widgetH);

    // CPU needle: the same history arc, peak marker, shadow and needle as
    // emitVertices(), drawn with QPainter.
    const NeedleGeometry g = needleGeometry(widgetW, widgetH);
    const QPointF pivot(g.pivotX, g.pivotY);
    const QPointF tip = tipFor(g, m_smoothedDbm);

    // History arc. From Thetis MeterManager.cs:40832-40851 [v2.10.3.15]
    const QVector<QPointF> history = historyOutline(g);
    if (history.size() >= 3) {
        p.setPen(Qt::NoPen);
        p.setBrush(ItemColourLook::effective("Needle.historyColor", m_historyColor, this));
        p.drawPolygon(history.constData(), static_cast<int>(history.size()));
        p.setBrush(Qt::NoBrush);
    }

    // Peak hold needle on a calibrated face, under the live needle.
    // From Thetis MeterManager.cs:40885-40910 [v2.10.3.15]
    if (g.calibrated && peakHold()) {
        QPen peakPen(ItemColourLook::effective("Needle.peakHoldColour", m_peakHoldColour, this));
        peakPen.setWidthF(static_cast<double>(m_strokeWidth));
        p.setPen(peakPen);
        p.drawLine(pivot, tipFor(g, peakValue()));
    }

    // Shadow. From Thetis MeterManager.cs:40912-40913 [v2.10.3.15] (if (ni.Shadow))
    if (m_shadow) {
        QPen shadowPen(QColor(0, 0, 0, 80));
        shadowPen.setWidthF(3.0f);
        p.setPen(shadowPen);
        p.drawLine(pivot + QPointF(1.0, 1.0), tip + QPointF(1.0, 1.0));
    }

    // Needle
    QPen needlePen(ItemColourLook::effective("Needle.needleColor", m_needleColor, this));
    needlePen.setWidthF(static_cast<double>(m_strokeWidth));
    p.setPen(needlePen);
    p.drawLine(pivot, tip);

    // Peak marker triangle on the S-meter arc.
    if (!g.calibrated && peakHold() && peakValue() > m_smoothedDbm + 1.0f) {
        const QVector<QPointF> marker = arcPeakMarker(g, peakValue());
        p.setPen(Qt::NoPen);
        p.setBrush(ItemColourLook::effective("Needle.peakHoldColour", m_peakHoldColour, this));
        p.drawPolygon(marker.constData(), static_cast<int>(marker.size()));
        p.setBrush(Qt::NoBrush);
    }

    paintOverlayDynamic(p, widgetW, widgetH);
    p.restore();
}

// From AetherSDR SMeterWidget.cpp paintEvent() — arc drawing section
void NeedleItem::paintBackground(QPainter& p, int widgetW, int widgetH)
{
    // Phase 3G-6 block 2: calibration-driven mode (ANANMM, CrossNeedle)
    // paints the gauge face via a companion ImageItem earlier in the
    // Background layer. Skip the default arc + fillRect entirely so
    // that image is visible.
    if (!m_scaleCalibration.isEmpty()) {
        return;
    }

    const QRect rect = meterRect(widgetW, widgetH);
    const float pw = static_cast<float>(rect.width());
    const float ph = static_cast<float>(rect.height());

    // Background fill — from AetherSDR SMeterWidget.cpp line 197
    p.fillRect(rect, WorkspacePreview::dataBackground(QColor(0x0f, 0x0f, 0x1a), "panel", parent()));

    // From AetherSDR: cx = width*0.5, radius = width*0.85
    // cy = height + radius - height*0.65
    const float cx = rect.left() + pw * 0.5f;
    const float radius = pw * kRadiusRatio;
    const float cy = rect.top() + ph + radius - ph * kCenterYRatio;

    const QRectF arcRect(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // S9 at fraction 0.6 → angle = 125 - 0.6*70 = 83 degrees
    const float arcSpan = kArcEndDeg - kArcStartDeg;  // 70 degrees
    const float s9Frac = 0.6f;
    const float s9Angle = kArcEndDeg - s9Frac * arcSpan;  // 83 degrees

    // AetherSDR uses default cap style (SquareCap) — do not set FlatCap
    QPen arcPen;
    arcPen.setWidthF(3.0f);

    // White segment: S9 angle to ARC_END (left portion, S0-S9)
    // QPainter drawArc: angles in 1/16 degrees, 0=3 o'clock, positive=CCW
    arcPen.setColor(QColor(0xc8, 0xd8, 0xe8));  // From AetherSDR
    p.setPen(arcPen);
    const int whiteStart = static_cast<int>(s9Angle * 16.0f);
    const int whiteSpan  = static_cast<int>((kArcEndDeg - s9Angle) * 16.0f);
    p.drawArc(arcRect, whiteStart, whiteSpan);

    // Red segment: ARC_START to S9 angle (right portion, S9+)
    arcPen.setColor(QColor(0xff, 0x44, 0x44));  // From AetherSDR
    p.setPen(arcPen);
    const int redStart = static_cast<int>(kArcStartDeg * 16.0f);
    const int redSpan  = static_cast<int>((s9Angle - kArcStartDeg) * 16.0f);
    p.drawArc(arcRect, redStart, redSpan);

    // Inner TX arc — from AetherSDR SMeterWidget.cpp lines 236-269
    // Blue arc 6px inside the outer arc (always drawn)
    const float innerR = radius - 6.0f;
    const QRectF innerArc(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);
    arcPen.setColor(QColor(0x00, 0x80, 0xd0));  // From AetherSDR blueColor
    p.setPen(arcPen);
    const int fullStart = static_cast<int>(kArcStartDeg * 16.0f);
    const int fullSpan  = static_cast<int>((kArcEndDeg - kArcStartDeg) * 16.0f);
    p.drawArc(innerArc, fullStart, fullSpan);
}

// Needle geometry: tapered quad from pivot to tip (2 triangles = 6 verts).
// Peak marker: amber triangle at arc edge (3 verts).
// From AetherSDR SMeterWidget.cpp paintEvent() — needle drawing section
//
// Phase 3G-6 block 2: branches on m_scaleCalibration. If non-empty,
// the pivot and tip come from m_needleOffset + calibratedPosition()
// (normalized gauge-face coordinates) with m_radiusRatio applied as
// an elliptical scale around the pivot and m_lengthFactor as a
// radial multiplier. The peak marker is suppressed in this mode —
// it isn't meaningful on a TX power/SWR gauge.
void NeedleItem::emitVertices(QVector<float>& verts, int widgetW, int widgetH)
{
    const NeedleGeometry g = needleGeometry(widgetW, widgetH);
    const bool calibrated = g.calibrated;
    const float pivotX = g.pivotX;
    const float pivotY = g.pivotY;
    const QPointF tip = tipFor(g, m_smoothedDbm);
    const float tipX = static_cast<float>(tip.x());
    const float tipY = static_cast<float>(tip.y());

    // Perpendicular direction for needle width
    const float dx = tipX - pivotX;
    const float dy = tipY - pivotY;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0f) { return; }

    const float perpX = -dy / len;
    const float perpY =  dx / len;

    // From AetherSDR: shadow pen width 3 (halfWidth 1.5), needle pen width 2 (halfWidth 1.0)
    // Uniform width along entire length — NOT tapered (matches QPainter drawLine)
    // The needle's pen is its saved stroke width (Thetis fStrokeWidth =
    // ni.StrokeWidth * fScale, MeterManager.cs:40883 [v2.10.3.15]); a
    // needle saved without one keeps AetherSDR's 2 px.
    const float shadowHW = 1.5f;
    const float needleHW = m_strokeWidth / 2.0f;

    // Pixel to NDC conversion lambdas
    const float invW = 2.0f / static_cast<float>(widgetW);
    const float invH = 2.0f / static_cast<float>(widgetH);
    auto ndcX = [invW](float px) { return px * invW - 1.0f; };
    auto ndcY = [invH](float py) { return 1.0f - py * invH; };
    auto emitVertex = [&](const QPointF& pt, const QColor& c) {
        verts << ndcX(static_cast<float>(pt.x())) << ndcY(static_cast<float>(pt.y()))
              << static_cast<float>(c.redF()) << static_cast<float>(c.greenF())
              << static_cast<float>(c.blueF()) << static_cast<float>(c.alphaF());
    };
    // A line from a to b, halfWidth either side, as two triangles.
    auto emitLine = [&](const QPointF& a, const QPointF& b, float halfWidth, const QColor& c) {
        const float lx = static_cast<float>(b.x() - a.x());
        const float ly = static_cast<float>(b.y() - a.y());
        const float l = std::sqrt(lx * lx + ly * ly);
        if (l < 1.0f) { return; }
        const QPointF n(-ly / l * halfWidth, lx / l * halfWidth);
        emitVertex(a + n, c);
        emitVertex(a - n, c);
        emitVertex(b + n, c);
        emitVertex(b + n, c);
        emitVertex(a - n, c);
        emitVertex(b - n, c);
    };

    // --- History arc (a fan from the pivot) ---
    // From Thetis MeterManager.cs:40832-40851 [v2.10.3.15]
    const QVector<QPointF> history = historyOutline(g);
    const QColor historyColour = ItemColourLook::effective("Needle.historyColor", m_historyColor, this);
    for (int i = 1; i + 1 < history.size(); ++i) {
        emitVertex(history[0], historyColour);
        emitVertex(history[i], historyColour);
        emitVertex(history[i + 1], historyColour);
    }

    // --- Peak hold needle on a calibrated face, under the live needle ---
    // From Thetis MeterManager.cs:40885-40910 [v2.10.3.15]
    if (calibrated && peakHold()) {
        emitLine(QPointF(pivotX, pivotY), tipFor(g, peakValue()), needleHW, ItemColourLook::effective("Needle.peakHoldColour", m_peakHoldColour, this));
    }

    // --- Shadow quad (black, offset +1px) ---
    // From AetherSDR SMeterWidget.cpp line 429: QPen(QColor(0,0,0,80), 3), offset (+1,+1)
    // From Thetis MeterManager.cs:40912-40913 [v2.10.3.15] (if (ni.Shadow))
    if (m_shadow) {
        const float sr = 0.0f, sg = 0.0f, sb = 0.0f, sa = 80.0f / 255.0f;
        const float sOff = 1.0f;
        const float sp1x = pivotX + perpX * shadowHW + sOff, sp1y = pivotY + perpY * shadowHW + sOff;
        const float sp2x = pivotX - perpX * shadowHW + sOff, sp2y = pivotY - perpY * shadowHW + sOff;
        const float sp3x = tipX + perpX * shadowHW + sOff,   sp3y = tipY + perpY * shadowHW + sOff;
        const float sp4x = tipX - perpX * shadowHW + sOff,   sp4y = tipY - perpY * shadowHW + sOff;
        // Triangle 1
        verts << ndcX(sp1x) << ndcY(sp1y) << sr << sg << sb << sa;
        verts << ndcX(sp2x) << ndcY(sp2y) << sr << sg << sb << sa;
        verts << ndcX(sp3x) << ndcY(sp3y) << sr << sg << sb << sa;
        // Triangle 2
        verts << ndcX(sp3x) << ndcY(sp3y) << sr << sg << sb << sa;
        verts << ndcX(sp2x) << ndcY(sp2y) << sr << sg << sb << sa;
        verts << ndcX(sp4x) << ndcY(sp4y) << sr << sg << sb << sa;
    }

    // --- Needle quad (color from m_needleColor, uniform width) ---
    // From AetherSDR SMeterWidget.cpp line 433: QPen(#ffffff, 2).
    // Phase 3G-6 block 2: honour per-needle color from the factory.
    const QColor needleColour = ItemColourLook::effective("Needle.needleColor", m_needleColor, this);
    const float nr = static_cast<float>(needleColour.redF());
    const float ng = static_cast<float>(needleColour.greenF());
    const float nb = static_cast<float>(needleColour.blueF());
    const float na = static_cast<float>(needleColour.alphaF());
    const float np1x = pivotX + perpX * needleHW, np1y = pivotY + perpY * needleHW;
    const float np2x = pivotX - perpX * needleHW, np2y = pivotY - perpY * needleHW;
    const float np3x = tipX + perpX * needleHW,   np3y = tipY + perpY * needleHW;
    const float np4x = tipX - perpX * needleHW,   np4y = tipY - perpY * needleHW;
    // Triangle 1
    verts << ndcX(np1x) << ndcY(np1y) << nr << ng << nb << na;
    verts << ndcX(np2x) << ndcY(np2y) << nr << ng << nb << na;
    verts << ndcX(np3x) << ndcY(np3y) << nr << ng << nb << na;
    // Triangle 2
    verts << ndcX(np3x) << ndcY(np3y) << nr << ng << nb << na;
    verts << ndcX(np2x) << ndcY(np2y) << nr << ng << nb << na;
    verts << ndcX(np4x) << ndcY(np4y) << nr << ng << nb << na;

    // --- Peak marker triangle (points INWARD toward center) ---
    // From AetherSDR SMeterWidget.cpp lines 437-462: tip at radius-2, base 6px behind.
    // Phase 3G-6 block 2: calibration mode (TX power/SWR/etc.) draws the
    // Thetis peak hold needle above instead of the triangle.
    // NereusSDR draws it in the peak hold colour (saved needles: #ffaa00).
    if (!calibrated && peakHold() && peakValue() > m_smoothedDbm + 1.0f) {
        const QVector<QPointF> marker = arcPeakMarker(g, peakValue());
        const QColor peakColour = ItemColourLook::effective("Needle.peakHoldColour", m_peakHoldColour, this);
        for (const QPointF& pt : marker) {
            emitVertex(pt, peakColour);
        }
    }
}

// The S-meter arc's peak marker: an inward-pointing triangle at the value.
// From AetherSDR SMeterWidget.cpp lines 437-462: tip at radius-2, base 6px behind.
QVector<QPointF> NeedleItem::arcPeakMarker(const NeedleGeometry& g, float value) const
{
    const float peakFrac = arcFraction(value);
    const float peakAngleDeg = kArcEndDeg - peakFrac * (kArcEndDeg - kArcStartDeg);
    const float peakAngleRad = qDegreesToRadians(peakAngleDeg);

    const float cosA = std::cos(peakAngleRad);
    const float sinA = std::sin(peakAngleRad);

    // Tip at radius-2 (inside the arc), base 6px behind toward center
    const float markerTipR = g.arcRadius - 2.0f;
    const float markerBaseR = g.arcRadius - 8.0f;
    const float markerHalf = 3.0f;

    // Tip (inward, on arc inner edge)
    const float mtx = g.arcCx + markerTipR * cosA;
    const float mty = g.arcCy - markerTipR * sinA;
    // Base center (further inward)
    const float mbcx = g.arcCx + markerBaseR * cosA;
    const float mbcy = g.arcCy - markerBaseR * sinA;
    // Perpendicular at base — from AetherSDR: perpCos = -sinA, perpSin = cosA
    const float mpx = -sinA * markerHalf;
    const float mpy =  cosA * markerHalf;

    return {QPointF(mtx, mty), QPointF(mbcx + mpx, mbcy + mpy), QPointF(mbcx - mpx, mbcy - mpy)};
}

// From AetherSDR SMeterWidget.cpp paintEvent() — tick drawing section
void NeedleItem::paintOverlayStatic(QPainter& p, int widgetW, int widgetH)
{
    // Phase 3G-6 block 2: calibration-driven mode gets its scale ticks
    // and source labels from the companion ImageItem gauge face /
    // NeedleScalePwrItem. Skip the built-in S-meter ticks + source
    // label entirely.
    if (!m_scaleCalibration.isEmpty()) {
        return;
    }

    const QRect rect = meterRect(widgetW, widgetH);
    const float pw = static_cast<float>(rect.width());
    const float ph = static_cast<float>(rect.height());
    const float cx = rect.left() + pw * 0.5f;
    const float radius = pw * kRadiusRatio;
    const float cy = rect.top() + ph + radius - ph * kCenterYRatio;

    // Tick definitions from AetherSDR SMeterWidget.cpp lines 340-347
    // Only odd S-units + over-S9 marks shown. BARE numbers (no "S" prefix).
    struct TickDef { float dbm; const char* label; bool isRed; };
    static const TickDef ticks[] = {
        {-121.0f, "1",   false},  // From AetherSDR: QString::number(s)
        {-109.0f, "3",   false},
        { -97.0f, "5",   false},
        { -85.0f, "7",   false},
        { -73.0f, "9",   false},
        { -53.0f, "+20", true},   // From AetherSDR: QString("+%1").arg(over)
        { -33.0f, "+40", true},
    };

    // Needle pivot for needleDir calculation
    // From AetherSDR SMeterWidget.cpp line 279-286: direction from (cx, needleCy) through arc point
    const float pivotY = rect.top() + ph + 6.0f;

    // From AetherSDR: tick font = max(10, height/10) px, bold
    QFont tickFont = WorkspacePreview::readableFont(p.font(), p.device());
    const int tickFontSize = qMax(10, static_cast<int>(ph) / 10);
    tickFont.setPixelSize(WorkspacePreview::readableTextPixelSize(tickFontSize));
    tickFont.setBold(true);
    p.setFont(tickFont);
    const QFontMetrics tfm(tickFont);

    for (const auto& tick : ticks) {
        const float frac = dbmToFraction(tick.dbm);
        const float angleDeg = kArcEndDeg - frac * (kArcEndDeg - kArcStartDeg);
        const float angleRad = qDegreesToRadians(angleDeg);

        // Arc point
        const float arcPtX = cx + radius * std::cos(angleRad);
        const float arcPtY = cy - radius * std::sin(angleRad);

        // needleDir: direction from pivot (cx, pivotY) through arc point, normalized
        // From AetherSDR SMeterWidget.cpp lines 279-286
        const float ndx = arcPtX - cx;
        const float ndy = arcPtY - pivotY;
        const float ndLen = std::sqrt(ndx * ndx + ndy * ndy);
        const float ux = ndx / ndLen;
        const float uy = ndy / ndLen;

        // From AetherSDR: tick starts 2px outside arc, extends 14px outward (both outside)
        // SMeterWidget.cpp lines 296-297
        const QPointF inner(arcPtX + 2.0f * ux, arcPtY + 2.0f * uy);
        const QPointF outer(arcPtX + 14.0f * ux, arcPtY + 14.0f * uy);

        const QColor tickColor = tick.isRed ? QColor(0xff, 0x44, 0x44)
                                            : QColor(0xc8, 0xd8, 0xe8);
        QPen tickPen(tickColor);
        tickPen.setWidthF(1.5f);
        p.setPen(tickPen);
        p.drawLine(inner, outer);

        // From AetherSDR: label 26px from arc point along needleDir
        // SMeterWidget.cpp line 303
        const QPointF labelPt(arcPtX + 26.0f * ux, arcPtY + 26.0f * uy);
        p.setPen(tickColor);
        const QString labelStr = QString::fromLatin1(tick.label);
        const int tw = tfm.horizontalAdvance(labelStr);
        p.drawText(QPointF(labelPt.x() - tw / 2.0f,
                            labelPt.y() + tfm.ascent() / 2.0f), labelStr);
    }

    // From AetherSDR: source label centered, max(9, h/14) px, non-bold, #8090a0
    QFont srcFont = WorkspacePreview::readableFont(p.font(), p.device());
    const int srcFontSize = qMax(9, static_cast<int>(ph) / 14);
    srcFont.setPixelSize(WorkspacePreview::readableTextPixelSize(srcFontSize));
    srcFont.setBold(false);
    p.setFont(srcFont);
    p.setPen(QColor(0x80, 0x90, 0xa0));
    const QFontMetrics srcFm(srcFont);
    const int srcW = srcFm.horizontalAdvance(m_sourceLabel);
    p.drawText(QPointF(rect.left() + (pw - srcW) / 2.0f,
                        rect.top() + srcFm.height() + 2.0f), m_sourceLabel);
}

// From AetherSDR SMeterWidget.cpp paintEvent() — readout drawing section
void NeedleItem::paintOverlayDynamic(QPainter& p, int widgetW, int widgetH)
{
    // Phase 3G-6 block 2: calibration-driven mode has no S-unit / dBm
    // readouts — the gauge face provides the reference frame visually.
    if (!m_scaleCalibration.isEmpty()) {
        return;
    }

    const QRect rect = meterRect(widgetW, widgetH);
    const float ph = static_cast<float>(rect.height());

    // Compute source label baseline for vertical positioning
    const int srcFontSize = qMax(9, static_cast<int>(ph) / 14);
    QFont srcFont = WorkspacePreview::readableFont(p.font(), p.device());
    srcFont.setPixelSize(WorkspacePreview::readableTextPixelSize(srcFontSize));
    const QFontMetrics srcFm(srcFont);
    const float topY = rect.top() + srcFm.height() + 2.0f;

    // From AetherSDR: value font = max(13, h/8) px, bold
    QFont valFont = WorkspacePreview::readableFont(p.font(), p.device());
    const int valFontSize = qMax(13, static_cast<int>(ph) / 8);
    valFont.setPixelSize(WorkspacePreview::readableTextPixelSize(valFontSize));
    valFont.setBold(true);
    p.setFont(valFont);
    const QFontMetrics valFm(valFont);

    // S-unit text (left, cyan) — from AetherSDR SMeterWidget.cpp line 533
    // AetherSDR uses drawText(x, topY, text) where topY is the baseline.
    // QPointF overload also treats Y as baseline — pass topY directly, no ascent offset.
    const QString sText = sUnitsReadout();
    p.setPen(QColor(0x00, 0xb4, 0xd8));
    p.drawText(QPointF(rect.left() + 6.0f, topY), sText);

    // Right-side readout (light steel) — from AetherSDR SMeterWidget.cpp line 537
    // Task 3.2: honor m_unitMode so the MultimeterPage unit combo drives this
    // secondary readout.  dBm (default): integer dBm string as before.
    // S / uV: route through formatValue.
    const QString dbmText = valueReadout();
    p.setPen(QColor(0xc8, 0xd8, 0xe8));
    const int dbmW = valFm.horizontalAdvance(dbmText);
    p.drawText(QPointF(rect.right() - dbmW - 6.0f, topY), dbmText);
}

// Format:
//   NEEDLE|x|y|w|h|bindingId|zOrder|sourceLabel
//     [|onlyRx|onlyTx|displayGroup]                    -- added block 1
//     [|calCount|v:x:y|v:x:y|...]                      -- added block 2
//     [|needleColor|offsetX|offsetY|radX|radY|lenFactor|direction|normalise100|maxPower|attack|decay]
//                                                      -- added block 2
//     [|shadow|peakHold|peakHoldColour|showHistory|historyMs|historyColour|ignoreHistoryMs|strokeWidth]
//                                                      -- container item parity
//       (peakHold empty: unset, the saved needle's own marker)
// Each trailing group is optional for backward compatibility: older
// serialized state deserializes cleanly with defaults for any missing
// tail fields.
QString NeedleItem::serialize() const
{
    // Joined, not chained .arg(): a source label holding "%1" must stay as
    // written.
    QString s = QStringList{QStringLiteral("NEEDLE"), baseFields(*this), m_sourceLabel,
                            QString::number(m_onlyWhenRx ? 1 : 0),
                            QString::number(m_onlyWhenTx ? 1 : 0),
                            QString::number(m_displayGroup)}
                    .join(QLatin1Char('|'));

    // Calibration map — value:x:y triplets with a leading count.
    s += QStringLiteral("|%1").arg(m_scaleCalibration.size());
    for (auto it = m_scaleCalibration.constBegin();
         it != m_scaleCalibration.constEnd(); ++it) {
        s += QStringLiteral("|%1:%2:%3")
            .arg(static_cast<double>(it.key()))
            .arg(it.value().x())
            .arg(it.value().y());
    }

    // Visual / geometry / smoothing extras.
    s += QStringLiteral("|%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11")
        .arg(m_needleColor.name(QColor::HexArgb))
        .arg(m_needleOffset.x())
        .arg(m_needleOffset.y())
        .arg(m_radiusRatio.x())
        .arg(m_radiusRatio.y())
        .arg(static_cast<double>(m_lengthFactor))
        .arg(static_cast<int>(m_direction))
        .arg(m_normaliseTo100W ? 1 : 0)
        .arg(static_cast<double>(m_maxPower))
        .arg(static_cast<double>(m_attackRatio))
        .arg(static_cast<double>(m_decayRatio));

    // Shadow, peak hold and history.
    s += QStringLiteral("|%1|%2|%3|%4|%5|%6|%7|%8")
        .arg(m_shadow ? 1 : 0)
        .arg(m_peakHold.has_value() ? QString::number(*m_peakHold ? 1 : 0) : QString())
        .arg(m_peakHoldColour.name(QColor::HexArgb))
        .arg(m_historyEnabled ? 1 : 0)
        .arg(m_historyDuration)
        .arg(m_historyColor.name(QColor::HexArgb))
        .arg(m_ignoreHistoryMs)
        .arg(static_cast<double>(m_strokeWidth));

    return s;
}

bool NeedleItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 8 || parts[0] != QLatin1String("NEEDLE")) {
        return false;
    }
    if (!parseBaseFields(*this, parts)) {
        return false;
    }
    m_sourceLabel = parts[7];

    // Filter fields (block 1)
    int idx = 8;
    if (parts.size() >= idx + 3) {
        m_onlyWhenRx   = (parts[idx++].toInt() != 0);
        m_onlyWhenTx   = (parts[idx++].toInt() != 0);
        m_displayGroup = parts[idx++].toInt();
    }

    // Calibration map (block 2)
    if (parts.size() > idx) {
        bool ok = false;
        const int calCount = parts[idx].toInt(&ok);
        if (ok && calCount >= 0 && parts.size() >= idx + 1 + calCount) {
            ++idx;
            QMap<float, QPointF> cal;
            for (int i = 0; i < calCount; ++i) {
                const QStringList tri = parts[idx + i].split(QLatin1Char(':'));
                if (tri.size() < 3) { cal.clear(); break; }
                bool okv = true, okx = true, oky = true;
                const float v  = tri[0].toFloat(&okv);
                const float nx = tri[1].toFloat(&okx);
                const float ny = tri[2].toFloat(&oky);
                if (!okv || !okx || !oky) { cal.clear(); break; }
                cal.insert(v, QPointF(nx, ny));
            }
            m_scaleCalibration = cal;
            idx += calCount;
        }
    }

    // Visual / geometry / smoothing extras (block 2)
    if (parts.size() >= idx + 11) {
        const QColor col(parts[idx]);
        if (col.isValid()) { m_needleColor = col; }
        m_needleOffset    = QPointF(parts[idx + 1].toDouble(), parts[idx + 2].toDouble());
        m_radiusRatio     = QPointF(parts[idx + 3].toDouble(), parts[idx + 4].toDouble());
        m_lengthFactor    = parts[idx + 5].toFloat();
        m_direction       = static_cast<NeedleDirection>(parts[idx + 6].toInt());
        m_normaliseTo100W = (parts[idx + 7].toInt() != 0);
        setMaxPower(parts[idx + 8].toFloat());
        m_attackRatio     = parts[idx + 9].toFloat();
        m_decayRatio      = parts[idx + 10].toFloat();
        idx += 11;
    }

    // Shadow, peak hold and history. A record without them keeps the
    // saved look: shadow on, the needle's own peak marker, no history.
    m_peakHold.reset();
    if (parts.size() >= idx + 8) {
        m_shadow = (parts[idx] != QLatin1String("0"));
        if (parts[idx + 1] == QLatin1String("1") || parts[idx + 1] == QLatin1String("0")) {
            m_peakHold = (parts[idx + 1] == QLatin1String("1"));
        }
        const QColor peakColour(parts[idx + 2]);
        if (peakColour.isValid()) { m_peakHoldColour = peakColour; }
        m_historyEnabled = (parts[idx + 3] == QLatin1String("1"));
        bool okH = false;
        const int historyMs = parts[idx + 4].toInt(&okH);
        if (okH) { setHistoryDuration(historyMs); }
        const QColor historyColour(parts[idx + 5]);
        if (historyColour.isValid()) { m_historyColor = historyColour; }
        bool okI = false;
        const int ignoreMs = parts[idx + 6].toInt(&okI);
        if (okI) { setIgnoreHistoryMs(ignoreMs); }
        bool okS = false;
        const float stroke = parts[idx + 7].toFloat(&okS);
        if (okS && stroke > 0.0f) { m_strokeWidth = stroke; }
    }

    return true;
}

} // namespace NereusSDR
