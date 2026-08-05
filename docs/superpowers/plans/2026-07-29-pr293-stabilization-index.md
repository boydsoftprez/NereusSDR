# PR 293 Full Stabilization Implementation Plan — Index

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix every approved defect in pull request #293, validate the incoming ADC-routing commits instead of duplicating them, pass pinned-source compliance, smoke-test a clean local application build, create GPG-signed commits, and push the result to `feature/phase3f-sub-epic-a-foundation`.

**Architecture:** The work is split into six independently testable plans. Identity becomes stable-ID-native before per-pan and TX work depend on it. TX state is then routed through one authoritative TX-bound slice. Protocol ownership is reduced to one P2 DDC assignment writer and diversity gains a complete WDSP lifecycle. Data-plane changes are transactional and audio withdrawal invalidates queued generations. RF2KS/TCI close the integration edge cases, followed by pinned-source compliance, the complete test suite, and an isolated smoke launch.

**Tech Stack:** C++20, Qt 6, CMake/CTest, WDSP, HPSDR Protocol 1/2, GitHub Actions, Python compliance scripts, GPG-signed Git commits.

---

## Authority and starting point

- Approved design: `docs/superpowers/specs/2026-07-29-pr-293-stabilization-design.md`
- Pull request: `boydsoftprez/NereusSDR#293`
- Branch: `feature/phase3f-sub-epic-a-foundation`
- Reviewed remote head: `40cc91907870f6811ac8ac2a661f6f862795f2ad`
- Design commit: `35dbf972d7a3910cb7a4709caa454615bff20ead`
- Worktree: `/Users/j.j.boyd/NereusSDR/.worktrees/phase3f-sub-epic-a-foundation`

Before production edits and again before the final push:

```bash
git fetch origin feature/phase3f-sub-epic-a-foundation
git merge-base --is-ancestor origin/feature/phase3f-sub-epic-a-foundation HEAD
git status --short --branch
```

If the remote head is no longer an ancestor, stop implementation, inspect the new commits, and integrate them without rewriting or discarding the contributor's work.

## Execution order

1. `2026-07-29-pr293-01-ci-compliance.md`
   - Establish the exact pinned-source compliance environment.
   - Repair direct Qt meta-object includes and all verified inline-tag omissions.
2. `2026-07-29-pr293-02-identity-per-pan.md`
   - Convert TX binding and UI callbacks to stable slice IDs.
   - Fix AGC, per-pan settings/overlays/wideband, RADE, and floating-window lifetime.
3. `2026-07-29-pr293-03-tx-authority.md`
   - Make TX-bound slice the single source for mode, bandplan, Alex, audio gating, VOX/TGXL, and WDSP RX suspension.
4. `2026-07-29-pr293-04-protocol-diversity.md`
   - Make full P2 `DdcAssignment` the only P2 wire owner.
   - Publish DDC/ADC/chain state consistently and implement the complete external-diversity lifecycle.
5. `2026-07-29-pr293-05-data-plane.md`
   - Make sample-rate narrowing transactional.
   - Prevent withdrawn mixer members from leaking stale queued audio.
6. `2026-07-29-pr293-06-integrations-verification.md`
   - Invalidate RF2KS in-flight replies and restore initial-failure backoff.
   - Fix TCI stable-ID enumeration.
   - Run focused, subsystem, full-suite, pinned-compliance, clean-build, and smoke-launch gates; sign and push commits.

## Global implementation rules

- Follow red-green-refactor for every behavior change: add or strengthen the named regression, build its exact target, confirm the expected failure, make the smallest production change, and rerun it.
- Tests are `EXCLUDE_FROM_ALL`. Always build the exact target before invoking its CTest name:

```bash
cmake --build build --target tst_master_mixer -j
ctest --test-dir build -R '^tst_master_mixer$' --output-on-failure
```

- Build the matching target before a label run. Build `all_tests` before the complete suite.
- Preserve upstream attribution, inline author/version tags, citations, and modification history. Run the repository provenance checks after every source-port task.
- Do not bypass hooks. Supply all four upstream source roots when committing:

```bash
NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis \
NEREUS_MI0BOT_DIR=/Users/j.j.boyd/mi0bot-Thetis \
NEREUS_DESKHPSDR_DIR=/Users/j.j.boyd/deskhpsdr \
NEREUS_FREEDV_DIR=/Users/j.j.boyd/freedv-gui \
git commit -S
```

- Keep commits reviewable in this order:
  1. plans;
  2. compliance;
  3. stable identity and per-pan ownership;
  4. authoritative TX state;
  5. P2 assignment and diversity;
  6. transactional data plane;
  7. RF2KS/TCI;
  8. any verification-only corrections.
- Re-run a focused test immediately after any rebase, conflict resolution, or remote-head integration that touches its subsystem.

## Completion evidence

The work is complete only when all six plans are checked, `git diff --check` is clean, the pinned-source checks pass, `all_tests` and the complete CTest suite pass, a newly configured application build succeeds, the app remains alive through the smoke interval under isolated settings, all implementation commits have verified GPG signatures, and the branch push to the existing PR succeeds.
