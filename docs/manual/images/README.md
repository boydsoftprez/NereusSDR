# Manual screenshot inventory

The manual uses real desktop captures and native phone/iPad simulator captures.
Original PNGs are preserved byte for byte. [Capture provenance](capture-provenance.json)
records each original’s dimensions, SHA-256, build, capture method and state.
These builds differ from the original reviewed source baseline, `d1608a3de75e`,
and the bounded desktop reconciliation baseline, `df6590ff12ae`.
The 2 October EQ/CFC originals show the earlier editors. The 14 October
workspace-look EQ/CFC captures (below) show the current mode buttons, width
fields and edit history; the pending count reset is not pictured. Selected-release
Core Settings and Canvas captures remain pending.
Screenshots illustrate the stated controls; they are not release compatibility
or live operating verification.

## Desktop captures

Captured through the native app screenshot API on 2 October 2026. The title was
**NereusSDR 0.5.2 · GUI copy codex/all-prs-radxa-gui@c1991b2d42ea [radxa_5c_r3]**.
The connected station was a remote Core with Hermes Lite 2, slice A, LSB receive.
MOX and TUNE were off. Tool windows were opened without starting servers,
reporter feeds, calibration or test transmissions.

The settings shown are the operator’s existing values, not recommended defaults.
During a keyboard navigation attempt the TX high cutoff changed from 3000 to
3001 Hz; it was immediately restored to 3000 Hz and verified on screen. No RF
keying occurred. The overview’s Core-busy message and ADC-overload indication
were actual capture states, not requirements for the procedures.

| Original | Procedure and state illustrated |
| --- | --- |
| [desktop-overview-original.png](desktop-overview-original.png) | Main window in receive, existing 3D spectrum layout. |
| [desktop-audio-devices-original.png](desktop-audio-devices-original.png) | Speakers, Headphones and TX Input cards. No negotiated-stream success is claimed. The Linux backend strip is unavailable on this macOS capture. |
| [desktop-tx-eq-original.png](desktop-tx-eq-original.png) | TX EQ editor; EQ disabled, existing curve preserved. |
| [desktop-cfc-original.png](desktop-cfc-original.png) | CFC editor, 10-band layout, Q factors enabled, Live Update off; no curve edits. |
| [desktop-tx-profile-original.png](desktop-tx-profile-original.png) | TX Profile page with Default active; no Save/Delete operation. |
| [desktop-puresignal-original.png](desktop-puresignal-original.png) | PS and MOX off, pump inactive; no feedback calibration. |
| [desktop-vax-original.png](desktop-vax-original.png) | VAX cards off, no consumers. Backend text is build/platform-specific; no external audio route is demonstrated. |
| [desktop-tci-server-original.png](desktop-tci-server-original.png) | This-window server stopped with localhost binding, separate Core server options (replaced in chapter 9 by the 15 October workspace-look original below; kept unmodified). |
| [desktop-spot-hub-original.png](desktop-spot-hub-original.png) | Identity setup with callsign/grid empty. |
| [desktop-freedv-source-original.png](desktop-freedv-source-original.png) | FreeDV source stopped, Auto-start off; no reporter service connection. |

The four overview figures embed the unaltered overview PNG in SVG compositions
and add vector callouts. Their JPG versions are browser-rendered exports for
repository and website compatibility.

| Figure | Callouts |
| --- | --- |
| `desktop-overview.jpg` | 1 connection and traffic; 2 selected VFO flag; 3 spectrum; 4 waterfall; 5 RX applet; 6 TX applet; 7 status banner. |
| `desktop-vfo.jpg` | 1 frequency entry; 2 filter width; 3 control ownership; 4 slice control tabs. |
| `desktop-rx.jpg` | 1 mode; 2 tuning step; 3 filter display/presets; 4 AGC/AUTO; 5 RIT/XIT. |
| `desktop-tx.jpg` | 1 RF Power; 2 Tune Pwr; 3 TUNE; 4 MOX; 5 monitor output/volume; 6 processing; 7 profile; 8 TX bandwidth. |

## Workspace-look recapture, 14 October 2026

The workspace look is now the default. Six desktop figures were recaptured with
`tests/visual/capture.py --jobs 4 --profiles mac-1512x869@2` and the offscreen
`nereus_visual_preview` tool at git `6350e45ce109d36d41c5f43b8c739d8ad8cb93d9`
(clean tree; the sidecars record it). The tool builds the real window with no
radio and fixed illustrative data, so these show layout and wording, not a
connected station, and no window appeared on screen. The 2 October originals above
stay byte for byte as preserved evidence, with their hashes checked; the manual
now uses the new files where one exists.

| New original | Scene | Replaces in the manual |
| --- | --- | --- |
| [desktop-rx-workspace-original.png](desktop-rx-workspace-original.png) | `look-two-band`, crop (2355,455)-(3024,965) device px | RX applet crop under `desktop-rx.svg` and `desktop-rx.jpg` (callouts 1 to 5 re-placed; SVG embeds this crop, not the full window) |
| [desktop-tx-eq-workspace-original.png](desktop-tx-eq-workspace-original.png) | `look-dialog-tx-eq` | `desktop-tx-eq-original.png` |
| [desktop-cfc-workspace-original.png](desktop-cfc-workspace-original.png) | `look-dialog-tx-cfc` | `desktop-cfc-original.png` |
| [desktop-puresignal-workspace-original.png](desktop-puresignal-workspace-original.png) | `look-dialog-pure-signal` | `desktop-puresignal-original.png` |
| [desktop-spot-hub-workspace-original.png](desktop-spot-hub-workspace-original.png) | `look-dialog-spot-hub`, Settings tab | `desktop-spot-hub-original.png` |
| [desktop-freedv-source-workspace-original.png](desktop-freedv-source-workspace-original.png) | `look-dialog-spot-hub`, FreeDV tab | `desktop-freedv-source-original.png` |

`desktop-audio-outputs.png` (not a preserved original) was replaced in place
from `look-dialog-setup-outputs`, trimmed to the page content (the Settings tree removed).

Not recaptured, still the 0.5.2 classic look: the unreferenced
`desktop-audio-devices-original.png` and `desktop-vax-original.png`.

## Connected-window recapture, 15 October 2026

Four more figures come from the same tool at git
`5ad9b0737ed74207edb5b2091487fccd40eff7c7` (every sidecar records `git_dirty`
false), profile `mac-1512x869@2`, folder `captures/clean/mac-1512x869@2`.
The `look-connected` window is the preview shown as if connected: a TX applet,
the **You control** flag and the ANAN-G2 banner, still with fixed illustrative
data and no radio. The 2 October originals stay byte for byte.

| New original | Scene | Replaces in the manual |
| --- | --- | --- |
| [desktop-overview-workspace-original.png](desktop-overview-workspace-original.png) | `look-connected`, the full window, unmodified | `desktop-overview.svg` and `.jpg` (7 callouts re-placed; the SVG embeds a JPEG copy of this window, the `.jpg` is a 1512 px render) |
| [desktop-vfo-workspace-original.png](desktop-vfo-workspace-original.png) | `look-connected--flag@3x.png`, the tool's own crop of slice A's flag | `desktop-vfo.svg` and `.jpg` (4 callouts re-placed) |
| [desktop-tx-profile-workspace-original.png](desktop-tx-profile-workspace-original.png) | `look-dialog-setup-tx-profile-active`, crop (404,0)-(1800,520) px, Settings tree removed | `desktop-tx-profile-original.png` in chapter 14 |
| [desktop-tx-workspace-original.png](desktop-tx-workspace-original.png) | `look-connected-tx--right-panel`, crop (0,78)-(612,1012) px: TX applet title through **SWR Prot** | `desktop-tx.svg` and `.jpg` (8 callouts re-placed; the SVG embeds a JPEG copy, q88) |
| [desktop-tci-server-workspace-original.png](desktop-tci-server-workspace-original.png) | `look-dialog-remote-setup-tci-server`, crop (404,0)-(1800,1520) px, Settings tree and the groups below **The Core's TCI server** removed | `desktop-tci-server-original.png` in chapter 9 |

The two rows above come from git `e874587d9f96b39737a84b4ff3f7f4df1d7354f8` (sidecars `git_dirty` false), folders `captures/clean-tx` and `captures/clean-tci`, same profile.

`desktop-modes-audio.png` (not a preserved original) was replaced in place from
`look-dialog-setup-digital-modes-full`, crop (404,0)-(1800,2436) px, Settings
tree removed.

## Native audio captures

Rendered on 9 October 2026 by the native audio engines plan's widget and phone-panel tests: the Setup pages offscreen on a Mac with fake device lists, and the Sound panel in the iOS simulator with scripted Core state. No sound device was opened and no radio was connected, so they show wording and layout, not a working stream. The Outputs and Digital modes captures are trimmed to their content.

| Image | State illustrated |
| --- | --- |
| [desktop-audio-outputs.png](desktop-audio-outputs.png) | Audio > Outputs on a Mac, no radio: This computer, Headphones off, Radio speaker grayed, Rescan devices grayed. |
| [desktop-modes-audio.png](desktop-modes-audio.png) | Audio > Digital modes on a Mac: four VAX cards off, TCI settings. |
| [iphone-sound-core-speaker.png](iphone-sound-core-speaker.png) | Sound panel with Radio speaker 40, Core speaker 60, AirPods route. Recaptured on 22 October 2026 in the workspace look, with the workspace switch, by `SoundPanelTests` (see Phone and iPad captures). |

## Phone and iPad captures

These images were recaptured on 21 October 2026, when the phone took the
desktop's workspace look, from the native simulator tests each row names, at
the sizes of the 1 October originals: an iPhone 17 simulator for most, an
iPhone 18 Pro Max simulator for **Sound**. On 22 October, after the final
review's fixes (the workspace switch in every presentation, accent text and
the 7 and 10 radii), all of them except Devices were recaptured again from
branch `claude/ios-workspace-look` at `45c52b2dd`; Devices did not change. The two
Sound images were recaptured once more at `fb8cebe45`, after the re-review
(the Sound switch rows greyed once, the output row's corner at 7). After the
7 and 10 radii (W11), Shared slices, Sound, Spot Hub, Devices, Radio, Mic muted
and the iPad column were recaptured again on 22 October: the iPhone 17 images
at `e20d70b38`, Sound and the iPad column at `d3207d2bb`. [Capture provenance](capture-provenance.json)
records each image's test, hash and build. **Sound** is a screenshot of the full running app driven by
`XCUIApplication`. The other phone images render the application’s actual
SwiftUI screens in a simulator window with test services or simulated Core
data. They are not HTML design boards. They do not prove received audio,
radio telemetry, pairing or RF transmission.

| Original | Native capture test and purpose |
| --- | --- |
| [iphone-welcome-original.png](iphone-welcome-original.png) | `ConnectionFlowShotTests`: Find my Core / Set up a Core. |
| [iphone-shared-slices-original.png](iphone-shared-slices-original.png) | `SeveralDevicesScreenTests`: foreign slice and Take control, synthetic waterfall. |
| [iphone-sound-original.png](iphone-sound-original.png) | `SoundPanelUITests`: Sound popover, Earpiece selected, no live Core audio. |
| [iphone-tools-original.png](iphone-tools-original.png) | `ToolsPagesShotTests`: tool list and capability labels. |
| [iphone-spot-hub-landscape-original.png](iphone-spot-hub-landscape-original.png) | `SpotScreenTests` / `ToolsPagesShotTests`: landscape Spot Hub, no spots. |
| [iphone-devices-original.png](iphone-devices-original.png) | `SetupRenderingTests`: Connected now / Paired; synthetic identity/device names. |
| [iphone-navigation-original.png](iphone-navigation-original.png) | `SetupRenderingTests`: local tuning/touch preferences. |
| [iphone-data-use-original.png](iphone-data-use-original.png) | `SetupRenderingTests`: Wi-Fi/cellular modes, test-start zero totals. |
| [iphone-radio-landscape-original.png](iphone-radio-landscape-original.png) | `RadioTabTests`: landscape Radio tab, simulated ANAN-G2 identity/telemetry. |
| [iphone-mic-muted-original.png](iphone-mic-muted-original.png) | `TxStageMetersTests`: scripted mic mute and missing TX readings. |

[ipad-applet-column-original.png](ipad-applet-column-original.png) is a full-app
`XCUIApplication` screenshot by `TxPanelUITests` on an iPad Pro 13-inch (M5)
simulator, recaptured on 21 October 2026 at `4ff934ec0` in the workspace look and
again on 22 October at `d3207d2bb` after the 7 and 10 radii. It shows
the applet-column arrangement in a test scenario with unavailable Core
readings, so it illustrates layout only. Redundant dim TX-panel captures are
not used as distinct operating states.

Capture logs, result bundles and exact test/source-line evidence were inspected
in the contributor workspace. The committed provenance file keeps original
file paths and byte hashes so contributors can trace and verify the chosen
assets. Original images contain no pairing code, credential or private network
address. Simulated station identities in fixtures are identified as such.

## Remaining screenshot work

These figures improve the draft but do not finish the release screenshot plan.
Capture the selected release build and record a complete connected state for:

| Area | Missing figure/state |
| --- | --- |
| Connections and pairing | Desktop Connections and Remote Access; phone Core selection/pairing confirmation with enrollment secrets excluded. |
| Slices and sharing | Desktop chooser with listener/controller roles; actual supported receiver and TX takeover confirmations. |
| Voice | TX Input and Speech Processor with receive-side mic test; working stage/readback state. |
| DSP and digital | Neural model/readiness page; active VAX RX and separate TX endpoints; TCI client list; RADE sync/profile. |
| Display | Spectrum/history/LIVE controls and container editing/clipboard actions. |
| Hardware/accessories | Antenna/HL2/PA, diversity, PGXL/TGXL/RF-Kit, tuner memory and completion. |
| Mobile daily operation | Live portrait/landscape receive, iPad two-slice portrait columns, Modes, interruption and reconnect. |
| Diagnostics | Desktop and phone performance pages with attributable observed state. |

No usable running Simulator UI was available during this authoring session.
The existing native test captures are therefore labeled precisely instead of
being presented as a new live walkthrough. Fault, keying and calibration
captures require a deliberate station session with the equipment and operator.
