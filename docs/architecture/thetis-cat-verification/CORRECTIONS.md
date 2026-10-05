# Consolidated integrated-review correction wave

This local wave starts at signed Task 14 head
`025e7f365552594e441bc1f3c82a2420cb41892a`. The initial full-feature
review identified five required findings. The same reviewer closed all five on
final signed product source `be3e0ab35e0d48a5364f6ef5c95e5e6a595e8bf3`,
parent `27dd27b08eea4ca04b07be4ee0ba867f3864b202`. The lead accepted the
refreshed full/platform software gates and closed this local packet. Historical
focused evidence and the introduced F2 defect remain recorded below.

| Finding | Correction and regression evidence |
| --- | --- |
| Owned OFF with an unavailable primary | Thetis RX/ZZTX0/ZZTU0/ZZUT0 release their existing matching coordinator claim before unrelated primary readability. Rigctld T/set_ptt OFF likewise bypasses the RX-prefix readability gate. Real sessions activate B while only A is removed, hidden or reused. Wrong-kind OFF, denied Tester-like transmit permission, coalesced last-owner release, new-ON refusal and newer native ownership remain covered. All 30 new wire cases failed before the fix. Native Tune/TwoTone completion follows its existing asynchronous settle; tests await actual completion with ordinary bounds. |
| Native mode notification lifetime and supersession | Private mode and mode/filter commit revisions distinguish a newer mode operation from a filter-only edit and from a no-op echo. SliceModel stops stale continuation after synchronous deletion, nested mode/filter changes and changes away and back. Early RADE callsign/sync notifications and saved-filter settings hooks are guarded too. A filter-only edit preserves its new cutoffs while the committed mode lifecycle and notification complete. Actual CAT and Rigctld model deletion crashed before correction; native duplicate/early-continuation cases reproduced stale state or notifications. Final tests cover owned slice DeferredDelete, whole-model deletion, no-op echoes, existing filter persistence and RADE behavior. |
| Combined Hamlib numeric validation | The complete native mode/passband calculation precedes the first frequency effect for K, long set_split_freq_mode and ERP. Before correction, 24 integer-edge cases returned invalid-argument while changing frequency; 24 parse-invalid controls already left state intact. All 48 rows now preserve frequency/mode/filter and their mutation-signal counts. Native -1 keep-passband behavior after frequency/band callbacks and guarded valid multi-effect writes are retained. |
| Desired global tuple and PTT retirement continuation | All three CAT command families read CatService's authoritative desired global tuple. During a real accepted OFF callback, an existing serial byte session reads coherent identity, serial number, AI, RTTY and PC/ZZPC/ZZTO reporting preferences and accepts a compatible nested setter. The snapshot-only fix exposed enabled replacement PTT stuck Stopped. A scoped, lifecycle-tagged explicit-ingress restart context now transfers that continuation; its revision is consumed before open/sampling failure callbacks can reenter. Old stop cleanup preserves a replacement owning the same transport and closes distinct retired inputs. Actual timer samples prove release/rearm/keying, and open/sampling failures prove one attempt, newest saved preferences and no retry from unchanged or unrelated preference proposals. |
| Platform and host PTY reasons | Both applet and Setup sync limit dialect/action tooltips to supported native platforms. Unsupported controls retain their disabled native-PTY reason, and remote controls explain the local-host requirement. The Setup remote tooltip fails before correction on actual macOS. Both Windows source branches fail a source-only selection audit before correction and pass after it; conditional runtime assertions are present for supported and unsupported platforms. No Windows SDK/compiler/runtime execution is claimed. |

The first signed correction attempt, `27dd27b`, builds the affected tests and
runs ten serial CTest
executables: native per-mode filter persistence, RADE mode swap, SliceModel
properties, CAT RX/TX, Rigctld, serial PTT, settings, Setup and applet. Result:
**10/10 CTests, EXIT 0, 43.91 seconds; 458 Qt passes, zero failures, two skips**.
The skips are the separately requested visual-capture slot and the official
Linux client slot on macOS. Earlier red, crash, fixture-chronology and stale-moc
setup attempts remain distinct in retained raw logs.

The fresh application/headless-daemon and 43 anchored-test build also passes.
The serial anchored run passes **43/43 CTests, EXIT 0, 140.33 seconds;
2,093 Qt passes, zero failures, two skips**. It includes the current production
coverage execution alongside ownership, lifecycle, settings and native TX guards.
The two skipped slots have the same explicit capture/Linux-client conditions.

Four named operator wording, placeholder and station reason/tool gates pass
**4/4 CTests, EXIT 0, 27.31 seconds; 61 Qt passes, zero failures/skips**.
The pre-freeze compliance run passes all **16 commands, EXIT 0, 95.36 seconds**.

The source audit preserves 52 byte-exact copied CAT notice regions, 1,264
relocated Thetis comments and the selected AetherSDR 23+3 comments. Native
SliceModel copied notice tails also remain byte-identical to the accepted base.
The exact XML, 419 descriptors, 349 active registrations, outcome accounting
and 783 fixture records are unchanged by this wave. Historical fixture status
is distinct from the combined production-coverage status as described in
[the verification README](README.md).

This wave touches existing native mode/filter notification continuations on
the RX parameter path; it does not change RX algorithms, RADE values/defaults,
audio routing or threads. AudioEngine, RxChannel, WdspEngine and RadioConnection
processing sources remain byte-identical to the original feature base.
Hardware/RF, physical cable/pin and received-audio evidence remain unperformed.

Raw commands, exits, durations, crashes, wire/native assertions, exact-notice
checks and fresh LastTest logs are retained locally under
`.crew/2026-10-04-thetis-cat-plan/task-14-fix-*`; the wave report contains the
signed freeze identity and subsequent full/platform evidence. This document
records the accepted corrections and local software verification; it does not
assert publication or physical radio/RF evidence.

The same reviewer's first scoped rereview addressed F1, F3, F4 and F5 but
identified an introduced F2 lifecycle regression. An early RADE exit callback
could leave the existing decoder and route while a nested setter observed the
intermediate non-RADE mode. The signed `27dd27b` attempt and interrupted full
build remain preserved. A narrow closure reconciles native RADE cleanup with
the actual retained engine channel; role/deferred admission and the existing
destroy/create/wire/start paths remain unchanged. Eight live parent/engine
regressions fail before this closure and pass afterward: both callsign and sync
boundaries, final AM, either RADE sideband and changes away and back. They prove
old decoder retirement, actual worker route count, new active replacement,
unchanged RxChannel identity, newest filter values and accepted notification
counts. The standalone deletion, no-op, ABA and filter lifecycle regressions
remain required and pass. The same reviewer accepted the targeted F2 closure
on `be3e0ab`; all five source findings are closed.

Current closure focused validation passes 10/10 CTests with 466 Qt passes,
zero failures and the same two expected capture/Linux-client skips. Current
application/headless-daemon build, exact notice audit and all 16 compliance
commands pass. Final combined/platform gates and reviewer acceptance are
complete, as recorded in the [verification README](README.md).

## Final accepted local packet

The application/daemon/all_tests build passes on the reviewed source. Selected
results cover all 1,153 registered executables: the first ordinary attempt's two
failed records are retained and replaced by unchanged complete passing reruns;
all 11 native-window records pass; all 22 realtime executables have accepted
individual host intervals. The final manifest has 19,838 Qt passes, zero failures
and 374 explicit skips, with 1,151 Qt-bearing registrations and 1,153 legitimate
Qt Totals blocks. No loaded whole-epoch qualification or duplicate pass count
is implied. The final Wi-Fi normal-dashboard observations and lead disposition
remain explicit; no earlier ambiguous/heavy attempt was retroactively qualified.

Current Linux Core/official Hamlib 17/17 (1,443 Qt passes, no skips), noSerial
13/13 (268 Qt passes, two expected skips) and all 16 compliance commands pass.
Exact 52 raw notice regions and 1,264 relocated comments are unchanged. The final
worker check confirms all 4,315 tracked hashes and 1,133 preserved binaries,
clean signed source and no task-owned running resources. The lead's closing
commit only updates README, progress, PR-DESCRIPTION and this corrections record;
reviewed product/test/build bytes remain unchanged. No public or hardware action
occurred, and Windows runtime, Linux GUI/x86, physical serial, other clients and
radio/audio/RF bench remain unperformed.
