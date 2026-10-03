# NereusSDR Alpha Test — OpenHPSDR Radios (P1 & P2)

**Hi, and thank you.** You're helping test an **alpha-stage** SDR console. This doc walks you through what to try, what "success" looks like, and — just as important — what does **not** work yet so you don't waste time filing bugs for features we know are missing.

**What this build is:**
- A ground-up C++/Qt6 port of [Thetis](https://github.com/ramdor/Thetis) (the Apache Labs / OpenHPSDR console), architecturally modelled on [AetherSDR](https://github.com/ten9876/AetherSDR).
- Supports the **full OpenHPSDR family** over **both Protocol 1 (P1)** and **Protocol 2 (P2)**. That includes the Apache Labs ANAN line (100/100B/100D/10/10E/200D/7000/8000/G2-Saturn), Hermes / Hermes Lite 2, Metis, Angelia, Orion, Orion MkII, and any other board that speaks the OpenHPSDR discovery + frame protocols.
- As of **Phase 3P (Apr 2026), the hardware / radio-plumbing / status-readout surfaces are userland-complete vs Thetis** — every Hardware Config page (Antenna-ALEX, OC Outputs, Calibration, HL2 I/O) and every Diagnostics dashboard a Thetis user reaches for has a NereusSDR equivalent, and PA / connection-quality / settings-validation readouts a Thetis user expects are exposed. **The DSP-parameter, Transmit, PA Settings, CAT/Network, Appearance, and Keyboard Setup pages are NOT there yet** — the page tree is built out, but most controls inside those pages are disabled placeholders pending later phases (3M TX, 3J TCI, 3K CAT, 3H skins, etc.). See "Setup pages that look built but are still shell" below for specifics. A previous revision of this doc overstated that as "every Setup page is wired"; that was wrong and this revision corrects it.
- The **goal of your test** is: confirm discovery → connect → listen to a live SSB QSO on your board, plus exercise the Phase 3P additions (Calibration, OC Outputs, Antenna Control, Alex filter live-LED, Radio Status dashboard, ADC Overload status-bar indicator).

**What this build is NOT:**
- Ready for transmit. **Do not key up.** The TX pipeline (Phase 3M) is intentionally cold — commands go on the wire but there is no SSB modulator yet.
- Feature-complete. Some controls you see in the UI are placeholder — more on that below in "What is NOT wired up".
- Signed with Apple Developer ID credentials. The v0.2.x alpha does not yet have paid Apple Developer credentials — the macOS app is **ad-hoc codesigned** (not Developer ID signed, not notarized), so first launch needs **right-click → Open → Open** to satisfy Gatekeeper. The Windows installer is **unsigned**, so first launch triggers a SmartScreen "Windows protected your PC" warning — click **More info → Run anyway**. See "macOS notes" below for the VAX HAL plugin situation on macOS (it's not shipped in the DMG; self-sign instructions are in that section).

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
| 3-UI | Full UI skeleton: 12 applets, 9 menus, 47-page SetupDialog, spectrum overlay panel, status bar | Working |
| 3F | Up to 4 independent panadapters in configurable layouts | **Not started** |
| 3G-1..7 | Dockable/floatable container system, GPU-rendered meter engine with 31 item types, container settings dialog, MMIO external-data subsystem | Working |
| 3G-8 | RX1 Display parity — every control on the Setup → Display pages wired to the renderer (47 controls) | Working |
| 3G-9 | Display refactor — Clarity Blue waterfall palette, ClarityController adaptive auto-tune, per-band Clarity memory, zoom persistence, Thetis-cited tooltips | Working |
| 3G-10 | RX DSP parity — 10 WDSP feature slices wired, per-slice-per-band persistence, VfoWidget 4-tab rewrite with S-meter | Working |
| 3G-11 | P1 field fixes — VFO frequency encoding as raw Hz | Working |
| 3G-13 | Step attenuator (Classic + Adaptive auto-att), ADC overflow detection, per-model preamp items | Working |
| 3G-14 | About dialog + built-in AI-assisted issue reporter (💡 menu bar widget) | Working |
| 3H | Thetis-inspired skin system | **Not started** |
| 3I | Protocol 1 support for the full ANAN/Hermes family incl. HL2 (discovery, connection, capability gating, saved radios, auto-reconnect) | Working |
| 3J | TCI protocol server (for N1MM+, Log4OM etc.) | Not started |
| 3K | CAT / rigctld bridge | Not started |
| ~~3L~~ | ~~HL2-specific I2C-over-ep2 encoding~~ | **Delivered via Phase 3P-E** — HL2 IoBoard I2C TLV queue + 12-step state machine + bandwidth monitor all working |
| 3M | TX pipeline — SSB, CW, full processing chain, PureSignal PA linearization | **Not started** |
| 3N | Release pipeline + /release skill + GPG-signed alpha binaries | Working — this is how you got the build |
| **3O** | **VAX audio routing** — `IAudioBus` abstraction with 5 platform backends (CoreAudio HAL plugin on macOS, PulseAudio pipes on Linux, PortAudio on Windows, plus fallbacks), **first-run VAX dialog** that auto-detects Windows virtual-cable families (VB-Audio / VAC / Voicemeeter / Dante / FlexRadio DAX), **MasterOutputWidget** in the menu bar (right-click → device picker, scroll-wheel fine-tune), **Setup → Audio** sub-tabs (Devices / VAX / TCI / Advanced), VfoWidget VAX channel selector, per-slice VAX channel persistence | Working |
| **3P-A** | **HL2 BPF filter switching now works on band/VFO change** (was: stuck at bank 10 C3=0 / C4=0 permanently). **HL2 step attenuator now actually attenuates** (was: ramdor's 5-bit mask misapplied; needed mi0bot's 6-bit mask + 0x40 enable + MOX TX/RX branch). Per-board codec subclasses. S-ATT slider widens to 0–63 dB on HL2. | Working |
| **3P-B** | **P2 wire-bytes parity** (OrionMKII family + Saturn with G8NJJ BPF1 override). New **Hardware → Antenna/ALEX → Alex-1 Filters + Alex-2 Filters** sub-sub-tabs with live LED column that lights the active HPF/LPF row for the current VFO. **ADC OVL split** into OVL₀ + OVL₁ on dual-ADC boards. **RX1 preamp toggle** on OrionMKII family. | Working |
| **3P-C** | **Preamp combo now populates per-board** from `BoardCapabilities::preampItemsForBoard()`. HL2 preamp corrected from 1-item to 4-item (0 / −10 / −20 / −30 dB). | Working |
| **3P-D** | New **Hardware → OC Outputs** Setup page — 7-pin per-band RX matrix (14×7), TX matrix (14×7), TX pin action grid, USB BCD, external PA control. Live pin-state LED row reflects the last OC byte sent. | Working |
| **3P-E** | New **Hardware → HL2 I/O** page (replaces the Phase 3I placeholder). Register state table polls `IoBoardHl2` at 40 ms. I2C transaction log. Live bandwidth monitor (EP6 ingress + EP2 egress + LAN-PHY throttle detect). Closes long-deferred Phase 3I-T12 work. | Working |
| **3P-F** | New **Hardware → Antenna/ALEX → Antenna Control** sub-sub-tab — 14 bands × TX/RX1/RX-only antenna grid + Block-TX safety toggles. `AlexController` / `ApolloController` / `PennyLaneController` accessory models. **RxApplet antenna buttons** auto-populate per-band from AlexController. | Working |
| **3P-G** | **Hardware → Calibration** page (renamed from PA Calibration). 5 Thetis-1:1 group boxes: Freq Cal, Level Cal (with Rx1/Rx2 6m LNA offsets), HPSDR Freq Cal Diagnostic (9-decimal correction factor + 10 MHz external-ref toggle), TX Display Cal, existing PA Current (A) group. Freq correction factor wires into P2 phase-word so per-radio reference-oscillator drift is compensable. | Working |
| **3P-H** | New **Diagnostics → Radio Status dashboard** (5 cards: PA Status, Forward/Reflected/SWR, PTT Source, Connection Quality, Settings Hygiene) + 4 sibling sub-tabs (Connection Quality / Settings Validation / Export-Import Config / Logs). **PA telemetry parsed from both P1 and P2** status packets with Thetis per-board scaling formulas. **ADC Overload status-bar label** left of STATION (yellow/red per hysteresis level, 2 s auto-hide, Thetis `ucInfoBar` parity). **Live LED wire-up** across Alex-1/2 Filters, OC Outputs pin-state, HL2 I/O register table. **Settings Hygiene** validates per-MAC AppSettings against BoardCapabilities on connect. Dark-theme checkbox + radio-button fix. Plus a discovery-driven **attribution enforcement pipeline** (corpus + preservation check + drift gate) that closed 74 historical dropped contributor tags. | Working |
| **CLI: `--profile <name>`** | Run multiple concurrent NereusSDR instances with separate AppSettings / audio / container state. Each instance has its own profile-scoped config. | Working |
| **Post-v0.2.1 maintenance (Apr 16–21)** | `fix(vfo)` full frequency-entry parser rewrite (closes [#73](https://github.com/boydsoftprez/NereusSDR/issues/73)). `fix(rx-applet)` STEP ↑/↓ arrows now step the tuning ladder (closes [#69](https://github.com/boydsoftprez/NereusSDR/issues/69)) and 500 Hz was added between 100 Hz and 1 kHz. `fix(receiver-manager)` ReceiverManager reset on disconnect so reconnecting the same rig doesn't leak receivers (closes [#75](https://github.com/boydsoftprez/NereusSDR/issues/75)). `fix(shutdown)` HL2 disconnect runs synchronously on the connection worker with a QSemaphore-bounded dispatch — fixes Winsock corruption on Windows HL2 shutdown (closes [#83](https://github.com/boydsoftprez/NereusSDR/issues/83)). `fix(p1)` HL2 I2C read responses now persist into the `IoBoardHl2` model so the HL2 I/O page stays in sync. `fix(codec)` `FreqCorrectionFactor` is now applied inside `P2CodecOrionMkII::hzToPhaseWord` — the Calibration page dial actually shifts the radio now (previously the UI changed but the phase word didn't). `fix(oc-matrix)` OcMatrix state guarded with QReadWriteLock for cross-thread safety. `fix(rx-applet)` RX1 preamp toggle queued onto the connection thread so the combo can't race the codec. `fix(p1)` bank scheduler ceiling now reads `codec->maxBank()` so HL2 and Anvelina Pro 3 stop clipping at bank 10. | Working |

**The short version:** after Phase 3P, **NereusSDR's hardware / radio-plumbing / status-readout surface is userland-complete vs Thetis.** Every Hardware Config + Diagnostics page a Thetis user reaches for has a NereusSDR equivalent; every PA / connection / settings-validation readout a Thetis user expects is exposed. **Much of the rest of the Setup tree is NOT there yet.** The DSP-parameter pages (beyond RX AGC), the Transmit section, PA Settings, CAT/Network, Appearance, Keyboard, and parts of Diagnostics and Display are built-out page shells with **disabled placeholder controls** — they're the scaffolding for later phases (3M, 3J, 3K, 3H). Single-receiver RX works end-to-end. **TX pipeline (Phase 3M) is still cold**; multi-panadapter (3F), TCI (3J), CAT (3K), skins (3H), recording (3M) remain not-started.

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

This is the **total** functional surface area as of today, on any supported radio:

**RX audio chain**
1. **Discover + connect + disconnect** any P1 or P2 OpenHPSDR radio on your LAN
2. **Tune RX1** anywhere in the HF range (P1 radios to 61.44 MHz; P2 boards like Saturn have broader range)
3. **Hear live audio** demodulated through WDSP in USB / LSB / AM / SAM / FM / CW with adjustable AGC (basic + advanced: threshold/hang/slope/attack/decay), noise blanker (NB1 or NB2 with advanced params), EMNR (NR2), SNB, APF, squelch (SSB/AM/FM 3-variant), bandpass filter, notch, mute, audio pan, binaural, and volume
4. **Per-slice-per-band DSP persistence** — all DSP settings saved per band, restored automatically on band change

**Display**
5. **Real-time spectrum** (GPU-accelerated, ~30 FPS, configurable averaging, peak hold, colours, grids, labels, FPS overlay)
6. **Real-time waterfall** (GPU-accelerated, Clarity Blue palette with adaptive auto-tune, configurable colour scheme, AGC, opacity, reverse scroll, timestamps, overlays)
7. **Click-to-tune** on the spectrum and waterfall, scroll-to-zoom, drag-to-adjust-reference-level, drag-filter-passband
8. **CTUN mode** — independent panadapter center and VFO, WDSP frequency shift. Zoom + centre persisted across restarts.

**Controls**
9. **Band switching** via the band button grid (14 bands: 160 / 80 / 60 / 40 / 30 / 20 / 17 / 15 / 12 / 10 / 6 m + GEN + WWV + XVTR), with per-band display settings and per-band Clarity memory
10. **Mode switching** via the mode button grid (USB/LSB/AM/SAM/FM/CW/CWL/CWU/DIGL/DIGU/DRM/SPEC) with mode containers (FM OPT / DIG / RTTY)
11. **Filter presets** via the filter button grid
12. **VFO display** with per-digit mouse-wheel tuning, **RIT/XIT client offset**, and **frequency lock**
13. **Signal S-meter** (arc needle + VfoWidget level bar with dBm readout, cyan→green gradient at S9, live from WDSP)
14. **Step attenuator** with Classic + Adaptive auto-attenuation, hysteresis, per-MAC persistence. **ADC overflow detection** — now surfaced as a flashing **ADC Overload** label in the status bar left of STATION (yellow/red per hysteresis level, 2 s Thetis-parity auto-hide). **Preamp** controls capability-gated per board (HL2: 4-item 0/−10/−20/−30 dB set; ANAN boards: stock ranges with per-model preamp items from Thetis).

**New in Phase 3P — all Setup pages a Thetis user reaches for**
15. **Hardware → Antenna / ALEX** with three sub-sub-tabs: **Antenna Control** (14-band × TX/RX1/RX-only antenna grid + Block-TX safety), **Alex-1 Filters** (HPF + LPF edges, Saturn BPF1 panel on G2/G2-1K, live LED column that tracks the active filter for the current VFO), **Alex-2 Filters** (same with live LED). Alex-1 actually drives the relays on the rig — tune across an HPF boundary and you'll hear the click; Alex-2 is UI-only today (codec-layer wire-up deferred).
16. **Hardware → OC Outputs** — master toggles (Penny ExtCtrl, N2ADR Filter, hot-switching, Reset), per-band RX matrix (14 × 7), per-band TX matrix (14 × 7), TX pin-action grid, USB BCD, external PA control, **live pin-state LED row** that reflects the last OC byte sent and flips on MOX.
17. **Hardware → Calibration** — Freq Cal, Level Cal (with Rx1/Rx2 6m LNA offsets), **HPSDR Freq Cal Diagnostic** (9-decimal correction factor that's wired into the P2 phase-word so per-radio reference-oscillator drift is now compensable), TX Display Cal, existing PA Current (A) group.
18. **Hardware → HL2 I/O** (HL2-only) — register state table polled at 40 ms, I2C transaction log, live bandwidth monitor (EP6 ingress + EP2 egress + LAN-PHY throttle detect). On HL2, N2ADR Filter enable, 12-step UpdateIOBoard state machine visualizer.
19. **RxApplet antenna buttons** populate per-band from `AlexController` and auto-switch on band change.

**New in Phase 3P-H — Diagnostics dashboard**
20. **Diagnostics → Radio Status** — single consolidated dashboard with 5 cards: PA Status (temp/current/voltage bar meters), Forward/Reflected/SWR (bar meters + last-TX peaks), PTT Source (pill row for MOX/VOX/CAT/Mic/CW/Tune/2-Tone + 8-event PTT history), Connection Quality summary, Settings Hygiene actions.
21. **Diagnostics → Connection Quality** — live EP6/EP2 byte rates, sequence gaps, LAN-PHY throttle counter.
22. **Diagnostics → Settings Validation** — per-MAC AppSettings validated against BoardCapabilities on every connect; full audit list with Reset / Forget / Re-validate actions. Auto-refreshes on change.
23. **Diagnostics → Export / Import** — round-trips the full AppSettings XML via Export / Import Config buttons.
24. **Diagnostics → Logs** — placeholder pending qCWarning capture wire-up.

**Phase 3O — VAX audio routing**
25. **First-run VAX dialog** — platform-specific auto-detect. On Windows, scans for VB-Audio / VAC / Voicemeeter / Dante / FlexRadio DAX cable families and pre-fills channel bindings. On macOS, the CoreAudio HAL plugin adds 4 native VAX output devices + 1 TX input. On Linux, loads `module-pipe-source × 4` + `module-pipe-sink × 1` under a `nereussdr-vax-*` namespace.
26. **MasterOutputWidget in the menu bar** — global volume, mute, scroll-wheel fine-tune, right-click device picker. One source of truth for output routing.
27. **Setup → Audio** sub-tabs — Devices (per-device driver API / buffer / format), VAX (4 channel strips with meter + gain + mute + device picker + auto-detect), TCI (placeholder for Phase 3J), Advanced (reset all audio settings, IVAC feedback parity controls).
28. **VFO flag VAX channel selector** — per-slice VAX assignment persisted.

**Storage + multi-instance**
29. **Saved radio profiles** keyed by MAC — discover once, re-use next launch with auto-reconnect
30. **Manually-added radios** outside the local subnet via the Add Manually dialog
31. **`--profile <name>` command-line flag** — run multiple concurrent NereusSDR instances, each with its own AppSettings / audio / container state (useful for testing against two radios in parallel, or keeping a pristine profile alongside your everyday config).

**Layout + meters**
32. **Full container system** with drag-to-dock, float, pin-on-top, axis-lock, layout persistence, per-container settings
33. **31 meter item types** styled and placed, 38+ meter presets in the library
34. **SetupDialog** — Display section fully wired (47 controls), Hardware section all sub-tabs wired, General → Options wired, Audio section wired, Diagnostics section wired, Calibration wired. Other sections (CAT/Network, DSP advanced, Transmit, Appearance, Keyboard) mostly stubbed pending later phases.
35. **About dialog** (Help → About NereusSDR) — version, Qt/WDSP versions, heritage credits, license, GPG fingerprint
36. **Built-in issue reporter** — click the 💡 in the menu bar to file bugs / feature requests directly to GitHub with structured templates

**Single-radio, single-receiver, receive-only.** TX pipeline (Phase 3M) is still cold; multi-panadapter (3F), TCI (3J), CAT (3K), skins (3H), recording (3M-rec) remain not-started.

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

You need Qt6 (Core, Widgets, Network, Multimedia, Test), CMake 3.20+, and FFTW3. On macOS: `brew install qt@6 cmake ninja fftw`. On Ubuntu: `apt install qt6-base-dev qt6-base-private-dev qt6-multimedia-dev qt6-shadertools-dev libfftw3-dev cmake ninja-build`. On Windows see `docs/build/windows.md` if it exists, otherwise the Qt online installer + MinGW/MSVC both work.

Launch:
- **macOS:** `open build-clean/NereusSDR.app`
- **Linux:** `./build-clean/NereusSDR`
- **Windows:** `build-clean/NereusSDR.exe`

### macOS notes — code signing and the VAX HAL plugin

The v0.2.x alpha line does not yet ship with paid Apple Developer ID credentials. That has two practical consequences on macOS:

**1. Gatekeeper first-launch warning (main app).** The `.app` inside the DMG is **ad-hoc codesigned** — it has a signature, but not one tied to an Apple-issued Developer ID and not notarized. On first launch you'll see:

> "NereusSDR" cannot be opened because the developer cannot be verified.

Dismiss it, then:

```text
Right-click NereusSDR.app → Open → Open (in the second dialog)
```

macOS remembers that choice; subsequent launches open normally. If that flow still fails (happens on some macOS 14.x configurations), clear the quarantine attribute manually:

```bash
xattr -dr com.apple.quarantine /Applications/NereusSDR.app
```

**2. VAX HAL plugin is not in the DMG.** NereusSDR's **VAX audio routing** on macOS is delivered as a CoreAudio **HAL plugin** (`NereusSDRVAX.driver`) that installs into `/Library/Audio/Plug-Ins/HAL/`. The macOS CoreAudio daemon (`coreaudiod`) is hardened and will not load a HAL plugin unless the system trusts its signature. Without Developer ID credentials the release pipeline can't produce a redistributable installer that `coreaudiod` will load on someone else's Mac, so **v0.2.2 does not attach the HAL plugin `.pkg` to the GitHub Release**. You can still:

- **Use the app without VAX** — the main-output speaker path works fine over any normal CoreAudio device. You lose the per-slice VAX channel routing and the MasterOutputWidget's VAX destinations, but RX, spectrum, audio, CAT (once wired), and everything else continues to work.
- **Self-sign and install the HAL plugin locally** if you want VAX routing today. An ad-hoc signature made on your own machine is trusted by `coreaudiod` on *that* machine.

**Self-sign the HAL plugin (macOS, 5 minutes):**

```bash
# From the NereusSDR repo root, after a source build:
cmake -B build-hal -S hal-plugin -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-hal -j

# Ad-hoc sign the driver bundle so coreaudiod will load it locally:
codesign --force --deep --sign - build-hal/NereusSDRVAX.driver
codesign --verify --verbose=2 build-hal/NereusSDRVAX.driver

# Install it (HAL plugins live in /Library/Audio/Plug-Ins/HAL):
sudo cp -R build-hal/NereusSDRVAX.driver /Library/Audio/Plug-Ins/HAL/

# Restart coreaudiod so it picks up the new plugin:
sudo killall coreaudiod
```

Launch NereusSDR and open **Setup → Audio → Devices**. You should now see four `NereusSDR VAX` output devices and one `NereusSDR VAX TX` input device in the device list. The first-run VAX dialog will offer to bind them to channels automatically.

**To uninstall:**

```bash
sudo rm -rf /Library/Audio/Plug-Ins/HAL/NereusSDRVAX.driver
sudo killall coreaudiod
```

> Why ad-hoc works locally but not for distribution: `codesign --sign -` writes a self-contained signature that includes the plugin's designated requirement but has no external certificate chain. `coreaudiod` on the machine that signed it accepts that signature; `coreaudiod` on any other Mac rejects it because nothing vouches for the identity. A future alpha, once Apple Developer credentials are in place, will ship a `.pkg` installer with a Developer-ID-signed + notarized driver that any Mac accepts.

If you hit issues self-signing — the driver is new (Phase 3O Sub-Phase 5) and the alpha test is the first external trial of it — please file a bug via the 💡 menu with your macOS version, the output of `codesign --verify --verbose=2 build-hal/NereusSDRVAX.driver`, and the `log show --predicate 'process == "coreaudiod"' --last 5m` tail.

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

Open **Setup → Hardware Config**. You should see a top-level tab strip with **up to 9 tabs**, gated on your board's capabilities — NereusSDR inspects the board's feature bits and hides tabs that don't apply.

Reference table:

| Tab | Radios that show it |
|---|---|
| **Radio Info** | All radios |
| **HL2 I/O** | Hermes Lite 2 only |
| **Antenna / ALEX** | All boards with ALEX hardware (Hermes / ANAN / Orion / Saturn). Contains three sub-sub-tabs: **Antenna Control**, **Alex-1 Filters**, **Alex-2 Filters**. The Saturn BPF1 panel inside Alex-1 Filters is visible only on ANAN-G2 / G2-1K. |
| **OC Outputs** | Boards with Open Collector outputs |
| **XVTR** | Boards supporting transverter offsets |
| **PureSignal** | 2-ADC boards with feedback path (ANAN-100D / 200D / 7000 / 8000 / G2 / Orion MkII) |
| **Diversity** | 2-ADC boards supporting diversity reception |
| **Calibration** | Boards with PA power-sensing hardware (renamed from "PA Calibration" in Phase 3P-G) |

For example:
- An **HL2** sees Radio Info + HL2 I/O + Antenna/ALEX (Alex-1 tab is mostly irrelevant on HL2 — board has no Alex hardware; the tab still renders).
- An **ANAN-100** sees Radio Info + Antenna/ALEX + OC Outputs + XVTR + Calibration (no PureSignal, no Diversity).
- An **ANAN-G2 / 7000 / 8000** sees all the ANAN tabs, including the Saturn BPF1 panel on G2 / G2-1K.

**✅ Success bar for step 6:** Radio Info populates with your actual board info (correct board name, ADC count, max RX, firmware, MAC, IP). The tabs you see match the capability table above for your board. The sub-sub-tabs inside Antenna/ALEX render (Antenna Control / Alex-1 Filters / Alex-2 Filters), and on G2/G2-1K the Saturn BPF1 panel is visible inside Alex-1 Filters.

### 7. Alex filter LEDs (new in Phase 3P-B + 3P-H)

Only if your board has Alex hardware:
1. Tune your VFO from 20 m (14 MHz) up through 22 MHz while watching **Setup → Hardware → Antenna/ALEX → Alex-1 Filters**.
2. Cross the HPF boundaries: **6.5 / 9.5 / 13 / 22 / 50 / 55 MHz.**
3. At each boundary: the **green LED column** on the left of the HPF table should jump to the matched row, AND you should hear a **relay click** on the rig.
4. Same for the LPF column: crossing each band's LPF boundary should light the correct LPF row.

**✅ Success bar for step 7:** Alex-1 LED flips track the VFO, relay clicks occur at the same boundaries. Alex-2 LEDs flip visually too, but no relay click (Alex-2 codec wire-up is a deferred follow-up — the LED is a preview of what we *would* send).

### 8. OC Outputs live pin state (new in Phase 3P-D + 3P-H)

Only if your board has Open Collector outputs:
1. Open **Setup → Hardware → OC Outputs → HF**.
2. Enable a pin on the TX row for 20 m.
3. Close Settings. Key MOX (or Tune) briefly. The **pin-state LED row** in the OC Outputs page should flip on key-down and flip back on unkey.
4. Change band on the main VFO — the LED row should reflect the new band's matrix row.

### 9. Calibration freq correction factor (new in Phase 3P-G)

Only if your board is G2 or similar with a tunable reference:
1. Tune to **10.000 MHz WWV**. Listen to the carrier zero-beat.
2. Open **Setup → Hardware → Calibration → HPSDR Freq Cal Diagnostic**.
3. Change the **Freq Correction Factor** from `1.000000000` to `1.000000500`. The on-air frequency should shift by ~5 Hz (roughly half a cycle on a WWV carrier).
4. **Reset the factor to 1.0 before moving on.** Otherwise your radio will run slightly off frequency on all subsequent sessions.

### 10. Radio Status dashboard (new in Phase 3P-H)

1. Open **Setup → Diagnostics → Radio Status**.
2. On idle (no TX): PA Temperature and PA Current should show 0, Forward/Reflected/SWR all at zero. Status bar (top strip) should show your radio name + uptime + firmware.
3. Open **Setup → Diagnostics → Settings Validation**. First connect should show `✓ No issues — all settings within board capability ranges.`
4. Open **Setup → Diagnostics → Connection Quality**. EP6 and EP2 byte rates should be non-zero with a live RX feed. Throttle = `ok`. Sequence gaps = 0 on a clean network.

### 11. ADC Overload status-bar indicator (new in Phase 3P-H)

1. Set 0 dB attenuation. Tune to a busy band.
2. Connect a strong antenna, or inject a hot signal from an RF generator. Overdrive until the ADC clips.
3. The **ADC Overload** label left of STATION should flash yellow, then red if sustained. Text reads "ADC0 Overload" (single-ADC boards) or "ADC0 Overload   ADC1 Overload" (dual-ADC, if both trip).
4. Remove the strong signal. The label should auto-hide ~2 seconds after the last clip event.

### 12. Quit cleanly

Close the app (red close button, or File → Quit, or ⌘Q on macOS, Alt+F4 on Windows).

**✅ Success bar for step 12:** the app closes without a crash dialog and without leaving a new crash report in your system's diagnostic reports folder (see "Reporting bugs" below for where to find it on each OS).

### 13. Relaunch + persistence

Open the app again. If you had your radio connected when you quit, it should **silently auto-reconnect** within a few seconds without you needing to open ConnectionPanel.

Then check that your persisted settings came back:
- Your last-tuned frequency and mode on each band
- Alex-1 edge customizations (if you changed any)
- OC Outputs matrix pins
- Antenna Control per-band antenna mappings
- Window splitter + dock layout + container contents

**✅ Success bar for step 13:** auto-reconnect works (or silently gives up without popping an error), and all persisted settings restore correctly.

### 14. Multi-instance via `--profile` (optional, Phase 3O)

If you have two radios on your network and want to run them concurrently:
```bash
# First instance, default profile
./NereusSDR &

# Second instance, separate profile
./NereusSDR --profile testrig &
```
Each instance gets its own AppSettings file, audio routing, and container layout. `--profile` accepts any simple name (`testrig`, `bench`, `portable`). Logs go to a profile-scoped path (see "Reporting bugs" below).

---

## What is NOT wired up (don't file bugs for these)

This is the list of things you will see in the UI that look like they should work but are intentionally stubbed for this phase. Please **do not file bugs** against these — they're tracked in the roadmap. Later phases will light them up.

### Transmit, period

- **No TX audio.** The SSB modulator doesn't exist yet. Keying up (MOX button, PTT input, Tune button) will put command bytes on the wire asking the radio to transmit, but there is no I/Q audio being generated to transmit with. **Don't test this. Alpha = RX only.**
- **No CW transmit.** Same reason.
- **No PureSignal linearization.** Scaffold only; no DSP behind the switch.
- **No external PTT input routing** — the ANAN OC Outputs tab has combos for PTT pin, CW key pin, and aux outputs; they persist the mapping but don't drive real GPIO until the TX pipeline lands (Phase 3M).

### Setup pages that look built but are still shell

A previous revision of this doc claimed "every Setup page a Thetis user reaches for has a NereusSDR equivalent." That was an overstatement. The page tree is built out, but **many pages have their controls disabled pending later phases**. Here's what's actually stub in the Setup dialog today, so you don't file bugs for disabled controls:

- **Setup → DSP** — the only live controls in the whole DSP section are the **AGC** controls on the AGC/ALC page (Mode / Attack / Decay / Hang / Slope / Max Gain / Fixed Gain / Hang Threshold). **Everything else on the DSP section is a disabled placeholder:** ALC (in-band / out-of-band / max gain), Leveler, NR1 (Taps/Delay/Gain/Leak), ANF, NR2 / EMNR (gain/NPE/position/trained/artifact), NB1, NB2, SNB, CW Keyer, CW Timing, APF, AM, SAM, FM squelch / RX / TX sub-groups, VOX / DEXP, CFC, TX Profiles, MNF. **Note:** the ON/OFF *toggles* for most of these features are live on the VfoWidget DSP tab and the RX applet — it's only the tunable-parameter Setup UI that's still cold.
- **Setup → Transmit** (all 4 pages: Power & PA, TX Profiles, Speech Processor, PureSignal) — essentially entirely stubbed pending Phase 3M. Max-power slider, SWR protection, compressor, CESSB, per-band gain, profile list / create / delete / copy — all disabled.
- **Setup → CAT & Network** (Serial Ports, TCI Server, TCP/IP CAT, MIDI Control) — all 4 pages are stubbed pending Phases 3J (TCI) and 3K (CAT / rigctld). You can navigate into them, nothing drives anything.
- **Setup → Keyboard → Shortcuts** — partial page; the keyboard-shortcut binder is not live yet.
- **Setup → Appearance** (Colors & Theme, Meter Styles, Gradients, Skins, Collapsible Display) — all 5 pages are stubbed pending Phase 3H (skin system).
- **Setup → Diagnostics → Signal Generator / Hardware Tests / Logging** — stubs alongside the four live Diagnostics sub-tabs (Radio Status / Connection Quality / Settings Validation / Export-Import). **Logs** sub-tab is a placeholder pending qCWarning capture wire-up.
- **Setup → Display → RX2 Display / TX Display** — stubs pending Phase 3F (multi-panadapter) / 3M (TX pipeline). The three live Display pages are Spectrum Defaults / Waterfall Defaults / Grid & Scales.
- **Setup → Hardware → PureSignal / Diversity** sub-tabs — scaffold only; state persists but no DSP or RX-fusion runs behind them.

**What you CAN rely on in the Setup dialog right now:**
- General → Startup & Preferences / UI Scale & Theme / Navigation / Options
- Hardware Config → Radio Info / Antenna-ALEX (Antenna Control + Alex-1 Filters + Alex-2 Filters) / OC Outputs / Calibration / HL2 I/O
- Audio → Devices / VAX / TCI (placeholder label) / Advanced
- DSP → AGC controls on the AGC/ALC page (the *only* live DSP Setup controls)
- Display → Spectrum Defaults / Waterfall Defaults / Grid & Scales
- Diagnostics → Radio Status / Connection Quality / Settings Validation / Export-Import

Everything outside that list in the Setup dialog is a disabled placeholder. Please don't file a "control X doesn't do anything" bug against those pages — we know.

### Controls that look live but are cold

- **PureSignal enable** (visible on 2-ADC boards) — state persists, no DSP runs.
- **Diversity enable** (visible on 2-ADC boards) — state persists, no second-RX fusion runs.
- **Alex-2 Filters relay click** — the LED column on Alex-2 Filters tracks the VFO correctly, but the codec wire-up to actually send the RX2 filter selection to the radio is deferred. You'll see the LED flip; you won't hear a relay click. Alex-1 filters DO drive the rig relays.
- **Saturn BPF1 codec path** — the Saturn BPF1 Bands panel on Alex-1 Filters persists your edge edits, but the codec currently ignores them and uses the standard Alex HPF bits. Deferred codec hookup per Phase 3P-H PR body.
- **Some Calibration group boxes** — Freq Cal and Level Cal don't yet run a live calibration routine (TX is needed for Level Cal; Freq Cal is receive-only but not yet wired to the panadapter-aware routine). HPSDR Freq Cal Diagnostic's correction factor IS live and does shift the radio.
- **Radio Status → PA Voltage** — parsed from status packet but intentionally not surfaced as a separate display field (Thetis doesn't surface it either; voltage is folded into forward-power scaling). Shows as blank.
- **Diagnostics → Logs** — placeholder panel; real qCWarning/qCDebug capture is a Phase 3P-H follow-up.

### UI features not yet in this build

- **TCI protocol server** (for N1MM+, Log4OM, etc.) — Phase 3J.
- **CAT / rigctld** (for logging programs) — Phase 3K.
- **Up to 4 independent panadapters in configurable layouts** — Phase 3F. You get RX1 only in the spectrum panel for now.
- **Second receiver (RX2)** — hardware support exists on 2-ADC boards (ANAN-7000/8000/G2/Orion MkII) but there's no UI for it yet.
- **Skin import from Thetis** — Phase 3H.
- **Firmware flashing from within the app** — not planned for this phase.
- **Multi-radio simultaneous connection** within a single process — not planned. Use `--profile` (Phase 3O) to run two app instances against two different radios in parallel.

### Things that might look weird but aren't bugs

- **Meter items that aren't wired up.** Many meter widgets render their background + zero-valued needle but don't update. Live on any supported radio: signal S-meter, spectrum/waterfall, clocks, PA telemetry in the Radio Status dashboard during TX (note: TX is cold, so this only shows live if a future TX pipeline lands). Most TX-side meters (ALC, compression, SWR), most PA-related meters in older containers, magic eye, data out — none of these update yet.
- **NYI tooltips** on some setup-dialog pages. Those pages haven't been ported from Thetis yet.
- **Right-click context menus** are minimal in some places.
- **Keyboard shortcuts** are partial.
- **Discovery is one-shot, user-triggered**, not continuous background rescanning.

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

If you launched with `--profile <name>` (Phase 3O multi-instance), the log lives under a profile-scoped path (note the literal `profiles/` segment the runtime inserts via `AppSettings::resolveConfigDir()`):
- **macOS:** `~/Library/Preferences/NereusSDR/profiles/<name>/nereussdr.log`
- **Linux:** `~/.config/NereusSDR/profiles/<name>/nereussdr.log`
- **Windows:** `%LOCALAPPDATA%\NereusSDR\profiles\<name>\nereussdr.log`

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
7. **HL2 owners:** confirmed BPF relay clicks on band change + S-ATT slider 0–63 dB range
8. **ALEX owners:** confirmed Alex-1 filter LED + relay click sync on VFO boundary crossings
9. Opened the new Phase 3P Setup pages (Calibration / OC Outputs / Antenna Control) without a crash
10. Opened the new Phase 3P-H Diagnostics tabs (Radio Status / Connection Quality / Settings Validation)
11. Quit cleanly

…then we've hit the **alpha-test success criterion for NereusSDR's Phase-3P hardware-and-status milestone.** (Not "userland-complete" — the DSP / Transmit / CAT / Appearance / Keyboard Setup pages are still shells. "Hardware surface + status readouts" is the honest scope of this test.) Everything beyond that is a bonus — and we especially want to hear about:

- Capability-gating misses (tabs that shouldn't be there on your board; tabs that should be there but are hidden)
- Settings that don't persist across a `Quit → Relaunch → Connect` cycle
- ADC overload trigger behavior that feels off (flashes red instantly, never flashes even with hot signal, stays stuck lit, etc.)
- Anything unusual about the `--profile` multi-instance mode if you try it

**Thank you for testing.** We're building a modern, open, cross-platform replacement for Thetis, and we can't do it without people running the code on real hardware and telling us what breaks. If you send us one usable bug report with logs attached, you've already contributed more than most people ever will to an open-source radio project.

J.J. Boyd ~ KG4VCF
