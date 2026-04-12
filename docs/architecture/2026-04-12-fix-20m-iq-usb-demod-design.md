# Design: Fix USB Demod Broken On All Bands (20m I/Q Bug)

**Date:** 2026-04-12
**Branch:** `fix/20m-iq-upper-sideband`
**Status:** Design — awaiting user review
**Scope:** Bug fix only. Right-side panel wiring and Setup→Display→RX1 palette
work are explicitly out of scope and belong on separate branches.

---

## 1. Problem Statement

On NereusSDR `main` (at `a0c959c`), receiving a signal with mode = **USB** on
any band produces unintelligible "Donald Duck" audio and the operator cannot
tune in cleanly. **LSB** on 40m / 80m sounds normal. **CW** sounds normal —
but its narrow filter masks any small audio-frequency offset, so this is not
evidence of a working demod. AM / FM have not been tested yet; the report is
framed around SSB.

The user originally reported the symptom as "20m broken." Clarifying
conversation revealed the true partition: the fault is **sideband-specific,
not band-specific**. 20m was simply the first USB band tested.

### Key observations that constrain the hypothesis

| Observation | What it rules out |
|---|---|
| WWV 10 MHz AM carrier lands on frequency in the waterfall | Display frequency math, `SpectrumWidget` bin mapping, DDC NCO programming, `ReceiverManager` routing |
| 20m carrier sits exactly under the VFO cursor in the waterfall | `ReceiverManager::setReceiverFrequency` path, `P2RadioConnection` NCO programming, Alex HPF/LPF |
| 40m / 80m LSB sound normal | WDSP channel creation, `fexchange2`, audio engine, decimation, I/Q stream integrity |
| CW demod sounds normal | Gross I/Q corruption, sample-rate mismatch, whole-channel failure |
| "Can't ever quite get tuned in" on any USB band | Consistent with a fixed wrong-sign bandpass — no amount of retuning moves the passband to the right place |

The fault must therefore be in code that behaves differently for USB vs LSB
but is **not** in the frequency-to-bin / NCO path. That narrows it to the
WDSP configuration path: `SetRXAMode` / `SetRXABandpassFreqs` / the cached
state in `RxChannel`.

---

## 2. Root Cause Hypothesis

The WDSP channel is being left in an **incoherent `(mode, bandpass)` state**
at channel-create time: mode = USB paired with **negative-Hz** (LSB-shaped)
bandpass bounds. In that state WDSP demodulates the image sideband, producing
exactly the symptoms observed.

The underlying root cause is **three disagreeing "default filter states"**
distributed across the code, combined with guarded setters that early-out on
stale cache equality.

### The three disagreeing defaults

| Location | Default mode | Default filter bounds |
|---|---|---|
| `src/models/SliceModel.h:142-144` | **USB** | `{100, 3000}` |
| `src/core/RxChannel.h:111, 120-121` | **LSB** | `{150, 2850}` ← positive, USB-shaped, **internally inconsistent with its own mode** |
| `src/core/WdspEngine.cpp:260-261` (pushed to WDSP on channel create) | **LSB** | `{-2850, -150}` ← LSB-shaped |

The comment at `WdspEngine.cpp:256-259` claims the init block was written to
"match RxChannel's cached defaults so subsequent setMode/setFilterFreqs calls
work correctly" — but the values it pushes do **not** match `RxChannel`'s
cached defaults. The `setFilterFreqs` guard

```cpp
if (m_filterLow == lowHz && m_filterHigh == highHz) { return; }
```

therefore cannot be trusted: the cache was never an accurate mirror of
WDSP's real state to begin with.

### Why this produces the observed symptom

On fresh launch:

1. `WdspEngine::createRxChannel` pushes `(LSB, -2850, -150)` to WDSP, but
   leaves `RxChannel`'s cache at `(LSB, 150, 2850)` — a state that does not
   exist in any consistent world.
2. `RadioModel::onWdspInitialized` (around `RadioModel.cpp:162-164`) calls
   `rxCh->setMode(USB)` then `rxCh->setFilterFreqs(100, 3000)`.
3. `setMode(USB)` sees cache `LSB`, pushes `SetRXAMode(ch, USB)`. WDSP is now
   USB. Depending on WDSP internals, `SetRXAMode` may or may not reset the
   bandpass state; either way, the final state depends on what
   `setFilterFreqs` does next.
4. `setFilterFreqs(100, 3000)` compares against cache `(150, 2850)` — not
   equal, so the call goes through. In the happy path WDSP is now
   `(USB, 100, 3000)`. ✅

So the **cold-boot path works** — which matches what the user reports (the
radio isn't completely silent, it produces audio). But the system is
fragile: any path that restores state from persisted settings, any path that
touches the guards in a different order, or any WDSP-internal side effect
of `SetRXAMode` that we're not accounting for, can leave the channel stuck
in `(USB, negative-bounds)`. That is the state we need to prove out with
logs.

### Why "can't tune in"

With USB selected but the bandpass set to `(-2850, -150)`, WDSP demodulates a
slice of the *opposite* side of zero from where the user is listening. As
the user retunes, the signal moves but the passband moves with it — the
operator's reference for "zero Hz audio" is shifted from where it should be,
producing the characteristic Donald Duck pitch shift and the inability to
zero-beat.

### Why CW sounds fine

CW uses a ~400 Hz wide passband centered on `kCwPitch` (600 Hz). An offset
in bandpass placement for CW just means the tone is at a slightly different
audio frequency — which is indistinguishable from normal operator tuning.
You can't hear a "wrong sideband" on CW.

---

## 3. The Two-Phase Approach

The fix is split into **diagnose-first** and **fix-at-the-root**. Do not
write a fix until the log evidence is in hand and the hypothesis is
confirmed (or replaced).

### Phase A — Instrument and prove

Before touching any logic, capture ground-truth logs of what mode and
filter bounds WDSP is in at each critical moment.

**Instrumentation points** (all `qCInfo(lcDsp)`; may be demoted to
`qCDebug` later):

1. **`WdspEngine::createRxChannel`** — log `(channelId, initial mode,
   initial filter low, initial filter high)` right after the
   `SetRXAMode`/`SetRXABandpassFreqs` init block.
2. **`RxChannel::setMode`** — log `(channelId, old cache mode, new mode,
   guard-hit?)`.
3. **`RxChannel::setFilterFreqs`** — log `(channelId, old cache low/high,
   new low/high, guard-hit?)`.
4. **`RadioModel::wireSliceSignals`** handlers for `dspModeChanged` and
   `filterChanged` — log that the handler fired and with what values.
5. **One-shot resync probe** in `RadioModel`, fired ~1 s after
   `WdspEngine::initializedChanged(true)`: re-read `SliceModel` mode /
   filter and push them through `setMode` / `setFilterFreqs` with guards
   bypassed once, logging before and after. If this single probe silently
   fixes the audio, we have directly proved the stale-cache hole.

**Reproduction protocol** (user, against ANAN-G2):

1. Launch fresh with clean config
2. Connect to the radio
3. Tune 20m USB, listen ~5 s
4. Switch to LSB on 40m, listen ~5 s
5. Switch back to USB on 20m, listen ~5 s
6. Toggle mode USB ↔ LSB ↔ USB without retuning, note any change in audio
7. Paste the `lcDsp` lines from `~/.config/NereusSDR/nereussdr.log` back
   into the session

**Expected log signatures that would confirm the hypothesis:**

- `createRxChannel` logs `mode=LSB, bounds=(-2850, -150)`
- The `dspModeChanged(USB)` handler fires; `setMode` logs `old=LSB, new=USB,
  guard=miss`; WDSP mode flips to USB
- The corresponding `filterChanged(100, 3000)` handler either never fires,
  or `setFilterFreqs` logs `guard=hit` with `cache=(150, 2850)` while WDSP
  is really at `(-2850, -150)` — leaving WDSP bounds stale

**Decision gate.** No fix code until the logs are seen and the user and the
agent agree on what they say. If the log shows something *other* than the
hypothesis, re-plan before fixing.

### Phase B — Root-cause fix

Assuming Phase A confirms the hypothesis, the fix has four parts. All of
them follow the CLAUDE.md **READ → SHOW → TRANSLATE** rule: before writing
any of them, the agent will quote the relevant Thetis source
(`cmaster.c` channel-create block, `console.cs` mode-change sequence,
`RXA.c` `SetRXAMode` / `SetRXABandpassFreqs` implementations) and cite
file and line in each code comment that lands.

#### Fix B1 — Single source of truth at channel create

Change `WdspEngine::createRxChannel` to accept the initial
`(DSPMode, double filterLow, double filterHigh)` from the active
`SliceModel`, and use **those** values for the `SetRXAMode` /
`SetRXABandpassFreqs` init block. Delete the hardcoded `LSB / -2850..-150`
lines and their comment. The caller at `RadioModel.cpp:158` passes
`m_activeSlice->dspMode()`, `m_activeSlice->filterLow()`,
`m_activeSlice->filterHigh()`.

#### Fix B2 — Make `RxChannel`'s cache self-consistent and match WDSP

`RxChannel` no longer holds a hardcoded-default `m_mode` /
`m_filterLow` / `m_filterHigh`. The constructor takes the initial mode and
filter and stores them in the atomics and members. The "cache" is then
guaranteed to mirror WDSP's real state from the first instant the object
exists.

#### Fix B3 — Remove the redundant apply loop in `onWdspInitialized`

Since the channel is now created in its correct state, the explicit
`setMode` / `setFilterFreqs` call pair at `RadioModel.cpp:162-164` becomes
a no-op in the happy path and can be removed. Subsequent `setAgcMode` /
`setAgcTop` calls are unrelated and remain. After this fix, runtime mode
and filter updates happen normally via `SliceModel::dspModeChanged` /
`filterChanged` signals.

#### Fix B4 — Belt-and-suspenders bandpass resync on mode change

In `RxChannel::setMode`, after the `SetRXAMode` call, unconditionally
push `SetRXABandpassFreqs(m_channelId, m_filterLow, m_filterHigh)` using
the cached values, regardless of whether the filter cache "changed."

**Why:** `SetRXAMode` in WDSP may internally reset or re-initialize
bandpass state; the operator's next action after switching sideband
is almost always retuning, and a stale bandpass at that moment is
exactly the failure mode this bug represents. One extra WDSP call per
mode change is free. The Thetis mode-change sequence should be quoted
in the comment to justify the ordering.

### What this change does **not** touch

- `SliceModel::defaultFilterForMode` — the Thetis-cited preset table is
  correct as written.
- `SliceModel::setDspMode` signal-emission order — correct as written.
- The `SetRXAShiftFreq` / CTUN shift path — innocent per the waterfall
  evidence.
- Any frequency / DDC / `ReceiverManager` / `P2RadioConnection` code.
- Visual design, UX behavior, default values, or DSP constants outside
  the direct scope of this fix. Per CLAUDE.md these are not agent-
  autonomous changes.

---

## 4. Verification

**Verification gates**, in order:

1. **Build gate.** `cmake --build build -j` clean on macOS, no new warnings
   in touched files. App relaunched after each successful build.
2. **Phase A log evidence gate.** Instrumented build produces a log from
   the reproduction protocol. Confirm hypothesis or re-plan.
3. **Post-fix repro gate.** Re-run the reproduction protocol against the
   radio. Acceptance:
    - Fresh launch → tune 20m USB → intelligible voice, normal tuning feel
    - 40m LSB still fine
    - Toggle USB ↔ LSB ↔ USB without retuning → audio sane in both states
    - CW / AM / FM on both a USB and an LSB band → still sane
    - WWV 10 MHz AM carrier still on frequency in the waterfall
      (regression check on the display path)
4. **Phase B post-fix log gate.** The `createRxChannel` log now shows
   mode and bounds matching the active slice. No `setFilterFreqs` guard-hits
   on mode change that leave WDSP stale.
5. **Core RX-path sanity.** Discovery → connect → I/Q → WDSP → audio end
   to end, per CONTRIBUTING.md's "changes that break the core RX path"
   clause.

---

## 5. Commit Plan

Branch: `fix/20m-iq-upper-sideband`. All commits GPG-signed. Co-Authored-By
on each commit.

1. `diag(dsp): instrument RxChannel / WdspEngine init and mode/filter calls`
2. `fix(dsp): seed WDSP channel from SliceModel, make RxChannel cache self-consistent`
3. `fix(dsp): resync bandpass after SetRXAMode to avoid stale filter state`
4. (optional) `chore(dsp): demote dsp init log lines from qCInfo to qCDebug`

Each commit must build and run. Commit 2 is the commit that actually fixes
the audio; splitting it from commit 3 keeps blame history legible.

**PR.** `fix/20m-iq-upper-sideband` → `main`. Title:
`fix(dsp): USB demod broken — WDSP bandpass stale at channel create`. PR body
will describe the symptom, the three-way default disagreement, and the fix.
Draft PR body shown to the user for approval before posting. PR URL opened
in a browser after posting.

**Worktree cleanup.** After merge, via
`superpowers:finishing-a-development-branch`.

---

## 6. Out-of-Scope Follow-Ups

The original user request bundled three workstreams into one message. Only
the first is addressed here. The other two belong on their own branches:

- **Right-side panel wiring** — feature work, separate design, separate
  branch.
- **Setup → General → Display → RX1 display settings wiring** (motivated by
  the desire to match SmartSDR / AetherSDR waterfall palette range) —
  feature work, separate design, separate branch. This may require
  revisiting the palette slider ranges in `SpectrumWidget` as a follow-up.
