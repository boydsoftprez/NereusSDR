# Settings persistence audit

Started 2026-11-05 by J.J. Boyd (KG4VCF), who reported, in his words: "whenever you
open it back up, it always starts from a what seems like predetermined baseline" on the
iPhone, and "I'm constantly having to set the sample rate back to 1.5 MHz, pretty much
every time I open the application or open a new slice."

This records everything a user can set, where it is kept, who owns it, and whether it
survives. The rules it is measured against are in `CLAUDE.md` under Settings.

## How to read this

- **The tables describe `main` at `dfa9e1c29`, as found, before any fix.** They are a
  reading of the source. Nothing was run on a radio, and no one's real settings were
  read. Line numbers are for that commit. What changed afterward is in
  [Findings and fixes](#findings-and-fixes).
- Each survival column is **sticks**, **reverts**, **n/a** or **unknown**. A row's
  verdict is **reverts** when a user would see their choice lost in some normal case.
  **Unknown** means the code was not followed far enough to say.
- Evidence is `file:line` for the save and the restore. Paths under `ios/` are written
  relative to `ios/` in that section.

## Summary

| Area | Rows | Sticks | Reverts | By design | Unknown |
| --- | ---: | ---: | ---: | ---: | ---: |
| Desktop: Setup dialog | 78 | 53 | 1 | 9 | 15 |
| Desktop: main window, display and layout | 55 | 43 | 8 | 4 | 0 |
| Receiver and transmit state (desktop and Core) | 49 | 31 | 7 | 4 | 7 |
| Core, stations and connect lists | 41 | 13 | 5 | 10 | 13 |
| iPhone and iPad | 59 | 31 | 16 | 5 | 7 |
| **All** | **282** | **171** | **37** | **32** | **42** |

## The two named reports

### The sample rate falling back

The rate in question is 1536 kHz. It is only on the Protocol 2 ladder
(`src/core/BoardCapabilities.cpp:408`), so this is Protocol 2 behavior, on a desktop and
on a Core alike. Five separate things put an old rate back. Each was shown failing by a
test before it was changed (`tests/tst_sample_rate_sticks.cpp`,
`tst_dsp_control_receive`).

1. **Changing band.** A rate was saved per slice per band, and a band change applied the
   band's old rate to the receiver (`RadioModel.cpp:20152-20156`). Each band and each
   slice letter kept its own old rate.
2. **Opening a new slice.** A slice that claimed a receiver of its own opened at the
   radio-wide connect rate (`RadioModel.h:8110`, `RadioModel.cpp:15596-15599`). A change
   from the slice flag moves one receiver and never updates that radio-wide value.
3. **A window or phone coming back to a Core.** The Core makes its saved slice again
   through `restoreSliceFor`, which skipped the rate
   (`RadioModel.cpp:17229`), so the slice took the connect rate and the next save wrote
   that over the saved one.
4. **Setup's rate.** A radio-wide change did not save on the slices, so at the next
   connect the older slice value moved the receiver back
   (`RadioModel.cpp:35292-35296`, `14813`).
5. **A slice leaving a shared receiver.** A slice tuned outside the receiver it shared
   claimed another one at the radio-wide connect rate, as in 2.

The ruling, from JJ on 2026-11-05: the rate stays what you set, on every band. Each
receiver remembers one rate, across restarts too. Thetis has only Setup's rate, and a
band change leaves it alone (`setup.cs:7066` [v2.10.3.15]).

### The iPhone reopening at a baseline

Backgrounding loses nothing, because the connection stays up. The baseline appears when
the process has ended (a force close, iOS ending it, or a new build). The app kept no
last Core, tab, page or panel, so it opened on Your Cores, waited for a tap, then
showed Panadapter with everything closed. The settings themselves mostly did stick. The
detail is under [iPhone and iPad](#iphone-and-ipad).

The ruling, from JJ on 2026-11-05: an auto reconnect setting. On, a cold launch
reconnects to the last used Core. Off, it opens on Your Cores. Either way, connecting
returns you to your place. Whether a display element such as a panel was open is not
persistent.

## Findings and fixes

Commits are on this document's branch. A row under Fixed has a test that failed before the
change and passes after, with these exceptions: the iPhone Main screen layout wiring and the
Connection panel path have no test of their own; `aPanLeftFloatingFloatsAgainWhenTheWindowOpens`
was written after its fix (its stack-level sibling was seen failing first). Nothing here was run
on a real radio or a real GPU.

### Fixed

| What the user saw | Cause | Fix |
| --- | --- | --- |
| Sample rate drops on a band change | Rate saved per slice per band | One rate per receiver (`hardware/<mac>/slices/<N>/SampleRate`); a band change leaves it alone. Older per-band values still supply the rate of the last band |
| New slice with its own receiver opens at 192 kHz | New receiver takes the radio-wide rate | Opens at the rate of the slice it was seeded from |
| Window or phone back on a Core loses its rate | `restoreSliceFor` skipped the rate, then the next save overwrote it | Comes back at its saved rate, unless that would move a receiver someone else is on |
| Setup's rate undone at the next connect | Not saved on the slices | Saved on each slice |
| Two radios overwrite each other's rate | One key for a slice letter across all radios | Each radio keeps its own, `hardware/<mac>/slices/<N>/SampleRate`, as `CLAUDE.md` asks |
| Two slices sharing a receiver fight at connect, and 192 kHz wins over 1536 kHz | Each slice applied its own saved rate in turn | One rate per receiver: the highest saved on it |
| A slice leaving a shared receiver drops to 192 kHz (tuned out, moved with its pan, or pushed off when the rate changes) | Same radio-wide rate | Keeps its rate on all three paths |
| A change is lost on a crash, kill or power cut | Most writes reached disk only at the next unrelated save or a clean stop | `AppSettings` saves about 2 s after the first unsaved change, on every role. Values handed to it only at close are still lost on a crash or SIGKILL (see Not fixed) |
| A locked slice comes back on 14.225 MHz and damages the 20 m memory | The restore used the setter the lock blocks | The restore assigns past the lock |
| A Core stops saving receiver settings for its whole run | A kept (protected) layout blocked every slice save | Only the layout record is held back; letters refused at admission keep their keys |
| A custom TX filter is lost going through RADE | Entry overwrote it with nothing kept | The filter from before RADE comes back, across a restart too |
| Diversity phase, gain and fine null can be lost at quit | No save scheduled for them | Scheduled |
| A crash during Fixed-source TUNE or two-tone would leave the temporary power on disk as that band's drive | An old in-memory path, exposed by the timed save above | The band-power write is held while a temporary power is active. Memory and on-air behavior are unchanged |
| A device leaving with the radio off loses its slices | The close deleted the keys with nowhere to save them | The slice is held for that device |
| A sign-in to a headless Core with the radio off misses its saved slices | The wait applied only to a Core inside a window | Applies to a headless Core too, for paired devices |
| Connecting turns a radio's Auto-connect back on | Both paths passed a fixed true | Set on first connect only; after that the saved choice stays |
| iPhone opens on a baseline after the app has ended | Nothing about your place was saved | Reconnect automatically, plus tab, page, mute and layouts (JJ's ruling above) |
| iPad open pans are lost | Never saved | Reopened from the slices and a saved list |
| Play the band through is not kept | Memory only | Saved; an absent device falls back to iOS without losing the choice |
| A background picture can vanish after an app update | Saved as an absolute path | Saved relative to Application Support |
| The panel hidden with the bottom bar's icon comes back | Only held in memory | Saved at the click and restored, in both looks |
| A pan floated onto a second monitor comes back docked | Geometry was saved, the fact it floated was not | Floats again after a restart; an absent screen falls back as the geometry code already does |
| Zoom slider sits at full right while the span is narrow, and the first touch jumps | The slider did not follow the span | Follows the active pan's span |
| FFT decimation returns to 1 at every launch | No key | Saved, and restored with a range check |
| A pan's overlay strip collapse is lost | No key | Saved for each pan |
| The dividers inside a pan row are lost (12h, 2h1, 2x2, 3h2) | Only the top-to-bottom split was saved | Each row saves and restores; sizes that no longer fit are ignored |
| Spots off in Spot Hub still draws spots on the second pan | Saved settings reached only the first pan | Reach every pan, and a pan opened later |
| A signal quit (SIGTERM, a session logout) loses the layout | Layout written only by closeEvent | The quit path writes the same values once. A crash or SIGKILL runs no handler, so this does not help there |

### Sweep findings that were wrong when checked

The tables below still show these as found.

- **Multimeter polling delay after a relaunch.** It is applied at window startup
  (`MainWindow.cpp:9995`). A test now pins that.
- **`LineInGain` never loaded.** It is deliberately derived from line-in boost
  (commit `f8cc4a379`).
- **`CLAUDE.md` against the code on antennas, TX power, ADC attenuation and preamp.** Three
  design calls below say the rule calls these never persisted. It does not: the rule says they
  are saved and sent on connect, which is what the code does. Nothing to decide.

### Not fixed, and why

- **A reused slice letter brings another band's AGC, NB and step onto a new slice, then saves
  them over that band's memory** (`RadioModel.cpp:17167`). Certain. Left alone here because
  that block is being changed for the new-receiver RIT and XIT work, and because the fix
  depends on the open question of what a new slice inherits.
- **Transmit edits made before the first connect of a run are dropped.** Likely, on a desktop
  and on a Core. `TransmitModel::persistOne` does nothing while `m_persistMac` is empty
  (`TransmitModel.cpp:2288`), and `loadFromSettings` then overwrites every field at connect.
  Not confirmed: which transmit controls are live with no radio. If they are all disabled
  offline this shrinks to mic source, which already has its own fix.
- **Values handed to the store only at close** (`PanLayoutId`, splitter sizes, window
  geometry) are lost on a crash or SIGKILL, because no handler runs. Writing them as they
  change is a larger change to the window and was not made here.
- **Unknown rows stay unknown.** Rows marked unknown in the tables were not settled by reading
  the code, and nothing here was run on a real radio.

### Reverts rows waiting on a design call

These are behaviors to choose, not defects, so they stay as they are until the maintainer
rules.

| Row | What happens now | Design call |
| --- | --- | --- |
| Tune step (desktop and Core) | Kept per slice and band, so it changes on a band change. Thetis keeps one | Models 3 |
| Extra slices on a desktop with no Core | One slice plus one per empty pan comes back; a Core brings back all | Models 2 |
| Which slice transmits | Follows the extra slices above: falls back when its slice is not restored | Models 2 |
| What a new slice inherits | A copy of defaults and the seed, then a reused letter's values | Models 1 |
| The open tab on a VFO flag | None at each launch | Main window 1 |
| iPhone band pan position | Back to the slice at each connect | iPhone DC6 |
| iPhone Setup's Display page scope and chosen band | Reset at each launch | Not raised separately |
| iPhone slice shown on this phone only, and its return slice | Reset when the session ends | Not raised separately |
| iPhone volume on a listened slice | Ends with the listening claim | Not raised separately |

The maintainer ruled that open panels and sheets are not persistent, so those rows stay
**reverts** in the tables and are as intended. Waterfall history on the phone is not kept, which
the phone's design call 8 recommends leaving.


## Desktop: Setup dialog

Scope: every leaf registered in `SetupDialog::buildTree()` (src/gui/SetupDialog.cpp:1714-2380), the models and services those pages write, and how each behaves in a window connected to a remote Core. Source only, at dfa9e1c29. Nothing was run.

Reading notes that apply to every row:

- `AppSettings::setValue` writes memory only (src/core/AppSettings.cpp:1133-1149). Disk is written by an explicit `save()`, by the 500 ms slice save (src/models/RadioModel.cpp:29345-29384), by close, quit and session end (src/gui/MainWindow.cpp:2346, 2376, 21413). "Sticks" for Reopen means a normal quit. A crash can lose an unflushed Setup edit (bug 2).
- In a remote window, a key that `classifySettingsKey` calls Station goes through `SettingsProxy` to the Core (src/core/AppSettings.cpp:1140-1143, src/core/settings/SettingsProxy.cpp:117-132). Everything else stays in this computer's file. Core restart below means a clean SIGTERM; bug 2 covers an unclean stop.
- Phone background is n/a for every row: this table is the desktop window. Rows are grouped. Where I read the store and the control but not the code that applies the value to the radio, the row says so and the verdict is unknown.
- "SD" means the remote Setup description service (src/core/setup/SetupDescriptionService.cpp), which publishes the same settings to apps. It validates each published row against `classifySettingsKey` (for example :1717, :1766, :1771, :2681, :2786) so it cannot publish a key the Core does not own.

| Surface | Setting | Store (exact key or file) | Owner | Saved when | Restored when | Reopen | Reconnect | Core restart | Band change | Add slice/pan | Phone background | Verdict | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| General > Startup | Auto-connect, local window | `radios/<mac>/` autoConnect via `setRadioAutoConnect` | window, per radio | toggle, with `save()` | page build (control); launch reader not read | sticks | n/a | n/a | n/a | n/a | n/a | unknown | Save GeneralSetupPages.cpp:73-78; control :60-66. `MainWindow::tryAutoReconnect` not read. |
| General > Startup | Auto-connect, remote window | `ConnectionTargets/` via `CoreTargetStore` | window, per Core | toggle, `store.upsert` | page build | unknown | n/a | n/a | n/a | n/a | n/a | unknown | GeneralSetupPages.cpp:34-58. Store's flush not read. |
| General > Startup | Callsign, grid square | `User/Callsign`, `User/GridSquare`, copies per spot source | Core (remote) or window (local) | apply, then background save | page build; spot sources | sticks | sticks | sticks | n/a | n/a | n/a | sticks | Control GeneralSetupPages.cpp:96-118; store SpotHubDialog.cpp:510-511; applied and saved RadioModel.cpp:7473-7486, 4813. |
| General > Options | Band plan region | `BandPlanRegion` | Core or window | change | page build; model | sticks | sticks | sticks | n/a | n/a | n/a | sticks | GeneralOptionsPage.cpp:674, :661; RadioModel.cpp:24952. Refused on the air by StationServer.cpp:9530-9543. |
| General > Options | Extended transmit, prevent TX on other band | `ExtendedTransmit`, `PreventTxOnDifferentBandToRx` | Core or window | change | page build; transmit gate reads live | sticks | sticks | sticks | n/a | n/a | n/a | sticks | GeneralOptionsPage.cpp:707, :887; scope SettingsScope.cpp:589, :597. |
| General > Options | Receive only | `RxOnly` | Core or window | change | `applyRxOnly` | sticks | sticks | sticks | n/a | n/a | n/a | sticks | GeneralOptionsPage.cpp:754; RadioModel.cpp:34627, :34634. |
| General > Options | Network watchdog | `NetworkWatchdogEnabled` | Core or window | change | page, connect | sticks | sticks | sticks | n/a | n/a | n/a | sticks | GeneralOptionsPage.cpp:803, :789; RadioModel.cpp:20240, :20248. |
| General > Options | Hide feedback level, invert PSA colors | `HideFeedbackLevel`, `InvertRedBluePsa` | window | change | widget and page build | sticks | sticks | n/a | n/a | n/a | n/a | sticks | GeneralOptionsPage.cpp:916, :937, :1712, :1718; PsaIndicatorWidget.cpp:230-231. |
| General > Options | CPU meter rate | `GeneralCpuMeterUpdateRateHz` | window | change | MainWindow build | sticks | n/a | n/a | n/a | n/a | n/a | sticks | GeneralOptionsPage.cpp:1005, :1000; MainWindow.cpp:14918. |
| General > Options | Transmit time-outs | `MoxTimeOutEnabled/Seconds`, `PingTimeOutEnabled/Seconds/Host`, `RemoteMoxTimeOut*` | Core or window | change. Ping saves nothing while the host does not parse, as Thetis | read each tick of the time-out | sticks | sticks | sticks | n/a | n/a | n/a | sticks | GeneralOptionsPage.cpp:1164, :1200-1201, :1211, :1058, :1113; RadioModel.cpp:31587. |
| General > Filters | Limit filter edges to sidebands | `LimitFiltersToSidebands` | Core or window | `setLimitFiltersToSidebands` | connect, every new slice | sticks | sticks | sticks | n/a | sticks | n/a | sticks | GeneralFiltersPage.cpp:105; RadioModel.cpp:20267-20269, :20256, :16877. |
| General > This Core, Cores > Your Cores, Audio with the Core | Core identity, targets, audio details | `ConnectionTargets/`, Core commands | window or Core | not audited in depth | not audited | unknown | unknown | unknown | n/a | n/a | n/a | unknown | Registered SetupDialog.cpp:1808-1833. Left to the Core and remote sweeps. |
| Hardware > Radio Info | Sample rate | `hardware/<mac>/radioInfo/sampleRate` | per radio MAC | change, with `save()` | page build; connect | sticks | sticks | sticks | n/a | n/a | n/a | sticks | HardwarePage.cpp:319-320; RadioInfoTab.cpp:343, :397-409; SampleRateCatalog.cpp:110; RadioModel.cpp:35511. Without a radio the edit is dropped (bug 3). |
| Hardware > Radio Info | ANAN-8000DLE show volts and amps | `HardwareAnan8000DleShowVoltsAmps` | window | change | MainWindow build | sticks | n/a | n/a | n/a | n/a | n/a | sticks | RadioInfoTab.cpp:233, :227; MainWindow.cpp:14603. |
| Hardware > Antenna | Per-band antenna tables, Block TX, relays | `hardware/<mac>/alex/antenna/...` | per radio MAC, Core-owned | AlexController save, flushed by slice save and teardown | connect | sticks | sticks | sticks | n/a | n/a | n/a | sticks | AntennaAlexTab.cpp:264-270 comment; RadioModel.cpp:29630; scope SettingsScope.cpp:764-776. Rule conflict: design call 1. |
| Hardware > Antenna | Antenna conflict policy | `Antenna_ConflictPolicy` | window | change | page build | sticks | sticks | n/a | n/a | n/a | n/a | sticks | AntennaAlexAntennaControlTab.cpp:935, :950. No reader outside the tab, so it changes nothing. On a Core page but stored per window (design call 5). |
| Hardware > Alex-1 and Alex-2 | HPF, LPF, BPF edges and bypass | `hardware/<mac>/alex/{hpf,lpf,bpf1}/...`, `alex2/...` | per radio MAC | change, with `save()` | page build; apply path only partly read | sticks | unknown | unknown | n/a | n/a | n/a | unknown | AntennaAlexAlex1Tab.cpp:1037-1118; AntennaAlexAlex2Tab.cpp:616-663; RadioModel.cpp:36482; keys core/AlexSettingsKeys.h:90-99. Controller load not read. |
| Hardware > OC Outputs | OC matrix | `hardware/<mac>/oc...` via OcMatrix | per radio MAC | page's matrix path not read in full; save at teardown | connect | unknown | sticks | unknown | n/a | n/a | n/a | unknown | RadioModel.cpp:20886, :20933; OcOutputsHfTab.cpp matrix wiring. |
| Hardware > OC Outputs | USB BCD, external PA, hot switching | `hardware/oc/usbBcd/*`, `hardware/oc/extPa/*`, `hardware/oc/allowHotSwitching` | none in effect | change | never | n/a | n/a | n/a | n/a | n/a | n/a | by design | Hidden until built: OcOutputsHfTab.cpp:202, :321, :370. Writes at :346-357, :399-405, :481, no reader anywhere. If unhidden these will revert (bug note in design call 4). |
| Hardware > Calibration | Frequency factor, 10 MHz ref, LNA offsets, TX offset, PA current, log volts | CalibrationController keys under `hardware/<mac>/` | per radio MAC | change, `m_calCtrl->save()` | connect (`setMacAddress`, load) | sticks | sticks | sticks | n/a | n/a | n/a | sticks | CalibrationTab.cpp:606-709; CalibrationController.cpp:447, :533; RadioModel.cpp:20960. Tab also emits duplicates to `paCalibration/cal/*`, never read back: CalibrationTab.cpp:843-847. |
| Hardware > HL2 Options | Clock and option rows | Hl2OptionsModel keys per MAC | per radio MAC | model `save()` | connect | unknown | sticks | unknown | n/a | n/a | n/a | unknown | Hl2OptionsTab.cpp:256, :414; Hl2OptionsModel.cpp:151; RadioModel.cpp:20956. Per-control save not read. |
| Hardware > HL2 I/O | N2ADR filter | `hardware/<mac>/hl2IoBoard/n2adrFilter` | per radio MAC | change, with `save()` | page build | sticks | unknown | unknown | n/a | n/a | n/a | unknown | Hl2IoBoardTab.cpp:1231, :1390; HardwarePage.cpp:319-320. Radio apply not read. |
| Hardware > XVTR, Bandwidth Monitor, DDC Routing | Transverter rows, throttle, routing | `xvtr/*`, `bandwidthMonitor/*`, DDC keys | none in effect | change | page only | n/a | n/a | n/a | n/a | n/a | n/a | by design | Hidden until built: HardwarePage.cpp:203-205, SetupDialog.cpp:1864. No reader outside the tabs. |
| Hardware | Active RX count | `hardware/<mac>/radioInfo/activeRxCount` | per radio MAC | no control writes it | connect | n/a | n/a | n/a | n/a | n/a | n/a | by design | Widget removed: RadioInfoTab.cpp:411-412; reader SampleRateCatalog.cpp:129. |
| PA > PA Gain | Profiles, active profile, gains | `hardware/<mac>/` manifest, active key, blobs | per radio MAC | edit, `saveProfile` | connect | sticks | sticks | sticks | n/a | n/a | n/a | sticks | PaSetupPages.cpp:1418, :1457, :1599; PaProfileManager.cpp:546, :704, :712, :231; RadioModel.cpp:21023. |
| PA > Watt Meter | Show PA Values page | `display/showPaValuesPage` | window | change | dialog build | sticks | n/a | n/a | n/a | n/a | n/a | sticks | PaSetupPages.cpp:2307, :2301; SetupDialog.cpp:2429. |
| PA > PA Values | Peak and minimum readouts | none | none | never | never | n/a | n/a | n/a | n/a | n/a | n/a | by design | Live telemetry; `resetPaValues`, SetupDialog.cpp:1932-1938. |
| Audio > Outputs | PC volume, mute | `audio/Master/Volume`, `audio/Master/Muted` | window | change, with `save()` | master widget build | sticks | n/a | n/a | n/a | n/a | n/a | sticks | AudioOutputsPage.cpp:264-265, :275-276; MasterOutputWidget:350-352. |
| Audio > Outputs and Microphone | Speakers, headphones, PC mic device, backend, buffer | `audio/Speakers/*`, `audio/Headphones/*`, `audio/TxInput/*` | window (a Core has its own, conf file) | engine setter, `saveToSettings` | engine open | sticks | sticks | n/a | n/a | n/a | n/a | sticks | AudioEngine.cpp:2455-2462, :1556, :675-679; DeviceCard.cpp:997, :448-451. |
| Audio > Outputs | Radio speaker rows | Core properties, Core's coalesced save | Core | command | Core | unknown | unknown | unknown | n/a | n/a | n/a | unknown | StationServer.cpp:143, :2112 comments; SD :321. Not traced. |
| Audio > Microphone | Mic gain, boost, XLR, tip ring, bias, PTT, line in | `hardware/<mac>/tx/MicGain`, `Mic_Input_Boost`, `Line_Input_On`, `Line_Input_Level`, others | per radio MAC | setter, `persistOne` | connect | sticks | sticks | sticks | n/a | n/a | n/a | sticks | AudioTxInputPage.cpp:951, :1215-1310; TransmitModel.cpp:2286-2294, :2296-2324, :718, :738; RadioModel.cpp:20997. Dropped without a radio (bug 3). |
| TransmitModel | Line in gain (no desktop Setup row found) | `hardware/<mac>/tx/LineInGain` | per radio MAC | setter | never | unknown | unknown | unknown | n/a | n/a | n/a | unknown | Written TransmitModel.cpp:810, :2869; `loadFromSettings` has no read. Default 23 at TransmitModel.h:3125. Caller list not exhausted. See bug 4. |
| Audio > Digital modes | VAX channel devices, enable, last detected cables | `audio/Vax<N>/*`, `audio/LastDetectedCables`, `audio/LastDetectedCablesSource` | window | change, with `save()` | engine open, page | sticks | sticks | n/a | n/a | n/a | n/a | sticks | AudioVaxPage.cpp:396-399, :620-622, :650, :694, :1948-1951; AudioEngine.cpp:1348; VaxFirstRunCheck.cpp:93-102. |
| Audio > Digital modes | TCI audio stream shape, TX channel, buffering | `TciAudioStream*`, `TciSliceA_OutputSampleRate`, `TciTxChannel`, `TciTxStreamBufferingMs` | window | change | page; server per session | sticks | sticks | n/a | n/a | n/a | n/a | sticks | AudioTciPage.cpp:117, :160, :184, :203, :238, :255; TciServer.cpp:2654-2697, :2344. |
| Audio > TX Profile | Mic profiles, active name | `hardware/<mac>/` mic profile manifest and blobs | per radio MAC | Save button or prompt, not on each edit | connect loads list; values come from `tx/` keys, not the profile | sticks | sticks | sticks | n/a | n/a | n/a | sticks | TxProfileSetupPage.cpp:357, :453; MicProfileManager.cpp:883, :1071-1098; RadioModel.cpp:8053-8054, :8123. |
| Audio > Advanced | Send IQ, TX monitor, mute VAX, DSP rate and block | `audio/SendIqToVax`, `audio/TxMonitorToVax`, `audio/MuteVaxDuringTxOnOtherSlice`, `audio/DspRate`, `audio/DspBlockSize` | window or Core | change | page | n/a | n/a | n/a | n/a | n/a | n/a | by design | Hidden until built: AudioAdvancedPage.cpp:138, :325-333. |
| DSP > AGC/ALC | AGC mode, threshold, hang, slope, attack, decay, max and fixed gain, auto offset | `Slice<N>/Band<key>/Agc*` | per slice-band | 500 ms slice save | connect, band change | sticks | sticks | sticks | reverts | n/a | n/a | sticks | DspSetupPages.cpp:473; SliceModel.cpp:3515-3574, :3731-3745; RadioModel.cpp:26048. Band change loads that band's own values: design call 3. |
| DSP > NR/ANF | NR1, NR2, NR4, MNR, DFNR tuning and positions | `Slice<N>/Nr*`, `Mnr*`, `Dfnr*`, `hardware/<mac>/slices/.../nnr/` | per slice | 500 ms slice save | connect | sticks | sticks | sticks | sticks | n/a | n/a | sticks | DspSetupPages.cpp:1203-1225, :1442-1454; SliceModel.cpp:3591, :3609, :3618, :2216; RadioModel.cpp:26131-26159, :26284-26292. Save-trigger sweep found no missing save for these. |
| DSP > NB/SNB | NB1 threshold, transition, lead, lag, NB2 mode, SNB K1, K2, output bandwidth | `Slice<N>/Nb1*`, `Nb2Mode`, `Snb*`; old defaults `NbDefault*`, `SnbDefault*` | per slice | 500 ms slice save | connect | sticks | sticks | sticks | sticks | n/a | n/a | sticks | DspSetupPages.cpp:2101-2110; SliceModel.cpp:3651-3656, :4073-4087; RadioModel.cpp:26368-26376, :26402. |
| DSP > CW | Keyer, break-in, sidetone, delays, serial lines | `hardware/<mac>/tx/CW_*` and siblings; pitch `CWPitch` | per radio MAC; pitch Core | setter | connect | sticks | sticks | sticks | n/a | n/a | n/a | sticks | TransmitModel.cpp:2828, :4825-4838; setter sweep found these persist. |
| DSP > CW | APF | `Slice<N>/ApfEnabled`, `ApfTuneHz` | per slice | slice save | connect | sticks | sticks | sticks | sticks | n/a | n/a | sticks | SliceModel.cpp:3684-3685, :4195-4196. |
| DSP > AM/SAM, FM | Squelch, FM offset, CTCSS, reverse, digital offsets | `Slice<N>/Band<key>/*sq*`, `Slice<N>/Fm*`, `DiglOffsetHz`, `RttyMarkHz` | per slice, some per band | slice save | connect | sticks | sticks | sticks | sticks | n/a | n/a | sticks | SliceModel.cpp:3690-3693, :4218-4221. |
| DSP > CFC, Transmit > Speech Processor | CFC bands, phase rotator, CESSB | `hardware/<mac>/tx/*`, CFC profile | per radio MAC | setter | connect | sticks | sticks | sticks | n/a | n/a | n/a | sticks | Setter sweep over TransmitModel.cpp; `loadFromSettings` :2296-2849. JSON setters `setCfcCompressionJson`, `setCfcProfile` are link-side and hold no save themselves; not traced further. |
| DSP > TNF | Notch list and flags | `NotchCount`, `Notch<N>*`, `NotchGlobalEnabled`, `NotchAutoIncrease`, `NotchVisualEnabled` | Core or window | NotchModel | connect | unknown | unknown | unknown | n/a | n/a | n/a | unknown | Scope SettingsScope.cpp:734-746. Model load and save not read (another sweep's area). |
| DSP > Filter Presets | 10 slots by 12 modes | `filters/<mode>/<slot>/{name,low,high}` | Core or window | edit | store read | sticks | sticks | sticks | n/a | n/a | n/a | sticks | FilterPresetsSetupPage.cpp:376, :391, :401; FilterPresetStore.cpp:58-62, :67, :85; scope SettingsScope.cpp:525. |
| DSP > Options | Buffer and filter sizes, filter type, impulse cache | `DspOptions*` | Core or window | change | RxChannel at mode change; WdspEngine | sticks | sticks | sticks | n/a | n/a | n/a | sticks | DspOptionsPage.cpp:416, :196, :211; RxChannel.cpp:3951-3959; WdspEngine.cpp:513-514; StationServer.cpp:9570. |
| DSP > Options | High resolution filter characteristics | `DspOptionsHighResFilterCharacteristics` | Core or window | change | MainWindow | sticks | sticks | sticks | n/a | n/a | n/a | sticks | DspOptionsPage.cpp:718, :325; MainWindow.cpp:15541, :15624. |
| Display > Spectrum, Waterfall, View, Colors | Detector, averaging, line width, fill, trace and other colors, grid, overlays, waterfall levels, rate, history, 3D | `Display*` bare key is the default, `Display*_<pan>` a pan's own (src/core/settings/PanDisplayKeys.h); DisplaySettingsModel keys | window, per pan | master write now; pan's sparse keys through 500 ms save with `save()` | `SpectrumWidget::loadSettings`, per pan | sticks | sticks | n/a | n/a | sticks | n/a | sticks | SetupPage.cpp:781, :855, :890; SpectrumWidget.cpp:1372-1908, :1911-1957, :2464-2480; DisplaySettingsModel.cpp:286-290, :380-384. A Setup write clears pan overrides by rule: PanDisplayKeys.h. |
| Display > FFT & Engine | FFT size, sizing mode, window, Hz per bin, rate | `DisplayFftSize`, `DisplayFftSizingMode`, `DisplayFftWindow`, `DisplayHzPerBinTarget`, `DisplayHzPerBinLastTarget`, `DisplaySpectrumFps` | Core or window | change | MainWindow FFT pool; proxied | sticks | sticks | sticks | n/a | sticks | n/a | sticks | DisplaySetupPages.cpp:267, :463, :1194; scope SettingsScope.cpp:543-552; SD :1741-1744. Rate straddle: design call 2. |
| Display > Levels | Per-band grid dB max and min | `DisplayGridMax_<band>`, `DisplayGridMin_<band>` | Core or window, per band | change | pan on band crossing | sticks | sticks | sticks | sticks | sticks | n/a | sticks | SpectrumWidget.cpp:1931-1934 comment; SettingsScope.cpp:527-534; StationServer.cpp:9601-9603. |
| Display > Peaks & Markers | Active peak hold, peak blobs | `DisplayActivePeakHold*`, `DisplayPeakBlob*` (and pan keys) | window | change | SpectrumWidget | sticks | sticks | n/a | n/a | sticks | n/a | sticks | SpectrumPeaksPage.cpp:129-187, :197-219; SpectrumWidget.cpp:4475. |
| Display > Transmit Display | TX analyzer FFT, window, detectors, averaging | `DisplayTx*` nine keys | Core or window | change, background save | TxAnalyzer | sticks | sticks | sticks | n/a | n/a | n/a | sticks | DisplaySetupPages.cpp:3139-3150, :3249; TxAnalyzer.cpp:1016; StationServer.cpp:9593. |
| Display > Transmit Display | TX grid ref level, dynamic range | `DisplayTxGridRefLevel`, `DisplayTxGridDynamicRange` | window | no literal writer found | page, only when no pan exists | unknown | unknown | n/a | n/a | n/a | n/a | unknown | DisplaySetupPages.cpp:4714-4715. Pan path not traced. |
| Display > Transmit Display | TX waterfall colors, levels, gradient | `DisplayTxWf*` | window | change | SpectrumWidget | unknown | unknown | n/a | n/a | n/a | n/a | unknown | DisplaySetupPages.cpp:3644, :3732. Reader not read. |
| Display > Multimeter | Polling delay | `MultimeterDelayMs` | Core or window | change, memory then flush | never at local launch; on opening this page; remote on snapshot | reverts | reverts | sticks | n/a | n/a | n/a | reverts | Bug 1. MultimeterPage.cpp:382, :315, :345, :263-275; MainWindow.cpp:5064; MeterPoller.cpp:219. |
| Display > Multimeter | Units, decimal, history length | `MultimeterUnitMode`, `MultimeterShowDecimal`, `MultimeterSignalHistoryDurationMs` | window | change | each meter item at creation | sticks | sticks | n/a | n/a | sticks | n/a | sticks | MultimeterPage.cpp:415, :432, :458, :277-308; MainWindow.cpp:15826. A meter's own unit click is stored per item: MeterWidget.cpp:365-374. |
| Display > Multimeter | Peak hold, text hold, digital delay, averaging | `MultimeterPeakHoldMs`, `MultimeterTextHoldMs`, `MultimeterDigitalDelayMs`, `MultimeterAverageWindow` | none in effect | change | page | n/a | n/a | n/a | n/a | n/a | n/a | by design | Hidden until built: MultimeterPage.cpp:173, :228; UnbuiltFeatureList.h:93, :118. |
| Display > Multimeter | Signal history enable | `MultimeterSignalHistoryEnabled` | window | change | page | sticks | sticks | n/a | n/a | n/a | n/a | sticks | MultimeterPage.cpp:450, :337. No reader outside the page ("persists only"), so it changes nothing. |
| Transmit > Power | SWR protection, tune SWR, wind back, TX inhibit | `SwrProtectionEnabled`, `SwrProtectionLimit`, `SwrTuneProtectionEnabled`, `TunePowerSwrIgnore`, `WindBackPowerSwr`, `TxInhibitMonitor*` | Core or window | change | RadioModel build | sticks | sticks | sticks | n/a | n/a | n/a | sticks | TransmitSetupPages.cpp:891-1030, :800-806; RadioModel.cpp:2873-2892, :20318-20325; StationServer.cpp:9585. |
| Transmit > Power | Disable HF PA | `DisableHfPa` | Core or window | change | `applyDisableHfPaSetting` | unknown | unknown | unknown | n/a | n/a | n/a | unknown | Scope SettingsScope.cpp:659. Apply path not read. |
| Transmit > Power | Tune power per band, tune and two-tone power source | `hardware/<mac>/tunePowerByBand/<band>`, `hardware/<mac>/tx/TuneDrivePowerOrigin`, others | per radio MAC | per-band: in memory, written at slice flush; source: setter | connect | sticks | sticks | sticks | sticks | n/a | n/a | sticks | TransmitModel.cpp:2200-2266, :1866-1874; RadioModel.cpp:20992, :29643, :29920. Rule conflict: design call 1. |
| Transmit > DEXP/VOX | DEXP and VOX levels, timing, side-chain | `hardware/<mac>/tx/*` | per radio MAC | setter | connect | sticks | sticks | sticks | n/a | n/a | n/a | sticks | TransmitModel.cpp:2296-2849, :2851-3103; TransmitSetupPages DEXP rows. |
| Transmit and Audio | VOX enable, MON enable, mic mute, MOX, TUN, two-tone active | not loaded | none | n/a | start at safe defaults | n/a | n/a | n/a | n/a | n/a | n/a | by design | RadioModel.cpp:20995-20996 comment ("NOT loaded"); TransmitModel.cpp:2307; "connecting never keys". |
| Test > Two-Tone IMD | Frequencies, level, power, pulsed, invert, delay | `hardware/<mac>/tx/*` | per radio MAC | setter | connect | sticks | sticks | sticks | n/a | n/a | n/a | sticks | TestTwoTonePage.cpp setters; setter sweep over TransmitModel.cpp. |
| Appearance > Meter Styles | S-meter face, peak hold, decay | `SMeter_FaceStyle`, `PeakHoldEnabled`, `PeakDecayRate` | window | change | widget and page | sticks | n/a | n/a | n/a | n/a | n/a | sticks | AppearanceSetupPages.cpp:488-502, :412-425; SMeterWidget.cpp:251, :354-363. |
| Appearance > Colors & Theme | Colors | `Display*` color keys | window | Setup default write | SpectrumWidget | sticks | n/a | n/a | n/a | sticks | n/a | sticks | SpectrumPeaksPage.cpp:222-226 pattern; DisplaySetupPages.cpp:1688-1720; SpectrumWidget.cpp:1372-1908. |
| CAT & Network > Serial, TCP/IP, Options, PTT | Channels, ports, slice, PTT, globals | `Cat/Channels/<n>/*` and siblings | Core (remote uses `stationCat` commands) or window | `CatSettings::save` | service start | unknown | unknown | unknown | n/a | n/a | n/a | unknown | CatLiveSetupPages.cpp:177, :355; CatSettings.cpp:83, :97, :120-123. Reader wiring not traced. |
| CAT & Network > TCI Server | Enable, bind, port | `TciServerEnabled`, `TciServerBindAddress`, `TciServerPort` | window | change | MainWindow build | sticks | sticks | n/a | n/a | n/a | n/a | sticks | CatNetworkSetupPages.cpp:123, :165, :187, :1088; MainWindow.cpp:2283-2292. |
| CAT & Network > TCI Server | Emulation flags, CW rules, IQ, sensor intervals, RX2 VFO | `TciEmulate*`, `TciCw*`, `TciIqSwap`, `TciAlwaysStreamIq`, `Tci*SensorIntervalMs`, `TciForget*`, `TciUseRx1*`, `TciCopyRx2*` | window | change | read per session and per event | sticks | sticks | n/a | n/a | n/a | n/a | sticks | CatNetworkSetupPages.cpp:634-971; TciProtocol.cpp:693-780, :1553-1558; TciServer.cpp:2618-2619, :4090, :5793. |
| CAT & Network > TCI Server | The Core's station TCI line | `StationTci_*` | Core | Core, background save | Core | sticks | sticks | sticks | n/a | n/a | n/a | sticks | StationTciController.cpp:214, :243, :297; SettingsScope.cpp:444. |
| CAT & Network > rigctld | Enable, bind, port (remote window only) | `RemoteRigctld/Enabled`, `BindAddress`, `Port` | window | change | MainWindow | sticks | sticks | n/a | n/a | n/a | n/a | sticks | RemoteRigctldPage.cpp:109-112, :52-71; MainWindow.cpp:19157-19163. |
| CAT & Network > RF-Kit | Auto reconnect, poll, antenna labels | `RfKit_*` | Core or window | change | RadioModel, page | sticks | sticks | sticks | n/a | n/a | n/a | sticks | RfKitPage.cpp:676-682, :708-716, :268-276, :321; RadioModel.cpp:10006-10012; StationAccessoryData.cpp:264. |
| CAT & Network > 4O3A PGXL | Bias, fan, LED, power cap, pair, TX antenna, slice, nickname | `PGXL_*` | Core or window | change, some accessory save | page, RadioModel | sticks | sticks | sticks | n/a | n/a | n/a | sticks | PgxlAdvancedPage.cpp:1363-1548, :468, :513-725; StationDeviceSettings.cpp:283-326; StationAccessoryData.cpp:227-229, :380-382; RadioModel.cpp:37320-37324. |
| CAT & Network > 4O3A TGXL | Antenna labels, nickname, auto tune recall | `TGXL_*` | Core or window | change | page, RadioModel | sticks | sticks | sticks | n/a | n/a | n/a | sticks | TgxlAdvancedPage.cpp:1239-1257, :679, :522, :544-554, :646; RadioModel.cpp:38216; StationAccessoryData.cpp:244. |
| CAT & Network > Rotor | Port, presets, config | Rotor keys | Core | command, background save | `loadSettings` | sticks | sticks | sticks | n/a | n/a | n/a | sticks | StationRotorController.cpp:297, :333, :782, :806, :843-844. |
| CAT & Network > Remote Access | Station access | bound by MainWindow | window | not read | not read | unknown | unknown | unknown | n/a | n/a | n/a | unknown | RemoteStationPage; SetupDialog.cpp:2296-2302. No AppSettings use in the page itself. |
| Diagnostics > Export / Import | Settings file import | whole file | window or Core | at next launch | next launch | n/a | n/a | n/a | n/a | n/a | n/a | by design | AppSettings.cpp:927-934 (`m_savesHeldUntilRestart`), :1075. |
| Diagnostics > Radio Status, Connection Quality, Logs, Settings Validation | Readouts | none found | none | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | by design | Registered SetupDialog.cpp:2356-2367; pages not read control by control. |

#### Remote Core behavior by page

Registered scope, from `buildTree()`. A remote window is `model && !model->ownsLocalDsp()` (SetupDialog.cpp:1225-1228). A Core page in a remote window waits for the Core's settings (SetupDialog.cpp:1096, :1306-1323) and is rebuilt from the snapshot, except the seven Display assemblies that keep their owners (SetupDialog.cpp:2124-2131).

| Page | Scope | In a remote window |
| --- | --- | --- |
| General > Startup, Options | Mixed | Station keys (`User/`, `BandPlanRegion`, `RxOnly`, time-outs, watchdog) write to the Core. The window's own flags stay local. Auto-connect swaps store to `ConnectionTargets/` (GeneralSetupPages.cpp:33). |
| General > Filters | Core | Writes the Core's key. |
| General > This Core, CAT > rigctld | Mixed, this computer | Only registered in a remote window (SetupDialog.cpp:1808, :2286). |
| Hardware Config | Core | Tabs show the Core's radio and write through, then the Core reloads its controllers. Against a Core that does not offer that, tabs are disabled and edits dropped (HardwarePage.cpp:300-307). |
| PA, Transmit, Test, DSP > CFC | Core | Gated per page on `transmitSettingsVersion`; PA Gain's auto-calibrate needs the transmit permission. |
| Audio > Outputs, Microphone, Advanced | Mixed | Devices stay local. Radio speaker, mic source, mic gain and radio mic controls follow Core availability or transmit permission. |
| Audio > Digital modes, CAT > TCI Server | This computer | Local keys, including TCI. Values an older Core stored under `Tci` are ignored, not migrated (SettingsScope.cpp:363-376). |
| DSP > AGC, NR, NB, CW, AM/SAM, FM, TNF | Core | Writes the active receiver, mirrored to the Core. |
| DSP > Filter Presets, Options | Core | Presets are the Core's. Options take effect at once on the Core (StationServer.cpp:9570). |
| Display pages | Mixed | Colors, overlays and levels are this computer's, per pan. FFT quartet, sizing mode, TX analyzer nine, band grid and `MultimeterDelayMs` are the Core's. |
| Appearance | This computer | Local. |
| CAT Serial, TCP/IP, Options, PTT | Core | Read the Core's `stationCat` and send commands (SetupDialog.cpp:2224-2227). |
| RF-Kit, 4O3A, Rotor | Core | Core's keys or commands. |

An edit made while the Core link is down is kept in the proxy cache and shown as applied, not sent, then overwritten by the next snapshot with a warning and a toast (SettingsProxy.cpp:117-132, :180-253; MainWindow.cpp:5068). That one is reasoned and visible, so I count it as by design.


## Desktop: main window, display and layout

How to read this area:

- All of these values live in one local settings file for the window, unless the Store column says Station. There is one file per `--profile` (src/main.cpp reads the profile before anything else), not one per radio or Core. On a remote Core the window keeps its own copy of every OperatorLocal key: `AppSettings::setValue` sends a key to the Core only when `SettingsScope` calls it Station (src/core/AppSettings.cpp:1133-1148, src/core/settings/SettingsScope.h:68-88, src/gui/GuiSessionCoordinator.cpp:172-175). So the same look and layout apply to a local radio and to every Core.
- Core restart cannot touch an OperatorLocal key, so that column says sticks. For Station rows that column is about the Core's own file, which this sweep did not read, so it says unknown.
- Reconnect: the spectrum widgets, containers and applets survive a drop and rejoin. `SpectrumWidget::setConnectionState` only changes the fade (src/gui/SpectrumWidget.cpp:18073), and `applyLayout` has no caller on disconnect (src/gui/MainWindow.cpp:2727, 9229, 20221). A switch of Core or radio destroys the window but goes through the full close path first (src/gui/MainWindow.cpp:3523-3531).
- Saving: `AppSettings::setValue` writes memory only. Disk is written by `save()`, which any 500 ms spectrum save, slice save or container commit also does for the whole map, and by the close paths. "At close" below means the value is not even handed to AppSettings until `closeEvent` (src/gui/MainWindow.cpp:21381-21413), the session-end save (2355-2377) or, for some, `aboutToQuit` (2320-2347). "Memory, then next save" means `setValue` with no `save()` of its own, safe on a normal close and lost on a crash until some other save runs.
- Slice controls on the flag and in the RX and TX applets (AGC, filter, NR, mode, step, RIT) forward to SliceModel and belong to the slice sweep. Overlay ATT, preamp and antenna controls are radio values for the same sweep. They are not rows here.

| Surface | Setting | Store (exact key or file) | Owner | Saved when | Restored when | Reopen | Reconnect | Core restart | Band change | Add slice/pan | Phone background | Verdict | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Main window | Position, size, maximized, full screen | `MainWindowGeometry` | Window | At close, session end, aboutToQuit | End of constructor; off-screen falls back to 1280x800 | sticks | sticks | sticks | n/a | n/a | n/a | sticks | Save MainWindow.cpp:8883-8888, 21411, 2345, 2375. Restore 2399, 8890-8919 |
| Pan layout | Layout template (1, 2v, 2x2, ...) | `PanLayoutId` | Window | At close and session end only, not aboutToQuit | buildUI, before any radio; slices added after connect | sticks | sticks | sticks | n/a | sticks | n/a | sticks | Save PanadapterStack.cpp:768 via MainWindow.cpp:21399, 2372. Restore 9221-9230. Lost on crash, see bug 8. Note: sticks (growing the layout is saved at close) |
| Pan layout | Root divider between pans | `PanSplitter0Sizes` | Window | At close and session end only | buildUI | sticks | sticks | sticks | n/a | unknown | n/a | sticks | Save PanadapterStack.cpp:758-768. Restore 771-786, MainWindow.cpp:9230. Note: unknown (saved sizes are applied to the saved layout only) |
| Pan layout | Nested dividers (inside 12h, 2x2, 2h1, 3h2 rows) | None | Window | Never | Never | reverts | sticks | n/a | n/a | n/a | n/a | reverts | No save: PanadapterStack.cpp:333-410 builds them, 753-757 says only the root is saved |
| Pan layout | Pan popped out to its own window (the state) | None | Window | Never | Never | reverts | sticks | n/a | n/a | sticks | n/a | reverts | floatPanadapter callers only MainWindow.cpp:2731, 8435, 12893; no startup caller. Comment PanadapterStack.cpp:656. Note: sticks (a grown layout refloats, MainWindow.cpp:2724-2732) |
| Pan layout | Popped-out pan's place and size | `FloatingPan_<panId>_Geometry` | Per pan id | Dock, close, shutdown; memory, then next save | Each time it is floated | sticks | sticks | sticks | n/a | n/a | n/a | sticks | Save PanFloatingWindow.cpp:155-162, PanadapterStack.cpp:610-617. Restore PanFloatingWindow.cpp:165-175, PanadapterStack.cpp:515 |
| Pan | Extended view permission | `Pan_<panId>_ExtendedView` (key reads `Pan_pan-0_ExtendedView`) | Per pan id | On toggle; memory, then next save | Applet constructor | sticks | sticks | sticks | sticks | sticks | n/a | sticks | Save PanadapterApplet.cpp:532-541. Restore 138-141 |
| Container panel | Panel show or hide, the bottom bar's menu icon | None (`EdgeDockArea::m_collapsed`; legacy look hides the pane) | Window | Never | Never | reverts | sticks | n/a | n/a | n/a | n/a | reverts | MainWindow.cpp:17938-17963; EdgeDockStrip.cpp:679-682 stores nothing |
| Container panel | Panel width, strip thickness and extents | `ContainerWorkspacePreview` (default look) or `ContainerWorkspace`, extensions `splitterSizes` and `arrangement.strips` | Window | 200 ms commit timer, flushed at close and aboutToQuit | restoreState and reconcile | sticks | sticks | sticks | n/a | n/a | n/a | sticks | Save ContainerManager.cpp:3320-3362, 1579-1615. Restore 3137-3145, 1514-1537. Legacy `MainSplitterSizes` only without a store, 3124-3157 |
| Containers | Place, size, dock mode, name, source slice, enable, RX and TX show, border, pin, lock | Workspace document, keys as above | Window | Commit writes and saves at once; drags on the 200 ms timer | MainWindow.cpp:9395, 9964, then reconcile | sticks | sticks | sticks | n/a | sticks | n/a | sticks | ContainerWorkspaceStore.cpp commit (`m_settings.save`), ContainerManager.cpp:3320-3362, 933-1045. Flush MainWindow.cpp:21391, 2340, 2369. Note: sticks (Hide when slice not open follows the slice) |
| Containers menu | Applet shown in panel, Hidden submenu | Workspace document entries (`Applet<id>Visible` only without a store) | Window | At once, via commit | Controller syncs from document | sticks | sticks | sticks | n/a | n/a | n/a | sticks | MainWindow.cpp:13652-13664, AppletVisibilityController.cpp syncWorkspace |
| Containers menu | Align to 8 px grid | Document `arrangement.gridAlignment` | Window | At once | Reconcile | sticks | sticks | sticks | n/a | n/a | n/a | sticks | ContainerManager.cpp:1734, 1191 |
| Containers menu | Arrange Workspace mode | None | Window | Never | Never | n/a | n/a | n/a | n/a | n/a | n/a | by design | Comment MainWindow.cpp:13590 "Runtime only, not saved" |
| Applets | An applet's own floating window: open state and place | `Applet<id>Floating`, `Applet<id>FloatGeometry` | Window | On open, move, close; memory, then next save | Applet install, deferred one tick | sticks | sticks | sticks | n/a | n/a | n/a | sticks | Save AppletPanelWidget.cpp:347, 408, 415, 424, 464. Restore 300-328, 411 |
| Meters | Meter items in a container, their properties | Workspace document | Window | Edit session commit; widget edits queued to `setEntryConfig` | Content factory at reconcile; Setup Multimeter values re-applied at MainWindow.cpp:9995 | sticks | sticks | sticks | n/a | n/a | n/a | sticks | ContainerContentHost.cpp:731-741. Restore ContainerManager.cpp:3409 path, MainWindow.cpp:9964-9974 |
| Meters | S meter dial right-click: RX mode, TX mode, face, peak hold, decay | Entry config in the document; `SMeter_*`, `PeakHoldEnabled`, `PeakDecayRate` only for a standalone widget | Per container entry | Each menu change | Dial built from entry config | sticks | sticks | sticks | n/a | n/a | n/a | sticks | SMeterDialContent.cpp:676-694; SMeterWidget.cpp:583, 624, 1644, 1764, 1801; load 340-364 |
| Workspace profiles | Profile list, active profile, each snapshot | `WorkspaceProfiles` | Window | Switch, Save, create, rename, remove, import; saved at once | MainWindow.cpp:12506-12508 | sticks | sticks | sticks | n/a | n/a | n/a | sticks | WorkspaceProfiles.cpp:431-477, 481-535. Load 202-245. Holds containers and screens only |
| View menu | Theme, dark mode, font size | None; Setup toggle disabled | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | by design | Not built: GeneralSetupPages.cpp:153-164. The look is the env var NEREUS_WORKSPACE_PREVIEW, not a setting |
| View menu | UI scale, Minimal Mode, Keyboard Shortcuts | `UiScalePercent` is read only, no writer in src | n/a | n/a | main.cpp:96-113 | n/a | n/a | n/a | n/a | n/a | n/a | by design | Not built, hidden: MainWindow.cpp:12985-13004, 13022, 13052 |
| View menu | Band plan label size | `BandPlanFontSize` | Window | Menu writes memory; every pan's save rewrites it | loadSettings; menu check from key | sticks | sticks | sticks | sticks | sticks | n/a | sticks | Save MainWindow.cpp:12929, SpectrumWidget.cpp:1946, 6863. Restore 1726, MainWindow.cpp:12904. Note: sticks (shared by all pans) |
| View menu | Display duplex (DUP) | `DisplayDuplex` | Window | On toggle; memory, then next save | Menu build, then applyDisplayDuplex | sticks | sticks | sticks | sticks | sticks | n/a | sticks | MoxDisplayController.cpp:232-240, MainWindow.cpp:12974-12981 |
| View menu | Performance overlay | `ShowPerfOverlay` | Window | On toggle; memory, then next save | Widget constructor | sticks | sticks | sticks | n/a | unknown | n/a | sticks | Save SpectrumWidget.cpp:6813-6815. Restore 1036-1045. Note: unknown (menu acts on active pan only) |
| Spectrum | Zoom, span | `DisplayBandwidth`, `DisplayBandwidth_<pan>` | Per pan, window | 500 ms after a change; flushed at close. Remote pans save at close | loadSettings, then first wiring accepts it inside the ceiling | sticks | sticks | sticks | sticks | sticks | n/a | sticks | Save SpectrumWidget.cpp:3801, 2150-2153, 2464-2480, 21381. Restore 1496, MainWindow.cpp:17077-17113. Clamp saved: see Design calls |
| Spectrum | Zoom slider under the pans | None; starts at 768 | Window | Never | Never | reverts | sticks | n/a | sticks | n/a | n/a | reverts | MainWindow.cpp:9359-9376, nothing else touches `zoomBar`. The span itself is the row above |
| Spectrum | Transmit view span | `DisplayTxViewBandwidth[_<pan>]` | Per pan | 500 ms; at close | loadSettings | sticks | sticks | sticks | sticks | sticks | n/a | sticks | SpectrumWidget.cpp:2293, 1713 |
| Spectrum | CTUN preference | `DisplayCtunEnabled[_<pan>]` | Per pan | 500 ms | loadSettings | sticks | sticks | sticks | sticks | sticks | n/a | sticks | SpectrumWidget.cpp:2155, 1512 |
| Waterfall | Palette, color gain, black level, fill alpha, fill, split, custom stops | `DisplayWfColorScheme`, `DisplayWfColorGain`, `DisplayWfBlackLevel`, `DisplayFftFillAlpha`, `DisplayPanFill`, `DisplaySpectrumFrac` (each `[_<pan>]`), `DisplayWfCustomStops` (shared) | Per pan except stops | 500 ms through DisplaySettingsModel | DisplaySettingsModel::load from loadSettings | sticks | sticks | sticks | sticks | sticks | n/a | sticks | DisplaySettingsModel.cpp:275-288, 373-382. SpectrumWidget.cpp:1911-1935, 6039, 6085. Note: sticks; a new pan inherits the bare key |
| Waterfall | Levels, AGC, NF AGC and offset, stop on TX, blanking, opacity, rate, history, averaging, timestamps, filter overlays | `DisplayWfHighLevel`, `DisplayWfLowLevel`, `DisplayWfAgc`, `WaterfallNFAGCEnabled`, `WaterfallAGCOffsetDb`, `WaterfallStopOnTx`, `WaterfallBlanking*`, `DisplayWf*`, `DisplayWaterfallHistoryMs`, `DisplayShow*Waterfall` (each `[_<pan>]`) | Per pan | 500 ms | loadSettings | sticks | sticks | sticks | sticks | sticks | n/a | sticks | SpectrumWidget.cpp:2117-2366 visitor; load 1500, 1593-1597. Setter scan: every persistent setter schedules a save. Note: sticks (not per band) |
| Waterfall | Transmit palette, levels, low color | `DisplayTxWfPalette`, `DisplayTxWfLowLevel`, `DisplayTxWfHighLevel`, `DisplayTxWfLowColor` | Shared by all pans, window | 500 ms | Shared loader | sticks | sticks | sticks | sticks | sticks | n/a | sticks | SpectrumWidget.cpp:1966-2003 |
| Spectrum | Detector, averaging, time, peak hold, line width, gradient, normalize, active peak hold, peak blobs, noise floor overlay, peak value overlay | `DisplaySpectrum*`, `DisplayPeakHold*`, `DisplayLineWidth`, `DisplayGradientEnabled`, `DisplayDispNormalize`, `DisplayActivePeakHold*`, `DisplayPeakBlobs*`, `DisplayShowNoiseFloor*`, `DisplayNoiseFloor*`, `DisplayPeakValue*`, `DisplayShowPeakValueOverlay` | Per pan | 500 ms | loadSettings | sticks | sticks | sticks | sticks | sticks | n/a | sticks | SpectrumWidget.cpp:2117-2366 visitor; load 1543, 1572-1574, 1792 |
| Spectrum | Grid and scale look: grid on, zero lines, FPS, cursor readout, dBm strip, label align, spacing, font, colors, background picture | `DisplayGridEnabled`, `DisplayShow*`, `DisplayDbmScaleVisible`, `DisplayFreq*`, `DisplayFillColor`, `DisplayTraceColor`, `DisplayBackground*`, filter and grid colors | Per pan, except the shared appearance colors | 500 ms; shared colors through writeDefault | loadSettings, shared loader | sticks | sticks | sticks | sticks | sticks | n/a | sticks | SpectrumWidget.cpp:2117-2366, 2065-2094; load 1679-1681, 1752, 1763 |
| Spectrum | dB grid step | `DisplayGridStep` | Window, shared | Memory, then next save | PanadapterModel load and loadSettings | sticks | sticks | sticks | sticks | sticks | n/a | sticks | PanadapterModel.cpp:400, 440; SpectrumWidget.cpp:1590 |
| Spectrum | Receive dBm range (strip drags, wheel, Setup) | `DisplayGridMax_<band>`, `DisplayGridMin_<band>` | Station, per band. On a remote Core these are the Core's | End of the drag; memory locally, the proxy remotely | PanadapterModel load; a Core change reloads | sticks | sticks | unknown | sticks | sticks | n/a | sticks | Save PanadapterModel.cpp:281-297, MainWindow.cpp:8234-8261, SpectrumWidget.cpp:14676-14706. Restore PanadapterModel.cpp:415-440, 453-470. Note: sticks (each band keeps its own); sticks (pans share a band's values) |
| Spectrum | Range while grid follows the noise floor | None, on purpose | Per pan | Never | Stored band range, then tracking resumes | n/a | n/a | n/a | n/a | n/a | n/a | by design | Comment SpectrumWidget.cpp:5594-5597 |
| Spectrum | Transmit dBm range | `DisplayTxGridRefLevel`, `DisplayTxGridDynamicRange` | Shared, window | On release | loadTransmitDisplayScale | sticks | sticks | sticks | sticks | sticks | n/a | sticks | SpectrumWidget.cpp:3926-3945, 3960-3970 |
| Spectrum | 3D view: mode, gain, span, angle, speed, slice shadow | `DisplaySpectrumRenderMode`, `Display3DGain`, `Display3DSpan`, `Display3DAngle`, `Display3DSpeed`, `Display3DSliceShadow` (each `[_<pan>]`) | Per pan | 500 ms | DisplaySettingsModel::load, then copied | sticks | sticks | sticks | sticks | sticks | n/a | sticks | DisplaySettingsModel.cpp:290-296, 384-390; SpectrumWidget.cpp:1885-1902 |
| Spectrum | 3D floor | `Display3DFloorDepth_<band>` | Per band | On change; memory, then next save | Model load, band push | sticks | sticks | sticks | sticks | sticks | n/a | sticks | PanadapterModel.cpp:387, 425. Note: sticks (per band) |
| Spectrum | Clarity on or off | `ClarityEnabled[_<pan>]`, `ClarityFloor_<band>` | Per pan; floor per band | 500 ms | loadSettings, registry | sticks | sticks | sticks | sticks | sticks | n/a | sticks | SpectrumWidget.cpp:1594, 2117-2366; PanadapterModel.cpp:118, 423. Live output is runtime by comment at 6508 |
| Display pill | FFT size, window, Hz per bin, sizing mode, FPS | `DisplayFftSize`, `DisplayFftWindow`, `DisplayHzPerBinTarget`, `DisplayFftSizingMode`, `DisplayHzPerBinLastTarget`, `DisplaySpectrumFps` | Station. On a remote Core these are the Core's | On change; memory locally, proxy remotely | Pool config at startup; pill reads keys | sticks | sticks | unknown | sticks | sticks | n/a | sticks | Save MainWindow.cpp:7381, 7465, 7477. Restore 6310-6355, 7532-7545. Remote paint rate: see Design calls |
| Display pill | Decimation (also Setup, Rendering) | None | Pool, memory only | Never | Starts at 1 | reverts | sticks | n/a | sticks | sticks | n/a | reverts | MainWindow.cpp:7507-7526; 7550 "Decimation has no key"; ControlRanges.h:655 |
| Display pill | Last tab open | `DisplayPillLastTab` | Window, one for all pans | On tab change; memory, then next save | Panel constructor | sticks | sticks | sticks | n/a | sticks | n/a | sticks | SpectrumOverlayPanel.cpp:2964, 1706 |
| Overlay strip | Strip collapsed with the arrow | None (`m_expanded`, starts true) | Per pan, memory | Never | Never | reverts | sticks | n/a | sticks | n/a | n/a | reverts | SpectrumOverlayPanel.cpp:845-853, .h:393. The `collapsed` signal has no listener in src/gui/*.cpp |
| Spot overlay | Spots on, font, levels, height, colors, source mask | `IsSpotsEnabled`, `SpotFontSize`, `SpotsMaxLevel`, `SpotsStartingHeightPercentage`, `IsSpotsOverride*`, `SpotsOverride*`, `SpotsBackgroundOpacity` | Window | Saved at the switch | First pan at startup only | reverts | sticks | sticks | sticks | reverts | n/a | reverts | Read SpectrumWidget.cpp:12356-12400. Callers MainWindow.cpp:2235, 19939, 19953, ContainerButtonDispatcher.cpp:595-601. Note: sticks on pan 0, reverts on others; reverts (a new pan does not load them) |
| Status bar | CPU row: system or app, row source | `CpuShowSystem`, `CpuRowSource` | Window | Menu; memory, then next save | Status bar build | sticks | sticks | sticks | n/a | n/a | n/a | sticks | Save MainWindow.cpp:17592, 17602. Restore 14889-14896 |
| Title bar | Master volume and mute | `audio/Master/Volume`, `audio/Master/Muted` | Window | Saved at once | Constructor, seeded to AudioEngine | sticks | sticks | sticks | n/a | n/a | n/a | sticks | MasterOutputWidget.cpp:441-444, 457-460. Restore 349-354, 414-415. Audio sweep covers devices |
| PureSignal view | IMD inspector and guides, two-tone measurements | `PureSignalImdInspectorVisible`, `PureSignalImdGuidesVisible`, `puresignal/showTwoToneMeasurements` | Per pan; window | Memory, then next save | loadSettings; controller | sticks | sticks | sticks | n/a | sticks | n/a | sticks | SpectrumWidget.cpp:13209, 13218, 1404-1405; MainWindow.cpp:19781-19785; MoxDisplayController.cpp:139-141 |
| VFO flag | Fold | `ReceiverFlag_<n>_Collapsed` | Per receiver letter, window, not sent to the Core | On toggle; memory, then next save | When the flag binds its slice | sticks | sticks | sticks | sticks | sticks | n/a | sticks | VfoWidget.cpp:4021-4025, 3626-3633, 4108-4112. Note: sticks (each letter its own) |
| VFO flag | Which tab is open | None (`m_activeTab`, starts -1) | Per flag, memory | Never | Never | reverts | unknown | n/a | sticks | n/a | n/a | reverts | VfoWidget.cpp:1020, 1679-1693. See Design calls |
| Applet | AM Mod Monitor source, flash limits, vintage meters | `ModMon/Source`, `ModMon/PosFlashPct`, `ModMon/NegFlashPct`, `ModMon/VintageMeters` | Window | Memory, then next save | Applet loadSettings | sticks | sticks | sticks | n/a | n/a | n/a | sticks | ModMonitorApplet.cpp:415-417, 475, 487, 439-464 |
| Applet | AM Mod Monitor feedback receiver | `ModMon/FbStream` | Station | Memory locally, proxy remotely | Applet loadSettings, pushed to model | sticks | sticks | unknown | n/a | n/a | n/a | sticks | ModMonitorApplet.cpp:313, 443, 456-458 |
| Applet | TCI gains | `TciSliceAGain`, `TciTxGain` | Window | Saved at once | Seeded; applied when server arrives | sticks | sticks | sticks | n/a | n/a | n/a | sticks | TciApplet.cpp:690-694, 713-716, 342-345, 162-168 |
| Applet | DEXP threshold marker | `PhoneCw/DexpThresholdMarkerDb` | Window | Memory, then next save | Constructor | sticks | sticks | sticks | n/a | n/a | n/a | sticks | PhoneCwApplet.cpp:1633, 584 |
| Applet | NNR advanced section open | `NnrControls/AdvancedExpanded` | Window | Memory, then next save | Constructor | sticks | sticks | sticks | n/a | n/a | n/a | sticks | NnrControls.cpp:265, 255 |
| Applet | Rotor dial shape | `Rotor/DialShape` | Window | Memory, then next save | Constructor | sticks | sticks | sticks | n/a | n/a | n/a | sticks | RotorDialWidget.cpp:581, 172 |
| Window | AmpView place | `ampview/geometry` | Window | Close; closeEvent closes it first | On open | sticks | sticks | sticks | n/a | n/a | n/a | sticks | AmpViewWindow.cpp:346-350, 311-320; MainWindow.cpp:21344 |

Non-standard keys, all still read back correctly: `Pan_<panId>_ExtendedView` and `FloatingPan_<panId>_Geometry` embed the pan id with a hyphen, and `ampview/geometry` and `puresignal/showTwoToneMeasurements` are not PascalCase. No QSettings use in this area. No lowercase boolean writes turned up in a pattern search of this area.


## Receiver and transmit state (desktop and Core)

Three things apply to many rows, so they are stated once here:

- **Disk timing.** `AppSettings::setValue` writes only an in-memory map (`src/core/AppSettings.cpp:1133-1149`). Disk writes happen on the 500 ms slice save (`src/models/RadioModel.cpp:29381`, `29459`), at window close (`src/gui/MainWindow.cpp:21413`) and at Core stop (`src/core/daemon/DaemonApp.cpp:655`, `663`). The slice save is flushed at disconnect and quit (`RadioModel.cpp:29890`). "Reopen: sticks" below means a clean quit. A crash or power cut loses anything still in memory.
- **Protected layout.** On a Core, every Slice row below reverts on Core restart once `m_receiveLayoutProtected` is set. See Clear bug 2. The rows show the behavior when it is not set.
- **Phone background.** This area holds the Core side only, and the model code read here does nothing when a phone backgrounds. Whether the phone's own reattach changes anything belongs to the phone sweep, so that column says unknown unless the row cannot apply.

Slice keys use `Slice<N>/` for per-receiver values and `Slice<N>/Band<B>/` for per-band values. N is the slice letter's number. A remote window never saves these: `scheduleSettingsSave` returns at once on a remote role (`RadioModel.cpp:29347`), and the Core saves them when its own slice changes.

| Surface | Setting | Store (exact key or file) | Owner | Saved when | Restored when | Reopen | Reconnect | Core restart | Band change | Add slice/pan | Phone background | Verdict | Evidence |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| VFO | Frequency | `Slice<N>/Band<B>/Frequency`, `Slice<N>/LastBand`; on a Core also the ReceiveLayoutStore manifest | per slice-band; Core manifest per radio MAC | frequencyChanged, 500 ms | Desktop connect via loadSliceState; Core via manifest | reverts | sticks | sticks | sticks | sticks | unknown | reverts | Save `SliceModel.cpp:3521`, `3712`, `RadioModel.cpp:25922`, `29119`. Restore `SliceModel.cpp:3727`, `3450`, `RadioModel.cpp:26830`. Reverts only with Lock on at quit on a desktop: Clear bug 1 |
| VFO | Mode | `Slice<N>/Band<B>/DspMode`; Core manifest | per slice-band | dspModeChanged, 500 ms | As frequency, assigned directly so Lock cannot stop it | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `SliceModel.cpp:3541`, `RadioModel.cpp:25980`. Restore `SliceModel.cpp:3784-3817`, `3452` |
| VFO | Filter cutoffs | `Slice<N>/Band<B>/Mode<M>/FilterLow,FilterHigh`, legacy `Slice<N>/Band<B>/FilterLow,FilterHigh` | per slice-band-mode | filterChanged, and on a mode change for the old mode | With band restore; mode change reads the new mode's pair | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `SliceModel.cpp:3530-3540`, `746-765`. Restore `SliceModel.cpp:3823-3834`, `3463-3486`, `772-788`. Push `RadioModel.cpp:12652` |
| AGC | Mode, threshold, hang, slope, attack, decay, fixed gain, hang threshold, max gain | `Slice<N>/Band<B>/AgcMode`, `AgcThreshold`, `AgcHang`, `AgcSlope`, `AgcAttack`, `AgcDecay`, `AgcFixedGain`, `AgcHangThreshold`, `AgcMaxGain` | per slice-band | each changed signal, 500 ms | With band restore | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `SliceModel.cpp:3522-3529`, `3542`, `RadioModel.cpp:26003-26094`. Restore `SliceModel.cpp:3730-3782`, `3835`. Push `RadioModel.cpp:12652` |
| AGC | AGC-T AUTO and offset | `Slice<N>/AgcAutoEnabled`, `Slice<N>/AgcAutoOffset` | per slice | autoAgc signals | With band restore | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `SliceModel.cpp:3704-3705`, `RadioModel.cpp:26637-26638`. Restore `SliceModel.cpp:3751-3773` |
| AGC | RF gain | `Slice<N>/RfGain` | per slice | rfGainChanged; readback schedules again | With band restore | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `SliceModel.cpp:3666`, `RadioModel.cpp:26672-26703`, `18494-18516`. Restore `SliceModel.cpp:4114` |
| NB | NB mode Off/NB/NB2 | `Slice<N>/Band<B>/NbMode` | per slice-band | nbModeChanged | With band restore | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `SliceModel.cpp:3549`, `RadioModel.cpp:26348`. Restore `SliceModel.cpp:3847` |
| NB | NB1, NB2, SNB tuning | `Slice<N>/Nb1Threshold`, `Nb1TransitionMs`, `Nb1LeadMs`, `Nb1LagMs`, `Nb2Mode`, `SnbK1`, `SnbK2`, `SnbOutputBandwidthHz` | per slice | each changed signal | With band restore; missing key falls to the old global then default | sticks | sticks | sticks | sticks | unknown | unknown | sticks | Save `SliceModel.cpp:3651-3658`, `RadioModel.cpp:26368-26414`. Restore `SliceModel.cpp:4073-4092`. Add slice unknown: co-hosted slices share NB through a mirror I did not trace |
| NR | Selected reducer and NNR knobs | `hardware/<mac>/slices/<N>/nnr/NrActive`, `NnrModelSlot`, `NnrMaskFloorDb`, `NnrPosition`, `NnrAlpha`, `NnrAlphaKneeDb`, `NnrTauSeconds`, `NnrMaxGainDb`, `NnrAttackMs`, `NnrReleaseMs`; legacy `Slice<N>/NrActive` | per radio MAC and slice | activeNrChanged, nnrConfigurationChanged | Slice creation, connect, band restore | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `SliceModel.cpp:2216-2231`, `3581`, `RadioModel.cpp:13073`, `13081`. Restore `SliceModel.cpp:2233-2323`, `RadioModel.cpp:21096-21099`. NR3 and DFNR turn off when no model can run, by design `RadioModel.cpp:13087-13090` |
| NR | NR1, NR2, NR3, NR4, DFNR, MNR knobs | `Slice<N>/Nr1*`, `Nr2*`, `Nr3*`, `Nr4*`, `Dfnr*`, `Mnr*` | per slice | each changed signal | With band restore | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `SliceModel.cpp:3583-3620`, `RadioModel.cpp:26121-26294`. Restore `SliceModel.cpp:3881-3984` |
| NR | BNR strength | `Slice<N>/BnrStrength` | per slice | No save trigger; only with another slice save | With band restore | unknown | sticks | unknown | sticks | sticks | unknown | unknown | Save `SliceModel.cpp:3614`; no bnrStrengthChanged connect in `RadioModel.cpp`. Restore `SliceModel.cpp:3964`. BNR is "in no build" per `RadioModel.cpp:13006-13007`, so I could not tell whether a control reaches it |
| NR | SNB and ANF on/off | `Slice<N>/SnbEnabled`, `Slice<N>/AnfEnabled` | per slice | changed signals | With band restore | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `SliceModel.cpp:3622-3623`, `RadioModel.cpp:26317`, `26329`. Restore `SliceModel.cpp:3986-3991` |
| Squelch | SSQL, AMSQ, FMSQ enable and threshold | `Slice<N>/SsqlEnabled`, `SsqlThresh`, `AmsqEnabled`, `AmsqThresh`, `FmsqEnabled`, `FmsqThresh` | per slice | changed signals | With band restore; pushed again at channel open | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `SliceModel.cpp:3640-3645`, `RadioModel.cpp:26451-26499`. Restore `SliceModel.cpp:4023-4040`. Push `RadioModel.cpp:12937-12947` |
| APF | Enable and tune | `Slice<N>/ApfEnabled`, `Slice<N>/ApfTuneHz` | per slice | changed signals | With band restore | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `SliceModel.cpp:3684-3685`, `RadioModel.cpp:26425`, `26439`. Restore `SliceModel.cpp:4195-4196` |
| RIT/XIT | RIT and XIT on/off and offset | `Slice<N>/RitEnabled`, `RitHz`, `XitEnabled`, `XitHz` | per slice | changed signals | With band restore | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `SliceModel.cpp:3661-3664`, `RadioModel.cpp:26615-26618`. Restore `SliceModel.cpp:4099-4110`. New-slice behavior belongs to the other session, recorded in the next table |
| VFO | Tune step | `Slice<N>/Band<B>/StepHz` | per slice-band | stepHzChanged | With band restore | sticks | sticks | sticks | reverts | sticks | unknown | reverts | Save `SliceModel.cpp:3543`, `RadioModel.cpp:26613`. Restore `SliceModel.cpp:3839`. A step set on 20 m does not follow to 40 m: Design call 3 |
| Audio | AF gain, mute, audio pan, binaural | `Slice<N>/AfGain`, `Muted`, `AudioPan`, `BinauralEnabled` | per slice | changed signals | With band restore | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `SliceModel.cpp:3660`, `3665`, `3676-3677`, `RadioModel.cpp:26511-26525`, `26658`. Restore `SliceModel.cpp:4096`, `4111`, `4184-4185` |
| Audio | VAX channel, output route | `Slice<N>/VaxChannel`, `Slice<N>/OutputRoute` | per slice; a remote window keeps VAX on its own computer | Setter writes at once; disk on next flush | Slice creation, loadFromSettings | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `SliceModel.cpp:4341`, `4375`. Restore `SliceModel.cpp:4460-4470`, `RadioModel.cpp:16978` |
| VFO | Lock | `Slice<N>/Locked` | per slice | lockedChanged | With band restore | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `SliceModel.cpp:3659`, `RadioModel.cpp:26614`. Restore `SliceModel.cpp:4093`. Its restore order causes Clear bug 1 |
| Mode panels | FM CTCSS, offset, TX mode, reverse; DIGL, DIGU offset; RTTY mark, shift | `Slice<N>/FmCtcssMode`, `FmCtcssValueHz`, `FmOffsetHz`, `FmTxMode`, `FmReverse`, `DiglOffsetHz`, `DiguOffsetHz`, `RttyMarkHz`, `RttyShiftHz` | per slice | changed signals | With band restore | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `SliceModel.cpp:3686-3694`, `RadioModel.cpp:26623-26631`. Restore `SliceModel.cpp:4197-4222` |
| Diversity | Phase, gain, fine null | `Slice<N>/Band<B>/DiversityPhaseDeg`, `DiversityGainDb`, `DiversityFineNullEnabled` | per slice-band | No save trigger of their own | With band restore | reverts | sticks | reverts | sticks | sticks | unknown | reverts | Save `SliceModel.cpp:3561-3564`; `RadioModel.cpp:38451-38459` only publishes. Restore `SliceModel.cpp:3861-3873`: Clear bug 5 |
| Diversity | Which slice holds diversity | Core: ReceiveLayoutStore diversity owner. Desktop: none found | Core per radio MAC | With layout capture | Hydrate on Core | unknown | unknown | sticks | n/a | unknown | unknown | unknown | Save `RadioModel.cpp:29137`. Restore `RadioModel.cpp:29328-29336`. Desktop path not found in this sweep |
| DDC | Sample rate per slice-band | `Slice<N>/Band<B>/SampleRate` | per slice-band | With slice save | With band restore | unknown | unknown | unknown | unknown | unknown | unknown | unknown | The controller owns this one. Save `SliceModel.cpp:3554`. Restore `SliceModel.cpp:3854`, `RadioModel.cpp:26840` |
| Pan | Pan center | None; derived from the slice frequency | derived | n/a | Connect | reverts | sticks | reverts | n/a | sticks | unknown | by design | `RadioModel.cpp:26848`; comment `src/gui/SpectrumWidget.cpp:1491-1493` says center is persisted through the slice. Off-center CTUN position is lost: Design call 6 |
| Pan | Zoom | `DisplayBandwidth` per pan | window | SpectrumWidget save timer | SpectrumWidget load | unknown | unknown | n/a | unknown | unknown | n/a | unknown | Save `src/gui/SpectrumWidget.cpp:2150`. Restore `SpectrumWidget.cpp:1497`. Clamp and rate interplay not traced; the display sweep should confirm |
| Pan | CTUN preference | `DisplayCtunEnabled` per pan | window | setCtunEnabled, save timer | SpectrumWidget load | sticks | sticks | n/a | sticks | sticks | n/a | sticks | Save `SpectrumWidget.cpp:2156`, `17809-17826`. Restore `SpectrumWidget.cpp:1512-1513` |
| Pan | Per-band dB range, dB step, Clarity floor, NF estimate, 3D depth | `DisplayGridMax_<B>`, `DisplayGridMin_<B>`, `DisplayGridStep`, `ClarityFloor_<B>`, `DisplayBandNFEstimate_<B>`, `Display3DFloorDepth_<B>` | Core or window; one store for all pans | Setter writes at once | PanadapterModel constructor; band follows on change | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `src/models/PanadapterModel.cpp:281-303`, `330-339`, `360`, `387`, `400`. Restore `PanadapterModel.cpp:413-447`, `405-411` |
| Pan | Model center, bandwidth, FFT size, average count | None | runtime | Never | Never | n/a | n/a | n/a | n/a | n/a | n/a | by design | `PanadapterModel.cpp:176-226`. Runtime values; no caller of the FFT setter in `src/models` or MainWindow, the zoom and rate are held elsewhere |
| Layout | Slices and pans on a Core | ReceiveLayoutStore per radio MAC | Core per radio MAC | Every slice flush | prepareReceiveLayout, then admission at connect | sticks | sticks | sticks | n/a | sticks | unknown | sticks | Save `RadioModel.cpp:29095-29143`, `29455`. Restore `RadioModel.cpp:28920-28935`, `29155-29339`, `13932-14043`, `29024-29092` |
| Layout | Extra slices on a desktop with no Core | Nothing records them; only `PanLayoutId` and each letter's settings | window | n/a | One starting slice plus one per empty pan | reverts | sticks | n/a | n/a | n/a | n/a | reverts | `RadioModel.cpp:19795`, `29097-29100`, `src/gui/MainWindow.cpp:20382-20391`, commit 3813bc07f. Design call 2 |
| Layout | All slice settings on a Core while the layout is protected | Slice keys and manifest are both held back | Core | Never, for the rest of the process | n/a | n/a | sticks | reverts | sticks | sticks | unknown | reverts | Set `RadioModel.cpp:13866`, `14019`, `22562`, `28937`, `29032`, `29060`; never cleared. Gates `RadioModel.cpp:29350`, `29440`, `29622`: Clear bug 2 |
| Antennas | Per-band RX, TX, RX-only antennas | `hardware/<mac>/alex/antenna/<B>/tx`, `rx`, `rxonly` | per radio MAC, per band | Alex dirty flag, slice flush, teardown | connect, `m_alexController.load()` | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `src/core/accessories/AlexController.cpp:613-615`, `RadioModel.cpp:2683-2684`, `29634`, `29913`. Restore `AlexController.cpp:576-578`, `RadioModel.cpp:20939-20940`, `26972-26986`. Rule conflict: Design call 4 |
| Antennas | Block TX, out-on-TX flags, BPF modes | `hardware/<mac>/alex/antenna/blockTxAnt2`, `blockTxAnt3`, `rxOutOnTx`, `ext1OutOnTx`, `ext2OutOnTx`, `rxOutOverride`, `useTxAntForRx`, `Alex0_BpfMode`, `Alex1_BpfMode` | per radio MAC | Dirty flag, as above | connect | sticks | sticks | sticks | n/a | sticks | unknown | sticks | Save `AlexController.cpp:617-626`, `RadioModel.cpp:2731`, `2806`, `2824`. Restore `AlexController.cpp:583-594` |
| TX | TX power per band | `hardware/<mac>/powerByBand/<B>` | per radio MAC, per band | powerChanged via drivePowerScroll, then setter | connect, applied through applyTransmitBand | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `TransmitModel.cpp:1501-1505`, `RadioModel.cpp:30882`, `8529`. Restore `TransmitModel.cpp:2700-2715`, `RadioModel.cpp:21000`, `8537-8546`. Rule conflict: Design call 4 |
| TX | Tune power per band, fixed tune power, drive source, limits, FM TX offset | `hardware/<mac>/tunePowerByBand/<band number>`, `hardware/<mac>/tx/FixedTunePower`, `TuneDrivePowerOrigin`, `hardware/<mac>/limitPowerByBand/<B>`, `limitTunePowerByBand/<B>`, `fmTxOffsetByBandMhz/<B>` | per radio MAC | Tune array on slice save and teardown; rest at setter | connect | sticks | sticks | sticks | sticks | sticks | unknown | sticks | Save `TransmitModel.cpp:2259-2265`, `1874`, `1895`, `1537`, `1562`, `1616`, `RadioModel.cpp:29643`, `29920`. Restore `TransmitModel.cpp:2231-2237`, `2732-2742`, `2777-2792` |
| TX | Mic gain, boost, XLR, line in, tip/ring, bias, PTT disable, user pins | `hardware/<mac>/tx/MicGain`, `Mic_Input_Boost`, `Mic_XLR`, `Line_Input_On`, `Line_Input_Level`, `Mic_TipRing`, `Mic_Bias`, `Mic_PTT_Disabled`, `UserDigOut` | per radio MAC | Setter writes at once; backstop at teardown | connect, loadFromSettings | sticks | sticks | sticks | n/a | sticks | unknown | sticks | Save `TransmitModel.cpp:647`, `697-821`, `2857-2870`. Restore `TransmitModel.cpp:2303-2348`. `LineInGain` is derived from the boost on load, by design at `2342-2345` |
| TX | Mic source and pre-VAX source | `hardware/<mac>/tx/Mic_Source`, `Mic_Source_PreVax`, fallback `tx/preconnect/Mic_Source` | per radio MAC, with a pre-connect fallback | Setter | connect | sticks | sticks | sticks | n/a | sticks | unknown | sticks | Save `TransmitModel.cpp:3663`, `3687`. Restore `TransmitModel.cpp:2473-2514` |
| TX | Mic in use, VOX on, MON on | None | runtime | Never | Never; always safe default | reverts | unknown | reverts | n/a | sticks | unknown | by design | Comment `TransmitModel.cpp:2272-2275`, `RadioModel.cpp:20996` |
| VOX | VOX threshold, gain, hang; DEXP; anti-VOX; monitor volume | `hardware/<mac>/tx/Dexp_Threshold`, `VOX_GainScalar`, `VOX_HangTime`, `DEXP_*`, `AntiVox_*`, `MonitorVolume` | per radio MAC | Setter | connect; re-primed into WDSP after TX setup | sticks | sticks | sticks | n/a | sticks | unknown | sticks | Save `TransmitModel.cpp:3134-3183`, `3240`, `3283-3312`, `3343-3508`. Restore `TransmitModel.cpp:2368-2456`. Reach `RadioModel.cpp:9081-9100` |
| TX EQ | EQ on, preamp, bands, freqs, parametric data, legacy box; leveler; ALC | `hardware/<mac>/tx/TXEQEnabled`, `TXEQPreamp`, `TXEQ<n>`, `TxEqFreq<n>`, `eq/*`, `TXParaEQData`, `EQUseLegacy`, `Lev_*`, `ALC_*` | per radio MAC | Setter | connect; pushed at TX channel setup | sticks | sticks | sticks | n/a | sticks | unknown | sticks | Save `TransmitModel.cpp:3807-3981`. Restore `TransmitModel.cpp:2560-2617`. Reach `RadioModel.cpp:9083`, pushTxProcessingChain |
| CFC | CFC, phase rotator, compander, AM carrier, CESSB | `hardware/<mac>/tx/CFC*`, `cpdr/on`, `CompanderLevel`, `AM_Carrier_Level`, `CESSB_On` | per radio MAC | Setter | connect; pushed at TX channel setup | sticks | sticks | sticks | n/a | sticks | unknown | sticks | Save `TransmitModel.cpp:4231-4403`, `4474-4513`. Restore `TransmitModel.cpp:2621-2684` |
| TX | TX filter low and high | `hardware/<mac>/tx/FilterLow`, `FilterHigh` | per radio MAC | Setter, including the RADE snap | connect | sticks | sticks | sticks | n/a | sticks | unknown | reverts | Save `TransmitModel.cpp:4539-4556`. Restore `TransmitModel.cpp:2689-2692`. Reverts through RADE: Clear bug 4, `RadioModel.cpp:25669-25682` |
| TX | Active TX profile name | `hardware/<mac>/tx/profile/` active key | per radio MAC | On select | Name read when shown. Values are not reapplied at connect; live values come from the `tx/` keys | sticks | sticks | sticks | n/a | sticks | unknown | sticks | Save `src/core/MicProfileManager.cpp:880`, `1094-1098`. Restore `MicProfileManager.cpp:867-872`, `RadioModel.cpp:8050-8056`. Full profile bank belongs to the TX sweep |
| PureSignal | Armed | None | runtime | Never | Never; profile recall only | reverts | sticks | reverts | n/a | sticks | unknown | by design | Comment `TransmitModel.cpp:2350-2365`, setter `559-565`. Design call 5 |
| PureSignal | Auto-cal, auto and quick attenuate, delays, peak override | `hardware/<mac>/pureSignal/AutoCalEnabled` and siblings | per radio MAC | configurationChanged | Model ctor and connect | sticks | sticks | sticks | n/a | sticks | unknown | unknown | Save `src/models/PureSignalSettings.cpp:292-307`, `RadioModel.cpp:2585-2588`. Restore `PureSignalSettings.cpp:197-205`, `RadioModel.cpp:20825`, `21603`. Only auto-cal was traced as far as the engine |
| TX | Two-tone settings | `hardware/<mac>/tx/TwoTone*` | per radio MAC | Setter; verb asks for a flush | connect | sticks | sticks | sticks | n/a | sticks | unknown | sticks | Save `TransmitModel.cpp:3527-3617`, `src/core/session/SessionCommandDispatcher.cpp:1555-1558`. Restore `TransmitModel.cpp:2526-2555` |
| CW | Keyer, break-in, sidetone, serial key, CWX memories | `hardware/<mac>/tx/CW_*`, `CWX_*` | per radio MAC | Setter | connect | sticks | sticks | sticks | n/a | sticks | unknown | unknown | Save `TransmitModel.cpp:4697-4960`. Restore `TransmitModel.cpp:2795-2848`. Keys match by script; reach to the radio not traced here |
| TX | Which slice transmits | `hardware/<mac>/TxBoundSliceId` | per radio MAC | Teardown only | Connect; falls back to A if that slice is absent | reverts | sticks | sticks | n/a | sticks | unknown | reverts | Save `src/core/TxSliceArbiter.cpp:529-535`, `RadioModel.cpp:29936`. Restore `TxSliceArbiter.cpp:231-234`, `537-545`. Desktop only: TX on a second slice of a pan falls back, follows Design call 2 |
| TX | Any TX edit before the first connect of a run | Dropped: `m_persistMac` is empty | none | Never | loadFromSettings overwrites at connect | reverts | reverts | n/a | n/a | n/a | n/a | unknown | `TransmitModel.cpp:2288`, `2298`. Only mic source has a pre-connect store. Whether controls are live offline is not verified: Clear bug 7 |

Not listed: `TransmitModel::setTxOwnerSlot` and its `tx/OwnerSlot` key have no caller outside TransmitModel, and the no-argument `loadFromSettings` has no caller found. No user control reaches them. `setSwrProtectFactor`, `setLastPower`, MOX and TUNE are runtime only by their own comments.

#### What a NEW slice inherits against what a RESTORED slice gets

A new slice takes one of two paths in `addSliceImpl`. A slice with nothing saved under its letter takes only what the first column shows. A letter used before also runs `restoreFromSettings(savedLetterBand, false)` at `RadioModel.cpp:17164-17169`, which the other session owns. Only frequency, mode, filter and sample rate are skipped there (`SliceModel.cpp:3727`, `3784`, `3783`, `3854`).

A restored slice takes one of three paths: desktop starting slice (`loadSliceState`, `RadioModel.cpp:26803`), desktop slice made for an empty pan (`RadioModel.cpp:17062-17068`), or Core hydrate (`restoreReceiveState`, `SliceModel.cpp:3389`). All three call `restoreFromSettings` with tuning, so the third column reads as one.

| Family | New slice, letter never used | New slice, letter used before | Restored at startup |
|---|---|---|---|
| Frequency | Active slice's | Active slice's | Its own, last band |
| Mode | Active slice's | Active slice's | Its own, last band |
| Filter | Not the active slice's. USB seed keeps the 100 to 3000 constructor filter. Any other mode gets that letter's pair for the band and mode, else the mode default | Same, so a remembered USB filter is ignored and later overwritten: Clear bug 3 | Its own, per band and mode |
| AGC mode and detail | Constructor default (Med, threshold -20) | The letter's values from the letter's LAST band, not the band it opens on: Clear bug 3 | Its own, per band |
| AGC-T AUTO, offset | Default off | The letter's own | Its own |
| NB mode | Default Off | The letter's last band's | Its own, per band |
| NB1, NB2, SNB tuning | Defaults (or the old global) via `SliceModel.cpp:4073-4092`, only when the restore runs; otherwise constructor values | The letter's own | Its own |
| NR selection and all NR knobs, SNB on, ANF on | Active slice's: `copyNoiseReductionFrom`, `SliceModel.cpp:2333-2377` | Active slice's, over the letter's | Its own, independent of the active slice (`RadioModel.cpp:17154-17155`) |
| Squelch | Default | The letter's own | Its own |
| APF, FM, DIG, RTTY | Default | The letter's own | Its own |
| RIT and XIT | Default off | The letter's saved RIT and XIT, today, through `SliceModel.cpp:4099-4110`. The ruling says off. Other session owns it | Its own, as ruled |
| Step | Default 100 Hz | The letter's last band's | Its own, per band |
| AF gain, RF gain | Default 50, 80 | The letter's own | Its own |
| Mute, lock | Default off | The letter's own, so a letter closed muted or locked opens that way | Its own |
| Pan center | Follows its frequency when bound | Same | Slice frequency |
| Zoom, CTUN | The pan's own keys when the pan is new; shares the pan otherwise. Not traced further | Same | The pan's own keys |
| Antennas | Setup's table for the band it opens on (`RadioModel.cpp:17076`) | Same | Setup's table for its band |
| VAX channel, output route | The letter's own keys or default (`RadioModel.cpp:16978`) | The letter's own | Its own |
| Sample rate | Not restored, follows the stream | Not restored | Controller's row |
| TX power, mic, EQ, CFC, PureSignal, VOX | Not per slice. Follows the transmit slice's band and the radio's keys | Same | Same |

In short: a new slice inherits frequency, mode and noise reduction from the slice you are on, and nothing else. Everything else comes from defaults or from whatever that letter last held.


## Core, stations and connect lists

Read from source only, tree at dfa9e1c29. No Core, window or settings file was run or opened. Paths are relative to the repository root.

Short answers to the questions in the dispatch. The rows below carry the evidence.

- **What lives where.** `classifySettingsKey` (src/core/settings/SettingsScope.cpp:888-911) decides. Station keys (`hardware/`, `Slice`, `Vfo`, `Notch`, `filters/`, `DspOptions`, `Nb`, `Snb`, `Rade`, `PGXL_`, `TGXL_`, `RfKit_`, `DxCluster`, `Rbn`, `Pota`, `PskReporter`, `FreeDv`, `Roundtable`, `User/`, `StationTci_`, `DisplayGridMax_`, `DisplayGridMin_`, and the whole keys at SettingsScope.cpp:538-704) live on the Core when a window is connected to one. Everything else defaults to the window's own file, including `radios/`, `ConnectionTargets/`, `RemoteVax/`, `RemoteRigctld/`, `Tci`, `Wsjtx` and `SpotCollector`.
- **What a window pushes on connect.** Nothing. It pulls a settings snapshot and a property snapshot from the Core. The proxy stays not ready until the snapshot lands (StationClient.cpp:4248-4256) and property changes go out only after SnapshotComplete (StationClient.cpp:3156).
- **When the Core's file is written.** Whole-file saves only: a 500 ms timer after slice, transmit, notch and radio speaker changes (RadioModel.cpp:29345-29385, StationServer.cpp:2115-2127, 9232-9241), a few immediate saves (rows C16, C17, C22, C23), and at a clean stop (DaemonApp.cpp:655, 663). SIGTERM and SIGINT reach the clean stop (server_main.cpp:356-361, 744). SIGKILL, a crash or a power cut loses whatever is only in memory. systemd sends SIGKILL if stopping takes over 15 s (packaging/nereusd.service.in:207).
- **Slices when a device leaves.** See C7 to C11. Leaving on purpose closes them at once and saves a copy for that device. A paired device that drops keeps them for 180 s, then the same. The copy comes back through `restoreSliceFor` at the next admission.
- **Stale values on reconnect.** No overwrite of the Core was found on the paths read. Offline edits are dropped and the Core's value wins (W10, W11, W12).

| ID | Surface | Setting | Store (exact key or file) | Owner | Saved when | Restored when | Reopen | Reconnect | Core restart | Band change | Add slice/pan | Phone background | Verdict | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| C1 | Window or phone, any Core setting sent as a settings write or remove | Setup pages and raw keys: FFT size and rate keys, grid dB, `hardware/<mac>/...` not model-owned (sample rate included), SWR and time-out keys, band plan, spot sources, `filters/`, `User/`, and so on | Core `NereusSDR.settings`, the key as sent | Core (per radio MAC for `hardware/`) | Memory at once. Disk only at the next unrelated save or a clean stop | Core load at start, then settings snapshot to each device | sticks | sticks | reverts on kill, crash or power cut; sticks on clean stop | n/a | n/a | sticks | reverts | Save: StationServer.cpp:9526-9610 and 9725-9727, SettingsProxyServer.cpp:342 (no save call in either). Clean stop: DaemonApp.cpp:663. Restore: StationServer.cpp:8641, StationClient.cpp:4233. Apply: StationServer.cpp:9569-9607. Each key's reader not checked one by one |
| C3 | Window or phone, mirrored slice properties | VFO, mode, filter, AGC, NR, RIT/XIT and the rest of SliceModel's 105 two-way properties | `Slice<N>/...`, `Slice<N>/Band<key>/...`, `hardware/<mac>/slices/<N>/...` | Core, per slice and band | 500 ms after the change | When the slice is made (`loadFromSettings`) | sticks | sticks | sticks | unknown (slice sweep) | unknown (slice sweep) | sticks | sticks | Save: RadioModel.cpp:25910 onward (wireSliceSignals), 29345-29385, 29620-29627. Restore: RadioModel.cpp:16978. Body of `loadFromSettings` left to the slice sweep |
| C4 | Same, while the saved layout is marked protected | Every slice setting, antenna flush, per-band tune power | Same keys, not written | Core | Never for the rest of that Core run | Older values at next start | sticks | sticks | reverts | reverts at restart | n/a | sticks | reverts | Latch set: RadioModel.cpp:13866, 14019, 22562, 28937, 29032, 29060, never cleared (RadioModel.h:8990). Gates: RadioModel.cpp:29350, 29440, 29589, 29622 |
| C5 | Same, while the Core waits for its radio | Slice settings edited before the radio is admitted | Same keys | Core | Held, then saved when admission finishes | As C3 | sticks | sticks | sticks unless killed before admission | n/a | n/a | sticks | sticks | RadioModel.cpp:29350, 29090-29092 |
| C6 | Core restart manifest | Which slices are live, letter, pan key, frequency, mode, owner | `hardware/<mac>/receiveLayout` | Core, per radio MAC | With each slice save and each owner change | Core start, then admission when the radio connects | n/a | sticks | sticks | n/a | sticks | sticks | sticks | Save: RadioModel.cpp:29113-29142, 2406-2407. Restore: RadioModel.cpp:28920-28929. Owner kept: ReceiveLayoutStore.h:38-46. An empty Core forgets it by ruling Q10: RadioModel.cpp:29102-29107 |
| C7 | Paired window quits or phone taps Disconnect, radio on | That device's slices, with every band's settings | `hardware/<mac>/deviceLayouts/<device>` | Core, per device and radio | At the close, file 500 ms later | Next admission of that device | sticks | sticks | sticks | n/a | n/a | n/a | sticks | Leave: MainWindow.cpp:2454, ios ConnectionFlow.swift:1703. Close and copy: StationServer.cpp:6092-6093, 12475-12479, 12364-12378, DeviceLayoutStore.cpp:216-232. Restore: StationServer.cpp:12278-12304, RadioModel.cpp:19974-20000, 16978 |
| C8 | Same, or a dropped device's 180 s ends, radio off | Same | Nothing saved, `Slice<N>/...` deleted | Core | Never | Never | reverts | reverts | reverts | n/a | n/a | reverts | reverts | StationServer.cpp:12364-12378, RadioModel.cpp:10232-10234. See Clear bugs 1 |
| C9 | Paired device signs in to a headless Core, radio off | Its saved slices from C7 | `hardware/<mac>/deviceLayouts/<device>` kept but not read | Core | n/a | Only at a later admission with the radio on | reverts | reverts | reverts | n/a | n/a | reverts | reverts | StationServer.cpp:12259-12266, 12276-12278, 4198. See Clear bugs 4 |
| C10 | Device link drops: phone in background and suspended, Wi-Fi loss, window crash | Its slices stay live and owned for 180 s | Live state, `receiveLayout` | Core, per device | C3 and C6 | On return, same slices. After 180 s, as C7 | n/a | sticks | sticks | n/a | n/a | sticks | sticks | StationServer.cpp:6083-6095, 3616-3628. Phone sends no leave on a scene change: ios NereusSDRApp.swift:234-242 |
| C11 | Window signed in with the older token | Its slices | Not saved | Nobody | Never | Kept for adoption only when it was alone on the Core | reverts when others are on the Core | same | n/a | n/a | n/a | n/a | by design: "cannot be recognised" | StationServer.cpp:12465-12471, DeviceLayoutStore.h:28-30 |
| C12 | Any device | A new slice on a freed letter starts from defaults; transmit selection and C-Tune pins end with the device | Letter's keys cleared | Core | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | by design: rulings 5.3 and Q8 | StationServer.cpp:12376-12377, 12442-12454 |
| C13 | Window or phone, mirrored transmit properties | TransmitModel's 110 two-way properties except keying | `hardware/<mac>/tx/<key>` and per-band tune power | Core, per radio MAC | Memory at once, file 500 ms later. Nothing kept if no radio has connected this run | `loadFromSettings(mac)` at radio connect | sticks | sticks | unknown | unknown | n/a | sticks | unknown | Save: TransmitModel.cpp:2286-2294, StationServer.cpp:2115-2119, 9240. Restore: RadioModel.cpp:20997, TransmitModel.cpp:2296-2298. Whether values reach the DSP left to the TX sweep. See Clear bugs 6 |
| C14 | Window or phone, `stepAtt` object | Step attenuator, preamp, auto-attenuate | `hardware/<mac>/options/{stepAtt,autoAtt,preamp}/...` | Core, per radio MAC | Debounced, then background save, and at teardown | `loadSettings(mac)` at connect | sticks | sticks | unknown | unknown | n/a | sticks | unknown | Save: StepAttenuatorController.cpp:170-177, 192-208. Restore: DaemonApp.cpp:829, body and radio apply not read. See Design calls 3 |
| C15 | Window or phone, `alexAntennas` object | RX and TX antennas per band, block TX, relays | `hardware/<mac>/alex/antenna/...` | Core, per radio MAC | With a slice save (`m_alexControllerDirty`), and teardown. Blocked by C4 | `m_alexController.load()` at connect | sticks | sticks | unknown | unknown | n/a | sticks | unknown | Save: RadioModel.cpp:2681, 2731, 29634-29637. Restore: RadioModel.cpp:20939-20940, radio apply not read. See Design calls 3 |
| C16 | Window or phone | Radio speaker level, mute, amp mode. Core speaker level, mute, device, details | Radio speaker per MAC; Core speaker under `coreSpeakerPrefix()` | Core | Radio speaker in memory, file 500 ms later. Core speaker background save at once | At connect | sticks | sticks | unknown | n/a | n/a | sticks | unknown | Save: RadioModel.cpp:23924-23938, 24162, 24185, 24222, 24277. Restore: RadioModel.cpp:21357, 23948, 24378, 24419, bodies not read |
| C17 | Window, VAX applet on a Core | Core VAX channel gains, mutes, TX gain | Keys from StationVax::rxGainKey, mutedKey, txGainKey | Core | Background save at once | Restore not read | sticks | sticks | unknown | n/a | n/a | n/a | unknown | Save: StationVaxFacade.cpp:282-288, 308, 325, 340 |
| C18 | Window or phone | Notch list and flags, PureSignal settings | `Notch*`, `hardware/<mac>/puresignal/...` | Core | Notches: `notches` asks for a save. PureSignal: `configurationChanged` | PureSignal load at connect. Notch load not read | sticks | sticks | unknown | n/a | n/a | sticks | unknown | StationServer.cpp:2120-2122, RadioModel.cpp:2584, 2587, 20825, NotchModel.cpp:731 |
| C19 | Window or phone | Pan center, bandwidth, dB floor and ceiling (mirrored two-way) | No center or bandwidth key found in PanadapterModel.cpp or ReceiveLayoutStore.cpp. Grid keys: `DisplayGridMax_<band>`, `DisplayGridMin_<band>` | Core | Grid keys through PanadapterModel, memory only | Grid on band change; center follows the slice | unknown | sticks | unknown | unknown | unknown | sticks | unknown | PanadapterModel.cpp:491-498, RadioModel.cpp:26848. Display sweep should settle this |
| C20 | nereusd.conf | `sample_rate_hz`, `audio_device`, `radio_mac`: starting values only | nereusd.conf, never written by the Core | Core operator | Hand edit | Used only until a saved value exists | n/a | n/a | by design: saved value wins | n/a | n/a | n/a | by design: ruling 2026-09-27, G-16, I6 | DaemonApp.cpp:288-292, 956-1019, DaemonConfig.cpp:50 |
| C21 | nereusd.conf | Other keys: `remote_port`, `core_name`, `remote_bind`, `rendezvous_servers`, `relay`, `status_page`, `status_port`, `state_directory`, and so on | nereusd.conf | Core operator | Hand edit | Each start | n/a | n/a | sticks | n/a | n/a | n/a | sticks | DaemonConfig.cpp:194-209 and the parser around it. Not settable from a window |
| C22 | Window or phone, This Core | Which radio the Core uses | `StationRadioChoice` | Core | Background save once the choice has connected | Core start | sticks | sticks | sticks | n/a | n/a | sticks | sticks | StationRadios.cpp:179-197, DaemonApp.cpp:288-292 |
| C23 | Window or phone | Core name, key backup acknowledged | `StationLabel`, `StationKeyBackupAcknowledged` | Core | Blocking save with rollback on failure | Acknowledgement read at StationDevicesFacade.cpp:143. Name restore not read | sticks | sticks | unknown | n/a | n/a | sticks | unknown | StationDevicesFacade.cpp:75-84, 312, 326 |
| C24 | Pairing | Paired devices, tokens, Core identity | Files under `cfg.securityDirectory` | Core | Not read | Not read | unknown | unknown | unknown | n/a | n/a | unknown | unknown | StationHost.cpp:160-163. Not traced in this sweep |
| C25 | Core web page | Status page on TCP 47911 | Nothing | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | by design: read-only, answers `GET /` only | StationStatusPage.h:29-31, StationStatusPage.cpp:182 |
| C26 | Import All Settings | Saves held until restart after an import | Imported file | Local store | Later saves are skipped on purpose | Next launch | by design | n/a | n/a | n/a | n/a | n/a | by design: Thetis DB.Merged parity | AppSettings.cpp:934-936, 954-959, 1111 |
| C27 | Settings file repair | A corrupt file falls back to `.bak`, one save behind | `NereusSDR.settings.bak` | Each store | Each save | Load | by design | n/a | by design | n/a | n/a | n/a | by design: issue 241 | AppSettings.cpp:757-798, 859-879 |
| W1 | Window, Connections | Saved Cores, selected Core, addresses, trust, service routes | `ConnectionTargets/V3` (V2 and V1 kept for rollback) | Window | Blocking save on each change, rolled back on failure | App start | sticks | sticks | n/a | n/a | n/a | n/a | sticks | Save: CoreTargetStore.cpp:1193-1209, 1216-1310. Restore: CoreTargetStore.cpp:653, StationStartupSelection.cpp:30-47. Scope: SettingsScope.cpp:423 |
| W2 | Window, Connections | Auto-connect to a saved Core | `autoConnect` inside `ConnectionTargets/V3` | Window | Inside W1 | App start | unknown | n/a | n/a | n/a | n/a | n/a | unknown | Round trip: CoreTargetStore.cpp:324-325, 528-534. Read: StationStartupSelection.cpp:73-80. The control that sets it was not traced |
| W3 | Window, rendezvous | Remote access server list | `RemoteAccessServers`, default when unset | Window | No writer found in src | Each dial | unknown | n/a | n/a | n/a | n/a | n/a | unknown | RemoteConnectionController.cpp:39-55. A restore with no save, so a hand edit only. Per-Core routes are in W1 |
| W4 | Window, Connections | Cores heard on the LAN | Memory only, expires by time | Window | Never | Next announcement | by design | n/a | n/a | n/a | n/a | n/a | by design: a live list | StationLanCache.cpp:47, 118-123 |
| W5 | Window, direct connect | Saved radios: name, address, port, MAC, board, protocol | `radios/<macKey>/...` | Window, per MAC | Blocking save on each change | App start, name layered over discovery | sticks | sticks | n/a | n/a | n/a | n/a | sticks | Save: ConnectionPanel.cpp:1216-1218, 1248-1249, 1328-1329, GuiConnectionController.cpp:948-950, 1027-1028. Restore: ConnectionPanel.cpp:1063-1068 |
| W6 | Window, direct connect | Last-used radio | `radios/lastConnected` | Window | At Connect and when connected, blocking save | App start, Connect menu | sticks | sticks | n/a | n/a | n/a | n/a | sticks | Save: ConnectionPanel.cpp:1217-1218, RadioModel.cpp:30635-30636. Restore: MainWindow.cpp:21015, 12675 |
| W7 | Window, direct connect | Auto-connect tick for a radio | `radios/<macKey>/autoConnect` | Window, per MAC | Blocking save, then forced back on by the next Connect | App start | reverts | n/a | n/a | n/a | n/a | n/a | reverts | ConnectionPanel.cpp:1216, GuiConnectionController.cpp:948, MainWindow.cpp:21015. See Clear bugs 3 |
| W8 | Window, direct connect | Pin to MAC, model override | `radios/<macKey>/pinToMac`, `modelOverride` | Window, per MAC | Blocking save | Connect and auto-connect | sticks | sticks | n/a | n/a | n/a | n/a | sticks | Save: ConnectionPanel.cpp:1213-1216, 1587-1588. Restore: MainWindow.cpp:21015 (tryAutoReconnect), GuiConnectionController.cpp:945-946 |
| W9 | AppSettings | Discovery profile | `radios/discoveryProfile` | Window | No caller in src/gui or src/models | Not read | unknown | n/a | n/a | n/a | n/a | n/a | unknown | AppSettings.h:552. RadioDiscovery not traced |
| W10 | Window on a Core | Core settings edited while the link is down | Proxy cache only | Core | Dropped, not queued | Core's value at the next snapshot | n/a | reverts, with a toast | n/a | n/a | n/a | n/a | by design: Task 19 and review Important 2 | SettingsProxy.cpp:117-131, 175-248, MainWindow.cpp:5079-5083 |
| W11 | Window on a Core | Mirrored properties edited while the link is down | Local model only | Core | Dropped | Core's value in the next snapshot | n/a | reverts, no toast | n/a | n/a | n/a | n/a | by design: "the client retains last-known state". See Design calls 4 | StationClient.cpp:1281-1283, 2479, 2537, 2615-2635 |
| W12 | Window on a Core | Anything local overwriting the Core at reconnect | n/a | Core | n/a | Core's snapshot | n/a | sticks | n/a | n/a | n/a | sticks | sticks | StationClient.cpp:4248-4256, 3156, MainWindow.cpp:6327-6336 (guarded by `ownsLocalDsp`). Key-by-key search covered the FFT, band plan, region and CW keys only, not every Station key |
| W13 | Window on a Core | The window's own keys: layout, colors, sound card | Window's `NereusSDR.settings` | Window | Close, quit, signal, and SpectrumWidget's own 500 ms save | App start | sticks on a clean quit | sticks | n/a | n/a | n/a | n/a | sticks | MainWindow.cpp:2346, 2376, 21413, main.cpp:418, SpectrumWidget.cpp:2464-2480. A remote model has no timed save: RadioModel.cpp:29347. See Design calls 5 |
| W14 | Window on a Core | Remote VAX channel per Core and slice, remote rigctld | `RemoteVax/<core>/Slice<N>/Channel`, `RemoteRigctld/...` | Window | Not read | Not read | unknown | unknown | n/a | n/a | n/a | n/a | unknown | SettingsScope.cpp:429-432, RemoteVaxRouter.cpp:114-117 |
| W15 | Window | Station settings kept for direct use while on a Core | Window's file, unread and unwritten while on a Core | Window | Direct mode only | Direct mode only | by design | n/a | n/a | n/a | n/a | n/a | by design: see Design calls 2 | AppSettings.cpp:1123-1125, 1140-1143, 1189-1197 |


## iPhone and iPad

### Launch and foreground

#### 1. Cold launch, step by step

1. `NereusSDRLaunch.main()` starts the app (`NereusApp/App/NereusSDRApp.swift:20-29`).
2. `NereusSDRApp.init` builds `AppModel.live()` (`NereusSDRApp.swift:68`, `NereusApp/App/AppModel.swift:468-476`).
   `AppModel.init` (`AppModel.swift:275`) makes an empty `MirrorStore` and sets `audioMuted = false`
   (`AppModel.swift:49`). It asks for sound (`AppModel.swift:374`) and applies the kept microphone and "while you
   transmit" choices (`AppModel.swift:428-430`).
3. Inside that, the phone's own stores load: band display settings (`NereusApp/Main/MainScreenModel.swift:245`),
   S-meter (`NereusApp/iPad/SMeterModel.swift:56`), keyed meters (`NereusApp/Main/KeyedMetersModel.swift:15`), spots
   (`NereusApp/Spots/SpotsModel.swift:302-303`), FreeDV Reporter (`NereusApp/FreeDV/FreeDVReporterModel.swift:160-165`).
4. `ConnectionFlow.init` reads the device key and the paired Cores from the Keychain
   (`NereusApp/Connect/ConnectionFlow.swift:740`, `:743`, `:3550`). It then picks the first screen, and this line is
   the whole decision: `screen = cores.isEmpty && notice == nil ? .welcome : .cores` (`ConnectionFlow.swift:744`).
   It starts browsing the network (`ConnectionFlow.swift:803-807`). **It never dials.** A Keychain read that fails
   counts as no Cores (`ConnectionFlow.swift:3549-3554`).
5. The long session starts, which applies the screen idle rule (`NereusSDRApp.swift:128`).
6. `RootView` is created with `selection = .panadapter` (`NereusApp/App/RootView.swift:24`, `:39-40`). The scene
   phase handler runs once at start (`NereusSDRApp.swift:234`).
7. **First screen:** `YourStationsScreen`, or `WelcomeScreen` when nothing is paired (`RootView.swift:93-105`).
8. The operator taps Connect (`NereusApp/Connect/YourStationsScreen.swift:350`, into `ConnectionFlow.swift:871`).
9. When the session is ready, `connected()` sets `screen = .band` (`ConnectionFlow.swift:2402`, `:2419-2421`). The
   tabs appear on Panadapter with all `MainScreen` state at its defaults (`NereusApp/Main/MainScreen.swift:90-133`).
   The band centers on the active slice at the kept zoom (`NereusApp/Main/BandSubscriber.swift:544-545`).

#### Returning from the background

**The app does not drop the connection when backgrounded.** The scene handler does exactly four things, and none of
them touches the session (`NereusSDRApp.swift:234-242`):

| Call | Effect |
| --- | --- |
| `AppSceneDiagnostics.changed` | Diagnostics only (`NereusSDRApp.swift:270-279`). |
| `activity.sceneChanged` | Refreshes the Live Activity on return (`NereusApp/Activity/LiveActivityController.swift:284-292`). |
| `longSession.sceneChanged` | Inactive counts as foreground; only `.background` counts as away (`NereusSDRApp.swift:237-239`, `NereusApp/Session/SessionController.swift:181-192`). |
| `transmit.sceneChanged` | Releases a held PTT when the scene is not active (`NereusApp/Main/TransmitModel.swift:937-941`). |

Going away:

- Sound keeps running. `audio` is a background mode (`NereusApp/Info.plist:43-48`). The audio session runs from
  `.ready` to `.stopped`, and through a retry wait it plays silence "so the app stays alive in the background"
  (`AppModel.swift:1268-1278`). Muting asks the Core for no sound but does not stop this (`AppModel.swift:448-454`).
- The display is dropped by default. "Sound only" is on by default (`NereusApp/App/PhoneSettings.swift:241-244`), so
  the policy asks for no endpoints (`NereusKit/Sources/NereusMedia/SessionPolicy.swift:179-180`) and the subscriber
  holds none (`BandSubscriber.swift:416-419`).
- Nothing in the app is saved at this moment. There is no save on background, and none is needed for the stores that
  exist, since each writes at the moment of change.

Coming back with the process alive:

- The band subscribes again (`BandSubscriber.swift:147-148`) and the waterfall gets an away mark such as "Locked
  19:42 to 20:15" (`SessionController.swift:346-352`, `:399`, `:404-411`).
- All tabs, pages, panels and sheets are exactly as left, because they are still in memory.
- **There is no reconnect on foreground.** `ConnectionFlow` never sees the scene phase; the only scene phase readers
  are the app, the Radio tab, Rotor and Roundtable. If the link died while suspended, the LINK LOST cover shows and
  waits for the session's retry schedule of 1, 2, 5, 10, 30, 60 s
  (`NereusKit/Sources/NereusLink/ReconnectPolicy.swift:4-8`). Only a network path change redials early
  (`ConnectionFlow.swift:3029-3053`). The timer sleeps on the continuous clock (`NereusKit/Sources/NereusLink/SystemLinkClock.swift:4`), so time spent
  suspended should count and the wait should be at most one 60 s step.

When the process does end. This part is iOS behavior I am inferring, not something in this code:

- iOS can suspend the app whenever sound is not running: on the Cores screen with no session (`AppModel.swift:1274-1275`),
  or after an interruption that does not resume, which moves to `.paused`
  (`NereusApp/Audio/AudioSessionController.swift:484-487`). A suspended app can later be killed.
- A swipe up in the app switcher and every newly installed build are also cold launches.
- All of these lead back to the steps above.

On the Core side, a session that ends without leaving is away for 180 s and keeps its place
(`src/core/session/DeviceSessionRegistry.h:41-42`). Disconnect sends `session.leave` (`ConnectionFlow.swift:1761-1770`)
and frees the place at once. Either way the slices are saved for the paired device and given back at its next
admission (`src/core/session/StationServer.cpp:12475-12479`, `:12272-12301`), so frequency and mode survive even a
long absence.

#### 2. What comes back, thing by thing

Owner column: **memory** = phone RAM only, **phone** = saved on this device, **Core** = sent by the Core.

| Thing | Cold launch | Foreground | Owner | Evidence |
| --- | --- | --- | --- | --- |
| Selected tab | **Reset** to Panadapter | Kept | memory | `RootView.swift:24`, `:39` |
| Setup page | **Reset** to the list | Kept | memory | `NereusApp/Setup/SetupRouter.swift:15` |
| Tools page, Radio page | **Reset** to the list | Kept | memory | `NereusApp/App/ToolsTab.swift:55`, `NereusApp/Radio/RadioTab.swift:53` |
| RX, TX and Sound panels | **Reset** closed | Kept | memory | `MainScreen.swift:90-92` |
| Pan, Display and Slices sheets | **Reset** closed | Kept | memory | `MainScreen.swift:93`, `NereusApp/Main/DropSheet.swift:7-12` |
| Band menu, column menu, Roundtable sheet | **Reset** closed | Kept | memory | `MainScreen.swift:99`, `:104`, `:122` |
| Display pill's tab | **Reset** to Spectrum | Kept | memory | `MainScreen.swift:95` |
| Active slice | Re-fetched (which slice after a saved-layout restore: unknown) | Kept | Core | `NereusApp/Band/BandSlicesModel.swift:144`, `:1336-1337` |
| Slice shown on this phone only (the jump) | **Reset** | Kept | memory | `BandSlicesModel.swift:152`, `:155` |
| Band, frequency, mode, filter, DSP | Re-fetched | Kept, live | Core | `StationServer.cpp:12272-12301` |
| Zoom | Restored | Kept | phone | `NereusApp/Band/BandModel.swift:1036-1041`, `BandSubscriber.swift:545` |
| Pan (the view's center) | **Reset**, centered on the active slice | Kept | memory | `BandModel.swift:498`, `BandSubscriber.swift:544-555` |
| Spectrum height, waterfall share | Restored | Kept | phone | `NereusKit/Sources/NereusBand/BandDisplaySettingsStore.swift:51-54`, `NereusApp/Main/DisplaySheetModel.swift:358-363` |
| Colors, palettes, waterfall levels, overlays, averaging | Restored | Kept | phone | `BandDisplaySettingsStore.swift:42-47`, `:60-80`, `MainScreenModel.swift:245` |
| dB Max and dB Min for the band | Re-fetched (older Core: phone) | Kept | Core | `NereusApp/Main/CoreBandScale.swift:9-23` |
| FFT size, window, Hz per bin, frames per second | Re-fetched | Kept | Core | `NereusApp/Main/CoreDisplayModel.swift:100-104` |
| Last Core connected to | **Not stored.** Only each Core's last address is | Kept | none | `ConnectionFlow.swift:744`, `NereusKit/Sources/NereusLink/PairedStation.swift:176-183` |
| Auto-connect on launch | **Does not exist** | n/a | none | `ConnectionFlow.swift:730-808` has no dial |
| iPad arrangement (column or front panel) | Computed from the window | Computed | none | `NereusApp/iPad/IPadLayout.swift:31-37` |
| iPad column shown, button column folded, wide Roundtable | **Reset** | Kept | memory | `MainScreen.swift:101`, `:106`, `:110`, `:129`, `:153` |
| iPad open pans, pan last touched | **Reset** to Pan 1 | Kept | memory | `NereusApp/Main/PanStackModel.swift:70`, `:73` |
| iPad Roundtable applet position | Restored | Kept | phone | `PhoneSettings.swift:340-345` |
| S-meter face, modes, peak hold, units | Restored | Kept | phone | `NereusKit/Sources/NereusBand/Meter/SMeterSettingsStore.swift:25`, `:57` |
| Keyed meter page and slots | Restored | Kept | phone | `KeyedMetersModel.swift:15`, `:34` |
| Meter readings | Re-fetched live | Live | Core | Follows from the mirror starting empty (`AppModel.swift:297`); not traced further |
| Mute this phone | **Reset** to unmuted | Kept | memory | `AppModel.swift:49` |
| Play the band through (route) | **Reset** to the iOS choice | Kept | memory | `NereusApp/Audio/AudioSessionController.swift:127` |
| Waterfall history | **Reset** empty | Kept, with an away mark | memory | `SessionController.swift:399` |

#### Every persistent key

There is **no** `@AppStorage`, `@SceneStorage`, `NSUserActivity`, state restoration or iCloud store anywhere in
`NereusApp`, `NereusKit` or `Shared`. All of it lives only on this device; nothing below is sent to the Core.

**Keychain**, service `NereusSDR` (`NereusKit/Sources/NereusLink/KeychainItem.swift:23`):

- `paired-cores` (`PairedStationStore.swift:12`)
- `device-key-secure-enclave` (`SecureEnclaveKeyStore.swift:12`)
- `device-key` (`KeychainKeyStore.swift:11`)

**UserDefaults under `phone.`**, through `PhoneSettings` (`PhoneSettings.swift:44`):

- `phone.touch.` + `dragToTune`, `tapToTune`, `snapTapToStep`, `pinchToZoom`, `doubleTapAction`
- `phone.dial.` + `kind`, `reversed`, `detentTicks`, `kilohertzBumps`
- `phone.audio.` + `microphone`, `quality`, `muteBandWhileTalking`
- `phone.connect.` + `deviceName`, `coresNeedingPairing`, `madeNewKey`, `coresWithoutRename`
- `phone.session.` + `soundOnlyWhenAway`, `keepScreenOn`, `sleepTimer`, `lowPowerDropsToSaver`, `hotPhoneSlows`
- `phone.data.` + `wifiMode`, `cellularMode`, `cellularWarning`, `cellularNoteShown`
- `phone.data.month`, and `phone.data.month.` + `cellularIn`, `cellularOut`, `wifiIn`, `wifiOut`, `warned`
  (`NereusApp/Session/DataUseMeter.swift:160-165`)
- `phone.roundtable.` + `signalFormat`, `firstRunNoteSeen`
- `phone.applets.roundtablePosition`
- `phone.swipe.` + `leftEdge`, `rightEdge`, `upTabBar`
- `phone.modMonitor.` + `source`, `posFlashPct`, `negFlashPct`, `meterStyle` (`NereusApp/Main/ModMonitorModel.swift:98-101`)
- `phone.spots.display`, `phone.rotor.turnOnTune` (`SpotsModel.swift:285-287`)
- `phone.freedv.` + `distanceMiles`, `frequencyKhz`, `bandFilter`, `follow` (`FreeDVReporterModel.swift:69-72`)
- `phone.diversity.memory.<band>.<slot>` (`NereusApp/Tools/DiversityModel.swift:545`)

**UserDefaults with their own full key:**

- `phone.band.display.<pan>`, with Pan 1 as `phone.band.display.1` (`BandDisplaySettingsStore.swift:15`,
  `BandSubscriber.swift:71`)
- `phone.display.master.v29`, `phone.display.shared.v29`, `phone.display.revision.v29`, `phone.display.scopeVersion`,
  `phone.display.deviceRowsShared` (`BandDisplaySettingsStore.swift:24-29`)
- `phone.smeter.settings` (`SMeterSettingsStore.swift:14`)
- `phone.keyedMeters.settings` (`NereusApp/Main/KeyedMeterSettings.swift:100`)
- `SetupFilterPresetsMode`, `SetupPaTempUnit` (`NereusApp/Setup/PhoneSetupKeys.swift:49`, `:55`). No `phone.` prefix.
- `display/showPaValuesPage` (`NereusApp/Setup/PaValuesModel.swift:35`). No `phone.` prefix.
- Debug only, read and never written: `NereusRowClock` (`NereusApp/Band/BandView.swift:99`), `NereusTraceRows`
  (`NereusApp/Band/WaterfallLineTrace.swift:35`)

**Files in Application Support:** `Measurements/measurements.csv` (`NereusApp/Diagnostics/MeasurementLog.swift:73`),
`DisplayPictures/<uuid>.<ext>` (`NereusApp/Setup/DescribedPage.swift:473-480`).

Everything else the band and panels show comes from the Core through the mirror and the settings proxy
(`AppModel.swift:60-64`), and the mirror starts empty on each launch.

#### 3. The predetermined baseline

Every launch starts from this, all of it plain `@State`, `@Published` or a default value:

| What you see | Value | Evidence |
| --- | --- | --- |
| First screen | Your Cores, or Welcome with nothing paired | `ConnectionFlow.swift:735`, `:744` |
| Connection | None, waiting for a tap | `AppModel.swift:42` |
| Tab | Panadapter | `RootView.swift:39` |
| Setup, Tools, Radio | At their lists | `SetupRouter.swift:15`, `ToolsTab.swift:55`, `RadioTab.swift:53` |
| Panels and sheets | All closed | `MainScreen.swift:90-93` |
| Display pill | Spectrum tab | `MainScreen.swift:95` |
| Band menu, column menu, Roundtable sheet | Closed | `MainScreen.swift:99`, `:104`, `:122` |
| Button column | Unfolded | `MainScreen.swift:106` |
| iPad applet column | Shown, narrow | `MainScreen.swift:153`, `:129` |
| Pans | Pan 1 only, last touched Pan 1 | `PanStackModel.swift:70`, `:73` |
| Band view | Centered on the active slice | `BandSubscriber.swift:544-545` |
| Waterfall | Empty, no look back | `BandModel.swift:431` |
| Sound | Unmuted, route left to iOS | `AppModel.swift:49`, `AudioSessionController.swift:127` |
| Jump to another slice | Off | `BandSlicesModel.swift:152`, `:155` |
| Setup's Display scope | Pan 1, no chosen band | `MainScreenModel.swift:37`, `:40` |

The comment in `RootView.swift:18-19`, "so it comes back as it was left", is true only inside one run of the process.

One side effect: leaving the band by Disconnect or the sleep timer also throws away `MainScreen`, Tools and Radio
state, because `tabs` leaves the view tree (`RootView.swift:114-115`, `ConnectionFlow.swift:1807-1818`). The tab
selection and the Setup path survive that, since `RootView` owns them (`RootView.swift:24`, `:27`).

#### 4. iPhone and iPad differences

- **Stores:** the same keys. Each device has its own and nothing syncs (`PhoneSettings.swift:47-48`).
- **Swipe defaults:** iPhone has the RX panel and TX panel on the edges. iPad has Off on both. Both have Roundtable on
  the tab bar swipe (`PhoneSettings.swift:388-389`).
- **Pans:** only an iPad can open more than one (`MainScreenModel.swift:258`, `PanStackModel.swift:202-205`). So only
  the iPad loses open pans at launch. Slices come back from the Core with their pan key, and with Pan 1 alone they all
  draw on the one band (`PanStackModel.swift:51-52`, `StationServer.cpp:12293`). A pan's own display values are kept
  and return when that pan is opened again (`PanStackModel.swift:457`).
- **Layout state:** the iPad adds applet column shown, Roundtable widened and applet order. Only applet order is saved
  (`PhoneSettings.swift:340-345`). The folded button column is also reachable on an iPhone held sideways
  (`MainScreen.swift:196`), and it resets on both.
- **Orientation:** never saved (`MainScreenModel.swift:559`). Each orientation keeps its own spectrum height
  (`BandDisplaySettingsStore.swift:52`) on both devices.
- **Setup:** one path on both, pushed on iPhone and shown beside the list on a regular-width iPad
  (`SetupRouter.swift:6-12`), so it is lost equally.
- **Connecting:** the same flow, and neither auto-connects. The device says phone or tablet when signing in
  (`NereusSDRApp.swift:70`).
- **Foreground and background:** the same code on both.

#### What a user would reasonably expect back

In the order I would do them. Each is argued under Design calls below.

1. Back on the last Core without tapping, unless they chose to leave (DC1).
2. Same tab (DC2).
3. Same Setup or Tools page (DC3).
4. iPad pans (clear bug 1) and iPad column state (DC4).
5. Mute and audio route (DC5, clear bug 2).
6. The Display pill's tab and the band's pan position (DC4, DC6).
7. A quick reconnect on return when the link died (DC7).

Not expected back: menus, transient sheets, waterfall history, a sleep countdown, PTT (DC4, DC8).

Counts: 31 sticks, 16 reverts, 7 unknown, 5 by design. Most unknowns are Core-owned values left to the Core sweep.

Phone-owned values have no Core dependency, so their Core restart cell is sticks. PhoneSettings keeps no copy in
memory: every getter reads UserDefaults (`PhoneSettings.swift:56-86`), so a value there reaches its consumer as soon
as that consumer reads it. UserDefaults writes happen at the change with no debounce in this code. iOS flushes them
to disk on its own schedule, so a crash in that window could lose a change. A normal quit should not.

"Reconnect" below means the link dropped and came back. Disconnect and connect again from Your Cores also clears the
`MainScreen`, Tools and Radio state, as noted in section 3.

| Surface | Setting | Store (exact key or file) | Owner | Saved when | Restored when | Reopen | Reconnect | Core restart | Band change | Add slice/pan | Phone background | Verdict | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Connect | Paired Cores | Keychain `paired-cores` | phone | Pair, rename, each session | `ConnectionFlow.init` | sticks | sticks | sticks | n/a | n/a | sticks | sticks | Save `PairedStationStore.swift:33-42`; restore `ConnectionFlow.swift:743`, `:3550` |
| Connect | Device key | Keychain `device-key-secure-enclave` or `device-key` | phone | First use, Make a new key | `ConnectionFlow.init` | sticks | sticks | sticks | n/a | n/a | sticks | sticks | Save `SecureEnclaveKeyStore.swift:25`, `KeychainKeyStore.swift:24`; restore `ConnectionFlow.swift:740` |
| Connect | This phone's name | `phone.connect.deviceName` | phone | Pairing | `ConnectionFlow.init` | sticks | sticks | sticks | n/a | n/a | sticks | sticks | Save `PhoneSettings.swift:199-208`; restore `ConnectionFlow.swift:734` |
| Connect | Pair-again, rename-grey and new-key notes | `phone.connect.coresNeedingPairing`, `.coresWithoutRename`, `.madeNewKey` | phone | When found out | `ConnectionFlow.init` | sticks | sticks | sticks | n/a | n/a | sticks | sticks | Save `PhoneSettings.swift:212-230`; restore `ConnectionFlow.swift:738-739`, `:3564` |
| Connect | A Core's last address and path | Keychain `paired-cores` (`lastGood`, `lastPath`, `endpoints`) | phone | Each session | At dial | sticks | sticks | sticks | n/a | n/a | sticks | sticks | Save `PairedStation.swift:142-148`; restore `PairedStation.swift:176-183` |
| Connect | Which Core I was on, return to it | No store | memory | Never | Never | reverts | sticks | sticks | n/a | n/a | sticks | reverts | No save; `ConnectionFlow.swift:744`; reconnect path `ConnectionFlow.swift:1660-1667` |
| Connect | Order of Cores, which row's Connect is highlighted | Keychain list, first-saved order | phone | Pairing | `ConnectionFlow.init` | sticks | sticks | sticks | n/a | n/a | sticks | sticks | `PairedStationStore.swift:21`, `YourStationsScreen.swift:73` (never moves to last used, DC1) |
| Navigation | Selected tab | `@State selection` | memory | Never | Never | reverts | sticks | sticks | n/a | n/a | sticks | reverts | `RootView.swift:24`, `:39` |
| Navigation | Setup page open | `SetupRouter.path` | memory | Never | Never | reverts | sticks | sticks | n/a | n/a | sticks | reverts | `SetupRouter.swift:15`, `RootView.swift:27` |
| Navigation | Tools page open, Radio page open | `@State route` | memory | Never | Never | reverts | sticks | sticks | n/a | n/a | sticks | reverts | `ToolsTab.swift:55`, `RadioTab.swift:53` |
| Band | RX, TX, Sound panels open | `@State` | memory | Never | Never | reverts | sticks | sticks | n/a | n/a | sticks | reverts | `MainScreen.swift:90-92` |
| Band | Pan, Display, Slices sheets, band and column menus, Roundtable sheet | `@State` | memory | Never | Never | reverts | sticks | sticks | n/a | n/a | sticks | reverts | `MainScreen.swift:93`, `:99`, `:104`, `:122` |
| Band | Display pill's open tab | `@State displayTab` | memory | Never | Never | reverts | sticks | sticks | n/a | n/a | sticks | reverts | `MainScreen.swift:95` |
| Setup | Display page scope and chosen band | `@Published` | memory | Never | Never | reverts | sticks | sticks | n/a | n/a | sticks | reverts | `MainScreenModel.swift:37`, `:40`, `SetupRouter.swift:29` |
| Band | Zoom (receive span) | `rxViewSpanHz` in `phone.band.display.<pan>` | per pan, phone | Pinch end, double tap, zoom action | At launch, used for the first view | sticks | sticks | sticks | sticks | sticks | sticks | sticks | Save `BandModel.swift:1036-1041`, `BandGestureLayer.swift:741-742`, `:774`, `:786`, `MainScreenModel.swift:283-289`; restore `MainScreenModel.swift:245`, `BandSubscriber.swift:545` (one span for all bands) |
| Band | Keyed view span | `txViewSpanHz` in the same key | per pan, phone | Keyed zoom | First key | sticks | sticks | sticks | sticks | sticks | sticks | sticks | Save `BandModel.swift:1123-1127`; restore `BandSubscriber.swift:536` |
| Band | Pan (view center) | `requestedView` | memory | Never | Never | reverts | sticks | sticks | n/a | unknown | sticks | reverts | `BandModel.swift:498`, no clear of it found; `BandSubscriber.swift:544-555` |
| Band | Receiver window under CTUN | Core | Core | Each move | Snapshot | unknown | unknown | unknown | unknown | unknown | unknown | unknown | `NereusApp/Band/BandDrag.swift:197-198`; Core sweep owns it |
| Band | Waterfall history, look back | Memory | memory | Never | Never | reverts | unknown | unknown | unknown | unknown | sticks | reverts | `BandModel.swift:431`, `:1200-1212`, `SessionController.swift:399`; `startOver` on media reconnect not traced |
| Display | Picture rows: colors, trace, palette, levels, detector, averaging, overlays, peaks, grid, 3D, timestamps | `phone.display.master.v29`, `phone.band.display.<pan>` | phone, All pans plus per pan | At change | Launch; a pan when opened | sticks | sticks | sticks | sticks | sticks | sticks | sticks | Save `MainScreenModel.swift:589-611`; restore `MainScreenModel.swift:245`, `BandDisplaySettingsStore.swift:204-225` |
| Display | This device's rows: spectrum height (each orientation), Look back, CTUN, DUP, decimation | `phone.display.shared.v29` | phone | At change | Launch | sticks | sticks | sticks | sticks | sticks | sticks | sticks | `BandDisplaySettingsStore.swift:51-54`; save `DisplaySheetModel.swift:358-363`; restore `BandDisplaySettingsStore.swift:210` |
| Display | Shared Appearance colors, custom gradients, TX scale and waterfall | `phone.display.shared.v29` | phone | At change | Launch | sticks | sticks | sticks | sticks | sticks | sticks | sticks | `BandDisplaySettingsStore.swift:42-47`, `:210` |
| Display | Background picture | `backgroundImagePath` (absolute path), `DisplayPictures/<uuid>` | phone | Import | Launch | unknown | sticks | sticks | sticks | sticks | sticks | unknown | Save `DescribedPage.swift:473-480`; read `NereusKit/Sources/NereusBand/BandRenderer.swift:1275-1280`; clear bug 3 |
| Display | dB Max, dB Min per band | Core `DisplayGridMax_<band>`, `DisplayGridMin_<band>` (older Core: phone) | Core | At change | Each session | sticks | sticks | unknown | sticks | sticks | sticks | unknown | `CoreBandScale.swift:9-23` read as a comment, code not traced; Core sweep owns it |
| Display | FFT size, window, Hz per bin, frames per second | Core `DisplayFftSize`, `DisplayFftWindow`, `DisplayHzPerBinTarget`, `DisplaySpectrumFps` | Core | At change | Each session | unknown | unknown | unknown | unknown | unknown | unknown | unknown | `CoreDisplayModel.swift:100-104`; Core sweep owns it |
| Display | TX display FFT and detectors | Core `DisplayTx*` | Core | At change | Each session | unknown | unknown | unknown | unknown | unknown | unknown | unknown | `NereusApp/Setup/CoreTxDisplaySettings.swift:20-28`; Core sweep owns it |
| Meters | S-meter face, modes, peak hold, decay, unit, decimal | `phone.smeter.settings` | phone | At change | Launch | sticks | sticks | sticks | sticks | sticks | sticks | sticks | Save `SMeterSettingsStore.swift:57`, `SMeterModel.swift:144`; restore `SMeterSettingsStore.swift:25`, `SMeterModel.swift:56` |
| Meters | Keyed meter page and slots | `phone.keyedMeters.settings` | phone | At change | Launch | sticks | sticks | sticks | sticks | sticks | sticks | sticks | `KeyedMetersModel.swift:15`, `:34` |
| Setup | Filter Presets mode, PA temperature unit | `SetupFilterPresetsMode`, `SetupPaTempUnit` | phone | At change | Launch | sticks | sticks | sticks | n/a | n/a | sticks | sticks | Save `PhoneSetupKeys.swift:353`, `:360`; restore `:41-44` |
| Setup | Show PA Values page | `display/showPaValuesPage` | phone | At change | Launch | sticks | sticks | sticks | n/a | n/a | sticks | sticks | `PaValuesModel.swift:85`, `:323` |
| Setup | Touch: drag, tap, snap, pinch, double tap | `phone.touch.*` | phone | At change | Read at use | sticks | sticks | sticks | n/a | n/a | sticks | sticks | Save `PhoneSettings.swift:91-122`, `NereusApp/Setup/NavigationPage.swift:78-79`; read `NereusApp/Band/BandGestureLayer.swift:585`, `:698`, `:755` |
| Setup | Tuning dial and its haptics | `phone.dial.*` | phone | At change | Read at use | sticks | sticks | sticks | n/a | n/a | sticks | sticks | Save `PhoneSettings.swift:140-161`; read `BandGestureLayer.swift:149`, `:251`, `AppModel.swift:405-408` |
| Setup | Panadapter swipes | `phone.swipe.*` | phone | At change | Read at use | sticks | sticks | sticks | n/a | n/a | sticks | sticks | `PhoneSettings.swift:351-376` |
| Audio | Microphone, mute band while talking | `phone.audio.microphone`, `.muteBandWhileTalking` | phone | At change | Launch | sticks | sticks | sticks | n/a | n/a | sticks | sticks | Save `NereusApp/Setup/AudioOnThisPhonePage.swift:82-83`, `:92`; restore `AppModel.swift:428-430` |
| Audio | Audio quality | `phone.audio.quality` | phone | At change | Launch | sticks | sticks | sticks | n/a | n/a | sticks | sticks | `NereusApp/Audio/AudioQualityModel.swift:306`, `:321` |
| Audio | Play the band through (route) | `chosen` | memory | Never | Never | reverts | sticks | sticks | n/a | n/a | sticks | reverts | `AudioSessionController.swift:127`, `:337-339`; clear bug 2 |
| Audio | Mute this phone | `audioMuted` | memory | Never | Never | reverts | sticks | sticks | n/a | n/a | sticks | reverts | `AppModel.swift:49`, `:373-374`, `:448-454` |
| Sessions | Sound only, keep screen on, Low Power, hot phone | `phone.session.*` | phone | At change | Read at use | sticks | sticks | sticks | n/a | n/a | sticks | sticks | Save `NereusApp/Setup/BatteryAndSessionsPage.swift:22-23`, `:49-52`, `:67-68`; read `SessionController.swift:330-336`, `:418` |
| Sessions | Sleep timer choice | `phone.session.sleepTimer` | phone | At change | Launch and each connect | sticks | sticks | sticks | n/a | n/a | sticks | sticks | Save `PhoneSettings.swift:255-261`; restore `SessionController.swift:152`, `:257` |
| Data | Wi-Fi and cellular modes, 5 GB warning, first-cellular note | `phone.data.*` | phone | At change | Read at use | sticks | sticks | sticks | n/a | n/a | sticks | sticks | `PhoneSettings.swift:288-317`, `SessionController.swift:333` |
| Data | Month's data counters | `phone.data.month*` | phone | Each sample | `DataUseMeter.init` | sticks | sticks | sticks | n/a | n/a | sticks | sticks | Save `DataUseMeter.swift:140`, `:193-200`; restore `:75`, `:173` |
| Roundtable | Signal format, first-run note | `phone.roundtable.*` | phone | At change | Read at use | sticks | sticks | sticks | n/a | n/a | sticks | sticks | `PhoneSettings.swift:323-333`, `MainScreen.swift:166` |
| iPad | Roundtable applet position | `phone.applets.roundtablePosition` | phone | Grip moved | Column created | sticks | sticks | sticks | n/a | n/a | sticks | sticks | `PhoneSettings.swift:340-343`, `NereusApp/iPad/AppletColumn.swift:52` |
| Spots | Spots shown on the band, turn beam on tune | `phone.spots.display`, `phone.rotor.turnOnTune` | phone | At change | Launch | sticks | sticks | sticks | n/a | n/a | sticks | sticks | `SpotsModel.swift:302-303`, `:550`, `:820`, `:839` |
| FreeDV | Miles, kHz, band filter, follow | `phone.freedv.*` | phone | At change | Launch | sticks | sticks | sticks | n/a | n/a | sticks | sticks | `FreeDVReporterModel.swift:160-165`, `:421-445` |
| TX panel | AM Mod Monitor source, flash limits, style | `phone.modMonitor.*` | phone | At change | Launch | sticks | sticks | sticks | n/a | n/a | sticks | sticks | `ModMonitorModel.swift:153-156`, `:262-315` |
| Tools | Diversity memories | `phone.diversity.memory.<band>.<slot>` | phone, per band, not per radio | Store tap | Read at use | sticks | sticks | sticks | n/a | n/a | sticks | sticks | `DiversityModel.swift:536`, `:549` |
| iPad | Open pans, pan last touched | `@Published open`, `lastTouched` | memory | Never | Never | reverts | sticks | sticks | n/a | n/a | sticks | reverts | `PanStackModel.swift:70`, `:73`, only opener `:245`; a smaller radio may close them `:405-421`; clear bug 1 |
| iPad, sideways iPhone | Applet column shown, button column folded, folded pans, wide Roundtable | `@State` | memory | Never | Never | reverts | sticks | sticks | n/a | n/a | sticks | reverts | `MainScreen.swift:101`, `:106`, `:110`, `:129`, `:153` |
| Slices | Frequency, mode, filter, slice settings, pan key | Core, with a per-device saved layout | Core, per device | Core | Admission | sticks | sticks | unknown | n/a | n/a | sticks | sticks | `StationServer.cpp:12272-12301`, `:12475-12479`; Core restart belongs to the Core sweep |
| Slices | Which slice is active after a saved layout returns | Core `active` flag | Core | Core | Snapshot | unknown | sticks | unknown | n/a | n/a | sticks | unknown | `BandSlicesModel.swift:1336-1337`; the restore call carries no active field, `StationServer.cpp:12293` |
| Slices | Slice shown on this phone only, its return slice | `@Published` | memory | Never | Never | reverts | unknown | unknown | n/a | n/a | sticks | reverts | `BandSlicesModel.swift:152`, `:155`, `:1344-1345` |
| Slices | This phone's volume on a listened slice | `listenLevels` | memory | Never | Never | reverts | unknown | unknown | n/a | n/a | sticks | reverts | `BandSlicesModel.swift:161`; listening claims end with the device's absence, `StationServer.cpp:12459` |
| Transmit | MON and transmit settings | Core `transmit` object | Core | At change | Snapshot | unknown | unknown | unknown | unknown | unknown | unknown | unknown | `TransmitModel.swift:1523-1530`; Core sweep owns it |
| Setup | Hardware PTT buttons, Buttons key a locked phone | Nothing kept | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | by design | `NereusApp/Setup/PttButtonsPage.swift:8-11` says nothing is kept for them |
| Band | Held PTT | Memory | memory | n/a | n/a | n/a | n/a | n/a | n/a | n/a | reverts | by design | `NereusSDRApp.swift:240-241`, `TransmitModel.swift:937-941`; connecting never keys |
| Band | Band display while away | Unsubscribed | memory | n/a | On return | n/a | n/a | n/a | n/a | n/a | reverts | by design | `SessionPolicy.swift:12-14`, `BandSubscriber.swift:135-140`; a setting turns it off |
| Connect | Session after Disconnect | Nothing dials | memory | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | by design | `ConnectionFlow.swift:1705-1707`, `:1805-1806` |
| Sessions | Sleep countdown | Memory | memory | n/a | Starts at connect | reverts | sticks | sticks | n/a | n/a | sticks | by design | `SessionController.swift:213-223`, `:255-257` say a new intent starts its own deadline |


## Bugs and design calls as found

The tables cite these by number, as each sweep wrote them against `dfa9e1c29`. Their status now is under [Findings and fixes](#findings-and-fixes).


### Desktop: Setup dialog

#### Clear bugs

##### 1. Multimeter polling delay does nothing after a relaunch until the page is opened (certain)

Scenario, local window: Setup > Display > Multimeter, set polling delay to 500 ms, quit, relaunch. Meters update at 100 ms, while the saved value is still 500. Open that Setup page and they jump to 500. A reconnect does not help either; only opening the page does.

Cause: `MeterPoller` starts at 100 ms (src/gui/meters/MeterPoller.cpp:219). Its only setters are the page's load and change handlers (src/gui/setup/MultimeterPage.cpp:345, :383-386). `MultimeterPage::applyPersistedMeterInterval` (:263-275) has one caller, MainWindow.cpp:5064, inside the `SettingsProxy` block, so it runs only in a remote window. The comment at MainWindow.cpp:5061 says the poller "read it at startup", and the comment at MultimeterPage.cpp:270 says it applied only on page open; no local startup call exists. Lazy page building since #272/#301 is what makes this reachable.

Why a bug: the value is saved and displayed yet not applied, and the remote window path shows the intended behavior.

##### 2. Setup edits are not written to disk when made, so an unclean stop loses them (certain for the code path; likely for how often it shows)

Scenario, remote window: change a raw Station setting (for example `SwrProtectionLimit`, `BandPlanRegion`, `DspOptions*`, a Display FFT key), then the Core loses power or is killed without SIGTERM. After restart the setting is back. A phone or second window saw the new value in between. Same shape in a local window for pages that call `setValue` with no `save()`, on a crash.

Cause: `StationServer::applySettingsWrite` (src/core/session/StationServer.cpp:9527-9609) calls `applyInboundWrite` and live apply hooks, and has no save call. A grep of StationServer.cpp and core/session finds none for this path. `AppSettings::setValue` is memory only (AppSettings.cpp:1133-1149). The Core reaches disk only if something else schedules a save (`RadioModel::scheduleSettingsSave`, :29345, a slice edit) or at `DaemonApp::stop` (src/core/daemon/DaemonApp.cpp:663). Also `stop()` returns before that save when there is no radio model (DaemonApp.cpp:608-611), so an edit taken with no model is not saved even on clean SIGTERM. Whether a window can reach Setup in that state was not checked.

Why a bug: other Core-side setting writers do flush in the background (StationAccessoryData.cpp:141, StationRotorController.cpp:806, StationTciController.cpp:214, RadioModel.cpp:7486), so the raw path is the odd one out. A Pi is often just unplugged.

##### 3. Editing radio settings before a radio has connected is silently dropped (likely)

Scenario, local window with no radio: open Setup > Transmit > Power, Audio > Microphone, DSP > CW or Test > Two-Tone, or Hardware > Radio Info, and change a value. The control shows it. Connect: it is back to the saved value or the default. No message.

Cause, transmit side: `TransmitModel::persistOne` returns when `m_persistMac` is empty (src/models/TransmitModel.cpp:2288-2290), `m_persistMac` is first set by `loadFromSettings` (:2298), which then overwrites every field from the store (:2296-2849, called at RadioModel.cpp:20997). Hardware side: `HardwarePage::onTabSettingChanged` returns on empty MAC (src/gui/setup/HardwarePage.cpp:300), and `applySavedBoardTabs` knows the saved radio yet leaves the MAC empty on purpose (:405-408).

Marked likely because I did not read every page's enabled state with no radio. `SetupDialog` itself gates on remote and receive-only, not on connection state.

##### 4. `LineInGain` is saved and never loaded (likely, low exposure)

`TransmitModel::setLineInGain` persists `hardware/<mac>/tx/LineInGain` (src/models/TransmitModel.cpp:810, :2869), but `loadFromSettings` (:2296-2849) has no read, so it returns to 23 (TransmitModel.h:3125) each connect. A mic profile carries `Mic_LineInGain` (MicProfileManager.cpp:1338). I found no desktop Setup page calling this setter; Microphone uses `setLineInBoost`, which round-trips. The caller list was not exhausted, and the SD's "Line In Gain" row (SetupDescriptionService.h, version 24) was not traced to this property.

#### Design calls

1. **CLAUDE.md says TX power and antenna selection are never persisted; the code persists them per radio.** `hardware/<mac>/tunePowerByBand/*` (TransmitModel.cpp:2244-2266), `hardware/<mac>/alex/antenna/*` (SettingsScope.cpp:764-776), and step attenuator and preamp under `hardware/<mac>/options/` (SettingsScope.cpp:748-762). Comments cite Thetis for these. Recommendation: keep the behavior, since Thetis does it, and rewrite the CLAUDE.md rule to say what is actually meant, so future audits stop reading it as a bug.

2. **`DisplaySpectrumFps` means two things on one key.** It sets the Core's FFT rate and the window's own paint rate, so two windows cannot paint at different rates (SettingsScope.cpp:67-88, already marked unresolved). A window setting its rate changes it for everyone. Recommendation: a window-local `DisplayPaintFps`, as that comment suggests.

3. **AGC values in Setup > DSP > AGC follow the band while NR and NB tuning follow the slice.** Change AGC slope on 20 m, go to 40 m, and the page shows 40 m's value. To a user that can look like a revert, and the page does not say it is per band. `AgcSlope` uses the band prefix (SliceModel.cpp:3524), `Nr2TrainT1` and `MnrAlpha` the slice prefix (:3591, :3618). Recommendation: keep the storage, and add a small note on that page naming the band being edited.

4. **Hidden unbuilt controls have saves with no restore.** OC extras write with no reader at all and never restore to their own controls (OcOutputsHfTab.cpp:346-405, :481), so they will revert the day `OcExtras` is built. Recommendation: nothing now; add a restore as part of building that feature. Audio > Advanced and Multimeter holds do restore to their controls.

5. **Controls that save and restore but change nothing.** `Antenna_ConflictPolicy` (visible, and stored per window on a Core page) and `MultimeterSignalHistoryEnabled` (visible). The project's rule for an unbuilt feature is to hide it (UnbuiltFeatureList.h). Recommendation: hide both until they do something, or wire them. Your call, since it changes what is shown.

6. **Setup writes a Display value and clears every pan's own override, by rule** (PanDisplayKeys.h). A user with a pan-specific color who changes the Setup default loses the pan's color at once, including pans not open. That is the approved rule, noted here because it reads as a revert. Recommendation: keep it.

7. **Flush policy for Setup in a local window.** Most pages rely on close or the next slice save; Hardware, Audio Outputs and VAX save at once. A single timed flush on any `AppSettings` write would close bug 2 for both local and Core at one site. Recommendation: do it there, in the write observer path (AppSettings.cpp:1288), debounced, rather than page by page.


### Desktop: main window, display and layout

#### Clear bugs

Most often hit first.

1. **The panel hide is forgotten. Certain.** Scenario: click the menu icon in the bottom bar to hide the container panel, quit, reopen. The panel is back. Cause: `EdgeDockArea::setUserCollapsed` only sets `m_collapsed` (src/gui/containers/EdgeDockStrip.cpp:679-682), the legacy look only calls `rightPane->hide()` (src/gui/MainWindow.cpp:17956-17960), and neither reaches `captureArrangement` or a key. Why a bug: layout is client-authoritative and persisted under CLAUDE.md "Settings", and the rest of the arrangement is saved.

2. **A popped-out pan comes back docked. Certain.** Scenario: float a pan onto a second monitor, quit, reopen. The pan is in the main window and its window is gone. Cause: `FloatingPan_<id>_Geometry` is saved (src/gui/PanFloatingWindow.cpp:155-162), but nothing records that the pan was floating. `floatPanadapter` has three callers: the grown layout, the pan's menu and the View menu (src/gui/MainWindow.cpp:2731, 8435, 12893), none at startup. src/gui/PanadapterStack.cpp:656 notes "Nereus has no canvas/restore marker". Why a bug: the geometry is kept with care for this case (src/gui/PanadapterStack.cpp:704-708 calls quitting with a pan floating "the ordinary way to end a session"), and applets and floating containers both reopen in their windows.

3. **The zoom slider does not show the span. Certain.** Scenario: zoom to 48 kHz, quit, reopen. The span is right, but the slider under the pans sits at full right. The slider runs 1 to 768 kHz and sets the span directly, so the first touch jumps the span to wherever the thumb lands, far from the 48 kHz in use. It also does not follow the wheel or the scale drag during a session, and it stays put when the active pan changes. Cause: `zoomBar` is a local set to 768 once, with one outgoing connect and nothing coming back (src/gui/MainWindow.cpp:9359-9376). Why a bug: the stored value and the control disagree, and CLAUDE.md's GUI to model sync rule expects the control to follow the model.

4. **FFT decimation is lost at every launch. Certain.** Scenario: set Decimation in the display pill or Setup, quit, reopen. It is 1 again. Cause: `applyDisplayDecimation` only calls `m_fftEnginePool->setDecimation` and writes no key (src/gui/MainWindow.cpp:7507-7526), `refreshFftPoolConfig` does not read one (6310-6355), and the code says so (7550 "Decimation has no key"). The other four FFT knobs beside it are saved and Station scoped, so this one needs a key added to `SettingsScope` as well. Why a bug: DSP and display settings are client-authoritative and persisted, and nothing marks this one as meant to reset.

5. **The overlay strip's collapse is lost. Certain.** Scenario: collapse the strip on a pan with its arrow, quit, reopen. It is expanded. Cause: `SpectrumOverlayPanel::toggle` flips `m_expanded` and emits `collapsed`, with no key and no listener found in src/gui/*.cpp (src/gui/SpectrumOverlayPanel.cpp:845-853, src/gui/SpectrumOverlayPanel.h:393). Why a bug: same rule as bug 1. Its sibling, the pill's last tab, is saved.

6. **Inner pan dividers are lost. Certain.** Scenario: use a 2x2, 12h, 2h1 or 3h2 layout, drag a divider inside a row, quit, reopen. Only the top-to-bottom split returns. Cause: only `m_rootSplitter->sizes()` is saved (src/gui/PanadapterStack.cpp:758-768), and the comment at 753-757 says the nested ones were left for later polish. Why a bug: the key names were planned and the code admits the gap.

7. **Saved spot settings do not reach any pan but the first. Certain for the read, likely for what the user sees.** Scenario: turn Spots off in Spot Hub, use a two-pan layout, quit, reopen, click the second pan so it becomes active. Spots draw there. Cause: `loadSpotDisplaySettings` runs at startup for `activeSpectrumWidget()` only (src/gui/MainWindow.cpp:2233-2237), the Spot Hub's settings change also reaches only that pan (19937-19941, 19953), new pans never call it (wireSpectrumForPan at 7193 does not), and `m_showSpots` defaults to true (src/gui/SpectrumWidget.h:4580). Only Spot Hub's own switch reaches every pan (src/gui/containers/ContainerButtonDispatcher.cpp:595-601). Font, level and color settings have the same gap. Why a bug: the keys are global and the switch path already treats them as every pan's.

8. **Close-only values are lost on a kill, crash or signal quit. Likely.** Scenario: change the pan layout, drag the pan divider or move the main window, then the app crashes, or it is ended with SIGTERM. Old layout on the next launch. Cause: `PanLayoutId`, `PanSplitter0Sizes` and floating geometry are handed to AppSettings only in `closeEvent` and the session-end save (src/gui/MainWindow.cpp:21399-21405, 2371-2374). The `aboutToQuit` handler calls itself the "Same flush as closeEvent" but does not call `saveSplitterState`, `saveFloatingGeometry` or any `SpectrumWidget::saveSettings` (2320-2347), so a pending 500 ms display save is also dropped there. main.cpp turns SIGTERM and SIGINT into a queued quit (src/main.cpp:161-176). Why likely and not certain: the comment at 2316 says closeEvent does not run on a signal, but whether Qt's quit closes windows first is outside this tree and I did not test it. The missing calls in that handler are certain. A crash loses these either way, since the other saves in a session write only what has already reached AppSettings.

#### Design calls

1. **The open tab on a VFO flag.** It resets to none at every launch and for each new flag (src/gui/widgets/VfoWidget.cpp:1020), while the flag's fold is saved per receiver. Recommendation: leave it. A flag that opens with a tab showing covers the spectrum, and nothing suggests anyone wants that back. If the maintainer does want it, store it beside `ReceiverFlag_<n>_Collapsed`.

2. **A saved clamp replaces the operator's zoom.** When a lower sample rate or a removed wideband source narrows the span, the clamp is what gets saved (src/gui/MainWindow.cpp:6510, 10477; src/gui/SpectrumWidget.cpp:10048-10058). Going back to the higher rate does not bring the old span back. Recommendation: keep it. It is simple and visible, and the comments only defend staying inside the ceiling. Storing a wanted span apart from the shown one is the fix if it annoys.

3. **One layout and look for every radio and Core.** `PanLayoutId`, per-pan display keys and the workspace document have no radio or Core in the key. A restored layout is not checked against the radio's limit at startup (src/gui/MainWindow.cpp:9221-9230 against 20456); on a Core it leaves empty pans, because restoring never creates station slices (20379-20381). Recommendation: keep one set, since operators expect their window to look the same, and treat a saved layout larger than the radio's limit as its own small fix.

4. **Remote paint rate.** `DisplaySpectrumFps` is both the Core's production rate and this window's paint rate, a straddle `SettingsScope.cpp` records as accepted and unresolved. I did not establish what the window paints at before the proxy's first snapshot, so that case is unknown. Recommendation: the separate local key that file suggests, if a remote operator ever wants their own rate.

5. **Closed pans keep their look.** Per-pan overrides stay after a pan closes, so reopening `pan-1` brings back the old look (`writeDefault` in src/core/settings/PanDisplayKeys.cpp treats closed pans as still holding keys; only reset clears them). Recommendation: keep it, as the operator's own values are not thrown away.

6. **What a workspace profile holds.** Containers and screens only. Pan layout, window size and display look are not in it (src/gui/containers/WorkspaceProfiles.h). The live arrangement goes back into the active profile only on Save or on switching away (src/gui/containers/WorkspaceProfiles.cpp:481-535), though restart keeps it through the live document. Recommendation: add pan layout to profiles if the maintainer sees them as whole-window setups, otherwise label them as container arrangements.

7. **Things that can look like a revert but are deliberate.** A corrupt workspace, or one written by a newer build, leaves the whole layout read-only with a notice, and every change is refused (src/gui/containers/ContainerWorkspaceStore.cpp load and commit). Someone moving between builds could meet this. And setting `NEREUS_WORKSPACE_PREVIEW=0` switches to a separate document (`ContainerWorkspace` instead of `ContainerWorkspacePreview`, same file lines 30-33), so a layout seems to change. Recommendation: keep both, and check the log line "Container workspace remains read-only" first when a layout will not stick.


### Receiver and transmit state (desktop and Core)

#### Clear bugs

##### 1. Lock on at quit: a desktop comes back on 14.225 MHz. Certain.

Scenario: on a desktop connected straight to the radio, tune Slice A to 7.074 MHz LSB, turn Lock on, quit, relaunch and connect. The VFO shows 14.225 MHz in LSB, locked, with the pan on 14.225.

Cause: the starting slice is made with `addSlice()` (`RadioModel.cpp:19795`). With a saved letter that runs `restoreFromSettings(band, false)` (`RadioModel.cpp:17167`), which sets `m_locked` true (`SliceModel.cpp:4093-4094`) and skips the frequency. Next, `loadSliceState` (`RadioModel.cpp:21104`, `26830`) calls `restoreFromSettings(band)`, whose frequency restore uses `setFrequency` (`SliceModel.cpp:3727-3728`). That setter returns at once when locked (`SliceModel.cpp:460`). The constructor frequency stays (`SliceModel.h:1924`), while mode, filter and AGC come from the 40 m slot. `RadioModel.cpp:26848` then centers the pan there. The next save writes 14.225 and LSB into the slice's 20 m slot and sets `LastBand` to 20 m, so the 20 m memory is damaged too.

Why a bug: nothing could want this. The Core path and the seed path already guard against the same trap, per their comments (`SliceModel.cpp:3447-3449`, `RadioModel.cpp:17165-17166`). The guard arrived with commit 7515380f3 on 2026-10-29, which also put this restore ahead of `loadSliceState`.

Fix belongs in `restoreFromSettings` at `SliceModel.cpp:3727`, not at the other session's lines: a restore should assign the saved frequency past the lock, as `restoreReceiveState` does.

##### 2. A Core whose layout is "kept" stops saving every receiver setting, for as long as it runs. Certain as code; how often it is hit depends on the station.

Scenario: a Core restarts and one saved receiver cannot start, for example the radio has too few free receivers, or the saved RADE audio owner cannot start its decoder. The connection panel says "Your saved layout is kept." You then retune, change mode, AGC, volume, anything, on the receivers that did start. They work. Restart the Core and every one of those changes is gone. If the same refusal happens at each start, the Core can never save receiver settings again.

Cause: `m_receiveLayoutProtected` is set at `RadioModel.cpp:13866`, `14019`, `22562`, `28937`, `29032` and `29060` and is never cleared; it defaults false at `RadioModel.h:8990`. `scheduleSettingsSave` returns on it (`RadioModel.cpp:29350`), as do `flushPendingSettingsSave` (`29440`), `materializePendingBandSettings` (`29589`) and `saveSliceState` (`29622`). Disconnect and stop go through the same gate, so the flush at `29890` does nothing. A Core with a saved layout that fails to parse is protected from the start (`28937`). TX keys and Alex are unaffected, as they save outside this gate (`29913-29927`).

Why a bug: the comments give the purpose as protecting the per-radio layout record (`RadioModel.cpp:13864`, `14014-14015`). `Slice<N>/...` keys are not that record. The status sentence says only that the layout is kept and nothing says edits are being dropped.

Recommended fix: hold back only `captureReceiveLayout` while protected, and keep writing the slice keys. The maintainer should confirm one point: a slice that was refused keeps its keys untouched, since it is not in the live list, so this looks safe, but it was not tested.

##### 3. A reused letter brings another band's AGC, NB and step onto a new slice, then saves them over this band's memory. Certain. Owned by the session at `RadioModel.cpp:17167`; no change proposed here.

Scenario: Slice B last ran on 40 m with AGC Slow and a 1 kHz step, and was closed. Later, on 20 m, press +RX. New B opens on 20 m (correct) with AGC Slow and 1 kHz, taken from 40 m. B's own 20 m values are never read. Half a second later they are overwritten.

Cause: `restoreFromSettings(*savedLetterBand, false)` at `RadioModel.cpp:17167` reads per-band keys from the letter's `LastBand`, after the frequency seed moved the slice to another band. The save then captures under the slice's current band (`RadioModel.cpp:29366-29367`, `SliceModel.cpp:3515-3543`).

Related, same block: the seed's `setDspMode` at `RadioModel.cpp:17161` is a no-change call when the seed is USB, because a new slice is already USB (`SliceModel.h:1960`, `SliceModel.cpp:712`). It takes the "keep cutoffs" branch, so the slice keeps 100 to 3000 even where the letter has a remembered USB filter for that band. Any other seed mode reads the remembered pair. The next save then writes 100 to 3000 over the remembered one.

##### 4. A custom TX filter is lost after any trip through RADE. Certain.

Scenario: set the TX filter to 200 to 2800 in USB. Switch the transmit slice to RADE, then back to USB. TX filter reads 100 to 3900. Your 200 to 2800 is gone, on disk too.

Cause: entering RADE writes 650 and 2350 through the persisting setters (`RadioModel.cpp:25672-25674`, `TransmitModel.cpp:4534-4556`). Nothing keeps the earlier value. Leaving RADE sees 650 and 2350 and writes the fixed 100 and 3900 (`RadioModel.cpp:25675-25680`). The comment there says "preserve custom voice bandwidths", which only covers a filter edited while in RADE.

Why a bug: the stated intent is to preserve the voice bandwidth. Also 100 to 3900 is not even the load default, which is 100 to 2900 (`TransmitModel.cpp:2689-2692`). This is NereusSDR-original code, so no Thetis behavior is at stake.

##### 5. Diversity phase, gain and fine null can be lost at quit. Certain.

Scenario: open Diversity, tune phase and gain to a null, touch nothing else, quit. They come back at their old values.

Cause: the values are saved as part of `captureBandSettings` (`SliceModel.cpp:3561-3564`), but no signal schedules a save for them. `blendChanged` only reconfigures and publishes (`RadioModel.cpp:38451-38459`). My signal comparison over `wireSliceSignals` found no save connected to `diversityPhaseDegChanged`, `diversityGainDbChanged` or `diversityFineNullEnabledChanged`. With nothing scheduled, the teardown flush does nothing (`RadioModel.cpp:29440`). Any other slice edit or a band change saves them by luck.

Why a bug: the same class of gap was fixed for step, lock and RIT, per the comment at `RadioModel.cpp:26598-26606`.

##### 6. TX edits on a Core can stay memory-only until something else saves. Likely.

Scenario: from a window, change mic gain or EQ on the Core and touch no receiver control. A clean Core stop keeps it (`DaemonApp.cpp:663`). A power cut or a kill does not.

Cause: TransmitModel setters write memory only (`TransmitModel.cpp:2286-2294`, `2253-2258`). The comment at `SessionCommandDispatcher.cpp:1555-1557` says exactly this and asks for a flush for two-tone frequencies only. I did not read all the other `requestSettingsSave` sites (`SessionCommandDispatcher.cpp:4450`, `4493`, `4593`, `StationServer.cpp:9240`, `12304`, `12378`, `12425`), so another path may cover some properties. That is why this is "likely". Same on a desktop for a crash, where close is the only flush if no slice control moves.

##### 7. TX edits made before the first connect of a run are thrown away. Likely.

Scenario: launch, change mic gain or EQ in a TX control with no radio connected, then connect. The stored value replaces yours.

Cause: `persistOne` does nothing while `m_persistMac` is empty (`TransmitModel.cpp:2288`), and `loadFromSettings` overwrites all fields at connect (`TransmitModel.cpp:2298` onward). Mic source alone got a fix (`tx/preconnect/Mic_Source`, `2461-2482`, `3659-3674`), so the problem is known for one control.

"Likely" because I did not check which TX controls are live while disconnected. If they are all disabled offline, this shrinks to mic source and is not a bug.

#### Design calls

##### 1. What a new slice should inherit (JJ's named complaint)

Today a new slice copies frequency, mode and noise reduction from the slice you are on and gets the rest from defaults or from what its letter last held, as the table above shows. So the same +RX press gives different results depending on the letter's history, which reads as "settings reverting". A letter's memory also opens muted or locked.

Recommendation: a new receiver is a copy of the slice you are on: filter, AGC, NB, NR, squelch, step and AF gain. RIT and XIT stay off by the existing ruling, and lock and mute start off. A letter's saved state is used only by a true restore (startup, a device returning). That removes Clear bug 3 at its root. The code is in the block the other session owns, so this is a decision to hand them, not a change from this sweep.

##### 2. Should a desktop bring back extra slices at restart?

A Core restores every slice from its layout. A desktop with no Core restores one slice plus one per empty pan, so a second slice on a pan is gone after relaunch (`RadioModel.cpp:29097-29100`), and so is TX if it was on that slice. The commit message for 3813bc07f treats operator-made slices as fresh, so this may have been deliberate at the time.

Recommendation: restore them the same way as the Core, using the same layout record per radio. One behavior everywhere is easier to explain, and JJ's complaint is about restored slices.

##### 3. Tune step per band

Step is stored per slice and band (`SliceModel.cpp:3543`), so it changes when you change band. Thetis keeps one saved wheel step (`console.cs:3205` `wheel_tune_index`) with a by-mode table (`console.cs:1988`), not per band.

Recommendation: make it per slice, no band part, unless JJ likes per-band step on purpose. Band stacking may touch this, so check with that session.

##### 4. CLAUDE.md says antennas and TX power are never persisted, and the code persists both

The Settings section lists them as radio-authoritative, never persisted. Antennas are stored per band and per radio (`AlexController.cpp:613-615`), with comments citing Thetis and a bench bug that asked for it (`RadioModel.cpp:2678-2684`). TX power is stored per band, citing Thetis `power_by_band` (`TransmitModel.cpp:1484-1505`).

Recommendation: keep the code, correct CLAUDE.md. As written, the rule invites someone to "fix" working persistence. ADC attenuation and preamp were outside this area, so another sweep should check them against the same sentence.

##### 5. PureSignal armed after restart

Armed state is not saved, on purpose (`TransmitModel.cpp:2350-2365`). The comment says a profile recall flips it, but connect never recalls the profile: it only sets the scope and reads the name (`RadioModel.cpp:8050-8056`). So a profile saved with PureSignal on still starts off, while its name is shown as active.

Recommendation: keep it off at start, which fits "connecting never keys", and correct the comment so nobody reads it as a promise.

##### 6. CTUN off-center position

Zoom and the CTUN preference are saved. The pan center is not, and restart puts the VFO back at center (`RadioModel.cpp:26848`). Someone parked off-center sees the pan move.

Recommendation: leave it. The comment at `SpectrumWidget.cpp:1491-1493` treats this as settled and it is low cost to the user. I did not check what Thetis does here, so JJ should say if parity matters.


### Core, stations and connect lists

#### Clear bugs

##### 1. A device that leaves while the Core's radio is off loses its slices (certain)

What the user does: turns the radio off, or the radio drops. Then closes the window, or leaves the phone away for 3 minutes. Later turns the radio on and connects.

What they see: Slices B and C are gone. Slice A is a new one with default settings, and every band's memory for those letters is gone.

Cause: `closeSliceFor` reads `currentRadioMac()`, which is empty unless the state is Connected (RadioModel.cpp:10232-10234). With no MAC it skips the copy and the save (StationServer.cpp:12364-12369) and still closes the slice and deletes its keys (StationServer.cpp:12377). `releaseDeviceClaims` calls it with no check (StationServer.cpp:12475-12479).

Why it is a bug: the sibling `saveTakenSlicesFor` guards this exact case. It says "With no radio connected there is no layout store to save in" and keeps the slices (StationServer.cpp:12406-12413). The documented rule is that a device's slices are saved for its return (DeviceLayoutStore.h:7-19).

##### 2. A setting changed from a window or phone is not written to the Core's disk (certain)

What the user does: changes a Setup value on a Core, such as FFT size, SWR protection or a DX cluster host, and does not tune anything. The Core then crashes or loses power.

What they see: the old value after the restart. A clean `systemctl restart` keeps it.

Cause: `applySettingsWrite` and `applySettingsRemove` change the store and apply the value but never ask for a save (StationServer.cpp:9526-9610, 9725-9727; SettingsProxyServer.cpp:342). There is no timer on the Core that saves regardless.

Why it is a bug: property writes ask for a save in the same file (StationServer.cpp:9232-9241), and `applyRemoteFreedvSetting` saves on its own (RadioModel.cpp:7486). The operator is told the value was accepted while it exists only in memory.

##### 3. Connect turns a radio's auto-connect tick back on (certain)

What the user does: unticks Auto-connect for a radio, then connects to it by hand.

What they see: next launch it connects by itself. With two radios used this way, a toast warns that several are marked.

Cause: both connect paths pass a fixed `true` to `saveRadio` (ConnectionPanel.cpp:1216, GuiConnectionController.cpp:948). They keep `pinToMac` from the saved entry but not `autoConnect`. `tryAutoReconnect` reads the flag (MainWindow.cpp:21015).

Why it is a bug: the checkbox writes the value (ConnectionPanel.cpp:519-523) and the editor dialog writes it (ConnectionPanel.cpp:1328), so the choice is offered and then thrown away. The comment at ConnectionPanel.cpp:1205 shows the overwrite was deliberate in Phase 3I, before those controls existed. See Design calls 1.

##### 4. A device signing in to a headless Core with the radio off does not get its saved slices (certain for the restore; likely for the default slice)

What the user does: closes the window with the radio on, so its slices are saved. Connects again while the Core's radio is off, then turns the radio on.

What they see: the saved slices do not come back. They reappear only at some later sign-in, beside whatever exists by then.

Cause: the restore needs a MAC (StationServer.cpp:12276-12278). The hold and retry for this case runs only when `hostsStationDevice()` is true (StationServer.cpp:12259-12264, 4198), and only a desktop-hosted Core sets that (StationHost.cpp:164, 177). DaemonApp never passes a hosting device. Step 4 then tries a default slice (StationServer.cpp:12319-12326). I did not read whether `addSlice` succeeds with the radio off.

Why it is a bug: the JJ 2026-10-23 comment fixed this for a Core inside a desktop window, with no reason given for leaving nereusd out.

##### 5. A protected layout stops every slice save for the rest of the Core's run (certain mechanism; the layout protection itself is deliberate)

What the user does: starts a Core whose saved layout could not be fully admitted, for example a receiver refused or the layout invalid. Tunes and changes slice settings for hours, then restarts the Core.

What they see: the old frequencies, modes and settings. The status line says only "Your saved layout is kept." (RadioModel.cpp:13683-13686).

Cause: `m_receiveLayoutProtected` is only ever set true (RadioModel.cpp:13866, 14019, 22562, 28937, 29032, 29060). It makes `scheduleSettingsSave`, `flushPendingSettingsSave`, `materializePendingBandSettings` and `saveSliceState` return early (RadioModel.cpp:29350, 29440, 29589, 29622). `saveSliceState` returns before the antenna and tune power flushes too (RadioModel.cpp:29634, 29643).

Why it is a bug: the stated purpose is that a temporary limit must not erase the layout record (RadioModel.cpp:13864). That needs only `captureReceiveLayout` held back, not each slice's own keys. The latch itself is intentional. It is listed as a bug because it reaches further than its stated purpose and the operator is not told.

##### 6. Transmit settings changed before the radio first connects are lost (likely)

What the user does: starts a Core with the radio off, changes a transmit setting from a window, then turns the radio on.

What they see: the setting returns to its saved value.

Cause: `persistOne` does nothing while `m_persistMac` is empty (TransmitModel.cpp:2286-2290). It is first set by `loadFromSettings(mac)` at connect, which loads the saved values (TransmitModel.cpp:2296-2298, RadioModel.cpp:20997).

Why "likely": I did not confirm that the Core accepts a transmit write with no radio connected.

#### Design calls

1. **Should Connect imply auto-connect?** Today it does, for every radio ever connected. Recommendation: a first connect may set it, but after that Connect keeps the saved value, as it already does for `pinToMac`. This is the fix for bug 3 if you agree.

2. **Direct use and Core use keep separate memories.** The same window has one set of Station values in its own file for direct connect and reads the Core's when on a Core (AppSettings.cpp:1123-1125). Moving between them looks like settings reverting. Recommendation: keep them separate, because a Core is shared and the two setups can differ. Say so in the UI or the docs, since this may explain some of the reports.

3. **The CLAUDE.md rule and the code disagree on ATT, preamp and antennas.** The rule says radio-authoritative and never persisted. The code persists them per MAC, on purpose: `hardware/<mac>/options/{stepAtt,autoAtt,preamp}/...` and `hardware/<mac>/alex/antenna/...` (SettingsScope.cpp:748-776, RadioModel.cpp:2681 area, with a comment citing a KG4VCF bench report about ANT2 on 20m). Recommendation: update the rule to match, and audit against the code.

4. **Mirrored edits made with the link down.** Setup values get a toast when dropped (W10). Applet and slice properties revert with no word (W11). Recommendation: the same toast for both, or disable the controls while the link is down. Your call since it changes what the user sees.

5. **A window on a Core has no timed save of its own file.** The 500 ms save returns at once for a remote model (RadioModel.cpp:29347), so window keys wait for a close or for SpectrumWidget's own save. A force-quit or crash loses the others. Recommendation: a timed save in the remote role too, using `addWriteObserver` for keys that stay local.

6. **`DisplaySpectrumFps` means two things.** It sets the Core's FFT rate and each window's paint rate, so one device's change moves all of them. The code calls this unresolved (SettingsScope.cpp:85-88). Recommendation: a separate local key for the paint rate, as that comment suggests.

7. **Editing `radio_mac` in nereusd.conf does nothing once a radio has been chosen.** The saved `StationRadioChoice` wins (DaemonApp.cpp:288-292), with no log line, unlike the sample rate and audio device (DaemonApp.cpp:975-979, 1008-1011). Recommendation: keep the rule and add the log line.

8. **The settings snapshot merges and never removes.** A key removed on the Core while this window was away stays in its cache until the window is replaced (SettingsProxy.cpp:175-219). Recommendation: leave it unless it shows up in practice. A live removal is handled (SettingsProxy.cpp:306-319), so only the away case is affected.


### iPhone and iPad

#### Clear bugs

##### 1. iPad open pans are gone after a relaunch (certain)

- **Scenario:** on an iPad, open Pan 2 from the Pan sheet. Force quit, or install a new build, then open and connect.
  Only Pan 1 shows. The slice that was on Pan 2 is back from the Core with its pan key, but is drawn on Pan 1's band.
- **Cause:** `open` starts as `[.first]` and is never saved (`NereusApp/Main/PanStackModel.swift:70`). The only caller
  of `openBand` is `addPan` (`PanStackModel.swift:231`, `:245`). The Core gives the slice back on its pan
  (`src/core/session/StationServer.cpp:12293`), and with Pan 1 alone every slice is drawn there
  (`PanStackModel.swift:51-52`).
- **Why a bug:** CLAUDE.md lists layout as client-authoritative and persisted. The pan's own display values are
  deliberately kept for when it opens again (`NereusKit/Sources/NereusBand/BandDisplaySettingsStore.swift:327-346`),
  so the pan's look survives while the pan does not. The phone and the Core also disagree about the layout.
- **Note:** opening the pans a device's slices already sit on, once the snapshot is whole, would need no new key. How
  to do it is a design question for the fix.

##### 2. Play the band through is not kept, though Setup marks it This phone (certain)

- **Scenario:** Setup, Audio, On this phone, choose a route. Quit and reopen, then connect. The choice is back to
  whatever iOS picks.
- **Cause:** the choice is only `@Published private(set) var chosen` (`NereusApp/Audio/AudioSessionController.swift:127`),
  set by `select` (`:337-339`) from Setup and the Sound panel (`NereusApp/Setup/AudioOnThisPhonePage.swift:77`,
  `NereusApp/Audio/SoundPanel.swift:89`). `AppModel` restores the microphone and the while-transmitting choices at
  launch, but not the route (`NereusApp/App/AppModel.swift:428-430`).
- **Why a bug:** the page carries the This phone tag (`AudioOnThisPhonePage.swift:162`), and `PhoneSettings` says
  settings marked that way are kept on the phone (`NereusApp/App/PhoneSettings.swift:38-39`). Its two neighbors on the
  same page stick.
- **Care in the fix:** a saved Bluetooth or wired route may not be connected at launch, so it needs a fallback.

##### 3. A background picture can vanish after an app update or a new install (likely)

- **Scenario:** import a background picture, then install a newer build. The band may draw no picture, while the
  setting still reads as having one.
- **Cause:** the import saves an absolute path (`NereusApp/Setup/DescribedPage.swift:480`), and the renderer opens
  exactly that path (`NereusKit/Sources/NereusBand/BandRenderer.swift:1275-1280`). A failed load draws nothing, with
  no message (`BandRenderer.swift:1283`).
- **Why only likely:** the code above I read. That iOS changes the app's container path across an update is platform
  behavior I did not test here. It needs one check on a real device. Saving the name relative to Application Support
  would fix it.

#### Design calls

##### DC1. Connect to the last Core on open

This is the biggest part of JJ's complaint.

- **Today:** Your Cores, then a tap (`NereusApp/Connect/ConnectionFlow.swift:744`).
- **Recommendation:** yes, as a Setup switch that is on by default. Save the last Core's identity key when a session
  reaches ready. Clear it on Disconnect and on Remove Core. At launch, when the key is present and that Core does not
  need pairing, dial it and show the connecting state with Cancel rather than the list. An explicit Disconnect still
  opens on Your Cores, which keeps the rule that nothing reconnects by itself after Disconnect
  (`ConnectionFlow.swift:1705-1707`).
- **Also:** connecting never keys, so the known ruling holds. Put the last used Core first or highlight it, since
  today the highlighted Connect is simply the Core paired first (`YourStationsScreen.swift:73`,
  `NereusKit/Sources/NereusLink/PairedStationStore.swift:21`).
- **Sub-question:** after the sleep timer ends a session, should the next open reconnect? I recommend yes. The timer
  ended it, not the operator, and opening the app says they want it back.

##### DC2. Return to the last tab

- **Recommendation:** yes. Save the tab in `PhoneSettings`, for example `phone.nav.tab`, and read it in `RootView`.
- **Why not `@SceneStorage`:** as I understand it, iOS discards scene storage after a force quit, and a force quit is
  one of the main cases here.

##### DC3. Return to the last Setup, Tools and Radio page

- **Recommendation:** yes for Setup and Tools, using the tab's approach, and fall back to the list when a page is
  gone, because a described page may not exist on another Core. `SetupTree.Route` and `ToolsTab.Page` would need to be
  saved in a stable form. Do Radio last.
- **Exception:** a Roundtable room falls back to Roundtable's list. Its title and room are live.

##### DC4. Panels, sheets and iPad column state

- **Recommendation:** split by how transient each is.
- Restore: the Display pill's last tab, iPad applet column shown or hidden, and folded button columns. These are
  layout, and CLAUDE.md lists layout as persisted.
- Do not restore: RX, TX and Sound panels, Pan, Display and Slices sheets, menus, wide Roundtable. On an iPhone they
  cover the band, so opening on one reads as a surprise, not a return. JJ may see the RX and TX panels differently,
  which is why I flag it.

##### DC5. Mute this phone

- **Today:** a mute carries across connections but resets at launch (`NereusApp/App/AppModel.swift:49`, `:373-374`).
- **Recommendation:** keep it, as `phone.audio.muted`. Someone who muted in a quiet room should not get sound on
  reopening, and the toolbar's speaker already shows muted (`NereusApp/Main/Toolbar.swift:218`).
- **The other side:** a silent app could look broken on a later day. The toolbar is the answer to that.

##### DC6. The band's pan position

- **Today:** zoom sticks, while the center returns to the slice (`NereusApp/Main/BandSubscriber.swift:544-545`).
- **Recommendation:** keep it this way at first, and do it last if at all. Under CTUN, a saved offset from the slice
  could be restored, but it may not fit the receiver's window after the Core has moved, and centering on the slice is
  a sane place to start. After the bigger items land, ask JJ if he still notices it.
- **Related:** there is one zoom per pan across all bands (`NereusKit/Sources/NereusBand/BandDisplaySettings.swift:370`),
  not one per band. That sits close to band stacking, so I record it and do not design it.

##### DC7. Redial at once on return to the foreground

- **Today:** a dead link waits for its timer, up to 60 s, with LINK LOST showing (`NereusKit/Sources/NereusLink/ReconnectPolicy.swift:8`).
  `AppModel.retryNow()` exists (`NereusApp/App/AppModel.swift:1037-1042`), but nothing calls it for a scene change.
- **Recommendation:** yes, call it when the scene becomes active while the session is waiting. This is small and it is
  what JJ's phrase "open it back up" feels like when the process survived. It is not strictly persistence. It changes
  connection behavior, so it needs his yes.

##### DC8. Things I recommend staying as they are

- **Waterfall history:** too big to save, and stale after an absence.
- **Sleep countdown:** a new listening session means a new deadline (`NereusApp/Session/SessionController.swift:213-223`).
- **Jump to another slice and listened slices:** the Core drops those claims when the device's absence ends
  (`src/core/session/StationServer.cpp:12459`), so restoring them would mean new claims on other devices' slices.
- **Setup's Display scope:** the page already picks a sensible default when opened.
- **PTT:** connecting never keys, and a held PTT is released on leaving (`NereusApp/Main/TransmitModel.swift:937-941`).

##### DC9. Key naming on the phone

- `SetupFilterPresetsMode`, `SetupPaTempUnit` and `display/showPaValuesPage` skip the `phone.` prefix that
  `PhoneSettings.swift:38-41` describes. They work, so this is tidiness and not a bug. Moving them would need a
  migration, so I recommend leaving them unless the new navigation keys bring a cleanup anyway.
- The CLAUDE.md rule of `AppSettings` and PascalCase keys is about the C++ app. The phone uses UserDefaults with
  dotted keys throughout, and I did not count that as a violation.
