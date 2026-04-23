# Phase 3G — RX Epic Sub-epic C-1: Noise Reduction (7-Filter Stack) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the full 7-filter RX noise-reduction stack — NR1 (ANR), NR2 (EMNR incl. post2 cascade), NR3 (RNNR / rnnoise), NR4 (SBNR / libspecbleach), DFNR (DeepFilterNet3), BNR (NVIDIA Broadcast SDK, Windows+NVIDIA), MNR (Apple Accelerate, macOS) — into NereusSDR with full Thetis UI parity, per-slice independence, `DspParamPopup` quick-controls, and a Setup → DSP → NR/ANF page mirroring the Thetis screenshot.

**Architecture (pivot from original design brief):** NR1/NR2/NR3/NR4 run as **WDSP pipeline stages** inside the RXA channel (not post-WDSP). We already ship `anr.c` + `emnr.c` in `third_party/wdsp/src/`; we port `rnnr.c` + `sbnr.c` from Thetis into the same tree, add `rnnoise` (BSD) and `libspecbleach` (LGPL-2.1) as WDSP dependencies, and wire all four via the native `SetRXA*` setter API Thetis already uses. DFNR/BNR/MNR run **post-WDSP-demod** inside `RxChannel::processIq()` (after `fexchange2()`) because Thetis has no equivalents. Mutual exclusion is enforced in `SliceModel::setActiveNr(NrSlot)` which flips the correct `SetRXA*Run` + post-WDSP-filter-active booleans atomically. Per-slice NR state is session-level (tuning knobs persisted once per slice, not per-band). NR3's rnnoise model is a global `RNNRloadModel(path)` setter with a bundled default + user-loadable `.bin`.

**Tech Stack:** C++20, Qt6, WDSP (extended), rnnoise (BSD 3-clause), libspecbleach (LGPL-2.1), DeepFilterNet3 via Rust `deep-filter-sys` (MIT/Apache-2.0), NVIDIA Broadcast SDK (EULA, runtime-loaded on Windows only, not redistributed), Apple Accelerate vDSP (macOS system framework).

**Spec reference:** [phase3g-rx-experience-epic-design.md](phase3g-rx-experience-epic-design.md) §sub-epic C. Note: C-1 scope expanded per user direction 2026-04-23 to **include all 7 filters in one PR** (C-2, C-3 collapsed into C-1; C-4 polish folded in).

**Upstream stamps:**
- Thetis `v2.10.3.13` @ `501e3f51`
- AetherSDR `main` @ `0cd4559` (verify in Task 0; refresh `[@sha]` on any new cite taken mid-plan)

**License attestations (verified 2026-04-23 at plan-writing time):**

| Upstream | License | Compatibility | Bundle |
|---|---|---|---|
| Thetis `rnnr.{c,h}` | GPL-2.0-or-later + Samphire dual-license clause for his own contributions | ✅ upward-compat with GPLv3 | Copy verbatim header + append NereusSDR modification block |
| Thetis `sbnr.{c,h}` | GPL-2.0-or-later + Samphire dual-license clause | ✅ upward-compat with GPLv3 | Copy verbatim header + append NereusSDR modification block |
| `rnnoise` (Xiph/Jean-Marc Valin) | BSD 3-clause | ✅ | `third_party/rnnoise/LICENSE` preserved; source bundled |
| `libspecbleach` (Luciano Dato) | LGPL-2.1 | ✅ static-link OK with LGPL notices | `third_party/libspecbleach/LICENSE` preserved; source bundled |
| DeepFilterNet3 + `deep-filter-sys` | MIT / Apache-2.0 (dual) | ✅ | `third_party/deep-filter-sys/LICENSE*` preserved; Rust cargo build; model tar.gz in per-platform Resources |
| NVIDIA Broadcast SDK | NVIDIA EULA | 🚫 not redistributable | **Runtime-loaded** from user's NVIDIA install on Windows; dlopen fallback if absent; never bundled |
| Apple Accelerate (vDSP) | Apple system framework | ✅ | Linked via `-framework Accelerate` on macOS; no bundle concern |

**Out of scope for this plan:**
- ANF (Automatic Notch Filter) — deferred to separate notch/ANF epic; belongs on the same Thetis Setup → DSP → NR/ANF page but is a distinct filter
- Thetis NR2 `trainT2` + `trainZetaThresh` detail UI beyond the visible T1/T2 spinboxes in the screenshot (user hasn't asked for deeper train tuning yet)
- TX-side noise processing (TXA NR) — Phase 3M-3 scope
- Per-band persistence of NR tuning — user chose **session-level only**; NR state persists once per slice, not per band-hop

---

## File structure

**New files (24):**

| File | Responsibility |
|---|---|
| `third_party/wdsp/src/rnnr.c` | **Byte-for-byte copy** from `Thetis/Project Files/Source/wdsp/rnnr.c` @ v2.10.3.13 |
| `third_party/wdsp/src/rnnr.h` | **Byte-for-byte copy** from Thetis |
| `third_party/wdsp/src/sbnr.c` | **Byte-for-byte copy** from Thetis |
| `third_party/wdsp/src/sbnr.h` | **Byte-for-byte copy** from Thetis |
| `third_party/rnnoise/` | Xiph rnnoise source tree + LICENSE + CMake glue |
| `third_party/libspecbleach/` | libspecbleach source tree + LICENSE + CMake glue |
| `third_party/deep-filter-sys/` | Rust crate vendored + model tar.gz + cargo build rules |
| `third_party/rnnoise/models/Default_large.bin` | Baked-in default rnnoise model for NR3 |
| `src/core/DeepFilterFilter.{h,cpp}` | Port from AetherSDR `src/core/DeepFilterFilter.{h,cpp}`; **modified** to accept 48 kHz stereo float natively (no internal 24↔48 resample) — documented in modification block |
| `src/core/NvidiaBnrFilter.{h,cpp}` | Port from AetherSDR; runtime-loaded NVIDIA SDK; Windows-only |
| `src/core/MacNRFilter.{h,cpp}` | Port from AetherSDR; macOS-only via `#ifdef __APPLE__` |
| `src/gui/widgets/DspParamPopup.{h,cpp}` | Port from AetherSDR `src/gui/DspParamPopup.{h,cpp}`; floating QWidget with sliders + tooltips + "More Settings…" button |
| `src/gui/setup/NrAnfSetupPage.{h,cpp}` | New Setup page — single Setup → DSP → NR/ANF page with sub-tabs for NR1/NR2/NR3/NR4/DFNR/BNR/MNR (Thetis screenshot layout) |
| `tests/tst_slice_nr_mutex.cpp` | Mutual-exclusion enforcement tests |
| `tests/tst_slice_nr_persistence.cpp` | Session-level persistence round-trip tests |
| `tests/tst_rx_channel_nr_wiring.cpp` | WDSP setter call verification (fake backend) |
| `tests/tst_nr_dfnr_bypass.cpp` | DFNR bypass-correctness on seeded noise+sine |
| `docs/architecture/phase3g-rx-epic-c1-verification/README.md` | Manual verification matrix (3G-8 style) |

**Modified files (17):**

| File | Change |
|---|---|
| `third_party/wdsp/CMakeLists.txt` | Add rnnr.c + sbnr.c to source glob; link rnnoise + libspecbleach |
| `third_party/wdsp/src/channel.c` | Add NR3 + NR4 to RXA pipeline creation (mirror Thetis cmaster.c); byte-for-byte from Thetis where changed |
| `src/core/wdsp_api.h` | Add full Thetis NR1/NR2 setter surface (missing post2* binds) + all NR3 binds + all NR4 binds |
| `src/core/RxChannel.{h,cpp}` | NR1/NR2/NR3/NR4 WDSP-setter wiring; DFNR/BNR/MNR post-WDSP member filters; activeNr dispatch in processIq |
| `src/core/WdspTypes.h` | Add `NrSlot` enum (Off/NR1/NR2/NR3/NR4/DFNR/BNR/MNR) + `AnrPosition` + `EmnrGainMethod` + `EmnrNpeMethod` + `SbnrAlgo` |
| `src/models/SliceModel.{h,cpp}` | `activeNr` + full NR tuning properties (~35 knobs across 7 filters); mutual-exclusion setter; session-level save/restore |
| `src/models/RadioModel.cpp` | Push full NR config on slice activate + radio connect |
| `src/core/AppSettings.h` | Document `Slice<N>/NrActive` + `Slice<N>/Nr1*` + `Slice<N>/Nr2*` + `Slice<N>/Nr3*` + `Slice<N>/Nr4*` + `Slice<N>/Dfnr*` + `Slice<N>/Bnr*` + `Slice<N>/Mnr*` + global `Nr3ModelPath` |
| `src/gui/widgets/VfoWidget.{h,cpp}` | Replace NR \| NR2 cells with 7-button NR bank sub-HBox (NR1 NR2 NR3 NR4 DFNR BNR MNR); right-click popup wiring; platform-gating; FM-mode gating for BNR |
| `src/gui/SpectrumOverlayPanel.{h,cpp}` | Mirror NR row with same 7 buttons + right-click behavior |
| `src/gui/setup/SetupDialog.cpp` | Register new NrAnfSetupPage; wire `selectPage("NR/ANF")` + optional sub-tab param |
| `src/gui/MainWindow.cpp` | Connect `openNrSetupRequested(NrSlot)` → SetupDialog.selectPage + sub-tab; wire 7 toggled signals → SliceModel.setActiveNr |
| `CMakeLists.txt` | `ENABLE_BNR` (Windows default ON), `ENABLE_MNR` (macOS default ON), `ENABLE_DFNR` (all default ON); deep-filter-sys cargo integration; NVIDIA SDK detection |
| `.github/workflows/ci.yml` | Add Rust toolchain install step for all 3 OS builds |
| `.github/workflows/release.yml` | Bundle DFNet3 model + rnnoise default model in per-platform artifacts |
| `CONTRIBUTING.md` | Add Rust + cargo to build deps; note libspecbleach + rnnoise bundling |
| `CHANGELOG.md` | Entry under `## [Unreleased]` |

---

## Pre-flight inventory (read-only)

Run these greps before editing so call-site footprint is current:

```bash
# NR state in current codebase (should be placeholder stubs only)
rg -n 'nrEnabled|emnrEnabled|setNr[A-Z]|m_nr[A-Za-z]|NrActive|activeNr|NrSlot' src/ tests/

# WDSP NR setter binds already in wdsp_api.h
rg -n 'SetRXAANR|SetRXAEMNR|SetRXARNNR|SetRXASBNR|RNNRloadModel' src/core/wdsp_api.h

# VfoWidget current NR button placement (should be m_nrToggle @ 0,2 + m_nr2Toggle @ 0,3)
rg -n 'm_nrToggle|m_nr2Toggle|makeToggle.*NR' src/gui/widgets/VfoWidget.cpp

# Signal wiring for Setup-to-tab jumps (Sub-epic B NB pattern to mirror)
rg -n 'openNbSetupRequested|selectPage.*NB' src/gui/
```

**Expected state at plan-writing time (2026-04-23) — will drift:**

- `src/core/RxChannel.h:295-315` — stub `m_nrEnabled`, `m_emnrEnabled` atomics + placeholder setters (from 3G-10 Stage 2)
- `src/core/wdsp_api.h:~250-340` — partial NB + no-NR surface
- `src/models/SliceModel.h:~194` — stub `emnrEnabled` Q_PROPERTY
- `src/gui/widgets/VfoWidget.cpp:957-967` — `m_nrToggle` (col 2) + `m_nr2Toggle` (col 3) to be replaced by 7-button bank
- `src/gui/MainWindow.cpp:~2418-2425` — `openNbSetupRequested` → `selectPage("NB/SNB")` pattern (Sub-epic B) — mirror this for NR

Any hit outside this list needs review before editing.

---

## Shared patterns (read once, applied everywhere)

**File header attribution (every new file):**
1. Copy Thetis/AetherSDR upstream header **byte-for-byte** including dual-licensing clause for MW0LGE files
2. For multi-file ports, use `// --- From <filename> ---` separators
3. Append `// Modification history (NereusSDR):` block with port date, human author (J.J. Boyd, KG4VCF), AI tooling disclosure (Anthropic Claude Code)
4. For the DFNR wrapper, include explicit modification note: "2026-04-2X — Accept 48 kHz stereo float natively. Removed internal 24↔48 resampler stages from AetherSDR's 24 kHz-pipeline-targeted wrapper. See Modification block."

**Inline cite format** (per `docs/attribution/HOW-TO-PORT.md` §Inline cite versioning):
```cpp
// From Thetis Project Files/Source/wdsp/emnr.c:951-995 [v2.10.3.13]
// From Thetis Project Files/Source/Console/setup.cs:17354 [v2.10.3.13]
// From AetherSDR src/core/DeepFilterFilter.cpp:42-78 [@0cd4559]
```

**PROVENANCE update:** Every new `src/` file needs a row in `docs/attribution/THETIS-PROVENANCE.md` listing upstream path + license + commit stamp.

**Pre-commit hook** (already installed via `scripts/install-hooks.sh`): runs `verify-thetis-headers.py`, `verify-inline-tag-preservation.py`, `check-new-ports.py` pre-push. If any fail, stop and fix — do not `--no-verify`.

**GPG-sign all commits:** `git commit -S -m "…"`. Never `--no-gpg-sign`.

**Atomic parameter pattern for cross-thread DSP:** Main thread writes via `std::atomic<T>`; audio thread reads via `load(std::memory_order_acquire)`. Never hold a mutex in the audio callback.

**Tooltip curation rule (per Q11):** For each user-facing NR control, read both Thetis `setup.designer.cs` tooltips (via `toolTip.SetToolTip(control, "...")` patterns) AND AetherSDR tooltips (via `setToolTip()` calls in VfoWidget.cpp + AetherDspDialog.cpp), and select or synthesize the version that is (1) technically accurate, (2) consistent with the NereusSDR voice (ham-radio technical, not consumer-audio), (3) includes both what the control does and a sensible starting value when non-obvious. Record source in inline comment: `// Tooltip: Thetis-synth` or `// Tooltip: AetherSDR-verbatim` or `// Tooltip: synthesized from both`.

---

## Tasks

### Task 0: Refresh upstream stamps + worktree verification

**Files:** none (read-only setup)

- [ ] **Step 1: Confirm Thetis + AetherSDR versions match plan header**

```bash
git -C /Users/j.j.boyd/Thetis describe --tags
# Expect: v2.10.3.13-7-g501e3f51 (or newer; if newer, update plan header)
git -C /Users/j.j.boyd/AetherSDR rev-parse --short HEAD
# Expect: 0cd4559 (or newer; if newer, update plan header before porting any new cite)
```

- [ ] **Step 2: Verify clean worktree on `claude/phase3g-rx-epic-c1-nr-xplat` branch**

```bash
cd /Users/j.j.boyd/NereusSDR/.claude/worktrees/phase3g-rx-epic-c1-nr-xplat
git status  # expect: clean on branch claude/phase3g-rx-epic-c1-nr-xplat
git log --oneline -5  # confirm fresh off main
```

- [ ] **Step 3: Build a clean baseline to establish warning-free starting point**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc) 2>&1 | tee /tmp/c1-baseline-build.log
```

Expected: succeeds with zero new warnings. If warnings present, note them; don't let later tasks be blamed for them.

---

### Task 1: Add `NrSlot` + companion enums to `WdspTypes.h`

NR state is a first-class enum, not a set of booleans. This task establishes the type surface before any setters land.

**Files:**
- Modify: `src/core/WdspTypes.h` (around line 230, next to existing `NbMode`)

- [ ] **Step 1: Add enum declarations**

Insert after the existing `enum class NbMode` block:

```cpp
// From Thetis console.cs:43297-43450 [v2.10.3.13] — SelectNR() enforces
// at-most-one-NR-per-channel by setting all 4 RXANR*Run flags atomically.
// NereusSDR extends the set with 3 post-WDSP external filters (DFNR/BNR/MNR)
// that don't exist in Thetis.
enum class NrSlot : int {
    Off  = 0,
    NR1  = 1,   // WDSP anr.c     (Warren Pratt, NR0V)
    NR2  = 2,   // WDSP emnr.c    (Warren Pratt, NR0V)
    NR3  = 3,   // WDSP rnnr.c    (Samphire MW0LGE, rnnoise backend)
    NR4  = 4,   // WDSP sbnr.c    (Samphire MW0LGE, libspecbleach backend)
    DFNR = 5,   // AetherSDR DeepFilterFilter (DeepFilterNet3, post-WDSP)
    BNR  = 6,   // AetherSDR NvidiaBnrFilter (NVIDIA Broadcast, Windows+NVIDIA, post-WDSP)
    MNR  = 7    // AetherSDR MacNRFilter (Apple Accelerate, macOS, post-WDSP)
};

// From Thetis wdsp/anr.h:102 [v2.10.3.13] — SetRXAANRPosition(int channel, int position)
// Applies to NR1, NR2, NR3 equally (all three are WDSP stages with Position).
// NR4/DFNR/BNR/MNR have no Position control.
enum class NrPosition : int {
    PreAgc  = 0,
    PostAgc = 1
};

// From Thetis setup.cs:17354-17468 [v2.10.3.13] — EMNR Gain Method radio group.
enum class EmnrGainMethod : int {
    Linear  = 0,
    Log     = 1,
    Gamma   = 2,   // default per Thetis EMNR_DEFAULT_GAIN_METHOD
    Trained = 3
};

// From Thetis setup.cs:17374-17404 [v2.10.3.13] — EMNR NPE Method radio group.
enum class EmnrNpeMethod : int {
    Osms  = 0,   // default
    Mmse  = 1,
    Nstat = 2
};

// From Thetis setup.cs:34511-34527 [v2.10.3.13] — SBNR Algo 1/2/3 radio group.
// Calls SetRXASBNRnoiseScalingType(channel, 0/1/2).
enum class SbnrAlgo : int {
    Algo1 = 0,
    Algo2 = 1,   // default per Thetis screenshot (selected in shipped config)
    Algo3 = 2
};
```

- [ ] **Step 2: Build to confirm no existing conflicts**

```bash
cmake --build build -j$(nproc) --target NereusSDR 2>&1 | tail -20
```

Expected: build succeeds.

- [ ] **Step 3: Commit**

```bash
git add src/core/WdspTypes.h
git commit -S -m "feat(wdsp-types): add NrSlot + NrPosition + EMNR/SBNR method enums

Establishes the type surface for Sub-epic C-1. NrSlot holds the full
7-filter set; Position applies to NR1/NR2/NR3. Enum values chosen to
match Thetis SetRXA*Run/Position/GainMethod/NPEMethod/noiseScalingType
integer conventions byte-for-byte so RxChannel setters pass through
without translation.

Cites: Thetis console.cs:43297, setup.cs:17354-34527 [v2.10.3.13]."
```

---

### Task 2: Extend `wdsp_api.h` with full Thetis NR1/NR2/NR3/NR4 setter surface

Every Thetis Setup screenshot control maps to one WDSP `SetRXA*` call. This task binds all of them so `RxChannel` can call them.

**Files:**
- Modify: `src/core/wdsp_api.h`

- [ ] **Step 1: Add ANR (NR1) full surface**

Inside the `// Noise reduction — ANR (NR1)` block (create if absent), add:

```cpp
// =====================================================================
// NR1 — Adaptive Noise Reduction (WDSP anr.c, Warren Pratt NR0V)
// From Thetis wdsp/anr.h:47-52 [v2.10.3.13]. Channel = WDSP channel id.
// =====================================================================
void SetRXAANRRun      (int channel, int run);
void SetRXAANRVals     (int channel, int taps, int delay, double gain, double leakage);
void SetRXAANRTaps     (int channel, int taps);
void SetRXAANRDelay    (int channel, int delay);
void SetRXAANRGain     (int channel, double gain);
void SetRXAANRLeakage  (int channel, double leakage);
void SetRXAANRPosition (int channel, int position);  // 0=pre-AGC, 1=post-AGC
```

- [ ] **Step 2: Add EMNR (NR2) full surface incl. post2 cascade**

```cpp
// =====================================================================
// NR2 — EMNR (WDSP emnr.c, Warren Pratt NR0V).
// From Thetis wdsp/emnr.h + setup.cs [v2.10.3.13].
// =====================================================================
void SetRXAEMNRRun             (int channel, int run);
void SetRXAEMNRPosition        (int channel, int position);
void SetRXAEMNRgainMethod      (int channel, int method);   // EmnrGainMethod
void SetRXAEMNRnpeMethod       (int channel, int method);   // EmnrNpeMethod
void SetRXAEMNRaeRun           (int channel, int run);      // AE Filter checkbox
void SetRXAEMNRaeZetaThresh    (int channel, double zetathresh);
void SetRXAEMNRaePsi           (int channel, double psi);
void SetRXAEMNRtrainZetaThresh (int channel, double thresh);  // T1 spinbox
void SetRXAEMNRtrainT2         (int channel, double t2);       // T2 spinbox

// Post-processing cascade — the "Noise post proc" group in Thetis screenshot.
// From Thetis wdsp/emnr.c:951-995 [v2.10.3.13]. Internal scaling: Thetis
// setup.cs divides the UI int value by 100 before passing (e.g. UI 15 → 0.15).
void SetRXAEMNRpost2Run    (int channel, int run);
void SetRXAEMNRpost2Nlevel (int channel, double nlevel);
void SetRXAEMNRpost2Factor (int channel, double factor);
void SetRXAEMNRpost2Rate   (int channel, double tc);
void SetRXAEMNRpost2Taper  (int channel, int taper);
```

- [ ] **Step 3: Add RNNR (NR3) full surface**

```cpp
// =====================================================================
// NR3 — RNNR (WDSP rnnr.c, Samphire MW0LGE, rnnoise backend).
// From Thetis wdsp/rnnr.c:161-176, 348-401 [v2.10.3.13].
// =====================================================================
void SetRXARNNRRun            (int channel, int run);
void SetRXARNNRPosition       (int channel, int position);
void SetRXARNNRUseDefaultGain (int channel, int use_default_gain);

// Global (not per-channel): loads an rnnoise .bin model. Empty path ("")
// reverts to the baked-in default model.
void RNNRloadModel (const char* file_path);
```

- [ ] **Step 4: Add SBNR (NR4) full surface**

```cpp
// =====================================================================
// NR4 — SBNR (WDSP sbnr.c, Samphire MW0LGE, libspecbleach backend).
// From Thetis wdsp/sbnr.c:144-241 [v2.10.3.13].
// =====================================================================
void SetRXASBNRRun                 (int channel, int run);
void SetRXASBNRreductionAmount     (int channel, float amount);        // 0-20 dB
void SetRXASBNRsmoothingFactor     (int channel, float factor);        // 0-100 %
void SetRXASBNRwhiteningFactor     (int channel, float factor);        // 0-100 %
void SetRXASBNRnoiseRescale        (int channel, float factor);        // 0-12 dB
void SetRXASBNRpostFilterThreshold (int channel, float threshold);     // -10..+10 dB
void SetRXASBNRnoiseScalingType    (int channel, int noise_scaling_type); // SbnrAlgo

// Note: no Position setter exists in sbnr.c — SBNR runs at a fixed post-AGC
// position per Thetis wdsp/channel.c pipeline construction.
```

- [ ] **Step 5: Build to confirm linker finds these (once rnnr/sbnr land)**

Build will fail linker step until Task 6 brings rnnr.c + sbnr.c into `third_party/wdsp/`. That's expected here. Confirm the compilation errors are linker-stage, not syntax errors in `wdsp_api.h`:

```bash
cmake --build build -j$(nproc) --target NereusSDR 2>&1 | grep -E 'error|undefined' | head -20
```

Expected: compilation succeeds; link errors only for `SetRXARNNR*`, `RNNRloadModel`, `SetRXASBNR*`. NR1/NR2 should already resolve (anr.c + emnr.c already in build).

- [ ] **Step 6: Commit**

```bash
git add src/core/wdsp_api.h
git commit -S -m "feat(wdsp-api): add full Thetis NR1-4 setter surface

Adds 23 NR setters covering the Thetis Setup → DSP → NR/ANF screen
byte-for-byte: NR1 Taps/Delay/Gain/Leak/Position (7 setters), NR2 full
including post2 cascade (14 setters), NR3 Run/Position/UseDefaultGain
and global RNNRloadModel (4 setters), NR4 full (7 setters).

Linker errors for NR3/NR4 expected until Task 6 brings rnnr.c + sbnr.c.

Cites: Thetis wdsp/{anr,emnr,rnnr,sbnr}.h [v2.10.3.13],
       setup.cs:17354-34567 [v2.10.3.13]."
```

---

### Task 3: Port `rnnr.c` + `rnnr.h` into `third_party/wdsp/src/`

**Files:**
- Create: `third_party/wdsp/src/rnnr.c`
- Create: `third_party/wdsp/src/rnnr.h`

- [ ] **Step 1: Copy source byte-for-byte from Thetis**

```bash
cp "/Users/j.j.boyd/Thetis/Project Files/Source/wdsp/rnnr.c" third_party/wdsp/src/rnnr.c
cp "/Users/j.j.boyd/Thetis/Project Files/Source/wdsp/rnnr.h" third_party/wdsp/src/rnnr.h
```

- [ ] **Step 2: Append NereusSDR modification block**

Add to the end of each file's top header comment block (immediately after the closing `//==========//` bar of the Samphire dual-licensing statement):

```c
//
// =============================================================================
// Modification history (NereusSDR):
//   2026-04-2X — Imported byte-for-byte from Thetis v2.10.3.13 @ 501e3f51.
//                No algorithmic changes. Linked against rnnoise BSD-3
//                (third_party/rnnoise/) at WDSP build time.
//                Authored by J.J. Boyd (KG4VCF), ported with AI-assisted
//                review via Anthropic Claude Code. GPLv2+ upstream upgraded
//                to GPLv3 combined work under NereusSDR's GPLv3 umbrella
//                (per-file dual-license clause by MW0LGE unaffected).
// =============================================================================
```

- [ ] **Step 3: Add PROVENANCE row**

Append to `docs/attribution/THETIS-PROVENANCE.md`:

```markdown
| `third_party/wdsp/src/rnnr.c` | Thetis `Project Files/Source/wdsp/rnnr.c` | v2.10.3.13 @ 501e3f51 | GPL-2.0-or-later + Samphire dual-license | 2026-04-2X |
| `third_party/wdsp/src/rnnr.h` | Thetis `Project Files/Source/wdsp/rnnr.h` | v2.10.3.13 @ 501e3f51 | GPL-2.0-or-later + Samphire dual-license | 2026-04-2X |
```

- [ ] **Step 4: Run header verifier**

```bash
python3 scripts/verify-thetis-headers.py third_party/wdsp/src/rnnr.c third_party/wdsp/src/rnnr.h
```

Expected: passes. If fails, read the verifier output and adjust the header block (the most common cause is missing a `// ---` separator or wrong modification-block date format).

- [ ] **Step 5: Commit (source only — build integration in next task)**

```bash
git add third_party/wdsp/src/rnnr.c third_party/wdsp/src/rnnr.h docs/attribution/THETIS-PROVENANCE.md
git commit -S -m "feat(wdsp): port rnnr.c + rnnr.h from Thetis (NR3 backend)

Byte-for-byte import of Samphire MW0LGE's rnnoise-based noise reduction
stage. Dual-license clause preserved verbatim. Links rnnoise (BSD-3)
which lands in the next commit.

Source: Thetis v2.10.3.13 @ 501e3f51.
License: GPL-2.0-or-later (upstream) + Samphire dual-license clause;
         compat with NereusSDR GPLv3 combined work."
```

---

### Task 4: Port `sbnr.c` + `sbnr.h` into `third_party/wdsp/src/`

Same pattern as Task 3 for `sbnr`. Repeat steps 1-5 with sbnr filenames and libspecbleach in the modification block.

**Files:**
- Create: `third_party/wdsp/src/sbnr.c`
- Create: `third_party/wdsp/src/sbnr.h`

- [ ] **Step 1: Copy source byte-for-byte**

```bash
cp "/Users/j.j.boyd/Thetis/Project Files/Source/wdsp/sbnr.c" third_party/wdsp/src/sbnr.c
cp "/Users/j.j.boyd/Thetis/Project Files/Source/wdsp/sbnr.h" third_party/wdsp/src/sbnr.h
```

- [ ] **Step 2: Append modification block** (same template as Task 3 Step 2, s/rnnoise/libspecbleach/, s/BSD-3/LGPL-2.1/)

- [ ] **Step 3: Add PROVENANCE rows** (two rows for sbnr.c + sbnr.h)

- [ ] **Step 4: Run header verifier**

```bash
python3 scripts/verify-thetis-headers.py third_party/wdsp/src/sbnr.c third_party/wdsp/src/sbnr.h
```

- [ ] **Step 5: Commit**

```bash
git add third_party/wdsp/src/sbnr.c third_party/wdsp/src/sbnr.h docs/attribution/THETIS-PROVENANCE.md
git commit -S -m "feat(wdsp): port sbnr.c + sbnr.h from Thetis (NR4 backend)

Byte-for-byte import. Links libspecbleach (LGPL-2.1) which lands in
the next commit. Samphire dual-license clause preserved verbatim.

Source: Thetis v2.10.3.13 @ 501e3f51."
```

---

### Task 5: Vendor `rnnoise` library into `third_party/rnnoise/`

rnnr.c expects `rnnoise.h` and the library at link time. We vendor Xiph's rnnoise BSD-3 source so it travels with NereusSDR (no external dep).

**Files:**
- Create: `third_party/rnnoise/` (full source tree + LICENSE + README)
- Create: `third_party/rnnoise/models/Default_large.bin` (baked-in default model for NR3)
- Create: `third_party/rnnoise/CMakeLists.txt` (glue)

- [ ] **Step 1: Vendor rnnoise source**

Fetch latest stable rnnoise from `https://gitlab.xiph.org/xiph/rnnoise` — use the same revision Thetis ships in its own `rnnoise/` subtree to guarantee rnnr.c compatibility:

```bash
# Check which rnnoise revision Thetis uses:
ls "/Users/j.j.boyd/Thetis/Project Files/Source/wdsp/rnnoise/"  # or similar
# Copy that revision's src + include into third_party/rnnoise/
mkdir -p third_party/rnnoise/src third_party/rnnoise/include third_party/rnnoise/models
cp -r "/Users/j.j.boyd/Thetis/Project Files/rnnoise-or-wherever/"/{src,include,COPYING,README*} third_party/rnnoise/
```

Verify `COPYING` is BSD-3 (Xiph) and preserve it as `third_party/rnnoise/LICENSE`.

- [ ] **Step 2: Copy default large model**

rnnoise ships with a compiled-in default model. For user-loadable models Thetis uses external .bin files; we bundle one as the "Default (large)" baseline. Copy Thetis's bundled model:

```bash
find "/Users/j.j.boyd/Thetis/" -name "*.bin" -path "*rnn*" -o -name "Default*.bin" 2>/dev/null
# Copy the one Thetis's UI references as "Default (large)"
cp <path> third_party/rnnoise/models/Default_large.bin
```

If Thetis doesn't ship a user-loadable default .bin (baked into the library instead), document that in the header comment and leave `models/` empty — the "Default (large)" label then means "baked into rnnoise at link time, no external file".

- [ ] **Step 3: Write a thin CMakeLists.txt**

```cmake
# third_party/rnnoise/CMakeLists.txt
#
# Vendored Xiph.org rnnoise (BSD 3-clause). Used by wdsp/rnnr.c (NR3 backend).
#
# Source: https://gitlab.xiph.org/xiph/rnnoise
# Revision: pinned to match Thetis v2.10.3.13 rnnoise integration.
# License: BSD 3-clause — see LICENSE.
cmake_minimum_required(VERSION 3.20)

file(GLOB RNNOISE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
add_library(rnnoise_static STATIC ${RNNOISE_SOURCES})
target_include_directories(rnnoise_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(rnnoise_static PRIVATE RNNOISE_BUILD)
set_target_properties(rnnoise_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
```

- [ ] **Step 4: Add PROVENANCE row (external-origin variant)**

Append to `docs/attribution/THIRD-PARTY-PROVENANCE.md` (or equivalent external-origin register):

```markdown
| `third_party/rnnoise/` | Xiph.org rnnoise | <revision> | BSD 3-clause | 2026-04-2X |
```

- [ ] **Step 5: Commit**

```bash
git add third_party/rnnoise/
git commit -S -m "feat(deps): vendor Xiph rnnoise (BSD-3) for NR3 backend

Vendored to match Thetis's rnnoise revision. BSD 3-clause LICENSE
preserved at third_party/rnnoise/LICENSE.

Links statically into wdsp_static via rnnoise_static target.
No external dependency at build or runtime."
```

---

### Task 6: Vendor `libspecbleach` into `third_party/libspecbleach/`

Same pattern as Task 5 for libspecbleach (LGPL-2.1).

**Files:**
- Create: `third_party/libspecbleach/` (full source tree + LICENSE)
- Create: `third_party/libspecbleach/CMakeLists.txt`

- [ ] **Step 1: Vendor libspecbleach source**

```bash
# Clone/copy from https://github.com/lucianodato/libspecbleach, pin to
# a revision compatible with Thetis's sbnr.c integration.
# Source + LICENSE (LGPL-2.1) preserved verbatim.
```

- [ ] **Step 2: CMakeLists.txt**

```cmake
# third_party/libspecbleach/CMakeLists.txt
#
# Vendored libspecbleach (LGPL-2.1). Used by wdsp/sbnr.c (NR4 backend).
# Source: https://github.com/lucianodato/libspecbleach
# License: LGPL-2.1 — see LICENSE.
cmake_minimum_required(VERSION 3.20)

file(GLOB SPECBLEACH_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
add_library(specbleach_static STATIC ${SPECBLEACH_SOURCES})
target_include_directories(specbleach_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(specbleach_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
```

- [ ] **Step 3: Document LGPL-2.1 static-linking compliance**

Static-linking LGPL-2.1 with a GPLv3 combined work requires offering users the ability to relink libspecbleach with a modified version. Add a note to `docs/attribution/LGPL-COMPLIANCE.md` (create if absent) describing how users can obtain the source + relink instructions. For AppImage/DMG/NSIS artifacts, bundle the libspecbleach source alongside or link to our GitHub mirror.

- [ ] **Step 4: Add PROVENANCE row + commit**

```bash
git add third_party/libspecbleach/ docs/attribution/THIRD-PARTY-PROVENANCE.md docs/attribution/LGPL-COMPLIANCE.md
git commit -S -m "feat(deps): vendor libspecbleach (LGPL-2.1) for NR4 backend

LGPL-2.1 source vendored. LICENSE preserved; static-link compliance
note added documenting user relinking support per LGPL section 6.

Links statically into wdsp_static via specbleach_static target."
```

---

### Task 7: CMake integration — bring rnnr/sbnr/rnnoise/libspecbleach into wdsp_static build

**Files:**
- Modify: `third_party/wdsp/CMakeLists.txt`

- [ ] **Step 1: Update source glob comment and linkage**

Open `third_party/wdsp/CMakeLists.txt`. The existing `file(GLOB WDSP_SOURCES ...)` will auto-pick up rnnr.c + sbnr.c from Task 3/4. Add explicit link to rnnoise_static + specbleach_static:

```cmake
# Bring in NR3/NR4 runtime backends (added 2026-04-2X for Sub-epic C-1).
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/rnnoise
                 ${CMAKE_BINARY_DIR}/third_party_rnnoise)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/libspecbleach
                 ${CMAKE_BINARY_DIR}/third_party_libspecbleach)

target_link_libraries(wdsp_static
    PUBLIC
        rnnoise_static
        specbleach_static
)

target_include_directories(wdsp_static PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/rnnoise/include
    ${CMAKE_SOURCE_DIR}/third_party/libspecbleach/include
)
```

- [ ] **Step 2: Build**

```bash
rm -rf build && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc) --target NereusSDR 2>&1 | tee /tmp/c1-wdsp-build.log
```

Expected: **full link succeeds** — the NR3/NR4 setter calls from Task 2 now resolve because rnnr.c + sbnr.c provide their implementations and link against rnnoise_static + specbleach_static.

If libspecbleach's `specbleach_adenoiser.h` is in `include/` (some distributions use `include/libspecbleach/`), adjust the include path in Step 1 until sbnr.c's `#include <specbleach_adenoiser.h>` resolves.

- [ ] **Step 3: Commit**

```bash
git add third_party/wdsp/CMakeLists.txt
git commit -S -m "build(wdsp): link rnnoise + libspecbleach into wdsp_static

NR3 (rnnr.c) links rnnoise_static; NR4 (sbnr.c) links specbleach_static.
Both added as subdirectories and exposed as PUBLIC link deps of
wdsp_static so NereusSDR picks them up transitively.

Build passes; NR3/NR4 setters from wdsp_api.h now resolve."
```

---

### Task 8: Wire RxChannel NR1/NR2/NR3/NR4 setters

**Files:**
- Modify: `src/core/RxChannel.h` (add ~30 setters + tuning struct)
- Modify: `src/core/RxChannel.cpp` (implementations)

- [ ] **Step 1: Add NR tuning struct to RxChannel.h**

Inside `RxChannel` class, public section:

```cpp
// From Thetis setup.cs NR/ANF tab [v2.10.3.13] — one struct per WDSP NR stage.
// Defaults match Thetis cmaster.c create-time values byte-for-byte.
struct Nr1Tuning {
    int    taps      = 64;    // setup.cs:udDSPNR1Taps.Value
    int    delay     = 16;    // setup.cs:udDSPNR1Delay.Value
    double gain      = 100.0; // UI value; WDSP scaling factor applied at setter boundary
    double leakage   = 100.0; // UI value; WDSP scaling factor applied at setter boundary
    NrPosition position = NrPosition::PostAgc;
};

struct Nr2Tuning {
    EmnrGainMethod gainMethod = EmnrGainMethod::Gamma;
    EmnrNpeMethod  npeMethod  = EmnrNpeMethod::Osms;
    double         trainT1    = -0.5;   // Thetis default visible in screenshot
    double         trainT2    = 0.20;
    bool           aeFilter   = true;   // checked in screenshot
    NrPosition     position   = NrPosition::PostAgc;
    // Post-processing cascade (the "Noise post proc" group)
    bool   post2Run    = false;
    double post2Level  = 15.0;  // UI value; divided by 100 at boundary per setup.cs:34711
    double post2Factor = 15.0;
    double post2Rate   = 5.0;
    int    post2Taper  = 12;
};

struct Nr3Tuning {
    NrPosition position           = NrPosition::PostAgc;
    bool       useDefaultGain     = true;   // "Use fixed gain for input samples" checkbox (screenshot: checked)
    // Model path is global (RNNRloadModel); not per-channel.
};

struct Nr4Tuning {
    double reductionAmount     = 10.0;  // dB; screenshot default
    double smoothingFactor     = 65.0;  // %; screenshot default (RX1; RX2 shows 0.0)
    double whiteningFactor     = 2.0;   // %
    double noiseRescale        = 2.0;   // dB
    double postFilterThreshold = -10.0; // dB
    SbnrAlgo algo              = SbnrAlgo::Algo2;
};

// Setters — safe to call from any thread. Audio thread reads the
// activeNr atomic and dispatches; these setters push through WDSP.
void setAnrTuning   (const Nr1Tuning& t);
void setEmnrTuning  (const Nr2Tuning& t);
void setRnnrTuning  (const Nr3Tuning& t);
void setSbnrTuning  (const Nr4Tuning& t);

// Per-knob convenience setters for popup sliders that push a single WDSP call.
void setAnrTaps    (int taps);
void setAnrDelay   (int delay);
void setAnrGain    (double gainUiValue);
void setAnrLeakage (double leakageUiValue);
void setAnrPosition(NrPosition p);

void setEmnrGainMethod (EmnrGainMethod m);
void setEmnrNpeMethod  (EmnrNpeMethod m);
void setEmnrAeFilter   (bool on);
void setEmnrPost2Run   (bool on);
void setEmnrPost2Factor(double factor);
void setEmnrPost2Rate  (double rate);
void setEmnrPost2Level (double level);
void setEmnrPost2Taper (int taper);

void setRnnrPosition       (NrPosition p);
void setRnnrUseDefaultGain (bool on);

void setSbnrReductionAmount     (double dB);
void setSbnrSmoothingFactor     (double pct);
void setSbnrWhiteningFactor     (double pct);
void setSbnrNoiseRescale        (double dB);
void setSbnrPostFilterThreshold (double dB);
void setSbnrAlgo                (SbnrAlgo a);

// Central mode setter — called by SliceModel::setActiveNr via RadioModel.
// Flips SetRXA*Run for WDSP-backed slots; toggles post-WDSP filter
// booleans for DFNR/BNR/MNR. Only ONE slot is on after this returns.
void setActiveNr(NrSlot slot);
NrSlot activeNr() const { return m_activeNr.load(std::memory_order_acquire); }
```

Add private members:

```cpp
std::atomic<NrSlot> m_activeNr{NrSlot::Off};
Nr1Tuning m_nr1Tuning;
Nr2Tuning m_nr2Tuning;
Nr3Tuning m_nr3Tuning;
Nr4Tuning m_nr4Tuning;
```

- [ ] **Step 2: Implement `setActiveNr` in RxChannel.cpp**

Mirror Thetis `console.cs:43297-43450 SelectNR()` logic. Sample:

```cpp
void RxChannel::setActiveNr(NrSlot slot)
{
    m_activeNr.store(slot, std::memory_order_release);

#ifdef HAVE_WDSP
    // Set all four WDSP NR Run flags; exactly zero or one is on.
    // Byte-for-byte from Thetis console.cs:43297-43450 [v2.10.3.13].
    SetRXAANRRun(m_channelId,  (slot == NrSlot::NR1) ? 1 : 0);
    SetRXAEMNRRun(m_channelId, (slot == NrSlot::NR2) ? 1 : 0);
    SetRXARNNRRun(m_channelId, (slot == NrSlot::NR3) ? 1 : 0);
    SetRXASBNRRun(m_channelId, (slot == NrSlot::NR4) ? 1 : 0);
#endif

    // Post-WDSP filters — toggle their active flag; processIq reads m_activeNr
    // and dispatches. The filter instances stay alive for faster re-activation.
    m_dfnrActive.store(slot == NrSlot::DFNR, std::memory_order_release);
    m_bnrActive.store (slot == NrSlot::BNR,  std::memory_order_release);
    m_mnrActive.store (slot == NrSlot::MNR,  std::memory_order_release);
}
```

- [ ] **Step 3: Implement the tuning setters**

Each calls one or more WDSP setters. Sample NR1:

```cpp
void RxChannel::setAnrTuning(const Nr1Tuning& t)
{
    m_nr1Tuning = t;
#ifdef HAVE_WDSP
    // From Thetis setup.cs:udDSPNR1* handlers [v2.10.3.13].
    // UI Gain/Leak values (0-999 typically) passed through as-is per Thetis:
    // the WDSP setter takes `double` and applies its own internal scaling.
    SetRXAANRVals(m_channelId, t.taps, t.delay, t.gain, t.leakage);
    SetRXAANRPosition(m_channelId, static_cast<int>(t.position));
#endif
}
```

NR2 setter must include post2 cascade. Note the /100 scaling from Thetis setup.cs:34711:

```cpp
void RxChannel::setEmnrPost2Factor(double factor)
{
    m_nr2Tuning.post2Factor = factor;
#ifdef HAVE_WDSP
    // From Thetis setup.cs:34711 [v2.10.3.13] — UI int ÷ 100 before call.
    SetRXAEMNRpost2Factor(m_channelId, factor / 100.0);
#endif
}
```

Include all ~25 individual knob setters + 4 tuning-struct setters + `setActiveNr` in this step. Each inline-cites the Thetis handler source line.

- [ ] **Step 4: Build + run a smoke test**

```bash
cmake --build build -j$(nproc) --target NereusSDR 2>&1 | tail -20
```

Expected: compiles + links clean. Run the app and toggle `SliceModel::activeNr` from the Qt console or a temporary test hook; confirm no crashes.

- [ ] **Step 5: Commit**

```bash
git add src/core/RxChannel.h src/core/RxChannel.cpp
git commit -S -m "feat(rx): wire full Thetis NR1-4 setter surface on RxChannel

Adds ~25 per-knob setters + 4 per-stage tuning structs + central
setActiveNr() enforcing at-most-one-WDSP-NR-active. Mirrors Thetis
console.cs:43297-43450 SelectNR() byte-for-byte.

NR1: Taps/Delay/Gain/Leak/Position.
NR2: GainMethod/NPEMethod/T1/T2/AEFilter + full post2 cascade.
NR3: Position/UseDefaultGain (model path is global, loaded at startup).
NR4: Reduction/Smoothing/Whitening/Rescale/SNRthresh + Algo 1/2/3.

DFNR/BNR/MNR atomics wired but filter classes land in later tasks.

Cites: Thetis console.cs:43297, setup.cs:17354-34567, cmaster.c:43-68
       [v2.10.3.13]."
```

---

### Task 9: Port AetherSDR `DeepFilterFilter` for DFNR (48 kHz-native)

**Files:**
- Create: `src/core/DeepFilterFilter.{h,cpp}`
- Create: `third_party/deep-filter-sys/` (Rust crate + model tar.gz)
- Modify: `CMakeLists.txt` (cargo integration)

- [ ] **Step 1: Copy AetherSDR source, then modify for 48 kHz native**

```bash
cp /Users/j.j.boyd/AetherSDR/src/core/DeepFilterFilter.h src/core/DeepFilterFilter.h
cp /Users/j.j.boyd/AetherSDR/src/core/DeepFilterFilter.cpp src/core/DeepFilterFilter.cpp
```

- [ ] **Step 2: Rewrite the wrapper's I/O signature and remove internal resamplers**

Replace the AetherSDR wrapper's `QByteArray process(const QByteArray& pcm24kStereo)` method with:

```cpp
// NereusSDR runs DFNR post-WDSP-demod at the audio rate (48 kHz stereo float).
// DeepFilterNet3 expects 48 kHz mono float [-1.0, 1.0]. We take L channel,
// run DFNet3, and copy the result back to both L and R. No internal resample.
void process(float* stereo48, int frames);
```

Then inside, remove the r8brain resampler object members (`m_upsampler`, `m_downsampler`), remove the 24→48 and 48→24 internal resample calls, and directly feed 48 kHz frames to `df_process_frame`. The model already expects 48 kHz.

- [ ] **Step 3: Update the file header to document the modification**

In `DeepFilterFilter.h`, replace AetherSDR's modification/history block with:

```cpp
// =============================================================================
// Modification history (NereusSDR):
//   2026-04-2X — Imported from AetherSDR @ 0cd4559. MODIFIED: accepts 48 kHz
//                stereo float natively instead of AetherSDR's 24 kHz int16
//                stereo. Removed internal r8brain 24↔48 resampler pair
//                (AetherSDR.src/core/DeepFilterFilter.cpp:~30-110) because
//                NereusSDR's post-WDSP audio is already at 48 kHz matching
//                DeepFilterNet3's native rate. Saves 2 resample passes per
//                audio block. Algorithm (df_process_frame) and tuning
//                surface (attenLimit, postFilterBeta) unchanged.
//                Authored by J.J. Boyd (KG4VCF) with AI-assisted review
//                via Anthropic Claude Code.
// =============================================================================
```

This is the license protocol's intended use — algorithm preserved byte-for-byte, I/O shim modified with a loud "Modified" note.

- [ ] **Step 4: Vendor deep-filter-sys Rust crate**

```bash
mkdir -p third_party/deep-filter-sys
# Fetch the AetherSDR-pinned revision:
cp -r /Users/j.j.boyd/AetherSDR/third_party/deep-filter-sys/* third_party/deep-filter-sys/
# Verify LICENSE files (MIT + Apache-2.0) travel with it.
```

Include the DeepFilterNet3 model tar.gz — it lives next to the crate.

- [ ] **Step 5: CMake cargo integration**

Add to root `CMakeLists.txt`:

```cmake
option(ENABLE_DFNR "Build with DeepFilterNet3 neural noise reduction" ON)
if(ENABLE_DFNR)
    find_program(CARGO_EXECUTABLE cargo REQUIRED
        DOC "Rust cargo build tool — required for DFNR (ENABLE_DFNR=ON)")

    set(DFNR_CRATE_DIR ${CMAKE_SOURCE_DIR}/third_party/deep-filter-sys)
    set(DFNR_STATIC_LIB ${CMAKE_BINARY_DIR}/dfnr/release/libdeep_filter.a)

    add_custom_command(
        OUTPUT ${DFNR_STATIC_LIB}
        COMMAND ${CARGO_EXECUTABLE} build --release
                --target-dir ${CMAKE_BINARY_DIR}/dfnr
                --manifest-path ${DFNR_CRATE_DIR}/Cargo.toml
        WORKING_DIRECTORY ${DFNR_CRATE_DIR}
        COMMENT "Building DeepFilterNet3 (deep-filter-sys) via cargo"
        VERBATIM)
    add_custom_target(dfnr_crate DEPENDS ${DFNR_STATIC_LIB})
    add_library(dfnr_static STATIC IMPORTED)
    set_target_properties(dfnr_static PROPERTIES
        IMPORTED_LOCATION ${DFNR_STATIC_LIB})
    add_dependencies(dfnr_static dfnr_crate)

    target_link_libraries(NereusSDRObjs PUBLIC dfnr_static)
    target_include_directories(NereusSDRObjs PUBLIC
        ${DFNR_CRATE_DIR}/include)
    target_compile_definitions(NereusSDRObjs PUBLIC HAVE_DFNR)
endif()
```

- [ ] **Step 6: Wire DFNR into RxChannel**

Add to `RxChannel.h`:

```cpp
#ifdef HAVE_DFNR
std::unique_ptr<DeepFilterFilter> m_dfnr;
std::atomic<bool> m_dfnrActive{false};
#endif
```

In `RxChannel::processIq()`, after `fexchange2` writes to `m_audioOutBuf`, insert:

```cpp
#ifdef HAVE_DFNR
if (m_dfnrActive.load(std::memory_order_acquire) && m_dfnr) {
    m_dfnr->process(m_audioOutBuf, m_bufferSize);
}
#endif
```

Add setters `setDfnrAttenLimit(double dB)` + `setDfnrPostFilterBeta(double beta)` that forward to `m_dfnr->setAttenLimit()` and `m_dfnr->setPostFilterBeta()`.

- [ ] **Step 7: Build + commit**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -30
```

Expected: cargo builds deep-filter-sys (~3-8 min first time), links clean, HAVE_DFNR defined.

```bash
git add src/core/DeepFilterFilter.{h,cpp} third_party/deep-filter-sys/ \
        CMakeLists.txt src/core/RxChannel.{h,cpp}
git commit -S -m "feat(dfnr): port DeepFilterNet3 wrapper, 48 kHz native

Imports AetherSDR's DeepFilterFilter class and modifies it to accept
48 kHz stereo float directly (removed internal r8brain 24↔48 resample
pair). Saves 2 resample passes per audio block vs verbatim port.

CMake cargo integration: ENABLE_DFNR default ON on all platforms;
requires rustc + cargo in build environment. Model tar.gz bundled
in third_party/deep-filter-sys/.

RxChannel owns one DeepFilterFilter; activeNr=DFNR sets m_dfnrActive
atomic; processIq dispatches post-fexchange2.

Cites: AetherSDR src/core/DeepFilterFilter.{h,cpp} [@0cd4559]."
```

---

### Task 10: Port AetherSDR `NvidiaBnrFilter` for BNR (runtime-loaded, Windows+NVIDIA)

**Files:**
- Create: `src/core/NvidiaBnrFilter.{h,cpp}`
- Modify: `CMakeLists.txt`
- Modify: `src/core/RxChannel.{h,cpp}`

- [ ] **Step 1: Copy AetherSDR source**

```bash
cp /Users/j.j.boyd/AetherSDR/src/core/NvidiaBnrFilter.{h,cpp} src/core/
```

- [ ] **Step 2: Modify for 48 kHz native audio** (same pattern as Task 9 Step 2)

BNR in AetherSDR accepts int16 stereo 24 kHz; NVIDIA Broadcast SDK works at 48 kHz internally. Strip the AetherSDR resample shim; accept 48 kHz float stereo natively. Document in modification block.

- [ ] **Step 3: Runtime NVIDIA SDK loading**

AetherSDR loads the NVIDIA Broadcast SDK via `LoadLibraryA` at runtime (not linked at build time — EULA prevents redistribution). Preserve this byte-for-byte. Windows absence-of-SDK path already returns `isValid()==false` which we use to hide the BNR button.

- [ ] **Step 4: CMake Windows-only gate**

```cmake
option(ENABLE_BNR "Build with NVIDIA Broadcast SDK (Windows only)" ON)
if(ENABLE_BNR AND WIN32)
    target_sources(NereusSDRObjs PRIVATE
        src/core/NvidiaBnrFilter.cpp)
    target_compile_definitions(NereusSDRObjs PUBLIC HAVE_BNR)
    # No link-time SDK dep — runtime LoadLibraryA only.
endif()
```

- [ ] **Step 5: Wire into RxChannel**

Parallel to DFNR: `std::unique_ptr<NvidiaBnrFilter> m_bnr`, `std::atomic<bool> m_bnrActive`, dispatch in `processIq`, setters for Strength.

- [ ] **Step 6: Build on Windows runner** (CI will do this; locally smoke on macOS = no-op compile)

- [ ] **Step 7: Commit**

```bash
git add src/core/NvidiaBnrFilter.{h,cpp} CMakeLists.txt src/core/RxChannel.{h,cpp}
git commit -S -m "feat(bnr): port NVIDIA Broadcast SDK wrapper (Windows+NVIDIA)

Runtime-loaded via LoadLibraryA — no build-time SDK dep, respects
NVIDIA EULA's no-redistribute clause. Absent SDK → isValid()==false →
BNR button hidden in UI.

Modified to accept 48 kHz stereo float natively; removed AetherSDR's
24↔48 resample shim.

ENABLE_BNR=ON on Windows only; no-op elsewhere.

Cites: AetherSDR src/core/NvidiaBnrFilter.{h,cpp} [@0cd4559]."
```

---

### Task 11: Port AetherSDR `MacNRFilter` for MNR (macOS Accelerate)

**Files:**
- Create: `src/core/MacNRFilter.{h,cpp}`
- Modify: `CMakeLists.txt`
- Modify: `src/core/RxChannel.{h,cpp}`

- [ ] **Step 1: Copy AetherSDR source**

```bash
cp /Users/j.j.boyd/AetherSDR/src/core/MacNRFilter.{h,cpp} src/core/
```

- [ ] **Step 2: Modify for 48 kHz native** (Accelerate vDSP is rate-agnostic; strip resampler, accept 48 kHz)

- [ ] **Step 3: CMake macOS-only gate**

```cmake
option(ENABLE_MNR "Build with Apple Accelerate-based MNR (macOS only)" ON)
if(ENABLE_MNR AND APPLE)
    target_sources(NereusSDRObjs PRIVATE
        src/core/MacNRFilter.cpp)
    target_link_libraries(NereusSDRObjs PUBLIC "-framework Accelerate")
    target_compile_definitions(NereusSDRObjs PUBLIC HAVE_MNR)
endif()
```

- [ ] **Step 4: Wire into RxChannel** (parallel to DFNR/BNR)

- [ ] **Step 5: Commit**

```bash
git add src/core/MacNRFilter.{h,cpp} CMakeLists.txt src/core/RxChannel.{h,cpp}
git commit -S -m "feat(mnr): port Apple Accelerate MMSE-Wiener NR (macOS)

#ifdef __APPLE__ gated; links -framework Accelerate; no external deps.
Modified to accept 48 kHz stereo float natively.

ENABLE_MNR=ON on macOS only.

Cites: AetherSDR src/core/MacNRFilter.{h,cpp} [@0cd4559]."
```

---

### Task 12: Extend SliceModel — activeNr + all 7 filters' tuning Q_PROPERTYs

**Files:**
- Modify: `src/models/SliceModel.h` + `src/models/SliceModel.cpp`

- [ ] **Step 1: Replace old emnrEnabled stub with full NR surface**

Remove `m_emnrEnabled` + `setEmnrEnabled()` placeholders. Add:

```cpp
// Active NR slot — single source of truth. Mutual exclusion enforced here.
Q_PROPERTY(NrSlot activeNr READ activeNr WRITE setActiveNr NOTIFY activeNrChanged)

// ~35 tuning Q_PROPERTYs covering NR1 (Taps/Delay/Gain/Leak/Position) +
// NR2 (GainMethod/NPEMethod/T1/T2/AEFilter/post2 block) + NR3 (Position/
// UseDefaultGain) + NR4 (Reduction/Smoothing/Whitening/Rescale/SNRthresh/
// Algo) + DFNR (AttenLimit/PostFilterBeta) + BNR (Strength) + MNR (Strength).
Q_PROPERTY(int nr1Taps READ nr1Taps WRITE setNr1Taps NOTIFY nr1TapsChanged)
// … (enumerate all in full plan)
```

- [ ] **Step 2: Implement mutual-exclusion setter**

```cpp
void SliceModel::setActiveNr(NrSlot slot)
{
    if (m_activeNr == slot) return;
    m_activeNr = slot;
    emit activeNrChanged(slot);
    // RadioModel listens and pushes to RxChannel::setActiveNr(slot)
    // which flips the correct SetRXA*Run / m_*Active pattern.
}
```

- [ ] **Step 3: Session-level persistence (no per-band)**

Per Q10 user decision. Extend `saveToSettings()` / `restoreFromSettings()` with:

```cpp
void SliceModel::saveToSettings(...)
{
    // Active NR slot + all ~35 knobs, session-level per slice.
    // NO _<bandKey> suffix — user chose session-level per Q10.
    s.setValue(QStringLiteral("Slice%1/NrActive").arg(m_id),
               static_cast<int>(m_activeNr));
    s.setValue(QStringLiteral("Slice%1/Nr1Taps").arg(m_id), m_nr1Taps);
    // …
}
```

- [ ] **Step 4: Build + commit**

```bash
git add src/models/SliceModel.{h,cpp}
git commit -S -m "feat(slice): add activeNr + 35 NR tuning properties

Replaces the 3G-10 Stage 2 emnrEnabled stub with the full 7-filter NR
surface. activeNr is the single source of truth for mutual exclusion.

Persistence: session-level per slice (Slice<N>/NrActive + Slice<N>/<Knob>).
NR is the only SliceModel domain that skips per-band persistence — user
directive 2026-04-23: NR tuning is slice-level, not band-level.

Cites: Thetis console.cs:43297 SelectNR() [v2.10.3.13]."
```

---

### Task 13: Port `DspParamPopup` from AetherSDR

**Files:**
- Create: `src/gui/widgets/DspParamPopup.{h,cpp}`

- [ ] **Step 1: Copy byte-for-byte**

```bash
cp /Users/j.j.boyd/AetherSDR/src/gui/DspParamPopup.h  src/gui/widgets/DspParamPopup.h
cp /Users/j.j.boyd/AetherSDR/src/gui/DspParamPopup.cpp src/gui/widgets/DspParamPopup.cpp
```

- [ ] **Step 2: Update namespace + add modification block**

Change namespace from `AetherSDR::` to `NereusSDR::`. Append NereusSDR modification block.

- [ ] **Step 3: Add PROVENANCE + commit**

```bash
git add src/gui/widgets/DspParamPopup.{h,cpp} docs/attribution/THETIS-PROVENANCE.md
git commit -S -m "feat(ui): port DspParamPopup from AetherSDR

Floating popup widget for right-click quick-controls. Used by the 7 NR
buttons and by ANF/other DSP controls. Byte-for-byte from AetherSDR
@0cd4559 except namespace + modification block.

Cites: AetherSDR src/gui/DspParamPopup.{h,cpp} [@0cd4559]."
```

---

### Task 14: VfoWidget 7-button NR bank + right-click popup wiring

**Files:**
- Modify: `src/gui/widgets/VfoWidget.{h,cpp}`

- [ ] **Step 1: Remove old NR + NR2 placeholder toggles**

Delete `m_nrToggle` and `m_nr2Toggle` (lines ~957, ~967). Remove their grid placements.

- [ ] **Step 2: Add 7-button NR bank sub-HBox**

```cpp
// In the DSP grid building code, replace the two-button placement with:
auto* nrBank = new QWidget(this);
auto* nrHBox = new QHBoxLayout(nrBank);
nrHBox->setContentsMargins(0, 0, 0, 0);
nrHBox->setSpacing(1);

m_nr1Btn  = makeCompactToggle(QStringLiteral("NR1"));
m_nr2Btn  = makeCompactToggle(QStringLiteral("NR2"));
m_nr3Btn  = makeCompactToggle(QStringLiteral("NR3"));
m_nr4Btn  = makeCompactToggle(QStringLiteral("NR4"));
m_dfnrBtn = makeCompactToggle(QStringLiteral("DFNR"));
m_bnrBtn  = makeCompactToggle(QStringLiteral("BNR"));
m_mnrBtn  = makeCompactToggle(QStringLiteral("MNR"));

for (auto* b : {m_nr1Btn, m_nr2Btn, m_nr3Btn, m_nr4Btn, m_dfnrBtn, m_bnrBtn, m_mnrBtn}) {
    b->setMaximumWidth(28);  // ~22-28 px for a 7-button bank
    b->setCheckable(true);
    b->setContextMenuPolicy(Qt::CustomContextMenu);
    nrHBox->addWidget(b);
}

// Replaces the current (0,2) + (0,3) placement with the bank spanning both.
grid->addWidget(nrBank, 0, 2, 1, 2);
```

`makeCompactToggle` is a new helper using smaller font (10px), tight padding.

- [ ] **Step 3: Platform + mode gating**

```cpp
#ifndef HAVE_BNR
m_bnrBtn->hide();
#endif
#ifndef HAVE_MNR
m_mnrBtn->hide();
#endif
// BNR also hidden in FM mode — mirrors AetherSDR VfoWidget.cpp:2395 [@0cd4559].
// Wire to mode change signal to toggle visibility.
```

- [ ] **Step 4: Left-click = activate slot (mutex via SliceModel)**

```cpp
connect(m_nr1Btn, &QPushButton::toggled, this, [this](bool on) {
    if (!m_updatingFromModel) {
        emit activeNrRequested(on ? NrSlot::NR1 : NrSlot::Off);
    }
});
// Repeat for all 7 buttons.
```

MainWindow catches `activeNrRequested(NrSlot)` and calls `SliceModel::setActiveNr(slot)`, which emits `activeNrChanged`, which drives `syncFromModel()` → updates button checked states mutually-exclusively.

- [ ] **Step 5: Right-click = quick-controls popup**

```cpp
connect(m_nr1Btn, &QPushButton::customContextMenuRequested, this, [this](const QPoint& pos) {
    showNr1Popup(m_nr1Btn->mapToGlobal(pos));
});
// Repeat for all 7.
```

- [ ] **Step 6: Implement popup builders**

Seven `showNr<X>Popup(QPoint)` methods. Each builds a `DspParamPopup` with the Q6 quick controls + "More Settings…" button. See Task 15 for the full popup contents per button.

- [ ] **Step 7: Build + screenshot-verify compact layout**

```bash
cmake --build build -j$(nproc) && ./build/NereusSDR &
# Take screenshot; verify 7 buttons fit within the old NR+NR2 cell span
# without overflowing the flag width.
```

- [ ] **Step 8: Commit**

```bash
git add src/gui/widgets/VfoWidget.{h,cpp}
git commit -S -m "feat(vfo): 7-button NR bank on the compact flag

Replaces NR | NR2 placeholder cells with a tight-packed 7-button
QHBoxLayout holding NR1 NR2 NR3 NR4 DFNR BNR MNR at ~24 px each.
Spans columns 2-3 of the existing DSP grid; flag outer dimensions
unchanged per user compaction directive.

Platform gating: BNR hidden off-Windows; MNR hidden off-macOS.
Mode gating: BNR hidden in FM mode (mirrors AetherSDR VfoWidget
.cpp:2395 @0cd4559).

Left-click toggles slot (mutex via SliceModel.setActiveNr).
Right-click opens DspParamPopup with quick controls (Task 15)."
```

---

### Task 15: Implement 7 DspParamPopup builders — quick controls per filter

Per Q9: 3-5 knobs each, drawn from AetherSDR inspiration, tooltips on every control.

**Files:**
- Modify: `src/gui/widgets/VfoWidget.cpp` (impl of `showNr<X>Popup`)

- [ ] **Step 1: NR1 popup (Thetis-native quick controls)**

```cpp
void VfoWidget::showNr1Popup(QPoint globalPos)
{
    auto* p = new DspParamPopup(this);
    p->setWindowTitle(QStringLiteral("NR1 — Adaptive"));
    // From Thetis setup.cs udDSPNR1* ranges [v2.10.3.13]:
    p->addSlider("Taps",  16, 128, m_slice->nr1Taps(),   kNr1TapsTooltip);
    p->addSlider("Delay", 1, 256, m_slice->nr1Delay(),  kNr1DelayTooltip);
    p->addSlider("Gain",  0, 999, m_slice->nr1Gain(),   kNr1GainTooltip);
    p->addSlider("Leak",  0, 999, m_slice->nr1Leak(),   kNr1LeakTooltip);
    p->addRadio("Position", {"Pre-AGC", "Post-AGC"},
                static_cast<int>(m_slice->nr1Position()), kNr1PositionTooltip);
    p->addMoreSettingsButton([this]() { emit openNrSetupRequested(NrSlot::NR1); });
    connect(p, &DspParamPopup::valueChanged, this, [this](const QString& label, double v) {
        if (label == "Taps") m_slice->setNr1Taps(static_cast<int>(v));
        // … each label forwards to the correct SliceModel setter
    });
    p->popup(globalPos);
}
```

- [ ] **Step 2: NR2 popup**

Quick controls: AE Filter ✓, post2 Run ✓, post2 Factor, post2 Rate, "More Settings…" (per Q6). Plus GainMethod radio is worth including for quick Gamma↔Log switches (Thetis screenshot has this as a central radio group).

- [ ] **Step 3: NR3 popup**

Quick controls: Position radio, UseDefaultGain ✓, "Load Model…" button (opens file dialog), "Reset to Default" button, "More Settings…"

- [ ] **Step 4: NR4 popup**

Quick controls: Reduction slider, Smoothing slider, Algo radio (1/2/3), "More Settings…"

- [ ] **Step 5: DFNR popup** (from AetherSDR MainWindow.cpp:8227-8324)

AttenLimit (0-100), PostFilterBeta (0-30, displayed as /100 → 0.00-0.30), "More Settings…"

- [ ] **Step 6: BNR popup** (AetherSDR has no popup; design minimal)

Strength slider, "More Settings…"

- [ ] **Step 7: MNR popup** (AetherSDR has no popup; design minimal)

Strength slider, "More Settings…"

- [ ] **Step 8: Tooltip strings — centralize**

Define tooltip string constants (`kNr1TapsTooltip` etc.) in `src/gui/widgets/NrTooltips.h`. Per Q11, synthesize from Thetis `setup.designer.cs` ToolTip calls and AetherSDR's AetherDspDialog.cpp tooltips. For each control, note in the comment which source (Thetis / AetherSDR / both-synthesized). Example:

```cpp
// Synthesized from Thetis setup.designer.cs (no tooltip present for Taps)
// and AetherSDR AetherDspDialog.cpp (no NR1 tooltip either).
// Original Warren Pratt anr.h doc comment:
//   "filter length in taps"
constexpr auto kNr1TapsTooltip =
    "Number of taps in the LMS adaptive filter (16-128). "
    "Higher values give better noise tracking but more CPU. "
    "Default 64 is a good starting point for SSB.";
```

- [ ] **Step 9: Commit**

```bash
git add src/gui/widgets/VfoWidget.cpp src/gui/widgets/NrTooltips.h
git commit -S -m "feat(vfo): implement 7 DspParamPopup quick-controls popups

Each NR button's right-click popup carries 3-5 most-adjusted knobs
(drawn from AetherSDR MainWindow.cpp:7980-8324 inspiration) plus
'More Settings…' which routes to Setup → DSP → NR/ANF at that tab.

Tooltips synthesized from Thetis setup.designer.cs + AetherSDR
AetherDspDialog — source noted per-string in NrTooltips.h.

Cites: AetherSDR MainWindow.cpp:7980-8324 [@0cd4559],
       Thetis setup.designer.cs [v2.10.3.13]."
```

---

### Task 16: SpectrumOverlayPanel mirror NR row

Mirror the VFO flag's 7-button NR bank on the SpectrumOverlayPanel's DSP flyout, so users who hide the flag still have NR access from the spectrum view.

**Files:**
- Modify: `src/gui/SpectrumOverlayPanel.{h,cpp}`

- [ ] **Step 1: Add NR row to the DSP flyout**

Same 7-button pattern as VfoWidget Task 14. Same platform gating, same right-click → popup behavior.

- [ ] **Step 2: Share popup builder code**

Pull the `showNr<X>Popup` methods out of VfoWidget into a shared `NrPopupBuilder` class that both widgets use. Avoids duplication.

- [ ] **Step 3: Commit**

```bash
git add src/gui/SpectrumOverlayPanel.{h,cpp} src/gui/widgets/NrPopupBuilder.{h,cpp}
git commit -S -m "feat(spectrum-overlay): mirror 7-button NR row

Users who hide the VFO flag retain NR access from the spectrum overlay.
Same buttons, same popup behavior, same platform gating. Popup builder
extracted to a shared class."
```

---

### Task 17: Setup → DSP → NR/ANF page, sub-tabs per filter

Mirror the Thetis screenshot layout 1:1 for the WDSP filters (NR1/NR2/NR3/NR4); use AetherSDR's dialog tabs as reference for DFNR/BNR/MNR.

**Files:**
- Create: `src/gui/setup/NrAnfSetupPage.{h,cpp}`
- Modify: `src/gui/setup/SetupDialog.cpp` (register page)

- [ ] **Step 1: Page skeleton with sub-tabs**

```cpp
class NrAnfSetupPage : public SetupPage {
    QTabWidget* m_subtabs;
    // Sub-tab widgets, one per filter
    Nr1SetupSubtab*  m_nr1Tab;
    Nr2SetupSubtab*  m_nr2Tab;
    Nr3SetupSubtab*  m_nr3Tab;
    Nr4SetupSubtab*  m_nr4Tab;
    DfnrSetupSubtab* m_dfnrTab;
    BnrSetupSubtab*  m_bnrTab;
    MnrSetupSubtab*  m_mnrTab;
public:
    // Callable from MainWindow via Setup.selectPage("NR/ANF").
    // Optional second arg picks the sub-tab (e.g. "NR2").
    void selectSubtab(const QString& name);
};
```

- [ ] **Step 2: NR1 sub-tab — Thetis screenshot layout**

Two-column layout (RX1 column + RX2 column) — both columns bind to the currently-selected slice model, but side-by-side UI matches Thetis's per-RX design. Each column:

```
NR (or "NR RX2")
  Taps:  [ 64 ]  (spinbox)
  Delay: [ 16 ]
  Gain:  [100 ]
  Leak:  [100 ]

NRs/ANF Position
  ◉ Pre-AGC
  ◯ Post-AGC
```

Wire every valueChanged → `m_slice->setNr1Taps(v)` etc. Live-push per the standing feedback rule.

- [ ] **Step 3: NR2 sub-tab — full Thetis surface**

Two-column layout (RX1 + RX2). Each:

```
NR2
  Gain Method:
    ◯ Linear   ◯ Log   ◉ Gamma   ◯ Trained
    T1: [-0.5]
    T2: [ 0.20]
  NPE Method:
    ◉ OSMS   ◯ MMSE   ◯ NSTAT
  ☑ AE Filter
  ☐ Noise post proc
    Level:  [15.0]
    Factor: [15.0]
    Rate:   [ 5.0]
    Taper:  [  12]
```

- [ ] **Step 4: NR3 sub-tab — model selector**

```
NR3
  Model: [Default (large)]  ▼
  [Use Model]  [Def]
  ☑ Use fixed gain for input samples
```

"Use Model" opens an OpenFileDialog (filter `*.bin`), validates via 64-byte alignment per Thetis setup.cs:34659-34688, persists path in `Nr3ModelPath` AppSettings key, calls `RNNRloadModel(path)`. "Def" calls `RNNRloadModel("")` and restores the baked-in default.

- [ ] **Step 5: NR4 sub-tab — full Thetis surface**

```
NR4 RX1
  Reduction:  [10.0] dB
  Smoothing:  [65.0] %
  Whitening:  [ 2.0] %
  Rescale:    [ 2.0] db
  SNRthresh:  [-10.0] db
  ◯ Algo 1  ◉ Algo 2  ◯ Algo 3
```

Same for NR4 RX2.

- [ ] **Step 6: DFNR sub-tab — AetherSDR controls**

From AetherSDR AetherDspDialog.cpp:542-613.

```
DFNR
  Attenuation Limit: [100] dB
  Post-Filter Beta:  [  0] → 0.00
```

- [ ] **Step 7: BNR sub-tab**

AetherSDR BNR dialog had `Enable BNR` + `Strength` only. Mirror.

- [ ] **Step 8: MNR sub-tab**

AetherSDR MNR dialog had `Enable MNR` + `Strength` only. Mirror.

- [ ] **Step 9: Register page in SetupDialog**

```cpp
// In SetupDialog.cpp
m_pages.insert("NR/ANF", new NrAnfSetupPage(m_radio, this));
```

- [ ] **Step 10: Commit**

```bash
git add src/gui/setup/NrAnfSetupPage.{h,cpp} src/gui/setup/SetupDialog.cpp
git commit -S -m "feat(setup): Setup → DSP → NR/ANF page with 7 sub-tabs

Thetis-exact layout for NR1/NR2/NR3/NR4 sub-tabs (two-column per-RX,
matching the Thetis setup.designer.cs NR/ANF screenshot byte-for-byte).

DFNR/BNR/MNR sub-tabs follow AetherSDR AetherDspDialog.cpp layouts
since Thetis has no equivalents.

Live-push on every valueChanged per Sub-epic B lesson 7 (persisted-
but-ignored is a design anti-pattern).

Cites: Thetis setup.designer.cs NR/ANF page [v2.10.3.13],
       AetherSDR AetherDspDialog.cpp:542-613 [@0cd4559]."
```

---

### Task 18: Wire `openNrSetupRequested(NrSlot)` in MainWindow

**Files:**
- Modify: `src/gui/MainWindow.cpp`

- [ ] **Step 1: Add signal handler**

Mirror Sub-epic B's `openNbSetupRequested` pattern at MainWindow.cpp:~2420:

```cpp
connect(vfo, &VfoWidget::openNrSetupRequested, this, [this](NrSlot slot) {
    auto* dialog = new SetupDialog(m_radioModel, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->selectPage(QStringLiteral("NR/ANF"));
    // Sub-tab name matches NrSlot enum name:
    auto* nrPage = qobject_cast<NrAnfSetupPage*>(dialog->currentPage());
    if (nrPage) {
        const QString subtab = slotToSubtabName(slot);  // "NR1", "NR2", …, "MNR"
        nrPage->selectSubtab(subtab);
    }
    dialog->show();
});
```

- [ ] **Step 2: Same for SpectrumOverlayPanel signal**

- [ ] **Step 3: Commit**

```bash
git add src/gui/MainWindow.cpp
git commit -S -m "feat(main-window): route openNrSetupRequested(NrSlot) → Setup

Mirrors Sub-epic B's openNbSetupRequested pattern. 'More Settings…'
clicks from any of the 7 DspParamPopup instances open Setup → DSP →
NR/ANF scrolled to the matching sub-tab."
```

---

### Task 19: RadioModel — push NR state on slice activate + connect

**Files:**
- Modify: `src/models/RadioModel.cpp`

- [ ] **Step 1: Add push-on-slice-change**

In `RadioModel::setActiveSlice(...)`, after the existing NB/AGC/filter pushes, add:

```cpp
// Push full NR state to the active slice's RxChannel.
auto* ch = m_receivers->channelForSlice(m_activeSliceId);
if (ch) {
    ch->setActiveNr(slice->activeNr());
    ch->setAnrTuning(slice->nr1Tuning());
    ch->setEmnrTuning(slice->nr2Tuning());
    ch->setRnnrTuning(slice->nr3Tuning());
    ch->setSbnrTuning(slice->nr4Tuning());
    ch->setDfnrAttenLimit(slice->dfnrAttenLimit());
    ch->setDfnrPostFilterBeta(slice->dfnrPostFilterBeta());
    ch->setBnrStrength(slice->bnrStrength());
    ch->setMnrStrength(slice->mnrStrength());
}
```

- [ ] **Step 2: On radio connect, restore global NR3 model path**

```cpp
// Immediately after radio-connect succeeds:
const QString modelPath = AppSettings::instance().value("Nr3ModelPath", "").toString();
RNNRloadModel(modelPath.toStdString().c_str());
```

- [ ] **Step 3: Commit**

```bash
git add src/models/RadioModel.cpp
git commit -S -m "feat(radio-model): push NR state on slice activate + connect

Full NR surface synced from SliceModel to RxChannel on every slice
change. Global NR3 model path restored on radio connect."
```

---

### Task 20: Unit tests

**Files:**
- Create: `tests/tst_slice_nr_mutex.cpp`
- Create: `tests/tst_slice_nr_persistence.cpp`
- Create: `tests/tst_rx_channel_nr_wiring.cpp`
- Create: `tests/tst_nr_dfnr_bypass.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Mutual-exclusion test**

```cpp
void TestSliceNrMutex::activatingNr1DisablesOthers() {
    SliceModel s(0);
    s.setActiveNr(NrSlot::NR2);
    QCOMPARE(s.activeNr(), NrSlot::NR2);
    s.setActiveNr(NrSlot::NR1);
    QCOMPARE(s.activeNr(), NrSlot::NR1);
    // All others must now report Off (there is no "other activeNr" in the
    // model, but we verify the enum transition and emitted signal sequence).
    QList<QVariant> args = captureSignalArgs(...);
    QCOMPARE(args.last(), QVariant::fromValue(NrSlot::NR1));
}
```

Plus tests for: Off→NR1, NR1→Off, cycling through all 7 slots.

- [ ] **Step 2: Persistence test**

```cpp
void TestSliceNrPersistence::sessionRoundTrip() {
    SliceModel s(0);
    s.setActiveNr(NrSlot::NR2);
    s.setNr1Taps(96);
    s.setNr2Post2Factor(17.5);
    s.setNr4ReductionAmount(12.0);
    s.saveToSettings();

    SliceModel s2(0);
    s2.restoreFromSettings();
    QCOMPARE(s2.activeNr(), NrSlot::NR2);
    QCOMPARE(s2.nr1Taps(), 96);
    QCOMPARE(s2.nr2Post2Factor(), 17.5);
    QCOMPARE(s2.nr4ReductionAmount(), 12.0);
}
```

Plus: no band suffix appears in keys (per Q10 session-level decision); all 7 filters round-trip; corrupted values fall back to defaults.

- [ ] **Step 3: RxChannel wiring test (fake WDSP backend)**

```cpp
void TestRxChannelNrWiring::setActiveNrCallsCorrectRunSetters() {
    FakeWdspBackend fake;
    RxChannel ch(42, 48000, &fake);
    ch.setActiveNr(NrSlot::NR2);
    QCOMPARE(fake.lastSetRXAANRRun,  (QPair<int,int>{42, 0}));
    QCOMPARE(fake.lastSetRXAEMNRRun, (QPair<int,int>{42, 1}));
    QCOMPARE(fake.lastSetRXARNNRRun, (QPair<int,int>{42, 0}));
    QCOMPARE(fake.lastSetRXASBNRRun, (QPair<int,int>{42, 0}));
    // DFNR/BNR/MNR atomics stay false.
    QVERIFY(!ch.dfnrActive());
}
```

- [ ] **Step 4: DFNR bypass-correctness**

Seed sine + pink noise audio. Confirm that DFNR off → output == input bit-for-bit. DFNR on → output differs (non-trivially reduced noise floor; sine peak preserved within ±0.5 dB).

- [ ] **Step 5: Register in CMakeLists + run**

```bash
ctest --test-dir build -R "tst_slice_nr|tst_rx_channel_nr|tst_nr_dfnr"
```

Expected: all pass.

- [ ] **Step 6: Commit**

```bash
git add tests/tst_slice_nr_mutex.cpp tests/tst_slice_nr_persistence.cpp \
        tests/tst_rx_channel_nr_wiring.cpp tests/tst_nr_dfnr_bypass.cpp \
        tests/CMakeLists.txt
git commit -S -m "test: add Sub-epic C-1 NR coverage

Mutual exclusion, session-level persistence round-trip, RxChannel
WDSP setter wiring via fake backend, DFNR bypass correctness on
seeded audio. Four test binaries, ~28 test cases total."
```

---

### Task 21: CI workflow — Rust toolchain on all platforms

**Files:**
- Modify: `.github/workflows/ci.yml`
- Modify: `.github/workflows/release.yml`

- [ ] **Step 1: Rust install step (all 3 OS jobs)**

```yaml
- name: Install Rust toolchain (for DeepFilterNet3)
  uses: dtolnay/rust-toolchain@stable
```

Cache cargo artifacts:

```yaml
- uses: Swatinem/rust-cache@v2
  with:
    workspaces: third_party/deep-filter-sys -> target
```

- [ ] **Step 2: libspecbleach + rnnoise — no new CI deps (vendored)**

Confirm CMake configure-time succeeds on a clean runner. No apt/brew install needed since sources are vendored.

- [ ] **Step 3: NVIDIA SDK absence handling (Windows CI)**

Runners don't have NVIDIA SDK installed. BNR's runtime `LoadLibraryA` returns null; `isValid()==false`; button hidden. No build failure, no test failure.

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/ci.yml .github/workflows/release.yml
git commit -S -m "ci: add Rust toolchain + cargo cache for DeepFilterNet3

All three OS jobs install stable rustc + cargo and cache the
deep-filter-sys target/ directory. First build ~3-8 min; cached
rebuilds <30 sec."
```

---

### Task 22: Release packaging — bundle models per platform

**Files:**
- Modify: `.github/workflows/release.yml`
- Modify: platform-specific packaging scripts (AppImage recipe, DMG recipe, NSIS script)

- [ ] **Step 1: Linux AppImage**

Include `third_party/rnnoise/models/Default_large.bin` in `usr/share/NereusSDR/models/rnnoise/`. Include DeepFilterNet3 model tar.gz extracted to `usr/share/NereusSDR/models/dfnet3/`. Code resolves paths via `QStandardPaths::AppDataLocation`.

- [ ] **Step 2: macOS DMG**

Bundle models in `Contents/Resources/models/`. Code resolves via `QCoreApplication::applicationDirPath() + "/../Resources/models/"`.

- [ ] **Step 3: Windows portable ZIP + NSIS**

Bundle models in `models/` next to the executable.

- [ ] **Step 4: Code that resolves model paths**

Add `src/core/ModelPaths.h` with a helper that returns the correct path per platform and per model. Used by `RadioModel` on connect to call `RNNRloadModel(rnnoiseModelPath())` + DFNR wrapper to locate DFNet3 tar.gz.

- [ ] **Step 5: Commit**

```bash
git add .github/workflows/release.yml .github/workflows/appimage-recipe.yml \
        scripts/macos-bundle.sh installer/windows/nereussdr.nsi \
        src/core/ModelPaths.{h,cpp}
git commit -S -m "packaging: bundle rnnoise + DFNet3 models per platform

Linux AppImage → usr/share/NereusSDR/models/
macOS DMG     → Contents/Resources/models/
Windows       → <install>/models/

ModelPaths helper resolves the right path per platform.
~18 MB added to each artifact (15 MB DFNet3 + ~85 KB rnnoise default)."
```

---

### Task 23: Manual verification matrix

**Files:**
- Create: `docs/architecture/phase3g-rx-epic-c1-verification/README.md`

- [ ] **Step 1: Write matrix in the 3G-8 style**

Categories:

**§A. VFO flag layout (per platform):**
- A1. Linux: 5 visible buttons (NR1/NR2/NR3/NR4/DFNR); BNR/MNR hidden
- A2. Windows no-NVIDIA: 5 visible (no BNR, no MNR)
- A3. Windows w/ NVIDIA RTX 4000+: 6 visible (BNR present)
- A4. macOS: 6 visible (MNR present, no BNR)
- A5. Buttons fit within the old NR|NR2 cell span; flag width unchanged

**§B. Mutual exclusion:**
- B1. Click NR1 → NR2-7 all off
- B2. Click NR4 while NR2 active → NR2 off, NR4 on
- B3. Click active button → Off
- B4. Spectrum overlay NR bank mirrors flag state

**§C. WDSP pipeline (audible — requires radio):**
- C1. NR1 on, noisy band → noise reduced on voice peaks
- C2. NR2 with AE Filter on → fewer musical artifacts
- C3. NR2 with post2 Factor 25 → more aggressive reduction audible
- C4. NR3 with Default model → voice-centric denoising
- C5. NR3 with user-loaded .bin → model swap audible
- C6. NR4 Algo 1 vs Algo 2 vs Algo 3 → different tonality
- C7. Position Pre-AGC vs Post-AGC → AGC behaves differently (NR before vs after AGC decision)

**§D. Right-click popups:**
- D1. Each of 7 buttons opens a DspParamPopup
- D2. Slider changes push live (audible immediately)
- D3. "More Settings…" opens Setup → DSP → NR/ANF at matching sub-tab
- D4. Tooltips visible on every control

**§E. Setup page — Thetis parity:**
- E1. NR1 sub-tab matches screenshot: Taps/Delay/Gain/Leak + Position + RX1/RX2 columns
- E2. NR2 sub-tab: Gain Method radios + T1/T2 + NPE Method radios + AE Filter + post2 group
- E3. NR3 sub-tab: Model selector + Use Model / Def buttons + UseDefaultGain checkbox
- E4. NR4 sub-tab: 5 spinboxes + Algo 1/2/3 radio + RX1/RX2 columns

**§F. Persistence:**
- F1. Activate NR2, tune post2 Factor to 18, quit, relaunch → NR2 still active, post2 Factor still 18
- F2. No band suffix in keys (session-level per Q10)
- F3. NR3 model path persists globally across launches
- F4. Per-slice independence: slice 0 with NR1, slice 1 with NR4 → independent after relaunch

**§G. Platform-specific:**
- G1. Windows w/ NVIDIA: BNR button present, clicking enables actual denoising
- G2. Windows no-NVIDIA: BNR button hidden (not just disabled)
- G3. macOS: MNR button present, clicking enables Accelerate-based NR
- G4. Linux: DFNR works end-to-end (model loads from AppImage payload)

**§H. Licensing artifacts:**
- H1. AboutDialog lists rnnoise (BSD) + libspecbleach (LGPL-2.1) + DeepFilterNet3 (MIT/Apache-2.0)
- H2. LGPL-COMPLIANCE.md renders user-facing relink instructions
- H3. `verify-thetis-headers.py` passes on all new wdsp/ files
- H4. `verify-inline-tag-preservation.py` passes on all `// From Thetis` cites

- [ ] **Step 2: Commit**

```bash
git add docs/architecture/phase3g-rx-epic-c1-verification/README.md
git commit -S -m "docs: Sub-epic C-1 manual verification matrix

40+ verification items across 8 categories covering layout, mutex,
WDSP audibility, popups, Setup-page Thetis parity, persistence,
per-platform gating, and licensing artifacts."
```

---

### Task 24: CHANGELOG + CONTRIBUTING.md update

**Files:**
- Modify: `CHANGELOG.md`
- Modify: `CONTRIBUTING.md`

- [ ] **Step 1: CHANGELOG entry under [Unreleased]**

```markdown
### Added — Phase 3G RX Experience Epic Sub-epic C-1

- Full 7-filter noise reduction stack: NR1 (ANR, WDSP), NR2 (EMNR incl. post2 cascade), NR3 (RNNR/rnnoise w/ user-loadable models), NR4 (SBNR/libspecbleach), DFNR (DeepFilterNet3), BNR (NVIDIA Broadcast, Windows+NVIDIA only), MNR (Apple Accelerate, macOS only).
- Setup → DSP → NR/ANF page with 7 sub-tabs mirroring the Thetis setup.designer.cs layout byte-for-byte.
- Right-click on any NR button → DspParamPopup with 3-5 quick-control knobs + "More Settings…" entry point.
- Per-slice NR independence; session-level persistence (tuning not per-band).
- Vendored rnnoise (BSD) and libspecbleach (LGPL-2.1) into WDSP build.
- Ported Thetis `rnnr.c` + `sbnr.c` (GPLv2+ + MW0LGE dual-license) into third_party/wdsp/src/.
- DeepFilterNet3 model + rnnoise default model bundled per platform (AppImage / DMG / NSIS).

### Build

- Rust/cargo required on all platforms for DFNR. CI installs dtolnay/rust-toolchain@stable.
- ~18 MB added to release artifacts (DFNet3 + rnnoise models).
```

- [ ] **Step 2: CONTRIBUTING.md deps section**

Add Rust to the build dependency list; note that libspecbleach + rnnoise are vendored (no apt/brew install).

- [ ] **Step 3: Commit**

```bash
git add CHANGELOG.md CONTRIBUTING.md
git commit -S -m "docs: CHANGELOG + CONTRIBUTING for Sub-epic C-1

Unreleased section gets the 7-filter NR entry. CONTRIBUTING gains
Rust/cargo as a required build dep."
```

---

### Task 25: Final smoke test + PR

- [ ] **Step 1: Full rebuild + test run**

```bash
rm -rf build && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc) 2>&1 | tee /tmp/c1-final-build.log
ctest --test-dir build --output-on-failure 2>&1 | tee /tmp/c1-final-test.log
```

Expected: zero new warnings, all tests pass.

- [ ] **Step 2: Launch + eyeball-verify the flag layout**

```bash
pkill -f NereusSDR; sleep 1
./build/NereusSDR &
# Screenshot; compare against docs/architecture/phase3g-rx-epic-c1-verification/
```

- [ ] **Step 3: Run the full verification matrix**

Tick off each item in `docs/architecture/phase3g-rx-epic-c1-verification/README.md`. Open an issue for any failed items.

- [ ] **Step 4: Open PR**

```bash
git push -u origin claude/phase3g-rx-epic-c1-nr-xplat
gh pr create --title "Phase 3G RX Epic Sub-epic C-1: 7-filter NR stack (Thetis-parity)" \
  --body "$(cat <<'EOF'
## Summary
- 7-filter NR stack: NR1/NR2/NR3/NR4 (WDSP stages, full Thetis parity incl. post2 cascade) + DFNR/BNR/MNR (post-WDSP external)
- Setup → DSP → NR/ANF page byte-for-byte matches the Thetis screenshot layout
- Right-click → DspParamPopup quick-controls per button; "More Settings…" routes to Setup sub-tab
- Per-slice mutual exclusion via SliceModel::setActiveNr
- Session-level persistence (not per-band)
- rnnoise (BSD) + libspecbleach (LGPL-2.1) vendored; DFNet3 via Rust cargo
- Rust toolchain now required; CI updated

## Upstream
- Thetis v2.10.3.13 @ 501e3f51 — NR1-4 setter surface, screenshot layout, tooltips for NR1-4
- AetherSDR main @ 0cd4559 — DFNR/BNR/MNR wrappers, DspParamPopup, tooltips for DFNR/BNR/MNR

## Test plan
- [ ] All 40+ items in docs/architecture/phase3g-rx-epic-c1-verification/
- [ ] NR1-4 audible on real radio per Thetis screenshot defaults
- [ ] DFNR/BNR/MNR per-platform sanity check
- [ ] Settings round-trip across app restarts
- [ ] Pre-commit header verifiers pass

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

- [ ] **Step 5: Post PR link back to the handoff chat for review**

---

## Self-Review

Ran a fresh-eyes pass over the plan against the design brief + user directives from 2026-04-23 interview:

**Spec coverage:**
- ✅ All 7 filters wired (Q2)
- ✅ WDSP-native for NR1-4 (Q1)
- ✅ License verified inline (Q3)
- ✅ Setup → DSP → NR/ANF page (Q4)
- ✅ NR3 baked + user-loadable model (Q5)
- ✅ SliceModel::setActiveNr mutex (Q6)
- ✅ Per-slice/per-channel (Q7)
- ✅ Session-level persistence (Q8)
- ✅ 3-5 knob popups drawn from AetherSDR (Q9)
- ✅ 7-button flag, platform-gated (Q10)
- ✅ Tooltips from both sources (Q11)

**Placeholder scan:**
- Found: Task 5 Step 1 says "Check which rnnoise revision Thetis uses" — this is an engineer-to-verify step, not a placeholder. Acceptable per plan pattern.
- Found: Task 22 Step 4 says "Code resolves paths via QStandardPaths::AppDataLocation" — concrete API, not placeholder.
- No "TBD", "TODO", or "fill in" found.

**Type consistency:**
- `NrSlot::NR1` used consistently (not `NrSlot::NR`).
- `SetRXA*Run` spelled consistently across tasks 2, 8, 19.
- `m_activeNr` atomic used consistently.

**Known risks / open items to validate during execution:**
1. rnnoise revision-pinning to Thetis's — verify at Task 5 Step 1; if Thetis doesn't ship its own rnnoise subtree, fall back to upstream tagged release.
2. libspecbleach include path (`<specbleach_adenoiser.h>` vs `<libspecbleach/specbleach_adenoiser.h>`) — adjust Task 7 Step 1 based on what the vendored source provides.
3. Thetis `udDSPNR1` UI gain/leak scaling factor — screenshot shows "100" as the default value, but WDSP `SetRXAANRGain(channel, double gain)` takes a typical value like 1e-5; the Thetis UI scales UI int → WDSP double at the handler boundary (likely `/ 1e7`). Task 8 Step 3 needs to verify the exact Thetis scaling per `setup.cs:udDSPNR1Gain_ValueChanged` before landing.

---

## Execution Handoff

Plan complete and saved to `docs/architecture/phase3g-rx-epic-c1-nr-xplat-plan.md`. Two execution options:

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration. Good fit given the plan's breadth and the number of independent ports.

**2. Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints for review.

Which approach?
