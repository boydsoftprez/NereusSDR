# Roundtable Signal Format Implementation Plan

> **For agentic workers:** Use the existing Crew execution workflow and NereusSDR adapter. One integrated review follows the combined implementation; the controller owns Git and delivery.

**Goal:** Let the operator tap Roundtable's reporter signal heading or reading to choose dBm, S-units, or both, with a remembered local choice on iPhone/iPad, desktop, and web.

**Architecture:** Reuse each client's existing S-unit formatter and raw reported dBm. Store a presentation preference on the client, with no Core command, protocol, measurement, or DSP change. Both uses compact stacked values in report tables; the existing reporter identity and age retain their space.

**Tech Stack:** SwiftUI/PhoneSettings, Qt/AppSettings, existing browser JavaScript/localStorage.

**Spec:** JJ's October 31 request and attached Roundtable reporter-table screenshot: “tap to switch to s meter vs dbm or have both.” The user has delegated routine implementation decisions and asked to continue toward release without repeated approvals. This is a specified display control; no separate design approval ceremony is added.

## Global Constraints

- Cycle dBm → S-units → Both → dBm from the signal heading and reporter reading; expose an explicit three-choice setting.
- Preserve existing initial display: phone/desktop frequency reports use dBm; desktop legacy reports use S-units; web attributed reports already use both. Persist explicit choices locally using established client settings conventions.
- Reuse the existing S9 = −73 dBm / 6 dB per S-unit conversion and existing rounding. Native dBm precision remains whole numbers; web remains one decimal.
- Keep raw reports, missing/invalid data, stale ages, reporter attribution and unknown/doubled-talkers behavior intact.
- Scope is Roundtable report tables and their compact/current/earlier variants, not receiver-meter calibration or spectrum peak labels.
- Use existing targets/caches; serialize iOS builds. The iOS cache contains a previous diagnostic binary and must be rebuilt before testing current source.
- Preserve the 150 ms tuning gate. Do not restart stations or alter operator settings during implementation.

## Review Focus

- Both at narrow phone/desktop/browser widths preserves callsign, signal and Age without overlap.
- Missing/non-finite/out-of-range readings do not become fabricated S readings.
- Local preference survives recreation/reload and does not send a Core command.
- Signal buttons are discoverable and keyboard/VoiceOver accessible, including within existing combined rows.
- Existing table/row selection and earlier-turn controls retain their actions when a signal is tapped.

### Task 1: iPhone and iPad

**Owner:** Terra 6.1 medium, `roundtable_signal_format_phone`.

**Files:** Existing PhoneSettings/AppModel, RoundtableModel, RoundtableRoomView, RoundtableToolsPage, and relevant existing iOS tests.

- [x] Add focused coverage for the three formats, cycle, persistence, and no Core writes.
- [x] Implement the local preference, tappable signals and settings picker using existing formatters.
- [x] Rebuild the matching test target and run the focused tests; inspect portrait and narrow Both renders.

### Task 2: Desktop and website

**Owner:** Terra 6.1 medium, `roundtable_signal_format_desktop_web`.

**Files:** RoundtablePanel, the necessary SpotHubDialog display-settings callsite, static roundtable.js/css, and relevant existing tests. No iOS ownership.

- [x] Add focused coverage for formatting, controls, local persistence and unchanged raw data.
- [x] Implement the matching choice with existing AppSettings/localStorage conventions and formatters.
- [x] Incrementally build matching native tests and run focused web tests; inspect narrow Both renders.

### Task 3: Integration and delivery

**Owner:** Controller; one Sol 6.1 high integrated review.

- [x] Verify actual diffs, test exits and visual artifacts; resolve the combined review findings in one scoped wave.
- [ ] Update release notes and bench guidance; make signed commits and open/merge the scoped PR under existing authorization.
- [ ] Run required final CI for the combined source once. Reuse unchanged evidence rather than repeating full suites locally.
- [ ] Deliver the updated web asset and next combined phone build after verification; include native changes in the next desktop candidate without an unsolicited station restart.

## Separate open report

Brian's Roundtable name-editor/keyboard layout report remains unconfirmed and is not part of this preference change. Its temporary diagnostic source was archived and removed. Phone 9636 is Testing and does not claim to fix that report.
