# Major release website preview

Prepared in `codex/website-major-release`; refreshed on 2026-10-04 against
release candidate `65154776440bc51751e9b89e4b172980bd1cb91c`.
Source integration and live publication are separate. This task has performed
no deployment, remote server write, push or new PR.

## Review locally

```sh
python3 -m http.server 8766 --bind 127.0.0.1 --directory website/public
```

Open http://127.0.0.1:8766/. The page keeps the existing static HTML/CSS/JS
setup and interactive desktop radio demo. It uses unmodified native simulator
captures; original paths and hashes are in `screenshot-sources.md`.

The draft keeps “Your station. Wherever you operate.” and brings the release
forward in operator terms: station Core and native consoles, independent
receivers, 3D/TNF/diversity, NNR/PureSignal 3, containers/Canvas/meters, and
native TX EQ/CFC. Recent phone work includes filter presets, TX profiles,
PA readings, voice editors, station tools and Core-described controls.

Guides are grouped by task: operate the station, install/connect a Core, and
shape transmitted audio. Upgrade navigation uses the new operator-facing
`docs/guides/upgrading-to-2026.10.0.md`, prepared in Release's working copy;
its source integration is still a publication dependency. The current Discord invite supplied by JJ is
https://discord.gg/m35ERjwRe. GitHub Issues remains the destination for tracked
bugs and feature requests.

## Before publication

- Review and approve this preview before using the existing deploy script.
- Integrate the release documentation into public main before publishing these
  guide links. As checked on 2026-10-04, GitHub main returned 404 for the manual
  index, SBC install guide and EQ/CFC guide. All exist in candidate 651547764.
  Recheck all linked documentation after Release ships its source integration.
- GitHub's latest published release remains v0.5.2 (24 May 2026), verified on
  2026-10-04. Its real links and asset sizes remain in the download section.
  Finalize the version, release date, history, platform requirements, commands
  and actual asset names when 2026.10.0 is published. Existing JavaScript
  changes download versions only when every expected desktop asset exists.
- Change forthcoming 2026.10.0 wording when the release is published.
- Confirm mobile distribution separately. Add an App Store or public TestFlight
  link only when the approved destination is available. Native source in the
  candidate does not establish public mobile availability.
- The SBC guide targets 64-bit Pi OS Lite or compatible Armbian Debian Trixie
  and a matching Core package. Do not imply existing public card images or
  published Trixie packages while release artifacts are still pending.
- CAT is explicitly deferred to the next release. Incomplete XDMA PR339 is not
  advertised. No shipping or primary worktree was edited by this task.

## Content and link basis

The candidate's operator manual and code establish the Core/GUI roles,
per-device receiver and transmit authority, native phone controls, display
transport, NNR fallback, PureSignal 3, diversity, containers and voice editors.
Its `docs/guides/install-core-sbc.md` and `docs/guides/tx-eq-cfc.md` establish
fresh-board setup and the desktop Graphic/Parametric EQ/CFC workflows.

The RV service introduces devices; direct, TURN and separate WebSocket relay
paths carry station sessions. IPv4/IPv6 and relay selection provide alternatives
for CGNAT/mobile networks; they do not guarantee every carrier/firewall path.
T-Mobile's official Home Internet connection page, checked on 2026-10-04,
confirms that its gateway does not offer configurable port forwarding:
https://www.t-mobile.com/support/home-internet/connect

Hardware copy says development used Pi4 and a 2 GB Rock5C, not a guaranteed
minimum or workload. Diversity requires supported synchronized two-ADC paths;
phone memories are not called shared. Canvas is desktop-only. Phone PureSignal
copy promises status and automatic calibration controls, not desktop AmpView or
advanced correction-file workflows. Clarity and earlier reducers are product
capabilities rather than new inventions. Attribution remains intact.

## Verification

The current refresh passed 390/768/1024/1440 px overflow and guide-grid checks,
mobile menu opening/closing and guide navigation, local image loading, HTML
ID/fragment/asset validation, JavaScript syntax and whitespace checks. All four
native screenshots still match their recorded source hashes. Independent
source/copy review checked the feature claims and candidate documentation; the
new upgrade guide was then checked in Release's working copy. Prior checks
also covered demo keyboard tuning. Browser proof captures are kept locally. No application builds or CI reruns are needed for
this static website change. Production Caddy headers are not exercised by the
Python preview; deployment remains a separate reviewed action.

## Source handoff

Release candidate 651547764 is a separate repository. Its website preimage
differs from this branch's parent: it lacks the previous Discord addition and
contains document-page CSS for the phone privacy/source pages. The final
651-based website patch restores the current invite and preserves that CSS.
Apply only the reviewed website delta; retain Release's version, docs and
privacy/source pages. No source integration or deployment is authorized by
this handoff.

## Own-site guide follow-up

JJ requested the existing one-source manual posture: keep the operator guide
and how-to pages on nereussdr.com, maintained from the repository manuscripts.
This follow-up starts from Root's signed docs/website checkpoint
`9f3ff34e60ccc8b5208ba9f85c865952228c9473` and leaves its application code and
manuscripts unchanged. The live homepage currently has no self-hosted manual
entry; this completes the agreed publication path rather than assuming an
already deployed guide route.

The existing `docs/manual/build_preview.py` renderer gains an opt-in website
build. Its default manual-review output remains available. Website output is
`/manual/` plus the 20 existing chapter filenames, and these how-to pages:
`/guides/install-core-sbc.html`, `/guides/tx-eq-cfc.html`,
`/guides/upgrading-to-2026.10.0.html`, and `/guides/remote-access.html`.
The last page renders `rendezvous/README.md`; the other three render their
same-named files under `docs/guides/`. Images, operator prose, mobile delivery
qualifications and capture/build limitations come from the source manuscripts.
Generated files are build output, not a second independently maintained manual.

Homepage operator links use those local routes. Cross-chapter and how-to links
are rewritten through the source-to-page map with their section fragments.
The upgrade guide's operator overview links point to homepage `#core` and
`#download`; technical source references and repository/release/issue links
retain GitHub destinations. The generated manual's source map records exact
manuscript and copied-image hashes without local machine paths.

Before publication, run the renderer and source/link checks, commit generated
output with any manuscript updates, and review the integrated website. This
follow-up generates a local preview only; it does not change public releases,
mobile distribution, production routing/configuration or the live server.
The earlier GitHub-main guide-link 404 gate is superseded by bundling these
rendered files with the site. Release still owns final source/docs integration
and the existing pending manual captures and live operating checks.
