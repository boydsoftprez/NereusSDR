# Thetis CAT catalogue resource

`CATStructs.xml` is copied byte-for-byte from Thetis
`Project Files/Source/Console/CAT/CATStructs.xml:1-2954`
[v2.10.3.15 @3759d096]. Upstream source has no top-of-file GPL header —
project-level LICENSE applies (Thetis GPL-2.0-or-later; NereusSDR GPL-3.0-or-later).
Thetis also carries `LICENSE-DUAL-LICENSING`, Richard Samphire MW0LGE's
dual-licensing statement for his own contributions; NereusSDR mirrors it
verbatim at the repository root, and it covers any of his work in these files.

Modification history (NereusSDR): 2026-10-04 — J.J. Boyd, with Codex,
added the unmodified resource and generated `CommandContracts.json` from
`docs/architecture/cat/2026-10-04-command-mapping.csv`. The JSON contains
approved NereusSDR outcome/family decisions and suffix categories from
`CATParser.cs:550-565` [v2.10.3.15]; it contains no handler implementation.
2026-10-06 — J.J. Boyd, with Claude Code, added the `LICENSE-DUAL-LICENSING` note.
