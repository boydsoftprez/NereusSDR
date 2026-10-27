# Digital audio and external applications

This chapter covers the audio paths and network controls used by digital-mode programs. VAX exposes virtual audio channels. TCI provides a WebSocket control and audio interface for compatible clients. These controls do not decode digital modes inside NereusSDR. A VAX meter or connected TCI client confirms only the part of the path it measures.

Desktop VAX channels and TCI audio preferences belong to **This computer**, even when the desktop window is connected to a remote Core. The desktop TCI server control can coordinate the Core server, which retains its own on/off state. iPhone/iPad **Tools > VAX Audio** and **Tools > TCI Server** pages operate the Core computer directly. A missing or gray phone tool can mean the Core build or radio does not offer it.

## Route receive audio through VAX

### Desktop setup

Start with a connected radio and an active slice tuned to the signal you want to decode. Open **File > Settings... > Audio > Digital modes**. Its **VAX** part has a card for each of **VAX 1** to **VAX 4** with **On**, **Device**, **Format**, **Used by:**, **Activity:**, **Rename…** and **Copy name**. On Mac and Linux NereusSDR makes the devices itself and **Device** names them. On Windows **Device** is a picker: choose a virtual cable per channel, as described in [Choose cables for digital modes](01-desktop-connect.md#choose-cables-for-digital-modes). For the channel you intend to use:

1. Turn **On** on. The channel becomes available as an audio input to consumer applications. To route a receiver, select its VFO and use the VFO's **VAX** tab/channel selector to assign the desired VAX channel. The VAX applet's slice-letter tag confirms which receiver is assigned. Use distinct channels if you need separate receiver feeds.
2. Use **Rename…** to give the channel a recognizable name if your operating system or decoder lists several similar NereusSDR devices. Use **Copy name** when you need the exact device name to paste into another program (on Linux it copies the PipeWire node name). This name does not change the slice being routed.
3. Read **Format** and **Used by:** as status, not selection controls. The operating system exposes the VAX source when the channel is on; choose the matching NereusSDR VAX source in the external digital program.
4. Show the **VAX** applet from the desktop applet/container controls (**Tools > VAX Audio...** opens **File > Settings... > Audio > Digital modes**, not the applet). The VAX row is labeled by channel number and assigned slice letter. The applet also lists channel device and contributing slices, with RX level, meter and mute; its transmit row, labeled **TX:**, shows the transmit slice and TX level/meter. RX mute suppresses only that VAX channel for apps, not the speaker. TX gain controls the VAX transmit level and may be disabled when transmit settings are not permitted. Its level control changes the level delivered to that virtual output; **Mute** suppresses only that VAX tap and does not mute the normal speaker output. Unmute and confirm activity on the row while a signal is present.

The desktop page configures VAX sources on this computer, including when the receiver stream arrives from a remote Core. On Windows the cable you pick is the operating-system device; on Mac and Linux NereusSDR makes it. If a channel is enabled but the program receives silence, verify in order: the channel is routed to the intended slice, that slice is producing audio, the row is not muted or turned down, the program selected the same VAX endpoint, and the operating system still has the virtual device. Re-select the device, or press **Rescan** on Windows, after changing audio drivers. An exposed channel with no consumer means the application has not opened it. A live level does not certify a decode.

### Phone and iPad

Tap **Tools > VAX Audio**. This is a Core tool: the Core computer must have a supported virtual audio backend and an available VAX device. Each **VAX 1 to 4** card shows the device name, contributing slice or slices, **Level** slider, **Meter**, and **Mute** switch. While a signal is present, use the meter to check the channel, adjust **Level** to change that VAX feed for apps, and switch **Mute** on/off to silence/restore only that channel for apps. The main speaker remains unaffected. The **VAX microphone** card on the phone shows the transmit slice, **Level**, and meter. Its level adjusts the VAX TX microphone gain; it is disabled when this phone cannot transmit, and the reason/refusal appears below. The page shows the Core's latest refusal after a rejected change. If channel or meter status reports that the Core/build does not provide it, follow that reason; older Core versions may omit the channel or level stream. The phone does not create a virtual device on the phone itself. A remote receive-audio path may have lower quality than local audio; use a local Core-side application for decoding when network audio degrades the signal.

[![Desktop Digital modes page with VAX channel cards, format fields and consumer status](images/desktop-modes-audio.png)](images/desktop-modes-audio.png)

*VAX receive channels before routing, rendered for the native audio build on a Mac: the cards are off and no program is using them. Check On, format and Used by independently. The separate TX endpoint is described below.*

## Send transmit audio from a digital application

Begin with a running desktop audio engine and a supported virtual audio backend. Enabling numbered VAX channels is for receive feeds; the TX input is separate. On macOS, choose **NereusSDR TX** as the digital program's audio output. With the Linux PulseAudio/pactl backend, choose the **NereusSDR TX** sink. With native PipeWire, route the program's audio output to the **NereusSDR TX input** stream in the system's audio graph; do not choose a numbered RX source as the TX destination. NereusSDR reads that incoming stream as transmit audio. Select **VAX TX (virtual device)** under **Source** in desktop **File > Settings... > Audio > Microphone**. Confirm the selected source and TX audio indication before transmitting, and use the ordinary transmit ownership, band, interlock and power checks in [chapter 5](05-transmit.md). This path works on macOS and Linux. The headless **nereusd** service publishes neither RX nor TX VAX devices; a running desktop audio engine is required for this virtual-device workflow. On Windows the receive channels use the cable you pick for each of **VAX 1** to **VAX 4**. Windows has no separate VAX transmit device, so **VAX TX (virtual device)** is grayed there, with this route in its tooltip, and so is the **VAX** toggle on the **Phone/CW** applet; a VAX source chosen earlier opens as **PC Mic**. To transmit a program's audio, set the program's audio output to a virtual cable, then open **File > Settings... > Audio > Microphone**, choose **PC Mic**, and pick the other end of that cable (its recording side) as the **Device** in the **PC microphone** card.

Do a receive-only check first. Confirm receive audio reaches the decoder and that the decoder's own status changes. For transmit, use a permitted, coordinated test at a safe power and frequency. Confirm TX ownership and the intended VAX source before keying. If the RF transmitter does not key, investigate the displayed TX refusal; changing VAX routing cannot grant TX ownership or clear a radio protection. Do not assume the program's transmit button keys the radio unless it is explicitly configured to use a supported control path.

## Configure TCI audio

On the desktop, open **File > Settings... > Audio > Digital modes**, in its TCI part. This controls the TCI audio bridge on this computer. Set **Slice A rate** to a rate supported by the connected client. The offered rates are 24, 48, 96 and 192 kHz (default 48 kHz). This page does not expose Slice B, C or D rate controls; C and D are explicitly unavailable over TCI.

Under **Audio stream**, choose the **Format:** and **Channels:** the client can decode, and the **Block size:**. The block size is also set on **File > Settings... > CAT & Network > TCI Server**; its default is 2048 samples. The formats are Int16, Int24, Int32 and Float32; channel count is mono or stereo. A mismatched client format can produce silence or invalid audio. Use the client's documented supported format, then reconnect the client and check its stream status. **TX channel** selects Left, Right or Both for transmitted audio. **TX buffering** controls the bridge's transmit audio buffer: a smaller value reduces delay but leaves less tolerance for uneven client delivery. If TX audio breaks up, increase buffering gradually and confirm the result at the receiver.

TCI streams are independent of the main speaker mute. The TCI applet provides Slice A and TX gain controls; use those stream-level controls to diagnose a TCI-only level. TCI audio setup is local to this desktop, including when it operates a remote Core.

## Start and monitor a TCI server

On desktop, open **File > Settings... > CAT & Network > TCI Server** or **Tools > TCI Server...**. If the TCI tool is unavailable, confirm the build includes WebSocket support. In Setup:

1. Turn on **Enable TCI Server**. Choose **Listen on** and **Port**. The listen address determines which network interface can accept connections. Use an interface reachable by the client and avoid exposing a server to a network you do not trust.
2. Review **Send initial state on connect** and the server options. The page also has **Compatibility** and **VFO Quirks** groups (for example **CW becomes CWU above 10 MHz** and the RX2 VFO choices), an **IQ Stream** group (**Always stream IQ**, **Swap I/Q channels**) and a **Sensors** group. Leave compatibility and emulation options off unless the client requires that protocol behavior.
3. Read the displayed **Status**. Open the TCI log if a client cannot connect; use the logged connection or refusal detail to correct the listen address, port, firewall or client target.
4. Configure the external client with the displayed host and port. Return to the TCI status/app applet and verify the client appears in **Clients**. A successful WebSocket connection does not mean that the client owns transmit control. In a remote window, a TCI program that keys with its own audio sends that audio, or silence, and never the microphone. Review the server options by their displayed labels: ExpertSDR3 protocol emulation, SunSDR2 PRO device emulation, CWL/CWU presented as CW, and sending initial state to new clients. On the phone, these options apply to apps connecting from that point onward and may be unavailable while on air.

TCI refuses a connection that comes from a web page, so a page open in your browser cannot key the radio. Native TCI programs connect as usual. If a TCI program stops answering while it holds the key, NereusSDR releases the key after about 3 seconds, as if the program had disconnected.

Before a TCI program requests transmit, confirm that the desktop window
hosting its server holds the station’s **TXcontrol**, using the ordinary
station transmit-holder indication. The external app does not become a
station device or acquire unheld station transmit merely by connecting.
An accepted TCI key can then acquire the separate app TX-audio lock. The
client’s TX/audio indication is evidence after an accepted request, not a
pre-key station-ownership control. If keying is refused, inspect the owning
window’s station permission first, then the app/server reason. The TCI applet
has status and RX/TX audio gain controls; those gains affect TCI streams,
not the main speaker route.

On iPhone/iPad, open **Tools > TCI Server** to inspect or operate the Core computer's server, port, options and connected clients. This is a Core control. A Core may not include or enable the server even when the desktop has its own TCI server. The phone page shows the Core server switch, port, station address when available, error/status, four options, and connected app rows with name, address, subscriptions, last command and TX badge. Use **Disconnect** only after confirming the app; the page asks first. If a control is unavailable, follow its capability/readiness or on-air reason.

[![Desktop TCI Server settings separating this-window and Core server controls](images/desktop-tci-server-workspace-original.png)](images/desktop-tci-server-workspace-original.png)

*TCI Server has two groups. **This window's server** is local to the desktop: here it listens on this computer only (127.0.0.1), port 50001, and its status reads Stopped. A note under it says the Core also serves TCI at port 50001 for apps on the Core's computer. **The Core's TCI server** group shows where the Core listens and its options. A chosen port is not a connected-client readback. This is a capture of the workspace-look window; no server was started for the figure, and the groups below these two are not shown.*

## Set DIG and RTTY offsets

Select the slice you intend to configure and choose **DIGU** or **DIGL** from its mode controls. The slice's **DIG Offset** control shifts the digital audio passband offset in hertz. Adjust it in small increments while watching or listening to the decoder, then verify the displayed value and decoder response. This offset only positions the receive/transmit audio passband; it is not a decoder, CAT connection, or automatic frequency correction.

When **DIGL** is selected, the RTTY controls expose **Mark** and **Shift**. Use the adjacent decrease/increase arrows or change the displayed value. Mark is the mark tone frequency; Shift is the separation between mark and space. The controls accept Mark from 1000 to 3500 Hz and Shift from 50 to 1000 Hz. The shown initial values are 2295 Hz and 170 Hz. Confirm the external RTTY program is configured for compatible tones and that the selected audio route is the one in use. NereusSDR provides these offset controls, not an integrated RTTY decoder. CAT control is a separate feature, described in [Control NereusSDR from CAT programs](#control-nereussdr-from-cat-programs). For the mobile modes page, see [chapter 7](07-iphone-operate.md).

## Choose the computer and route

For the worked route below, use macOS or the Linux PulseAudio/pactl backend, and run the desktop NereusSDR window and the external digital program on the same computer. VAX carries the selected receiver to that computer’s audio system, and the separate **NereusSDR TX** output device/sink carries the program’s transmit audio back into NereusSDR. With native PipeWire, the TX destination is an input stream that needs a system audio-graph link; follow [the backend-specific TX directions](#send-transmit-audio-from-a-digital-application) rather than looking for a PulseAudio sink. A VAX RX channel is not the TX input. The program must support selecting an operating-system audio device; this chapter does not assume or specify its own rig-control or automatic-keying settings. If the program also controls the radio, see [Control NereusSDR from CAT programs](#control-nereussdr-from-cat-programs).

With a remote desktop session, the VAX devices belong to the computer running that desktop window. Eligible Core receiver audio can feed the remote window's VAX output, so an external program on that same computer can consume it. With **VAX TX** as the transmit source, the remote window sends the Core what programs on this computer write to its VAX TX device. The external program and any audio endpoint must be on the same computer as that VAX device. After a dropout, the remote window's VAX output keeps a larger reserve, up to a quarter second, so later short pauses in the network link are not heard. After 10 minutes with no dropout it shrinks again the next time the Core's audio goes quiet. The larger reserve moves a digital program's DT by up to about 0.12 s. The phone's **Tools > VAX Audio** and **Tools > TCI Server** pages address the Core; they do not create an audio device on the phone. A headless **nereusd** process does not publish the VAX endpoints described here.

Use VAX when the external program needs audio endpoints and NereusSDR will
key the radio manually. Choose TCI only when the program supports the TCI
protocol and needs subscribed receiver audio or TCI control. In that route,
receive audio travels from the server’s subscribed slice to the client; a
client supporting TCI transmit can return audio and request keying.

For a desktop-hosted TCI server, first verify that the owning window/device
holds station transmit control. Set the server’s sample rate and format to
values the external program supports, then use the server/client list and
**TCI Clients** to check connection and receiver subscription. The separate
app TX/audio indication appears after an accepted request. Connection,
subscription and station ownership do not prove decoding or RF output. A
Core-hosted server uses the Core’s transmit authority; the phone’s TCI page
controls that service rather than granting the external program its own
station ownership. Follow the displayed refusal when authority is unavailable.
See [Configure TCI audio](#configure-tci-audio), [Start and monitor a TCI
server](#start-and-monitor-a-tci-server), and [Inspect the desktop TCI client
chain](#inspect-the-desktop-tci-client-chain) for their actual controls.

## Worked route: receive through VAX, key from NereusSDR

This procedure uses macOS or Linux PulseAudio/pactl, one desktop slice, one external audio-input consumer, and NereusSDR’s own transmit controls. Native PipeWire uses the separate graph-link alternative described [above](#send-transmit-audio-from-a-digital-application). It does not configure a digital program's undocumented controls. Do the receive check before preparing transmit:

1. Connect the radio, select a slice you control, tune the signal, and set the appropriate digital mode and passband. Set **DIG Offset** only when the signal or decoder requires an audio passband shift; it is not frequency correction. For phone-only step sizes and controls, use the mobile directions under [Set DIG and RTTY offsets](#set-dig-and-rtty-offsets).
2. On the desktop open **File > Settings... > Audio > Digital modes**, switch **On** for one channel, and assign that channel in the selected slice's VFO **VAX** tab. The **VAX** applet should tag the channel with that slice letter. If another VAX app already uses the channel, choose a free channel or knowingly share the summed feed.
3. In the **VAX** applet, confirm the assigned row is not muted and its level meter responds to the selected signal. The applet meter measures the NereusSDR tap. It does not show whether the operating system or decoder received the samples.
4. In the external program, select that same **NereusSDR VAX N** device as its audio input. Confirm its own input meter responds, then confirm its receive/decode status changes for the signal. A moving NereusSDR meter with a quiet program meter points to the device selection or audio-system link. If the program meter moves but no decoder result appears, recheck the signal, mode, passband and offset before changing the NereusSDR route. A decoder result is the last check in this receive chain; a VAX meter alone is not evidence of a decode.
5. To prepare a controlled transmit test, stop the receiver-only activity in the program and set its audio output to **NereusSDR TX**. In NereusSDR open **File > Settings... > Audio > Microphone** and select **VAX TX (virtual device)**. Check that the VAX applet's **TX** row identifies the intended transmit slice and that its meter responds when the program supplies audio. The VAX bus accepts a separate transmit stream; numbered RX VAX channels are not the TX destination. The device/sink selection in this example is for macOS and Linux PulseAudio/pactl; native PipeWire instead needs its TX input stream linked in the system audio graph. Windows has no separate VAX transmit device, so **VAX TX (virtual device)** is grayed there; a VAX source chosen earlier opens as **PC Mic**. To transmit a program's audio, set the program's audio output to a virtual cable, then open **File > Settings... > Audio > Microphone**, choose **PC Mic**, and pick the other end of that cable (its recording side) as the **Device** in the **PC microphone** card. Headless **nereusd** does not publish a TX device.
6. Before keying, check the normal transmit ownership, selected slice, band, interlocks and power state in [chapter 5](05-transmit.md). Use NereusSDR's own **MOX** control for this manual-key route; do not rely on an external program's keying button or CAT command; a program that keys through CAT is covered in [Control NereusSDR from CAT programs](#control-nereussdr-from-cat-programs). Key only for a coordinated test at a safe frequency and power, with the expected transmit audio. A refusal or an unkeyed transmitter is a station transmit-state issue; changing the VAX endpoint cannot grant ownership or clear protection.
7. Stop the program's transmit audio, turn **MOX** off, and confirm the transmit indication clears. In **File > Settings... > Audio > Microphone**, select the microphone source that was active before the digital test. Restore the prior microphone profile if it changed. Verify the source and profile readbacks before returning to voice operation; see [chapter 14](14-voice-profiles.md) for the profile controls. The VAX TX meter and the radio's TX state do not establish that a remote station decoded the signal.

If the selected Core or build refuses a VAX action, follow its displayed capability/readiness reason. If the route cannot produce a working TX endpoint on the host operating system, keep the session receive-only or use a separately documented, supported TCI client path. Do not translate a VAX setup into guessed external-program fields.

## What these controls do not provide

VAX and TCI route audio and client control. They do not add a built-in WSJT-X/RTTY decoder, IQ-over-VAX, TX monitor-to-VAX, or an automatic VAX mute during another slice's transmit. Use only the controls actually shown by the installed build. If an advertised advanced group is absent, it is not enabled by changing a parent checkbox.

## Control NereusSDR from CAT programs

CAT lets a logger, contest program or other rig-control program read and set the frequency, mode and other radio state, and key the radio, over a serial port, a network connection or a Hamlib rigctld connection. It carries commands, not audio: use VAX or TCI above for the audio path. A CAT program follows the same transmit rules as every other key source, so it can be refused for the same reasons.

Open **File > Settings... > CAT & Network** and use these pages, or choose **Tools > CAT Control...** to jump to the TCP/IP CAT page:

1. **Serial Ports** and **TCP/IP CAT** each show four channels, **CAT 1** to **CAT 4**. **Serial Ports** has **Enable serial CAT**; **TCP/IP CAT** has **Enable TCP CAT**, **Enable PTY** (a virtual serial port, macOS and Linux only) and **Enable Hamlib rigctld**, each network option with its own **Listen on:** and **Port:**. For a serial channel choose the **Device:**, **Baud:**, **Parity:**, **Bits:** and **Stops:**. A virtual serial port exists only while its channel is on.
2. Bind each channel to slices with **VFO A:** and **VFO B:**. A channel controls the slices assigned to it, whichever slice is selected on screen. If a bound slice is closed, the channel says so; pick another slice.
3. A network listener that is open to your network shows a warning: any device that can reach the port can tune and key the radio, and there is no password. Listen on the local address unless the program runs on another computer you trust.
4. **CAT Options** holds compatibility and automatic-information choices (**Report identity:**, **Send TCP welcome banner**, **Allow Kenwood AI command**, **Enable automatic frequency information**, **Report DIGL / DIGU as LSB / USB**, **Apply power limits to CAT power queries**, **Always recenter VFOs** and the RTTY frequency reporting offsets). Its **CAT Tester** sends one command on a channel and shows the reply; commands that transmit are refused there. **Show CAT Log…** opens the log.
5. **CAT PTT** lets a hardware push-to-talk line key the radio. Tick **Enable input PTT**, choose the **Input source:** (**None**, **CAT1** to **CAT4**, or **Physical** with a **Separate physical device:**); tick the box that begins **Legacy RTS wiring** (it samples the CTS input) or the one that begins **Legacy DTR wiring** (it samples the DSR input) to match your cable, and set the serial fields for a separate device. Opening the page or restoring settings never keys the radio, and NereusSDR never drives the RTS or DTR pins.
6. The **CAT** applet shows a light for each channel (hover for its state). On the pages, a network channel's status line shows whether it is listening, its address and its clients; a serial channel's shows its port state.

In a window signed in to a Core, the CAT channels above are the Core's. To control the window itself from a program on this computer, such as WSJT-X, open **File > Settings... > CAT & Network > rigctld**, tick **Enable rigctld**, and point the program's Hamlib NET rigctl at the **Listen on:** address and **Port:** (127.0.0.1 and 4532 unless you change them). The program controls the slice this window transmits on; when the window does not control that one, its active slice; failing that, the lowest-numbered slice it controls. It keys as this window. While it keys, what it plays into NereusSDR TX goes on the air, never this computer's microphone. While it is on the air, changes to the slice's frequency, mode, filter, band, TX antenna and XIT are refused, as the Core freezes them while transmitting. A slice the window only listens to is never tuned. VFO B is refused, so set the program's split operation to None or Fake It. **Status:** shows whether it is listening and how many programs are connected.

Test with a non-transmitting command first, such as a frequency change, and watch the slice follow. Then check that the program's keying button is refused or accepted as you expect: if the station does not hold transmit, the CAT key request is refused with the usual reason shown in the window, and changing CAT settings cannot grant ownership.

## Cable inventory and audio reset

Open **File > Settings... > Audio > Digital modes** on the desktop. At the foot of the VAX list, **Detected virtual cables** lists cable device names and whether each is an input or output. Press **Rescan** after installing or enabling a virtual cable. The page updates the inventory and opens first-run guidance when it detects newly added cables. This is inventory and guidance; pick the Windows cable for each channel in that channel's **Device** on the same page. On macOS and Linux, the supported VAX endpoints are created by their native backend.

**File > Settings... > Audio > Advanced** keeps the logs, the feature flags and the reset. Its **Reset** group contains **Reset all audio to defaults…**. The confirmation lists the device choices for the speakers, headphones, transmit input and VAX channels 1 to 4, the audio processing rate and block size, and the feature flags. **Cancel** leaves settings alone. **Reset all audio** clears those local audio preferences and makes first-run setup appear on the next launch. Per-slice VAX assignments are retained. In a remote desktop window, this resets the desktop computer's audio devices and leaves Core DSP settings alone. Revisit the audio pages and choose the needed local playback, microphone and VAX endpoints afterward.

The page's **DSP** sample-rate/block-size group and advanced IQ/TX-monitor/mute flags are gated as unbuilt in this baseline and hidden. They are not operator controls here.

## Inspect the desktop TCI client chain

The **TCI Clients** applet (the container catalog may still list it as Client Chain) is in the default meter/container panel when WebSocket support is built. Show it from **Containers > Applets** if hidden. Its top line reports the local desktop TCI server bind address and port or **Server not running**. The local client list shows each app's peer address, reported name, connection duration, subscribed IQ/audio slice numbers, sensor subscriptions, active TX audio badge, last command and age, and dropped-frame count when nonzero. **Auto-refresh** updates once per second while the applet is visible; turn it off to freeze periodic refresh, or show the applet for an immediate refresh. Client connect/disconnect events also refresh the list.

For a local desktop client, its **Disconnect** button closes that client's WebSocket immediately. For an app attached to a remote Core, inspect the separate **The Core's TCI server** rows. They report app name/address, TX badge, subscriptions and last command. Their **Disconnect** requests removal from the Core and is disabled while that Core is on air; the tooltip directs you to try again when transmission stops. An empty client list means no apps are connected to that server. A server unavailable reason means the Core lacks that TCI capability. A listed connection and subscriptions do not confirm successful audio reception or permission to transmit.

For RADE receive and FreeDV Reporter, see [chapter 15](15-rade-freedv.md). For Spot Hub sources, see [chapter 16](16-spots-reporters.md). For receiver noise reduction and filter setup, see [chapter 13](13-receiver-dsp.md).
