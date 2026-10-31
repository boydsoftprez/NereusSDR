# NNR Warning and Operator Choice Implementation Plan

> **For agentic workers:** Crew execution with one integrated review; use the existing user-approved scope, not another approval ceremony.

**Goal:** Preserve selected NNR and show a measured warning with explicit operator remedies across Core, desktop and iPhone.
**Architecture:** Core owns fresh backlog monitoring and separate read-only warning state; bounded input recovery remains unchanged. Existing model/off commands implement operator choices; old limits remain compatibility readbacks.
**Tech Stack:** C++20/Qt6, Swift/SwiftUI, existing station-link minor11 schemas.
**Spec:** docs/superpowers/specs/2026-10-31-nnr-warning-policy-design.md
**Workspace:** /Users/j.j.boyd/.codex/worktrees/frequency-wheel-699/NereusSDR
**Branch/base:** codex/nnr-warning-policy at61bbc176845a908542dc48e4e4b520bd718aa551 (includes merged About credit712).

## Global Constraints
Preserve receive queue500ms/250ms bounds; no real radio actions or restarts. New outbound property nnrLoadWarning has distinct capability gating; minor11 unchanged. Existing nnrLimit never represents a warning. Warning does not change activeNr/model. Source facts: ReceiverDspLoadSampler.h, RxDspWorker.h/.cpp, NnrLoadGovernor.h/.cpp, station-linkv1 feature rules. Preserve signing/hooks and all unrelated source. Use crew/cost-aware instructions and CLAUDE/CONTRIBUTING/fast-test-loop.

## Review Focus
Stale/torn/idle samples and shared-stream/routing generations; normal bursty neural frames without lag; exact remote field/capability filtering and unknown-field logs; old-Core actual limits versus new warning; authority/read-only/reconnect UX and popup bounds.

### Task 1: Core observation, warning state, and compatible wire
Owner: Sol6.1 high; production Core/models/session + relevant C++ tests/link fixtures only. Coordinate exact capability spelling with task2 before its work starts.
- [ ] Read scout reports and settle observation freshness, reset and expiry semantics in this spec before code.
- [ ] Add meaningful failing tests for busy-without-backlog, sustainedfreshlag, recovery, stale/idle/reset, model retained, queuebounds, wire old/new filtering.
- [ ] Implement monitor/lock-free metadata as needed; remove automatic reduction call from live policy, publish distinct warning string/capability; keep old readbacks/retry compatible.
- [ ] Build exact affected test targets before anchored ctest; retain RED/GREEN logs and source state. No unrelated whole-suite iteration.
- [ ] Return uncommitted changes and report; controller reviews actual diff before task2.

### Task 2: Consistent operator surfaces and documentation
Owner: Sol6.1 medium/high according to final contract; gui/ios and focused UI/model tests + documentation. Core contract from task1 is prerequisite.
- [ ] Add regression tests proving pressure warning without fake nnrLimit, correct UseStandard/Off writes, oldlimitretry, authority guards, clear/slice transitions and long/narrow text.
- [ ] Update shared desktop NnrControls, compact RX warnings and iPhone NnrStepBack/RxPanel/Modes/settings; exact identities/field/capability from task1. No fake/dismiss-only NNR toggles.
- [ ] Update manual/upgrade/changelog/bench notes; regenerate website/manual outputs using established script only if required.
- [ ] Build/run focused tests; native visual inspection of applicable desktop and phone simulator bounds; report evidence and limits.

### Task 3: Integrated review, full gates and delivery
Owner controller and fresh independent Sol6.1 high reviewer.
- [ ] Review source/acceptance/compatibility and raw focused evidence together once; resolve important findings in one correction wave then scoped rereview.
- [ ] Sign scoped commits with hooks; run applicable final full-suite/required CI on integrated tree. Reuse completed same-code evidence and do not broaden/repeat without a concern.
- [ ] Push/open PR and merge via normal protected workflow under ongoing release authorization. Update release ledger, build/source identity and next candidate plan. Do not mutate existing RC1 tag or claim running binaries contain unbuilt changes; actual station switch requires current idle authorization.

## Implementation verification checkpoint

Core and shared desktop/iPhone implementation completed; independent integrated review plus scoped re-review cleared R1/R2/R3. Freshness/source identity, mixed-input attribution, native diagnostic wrapping and selected-NNR notice priority are covered. Focused Core and app behavior gates passed; repaired narrow capture and full combined validation remain final controller gates. Exact logs and intermediate failures are retained in the ignored crew ledger; no physical listening acceptance is claimed.
