# macOS Intel Build Restoration — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore the macOS Intel (x86_64) build to `release.yml` so every tagged release ships signed/notarized DMG + PKG for both Apple Silicon and Intel.

**Architecture:** Two-stage delivery on a single feature branch. Stage A pushes a throwaway shakedown workflow to scout `macos-13` runner availability. Stage B (or C, if Option 2 fallback is needed) ports the working matrix shape into `release.yml`, fixes a latent PKG-filename collision in `packaging/macos/hal-installer.sh`, updates `README.md`, and deletes the shakedown file. All in one PR.

**Tech Stack:** GitHub Actions YAML, Bash, Homebrew (or `aqtinstall` if Option 2), CMake, Qt 6.8, FFTW, Rust toolchain (DFNR), Apple Developer ID codesign + notarytool.

**Spec:** [`2026-05-03-macos-intel-build-restoration-design.md`](2026-05-03-macos-intel-build-restoration-design.md)

---

## Branch setup

This work happens in the existing worktree `sweet-euclid-18d4fe` on branch `claude/sweet-euclid-18d4fe`. The branch is already clean and up to date with `main`. No setup required.

If executing from a fresh checkout: `git checkout -b feature/macos-intel-build-restoration`.

## File structure (changes touching the repo)

| File | Stage | Action | Notes |
|---|---|---|---|
| `.github/workflows/release-macos-shakedown.yml` | A | Create | Throwaway scout workflow, deleted in Stage B |
| `.github/workflows/release.yml` | B | Modify | Add Intel matrix row, parameterize PKG filename, remove tombstone comment |
| `packaging/macos/hal-installer.sh` | B | Modify | Honour `PKG_SUFFIX` env var, default `macOS` for back-compat with local invocations |
| `README.md` | B | Modify | Two-arch wording in Releases & Installation section |
| `docs/architecture/2026-05-03-macos-intel-build-restoration-design.md` | B | (already exists) | Spec; commit alongside Stage B changes |
| `docs/architecture/2026-05-03-macos-intel-build-restoration-plan.md` | B | (already exists) | This file; commit alongside Stage B changes |

## Conventions to follow

- **GPG-sign all commits.** Never `--no-gpg-sign`. (`git commit -S`)
- **Match the project's commit-message style.** Recent examples:
  - `release: v0.3.1`
  - `docs(release-notes): use GitHub alert callouts for prominent intro blocks`
  - `fix(rx-applet,spectrum): address Codex review on PR #166`
  - Use `feat(<scope>):`, `fix(<scope>):`, `docs(<scope>):`, `ci(<scope>):` prefixes. For this work the scope is `release` or `ci`.
- **Don't commit anything to `main` directly.** PR-based.
- **No `--no-verify` on commits.** Pre-commit hooks run inline checkers (Thetis attribution, etc.); they should all pass on this work since no source code changes.

---

## Stage A — Shakedown (Phase 1)

### Task A1: Create the shakedown workflow file

**Files:**
- Create: `.github/workflows/release-macos-shakedown.yml`

- [ ] **Step 1: Write the workflow file**

```yaml
name: Release macOS Shakedown (throwaway)

# This workflow exists ONLY to scout `macos-13` runner availability and
# x86_64 build path on macOS before adding the Intel matrix row to
# release.yml. It triggers manually only — no tag push, no pull_request,
# no schedule. Deleted in the Phase 2 commit of the same PR.

on:
  workflow_dispatch:

permissions:
  contents: read

jobs:
  shakedown-macos-intel:
    strategy:
      fail-fast: false
      matrix:
        include:
          - runner: macos-13
            arch: x86_64
            deployment_target: "11.0"
    runs-on: ${{ matrix.runner }}
    steps:
      - uses: actions/checkout@v4

      - name: Verify runner identity
        run: |
          echo "Runner: ${{ matrix.runner }}"
          echo "Arch (uname -m): $(uname -m)"
          echo "macOS version: $(sw_vers -productVersion)"
          echo "Xcode: $(xcodebuild -version 2>&1 | head -1)"

      - name: Install dependencies
        run: brew install qt@6 ninja create-dmg fftw ccache

      - name: Verify Qt version is 6.8+
        run: |
          QT_VER=$(brew list --versions qt@6 | awk '{print $2}')
          echo "Brew qt@6 version: $QT_VER"
          MAJOR=$(echo "$QT_VER" | cut -d. -f1)
          MINOR=$(echo "$QT_VER" | cut -d. -f2)
          if [ "$MAJOR" -lt 6 ] || { [ "$MAJOR" -eq 6 ] && [ "$MINOR" -lt 8 ]; }; then
            echo "::error::Qt $QT_VER is below 6.8 minimum required"
            exit 1
          fi

      - name: Install Rust toolchain
        uses: dtolnay/rust-toolchain@stable

      - name: Set up DeepFilterNet3 library
        run: bash setup-deepfilter.sh

      - name: Configure
        run: |
          cmake -B build -G Ninja \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_OSX_DEPLOYMENT_TARGET="${{ matrix.deployment_target }}" \
            -DCMAKE_OSX_ARCHITECTURES="${{ matrix.arch }}" \
            -DCMAKE_PREFIX_PATH="$(brew --prefix)"

      - name: Build
        run: cmake --build build -j$(sysctl -n hw.ncpu)

      - name: Test
        working-directory: build
        run: ctest --output-on-failure --no-tests=ignore

      - name: Verify built binary is x86_64
        run: |
          BIN_PATH=build/NereusSDR.app/Contents/MacOS/NereusSDR
          if [ ! -f "$BIN_PATH" ]; then
            echo "::error::$BIN_PATH not found"
            exit 1
          fi
          FILE_OUTPUT=$(file "$BIN_PATH")
          echo "$FILE_OUTPUT"
          if ! echo "$FILE_OUTPUT" | grep -q "Mach-O 64-bit executable x86_64"; then
            echo "::error::Binary is NOT x86_64. Got: $FILE_OUTPUT"
            exit 1
          fi
          echo "✓ Binary confirmed x86_64"

      - name: Upload .app artifact for inspection
        uses: actions/upload-artifact@v4
        with:
          name: shakedown-macos-intel-app
          path: build/NereusSDR.app
          retention-days: 3
```

- [ ] **Step 2: Commit**

```bash
git add .github/workflows/release-macos-shakedown.yml \
        docs/architecture/2026-05-03-macos-intel-build-restoration-design.md \
        docs/architecture/2026-05-03-macos-intel-build-restoration-plan.md
git commit -S -m "ci(release): add throwaway macOS Intel shakedown workflow

Scout macos-13 runner availability + x86_64 build path before adding
the Intel matrix row back to release.yml. Workflow_dispatch only.
Deleted in the Phase 2 commit of the same PR.

Spec: docs/architecture/2026-05-03-macos-intel-build-restoration-design.md
Plan: docs/architecture/2026-05-03-macos-intel-build-restoration-plan.md"
```

- [ ] **Step 3: Push branch**

```bash
git push -u origin claude/sweet-euclid-18d4fe
```

### Task A2: Trigger the shakedown and observe

- [ ] **Step 1: Trigger workflow_dispatch**

```bash
gh workflow run release-macos-shakedown.yml --ref claude/sweet-euclid-18d4fe
sleep 5
gh run list --workflow=release-macos-shakedown.yml --limit=1
```

Capture the run ID from the output for the next step.

- [ ] **Step 2: Watch the run to completion**

```bash
RUN_ID=$(gh run list --workflow=release-macos-shakedown.yml --limit=1 --json databaseId --jq '.[0].databaseId')
gh run watch "$RUN_ID" --exit-status
```

Expected (success): `gh run watch` exits 0 once all steps complete green.

- [ ] **Step 3: Branch on outcome**

If the run is **green**: proceed to Stage B (Option 1, native `macos-13`).

If the run **fails at the `runs-on` resolution step** (the original
2026-04-12 failure mode — error like `configuration 'macos-13-us-default'
is not supported`): proceed to Stage C (pivot to Option 2 cross-compile).

If the run **fails at brew install** (e.g. `qt@6` not bottled for
`macos-13`, or version below 6.8): proceed to Stage C.

If the run **fails at any other step** (cmake configure, build, ctest):
diagnose in shakedown context. Fix the underlying issue, re-trigger
shakedown. Do not enter Stage B until shakedown is green. Document the
failure mode in the eventual PR description for posterity.

---

## Stage B — Phase 2 Port (Option 1, native `macos-13`)

**Run this stage only if Stage A's shakedown was green with `runs-on: macos-13`.** If you came from Stage C (Option 2 pivot), skip to "Stage B (Option 2 variant)" further down.

### Task B1: Parameterize the PKG output filename in `hal-installer.sh`

**Files:**
- Modify: `packaging/macos/hal-installer.sh:25` (insert PKG_SUFFIX after VERSION resolution), `:164` (use PKG_SUFFIX in productbuild output path), `:168` (echo line for parity)

- [ ] **Step 1: Read the current state of `hal-installer.sh`**

```bash
sed -n '20,30p;160,170p' packaging/macos/hal-installer.sh
```

Expected output around line 25:
```
VERSION="${VERSION:-$(grep 'project(NereusSDR' CMakeLists.txt | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' || echo "0.0.0")}"
```

Expected output around line 164:
```
PRODUCTBUILD_ARGS+=("${BUILD_DIR}/NereusSDR-${VERSION}-macOS.pkg")
productbuild "${PRODUCTBUILD_ARGS[@]}"

echo ""
echo "=== Installer created: ${BUILD_DIR}/NereusSDR-${VERSION}-macOS.pkg ==="
```

- [ ] **Step 2: Write the failing local "test" — observe current filename**

```bash
# In a scratch dir or just observe what the script WOULD write — don't actually run it
grep -n "macOS.pkg" packaging/macos/hal-installer.sh
```

Expected: two lines, both containing `NereusSDR-${VERSION}-macOS.pkg` literally. This is the "fail" state — there's no way to influence the suffix.

- [ ] **Step 3: Add PKG_SUFFIX env var honouring**

Use the `Edit` tool to make these two changes:

Change 1 — insert PKG_SUFFIX resolution after the VERSION line (around line 25):

Old:
```bash
VERSION="${VERSION:-$(grep 'project(NereusSDR' CMakeLists.txt | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' || echo "0.0.0")}"

echo "=== Building NereusSDR macOS installer v${VERSION} ==="
```

New:
```bash
VERSION="${VERSION:-$(grep 'project(NereusSDR' CMakeLists.txt | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' || echo "0.0.0")}"

# PKG_SUFFIX selects the arch-tagged segment of the output filename. The
# matrix-driven release pipeline sets this per matrix row to one of
# 'macOS-apple-silicon' or 'macOS-intel' so two matrix rows do not
# collide on the same output filename. Defaults to plain 'macOS' so
# devs running this script by hand keep the historical filename.
PKG_SUFFIX="${PKG_SUFFIX:-macOS}"

echo "=== Building NereusSDR macOS installer v${VERSION} (${PKG_SUFFIX}) ==="
```

Change 2 — replace both `-macOS.pkg` references with `-${PKG_SUFFIX}.pkg`:

Old:
```bash
PRODUCTBUILD_ARGS+=("${BUILD_DIR}/NereusSDR-${VERSION}-macOS.pkg")
productbuild "${PRODUCTBUILD_ARGS[@]}"

echo ""
echo "=== Installer created: ${BUILD_DIR}/NereusSDR-${VERSION}-macOS.pkg ==="
```

New:
```bash
PRODUCTBUILD_ARGS+=("${BUILD_DIR}/NereusSDR-${VERSION}-${PKG_SUFFIX}.pkg")
productbuild "${PRODUCTBUILD_ARGS[@]}"

echo ""
echo "=== Installer created: ${BUILD_DIR}/NereusSDR-${VERSION}-${PKG_SUFFIX}.pkg ==="
```

- [ ] **Step 4: Verify the script syntax-parses**

```bash
bash -n packaging/macos/hal-installer.sh
echo "exit=$?"
```

Expected: `exit=0`

- [ ] **Step 5: Verify the env-var is honoured (dry-parse, no actual build)**

Without running the full installer, verify that the variable substitution lands where expected by re-grepping:

```bash
grep -n "PKG_SUFFIX" packaging/macos/hal-installer.sh
```

Expected: 5 lines containing `PKG_SUFFIX`:
1. The comment line "PKG_SUFFIX selects ..."
2. The `PKG_SUFFIX="${PKG_SUFFIX:-macOS}"` assignment
3. The opening `echo "=== Building ... (${PKG_SUFFIX}) ===` line
4. The `PRODUCTBUILD_ARGS+=` line
5. The closing `echo "=== Installer created ... ${PKG_SUFFIX}.pkg ===` line

- [ ] **Step 6: Commit**

```bash
git add packaging/macos/hal-installer.sh
git commit -S -m "fix(packaging): honour PKG_SUFFIX env var for arch-tagged PKG filenames

The matrix-driven macOS release pipeline needs to write two .pkg files
(one per arch) with distinct filenames. PKG_SUFFIX defaults to plain
'macOS' so local invocations and existing scripts keep the historical
filename; release.yml sets it to 'macOS-apple-silicon' or 'macOS-intel'
per matrix row.

Spec: docs/architecture/2026-05-03-macos-intel-build-restoration-design.md"
```

### Task B2: Add the Intel matrix row to `release.yml`

**Files:**
- Modify: `.github/workflows/release.yml:274-285` (matrix include block + tombstone comment)

- [ ] **Step 1: Read current state**

```bash
sed -n '270,295p' .github/workflows/release.yml
```

Expected output:
```yaml
  build-macos:
    needs: prepare
    strategy:
      fail-fast: false
      matrix:
        include:
          - runner: macos-15
            arch: arm64
            dmg_suffix: macOS-apple-silicon
            deployment_target: "14.0"
    # Intel Mac (macos-13) build dropped: GitHub Actions reports the runner
    # label as unsupported ("configuration 'macos-13-us-default' is not
    # supported") as of Phase 3N shakedown 2026-04-12. Intel Mac users should
    # build from source until either the runner is restored OR we cross-
    # compile an x86_64 binary on macos-15 via CMAKE_OSX_ARCHITECTURES.
    # Tracked for Phase 3N+1.
    runs-on: ${{ matrix.runner }}
```

- [ ] **Step 2: Add Intel row + remove tombstone comment**

Use the `Edit` tool:

Old:
```yaml
      matrix:
        include:
          - runner: macos-15
            arch: arm64
            dmg_suffix: macOS-apple-silicon
            deployment_target: "14.0"
    # Intel Mac (macos-13) build dropped: GitHub Actions reports the runner
    # label as unsupported ("configuration 'macos-13-us-default' is not
    # supported") as of Phase 3N shakedown 2026-04-12. Intel Mac users should
    # build from source until either the runner is restored OR we cross-
    # compile an x86_64 binary on macos-15 via CMAKE_OSX_ARCHITECTURES.
    # Tracked for Phase 3N+1.
    runs-on: ${{ matrix.runner }}
```

New:
```yaml
      matrix:
        include:
          - runner: macos-15
            arch: arm64
            dmg_suffix: macOS-apple-silicon
            deployment_target: "14.0"
          - runner: macos-13
            arch: x86_64
            dmg_suffix: macOS-intel
            deployment_target: "11.0"
    runs-on: ${{ matrix.runner }}
```

- [ ] **Step 3: Verify YAML still parses**

```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/release.yml'))" && echo "YAML OK"
```

Expected: `YAML OK`

### Task B3: Wire `PKG_SUFFIX` env var through `release.yml`

**Files:**
- Modify: `.github/workflows/release.yml:429-439` (Build .pkg env block — add PKG_SUFFIX), `:444-455` (notarize + staple PKG path references), `:497-501` (artifact upload — already arch-namespaced, no change needed)

- [ ] **Step 1: Read current state**

```bash
sed -n '425,460p' .github/workflows/release.yml
```

Expected output:
```yaml
      - name: Build .pkg
        env:
          APPLE_INSTALLER_ID: ${{ secrets.APPLE_INSTALLER_ID }}
          # Pass the FULL tag version (including any -rcN / -alpha / -beta
          # suffix) so productbuild writes NereusSDR-${VERSION}-macOS.pkg
          # at the same path the notarize/staple/upload steps below expect.
          # CMakeLists.txt VERSION can only carry the numeric base, so the
          # packaging script's CMake-parse fallback would otherwise drop
          # the suffix and break pre-release builds.
          VERSION: ${{ needs.prepare.outputs.version }}
        run: bash packaging/macos/hal-installer.sh

      - name: Notarize .pkg
        if: env.HAS_APPLE_DEVELOPER_ID == 'true'
        run: |
          xcrun notarytool submit \
            build/NereusSDR-${{ needs.prepare.outputs.version }}-macOS.pkg \
            --apple-id "${{ secrets.APPLE_ID }}" \
            --team-id "${{ secrets.APPLE_TEAM_ID }}" \
            --password "${{ secrets.APPLE_APP_PASSWORD }}" \
            --wait

      - name: Staple .pkg
        if: env.HAS_APPLE_DEVELOPER_ID == 'true'
        run: |
          xcrun stapler staple \
            build/NereusSDR-${{ needs.prepare.outputs.version }}-macOS.pkg
```

- [ ] **Step 2: Add `PKG_SUFFIX` env var, update notarize + staple paths**

Use the `Edit` tool:

Old:
```yaml
      - name: Build .pkg
        env:
          APPLE_INSTALLER_ID: ${{ secrets.APPLE_INSTALLER_ID }}
          # Pass the FULL tag version (including any -rcN / -alpha / -beta
          # suffix) so productbuild writes NereusSDR-${VERSION}-macOS.pkg
          # at the same path the notarize/staple/upload steps below expect.
          # CMakeLists.txt VERSION can only carry the numeric base, so the
          # packaging script's CMake-parse fallback would otherwise drop
          # the suffix and break pre-release builds.
          VERSION: ${{ needs.prepare.outputs.version }}
        run: bash packaging/macos/hal-installer.sh

      - name: Notarize .pkg
        if: env.HAS_APPLE_DEVELOPER_ID == 'true'
        run: |
          xcrun notarytool submit \
            build/NereusSDR-${{ needs.prepare.outputs.version }}-macOS.pkg \
            --apple-id "${{ secrets.APPLE_ID }}" \
            --team-id "${{ secrets.APPLE_TEAM_ID }}" \
            --password "${{ secrets.APPLE_APP_PASSWORD }}" \
            --wait

      - name: Staple .pkg
        if: env.HAS_APPLE_DEVELOPER_ID == 'true'
        run: |
          xcrun stapler staple \
            build/NereusSDR-${{ needs.prepare.outputs.version }}-macOS.pkg
```

New:
```yaml
      - name: Build .pkg
        env:
          APPLE_INSTALLER_ID: ${{ secrets.APPLE_INSTALLER_ID }}
          # Pass the FULL tag version (including any -rcN / -alpha / -beta
          # suffix) so productbuild writes the correctly-suffixed PKG at
          # the same path the notarize/staple/upload steps below expect.
          # CMakeLists.txt VERSION can only carry the numeric base, so the
          # packaging script's CMake-parse fallback would otherwise drop
          # the suffix and break pre-release builds.
          VERSION: ${{ needs.prepare.outputs.version }}
          # PKG_SUFFIX selects the arch-tagged segment of the output PKG
          # filename so the two matrix rows do not collide. Mirrors the
          # DMG dmg_suffix convention.
          PKG_SUFFIX: ${{ matrix.dmg_suffix }}
        run: bash packaging/macos/hal-installer.sh

      - name: Notarize .pkg
        if: env.HAS_APPLE_DEVELOPER_ID == 'true'
        run: |
          xcrun notarytool submit \
            build/NereusSDR-${{ needs.prepare.outputs.version }}-${{ matrix.dmg_suffix }}.pkg \
            --apple-id "${{ secrets.APPLE_ID }}" \
            --team-id "${{ secrets.APPLE_TEAM_ID }}" \
            --password "${{ secrets.APPLE_APP_PASSWORD }}" \
            --wait

      - name: Staple .pkg
        if: env.HAS_APPLE_DEVELOPER_ID == 'true'
        run: |
          xcrun stapler staple \
            build/NereusSDR-${{ needs.prepare.outputs.version }}-${{ matrix.dmg_suffix }}.pkg
```

- [ ] **Step 3: Verify YAML still parses**

```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/release.yml'))" && echo "YAML OK"
```

Expected: `YAML OK`

- [ ] **Step 4: Verify all PKG references are now arch-suffixed**

```bash
grep -n "macOS.pkg" .github/workflows/release.yml || echo "none found (good)"
grep -n 'matrix.dmg_suffix.*\.pkg' .github/workflows/release.yml
```

Expected:
- First grep: `none found (good)`
- Second grep: 2 occurrences — the notarize step and the staple step

### Task B4: Update README.md

**Files:**
- Modify: `README.md:47-48`

- [ ] **Step 1: Read current state**

```bash
sed -n '45,55p' README.md
```

Expected output:
```
## Releases & Installation

Pre-built binaries for Linux (AppImage, x86_64 + aarch64), macOS (DMG +
PKG, Apple Silicon), and Windows (NSIS installer + portable ZIP, x64) are
published as GitHub Releases:
```

- [ ] **Step 2: Update wording**

Use the `Edit` tool:

Old:
```
Pre-built binaries for Linux (AppImage, x86_64 + aarch64), macOS (DMG +
PKG, Apple Silicon), and Windows (NSIS installer + portable ZIP, x64) are
published as GitHub Releases:
```

New:
```
Pre-built binaries for Linux (AppImage, x86_64 + aarch64), macOS (DMG +
PKG, Apple Silicon + Intel), and Windows (NSIS installer + portable ZIP,
x64) are published as GitHub Releases:
```

- [ ] **Step 3: Verify**

```bash
grep -n "Apple Silicon + Intel" README.md
```

Expected: one match at line 48 (or thereabouts).

### Task B5: Delete the shakedown workflow file

**Files:**
- Delete: `.github/workflows/release-macos-shakedown.yml`

- [ ] **Step 1: Remove**

```bash
git rm .github/workflows/release-macos-shakedown.yml
```

- [ ] **Step 2: Verify gone**

```bash
ls .github/workflows/release-macos-shakedown.yml 2>&1 | grep -q "No such file" && echo "gone (good)"
```

Expected: `gone (good)`

### Task B6: Commit Stage B as a single logical commit

- [ ] **Step 1: Stage everything**

```bash
git status
```

Expected modified/deleted files:
- modified: `.github/workflows/release.yml`
- modified: `README.md`
- deleted:  `.github/workflows/release-macos-shakedown.yml`

(`packaging/macos/hal-installer.sh` was committed separately in Task B1.)

- [ ] **Step 2: Commit**

```bash
git add .github/workflows/release.yml README.md
git commit -S -m "ci(release): add macOS Intel matrix row, drop shakedown workflow

Adds a second build-macos matrix row (runner: macos-13, arch: x86_64,
deployment_target: 11.0) so the release pipeline produces signed +
notarized DMG + PKG artifacts for both Apple Silicon and Intel Macs.

PKG filenames now include the arch suffix (macOS-apple-silicon /
macOS-intel) — matches the DMG convention. PKG_SUFFIX env var threads
the matrix value through to packaging/macos/hal-installer.sh.

The throwaway release-macos-shakedown.yml that scouted runner
availability is removed.

README updated to advertise both arch paths.

Spec: docs/architecture/2026-05-03-macos-intel-build-restoration-design.md
Plan: docs/architecture/2026-05-03-macos-intel-build-restoration-plan.md"
```

- [ ] **Step 3: Verify commit log**

```bash
git log --oneline main..HEAD
```

Expected: 2-3 commits (shakedown create, hal-installer fix, Stage B port).

### Task B7: Push and self-test (optional but recommended)

- [ ] **Step 1: Push the updated branch**

```bash
git push origin claude/sweet-euclid-18d4fe
```

- [ ] **Step 2 (recommended skip): synthetic-tag dry-run**

The cleanest way to verify the modified `release.yml` end-to-end without churning a real release is to tag a synthetic version, watch the pipeline produce a draft release, then delete tag + draft.

**This step is OPTIONAL.** The shakedown already proved the build path; the Stage B changes are incremental wiring (matrix row + filename refs) with small bug surface. Skipping means the next real release validates the wiring in production. Maintainer's call — confirm before doing this. If skipping, jump to Task B8.

If running the dry-run:

```bash
# 1. Add a CHANGELOG stub for the synthetic version
sed -i '' '/^## \[0\.3\.1\]/i\
## [0.3.2-test1] - 2026-05-03\
\
Synthetic test tag for macOS Intel matrix dry-run. Do not ship.\
\
' CHANGELOG.md

# 2. Bump CMakeLists.txt to match (since prepare validates this)
sed -i '' 's/project(NereusSDR VERSION 0\.3\.1/project(NereusSDR VERSION 0.3.2/' CMakeLists.txt

# 3. Commit + tag
git add CHANGELOG.md CMakeLists.txt
git commit -S -m "ci(release): synthetic v0.3.2-test1 tag for macOS Intel dry-run

WILL BE REVERTED before merge."
git tag v0.3.2-test1
git push origin v0.3.2-test1
```

- [ ] **Step 3 (only if Step 2 was run): watch the run to completion**

```bash
sleep 10
RUN_ID=$(gh run list --workflow=release.yml --limit=1 --json databaseId --jq '.[0].databaseId')
gh run watch "$RUN_ID" --exit-status
```

Expected: green workflow ending with a draft release containing all 4 macOS artifacts:

```bash
gh release view v0.3.2-test1 --json assets --jq '.assets[].name' | grep macOS
```

Expected output:
```
NereusSDR-0.3.2-test1-macOS-apple-silicon.dmg
NereusSDR-0.3.2-test1-macOS-apple-silicon.dmg.asc
NereusSDR-0.3.2-test1-macOS-apple-silicon.pkg
NereusSDR-0.3.2-test1-macOS-apple-silicon.pkg.asc
NereusSDR-0.3.2-test1-macOS-intel.dmg
NereusSDR-0.3.2-test1-macOS-intel.dmg.asc
NereusSDR-0.3.2-test1-macOS-intel.pkg
NereusSDR-0.3.2-test1-macOS-intel.pkg.asc
```

- [ ] **Step 4 (only if Step 2 was run): clean up synthetic tag, draft release, and revert commit**

```bash
gh release delete v0.3.2-test1 --yes --cleanup-tag
git push origin :refs/tags/v0.3.2-test1   # may already be deleted by --cleanup-tag
git revert --no-edit HEAD
git push origin claude/sweet-euclid-18d4fe
```

Expected: synthetic tag and draft release gone, branch is back to the clean Stage B state.

### Task B8: Open the PR

- [ ] **Step 1: Confirm with the maintainer first**

The user has a standing rule: never open a PR without showing the description first. Draft the PR description in chat, get the explicit go-ahead, then run the `gh pr create` command.

PR body draft (adjust based on actual shakedown outcome):

```markdown
## Summary

Restores the macOS Intel (x86_64) build to the release pipeline. Every tagged release will now ship four macOS artifacts (DMG + PKG × Apple Silicon + Intel), all signed with the existing Developer ID cert, all notarized + stapled, all in `SHA256SUMS.txt`.

## Approach

Followed the two-phase plan in [`docs/architecture/2026-05-03-macos-intel-build-restoration-plan.md`](docs/architecture/2026-05-03-macos-intel-build-restoration-plan.md):

- **Phase 1**: pushed a throwaway `release-macos-shakedown.yml` workflow to scout `macos-13` runner availability + brew bottle resolution + x86_64 build path. Result: <green / red, fill in actual outcome>.
- **Phase 2**: ported the matrix row into `release.yml`, fixed a latent PKG-filename collision in `packaging/macos/hal-installer.sh` (added `PKG_SUFFIX` env var), updated `README.md`, deleted the shakedown file.

Spec: [`docs/architecture/2026-05-03-macos-intel-build-restoration-design.md`](docs/architecture/2026-05-03-macos-intel-build-restoration-design.md)

## Notable side-effect

The existing arm64 PKG renames from `NereusSDR-X.Y.Z-macOS.pkg` → `NereusSDR-X.Y.Z-macOS-apple-silicon.pkg` to mirror the DMG convention. Anyone with download automation hardcoding the old name will need to update.

## Test plan

- [x] Phase 1 shakedown ran green on `macos-13` (run #<fill in>)
- [x] Built binary verified `Mach-O 64-bit executable x86_64`
- [ ] (optional) Phase 2 synthetic-tag dry-run produced 4 expected macOS artifacts
- [ ] Manual smoke test on real Intel Mac after next real release: download Intel DMG, verify Gatekeeper accepts notarized binary, verify spectrum + audio work
```

- [ ] **Step 2: Open the PR (only after maintainer says go)**

```bash
gh pr create \
  --base main \
  --head claude/sweet-euclid-18d4fe \
  --title "ci(release): restore macOS Intel build artifacts" \
  --body "$(cat <<'EOF'
[paste approved body here]
EOF
)"
```

---

## Stage C — Pivot to Option 2 (cross-compile from `macos-15`)

**Run this stage only if Stage A's shakedown failed on runner availability or brew bottle resolution.**

### Task C1: Pivot the shakedown file to cross-compile shape

**Files:**
- Modify: `.github/workflows/release-macos-shakedown.yml` (rewrite the matrix + steps)

- [ ] **Step 1: Replace the shakedown contents**

Use the `Write` tool to overwrite the file:

```yaml
name: Release macOS Shakedown (throwaway, Option 2)

# Pivoted from Option 1 (native macos-13) to Option 2 (cross-compile
# x86_64 from macos-15 via aqtinstall). Scouts the cross-compile path
# before adding the Intel matrix row to release.yml. Workflow_dispatch
# only. Deleted in the Phase 2 commit of the same PR.

on:
  workflow_dispatch:

permissions:
  contents: read

jobs:
  shakedown-macos-intel-crosscompile:
    strategy:
      fail-fast: false
      matrix:
        include:
          - runner: macos-15
            arch: x86_64
            deployment_target: "11.0"
    runs-on: ${{ matrix.runner }}
    steps:
      - uses: actions/checkout@v4

      - name: Verify runner identity + Rosetta presence
        run: |
          echo "Runner: ${{ matrix.runner }}"
          echo "Host arch: $(uname -m)"
          echo "macOS version: $(sw_vers -productVersion)"
          if /usr/bin/arch -x86_64 /usr/bin/true 2>/dev/null; then
            echo "✓ Rosetta is available — x86_64 binaries can run for ctest"
          else
            echo "::warning::Rosetta NOT present on runner — ctest will be skipped"
          fi

      - name: Install non-Qt brew packages
        run: brew install ninja create-dmg ccache

      - name: Install Qt 6.8 for x86_64 macOS via aqtinstall
        uses: jurplel/install-qt-action@v4
        with:
          version: '6.8.*'
          host: 'mac'
          target: 'desktop'
          arch: 'clang_64'
          modules: 'qtmultimedia qtshadertools'
          cache: true

      - name: Build FFTW from source for x86_64
        run: |
          curl -fsSL https://fftw.org/pub/fftw/fftw-3.3.10.tar.gz -o fftw.tar.gz
          tar xzf fftw.tar.gz
          cd fftw-3.3.10
          ./configure --enable-single --enable-shared --disable-doc \
            --prefix="${{ github.workspace }}/fftw-x86_64" \
            CFLAGS="-arch x86_64 -mmacosx-version-min=11.0" \
            --host=x86_64-apple-darwin
          make -j$(sysctl -n hw.ncpu)
          make install

      - name: Install Rust toolchain with x86_64 target
        uses: dtolnay/rust-toolchain@stable
        with:
          targets: x86_64-apple-darwin

      - name: Set up DeepFilterNet3 library (cross-compile path)
        env:
          # setup-deepfilter.sh detects host arch by default; force the
          # cross-compile target.
          CARGO_BUILD_TARGET: x86_64-apple-darwin
        run: bash setup-deepfilter.sh

      - name: Configure
        run: |
          cmake -B build -G Ninja \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_OSX_DEPLOYMENT_TARGET="${{ matrix.deployment_target }}" \
            -DCMAKE_OSX_ARCHITECTURES="${{ matrix.arch }}" \
            -DCMAKE_PREFIX_PATH="${{ github.workspace }}/fftw-x86_64;$(qmake -query QT_INSTALL_PREFIX)"

      - name: Build
        run: cmake --build build -j$(sysctl -n hw.ncpu)

      - name: Test (via Rosetta if available)
        working-directory: build
        run: |
          if /usr/bin/arch -x86_64 /usr/bin/true 2>/dev/null; then
            ctest --output-on-failure --no-tests=ignore
          else
            echo "::warning::Skipping ctest — Rosetta not present on runner"
          fi

      - name: Verify built binary is x86_64
        run: |
          BIN_PATH=build/NereusSDR.app/Contents/MacOS/NereusSDR
          FILE_OUTPUT=$(file "$BIN_PATH")
          echo "$FILE_OUTPUT"
          if ! echo "$FILE_OUTPUT" | grep -q "Mach-O 64-bit executable x86_64"; then
            echo "::error::Binary is NOT x86_64. Got: $FILE_OUTPUT"
            exit 1
          fi
          echo "✓ Binary confirmed x86_64"

      - name: Upload .app artifact for inspection
        uses: actions/upload-artifact@v4
        with:
          name: shakedown-macos-intel-crosscompile-app
          path: build/NereusSDR.app
          retention-days: 3
```

- [ ] **Step 2: Commit the pivot**

```bash
git add .github/workflows/release-macos-shakedown.yml
git commit -S -m "ci(release): pivot macOS shakedown to Option 2 (cross-compile)

Stage A's native macos-13 shakedown failed on <reason from Stage A
Step 3>. Pivoting to Option 2 per spec: cross-compile x86_64 from
macos-15 via aqtinstall + FFTW from source + Rust target
x86_64-apple-darwin."
git push origin claude/sweet-euclid-18d4fe
```

- [ ] **Step 3: Re-trigger and watch**

```bash
gh workflow run release-macos-shakedown.yml --ref claude/sweet-euclid-18d4fe
sleep 5
RUN_ID=$(gh run list --workflow=release-macos-shakedown.yml --limit=1 --json databaseId --jq '.[0].databaseId')
gh run watch "$RUN_ID" --exit-status
```

If green: proceed to Stage B (Option 2 variant) below.

If red: diagnose, fix, repeat. Do not enter Stage B until green.

### Task C2: Stage B (Option 2 variant) — when cross-compile is the path

This is structurally the same as the Option 1 Stage B with these per-task differences. Run all tasks in Stage B order (B1 → B6 → B7 → B8) but apply the deltas below.

- **Task B1 (`hal-installer.sh` PKG_SUFFIX)** — unchanged. Same edit.

- **Task B2 (matrix row)** — Stage A's pivoted shakedown picked the `runs-on: macos-15` cross-compile shape, so the new matrix row is:

```yaml
          - runner: macos-15
            arch: x86_64
            dmg_suffix: macOS-intel
            deployment_target: "11.0"
```

  (Note: same `runner` value as the arm64 row. The matrix discriminant is `arch`.)

  **Plus** the build-macos job needs additional cross-compile plumbing — these are new steps to insert:

  1. Replace the unconditional `brew install qt@6 ninja create-dmg fftw ccache` with a conditional split:
     - On the arm64 row: keep `brew install qt@6 ninja create-dmg fftw ccache` as-is.
     - On the x86_64 row: install non-Qt brew packages (`brew install ninja create-dmg ccache`), then install Qt via `jurplel/install-qt-action@v4` with `arch: clang_64`, then build FFTW from source.
  2. Add `targets: x86_64-apple-darwin` to the `dtolnay/rust-toolchain@stable` step, gated on `${{ matrix.arch == 'x86_64' }}`.
  3. Pass `CARGO_BUILD_TARGET: x86_64-apple-darwin` env to `setup-deepfilter.sh` on the x86_64 row.
  4. Adjust `CMAKE_PREFIX_PATH` per row.

  These are non-trivial. Mirror the exact step contents from the pivoted shakedown file (Task C1) — copy-paste the working steps, adapting `if:` guards to gate per-arch.

- **Task B3 (PKG_SUFFIX wiring)** — unchanged. Same edit.

- **Task B4 (README.md)** — unchanged. Same edit.

- **Task B5 (delete shakedown)** — unchanged.

- **Task B6 (commit)** — adjust commit message:

```
ci(release): add macOS Intel cross-compile matrix row + PKG suffix

Adds a second build-macos matrix row (runner: macos-15, arch: x86_64,
deployment_target: 11.0) cross-compiling x86_64 from the Apple Silicon
runner via aqtinstall (Qt) + FFTW-from-source + Rust target
x86_64-apple-darwin. The native macos-13 path was unviable on
2026-05-03 due to <reason>.

[rest as Task B6 message]
```

- **Task B7 (push + self-test)** — unchanged.

- **Task B8 (PR)** — adjust PR description "Approach" section to mention Option 2.

---

## Self-review notes

- **Spec coverage:** Every acceptance criterion in the spec maps to a task. Goal (4 macOS artifacts) → Tasks B2 + B3 + B5 (Option 1) or B2-variant (Option 2). Filename convention → Tasks B1 + B3. README → Task B4. Tombstone removal → Task B2. Shakedown deletion → Task B5. End-to-end verification → Task B7.
- **Placeholder scan:** No "TBD", no "TODO", no "implement later" stubs. The optional synthetic-tag dry-run in Task B7 has full code; skipping it is an explicit maintainer decision, not a placeholder.
- **Consistency check:** PKG_SUFFIX is named the same way in `hal-installer.sh` (Task B1), the env-var passing block in `release.yml` (Task B3), and the matrix `dmg_suffix` it draws from. Notarize and staple steps both use `${{ matrix.dmg_suffix }}` consistently. Matrix row YAML keys (`runner`, `arch`, `dmg_suffix`, `deployment_target`) match between Stage A shakedown, Stage B port, and Stage C pivot.
- **Bite-sized:** Steps are 2-5 minutes each. The longest single step is Task B7 Step 3 (watching the workflow run) which is wall-clock dominated by CI not by engineer attention.
- **Branch logic:** Stages A → B is the success path. Stages A → C → B-variant is the fallback. Both terminal states converge on the same set of files committed.

## Out of scope (not in this plan)

- Universal binary (rejected in spec).
- Bumping arm64 deployment target from 14.0.
- Linux x86_64 macOS path.
- Changes to Windows or Linux release jobs.
- Any source code (`src/`), test code (`tests/`), or third-party (`third_party/`) changes.
