# NereusSDR Alpha Test — Hermes Lite 2

**Hi, and thank you.** You're helping test an **alpha-stage** SDR console. This doc walks you through what to try, what "success" looks like, and — just as important — what does **not** work yet so you don't waste time filing bugs for features we know are missing.

**What this build is:**
- A ground-up C++/Qt6 port of [Thetis](https://github.com/ramdor/Thetis) (the Apache Labs / OpenHPSDR console), architecturally modelled on [AetherSDR](https://github.com/ten9876/AetherSDR).
- This particular test build is **Phase 3I** — the work that brought Protocol 1 radios (HL2, ANAN-10/100/10E/100B/100D/200D, Metis) into the app alongside the Protocol 2 radios (ANAN-G2 Saturn, ANAN-7000/8000) that already worked.
- The **goal of your test** is simple: can NereusSDR discover your HL2, connect, and let you listen to a live SSB signal with a working spectrum and waterfall?

**What this build is NOT:**
- Ready for transmit. **Do not key up.** The TX pipeline is intentionally cold — commands go on the wire but there is no SSB modulator yet.
- Feature-complete. Dozens of controls you see in the UI are placeholder — more on that below.
- Signed. On macOS you'll need to right-click → Open the first time. On Windows you'll see a SmartScreen warning you'll need to click through.

---

## What the whole app looks like right now

Before you dive in, here's the honest state of the overall application so you have realistic expectations. NereusSDR is built in layered phases — each phase adds one focused slice of functionality. The slices landed so far, in rough order:

| Phase | What it added | State |
|---|---|---|
| 3A | Protocol 2 radio discovery + connection (ANAN-G2 / Saturn first) | Working |
| 3B | WDSP DSP engine integration — RX1 demod (USB/LSB/AM/CW), AGC, NB1/NB2, bandpass filters, audio pipeline | Working |
| 3C | macOS build + crash fixes | Working |
| 3D | GPU-accelerated spectrum + waterfall via Qt's QRhi abstraction (Metal on macOS, D3D12 on Windows, Vulkan on Linux) | Working |
| 3E | VFO tuning, mode selection, filter controls, multi-receiver foundation | Working (RX1 only in the UI) |
| 3-UI | Full UI skeleton: 12 applets, 9 menus, 47-page SetupDialog, spectrum overlay panel, status bar | Skeleton + partial wiring |
| 3F | Up to 4 independent panadapters in configurable layouts | **Not started** |
| 3G-1..7 | Dockable/floatable container system, GPU-rendered meter engine with 31 item types, container settings dialog, MMIO external-data subsystem | Working |
| 3G-8 | RX1 Display parity — every control on the Setup → Display pages wired to the renderer (47 controls) | Working |
| 3H | Thetis-inspired skin system | **Not started** |
| 3I | **THIS PHASE** — Protocol 1 support for the full ANAN/Hermes family, including HL2 | Working — that's what you're testing |
| 3J | TCI protocol server (for N1MM+, Log4OM etc.) | Not started |
| 3K | CAT / rigctld bridge | Not started |
| 3L | HL2 IoBoardHl2 I2C-over-ep2 encoding (currently inside a closed DLL) | Not started |
| 3M | TX pipeline — SSB, CW, full processing chain, PureSignal PA linearization | **Not started** |

The short version: **single-receiver RX works end-to-end on any supported ANAN/Hermes radio. Everything beyond that is either scaffolded, cold-wired, or not started.**

### Touring the app

When you launch NereusSDR, you'll see a main window **modelled after [AetherSDR](https://github.com/ten9876/AetherSDR)** — the modern Qt6 FlexRadio console that gave us our architectural template. The overall look, dock structure, and interaction patterns are AetherSDR's. The **feature set and radio behaviour** is a reimagination of [Thetis](https://github.com/ramdor/Thetis) — we're not cloning Thetis's WinForms layout, we're rebuilding its functionality on a cleaner chassis. Here's what's where:

**Main window layout**
- **Spectrum + waterfall widget** dominates the upper-middle of the window. GPU-rendered, ~30 FPS, 4096-point FFT with FFT-shift + mirror, Blackman-Harris window. Click anywhere on the spectrum to retune the VFO. Scroll-wheel over the spectrum drags the reference level; scroll over the frequency bar zooms. Drag the filter passband to resize filters. Right-click for a display-settings popup.
- **Floating VFO marker** with a flag widget showing the current tune frequency, mode, and filter passband overlay. Double-click the marker to recenter the panadapter.
- **Dockable/floatable container panels** around the edges — these hold the meters, applets, and control widgets. You can drag them to float, dock them to any edge, pin them on top, axis-lock them, or hide their title bars. Layout persists across restarts.
- **Menu bar** across the top — 9 menus (File, Radio, View, DSP, Band, Mode, Containers, Tools, Help), ~60 items. Some are wired, some are stubs.
- **Status bar** across the bottom — double-height, shows UTC clock, radio info, TX/RX indicators, signal level, CPU usage.

**Applet panel (right side by default)**
- **12 applets** from Thetis, ported as Qt widgets. The most complete one is **RX** — mode, AGC, AF gain, filter presets are all wired to the live slice. The others (TX, PhoneCw, EQ, FM, Digital, PureSignal, Diversity, CWX, DVK, CAT, Tuner) have their UI built out but most of their controls don't drive anything yet. You can click them, they'll look right, they won't do anything.

**Meter widgets**
- A **GPU-rendered meter engine** powers everything meter-shaped. ~31 item types in the engine: bars, needles, arc-style S-meters, dials, text overlays, rotators, clocks, magic-eye tubes, image atoms, LEDs, history graphs, multi-meter displays, data out, and interactive button grids.
- **What actually updates with real data on your HL2:** the signal S-meter (arc needle), the noise-floor / RF power bars bound to the live RX1 channel, the clocks, and the basic button grids (band, mode, filter, antenna, tuning step).
- **What doesn't update:** most TX-side meters (forward/reverse power, SWR, ALC, compression, mic gain), PureSignal feedback indicators, PA-related gauges, the magic eye, data out. They render their backgrounds and needles-at-zero correctly but there's no source pushing data into them. That's because the underlying pipelines (TX, PureSignal, PA control) are later phases.

**Container settings**
- Right-click a container's title bar → Container Settings. The **3-column container settings dialog** (ported from Thetis) lets you pick meter items from a library, drag them into your container, and edit per-item properties with a huge ~155-field property editor that covers every item type. Snapshot-and-revert works. Container-level lock/hide-title/minimises/highlight work. This is surprisingly complete.
- **38+ meter presets** ship in the item library — pre-built S-meters, CrossNeedle dual forward/reverse power meters, ANANMM 7-needle multi-meter (with exact Thetis calibration curves), edge-style meters, etc. You can drop any of them into any container.

**SetupDialog**
- **47 pages across 10 categories.** The ones that are **fully functional right now**:
  - **Display → Spectrum Defaults** (17 controls: averaging, peak hold, trace fill/width/colour, gradient, cal offset, FPS overlay, etc.)
  - **Display → Waterfall Defaults** (17 controls: AGC, reverse scroll, opacity, update rate, overlays, timestamp, low/mid/high colours)
  - **Display → Grid & Scales** (13 controls, including per-band Max/Min storage across all 14 bands — 160 m through 6 m + GEN + WWV + XVTR)
  - **Hardware Config** (new in Phase 3I — what you'll be testing) with 9 capability-gated sub-tabs
- The **other ~40 pages** mostly exist as stub widgets with `NYI` (Not Yet Implemented) tooltips on disabled controls. You can navigate into them, they won't crash, but they won't affect anything.

**Spectrum overlay panel**
- Hover-reveal overlay button strip on the spectrum widget — 10 buttons with 5 flyout sub-panels (display, filter, noise, spots, tools). Partially wired; auto-closes on outside click.

**Persistence**
- Dock layout, container contents, floating positions, pin-on-top state, per-band grid values, audio device selection, last-connected radio, per-MAC hardware settings (new in 3I), spectrum/waterfall settings — all persist across app restarts via an XML-based `AppSettings` store.

### What actually works across the whole app right now

Not just the Phase 3I connector — this is the **total** functional surface area as of today, on any supported radio:

1. **Discover + connect + disconnect** any P1 or P2 OpenHPSDR radio on your LAN
2. **Tune RX1** anywhere in the HF range (P1 radios go up to 61.44 MHz; P2 boards like Saturn have broader range)
3. **Hear live audio** demodulated through WDSP in USB, LSB, AM, or CW with adjustable AGC, noise blanker (NB1 or NB2), bandpass filter, notch, and volume
4. **See real-time spectrum** (GPU-accelerated, ~30 FPS, configurable averaging, peak hold, colours, grids, labels, FPS overlay)
5. **See real-time waterfall** (GPU-accelerated, configurable colour scheme, AGC, opacity, reverse scroll, timestamps, overlays)
6. **Click-to-tune** on the spectrum and waterfall, scroll-to-zoom, drag-to-adjust-reference-level, drag-filter-passband
7. **CTUN mode** — independent panadapter center and VFO, WDSP frequency shift
8. **Band switching** via the band button grid (14 bands: 160/80/60/40/30/20/17/15/12/10/6 m + GEN + WWV + XVTR), with per-band display settings
9. **Mode switching** via the mode button grid — USB/LSB/AM/SAM/FM/CW/CWL/CWU/DIGL/DIGU/DRM/SPEC
10. **Filter presets** via the filter button grid
11. **VFO display** with per-digit mouse-wheel tuning
12. **Signal S-meter** (arc needle, live from WDSP)
13. **Attenuator / preamp / sample rate** controls for all supported boards, capability-gated per board
14. **Saved radio profiles** keyed by MAC — discover once, re-use next launch with auto-reconnect
15. **Manually-added radios** outside the local subnet via the Add Manually dialog
16. **Full container system** with drag-to-dock, layout persistence, per-container settings
17. **31 meter item types** styled and placed, with 38+ presets in the library
18. **47-page Setup dialog** — Display section fully wired (47 controls), Hardware Config section new in 3I, other pages stubbed

**Single-radio, single-receiver, receive-only.** Everything else listed in the "Planned" column of the phase table above is either not started or scaffolded but inert.

### Where this build is especially rough

Things that work but aren't polished:
- **First discovery click** can freeze the GUI for up to ~10 seconds while the app walks every network interface (VPN tunnels, Docker bridges, etc. all count). This is a one-shot per click; it will not lock up repeatedly. Async discovery rewrite is a follow-up.
- **Meter widgets that aren't bound** to real data show their background graphics and a zero-valued needle. They look broken but they're just waiting for a future phase to hook them up.
- **Applets beyond RX** (TX, PhoneCw, EQ, FM, Digital, PureSignal, Diversity, CWX, DVK, CAT, Tuner) are cosmetic — clicking controls won't affect anything.
- **~40 of the 47 Setup pages** are disabled-controls-with-NYI-tooltips. They won't crash, they just don't do anything.
- **Some menu items** in the menu bar are not yet wired. Mostly harmless — you'll see an action and nothing will happen.
- **Keyboard shortcuts** are partial.
- **Multi-RX** — the app has the multi-receiver foundation in place but the UI only exposes RX1 for now. RX2 hardware support exists on 2-ADC boards; there's no second-receiver spectrum yet.

### Performance expectations

On a modern laptop (M-series Mac, recent AMD/Intel desktop, decent mid-range Linux box):
- **Idle CPU:** ~1–5% after connect
- **Audio:** no dropouts at 48 / 96 / 192 / 384 kHz
- **Spectrum:** ~30 FPS steady on the GPU path
- **Memory:** ~250–500 MB resident depending on container layout
- **First-launch FFTW wisdom caching:** the very first time you run the app it will pop a progress dialog while FFTW plans its optimal FFT routines for your CPU. This takes ~10–60 seconds, once, ever. Subsequent launches skip it.

---

## Before you start

### Things you need

- A **Hermes Lite 2** powered up and on the same Ethernet/Wi-Fi LAN as your computer.
- A recent **firmware** on the HL2 — v70 or newer. The app will refuse to connect to anything older than v70 and tell you why.
- Speakers or headphones plugged into your computer's audio output (NereusSDR decodes audio on the computer side, not on the radio).
- A browser and a GitHub account so you can report what you see.

### Safety

- **Do not transmit.** Do not press the PTT button on a connected mic, do not click MOX, do not hit Tune. TX will not produce RF output in this build even if you try, but it's good habit to assume "alpha = RX only" until told otherwise.
- **Pick a quiet time on the bench** for the first run. If it crashes mid-connect, it will close your audio session and might interfere with any other app using the sound card.

### Get the build

**Option A: pre-built installer (recommended for non-developers)**
Grab the latest build from <https://github.com/boydsoftprez/NereusSDR/releases> — pick the installer for your OS. **NOTE:** the Phase 3I work is currently on PR #12 (branch `feature/phase3i-radio-connector-port`) and may not be in a tagged release yet. If you don't see a release labeled "Phase 3I" or newer, go with Option B.

**Option B: build from source**
```bash
git clone https://github.com/boydsoftprez/NereusSDR.git
cd NereusSDR
git checkout feature/phase3i-radio-connector-port
cmake -S . -B build-clean -G Ninja
cmake --build build-clean -j
```

You need Qt6 (Core, Widgets, Network, Multimedia, Test), CMake 3.20+, and FFTW3. On macOS: `brew install qt@6 cmake ninja fftw`. On Ubuntu: `apt install qt6-base-dev qt6-multimedia-dev libfftw3-dev cmake ninja-build`. On Windows see `docs/build/windows.md` if it exists, otherwise the Qt online installer + MinGW/MSVC both work.

Launch:
- **macOS:** `open build-clean/NereusSDR.app`
- **Linux:** `./build-clean/NereusSDR`
- **Windows:** `build-clean/NereusSDR.exe`

---

## The actual test

### 1. Launch and look around

When the app opens you should see:

- A main window with a dark UI
- A spectrum display that draws a flat baseline (no signal yet)
- A waterfall underneath it (also empty)
- Various meter widgets, menus, and docked panels

**What "success" looks like here:** the app opens, draws its UI, and does not crash within the first 30 seconds of idle. That's it.

**Known slightly-rough edges:** the window might take a second or two to fully paint. Some container frames have hover-reveal title bars — that's intentional. The CPU meter in the status bar should read low single digits when idle.

**If it crashes on launch:** skip to the "Reporting bugs" section at the bottom and send us the crash log.

### 2. Open the Connection Panel and find your HL2

Somewhere in the menus (likely **Radio → Connection…** or a connection button in the UI) there's a **ConnectionPanel** — a dialog listing discovered radios.

1. Open it.
2. Click **Start Discovery**.
3. Within about 5 seconds your HL2 should appear in the list.

**What each column should show for a healthy HL2:**
- **Status dot:** green (online and free)
- **Name:** "Hermes Lite 2"
- **Board:** HermesLite
- **Protocol:** P1
- **IP:** your HL2's IPv4 address on the LAN
- **MAC:** your HL2's MAC
- **Firmware:** 70 or higher (probably 72+)
- **In-Use:** "free"

**✅ Success bar for step 2:** your HL2 shows up in the list within ~5 seconds with all columns populated.

**Known rough edge:** discovery is not async yet — clicking **Start Discovery** may freeze the GUI for up to 10 seconds while it walks every network interface on your computer. On a Mac with lots of VPN tunnels or Docker bridges this can be noticeable. It should not last longer than ~15 seconds. If the GUI is still frozen after 30 seconds, something is wrong and we'd like to know.

**If your HL2 doesn't appear:**
- Is the HL2 powered on and the link light solid?
- Can you ping it from your computer? `ping 192.168.1.something`
- Are you on the same subnet? (Discovery uses broadcast; it won't cross routers.)
- Try clicking **Rescan** or **Stop** then **Start Discovery** again.

### 3. Connect

Double-click your HL2's row (or click it once and click **Connect**).

Within about 2 seconds:
- The state should flip through Connecting → **Connected**
- The connection panel should show your HL2 as the active radio

**✅ Success bar for step 3:** state reaches Connected without a crash. If the app crashes within the first second of clicking Connect, please grab the crash report — there's been a history of WDSP init races that we chased down, and a regression there is worth knowing about.

**Known rough edge:** if your HL2 firmware is below v70, the app will refuse to connect and give you a "Firmware too old" error. That's by design. Flash newer firmware and try again.

### 4. Listen to a live signal — THE BIG ONE

This is the single most important thing to verify.

1. Pick a frequency where you expect a signal to be. Good candidates:
   - **20 m daytime:** tune to **14.200 MHz LSB**
   - **40 m evening:** tune to **7.200 MHz LSB** (note: Lower Side Band on 40 m and below)
   - **17 m daytime:** tune to **18.130 MHz USB**
2. Set the mode to **LSB** or **USB** as appropriate.
3. Adjust volume.

**✅ Primary success criterion for the entire alpha test:**

> You should **hear live radio** through your computer's speakers. Spectrum should show signal peaks where stations are transmitting. Waterfall should scroll and show vertical signal trails. Moving the VFO should tune to different stations, and you should hear them come in and drop out as you sweep.

**If you can listen to a QSO on 20 m through NereusSDR with a live waterfall and working spectrum, the Phase 3I port is a success.** That's the whole thing we're testing.

**Expected UI behaviors at this stage:**
- Spectrum updates ~30 times per second
- Waterfall scrolls smoothly
- S-meter / signal strength indicator moves with incoming signal
- Volume control adjusts audio level
- Clicking somewhere on the spectrum retunes the VFO to that frequency
- Scrolling over a digit of the VFO display increments/decrements that digit

### 5. Change a few things and confirm they work

While listening to a live signal:

- **Attenuator:** find the attenuator control (probably in one of the applets on the right). Slide it from 0 to 30 dB. Background noise should drop by roughly 30 dB audibly and visibly on the spectrum. HL2 supports 0–60 dB in 1 dB steps (different from the other ANAN radios which are 0–31 dB).
- **Sample rate:** open **Setup → Hardware Config → Radio Info**, change the sample rate combo from 48k to 192k or 384k. The spectrum span should widen correspondingly. No crash.
- **Mode:** switch LSB ↔ USB. Audio character should change (on SSB the wrong sideband sounds like Donald Duck).
- **Volume:** the volume control should work.
- **Disconnect and reconnect:** click Disconnect, wait for the spectrum to go quiet, click Connect again. Audio should resume.

**✅ Success bar for step 5:** at least the attenuator, volume, and mode controls visibly/audibly affect what you're hearing, and a disconnect/reconnect round trip doesn't crash.

### 6. Hardware Config tabs

Open **Setup → Hardware Config**. You should see a tab strip at the top with 9 tabs, but **most of them will be hidden** because they don't apply to HL2. For an HL2 you should see only:

- **Radio Info** — read-only labels showing board type, protocol, ADC count, max RX, firmware, MAC, IP, plus a live sample-rate combo and active-RX spinbox
- **HL2 I/O Board** — GPIO / PTT / CW input / aux output assignments
- **Bandwidth Monitor** — LAN PHY rate display, throttle threshold, auto-pause toggle

The tabs for Antenna/ALEX, OC Outputs, XVTR, PureSignal, Diversity, and PA Calibration should all be **hidden** — HL2 doesn't have that hardware.

**✅ Success bar for step 6:** Radio Info populates with your actual HL2 info; the other HL2-specific tabs are visible; the non-HL2 tabs are not.

### 7. Quit cleanly

Close the app (red close button, or File → Quit, or ⌘Q on macOS, Alt+F4 on Windows).

**✅ Success bar for step 7:** the app closes without a crash dialog and without leaving a new crash report in your system's diagnostic reports folder (see "Reporting bugs" below for where to find it on each OS).

### 8. Relaunch

Open the app again. If you had your HL2 connected when you quit, it should **silently auto-reconnect** within a few seconds without you needing to open ConnectionPanel.

**✅ Success bar for step 8:** auto-reconnect works, or at worst it silently gives up without popping an error.

---

## What is NOT wired up (don't file bugs for these)

This is the list of things you will see in the UI that look like they should work but are intentionally stubbed for this phase. Please **do not file bugs** against these — they're tracked in the Phase 3I design doc §9 and the Phase 3I verification doc. Later phases will light them up.

### Transmit, period

- **No TX audio.** The SSB modulator doesn't exist yet. Keying up (MOX button, PTT input, Tune button) will put command bytes on the wire asking the radio to transmit, but there is no I/Q audio being generated to transmit with. On HL2 specifically you should not see the radio's TX LED come on if you try, because the MOX state machine hasn't been fully wired either. **But don't test this. Alpha = RX only.**
- **No CW transmit.** Same reason.
- **No PureSignal linearization.** The PureSignal tab is hidden for HL2 anyway.
- **No external PTT input routing** — the HL2 I/O Board tab has combos for PTT pin, CW key pin, and aux outputs, but they just persist the setting. They don't drive real GPIO yet. Phase 3L.
- **No aux output assignments active** — same story. Tab remembers your pick, nothing acts on it yet.

### Controls that look live but are cold

- **PureSignal enable** (hidden for HL2 but visible for 2-ADC boards) — state persists, no DSP runs.
- **PA Calibration** tables — state persists, no real calibration.
- **Bandwidth Monitor** live PHY rate — label shows "— Mbps" because no real feed is wired yet. The app does watch for HL2 LAN throttling internally via a sequence-gap heuristic, but the rate display itself is a Phase 3L item.
- **XVTR transverter editor** (hidden for HL2) — table works, but nothing downstream acts on the entries yet.
- **OC Outputs mask grid** (hidden for HL2) — persists, doesn't drive relays.

### UI features not yet in Phase 3I

- **TCI protocol server** (for N1MM+, Log4OM, etc.) — Phase 3J.
- **CAT / rigctld** (for logging programs) — Phase 3K.
- **Up to 4 independent panadapters in configurable layouts** — Phase 3F. You get RX1 only in the spectrum panel for now.
- **Skin import from Thetis** — Phase 3H.
- **Firmware flashing from within the app** — not planned for Phase 3I.
- **Multi-radio simultaneous connection** — not planned at all; one radio at a time.

### Things that might look weird but aren't bugs

- **The atoms / meter items that aren't wired up.** You'll see meter widgets in the UI that don't update or have placeholder values. The meter engine supports ~31 item types (bars, needles, dials, text overlays, rotators, clocks, magic-eye displays, data out, etc.), and many of them are fully styled but not yet bound to real data streams. The ones that DO work for HL2: signal S-meter, spectrum/waterfall, the clock widgets. The ones that don't: most TX meters (power, ALC, compression, SWR), most PA-related meters, magic eye, data out. If a meter isn't moving, assume it's not wired up rather than broken.
- **Some setup-dialog pages still say "NYI" (Not Yet Implemented)** on their tooltips. Those are the pages that haven't been ported from Thetis yet.
- **Right-click context menus** are minimal in some places.
- **Keyboard shortcuts** are partial.
- **The continuous radio re-discovery** behavior you might expect from some other SDR apps is not present — discovery is one-shot, user-triggered. This was a deliberate fix during Phase 3I smoke testing because the original implementation was blocking the GUI for 15+ seconds every 5 seconds.

---

## What we'd love you to try that's unusual

Beyond the basic "can I hear signals" test, these are the spicier scenarios we haven't hit often:

1. **Leave the app connected for an hour or two.** Does CPU usage climb over time? Does audio glitch? Does the spectrum stop updating? (We're hunting for memory leaks and slow-drift bugs that only show up after extended runs.)
2. **Pull the HL2's Ethernet cable** while connected. The app should detect the silence within 2 seconds, transition to an Error state, and try to reconnect up to 3 times at 5-second intervals. Plug the cable back in during that window — it should recover. If you wait longer than ~20 seconds, it should give up and stay in Error until you click Connect again.
3. **Let your HL2 be somewhere unusual on your network** — behind a VPN, on a different subnet accessible via static route, or on the other end of a LAN bridge. Broadcast discovery won't find it but the manual-add dialog (**Add Manually…** button in ConnectionPanel) lets you type in the IP directly. Worth confirming that works.
4. **Change the sample rate while listening** to a signal. Does audio stay connected? Does the spectrum span update correctly?
5. **Try Add Manually with a wrong IP.** Does the app handle "radio doesn't respond" gracefully, or does it freeze?

---

## Reporting bugs

When something goes wrong, the more of these you can include, the faster we can fix it:

### 1. What you were doing

One line describing what you clicked / typed right before the problem. Example: *"I clicked Connect on the HL2 row and about 0.5 s later the app crashed."*

### 2. Which version

```bash
cd NereusSDR
git rev-parse HEAD
```

Or, if you built from a release, the version string from **Help → About**.

### 3. The app log

NereusSDR writes every log line to a file. Find it:
- **macOS:** `~/Library/Application Support/NereusSDR/nereussdr.log` (or somewhere under `~/Library/Preferences/NereusSDR/`)
- **Linux:** `~/.config/NereusSDR/nereussdr.log`
- **Windows:** `%LOCALAPPDATA%\NereusSDR\nereussdr.log`

The file is rewritten on each launch, so grab it *before* you relaunch the app after a crash.

### 4. Any crash report

If the OS popped a crash dialog, there's a full crash log somewhere:
- **macOS:** `~/Library/Logs/DiagnosticReports/NereusSDR-*.ips` (most recent one)
- **Linux:** usually in `/var/log/apport/` or as a core dump in your home dir, depending on distro
- **Windows:** Event Viewer → Windows Logs → Application, find the NereusSDR entry

Attach the whole file to the bug report. It's usually 30–100 KB of text.

### 5. Your HL2 firmware version

You can read it from the **Setup → Hardware Config → Radio Info** tab once you're connected, or look at the HL2's built-in web UI.

### 6. File the report

- Easy: comment on [PR #12](https://github.com/boydsoftprez/NereusSDR/pull/12) with your findings
- Or: open a new issue at <https://github.com/boydsoftprez/NereusSDR/issues/new> and tag it `alpha-test` / `hl2` / `phase-3i` as appropriate

**What's useful:**
- Repro steps
- What you expected vs what happened
- Logs + crash reports if available
- Screenshots for visual bugs
- Your OS version

**What's not useful (we already know):**
- "TX doesn't work" — intentional, see above
- "Meter X doesn't move" — intentional, see above
- "UI element Y looks rough" — known, alpha stage

---

## Bottom line

If at the end of this test you:

1. Launched the app
2. Discovered your HL2 in ConnectionPanel
3. Connected to it without a crash
4. Heard a live SSB QSO with a working spectrum and waterfall
5. Quit cleanly

…then we've hit our **alpha-test success criterion** for Phase 3I. Everything beyond that is a bonus.

**Thank you for testing.** We're trying to build a modern, open, cross-platform replacement for Thetis, and we can't do it without people running the code on real hardware and telling us what breaks. If you send us one usable bug report with logs attached, you've already contributed more than most people ever will to an open-source radio project.

73 de the NereusSDR team
