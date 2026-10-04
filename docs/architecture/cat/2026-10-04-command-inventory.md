# CAT command source inventory

This factual inventory accompanies the CAT design and implementation plan.
It records all 419 descriptors from Thetis v2.10.3.15 (`3759d096`): 349 active
(39 standard, 310 extended) and 70 inactive. It is not a claim of implemented
Nereus behavior.

DescriptorLine refers to `Project Files/Source/Console/CAT/CATStructs.xml`;
HandlerLine refers to `Project Files/Source/Console/CAT/CATCommands.cs`.
Blank handler lines occur for inactive descriptors without a method. Negative
widths denote an unavailable form, as used by the upstream parser; they are
not arbitrary-length payload permission. The maximum active request suffix
width in this source is 36 bytes.

Source: [ramdor/Thetis v2.10.3.15](https://github.com/ramdor/Thetis/tree/v2.10.3.15/Project%20Files/Source/Console/CAT).
The upstream XML has no per-file license header; its project-level license
applies. This document and CSV extract descriptor facts and source locations,
not executable handler logic. Any subsequent resource/code port must follow
`docs/attribution/HOW-TO-PORT.md` and register provenance in the same commit.

XML SHA-256: `f3eceafae604d8d5162b22d1b34b5ff87fa2617b0510eea7cb13c73a6a64c418`.

The companion [planning mapping matrix](2026-10-04-command-mapping.csv) supplies source ranges, explicit targets, scaling and read/set contracts against Core Controller candidate `27716f5d`. It classifies all419 rows:14 Faithful,163 Adapted,42 SourceInert,130 Unavailable and70 Inactive. These are reviewed planning outcomes, not implemented command counts. Task1 turns them into executable compatibility fixtures. No active descriptor
may be silently omitted, and the final report must distinguish unavailable
features from faithful or adapted implementations.

Audit correction: `ZZHW` at XML line 1958 contains `<active>true </active>`.
Thetis `CATParser.cs:481-482` uses `Convert.ToBoolean`, which accepts surrounding
whitespace ([Microsoft Boolean.Parse documentation](https://learn.microsoft.com/en-us/dotnet/api/system.boolean.parse?view=netframework-4.8.1)).
A literal string comparison in the first extraction incorrectly marked it inactive.
The corrected inventory matches source parser semantics: 349 active / 70 inactive.
The runtime catalogue test must cover this exact whitespace case.

Overlapping research ownership was reconciled explicitly: RX evidence owns buffer HA/HR/HU and quick-memory/transverter decisions; TX evidence owns TX-meter selector, MIDI and global reporting decisions. The combined matrix takes the finalized agreeing contracts and includes all other lead-owned mode/display rows. Negative-width forms remain disabled regardless of outcome.

Implementation-source correction: `CATParser.cs:965-967` dispatches `ZZMX` through `case"ZZMX":`. The earlier scanner required whitespace and incorrectly reported an upstream omission. This is an extraction error, not a source defect; the unavailable Nereus memory-store contract is unchanged.
