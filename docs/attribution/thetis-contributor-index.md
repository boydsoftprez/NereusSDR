# Thetis Contributor Index

Per-source catalog of contributors named in Thetis source files, built
during the NereusSDR v0.2.0 compliance second-pass audit.

Source of record: `/Users/j.j.boyd/Thetis/Project Files/Source/...`
Scope: the 27 Thetis sources cited in `THETIS-PROVENANCE.md`.

## Conventions

- **Block** = contributor named in the file's top-level Copyright block
  (verbatim year range + name + callsign where the source gives one).
- **Inline** = contributor attributed via inline comments / modification
  markers in the body of the file. Years reflect what the file states;
  "n.d." means the file gives no date for that contribution.
- Callsign glossary: W2PA=Chris Codella, W5WC=Doug Wigley, MW0LGE=Richard
  Samphire, NR0V=Warren Pratt, VK6APH=Phil Harman, G8NJJ=Laurence Barker,
  KD5TFD=Bill Tracey, N1GP=Rick Koch, W4WMT=Bryan Rambo, W5SD=Scott McKee,
  WD5Y=Joe, M0YGG=Paul M, MI0BOT=Simon Brown,
  FlexRadio=FlexRadio Systems (PowerSDR origin).

## Index

### `Project Files/Source/ChannelMaster/cmaster.c`
- Block: Warren Pratt (NR0V) 2014-2019
- Inline: (none of the tracked callsigns cited)

### `Project Files/Source/ChannelMaster/netInterface.c`
- Block: Bill Tracey (KD5TFD) 2006-2007; Doug Wigley (W5WC) 2010-2020
- Inline: (none of the tracked callsigns cited)

### `Project Files/Source/ChannelMaster/network.c`
- Block: Doug Wigley (W5WC) 2015-2020
- Inline: MW0LGE_22b (Richard Samphire) n.d. - WSA init refactor

### `Project Files/Source/ChannelMaster/network.h`
- Block: Doug Wigley (W5WC) 2015-2020
- Inline: (none of the tracked callsigns cited)

### `Project Files/Source/ChannelMaster/networkproto1.c`
- Block: Doug Wigley (W5WC) 2020
- Inline: (none of the tracked callsigns cited)

### `Project Files/Source/Console/clsDiscoveredRadioPicker.cs`
- Block: Richard Samphire (MW0LGE) 2020-2026
- Inline: (none of the tracked callsigns cited)

### `Project Files/Source/Console/clsHardwareSpecific.cs`
- Block: Richard Samphire (MW0LGE) 2020-2025
- Inline: G8NJJ (Laurence Barker) n.d. - ANAN_G2_1K PA comment

### `Project Files/Source/Console/cmaster.cs`
- Block: "Original authors" 2000-2025; Richard Samphire (MW0LGE) 2020-2025
- Inline: (none of the tracked callsigns cited)

### `Project Files/Source/Console/console.cs`
- Block: FlexRadio Systems 2004-2009; Doug Wigley (W5WC) 2010-2020;
  Richard Samphire (MW0LGE) 2019-2026 "Modified heavily"
- Inline: Chris Codella (W2PA) May 2017-April 2019 - Behringer MIDI
  support, DB import, QSK for Protocol-2 v1.7; Laurence Barker (G8NJJ)
  n.d. - Andromeda/Aries/Saturn/ANAN-G2 support, Diversity CAT,
  RIT/XIT UI, filter popup, button bar; WD5Y (Joe) n.d. -
  RX2 mute/ForeColor ideas credited via MW0LGE [2.10.1.0]
- Note: credit line "Sizenko Alexander of Style-7 ... Digital-7 font"
  at line 7 (font credit, not code contribution)

### `Project Files/Source/Console/display.cs`
- Block: FlexRadio Systems 2004-2009; Doug Wigley (W5WC) 2010-2020;
  Phil Harman (VK6APH) 2013 "Waterfall AGC Modifications"; Richard
  Samphire (MW0LGE) 2020-2025 "Transitions to directX and continual
  modifications"
- Inline: MW0LGE tagged mods throughout ([2.10.1.0], [2.10.3], etc.)

### `Project Files/Source/Console/DiversityForm.cs`
- Block: FlexRadio Systems 2004-2009 (MW0LGE dual-licensing stanza
  appended, but no MW0LGE copyright line added)
- Inline: G8NJJ (Laurence Barker) 31/3/2018, 6/8/2019 - Diversity CAT
  access, gain setter for Andromeda

### `Project Files/Source/Console/dsp.cs`
- Block: Warren Pratt (NR0V) 2013-2017
- Inline: (none of the tracked callsigns cited)

### `Project Files/Source/Console/enums.cs`
- Block: "Original authors" 2000-2025; Richard Samphire (MW0LGE) 2020-2025
- Inline: G8NJJ (Laurence Barker) n.d. - ANAN_G2, ANAN_G2_1K, Saturn enum
  entries; MI0BOT (Simon Brown) n.d. - HermesLite enum entry

### `Project Files/Source/Console/frmAddCustomRadio.cs`
- Block: Richard Samphire (MW0LGE) 2020-2026
- Inline: (none of the tracked callsigns cited)

### `Project Files/Source/Console/frmAddCustomRadio.Designer.cs`
- Block: (none - WinForms designer file, no copyright header)
- Inline: (none - auto-generated designer code)

### `Project Files/Source/Console/frmMeterDisplay.cs`
- Block: Richard Samphire (MW0LGE) 2020-2025
- Inline: (none of the tracked callsigns cited)

### `Project Files/Source/Console/HPSDR/NetworkIO.cs`
- Block: (none at top - file starts with `using` directives);
  commented-out block further down (lines 279+, prefixed `//*`)
  preserves historical: Bill Tracey (KD5TFD) 2006; Doug Wigley (W5WC)
  2010-2020
- Inline: W2PA (Chris Codella) n.d. - "Adds capability for CAT control
  via console" (line 480)
- Note: Active copyright attribution is weak here; the historical KD5TFD
  + W5WC block is commented out. Current file has no live header.

### `Project Files/Source/Console/HPSDR/specHPSDR.cs`
- Block: Doug Wigley (W5WC) 2010-2018
- Inline: G8NJJ (Laurence Barker) n.d. - ANAN_G2_1K PA note

### `Project Files/Source/Console/MeterManager.cs`
- Block: Richard Samphire (MW0LGE) 2020-2026
- Inline: (none of the tracked callsigns cited)

### `Project Files/Source/Console/PSForm.cs`
- Block: "Original authors" 2000-2025; Richard Samphire (MW0LGE) 2020-2025
- Inline: (none of the tracked callsigns cited)

### `Project Files/Source/Console/radio.cs`
- Block: FlexRadio Systems 2004-2009; Doug Wigley (W5WC) 2010-2020;
  Richard Samphire (MW0LGE) 2019-2026
- Inline: (none of the tracked callsigns cited in sampled regions)

### `Project Files/Source/Console/rxa.cs`
- Block: (none - file has no copyright header; starts with `using`
  directives)
- Inline: (none of the tracked callsigns cited)
- Note: no header present in Thetis source. Any outbound NereusSDR
  derivative of this file should flag the absence.

### `Project Files/Source/Console/setup.cs`
- Block: FlexRadio Systems 2004-2009; Doug Wigley (W5WC) 2010-2020;
  Richard Samphire (MW0LGE) 2019-2026 "Continual modifications"
- Inline: Warren Pratt (NR0V) n.d. - resampler tuning guidance
  comments (lines ~233-268); Laurence Barker (G8NJJ) n.d. - Andromeda /
  Aries / Ganymede setup controls, Alex block-TX toggles, VAC rate
  logic, ANAN_G2 / G2_1K wiring; Chris Codella (W2PA) n.d. - MIDI VFO
  sensitivity, single-TX-profile export; Bryan Rambo (W4WMT) n.d. -
  [2.10.3.5] implements issue #87

### `Project Files/Source/Console/setup.designer.cs`
- Block: (none - WinForms designer file, no copyright header)
- Inline: (none - auto-generated designer code)

### `Project Files/Source/Console/ucMeter.cs`
- Block: Richard Samphire (MW0LGE) 2020-2025
- Inline: (none of the tracked callsigns cited)

### `Project Files/Source/Console/ucRadioList.cs`
- Block: Richard Samphire (MW0LGE) 2020-2026
- Inline: (none of the tracked callsigns cited)

### `Project Files/Source/Console/xvtr.cs`
- Block: FlexRadio Systems 2004-2009; Doug Wigley (W5WC) 2010-2013
- Inline: (none of the tracked callsigns cited)
- Note: MW0LGE dual-licensing stanza is appended but no MW0LGE
  copyright line has been added; xvtr.cs Wigley range ends 2013
  (earlier than most other files).

## Cross-file rollup

Files attributed to each contributor (block-level only):

- **FlexRadio Systems (2004-2009)**: console.cs, display.cs, DiversityForm.cs,
  radio.cs, setup.cs, xvtr.cs
- **Doug Wigley / W5WC (varying 2010-2020)**: console.cs, display.cs, radio.cs,
  setup.cs, xvtr.cs (ends 2013), netInterface.c, network.c, network.h,
  networkproto1.c, specHPSDR.cs (ends 2018), NetworkIO.cs (commented-out block)
- **Richard Samphire / MW0LGE (varying 2019-2026)**: console.cs, display.cs,
  radio.cs, setup.cs, clsDiscoveredRadioPicker.cs, clsHardwareSpecific.cs,
  cmaster.cs, enums.cs, frmAddCustomRadio.cs, frmMeterDisplay.cs,
  MeterManager.cs, PSForm.cs, ucMeter.cs, ucRadioList.cs
- **Warren Pratt / NR0V (2013-2019)**: cmaster.c (2014-2019), dsp.cs (2013-2017)
- **Phil Harman / VK6APH (2013)**: display.cs (Waterfall AGC)
- **Bill Tracey / KD5TFD (2006-2007)**: netInterface.c, NetworkIO.cs
  (commented-out)
- **"Original authors" (2000-2025)** (placeholder FlexRadio-era credit
  used by MW0LGE): cmaster.cs, enums.cs, PSForm.cs

Files with inline mods attributed to each contributor:

- **W2PA (Chris Codella)**: console.cs, setup.cs, NetworkIO.cs
- **G8NJJ (Laurence Barker)**: console.cs, setup.cs, DiversityForm.cs,
  enums.cs, clsHardwareSpecific.cs, specHPSDR.cs, network.c (spotted-by
  credit)
- **WD5Y (Joe)**: console.cs (via MW0LGE [2.10.1.0] credits)
- **MI0BOT (Simon Brown)**: enums.cs (HermesLite)
- **W4WMT (Bryan Rambo)**: setup.cs ([2.10.3.5] implements #87)
- **NR0V (Warren Pratt)**: setup.cs (resampler guidance comments)

## Files worth flagging for the next phase

- `NetworkIO.cs` - no live copyright header; historical KD5TFD + W5WC
  block present only as commented-out relic. Any NereusSDR derivative
  must decide whether to reinstate that block or cite the file's
  implicit FlexRadio + W5WC lineage from sibling files.
- `rxa.cs` - no header at all in Thetis source. If NereusSDR carries a
  derivative, attribution must be synthesized from sibling files
  (likely FlexRadio 2004-2009 + W5WC 2010-2020 + MW0LGE stanza).
- `xvtr.cs` - W5WC range ends 2013 (unusual); no MW0LGE copyright line
  despite MW0LGE dual-licensing stanza being appended.
- `console.cs` and `setup.cs` - richest inline attribution surface;
  expect the most line-item diffs in the per-file reconciliation step.
- `DiversityForm.cs` - FlexRadio-only block (no W5WC or MW0LGE line);
  MW0LGE dual-licensing stanza appears without matching copyright.
- Files with `Copyright (C) 2000-2025 Original authors` (cmaster.cs,
  enums.cs, PSForm.cs) use a placeholder phrasing rather than naming
  FlexRadio or W5WC; NereusSDR headers for these files should preserve
  the placeholder verbatim (per `feedback_source_first_exceptions`).
