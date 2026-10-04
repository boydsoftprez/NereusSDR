# NereusSDR User Manual

Working draft, 4 October 2026. Desktop instructions for Core Settings, audio
choices, Canvas, EQ/CFC and transmit ownership have been reconciled with the
2026.10 release candidate. Other procedures retain their reviewed development
source baseline. This is an operator draft, with release captures and live
station checks still pending.

The iPhone/iPad chapters describe a separately developed native app. Mobile
availability and delivery are separate from the desktop/Core release; inclusion
of these chapters does not mean an App Store or TestFlight release is available.
Check your application's build and the Core's offered capabilities when a
control described here is absent. [Verification and build scope](verification.md)
records the exact source and capture baselines.

This manual explains how to operate NereusSDR on a desktop computer and from
an iPhone or iPad. It assumes you already know the basics of operating an
SDR: choosing a band and mode, tuning a signal, setting a receive filter,
and using your radio's receive and transmit connections.

The focus is on using NereusSDR: where to find a control, when to use it,
what it changes, and how to check the result.

## Your radio, the Core, and the app

Your radio supplies the receive signals and produces the transmitted RF.
NereusSDR performs the signal processing and provides the controls you use
to operate it.

For iPhone and iPad operation, a NereusSDR **Core** runs on a computer at
your station. The Core talks to the radio and performs the signal
processing. The app connects to the Core, displays the spectrum and
waterfall, plays receive audio, and sends your control changes and
microphone audio back to it. The Core must be running and reachable for
the app to operate your radio.

In this manual, **Core** means the NereusSDR service you connect to.
**Station** means your radio installation as a whole.

## Find the instructions you need

Start with the connection chapter for the device you are using, then follow
the operating chapters. Use the troubleshooting and reference chapters
when you need a specific answer.

| Chapter | What you will do |
| --- | --- |
| [1. Install and connect on the desktop](01-desktop-connect.md) | Install NereusSDR, choose your radio, connect, and select your audio devices. |
| [2. Get to know the desktop window](02-desktop-window.md) | Find the spectrum, waterfall, VFO flags, RX and TX applets, meters, and connection indicators. |
| [3. Tune and receive](03-receive.md) | Select a band, mode, filter, and tuning step; use CTUN, AGC, noise reduction, and receive audio controls. |
| [4. Work with slices and panadapters](04-slices.md) | Add and select slices, arrange the display, and understand which controls apply to a slice or the radio. |
| [5. Set up and transmit](05-transmit.md) | Choose a microphone source and profile, adjust levels and transmit processing, set power, and use PTT, MOX, and TUNE. |
| [6. Connect the iPhone or iPad](06-iphone-connect.md) | Set up the Core, pair the app, connect on your home network, and connect away from home. |
| [7. Operate from the iPhone or iPad](07-iphone-operate.md) | Navigate the tabs, tune with touch controls, adjust receive settings, select audio, and transmit. |
| [8. Use several devices](08-shared-core.md) | See who owns each slice and transmit, take over when needed, and understand changes that affect other operators. |
| [9. Digital audio and external applications](09-tools.md) | Route and test VAX/TCI audio, connect clients, and distinguish desktop-local from Core-hosted routes. |
| [10. Display, meters and layout](10-customize.md) | Adjust FFT, spectrum, waterfall, grid, history, 3D, TX display, meters and custom containers. |
| [11. Troubleshoot operation](11-troubleshooting.md) | Follow checks for connection failures, missing audio, unexpected tuning behavior, and refused transmit requests. |
| [12. Control reference](12-reference.md) | Look up controls, menu paths, shortcuts, and NereusSDR terminology. |
| [13. Receiver DSP and filter setup](13-receiver-dsp.md) | Adjust AGC, blankers, noise-reduction methods/models, notches, filter presets and DSP options. |
| [14. Voice processing and profiles](14-voice-profiles.md) | Configure microphone/TX profiles, EQ, compression, leveler, expander, VOX and modulation monitoring. |
| [15. RADE and FreeDV operation](15-rade-freedv.md) | Receive and transmit RADE voice, interpret decode status, and use the FreeDV Reporter. |
| [16. Spots and reporters](16-spots-reporters.md) | Configure spot sources, filter and display results, and tune from station lists. |
| [17. Hardware, antennas and calibration](17-hardware-antennas-calibration.md) | Configure radio/ALEX/HL2/OC controls and supported level, PA and wattmeter calibration. |
| [18. PureSignal and diversity](18-puresignal-diversity.md) | Configure supported feedback calibration and two-ADC diversity, and check status/results. |
| [19. Amplifiers and tuners](19-amplifiers-tuners.md) | Configure and operate supported PGXL, TGXL and RF-Kit accessories with their interlocks and readbacks. |
| [20. Phone preferences and session care](20-phone-preferences.md) | Choose touch/audio routes, manage background listening, screen/sleep/data use and time-outs, and recover interruptions. |

## Follow a complete operating workflow

For a first desktop receive session, follow [connect](01-desktop-connect.md),
[window controls](02-desktop-window.md) and [tune/receive](03-receive.md).
For a first phone session, follow [Core pairing](06-iphone-connect.md),
[phone operation](07-iphone-operate.md) and [phone preferences](20-phone-preferences.md).
Use [shared operation](08-shared-core.md) when another device is already at the station.

Before voice transmission, use [transmit setup](05-transmit.md) and
[voice profiles](14-voice-profiles.md). For a digital application, first verify
[audio routing](09-tools.md); for RADE use [its operating procedure](15-rade-freedv.md).
Hardware and accessory setup have their own chapters because those changes
affect the physical station and may affect other receivers or operators.

The [control reference](12-reference.md) maps settings pages and gestures to
procedures and explains missing or disabled features. Use
[troubleshooting](11-troubleshooting.md) to isolate a fault and collect evidence.

## Finding your way around the iPhone app

Once connected to a Core, the app has five tabs:

| Tab | Use it for |
| --- | --- |
| **Panadapter** | View the band and operate from the main spectrum and waterfall screen. |
| **Modes** | Open the selected slice's operating controls. |
| **Tools** | Open station tools such as Spot Hub and FreeDV Reporter. |
| **Radio** | Open radio management and status pages. |
| **Setup** | Configure the Core and this device. |

On first launch, **Find my Core** starts the connection workflow.
**Set up a Core** explains what must be running at your station before
the app can connect.

## Reading the procedures

Desktop instructions use **click**, **right-click**, and **drag**.
iPhone and iPad instructions use **tap**, **press and hold**, and **swipe**.
Menu paths are written in the order you open them. On the desktop,
**Setup** means the settings window opened with **File > Settings...**;
for example, **Setup > Display** names a page in that window. On the
iPhone or iPad, **Setup** is a tab.

Each procedure identifies the relevant controls, gives the steps in
operating order, and describes the result you should see or hear.
Desktop and mobile instructions are labeled where the controls or
gestures differ.

Screenshots sit beside the procedures they explain. Select a figure to open
its full-size image when you need to inspect a label or readback. Desktop overview figures
have numbered callouts; settings and tool captures show their actual idle or
receive state. Native phone and iPad simulator captures identify test data in
their captions. The settings shown are examples, not recommended starting
values. These figures come from separately recorded development builds. They
do not establish a live radio walkthrough or release compatibility.
