# Phase 3N — Release Pipeline & `release` Skill (Design)

**Status:** Approved (design), pending implementation plan
**Date:** 2026-04-12
**Author:** JJ Boyd ~KG4VCF (with Claude Code)
**Phase:** 3N — Packaging
**Supersedes:** ad-hoc `appimage.yml` / `macos-dmg.yml` / `windows-installer.yml` / `sign-release.yml` chain

---

## 1. Goals

1. **One reproducible release path.** Pushing a `vX.Y.Z` tag — by hand or via the
   skill — produces the same draft GitHub Release every time.
2. **PR-time CI on every platform.** Every pull request builds and tests on
   Linux, macOS, and Windows, and uploads downloadable build artifacts the
   author can smoke-test.
3. **A `release` skill** that drives the version bump, changelog, commit, tag,
   push, and watches the release workflow — ending by opening the draft
   release in a browser for human review.
4. **Ship alpha builds now without paid code-signing certs**, while keeping a
   real trust anchor (GPG-signed `SHA256SUMS.txt` with key `KG4VCF`).
5. **A clean upgrade path to signed/notarized releases** when Apple Developer
   ID and Azure Trusted Signing are obtained — captured as Phase 3N+1, not
   half-implemented now.

## 2. Non-Goals

- Automated UI / integration tests against real radios
- In-app auto-update / update channels
- Telemetry or crash reporting
- Distribution channels beyond GitHub Releases (no Homebrew tap, winget,
  Flatpak, Snap, AUR) — defer until post-1.0
- Release branches (`release/*`) — releases cut from `main` only, until
  asked for
- Rollback automation — leave failed tags in place; fix forward

---

## 3. Architecture Overview

Three layers with clean separation:

```
┌─────────────────────────────────────────────────────────┐
│  release skill  (~/.claude/skills/release/SKILL.md)     │
│    human-facing front door: bump, changelog, tag, push  │
└──────────────────────┬──────────────────────────────────┘
                       │ git push origin vX.Y.Z
                       ▼
┌─────────────────────────────────────────────────────────┐
│  release.yml  (.github/workflows/release.yml)           │
│    tag-triggered, reproducible build engine             │
│    prepare → build×3 → sign-and-publish                 │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  ci.yml  (.github/workflows/ci.yml)                     │
│    PR-triggered, build + test + cache + upload          │
│    independent of releases                              │
└─────────────────────────────────────────────────────────┘
```

### 3.1 Workflow consolidation

The following existing workflows are **folded into `release.yml`** and
deleted from the repo:

- `appimage.yml` → `build-linux` job
- `macos-dmg.yml` → `build-macos` job
- `windows-installer.yml` → `build-windows` job
- `sign-release.yml` → `sign-and-publish` job

The current `workflow_run` chain (sign-release waits on appimage + windows
to complete) is fragile, hard to debug, and means a release is never
atomic. One workflow with `needs:` between jobs gives atomicity, a single
status check, and one log location.

### 3.2 Workflows that remain separate

- `ci.yml` — PR builds. Different trigger, different purpose.
- `codeql.yml` — security scan, independent cadence.
- `docker-ci-image.yml` — base image rebuild, independent.

---

## 4. `release.yml` — Job Graph

```
              ┌─ build-linux   (ubuntu-24.04   → .AppImage)
prepare ──────┼─ build-macos   (macos-15       → .dmg, ad-hoc signed)
              └─ build-windows (windows-latest → .exe installer, unsigned)
                         │
                         ▼
                  sign-and-publish
                  (GPG SHA256SUMS.txt.asc, draft GH Release)
```

### 4.1 Trigger

```yaml
on:
  push:
    tags: ['v*']
  workflow_dispatch:
    inputs:
      tag:
        description: 'Tag to (re-)build, e.g. v0.2.0'
        required: true
```

`workflow_dispatch` lets you re-run a release for an existing tag without
deleting and re-pushing it.

### 4.2 `prepare` job

Runs once on `ubuntu-latest`, ~10 seconds. Outputs consumed by all build jobs.

**Steps:**
1. `actions/checkout@v4` with `fetch-depth: 0` (need full history for changelog).
2. **Validate tag format:** regex `^v[0-9]+\.[0-9]+\.[0-9]+(-[A-Za-z0-9.]+)?$`.
   Fail with a clear message if it doesn't match.
3. **Extract version** from tag (strip leading `v`).
4. **Verify CMakeLists.txt version matches tag.** Grep `project(NereusSDR
   VERSION X.Y.Z)`. Fail fast with a diff if they don't match — prevents
   shipping mismatched binaries.
5. **Extract release notes** from `CHANGELOG.md`: the section bounded by
   `## [X.Y.Z]` and the next `## [`. Fail if missing. Write to
   `release_notes.md` artifact.
6. **Detect prerelease** from tag suffix (`-alpha`, `-beta`, `-rc`) and set
   output `is_prerelease`. The workflow's prerelease decision is based
   purely on tag suffix; policy about whether `0.x` should always be
   prerelease lives in the skill (§6.4), so a human can override it by
   choosing the tag name.

**Outputs:** `version`, `is_prerelease`, `release_notes` (artifact).

### 4.3 `build-linux` job

`runs-on: ubuntu-24.04`. Mirrors the existing `appimage.yml` build with
caching added.

**Steps (high level):**
1. Checkout.
2. Restore caches: ccache, vcpkg (if used), Qt install dir.
3. Install system Qt6 + build deps via `apt`.
4. Configure CMake `Release`, build with Ninja.
5. Run unit tests (`ctest --output-on-failure`). **Test failure blocks the
   release.**
6. Package as AppImage via `linuxdeployqt` (or `linuxdeploy` — TBD by plan).
7. Upload `NereusSDR-${version}-linux-x86_64.AppImage` as job artifact.

### 4.4 `build-macos` job

`runs-on: macos-15` (Apple Silicon). Mirrors `macos-dmg.yml` plus caching.

**Steps:**
1. Checkout.
2. Restore caches: ccache, Homebrew packages.
3. `brew install qt@6 fftw ninja` (or use cached install).
4. Configure CMake `Release`, build with Ninja.
5. Run unit tests.
6. `macdeployqt` to bundle Qt frameworks into the `.app`.
7. **Ad-hoc codesign:** `codesign --force --deep --sign - NereusSDR.app`.
   Stable across runs and required for the binary to launch on Apple Silicon
   at all. Ad-hoc identity (`-`) means Gatekeeper will require right-click →
   Open the first time per user — documented in release notes.
8. Build DMG via `create-dmg` or `hdiutil`.
9. Upload `NereusSDR-${version}-macos-arm64.dmg` as job artifact.

**Phase 3N+1 hook:** when a real Developer ID is available, steps 7 and 9
get a `codesign --sign "Developer ID Application: ..."` and a `notarytool
submit --wait --staple` step. See §10.

### 4.5 `build-windows` job

`runs-on: windows-latest`. Mirrors `windows-installer.yml` plus caching.

**Steps:**
1. Checkout.
2. Restore caches: ccache (via `ccache-action` Windows port) or `sccache`,
   Qt install dir, vcpkg cache.
3. Install Qt6 via `jurplel/install-qt-action`.
4. Configure CMake `Release` with the bundled FFTW DLL from
   `third_party/fftw3/`.
5. Build with Ninja.
6. Run unit tests.
7. `windeployqt` against the built `.exe`.
8. Build NSIS installer via `makensis` against `scripts/windows/installer.nsi`
   (script to be created during implementation — see §8.1).
9. Upload `NereusSDR-${version}-windows-x64.exe` as job artifact.

**Phase 3N+1 hook:** when Azure Trusted Signing is configured, an
`azuresigntool sign ...` step is inserted between (8) and (9). See §10.

### 4.6 `sign-and-publish` job

`runs-on: ubuntu-latest`. `needs: [build-linux, build-macos, build-windows]`.

**Steps:**
1. Download all build artifacts to `artifacts/`.
2. Generate source tarball: `git archive --format=tar.gz
   --prefix=NereusSDR-${version}/ ${tag} > artifacts/NereusSDR-${version}-source.tar.gz`.
3. Generate `artifacts/SHA256SUMS.txt` covering every file in `artifacts/`.
4. Import GPG key from secrets (`GPG_PRIVATE_KEY`, `GPG_PASSPHRASE`).
   Verifies fingerprint against an expected value (`KG4VCF`) baked into the
   workflow.
5. **Detached-sign every artifact** with `gpg --detach-sign --armor` →
   `*.asc` siblings, including `SHA256SUMS.txt.asc`.
6. **Create draft GitHub Release** via `gh release create`:
   - Tag: `${tag}`
   - Title: `NereusSDR ${version}`
   - Body: contents of `release_notes.md`
   - `--draft` (always)
   - `--prerelease` if `is_prerelease=true`
   - All artifacts attached
7. Output the draft release URL to the workflow summary.

**Why draft, not auto-publish:** one human checkpoint between "build
succeeded" and "users see it." The `release` skill opens the URL
automatically; you review and click Publish.

---

## 5. `ci.yml` — Tightening

The existing `ci.yml` already builds 3 platforms on PR. Changes:

1. **Run tests** (`ctest --output-on-failure`) on every platform after build.
   Verify tests already run; if not, add.
2. **Caching** per platform: ccache on all 3, Qt install dir, system package
   caches where applicable. Target: warm CI < 5 min, cold CI ~15 min.
3. **Upload PR artifacts** — each job uploads its unsigned binary
   (`.AppImage` / `.dmg` / `.exe`) with `actions/upload-artifact@v4`,
   retention 7 days. Lets reviewers smoke-test a PR without checking it out.
4. **Concurrency group** to cancel stale runs on force-push:
   ```yaml
   concurrency:
     group: ci-${{ github.ref }}
     cancel-in-progress: true
   ```
5. **Branch protection** (one-time GitHub setting, not in YAML): mark all 3
   `ci.yml` jobs as required status checks for merges to `main`. Documented
   in §9.

---

## 6. The `release` Skill

**Location:** `~/.claude/skills/release/SKILL.md` (user-level skill, applies
to any repo that opts in via a project marker file — Phase 3N only enables
it for NereusSDR).

**Invocation:** `/release` (interactive bump prompt) or `/release patch` /
`/release minor` / `/release major` (skip the prompt).

**Implementation language:** the skill is a Markdown file containing
instructions for Claude Code, plus a small `bin/release.sh` helper for the
deterministic shell steps (tag validation, git operations). Claude drives
the conversation; the helper does the mechanical work that doesn't need
LLM judgment.

### 6.1 Step sequence

1. **Pre-flight checks** — abort if any fail:
   - `git status --porcelain` is empty (clean tree)
   - Current branch is `main`, up to date with `origin/main`
   - `gh`, `gpg`, `git` available; GPG key cached/unlocked
   - Last `ci.yml` run on `main` was green (`gh run list --workflow ci.yml
     --branch main --limit 1`)
   - No existing tag matches the version we're about to create
   - Tag is not `*-test*` (reserved for manual workflow shakedown)

2. **Version bump decision:**
   - Find last tag: `git describe --tags --abbrev=0`
   - Show `git log <last>..HEAD --oneline`
   - Ask **major / minor / patch** (or use the CLI arg)
   - Compute new version, show the proposed `CMakeLists.txt` diff

3. **Changelog draft:**
   - Parse commit subjects since last tag, group by Conventional Commit
     prefix (`feat:`, `fix:`, `docs:`, `refactor:`, etc.). Bucket the rest
     under "Other."
   - Draft a `## [X.Y.Z] - YYYY-MM-DD` section, prepend it under
     `## [Unreleased]` in `CHANGELOG.md`.
   - Open `CHANGELOG.md` in `$EDITOR` (fallback `nano`). User revises, saves,
     closes.
   - Skill re-reads the file; abort if the new section is empty or absent.

4. **Doc sweep:** update version references the skill knows about:
   - `CMakeLists.txt` `project(... VERSION X.Y.Z)`
   - `README.md` "Current version" line if present
   - `docs/MASTER-PLAN.md` if it has a version line
   - Any other version-bearing file is **flagged for manual update**, not
     auto-touched

5. **Commit + tag:**
   - `git add` only the touched files (no `git add -A`)
   - Single GPG-signed commit: `release: v X.Y.Z`
   - Co-Authored-By trailer per the user's standing rule
   - Annotated, signed tag: `git tag -s vX.Y.Z -m "NereusSDR vX.Y.Z"`
   - Show the commit and tag to the user before pushing

6. **Push:** `git push origin main && git push origin vX.Y.Z`. This is the
   `release.yml` trigger.

7. **Watch:** `gh run watch` on the new workflow. Stream status. On any
   failure: stop, print failing job log tail, print recovery commands:
   - Re-run: `gh run rerun <id>`
   - Abandon (manual, destructive): `git push --delete origin vX.Y.Z && git
     tag -d vX.Y.Z`

   The skill never runs the abandon path itself.

8. **Open draft release:** when the workflow finishes green,
   `gh release view vX.Y.Z --web` opens the draft in your browser. Skill
   ends. You review artifacts, edit notes if needed, click Publish.

### 6.2 What the skill does NOT do

- Publish the release (human gate)
- Delete tags or revert commits on failure (destructive)
- Push to remotes other than `origin`
- Run on any branch other than `main`
- Operate on `*-test*` tags (reserved for workflow shakedown)

### 6.3 Dry-run mode

`/release --dry-run`: runs every step except `git push`, `git commit`,
`git tag`, and `gh release create`. Prints what *would* happen. No state
changes. Used for testing the skill itself and previewing changelog drafts
without committing.

### 6.4 Prerelease handling

- If the tag includes `-alpha` / `-beta` / `-rc`, the skill forces
  prerelease on the GitHub Release.
- For clean `v0.x.y` tags, the skill asks `publish as prerelease? (Y/n)`
  defaulting to **Y** while NereusSDR is below 1.0.
- For `v1.x.y+`, default flips to **n**.

---

## 7. Failure Modes & Recovery

| Failure | Detected by | Behavior |
|---|---|---|
| Tag format invalid | `prepare` job regex | Workflow fails <10s, no builds run |
| Tag version ≠ CMakeLists version | `prepare` job grep | Fails fast with diff |
| Changelog section missing | `prepare` job extract step | Fails fast |
| Build failure on one platform | That build job | Other platforms still complete (`fail-fast: false`); `sign-and-publish` blocked by `needs:` |
| Test failure | Build job's `ctest` step | Same as build failure |
| GPG key import fails | `sign-and-publish` | Job fails, no draft release created |
| Skill: dirty tree / red CI / existing tag | Pre-flight | Aborts before any state change |
| Skill: empty changelog after edit | Step 3 | Aborts, leaves CHANGELOG.md staged for manual fix |
| Skill: workflow fails after tag pushed | Watch step | Stops, prints log + recovery commands, exits non-zero, **leaves tag in place** |
| Skill: network drops mid-watch | Watch step | Exits with resume command; tag still in place; workflow still running on GitHub |

**No automated rollback by design.** Mature release tooling (release-please,
goreleaser, cargo-release) leaves failed tags in place and fixes forward.
Reasons:
- Tags propagate fast; force-deleting a published ref is destructive
- Most failures are transient (flaky runner, network, missing secret) and
  fixable in place via `gh run rerun`
- Non-transient failures are best resolved by landing a fix on `main` and
  cutting a new patch — cleaner history, no rewritten refs

---

## 8. Testing Strategy

### 8.1 Workflow itself

1. **Local dry-run with `nektos/act`** during development. Catches YAML
   errors and basic logic without burning real runner minutes. Spec the
   command: `act push -e events/release-tag.json`.
2. **Disposable test tags** for first real runs: `v0.0.1-test1`,
   `v0.0.1-test2`, etc. Skill refuses to operate on `-test*` tags, so they
   must be hand-created. Delete them after the workflow has been green
   twice on each platform.
3. Packaging scripts (`scripts/macos/build-dmg.sh`,
   `scripts/windows/installer.nsi`, `scripts/linux/build-appimage.sh`) get
   created during implementation; today the `scripts/` dir is empty.

### 8.2 Skill itself

- `/release --dry-run` exercises the full flow with no state changes
- Manual smoke tests against a sandbox repo before installing into
  NereusSDR

### 8.3 Each release artifact (manual checklist in release notes template)

A 10-minute checklist appended to the release-notes template:
- Launch the binary on each OS
- Verify version string in About dialog matches tag
- Connect to a test radio if available; verify spectrum renders
- Verify wisdom file generates on first run (Linux) and is reused on second
- Verify settings file persists across runs

Not automated. Automation cost is not justified for a solo project.

---

## 9. One-time GitHub Setup

These are clicked through on github.com once and recorded here for
reproducibility:

1. **Branch protection on `main`:**
   - Require pull request before merging
   - Require status checks: `CI / Build (Linux x64)`,
     `CI / Build (macOS Apple Silicon)`, `CI / Build (Windows x64)`
   - Require signed commits
   - Require branches up to date before merging

2. **Repository secrets** (Settings → Secrets and variables → Actions):
   - `GPG_PRIVATE_KEY` — exported with `gpg --armor --export-secret-keys
     KG4VCF` (already exists, used by current sign-release.yml)
   - `GPG_PASSPHRASE` — already exists
   - **Phase 3N+1, not yet:** `APPLE_CERT_P12`, `APPLE_CERT_PASSWORD`,
     `APPLE_ID`, `APPLE_TEAM_ID`, `APPLE_APP_PASSWORD`,
     `AZURE_TENANT_ID`, `AZURE_CLIENT_ID`, `AZURE_CLIENT_SECRET`,
     `AZURE_TRUSTED_SIGNING_ENDPOINT`,
     `AZURE_TRUSTED_SIGNING_ACCOUNT`,
     `AZURE_TRUSTED_SIGNING_PROFILE`

---

## 10. Phase 3N+1 — Code Signing (Future)

Captured here so the upgrade is well-defined when certs land. **Do not
implement now.**

### 10.1 macOS — Apple Developer ID + notarization

Files to touch:
- `.github/workflows/release.yml` `build-macos` job:
  - Add step: import `APPLE_CERT_P12` into a temporary keychain
  - Replace `codesign --sign -` with `codesign --options runtime --sign
    "Developer ID Application: <Name> (<TeamID>)" --force --deep`
  - Add step after DMG build: `xcrun notarytool submit
    NereusSDR-${version}-macos-arm64.dmg --apple-id "$APPLE_ID"
    --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_PASSWORD" --wait`
  - Add step: `xcrun stapler staple NereusSDR-${version}-macos-arm64.dmg`
- Release notes template: remove the "right-click → Open" instructions

### 10.2 Windows — Azure Trusted Signing

Files to touch:
- `.github/workflows/release.yml` `build-windows` job:
  - Add step after `makensis`: `azuresigntool sign -kvu
    "$AZURE_TRUSTED_SIGNING_ENDPOINT" -kvc
    "$AZURE_TRUSTED_SIGNING_PROFILE" -kvi "$AZURE_CLIENT_ID" -kvs
    "$AZURE_CLIENT_SECRET" -kvt "$AZURE_TENANT_ID" -tr
    http://timestamp.acs.microsoft.com -td sha256 NereusSDR-${version}-windows-x64.exe`
- Release notes template: remove the SmartScreen "More info → Run anyway"
  instructions

Both are additive — no workflow restructuring required.

---

## Appendix A — Obtaining an Apple Developer ID

**Cost:** $99 USD/year. **Time to get usable certs:** 1–7 days (Apple
review can take a few days for new individuals).

1. **Sign in to developer.apple.com** with the Apple ID you want associated
   with the account. Use a personal Apple ID — moving the developer
   account between Apple IDs later is painful.
2. **Enroll** at <https://developer.apple.com/programs/enroll/>:
   - Choose **Individual** (not Organization) unless you want NereusSDR
     attributed to a legal entity. Individual is faster (no D-U-N-S
     number required) and the cert reads as your legal name.
   - Pay $99. Apple processes the enrollment, usually within 24–48 hours.
3. **Once enrolled**, go to <https://developer.apple.com/account/resources/certificates/list>:
   - Click **+** to create a new certificate
   - Select **Developer ID Application** (NOT "Mac App Distribution" — that
     one is for the Mac App Store, which we don't ship to)
   - Follow the prompts to upload a Certificate Signing Request (CSR). On
     your Mac: **Keychain Access → Certificate Assistant → Request a
     Certificate from a Certificate Authority** → save to disk. Upload
     that file.
   - Download the resulting `.cer`, double-click to install in Keychain.
4. **Export to .p12 for CI:**
   - Open Keychain Access → My Certificates → find "Developer ID
     Application: <Your Name> (<TeamID>)"
   - Right-click → Export → save as `developer-id.p12`, set a strong
     password (this becomes `APPLE_CERT_PASSWORD`)
5. **Create an app-specific password** for notarytool at
   <https://account.apple.com/account/manage> → Sign-In and Security →
   App-Specific Passwords → Generate. Label it "NereusSDR notarytool."
   This is `APPLE_APP_PASSWORD`. (Do NOT use your real Apple ID password.)
6. **Find your Team ID** at
   <https://developer.apple.com/account> → Membership Details. 10-character
   string. This is `APPLE_TEAM_ID`.
7. **Add to GitHub secrets** (NereusSDR repo → Settings → Secrets and
   variables → Actions → New repository secret):
   - `APPLE_CERT_P12` — base64-encoded contents of `developer-id.p12`
     (`base64 -i developer-id.p12 | pbcopy`)
   - `APPLE_CERT_PASSWORD` — the export password from step 4
   - `APPLE_ID` — the email address on your developer account
   - `APPLE_TEAM_ID` — from step 6
   - `APPLE_APP_PASSWORD` — from step 5
8. **Tell me when it's done.** I implement Phase 3N+1 §10.1 against the
   spec — should be a single PR, ~30 min of work.

**Renewal:** the $99 is annual. Apple emails 30 days before expiry. Your
existing Developer ID Application certificate is valid for 5 years
independent of the membership renewal, but you can't notarize new builds
if the membership lapses.

---

## Appendix B — Obtaining Azure Trusted Signing

**Why this and not Sectigo / DigiCert / SSL.com:** Azure Trusted Signing is
a Microsoft service that issues short-lived (3-day) signing certificates
out of an HSM, billed at ~$10/month. Crucially, signatures from it carry
SmartScreen reputation **equivalent to an EV cert**, with no $400+/year
EV cert purchase, no USB token, no awkward HSM integration. It is the
correct answer for new projects in 2026.

**Cost:** ~$10 USD/month (Azure Basic tier; first month often free with new
Azure account credits). **Time to get usable certs:** 1–3 days for
identity validation.

1. **Create an Azure account** at <https://azure.microsoft.com> if you
   don't have one. Personal Microsoft account works.
2. **In the Azure Portal**, create a **Trusted Signing Account**:
   - Search "Trusted Signing" in the top bar → Create
   - Resource group: create new, name it `nereussdr-signing`
   - Region: pick the closest one (East US, West US, etc.)
   - Pricing tier: **Basic** ($9.99/month at time of writing)
   - Name: `nereussdr-signing-account`
3. **Submit identity validation:**
   - Inside the Trusted Signing Account → **Identity validation** → New
   - Type: **Individual** (matches Apple choice; faster)
   - Provide: legal name, government ID upload, address, phone
   - Microsoft reviews this in 1–3 business days. You'll get an email when
     approved.
4. **Create a Certificate Profile** (after validation approves):
   - Inside the account → **Certificate profiles** → New
   - Type: **Public Trust** (for distributing to end users)
   - Identity validation: select the one approved in step 3
   - Name: `nereussdr-public-trust`
   - This is `AZURE_TRUSTED_SIGNING_PROFILE`
5. **Create a service principal for CI** (this is the headless identity
   GitHub Actions uses to talk to Azure):
   - Azure Portal → **Microsoft Entra ID** → App registrations → New
     registration
   - Name: `nereussdr-github-signing`
   - Supported account types: **Single tenant**
   - Register → note the **Application (client) ID** (this is
     `AZURE_CLIENT_ID`) and **Directory (tenant) ID** (this is
     `AZURE_TENANT_ID`)
   - Inside the registration → **Certificates & secrets** → New client
     secret → label "GitHub Actions" → expiry 24 months → copy the value
     immediately (this is `AZURE_CLIENT_SECRET`, only shown once)
6. **Grant the service principal signing permission:**
   - Back in the Trusted Signing Account → **Access control (IAM)** → Add
     role assignment
   - Role: **Trusted Signing Certificate Profile Signer**
   - Assign access to: User, group, or service principal
   - Members: select `nereussdr-github-signing`
   - Save
7. **Note the signing endpoint URL** from the Trusted Signing Account
   overview page. Looks like `https://eus.codesigning.azure.net`. This is
   `AZURE_TRUSTED_SIGNING_ENDPOINT`.
8. **Add to GitHub secrets:**
   - `AZURE_TENANT_ID`
   - `AZURE_CLIENT_ID`
   - `AZURE_CLIENT_SECRET`
   - `AZURE_TRUSTED_SIGNING_ENDPOINT`
   - `AZURE_TRUSTED_SIGNING_ACCOUNT` (the account name from step 2)
   - `AZURE_TRUSTED_SIGNING_PROFILE` (from step 4)
9. **Tell me when it's done.** I implement Phase 3N+1 §10.2 — single PR,
   ~30 min.

**Renewal:** monthly billing on Azure. The 24-month client secret will
need rotation before it expires; calendar that. Signing certs themselves
are issued fresh (3-day lifetime) on every signing operation, so there is
nothing to manually renew on the cert side.

---

## 11. Files Touched (Implementation Scope)

**New files:**
- `.github/workflows/release.yml`
- `scripts/linux/build-appimage.sh`
- `scripts/macos/build-dmg.sh`
- `scripts/windows/installer.nsi`
- `~/.claude/skills/release/SKILL.md`
- `~/.claude/skills/release/bin/release.sh`
- `docs/architecture/2026-04-12-phase3n-release-pipeline-design.md` (this file)

**Modified files:**
- `.github/workflows/ci.yml` (caching, tests, artifact upload, concurrency)
- `CHANGELOG.md` (add `## [Unreleased]` section if missing — Keep-a-Changelog
  format)
- `README.md` (release / install section pointing at GitHub Releases page)

**Deleted files:**
- `.github/workflows/appimage.yml`
- `.github/workflows/macos-dmg.yml`
- `.github/workflows/windows-installer.yml`
- `.github/workflows/sign-release.yml`

---

## 12. Open Questions

None blocking. Items deferred to the implementation plan:

- Exact AppImage tooling: `linuxdeployqt` vs `linuxdeploy` (both work,
  `linuxdeploy` is more actively maintained)
- DMG creation tool: `create-dmg` (Homebrew) vs raw `hdiutil`
  (`create-dmg` produces nicer-looking DMGs with backgrounds; `hdiutil`
  has zero dependencies)
- Whether `ci.yml` and `release.yml` should share build steps via a
  composite action under `.github/actions/` to avoid drift, or stay
  duplicated for clarity. Lean: composite action, but only after both
  workflows are stable.
