# iPhone and iPad release evidence

Use this with plan Tasks 68 to 70 and the device results in
`docs/architecture/2026-09-23-iphone-app-verification/README.md`. It records
what must be proved before distribution; it does not mark the release ready.
The controller owns local archive work. JJ owns account, contact, hardware
observations and submission decisions. No push or PR creation is authorized
without JJ's explicit go-ahead.

## Accepted source

- Integrate the approved software and UI changes into phone main. Record the
  exact commit and tree; preserve signed commits and the three protected
  specification documents' equality with the plan branch.
- Run the final combined NereusKit and full app/UI scheme checks, Release
  build, interop checks required by changed contracts, provenance and privacy
  scans. Record real exits, counts, skips and evidence paths on that commit.
  Lane tests prove their frozen snapshots, not a different combined tree.
- Review the entire branch from the original implementation base
  `b524fb0a9`, resolve findings and recheck affected behavior. Draft the final
  PR description from that reviewed result.
- Keep unobserved device, bench and Core-dependent criteria open. Simulator
  screenshots do not prove on-air behavior, battery/heat endurance or physical
  button support; data estimates do not satisfy traffic measurements.

## Local archive

Only after the accepted phone-main source is clean, reserve the shared build
slot, confirm no simulator is booted, and run `ios/scripts/archive.sh`. It
uses jobs 2, the configured signing team, HEAD's build number and source tag.
Preserve Xcode's actual signing or provisioning failure; do not change the
team, identifiers or entitlements to bypass it.

Record the archive's path, source commit, version, build number, signing
identity, bundle identifiers and app/widget contents. Verify the archive
contains `PrivacyInfo.xcprivacy` and the declared required-reason categories
cover the shipped binaries. Run the existing store-readiness checks against
the archived app by setting `NEREUS_IOS_RELEASE_APP` to its
`Products/Applications/NereusSDR.app` path:

```sh
python3 tests/compliance/test_ios_store_readiness.py
```

The same check without that environment variable validates source metadata
only. A successful simulator Release build is not a signed device archive,
and a local archive is not an upload or App Store acceptance.

## Distribution preparation

- Capture the final approved iPhone and iPad UI for the store, in the required
  sizes. Reconcile `description.md`, `keywords.txt` and `review-notes.md` with
  the accepted build; keep the Core requirement clear.
- Record the review video against an identified real station, replace
  `VIDEO LINK` in the notes and provide App Review contact details in App
  Store Connect. Keep those personal details out of the repository.
- Finish the hardware-button experiment and settle the Release background
  modes and provisioning. The isolated `PTT_PROBE` experiment is not shipping
  radio control. Keep Camera Control and AirPods stem exclusions from the
  approved plan.
- Follow JJ's recorded encryption answer in `export-compliance.md`; preserve
  the current absent plist keys until the approved documentation/code exists.
  Account declarations, territorial availability and reporting are separate
  pending steps, not results of a local compliance test.
- Confirm the privacy and corresponding-source pages are live from main
  after the phone PR merges (D134), and that the source identifies the exact
  distributed build. Do not publish these pages from a working branch.
- Keep D137, D138 and D139 explicitly identified as controller calls for JJ
  to confirm in the PR review. Settings backup and restore remain deferred
  to a later release and on the roadmap (D133).
