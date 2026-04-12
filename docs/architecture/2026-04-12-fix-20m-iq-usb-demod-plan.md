# Fix USB Demod Broken — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the Donald-Duck USB demod bug by eliminating the three-way inconsistency between `SliceModel` defaults, `RxChannel` cached state, and the hardcoded `WdspEngine::createRxChannel` init block.

**Architecture:** Two phases. Phase A instruments the DSP init / mode / filter paths with `qCInfo(lcDsp)` logs to capture ground-truth WDSP state and confirm the hypothesis against a live ANAN-G2. Phase B seeds the WDSP channel from the active `SliceModel` at create time, makes `RxChannel`'s cache self-consistent, removes the stale apply loop in `RadioModel::onWdspInitialized`, and adds an unconditional bandpass resync after every `SetRXAMode` call.

**Tech Stack:** C++20, Qt6, WDSP (C), CMake + Ninja. No unit-test infrastructure in this repo — verification is log-evidence + live-radio acoustic test per the reproduction protocol in the design doc.

**Design doc:** [`2026-04-12-fix-20m-iq-usb-demod-design.md`](2026-04-12-fix-20m-iq-usb-demod-design.md)

**Worktree:** `/Users/j.j.boyd/NereusSDR/.worktrees/fix-20m-iq` on branch `fix/20m-iq-upper-sideband`.

**Testing reality check:** This is an integration bug that cannot be exercised by a unit test without an ANAN-G2 radio and WDSP actively running. The "test" at each verification gate is a live repro with the user at the console. Plan is structured around that constraint; the user participates as the test harness in Tasks A6 and B8.

---

## File Structure

| File | Role | Phase |
|---|---|---|
| `src/core/WdspEngine.cpp` | Remove hardcoded mode/filter init; take initial state from caller | A (log), B (fix) |
| `src/core/WdspEngine.h` | New signature for `createRxChannel` | B |
| `src/core/RxChannel.cpp` | Add instrumentation; add unconditional bandpass resync after `SetRXAMode` | A, B |
| `src/core/RxChannel.h` | Remove hardcoded cached defaults; take initial mode/filter via constructor | B |
| `src/models/RadioModel.cpp` | Pass slice state to `createRxChannel`; remove redundant apply loop; add instrumentation to slice signal handlers | A, B |
| `docs/architecture/2026-04-12-fix-20m-iq-usb-demod-design.md` | Already committed — the spec | — |

---

## Phase A — Instrument and Prove

### Task A1: Instrument `WdspEngine::createRxChannel`

**Files:**
- Modify: `src/core/WdspEngine.cpp:255-267` (area around the `SetRXAMode` init block)

- [ ] **Step 1: Add a `qCInfo` log line immediately after the init block**

In `WdspEngine.cpp`, find the block ending with:

```cpp
    SetRXAAGCMode(channelId, static_cast<int>(AGCMode::Med));
    SetRXAAGCTop(channelId, 80.0);

    qCInfo(lcDsp) << "Created RX channel" << channelId
                   << "bufSize=" << inputBufferSize
                   << "rate=" << inputSampleRate;
#endif
```

Replace the existing `qCInfo` line with:

```cpp
    qCInfo(lcDsp) << "Created RX channel" << channelId
                   << "bufSize=" << inputBufferSize
                   << "rate=" << inputSampleRate
                   << "| init WDSP mode=LSB(0) bounds=(-2850,-150)"
                   << "| NOTE: hardcoded init — cache may diverge";
```

This makes it obvious in the log *what WDSP was actually initialized to*, not what anybody thinks it was initialized to.

- [ ] **Step 2: Build the worktree for the first time**

```bash
cd /Users/j.j.boyd/NereusSDR/.worktrees/fix-20m-iq
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

Expected: clean build, no warnings introduced by the change.

- [ ] **Step 3: Do not commit yet — continue to A2.**

---

### Task A2: Instrument `RxChannel::setMode`

**Files:**
- Modify: `src/core/RxChannel.cpp:24-39`

- [ ] **Step 1: Add guard-miss and guard-hit logging**

Replace the body of `setMode` with:

```cpp
void RxChannel::setMode(DSPMode mode)
{
    int val = static_cast<int>(mode);
    int oldVal = m_mode.load();
    if (val == oldVal) {
        qCInfo(lcDsp) << "RxChannel" << m_channelId
                       << "setMode GUARD-HIT — cache already"
                       << val << "— WDSP NOT updated";
        return;
    }

    m_mode.store(val);

#ifdef HAVE_WDSP
    // From Thetis wdsp-integration.md section 4.2
    SetRXAMode(m_channelId, val);
    qCInfo(lcDsp) << "RxChannel" << m_channelId
                   << "setMode MISS — old=" << oldVal
                   << "new=" << val << "— pushed to WDSP";
#endif

    emit modeChanged(mode);
}
```

- [ ] **Step 2: Rebuild**

```bash
cmake --build build -j
```

Expected: clean.

- [ ] **Step 3: Do not commit yet — continue.**

---

### Task A3: Instrument `RxChannel::setFilterFreqs`

**Files:**
- Modify: `src/core/RxChannel.cpp:45-59`

- [ ] **Step 1: Add guard-miss and guard-hit logging**

Replace the body of `setFilterFreqs` with:

```cpp
void RxChannel::setFilterFreqs(double lowHz, double highHz)
{
    if (m_filterLow == lowHz && m_filterHigh == highHz) {
        qCInfo(lcDsp) << "RxChannel" << m_channelId
                       << "setFilterFreqs GUARD-HIT — cache already"
                       << lowHz << highHz << "— WDSP NOT updated";
        return;
    }

    double oldLow = m_filterLow;
    double oldHigh = m_filterHigh;
    m_filterLow = lowHz;
    m_filterHigh = highHz;

#ifdef HAVE_WDSP
    SetRXABandpassFreqs(m_channelId, lowHz, highHz);
    qCInfo(lcDsp) << "RxChannel" << m_channelId
                   << "setFilterFreqs MISS — old=(" << oldLow << oldHigh
                   << ") new=(" << lowHz << highHz << ") — pushed to WDSP";
#endif

    emit filterChanged(lowHz, highHz);
}
```

- [ ] **Step 2: Rebuild**

```bash
cmake --build build -j
```

Expected: clean.

---

### Task A4: Instrument `RadioModel` slice signal handlers

**Files:**
- Modify: `src/models/RadioModel.cpp:346-361` (the `dspModeChanged` and `filterChanged` lambda handlers in `wireSliceSignals`)

- [ ] **Step 1: Add entry logs to both handlers**

Replace the two handlers with:

```cpp
    // Mode → WDSP
    connect(slice, &SliceModel::dspModeChanged, this, [this](DSPMode mode) {
        qCInfo(lcDsp) << "RadioModel: dspModeChanged handler FIRED with"
                       << static_cast<int>(mode);
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setMode(mode);
        }
        scheduleSettingsSave();
    });

    // Filter → WDSP
    connect(slice, &SliceModel::filterChanged, this, [this](int low, int high) {
        qCInfo(lcDsp) << "RadioModel: filterChanged handler FIRED with"
                       << low << high;
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setFilterFreqs(low, high);
        }
        scheduleSettingsSave();
    });
```

- [ ] **Step 2: Rebuild**

```bash
cmake --build build -j
```

Expected: clean.

---

### Task A5: Add a one-shot "resync probe" after WDSP init

**Files:**
- Modify: `src/models/RadioModel.cpp:158-176` (the `initializedChanged` lambda that creates the RX channel)

Rationale: this probe proves the stale-cache hypothesis directly. If simply re-pushing the current `SliceModel` state with bypassed guards fixes the audio, the bug is exactly what we think it is.

- [ ] **Step 1: Add the probe**

Inside the `initializedChanged` lambda, after the existing `rxCh->setActive(true);` line and before `m_audioEngine->start();`, add:

```cpp
        // DIAGNOSTIC: one-shot resync probe (remove in Phase B).
        // Forces mode/filter to match SliceModel by briefly perturbing then
        // restoring the RxChannel cache to bypass the equality guards.
        if (rxCh && m_activeSlice) {
            DSPMode wantMode = m_activeSlice->dspMode();
            int wantLow = m_activeSlice->filterLow();
            int wantHigh = m_activeSlice->filterHigh();
            qCInfo(lcDsp) << "RESYNC PROBE: forcing WDSP to slice state mode="
                           << static_cast<int>(wantMode)
                           << "bounds=(" << wantLow << wantHigh << ")";
            // Perturb cache to force a miss on the next set call
            rxCh->setMode(wantMode == DSPMode::USB ? DSPMode::LSB : DSPMode::USB);
            rxCh->setMode(wantMode);
            rxCh->setFilterFreqs(wantLow + 1, wantHigh + 1);
            rxCh->setFilterFreqs(wantLow, wantHigh);
            qCInfo(lcDsp) << "RESYNC PROBE: done";
        }
```

- [ ] **Step 2: Rebuild and relaunch**

```bash
cmake --build build -j
pkill -f NereusSDR.app 2>/dev/null
open build/NereusSDR.app
```

Expected: clean build, app launches.

---

### Task A6: Live repro against the ANAN-G2 (user-in-the-loop gate)

**Files:** none — this is a runtime observation task.

- [ ] **Step 1: User runs the repro protocol**

Ask the user to:

1. Stop if already running, relaunch the app
2. Connect to the ANAN-G2
3. Tune 20m USB, listen ~5 s
4. Switch to 40m LSB, listen ~5 s
5. Switch back to 20m USB, listen ~5 s
6. Toggle USB ↔ LSB ↔ USB on 20m without retuning
7. Paste the `lcDsp` log lines from `~/.config/NereusSDR/nereussdr.log` into the chat

- [ ] **Step 2: Diagnose the log**

Confirm exactly one of these:

- **(a)** Hypothesis confirmed — `createRxChannel` init logs `LSB/(-2850,-150)`, and at some point a `setFilterFreqs` or `setMode` guard-hit leaves WDSP in an incoherent state, AND the resync probe produces audible improvement when it runs
- **(b)** Hypothesis refuted — WDSP is coherent at all times but USB audio is still wrong → re-plan before touching code

Only if **(a)**: proceed to A7. If **(b)**: stop, return to brainstorming.

---

### Task A7: Commit the Phase A instrumentation

- [ ] **Step 1: Stage and commit, GPG-signed**

```bash
git add src/core/WdspEngine.cpp src/core/RxChannel.cpp src/models/RadioModel.cpp
git commit -S -m "$(cat <<'EOF'
diag(dsp): instrument RxChannel / WdspEngine init and mode/filter calls

Adds qCInfo(lcDsp) entries at every mode/filter set call site and at
WDSP channel creation to capture ground-truth state during the USB
demod bug hunt. Includes a one-shot resync probe in RadioModel that
forces WDSP to match SliceModel by perturbing the RxChannel cache,
used to prove the stale-cache hypothesis. Log lines and probe are
temporary and will be cleaned up in the Phase B fix commit.

Refs design: docs/architecture/2026-04-12-fix-20m-iq-usb-demod-design.md

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: signed commit with `gpg: Good signature from "JJ Boyd <kg4vcf@gmail.com>"`.

---

## Phase B — Root-Cause Fix

Do not start Phase B until Task A6 has produced log evidence that matches the hypothesis.

### Task B1: READ the Thetis source for channel create + mode change

**Files (read only):**
- `../Thetis/Project Files/Source/wdsp/cmaster.c` — channel create sequence
- `../Thetis/Project Files/Source/Console/console.cs` — mode-change handler (search for `SetRXAMode`, `SetRXABandpassFreqs`)
- `../Thetis/Project Files/Source/wdsp/RXA.c` — implementations of `SetRXAMode` and `SetRXABandpassFreqs`

- [ ] **Step 1: Locate and quote the Thetis channel-create sequence**

```bash
grep -n "SetRXAMode\|SetRXABandpassFreqs" ../../../Thetis/Project\ Files/Source/wdsp/cmaster.c
```

Note file + line numbers and paste the relevant 10-20 lines into the chat. Per CLAUDE.md READ → SHOW → TRANSLATE, nothing new is written until this is quoted.

- [ ] **Step 2: Locate and quote the mode-change handler in `console.cs`**

```bash
grep -n "SetRXAMode" "../../../Thetis/Project Files/Source/Console/console.cs" | head -30
```

Identify the function that handles mode switch (typically `radio_ModeChangeHandler` or similar) and quote the body into the chat.

- [ ] **Step 3: Confirm whether `SetRXAMode` internally resets bandpass**

Read the top of `SetRXAMode` in `RXA.c`:

```bash
grep -n "^void SetRXAMode\|^PORT void SetRXAMode" "../../../Thetis/Project Files/Source/wdsp/RXA.c"
```

Paste the function body. The answer to "does `SetRXAMode` reset bandpass" determines whether B4 (unconditional resync after mode change) is strictly required or merely defensive.

---

### Task B2: Remove hardcoded defaults from `RxChannel.h`

**Files:**
- Modify: `src/core/RxChannel.h:111, 120-121` (cached default values)
- Modify: `src/core/RxChannel.h` constructor declaration
- Modify: `src/core/RxChannel.cpp` constructor definition

- [ ] **Step 1: Update header**

In `src/core/RxChannel.h`, change the constructor signature:

```cpp
    RxChannel(int channelId, int bufferSize, int sampleRate,
              DSPMode initialMode,
              double initialFilterLow, double initialFilterHigh,
              QObject* parent = nullptr);
```

Change the atomic initializer and cached filter initializers to drop the hardcoded values:

```cpp
    // Atomic flags for lock-free audio thread reads.
    // Initial values are set by the constructor — no hardcoded defaults
    // here, because they have caused mode/filter incoherence bugs.
    std::atomic<int> m_mode;
    std::atomic<int> m_agcMode{static_cast<int>(AGCMode::Med)};
    std::atomic<bool> m_nb1Enabled{false};
    std::atomic<bool> m_nb2Enabled{false};
    std::atomic<bool> m_nrEnabled{false};
    std::atomic<bool> m_anfEnabled{false};
    std::atomic<bool> m_active{false};

    // Cached filter state — set by constructor, never defaulted.
    double m_filterLow;
    double m_filterHigh;
```

- [ ] **Step 2: Update constructor body**

In `src/core/RxChannel.cpp`, replace the constructor:

```cpp
RxChannel::RxChannel(int channelId, int bufferSize, int sampleRate,
                     DSPMode initialMode,
                     double initialFilterLow, double initialFilterHigh,
                     QObject* parent)
    : QObject(parent)
    , m_channelId(channelId)
    , m_bufferSize(bufferSize)
    , m_sampleRate(sampleRate)
    , m_mode(static_cast<int>(initialMode))
    , m_filterLow(initialFilterLow)
    , m_filterHigh(initialFilterHigh)
{
}
```

- [ ] **Step 3: Build — expect compile error at `WdspEngine` call site**

```bash
cmake --build build -j
```

Expected: failure, because `WdspEngine::createRxChannel` now calls `std::make_unique<RxChannel>(...)` with the old signature. Move on to B3.

---

### Task B3: Seed the WDSP channel from `SliceModel` in `createRxChannel`

**Files:**
- Modify: `src/core/WdspEngine.h` — signature of `createRxChannel`
- Modify: `src/core/WdspEngine.cpp:220-275`

- [ ] **Step 1: Update header**

In `WdspEngine.h`, change `createRxChannel` signature to accept the initial mode and filter. Example (preserve existing sample-rate parameters, insert mode/filter before `parent`):

```cpp
    RxChannel* createRxChannel(int channelId,
                               int inputBufferSize, int outputBufferSize,
                               int inputSampleRate, int dspSampleRate,
                               int outputSampleRate,
                               DSPMode initialMode,
                               double initialFilterLow,
                               double initialFilterHigh);
```

(Use `DSPMode` from `WdspTypes.h` — include it if not already.)

- [ ] **Step 2: Rewrite the init block to use the new parameters**

In `WdspEngine.cpp`, replace the hardcoded `SetRXAMode(LSB)` / `SetRXABandpassFreqs(-2850, -150)` lines (currently around 256-263) with:

```cpp
    // Seed WDSP with the active slice's actual (mode, bandpass) so the
    // channel is born in a coherent state. From Thetis cmaster.c:XX
    // (quoted in B1) — channel create sequences SetRXAMode first, then
    // SetRXABandpassFreqs.
    SetRXAMode(channelId, static_cast<int>(initialMode));
    SetRXABandpassFreqs(channelId, initialFilterLow, initialFilterHigh);
    SetRXAAGCMode(channelId, static_cast<int>(AGCMode::Med));
    SetRXAAGCTop(channelId, 80.0);

    qCInfo(lcDsp) << "Created RX channel" << channelId
                   << "bufSize=" << inputBufferSize
                   << "rate=" << inputSampleRate
                   << "| seeded mode=" << static_cast<int>(initialMode)
                   << "bounds=(" << initialFilterLow
                   << initialFilterHigh << ")";
```

Replace the `Thetis cmaster.c:XX` citation with the actual line number from B1 before committing.

- [ ] **Step 3: Update the `std::make_unique<RxChannel>` call**

Further down in `createRxChannel`, replace:

```cpp
    auto channel = std::make_unique<RxChannel>(channelId, inputBufferSize,
                                               inputSampleRate, this);
```

with:

```cpp
    auto channel = std::make_unique<RxChannel>(channelId, inputBufferSize,
                                               inputSampleRate,
                                               initialMode,
                                               initialFilterLow,
                                               initialFilterHigh,
                                               this);
```

- [ ] **Step 4: Build — expect compile error at `RadioModel` call site**

```bash
cmake --build build -j
```

Expected: failure at `RadioModel.cpp:158` because the call site still uses the old signature. Move on to B4.

---

### Task B4: Update `RadioModel::onWdspInitialized` to pass slice state

**Files:**
- Modify: `src/models/RadioModel.cpp:151-178` (the `initializedChanged` lambda)

- [ ] **Step 1: Pass slice mode/filter to `createRxChannel`**

Replace the current `createRxChannel` call:

```cpp
        RxChannel* rxCh = m_wdspEngine->createRxChannel(0, 1024, 4096, 768000, 48000, 48000);
```

with:

```cpp
        DSPMode initialMode = m_activeSlice ? m_activeSlice->dspMode() : DSPMode::USB;
        double initialLow = m_activeSlice ? m_activeSlice->filterLow() : 100.0;
        double initialHigh = m_activeSlice ? m_activeSlice->filterHigh() : 3000.0;
        RxChannel* rxCh = m_wdspEngine->createRxChannel(
            0, 1024, 4096, 768000, 48000, 48000,
            initialMode, initialLow, initialHigh);
```

- [ ] **Step 2: Remove the redundant apply loop**

Delete these lines:

```cpp
            // Apply slice state to WDSP channel (no longer hardcoded)
            if (m_activeSlice) {
                rxCh->setMode(m_activeSlice->dspMode());
                rxCh->setFilterFreqs(m_activeSlice->filterLow(),
                                     m_activeSlice->filterHigh());
                rxCh->setAgcMode(m_activeSlice->agcMode());
                rxCh->setAgcTop(m_activeSlice->rfGain());
            }
```

Replace with the AGC-only remnant (AGC wasn't part of the bug, still needs to be applied):

```cpp
            if (m_activeSlice) {
                rxCh->setAgcMode(m_activeSlice->agcMode());
                rxCh->setAgcTop(m_activeSlice->rfGain());
            }
```

- [ ] **Step 3: Remove the resync probe from A5**

Delete the `// DIAGNOSTIC: one-shot resync probe` block added in Task A5. It was diagnostic scaffolding for Phase A and must not ship.

- [ ] **Step 4: Build**

```bash
cmake --build build -j
```

Expected: clean build.

---

### Task B5: Add unconditional bandpass resync in `RxChannel::setMode`

**Files:**
- Modify: `src/core/RxChannel.cpp:24-39` (the `setMode` body, currently still carrying A2 diagnostics)

- [ ] **Step 1: Replace the `setMode` body**

Replace the entire `setMode` function with:

```cpp
void RxChannel::setMode(DSPMode mode)
{
    int val = static_cast<int>(mode);
    if (val == m_mode.load()) {
        return;
    }

    m_mode.store(val);

#ifdef HAVE_WDSP
    // From Thetis console.cs:XX — on mode change, Thetis sequences
    // SetRXAMode followed immediately by the current bandpass bounds.
    // We mirror that here unconditionally because SetRXAMode can perturb
    // WDSP internal bandpass state, and the equality guard on
    // setFilterFreqs is not trustworthy when mode and filter are stored
    // as two separate caches. One extra WDSP call per mode change is
    // negligible and eliminates a whole class of stale-state bugs.
    SetRXAMode(m_channelId, val);
    SetRXABandpassFreqs(m_channelId, m_filterLow, m_filterHigh);
#endif

    emit modeChanged(mode);
}
```

Replace `console.cs:XX` with the real line number from Task B1. This also drops the A2 diagnostic log lines from `setMode`.

- [ ] **Step 2: Remove the A3 diagnostic lines from `setFilterFreqs`**

Restore the original terse body:

```cpp
void RxChannel::setFilterFreqs(double lowHz, double highHz)
{
    if (m_filterLow == lowHz && m_filterHigh == highHz) {
        return;
    }

    m_filterLow = lowHz;
    m_filterHigh = highHz;

#ifdef HAVE_WDSP
    SetRXABandpassFreqs(m_channelId, lowHz, highHz);
#endif

    emit filterChanged(lowHz, highHz);
}
```

- [ ] **Step 3: Remove the A4 diagnostic log lines from `RadioModel`**

Restore the original terse `dspModeChanged` and `filterChanged` handler bodies (drop the `handler FIRED with` log lines added in Task A4; keep the handlers themselves).

- [ ] **Step 4: Build**

```bash
cmake --build build -j
```

Expected: clean.

---

### Task B6: Commit the Phase B fix

- [ ] **Step 1: Split into two commits — the seed-from-slice fix, and the resync fix**

First commit (B2+B3+B4 rolled together — they are a unit, the compile breaks until all three land):

```bash
git add src/core/WdspEngine.h src/core/WdspEngine.cpp \
        src/core/RxChannel.h src/core/RxChannel.cpp \
        src/models/RadioModel.cpp
git commit -S -m "$(cat <<'EOF'
fix(dsp): seed WDSP channel from SliceModel, make RxChannel cache self-consistent

Eliminates the three-way default-state disagreement between SliceModel,
RxChannel's cached m_mode/m_filterLow/m_filterHigh, and the hardcoded
LSB/-2850..-150 init block in WdspEngine::createRxChannel. The channel
is now born in the correct (mode, bandpass) state read from the active
slice, and RxChannel's cache mirrors WDSP from the first instant.

Removes the redundant setMode/setFilterFreqs apply loop in
RadioModel::onWdspInitialized (no longer needed) and the diagnostic
resync probe added in Phase A.

Fixes the "Donald Duck USB" symptom: on every USB band the channel was
left in a USB mode with an LSB-shaped bandpass (-2850..-150 Hz), which
WDSP demodulated as the image sideband. With the correct (USB, 100..3000)
bandpass seeded at create time the symptom is gone.

Refs: docs/architecture/2026-04-12-fix-20m-iq-usb-demod-design.md

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

Second commit (B5 — the defensive resync):

```bash
git add src/core/RxChannel.cpp
git commit -S -m "$(cat <<'EOF'
fix(dsp): resync bandpass after SetRXAMode to avoid stale filter state

Thetis's mode-change handler (console.cs:XX) sequences SetRXAMode
immediately followed by the current bandpass. Mirror that here so the
setFilterFreqs equality guard can never strand WDSP in a stale bandpass
if SetRXAMode perturbs internal state. One extra WDSP call per mode
change, no hot-path cost.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

Replace `console.cs:XX` in the second commit message with the line from B1 before running.

Expected: two signed commits, both builds clean on their own.

---

### Task B7: Relaunch and run the regression protocol

**Files:** none — runtime verification.

- [ ] **Step 1: Relaunch the app against the radio**

```bash
pkill -f NereusSDR.app 2>/dev/null
cmake --build build -j
open build/NereusSDR.app
```

- [ ] **Step 2: User runs the acceptance checklist from §4 of the design doc**

Ask the user to confirm each of the following:

1. Fresh launch → tune 20m USB → voice is intelligible and tuning feels normal
2. 40m LSB still sounds fine
3. Toggle USB ↔ LSB ↔ USB on 20m without retuning — audio sane in both
4. CW on a USB band (CWU) and an LSB band (CWL) — still sane
5. AM on 20m, FM on a known FM repeater — sane
6. WWV 10 MHz AM carrier still lands on frequency in the waterfall

Stop and return to planning if any of these fail.

---

### Task B8: Draft PR, show to user, then post

**Files:** none.

- [ ] **Step 1: Push the branch**

```bash
git push -u origin fix/20m-iq-upper-sideband
```

- [ ] **Step 2: Draft PR body and show it to the user for approval**

Draft a PR titled `fix(dsp): USB demod broken — WDSP bandpass stale at channel create` with a body that includes:

- Symptom summary (Donald Duck USB on every band, LSB fine, CW fine, waterfall placement correct)
- Root cause: three-way default-state disagreement, stale-cache guard hole
- Fix summary: seed channel from SliceModel, make cache self-consistent, resync bandpass after SetRXAMode
- Verification: link to the design doc and acceptance checklist
- Signed `JJ Boyd ~KG4VCF` + `Co-Authored with Claude Code`

Per the "review public posts" memory, paste the draft body into the chat and wait for approval before running `gh pr create`.

- [ ] **Step 3: Create the PR once approved**

```bash
gh pr create --title "fix(dsp): USB demod broken — WDSP bandpass stale at channel create" --body "<approved body>"
```

- [ ] **Step 4: Open the PR in the browser** (per "open links after posting" memory)

```bash
gh pr view --web
```

---

### Task B9: Worktree cleanup — deferred

**Files:** none.

- [ ] **Step 1: After merge, run `superpowers:finishing-a-development-branch`**

This removes the worktree `/Users/j.j.boyd/NereusSDR/.worktrees/fix-20m-iq` and cleans up the branch. Do not run until the PR is merged.

---

## Self-Review Summary

- **Spec coverage:** §2 root cause → B2/B3/B4 (seed from slice). §3 Fix 4 (belt-and-suspenders) → B5. §3 Fix 3 (remove apply loop) → B4 step 2. §4 verification gates → B7. §5 commit plan → A7 + B6. §6 out-of-scope follow-ups → not in plan (correct).
- **Placeholder scan:** Thetis file/line citations are marked `XX` in B3 and B5 and must be filled in during Task B1. No other placeholders.
- **Type consistency:** `DSPMode` / `double initialFilterLow` used consistently across B2/B3/B4. `RxChannel` constructor signature in B2 matches the call site rewrite in B3 matches the invocation in B4.
- **TDD waiver:** No unit tests exist for this subsystem and the fix depends on live WDSP + live radio I/Q. Verification is log-based in A6 and acoustic/live in B7. This is documented in the Testing Reality Check at the top of the plan.
