# Phase 3F session state and handoff, 2026-07-28 into 2026-07-30

Working record for the next session on
`feature/phase3f-sub-epic-a-foundation` (PR #293).

---

## 0. Worktree status

**2026-07-30 update.** The concurrent session described here finished at
`282779b0` on 2026-07-29 22:26 and pushed. The worktree was clean and idle
when the 2026-07-30 session picked it up, with nothing in flight and no
uncommitted edits. The warning below is kept because the hazard recurs
whenever chips commit into this branch directly.

Before doing anything, run `git status` and `git log`, and check whether work
is in flight. Committing or rebasing over another session's uncommitted edits
will destroy them.

The task ledger for the Sub-Epic J run is at
`.superpowers/sdd/2026-07-28-phase3f-sub-epic-j-control-plane-plan/progress.md`,
with per-task reports beside it. The PR 293 stabilization pass has its own at
`.superpowers/sdd/2026-07-29-pr293-stabilization-index/`. Both directories are
git-ignored, so they are local-only and do not survive a clean.

Note that the stabilization pass's own report states full-suite, label and
smoke verification were left "controller-owned" and unperformed. The
2026-07-30 session ran them: `all_tests` plus `ctest -j8` was green at
568/568 on `282779b0`, and 570/570 after the three chip commits below.

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
| `task_5a929186` | ANAN-G2E locks up on disconnect | **Closed 2026-07-30.** Became PR #306, merged to main. All four commits (`5baaec33`, `443b5e1e`, `2dfe7f65`, merge `d1b497b2`) confirmed ancestors of this branch's HEAD. Root cause was not our stop frame: teardown reopened ConnectionPanel, whose ctor auto-scans, so a discovery burst hit the radio 7 to 15 ms after `run=0`. Fix is a 3 s post-disconnect discovery quiet period. |
| `task_16e1289c` | RX audio dies mid-session after TX or TUNE | **Code landed and pushed**, `d7c59434` ("the MOX barrier withdrawal cannot strand a slice"), further hardened by finding H3 of the stabilization pass in `644936df` (mixer withdrawal / drain race). **Still bench-gated: do not close until the symptom is confirmed gone on the radio.** |
| `task_c1e6fbad` | Per-slice gaps found during Sub-Epic J | **Closed 2026-07-30**, all three sub-items, on this branch. See below. |

`task_c1e6fbad` closed. **Read this part before starting any chip work.**

The chip's own session had already fixed all three sub-items and had never
pushed. Its three commits sat in
`.claude/worktrees/upbeat-rhodes-7dad5d` on `claude/upbeat-rhodes-7dad5d`,
reachable from no origin ref. The 2026-07-30 session checked the chips
against branch HEAD exactly as the rule below says, saw nothing, and did the
work a second time.

**Checking HEAD is not sufficient.** A chip that commits locally and does not
push is indistinguishable from a chip that did nothing. Before starting chip
work, also check the chip's own worktree:

```
git -C .claude/worktrees/<session-worktree> log --oneline origin/main..HEAD
git -C .claude/worktrees/<session-worktree> branch -r --contains HEAD
```

An empty second command means that work exists nowhere but that folder.

What the branch carries now, after reconciling the two:

- `994177c2` (theirs, cherry-picked) NB1/NB2/SNB detail tuning. Where WDSP
  keeps the state decides the design: `SetEXTANB*` -> `panb[id]`
  (`wdsp/nob.c:376-423`), `SetEXTNOBMode` -> `pnob[id]`
  (`wdsp/nobII.c:658-663`), both per receiver; `SetRXASNBA*` ->
  `rxa[channel].snba` (`wdsp/snb.c:621-670`), per RXA channel. So the five
  NB1/NB2 knobs are per-slice with stream-shared mirroring across
  `slicesOnStream`, matching `nbMode`, and the three SNB knobs stay
  independent. Adds 8 `SliceModel` properties, per-slice per-band
  persistence, and a one-time fallback to the old radio-global keys.
  Supersedes `0a8e63e6` (reverted at `06247179`), which read Thetis's Setup
  page and wrote every knob to every receiver. That is faithful to a console
  with exactly two receivers and loses per-slice control on ours, and it
  misses the Sub-Epic I Task 4b hazard where the stream's single blanking
  pass goes to whichever co-host reaches `processIq` first.
- `6fbd23d3` (theirs, cherry-picked) `rx_volume` plus `rx_anf_enable` live
  broadcast. The `rx_volume` half matches `ca6c33cd` (reverted at `3189cbf8`)
  exactly, down to the citations. The extra half is the one that was missed:
  `rx_anf_enable`'s broadcast hung off `activeNrChanged`, which stopped
  firing on a pure ANF toggle once Task 1 gave ANF its own property, so
  flipping ANF told no connected client until reconnect. Thetis subscribes
  to ANF separately (`ANFChangedHandlers`, TCIServer.cs:6761 [v2.10.3.15]).
- `56174c12` (kept) `setRxApf` / `setRxBin` stored into private arrays and
  read back out, so a TCI client could enable APF or binaural, query it, be
  told it was on, and reach no DSP. Both already had `SliceModel` properties
  wired to `RxChannel`. Routed through `sliceById(rx)` and the dead arrays
  deleted. Functionally equivalent to the chip's `d28bf1c4`; this one also
  records why `setRxNf` stays a stub, namely that upstream is asymmetric
  (query reads per-rx `GetMNF(rx + 1)`, set writes radio-global `TNFActive`,
  TCIServer.cs:1895-1910 [v2.10.3.15]) and NereusSDR has no MNF/TNF model to
  route either half to.

Open PRs are only **#293** (draft, this branch) and **#291** (ready,
`feature/rfkit-rf2ks-applet`, TX audio quality). Everything else has merged.
The chips commit directly into this branch rather than onto their own
branches, which is why coordination matters.

For each chip: confirm what actually landed, confirm it is committed and
pushed, and either fold it into PR #293 or give it its own PR. Do not close a
chip on an agent's report alone. Two fixes on 2026-07-29 were reported DONE
with passing tests and did not work on the bench.

A chip fix that only a bench can confirm stays open until the bench confirms
it. `task_16e1289c` is the live example.

### Sessions holding unpushed work, 2026-07-30

Archiving a session deletes its worktree. These were found holding work that
exists on no origin ref, so they must not be archived until it is landed or
deliberately dropped:

| Worktree | At risk |
| --- | --- |
| `upbeat-rhodes-7dad5d` | the chip work above; two of three now cherry-picked onto this branch |
| `container-refactor-worktree-cf9a3b` | 46 commits, meters / ANAN MM refactor |
| `heuristic-keller-530529` | 3 design docs, remote daemon (nereusd) architecture |
| `wonderful-boyd-27018b` | 3 uncommitted files plus 1 unpushed commit |
| `tx-display` | 1 uncommitted file, 32 commits |
| 6 others | 1 to 2 uncommitted files each: `awesome-yalow-c268f7`, `elegant-liskov-73ad75`, `gifted-pike-182801`, `strange-dubinsky-6f972f`, `sweet-dewdney-00b0fd`, `thirsty-ellis-558747` |

Confirmed safe, everything on origin: `angry-maxwell-3a8f5b` (the G2E bench
session), `intelligent-ellis-8e5932` (the `task_16e1289c` chip), and the two
`agent-*` 3F worktrees.

The session list's `prNumber` / `prState` fields are stale and cannot be
trusted: several PRs it reports OPEN are merged. Re-check with `gh pr view`
before acting on them.

### One Codex fix that belongs here and is not here

`9c3f9f72` (`fix(spectrum): initialize and release the overlay textures
[Codex P1 + P2]`) lives only on `feature/rfkit-rf2ks-applet`, PR #291, which
has never merged to main. Both of its defects are live in this branch's tree:
`m_overlayStatic` / `m_overlayDynamic` are allocated without `fill()` so the
waterfall region composites uninitialised memory after init or resize, and
`releaseResources()` unlocks only `m_overlayDynamic` while
`initOverlayPipeline()` locks both and re-runs on device or surface
recreation, leaking a MemoryLock registration every time.

It matters more here than where it was fixed. This branch is the
multi-panadapter epic, so every pan is its own SpectrumWidget with its own
overlay pair and both faults scale with pan count. The patch applies cleanly
to this branch (one file, +26 lines). PR #291 is CONFLICTING and cannot merge
as-is, so waiting for it couples a 26-line fix to an 85-file untangle.

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

**2. Chip closeout**, per section 2. Two of three are closed; `task_16e1289c`
needs the bench.

**3. Merge decision on PR #293.** Roughly +67k lines over 103 commits, still
draft, and **no review of any kind has ever run on it** (`reviewCount` is
literally 0; the four comments on the PR are CT1IQI's two technical notes and
two replies). Confirmed 2026-07-30: all nine CI checks pass, `mergeable` is
MERGEABLE and `mergeStateStatus` is CLEAN, and the full suite is green at
570/570 through the `all_tests` target. So nothing mechanical blocks the
merge. What blocks it is judgement: an unreviewed +67k diff that reorganises
the RX data plane, and a bench matrix that has never been run. Gate on those
two, not on the suite.

**4. Diversity.** The contributor's specification is the agreed target: DDC n
and n+1 with n even, n synchronous on ADC0, n+1 on ADC1, Alex0 and Alex1
filtered identically, diversity receivers allocated first, and more than one
diversity receiver supported. None of that is implemented. Needs a DivId
allocator that does not exist: WDSP `pdiv[]` is a 2-slot array keyed by a
diversity id rather than the channel id, and `pdiv[id]` dereferences
unallocated state and **crashes for id >= 2**. Sub-Epic G landed only 4 of
its 25 tasks.

**5. Per-slice antenna versus ADC routing. Diagnosed 2026-07-30, not fixed.**
This was recorded here as a design tension that "works for the current
operator flow". It does not. Two slices on the SAME band, second slice set to
EXT1: the selection applies, a sibling slice reverts it 2.6 s later, and
neither pan ever reaches chain 1.

Two root causes, both verified in code against a live G2 log. The antenna is
stored per band rather than per slice, so two slices on 80m share one value
and the one on ANT1 clears the other's EXT1 for the whole band. And
`SliceStreamAllocator` places slices by frequency alone, so it co-hosts a pair
whose antennas have just made co-hosting impossible: one stream is one DDC,
and one DDC has one ADC.

Both have to move together, because fixing either alone leaves the symptom.
Full diagnosis, log evidence, and the constraints a fix must respect:
`docs/architecture/2026-07-30-per-slice-antenna-adc-routing.md`.

Agreed plan: PR #293 ships as is with this recorded as a known limitation,
and the fix runs as its own branch afterwards. Row 17 of the G2 matrix now
covers the same-band case that rows 15 and 16 structurally cannot catch.

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

**2026-07-30 adds three more, all found by closing `task_c1e6fbad`:**

- `setRxApf` / `setRxBin` stored into a private array and read back out of
  it. `stub_dsp_toggles_roundtrip` asserted set-then-get and passed, because
  round-trip through a variable is a tautology. The property that decides
  anything, and that was already wired to WDSP, was never consulted. This was
  the third appearance of this exact defect on this exact block of shims.
- The NB Setup page's crash gate probed channel 0, then the writes it guarded
  also went to channel 0. It read the same wrong thing the code decided on,
  so it looked consistent while making the whole page dead whenever channel 0
  in particular was down.
- The stale comment is its own failure mode. The APF/BIN header said
  "SliceModel doesn't expose these as Q_PROPERTYs yet" long after it did, and
  that sentence is why the shims kept getting skipped. Comments asserting an
  absence need re-checking as hard as code does.

Corollary worth adding: **assert the destination, not the echo.** A test that
sets a value and reads it back proves storage, never effect.

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
