# Upstream headers for Setup descriptions

The JSON files in this directory carry UI text and ranges from the desktop's Thetis-derived Setup pages. This file preserves the upstream licence header for `setup.cs` verbatim. Thetis `setup.designer.cs` has no top-of-file copyright or permission header; its project-level licence applies.

Ported from Thetis `Project Files/Source/Console/setup.cs` and
`setup.designer.cs` at v2.10.3.15 (3759d09).

Sources: Thetis v2.10.3.15 (3759d09), `Project Files/Source/Console/setup.cs` and `setup.designer.cs`.

Covered JSON files: `general.json`, `hardware.json`, `pa.json`, `test.json`, `diagnostics.json`, `catNetwork.json`, `dsp.json`, `display.json`, `appearance.json`, `transmit.json`, `audio.json`.

## setup.cs — verbatim upstream header

````text
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


````

## Modification history (NereusSDR)

2026-09-27 - J.J. Boyd (KG4VCF), with AI-assisted transformation via OpenAI Codex.
2026-09-28 - J.J. Boyd (KG4VCF), with AI-assisted transformation via OpenAI Codex: Hardware Antenna/ALEX scalar descriptions.
2026-09-28 - J.J. Boyd (KG4VCF), with AI-assisted transformation via OpenAI Codex: model-aware Antenna/ALEX relay labels and visibility.
2026-09-28 - J.J. Boyd (KG4VCF), with AI-assisted transformation via OpenAI Codex: PA Values readout descriptions.
2026-09-28 - J.J. Boyd (KG4VCF), with AI-assisted transformation via OpenAI Codex: ANAN-G2E PA bypass description.
2026-09-28 - J.J. Boyd (KG4VCF), with AI-assisted transformation via OpenAI Codex: partial Display spectrum, meter, and TX analyzer settings description.
2026-09-28 - J.J. Boyd (KG4VCF), with AI-assisted transformation via OpenAI Codex: 14-band TX and RX antenna table captions and tooltips, plus SKU-specific RX-only labels.
2026-09-28 - J.J. Boyd (KG4VCF), with AI-assisted transformation via OpenAI Codex: PA Current and DC Voltage telemetry readout descriptions.
2026-09-28 - J.J. Boyd (KG4VCF), with AI-assisted transformation via OpenAI Codex: partial Appearance Colors & Theme swatch description.
2026-09-28 - J.J. Boyd (KG4VCF), with AI-assisted transformation via OpenAI Codex: PA raw-power and forward/reverse voltage readout descriptions using existing native PA Values labels.
2026-09-28 - J.J. Boyd (KG4VCF), with AI-assisted transformation via OpenAI Codex: Meter Styles S-meter face, peak hold, and decay descriptions from the built native page.
2026-09-28 - J.J. Boyd (KG4VCF), with AI-assisted transformation via OpenAI Codex: RX spectrum and waterfall detector, averaging, time, and decimation descriptions from the built native pages.
2026-09-28 - J.J. Boyd (KG4VCF), with AI-assisted transformation via OpenAI Codex: eight RX renderer and waterfall pace descriptions from the built native pages; upstream Setup references retained where present.
2026-09-28 - J.J. Boyd (KG4VCF), with AI-assisted transformation via OpenAI Codex: four Waterfall Defaults overlay descriptions from the built native page; original Thetis Setup widget references retained.
2026-09-28 - J.J. Boyd (KG4VCF), with AI-assisted transformation via Anthropic Claude Code: thirteen Spectrum Peaks descriptions (active peak hold and peak blobs) from the built native page; defaults and ranges as the desktop loads them.
2026-09-28 - J.J. Boyd (KG4VCF), with AI-assisted transformation via Anthropic Claude Code: Display V12 descriptions of the rest of Spectrum Defaults, Waterfall Defaults, Grid & Scales, Multimeter, TX Display's waterfall amplitude scale and 3D View, and Appearance's Reset all colors, from the built native pages; defaults and ranges as the desktop loads them; tooltips that named source code were rewritten on the native pages first.
