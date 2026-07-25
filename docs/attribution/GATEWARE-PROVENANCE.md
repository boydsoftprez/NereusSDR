# FPGA Gateware Provenance — NereusSDR hardware-fact inventory

This document catalogs every NereusSDR file that cites **FPGA gateware** as the
authority for a hardware fact, and every file (if any) that ports gateware
logic.

NereusSDR is distributed under GPLv3 (root `LICENSE`). The gateware referenced
here is also GPLv3 — fully compatible. See §License below.

## Why this table exists

Added 2026-07-25. Until then, every row in `src/core/BoardCapabilities.cpp`
cited only Thetis client code (`clsHardwareSpecific.cs`, `network.h`,
`enums.cs`). That made the capability table an inventory of *what Thetis asks
for*, presented as *what the hardware can do* — two different things, and the
gap between them is large:

| Value | Meaning | Source |
| --- | --- | --- |
| 14 | FPGA fabric capacity | `Orion.v:956` |
| 10 | bootloader 2 MB file-size cap | `Orion.v:957` |
| 8 | receivers in the shipped build | `Orion.v:958` (`localparam NR = 8`) |
| 8 @ 192 kHz / 2 @ 1536 kHz | link-budget limit | `Orion.v:632` (N1GP's test notes) |
| 5 | what Thetis requests | Thetis `console.cs` `UpdateDDCs` `nddc` |

The capability table recorded 5.

Two structural facts follow, and they matter more than any single number:

1. **`NR` is a compile-time Verilog constant that changes between firmware
   releases** — shipped as 2, 4, 7 and 8 at different times on the same board
   (`Orion.v` changelog). No static per-board integer can be correct for every
   firmware a user might be running.
2. **The usable count is rate-dependent**, because aggregate I/Q throughput is
   link-bound (P2 I/Q is 24-bit I+Q = 6 bytes per sample). Eight receivers at
   192 kHz and two at 1536 kHz are the same silicon.

## Kinds

* **`reference`** — the NereusSDR file cites a gateware *fact* (a count, an
  identification byte, a clock rate). Facts are not copyrightable; no verbatim
  header is required, and no derivative work is created. This is the normal kind.
* **`port`** — the NereusSDR file contains logic translated from gateware. Rare
  and usually a design smell, since gateware logic runs on the radio rather than
  in the client. Requires the full port protocol: verbatim upstream header,
  preserved author tags, and maintainer sign-off. **There are currently no
  `port` rows, and adding one should be questioned before it is accepted.**

## Upstream repositories

| Repo | Clone path | Pinned | License |
| --- | --- | --- | --- |
| [n1gp/Anvelina_PROIII](https://github.com/n1gp/Anvelina_PROIII) | `../n1gp-Anvelina_PROIII/` | `8e86a61` ("Version 2.2.14 Final", 2026-07-06) | GPLv3 |

Do not `git pull` the reference repos. Re-pinning is a deliberate act: bump the
SHA here and re-verify every citation below, because gateware constants move
between releases in a way client-code constants generally do not.

Author tags present in this gateware: `Yurij-eu2av`. If a gateware comment is
ever quoted verbatim, the inline-comment-preservation rule in `CLAUDE.md`
applies to it exactly as it does to Thetis tags.

## Table

| NereusSDR file | Upstream file | Lines | Kind | Fact established |
| --- | --- | --- | --- | --- |
| src/core/BoardCapabilities.h | Orion.v | 632; 956-958; 964 | reference | Documents the five-layer distinction behind `maxSlices` / `userDdcCount`: fabric capacity 14, bootloader cap 10, shipped `NR = 8`, link-bound 8@192k/2@1536k, Thetis policy 5. Records that the table's values are Thetis policy, that `NR` moves between firmware releases, and that the usable count is rate-dependent. No value changed — the Phase 3F ceiling deliberately stays at 5 pending bench verification. |

## Open questions

* **ANAN-G2E / HermesC10 has no public gateware.** A global GitHub code search
  for `HermesC10` returns only client code (Thetis forks, NereusSDR, one other
  client). "HermesC10" is a Thetis enum label, not a hardware identifier — the
  radio identifies by board-type byte `0x14`. "C10" is most likely **Cyclone
  10**, the FPGA part, which would make the G2E a Hermes-lineage design on newer
  silicon and would independently support Thetis's Hermes-class grouping (and
  therefore `P2CodecHermes` putting stream 0 on DDC0). **Unconfirmed — inferred
  from the part name, not read from a source.** Resolve by probing real hardware
  or by asking N1GP, who wrote the community P2 firmware.
* **`board_type` mismatch worth checking.** The repo is named `Anvelina_PROIII`,
  but its top-level file is `Orion.v` reporting `board_type = 8'h05` (Orion).
  NereusSDR treats AnvelinaPro3 as a distinct board. That may be correct if the
  SKU differentiates elsewhere in the discovery reply, but it has not been
  verified.
* **`n1gp/HPSDR-Orion-GiGE-16RX`** ships Orion gateware with gigabit and 16
  receivers, distributed as a Quartus archive (`.qar`) rather than loose Verilog.
  Not yet examined; relevant when revisiting the slice ceiling, since it
  demonstrates 16 RX on Orion-class hardware.

## License

NereusSDR: GPLv3 (root `LICENSE`).
n1gp/Anvelina_PROIII: GPLv3 (`LICENSE` in that repo).

Compatible. Citing gateware facts creates no additional obligation. Should a
`port` row ever be added, the gateware's own copyright notices must be preserved
verbatim in the NereusSDR file per the standard port protocol.
