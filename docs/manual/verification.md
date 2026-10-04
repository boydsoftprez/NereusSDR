# Draft verification record

Contributor evidence for the expanded operator draft, reviewed 2 October 2026,
with bounded desktop source reconciliation on 4 October 2026.
This record distinguishes source coverage, presentation checks, authentic
captures and live operating checks. Passing one does not establish the others.

## Documented build

- Original independently reviewed source baseline: `d1608a3de75e5c307187966110f2e51e8ecb0d17`.
- Bounded desktop reconciliation: `df6590ff12aeffb7a73f46acc37ca3824dfe3901`, tree `48a41523ce4562c2cad5c7e8be316009e275a913`, the 2026.10 candidate inspected on 4 October.
- The consolidated candidate includes the accepted signed mobile checkpoint `8e1c06198bf2023cf846cddb5296ef952cc9812c`, tree `6db2011dfac47bf68f36b3fedd672d91acfb51df`, plus the six reviewed logging-privacy annotations from signed `1f6f2be0a4bd8a40d2c62a0b53f17b062dcf58c6` (mobile marketing version 2026.9.0; tested preview build 5801). Native app chapters retain their earlier development/source-verified scope; delivery and release acceptance are separate.
- Desktop captures: `codex/all-prs-radxa-gui@c1991b2d42ea`, title version 0.5.2.
- Phone simulator capture source: `46591d6cd62a916a6eef83e65d12fc655259b44d`.
- iPad simulator build identities and per-image provenance: [capture inventory](images/README.md).
- Audience: SDR-literate HPSDR hobbyists operating the desktop and iPhone/iPad app.
- Source: twenty task/reference chapters, operator index, shared image directory.

The source and captured desktop are different development builds. The figures
illustrate the captioned controls and states; they do not establish release
availability or the appearance of uncaptured settings. Native phone/iPad
simulator images include test data, while their procedures are checked against
implemented source. No live-phone walkthrough or public-release compatibility
claim is made.

## Source and coverage evidence

The implemented-task inventory includes 126 operating areas, plus explicit
unbuilt exclusions. The [coverage record](coverage.md) maps areas to the actual
chapters and records the integrated review. Registration, handler bindings,
capability/readiness gates and the unbuilt-feature list establish scope.
Design boards, a class name or a disabled placeholder alone are insufficient.

| Subject | Principal source evidence |
| --- | --- |
| Connections, menus, window/slices | `MainWindow`, connection selector/panels, `VfoWidget`, `SpectrumWidget`, receiver allocation and pan-layout classes. |
| Receive/DSP/filters | RX applet, DSP Setup pages/options, filter-preset store/editor, DSP asset dialog and model/runtime gates. |
| TX/profiles/voice | TX/Phone-CW applets, TX Input, transmit/DSP Setup pages, TX Profile editor and Mic Profile Manager. |
| Mobile connection/operation/sharing | `ios/NereusApp/Connect`, `Main`, `Band`, `Modes`, `Shared`, `Session`, `Audio`, `Setup`, `Tools` and `Radio`; Core descriptions and command/readback handlers. |
| Digital/RADE/spots | VAX/TCI pages and routers, RADE applet, reporter clients, Spot Hub and phone tool pages. |
| Hardware/calibration/accessories | Hardware tab registration and board capabilities; ALEX, HL2, OC, PA, PureSignal, diversity, PGXL/TGXL/RF-Kit handlers and phone tool limits. |
| Display/support | Display/Appearance pages, spectrum input handlers, container/MMIO designer and meter sources; diagnostics, logs, support and backup/import handlers. |

The earlier twelve-chapter overview was rejected for incomplete coverage. Its
review verdict does not apply to this expanded edition. Delegated source reads
that used the older root checkout were identified during integration, rejected
and redone against the baseline above. The integrated review independently
checked the resulting instructions; author handoffs alone were not acceptance.

## Integrated review result

The first review of the expanded handoff found one blocking instruction and
important accuracy/coverage gaps: 95 inventory areas covered and 31 needing
expansion. The bounded correction wave addressed the PureSignal calibration
order, VAX platform/routing, Core rename, profile transfer, neural DSP,
transmitter/hardware policy, desktop listening/audio recovery, display and
native mobile workflows. Newly introduced claims were checked within the
same scoped review and corrected before acceptance.

The prior independent review accepted the source-coverage correction wave.
A subsequent operator-usability audit still found incomplete joined workflows
for digital audio, voice profile recall, RADE and mobile handoffs, plus dense
procedures and too few figures. Those findings prompted another bounded
authoring and screenshot pass. The phone handoff entry in [coverage](coverage.md)
now explicitly records the implementation limit: receiver takeover is offered,
but no general phone transmit takeover action is exposed. Coverage is a map
of documented scope, not a certificate that every task has been exercised.
The contributor reports remain in the ignored review workspace.

The latest independent scoped review accepted the digital RX/TX/restore route,
voice baseline/recall/rollback, joined RADE session, coordinated TGXL cycle,
native mobile daily sessions and supported receiver/TX starting states. It
also inspected all twenty new capture originals and captions. Corrections
included RADE pre-decode silence, station-versus-app TCI ownership, both EQ/CFC
layout resets, profile Save versus activation, backend-specific VAX endpoints
and the actual headphone-pause resume action. No required finding remains
open within that usability scope. This does not recertify unchanged chapters
or complete the live/release checks below.

## Bounded release-candidate reconciliation

This carry is documentation only. It preserves the twenty-chapter manuscript,
original capture bytes/provenance and prior independent reviews for unchanged
areas. It reconciles the following source-visible differences. These edits
were checked against candidate handlers and gates, not a running application.
The controller performed the changed-area source review; the release integrator
owns final independent integration review. This is not another whole-manual
acceptance or physical operating certification.

| Changed area | Candidate source evidence | Result in the manual |
| --- | --- | --- |
| Core Settings and direct addresses | `SetupDialog.cpp:1444–1462`; `ConnectionSelector.cpp:129–175`; `CoresSetupPage.cpp:148–302,307–423,575–661` | Chapter 1 separates inspected Core from this window's connection, verifies identity before retaining addresses and separates manual/worked/Core-supplied address evidence. |
| Shared Core rename and administration | `CoreSettingsHost.cpp:161–196`; `CoresSetupPage.cpp:190–277,663–745`; `MainWindow.cpp:10365–10396` | Accepted shared rename, temporary authenticated sign-in limits, disabled radio/device administration and actual Connections/Manage Radios contexts are explicit. |
| Computer audio, Core media and remote mic | `AudioDevicesPage.cpp:43–94`; `RemoteAudioWidget.cpp:41–65,93–128`; `AudioTxInputPage.cpp:441–471,499–518`; `RemoteMicSource.h:20–25`; `BoardCapabilities.h:510–524`; `RadioModel.cpp:9921–9925` | Chapters 1, 5 and 14 distinguish local capture/playback, selected quality versus actual media, session Radio Mic, absent radio-mic VOX/program route and the HL2 audio-add-on prerequisite. |
| Canvas/contents/source and cancellation | `ContainerSettingsDialog.cpp:414–450,538–556,1081–1180,2047–2050,2125–2134,2460–2608`; `ContentPropertyEditor.cpp:153–183,234–263`; `ContainerEditSession.cpp:96–136`; `ContainerControlCatalog.h:16–28` | Chapter 10 covers Canvas gestures/geometry/layer/lock, fifteen native controls, singleton moves/return, preview versus live operations, portable append/file replacement and Cancel since last Apply. |
| EQ/CFC mode, count and reset | `TxEqDialog.cpp:341–367,657–668,1313–1364`; `TxCfcDialog.cpp:303–344,429–518,1122–1124,1193–1209`; `ParametricEqWidget.h:143–149`; `ParametricEqWidget.cpp:207–220`; `tst_tx_cfc_dialog.cpp:436–460` | Chapter 14 uses Graphic · Legacy / Parametric, exact width fields, explicit count Apply/Cancel and Undo/Redo. CFC resets zero the corresponding overall level, preserving frequencies/selection/the other graph; Parametric Reset resets its preamp and frequencies. Historical captions identify the earlier UI. |
| Transmit takeover, receive tail and radio-side PTT | `TransmitHolder.cpp:123–183`; `StationTransmitTake.cpp:472–499,719–793`; `TakeTransmitDialog.cpp`; `RemoteKeying.cpp:299–328` | Chapters 5 and 8 preserve receiver-versus-TX authority, name take confirmation/recheck, wait for receive and distinguish hosting-desktop radio PTT from headless behavior. Phone first-key/no-general-take limits remain development-app instructions. |

The candidate disables **Meter Data Sources (MMIO)** in the new draft editor
(`ContainerSettingsDialog.cpp:449,1876`). Retained bindings can still be used,
but chapter 10 does not promise a new-endpoint setup path there. That UI gap is
recorded as a limit rather than filled with a guessed entry point. No new
Canvas/Core Settings/EQ/CFC release screenshot was captured in this carry.

Unchanged source-sensitive DSP, RF, PA, accessories and native-phone procedures
retain the original reviewed baseline; they need the selected build/equipment
checks below before publication. No unreviewed PA sweep or native-app delivery
is accepted by this documentation commit.

## Presentation verification

Run the reproducible checks documented in [authoring](authoring.md):

```sh
python3 docs/manual/build_preview.py
python3 docs/manual/check_manual.py --preview build/manual-preview
```

The 2 October presentation checks passed for all 20 chapters, 654 local manuscript links/images,
all section targets, SVG/raster validity and original-capture integrity.
These check chapter structure, local manuscript/page/section/asset links,
image alt text, Markdown whitespace, SVG syntax and original-capture integrity.
That reader build passed 21 pages and 1003 local page/asset/section
references. Browser inspection confirmed the 1280-pixel desktop and 390-pixel
phone-width layouts: no page-level horizontal overflow, wide tables scroll
within their own area, all 20 chapter links are present, and section links land
on their headings. That browser inspection covered the earlier prose. No navigation/CSS redesign
was made in the bounded carry; fresh reference/provenance/render checks are
required for its changed prose. The desktop EQ and native phone figures loaded at their intended widths;
portrait images fit the phone reader and link to their full-size originals.
Chapter navigation and section contents start open. All 21 original PNGs
match the recorded SHA-256 values.
Temporary viewport overrides were reset after inspection. This static preview renders the
same Markdown/images with the website stylesheet; it has not been published.

The 4 October carry reran the same commands after correcting contributor-record
links in the preview builder. Fresh checks passed for 20 chapters, 669 local
manuscript references, 21 reader pages, 1019 rendered references and all 21
original-capture hashes. Contributor records remain source downloads rather
than extra reader chapters. The original PNGs, annotations/exports and capture
provenance were unchanged. Chapter and section disclosure controls still carry
`open`; no new browser inspection or release/runtime certification is claimed.

## Evidence still required for publication

Complete the [remaining release screenshot plan](images/README.md) and live
sessions in [authoring](authoring.md). The draft now includes four annotated
overview figures, nine additional desktop captures and eleven native phone/iPad
simulator figures. Test-state pictures do not replace connected station checks. Verify the chosen release build's
pairing, reconnect, ownership/TX handoff, mic/voice route, digital applications,
network paths and hardware/accessory procedures with the relevant equipment.

The PA automatic-calibration interface needs separate validation of its passive
sampling and drive-point progression before an end-to-end sweep can be
recommended. That limitation is documented beside its controls. No RF
transmission, station calibration, device replacement/revocation or external
service configuration was performed during authoring.

Luna medium drafted bounded sections; the controller integrated and checked
them; Sol 6.1 high reviews the comprehensive draft and the bounded correction wave. Usage measurements are
unavailable. No measured monetary saving is claimed.
