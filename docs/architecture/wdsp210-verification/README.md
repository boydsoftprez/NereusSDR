# WDSP 2.10 / NNR / PureSignal 3 verification

Implementation is authorized under the approved design and explicit persistence
requirements, using `yonder-cost-aware-execution`. Software, platform and RF
acceptance are separate below.

## Source and coordination

- Feature branch: `codex/wdsp210-nnr-ps3-design`; signed design, plan and baseline
  commits `73efb273`, `a6b81c99`, `e8a65674`.
- Shared signed Core/GUI base: `55e7d49f`. The existing **Resume Nereus core split**
  task owns `codex/integrate-r2-main`, serial combined integration and publication.
  Its independent wideband changes are not a mandatory rebase for this feature.
- Existing [PR #315](https://github.com/boydsoftprez/NereusSDR/pull/315) remains
  on that task's public-push/PR-mutation hold. This task prepares signed local
  source; it does not publish, merge publicly, deploy or operate hardware.
- TAPR source: `b02d5bac675dd2f33ec2bab2b339f79a597c47dd`, `wdsp 2.10/Source`.
  Final native tree: 167 C/headers, 156 pinned names plus 11 local files;
  126 pinned files byte-identical and 30 with documented downstream changes.
- Latest recovery stash `1384116365373a0238d5f71865bafa428c0d4983` preserves the
  pre-rebase production checkpoint. All 171 tracked changes, 66 new files and
  three deletions were reconciled with no missing files. Older recovery stashes
  remain retained. Source changes after this checkpoint are intentional fixes.

## Implemented boundaries

| Boundary | Implementation and software evidence |
| --- | --- |
| Vendor and ABI | Explicit 83-C-unit build; old PS2 calls removed; eight PS3 lifecycle/status helpers exported; original notices retained |
| CFC compatibility | Seven-argument Qg/Qe API retained beside native split setters; frozen 65-pair pre-import fixture unchanged |
| NNR | Nine normal tuning fields, distinct NR identity, mutual exclusion, real model/readback and sample-rate gates |
| NNR persistence | Per-radio/stable-slice storage, inactive/deleted receiver flush, migration, accepted reset, atomic write-failure retry |
| Model assets | Station-owned bounded imports, hashes and validation; pending/applied override state; slice-preserving model rebuild |
| PS3 preferences | Nine normal properties, non-actuating hydration, saved automatic intent, explicit calibration pause |
| PS3 lifecycle | Native quiescent and active stop, retained correction Apply, exact file generations, pending restore cancellation, teardown |
| Session | Protocol minor 5/capability gates, accepted-value property results, stale-action retirement and receive-only refusal |
| Display | Four 4096-value sample arrays and four 512-value curve arrays; source-backed transforms, visibility/session subscriptions |
| GUI | NNR right-click/Setup, model/correction manager, shared PS dialog/applets/status, AmpView, persistent local display preferences |

The legacy PS-RX/PS-TX spectrum checkbox had no working route and is explicitly
unavailable. Its preference is retained. Two-tone measurement display uses the
existing spectrum overlay. See [operator notes](../wdsp210-operator-notes.md)
and [control coverage](../wdsp210-control-coverage.md).

## Verification environment and results

Ninja RelWithDebInfo, AppleClang 21, Qt 6.11, CMake 4.3.1, macOS 27 arm64,
FFTW 3.3.11; tests enabled with DFNR/MNR. TestSandboxInit isolates storage.
No real radio was connected or operated by this task.

| Check | Result / artifact |
| --- | --- |
| Pre-import NR/PS/CFC baseline | 11 targets passed, 27.25 s; CFC fixture frozen before import |
| Final desktop/Core/daemon/all-tests build | Passed; 716 test executables, `/tmp/nereus-wdsp210-integrated-final-build.log` |
| First combined full suite | 708/716 passed, 334.84 s; all eight failures investigated and corrected |
| Correction pass | 8/9 selected targets passed, 64.13 s; remaining parser-incompatible FIFO fixture replaced by actual native ramp cancellation |
| Final native PS lifecycle | Passed, 32.58 s; `/tmp/nereus-wdsp210-lifecycle-ctest-5.log` |
| Reconnect/model apply | Passed with distinct A/B model and native tuning while B stays active; included in correction pass |
| CFC and native filter resize | Passed with unchanged 65-pair fixture plus RX/TX resize/process/re-resize/teardown; correction pass |
| Session/schema/assets | Accepted-value results, radio identity, remote import/export with R4 restore refusal, schema policy inventory passed |
| Native UI 1x | Four selected targets passed, 3.82 s; captures inspected in `/tmp/nereus-wdsp210-ui-final-1x` |
| Native UI 1.5x | NNR/asset/AmpView checks passed; final PS geometry/OnTop check passed in `/tmp/nereus-wdsp210-ui-final-1_5x-psform-3.log`; actual captures inspected |
| Full final suite | **716/716 passed**, 267.01 s; `/tmp/nereus-wdsp210-integrated-final-ctest.log` |
| Final persistence additions | Two affected targets passed, 1.76 s; actual PS3 settings-file reload and accepted remote two-slice/PS3 reload into fresh Core models, stale GUI hydration and no write/action replay; `/tmp/nereus-wdsp210-persistence-final-ctest.log` |
| Native AddressSanitizer | Passed, exit 0, no address-sanitizer finding; 83 WDSP C units instrumented. RX/TX resize/process, valid PS3 restore, active/quiescent stop, retained Apply, post-parse cancellation, bounded display and teardown. [Reproducer and scope](native-asan/README.md); `/tmp/nereus-wdsp210-native-asan.log` |
| Attribution and ownership | Final staged hook checks passed (`/tmp/nereus-wdsp210-final-attribution.log`): Thetis 362/362, FreeDV 13/13, Aether/WDSP headers, port registry, inline tags, compliance and GUI/DSP gates |
| WDSP census/exports | 162 GPL-2.0-or-later files plus five utility exceptions; all 287 parsed curated/compatibility declarations, including eight PS3 helpers, present in the linked native archive; zero missing symbols |

The first full run exposed two pinned filter-resize use-after-free sites:
`SetTXAFMEmphNC` in `emph.c` and `SetRXAFMNCde` in `fmd.c`. Both discarded
`build_fcimp`'s replacement after freeing the old object. Both now retain the
returned pointer. The exact source/patch disposition is in
[vendor inventory](vendor-inventory.md). After the full suite passed, two test-only persistence additions were built
and passed; production source did not change. Other corrected failures were fixture
setup/teardown, new protocol/schema expectations and relative fixture paths
under compiler caching; assertions were retained against the real boundaries.

The ASan probe instruments WDSP and its harness, not the linked rnnoise,
libspecbleach or Homebrew FFTW dependencies. Apple ASan does not support leak
detection. The probe uses native channel creation without generating FFTW
wisdom; a setup-only wisdom run was stopped before the successful bounded
lifecycle run. No maximum-sample calibration or RF-quality claim follows from
the restored display fixture (0 samples, 512 correction points).

One consolidated independent requirements/risk review completed. Its four
findings—transient starts surviving Off, stop requiring future feedback,
Apply Current failing to restart IQC, and remote asset management hidden by
R4—are corrected and covered by native/session/UI regressions. The lead verified
the corrections; there is no second routine independent review loop.

## Transport and persistence contracts

The existing direct-display limit remains 64 KiB per message. A PS3 snapshot
uses at most three bounded chunks and 160 KiB aggregate, latest-only assembly,
and at most 10 Hz while visible. R35 accounts for actual transmitted bytes.
Maximum native geometry is 147456 bytes before framing.

Core owns accepted DSP settings and assets. Normal settings survive restart;
one-shot commands, MOX, counters, live corrections and diagnostic bypass never
replay from configuration. Atomic save failures remain visible and retryable.
A saved model choice is distinct from applying its pending receiver rebuild.

## Remaining acceptance and handoff

- Full-suite, focused persistence and native address-sanitizer results are
  recorded above. The signed source handoff goes to the Core/GUI owner, who
  preserves independent CMake additions and recovery semantics when combining it.
- Linux/Windows and Linux ARM64 platform runs remain pending; macOS results do
  not establish those platforms. Public CI is not triggered during the hold.
- Full desktop/headless-Core process-relaunch workflows, NNR listening, Rock
  5C Standard/Premium/multi-slice load and audio continuity, and actual radio
  reconnect require the arranged operator session. Settings/model file-reload
  fixtures establish the software boundary, not completion of that operator matrix.
- Local PS3 feedback level, convergence, attenuation behavior and distortion
  measurements require an explicitly authorized bench. Remote actuation and RF
  acceptance belong to R4. No RF improvement or hardware acceptance is claimed.
- The colleague's separate recovery trial lost Rock reachability after a brief
  session; its cause is unassigned and it is not WDSP acceptance evidence.

Verbatim upstream whitespace is intentionally preserved, including a source
notice in `Ps3DisplayAdapter.h` and the native writer fixture's final blank line.
Authored changes pass whitespace checks; imported notices/data are not rewritten
merely to normalize whitespace.
