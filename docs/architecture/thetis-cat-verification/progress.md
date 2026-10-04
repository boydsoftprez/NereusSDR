# Accepted CAT execution ledger

Approved 2026-10-04 by J.J. Boyd (KG4VCF). Sequential crew workers used
OpenAI Codex Sol 6.1, with medium or high effort as listed. The lead accepted
Tasks 1–13 from actual diffs, retained raw command/exit logs and signed commits.
Task 14 records completed local software verification; the lead's one fresh integrated
requirements/code review is still pending. Token usage and total task wall time
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
| 14 Final acceptance / high | This local acceptance/correction commit; exact signed hash is in the retained Task 14 report | Fresh application and 43 named checks; native untyped Tune OFF correction, precise retained applet reasons, reason scan/three catalogue expectation updates; final 1120/1120 ordinary + 11/11 native-window + 22/22 realtime CTests, all EXIT0, covering 1,153 registrations; Qt 19,694/0/374. Refreshed Linux17/official Hamlib and noSerial13; exact compliance/raw bytes/accounting, failed epochs, final report and local PR packet. Lead integrated review and known Windows PTY tooltip correction remain pending. |

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
