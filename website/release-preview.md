# Major release website preview

Prepared on 2026-10-02 in `codex/website-major-release`. This is a local review
copy. No deployment, remote server write, push or PR has been performed.

The page keeps the existing static HTML/CSS/JS setup and interactive desktop
radio demo. It adds native iPhone simulator captures, a Core/SBC explanation,
a radio-to-Core-to-console diagram, shared station operation, and the major
release's desktop feature changes. Screenshot originals and hashes are in
`screenshot-sources.md`; pixels are unchanged.

## Local review

Run from this checkout:

```sh
python3 -m http.server 8766 --bind 127.0.0.1 --directory website/public
```

Open http://127.0.0.1:8766/. Review the mobile section and Core section as well
as the top of the page. The live website is independent of this preview.

The follow-up removes alpha release language, adds an operator-focused Nereus
identity section, and rewrites the heritage around both upstream foundations
and original Core/mobile work. Feature coverage now includes 3D spectrum,
NNR load-aware noise reduction, per-slice audio routing, persistent TNF,
AM modulation instrumentation and live diagnostics.

## Before publication

- Set the final release version, date and actual download asset details when
  the major release exists. Retain coherent signed-download links and commands.
  Existing JavaScript only swaps releases when all desktop asset suffixes match.
- Replace the hero's "The next major release", Status's "Coming in the major
  release", and Download's upcoming-release note when the release is published.
  Add the new release to the release-history list; keep old releases historical.
- Confirm mobile distribution separately. Add an App Store or public TestFlight
  link only when JJ supplies the approved destination. The current page does
  not advertise either as publicly available.
- Confirm the final release scope. The native app is being worked on; these
  captures document a development build, not a frozen release candidate.
- Keep station-card image availability precise. Main contains build recipes
  and a manual workflow, which do not establish public prebuilt card downloads.
- Review and approve the local result before using the existing deploy script.

## Content grounding

Core: origin/main `15508fb42` including PR327. Main's older receive-only docs
are stale; RemoteTransmitClient and session ownership code establish remote
transmit and four-device coordination. Hardware records establish Pi4/HL2 and
ROCK5C/G2 development. They do not guarantee arbitrary ARM64 board support or
unlimited simultaneous DSP loads.

Desktop feature copy is grounded in the main source and Unreleased changes:
multiple pans/slices, TNF, AM/SAM/DSB, AM monitor, vintage meter faces and
RF2K-S integration. No promise of complete hardware validation is added.
The open PR323 enhancement package is not advertised as released.

Identity evidence: StationHost, StationServer, ReceiverPlanner, DisplayCodec
and SpectrumEndpoint establish the station architecture, per-device views,
receiver-change confirmation and calibrated display transport. PanadapterStack,
ClarityController, DssRenderer, NnrSettings and MasterMixer establish the
workspace, adaptive display, 3D, NNR fallback and audio-routing claims.
Diagnostics history remains unbuilt; public copy says live counters instead.
Clarity and earlier noise reducers predate v0.5.2 and are described as product
capabilities, not new inventions. Heritage retains Thetis/WDSP/Aether attribution.

Scale check (snapshot additions versus v0.5.2, not original production-code
size): origin/main 15508fb42 adds 1,456,270 lines across 2,889 files;
claude/iphone-app as inspected adds 2,147,303 across 4,985 files. These include
tests, dependencies, docs and tooling. The mobile branch is separate from main.
The public page focuses on operator outcomes rather than a raw line count.

## Verification for this preview

- Desktop, 390 px phone, 768 px tablet, 1024 px and 1440 px layouts inspected.
  Document width matched viewport width; no horizontal overflow or failed images.
- Mobile menu opens, follows the iPhone anchor and closes; final mobile link font
  is 15.2 px. Screenshots stack at a readable 290 px width on phones.
- Existing desktop-demo keyboard tuning still changes its frequency.
- HTML checks: 31 unique ids, seven image elements, no missing local assets,
  broken local fragments or missing alt attributes. All new image dimensions
  match their PNG headers and the originals' SHA-256 hashes.
- JavaScript syntax check and `git diff --check` pass. JavaScript is unchanged.
- Independent content and code reviews found no blocking issues.
- Follow-up source review caught an unbuilt connection-history placeholder;
  copy corrected to live connection counters. Responsive checks repeated for
  the identity and expanded feature sections; visible page text has no alpha.
- No deployment script or remote mutation was run. Production CSP has not been
  exercised by the Python preview, which serves the same local static assets.
