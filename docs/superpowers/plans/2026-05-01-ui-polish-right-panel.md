# UI Polish — Right-Panel Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Land Phase C of the UI polish design — TxApplet density cleanup (wire SWR gauge, remove ATU/MEM/Tune Mode/DUP/xPA NYI rows, swap TUNE+MOX above VOX+MON) and ghost-applet stack hiding (8 of 13 right-panel applets gated in MainWindow init until their respective features ship).

**Architecture:** Two scoped surface fixes. C1 is a single-file edit (TxApplet.cpp) — wire one line of the existing forward-power lambda to drive the SWR gauge; delete 5 NYI rows; reorder buttons. C2 gates ghost applets at the construction call sites in MainWindow.cpp:1497-1514 (where each ghost applet is `new`'d and `panel->addApplet()` is called); files stay in the tree, just one-liner gating.

**Tech Stack:** Qt 6 / C++20. Depends on Plan 1 (Foundation) for canonical palette and applet panel gutter. No new classes; minimal new code.

---

## Reference

- **Design doc:** `docs/architecture/ui-audit-polish-plan.md` — Phase C (C1 + C2).
- **Prerequisite:** Plan 1 (Foundation) lands first.
- **Key code paths:**
  - `src/gui/applets/TxApplet.cpp:707` — existing `RadioStatus::powerChanged` connect with EMA on forward power; SWR is the third arg, currently unused.
  - `src/gui/applets/TxApplet.cpp:240, 249` — SWR gauge construction with `NyiOverlay::markNyi(swrGauge, "Phase 3I-1")` flag to remove.
  - `src/gui/MainWindow.cpp:1497-1514` — ghost-applet construction call sites.
  - `src/gui/applets/PhoneCwApplet.cpp` — CW tab body to stub.

---

## File-Structure Overview

### Files modified
- `src/gui/applets/TxApplet.cpp` (C1: wire SWR gauge, remove 5 NYI rows, reorder buttons)
- `src/gui/MainWindow.cpp` (C2: gate 8 ghost applets in `panel->addApplet()` calls)
- `src/gui/applets/PhoneCwApplet.cpp` (C2: stub CW tab body)

### Tests added
- None — TxApplet and MainWindow changes are visual; existing tests verify the model side.

---

## Tasks

### Task 1: Branch checkpoint after Plan 2 lands

- [ ] **Step 1: Verify Plans 1 + 2 commits**

```bash
git log --oneline | head -50 | grep -cE "ui\(|style\(|model\(|test\("
```

- [ ] **Step 2: Run baseline tests**

```bash
ctest --test-dir build --output-on-failure 2>&1 | tail -10
```

---

### Task 2: C1 — Wire SWR gauge

**Files:**
- Modify: `src/gui/applets/TxApplet.cpp:240, :249, :707`

- [ ] **Step 1: Remove the NyiOverlay flag on the SWR gauge**

Find line 249:
```cpp
NyiOverlay::markNyi(swrGauge, QStringLiteral("Phase 3I-1"));
```

Delete this line.

- [ ] **Step 2: Wire SWR into the existing powerChanged lambda**

At line 707, the existing connect is:
```cpp
connect(&m_model->radioStatus(), &RadioStatus::powerChanged,
        this, [this](float fwd, float refl, float swr) {
    // ... existing EMA on m_fwdPowerGauge using fwd ...
});
```

Inside the lambda, add `m_swrGauge->setValue(swr)` (no EMA needed; SWR can update at the natural 20 Hz rate from `RadioStatus`):

```cpp
connect(&m_model->radioStatus(), &RadioStatus::powerChanged,
        this, [this](float fwd, float refl, float swr) {
    // (existing fwd EMA + setValue)
    m_swrGauge->setValue(static_cast<double>(swr));
});
```

- [ ] **Step 3: Build + run with TX engaged**

```bash
cmake --build build -j$(nproc) --target NereusSDR
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
```

Engage MOX (mic or 2-Tone). Verify SWR gauge shows live SWR (1.0+ depending on antenna match).

- [ ] **Step 4: Commit**

```bash
git add src/gui/applets/TxApplet.cpp
git commit -m "$(cat <<'EOF'
ui(tx-applet): wire SWR gauge to existing powerChanged lambda

The SWR data was already flowing through RadioStatus::powerChanged
(third argument). The forward-power gauge connect at line 707 read
only the first argument, ignoring SWR. Added m_swrGauge->setValue(swr)
inside the same lambda + removed the NyiOverlay::markNyi flag.

Per docs/architecture/ui-audit-polish-plan.md §C1.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: C1 — Verify SWR Prot LED data path; wire or remove

**Files:**
- Modify: `src/gui/applets/TxApplet.cpp` (SWR Prot LED row)

- [ ] **Step 1: Check if SWR-protection-tripped data flows through RadioStatus**

```bash
grep -n "swrProt\|swrTripped\|protectionTripped" src/core/RadioStatus.{h,cpp} src/core/RadioConnection.{h,cpp} 2>/dev/null
```

- [ ] **Step 2a (if data exists): wire the LED**

Connect a signal in `RadioStatus` (e.g. `swrProtectionTripped(bool)`) to the LED's `setLit` method. Remove the NYI flag.

- [ ] **Step 2b (if data doesn't exist): remove the LED row**

If no protection-tripped flag flows from the radio, delete the LED construction in `TxApplet.cpp:657`. It returns when the underlying feature ships.

- [ ] **Step 3: Build + visual**

If wired: trigger high SWR → LED lights up. If removed: row is gone.

- [ ] **Step 4: Commit**

```bash
git add src/gui/applets/TxApplet.cpp src/core/RadioStatus.{h,cpp}  # if signal added
git commit -m "ui(tx-applet): wire SWR Prot LED to RadioStatus signal (or remove if data unavailable)

Per docs/architecture/ui-audit-polish-plan.md §C1.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: C1 — Remove NYI placeholder rows (ATU, MEM, Tune Mode, DUP, xPA)

**Files:**
- Modify: `src/gui/applets/TxApplet.cpp:518, :527, :563, :615, :632`

- [ ] **Step 1: Locate and delete each row**

```bash
grep -n "ATU\|MEM\|Tune Mode\|DUP\|xPA" src/gui/applets/TxApplet.cpp | head -10
```

For each row (ATU button, MEM button, Tune Mode combo, DUP button, xPA button), delete:
- The widget construction (button/combo)
- The row layout addition (`row->addWidget(...)` or layout->addLayout/addRow)
- Any associated `NyiOverlay::markNyi(...)` calls
- Any leftover member variable declarations in `TxApplet.h` if no other code references them

- [ ] **Step 2: Build + visual**

TxApplet body is ~80 px shorter. The 5 NYI rows are gone.

- [ ] **Step 3: Commit**

```bash
git add src/gui/applets/TxApplet.{h,cpp}
git commit -m "$(cat <<'EOF'
ui(tx-applet): remove NYI ATU / MEM / Tune Mode / DUP / xPA rows

Per docs/architecture/ui-audit-polish-plan.md §C1, these placeholders
returned no underlying functionality. Each will be re-added when its
feature phase ships:
- ATU + Tune Mode → ATU phase (no plan yet)
- MEM → channel-memory phase
- DUP → full-duplex audio routing phase
- xPA → external-PA hardware-specific phase

PS-A button (NYI until 3M-4 PureSignal) stays hidden — already
conditionally hidden via existing logic.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: C1 — Reorder TxApplet rows: TUNE + MOX above VOX + MON

**Files:**
- Modify: `src/gui/applets/TxApplet.cpp` (row layout in main `setupUi` or equivalent)

- [ ] **Step 1: Identify the row construction order**

Read TxApplet's main UI builder function. Today's order (per inventory):
1. Mic-source badge
2. FWD power gauge
3. SWR gauge (now wired per Task 2)
4. RF Power slider
5. Tune Power slider
6. VOX + MON toggles
7. Monitor volume slider
8. LEV + EQ + CFC toggles
9. TUNE + MOX buttons
10. TX Profile combo
11. 2-Tone

- [ ] **Step 2: Move the TUNE + MOX row above VOX + MON**

Final order (per design doc §C1 update):
1. Mic-source badge
2. FWD power gauge
3. SWR gauge
4. RF Power slider
5. Tune Power slider
6. **TUNE + MOX** (moved up)
7. VOX + MON toggles
8. Monitor volume slider
9. LEV + EQ + CFC toggles
10. TX Profile combo
11. 2-Tone

This is a layout change — move the `vbox->addLayout(tuneMoxRow)` call (or equivalent) from after LEV/EQ/CFC to before VOX/MON.

- [ ] **Step 3: Build + visual**

TxApplet now reads top-to-bottom: meters → power sliders → primary action (TUNE/MOX) → secondary modulation (VOX/MON, monitor volume) → processor toggles (LEV/EQ/CFC) → profile + filter cluster → test signal. Cleaner action-button hierarchy.

- [ ] **Step 4: Commit**

```bash
git add src/gui/applets/TxApplet.cpp
git commit -m "$(cat <<'EOF'
ui(tx-applet): swap TUNE + MOX above VOX + MON for action-button prominence

Per docs/architecture/ui-audit-polish-plan.md §C1 (decision 6, added
during walkthrough). Promotes the primary TX action buttons (TUNE,
MOX) to a more prominent position. Final row order reads:
meters → power → primary action → modulation → processors → profile
→ test signal.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: C2 — Gate 8 ghost applets in MainWindow

**Files:**
- Modify: `src/gui/MainWindow.cpp:1497-1514` (per design doc reference)

- [ ] **Step 1: Read the construction block**

```bash
sed -n '1490,1520p' src/gui/MainWindow.cpp
```

Expected: a sequence of `m_xApplet = new XApplet(...); panel->addApplet(m_xApplet);` for each applet — Eq, Digital, PureSignal, Diversity, Cwx, Dvk, Cat, Tuner. Plus EqApplet at :1497-1498 already explicitly added. FmApplet (the other ghost) may live elsewhere — verify.

- [ ] **Step 2: Comment out or guard the ghost-applet add calls**

Choose ONE of:
- **Option A (cleanest): comment out** the `new XApplet(...)` and `panel->addApplet(...)` lines for Eq/FM/Digital/PureSignal/Diversity/Cwx/Dvk/Cat/Tuner. Leave a `// TODO 3X-Y: re-enable when feature ships` comment per applet so the future re-enable is one-line.

- **Option B (gated): introduce a config flag** `#if NEREUSSDR_SHOW_GHOST_APPLETS` block and gate at compile time.

Per design doc decision: comment them out (uncomment when feature ships, per phase). Don't introduce a runtime config (defaults are debated; comment is unambiguous).

```cpp
// Ghost applets — hidden per docs/architecture/ui-audit-polish-plan.md §C2.
// Uncomment each when its feature phase ships.
//
// m_eqApplet = new EqApplet(m_radioModel, nullptr);            // 3I-3 (TX/RX EQ wiring)
// panel->addApplet(m_eqApplet);
// m_fmApplet = new FmApplet(m_radioModel, nullptr);            // 3I-3 (FM controls)
// panel->addApplet(m_fmApplet);
// m_digitalApplet = new DigitalApplet(m_radioModel, nullptr);  // 3-VAX
// panel->addApplet(m_digitalApplet);
// m_pureSignalApplet = new PureSignalApplet(m_radioModel, nullptr);  // 3M-4 (PureSignal)
// panel->addApplet(m_pureSignalApplet);
// m_diversityApplet = new DiversityApplet(m_radioModel, nullptr);    // 3F (multi-RX)
// panel->addApplet(m_diversityApplet);
// m_cwxApplet = new CwxApplet(m_radioModel, nullptr);          // 3I-2 (CW keyer)
// panel->addApplet(m_cwxApplet);
// m_dvkApplet = new DvkApplet(m_radioModel, nullptr);          // 3I-1 (DVK)
// panel->addApplet(m_dvkApplet);
// m_catApplet = new CatApplet(m_radioModel, nullptr);          // 3J/3K/3-VAX
// panel->addApplet(m_catApplet);
// m_tunerApplet = new TunerApplet(m_radioModel, nullptr);      // ATU phase (no plan yet)
// panel->addApplet(m_tunerApplet);
```

If any of `m_eqApplet`, etc. are referenced elsewhere in `MainWindow.cpp`, those references must also be removed or guarded — verify with `grep -n "m_eqApplet\|m_fmApplet\|..." src/gui/MainWindow.cpp`.

- [ ] **Step 3: Build + visual**

```bash
cmake --build build -j$(nproc) --target NereusSDR 2>&1 | tail -10
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
```

Right panel shows only 4 applets: RX, TX, Phone/CW, VAX. The 9 ghosts are gone.

- [ ] **Step 4: Commit**

```bash
git add src/gui/MainWindow.cpp
git commit -m "$(cat <<'EOF'
ui(main-window): hide ghost applets until their feature phases ship

Per docs/architecture/ui-audit-polish-plan.md §C2, 9 of 13 right-panel
applets are entirely placeholder-only today (no wired controls). Showing
them is misleading — users click "Equalizer" and nothing happens.

Commented out the construction + addApplet() calls for Eq, FM, Digital,
PureSignal, Diversity, Cwx, Dvk, Cat, Tuner. Each line carries a
\"TODO <phase>: re-enable\" comment so the re-enable is one-line when
the underlying feature ships.

Right panel now shows 4 working applets: RX, TX, Phone/CW, VAX.

Files stay in the tree; only the addApplet calls are gated.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: C2 — Stub PhoneCwApplet's CW tab body

**Files:**
- Modify: `src/gui/applets/PhoneCwApplet.cpp`

- [ ] **Step 1: Locate the CW tab body construction**

Per inventory: `PhoneCwApplet` uses a `QStackedWidget` with Phone (index 0) and CW (index 1). The CW page is built by `buildCwPage()` (or equivalent) — search:

```bash
grep -n "buildCwPage\|CW page\|m_cwPage\|setCurrentIndex(1)" src/gui/applets/PhoneCwApplet.cpp | head -10
```

- [ ] **Step 2: Replace CW page contents with a "Coming in 3M-2" placeholder**

Keep the CW tab BUTTON in the tab bar (per inventory: line 199, `m_stack->setCurrentIndex(1)` connect). Replace the CW page contents (everything in `buildCwPage`) with:

```cpp
// Per docs/architecture/ui-audit-polish-plan.md §C2 — CW TX is deferred
// to phase 3M-2. Strip the CW tab body to a placeholder until then.

QWidget* cwPage = new QWidget;
auto* layout = new QVBoxLayout(cwPage);
layout->setContentsMargins(8, 24, 8, 24);
layout->setAlignment(Qt::AlignCenter);

auto* label = new QLabel(QStringLiteral(
    "CW TX coming in Phase 3M-2.\n\n"
    "Speed, pitch, sidetone, break-in, iambic, firmware keyer\n"
    "controls will appear here when CW TX is wired up.\n\n"
    "For now, see Setup → DSP → CW for available CW config."
));
label->setAlignment(Qt::AlignCenter);
label->setWordWrap(true);
label->setStyleSheet(QStringLiteral(
    "QLabel { color: %1; font-size: 11px; line-height: 1.4; }"
).arg(NereusSDR::Style::kTextSecondary));
layout->addWidget(label);

m_stack->addWidget(cwPage);
```

Delete the original CW page content code (mic compression gauge, speed slider, pitch buttons, etc. — all NYI).

- [ ] **Step 3: Build + visual**

Open PhoneCwApplet → click "CW" tab. The tab body shows the "Coming in Phase 3M-2" placeholder.

- [ ] **Step 4: Commit**

```bash
git add src/gui/applets/PhoneCwApplet.cpp
git commit -m "$(cat <<'EOF'
ui(phone-cw-applet): stub CW tab body until 3M-2 ships

Per docs/architecture/ui-audit-polish-plan.md §C2. The CW tab's controls
(speed, pitch, delay, sidetone, break-in, iambic, firmware-keyer toggle)
were all NYI placeholders. Replaced with a single \"CW TX coming in
Phase 3M-2\" placeholder label until the underlying CW TX implementation
lands.

Tab structure intact — Phone tab still works fully; CW tab button is
visible in the tab bar; the body is a single message panel.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 8: Final verification

**Files:**
- None modified

- [ ] **Step 1: Build + tests**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -10
ctest --test-dir build --output-on-failure 2>&1 | tail -10
```

- [ ] **Step 2: Visual sweep**

```bash
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
screencapture -x -t png /tmp/nereussdr-after-right-panel.png
```

Right panel: 4 applets (RX/TX/Phone-CW/VAX). TxApplet body shorter (5 rows removed), TUNE+MOX above VOX+MON, SWR gauge functional. PhoneCwApplet CW tab shows placeholder.

- [ ] **Step 3: Plan summary**

Plan 3 complete. ~6 commits land:
- C1 Task 2: SWR gauge wire
- C1 Task 3: SWR Prot LED (wire OR remove)
- C1 Task 4: 5 NYI rows removed
- C1 Task 5: TUNE/MOX swap
- C2 Task 6: 9 ghost applets gated
- C2 Task 7: PhoneCwApplet CW tab stub

---

## Self-Review

**Spec coverage:**
- C1 (TxApplet density) — Tasks 2, 3, 4, 5 ✓
- C2 (Whole-applet NYI ghost stack) — Tasks 6, 7 ✓
- C1 layout reshuffle (TUNE/MOX swap) — Task 5 ✓ (locked late in walkthrough; explicitly captured)

**Placeholder scan:** none.

**Type consistency:** SWR gauge member `m_swrGauge` referenced in Task 2 matches the inventory. The 9 ghost-applet member names (`m_eqApplet` etc.) match the existing MainWindow.h declarations.

**Dependencies:** This plan depends on Plan 1 (canonical palette for any new label styling, e.g. the CW tab placeholder). Independent of Plans 2, 4, 5.
