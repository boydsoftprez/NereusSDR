# NereusSDR Alpha Test — OpenHPSDR Radios (P1 & P2)

**Hi, and thank you.** You're helping test an **alpha-stage** SDR console. This doc walks you through what to try, what "success" looks like, and — just as important — what does **not** work yet so you don't waste time filing bugs for features we know are missing.

**What this build is:**
- A ground-up C++/Qt6 port of [Thetis](https://github.com/ramdor/Thetis) (the Apache Labs / OpenHPSDR console), architecturally modelled on [AetherSDR](https://github.com/ten9876/AetherSDR).
- Supports the **full OpenHPSDR family** over **both Protocol 1 (P1)** and **Protocol 2 (P2)**. That includes the Apache Labs ANAN line (100/100B/100D/10/10E/200D/7000/8000/G2-Saturn), Hermes / Hermes Lite 2, Metis, Angelia, Orion, Orion MkII, and any other board that speaks the OpenHPSDR discovery + frame protocols.
- The **goal of your test** is simple: can NereusSDR discover your radio, connect, and let you listen to a live SSB signal with a working spectrum and waterfall?

**What this build is NOT:**
- Ready for transmit. **Do not key up.** The TX pipeline is intentionally cold — commands go on the wire but there is no SSB modulator yet.
- Feature-complete. Dozens of controls you see in the UI are placeholder — more on that below.
- Signed on macOS or Windows. First launch on macOS needs right-click → Open. First launch on Windows triggers a SmartScreen warning you'll need to click through.

---

## Supported radios

| Family | Protocol | State |
|---|---|---|
| ANAN-G2 / Saturn | P2 | Primary dev target — fully tested |
| ANAN-7000DLE / 8000DLE | P2 | Should work (same P2 code path as G2) |
| ANAN-200D / 100D / 100B / 100 / 10 / 10E | P1 | Works — per-board capability gating |
| Hermes / Hermes Lite 2 | P1 | Works — HL2 has its own I/O Board tab |
| Metis / Angelia / Orion / Orion MkII | P1 | Works — same P1 discovery + frame parser |

If your radio implements stock OpenHPSDR P1 or P2 protocol, the app will try to talk to it. Capability differences (ADC count, max receivers, preamp ranges, attenuator step size, sample rates, PureSignal / Diversity / XVTR presence) are driven by a per-board registry — if your board shows up under a wrong name or wrong capabilities, that's a bug we want to hear about.

---

## What the whole app looks like right now

Before you dive in, here's the honest state of the overall application so you have realistic expectations. NereusSDR is built in layered phases — each phase adds one focused slice of functionality. The slices landed so far, in rough order:

| Phase | What it added | State |
|---|---|---|
| 3A | Protocol 2 radio discovery + connection (ANAN-G2 / Saturn first) | Working |
| 3B | WDSP DSP engine integration — RX1 demod (USB/LSB/AM/CW), AGC, NB1/NB2, bandpass filters, audio pipeline | Working |
| 3C | macOS build + crash fixes | Working |
| 3D | GPU-accelerated spectrum + waterfall via Qt's QRhi abstraction (Metal on macOS, D3D12 on Windows, OpenGL on Linux) | Working |
| 3E | VFO tuning, mode selection, filter controls, multi-receiver foundation | Working (RX1 only in the UI) |
| 3-UI | Full UI skeleton: 12 applets, 9 menus, 47-page SetupDialog, spectrum overlay panel, status bar | Skeleton + partial wiring |
| 3F | Up to 4 independent panadapters in configurable layouts | **Not started** |
| 3G-1..7 | Dockable/floatable container system, GPU-rendered meter engine with 31 item types, container settings dialog, MMIO external-data subsystem | Working |
| 3G-8 | RX1 Display parity — every control on the Setup → Display pages wired to the renderer (47 controls) | Working |
| 3G-9 | Display refactor — Clarity Blue waterfall palette, ClarityController adaptive auto-tune (EWMA + deadband + noise floor estimator), per-band Clarity memory, zoom persistence, Thetis-cited tooltips on 47 setup controls | Working |
| 3G-10 | RX DSP parity — 10 WDSP feature slices wired (AGC advanced, EMNR/NR2, SNB, APF, squelch, mute/pan/binaural, NB2 advanced, RIT/XIT, frequency lock, mode containers FM/DIG/RTTY), per-slice-per-band persistence, VfoWidget 4-tab rewrite with S-meter | Working |
| 3G-11 | P1 field fixes — VFO frequency encoding as raw Hz | Working |
| 3G-13 | Step attenuator — Classic + Adaptive auto-attenuation, ADC overflow detection, OVL status badge, per-model preamp items | Working |
| 3G-14 | About dialog + built-in AI-assisted issue reporter (💡 menu bar widget) | Working |
| 3H | Thetis-inspired skin system | **Not started** |
| 3I | **Protocol 1 support** for the full ANAN/Hermes family including HL2 (discovery, connection, capability gating, Hardware page, saved radios, auto-reconnect) | Working |
| 3J | TCI protocol server (for N1MM+, Log4OM etc.) | Not started |
| 3K | CAT / rigctld bridge | Not started |
| 3L | HL2-specific I2C-over-ep2 encoding (currently inside a closed DLL) | Not started |
| 3M | TX pipeline — SSB, CW, full processing chain, PureSignal PA linearization | **Not started** |
| 3N | **Release pipeline + /release skill + GPG-signed alpha binaries** | Working — this is how you got the build |

The short version: **single-receiver RX works end-to-end on any supported OpenHPSDR radio, P1 or P2. Everything beyond that is either scaffolded, cold-wired, or not started.**

### Touring the app

When you launch NereusSDR, you'll see a main window **modelled after [AetherSDR](https://github.com/ten9876/AetherSDR)** — the modern Qt6 FlexRadio console that gave us our architectural template. The overall look, dock structure, and interaction patterns are AetherSDR's. The **feature set and radio behaviour** is a reimagination of [Thetis](https://github.com/ramdor/Thetis) — we're not cloning Thetis's WinForms layout, we're rebuilding its functionality on a cleaner chassis. Here's what's where:

**Main window layout**
- **Spectrum + waterfall widget** dominates the upper-middle of the window. GPU-rendered (Metal on macOS, D3D12 on Windows, OpenGL on Linux), ~30 FPS, 4096-point FFT with FFT-shift + mirror, Blackman-Harris window. Click anywhere on the spectrum to retune the VFO. Scroll-wheel over the spectrum drags the reference level; scroll over the frequency bar zooms. Drag the filter passband to resize filters. Right-click for a display-settings popup. The **Clarity Blue** waterfall palette with adaptive auto-tune adjusts noise floor and gain per-band automatically.
- **Floating VFO marker** with a flag widget showing the current tune frequency, mode, and filter passband overlay. Double-click the marker to recenter the panadapter.
- **Dockable/floatable container panels** around the edges — these hold the meters, applets, and control widgets. You can drag them to float, dock them to any edge, pin them on top, axis-lock them, or hide their title bars. Layout persists across restarts.
- **Menu bar** across the top — 9 menus (File, Radio, View, DSP, Band, Mode, Containers, Tools, Help), ~60 items. Some are wired, some are stubs.
- **Status bar** across the bottom — double-height, shows UTC clock, radio info, TX/RX indicators, signal level, CPU usage.

**Applet panel (right side by default)**
- **12 applets** from Thetis, ported as Qt widgets. The most complete one is **RX** — mode, AGC, AF gain, filter presets, step attenuator (Classic + Adaptive auto-att), preamp, ADC overflow (OVL) badge, and S-meter are all wired to the live slice. The **VfoWidget** floats above the spectrum with a 4-tab layout (Audio/DSP/Mode/X-RIT), a 4x2 DSP toggle grid, AGC 5-button row, and S-meter level bar with dBm readout. The other applets (TX, PhoneCw, EQ, FM, Digital, PureSignal, Diversity, CWX, DVK, CAT, Tuner) have their UI built out but most of their controls don't drive anything yet.

**Meter widgets**
- A **GPU-rendered meter engine** powers everything meter-shaped. ~31 item types in the engine: bars, needles, arc-style S-meters, dials, text overlays, rotators, clocks, magic-eye tubes, image atoms, LEDs, history graphs, multi-meter displays, data out, and interactive button grids.
- **What actually updates with real data** (any supported radio): the signal S-meter (arc needle + VfoWidget level bar with dBm readout), the noise-floor / RF power bars bound to the live RX1 channel, the clocks, and the basic button grids (band, mode, filter, antenna, tuning step). The S-meter in the VfoWidget shows a cyan→green gradient at the S9 boundary.
- **What doesn't update:** most TX-side meters (forward/reverse power, SWR, ALC, compression, mic gain), PureSignal feedback indicators, PA-related gauges, the magic eye, data out. They render their backgrounds and needles-at-zero correctly but there's no source pushing data into them. That's because the underlying pipelines (TX, PureSignal, PA control) are later phases.

**Container settings**
- Right-click a container's title bar → Container Settings. The **3-column container settings dialog** (ported from Thetis) lets you pick meter items from a library, drag them into your container, and edit per-item properties with a huge ~155-field property editor that covers every item type. Snapshot-and-revert works. Container-level lock/hide-title/minimises/highlight work. This is surprisingly complete.
- **38+ meter presets** ship in the item library — pre-built S-meters, CrossNeedle dual forward/reverse power meters, ANANMM 7-needle multi-meter (with exact Thetis calibration curves), edge-style meters, etc. You can drop any of them into any container.

**SetupDialog**
- **47 pages across 10 categories.** The ones that are **fully functional right now**:
  - **Display → Spectrum Defaults** (17 controls: averaging, peak hold, trace fill/width/colour, gradient, cal offset, FPS overlay, Clarity adaptive auto-tune, etc.)
  - **Display → Waterfall Defaults** (17 controls: AGC, reverse scroll, opacity, update rate, overlays, timestamp, low/mid/high colours, Clarity Blue palette)
  - **Display → Grid & Scales** (13 controls, including per-band Max/Min storage across all 14 bands — 160 m through 6 m + GEN + WWV + XVTR)
  - **General → Options** — step attenuator configuration (Classic / Adaptive auto-att modes)
  - **Hardware Config** — 9 capability-gated sub-tabs. Which tabs you see depends on your radio. On an HL2 most tabs hide because the hardware isn't there; on an ANAN-7000 / 8000 / G2 all 9 tabs are visible.
- The **other ~35 pages** mostly exist as stub widgets with `NYI` (Not Yet Implemented) tooltips on disabled controls. You can navigate into them, they won't crash, but they won't affect anything.

**Spectrum overlay panel**
- Hover-reveal overlay button strip on the spectrum widget — 10 buttons with 5 flyout sub-panels (display, filter, noise, spots, tools). Partially wired; auto-closes on outside click.

**Persistence**
- Dock layout, container contents, floating positions, pin-on-top state, per-band grid values, audio device selection, last-connected radio, per-MAC hardware settings, spectrum/waterfall settings — all persist across app restarts via an XML-based `AppSettings` store.

### What actually works across the whole app right now

Not just the Phase 3I connector — this is the **total** functional surface area as of today, on any supported radio:

1. **Discover + connect + disconnect** any P1 or P2 OpenHPSDR radio on your LAN
2. **Tune RX1** anywhere in the HF range (P1 radios go up to 61.44 MHz; P2 boards like Saturn have broader range)
3. **Hear live audio** demodulated through WDSP in USB, LSB, AM, CW, or FM with adjustable AGC (basic + advanced: threshold/hang/slope/attack/decay), noise blanker (NB1 or NB2 with advanced params), EMNR (NR2), SNB, APF, squelch (SSB/AM/FM 3-variant), bandpass filter, notch, mute, audio pan, binaural, and volume
4. **See real-time spectrum** (GPU-accelerated, ~30 FPS, configurable averaging, peak hold, colours, grids, labels, FPS overlay)
5. **See real-time waterfall** (GPU-accelerated, Clarity Blue palette with adaptive auto-tune, configurable colour scheme, AGC, opacity, reverse scroll, timestamps, overlays)
6. **Click-to-tune** on the spectrum and waterfall, scroll-to-zoom, drag-to-adjust-reference-level, drag-filter-passband
7. **CTUN mode** — independent panadapter center and VFO, WDSP frequency shift
8. **Band switching** via the band button grid (14 bands: 160/80/60/40/30/20/17/15/12/10/6 m + GEN + WWV + XVTR), with per-band display settings and **per-band Clarity memory**
9. **Mode switching** via the mode button grid — USB/LSB/AM/SAM/FM/CW/CWL/CWU/DIGL/DIGU/DRM/SPEC, with mode containers (FM OPT/DIG/RTTY)
10. **Filter presets** via the filter button grid
11. **VFO display** with per-digit mouse-wheel tuning, **RIT/XIT client offset**, and **frequency lock**
12. **Signal S-meter** (arc needle + VfoWidget level bar with dBm readout, cyan→green gradient at S9, live from WDSP)
13. **Step attenuator** with Classic + Adaptive auto-attenuation modes, hysteresis, and per-MAC persistence. **ADC overflow detection** with OVL status badge. **Preamp** controls capability-gated per board (HL2 has its own 0–60 dB range; ANAN boards use stock 0–31 dB / 0–61 dB ranges with per-model preamp items from Thetis)
14. **Per-slice-per-band DSP persistence** — all DSP settings saved per band, restored automatically on band change
15. **Saved radio profiles** keyed by MAC — discover once, re-use next launch with auto-reconnect
16. **Manually-added radios** outside the local subnet via the Add Manually dialog
17. **Full container system** with drag-to-dock, layout persistence, per-container settings
18. **31 meter item types** styled and placed, with 38+ presets in the library
19. **47-page Setup dialog** — Display section fully wired (47 controls), Hardware Config section with capability-gated tabs, General → Options for step ATT config, other pages stubbed
20. **About dialog** (Help → About NereusSDR) — version, build info, Qt/WDSP versions, heritage credits, license, GPG fingerprint
21. **Built-in issue reporter** — click the 💡 in the menu bar to file bug reports or feature requests directly to GitHub with structured templates
22. **Zoom persistence** — visible bandwidth saved/restored across restarts

**Single-radio, single-receiver, receive-only.** Everything else listed in the "Planned" column of the phase table above is either not started or scaffolded but inert.

### Where this build is especially rough

Things that work but aren't polished:
- **First discovery click** can freeze the GUI for up to ~10 seconds while the app walks every network interface (VPN tunnels, Docker bridges, etc. all count). This is a one-shot per click; it will not lock up repeatedly. Async discovery rewrite is a follow-up.
- **Meter widgets that aren't bound** to real data show their background graphics and a zero-valued needle. They look broken but they're just waiting for a future phase to hook them up.
- **Applets beyond RX** (TX, PhoneCw, EQ, FM, Digital, PureSignal, Diversity, CWX, DVK, CAT, Tuner) are cosmetic — clicking controls won't affect anything.
- **~40 of the 47 Setup pages** are disabled-controls-with-NYI-tooltips. They won't crash, they just don't do anything.
- **Some menu items** in the menu bar are not yet wired. Mostly harmless — you'll see an action and nothing will happen.
- **Keyboard shortcuts** are partial.
- **Multi-RX** — the app has the multi-receiver foundation in place but the UI only exposes RX1 for now. RX2 hardware support exists on 2-ADC boards (ANAN-7000 / 8000 / G2 / Orion MkII); there's no second-receiver spectrum yet.

### Performance expectations

On a modern laptop (M-series Mac, recent AMD/Intel desktop, decent mid-range Linux box):
- **Idle CPU:** ~1–5% after connect
- **Audio:** no dropouts at 48 / 96 / 192 / 384 kHz where the hardware supports it (HL2 maxes at 384 k; ANAN boards go higher)
- **Spectrum:** ~30 FPS steady on the GPU path
- **Memory:** ~250–500 MB resident depending on container layout
- **First-launch FFTW wisdom caching:** the very first time you run the app it will pop a progress dialog while FFTW plans its optimal FFT routines for your CPU. This can take up to ~15 minutes depending on your hardware. It only happens once; results are cached under `~/.config/NereusSDR/` for subsequent launches.

---

## Before you start

### Things you need

- An **OpenHPSDR-compatible radio** (see "Supported radios" above) powered up and on the same Ethernet/Wi-Fi LAN as your computer.
- Recent firmware on your radio. There are no hard-coded firmware minimums — if your radio speaks the OpenHPSDR protocol, NereusSDR will try to talk to it. That said, we recommend:
  - **Hermes Lite 2:** v70 or newer for best results.
  - **ANAN-G2 / Saturn:** recent radioberry / Saturn firmware — the P2 path has been tested against current production firmware.
  - **Other ANAN / Hermes / Metis / Angelia / Orion:** run whatever firmware you'd use with Thetis. If Thetis connects, NereusSDR should.
- Speakers or headphones plugged into your computer's audio output (NereusSDR decodes audio on the computer side, not on the radio).
- A browser and a GitHub account so you can report what you see.

### Safety

- **Do not transmit.** Do not press the PTT button on a connected mic, do not click MOX, do not hit Tune. TX will not produce RF output in this build even if you try, but it's good habit to assume "alpha = RX only" until told otherwise.
- **Pick a quiet time on the bench** for the first run. If it crashes mid-connect, it will close your audio session and might interfere with any other app using the sound card.

### Get the build

**Option A: pre-built binary (recommended for non-developers)**

Grab the latest release from <https://github.com/boydsoftprez/NereusSDR/releases>. Pick the right file for your OS:

| Platform | File |
|---|---|
| Linux x86_64 | `NereusSDR-vX.Y.Z-x86_64.AppImage` |
| Linux aarch64 (Pi / ARM server) | `NereusSDR-vX.Y.Z-aarch64.AppImage` |
| macOS Apple Silicon | `NereusSDR-vX.Y.Z-macOS-apple-silicon.dmg` |
| Windows (installer) | `NereusSDR-vX.Y.Z-Windows-x64-setup.exe` |
| Windows (portable zip) | `NereusSDR-vX.Y.Z-Windows-x64-portable.zip` |

**Intel Mac:** no pre-built binary yet — GitHub Actions retired the free Intel macOS runner. Build from source (Option B).

Every artifact is detached-GPG-signed (key `KG4VCF`). To verify before running:

```bash
gpg --keyserver keyserver.ubuntu.com --recv-keys KG4VCF
gpg --verify SHA256SUMS.txt.asc SHA256SUMS.txt
sha256sum -c SHA256SUMS.txt
```

**Option B: build from source**

```bash
git clone https://github.com/boydsoftprez/NereusSDR.git
cd NereusSDR
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

### 2. Open the Connection Panel and find your radio

Somewhere in the menus (likely **Radio → Connection…** or a connection button in the UI) there's a **ConnectionPanel** — a dialog listing discovered radios.

1. Open it.
2. Click **Start Discovery**.
3. Within about 5 seconds your radio should appear in the list.

**What each column should show for a healthy radio:**
- **Status dot:** green (online and free)
- **Name:** the board name (`Hermes Lite 2`, `ANAN-G2`, `ANAN-7000DLE`, `Hermes`, `Orion`, etc.)
- **Board:** the detected board type
- **Protocol:** `P1` for HL2 / Hermes / Metis / older ANAN, `P2` for ANAN-G2 / 7000 / 8000
- **IP:** your radio's IPv4 address on the LAN
- **MAC:** your radio's MAC
- **Firmware:** the firmware version the radio reported
- **In-Use:** `free` (or `busy` if another client is connected)

**✅ Success bar for step 2:** your radio shows up in the list within ~5 seconds with all columns populated, board type detected correctly, and protocol reported correctly for that board (P1 for Hermes-family, P2 for ANAN-G2/7000/8000).

**Known rough edge:** discovery is not async yet — clicking **Start Discovery** may freeze the GUI for up to 10 seconds while it walks every network interface on your computer. On a Mac with lots of VPN tunnels or Docker bridges this can be noticeable. It should not last longer than ~15 seconds. If the GUI is still frozen after 30 seconds, something is wrong and we'd like to know.

**If your radio doesn't appear:**
- Is the radio powered on and the link light solid?
- Can you ping it from your computer? `ping 192.168.1.something`
- Are you on the same subnet? (Discovery uses broadcast; it won't cross routers.)
- Try clicking **Rescan** or **Stop** then **Start Discovery** again.
- If the radio is on a different subnet or behind a VPN, use **Add Manually…** and type the IP directly.

### 3. Connect

Double-click your radio's row (or click it once and click **Connect**).

Within about 2 seconds:
- The state should flip through Connecting → **Connected**
- The connection panel should show your radio as the active radio

**✅ Success bar for step 3:** state reaches Connected without a crash. If the app crashes within the first second of clicking Connect, please grab the crash report — there's been a history of WDSP init races that we chased down, and a regression there is worth knowing about.

**Known rough edge:** there are no hard-coded firmware minimums (those were dropped in v0.1.4), so even older firmware should attempt to connect. If the board doesn't respond correctly to the protocol handshake, the app will bail with an error. Older HL2 firmware (below v70) may exhibit quirks — we recommend updating if you can.

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

**If you can listen to a QSO on 20 m through NereusSDR with a live waterfall and working spectrum, the alpha is a success.** That's the whole thing we're testing.

**Expected UI behaviors at this stage:**
- Spectrum updates ~30 times per second
- Waterfall scrolls smoothly
- S-meter / signal strength indicator moves with incoming signal
- Volume control adjusts audio level
- Clicking somewhere on the spectrum retunes the VFO to that frequency
- Scrolling over a digit of the VFO display increments/decrements that digit

### 5. Change a few things and confirm they work

While listening to a live signal:

- **Step attenuator / preamp:** find the ATT/S-ATT row in the RX applet. The step attenuator has two modes: **Classic** (manual slider) and **Adaptive** (auto-attenuation with hysteresis). Slide the attenuator across its range — background noise should drop audibly and visibly on the spectrum. The range depends on your board (HL2: 0–60 dB in 1 dB steps; Hermes/ANAN-10/100/100B: preamp on/off plus 0–31 dB; ANAN-100D/200D/7000/8000/G2: 0–31 dB or 0–61 dB with per-model preamp items). If the control's range matches your board, capability gating is working. Watch for the **OVL** (ADC overload) badge — it should light up if the ADC is clipping.
- **DSP controls:** in the VfoWidget's DSP tab, toggle NR2 (EMNR), SNB, APF, NB2. Each should audibly affect the received audio. Try the AGC row (OFF/SLOW/MED/FAST/CUSTOM) — switching AGC modes should change how the audio responds to signal level changes.
- **Squelch:** try the squelch control — on SSB/AM/FM it should mute audio below the threshold. FM squelch behaves differently than SSB squelch (three variants ported from Thetis).
- **Mode containers:** switch to FM, DIG, or RTTY mode. Each has its own option container with mode-specific controls.
- **Sample rate:** open **Setup → Hardware Config → Radio Info**, change the sample rate combo. Available rates depend on the board (HL2: 48/96/192/384 k; ANAN: usually 48/96/192/384 k, some boards go higher on P2). The spectrum span should widen/narrow correspondingly. No crash on rate change.
- **Mode:** switch LSB ↔ USB. Audio character should change (on SSB the wrong sideband sounds like Donald Duck).
- **RIT/XIT:** in the VfoWidget X-RIT tab, enable RIT and adjust the offset. The received audio should shift without changing the displayed VFO frequency.
- **Volume:** the volume control should work.
- **Disconnect and reconnect:** click Disconnect, wait for the spectrum to go quiet, click Connect again. Audio should resume.
- **Band change persistence:** switch bands a few times. When you return to a previous band, all your DSP settings (AGC mode, NR, NB, squelch level, etc.) should be restored exactly as you left them.

**✅ Success bar for step 5:** at least the attenuator, DSP toggles, volume, and mode controls visibly/audibly affect what you're hearing, band-change persistence restores your settings, and a disconnect/reconnect round trip doesn't crash.

### 6. Hardware Config tabs

Open **Setup → Hardware Config**. You should see a tab strip at the top with **up to 9 tabs**, but **which tabs are visible depends on your radio's capabilities.** This is capability gating — NereusSDR inspects your board's feature bits and hides tabs that don't apply.

Reference table:

| Tab | Radios that show it |
|---|---|
| **Radio Info** | All radios |
| **HL2 I/O Board** | Hermes Lite 2 only |
| **Bandwidth Monitor** | Hermes Lite 2 only |
| **Antenna / ALEX** | All boards with ALEX hardware (Hermes / ANAN / Orion / Saturn) |
| **OC Outputs** | Boards with Open Collector outputs |
| **XVTR** | Boards supporting transverter offsets |
| **PureSignal** | 2-ADC boards with feedback path (ANAN-100D / 200D / 7000 / 8000 / G2 / Orion MkII) |
| **Diversity** | 2-ADC boards supporting diversity reception |
| **PA Calibration** | Boards with PA power-sensing hardware |

For example:
- An **HL2** sees Radio Info + HL2 I/O Board + Bandwidth Monitor (most others hidden).
- An **ANAN-100** sees Radio Info + Antenna/ALEX + OC Outputs + XVTR + PA Calibration (no PureSignal, no Diversity, no HL2 tabs).
- An **ANAN-G2 / 7000 / 8000** sees all 9 minus the HL2-specific pair.

**✅ Success bar for step 6:** Radio Info populates with your actual board info (correct board name, ADC count, max RX, firmware, MAC, IP). The tabs you do see match the capability table above for your board. The tabs you don't see are hidden because your board doesn't have that hardware.

### 7. Quit cleanly

Close the app (red close button, or File → Quit, or ⌘Q on macOS, Alt+F4 on Windows).

**✅ Success bar for step 7:** the app closes without a crash dialog and without leaving a new crash report in your system's diagnostic reports folder (see "Reporting bugs" below for where to find it on each OS).

### 8. Relaunch

Open the app again. If you had your radio connected when you quit, it should **silently auto-reconnect** within a few seconds without you needing to open ConnectionPanel.

**✅ Success bar for step 8:** auto-reconnect works, or at worst it silently gives up without popping an error.

---

## What is NOT wired up (don't file bugs for these)

This is the list of things you will see in the UI that look like they should work but are intentionally stubbed for this phase. Please **do not file bugs** against these — they're tracked in the Phase 3I / 3L / 3M design docs. Later phases will light them up.

### Transmit, period

- **No TX audio.** The SSB modulator doesn't exist yet. Keying up (MOX button, PTT input, Tune button) will put command bytes on the wire asking the radio to transmit, but there is no I/Q audio being generated to transmit with. **Don't test this. Alpha = RX only.**
- **No CW transmit.** Same reason.
- **No PureSignal linearization.** Scaffold only; no DSP behind the switch.
- **No external PTT input routing** — the HL2 I/O Board tab and the ANAN OC Outputs tab have combos for PTT pin, CW key pin, and aux outputs, but they just persist the setting. They don't drive real GPIO yet. Phase 3L / Phase 3M.

### Controls that look live but are cold

- **PureSignal enable** (visible on 2-ADC boards) — state persists, no DSP runs.
- **Diversity enable** (visible on 2-ADC boards) — state persists, no second-RX fusion runs.
- **PA Calibration** tables — state persists, no real calibration.
- **Bandwidth Monitor** live PHY rate (HL2) — label shows `— Mbps` because no real feed is wired yet. The app does watch for HL2 LAN throttling internally via a sequence-gap heuristic, but the rate display itself is a Phase 3L item.
- **XVTR transverter editor** — table works, but nothing downstream acts on the entries yet.
- **OC Outputs mask grid** — persists, doesn't drive relays.

### UI features not yet in this build

- **TCI protocol server** (for N1MM+, Log4OM, etc.) — Phase 3J.
- **CAT / rigctld** (for logging programs) — Phase 3K.
- **Up to 4 independent panadapters in configurable layouts** — Phase 3F. You get RX1 only in the spectrum panel for now.
- **Second receiver (RX2)** — hardware support exists on 2-ADC boards (ANAN-7000/8000/G2/Orion MkII) but there's no UI for it yet.
- **Skin import from Thetis** — Phase 3H.
- **Firmware flashing from within the app** — not planned for this phase.
- **Multi-radio simultaneous connection** — not planned at all; one radio at a time.

### Things that might look weird but aren't bugs

- **The atoms / meter items that aren't wired up.** You'll see meter widgets in the UI that don't update or have placeholder values. The meter engine supports ~31 item types (bars, needles, dials, text overlays, rotators, clocks, magic-eye displays, data out, etc.), and many of them are fully styled but not yet bound to real data streams. The ones that DO work on any supported radio: signal S-meter, spectrum/waterfall, the clock widgets. The ones that don't: most TX meters (power, ALC, compression, SWR), most PA-related meters, magic eye, data out. If a meter isn't moving, assume it's not wired up rather than broken.
- **Some setup-dialog pages still say "NYI" (Not Yet Implemented)** on their tooltips. Those are the pages that haven't been ported from Thetis yet.
- **Right-click context menus** are minimal in some places.
- **Keyboard shortcuts** are partial.
- **The continuous radio re-discovery** behavior you might expect from some other SDR apps is not present — discovery is one-shot, user-triggered. This was a deliberate fix because the original implementation was blocking the GUI for 15+ seconds every 5 seconds.

---

## What we'd love you to try that's unusual

Beyond the basic "can I hear signals" test, these are the spicier scenarios we haven't hit often:

1. **Leave the app connected for an hour or two.** Does CPU usage climb over time? Does audio glitch? Does the spectrum stop updating? (We're hunting for memory leaks and slow-drift bugs that only show up after extended runs.)
2. **Pull the radio's Ethernet cable** while connected. The app should detect the silence within 2 seconds, transition to an Error state, and try to reconnect up to 3 times at 5-second intervals. Plug the cable back in during that window — it should recover. If you wait longer than ~20 seconds, it should give up and stay in Error until you click Connect again.
3. **Let your radio be somewhere unusual on your network** — behind a VPN, on a different subnet accessible via static route, or on the other end of a LAN bridge. Broadcast discovery won't find it but the manual-add dialog (**Add Manually…** button in ConnectionPanel) lets you type in the IP directly. Worth confirming that works on your board.
4. **Change the sample rate while listening** to a signal. Does audio stay connected? Does the spectrum span update correctly?
5. **Try Add Manually with a wrong IP.** Does the app handle "radio doesn't respond" gracefully, or does it freeze?
6. **Cold / warm / hot swap radios.** If you have more than one supported board on your network, switch between them: connect to A, disconnect, connect to B. No crash, no stale state bleeding from the first radio.
7. **Exercise the capability gating.** Open Hardware Config on each board you have and confirm the right tabs appear/hide. If you see a tab that shouldn't be there (e.g. PureSignal on a Hermes-classic), that's a capability-registry bug.
8. **Adaptive auto-attenuation.** Switch the step attenuator to Adaptive mode (Setup → General → Options). Tune to a band with strong signals. Does the auto-att respond to ADC overload by increasing attenuation? Does it back off when the strong signal goes away?
9. **Per-band DSP persistence torture test.** Set up different DSP configurations on 3–4 bands (e.g. NR2 on 40m, APF on 20m, high squelch on 2m). Switch bands rapidly. Do all settings restore correctly every time?
10. **Clarity adaptive auto-tune.** On the spectrum overlay panel, check the Clarity status badge. Re-tune to a quiet band vs. a noisy band. The noise floor estimator should adapt. Try the "Reset to Smooth Defaults" button on Setup → Display → Spectrum Defaults.

---

## Reporting bugs

**Easiest way:** click the **💡 lightbulb** in the top-right corner of the menu bar. The built-in issue reporter walks you through filing a structured bug report or feature request, then opens the GitHub issue with the fields pre-filled. You can also file directly at <https://github.com/boydsoftprez/NereusSDR/issues/new/choose>.

When something goes wrong, the more of these you can include, the faster we can fix it:

### 1. What you were doing

One line describing what you clicked / typed right before the problem. Example: *"I clicked Connect on the ANAN-G2 row and about 0.5 s later the app crashed."*

### 2. Which version

From **Help → About NereusSDR** (shows version, build info, Qt/WDSP versions, and GPG fingerprint), or from the terminal:

```bash
cd NereusSDR
git rev-parse HEAD
```

If you're on a tagged release, give us the tag (e.g. `v0.1.1`).

### 3. Which radio

- Board family (HL2 / Hermes / Metis / Angelia / Orion / Orion MkII / ANAN-10/100/100B/100D/200D/7000/8000 / G2-Saturn / other)
- Protocol (P1 or P2 — NereusSDR tells you this in the Connection Panel)
- Firmware version (from Setup → Hardware Config → Radio Info once connected, or your radio's built-in web UI)

### 4. The app log

NereusSDR writes every log line to a file. Find it:
- **macOS:** `~/Library/Application Support/NereusSDR/nereussdr.log` (or under `~/Library/Preferences/NereusSDR/`)
- **Linux:** `~/.config/NereusSDR/nereussdr.log`
- **Windows:** `%LOCALAPPDATA%\NereusSDR\nereussdr.log`

The file is rewritten on each launch, so grab it *before* you relaunch the app after a crash.

### 5. Any crash report

If the OS popped a crash dialog, there's a full crash log somewhere:
- **macOS:** `~/Library/Logs/DiagnosticReports/NereusSDR-*.ips` (most recent one)
- **Linux:** usually in `/var/log/apport/` or as a core dump in your home dir, depending on distro
- **Windows:** Event Viewer → Windows Logs → Application, find the NereusSDR entry

Attach the whole file to the bug report. It's usually 30–100 KB of text.

### 6. File the report

Use the **💡 in-app reporter** (easiest), or open a new issue at <https://github.com/boydsoftprez/NereusSDR/issues/new/choose> and pick **Bug Report** or **Feature Request**. The structured templates ask for your radio model, OS, version, and steps to reproduce.

**What's useful:**
- Repro steps
- What you expected vs what happened
- Logs + crash reports if available
- Screenshots for visual bugs
- Your OS version and your radio's board + firmware

**What's not useful (we already know):**
- "TX doesn't work" — intentional, see above
- "Meter X doesn't move" — intentional, see above
- "UI element Y looks rough" — known, alpha stage

---

## Bottom line

If at the end of this test you:

1. Launched the app
2. Discovered your radio in ConnectionPanel
3. Connected to it without a crash
4. Heard a live SSB QSO with a working spectrum and waterfall
5. Toggled a few DSP features (NR, AGC, attenuator) and heard them work
6. Switched bands and had your settings come back
7. Quit cleanly

…then we've hit our **alpha-test success criterion**. Everything beyond that is a bonus.

**Thank you for testing.** We're trying to build a modern, open, cross-platform replacement for Thetis, and we can't do it without people running the code on real hardware and telling us what breaks. If you send us one usable bug report with logs attached, you've already contributed more than most people ever will to an open-source radio project.

JJ Boyd ~KG4VCF

🤖 Co-Authored with [Claude Code](https://claude.com/claude-code)
