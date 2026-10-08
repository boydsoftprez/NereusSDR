# Native audio engines and live device lists

Status: draft for JJ's review, October 8, 2026. JJ settled the decisions
below in a brainstorm on October 7 and 8, 2026, partly by looking at the
mockups in `2026-10-08-native-audio-engines-design/`, and called for this
spec on October 8. Nothing in it is built. It builds on the radio speaker
spec (`2026-10-05-radio-speaker-and-audio-setup-design.md`, merged in PR
355), whose Setup > Audio pages it changes. Requirement IDs R-AUD-01 to R-AUD-34 are new
here.

**This work touches the core receive audio path (I/Q → WDSP → audio).**
Every speaker and headphone stream moves to a new engine, and a
clock-matched buffer is added between the DSP and each device. The
receive DSP itself (WDSP channels, demodulation, AF and the master mixer)
does not change.

## The problem

AirPods connected in the middle of a session never appeared in the device
lists, not even after Rescan. That is how NereusSDR's audio layer works
today, on every platform:

- NereusSDR opens this computer's speakers, headphones and Windows VAX
  cables through PortAudio v19.7.0 (`CMakeLists.txt:526-530`), which builds
  its device list once, in `Pa_Initialize`. Refreshing it needs
  `Pa_Terminate`, which closes every open stream (`pa_front.c:343-349`,
  `:398-415`). The upstream hotplug branch (2016) was never merged, and a
  maintainer said in 2022 that terminate-and-initialize is the only way.
  NereusSDR's Rescan does neither: it only re-detects the Linux backend
  (`AudioEngine::rescanLinuxBackend`, `AudioEngine.cpp:503`).
- On Windows, PortAudio's default driver is MME (`pa_win_hostapis.c:72-96`).
  Its WASAPI support polls in shared mode, with a floor of about 10 ms, and
  has no low-latency shared mode. The Exclusive checkbox in Setup never
  reaches the stream (`AudioEngine.cpp:836-843`; no host-specific stream
  info, `PortAudioBus.cpp:344`).
- On Linux the vendored PortAudio has only ALSA, JACK and OSS
  (`CMakeLists.txt:549-552`), so a Bluetooth headset on a PipeWire desktop
  never shows as its own device.
- Between the DSP and each device sits our own output ring, 10 to 40 ms
  in normal use with a 100 ms cap (`PortAudioBus.cpp:235-248`,
  `PortAudioBus.h:205`), with no clock matching. The radio's clock and the
  sound card's clock differ slightly, so the ring slowly fills toward its
  cap or runs dry.
- The PC mic reaches the transmitter through the `nereus-audio-capture`
  helper process in 480-frame records on a pipe, pumped every 10 ms
  (`CaptureHelper.cpp:144`). By reading the code, that hop adds roughly 10
  to 20 ms; it has not been measured.
- A headless Core can't be told which sound card to play on except in its
  config file, and as shipped it can't open a sound card at all: its
  systemd unit runs as a dynamic user with no `audio` group
  (`packaging/nereusd.service.in:135-149`). No window can see or change
  the Core's own speaker (`remote-controls.md` rows 142 and 160 cover this
  computer only).

Found along the way, and fixed by this design:

1. A saved WASAPI choice reopens on MME: the saved device is matched by
   name across every driver, first exact match wins, and MME comes first
   (`PortAudioBus.cpp:939-977`; callers `AudioEngine.cpp:401`, `:1135`,
   `MainWindow.cpp:1421`).
2. The Exclusive, Event-driven and Bypass mixer checkboxes do nothing
   (above).
3. The VAX first-run dialog's "Rescan now" button is wired to nothing
   (`VaxFirstRunDialog.cpp:647-653`).
4. On a Mac without a built-in mic, "(platform default)" falls back to the
   first input it finds (`PortAudioBus.cpp:207-237`), which can be AirPods,
   dropping them to phone-call quality.
5. The Mac DSP thread joins the default output device's audio workgroup
   only when it starts (`RealtimeAudioPriority.cpp:92-152`,
   `RxDspWorker.cpp:202-228`), so after a device switch it is tied to the
   wrong device.
6. `#ifdef __APPLE__` at `PortAudioBus.cpp:119` breaks the `Q_OS_MAC` rule.
7. `CMakeLists.txt:532-541` says the ASIO SDK is GPL-incompatible. That has
   been out of date since Steinberg released ASIO SDK 2.3.4 under
   GPL-3.0-only (October 29, 2025), and so is section 8.5 of
   `2026-04-19-vax-design.md`.

## What the operator gets

- Device lists that update by themselves. AirPods, a USB interface or a
  virtual cable appear the moment the system sees them, in Setup, in the
  header's speaker menu and on the VAX page, and nothing that is playing
  stops.
- A device that goes away is shown as "not connected". Speakers and
  headphones keep playing on the system default meanwhile; the mic and VAX
  go silent rather than switching to something else. Everything switches
  back by itself when the device returns.
- The lowest latency each system allows: Core Audio on the Mac, Windows'
  own low-latency shared mode (or exclusive mode, or ASIO) on Windows,
  PipeWire or PulseAudio on Linux, and ALSA straight to the sound card on
  a headless Core.
- A "Delay" line in each device's details: what the path from the radio
  to that device takes now, in milliseconds, with a manual override.
- ASIO on Windows, with each channel pair of an interface as its own
  entry, and the same for multi-channel interfaces on Mac and Linux.
- The Core's own speaker, chosen and checked from any connected window,
  with its level and mute on the phone too.

## Decisions

Each decision is JJ's. The design choices this spec made on its own are
listed under "Design choices to confirm".

| # | Decision | Reason |
|---|---|---|
| D1 | Speakers, headphones, the PC mic and Windows VAX move to native engines: Core Audio (Mac), Windows audio shared and exclusive, and ASIO (Windows), PipeWire and PulseAudio (Linux desktops), and ALSA direct (the headless Core). PortAudio stays as the "Older drivers" choice. | Live lists need the system's own device-change notices, which PortAudio does not have; and JJ's bar is the lowest latency on every system. JJ: "1, native engines on all three", widened by D3, D16 and D18. |
| D2 | A clock-matched buffer (WDSP's rmatchV) on the local DSP-to-device path. | Today that path has none, so its fill drifts (above). Remote playback and Thetis's VAC and ASIO paths already use rmatchV. |
| D3 | Native ASIO is its own engine on Windows, studied from Thetis's cmASIO and ported where it fits, with its headers. | JJ raised cmASIO; the GPL-3 ASIO SDK makes it possible ("1 yes"). |
| D4 | A chosen speaker or headphone device that goes away: keep playing on the system default, show "not connected" in Setup and the speaker menu, and switch back by itself when it returns. | The band keeps playing; nothing to redo when the device comes back. |
| D5 | A chosen PC mic that goes away is never replaced by another mic. It goes silent, the transmit panel says so, and it resumes by itself when it returns. | Transmitting on a mic the operator did not pick is worse than silence. |
| D6 | NereusSDR never opens a Bluetooth mic on its own, not even through "(platform default)". Picked by name, it works, with a plain note that the headphones drop to phone-call quality while it is the mic. When the same headset is mic and speakers, the mic opens first. | Opening a Bluetooth mic drops the whole headset to the hands-free profile. freedv-gui opens the mic first for the same reason. |
| D7 | The clock-matched buffer sizes itself (the smallest size that runs without clicks, one step up on a dry run), shows the current delay in milliseconds in Device details, and has a manual delay override. | Lowest delay without the operator tuning it, and a way out when automatic is wrong. |
| D8 | Windows with no saved choice uses Windows audio in low-latency shared mode. Exclusive mode and ASIO are one choice away in Setup. | Lowest latency that still lets other apps play. |
| D9 | Every ASIO channel pair is its own entry in the speaker, headphone and mic lists ("Focusrite USB ASIO · Outputs 3-4"). The mic keeps cmASIO's left, right or both. Speakers and headphones can share one interface on different pairs. Choosing a second ASIO driver asks to move every ASIO device to it together. | One ASIO driver runs at a time (D15). JJ approved the mockup ("this all looks good"). |
| D10 | One Driver list replaces the Driver API list and the three WASAPI checkboxes: "Windows audio, shared", "Windows audio, exclusive", "ASIO", then "Older drivers". | The checkboxes did nothing, and the list says what each choice is. |
| D11 | Migration: a choice left at the default moves to the new engine on the same device; a saved WASAPI choice moves too (Exclusive on becomes "Windows audio, exclusive"); an older driver picked by name stays. | Nobody loses a device on upgrade, and nobody who chose an older driver on purpose is moved off it. |
| D12 | Windows VAX channels move to the native Windows engine, with live lists and the D11 rule. A VAX channel can use an ASIO pair under the one-driver rule. | Same live lists for cables as for speakers. |
| D13 | A VAX channel whose cable or pair goes away goes silent, shows "not connected", and resumes when it returns. It is never moved to another cable. | Digital-mode apps listen on a specific cable. |
| D14 | When an ASIO driver asks for a restart (usually after a change in its own settings window), NereusSDR restarts its ASIO audio right away by itself. A driver that allows one buffer size shows it greyed with "Set in the ASIO control panel". | The driver's settings take effect without the operator doing anything. |
| D15 | ASIO buffer size and sample rate stay in each device's details, kept in step across every device on the driver, with "Buffer size and sample rate are shared with ... on the same ASIO driver." | One driver has one buffer size and one rate (`ASIOCreateBuffers` takes a single size, `asio.h:64-65`). JJ: "ok looks good". |
| D16 | Linux computers running PulseAudio rather than PipeWire get a native PulseAudio engine. | Ubuntu 22.04 and similar still run PulseAudio; piHPSDR uses it natively. |
| D17 | On Mac and Linux, a multi-channel interface lists each output and input pair as its own entry, as ASIO does. | Same as D9 on every system. JJ: "1 yes". |
| D18 | The headless Core plays to its sound card through ALSA directly, with a native engine that watches cards being plugged in and out. | A systemd service has no desktop session, so no PipeWire or PulseAudio. |
| D19 | The Core may open a sound card out of the box: its installers and station images grant sound access the way they already grant serial-port access. | A card plugged into the Pi just works. |
| D20 | The header PC icon's right-click menu lists only the speakers' current driver ("Speakers · Windows audio, shared"), pairs grouped under their interface, a missing chosen device ticked at the top in amber with "Playing on ... until it comes back.", and "Sound setup…" at the bottom. The PC tooltip names the device playing now. | Today's menu lists every PortAudio driver, which is long and has nowhere to show a missing device. JJ chose option A of the mockup. |
| D21 | R-R3-36's keying rule stays: with the PC mic missing, voice-mode MOX is refused with "Microphone is not ready. Check Audio settings and retry.", and losing the mic mid-transmission releases MOX. New: the transmit panel's source badge turns amber, "PC mic not connected". | The rule already protects the operator; the badge says why before MOX is pressed. JJ: "ok 1". |
| D22 | The PC mic stays in its helper process, but its hand-off becomes shared memory with a wake signal per buffer, as browsers do it. On the Mac the helper's audio thread joins the device's audio workgroup. Target: under 1 ms for the hop, measured. | Keeps R-R3-36's protection against a hanging mic, without the pipe's delay. |
| D23 | The Core's own speaker is chosen and checked from any window connected to that Core, with the same live list and "not connected" handling as this computer's. The config file's `audio_device` stays a starting value. | Today nobody can see or change it from a window. |
| D24 | It is a "Core speaker" card on Outputs, between Headphones and Radio speaker. | JJ: "option 1 as shown". |
| D25 | The phone gets only the Core speaker's level and mute, in its Sound panel under Radio speaker. Choosing the device stays on the desktop. | Parity is for function; setup machinery stays on the desktop. |
| D26 | When the Core has no sound card, the phone leaves that section out, and it appears by itself when one is plugged in. The desktop card shows either way. | Nothing to control; Setup is where one gets set up. |
| D27 | Speakers or headphones on "(platform default)" follow the computer's default output right away when it changes. | That is what "platform default" means; AirPods connecting is the common case. |
| D28 | A PC mic on "(platform default)" follows the default input too, but never during a transmission (it waits for unkey) and never to a Bluetooth mic. Any Bluetooth mic works when picked by name. | No surprise switches on the air, and D6. |
| D29 | A Bluetooth headset picked as the mic stays open the whole session, as today, so keying is instant and VOX works. The Setup note suggests the headset for listening and the computer's mic for talking. | Opening it only when keying would cut the start of every over. |
| D30 | A device that is connected but held by another program is handled like an unplugged one, and shows "in use by another program". | Same rule, same recovery, one less state to learn. |
| D31 | On a Core box that starts into a desktop, the Core leaves every sound card alone until a Core speaker is picked in a window. On a box without a desktop it uses its default card out of the box. | The desktop's own sound system uses the cards, and an ALSA card has one user at a time. |
| D32 | Bench machines: a Mac, the Pis, AirPods, a Windows PC, an ASIO interface on it, and a PipeWire desktop (current Ubuntu). There is no PulseAudio desktop, so that engine and its pair check stay untested on hardware. | What JJ has. |
| D33 | The ASIO driver runs in the mic helper process whenever any device uses ASIO, outputs included, over the same shared-memory hand-off as the mic. | Many ASIO drivers accept one program at a time, and the PC mic lives in the helper (D22), so ASIO in two processes would fail on those drivers. A hanging ASIO driver then cannot freeze the window either. The cost is one sub-millisecond hop on ASIO outputs, measured by V-HW-8. JJ, 2026-10-08: "1 in the helper", over ASIO in the window's process. |

## Source facts

### PortAudio, as pinned

- v19.7.0 (`147dd722`, 2021-03-31) is the pin (`CMakeLists.txt:526-530`,
  `cmake/NereusDependencyArchives.cmake:136-137`) and still the latest
  release; master at `873e3c8` (2026-10-02) has no refresh API either
  (`pa_mac_core.c:391` builds the list only at init).
- `Pa_Initialize` is reference-counted (`pa_front.c:156`, `:361`); the last
  `Pa_Terminate` closes every stream and invalidates every device index
  (`pa_front.c:343-349`, `:398-415`). NereusSDR's `AudioEngine` owns the
  main process's only pair (`AudioEngine.cpp:325-344`, `:442`); the mic
  helper has its own (`CaptureHelper.cpp:377-378`, `:500-505`).
- MME truncates names to 31 characters (`pa_win_wmme.c:706`, `:842`), and
  one device has a different name under each Windows driver.
- On the Mac, PortAudio adds no layer of its own: NereusSDR's streams are
  output-only or input-only at 128 frames (`PortAudioBus.cpp:377-380`,
  `PortAudioBus.h:70`), and the requested latency rounds to a 128-frame
  hardware buffer (`pa_mac_core.c:85`, `:557-569`, `:1645-1668`). The
  largest term there is our own ring (above).

### System notices

- Mac: a block listener on `kAudioHardwarePropertyDevices` runs on the
  queue it is given (`AudioHardware.h:382-389`, macOS SDK 27.0).
  NereusSDR already does this for its VAX driver on a private serial queue
  (`CoreAudioHalBus.cpp:105-260`, `:168-176`). A device's UID survives
  reboots (`AudioHardwareBase.h:643-649`).
- Windows: endpoint notifications (`IMMNotificationClient`) must not block,
  must not register or unregister inside the callback, and must not release
  the last reference (Microsoft Learn). Audacity posts each one to its event
  loop (`windowssystemaudiodeviceslistener.cpp:120-169`).
- ASIO has no device-arrival message: its host messages are reset, buffer
  size change (unsupported), resync, latency change and the like
  (`asio.h:428-452`, SDK 2.3.3 as vendored in Thetis). A reset request
  means "close the driver and open it again" (`asio.h:434-443`). When the
  minimum and maximum buffer sizes are equal, the driver allows one size
  (`asio.h:640-642`).
- PipeWire runs its callbacks on the thread loop's thread with the loop
  lock held (pw_thread_loop documentation).
- No reference measures how long a Bluetooth device takes to become
  openable after it connects. AetherSDR's 750 ms (`MainWindow.cpp:4182`,
  `:4197`) and Audacity's 500 ms (`macossystemaudiodeviceslistener.cpp:12-18`,
  `:75-82`) are both debounces.

### How others do it

- Audacity 4 (`1d87b70`) listens per system, debounces 500 ms without
  restarting the timer, waits while its engine is busy, then terminates and
  re-initializes PortAudio and restores its streams
  (`au3audiodrivercontroller.cpp:187-205`, `DeviceManager.cpp:430-446`).
- AetherSDR (`3a1f59ea`) uses Qt's device-change signals
  (`MainWindow.cpp:4186-4190`), saves Qt's device id, falls back to the
  default when a device vanishes (`MainWindow.cpp:4238-4247`), and never
  switches back.
- freedv-gui (`b299e2bb3`) moved to native engines for fewer dropouts and
  less overhead (PR 847), not for hotplug; it matches devices by name and
  its Refresh restarts the engine (`dlg_audiooptions.cpp:1146-1163`). Its
  PR 971 (`cf5001508`) opens a Bluetooth mic before the speaker. It detects
  Bluetooth by transport type (`MacAudioDevice.cpp:55-75`).
- deskHPSDR (`b74bd6d`) dropped PortAudio (`0b899f9`) for Core Audio on the
  Mac and miniaudio on Linux, polls each device's "is alive" flag every
  250 ms (`buffered_audio.c:416-473`) and does not reopen a returning
  device.
- piHPSDR (`336e5c9`) defaults to native PipeWire on Linux
  (`Makefile:47`), asks for a 256/48000 quantum (`pipewire.c:66-67`,
  raised from 128 for HDMI dropouts, `:60-63`) and enumerates once
  (`main.c:127`). Thetis (`852bf0e`) enumerates once too
  (`console.cs:1136`).
- Thetis cmASIO (`Project Files/Source/ChannelMaster/cmasio.c`
  [v2.10.3.15], W4WMT, GPLv2 or later, with the MW0LGE dual-licence
  statement) wraps Steinberg's host sample and puts an rmatchV in each
  direction (`cmasio.c:90-93`; 5 blocks unless set, `:84`). It never opens
  the driver's control panel (`hostsample.cpp:524`, commented out) and
  only stops on a reset request (`hostsample.cpp:279-285`).

### Latency figures

- Windows low-latency shared mode (`IAudioClient3`) accepts periods down to
  the engine's minimum (Microsoft Learn's example: 48 frames, 1 ms) in the
  device's own format. Exclusive mode with event callbacks goes lower but
  locks the device.
- PipeWire's default quantum is 1024 frames at 48 kHz (about 21 ms); a
  client asks for less with `node.latency`.
- deskHPSDR: 128 frames out and 256 in on Core Audio
  (`coreaudio.c:42-44`), ring target about 32 ms
  (`buffered_audio.c:119-123`). freedv-gui: 20 ms on the Mac, 10 ms WASAPI
  shared, 20 ms PulseAudio.

### NereusSDR today

- Every device stream is an `IAudioBus` (`src/core/IAudioBus.h`), made by
  `AudioEngine::makeBus` (`AudioEngine.cpp:815-849`): speakers, headphones
  and Windows VAX through `PortAudioBus`; Mac VAX through `CoreAudioHalBus`;
  Linux VAX through `PipeWireBus` or `LinuxPipeBus`. Native PipeWire
  speaker and sidetone outputs exist but are never called.
- A saved device is a name plus a driver name under `audio/Speakers`,
  `audio/Headphones`, `audio/TxInput` and `audio/Vax<N>`
  (`AudioDeviceConfig.cpp`), never an index.
- WDSP's rmatchV takes two critical sections inside `xrmatchIN` and
  `xrmatchOUT` (`third_party/wdsp/src/rmatch.c:309-360`, `:435-465`).
  NereusSDR therefore keeps it off device callbacks:
  `RemoteAudioRateMatcher` runs it on the receiver worker and refills the
  device's queue by the device's consumed-frame count
  (`RemoteAudioRateMatcher.h:148-155`, `IAudioBus::outputPacing`). Thetis's
  cmASIO calls it inside the ASIO callback; ours does not, because the
  project rule bars locks in audio callbacks.
- R-R3-36's keying rule: `RadioModel.cpp:21154-21162` refuses voice MOX
  while the PC mic is not ready; `RadioModel::onCaptureStatusChanged`
  (`:21237`) releases MOX when it is lost; `RadioModel::updatePcCaptureDemand`
  (`:25216`) holds the capture for the whole session while the mic source
  is PC.
- The Core: `audio_device` in `nereusd.conf` seeds `audio/Speakers/DeviceName`
  only when nothing is saved (`DaemonApp.cpp:947-970`). Station images add
  groups with drop-ins (`packaging/station-image/common/nereusd-serial.conf`,
  installed by `install-station.sh` and the pi-gen stage). The unit does
  not hide devices (no `PrivateDevices`), so the `audio` group is all it
  lacks. The Core exposes its radio speaker's level and mute
  (`StationServer.cpp:1545-1546`, `:1741`; `MirrorPolicy.cpp:1210-1211`,
  `:1330-1334`) but nothing about its own speaker.

## Architecture

### Engines

Each engine implements `IAudioBus` for its streams, so `AudioEngine`,
`MasterMixer`, the VAX mixer and remote playback keep talking to the same
interface. `AudioEngine::makeBus` chooses the engine from the saved
`Engine` key (R-AUD-04).

| Engine | Where | Output | Input | Device notices |
|---|---|---|---|---|
| Core Audio | Mac | AUHAL output unit with a channel map | AUHAL input unit (in the mic helper) | `kAudioHardwarePropertyDevices`, both default devices, each device's alive and hog-mode state |
| Windows audio, shared | Windows | `IAudioClient3` low-latency shared stream, event-driven | the same, in the mic helper | `IMMNotificationClient` |
| Windows audio, exclusive | Windows | exclusive, event-driven, the device's best format | the same, in the mic helper | `IMMNotificationClient` |
| ASIO | Windows | one driver, pairs as devices | the same driver | none of its own; the installed drivers are re-checked when Windows reports a device change, and reset requests restart it (R-AUD-21) |
| PipeWire | Linux | `pw_stream` with `node.latency` from the buffer size | the same, in the mic helper | registry listener on the thread loop |
| PulseAudio | Linux | `pa_stream` with small requested buffers | the same, in the mic helper | `pa_context_subscribe` |
| ALSA direct | Core | `hw:` PCM, the card's own format | not part of this design | inotify on `/dev/snd` |
| Older drivers | all | `PortAudioBus`, unchanged | unchanged | none; Rescan re-initializes PortAudio |

The ASIO driver runs in the mic helper process whenever any device uses
ASIO (D33), so the helper then runs even while the mic is not in use.
Engine callbacks run at the platform's real-time audio priority, which
`RealtimeAudioPriority` already provides for the DSP thread (Mac
workgroups, Windows MMCSS, Linux `SCHED_FIFO`).

ASIO is ported from cmASIO where it fits (driver loading, buffer creation,
the sample-format conversions), with cmASIO's headers byte-for-byte and a
`THETIS-PROVENANCE.md` row, and its inline comments kept. Where ours
differs: rmatchV runs off the ASIO callback (above), the driver's control
panel opens from Setup, and a reset request restarts the driver instead of
stopping it. The ASIO SDK 2.3.4 (GPL-3.0-only) is vendored under
`third_party/`; downloading it waits for JJ's yes during the plan.

### The device catalogue

A new `IAudioDeviceCatalog`, under `src/core/audio/` and free of GUI
headers (rule R1), is the one source for every device list:

- per engine, its outputs and inputs, each with its saved-identity key
  (below), display name, channel pairs, transport (built-in, USB,
  Bluetooth, HDMI, virtual), and state (present, not connected, in use by
  another program);
- the system's default output and input;
- `devicesChanged()` and `defaultChanged()` signals on the main thread.

Each engine posts its system notices to the catalogue's thread; nothing is
re-listed inside a system callback. A burst of notices is debounced (500
ms, not restarted by later notices), then the catalogue re-lists. A
device that reappears is opened with retries (250 ms, 500 ms, 1 s, 2 s,
then every 2 s while present), because a Bluetooth device can be listed
before it can be opened.

Setup's cards, the header menu, the VAX cable rows, the VAX first-run
dialog and `VirtualCableDetector` read the catalogue instead of calling
PortAudio enumeration. Older drivers' entries come from PortAudio's list,
refreshed only by Rescan.

### Saved identity

A saved device is the system's own ID plus its name:

| Engine | ID |
|---|---|
| Core Audio | device UID |
| Windows audio | endpoint ID string |
| ASIO | driver name and channel pair |
| PipeWire | `node.name` |
| PulseAudio | sink or source name |
| ALSA direct | card ID and device number |
| Older drivers | PortAudio host API name and device name, as today |

Matching runs ID first, then name within the same engine, never across
engines (that is the fix for bug 1). Two devices with one name are told
apart by ID.

### Clock matching and the delay readout

Each local output gets an rmatchV between the DSP and the device, built the
way remote playback's already is: the DSP side writes into rmatchV on the
audio thread that feeds the device today, and refills a lock-free queue
(`AudioRingSpsc`) by the device's consumed-frame count; the device callback
only reads that queue. The automatic size starts at the device callback
plus the measured arrival jitter, and steps up one size on each dry run
(`getRMatchDiags` underflows); it never steps down while the stream is
open. The manual delay replaces the automatic size. Remote playback keeps
its own matcher (`RemoteAudioRateMatcher`) and feeds the device's queue
directly, so its audio never passes through a second one.

"Now X ms from the radio to <device>" adds the rmatchV fill, the queue, the
device buffer and the latency the system reports for the device
(`OutputPacing::deviceLatencyNs`, R-R3-35). The mic's line uses the same
parts in the other direction, plus the helper hop.

### The PC mic hand-off

The helper keeps its process and its supervisor (`CaptureSupervisor`), so a
mic that hangs while opening still cannot hang the window (R-R3-36). What
changes is the hand-off:

- the helper captures in the native engine's real-time callback and writes
  into a shared-memory lock-free ring, with a wake signal per buffer
  (Chromium's audio service and Firefox's audioipc work this way);
- this replaces the 480-frame records on stdout and the 10 ms pump;
  stdout keeps only status and control;
- on the Mac the helper's audio thread joins the input device's audio
  workgroup across the process boundary (`os_workgroup_copy_port`,
  `os_workgroup_create_with_port`);
- one self-sizing rmatchV where the mic meets the radio's transmit clock.

The helper uses the same engine and saved identity as the main process.

### The Core speaker

The Core's speaker is the Core's own output, through the ALSA direct
engine. Its level and mute are the Core's master level and mute
(`audio/Master/Volume` and `audio/Master/Muted` in the Core's settings),
so on upgrade it plays at the level it does today. New properties on the
Core's `RadioModel`, mirrored the way `radioSpeakerVolume` is and sent
only to a peer that declared `coreSpeaker` 1:

| Property | Direction | Meaning |
|---|---|---|
| `coreSpeakerVolume` | both ways | 0 to 100 |
| `coreSpeakerMuted` | both ways | mute |
| `coreSpeakerDevice` | both ways | the saved choice (ID and name), empty for the Core's default |
| `coreSpeakerDevices` | Core to window | the Core's catalogue: each card's ID, name and state |
| `coreSpeakerState` | Core to window | playing on which card, not connected, in use, no card, or waiting for a pick (D31) |
| `coreSpeakerDetails` | both ways | buffer size, delay setting and sample rate; the negotiated format and the delay now (Core to window) |

A window sets these; the Core applies them and saves them in its own
settings. The phone reads only the volume, the mute and the state.

## Behaviour

### Engines and lists

R-AUD-01. Each system's Driver list, in this order:

- Mac: "Core Audio", greyed (the only native choice; older drivers add
  nothing on the Mac).
- Windows: "Windows audio, shared", "Windows audio, exclusive", "ASIO",
  then "Older drivers": MME, DirectSound, WDM-KS.
- Linux with PipeWire: "PipeWire", then "Older drivers": JACK, ALSA.
  PipeWire also serves programs written for PulseAudio, so there is no
  separate PulseAudio entry; a PulseAudio server that reports itself as
  PipeWire counts as PipeWire.
- Linux with PulseAudio: "PipeWire (not running)" greyed, "PulseAudio",
  then the older drivers. With neither running, both show greyed with
  "(not running)" and the older drivers remain.
- The Core: "ALSA, direct", greyed, with "The Core runs without a desktop,
  so it plays straight to the sound card."

The Outputs page's Sound system line (R-SPK-21) names the engine:
"Windows audio (WASAPI)", adding " and ASIO (<driver>)" while one is in
use; "Core Audio"; "PipeWire. NereusSDR talks to it directly."; or
"PulseAudio. PipeWire was not found, so NereusSDR talks to PulseAudio
directly."; then "Older drivers in use: <names>." when a card uses one.

The three WASAPI checkboxes are gone (D10). R-SPK-24's Driver API and
checkbox rows are replaced by this requirement.

R-AUD-02. With no saved choice, speakers, headphones and the PC mic use
the system's first native choice: Core Audio, Windows audio shared,
PipeWire (PulseAudio where PipeWire is not running), ALSA direct on the
Core. Each starts on "(platform default)".

R-AUD-03. Every device list (Setup's cards, the header speaker menu, the
VAX cable rows and the VAX first-run dialog) shows a device added or
removed within 1 s of the system reporting it, without Rescan, and without
interrupting any stream on another device.

R-AUD-04. A saved device stores `Engine`, `DeviceId` and `DeviceName`
under its prefix (`audio/Speakers`, `audio/Headphones`, `audio/TxInput`,
`audio/Vax<N>`), plus `FirstChannel` for an interface pair, `MicChannel`
(`Left`, `Right` or `Both`) for the mic, and `DelayMs` (0 for automatic).
Matching follows "Saved identity". The existing keys stay readable for
migration.

R-AUD-05. Migration, once, on the first start after the update (D11):

| Saved before | After |
|---|---|
| no `DriverApi`, or MME on Windows | the system's native engine (R-AUD-02), same device |
| WASAPI, `ExclusiveMode` False | Windows audio, shared, same device |
| WASAPI, `ExclusiveMode` True | Windows audio, exclusive, same device |
| Core Audio (Mac) | Core Audio, same device |
| ALSA with the device `pipewire`, `pulse` or `default` (PortAudio's way to reach the sound server) | the running native engine, on "(platform default)" |
| MME, DirectSound, WDM-KS or JACK picked by name, or ALSA with a card's own device | unchanged, under Older drivers |

"Same device" is matched by name, with two allowances: an MME name, cut to
31 characters, matches the Windows endpoint whose name starts with it; and
on Linux an ALSA card name matches the sound server's device for the same
card and device number, where the server reports them (confirmed on the
bench, V-HW-4). A device that can't be found after migration shows "not
connected" (R-AUD-08 to R-AUD-10), never a silent switch.

R-AUD-06. Rescan devices re-initializes PortAudio for the older drivers.
Streams on older drivers fade out, close and reopen; native streams are
untouched. Its note says "Only the older drivers need this. <engine> lists
update by themselves." On the Mac it is greyed with "Core Audio lists
update by themselves, so there is nothing to rescan." This changes what
the Rescan devices button on R-SPK-21's Outputs page does. The VAX
first-run dialog's "Rescan now" does the same (bug 3).

R-AUD-07. An interface with more than two channels lists each pair as its
own entry, grouped under the interface ("Focusrite USB ASIO · Outputs
3-4"), on ASIO, Core Audio and PipeWire, and on PulseAudio where the
interface's profile exposes its channels. Speakers and headphones may use
different pairs of one interface; both on the same pair shows "Speakers
and headphones are on the same pair, so they play together." The mic's
pair has "Mic is on: Left / Right / Both".

### When a device goes away or is busy

R-AUD-08. Speakers or headphones whose chosen device goes away keep
playing on the system default, at once (on the stream's error, without
waiting for the list). The card and the header menu show "<name> (not
connected)" and "<name> is not connected. Playing on the system default,
<default>, until it comes back." When it returns, playback moves back by
itself.

R-AUD-09. A PC mic whose chosen device goes away goes silent. It is never
replaced. The Microphone page says "<name> is not connected. The mic stays
silent until it comes back; NereusSDR never switches to another mic on its
own." The transmit badge follows R-AUD-24. When the device returns, the
mic resumes by itself.

R-AUD-10. A VAX channel whose cable or pair goes away goes silent and
shows "<name> is not connected. VAX <N> stays silent until it comes back;
NereusSDR never sends it anywhere else." It resumes by itself.

R-AUD-11. A chosen device that is present but held by another program
(Windows exclusive mode, a single-client ASIO driver, a desktop sound
system on the Core box) is handled as in R-AUD-08 to R-AUD-10, with "in use
by another program" in place of "not connected", and switches back by
itself when it frees.

R-AUD-12. Speakers or headphones on "(platform default)" follow the
system's default output when it changes, right away. The delay readout
updates and the PC tooltip names the new device.

R-AUD-13. A PC mic on "(platform default)" follows the system's default
input when it changes, except: during a transmission the switch waits for
unkey; and it never follows to a Bluetooth mic (it stays on the mic it
has). The transmit badge's tooltip names the mic in use.

### Bluetooth

R-AUD-14. Bluetooth is told by the transport the system reports.
NereusSDR opens a Bluetooth mic only when it is picked by name, never as
a fallback (bug 4). Then the Microphone page shows "Bluetooth headsets
switch to phone-call quality, for listening too, while they are your mic.
For the best sound, listen on <name> and talk on a wired or built-in
mic." When one headset is both mic and speakers, the mic opens first. A
Bluetooth mic stays open the whole session while the mic source is PC, as
today.

### Latency

R-AUD-15. Each local output has a clock-matched buffer ("Clock matching
and the delay readout") whose size is automatic by default. Device details
show "Delay: Automatic / 2 / 3 / 5 / 10 / 20 / 40 ms" and "Now X ms from
the radio to <device>". On each system measured in V-HW-8, at defaults,
the delay is no larger than the previous build's on the same machine.

R-AUD-16. Windows audio, shared runs the device at the engine's smallest
period in the device's own format. Exclusive runs event-driven in the
device's best format and shows "Other apps cannot play through this
device while NereusSDR has it." Older drivers show "An older driver: more
delay, and its list updates only with Rescan devices."

R-AUD-17. The PC mic hand-off follows "The PC mic hand-off". The mic's
Device details show its delay now: the parts of R-AUD-15 in the other
direction, plus the hop. Target: under 1 ms for the hop between the
helper and the window.

R-AUD-18. On the Mac, the DSP thread leaves and rejoins the audio
workgroup of the device it plays to whenever that device changes, and the
mic helper's audio thread joins its input device's workgroup (bug 5).

### ASIO

R-AUD-19. One ASIO driver at a time. Choosing a pair on a second driver
for any device (speakers, headphones, mic or a VAX channel) asks "NereusSDR
can use one ASIO driver at a time. Switching <device> to <driver> also
moves:", lists each other ASIO device and the pair it moves to, and offers
"Switch all to <driver>" and Cancel. Cancel keeps everything as it was.

R-AUD-20. Buffer size and sample rate appear in each ASIO device's
details, kept equal across the driver's devices, with "Buffer size and
sample rate are shared with <devices>, on the same ASIO driver." A driver
whose minimum and maximum buffer sizes are equal shows the size greyed
with "Set in the ASIO control panel".

R-AUD-21. When the driver sends a reset request, NereusSDR stops, closes
and reopens the ASIO driver with its new settings, outside the driver's
callback, and the details show "Restarted with the driver's new settings."
for a few seconds. Other engines' streams are untouched.

R-AUD-22. "ASIO control panel" in an ASIO device's details opens the
driver's own settings window. It is greyed on other drivers and other
systems.

### Header

R-AUD-23. The PC icon's right-click menu (`MasterOutputWidget`) follows
D20 and `header-menu-mockup.html` option A. Left click still mutes. The
tooltip: "PC volume. Click to mute, right-click for speakers." plus the
device playing now.

### Keying with the mic missing

R-AUD-24. R-R3-36's keying rule is unchanged (D21). New: while the PC mic
is the source and not connected (or in use by another program), the
transmit panel's source badge (`TxApplet`, `m_micSourceBadge`) reads "PC
mic not connected" in amber, with the tooltip "<device> is not connected.
Transmit audio is silent until it comes back; NereusSDR never switches to
another mic by itself. Change the source in Settings > Audio > Microphone."
When the device returns, the badge clears and the mic resumes by itself;
the operator presses MOX again. Retry stays for other mic failures. Tune,
two-tone and TCI key normally.

### The Core

R-AUD-25. The Core plays to its sound card through the ALSA direct engine.
Cards plugged in or out appear and go within 1 s, and a chosen card that
goes away falls back to the Core's default card and back, as R-AUD-08.

R-AUD-26. The Core's installers and station images add a drop-in granting
the `audio` group, next to the serial one (D19).

R-AUD-27. A window connected to a Core shows the "Core speaker" card on
Outputs, between Headphones and Radio speaker, as in
`core-speaker-mockup.html`: "A speaker or sound card plugged into
<Core>."; Volume and "Mute Core speaker"; Device, listing "(the Core's
default)" and the Core's cards; the note "This is the speaker at the
Core, for listening where the Core sits. Changes here reach every window
and the phone. Each slice's AF level and mute still apply."; and Device
details with the R-AUD-01 Core driver line, sample rate, channels, buffer
size, delay and the negotiated format. The card:

- while the Core is unreachable: greyed with "Connect to the Core to
  change these.";
- with an older Core: greyed with "This Core can't set its speaker from
  here. Update the Core.";
- in a window that runs the radio itself: absent, because the This
  computer card is that speaker;
- with no card at the Core: shown, with "No sound card is plugged into the
  Core. Plug in a USB sound card or speaker and it shows up here by
  itself.";
- with the chosen card missing: "<name> is not connected at the Core.
  Playing on the Core's default, <card>, until it comes back.", or, with
  no other card, "<name> is not connected at the Core, and the Core has no
  other sound card, so it is silent until it comes back."

R-AUD-28. The Core speaker's level and mute change only what the Core's
sound card plays: never the audio sent to windows, the phone, TCI or the
radio.

R-AUD-29. The phone's Sound panel gains a "Core speaker" section under
Radio speaker, as in `phone-core-speaker-mockup.html`: a cyan slider
(SF Symbol `hifispeaker`) and "Mute Core speaker". With the chosen card
missing it shows the R-AUD-27 note in amber. When the Core plays on no
card (none plugged in, or D31's waiting state), the section is left out,
and it appears by itself when that changes. The phone's described Setup
pages do not gain the card.

R-AUD-30. On a Core box that starts into a desktop, the Core opens no
sound card until a Core speaker is picked in a window. The Device list
then starts at "(none)" with "The Core's computer runs a desktop, which
uses its sound cards. Pick one here to play the Core speaker on it." Once one
is picked, the card adds "The desktop can't play through <card> while
the Core has it." On a box without a desktop, the Core uses its default
card from the start.

### Building and testing

R-AUD-31. On Linux one build carries both the PipeWire and PulseAudio
engines; which is offered depends on which sound server answers at run
time.

R-AUD-32. Tests never open a real audio device, on any engine. R-R3-21's
rule against PortAudio in tests extends to every native engine; the
catalogue and each engine have test seams with fakes.

R-AUD-33. `PortAudioBus.cpp:119` uses `Q_OS_MAC` (bug 6), and
`CMakeLists.txt:532-541` and `2026-04-19-vax-design.md` section 8.5 say what
the ASIO licence is now (bug 7).

R-AUD-34. The iPhone's own audio (AVAudioSession) is unchanged. Remote
desktop windows use the native engines for this computer's devices exactly
as local windows do.

## Out of scope

- A microphone plugged into the Core box.
- The iPhone's own audio routing.
- The remote audio codec and network path.
- A NereusSDR VAX driver for Windows.
- Removing PortAudio. It stays for the older drivers.
- miniaudio or any other wrapper library.

## Verification

Software tests (this machine, offscreen, fakes only):

- V-SW-1. Catalogue: fake engines adding, removing and re-adding devices
  update every list within the debounce, and an open stream on another
  device is untouched; a burst of notices re-lists once; a returning
  Bluetooth device that fails its first opens is retried on the schedule.
  (R-AUD-03)
- V-SW-2. Matching: ID before name; name within one engine only; two
  devices with one name told apart by ID; a WASAPI choice never reopens on
  MME. (R-AUD-04, bug 1)
- V-SW-3. Migration: one profile per row of the R-AUD-05 table produces
  the row's result, once. (R-AUD-05)
- V-SW-4. Fallback and return: speakers and headphones to the default and
  back; mic and VAX silent and back; busy handled the same with its text;
  the platform default following outputs and inputs, held during
  transmit, never to Bluetooth. (R-AUD-08 to R-AUD-14)
- V-SW-5. Clock matching: simulated device clocks 200 ppm fast and slow
  for 10 minutes stay inside the buffer with no dry runs after sizing; a
  forced dry run steps the size up once; the readout equals the sum of its
  parts; the device callback takes no lock. (R-AUD-15)
- V-SW-6. Mic hand-off: the shared-memory ring passes audio in order with
  no loss under a slow and a fast reader; a helper that hangs while opening
  still leaves the window responsive. (R-AUD-17, R-R3-36)
- V-SW-7. ASIO logic with a fake driver: the switch-all prompt lists every
  other ASIO device including VAX, and Cancel changes nothing; buffer and
  rate stay equal across devices; a one-size driver greys the size; a reset
  request restarts outside the callback. (R-AUD-19 to R-AUD-21)
- V-SW-8. Keying: R-R3-36's existing tests stay green; the badge text,
  colour and tooltip follow the mic state; a returning mic clears it.
  (R-AUD-24)
- V-SW-9. Core speaker: the properties round-trip between a Core and a
  remote `RadioModel`; they are withheld from a peer that did not declare
  `coreSpeaker` 1; an older Core greys the card; the level and mute leave
  the remote, TCI and radio feeds unchanged; the phone section's presence
  follows the state; the desktop-box rule. (R-AUD-27 to R-AUD-30)
- V-SW-10. `tst_core_has_no_gui_includes` and the no-device test rule stay
  green with the new code. (R-AUD-32)

UI checks (rendered pixels, by the repository's UI gate):

- V-UI-1. Outputs and Microphone on each system, against
  `asio-setup-mockup.html`: Driver lists, pairs, not connected, busy,
  Bluetooth note, delay line, ASIO details and prompt.
- V-UI-2. The header menu and tooltip, against `header-menu-mockup.html`.
- V-UI-3. The transmit badge, against `tx-mic-mockup.html`.
- V-UI-4. The Core speaker card in each state, against
  `core-speaker-mockup.html`.
- V-UI-5. The phone's Sound panel, against `phone-core-speaker-mockup.html`,
  in the simulator.

Hardware (a person at the bench):

- V-HW-1. Mac: AirPods and a USB interface connected and removed mid-session
  appear and go by themselves; speakers fall back and return; the default
  following; picking AirPods as the mic shows the note and opens the mic
  first.
- V-HW-2. Windows PC: the same as V-HW-1 with Windows audio shared and
  exclusive; an update from a saved MME and a saved WASAPI choice;
  VB-CABLE installed and removed while running.
- V-HW-3. ASIO interface on that PC: pairs listed; speakers and headphones
  on two pairs; the control panel button; a buffer change in the driver's
  window restarts with the new size; another program holding the driver
  shows "in use".
- V-HW-4. PipeWire desktop (current Ubuntu): a Bluetooth headset appears as
  its own device; an interface's pairs; fallback and return.
- V-HW-5. PulseAudio: not tested on hardware (no PulseAudio desktop). It
  stays so in the release notes unless Ubuntu 22.04 is run from a USB
  stick on the bench.
- V-HW-6. Pi 4 and Pi 5 Cores: a USB sound card plugged in and out shows
  up and goes in a window's Core speaker card; the headphone jack and HDMI
  listed; the level and mute from a window and the phone; the drop-in
  lets a fresh station image play out of the box.
- V-HW-7. A Pi running the Pi OS desktop: the Core leaves the cards alone
  until one is picked, then the note shows. Pending until a Pi with the
  desktop image is on the bench.
- V-HW-8. Delay at defaults on the Mac, the Windows PC (shared, exclusive,
  ASIO) and the PipeWire desktop: a test hook puts a click into the
  speaker path and times its return through a loopback cable into an
  input of the same interface, minus that input's reported latency. Taken
  on the build before this change (with the hook added first) and after:
  never larger than before, and the readout within 2 ms of the
  measurement.
- V-HW-9. The mic hop on the Mac and the Windows PC, timed from the
  helper's write to the window's read on the machine's monotonic clock:
  under 1 ms.

## Design choices to confirm

These are this spec's own choices, made because each followed from a
decision above or had one sensible answer. Each can be vetoed.

1. Windows' "(platform default)" follows the default device, not the
   default communications device.
2. Device notices are debounced 500 ms, and a returning device is retried
   at 250 ms, 500 ms, 1 s, 2 s, then every 2 s.
3. When another program changes a device's sample rate, the engine reopens
   at the new rate and the clock matching absorbs it, with no prompt.
4. ALSA cards are named as ALSA names them ("bcm2835 Headphones", "USB
   Audio Device") and saved by card ID and device number.
5. The Core watches cards with inotify on `/dev/snd`, adding no library.
6. PipeWire and PulseAudio are both linked directly, as PipeWire is today;
   which runs is decided at start-up.
7. A Core box "starts into a desktop" when systemd's default target is
   `graphical.target`.
8. The phone's Core speaker section shows only while the Core plays on a
   card, so D31's waiting state counts as no card.
9. The Core speaker's level and mute are the Core's existing master level
    and mute, so nothing changes in loudness on upgrade.
10. The manual delay choices are 2, 3, 5, 10, 20 and 40 ms, and the
    automatic size never shrinks while a stream is open.
11. The PipeWire buffer size in Device details is the requested
    `node.latency` (default 128 frames, as today's default buffer); the
    readout shows what the graph really runs at.
12. Rescan affects only the older drivers.
13. The new user-facing strings in R-AUD-06 to R-AUD-30 are this spec's
    wording, beyond those JJ saw in the mockups.

## Mockups

In `2026-10-08-native-audio-engines-design/`:

- `asio-setup-mockup.html`: Setup > Audio Outputs, Microphone and Digital
  modes on Windows, Mac, Linux with PipeWire and Linux with PulseAudio:
  the Driver list, interface pairs, the one-driver prompt, the ASIO
  control panel stand-in, the delay line and the not-connected notes.
  Built on PR 355's pages.
- `header-menu-mockup.html`: the PC icon's speaker menu, options A (chosen)
  and B.
- `tx-mic-mockup.html`: the transmit panel's badge with the mic missing,
  and the keying rule.
- `core-speaker-mockup.html`: the Core speaker card in a window connected
  to a Core, unreachable, and running the radio itself, with cards plugged
  in and out.
- `phone-core-speaker-mockup.html`: the phone's Sound panel with the Core
  speaker section, its unplugged note, and the two no-card options (option
  1 chosen).

Delay and buffer numbers in the mockups are examples, not measurements.
