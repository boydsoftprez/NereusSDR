// Modification history (NereusSDR):
//   2026-10-23  J.J. Boyd (KG4VCF), AI-assisted via Claude Code:
//                 remoteTraceStartShareForTest().
//   2026-10-23  J.J. Boyd (KG4VCF), AI-assisted via Claude Code: the
//                 noise floor's measurement row (m_noiseFloorMeasurement),
//                 the wing placeholders fillWidebandWings marks and the
//                 frames with data since, and a remote pan's view of the
//                 Core's trace average warming up (noteRemoteTraceWarmUp).
//   2026-10-23  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code:
//                 imdOverlayShown() and syncWfTimestampTicker() replace
//                 imdMarkersDrawn() and sameImdMarkers(); test seams
//                 wfTimestampTickerActiveForTest() and
//                 overlayRebuildsEveryFrameForTest().
//   2026-10-22  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code:
//                 imdMarkersDrawn(), sameImdMarkers() and
//                 m_overlayRowPeriodMs, so the static overlay is marked
//                 only when the IMD markers or the time scale change.
//   2026-10-22  fftTraceFloorNdc(), fftTraceShiftDb() and
//                 buildFftPeakVertices(), with their test seams, so the GPU
//                 trace sits on its grid. After Longpath (Martin Fischer,
//                 OE5SOS; PR 100, c3ce95bf [@383f058]) by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Claude
//                 Code.
//   2026-10-22  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (Claude Opus 5.5): NB Blank's decision is
//                 WaterfallImpulseBlanker (a row's median against the
//                 median of accepted rows, at most two rows blanked in a
//                 row); waterfallRowsBlanked() counts the rows it replaced.
//   2026-10-22  J.J. Boyd (KG4VCF), AI-assisted via Claude Code: a live
//                 trace of a remote pan's row clock, gated on
//                 NEREUS_TRACE_REMOTE_ROWS=1 (diagnostics only).
//   2026-10-21  J.J. Boyd (KG4VCF), AI-assisted via Claude Code: a remote
//                 pan's waterfall scrolls by a steady row clock read each
//                 frame; its rows are handed over early with the time each
//                 is due and go in on a grid locked to those due times
//                 (RemoteWaterfallRowClock). The row queue holds a full
//                 hold's rows, a drawn frame takes at most two rows, and
//                 rows due after the transmit view lets go are kept.
//   2026-10-21  J.J. Boyd (KG4VCF), AI-assisted via Claude Code: a remote
//                 pan's waterfall ticks and glides at the Core's row period.
//   2026-10-21  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (Claude Opus 5.5): a drag keeps the receiver's press
//                 frequency, not an offset against the press-time view;
//                 the filter edge drag's own slice and filter.
//   2026-10-18  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (Claude Opus 5.5): receiver grab hit test, drag offset
//                 state and the wheel step accumulator.
//   2026-10-13  J.J. Boyd (KG4VCF), OpenAI Codex: GUI cached IMD frame API.
//   2026-10-13 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//                 Test-only friend for visible-plane IMD integration evidence.
//   2026-10-12 - Share the nine Appearance colors and RX Custom definition,
//                 retaining per-pan picture colors and independent TX.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Share the computer TX scale without stale inactive saves;
//                 preserve non-Manual waterfall modes in Smooth Defaults.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Apply approved effective trims, runtime level refresh and
//                 stored-step Track floor bounds without endpoint writes.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Query closed-pan ship defaults through the shared inventory.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Persist each pan's requested Clarity choice; expose pure
//                 current values and page-scoped picture reset.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Track the approved workspace text minimum in paint caches.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

#pragma once

// =================================================================
// src/gui/SpectrumWidget.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/enums.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-12: Export signal addresses for Windows DLL consumers.
//               J.J. Boyd / KG4VCF, AI-assisted via OpenAI Codex.
//   2026-10-12: Refresh CTUN container state on mirrored pan changes without
//               a new pin request. J.J. Boyd / KG4VCF, AI-assisted via Codex.
//
//   2026-10-11 : localSpectrumSourceForTest(), so a test can tell which
//                 receiver's frame a pan drew. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-12 - Preserve each pan's Track floor choice during Level Cal.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Expose explicit trace/fill fallback for approved parent gating.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-10 : Display pill plan Task 19: displayPillRequested() and
//                 bandRangeRequested() for the trimmed right-click menu.
//                 J.J. Boyd (KG4VCF), AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-10-10 : Display pill plan Task 15: setRemoteDecimation(), and
//                 moxHoldLengthMs() from the Core's FFT shape on a pan a
//                 Core feeds. J.J. Boyd (KG4VCF), AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-10-10 : Display pill plan Task 13: setDisplayKeyed() (the window's
//                 key edges on this pan), setLocalFftShape() and
//                 moxHoldLengthMs(), and the per-band waterfall AGC run
//                 levels kept in memory. J.J. Boyd (KG4VCF), AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-10-10 : Display pill plan Task 12: setClarityFloorTargetDbm(), the
//                 Clarity floor the drawn one moves toward on every
//                 waterfall row. J.J. Boyd (KG4VCF), AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-10-10 : Display pill plan Task 8: Cal Offset as its own term.
//                 setStationCalOffsetDb() / stationCalOffsetDb() carry the
//                 station's receive offset (never saved); setDbmCalOffset()
//                 / dbmCalOffset() are the operator's Cal Offset alone.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-10 : Display pill plan Task 5: a trace colour of its own
//                 (traceColor), the GPU fill's lower edge taken from the
//                 fill colour, and a background behind the 2D spectrum
//                 (backgroundEnabled, backgroundFillColor,
//                 backgroundImagePath, backgroundOpacity), ported from
//                 AetherSDR src/gui/SpectrumWidget.{h,cpp} [@1e0718ad];
//                 attribution block below. J.J. Boyd (KG4VCF), AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-10-10 : Display pill plan Task 17: the waterfall NF-AGC offset
//                 in 0.1 dB steps (waterfallAGCOffsetTenthsDb). J.J. Boyd
//                 (KG4VCF), AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-10-11 : Follow-up wave F6: updateSpectrumLinear() is told
//                 whether the frame was computed after the last FFT input
//                 purge; only such a frame ends the waterfall's hold after
//                 a key edge. J.J. Boyd (KG4VCF), AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-10-10 : Follow-up wave F3: displayKeyedChanged(), so the Display
//                 pill can grey dB Max and dB Min while the pan is keyed.
//                 J.J. Boyd (KG4VCF), AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-10-10 : Follow-up wave F1: NB Blank's threshold is in dB
//                 (waterfallBlankingThresholdDb, 1 to 20, default 6), a
//                 NereusSDR design; AetherSDR's factor is not carried
//                 over. J.J. Boyd (KG4VCF), AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-10-10 : Display pill plan Task 4: NB Blank on the waterfall
//                 (waterfallBlankingEnabled / Threshold), a fixed grid
//                 spacing (freqGridSpacingHz) and the frequency scale's
//                 text size (freqScaleFontPx, freqScaleHeight), ported
//                 from AetherSDR src/gui/SpectrumWidget.{h,cpp}
//                 [@1e0718ad]; attribution block below. J.J. Boyd
//                 (KG4VCF), AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-10 : Display pill plan Task 3: setClarityFloorDbm() and
//                 effectiveWaterfallLevels(); waterfall Black Level and
//                 Color Gain defaults 57 and 60 (core/spectrum/
//                 ClarityLevels.h). J.J. Boyd (KG4VCF), AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-10-11 : m_wfHistoryRescales, waterfallHistoryForTest() and its
//                 companions, for the waterfall history's resize tests.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 : R-DISP-RETUNE (third wave): setRemoteSpectrumContext takes
//                 the Core's word on whether a context continues the picture
//                 shown (coreCarriedOn) in place of the span asked. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 : R-DISP-RETUNE: RemoteContextPicture and
//                 remoteContextPicture(): a context for the picture moved
//                 in frequency is told apart from a new picture. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 : setWaterfallGlideForTest(), so a native test can hold
//                 the GPU waterfall's glide at a set point between two
//                 rows. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 : m_wfRowsSinceUpload, waterfallRectForTest() and
//                 setWaterfallLiveForTest() for the GPU waterfall's ring
//                 seam, paused glide and whole-ring upload. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-10 : R-DISP-HOLD: remoteContextKeptPicture() and
//                 remoteCodecForTest(). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-09 : DynamicOverlayRows and m_overlayDynamicRows, the GPU
//                 dynamic overlay's row tracking, and spectrumRectForTest().
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 : Rotor control plan Task 8: SpotMarker::bearingDeg, the
//                 spot menu's Turn beam (setSpotBeamTurner,
//                 buildSpotContextMenu) and spotTuned for auto-turn.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-04 : Opt-in numeric diagnostics for actual waterfall rows and
//                 applied Core extras. J.J. Boyd (KG4VCF), AI-assisted
//                 via OpenAI Codex. No display behavior changes.
//   2026-10-04 : retain each 2D waterfall row's RF window so TX history
//                 aligns on the restored RX axis without resampling the
//                 saved RX rows. J.J. Boyd (KG4VCF), AI-assisted via
//                 OpenAI Codex.
//   2026-10-01  J.J. Boyd / KG4VCF. Opt-in numeric RX history diagnostics.
//                 AI-assisted via OpenAI Codex.
//   2026-10-02 : shared SpectrumLayout for CPU/GPU panel paint and hit
//                 geometry (issues #141/#147). J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-09-30 : m_fftFillHasData, whether the fill buffer holds the current
//                 trace's fill now that the fill is written only while pan
//                 fill is on (GUI memory leak). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-30 : setEdgeMarkedSlice, refreshSliceFlags and
//                 drawOffScreenArrow: a listened slice a layout change
//                 placed here draws its own edge marker, never through the
//                 pan's VFO (desktop listening fix 3). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-30 : ForeignSliceMarker::unowned; a marker's click words say
//                 who controls the slice and offer Take control (desktop
//                 listening lane review). J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-30 : setGridFollowSaveHold, the saved grid noise floor follow
//                 held at the user's value while the level calibration
//                 holds the follow off. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-29 : setCoreWaterfallLevelsAvailable / setCoreWaterfallLevels, a
//                 remote window's waterfall AGC levels from the Core. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
//   2026-09-24 : peakHoldEnabledChanged signal (R-R3-49, R-R3-21).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 : iPhone app plan Task 78 (R-IOS-02, R-IOS-30): other
//                 devices' slices drawn as markers (dashed, no flag, not
//                 draggable) with a label that says whose they are.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 : WfGradientStop and wfSchemeStops() moved to
//                 core/spectrum/WaterfallPalettes.h (iPhone app Task 19,
//                 R-IOS-06). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-08-08 — J.J. Boyd (KG4VCF). Two bench-reported defects.
//                 (1) Extended pan: Sub-Epic F shipped the wideband DATA
//                 path and never the paint, so the stored bins had no
//                 reader and visibleBinRange()'s clamp stretched the DDC
//                 trace across the whole panel once zoomed past the DDC
//                 rate. Adds listenableIslandPixels() /
//                 fillWidebandWings() / drawExtendedIslandBounds(); wing
//                 frequency axis and dB calibration from Thetis
//                 wbDisplay.cs:440, 2106, 4511, 4680-4704 [v2.10.3.15].
//                 (2) A floated pan lost its render context. Adds
//                 prepareForTopLevelChange() / resetGpuResources() /
//                 applyNativeWindowIsolationPolicy(), ported from
//                 AetherSDR src/gui/SpectrumWidget.cpp:1843-1857,
//                 2227-2247, 7092-7110 [@1e0718ad]. AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-09-22 — J.J. Boyd (KG4VCF). Adapt prepareForShutdown() from
//                 AetherSDR src/gui/SpectrumWidget.cpp:2397-2420 [@0dea0dd7].
//                 Keep a standalone widget's owning graphics window alive
//                 until QRhiWidget teardown. AI-assisted via OpenAI Codex.
//   2026-09-26 : bandPlanManager() accessor, so a test reads the plan the
//                 strip draws (R-IOS-11, R-R3-49). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 28 (R-R3-49, A11):
//                                    transmitRefLevel() and
//                                    transmitDynamicRange() readers.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 29 (R-R3-49, R-R3-12):
//                                    drawsSpectrumTrace(); no stale receive
//                                    trace under the transmit axis.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26 : a remote waterfall row queue (R-R3-21, R-R3-08): rows wait
//                 for the ticker with the RF window they were captured at.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 : Parity Task 31 (A11, R-R3-49): display duplex (DUP) off by
//                 default as Thetis; with it on while keyed the pan keeps the
//                 receive span and bins under the transmit grid and levels,
//                 the TX filter sits against the receive span without XIT,
//                 the receive trace takes the transmit calibration, and a
//                 change while keyed swaps the view and resets the peaks.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 : VFO flag crash lane: test seams for the VFO marker and
//                 passband. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-01: Completed attribution for per-slice marker colors
//                 and selected-last paint order
//                 by J.J. Boyd (KG4VCF), with AI assistance via
//                 OpenAI Codex. Port introduced 2026-09-23.
//                 Source: AetherSDR src/gui/SpectrumWidget.cpp [@9f81dc00].
//                 Upstream has no per-file copyright header.
//                 Copyright (C) 2024-2026 Jeremy (KK7GWY) and
//                 AetherSDR contributors. GPLv3; project source:
//                 https://github.com/ten9876/AetherSDR
//   2026-10-09 - Spot list and spot display changes mark the GPU path's
//                 static overlay dirty, as AetherSDR [@1e0718ad] does, so
//                 the spot labels follow them. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-12: C-Tune switch held: applyRemoteCtunState takes the reason
//                 the receiver's C-Tune setting is not this window's to
//                 change (ctunHeldReason), and setCtunEnabled then changes
//                 nothing. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

// =================================================================
// Source attribution (AetherSDR, GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors,
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file carries ports and structural derivatives of AetherSDR
//   source. AetherSDR is licensed under the GNU General Public License
//   v3. NereusSDR is also GPLv3. Attribution follows GPLv3 §5
//   requirements. AetherSDR has no per-file license header; its project
//   LICENSE applies.
//
//   Ported at 1e0718ad from src/gui/SpectrumWidget.h (with
//   src/gui/SpectrumOverlayMenu.cpp for ranges): the NB waterfall
//   blanker, the frequency grid spacing and the frequency scale text
//   size (Modification history above, 2026-10-10, Task 4); the separate
//   trace colour and the spectrum background (2026-10-10, Task 5). Each
//   ported block carries its own "From AetherSDR" cite. The 3D
//   stacked-trace port has its own block below.
// =================================================================

// =================================================================
// Source attribution for 3D stacked-trace spectrum (AetherSDR, GPLv3):
//   Project lead: Jeremy (KK7GWY) / AetherSDR contributors
//   https://github.com/ten9876/AetherSDR
//   Upstream files at 1872028c: src/gui/SpectrumWidget.h and
//   src/gui/DssRenderer.h. AetherSDR has no per-file license header;
//   its project LICENSE is GPLv3.
//
// Modification history (3D stacked-trace port, NereusSDR):
//   2026-10-02 - Integrated the display bindings and speed fold with
//                landed remote capture, RX/TX and marker paths for
//                NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                integration via OpenAI Codex.
//   2026-08-08 - Adapted the AetherSDR 3D controls and renderer state
//                for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Anthropic Claude Code. The 3D Speed
//                row divider and peak-hold fold are NereusSDR additions.
//   2026-10-01 - Corrected frame-fold and skipped-paint GPU upload state
//                for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                implementation via OpenAI Codex.
// =================================================================

/*  enums.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2025 Original authors
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

#include "gui/ImdMeasurementPresenter.h"
#include "gui/ImdWaterfallAnnotations.h"
#include "gui/RemoteSpectrumCapture.h"
#include "gui/spectrum/RemoteWaterfallRowClock.h"
#include "gui/NereusGuiExport.h"
#include "gui/WaterfallImpulseBlanker.h"
#include "gui/WheelStepAccumulator.h"

#include <QElapsedTimer>
#include <QPointer>
#include <QRegion>
#include <QWidget>
#include <QVector>
#include <QImage>
#include <QPainterPath>
#include <QPixmap>
#include <QColor>
#include <functional>
#include <QPoint>
#include <QMap>
#include <QSet>
#include <QHash>
#include <QStaticText>
#include <QTimer>
#include <QPropertyAnimation>

#include <functional>
#include <deque>
#include <optional>
#include <QStringList>

#include "core/spectrum/ActivePeakHoldTrace.h"
#include "core/spectrum/DisplayFollowers.h"
#include "core/spectrum/PeakBlobDetector.h"
#include "core/spectrum/SpectrumAvenger.h"
#include "core/spectrum/SpectrumReducer.h"

#include <array>
#include <limits>
#include "gui/DssRenderer.h"
#include <utility>

#include "core/ConnectionState.h"
#include "core/WdspTypes.h"  // DSPMode — for TX filter IQ-space mapping (Plan 4 D9)
#include "core/spectrum/SpectrumDetectorMode.h"
#include "core/spectrum/ISpectrumSink.h"  // R1 Task 4: also supplies WfColorScheme + AverageMode
#include "core/spectrum/WaterfallPalettes.h"  // iPhone app Task 19: WfGradientStop, wfSchemeStops
#include "core/session/media/DisplayCodec.h"
#include "models/SpotBeamTurner.h"  // rotor control plan Task 8: QPointer needs the type

QT_BEGIN_NAMESPACE
class QLabel;
class QMenu;
QT_END_NAMESPACE

// GPU spectrum: QRhiWidget base class for Metal/Vulkan/D3D12 rendering.
// CPU fallback: QWidget with QPainter.
// Note: NEREUS_GPU_SPECTRUM is set in CMakeLists.txt via target_compile_definitions.
#ifdef NEREUS_GPU_SPECTRUM
#include <QRhiWidget>
#include <rhi/qrhi.h>
#include "gui/DssMeshGeometry.h"  // DssOutlinePipelineMode (Task 7 Step 6b)
using SpectrumBaseClass = QRhiWidget;
#else
using SpectrumBaseClass = QWidget;
#endif

class TestDssRowTee;
class TestWorkspaceSpectrumFontMinimum;

namespace NereusSDR {

class BandPlanManager;
class PanadapterStack;
class RemoteMediaController;
class SpectrumOverlayMenu;
class VfoWidget;
class ImdOverlay;  // Phase 3M-4 Task 12 — two-tone IMD overlay analytical core
class ImdSpectrumTestAccess;
class WaterfallTicker;  // src/gui/spectrum/WaterfallTicker.h
struct SpectrumEndpointContext;
class DisplaySettingsModel;  // src/models/DisplaySettingsModel.h (3D Stacked-Trace Spectrum Plan Task 18)

// Waterfall color scheme presets: extracted to core in R1 Task 4
// (src/core/spectrum/ISpectrumSink.h) alongside AverageMode below, because
// RadioModel::applyClaritySmoothDefaults names specific enumerators of
// both and must keep compiling once this file is no longer reachable from
// src/models. Same name, same namespace, no alias needed (contrast
// SpectrumDetector below, which was renamed and does need one).

// Spectrum render mode for the panadapter surface.
// From AetherSDR SpectrumWidget.h:78-82 [@1872028c].
enum class SpectrumRenderMode : int {
    Mode2D = 0,    // FFT trace + scrolling waterfall (classic)
    Mode3D,        // 3DSS perspective stacked-trace surface
    Count          // sentinel
};

// Frequency label alignment for the bottom scale bar.
// From Thetis comboDisplayLabelAlign (setup.designer.cs:34635).
// Thetis exposes 5 options (Left/Center/Right/Auto/Off); NereusSDR
// Phase 3G-8 commit 5 expands the previous 2-mode implementation to
// match.
enum class FreqLabelAlign : int {
    Left = 0,
    Center,
    Right,
    Auto,   // centered when room, otherwise left
    Off,    // suppress frequency labels entirely
    Count
};

// AverageMode: see the ISpectrumSink.h comment above WfColorScheme; same
// R1 Task 4 extraction, same reasoning, no alias needed.

// Spectrum detector type: extracted to core in R1 (src/core/spectrum/
// SpectrumDetectorMode.h) so this GUI header is no longer required to get
// the enum; that header carries the full Thetis citation trail. Alias kept
// so existing call sites (SpectrumDetector::Peak, etc.) are untouched.
using SpectrumDetector = NereusSDR::SpectrumDetectorMode;

// Spectrum averaging mode (split from legacy AverageMode for Thetis parity).
// Ported from Thetis comboDispPanAveraging / comboDispWFAveraging
// (setup.designer.cs:34835 / setup.designer.cs:34436 [v2.10.3.13]).
// Items: None / Recursive / Time Window / Log Recursive (4 items, both combos).
// Applied across frames: each new FFT result is mixed with the running
// history buffer per the chosen policy.
// From Thetis specHPSDR.cs:383-415 [v2.10.3.13] AverageMode / AverageModeWF
// → SetDisplayAverageMode(disp, pixout, mode).
enum class SpectrumAveraging : int {
    None         = 0, // pass frame through unchanged (Thetis "None")
    Recursive    = 1, // exponential decay in linear domain (Thetis "Recursive")
    TimeWindow   = 2, // sliding time window (Thetis "Time Window")
    LogRecursive = 3, // exponential decay in log/dB domain (Thetis "Log Recursive")
    Count
};

// WfGradientStop and wfSchemeStops() live in core/spectrum/WaterfallPalettes.h
// (iPhone app Task 19), included above, so the Core's catalogue reads the
// same palettes.

// Interpolate a 0..1 position across a scheme's gradient stops. Extracted
// from dbmToRgb()'s inline loop so the 3DSS palette can share the stops
// without inheriting the waterfall gain / black-level window applied above
// it. Behaviour is unchanged for dbmToRgb.
QRgb interpolateWfGradient(float t, const WfGradientStop* stops, int count);

// CPU-rendered spectrum + waterfall display widget.
// Phase A: QPainter fallback (get something visible fast).
// Phase B: Switch to QRhiWidget for GPU rendering.
//
// Layout (top to bottom):
//   36px  - dBm scale (left strip)
//   ~40%  - spectrum trace (FFT line plot)
//   4px   - divider
//   ~60%  - waterfall (scrolling heat-map history)
//   20px  - frequency scale bar
//
// From gpu-waterfall.md lines 274-289
class NEREUS_GUI_EXPORT SpectrumWidget : public SpectrumBaseClass, public NereusSDR::ISpectrumSink {
    Q_OBJECT
    friend class ::TestDssRowTee;
    friend class ::TestWorkspaceSpectrumFontMinimum;

    // Phase 3Q-8: animated dim factor for the disconnect overlay.
    // 1.0 = no dim (connected), 0.4 = 60% dim (disconnected, after 800 ms fade).
    Q_PROPERTY(float disconnectFade READ disconnectFade WRITE setDisconnectFade)

public:
    explicit SpectrumWidget(QWidget* parent = nullptr);
    ~SpectrumWidget() override;

    QSize sizeHint() const override { return {800, 400}; }

    // Cap on waterfall scrollback history capacity, in rows. Bounds memory use.
    // Public so unit tests (tst_waterfall_scrollback) can mirror the
    // capacity-clamp formula in their parallel shim.
    //
    // Originally 4096 (~32 MB at 2000 px wide) — ported from unmerged
    // AetherSDR PR #1478 [@2bb3b5c]. NereusSDR raised the cap to 16384
    // (~128 MB at 2000 px wide) post-merge to give ~8 min effective rewind
    // at the default 30 ms refresh and 20+ min at any period ≥ 73 ms.
    // Disk-spool tier deferred to Phase 3M (Recording).
    static constexpr int kMaxWaterfallHistoryRows = 16384;

    // ---- Frequency range ----
    void setFrequencyRange(double centerHz, double bandwidthHz);

    /// R-R3-21: the pan overlay's zoom buttons. zoomBy() scales the visible
    /// span (factor > 1 zooms out); zoomToSegment() fits the band-plan
    /// segment holding the VFO; zoomToBand() fits every segment of the
    /// VFO's band. Each goes through the Ctrl+wheel zoom's path, so the
    /// FFT replans and the wideband view follows as it does there. The
    /// fits do nothing where the band plan has no segment under the VFO.
    void zoomBy(double factor);
    void zoomToSegment();
    void zoomToBand();

    /// Re-aim the display window WITHOUT touching waterfall history.
    ///
    /// setFrequencyRange treats any bandwidth change as a large shift and
    /// clears the scrollback (SpectrumWidget.cpp:1238-1249). That is right
    /// for a band jump and wrong for the MOX edge, which changes the window
    /// twice per transmission: using it there deleted the receive
    /// scrollback on every key-up and again on every un-key. The 2026-05-08
    /// handoff called for exactly the opposite -- "on MOX-down, RX1
    /// spectrum returns to the panadapter without losing waterfall
    /// scrollback continuity".
    ///
    /// Transmit rows and receive rows describe different windows. Keep
    /// each captured row intact and project only the live viewport, so
    /// returning to RX restores its history without repeated resampling.
    void setDisplayWindowPreservingHistory(double centerHz, double bandwidthHz);

    /// Render already-detected transmit pixels, bypassing the receive
    /// detector and avenger.
    ///
    /// TxAnalyzer's output has ALREADY had a detector and averaging applied
    /// by WDSP, using the TX Display controls. Feeding it through
    /// updateSpectrumLinear ran applySpectrumDetector and m_spectrumAvenger
    /// over it a second time using the RECEIVE controls, so the transmit
    /// settings only behaved as labelled while receive happened to sit at
    /// Peak / None. Found by Codex on PR #317.
    ///
    /// Input is dBm, matching TxAnalyzer::txFftReady, so there is no
    /// linear round trip either: the previous path raised each pixel to a
    /// power and took its log again purely to satisfy a signature.
    ///
    /// This is the trace-plane twin of setTxExternalWaterfall, which
    /// already stands the receive path down for the waterfall plane.
    void updateSpectrumFromTxPixels(int receiverId,
                                    const QVector<float>& binsDbm);
    /// R3: render Core's independent, already reduced dBm planes without
    /// running the local detector/averager over them again.
    /// R-R3-01/R-R3-08 (parity Task 17): `grantedFftSize` is the FFT size
    /// the Core runs for this pan (its grant, or what was asked of an older
    /// Core), 0 when unknown. The bin width, the Hz/bin readout and the
    /// normalise shift follow it, and the context's frame rate (the rate the
    /// Core actually sends, after the display budget) times the peak hold,
    /// blob and noise-floor decay.
    /// R-DISP-RETUNE: `coreCarriedOn` is the Core's own word on whether
    /// this context continues the picture the pan shows (its
    /// `pictureGeneration` is the one of the context shown), and it alone
    /// decides: true is the picture shown, moved or not; false is a new
    /// picture whatever its numbers. The pan compares no spans or centers
    /// to tell a move from a new picture. Without it (a Core that states no
    /// picture) the rule is the one before the moved picture: a context
    /// that differs only in its dBm range or frame rate is the picture
    /// shown and anything else is a new one.
    void setRemoteSpectrumContext(const SpectrumEndpointContext& context,
                                  double sourceCentreHz, double sampleRateHz,
                                  int grantedFftSize = 0,
                                  std::optional<bool> coreCarriedOn = std::nullopt);
    /// R-DISP-HOLD: whether the last context was for the picture the pan
    /// already showed (same receiver and source geometry, same view, same
    /// sample counts and wide row), so the pan kept its last frame, peak
    /// hold, blobs and pending waterfall row. A new picture empties them.
    bool remoteContextKeptPicture() const
    {
        return m_remoteContextPicture == RemoteContextPicture::Same;
    }
    /// R-DISP-RETUNE: what the last context was to the picture the pan
    /// already showed.
    enum class RemoteContextPicture {
        /// Anything else: the pan starts again, as it always did.
        New,
        /// The Core says this context continues the picture shown, and
        /// something the pan holds per pixel changed with it (a retune or a
        /// pan drag moved the stream center, the view center or both). Only
        /// a Core that states the picture can say so; the pan derives no
        /// move of its own. The Core's averaged trace carried
        /// on, so the drawn trace stays, at the frequency it was captured
        /// at, until the keyframe; the waterfall history and its queued rows
        /// keep their frequencies; the noise floor estimate is untouched.
        /// The peak hold and the blobs empty: they are held per pixel and
        /// every pixel now shows another frequency.
        Moved,
        /// R-DISP-HOLD: the picture the pan already showed.
        Same,
    };
    RemoteContextPicture remoteContextPicture() const { return m_remoteContextPicture; }
    bool updateRemoteSpectrum(const DisplayCodecFrame& frame);
    /// queueRow false: the frame's waterfall row is not queued (it was
    /// handed over early with its due time); the trace is drawn as ever.
    bool updateRemoteSpectrum(const DisplayCodecFrame& frame,
                              const NereusSDR::RemoteSpectrumCapture& capture,
                              bool measurementCurrent = true, bool queueRow = true);
    /// R-R3-21 / R-R3-08: one remote waterfall row, captured at centreHz and
    /// spanHz, queued for the ticker without touching the trace: a gap row
    /// (blended or repeated) or a row presented after its pan was tuned. It
    /// is drawn at the frequency it was captured at. False outside a remote
    /// window or for an empty or non-finite row.
    bool enqueueRemoteWaterfallRow(const QVector<float>& pixelsDbm,
                                   const QVector<float>& wideDbm,
                                   double centreHz, double spanHz);
    bool enqueueRemoteWaterfallRow(const QVector<float>& pixelsDbm,
                                   const QVector<float>& wideDbm,
                                   const NereusSDR::RemoteSpectrumCapture& capture);
    /// A row handed over before it is due: dueInNs from now (negative when
    /// already past), on the caller's clock, so the caller's clock and this
    /// widget's need not be the same. The row clock slides it in by then.
    /// frameEncoderSequence: the row is a decoded frame's own row (its
    /// trace is drawn at the due time by updateRemoteSpectrum with
    /// queueRow false), not a synthesized one. The rows queued without a
    /// due time (the overloads above) are due when queued.
    bool enqueueRemoteWaterfallRow(const QVector<float>& pixelsDbm,
                                   const QVector<float>& wideDbm,
                                   const NereusSDR::RemoteSpectrumCapture& capture,
                                   qint64 dueInNs,
                                   std::optional<quint32> frameEncoderSequence = std::nullopt);
    /// Forget the rows handed over early that are not due yet (the
    /// presenter forgot their items: a key, a new context, a teardown).
    /// Returns how many were forgotten.
    int forgetEarlyRemoteRows();
    /// Rows not drawn yet, beyond the rows a full hold hands over early
    /// (remoteRowQueueCap()); past the cap the oldest is dropped.
    static constexpr int kMaxRemoteRowQueue = 32;
    /// The row queue's cap: kMaxRemoteRowQueue plus the rows of the longest
    /// hold the presenter trusts (RemoteDisplayPresenter::kMaxHoldNs) at the
    /// Core's row period, so rows handed over early for a deep audio hold
    /// are never dropped before they are due.
    int remoteRowQueueCap() const;
    /// R-R3-21: a remote trace captured at centreHz and spanHz, drawn on the
    /// current axis (a frame presented after its pan was tuned). False
    /// outside a remote window, during MOX, or for a non-finite trace.
    bool presentRemoteTraceCaptured(const QVector<float>& traceDbm,
                                    double centreHz, double spanHz);
    /// Rows dropped because the queue was full, this widget's life.
    quint64 remoteRowsDropped() const { return m_remoteRowsDropped; }
    /// The Core's granted FFT size for this pan, 0 outside a remote window
    /// or before its first context.
    int remoteGrantedFftSize() const { return m_remoteSpectrum ? m_remoteFftSize : 0; }
    /// The frame rate the Core sends this pan, 0 outside a remote window or
    /// before its first context.
    int remoteFrameRate() const { return m_remoteSpectrum ? m_remoteFps : 0; }
    /// Display pill plan Task 15: the decimation the Core runs this pan's
    /// FFT at, which is the one this window last asked it for through the
    /// Core's decimation control (1 for a Core that has no such control).
    /// RemoteMediaController gives it with each accepted receive context;
    /// below 1 is taken as 1. With remoteGrantedFftSize() and the context's
    /// source rate it gives a remote pan's moxHoldLengthMs().
    void setRemoteDecimation(int decimation);
    int remoteDecimation() const { return m_remoteDecimation; }
    /// Retire incoming/pending planes while preserving painted RF history.
    /// Used while renewing the same remote display binding; full retirement
    /// (disconnect, replacement or rejection) uses clearRemoteSpectrum().
    void invalidateRemoteSpectrumFrame();
    void clearRemoteSpectrum();
    // Opt-in diagnostic events; numeric fields only. No history policy changes.
    enum class RxHistoryEvent {
        RemoteFullClear = 1, Leave3D = 2, LiveImageResize = 3,
        HistoryBufferReset = 4, WaterfallClear = 5, RfReproject = 6,
        ViewportRebuild = 7, MediaRetired = 8, BudgetBindingRetired = 9,
        LegacyBindingRetired = 10, LegacyBindingCreated = 11,
        BudgetBindingCreated = 12, BindingPreserved = 13,
        EndpointRejected = 14, FrameRenewal = 15
    };
    void traceRxHistoryEvent(RxHistoryEvent event, int sourceStream = -1,
                             quint64 sourceEpoch = 0, QSize nextImageSize = {},
                             double oldCentreHz = 0, double oldSpanHz = 0,
                             double newCentreHz = 0, double newSpanHz = 0) const;
    bool remoteWidebandAvailable() const { return m_remoteWidebandAvailable; }
    bool remoteWidebandActive() const { return m_remoteWidebandActive; }
    void setCenterFrequency(double centerHz);
    double centerFrequency() const { return m_centerHz; }
    double bandwidth() const { return m_bandwidthHz; }

    /// Width the dBm scale strip reserves along the right edge, 0 when hidden.
    ///
    /// Public so a parent laying widgets over this one can keep clear of it.
    /// PanadapterApplet's status strip is pinned to the top-right and was
    /// landing on top of the strip's range up/down arrows, which made them
    /// hard to see and hard to hit.
    int reservedRightEdgeWidth() const { return effectiveStripW(); }

    // Re-fire the auto-zoom replan with the current bandwidth.  Used by
    // setup pages (e.g. when the user changes the Hz/bin target) to kick
    // the FFTEngine into recomputing targetSize without a zoom action.
    void requestAutoZoomReplan() { emit bandwidthChangeRequested(m_bandwidthHz); }
    void setDdcCenterFrequency(double hz);
    double ddcCenterFrequency() const { return m_ddcCenterHz; }
    void setSampleRate(double hz);
    double sampleRate() const { return m_sampleRateHz; }

    /// 3M-5d follow-up: TX-side bin-frequency context used by
    /// visibleBinRange() when m_moxOverlay is true.  RX path is
    /// unchanged.  m_txCenterHz is the TX channel center (VFO
    /// frequency on the air); m_txSampleRateHz is the TX baseband
    /// sample rate the TxAnalyzer feeds into WDSP (96000 Hz on the
    /// HL2 + ANAN-G2 family).  Without these, the WDSP bin array
    /// from GetPixels(pixout=0|1) gets sliced against the RX DDC
    /// rate + center, which produces a stretched, mis-mapped
    /// waterfall during MOX (bench 2026-05-10 surfaced three peaks
    /// across the visible window from a single TUNE tone).
    void setTxCenterFrequency(double hz);
    double txCenterFrequency() const noexcept { return m_txCenterHz; }
    void setTxSampleRate(double hz);
    double txSampleRate() const noexcept { return m_txSampleRateHz; }

    // ---- Display range ----
    void setDbmRange(float minDbm, float maxDbm);
    // Display pill plan Task 9: the RECEIVE range, as the band's stored dB
    // Max and Min give it (PanModels). In receive this is setDbmRange().
    // While keyed the live pair is the transmit grid, so the range goes to
    // the receive store and shows when the pan returns to receive.
    void setReceiveDbmRange(float minDbm, float maxDbm);
    // 3D Stacked-Trace Spectrum Plan Task 18: named setters for the two
    // fields setDbmRange() has always written directly, so
    // DisplaySettingsModel has something to bind each of the fifteen
    // values to individually (setDbmRange sets both at once; the popup's
    // Ref Level and Dyn Range sliders each set one).
    void setRefLevel(float dBm);
    void setDynamicRange(float dB);
    float refLevel() const { return m_refLevel; }
    float dynamicRange() const { return m_dynamicRange; }
    /// Parity Task 28: the transmit grid's pair whether or not it is live
    /// (setMoxOverlay swaps it in while keyed), for the transmit window a
    /// remote pan asks the Core for.
    /// One computer TX pair. The caller retains its current endpoint UI
    /// rules; storage retains the native Ref -200..200 / Range 10..200.
    void setTransmitDbmRange(float minDbm, float maxDbm);
    float transmitRefLevel() const { return m_moxOverlay ? m_refLevel : m_txRefLevel; }
    float transmitDynamicRange() const
    {
        return m_moxOverlay ? m_dynamicRange : m_txDynamicRange;
    }

    // Waterfall/spectrum vertical split fraction (Task 18's third new
    // setter): DisplaySettingsModel's binding target for the divider
    // drag in mouseMoveEvent. No popup or Setup control reaches this
    // directly (yet); persisted via DisplaySpectrumFrac regardless.
    void  setSpectrumFrac(float frac);
    float spectrumFrac() const { return m_spectrumFrac; }

    // ---- Waterfall settings ----
    void setWfColorScheme(WfColorScheme scheme) override;
    Q_INVOKABLE void setRxWfGradient(QString encoded);
    QString rxWfGradient() const { return m_rxWfGradient; }
    static QString defaultRxWfGradient();
    WfColorScheme wfColorScheme() const { return m_wfColorScheme; }
    void setWfColorGain(int gain);
    int  wfColorGain() const { return m_wfColorGain; }
    void setWfBlackLevel(int level);
    int  wfBlackLevel() const { return m_wfBlackLevel; }

    // ---- Spectrum renderer controls (Phase 3G-8 commit 3) ----

    // Legacy combined averaging mode — kept for backward compat (existing
    // callers not yet migrated).  Routes internally to setSpectrumAveraging().
    // Retired key: DisplayAverageMode (migration in Task 5.1).
    void setAverageMode(AverageMode m) override;
    AverageMode averageMode() const { return m_averageMode; }

    // ---- Detector + Averaging split (Task 2.1, handwave fix from 3G-8) ----
    // Ported from Thetis specHPSDR.cs:302-415 [v2.10.3.13] DetTypePan /
    // DetTypeWF / AverageMode / AverageModeWF.
    // RX1 scope dropped; NereusSDR applies as global panadapter default
    // with per-pan override via ContainerSettings dialog (3G-6 pattern).

    // Spectrum (panadapter) detector — bin-reduction policy.
    // From Thetis comboDispPanDetector [v2.10.3.13] (setup.designer.cs:34876).
    void setSpectrumDetector(SpectrumDetector d);
    SpectrumDetector spectrumDetector() const { return m_spectrumDetector; }

    // Spectrum (panadapter) averaging — frame-smoothing policy.
    // From Thetis comboDispPanAveraging [v2.10.3.13] (setup.designer.cs:34835).
    void setSpectrumAveraging(SpectrumAveraging a);
    SpectrumAveraging spectrumAveraging() const { return m_spectrumAveraging; }

    // Waterfall detector — bin-reduction policy for waterfall rows.
    // From Thetis comboDispWFDetector [v2.10.3.13] (setup.designer.cs:34461).
    void setWaterfallDetector(SpectrumDetector d);
    SpectrumDetector waterfallDetector() const { return m_waterfallDetector; }

    // Waterfall averaging — frame-smoothing policy for waterfall rows.
    // From Thetis comboDispWFAveraging [v2.10.3.13] (setup.designer.cs:34436).
    void setWaterfallAveraging(SpectrumAveraging a);
    SpectrumAveraging waterfallAveraging() const { return m_waterfallAveraging; }

    // Read-only access to post-pipeline output arrays (dBm display pixels).
    // Exposed for tests that drive updateSpectrumLinear() and need to
    // assert the avenger output for a given detector + averaging combo.
    // Pipeline contract: m_renderedPixels is sized to displayWidth and
    // contains the spectrum trace data; m_wfRenderedPixels is the same
    // shape for the waterfall plane.
    const QVector<float>& renderedPixels()   const { return m_renderedPixels;   }
    const QVector<float>& wfRenderedPixels() const { return m_wfRenderedPixels; }

    // 2026-05-22 bench fix for MaxBin meter accuracy.
    // Returns the strongest dBm pixel inside the active slice's IF
    // passband, computed from the undented spectrum pixels
    // (measurementPixels(), post detector + avenger pipeline -- see the
    // visual-notch note in the definition). Falls back to -400 sentinel
    // when those pixels are empty (cold start) or when the slice passband
    // falls entirely outside the visible spectrum window.
    //
    // The raw per-bin FFT power that MaxBin previously scanned (via
    // WdspEngine::onSpectrumBinsForMaxBin reading FFTEngine::fftReady)
    // can be ~12-17 dB below the spectrum's displayed pixel value
    // because the spectrum's detector + invEnb window-normalization +
    // avenger time-smoothing reconstructs the integrated signal power
    // that a single FFT bin can't show on its own. Sourcing MaxBin
    // from the display pipeline makes the meter read what the operator
    // visually sees on the trace.
    double peakDbmInSlicePassband() const;
    double peakDbmInPassband(double lowHz, double highHz) const;

    // Static helper for detector math. Exposed for unit tests.
    // Note: legacy bin-reduction helper, kept for tst_detector_modes;
    // production rendering uses applySpectrumDetector() (free function in
    // core/spectrum/SpectrumDetector.h, verbatim WDSP analyzer.c port) instead.
    // From Thetis specHPSDR.cs:302-321 [v2.10.3.13] DetTypePan / DetTypeWF.
    static void applyDetector(const QVector<float>& input, QVector<float>& output,
                              SpectrumDetector mode, int outputBins);

    // Smoothing time constant for Weighted / Logarithmic / TimeWindow.
    // 0.0 = no smoothing, 1.0 = infinite smoothing.
    //
    // DEPRECATED: use setSpectrumAverageTimeMs / setWaterfallAverageTimeMs
    // — those compute alpha from the time constant + frame rate via the
    // Thetis formula α = exp(-1 / (fps × τ)) per specHPSDR.cs:351-380
    // [v2.10.3.13]. The bare alpha setter is kept only for callers not yet
    // migrated; it overwrites the spectrum alpha and is clobbered on the
    // next time-spin or fps change.
    void setAverageAlpha(float alpha);
    float averageAlpha() const { return m_spectrumAverageAlpha; }

    // Per-side averaging time constants (milliseconds, ms→τ via /1000).
    // Drives both spectrum and waterfall paths independently — Thetis
    // specHPSDR.cs has separate AvTau / AvTauWF setters that each compute
    // a back-multiplier α via Math.Exp(-1.0 / (frame_rate * tau)).
    // From Thetis specHPSDR.cs:351-380 [v2.10.3.13] — AvTau / AvTauWF.
    // From Thetis setup.cs udDisplayAVGTime_ValueChanged (default 30 ms)
    // and udDisplayAVTimeWF_ValueChanged (default 120 ms).
    void setSpectrumAverageTimeMs(int ms) override;
    int  spectrumAverageTimeMs() const { return m_spectrumAverageTimeMs; }
    void setWaterfallAverageTimeMs(int ms);
    int  waterfallAverageTimeMs() const { return m_waterfallAverageTimeMs; }
    float spectrumAverageAlpha() const { return m_spectrumAverageAlpha; }
    float waterfallAverageAlpha() const { return m_waterfallAverageAlpha; }

    // Peak hold: track per-bin max, decay after delay.
    void setPeakHoldEnabled(bool on);
    bool peakHoldEnabled() const { return m_peakHoldEnabled; }
    void setPeakHoldDelayMs(int ms);
    int  peakHoldDelayMs() const { return m_peakHoldDelayMs; }

    // ---- Active Peak Hold trace (Task 2.5) ----
    // Per-bin max tracking with configurable hold / decay / fill / TX gating.
    // Rendered as a separate pass in drawSpectrum() (Q14.1 locked decision).
    // From Thetis display.cs m_bActivePeakHold [v2.10.3.13].
    void setActivePeakHoldEnabled(bool on);
    void setActivePeakHoldDurationMs(int ms);
    void setActivePeakHoldDropDbPerSec(double r);
    void setActivePeakHoldFill(bool on);
    void setActivePeakHoldOnTx(bool on);
    // NereusSDR-original — Thetis ties the peak trace colour to the data-line
    // colour. We expose it separately so the user can keep a distinct peak
    // hold colour even when "Reset to Smooth Defaults" recolours the live
    // trace (which would otherwise hide the peak trace behind a same-coloured
    // solid line).
    void setActivePeakHoldColor(const QColor& c);
    /// Re-read the Spectrum Peaks settings (stored once for every pan) and
    /// redraw. reloadSpectrumPeaksSettingsOnAllPans does it for every pan
    /// that exists, so a Setup change reaches them all at once.
    void reloadSpectrumPeaksSettings();
    static void reloadSpectrumPeaksSettingsOnAllPans();

    // This pan transmitting, for the peak hold's transmit gate. setMoxOverlay
    // drives it on every MOX edge (MoxDisplayController, local and remote).
    void setActivePeakHoldTxActive(bool tx);

    bool   activePeakHoldEnabled()    const { return m_activePeakHold.enabled(); }
    /// Enabled and not switched off by this pan transmitting (Thetis
    /// display.cs:5011 [v2.10.3.15] bSpectralPeakHold).
    bool   activePeakHoldActive()     const { return m_activePeakHold.active(); }
    bool   activePeakHoldOnTx()       const { return m_activePeakHold.onTx(); }
    int    activePeakHoldDurationMs() const { return m_activePeakHold.durationMs(); }
    double activePeakHoldDropDbPerSec() const { return m_activePeakHold.dropDbPerSec(); }
    bool   activePeakHoldFill()        const { return m_activePeakHold.fill(); }
    QColor activePeakHoldColor()      const { return m_activePeakHoldColor; }

    // ---- Peak Blobs (Task 2.6) ----
    // Top-N peak markers with labeled ellipses. Rendered as a separate QPainter
    // pass in drawSpectrum() after the Active Peak Hold trace.
    // Defaults mirror Thetis Display.cs:4395-4714 [v2.10.3.13].
    void setPeakBlobsEnabled(bool e);
    void setPeakBlobsCount(int n);
    void setPeakBlobsInsideFilterOnly(bool i);
    void setPeakBlobsHoldEnabled(bool h);
    void setPeakBlobsHoldMs(int ms);
    void setPeakBlobsHoldDrop(bool d);
    void setPeakBlobsFallDbPerSec(double r);
    void setPeakBlobColor(const QColor& c);
    void setPeakBlobTextColor(const QColor& c);

    bool   peakBlobsEnabled()     const { return m_peakBlobs.enabled(); }
    int    peakBlobsCount()       const { return m_peakBlobs.count(); }
    bool   peakBlobsInsideFilterOnly() const { return m_peakBlobs.insideOnly(); }
    bool   peakBlobsHoldEnabled() const { return m_peakBlobs.holdEnabled(); }
    int    peakBlobsHoldMs()      const { return m_peakBlobs.holdMs(); }
    bool   peakBlobsHoldDrop()    const { return m_peakBlobs.holdDrop(); }
    double peakBlobsFallDbPerSec() const { return m_peakBlobs.fallDbPerSec(); }
    QColor peakBlobColor()        const { return m_peakBlobColor; }
    QColor peakBlobTextColor()    const { return m_peakBlobTextColor; }

    // Trace fill (under-the-curve shaded region).
    void setPanFillEnabled(bool on) override;
    bool panFillEnabled() const { return m_panFill; }
    void setFillAlpha(float a);       // 0.0 .. 1.0
    float fillAlpha() const { return m_fillAlpha; }

    // Trace line width (QPainter pen width). GPU path uses line list
    // default width until commit 5 renderer additions.
    void setLineWidth(float w);
    float lineWidth() const { return m_lineWidth; }

    // Trace gradient: when enabled, the QPainter fill gradient ramps
    // from transparent at baseline to the fill color at the trace,
    // and the trace uses a vertical color gradient. GPU path keeps
    // its existing per-vertex heatmap coloring (wire-up in commit 5).
    void setGradientEnabled(bool on);
    bool gradientEnabled() const { return m_gradientEnabled; }

    // Display calibration offset added to every bin before it's mapped
    // to screen Y. Ported from Thetis Display.RX1DisplayCalOffset
    // (display.cs:1372). Range: -30 .. +30 dB.
    // Display pill plan Task 8: this is the operator's Cal Offset alone
    // (key DisplayCalOffset, default 0, saved per pan). The station's
    // receive offset is the separate term below; the two are added in
    // displayCalOffsetDb().
    void setDbmCalOffset(float db);
    float dbmCalOffset() const { return m_dbmCalOffset; }

    // The station's receive offset for the stream this pan shows
    // (RadioModel::rxMeterOffsetDbForStream: calibration plus preamp or
    // attenuator). MainWindow pushes it with every frame and when the
    // station's offsets change. Never saved and never announced: it is the
    // station's fact, not a setting of this pan. Not held to the Cal
    // Offset's -30 .. +30 dB, which is the range of the operator's control.
    void setStationCalOffsetDb(float db);
    float stationCalOffsetDb() const { return m_stationCalOffsetDb; }

    // Trace colour (Phase 3G-8 commit 6 wiring). Single colour used for
    // both the line and the fill in the current renderer; plan §6 S11/S13
    // track splitting these if needed by future UI polish.
    void setFillColor(const QColor& c) override;
    QColor fillColor() const { return m_fillColor; }

    // Display pill plan Task 5: the trace line's own colour. Until one is
    // chosen (on this pan or as the Setup default) the trace is drawn in
    // the fill colour and follows it, as it always did. Per pan, key
    // DisplayTraceColor (default: none stored).
    // From AetherSDR src/gui/SpectrumWidget.h:461 [@1e0718ad]
    // NereusSDR, based on AetherSDR: AetherSDR's line colour has a fixed
    // cyan default beside a cyan fill; here "not chosen" is a state of its
    // own, so changing the fill colour still changes an unchosen trace.
    QColor traceColor() const { return m_traceColor.isValid() ? m_traceColor : m_fillColor; }
    bool traceColorFollowsFill() const { return !m_traceColor.isValid(); }
    void setTraceColor(const QColor& c) override;

    // Display pill plan Task 5: a background behind the 2D spectrum's grid
    // and trace: a fill colour, and over it an optional picture scaled to
    // cover the spectrum area. Opacity is how strongly the colour covers
    // the picture: 0 shows the picture alone, 100 the colour alone. With
    // the background off (the default) nothing is drawn and the spectrum
    // looks as it did before this existed; the picture, colour and opacity
    // stay chosen. An empty path clears the picture. The 3D spectrum is
    // not given a background. Per pan, keys DisplayBackgroundEnabled
    // (False), DisplayBackgroundImage (none), DisplayBackgroundOpacity (80)
    // and DisplayBackgroundColor (#0a0a14).
    // From AetherSDR src/gui/SpectrumWidget.h:435-440 [@1e0718ad]
    // NereusSDR, based on AetherSDR: on/off is a setting of its own (in
    // AetherSDR "Off" stores the path "none" and so forgets the picture),
    // no picture ships as a default, and every value is saved per pan.
    bool    backgroundEnabled() const { return m_bgEnabled; }
    void    setBackgroundEnabled(bool on);
    QString backgroundImagePath() const { return m_bgImagePath; }
    void    setBackgroundImagePath(const QString& path);
    int     backgroundOpacity() const { return m_bgOpacity; }
    void    setBackgroundOpacity(int pct);
    QColor  backgroundFillColor() const { return m_bgFillColor; }
    void    setBackgroundFillColor(const QColor& c);

    // ---- Waterfall renderer controls (Phase 3G-8 commit 4) ----

    void setWfHighThreshold(float dbm);
    float wfHighThreshold() const { return m_wfHighThreshold; }
    void setWfLowThreshold(float dbm);
    float wfLowThreshold() const { return m_wfLowThreshold; }
    void setWfAgcEnabled(bool on) override;
    void enableWaterfallAgcIfManual() override;
    bool wfAgcEnabled() const { return m_wfAgcEnabled; }

    /// Force the waterfall-AGC running min/max tracker to re-prime on the
    /// next bin push.  Use when the bin source changes dynamic range
    /// abruptly — without this, the 0.05 alpha follower takes ~3 s to
    /// converge. Not for a MOX edge: a key keeps the run level
    /// (setDisplayKeyed).
    void resetWaterfallAgc() { m_wfLevels.resetAgc(); }

    // Display pill plan Task 13 (design 5.3 to 5.5): this pan's half of the
    // window's key and un-key display edges. MoxDisplayController calls it
    // on every pan of the window, the transmitting one and the others,
    // before its rise and its fall.
    //
    // keyed = true: the waterfall AGC run level is saved under bandKey (the
    // receive context) and the follower freezes: no row of the key moves
    // the level, and rows keep drawing with the level in force.
    // keyed = false: the run level saved under bandKey comes back, or the
    // restart floor (WaterfallLevelFollower::kAgcRestartFloorDbm) when that
    // band has none, and the follower holds for holdLengthMs.
    // Either edge starts the noise-floor fast attack with holdLengthMs as
    // its minimum.
    //
    // bandKey is the pan's band at that edge, as an int; a band changed
    // while keyed restores the new band's level. The saved levels live
    // here, in memory. A repeat of the state the pan is already in does
    // nothing.
    void setDisplayKeyed(bool keyed, qint64 holdLengthMs, int bandKey);
    bool displayKeyed() const { return m_displayKeyed; }
    /// The size, decimation and input rate of the engine that feeds this
    /// pan in a window running its own DSP. MainWindow gives it whenever
    /// one of the three changes; zeros mean the pan has no engine here.
    void setLocalFftShape(int fftSize, int decimation, double sampleRateHz);
    /// How long this pan holds its estimates after an un-key
    /// (core/spectrum/MoxDisplayHold.h): from the local FFT shape, or
    /// kMoxHoldMinMs with none. A pan a Core feeds (remoteGrantedFftSize()
    /// above 0) uses the Core's shape instead: the granted size, the rate
    /// given to setRemoteSpectrumContext and remoteDecimation(). The Core's
    /// shape wins when both are set, because the Core's FFT is the one
    /// whose input refills after its key.
    qint64 moxHoldLengthMs() const;
    /// The waterfall AGC follower's run level; empty before its first row.
    std::optional<float> waterfallAgcRunLevel() const { return m_wfLevels.agcRunLevel(); }
    /// Test seam: the clock the waterfall levels, the hold and the noise
    /// floor read, in milliseconds. Empty gives the system clock back.
    void setDisplayClockForTest(std::function<qint64()> nowMs)
    {
        m_displayClockForTest = std::move(nowMs);
    }

    /// Clear the spectrum + waterfall avenger accumulators.  Use on bin-source
    /// changes that are abrupt enough that fading from the previous source's
    /// state via LogRecursive smoothing produces visible artifacts (e.g. MOX
    /// rise: the trace pipeline averages over ~3 s and during that window the
    /// trace shows a blend of pre-MOX RX state and post-MOX TX state, which
    /// looks like mic-driven content overlaid on the tune tone).
    void clearAvengers()
    {
        m_spectrumReducer.clearAveraging();
        m_waterfallReducer.clearAveraging();
        m_extendedSpectrumAvenger.clear();
        m_extendedWaterfallAvenger.clear();
    }

    // The pan's saved choice, separate from whether a measured Clarity
    // level is currently driving its waterfall (clarityActive()).
    void setClarityEnabled(bool on);
    bool clarityEnabled() const { return m_clarityEnabled; }
    void setClarityActive(bool on);
    bool clarityActive() const { return m_clarityActive; }

    // Display pill plan Task 3: the noise floor Clarity measured for this
    // pan, in dBm. While Clarity is active the waterfall's Black Level and
    // Color Gain place the colour range against it
    // (core/spectrum/ClarityLevels.h): 50 puts black at the floor and up is
    // blacker. Runtime state: never saved, never announced. A value that is
    // not finite is ignored. clarityFloorDbm() is NaN until the first one
    // arrives, and until then an active Clarity colours with its own
    // threshold pair.
    void  setClarityFloorDbm(float noiseFloorDbm);
    float clarityFloorDbm() const { return m_clarityFloorDbm; }
    // Display pill plan Task 12: the floor to move toward. The drawn floor
    // (clarityFloorDbm()) goes part of the way on every waterfall row
    // (pushWaterfallRow), so a new Clarity level arrives as a glide and not
    // as a step. setClarityFloorDbm() above is the immediate form, for a
    // band-change snap and the first level, and it sets the target too.
    // With no drawn floor yet the target is taken at once. A value that is
    // not finite is ignored.
    void  setClarityFloorTargetDbm(float noiseFloorDbm);

    // Issue #230 fix — Thetis-faithful split between persistent user
    // thresholds (above) and runtime render-active thresholds (below).
    // From Thetis display.cs:6575-6594 [v2.10.3.13]: the render path
    // Upstream tags preserved: //MW0LGE (from cited display.cs:6588) [v2.10.3.15]
    // seeds per-draw locals from the persistent fields then lets AGC /
    // NF-AGC / "Use spectrum min/max" / Clarity override the locals.
    // Active getters are exposed for the regression test; no public
    // setter — runtime layers set these via the dedicated paths below
    // and composeWaterfallActiveThresholds().
    float wfActiveLowThreshold()  const { return m_wfActiveLowThreshold;  }
    float wfActiveHighThreshold() const { return m_wfActiveHighThreshold; }
    // The low and high dBm the waterfall palette spans right now: the TX
    // pair under the MOX overlay; with Clarity active, the black point and
    // span Black Level and Color Gain set against the noise floor; else the
    // active Low and High with no Black Level or Color Gain offset.
    // dbmToRgb and traceWaterfallState both call it.
    std::pair<float, float> effectiveWaterfallLevels() const;

    // Clarity controller writes the render-active mirror only — never
    // the persisted user field. Modeled on the AGC path in
    // Thetis display.cs:6584 [v2.10.3.13] where the AGC running min
    // (_RX1waterfallPreviousMinValue) is a runtime field separate from
    // waterfall_low_threshold.
    void setClarityWaterfallThresholds(float low, float high);

    // Threshold composition pulled out of pushWaterfallRow() so the
    // regression test can drive it headlessly. Mutates the active
    // mirror and the AGC running-envelope state; never the persistent
    // user fields. Mirrors the Thetis local-variable composition at
    // display.cs:6575-6594 [v2.10.3.13].
    void composeWaterfallActiveThresholds(const QVector<float>& wfPixelsDbm);

    // JJ's ruling of 2026-09-28: a remote window's waterfall AGC and NF-AGC
    // use the Core's levels (display extras, the same the phone draws with),
    // not a follower of their own run on rows the Core has already clamped
    // to the dBm window. RemoteMediaController turns this on for a Core that
    // offers display extras. While on, with AGC on (NF-AGC is an option
    // under AGC, so AGC off means the stored levels, as it does locally)
    // and Clarity not setting the levels, the waterfall colours against the
    // last levels the Core sent (the stored levels until the first arrive)
    // and the local follower does not run.
    void setCoreWaterfallLevelsAvailable(bool available);
    bool coreWaterfallLevelsAvailable() const { return m_coreWfLevelsAvailable; }
    /// The Core's levels are the ones in use now.
    bool coreWaterfallLevelsInUse() const
    {
        // NF-AGC is an option under AGC (WaterfallLevelFollower::compose):
        // with AGC off the levels are the stored ones, here as locally.
        return m_coreWfLevelsAvailable && !m_clarityActive && m_wfAgcEnabled;
    }
    /// The Core's levels for the latest waterfall line (NSDX section 0x08).
    void setCoreWaterfallLevels(float lowDbm, float highDbm);
    /// Last accepted Core pair, also retained by the renderer while a new
    /// display context is pending. Distinct from Clarity's active pair.
    std::optional<std::pair<float, float>> coreWaterfallLevels() const
    {
        return m_coreWfLevels;
    }
    // NF-AGC: auto-track waterfall thresholds to noise floor + offset.
    void setWaterfallNFAGCEnabled(bool on);
    bool waterfallNFAGCEnabled() const { return m_wfNfAgcEnabled; }
    // The offset in whole dB: Setup's control, the Display pill's slider
    // (rulings wave R6) and the value a remote window sends its Core. The
    // getter rounds the 0.1 dB value below.
    void setWaterfallAGCOffsetDb(int db);
    int  waterfallAGCOffsetDb() const;
    // Display pill plan Task 17 (design S7): the value is kept in tenths
    // of a dB, -600 to 600, saved under WaterfallAGCOffsetDb as a number of
    // dB and given to this pan's own level follower. Both setters announce
    // "WaterfallAGCOffsetDb". Rulings wave R6: no control sets a fraction
    // any more (the pill's slider is whole dB), and a stored fraction is
    // rounded to the nearest dB when the settings are read.
    void setWaterfallAGCOffsetTenthsDb(int tenths);
    int  waterfallAGCOffsetTenthsDb() const { return m_wfNfAgcOffsetTenthsDb; }
    // Stop-on-TX: pause pushWaterfallRow() while TX is active.
    void setWaterfallStopOnTx(bool on);
    bool waterfallStopOnTx() const { return m_wfStopOnTx; }

    // NB Blank: a waterfall row whose level (the median of its bins, in
    // dB) jumps over a rolling baseline (a wideband burst that would draw a
    // stripe) is replaced by the last accepted row, at most two rows in a
    // row (WaterfallImpulseBlanker). Per pan, keys WaterfallBlankingEnabled
    // (default off) and WaterfallBlankingThresholdDb (default 6).
    // From AetherSDR src/gui/SpectrumWidget.h:412-413 [@1e0718ad]
    // NereusSDR design (follow-up wave F1): the threshold is in dB, 1 to
    // 20: how far above the baseline a row's level (the median of its
    // pixels in dB) must be to be replaced. AetherSDR's is a factor of raw tile intensity, which has
    // no dB meaning; it is not carried over and its key
    // (WaterfallBlankingThreshold) is no longer read.
    bool waterfallBlankingEnabled() const { return m_wfBlankerEnabled; }
    void setWaterfallBlankingEnabled(bool on);
    int  waterfallBlankingThresholdDb() const { return m_wfBlankerThresholdDb; }
    void setWaterfallBlankingThresholdDb(int db);
    // Rows NB Blank replaced, this widget's life (diagnostics only).
    quint64 waterfallRowsBlanked() const { return m_wfRowsBlanked; }

    // The grid's and the frequency scale's spacing: 0 is Auto, otherwise
    // one of kFreqGridSpacingsHz. Any other value is refused. Per pan, key
    // DisplayFreqGridSpacing (default 0).
    // From AetherSDR src/gui/SpectrumWidget.h:464 [@1e0718ad]
    // NereusSDR, based on AetherSDR: Hz, not kHz, like every other
    // frequency this widget holds.
    int  freqGridSpacingHz() const { return m_freqGridSpacingHz; }
    void setFreqGridSpacingHz(int hz);
    // From AetherSDR src/gui/SpectrumOverlayMenu.cpp:1831 [@1e0718ad]
    // (1, 2, 5, 10, 25, 50 and 100 kHz).
    static constexpr std::array<int, 7> kFreqGridSpacingsHz{
        1000, 2000, 5000, 10000, 25000, 50000, 100000};

    // The frequency scale's text size in pixels, 8 to 14. Per pan, key
    // DisplayFreqScaleFontPx (default 10, the size it always was).
    // From AetherSDR src/gui/SpectrumWidget.h:465 [@1e0718ad]
    // NereusSDR, based on AetherSDR: pixels, not points, because this
    // widget's scale text has always been set in pixels.
    int  freqScaleFontPx() const { return m_freqScaleFontPx; }
    void setFreqScaleFontPx(int px);
    // The scale strip's height: kFreqScaleH, more when the text needs it.
    int  freqScaleHeight() const;

    // ── 3DSS stacked-trace mode ───────────────────────────────────────────
    // Persistence (3D Stacked-Trace Spectrum Plan Task 14): render mode,
    // gain, row span, angle and slice-shadow persist per panadapter here
    // (loadSettings()/saveSettings(), settingsKey() convention) -- see
    // DisplaySpectrumRenderMode / Display3DGain / Display3DSpan /
    // Display3DAngle / Display3DSliceShadow. 3D Floor is the exception: it
    // persists per BAND instead, on PanadapterModel::dss3DFloorDepthForBand,
    // because it is anchored to the measured noise floor, which is a
    // per-band property. setDssFloorDepth()/dssFloorDepth() below remain
    // the live runtime mirror the 3D renderer reads every frame; they do
    // not write an AppSettings key themselves.
    void setSpectrumRenderMode(int mode);
    int  spectrumRenderMode() const { return static_cast<int>(m_spectrumRenderMode); }
    void setDssFloorDepth(int dB);
    int  dssFloorDepth() const { return m_dssFloorDepth; }
    void setDssGain(int pct);
    int  dssGain() const { return m_dssGain; }
    void setDssRowSpan(int pct);
    int  dssRowSpan() const { return m_dssRowSpan; }
    void setDssAngle(int pct);
    int  dssAngle() const { return m_dssAngle; }

    // 3D Speed (3D Stacked-Trace Spectrum Plan Task 24, NereusSDR-
    // original): how many waterfall rows each pushed 3D row covers. 0..10,
    // persisted per panadapter as Display3DSpeed, default 0. 0 clamps to
    // "Match" -- see effectiveDssRowDivider() below for what that resolves
    // to. Design doc section 4.5.
    void setDssRowDivider(int n);
    int  dssRowDivider() const { return m_dssRowDivider; }
    // The divider actually in effect: dssRowDivider() when it is a manual
    // 1..10 value, otherwise the automatic Match value -- the waterfall's
    // own pixel height divided by kDssVisibleRows, rounded, clamped to
    // [1, kDssMaxAutoRowDivider]. Read live on every use (no cached value,
    // no resize hook), so a window or split-fraction change is followed
    // with no extra plumbing.
    int  effectiveDssRowDivider() const;
    // Ceiling on the automatic (Match) divider, so an extreme window size
    // cannot make the 3D surface crawl to a near-standstill.
    static constexpr int kDssMaxAutoRowDivider = 64;

    void setThreeDSliceDepth(bool on);
    bool threeDSliceDepth() const { return m_threeDSliceDepth; }
    DssShape dssShape() const { return dssShapeForAngle(m_dssAngle); }

    // Maps a normalised 0..1 surface strength to a colour, gamma-shaped by
    // "3D Gain" alone. Deliberately independent of the waterfall gain /
    // black-level knobs -- see the .cpp for why. Public so it is directly
    // testable and so uploadDssPaletteLut() (GPU-only) has a CPU-callable
    // sibling.
    QRgb dssStrengthToRgb(float s) const;
    // Folds the inputs that actually change dssStrengthToRgb()'s output, so
    // the palette LUT re-bakes only on a real change.
    quint64 dssPaletteToken() const;

    // The 3DSS surface baseline: the measured noise floor (NoiseFloorTracker
    // / m_nfLerpAverage) offset down by 3D Floor, so the stack keeps a
    // constant apparent height as band conditions move.
    float dssFloorDbm() const;
    // The dBm display span the surface height maps across.
    float dssSpanDb() const;
    // dssSpanDb() rounded to the nearest 0.5 dB. The scale (drawDbmScale3D),
    // the CPU fallback surface (buildDssImage), and the GPU mesh's rangeDb
    // uniform (writeDssMeshUbo) must all read the SAME rounded value, or
    // their pixel mappings disagree by up to 0.25 dB whenever the configured
    // dBm range is not already an exact 0.5 dB multiple -- Task 11 fast-
    // follow, closing a gap where writeDssMeshUbo wrote the unrounded value.
    float dssRoundedSpanDb() const;
    // The dBm span the COLOUR mapping (not the height mapping) reads
    // across: dssSpanDb() capped at kDssColorSpanDb. A stable aperture
    // independent of the Ref-level height span, so a wide dBm range does
    // not stretch the colormap thinner than kDssColorSpanDb and wash the
    // surface out relative to the reference implementation -- see the
    // review-round fix note on writeDssMeshUbo()'s colorRangeDb field.
    float dssColorRangeDb() const;
    // Windows the off-screen DDC bins of `fullBins` (the same FFT frame
    // that produced the on-screen row) into the wide channel that fills the
    // wedge beside the exact-channel trapezoid. Public so it is directly
    // testable without a live FFT pipeline.
    QVector<float> buildDssWideRow(const QVector<float>& fullBins,
                                   double& wideCenterMhzOut,
                                   double& wideBandwidthMhzOut) const;
    // Wedge-closing row span for the current 3D Span setting, scaled
    // against whatever wide-channel overhang is actually on screen.
    float dssRowSpanTarget(double targetBandwidthMhz) const;

    // One slice passband darkened onto the 3D surface (dss_mesh.frag's
    // applySliceShadow). low/high/centre are in the same [0,1] viewport-unit
    // space hzToX() uses (0 = the on-screen bandwidth's left edge, 1 = its
    // right edge): centreUnit is the slice's own CARRIER frequency, matching
    // drawSliceMarker()'s separate VFO-centre-line pixel, not the passband
    // midpoint. alpha/centreAlpha are the band-fill and centre-cue mix
    // strengths the shader applies; cue is the slice's accent colour tinting
    // both. From AetherSDR SpectrumWidget.cpp:14216-14366 [@1872028c]
    // (writeShadowSlot's per-descriptor fields).
    struct DssShadowBand {
        float lowUnit{0.0f};
        float highUnit{0.0f};
        float centreUnit{0.0f};
        float alpha{0.0f};
        QColor cue;
        float centreAlpha{0.0f};
    };

    // Maps every visible slice's passband (sliceMarkerGeometry()) onto
    // viewport units for the DSS shader's slice-shadow decal, clamped to the
    // shader's fixed 8-entry shadowBands/shadowStyles arrays. Empty when 3D
    // Slice Shadow (threeDSliceDepth()) is off, or when a slice's passband
    // never touches the visible viewport.
    QVector<DssShadowBand> buildDssShadowBands() const;

    // Test seams. pushWaterfallRow() is private and normally driven by the
    // WaterfallTicker thread; these let the row-tee placement be proven
    // without standing up a ticker or a QRhi context.
    void pushWaterfallRowForTest(const QVector<float>& wfPixelsDbm) {
        pushWaterfallRow(wfPixelsDbm);
    }
    void pushImdReceiverRowForTest(const QVector<float>& wfPixelsDbm) {
        pushWaterfallRow(wfPixelsDbm, true, ++m_imdAcceptedFrameOrdinal);
    }
    const ImdWaterfallAnnotations& imdWaterfallAnnotationsForTest() const {
        return m_imdWaterfallAnnotations;
    }
    quint64 waterfallRowSequenceForTest() const { return m_wfNextRowSequence; }
    std::optional<qreal> imdEventYForTest(quint64 sequence, qreal glideRows = 0.0) const {
        return imdEventY(sequence, glideRows);
    }
    void setTxActiveForTest(bool on) { m_txActiveForTest = on; }
    int  dssRowsPushedForTest() const { return m_dssRowsPushed; }
    // The receiver whose frame this pan took last (-1 before the first).
    int  localSpectrumSourceForTest() const { return m_localSpectrumSource; }
    // R-R3-21: the remote row queue, and the ticker run by hand.
    int  remoteRowQueueDepthForTest() const { return int(m_remoteRowQueue.size()); }
    quint64 remoteRowsDroppedForTest() const { return m_remoteRowsDropped; }
    quint64 remoteRowsPushedForTest() const { return m_remoteRowsPushed; }
    QVector<float> lastRemoteRowPushedForTest() const { return m_lastRemoteRowPushed; }
    void setWaterfallTickerPausedForTest(bool paused) { m_waterfallTickerPausedForTest = paused; }
    // The period the waterfall ticker runs at, in ns.
    qint64 waterfallTickerPeriodNsForTest() const;
    double waterfallRowPeriodMsForTest() const { return waterfallRowPeriodMs(); }
    void tickWaterfallForTest() { onWaterfallTick(); }
    // The remote row clock run by hand, at a time of the test's choosing.
    void setRemoteRowNowNsForTest(qint64 ns) { m_remoteRowNowNsForTest = ns; }
    // A drawn frame's pump (paintEvent), and the ticker's.
    void pumpRemoteWaterfallForTest() { pumpRemoteWaterfallOnFrame(); }
    void tickRemoteWaterfallPumpForTest() { pumpRemoteWaterfallOnTick(); }
    const RemoteWaterfallRowClock& remoteRowClockForTest() const { return m_remoteRowClock; }
    qint64 remoteRowNowNsForTest() const { return remoteRowNowNs(); }
    // When the queued row at index is due, on remoteRowNowNs()'s clock.
    qint64 remoteRowDueNsForTest(int index) const { return m_remoteRowQueue.at(index).dueNs; }
    // Parity Task 18 (B3.5): the waterfall's rewind history, in rows.
    int  waterfallHistoryRowsForTest() const { return m_wfHistoryRowCount; }
    const QImage& liveWaterfallForTest() const { return m_waterfall; }
    int liveWaterfallWriteRowForTest() const { return m_wfWriteRow; }
    // The rewind history image itself, and how many times its whole
    // image has been rescaled to a new width (once per resize that comes to
    // rest, never once per row while the width is still changing).
    const QImage& waterfallHistoryForTest() const { return m_waterfallHistory; }
    int waterfallHistoryWriteRowForTest() const { return m_wfHistoryWriteRow; }
    int waterfallHistoryOffsetRowsForTest() const { return m_wfHistoryOffsetRows; }
    quint64 waterfallHistoryRescalesForTest() const { return m_wfHistoryRescales; }
    bool waterfallHistoryResizePendingForTest() const {
        return m_historyResizeTimer && m_historyResizeTimer->isActive();
    }
    // 3D Speed (Task 24) test seams.
    int  effectiveDssRowDividerForTest() const { return effectiveDssRowDivider(); }
    int  dssFoldCountForTest() const { return m_dssFoldCount; }
    float dssScrollIncrementForTest(int deltaMs) const { return dssScrollIncrement(deltaMs); }
    // The waterfall image height in pixels, what effectiveDssRowDivider()'s
    // automatic branch actually divides by kDssVisibleRows. Did not exist
    // before Task 24; there was no prior reason for a test to read it.
    int  waterfallHeightForTest() const { return m_waterfall.height(); }
    // Forwards DssRenderer's own existing rowDataRing()/headRing() row
    // accessors (both already public and used by tst_dss_renderer_ring.cpp
    // against a standalone DssRenderer) so a test can read one column of
    // the newest ring row without a live GPU paint. column must be in
    // [0, kDssCols).
    float dssNewestRowColumnForTest(int column) const {
        return m_dss.rowDataRing(m_dss.headRing())[column];
    }
    // NoiseFloorTracker runs from live FFT frames; this seam drives
    // dssFloorDbm() directly so the floor-anchoring math is testable
    // without standing up the noise-floor pipeline.
    void setMeasuredNoiseFloorForTest(float dbm) { m_noiseFloor.setLerpAverage(dbm); }
    // Test only: the share of the Core's trace average its starting value
    // held at the last frame (0 once warm; noteRemoteTraceWarmUp).
    double remoteTraceStartShareForTest() const { return m_remoteTraceStartShare; }
    // Test only (R-DISP-HOLD): the codec context of the remote display this
    // pan accepts frames for (all zero while it accepts none).
    const DisplayCodecContext& remoteCodecForTest() const { return m_remoteCodec; }
    // The spectrum's rect in the current layout (logical pixels), so a
    // native test can tell spectrum rows of a grabbed frame from the
    // frequency scale and waterfall rows below them.
    QRect spectrumRectForTest() const { return spectrumLayout().spectrum; }
    // The waterfall's rect in the same layout, so a native test can find
    // the waterfall's first and last rows in a grabbed frame.
    QRect waterfallRectForTest() const { return spectrumLayout().waterfall; }
    // Pauses or resumes the waterfall, as scrolling back on the time scale
    // and its LIVE button do.
    void setWaterfallLiveForTest(bool live) { setWaterfallLive(live); }
    // Holds the GPU waterfall's glide at `rows` (0 = at rest on the newest
    // row, 1 = a whole row short of it) in place of the clock, so a native
    // test can draw a frame at a chosen point between two rows. A negative
    // value hands the glide back to the clock.
    void setWaterfallGlideForTest(float rows) { m_wfGlideRowsForTest = rows; }

    // Which rows of the GPU dynamic overlay texture (device pixels, from
    // the top) renderGpuFrame() clears and uploads. The texture covers the
    // whole widget but is rebuilt only over the spectrum band, so this
    // keeps the rows that may still hold pixels from an earlier frame: all
    // of a freshly created texture (its memory is undefined), or the band
    // last uploaded. Each upload covers those rows as well as the current
    // band, so a spectrum that got shorter leaves no line below it.
    // NereusSDR-original; tst_spectrum_dynamic_overlay_rows drives it.
    class DynamicOverlayRows {
    public:
        // The texture was (re)created with `textureRows` rows.
        void textureCreated(int textureRows)
        {
            m_textureRows = qMax(1, textureRows);
            m_staleRows = m_textureRows;
        }
        // True when the rows that may hold earlier pixels differ from the
        // band, so the overlay is rebuilt even if no feature asked for it.
        bool needsRebuild(int bandRows) const { return m_staleRows != clampRows(bandRows); }
        // Rows to clear and upload this frame for a band of `bandRows`.
        // After that upload only the band can hold pixels.
        int beginUpload(int bandRows)
        {
            const int band = clampRows(bandRows);
            const int rows = qMax(band, m_staleRows);
            m_staleRows = band;
            return rows;
        }

    private:
        int clampRows(int rows) const { return qBound(1, rows, m_textureRows); }
        int m_textureRows{1};
        int m_staleRows{1};
    };
    // updateSpectrumLinear() is the only production writer of
    // m_lastFullBinsDbm (see its definition); this seam drives
    // buildDssWideRow()'s production call path (via pushDssRow()) without
    // standing up the FFT pipeline.
    void setFullBinsForTest(const QVector<float>& binsDbm) {
        m_lastFullBinsDbm = binsDbm;
    }
    double dssNewestRowWideBandwidthForTest() const {
        return m_dss.rowWideBandwidthMhzAtAge(0);
    }
    // Public one-line forward (drawDbmScale3D is private) so the 3D dBm
    // scale can be painted onto an off-widget QImage/QPainter without a
    // live paintEvent(), following the drawSpotMarkersForTest /
    // drawNotchMarkersForTest convention above.
    void drawDbmScale3DForTest(QPainter& p, const QRect& specRect, float floorDbm) {
        drawDbmScale3D(p, specRect, floorDbm);
    }
    // drawDbmScaleLabels' rangeDb<=0 guard is unreachable through
    // drawDbmScale3DForTest() alone: dssSpanDb() floors at 1.0f (Task 9), so
    // drawDbmScale3D() can never compute a non-positive span in production.
    // This seam drives the guard directly so the degenerate-span case can be
    // proven, not just argued.
    void drawDbmScaleLabelsForTest(QPainter& p, const QRect& specRect,
                                   float topDbm, float rangeDb) {
        drawDbmScaleLabels(p, specRect, topDbm, rangeDb);
    }
    // Lets a test independently reconstruct drawDbmScale3D's exact output
    // (chrome, then labels at floorDbm+dssRoundedSpanDb()) from public
    // pieces alone, to cross-check that drawDbmScale3D really uses the
    // rounded span rather than an independently-diverged computation.
    void drawDbmScaleChromeForTest(QPainter& p, const QRect& specRect) {
        drawDbmScaleChrome(p, specRect);
    }

    void setWfOpacity(int percent);          // 0..100
    int  wfOpacity() const { return m_wfOpacity; }
    void setWfUpdatePeriodMs(int ms) override;
    int  wfUpdatePeriodMs() const { return m_wfUpdatePeriodMs; }

    /// Existing visible and retained row state; no allocation or refresh.
    int visibleWaterfallRows() const { return m_waterfall.height(); }
    int waterfallHistoryRows() const { return m_wfHistoryRowCount; }
    int waterfallHistoryCapacityRows() const;
    int maxWaterfallHistoryOffsetRows() const;
    qint64 effectiveWaterfallHistorySeconds() const;
    qint64 waterfallHistoryMs() const  { return m_waterfallHistoryMs; }
    void   setWaterfallHistoryMs(qint64 ms);
    bool   wfLive() const              { return m_wfLive; }

    // Ported from setup.cs:7801 Display.WaterfallUseRX1SpectrumMinMax.
    void setWfUseSpectrumMinMax(bool on);
    bool wfUseSpectrumMinMax() const { return m_wfUseSpectrumMinMax; }

    // Ported from setup.designer.cs:34428 comboDispWFAveraging / AverageModeWF.
    void setWfAverageMode(AverageMode m);
    AverageMode wfAverageMode() const { return m_wfAverageMode; }

    // Timestamp overlay (NereusSDR extensions W8/W9).
    enum class TimestampPosition : int { None = 0, Left, Right, Count };
    enum class TimestampMode     : int { UTC = 0, Local, Count };
    void setWfTimestampPosition(TimestampPosition p);
    TimestampPosition wfTimestampPosition() const { return m_wfTimestampPos; }
    void setWfTimestampMode(TimestampMode m);
    TimestampMode wfTimestampMode() const { return m_wfTimestampMode; }

    // Filter / zero-line overlays on the waterfall.
    // From setup.cs:1048-1052 Display.ShowRXFilterOnWaterfall / ShowTXFilterOnRXWaterfall
    // / ShowRXZeroLineOnWaterfall / ShowTXZeroLineOnWaterfall.
    void setShowRxFilterOnWaterfall(bool on);
    bool showRxFilterOnWaterfall() const { return m_showRxFilterOnWaterfall; }
    void setShowTxFilterOnRxWaterfall(bool on);
    bool showTxFilterOnRxWaterfall() const { return m_showTxFilterOnRxWaterfall; }
    void setShowRxZeroLineOnWaterfall(bool on);
    bool showRxZeroLineOnWaterfall() const { return m_showRxZeroLineOnWaterfall; }
    void setShowTxZeroLineOnWaterfall(bool on);
    bool showTxZeroLineOnWaterfall() const { return m_showTxZeroLineOnWaterfall; }

    // ---- Grid / scales renderer controls (Phase 3G-8 commit 5) ----

    void setGridEnabled(bool on);
    bool gridEnabled() const { return m_gridEnabled; }

    // Right-edge dBm scale strip visibility. When false, the strip is
    // hidden and the spectrum fills the full widget width.
    void setDbmScaleVisible(bool on);
    bool dbmScaleVisible() const { return m_dbmScaleVisible; }

    // Bandplan overlay (Phase 3G RX Epic sub-epic D)
    void setBandPlanManager(NereusSDR::BandPlanManager* mgr);
    // The plan manager whose active plan the strip draws (non-owning).
    NereusSDR::BandPlanManager* bandPlanManager() const { return m_bandPlanMgr; }
    void setBandPlanFontSize(int pt);             // 0 = off
    int  bandPlanFontSize() const { return m_bandPlanFontSize; }
    bool bandPlanVisible() const { return m_bandPlanFontSize > 0; }

    void setShowZeroLine(bool on);
    bool showZeroLine() const { return m_showZeroLine; }

    void setShowFps(bool on);
    bool showFps() const { return m_showFps; }

    // 2026-05-26 KG4VCF perf instrumentation: toggle the in-spectrum
    // perf overlay (paint/gap/fft/overlay timings + audio underruns
    // + UDP drops + memory pressure).  Persisted via AppSettings
    // "ShowPerfOverlay"; View -> Performance Overlay wires here.
    void setShowPerfOverlay(bool on);
    bool showPerfOverlay() const { return m_showPerfOverlay; }
    // Parity ruling C13: in a remote window, the Core's counters the
    // overlay shows below this computer's, each group headed. Read at each
    // overlay refresh; empty (the default) in a local window.
    void setCorePerfLinesProvider(std::function<QStringList()> provider);
    /// The overlay's lines as drawn (this computer's, then the Core's).
    QStringList perfOverlayLines() const;

    // B8 Task 21: cursor frequency readout visibility.
    // Default true (matches the previously always-on behavior).
    void setCursorFreqVisible(bool on);
    bool cursorFreqVisible() const noexcept { return m_showCursorFreq; }

    void setFreqLabelAlign(FreqLabelAlign a);
    FreqLabelAlign freqLabelAlign() const { return m_freqLabelAlign; }

    // Test seams (tst_spectrum_widget_additions): the grid and scale
    // spacing the two draw functions use, the scale strip's rectangle, and
    // the scale drawn into a painter outside the live paintEvent().
    void drawFrequencyGridForTest(QPainter& painter, const QRect& rect) {
        drawGrid(painter, rect);
    }
    double gridFreqStepHzForTest() const  { return gridFreqStepHz(); }
    // The dB values drawGrid() draws its horizontal lines at, bottom up.
    QVector<float> dbGridLinesForTest() const
    {
        QVector<float> lines;
        forEachDbGridLine([&lines](float dbm) { lines.append(dbm); });
        return lines;
    }
    double scaleFreqStepHzForTest() const { return scaleFreqStepHz(); }
    QRect  freqScaleRectForTest() const   { return spectrumLayout().frequency; }
    void   drawFreqScaleForTest(QPainter& p, const QRect& r) { drawFreqScale(p, r); }
    // How many scale labels the last drawFreqScale() drew.
    int    freqScaleLabelsDrawnForTest() const { return m_freqScaleLabelsDrawn; }

    // Display pill plan Task 5 test seams. The trace as the QPainter path
    // draws it and the background both paths paint, drawn into a caller's
    // painter (an offscreen grab() of a QRhiWidget build shows neither),
    // and the trace and fill vertices exactly as renderGpuFrame() uploads
    // them.
    void setRenderedPixelsForTest(const QVector<float>& dbm) { m_renderedPixels = dbm; }
    void drawSpectrumForTest(QPainter& p, const QRect& r) { drawSpectrum(p, r); }
    bool drawsSpectrumBackgroundForTest() const { return drawsSpectrumBackground(); }
    void paintSpectrumBackgroundForTest(QPainter& p, const QRect& r) {
        paintSpectrumBackground(p, r);
    }
    bool backgroundImageLoadedForTest() const { return !m_bgImage.isNull(); }
#ifdef NEREUS_GPU_SPECTRUM
    struct FftTraceVerticesForTest {
        QVector<float> line;   // one vertex per display pixel
        QVector<float> fill;   // two per display pixel: at the trace, at the base
        int stride{kFftVertStride};  // floats per vertex: x, y, r, g, b, a
    };
    FftTraceVerticesForTest fftTraceVerticesForTest() const {
        FftTraceVerticesForTest v;
        buildFftTraceVertices(static_cast<int>(qMin<qsizetype>(m_renderedPixels.size(),
                                                               kMaxFftBins)),
                              fftTraceFloorNdc(spectrumLayout().spectrum),
                              true, v.line, v.fill);
        return v;
    }
    // The live trace as renderGpuFrame() uploads it for a spectrum viewport
    // of this size in device pixels at this pixel ratio: what
    // SpectrumTraceRibbon.h builds from the line vertices above (four
    // vertices per segment over one device pixel wide, the line's own
    // vertices at or under it).
    QVector<float> fftTraceRibbonForTest(float viewportWidthPx, float viewportHeightPx,
                                         float devicePixelRatio) const;
    // The peak hold vertices as renderGpuFrame() uploads them, for a peak
    // hold set here (one value per display pixel).
    void setPeakHoldPixelsForTest(const QVector<float>& dbm) { m_pxPeakHold = dbm; }
    QVector<float> fftPeakVerticesForTest() const {
        QVector<float> v;
        buildFftPeakVertices(static_cast<int>(qMin<qsizetype>(m_pxPeakHold.size(),
                                                              kMaxFftBins)),
                             fftTraceFloorNdc(spectrumLayout().spectrum), v);
        return v;
    }
#endif
    // Where the grid, the dBm scale and every overlay put this dBm value in
    // the spectrum area (dbmToYf), in logical pixels.
    float gridYForTest(float dbm) const { return dbmToYf(dbm, spectrumLayout().spectrum); }

    // Configurable grid/text/zero-line/band-edge colours. Previously
    // hardcoded. Ported from Thetis setup.cs:1040-1044 Display.GridColor
    // / GridPenDark / HGridColor / GridTextColor / GridZeroColor and
    // display.cs:1941 BandEdgeColor.
    void setGridColor(const QColor& c);
    QColor gridColor() const { return m_gridColor; }
    void setGridFineColor(const QColor& c);
    QColor gridFineColor() const { return m_gridFineColor; }
    void setHGridColor(const QColor& c);
    QColor hGridColor() const { return m_hGridColor; }
    // Approved scope: persisted per pan in DisplayGridStep, 1 to 40 dB.
    // Historical model-routing description follows.
    // The spacing of the horizontal dB grid lines, in whole dB: Setup's
    // "dB Step" (PanadapterModel::gridStep, pushed here by PanModels). A
    // step below 1 is refused. Not saved here: the model owns the setting.
    // From Thetis setup.cs:7920-7923 [v2.10.3.15]: udDisplayGridStep sets
    // Display.SpectrumGridStep, and display.cs:9653-9668 draws a line every
    // grid_step dB. The step is what is drawn; there is no automatic step.
    void setDbGridStep(int stepDb);
    int  dbGridStep() const { return m_dbGridStep; }
    void setGridTextColor(const QColor& c);
    QColor gridTextColor() const { return m_gridTextColor; }
    // Plan 4 D9c-1: zero-line color split into separate RX and TX colors.
    // RX default: red (Thetis convention).
    // TX default: amber (NereusSDR-original — distinguishes during split TX).
    void setRxZeroLineColor(const QColor& c);
    QColor rxZeroLineColor() const noexcept { return m_rxZeroLineColor; }

    void setTxZeroLineColor(const QColor& c);
    QColor txZeroLineColor() const noexcept { return m_txZeroLineColor; }

    /// Reset all user-customisable Plan 4 D9/D9c display colors to compile-time
    /// defaults.  Gives users an escape hatch from broken color combinations.
    /// Plan 4 D9c-3 — scoped to TX filter, RX filter, RX zero line, TX zero
    /// line only.  Plan 5+ may extend the scope.
    void resetDisplayColorsToDefaults();

    // Plan 4 D9c-4 — forward-compat scaffolding.  No paint code yet — these
    // colors light up only when:
    //   - TNF (Tracking Notch Filter) feature ships
    //   - SubRX (3F multi-pan / multi-RX) ships
    // Persisted now so user-customised colors survive across the version
    // that adds the feature.
    void setTnfFilterColor(const QColor& c);
    QColor tnfFilterColor() const noexcept { return m_tnfFilterColor; }

    void setSubRxFilterColor(const QColor& c);
    QColor subRxFilterColor() const noexcept { return m_subRxFilterColor; }

    void setBandEdgeColor(const QColor& c);
    QColor bandEdgeColor() const { return m_bandEdgeColor; }

    // ---- Task 2.3: Spectrum text overlays ----
    // Corner-text overlays: noise floor dBm, peak dBm @ MHz, bin width readout.
    // MHz cursor format is applied to the cursor-hover label in drawCursorInfo.

    // OverlayPosition: 4-corner placement for corner-text overlays.
    // NereusSDR-native enum; Thetis uses fixed positions (e.g. infoBar is top).
    enum class OverlayPosition { TopLeft, TopRight, BottomLeft, BottomRight };

    // formatCursorFreq — format a frequency value for the cursor label.
    // Always returns MHz format ("7.1735 MHz") — earlier integer-Hz path
    // ("7173500 Hz") was retired because it duplicated the visibility-only
    // toggle in SpectrumOverlayPanel and confused users (two controls with
    // the same name driving different state).  The Setup → Display →
    // Spectrum Defaults checkbox now controls visibility (m_showCursorFreq)
    // alongside the overlay-panel button — single source of truth.
    QString formatCursorFreq(double hz) const;

    // ShowBinWidth — toggle bin-width readout label in spectrum corner.
    // Displays sampleRate / fftSize in kHz (e.g. "11.719 Hz/bin").
    // From Thetis setup.cs:7061 lblDisplayBinWidth.Text [v2.10.3.13].
    void setShowBinWidth(bool on);
    bool showBinWidth() const { return m_showBinWidth; }
    // binWidthHz — returns current bin width; exposed for unit tests.
    double binWidthHz() const;

    // ShowNoiseFloor — render noise-floor estimate as corner text overlay.
    // From Thetis display.cs:2304-2308 ShowNoiseFloorDBM [v2.10.3.13];
    // rendered at display.cs:5440 in DrawSpectrumDX2D.
    void setShowNoiseFloor(bool on);
    bool showNoiseFloor() const { return m_showNoiseFloor; }
    void setShowNoiseFloorPosition(OverlayPosition pos);
    OverlayPosition showNoiseFloorPosition() const { return m_noiseFloorPosition; }

    // NF shift offset — From Thetis display.cs:5763 [v2.10.3.13] _fNFshiftDBM.
    // Operator-tunable shift in dB applied to the rendered NF level (line +
    // text + connector).  Clamped to [-12, +12]; default 0.
    void  setNFShiftDbm(float db);
    float nfShiftDbm() const { return m_nfShiftDbm; }

    // Fast-attack flag — From Thetis display.cs:917-927 [v2.10.3.13]
    // m_bFastAttackNoiseFloorRX1.  When true the line + text render gray
    // (m_noiseFloorFastColor) instead of red/yellow to signal the smoothed
    // average is still settling.  Caller (MainWindow) sets on band/freq
    // change and clears after attack-time elapses. Its minimum is
    // NoiseFloorFollower::kFastAttackMinMs; a MOX edge passes the un-key
    // hold length instead, through setDisplayKeyed().
    void setNoiseFloorFastAttack(bool on);
    bool noiseFloorFastAttack() const { return m_noiseFloor.fastAttack(); }

    // NF colour customisation — From Thetis display.cs:2316/2329 [v2.10.3.13]:
    //   noisefloor_color      = Color.Red     (line + box)
    //   noisefloor_color_text = Color.Yellow  (dBm label)
    // Fast-attack swap colour is NereusSDR-original (Thetis hard-codes gray).
    void   setNoiseFloorColor(const QColor& c);
    QColor noiseFloorColor() const { return m_noiseFloorColor; }
    void   setNoiseFloorTextColor(const QColor& c);
    QColor noiseFloorTextColor() const { return m_noiseFloorTextColor; }
    void   setNoiseFloorFastColor(const QColor& c);
    QColor noiseFloorFastColor() const { return m_noiseFloorFastColor; }

    // NF line width — From Thetis display.cs:2310 [v2.10.3.13]
    // m_fNoiseFloorLineWidth=1.0f.  Range 1..5 in NereusSDR.
    void  setNoiseFloorLineWidth(float w);
    float noiseFloorLineWidth() const { return m_noiseFloorLineWidth; }

    // ---- NF-aware grid (Task 2.9) ----
    // When enabled, the grid lower bound auto-tracks the live noise floor
    // estimate delivered via onNoiseFloorChanged(). Ported from Thetis
    // console.cs:46074-46086 [v2.10.3.13] GridMinFollowsNFRX1 / tmrAutoAGC_Tick.
    // RX1 scope dropped; NereusSDR applies as a global panadapter default
    // with per-pan override via ContainerSettings dialog (3G-6 pattern).
    // From Thetis setup.cs:24202-24213 [v2.10.3.13]
    // — RX1 scope dropped; NereusSDR applies as global panadapter default
    //   with per-pan override via ContainerSettings dialog (3G-6 pattern).
    void setAdjustGridMinToNoiseFloor(bool on);
    bool adjustGridMinToNoiseFloor() const { return m_adjustGridMinToNF; }
    // Level Cal: while the level calibration holds the follow off, every
    // pan saves `saved` (the user's value) for it instead of the live one,
    // so a quit or a crash in the middle of a run never stores the held
    // value. std::nullopt ends the hold. One setting for every pan, so one
    // hold for the process.
    // Display rebuild: that process hold now snapshots and restores each
    // pan's own choice, including a pan loaded while calibration runs.
    static void setGridFollowSaveHold(std::optional<bool> saved);
    static std::optional<bool> gridFollowSaveHold();

    // Offset added to the NF estimate to compute the new grid min.
    // From Thetis console.cs:46035 _RX1NFoffsetGridFollow = 5f [v2.10.3.13]
    // — NereusSDR uses a range of -60..+60 with default 0 (see design 2E).
    //   Thetis subtracts the offset (setPoint = nf - offset) with default +5;
    //   NereusSDR adds the offset (proposedMin = nf + offset) with default 0,
    //   preserving equivalent semantics when the user enters a negative value.
    void setNFOffsetGridFollow(int db);
    int  nfOffsetGridFollow() const { return m_nfOffsetGridFollow; }

    // When true, move grid max by the same delta to preserve the dB range.
    // From Thetis console.cs:46085 _maintainNFAdjustDeltaRX1 [v2.10.3.13].
    // NF grid range guard: abs incase //MW0LGE [2.9.0.7] [original inline comment from console.cs:46081]
    void setMaintainNFAdjustDelta(bool on);
    bool maintainNFAdjustDelta() const { return m_maintainNFAdjustDelta; }

    // Accessors for the current grid min/max in dBm (derived from
    // m_refLevel / m_dynamicRange). Read back by tests and GridScalesPage.
    int gridMin() const { return qRound(m_refLevel - m_dynamicRange); }
    int gridMax() const { return qRound(m_refLevel); }

    // Test helper: directly fire the NF-changed handler without needing
    // a live ClarityController. Exposed in tests only; production code
    // uses the onNoiseFloorChanged() slot via signal/slot connection.
    void testApplyNoiseFloor(float nfDbm) { onNoiseFloorChanged(nfDbm); }
    void gridFollowTickForTest() { gridFollowTick(); }

    // DispNormalize — normalize-to-1-Hz before display.
    // Routes to SetDisplayNormOneHz in the WDSP spectrum engine.
    // From Thetis specHPSDR.cs:291-293 [v2.10.3.13] NormOneHzPan property;
    // wired from setup.cs:18093-18099 chkDispNormalize_CheckedChanged.
    // Note: In Thetis this calls SpecHPSDRDLL.SetDisplayNormOneHz (a WDSP
    // call); NereusSDR stores the flag and propagates it to FFTEngine when
    // the WDSP spectrum engine is integrated (Task 5.x).
    void setDispNormalize(bool on);
    bool dispNormalize() const { return m_dispNormalize; }
    // Normalise is on and the spectrum detector is Average, Sample or RMS
    // (Thetis applies it only then; DisplayFollowers
    // normalizeAppliesToDetector). The stored choice is dispNormalize().
    bool normalizeActive() const;

    // ShowPeakValueOverlay — scan visible bins, render "Peak: X.X dBm @ Y.YYYY MHz"
    // as corner text. Refreshed on a timer throttled by m_peakTextDelayMs.
    // From Thetis console.cs:20073 PeakTextDelay default=500ms [v2.10.3.13].
    // Upstream tags preserved: //MW0LGE (from cited console.cs:20070) [v2.10.3.15]
    // PeakTextColor default DodgerBlue from console.cs:20278 [v2.10.3.13].
    void setShowPeakValueOverlay(bool on);
    bool showPeakValueOverlay() const { return m_showPeakValueOverlay; }
    void setPeakValuePosition(OverlayPosition pos);
    OverlayPosition peakValuePosition() const { return m_peakValuePosition; }
    void setPeakTextDelayMs(int ms);
    int  peakTextDelayMs() const { return m_peakTextDelayMs; }
    void setPeakValueColor(const QColor& c);
    QColor peakValueColor() const { return m_peakValueColor; }

    // ---- HIGH SWR / PA safety overlay ----
    // Ported from Thetis display.cs:4183-4201 [v2.10.3.13]
    // Mirrors the DX2D "HIGH SWR" warning block: red centred text +
    // 6 px red border around the spectrum area. When `foldback` is true,
    // "\n\nPOWER FOLD BACK" is appended to the text per display.cs:4187-4194.
    // //MW0LGE_21k8  [original inline comment from display.cs:4213]
    void setHighSwrOverlay(bool active, bool foldback) noexcept override;
    bool isHighSwrOverlayActive() const noexcept { return m_highSwrActive; }
    bool isHighSwrFoldback()      const noexcept { return m_highSwrFoldback; }

    // ---- MOX / TX mode overlay (H.1, Phase 3M-1a) ----
    //
    // Ported from Thetis display.cs:1569-1593 [v2.10.3.13] Display.MOX setter.
    // When MOX is active, a red 3 px border is drawn around the spectrum
    // panel indicating TX-mode. Matches Thetis's use of tx_vgrid_pen /
    // tx_band_edge_pen (display.cs:2086, 1955 [v2.10.3.13]) which colour the
    // grid red during TX. NereusSDR renders a simpler border tint — full
    // grid colour recolouring is deferred to 3M-3.
    //
    // setTxAttenuatorOffsetDb: when ATT-on-TX is active (F.2 path) the
    // dBm scale shifts by the attenuator value so the displayed noise floor
    // stays calibrated.  Matches Thetis display.cs:4840 [v2.10.3.13]:
    //   if (!local_mox) fOffset += rx1_preamp_offset;
    // (offsets applied only in RX; TX path uses its own cal offset).
    //
    // setTxFilterVisible: stub activated from the DisplayPage
    // DrawTXFilter flag (display.cs:2481 [v2.10.3.13]).  The waterfall
    // filter overlay is already implemented (setShowTxFilterOnRxWaterfall);
    // this slot drives the spectrum-panel TX filter shadow.

    bool isMoxOverlayActive() const noexcept { return m_moxOverlay; }
    /// Whether the GPU path draws a spectrum trace this frame: new pixels,
    /// or the last trace it built. Parity Task 29: never the last trace
    /// while the MOX overlay is on and there are no transmit pixels (a
    /// remote pan on a Core that sends no transmit display): the receive
    /// trace would sit under the transmit axis and read at the wrong
    /// frequencies, so the pan stays blank with its status line.
    bool drawsSpectrumTrace() const noexcept
    {
        return !m_renderedPixels.isEmpty() || (m_visibleBinCount > 0 && !showsTransmitView());
    }
    float txAttenuatorOffsetDb() const noexcept { return m_txAttOffsetDb; }
    /// Parity Task 31 (A11): display duplex (DUP). Off by default, as
    /// Thetis's _display_duplex (console.cs:15390-15395 [v2.10.3.15]).
    bool displayDuplex() const noexcept { return m_displayDuplex; }
    /// Whether the pan shows the transmit display: keyed (the MOX overlay)
    /// with DUP off. Keyed with DUP on the pan keeps the receiver's span,
    /// bins and frames, under the transmit grid and waterfall levels.
    bool showsTransmitView() const noexcept { return m_moxOverlay && !m_displayDuplex; }
    /// The transmit display calibration (Setup > Calibration's TX Display
    /// Cal Offset), Thetis tx_display_cal_offset: the keyed trace's
    /// calibration, DUP on or off (display.cs:4820-4850 [v2.10.3.15],
    /// RX1Offset).
    float txDisplayCalOffsetDb() const noexcept { return m_txDisplayCalOffsetDb; }
    void setTxDisplayCalOffsetDb(float db);
    /// The preamp half of the receive calibration (setStationCalOffsetDb
    /// carries both), Thetis Display.RX1PreampOffset; keyed with DUP on the trace
    /// adds the receive calibration without it.
    float rxPreampOffsetDb() const noexcept { return m_rxPreampOffsetDb; }
    void setRxPreampOffsetDb(float db);
    /// The calibration the trace is drawn with now (see displayCalOffsetDb
    /// in the .cpp for the rule).
    float displayCalOffsetDb() const;
    bool txFilterVisible() const noexcept { return m_txFilterVisible; }

public slots:
    void setDisplayFps(int fps);

    // Slot wired from MoxController::moxStateChanged (MainWindow::setupModel).
    // Draws a 3 px red border around the spectrum area when isTx=true.
    // From Thetis display.cs:1569-1593 [v2.10.3.13] Display.MOX setter.
    void setMoxOverlay(bool isTx);

    // Slot fed from StepAttenuatorController::txAttenuatorOffsetDbChanged.
    // Shifts the dBm calibration display during TX-active ATT-on-TX.
    // From Thetis display.cs:4840 [v2.10.3.13].
    // Upstream tags preserved: //MW0LGE (from cited upstream lines) [v2.10.3.15]
    void setTxAttenuatorOffsetDb(float offsetDb);

    // Slot driven from DisplayPage DrawTXFilter checkbox.
    // From Thetis display.cs:2481 [v2.10.3.13].
    void setTxFilterVisible(bool on);

    // ---- Two-tone IMD overlay state slots (Phase 3M-4 Task 12) ----
    //
    // From Thetis display.cs:5008 [v2.10.3.13] show condition:
    //   show_imd_measurements = local_mox && _testing_imd
    //                           && _show_imd_measurements && displayduplex;
    //
    // Three of the four flags come from external coordinators wired by
    // MainWindow:
    //   setMoxOverlay(bool)            <- already exists; drives local_mox
    //   setTestingIMD(bool)            <- TwoToneController::twoToneActiveChanged
    //                                      (mirrors Thetis Display.TestingIMD,
    //                                      display.cs:296-302 [v2.10.3.13])
    //   setShowIMDMeasurements(bool)   <- PureSignal::show2ToneMeasurementsChanged
    //                                      (mirrors Thetis Display.ShowIMDMeasurments,
    //                                      display.cs:304-311 [v2.10.3.13])
    //   setDisplayDuplex(bool)         <- console.cs:15390-15395 [v2.10.3.15]
    //                                      DisplayDuplex, false by default as
    //                                      Thetis (parity Task 31); set on the
    //                                      transmitting pan by
    //                                      MoxDisplayController.
    //
    // setShowIMDMeasurements(false) calls ImdOverlay::reset() to clear EMA
    // state, mirroring the Thetis display.cs:5680 [v2.10.3.13] behaviour:
    //   else if (_ema_dbc != -999) _ema_dbc = -999;
    void setTestingIMD(bool on);
    void setShowIMDMeasurements(bool on);
    void setDisplayDuplex(bool on);
    const ImdMeasurementFrame& imdMeasurementFrame() const;
    void setImdInspectorVisible(bool visible);
    void setImdGuidesVisible(bool visible);
    bool imdInspectorVisible() const { return m_imdInspectorVisible; }
    bool imdGuidesVisible() const { return m_imdGuidesVisible; }
    ImdProduct imdSelectedProduct() const { return m_imdSelectedProduct; }
    void setImdSelectedProduct(ImdProduct product);
    void queueImdCorrectionEdge(bool correctionsApplied);
    // A remote keyframe wait means the previously accepted receiver frame
    // is no longer a current measurement, even while its picture is held.
    void noteImdReceiverInputLost();

    // ---- TX filter overlay (Plan 4 D9, Cluster E) ----

    /// Set the TX filter audio-Hz range.  Triggers a panadapter overlay
    /// repaint (always) and a waterfall column repaint (MOX-gated via
    /// existing m_moxOverlay).
    /// Source: NereusSDR-original glue; per-mode IQ-space mapping follows
    /// deskhpsdr/transmitter.c:2136-2186 [@120188f].
    void setTxFilterRange(int audioLowHz, int audioHighHz);

    /// Set the DSP mode so the TX filter overlay uses the correct IQ-space
    /// sign convention (USB positive, LSB negated+swapped, AM symmetric).
    /// Wired from SliceModel::dspModeChanged in MainWindow::wireSliceToSpectrum.
    void setTxMode(DSPMode mode);

    /// Signed Hz offset added to m_vfoHz when computing the TX overlay
    /// position.  Tracks the slice's active XIT offset (xitEnabled ? xitHz : 0)
    /// so the orange band centers on the actual TX frequency, not the RX VFO.
    /// Wired from SliceModel::xitEnabledChanged + xitHzChanged in MainWindow.
    void setTxVfoOffsetHz(int offsetHz);

    int txFilterLow()  const noexcept { return m_txFilterLow; }
    int txFilterHigh() const noexcept { return m_txFilterHigh; }

    // ---- TX / RX filter overlay colors (Plan 4 D9b, Cluster F) ----

    /// Set the TX passband overlay fill colour and opacity.
    /// Persists to DisplayTxFilterColor (per-pan AppSettings key).
    void setTxFilterColor(const QColor& c);
    QColor txFilterColor() const noexcept { return m_txFilterColor; }

    /// Set the RX passband overlay fill colour and opacity.
    /// Persists to DisplayRxFilterColor (per-pan AppSettings key).
    void setRxFilterColor(const QColor& c);
    QColor rxFilterColor() const noexcept { return m_rxFilterColor; }

    // ---- 3M-5b: TX waterfall colormap setters / getters ----
    // From Thetis display.cs:6506-6595 [v2.10.3.13+501e3f51] -- TX thresholds,
    // palette, and low-color are switched inline per-frame when MOX is active.
    // MW0LGE [2.9.0.7]  [original inline comment from display.cs:6588]
    int  txWfLowLevel()  const noexcept { return m_txWfLowLevel;  }
    int  txWfHighLevel() const noexcept { return m_txWfHighLevel; }
    WfColorScheme txWfPalette()   const noexcept { return m_txWfPalette;   }
    QColor        txWfLowColor()  const noexcept { return m_txWfLowColor;  }
    QString       txWfGradient()  const noexcept { return m_txWfGradient;  }

    void setTxWfLowLevel(int dbm);
    void setTxWfHighLevel(int dbm);
    void setTxWfPalette(WfColorScheme s);
    void setTxWfLowColor(const QColor& c);
    void setTxWfGradient(const QString& encoded);

    // ---- 3M-5d: TX waterfall pixout=1 tap ----
    // When true, updateSpectrumLinear() skips its internal call to
    // pushWaterfallRow() because the waterfall plane is being driven
    // separately via pushTxWaterfallRow() (TxAnalyzer's pixout=1 stream,
    // wired in MainWindow on MOX-up).  Defaults to false; only flipped
    // by MainWindow on MOX edges.  RX path is unaffected.
    void setTxExternalWaterfall(bool on);
    bool txExternalWaterfall() const noexcept { return m_txExternalWaterfall; }

public slots:
    /// Slot that bridges TxAnalyzer::txWaterfallReady (pixout=1, WDSP
    /// already applied DetTypeWF + AverageModeWF) into the existing
    /// waterfall-row push path.  Skips the spectrum-trace pipeline
    /// entirely.  Only meaningful while setTxExternalWaterfall(true) has
    /// been called by MainWindow on MOX-up; otherwise the internal
    /// updateSpectrumLinear path is already pushing rows and a second
    /// push would race.
    void pushTxWaterfallRow(int receiverId, const QVector<float>& binsDbm);

public:

#ifdef NEREUS_BUILD_TESTS
    // Test seams for tst_tx_waterfall_colormap (3M-5b).
    // These expose private methods via the NEREUS_BUILD_TESTS gate so the
    // test binary can round-trip settings and call dbmToRgb directly.
    void saveSettingsForTest()              { saveSettings(); }
    void loadSettingsForTest()              { loadSettings(); }
    /// The MOX tune gate is the property under test; synthesising a press,
    /// a wheel notch and a spot-label hit would test Qt's event delivery
    /// instead. Every emitter routes through requestTune, which is greppable.
    void requestTuneForTest(double hz)      { requestTune(hz); }
    QRgb dbmToRgbForTest(float dbm) const  { return dbmToRgb(dbm); }
    /// The low and high dBm dbmToRgb spans right now (tst_clarity_levels).
    std::pair<float, float> effectiveWaterfallLevelsForTest() const
    {
        return effectiveWaterfallLevels();
    }
    void setWfLowThresholdForTest(float dbm)  { m_wfLowThreshold  = dbm; }
    void setWfHighThresholdForTest(float dbm) { m_wfHighThreshold = dbm; }
    // VFO flag crash lane: this pan's VFO marker and passband, which the
    // slice the pan hosts drives.
    double vfoFrequencyForTest() const { return m_vfoHz; }
    int filterLowForTest() const { return m_filterLowHz; }
    int filterHighForTest() const { return m_filterHighHz; }
#endif

    // ---- Per-pan settings persistence ----
    // setPanIndex() also forwards to this widget's owned
    // DisplaySettingsModel (Task 18) so the child's per-pan keys always
    // track this widget's own. Out-of-line (not inline) because
    // DisplaySettingsModel is only forward-declared in this header.
    void setPanIndex(int idx);
    int  panIndex() const { return m_panIndex; }
    // The DisplaySettingsModel this widget owns and binds to (Task 18):
    // one per SpectrumWidget, i.e. one per panadapter. Surfaces that
    // follow the active pan reach it through
    // RadioModel::spectrumWidget()->displaySettings().
    DisplaySettingsModel* displaySettings() const { return m_displaySettings; }
    void loadSettings();
    // Display pill plan Task 1. The base name of every per-pan display key
    // this widget and its DisplaySettingsModel persist. The pan's own
    // override of a base is PanDisplayKeys::panKey(base, panIndex()); the
    // bare base is the Setup default (core/settings/PanDisplayKeys.h).
    QStringList perPanDisplayKeyBases() const;
    // Display pill plan Task 21: the ship default a pan falls back to for
    // one of those bases when neither the pan nor Setup has a value, as the
    // key stores it. Empty for a base that is not per-pan (and for the
    // trace colour, whose ship default is "follow the fill colour").
    QString perPanShipDefault(const QString& base) const;
    // Canonical ship default without a widget, settings access, or runtime
    // setup. Uses the same inventory as live values and persistence.
    static QString perPanShipDefaultForClosedPan(const QString& base);
    // The current value in the same format a save uses, without writing,
    // flushing pending settings or changing the widget/model.
    QString perPanDisplayValue(const QString& base) const;
    // The ones "Reset this pan" clears: the list above without the pan's
    // view, which is not its appearance (the zoom "DisplayBandwidth", the
    // transmit view span "DisplayTxViewBandwidth" and the CTUN preference
    // "DisplayCtunEnabled").
    QStringList resettablePanDisplayKeyBases() const;
    // Drops this pan's own appearance overrides (the resettable keys, for
    // this pan only) and puts the live widget on what it now inherits; its
    // zoom, transmit view span and CTUN preference stay as they are, live
    // and stored. A pending save is
    // cancelled, the widget and its DisplaySettingsModel are reloaded,
    // whatever the reloaded values feed is rebuilt, and panDisplayReset()
    // is emitted once. The Setup defaults and every setting that is not
    // per pan are left alone.
    void resetPanDisplayOverrides();
    // Reset only known picture keys, preserving pending values on other
    // pages. Unknown keys and view keys are never cleared.
    void resetPanDisplayOverridesForKeys(const QStringList& bases);
    // A change made on this pan is stored up to half a second later (the
    // coalesced save). While one is waiting, this stores `base` now, as
    // that save will: the pan's own key, and only when the pan already has
    // it or its value differs from what it inherits. Setup calls it before
    // it changes the default for `base`, so a pan that was just given a
    // value of its own is seen to have one and keeps it. Nothing happens
    // when no save is waiting; the waiting save still runs.
    void storePendingPerPanSetting(const QString& base);
    // Test seam: whether a coalesced save is waiting to run.
    bool settingsSavePendingForTest() const { return m_settingsSaveScheduled; }
    void loadSpectrumPeaksSettings();
    // The grid's noise-floor tracking rule both feeds share, and its timer.
    void applyGridFollow(float nfDbm, bool force = false);
    /// Last accepted finite measured floor, without deriving one from the grid.
    std::optional<float> acceptedGridFollowFloorDbm() const;
    void updateGridFollowTimer();
    // Applies a shared (once for every pan) setting to every other pan.
    void shareWithOtherPans(const std::function<void(SpectrumWidget*)>& apply);
    void saveSettings();
    // Public coalesced-save trigger. Used by setup pages that call setDbmRange()
    // directly (which has no internal save) and need to ensure the new range
    // is persisted (e.g. Task 2.9 Copy button, per-band NF priming).
    void requestSettingsSave() { scheduleSettingsSave(); }

    // ---- VFO / filter overlay ----
    void setVfoFrequency(double hz);
    void setFilterOffset(int lowHz, int highHz);  // updates filter passband overlay

    // ---- CTUN mode (SmartSDR-style independent pan) ----
    void setCtunEnabled(bool enabled);
    bool ctunEnabled() const { return m_ctunEnabled; }
    bool ctunPreference() const { return m_ctunPreference; }
    bool ctunAvailable() const { return m_ctunAvailable; }
    // Core's effective stream state must not emit a gesture or overwrite the
    // per-pan preference restored when a new station stream is acquired.
    // `heldReason` is not empty when the receiver's C-Tune setting is not
    // this window's to change (another holder's receiver, or a slice it
    // only listens to): the pan shows the receiver's setting, its switch is
    // disabled with these words, and setCtunEnabled changes nothing.
    void applyRemoteCtunState(bool available, bool pinned, const QString& heldReason = QString());
    // Why the C-Tune switch is held; empty when it is free or unavailable.
    QString ctunHeldReason() const { return m_ctunHeldReason; }
    void recenterOnVfo();
    // R-R3-18/19: true while centerChanged comes from a zoom (wheel zoom or
    // frequency-scale drag) re-centring the view on the VFO, not from the
    // operator moving the pan. A remote window uses it to keep a zoom a view
    // change on a stream other receivers share.
    bool isZoomRecentring() const { return m_draggingBandwidth || m_wheelZoomRecentring; }
    // Ends the current pan or frequency-scale drag without click-to-tune
    // on release; the rest leaves the view where it is. Used when the Core
    // refuses to move its window for this drag.
    void endPanDrag();

    // ---- Tuning step ----
    void setStepSize(int hz) { m_stepHz = hz; }
    int  stepSize() const { return m_stepHz; }

    // ---- VFO flag widgets (AetherSDR pattern) ----
    VfoWidget* addVfoWidget(int sliceIndex);
    void removeVfoWidget(int sliceIndex);
    VfoWidget* vfoWidget(int sliceIndex) const;
    void updateVfoPositions();
    /// Where this pan's receiver flags show, in this widget's coordinates:
    /// each flag, open or folded, with its side buttons and folded
    /// frequency label (VfoWidget::occupiedRegion). PanadapterApplet keeps
    /// its corner badges out of it so the flags stay on top of them.
    QRegion receiverFlagRegion() const { return m_receiverFlagRegion; }
    /// A slice this pan shows only as its flag and, off the span, as its own
    /// edge marker, never through the pan's VFO: a listened slice a layout
    /// change placed on this pan. Cleared when its flag is removed.
    void setEdgeMarkedSlice(int sliceIndex, bool edgeMarked);
    bool isEdgeMarkedSlice(int sliceIndex) const { return m_edgeMarkedSlices.contains(sliceIndex); }
    /// Re-place the flags and repaint the edge markers, for a slice whose
    /// tune must not go through the pan's VFO.
    void refreshSliceFlags();

    /// Pin sliceIndex's flag to the front of this pan's stacking order.
    ///
    /// Bench-reported 2026-07-28 (Sub-Epic J): with Slice A selected, Slice
    /// B's flag still covered A's, clipping A's frequency readout to
    /// ".955.300". z-order was creation order -- addVfoWidget's one-shot
    /// raise() -- crossed with updateVfoPositions()'s own per-frame raise()
    /// over m_vfoWidgets (a QMap sorted by slice index), which puts whichever
    /// slice has the HIGHER index on top after every single position pass,
    /// with no regard for which one the operator selected.
    ///
    /// A one-shot raise() here would not survive that: updateVfoPositions()
    /// runs every render frame (see its own comment) and would re-apply the
    /// raw ascending order on the very next pass. m_frontSliceIndex is the
    /// pin updateVfoPositions() re-asserts at the end of its own loop so the
    /// front flag survives the next frame too, and every one after it, until
    /// this is called again.
    ///
    /// A sliceIndex this pan does not host (not yet built, or hosted by a
    /// different pan) leaves the pin set but is a harmless no-op here: only
    /// the pan that actually hosts the active slice re-orders, exactly as
    /// PanadapterStack::setActiveSliceOnHostingPan already scopes the rest of
    /// the active-slice machinery to the hosting pan alone.
    void setFrontSliceIndex(int sliceIndex);

// Plain public, not public slots: the enclosing section above is a slots
// block and moc rejects a nested struct inside one.
public:
    // ---- RX marker geometry (Phase 3F) ----

    /// One RX marker's inputs: a slice centre, that slice's own signed filter
    /// edges, and the flag whose bottom edge its triangle hangs from (null
    /// when the pan is drawing its own VFO with no flag created yet).
    ///
    /// Also which slice the marker belongs to, whether that slice is the one
    /// the operator has selected (VfoWidget::isActiveSlice), and the two
    /// colours that follow from those. The selected slice draws everything in
    /// its own slice colour. Any other slice draws its centre line and
    /// triangle in the darker partner of its colour and its filter edges in
    /// a neutral grey. Both colours are opaque; drawSliceMarker applies each
    /// element's own alpha. The shaded passband is not here: it stays the
    /// operator's m_rxFilterColor for every slice.
    struct SliceMarkerGeometry {
        double centreHz{0.0};
        int    filterLowHz{0};
        int    filterHighHz{0};
        const VfoWidget* flag{nullptr};
        int    sliceIndex{0};
        bool   active{true};
        QColor lineColor;  // VFO centre line and triangle
        QColor edgeColor;  // filter edge lines
    };

    /// iPhone app plan Task 78 (the several-devices design, section 12
    /// item 1; D46): another device's slice, drawn as the phone draws it: a
    /// dashed centre line in the slice's colour with a hollow triangle,
    /// dashed grey passband edges with no fill, and a label at the foot of
    /// the spectrum (the letter and the device's short name, plus TX by
    /// ruling 5.4a). No flag; not draggable.
    struct ForeignSliceMarker {
        int     sliceId{-1};
        double  centreHz{0.0};
        int     filterLowHz{0};
        int     filterHighHz{0};
        QColor  color;
        QString letter;
        QString ownerShortName;
        QString ownerName;
        bool    tx{false};
        bool    away{false};
        /// Nobody controls the slice (the Core's access entry names no
        /// controller, or the Core sends no access data for it).
        bool    unowned{false};
    };
    void setForeignSliceMarkers(const QVector<ForeignSliceMarker>& markers);
    const QVector<ForeignSliceMarker>& foreignSliceMarkers() const { return m_foreignMarkers; }
    void setOwnSliceMarkerPresentationAllowed(bool allowed);
    /// The label's text: "B iPhone", "B iPhone TX".
    static QString foreignMarkerLabel(const ForeignSliceMarker& marker);
    /// What a click on the label says.
    static QString foreignMarkerExplanation(const ForeignSliceMarker& marker);
    /// Where the label was last drawn (an invalid rect before a paint, or
    /// while the slice is off this pan).
    QRect foreignMarkerLabelRect(int sliceId) const { return m_foreignLabelRects.value(sliceId); }
    /// Test seam: paints the foreign markers alone, as paintEvent does.
    void drawForeignMarkersForTest(QPainter& p, const QRect& specRect, const QRect& wfRect)
    { drawForeignMarkers(p, specRect, wfRect); }

    /// Every RX marker this pan must paint, one per hosted slice, in paint
    /// order: the slices the operator has not selected first, in slice order,
    /// then the selected slice, so its marker lands on top where two overlap.
    ///
    /// This is drawVfoMarker()'s whole decision, split out so it is reachable
    /// without a shown QRhiWidget. See tests/tst_pan_flag_positions.cpp.
    QVector<SliceMarkerGeometry> sliceMarkerGeometry() const;
    /// Paint-only seams for palette assertions without a shown QRhiWidget.
    void drawOwnMarkersForTest(QPainter& p, const QRect& specRect, const QRect& wfRect)
    { drawVfoMarker(p, specRect, wfRect); }
    void drawOffScreenIndicatorForTest(QPainter& p, const QRect& specRect, const QRect& wfRect)
    { drawOffScreenIndicator(p, specRect, wfRect); }

public slots:
    // Phase 3Q-8: update connection state for the disconnect overlay.
    void setConnectionState(NereusSDR::ConnectionState s);

    // Feed a new FFT frame.  binsLinear are |X[k]|² linear-power values,
    // one per frequency bin (full FFT, neg-freq first then pos-freq).
    // windowEnb is the Equivalent Noise Bandwidth of the FFT window in
    // bins; the detector applies invEnb = 1/windowEnb for Average / Sample
    // / RMS modes (analyzer.c:368-441 [v2.10.3.13]).  dbmOffset is the
    // window coherent-gain compensation -20·log10(Σw[i]) that the avenger
    // applies via scale = 10^(dbmOffset/10) so post-pipeline pixels read
    // calibrated dBm.  Called on the main thread after FFTEngine delivers
    // the frame via fftReadyLinear.
    //
    // Pipeline: visibleBinRange() slice -> applySpectrumDetector() ->
    // SpectrumAvenger::apply() -> m_renderedPixels (dBm, displayWidth).
    // Mirrors WDSP analyzer.c detector() at :283 + avenger() at :464
    // [v2.10.3.13] -- the same two-stage reduction Thetis runs per
    // display plane.  Same slice feeds the waterfall pipeline (own
    // detector + avenger) and lands in m_wfRenderedPixels.
    //
    // computedAfterInputPurge is false for a frame whose samples are from
    // before the last FFT input purge (the window compares the frame's
    // generation with the pool's). Such a frame is drawn as any other, but
    // it does not end the waterfall's hold after a key edge.
    void updateSpectrumLinear(int receiverId, const QVector<float>& binsLinear,
                              double windowEnb, double dbmOffset,
                              bool computedAfterInputPurge = true);

    // NF-aware grid slot — wired to ClarityController::noiseFloorChanged in MainWindow.
    // From Thetis console.cs:46074-46086 [v2.10.3.13] tmrAutoAGC_Tick NF grid block.
    // Range delta uses std::abs() — abs incase //MW0LGE [2.9.0.7] [original inline comment from console.cs:46081]
    // NereusSDR-original: global panadapter default, no RX1/RX2 split.
    void onNoiseFloorChanged(float nfDbm);
    // Thetis tmrAutoAGC port: the grid rule on the pan's own noise floor.
    void gridFollowTick();

    // ── Waterfall scrollback (sub-epic E) ─────────────────────────────────
    // Reset the rewind ring buffer back to empty + live state. Public so
    // MainWindow can wire it to RadioModel::connectionStateChanged when
    // the radio disconnects (see plan §Task 4 Step 3-4 — NereusSDR has no
    // SpectrumWidget::clearDisplay() equivalent, so the flush is plumbed
    // through MainWindow rather than embedded in resizeEvent).
    // From AetherSDR SpectrumWidget.cpp:740-756 [@0cd4559]
    void clearWaterfallHistory();

    /// Phase 3F Sub-Epic F Task 6: receive wideband bins for the active-ADC
    /// extended pan. Bins are stored per-ADC; Task 8 (2026-08-08) is what
    /// finally paints them.
    void setWidebandBins(int adcIndex, const QVector<float>& dbmBins);
    QVector<float> widebandBinsForTest(int adcIndex) const
    {
        return adcIndex == 0 ? m_widebandBinsAdc0 : m_widebandBinsAdc1;
    }

    /// Which ADC's wideband bins this pan's wings read.
    ///
    /// MainWindow fans every ADC's frame to every pan, so without this a pan
    /// on ADC1 would paint ADC0's spectrum in its wings. Resolved from the
    /// pan's active slice through RadioModel::sliceChainIndex, the same
    /// answer the WIDE pill and the CH tag are keyed on.
    void setWidebandAdcIndex(int adcIndex);
    int  widebandAdcIndex() const { return m_widebandAdcIndex; }

    /// ADC clock backing the wideband stream. Sets the wing frequency axis:
    /// the wideband FFT is real-input, so its bins span 0..rate/2.
    /// From Thetis wbDisplay.cs:4511 [v2.10.3.15] — `private int
    /// sample_rate = 122880000;`, the same value RadioModel already seeds
    /// WidebandFftEngine with. Upstream hardcodes it too; nothing in Thetis
    /// writes wbDisplay.SampleRate.
    void setWidebandAdcRateHz(double rateHz);
    double widebandAdcRateHz() const { return m_widebandAdcRateHz; }

    /// Zoom-out ceiling for both operator zoom gestures.
    ///
    /// Bench 2026-08-08: the wheel and the frequency-scale drag both clamped
    /// visible bandwidth to the DDC sample rate, and `m_bandwidthHz >
    /// m_sampleRateHz` is the ONLY trigger for extended mode -- so the
    /// "Extended view (wideband wings)" toggle enabled a state no gesture
    /// could reach. With extended view allowed the ceiling becomes the
    /// wideband ADC's Nyquist (the span wing data actually covers); with it
    /// switched off the ceiling stays at the DDC rate, exactly as before.
    /// A remote pan uses the same rule only after its accepted context says
    /// that a concrete ADC source is available; active streaming is separate.
    double maxZoomOutBandwidthHz() const;

    /// Set the display window with the span held to what this pan may show.
    ///
    /// The ceiling is maxZoomOutBandwidthHz(): the DDC rate normally, the
    /// wideband ADC's Nyquist once extended view is allowed. A span of zero
    /// or less, or one above the ceiling, becomes the ceiling.
    ///
    /// This exists because three separate places in MainWindow re-apply a
    /// span (restore at startup, the sample-rate handler, and the stream
    /// re-subscription path) and each clamped against the DDC rate on its
    /// own. An extended zoom therefore survived until whichever of them ran
    /// last, and Codex found them one at a time over three rounds on PR #318.
    /// One definition, so there is no fourth.
    void setDisplayWindowClamped(double centreHz, double requestedSpanHz);

    /// dB correction applied to a raw wideband bin before it is drawn beside
    /// the DDC trace. Two terms; see the definitions for the derivations.
    ///
    ///  1. FFT normalisation — refers the transform to 0 dBFS.
    ///  2. Bandwidth normalisation — refers the wideband bin to the DDC's
    ///     bin width, so the two halves of the trace are power densities on
    ///     one scale rather than power-per-bin on two different scales.
    ///
    /// Public so tests can assert both terms are present. Bench 2026-08-08
    /// found each one alone puts the wings off the panel: with only term 1
    /// they saturate, and term 2 is what brings them onto the island's scale.
    ///
    /// `detector` is the one that produced the DDC pixels this wing sits
    /// beside, and it is not decoration: Average / Sample / RMS are handed
    /// invEnb and divide the window ENB out of the island
    /// (SpectrumDetector.cpp cases 2, 3, 4), while Peak and Rosenfell take a
    /// max and leave it in. The island's effective noise reference therefore
    /// differs by the ENB depending on which one is selected, and the trace
    /// and waterfall planes select independently.
    static float widebandFftNormalisationDb();
    float widebandBandwidthNormalisationDb(SpectrumDetector detector) const;
    float widebandTotalCalibrationDb(SpectrumDetector detector) const;

    /// Half-width of the DDC span actually drawn, kDdcClipFraction of the
    /// rate already removed from each side.
    ///
    /// One definition for three consumers: the island pixel geometry, the
    /// dashed boundary markers, and the click router that decides whether a
    /// click retunes the slice or the DDC. They disagreed before this
    /// existed, so each 4% edge strip painted wideband data while the marker
    /// sat outside it and a click there still retuned the slice. Meaningful
    /// only in extended mode, which is the only place all three use it.
    double ddcIslandHalfSpanHz() const;

    /// Pixel span [first, last] the DDC actually covers inside the current
    /// display window; the wings are everything outside it.
    ///
    /// Outside extended mode this is the whole width, which is what makes
    /// the ordinary path a no-op. Public so a test can assert the geometry
    /// without a GPU: the bug it closes is that visibleBinRange() CLAMPS to
    /// the DDC bin array, so zooming past the DDC rate used to stretch the
    /// DDC's spectrum across the whole panel instead of shrinking it into
    /// its true span — a trace that no longer agreed with the frequency
    /// scale drawn beneath it.
    std::pair<int, int> listenableIslandPixels(int displayWidth) const;

    /// Phase 3F Sub-Epic F Tasks 7-10: allow extended-pan rendering.
    /// The actual state is on only when allowed AND the visible bandwidth
    /// exceeds a known positive DDC sample rate. paintEvent
    /// will render wideband bins as a background fill behind the
    /// listenable DDC island. Full visual polish (dashed boundary lines,
    /// palette-aware bin draw) lands in a post-bench iteration; for now
    /// extendedMode is the derived actual state that drives the signal.
    void setExtendedViewAllowed(bool allowed);
    bool extendedViewAllowed() const { return m_extendedViewAllowed; }
    // Local radio/ADC eligibility, independent of the saved operator toggle.
    // The pan bridge supplies this once the radio descriptor is known.
    void setLocalWidebandAvailable(bool available);
    bool extendedMode() const { return m_extendedMode; }

    // ── Detach-safe RHI lifecycle (float / dock a pan) ────────────────────
    //
    // Bench report 2026-08-08: floating a pan painted one frame and then
    // logged "QRhiWidget: No QRhi" once per display tick forever, i.e. a
    // frozen panadapter in the popped-out window. Confirmed on an ANAN-G2E:
    // one `got CAMetalLayer, pixel size 2356x1702` followed by 30 warnings a
    // second (the display timer's rate, one widget).
    //
    // A QRhiWidget's render context belongs to the top-level window it lives
    // in, so moving one between windows is a teardown and a rebuild, not a
    // reparent. The three calls below are that teardown, split the way
    // AetherSDR splits them because each has to happen at a different point
    // relative to the setParent() call.
    //
    // From AetherSDR src/gui/SpectrumWidget.cpp:2227-2247, 7092-7110,
    // 1843-1857 [@1e0718ad].

    /// Tell Qt's RHI machinery this widget is leaving its top-level, BEFORE
    /// the reparent. QRhiWidget registers a cleanup callback on the current
    /// backing-store QRhi; a direct reparent can miss Qt's own notification
    /// and leave that callback pointing at a QRhiWidgetPrivate the old QRhi
    /// will later fire against.
    ///
    /// Must be sent exactly once, before the move. refreshAfterReparent must
    /// not re-send it.
    void prepareForTopLevelChange();

    /// Idempotent final teardown while the owning top-level QRhi still lives.
    void prepareForShutdown();

    /// Drop the pipelines/buffers/textures so initialize() rebuilds them
    /// against the new window's surface. Linux/OpenGL survives the move, so
    /// there it is just a repaint request.
    void resetGpuResources();

    /// Re-assert the native-window attributes as one unit.
    ///
    /// WA_NativeWindow gives the widget its own Metal leaf;
    /// WA_DontCreateNativeAncestors stops realizing that leaf from dragging
    /// the ancestor QWidget tree native behind it. They are set together,
    /// here, so a reparent that re-realizes the native window can never
    /// reassert one without the other. Idempotent; a no-op off macOS and on
    /// non-GPU builds.
    void applyNativeWindowIsolationPolicy();

private:
    friend class RemoteMediaController; // Private opt-in display provenance only.
    bool m_shutdownPrepared {false};

public:
    /// Fraction of the DDC's spectrum discarded at EACH edge when drawing
    /// the listenable island.
    ///
    /// From Thetis specHPSDR.cs:529 [v2.10.3.15] — `const double
    /// CLIP_FRACTION = 0.04;` with upstream's own comment "fraction of the
    /// spectrum to clip off each side of each sub-span", applied at :535 as
    /// `clip = floor(CLIP_FRACTION * fft_size)`.
    ///
    /// Bench 2026-08-08: "still black gaps between wideband" and the DDC.
    /// The outermost DDC bins sit in the decimation filter's transition band
    /// and read near-nothing. At normal zoom you never see them because the
    /// window is inside the DDC, but the extended pan draws the island at
    /// its true width and puts the skirt on screen as a dark band at each
    /// edge. Upstream discards it; so do we, and the wideband wings cover
    /// that span instead.
    ///
    /// Applied ONLY in extended mode. The ordinary path keeps every bin it
    /// always had -- clipping there would be a wider behaviour change than
    /// the reported defect.
    static constexpr double kDdcClipFraction = 0.04;

    // ── Spot overlay (Phase 3J-2 Task E1) ─────────────────────────────────
    // Public structs + setters re-declared under a fresh `public:` access
    // specifier so MOC doesn't try to interpret the nested struct as a
    // slot declaration (the enclosing block above is `public slots:`).
    //
    // Spot marker descriptor pushed into the panadapter overlay. Mirrors
    // AetherSDR's SpotMarker struct (SpectrumWidget.h:283-294 [@0cd4559])
    // verbatim so the upstream drawSpotMarkers() algorithm ports unchanged.
    // From AetherSDR src/gui/SpectrumWidget.h:283-294 [@0cd4559]
    struct SpotMarker {
        int    index{-1};
        QString callsign;
        double freqMhz{0.0};
        QString color;       // #AARRGGBB or empty for default
        QString mode;
        QColor  dxccColor;   // DXCC-aware color from DxccColorProvider (#330)
        QString source;
        QString spotterCallsign;
        QString comment;
        qint64  timestampMs{0};
        // NereusSDR addition (rotor control plan Task 8): the Core's
        // short-path bearing to the spot (SpotData::bearingDeg), -1 when
        // it served none; SpotBeamTurner works one out then.
        double  bearingDeg{-1.0};
    };

    // Cluster badge descriptor for spots that overflowed the level cap.
    // From AetherSDR src/gui/SpectrumWidget.h:297-300 [@0cd4559]
    struct SpotCluster {
        QRect rect;
        QVector<SpotMarker> spots;
    };

    // Click hit-test rectangle bound to a single SpotMarker. The rect is
    // the label box drawn by drawSpotMarkers; freqMhz is the click-to-tune
    // target; markerIndex points back into m_spotMarkers for tooltip data.
    // From AetherSDR src/gui/SpectrumWidget.h:635-639 [@0cd4559]
    struct SpotHitRect {
        QRect  rect;
        double freqMhz{0.0};
        int    markerIndex{-1};
    };

    void setSpotMarkers(const QVector<SpotMarker>& markers);
    // 2026-05-12 bench fix (Gap #6 — Spot List hover sync).  Driven
    // by SpotHubDialog when the user mouses over a row so the
    // matching marker on the panadapter highlights.  -1 clears.
    void setHoverSpotIndexExternal(int idx) {
        if (m_hoverSpotIndexExternal != idx) {
            m_hoverSpotIndexExternal = idx;
            markOverlayDirty();  // the highlight is in the static overlay
        }
    }
    // 2026-05-12 bench fix (Gap #7).  Per-source panadapter overlay
    // visibility.  Missing key == visible (default).  Source strings
    // match SpotMarker::source: "Cluster", "RBN", "WSJT-X",
    // "SpotCollector", "POTA", "FreeDV", "PSK", "Memory".
    void setSpotSourceVisible(const QString& source, bool visible) {
        const bool current = m_spotSourceVisible.value(source, true);
        // Phase 3J-1 closeout follow-up (2026-05-12): always INSERT the
        // mask state, even when current==visible.  Previously we'd skip
        // the insert if the new value matched the default, leaving the
        // hash empty.  That's fine until a spot arrives later whose
        // source somehow doesn't match the key string format -- the
        // mask check m_spotSourceVisible.value(...) falls back to the
        // default 'true' and shows it.  Always inserting guarantees
        // the key is present for predictable mask behaviour and avoids
        // a class of "default-true silently shows" bugs the bench
        // operator hit with FreeDV spots on 2026-05-12.
        m_spotSourceVisible.insert(source, visible);
        if (current != visible) { markOverlayDirty(); }
    }
    bool isSpotSourceVisible(const QString& source) const {
        return m_spotSourceVisible.value(source, true);
    }
    // The spot labels are drawn into the GPU path's cached static overlay,
    // so every input to drawSpotMarkers marks it dirty. AetherSDR's
    // setShowSpots repaints only (SpectrumWidget.h:680 [@1e0718ad]); ours
    // gates drawSpotMarkers inside the static overlay on m_showSpots, so
    // the toggle invalidates it too.
    void setShowSpots(bool on) { m_showSpots = on; markOverlayDirty(); }
    bool showSpots() const { return m_showSpots; }
    // From AetherSDR src/gui/SpectrumWidget.h:713-722 [@1e0718ad]
    void setSpotFontSize(int px) { m_spotFontSize = px; markOverlayDirty(); }
    void setSpotMaxLevels(int n) { m_spotMaxLevels = n; markOverlayDirty(); }
    void setSpotStartPct(int pct) { m_spotStartPct = pct; markOverlayDirty(); }
    void setSpotOverrideColors(bool on) { m_spotOverrideColors = on; markOverlayDirty(); }
    void setSpotOverrideBg(bool on) { m_spotOverrideBg = on; markOverlayDirty(); }
    void setSpotColor(const QColor& c) { m_spotColor = c; markOverlayDirty(); }
    void setSpotBgColor(const QColor& c) { m_spotBgColor = c; markOverlayDirty(); }
    void setSpotBgOpacity(int pct) { m_spotBgOpacity = pct; markOverlayDirty(); }

    // Phase 3J-2 + 3R M2: refresh every Display tab knob from AppSettings.
    // Wired to SpotHubDialog::settingsChanged in MainWindow::openSpotHub
    // so the live spectrum overlay tracks the dialog. Defaults match the
    // F4 buildDisplayTab read-side at SpotHubDialog.cpp:1714-1730.
    void loadSpotDisplaySettings();

    // Test seams (Phase 3J-2 Task E1). Public read-only views into the
    // private state drawSpotMarkers() rebuilds each frame; the test
    // suite asserts contract by inspecting these vectors after a
    // synthetic render pass.
    const QVector<SpotMarker>&   spotMarkersForTest()   const { return m_spotMarkers; }
    const QVector<SpotHitRect>&  spotClickRectsForTest() const { return m_spotClickRects; }
    const QVector<SpotCluster>&  spotClustersForTest()  const { return m_spotClusters; }
    void  drawSpotMarkersForTest(QPainter& p, const QRect& specRect) {
        drawSpotMarkers(p, specRect);
    }

    // Phase 3J-2 + 3R M2 test seams. Read-only views into the Spot
    // Display knob state so the M2 round-trip test can pin the
    // loadSpotDisplaySettings push-path without driving a paint cycle.
    int    spotFontSizeForTest()        const { return m_spotFontSize; }
    int    spotMaxLevelsForTest()       const { return m_spotMaxLevels; }
    int    spotStartPctForTest()        const { return m_spotStartPct; }
    int    spotBgOpacityForTest()       const { return m_spotBgOpacity; }
    bool   spotOverrideColorsForTest()  const { return m_spotOverrideColors; }
    bool   spotOverrideBgForTest()      const { return m_spotOverrideBg; }
    QColor spotColorForTest()           const { return m_spotColor; }
    QColor spotBgColorForTest()         const { return m_spotBgColor; }

    // ---- TNF / notch overlay (design section 8.1) ----
    // Ported from AetherSDR's TnfMarker (src/gui/SpectrumWidget.h:575-581
    // [@c6481cbf]) with depthDb + permanent replaced by `active`: WDSP's
    // notch DB carries neither depth nor permanence, its add entry point
    // taking fcenter / fwidth / active only
    // (third_party/wdsp/src/nbp.c:362, RXANBPAddNotch).
    //
    // UNIT BOUNDARY: freqMhz is the ONLY MHz quantity in the TNF stack.
    // NotchModel::centerHz, all five notch*Requested signals,
    // setNotchMinWidthHz and the visual-notch dent maths are Hz.
    // MainWindow::refreshPanNotchMarkers is the only conversion site.
    struct NotchMarker {
        int    id{-1};
        double freqMhz{0.0};
        double widthHz{200.0};
        bool   active{true};
    };

    // From AetherSDR src/gui/SpectrumWidget.cpp:13436-13440 [@c6481cbf].
    // markOverlayDirty(), not the bare update() the spot push uses: notch
    // chrome is cached in the GPU static-overlay texture, so a dragged
    // marker would otherwise not move on the shipping path.
    void setNotchMarkers(const QVector<NotchMarker>& markers);

    // From AetherSDR src/gui/SpectrumWidget.cpp:13497-13501 [@c6481cbf].
    // Master TNF flag.  Repaints every marker in the TNF-off colour rather
    // than hiding it (Thetis display.cs:8704-8707 [v2.10.3.15]).
    void setNotchGlobalEnabled(bool on);

    // WDSP's minimum notch width for the channel feeding this pan
    // (third_party/wdsp/src/nbp.c:594, RXANBPGetMinNotchWidth).  Pushed
    // rather than pulled because it varies with nc and sample rate; Thetis
    // caches it the same way (display.cs:1082 [v2.10.3.15]) and refreshes
    // it on filter-size change (console.cs:39052-39054 [v2.10.3.15],
    // UpdateMinimumNotchWidthRX).  Consumed by the visual-notch dent.
    void setNotchMinWidthHz(double hz);

    // ---- Visual notch (trace dent), design section 8.3 ----
    // Owner of the persisted state is NotchModel (key NotchVisualEnabled);
    // MainWindow::refreshPanVisualNotch pushes the same value at every pan.
    // From Thetis display.cs:1070 [v2.10.3.15]:
    //   private static bool m_bShowVisualNotch = false;
    void setVisualNotchEnabled(bool on);
    bool visualNotchEnabled() const { return m_visualNotchEnabled; }

    // Test seams, following the spotMarkersForTest convention above.
    const QVector<NotchMarker>& notchMarkersForTest() const { return m_notchMarkers; }
    bool   notchGlobalEnabledForTest() const { return m_notchGlobalEnabled; }
    double notchMinWidthHzForTest()    const { return m_notchMinWidthHz; }
    void drawNotchMarkersForTest(QPainter& p, const QRect& specRect) {
        drawNotchMarkers(p, specRect);
    }
    QRect notchSpecRectForTest() const { return notchSpecRect(); }

    // Visual-notch test seams (design section 8.3). Read-only views into the
    // state updateSpectrumLinear rebuilds each frame, so the section 11 test
    // can pin the measurement-routing contract without a paint cycle.
    float nfFftBinAverageForTest() const { return m_noiseFloor.fftBinAverage(); }
    float nfLerpAverageForTest() const { return m_noiseFloor.lerpAverage(); }
    const QVector<float>& undentedPixelsForTest() const {
        return measurementPixels();
    }
    const QVector<float>& activePeakHoldPeaksForTest() const {
        return m_activePeakHold.peaks();
    }
    const QVector<PeakBlob>& peakBlobsForTest() const {
        return m_peakBlobs.blobs();
    }
    // Selection and hover are written by the interaction layer (design
    // section 7.4); these also give the render tests a writer for the
    // Chartreuse highlight branch.
    void setSelectedNotchIdForTest(int id) { m_selectedNotchId = id; }
    void setHoveredNotchIdForTest(int id)  { m_hoveredNotchId = id; }

    // ---- TNF / notch interaction (design section 7) ----
    // Which part of a notch a press at a given pixel would grab.
    // From Thetis console.cs:49032-49067 [v2.10.3.15]: a side-of-centre
    // default, an 8 px minimum on-screen width before edge zones exist at
    // all, a +/- 4 px edge zone, and Shift as an explicit alternative to
    // being near an edge.  Centre is upstream's m_bDraggingNotch; LowEdge
    // and HighEdge are m_bDraggingNotchBW plus the side flag
    // m_BDragginNotchBWRightSide, folded into one value because they are
    // never independently meaningful.
    enum class NotchGrab { None, Centre, LowEdge, HighEdge };

    // Test seams, following the spotMarkersForTest convention above.
    // Public read-only views onto the private pixel-space logic so
    // tst_notch_hit_test can pin the Thetis rules without a real mouse.
    int       notchAtPixelForTest(int x) const;
    NotchGrab notchGrabAtForTest(int id, int x, bool shiftHeld) const;
    int       selectedNotchIdForTest() const { return m_selectedNotchId; }
    int       hoveredNotchIdForTest()  const { return m_hoveredNotchId; }
    // Populate a caller-owned QMenu with the notch actions.  Exists as a
    // seam because QMenu::exec() blocks, so the menu contents cannot be
    // asserted through a synthetic right-click.
    void buildNotchContextMenuForTest(int id, QMenu& menu) {
        buildNotchContextMenu(id, menu);
    }
    // Rotor control plan Task 8: the same seam for a spot label's menu
    // (markerIdx indexes spotMarkersForTest()).
    void buildSpotContextMenuForTest(int markerIdx, QMenu& menu) {
        buildSpotContextMenu(markerIdx, menu);
    }
    // A left click on a spot label, without synthesising one.
    void clickSpotForTest(int markerIdx) {
        if (markerIdx >= 0 && markerIdx < m_spotMarkers.size()) {
            const SpotMarker& sm = m_spotMarkers[markerIdx];
            tuneToSpot(sm.freqMhz * 1.0e6, sm.callsign, sm.bearingDeg);
            emit spotTriggered(sm.index);
        }
    }

    // Rotor control plan Task 8: what answers the spot menu's "Turn beam
    // to CALL (330°)" (shown greyed with the reason without one, or with no
    // rotor or bearing). Not owned.
    void setSpotBeamTurner(SpotBeamTurner* turner) { m_spotBeamTurner = turner; }

    // Overlay-cache seam.  Returns false on a CPU-only build, where there
    // is no cached texture to invalidate.
    bool overlayStaticDirtyForTest() const {
#ifdef NEREUS_GPU_SPECTRUM
        return m_overlayStaticDirty;
#else
        return false;
#endif
    }
    void clearOverlayStaticDirtyForTest() {
#ifdef NEREUS_GPU_SPECTRUM
        m_overlayStaticDirty = false;
#endif
    }
    // Whether the GPU path rebuilds the overlay on every frame (the IMD
    // display is shown), which offscreen tests cannot run.
    bool overlayRebuildsEveryFrameForTest() const { return imdOverlayShown(); }
    // The waterfall timestamp's one-second overlay refresh.
    bool wfTimestampTickerActiveForTest() const {
        return m_wfTimestampTicker && m_wfTimestampTicker->isActive();
    }
    // I2 fix seam: runs the same GPU-path 3D dBm-scale overlay-cache
    // freshness check updateSpectrumLinear() runs every frame (right
    // after processNoiseFloor()), without needing a synthetic FFT frame.
    // No-op on a CPU-only build -- nothing is cached there to go stale,
    // matching overlayStaticDirtyForTest() above.
    void refreshDssScaleOverlayFreshnessForTest() {
#ifdef NEREUS_GPU_SPECTRUM
        updateDssScaleOverlayFreshness();
#endif
    }
    // Task 18: counts real (non-guarded) applies across all fifteen
    // DisplaySettingsModel-bound appliers. Used to prove loadSettings()
    // never drives an apply (it pushes straight into the model, bypassing
    // these appliers entirely) and that a no-op re-apply does not move it.
    int displaySettingsApplyCountForTest() const { return m_displaySettingsApplyCount; }

    // Whether the GPU 3DSS mesh pipeline is up: the runtime half of
    // dssRowSpanSupported() (DssMeshGeometry.h). False until the renderer
    // has built the mesh, false again if it loses it, and always false in a
    // CPU-only build, which has no GPU mesh at all. dssMeshReadyChanged()
    // says when it moves, so a control that depends on it (the Display
    // pill's 3D Span and Slice Shadow) can follow it.
#ifdef NEREUS_GPU_SPECTRUM
    bool dssMeshReady() const { return m_dssMeshReady; }
#else
    bool dssMeshReady() const { return false; }
#endif
    // Test seam: stands in for the renderer bringing the mesh up or losing
    // it, where a test has no GPU to do it. No-op on a CPU-only build.
    void setDssMeshReadyForTest(bool ready) {
#ifdef NEREUS_GPU_SPECTRUM
        setDssMeshReady(ready);
#else
        Q_UNUSED(ready);
#endif
    }

signals:
    void imdMeasurementChanged(const ImdMeasurementFrame& frame);
    void imdPresentationChanged();
    // The GPU 3DSS mesh came up or was lost; see dssMeshReady(). Emitted
    // once per change, on the GUI thread, from inside the renderer's own
    // initialize, render or release step: a receiver that touches widgets
    // should take it queued and read dssMeshReady() when it runs.
    void dssMeshReadyChanged(bool ready);
    // Display pill plan Task 1: resetPanDisplayOverrides() finished. Every
    // per-pan display value may have changed without its own signal (the
    // reload assigns them as a load does), so a control showing one
    // re-reads it from the widget on this.
    void panDisplayReset();
    // Display pill plan Task 2: a display setting that has no change signal
    // of its own took a new value. keyBase is the setting's key base as it
    // is stored ("DisplayGridEnabled", "DisplayWfAgc",
    // "WaterfallNFAGCEnabled", "DisplayShowNoiseFloor", ...). Emitted once
    // per change, after the value is in place; not for a same-value set,
    // and not from loadSettings() or resetPanDisplayOverrides() (the reset
    // says panDisplayReset() instead). setDbmRange() names the end that
    // moved: "DisplayGridMax", "DisplayGridMin", or one after the other.
    void displaySettingChanged(const QString& keyBase);
    // The flags on this pan were just raised above their siblings: a
    // placement pass (updateVfoPositions()), a new flag (addVfoWidget()) or
    // a new front flag (setFrontSliceIndex()). A sibling that has to stay
    // above the flags, an open overlay flyout, raises itself again on this
    // (SpectrumOverlayPanel::raiseOpenFlyout()). Emitted on the GUI thread
    // from inside the call that raised, and only while the pan has a flag.
    // Also emitted when a flag raises its floating buttons outside a
    // placement pass (VfoWidget::floatingButtonsRaised(): a re-show, the
    // station presentation gate, the access line). A placement pass emits
    // it once, at its end, not once per flag or button.
    void flagsRaised();
    // Task 2: Clarity started or stopped driving this pan's waterfall
    // levels (setClarityActive()).
    void clarityActiveChanged(bool active);
    // Follow-up wave F3: this pan was keyed or un-keyed (setDisplayKeyed).
    // Emitted once per edge, after the pan's keyed work is done.
    void displayKeyedChanged(bool keyed);
    /// receiverFlagRegion() changed: a flag moved, folded, opened, showed or
    /// hid.
    void receiverFlagRegionChanged();
    /// Task 78: a click on another device's slice label, with what it says.
    void foreignMarkerClicked(int sliceId, const QString& explanation);
    // 3M-5b: emitted when any TX waterfall colormap property changes.
    // Setup page wires this to update the preview / UI state.
    void txWfSettingsChanged();
    // 2026-05-22 bench fix: emitted after each updateSpectrumLinear
    // completes (m_renderedPixels populated). MainWindow consumes this
    // to push peakDbmInSlicePassband() into WdspEngine's MaxBin detector
    // so the analog S-meter reads what the operator visually sees on
    // the spectrum instead of the raw per-bin FFT value (which can be
    // ~12-17 dB lower due to window-spread power and missing detector
    // pipeline). See peakDbmInSlicePassband doc.
    void spectrumFrameRendered();
    /// A remote pan's granted FFT size or sample rate changed, so its bin
    /// width did (parity Task 17). Setup > Spectrum Defaults refreshes its
    /// readouts from it.
    void remoteSpectrumGrantChanged();

    /// The transmit view window moved or resized. Carries the window the
    /// analyzer must now cover.
    ///
    /// Thetis derives analyzer span and display window from ONE zoom state
    /// (specHPSDR.cs initAnalyzer), so its pixels always cover exactly what
    /// is on screen. Ours are set independently, which is fine until the
    /// operator zooms mid-transmission: the analyzer stays clipped to the
    /// span chosen at key-down, the pan shows something wider, and
    /// SpectrumWidget stretches one across the other -- the trace then sits
    /// wherever that stretch puts it rather than on frequency. Bench
    /// 2026-08-05, "zoom out and the tone is stuck where key down occurred".
    void txViewWindowChanged(double centerHz, double bandwidthHz);

    // Phase 3Q-8: emitted on a left-click while not Connected.
    // MainWindow wires this to showConnectionPanel().
    void disconnectedClickRequest();

    // Emitted when user clicks on spectrum/waterfall to tune
    void frequencyClicked(double hz);
    // Phase 3J-2 Task E1: emitted when the user clicks a spot label
    // (or selects a spot from a cluster badge popup). spotIndex is the
    // SpotMarker::index that was bound to the clicked label so spot
    // sources (Memory, DX cluster, RBN, etc.) can react.
    // From AetherSDR src/gui/SpectrumWidget.h:327 [@0cd4559]
    void spotTriggered(int spotIndex);
    // Rotor control plan Task 8: the operator tuned to a spot (a label
    // click, a cluster popup entry or the menu's Tune), with its call and
    // its Core-served bearing (-1 when none). MainWindow hands it to
    // SpotBeamTurner::spotTuned, which turns the beam only with auto-turn
    // on.
    void spotTuned(const QString& call, double bearingDeg);

    // 2026-05-12 bench fix (Gap #3 from adversarial audit).  Emitted
    // from the right-click context menu's "Remove Spot" action so the
    // SpotModel can purge the spot.  Mirrors AetherSDR
    // SpectrumWidget.h:357 [@0cd4559].
    void spotRemoveRequested(int spotIndex);

    // 2026-05-12 bench fix (Gap #6).  Fires when the mouse enters or
    // leaves a spot label hit-rect.  -1 indicates "no spot under
    // cursor" (use to clear the Spot List highlight).  Drives the
    // panadapter <-> Spot List hover sync.
    void spotHoverIndexChanged(int spotIndex);

    // ---- TNF / notch overlay (design section 8.1) ----
    // Wired per pan by MainWindow::wirePanNotchHandlers, so a marker drawn
    // on a pan acts through that pan's own frequency mapping while the
    // NotchModel the handlers mutate stays global (design D1).  `narrow` is
    // the Shift-held 100 Hz add (Thetis console.cs:40269 [v2.10.3.15]).
    //
    // UNIT BOUNDARY: every frequency here is absolute RF in Hz.  The only
    // MHz quantity in the TNF stack is NotchMarker::freqMhz, converted once
    // in MainWindow::refreshPanNotchMarkers.  Emitters land with the
    // interaction layer (design sections 7.1 through 7.4).
    void notchCreateRequested(double freqHz, bool narrow);
    void notchMoveRequested(int id, double newFreqHz);
    void notchWidthRequested(int id, double widthHz);
    /// Emitted when a notch drag ends, so the coalesced DSP push can flush
    /// immediately and the final position is exact rather than up to one
    /// coalescing window stale.
    void notchDragFinished();
    void notchActiveRequested(int id, bool active);
    void notchRemoveRequested(int id);

    // Emitted when user drags a filter edge
    void filterEdgeDragged(int lowHz, int highHz);
    // Emitted when pan center changes (drag, auto-scroll)
    void centerChanged(double centerHz);
    // Emitted when user scrolls to change bandwidth
    void bandwidthChangeRequested(double newBandwidthHz);

    /// Phase 3F Sub-Epic F Task 10: emitted when zoom-state or operator
    /// toggle changes extended-mode. Consumers (RadioModel via MainWindow)
    /// flip the active slice's widebandExtensionRequested property,
    /// which in turn auto-bypasses Alex BPF + enables the wideband
    /// stream (Task 11 wiring already in place).
    void widebandExtensionStateChanged(bool extensionRequested);

    /// Phase 3F Sub-Epic F Task 12: emitted when the operator clicks
    /// in the extended-pan "wing" (outside the DDC listenable island).
    /// Consumer (MainWindow) retunes the active slice's frequency so
    /// the clicked Hz becomes the new DDC center. Only ever fires when
    /// m_extendedMode is true; click-inside-island routes through the
    /// existing frequencyClicked signal (slice retune in place).
    void ddcRetuneRequested(double frequencyHz);

    // Emitted when user-visible dBm range changes via the scale strip
    // (arrow click, drag-pan on strip body, wheel zoom). Args are the
    // new floor (min) and ceiling (max) in dBm.
    // From AetherSDR SpectrumWidget.cpp:1734 [@0cd4559]
    void dbmRangeChangeRequested(float minDbm, float maxDbm);

    // Display pill plan Task 19: the right-click menu's "Display
    // settings..." line was chosen. The window opens this pan's Display
    // pill.
    void displayPillRequested();

    // Display pill plan Task 19: the right-click menu's Ref Level or Dyn
    // Range was moved, asking for this receive range in dBm (floor,
    // ceiling). Ref Level moves the ceiling and keeps the span; Dyn Range
    // keeps the ceiling and moves the floor. The widget does not take the
    // range itself: the band store does (design S9), and the pan follows
    // the store through setReceiveDbmRange().
    void bandRangeRequested(float minDbm, float maxDbm);

    // Emitted when CTUN mode changes
    void ctunEnabledChanged(bool enabled);

    // Mirrored presentation changed, without an operator gesture. Keep
    // container lights and availability current without sending a pin.
    void remoteCtunStateChanged();

    // R-R3-21 (R3 unfinished controls fix wave M3): peak hold was turned
    // on or off, so a container's Peak button lights at once.
    void peakHoldEnabledChanged(bool enabled);

    // Emitted when m_ddcCenterHz changes (panadapter pan, band jump, etc.).
    // Used by MainWindow to re-push the CTUN slice offset into MaxBin's
    // detector so its scan window tracks the slice even when only the DDC
    // moves.  Without this, panning the spectrum leaves MaxBin scanning
    // bins at the OLD DDC-relative position until the next slice tune.
    void ddcCenterFrequencyChanged(double hz);

    // Display pill plan Task 9: the pan's receive centre frequency took a
    // new value (setCenterFrequency(), setFrequencyRange(), a pan drag: every
    // path through applyViewWindow()). Not emitted while the transmit view
    // span is loaded; that centre is the transmitter's, not the pan's.
    // PanModels follows it to know the pan's band.
    void centerFrequencyChanged(double centreHz);

    // Plan 4 D9 test seam: fires from drawTxFilterOverlay() after pixel
    // coordinates are computed.  Production code ignores this signal;
    // tests use QSignalSpy to verify paint was triggered with the right band.
    // Same pattern as TxChannel::txFilterApplied (Plan 4 D8).
    void txFilterOverlayPainted(int xLeft, int xRight);

    // ── 3DSS Setup-dialog mirror (3D Stacked-Trace Spectrum Plan Task 15) ──
    // Two surfaces now edit the seven DSS controls above (Task 24 adds
    // dssRowDividerChanged, the seventh): the Task 13 right-click overlay
    // menu and Setup -> Display -> 3D View (Display3DSetupPage). Each
    // setter emits its matching signal here after its own
    // state-settles/early-return guard (`if (m_field == v) { return; }`),
    // so the Setup page can follow a change made through the overlay menu
    // (or any other caller) the same way it already follows its own
    // widgets, without a second polling path. Six of the seven are
    // NereusSDR-original signal infrastructure -- source-first governs
    // DSP/radio logic, not this control-surface wiring.
    // dssFloorDepthChanged borrows its guarded-emit shape from AetherSDR
    // SpectrumWidget.h:799 [@1872028c] dssFloorDepthResolved (a real but
    // narrower-purpose upstream signal that fed the SAME overlay menu
    // back for a Flex/Kiwi source-dispatch reason NereusSDR's single-
    // source model does not have) -- see docs/attribution/aethersdr-
    // reconciliation.md, "3D Stacked-Trace Spectrum Plan" section, the
    // Task 15 rows for this file, for the full citation.
    void spectrumRenderModeChanged(int mode);
    void dssFloorDepthChanged(int dB);
    void dssGainChanged(int pct);
    void dssRowSpanChanged(int pct);
    void dssAngleChanged(int pct);
    void dssRowDividerChanged(int n);
    void threeDSliceDepthChanged(bool on);

protected:
#ifdef NEREUS_GPU_SPECTRUM
    void initialize(QRhiCommandBuffer* cb) override;
    void render(QRhiCommandBuffer* cb) override;
    void releaseResources() override;
#endif
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;  // mouse overlay forwarding
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    /// Swallowed over the dBm strip, where right-drag adjusts the range;
    /// ignored elsewhere so PanadapterApplet still gets the pan menu.
    void contextMenuEvent(QContextMenuEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct SpectrumLayout {
        int contentHeight;
        QRect spectrum;
        QRect divider;
        QRect frequency;
        QRect waterfall;
        QRect waterfallChrome; // Full width for right-edge history/LIVE controls.
    };
    SpectrumLayout spectrumLayout() const;

    // Phase 3F Sub-Epic F Task 6: latest wideband bins per ADC.  Each entry
    // is sized 8192 (kOutputBins from WidebandFftEngine) when populated;
    // empty until the first widebandSpectrumReady arrives.  m_extendedMode
    // gates whether the bins are actually painted; the storage is silent
    // until F polish (T7-T10) wires the paint path.
    QVector<float> m_widebandBinsAdc0;
    QVector<float> m_widebandBinsAdc1;
    bool           m_extendedViewAllowed{true};
    // Preserve preconnection/standalone restore geometry until the pan bridge
    // binds a known board/protocol/ADC capability. This is not a saved setting.
    bool           m_localWidebandAvailable{true};
    bool           m_extendedMode{false};
    int            m_widebandAdcIndex{0};
    double         m_widebandAdcRateHz{122880000.0};  // Thetis wbDisplay.cs:4511
    void recomputeExtendedMode();

    /// Pull the visible span back to `bandwidthHz` when extended view is
    /// withdrawn, through setFrequencyRange so the change carries the same
    /// repaint and waterfall handling as any other zoom.
    void applyViewWindowForExtendedClamp(double bandwidthHz);
    /// The Ctrl+wheel zoom's steps for an operator zoom to `bandwidthHz`
    /// around `centreHz` (clamped to 1 kHz .. maxZoomOutBandwidthHz()).
    void applyOperatorZoom(double centreHz, double bandwidthHz);

    /// Overwrite the wing pixels (everything outside the listenable island)
    /// with wideband ADC data, in the linear-power domain the avenger
    /// expects. `dbmOffset` is the same offset the DDC plane is folded
    /// through, so both sides of a boundary land on one dB scale.
    ///
    /// Writes a floor rather than leaving stale island data when no wideband
    /// frame has arrived (P1 boards, wideband disabled, or the first frames
    /// after a zoom-out): a wing showing the DDC's spectrum repeated would
    /// be a display that invents signals.
    ///
    /// `detector` is the one that produced this plane's island pixels; it
    /// picks the DDC noise reference the wings are calibrated against. Passed
    /// per call rather than read off a member because the trace and the
    /// waterfall choose their detectors independently.
    /// `placeholder`, when given, is set true for each wing pixel filled
    /// with the window's bottom because there was no wideband data for it.
    void fillWidebandWings(QVector<float>& linearPixels,
                           int islandFirstPx, int islandLastPx,
                           double dbmOffset,
                           SpectrumDetector detector,
                           QVector<bool>* placeholder = nullptr) const;




    // ---- Phase 3Q-8: disconnect overlay state ----
    // The CPU paintEvent path can paint a QPainter overlay, but the GPU
    // (QRhi) path early-returns and refuses QPainter. To work in both modes
    // we use a child QLabel — Qt composites it on top of the QRhi surface.
    NereusSDR::ConnectionState m_connState{NereusSDR::ConnectionState::Disconnected};
    float m_disconnectFade{1.0f};  // animated; 1.0 connected, 0.4 disconnected
    QPropertyAnimation* m_fadeAnim{nullptr};
    QLabel* m_disconnectLabel{nullptr};

    float disconnectFade() const { return m_disconnectFade; }
    void  setDisconnectFade(float f) { m_disconnectFade = f; update(); }

    // ---- Drawing helpers ----
    void drawGrid(QPainter& p, const QRect& specRect);
    // The vertical grid's line spacing and the frequency scale's label
    // spacing, in Hz, for the span on screen.
    double gridFreqStepHz() const;
    double scaleFreqStepHz() const;
    // The horizontal dB grid lines of the range on screen (the transmit
    // range while the pan is keyed), bottom up: fn(dbm) for each.
    // From Thetis display.cs:9653-9668 [v2.10.3.15]: a line every grid_step
    // dB (num = grid_max - i * grid_step), grid_step being Setup's dB Step.
    // MW0LGE [original inline comment from display.cs:9661]
    // NereusSDR keeps its own origin: the lines count up from the bottom of
    // the range (Thetis counts down from dB Max), left alone here so the
    // picture at the default step of 10 is the one drawn before.
    template <typename Fn>
    void forEachDbGridLine(Fn&& fn) const
    {
        const float bottom = m_refLevel - m_dynamicRange;
        const float step = static_cast<float>(m_dbGridStep);
        for (float dbm = bottom + step; dbm < m_refLevel; dbm += step) {
            fn(dbm);
        }
    }

    /// Dashed markers at the DDC edges while an extended pan is showing
    /// wideband wings. Called from drawGrid so it rides the same cached
    /// static-overlay texture as the rest of the chrome.
    void drawExtendedIslandBounds(QPainter& p, const QRect& specRect);
    void drawSpectrum(QPainter& p, const QRect& specRect);
    // 3DSS CPU fallback surface (Task 10): builds/caches the DssRenderer
    // QImage at up to px, capped by kDssFallbackMaxW/H so a HiDPI/maximized
    // window doesn't rebuild a multi-megapixel image every frame -- the
    // surface is intrinsically low-res and gets stretched on draw either
    // way. Shared by drawSpectrum()'s QPainter path (NEREUS_GPU_SPECTRUM=OFF
    // builds, which never reach renderGpuFrame() at all) and
    // uploadDssFallbackImage() (the RGBA16F-unsupported GPU path). Unlike
    // upstream's buildDssImage(), floorDbm is not a caller-supplied
    // parameter: NereusSDR has one floor source (dssFloorDbm()), not
    // upstream's per-caller value (m_lastDetectDssFloor in 2D, dssFloorDbm()
    // in 3D).
    // From AetherSDR SpectrumWidget.cpp:11880-11894 [@1872028c].
    const QImage& buildDssImage(const QSize& px, int scaleStripPx);
    // Active Peak Hold separate render pass (Q14.1). Called from drawSpectrum()
    // after the fill path so the peak trace sits on top of fill but below
    // the live trace line.  Iterates the per-display-pixel peak array
    // (sized to m_renderedPixels.size()).
    // From Thetis Display.cs:5341 [v2.10.3.13] -- spectralPeaks[i] indexed
    // by display pixel.
    void paintActivePeakHoldTrace(QPainter& p, const QRect& specRect);
    // Peak Blobs render pass (Task 2.6). Draws labeled ellipses at the top-N
    // local maxima. Called from drawSpectrum() after the live trace line so
    // the blobs sit on top of all spectrum content.  Blob.binIndex is now
    // a display-pixel index (0..displayWidth-1) per the pipeline migration.
    // From Thetis Display.cs:5453-5508 [v2.10.3.13].
    void paintPeakBlobs(QPainter& p, const QRect& specRect);
    // Noise-floor overlay: horizontal dashed line across the spectrum at the
    // estimated NF level + corner text label.  Called from both CPU drawSpectrum
    // and GPU renderGpuFrame overlay block so the same NF visuals appear
    // regardless of paint backend.  Reads the state maintained by
    // processNoiseFloor() (m_nfLerpAverage / m_nfFftBinAverage).
    // From Thetis display.cs:5423-5448 [v2.10.3.13] (line + text combined).
    void paintNoiseFloorOverlay(QPainter& p, const QRect& specRect);

    // Source-first port of Thetis processNoiseFloor — display.cs:5866-5912
    // [v2.10.3.13].  Called once per spectrum frame from
    // updateSpectrumLinear after m_renderedPixels is finalised; iterates
    // measurementPixels() (the UNDENTED copy, design section 8.3) to
    // accumulate (count, linear-sum) of bins below the
    // previous-frame estimate (averageCount/averageSum), then updates
    // m_nfFftBinAverage (per-frame) and m_nfLerpAverage (smoothed).
    // Also runs the fast-attack convergence-gated auto-clear from
    // display.cs:5904-5908.
    void processNoiseFloor();
    // A remote pan's noise floor while the Core's trace average warms up
    // (PR 628 re-review): the Core sends the averaged trace only, so for a
    // new picture the pan works out the share of that average the Core's
    // starting value still holds from the frame's encoder sequence. At Log
    // Recursive, for a follower that has no estimate yet, it takes the
    // start out of the trace; otherwise it holds the estimate it has (or
    // none) until the average is warm. Leaves the row to measure in
    // m_noiseFloorMeasurement for this frame.
    void noteRemoteTraceWarmUp(const DisplayCodecFrame& frame);
    void updateReducedSpectrumOverlays();
    void drawWaterfall(QPainter& p, const QRect& wfRect);
    // HIGH SWR / PA safety overlay — ported from display.cs:4183-4201 [v2.10.3.13]
    void paintHighSwrOverlay(QPainter& p);
    // MOX / TX border overlay — ported from display.cs:1569-1593 [v2.10.3.13]
    // Phase 3M-1a H.1.
    void paintMoxOverlay(QPainter& p);
    // Phase 3G-8 commit 10: overlay-only waterfall chrome (filter bands,
    // zero lines, timestamp, opacity dim) split out so the GPU overlay
    // texture can render the same chrome without blitting the waterfall
    // image. Called from drawWaterfall() on the QPainter fallback path
    // and from the GPU overlay build step.
    void drawWaterfallChrome(QPainter& p, const QRect& wfRect);
    void drawFreqScale(QPainter& p, const QRect& r);
    void drawDbmScale(QPainter& p, const QRect& specRect);
    // Task 11 (3D dBm scale). drawDbmScaleChrome/drawDbmScaleLabels are new
    // additions -- ported from AetherSDR's shared 2D/3D helpers -- and are
    // NOT wired into drawDbmScale() above, which stays untouched so the 2D
    // path sees zero behavioural change. Only drawDbmScale3D() calls them.
    // From AetherSDR SpectrumWidget.cpp:17223-17335 [@1872028c]
    void drawDbmScaleChrome(QPainter& p, const QRect& specRect);
    void drawDbmScaleLabels(QPainter& p, const QRect& specRect,
                            float topDbm, float rangeDb);
    // In 3D mode, a single right-side axis cannot be pixel-exact for every
    // perspective row. Keep it as a full-height amplitude reference anchored
    // to the 3D floor, so plain drag visibly shifts the dBm numbers and
    // Ctrl/Meta-drag changes the span.
    void drawDbmScale3D(QPainter& p, const QRect& specRect, float floorDbm);
    void drawBandPlan(QPainter& p, const QRect& specRect);

    // ── Waterfall scrollback (sub-epic E) ─────────────────────────────────
    // From AetherSDR SpectrumWidget.h:402-413 [@0cd4559]
    void drawTimeScale(QPainter& p, const QRect& wfRect);
    QRect waterfallTimeScaleRect(const QRect& wfRect) const;
    QRect waterfallLiveButtonRect(const QRect& wfRect) const;
    int   waterfallStripWidth() const;
    void  ensureWaterfallHistory();
    bool  historyResizeSettling() const;
    void  rebuildWaterfallViewport();
    void  rebuildWaterfallViewport(double centerHz, double bandwidthHz);
    void  setWaterfallLive(bool live);
    void  appendHistoryRow(const QRgb* rowData, qint64 timestampMs,
                           bool imdEligible = false, quint64 acceptedOrdinal = 0);
    // The IMD correction edge's annotation on history row rowSequence, if
    // that row is the first eligible one accepted after the edge.
    void  recordImdAnnotationForRow(quint64 rowSequence, bool imdEligible,
                                    quint64 acceptedOrdinal);
    int   historyRowIndexForAge(int ageRows) const;
    std::optional<qreal> imdEventY(quint64 sequence, qreal glideRows) const;
    QString pausedTimeLabelForAge(int ageRows) const;
    void  reprojectWaterfall(double oldCenterHz, double oldBandwidthHz,
                             double newCenterHz, double newBandwidthHz);
    // (clearWaterfallHistory moved to public slots: in sub-epic E task 4 review.)

    // drawVfoMarker walks sliceMarkerGeometry() and hands each entry to
    // drawSliceMarker, which paints one slice's passband fill, filter edges,
    // VFO centre line and triangle. Split in Phase 3F so a pan hosting several
    // slices paints one marker per slice instead of one per pan.
    void drawVfoMarker(QPainter& p, const QRect& specRect, const QRect& wfRect);
    void drawSliceMarker(QPainter& p, const QRect& specRect, const QRect& wfRect,
                         const SliceMarkerGeometry& g);
    // Task 78: other devices' slices, after this window's own.
    void drawForeignMarkers(QPainter& p, const QRect& specRect, const QRect& wfRect);
    QVector<ForeignSliceMarker> m_foreignMarkers;
    bool m_ownSliceMarkerPresentationAllowed{true};
    QHash<int, QRect> m_foreignLabelRects;
    void drawCursorInfo(QPainter& p, const QRect& specRect);

    // ---- Spot overlay (Phase 3J-2 Task E1) ----
    // From AetherSDR src/gui/SpectrumWidget.cpp:4497-4633 [@0cd4559]
    // Algorithm preserved verbatim:
    //  - Color priority: override -> DXCC -> spot color -> default cyan.
    //  - Multi-level vertical stacking with collision-induced nudge-down.
    //  - Re-scan from top after each nudge (handles cascade overlaps).
    //  - Overflow into +N cluster badges at maxBottom + 2; cluster bin
    //    width = 40 px.
    //  - Vertical dotted tick from spectrum bottom to label.
    //  - Optional background pill with configurable opacity.
    //  - Click-to-tune via frequencyClicked(hz) signal (NereusSDR Hz units;
    //    AetherSDR emits MHz, the call site multiplies by 1e6).
    //  - Cluster badge popup menu with formatted spot lines.
    void drawSpotMarkers(QPainter& p, const QRect& specRect);
    void showSpotClusterPopup(const SpotCluster& cluster, const QPoint& globalPos);

    // ---- TNF / notch overlay (design section 8.2) ----
    // Geometry ported unchanged from AetherSDR drawTnfMarkers
    // (src/gui/SpectrumWidget.cpp:13503-13554 [@c6481cbf]): translucent
    // fill over the full spectrum height, diagonal hatch clipped to the
    // notch rect, 1 px edge lines at both boundaries, downward triangle
    // grab handle at the top.  Colours come from Thetis instead
    // (display.cs:386-390, 8691-8722 [v2.10.3.15]).
    void drawNotchMarkers(QPainter& p, const QRect& specRect);
    QColor notchColor(const NotchMarker& n) const;

    // The SINGLE notch geometry source.  Both paint call sites pass this
    // rect, and the interaction layer's pixel hit test builds from it, so
    // hit boxes cannot drift away from the drawn markers.  Reproduces the
    // shared spectrumLayout() rectangle on both render paths.
    QRect notchSpecRect() const;

    // ---- TNF / notch interaction (design section 7) ----
    // notchMarkerById: linear id lookup over the render-side mirror.
    // notchAtPixel:    pixel-space port of Thetis
    //                  MNotchDB.NotchThatSurroundsFrequencyInBW.
    // notchGrabAt:     edge-vs-centre discrimination for a press.
    // buildNotchContextMenu: right-click menu over a notch marker.
    const NotchMarker* notchMarkerById(int id) const;
    int       notchAtPixel(int x, const QRect& specRect) const;
    NotchGrab notchGrabAt(int id, int x, bool shiftHeld,
                          const QRect& specRect) const;
    void      buildNotchContextMenu(int id, QMenu& menu);
    // Rotor control plan Task 8: the right-click menu over a spot label.
    void      buildSpotContextMenu(int markerIdx, QMenu& menu);
    // Tune to a spot through requestTune and, when the tune went out, say
    // so on spotTuned.
    void      tuneToSpot(double hz, const QString& call, double bearingDeg);

    // ---- Visual notch (design section 8.3) ----
    /// True when this frame's pixels are to be dented: the toggle is on, we
    /// are not transmitting, the master TNF enable is on, and at least one
    /// marker exists. Mirrors the Thetis gate plus the _tnf_active half of
    /// its per-notch _Use flag.
    bool visualNotchWillDent() const;
    /// Subtract the notch skirts from `pixels` in place. Safe on either
    /// plane's array; the caller decides which.
    void applyVisualNotchDent(QVector<float>& pixels) const;
    /// The pristine (undented) spectrum pixels. Consumers that MEASURE
    /// rather than draw read this, never m_renderedPixels.
    const QVector<float>& measurementPixels() const;

    // ---- TX filter overlay (Plan 4 D9, Cluster E) ----
    // drawTxFilterOverlay: panadapter band fill + border lines + label.
    //   Called when m_txFilterVisible is set; gating is at the call site.
    // drawTxFilterWaterfallColumn: waterfall column fill, MOX-gated.
    //   Called when m_showTxFilterOnRxWaterfall && m_moxOverlay.
    // Per deskhpsdr/transmitter.c:2136-2186 [@120188f] for IQ-space mapping.
    void drawTxFilterOverlay(QPainter& p, const QRect& specRect);
    // The XIT the TX filter overlay adds to the VFO: none while keyed with
    // DUP on (parity Task 31).
    double txFilterXitHz() const;
    // setMoxOverlay's span half: park the receive view and load the
    // transmit span, or give the receive view back (parity Task 31 moved
    // them out so a DUP change while keyed makes the same swap).
    // pushTxWaterfallRow's row: the visible bins, calibrated as the trace
    // (parity Task 31).
    QVector<float> txWaterfallRow(const QVector<float>& binsDbm) const;
    void loadTransmitSpan();
    void restoreReceiveSpan();
    // Parity Task 31: a DUP change while keyed resets the blob maxima and
    // the active peak hold (Thetis display.cs:514-521 [v2.10.3.15]).
    void resetPeaksForDuplexChange();
public:
    /// Thetis's peak reset for one receiver (ResetBlobMaximums(rx, true) +
    /// ResetSpectrumPeaks(rx)): the blobs and the active peak hold empty and
    /// wait out the 500 ms display delay. applyViewWindow calls it on every
    /// moved or resized view; MoxDisplayController on every MOX edge and on
    /// the radio connecting.
    void resetPeaks();
private:
    void drawTxFilterWaterfallColumn(QPainter& p, const QRect& wfRect);

    // ---- Two-tone IMD overlay (Phase 3M-4 Task 12) ----
    // From Thetis display.cs:5008 [v2.10.3.13]:
    //   show_imd_measurements = local_mox && _testing_imd
    //                           && _show_imd_measurements && displayduplex;
    // Renders cached product markers/guides; ImdMeasurementPanel owns the numerical chrome.
    // Called from paintEvent (CPU path) and from the GPU overlay rebuild
    // (painting consumes the accepted measurement frame).
    void drawImdOverlay(QPainter& p, const QRect& specRect);
    ImdInputContext imdInputContext(quint64 sequence, bool sourceEligible) const;
    void acceptImdVisibleSpectrum(quint64 sequence, bool sourceEligible);
    // The IMD show condition: while it holds the GPU path rebuilds the
    // static overlay every frame (its markers and gliding annotations).
    bool imdOverlayShown() const;
    void invalidateImdMeasurement();
    qint64 imdFreshnessTimeoutMs() const;
    void checkImdFreshness(qint64 nowMs);

    // Shared audio→IQ-space conversion used by both draw methods.
    // Returns {iqLowHz, iqHighHz} signed offsets from the VFO center.
    // Per deskhpsdr/transmitter.c:2136-2186 [@120188f].
    std::pair<int,int> txAudioToIq(int audioLow, int audioHigh, DSPMode mode) const;

    // ---- Coordinate helpers ----
    int    hzToX(double hz, const QRect& r) const;
    double xToHz(int x, const QRect& r) const;
    int    dbmToY(float dbm, const QRect& r) const;
    float  dbmToYf(float dbm, const QRect& r) const; // sub-pixel variant for antialiased trace

    // Band plan strip height — when the band plan is enabled, the bottom
    // m_bandPlanFontSize + 4 pixels of every spectrum-coordinate rect are
    // RESERVED for the strip.  dbmToY / dbmToYf subtract this from the
    // effective drawing area so the dBm floor maps to the TOP edge of the
    // band plan strip rather than the bottom of the panel.  Result: every
    // spectrum overlay (trace, NF line+box+text, peak hold, peak blobs,
    // grid lines, dBm-scale labels) automatically anchors at the band plan
    // top — they all derive their Y from these helpers.  drawBandPlan
    // itself paints below this carve-out using r.bottom() directly so the
    // strip still lands at the panel bottom.
    int bandPlanStripHeight(const QPaintDevice* device = nullptr) const;

    // 1-Hz-bandwidth normalisation shift — returns -10*log10(binWidthHz)
    // when m_dispNormalize is on, 0 otherwise.  Applied inside dbmToY /
    // dbmToYf so the entire spectrum (trace + every overlay derived from
    // these helpers) renormalises in lockstep when the toggle flips.
    // Mirrors Thetis SetDisplayNormOneHz (specHPSDR.cs:325) at render time.
    float normalizeShiftDb() const;

    // Returns kDbmStripW when the dBm scale strip is visible, 0 otherwise.
    // Used everywhere a rect excludes the right-edge strip so that hiding
    // the strip automatically gives the spectrum full widget width.
    int    effectiveStripW() const;

    // Returns the first and last FFT bin indices visible in the current
    // display window (m_centerHz ± m_bandwidthHz/2), mapped against
    // the DDC center and sample rate. Clamped to [0, binCount-1].
    std::pair<int, int> visibleBinRange(int binCount) const;

    // ---- Waterfall helpers ----
    // Feeds a post-pipeline waterfall row (display-pixel dBm, length
    // displayWidth).  Caller is updateSpectrumLinear after the waterfall
    // detector + avenger have run.  Inner AGC + NF-AGC + threshold compute
    // operate on display pixels per Thetis Display.cs:6713-6738
    // [v2.10.3.13] (waterfall_data[i] indexed by pixel).
    void   pushWaterfallRow(const QVector<float>& wfPixelsDbm, bool imdEligible = false,
                            quint64 acceptedOrdinal = 0);
    // One row's step of Clarity's drawn floor toward its target
    // (setClarityFloorTargetDbm).
    void   slewClarityFloor();
    QRgb   dbmToRgb(float dbm) const;
    void traceWaterfallState(const QVector<float>& calibratedPixelsDbm,
                             const QRgb* scanline, int width, qint64 rowTimeMs);

    // 3D Speed (Task 24): folds up to effectiveDssRowDivider() waterfall
    // rows into one 3D row by per-column peak-hold before handing the
    // result to pushDssRow(), so a divider above 1 does not lose a
    // one-tick burst. Called from pushWaterfallRow() downstream of the
    // stop-on-TX gate: same placement rule pushDssRow() documented
    // directly before this task; see the call site there for why
    // placement matters.
    void accumulateDssRow(const QVector<float>& wfPixelsDbm);

    // 3DSS: resamples + stores one already-folded row into the
    // stacked-trace ring. Called only from accumulateDssRow(), once its
    // fold count reaches effectiveDssRowDivider(): wfPixelsDbm is the
    // folded exact row (m_dssFoldRow), and the wide (off-screen) channel
    // is built from the folded m_dssFoldFullBins rather than
    // m_lastFullBinsDbm directly (Task 24), so a burst that lasts a
    // single waterfall tick still reaches both channels even when
    // several ticks are folded into one 3D row. Task 8 extended this to
    // also feed the wide channel via pushRowWithWide.
    void pushDssRow(const QVector<float>& wfPixelsDbm);

    // The glide-clock increment per millisecond of wall-clock delta
    // between display ticks, scaled by effectiveDssRowDivider() so the
    // scroll phase still completes exactly once per pushed 3D row
    // regardless of how many waterfall ticks that row folds (Task 24).
    // Used by the m_displayTimer lambda and exposed by
    // dssScrollIncrementForTest().
    float dssScrollIncrement(int deltaMs) const;

    // ---- FFT pipeline state ----
    // Single Thetis-faithful pipeline: linear-power FFT bins -> visible
    // slice -> detector -> avenger -> dBm display pixels.  Spectrum and
    // waterfall keep separate detector + avenger state per WDSP per-plane
    // model (each ANALYZER_INFO[] entry has its own DetType + AvMode).
    QVector<float> m_fullLinearBins;       // FFTEngine input cache (|X[k]|²)
    QVector<float> m_renderedPixels;       // spectrum avenger output (dBm)
    QVector<float> m_wfRenderedPixels;     // waterfall avenger output (dBm)
    // 3DSS wide channel feed: m_fullLinearBins converted to dBm at full
    // bin resolution (unsliced -- NOT visibleBinRange()'d), so
    // buildDssWideRow() can window bins outside the current view. Cached
    // once per FFT frame in updateSpectrumLinear(), gated on Mode3D since
    // nothing reads it in 2D. See buildDssWideRow()/pushDssRow() (.cpp).
    QVector<float> m_lastFullBinsDbm;

    // Equivalent noise bandwidth of the current DDC FFT window. The normal
    // reducer receives the value per call. Extended mode also caches it so
    // widebandBandwidthNormalisationDb can align wing and island power.
    double m_fftWindowEnb{1.0};

    // R1 Task 5: the crop-and-reduce stage (visible-slice -> detector ->
    // avenger) moved to core as NereusSDR::SpectrumReducer, which owns what
    // used to be m_displayLinearPixels / m_wfDisplayLinearPixels and the two
    // SpectrumAvenger instances.  One reducer per display plane, because
    // WDSP gives each ANALYZER_INFO[] plane its own DetType + AvMode.
    // See SpectrumAvenger.h for the av_mode wire-format mapping (-1 peak /
    // 0 none / 1 recursive / 2 window / 3 log-recursive);
    // analyzer.c:464-554 [v2.10.3.13] is the verbatim port.
    //
    // The widget still supplies ReducerConfig::pixels from its own geometry,
    // so rendering is unchanged; the dependency is now explicit rather than
    // baked into the reduction stage.
    NereusSDR::SpectrumReducer m_spectrumReducer;
    NereusSDR::SpectrumReducer m_waterfallReducer;

    // Extended mode needs one composition buffer and avenger per plane so
    // the DDC island and wideband wings can be combined in linear power.
    // Normal rendering and daemon endpoints continue to use SpectrumReducer.
    QVector<float> m_extendedSpectrumLinearPixels;
    QVector<float> m_extendedWaterfallLinearPixels;
    NereusSDR::SpectrumAvenger m_extendedSpectrumAvenger;
    NereusSDR::SpectrumAvenger m_extendedWaterfallAvenger;
    // The extended trace's wing pixels that held no data this frame, and
    // per pixel the frames with data since it last held none.
    QVector<bool> m_extendedSpectrumPlaceholder;
    QVector<int> m_extendedSpectrumFramesWithData;
    // The row the noise floor measures when it is not the trace: the
    // average with its starting value taken out while it warms up, with NaN
    // for wing pixels that hold no data (or, on a remote pan, the Core's
    // trace so treated: noteRemoteTraceWarmUp). Fresh only for the frame it
    // was written for.
    QVector<float> m_noiseFloorMeasurement;
    bool m_noiseFloorMeasurementFresh {false};
    // Whether the last row the noise floor took had no measured pixel (a
    // hold), so the fast attack runs its time again when data comes back.
    bool m_noiseFloorHeldLastFrame {false};
    // When that hold began, on the display clock.
    qint64 m_noiseFloorHoldStartMs {0};
    // A remote pan's view of the Core's trace average: the share of it the
    // Core's starting value held when the current context started (1 for a
    // new picture, 0 once warm or when the mode has no starting value), the
    // share at the last frame, and whether the pan takes the start out of
    // the trace (a follower with no estimate yet at Log Recursive) rather
    // than hold its estimate (noteRemoteTraceWarmUp).
    double m_remoteTraceStartShareAtContext {0.0};
    double m_remoteTraceStartShare {0.0};
    bool m_remoteTraceTakeStartOut {false};

    // ---- Frequency range ----
    double m_centerHz{14225000.0};    // 14.225 MHz default
    double m_bandwidthHz{192000.0};   // 192 kHz default (P1 base sample rate)
    double m_ddcCenterHz{14225000.0};   // DDC hardware center frequency
    double m_sampleRateHz{768000.0};    // DDC sample rate

    // ---- Display range ----
    // From Thetis display.cs:1743-1754. Init values must match the
    // ship defaults in loadSettings() (SpectrumWidget.cpp ~line 392) —
    // any divergence shows up if a code path reads these before
    // loadSettings runs, and the slider in SetupDialog briefly shows
    // the stale value. Calibrated 2026-04-30 against a residential
    // HF noise floor; see loadSettings comment for rationale.
    float  m_refLevel{-48.0f};        // top of display (dBm)
    float  m_dynamicRange{68.0f};     // range in dB (bottom = refLevel - dynamicRange)

    // ---- Waterfall ----
    QImage m_waterfall;               // ring buffer (Format_RGB32)
    int    m_wfWriteRow{0};

    // ── Waterfall scrollback (sub-epic E) ─────────────────────────────────
    // From AetherSDR SpectrumWidget.h:493-502 [@0cd4559]
    QImage          m_waterfallHistory;            // RGB32 ring buffer
    QVector<qint64> m_wfHistoryTimestamps;         // parallel; per-row wall-clock ms
    QVector<quint64> m_wfHistorySequences;         // parallel accepted-row identities
    quint64 m_wfNextRowSequence{0};
    struct WaterfallRowWindow {
        double centerHz{0.0};
        double bandwidthHz{0.0};
    };
    QVector<WaterfallRowWindow> m_wfHistoryWindows; // original RF window per row
    int             m_wfHistoryWriteRow{0};        // LIFO; index 0 = newest
    mutable QElapsedTimer m_historyTraceViewportClock;
    int             m_wfHistoryRowCount{0};        // saturates at capacity
    int             m_wfHistoryOffsetRows{0};      // 0 = newest visible at top
    bool            m_wfLive{true};                // pause/live state
    bool            m_draggingTimeScale{false};    // gesture flag
    int             m_timeScaleDragStartY{0};      // anchor Y at mousedown
    int             m_timeScaleDragStartOffsetRows{0};

    // Default depth (overridden at runtime by m_waterfallHistoryMs from AppSettings).
    // From AetherSDR SpectrumWidget.h:502 [@0cd4559]
    static constexpr qint64 kDefaultWaterfallHistoryMs = 20LL * 60LL * 1000LL;

    // (kMaxWaterfallHistoryRows declared at the top of the public block —
    // moved there in sub-epic E task 2 so test-shims can mirror the
    // capacity-clamp formula without befriending the class.)

    // Runtime-configurable depth; persisted as AppSettings("DisplayWaterfallHistoryMs").
    // NereusSDR-side enhancement — see plan §authoring-time #1.
    qint64          m_waterfallHistoryMs{kDefaultWaterfallHistoryMs};

    // Debounce timer for ensureWaterfallHistory() during rapid resize / slider drag.
    // From AetherSDR SpectrumWidget.h:559 [@2bb3b5c]
    // (debounce timer added by unmerged AetherSDR PR #1478 — see plan §authoring-time #2)
    QTimer*         m_historyResizeTimer{nullptr};
    // Whole-image rescales of m_waterfallHistory, for the resize tests.
    quint64         m_wfHistoryRescales{0};
    QTimer          m_displayTimer;
    bool            m_hasNewSpectrum{false};

    // ---- Waterfall display controls ----
    // From AetherSDR SpectrumWidget defaults + Thetis display.cs:2522-2536
    WfColorScheme m_wfColorScheme{WfColorScheme::Default};
    // Display pill plan Task 3: 60 and 57 reproduce the look before the
    // Clarity level model (ClarityLevels::kColorGainDefault / kBlackLevelDefault).
    int    m_wfColorGain{60};         // 0-100
    int    m_wfBlackLevel{57};        // 0-100, 50 = black at the noise floor — keep in sync with loadSettings ship default
    // Waterfall uses its own dBm range (narrower than spectrum for better contrast).
    // Persistent user-configured thresholds (saved/loaded as DisplayWfHigh/LowLevel).
    // Ship defaults — keep in sync with loadSettings (SpectrumWidget.cpp)
    // From Thetis display.cs:2522 + 2536 [v2.10.3.13] waterfall_high_threshold /
    // waterfall_low_threshold (the persisted user values, distinct from
    // _RX1waterfallPreviousMinValue runtime AGC tracking state).
    float  m_wfHighThreshold{-62.0f};
    float  m_wfLowThreshold{-122.0f};

    // Render-active mirror, equivalent to Thetis's per-render
    // local high_threshold / low_threshold at display.cs:6575/6584/6590
    // [v2.10.3.13]. AGC / NF-AGC / Clarity override these; the
    // persistent fields above are never touched by runtime overrides.
    // Issue #230 fix.
    float  m_wfActiveHighThreshold{-62.0f};
    float  m_wfActiveLowThreshold{-122.0f};

    // ---- Smoothing constant ----
    // From AetherSDR SpectrumWidget.h:417 — SMOOTH_ALPHA = 0.35f
    static constexpr float kSmoothAlpha = 0.35f;

    // ---- Layout constants ----
    // From gpu-waterfall.md:590-593
    float  m_spectrumFrac{0.40f};     // 40% spectrum, 60% waterfall
    // From AetherSDR src/gui/SpectrumWidget.h:1574-1578 [@1e0718ad]
    // (freqScaleHeight() here; the default text is 10 px, not 8 pt):
    // Height of the frequency scale bar at the default 8 pt label size.
    // Use freqScaleH() in layout code — it grows with the user's label
    // font so larger text never clips (#3501).
    static constexpr int kFreqScaleH = 28;  // Taller for easier grab target
    static constexpr int kDividerH = 4;
    static constexpr int kDbmStripW = 36;
    // Height of each arrow button at the top of the dBm strip.
    // From AetherSDR SpectrumWidget.h:539 [@0cd4559]
    static constexpr int kDbmArrowH = 14;

    // ---- Spectrum fill ----
    // From AetherSDR defaults
    QColor m_fillColor{0x00, 0xe5, 0xff};  // cyan
    // Display pill plan Task 5: the trace line's own colour; invalid while
    // none is chosen, and the trace is then drawn in m_fillColor.
    // From AetherSDR src/gui/SpectrumWidget.h:1423 [@1e0718ad]
    QColor m_traceColor;

    // Display pill plan Task 5: the spectrum background.
    // From AetherSDR src/gui/SpectrumWidget.h:1726-1736 [@1e0718ad]
    // Background image
    QImage  m_bgImage;
    QImage  m_bgScaled;     // cached at current specRect size
    QString m_bgImagePath;
    QSize   m_bgScaledSize;
    int     m_bgOpacity{80};  // 0=full image, 100=solid dark (default 80%)
    // Solid fill colour painted BENEATH the bg image (#1741).  Default
    // matches the pre-feature compositing colour so visual is unchanged
    // until the operator picks something else via the spectrum overlay
    // menu's "Background:" colour swatch.
    QColor  m_bgFillColor{QColor(0x0a, 0x0a, 0x14)};
    // NereusSDR: the background is drawn only while this is on (S6).
    bool    m_bgEnabled{false};
    // Whether this frame gets the background: on, and the 2D spectrum.
    bool drawsSpectrumBackground() const;
    // The fill colour, then the picture over it. Shared by paintEvent() and
    // the layer renderGpuFrame() uploads.
    void paintSpectrumBackground(QPainter& p, const QRect& specRect);
    // (Re)reads the picture from m_bgImagePath, or drops it while the
    // background is off or no picture is chosen.
    void reloadBackgroundImage();
    // The background changed: the GPU layer is repainted on the next frame.
    void markBackgroundDirty();
    float  m_fillAlpha{0.70f};
    bool   m_panFill{true};

    // ---- Phase 3G-8 commit 3: spectrum renderer state ----

    AverageMode m_averageMode{AverageMode::Logarithmic};
    // Spectrum + waterfall averaging time constants in milliseconds, with
    // per-side back-multiplier alphas computed via Thetis math:
    //   α = exp(-1 / (fps × τ))  [specHPSDR.cs:358 / :374, v2.10.3.13]
    // Defaults match Thetis (setup.cs udDisplayAVGTime_ValueChanged = 30 ms,
    // udDisplayAVTimeWF_ValueChanged = 120 ms).
    int         m_spectrumAverageTimeMs{30};
    int         m_waterfallAverageTimeMs{120};
    // Recomputed by recomputeAverageAlphas() whenever fps or time changes.
    float       m_spectrumAverageAlpha{0.0f};
    float       m_waterfallAverageAlpha{0.0f};

    // ---- Task 2.1: Detector + Averaging split (handwave fix from 3G-8) ----
    // Ported from Thetis specHPSDR.cs:302-415 [v2.10.3.13].
    SpectrumDetector  m_spectrumDetector{SpectrumDetector::Peak};
    SpectrumAveraging m_spectrumAveraging{SpectrumAveraging::LogRecursive};
    SpectrumDetector  m_waterfallDetector{SpectrumDetector::Peak};
    SpectrumAveraging m_waterfallAveraging{SpectrumAveraging::None};

    bool        m_peakHoldEnabled{false};
    int         m_peakHoldDelayMs{2000};
    // Per-display-pixel running max (replaces full-bin m_peakHoldBins).
    // Sized to displayWidth on first updateSpectrumLinear and re-sized on
    // resize.  Decay timer below resets it on tick.
    QVector<float> m_pxPeakHold;
    QTimer*     m_peakHoldDecayTimer{nullptr};

    // Active Peak Hold trace (Task 2.5). Separate from the legacy peak hold
    // above; rendered as a distinct pass per Q14.1.
    // From Thetis display.cs m_bActivePeakHold [v2.10.3.13].
    ActivePeakHoldTrace m_activePeakHold;
    // NereusSDR-original — distinct trace colour so the peak trace stays
    // visible even when the data-line colour is changed (e.g. "Reset to
    // Smooth Defaults" sets the data line to white). Default gold for high
    // contrast against the typical clarity-blue palette and against white.
    QColor m_activePeakHoldColor{0xFF, 0xD7, 0x00, 0xFF};

    // Peak Blobs detector (Task 2.6). Top-N local maxima with hold/decay.
    // From Thetis display.cs:4395-4714, 5453-5508 [v2.10.3.13].
    // Inline attribution from ported range — display.cs:4829 //MW0LGE [2.10.1.0] fix issue #137;
    // display.cs:4972 //[2.10.3.9]MW0LGE raw grid control option; display.cs:5109 //MW0LGE not used.
    PeakBlobDetector m_peakBlobs;
    // From Thetis display.cs:8434 [v2.10.3.13] m_bDX2_PeakBlob = Color.OrangeRed
    QColor m_peakBlobColor{0xFF, 0x45, 0x00, 0xFF};
    // From Thetis display.cs:8435 [v2.10.3.13] m_bDX2_PeakBlobText = Color.Chartreuse
    QColor m_peakBlobTextColor{0x7F, 0xFF, 0x00, 0xFF};
    // 2026-05-26 KG4VCF perf polish: pre-rendered blob marker pixmap.
    // paintPeakBlobs blits this instead of calling QPainter::drawEllipse
    // each blob; drawEllipse went through Qt's parallel raster span
    // path and the main thread blocked in QLatch::waitInternal waiting
    // for QThreadPool workers (starved under build load).  Pixmap blit
    // is a memcpy-style operation that skips the raster engine.
    // Rebuilt when m_peakBlobColor changes.
    QPixmap m_blobMarkerPixmap;
    void rebuildBlobMarkerPixmap();

    // 2026-05-26 KG4VCF perf polish: pre-allocated scratch vectors
    // for the CPU spectrum / peak-hold paint paths.  Previously
    // drawSpectrum() allocated `QVector<QPointF> points(n)` (~16 KB)
    // and `QVector<QPointF> peakPoints(n)` (~16 KB) per paint.  GPU
    // path is hot on macOS so these don't run there, but on CPU
    // fallback they are the dominant per-paint allocation source.
    // Hoisted to members; drawSpectrum resizes (no-op when same n).
    QVector<QPointF> m_specPointsScratch;
    QVector<QPointF> m_specPeakPointsScratch;
    // QPainterPath is also re-constructed per paint in the fill +
    // peak-hold blocks; caching as a member + clear()-and-reuse
    // amortises its internal QVector allocation.
    QPainterPath m_specFillPathScratch;
    QPainterPath m_specPeakPathScratch;

    // ControlRanges::kDisplayLineWidthDefaultPx (Thetis udDisplayLineWidth's
    // Value, setup.designer.cs:53039-53043 [v2.10.3.15]).
    float       m_lineWidth{1.0f};
    bool        m_gradientEnabled{false};

    // Ported from Thetis Display.RX1DisplayCalOffset (display.cs:1372).
    // Display pill plan Task 8: the operator's Cal Offset only.
    float       m_dbmCalOffset{0.0f};
    // The station's receive offset (setStationCalOffsetDb). Not a setting.
    float       m_stationCalOffsetDb{0.0f};

    // ---- 3M-5b: TX-specific waterfall colormap settings ----
    // Active during MOX per Thetis display.cs:6506-6595 [v2.10.3.13+501e3f51]
    // inline per-frame branch.  No state machine.
    int           m_txWfLowLevel{-70};        // dBm, from Thetis Display.cs:1925 [v2.10.3.13+501e3f51] tx_wf_amp_min default
    int           m_txWfHighLevel{30};         // dBm, from Thetis Display.cs:1911 [v2.10.3.13+501e3f51] tx_wf_amp_max default
    WfColorScheme m_txWfPalette{WfColorScheme::Enhanced}; // Thetis Display.cs:428 [v2.10.3.13+501e3f51] _tx_color_scheme = ColorScheme.enhanced
    QColor        m_txWfLowColor{Qt::black};   // Thetis Display.cs:2516 [v2.10.3.13+501e3f51] waterfall_low_color_tx = Color.Black
    QString       m_rxWfGradient;
    std::array<QRgb, 101> m_rxCustomLut{};
    bool                  m_rxCustomLutValid{false};
    quint64               m_rxCustomPaletteRevision{0};
    QString       m_txWfGradient;              // encoded gradient string for Custom palette
    // 3M-5c: 101-entry color LUT cached from m_txWfGradient encoded text.
    // Mirrors the Thetis WaterfallTXGradient() 101-color array built at
    // setup.cs:33314-33322 [v2.10.3.13+501e3f51]. Rebuilt inside
    // setTxWfGradient() whenever the encoded string changes; consumed by
    // dbmToRgb() when isTx && palette == Custom. m_txCustomLutValid stays
    // false until the LUT has been populated at least once (signals "fall
    // back to kCustomFallbackStops" so the Custom palette behaves usefully
    // before the user picks one in Setup -> Display -> TX).
    std::array<QRgb, 101> m_txCustomLut{};
    bool                  m_txCustomLutValid{false};

    // 3M-5d: when true, updateSpectrumLinear skips its internal
    // pushWaterfallRow because the waterfall is being driven externally
    // via pushTxWaterfallRow (TxAnalyzer's pixout=1 stream).  Only set
    // true during MOX, by MainWindow.
    bool                  m_txExternalWaterfall{false};
    bool m_remoteSpectrum{false};
    DisplayCodecContext m_remoteCodec;
    NereusSDR::RemoteSpectrumCapture m_remoteCapture;
    NereusSDR::RemoteSpectrumCapture m_pendingRemoteCapture;
    double m_remoteExactCentreHz{0.0};
    double m_remoteExactSpanHz{0.0};
    // Parity Task 17: the Core's granted FFT size and frame rate for this
    // remote pan (0 until its first context).
    int m_remoteFftSize{0};
    int m_remoteFps{0};
    // The Core's frames per waterfall row for this remote pan (0 until its
    // first context): with m_remoteFps, the period its rows come at.
    int m_remoteFramesPerLine{0};
    // Display pill plan Task 15 (setRemoteDecimation).
    int m_remoteDecimation{1};
    RemoteContextPicture m_remoteContextPicture{RemoteContextPicture::New};
    bool m_remoteWidebandAvailable{false};
    bool m_remoteWidebandActive{false};
    double m_remoteWidebandAdcRateHz{0.0};
    QVector<float> m_pendingRemoteWide;
    // R-R3-21 / R-R3-08: remote waterfall rows waiting for the ticker,
    // oldest first, each with the RF window it was captured at. The ticker
    // draws one a tick, two while more than two wait (a late burst plays out
    // instead of being squeezed into one row).
    // NereusSDR-original diagnostic metadata; it never selects or changes a row.
    struct WaterfallRowDiagnostics {
        std::optional<quint32> encoderSequence;
        int presentationKind{-1}; // RemoteDisplayPresenter::Kind; -1 when unavailable.
        int sliceId{-1};
        quint64 bindingStreamEpoch{0};
    };
    struct RemoteWaterfallRow {
        QVector<float> pixelsDbm;
        QVector<float> wideDbm;
        double centreHz{0.0};
        double spanHz{0.0};
        NereusSDR::RemoteSpectrumCapture capture;
        WaterfallRowDiagnostics diagnostics;
        quint64 imdAcceptedOrdinal{0};
        // When the row was queued and when it is due on screen, on
        // remoteRowNowNs()'s clock. A row queued with no due time is due
        // when queued.
        qint64 queuedNs{0};
        qint64 dueNs{0};
        // Handed over before it was due (enqueueRemoteWaterfallRow with a
        // due time): forgotten with the presenter's items.
        bool handedEarly{false};
    };
    // An early frame row goes in about a row period before its trace is
    // accepted for IMD: the history row each one wrote, by its encoder
    // sequence, so the acceptance can tag it (attachImdOrdinalToEarlyRow).
    struct ImdEarlyHistoryRow {
        quint32 encoderSequence{0};
        quint64 rowSequence{0};
    };
    std::deque<ImdEarlyHistoryRow> m_imdEarlyHistoryRows;
    static constexpr int kMaxImdEarlyHistoryRows = 16;
    void attachImdOrdinalToEarlyRow(quint32 encoderSequence, quint64 acceptedOrdinal);
    QList<RemoteWaterfallRow> m_remoteRowQueue;
    quint64 m_remoteRowsDropped{0};
    quint64 m_remoteRowsPushed{0};
    QVector<float> m_lastRemoteRowPushed;
    bool m_waterfallTickerPausedForTest{false};
    // setWaterfallGlideForTest(): negative leaves the glide to the clock.
    float m_wfGlideRowsForTest{-1.0f};
    void onWaterfallTick();
    void drainRemoteWaterfallRows();
    // Put the queue's oldest rows into the waterfall, at most rows of them.
    void putRemoteWaterfallRows(int rows);
    // Drop the oldest rows past remoteRowQueueCap().
    void trimRemoteRowQueue();
    // The pump on a drawn frame (paintEvent) and on a ticker tick. A drawn
    // frame takes at most RemoteWaterfallRowClock::kMaxRowsPerFrame rows
    // less those the ticks put in since the last drawn frame. A tick
    // starts a budget of its own only when neither a frame nor a fresh
    // budget came in the last half row period: ticks queued up behind a
    // stall arrive back to back and share one, and a pan that is not
    // drawing has its ticks (a row period apart) keep its rows going in.
    void pumpRemoteWaterfallOnFrame();
    void pumpRemoteWaterfallOnTick();
    int m_remoteRowsByTicks{0};
    qint64 m_remoteBudgetStartNs{0};
    bool m_remoteBudgetStarted{false};
    // The remote row clock: advance it to now and put in the rows it says
    // are due, at most maxRows. Returns the rows put in. Run on every
    // frame the pan draws and on every ticker tick.
    int pumpRemoteWaterfallRows(int maxRows);
    // The steady clock the remote rows are queued and put in by, in ns.
    qint64 remoteRowNowNs() const;
    // A remote pan's waterfall scrolls by this clock, at the Core's row
    // period, on a grid locked to the rows' due times
    // (RemoteWaterfallRowClock.h).
    RemoteWaterfallRowClock m_remoteRowClock;
    QElapsedTimer m_remoteRowSteadyClock;
    // NEREUS_TRACE_REMOTE_ROWS=1, read once at construction: qCInfo lines
    // (lcSpectrum, "REMOTE_ROWS ...") for the row clock's starves,
    // re-anchors, two-row puts and paint gaps, at most
    // kRemoteRowTraceEventCap a window, and a summary each window while the
    // clock runs. Diagnostics only: nothing here changes a row.
    static constexpr qint64 kRemoteRowTraceWindowNs = 10'000'000'000LL;
    static constexpr int kRemoteRowTraceEventCap = 40;
    static constexpr qint64 kRemoteRowTracePaintGapNs = 25'000'000;
    struct RemoteRowTrace {
        qint64 nowNs{0};            // the pump's now, for the clock's observer
        qint64 windowStartNs{-1};
        int eventLines{0};
        int eventsSuppressed{0};
        int rowsPut{0};
        int starved{0};
        int reanchors{0};
        int twoRowPuts{0};
        int paintPumps{0};
        int tickPumps{0};
        int paintGaps{0};
        int minQueue{-1};
        int maxQueue{-1};
        QList<double> dueMinusNowMs;  // each row put in: its due time minus now
        qint64 lastPaintPumpNs{-1};
        qint64 lastPutNs{-1};
        qint64 starveStartNs{-1};
    };
    bool m_traceRemoteRows{false};
    RemoteRowTrace m_remoteRowTrace;
    void traceRemoteRowEvent(const QString& line);
    void traceRemoteRowWindow(qint64 nowNs);
    void onRemoteRowClockEvent(const RemoteWaterfallRowClock::Event& event);
    std::optional<qint64> m_remoteRowNowNsForTest;
    // A remote pan's row period on the Core's clock: framesPerLine frames
    // at the context's frame rate, in ns. 0 before the first context.
    qint64 remoteWaterfallRowPeriodNs() const;
    // The time between two waterfall rows: the Core's row period on a
    // remote pan, the pan's update period otherwise.
    double waterfallRowPeriodMs() const;
    // Run the waterfall ticker at waterfallRowPeriodMs().
    void applyWaterfallTickerPeriod();
    QVector<float> reprojectedToView(const QVector<float>& pixelsDbm,
                                     double centreHz, double spanHz) const;
    void refreshRemoteTraceProjection();
    // R-R3-21: the last trace the Core sent, as sent, and its window.
    QVector<float> m_remoteTraceCaptured;
    double m_remoteTraceCentreHz{0.0};
    double m_remoteTraceSpanHz{0.0};
    double m_remoteWideCentreHz{0.0};
    double m_remoteWideSpanHz{0.0};

    // ---- Phase 3G-8 commit 4: waterfall renderer state ----

    bool  m_wfAgcEnabled{true};
    // The Core's waterfall AGC levels (setCoreWaterfallLevelsAvailable).
    bool  m_coreWfLevelsAvailable{false};
    std::optional<std::pair<float, float>> m_coreWfLevels;
    struct WaterfallExtrasDiagnostics {
        quint32 endpointId{0};
        quint32 contextGeneration{0};
        quint32 encoderSequence{0};
    };
    std::optional<WaterfallExtrasDiagnostics> m_wfExtrasDiagnostics;
    QPointer<PanadapterStack> m_wfDiagnosticStack;
    QString m_wfDiagnosticPanId;
    int m_wfDiagnosticSliceId{-1};
    quint64 m_wfDiagnosticStreamEpoch{0};
    WaterfallRowDiagnostics m_pendingWfRowDiagnostics;
    QElapsedTimer m_wfDiagnosticLogClock;
    QString m_wfDiagnosticLastState;
    QString m_wfDiagnosticFirstChangedRow;
    quint64 m_wfDiagnosticCollapsedChanges{0};
    bool  m_clarityEnabled{true};    // Persistent requested choice for this pan.
    bool  m_clarityActive{false};     // Phase 3G-9c: suppresses legacy AGC when Clarity drives thresholds
    // Clarity's noise floor for this pan; NaN until setClarityFloorDbm().
    float m_clarityFloorDbm{std::numeric_limits<float>::quiet_NaN()};
    // Where the drawn floor is heading (setClarityFloorTargetDbm); NaN until
    // a floor arrives.
    float m_clarityFloorTargetDbm{std::numeric_limits<float>::quiet_NaN()};
    // NF-AGC: Task 2.8 — auto-track thresholds to noise floor + offset.
    bool  m_wfNfAgcEnabled{false};
    int   m_wfNfAgcOffsetTenthsDb{0}; // offset applied above/below noise floor, in 0.1 dB
    // Stop-on-TX: Task 2.8 — gate pushWaterfallRow() while TX is active.
    bool  m_wfStopOnTx{false};

    // From AetherSDR src/gui/SpectrumWidget.h:1771-1779 [@1e0718ad]
    // NereusSDR, based on AetherSDR: the ring (32 rows) and the decision
    // are WaterfallImpulseBlanker's, which holds each accepted row's median
    // in dB (rows here are dBm, AetherSDR's are intensity); the last
    // accepted row is kept in dBm, and the unused fill / interpolate mode
    // is left out.
    // NB Waterfall Blanker (#277)
    bool  m_wfBlankerEnabled{false};
    WaterfallImpulseBlanker m_wfBlanker;
    QVector<float> m_wfLastGoodRowDbm;
    // Rows NB Blank replaced (diagnostics; never reset).
    quint64 m_wfRowsBlanked{0};
    // NereusSDR design (follow-up wave F1): the threshold in dB, 1 to 20,
    // in place of AetherSDR's m_wfBlankerThreshold{1.15f} (an impulse
    // multiplier vs the rolling baseline of tile intensity).
    int   m_wfBlankerThresholdDb{6};
    // Replaces rowDbm with the last good row when it is a burst.
    void  blankWaterfallRow(QVector<float>& rowDbm);
    void  resetWaterfallBlanker();

    // From AetherSDR src/gui/SpectrumWidget.h:1426-1427 [@1e0718ad]
    int   m_freqGridSpacingHz{0};    // 0=Auto, or 1/2/5/10/25/50/100 kHz (#1390)
    // NereusSDR, based on AetherSDR: pixels and a default of 10, this
    // widget's scale text as it always was (AetherSDR: points, 8).
    int   m_freqScaleFontPx{10};     // freq-scale label size, 8..14 pt (#3501)
    int   m_freqScaleLabelsDrawn{0}; // test seam, see freqScaleLabelsDrawnForTest()
    int   m_wfOpacity{100};           // 0..100
    int   m_wfUpdatePeriodMs{30};     // NereusSDR default per §10 divergence
    bool  m_wfUseSpectrumMinMax{false};
    AverageMode m_wfAverageMode{AverageMode::None};

    TimestampPosition m_wfTimestampPos{TimestampPosition::None};
    TimestampMode     m_wfTimestampMode{TimestampMode::UTC};

    bool  m_showRxFilterOnWaterfall{false};
    bool  m_showTxFilterOnRxWaterfall{true};   // as loadSettings() defaults it
    bool  m_showRxZeroLineOnWaterfall{false};
    bool  m_showTxZeroLineOnWaterfall{false};

    // ---- 3DSS stacked-trace mode state ----
    SpectrumRenderMode m_spectrumRenderMode{SpectrumRenderMode::Mode2D};
    // 3DSS floor depth: how far below the measured noise floor the surface
    // baseline sits, in dB. Persisted per band (design doc section 6.3).
    int  m_dssFloorDepth{6};
    // colour gamma 0-100
    int  m_dssGain{70};   // 3DSS colour floor 0-100 (gamma of palette lookup)
    // [original inline comment from AetherSDR src/gui/SpectrumWidget.h:1485-1492 @1872028c]
    // 3DSS wedge close-in 0-100 (see setDssRowSpan). Defaults to 100 -- fully
    // ON -- by deliberate product decision, not by omission: three reviewers
    // read 0 as the safer default since it is the reference rendering. The
    // control is buried in the Display overlay's 3D VIEW section, so shipping
    // it off would mean most operators never discover the feature exists.
    // Anyone who wants the classic trapezoid has a labelled slider; anyone who
    // does not know to look gets the intended view. Do not flip this to 0
    // without also solving the discoverability side.
    int  m_dssRowSpan{100};
    //-KG4VCF [v0.5.3] NereusSDR-original: upstream renders at one fixed
    // viewing angle. 50 reproduces its geometry exactly.
    int  m_dssAngle{50};
    // 3D Speed (Task 24, NereusSDR-original): 0 == Match (automatic; see
    // effectiveDssRowDivider()), 1..10 == manual row divider.
    // m_dssFoldRow/m_dssFoldFullBins/m_dssFoldCount hold the peak-hold
    // fold in progress across the waterfall ticks between two pushed 3D
    // rows -- see accumulateDssRow() and design doc section 4.5.
    int  m_dssRowDivider{0};
    QVector<float> m_dssFoldRow;
    QVector<float> m_dssFoldFullBins; // Actual calibrated wide plane, not raw DDC bins.
    double m_dssFoldWideCenterMhz{0.0};
    double m_dssFoldWideBandwidthMhz{0.0};
    NereusSDR::RemoteSpectrumCapture m_dssFoldCapture;
    bool m_dssFoldRemote{false};
    bool m_dssFoldTx{false};
    int m_localSpectrumSource{-1};
    int m_dssFoldLocalSource{-1};
    int  m_dssFoldCount{0};
    double m_dssFoldCenterHz{0.0};
    double m_dssFoldBandwidthHz{0.0};
    double m_dssFoldDdcCenterHz{0.0};
    double m_dssFoldSampleRateHz{0.0};
    bool m_threeDSliceDepth{false};
    // [original inline comment from AetherSDR src/gui/SpectrumWidget.h:1474-1476 @1872028c]
    // GUI-thread only: pushRow() (updateSpectrum / updateKiwiSdrWaterfallRow) and
    // the renderGpuFrame/paint reads all run on the GUI thread, so m_dss needs no
    // lock. Do NOT call pushRow() from a worker/audio thread without adding one.
    DssRenderer m_dss;
    int  m_dssRowsPushed{0};
    bool m_txActiveForTest{false};
    // Continuous scroll progress toward the back between discrete DSS row
    // pushes (0 just after a push, ramping to 1 by the time the next one
    // lands), so the mesh glides instead of popping once per push. Advanced
    // from wall clock in the m_displayTimer tick, reset in pushDssRow().
    // NereusSDR-original: upstream drives the equivalent quantity from a
    // QElapsedTimer restarted per push burst (SpectrumWidget.cpp:6031-6064
    // [@1872028c]); NereusSDR's producer always advances by exactly one
    // row, so an accumulate/reset pair on the existing display timer needs
    // no new clock object.
    float  m_dssScrollProgressRows{0.0f};
    qint64 m_dssLastTickMs{0};   // wall clock of the previous display tick

    // 3DSS CPU fallback surface (Task 10). Consumed by BOTH the GPU
    // "mesh unavailable" path and the CPU paint path, so these stay outside
    // the NEREUS_GPU_SPECTRUM block below -- the QPainter path needs them
    // even when GPU spectrum rendering is disabled (-DNEREUS_GPU_SPECTRUM=OFF).
    // From AetherSDR SpectrumWidget.h:1510-1511 [@1872028c].
    static constexpr int kDssFallbackMaxW = 1024;   // 3DSS surface texture/image caps
    static constexpr int kDssFallbackMaxH = 512;

    // AGC rolling envelope (tracked across waterfall rows), in
    // core/spectrum/DisplayFollowers since iPhone app Task 20.
    WaterfallLevelFollower m_wfLevels;

    // Display pill plan Task 13 (setDisplayKeyed). The window is keyed, as
    // this pan was last told; the AGC run level saved for each band at a
    // key (receive context), in memory; the local engine's shape for the
    // hold length; and the clock seam.
    // From Thetis display.cs:2810-2829 [v2.10.3.15]: _waterfallAgcCache,
    // keyed by band and by receive or transmit context. NereusSDR, based on
    // Thetis: only the receive context is kept, because the transmit levels
    // here are the static pair and no follower runs on a transmit row.
    bool m_displayKeyed{false};
    QHash<int, float> m_agcRunLevelByBand;
    int    m_localFftSize{0};
    int    m_localFftDecimation{0};
    double m_localFftSampleRateHz{0.0};
    std::function<qint64()> m_displayClockForTest;
    qint64 displayNowMs() const;
    // setNoiseFloorFastAttack with the trigger's own minimum.
    void applyNoiseFloorFastAttack(bool on, qint64 minMs);

    // Rate-limit waterfall pushes per m_wfUpdatePeriodMs.
    qint64 m_wfLastPushMs{0};

    // 2026-05-25 KG4VCF bench fix: timer-driven waterfall row push.
    // Decouples row push cadence from FFT arrival cadence so network-
    // burst FFT delivery (multiple FFTs in 10 ms then nothing for
    // 50 ms) does not produce visible scroll stutter.  FFT arrivals
    // overwrite m_pendingWfPixelsDbm; the ticker fires at
    // m_wfUpdatePeriodMs and the consumer slot consumes the latest
    // cached value.
    //
    // The ticker lives on its own thread (m_waterfallTickerThread) so
    // any main-thread delay (focus event, layout pass, system
    // notification) cannot delay the tick firing -- only the
    // queued-slot delivery into the main thread's event queue.  This
    // is the "first-class waterfall" option-A fix from the 2026-05-25
    // bench session.
    QVector<float>     m_pendingWfPixelsDbm;
    bool               m_pendingWfPixelsDbmDirty{false};
    quint64            m_pendingWfImdAcceptedOrdinal{0};
    // Set on a key or un-key edge (setDisplayKeyed), cleared by the first
    // spectrum cached after it that was computed after the purge. While
    // set, the ticker adds no rows.
    bool               m_pendingWfPixelsStaleSinceKeyEdge{false};
    QThread*           m_waterfallTickerThread{nullptr};
    WaterfallTicker*   m_waterfallTicker{nullptr};

    // 1 Hz overlay repaint tick for the waterfall timestamp; started on
    // demand when the user selects a non-None timestamp position.
    QTimer* m_wfTimestampTicker{nullptr};
    // Starts the ticker while a timestamp is shown, stops it otherwise.
    void syncWfTimestampTicker();

    // ---- Phase 3G-8 commit 5: grid / scales renderer state ----

    bool  m_gridEnabled{true};
    bool  m_showZeroLine{false};
    bool  m_showFps{false};
    // ── Two grids, one live ─────────────────────────────────────────────
    //
    // From Thetis display.cs:1782-1804 + :1887-1905 [v2.10.3.15]:
    //     private static int tx_spectrum_grid_max =  20;
    //     private static int tx_spectrum_grid_min = -80;
    //     public static int SpectrumGridMaxMoxModified {
    //         get { return localMox(1) ? tx_spectrum_grid_max
    //                                  : spectrum_grid_max; }
    //     }
    // Receive and transmit each keep their own range, and the renderer picks
    // which to read. Nothing is saved and restored across the MOX edge.
    //
    // NereusSDR keeps m_refLevel / m_dynamicRange as the LIVE pair so the
    // ~60 render sites that read them stay untouched, and stores the two
    // inactive halves here. setMoxOverlay is the only swap point.
    //
    // This replaces a save-and-restore around the MOX edges, which was
    // fragile in a way this design cannot be: with one variable plus a
    // capture on un-key, ANY writer between the edges silently redefines
    // what the operator "chose". The receive noise-floor tracker did exactly
    // that on an ORION-class radio, whose receiver keeps running through
    // transmit, and the captured range came back on the next key-up with no
    // drawable labels. With two stores a receive-side tracker cannot reach
    // the transmit grid at all, whether or not anyone remembered to gate it.
    // Bench 2026-08-05, JJ KG4VCF: "we need a dynamic range for transmit and
    // a dynamic range for receive just like Thetis has".
    // The view window is stored per direction for the same reason the grid
    // is: re-imposing a default on every rise edge throws away whatever the
    // operator did last transmission. Bench 2026-08-05, zooming out during
    // transmit held for that cycle and snapped back on the next.
    //
    // Only the SPAN is really remembered for transmit -- the centre is
    // recomputed from the carrier on each key-up, since the carrier moves.
    double m_rxViewCenterHz{0.0};
    double m_rxViewBandwidthHz{0.0};
    double m_txViewCenterHz{0.0};
    double m_txViewBandwidthHz{8000.0};   // Thetis +/-4 kHz seed

    float m_rxRefLevel{-48.0f};
    float m_rxDynamicRange{68.0f};
    float m_txRefLevel{20.0f};      // Thetis tx_spectrum_grid_max
    float m_txDynamicRange{100.0f}; // tx_spectrum_grid_max - tx_spectrum_grid_min

    bool  m_dbmScaleVisible{true};  // right-edge dBm strip; false → spectrum fills full width
    bool  m_showCursorFreq{true};   // B8 Task 21: cursor frequency readout; default on
    FreqLabelAlign m_freqLabelAlign{FreqLabelAlign::Center};

    NereusSDR::BandPlanManager* m_bandPlanMgr{nullptr};   // non-owning
    int                          m_bandPlanFontSize{6};   // 0 = off; AetherSDR default

    QColor m_gridColor{255, 255, 255, 40};       // vertical freq grid
    QColor m_gridFineColor{255, 255, 255, 20};   // 1/5 step fine grid
    QColor m_hGridColor{255, 255, 255, 40};      // horizontal dBm grid
    // The approved native step stays this pan's picture choice; it is
    // loaded here rather than pushed by PanModels. Historical origin:
    // The model's default (PanadapterModel's kDefaultGridStep), drawn until
    // PanModels pushes the stored step.
    static constexpr int kDefaultDbGridStep = 10;
    // Existing Setup dB Step domain (DisplaySetupPages GridScalesPage).
    static constexpr int kMinDbGridStep = 1;
    static constexpr int kMaxDbGridStep = 40;
    int    m_dbGridStep{kDefaultDbGridStep};     // dB between horizontal grid lines
    QColor m_gridTextColor{255, 255, 0};         // yellow text default
    // Plan 4 D9c-1: split zero-line color into RX + TX.
    QColor m_rxZeroLineColor{255, 0, 0};         // red default (Thetis convention)
    QColor m_txZeroLineColor{255, 184, 0};       // amber default (NereusSDR-original)
    QColor m_bandEdgeColor{255, 0, 0};           // red default (Thetis)

    // Plan 4 D9c-4: TNF + SubRX forward-compat scaffolding.  No paint yet.
    QColor m_tnfFilterColor  {255,  80,  80,  80};   // red translucent placeholder
    QColor m_subRxFilterColor{180,   0, 220,  80};   // purple translucent placeholder

    // FPS overlay tracking
    int    m_fpsFrameCount{0};
    qint64 m_fpsLastUpdateMs{0};
    float  m_fpsDisplayValue{0.0f};

    // The row period the waterfall time scale was last marked for
    // (applyWaterfallTickerPeriod); 0 until the first.
    double m_overlayRowPeriodMs{0.0};

    // Toggle for the in-spectrum perf overlay (drawn in a corner showing
    // paint/gap/fft/overlay-rebuild timings + audio underruns + memory
    // pressure).  Persisted via AppSettings key "ShowPerfOverlay"; View
    // menu wires the setter.  Declared unconditionally (unlike the
    // renderGpuFrame-only perf fields further down, guarded by
    // NEREUS_GPU_SPECTRUM): showPerfOverlay()/setShowPerfOverlay() and the
    // constructor's m_perfPollTimer setup are ordinary shared code, so a
    // CPU-only build needs the same storage even though it has no GPU
    // overlay to actually draw the HUD into -- same reasoning as
    // dssMeshReady() above. This was a pre-existing CPU-build break
    // (predates the 3DSS port) caught while verifying -DNEREUS_GPU_SPECTRUM=OFF
    // for this fix wave.
    bool m_showPerfOverlay{false};
    std::function<QStringList()> m_corePerfLinesProvider;
    // 1 Hz timer that polls memory pressure + drives perf overlay refresh.
    // Owned by SpectrumWidget via Qt parent ownership.
    QTimer* m_perfPollTimer{nullptr};

    // ---- VFO / filter overlay ----
    double m_vfoHz{0.0};
    int    m_filterLowHz{-2850};    // LSB default — from Thetis
    int    m_filterHighHz{-150};
    int    m_stepHz{100};           // tuning step size

    int    m_panIndex{0};            // for per-pan settings keys

    // 3D Stacked-Trace Spectrum Plan Task 18: this widget's owned
    // DisplaySettingsModel (one per SpectrumWidget / panadapter),
    // constructed in the SpectrumWidget constructor and bound in
    // bindDisplaySettings(). m_displaySettingsApplyCount is a test seam;
    // see displaySettingsApplyCountForTest().
    DisplaySettingsModel* m_displaySettings{nullptr};
    int                   m_displaySettingsApplyCount{0};
    bool                  m_reflectingDisplaySettings{false};
    bool                  m_loadingTransmitDisplayScale{false};

    // ---- VFO flag widgets ----
    QMap<int, VfoWidget*> m_vfoWidgets;
    // Slices shown here only as their flag and their own edge marker
    // (setEdgeMarkedSlice).
    QSet<int> m_edgeMarkedSlices;

    // Which slice's flag stays on top -- see setFrontSliceIndex(). -1 = no
    // pin; updateVfoPositions() falls back to its own ascending-index order.
    int m_frontSliceIndex{-1};
    // True while updateVfoPositions() is placing the flags: the pass says
    // flagsRaised() once at its end, so the buttons each flag raises on the
    // way are not announced one by one.
    bool m_placingFlags{false};

    // Re-applies the m_frontSliceIndex pin. Called by setFrontSliceIndex()
    // for an immediate effect, and again at the end of every
    // updateVfoPositions() pass, because that loop unconditionally raises
    // every visible flag once per frame and would otherwise undo the pin on
    // the very next frame.
    void raiseFrontVfoWidget();

    // ---- CTUN mode ----
    bool   m_ctunEnabled{true};  // true = SmartSDR-style (pan independent of VFO)
    bool   m_ctunPreference{true};
    bool   m_ctunAvailable{true};
    QString m_ctunHeldReason;   // see applyRemoteCtunState()
    enum class VfoOffScreen { None, Left, Right };
    VfoOffScreen m_vfoOffScreen{VfoOffScreen::None};
    void drawOffScreenIndicator(QPainter& p, const QRect& specRect, const QRect& wfRect);
    // One edge arrow and its frequency label; row 0 is the pan's own VFO.
    void drawOffScreenArrow(QPainter& p, const QRect& specRect, bool left,
                            const QColor& arrowColor, double hz, int row);

    // ---- Mouse tracking overlay (QRhiWidget macOS workaround) ----
    QWidget* m_mouseOverlay{nullptr};

    // ---- Overlay menu ----
    SpectrumOverlayMenu* m_overlayMenu{nullptr};
    // Ref Level, Dyn Range and CTUN, read from the widget's own state
    // into the menu without the menu emitting (display pill plan Task 19).
    void seedOverlayMenu();

    // ---- Spot overlay state (Phase 3J-2 Task E1) ----
    // Backing store + per-frame click-rect / cluster vectors. Defaults
    // match AetherSDR src/gui/SpectrumWidget.h:634-651 [@0cd4559].
    QVector<SpotMarker>  m_spotMarkers;
    QPointer<SpotBeamTurner> m_spotBeamTurner;  // rotor control plan Task 8
    QVector<SpotHitRect> m_spotClickRects;
    QVector<SpotCluster> m_spotClusters;
    bool   m_showSpots{true};
    int    m_spotFontSize{16};
    int    m_spotMaxLevels{3};
    int    m_spotStartPct{50};       // % down from top of spectrum
    bool   m_spotOverrideColors{false};
    bool   m_spotOverrideBg{true};
    QColor m_spotColor{Qt::yellow};
    QColor m_spotBgColor{Qt::black};
    int    m_spotBgOpacity{48};

    // 2026-05-12 bench fix (Gap #6).  m_hoverSpotIndex: which spot
    // label the mouse is currently over (SpotMarker::index, -1 if
    // none).  Emitted via spotHoverIndexChanged so the Spot List can
    // highlight the matching row.  m_hoverSpotIndexExternal: opposite
    // direction — set by setHoverSpotIndexExternal() when the user
    // hovers a row in the Spot List, drives a halo around the label
    // in drawSpotMarkers.
    int    m_hoverSpotIndex{-1};
    int    m_hoverSpotIndexExternal{-1};

    // 2026-05-12 bench fix (Gap #7).  Per-source visibility mask for
    // the panadapter overlay.  Keys are SpotMarker::source strings
    // ("Cluster", "RBN", "WSJT-X", "SpotCollector", "POTA", "FreeDV",
    // "PSK", "Memory"); missing keys default to visible.  Used by
    // drawSpotMarkers to skip the per-marker draw when the source's
    // panadapter visibility toggle is off.  SpotHubDialog Display tab
    // drives this via setSpotSourceVisible.
    QHash<QString, bool> m_spotSourceVisible;

    // ---- TNF / notch overlay state (design section 8.1) ----
    // Main-thread only.  Both the paint path and the interaction layer run
    // there, so plain members rather than atomics; NotchModel is the
    // authoritative store and this is a render-side mirror of it.
    // Shape mirrors AetherSDR src/gui/SpectrumWidget.h:1608-1609 and
    // :1648 [@c6481cbf].
    QVector<NotchMarker> m_notchMarkers;
    // Master TNF enable, default OFF.  Matches NotchModel::globalEnabled()
    // (also false) and both upstreams: Thetis ships chkTNF unchecked, and
    // WDSP creates the notch database with master run 0
    // (third_party/wdsp/src/RXA.c:87).  A widget default of true would
    // paint every marker in the active colour for the one frame between
    // construction and the first MainWindow::refreshPanNotchMarkers push.
    bool   m_notchGlobalEnabled{false};
    // 100 Hz on this tree: nc = 4096 at a 48 kHz dsp rate through the
    // wintype-0 arm of min_notch_width (third_party/wdsp/src/nbp.c:88,
    // 1600.0 / (nc / 256) * (rate / 48000)).  Overwritten by
    // setNotchMinWidthHz once a channel is open.
    double m_notchMinWidthHz{100.0};

    // ---- Visual notch state (design section 8.3) ----
    // From Thetis display.cs:1070 [v2.10.3.15]: m_bShowVisualNotch = false.
    bool m_visualNotchEnabled{false};

    // Pristine mirror of m_renderedPixels, populated ONLY on the frames that
    // actually dent so the default-off path costs nothing. Thetis keeps a
    // permanent second array instead (current_display_data_copy, filled by
    // the memcpy at display.cs:5046-5049 and handed to the render loop at
    // :5055 [v2.10.3.15]) because its analyzer hands one over for free.
    // measurementPixels() falls back to m_renderedPixels when this is empty
    // or stale-sized.
    QVector<float> m_undentedPixels;

    // From Thetis display.cs:4778 [v2.10.3.15]: float fAttenuation = 100f;
    static constexpr float kNotchDentAttenuationDb = 100.0f;

    // From Thetis display.cs:8680 [v2.10.3.15]:
    //   dNewWidth += 20; // fudge factor to align better with spectrum notch
    static constexpr double kNotchDentFudgeHz = 20.0;

    // Written by the interaction layer (design section 7.4); drive the
    // Chartreuse highlight and the hover popup respectively.  Declared
    // here rather than in the interaction layer because notchColor() reads
    // both.
    int    m_selectedNotchId{-1};
    int    m_hoveredNotchId{-1};

    // ---- TNF / notch drag state (design section 7.2) ----
    // From Thetis console.cs:33284-33288 [v2.10.3.15], the drag-state
    // block.  m_notchGrab folds upstream's m_bDraggingNotch /
    // m_bDraggingNotchBW / m_BDragginNotchBWRightSide trio into one value;
    // m_notchDragStartX is _drag_notch_start_point.X (the press pixel, the
    // delta is worked in the move handler); m_notchDragStartData is
    // drag_notch_start_data, which carries the notch WIDTH for an edge
    // drag and the notch CENTRE for a whole-notch drag.  m_nNotchRX has no
    // analogue: the pan the drag started on is the widget receiving the
    // events.
    //NOTCH MW0LGE  [original section marker from console.cs:33283]
    NotchGrab m_notchGrab{NotchGrab::None};
    int       m_notchDragStartX{0};
    double    m_notchDragStartData{0.0};

    // ── QStaticText label cache ──────────────────────────────────────────
    // Pre-shaped (HarfBuzz-run-once) labels for the high-rate paint
    // loops in drawDbmScale and drawFreqScale.  Without this, each
    // paint reshapes every label string from scratch — AetherSDR
    // measured ~5% main-thread CPU on a single DSP curve widget from
    // shapeText, and our SpectrumWidget has 23+ drawText sites firing
    // at 20 fps.  Cache keys are the formatted label strings ("-100",
    // "14.230" etc.), values are QStaticText with AggressiveCaching.
    // Strings repeat heavily in steady state (dBm scale labels never
    // change between paints; freq labels cycle through a small set as
    // the user pans), so the cache hits >99% after the first paint.
    // mutable because drawDbm/Freq are non-const but the cache is hidden
    // state — keeping the helper signatures clean.  Lifetime: widget
    // lifetime; no invalidation needed at this font size since both
    // strips set a fixed pointSize per-paint.
    // Inspired by AetherSDR [@3503ae98] PR #2556 perf(gui): cache axis
    // labels as QStaticText.
    mutable QHash<QString, QStaticText> m_dbmLabelCache;
    mutable QHash<QString, QStaticText> m_freqLabelCache;

    // ---- Task 2.3: Spectrum text overlay state ----

    // (m_showMHzOnCursor retired in 2026-05 — see formatCursorFreq comment.)

    // From Thetis setup.cs:7061 [v2.10.3.13] lblDisplayBinWidth
    bool m_showBinWidth{false};

    // From Thetis display.cs:2304 [v2.10.3.13] m_bShowNoiseFloorDBM (default true in Thetis;
    // NereusSDR defaults off so the overlay is opt-in rather than on by default)
    bool            m_showNoiseFloor{false};
    OverlayPosition m_noiseFloorPosition{OverlayPosition::BottomLeft};

    // NF render colours — From Thetis display.cs:2316-2337 [v2.10.3.13]:
    //   private static Color noisefloor_color      = Color.Red;
    //   private static Color noisefloor_color_text = Color.Yellow;
    // Defaults match Thetis exactly so the line is visually distinct from
    // grid text (yellow) and from the spectrum trace.  Made configurable
    // via setNoiseFloorColor / setNoiseFloorTextColor when Setup wires them.
    // Default line + text colour — NereusSDR-tweaked from Thetis red/yellow
    // (display.cs:2316/2329 [v2.10.3.13]).  Thetis renders against a
    // configurable trace colour; NereusSDR's stock spectrum trace is cyan
    // (#00E5FF, see m_fillColor), and Thetis-default red dashes blend with
    // both cyan-trace fill and the brown band-plan strip beneath.  Default
    // tweak: bright magenta line + yellow text.  Both still match Thetis
    // semantics (warm/distinctive line, separate-colour text label) and
    // give the user clearly visible defaults that they can dial back to red
    // via the colour picker if they prefer Thetis-stock colours.
    QColor m_noiseFloorColor     {0xFF, 0x40, 0xFF};   // bright magenta
    QColor m_noiseFloorTextColor {Qt::yellow};
    // Fast-attack swap colour — From Thetis display.cs:5431-5432 [v2.10.3.13]:
    //   nf_colour      = bFast ? m_bDX2_Gray : m_bDX2_noisefloor;
    //   nf_colour_text = bFast ? m_bDX2_Gray : m_bDX2_noisefloor_text;
    // Lightened from Qt::gray (160,160,164) to #C8C8C8 so the swatch and
    // the rendered overlay both stand out against NereusSDR's dark UI
    // background; Thetis renders against a lighter-grey grid backdrop where
    // medium-grey reads fine.
    QColor m_noiseFloorFastColor {0xC8, 0xC8, 0xC8};
    // From Thetis display.cs:2310 [v2.10.3.13] m_fNoiseFloorLineWidth=1.0f.
    float  m_noiseFloorLineWidth {1.0f};

    // FFT-replan crossfade — when auto-zoom replans the FFT (or the user
    // moves the size slider), the avenger's history is cleared to avoid
    // cross-resolution ghosting.  Without smoothing, the first new frame
    // would snap into place ("trace drops in from sky").  We capture the
    // last good rendered pixels at the moment of replan and crossfade the
    // first kReplanFadeFrames frames of the new resolution against them so
    // the trace dissolves smoothly into the new layout.
    QVector<float> m_postReplanFrozenDb;
    int            m_postReplanFrameCount{0};
    static constexpr int kReplanFadeFrames = 8;

    // Per-frame + smoothed noise-floor estimates — source-first port of
    // Thetis display.cs:4633-4636 [v2.10.3.13]:
    //   m_fFFTBinAverageRX1 — current per-frame avg of bins below the
    //                         previous-frame estimate (linear-power blend)
    //   m_fLerpAverageRX1   — exponentially smoothed toward fftBinAverage
    //                         with framesInAttack-rate (display.cs:5901-5902)
    // Updated each spectrum frame in processNoiseFloor() after
    // updateSpectrumLinear finalises m_renderedPixels.  paintNoiseFloorOverlay
    // uses m_nfLerpAverage for line/box Y and m_nfFftBinAverage for actual Y.
    // The estimates, the attack time (Thetis display.cs:4638 [v2.10.3.13]
    // m_fAttackTimeInMSForRX1=2000), the sensitivity (display.cs:5775 +
    // 5783, _NFsensitivity = 3) and the fast-attack flag (display.cs:917-927
    // m_bFastAttackNoiseFloorRX1, set on band change / freq jump / MOX
    // transition, cleared per display.cs:5904-5908) live in
    // core/spectrum/DisplayFollowers since iPhone app Task 20.
    NoiseFloorFollower m_noiseFloor;

    // Operator-tunable NF shift — From Thetis display.cs:5763 [v2.10.3.13]:
    //   private static float _fNFshiftDBM = 0;
    // Clamped to [-12, +12] in the setter (Thetis clamps to 12 at
    // display.cs:5771).  Applied to both lerp and actual values per
    // display.cs:5400 + 5403.
    float m_nfShiftDbm{0.0f};

    // ---- NF-aware grid (Task 2.9) ----
    // From Thetis console.cs:46025-46085 [v2.10.3.13] GridMinFollowsNFRX1,
    // _RX1NFoffsetGridFollow, _maintainNFAdjustDeltaRX1.
    // abs() guard on fDelta: abs incase //MW0LGE [2.9.0.7] [original inline comment from console.cs:46081]
    // NereusSDR-original: applied as global default; RX1-scope dropped.
    bool m_adjustGridMinToNF{false};
    std::optional<bool> m_gridFollowSaveHold;
    int  m_nfOffsetGridFollow{0};    // dB offset added to NF estimate (default 0)
    bool m_maintainNFAdjustDelta{false};
    // Thetis tmrAutoAGC port (gridFollowTick): the display noise floor
    // Thetis exposes (m_fNoiseFloorRX1, lerp + NF shift) and whether it is
    // good (m_bNoiseFloorGoodRX1), the 500 ms timer, and when Clarity last
    // fed this pan (a pan Clarity feeds keeps Clarity's grid).
    float   m_gridFollowNfDbm{-200.0f};
    std::optional<float> m_lastAcceptedGridFollowNfDbm;
    bool    m_gridFollowNfGood{false};
    QTimer* m_gridFollowTimer{nullptr};
    qint64  m_lastClarityGridNfMs{0};
    static constexpr qint64 kClarityGridHoldMs = 1500;

    // From Thetis specHPSDR.cs:325 [v2.10.3.13] NormOneHzPan
    bool m_dispNormalize{false};

    // From Thetis console.cs:20073 peak_text_delay=500 [v2.10.3.13]
    // Upstream tags preserved: //MW0LGE (from cited console.cs:20070) [v2.10.3.15]
    // Color from console.cs:20278 Color.DodgerBlue [v2.10.3.13]
    bool            m_showPeakValueOverlay{false};
    OverlayPosition m_peakValuePosition{OverlayPosition::TopRight};
    int             m_peakTextDelayMs{500};
    // From Thetis console.cs:20278 [v2.10.3.13]: Color.DodgerBlue = #1E90FF
    QColor          m_peakValueColor{0x1E, 0x90, 0xFF};

    // Peak text overlay refresh timer — throttled by m_peakTextDelayMs.
    QTimer*         m_peakTextTimer{nullptr};
    // Current cached peak overlay text (refreshed by timer, rendered every frame).
    QString         m_peakTextCache;

    // drawTextOverlay helper — renders a text string at a corner position on
    // the spectrum rect, using the given colour.
    void drawTextOverlay(QPainter& p, const QRect& specRect,
                         OverlayPosition pos, const QString& text,
                         const QColor& color);

    // ---- Coalesced settings save ----
    void scheduleSettingsSave();
    // Drops a save that is waiting; the timer still fires and does nothing.
    void cancelScheduledSettingsSave();
    bool m_settingsSaveScheduled{false};
    quint64 m_settingsSaveGeneration{0};

    // Display pill plan Task 1: base name, the value a save stores, the
    // ship default the load falls back to (both as stored strings). See
    // visitPerPanSettings() in the .cpp.
    using PerPanSettingVisitor =
        std::function<void(const QString&, const QString&, const QString&)>;
    void visitPerPanSettings(const PerPanSettingVisitor& visit) const;
    using SharedAppearanceVisitor = std::function<void(const QString&, QColor SpectrumWidget::*, const QColor&)>;
    static void visitSharedAppearanceSettings(const SharedAppearanceVisitor& visit);
    void loadSharedAppearanceSettings(bool retirePanKeys);
    using SharedTxWfApply = std::function<void(SpectrumWidget&, const QString&)>;
    using SharedTxWfVisitor = std::function<void(const QString&, const QString&, const QString&, const SharedTxWfApply&)>;
    static void visitSharedTransmitWaterfallSettings(const SpectrumWidget* source, const SharedTxWfVisitor& visit);
    void loadSharedTransmitWaterfallSettings(bool retirePanKeys);
    void setSharedTransmitWaterfallValue(const QString& base, const QString& value);
    void setSharedAppearanceColor(const QString& base, const QColor& color, QColor SpectrumWidget::*member);
    void rebuildRxWfGradient();
    void rebuildTxWfGradient();
    void loadSharedWaterfallGradients(bool retirePanKeys);
    void applyTransmitDbmRange(float minDbm, float maxDbm, bool persist);
    static void visitPerPanSettings(const SpectrumWidget* source,
                                    const PerPanSettingVisitor& visit);
    void resetPanDisplayOverridesImpl(const QStringList& bases);
    void writePerPanSettingNow(const QString& base);

    // Display pill plan Task 2: emits displaySettingChanged(keyBase) unless
    // a load or a pan reset is running.
    void announceDisplaySetting(const QString& keyBase);
    void announceEffectiveWaterfallLevels();
    std::optional<std::pair<float, float>> m_announcedEffectiveWaterfallLevels;

    // 3D Stacked-Trace Spectrum Plan Task 18: wires m_displaySettings
    // bidirectionally (called once from the constructor).
    // syncDisplaySettingsFromWidget() is called from every write site of
    // the eight fields with no per-field widget signal (named setters,
    // setDbmRange(), loadSettings(), the dBm-strip and divider mouse
    // drags, wheelEvent) -- see SpectrumWidget.cpp for the full list.
    // Task 20 extended it to also push the six 3D fields (Task 24 makes
    // it seven), so loadSettings() (which assigns those directly, with no
    // signal) still seeds the model; at every other call site the seven
    // 3D pushes are redundant no-ops, since the dedicated widget-level
    // signal already reached the model first.
    void bindDisplaySettings();
    void syncDisplaySettingsFromWidget();
    void loadTransmitDisplayScale(bool retirePanKeys);

    // Recompute m_spectrumAverageAlpha + m_waterfallAverageAlpha from the
    // current per-side time constants and live FPS using the Thetis formula:
    //   α = exp(-1 / (fps × τ_seconds))
    // From Thetis specHPSDR.cs:351-380 [v2.10.3.13] AvTau / AvTauWF setters.
    void recomputeAverageAlphas();
    // Frames per second the per-frame overlays (active peak hold, peak
    // blobs, noise floor) advance at: a remote pan's frame rate from the
    // Core, else this window's display timer (parity Task 17).
    int overlayFrameRate() const;

    // ---- Mouse state ----
    bool   m_draggingDbm{false};
    int    m_dragStartY{0};
    float  m_dragStartRef{0.0f};
    /// Right-drag on the dBm strip: move the TOP only, leaving the floor
    /// where it is, so the range stretches instead of sliding.
    ///
    /// From Thetis PanDisplay.cs [v2.10.3.15]: left-press on
    /// dBmScalePanadapterRegion sets gridminmaxadjust (:4105-4115, max AND
    /// min move together), right-press sets gridmaxadjust (:4424-4431, max
    /// only). Two modes on the same strip, chosen by button. We had only
    /// the pan, with range relegated to the scroll wheel.
    /// True when `pos` is on the dBm strip: right of the strip edge and
    /// above the spectrum/waterfall divider. Shared by the press handler and
    /// contextMenuEvent so the two cannot disagree about where the strip is.
    bool isOnDbmStrip(const QPoint& pos) const;

    /// Emit txViewWindowChanged when transmitting and the window actually
    /// moved. Called from every site that writes m_centerHz / m_bandwidthHz
    /// so no zoom path can silently leave the analyzer behind.
    /// THE only place m_centerHz / m_bandwidthHz may be written.
    ///
    /// The transmit display has two things that must agree: the window on
    /// screen, and the span the WDSP analyzer is clipped to. Thetis gets
    /// that for free because initAnalyzer derives both from one zoom state.
    /// Ours are separate objects, so agreement depends on every writer
    /// remembering to announce itself -- and on 2026-08-05 six separate
    /// defects came from writers that did not: a stuck tone, a stretched
    /// axis, a jittering passband.
    ///
    /// Routing every write through here makes the announcement structural
    /// rather than remembered. A new zoom or pan path added later cannot
    /// silently desync the analyzer, because it cannot change the window
    /// without going through this.
    ///
    /// Pass the current value for whichever of the two is not changing.
    void applyViewWindow(double centreHz, double bandwidthHz);

    void notifyTxViewWindow();

    /// THE only place frequencyClicked may be emitted.
    ///
    /// Withheld while this pan is showing transmit, for the reason the
    /// release branch already gave: panning and zooming during transmit are
    /// new on this branch, so a pointer gesture over the pan can now move a
    /// keyed transmitter into an antenna and an amplifier.
    ///
    /// That gate went on the short-pan-release branch alone, and it was not
    /// the only way in. A press inside the passband sets m_draggingVfo and
    /// mouseMoveEvent tunes on every move; a spot label tunes on press; the
    /// wheel tunes on every notch. Codex found all three on PR #317, and the
    /// lesson is the same one applyViewWindow already learned on this branch:
    /// a rule enforced at one of several call sites is a rule that holds
    /// until someone adds the next call site. Routed rather than repeated.
    ///
    /// Menu actions go through it too. The hazard is the radio moving while
    /// keyed, which does not care whether the operator meant it.
    void requestTune(double hz);

    bool   m_draggingDbmRange{false};
    float  m_dragStartFloor{0.0f};

    // 3D stacked-trace spectrum plan, Task 16: the dBm-strip drag-pan
    // gesture targets 3D Floor instead of Ref Level while in 3D mode (see
    // the mode branch in mouseMoveEvent). dssFloorDepth() is captured here
    // at press time, the same way m_dragStartRef captures m_refLevel above,
    // so the 3D branch has its own drag-start baseline to offset from.
    // NereusSDR-original: upstream AetherSDR's 3D Floor has no drag
    // binding at all (SpectrumWidget.cpp's m_draggingDbm only ever moves
    // its Ref Level equivalent), so there is no upstream field to port.
    int    m_dragStartDssFloorDepth{0};

    // Task 19: Ctrl-drag (Cmd/Meta too) on the dBm strip -- zooms the span
    // with the bottom pinned, instead of panning the plain drag above does.
    // A separate flag + capture set from m_draggingDbm's, even though the
    // two gestures are mutually exclusive at press time (arrowRow/Ctrl/body
    // hit-tests are checked in order and each `return`s), because they
    // capture different quantities: m_dragStartRef is the pre-drag TOP,
    // these are the pre-drag RANGE and BOTTOM. Named m_dbmRangeDragStartY
    // rather than reusing m_dragStartY for the same reason -- one drag
    // reads "Y since press" against a Ref baseline, the other against a
    // Range baseline, and giving them the same storage would make a stale
    // read silently plausible after a copy/paste edit.
    //
    // Mirrors upstream's m_dbmDragStartY / m_dbmDragStartRange /
    // m_dbmDragStartBottom (AetherSDR SpectrumWidget.h, used by
    // SpectrumWidget.cpp:9537-9550, 10493-10501 [@1872028c]). Upstream also
    // keeps m_dbmDragStartRef for its release-handler transition math
    // (oldMinDbm/oldMaxDbm for beginDbmRangeTransition); NereusSDR's release
    // handler has no transition system to feed (see the .cpp mouseRelease
    // change), so that field has no NereusSDR counterpart.
    bool   m_draggingDbmRangeZoom{false};
    int    m_dbmRangeDragStartY{0};
    float  m_dbmRangeDragStartRange{0.0f};
    float  m_dbmRangeDragStartBottom{0.0f};
    QPoint m_mousePos;              // for cursor frequency display
    bool   m_mouseInWidget{false};

    // Filter edge drag — from AetherSDR SpectrumWidget.h:429-432
    enum class FilterEdge { None, Low, High };
    FilterEdge m_draggingFilter{FilterEdge::None};
    int  m_filterDragStartX{0};     // pixel X at grab time
    int  m_filterDragStartHz{0};    // filter edge Hz at grab time
    // The grabbed receiver's flag (-1: the pan's own filter, no flag) and
    // its filter as the drag has set it (2026-10-21). A flagged receiver's
    // filter is set through its flag, never through this pan's
    // m_filterLowHz/m_filterHighHz and filterEdgeDragged, which belong to
    // the pan's own slice.
    int  m_filterDragSliceIndex{-1};
    int  m_filterDragLowHz{0};
    int  m_filterDragHighHz{0};

    // Passband center drag (slide-to-tune) — AetherSDR:434
    bool m_draggingVfo{false};

    // Receiver grab (2026-10-18). Which part of which receiver a point is
    // over, for the press and the hover cursor alike: the selected
    // receiver first, then the others topmost first. An edge of a receiver
    // that is not selected tunes it like its body does.
    enum class ReceiverGrabPart { None, LowEdge, HighEdge, Tune };
    struct ReceiverGrab {
        ReceiverGrabPart     part{ReceiverGrabPart::None};
        SliceMarkerGeometry  marker;
    };
    ReceiverGrab receiverGrabAt(int mx, int my, const QRect& specRect) const;
    // From AetherSDR src/gui/SpectrumWidget.cpp:360 [@1e0718ad]
    static constexpr int kReceiverEdgeGrabPx = 8;
    // From AetherSDR src/gui/SpectrumWidget.cpp:361 [@1e0718ad]
    static constexpr int kReceiverMinBodyPx = 6;
    // Half width of the slide-to-tune strip around a receiver's center line.
    // From AetherSDR src/gui/SpectrumWidget.cpp:9952 [@1e0718ad]
    static constexpr int kReceiverCenterGrabPx = 8;

    // One drag-to-tune, from the spectrum or from a flag. The receiver's
    // frequency and the press column are kept, so the receiver moves by the
    // cursor's pixel travel at the current Hz per pixel and never jumps to
    // the cursor, nor by a view change mid-drag (2026-10-21).
    // A receiver with a flag, the selected one included, is tuned through
    // its own flag (VfoWidget::tuneFromDrag), the path the flag's wheel
    // uses, so the drag moves the receiver grabbed and not whichever slice
    // the pan's frequencyClicked resolves to (2026-10-21).
    struct ReceiverDrag {
        int    sliceIndex{-1};   // -1: the pan's own VFO, no flag
        bool   viaFlag{false};
        double pressHz{0.0};     // the receiver's frequency at the press
        int    pressX{0};
        int    stepHz{1};
        bool   moved{false};     // a move has left the press column
    };
    ReceiverDrag m_receiverDrag;
    bool m_flagDragActive{false};
    void beginReceiverDrag(int sliceIndex, bool viaFlag, double centerHz, int pressX);
    void moveReceiverDrag(int mx, const QRect& specRect);

    // Wheel and trackpad steps for every wheel branch (2026-10-18).
    WheelStepAccumulator m_wheelSteps;

    // Divider drag (spectrum/waterfall split) — AetherSDR:419
    bool m_draggingDivider{false};

    // Pan drag (waterfall/spectrum drag to change center) — AetherSDR:425-427
    bool   m_draggingPan{false};
    int    m_panDragStartX{0};
    double m_panDragStartCenter{0.0};

    // Bandwidth drag (frequency scale bar) — AetherSDR:421-423
    bool   m_draggingBandwidth{false};
    bool   m_wheelZoomRecentring{false};
    int    m_bwDragStartX{0};
    double m_bwDragStartBw{0.0};

    // (kFilterGrab, the 5 px edge zone from AetherSDR line 1087, gave way
    // to kReceiverEdgeGrabPx and kReceiverMinBodyPx above on 2026-10-18.)

    // ---- HIGH SWR / PA safety overlay state ----
    // Ported from Thetis display.cs:4183-4201 [v2.10.3.13]
    bool m_highSwrActive{false};
    bool m_highSwrFoldback{false};

    // ---- MOX / TX overlay state (H.1, Phase 3M-1a) ----
    // From Thetis display.cs:1568 [v2.10.3.13]: static bool _mox = false;
    bool  m_moxOverlay{false};

    // 3M-5d follow-up: TX-side bin-frequency context for visibleBinRange().
    // Set by MainWindow on MOX rise (and on VFO change during TX) so the
    // TX FFT pixel array can be sliced against the TX channel center
    // + TX sample rate instead of the RX DDC rate + center.  Without
    // these, the same WDSP bin array gets sliced as if it covered the
    // RX DDC bandwidth, which on HL2 is 8x wider than the TX bandwidth
    // -- producing a stretched display that aliases TX-baseband edges
    // onto the visible window (bench 2026-05-10).
    double m_txCenterHz{0.0};
    double m_txSampleRateHz{96000.0};
    // TX attenuator cal offset — applied as an additional dBm shift during TX.
    // From Thetis display.cs:4840 [v2.10.3.13]: if (!local_mox) fOffset += rx1_preamp_offset;
    // Upstream tags preserved: //MW0LGE (from cited upstream lines) [v2.10.3.15]
    float m_txAttOffsetDb{0.0f};
    // TX filter visibility in spectrum panel.
    // From Thetis display.cs:2481 [v2.10.3.13]: DrawTXFilter flag.
    bool  m_txFilterVisible{false};

    // ---- Two-tone IMD overlay state (Phase 3M-4 Task 12) ----
    // From Thetis display.cs:5008 [v2.10.3.13] show condition. Owns an
    // ImdOverlay analytical core (peak detection + EMA + readout) that
    // analyzes accepted display planes once; painting reads its cached frame.
    bool m_testingIMD{false};            // Display.TestingIMD mirror
    bool m_showIMDMeasurements{false};   // Display.ShowIMDMeasurments mirror
    // Parity Task 31 (A11): display duplex, false by default as Thetis's
    // _display_duplex (console.cs:15390-15395 [v2.10.3.15]). With it on
    // while keyed the pan keeps the receiver (showsTransmitView() false).
    bool m_displayDuplex{false};
    // Whether setMoxOverlay loaded the transmit span (the rise with DUP
    // off, or DUP turned off while keyed), so the fall or DUP turned on
    // gives the receive span back.
    bool m_txSpanLoaded{false};
    // Thetis tx_display_cal_offset (display.cs:1413 [v2.10.3.15]).
    float m_txDisplayCalOffsetDb{0.0f};
    // Thetis rx1_preamp_offset (RadioModel::rxPreampOffsetDb).
    float m_rxPreampOffsetDb{0.0f};
    // The presenter owns the legacy helper; this alias retains existing
    // reset/format semantics without allocating another EMA state.
    ImdMeasurementPresenter m_imdPresenter;
    ImdOverlay* m_imdOverlay{nullptr}; // Non-owning alias of the presenter's legacy helper.
    quint64 m_imdContextRevision{0};
    ImdWaterfallAnnotations m_imdWaterfallAnnotations;
    struct PendingImdCorrectionEdge {
        bool correctionsApplied{false};
        quint64 contextRevision{0};
        quint64 acceptedOrdinalAtObservation{0};
    };
    std::optional<PendingImdCorrectionEdge> m_imdPendingCorrectionEdge;
    qreal m_imdWaterfallGlideRows{0.0};
    quint64 m_imdAcceptedFrameOrdinal{0}; // GUI-local, independent of encoder wrap
    quint64 m_imdLocalFrameSequence{0};
    int m_imdLocalSource{-1};
    quint64 m_imdRemoteFrameSequence{0};
    bool m_hasImdRemoteFrame{false};
    int m_imdPixelCount{0};
    bool m_imdInspectorVisible{false};
    bool m_imdGuidesVisible{true};
    ImdProduct m_imdSelectedProduct{ImdProduct::ThirdLower};
    friend class ImdSpectrumTestAccess;
    friend class ImdWaterfallTestAccess;

    // ---- TX filter overlay range + mode (Plan 4 D9, Cluster E) ----
    // Audio-Hz edges of the TX passband; updated by setTxFilterRange().
    // IQ-space conversion (per deskhpsdr/transmitter.c:2136-2186 [@120188f])
    // is applied at draw time using m_txMode.
    int     m_txFilterLow{100};   // default matches TransmitModel::m_filterLow
    int     m_txFilterHigh{2900}; // default matches TransmitModel::m_filterHigh
    DSPMode m_txMode{DSPMode::USB};
    // Signed Hz offset added to m_vfoHz for the TX overlay position so the
    // orange band tracks XIT shifts (xitEnabled ? xitHz : 0).  Updated via
    // setTxVfoOffsetHz from MainWindow on SliceModel xit signals.
    int     m_txVfoOffsetHz{0};
    QColor  m_txFilterColor{255, 120, 60, 46}; // matches kTxFilterOverlayFill default
    // Plan 4 D9b (Cluster F): user-pickable RX filter overlay color.
    // Default matches Style::kRxFilterOverlayFill = "rgba(0, 180, 216, 80)".
    QColor  m_rxFilterColor{0x00, 0xb4, 0xd8, 80};

#ifdef NEREUS_GPU_SPECTRUM
    bool m_rhiInitialized{false};

    // GPU pipeline init helpers
    void initWaterfallPipeline();
    void initOverlayPipeline();
    void initSpectrumPipeline();
    void renderGpuFrame(QRhiCommandBuffer* cb);

    // ---- Waterfall GPU resources ----
    QRhiGraphicsPipeline*       m_wfPipeline{nullptr};
    QRhiShaderResourceBindings* m_wfSrb{nullptr};
    QRhiBuffer*                 m_wfVbo{nullptr};
    QRhiBuffer*                 m_wfUbo{nullptr};
    QRhiTexture*                m_wfGpuTex{nullptr};
    QRhiSampler*                m_wfSampler{nullptr};
    int  m_wfGpuTexW{0};
    int  m_wfGpuTexH{0};
    bool m_wfTexFullUpload{true};
    int  m_wfLastUploadedRow{-1};
    // Rows pushed since the last GPU frame took the texture's rows, counted
    // up to the ring's height: at that many the whole ring is sent.
    int  m_wfRowsSinceUpload{0};

    // ---- Overlay GPU resources ----
    QRhiGraphicsPipeline*       m_ovPipeline{nullptr};
    QRhiShaderResourceBindings* m_ovSrb{nullptr};
    QRhiBuffer*                 m_ovVbo{nullptr};
    QRhiTexture*                m_ovGpuTex{nullptr};
    QRhiSampler*                m_ovSampler{nullptr};
    QImage m_overlayStatic;
    bool   m_readableTextCache{false};
    bool   m_overlayStaticDirty{true};
    bool   m_overlayNeedsUpload{true};

    // I2 fix (final review of the 3D stacked-trace spectrum port): dBm
    // value of dssFloorDbm() as of the last time the 3D dBm-scale strip's
    // label set was actually baked into m_overlayStatic. Compared each
    // frame (updateDssScaleOverlayFreshness(), called from
    // updateSpectrumLinear right after processNoiseFloor()) against the
    // live dssFloorDbm() -- itself re-lerped every frame -- so the overlay
    // is only marked dirty when the ROUNDED LABEL SET would actually
    // change, not on every sub-pixel lerp step. std::nullopt means "never
    // baked yet"; harmless, since m_overlayStaticDirty already starts
    // true above and bakes the first real frame regardless of this check.
    std::optional<float> m_dssLastBakedFloorDbm;
    // I2 fix: the freshness check itself -- see m_dssLastBakedFloorDbm.
    void updateDssScaleOverlayFreshness();

    // 2026-05-26 KG4VCF dual-layer overlay split.
    //
    // The static texture above carries chrome (grid, scales, bandplan,
    // VFO marker, spot markers, freq/time scale, waterfall chrome,
    // perf overlay, etc.) that only changes on operator interaction.
    // The dynamic texture below carries the per-frame features --
    // peak hold trace, peak blobs, noise-floor line + text -- so
    // those can animate at display rate without forcing a full chrome
    // repaint each tick.
    //
    // Both textures alpha-composite onto the spectrum trace.  Same
    // pipeline + sampler + UBO + VBO as the static layer -- only the
    // SRB and texture differ.  The dynamic image is the SAME size as
    // the static one (full window) for code simplicity; in practice
    // it stays mostly transparent except for the spectrum-area
    // overlays, so the GPU sampler reads through to spectrum
    // un-tinted for the rest of the widget.
    QRhiShaderResourceBindings* m_ovDynSrb{nullptr};
    QRhiTexture*                m_ovDynGpuTex{nullptr};
    QImage m_overlayDynamic;
    bool   m_overlayDynamicDirty{true};
    bool   m_overlayDynamicNeedsUpload{true};
    // Rows of m_ovDynGpuTex that may still hold an earlier frame's
    // pixels; renderGpuFrame() clears and uploads them with the band.
    DynamicOverlayRows m_overlayDynamicRows;

    // 2026-05-25 perf fix: timestamp of the last per-frame "dynamic
    // overlay" force-dirty in updateSpectrumLinear.  Rate-limits the
    // overlay rebuild for Active Peak Hold / Peak Blobs / Noise Floor
    // (all features that previously rebuilt the FULL overlay on every
    // 30 Hz spectrum frame, defeating the cache).  See the rationale
    // at SpectrumWidget.cpp around the "perf fix" comment block.
    qint64 m_overlayDynamicDirtyMs{0};

    // 2026-05-26 KG4VCF perf instrumentation: wall-clock timestamp
    // of the previous renderGpuFrame entry.  Used to feed
    // PerfMonitor::recordInterFrameGap(now - m_lastPaintWallMs) so the
    // perf overlay can show if paint events are arriving on time.
    // A gap > the display period == main thread was blocked.
    qint64 m_lastPaintWallMs{0};

    // ---- FFT spectrum GPU resources ----
    QRhiGraphicsPipeline*       m_fftLinePipeline{nullptr};
    QRhiGraphicsPipeline*       m_fftFillPipeline{nullptr};
    QRhiShaderResourceBindings* m_fftSrb{nullptr};
    QRhiBuffer*                 m_fftLineVbo{nullptr};
    QRhiBuffer*                 m_fftFillVbo{nullptr};
    // Phase 3G-8 commit 10: peak hold VBO — same layout as line VBO,
    // generated only when peak hold is enabled. m_peakHoldHasData is
    // false between peak decay resets so we skip the draw call.
    QRhiBuffer*                 m_fftPeakVbo{nullptr};
    bool                        m_peakHoldHasData{false};
    // Whether m_fftFillVbo holds the fill for the trace m_fftLineVbo holds.
    // The fill is written only while pan fill is on, so turning it on draws
    // no fill left over from an earlier trace.
    bool                        m_fftFillHasData{false};
    // From AetherSDR: kMaxFftBins = 8192, kFftVertStride = 6
    static constexpr int kMaxFftBins = 65536;
    static constexpr int kFftVertStride = 6;  // x, y, r, g, b, a
    // The trace line's vertices and, when asked, the fill's, for the first
    // n display pixels, with the dBm floor at yFloor (fftTraceFloorNdc() of
    // the spectrum area). renderGpuFrame() uploads what this builds.
    void buildFftTraceVertices(int n, float yFloor, bool buildFill,
                               QVector<float>& lineVerts,
                               QVector<float>& fillVerts) const;
    // The peak hold line's vertices for the first n display pixels, on the
    // trace's floor (yFloor) and shift. renderGpuFrame() uploads what this
    // builds.
    void buildFftPeakVertices(int n, float yFloor,
                              QVector<float>& peakVerts) const;
    // The trace's dBm floor in the viewport's device coordinates (the top
    // of the band plan strip, as dbmToYf) and the dB added to every value
    // (calibration and 1 Hz normalization, as dbmToYf).
    // From Longpath src/gui/SpectrumWidget.cpp:11648-11665 [@383f058]
    float fftTraceFloorNdc(const QRect& specRect) const;
    float fftTraceShiftDb() const;

    // ---- Spectrum background layer (Display pill plan Task 5) ----
    // A texture and SRB of its own for the same overlay pipeline, drawn in
    // the spectrum viewport before the trace. Created the first time the
    // background is on, so a pan that never turns it on never has them.
    // From AetherSDR src/gui/SpectrumWidget.h:1956-1960 [@1e0718ad]
    QRhiTexture*                m_bgGpuTex{nullptr};
    QRhiShaderResourceBindings* m_bgSrb{nullptr};
    QImage m_bgLayer;
    bool   m_bgLayerDirty{true};
    // Brings the layer's texture up to date; false when it cannot be drawn.
    bool prepareBackgroundLayer(QRhiResourceUpdateBatch* batch, const QRect& specRect);

    // ---- 3DSS mesh GPU resources ----
    bool initDssMeshPipeline();
    void rebuildDssMeshIfNeeded(QRhiResourceUpdateBatch* batch);
    QVector<int> dssHeightRowsToUpload() const;
    void uploadDssHeightRows(QRhiResourceUpdateBatch* batch);
    void uploadDssPaletteLut(QRhiResourceUpdateBatch* batch);
    void writeDssMeshUbo(QRhiResourceUpdateBatch* batch,
                         const QRect& specRect, float dpr);

    QRhiGraphicsPipeline*       m_dssFillPipeline{nullptr};
    QRhiGraphicsPipeline*       m_dssLinePipeline{nullptr};
    QRhiShaderResourceBindings* m_dssSrb{nullptr};
    QRhiBuffer*                 m_dssMeshVbo{nullptr};
    QRhiBuffer*                 m_dssMeshLineVbo{nullptr};
    QRhiBuffer*                 m_dssUbo{nullptr};
    QRhiTexture*                m_dssHeightTex{nullptr};
    QRhiTexture*                m_dssPaletteTex{nullptr};
    QRhiSampler*                m_dssHeightSampler{nullptr};
    QRhiSampler*                m_dssPaletteSampler{nullptr};
    bool    m_dssMeshReady{false};
    // False until rebuildDssMeshIfNeeded() has actually pushed vertices
    // for m_dssMeshCols. Guards the first-upload case that a plain dirty
    // flag misses, and is reset on teardown so a rebuilt pipeline uploads
    // again.
    bool    m_dssMeshUploaded{false};
    int     m_dssMeshCols{0};
    quint64 m_dssLutToken{~0ull};
    quint64 m_dssUploadedRowGeneration{~0ull};
    int     m_dssLastUploadedHead{-1};
    // Linux takes Qt's default QRhiWidget backend (this file's setApi() only
    // covers Q_OS_MAC/Q_OS_WIN), which is typically OpenGL -- so this cannot
    // default to DedicatedRibbonPipeline and stay correct there; it is
    // overwritten from the real backend at the top of initDssMeshPipeline()
    // before anything reads it.
    DssOutlinePipelineMode m_dssOutlinePipelineMode{
        DssOutlinePipelineMode::DedicatedRibbonPipeline};

    // ---- 3DSS CPU-fallback quad (Task 10: RGBA16F unsupported) ----
    // Composited through the SAME overlay pipeline (m_ovPipeline/m_ovVbo/
    // m_ovSampler) the static/dynamic chrome layers already use -- "blit it
    // through the existing overlay pipeline" per the task brief -- with its
    // own texture + SRB sized to the spectrum viewport (capped at
    // kDssFallbackMaxW/H) rather than the full window. Deliberately NOT
    // named m_dssSrb/m_dssGpuTex: those upstream names belong to the GPU
    // MESH resources above (m_dssFillPipeline/m_dssSrb/...), claimed by
    // Task 7 before this task's cached-image fallback existed to need its
    // own pair -- see docs/attribution/aethersdr-reconciliation.md, Task 7
    // row, "Task 10 must pick different names for the fallback quad's own
    // SRB/texture". Upstream itself keeps these two concepts separate too
    // (m_dssMeshFillPipeline/m_dssMeshSrb for the mesh vs. m_dssGpuTex/
    // m_dssSrb for its own cached-image fallback); NereusSDR's mesh
    // resources just happen to have already claimed the shorter names.
    void uploadDssFallbackImage(QRhiResourceUpdateBatch* batch,
                                const QRect& specRect, float dpr);
    QRhiTexture*                m_dssFallbackTex{nullptr};
    QRhiShaderResourceBindings* m_dssFallbackSrb{nullptr};
    int     m_dssFallbackTexW{0};
    int     m_dssFallbackTexH{0};
    quint64 m_dssFallbackUploadedGen{~0ull};

#endif

    // Also reset on MOX/context changes in the shared path. The CPU painter
    // uses m_renderedPixels for its trace; this count remains zero there.
    int m_visibleBinCount{0};  // GPU bins rendered this frame (draw call count)
    // The line width, in device pixels, the trace geometry in m_fftLineVbo
    // was built for: it decides that pass's pipeline and vertex count
    // (SpectrumTraceRibbon.h).
    float m_traceRibbonWidthPx{1.0f};

    // Whether the GPU 3DSS mesh pipeline is up and has data this frame.
    // Deliberately declared OUTSIDE the NEREUS_GPU_SPECTRUM block above
    // (unlike the GPU-only resources and helpers it reads) because callers
    // such as the 3D VIEW overlay menu's "3D Span" control gate
    // (dssRowSpanSupported(), DssMeshGeometry.h) need to ask this question
    // in both build configurations. A CPU-only build has no GPU mesh at
    // all, so the answer there is always false. The accessor,
    // dssMeshReady(), is public (above, with dssMeshReadyChanged()); every
    // write of the flag goes through this setter so the signal cannot be
    // missed.
#ifdef NEREUS_GPU_SPECTRUM
    void setDssMeshReady(bool ready) {
        if (m_dssMeshReady == ready) { return; }
        m_dssMeshReady = ready;
        emit dssMeshReadyChanged(ready);
    }
#endif

    QRegion m_receiverFlagRegion;
    void refreshReceiverFlagRegion();

    // Invalidate the GPU-path cached overlay texture so grid, labels,
    // dBm scale, waterfall filter/zero-line/timestamp overlays, and
    // other QPainter-drawn chrome re-render on next frame. Safe no-op
    // when the GPU path is disabled.
    void markOverlayDirty() {
#ifdef NEREUS_GPU_SPECTRUM
        m_overlayStaticDirty = true;
#endif
        update();
    }
};

} // namespace NereusSDR
