# macOS Intel build restoration — design

**Status**: Draft, awaiting maintainer review
**Author**: Claude (Opus 4.7) on behalf of JJ Boyd / KG4VCF
**Date**: 2026-05-03
**Related**: [`2026-04-12-phase3n-release-pipeline-design.md`](2026-04-12-phase3n-release-pipeline-design.md)
(original Phase 3N design that introduced the macOS arm64 build)

## Goal

Restore the macOS Intel (x86_64) build to `release.yml` so the GitHub
Release for every tagged version ships **two** macOS artifact pairs —
one for Apple Silicon, one for Intel — both signed with the existing
Developer ID certificate, both notarized, both stapled, both listed in
`SHA256SUMS.txt`.

## Background

Phase 3N (Packaging) originally shipped a two-arch macOS matrix
(`macos-15` / arm64 + `macos-13` / x86_64). During shakedown on
2026-04-12 the `macos-13` runner was reported by GitHub Actions as an
unsupported runner label ("configuration `macos-13-us-default` is not
supported"), so the Intel row was deleted and a tombstone comment was
left in `release.yml` lines 280-285 deferring the restoration to "Phase
3N+1." The arm64 row has shipped every release since v0.2.0.

GitHub Actions runner availability has fluctuated since 2026-04-12, and
the goal of this work is to retry the native `macos-13` path
empirically before committing to a structural change. If the runner is
still unavailable, the cross-compile fallback (option 2 below) does the
job from the existing `macos-15` runner without depending on a second
runner OS.

## Approach

Two-phase delivery on a single feature branch and a single PR.

### Phase 1 — Shakedown (cheap, throwaway)

Add a one-off workflow file, `.github/workflows/release-macos-shakedown.yml`,
that:

- Triggers **only** on `workflow_dispatch` (no tag push, no implicit
  triggers — cannot accidentally affect a real release).
- Runs a single matrix-of-1 row (deliberately mirrors `release.yml`'s
  `build-macos` matrix shape so the working row can be lifted into
  `release.yml` verbatim in Phase 2): `runner: macos-13`, `arch:
  x86_64`, `deployment_target: "11.0"`.
- Mirrors the steps from `release.yml`'s `build-macos` job up to and
  including `ctest`, plus a one-liner that runs
  `file build/NereusSDR.app/Contents/MacOS/NereusSDR` and asserts the
  output contains `Mach-O 64-bit executable x86_64` (catches the
  Rosetta-trap where the build ran on an arm64 runner instead of an
  x86_64 one).
- Does **not** import Apple secrets, **not** run codesign with a Developer
  ID, **not** create a DMG or PKG, **not** notarize, **not** upload a
  release asset. It uploads the unsigned `.app` bundle as a workflow
  artifact for inspection only.

The shakedown file lives on the feature branch only. It is **deleted**
in Phase 2 of the same PR — it does not ship to `main` long-term.

### Phase 1 decision tree

| Shakedown outcome | Action |
|---|---|
| Green end-to-end | Proceed to Phase 2 with **Option 1** (native `macos-13`). |
| Red on `runs-on` resolution (the original failure) | Pivot the shakedown file to **Option 2** (cross-compile from `macos-15` via `aqtinstall` + FFTW from source + `targets: x86_64-apple-darwin` on the Rust toolchain). Re-run the shakedown. |
| Red on brew bottle resolution (e.g. `qt@6` not bottled for `macos-13`) | Same pivot to Option 2. |
| Red on cmake / build / link / ctest | Diagnose in shakedown context; do not enter Phase 2 until green. |

### Phase 2 — Port the working matrix into `release.yml`

Once Phase 1 is green, the same PR additionally:

1. Adds a second matrix row to `release.yml`'s `build-macos` job. Shape
   for **Option 1** (native, the recommended path):
   ```yaml
   - runner: macos-13
     arch: x86_64
     dmg_suffix: macOS-intel
     deployment_target: "11.0"
   ```
   Shape for **Option 2** (cross-compile, fallback) — same `runs-on`
   value as the arm64 row, but with `arch: x86_64` and the additional
   plumbing called out in §"Option 2 plumbing" below.

2. Removes the tombstone comment block at `release.yml` lines 280-285.

3. Renames the PKG output filename to match the DMG arch-suffix
   convention. Today the PKG is written as
   `NereusSDR-${VERSION}-macOS.pkg` from
   [`packaging/macos/hal-installer.sh:164`](../../packaging/macos/hal-installer.sh)
   and referenced at the same name at `release.yml:445` and `:455`.
   Adding an Intel build without changing this would cause both
   matrix rows to write the same filename — per-job artifact upload
   uses `name: macos-hal-pkg-${{ matrix.arch }}` so the upload itself
   succeeds, but the staging step at `release.yml:693-700` does
   `find ... -name '*.pkg' -exec cp {} artifacts/` and the second copy
   silently overwrites the first.

   Fix: introduce a `PKG_SUFFIX` env var in `hal-installer.sh` that
   defaults to `macOS` (no behaviour change for local devs running the
   script by hand) and is set by `release.yml` to the matrix's
   `dmg_suffix` value (`macOS-apple-silicon` / `macOS-intel`). The PKG
   output line becomes:
   ```bash
   PKG_SUFFIX="${PKG_SUFFIX:-macOS}"
   PRODUCTBUILD_ARGS+=("${BUILD_DIR}/NereusSDR-${VERSION}-${PKG_SUFFIX}.pkg")
   ```
   Both arm64 and Intel PKGs gain arch-suffixed names, mirroring the
   DMG convention. The notarize, staple, and upload steps in
   `release.yml` get the matching `${{ matrix.dmg_suffix }}`
   substitution.

4. Updates `README.md:47-48` from
   > Pre-built binaries for Linux (AppImage, x86_64 + aarch64), macOS
   > (DMG + PKG, Apple Silicon), and Windows ...

   to call out both arches:
   > Pre-built binaries for Linux (AppImage, x86_64 + aarch64), macOS
   > (DMG + PKG, Apple Silicon + Intel), and Windows ...

   No other README sections need touching.

5. Deletes `.github/workflows/release-macos-shakedown.yml`.

## Option 2 plumbing (only if Option 1 fails)

If `macos-13` is unusable, the matrix shape changes to
cross-compile both arches from the `macos-15` runner:

- Both matrix rows use `runner: macos-15`. `arch` differs (`arm64` /
  `x86_64`).
- Replace the `brew install qt@6` step with `jurplel/install-qt-action@v4`
  using `arch: clang_64` for x86_64 macOS Qt. Keep `brew install ninja
  create-dmg ccache`. **Drop** brew FFTW — see next.
- Build FFTW from source for x86_64 (small library, ~30s additional CI
  time). The arm64 row continues to use brew FFTW.
- Add a conditional `targets: x86_64-apple-darwin` to the
  `dtolnay/rust-toolchain@stable` step, gated on `${{ matrix.arch ==
  'x86_64' }}`.
- `ctest` for the x86_64 row runs the test binaries via Rosetta on the
  arm64 runner. Runtime cost: ~2× slower than native, but the test
  suite is small enough that this adds <2 min to CI wall-clock.
  Requires Rosetta to be present on the `macos-15` runner image
  (verify in the shakedown — Apple ships Rosetta on stock 15 but
  GitHub may strip it).

The decision between Option 1 and Option 2 is made empirically by the
shakedown. The PR opens with whichever shape works.

## Files touched

| File | Phase | Change |
|---|---|---|
| `.github/workflows/release-macos-shakedown.yml` | 1 (create), 2 (delete) | Throwaway scout workflow |
| `.github/workflows/release.yml` | 2 | Add matrix row, remove comment block 280-285, parameterize PKG filename |
| `packaging/macos/hal-installer.sh` | 2 | Add `PKG_SUFFIX` env, default `macOS` |
| `README.md` | 2 | Two-arch wording in Releases & Installation section (line 47-48) |

No source code (`src/`), no test code (`tests/`), no third-party
(`third_party/`), no other workflow files.

## Acceptance criteria

- `release.yml`'s `build-macos` matrix includes both `arm64` and
  `x86_64` rows. Both produce signed + notarized + stapled DMG and PKG
  artifacts.
- Artifact filenames:
  - `NereusSDR-X.Y.Z-macOS-apple-silicon.dmg`
  - `NereusSDR-X.Y.Z-macOS-apple-silicon.pkg`
  - `NereusSDR-X.Y.Z-macOS-intel.dmg`
  - `NereusSDR-X.Y.Z-macOS-intel.pkg`
- All four are present in `SHA256SUMS.txt` and signed by the existing
  GPG key (`KG4VCF`).
- `README.md` Releases & Installation section mentions both arch
  paths.
- The tombstone comment block at `release.yml:280-285` is gone.
- The `release-macos-shakedown.yml` file is gone from `main` post-merge.
- A real release (or a workflow_dispatch invocation against a real
  tag) produces all four artifacts end-to-end without manual
  intervention.

## Non-goals

- **Universal binary**. Considered and rejected: the realistic
  build path is still two matrix rows plus a third `lipo`-merge job, so
  it is the existing two-build cost *plus* a third job rather than a
  replacement for two builds. ~1.4× artifact size on disk for users
  who only need one slice. Two separate artifacts is the chosen UX.
- **Universal cross-arch testing**. `ctest` runs once per matrix row
  on its own arch (or via Rosetta in the Option 2 fallback). No
  matrix-of-matrices.
- **Linux x86_64 macOS path**. Out of scope.
- **Bumping the existing arm64 deployment target** (currently 14.0,
  Sonoma). Separate decision, not on the table here.
- **Changing Windows or Linux jobs.** No intentional or incidental
  edits.
- **Adding a test suite for the workflow itself** beyond the shakedown
  scout. The `release.yml` workflow is exercised every release; a
  regression would surface there.

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| `macos-13` runner is still unavailable (the original failure) | Phase 1 shakedown surfaces this in ~3 minutes for ~$0 of CI; pivot to Option 2 with the same shakedown infrastructure. |
| Brew bottle for `qt@6` on `macos-13` resolves to <6.8 | Phase 1 shakedown's cmake configure step would fail on `find_package(Qt6 6.8 ...)`; pivot to Option 2 (`aqtinstall` pins Qt version exactly). |
| Renaming the existing arm64 PKG (`-macOS.pkg` → `-macOS-apple-silicon.pkg`) breaks downstream automation that hardcoded the old name | Realistic blast radius is small — release URLs change every version anyway, and PKG download counts are an order of magnitude below DMG. Acceptable break, called out in release notes. |
| Notarization adds wall-clock to release runs | The `xcrun notarytool submit --wait` step blocks for ~5-10 min per artifact. Two notarization rounds (DMG + PKG) per arch × 2 arches = 4 notarization rounds total. The matrix runs in parallel, so wall-clock is ~10 min added per release, not 4×. |
| Apple Developer ID cert revocation or notary outage | Inherited from existing arm64 path; no new exposure. |
| DFNR Rust build fails on x86_64 macOS | `setup-deepfilter.sh` is already arch-aware (`darwin-x86_64` → `x86_64-apple-darwin`); the pre-built binary fallback path covers the case where Rust source build fails on the runner. Phase 1 shakedown exercises this. |

## Test approach (manual + automated)

- **Phase 1 shakedown** is the primary automated validation. Triggered
  manually via `workflow_dispatch` against the feature branch. Pass =
  green workflow + correct `Mach-O 64-bit executable x86_64` output.
- **Phase 2 release.yml dry-run**: trigger via `workflow_dispatch` with
  the `tag` input pointing at either the next real release tag (after
  CMakeLists.txt + CHANGELOG bump) or a separate test tag with a
  CHANGELOG stub. The full pipeline (build → notarize → publish draft)
  must complete without manual steps. The draft can be deleted after
  inspection if it was a test tag.
- **Manual smoke test on a real Intel Mac** (deferred to whoever owns
  the next-release sign-off, not blocking for the PR): download the
  Intel DMG from the published release, mount, drag-install, launch,
  verify Gatekeeper accepts the notarized binary, verify spectrum +
  audio work.

## Implementation sequencing (high-level)

This section is intentionally light — the detailed step-by-step lives
in the implementation plan that follows the spec review. Rough shape:

1. Branch from `main`.
2. Write Phase 1 shakedown workflow file. Push. Trigger. Observe.
3. Branch on outcome (green → Option 1; red → Option 2 pivot in same
   shakedown file → re-trigger).
4. Once shakedown is green, write Phase 2 changes (release.yml matrix
   row, hal-installer.sh `PKG_SUFFIX`, README two-arch wording, delete
   shakedown file).
5. Self-test the modified `release.yml` via `workflow_dispatch` against
   a tag (with CHANGELOG stub if using a synthetic tag).
6. Open PR with full release-tag dry-run results in the PR description.
