# HL2 Two-Panadapter Cap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Raise the Hermes Lite 2 from 1 operator slice to 2 panadapters and 5 flags, matching what mi0bot has always supported, and stop announcing four DDCs to a radio that needs two.

**Architecture:** Six tasks. Tasks 1 to 4 correct the capability row and the code path that enforced it; they change no wire bytes. Tasks 5 and 6 are the maintainer-approved deviation from mi0bot and land last so they can be reverted alone. Task 5 is a safety fix that must precede Task 6.

**Tech Stack:** C++20, Qt6, QtTest, CMake + Ninja.

**Design doc:** [docs/architecture/2026-07-31-hl2-slice-cap-design.md](../../architecture/2026-07-31-hl2-slice-cap-design.md). Read §2, §4.4, §5 and §6 before starting.

## Global Constraints

- **mi0bot-Thetis at `../mi0bot-Thetis/` is the only behavioural authority for the HL2.** ramdor Thetis has no `HERMESLITE` case in `UpdateDDCs` at all. `P1CodecStandard` implements ramdor's HERMES-class arm and is a different SKU family; consult it for C++ shape only, never for HL2 behaviour.
- **Every new or modified inline upstream cite carries a version stamp.** mi0bot cites use `[v2.10.3.13-beta2]`. Format matches what is already in these files: `// From mi0bot console.cs:8425-8429 [v2.10.3.13-beta2]:`.
- **GPG-sign every commit.** Never pass `--no-gpg-sign`. Never add a `Co-Authored-By: Claude` trailer.
- **No em-dashes** in commit messages, code comments, or docs. Use periods, colons, semicolons, parentheses or commas.
- **Do not run the full test suite between tasks.** Build and run the single named target per task. `ctest -L core` once at the end of Task 4, full suite once at the end of Task 6. See [docs/development/fast-test-loop.md](../../development/fast-test-loop.md).
- **Branch:** `fix/hl2-two-slice-cap`, worktree `.claude/worktrees/hl2-two-slice`, based on `feature/phase3f-sub-epic-a-foundation` (PR #293). Do not rebase onto `main`; these fields do not exist there.
- **Do not merge, and do not post anything to GitHub.** Maintainer sign-off is required (design doc §10).
- **Pre-commit hooks run automatically.** If a commit needs the Thetis path, prefix with `NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`.

## Build Commands

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

Per-test loop used throughout:

```bash
cmake --build build --target <target> && ctest --test-dir build -R '^<target>$' --output-on-failure
```

## File Structure

| File | Change | Responsibility |
| --- | --- | --- |
| `src/core/codec/P1CodecHl2.cpp` | Modify `applyDdcAssignment` (Task 1), `applyPureSignalDdcConfig` (Task 6) | Per-board HL2 wire policy. The only file carrying HL2 DDC behaviour. |
| `src/core/BoardCapabilities.cpp` | Modify `kHermesLite`, `kHermesLiteRxOnly` (Task 2) | Per-SKU capability rows. |
| `docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md` | Modify §2 table (Task 2) | The table the wrong value came from. Fixed in the same commit as the value. |
| `src/models/RadioModel.cpp` | Modify `invokeCodecDdcAssignment` (Task 4) | Pushes stream-1 liveness into ReceiverManager. |
| `src/core/P1RadioConnection.cpp` | Modify `applyPsDdcConfig` (Task 5) | Routes receiver-count changes through the stream restart. |
| `tests/tst_p1_codec_hl2.cpp` | Add cases (Tasks 1, 6) | HL2 codec behaviour. |
| `tests/tst_board_capabilities_phase3f.cpp` | Modify (Tasks 2, 3) | Capability-row values and the table-wide invariant. |
| `tests/tst_p1_hl2_rx2_wiring.cpp` | Create (Tasks 4, 5) | End-to-end wiring: model to ReceiverManager, and count-change restart. |

---

### Task 1: Re-port the truncated rx2 branches in `P1CodecHl2::applyDdcAssignment`

`applyDdcAssignment` stops at the line before mi0bot's `if (rx2_enabled)` in both branches that have one. It also hard-returns when slice 0 is dead, which strands a slice-B-only configuration.

**Files:**
- Modify: `src/core/codec/P1CodecHl2.cpp:753-854`
- Test: `tests/tst_p1_codec_hl2.cpp`

**Interfaces:**
- Consumes: `DdcAssignment` (`src/core/DdcAssignment.h`), fields `p1RxCount`, `nDdc`, `p1DdcConfig`, `ddcEnable`, `syncEnable`, `rate[8]`, `adcCtrl1`, `adcCtrl2`, `streamDdc[5]`, `psFwdDdc`, `psRevDdc`. `SliceConfig` (`src/core/codec/CodecContext.h:38-46`), fields used here: `.live`, `.sampleRateHz`.
- Produces: `P1CodecHl2::applyDdcAssignment` assigns `streamDdc[1] = 1` in the two plain-RX branches. Task 2 depends on the resulting capacity being 2.

- [ ] **Step 1: Write the failing tests**

Add to `tests/tst_p1_codec_hl2.cpp`, inside the `private slots:` section:

```cpp
    // Phase 3F: HL2 supports a second receiver on DDC1.
    // From mi0bot console.cs:8425-8429 [v2.10.3.13-beta2], inside
    // case HPSDRModel.HERMESLITE, the !mox && !diversity arm:
    //   if (rx2_enabled)
    //   {
    //       DDCEnable += DDC1;
    //       Rate[1] = rx2_rate;
    //   }
    void ddc_assignment_plain_rx_gives_stream1_ddc1() {
        P1CodecHl2 codec;
        CodecContext ctx{};
        ctx.mox = false;
        ctx.diversity = false;
        ctx.puresignalRun = false;

        std::array<SliceConfig, 5> slices{};
        slices[0].live = true;
        slices[0].sampleRateHz = 192000;
        slices[1].live = true;
        slices[1].sampleRateHz = 192000;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, slices);

        QCOMPARE(a.streamDdc[0], 0);
        QCOMPARE(a.streamDdc[1], 1);
        QCOMPARE(a.ddcEnable, 1 + 2);          // DDC0 + DDC1
        QCOMPARE(a.rate[0], 192000);
        QCOMPARE(a.rate[1], 192000);
        QCOMPARE(a.p1DdcConfig, 4);
        QCOMPARE(a.syncEnable, 0);
    }

    // From mi0bot console.cs:8453-8457 [v2.10.3.13-beta2], the
    // mox && !diversity && !puresignal arm, same rx2 block.
    void ddc_assignment_mox_no_ps_gives_stream1_ddc1() {
        P1CodecHl2 codec;
        CodecContext ctx{};
        ctx.mox = true;
        ctx.diversity = false;
        ctx.puresignalRun = false;

        std::array<SliceConfig, 5> slices{};
        slices[0].live = true;
        slices[0].sampleRateHz = 192000;
        slices[1].live = true;
        slices[1].sampleRateHz = 96000;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, slices);

        QCOMPARE(a.streamDdc[1], 1);
        QCOMPARE(a.ddcEnable, 1 + 2);
        QCOMPARE(a.rate[1], 96000);
    }

    // Slice B alone must still get a DDC. The old early return dropped the
    // whole assignment when slices[0] was dormant.
    void ddc_assignment_stream1_only_still_assigns() {
        P1CodecHl2 codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> slices{};
        slices[1].live = true;
        slices[1].sampleRateHz = 192000;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, slices);

        QCOMPARE(a.streamDdc[0], -1);
        QCOMPARE(a.streamDdc[1], 1);
    }

    // PureSignal reclaims DDC0+DDC1 as a sync pair, so stream 1 is
    // suppressed rather than left bound to a repurposed DDC.
    // From mi0bot console.cs:8469-8488 [v2.10.3.13-beta2].
    void ddc_assignment_ps_mox_suppresses_stream1() {
        P1CodecHl2 codec;
        CodecContext ctx{};
        ctx.mox = true;
        ctx.puresignalRun = true;

        std::array<SliceConfig, 5> slices{};
        slices[0].live = true;
        slices[0].sampleRateHz = 192000;
        slices[1].live = true;
        slices[1].sampleRateHz = 192000;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, slices);

        QCOMPARE(a.streamDdc[1], -1);
        QCOMPARE(a.syncEnable, 2);             // DDC1 is the sync partner

        // Deliberately NOT asserting psFwdDdc / psRevDdc here. See the
        // note below this task: the two are inconsistent with
        // applyPureSignalDdcConfig today and pinning either value in a test
        // would freeze a question that belongs to the maintainer.
    }

    // No arm of the mi0bot HERMESLITE case enables anything above DDC1.
    // DDC2 and DDC3 are the PureSignal pair (console.cs:8757-8762 GetDDC:
    // rx1 = 0; rx2 = 1; psrx = 2; pstx = 3).
    void ddc_assignment_never_assigns_above_ddc1() {
        P1CodecHl2 codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> slices{};
        for (int i = 0; i < 5; ++i) {
            slices[i].live = true;
            slices[i].sampleRateHz = 192000;
        }

        const DdcAssignment a = codec.applyDdcAssignment(ctx, slices);

        QCOMPARE(a.streamDdc[2], -1);
        QCOMPARE(a.streamDdc[3], -1);
        QCOMPARE(a.streamDdc[4], -1);
    }
```

Add `#include "core/DdcAssignment.h"` and `#include "core/codec/CodecContext.h"` to the include block at the top of the file if not already present.

- [ ] **Step 2: Run tests to verify they fail**

```bash
cmake --build build --target tst_p1_codec_hl2 && ctest --test-dir build -R '^tst_p1_codec_hl2$' --output-on-failure
```

Expected: FAIL. `ddc_assignment_plain_rx_gives_stream1_ddc1` reports `streamDdc[1]` is `-1`, not `1`. `ddc_assignment_stream1_only_still_assigns` reports `streamDdc[1]` is `-1` because of the early return.

- [ ] **Step 3: Replace the body of `applyDdcAssignment`**

In `src/core/codec/P1CodecHl2.cpp`, replace lines 757 to 782 (from `DdcAssignment a{};` through `const int rx1Rate = slices[0].sampleRateHz;`) with:

```cpp
    DdcAssignment a{};
    constexpr int DDC0bit = 1;
    constexpr int DDC1bit = 2;

    // From mi0bot console.cs:8412-8413 [v2.10.3.13-beta2]:
    //   case HPSDRModel.HERMESLITE: // MI0BOT: HL2 (at console.cs:8409)
    //   P1_rxcount = 4;   // RX4 used for puresignal feedback
    //   nddc = 4;
    //MI0BOT  [HL2 case-statement marker at console.cs:8409 — within ±5 of 8412]
    a.p1RxCount = 4;  // RX4 used for puresignal feedback
    a.nDdc = 4;

    // Phase 3F: stream 0 on DDC0, stream 1 on DDC1.
    //
    // Streams 2-4 are never assigned on the HL2. No arm of the mi0bot
    // HERMESLITE case enables anything above DDC1, and DDC2/DDC3 are the
    // PureSignal pair (mi0bot console.cs:8757-8762 [v2.10.3.13-beta2]
    // GetDDC() returns rx1 = 0; rx2 = 1; psrx = 2; pstx = 3 for HL2 P1
    // PS-MOX).
    //
    // Neither stream is assumed live. A slice-B-only configuration is
    // reachable whenever slice A is removed from a two-slice layout, and an
    // early return on slices[0] stranded it.
    const bool rx1Live = slices[0].live;
    const bool rx2Live = slices[1].live;
    const int  rx1Rate = rx1Live ? slices[0].sampleRateHz : 0;
    const int  rx2Rate = rx2Live ? slices[1].sampleRateHz : 0;

    if (rx1Live) { a.streamDdc[0] = 0; }
```

Then in the `if (!ctx.mox) { if (!ctx.diversity) {` branch, after `a.adcCtrl2 = 0;`, extend the cite comment and add the rx2 block so the branch reads:

```cpp
            // From mi0bot console.cs:8417-8430 [v2.10.3.13-beta2]:
            //   P1_DDCConfig = 4; DDCEnable = DDC0; SyncEnable = 0;
            //   Rate[0] = rx1_rate; cntrl1 = 0; cntrl2 = 0;
            //   if (rx2_enabled)
            //   {
            //       DDCEnable += DDC1;
            //       Rate[1] = rx2_rate;
            //   }
            a.p1DdcConfig = 4;
            a.ddcEnable = DDC0bit;
            a.syncEnable = 0;
            a.rate[0] = rx1Rate;
            a.adcCtrl1 = 0;
            a.adcCtrl2 = 0;

            // From mi0bot console.cs:8425-8429 [v2.10.3.13-beta2]
            if (rx2Live) {
                a.ddcEnable += DDC1bit;
                a.rate[1] = rx2Rate;
                a.streamDdc[1] = 1;
            }
```

And in the `} else { if (!ctx.diversity && !ctx.puresignalRun) {` branch, likewise:

```cpp
            // From mi0bot console.cs:8444-8458 [v2.10.3.13-beta2]:
            //   P1_DDCConfig = 4; DDCEnable = DDC0; SyncEnable = 0;
            //   Rate[0] = rx1_rate; cntrl1 = 0; cntrl2 = 0;
            //   if (rx2_enabled)
            //   {
            //       DDCEnable += DDC1;
            //       Rate[1] = rx2_rate;
            //   }
            a.p1DdcConfig = 4;
            a.ddcEnable = DDC0bit;
            a.syncEnable = 0;
            a.rate[0] = rx1Rate;
            a.adcCtrl1 = 0;
            a.adcCtrl2 = 0;

            // From mi0bot console.cs:8453-8457 [v2.10.3.13-beta2]
            if (rx2Live) {
                a.ddcEnable += DDC1bit;
                a.rate[1] = rx2Rate;
                a.streamDdc[1] = 1;
            }
```

Leave the two diversity branches and the PS-MOX branch exactly as they are. They set `syncEnable = DDC1bit` and never touch `streamDdc[1]`, which is the suppression the design doc §5 requires.

Delete the stale comment block at old lines 761-766 (`// HL2: maxSlices=1; only stream 0 is used...`) and the `if (!slices[0].live) { return a; }` it introduced. Delete the stale sentence "The early return above already guarantees slices[0].live here."

- [ ] **Step 4: Run tests to verify they pass**

```bash
cmake --build build --target tst_p1_codec_hl2 && ctest --test-dir build -R '^tst_p1_codec_hl2$' --output-on-failure
```

Expected: PASS, all cases including the pre-existing bank-11 attenuator cases.

- [ ] **Step 5: Commit**

```bash
git add src/core/codec/P1CodecHl2.cpp tests/tst_p1_codec_hl2.cpp
git commit -S -m "fix(3f): re-port the HL2 rx2 branches dropped from applyDdcAssignment

applyDdcAssignment stopped one line short of mi0bot's rx2_enabled block in
both arms that have one, so DDC1 was never handed to a second stream:

  // mi0bot console.cs:8425-8429 and :8453-8457 [v2.10.3.13-beta2]
  if (rx2_enabled)
  {
      DDCEnable += DDC1;
      Rate[1] = rx2_rate;
  }

Restores both, assigning streamDdc[1] = 1 alongside the enable bit and rate.
The diversity and PureSignal arms are untouched: they claim DDC1 as a sync
partner and correctly leave stream 1 unassigned.

Also drops the early return on a dormant slices[0], which stranded a
slice-B-only layout, and the maxSlices=1 comment that justified it.

No wire bytes change. p1RxCount and nDdc stay at 4."
```

> **Noticed while working here, deliberately not fixed.** `P1CodecHl2.cpp:846-849` sets `a.psFwdDdc = 0; a.psRevDdc = 1;` under a comment that reads "same as applyPureSignalDdcConfig — psFbDdc=2, txMonDdc=3". The comment contradicts its own code, and `applyPureSignalDdcConfig` at `:670,672` really does set 2 and 3. It has never mattered because `applyDdcAssignment` is not pushed to the P1 wire (`RadioModel.cpp:14281`), but it becomes live the moment Phase 3F Sub-Epic C connects that path, and PureSignal would then be aimed at DDC0/DDC1 instead of the DDC2/DDC3 pair mi0bot specifies at `console.cs:8757-8762 [v2.10.3.13-beta2]`. Out of scope here: this plan does not touch the PS branch. Raise it with the maintainer rather than changing it in passing.

---

### Task 2: Correct the capability rows and the design-doc table

**Files:**
- Modify: `src/core/BoardCapabilities.cpp:800-801` (`kHermesLite`), `:907-908` (`kHermesLiteRxOnly`)
- Modify: `docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md` §2 table
- Test: `tests/tst_board_capabilities_phase3f.cpp:86`, `:297`

**Interfaces:**
- Consumes: `P1CodecHl2::applyDdcAssignment` capacity of 2 from Task 1. Without it, `user_ddc_count_never_exceeds_native_codec_capacity` fails.
- Produces: `BoardCapsTable::forBoard(HPSDRHW::HermesLite).userDdcCount == 2` and `.maxSlices == 5`. Task 3's invariant reads these.

- [ ] **Step 1: Update the failing expectations**

In `tests/tst_board_capabilities_phase3f.cpp`, change line 86 from `QCOMPARE(caps.maxSlices, 1);` to `QCOMPARE(caps.maxSlices, 5);` and line 297 from `QCOMPARE(capabilitiesFor(HPSDRModel::HERMESLITE).userDdcCount, 1); // DDC0 only` to:

```cpp
        QCOMPARE(capabilitiesFor(HPSDRModel::HERMESLITE).userDdcCount, 2); // DDC0 + DDC1
```

Add a new case documenting why the two axes differ on this row:

```cpp
    // HL2 is the second row where maxSlices exceeds userDdcCount, after the
    // ANAN-G2E. Two DDC windows (mi0bot console.cs:8425-8429
    // [v2.10.3.13-beta2]) with up to five flags sharing them: a slice inside
    // an active window costs no DDC and no wire bandwidth.
    void hermeslite_flags_exceed_panadapters()
    {
        const BoardCapabilities& caps = BoardCapsTable::forBoard(HPSDRHW::HermesLite);
        QCOMPARE(caps.userDdcCount, 2);
        QCOMPARE(caps.maxSlices, 5);
        QCOMPARE(BoardCapsTable::forBoard(HPSDRHW::HermesLiteRxOnly).userDdcCount, 2);
        QCOMPARE(BoardCapsTable::forBoard(HPSDRHW::HermesLiteRxOnly).maxSlices, 5);
    }
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build build --target tst_board_capabilities_phase3f && ctest --test-dir build -R '^tst_board_capabilities_phase3f$' --output-on-failure
```

Expected: FAIL. `hermeslite_flags_exceed_panadapters` reports `userDdcCount` is 1, not 2.

- [ ] **Step 3: Update both capability rows**

In `src/core/BoardCapabilities.cpp`, replace lines 800-801 with:

```cpp
    // Phase 3F: HL2 gets two panadapters and five flags.
    //
    // From mi0bot console.cs:8425-8429 [v2.10.3.13-beta2], inside
    // case HPSDRModel.HERMESLITE:
    //   if (rx2_enabled)
    //   {
    //       DDCEnable += DDC1;
    //       Rate[1] = rx2_rate;
    //   }
    // repeated at :8453-8457 for the mox path. No arm of that case enables
    // anything above DDC1, so userDdcCount is 2 and not 4: DDC2 and DDC3 are
    // the PureSignal pair (console.cs:8757-8762 GetDDC).
    //
    // maxSlices exceeds userDdcCount deliberately. Slices sharing one DDC
    // window cost no wire bandwidth (BoardCapabilities.h:326-328), so flags
    // are capped by the Phase 3F project ceiling of 5 rather than by DDC
    // count, as on the ANAN-G2E row.
    //
    // These were both 1 until 2026-07-31, derived from design doc §2, whose
    // DDC-reservation cite is ramdor console.cs:8186-8538 [v2.10.3.15]. That
    // switch has no HERMESLITE case at all. See
    // docs/architecture/2026-07-31-hl2-slice-cap-design.md §2.
    .maxSlices        = 5,
    .userDdcCount     = 2,
```

Replace lines 907-908 (`kHermesLiteRxOnly`) with:

```cpp
    // Mirrors kHermesLite: same silicon, same DDC map, TX driver absent.
    // See that row and docs/architecture/2026-07-31-hl2-slice-cap-design.md §2.
    .maxSlices        = 5,
    .userDdcCount     = 2,
```

- [ ] **Step 4: Fix the design-doc table in the same commit**

In `docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md` §2, change the two HL2 rows of the "Resolved values per SKU" table:

```markdown
| HermesLite2 (HL2) | 1 | 4 | DDC0-1 | **5** | 48, 96, 192, 384 | false | 0 (defer, P1 mechanism) |
| HermesLite2 RX-only | 1 | 4 | DDC0-1 | **5** | 48, 96, 192, 384 | false | 0 |
```

Immediately after the "Source cites" list in that section, add:

```markdown
#### Note: the HL2 rows were derived from a source that does not cover the HL2

Corrected 2026-07-31. Both HL2 rows read `DDC0 only | 1` until then, sourced
from the "DDC reservations" cite above, ramdor Thetis `console.cs:8186-8538
[v2.10.3.15]`. That switch has no `HERMESLITE` case: seven case groups, no
`default:` arm, and `HERMESLITE` appears in ramdor only in `enums.cs:128,397`
and `clsHardwareSpecific.cs:353,393`. An HL2 leaves it with `nddc = 0`.

mi0bot is authoritative for this SKU and enables DDC1 for RX2 in two arms of
its `HERMESLITE` case (`console.cs:8425-8429` and `:8453-8457
[v2.10.3.13-beta2]`), so the row is `DDC0-1`. `maxSlices` moves to the project
ceiling of 5 because slices sharing a DDC window cost nothing.

Same failure mode as the ANAN-G2E row noted above: a value copied from a cite
that does not describe the SKU. Full analysis in
[2026-07-31-hl2-slice-cap-design.md](2026-07-31-hl2-slice-cap-design.md).
```

- [ ] **Step 5: Run test to verify it passes**

```bash
cmake --build build --target tst_board_capabilities_phase3f && ctest --test-dir build -R '^tst_board_capabilities_phase3f$' --output-on-failure
```

Expected: PASS, including the pre-existing `user_ddc_count_never_exceeds_native_codec_capacity`, which now sees capacity 2 against `userDdcCount` 2.

- [ ] **Step 6: Commit**

```bash
git add src/core/BoardCapabilities.cpp \
        docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md \
        tests/tst_board_capabilities_phase3f.cpp
git commit -S -m "fix(3f): give the HL2 two panadapters and five flags

Both HL2 rows capped userDdcCount and maxSlices at 1. mi0bot enables DDC1
for RX2 in two arms of its HERMESLITE case (console.cs:8425-8429 and
:8453-8457 [v2.10.3.13-beta2]), so the correct panadapter count is 2.

maxSlices goes to 5, the ceiling every other SKU already has. It is a
separate axis: slices sharing one DDC window cost no wire bandwidth, so
flags are not bounded by DDC count. The ANAN-G2E row already differs this
way.

Fixes the design doc section 2 table in the same commit, and records why it
was wrong. Its DDC-reservation cite is ramdor console.cs:8186-8538
[v2.10.3.15], a switch with no HERMESLITE case: seven case groups, no
default arm. An HL2 leaves it with nddc = 0. Same failure mode the ANAN-G2E
row carries a note for."
```

---

### Task 3: Make under-exposure a test failure, table-wide

`user_ddc_count_never_exceeds_native_codec_capacity` asserts `userDdcCount <= codecCapacity`. That is one-sided, and it is exactly why a row of 1 against a codec that could do 2 survived. Add the lower bound with a named-exceptions list.

**Files:**
- Modify: `tests/tst_board_capabilities_phase3f.cpp:322-345`, `:393-407`

**Interfaces:**
- Consumes: `assignableStreams<Codec>(const Codec&)` (line 41), `BoardCapsTable::forBoard(HPSDRHW)`.
- Produces: nothing consumed by later tasks.

Measured codec capacities on this branch, for the exceptions list below: `P1CodecStandard` assigns streams 0-3, capacity **4**. `P1CodecHl2` after Task 1, capacity **2**. `P2CodecOrionMkII` and `P2CodecSaturn`, capacity **5**. `P2CodecHermes`, capacity `familyDdcCount()`, which is **4** for HermesC10.

- [ ] **Step 1: Write the failing test**

Replace the `expectFits` helper at lines 393-407 with a two-sided version, and add the exceptions table just above it in the same `private:` section:

```cpp
private:
    // Boards whose userDdcCount is deliberately BELOW what their codec will
    // assign. Every entry needs a reason, because an unexplained gap is how
    // the HL2 shipped at 1 against a 2-stream codec from 2026-05-26 to
    // 2026-07-31.
    //
    // All three entries here are the same shape: the board has fewer hardware
    // DDCs than the shared P1CodecStandard, which implements only ramdor's
    // HERMES-class arm (nddc = 4, console.cs:8387-8459 [v2.10.3.15]).
    struct UnderExposed {
        HPSDRHW     hw;
        const char* reason;
    };

    static const std::vector<UnderExposed>& documentedUnderExposure()
    {
        static const std::vector<UnderExposed> kRows = {
            {HPSDRHW::Atlas,
             "Metis has 3 DDCs (design doc section 2); P1CodecStandard is "
             "shared with the 4-DDC Hermes class"},
            {HPSDRHW::HermesII,
             "ANAN-10E/100B have 2 DDCs (Thetis console.cs:8461-8464 "
             "[v2.10.3.15], nddc = 2); P1CodecStandard is shared with the "
             "4-DDC Hermes class"},
            {HPSDRHW::HermesLiteRxOnly,
             "HL2 has 2 user DDCs (mi0bot console.cs:8425-8429 "
             "[v2.10.3.13-beta2]); this SKU has no HPSDRModel of its own so "
             "P1 selectCodec lands on the 4-stream P1CodecStandard"},
        };
        return kRows;
    }

    static const char* underExposureReason(HPSDRHW hw)
    {
        for (const UnderExposed& row : documentedUnderExposure()) {
            if (row.hw == hw) { return row.reason; }
        }
        return nullptr;
    }

    static void expectFits(HPSDRHW hw, int codecCapacity)
    {
        const BoardCapabilities& caps = BoardCapsTable::forBoard(hw);
        const QString name = QString::fromLatin1(caps.displayName);

        QVERIFY2(caps.userDdcCount <= codecCapacity,
                 qPrintable(QStringLiteral("%1: userDdcCount %2 exceeds the %3 "
                                           "streams its codec will assign")
                                .arg(name)
                                .arg(caps.userDdcCount)
                                .arg(codecCapacity)));

        // A pool smaller than the codec can drive is not a defect, but a pool
        // of zero on a board that has DDCs means no slice can ever bind.
        QVERIFY2(caps.userDdcCount >= 1,
                 qPrintable(QStringLiteral("%1: userDdcCount must be at least 1")
                                .arg(name)));

        // The other side. Exposing fewer DDCs than the codec will assign
        // silently costs the operator a panadapter, and nothing else in the
        // build notices. Allowed only with a written reason.
        if (caps.userDdcCount < codecCapacity) {
            const char* why = underExposureReason(hw);
            QVERIFY2(why != nullptr,
                     qPrintable(QStringLiteral(
                         "%1: userDdcCount %2 is below the %3 streams its "
                         "codec will assign, with no entry in "
                         "documentedUnderExposure(). Either raise the row or "
                         "add an entry saying why the hardware cannot use "
                         "them.")
                             .arg(name)
                             .arg(caps.userDdcCount)
                             .arg(codecCapacity)));
            QVERIFY2(qstrlen(why) > 0,
                     qPrintable(QStringLiteral("%1: under-exposure reason is empty")
                                    .arg(name)));
        }
    }
```

Add `#include <vector>` to the include block.

Then add a case proving the guard actually bites, so a future refactor cannot quietly neuter it:

```cpp
    // The guard has to fail on an undocumented gap, not just pass on the
    // documented ones. HermesLite is deliberately absent from
    // documentedUnderExposure(): its row matches its codec exactly, so if
    // anyone lowers it again this is what fires.
    void under_exposure_needs_a_written_reason()
    {
        QVERIFY(underExposureReason(HPSDRHW::HermesLite) == nullptr);
        QCOMPARE(BoardCapsTable::forBoard(HPSDRHW::HermesLite).userDdcCount,
                 assignableStreams(P1CodecHl2{}));

        for (const UnderExposed& row : documentedUnderExposure()) {
            QVERIFY2(qstrlen(row.reason) > 0,
                     "every documentedUnderExposure entry needs a reason");
        }
    }
```

- [ ] **Step 2: Run test to verify the new case compiles and passes, and that the guard bites**

```bash
cmake --build build --target tst_board_capabilities_phase3f && ctest --test-dir build -R '^tst_board_capabilities_phase3f$' --output-on-failure
```

Expected: PASS.

Now prove the guard fires. Temporarily set `kHermesLite.userDdcCount` back to `1` in `src/core/BoardCapabilities.cpp`, rebuild and rerun.

Expected: FAIL, with `Hermes Lite 2: userDdcCount 1 is below the 2 streams its codec will assign, with no entry in documentedUnderExposure()`.

Revert the temporary edit and rerun. Expected: PASS.

- [ ] **Step 3: Commit**

```bash
git add tests/tst_board_capabilities_phase3f.cpp
git commit -S -m "test(3f): fail the build when a SKU under-exposes its DDCs

user_ddc_count_never_exceeds_native_codec_capacity only asserted
userDdcCount <= codec capacity. One-sided, and that is precisely how the HL2
sat at 1 against a 2-stream codec for two months: 1 is happily less than 2.

Adds the lower bound. A row below its codec's capacity now needs an entry in
documentedUnderExposure() with a written reason. Three exist today, all the
same shape: Atlas, HermesII and HermesLiteRxOnly have fewer hardware DDCs
than the shared P1CodecStandard, which implements only ramdor's 4-DDC
HERMES-class arm.

Adds a case asserting the guard bites rather than merely passing, since a
guard that cannot fail is not a guard."
```

---

### Task 4: Wire stream-1 liveness into `ReceiverManager`

`ReceiverManager::setRx2Enabled(bool)` has zero callers anywhere in `src/` or `tests/`. `m_rx2Enabled` is initialised false at `ReceiverManager.cpp:151` and never changes, so `applyPureSignalDdcConfig`'s rx2 branches, which are already correctly ported, can never fire.

**Files:**
- Modify: `src/models/RadioModel.cpp:14277` (`invokeCodecDdcAssignment`)
- Create: `tests/tst_p1_hl2_rx2_wiring.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `RadioModel::buildStreamConfigsForCodec()` (`RadioModel.h:2631`). `ReceiverManager::setRx2Enabled(bool)` (`ReceiverManager.h:233`), `ReceiverManager::setRx2Rate(int)` (`:229`), `ReceiverManager::setP1Codec(IP1Codec*)` (`:195`, **raw pointer, caller keeps ownership**), `setHpsdrModel(HPSDRModel)` (`:209`), signal `ddcConfigChanged(const NereusSDR::PsDdcConfig&)` (`:267`). `RadioModel::receiverManager()` (`RadioModel.h:235`), `addSlice()` (`:677`), `sliceById(int)` (`:455`), `configureStreamPool(int,int,int)` (`:501`).
- Produces: `ReceiverManager::rx2Enabled()` const getter, added in Step 3. After any DDC-assignment request, `m_rx2Enabled` equals `buildStreamConfigsForCodec()[1].live`.

**Type notes.** `PsDdcConfig` (`src/core/codec/CodecContext.h:542-549`) uses `uint8_t ddcEnable`, `uint8_t syncEnable`, `uint32_t rate[8]`, `int p1RxCount`, `int nDdc`. `QCOMPARE` is type-strict, so wrap the narrow fields: `QCOMPARE(int(cfg.ddcEnable), 3)`. `DdcAssignment` (`src/core/DdcAssignment.h`) uses plain `int` throughout including `std::array<int, 8> rate`, so no casts are needed there. `PsDdcConfig` already has `Q_DECLARE_METATYPE`, so `QSignalSpy` can carry it.

- [ ] **Step 1: Write the failing test**

Create `tests/tst_p1_hl2_rx2_wiring.cpp`:

```cpp
// no-port-check: NereusSDR-original wiring test, no ported logic.
#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/HpsdrModel.h"
#include "core/ReceiverManager.h"
#include "core/codec/CodecContext.h"
#include "core/codec/P1CodecHl2.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {
// ReceiverManager has no getter for the config it last computed, so read it
// off the signal it already emits.
PsDdcConfig lastConfig(QSignalSpy& spy)
{
    Q_ASSERT(spy.count() > 0);
    return spy.last().at(0).value<PsDdcConfig>();
}
} // namespace

class TestP1Hl2Rx2Wiring : public QObject {
    Q_OBJECT
private slots:
    // ReceiverManager::setRx2Enabled had no caller anywhere in src/ or
    // tests/, so m_rx2Enabled was permanently false and the rx2 arms of
    // applyPureSignalDdcConfig (P1CodecHl2.cpp:569-572 and :593-596, both
    // correctly ported all along) could never fire.
    void rx2_enabled_reaches_the_codec()
    {
        P1CodecHl2 codec;          // setP1Codec takes a raw pointer
        ReceiverManager rm;
        rm.setP1Codec(&codec);
        rm.setHpsdrModel(HPSDRModel::HERMESLITE);
        rm.setRx1Rate(192000);

        QSignalSpy spy(&rm, &ReceiverManager::ddcConfigChanged);

        rm.setRx2Rate(96000);
        rm.setRx2Enabled(true);

        QVERIFY(spy.count() > 0);
        const PsDdcConfig cfg = lastConfig(spy);
        QCOMPARE(int(cfg.ddcEnable), 1 + 2);      // DDC0 + DDC1
        QCOMPARE(int(cfg.rate[1]), 96000);
    }

    void rx2_disabled_drops_ddc1()
    {
        P1CodecHl2 codec;
        ReceiverManager rm;
        rm.setP1Codec(&codec);
        rm.setHpsdrModel(HPSDRModel::HERMESLITE);
        rm.setRx1Rate(192000);
        rm.setRx2Rate(96000);
        rm.setRx2Enabled(true);

        QSignalSpy spy(&rm, &ReceiverManager::ddcConfigChanged);
        rm.setRx2Enabled(false);

        QVERIFY(spy.count() > 0);
        const PsDdcConfig cfg = lastConfig(spy);
        QCOMPARE(int(cfg.ddcEnable), 1);          // DDC0 only
        QCOMPARE(int(cfg.rate[1]), 0);
    }
};

QTEST_MAIN(TestP1Hl2Rx2Wiring)
#include "tst_p1_hl2_rx2_wiring.moc"
```

Register the test in `tests/CMakeLists.txt` next to the other P1 entries, in alphabetical position near line 637:

```cmake
nereus_add_test(tst_p1_hl2_rx2_wiring)
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build build --target tst_p1_hl2_rx2_wiring && ctest --test-dir build -R '^tst_p1_hl2_rx2_wiring$' --output-on-failure
```

Expected: PASS for these two, because they call `setRx2Enabled` directly. They pin the codec contract. The real gap is the missing production caller, covered next.

- [ ] **Step 3: Add the production caller**

In `src/models/RadioModel.cpp`, in `invokeCodecDdcAssignment()` (line 14277), insert immediately after `const NereusSDR::DdcAssignment assignment = computeDdcAssignment();`:

```cpp
    // Phase 3F: publish stream-1 liveness to ReceiverManager.
    //
    // ReceiverManager::setRx2Enabled had no caller, so m_rx2Enabled was
    // permanently false and the rx2 arms of the P1 codecs'
    // applyPureSignalDdcConfig could never fire, whatever the capability row
    // said. buildStreamConfigsForCodec() is the single source of stream
    // liveness and is what computeDdcAssignment() just consumed, so reading
    // it again here cannot disagree with the assignment above.
    if (m_receiverManager) {
        const std::array<NereusSDR::SliceConfig, 5> streams =
            buildStreamConfigsForCodec();
        m_receiverManager->setRx2Rate(streams[1].live ? streams[1].sampleRateHz
                                                      : 0);
        m_receiverManager->setRx2Enabled(streams[1].live);
    }
```

Order matters: set the rate before the enable, so the config recomputed by `setRx2Enabled` already carries the right rate rather than recomputing twice with a stale one.

- [ ] **Step 4: Add the end-to-end case**

`ReceiverManager` has no getter for `m_rx2Enabled`. Add one next to the other trivial accessors in `src/core/ReceiverManager.h`:

```cpp
    /// Phase 3F: whether stream 1 is live. Published by
    /// RadioModel::invokeCodecDdcAssignment from buildStreamConfigsForCodec().
    bool rx2Enabled() const { return m_rx2Enabled; }
```

No test-only hook is needed on `RadioModel`. `requestDdcAssignment` is wired to every slice's `frequencyChanged` (see the comment at `RadioModel.cpp:14293`), so tuning a slice drives the real production path. This is the same route `tst_alex_per_adc_bpf_wire` uses, as noted at `RadioModel.h:1495-1497`.

Append to `tests/tst_p1_hl2_rx2_wiring.cpp`, inside the `private slots:` block:

```cpp
    // The production path. A second live stream must reach ReceiverManager
    // without anyone calling setRx2Enabled by hand, which is the wiring that
    // was missing. Driven through slice frequency changes because
    // requestDdcAssignment hangs off SliceModel::frequencyChanged.
    void second_live_stream_enables_rx2_end_to_end()
    {
        P1CodecHl2 codec;
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        model.receiverManager()->setP1Codec(&codec);
        model.receiverManager()->setHpsdrModel(HPSDRModel::HERMESLITE);
        model.configureStreamPool(/*userDdcCount=*/2, /*maxSlices=*/5, 192000);

        const int idA = model.addSlice();
        SliceModel* a = model.sliceById(idA);
        QVERIFY(a != nullptr);
        a->setFrequency(14200000);

        // Only stream 0 so far.
        QVERIFY(!model.receiverManager()->rx2Enabled());

        // 7.1 MHz is far outside slice A's 192 kHz window, so the allocator
        // must claim stream 1 rather than co-host on stream 0.
        const int idB = model.addSlice();
        SliceModel* b = model.sliceById(idB);
        QVERIFY(b != nullptr);
        b->setFrequency(7100000);

        QVERIFY(model.buildStreamConfigsForCodecForTest()[1].live);
        QVERIFY(model.receiverManager()->rx2Enabled());
    }
```

If `addSlice()` needs a pan id on this branch, pass one: `model.addSlice(QStringLiteral("pan0"))`. Check the signature at `RadioModel.h:677` and the `addSliceOnPan` neighbours before assuming the defaulted form works standalone.

- [ ] **Step 5: Run tests to verify they pass**

```bash
cmake --build build --target tst_p1_hl2_rx2_wiring && ctest --test-dir build -R '^tst_p1_hl2_rx2_wiring$' --output-on-failure
```

Expected: PASS, all three cases.

- [ ] **Step 6: Run the core subsystem once**

```bash
cmake --build build --target tests_core && ctest --test-dir build -L core
```

Expected: all green. `RadioModel` and `ReceiverManager` are widely depended on, so this is the checkpoint for tasks 1 to 4. Investigate any failure; do not carry it forward.

- [ ] **Step 7: Commit**

```bash
git add src/models/RadioModel.cpp src/core/ReceiverManager.h \
        tests/tst_p1_hl2_rx2_wiring.cpp tests/CMakeLists.txt
git commit -S -m "fix(3f): give ReceiverManager::setRx2Enabled a caller

setRx2Enabled had zero callers anywhere in src/ or tests/. m_rx2Enabled was
initialised false at ReceiverManager.cpp:151 and never changed, so the rx2
arms of applyPureSignalDdcConfig, which were ported correctly all along,
could never fire no matter what the capability row said.

invokeCodecDdcAssignment now publishes stream-1 liveness and rate from
buildStreamConfigsForCodec(), the same source computeDdcAssignment() just
consumed, so the two cannot disagree.

Adds tst_p1_hl2_rx2_wiring covering the codec contract in both directions
and the production path end to end."
```

---

### Task 5: Route receiver-count changes through the stream restart

`applyPsDdcConfig` assigns `m_activeRxCount` directly and only forces a bank-0 flush. The count sets the ep6 slot layout for both sides (`slotBytes = 6 * numRx + 2`), and nothing in a frame identifies which layout produced it: the `0x7F 0x7F 0x7F` sync check is layout-independent, so a mismatch is misparsed into corrupted audio rather than rejected. `restartStreamWithCount` already exists to do this correctly.

This is a pre-existing latent defect. Today's HL2 connect already steps the count from the 2 seeded at `P1RadioConnection.cpp:695` to the 4 written here, by bare assignment. It goes unnoticed because it happens once at startup. Task 6 would make it recur on every PureSignal toggle, so it is fixed first.

**Files:**
- Modify: `src/core/P1RadioConnection.cpp:1826-1829`
- Test: `tests/tst_p1_hl2_rx2_wiring.cpp`

**Interfaces:**
- Consumes: `P1RadioConnection::restartStreamWithCount(int)` (`P1RadioConnection.cpp:1002-1017`). Idempotent, and a no-op recording the value when `!m_running`.
- Produces: any change to the announced receiver count performs stop, prime, start, prime. Task 6 depends on this.

- [ ] **Step 1: Give the fake radio a metis-stop counter**

`tests/fakes/P1FakeRadio.h` exposes `ep2FramesReceived()` and `isRunning()` but no stop count, and `isRunning()` alone cannot see a stop followed immediately by a start within one event-loop turn. Add a counter next to `m_ep2Count`.

In `tests/fakes/P1FakeRadio.h`, add the accessor beside `ep2FramesReceived()` at line 52:

```cpp
    int  metisStopCount()    const { return m_stopCount; }
```

and the member beside `m_ep2Count` at line 78:

```cpp
    int          m_stopCount{0};
```

In `tests/fakes/P1FakeRadio.cpp`, inside `handleMetisCommand`, increment `m_stopCount` on the branch that clears `m_running` (the metis command with the start/stop byte set to 0). Read the existing branch and add the single `++m_stopCount;` line there rather than restructuring it.

- [ ] **Step 2: Write the failing test**

Add to `tests/tst_p1_hl2_rx2_wiring.cpp`. Add these includes at the top: `#include "core/P1RadioConnection.h"`, `#include "core/RadioDiscovery.h"`, `#include "fakes/P1FakeRadio.h"`, and `using NereusSDR::Test::P1FakeRadio;` beside the existing `using namespace NereusSDR;`.

Add this private helper to the class, mirroring `tst_p1_loopback_connection.cpp:46-56`:

```cpp
private:
    RadioInfo makeInfo(P1FakeRadio& fake) const {
        RadioInfo info;
        info.address         = fake.localAddress();
        info.port            = fake.localPort();
        info.boardType       = HPSDRHW::HermesLite;
        info.protocol        = ProtocolVersion::Protocol1;
        info.macAddress      = QStringLiteral("aa:bb:cc:11:22:33");
        info.firmwareVersion = 72;
        info.name            = QStringLiteral("FakeHL2");
        return info;
    }
```

Then add to `private slots:`:

```cpp
    // Changing the announced receiver count changes the ep6 slot layout on
    // both sides at once (parseEp6Frame's slotBytes = 6 * numRx + 2). A bare
    // assignment leaves frames in flight that were composed under the old
    // layout, and parseEp6Frame cannot detect the mismatch: its 7F 7F 7F sync
    // check is layout-independent, so those frames are misparsed into
    // corrupted audio rather than rejected. restartStreamWithCount does the
    // stop, prime, start, prime cycle that makes both sides agree.
    void ps_ddc_config_count_change_restarts_the_stream()
    {
        P1FakeRadio fake;
        fake.setAutoStreamEnabled(false);   // no ep6 noise; we only count stops
        fake.start();

        P1RadioConnection conn;
        conn.init();
        conn.connectToRadio(makeInfo(fake));
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        const int stopsBefore = fake.metisStopCount();

        PsDdcConfig cfg{};
        cfg.p1RxCount = 4;      // connect seeds 2 (P1RadioConnection.cpp:695)
        cfg.nDdc      = 4;
        conn.applyPsDdcConfig(cfg);

        QTRY_VERIFY_WITH_TIMEOUT(fake.metisStopCount() > stopsBefore, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);
    }

    // The same count must not restart. restartStreamWithCount is idempotent
    // and a spurious stop/start costs the operator an audio dropout.
    void ps_ddc_config_same_count_does_not_restart()
    {
        P1FakeRadio fake;
        fake.setAutoStreamEnabled(false);
        fake.start();

        P1RadioConnection conn;
        conn.init();
        conn.connectToRadio(makeInfo(fake));
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        PsDdcConfig first{};
        first.p1RxCount = 4;
        first.nDdc      = 4;
        conn.applyPsDdcConfig(first);
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        const int stopsBefore = fake.metisStopCount();
        conn.applyPsDdcConfig(first);     // identical config, second time

        QTest::qWait(200);
        QCOMPARE(fake.metisStopCount(), stopsBefore);
    }
```

If `applyPsDdcConfig` is not public on `P1RadioConnection`, check `src/core/P1RadioConnection.h:268` for its declared visibility and reach it the way `ReceiverManager::ddcConfigChanged` does in production, by connecting the signal, rather than widening production visibility for a test.

- [ ] **Step 3: Run test to verify it fails**

```bash
cmake --build build --target tst_p1_hl2_rx2_wiring && ctest --test-dir build -R '^tst_p1_hl2_rx2_wiring$' --output-on-failure
```

Expected: FAIL on `ps_ddc_config_count_change_restarts_the_stream`, with the stop count unchanged, because `applyPsDdcConfig` assigns directly.

- [ ] **Step 4: Route through `restartStreamWithCount`**

In `src/core/P1RadioConnection.cpp`, replace lines 1826-1829 with:

```cpp
    if (cfg.p1RxCount > 0 && m_activeRxCount != cfg.p1RxCount) {
        // restartStreamWithCount rather than a bare assignment. The count is
        // the ep6 slot layout (parseEp6Frame's slotBytes = 6 * numRx + 2),
        // and both sides have to change together: a frame composed under the
        // old layout and parsed under the new one is silently misparsed,
        // because the 7F 7F 7F sync check does not encode the layout. The
        // stop, prime, start, prime cycle is the existing mechanism for
        // exactly this (restartStreamWithCount, this file). Idempotent, and
        // a plain record of the value when the stream is not yet running.
        restartStreamWithCount(cfg.p1RxCount);
        changed = true;
    }
```

- [ ] **Step 5: Run tests to verify they pass**

```bash
cmake --build build --target tst_p1_hl2_rx2_wiring && ctest --test-dir build -R '^tst_p1_hl2_rx2_wiring$' --output-on-failure
```

Expected: PASS.

- [ ] **Step 6: Check for collateral in the P1 suite**

```bash
cmake --build build --target tst_p1_loopback_connection && ctest --test-dir build -R '^tst_p1_(loopback_connection|codec_hl2|adc_overflow_extraction|mic_extraction)$' --output-on-failure
```

Expected: PASS. These exercise the connect path where the count first moves from 2 to 4, which now restarts the stream where previously it did not.

- [ ] **Step 7: Commit**

```bash
git add src/core/P1RadioConnection.cpp tests/tst_p1_hl2_rx2_wiring.cpp \
        tests/fakes/P1FakeRadio.h tests/fakes/P1FakeRadio.cpp
git commit -S -m "fix(p1): restart the ep6 stream when the receiver count changes

applyPsDdcConfig assigned m_activeRxCount directly and forced a bank-0
flush. The count is the ep6 slot layout on both sides (parseEp6Frame's
slotBytes = 6 * numRx + 2), so frames already in flight are parsed under the
wrong layout, and nothing detects it: the 7F 7F 7F sync check is
layout-independent, so the result is corrupted audio rather than a rejected
frame.

restartStreamWithCount already does the stop, prime, start, prime cycle for
this and is idempotent. Route through it.

Latent today rather than new: every HL2 connect already steps the count from
the 2 seeded at P1RadioConnection.cpp:695 to the 4 applyPsDdcConfig writes,
with the same window, unnoticed because it happens once at startup."
```

---

### Task 6: Announce two DDCs when PureSignal is off (approved deviation)

> **This is a deliberate deviation from mi0bot, approved by the maintainer on 2026-07-31 conditional on bench rows 8 to 13 of the design doc.** mi0bot sets `P1_rxcount = 4; nddc = 4;` at `console.cs:8412-8413 [v2.10.3.13-beta2]` unconditionally, in every state and at every rate. There is no upstream basis for this change; it is justified by the link budget in design doc §4. If the bench fails, revert this commit alone. Tasks 1 to 5 do not depend on it.

**Files:**
- Modify: `src/core/codec/P1CodecHl2.cpp:556-557` and the plain-RX branches of `applyPureSignalDdcConfig`
- Test: `tests/tst_p1_codec_hl2.cpp`

**Interfaces:**
- Consumes: `restartStreamWithCount` routing from Task 5. Without it this change introduces a misparse window on every PureSignal toggle.
- Produces: `applyPureSignalDdcConfig(...).p1RxCount` is 2 when `psEnabled` is false, 4 when true. `cfg.nDdc` stays 4 in all cases.

Key on `psEnabled`, the operator's PureSignal master toggle, never on `moxState`. Keying on MOX would flip the announced count on every key-down (design doc §4.4).

- [ ] **Step 1: Write the failing tests**

Add to `tests/tst_p1_codec_hl2.cpp`, inside `private slots:`:

```cpp
    // ── Approved deviation from mi0bot, 2026-07-31 ──────────────────────
    // mi0bot announces nddc = 4 unconditionally for the HL2
    // (console.cs:8412-8413 [v2.10.3.13-beta2]). NereusSDR announces 2 when
    // PureSignal is off, halving the ep6 datagram rate: about 23 Mbit/s
    // rather than 44 at 192 kHz. PureSignal needs four DDCs (DDC0+DDC1 as
    // the sync pair, DDC2 feedback, DDC3 TX monitor, console.cs:8757-8762
    // GetDDC), so the count stays 4 whenever PS is enabled.
    // See docs/architecture/2026-07-31-hl2-slice-cap-design.md section 6.2.
    void announced_count_is_two_with_puresignal_off() {
        P1CodecHl2 codec;
        const PsDdcConfig cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::HERMESLITE,
            /*psEnabled=*/false, /*diversityEnabled=*/false,
            /*moxState=*/false, /*rx1Rate=*/192000, /*rx2Rate=*/0,
            /*rx2Enabled=*/false, /*adcCtrl1=*/0, /*adcCtrl2=*/0);
        QCOMPARE(cfg.p1RxCount, 2);
        QCOMPARE(cfg.nDdc, 4);          // PS freq-override gate, unchanged
    }

    void announced_count_is_four_with_puresignal_on() {
        P1CodecHl2 codec;
        const PsDdcConfig cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::HERMESLITE,
            /*psEnabled=*/true, /*diversityEnabled=*/false,
            /*moxState=*/false, /*rx1Rate=*/192000, /*rx2Rate=*/0,
            /*rx2Enabled=*/false, /*adcCtrl1=*/0, /*adcCtrl2=*/0);
        QCOMPARE(cfg.p1RxCount, 4);
    }

    // The central claim of the policy: MOX must not move the count. If this
    // ever regresses to keying on the run state, the ep6 slot layout would
    // change on every key-down.
    void announced_count_does_not_move_on_mox() {
        P1CodecHl2 codec;
        for (bool ps : {false, true}) {
            const PsDdcConfig rx = codec.applyPureSignalDdcConfig(
                HPSDRModel::HERMESLITE, ps, false, /*moxState=*/false,
                192000, 192000, /*rx2Enabled=*/true, 0, 0);
            const PsDdcConfig tx = codec.applyPureSignalDdcConfig(
                HPSDRModel::HERMESLITE, ps, false, /*moxState=*/true,
                192000, 192000, /*rx2Enabled=*/true, 0, 0);
            QCOMPARE(rx.p1RxCount, tx.p1RxCount);
        }
    }

    // The wire byte the radio actually sees. C4 bits 3-5 carry nddc - 1.
    // Source: mi0bot networkproto1.c:968 [v2.10.3.13-beta2]
    //   C4 |= (nddc - 1) << 3;   // number of DDCs to run
    void bank0_c4_encodes_the_announced_count() {
        quint8 out[5] = {};
        P1RadioConnection::composeCcBank0(out, 192000, /*mox=*/false,
                                          /*activeRxCount=*/2);
        QCOMPARE(int(out[4]), (2 - 1) << 3);   // 0x08

        P1RadioConnection::composeCcBank0(out, 192000, /*mox=*/false,
                                          /*activeRxCount=*/4);
        QCOMPARE(int(out[4]), (4 - 1) << 3);   // 0x18
    }

    // The deviation is HL2-only. P1CodecStandard serves ramdor's HERMES-class
    // arm, a different SKU family with no approval attached to it, and it must
    // keep announcing 4 unconditionally (console.cs:8391-8392 [v2.10.3.15]).
    void deviation_does_not_leak_into_the_hermes_class_codec() {
        P1CodecStandard codec;
        for (bool ps : {false, true}) {
            const PsDdcConfig cfg = codec.applyPureSignalDdcConfig(
                HPSDRModel::HERMES, ps, /*diversityEnabled=*/false,
                /*moxState=*/false, 192000, 0, /*rx2Enabled=*/false, 0, 0);
            QCOMPARE(cfg.p1RxCount, 4);
        }
    }
```

Add `#include "core/P1RadioConnection.h"`, `#include "core/HpsdrModel.h"` and `#include "core/codec/P1CodecStandard.h"` to the test's include block.

- [ ] **Step 2: Run tests to verify they fail**

```bash
cmake --build build --target tst_p1_codec_hl2 && ctest --test-dir build -R '^tst_p1_codec_hl2$' --output-on-failure
```

Expected: FAIL on `announced_count_is_two_with_puresignal_off`, reporting `p1RxCount` is 4, not 2. `bank0_c4_encodes_the_announced_count` should already PASS; it pins existing behaviour.

- [ ] **Step 3: Implement the deviation**

In `src/core/codec/P1CodecHl2.cpp`, replace lines 551-557 with:

```cpp
    // From mi0bot console.cs:8409-8413 [v2.10.3.13-beta2]
    //   case HPSDRModel.HERMESLITE: // MI0BOT: HL2
    //
    // Inline tag preserved per CLAUDE.md "Inline comment preservation":
    //MI0BOT  [HL2 case-statement marker at console.cs:8409]
    cfg.nDdc = 4;

    // ── APPROVED DEVIATION FROM mi0bot, 2026-07-31 ─────────────────────────
    //
    // mi0bot sets P1_rxcount = 4 unconditionally here
    // (console.cs:8412-8413 [v2.10.3.13-beta2]), in every MOX, diversity and
    // PureSignal state and at every sample rate. This is NOT a port. It is a
    // NereusSDR-original divergence justified by the link budget, approved by
    // the maintainer on 2026-07-31 conditional on bench verification.
    //
    // p1RxCount becomes the wire C4 field, nddc - 1 << 3
    // (P1RadioConnection::composeCcBank0), and the ep6 slot layout,
    // slotBytes = 6 * numRx + 2. Because ep6 datagrams are fixed at 1032
    // bytes, sample capacity falls as the count rises
    // (networkproto1.c:527: spr = 504 / (6 * nddc + 2)), so the datagram rate
    // scales with the count whether or not the extra DDCs are consumed.
    // Announcing 4 for a two-panadapter board costs about 44 Mbit/s at
    // 192 kHz and 89 at 384, against 23 and 47 for 2.
    //
    // PureSignal genuinely needs four: DDC0 and DDC1 as the sync pair, DDC2
    // feedback, DDC3 TX monitor (mi0bot console.cs:8757-8762
    // [v2.10.3.13-beta2] GetDDC: rx1 = 0; rx2 = 1; psrx = 2; pstx = 3).
    //
    // Keyed on psEnabled, the operator's master toggle, NOT on moxState or
    // the PS run state. Keying on either would flip the count on every
    // key-down, changing the slot layout mid-stream. The floor is 2 rather
    // than 1 because 2 is already sent on every P1 connect
    // (P1RadioConnection.cpp:695) and 1 never has been.
    //
    // nDdc stays 4 in all cases: it feeds only the bank-2/3 frequency
    // override gate (m_psNDdc), not the wire count.
    //
    // Full rationale and the bench gate that conditions this approval:
    // docs/architecture/2026-07-31-hl2-slice-cap-design.md section 6.2.
    cfg.p1RxCount = psEnabled ? 4 : 2;
```

Leave every branch below unchanged, including both `if (rx2Enabled)` blocks and the PS-MOX arm.

- [ ] **Step 4: Run tests to verify they pass**

```bash
cmake --build build --target tst_p1_codec_hl2 && ctest --test-dir build -R '^tst_p1_codec_hl2$' --output-on-failure
```

Expected: PASS, all cases.

- [ ] **Step 5: Record the deviation in the compliance inventory**

Check whether `docs/attribution/` requires an entry for a deliberate behavioural divergence from an upstream. Read `docs/attribution/HOW-TO-PORT.md` for the divergence-recording convention, and grep for how the Phase 3M-3a-iv anti-VOX divergence was recorded, which is the closest precedent:

```bash
grep -rn "divergence" docs/attribution/ docs/architecture/phase3m-3a-iv-antivox-feed-design.md | head -20
```

Follow whatever that precedent establishes. If it requires a row somewhere, add it in this commit.

- [ ] **Step 6: Run the full suite once**

```bash
cmake --build build --target all_tests && ctest --test-dir build --output-on-failure
```

Expected: all green. This is the single full-suite run for the whole plan; it costs about 32 minutes cold. Diagnose and fix any failure, including ones that look unrelated.

- [ ] **Step 7: Commit**

```bash
git add src/core/codec/P1CodecHl2.cpp tests/tst_p1_codec_hl2.cpp
git commit -S -m "feat(hl2): announce two DDCs when PureSignal is off

Approved deviation from mi0bot, maintainer sign-off 2026-07-31, conditional
on bench rows 8 to 13 of the design doc.

mi0bot announces nddc = 4 for the HL2 unconditionally
(console.cs:8412-8413 [v2.10.3.13-beta2]). ep6 datagrams are fixed at 1032
bytes and sample capacity falls as the count rises
(networkproto1.c:527, spr = 504 / (6 * nddc + 2)), so the datagram rate
scales with the announced count whether or not the extra DDCs are consumed.
A two-panadapter board announcing 4 burns about 44 Mbit/s at 192 kHz and 89
at 384, on a 100 Mbit PHY, and worse over a routed tunnel.

Announces 2 with PureSignal off, 4 with it on. PureSignal genuinely needs
four: DDC0+DDC1 as the sync pair, DDC2 feedback, DDC3 TX monitor
(console.cs:8757-8762 GetDDC).

Keyed on the PureSignal master toggle, never on MOX or the run state, so the
count changes only when the operator switches PureSignal rather than on
every key-down. Floor is 2 rather than 1 because 2 is already sent on every
P1 connect and 1 never has been.

This is not a port. Revert this commit alone if the bench fails; the
two-panadapter change does not depend on it."
```

---

## After the plan

Do **not** merge, and do **not** open a PR or post anything to GitHub. Report back with:

1. Test results for each task.
2. Any place the code did not match this plan's assumptions, especially the `ReceiverManager` and `RadioModel` accessor names in Task 4 and the loopback fixture in Task 5.
3. A build the maintainer can run against the live HL2, so bench rows 1 to 13 in design doc §9 can be worked through.

The design doc's bench matrix is the acceptance gate. Rows 8 to 13 specifically gate Task 6; if they fail, revert that commit and ship tasks 1 to 5.
