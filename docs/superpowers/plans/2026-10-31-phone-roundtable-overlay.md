# Phone Roundtable spectrum card correction

> For agentic workers: use subagent-driven-development for this bounded correction. The controller owns integration and release actions.

**Goal:** Make the phone spectrum's Roundtable card honor the selected signal units and the approved compact information hierarchy.

**Architecture:** Replace serialized report text and its reparsing with typed presentation fields and one fitted card plan. Reuse `RoundtableText.signalLines` and the device-local `RoundtableModel.signalFormat`; derive rendering, height, protected geometry and hit regions from the same plan.

**Tech stack:** Existing SwiftUI app, Roundtable models and native simulator test fixtures.

**Spec:** `docs/architecture/2026-10-29-roundtable-frequency-first/README.md` and its approved board; the maintainer's 2026-10-31 screenshot and request. Source audit: `/tmp/nereus-phone-roundtable-overlay-audit.md`.

## Constraints and review focus

- Phone-only follow-up from `main@abd8478f`. The maintainer explicitly authorized merging and delivering this fix to TestFlight on 2026-11-01 before the still-pending desktop release. No desktop source or station rollout is part of this correction.
- No Core, DSP, radio, protocol, connection, consent, identity-store or user-default changes. Formatting and reading reports never tune or transmit.
- Keep exact-frequency presence independent of loaded reports. Keep duplicate callsigns, scoped source/turn matching, unknown/doubled/quiet states and original measurement ages honest.
- Filter eligible actual readings before the compact-card cap. Missing readings are absent from the compact report rows, with full status still available in details. Never manufacture a value or append dBm to a missing token.
- Reuse the existing S-unit/dBm/Both formatter and calibration. Both is one semantic reporter row with sufficient height for two signal lines.
- Use structured identity, signal and age fields with normal native hierarchy. A narrow strip uses presence/state/details disclosure instead of word splitting. Do not promise columns where the available width cannot fit them.
- Preserve explicit Read reports versus Tune actions, keyed no-tune behavior, existing accessibility identifiers and useful semantic accessibility labels. Keep any signal-unit action separate from tuning and avoid nested buttons; the existing action dialog is an acceptable compact control.
- Desktop has a corresponding hardcoded dBm presentation; record it as a separate follow-up, without changing the frozen release.

## One implementation task

- [x] Add focused failing coverage for selected formats in the spectrum card, invalid/missing/current-turn eligibility before capping, duplicate presence, unknown/doubled/quiet attribution and narrow/Both fitted geometry.
- [x] Implement typed card data and a fitted presentation plan in `ios/NereusApp/Roundtable/RoundtableTagLayer.swift`, reusing existing formatting helpers.
- [x] Replace the flat monospaced report block with the approved presence/state/reporter hierarchy and a concise narrow form. Keep size, keepouts and hit regions consistent.
- [x] Verify format changes make no Core or tuning calls. Read reports must dispatch only its existing read callback, which may subscribe through the Core, and must never tune or key. Retain explicit Tune and keyed restrictions. Update affected implementation-shaped assertions in the existing Roundtable test suites.
- [x] Build matching app tests in isolated derived data, then run the affected Roundtable presence, sheet and tools suites using the repository simulator helper. Do not rerun unrelated desktop suites.
- [x] Capture native portrait with the large flag, portrait with the half sheet, landscape and Both fixtures. Inspect the actual renders for wrapping, clipping, overlap, row attribution and legibility; correct concrete defects once rather than repeating unchanged tests.
- [x] Return source diff, matching build/test results and render paths for controller review. No commit, PR, merge, archive, upload, station action or publication without controller delegation.

Verification and controller handoff: `/tmp/nereus-phone-roundtable-overlay-implementation.md`; final product 79/79 affected-suite tests passed, plus the final native card capture/action check 1/1 passed. Native screenshot review complete. Changes remain uncommitted.

## Controller review corrections (2026-11-01)

- [x] Keep an 8-point native state-font floor, honest middle ellipsis for overlong identities, and full accessible/details identity; verify the full Unknown label at the actual 76.333-point native strip as well as nominal 80/100-point cases.
- [x] Make ended unknown/unattributed observations visibly historical with Last heard; retain Last talker for known identity and Last overlap / Unattributed for ended combined RF. Preserve measurement ages and avoid a new pill.
- [x] Add long portable-call/nth and ended-observation regressions, capture complete native render bounds, and run the matching affected suite: 81/81 passed in 52.273 seconds. Review addendum and close-up paths are in `/tmp/nereus-phone-roundtable-overlay-implementation.md`.

## Controller delivery authorization (2026-11-01)

JJ: “Ok get it on TestFlight.” Independent final integrated review passed with no blockers. The controller will merge through normal protection, archive clean merged main using full Git history, verify identity/signature/privacy/build number, then upload through App Store Connect and enable the existing internal and public preview groups. Desktop release gates remain separately pending.
