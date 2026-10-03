# TX Panadapter Attempt 2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild the TX panadapter source-switch with strict Thetis-parity SetAnalyzer params + a diagnostic probe, replacing attempt 1's CalcSpectrum-derived params with initAnalyzer-derived params, and bench whether this kills the -36 dBm DC pedestal.

**Architecture:** Re-apply the attempt 1 stash (`stash@{0}`) on the rebased `claude/tx-display` tree, resolve conflicts on six drifted files, then change four config knobs (`clip`, `window_type`, `frame_rate`, disp ID) to match Thetis `initAnalyzer` verbatim plus add a built-in diagnostic probe. All changes are throwaway commits on top of `c2e62e6` (the gen-state probe); reverted before any PR ships.

**Tech Stack:** C++20, Qt6, WDSP (vendored TAPR v1.29 + linux_port.h), CMake/Ninja, `qCInfo(lcDsp)` logging.

**Spec:** [docs/architecture/tx-display-attempt2-design.md](tx-display-attempt2-design.md). Read before starting.

**Worktree:** `/Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display`. Branch `claude/tx-display`. Top of branch before this plan: `489e6c5 docs(tx-display): attempt-2 strict Thetis-parity SetAnalyzer design`.

**Thetis source pin for new cites:** `[v2.10.3.13+501e3f51]` per session start.

**Testing strategy note:** Unit tests are deferred for this throwaway debug sprint per `feedback_minimize_test_invocations.md`. Verification is the runtime diagnostic probe (Task 3) and the bench cycle (Task 4). All commits are GPG-signed (`-S`) per `feedback_gpg_sign_commits.md`. The probe + the attempt 2 changes will be reverted before any PR ships.

---

## File Structure

| File | Status | Responsibility |
|---|---|---|
| `src/core/TxAnalyzer.h` | new (from stash) | Qt class declaration. Holds `kTxDispId = 5`, default `m_outputFps = 15`, default `m_fftSize = 4096`. |
| `src/core/TxAnalyzer.cpp` | new (from stash, then edited) | XCreateAnalyzer/SetAnalyzer/GetPixels glue. Param block sourced from Thetis `initAnalyzer`. Built-in diagnostic probe. |
| `src/core/WdspEngine.cpp` | modified (from stash) | After `createTxChannel`: `TXASetSipMode(ch, 1)` + `TXASetSipDisplay(ch, 5)`. |
| `src/core/wdsp_api.h` | modified (from stash) | 14 analyzer + siphon C declarations. |
| `src/gui/MainWindow.h` | modified (from stash) | `m_txAnalyzer` member + four `m_savedSpectrum*` members. |
| `src/gui/MainWindow.cpp` | modified (from stash) | TxAnalyzer construction at startup; MOX-aware source-switch lambda. |
| `src/gui/SpectrumWidget.h` | modified (from stash) | `resetWaterfallAgc()` getter. |
| `CMakeLists.txt` | modified (from stash) | Add `TxAnalyzer.cpp` to `CORE_SOURCES`. |

---

## Task 1: Re-apply the attempt 1 stash on the rebased tree

**Files:**
- Apply stash to working tree (no commit yet); resolve conflicts on six files.

**Conflict expectations:** All six modified files have moved on main during the 130-commit drift. The attempt 1 changes are additive (679 insertions, 0 deletions in stash diff per design doc §7.1), so resolution should be additive-only (no semantic merges).

- [ ] **Step 1.1: Confirm clean working tree before starting**

Run:
```bash
git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display status -sb
```

Expected output: `## claude/tx-display` followed by zero modified or staged files. If anything is dirty, stop and reconcile before continuing.

- [ ] **Step 1.2: Verify the stash is intact**

Run:
```bash
git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display stash show -p --include-untracked stash@{0} --stat
```

Expected: 8 files (`CMakeLists.txt`, `src/core/TxAnalyzer.cpp`, `src/core/TxAnalyzer.h`, `src/core/WdspEngine.cpp`, `src/core/wdsp_api.h`, `src/gui/MainWindow.cpp`, `src/gui/MainWindow.h`, `src/gui/SpectrumWidget.h`), 678 total insertions.

- [ ] **Step 1.3: Apply the stash (do NOT pop, keep stash intact)**

Run:
```bash
git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display stash apply stash@{0}
```

Expected: conflict messages on 5 modified files. `TxAnalyzer.{h,cpp}` are new files and apply cleanly.

If `git stash apply` reports no conflicts (drift resolved itself), skip to Step 1.9.

- [ ] **Step 1.4: Resolve the conflict in `CMakeLists.txt`**

Open `src/core/CMakeLists.txt` (or wherever `CORE_SOURCES` lives now). The stash adds one line:
```
    src/core/TxAnalyzer.cpp
```
between `FFTEngine.cpp` and `NoiseFloorEstimator.cpp`. The post-rebase tree may have other entries between those two. Place `TxAnalyzer.cpp` alphabetically (after `TwoToneController.cpp`) or per the existing alphabetical convention. Then:

```bash
git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display add CMakeLists.txt
```

- [ ] **Step 1.5: Resolve the conflict in `src/core/wdsp_api.h`**

The stash adds 73 lines: 14 extern-C declarations for `XCreateAnalyzer`, `DestroyAnalyzer`, `SetAnalyzer`, `GetPixels`, `Spectrum0`, `SetDisplayDetectorMode`, `SetDisplayAverageMode`, `SetDisplayNumAverage`, `SetDisplayAvBackmult`, `SetDisplaySampleRate`, `ResetPixelBuffers`, `TXASetSipMode`, `TXASetSipDisplay`, `TXASetSipPosition`. They land just before the closing `} // extern "C"`.

If the post-rebase `wdsp_api.h` has reorganized the closing block, place the new declarations in the same logical position (after `SetPSRxIdx` / `SetPSTxIdx`, before the closing brace).

```bash
git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display add src/core/wdsp_api.h
```

- [ ] **Step 1.6: Resolve the conflict in `src/core/WdspEngine.cpp`**

The stash adds a 27-line block at the end of `createTxChannel` (after `m_txChannels.emplace(channelId, std::move(wrapper));` and before `qCInfo(lcDsp) << "Created TX channel" << channelId;`). The block has the comment header explaining `TXASetSipMode(channelId, 1)` + `TXASetSipDisplay(channelId, /*txinid=*/2)`.

**Two changes during conflict resolution:**

1. The disp ID constant. The stash uses `2`. Change it to `5`:
   ```cpp
   TXASetSipDisplay(channelId, /*txinid=*/5);  // = TxAnalyzer::kTxDispId
   ```
2. Update the comment block to reflect disp 5 (search-and-replace `kTxDispId=2` → `kTxDispId=5` and update the `cmaster.inid(1, 0)` arithmetic note from `(1<<1)|0` to `cmRCVR + 0 = 5`).

Add the verbatim-Thetis cite stamp `[v2.10.3.13+501e3f51]` to any new `// From Thetis` lines (the stash already had this stamp; verify on conflict resolution).

```bash
git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display add src/core/WdspEngine.cpp
```

- [ ] **Step 1.7: Resolve the conflict in `src/gui/MainWindow.cpp`**

The stash adds two blocks:
1. `#include "core/TxAnalyzer.h"` near the existing `core/FFTEngine.h` include.
2. A 16-line block in `buildUI()` constructing `m_txAnalyzer = new TxAnalyzer(TxAnalyzer::kTxDispId, this);` followed by `m_txAnalyzer->setSampleRate(96000.0);` and `m_txAnalyzer->setOutputFps(30);`.
3. A 101-line block in the `MoxController` connect lambda performing the source-switch.

**Three changes during conflict resolution:**

1. The construction block: change `m_txAnalyzer->setOutputFps(30);` to `m_txAnalyzer->setOutputFps(15);` (Thetis `frame_rate` default per `specHPSDR.cs:335 [v2.10.3.13+501e3f51]`).
2. Verify all `// From Thetis` cites in the lambda block carry `[v2.10.3.13+501e3f51]` (stash already had this; refresh if drift caused stamp removal).
3. If `buildUI()` has been refactored or split on main, place the construction block in the equivalent location (after `m_fftEngine` is constructed, before the `m_radioModel->setSpectrumWidget` call).

```bash
git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display add src/gui/MainWindow.cpp
```

- [ ] **Step 1.8: Resolve conflicts in `src/gui/MainWindow.h` and `src/gui/SpectrumWidget.h`**

`MainWindow.h`: forward-declares `class TxAnalyzer;` and adds five private members (`m_txAnalyzer`, `m_savedSpectrumSampleRate`, `m_savedSpectrumCenterHz`, `m_savedSpectrumBandwidth`, `m_savedSpectrumDdcHz`). Place near other forward declarations and member declarations following the existing pattern.

`SpectrumWidget.h`: adds `void resetWaterfallAgc() { m_wfAgcPrimed = false; }` as a public inline method near `wfLowThreshold`. Member `m_wfAgcPrimed` already exists in the class.

```bash
git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display add src/gui/MainWindow.h src/gui/SpectrumWidget.h
```

- [ ] **Step 1.9: Verify `TxAnalyzer.{h,cpp}` are present and unchanged from stash**

Run:
```bash
git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display status -sb
```

Expected: `M` flags on the four conflict files staged (CMakeLists, wdsp_api.h, WdspEngine.cpp, MainWindow.{h,cpp}, SpectrumWidget.h), plus `A` (added) flags on `src/core/TxAnalyzer.cpp` and `src/core/TxAnalyzer.h`.

```bash
git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display add src/core/TxAnalyzer.h src/core/TxAnalyzer.cpp
```

- [ ] **Step 1.10: Build and verify clean compile**

Run:
```bash
cd /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display && cmake --build build --parallel --target NereusSDR 2>&1 | tail -10
```

Expected: `[N/N] Linking CXX executable NereusSDR.app/...` with zero errors. If the build fails, fix the conflict resolution issue and re-run.

- [ ] **Step 1.11: Commit (GPG-signed)**

Run:
```bash
git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display commit -S -m "$(cat <<'EOF'
debug(tx-display): re-apply attempt 1 stash on rebased tree

Re-applies stash@{0} (tx-display-attempt-1-shelved) on top of the gen-state
probe. Six files conflict-resolved against 130-commit drift; TxAnalyzer.{h,cpp}
land as new files. No behavioral change yet; Task 2 of the attempt 2 plan
will swap params to verbatim Thetis initAnalyzer.

Throwaway. Revert before any PR.
EOF
)"
```

Expected: commit succeeds with GPG signature.

---

## Task 2: Update params to strict Thetis initAnalyzer parity

**Files:**
- Modify: `src/core/TxAnalyzer.h` (default values).
- Modify: `src/core/TxAnalyzer.cpp` (cite block, applySetAnalyzer body).
- Modify: `src/core/WdspEngine.cpp` (TXASetSipDisplay arg already changed in Task 1.6, re-verify).

- [ ] **Step 2.1: Update `TxAnalyzer.h` defaults**

Open `src/core/TxAnalyzer.h`. Change the defaults:

```cpp
// Before (post-stash):
static constexpr int kTxDispId = 2;
// ... in private members ...
double m_sampleRate{96000.0};
int    m_outputFps{30};
int    m_fftSize{4096};
int    m_numPixels{1024};

// After (attempt 2):
// From Thetis cmaster.cs:411 [v2.10.3.13+501e3f51] — cmRCVR = 5; the TX
// panadapter siphon target is cmaster.inid(1, 0) = cmRCVR + 0 = 5.
static constexpr int kTxDispId = 5;
// ... in private members ...
double m_sampleRate{96000.0};
// From Thetis specHPSDR.cs:335 [v2.10.3.13+501e3f51] — frame_rate default = 15.
int    m_outputFps{15};
int    m_fftSize{4096};
int    m_numPixels{1024};
```

- [ ] **Step 2.2: Replace the cite block in `TxAnalyzer.cpp` header**

Find the `// Implementation notes` block at the top of `src/core/TxAnalyzer.cpp` and replace its body. Change:

```
// The XCreateAnalyzer / SetAnalyzer parameter values come from Thetis's
// CalcSpectrum path at specHPSDR.cs:738-806 [v2.10.3.13], adapted for the
// TX siphon source.  ...
```

to:

```
// The XCreateAnalyzer / SetAnalyzer parameter values come from Thetis's
// initAnalyzer path at specHPSDR.cs:504-650 [v2.10.3.13+501e3f51] — the
// PANAFALL/PANADAPTER analyzer setup.  attempt 1 mistakenly sourced from
// CalcSpectrum (specHPSDR.cs:738-806), which is the SPECTRUM/HISTOGRAM/
// SPECTRASCOPE path that PANAFALL never reaches per console.cs:8015-8020 +
// :8098-8108 [v2.10.3.13+501e3f51].  See
// docs/architecture/tx-display-attempt2-design.md §3.1 for the param
// deltas.
```

Update the modification history block at the bottom to add a 2026-05-10 entry: "Strict Thetis-parity attempt 2 by J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code."

- [ ] **Step 2.3: Replace `applySetAnalyzer()` body in `TxAnalyzer.cpp`**

Replace the entire `applySetAnalyzer()` function with the design-doc §4.1 version:

```cpp
#ifdef HAVE_WDSP
void TxAnalyzer::applySetAnalyzer()
{
    // From Thetis specHPSDR.cs:529 + :534-643 [v2.10.3.13+501e3f51] —
    // initAnalyzer case 1 (complex FFT) + the SetAnalyzer call at :624.
    //
    // Defaults: window_type=4 (Hamming) at :134; kaiser_pi=14.0 at :145;
    // frame_rate=15 at :335; CLIP_FRACTION=0.04 at :529; KEEP_TIME=0.1
    // at :779.
    constexpr double kClipFraction = 0.04;
    constexpr double kKeepTime     = 0.1;
    const int clip = static_cast<int>(
        std::floor(kClipFraction * static_cast<double>(m_fftSize)));
    const double samplesPerFrame =
        m_sampleRate / static_cast<double>(m_outputFps);
    const int ovrlp = std::max(0,
        static_cast<int>(std::ceil(static_cast<double>(m_fftSize) -
                                    samplesPerFrame)));
    const int max_w = m_fftSize + static_cast<int>(std::min(
        kKeepTime * m_sampleRate,
        kKeepTime * static_cast<double>(m_fftSize) *
                    static_cast<double>(m_outputFps)));
    int flp[1] = {0};

    SetAnalyzer(
        m_dispId,
        /*n_pixout=*/1,
        /*n_fft=*/1,
        /*typ=*/1,
        flp,
        /*sz=*/m_fftSize,
        /*bf_sz=*/m_fftSize,
        /*win_type=*/4,            // Thetis default (Hamming)
        /*pi=*/14.0,               // Thetis default (unused for non-Kaiser)
        /*ovrlp=*/ovrlp,
        /*clp=*/clip,              // Thetis: floor(0.04 * fft_size) = 163
        /*fscLin=*/0.0,
        /*fscHin=*/0.0,
        /*n_pix=*/m_numPixels,
        /*n_stch=*/1,
        /*calset=*/0,
        /*fmin=*/0.0,
        /*fmax=*/0.0,
        /*max_w=*/max_w);

    // From Thetis specHPSDR.cs:301-322 [v2.10.3.13+501e3f51] — DetTypePan
    // / DetTypeWF setters.  Default UI state is peak detection (mode 0),
    // average off (mode 0), num_avg = 1.
    SetDisplayDetectorMode(m_dispId, /*pixout=*/0, /*mode=*/0);
    SetDisplayAverageMode (m_dispId, /*pixout=*/0, /*mode=*/0);
    SetDisplayNumAverage  (m_dispId, /*pixout=*/0, /*num=*/1);
    SetDisplaySampleRate  (m_dispId, static_cast<int>(m_sampleRate));
}
#endif // HAVE_WDSP
```

- [ ] **Step 2.4: Re-verify `WdspEngine.cpp` disp arg is 5**

Run:
```bash
grep -n "TXASetSipDisplay" /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display/src/core/WdspEngine.cpp
```

Expected: line shows `TXASetSipDisplay(channelId, /*txinid=*/5);`. If it still shows `2`, fix it now.

- [ ] **Step 2.5: Build and verify clean compile**

Run:
```bash
cd /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display && cmake --build build --parallel --target NereusSDR 2>&1 | tail -10
```

Expected: build succeeds.

- [ ] **Step 2.6: Commit (GPG-signed)**

Run:
```bash
git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display add -u src/core/TxAnalyzer.cpp src/core/TxAnalyzer.h src/core/WdspEngine.cpp && git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display commit -S -m "$(cat <<'EOF'
debug(tx-display): swap to Thetis initAnalyzer params verbatim

Replaces attempt 1's CalcSpectrum-derived SetAnalyzer params with
initAnalyzer-derived params per design doc §3.1:

- clip 0 -> floor(0.04 * fft_size) = 163
- window_type 1 (BH4) -> 4 (Hamming, Thetis default)
- frame_rate 30 -> 15 (Thetis default)
- disp ID 2 -> 5 (cmaster.inid(1, 0) = cmRCVR + 0)
- kaiser_pi 0.0 -> 14.0 (Thetis default, unused for Hamming)

Header cite block updated: CalcSpectrum -> initAnalyzer.

Throwaway. Revert before any PR.
EOF
)"
```

---

## Task 3: Add diagnostic probe

**Files:**
- Modify: `src/core/TxAnalyzer.h` (member additions).
- Modify: `src/core/TxAnalyzer.cpp` (probe implementation).

- [ ] **Step 3.1: Add probe state to `TxAnalyzer.h`**

In the private section of `class TxAnalyzer`, add:

```cpp
// Throwaway diagnostic probe state (2026-05-10, KG4VCF).  Reset on each
// MOX rise via resetMoxRiseProbe().  Logs first kProbeMaxFrames after
// each MOX rise.  Removed before any PR.
static constexpr int kProbeMaxFrames = 20;
int m_moxRiseSeq{0};      // increments on each MOX rise
int m_probeFrameCount{0}; // resets on each MOX rise
```

Also add a public method declaration:

```cpp
/// Throwaway: reset diagnostic frame counter on MOX rise.  Removed
/// before any PR.
void resetMoxRiseProbe();
```

- [ ] **Step 3.2: Implement `resetMoxRiseProbe()` in `TxAnalyzer.cpp`**

Add at the bottom of the namespace block:

```cpp
void TxAnalyzer::resetMoxRiseProbe()
{
    m_moxRiseSeq++;
    m_probeFrameCount = 0;
}
```

- [ ] **Step 3.3: Add cfg log line at the end of `applySetAnalyzer()`**

Inside `applySetAnalyzer()`, after `SetDisplaySampleRate(...)`, add:

```cpp
    // Throwaway diagnostic probe (2026-05-10, KG4VCF).  Removed before any PR.
    qCInfo(lcDsp).noquote()
        << "[TXDIAG-attempt2-cfg]"
        << "disp=" << m_dispId
        << "fft_size=" << m_fftSize
        << "sample_rate=" << m_sampleRate
        << "fps=" << m_outputFps
        << "clip=" << clip
        << "win_type=4 pi=14.0"
        << "ovrlp=" << ovrlp
        << "n_pix=" << m_numPixels
        << "max_w=" << max_w;
```

- [ ] **Step 3.4: Add per-frame probe in `poll()`**

In `TxAnalyzer::poll()`, immediately after `GetPixels(m_dispId, 0, m_pixBuf.data(), &flag);` and before the existing diagnostic block (or replacing it), add:

```cpp
    // Throwaway diagnostic probe (2026-05-10, KG4VCF).  First N frames
    // after each MOX rise.  Removed before any PR.
    if (flag != 0 && m_probeFrameCount < kProbeMaxFrames) {
        const int n = static_cast<int>(m_pixBuf.size());
        const int dcBin = n / 2;
        // Top-5 (idx, dBm) by descending dBm.
        int topIdx[5] = {-1, -1, -1, -1, -1};
        float topVal[5] = {-1e9f, -1e9f, -1e9f, -1e9f, -1e9f};
        for (int i = 0; i < n; ++i) {
            const float v = m_pixBuf[i];
            for (int k = 0; k < 5; ++k) {
                if (v > topVal[k]) {
                    for (int j = 4; j > k; --j) {
                        topVal[j] = topVal[j-1];
                        topIdx[j] = topIdx[j-1];
                    }
                    topVal[k] = v;
                    topIdx[k] = i;
                    break;
                }
            }
        }
        // 7-point neighborhood across DC.
        const int probeOffsets[7] = {-3, -2, -1, 0, 1, 2, 3};
        QString dcNeighborhood;
        for (int off : probeOffsets) {
            const int idx = std::clamp(dcBin + off, 0, n - 1);
            dcNeighborhood += QStringLiteral("bin%1=%2 ")
                .arg(idx).arg(m_pixBuf[idx]);
        }
        qCInfo(lcDsp).noquote()
            << "[TXDIAG-attempt2-frame]"
            << "mox_rise_seq=" << m_moxRiseSeq
            << "frame=" << m_probeFrameCount
            << "top5=[(" << topIdx[0] << "," << topVal[0] << ")"
            << "(" << topIdx[1] << "," << topVal[1] << ")"
            << "(" << topIdx[2] << "," << topVal[2] << ")"
            << "(" << topIdx[3] << "," << topVal[3] << ")"
            << "(" << topIdx[4] << "," << topVal[4] << ")]"
            << "dc_neighborhood=[" << dcNeighborhood.trimmed() << "]";
        m_probeFrameCount++;
    }
```

If the existing attempt-1 diagnostic block (lines tagged `BENCH DIAGNOSTIC` in the stash version of the file) is still present, delete it; the new probe replaces it.

- [ ] **Step 3.5: Wire `resetMoxRiseProbe()` from MainWindow MOX-rise lambda**

In `src/gui/MainWindow.cpp`, find the existing MOX-rise lambda block (the `if (isTx) { ... }` branch added by the stash). Inside the `isTx` branch, after `m_txAnalyzer->setNumPixels(...)` and before `m_txAnalyzer->start()`, add:

```cpp
                    // Throwaway diagnostic probe (2026-05-10, KG4VCF).
                    // Reset frame counter so we capture cold-start state
                    // of each MOX rise.  Removed before any PR.
                    m_txAnalyzer->resetMoxRiseProbe();
```

- [ ] **Step 3.6: Build and verify clean compile**

Run:
```bash
cd /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display && cmake --build build --parallel --target NereusSDR 2>&1 | tail -10
```

Expected: build succeeds.

- [ ] **Step 3.7: Commit (GPG-signed)**

Run:
```bash
git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display add -u src/core/TxAnalyzer.h src/core/TxAnalyzer.cpp src/gui/MainWindow.cpp && git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display commit -S -m "$(cat <<'EOF'
debug(tx-display): add attempt-2 diagnostic probe

Adds [TXDIAG-attempt2-cfg] one-shot log at applySetAnalyzer time and
[TXDIAG-attempt2-frame] per-frame log for first 20 frames after each MOX
rise. Probe captures top-5 bins + 7-point DC neighborhood, lets a single
bench cycle confirm whether the initAnalyzer-parity fix kills the DC
pedestal observed in attempt 1.

Throwaway. Revert before any PR.
EOF
)"
```

---

## Task 4: Bench validation

**Files:** none (manual bench cycle).

- [ ] **Step 4.1: Kill running app + relaunch with fresh log file**

Run:
```bash
killall NereusSDR 2>/dev/null; sleep 1 && LOGFILE="/tmp/nereus-tx-attempt2-$(date +%Y%m%d-%H%M%S).log" && echo "Log file: $LOGFILE" && /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display/build/NereusSDR.app/Contents/MacOS/NereusSDR > "$LOGFILE" 2>&1 &
disown
```

- [ ] **Step 4.2: Verify app started and connected**

Run:
```bash
sleep 3 && ps -eo pid,command | grep "tx-display.*NereusSDR " | grep -v grep | head -2 && grep -m 1 "Starting NereusSDR" "$LOGFILE"
```

Expected: PID listed under tx-display path; banner shows current version. If app didn't start, check the log file head for crashes.

- [ ] **Step 4.3: User benches TUN single-tone**

User action: connect to ANAN-G2 (Saturn) preferred, otherwise HL2. USB on 14.241 MHz. Key TUN once for ~3 seconds.

After un-key, capture probe output:
```bash
grep "TXDIAG-attempt2" "$LOGFILE"
```

Expected: one `[TXDIAG-attempt2-cfg]` line at app start (or on first MOX rise) and N `[TXDIAG-attempt2-frame]` lines (up to 20) for the TUN MOX rise.

- [ ] **Step 4.4: User benches 2-tone test**

User action: Tools → PureSignal → Two-tone toggle. Hold for ~3 seconds. Toggle off.

Re-run the grep to capture the additional 2-tone frames:
```bash
grep "TXDIAG-attempt2" "$LOGFILE" | tail -40
```

- [ ] **Step 4.5: Interpret results against acceptance criteria**

Per design doc §6.2:

**TUN frame acceptance:**
- Peak bin at `n/2 + (gen1.tone.freq * n / sample_rate)` ≈ `n/2 + 6` for 600 Hz tone at 96 kHz / 1024 pixels.
- DC bin (`binN/2`) value must be ≥ 60 dB below the peak.
- Pedestal width (bins within 20 dB of DC) ≤ 3 bins centered on DC.

**2-tone frame acceptance:**
- Two peaks separated by `(1900 - 700) Hz / bin_width` pixels.
- DC bin value ≥ 40 dB below the lower peak.
- Pedestal width ≤ 3 bins centered on DC.

**Visual check (window the app, key TUN/2-tone again):**
- Narrow tones, sharp passband cutoff visible on the waterfall.
- No rainbow saturation centered on the carrier.

**MOX-down check:**
- RX1 spectrum returns within 1 frame.
- Waterfall scrollback continues without history wipe.

- [ ] **Step 4.6: Report findings to user**

Three branches:

**A. All acceptance criteria pass:** report success in chat, propose Task 5 (revert probe + clean PR prep). Stop here.

**B. Most criteria pass, narrow miss (e.g., one tone shows 5-bin pedestal instead of 3):** report partial success, propose tightening one or two specific param choices and re-bench. Document the miss in the design doc as a known limitation.

**C. DC pedestal still present (-30 to -40 dBm range like attempt 1):** report that the parity fix did not kill the bug. Pivot to design doc §8 follow-ups in priority order: (1) cold-start `I_samples` buffer state, (2) `XCreateAnalyzer` + `SetAnalyzer` ordering, (3) first-push timing race, (4) Thetis bin-for-bin comparison bench. Open a new brainstorm thread.

---

## Task 5 (conditional, only on Task 4.6 branch A): Clean up before PR

**Files:** revert probe commits, draft PR-ready commit.

This task only runs if Task 4.6 ends in branch A. Skip if branch B or C.

- [ ] **Step 5.1: Revert the gen-state probe commit (`c2e62e6`)**

The gen-state probe is no longer needed once the analyzer pedestal is fixed.

```bash
git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display revert --no-edit c2e62e6
```

Wait, `--no-edit` is forbidden per CLAUDE.md for git rebase, but `--no-edit` works on `git revert`. Verify by running `git revert --help` if uncertain. If `--no-edit` works on revert, use it. Otherwise omit and accept the default revert message.

- [ ] **Step 5.2: Revert the attempt-2 diagnostic probe commit (Task 3.7)**

```bash
git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display revert --no-edit HEAD
```

(HEAD here refers to whichever commit is the Task 3.7 probe commit; verify with `git log -3 --oneline` before reverting.)

- [ ] **Step 5.3: Squash the attempt-2 implementation into one clean commit**

Squash Task 1.11, Task 2.6, and Task 3.7 reverts into a single PR-ready commit. Use interactive rebase or `git reset --soft <base>` + new commit. After squash, the diff vs `4eef91e` (origin/main) should be only the TX panadapter feature without the probes.

- [ ] **Step 5.4: Run full ctest suite**

Run:
```bash
cd /Users/j.j.boyd/NereusSDR/.claude/worktrees/tx-display && cmake --build build --parallel && ctest --test-dir build --output-on-failure 2>&1 | tail -30
```

Expected: 100% test pass.

- [ ] **Step 5.5: Update [docs/architecture/tx-display-attempt2-design.md](tx-display-attempt2-design.md) sign-off section**

Mark §10 status as "Approved and bench-verified on YYYY-MM-DD". Note any limitations discovered during bench.

- [ ] **Step 5.6: Draft PR (do NOT post until JJ approves)**

Per `feedback_no_public_posts_without_review.md`. Draft the PR title and body in chat. Wait for explicit "post it" before running `gh pr create`.

---

## Open follow-ups if Task 4.6 ends in branch C

If the parity fix does not kill the DC pedestal, the next investigation move (per design doc §8) is:

1. **Cold-start `I_samples` buffer state.** Read `XCreateAnalyzer` + `SetAnalyzer` impl in `analyzer.c` to verify the input rings are zeroed before first push. If not, add a one-line `memset` in `Spectrum0` first-frame handling and re-bench.
2. **`XCreateAnalyzer` + `SetAnalyzer` ordering race.** Add a barrier or one-time init flag check to ensure SetAnalyzer fully completes before the first xsiphon push fires.
3. **First-push timing.** Verify MOX rise order: TxAnalyzer construction → applySetAnalyzer → MOX edge → siphon push. Add ordered-startup logging if needed.
4. **Thetis bin-for-bin comparison.** Patrick or another tester runs Thetis with logging hooked into the same probe indices for the same input.

These are NOT part of attempt 2's plan. Each becomes a separate brainstorm + plan if reached.

---

## Self-review notes

**Spec coverage check:**
- §2.1 bench data: covered by Task 4 (re-bench post-fix).
- §3 smoking gun: covered by Task 2 (param swap).
- §4 architecture: covered by Tasks 1 + 2 (stash apply + param swap).
- §4.1 exact param table: covered by Task 2.3.
- §4.2 disp ID: covered by Tasks 1.6, 2.1, 2.4.
- §5 diagnostic probe: covered by Task 3.
- §6 bench plan + acceptance criteria: covered by Task 4.
- §7 implementation notes: reflected in Task 1 conflict-resolution steps.
- §8 follow-ups: surfaced in this plan's "Open follow-ups" section + Task 4.6 branch C.

**Placeholder scan:** no `TBD`, `FIXME`, or "implement later". All steps have actual code or commands.

**Type consistency check:** `kTxDispId`, `m_outputFps`, `m_fftSize`, `kProbeMaxFrames`, `m_moxRiseSeq`, `m_probeFrameCount`, `resetMoxRiseProbe()` are used consistently across Tasks 2, 3, and the design doc.

**Implementation cost estimate (recap from design doc §9):**
- Task 1 (stash + conflict resolution): 60-90 min.
- Task 2 (param swap): 15 min.
- Task 3 (diagnostic probe): 30 min.
- Task 4 (bench): 30 min.
- Task 5 (cleanup, if branch A): 30 min.

Total: 2.5-3.5 hours from approval to bench result.
