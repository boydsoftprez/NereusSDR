# Phase 3F session state and handoff, 2026-07-28 into 2026-07-29

Working record for the next session. Written at the end of a long day on
`feature/phase3f-sub-epic-a-foundation` (PR #293).

---

## 0. READ THIS FIRST: another session is live in this worktree

At the time of writing, a chip session is **actively committing into this
same branch and worktree**. When this file was written:

- HEAD was `b62cfba9`, four commits past the `40cc9190` that was last
  verified green at 568/568.
- Four files were **uncommitted and mid-edit**:
  `src/models/RadioModel.{h,cpp}`,
  `tests/tst_band_plan_guard_mox_rejection.cpp`,
  `tests/tst_radio_model_push_tx_mode_and_bandpass.cpp`.
- Those four commits are a "PR 293 stabilization pass" (scope doc, plan
  doc, a compliance provenance fix, and a slice/pan ownership fix).

**Do not assume this branch is green, and do not assume it is idle.** Before
doing anything, run `git status` and `git log`, and check whether work is in
flight. Committing or rebasing over another session's uncommitted edits will
destroy them.

The task ledger for the Sub-Epic J run is at
`.superpowers/sdd/2026-07-28-phase3f-sub-epic-j-control-plane-plan/progress.md`,
with per-task reports beside it. That directory is git-ignored, so it is
local-only and does not survive a clean.

---

## 1. Environment

**Worktree:** `/Users/j.j.boyd/NereusSDR/.worktrees/phase3f-sub-epic-a-foundation`
**Branch:** `feature/phase3f-sub-epic-a-foundation`, PR #293, still **draft**

Do **not** create a new worktree and do **not** use worktree isolation for
agents. Isolation lands on the wrong base and produces bogus attribution
failures. Around 100 worktrees already exist on this machine and many are
marked prunable; do not add more.

Read `CLAUDE.md` and `CONTRIBUTING.md` in the worktree before starting.

### Three traps that cost real time today

1. **Test binaries are `EXCLUDE_FROM_ALL`.** A plain `cmake --build build`
   does not build them, and a bare `ctest` then runs **stale binaries** and
   reports false green. This hid a guaranteed segfault and five hollow tests
   for most of a day. Always:

   ```bash
   cmake --build build --target all_tests -j8 && (cd build && ctest -j8)
   ```

   Treat any "the suite passes" claim not made through that target as
   unverified.

2. **`pkill -f` on the build path does not match** the running app, because
   it launches with a relative path so the path never appears in its command
   line. A stale instance survived several relaunches before this was found.
   Use `pkill -x NereusSDR`.

3. **`core.hooksPath` points at `/Users/j.j.boyd/NereusSDR/scripts/git-hooks`,
   the main checkout**, not this worktree. Hook changes made here do not fire
   locally until they reach main. CI does run them. Main commit `b8fd5a6c`
   improves worktree resolution for the tag checker.

Also: `rerere` is enabled with autoupdate on this branch and produced a
resolution that **compiled as an error**. Consider `git config
rerere.autoupdate false`.

---

## 2. Chip management, explicitly part of the next session's job

Three chips were spawned today. The next session owns driving these to
committed, pushed, and on a PR ready for merge.

| Chip | Subject | State |
| --- | --- | --- |
| `task_5a929186` | ANAN-G2E locks up on disconnect | **Done.** Became PR #306, now merged to main. Root cause was not our stop frame: teardown reopened ConnectionPanel, whose ctor auto-scans, so a discovery burst hit the radio 7 to 15 ms after `run=0`. Fix is a 3 s post-disconnect discovery quiet period. |
| `task_16e1289c` | RX audio dies mid-session after TX or TUNE | Work landed in this branch as `d7c59434` ("the MOX barrier withdrawal cannot strand a slice"). **Verify it actually fixes the bench symptom before closing.** |
| `task_c1e6fbad` | Per-slice gaps found during Sub-Epic J | Open. Covers NB tuning sliders hardcoded to channel 0, the TCI live-broadcast `rx_volume` path in `TciServer.cpp`, and APF/BIN stub shims. |

Open PRs are only **#293** (draft, this branch) and **#291** (ready,
`feature/rfkit-rf2ks-applet`, TX audio quality). Everything else has merged.
The chips are committing directly into this branch rather than onto their own
branches, which is why coordination matters.

For each chip: confirm what actually landed, confirm it is committed and
pushed, and either fold it into PR #293 or give it its own PR. Do not close a
chip on an agent's report alone. Two fixes today were reported DONE with
passing tests and did not work on the bench.

---

## 3. What landed today

**Sub-Epic J, per-slice control plane**, 11 tasks. Design and plan at
`docs/architecture/2026-07-28-phase3f-sub-epic-j-control-plane-{design,plan}.md`.
Every DSP control now addresses the slice the operator is working. One rule,
two clauses: a control drawn on a flag targets that flag's slice; a control
attached to no slice targets the active slice. Enforced by
`scripts/verify-no-gui-dsp-access.py`.

**Bench fixes**, all from live ANAN-G2E sessions:

- Mixer readiness barrier no longer mistakes a late slice for a stopped one.
  Fixed scratchy and robotic audio with two pans. Verified by a 1-hour
  on-air rag chew with no pops or clicks.
- PureSignal crashed the app on MOX: `pscc` ran on a WDSP channel with no
  TXA, because Phase 3F moved the TXA above the RX block and one literal was
  left behind.
- The MOX RX-audio gate had **never been connected** despite the header
  claiming RadioModel wired it. RX audio was never muted during transmit.
- 10 ms raised-cosine up-slew ported from `create_aaslew`, fixing a
  "kerplunk" on unkey.
- Each flag places at its own frequency and keeps its own passband.
- Click-to-tune follows the selected flag. Pan-0 was skipping
  `wireSpectrumForPan` and using lambdas that captured `activeSlice()` by
  value, so it drove Slice A for the life of the session.
- The active flag now stacks above its siblings.
- The right-click display menu no longer opens off-screen.

**Three ADC and filter defects**, found by auditing against a contributor
review (see section 5):

- Antenna-driven ADC routing existed only in `P2CodecOrionMkII`.
  `P2CodecSaturn` overrode `applyDdcAssignment` and never got it, and every
  test constructed the OrionMkII codec. Saturn is the ANAN-G2, so the
  feature did nothing on the radio it was written for. Saturn's override is
  deleted; it now inherits, with an anti-drift test sweeping both codecs.
- `CodecContext::adcCtrl` was never seeded on Protocol 2, so the diversity
  DDC0/DDC1 pair both sat on ADC0, the same physical input twice. Thetis
  defaults `rx_adc_ctrl1` to 4 (`console.cs:15099`, encoding proven at
  `setup.cs:16934`). Now seeded and gated on the board's ADC count.
- The Alex filter analysis grouped slices by a key pinned to ADC0, so
  chain 1 never got a decision. Now follows the DDC.

Related discovery: **nothing recomputed the DDC assignment on an antenna
change**, so the routing was invisible even where it existed.

---

## 4. Next work, in priority order

**1. Bench matrix. Needs the operator and a radio. Highest value.**
Rows 15 and 16 of `docs/architecture/2026-05-26-phase3f-verification/g2-results.md`
are executable for the first time and verify today's ADC and filter work on
real hardware. Row 15 now requires selecting the second slice's flag first;
that step is load-bearing, because the antenna is held per band and its label
syncs onto the active slice only. Also the 8-row Sub-Epic J matrix in its
plan, and the **two-pan simultaneous audio listen test**, which is what
started this whole thread and still has not been done.

**2. Chip closeout**, per section 2.

**3. Merge decision on PR #293.** Roughly +59k lines over 100+ commits,
draft, and **no Codex review has ever run on it**. Gate the merge on the
bench matrix rather than the suite alone.

**4. Diversity.** The contributor's specification is the agreed target: DDC n
and n+1 with n even, n synchronous on ADC0, n+1 on ADC1, Alex0 and Alex1
filtered identically, diversity receivers allocated first, and more than one
diversity receiver supported. None of that is implemented. Needs a DivId
allocator that does not exist: WDSP `pdiv[]` is a 2-slot array keyed by a
diversity id rather than the channel id, and `pdiv[id]` dereferences
unallocated state and **crashes for id >= 2**. Sub-Epic G landed only 4 of
its 25 tasks.

**5. Design tension worth its own look.** The antenna is per-band-global in
`AlexController` while ADC routing is per-slice in the codec, so the two
models do not fully compose on a multi-slice radio. It works for the current
operator flow.

---

## 5. Contributor review, answered

CT1IQI left two detailed technical comments on PR #293 (2026-05-31 and
2026-06-04) while building a G2 xDMA interface. He was right on essentially
every point, and chasing them found four defects nobody knew about. A reply
was posted 2026-07-29 as `issuecomment-5123749470`.

Full audit with file and line evidence:
`.superpowers/sdd/2026-07-28-phase3f-sub-epic-j-control-plane-plan/ct1iqi-audit.md`

Still owed to him: the diversity work in item 4 above, and a separate issue
for his **sample-rate budget** idea (bound receiver count against transport
bandwidth and WDSP headroom, rather than only against DDC count). His framing
is better than what exists.

---

## 6. Facts that cost time to learn

- `RadioModel::activeSliceChanged(int)` carries a **list position, not a
  slice id**. Resolve through `activeSlice()->sliceIndex()`. This caught four
  separate agents.
- Slice ids are not list positions anywhere. `removeSlice` does not renumber
  survivors. Check which any API expects.
- `NbMode` is `{ Off = 0, NB = 1, NB2 = 2 }`. There is no `NB1`.
- **`MainWindow` is not constructible in the test harness.** Logic in a
  MainWindow lambda cannot be tested, and two fixes today shipped green and
  failed on the bench for exactly that reason. Put logic on
  `PanadapterStack`, `SpectrumWidget`, `PanadapterApplet` or `RadioModel`.
- `wireSliceSignals` early-returns whenever `m_connection` is null, which is
  always true for a bare `RadioModel` in tests.
- **ADC count is not filter-chain count.** ANAN-100D and 200D report two
  ADCs but are absent from Thetis's `setAlex2HPF` model list: two ADCs, one
  driven chain. Use `rxFilterChainCount`, not `adcCount` and not `hasAlex2`.
- The authoritative source of DDC settings is `DdcAssignment` plus the
  per-board codec, **not** `ReceiverManager`.
  `ReceiverConfig::isPureSignalFeedback` and `isDiversity` are dead state,
  never written and never read. `ReceiverManager::setDiversityEnabled` has no
  production caller.
- The noise blanker is per DDC stream, not per slice: `ANB panb` and
  `NOB pnob` sit in `struct _rcvr` beside `double* audio[cmMAXSubRcvr]`
  (Thetis `cmaster.h:74-82`). Co-hosted slices cannot have independent
  blankers; `RadioModel` mirrors `nbMode` across them.

---

## 7. The pattern worth acting on

Five times today a guard existed and guarded nothing:

- The MOX audio gate was implemented, tested, documented as wired, and never
  connected. It ran only inside one unit test.
- `ReceiverConfig::isPureSignalFeedback` is written by nobody and read by
  nobody, which led a contributor to reasonably doubt the DDC data.
- A test precondition asserted `receiverConfig(...).adcIndex == 1` with a
  comment saying it existed to stop exactly the bug that then shipped. It
  passed, because the field it checked had stopped being the field that
  decides.
- Four tests for antenna-driven ADC routing all instantiated the wrong codec,
  so they passed while the feature was entirely absent on the target SKU.
- `rerere` auto-applied a conflict resolution that compiled as an error.

Each looked like coverage. That pattern found more real bugs today than
writing new code did, and a deliberate sweep for it would likely find more.
The general shape: **a check that reads a different thing than the code
decides on is worse than no check, because it buys false confidence.**

---

## 8. Standing rules

GPG-sign every commit; never `--no-gpg-sign`. **No em-dashes** in any
authored text, including commit messages and code comments. Source-first from
Thetis at `../Thetis` (v2.10.3.15, `3759d096`), cites as
`// From Thetis <file>:<line> [v2.10.3.15]` verified against the real file,
upstream author tags preserved verbatim. Run the pre-port checklist before
reading upstream source. TDD, and a test that cannot fail is not a test:
prove it fails before implementing, ideally by stubbing the new behaviour and
watching it go red. Targeted tests during iteration, full suite once at the
end. Never post anything to GitHub without drafting it in chat and getting
explicit approval first.
