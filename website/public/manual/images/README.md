# Manual screenshot inventory

The manual uses real desktop captures and native phone/iPad simulator captures.
Original PNGs are preserved byte for byte. [Capture provenance](capture-provenance.json)
records each original’s dimensions, SHA-256, build, capture method and state.
These builds differ from the original reviewed source baseline, `d1608a3de75e`,
and the bounded desktop reconciliation baseline, `df6590ff12ae`.
The EQ/CFC figures show the earlier editors. They do not show the candidate's
mode buttons, pending count reset, width controls or edit history; those
selected-release captures, Core Settings and Canvas captures remain pending.
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
| [desktop-tci-server-original.png](desktop-tci-server-original.png) | This-window server stopped with localhost binding, separate Core server options. |
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

## Phone and iPad captures

The audio-quality acceptance run on 1 October 2026 built source
`46591d6cd62a916a6eef83e65d12fc655259b44d`. Its frozen-source record was unchanged
after the run. The phone images below come from that run’s native simulator
tests. **Sound** is a screenshot of the full running app driven by
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
`XCUIApplication` screenshot from the 2 October focused TX-panel run. Its source
manifest records accepted revision `0da733c21bc64bdeef40717eaa48cfe259862b97`;
the kit product ledger records `4be63a9d6fd6d168fe6be2e176d419e113834096`.
Both identities are retained rather than reconciled by assumption. It shows
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
