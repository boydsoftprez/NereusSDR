# How to Port a File from Thetis / mi0bot-Thetis / any GPL Upstream

When you (or an AI agent) port code from a GPL-licensed upstream into
NereusSDR, the file's license header is handled this way:

1. Locate the upstream source file that you ported from.
2. Copy the upstream file's **entire header block** — from the top of the
   file through the end of the copyright / GPL / dual-license region —
   **verbatim, character-for-character**. No substitutions, no
   reformatting, no address modernization, no contributor consolidation.
3. Prepend a short NereusSDR port-citation block stating the source
   file(s) followed by a Modification-History block stating the port
   date, human author, and AI tooling (if any). Format:

   ```cpp
   // =================================================================
   // <repo-relative path>  (NereusSDR)
   // =================================================================
   //
   // Ported from <upstream> source:
   //   <path>, original licence from <upstream> source is included below
   //
   // =================================================================
   // Modification history (NereusSDR):
   //   YYYY-MM-DD — Reimplemented in C++20/Qt6 for NereusSDR by <name>
   //                 (<callsign>), with AI-assisted transformation via
   //                 <tool> if applicable.
   // =================================================================
   ```

4. For multi-source files: stack each cited source's verbatim header
   block in citation order, blank comment line between.
5. For sources with no header (e.g. NetworkIO.cs): NereusSDR block +
   note `Upstream source has no top-of-file GPL header — project-level
   LICENSE applies`. No fabrication.
6. For upstream projects with no per-file headers (e.g. AetherSDR):
   reference the project's URL and primary author at NereusSDR block
   level; there is no verbatim block to copy.

Additionally: every ported function / block / inline constant that
carries an inline attribution marker in the upstream source
(`//-W2PA`, `//MW0LGE [x.y.z]`, `// added by G8NJJ for X`, etc.) MUST
preserve that marker at the corresponding position in the NereusSDR
port. When NereusSDR itself makes post-port modifications inside such a
marked region, add a NereusSDR marker in the same style using the
NereusSDR release version tag: `//-KG4VCF [v0.2.0] description`.

See `docs/attribution/THETIS-PROVENANCE.md` for the file mapping and
`docs/attribution/REMEDIATION-LOG.md` for historical cure entries.

This is a merge-blocking requirement, not a style preference. A PR that
ports without preserving the source's verbatim header will not be
merged.

## Mechanization

The script `scripts/rewrite-verbatim-headers.py` applies rules 1–5
automatically for all files listed in the PROVENANCE derivative tables.
`scripts/verify-thetis-headers.py` is the merge-gate check — it
confirms each file carries the required anchor markers (`Ported from`,
`Thetis`, `Copyright (C)`, `General Public License`, `Modification
history (NereusSDR)`).
