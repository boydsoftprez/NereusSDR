# Phase 3N — Release Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship reproducible, GPG-signed alpha releases of NereusSDR for Linux / macOS / Windows from a single tag-triggered workflow, plus a `/release` skill that drives bump → changelog → tag → push → watch → open-draft.

**Architecture:** Consolidate `appimage.yml` + `macos-dmg.yml` + `windows-installer.yml` + `sign-release.yml` into one `release.yml` (jobs: `prepare` → `build-linux`/`build-macos`/`build-windows` in parallel → `sign-and-publish`). Tighten `ci.yml` with caching, tests, PR artifact uploads, and concurrency cancellation. Add a Claude Code skill at `~/.claude/skills/release/` that orchestrates the human side.

**Tech Stack:** GitHub Actions, CMake/Ninja, Qt6, `linuxdeploy` (Linux), `macdeployqt` + `create-dmg` + ad-hoc `codesign` (macOS), `windeployqt` + portable ZIP (Windows), GPG (KG4VCF) for `SHA256SUMS.txt`, `gh` CLI, bash for the skill helper.

**Spec:** [`docs/architecture/2026-04-12-phase3n-release-pipeline-design.md`](2026-04-12-phase3n-release-pipeline-design.md)

---

## File Structure

**New files:**

| Path | Responsibility |
|---|---|
| `.github/workflows/release.yml` | Tag-triggered consolidated release workflow (4 jobs) |
| `.github/release-notes-template.md` | Template appended to every draft release body |
| `scripts/windows/installer.nsi` | NSIS installer script (Start Menu, uninstaller, install dir) |
| `~/.claude/skills/release/SKILL.md` | Skill instructions for Claude Code |
| `~/.claude/skills/release/bin/release.sh` | Deterministic shell helper invoked by the skill |
| `docs/architecture/2026-04-12-phase3n-release-pipeline-plan.md` | This plan |

**Release artifact set (mirrors AetherSDR shape):**

| Artifact | Job | Notes |
|---|---|---|
| `NereusSDR-vX.Y.Z-x86_64.AppImage` | build-linux (x86_64) | |
| `NereusSDR-vX.Y.Z-aarch64.AppImage` | build-linux (aarch64) | |
| `NereusSDR-vX.Y.Z-macOS-apple-silicon.dmg` | build-macos (arm64) | ad-hoc signed |
| `NereusSDR-vX.Y.Z-macOS-intel.dmg` | build-macos (x86_64) | ad-hoc signed, macos-13 runner |
| `NereusSDR-vX.Y.Z-Windows-x64-portable.zip` | build-windows | extract-and-run |
| `NereusSDR-vX.Y.Z-Windows-x64-setup.exe` | build-windows | NSIS installer w/ Start Menu + uninstaller |
| `NereusSDR-vX.Y.Z-source.tar.gz` | sign-and-publish | git archive of the tag |
| `SHA256SUMS.txt` + `.asc` | sign-and-publish | GPG-signed by KG4VCF |
| `*.asc` for each binary | sign-and-publish | detached GPG signatures |

**Modified files:**

| Path | Change |
|---|---|
| `.github/workflows/ci.yml` | Add ccache, ctest, artifact uploads, concurrency group |
| `README.md` | Add "Releases & Installation" section pointing at GH Releases |

**Deleted files (Task 8 only):**

- `.github/workflows/appimage.yml`
- `.github/workflows/macos-dmg.yml`
- `.github/workflows/windows-installer.yml`
- `.github/workflows/sign-release.yml`

**Notes:**
- The existing `appimage.yml` builds both `x86_64` and `aarch64`. The plan **preserves both** in the new `build-linux` matrix.
- The existing `macos-dmg.yml` builds Apple Silicon only. The plan **adds Intel** as a second matrix entry on `macos-13` (Apple's last Intel-capable runner) to mirror the AetherSDR release shape.
- The existing `windows-installer.yml` is misnamed — it produces a portable ZIP, not a real installer. The plan **ships both** a portable ZIP (extract-and-run, power users) and an NSIS-built `setup.exe` (Start Menu shortcut, uninstaller, normal users). Authenticode signing of the `setup.exe` is Phase 3N+1.
- Tests do not currently exist on `main` (they live on a worktree branch). Plan adds `ctest --output-on-failure` calls anyway — `ctest` returns success when no tests are registered, and the worktree merge will retroactively populate them.

---

## Pre-flight (one-time, manual — do these BEFORE Task 1)

- [ ] **PF.1** — Confirm GPG key `KG4VCF` is the one already wired into the existing `sign-release.yml`. Run:
  ```bash
  gh secret list --repo boydsoftprez/NereusSDR | grep -E 'GPG_PRIVATE_KEY|GPG_PASSPHRASE'
  ```
  Expected: both secrets present. If absent, ask the user to populate them — Phase 3N cannot ship without them.

- [ ] **PF.2** — Confirm the worktree is clean and you are on a fresh feature branch off `main`:
  ```bash
  cd /Users/j.j.boyd/NereusSDR
  git checkout main && git pull --ff-only
  git checkout -b feature/phase3n-release-pipeline
  git status
  ```
  Expected: clean working tree, on `feature/phase3n-release-pipeline`.

---

## Task 1: Tighten `ci.yml` (caching, tests, artifacts, concurrency)

**Files:**
- Modify: `.github/workflows/ci.yml`

**Why first:** the tightened CI is what proves your future PRs are green before the `release` skill will let you tag. Land it first so subsequent tasks build on a faster, more thorough CI.

- [ ] **Step 1.1: Add concurrency group at the top of the workflow**

  Edit `.github/workflows/ci.yml`. After the `on:` block (line 8), insert:

  ```yaml
  concurrency:
    group: ci-${{ github.ref }}
    cancel-in-progress: true
  ```

- [ ] **Step 1.2: Add ccache restore step (Linux + macOS)**

  After the existing dependency-install steps for Linux/macOS, add:

  ```yaml
        - name: Install ccache (Linux)
          if: runner.os == 'Linux'
          run: sudo apt-get install -y ccache

        - name: Install ccache (macOS)
          if: runner.os == 'macOS'
          run: brew install ccache

        - name: Restore ccache
          if: runner.os != 'Windows'
          uses: actions/cache@v4
          with:
            path: ~/.ccache
            key: ccache-${{ runner.os }}-${{ github.sha }}
            restore-keys: |
              ccache-${{ runner.os }}-

        - name: Configure ccache
          if: runner.os != 'Windows'
          run: |
            ccache --max-size=2G
            ccache --zero-stats
            echo "CMAKE_C_COMPILER_LAUNCHER=ccache" >> $GITHUB_ENV
            echo "CMAKE_CXX_COMPILER_LAUNCHER=ccache" >> $GITHUB_ENV
  ```

- [ ] **Step 1.3: Add sccache for Windows**

  Before the Windows configure step, add:

  ```yaml
        - name: Setup sccache (Windows)
          if: runner.os == 'Windows'
          uses: mozilla-actions/sccache-action@v0.0.6

        - name: Configure sccache env (Windows)
          if: runner.os == 'Windows'
          shell: bash
          run: |
            echo "CMAKE_C_COMPILER_LAUNCHER=sccache" >> $GITHUB_ENV
            echo "CMAKE_CXX_COMPILER_LAUNCHER=sccache" >> $GITHUB_ENV
            echo "SCCACHE_GHA_ENABLED=true" >> $GITHUB_ENV
  ```

- [ ] **Step 1.4: Add test step after Build**

  After the `Build (Windows)` step (around current line 105), add:

  ```yaml
        - name: Test
          working-directory: build
          shell: bash
          run: ctest --output-on-failure --no-tests=ignore
  ```

  `--no-tests=ignore` makes `ctest` succeed when no tests are registered (current state on `main`).

- [ ] **Step 1.5: Add PR artifact upload**

  At the end of the job, add:

  ```yaml
        - name: Locate built binary
          if: github.event_name == 'pull_request'
          id: locate
          shell: bash
          run: |
            case "${{ runner.os }}" in
              Linux)   echo "binary=build/NereusSDR" >> "$GITHUB_OUTPUT" ;;
              macOS)   echo "binary=build/NereusSDR.app" >> "$GITHUB_OUTPUT" ;;
              Windows) echo "binary=build/NereusSDR.exe" >> "$GITHUB_OUTPUT" ;;
            esac

        - name: Upload PR build artifact
          if: github.event_name == 'pull_request'
          uses: actions/upload-artifact@v4
          with:
            name: NereusSDR-pr${{ github.event.number }}-${{ matrix.name }}
            path: ${{ steps.locate.outputs.binary }}
            retention-days: 7
            if-no-files-found: warn
  ```

- [ ] **Step 1.6: Print ccache stats at end of job**

  Append:

  ```yaml
        - name: ccache stats
          if: runner.os != 'Windows'
          run: ccache --show-stats
  ```

- [ ] **Step 1.7: Local YAML lint**

  Run:
  ```bash
  python3 -c 'import yaml; yaml.safe_load(open(".github/workflows/ci.yml"))' && echo "ci.yml OK"
  ```
  Expected: `ci.yml OK`. If it errors, fix the YAML.

- [ ] **Step 1.8: Commit**

  ```bash
  git add .github/workflows/ci.yml
  git commit -S -m "$(cat <<'EOF'
  ci: cache, test, artifact uploads, concurrency cancellation

  Phase 3N task 1. Adds ccache (Linux/macOS) + sccache (Windows),
  ctest after build, PR build artifact uploads (7-day retention),
  and concurrency-group cancellation on force-push.

  Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
  EOF
  )"
  ```

---

## Task 2: Create `release.yml` skeleton + `prepare` job

**Files:**
- Create: `.github/workflows/release.yml`

- [ ] **Step 2.1: Write the workflow skeleton with the `prepare` job**

  Create `.github/workflows/release.yml`:

  ```yaml
  name: Release

  on:
    push:
      tags: ['v*']
    workflow_dispatch:
      inputs:
        tag:
          description: 'Tag to (re-)build, e.g. v0.2.0'
          required: true

  permissions:
    contents: write

  concurrency:
    group: release-${{ github.ref }}
    cancel-in-progress: false

  jobs:
    prepare:
      runs-on: ubuntu-latest
      outputs:
        version: ${{ steps.meta.outputs.version }}
        tag: ${{ steps.meta.outputs.tag }}
        is_prerelease: ${{ steps.meta.outputs.is_prerelease }}
      steps:
        - uses: actions/checkout@v4
          with:
            fetch-depth: 0
            ref: ${{ github.event.inputs.tag || github.ref }}

        - name: Resolve tag and validate format
          id: meta
          shell: bash
          run: |
            set -euo pipefail
            TAG="${{ github.event.inputs.tag || github.ref_name }}"
            echo "Tag: $TAG"
            if ! [[ "$TAG" =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-[A-Za-z0-9.]+)?$ ]]; then
              echo "::error::Tag '$TAG' does not match vMAJOR.MINOR.PATCH[-suffix]"
              exit 1
            fi
            VERSION="${TAG#v}"
            echo "tag=$TAG" >> "$GITHUB_OUTPUT"
            echo "version=$VERSION" >> "$GITHUB_OUTPUT"
            if [[ "$TAG" =~ -(alpha|beta|rc) ]]; then
              echo "is_prerelease=true" >> "$GITHUB_OUTPUT"
            else
              echo "is_prerelease=false" >> "$GITHUB_OUTPUT"
            fi

        - name: Verify CMakeLists.txt version matches tag
          shell: bash
          run: |
            set -euo pipefail
            CMAKE_VER=$(grep -oE 'project\(NereusSDR VERSION [0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt | awk '{print $3}')
            TAG_VER="${{ steps.meta.outputs.version }}"
            BASE_VER="${TAG_VER%%-*}"
            if [[ "$CMAKE_VER" != "$BASE_VER" ]]; then
              echo "::error::CMakeLists.txt version ($CMAKE_VER) != tag version ($BASE_VER)"
              exit 1
            fi
            echo "Version match: $CMAKE_VER"

        - name: Extract release notes from CHANGELOG.md
          shell: bash
          run: |
            set -euo pipefail
            VERSION="${{ steps.meta.outputs.version }}"
            awk -v v="$VERSION" '
              $0 ~ "^## \\[" v "\\]" {flag=1; next}
              flag && /^## \[/ {flag=0}
              flag {print}
            ' CHANGELOG.md > release_notes.md
            if [[ ! -s release_notes.md ]]; then
              echo "::error::No CHANGELOG.md section found for version $VERSION"
              exit 1
            fi
            echo "----- release notes -----"
            cat release_notes.md
            echo "-------------------------"

        - name: Append release-notes template
          shell: bash
          run: |
            if [[ -f .github/release-notes-template.md ]]; then
              echo "" >> release_notes.md
              cat .github/release-notes-template.md >> release_notes.md
            fi

        - name: Upload release notes artifact
          uses: actions/upload-artifact@v4
          with:
            name: release-notes
            path: release_notes.md
            retention-days: 1
  ```

- [ ] **Step 2.2: Create release-notes template**

  Create `.github/release-notes-template.md`:

  ```markdown
  ---

  ## Installation

  ### Linux

  **x86_64:** download `NereusSDR-vX.Y.Z-x86_64.AppImage`
  **aarch64:** download `NereusSDR-vX.Y.Z-aarch64.AppImage`

  ```bash
  chmod +x NereusSDR-vX.Y.Z-*.AppImage
  ./NereusSDR-vX.Y.Z-*.AppImage
  ```

  ### macOS

  **Apple Silicon (M1/M2/M3/M4):** `NereusSDR-vX.Y.Z-macOS-apple-silicon.dmg`
  **Intel:** `NereusSDR-vX.Y.Z-macOS-intel.dmg`

  These builds are **ad-hoc signed** (no Apple Developer ID yet). On first launch:
  1. Open the DMG, drag NereusSDR to Applications.
  2. **Right-click** NereusSDR.app → **Open** → **Open** in the dialog.
  3. After the first launch, double-clicking works normally.

  ### Windows

  Two options — pick one:

  - **Installer (recommended):** `NereusSDR-vX.Y.Z-Windows-x64-setup.exe` — installs
    to `Program Files`, adds Start Menu shortcut, registers an uninstaller.
  - **Portable:** `NereusSDR-vX.Y.Z-Windows-x64-portable.zip` — extract and run
    `NereusSDR.exe`. No install footprint.

  Both are unsigned at the moment. SmartScreen may flag the binary on first run
  (no Authenticode signature yet); click **More info → Run anyway**.

  ## Verification

  All artifacts are GPG-signed by `KG4VCF`. To verify:
  ```bash
  gpg --keyserver keyserver.ubuntu.com --recv-keys KG4VCF
  gpg --verify SHA256SUMS.txt.asc SHA256SUMS.txt
  sha256sum -c SHA256SUMS.txt
  ```

  ## Reporting Issues

  This is an alpha build for debuggers/testers. Please report issues at
  <https://github.com/boydsoftprez/NereusSDR/issues> with: OS, radio model,
  protocol version, and log file (`~/.config/NereusSDR/nereussdr.log`).
  ```

- [ ] **Step 2.3: Lint the YAML**

  ```bash
  python3 -c 'import yaml; yaml.safe_load(open(".github/workflows/release.yml"))' && echo "release.yml OK"
  ```
  Expected: `release.yml OK`.

- [ ] **Step 2.4: Commit**

  ```bash
  git add .github/workflows/release.yml .github/release-notes-template.md
  git commit -S -m "$(cat <<'EOF'
  ci(release): add release.yml prepare job + notes template

  Phase 3N task 2. Tag-triggered workflow skeleton with prepare
  job: validates v<semver>[-suffix] format, verifies CMakeLists.txt
  version matches tag, extracts the matching CHANGELOG.md section
  into release_notes.md, and appends the install/verification
  template. Build jobs land in tasks 3-5.

  Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
  EOF
  )"
  ```

---

## Task 3: Add `build-linux` job to `release.yml`

**Files:**
- Modify: `.github/workflows/release.yml`

**Source of truth:** preserves the dual-arch matrix (x86_64 + aarch64) and the Qt 6.7.3 / aqtinstall logic from the existing `appimage.yml`.

- [ ] **Step 3.1: Append the `build-linux` job after `prepare`**

  Add to `.github/workflows/release.yml`:

  ```yaml
    build-linux:
      needs: prepare
      strategy:
        fail-fast: false
        matrix:
          include:
            - runner: ubuntu-22.04
              arch: x86_64
              use_aqt: true
            - runner: ubuntu-24.04-arm
              arch: aarch64
              use_aqt: false
      runs-on: ${{ matrix.runner }}
      steps:
        - uses: actions/checkout@v4
          with:
            ref: ${{ needs.prepare.outputs.tag }}

        - name: Install system dependencies
          run: |
            sudo apt-get update
            sudo apt-get install -y cmake ninja-build pkg-config ccache \
              libgl1-mesa-dev libxkbcommon-dev libxkbcommon-x11-0 \
              libxcb-cursor0 libxcb-icccm4 libxcb-keysyms1 libxcb-shape0 \
              libxcb-render-util0 libxcb-xinerama0 libxcb-randr0 libxcb-xfixes0 \
              gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
              gstreamer1.0-pulseaudio gstreamer1.0-gl \
              libgstreamer-gl1.0-0 libgstreamer-plugins-base1.0-0 \
              libfuse2 file desktop-file-utils \
              libfftw3-dev imagemagick

        - name: Install system Qt (ARM only)
          if: matrix.use_aqt == false
          run: |
            sudo apt-get install -y qt6-base-dev qt6-multimedia-dev \
              qt6-base-private-dev

        - name: Install Qt 6.7 via aqtinstall (x86_64)
          if: matrix.use_aqt == true
          run: |
            pip3 install aqtinstall
            aqt install-qt linux desktop 6.7.3 linux_gcc_64 \
              -m qtmultimedia \
              --outputdir ${{ github.workspace }}/Qt
            echo "Qt6_DIR=${{ github.workspace }}/Qt/6.7.3/gcc_64/lib/cmake/Qt6" >> $GITHUB_ENV
            echo "CMAKE_PREFIX_PATH=${{ github.workspace }}/Qt/6.7.3/gcc_64" >> $GITHUB_ENV
            echo "PATH=${{ github.workspace }}/Qt/6.7.3/gcc_64/bin:$PATH" >> $GITHUB_ENV
            echo "LD_LIBRARY_PATH=${{ github.workspace }}/Qt/6.7.3/gcc_64/lib:$LD_LIBRARY_PATH" >> $GITHUB_ENV

        - name: Restore ccache
          uses: actions/cache@v4
          with:
            path: ~/.ccache
            key: release-ccache-linux-${{ matrix.arch }}-${{ needs.prepare.outputs.tag }}
            restore-keys: |
              release-ccache-linux-${{ matrix.arch }}-

        - name: Configure
          run: |
            ccache --max-size=2G
            CMAKE_EXTRA=""
            if [ "${{ matrix.use_aqt }}" = "true" ]; then
              CMAKE_EXTRA="-DCMAKE_PREFIX_PATH=${{ github.workspace }}/Qt/6.7.3/gcc_64"
            fi
            cmake -B build -G Ninja \
              -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_INSTALL_PREFIX=/usr \
              -DCMAKE_C_COMPILER_LAUNCHER=ccache \
              -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
              $CMAKE_EXTRA

        - name: Build
          run: cmake --build build -j$(nproc)

        - name: Test
          working-directory: build
          run: ctest --output-on-failure --no-tests=ignore

        - name: Install to AppDir
          run: |
            DESTDIR=${{ github.workspace }}/AppDir cmake --install build

        - name: Download linuxdeploy + Qt plugin
          run: |
            wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${{ matrix.arch }}.AppImage
            wget -q https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${{ matrix.arch }}.AppImage
            chmod +x linuxdeploy-${{ matrix.arch }}.AppImage linuxdeploy-plugin-qt-${{ matrix.arch }}.AppImage

        - name: Build AppImage
          env:
            LDAI_OUTPUT: NereusSDR-${{ needs.prepare.outputs.version }}-${{ matrix.arch }}.AppImage
            QT_SELECT: qt6
            EXTRA_QT_PLUGINS: multimedia
          run: |
            if [ "${{ matrix.use_aqt }}" = "true" ]; then
              export QMAKE=${{ github.workspace }}/Qt/6.7.3/gcc_64/bin/qmake
            else
              export QMAKE=$(which qmake6 2>/dev/null || which qmake)
            fi
            mkdir -p AppDir/usr/share/applications
            cat > AppDir/usr/share/applications/NereusSDR.desktop << 'DESKTOP'
            [Desktop Entry]
            Type=Application
            Name=NereusSDR
            Exec=NereusSDR
            Icon=nereussdr
            Categories=HamRadio;Audio;
            Comment=Cross-platform OpenHPSDR SDR Console
            DESKTOP
            mkdir -p AppDir/usr/share/icons/hicolor/256x256/apps
            convert -size 256x256 xc:"#0f0f1a" -fill "#00b4d8" -gravity center \
              -pointsize 48 -annotate +0+0 "NSDR" \
              AppDir/usr/share/icons/hicolor/256x256/apps/nereussdr.png 2>/dev/null || \
            touch AppDir/usr/share/icons/hicolor/256x256/apps/nereussdr.png

            ./linuxdeploy-${{ matrix.arch }}.AppImage \
              --appdir AppDir \
              --plugin qt \
              --desktop-file AppDir/usr/share/applications/NereusSDR.desktop \
              --output appimage

        - name: Upload AppImage artifact
          uses: actions/upload-artifact@v4
          with:
            name: linux-${{ matrix.arch }}
            path: NereusSDR-*.AppImage
            retention-days: 7
  ```

- [ ] **Step 3.2: Lint and commit**

  ```bash
  python3 -c 'import yaml; yaml.safe_load(open(".github/workflows/release.yml"))' && echo OK
  git add .github/workflows/release.yml
  git commit -S -m "$(cat <<'EOF'
  ci(release): add build-linux job (x86_64 + aarch64 AppImage)

  Phase 3N task 3. Adapted from appimage.yml: dual-arch matrix,
  Qt 6.7.3 via aqtinstall on x86_64, system Qt6 on aarch64,
  linuxdeploy-plugin-qt for AppImage packaging. Adds ccache + ctest.

  Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
  EOF
  )"
  ```

---

## Task 4: Add `build-macos` job to `release.yml` (matrix: apple-silicon + intel)

**Files:**
- Modify: `.github/workflows/release.yml`

- [ ] **Step 4.1: Append the `build-macos` job with matrix**

  Add after `build-linux`:

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
            - runner: macos-13
              arch: x86_64
              dmg_suffix: macOS-intel
              deployment_target: "11.0"
      runs-on: ${{ matrix.runner }}
      steps:
        - uses: actions/checkout@v4
          with:
            ref: ${{ needs.prepare.outputs.tag }}

        - name: Install dependencies
          run: brew install qt@6 ninja create-dmg fftw ccache

        - name: Restore ccache
          uses: actions/cache@v4
          with:
            path: ~/Library/Caches/ccache
            key: release-ccache-macos-${{ matrix.arch }}-${{ needs.prepare.outputs.tag }}
            restore-keys: |
              release-ccache-macos-${{ matrix.arch }}-

        - name: Configure
          run: |
            ccache --max-size=2G
            cmake -B build -G Ninja \
              -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_OSX_DEPLOYMENT_TARGET="${{ matrix.deployment_target }}" \
              -DCMAKE_OSX_ARCHITECTURES="${{ matrix.arch }}" \
              -DCMAKE_PREFIX_PATH="$(brew --prefix)" \
              -DCMAKE_C_COMPILER_LAUNCHER=ccache \
              -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

        - name: Build
          run: cmake --build build -j$(sysctl -n hw.ncpu)

        - name: Test
          working-directory: build
          run: ctest --output-on-failure --no-tests=ignore

        - name: Bundle Qt frameworks
          run: |
            $(brew --prefix qt@6)/bin/macdeployqt build/NereusSDR.app \
              -verbose=1 -always-overwrite

        - name: Ad-hoc codesign
          run: |
            codesign --force --deep --sign - build/NereusSDR.app
            codesign --verify --verbose=2 build/NereusSDR.app

        - name: Create DMG
          run: |
            create-dmg \
              --volname "NereusSDR" \
              --window-pos 200 120 \
              --window-size 600 400 \
              --icon-size 100 \
              --icon "NereusSDR.app" 150 110 \
              --hide-extension "NereusSDR.app" \
              --app-drop-link 450 110 \
              "NereusSDR-${{ needs.prepare.outputs.version }}-${{ matrix.dmg_suffix }}.dmg" \
              "build/NereusSDR.app" \
            || true
            ls -la NereusSDR-*.dmg

        - name: Upload DMG artifact
          uses: actions/upload-artifact@v4
          with:
            name: macos-${{ matrix.arch }}
            path: NereusSDR-*.dmg
            retention-days: 7
  ```

  Notes:
  - `macos-13` is Apple's last Intel-capable runner. Apple has announced retirement; if it disappears before we reach 1.0, the Intel matrix entry can be dropped without restructuring.
  - `CMAKE_OSX_ARCHITECTURES` forces single-arch build per matrix entry (no fat binaries — keeps DMG sizes down and matches AetherSDR's split).
  - Intel uses an older deployment target (11.0 / Big Sur) since Intel users often run older macOS than Apple Silicon users.

- [ ] **Step 4.2: Lint and commit**

  ```bash
  python3 -c 'import yaml; yaml.safe_load(open(".github/workflows/release.yml"))' && echo OK
  git add .github/workflows/release.yml
  git commit -S -m "$(cat <<'EOF'
  ci(release): add build-macos job (ad-hoc signed DMG)

  Phase 3N task 4. Adapted from macos-dmg.yml: macos-15 (Apple
  Silicon), Homebrew Qt6, Ninja, macdeployqt, create-dmg.
  Adds ccache, ctest, and ad-hoc codesign step (Apple Developer
  ID gated to Phase 3N+1).

  Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
  EOF
  )"
  ```

---

## Task 5: Add `build-windows` job to `release.yml` (portable ZIP + NSIS installer)

**Files:**
- Create: `scripts/windows/installer.nsi`
- Modify: `.github/workflows/release.yml`

**Note:** ships **both** a portable ZIP and an NSIS-built `setup.exe` to mirror the AetherSDR release shape. Authenticode signing of the `setup.exe` is Phase 3N+1.

- [ ] **Step 5.0: Create the NSIS installer script**

  Create `scripts/windows/installer.nsi`:

  ```nsis
  ; NereusSDR NSIS installer script
  ; Builds: NereusSDR-vX.Y.Z-Windows-x64-setup.exe
  ; Invoked by .github/workflows/release.yml build-windows job.

  !include "MUI2.nsh"

  ;--- Variables passed in via /D on the makensis command line ---
  ; NSDR_VERSION   — full version string, e.g. 0.2.0
  ; NSDR_DEPLOYDIR — path to the windeployqt'd deploy/ directory
  ; NSDR_OUTFILE   — full output path for the .exe

  !ifndef NSDR_VERSION
    !define NSDR_VERSION "0.0.0"
  !endif
  !ifndef NSDR_DEPLOYDIR
    !define NSDR_DEPLOYDIR "deploy"
  !endif
  !ifndef NSDR_OUTFILE
    !define NSDR_OUTFILE "NereusSDR-setup.exe"
  !endif

  Name "NereusSDR ${NSDR_VERSION}"
  OutFile "${NSDR_OUTFILE}"
  Unicode True
  InstallDir "$PROGRAMFILES64\NereusSDR"
  InstallDirRegKey HKLM "Software\NereusSDR" "InstallDir"
  RequestExecutionLevel admin

  VIProductVersion "${NSDR_VERSION}.0"
  VIAddVersionKey "ProductName" "NereusSDR"
  VIAddVersionKey "FileDescription" "NereusSDR — cross-platform OpenHPSDR SDR console"
  VIAddVersionKey "FileVersion" "${NSDR_VERSION}"
  VIAddVersionKey "ProductVersion" "${NSDR_VERSION}"
  VIAddVersionKey "LegalCopyright" "© JJ Boyd ~KG4VCF"

  ;--- Modern UI pages ---
  !define MUI_ABORTWARNING
  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH

  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES

  !insertmacro MUI_LANGUAGE "English"

  Section "NereusSDR (required)" SecMain
    SectionIn RO
    SetOutPath "$INSTDIR"
    File /r "${NSDR_DEPLOYDIR}\*.*"

    WriteRegStr HKLM "Software\NereusSDR" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\NereusSDR" "Version" "${NSDR_VERSION}"

    ; Add/Remove Programs entry
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NereusSDR" \
                "DisplayName" "NereusSDR ${NSDR_VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NereusSDR" \
                "DisplayVersion" "${NSDR_VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NereusSDR" \
                "Publisher" "JJ Boyd ~KG4VCF"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NereusSDR" \
                "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NereusSDR" \
                "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NereusSDR" \
                "DisplayIcon" "$INSTDIR\NereusSDR.exe"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NereusSDR" \
                "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NereusSDR" \
                "NoRepair" 1

    WriteUninstaller "$INSTDIR\uninstall.exe"
  SectionEnd

  Section "Start Menu shortcut" SecStartMenu
    CreateDirectory "$SMPROGRAMS\NereusSDR"
    CreateShortcut "$SMPROGRAMS\NereusSDR\NereusSDR.lnk" "$INSTDIR\NereusSDR.exe"
    CreateShortcut "$SMPROGRAMS\NereusSDR\Uninstall NereusSDR.lnk" "$INSTDIR\uninstall.exe"
  SectionEnd

  Section "Desktop shortcut" SecDesktop
    CreateShortcut "$DESKTOP\NereusSDR.lnk" "$INSTDIR\NereusSDR.exe"
  SectionEnd

  ;--- Uninstaller ---
  Section "Uninstall"
    Delete "$DESKTOP\NereusSDR.lnk"
    Delete "$SMPROGRAMS\NereusSDR\NereusSDR.lnk"
    Delete "$SMPROGRAMS\NereusSDR\Uninstall NereusSDR.lnk"
    RMDir  "$SMPROGRAMS\NereusSDR"

    RMDir /r "$INSTDIR"

    DeleteRegKey HKLM "Software\NereusSDR"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NereusSDR"
  SectionEnd
  ```

- [ ] **Step 5.1: Append the `build-windows` job**

  Add after `build-macos`:

  ```yaml
    build-windows:
      needs: prepare
      runs-on: windows-latest
      steps:
        - uses: actions/checkout@v4
          with:
            ref: ${{ needs.prepare.outputs.tag }}

        - name: Set up MSVC
          uses: ilammy/msvc-dev-cmd@v1

        - name: Install Qt6
          uses: jurplel/install-qt-action@v4
          with:
            version: '6.7.*'
            host: 'windows'
            target: 'desktop'
            arch: 'win64_msvc2019_64'
            modules: 'qtmultimedia qtserialport qtwebsockets qtshadertools'
            cache: true

        - name: Install Ninja and NSIS
          run: |
            choco install ninja -y
            choco install nsis -y

        - name: Setup sccache
          uses: mozilla-actions/sccache-action@v0.0.6

        - name: Setup FFTW3
          shell: pwsh
          run: |
            $fftw_dir = "third_party/fftw3"
            New-Item -ItemType Directory -Force -Path "$fftw_dir/include", "$fftw_dir/lib", "$fftw_dir/bin"
            Invoke-WebRequest -Uri "https://www.fftw.org/fftw-3.3.5-dll64.zip" -OutFile fftw.zip
            Expand-Archive -Path fftw.zip -DestinationPath fftw-tmp
            Copy-Item fftw-tmp/fftw3.h "$fftw_dir/include/"
            Copy-Item fftw-tmp/libfftw3-3.dll "$fftw_dir/bin/"
            cd fftw-tmp
            lib /def:libfftw3-3.def /out:fftw3.lib /machine:x64
            cd ..
            Copy-Item fftw-tmp/fftw3.lib "$fftw_dir/lib/"

        - name: Configure
          shell: bash
          env:
            SCCACHE_GHA_ENABLED: "true"
          run: |
            cmake -B build -G Ninja \
              -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_C_COMPILER_LAUNCHER=sccache \
              -DCMAKE_CXX_COMPILER_LAUNCHER=sccache

        - name: Build
          shell: bash
          env:
            SCCACHE_GHA_ENABLED: "true"
          run: cmake --build build -j${NUMBER_OF_PROCESSORS}

        - name: Test
          working-directory: build
          shell: bash
          run: ctest --output-on-failure --no-tests=ignore

        - name: Deploy Qt
          shell: pwsh
          run: |
            mkdir deploy
            copy build\NereusSDR.exe deploy\
            windeployqt deploy\NereusSDR.exe --release --no-translations --no-system-d3d-compiler
            if (Test-Path "third_party\fftw3\bin\libfftw3-3.dll") {
              copy third_party\fftw3\bin\libfftw3-3.dll deploy\
            }

        - name: Create portable ZIP
          shell: pwsh
          run: |
            $name = "NereusSDR-${{ needs.prepare.outputs.version }}-Windows-x64-portable.zip"
            Compress-Archive -Path deploy\* -DestinationPath $name

        - name: Build NSIS installer
          shell: pwsh
          run: |
            $version = "${{ needs.prepare.outputs.version }}"
            $deploy  = (Resolve-Path deploy).Path
            $outfile = "NereusSDR-$version-Windows-x64-setup.exe"
            & "C:\Program Files (x86)\NSIS\makensis.exe" `
                /DNSDR_VERSION=$version `
                /DNSDR_DEPLOYDIR=$deploy `
                /DNSDR_OUTFILE=$outfile `
                scripts\windows\installer.nsi
            if (-not (Test-Path $outfile)) {
              Write-Error "NSIS did not produce $outfile"
              exit 1
            }
            Get-Item $outfile

        - name: Upload Windows artifacts
          uses: actions/upload-artifact@v4
          with:
            name: windows-x64
            path: |
              NereusSDR-*.zip
              NereusSDR-*-setup.exe
            retention-days: 7
  ```

- [ ] **Step 5.2: Lint and commit**

  ```bash
  python3 -c 'import yaml; yaml.safe_load(open(".github/workflows/release.yml"))' && echo OK
  git add .github/workflows/release.yml
  git commit -S -m "$(cat <<'EOF'
  ci(release): add build-windows job (portable ZIP)

  Phase 3N task 5. Adapted from windows-installer.yml: MSVC,
  Qt 6.7 via install-qt-action, sccache, FFTW3 prebuilt DLL,
  windeployqt, portable ZIP. NSIS installer + Authenticode
  signing deferred to Phase 3N+1.

  Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
  EOF
  )"
  ```

---

## Task 6: Add `sign-and-publish` job to `release.yml`

**Files:**
- Modify: `.github/workflows/release.yml`

**Source of truth:** adapted from existing `sign-release.yml`.

- [ ] **Step 6.1: Append the `sign-and-publish` job**

  ```yaml
    sign-and-publish:
      needs: [prepare, build-linux, build-macos, build-windows]
      runs-on: ubuntu-latest
      steps:
        - uses: actions/checkout@v4
          with:
            fetch-depth: 0
            ref: ${{ needs.prepare.outputs.tag }}

        - name: Download all build artifacts
          uses: actions/download-artifact@v4
          with:
            path: downloaded

        - name: Stage artifacts
          run: |
            mkdir -p artifacts
            find downloaded -type f \( \
              -name '*.AppImage' -o \
              -name '*.dmg' -o \
              -name '*.zip' -o \
              -name '*-setup.exe' -o \
              -name 'release_notes.md' \
            \) -exec cp {} artifacts/ \;
            ls -la artifacts/

        - name: Generate source tarball
          run: |
            VERSION="${{ needs.prepare.outputs.version }}"
            TAG="${{ needs.prepare.outputs.tag }}"
            git archive --format=tar.gz \
              --prefix="NereusSDR-${VERSION}/" \
              "${TAG}" > "artifacts/NereusSDR-${VERSION}-source.tar.gz"

        - name: Generate SHA256SUMS
          working-directory: artifacts
          run: |
            # exclude release_notes.md from sums (not a release artifact)
            sha256sum $(ls | grep -v '^release_notes.md$') > SHA256SUMS.txt
            cat SHA256SUMS.txt

        - name: Import GPG key
          env:
            GPG_PRIVATE_KEY: ${{ secrets.GPG_PRIVATE_KEY }}
            GPG_PASSPHRASE: ${{ secrets.GPG_PASSPHRASE }}
          run: |
            echo "$GPG_PRIVATE_KEY" | gpg --batch --import
            KEY_FP=$(gpg --list-secret-keys --with-colons | awk -F: '/^fpr/ {print $10; exit}')
            echo "${KEY_FP}:6:" | gpg --import-ownertrust
            gpg --list-secret-keys

        - name: Sign artifacts
          env:
            GPG_PASSPHRASE: ${{ secrets.GPG_PASSPHRASE }}
          working-directory: artifacts
          run: |
            for f in *.AppImage *.dmg *.zip *.exe *.tar.gz SHA256SUMS.txt; do
              [ -f "$f" ] || continue
              gpg --batch --yes --pinentry-mode loopback \
                --passphrase "$GPG_PASSPHRASE" \
                --detach-sign --armor "$f"
            done
            ls -la

        - name: Create draft GitHub Release
          env:
            GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}
          run: |
            VERSION="${{ needs.prepare.outputs.version }}"
            TAG="${{ needs.prepare.outputs.tag }}"
            PRERELEASE_FLAG=""
            if [[ "${{ needs.prepare.outputs.is_prerelease }}" == "true" ]]; then
              PRERELEASE_FLAG="--prerelease"
            fi
            # Delete existing draft for this tag if rerunning
            gh release delete "$TAG" --yes --cleanup-tag=false 2>/dev/null || true
            gh release create "$TAG" \
              --title "NereusSDR ${VERSION}" \
              --notes-file artifacts/release_notes.md \
              --draft \
              $PRERELEASE_FLAG \
              artifacts/*.AppImage \
              artifacts/*.dmg \
              artifacts/*.zip \
              artifacts/*-setup.exe \
              artifacts/*.tar.gz \
              artifacts/SHA256SUMS.txt \
              artifacts/*.asc

        - name: Print release URL
          env:
            GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}
          run: |
            URL=$(gh release view "${{ needs.prepare.outputs.tag }}" --json url -q .url)
            echo "Draft release: $URL"
            echo "## Draft release" >> "$GITHUB_STEP_SUMMARY"
            echo "$URL" >> "$GITHUB_STEP_SUMMARY"
  ```

- [ ] **Step 6.2: Lint and commit**

  ```bash
  python3 -c 'import yaml; yaml.safe_load(open(".github/workflows/release.yml"))' && echo OK
  git add .github/workflows/release.yml
  git commit -S -m "$(cat <<'EOF'
  ci(release): add sign-and-publish job

  Phase 3N task 6. Final job in release.yml: downloads all build
  artifacts, generates source tarball + SHA256SUMS, imports
  GPG_PRIVATE_KEY, detached-signs every artifact, and creates a
  draft GitHub Release. Re-runnable on the same tag (deletes
  existing draft first).

  Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
  EOF
  )"
  ```

---

## Task 7: End-to-end shakedown with disposable test tag

**Files:** none modified.

**Goal:** prove `release.yml` works on a real GitHub-hosted runner before deleting the old workflows. We use a `*-test*` tag so the future skill (Task 9) will refuse to operate on it.

- [ ] **Step 7.1: Push the feature branch**

  ```bash
  git push -u origin feature/phase3n-release-pipeline
  ```

- [ ] **Step 7.2: Open a draft PR for visibility**

  ```bash
  gh pr create --draft --title "Phase 3N: release pipeline + skill (WIP)" \
    --body "Draft. Tracking commits for the Phase 3N implementation. Will be marked ready after Task 13."
  ```

- [ ] **Step 7.3: Create a disposable test tag pointing at the current branch tip**

  ```bash
  git tag -a v0.0.1-test1 -m "Phase 3N shakedown — disposable" -s
  git push origin v0.0.1-test1
  ```

  Note: this tag's CMakeLists.txt version is `0.1.0` (current), which will fail the prepare-job version check intentionally. That's the first thing we test: **does it fail the way it should?**

- [ ] **Step 7.4: Watch the workflow fail in `prepare`**

  ```bash
  gh run watch
  ```
  Expected: `prepare` job fails on the "Verify CMakeLists.txt version matches tag" step. **Good** — fast-fail validation works.

- [ ] **Step 7.5: Delete the bad test tag**

  ```bash
  git push --delete origin v0.0.1-test1
  git tag -d v0.0.1-test1
  ```

- [ ] **Step 7.6: Bump CMakeLists.txt to 0.0.1 temporarily and add a stub CHANGELOG entry**

  Edit `CMakeLists.txt`:
  ```
  project(NereusSDR VERSION 0.0.1 LANGUAGES C CXX)
  ```

  Edit `CHANGELOG.md`, add at the top under `# Changelog`:
  ```markdown
  ## [0.0.1-test2] - 2026-04-12

  Phase 3N pipeline shakedown release. Not a real release.
  ```

  ```bash
  git add CMakeLists.txt CHANGELOG.md
  git commit -S -m "test: bump to 0.0.1 for Phase 3N shakedown (will revert)"
  git push
  ```

- [ ] **Step 7.7: Re-tag and push**

  ```bash
  git tag -a v0.0.1-test2 -m "Phase 3N shakedown 2" -s
  git push origin v0.0.1-test2
  gh run watch
  ```

  Expected: workflow runs all 4 jobs to completion (build-linux × 2 archs, build-macos × 2 archs, build-windows × 1, then sign-and-publish). May take 30–60 min on cold caches. A **draft prerelease** appears at <https://github.com/boydsoftprez/NereusSDR/releases> with these artifacts:

  - `NereusSDR-0.0.1-test2-x86_64.AppImage` (+ `.asc`)
  - `NereusSDR-0.0.1-test2-aarch64.AppImage` (+ `.asc`)
  - `NereusSDR-0.0.1-test2-macOS-apple-silicon.dmg` (+ `.asc`)
  - `NereusSDR-0.0.1-test2-macOS-intel.dmg` (+ `.asc`)
  - `NereusSDR-0.0.1-test2-Windows-x64-portable.zip` (+ `.asc`)
  - `NereusSDR-0.0.1-test2-Windows-x64-setup.exe` (+ `.asc`)
  - `NereusSDR-0.0.1-test2-source.tar.gz` (+ `.asc`)
  - `SHA256SUMS.txt` (+ `.asc`)

- [ ] **Step 7.8: Download every artifact from the draft and smoke test**

  ```bash
  mkdir /tmp/nsdr-test && cd /tmp/nsdr-test
  gh release download v0.0.1-test2 --repo boydsoftprez/NereusSDR
  gpg --verify SHA256SUMS.txt.asc SHA256SUMS.txt
  sha256sum -c SHA256SUMS.txt
  ```

  Expected: GPG signature `Good signature`, all sha256sums match.

  Then on each platform locally:
  - **Linux x86_64:** `chmod +x NereusSDR-0.0.1-test2-x86_64.AppImage && ./NereusSDR-0.0.1-test2-x86_64.AppImage` — launches, About dialog shows `0.0.1`.
  - **Linux aarch64:** same on a Pi or ARM box if available; skip if no hardware.
  - **macOS Apple Silicon:** open the DMG, drag to Applications, right-click → Open. Launches.
  - **macOS Intel:** same on an Intel Mac if available; skip if no hardware.
  - **Windows portable:** unzip, run `NereusSDR.exe`. Launches.
  - **Windows installer:** run `NereusSDR-0.0.1-test2-Windows-x64-setup.exe`, click through the wizard, verify Start Menu shortcut works, verify Add/Remove Programs entry appears, then uninstall and verify clean removal.

- [ ] **Step 7.9: Delete the draft release and the test tag**

  ```bash
  gh release delete v0.0.1-test2 --yes --cleanup-tag=false
  git push --delete origin v0.0.1-test2
  git tag -d v0.0.1-test2
  ```

- [ ] **Step 7.10: Revert the version bump**

  ```bash
  git revert HEAD~1   # the "test: bump to 0.0.1" commit (HEAD~0 is the test tag commit which is gone)
  # Actually: revert by hand since the test commit may not be HEAD anymore
  # Reset CMakeLists to 0.1.0, drop the [0.0.1-test2] CHANGELOG section
  ```

  Manual: edit `CMakeLists.txt` back to `VERSION 0.1.0` and delete the `## [0.0.1-test2]` block from `CHANGELOG.md`. Then:

  ```bash
  git add CMakeLists.txt CHANGELOG.md
  git commit -S -m "test: revert Phase 3N shakedown version bump"
  git push
  ```

- [ ] **Step 7.11: STOP — get user signoff before Task 8**

  The old workflows are about to be deleted. Confirm with the user:
  > "Phase 3N shakedown passed: prepare-job validation worked, all 3 platform builds succeeded, GPG signatures verified, all 3 binaries launched. Ready to delete the legacy workflows (appimage, macos-dmg, windows-installer, sign-release)?"

  Wait for explicit yes.

---

## Task 8: Delete legacy workflows

**Files:**
- Delete: `.github/workflows/appimage.yml`
- Delete: `.github/workflows/macos-dmg.yml`
- Delete: `.github/workflows/windows-installer.yml`
- Delete: `.github/workflows/sign-release.yml`

- [ ] **Step 8.1: Remove the four files**

  ```bash
  git rm .github/workflows/appimage.yml \
         .github/workflows/macos-dmg.yml \
         .github/workflows/windows-installer.yml \
         .github/workflows/sign-release.yml
  ```

- [ ] **Step 8.2: Commit**

  ```bash
  git commit -S -m "$(cat <<'EOF'
  ci: remove legacy release workflows

  Phase 3N task 8. Replaced by the consolidated release.yml
  (see docs/architecture/2026-04-12-phase3n-release-pipeline-design.md).
  Shakedown passed against v0.0.1-test2 in Task 7.

  Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
  EOF
  )"
  git push
  ```

---

## Task 9: Create the `release` skill (`SKILL.md`)

**Files:**
- Create: `~/.claude/skills/release/SKILL.md`

**Note:** the skill lives in the user's `~/.claude/` directory, OUTSIDE the NereusSDR repo. Do not commit it to the repo.

- [ ] **Step 9.1: Make the skill directory**

  ```bash
  mkdir -p ~/.claude/skills/release/bin
  ```

- [ ] **Step 9.2: Write SKILL.md**

  Create `~/.claude/skills/release/SKILL.md`:

  ```markdown
  ---
  name: release
  description: Cut a NereusSDR (or compatible repo) release. Drives the version bump, changelog draft, GPG-signed commit/tag, push, watches release.yml, and opens the draft GitHub Release in a browser. Use when the user types /release, /release patch, /release minor, /release major, or asks to "cut a release", "ship a release", "tag a release".
  ---

  # release skill

  You are driving a release. The reproducible engine is `.github/workflows/release.yml` in the target repo; this skill is the human-facing front door.

  ## Eligibility check

  This skill only runs in repos that have `.github/workflows/release.yml` AND a `CHANGELOG.md` AND a `CMakeLists.txt` with a `project(... VERSION X.Y.Z)` line. If any are missing, abort with a clear message and do nothing.

  ## Step sequence (do these in order, do NOT skip)

  ### 1. Pre-flight

  Run the helper:
  ```bash
  bash ~/.claude/skills/release/bin/release.sh preflight
  ```

  This checks: clean tree, on `main`, up to date with `origin/main`, `gh`/`gpg`/`git` available, last `ci.yml` run on `main` was green, no existing tag for the version we're about to create. The helper exits non-zero on any failure with a specific message — relay it to the user and stop.

  ### 2. Version bump

  Show the user `git log $(git describe --tags --abbrev=0)..HEAD --oneline`. If the user invoked `/release patch|minor|major`, use that. Otherwise ask: **"What kind of release? (major/minor/patch)"** Compute the new version. Refuse to operate on tags containing `-test` (those are reserved for workflow shakedown).

  ### 3. Changelog draft

  Run:
  ```bash
  bash ~/.claude/skills/release/bin/release.sh draft-changelog <new-version>
  ```

  The helper parses commits since the last tag, groups them by Conventional Commit prefix (`feat:`, `fix:`, `docs:`, `refactor:`, `test:`, `chore:`, `ci:`, plus an "Other" bucket), prepends a `## [X.Y.Z] - YYYY-MM-DD` section to `CHANGELOG.md`, and opens it in `$EDITOR` (fallback `nano`). After the editor exits, re-read CHANGELOG.md and verify the new section is non-empty. If empty, abort and instruct the user to fix.

  ### 4. Doc sweep

  Update version references:
  - `CMakeLists.txt`: `project(NereusSDR VERSION X.Y.Z)` line
  - `README.md`: any "Current version" line
  - `docs/MASTER-PLAN.md`: any version line

  Show each diff to the user before staging. If any other file contains the old version string, list it and tell the user to update manually — do NOT auto-edit.

  ### 5. Commit and tag

  ```bash
  git add CMakeLists.txt CHANGELOG.md README.md docs/MASTER-PLAN.md
  git commit -S -m "release: v X.Y.Z

  Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
  git tag -s vX.Y.Z -m "NereusSDR vX.Y.Z"
  git show --stat HEAD
  git show vX.Y.Z
  ```

  Show both to the user. Ask: **"Push and trigger release.yml? (y/N)"** Default no. Wait for explicit y.

  ### 6. Push

  ```bash
  git push origin main
  git push origin vX.Y.Z
  ```

  ### 7. Watch

  ```bash
  gh run watch
  ```

  Stream until completion. On any failure: stop, fetch the failing job's log tail with `gh run view <id> --log-failed`, print the last 50 lines, and print BOTH recovery options:

  > **Recovery options:**
  > 1. Re-run (recommended for transient failures): `gh run rerun <run-id>`
  > 2. Abandon and fix forward (destructive — only if you know the tag must die): `git push --delete origin vX.Y.Z && git tag -d vX.Y.Z`
  >
  > The skill will NOT run option 2 itself.

  Exit non-zero. Do NOT continue to step 8.

  ### 8. Open the draft release

  ```bash
  URL=$(gh release view vX.Y.Z --json url -q .url)
  echo "Draft release: $URL"
  open "$URL"   # macOS; use xdg-open on Linux
  ```

  Tell the user: **"Draft release ready. Review artifacts, edit notes if needed, click Publish when satisfied. The skill does not publish for you."**

  ## Dry-run mode

  If invoked with `--dry-run`, do steps 1–4 but skip 5–8. Print what *would* happen at each skipped step. Make no commits, no tags, no pushes, no GH releases.

  ## What this skill does NOT do

  - Publish the GitHub release (human gate)
  - Delete tags or revert commits on failure (destructive)
  - Push to remotes other than `origin`
  - Run on any branch other than `main`
  - Operate on `*-test*` tags
  - Skip pre-flight checks even if the user says "just push it"
  ```

- [ ] **Step 9.3: Smoke-check the SKILL.md is loadable**

  ```bash
  ls -la ~/.claude/skills/release/SKILL.md
  head -3 ~/.claude/skills/release/SKILL.md
  ```
  Expected: file exists, frontmatter shows `name: release`.

---

## Task 10: Create the `release.sh` helper

**Files:**
- Create: `~/.claude/skills/release/bin/release.sh`

- [ ] **Step 10.1: Write the helper**

  Create `~/.claude/skills/release/bin/release.sh`:

  ```bash
  #!/usr/bin/env bash
  # release.sh — deterministic helper for the /release skill
  # subcommands: preflight, draft-changelog
  set -euo pipefail

  CMD="${1:-}"
  shift || true

  err() { echo "release.sh: $*" >&2; exit 1; }

  preflight() {
    [[ -f .github/workflows/release.yml ]] || err "no .github/workflows/release.yml in $(pwd)"
    [[ -f CHANGELOG.md ]] || err "no CHANGELOG.md"
    [[ -f CMakeLists.txt ]] || err "no CMakeLists.txt"

    command -v gh >/dev/null || err "gh CLI not installed"
    command -v gpg >/dev/null || err "gpg not installed"
    command -v git >/dev/null || err "git not installed"

    # Clean tree
    if [[ -n "$(git status --porcelain)" ]]; then
      git status
      err "working tree is not clean"
    fi

    # On main
    BRANCH="$(git rev-parse --abbrev-ref HEAD)"
    [[ "$BRANCH" == "main" ]] || err "not on main (currently: $BRANCH)"

    # Up to date with origin/main
    git fetch origin main --quiet
    LOCAL="$(git rev-parse main)"
    REMOTE="$(git rev-parse origin/main)"
    [[ "$LOCAL" == "$REMOTE" ]] || err "main is not up to date with origin/main (local=$LOCAL remote=$REMOTE)"

    # Last ci.yml run on main is green
    LAST_CI="$(gh run list --workflow ci.yml --branch main --limit 1 --json conclusion -q '.[0].conclusion' || echo unknown)"
    [[ "$LAST_CI" == "success" ]] || err "last ci.yml run on main is '$LAST_CI', not 'success'"

    # GPG key available
    gpg --list-secret-keys >/dev/null 2>&1 || err "no GPG secret key available"

    echo "preflight OK"
  }

  draft_changelog() {
    local new_version="${1:-}"
    [[ -n "$new_version" ]] || err "draft-changelog requires <version> arg"

    local last_tag
    last_tag="$(git describe --tags --abbrev=0 2>/dev/null || echo "")"
    local range
    if [[ -n "$last_tag" ]]; then
      range="${last_tag}..HEAD"
    else
      range="HEAD"
    fi

    local today
    today="$(date +%Y-%m-%d)"
    local tmp
    tmp="$(mktemp)"

    {
      echo "## [${new_version}] - ${today}"
      echo
      echo "### Features"
      git log "$range" --no-merges --pretty='%s' | grep -E '^feat(\(.+\))?:' | sed 's/^/- /' || echo "- (none)"
      echo
      echo "### Fixes"
      git log "$range" --no-merges --pretty='%s' | grep -E '^fix(\(.+\))?:' | sed 's/^/- /' || echo "- (none)"
      echo
      echo "### Docs"
      git log "$range" --no-merges --pretty='%s' | grep -E '^docs(\(.+\))?:' | sed 's/^/- /' || echo "- (none)"
      echo
      echo "### CI / Build"
      git log "$range" --no-merges --pretty='%s' | grep -E '^(ci|build|chore)(\(.+\))?:' | sed 's/^/- /' || echo "- (none)"
      echo
      echo "### Other"
      git log "$range" --no-merges --pretty='%s' | grep -vE '^(feat|fix|docs|ci|build|chore|test|refactor)(\(.+\))?:' | sed 's/^/- /' || echo "- (none)"
      echo
      echo "### Tests"
      git log "$range" --no-merges --pretty='%s' | grep -E '^test(\(.+\))?:' | sed 's/^/- /' || echo "- (none)"
      echo
      echo "### Refactors"
      git log "$range" --no-merges --pretty='%s' | grep -E '^refactor(\(.+\))?:' | sed 's/^/- /' || echo "- (none)"
      echo
    } > "$tmp"

    # Prepend the new section to CHANGELOG.md after the "# Changelog" line
    awk -v insert="$tmp" '
      /^# Changelog/ && !done {
        print
        print ""
        while ((getline line < insert) > 0) print line
        done = 1
        next
      }
      { print }
    ' CHANGELOG.md > CHANGELOG.md.new
    mv CHANGELOG.md.new CHANGELOG.md
    rm -f "$tmp"

    "${EDITOR:-nano}" CHANGELOG.md

    # Verify the new section still exists and is non-empty
    if ! grep -q "^## \[${new_version}\]" CHANGELOG.md; then
      err "after edit, CHANGELOG.md no longer contains a section for ${new_version}"
    fi
    echo "draft-changelog OK"
  }

  case "$CMD" in
    preflight)        preflight "$@" ;;
    draft-changelog)  draft_changelog "$@" ;;
    *)                err "unknown subcommand: $CMD (try: preflight, draft-changelog)" ;;
  esac
  ```

- [ ] **Step 10.2: Make executable**

  ```bash
  chmod +x ~/.claude/skills/release/bin/release.sh
  ```

- [ ] **Step 10.3: Syntax check**

  ```bash
  bash -n ~/.claude/skills/release/bin/release.sh && echo "syntax OK"
  ```
  Expected: `syntax OK`.

- [ ] **Step 10.4: Smoke test the unknown subcommand**

  ```bash
  ~/.claude/skills/release/bin/release.sh nope 2>&1 || true
  ```
  Expected: `release.sh: unknown subcommand: nope (try: preflight, draft-changelog)`.

---

## Task 11: Test the release skill in dry-run mode

**Files:** none modified.

- [ ] **Step 11.1: Run preflight against the NereusSDR repo**

  ```bash
  cd /Users/j.j.boyd/NereusSDR
  ~/.claude/skills/release/bin/release.sh preflight
  ```

  Expected: either `preflight OK`, OR a specific failure (likely "working tree is not clean" because we're on the feature branch). Both are acceptable — they prove the helper works.

- [ ] **Step 11.2: Run draft-changelog against a temporary scratch copy**

  ```bash
  cp CHANGELOG.md /tmp/CHANGELOG.md.bak
  EDITOR=true ~/.claude/skills/release/bin/release.sh draft-changelog 9.9.9-test
  head -40 CHANGELOG.md
  ```

  Expected: a `## [9.9.9-test] - YYYY-MM-DD` section appears at the top with feature/fix/docs/etc. buckets populated from recent commits. `EDITOR=true` makes the editor a no-op so the script proceeds without UI.

- [ ] **Step 11.3: Restore CHANGELOG**

  ```bash
  cp /tmp/CHANGELOG.md.bak CHANGELOG.md
  rm /tmp/CHANGELOG.md.bak
  git status   # should show CHANGELOG.md unchanged
  ```

---

## Task 12: README "Releases & Installation" section

**Files:**
- Modify: `README.md`

- [ ] **Step 12.1: Read the current README**

  ```bash
  head -60 README.md
  ```
  Identify where to insert the new section — after the project description, before "Build from source" (or wherever fits the existing structure).

- [ ] **Step 12.2: Insert the section**

  Add this block at the appropriate location:

  ```markdown
  ## Releases & Installation

  Pre-built binaries for Linux (AppImage, x86_64 + aarch64), macOS (DMG, Apple
  Silicon + Intel), and Windows (NSIS installer + portable ZIP, x64) are
  published as GitHub Releases:

  **<https://github.com/boydsoftprez/NereusSDR/releases>**

  All artifacts are GPG-signed (`KG4VCF`) via `SHA256SUMS.txt.asc`. To verify:

  ```bash
  gpg --keyserver keyserver.ubuntu.com --recv-keys KG4VCF
  gpg --verify SHA256SUMS.txt.asc SHA256SUMS.txt
  sha256sum -c SHA256SUMS.txt
  ```

  > **Alpha builds:** until Apple Developer ID and Authenticode certificates
  > are obtained, macOS users will need to right-click → Open the DMG on first
  > launch, and Windows users will see a SmartScreen warning to click through.
  > Linux is unaffected. See the per-release notes for details.
  ```

- [ ] **Step 12.3: Commit**

  ```bash
  git add README.md
  git commit -S -m "$(cat <<'EOF'
  docs(readme): add Releases & Installation section

  Phase 3N task 12. Points users at GitHub Releases, documents
  GPG verification with KG4VCF, and explains the macOS/Windows
  warning click-through pending Phase 3N+1 signing.

  Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
  EOF
  )"
  git push
  ```

---

## Task 13: One-time GitHub setup (manual checklist for the user)

**Files:** none. This task is a checklist printed for the human to perform via the GitHub web UI.

- [ ] **Step 13.1: Print the checklist for the user**

  Tell the user verbatim:

  > Phase 3N is code-complete. Two manual GitHub settings still need to be
  > configured (one-time, no code changes):
  >
  > **A. Branch protection on `main`:**
  > 1. Go to <https://github.com/boydsoftprez/NereusSDR/settings/branches>
  > 2. Edit (or add) the rule for `main`
  > 3. Enable: **Require a pull request before merging** → Require approvals: 0 (solo dev)
  > 4. Enable: **Require status checks to pass before merging** → search for and add:
  >    - `Build (Linux x64)`
  >    - `Build (macOS Apple Silicon)`
  >    - `Build (Windows x64)`
  > 5. Enable: **Require branches to be up to date before merging**
  > 6. Enable: **Require signed commits**
  > 7. Save
  >
  > **B. Verify required secrets exist:**
  > ```
  > gh secret list --repo boydsoftprez/NereusSDR
  > ```
  > Expected: `GPG_PRIVATE_KEY` and `GPG_PASSPHRASE` both present.
  >
  > Tell me when both are done and I'll mark the PR ready for merge.

- [ ] **Step 13.2: Wait for user confirmation, then mark PR ready**

  After the user confirms:

  ```bash
  gh pr ready
  ```

- [ ] **Step 13.3: Update CLAUDE.md to flip Phase 3N to Complete**

  Read the current "Current Phase" table in CLAUDE.md and add:

  ```
  | **3N: Packaging** | **release.yml + /release skill, GPG-signed alpha builds for Linux/macOS/Windows** | **Complete** |
  ```

  Also update the "Current Phase:" line if appropriate.

  ```bash
  git add CLAUDE.md
  git commit -S -m "$(cat <<'EOF'
  docs(claude): flip Phase 3N to Complete

  Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
  EOF
  )"
  git push
  ```

- [ ] **Step 13.4: Hand off**

  Tell the user:
  > Phase 3N implementation complete. The PR is ready for your review and
  > merge. After merge, you can cut the first real alpha with `/release patch`
  > (which will produce v0.1.1) or `/release minor` (v0.2.0) — your call on the
  > version. The skill will walk you through the rest.

---

## Appendix — Manual smoke checklist (for every real release)

Append to release notes once cut, run by hand:

- [ ] Linux x86_64 AppImage launches; About dialog shows correct version
- [ ] Linux aarch64 AppImage launches on a Pi or ARM box (or skip if no hardware)
- [ ] macOS Apple Silicon DMG mounts; right-click → Open works first time; About dialog correct
- [ ] macOS Intel DMG mounts and launches on an Intel Mac (or skip if no hardware)
- [ ] Windows portable ZIP extracts; `NereusSDR.exe` runs; SmartScreen click-through works
- [ ] Windows installer runs; Start Menu shortcut created; About dialog correct; uninstaller cleanly removes everything
- [ ] On at least one platform: connect to a real radio, verify spectrum renders
- [ ] On at least one platform: verify settings file persists across runs
- [ ] `gpg --verify SHA256SUMS.txt.asc SHA256SUMS.txt` returns Good signature
- [ ] `sha256sum -c SHA256SUMS.txt` shows OK for every file
