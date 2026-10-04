# Control reference

Use this chapter to locate common commands and distinguish which part of the station they affect. Menu paths name the desktop command first; iPhone/iPad routes use tab names. For operating procedures, see [receive](03-receive.md), [slices](04-slices.md), [transmit](05-transmit.md), and [shared Core operation](08-shared-core.md).

## Common paths

| Task | Desktop path | iPhone or iPad path | Scope |
| --- | --- | --- | --- |
| Select playback and microphone devices | **Setup > Audio > Devices** | **Setup > Audio** device controls | This computer / this phone |
| Configure VAX audio | **Tools > VAX Audio...** or **Setup > Audio > VAX** | **Tools > VAX Audio** | Desktop: this computer, including remote windows. Phone: Core computer. |
| Configure TCI audio streams | **Setup > Audio > TCI** | **Tools > TCI Server** for Core server options | Desktop audio setup is this computer; iOS server is Core |
| Enable TCI server | **Tools > TCI Server...** or **Setup > CAT & Network > TCI Server** | **Tools > TCI Server** | This computer on desktop; Core on phone |
| Spot sources and filters | **Tools > Spot Hub...** | **Tools > Spot Hub** | Core station data |
| FreeDV station list | **Tools > FreeDV Reporter...** | **Tools > FreeDV Reporter** | Core station service |
| Link and audio diagnostics | **Tools > Network Diagnostics...** | **Tools > Connection and performance** | Desktop network view / phone and Core measurements |
| PureSignal status and controls | **Tools > PureSignal...** | **Tools > PureSignal** | Radio/Core; requires compatible capability |
| Support diagnostics | **Tools > Support Bundle...** | **Tools > Support Bundle** | Bundle identifies phone and Core contents |
| Display and appearance | **Setup > Display**; **Setup > Appearance** | **Setup > Display** | Page labels identify phone/Core scope |
| Inspect saved Core, retain addresses and rename | **Setup > Cores > Your Cores** | Saved Core row actions; **Setup > Devices** when connected | Desktop inspection is local; accepted rename changes the shared Core. |
| Core audio quality and media recovery | **Setup > Cores > Audio with the Core** | **Sound**, **Setup > Data use** | Current window/device preference; read actual format/health. |
| Canvas, meter sources and individual controls | **Containers > New / Edit Container** | No desktop Canvas editor on phone | Desktop arrangement; live controls retain their ordinary receiver/TX authority. |
| Core service and pairing | **Setup > CAT & Network > Remote Access** | Welcome/connect flow; **Setup > Devices** for paired-device administration | Service on this computer; device registry on Core |
| Radio selection and saved targets | **Radio > Connections…** (or **Manage Radios…** in a radio-panel window) | **Radio > More > Manage Radios** for the radio the connected Core runs; **Your Cores** list for saved connection targets | Choosing a different radio changes the Core's shared radio and reconnects devices; this is not the same as choosing a saved Core target. |
| TX input and microphone route | **Setup > Audio > TX Input** | Main **TX**, **Modes** transmit section; **Setup > Audio > On this phone** for the phone microphone | Mixed input/device and Core transmit settings |
| TX/mic profiles | **File > Profiles > TX Profiles... / Mic Profiles...**, **Setup > Audio > TX Profile** | Offered Core-described profile settings | Core transmit configuration |
| Filter bank | **Setup > DSP > Filter Presets** | Offered Core-described Setup page; quick presets in Modes | Shared Core bank; choosing a preset applies to one slice |
| Advanced receive DSP | **Setup > DSP > AGC/ALC, NR/ANF, NB/SNB, CW, TNF, Options** | **Modes**, then offered Core Setup pages | Core receive processing; some pages include separate TX groups |
| Antennas and radio hardware | **Setup > Hardware > Hardware Config** | **Radio**, **Setup > Hardware** as offered | Physical radio/front end, often shared by several slices |
| PA calibration and readings | **Setup > PA > PA Gain / Watt Meter / PA Values** | Offered Core Setup pages | Radio/Core; compatible PA profile required |
| Voice processor, expander, VOX | **Setup > Transmit > Speech Processor / DEXP/VOX**; **Setup > DSP > CFC** | Offered Core controls, Modes transmit section | Core TX, subject to transmit-settings permission |
| External amplifiers and tuners | **Setup > CAT & Network > 4O3A / RF-Kit**, accessory applets | **Radio** accessory pages, offered Core Setup | Core station accessories |
| Waterfall history and 3D | Band display; **Setup > Display > Waterfall Defaults / 3D View** | Main Display; **Setup > Display > On this phone** | Local picture/history; Core capability governs feeds |
| Meter containers | **Containers > New Container... / Edit Container** | No desktop container designer on phone | Desktop layout and item sources |
| Settings validation and backup | **Setup > Diagnostics > Settings Validation / Export / Import** | Offered Core hygiene controls; device-key backup in Devices | Operation-dependent scope, explained before confirmation |
| Logs | **Setup > Diagnostics > Logs**, Support dialog | **Setup > Diagnostics > Logs**; Support Bundle | Desktop/phone view; Core categories are shared |

## iPhone and iPad operator procedures

### Change the radio a connected Core runs

With the phone connected to the Core you intend to change, open **Radio > More > Manage Radios**. The page asks the Core for its radio list while the page is open. Read each card's name (or MAC if unnamed), model, protocol, IP address and MAC; **In use** identifies the radio it currently runs. **Scan again** requests a fresh LAN scan. Wait for **The Core is looking for radios** and the refreshed list. An empty-list message or a greyed action states when the Core is offline, too old, on air, or this session did not use a paired device key.

To switch radios, choose an inactive radio, review the confirmation, then tap **Choose**. The confirmation says the Core switches and every connected device, including this phone, reconnects. After the phone reconnects, read the Core/link and radio identity before continuing; do not assume a successful tap changed the selected radio. If the Core refuses the request, its words appear in **Refusal**. A disconnect or missing capability disables the action with its reason. Choosing the current **In use** radio is not a switch.

Use **Change model** only to change the model saved for a listed radio. Pick one of the Core-provided models; the checkmark marks the model the Core currently reports. The Core saves that choice for its next connection to that radio, so it does not switch the Core immediately. When the older Core does not list model choices, or lists only one, the page gives the reason instead of guessing. To remove an inactive saved radio from the Core, tap **Forget**, read the confirmation that also removes that radio's saved model, then tap **Forget** to confirm. A later scan may list the radio again while it is on the network. Check the resulting list or the Core's refusal before proceeding. These actions affect the Core's station, not just the phone's saved **Your Cores** list.

### Adjust the Core's VAX levels from the phone

Open **Tools > VAX Audio**. This page controls VAX channels on the Core's computer, not audio devices on the phone. Each of **VAX 1–4** identifies its virtual device and feeding slice or slices, then provides **Level**, a 0–100% slider, a level meter, and **Mute**. Set a channel level to change what digital-mode apps receive from that VAX channel. **Mute** silences that channel for its applications; it does not mute the speakers. Watch the meter and device/slice labels to make sure the intended receive path is selected. The **VAX microphone** section identifies the transmit slice and has its own **Level** slider and meter. That level changes the Core's VAX microphone input, not this phone's microphone route. Wait for values/meters to refresh; if a request is refused, the page displays the Core's reason. The TX-level slider is disabled unless this phone may transmit. Missing channel or meter data shows an older-Core/unavailable reason rather than a zero value. VAX application routing and platform availability are in [Digital audio and external applications](09-tools.md).

### Use iPad screen arrangements and the meter

Follow the [two-slice iPad listening session](07-iphone-operate.md#worked-session-two-slices-on-an-ipad) for the operating order. The details below explain the layout and meter choices used by that session.

On an iPad, the band screen uses three arrangements. With a regular-width window whose shorter side is at least 600 points, landscape uses the **applet column**: the band on the left and a 320-point column on the right, with the S-meter at the top and RX then TX controls below in one scroll view. Use the toolbar corner button to hide or show that column. In portrait at that size, the band stays full width and the S-meter, RX and TX are three columns below it; RX and TX scroll separately. Turning the iPad to landscape switches back to the applet column. A narrow split-view window or compact-width environment falls back to the phone arrangement, even on an iPad.

The analog S-meter shows the active slice's RX reading while receiving and the selected TX reading while on air. Tap the meter title bar's menu button, or press and hold the meter face, to open its menu. **RX Mode** offers Signal, Sig Avg, Signal Peak and Max Bin; **TX Mode** offers Power, SWR, Level and Compression. A mode whose required Core reading is unavailable is disabled with a reason. **Peak Hold** contains **Enabled**, **Decay**, and **Reset**. **Meter Face** selects one of the offered vintage faces or **Classic**. Check the needle/readout after a mode change; a disabled option is not a zero meter reading.

### Edit a Core-described phone Setup control

Open **Setup**, choose a category, then a page the connected Core describes. The page uses the Core's labels, help text, choices, units and ranges. Native controls operate the shared Core/radio setting described by that page, not a private phone copy. A toggle or choice sends its selected value; a number control uses its step buttons; a slider sends when you release it; a text field sends when you submit it. A button or a particular value can ask for a confirmation. Read the control name and question, then select **Yes** to send or **No** to leave the value unchanged. Wait for the Core's updated value before treating an edit as accepted. If the Core refuses or the link is lost, the page shows its refusal or unavailable reason and retains the last accepted readback. Controls can be disabled for missing Core capability, stale/not-ready settings, transmit ownership, or on-air state. Do not retry until that displayed condition is addressed.

Some tables have native panels rather than generic rows: **Notch Table** edits an existing notch's center/width, active state and delete action; use [receiver DSP notch procedures](13-receiver-dsp.md). **Settings Validation** lists severity, problem and detail and provides **Re-validate**, **Repair**, and **Forget** with Core confirmations where offered; use [settings recovery](11-troubleshooting.md). **Antenna Rows** selects per-band physical paths; see [hardware and antennas](17-hardware-antennas-calibration.md). **PA Telemetry** is read-only and marks stale/unreported readings unavailable with their age; see [PA readings](17-hardware-antennas-calibration.md). **CFC Bands** edits the transmit compressor/EQ curves on the Core; see [voice processing](14-voice-profiles.md). If a specialized control says it is unavailable on this phone, do not infer that its generic table placeholder is editable.

## Desktop Setup page map

Open **File > Settings...** and select a leaf in the left-hand tree. Category
names group pages; selecting a category is not the same as selecting an
editable page. Read page/group scope and disabled reasons. In a remote window,
some values arrive asynchronously from the Core and can remain unavailable
until that readback is ready.

| Category | Implemented pages in the covered source | Detailed instructions |
| --- | --- | --- |
| **General** | Startup & Preferences; Options; This Core in a remote window. | [Connect](01-desktop-connect.md), [sharing](08-shared-core.md), [spots and identity](16-spots-reporters.md). |
| **Cores** | Your Cores (Overview, Addresses, Radio, Devices); Audio with the Core. Inspection and current connection are distinct. | [Core Settings and audio](01-desktop-connect.md), [sharing](08-shared-core.md). |
| **Hardware** | Hardware Config, whose tabs depend on the radio. | [Hardware and antennas](17-hardware-antennas-calibration.md). |
| **PA** | PA Gain; Watt Meter; PA Values, with radio capability reasons. | [Calibration](17-hardware-antennas-calibration.md). |
| **Audio** | Devices; TX Input where available; VAX; TCI; Advanced; TX Profile. | [Transmit](05-transmit.md), [digital routing](09-tools.md), [profiles](14-voice-profiles.md). |
| **DSP** | AGC/ALC; NR/ANF; NB/SNB; CW; AM/SAM; FM; CFC; TNF; Filter Presets; Options. | [Receiver DSP](13-receiver-dsp.md), [voice processing](14-voice-profiles.md). A parent page can contain disabled or hidden unbuilt groups. |
| **Display** | Spectrum Defaults; Spectrum Peaks; Waterfall Defaults; Grid & Scales; Multimeter; TX Display; 3D View. | [Display and layout](10-customize.md). |
| **Transmit** | Power; Speech Processor; DEXP/VOX. | [Transmit](05-transmit.md), [voice processing](14-voice-profiles.md). TX Profile is under Audio. |
| **Appearance** | Colors & Theme; Meter Styles. | [Display and layout](10-customize.md). |
| **CAT & Network** | TCI Server when built; 4O3A; Remote Access; RF-Kit. | [Digital routing](09-tools.md), [phone connection](06-iphone-connect.md), [accessories](19-amplifiers-tuners.md). The category name does not imply implemented serial CAT. |
| **Test** | Two-Tone IMD, with transmit/radio gates. | [PureSignal](18-puresignal-diversity.md). |
| **Diagnostics** | Radio Status; Connection Quality; Settings Validation; Export / Import; Logs. | [Troubleshooting and support](11-troubleshooting.md). |

The phone Setup tree combines native phone pages with pages described by the
connected Core. Therefore this desktop tree is not a promise of an identical
phone tree. A Core can omit an unsupported page, or show it with a version or
capability reason. Native phone Navigation, Audio, Display, Battery and
sessions, Data use, PTT buttons, Devices, Logs and About pages are covered in
[phone session care](20-phone-preferences.md) and the phone chapters.

## Menus and applets at a glance

| Desktop surface | Main implemented operating uses |
| --- | --- |
| **File** | Settings, profile editor routes, configuration Import/Export routes, Quit. |
| **Radio** | Connect last target, Disconnect, Manage Radios, state-dependent radio management and Protocol Info. |
| **View** | Pan Layout, add slice, float pan, display duplex, Band Plan and performance presentation. |
| **Band** | Choose available HF/GEN/WWV band targets. |
| **Mode** | Set active slice's demodulation. FM availability here is receive support, not FM transmit. |
| **DSP** | Quick NR/NB/ANF/SNB/APF/AGC/TNF actions for the selected receiver or stated master function. |
| **Containers** | Applet visibility, container creation/editing and default-layout reset. |
| **Tools** | VAX/TCI when built, Spot Hub, FreeDV Reporter, RADE-related operating surfaces, PureSignal, Diversity, Network Diagnostics and Support Bundle as offered. |
| **Help** | What's New, About and platform-specific diagnostics. Integrated Getting Started/Help/Data Modes pages remain unbuilt. |

The **Containers** menu's **Applets** section and the panel **☰** menu show
registered applets. Availability can depend on connected hardware, an enabled
service or the desktop build. An amplifier panel will not become functional
merely because it is checked. Use [window controls](02-desktop-window.md) to
show/float/dock an applet, and the relevant subject chapter to configure it.

## What is personal and what is shared

| Setting or operation | Normal scope and qualification |
| --- | --- |
| Selecting the active slice, arranging panes or floating controls. | This device's operating focus/layout. Other slices continue receiving. |
| Frequency, mode, passband, AGC and RX DSP of an owned slice. | That shared receiver. A listening device does not gain edit rights by selecting it. |
| **Your volume** and mute while listening to another device's slice. | Personal to the listening device. |
| Preset-bank edits, notches and station identity. | Core bank/station state, distinct from ordinary per-slice preset selection. |
| ADC attenuation, preamp, physical antenna/filter routing and sample-rate resources. | Radio/receiver-window hardware; other slices can be affected. Read any impact confirmation. |
| Microphone device/backend and playback route. | Computer or phone hosting that route. Core TX input/processing settings are a different group. |
| TX slice, keying, drive, TX bandwidth, processors and profiles. | Core transmitter, with TX ownership/settings gates. Receiving successfully is not transmit permission. |
| Desktop VAX and TCI. | This computer, including a remote desktop window. |
| Phone Tools VAX and TCI Server. | Core's computer. These pages do not create desktop virtual devices on the phone. |
| Palette, grid, layout and local history. | Local presentation, with separately scoped Core analyzer/subscription fields where offered. |
| Paired-device removal, pairing and Core key backup. | Core device registry/identity. Removing a saved Core from a phone is a different action. |
| Diagnostic category switches for a Core. | Shared logging. Clearing a view need not clear the underlying file. |

**Mixed** or **Both** means inspect the individual group. It does not mean
every field affects both places equally. A requested change, a pending
confirmation and an accepted readback are different states.

## Verified desktop shortcuts

| Shortcut | Action |
| --- | --- |
| **Ctrl+,** | Open **Settings...** |
| **Ctrl+K** | Connect to the remembered target when actionable. |
| **Ctrl+Shift+K** | Disconnect. |
| **Ctrl+L** | Open **Pan Layout...** |
| **Ctrl+R** | Add a slice |
| **Ctrl+Shift+S** | Open **Spot Hub...** |
| **Ctrl+Shift+R** | Open **FreeDV Reporter...** |
| **Ctrl+Shift+D** | Open **Diversity...** |
| **Ctrl+Shift+X** | Clear all spots. |
| **Ctrl+Shift+N** | Toggle TNF master processing. |
| **Ctrl+Q** | Quit NereusSDR |

These actions are present in the desktop menus of the source build covered by this draft. The current build has no shortcut-remapping page. macOS may display the platform's Command modifier in place of Ctrl; use the shortcut shown in the menu.

## Gesture reference

| Surface/action | Gesture | Readback to check |
| --- | --- | --- |
| Desktop numeric frequency. | Double-click the frequency, enter explicit units, Enter. | Final VFO frequency. |
| Desktop quick tune. | Click empty spectrum/waterfall; wheel over frequency/band. | Active slice and selected step. |
| Desktop pan. | Drag empty band horizontally. | View centre/span, not an assumed VFO change. |
| Desktop filter/tune. | Drag an edge to resize; drag inside the passband to tune. | Both filter edges or VFO frequency as appropriate. |
| Desktop band zoom. | Ctrl/Command+wheel. | Frequency span. |
| Desktop reference level. | Shift+wheel over band. | Level window. |
| Desktop dBm ruler. | Wheel changes range; left-drag moves it; right-drag stretches it. | Minimum/maximum level and visibility of signals. |
| Desktop RX-to-TX filter match. | Shift-click a preset. | RX passband and TX BW separately; the TX request can be refused. |
| Desktop manual notch. | Add from spectrum context menu; drag centre/edge; wheel adjusts width, Shift for fine width. | Marker, TNF master and audible result. |
| Phone selection/tuning. | Tap flag to select; use enabled tap/drag/number-pad/dial gestures. | Ownership and final frequency. |
| Phone zoom/pan. | Pinch where enabled; drag empty band; use minus/plus. | Span/centre and live/history state. |
| Phone gestures. | Choose them in Setup > General > Navigation. | Verify the chosen action in receive. |
| Phone PTT. | Tap on-screen PTT to key, tap again to unkey. | TX badge, owner, keying indication and final receive state. |

Notch hit areas take precedence over ordinary band gestures. Locked,
listened-to or on-air controls can refuse a request. See [desktop gestures](02-desktop-window.md)
and [phone operation](07-iphone-operate.md) for the full context.

## Availability in this edition

This manual covers a development checkout, with real desktop figures from a
separately identified development GUI. Check your installed build and connected
Core rather than treating the manual as a public release compatibility list.
The operator-facing exclusions below explain common missing controls.

| Missing/unimplemented surface | Supported alternative or boundary |
| --- | --- |
| Serial/TCP CAT, MIDI and their status/tool pages. | Use supported TCI where suitable; it is its own interface and build dependency. |
| CWX, Phone/CW keyer page, CW keyer settings. | CW receive mode/filter/APF controls remain separately usable. Do not infer a working software keyer from CW modes. |
| Voice recorder/playback, DVK, macro buttons, Memory Manager and memory spots. | Use ordinary live receive/transmit and the implemented spot sources. |
| FM TX/repeater/offset/reverse, CTCSS encode/tone squelch and FM Phone/CW page. | FM receive/squelch remain; do not plan repeater transmission with these shells. |
| Transverters, VHF band/OC pages and band stacking. | Current implemented band selection and HF hardware controls. |
| Minimal Mode, UI Scale/Theme page, desktop Navigation editor, skins and keyboard remapping. | Actual fixed menu shortcuts, supported layout/colors and phone Navigation. |
| Receive DSP Equalizer. | TX Equalizer is a separate implemented feature. |
| SAM advanced options and APF bandwidth/gain fields. | SAM demodulation and APF enable/frequency have their own implemented paths. |
| Container Var1/Var2, RX2/Sub RX/Pan Swap, macros, Voice, Click Box. | Slices and ordinary filter presets; only wired designer items should be used. |
| FDX and container RX/TX antenna-split switch. | Display duplex **DUP** is implemented separately and is not full-duplex transmit support. |
| Container AVG, Multimeter averaging/holds/global history-enable checkbox and PBSNR. | Spectrum averaging, S-meter peak hold and a bound container History Graph first series are implemented; History duration updates existing graphs. The graph's second-axis editor has no live update path. |
| DDC Routing editor, bandwidth monitor, second HL2 I2C bus, hardware Signal Generator/Tests, frequency calibration Start. | Automatic DDC allocation, supported HL2 options and implemented diagnostic/calibration fields. |
| ACC microphone input/Phone-CW +ACC; Phone-CW MON. | Offered MIC/BAL/LINE/PC routes; TX applet MON is separate and implemented. |
| RF-Kit TUNE/BYPASS. | Use the accessory's implemented status and controls; do not assume these key/tune it. |
| Audio bit-depth/default-rate lookup, mic monitor/first-PTT tone check, IQ/TX-monitor-to-VAX and mute-VAX-during-other-slice-TX. | Implemented device/capture, VAX audio and TX monitor paths. |
| Desktop TX grid controls and waterfall low-level color. | Supported TX analyzer/palette and phone-local grid controls are separate. |
| Generic Logging & Performance and 60-second Connection Quality history. | Implemented Logs, Support diagnostic categories and current network measurements. |
| Phone headset/Bluetooth/Action-button/locked-phone PTT; High audio quality. | On-screen PTT; Standard audio. Disabled labels do not establish accessory support. |
| Spot Hub's unbuilt WSJT-X filter group, RBN rate limit and FreeDV-decode-to-PSK option. | The implemented feed/listener/filter controls described in the spots chapter. |

Capability-held features such as PureSignal, diversity, PA profiles, specific
antenna controls and neural noise processors require their compatible hardware,
Core or installed assets. TCI/Client Chain availability also depends on the
desktop build. Those are different from the explicit unbuilt surfaces above.

## Abbreviations used on operating controls

| Label | Meaning in use |
| --- | --- |
| **AF** | Receive audio level. |
| **AGC / AGC-T / AUTO** | Gain response, maximum receive gain, noise-floor-based gain adjustment. |
| **ATT / S-ATT / A-ATT** | Input attenuation, with stepped/automatic state where supported. |
| **SQL** | Squelch gate and threshold. |
| **NB / NB2 / SNB** | Impulse/spectral blanking methods. |
| **NR / ANF / TNF / APF** | Noise reduction, automatic notch, manual-notch master, CW peak filter. |
| **RIT / XIT / CTUN** | Receive offset, transmit offset, independent-pan tuning. |
| **RF PWR / TUNE PWR** | Normal/tune drive, or supported PA-profile watts interpretation. |
| **MOX / TUNE / MON** | Manual keying, tune transmission and audio monitor. |
| **PROC / LEV / ALC / EQ / CFC / DEXP / VOX** | Transmit processing/leveler/level control/equalizer/compressor/expander/voice keying; see the voice chapter for interactions. |
| **DUP** | Display duplex; distinct from full-duplex RF operation. |
| **BIN** | Binaural I/Q headphone playback. |
| **TX BW** | Transmit audio passband width/edges, not a receive slice allocation. |

## Find a worked procedure

| Goal | Read |
| --- | --- |
| Hear a weak voice station and compare interference treatment. | [Receive](03-receive.md), then [DSP parameters](13-receiver-dsp.md). |
| Listen to two parts of a band or arrange multiple panes. | [Slices and panadapters](04-slices.md). |
| Prepare a microphone and make an over. | [Transmit](05-transmit.md), then [voice profiles](14-voice-profiles.md). |
| Connect a digital application. | [Complete VAX receive/TX/restore route](09-tools.md#worked-route-receive-through-vax-key-from-nereussdr). |
| Use RADE digital voice and find another station. | [RADE receive](15-rade-freedv.md#receive-with-rade), [short call](15-rade-freedv.md#prepare-and-transmit-rade), then [reporter connection](15-rade-freedv.md#open-freedv-reporter). |
| Configure the radio front end, antenna or PA readings. | [Hardware/calibration](17-hardware-antennas-calibration.md). |
| Operate feedback calibration or two-ADC diversity. | [PureSignal/diversity](18-puresignal-diversity.md). |
| Configure an external amplifier/tuner. | [Accessories](19-amplifiers-tuners.md). |
| Hand receiver control to another device and identify the separate TX holder. | [Coordinated receiver handoff and phone TX limits](08-shared-core.md#coordinate-a-second-device-handoff). |
| Listen from a phone with headphones or cellular data and recover an interruption. | [Long headphone session](20-phone-preferences.md#worked-example-a-long-headphone-listening-session). |
| Listen to two slices on an iPad. | [Two-slice session](07-iphone-operate.md#worked-session-two-slices-on-an-ipad), then [layout and meter choices](#use-ipad-screen-arrangements-and-the-meter). |
| Collect evidence for a connection or audio fault. | [Troubleshooting](11-troubleshooting.md). |

## Terms

**Core**: The NereusSDR service connected to a radio. In phone operation it runs on the station computer and performs signal processing.

**Device**: A desktop window or paired phone connecting to a Core. Device-local audio and display choices do not automatically change another device.

**Slice**: A receiver channel with its own frequency, mode, filter, and receive controls. It may be owned by one device and shown read-only on another.

**Panadapter**: A frequency display containing one or more slices. A panadapter is a display arrangement; it is not itself a receiver.

**TCI**: The interface through which compatible programs can exchange radio control and audio with NereusSDR. A connected client needs transmit ownership before it can key.

**VAX**: Virtual audio channels for routing receive and transmit sound to digital-mode software on the computer running those channels.

**RADE**: A digital voice mode with receive decoding and a Core transmit vocoder. Select **RADE-U** or **RADE-L** for the appropriate sideband, then use the RADE applet for profile and decode status.

**TXcontrol**: Ownership held by the desktop window that is allowed to control transmit. TCI transmit requires this ownership.
