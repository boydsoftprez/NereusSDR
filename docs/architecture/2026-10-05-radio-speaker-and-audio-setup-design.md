# Radio speaker control and Audio Setup redesign

Status: design for review. JJ settled the decisions below in a brainstorm on
October 5, 2026, by looking at the mockups in
`2026-10-05-radio-speaker-and-audio-setup-design/`, and called for this spec
the same day. Nothing in it is built yet. Requirement IDs R-SPK-01 to R-SPK-24
are new here.

## The problem

When a radio with a built-in or external speaker is connected, NereusSDR
plays the band through the computer's speakers and the radio's speaker at
once. The only control for the radio's speaker is the header's master
volume, and it is shared: the receive mix NereusSDR sends to the radio's
codec is scaled by the master volume and silenced by the master mute
(`src/core/AudioEngine.cpp:2685-2705`, "At the master volume, or silence
while the master is muted, so the radio's stream never stops"). Turning one
down turns both down. Nobody can silence the radio's speaker and keep
listening on the computer, or the other way round.

On a headless Core it is worse. The Core's own master volume decides the
radio's speaker level, and no remote window or phone can reach it.

Thetis has the same coupling. It routes the receive audio to the radio's
codec whenever the radio is the audio device (`ChannelMaster/netInterface.c:1571-1575`
[v2.10.3.15]) and sets that mix's volume from master AF
(`Console/cmaster.cs:954-957` [v2.10.3.15], `CMSetAudioVolume`). NereusSDR
copied it faithfully (`src/models/RadioModel.cpp:20314-20326`). This design
departs from Thetis on purpose, because NereusSDR plays to more places at
once (several windows, a phone, a remote Core) and each needs its own level.

The second half of this design reorganises Setup > Audio, which grew five
pages with overlapping sections (see "Audio Setup today").

## What the operator gets

- Two independent volume controls in the header, side by side: **PC** in
  cyan (this computer's speakers) and **RADIO** in amber (the radio's
  speaker). Neither moves the other.
- Clicking either icon mutes it. The icon shows the muted state.
- RADIO is the same control everywhere: the Core owns it, and every desktop
  window and the iPhone show and change the same value.
- On radios with a switchable speaker amplifier, muting RADIO also switches
  the amplifier off, and Setup offers Normal / Off while transmitting /
  Always off.
- The VFO flag's speaker tab shows when that slice is muted.
- Every desktop emoji icon becomes one of our own drawn icons, so the app
  looks the same on Mac, Windows and Linux.
- Setup > Audio becomes Outputs, Microphone, Digital modes, TX Profile and
  Advanced, with each setting in exactly one place.

## Decisions

Each decision is JJ's unless marked as a design choice of this spec; those
are listed again under "Design choices to confirm".

| # | Decision | Reason |
|---|---|---|
| D1 | Two separate controls, PC and RADIO, side by side (layout A of `header-layouts.html`), with the short word labels "PC" and "RADIO". | JJ picked A over icon-only (B) and stacked (C): both sliders keep full size and the word tells them apart at a glance. |
| D2 | PC keeps today's master volume and mute, saved keys and behaviour for this computer's speakers. Headphones stay exempt from it, as today. | No change to what PC already means locally. |
| D3 | RADIO is adjustable everywhere: owned by the Core, mirrored to desktop remote windows and the iPhone. | The speaker is at the radio, so one value for everyone; JJ chose this over "Core window only". |
| D4 | Upgrade default: RADIO starts at the current master level, unmuted. | Nobody hears a change on the day they upgrade. |
| D5 | Clicking the PC or RADIO icon toggles that mute. | Kept from today's header. |
| D6 | The VFO flag's speaker tab icon follows the slice's mute. | Today it is always the playing speaker, even on a muted slice. |
| D7 | Our own SVG icons replace every desktop emoji: speaker on and muted, radio on, muted and unavailable, locked and unlocked padlock, and the feature-request bulb. They are drawn in the Mac emoji style: a chrome speaker with cyan waves, an amber vintage receiver, a brass padlock with a chrome shackle and a glowing bulb (`icons.html`, `icons-qt-render.png`). | Emoji come from the system font, so they look different on Windows and Linux. JJ rejected flat, embossed and outline styles, and approved this set. |
| D8 | On boards with the speaker amplifier switch, muting RADIO also switches the amplifier off. | A muted speaker should be silent, not hissing. |
| D9 | Setup gets the three-way amplifier choice: Normal / Off while transmitting / Always off. The default is Normal. It is greyed out, with the reason, on boards without the switch. | JJ chose this over a single "disable the amplifier" box. It is piHPSDR's set of choices (see "Source facts"). |
| D10 | When there is no radio speaker to control, the RADIO group is disabled with a tooltip, never hidden. | Disabled-not-hidden rule. See D11 for when that is. |
| D11 | **Correction found while writing this spec.** The brainstorm treated an HL2 without its AK4951 audio add-on as "no speaker". NereusSDR cannot tell: the HL2's discovery reply carries no field for the add-on (`src/core/BoardCapabilities.h:511-521`, Hermes-Lite2 `usopenhpsdr1.v:254-314` @7472bd1). The HL2 therefore keeps RADIO enabled, with a note that it needs the audio add-on board, exactly as Radio Mic already does (`BoardCapabilities::radioMicNeedsAddOn`). RADIO is disabled only when no radio is connected, or when the Core is too old to support it (R-SPK-06). | We can't infer what the hardware doesn't report; this follows the existing Radio Mic precedent. |
| D12 | iPhone: the Sound panel gets a "Radio speaker" section between the mute switch and "Play the band through": an amber slider and "Mute radio speaker". The existing "Mute" becomes "Mute this phone". The phone keeps SF Symbols (`phone-sound-panel.html`). | Same control as the desktop's RADIO, in the panel the band's speaker button already opens. |
| D13 | Setup > Audio is grouped by where the sound goes: Outputs, Microphone, Digital modes, TX Profile, Advanced (`audio-setup.html`). | JJ chose this over keeping five pages or adding a quick-setup page. |
| D14 | Rarely changed device settings fold under "Device details" on Outputs and Microphone. | JJ approved: the volume and device you change most stay in front. |
| D15 | One layout on Mac, Windows and Linux. Differences are single rows or status lines inside it (see "Platforms"). | JJ asked for the same look everywhere. |
| D16 | VAX Device row: Mac and Linux show NereusSDR's own device by name only. Windows picks an installed virtual cable. | NereusSDR brings its own VAX devices on Mac (a bundled driver) and Linux (devices it creates in PipeWire or PulseAudio). Windows has none only because of driver-signing cost. |
| D17 | The ANAN-G2E shows the amplifier choice greyed out, with its own reason, until JJ bench-tests it (V-HW-6). | No reference sends the switch to a G2E at its latest (see "Source facts"), so we don't ship a control that may do nothing. JJ chose this over including it now or leaving it out for good. |

## Source facts

All cites at Thetis v2.10.3.15 (`3759d09`) and piHPSDR `4aa95c5`.

- The amplifier bit is P2 high-priority byte 1400, bit 1, written as the
  inverse of "amplifier enabled":
  `packetbuf[1400] = xvtr_enable | (!audioamp_enable) << 1 | atu_tune << 2;`
  (Thetis `ChannelMaster/network.c:1028`). piHPSDR names the same bit
  `ANAN7000_HIPRIO1400_SPKR_MUTE 0x00000002 // Enable/mute audio (1 = mute)`
  (`src/alex.h:129`).
- NereusSDR does not write byte 1400 today. `P2CodecOrionMkII.cpp:286-289`
  quotes the Thetis line as "not ported here", so the byte is zero: the
  amplifier is on, the transverter-out bit is off and the ATU-tune bit is
  off. Writing bit 1 must leave bits 0 and 2 at zero, as now.
- Thetis's control is one checkbox, `chkDisableRearSpeakerJacksAudioAmplifier`
  (`Console/setup.cs:22897-22900`), which sets `console.EnableAudioAmplifier`
  and calls `NetworkIO.SetAudioAmpEnable` only when
  `HardwareSpecific.HasAudioAmplifier` (`Console/console.cs:46856-46862`,
  `ChannelMaster/netInterface.c:1103-1107`).
- `HasAudioAmplifier` is true only on Protocol 2 with ANAN-7000D, ANAN-8000D,
  Anvelina Pro 3, ANAN-G2, ANAN-G2 1K or Red Pitaya
  (`Console/clsHardwareSpecific.cs:459-467`).
- piHPSDR offers "Spkr Amp": On / Mute on TX / Off (`src/radio_menu.c:270-295`,
  `727-746`), shown for `NEW_DEVICE_ORION2`, `NEW_DEVICE_SATURN` and
  `NEW_DEVICE_G2E` (`src/radio_menu.c:720-725`). Its sender
  (`src/new_protocol.c:868-882`) sets the mute bit when the choice is Off,
  or when it is Mute on TX while transmitting, **except** in CW (CWL, CWU)
  and while tuning, "if we expect a side tone from CW or TUNEing". Its
  comment also records that muting the amplifier "affects speakers,
  headphone, and LineOut", and that switching it makes the speakers "pop".
- The ANAN-G2E, checked at each reference's latest, not only the pins
  (Thetis `852bf0e`, mi0bot-Thetis `0cef1c9`, piHPSDR `2efb67f`, deskHPSDR
  `e519024`, all fetched 2026-10-05). Thetis knows the G2E (`ANAN_G2E`,
  "N1GP G2E added", `enums.cs:130`) but still leaves it out of
  `HasAudioAmplifier` (`clsHardwareSpecific.cs:459-467`, unchanged on
  master); mi0bot-Thetis is the same (`clsHardwareSpecific.cs:488-496`).
  piHPSDR shows "Spkr Amp" for `NEW_DEVICE_G2E` (`radio_menu.c:720-721`,
  added when the G1 was renamed G2E in `dc67fa2`), but its sender sets the
  bit only inside `if (device == NEW_DEVICE_ORION2 || device ==
  NEW_DEVICE_SATURN)` (`new_protocol.c:805-829` at `2efb67f`), so on a G2E
  the menu sends nothing. deskHPSDR is the same: it added `NEW_DEVICE_G2E`
  to its "Mute Spkr Amp" menu (`radio_menu.c:941-945`, `b0d0e99` "Add G2E
  support"), but its sender also sets the bit only for `NEW_DEVICE_ORION2`
  and `NEW_DEVICE_SATURN` (`new_protocol.c:1718-1733`), and the G2E is its
  own device id (`NEW_DEVICE_G2E 1020`, `discovered.h:60`).
  Neither pinned gateware covers the G2E. So no reference switches a G2E
  amplifier on the wire, and none says whether the board has one.

## Behaviour

### The two levels

R-SPK-01. The radio's codec feed is scaled by the RADIO level and silenced
by RADIO mute, and by nothing else. The PC level and PC mute no longer
touch it. The feed keeps flowing while muted (zeros), as today, so the
radio's audio stream never stops.

R-SPK-02. PC level and PC mute keep today's behaviour for this computer's
speakers, including the existing keys `audio/Master/Volume` and
`audio/Master/Muted`. Headphones stay exempt from PC.

R-SPK-03. RADIO level is 0 to 100 and maps to gain the same way PC does
today, so 72 on RADIO sounds like 72 on PC did.

R-SPK-04. Each slice's own AF level and mute still apply to both outputs, as
they do today.

R-SPK-05. Upgrade: when a radio has no saved RADIO level, it starts at this
station's current `audio/Master/Volume`, unmuted, amplifier Normal.

### Availability

R-SPK-06. RADIO is enabled whenever a radio that carries radio audio is
connected (`carriesRadioAudio()`, true for every P1 and P2 board today). It
is disabled, with a tooltip, when:

- no radio is connected: "No radio connected";
- in a remote window or on the phone, the Core does not support it
  (R-SPK-14): "This Core can't set the radio speaker. Update the Core."

R-SPK-07. On a Hermes Lite 2, RADIO stays enabled, and its tooltip and Setup
status line say that the headphone output needs the audio add-on board (D11).

### The amplifier

R-SPK-08. The amplifier choice is offered on exactly the boards where
Thetis's `HasAudioAmplifier` is true: Protocol 2 with ANAN-7000D, ANAN-8000D,
Anvelina Pro 3, ANAN-G2, ANAN-G2 1K or Red Pitaya. Anywhere else it is shown
greyed out with the reason ("This radio has no switchable speaker amplifier.").
The ANAN-G2E is not on the list: at their latest, Thetis and mi0bot-Thetis
leave it out, and the G2E menu entries in piHPSDR and deskHPSDR never reach
the wire (see "Source facts"). On a G2E the choice is greyed out with its
own reason ("Not yet tested on the ANAN-G2E.") and the byte stays as today
(D17). It is turned on in a later change once V-HW-6 passes.

R-SPK-09. The bit sent is "amplifier off" when any of these hold:

- the choice is Always off;
- RADIO is muted (D8);
- the choice is Off while transmitting, the radio is transmitting, the mode
  is not CW, and Tune is not active (piHPSDR `new_protocol.c:877-882`).

Otherwise the bit says "amplifier on". On boards without the switch the
byte stays as today (zero).

R-SPK-10. Setup explains the choice under it, in operator wording: the
amplifier feeds the radio's speaker jacks, Off while transmitting keeps it on
for CW and Tune so the sidetone is heard, and switching it can make a pop.
While it is off, a status line says so and why ("Amplifier is off now:
radio speaker muted." / "...: transmitting." / "Amplifier is off now.").
Whether it also silences the radio's headphone and line-out jacks (piHPSDR's
comment says it does) is checked on the bench before the wording names those
jacks (V-HW-2).

### Where it lives and how it travels

R-SPK-11. RADIO level, RADIO mute and the amplifier choice are Core-owned
state on `RadioModel`, as three new properties: `radioSpeakerVolume` (int,
0 to 100), `radioSpeakerMuted` (bool) and `speakerAmplifierMode` (int:
0 Normal, 1 Off while transmitting, 2 Always off). There are two read-only
reports beside them: `radioSpeakerAvailability` (int: 0 no radio,
1 available, 2 available but needs an add-on board) and
`speakerAmplifierAvailable` (bool).

R-SPK-12. Saved per radio, on the station that owns the radio, under
`hardware/<mac>/RadioSpeaker/Volume`, `.../Muted` (`"True"`/`"False"`) and
`.../AmplifierMode`. Like the other per-MAC keys, they are loaded on
connect. Connecting never keys and never changes the level the radio last
played at.

R-SPK-13. `MirrorPolicy` mirrors the three settable properties
Bidirectional and the two reports Outbound, so a remote window or the phone
changes the Core's value, and every other window and phone follows it.

R-SPK-14. The five properties go only to a peer that declared the new
feature `radioSpeaker` 1, through a `MirrorPolicy::featureGates()` entry
for each, as `paTransmitBand` and `txInhibitReason` do. A client connected
to a Core without the feature shows RADIO disabled (R-SPK-06). A Core never
sends them to an older client, which keeps working as today.

R-SPK-15. Threads: the GUI writes `RadioModel` on the main thread. The
level and mute reach the audio thread as `std::atomic` values in
`AudioEngine`, read once per block, with no lock in the audio callback.
The amplifier bit is worked out on the connection thread, from the
amplifier choice and mute (given through a queued setter, like the
connection's other settings) and the transmit, mode and Tune state it
already sends.

R-SPK-16. On a remote window, PC is this computer and RADIO is the
speaker at the Core. The RADIO tooltip says so ("Radio speaker at the Core
(shared with every window and the phone)"). On the desktop running the Core,
both are local and RADIO is still the shared value.

### Header

R-SPK-17. `MasterOutputWidget` becomes the PC group, plus a new RADIO group
beside it. Each group is icon, word label, slider (100 px) and readout, in
the existing `kSliderStyle` and `kDbLabelStyle`; RADIO's slider fill is
amber. They sit in the title bar strip (height 32) where the master volume
is today, followed by the feature-request bulb. Disabled RADIO shows the
greyed radio icon and the readout "--".

### VFO flag

R-SPK-18. The flag's speaker tab (`VfoWidget.cpp:1250`, today the fixed
speaker emoji) shows the speaker icon while the slice plays and the muted
speaker icon while it is muted, and follows the slice's mute from any
source (the flag's own mute button, the RX applet, a remote window or the
phone). The tab opens the audio controls as it does today.

### Icons

R-SPK-19. The approved SVGs ship as resources, generated from
`icons/gen_icons.py` (64 by 64 viewBox, gradients and layered shapes only,
because Qt SVG renders SVG Tiny and has no filters):

| Icon | Replaces |
|---|---|
| `pc-on.svg`, `pc-muted.svg` | the speaker emoji in `MasterOutputWidget.cpp:68-69` and `VfoWidget.cpp:1250` |
| `radio-on.svg`, `radio-muted.svg`, `radio-none.svg` | new, for RADIO |
| `lock.svg`, `unlock.svg` | the padlock emoji in `RxApplet.cpp:672-683`, `2009-2010` and `2125-2126` |
| `bulb.svg` | the bulb `TitleBar.cpp` paints with QPainter today, for one consistent set |

They render through `QSvgRenderer` at the button's size and device pixel
ratio (`icons-qt-render.png` shows Qt's own render at 96, 32 and 18 px).

### iPhone

R-SPK-20. The Sound panel (`ios/NereusApp/Audio/SoundPanel.swift`)
renames "Mute" to "Mute this phone", with no change to what it does, and
adds the Radio speaker section: the amber slider with readout and "Mute
radio speaker", bound to the mirrored `RadioModel` properties. When
unavailable it is disabled with the same reasons as R-SPK-06 and R-SPK-07,
shown as a note under it. The amplifier choice is in the phone's Setup
through the described Outputs page (R-SPK-23), not in the Sound panel.

## Audio Setup today

Rendered from the real pages (`audio-setup-today.png`): Devices, TX Input,
VAX, TCI, Advanced, then TX Profile. Problems found:

1. The PC microphone is set up on two pages (Devices' "TX Input
   (Microphone)" and TX Input's "PC Mic"), each with its own "Retry
   microphone".
2. Nothing covers the radio's speaker. The note at the bottom of Devices
   (`AudioDevicesPage.cpp:75-77`) says the master volume controls only this
   computer, which is wrong today.
3. Advanced, TCI and TX Input add their groups after the trailing spacer
   `SetupPage` puts in its content layout (`SetupPage.cpp:113`), so on a tall
   window the content sinks to the bottom under an empty band.
4. The VAX page says "PipeWire sources" on every computer
   (`AudioVaxPage.cpp:1002`, `1010`).
5. The VAX "Level" bar is only a meter.
6. TCI's "Master Mute Behavior" box holds no setting, only a paragraph.
7. `AudioBackendStrip` sits on top of every Audio page on every system
   (`SetupDialog.cpp:1332-1345`). On a Mac or Windows it reads "None ·
   Linux audio backend not available on this platform", with a disabled
   Rescan (`AudioBackendStrip.cpp:65-69`, `99-101`).
8. Exclusive, Event-driven and Bypass mixer are WASAPI only (their tooltip
   says so, `DeviceCard.cpp:351-362`) but are shown and tickable everywhere.
9. Windows VAX: each card's device picker is hidden on every system
   (`AudioVaxPage.cpp:226-228`), yet on Windows "On" stays disabled until a
   cable is picked (`AudioVaxPage.cpp:704-713`). The only way to choose or
   change a cable is the first-run dialog or a Rescan that finds a new one
   (`VaxFirstRunDialog`).

## Audio Setup redesigned

R-SPK-21. The Audio category holds five pages, in this order. Each control
appears on one page only. Content starts at the top of every page (fixes
problem 3 for every page, including those outside Audio that make the
same mistake, by inserting before the spacer as `SetupPage` intends).

**Outputs**

- A "Sound system" status line at the top. It replaces the strip on every
  page (problem 7). Mac: "Core Audio". Windows: "Windows audio", naming
  WASAPI as the default driver. Linux: PipeWire (direct), PulseAudio
  (through pactl, used when PipeWire is not found), or "None found" in red
  with what to start (from `LinuxAudioBackend`: PipeWire, Pactl, None).
- **This computer** (cyan): Volume with Mute (the same control as PC in the
  header), Device, then Device details folded: Driver API, Sample rate,
  Channels, Buffer size with its milliseconds, Options, Negotiated.
- **Headphones**: Enabled, Device, Device details. Greyed until Enabled.
  The note says the PC volume does not affect headphones.
- **Radio speaker** (amber): a status line naming what the radio has, then
  Volume with "Mute radio speaker" (the same control as RADIO), the note
  that slice AF and mute still apply, and Speaker amplifier with its three
  choices, explanation and live status (R-SPK-08 to R-SPK-10). In a remote
  window the text says the speaker is at the Core and changes reach every
  window and the phone.
- One "Rescan devices" button for the page. On Linux it also checks the
  sound system again (`rescanLinuxBackend`).

**Microphone**

- Source: PC mic / Radio mic / VAX TX. The sections not picked stay
  visible but greyed out. Radio mic follows `radioMicSelectable()` and its
  existing reasons.
- **PC microphone**: Device, Test Mic with its meter, capture status with
  one "Retry microphone", and Device details folded.
- **Radio microphone (board name)**: the existing per-board group
  (Hermes / Atlas, Orion-MkII, Saturn G2, HL2), unchanged.
- **Mic gain**, which applies to whichever source is picked.

**Digital modes**

- VAX: one sentence for this system (see "Platforms"), a status line, four
  cards. Each card: On, Device, Format, Used by, Activity (renamed from
  Level, problem 5), Rename and Copy name. Below the cards: "Detected
  virtual cables" with Rescan (moved from Advanced).
- TCI: one sentence saying TCI audio is separate from the PC and radio
  speaker volumes (replaces problem 6's box), then Audio stream (Slice A
  rate, Format, Channels, Block size, the C and D note) and Transmit (TX
  channel, TX buffering).
- The Opus note for remote windows stays, as today.

**TX Profile**: unchanged.

**Advanced**: Logs ("Open logs folder", moved from the strip) and Reset.
The DSP group stays hidden until it is built, as today (`UnbuiltFeatures`).
Feature Flags stays as today.

R-SPK-22. Every existing setting keeps its saved key and its
`nereusSetupId`, so nothing a user has set is lost and nothing the phone
or a remote window looks up breaks. Only labels, grouping and folding
change. The Devices page's mic group and TX Input's PC Mic group become
the one PC microphone section; both wrote the same device settings.

R-SPK-23. The Setup description (`resources/setup/audio.json`, version 24)
keeps the page id `audio.txInput`, retitled "Microphone", and gains a
page `audio.outputs` with the Radio speaker section (level, mute,
amplifier choice) bound to the new `RadioModel` properties, `where:
station`, gated on `radioSpeaker` 1. The version goes to 25.

## Platforms

R-SPK-24. One layout on every system (D15). The differences:

| | Mac | Windows | Linux |
|---|---|---|---|
| Sound system line | Core Audio | Windows audio (WASAPI default) | PipeWire, PulseAudio through pactl, or none found |
| Driver API list | what PortAudio reports (Core Audio) | what PortAudio reports (WASAPI, DirectSound, MME...) | what PortAudio reports (PipeWire, PulseAudio, ALSA...) |
| Exclusive / Event-driven / Bypass | greyed, "These three work only with WASAPI on Windows." | live when the card's driver is WASAPI, greyed otherwise | greyed, as on Mac |
| VAX sentence | NereusSDR VAX 1 to 4 come from NereusSDR's own driver | each channel uses a virtual cable you install (VB-CABLE, Voicemeeter or VAC) | NereusSDR creates VAX 1 to 4 in the sound system |
| VAX Device row | the name only | a picker of detected cables, "On" disabled until one is picked (fixes problem 9) | the name only |
| VAX failure line | the driver was blocked: allow it in System Settings > Privacy & Security and restart | no cable found: install one, then Rescan | no sound system running, so the devices cannot be made |

The existing per-platform status texts in `AudioVaxPage.cpp:589-702`
supply these meanings; they are reworded for operators, without the cites
and internal names.

## Out of scope

- A NereusSDR VAX driver for Windows.
- Per-slice routing to the radio speaker (which slices play there). RADIO
  plays the same mix the radio gets today.
- The DSP rate and block size group, which stays hidden until built.
- Changing the headphones' exemption from PC volume.
- The iPhone's own Setup > Devices page and its icons (SF Symbols stay).
- Any control for the radio's own front-panel volume knob, where a radio has
  one; this design controls what NereusSDR sends.

## Verification

Software tests (this machine, offscreen):

- V-SW-1. AudioEngine: with RADIO and PC set differently, the radio tap
  receives the mix at the RADIO gain and the speakers at the PC gain; each
  mute silences only its own output; the radio tap keeps receiving blocks
  (zeros) while muted. (R-SPK-01 to R-SPK-04)
- V-SW-2. Upgrade: a profile with `audio/Master/Volume` 0.72 and no RadioSpeaker
  keys loads RADIO 72, unmuted, Normal; a profile with saved keys loads
  those. (R-SPK-05, R-SPK-12)
- V-SW-3. P2 high-priority packet byte 1400: zero on boards without the
  switch; bit 1 set exactly in the R-SPK-09 cases on a G2 (Always off;
  muted; Off while transmitting while transmitting in SSB; and not set in
  CW or Tune); bits 0 and 2 stay zero. (R-SPK-08, R-SPK-09)
- V-SW-4. Mirror: the three properties round-trip between a Core and a
  remote RadioModel; they are withheld from a peer that did not declare
  `radioSpeaker` 1; a client of an older Core shows RADIO disabled with the
  update reason. (R-SPK-06, R-SPK-13, R-SPK-14)
- V-SW-5. Availability: no radio gives disabled with "No radio connected";
  HL2 gives enabled with the add-on note; amplifier availability matches
  the R-SPK-08 board list for every `HPSDRModel` on P1 and P2.
  (R-SPK-06 to R-SPK-08)
- V-SW-6. Setup pages: each control id from today's pages appears exactly
  once across the new pages; every page's first group is at the top on a
  tall window; the Outputs status line and VAX rows match each platform's
  build. (R-SPK-21, R-SPK-22, R-SPK-24)
- V-SW-7. Setup description version 25 validates, keeps `audio.txInput`,
  and describes `audio.outputs`. (R-SPK-23)
- V-SW-8. `tst_core_has_no_gui_includes` stays green (the new state is on
  `RadioModel` and `AudioEngine`, not in the GUI).

UI checks (rendered pixels, by the repository's UI gate):

- V-UI-1. Header at 1x and 2x on Mac, Windows and Linux: both groups, all
  icon states, disabled RADIO, matching `header-layouts.html` layout A and
  `icons.html`.
- V-UI-2. VFO flag tab while playing and muted, muted from each source.
- V-UI-3. RX applet padlocks and the bulb with the new icons.
- V-UI-4. Each Audio page against `audio-setup.html`, folded and unfolded,
  local and remote.
- V-UI-5. iPhone Sound panel against `phone-sound-panel.html`, available
  and unavailable, in the simulator.

Hardware (a person at the radio):

- V-HW-1. ANAN-G2 on P2: RADIO and PC independent by ear; RADIO mute is
  silent at the speaker and the amplifier is off; each amplifier choice
  behaves as R-SPK-09 during SSB, CW and Tune.
- V-HW-2. On the G2, whether amplifier off also silences the headphone
  and line-out jacks; the R-SPK-10 wording follows the result.
- V-HW-3. A P1 board with a codec output (ANAN-100D or similar): RADIO
  level and mute by ear; the amplifier choice greyed out.
- V-HW-4. HL2 with the AK4951 board: RADIO level and mute at its
  headphone output.
- V-HW-5. Remote: a desktop remote window and the iPhone move the same
  RADIO value on a Core with a G2, and each follows the other.
- V-HW-6. ANAN-G2E on P2 (JJ's bench): with a test build that sends the
  amplifier bit, whether "amplifier off" silences the G2E's speaker. Pass
  adds the G2E to the R-SPK-08 list; fail leaves D17 as it is.

## Design choices to confirm

These are this spec's choices, not settled in the brainstorm. JJ's review
confirms or changes them:

1. D11: the HL2 keeps RADIO enabled with the add-on note, because the
   radio can't report the board.
2. R-SPK-09: Off while transmitting stays on for CW and Tune, as piHPSDR.
3. R-SPK-12: RADIO is saved per radio (per MAC), not once per station.
4. R-SPK-23: the amplifier choice reaches the phone through its Setup
   (described Outputs page), not the Sound panel.

## Mockups

In `2026-10-05-radio-speaker-and-audio-setup-design/`:

- `header-layouts.html`: layouts A, B and C, and A's states; A is chosen.
- `icons.html` and `icons-qt-render.png`: the approved icon set, in the
  browser and as Qt renders it; the SVGs and their generator are in
  `icons/`.
- `phone-sound-panel.html`: the iPhone Sound panel.
- `audio-setup.html`: the interactive Setup > Audio redesign with the header,
  switchable by radio, window, transmit state and platform.
- `audio-setup-today.png`: today's five pages, rendered from the code.

The mockups were updated for D11: the "Hermes Lite 2, no audio board" state
became "No radio connected".
