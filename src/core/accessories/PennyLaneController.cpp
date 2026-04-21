// =================================================================
// src/core/accessories/PennyLaneController.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/HPSDR/Penny.cs (full file)
//   Project Files/Source/Console/console.cs:14899 (PennyExtCtrlEnabled property)
//   Project Files/Source/Console/setup.cs:12679+ (chkPennyExtCtrl_CheckedChanged)
//
// Note: The per-band OC bitmask arrays (TXABitMasks/RXABitMasks, setBandABitMask /
// setBandBBitMask, etc.) and the per-pin TX action mapping (TXPinAction[][],
// setTXPinAction, setRXPinPA, setTXPinPA) were already ported in Phase 3P-D Task 1
// as OcMatrix. This controller wraps OcMatrix and adds the master ext-ctrl toggle.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Thin wrapper: master extCtrlEnabled toggle +
//                per-MAC persistence. OC bitmask logic lives in OcMatrix
//                (Phase 3P-D Task 1). Setup UI deferred to a follow-up if
//                Penny owners request it.
// =================================================================
//
// === Verbatim Thetis Console/HPSDR/Penny.cs header (lines 1-22) ===
/*
*
* Copyright (C) 2008 Bill Tracey, KD5TFD, bill@ewjt.com
* Copyright (C) 2010-2020  Doug Wigley
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
//
// this module contains code to support the Penelope Transmitter board
//
// --- From console.cs ---
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
//
// =================================================================

#include "PennyLaneController.h"
#include "core/AppSettings.h"

namespace NereusSDR {

// Source: console.cs:14899 "private bool penny_ext_ctrl_enabled = true;" [@501e3f5]
// Default true — matches Thetis initialization.
PennyLaneController::PennyLaneController(QObject* parent) : QObject(parent)
{
    // m_extCtrlEnabled initialized to true in header (matches Thetis default)
}

// Source: console.cs:14899 PennyExtCtrlEnabled getter [@501e3f5]
// "get { return penny_ext_ctrl_enabled; }"
bool PennyLaneController::extCtrlEnabled() const { return m_extCtrlEnabled; }

// Source: console.cs:14899 PennyExtCtrlEnabled setter [@501e3f5]
// "penny_ext_ctrl_enabled = value;
//  if (!initializing) {
//    Band lo_band = BandByFreq(XVTRForm.TranslateFreq(VFOAFreq), rx1_xvtr_index, current_region);
//    Band lo_bandb = BandByFreq(XVTRForm.TranslateFreq(VFOBFreq), rx2_xvtr_index, current_region);
//    int bits = Penny.getPenny().ExtCtrlEnable(lo_band, lo_bandb, _mox, value, _tuning, SetupForm.TestIMD, chkExternalPA.Checked); // MW0LGE_21j
//    if (!IsSetupFormNull) SetupForm.UpdateOCLedStrip(_mox, bits);
//  }"
// NereusSDR: protocol wire-up (Penny.ExtCtrlEnable / NetworkIO.SetOCBits) and
// LED strip update deferred to the codec layer.
// Inline comment "// need side effect of this to push change to native code"
// [setup.cs:12679 — repeated for each chkPenOCrcv/chkPenOCxmit handler] [@501e3f5]
void PennyLaneController::setExtCtrlEnabled(bool on)
{
    if (m_extCtrlEnabled == on) { return; }
    m_extCtrlEnabled = on;
    emit extCtrlEnabledChanged(on);
}

void PennyLaneController::setMacAddress(const QString& mac) { m_mac = mac; }

QString PennyLaneController::persistenceKey() const
{
    return QStringLiteral("hardware/%1/penny").arg(m_mac);
}

void PennyLaneController::load()
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();
    const QString base = persistenceKey();
    // Default "True" matches Thetis penny_ext_ctrl_enabled = true [console.cs:14899] [@501e3f5]
    m_extCtrlEnabled = (s.value(QStringLiteral("%1/extCtrlEnabled").arg(base), QStringLiteral("True")).toString() == QStringLiteral("True"));
    emit extCtrlEnabledChanged(m_extCtrlEnabled);
}

void PennyLaneController::save()
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();
    const QString base = persistenceKey();
    s.setValue(QStringLiteral("%1/extCtrlEnabled").arg(base), m_extCtrlEnabled ? QStringLiteral("True") : QStringLiteral("False"));
}

} // namespace NereusSDR
