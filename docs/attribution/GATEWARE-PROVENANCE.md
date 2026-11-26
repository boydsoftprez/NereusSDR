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
| [TAPR/OpenHPSDR-Firmware](https://github.com/TAPR/OpenHPSDR-Firmware) | `../TAPR-OpenHPSDR-Firmware/` | `e7c6584` | Verilog headers GPLv2-or-later; repository `LICENSE` GPLv3 |

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
| tests/tst_alex_per_adc_bpf_wire.cpp | Orion.v; High_Priority_CC.v | 2346-2348; 257, 261 | reference | Establishes that the two Alex control words are *alternative sources for one* 16-bit TX filter word, not a redundant pair: `Alex_upper` takes `Alex_Tx_data` (bytes 1428-1429) while keyed with valid one-hot ANT bits, else `Alex_data[47:32]` (bytes 1432-1433). Also fixes the byte-to-bit mapping (`Alex_Tx_data[n]` is register bit `n+16`) via the ANT nibble the gateware validates. Justifies replacing the word-equality assertion in `lpf_bits_are_untouched_by_every_bpf_decision` with a per-word one. No value changed. |
| src/core/P2RadioConnection.h | Tx1_IQ_fifo.vhd; Orion.v | 106; 1673, 1676 | reference | The P2 TX I/Q FIFO in the radio holds 4096 samples (`lpm_numwords => 4096`, 21.3 ms at 192 kHz), its writer ignores "full" (`.full(1'b0)`, so samples sent past it are lost in the radio), and it is zeroed unless PTT (`IQ_Tx_data = FPGA_PTT ? ... : 0`). Sets the send thread's 15 ms target lead (`TxIqPacer::kTargetLeadSamples = 2880`) below that capacity. R-IOS-13, R-R3-42, 2026-09-27. |
| src/core/RadioDiscovery.cpp | Orion.v; Ethernet/sdr_send.v | 634; 965-966; 171-172 | reference | The Anvelina Pro 3 puts the last number of its release in byte 23 of the Protocol 2 discovery reply: `Orion_version = 8'd22` and `beta_version = 8'd14` for release 2.2.14, with `beta_version` the last byte of the 24-byte reply `sdr_send.v` builds. `RadioDiscovery::parseP2Reply` reads that byte into `RadioInfo::firmwareBetaVersion` (the read itself follows Thetis `clsRadioDiscovery.cs:1184`), so the log and the windows can tell one 2.2.x release from another. 2026-10-12. |
| src/core/codec/P2CodecOrionMkII.cpp | Orion.v; Rx_specific_C&C.v | 632; 958; 1337-1363; 1921-1956; 218-221 | reference | On the Anvelina Pro 3 the gateware builds receivers 1 to NR-1 (`NR = 8`) as identical instances of one module, each with its own FIFO, and reads every receiver's ADC and rate from the same per-receiver bytes. Which idle receiver hosts a stream is therefore the client's choice: the third and fourth streams take DDC5 and DDC6, DDC4 last, after DDC4 arrived damaged on the bench. The gateware's notes say two receivers at 1536 kHz run without Seq errors. 2026-10-12. |
| tests/tst_codec_5_slice_assignment.cpp | Orion.v | 1939-1954 | reference | The same identical-instance fact, as the reason the Anvelina Pro 3 test may expect DDC5 and DDC6 for the third and fourth streams. 2026-10-12. |
| src/core/codec/P1CodecStandard.cpp | TAPR "Protocol 1/ANAN-10E & 100B/Hermes10E_v1.5.qar" Hermes.v, Tx_MAC.v; "previous ANAN-10E versions/ANAN_10E_V1.3.qar", "ANAN_10E_V1.4.qar" Hermes.v; "Protocol 1/Hermes/previous Hermes versions/Hermes_v2.9.qar" Hermes.v; "Protocol 1/ANAN-10 & 100/Hermes10_v3.2b.qar" Hermes.v; "Protocol 1/Hermes/previous Hermes versions/Release notes_v1.3 to v3.1.txt"; "Protocol 1/ANAN-10E & 100B/ANAN-10E release notes.txt" | 10E v1.5 Hermes.v 260, 1072, 1135, 1779; Tx_MAC.v 1058-1059; 10E v1.3 Hermes.v 253, 1065; 10E v1.4 Hermes.v 256, 1068; Hermes v2.9 Hermes.v 1240, 1319; v3.2b Hermes.v 358, 1305; Hermes notes 3-6, 36-38, 48-50; 10E notes 1-2 | reference | `usesTwoReceiverMap`. TAPR's ANAN-10E firmware (v1.3 to v1.5) reports its version as the firmware code (13 to 15), sends board byte 0x01 (Hermes), runs `NR = 2` receivers and feeds RX2 from the transmit DAC while the PureSignal bit (bank 11 C2 bit 6) is set and PTT is on. Hermes v1.3 has one receiver, v1.4 to v1.9 two, v2.0 four; Hermes v2.9 and the ANAN-10/100 v3.2b build run `NR = 4` with the DAC on RX4. So a HERMES / ANAN-10 / ANAN-100 model on codes 12 to 19 takes the two-receiver map. v1.2 has no archive; code 12 follows the pattern of the archived builds. The `.qar` archives are zlib-compressed Quartus archives; line numbers are in the unpacked files. 2026-11-26. |
| tests/tst_p1_ddc_layout_per_model.cpp; tests/tst_p1_puresignal_ddc_freq.cpp | (as the row above) | (as the row above) | reference | The same firmware facts, as the expected values for each board byte and firmware code. 2026-11-26. |

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
