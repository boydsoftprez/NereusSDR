#pragma once

// =================================================================
// src/gui/AboutDialog.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source (pairs with src/gui/AboutDialog.cpp):
//   Project Files/Source/Console/frmAbout.cs — original licence
//     from Thetis source is included below.
//   Project Files/Source/Console/frmAbout.Designer.cs:57-81 —
//     referenced by AboutDialog.cpp's kRoster verbatim reproduction.
//     Upstream Designer.cs has no top-of-file GPL header — paired
//     frmAbout.cs header (below) governs.
//
// Thetis version pin: [@501e3f5].
//
// Design and layout patterns from AetherSDR (ten9876/AetherSDR,
//   GPLv3): src/gui/MainWindow.cpp (about-box section) and
//   src/gui/TitleBar.{h,cpp}. AetherSDR has no per-file headers;
//   project-level citation per HOW-TO-PORT.md rule 6.
//
// --- From Thetis Project Files/Source/Console/frmAbout.cs (verbatim header) ---
/*  frmAbout.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2025 Richard Samphire MW0LGE

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
// --- End Thetis frmAbout.cs verbatim header ---
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Implemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted authoring via Anthropic
//                 Claude Code. Contributor list, copyright string,
//                 and §5(d) notice content are NereusSDR-specific.
//   2026-04-18 — Brought lineage forward to full Thetis-roster parity:
//                 scrollable contributor list reproducing
//                 Thetis/frmAbout.Designer.cs:57-81 verbatim, Links
//                 section, revised licence text reflecting v2-or-later
//                 headers + v3 distribution, AI-assisted-port
//                 disclosure. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-04-18 — Promoted to derived-file status: added Thetis port
//                 citation + frmAbout.cs verbatim header + [@501e3f5]
//                 version pin to pair with AboutDialog.cpp. Replaces
//                 the earlier no-port-check exemption. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QDialog>

namespace NereusSDR {

class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);

private:
    void buildUI();
};

} // namespace NereusSDR
