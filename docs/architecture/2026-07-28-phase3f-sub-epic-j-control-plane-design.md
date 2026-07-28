# Phase 3F Sub-Epic J: per-slice control plane + anti-VOX mix

Design doc. Authored 2026-07-28 by J.J. Boyd (KG4VCF), with AI-assisted
drafting via Anthropic Claude Code.

Upstream reference: Thetis `v2.10.3.15` (`3759d096`).

---

## 1. What this is

Sub-Epic I built the multi-slice **data plane**: I/Q accumulation is keyed
by DDC stream and each slice is demodulated through its own WDSP channel.
That works.

What was never specified is the **control plane**. A handful of controls in
the GUI reach past `SliceModel` and call `wdspEngine()->rxChannel(0)`
directly, so they act on slice A no matter which slice the operator is
working. `MainWindow.cpp:896` records the gap as "the actual Phase 3F
multi-slice DSP epic" but no sub-epic was ever written for it.

This document specifies that sub-epic, plus the anti-VOX mixer port that
Sub-Epic I's plan deferred with "the real aamix port was always scoped to
arrive with multi-pan".

### Bench origin

ANAN-G2E, 2026-07-28. A second receiver added to the same pan appeared
"stuck to the exact dial freq as the first", with its flag "just following
the A flag".

Diagnosis established that the **model layer is not at fault**. Two slices
sharing one DDC window are placed by `SliceStreamAllocator::retuneSlice` as
`JoinedExisting` with `shiftOffsetHz = frequencyHz - centreHz`, so they hold
independent frequencies and independent shift oscillators. Pinned by
`tst_radio_model_slice_lifecycle::secondSliceOnAPanTunesIndependently`.

The defect is entirely in the controls above that layer.

---

## 2. What is already correct (do not re-implement)

This section exists because the first read of this problem badly
over-estimated it. Establish these before touching anything.

| Already correct | Evidence |
| --- | --- |
| Per-slice demodulation | Sub-Epic I data plane; each slice has its own WDSP channel |
| Per-slice frequency + shift | `SliceStreamAllocator::retuneSlice`, test above |
| Model to WDSP push | `RadioModel.cpp:8859` resolves `rxChannel(slice->sliceIndex())` |
| NR (all seven slots) | `VfoWidget.cpp:1542` writes `m_slice->setActiveNr(slot)` directly, with mutual exclusion |
| SNB, APF | `MainWindow.cpp:1214-1219` capture `slice` and write `SliceModel` |
| NB mode | `SliceModel::nbMode` exists and is pushed per slice |
| Stereo pan capability | `MasterMixer::setSliceGain(sliceId, gain, pan)` already accepts pan |

The per-slice pipeline is built and works. Every item in section 5 is a
control going **around** it, not a hole in it.

---

## 3. The rule

Two clauses, one mechanism.

1. **A control drawn on a flag targets that flag's slice.** This is the
   existing per-pan rule ("a button drawn on a pan targets THAT pan, never
   route through `activePanId()`") applied one level down.
2. **A control not attached to any slice targets the active slice**, meaning
   whichever flag the operator last clicked (`RadioModel::activeSlice()`).
   This covers menu items and the container S-meter.

**Mechanism: `src/gui/` never calls `wdspEngine()->rxChannel(...)`.** The GUI
writes a `SliceModel` property; `RadioModel` pushes it to
`rxChannel(slice->sliceIndex())`. That is already how frequency, mode,
filter, AGC, `activeNr` and `nbMode` travel. The rule is greppable, which is
what makes item J9 possible.

---

## 4. Per-slice versus shared

Settled from Thetis's own receiver struct rather than assumption. From
`Project Files/Source/ChannelMaster/cmaster.h:74-82 [v2.10.3.15]`:

```c
struct _rcvr
{
    int ch_outrate;                  // rate at rcvr channel output
    int ch_outsize;
    double* audio[cmMAXSubRcvr];     // audio buff, per subrx
    volatile long run_pan;           // run panadapter
    ANB panb;                        // noiseblanker, per receiver
    NOB pnob;                        // noiseblanker II, per receiver
} rcvr[cmMAXrcvr];
```

Audio is per sub-receiver. Both noise blankers are per **receiver**.

| Per-slice (RXA-resident, one per channel) | Shared per DDC (one per receiver) |
| --- | --- |
| NR, ANF, AGC, filters, squelch, APF, SNB | NB1 (`ANB panb`), NB2 (`NOB pnob`) |
| CTUN shift, audio gain, stereo pan | stream sample rate, panadapter run flag |

Two slices co-hosted on one DDC therefore **cannot** have independent noise
blankers. This is upstream topology, not a NereusSDR limitation, and the UI
must stop implying otherwise (J6).

---

## 5. Work items

### Actual defects

**J1. ANF acts on the wrong slice.**
`MainWindow.cpp:1206-1208` captures `this` and writes `rxChannel(0)`, while
its immediate neighbours `snbChanged` and `apfChanged` capture `slice` and
write `SliceModel`. ANF is the odd one out because it is the one setting with
no `SliceModel` property.

Add `SliceModel::anfEnabled` (bool, per-slice, per-band persisted alongside
`snbEnabled` / `apfEnabled`), push it from `RadioModel::wireSliceSignals` in
the same shape as `activeNrChanged`, and route the flag through it.

**J2. Dead NR handler.**
`MainWindow.cpp:1203-1205` connects `VfoWidget::nrChanged` to
`rxChannel(0)->setNrEnabled()`. Nothing emits `nrChanged`: the NR bank writes
`m_slice->setActiveNr(slot)` directly. Delete the handler and the unused
signal. Harmless today, but it is what made the first reading of this problem
conclude NR was broken.

### Controls with no slice in scope

**J3. DSP menu ANF.**
`MainWindow.cpp:5166`. Resolve `activeSlice()` at invocation. Reflect the
active slice's state when the menu opens, so the check state is not stale.

**J4. Container S-meter / MeterPoller.**
`MainWindow.cpp:3616` binds `MeterPoller` to `rxChannel(0)` once at WDSP init.
Re-bind on `activeSliceChanged`. Per-flag S-meters already follow their own
slice and are unaffected.

### Multi-slice correctness

**J5. CTUN and shift.**
`MainWindow.cpp:7290` and `:7302` write `rxChannel(0)` from slice A's
dedicated `wireSliceToSpectrum` path. Route through the owning slice so a
second slice on the same pan gets its own shift. `bindSliceToStream` already
computes the correct offset; these sites must stop overriding it with A's.

**J6. Shared NB, honestly presented.**
NB stays visible and usable on every flag, and toggling it on one visibly
updates the others co-hosted on the same DDC. Decided over greying the
control: a dead button forces a hover to discover why, whereas controls that
move together explain themselves. Slices on different DDCs are unaffected.

### New control surfaces

**J7. Stereo pan per flag.**
A pan slider on each flag, defaulting to centre, wired to
`MasterMixer::setSliceGain(sliceId, gain, pan)`. No automatic panning: adding
a second receiver must not change how the first one sounds. Thetis pans
sub-receivers for dual-watch; this gives the operator the same ability
without doing it behind their back. Persist per slice.

**J8. TCI per-slice volume and gain.**
`TciProtocol.cpp:339` and `:542` defer per-slice audio to "Phase 3F
multi-pan". Point them at the addressed slice.

### Prevention

**J9. Ban the shortcut.**
A check that fails when `src/gui/` calls `wdspEngine()->rxChannel(`. Every
legitimate use goes through `SliceModel`. Without this the pattern returns:
J1 exists because nothing stopped it.

### Anti-VOX

**J10. Anti-VOX hears every slice.**

Today `RxDspWorker.cpp:603` emits `antiVoxSampleReady` only from the stream
hosting **slice 0**, so the canceller hears receiver A alone. With a second
receiver audible the reference no longer matches what reaches the speakers.
The consequence is already documented in that file: `antivox_level` drives
`asig = avsig - antivox_gain * antivox_level` (`dexp.c:313-316
[v2.10.3.15]`) either too hard or not hard enough, producing a false VOX
trigger (unintended transmit) or a failure to cancel.

Thetis feeds **every** sub-receiver into a dedicated per-transmitter mixer:

```c
for (k = 0; k < pcm->cmXMTR; k++)
    xMixAudio (pcm->xmtr[k].pavoxmix, -1, chid (stream, j),
               pcm->rcvr[rx].audio[j]);   // send audio to anti-vox mixer(s)
```
`cmaster.c:371-372 [v2.10.3.15]`

That mixer is a second `aamix` instance (`cmaster.c:159-175`), created with:

- `ninputs = cmRCVR * cmSubRCVR`, `what = (1 << (cmRCVR * cmSubRCVR)) - 1`
  (mix everything), and **`active = 0`**: no stream is a barrier member, so it
  never waits for a producer. This maps onto `MasterMixer`'s existing
  `setSliceOpportunistic()`.
- **All four slew parameters `0.000`**, unlike the RX mixer's `0.010`
  (`cmaster.c:310-313`). No fade. This falls out for free, because our slew
  only arms on a membership change and anti-VOX makes none.
- Outbound is `SendAntiVOXData`.

So: a second `MasterMixer` instance, every slice registered opportunistic,
drained to `TxChannel::SendAntiVOXData`. Reuses the ring, sum and
opportunistic machinery added 2026-07-27; no new DSP structure.

**Cadence constraint, and it is load-bearing.** The current slice-0 gate is
not arbitrary. It guarantees exactly one block per `outSize / outRate`
seconds, which is the cadence DEXP was configured for, and it holds because
`inSize = 64 * rate / 48000` makes `inSize / inputRate == outSize / outRate`
per stream. Moving to a mixer relocates where cadence comes from. The
implementation must pin the drained block rate explicitly and test it,
or anti-VOX is fed at the wrong rate and reproduces the same
false-trigger failure by a different route.

---

## 6. Testing

Every item gets a failing test first.

- **J1**: ANF on slice B's flag sets B's `anfEnabled` and leaves A's alone.
- **J2**: no connection remains to `nrChanged`; grep-level assertion that the
  signal is gone.
- **J3, J4**: after `activeSliceChanged`, the menu action and `MeterPoller`
  address the new slice.
- **J5**: two slices on one pan retune to different frequencies and each
  channel receives its own shift, at the `RxChannel` boundary.
- **J6**: toggling NB on one co-hosted slice updates its co-host; slices on
  different DDCs do not follow.
- **J7**: pan value reaches `MasterMixer` per slice; default is centre;
  round-trips through persistence.
- **J8**: TCI volume for slice N addresses slice N.
- **J9**: the check fails on a deliberately added `rxChannel(0)` in
  `src/gui/`, and passes on the tree.
- **J10**: the anti-VOX mixer sums all producing slices; a slice that stops
  feeding never stalls it (opportunistic); **and the drained block rate
  matches `outSize / outRate` across stream widths.**

Full suite once at the end of the sub-epic, per standing preference. Targeted
tests during iteration.

---

## 7. Out of scope

These are the other open Phase 3F subsystems, each needing its own spec:

- **K. Diversity completion.** 21 of 25 Sub-Epic G tasks deferred. Needs a
  DivId allocator: WDSP `pdiv[]` is a 2-slot array keyed by an External
  Diversity id, **not** the RXA channel id, and `pdiv[id]` derefs unallocated
  state for `id >= 2` (crashes). `DiversityApplet` is also commented out at
  `MainWindow.cpp:4680`.
- **L. P1 wideband (3F-W).** No plan exists. Five `BoardCapabilities` rows
  carry `widebandAdcs = 0` with "P1 board, wideband mechanism differs".
  Requires research before it can be sized.
- **M. Advanced ANF tuning.** Taps / Delay / Gain / Leakage absent from
  `SliceModel` (`DspSetupPages.cpp:1348`). Small, and it pairs naturally with
  J1 since J1 introduces `anfEnabled`.

Also noted during the audit and not addressed here: **Sub-Epic B has no
implementation note** in the design doc, unlike A and C through G. The codec
work clearly shipped, so this is likely a bookkeeping miss, but the
completion record for B is absent.

---

## 8. References

### Thetis (`v2.10.3.15`, `3759d096`)

- `ChannelMaster/cmaster.h:74-82` - `_rcvr` struct: audio per sub-receiver,
  noise blankers per receiver
- `ChannelMaster/cmaster.c:159-175` - anti-VOX mixer creation, `active = 0`,
  zero slew
- `ChannelMaster/cmaster.c:371-372` - every sub-receiver fed to the anti-VOX
  mixer
- `ChannelMaster/cmaster.c:297-313` - RX mixer creation, `tslewup` /
  `tslewdown` `0.010`
- `wdsp/dexp.c:313-316` - `asig = avsig - antivox_gain * antivox_level`

### NereusSDR

- `docs/architecture/2026-07-24-phase3f-sub-epic-i-data-plane-plan.md` -
  data plane, and the deferrals this sub-epic picks up
- `docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md` -
  parent design; sub-epic ordering in section 14
