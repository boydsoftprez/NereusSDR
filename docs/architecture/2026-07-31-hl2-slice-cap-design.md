# Hermes Lite 2 slice cap: raising 1 to 2 panadapters and 5 flags

**Status:** design approved 2026-07-31, implementation pending
**Base:** `feature/phase3f-sub-epic-a-foundation` (PR #293). The fields this
document changes do not exist on `main`.
**Amends:** [2026-05-26-phase3f-multi-pan-multi-slice-design.md](2026-05-26-phase3f-multi-pan-multi-slice-design.md) §2

---

## 1. Summary

`src/core/BoardCapabilities.cpp` caps the Hermes Lite 2 at one operator slice
and one user DDC:

```cpp
.maxSlices     = 1,   // Phase 3F: HL2 single-slice cap (1-ADC; DDCs reserved for firmware quirks)
.userDdcCount  = 1,   // Phase 3F Sub-Epic I: HL2 user DDCs = DDC0 only (design doc §2)
```

Both values are wrong, and the comment naming "firmware quirks" names no quirk.
mi0bot supports a second receiver on the HL2 and always has. This document
resolves the row to **2 user DDCs and 5 slices**, corrects the design-doc table
that produced the wrong values, and separately removes an unrelated
over-announcement of DDC count that costs every HL2 user roughly half their
link budget.

| Field | Was | Becomes | Authority |
| --- | --- | --- | --- |
| `userDdcCount` | 1 | **2** | mi0bot `console.cs:8425-8429 [v2.10.3.13-beta2]` |
| `maxSlices` | 1 | **5** | Phase 3F project ceiling, unchanged from every other SKU |

Applies to both `kHermesLite` and `kHermesLiteRxOnly`.

---

## 2. Why the value was wrong

### 2.1 mi0bot supports RX2 on the HL2

`HPSDRModel.HERMESLITE` is one of four case labels, `HERMES`, `HERMESLITE`,
`ANAN10` and `ANAN100`, that fall through to a single shared body in
`UpdateDDCs` (`console.cs:8408-8411 [v2.10.3.13-beta2]`); it does not stand
alone. Inside that shared body, `DDC1` is added in exactly two places, both
guarded by `rx2_enabled`:

```csharp
// mi0bot console.cs:8425-8429 [v2.10.3.13-beta2]   (!mox && !diversity)
if (rx2_enabled)
{
    DDCEnable += DDC1;
    Rate[1] = rx2_rate;
}
```

```csharp
// mi0bot console.cs:8453-8457 [v2.10.3.13-beta2]   (mox && !diversity && !PS)
if (rx2_enabled)
{
    DDCEnable += DDC1;
    Rate[1] = rx2_rate;
}
```

No arm of that case enables anything above `DDC1`. The `P1_rxcount = 4; nddc = 4;`
at `console.cs:8412-8413 [v2.10.3.13-beta2]` is the announced DDC count, not a
user-receiver count, and the two are independent (§4).

Two further pieces of mi0bot confirm the second receiver is fully realised
upstream rather than vestigial:

- `networkproto1.c:995-1005 [v2.10.3.13-beta2]`, `case 3: //RX2 VFO (DDC1)` in
  `WriteMainLoop_HL2`, falls through to `ddc_freq = prn->rx[1].frequency` for the
  HL2 because `nddc == 4`. DDC1 carries an independently tunable frequency.
- `networkproto1.c:544-559 [v2.10.3.13-beta2]`, `MetisReadThreadMainLoop_HL2`
  `case 4:`, routes `RxBuff[1]` into a second stream via
  `xrouter(0, 0, 2, spr, prn->RxBuff[1])`.

### 2.2 Where the 1 came from

Two commits on the Phase 3F branch set the value, both deriving it from the
design-doc table rather than from source:

| Commit | Date | Value set | Stated authority |
| --- | --- | --- | --- |
| `af39fc70` | 2026-05-26 | `maxSlices = 1` | "design §2 per-SKU capability table" |
| `0ac488e9` | 2026-07-24 | `userDdcCount = 1` | "design doc §2 ... cited there against Thetis `console.cs:8186-8538 [v2.10.3.15]`" |

**That cite is ramdor Thetis, and ramdor Thetis has no HL2 case at all.**
`UpdateDDCs` spans `console.cs:8195-8548 [v2.10.3.15]`. Its switch contains
five case groups, at lines 8220, 8305, 8387, 8461 and 8533, and no `default:`
arm. `HERMESLITE` is not among them. Across the whole
`Project Files/Source/` tree, ramdor's case-sensitive `HERMESLITE` (the
`HPSDRModel` enumerator) appears on five lines: `enums.cs:128`,
`clsHardwareSpecific.cs:353,354,393`, and `ChannelMaster/network.h:444`. Two
further lines, `enums.cs:397` and `ChannelMaster/network.h:422`, carry
`HermesLite = 6`, mixed case, a different symbol in the separate `HPSDRHW`
enum, not another sighting of the model. Seven lines in three files either
way. None of them is a case label in `UpdateDDCs`, so an HL2 still leaves
ramdor's switch with `nddc = 0`.

So the HL2 rows were derived from a source that is silent on the HL2, on a
project where `../mi0bot-Thetis/` is the designated authority for that SKU. This
is the same failure mode §2 already documents and corrects for the ANAN-G2E row,
which was "copied wholesale from the rows around it" until 2026-07-25.

No firmware quirk exists. The comment should never have implied one without
naming it.

---

## 3. Resolved values

### 3.1 Panadapters: `userDdcCount = 2`

A panadapter is a DDC window. mi0bot gives the HL2 operator DDC0 and DDC1, so
two. See §7 for why four is not on the table today.

### 3.2 Flags: `maxSlices = 5`

`maxSlices` and `userDdcCount` are separate axes, per `BoardCapabilities.h:326-328`:
several slices can share one DDC when their frequencies fall inside its window,
so `maxSlices` may legitimately exceed `userDdcCount`. The ANAN-G2E row already
does this at 5 over 4.

A second flag inside a window already being received is free. It costs no wire
bandwidth, no DDC and no enable bit: one more WDSP channel and one more
demodulator on I/Q that is already arriving. There is no reason to hold the HL2
below the ceiling that every other SKU gets.

Five is the Phase 3F project ceiling held across all SKUs per the maintainer
decision of 2026-07-25, not a hardware limit. This row does not change it.
A slice that falls outside both active windows with no free stream is refused
with an explanation by `SliceStreamAllocator::placeSlice`, which is existing
behaviour.

---

## 4. Bandwidth

### 4.1 How Protocol 1 spends the link

The ep6 stream is fixed-size 1032-byte datagrams. Sample capacity per datagram
falls as the announced DDC count rises, from
`networkproto1.c:527 [v2.10.3.13-beta2]`:

```c
spr = 504 / (6 * nddc + 2);   // samples per ddc per 512-byte subframe
```

Two subframes per datagram, so the datagram rate is `sampleRate / (2 * spr)`.
The count announced to the radio, not the count actually consumed, is what
drives the link load. At Ethernet framing of about 1098 bytes per datagram:

| Sample rate | nddc=1 | nddc=2 | nddc=4 |
| --- | --- | --- | --- |
| 48 kHz | 3.3 Mbit/s | 5.9 | 11.1 |
| 96 kHz | 6.7 | 11.7 | 22.2 |
| 192 kHz | 13.4 | 23.4 | **44.4** |
| 384 kHz | 26.8 | 46.8 | **88.8** |

### 4.2 NereusSDR currently announces 4, always

`P1CodecHl2::applyDdcAssignment` and `applyPureSignalDdcConfig` both set
`p1RxCount = 4` unconditionally. `P1RadioConnection::applyPsDdcConfig` copies
it into `m_activeRxCount`, and the composer that actually runs on a live HL2
connection, `P1CodecHl2::composeCcForBank` case 0, encodes it as
`C4 |= ((activeRxCount - 1) & 0x0F) << 3`. The static
`P1RadioConnection::composeCcBank0` helper implements the equivalent
`C4 = (nddc - 1) << 3` formula at `P1RadioConnection.cpp:400-401`, but
nothing in production calls it: every live HL2 connection reaches bank 0
through the codec instead. A live HL2 log on 2026-07-31 confirms the count:
`P1: applyPsDdcConfig nDdc=4 activeRx=4`.

A single-slice HL2 at 384 kHz therefore runs about 89 Mbit/s on a 100 Mbit PHY.
This is inherited from mi0bot, which announces 4 for the same case, and it is
the likely reason `hasBandwidthMonitor = true` on this row.

**Consequence for this change: the second panadapter costs nothing.** DDC1's
samples already arrive inside every datagram, are already parsed by
`parseEp6Frame`, and are already emitted as `iqDataReceived(1, ...)`. They are
discarded only because no slice is bound to them.

### 4.3 The monitor does not police this

`HermesLiteBandwidthMonitor` is a faithful port of MW0LGE's
`bandwidth_monitor.{c,h}` byte-rate telemetry plus a NereusSDR-original stall
detector that asserts after three consecutive ticks of ep6 silence while ep2
keeps sending. It has no budget model and reports after the fact. It cannot gate
slice creation, and building something that could is out of scope here.

### 4.4 Announce what is needed

PureSignal on the HL2 needs four DDCs: DDC0 and DDC1 as the sync pair, DDC2 as
feedback and DDC3 as TX monitor, per mi0bot `console.cs:8757-8762
[v2.10.3.13-beta2]` `GetDDC()` returning `rx1 = 0; rx2 = 1; psrx = 2; pstx = 3`
for HL2 P1 PS-MOX. PS engages on MOX.

Sizing the announced count purely to live slices would therefore flip it between
2 and 4 on **every key-down**, changing the ep6 slot layout mid-stream while
frames composed under the old layout are still in flight. That trades a real
race on every transmit for bandwidth saved during transmit, when nothing is
being listened to.

**Policy: announce 4 when PureSignal is enabled, 2 otherwise.** The count then
changes only when PureSignal is toggled, never on MOX.

The floor stays at 2 rather than 1 even for a single slice.
`P1RadioConnection.cpp:695` seeds `m_activeRxCount = 2` on every P1 connect, so
the HL2 is known to accept a two-slot layout at least briefly, until
`applyPsDdcConfig` raises it to 4. That is weak evidence, not proof of sustained
operation, which is why §9 row 9 exists. It is still more than exists for 1,
which we have never sent and which mi0bot never sends for this board. About
10 Mbit/s is not worth guessing on a wire field. Dropping to 1 remains available
later if row 9 passes and someone wants the extra headroom.

Resulting load at 192 kHz: 23.4 Mbit/s with PS off for either one or two
panadapters, against 44.4 today. At 384 kHz, about 47 against about 89.

This matters most for the operator's remote HL2, reached over a routed tunnel at
a measured 45 ms RTT, where the tunnel is a tighter constraint than the radio's
own PHY.

### 4.5 No per-slice rate cap

Protocol 1 carries a single global sample rate in bank 0 C1
(`SampleRateIn2Bits`, `networkproto1.c:620`). "Slice B at 96 kHz while slice A
runs 192 kHz" is not expressible on the wire, so a per-slice rate cap is not a
lever this protocol offers.

---

## 5. PureSignal interaction

With PureSignal enabled, key-down reclaims DDC0 and DDC1 as a sync pair
(`DDCEnable = DDC0; SyncEnable = DDC1`, mi0bot `console.cs:8469-8488
[v2.10.3.13-beta2]`). Slice B's DDC is taken for the duration of the
transmission.

**Operator-visible behaviour: with PS on, slice B goes quiet on key-down and
returns on key-up. Anything sharing DDC1's window goes with it. Slice A is
unaffected.**

This is upstream behaviour, not introduced here, and `P1CodecStandard` already
handles the equivalent case on Hermes-class boards, where its comment records
that slices C and D "are suppressed during PS-active or diversity-active because
those modes reclaim DDC0+DDC1 as a sync pair". The HL2 path must suppress stream
1 cleanly for the same reason rather than leaving it bound to a DDC that has
been repurposed.

---

## 6. Implementation

### 6.1 Commit 1: raise the cap

**`src/core/BoardCapabilities.cpp`**, rows `kHermesLite` and `kHermesLiteRxOnly`:
`maxSlices` to 5, `userDdcCount` to 2. Replace the "firmware quirks" comment
with the `console.cs:8425-8429 [v2.10.3.13-beta2]` cite and a pointer to this
document.

**`src/core/codec/P1CodecHl2.cpp`**, `applyDdcAssignment`: re-port the two
truncated `rx2_enabled` blocks, setting `streamDdc[1] = 1`,
`ddcEnable += DDC1bit` and `rate[1] = rx2Rate` in the two branches mi0bot has
them in, and only those two. Remove the early `if (!slices[0].live) return` at
`P1CodecHl2.cpp:764`, which strands a slice-B-only configuration.

The logic is ported from mi0bot `console.cs:8425-8429` and `:8453-8457`
`[v2.10.3.13-beta2]`, the HL2 case arm, and from nowhere else.
`P1CodecStandard` is referenced only for the C++ shape of a
`DdcAssignment`-returning codec method. It implements ramdor's HERMES-class arm,
which is a different SKU family, and it is not a behavioural authority for
anything in this file. Where the two disagree, mi0bot wins.

Note that `applyPureSignalDdcConfig` in the same file already has both
`rx2_enabled` blocks ported correctly (`P1CodecHl2.cpp:569-572` and `:593-596`).
Only `applyDdcAssignment` was truncated.

**`ReceiverManager`**: `setRx2Enabled(bool)` exists with zero callers anywhere in
`src/` or `tests/`; `m_rx2Enabled` is initialised false at
`ReceiverManager.cpp:151` and never changes. This is the single reason the rx2
branch never fires. Wire it, and `setRx2Rate`, from the model so they track
whether stream 1 is live.

**Design doc §2**: correct both HL2 rows to `DDC0-1` / `5`, and record that the
section's DDC-reservation cite points at a ramdor source with no HL2 arm.
Per the standing rule, the table that produced the value gets fixed in the same
commit as the value.

### 6.2 Commit 2: stop over-announcing (approved deviation)

> **Approved deviation from mi0bot.** Maintainer sign-off J.J. Boyd / KG4VCF,
> 2026-07-31: *"happy to keep it as an approved deviation as long as it works
> correctly."* Recorded here per the source-first exceptions rule. The
> correctness conditions in §6.2.2 are part of the approval, not commentary on
> it. If the bench in §9 does not clear them, this commit is reverted and the
> cap correction in §6.1 ships alone.

**`src/core/codec/P1CodecHl2.cpp`**, `applyPureSignalDdcConfig`: set
`cfg.p1RxCount` per §4.4, 4 when PureSignal is enabled and 2 otherwise, instead
of the current unconditional 4. `cfg.nDdc` stays 4 so the bank-2/3 frequency
override gate (`m_psNDdc`) is unaffected.

Kept as its own commit so the wire-format change can be reverted without losing
the second panadapter, which does not depend on it.

#### 6.2.1 There is no mi0bot basis for this

mi0bot sets `P1_rxcount = 4; nddc = 4;` at `console.cs:8412-8413
[v2.10.3.13-beta2]`, before any branch of the HL2 case. Unconditional, in every
MOX, diversity and PureSignal state, at every sample rate. `MetisReadThreadMainLoop_HL2`
does contain a `case 2:` in its `switch (nddc)` at `networkproto1.c:544-546`,
but the HL2 cannot reach it, because the model arm always sets 4. That case
serves ANAN10E and ANAN100B.

This is therefore a NereusSDR-original divergence on a wire field, not a port.
It is justified by the link-budget arithmetic in §4, not by upstream, and it is
labelled as such at the call site.

#### 6.2.2 Correctness conditions

**Key on the PureSignal master enable, not the per-transmission run state.**
`applyPureSignalDdcConfig` receives `psEnabled`, which is the operator's PS
toggle. Keying on it means the announced count changes only when PureSignal is
switched, never on MOX. Keying on `puresignal_run` instead would flip the count
on every key-down and is explicitly rejected (§4.4).

**Route the change through `restartStreamWithCount`.** The announced count sets
the ep6 slot layout for both sides (`slotBytes = 6 * numRx + 2`). Changing
`m_activeRxCount` while the stream runs leaves frames in flight that were
composed under the old layout, and nothing in the frame identifies which layout
produced it: the `0x7F 0x7F 0x7F` sync check at `parseEp6Frame` is
layout-independent, so a mismatch is silently misparsed rather than rejected.

`P1RadioConnection::restartStreamWithCount` (`P1RadioConnection.cpp:1002-1017`)
already performs the required stop, prime, start, prime cycle and is idempotent.
`applyPsDdcConfig` currently bypasses it, assigning `m_activeRxCount` directly at
`P1RadioConnection.cpp:1827` and only forcing a bank-0 flush. That must go
through `restartStreamWithCount` instead.

**This narrows the window; it does not close it.** `applyPsDdcConfig` calls
`restartStreamWithCount` at `P1RadioConnection.cpp:1835`, but does not set
`m_forceBank0Next = true` until 24 lines later, after `restartStreamWithCount`
has already returned. The pre-start priming burst inside
`restartStreamWithCount` therefore carries no forced bank 0: it just
continues wherever `m_ccRoundRobinIdx` already was, covering 12 round-robin
slots (`sendPrimingBurst(3)`'s six `sendCommandFrame()` calls, two banks each)
out of the HL2's 19 banks (0 through `maxBank()` = 18). Bank 0 is likely but
not guaranteed to land inside that pre-start burst before `sendMetisStart`
resumes the ep6 stream, so this narrows the mismatch window to under one
round-robin visit rather than closing it. Setting `m_forceBank0Next = true`
before the `restartStreamWithCount` call, instead of 24 lines after it, would
close the window properly. That reordering is not made here; it is recorded
as a follow-up.

This is a pre-existing latent defect, not one introduced here: today's HL2
connect sequence already steps the count from the 2 seeded at
`P1RadioConnection.cpp:695` to the 4 that `applyPsDdcConfig` writes, by bare
assignment, with the same mismatch window. It goes unnoticed because it happens
once during startup. This change would make it recur on every PureSignal toggle,
so it has to be fixed rather than inherited.

**Accepted cost.** A stop/start cycle drops audio briefly. PureSignal toggling is
a deliberate, infrequent operator action, so a short dropout there is acceptable;
a dropout on every transmission would not have been.

### 6.3 Note on the two count fields

Upstream `P1_rxcount` is write-only: assigned in `Protocol1DDCConfig`
(`netInterface.c:1297 [v2.10.3.13-beta2]`), declared at `network.h:502`, and
never read in the ChannelMaster tree. The variable that drives both the wire C4
field and the ep6 parse is `nddc`.

NereusSDR has these inverted: `cfg.p1RxCount` drives C4 and `parseEp6Frame`,
while `cfg.nDdc` feeds only the PureSignal frequency-override gate. Because both
are 4 on the HL2 today the wire is byte-identical, so this is not a live defect,
and the separation is convenient here. It is recorded so the next reader does
not assume the names match upstream semantics.

### 6.3a The announced count has two axes (bench follow-up, 2026-08-01)

The deviation in 6.2 makes the announced count vary, and a varying count turned
out to have more than one author. Two independent things need a say in it and
neither can see the other:

| Axis | Source | Wants |
| --- | --- | --- |
| DDC configuration | `applyPsDdcConfig` from `PsDdcConfig::p1RxCount` | 4 with PureSignal on, 2 off |
| Panadapters | `setActiveReceiverCount`, following the operator | 1 or 2 on the HL2 |

Before this change, three call sites wrote one field and the last writer won.
On a live HL2 with PureSignal on, removing the second panadapter dropped the
announcement to one receiver, and DDC2 and DDC3 left the ep6 frame entirely
until the next key-down happened to rewrite it:

```
14:07:26  DDCAssign fire: psEn=true ... rx2En=false
14:07:33  HL2 TX edge: mox=1 ... activeRx=1      PureSignal needs 4
14:07:33  applyPsDdcConfig ...     activeRx=4    repaired, by luck of timing
```

Worse, `setActiveReceiverCount` was a bare assignment with no stream restart, so
the radio and `parseEp6Frame` were left on different slot geometries with no way
to notice: the `7F 7F 7F` sync check is layout-independent.

The count is now derived, `max(codec axis, panadapter axis)`, with
`P1RadioConnection::announceRxCount` as its single writer.
`restartStreamWithCount` is private so no caller can set the count knowing only
one axis. Cheap to over-announce briefly, ruinous to under-announce: the count
*is* the ep6 slot layout, so a receiver the codec expects is simply absent from
the frame.

This was latent before the cap change, because a one-panadapter HL2 had a pan
axis that never moved. It is a precondition for 6.2 being safe, not an
independent nicety.

Side effect worth noting on a 2-DDC board: adding or removing a panadapter no
longer restarts the ep6 stream at all, because the codec axis already holds the
announcement at 2. That removes an audio interruption 6.5 would otherwise have
introduced.

### 6.4 Not in scope

`RadioModel.cpp:14313` forwards `DdcAssignment` to `P2RadioConnection` only,
under a comment at `RadioModel.cpp:14281` recording that "full P1 integration is
deferred to Phase 3F Sub-Epic C". The P1 wire path runs through
`applyPsDdcConfig` instead. This change works within that split rather than
closing it.

### 6.5 Fleet-wide effect of routing through restartStreamWithCount

`applyPsDdcConfig` is not HL2-specific. It is `P1RadioConnection`'s single
entry point for every Protocol 1 board's PS DDC config, so the
`restartStreamWithCount` fix in §6.2.2 changes connect-time wire behaviour
for every board whose codec reports a steady-state `p1RxCount` other than the
`2` seeded at `P1RadioConnection.cpp:695`, not only the HL2.

Boards that see the change, because their codec's live count differs from
the seed:

| Board (physical) | `HPSDRModel` value(s) | `p1RxCount` | Change on connect |
| --- | --- | --- | --- |
| HermesLite (HL2) | HERMESLITE | `psEnabled ? 4 : 2`, default 2 (`P1CodecHl2`) | None: stays at 2 (PureSignal is off by default) |
| HermesLiteRxOnly (HL2 RX-only kit) | none (has no `HPSDRModel` of its own; the model walk falls through to HERMES) | 4, always (`psDdcConfigHermesClass` via `P1CodecStandard`) | 2 to 4 |
| Hermes | HERMES, ANAN10, ANAN100 | 4, always (`psDdcConfigHermesClass`) | 2 to 4 |
| Angelia | ANAN100D | 5, always (`psDdcConfigG2Class`) | 2 to 5 |
| Orion | ANAN200D | 5, always (`psDdcConfigG2Class`) | 2 to 5 |

**HL2 row correction.** Task 6 changed `P1CodecHl2::applyPureSignalDdcConfig`
to return `psEnabled ? 4 : 2` instead of an unconditional 4, and `m_psEnabled`
defaults false (`ReceiverManager.cpp:146`). On a real HL2 connect the codec
therefore returns 2, matching the `m_activeRxCount = 2` seed at
`P1RadioConnection.cpp:695`, so the guard at `P1RadioConnection.cpp:1826`
(`m_activeRxCount != cfg.p1RxCount`) is false and no restart happens at
connect time at all. The connect-time restart this row used to describe is
exercised by no available hardware. The same function through the same
caller is exercised on live HL2 silicon by §9 row 11 (toggle PureSignal ten
times), just at PS-toggle time rather than connect time, once the operator
turns PureSignal on after connecting. A maintainer reading this table before
this correction would conclude the HL2 covers the connect path. It does not.

**HermesLiteRxOnly.** This row has no `HPSDRModel` of its own, so it never
reaches `P1CodecHl2::applyPureSignalDdcConfig`: the live codec is
`P1CodecStandard`, whose Hermes-class arm returns an unconditional 4
(`tests/tst_board_capabilities_phase3f.cpp`'s `documentedUnderExposure()`
records the same fallthrough as a deliberate, written-reason exception). It
mirrors the standard HermesLite capability row (`userDdcCount = 2`,
`maxSlices = 5`) but gets none of Task 6's PS-off saving, because that
saving is implemented only in `P1CodecHl2`.

Unaffected:

- HermesII (ANAN10E, ANAN100B): `psDdcConfigHermesIIClass` always returns 2,
  matching the seed. `restartStreamWithCount`'s idempotency guard makes this
  a no-op.
- Atlas (HPSDR): the default switch arm returns an unset `PsDdcConfig{}`
  (`p1RxCount = 0`), which the `cfg.p1RxCount > 0` guard in `applyPsDdcConfig`
  skips entirely.

**A board list needs one correction against an easy misreading of the
switch statement.** `P1CodecStandard::applyPureSignalDdcConfig` also names
`ORIONMKII`, `ANAN7000D`, `ANAN8000D`, `ANAN_G2`, `ANAN_G2_1K`, and
`ANVELINAPRO3` alongside Angelia and Orion in the same case group (Thetis's
own switch groups them together), and names `ANAN_G2E` alongside
Hermes/ANAN10/ANAN100. None of those seven models is on this risk surface:
each one's `HPSDRHW` board (`OrionMKII`, `Saturn`, or `HermesC10`) carries
`.protocol = ProtocolVersion::Protocol2` in `BoardCapabilities.cpp`, so a
real unit of any of those types connects through `P2RadioConnection`, never
`P1RadioConnection`. (`ANVELINAPRO3` gets its own codec class,
`P1CodecAnvelinaPro3`, at `selectCodec()`'s dispatch; it does not override
`applyPureSignalDdcConfig`, so it genuinely inherits this same 5 from the
`P1CodecStandard` base rather than the arm being dead code, but the board
mapping makes the point moot either way.) Those switch arms exist for
byte-for-byte parity with Thetis's C# `UpdateDDCs`, not because NereusSDR
ever dispatches a live Protocol 1 connection to them. In particular, a real
ANAN-G2 is a Protocol 2 radio and is not affected by this change at all.

**Unavoidable.** `applyPsDdcConfig` is shared code, not a per-board
dispatch. A board-specific carve-out, routing through
`restartStreamWithCount` only for the HL2, would leave the identical
misparse-on-count-change bug live for Hermes, Angelia, and Orion, which is
the exact bug this task exists to fix. Restricting the fix to the HL2 was
never a real option.

**Closest existing precedent.** The same stop, prime, start, prime shape
already ships for a different field: `restartStreamWithRate`
(`P1RadioConnection.cpp`, same file as `restartStreamWithCount`) performs
the identical cycle for live sample-rate changes, across every Protocol 1
board, since Task 1.6. It is the closest existing evidence that a
mid-session stop and restart on this radio family does not itself cause
problems. It is not proof for the receiver-count field specifically, and it
does not substitute for the bench rows below.

**Unverified.** Bench access on this branch is limited to a live HL2 (§9).
No Hermes, Angelia, or Orion unit has exercised this change. §9 rows 14 and
15 name what a non-HL2 Protocol 1 bench would need to confirm.

---

## 7. Open question: four panadapters

The HL2 streams four DDCs today. DDC2 and DDC3 are not user receivers because
mi0bot tunes DDC2 to the TX frequency (`networkproto1.c` `case 5`, non-Orion
path), which is the PureSignal path.

Using them as operator panadapters would be a NereusSDR-original divergence from
upstream bank composition, and it directly conflicts with PureSignal, which
needs exactly those two. The tradeoff would be 2 panadapters with PS on and 4
with PS off, a count that shifts under the operator when PS is toggled.
Bandwidth-wise four panadapters costs roughly what is already being paid today.

Resolving it needs, at minimum:

1. `softerhardware/Hermes-Lite2` gateware added to the reference repos and a row
   in [../attribution/GATEWARE-PROVENANCE.md](../attribution/GATEWARE-PROVENANCE.md),
   which currently has no HL2 entry. Per that document, adding a reference repo
   is a deliberate pinned act.
2. A bank-composition divergence from mi0bot for DDC2 and DDC3.
3. A policy for what the operator sees when PureSignal is switched on and the
   pan count has to fall.

Deferred. Not blocking this change.

---

## 7a. PureSignal will not converge at 48 kHz (bench, 2026-08-01)

Not caused by this change, and not a NereusSDR defect, but found while bench
verifying it and worth recording because the failure is silent and the cause is
several layers down.

**Symptom.** With PureSignal enabled on a live HL2, calcc parks in LCOLLECT
(`state=4`) forever. `corrApplied`, `calCount` and `feedbackLevel` all stay 0.
Nothing warns the operator.

**Cause.** LCOLLECT sorts transmit samples into `ints` bins by TX-reference
envelope and will not advance until EVERY bin holds `spi` samples
(`calcc.c:757-776 [WDSP v1.29]`, `ints=16`, `spi=256`). A two-tone at the Thetis
default 700 / 1900 Hz has envelope `2A|cos(2*pi*600*t)|`, which at 48 kHz
repeats every 40 samples and therefore takes only **20 distinct magnitudes**,
`|cos(pi*n/40)|`. Scaled by `hw_scale = 1/0.233` those 20 values do not cover
all 16 bins:

| n | \|cos(pi n/40)\| | bin |
| --- | --- | --- |
| 15 | 0.3827 | 6 |
| 16 | 0.3090 | 4 |

Bin 5 is stepped over, gets zero samples, and `full_ints` can never reach 16.
Every four seconds the reset at `calcc.c:779` wipes the partial progress and it
starts again.

Measured directly, one second of live samples per bin:

```
["1203 2406 2406 2406 2406 0 2405 2405 2405 2405 2405 2405 4810 2405 4810 9623"]
belowSpi=1/16  weakest=bin5@0  overScale=1203/48108
```

48108 / 2405 = 20.0 exactly, confirming the 20-magnitude prediction.

**Why 192 kHz works.** The envelope period becomes 160 samples, giving 80
distinct magnitudes, which covers every bin. Verified on 40 m at 192 kHz:
`corrApplied=1`, `calCount` climbing steadily, `feedbackLevel=152`,
`dogCount=0`.

**Why the HL2 is exposed and other boards are not.** Thetis forces
`Rate[0] = Rate[1] = ps_rate` (192000) for the PS pair on every other board.
mi0bot carves the HL2 out and uses `rx1_rate` instead
(`console.cs:8475-8484 [v2.10.3.13-beta2]`, "MI0BOT: HL2 can work at a high
sample rate"), and on Protocol 1 there is a single global sample rate anyway,
so the PS DDCs run at whatever the operator selected. mi0bot has the identical
failure at 48 kHz; this is shared with upstream, not introduced here.

**Not fixable by forcing a rate on P1**, because P1 has one global sample rate
for all DDCs: raising it for PureSignal would raise it for the whole receiver.
The options are to warn the operator when PureSignal is enabled at a rate whose
two-tone envelope cannot cover the bins (computable from
`sampleRate / (f2 - f1)`), or to document it. Left as a maintainer decision;
warning is the better of the two, since the present behaviour is silent.

## 8. Tests

`tests/tst_board_capabilities_phase3f.cpp` already contains
`user_ddc_count_never_exceeds_native_codec_capacity` (line 322), which asserts
each SKU's `userDdcCount` is **at most** what its codec can assign, via
`expectFits(HPSDRHW::HermesLite, assignableStreams(P1CodecHl2{}))`.

That one-sided check is exactly what let this through: 1 is happily less than 2.
It also means raising `userDdcCount` to 2 without fixing the codec fails an
existing test, which is the right coupling.

**Add the other side, table-wide.** Every row must expose *as many* user DDCs as
its codec can assign, unless it appears in an explicit exceptions list carrying a
written reason. Under-exposure becomes a failure that has to be justified in
code rather than a silent capability loss. The known Orion-class P1 gap at
`tst_board_capabilities_phase3f.cpp:364` already establishes the `QEXPECT_FAIL`
pattern for a documented exception.

Per-SKU assertions for the two HL2 rows update alongside.

### 8.1 Locking the §6.2 deviation

Because commit 2 is a deviation rather than a port, the tests are the only thing
holding it to its stated shape. Three are required:

1. **Wire bytes.** The live composer, `P1CodecHl2::composeCcForBank` case 0
   (reached from `P1RadioConnection::sendCommandFrame`, not from the static
   `composeCcBank0` helper, which has no production caller), emits
   `C4 = 0x08` for an HL2 with PureSignal off and `C4 = 0x18` with it on,
   that is `(2-1) << 3` and `(4-1) << 3`. A byte-level assertion in the same
   style as the existing P1 wire-lock tests.
2. **MOX does not move the count.** Drive `applyPureSignalDdcConfig` across MOX
   transitions with `psEnabled` fixed, and assert `p1RxCount` is unchanged. This
   is the guard on §4.4's central claim; if it ever regresses to keying on the
   run state, this fails.
3. **Count changes restart the stream.** Assert that a change in the announced
   count goes through the stop, prime, start, prime path rather than a bare
   assignment, so the §6.2.2 layout-mismatch window cannot silently return.

`P1CodecStandard` must be left alone by all three. It serves ramdor's
HERMES-class arm and is not in scope for an HL2 deviation.

---

## 9. Bench verification

Live HL2, reached over a routed link and added through Add Custom Radio, so not
discoverable by broadcast.

Rows 1 to 7 verify the cap correction (§6.1). Rows 8 to 13 are the correctness
gate on the approved deviation (§6.2). **If any of rows 8 to 13 fail, commit 2
is reverted and §6.1 ships alone.** Rows 14 and 15 verify the fleet-wide
connect-time effect documented in §6.5, on a non-HL2 Protocol 1 board (Hermes,
Angelia, or Orion). No such hardware exists on this branch; both rows are
blocked until it does. Rows 16 and 17 verify the two-axis count of §6.3a, and
run on the HL2.

| # | Check | Expected |
| --- | --- | --- |
| 1 | Add slice B | Accepted, second panadapter appears |
| 2 | Tune slice A and slice B independently | Both retune, no cross-talk |
| 3 | Audio on both slices | Independently audible |
| 4 | Add a third flag inside slice A's window | Accepted, no new DDC, no change in datagram rate |
| 5 | Add a flag outside both windows with both streams busy | Refused with an explanation, not silently dropped |
| 6 | Bandwidth monitor, 2 slices at 192 kHz | No throttle assertion |
| 7 | Enable PureSignal, transmit | PS locks; slice B suppressed cleanly on key-down and returns on key-up; slice A unaffected |
| 8 | Bank 0 C4 on the wire, PS off | `0x08` |
| 9 | Sustained RX at `nddc = 2`, PS off, 15 min | Zero `parseEp6Frame rejected frame` warnings; no clicks, dropouts or corrupted audio |
| 10 | Key up and down 20 times with PS off | C4 stays `0x08` throughout; count never moves on MOX |
| 11 | Toggle PureSignal on and off 10 times | C4 tracks `0x18` and `0x08`; each transition costs one brief dropout and recovers clean, with no lingering misparse |
| 12 | 384 kHz, PS off, 2 slices | Link load roughly halved against the pre-change baseline, near 47 Mbit/s rather than near 89 |
| 13 | Rows 9 to 12 over the 45 ms tunnel | Stable, no ep6 stalls |
| 14 | Connect a non-HL2 Protocol 1 board (Hermes, Angelia, or Orion). §6.5's fix now stops, primes, starts and primes on this connect where it previously did not (Hermes 2 to 4, Angelia/Orion 2 to 5) | Reaches a streaming Connected state; no rejected frames, clicks, dropouts or corrupted audio in the first minute. **Requires hardware nobody on this branch has.** |
| 15 | Same connect, watch the ep6 silence path across the extra stop/start | The radio-side turnaround does not run long enough to trip the 2000 ms ep6 silence watchdog (`kWatchdogSilenceMs`, `P1RadioConnection.h:471`) into a `LinkLost`/reconnect cycle. **Requires hardware nobody on this branch has.** |
| 16 | PureSignal on, remove the second panadapter, then key down | C4 stays `0x18` throughout. The defect §6.3a fixes showed as `0x00` (one receiver) between the removal and the next key-down, taking DDC2 and DDC3 out of the frame |
| 17 | PureSignal off, add and remove a panadapter | C4 stays `0x08` and no ep6 stop/start occurs, so no audio interruption: the codec axis already holds the announcement at 2 |

Row 9 is the one that matters most. `nddc = 2` is sent today only during the
first moments of a connection, so a sustained run is the first real evidence
that HL2 gateware streams a two-slot layout correctly for an extended period.
A misparse would show as corrupted audio rather than a rejected frame, because
the sync check is layout-independent (§6.2.2), so listen as well as read the log.

Rows 14 and 15 are not a re-run of row 9 on different hardware: they check
that the connect-time restart itself completes cleanly on boards other than
the HL2, which nothing on this branch has verified (§6.5).

---

## 10. Maintainer sign-off

This changes defaults affecting every HL2 user, so per CLAUDE.md's autonomous
agent boundaries it is implemented and demonstrated, never merged unilaterally.

| Item | Status |
| --- | --- |
| §6.1 cap correction to 2 DDCs / 5 slices | Design approved 2026-07-31. Merge approval pending bench rows 1 to 7. |
| §6.2 wire deviation from mi0bot | Approved as an explicit deviation 2026-07-31, conditional on bench rows 8 to 13. Reverted if they fail. |
| §7 four panadapters | Deferred, not approved, not in scope. |
