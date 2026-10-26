# Anvelina filter review fixes and DX open collector outputs: plan

Date: 2026-10-26. Branch `claude/anvelina-filter-oc-dc22e7`, from `origin/main` `36f4a279e`.
Execution: run with `crew` under `cost-aware-execution`. One implementer at a time on `opus`
(the mapping document task on `sonnet`), no per-task reviews, one whole-branch review at the end.
Crew workspace: `/Users/j.j.boyd/NereusSDR-crew/anvelina-filter-oc/` (outside the session worktree).

## Background

"Yuri's Thetis" is EU2AV's fork `eu2av/OpenHPSDR-Thetis-Enhanced` (clone `/Users/j.j.boyd/eu2av-Thetis-Enhanced`,
read at `origin/main` `567a7ec`). For every filter, Alex, antenna and open collector file it is identical to stock
Thetis v2.10.3.15 (`/Users/j.j.boyd/Thetis`, `3759d09`); see the workspace's `yuri-identity.md`. The line-cited
comparison is the workspace's `mapping-draft.md`; the open collector inventory is `oc-inventory.md`.

JJ's open collector spec (Yuri EU2AV, 2026-05-21, `/Users/j.j.boyd/Downloads/Specification_ Open_Collector_Anvelina_DX for Thetis.pdf`)
adds four outputs on the Anvelina: Protocol 2 high-priority byte 1397, bits 1-4 drive USEROUT7-10, bit 0 and bits
5-7 are sent as 0. Gateware `n1gp-Anvelina_PROIII` `8e86a61`: `High_Priority_CC.v:251` latches byte 1397 bits [4:0];
`Orion.v:920-923` drives USEROUT7..10 from bits 1..4 only while `run` is 1. Thetis carries the wire code
(`ChannelMaster/network.c:1016-1020`, `netInterface.c:410-417 SetOCExtraBits`, `networkproto1.c:668-674` bank 17)
but nothing calls `SetOCExtraBits` and there is no UI.

## Global Constraints

- Read `CLAUDE.md`, `CONTRIBUTING.md` and `docs/attribution/HOW-TO-PORT.md` before writing code.
- Source first: read the Thetis lines before writing; quote them in a `// From Thetis <file>:<line> [v2.10.3.15]`
  cite above each ported block. Keep constants exact as named `constexpr` with the cite. Gateware facts cite
  `// Gateware n1gp-Anvelina_PROIII <file>:<line> [@8e86a61]` (facts only, no Verilog logic ported).
  Primary reference (JJ, 2026-10-26): Yuri's Thetis, cited
  `// From Thetis-Enhanced (EU2AV) <file>:<line> [@567a7ec]` next to the stock cite. Read it with
  `git -C /Users/j.j.boyd/eu2av-Thetis-Enhanced show origin/main:'Project Files/Source/<path>'` (never pull). Files
  identical to stock (Penny.cs, Alex.cs, ChannelMaster/*) share line numbers; console.cs, setup.cs and
  setup.designer.cs do not, so look the line up in Yuri's file.
  The spec cites as `// Spec: Open_Collector_Anvelina_DX for Thetis (EU2AV, 2026-05-21), <section>`.
- A file that ports new upstream code gets that upstream file's header byte-for-byte (every Copyright line, the
  GPL block, the Samphire dual-licence block when present) under `// --- From <file> ---`, plus a
  "Modification history (NereusSDR)" line: `2026-10-26 - <what> (J.J. Boyd KG4VCF, AI-assisted: Claude Code)`.
  Files that already carry the header only get the history line. New ports add a row to
  `docs/attribution/THETIS-PROVENANCE.md`. Every `//` comment inside ported logic is copied verbatim, developer
  tags included; `scripts/verify-inline-tag-preservation.py` must pass.
- C++: no `goto`, no raw `new`/`delete` except Qt-parented widgets, `constexpr` not `#define`, braces on all control
  flow, `kPascalCase` constants, `m_camelCase` members, `qCWarning` for errors, `Q_OS_*` guards. Don't remove code
  you didn't add.
- Settings: `AppSettings`, never `QSettings`. Hardware-specific values live per MAC under `hardware/<mac>/...`,
  following `OcMatrix`'s existing key style (`hardware/<mac>/oc/...`). Booleans are the strings `"True"` / `"False"`.
- Remote parity: anything a local Setup control changes must reach a remote Core the same way the existing OC keys
  do (`src/core/session/StationServer.cpp`, `isTransmitHardwareKeyTakenOnAir` and its neighbours). No control is
  disabled on a remote connection without a task that closes the gap.
- UI rule: a control for hardware the connected radio lacks is hidden; a control that exists but cannot act right
  now is shown disabled with a plain reason tooltip.
- Builds and tests: build only through `/Users/j.j.boyd/NereusSDR-crew/anvelina-filter-oc/build.sh <targets>`
  (takes and releases the `lane-admit.sh` admission, `nice -n 20`, `-j 6`, one build folder `build/` in this
  worktree). Run single tests only:
  `QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_name$' --output-on-failure`. A test binary run by hand
  sets `HOME`, `CFFIXED_USER_HOME` and `NEREUS_TEST_HOME` to a scratch folder whose path contains `qttest`, and
  `NEREUS_WORKSPACE_PREVIEW=0`. Never run the whole suite.
- Never key a radio, never connect to a radio, never touch JJ's settings (`~/Library/Preferences/NereusSDR`,
  `~/.config/NereusSDR`), his STRIX or any bench device.
- Commits: GPG-signed (never `--no-gpg-sign`), no `Co-Authored-By` trailer, never push.
- Transmit and relay bits: tasks marked **TX** change what goes on the wire while keyed or which relays switch.
  Each states its wire effect in its commit message.

## Task 1: Apply the OC transmit pin actions (M10) **TX**

The Setup > Hardware Config > OC Outputs "TX Pin Action mapping" grid stores per-pin actions
(`OcMatrix::setPinAction`) but nothing applies them: `OcMatrix::maskFor` returns the raw matrix and
`OcMatrix::pinAction` is read only inside `OcMatrix`. Thetis masks the TX bits by the action.

Port from Thetis `Console/HPSDR/Penny.cs` `UpdateExtCtrl` (:183-186) and `adjustForTXAction` (:303-366), and
`adjustForRX` (:268-301). NereusSDR keeps one action per pin (no per-group array) and has no "consider PA" arrays;
treat consider-PA as false, which is the Thetis default (`Penny.cs:62-63` `TXPinPA[group, pin] = false;
RXPinPA[group, pin] = false;`), so `adjustForRX` returns the bits unchanged; port it anyway with the PA arrays
reduced to that default and a comment saying so.

Files: `src/core/OcMatrix.{h,cpp}`; the two callers that fill `ctx.ocByte` for transmit,
`src/core/P1RadioConnection.cpp` (~4426-4440) and `src/core/P2RadioConnection.cpp` (`composedOcByte`, ~4494);
tests `tests/tst_oc_matrix_tx_pin_action.cpp` (new) plus its `tests/CMakeLists.txt` entry.

Interfaces:
- `quint8 OcMatrix::maskFor(Band band, bool tx, bool tune, bool twoTone) const;` applies the action mask when
  `tx` is true. Keep the existing two-argument `maskFor(Band, bool)` as a call with `tune = false,
  twoTone = false` so other callers keep compiling.
- The connections pass the live tune and two-tone state. Find where NereusSDR knows TUN and two-tone are active
  (MoxController / TransmitModel); carry them to the connection the same way `m_mox` gets there (thread rules:
  cross-thread state is `std::atomic` or a queued signal).

Acceptance (unit, on `OcMatrix`):
- Pin 1 enabled in the 20 m TX matrix, action Mox: `maskFor(B20M, true, false, false) == 0x01`;
  `maskFor(B20M, true, true, false) == 0x00`; `maskFor(B20M, true, false, true) == 0x00`.
- Action Tune: tx only `0x00`, tune `0x01`. Action TwoTone: tx only `0x00`, twoTone `0x01`.
- Action MoxTune: tx `0x01`, tune `0x01`, twoTone `0x00`. MoxTwoTone: tx `0x01`, tune `0x00`, twoTone `0x01`.
  TuneTwoTone: tx `0x00`, tune `0x01`, twoTone `0x01`. MoxTuneTwoTone: all three `0x01`.
- Receive (`tx = false`) is unaffected by the action: RX matrix pin 3 on 40 m gives `0x04` whatever pin 3's action.
- Default actions (MoxTuneTwoTone) leave every existing caller's byte unchanged.

Verification: the new test plus `tst_oc_outputs_hf_tab` and `tst_oc_outputs_live_pins`. Model tier: opus.

## Task 2: Dropped (M12 already matches)

The review draft listed "Disable RX Bypass relay" as shown on Mk II models. It is already hidden there:
`src/core/SkuUiProfile.cpp:146-151` sets `hasRxBypassUi = false` for ANAN7000D and ANVELINAPRO3 (and the G2 rows),
and `AntennaAlexAntennaControlTab.cpp:939` applies it; the value is stored per MAC
(`AlexController.cpp:588`), so another radio's setting cannot reach these. Nothing to build; Task 6 records it as a
match.

## Task 3: Anvelina DX outputs, model and wire (M5)

Add the four DX outputs as their own state, separate from the band OC byte.

- Model: a small class `AnvelinaDxOutputs` (`src/core/AnvelinaDxOutputs.{h,cpp}`) owned by `RadioModel` beside
  `OcMatrix`, persisted per MAC: `hardware/<mac>/oc/dx/out7/enabled` .. `out10/enabled` (`"True"`/`"False"`,
  default `"False"`) and `hardware/<mac>/oc/dx/out7/label` .. `out10/label` (string, default empty). API:
  `bool outputOn(int out) const` (out 7..10), `void setOutputOn(int out, bool on)`, `void setAll(bool on)`,
  `QString label(int out) const`, `void setLabel(int out, const QString&)`, `quint8 wireByte() const`,
  `setMacAddress`, `load`, `save`, signal `changed()`.
- Wire byte: out7 -> bit 1, out8 -> bit 2, out9 -> bit 3, out10 -> bit 4; bit 0 and bits 5-7 always 0.
  Mask: `kDxWireMask = 0x1E` (bits 1-4). JJ's answer to question 2 (2026-10-26, "yes"): follow the spec and the
  gateware so all four outputs work. This is a deliberate deviation from Thetis's `& 0x0f` (network.c:1019,
  networkproto1.c:670), which never sends bit 4 although its own comment says `// [4:0]`. The code comment marks it
  `NereusSDR-divergence` and cites the spec (page 2, "Structure of Byte 1397") and `Orion.v:923`
  (`assign USEROUT10 = run ? Open_Collector_Anvelina_DX[4] : 1'b0;`); the PR body says so too.
- Codec: `CodecContext::ocExtrasByte` (new, default 0). `P2CodecOrionMkII` writes `buf[1397]` from it only when
  `ctx.model == HPSDRModel::ANVELINAPRO3`, ported from `network.c:1016-1020` (quote the block verbatim, including
  `//1397 - Open_Collector_Anvelina_DX` and `// [4:0]`). `P1CodecAnvelinaPro3` bank 17 sends `ctx.ocExtrasByte`
  instead of `ctx.ocByte` (`networkproto1.c:668-674`, `C1 = prn->oc_output_extras & 0x0f;`); the P1 mask is the same
  `kDxWireMask`.
- Both connections fill `ctx.ocExtrasByte` from the model. Sent every high-priority packet like byte 1401; the
  gateware already holds the pins low while `run` is 0, so nothing extra gates the wire.
- Remote: the `hardware/<mac>/oc/dx/...` keys reach a remote Core and apply at once, keyed or not (they are not
  transmit settings).

Files: the new class, `RadioModel.{h,cpp}`, `CodecContext.h`, `P2CodecOrionMkII.cpp`, `P1CodecAnvelinaPro3.{h,cpp}`,
`P1RadioConnection.cpp`, `P2RadioConnection.cpp`, `StationServer.cpp` (or wherever OC keys are applied on a Core),
`CMakeLists.txt`, tests `tests/tst_anvelina_dx_outputs.cpp` (new), `tests/tst_p1_codec_anvelina_pro3.cpp`, the P2
codec test that checks the high-priority packet.

Acceptance:
- Model: all off -> `wireByte() == 0x00`; out7 on -> `0x02`; out7 and out9 on -> `0x0A`; all on -> `0x1E`; persistence round-trips per MAC; a second MAC keeps its own values.
- P2: ANVELINAPRO3 with `ocExtrasByte = 0x0A` -> byte 1397 = `0x0A`; ANAN7000D with the same value -> `0x00`;
  byte 1401 unchanged by the extras.
- P1: bank 17 C1 equals `ocExtrasByte & 0x1E`, and no longer the band OC byte (band OC `0x05`, extras `0x00`
  -> C1 `0x00`).

Verification: the three tests. Model tier: opus.

## Task 4: Anvelina DX outputs, Setup UI

Built only after JJ signs off the mockup. The approved mockup is recorded here before dispatch (path and the
decisions it settled: placement, labels, All on / All off, disabled-when-stopped wording).

Files: `src/gui/setup/hardware/OcOutputsTab.{h,cpp}` and a test (`tests/tst_oc_outputs_dx_group.cpp`, new).
Acceptance and verification: written from the signed-off mockup. Model tier: opus.

## Task 5: "Gnd BPF2 on Tx" setting (M1) **TX**

JJ's answer to question 1 (2026-10-26, "match thetis"): default unticked, as on a fresh Thetis install (the designer gives `chkBPF2Gnd` no `Checked` line and `ForceAllEvents` pushes the unchecked state at startup, `setup.cs:579`, `:2867`). This is a behaviour change for JJ's radios: keyed Alex1 bit 8 goes clear until the box is ticked; the PR body says so. Thetis: `console.cs:29091` `if (bpf2_gnd) NetworkIO.SetBPF2Gnd(1);`, the
property at `console.cs:10903`, the Setup box `chkBPF2Gnd` "Gnd BPF2 on Tx" (`setup.designer.cs:5736-5745`, shown
on the Mk II models at `setup.cs:6208`), its handler `setup.cs:18214-18218`. NereusSDR hard-codes Alex1 bit 8 on
while keyed (`src/core/codec/P2CodecOrionMkII.cpp:600-615`). Add the box on Antenna / ALEX beside the other
"on TX" boxes, shown on the models where Thetis shows it, persisted per MAC (`"False"` default), carried in `CodecContext`. Acceptance: keyed Alex1 bit 8 follows the box; unkeyed it is clear either way. Model tier: opus.

## Task 6: The mapping document and provenance

Write `docs/architecture/2026-10-26-anvelina-filter-relay-oc-mapping.md` from the workspace's `mapping-draft.md`
and `oc-inventory.md`: every table row cites NereusSDR file:line at the final branch head, stock Thetis
v2.10.3.15 file:line, and Yuri's fork (`567a7ec`) file:line (identical files share line numbers; for
`console.cs`, `setup.cs` and `setup.designer.cs` map each stock line to Yuri's line, script in the workspace).
Each mismatch says fixed (task and commit) or flagged (why, and whose call). Add THETIS-PROVENANCE rows for any
new port that Tasks 1-5 did not add. Model tier: sonnet.

## Task 7: Input 2's band-pass follows Slice B when no slice uses input 2 (M2)

JJ's standing rule (2026-10-26): Yuri's Thetis (EU2AV `567a7ec`) is the primary reference, stock Thetis v2.10.3.15 the
cross-check; cite both. Yuri's `console.cs:15470` (stock `console.cs:15435-15444`), in `UpdateRX2DDSFreq`, calls
`setAlex2HPF(rx2_dds_freq_mhz)` for ANVELINAPRO3 (and the other Mk II models) with no `rx2_enabled` test, and
`RX2DDSFreq` follows VFO B whether RX2 is on or not (stock `console.cs:32906`, `:32921`). So Alex1's receive half
always carries the input-2 filter row for VFO B's frequency. NereusSDR maps Thetis RX2 / VFO B to Slice B (project
slice policy: Slice A = trx:0, Slice B = trx:1). Today, with no slice on input 2, NereusSDR mirrors Alex0's
selection into Alex1 (`src/core/codec/P2CodecOrionMkII.cpp:691-705`, `src/core/P2RadioConnection.cpp:4451-4454`,
`RadioModel.cpp:26643` returns -1 for an empty chain).

Change: when no slice is on input 2 and Slice B exists, Alex1's receive half takes the input-2 (Alex-2 / BPF2) row
for Slice B's frequency, through the same Alex-2 filter table and edges the input-2 path already uses. When Slice B
does not exist, keep today's mirror. A slice on input 2 keeps its current rule (unchanged). Keyed behaviour: read
`setAlex2HPF` callers in Yuri's tree (`console.cs:18760-19063`) and the keyed PureSignal bypass path before changing
anything keyed; if Thetis keeps following VFO B while keyed, match it, and say in the report what changed on the wire
keyed (receive half of Alex1 only; Alex1 bits 0-15).

Files: `RadioModel.cpp` (the input-2 chain), `P2RadioConnection.cpp`, `P2CodecOrionMkII.cpp` as needed, a test
(extend the existing Alex-2 / BPF2 selection test, or `tests/tst_anvelina_input2_follows_slice_b.cpp`, new).

Acceptance: Anvelina, Slice A 7.1 MHz on input 1, Slice B 14.2 MHz on input 1, nothing on input 2: Alex1 receive half
is the 11-22 MHz row (`0x0002`), not Alex0's `0x0010`. Slice B absent: Alex1 receive half equals today's mirror.
Slice on input 2: unchanged from today. Verification: the touched test. Model tier: opus.

## Flagged, not built (JJ's call or another owner)

- M4, deliberate difference (JJ, 2026-10-26, "yes keep the nereus way"): under click-tune Thetis (Yuri's and stock) selects the band-pass from the display centre; NereusSDR keeps selecting from each slice's own frequency, combined across the slices on one input, so the filter always passes the frequency being listened to. Task 6's table and the PR body record it as a deliberate deviation.
- M3: Alex1 low-pass while unkeyed holds TX, not RX; inert on this gateware (`Orion.v:2346-2348`). No change.
- M6, M8: Protocol 1 Mk II relay split (bank 0) and input-2 bits (bank 16). The Anvelina gateware is Protocol 2
  only; these matter for Mk II boards on Protocol 1 firmware. Separate work.
- M7: Protocol 1 bank 10 C3 bit 7 is set on receive on every P1 Alex board (a deliberate deskHPSDR-based choice);
  Thetis sets it only for Disable PA.
- M9: transverter enable (byte 1400 bit 0) not sent; belongs to the transverter work.
- M11: external Aries antenna clamp, already noted as deferred in `RadioModel.cpp`.
- The spec's macro `SET_ANVELINA_DX_OUT`: Thetis has no such command and NereusSDR's macro buttons are unbuilt.
