# PR 293 Stabilization 01 — CI and Provenance Compliance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reproduce CI's exact upstream-source comparison locally, restore every confirmed inline attribution tag, and make `MainWindow.cpp` self-sufficient for its Qt meta-object types on all supported compilers.

**Architecture:** CI pins are treated as immutable source authority. Detached temporary worktrees at those commits feed the existing provenance scripts, eliminating false confidence from locally advanced upstream clones. The Qt fix uses direct includes in the translation unit that invokes `QMetaMethod` and `QMetaProperty`. No behavior is changed in this plan.

**Tech Stack:** Python 3 compliance scripts, Git worktrees, C++20, Qt 6, CMake, GitHub Actions.

---

## Files

- Modify: `src/gui/MainWindow.cpp`
- Modify only where the pinned checker proves a missing tag:
  - `src/models/RadioModel.cpp`
  - `src/core/RadioConnection.h`
  - `src/core/P2RadioConnection.cpp`
  - `src/core/codec/CodecContext.h`
  - `src/core/codec/P2CodecSaturn.cpp`
  - `src/core/codec/P1CodecAnvelinaPro3.cpp`
  - `src/core/codec/P2CodecOrionMkII.cpp`
  - `tests/tst_alex_per_adc_bpf_wire.cpp`
  - `tests/tst_codec_5_slice_assignment.cpp`
- Verify: `.github/workflows/ci.yml`
- Verify: `scripts/verify-inline-tag-preservation.py`
- Verify: the remaining provenance/license scripts invoked by `.pre-commit-config.yaml`

## Task 1: Freeze the branch and exact CI source authority

- [ ] **Step 1: Refresh the PR branch and confirm fast-forward ancestry**

```bash
git fetch origin feature/phase3f-sub-epic-a-foundation
git merge-base --is-ancestor origin/feature/phase3f-sub-epic-a-foundation HEAD
git status --short --branch
```

Expected: the ancestry command exits zero and the only pending changes are these plan documents.

- [ ] **Step 2: Verify the pinned commits in CI**

```bash
rg -n "501e3f513f73f07742d7e1b85a0e9528bd14977d|c26a8a4c7592605039be5f4b5973b33e04e91e54|77e793a37e0ecf6cfa99b7cf21a410fd154e9d94" .github scripts
```

Expected authorities:

- Thetis: `501e3f513f73f07742d7e1b85a0e9528bd14977d`
- mi0bot-Thetis: `c26a8a4c7592605039be5f4b5973b33e04e91e54`
- freedv-gui: `77e793a37e0ecf6cfa99b7cf21a410fd154e9d94`

- [ ] **Step 3: Create detached source worktrees without changing the user's clones**

```bash
PR293_PIN_ROOT="$(mktemp -d /tmp/nereus-pr293-pins.XXXXXX)"
git -C /Users/j.j.boyd/Thetis worktree add --detach "$PR293_PIN_ROOT/Thetis" 501e3f513f73f07742d7e1b85a0e9528bd14977d
git -C /Users/j.j.boyd/mi0bot-Thetis worktree add --detach "$PR293_PIN_ROOT/mi0bot-Thetis" c26a8a4c7592605039be5f4b5973b33e04e91e54
git -C /Users/j.j.boyd/freedv-gui worktree add --detach "$PR293_PIN_ROOT/freedv-gui" 77e793a37e0ecf6cfa99b7cf21a410fd154e9d94
```

Keep `PR293_PIN_ROOT` available for every compliance command in this plan and Plan 06.

## Task 2: Capture the failing compliance baseline

- [ ] **Step 1: Run the exact inline-tag checker against the pinned trees**

```bash
NEREUS_THETIS_DIR="$PR293_PIN_ROOT/Thetis" \
NEREUS_MI0BOT_DIR="$PR293_PIN_ROOT/mi0bot-Thetis" \
NEREUS_DESKHPSDR_DIR=/Users/j.j.boyd/deskhpsdr \
NEREUS_FREEDV_DIR="$PR293_PIN_ROOT/freedv-gui" \
python3 scripts/verify-inline-tag-preservation.py
```

Expected before repair: non-zero exit with confirmed missing-tag findings. Save the full output in the task log; it is the authoritative edit list.

- [ ] **Step 2: Run every repository provenance hook against the same pins**

```bash
NEREUS_THETIS_DIR="$PR293_PIN_ROOT/Thetis" \
NEREUS_MI0BOT_DIR="$PR293_PIN_ROOT/mi0bot-Thetis" \
NEREUS_DESKHPSDR_DIR=/Users/j.j.boyd/deskhpsdr \
NEREUS_FREEDV_DIR="$PR293_PIN_ROOT/freedv-gui" \
pre-commit run --all-files
```

Expected: record all failures before editing. Do not repair files that the exact pinned run does not identify unless another hook provides a concrete failure.

## Task 3: Restore verified inline tags without changing code

- [ ] **Step 1: Inspect each pinned source citation before editing**

For every checker result, open the Nereus citation and the exact upstream line window named by the script. Confirm:

1. the Nereus code is derived from that upstream construct;
2. the upstream window contains the author/version tag;
3. the Nereus comment is within the checker's permitted distance from the cited construct;
4. the original tag text is preserved verbatim.

Use `rg -n` and `sed -n` on the pinned tree; never execute upstream source.

- [ ] **Step 2: Add only the missing comment tokens**

Apply comment-only edits to the confirmed files. Preserve existing line citations and modification history. For a tag such as `//MW0LGE_22b missed out`, retain that exact token and add a short bracketed note only when needed to explain why it appears beside transformed C++.

- [ ] **Step 3: Prove the inline-tag checker is green**

```bash
NEREUS_THETIS_DIR="$PR293_PIN_ROOT/Thetis" \
NEREUS_MI0BOT_DIR="$PR293_PIN_ROOT/mi0bot-Thetis" \
NEREUS_DESKHPSDR_DIR=/Users/j.j.boyd/deskhpsdr \
NEREUS_FREEDV_DIR="$PR293_PIN_ROOT/freedv-gui" \
python3 scripts/verify-inline-tag-preservation.py
```

Expected: exit zero.

## Task 4: Make Qt meta-object dependencies explicit

- [ ] **Step 1: Capture the missing direct-include condition**

```bash
rg -n '^#include <QMeta(Method|Property)>$|QMeta(Method|Property)' src/gui/MainWindow.cpp
```

Expected before repair: use sites exist, but one or both direct include lines are absent.

- [ ] **Step 2: Add the direct includes**

In `src/gui/MainWindow.cpp`, add:

```cpp
#include <QMetaMethod>
#include <QMetaProperty>
```

Place them in the sorted Qt include block. Do not rely on another Qt header's transitive includes.

- [ ] **Step 3: Rebuild the production translation unit and app target**

```bash
cmake --build build --target NereusSDRObjs -j
cmake --build build --target NereusSDR -j
```

Expected: both targets build successfully.

## Task 5: Verify and commit the compliance slice

- [ ] **Step 1: Run the complete pinned pre-commit suite**

```bash
NEREUS_THETIS_DIR="$PR293_PIN_ROOT/Thetis" \
NEREUS_MI0BOT_DIR="$PR293_PIN_ROOT/mi0bot-Thetis" \
NEREUS_DESKHPSDR_DIR=/Users/j.j.boyd/deskhpsdr \
NEREUS_FREEDV_DIR="$PR293_PIN_ROOT/freedv-gui" \
pre-commit run --all-files
```

Expected: all hooks pass.

- [ ] **Step 2: Confirm this commit contains only direct includes and provenance comments**

```bash
git diff --check
git diff -- src/gui/MainWindow.cpp src/models/RadioModel.cpp src/core/RadioConnection.h src/core/P2RadioConnection.cpp src/core/codec tests
```

- [ ] **Step 3: Create and verify the signed commit**

```bash
git add src/gui/MainWindow.cpp src/models/RadioModel.cpp src/core/RadioConnection.h \
  src/core/P2RadioConnection.cpp src/core/codec tests/tst_alex_per_adc_bpf_wire.cpp \
  tests/tst_codec_5_slice_assignment.cpp
NEREUS_THETIS_DIR="$PR293_PIN_ROOT/Thetis" \
NEREUS_MI0BOT_DIR="$PR293_PIN_ROOT/mi0bot-Thetis" \
NEREUS_DESKHPSDR_DIR=/Users/j.j.boyd/deskhpsdr \
NEREUS_FREEDV_DIR="$PR293_PIN_ROOT/freedv-gui" \
git commit -S -m "fix(compliance): preserve pinned upstream provenance"
git log --show-signature -1
```

Expected: a good signature and a clean hook run. If the pinned checker names fewer files than the initial review list, stage only the files actually repaired.
