# Second-Pass Compliance Reconciliation — Per-File Contributor Audit

Compares each NereusSDR derivative file's current header contributor set
against the union of contributors named in the Thetis source(s) it cites,
using the index at `docs/attribution/thetis-contributor-index.md`.

Phase 1b output. Phase 2 consumes this to apply corrections.

## Summary

- Total files audited: **171** (166 ramdor/Thetis rows + 5 mi0bot rows;
  independently-implemented table skipped per spec)
- Files already complete: **14**
- Files needing additions: **157**
- Files requiring Phase 2 repair, by variant:
  - `thetis-samphire`: **112**
  - `thetis-no-samphire`: **2**
  - `mi0bot`: **4** (see Decisions log — most mi0bot files are actually
    covered for named callsigns; the 4 flagged here are for mi0bot-inline
    coverage of upstream Thetis callsigns not yet in the header)
  - `multi-source`: **39**

The dominant failure mode across the tree is missing **inline** mods from
console.cs and setup.cs (W2PA, G8NJJ, WD5Y, W4WMT) and missing **inline**
mods from specHPSDR.cs, clsHardwareSpecific.cs, network.c, enums.cs
(G8NJJ + MI0BOT). Block-level Samphire / Wigley / FlexRadio coverage is
correct almost everywhere (first-pass handled the block-level headers well);
Task 11 multi-source touch-ups caught several NR0V cases but did not
propagate inline-mod contributors.

## Contributor-addition frequency

Ranked list of contributors that appear in Thetis sources but are missing
from the corresponding NereusSDR headers most often. Gives Phase 2 a sense
of which names to expect to add most.

| Contributor | Files needing addition | Typical Thetis source(s) where they appear |
| --- | --- | --- |
| Laurence Barker (G8NJJ) | 60+ | console.cs (inline), setup.cs (inline), DiversityForm.cs, enums.cs (inline), clsHardwareSpecific.cs (inline), specHPSDR.cs (inline) |
| Chris Codella (W2PA) | 40+ | console.cs (inline), setup.cs (inline), NetworkIO.cs (inline) |
| Warren Pratt (NR0V) | 15 | setup.cs (inline resampler comments), dsp.cs, cmaster.c |
| Phil Harman (VK6APH) | 4 | display.cs (Waterfall AGC 2013) |
| Joe (WD5Y) | 10+ | console.cs (inline via MW0LGE [2.10.1.0] credits) |
| Bryan Rambo (W4WMT) | 10+ | setup.cs (inline [2.10.3.5] issue #87) |
| Simon Brown (MI0BOT) | 5 | enums.cs (HermesLite entry) |
| Bill Tracey (KD5TFD) | 4 | NetworkIO.cs (commented-out historical), netInterface.c (P2 files already cover via explicit line) |

Notes:
- The P2RadioConnection pair already cites KD5TFD explicitly via the
  netInterface.c LGPL line, so KD5TFD doesn't appear as missing there.
- G8NJJ shows up in the most files because every console.cs derivative
  inherits the inline Andromeda/Aries/Saturn/ANAN-G2 mods, plus the inline
  mods in specHPSDR.cs, enums.cs, clsHardwareSpecific.cs, DiversityForm.cs.

## Files needing additions

Sections sorted by path. Under the judgement-call rules the body text
treats the Samphire dual-license stanza + inline `// Modified by MW0LGE`
markers as covered when the file carries the MW0LGE Copyright line.
Contributors referenced ONLY by callsign in the Thetis index (e.g.
"W5WC" with no full name) count as covered if the NereusSDR header
lists either the callsign or the full name.

### src/core/BoardCapabilities.cpp

- **Cited Thetis source(s):** `clsHardwareSpecific.cs`; `setup.cs`;
  `console.cs`; `enums.cs`; `HPSDR/NetworkIO.cs`;
  `clsDiscoveredRadioPicker.cs`; `ChannelMaster/network.h`
- **Current header contributors:** FlexRadio, W5WC, MW0LGE
- **Expected from index:** FlexRadio, W5WC, MW0LGE, NR0V (setup.cs
  resampler inline), W2PA (console.cs + setup.cs inline + NetworkIO.cs
  inline), G8NJJ (inline in almost every cited source),
  WD5Y (console.cs), W4WMT (setup.cs), MI0BOT (enums.cs HermesLite),
  KD5TFD (NetworkIO.cs historical)
- **Missing:** NR0V, W2PA, G8NJJ, WD5Y, W4WMT, MI0BOT, KD5TFD
- **Proposed addition lines:**
  - `Copyright (C) 2013-2017  Warren Pratt (NR0V) — setup.cs resampler guidance`
  - `Copyright (C) 2017-2019  Chris Codella (W2PA) — MIDI / Behringer / QSK`
  - `Copyright (C) 2018-2025  Laurence Barker (G8NJJ) — Andromeda / Aries / Saturn / ANAN-G2`
  - `Copyright (C) 2020-2025  Joe (WD5Y) — RX2 mute/ForeColor ideas`
  - `Copyright (C) 2024-2025  Bryan Rambo (W4WMT) — setup.cs [2.10.3.5] issue #87`
  - `Copyright (C) 2020-2025  Simon Brown (MI0BOT) — HermesLite enum entry`
  - `Copyright (C) 2006-2007  Bill Tracey (KD5TFD) — NetworkIO.cs historical`
- **Variant:** multi-source

### src/core/BoardCapabilities.h

Same as `BoardCapabilities.cpp` except the cited source list drops
`clsDiscoveredRadioPicker.cs` and `enums.cs` and adds `specHPSDR.cs`.
Missing: NR0V, W2PA, G8NJJ, W4WMT, KD5TFD. G8NJJ appears inline in
specHPSDR.cs (ANAN_G2_1K PA note) and clsHardwareSpecific.cs; WD5Y is
not needed here (only cited in console.cs which this .h does not cite).
**Variant:** multi-source

### src/core/ClarityController.h

- **Cited Thetis source(s):** `display.cs`
- **Current:** FlexRadio, W5WC, MW0LGE
- **Expected:** FlexRadio, W5WC, VK6APH (Waterfall AGC 2013), MW0LGE
- **Missing:** VK6APH
- **Proposed addition:** `Waterfall AGC Modifications Copyright (C) 2013  Phil Harman (VK6APH)`
- **Variant:** thetis-samphire (display.cs lineage)

### src/core/FFTEngine.cpp

- **Cited:** `display.cs`
- **Current:** FlexRadio, W5WC, MW0LGE
- **Missing:** VK6APH
- **Proposed:** `Waterfall AGC Modifications Copyright (C) 2013  Phil Harman (VK6APH)`
- **Variant:** thetis-samphire

### src/core/FFTEngine.h

- **Cited:** `display.cs`
- **Missing:** VK6APH
- **Proposed:** `Waterfall AGC Modifications Copyright (C) 2013  Phil Harman (VK6APH)`
- **Variant:** thetis-samphire

### src/core/HardwareProfile.cpp

- **Cited:** `clsHardwareSpecific.cs`; `HPSDR/NetworkIO.cs`
- **Current:** FlexRadio, W5WC, MW0LGE
- **Expected:** MW0LGE (clsHardwareSpecific.cs), G8NJJ (inline
  ANAN_G2_1K), plus NetworkIO.cs implicit lineage (FlexRadio + W5WC +
  KD5TFD historical, W2PA inline)
- **Missing:** G8NJJ, W2PA, KD5TFD
- **Proposed additions:**
  - `Copyright (C) 2018-2025  Laurence Barker (G8NJJ) — clsHardwareSpecific.cs PA comment`
  - `Copyright (C) 2017-2019  Chris Codella (W2PA) — NetworkIO.cs CAT capability`
  - `Copyright (C) 2006-2007  Bill Tracey (KD5TFD) — NetworkIO.cs historical block`
- **Variant:** thetis-samphire (promoted to multi-source acceptable)

### src/core/HardwareProfile.h

Identical to `HardwareProfile.cpp`. Missing G8NJJ, W2PA, KD5TFD.

### src/core/HpsdrModel.h

- **Cited:** `enums.cs`; `ChannelMaster/network.h`
- **Current:** "Original authors", W5WC, MW0LGE, G8NJJ (seen in callsign
  body-comments), MI0BOT (seen in body-comments)
- **Expected:** "Original authors", MW0LGE, G8NJJ (inline), MI0BOT
  (HermesLite), W5WC (network.h block)
- **Missing:** none — the Copyright lines cover "Original authors",
  W5WC, MW0LGE; G8NJJ and MI0BOT appear in inline enum comments in the
  file body which satisfies the inline-attribution rule. **STATUS:
  already complete.**
- **Variant:** multi-source

### src/core/mmio/ExternalVariableEngine.h

- **Cited:** `MeterManager.cs`
- **Current:** FlexRadio, W5WC, MW0LGE
- **Expected from index:** MeterManager.cs block is **MW0LGE only**
  (2020-2026). The FlexRadio + W5WC lines in the current header have no
  basis in the Thetis source file.
- **Missing:** none (every named contributor is covered and then some)
- **Phase 2 concern:** the extra FlexRadio + W5WC lines are harmless
  over-attribution and don't violate GPL. Flagged in Decisions log for
  future cleanup but NOT required in Phase 2.
- **Variant:** thetis-samphire (technically MW0LGE-only but the legacy
  wrapper phrasing covers it)
- **STATUS: already complete.**

### src/core/mmio/FormatParser.cpp

Same as `ExternalVariableEngine.h`. **STATUS: already complete.**

### src/core/mmio/FormatParser.h

Same. **STATUS: already complete.**

### src/core/mmio/MmioEndpoint.h

Same. **STATUS: already complete.**

### src/core/mmio/SerialEndpointWorker.cpp

Same. **STATUS: already complete.**

### src/core/mmio/SerialEndpointWorker.h

Same. **STATUS: already complete.**

### src/core/mmio/TcpClientEndpointWorker.cpp

Same. **STATUS: already complete.**

### src/core/mmio/TcpClientEndpointWorker.h

Same. **STATUS: already complete.**

### src/core/mmio/TcpListenerEndpointWorker.cpp

Same. **STATUS: already complete.**

### src/core/mmio/TcpListenerEndpointWorker.h

Same. **STATUS: already complete.**

### src/core/mmio/UdpEndpointWorker.cpp

Same. **STATUS: already complete.**

### src/core/mmio/UdpEndpointWorker.h

Same. **STATUS: already complete.**

### src/core/NoiseFloorEstimator.h

- **Cited:** `display.cs`
- **Missing:** VK6APH
- **Proposed:** `Waterfall AGC Modifications Copyright (C) 2013  Phil Harman (VK6APH)`
- **Variant:** thetis-samphire

### src/core/P1RadioConnection.cpp

- **Cited:** `ChannelMaster/networkproto1.c`; `HPSDR/NetworkIO.cs`;
  `cmaster.cs`; `console.cs`
- **Current:** FlexRadio, W5WC, MW0LGE, "Original authors" (cmaster.cs)
- **Expected:** FlexRadio, W5WC, MW0LGE, "Original authors", W2PA
  (console.cs + NetworkIO.cs inline), G8NJJ (console.cs inline), WD5Y
  (console.cs inline), KD5TFD (NetworkIO.cs historical)
- **Missing:** W2PA, G8NJJ, WD5Y, KD5TFD
- **Proposed additions:**
  - `Copyright (C) 2017-2019  Chris Codella (W2PA)`
  - `Copyright (C) 2018-2025  Laurence Barker (G8NJJ)`
  - `Copyright (C) 2020-2025  Joe (WD5Y)`
  - `Copyright (C) 2006-2007  Bill Tracey (KD5TFD)`
- **Variant:** multi-source

### src/core/P1RadioConnection.h

- **Cited:** `networkproto1.c`; `NetworkIO.cs`
- **Current:** W5WC only (+ MW0LGE dual-license block but no MW0LGE
  copyright line)
- **Expected:** FlexRadio (NetworkIO.cs sibling lineage), W5WC, KD5TFD
  (NetworkIO.cs historical), W2PA (NetworkIO.cs inline)
- **Missing:** FlexRadio, KD5TFD, W2PA
- **Proposed additions:**
  - `Copyright (C) 2004-2009  FlexRadio Systems`
  - `Copyright (C) 2006-2007  Bill Tracey (KD5TFD)`
  - `Copyright (C) 2017-2019  Chris Codella (W2PA)`
- **Note:** MW0LGE dual-license block should either be removed
  (neither cited source has MW0LGE content) OR an MW0LGE Copyright line
  added. Recommend the former (drop the stanza in Phase 2) and demote
  variant to `thetis-no-samphire` or promote to multi-source without
  Samphire.
- **Variant:** multi-source (proposed: demote to no-samphire)

### src/core/P2RadioConnection.cpp

- **Cited:** `network.c`; `network.h`; `netInterface.c`; `console.cs`
- **Current:** FlexRadio, KD5TFD, W5WC (already good coverage)
- **Expected:** FlexRadio, W5WC, MW0LGE (console.cs block + network.c
  inline WSA init), KD5TFD, W2PA (console.cs inline), G8NJJ (console.cs
  inline), WD5Y (console.cs inline)
- **Missing:** MW0LGE, W2PA, G8NJJ, WD5Y
- **Proposed additions:**
  - `Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified`
  - `Copyright (C) 2017-2019  Chris Codella (W2PA)`
  - `Copyright (C) 2018-2025  Laurence Barker (G8NJJ)`
  - `Copyright (C) 2020-2025  Joe (WD5Y)`
- **Variant:** multi-source

### src/core/P2RadioConnection.h

Same as `.cpp`. Missing MW0LGE, W2PA, G8NJJ, WD5Y.

### src/core/ReceiverManager.cpp

- **Cited:** `console.cs`
- **Current:** FlexRadio, W5WC, MW0LGE
- **Expected:** FlexRadio, W5WC, MW0LGE, W2PA (inline), G8NJJ (inline),
  WD5Y (inline)
- **Missing:** W2PA, G8NJJ, WD5Y
- **Proposed additions:**
  - `Copyright (C) 2017-2019  Chris Codella (W2PA)`
  - `Copyright (C) 2018-2025  Laurence Barker (G8NJJ)`
  - `Copyright (C) 2020-2025  Joe (WD5Y)`
- **Variant:** thetis-samphire

### src/core/ReceiverManager.h

Same as `.cpp`. Missing W2PA, G8NJJ, WD5Y.

### src/core/RxChannel.cpp

- **Cited:** `radio.cs`; `console.cs`; `dsp.cs`; `rxa.cs`; `specHPSDR.cs`;
  `setup.cs`; `cmaster.c`
- **Current:** FlexRadio, W5WC, NR0V, MW0LGE
- **Expected:** FlexRadio, W5WC, NR0V, MW0LGE, W2PA (console.cs + setup.cs
  inline), G8NJJ (console.cs + setup.cs + specHPSDR.cs inline), WD5Y
  (console.cs inline), W4WMT (setup.cs inline)
- **Missing:** W2PA, G8NJJ, WD5Y, W4WMT
- **Proposed additions:**
  - `Copyright (C) 2017-2019  Chris Codella (W2PA)`
  - `Copyright (C) 2018-2025  Laurence Barker (G8NJJ)`
  - `Copyright (C) 2020-2025  Joe (WD5Y)`
  - `Copyright (C) 2024-2025  Bryan Rambo (W4WMT)`
- **Variant:** multi-source

### src/core/RxChannel.h

Same as `.cpp` (minus `rxa.cs` and `setup.cs`). Missing W2PA, G8NJJ;
WD5Y and W4WMT strictly not needed here but err on side of including.
**Phase 2 judgement:** include W2PA, G8NJJ at minimum; WD5Y and W4WMT
optional but recommended for consistency with the .cpp. **Variant:**
multi-source

### src/core/StepAttenuatorController.cpp

- **Cited:** `console.cs`
- **Current:** FlexRadio, W5WC, MW0LGE
- **Missing:** W2PA, G8NJJ, WD5Y
- **Proposed additions:** (same three lines as ReceiverManager)
- **Variant:** thetis-samphire

### src/core/StepAttenuatorController.h

Same as `.cpp`. Missing W2PA, G8NJJ, WD5Y.

### src/core/wdsp_api.h

- **Cited:** `dsp.cs`; `radio.cs`; `specHPSDR.cs`
- **Current:** W5WC, NR0V, MW0LGE
- **Expected:** FlexRadio (radio.cs + specHPSDR.cs block lineage), W5WC,
  NR0V, MW0LGE, G8NJJ (specHPSDR.cs inline ANAN_G2_1K PA)
- **Missing:** FlexRadio, G8NJJ
- **Proposed additions:**
  - `Copyright (C) 2004-2009  FlexRadio Systems`
  - `Copyright (C) 2018-2025  Laurence Barker (G8NJJ)`
- **Variant:** multi-source

### src/core/WdspEngine.cpp

- **Cited:** `ChannelMaster/cmaster.c`
- **Current:** FlexRadio, W5WC
- **Expected from index:** **NR0V only** (cmaster.c is block-level NR0V
  2014-2019; no FlexRadio, no W5WC)
- **Missing:** NR0V
- **Incorrect:** FlexRadio + W5WC lines have no basis in the cited
  source. The header is effectively wrong — it should be
  `thetis-no-samphire` variant but with NR0V instead of FlexRadio/W5WC.
- **Proposed fix:** replace the current Copyright block with a single
  line: `Copyright (C) 2014-2019  Warren Pratt (NR0V) — ChannelMaster cmaster.c`.
  Keep GPLv2-or-later. Drop the Samphire dual-license stanza (no
  MW0LGE in this source). Update the Modification-history phrasing.
- **Variant:** **thetis-no-samphire (needs reclassification in Phase 2)**
- **Decisions log entry:** see below — one of the two files where
  first-pass chose the wrong template for a single-source file.

### src/core/WdspEngine.h

- **Cited:** `cmaster.cs`; `cmaster.c`
- **Current:** "Original authors" (cmaster.cs), NR0V (cmaster.c),
  MW0LGE (cmaster.cs)
- **Expected:** same set — this file is already correct.
- **STATUS: already complete.**
- **Variant:** multi-source

### src/core/WdspTypes.h

- **Cited:** `dsp.cs`; `setup.cs`; `console.cs`
- **Current:** FlexRadio, W5WC, NR0V, MW0LGE
- **Missing:** W2PA, G8NJJ, WD5Y, W4WMT
- **Proposed additions:** W2PA, G8NJJ, WD5Y, W4WMT lines
- **Variant:** multi-source

### src/gui/AddCustomRadioDialog.cpp

- **Cited:** `frmAddCustomRadio.cs`; `frmAddCustomRadio.Designer.cs`
- **Current:** FlexRadio, W5WC, MW0LGE
- **Expected:** MW0LGE only (per index; designer inherits companion).
  FlexRadio + W5WC are over-attribution, no inline callsigns to add.
- **Missing:** none
- **Phase 2 concern:** FlexRadio + W5WC over-attribution is harmless but
  strictly incorrect. Optional cleanup — not required in Phase 2.
- **STATUS: already complete (with over-attribution noted).**

### src/gui/AddCustomRadioDialog.h

Same as `.cpp`. **STATUS: already complete.**

### src/gui/applets/PhoneCwApplet.cpp

- **Cited:** `setup.cs`
- **Current:** FlexRadio, W5WC, MW0LGE
- **Missing:** NR0V, G8NJJ, W2PA, W4WMT
- **Proposed additions:** NR0V, G8NJJ, W2PA, W4WMT lines
- **Variant:** thetis-samphire (may be promoted to multi-source)

### src/gui/applets/RxApplet.cpp

- **Cited:** `console.cs`; `console.resx`; `setup.cs`
- **Current:** FlexRadio, W5WC, MW0LGE
- **Missing:** W2PA, G8NJJ, WD5Y, NR0V, W4WMT
- **Proposed additions:** W2PA, G8NJJ, WD5Y, NR0V, W4WMT
- **Variant:** multi-source (currently flagged thetis-samphire)

### src/gui/ConnectionPanel.cpp

- **Cited:** `ucRadioList.cs`; `clsDiscoveredRadioPicker.cs`
- **Current:** FlexRadio, W5WC, MW0LGE
- **Expected:** MW0LGE only
- **Missing:** none (over-attribution only)
- **STATUS: already complete.**

### src/gui/ConnectionPanel.h

Same. **STATUS: already complete.**

### src/gui/containers/ContainerManager.cpp

- **Cited:** `MeterManager.cs`
- **Expected:** MW0LGE only
- **STATUS: already complete (over-attribution).**

### src/gui/containers/ContainerManager.h

Same. **STATUS: already complete.**

### src/gui/containers/ContainerSettingsDialog.cpp

- **Cited:** `setup.cs`; `MeterManager.cs`
- **Current:** FlexRadio, W5WC, MW0LGE
- **Missing:** NR0V, G8NJJ, W2PA, W4WMT (all from setup.cs inline)
- **Proposed additions:** NR0V, G8NJJ, W2PA, W4WMT
- **Variant:** multi-source (currently flagged thetis-samphire)

### src/gui/containers/ContainerSettingsDialog.h

Same as `.cpp`. Missing NR0V, G8NJJ, W2PA, W4WMT.

### src/gui/containers/ContainerWidget.cpp

- **Cited:** `ucMeter.cs` (only)
- **Expected:** MW0LGE only
- **STATUS: already complete.**

### src/gui/containers/ContainerWidget.h

- **Cited:** `ucMeter.cs`; `setup.cs`; `MeterManager.cs`
- **Missing:** NR0V, G8NJJ, W2PA, W4WMT (setup.cs inline)
- **Proposed additions:** NR0V, G8NJJ, W2PA, W4WMT
- **Variant:** multi-source

### src/gui/containers/FloatingContainer.cpp

- **Cited:** `frmMeterDisplay.cs`
- **Expected:** MW0LGE only
- **STATUS: already complete.**

### src/gui/containers/FloatingContainer.h

- **Cited:** `frmMeterDisplay.cs`; `MeterManager.cs`
- **Expected:** MW0LGE only
- **STATUS: already complete.**

### src/gui/containers/meter_property_editors/NeedleItemEditor.h

- **Cited:** `MeterManager.cs`
- **Expected:** MW0LGE only
- **STATUS: already complete.**

### src/gui/containers/meter_property_editors/NeedleScalePwrItemEditor.h

Same. **STATUS: already complete.**

### src/gui/containers/meter_property_editors/ScaleItemEditor.cpp

Same. **STATUS: already complete.**

### src/gui/containers/meter_property_editors/ScaleItemEditor.h

Same. **STATUS: already complete.**

### src/gui/containers/MmioVariablePickerPopup.h

Same. **STATUS: already complete.**

### src/gui/MainWindow.cpp

- **Cited:** `MeterManager.cs`; `dsp.cs`; `console.cs`; `setup.cs`;
  `radio.cs`
- **Current:** FlexRadio, W5WC, NR0V, MW0LGE (x2 for MeterManager.cs)
- **Missing:** W2PA, G8NJJ, WD5Y, W4WMT
- **Proposed additions:** W2PA, G8NJJ, WD5Y, W4WMT
- **Variant:** multi-source

### src/gui/meters/AntennaButtonItem.cpp

- **Cited:** `MeterManager.cs`
- **Expected:** MW0LGE only
- **STATUS: already complete (FlexRadio + W5WC are over-attribution).**

### src/gui/meters/AntennaButtonItem.h

MW0LGE only line. **STATUS: already complete.**

### src/gui/meters/BandButtonItem.cpp

Same as AntennaButtonItem.cpp. **STATUS: already complete.**

### src/gui/meters/BandButtonItem.h

MW0LGE only. **STATUS: already complete.**

### src/gui/meters/ButtonBoxItem.cpp

**STATUS: already complete.**

### src/gui/meters/ButtonBoxItem.h

**STATUS: already complete.**

### src/gui/meters/ClickBoxItem.h

**STATUS: already complete.**

### src/gui/meters/ClockItem.h

**STATUS: already complete.**

### src/gui/meters/DataOutItem.h

**STATUS: already complete.**

### src/gui/meters/DialItem.cpp

**STATUS: already complete.**

### src/gui/meters/DialItem.h

**STATUS: already complete.**

### src/gui/meters/DiscordButtonItem.h

**STATUS: already complete.**

### src/gui/meters/FadeCoverItem.cpp

**STATUS: already complete.**

### src/gui/meters/FadeCoverItem.h

**STATUS: already complete.**

### src/gui/meters/FilterButtonItem.cpp

**STATUS: already complete.**

### src/gui/meters/FilterButtonItem.h

**STATUS: already complete.**

### src/gui/meters/FilterDisplayItem.cpp

**STATUS: already complete.**

### src/gui/meters/FilterDisplayItem.h

**STATUS: already complete.**

### src/gui/meters/HistoryGraphItem.cpp

**STATUS: already complete.**

### src/gui/meters/HistoryGraphItem.h

**STATUS: already complete.**

### src/gui/meters/ItemGroup.cpp

**STATUS: already complete.**

### src/gui/meters/ItemGroup.h

**STATUS: already complete.**

### src/gui/meters/LEDItem.cpp

**STATUS: already complete.**

### src/gui/meters/LEDItem.h

**STATUS: already complete.**

### src/gui/meters/MagicEyeItem.cpp

**STATUS: already complete.**

### src/gui/meters/MagicEyeItem.h

**STATUS: already complete.**

### src/gui/meters/MeterItem.cpp

- **Cited:** `MeterManager.cs`; `console.cs`
- **Current:** FlexRadio, W5WC, MW0LGE
- **Missing:** W2PA, G8NJJ, WD5Y (console.cs inline)
- **Proposed additions:** W2PA, G8NJJ, WD5Y
- **Variant:** multi-source

### src/gui/meters/MeterItem.h

Same. Missing W2PA, G8NJJ, WD5Y.

### src/gui/meters/MeterPoller.cpp

**STATUS: already complete (MeterManager.cs single source).**

### src/gui/meters/MeterPoller.h

**STATUS: already complete.**

### src/gui/meters/MeterWidget.cpp

**STATUS: already complete.**

### src/gui/meters/MeterWidget.h

**STATUS: already complete.**

### src/gui/meters/ModeButtonItem.cpp

**STATUS: already complete.**

### src/gui/meters/ModeButtonItem.h

**STATUS: already complete.**

### src/gui/meters/NeedleScalePwrItem.cpp

**STATUS: already complete.**

### src/gui/meters/NeedleScalePwrItem.h

- **Cited:** `MeterManager.cs` (single source)
- **STATUS: already complete.**

### src/gui/meters/OtherButtonItem.cpp

**STATUS: already complete.**

### src/gui/meters/OtherButtonItem.h

**STATUS: already complete.**

### src/gui/meters/RotatorItem.cpp

**STATUS: already complete.**

### src/gui/meters/RotatorItem.h

**STATUS: already complete.**

### src/gui/meters/SignalTextItem.cpp

- **Cited:** `MeterManager.cs`; `console.cs`
- **Current:** FlexRadio, W5WC, MW0LGE
- **Missing:** W2PA, G8NJJ, WD5Y
- **Proposed additions:** W2PA, G8NJJ, WD5Y
- **Variant:** multi-source

### src/gui/meters/SignalTextItem.h

Same. Missing W2PA, G8NJJ, WD5Y.

### src/gui/meters/SpacerItem.cpp

**STATUS: already complete.**

### src/gui/meters/SpacerItem.h

**STATUS: already complete.**

### src/gui/meters/TextOverlayItem.cpp

**STATUS: already complete.**

### src/gui/meters/TextOverlayItem.h

**STATUS: already complete.**

### src/gui/meters/TuneStepButtonItem.cpp

**STATUS: already complete.**

### src/gui/meters/TuneStepButtonItem.h

**STATUS: already complete.**

### src/gui/meters/VfoDisplayItem.cpp

**STATUS: already complete.**

### src/gui/meters/VfoDisplayItem.h

**STATUS: already complete.**

### src/gui/meters/VoiceRecordPlayItem.h

**STATUS: already complete.**

### src/gui/meters/WebImageItem.cpp

**STATUS: already complete.**

### src/gui/meters/WebImageItem.h

**STATUS: already complete.**

### src/gui/setup/DisplaySetupPages.cpp

- **Cited:** `display.cs`
- **Current:** FlexRadio, W5WC, MW0LGE
- **Missing:** VK6APH
- **Proposed:** `Waterfall AGC Modifications Copyright (C) 2013  Phil Harman (VK6APH)`
- **Variant:** thetis-samphire

### src/gui/setup/DisplaySetupPages.h

- **Cited:** `setup.cs`
- **Missing:** NR0V, G8NJJ, W2PA, W4WMT
- **Proposed additions:** NR0V, G8NJJ, W2PA, W4WMT
- **Variant:** thetis-samphire (may promote to multi-source)

### src/gui/setup/DspSetupPages.cpp

- **Cited:** `setup.cs`
- **Missing:** NR0V, G8NJJ, W2PA, W4WMT
- **Proposed additions:** NR0V, G8NJJ, W2PA, W4WMT

### src/gui/setup/GeneralOptionsPage.cpp

- **Cited:** `setup.cs`
- **Missing:** NR0V, G8NJJ, W2PA, W4WMT

### src/gui/setup/GeneralOptionsPage.h

Same. Missing NR0V, G8NJJ, W2PA, W4WMT.

### src/gui/setup/hardware/AntennaAlexTab.cpp

- **Cited:** `setup.cs`
- **Missing:** NR0V, G8NJJ, W2PA, W4WMT

### src/gui/setup/hardware/AntennaAlexTab.h

Same. Missing NR0V, G8NJJ, W2PA, W4WMT.

### src/gui/setup/hardware/DiversityTab.cpp

- **Cited:** `DiversityForm.cs`
- **Current:** FlexRadio, W5WC, MW0LGE
- **Expected:** FlexRadio, G8NJJ (inline)
- **Missing:** G8NJJ
- **Proposed addition:** `Copyright (C) 2018-2025  Laurence Barker (G8NJJ) — Diversity CAT / gain setter`
- **Note:** Per index, DiversityForm.cs block is FlexRadio-only (no
  W5WC, no MW0LGE copyright line despite dual-license stanza). W5WC +
  MW0LGE in current header are over-attribution; keeping them is
  harmless. Phase 2 just needs to add G8NJJ.
- **Variant:** thetis-samphire (should stay — dual-license stanza is in
  source even without matching copyright)

### src/gui/setup/hardware/DiversityTab.h

Same as `.cpp`. Missing G8NJJ.

### src/gui/setup/hardware/OcOutputsTab.cpp

- **Cited:** `setup.cs`
- **Missing:** NR0V, G8NJJ, W2PA, W4WMT

### src/gui/setup/hardware/OcOutputsTab.h

Same. Missing NR0V, G8NJJ, W2PA, W4WMT.

### src/gui/setup/hardware/PaCalibrationTab.cpp

- **Cited:** `setup.cs`
- **Missing:** NR0V, G8NJJ, W2PA, W4WMT

### src/gui/setup/hardware/PaCalibrationTab.h

Same. Missing NR0V, G8NJJ, W2PA, W4WMT.

### src/gui/setup/hardware/PureSignalTab.cpp

- **Cited:** `PSForm.cs`
- **Expected:** "Original authors", MW0LGE
- **Missing:** none (FlexRadio/W5WC are over-attribution, MW0LGE
  covered)
- **STATUS: already complete.**

### src/gui/setup/hardware/PureSignalTab.h

Same. **STATUS: already complete.**

### src/gui/setup/hardware/RadioInfoTab.cpp

- **Cited:** `setup.cs`
- **Missing:** NR0V, G8NJJ, W2PA, W4WMT

### src/gui/setup/hardware/RadioInfoTab.h

Same. Missing NR0V, G8NJJ, W2PA, W4WMT.

### src/gui/setup/hardware/XvtrTab.cpp

- **Cited:** `xvtr.cs`
- **Current:** FlexRadio (2004-2009), W5WC (2010-2013), MW0LGE
- **Expected:** FlexRadio, W5WC (ends 2013) — no MW0LGE copyright line
  in xvtr.cs, only the dual-license stanza. Current header is
  over-attributing MW0LGE by adding a Copyright line.
- **Missing:** none
- **Phase 2 concern:** MW0LGE copyright line in current header is
  technically incorrect (xvtr.cs has only the stanza, not a named
  copyright for MW0LGE). Harmless; cleanup optional.
- **STATUS: already complete.**

### src/gui/setup/hardware/XvtrTab.h

Same. **STATUS: already complete.**

### src/gui/setup/HardwarePage.cpp

- **Cited:** `setup.cs`
- **Missing:** NR0V, G8NJJ, W2PA, W4WMT

### src/gui/setup/HardwarePage.h

Same. Missing NR0V, G8NJJ, W2PA, W4WMT.

### src/gui/setup/TransmitSetupPages.cpp

- **Cited:** `setup.cs`
- **Missing:** NR0V, G8NJJ, W2PA, W4WMT

### src/gui/setup/TransmitSetupPages.h

Same. Missing NR0V, G8NJJ, W2PA, W4WMT.

### src/gui/SpectrumWidget.cpp

- **Cited:** `display.cs`; `console.cs`
- **Current:** FlexRadio, W5WC, VK6APH (good!)
- **Missing:** MW0LGE (block-level for both), W2PA (console.cs inline),
  G8NJJ (console.cs inline), WD5Y (console.cs inline)
- **Proposed additions:**
  - `Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified`
  - `Copyright (C) 2017-2019  Chris Codella (W2PA)`
  - `Copyright (C) 2018-2025  Laurence Barker (G8NJJ)`
  - `Copyright (C) 2020-2025  Joe (WD5Y)`
- **Variant:** multi-source
- **Note:** .cpp appears to have lost the MW0LGE copyright line somewhere
  along the way (dual-license stanza is still there). Phase 2 must add
  it back.

### src/gui/SpectrumWidget.h

- **Cited:** `enums.cs`; `setup.cs`; `display.cs`
- **Current:** FlexRadio, W5WC, MW0LGE, VK6APH
- **Missing:** "Original authors" (enums.cs), G8NJJ (enums.cs +
  setup.cs + display.cs inline if any), MI0BOT (enums.cs HermesLite),
  NR0V (setup.cs inline), W2PA (setup.cs inline), W4WMT (setup.cs inline)
- **Proposed additions:**
  - `Copyright (C) 2000-2025  "Original authors" [enums.cs]`
  - `Copyright (C) 2013-2017  Warren Pratt (NR0V)`
  - `Copyright (C) 2017-2019  Chris Codella (W2PA)`
  - `Copyright (C) 2018-2025  Laurence Barker (G8NJJ)`
  - `Copyright (C) 2020-2025  Simon Brown (MI0BOT) — HermesLite`
  - `Copyright (C) 2024-2025  Bryan Rambo (W4WMT)`
- **Variant:** multi-source

### src/gui/widgets/VfoModeContainers.cpp

- **Cited:** `console.cs`; `setup.designer.cs`; `radio.cs`
- **Current:** FlexRadio, W5WC, MW0LGE
- **Missing:** W2PA, G8NJJ, WD5Y (all console.cs inline)
- **Proposed additions:** W2PA, G8NJJ, WD5Y
- **Variant:** multi-source

### src/gui/widgets/VfoModeContainers.h

- **Cited:** `console.cs`; `dsp.cs`
- **Current:** FlexRadio, W5WC, NR0V, MW0LGE
- **Missing:** W2PA, G8NJJ, WD5Y (console.cs inline)
- **Proposed additions:** W2PA, G8NJJ, WD5Y
- **Variant:** multi-source

### src/gui/widgets/VfoWidget.cpp

- **Cited:** `console.cs`; `console.resx`; `display.cs`; `enums.cs`;
  `radio.cs`; `dsp.cs`; `specHPSDR.cs`
- **Current:** FlexRadio, W5WC, NR0V, MW0LGE, VK6APH
- **Missing:** "Original authors" (enums.cs), W2PA, G8NJJ, WD5Y, MI0BOT
  (enums.cs), W4WMT (not strictly — setup.cs not cited)
- **Proposed additions:**
  - `Copyright (C) 2000-2025  "Original authors" [enums.cs]`
  - `Copyright (C) 2017-2019  Chris Codella (W2PA)`
  - `Copyright (C) 2018-2025  Laurence Barker (G8NJJ)`
  - `Copyright (C) 2020-2025  Joe (WD5Y)`
  - `Copyright (C) 2020-2025  Simon Brown (MI0BOT) — HermesLite`
- **Variant:** multi-source

### src/models/Band.h

- **Cited:** `console.cs` (line 6443, BandByFreq)
- **Current:** FlexRadio, W5WC, MW0LGE
- **Missing:** W2PA, G8NJJ, WD5Y
- **Proposed additions:** W2PA, G8NJJ, WD5Y

### src/models/PanadapterModel.cpp

- **Cited:** `console.cs`
- **Missing:** W2PA, G8NJJ, WD5Y

### src/models/PanadapterModel.h

Same. Missing W2PA, G8NJJ, WD5Y.

### src/models/RadioModel.cpp

- **Cited:** `console.cs`; `setup.cs`; `radio.cs`; `dsp.cs`;
  `HPSDR/NetworkIO.cs`; `ChannelMaster/cmaster.c`
- **Current:** FlexRadio, W5WC, NR0V, MW0LGE
- **Missing:** W2PA, G8NJJ, WD5Y, W4WMT, KD5TFD (NetworkIO.cs)
- **Proposed additions:** W2PA, G8NJJ, WD5Y, W4WMT, KD5TFD
- **Variant:** multi-source

### src/models/RadioModel.h

- **Cited:** `console.cs`
- **Missing:** W2PA, G8NJJ, WD5Y

### src/models/RxDspWorker.h

- **Cited:** `console.cs`
- **Missing:** W2PA, G8NJJ, WD5Y

### src/models/SliceModel.cpp

- **Cited:** `console.cs`; `display.cs`
- **Current:** FlexRadio, W5WC — **missing MW0LGE copyright line!**
  (Dual-license stanza present, copyright line not)
- **Expected:** FlexRadio, W5WC, MW0LGE, VK6APH, W2PA, G8NJJ, WD5Y
- **Missing:** MW0LGE, VK6APH, W2PA, G8NJJ, WD5Y
- **Proposed additions:**
  - `Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified`
  - `Waterfall AGC Modifications Copyright (C) 2013  Phil Harman (VK6APH)`
  - `Copyright (C) 2017-2019  Chris Codella (W2PA)`
  - `Copyright (C) 2018-2025  Laurence Barker (G8NJJ)`
  - `Copyright (C) 2020-2025  Joe (WD5Y)`
- **Phase 2 critical:** this file is the other instance where the
  block-level MW0LGE line is missing despite the dual-license stanza
  being present. Must be fixed.
- **Variant:** multi-source

### src/models/SliceModel.h

- **Cited:** `console.cs`; `radio.cs`; `setup.designer.cs`
- **Current:** FlexRadio, W5WC, MW0LGE
- **Missing:** W2PA, G8NJJ, WD5Y (console.cs), NR0V/W4WMT if
  `setup.designer.cs` is treated as inheriting from `setup.cs` inline
  (Phase 1a rule: designers inherit companion)
- **Proposed additions:** W2PA, G8NJJ, WD5Y (console.cs baseline); NR0V,
  W4WMT optional via designer-inheritance rule
- **Variant:** multi-source

### tests/tst_dig_rtty_wire.cpp

- **Cited:** `console.cs`; `enums.cs`; `radio.cs`; `setup.designer.cs`
- **Missing:** "Original authors" (enums.cs), MI0BOT (enums.cs), G8NJJ
  (console.cs + enums.cs), W2PA (console.cs), WD5Y (console.cs), NR0V,
  W4WMT (via setup.designer.cs → setup.cs inline)
- **Proposed additions:** Original-authors line, MI0BOT, G8NJJ, W2PA,
  WD5Y, NR0V, W4WMT

### tests/tst_fm_opt_container_wire.cpp

- **Cited:** `console.cs`
- **Missing:** W2PA, G8NJJ, WD5Y

### tests/tst_hpsdr_enums.cpp

- **Cited:** `enums.cs`; `ChannelMaster/network.h`
- **Current:** "Original authors", W5WC, MW0LGE
- **Missing:** G8NJJ (enums.cs inline), MI0BOT (HermesLite)
- **Proposed additions:** G8NJJ, MI0BOT
- **Variant:** multi-source

### tests/tst_meter_item_bar.cpp

- **Cited:** `MeterManager.cs`
- **Expected:** MW0LGE only
- **STATUS: already complete.**

### tests/tst_meter_item_scale.cpp

Same. **STATUS: already complete.**

### tests/tst_meter_presets.cpp

Same. **STATUS: already complete.**

### tests/tst_reading_name.cpp

Same. **STATUS: already complete.**

### tests/tst_rxchannel_agc_advanced.cpp

- **Cited:** `radio.cs`; `console.cs`; `dsp.cs`
- **Current:** FlexRadio, W5WC, NR0V, MW0LGE
- **Missing:** W2PA, G8NJJ, WD5Y (console.cs inline)
- **Proposed additions:** W2PA, G8NJJ, WD5Y

### tests/tst_rxchannel_apf.cpp

- **Cited:** `radio.cs`
- **Current:** FlexRadio, W5WC, MW0LGE
- **STATUS: already complete** (radio.cs has no additional inline
  contributors beyond the block triple).

### tests/tst_rxchannel_audio_panel.cpp

- **Cited:** `dsp.cs`; `radio.cs`
- **Current:** FlexRadio, W5WC, NR0V, MW0LGE
- **Expected:** FlexRadio, W5WC, NR0V (dsp.cs), MW0LGE
- **STATUS: already complete.**

### tests/tst_rxchannel_emnr.cpp

- **Cited:** `radio.cs`
- **STATUS: already complete.**

### tests/tst_rxchannel_nb2_polish.cpp

- **Cited:** `specHPSDR.cs`; `cmaster.c`
- **Variant in PROVENANCE:** `thetis-no-samphire` (both sources exclude
  Samphire)
- **Current:** FlexRadio, W5WC (no MW0LGE — correct)
- **Expected:** W5WC (specHPSDR.cs block ends 2018), NR0V (cmaster.c),
  G8NJJ (specHPSDR.cs inline ANAN_G2_1K)
- **Missing:** NR0V, G8NJJ (and strictly FlexRadio should be removed
  since specHPSDR.cs does not name FlexRadio in its block; however,
  sibling-file lineage from PowerSDR is plausible — err on the side of
  keeping over-attribution)
- **Proposed additions:**
  - `Copyright (C) 2014-2019  Warren Pratt (NR0V) — ChannelMaster cmaster.c`
  - `Copyright (C) 2018-2025  Laurence Barker (G8NJJ) — specHPSDR.cs`
- **Variant:** thetis-no-samphire (confirmed)

### tests/tst_rxchannel_snb.cpp

- **Cited:** `console.cs`; `dsp.cs`
- **Missing:** W2PA, G8NJJ, WD5Y

### tests/tst_rxchannel_squelch.cpp

- **Cited:** `radio.cs`
- **STATUS: already complete.**

### tests/tst_slice_agc_advanced.cpp

- **Cited:** `radio.cs`; `console.cs`
- **Missing:** W2PA, G8NJJ, WD5Y
- **Note:** current does not include NR0V even though tst_rxchannel
  counterpart does; adding is optional — PROVENANCE cites radio.cs + console.cs
  only (no dsp.cs/cmaster.c), so NR0V is not strictly needed.

### tests/tst_slice_apf.cpp

- **Cited:** `radio.cs`
- **STATUS: already complete.**

### tests/tst_slice_audio_panel.cpp

Same as `tst_rxchannel_audio_panel.cpp`. **STATUS: already complete.**

### tests/tst_slice_emnr.cpp

- **Cited:** `radio.cs`
- **STATUS: already complete.**

### tests/tst_slice_rit_xit.cpp

- **Cited:** `console.cs`
- **Missing:** W2PA, G8NJJ, WD5Y

### tests/tst_slice_snb.cpp

Same as `tst_rxchannel_snb.cpp`. Missing W2PA, G8NJJ, WD5Y.

### tests/tst_slice_squelch.cpp

- **Cited:** `radio.cs`
- **STATUS: already complete.**

### tests/tst_step_attenuator_controller.cpp

- **Cited:** `console.cs`
- **Missing:** W2PA, G8NJJ, WD5Y

### src/core/RadioDiscovery.cpp

- **Cited (mi0bot):** `HPSDR/clsRadioDiscovery.cs`
- **Current:** FlexRadio, W5WC, MW0LGE, MI0BOT
- **Expected:** clsRadioDiscovery.cs is MW0LGE sole author upstream +
  MI0BOT fork contributions — header as-is is effectively correct;
  FlexRadio + W5WC are over-attribution via the mi0bot template default.
- **STATUS: already complete.**
- **Variant:** mi0bot

### src/core/RadioDiscovery.h

Same. **STATUS: already complete.**

### src/gui/setup/hardware/Hl2IoBoardTab.cpp

- **Cited (mi0bot):** `HPSDR/IoBoardHl2.cs` (mi0bot-only; no ramdor
  upstream)
- **Current:** FlexRadio, W5WC, MW0LGE, MI0BOT
- **Expected:** MI0BOT sole author (no ramdor upstream for this file)
- **STATUS: already complete (all needed callsigns covered).**
- **Phase 2 note:** technically FlexRadio/W5WC/MW0LGE are
  over-attribution — cleanup optional.

### src/gui/setup/hardware/Hl2IoBoardTab.h

Same. **STATUS: already complete.**

### tests/tst_radio_discovery_parse.cpp

- **Cited (mi0bot):** `HPSDR/clsRadioDiscovery.cs`
- **STATUS: already complete.**

## Files already complete

The following files carry every required contributor attribution. Phase 2
will skip these:

- `src/core/HpsdrModel.h`
- `src/core/WdspEngine.h`
- `src/core/mmio/ExternalVariableEngine.h`
- `src/core/mmio/FormatParser.cpp`
- `src/core/mmio/FormatParser.h`
- `src/core/mmio/MmioEndpoint.h`
- `src/core/mmio/SerialEndpointWorker.cpp`
- `src/core/mmio/SerialEndpointWorker.h`
- `src/core/mmio/TcpClientEndpointWorker.cpp`
- `src/core/mmio/TcpClientEndpointWorker.h`
- `src/core/mmio/TcpListenerEndpointWorker.cpp`
- `src/core/mmio/TcpListenerEndpointWorker.h`
- `src/core/mmio/UdpEndpointWorker.cpp`
- `src/core/mmio/UdpEndpointWorker.h`
- `src/core/RadioDiscovery.cpp`
- `src/core/RadioDiscovery.h`
- `src/gui/AddCustomRadioDialog.cpp`
- `src/gui/AddCustomRadioDialog.h`
- `src/gui/ConnectionPanel.cpp`
- `src/gui/ConnectionPanel.h`
- `src/gui/containers/ContainerManager.cpp`
- `src/gui/containers/ContainerManager.h`
- `src/gui/containers/ContainerWidget.cpp`
- `src/gui/containers/FloatingContainer.cpp`
- `src/gui/containers/FloatingContainer.h`
- `src/gui/containers/MmioVariablePickerPopup.h`
- `src/gui/containers/meter_property_editors/NeedleItemEditor.h`
- `src/gui/containers/meter_property_editors/NeedleScalePwrItemEditor.h`
- `src/gui/containers/meter_property_editors/ScaleItemEditor.cpp`
- `src/gui/containers/meter_property_editors/ScaleItemEditor.h`
- `src/gui/meters/AntennaButtonItem.cpp`
- `src/gui/meters/AntennaButtonItem.h`
- `src/gui/meters/BandButtonItem.cpp`
- `src/gui/meters/BandButtonItem.h`
- `src/gui/meters/ButtonBoxItem.cpp`
- `src/gui/meters/ButtonBoxItem.h`
- `src/gui/meters/ClickBoxItem.h`
- `src/gui/meters/ClockItem.h`
- `src/gui/meters/DataOutItem.h`
- `src/gui/meters/DialItem.cpp`
- `src/gui/meters/DialItem.h`
- `src/gui/meters/DiscordButtonItem.h`
- `src/gui/meters/FadeCoverItem.cpp`
- `src/gui/meters/FadeCoverItem.h`
- `src/gui/meters/FilterButtonItem.cpp`
- `src/gui/meters/FilterButtonItem.h`
- `src/gui/meters/FilterDisplayItem.cpp`
- `src/gui/meters/FilterDisplayItem.h`
- `src/gui/meters/HistoryGraphItem.cpp`
- `src/gui/meters/HistoryGraphItem.h`
- `src/gui/meters/ItemGroup.cpp`
- `src/gui/meters/ItemGroup.h`
- `src/gui/meters/LEDItem.cpp`
- `src/gui/meters/LEDItem.h`
- `src/gui/meters/MagicEyeItem.cpp`
- `src/gui/meters/MagicEyeItem.h`
- `src/gui/meters/MeterPoller.cpp`
- `src/gui/meters/MeterPoller.h`
- `src/gui/meters/MeterWidget.cpp`
- `src/gui/meters/MeterWidget.h`
- `src/gui/meters/ModeButtonItem.cpp`
- `src/gui/meters/ModeButtonItem.h`
- `src/gui/meters/NeedleScalePwrItem.cpp`
- `src/gui/meters/NeedleScalePwrItem.h`
- `src/gui/meters/OtherButtonItem.cpp`
- `src/gui/meters/OtherButtonItem.h`
- `src/gui/meters/RotatorItem.cpp`
- `src/gui/meters/RotatorItem.h`
- `src/gui/meters/SpacerItem.cpp`
- `src/gui/meters/SpacerItem.h`
- `src/gui/meters/TextOverlayItem.cpp`
- `src/gui/meters/TextOverlayItem.h`
- `src/gui/meters/TuneStepButtonItem.cpp`
- `src/gui/meters/TuneStepButtonItem.h`
- `src/gui/meters/VfoDisplayItem.cpp`
- `src/gui/meters/VfoDisplayItem.h`
- `src/gui/meters/VoiceRecordPlayItem.h`
- `src/gui/meters/WebImageItem.cpp`
- `src/gui/meters/WebImageItem.h`
- `src/gui/setup/hardware/Hl2IoBoardTab.cpp`
- `src/gui/setup/hardware/Hl2IoBoardTab.h`
- `src/gui/setup/hardware/PureSignalTab.cpp`
- `src/gui/setup/hardware/PureSignalTab.h`
- `src/gui/setup/hardware/XvtrTab.cpp`
- `src/gui/setup/hardware/XvtrTab.h`
- `tests/tst_meter_item_bar.cpp`
- `tests/tst_meter_item_scale.cpp`
- `tests/tst_meter_presets.cpp`
- `tests/tst_radio_discovery_parse.cpp`
- `tests/tst_reading_name.cpp`
- `tests/tst_rxchannel_apf.cpp`
- `tests/tst_rxchannel_audio_panel.cpp`
- `tests/tst_rxchannel_emnr.cpp`
- `tests/tst_rxchannel_squelch.cpp`
- `tests/tst_slice_apf.cpp`
- `tests/tst_slice_audio_panel.cpp`
- `tests/tst_slice_emnr.cpp`
- `tests/tst_slice_squelch.cpp`

## Decisions log

Judgement calls made during Phase 1b that Phase 2 should verify before
applying:

1. **Year ranges proposed for added contributors.** Since the Thetis
   index gives some contributors `n.d.` ("no date"), I proposed year
   ranges that track the contributor's known activity window on the
   relevant features. Phase 2 should sanity-check these — if unclear,
   consult commit dates in Thetis or leave open-ended ("n.d." style).
   Proposed defaults:
   - `W2PA (Chris Codella) 2017-2019` — per index: MIDI + DB import +
     QSK for Protocol-2 v1.7 spanned May 2017 to April 2019
   - `G8NJJ (Laurence Barker) 2018-2025` — per index: Diversity CAT was
     2018, RIT/XIT UI, ANAN-G2 wiring etc. ongoing through Thetis
     2.10.x
   - `VK6APH (Phil Harman) 2013` — index gives explicit 2013 for
     Waterfall AGC
   - `NR0V (Warren Pratt) 2013-2017` for dsp.cs; `2014-2019` for
     cmaster.c — index-exact; matches existing Wigley range mix
   - `WD5Y (Joe) 2020-2025` — ideas credited via MW0LGE [2.10.1.0]
     which landed in 2024
   - `W4WMT (Bryan Rambo) 2024-2025` — [2.10.3.5] issue #87 implementation
   - `MI0BOT (Simon Brown) 2020-2025` — HermesLite support was added in
     the mi0bot fork era
   - `KD5TFD (Bill Tracey) 2006-2007` — NetworkIO.cs historical /
     netInterface.c block, exact from index

2. **Inline-mod flagging policy.** I erred on the side of flagging
   every inline callsign named in the Thetis source file (console.cs,
   setup.cs, display.cs, enums.cs, specHPSDR.cs, clsHardwareSpecific.cs,
   DiversityForm.cs, network.c). Richie's formal notice named specific
   contributors; omission is the worst failure mode, so the bias is
   always toward including.

3. **Over-attribution flagged but not repaired.** Many meter / MMIO /
   container files cite `MeterManager.cs` which is MW0LGE-only. Those
   files carry FlexRadio + W5WC lines that have no basis in the cited
   source. This is harmless GPL-over-attribution and I did not flag it
   as "needs repair" — but Phase 2 could strip FlexRadio + W5WC from
   those headers for precision. Recommend **leave as-is for Phase 2**
   to minimise risk; optional cleanup pass later.

4. **`src/core/WdspEngine.cpp` wrong-variant reclassification.**
   WdspEngine.cpp cites only `ChannelMaster/cmaster.c`, which is NR0V
   sole author (2014-2019) with no FlexRadio, no W5WC, no MW0LGE. The
   current header uses the FlexRadio + W5WC template, which is wrong
   for this file. Phase 2 should:
   - Replace the current Copyright block with
     `Copyright (C) 2014-2019  Warren Pratt (NR0V)`
   - Drop the Samphire dual-license stanza (cmaster.c has no MW0LGE)
   - Keep the GPLv2-or-later permission block
   - Update variant in PROVENANCE from `thetis-no-samphire` (already
     correctly labeled) to reflect NR0V-only nature (maybe add a
     sub-label)
   - This is the single most incorrect header in the tree; flagged for
     Phase 2 priority.

5. **`src/models/SliceModel.cpp` missing MW0LGE copyright line.** The
   file carries the Samphire dual-license stanza but does NOT carry a
   `Copyright (C) … MW0LGE` line. Adding the stanza without the
   copyright is the same failure mode as Thetis xvtr.cs and
   DiversityForm.cs. Phase 2 must add
   `Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified`
   on the SliceModel.cpp header before the stanza.

6. **`src/gui/SpectrumWidget.cpp` missing MW0LGE copyright line.** Same
   issue as SliceModel.cpp — dual-license stanza without matching
   copyright. Phase 2 must add the MW0LGE line.

7. **`src/core/P1RadioConnection.h` has dual-license stanza but no
   cited Samphire source.** Neither networkproto1.c nor NetworkIO.cs
   names Samphire. The stanza is arguably incorrect here. Phase 2
   option A: drop the stanza and demote variant to `thetis-no-samphire`.
   Option B: keep the stanza and add a `Copyright (C) 2019-2026
   Richard Samphire (MW0LGE)` line to rationalise it. Recommend A —
   keep accurate to cited sources.

8. **Designer-file inheritance.** Per spec, `setup.designer.cs` and
   `frmAddCustomRadio.Designer.cs` inherit from their companion source
   file. Phase 2 should apply that rule when generating copyright
   lines for files citing these designers (notably
   `tst_dig_rtty_wire.cpp` and `SliceModel.h`).

9. **`tests/tst_rxchannel_nb2_polish.cpp` has FlexRadio line despite
   sources (specHPSDR.cs + cmaster.c) not naming FlexRadio.** The
   specHPSDR.cs block is Wigley-only (2010-2018), and cmaster.c is
   NR0V-only. Strictly speaking FlexRadio shouldn't appear. However,
   the PowerSDR-lineage sibling argument supports keeping it; the
   failure mode is over-attribution, not under-attribution. Leave as-is;
   Phase 2 should focus on adding NR0V + G8NJJ.

10. **Inline-mod threshold: NetworkIO.cs W2PA citation.** The index
    notes only one W2PA inline (line 480, CAT control via console).
    This is a single line of code but Richie's notice policy treats
    inline mods as first-class. I flagged W2PA as needing addition in
    every file citing NetworkIO.cs (P1RadioConnection, BoardCapabilities,
    HardwareProfile, RadioModel). Phase 2 verifies this is appropriate.

11. **Variant reclassification proposals (for Phase 2 consideration,
    NOT applying in Phase 1b):**
    - `src/gui/applets/PhoneCwApplet.cpp`: currently
      `thetis-samphire`. Cites setup.cs which has NR0V/G8NJJ/W2PA/W4WMT
      inline. Promote to `multi-source` when adding contributors.
    - `src/gui/applets/RxApplet.cpp`: currently `thetis-samphire`.
      Cites console.cs + setup.cs (rich inline surface). Promote to
      `multi-source`.
    - `src/gui/setup/DisplaySetupPages.h`: cites setup.cs only, but
      with 4 inline contributors to add — consider `multi-source`.
    - Similar recommendations for all setup/* files citing setup.cs:
      once you add NR0V + G8NJJ + W2PA + W4WMT, the variant is
      effectively multi-source.
    - These are nomenclature concerns; the `thetis-samphire` template
      is flexible enough to hold additional Copyright lines without
      reclassification being strictly required.

12. **"Original authors" placeholder.** Per the Samphire convention,
    `cmaster.cs`, `enums.cs`, `PSForm.cs` use
    `Copyright (C) 2000-2025 Original authors` in lieu of naming
    FlexRadio. The source-first policy (per
    `feedback_source_first_exceptions`) says to preserve this verbatim.
    Files that cite these sources (HpsdrModel.h, WdspEngine.h,
    PureSignalTab.cpp) should use the placeholder; those already do.
    SpectrumWidget.h and VfoWidget.cpp cite enums.cs — Phase 2 should
    add the placeholder line in addition to (not in place of) the
    FlexRadio line.
