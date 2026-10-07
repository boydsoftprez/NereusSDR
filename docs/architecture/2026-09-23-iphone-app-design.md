# NereusSDR for iPhone and iPad: Design Spec

Status: **Approved** by JJ (KG4VCF) on 2026-09-23 ("onward"); D35 to D43
were added while planning, each his call; he changed D38 later that day and set
D43's wording rule on 2026-09-24. On 2026-09-24 he also replaced one device at a
time with several devices at once (§3.9, D44 to D67; D58 to D67 answer the station
design's questions the same evening), which replaces D21 and carries D22 forward.
Every decision in §3 is JJ's except D137, D138 and D139, which are the phone
controller's calls from 2026-10-01 and wait for JJ to confirm them at the pull
request review (D137 and D138 change what the app does; D139 is a test rule).
Plan: [2026-09-23-iphone-app-plan.md](2026-09-23-iphone-app-plan.md)
Branch: `claude/nereussdr-iphone-app-5fb988` (these documents); the app itself is built on `claude/iphone-app`.

This app is a client of the remote station, `nereusd`. That work is not on
`main` yet. Its documents live on the branch `codex/integrate-r2-main` (read at
`f9e6ef99`) and are cited here by path:

* `docs/architecture/2026-07-28-remote-daemon-architecture-design.md`, "the
  remote design"
* `docs/architecture/2026-08-02-remote-station-identity-and-pairing-design.md`,
  "the pairing design"
* `docs/architecture/2026-09-20-remote-daemon-r3-plan.md`, "the R3 plan", whose
  requirements are numbered `R-R3-nn`
* `docs/architecture/2026-09-20-remote-media-control-v1.md`, "media control"

Mockups: every screen is pictured in `2026-09-23-iphone-app-design/`, one
picture per board section (§10). `2026-09-23-iphone-app-design/board.html` is
the interactive board itself; open it in a browser, and it loads nothing from
the network. The same board is published as a private artifact:
<https://claude.ai/artifact/9Bu5TcyL63LvkGSB28tQo9>.

Signals, station names, addresses and readings in the mockups are examples.

---

## 1. Goal

Run the whole station from an iPhone or an iPad, at home on Wi-Fi or away on
cellular, as a full operating console: receive **and** transmit, both in the
first App Store release.

The phone is a thin client of the station. Radio protocol, DSP, demodulation,
the transmit chain and every safety interlock stay on the station. The phone
draws the band, plays the audio, captures the microphone, and sends what the
operator asks for. It looks and works like NereusSDR, VFO flags included, so an
operator who knows the desktop knows the phone.

## 2. Non-goals

* **Not the desktop app compiled for iOS.** Rejected because it would put WDSP
  and Thetis-derived GPL code on the App Store (§4.11).
* **Not a web page in Safari.** Rejected because the audio stops when the phone
  locks.
* **No demo mode.** A station is required (D19). App Review gets a video of the
  app running against a real station instead.
* **Not in this release, by JJ's choice:**
  * Parked by JJ ("skip for now"): a left-handed layout; larger text and
    VoiceOver.
  * Declined: WSJT-X decodes on the phone; the iPad beside a logging app; a
    listen-only guest device.
  * Rejected as unneeded: a Face ID gate for transmit. A lost phone is handled
    by revoking it from any paired device, which drops its session at once.
  * Offered and not taken up: alerts while the app is closed, an Apple Watch
    app, Siri and Shortcuts and widgets, sending CW from the phone.
  * Dropped for now while planning (D42): MIDI Mapping and Macro Buttons.

---

## 3. Decisions

Every row is a call JJ made, one question at a time, on a rendered mockup.
"Rejected" lists the alternatives he was shown and turned down.

### 3.1 Product

| # | Decision | Why | Rejected |
| --- | --- | --- | --- |
| D1 | Run the station from anywhere as a full operating console. | The purpose of the app. | |
| D2 | Anyone can install it, through the App Store, after a TestFlight beta round. | Reach, with a safety net first. | |
| D3 | A native Swift app, a thin client of `nereusd`. All DSP stays on the station. | The station already does the work; the phone stays light and the radio logic lives in one place. | The Qt app built for iOS (GPL and WDSP code on the store); a Safari web app (audio stops when locked). |
| D4 | The app's own code is GPLv3 plus an App Store permission clause. Any code shared with the phone needs the same clause. | Apple's store terms add restrictions GPLv3 forbids unless the copyright holders grant an exception. | |
| D5 | The first store release has receive **and** transmit. | JJ: "do not descope". It therefore ships after R3, R4 and R5 (§4.2). | Listen first, transmit later. |
| D6 | iPhone and iPad. | Both are operating positions. | |

### 3.2 On the band

| # | Decision | Why | Rejected |
| --- | --- | --- | --- |
| D7 | The phone keeps NereusSDR's VFO flags and look: a toolbar, a tall panadapter with the flag and the band-plan strip, the waterfall with zoom, and a tab bar (Panadapter, Modes, Tools, Radio, Setup). | It has to be NereusSDR. | A full-screen band with a bottom sheet (it dropped the flags). |
| D8 | PTT is a round button on the waterfall, bottom left, and it **toggles**: tap to key, tap again to unkey. | One thumb, no hold. Its safety net is §4.6. | Hold to talk. |
| D9 | One flag rule for both devices: every slice keeps its full flag unless it would land on another flag, and then it folds to a one-line tag (letter, frequency, TX badge). The active slice is always full; one tap swaps. On the phone this nearly always folds; on the iPad it usually stays full. | Flags stay readable on a narrow screen without burying one under another. | Stacked full flags; the desktop's placement. |
| D10 | Band markers follow the desktop: each slice's centre line, triangle and passband edges in its own colour when it is the selected slice; the others with a darker line and triangle and grey edges; the selected slice's marker on top. One slice is selected for the whole window. The shaded passband keeps the operator's colour. | JJ's desktop call of 2026-09-23 (committed on `claude/slice-marker-colours`, not yet merged); the phone draws the desktop's band. | Every slice in slice A's cyan. |
| D11 | Turned sideways, the same layout turned: toolbar on top with the station in the middle, the band edge to edge, PTT and zoom on the waterfall. | Nothing to relearn; the wider band keeps both flags full. | Band only with no tab bar; the iPad's applet column at phone size. |
| D12 | A tuning dial, three kinds, all selectable in Setup, General, Navigation (this phone): a knob on the waterfall, a knob in a sheet, a thumbwheel. **Off until chosen.** A haptic tick on each detent and a firmer bump on each whole kilohertz. | JJ asked for a dial with selectable detent steps and haptics. | |
| D13 | Spots start below the flags on the phone: at the lower of halfway down and just under the lowest flag, using only the rows that fit, with the rest in +N badges. Upright that is still halfway, as on the desktop. | Sideways, the desktop's halfway start puts spots behind the flags. | The desktop's halfway start regardless. |
| D14 | While transmitting, the band shows the mic level beside RF power and SWR, upright and sideways, as well as in the TX panel. | JJ asked for it during the confirm pass. | |

### 3.3 Tabs

| # | Decision | Why | Rejected |
| --- | --- | --- | --- |
| D15 | Controls live in both places: the Modes tab has the active slice's full set; the RX and TX panels on the band keep a quick subset. | Everything is reachable, and the common things are one tap away. | |
| D16 | Setup is the desktop's whole Setup tree (same categories, order and page names), each category marked Core, This phone or Both, with Devices added first. | One mental model on both. | |
| D17 | The front end (preamp, step attenuator, RX and TX antennas, RX-only inputs) lives in the Modes tab with the slice, as the desktop's RX applet has it, not on Radio. The RX2 attenuator sits in the Step att row (D131). | It is set with the slice. | |
| D18 | The amp's OPERATE and the tuner's TUNE live in the TX panel, beside RF power, TUNE and MOX. | They are part of getting on the air. | |

### 3.4 Connecting

| # | Decision | Why | Rejected |
| --- | --- | --- | --- |
| D19 | A Core is required: no demo mode. | The app is a console for a real station. | "Try it without a Core" with a sample band. |
| D20 | The station runs from **a switch in the desktop app**, on a new Remote Access page under CAT & Network. It keeps running after the app closes and can start with the computer. | No second install for the common case. | A separate station install. |
| D21 | ~~**Ask before taking over.** Connecting to a station another device is using names that device and asks first. A device reclaiming its own dropped session is not asked.~~ **Replaced on 2026-09-24 by §3.9:** a second device connects alongside the first (D44); asking before taking a place applies to a fifth device (D55); reclaiming keeps its rule (D56). | A second person at home is not thrown off silently. Amends remote design §7.1 (§4.5). | Preempt at once. |
| D22 | **Taking over may cut a transmission off.** "Unkey and take over" unkeys the other device first. Carried into §3.9 on 2026-09-24: taking transmit (D51), or a place (D55), from a device on the air unkeys it first. | A transmission left running at home can be stopped from anywhere. | Wait until it unkeys. |
| D23 | **The app keeps the older link too.** Every app release still speaks the link one major version back; only a station two majors behind is refused. Features the Core can't do yet are greyed "Needs a newer Core". | An App Store update must never lock the operator out. Amends remote design §7.0 (§4.4). | A station self-update triggered from the phone; updating at the station only. |

### 3.5 Away from the app

| # | Decision | Why | Rejected |
| --- | --- | --- | --- |
| D24 | **Locking the phone while keyed unkeys it.** The screen stays awake while keyed, so it only locks when the operator locks it. iOS tells an app about a lock only as protected data becomes unavailable, about 10 s after the lock and only on a phone with a passcode, so a locked phone stops within about 10 s; without a passcode the Core's transmit time-out is the backstop (JJ accepted this limit on 2026-09-26 to keep D25, over unkeying whenever the app leaves the screen or using a private lock signal). A locked phone never transmits from its screen; the lock-screen card only reports. | iOS can't start the microphone while locked. On the lock screen UNKEY, Mute and Cancel run with one tap, and Reconnect asks for Face ID (JJ, 2026-09-26). | Transmitting while locked with a Face ID UNKEY. |
| D25 | **Switching apps while keyed keeps transmitting.** TX and its clock stay red in the Dynamic Island over every app, with the orange microphone dot, and the opened island unkeys in one tap. | Reading a net script in another app while talking. | Leaving the app unkeys. |
| D26 | **A hardware button keys a locked phone through Apple's Push to Talk**: a headset button, a paired Bluetooth PTT button or the Action button. While connected, iOS shows its blue Push to Talk pill and its own lock-screen panel; only one Push to Talk app can be active at a time. | A button in a pocket is the one way to key a locked phone, and only Apple's system can do it. This is the one exception to D24. | Buttons only while unlocked. |
| D27 | The screen stays on while the band is showing, on battery too. "Never" and "While charging" remain as choices. | Operating is looking at the band. | "While charging" as the default. |
| D28 | Cellular starts at Balanced; Wi-Fi at Full. | Data costs money on cellular. | Full on cellular too; asking the first time. |
| D29 | **Transmit time-out on for phone and iPad**, at Thetis's default of 3 minutes, adjustable from 30 seconds to 30 minutes or off. The desktop at the station keeps Thetis's default of off. | The PTT is a toggle, so a missed second tap or a pocket press could keep the radio on the air while the link is healthy, and nothing else would stop it. | Off until switched on; on everywhere, the desktop included. |

### 3.6 iPad, spots and reporters

| # | Decision | Why | Rejected |
| --- | --- | --- | --- |
| D30 | iPad on its side: the applet column, like the desktop's main window. The band on the left; the analog S-meter, RX and TX (with the amp and tuner) in one column on the right; a corner button hides the column. | The desktop's own arrangement. | The phone's two panels docked on both sides; the phone layout scaled up. |
| D31 | iPad upright: the applets below the band in three columns, like a radio's front panel; on its side it goes back to the column. | The band keeps the full width, so both flags stay full. | The column beside the band; the column over the band. |
| D32 | Spot Hub on the phone is one Tools page with the sources listed. | The desktop's ten tabs don't fit a phone. | Ten tabs across the top. |
| D33 | FreeDV Reporter: a tap tunes to a station, as the desktop's double-click does; its details and actions are on press and hold. | Same as the Spot List. | |

### 3.7 The confirm pass

| # | Decision |
| --- | --- |
| D34 | On 2026-09-23 JJ reviewed every drawn detail he had not ruled on, in six steps, and kept all of them as drawn. They are listed per screen in §5. |

### 3.8 Decided while planning

Choices the code could not settle, put to JJ on 2026-09-23 with a
recommendation each. He kept D35 to D40 as recommended; D41 and D42 are his
answers in his own words. He later changed D38 against the recommendation, to match
his 2026-09-22 answer to the Core/GUI session. D43 is a wording rule he gave that
session on 2026-09-24.

| # | Decision | Why | Rejected |
| --- | --- | --- | --- |
| D35 | **The desktop's station switch hands over.** While NereusSDR is open it runs the station itself and serves the phone; when it closes, a background station (`nereusd`) takes over the radio and keeps serving; when it opens again it takes the radio back. While the app is open, the operator at the desktop and a phone can both operate, as when standing at the radio with the phone in hand. With §3.9 the desktop's own window takes part as one of the devices on its Core, owning its own slices and taking transmit by the same rules; the Core/GUI session's design for several devices says how it counts toward the four. | The desktop keeps working exactly as it does today, which remote design §5 requires; the loopback end state needs every control mirrored first. | The desktop as a client of a background station whenever the switch is on. |
| D36 | **The station also advertises itself over Bonjour (DNS-SD)**, alongside its existing announcement, and the phone finds stations that way. | iOS only lets an app receive custom multicast with a special permission Apple grants on request, and a refusal would block the release. Amends pairing design §6. | Asking Apple for the multicast permission. |
| D37 | **The pairing code's key exchange uses a published library on both ends:** SPAKE2+EE (BSD-2-Clause) on libsodium (ISC). | Homemade cryptography is how security bugs get in. | Writing the exchange on the cryptography already shipped (OpenSSL, Mbed TLS). |
| D38 | **The rendezvous and relay run on a dedicated server of their own**, at `rv.nereussdr.com` (with `rv4` and `rv6` for the relay), not on the website's server. The rendezvous's WebSocket rides behind that server's own Caddy by host name; coturn takes UDP 3478 and 443; the website's server and its files are untouched. | JJ, 2026-09-26, reversing his 2026-09-23 call after a review showed a flood through the rendezvous could take nereussdr.com down on the shared server. | The website's server (JJ's 2026-09-23 call: no new machine to run). |
| D39 | **The station also accepts the app one major version back**, just as the app accepts the station one back. | Neither update order can lock the operator out while an App Store update waits in review. Amends §4.4 and remote design §7.0. | Changing only the app's side. |
| D40 | **Filter presets live on the station**, and every device shows the same ones. | One station, one set of presets, like the radio's own memories. A desktop connected remotely starts sharing the station's presets; a desktop running the radio locally is unchanged. | Each device keeping its own. |
| D41 | **An item the desktop has not built appears on the phone once it exists.** The phone keeps the desktop's order and names for tools, Radio tab items and Setup pages, and leaves out any the station does not offer yet; each appears by itself, in its place, when it is built. VAX and antenna selection are built and working, so VAX Audio and Antenna Setup are on the phone from the start even though their desktop menu entries are not finished. | App Review rejects apps that show placeholder or "coming soon" items. JJ: "show each once it exists however vax, antenna selection are there and working". | Greyed items as on the desktop; building every missing desktop feature in this plan. |
| D42 | **MIDI Mapping and Macro Buttons are dropped for now.** | Neither exists on the desktop and nothing defined what they would do. JJ: "drop these for now". | Building a first version now; drawing them on the board first. |
| D43 | **On screen, the NereusSDR computer you connect to, update and pair with is "the Core".** "Station" appears on screen only in its ham sense: your station, the station callsign, the station's network. This document's prose, code identifiers, wire names and the link document keep "station". | One name for the same computer on the desktop and the phone, while "station" keeps the meaning hams give it. JJ's rule, given to the Core/GUI session on 2026-09-24. | "Station" for both. |


### 3.9 Several devices at once

On 2026-09-24 JJ asked for several devices on one Core at the same time, in place
of one device taking the Core over: "since we have a computer running there,
whether it be single board or core combined, it makes sense to me to be able to
have both clients connected." He settled the model one question at a time, then
kept the drawn screens (§5.8, and §5.9 for the notices and the states in between) with
the details they needed, marked below as drawn details. The station half (the session
model, who owns what, the arbitration, and the admission rules that replace link
§12.4) is [2026-09-24-several-devices-on-one-core-design.md](2026-09-24-several-devices-on-one-core-design.md),
written by the Core/GUI session and reviewed against this spec; §4.5 says what the phone
needs from it, and that design's sections 10.2 to 10.9 give the wire.

| # | Decision | Why | Rejected |
| --- | --- | --- | --- |
| D44 | **Up to four devices on one Core at the same time**, each with its own session. | JJ's call. | As many as the radio has slices (recommended); two at a time. |
| D45 | **Each device owns its slices and pans.** The Core hands receivers out from the radio's pool; only the owning device tunes, changes or closes its slices. | Two operators never fight over one VFO. | Everyone shares every slice. |
| D46 | **Other devices' slices show on the band as labelled, read-only markers:** a dashed line in that slice's colour, a label at the foot of the spectrum with the owning device's name, and no flag. A tap on the label says whose slice it is and that it can be changed only there (D115). | You see who is where before you tune onto them. | Only your own slices. |
| D47 | **Slice letters are shared across the Core.** A slice has one letter on every device, handed out from one pool as receivers are. | Two operators can say "slice B" and mean the same one. A drawn detail. | |
| D48 | **Two devices may share one receiver when their slices fit its window.** | JJ's call: the radio's receivers go further. | Each device gets its own receiver (recommended). |
| D49 | **When no receiver is free, a device takes one after confirming.** It sees which device has each receiver and what it is doing, and picks one; that device's slice on it closes, and that device is told who took the receiver and when, with Take it back, which asks the same question the other way. | D21's rule, per receiver. | Asking the other device first; never taking, only naming who holds them. |
| D50 | **Moving a pan whose receiver another device shares asks first**, naming that device. Once confirmed, that device's slice moves to a free receiver, or closes if none is free, and that device is told. | The same rule as other shared changes (D53). | The pan stays put while shared. |
| D51 | **One device holds transmit, and its operator is the control operator.** Every device shows who has it: the PTT button names the device (a drawn detail) and TX marks its slice. Another device takes transmit after confirming; the holder is told, with Take it back. If the holder is on the air, the button reads "Unkey and take over" in red and the Core unkeys it first. | One transmitter, one control operator. | Whoever keys first; only the device that connected first. |
| D52 | **The radio's own PTT takes transmit**: a mic or a footswitch (amended by D58 the same day: a program, even one on the Core's computer, transmits only while its own device holds transmit). The press is the confirmation; the holder is told, with Take it back, and a holder on the air is unkeyed first. | The operator at the radio is never locked out by a device elsewhere. | Ignoring it while a device holds transmit; keying for the holder. |
| D53 | **Any device may change a setting every device hears** (sample rate, preamp and attenuator, antennas, PureSignal, the amp and the tuner), but a change that would disturb another device's slices asks first and names that device, which is told afterwards. | JJ's call. | Only the device holding transmit (recommended); any device, with everyone told and nobody asked. |
| D54 | **When the devices together ask for more display and audio than the Core can send, the device with transmit keeps its full band and sound**; the others share what is left, their frame rates dropping first, with a "Sharing" chip on the band while it lasts (a drawn detail). | The control operator's band and sound come first. | Equal shares; first connected first. |
| D55 | **A fifth device takes one's place after confirming.** It lists the four connected (name, how long, listening on what or transmitting, when last active), starting on the one idle longest (a drawn detail). The device whose place is taken is told who took it and when, with Take it back; a device on the air is unkeyed first. | Four is the limit, and nobody is locked out for good. | Refusing the fifth, naming who is connected. |
| D56 | **A device coming back to its own dropped session is not asked.** It gets its slices back, and transmit only if nobody took it meanwhile (a drawn detail), within the 3 minutes D62 gives it. | D21's reclaim rule kept: a phone that lost its link is never locked out of its own transmitter, and never snatches transmit back from someone who took it properly. | |
| D57 | **Setup, Devices lists the devices connected now** apart from the devices only paired, each with its slice letters and bands, and TX on the one with transmit. | Who is on the Core, at a glance. | |
| D58 | **A program transmits only while its own device holds transmit.** WSJT-X or any program keying through a device's window (TCI, VAX or CAT) is refused otherwise, and the window says why; a person takes transmit first. The same holds on the computer whose window runs the Core. On a Core with no NereusSDR window, programs can't transmit. | A program keys on its own schedule and can't see who else is on the air, so it never takes the radio from a person or makes someone the control operator without them choosing it. JJ's answers to the station design's questions, 2026-09-24; for the Core's own computer he put it as "a tci program on the same computer as core/gui client should be able to tx?", and it can, while that window has transmit. | Taking transmit like the radio's PTT; taking it only when nobody holds it; on the Core's own computer, taking transmit like the radio's PTT. |
| D59 | **An older NereusSDR window is let in.** It works with its own slices; anything that would affect another device is refused with a plain "update NereusSDR"; if a take closes its only slice, it is disconnected with that reason. | The desktop updates separately from the Core and the phone, and the operator is never locked out (D23). | Refusing it until it is updated. |
| D60 | **While the holder is on the air, changes to the transmit path wait**: the amp, the tuner, an antenna, PureSignal, the interlock, the power limit, a change that would move or close the transmitting slice, and a Protocol 1 sample-rate change are refused with "<device> is on the air. Try again when they stop." Another device can still Unkey and take over, then make the change. | Switching relays or an amp under a carrier can damage equipment, and the holder is the control operator. Narrows D53 while on the air. | Asking, then changing it during the transmission. |
| D61 | **The receive antenna stays put on a band crossing** while another device listens through that antenna input, and the person tuning is told ("The antenna stays on ANT1 while the iPad listens on it."). The new band's transmit antenna still applies at key-down. | Tuning is never held up by a question, and nobody's reception changes under them. | Switching and telling the others afterwards; asking first. |
| D62 | **A device that drops keeps its place, its slices and transmit for 3 minutes**, unkeyed at once. After that its slices are saved for its return, and its place and transmit are freed; coming back to a full Core then brings the fifth-device question. | The phone's reconnect attempts fall inside it, and a phone left dead in a drawer doesn't hold a place or transmit for ever. JJ chose 3 minutes when offered two or no limit. | Two minutes (recommended); until it comes back. |
| D63 | **When nobody has transmit, a tap on PTT takes it and keys**, and every device shows who has it. Programs still never take it (D58). | Nobody is on the air to disturb, and it works the way operating the radio alone does today. | Asking "Take transmit?" first. |
| D64 | **Five narrower rules, kept together:** nobody can take the receiver of a slice someone is transmitting on; while the radio's own mic or footswitch is keyed, the slice it transmits on can't be retuned until the press ends; a fifth device can't take the place of the computer running the Core; sound is never cut when the Core runs short (the other devices' bands slow down, then pause with sound kept, until there is room); the desktop running the Core asks before taking transmit, like any device, while a mic plugged into the radio still takes it at once. | Each follows from JJ's calls or from transmit safety; the station's design for several devices lists them as confirmed. | |
| D65 | **The phone's name, as the Core lists it, is an editable field at pairing**, filled in with the phone's own name once Apple grants the user-assigned device name entitlement (JJ applies for it from his developer account), and with "iPhone" or "iPad" until then. The short name the Core shows on PTT and markers is the model ("iPhone", "iPad"), numbered by the Core when two collide. | Since iOS 16 an app reads the phone's own name only with that entitlement; nothing waits on Apple's answer. | An editable field with no request to Apple. |
| D66 | **An older NereusSDR window finding no room is turned away, saying why**: with four devices on, or no receiver free for its slice, it is refused, retryable, with "The Core is full. Update NereusSDR to take a device's place, or try again later." Nobody already connected is disturbed. | An older window can't show the fifth-device question (D55), and letting it past the limit would break D44. Narrows D55 and D59 for older windows. | Letting it in over the limit. |
| D67 | **Deleting and reinstalling the app keeps its pairings.** The phone keeps its device key and its list of paired Cores (their identity keys, labels and addresses) in the iPhone's secure storage (the Keychain), which survives deleting the app; after reinstalling it connects as before. | Otherwise the key would survive and the list would not, and a reinstalled phone would be stuck: the Core says it's already paired while the phone has forgotten the Core. It also keeps a Core from gaining a second entry for the same phone. | Starting fresh: a new key, pairing again as a new device, the old entry left on each Core until someone removes it. |

### 3.10 Decided while building

| # | Decision | Why | Rejected |
| --- | --- | --- | --- |
| D68 | **When headphones or AirPods disconnect, the band's sound pauses** with "Sound paused: your headphones disconnected. Tap to play on the speaker.", and plays on the speaker only after that tap. The band keeps showing. | Apple's convention for audio apps: the radio never suddenly plays out loud wherever the operator is. JJ, 2026-09-24; drawn on the band in picture 12 (board v54). | Keeping on playing through the speaker. |
| D69 | **Entering an address takes the address alone, with the port in a field of its own:** a name, an IPv4 address, or an IPv6 address with or without brackets, growing to a second line so a whole IPv6 address stays readable; below it the port, filled in with the Core's standard port (47910). Pasting an address with its port fills both fields. | Square brackets are awkward to type on a phone, and a port kept apart leaves a bare IPv6 address unambiguous. JJ, 2026-09-24, from the board's Enter an address screen. | Brackets and a typed port in one field, as the desktop reads an address. |
| D70 | **A Core that removed or forgot this phone stays in Your Cores with Pair**, in place of Connect, under a notice ("KG4VCF/attic removed this phone. Pair with it again to use it."); pairing again replaces what the phone kept for it. | Its address is already known, so pairing again goes straight to the code instead of a retyped address. JJ, 2026-09-24, from the board's pairing states. | Dropping the Core from the list. |
| D71 | **Under On this network, the phone lists every Core that takes a new device:** an unclaimed one (Pair, one tap where the Core allows it, or Use code), and one already paired with other devices that opened pairing for one more (Use code, with its address filled in); an unclaimed Core with pairing closed is listed greyed, saying so. A claimed Core with pairing closed isn't listed. | Adding a second device, an iPad after the iPhone, is the common case, and the address is already on the network. JJ, 2026-09-24. | Unclaimed Cores only. |
| D72 | **A phone that can't read its own key any more says so, and makes a new key when asked;** every Core then needs pairing again, and each Core keeps listing this phone's old entry until it is removed from Devices on another paired device or on the Core. | A Secure Enclave key doesn't survive an erase and restore, and a phone that silently fails every sign-in leaves the operator stuck. JJ, 2026-09-25, from board v55. | Asking each Core to drop the old entry at the next pairing. |
| D73 | **The toolbar's Pan 1 and Display each open a sheet under the toolbar.** Pan 1: the band grid (the Core's band list, 160 to 6 and WWV), which moves the pan's active slice to that band where it last was on it (its frequency, mode and filter, from the Core's own band memory); Add a slice here; Add a notch at A (the desktop's +TNF, placed by the Core); and Extended view. Display: this pan's look on this phone: the waterfall's palette and its levels (Clarity with Re-tune, Auto or Manual), the spectrum's fill, top and range, the Core's extras on the band (peak hold, peaks, the noise-floor line), and More display options in Setup. A control the Core can't do yet is greyed with "Needs a newer Core". | The two toolbar buttons opened nothing, and a control is never silently dead. They take the desktop's pan-level controls (its BAND grid, +RX, +TNF and the extended view) and the display settings each device keeps for itself. JJ, 2026-09-25, from board v56: "Keep, build before install". | Leaving the two buttons out of the listening build, or building them after the first install. |
| D74 | **Tuning on the band works as on the desktop:** a drag on the band moves the band (the view pans, the slices stay on their frequencies); a drag that starts on a flag or its passband tunes that slice, in its step; a tap on empty band still tunes the active slice there at once. Each slice's tuning step shows on its flag, and a tap lists the radio's steps. A tap on the flag's frequency opens a number pad to type one, in MHz or kHz. | JJ's first test on his iPhone (2026-09-25): a drag that always tuned made the band hard to move and a precise frequency almost impossible to reach. JJ approved the four drawings (picture 26) the same day: "build the tuning as drawn". | Every drag tuning the active slice (the first build's rule); the step only on the tuning dial. |
| D75 | **The spectrum trace's line width is set on the phone**, a Line slider on the Display sheet from one screen pixel (a hairline) to 3 points, starting at 0.5 points (the desktop's 1.5, and even 1, read heavy on a phone's screen). | JJ, 2026-09-25, from the device: "would be nice if the trace line on the spectrum wasn't so thick or configurable with a display slider", then "one may even be too thick". The desktop has the same setting (Line Width, 1 to 3 px). | A fixed width. |
| D76 | **A Core is renamed from Your Cores:** press and hold its row, Rename, and the name goes to the Core (`station.rename`), so every device shows it. The name keeps the Core's own form (a callsign, then optionally `/` and up to 32 letters, digits, `-` or `_`, as `KG4VCF/shack`); the Core refuses any other and says why. Until a Core has a name, its row shows its address. | JJ, 2026-09-25, from his first test: the Pi showed only its address. He chose the Core's own name, reached from Your Cores, over a name kept on one phone. | A name kept only on this phone; renaming only from Setup's Devices page. |
| D77 | **One tap on the speaker button opens a small Sound panel:** a Mute switch, then where the band plays (Speaker, Earpiece, AirPods when connected) with a tick on the current one. | Press and hold felt hidden (JJ, 2026-09-26: "long presses ... always feel hidden"); one visible place for everything about sound. | A tap mutes and press and hold opens the routes (the first build); the routes in the RX panel. |
| D78 | **Nothing depends on press and hold.** Every action reached by press and hold also has a visible way in: each Core row in Your Cores has a "⋯" button for its actions (Rename, and Remove when it arrives); each FreeDV Reporter row has an ⓘ details button; a spot's details on the band are also in the Spot List. Press and hold stays only as a shortcut. **Reworded by D114 (JJ, 2026-09-29):** gestures and long presses are fine as extra ways in; one that is the only way to an action needs JJ's explicit approval; anything visible on screen is not a hidden touch surface. | JJ, 2026-09-26: "long presses ... always feel hidden". | Press and hold as the only way to rename a Core or see a station's details. |
| D79 | **The band plan is the station's, drawn as the desktop draws it.** Setup's Display page (This phone and Core, laid out like the desktop's settings; JJ 2026-09-26 moved it out of the Display sheet, where it split the spectrum controls) has a Band plan group: a picker listing the Core's plans with a tick on the station's current one; picking one changes the station's plan (the desktop's Band Plan setting), so the desktop and every device follow, as a desktop remote window does. The same group has the plan's size, Off, Small, Medium, Large or Huge as on the desktop's View menu, kept on this phone and starting at Small. The strip is drawn like the desktop's: each segment's colour dimmed by its licence class and blended into the band's background, a thin separator at each segment's left edge, a bold label at the chosen size, the strip's height following the size, stopping at the dBm scale, and the plan's spot dots when the Core sends them. | JJ, 2026-09-26, from TestFlight: "the bandplan is not the same as the desktop, we want parity there and somewhere the ability to select the bandplan like in the desktop app or even disable the bandplan"; he chose the Display sheet for its controls and the whole station for the plan choice ("the whole station"). | A plan kept only on this phone; the strip on or off only; the phone's own drawing. |
| D80 | **MON plays your transmitted audio, as it sounds on the air, in this phone's headphones.** It follows the desktop control: MON toggles the Core's `monEnabled` setting and requests the Core's transmit monitor for this holder (`monitor-audio {route: headphones}`), at the Core's monitor volume. For a remote transmit holder, the Core's speakers stay quiet; monitor audio goes to the phone's headphones only. With no wired or Bluetooth headphones MON is greyed with its reason, so the loudspeaker cannot feed back into the microphone. A Core that does not send the monitor greys MON with its reason. The TX panel and the Modes tab behave the same. | JJ, 2026-09-26: "mon should play back in your headphone if no head phones greyed out may be best so you do not have feedback it is for hearing your txed audio as it sound on the air". JJ, 2026-09-27: settled that the phone follows the desktop `monEnabled` control while routing the remote monitor to headphones only; remote playback uses the Core/GUI session's parity Task 32 (`txMonitorAudioVersion` 1, media op `monitor-audio {route}`). | MON on the loudspeaker; omitting the desktop `monEnabled` control. |
| D81 | **A visible Match RX button sets the TX filter to the RX filter**, beside the TX filter in the Modes tab's Transmit section and in the TX panel: one tap sets the transmit passband to the active slice's receive filter, converted to audio frequencies as the desktop converts it (the LSB family flips the edges, the USB family keeps them, AM, SAM, DSB, FM and DRM use 0 to the filter's upper edge, |high|). It is a transmit setting: where the phone may not change transmit settings the button is greyed with the Core's reason. No press and hold or other shortcut. | JJ, 2026-09-26: the desktop's Shift+click on a filter preset matches TX to RX; on the phone he chose "a visible match rx". | A press and hold on a filter preset or on the passband. |
| D82 | **The split between the spectrum and the waterfall starts at the desktop's 40% spectrum and moves by dragging the frequency scale**, marked by a small ≡ at its left end, taking no extra height; the Display sheet's Spectrum height slider (20% to 80%) sets the same value, kept per pan. The dBm scale gets the desktop's ▲ and ▼ (10 dB a tap), and a drag on the scale shifts it smoothly. A drag elsewhere on the band still pans; a drag on a flag still tunes. | JJ, 2026-09-26: approved both drags, then chose the frequency-scale handle from the board ("yes i like the split from an icon on the left like you have") over a grip bar that took too much room. | A grip bar across the band; a fixed half-and-half split. |
| D83 | **The phone draws the band with the desktop's exact colours and sizes**: the trace colour and fill strength, the noise-floor line, the peak markers, peak hold and the grid's colours, even where the desktop took those values from AetherSDR or Thetis. D4 keeps their code out of the app; a colour, a width or a size is a value, not code, so the phone uses the same value and writes its own drawing code. | JJ, 2026-09-26: "Yes" to matching the desktop's colours and sizes. | The phone's own paler values. |
| D84 | **Sideways the band starts at 55% spectrum; upright it starts at the desktop's 40%.** Either can be moved as D82 says. **No latency added that can be avoided:** the phone sends its microphone at a steady 20 ms with no cushion of its own, the band player queues only what the audio callback needs, and any cushion at the Core adapts to measured jitter rather than sitting at a fixed size. | JJ, 2026-09-27: "Yes on 55%" (a sideways phone is too short for a flag and a spot row at 40%), and "we want to not add latency unnecessarily ideally removing latency from the path when we reasonably can". | 40% sideways with spots only as badges; a fixed 100 to 200 ms cushion. |
| D85 | **The Core's question before a change that affects another device's listening appears over the screen where the operator tapped**, on any tab or page (the Tuner Genius page, the TX panel, Modes, Setup, Radio, the band), never only on the band. It keeps asking (the Core's rule); one tap answers it there. The TX panel's tuner row carries the Tuner Genius's antenna buttons (ANT 1 to 3 with the operator's names), as the desktop's tuner controls do. | JJ, 2026-09-27, from TestFlight build 5: the question for a tuner antenna change sat on the band while he was on the Tuner Genius page ("make it so we can ack that from the same screen not hidden"); and the tuner's antennas belong on the TX panel. | The question only on the band; no question for accessory changes. |
| D86 | **The S-meter has the desktop's meter menu**: a visible ☰ on its title bar opens RX Mode (Signal, Sig Avg, Signal Peak, Max Bin), TX Mode (Power, SWR, Level, Compression), Peak Hold (on or off, Decay Fast 20, Medium 10 or Slow 5 dB/s, Reset) and Meter Face (Classic and the six vintage faces), wherever the analog S-meter shows (the iPad, and the iPhone where it draws one); a press and hold on the meter opens the same menu as a shortcut (approved). Choices are kept on this device. Signal, Sig Avg and Signal Peak use the active slice's Core readings. Max Bin is measured by the phone from that slice's own pan's displayed calibrated trace in the slice passband, before any visual notch dent; it is not a Core field and gets no extra calibration offset. If the pan has no valid display data, show no reading and disable Max Bin with a reason telling the operator to open that pan's display. TX modes use the current Core readings and are disabled with an explanatory reason when the Core lacks the required reading or version. The phone draws its own faces with the desktop's values (D83). | JJ, 2026-09-27: the iPad lane left the ☰ off; the desktop's right-click menu has four parts (PR #320 added the faces); "A long press and the menu button sounds good to me". | The ☰ left off; the meter fixed on Signal Peak. |
| D87 | **A phone connects by Core identity across changing routes:** direct/LAN/manual pairing remains available without rendezvous; initial direct and fresh rendezvous routes race before authentication, and only the first verified Core identity signs in. Existing-device code pairing completes the full SPAKE exchange while preserving and rechecking the existing device record. Later better-path switching, fallback and media replacement retain manual routes as backup; transient ICE addresses are not saved as WebSocket endpoints, and paths never switch while keyed or VOX is armed. An authenticated `controlChannelVersion = 0` is cached with its observation time for five minutes; stale, legacy untimestamped, rollback, or unparseable observations become unknown for connection discovery. Each real network-generation change invalidates the negative cache once; duplicate callbacks do not repeat invalidation or reset backoff. Once unknown, ordinary bounded rendezvous races and backoff retries remain eligible. Failed probes do not renew a negative result. Authenticated version 1 or full same-identity code pairing clears it. A cached relay-deny policy remains in force. | JJ, 2026-09-27: build 7 failed; "correct right away"; then approved "Yes, prioritize complete behavior" before remaining screens. Keep the complete Task 29a behavior; an initial race alone is not completion. | Re-pairing a known key, stale private routes delaying rendezvous, identity inferred from an error or mailbox, or treating the first race as complete path management. |
| D88 | **Stop cannot sustain transmission or act on a replacement connection.** At local off intent the phone suppresses every heartbeat path until each pending off has its own matching accepted Core result. Failed or refused off stays suppressed until logical-session reset, and new on intents receive a visible reconnect reason. Command IDs and send returns are not delivery proof. Ordinary queued commands, copies and late key results stay bound to the logical session that admitted them; OLD work cannot key, unkey or clear safety state in NEW. Valid route upgrades within one logical session remain supported. The existing 100 ms heartbeat cadence and 400 ms Core watchdog are unchanged. | JJ, 2026-09-28: "yes fix those bugs", approving the reproduced transmit-release and stale-command repairs. JJ later uploaded the earlier build 8 archive while repair was in progress; the independently reviewed correction was uploaded in build 9 at 07:39 CDT on September 28. | Independent keepalives continuing while off is unresolved; clearing a failed off with an unrelated result; old queued commands or compensation reaching a replacement session. |
| D89 | **All desktop Network Diagnostics features belong in Tools, adapted for the phone.** A visible **Connection and performance** entry includes all four remote diagnostics tabs, every chart and detailed reading, history ranges and series selection, plus the local desktop window's additional readings and session-stat reset. It also shows active control and media routes, actual selected addresses and IPv4/IPv6 families, transport and direct/relay status, and how rendezvous contributed. Saved or advertised addresses are not presented as the selected peer. Preserve measurement sources, units and meaning, with explicit unavailable/stale states and visible controls. The approved September 28 board stacks charts vertically in portrait and uses two columns in landscape where space permits. Tools and the existing dot/ms control open this same destination. The existing licences/build page becomes **About this app**, separate from live diagnostics. | JJ, 2026-09-28, reports RV working in build 8, asks for actual IP/IPv6 details and desktop charts, prefers Tools, then explicitly selects **All Network Diagnostics features**. This covers Network Diagnostics, not every desktop Tools feature as a new addition; existing Tools scope remains. | A hidden tap on Direct, only porting two charts, one ambiguous address for different control/media routes, duplicate diagnostics pages, or invented readings. |
| D90 | **About this app carries all the desktop About information, accurately adapted to the phone.** Include project history, the full credited contributor roster, copyright and licence information, no-warranty notice, AI-authoring disclosure, upstream/project/community/protocol links and releases. Identify the phone with its actual app version, build and source tag, and list its actual bundled libraries and notices. Explain desktop/Core lineage and dependencies separately rather than claiming the phone embeds them. Preserve the phone's own licence and App Store permission clause, and include Corresponding Source. | JJ, 2026-09-28: "under the about page it should also bring in all the about info from thedesktop app too". | Merely renaming Diagnostics, omitting credits, presenting desktop build/library versions as phone versions, or replacing the phone's licence with the desktop's terms. |

| D91 | **Remove Core disconnects first, then removes the saved entry from this phone.** Reach it through the visible actions button in Your Cores or the Core section of Radio while connected. One Remove action cancels any connection or reconnect for that Core, completes the normal safe disconnect, then deletes its saved identity, addresses and per-Core connection hints. It does not revoke this phone on the Core or affect other devices. A failed local save leaves the entry visible with an explanation; no automatic retry or late callback restores it. Existing transmit safety gates remain in force. | JJ, 2026-09-28: "remove should disconnect then remove". | Requiring a separate Disconnect tap first, forgetting a still-active connection, a hidden gesture, or treating local removal as Core-side device revocation. |

### 3.11 Decided on the night of 2026-09-28

JJ's rulings for the phone, relayed by the Core/GUI lead that night (items 1 to 8 of
`rulings-2026-09-28-night-for-phone.md` in the phone crew folder), and his answers to the
phone controller the same day. Where a decision waits on the Core, the Core/GUI session's
note for the phone names the wire; the plan marks each part that waits.

| # | Decision | Why | Rejected |
| --- | --- | --- | --- |
| D92 | **The TX EQ curve can be edited from the phone, including while transmitting.** The TX Equalizer shows the Core's parametric curve (`txEqCurve`) drawn as the Core sends it, never reordered, says which EQ is on the air (the parametric curve or the ten-band legacy EQ), and says "unavailable" instead of drawing a flat line when the Core cannot read the saved curve. An edit sends the whole curve through the Core's `txEq.setCurve`, which rounds, orders and stores it as a desktop edit does; the active TX profile then shows as changed, and saving it stays with the profile's Save. Edits follow the same permission rules as the other transmit settings: allowed on or off the air where the phone may change transmit settings, greyed with the Core's reason where it may not, and an edit never keys. The page is drawn as a board section first and built after JJ approves it, as every new screen is (plan Part J). | JJ, 2026-09-28 night (ruling 3): "build it", allowed while transmitting like the desktop, same permission rules as other transmit settings. The read-only curve is `txeq-curve-for-phone.md`; the write verb is the proposal in the Core/GUI lead's TX EQ curve report, and its wire note follows. | Read-only on the phone; the phone writing the gzip-wrapped `txEqParaEqData` itself; refusing edits while keyed. |
| D93 | **(Deferred by D107; not built.)** **The phone saves a settings backup holding the Core's settings and this phone's settings in one file**, the combined format the desktop remote window writes (`SettingsBackup`, named `NereusSDR.nereus-settings`): the Core's part from the Core's settings export, and this phone's settings in the part where the desktop remote window puts its own. It is saved through the Share sheet, wherever the operator chooses. Pairing keys are never in it: not this phone's device key, not its saved Cores' identity keys, not the Core's identity key or pairing records. The page is drawn as a board section first and built after JJ approves it. | JJ, 2026-09-28 night (ruling 7b): "BUILD a phone settings backup", in the same combined file format, pairing keys never included. The Core's export already exists (`settingsBackupVersion` 1). | A file format of the phone's own; phone settings only; keys in the file so a restore pairs again. |
| D94 | **(Deferred by D107; not built.)** **Restoring a backup from the phone asks first and then lets the Core apply it.** The operator opens a backup file through the Share sheet. Before anything changes, the phone names the other devices connected to the Core and asks for confirmation; the Core then applies its part with a radio reconnect, the other devices reconnect by themselves, and this phone applies its own part. A restore is refused while any device transmits, and the phone shows the Core's reason as sent. | JJ, 2026-09-28 night (ruling 4): name the other connected devices, confirm, the Core applies with a radio reconnect, other devices reconnect automatically, refused while anyone transmits. The Core's restore wire note follows. | A restore that does not name the other devices; one that interrupts a transmission; applying the Core's part without a radio reconnect. |
| D95 | **Setup's Diagnostics has a Logs page for the Core's log as it happens:** the Core's live log stream (`coreLog`) with switches for the Core's log categories, labelled with the Core's own labels. Clear clears the view on this phone only and never the Core's log. The phone's own log is not on this page; it stays in the Support Bundle. The Tools parity lane builds the page with a category list held on the phone behind a seam until the Core sends its labels (`log-categories-for-phone.md` names the wire). | JJ, 2026-09-28 night (ruling 7c): "BUILD a phone Logs page", Clear clears the view only, the phone's own log stays in the support bundle. | The phone's own log on this page; Clear erasing the Core's log; category names chosen by the phone once the Core sends its own. |
| D96 | **The phone runs no TCI server of its own.** Where the desktop shows its own local TCI server's settings, the phone shows them disabled with a plain reason in the style of "This runs on the desktop computer.". The Core publishes all its own TCI server's settings (IQ stream, audio block, TX channel, sensor intervals, VFO quirks, CW above 10 MHz and the rest) for the phone and the remote windows; the phone shows and edits them once that wire note lands. | JJ, 2026-09-28 night (ruling 7a). | A TCI server on the phone; hiding the local server's settings. |
| D97 | **Reset to Smooth Defaults on the phone applies a spectrum average time of 650 ms**, as the Core describes it: the March tuning is kept exactly, and spectrum averaging becomes a stored average time of 650 ms (alpha 0.05 at 30 frames a second) with no threshold gap. If the Core's description changes the setting's wire name, a later note names it. | JJ, 2026-09-28 night (ruling 2); the Core and desktop fix is queued. | An averaging value of the phone's own. |
| D98 | **Diagnostics' "Reset to defaults" becomes "Repair invalid settings", and works from the phone.** It keeps the same repair behaviour and runs on a remote Core, where the phone showed Reset to defaults disabled with "Reset to defaults is not available on this Core.". The phone shows the action with the Core's label wherever it shows it. | JJ, 2026-09-28 night (ruling 5). | Keeping Reset to defaults, disabled, on a remote Core. |
| D99 | **Setup > Display's defaults stay NereusSDR's own:** the zero line, the grid's dB step, the grid's noise-floor offset, Normalize, the waterfall's high and low levels and the signal history. The phone's defaults do not change. | JJ, 2026-09-28 night (ruling 1). | Changing any of these defaults. |
| D100 | **Audio Reset turns headphones off**, as it already does. No phone change. | JJ, 2026-09-28 night (ruling 6). | Leaving headphones on after an Audio Reset. |
| D101 | **"Waiting for a radio"** is accepted as the wording on the desktop's LAN row, matching the phone. No phone change. | JJ, 2026-09-28 night (ruling 8). | Different wording on the desktop's row. |
| D102 | **The AM Mod Monitor is built as the board draws it**, on the TX panel only (not the Modes tab), below the panel's usual controls. Its settings open in a sheet from the monitor's Settings button and from nowhere else; the sheet edits the Core's PA feedback receiver, which every device shares, and marks it shared; the sheet has the Bars or Meters style switch. | JJ, 2026-09-28, approving the AM Mod Monitor board section as drawn with these five choices. | The monitor on the Modes tab too; its settings on the TX panel itself; the feedback receiver shown as if it were this phone's own. |
| D103 | **The App Store encryption question is answered YES:** the app uses standard encryption in addition to what iOS provides (TLS for control, DTLS and SRTP for media, the pairing exchange, the device keys). | JJ to the phone controller, 2026-09-28. `ios/AppStore/export-compliance.md` already says so; Task 69 records it. | Answering that the app uses only the encryption built into iOS. |

### 3.12 Decided from 2026-09-28 to 2026-09-29

JJ's later rulings for the phone, taken from the phone controller's ledger. Items marked
"board approved" were drawn as a board section first, as Part J requires. Where a decision
waits on the Core, the Core/GUI session's note for the phone names the wire.

| # | Decision | Why | Rejected |
| --- | --- | --- | --- |
| D104 | **The transmit stage meters are built as the board draws them:** a strip under Mic level and above PROC, VOX and MON on the TX panel; the peak mark kept; dimmed last readings off the air with one line; on an older Core that does not send the readings, the single line "This Core does not send these readings. Updating the Core may help."; short CFC labels; the bars kept in large type. | JJ, 2026-09-28, approving the stage meter board section as drawn. | A strip elsewhere on the panel; no peak mark; blank meters off the air. |
| D105 | **Mic mute matches the desktop:** the phone has no mute button. Mic level greys with a plain reason while the Core's mic is muted (`transmit.micMuted`, from `transmitSettingsVersion` 10). | JJ, 2026-09-28: mic mute matches the desktop. | A mute button on the phone. |
| D106 | **Your Cores shows "Waiting for a radio"** as the row's subtitle, after the address and the device count. It states the condition only and names no radio. | JJ, 2026-09-28, answering whether the phone should show what the desktop's LAN row shows. | Leaving the state off the phone's row; a radio name in the subtitle. |
| D107 | **Settings backup and restore on the phone is deferred** (amends D93 and D94). JJ: "way too complex"; come back to it later. Task 58a is not built, the backup and restore board section stays a draft, and R-IOS-35 waits with it. The Core was told. D133 moves it to a later release and keeps it on the roadmap. | JJ, 2026-09-28. | Building Back up alone first; building it now. |
| D108 | **The RADE row on the VFO flag follows the Core's note where it differs from the board.** Six differences: (1) the older-Core reason reads "This Core does not send RADE sync. Updating the Core may help." (the board said "RADE reception"); (2) the older-Core row's prefix is the decoded callsign, else "RADE"; (3) locked on with no SNR number shows the hollow dot and "---" (the board drew the filled dot); (4) the offset sign comes from the uncut value, so -0.4 Hz shows "0Hz"; (5) the row's text uses the note's spaced form, `K1ABC ● 12dB +38Hz`; (6) a synced row from a Core that sends the gate always shows the offset ("+0Hz" until the first report), and a synced row with no offset draws without it. | JJ, on the RADE flag report's section 3: "Yes", follow the Core's note. | The board's drawing where it differs from the Core's note. |
| D109 | **Taking over a slice takes it as it is:** every setting kept, no slice made or removed. The device that had it is told ("<device> took control of slice <letter>. You are still listening.") and can take it back. | JJ, 2026-09-29. | Closing and remaking the slice; resetting its settings. |
| D110 | **The take-over screens are approved as drawn, with all eight of the board report's recommendations, minus the listen-only disabled state.** The Core has no listen-only control tier (receive-only refuses only the key and transmit requests), so Take control stays live on a receive-only connection and shows the Core's refusal in its own words if refused; the placeholder listen-only words are dropped. The first-key notice follows the Core's rule: the taker's mark is set on the take, cleared by choosing a transmit slice, by the slice closing or by control moving; it shows only on a key from that device that would land on the taken slice while the device owns no other slice that can transmit; transmit never jumps. (The prompt and refusal parts are amended by D123.) | JJ, 2026-09-29, approving the board; the Core lead's answers to the board's open questions the same day. | A disabled Take control on receive-only; phone-worded listen-only text; transmit moving to the taken slice. |
| D111 | **The direct media path falls back after 5000 ms with no packets.** | JJ, 2026-09-29 (he had answered it earlier; the Core's `kDirectMediaSilenceFallbackMs` is 5000). | 3000 ms. |
| D112 | **Alex low-pass filters on the phone match the desktop:** one dot per row, filled for the filter in use, outline otherwise, display only, spoken as "In use"; no dot when `alexLpfBits` is -1 or absent. | JJ, 2026-09-29. | Filters shown without a dot; a dot that can be tapped. |
| D113 | **The phone's flag icons become real buttons, as on the desktop, and the active flag is finger-sized all the time:** two rows, the antenna controls merged, the tab panels full width. The look was chosen on 2026-09-30 (D118). | JJ, 2026-09-29. | Flag icons that are not buttons; a small flag that grows only while touched. |
| D114 | **The touch rule (amends D78):** gestures and long presses are fine as extra ways in. A gesture or long press that is the only way to an action needs JJ's explicit approval. Anything visible on screen is not a hidden touch surface and needs no button chrome. | JJ, 2026-09-29. | Forbidding gestures outright; treating a visible icon as hidden. |
| D115 | **A slice another device controls is worded as the Core words it:** "That slice belongs to <holder>. It can be changed only there." (older windows); "Slice <letter> is controlled by <holder>. Take control to change it." (listening); "Nobody controls slice <letter>. Take control to change it." The holder words are the device's own name as-is, "the Core" for an empty device or the station device, "a <kind>" (lowercase) when it has no name, and "another device" when neither is known. This replaces "Only <owner> can tune it or close it." (The "needs an update" refusal was dropped by D123.) | Follows the Core lead's owner words (`owner-words-from-core.md`, 2026-09-29); JJ's D109 and D110. | The phone's own owner wording. |
| D116 | **The phone declares Setup description 17.** Version 18 and above wait on JJ's decision about the HL2 clock rows. (Superseded by D117, 2026-09-30.) | JJ, 2026-09-29: the phone stays at 17 until that decision. | Declaring 18 or 19 before the decision. |

### 3.13 Decided on 2026-09-30 and 2026-10-01

JJ's rulings for the phone on 2026-09-30, taken from the phone controller's ledger. Items
marked "board approved" were drawn as a board section first, as Part J requires. Where a
decision waits on the Core, the Core/GUI session's note for the phone names the wire.

| # | Decision | Why | Rejected |
| --- | --- | --- | --- |
| D117 | **The phone moves to the latest Setup description the Core offers, with the HL2 clock rows live as on the desktop** (supersedes D116). The phone builds each new version as it lands: 22 first, then the later versions the Core added the same day. | JJ, 2026-09-30, in this session, confirming the Core lead's relay: "your latest version". The earlier hold at 17 (D116) waited on this decision. | Staying at 17; stopping at 19. |
| D118 | **The flag buttons take look 2** (completes D113): plain words, thin dividers, the open tab underlined in the slice colour, bare side icons. The flag is then redrawn in the desktop's row order: the meter on its own row, the step control next to X and RIT, one fixed layout with the tab type capped at 14 points (at least 6.2 points of clearance at the cap), and approved as drawn on the board. | JJ, 2026-09-30: look 2, then approval of the redraw ("approved the redrawn flag as drawn"). | The other round-2 looks; a layout that changes with the type size. |
| D119 | **The flag board's remaining recommendations are accepted:** the flag folds sideways at about 13.3 kHz of band width; the iPad folds flags at the same threshold as the phone (242 points, 246 in large type), so nothing changes there; Close on slice A is greyed with its reason; the placeholder sentences are "This radio has no BYPS switch." and "Slice A always stays open."; receive-only transmit uses the phone's existing listen-only transmit presentation, with no new sentence; VAX has its own panel. | JJ, 2026-09-30, accepting the board's recommendations; the iPad threshold in answer to a separate question the same day. | A different iPad threshold; a new receive-only sentence; VAX inside another panel. |
| D120 | **The slice list, the jump and the slim flag are approved as drawn (board approved).** The Slice button opens a list of every slice instead of stepping (a tap on a flag still makes that slice active); rows stay in letter order and are never regrouped; the pills read "This pan" and "Another pan". Listen on a slice in another pan jumps to that slice's band and shows its flag with one tap back; a listened slice is always visible (its flag in view, an edge marker otherwise, and its list row), and the edge marker and list row count as showing it. The controls follow the band you look at: a jump to A makes A the active slice (Slice button, Modes, RX panel and meter show A, greyed while you only listen, with Take control there and your own volume and Stop listening live), and Back makes your own slice active again. Pressing PTT while viewing a listened slice's band switches back to the transmit slice's band and stays there; the marker for A returns in one tap. Listening continues after going back to your own band until Stop listening, Release or the slice closing. The list rows keep their volume, the AF Gain label is kept, and the hosting desktop gets the took-control notice with Take it back. | JJ, 2026-09-30: "board look good", with his rulings on the list, the jump and listening the same day (answering the open questions of the slice list and jump reports). | An edge tag instead of a jump; stepping through slices; rows grouped by pan; listening that ends when the band changes. |
| D121 | **On a listening flag, Take control is one slim row, and the volume lives in the speaker tab.** The owner block is a single header-height row (44 points) with a compact outline Take control. AF Gain, Mute and Stop listening sit in the existing speaker tab panel, not a new row. The Modes page block is unchanged. | JJ, 2026-09-30: Take control was too big and broke the flag's flow; volume belongs in the existing speaker tab, not a new row. | A tall owner block; a new volume row on the flag. |
| D122 | **After Take control, a tap on the band tunes the taken slice** like any slice you own. | JJ, 2026-09-30. | A taken slice that ignores band taps. |
| D123 | **Take control never prompts anyone, the desktop included** (amends D110 and D115): it takes at once and the former holder stays a listener. Every slice can be taken; the only refusal kept is "transmitting, take once it stops", and the "needs an update" refusal is dropped. A device may take the Core's own slice when nobody is at the Core's desktop; the desktop-asked frames are gone. When the take came from the hosting desktop, the listener is named by that desktop's own device name, not "the Core" (the phone finds it as the connected device that hosts the Core and never matches by id). On an older Core that answers below `sliceAccess` 3, the Core's own slice on a headless Core stays greyed with "Slice <letter> is run by the Core itself, so control of it cannot pass to this device." | JJ, 2026-09-30 (three rulings: a device may take the Core's own slice with nobody at the desktop; every slice up for grabs; no prompt for anyone), the Core lead's `sliceAccess` 3 shape (`takeover-final-shape-from-core.md` and the same-day notes). | Asking the desktop; the phrase "the Core" for a desktop-hosted take; a separate refusal for older Cores. |
| D124 | **Take control is greyed ahead of time while the slice is on the air** (the Core's `access:<id>.onAir`), with the Core's words under it ("Slice <letter> is transmitting. Take control once it stops."), and live again when the transmission ends. | JJ, 2026-09-30: yes, grey it ahead of time. | Leaving it live and refusing after the tap. |
| D125 | **If the Core refuses to serve a listened slice's band display** (no capacity, or the receiver is not on the Core), the phone stays on its own band, shows the Core's words, and keeps hearing the slice with the edge marker. | JJ, 2026-09-30. | Jumping anyway; dropping the listening. |
| D126 | **Slices on one input share the radio's filters, and the phone shows the Core's words** (ruling (c) and (d)). One counted set of slices and one away rule cover both filters; the receive low-pass follows the highest slice on the input as the desktop does; the band-pass bypass is kept. Where the desktop would say why a filter is off or wide, the phone shows the Core's reason in the flag's Filter policy menu (the filter reason and the low-pass reason, the low-pass reason possibly alone). The WIDE chip on the band was drawn for review and is approved as drawn (D136). Using the radio's second receiver input for a slice on another band waits for a separate feed on that input (D131). | JJ, 2026-09-30: option (c) and (d) of the shared-filter options report, which the bench confirmed the same day (slice A on 80 m darkened the phone's 20 m; A on 20 m was normal). | Option (b); giving the phone its own filter rule. |
| D127 | **While another slice's band is shown (a jump), the split between the spectrum and the waterfall moves down by just the points the flag needs to clear the frequency scale.** The moved split is never saved and returns to its saved place on Back. | JJ, 2026-09-30: option (a) of the jumped-flag question for small phones. | Shrinking the flag; saving the moved split. |
| D128 | **Mic Gain sits in the TX panel directly under the mic level meter,** as on the desktop's Phone/CW applet. The Modes row stays. It greys with the Core's words (for example "The Core's mic is muted.") while the Core's mic is muted. | JJ, 2026-09-30. | Mic Gain only in Modes. |
| D129 | **The TX badge on a flag starts the take-over,** so one tap can end with that flag transmitting. In order, per the Core's signals (`txbadge-core-signals.md`, Core trunk 01d797e56): (1) my slice and I hold transmit: the slice becomes the transmit slice; (2) my slice and another device holds transmit: ask once, "Take transmit from <device>?", take transmit, then make the slice the transmit slice (with nobody holding, take transmit at once); (3) another device's slice: take control of the slice first, wait for the holder to change (about 1 s) before deciding whether to ask, then case 2. The slice step comes only after this device sees itself as the holder; each reply is matched by its command id; a failed take, a cancel or a link drop ends the badge take with nothing pending. Nothing keys: the badge only takes. Refusals are unchanged (a slice on the air; the radio transmitting on that frequency), and a badge that cannot start a take is disabled with the Core's reason, never asked. | JJ, 2026-09-30, relayed by the Core lead (phone parity with the desktop badge). | Keying on the badge; a take that sends a stale slice change after a later grant. |
| D130 | **The Level Cal preamp line reads "Preamp: none on this receiver input."** "Level calibration is running." stays as it is. | JJ, 2026-09-30. | A longer preamp sentence. |
| D131 | **The RX2 attenuator sits in the Step att row of the front end,** keyed to the Core's RX2 slice mask so it follows the real receiver input, and written to the Core's `rx2AttenuationDb` (0 to 31 dB; above that the Core refuses with "RX2's attenuator goes from 0 to 31 dB."). The RX2 preamp list comes from the Core's catalog (the HPSDR radios only) and is greyed with the Core's reason on a radio without one; the phone keeps no table of its own. On the ANAN G2 the RX2 input (ADC1) is not on the antenna switch: ANT1 TX/RX feeds ADC0 only (JJ, from the codec comment and the G2 results, row 15), so a separate feed on the RX2 jack is what would make a second input useful. | JJ, 2026-09-30 (placement on Modes, Front end; the Core's catalog rule; the G2 fact). | A phone-side RX2 preamp table; an RX2 attenuator outside the Step att row. |
| D132 | **The build number is the commit count.** `CFBundleVersion` is set from the number of commits (`ios/scripts/build-number.sh`) in the device install and in archives, so About no longer shows build 1. TestFlight archives are made from phone main (`claude/iphone-app`) only. | JJ, 2026-09-30, after About showed build 1. | A build number fixed at 1; archives from lane branches. |
| D133 | **Settings backup and restore (Task 58a) is out of this release and stays on the roadmap** (restates D107 as a release call). It is deferred to a later release: not built now, the board section stays a draft, R-IOS-35 waits, and the project roadmap (`docs/MASTER-PLAN.md`, the deferred features table) lists it so it is not lost. | JJ, 2026-09-30: "Option 1 but keep on roadmap". | Building Back up alone first; dropping it from the roadmap. |
| D134 | **The website's privacy page (`iphone-privacy.html`) and source-code page (`iphone-source.html`) go live when the phone's pull request merges to main,** deployed from main with `website/deploy.sh`, not before. The pull request text carries this as a checklist item (plan Task 69). | JJ, 2026-09-30. | Deploying the pages ahead of the merge; deploying from a lane branch. |
| D135 | **Taking transmit from the TX panel matches the flag's TX badge (D129):** when nobody holds transmit, the tap takes it at once; when another device holds it, the usual ask ("Take transmit from <device>?") comes first. No code change was needed: the panel's take already does this. | JJ, 2026-09-30, closing the open question on the TX panel take. | A different rule in the panel than on the flag. |
| D136 | **The WIDE chip is approved as drawn** (JJ: "Yes wide chip looks correct") and is built: amber, on the band, shown only while the first receiver input is not Filtered, a tap opens the flag's more menu with the Core's reasons. It moves below the frames-per-second readout when that readout shows. This replaces the "pending board review" record of D126. | JJ, 2026-09-30, on the WIDE chip board. | Leaving the chip over the readout; no chip. |
| D137 | **When headphones go away while MON is on, the phone sends `monitor-audio {route: none}` and also writes `monEnabled` off,** because MON shows the Core's `monEnabled`; leaving it on would show MON on while nothing is sent. This is the controller's implementation call on Task 55b's consistency question, for JJ to confirm at the pull request review. | The phone controller, 2026-10-01 (not yet confirmed by JJ). | Sending the route change without writing `monEnabled` off. |
| D138 | **Take transmit greys while a question about the take is on screen,** so a second tap cannot start a second take under the first. It is live again when the question is answered or dismissed. | Controller call, for JJ to confirm at the pull request review: the phone controller's cleanup lane, 2026-10-01 (not yet confirmed by JJ). | Leaving the control live under the question. |
| D139 | **Tests may keep one private accessibility call,** because no public replacement exists. It lives in the test target only, is isolated in one place and restored after each use, and never ships in the app. | Controller call, for JJ to confirm at the pull request review: the phone controller's ruling on the cleanup lane, 2026-10-01 (not yet confirmed by JJ). | A public API that does not cover the case; no such check. |
| D140 | **Core settings respond at the touch in every tab, panel, flag and Setup.** The phone shows the operator's chosen value while that edit is unanswered; a newer edit replaces an older pending one. The matching Core answer settles the edit, and a refusal restores the Core's value with its reason beside the control. After five seconds without confirmation, the responsible control snaps back to the latest Core value and retains an unconfirmed notice. An older timeout or reply cannot overwrite a newer current-session adjustment. A current submitted typed entry follows the rollback; unsubmitted or newer input remains an editable draft. A lost or replaced session discards unanswered edits and restores the last Core values; it never re-sends automatically. The common lost-link wording is "The connection to the Core dropped before it confirmed this change.", including Setup. Tools sliders send at most once every 50 milliseconds while dragging, with the final value on release. Disconnected command controls say "This app is not connected to the Core, so nothing was changed." PTT, TUNE, MOX, 2-Tone, VOX arm and take keep their ordered keying path. | JJ, 2026-10-01: ordinary controls must work in real time; the explicit timeout follow-up is "what no i do not want drift, snapping back is better than drift". JJ approved common lost-link wording, drag sends and the disconnected message. | Keeping an unconfirmed value after the deadline; overwriting a newer adjustment with an older result; replaying unanswered edits after reconnecting. |
| D141 | **Saver and Audio only keep the chosen audio quality.** They reduce display traffic; they do not silently cap High, Save data or Lossless. Lossless stays available for digital modes. The cellular choices and audio-quality page warn that Lossless can use about 720 MB an hour while carried. Costs remain estimates until measured, and microphone cost depends on its actually negotiated format. | JJ, 2026-10-01: "Keep audio quality with a warning". | Silently changing audio quality when a display mode changes; presenting High's estimate as Lossless's cost. |
| D142 | **Diversity belongs to one live slice and can move on a Core offering movable Diversity.** Its DIV badge sits under that flag's filter width, leaving TX finger-sized; listening devices see the badge even without joining the slice. Tapping it opens that slice's Diversity page. The larger menu title matches its neighbours. The flag's More menu and the page keep their switches; the page's On/Off stays beside Use, and no DSP-panel button is added. A move requires control of both source and target slices and uses one guarded Core command. The common notice is "Diversity moved from B to C. Both slices paused briefly." with the actual slice letters; bench timing remains unmeasured. Closing its slice turns Diversity off without moving it; the blend goes with that slice and saved memories remain. After a Core restart, restore the same slice's phase and gain if the slice is restored, otherwise leave Diversity off. Keep Diversity running during PureSignal where supported; otherwise pause with the Core's reason. Summary, blend and memory controls remain read-only for a listener without control. Legacy Cores keep their supported slice-A behavior and disable moves with a reason. | JJ, 2026-10-01: approved layout with larger menu text and all nine recorded behavior choices. | Automatic moves on close; interrupting another operator's slice without control; hiding the live owner from listeners; an additional DSP button. |
| D143 | **The reviewed TX drawer keeps a pinned header and key controls, then RF, configured amp/tuner/antenna controls, Audio, Voice and Processing.** Settings opens Modes, Transmit; Back preserves the open drawer and scroll position. TUNE, MOX, 2-Tone and PS-A stay reachable. RF keeps RF/SWR meters and RF/Tune power. Audio keeps Mic level, Mic Gain and all transmit-stage meters with their peaks. Voice provides quick VOX level and AntiVOX gain, with detailed timing below. Processing keeps PROC, LEV, EQ, CFC, DEXP and MON, followed by the profile. Configured but unavailable controls show their reasons; unconfigured optional equipment is omitted. Existing AM carrier and Mod Monitor remain. High SWR uses a conditional text pill or onset toast only while a fresh transmitting reading exceeds the known limit; it does not claim that the Core cut power. Detailed settings remain in Modes, with optional long-press routes as shortcuts. Full and folded DIV routes, TX, Settings and other interactive controls require actual 44-point targets and viewport containment. | JJ, 2026-10-01: approved TX board after restoring audio readings and ordering pinned, RF, amp/tuner/antenna, Audio, Voice and Processing. | A permanent SWR icon or explanation; removing useful audio levels; obscuring pinned controls after navigation. |

---

## 4. Architecture

### 4.1 What runs where

| Job | Where |
| --- | --- |
| Radio protocol (P1 and P2), DSP, demodulation, the TX chain, PureSignal, meters | Station |
| FFT and display reduction for every panadapter the phone shows | Station, sent as display frames per endpoint |
| The audio mix (one 48 kHz stereo master) and its Opus encoding | Station (R-R3-06) |
| Audio playback and routing (speaker, earpiece, AirPods) | Phone |
| Microphone capture and its Opus uplink | Phone (R4); the station's PROC, EQ and leveler shape it |
| Amp and tuner connections (Power Genius XL, Tuner Genius XL, RF2K-S) | Station (R-R3-22); the phone sends intent |
| DX cluster, RBN, POTA, PSK Reporter, FreeDV Reporter | Station (remote design §6.4); how they look is set on each phone |
| WSJT-X | Wherever WSJT-X runs |
| TX interlock, PA protection, SWR gating, the watchdog, the time-out | Station, always authoritative |
| Drawing the band and flags, gestures, haptics, the lock-screen card, Apple's Push to Talk | Phone |
| Settings | Station, This phone or Both, per remote design §6.3 and the Setup tags (D16) |

### 4.2 The link, and the station phases the release needs

The phone uses the station's link as the desktop does in remote mode:

* **R2:** the TLS WebSocket control session, the state mirror, the settings
  proxy, and the connect sequence of remote design §7.0 (hello with versions,
  authentication, capabilities, snapshot, snapshot-complete, then the TX gate).
* **R3:** display endpoints the client sizes itself (media control: 1 to 4096
  pixels, 1 to 60 frames a second, per endpoint) and audio. Opus receive audio
  uses 48 kHz stereo, 40 ms frames and constrained VBR. The release's High,
  Save data and negotiated Lossless choices are specified in §5.4 item 9;
  the original 24 kbit/s R3 profile is not the release's default.
* **R4:** transmit and its safety harness: the microphone uplink, the
  unkey-confirmed handoff gate, the TX watchdog (§12.1), uplink starvation
  (§12.3), the PTT time-out, and TX disabled until the snapshot is complete
  (§12.2).
* **R5:** the transport ladder: rendezvous, direct first and relay last, and
  manual address entry (pairing design §5 and §8).
* **R6:** operations: reachability diagnostics, connection UX and packaging.
  The R3 plan (§4d) leaves full pairing administration to R6.

Per D5, the first store release needs R3, R4 and R5, plus the station-side work
in §6.1.

### 4.3 A written, versioned link

The phone is the first client not built from the NereusSDR C++ tree, so it
cannot share the wire code. The link needs a written specification (framing,
handshake, snapshot, every message, capabilities, version rules) and a
conformance suite that the station and the app both run (R-IOS-01). The R2 wire
is JSON over wss today and R3 adds compact media codecs; both are covered.

### 4.4 Versions (D23)

Remote design §7.0: both ends advertise `major.minor`; a differing major is
refused with a message naming both versions; a differing minor negotiates down.
This spec amends it for the phone:

* **Every app release implements the station's current major and the one before
  it.** A station one major behind is served through the older link, and new
  features stay greyed "Needs a newer Core".
* **Only a station two or more majors behind is refused**, and the refusal
  names both versions ("The Core needs updating").
* Within a major, the capability descriptor gates each feature; the phone greys
  what the station does not advertise.

The station follows the same rule the other way (D39): it serves a client one
major behind as well as its own, and refuses only a client two or more majors
apart, naming both versions. Both ends advertise the majors they support and
agree on the highest they share. The conformance suite runs at every major
either end supports (R-IOS-01, R-IOS-16).

### 4.5 Several devices at once (§3.9)

Remote design §7.1 has one operator and one session, and a second authenticated
connection preempts the first. §3.9 replaces that: up to four paired devices hold
sessions on one Core at the same time. The station half (the session model, who
owns which slice, pan and receiver, the arbitration of transmit, receivers and
shared settings, and the admission rules that replace link §12.4) is
[2026-09-24-several-devices-on-one-core-design.md](2026-09-24-several-devices-on-one-core-design.md).
What the phone needs from it:

1. **Who is on the Core:** every connected device's name, how long it has been
   connected, when it was last active, and what it is doing (listening on which
   slices, or transmitting, with the TX clock). The phone shows it on Setup,
   Devices (D57) and in a fifth device's list (D55). Only paired devices ever see
   it, because it comes after authentication.
2. **Whose each slice is:** every slice carries its owning device and its letter
   from the Core's one pool (D47). The phone draws its own slices with flags and
   the others as read-only markers (D46), and never sends a change to a slice it
   doesn't own; the Core refuses one anyway.
3. **Who has transmit:** the holder's device name and whether it is on the air,
   sent to every session (D51). Taking transmit is a confirmed request; when the
   holder is on the air the Core unkeys it through R4's unkey-confirmed gate
   before transmit moves. The radio's own PTT takes transmit with no request
   (D52).
4. **Receivers:** which device's slices each receiver carries, so the phone can
   offer a receiver to take (D49) and name whose slice a pan move reaches (D50).
   Before the operator confirms either, the Core says what happens to the other
   device's slice: it moves to a free receiver, or it closes.
5. **Shared settings:** before a change that would disturb another device's
   slices, the Core says which devices and slices it reaches, so the phone can ask
   (D53).
6. **Notices:** a device is told who took its receiver, transmit or place, and
   when, and who changed a shared setting that reaches its slices, with Take it
   back where D49, D51, D52 and D55 give one.
7. **Capacity:** when the Core's display allowance or an allocation result says the
   phone's display doesn't fit (D54), the phone lowers its own frame rate, then its
   pixels, to the budget design's floor (the agreed quality policy), and shows the
   "Sharing" chip with the rate it settled on. The Core says why: its connection is
   shared, or its processing is short. Sound is never cut (D64).
8. **A fifth device and reclaiming:** with four connected, a fifth device takes
   one's place only when its operator confirms (D55), and a device on the air is
   unkeyed first. A device coming back to its own dropped session (the same device
   key) within 3 minutes takes it back without asking and gets transmit back only if
   nobody took it meanwhile (D56, D62).

### 4.6 Transmit safety

The PTT is a latch (D8), so the phone leans on every layer below. None of them
is new except the time-out's default.

1. **Link lost:** the station drops MOX on the watchdog deadline (remote design
   §12.1).
2. **Microphone starving on a live link:** the station's starvation deadline and
   per-mode action (remote design §12.3).
3. **Keyed too long:** the station's time-out, on for remote devices at 3
   minutes (D29, R-IOS-04).
4. **Half-connected:** transmit stays disabled until the snapshot is complete
   (remote design §12.2).
5. **Interlocks:** the TX interlock, PA protection and SWR gating stay at the
   station and cannot be overridden remotely (remote design §12.2). The phone
   shows a refusal with its reason and, where there is one, its fix ("Operate
   amp").
6. **On the phone:** locking unkeys (D24); the screen stays awake while keyed; a
   phone call or Siri interrupting the audio session unkeys; switching apps keeps
   transmitting, with the island (D25).
7. **After a reconnect, transmit stays off** until the operator taps PTT. It
   never resumes by itself.
8. **One device holds transmit** (D51). Taking transmit, or a place, from a
   device on the air unkeys it first (D22, D51, D55), and the radio's own PTT
   takes transmit the same way (D52).

### 4.7 Background, battery and data

* **Audio in the background.** The app declares the audio background mode for
  playback and capture. iOS will not start the microphone in the background
  (AVAudioSession refuses to begin recording there since iOS 12.4); a capture
  begun in the foreground may continue, and Apple's Push to Talk may start it
  for a hardware button (D26).
* **The lock-screen card and the Dynamic Island** are a Live Activity. Apple's
  limits shape them: updates are throttled, so readings step every few seconds;
  a Live Activity is active for up to 8 hours, then leaves the Dynamic Island
  and stays on the lock screen for up to 4 more; starting one needs the app in
  the foreground (or a Live Activity intent); its data is limited to 4 KB; on a
  locked device a button runs after the operator authenticates unless the app
  allows it without (UNKEY, Mute and Cancel run with one tap; Reconnect asks,
  JJ 2026-09-26); the lock
  screen card is at most 160 points tall; StandBy shows it at twice the size.
* **Sound only.** While the phone is locked or another app is in front, the
  phone asks the station to stop sending the band. Audio and the card's readings
  continue. This uses the R3 endpoint controls (R-R3-08) and telemetry
  (R-R3-13).
* **Data use.** Display frames are most of the data, so each mode sets the
  frame rate and detail it asks for (§5.4). The published figures are
  estimates from the reference clients measured in remote design §2.5 (94.4 and
  144.2 kbit/s at 30 frames a second, best case, one panadapter, receive only)
  until the bench measures ours (R-IOS-23).
* **Power.** Low Power Mode drops the band to Saver; a hot phone slows the band
  until it cools.

### 4.8 Amps and tuner

The station owns the accessory connections, discovery, configuration and
telemetry (R-R3-22), and receive-only operation cannot trigger transmit-coupled
accessory commands (R-R3-25). The R3 plan leaves the authorised transmit and
tuner workflows to R4, so the commands the phone needs (the amp's OPERATE and
STANDBY; the tuner's TUNE, OPERATE, BYPASS and antenna; the RF2K-S's OPERATE,
STANDBY and antenna) are station work for R4 (R-IOS-05). Discovery runs on the
station's network, which the phone is often not on.

### 4.9 Spots and FreeDV Reporter

The spot clients and the FreeDV Reporter connection run at the station (remote
design §6.4), so the station keeps collecting, and stays listed, while the phone
is away. Their lists reach the phone as record streams (remote design §6.1a).
How spots look on the band is set on each phone. The FreeDV Reporter lists the
operator's **callsign**, never the station's private label (pairing design
§3.3).

### 4.10 Values the station owns

Everything the phone shows about the radio comes from the station: the mode
list, the filter presets for each mode, the tune-step list, AGC ranges, meter
ranges and the board's capabilities (R-IOS-06). The phone draws its controls
from them. This keeps each radio correct (a Hermes Lite 2 and an ANAN-G2 differ)
and keeps Thetis-derived data out of the app (§4.11). The step list is Thetis's
26-entry `tune_step_list` (1 Hz to 10 MHz, `console.cs:1953-1982` [v2.10.3.15])
once the desktop ports it; today the desktop's STEP cycles a six-entry stand-in.

### 4.11 Licence (D4)

* The app is new Swift code, NereusSDR-original, licensed GPLv3 plus the App
  Store permission clause.
* **Nothing in it is copied or translated from Thetis, WDSP or AetherSDR.**
  Their authors have not granted the clause. The look is drawn fresh in Swift.
* Code shared with the station (for example a display-frame decoder compiled
  into the app rather than rewritten) must be NereusSDR-original, and every one
  of its copyright holders must agree to the clause.
* A provenance check covers the app's sources in CI (R-IOS-29).

Study the actual NereusSDR desktop, Thetis and OpenHPSDR sources to understand
behavior and protocol details before writing the phone implementation. The
phone keeps its own identity and implementation; a reference study is not a
default instruction to port code. Any authorized source reuse retains its
copyright notices, licence and attribution and must satisfy D4 before it can
ship in the app. Studying a meter or control does not change these conditions.

### 4.12 The app's structure

**Proposed here, not yet discussed with JJ.**

* **Screens in SwiftUI;** the band (spectrum, waterfall, flags and markers) in a
  Metal view, since the desktop draws the band on the GPU and Metal is iOS's GPU
  interface. SwiftUI is also what a Live Activity is written in.
* **Modules:** Link (TLS session, handshake, versions, reconnect); Mirror (the
  snapshot and live updates); Media (Opus in and out, the jitter buffer, the
  display-frame decoder); Band (renderer, gestures, haptics); Screens (tabs,
  Setup, sheets); Platform (the audio session, Live Activity, Push to Talk, the
  Action button, a Bluetooth PTT button, Bluetooth MIDI, Local Network).
* **Minimum iOS 17.4,** because Apple's Push to Talk transmit intent, which the
  Action button runs, needs it.

### 4.13 Building and shipping

* Building needs Xcode (Xcode 27 is installed on this Mac, and the app builds and runs on it).
* JJ's Apple developer account, which already signs the macOS DMG, covers
  TestFlight and the App Store.
* The first build goes to a TestFlight beta round (D2).
* App Review gets a video of the app on a real station, since there is no demo
  mode (D19).
* The export-compliance answers (the app uses TLS, SRTP and the pairing key
  exchange) and the privacy label are prepared with the first TestFlight build.
  The app ID carries the Push to Talk capability.
* The encryption question is answered YES: standard encryption in addition to
  what iOS provides (D103, JJ, 2026-09-28).

---

## 5. The screens, as confirmed

Each group lists what JJ decided in §3 and every drawn detail he confirmed in
the pass (D34). Pictures are in `2026-09-23-iphone-app-design/`.

### 5.1 On the band

![On the band](2026-09-23-iphone-app-design/01-on-the-band.jpg)
![Turned sideways](2026-09-23-iphone-app-design/02-sideways.jpg)
![Tuning dial](2026-09-23-iphone-app-design/03-tuning-dial.jpg)
![Spots on the band](2026-09-23-iphone-app-design/04-spots.jpg)
![The Pan and Display buttons](2026-09-23-iphone-app-design/25-pan-and-display-sheets.jpg)
![Tuning on the band](2026-09-23-iphone-app-design/26-tuning-on-the-band.jpg)

1. The toolbar, left to right: RX panel, the Sound panel (D77), Slice A, Pan 1,
   Display, the link dot with its round-trip time (a tap opens the Radio tab, JJ
   2026-09-26), and TX panel.
2. The tab bar: Panadapter, Modes, Tools, Radio, Setup.
3. Tap the band to tune the active slice there; drag the band to move it;
   drag a flag or its passband to tune that slice in its step (D74). Zoom
   minus and plus sit at the bottom right of the waterfall; PTT at the bottom
   left.
4. The flag uses the desktop's text size, narrowed to fit. The dBm scale is a
   little larger for fingers, and the band plan strip shows the station's plan
   at Small (D79).
5. The RX panel holds AF gain, AGC, filter presets, the noise buttons and
   squelch. The TX panel follows D143: pinned Settings and key controls,
   RF, configured amp/tuner/antenna controls, Audio, Voice and Processing,
   then the profile, keeping AM carrier and Mod Monitor.
6. Keyed: a timer on the PTT, the orange TX filter, the passband shading hidden
   (as on the desktop), and RF power, SWR and the mic level on the waterfall.
7. Sideways, the panels slide in from the sides. On a charger on its side, iOS
   shows the lock-screen card at double size.
8. The dial ticks on every detent and bumps harder on every whole kilohertz,
   with 36 detents in a full turn. Tap the knob's middle for the step list.
   Reversing the direction is a setting.
9. Setup's Navigation page on the phone swaps the desktop's Mouse section for
   Touch: drag to tune, tap to tune (optionally snapped to the step), pinch to
   zoom, and a choice of double-tap action.
10. Spots: tap a callsign to tune to it. Press and hold for its details (mode,
    source, spotter, comment, time) with a Tune button. Tap a +N badge to see
    the hidden spots as a list. Colours follow the desktop's DXCC priority.
11. How spots look is set on each phone. Which spots there are comes from the
    Core.
12. The mic meter is titled "Mic level", not the desktop's "Level", because on
    the band it sits next to RF power and SWR, where "Level" could be mistaken
    for the leveler. It is the desktop's Phone/CW applet gauge: -40 to +10 dB,
    yellow from -10, red from 0.
13. As on the desktop, each meter's title sits inside its bar, so a healthy mic
    level covers the words.
14. Pan 1 opens the pan's sheet under the toolbar, titled with the pan and its
    active slice (D73). The band grid's buttons are the Core's band list in its
    order, and the slice's band is lit. A tap moves the active slice to that band
    where it last was on it: its frequency, mode and filter, from the Core's band
    memory. Below the grid, Add a slice here opens a new slice on this pan, Add a
    notch at A puts a notch where slice A is listening, as the desktop's +TNF
    does, and Extended view shows the radio's full width either side of the band.
15. Display opens this pan's display sheet, kept on this phone (D73): the
    waterfall's palette and its levels (Clarity, which sets them from the noise
    floor and can be re-tuned; Auto, which follows each line's weakest and
    strongest signals; Manual, the levels set in Setup), the spectrum's fill, top
    and range, and the Core's extras on the band: peak hold, peaks and the
    noise-floor line. More display options in Setup goes to the Setup tab.
16. A control on either sheet that the Core can't do yet stays in place, greyed,
    with "Needs a newer Core" (D23).
17. Each flag shows its slice's tuning step; a tap lists the radio's steps
    and the one picked is that slice's, used by drags, taps (with snap on) and
    the dial. A tap on the flag's frequency opens a number pad: the frequency
    in MHz or kHz, Enter tunes the slice there (D74). With the pop-up knob
    chosen, that tap raises the knob instead, and a tap on the frequency above
    it opens the number pad (JJ, 2026-09-26; the setting's name "Pop-up knob").
18. The Display sheet's Line slider sets the trace's width on this phone, from
    one screen pixel to 3 points, starting at 0.5 points (D75).
19. Setup's Display page holds the band plan: its picker changes the station's
    band plan, and its size (Off, Small, Medium, Large, Huge) is kept on this
    phone (D79). The Display sheet keeps only waterfall and spectrum controls.

Core settings in this view and every other tab follow D140: show the operator's
edit at the touch, resolve it from its matching Core answer and keep newer edits
ahead of older replies. After five seconds without confirmation the responsible
control returns to the latest Core value with a notice; a session change discards
pending edits without re-sending. Keying controls retain their ordered safety path.

### 5.2 Tabs

![Tabs](2026-09-23-iphone-app-design/05-tabs.jpg)

1. **Modes** is one scrolling page for the active slice, top to bottom: slice
   switch, mode (the 14 modes), filter (the mode's presets plus the low and high
   edges), front end, AGC with AGC-T and AUTO, noise, audio, RIT and XIT, then
   transmit (TX filter, mic gain, PROC, LEV, EQ, CFC, VOX, MON). Mic Gain also sits
   directly under the mic level meter in the TX panel (D128).
2. While transmitting, every tab's title bar shows a red TX pill with the clock
   and Stop; one tap unkeys.
3. **Tools** lists the desktop's tools in its order, each marked Core, This
   phone or Both, showing each once the Core offers it (D41). On the phone,
   PureSignal is on, off and status only; calibration stays at the Core.
4. MIDI Mapping and Macro Buttons are dropped for now (D42). **Connection and
   performance** in Tools covers active connection details and all desktop Network
   Diagnostics features (D89); Support Bundle covers both ends. The current
   Setup page containing only Licences and the build identifier becomes **About this
   app**, expanded with the desktop's full About information adapted to the phone (D90).
5. **Radio** opens with the Core and link (name, direct or relay, round-trip
   time) and Disconnect; then the radio at a glance (model, firmware, protocol,
   sample rate, slices in use, PA volts, ADC overload, the station computer's
   CPU); then the accessories, each with a one-line status that opens its page;
   then the rest of the desktop's Radio menu: Antenna Setup, Transverters,
   Manage Radios, Protocol Info, each shown once the station offers it (D41).
6. **Setup** puts Devices first, then the desktop's categories:

   | Category | Pages | Marked |
   | --- | --- | --- |
   | Devices | Paired phones and computers, Add a device | Core |
   | General | Startup & Preferences, UI Scale & Theme, Navigation, Battery and sessions, Options | Both |
   | Hardware | Hardware Config, DDC Routing | Core |
   | PA | PA Gain, Watt Meter, PA Values | Core |
   | Audio | On this phone, Devices, TX Input, VAX, TCI, Advanced, TX Profile | Both |
   | DSP | AGC/ALC, NR/ANF, NB/SNB, CW, AM/SAM, FM, CFC, TNF, Filter Presets, Options | Core |
   | Display | Spectrum Defaults, Spectrum Peaks, Waterfall Defaults, Grid & Scales, Multimeter, TX Display | Both |
   | Transmit | Power, TX Profiles, Speech Processor, DEXP/VOX, PTT buttons | Both |
   | Appearance | Colors & Theme, Meter Styles, Gradients | This phone |
   | CAT & Network | Serial Ports, TCI Server, Peripherals, 4O3A, RF-Kit, TCP/IP CAT, MIDI Control, Data use | Both |
   | Test | Two-Tone IMD | Core |
   | Diagnostics | Radio Status, Connection Quality, Settings Validation, Logs, Export / Import | Both |

   Core settings are shared by every device paired with the Core;
   settings marked This phone stay on the phone. A page the desktop has not
   built yet appears once it exists (D41).
7. Left off the phone: the Keyboard page on the iPhone (an iPad with a keyboard
   gets it once the desktop builds it, D41); Appearance's Skins and Collapsible
   Display, which are about the desktop's window; the desktop's Remote Access
   page, whose job Devices does on the phone.
8. **Devices**: Rename for the Core; a reminder to back up the Core's key;
   the devices connected now (their slices and bands, and TX on the one with
   transmit) apart from the devices only paired, each paired device with when it
   was paired and last seen; one-tap Revoke, which drops that device at once,
   even mid-session; Add a device, which shows a one-time code; a note that up to
   four devices can be connected at once (D57, §5.8 item 13).
9. **TX Equalizer** in Tools draws the Core's TX EQ curve as sent, says which EQ
   is on the air, and edits the curve on the Core, on or off the air, under the
   rules of the other transmit settings (D92). Its layout is a board section
   JJ approves before it is built.
10. **Diagnostics > Export / Import** (deferred to a later release by D107 and D133; Task 58a is not built) saves a backup of the Core's settings and
    this phone's settings in one file through the Share sheet, never with any
    pairing key (D93), and restores one after naming the other connected devices
    and asking; the Core applies its part with a radio reconnect and refuses while
    anyone transmits (D94). Its layout is a board section JJ approves before it is
    built.
11. **Diagnostics > Logs** shows the Core's log as it happens with the Core's log
    category switches; Clear clears only this phone's view; the phone's own log
    stays in the Support Bundle (D95).
12. **TCI**: the desktop's own local TCI server's settings show disabled with a
    plain reason, since the phone runs no TCI server; the Core's TCI server
    settings are shown and edited once the Core publishes them (D96).
13. **Display > Spectrum Defaults**: Reset to Smooth Defaults applies the Core's
    650 ms spectrum average time (D97). The Display defaults themselves stay
    NereusSDR's own (D99).
14. **Diagnostics > Settings Validation**: "Repair invalid settings" in place of
    "Reset to defaults", working on a remote Core (D98).

### 5.3 Getting connected

![First launch](2026-09-23-iphone-app-design/06-first-launch.jpg)
![Connecting](2026-09-23-iphone-app-design/07-connecting.jpg)
![The station's side](2026-09-23-iphone-app-design/08-station-side.jpg)
![When things aren't right](2026-09-23-iphone-app-design/10-trouble.jpg)
![Pairing and connecting: the states in between](2026-09-23-iphone-app-design/24-pairing-and-connecting-states.jpg)

1. **Welcome:** one picture of radio, Core and phone ("Your station, from
   anywhere."), with two ways on: Find my Core and Set up a Core.
2. iOS asks once to search the local network, with the app's own reason under
   its question.
3. **Found it:** an unclaimed Core on the same Wi-Fi is claimed with one
   tap. From anywhere else its code works, and an address can be typed.
4. The microphone is asked for right after pairing, so iOS never interrupts the
   first transmission. Listening works without it.
5. **Set up a Core** explains the two ways to run one (on a computer, or on
   a small box from a flashed card). It finds the radio and waits for its first
   device, showing a code with no time limit until five wrong codes in a row
   close pairing, which only the Core's own computer then reopens.
6. **Your Cores** lists paired Cores first, then the Cores on this network
   that take a new device (D71). A Core that removed or forgot this phone
   stays listed with Pair (D70). The phone lists Cores only, never radios
   directly. Its visible actions menu includes Remove Core: disconnect first, then
   remove this phone's saved entry (D91).
7. A pairing code is a number and two words (for example `7-anvil-harbor`),
   typed once.
   **Enter an address** takes the Core's name, IPv4 address or IPv6 address
   (no brackets needed) and, in a field of its own below it, the port, filled in
   with the Core's standard port (D69). Connect goes on to the code when this
   phone isn't paired with the Core yet.
   **Pairing, the states in between** (picture 24): the code screen names the
   Core it pairs with, says where the code is (the Core's status page, or
   `nereusd pairing show` on its computer) and holds this phone's name for the
   Core (D65); a wrong code shows the Core's words and the wait before its new
   code; after the fifth wrong code in a row, pairing is closed and opens again
   only at the Core; when iOS isn't allowed to reach this network, the
   Core-not-answering sheet says so and where to allow it; a phone that can't
   read its own key any more (after an erase and restore) says so and offers a
   new key, after which each Core needs pairing again (D72).
8. **Link lost while keyed:** the phone says the Core stops transmitting on
   its own when the link goes. That is the Core's promise, since the phone
   can't see it happen. The phone keeps retrying, with Cancel.
9. **Back on the air:** transmit stays off until the operator taps PTT.
10. **The desktop's Remote Access page:** Run a Core on this computer; Keep
    it running when NereusSDR is closed; Start it with the computer; the
    Core's name with Rename; the pairing code, shown until a device claims
    it; then the paired devices with Revoke, and Add a device.
11. **A small box** shows its code, while it is unclaimed, on a status page
    any browser on its own network can open, which changes nothing, and to
    `nereusd pairing show` on its computer for claiming over SSH (`sudo nereusd
    pairing show` on a packaged Core, with no other options); the code never
    goes to its log. Only the console or an already-paired device can reopen
    pairing.
12. **Another device on the Core** no longer brings a question: up to four
    connect side by side (§5.8). The question comes for a fifth device, which
    names the four, how long each has been connected, when each was last used,
    and whether each is listening or transmitting.
13. A phone taking back its own dropped session is not asked; it gets its
    slices back, and transmit only if nobody took it meanwhile. The device whose
    place is taken is told who took it and when; its band stops; Take it back
    asks the same question the other way.
14. **Five trouble screens,** each naming its cause, so the operator knows
    whether to walk to the radio, the Core or the phone:
    * The radio is off: the Core answers but can't hear the radio; the
      desktop's DISCONNECTED over the band, the last frame and the five-second
      retry.
    * The Core isn't answering: what the phone tried (this Wi-Fi, direct,
      relay) and what to check.
    * The Core needs updating: only when it is two versions behind; names
      both versions.
    * An older Core: one version behind or less; the phone connects and
      greys what the Core can't do yet.
    * This phone is offline: not the Core's fault; the phone waits for a
      network and says the Core has already unkeyed.

### 5.4 Amps, tuner, audio and data

![Amps and tuner](2026-09-23-iphone-app-design/11-amps-and-tuner.jpg)
![Audio and data use](2026-09-23-iphone-app-design/12-audio-and-data.jpg)

1. Each accessory has its own page, reached from the Radio tab, saying it is on
   the station's network (the RF2K-S says "polled by the Core").
2. **Power Genius XL:** OPERATE, output and efficiency; the band it got from the
   radio and what it is paired with; the TX interlock summary (Block, the SWR
   gate and its grace time) with "Change in Setup"; its fault history; Advanced.
3. **Tuner Genius XL:** OPERATE, TUNE, a line when a tune is recalled from
   memory, three antennas with the operator's labels, the tune memory, Advanced.
4. **RF2K-S:** OPERATE or STANDBY, power, SWR, volts and amps, the antennas. Its
   tuner buttons are greyed with a note to use the amp's front panel until its
   firmware accepts commands.
5. **Interlock says no:** with the amp in STANDBY and the interlock on Block,
   PTT is refused with a red explanation and an "Operate amp" button.
6. **Audio on this phone:** the band plays through the iPhone speaker by
   default, or the earpiece, or AirPods. The microphone stays on the iPhone by
   default, because the AirPods' microphone would drop them to phone-call
   quality both ways (A2DP is output-only; with the hands-free profile allowed,
   iOS prefers it).
7. While transmitting, the band is muted, so the speaker can't feed the
   microphone, and MON plays in headphones only. Both are on by default.
8. iPhone voice processing is off, so the station's PROC, EQ and leveler shape
   the voice.
9. Audio quality is **High** by default (Opus at 48 kbit/s, audio up to 20 kHz),
   **Save data** (24 kbit/s, audio up to 8 kHz), or **Lossless** (48 kHz stereo
   16-bit L16, unchanged Core audio for digital modes). Explicit Opus bitrate
   requests use only the Core's advertised measured profiles and require
   `audioQualityVersion` 1. High remains selectable on an older Core without
   that offer, using its supported Opus profile without a bitrate request;
   the phone does not claim it negotiated 48 kbit/s in that case. Save data
   waits for the catalogue and stays disabled with a reason when not offered.
   Lossless requires a negotiated line and falls back to High Opus when the
   line or network cannot carry it. The Core's actual context and refusal
   determine the displayed result (R-R3-23, R-IOS-09).

   The microphone uses 48 kbit/s Opus, or 24 kbit/s under Save data, in mono
   48 kHz 20 ms frames. Negotiated Lossless carries each microphone frame as
   five 4 ms L16 packets, with mono repeated in the two channels. A quality
   change while keyed applies at a frame boundary without restarting capture,
   the key, encoder history or the RTP clock. Lossless fallback uses High.
   Receive queues are bounded at 64 Opus packets or 640 L16 packets, each
   representing at most 2.56 seconds; this is a burst limit, not a playback
   target or a reason to add that much latency (D84).
10. One tap on the speaker button on the band opens the Sound panel: Mute, then
    Speaker, Earpiece or AirPods (D77), without leaving the band.
11. **Data use:** one choice for Wi-Fi (Full or Balanced) and one for cellular
    (Full, Balanced, Saver or Audio only), each with its cost per hour;
    counters for this session and this month; a warning past 5 GB a month on
    cellular, on by default.

    | Mode | What it asks for | Estimated |
    | --- | --- | --- |
    | Full | 30 frames a second, full detail | about 70 MB an hour |
    | Balanced | 15 frames a second | about 45 MB an hour |
    | Saver | 5 frames a second, half the detail | about 30 MB an hour |
    | Audio only | No band, just the sound | about 24 MB an hour |

    These totals include High audio. Save data audio is about 13 MB an hour;
    Lossless is about 720 MB an hour while the connection carries it.
    Transmitting adds about 24 MB for each hour of talking at High, 13 MB at
    Save data, or 720 MB while the microphone line carries Lossless. If it
    cannot, the microphone uses High. The L16 payload calculation is
    48000 × 2 × 16 × 3600 / 8 = 691.2 MB per hour; about 720 MB includes
    estimated transport overhead. These are estimates, not measured app
    traffic or a cellular cap. Incoming and outgoing counters remain separate.
    Saver and Audio only reduce display traffic while keeping the selected audio
    quality (D141). A cellular Lossless warning shows its estimated cost on the
    audio-quality page and under the cellular data choices; quality is never
    silently changed by the display mode.
12. The first time on cellular, the phone names the selected mode and its
    estimated cost. Cellular starts at Balanced; the operator can keep Full
    selected for cellular independently of the Wi-Fi choice. The September 28
    board revision replaces the floating chip with one slim passive information
    row below the toolbar, above the slice labels. Show the phone's network,
    actual active path and measured app traffic; incoming and outgoing rates
    each carry their own independently scaled unit, such as Mbps or kbps.
    Do not add a Details button or a second row. The existing dot/ms control
    is the visible entry to Connection and performance under Tools (D89).
13. The data figures are estimates until the bench measures them (R-IOS-23).

### 5.5 Away from the app

![Lock screen](2026-09-23-iphone-app-design/13-lock-screen.jpg)
![In another app](2026-09-23-iphone-app-design/14-other-apps.jpg)
![A hardware PTT button](2026-09-23-iphone-app-design/15-hardware-ptt.jpg)
![Long sessions](2026-09-23-iphone-app-design/16-long-sessions.jpg)
![Transmit time-out](2026-09-23-iphone-app-design/17-transmit-time-out.jpg)

1. **The lock-screen card** shows the Core, the link, slice A with its
   frequency and signal level, and mute. iOS limits how often the card can
   change, so the reading steps every few seconds.
2. When the link drops, the lock screen lights up and buzzes, then keeps
   retrying until cancelled. Back on the air, transmit stays off.
3. **In another app,** the Dynamic Island keeps the slice and frequency beside
   the camera. Pressing and holding opens the card, where UNKEY works in one
   tap. On iPhones without the island, the card shows only on the lock screen
   and a lost link arrives as a banner.
4. If the link drops while keyed in another app, the island opens by itself,
   the phone buzzes, and it says the Core has already unkeyed.
5. The opened island shows forward power, SWR and the time left before the
   time-out ("Time-out in 2:13").
6. **PTT buttons** (Setup, Transmit, this phone): the headset button, a paired
   Bluetooth PTT button, and the Action button (chosen in the iPhone's
   Settings). All toggle, like the PTT on the band. One switch, on by default,
   decides whether they key a locked phone.
7. While connected, iOS shows its blue Push to Talk pill and its own lock-screen
   panel, and only one Push to Talk app can be active at a time. Apple's rule;
   it comes with D26.
8. **Keyed from a pocket:** the card turns red and says to press the button
   again to unkey; the card's own UNKEY also works with one tap, no Face ID
   (JJ, 2026-09-26). On the band,
   the PTT follows the button and says what keyed it ("Keyed by headset").
9. **Battery and sessions** (Setup, General, this phone): sound only while
   locked or in another app, on by default.
10. Keep the screen on: Never, While charging, or Always, with Always the
    default (D27). It always stays on while keyed.
11. A sleep timer (off, 30 minutes, 1 hour or 2 hours, then disconnect).
    JJ's September 28 decision: if VOX is armed but the phone is not
    transmitting when the timer expires, turn off VOX and disconnect. If
    actively transmitting, wait until transmission ends, then turn off VOX
    and disconnect. Bind this work to the expiring session so a delayed
    completion cannot affect its replacement (D88). Low Power Mode drops the
    band to Saver, and a hot phone slows the band until it cools; both are on
    by default.
12. After a while locked, the waterfall marks the stretch that was sound only
    ("Locked 19:42 to 20:15 · sound only"), so nothing looks lost.
13. Just before iOS ends the card at eight hours, the app leaves a last message
    on it. The sound carries on, and opening NereusSDR starts a fresh card.
14. **Transmit time-out** (D29): on the PTT buttons page, a Transmit time-out
    group marked Core with "Stop transmitting after: 3 minutes" (30 seconds
    to 30 minutes, or off). When it fires, the band shows an amber notice in the
    Core's words (for a phone: "Transmit stopped after 3:00, the Core's time-out
    for phones and tablets.", from `src/core/session/TransmitStateFacade.cpp`),
    and PTT is back to Tap.

### 5.6 iPad

![iPad on its side](2026-09-23-iphone-app-design/18-ipad-landscape.jpg)
![iPad upright](2026-09-23-iphone-app-design/19-ipad-upright.jpg)

1. On its side, the analog S-meter heads the column, then RX, then TX with the
   amp and tuner. A corner button hides the column, and the Core's name shows
   in the toolbar. Otherwise the iPad has the phone's toolbar, flags, band plan,
   toggle PTT and tabs.
2. Upright (834 by 1210 points on the 11-inch), the S-meter, RX and TX sit in
   three columns below the band, and the column button is hidden.

### 5.7 Spot Hub and FreeDV Reporter

![Spot Hub](2026-09-23-iphone-app-design/20-spot-hub.jpg)
![FreeDV Reporter](2026-09-23-iphone-app-design/21-freedv-reporter.jpg)

1. **Spot Hub** is one Tools page: the Spot List and Display first; then each
   source with a status dot (cluster, RBN, WSJT-X, SpotCollector, POTA, FreeDV,
   PSK Reporter); then Identity. The sources and the list live at the Core;
   how spots look lives on the phone.
2. **Spot List:** newest first, with the desktop's columns (time, kHz, call,
   mode, source, spotter, comment), source pills and a band filter. A tap tunes.
3. **Display:** the desktop's knobs and defaults (spots on; 3 levels of 1 to 10;
   halfway, 0 to 100; font 16 of 8 to 32; override colours off; override
   background on at 48). Each source can be shown or hidden on this phone's
   band. Spot lifetime (default 30 minutes) and "Clear all spots" belong to the
   Core.
4. **A source page** (the cluster, for example): server, port, callsign,
   auto-connect (off), the live console and a command line.
5. **FreeDV Reporter** folds the desktop's 14 columns into two lines per
   station. Rows tint rust while a station transmits, slate while it hears
   someone and mauve when its message changes, fading after six seconds.
6. A band filter (All by default) and "follow the radio" by band or frequency
   (the desktop's track frequency).
7. The operator's status message, with saved messages and Send.
8. Pressing and holding a station shows every column plus the desktop's
   right-click actions: tune, ask to QSY, look up on QRZ.com or HamQTH, copy the
   callsign.
9. **Ask to QSY:** pick the slice's frequency, theirs, or type one, then Send
   QSY. As on the desktop, the operator's radio tunes there too.
10. The reporter connection and "hide my station" run at the Core, so your
    station stays listed while the phone is away. Miles and kHz are set per
    phone. Distance and heading stay blank until a grid square is set.

### 5.8 Several devices at once

![Several devices at once](2026-09-23-iphone-app-design/22-several-devices.jpg)

On every screen here this phone has slice A on 40 m; the MacBook at the shack has
slices B (sharing this phone's receiver) and D, and has transmit; the iPad has
slice C on 20 m.

1. **Another device's slice** on the band: a dashed line in its colour, a label
   at the foot of the spectrum with the device's name, and TX on the label while
   it has transmit; no flag. A tap on the label opens a note: whose slice, where,
   and that it can be changed only there (D115).
2. **PTT names the holder** while another device has transmit ("PTT" over
   "MacBook", in muted red), and this phone's own TX badges are off.
3. **Taking transmit:** a tap on PTT asks first, naming the device and what it is
   doing; after Take transmit, PTT keys as usual.
4. **While it's on the air:** the band shows the other device's transmission, PTT
   turns red with its name, and the button reads "Unkey and take over" in red
   beside the TX clock.
5. **Transmit taken from you:** a notice with who and when, and Take it back.
6. **Taken at the radio:** the radio's own PTT took transmit; PTT names the
   radio; Take it back.
7. **When the Core runs short:** a "Sharing · 12 fps" chip and a one-time notice
   that the device with transmit keeps its full band and sound.
8. **Every receiver in use:** to listen where no receiver reaches, pick a
   receiver to take from a list of who has each one and what they are doing.
9. **Your receiver taken:** RECEIVER TAKEN over the band, with who and when;
   slice A closed with its frequency and settings kept; Take it back. The
   connection stays up.
10. **Moving a shared receiver:** going to another band names the device that
    shares the receiver and says what happens to its slice ("Go to 20 m" or
    "Stay on 40 m").
11. **Shared settings:** a change that reaches another device's slices names that
    device, shows the change (the attenuator on ADC 1 from 0 dB to 20 dB), and
    asks.
12. **Told afterwards:** the other device gets a note of who changed what, and
    when; there is nothing to answer.
13. **Setup, Devices:** Connected now, with each device's slice letters and
    bands, TX on the one with transmit, and Revoke; then Paired. "Up to four
    devices can be connected at once."
14. **A fifth device:** "Four devices are on KG4VCF/shack", the four with what
    each is doing, starting on the one idle longest that can be replaced (a device
    that's away counts as idle longest; the computer running the Core can't be
    replaced, D64); picking the one on the air turns the button red ("Unkey and take
    the MacBook's place").
15. **Your own session:** after a lost link the phone comes back without asking.
16. **The device whose place was taken** (an iPad): TAKEN OVER, with who and
    when; its slices and settings kept on the Core; Take it back.

### 5.9 Several devices: states and notices

![Several devices: states and notices](2026-09-23-iphone-app-design/23-several-devices-states-and-notices.jpg)

Drawn the same evening as §5.8 and kept by JJ (board v50 and v51), with the same cast.

**The states in between:**

1. **A device that's away:** the MacBook's link dropped while it had transmit; its
   label greys with "away", and taking transmit asks "Take transmit from the
   MacBook?" without the red button, since nobody is on the air.
2. **Transmit on its way:** right after Unkey and take over, PTT waits with a
   turning ring and "Wait" while the Core unkeys the other device, then reads Tap.
3. **Held by the radio's mic:** on a Core with no desktop, the radio's own mic
   transmits on this phone's slice A; the flag says ON AIR, the frequency greys, and
   tuning slice A waits until the press ends.
4. **Away, on Devices:** an amber dot and "away for 1 minute"; it keeps its place and
   slices for 3 minutes.
5. **Away, for a fifth device:** a device that's away comes first in the list.

**Notices:**

6. **Back within 3 minutes, without transmit:** "Back on the air. It was this
   phone's own session, so it didn't ask."
7. **Back, transmit taken meanwhile:** "The MacBook took transmit while you were
   away, at 19:55." with Take it back.
8. **Back after 3 minutes:** "Back after more than 3 minutes. Your slices are back,
   and transmit was freed while you were away."
9. **Back after 3 minutes to a full Core:** the fifth-device question, headed "Your
   place went to another device", saying the phone was away more than 3 minutes.
10. **A slice that couldn't come back:** "Slice E couldn't come back: every receiver
    is in use. Its frequency and settings are saved."
11. **The antenna stays:** "The antenna stays on ANT1 while the MacBook listens on
    it." (D61)
12. **The Core is busy:** the Sharing chip with "The Core is busy." when processing
    runs short, in place of "The Core's connection is full."
13. **Short, nobody has transmit:** "The Core's connection is full. The devices share
    it, so this phone's band slows to 12 frames a second until there's room." (also
    when the radio's mic has transmit)

---

## 6. Requirements

IDs `R-IOS-nn` are new with this spec, stable, and never renumbered; a withdrawn
one is marked, not deleted. It relies on R-R3-06, R-R3-08, R-R3-13, R-R3-22,
R-R3-23 and R-R3-25 as the R3 plan defines them, and amends remote design §7.0
(D23) and §7.1 (§3.9, which replaced D21 on 2026-09-24).

Evidence layers:

* **Software:** unit and conformance tests, in the app's test target and the
  station's ctest (run offscreen).
* **Integration:** the app against a real `nereusd` over the link, on a LAN and
  through the relay.
* **Device:** on a real iPhone or iPad: lock screen, background audio, Push to
  Talk, haptics, permissions.
* **Bench:** with a radio on the air: an ANAN-G2, a Hermes Lite 2, and the
  Power Genius XL and Tuner Genius XL where the requirement names them.
* **Measurement:** data, battery and heat over hours.

Hardware evidence stays **pending** until a device or the bench shows it.

### 6.1 Station-side work the app needs

| ID | Requirement | Owner | Evidence |
| --- | --- | --- | --- |
| R-IOS-01 | A written, versioned specification of the link (framing, handshake, snapshot, messages, capabilities, version rules) and a conformance suite the station and the app both run. | Station, before R4 | Software: the suite passes on the station and in the app, at both majors the app speaks. |
| R-IOS-02 | Several sessions at once (§3.9, §4.5 items 1 to 3 and 8; amended 2026-09-24 from the one-holder report): up to four paired devices, each owning its slices and pans; one pool of slice letters; receivers from the radio's pool, shared when slices fit a window; who is on the Core, whose each slice is and who has transmit, sent to every session; a fifth device takes a place only on its operator's confirmation; a device reclaiming its own session is admitted without asking and gets transmit back only if nobody took it. | Station, R4, per the Core/GUI session's design for several devices | Software: session tests for four devices, ownership refusals, the letter pool, shared receivers, the fifth device and silent reclaim. Integration: four devices on one Core. |
| R-IOS-03 | "Unkey and take over" (amended 2026-09-24): taking transmit (D51) or a place (D55) from a device on the air unkeys it through the unkey-confirmed gate before transmit or the place moves; the radio's own PTT takes transmit, unkeying a holder on the air first (D52). | Station, R4 | Software. Bench: the holder keyed on air, the taker confirms, the carrier stops before transmit moves; the radio's PTT pressed while a phone holds transmit. |
| R-IOS-04 | A remote transmit time-out: a station setting, on by default at 180 seconds for sessions from paired remote devices (30 seconds to 30 minutes, or off), off by default for the desktop at the station as in Thetis. When it fires the station drops MOX and TUNE and tells the client why; the time remaining is part of the transmit state the client sees. From Thetis `TimeOutTimerManager.cs` and `setup.designer.cs` `udMoxToTSeconds` (180, range 30 to 1800) and `chkToTMox` (off) [v2.10.3.15]. | Station, R4 (remote design §12.2) | Software: the timer, the reason and the remaining time. Bench: it fires on air at the setting. |
| R-IOS-05 | Transmit-coupled accessory commands through the station: the amp's OPERATE and STANDBY; the tuner's TUNE, OPERATE, BYPASS and antenna; the RF2K-S's OPERATE, STANDBY and antenna. The TX interlock is enforced at the station and every refusal carries its reason. | Station, R4 (R-R3-25) | Software. Bench: Power Genius XL and Tuner Genius XL; RF2K-S when one is available. |
| R-IOS-06 | The station advertises the values it owns and the phone shows (§4.10): the mode list, filter presets per mode, the tune-step list, AGC ranges, meter ranges and the board's capabilities. | Station, R3 follow-on | Software: the snapshot carries them; the phone builds its controls from them for an ANAN-G2 and a Hermes Lite 2. |
| R-IOS-07 | The desktop's Remote Access page of §5.3 (items 10 and 11 describe it and the small box). | Desktop, R6 | Software, and screenshots of the running page per the ui-verification skill. |
| R-IOS-08 | Pairing, devices and revocation per the pairing design: one tap on the LAN, the code from anywhere, the window open while unclaimed with no timer, reopening from the console or a paired device, revoke dropping a live session, the station-key backup prompt, and the code on a small box's status page and console. | Station, R5 and R6 | Software. Integration: pair on the LAN, pair by code through the relay, revoke mid-session. |
| R-IOS-09 | High by default, Save data and negotiated Lossless, as §5.4 item 9. Explicit Opus bitrate requests use the Core's measured offer (R-R3-23); unsupported choices stay visible with reasons. High can use a legacy Core's supported Opus without claiming a negotiated bitrate. The microphone follows quality changes and Lossless fallback at frame boundaries without restarting the key or RTP clock. | Station, R3; phone media | Integration, including actual encoder configuration, Lossless negotiation/fallback and continuous RTP across changes. |
| R-IOS-10 | Sound only: with every display endpoint disabled, audio and the card's readings keep flowing (R-R3-08, R-R3-13). | Station, R3 | Integration: traffic drops to audio and telemetry, and the card's reading keeps moving. |
| R-IOS-30 | Asked changes and notices (§4.5 items 4 to 6, D49, D50, D53): taking a receiver, moving a shared receiver, and a shared setting that would disturb another device's slices are confirmed requests; the Core says beforehand which devices and slices they reach and what happens to them; every affected device is told who, what and when, with Take it back where §3.9 gives one. | Station, R4, per the Core/GUI session's design for several devices | Software: each request confirmed and cancelled, and each notice. Integration: two devices. |
| R-IOS-31 | Capacity (D54): when the sessions together ask for more display and audio than the Core can send, the transmit holder keeps its full display and audio, the others' frame rates drop first, and each slowed session is told the rate it gets. | Station, R3 follow-on | Software: the allocation with four sessions. Measurement: four devices over a constrained uplink. |

### 6.2 The app

| ID | Requirement | Evidence |
| --- | --- | --- |
| R-IOS-11 | On the band as in §5.1 items 1 to 7 and D7 to D11: toolbar, flags and the fold rule, markers per D10, toggle PTT, zoom, band plan, RX and TX panels, keyed view; the same layout sideways. | Software: layout and fold-rule tests. Device: screenshots on two iPhone sizes, both orientations, compared against the board. |
| R-IOS-12 | Tuning: tap and drag on the band; the three dials, off until chosen; steps from the station's list; haptics per detent and per whole kilohertz; the Touch settings (§5.1 items 8 and 9). | Software: step and detent arithmetic. Device: the haptics felt on a real iPhone. |
| R-IOS-13 | Transmitting from the phone: the toggle PTT; the keyed view with RF power, SWR and mic level (D14); refusals shown with the station's reason and fix; transmit never resumes after a reconnect; an audio interruption (a call, Siri) unkeys. | Software: the PTT state machine. Bench: keying on air from the phone; a call arriving mid-transmission unkeys on air. |
| R-IOS-14 | Locked and in other apps (§5.5 items 1 to 5): locking unkeys; the screen stays awake while keyed; switching apps keeps transmitting with the island; the card as drawn; sound only; the eight-hour last message. | Device: a real iPhone locked, switched and left eight hours. Bench: transmission stops on lock and continues across an app switch. |
| R-IOS-15 | Hardware PTT through Apple's Push to Talk (§5.5 items 6 to 8): headset button, Bluetooth PTT button, Action button; keying a locked phone; "Keyed by headset". | Device and Bench, with each button. |
| R-IOS-16 | Connecting (§5.3): first launch, the Local Network question, pairing three ways, the microphone asked after pairing, the station list, link lost and back on the air, the five trouble screens; the version rules of §4.4. | Integration: each failure induced against a real station, including a station one major back and one two majors back. Device: the permission prompts. |
| R-IOS-17 | The several-devices screens of §5.8 and §5.9 and §5.3 items 12 and 13 (amended 2026-09-24 from the takeover screens). | Integration: four devices on one Core, listening and transmitting, with a fifth device and the radio's own PTT. Device: screenshots compared against the board. |
| R-IOS-18 | Tabs and Setup (§5.2): Modes, Tools, Radio, the Setup tree with its tags, Devices; controls in both places (D15). | Software: each control writes to its owner (station or phone). Integration: a station setting changed on the phone shows on the desktop. |
| R-IOS-19 | Accessories on the phone (§5.4 items 1 to 5): the three pages, the TX panel controls, the interlock refusal with "Operate amp". | Bench with the devices. |
| R-IOS-20 | Audio on the phone (§5.4 items 6 to 10): routes, the iPhone microphone by default, the band muted while talking, MON in headphones only, voice processing off, the quality choice, the speaker's route menu. | Device: every route, including AirPods; no feedback on the speaker while keyed. Bench: on-air audio shaped by the station's processing. |
| R-IOS-21 | The transmit time-out on the phone (§5.5 items 5 and 14): the setting marked Core, the time left in the island, the notice when it fires. | Software. Bench, with R-IOS-04. |
| R-IOS-22 | Long sessions (§5.5 items 9 to 13): sound only when locked, the screen-on choice defaulting to Always, the sleep timer, Low Power Mode to Saver, the heat slow-down, the locked stretch marked in the waterfall. | Device and Measurement: a multi-hour run with battery and heat logged. |
| R-IOS-23 | Data use (§5.4 items 11 to 13): the modes and their defaults (D28), the counters, the 5 GB warning, the first-time cellular note and chip. The estimates are replaced by measured figures before release. | Measurement: each mode for an hour on Wi-Fi and on cellular. |
| R-IOS-24 | iPad on its side and upright (§5.6, D30, D31). | Device: an 11-inch iPad in both orientations. |
| R-IOS-25 | Spots on the band and Spot Hub (§5.1 items 10 and 11, §5.7 items 1 to 4). | Integration: the station's spot clients feeding the phone. Device. |
| R-IOS-26 | FreeDV Reporter (§5.7 items 5 to 10), with the callsign and never the station label. | Integration: against the live reporter. |
| R-IOS-27 | The app draws each radio's controls from the values the station advertises (R-IOS-06) and has no radio tables of its own. | Software: the same build shows an ANAN-G2's and a Hermes Lite 2's presets and ranges correctly. |
| R-IOS-28 | Store readiness (§4.13): the TestFlight round, the App Review video on a real station, the export-compliance and privacy answers, the Push to Talk capability. | The App Store Connect record, checked by JJ. |
| R-IOS-29 | Licence (§4.11): no Thetis, WDSP or AetherSDR code in the app; the App Store permission clause in its licence; shared code cleared with its copyright holders. | Software: the repository's provenance check extended to the app's sources, run in CI. |
| R-IOS-32 | Connection and performance (D89): active control/media addresses and IPv4/IPv6 families, transport and direct/relay path, rendezvous involvement, and the complete desktop Network Diagnostics feature set, reached through a visible Tools entry. | Software: actual selected-route changes, every chart/detail/control in the Task 59 parity inventory, measurement fixtures, history and reset boundaries; screenshots and desktop comparison; device verification on direct IPv4, direct IPv6 and RV/relay routes. |
| R-IOS-33 | About this app (D90): the complete desktop About information adapted to the phone, with accurate app identity, credits, project links, phone library notices and Corresponding Source. | Software: item-by-item comparison with desktop About; bundle-version/build checks, reachable links and complete notices; phone portrait/landscape screenshots and offline access to bundled information. |
| R-IOS-34 | The TX EQ curve (D92): the Core's `txEqCurve` drawn as sent on a -24 to 24 dB scale, with which EQ is on the air, the unavailable state, and an older Core's disabled reason; editing through `txEq.setCurve` under the transmit-settings rules, on or off the air, never keying. The editing half waits on the Core's `txEq.setCurve` wire note. | Software: the drawing against the link document's worked example to 0.01 dB; the `tx-eq-curve` conformance session; the permission gate and the Core's refusal words; an edit's message against `FakeStation`. Integration: an edit on the phone shows in the desktop's TX EQ dialog. Bench: an edit while keyed on air (pending). |
| R-IOS-35 | (Deferred to a later release by D107 and D133; Task 58a is not built, and the item stays on the project roadmap.) Settings backup and restore (D93, D94): one `NereusSDR.nereus-settings` file in the desktop remote window's combined format with the Core's part and this phone's part, saved and opened through the Share sheet, holding no pairing key; a restore that names the other connected devices, asks, and lets the Core apply its part with a radio reconnect; refused while anyone transmits. The restore half waits on the Core's restore wire note. | Software: the file decodes as the desktop's `SettingsBackup` does; a test finds no key material in it; the restore flow's naming, confirmation, refusal and failure (existing settings kept). Integration: a backup from the phone opened by a desktop remote window; a restore with a second device connected, which reconnects by itself. |
| R-IOS-36 | The Core's Logs page (D95): the live `coreLog` stream, the Core's log category switches with the Core's labels, Clear on this phone only; the phone's own log only in the Support Bundle. The labels wait on `log-categories-for-phone.md`. | Software: the stream, the backlog, Clear leaving the Core untouched, the categories through the seam and then from the Core. Integration: the live log of a real Core on the phone. |
| R-IOS-37 | TCI on the phone (D96): the desktop's local TCI server settings shown disabled with a plain reason; the Core's own TCI server settings shown and edited once published. The Core's settings wait on its TCI wire note. | Software: the disabled rows and their reason; each published setting written to the Core. Integration: a TCI setting changed on the phone shows on the desktop. |
| R-IOS-38 | Reset to Smooth Defaults (D97): the Core's 650 ms spectrum average time with the rest of the March tuning. | Software: the values the action writes, against the Core's description. Integration: the same values after the desktop's Reset to Smooth Defaults. |
| R-IOS-39 | Repair invalid settings (D98): shown with the Core's label and run on a remote Core. It waits on the Core's description of the renamed action. | Software: the action rendered from the description and its message against `FakeStation`. Integration: a repair run from the phone on a real Core. |
| R-IOS-40 | Transmit stage meters and the muted mic (D104, D105): the strip under Mic level with the peak mark, dimmed readings off the air, the older-Core line, short CFC labels; Mic level greyed with a plain reason while the Core's mic is muted, and no mute button. | Software: each state's words against `FakeStation`, large type keeping the bars. Integration and bench: the readings keyed and idle against a real Core (idle values not yet observed). |
| R-IOS-41 | The RADE row on the VFO flag (D108): callsign or RADE, the lock dot, SNR and offset in the note's spaced form, the six differences from the board. | Software: the words, dot, offset sign and older-Core gate cases (`RadeRowWordsTests`, `RadeRowMirrorTests`). Integration: against a Core that sends `radeStatus`. |
| R-IOS-42 | Taking control of a slice (D109, D110, D115, D123, D124): the slice kept as it is, no prompt for anyone, the old owner told and able to take it back, the hosting desktop named by its own device name, the Core's refusals shown as sent, Take control greyed ahead of time while the slice is on the air, the first-key notice, and the owner words as the Core sends them. | Software: each refusal string and holder word against the Core's text; Take control live on receive-only. Integration: a take-over between two devices on a real Core. |
| R-IOS-43 | Alex low-pass rows (D112): a dot per row, filled for the filter in use, display only, no dot when `alexLpfBits` is -1 or absent. | Software: the rows against fixtures with and without `alexLpfBits`. |
| R-IOS-44 | Flag buttons (D113): flag icons are real buttons and the active flag is finger-sized at all times, with targets of at least 40 by 40 points. The look is look 2 (D118). | Software: target sizes and overlaps at large type. Board: JJ sees the looks before anything is built. |
| R-IOS-45 | The slice list, the jump and the slim flag (D119 to D122, D125, D127): the Slice button opens the list in letter order; Listen jumps to the slice's band with its flag, one tap back; the controls follow the band viewed; PTT returns to the transmit slice's band; the slim owner row with AF Gain, Mute and Stop listening in the speaker tab; a tap on the band tunes a taken slice; the Core's refusal of a band display keeps the phone on its own band; the split moves down by just what the flag needs, unsaved, restored on Back. | Software: list order, the active slice per viewed band, the refusal words, the split floor and its restore, large type. Board: the approved frames. Integration: a listen, jump and take against a real Core. |
| R-IOS-46 | The TX badge take-over (D129): the three cases in order, one ask at most for transmit, nothing keys, refusals shown with the Core's reason, a badge that cannot start a take disabled. | Software: each case, the command-id matching, the stale-grant guard and the holder wait against `FakeStation`. Integration: a badge tap on another device's slice on a real Core (the Core's trunk 01d797e56). |
| R-IOS-47 | Shared-input filter words (D126): the filter reason and the low-pass reason from the Core shown in the flag's Filter policy menu as the desktop shows them; the WIDE chip on the band is approved as drawn and sits below the frames-per-second readout when that shows (D136). | Software: the words against the Core's fields with and without the low-pass reason. Bench: slice A on 80 m with the phone on 20 m against a real radio. |
| R-IOS-48 | The RX2 attenuator and preamp rows and the Level Cal preamp line (D130, D131): the attenuator in the Step att row 0 to 31 dB through the Core's field, the preamp list from the Core's catalog and greyed with its reason, "Preamp: none on this receiver input." | Software: each row against catalog fixtures with and without RX2. Bench: an HPSDR radio and a G2. |

### 6.3 How this becomes a plan

One plan builds all of it, station and app together, ordered by dependency:
[2026-09-23-iphone-app-plan.md](2026-09-23-iphone-app-plan.md). Two sessions run
it, one owner per task (JJ, 2026-09-24): the Core/GUI session builds every station
and desktop task in its lanes, and the phone session builds the app, each phone task
waiting for the station tasks it uses. What follows was the first proposal, which JJ
turned down on 2026-09-23 ("i hate us splitting plans"); it is kept for the record.

This spec is larger than one plan. The station-side requirements (§6.1) go
into the remote-station phase plans that own them: R-IOS-06, R-IOS-09 and
R-IOS-10 into an R3 follow-on; R-IOS-02 to R-IOS-05 into R4; R-IOS-08 into R5
and R6; R-IOS-07 into R6. R-IOS-01, the written link and its conformance
suite, comes first, because every other plan tests against it. The app (§6.2)
gets its own plan, built against that suite from the start and taken onto
the air as R3, R4 and R5 land.

---

## 7. Error handling

| Situation | The Core | The phone |
| --- | --- | --- |
| Link lost while listening | Keeps the radio state (remote design §13) | Retries with backoff, cancellable; LINK LOST on the band, the card and the island |
| Link lost while keyed | Drops MOX on the watchdog | Says the Core stops on its own; retries; back on the air with transmit off |
| The radio is off | Answers; reports the radio link | The desktop's DISCONNECTED, the last frame, the five-second retry |
| The Core isn't answering | | What it tried (this Wi-Fi, direct, relay) and what to check |
| Versions two majors apart | Refuses, naming both | "The Core needs updating", naming both |
| An older Core | Negotiates down | Connects; greys "Needs a newer Core" |
| The phone is offline | Unkeys if it was keyed (watchdog) | Waits for a network; says the Core has already unkeyed |
| Four devices are connected | Lists them (R-IOS-02) | The fifth device's question (§5.8) |
| This device's place is taken | Admits the fifth device, unkeying this one first if it was on the air | The band stops; who and when; Take it back |
| Transmit taken, by a device or the radio's own PTT | Moves transmit, unkeying the holder first if on the air (R-IOS-03) | PTT names the new holder; a notice with Take it back |
| This device's receiver taken | Closes the slice on it and keeps its settings (R-IOS-30) | RECEIVER TAKEN; who and when; Take it back |
| A shared setting changed by another device | Applies it and tells the devices it reaches (R-IOS-30) | A note of who changed what |
| The Core runs short of room | Keeps the transmit holder whole and slows the rest (R-IOS-31) | The "Sharing" chip |
| Transmit refused (interlock, amp in STANDBY, PA protection) | Refuses with a reason | Red explanation, with the fix where there is one |
| The time-out fires | Drops MOX, gives the reason | Amber notice; PTT back to Tap |
| A call or Siri interrupts | Unkeyed by the phone's request | Unkeys; audio resumes afterwards |
| The device is revoked | Drops the session at once (pairing design §7) | Back to the list of Cores, where it stays with Pair (D70) |
| Low Power Mode or a hot phone | Honours the smaller display request | Saver, or a slower band until it cools |

---

## 8. Verification plan

* **Before any code:** the link specification and its conformance suite
  (R-IOS-01), so the app and the station are tested against the same contract.
* **Software on every change:** the app's unit tests and the station's ctest,
  run offscreen.
* **Integration:** the app against a real `nereusd` on a LAN and through the
  relay, including a station one major behind and one two majors behind, and
  four devices on one Core with a fifth trying to connect.
* **Device matrix:** two iPhone sizes (one with the Action button), an 11-inch
  iPad, AirPods, a Bluetooth PTT button, both orientations, locked and
  unlocked, and an eight-hour session.
* **Bench, on air:** keying from the phone; lock, app switch and a call during a
  transmission; the time-out firing; taking transmit, and a place, from a device
  on the air, and the radio's own PTT taking transmit from a phone; the
  remote design's watchdog test (the link severed mid-transmission on each relay
  rung) rerun with the phone as the client; the amp and tuner commands.
* **Measurement:** data per mode per hour on Wi-Fi and cellular, battery per
  hour with the screen on and with sound only, and heat.
* **UI evidence:** screenshots of every screen in §5 from the real app on a
  device, compared against the board, per the ui-verification skill.

---

## 9. Open items

Each has an owner and the moment it is settled.

* **Data figures** (§5.4): measured on the bench before release (R-IOS-23).
* **Microphone starvation deadlines and the per-mode action:** R4, remote design
  §12.3.
* **Keepalive intervals on cellular:** the pairing design's §9.8 bench.
* **Console command names** for reopening pairing and resetting a station to
  unclaimed: R6.
* **The app's structure and minimum iOS version** (§4.12): proposed in this
  spec; JJ rules on them in his review.
* **The station's design for several devices**: written as
  [2026-09-24-several-devices-on-one-core-design.md](2026-09-24-several-devices-on-one-core-design.md)
  and reviewed twice against this spec (2026-09-24); §4.5, R-IOS-02, R-IOS-03,
  R-IOS-30 and R-IOS-31 take their wire form from its sections 10.2 to 10.9.

---

## 10. Mockups

The pictures, one per board section, in `2026-09-23-iphone-app-design/`:

| Picture | Board section | Spec |
| --- | --- | --- |
| `01-on-the-band.jpg` | On the band | §5.1 |
| `02-sideways.jpg` | iPhone turned sideways | §5.1 |
| `03-tuning-dial.jpg` | Tuning dial | §5.1 |
| `04-spots.jpg` | Spots on the band | §5.1 |
| `05-tabs.jpg` | Tabs | §5.2 |
| `06-first-launch.jpg` | First launch | §5.3 |
| `07-connecting.jpg` | Connecting | §5.3 |
| `08-station-side.jpg` | The station's side | §5.3 |
| `09-takeover.jpg` | The takeover question (replaced on 2026-09-24 by `22`; kept for the record) | §3.4 D21 |
| `10-trouble.jpg` | When things aren't right | §5.3 |
| `11-amps-and-tuner.jpg` | Amps and tuner | §5.4 |
| `12-audio-and-data.jpg` | Audio and data use | §5.4 |
| `13-lock-screen.jpg` | Lock screen | §5.5 |
| `14-other-apps.jpg` | In another app | §5.5 |
| `15-hardware-ptt.jpg` | A hardware PTT button | §5.5 |
| `16-long-sessions.jpg` | Long sessions | §5.5 |
| `17-transmit-time-out.jpg` | Transmit time-out | §5.5 |
| `18-ipad-landscape.jpg` | iPad (on its side) | §5.6 |
| `19-ipad-upright.jpg` | iPad held upright | §5.6 |
| `20-spot-hub.jpg` | Spot Hub on the phone | §5.7 |
| `21-freedv-reporter.jpg` | FreeDV Reporter | §5.7 |
| `22-several-devices.jpg` | Several devices at once | §5.8 |
| `23-several-devices-states-and-notices.jpg` | Several devices: states and notices | §5.9 |
| `24-pairing-and-connecting-states.jpg` | Pairing and connecting: the states in between | §5.3 |
| `25-pan-and-display-sheets.jpg` | The Pan and Display buttons | §5.1 |
| `26-tuning-on-the-band.jpg` | Tuning on the band | §5.1 |

`board.html` in the same folder is the whole interactive board: the knobs
turn, the PTT keys, the flags fold, and the lock-screen states step through.

On 2026-09-24 the board's on-screen words were brought to D43 (board v53):
"the Core" for the NereusSDR computer, "station" only in its ham sense. The
pictures were taken again from that board, `09-takeover.jpg` excepted.
