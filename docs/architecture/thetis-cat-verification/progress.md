# Accepted CAT execution ledger

Approved 2026-10-04 by J.J. Boyd (KG4VCF). Sequential crew workers used
OpenAI Codex Sol 6.1, with medium or high effort as listed. The lead accepted
Tasks 1–14 from actual diffs, retained raw command/exit logs and signed commits.
The one fresh integrated review and its same-reviewer scoped corrections are
accepted on final product source `be3e0ab35e0d48a5364f6ef5c95e5e6a595e8bf3`.
Refreshed full/platform software verification is accepted. Token usage and total
task wall time
are unknown; measured command durations are preserved in the raw logs.

Implementation starts at `620883fde90b629ec15005fdac8d6ac7fcd43998`, based on
qualified Core `27716f5d6700e1e7d1808acfe478756249828e8c`. Task 14 starts at
`d8c4fc1f258966f8d78e24a807fe48b72457e63a`. These are distinct baselines;
no fetch/rebase or later Core currency claim is implied. Sources remain Thetis
v2.10.3.15 / `3759d096` and AetherSDR `1e0718ad`.

| Task / worker effort | Accepted signed implementation commit | Actual acceptance and material corrections |
| --- | --- | --- |
| 1 Catalogue / medium | `85c82e804135907b6fd1d06ea546918a8e6b0f4f` | Fresh catalogue 1/1 and exact source XML/header checks. 419 descriptors; 752 original records, 285 responses deliberately pending. Lead correction `cbc4cd3c9` preserves all negative signed widths and fixes the false ZZMX dispatch finding. |
| 2 Parser / medium | `c67033f743977759f1d1260894a8a9baddf161d3` | Rebuilt parser/catalogue 2/2; 752 parser records, 467 supplied frames/rejections, distinct from production execution. Standard error scope corrected to actual source. |
| 3 Bindings/lifetime / high | `cef91403cc161b58bd24ebaf7c19c6357d6c25b8` | CAT 3/3, lifecycle/R1 5/5, standalone readiness 1/1. Inert construction, policy-ready start, permanent retirement, public station authority and frozen incarnations. |
| 4 TX ownership / high | `def5473266ef20b1f50eaa5c10c517f11d8e311b` | Fresh 12/12; 11 compliance checks. Reproduced and repaired idle foreign OFF, phase-walk supersession, stale tone restore, gate/pre-observation cancellation and callback lifetimes. |
| 5 RX/VFO / medium | `357fb73dd62058b5c4d14d6004ad73fe76c078dd` | Fresh 10/10; exact notices and 553 source comments. 158 owning production records. Native frequency callback deletion/reentry fixed; signed width overflow returns O; rather than losing digits. |
| 6 RX DSP / medium | `b4c1fd6c7874bff11051a143e145e6807838e5d1` | Fresh 17/17; notices/comments/compliance. 174 owning records; +24 literal V/v/+/- buffer cases. Actual ready WDSP parameter wiring, admitted diversity, ADC frontend and atomic lock notifications verified. |
| 7 TX/global/coverage / high | `fe9a3879c1ee9de6f1a5d1666caf3f4ffb598de9` | Fresh 17/17 and reordered native TX fixture lifetime proof. 349 active registrations and all 783 production records, zero pending. Fixed ZZSN configured-text interpretation, EQ prevalidation, finite ready meter floor and superseded PS disconnect. |
| 8 TCP/reporting / high | `a63fd893a31c4897a6d9fd34c3b318375241ca7f` | Fresh 13/13, 19 compliance Python cases. Real loopback clients, bounded framing/queues, strict GUIDs and actual-state AI; fixed synchronous Qt abort deletion and relevant-target report cancellation. |
| 9 Serial/input PTT / high | `1b201a51d14836e99a89e87f09f15853d06d8674` | Fresh 15/15, final serial 2/2; independent no-SerialPort 3/3 with artifact proof. Actual owned QSerialPort PTY bytes; injected CTS/DSR edge/claims. Corrected queue cap, detach-before-close and persisted device collision. |
| 10 POSIX PTY / high | `b4542c654920cef74b0fd0719d1f5b1b9eb9df1c` | Fresh 16/16, no-SerialPort 4/4. Real immediate first request/no echo, observed HUP/reset/reopen and queue cleanup. Bounded drain before raw-mode change prevents first-request loss; unseen close/reopen gap remains explicit. |
| 11 Live UI/config / high | `0d4b45f3a3e1b432bb71294408a9517e79be3e16` | 26 unchanged-source gates, corrected catalogue 3/3, final GUI 5/5; no-SerialPort 6/6 + final GUI 2/2. Native and scaled actual captures inspected. Fixed nested persistence, retained incarnations, tester keying and page deletion. CAT1 applet button wiring is explicit. |
| 12 Integration/platform / high | `13cf119c68543b72e8e3662a42cf2814e1077bec` | Mac 30/30, Linux 16/16, no-SerialPort 8/8, final affected 2/2 each. Linux native HUP and retained slave-tail failures reproduced and fixed. CI requests optional Qt SerialPort; Windows execution remains pending. |
| 13 Separate Hamlib / high | `d8c4fc1f258966f8d78e24a807fe48b72457e63a` | Mac 26/26; Linux 17/17 and actual Hamlib 4.7.0/554e02b39 client, 14/14 final native MOX proof; no-SerialPort 13/13. Real six client invocations and fresh readback, split/AF/RIT/T/t/disconnect. Mandatory handshake/LOCK terminator corrected from primary Hamlib contracts. Native/scaled UI and exact 23+3 Aether comments pass. |
| 14 Final acceptance / high | `025e7f365552594e441bc1f3c82a2420cb41892a` | Fresh application and 43 named checks; native untyped Tune OFF correction, precise retained applet reasons, reason scan/three catalogue expectation updates; final 1120/1120 ordinary + 11/11 native-window + 22/22 realtime CTests, all EXIT0, covering 1,153 registrations; Qt 19,694/0/374. Refreshed Linux17/official Hamlib and noSerial13; exact compliance/raw bytes/accounting, failed epochs, final report and local PR packet. Initial integrated review subsequently required the five corrections documented below. |

The [one consolidated correction wave](CORRECTIONS.md) starts at accepted
Task 14 head `025e7f3`. Signed `27dd27b08eea4ca04b07be4ee0ba867f3864b202`
addresses owned OFF, mode/filter continuation, combined numeric validation,
desired global snapshots/functional PTT restart and PTY platform/host reasons.
The same reviewer's scoped rereview closed four findings and identified an
introduced live RADE lifecycle defect in F2. Narrow signed
`be3e0ab35e0d48a5364f6ef5c95e5e6a595e8bf3` corrects actual retained codec/route
retirement; all five source findings are now closed. Its current focused result
is 10/10, 466 Qt passes, zero failures and two expected skips.

Final current-source acceptance reconciles 1,153 unique registered executables:
1,120 ordinary successful records (the first attempt was 1,118/1,120, EXIT 8,
with two unchanged full-executable passing replacements), 11/11 native-window,
and 22 realtime records qualified individually with explicit host provenance.
There is no one-command green ordinary attempt or uninterrupted quiet realtime
epoch claim. Totals are 19,838 Qt passes, zero failures and 374 skips across
1,151 Qt-bearing registrations and 1,153 legitimate Qt Totals blocks. The
linger-teardown registration runs three Qt subtests; two registrations are
non-Qt. Current Linux17 passes with 1,443 Qt passes/zero skips and actual official
Hamlib/native MOX proof; noSerial13 passes with 268 Qt passes/two expected skips.
All 16 final compliance commands pass. The [README](README.md) records exact
build/test durations, retained failed attempts, interval qualification and limits.

The final worker handback confirms clean signed source, all 4,315 tracked hashes
and 1,133 preserved binaries unchanged, and no task-owned processes or containers.
The lead independently matched the selected manifest to a fresh CTest inventory,
closed the four existing verification documents and retained all source/test/build
bytes from the reviewed freeze. The closing commit is documentation-only.

Each row's raw evidence/report is retained locally under
`.crew/2026-10-04-thetis-cat-plan/task-N-*`. Normal hooks and GPG signatures
passed for the accepted commits; Task 14 verifies its own normal signed commit
before handback. Lead contract commits between implementations are retained:
`5326a69fc`, `66587fc8b`, `f47d018cf`, `789c35231`, `25d9ecd66`.

The current fixture `executionStatus` is the historical owning-family field:
158 Task 5, 174 Task 6, 305 Task 7, 146 not executed by those earlier owners.
The separate `productionCoverageStatus` records all 783 executed by the combined
production target, including 140 inactive and six router cases. Task 14 reruns
that real target; metadata alone is not execution evidence. Counts remain
Faithful 15, Adapted 163, SourceInert 42, Unavailable 129, Inactive 70.

Unperformed evidence remains Windows SDK/runtime, Linux native GUI/x86,
physical serial cables/CTS/DSR, other logger/digital-mode clients, actual radio
MOX/RF delivery and discovery → connect → I/Q/WDSP → speaker bench. The
unobservable PTY peer gap and Hamlib 4.7 missing-target LOCK extra-read limit
remain documented. No public PR, hosted CI, merge, release or hardware operation
was authorized by this local execution.
