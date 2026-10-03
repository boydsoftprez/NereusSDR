# R3 NR3 model crash hotfix implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. One task. Runs in the
> hotfix worktree `/Users/j.j.boyd/.codex/worktrees/nereus-hotfix/NereusSDR` (branch
> `codex/hotfix-nr3`, build directory `build-hotfix`), based on the integration head
> `7f4b69f1`; the controller carries it into the integration branch afterwards.

**Goal:** the Core starts on Linux again. The Core built at `9e8ae810` crashed on the
Rock 5C about 15 ms after start because rnnoise's `rnnoise_model_from_buffer` leaves
the model's `file` field unset and `rnnoise_model_free` then closes a stale pointer;
the NR3 model check added for Core-owned NR3 models is the first caller of that
function, and glibc (unlike macOS) does not clear freed memory.

**Architecture:** fix the dependency where the bug is: compile a copy of rnnoise's
`denoise.c` that also clears `model->file` in `rnnoise_model_from_buffer`, guarded so
configure stops if a new rnnoise pin changes the anchor text. Add a test that fails
without the fix on Linux regardless of what the process freed earlier. Fix the
separate GUI build break seen with `NEREUS_GPU_SPECTRUM=OFF`.

**Tech stack:** CMake, C/C++20, Qt Test (off-screen), the arm64 Debian 13 Docker
container used for the Rock build.

**Spec:** the crash investigation of 2026-09-23 (backtrace, root cause, proven patches
and before/after test output): `/Users/j.j.boyd/.config/nereus/work/rock-docker/debug-9e8ae810/`
(`backtrace-relwithdebinfo.txt`, `patches/fix.diff`, `patches/test.diff`,
`tests-before-fix.txt`, `tests-after-final-fix.txt`, `nereusd-after-fix.txt`,
`build-tests.sh`). R3 plan requirements R-R3-21, R-R3-10.

## Global Constraints

- Work only in the hotfix worktree and its build directory; never checkout, rebase,
  merge, reset or push; never touch the integration, lane B or checkpoint worktrees.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`), never
  `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash characters. Stage
  explicit paths only. Every commit names its R-R3 IDs.
- The fetched rnnoise source is never edited in place; the patched copy lives in the
  build directory and the change is recorded where the repository records patched
  third-party code (find the convention under `docs/attribution/` and follow it).
- Tests: prefix every ctest and test binary with `QT_QPA_PLATFORM=offscreen`. Build
  exact targets. Never build the `NereusSDR` target. No hardware; the Docker container
  on this Mac is allowed and required for the Linux evidence.

## Task 1: Clear rnnoise's file field; fix the GPU-off GUI build

**Requirements:** R-R3-21, R-R3-10.

**Files:**
- Modify: `third_party/rnnoise/CMakeLists.txt` (the patched `denoise.c` copy with the
  anchor guard, as in `patches/fix.diff`), `tests/tst_dsp_asset_store.cpp` (the
  `nr3TrialLoadIgnoresStaleHeapBytes` case, as in `patches/test.diff`),
  `src/gui/SpectrumWidget.cpp` (the `QRhiWidget::contextMenuEvent` call near line 9422
  compiles only where the GPU spectrum is built), the attribution record for the patch
- Test: `tests/tst_dsp_asset_store.cpp`, `tests/tst_dsp_asset_service.cpp`

**Acceptance:**
- In the arm64 container, from this branch's committed tree: `tst_dsp_asset_store`
  (with the new case) and `tst_dsp_asset_service` fail without the CMake patch (show
  it) and pass with it; the staged `nereusd` started with the Rock's configuration
  (radio pinned, no radio or audio device present) logs "nereusd started".
- On the Mac both tests pass.
- A configure with `NEREUS_GPU_SPECTRUM=OFF` builds the GUI library and the two tests.
- Configure stops with a clear message if the anchor text in `denoise.c` changes.

**Verification:**
```sh
cmake --build build-hotfix --target tst_dsp_asset_store tst_dsp_asset_service -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir build-hotfix -R '^(tst_dsp_asset_store|tst_dsp_asset_service)$' --no-tests=error --output-on-failure
```
plus the container runs above (the investigation's `build-tests.sh` shows how), with
their output in the report.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Patch and test with Linux red then green; commit.
- [ ] **Step 2:** GPU-off build fix; commit.
