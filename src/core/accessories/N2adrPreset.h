#pragma once

// =================================================================
// src/core/accessories/N2adrPreset.h  (NereusSDR)
// =================================================================
//
// Ported from mi0bot-Thetis source:
//   Project Files/Source/Console/setup.cs:chkHERCULES_CheckedChanged
//   (mi0bot v2.10.3.13-beta2 / @c26a8a4, lines 14311-14424 — HERMESLITE
//   branch lines 14324-14368)
//
// Single source of truth for the N2ADR Filter preset that populates the
// shared OcMatrix when the user enables the "N2ADR Filter board" toggle
// in Hl2IoBoardTab.  Two callers (Hl2IoBoardTab::onN2adrToggled and
// RadioModel::handleConnect's app-launch reconcile) used to carry
// duplicate per-band tables; this helper centralises them so future
// upstream resyncs only touch one place.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — Extracted from Hl2IoBoardTab.cpp:950-995 + RadioModel.cpp
//                :1077-1111 to centralise the per-band write table.  Adds
//                the 13 SWL bands × pin-7 RX entries (mi0bot setup.cs
//                :14346-14358) that were previously missing — closes the
//                Phase 3L HL2 N2ADR visibility gap (the SWL OC matrix sub-
//                tab was a placeholder until this commit).
//                J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                Anthropic Claude Code.
//   2026-09-24 - R-R3-46: applyN2adrPresetReceiveOnly, the preset's
//                receive half for a station that may not change transmit
//                settings. NereusSDR-original. J.J. Boyd (KG4VCF), AI-
//                assisted via Anthropic Claude Code.
// =================================================================
//
//=================================================================
// setup.cs (mi0bot fork)
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
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves his       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

namespace NereusSDR {

class OcMatrix;

// Apply the N2ADR Filter preset to `oc`.
//
//   enabled = true:   wipes every band/pin/{rx,tx} cell, then populates
//                     the per-band pattern that matches the N2ADR filter
//                     wiring (10 ham bands + 13 SWL bands, RX pin-7
//                     auto-fill).  Mirrors mi0bot setup.cs:14315-14368
//                     (case true).
//
//   enabled = false:  wipes every cell.  Mirrors mi0bot setup.cs
//                     :14414-14422 (case false).
//
// In both branches the wipe destroys any other manual OC pin
// configuration the user may have made — this matches mi0bot's "N2ADR
// owns the OC matrix while enabled" model exactly.
//
// Caller is responsible for OcMatrix::save() afterwards if persistence
// is required (Hl2IoBoardTab does; the codec layer does not).
void applyN2adrPreset(OcMatrix& oc, bool enabled);

// R-R3-46 (NereusSDR-original): the preset's receive half only. The receive
// pins are cleared and filled as applyN2adrPreset does; every transmit pin
// keeps its current value. For a receive-only station (a Core, or a remote
// window, without the transmit permission), whose transmit OC pins the
// operator may not change from there.
void applyN2adrPresetReceiveOnly(OcMatrix& oc, bool enabled);

} // namespace NereusSDR
