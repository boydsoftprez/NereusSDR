# Changelog

## [Unreleased]

### Fixed
- **HL2 TX UI parity with mi0bot-Thetis (#175)**: the RF Power and Tune Power sliders, the per-band Tune Power grid in Setup, and the underlying TX power/tune DSP path now follow mi0bot's HL2 attenuator semantics rather than the canonical 0-100 W scale. On HL2: RF Power slider 0-90 step 6 (16-step output attenuator, 0.5 dB/step); Tune Power slider 0-99 step 3 (33 sub-steps; 0-51 = DSP audio gain modulation, 52-99 = PA attenuator territory); Fixed-mode Tune Power spinbox -16.5 to 0 dB in 0.5 dB increments. Setup → Transmit → Power gains a new "Tune" group (`grpPATune` port) with 3 drive-source radios + TX TUN Meter combo + Fixed-mode spinbox, sitting above the existing per-band grid. TX volume on the wire matches mi0bot exactly via the new `setPowerUsingTargetDbm` HL2 sub-step DSP modulation and `computeAudioVolume` HL2 audio-volume formula `(hl2Power * gbb / 100) / 93.75` (`mi0bot-Thetis console.cs:47660-47673 + 47775-47778 [v2.10.3.13-beta2]`). Non-HL2 SKUs unchanged.
- **Tooltip wording** on RF Power and Tune Power sliders rewritten across every SKU to match Thetis upstream: "Transmit Drive — relative value, not watts unless the PA profile is configured with MAX watts @ 100%". Replaces the previous "RF output power (0-100 W)" wording which was misleading on every SKU, not just HL2.

### Migration
HL2 users upgrading: existing per-band Tune Power values reinterpret as int 0-99 (mi0bot encoding) → dB at the UI boundary. No migration step required (matches mi0bot's polymorphic-key pattern).

## [0.3.2] - 2026-05-03 - PA Calibration Safety Hotfix (#167)

> [!CAUTION]
> **Safety hotfix.** v0.3.1 could drive an ANAN-8000DLE PA past its rated 200 W ceiling at low TUNE-slider positions. K2GX measured >300 W on 80 m TUNE at slider 50 %. Root cause: the drive byte applied no per-band PA gain compensation, so high-gain PAs (8000DLE 80 m PA = 50.5 dB) reached full output well below slider 100 %. v0.3.2 ports Thetis SetPowerUsingTargetDBM faithfully and applies the per-band PA gain table at the audio output level, exactly as Thetis has done for years. Pre-hotfix wire byte 127 at slider 50 % / 80 m / 8000DLE now reads ~49.

> [!IMPORTANT]
> **Existing users — no action required.** Your saved radios, mic profiles, DSP settings, PA forward-power calibration (Watt Meter cal-points), and per-band tune power carry forward byte-identical. The new per-MAC PA Gain profiles seed automatically on first connect (16 factory rows + Bypass) and pick `Default - <connected model>` as the active profile. HL2 cannot regress on HF transmit — a sentinel deviation (gbb >= 99.5 -> linear fallback) preserves the v0.3.1 linear behaviour on bands where the factory PA gain row is 100.0 (HL2's "no compensation" marker on HF).

### Safety
- **Per-band PA gain compensation now applied** at the audio output level, not the wire-byte path. Faithful port of Thetis SetPowerUsingTargetDBM (console.cs:46645-46762 [v2.10.3.13]). Per-band gain table from clsHardwareSpecific.cs:459-751 (and mi0bot HL2 row at v2.10.3.13-beta2). The math: `target_dBm = 10*log10(slider_W*1000) - gbb; audio_volume = min(sqrt(10^(target_dBm/10) * 0.05) / 0.8, 1.0)`. Resolves K2GX (ANAN-8000DLE, P2) field report of >300 W output on a 200 W radio at low TUNE slider positions.
- **Wire-byte vs IQ-scalar topology corrected** to MW0LGE-canonical (audio.cs:268 + cmaster.cs:1117): SWR foldback now applies to the IQ scalar only, NOT to the wire byte. Pre-hotfix code applied SWR to the wire byte (citing mi0bot NetworkIO.cs:209-211 [v2.10.3.14-beta1]); reverted to MW0LGE topology with inline citation at both call sites.

### Setup -> PA full parity surface
- **PA Gain page** (Setup → PA → PA Gain): ported from Thetis tpGainByBand (setup.designer.cs:47386-47525 [v2.10.3.13]). 14-band gain spinbox grid, 14×9 drive-step adjust matrix, per-band max-power column, profile combo with New / Copy / Delete / Reset Defaults buttons, warning icon + label, chkPANewCal toggle, chkAutoPACalibrate with auto-cal sweep state machine (HF-only band loop, per-step settle via QTimer, FWD reading via RadioStatus, gain delta written into the active profile).
- **Watt Meter page**: Thetis tpWattMeter parity. Existing PaCalibrationGroup + new chkPAValues "Show PA Values page" toggle + Reset PA Values button.
- **PA Values page** (NereusSDR-spin promotion): closed the 4-label gap from v0.3.1 (Raw FWD power, Drive, FWD voltage, REV voltage) via the new public PaTelemetryScaling helpers (lifted from RadioModel.cpp private helpers). Running peak/min tracking on six telemetry values + in-page Reset button.
- **Per-SKU visibility**: `BoardCapabilities`-driven. PA category hidden on RX-only SKUs; editor controls disabled with "no PA support" banner when `caps.hasPaProfile=false` (Atlas / RedPitaya); informational warnings for Ganymede 500 W follow-up and PureSignal recovery linkage. Mirrors Thetis comboRadioModel_SelectedIndexChanged (setup.cs:19812-20310 [v2.10.3.13]).

### TX
- **TwoToneController** routes through the new math kernel (`bTwoTone=true` so txMode=2 selects `_2ToneDrivePowerSource`; gain compensation applies during the IMD test).
- **ATT-on-TX-on-power-change safety scaffolding** added: when PureSignal arrives in
  Phase 3M-4, the safety gate that forces TX attenuator to 31 dB on drive-power changes
  (preventing RX frontend over-drive via the PS feedback DDC) will activate automatically —
  no further wiring needed. **In v0.3.2 this subsystem has no observable behaviour**
  because PureSignal is not yet implemented; the predicate gating the safety check
  returns false unconditionally. The state and tests pre-stage the 3M-4 enablement.
  See `docs/architecture/pa-calibration-hotfix.md` §5.
- New `TransmitModel::computeAudioVolume(profile, band, sliderWatts)` math kernel (HL2 sentinel + Bypass-profile short-circuit + Thetis dBm math).
- New `TransmitModel::setPowerUsingTargetDbm(...)` deep-parity wrapper (all txMode branches + drive-source enum routing + power_by_band write side-effect on txMode 0 + ATT-on-TX gate + audioVolumeChanged signal).
- New `m_powerByBand[14]` per-band normal-mode power array (default 100 W per `limitPower_by_band` console.cs:1817 [v2.10.3.13]).

### NereusSDR-original deviations (justified inline)
- **HL2 sentinel**: `gbb >= 99.5` short-circuits `computeAudioVolume` to a linear fallback (`clamp(sliderWatts/100, 0, 1)`). Preserves the pre-v0.3.2 HF-transmit behaviour on HL2, whose factory PA gain row at clsHardwareSpecific.cs:484 [v2.10.3.13-beta2 @c26a8a4] is 100.0 on HF as a "no compensation" marker (Thetis HL2 users live with `audio_volume ~= 0.0009`; NereusSDR's existing v0.3.1 users must not regress). Bypass profile (all-100.0 sentinel) trips the same fallback.
- **NereusSDR-canonical PA profile serialization** (27-entry → 171-pipe-delimited fields). NOT byte-compatible with Thetis 423/507. Thetis profile-string import is a deferred follow-up.
- **PA Values page promoted to its own dialog page** (Thetis hosts `panelPAValues` inline on the Watt Meter page). Cross-page wiring routes Reset PA Values from the WattMeter page to the Values page's `resetPaValues()` slot.

### Known limitations / deferred
- ChannelMaster `SetTXFixedGain` is a glue stub at `third_party/wdsp/src/txgain_stub.c` that stores the IQ scalar but doesn't apply DSP attenuation. The wire-byte path is the primary K2GX safety lever; full DSP attenuation lands when ChannelMaster is ported (3L).
- XVTR transverter LO band translation not ported (NereusSDR has only one XVTR slot; sentinel fallback in `computeAudioVolume` catches `Band::XVTR` and falls back to linear).
- PureSignal feedback DDC + `chkFWCATUBypass` live wiring deferred to Phase 3M-4. The ATT-on-TX gate becomes active when this lands.
- Andromeda Ganymede 500 W PA tab deferred (informational warning shown when `caps.canDriveGanymede=true`).
- Thetis profile-string import deferred.

### Acknowledgments
Thanks to **K2GX** for the field report that triggered this hotfix.

## [0.3.1] - 2026-05-03

> [!NOTE]
> **You can transmit SSB now, on every board including the Hermes Lite 2.** v0.3.0 was pulled within hours of release for a packaging fix and effectively never reached testers. v0.3.1 is the first build most users will see — it carries forward everything 0.3.0 was supposed to deliver (SSB voice transmit with broadcast-grade processing, the rebuilt VPN-reach connection workflow, the expanded HL2 configuration surface, status-bar redesign, signed/notarized macOS builds) **plus** the post-0.3.0 polish: per-profile TX bandwidth control, user-editable filter presets, TX filter overlay on the panadapter and waterfall, mode-aware filter grids, the per-board PA forward-power calibration system (Watt Meter / PA Values pages), the Setup IA reshape, and the Hermes Lite 2 ATT/filter safety audit closure that clears HL2 SSB transmit for bench testing.

> [!IMPORTANT]
> 📖 **Alpha testers — start here:** [docs/debugging/v0.3.1-alpha-tester-smoketest.md](https://github.com/boydsoftprez/NereusSDR/blob/main/docs/debugging/v0.3.1-alpha-tester-smoketest.md)
>
> Walkthrough of what to try, what "success" looks like on your radio, and what's intentionally cold so you don't file bugs against it. v0.3.1 expands the v0.3.0 doc with the new TX bandwidth controls, filter preset editor, TX filter overlay, and **clears HL2 for bench-TX** with the ATT/filter safety steps the audit produced. Returning testers — receive-side coverage didn't move; the v0.2.3 doc is still the right reference for RX-side behavior.

### Upgrade Steps

* **Existing users** — no action required. Your saved radios, mic profiles, DSP settings, and per-band Clarity memory carry forward. Mic profiles gain new `FilterLow` / `FilterHigh` keys; existing profiles get sensible defaults seeded automatically on first launch.
* **Fresh installs** — the spectrum and waterfall ship 12 dB lower than they did in v0.2.3 so band noise no longer floods the bottom of the panadapter on a typical residential antenna. If you preferred the old look, Setup → Display → Reset to legacy defaults.
* **macOS** — the DMG and PKG are Apple Developer ID-signed and notarized. Gatekeeper should accept both without right-click → Open. If you have an older alpha installed, uninstall it before installing v0.3.1.
* **Hermes Lite 2** — N2ADR filter board options and step-attenuator settings are saved per radio (was global). Verify Setup → Hardware → Hermes Lite Options matches your prior config the first time you connect. **HL2 TX is now bench-cleared** — see the alpha-tester guide for the ATT/filter pre-flight checklist before keying up.
* **Anyone running the v0.3.0 build that briefly published** — uninstall and replace with v0.3.1. The 0.3.0 artifacts shipped a Windows portable bundle with missing MinGW runtime DLLs; that's fixed in 0.3.1.

### Breaking Changes

* The PSU/supply-voltage status indicator is gone. The PA drain-voltage label remains on MkII-class boards (Saturn / G2 / 8000D / 7000DLE / OrionMkII / Anvelina Pro 3) and is now the sole supply readout.
* The "OC Outputs" setup page is renamed to "Hermes Lite Control" when an HL2 is connected.
* Default spectrum/waterfall levels shifted 12 dB lower (new installs only).
* Clarity (adaptive noise-floor tuning) defaults to ON for new installs.
* **PA / Watt Meter / PA Values** are now top-level Setup categories (previously bundled under Transmit). The Hardware → Calibration tab Group 5 was relabelled "PA Current (A) calculation" → "Volts/Amps Calibration".
* Transmit → "Power & PA" was renamed to **Power**; the placeholder per-band PA gain group was removed (deferred to 3M-3 follow-up).

### New Features

**You can transmit SSB voice — on every supported radio family, including Hermes Lite 2.** Microphone in, RF out. The HL2 ATT/filter safety audit closed during 0.3.1 polish — the step-attenuator and filter gating have been verified end-to-end on HL2 hardware. The MOX path is hardened with VOX, anti-VOX, and a two-tone test mode for measuring carrier suppression and IMD.

**Broadcast-grade transmit audio.** A full processing chain you can dial in or run from a preset:

* 10-band transmit equalizer with click-and-drag parametric editor
* Transmit leveler and ALC with attack/decay/hang
* Multi-band continuous-frequency compressor (CFC) with per-band gain and post-EQ
* Compander pre-distortion / drive-ratio (CPDR)
* Controlled-envelope SSB (CESSB) for legal-limit power without flat-topping
* Phase rotator for symmetric peak distribution

**21 factory mic profiles** ported verbatim from Thetis (Heil PR40, Heil PR781, Yamaha CM500, Behringer XM8500, etc.) — drop-down on the TX applet, save your own with one click. Each profile now includes per-profile TX bandwidth (FilterLow/FilterHigh) seeded from sensible defaults.

**Per-profile TX bandwidth control** *(new in 0.3.1 since rc1)*:

* TxApplet gains TX BW low/high spinboxes and a live status label (e.g. `100 – 3100 Hz · 3.0 kHz`)
* TX Filter group on TxProfileSetupPage for editing per-profile bandwidth
* Debounced TX filter → WDSP path applies changes without keying glitches
* MicProfileManager extended with FilterLow/FilterHigh on every profile

**User-editable filter preset store** *(new in 0.3.1 since rc1)*:

* Filter presets are now editable per-mode (CW / SSB / AM / FM / DIGI)
* New Setup → Filters → Filter Presets page with per-mode lists and edit dialog
* `FilterPresetEditDialog` for adjusting low/high cut and naming presets
* Mode-aware filter preset grid on RxApplet — buttons reflect the current mode
* Shift+click a preset on the TX applet to snap TX BW to match the active RX preset

**TX filter overlay on the panadapter and waterfall** *(new in 0.3.1 since rc1)*:

* TX bandwidth is drawn on the spectrum and waterfall during MOX/TUNE — orange when transmitting, cyan otherwise
* Z-ordered under bandplan and frequency labels so it doesn't obscure tick marks
* TX/RX overlay color pickers in Setup → Display → Colors & Theme
* Color pickers consolidated under one Colors & Theme section (was scattered across Spectrum / Waterfall / Grid)

**Connection workflow rebuilt:**

* Radios behind a VPN tunnel (WireGuard, ZeroTier, Tailscale) now connect — the unicast probe path doesn't require UDP broadcast.
* New 16-model picker organized by silicon family (Auto-detect, Atlas, Hermes, Hermes II, Angelia, Orion, Orion MkII, Hermes Lite 2, Saturn).
* Auto-connect-on-launch with a per-radio toggle.
* When the radio drops, the spectrum fades and shows a "DISCONNECTED" overlay you can click to open the connection panel — no more frozen-spectrum mystery.
* Saved radios stay in the list permanently; only discovered-only entries age out.

**Status bar redesign:**

* The title bar now shows a compact strip: state dot · transmit Mbps · ping ms · receive Mbps · audio indicator. Hover for the full diagnostic tooltip.
* The receive panel adapts to window width: at full width it shows mode, filter, AGC, NR, NB, APF, and squelch side-by-side; on narrow windows the less-critical badges drop out and a "…" overflow chip lists what was hidden.
* New ADC overload badge (yellow on any clip, red on level > 3, auto-clears).
* New CPU readout — right-click to toggle between system-wide and this-process-only.

**Per-board PA forward-power calibration** *(new in 0.3.1):*

* `PaCalProfile` model with `PaCalBoardClass` enum (None / Anan10 / Anan100 / Anan8000); HL2 maps to Anan10.
* `CalibrationController` persists 10 cal-points per board class, scoped per-MAC.
* New Setup → PA → Watt Meter page hosts the calibration grid.
* `alex_fwd` reading routed through `CalibratedPAPower` interpolation; SWR protection scaling factor applied at every `setTxDrive` site.

**Setup IA reshape** *(new in 0.3.1):*

* New top-level **PA** category in SetupDialog (gated on `caps.hasPaProfile`) with three sub-pages: PA Gain (placeholder for 3M-3 follow-up), Watt Meter (cal points), PA Values (9 live telemetry fields including FWD / REV / SWR / PA current / temperature / supply volts / ADC overload / raw ADC).
* Hardware → Calibration tab is always visible; PA-specific groups self-gate at the new top-level.

**P1 wire-format parity expanded** *(new in 0.3.1):*

* bank 11 C2 wires `line_in_gain` (low 5 bits) + `puresignal_run` flag (bit 6)
* bank 11 C3 wires `user_dig_out` (low 4 bits)
* C0 XmitBit on frequency banks (1, 2, 3, 5-9) under MOX
* New `RadioConnection` virtual setters: `setLineInGain`, `setUserDigOut`, `setPuresignalRun` (P1 + P2 overrides)

**Hermes Lite 2 configuration surface expanded:**

* Hermes Lite Options tab: I2C control, I/O pin state, real-time pin readout
* N2ADR filter board: HERCULES toggle writes all 13 SWL pin-7 entries
* SWL bands × 7-pins matrix on the OC Outputs page
* Step attenuator now accepts the full signed −28..+32 dB range
* All HL2 settings persist per radio
* **HL2 SSB transmit is bench-cleared** as of 0.3.1 — ATT/filter audit closed during polish

**More accurate diagnostics:**

* Network Diagnostics dialog with 4-section health grid
* Ping/round-trip readout uses a min-filter window — sub-millisecond LAN connections now read correctly instead of showing a smeared average

**Capability-gated UI cleanups** *(new in 0.3.1)*:

* `hasStepAttenuatorCal` gates the Adaptive auto-att mode visibility
* `hasSidetoneGenerator` gates the CW Sidetone Volume slider
* `hasPennyLane` gates the User Dig Out UI on `OcOutputsTab`
* Antenna popup builder is now capability-gated and shared between VfoWidget and RxApplet
* Filter preset grid is mode-aware (driven by `commonPresetsForMode`)
* DSB and DRM modes added to VFO mode list (11-mode parity with RxApplet)

**XIT support wired** *(new in 0.3.1 since rc1)*:

* VFO Flag X/RIT tab and RxApplet wire XIT placeholders to SliceModel
* `xitHz` offset applied to TX frequency (mirrors RIT pattern on RX)

### Bug Fixes

* **PA voltage on ANAN-G2 and other MkII radios was off by ~20 %** — formula corrected to match Thetis. A live G2 reading 13.8 V was being displayed as 11.0 V; now reads correctly.
* **HL2 mic_ptt was inverted on every non-HL2 P1 board** — eliminates rapid T/R relay clicking on TUNE/TX for Atlas, Hermes, HermesII, Angelia, Orion, OrionMkII, AnvelinaPro3, RedPitaya. Matches Thetis `networkproto1.c:597-598` direct polarity.
* **Hermes Lite 2 PA enable** — the bit that turns on the HL2 power amplifier on transmit was missing in earlier builds; HL2 transmit now works on the wire (and is now bench-cleared as of 0.3.1).
* **P2 ANAN transmit path** — a WDSP filter stage was incorrectly active on Protocol 2 and broke ANAN transmit; now gated to the protocols that need it.
* **Hermes Lite 2 mic decimation** — radio-rate mic samples are now correctly down-converted to 48 kHz before reaching the transmit chain (PR #161, KM4BLG).
* **Hermes Lite 2 N2ADR settings used to leak between radios** — now stored per-MAC.
* **PA cal-point spinbox edits weren't persisting** *(0.3.1 since rc1)* — `PaCalibrationGroup` now routes spinbox edits through the controller's save() path.
* **macOS .pkg filename was missing the -rcN suffix on pre-release tags** *(0.3.1 since rc1)* — release.yml now preserves the pre-release suffix in the .pkg artifact name.
* **//MI0BOT author tag was being dropped during ports** *(0.3.1 since rc1)* — pre-commit hook now preserves verbatim near `mi0bot setup.cs:5463` cite.
* **Network Diagnostics throughput showed 0.00 Mbps** for any real traffic — fixed a redundant unit conversion.
* **Glyph rendering on macOS** — up/down arrows and em-dashes were rendering as garbage characters under certain font fallback paths. Switched to Unicode codepoints, sweep complete.
* **TX filter coalescing was dropping mid-flight changes** *(0.3.1 since rc1)* — replaced a broken QTimer debounce with direct WDSP application; aligned tests with direct-apply behavior.
* **TX filter and bandplan z-order on the spectrum** *(0.3.1 since rc1)* — TX/RX overlays now sit under bandplan and frequency labels.
* **Spectrum overlay defaults** *(0.3.1 since rc1)* — TX overlay flags now default to True; spectrum repaints on MOX flip.
* **AGC-T slider direction on VFO Flag** *(0.3.1 since rc1)* — now matches RxApplet (and Thetis) instead of inverting.
* **Filter overlay color theming** *(0.3.1 since rc1)* — passband cyan in RX, orange in TX/TUNE, MOX-gated to avoid flicker.

### Performance Improvements

* Transmit mic resampler runs without periodic stalls (clean 720 → 256 sample re-blocking).
* PipeWire audio backend on Linux delivers lower latency and no stutter under load.
* Spectrum overlay is cached and only invalidated on real changes — fewer unnecessary GPU uploads.
* AppletPanel scrollbar reserves a fixed 8 px gutter to prevent content clipping/repaint thrash *(0.3.1 since rc1)*.

### Other Changes

* Build system bumped to Qt 6.8 LTS (3-year support window) on all platforms.
* Linux PipeWire support is the default Linux audio path; the legacy pactl/FIFO bridge stays as a fallback.
* macOS builds use the Apple Silicon-native arm64 toolchain.
* All published artifacts are GPG-signed; checksums file shipped alongside.
* Bundled neural noise-reduction models (rnnoise + DeepFilterNet3) ship inside every release artifact.
* Substantial style-constants consolidation pass — TitleBar, RxApplet, PhoneCwApplet, FmApplet, EqApplet, VaxApplet, VfoWidget, SpectrumOverlayPanel, all Setup pages, AboutDialog, SupportDialog, NetworkDiagnosticsDialog, AddCustomRadioDialog, GeneralOptionsPage, AppearanceSetupPages, TransmitSetupPages now use canonical `Style::*` palette helpers (`applyDarkPageStyle`, `kLineEditStyle`, `kButtonStyle`, `kSliderStyle`, `kComboStyle`, `kLabelMid`, `dspToggleStyle`, `doubleSpinBoxStyle`).
* TxApplet visual cleanup: TUNE+MOX repositioned above VOX+MON for action-button prominence; NYI rows (ATU / MEM / Tune Mode / DUP / xPA) removed; SWR Prot LED wired to `SwrProtectionController`; SWR gauge wired to `powerChanged`.
* RxApplet AF gain row dropped (the VFO flag and master cover the same surface); Mute relocated; AGC-T moved to its own full-width row (Option B from the polish plan).
* PhoneCwApplet CW tab is now a stub until 3M-2 ships.
* MainWindow hides ghost applets until their feature phases ship.
* "Colour" → "Color" (American spelling) in user-visible strings; internal type names retain "Colour" for Thetis source parity.
* TxCfcDialog landed scalar-complete (profile combo + global spinboxes + 30 per-band spinboxes for F/COMP/POST-EQ across 10 bands) but is visually spartan; full Thetis-faithful `ucParametricEq` widget port is queued as a follow-up.

## [0.3.1-rc1] - 2026-05-02

Pre-release for bench verification of the P1 full-parity epic and Setup IA
reshape. Triggered by rapid T/R relay clicking on the ANAN-10E (HermesII)
on TUNE/TX. Targets all non-HL2 P1 boards for byte-faithful Thetis parity,
ports the per-board PA forward-power calibration system, and restructures
the Setup tree to mirror Thetis's information architecture.

### Fixes (RF-impacting)
- **fix(p1/standard): mic_ptt direct polarity** — eliminates rapid T/R relay
  clicking on TUNE/TX for every non-HL2 P1 board (Atlas, Hermes, HermesII,
  Angelia, Orion, OrionMkII, AnvelinaPro3, RedPitaya). Matches Thetis
  `networkproto1.c:597-598 [v2.10.3.13]` direct polarity.

### Features
- P1 wire-format parity (vs Thetis `networkproto1.c [v2.10.3.13]`):
  - bank 11 C2 wires `line_in_gain` (low 5 bits) + `puresignal_run` flag (bit 6)
  - bank 11 C3 wires `user_dig_out` (low 4 bits)
  - C0 XmitBit on frequency banks (1, 2, 3, 5-9) under MOX
- New `RadioConnection` virtual setters: `setLineInGain`, `setUserDigOut`,
  `setPuresignalRun`. P1 + P2 overrides; `TransmitModel` properties wired.
- Per-board PA forward-power calibration:
  - `PaCalProfile` model with `PaCalBoardClass` enum (None / Anan10 / Anan100 /
    Anan8000); HL2 maps to `Anan10` (mi0bot `setup.cs:5463-5466` byte-faithful).
  - `CalibrationController` persistence (per-MAC under
    `hardware/<mac>/paCalibration/calPoint{1..10}`).
  - `PaCalibrationGroup` widget — 10 cal-point spinboxes per board class.
  - `alex_fwd` reading routed through `CalibratedPAPower` interpolation.
  - `TransmitModel::swrProtectFactor` applied at every `setTxDrive` scale site.
- Three `BoardCapabilities` audit-gap closures:
  - `hasStepAttenuatorCal` gates Adaptive auto-att mode.
  - `hasSidetoneGenerator` gates CW Sidetone Volume slider visibility.
  - `hasPennyLane` gates User Dig Out UI on `OcOutputsTab`.

### Setup IA reshape (mirrors Thetis's information architecture)
- New top-level **PA** category in `SetupDialog` (gated on `caps.hasPaProfile`),
  with three sub-pages:
  - **PA Gain** — placeholder for 3M-3 work (per-band gain table + auto-cal)
  - **Watt Meter** — hosts the migrated `PaCalibrationGroup`
  - **PA Values** — 9 live telemetry fields (FWD / REV / SWR / PA current /
    temperature / supply volts / ADC overload / raw FWD-ADC / raw REV-ADC)
- Hardware → Calibration tab Group 5 relabelled "PA Current (A) calculation"
  → "Volts/Amps Calibration" (matches Thetis `groupBoxTS27` intent). Internal
  members renamed to upstream `udAmpSens` / `udAmpVoff`.
- Transmit → "Power & PA" renamed to **"Power"**; placeholder PA group
  (per-band-gain + Fan Control NYI labels) removed.
- Over-aggressive `setTabVisible` gate on Calibration tab dropped — tab is
  now always visible; PA-specific groups self-gate at the new top-level.

### Imported from main (since v0.3.0)
- fix(connect): wait for wisdom completion via connect-then-check pattern
- fix(persistence): restore last-used band/frequency on launch
- fix(persistence): flush coalesced slice save on close, wire missing signals
- fix(shutdown): eliminate ~5 s Cmd-Q beach ball
- fix(tx): construct `TxChannel` with no Qt parent so `moveToThread` works
- fix(wdsp): always show wisdom progress dialog (collapse fast/slow paths)
- docs(readme): bring v0.3.0 features forward in main narrative
- docs: v0.3.0 alpha-tester smoketest + README pointer

### Tests
- 17 new test files / 60+ new test cases across the two sub-epics.
- All epic-relevant tests green locally on macOS. Full ctest expected green
  on Linux / macOS / Windows in CI.

### Known status
- **Not yet bench-verified on hardware.** That's the whole point of this RC.
- Open follow-ups tracked separately:
  - v0.3.0 Windows portable bundle missing MinGW runtime DLLs
    (`libgcc_s_seh-1.dll` + `libstdc++-6.dll` + `libwinpthread-1.dll`);
    FFmpeg DLLs depend on these. Fix in `release.yml` deploy step targeted
    for v0.3.1 final.
  - 9 Thetis Transmit-tab groups not yet ported (`grpPATune`, `chkPulsedTune`,
    `chkRecoverPAProfileFromTXProfile`, `chkLimitExtAmpOnOverload`,
    `udTXFilterLow/HighSave`, `grpTXAM`, `grpTXMonitor`, `grpTXFilter`,
    `chkTXExpert`) — pre-existing gaps documented in TODO in `PowerPage`.

### Source citations
Every wire-format change cites Thetis `[v2.10.3.13]` per `HOW-TO-PORT.md`.
mi0bot HL2-specific ports cite `[v2.10.3.13-beta2]`. Production code uses
plain version stamps (no sha-in-brackets); test files use the richer form
where appropriate.

J.J. Boyd ~ KG4VCF

## [0.3.0] - 2026-05-01

### Features
- feat(applet): TxEqDialog parametric panel + style fix (Batch 9)
- feat(applet): TxCfcDialog full Thetis-verbatim rewrite (Batch 8)
- feat(core): TxChannel::getCfcDisplayCompression WDSP wrapper (Batch 7)
- feat(core): ParaEqEnvelope + TransmitModel.txEqParaEqData (Batch 6)
- feat(widget): ParametricEqWidget JSON + public API + test (Batch 5)
- feat(widget): ParametricEqWidget mouse + wheel + 6 signals (Batch 4)
- feat(widget): ParametricEqWidget paintEvent + 10 draw helpers (Batch 3)
- feat(widget): ParametricEqWidget axis math + ordering (Batch 2)
- feat(widget): ParametricEqWidget skeleton + palette + ctor (Batch 1)
- feat(chrome): finish 3Q chrome — SVG icons, glyph encoding, voltage source-first, CPU toggle
- feat(hl2): Hl2OptionsTab — Hermes Lite Options + I2C Control + I/O Pin State
- feat(hl2): N2ADR HERCULES toggle writes 13 SWL pin-7 entries (closes 30% gap)
- feat(hl2): real OcOutputsSwlTab — 13 SWL bands × 7 pins matrix (mi0bot parity)
- feat(widgets): RxDashboard drop-priority on narrow window
- feat(mainwindow): restyle right-side strip — PSU/PA/CPU MetricLabel + TX StatusBadge
- feat(model): extend Band enum with 13 SWL bands (mi0bot parity for HL2 N2ADR)
- feat(widgets): add MetricLabel — labelled-metric pair for status strip
- feat(mainwindow): swap m_callsignLabel for StationBlock — radio-name anchor
- feat(mainwindow): replace m_statusConnInfo with RxDashboard (F.1)
- feat(widgets): RxDashboard — RX1 + freq + mode/filter/AGC + active-only NR/NB/APF/SQL
- feat(mainwindow): segment hover tooltip — aggregated diagnostic body
- feat(mainwindow): wire ConnectionSegment to RadioModel + AudioEngine
- feat(titlebar): rebuild ConnectionSegment — single dot + RTT + audio pip
- feat(hl2): relabel OC Outputs → "Hermes Lite Control" + extend visibility for HL2
- feat(gui): NetworkDiagnosticsDialog — 4-section diagnostic grid
- feat(hl2): bring-up carry-forward — sequenced I2C probe + persistence + diagnostic surface
- feat(audio): flowStateChanged signal — Healthy/Underrun/Stalled/Dead FSM
- feat(connection): supplyVoltsChanged + userAdc0Changed signals
- feat(connection): pingRttMeasured signal — RTT from C&C round-trip
- feat(connection): rolling byte-rate counters for ▲▼ Mbps readout
- feat(widgets): add StationBlock — clickable bordered radio-name anchor
- feat(widgets): add StatusBadge — icon-prefix pill with 5 colour variants
- feat(profile): MicProfileManager live capture/apply for CFC/CPDR/CESSB/PhRot (3M-3a-ii G.2)
- feat(applet): TxApplet PROC + CFC enable + TxCfcDialog (3M-3a-ii F + A)
- feat(connection): cold-launch ConnectionPanel auto-open (3Q polish)
- feat(setup): CFC page (PhRot + CFC + CESSB) + dashboard live status (3M-3a-ii E)
- feat(connection): edit saved radio entries (3Q polish)
- feat(profile): MicProfileManager bundles 41 CFC/CPDR/CESSB/PhRot keys (3M-3a-ii G)
- feat(model): wire TM → TxChannel routing for CFC/CPDR/CESSB/PhRot (3M-3a-ii D)
- feat(model): TransmitModel CFC/CPDR/CESSB/PhRot properties (3M-3a-ii C)
- feat(tx): add TxChannel phrot Corner/Nstages/Reverse wrappers (3M-3a-ii B.2)
- feat(tx): add TxChannel CFC + CPDR + CESSB wrappers (3M-3a-ii B)
- feat(setup): SpeechProcessorPage rewrite as TX dashboard (3M-3a-i E)
- feat(applet): TxEqDialog adds profile combo + Save/Save-As/Delete (3M-3a-i A.2)
- feat(profile): port 20 Thetis factory TX profiles verbatim (3M-3a-i A.2)
- feat(applet): wire EQ right-click + Tools menu to TxEqDialog launch (3M-3a-i A.1)
- feat(applet): TxEqDialog scaffold — 10-band sliders + freq spinboxes + preamp + Nc/Mp/Ctfmode/Wintype (3M-3a-i A.1)
- feat(applet): TxApplet quick-toggle row [LEV] [EQ] [PROC] (3M-3a-i F)
- feat(setup): AgcAlcSetupPage adds TX Leveler + TX ALC sections (3M-3a-i D)
- feat(model): wire TransmitModel→TxChannel TX EQ + Lev + ALC routing (3M-3a-i Batch 2)
- feat(profile): MicProfileManager bundles 27 EQ/Lev/ALC keys (3M-3a-i G)
- feat(model): TransmitModel TX EQ + Leveler + ALC properties (3M-3a-i C)
- feat(tx): add TxChannel EQ + Leveler/ALC wrappers (3M-3a-i B)
- feat(3m-1c): RadioModel TxMicSource lifecycle + AudioEngine PC override gate
- feat(3m-1c): VOX defensive guards on all 5 DEXP-touching setters
- feat(3m-1c): P2 port 1026 mic frame parsing → TxMicSource
- feat(3m-1c): P1 EP6 mic16 extraction → TxMicSource
- feat(3m-1c): TxMicSource — Thetis Inbound/cm_main port
- feat(3m-1c): Phase L — cross-cutting wiring + 720→256 mic re-blocker
- feat(3m-1c): Phase J — TxApplet 2-TONE button + profile combo + Setup TX Profile page (J.1-J.4)
- feat(3m-1c): Phase I — TwoToneController activation handler (I.1-I.5)
- feat(3m-1c): Phase H — Setup → Test → Two-Tone page
- feat(3m-1c): Phase G — VFO Flag TX badge wire-up + Phase L routing demo
- feat(3m-1c): Phase F — MicProfileManager class (load/save/delete/setActive)
- feat(3m-1c): Phase E.2-E.6 — 12 TXA PostGen wrapper setters
- feat(3m-1c): Phase E.1 — TxChannel push-driven refactor
- feat(3m-1c): Phase D.1/D.2 — AudioEngine micBlockReady signal + clearMicBuffer
- feat(3m-1c): Phase C.2/C.3/C.4 — multicast Pre/Post MOX state-change signals
- feat(3m-1c): Phase B.3 — add DrivePowerSource enum + TwoToneDrivePowerSource property
- feat(3m-1c): Phase B.2 — add 7 two-tone test properties to TransmitModel
- feat(3m-1c): Phase B.1 — rename 15 TransmitModel keys to Thetis column names
- feat(3n-5.5): wire Developer ID signing + notarization for macOS
- feat(tx): wire WDSP Leveler stage with upstream defaults — pull from 3M-3a
- feat(3m-1b): L.3 Mic-source HL2 force-Pc on connect — Phase L complete
- feat(3m-1b): L.2 AppSettings persistence per-MAC for TransmitModel
- feat(3m-1b): L.1 RadioModel owns Pc/Radio mic sources + composite router
- feat(3m-1b): K.2 MOX rejection toast + tooltip override — Phase K complete
- feat(3m-1b): K.1 BandPlanGuard SSB-mode allow-list for TX
- feat(3m-1b): J.3 TxApplet MON toggle + volume slider + mic-source badge — Phase J complete
- feat(3m-1b): J.2 TxApplet VOX toggle button + settings popup
- feat(3m-1b): J.1 TxApplet Mic Gain slider
- feat(3m-1b): I.4 Mic gain slider per-board range + I.3 nit fixes
- feat(3m-1b): I.3 Radio Mic settings group with per-family layout
- feat(3m-1b): I.2 PC Mic settings group on AudioTxInputPage
- feat(3m-1b): I.1 AudioTxInputPage skeleton + TransmitModel::micSource
- feat(3m-1b): H.5 mic_ptt extraction from P1/P2 status frames — Phase H complete
- feat(3m-1b): H.4 MoxController PTT-source dispatch (MIC/CAT/VOX/SPACE/X2)
- feat(3m-1b): H.3 MoxController VOX hang-time + anti-VOX gain + anti-VOX source
- feat(3m-1b): H.2 MoxController::setVoxThreshold with mic-boost-aware scaling
- feat(3m-1b): H.1 MoxController::setVoxEnabled with voice-family mode-gate
- feat(3m-1b): G.6 wire setMicXlr (P2-only byte-50 bit 5; P1 storage-only) — Phase G complete
- feat(3m-1b): G.5 wire setMicPTT (P1 case-11 C1 bit 6 + P2 byte-50 bit 2, polarity inverted)
- feat(3m-1b): G.4 wire setMicBias (P1 case-11 C1 bit 5 + P2 byte-50 bit 4)
- feat(3m-1b): G.3 wire setMicTipRing (P1 case-11 C1 bit 4 + P2 byte-50 bit 3, polarity inverted)
- feat(3m-1b): G.2 wire setLineIn (P1 case-10 C2 bit 1 + P2 byte-50 bit 0)
- feat(3m-1b): G.1 wire setMicBoost (P1 case-10 C2 bit 0 + P2 byte-50 bit 1)
- feat(3m-1b): F.3 add CompositeTxMicRouter — selector with HL2 force-PC + MOX-locked switching
- feat(3m-1b): F.2 add RadioMicSource — SPSC ring drained by audio thread
- feat(3m-1b): F.4 add RadioConnection::micFrameDecoded Qt signal
- feat(3m-1b): F.1 add PcMicSource — TxMicRouter impl tapping AudioEngine::pullTxMic
- feat(3m-1b): E.3 add AudioEngine::txMonitorBlockReady slot for TXA siphon mix
- feat(3m-1b): E.2 add AudioEngine TX monitor enable + volume state
- feat(3m-1b): E.1 add AudioEngine::pullTxMic for PcMicSource tap
- feat(3m-1b): D.7 add TxChannel TX meter readouts (TxMic + ALC live; 4 deferred)
- feat(3m-1b): D.6 add TxChannel mic-mute path via setMicPreamp + recomputeTxAPanelGain1
- feat(3m-1b): D.5 add TxChannel::sip1OutputReady signal for MON siphon
- feat(3m-1b): D.4 expand TxChannel::setStageRunning to MicMeter/AlcMeter/AmMod/FmMod
- feat(3m-1b): D.3 add TxChannel VOX/anti-VOX WDSP wrappers (5x)
- feat(3m-1b): D.2 add TxChannel per-mode TXA config setters
- feat(3m-1b): add TransmitModel MON properties (2x)
- feat(3m-1b): add TransmitModel anti-VOX properties (2x)
- feat(3m-1b): add TransmitModel VOX properties (4x)
- feat(3m-1b): add TransmitModel mic-jack flag properties (8x)
- feat(3m-1b): add TransmitModel::micGainDb + derived micPreampLinear
- feat(settings): migrate manual-IP-port macKey to real MAC on probe success (3Q-12)
- feat(discovery): saved radios never age out; discovered-only at 60s (3Q-11)
- feat(connection): auto-connect failure path + multi-flag handling (3Q-10)
- feat(3m-1b): add BoardCapabilities::hasMicJack flag
- feat(menu): role-based Radio menu — Connect/Disconnect mutually exclusive (3Q-9)
- feat(spectrum): disconnect overlay — fade + DISCONNECTED + click-to-open (3Q-8)
- feat(statusbar): verbose connection-info strip (3Q-7)
- feat(titlebar): connection segment with state dot, rates, activity LED (3Q-6)
- feat(connection): ConnectionPanel polish — pills, Last Seen, status strip (3Q-5)
- feat(connection): rebuild Add Radio dialog with model-aware SKU picker (3Q-4)
- feat(connection): structured ConnectFailure + frameReceived signal (3Q-3)
- feat(discovery): add RadioDiscovery::probeAddress() unicast probe (3Q-2)
- feat(connection): introduce ConnectionState enum on RadioModel (3Q-1)
- feat(ui): H.4 PowerPaPage Power/TunePwr/ATTOnTX/ForceATT activation (3M-1a)
- feat(ui): H.3 TxApplet TUN/Tune-Power/RF-Power/MOX activation (3M-1a)
- feat(ui): H.2 MeterPoller TX bindings on MOX (3M-1a)
- feat(ui): H.1 SpectrumWidget MOX overlay wiring (3M-1a)
- feat(3m-1a): G.4 RadioModel::setTune TUN orchestrator
- feat(tx): G.3 TransmitModel tunePowerByBand[14] + per-MAC persistence (3M-1a)
- feat(ui): G.2 wire Receive Only checkbox visibility from caps.isRxOnlySku (3M-1a)
- feat(3m-1a): G.1 — RadioModel integration (MoxController + TxChannel + TxMicRouter)
- feat(safety): F.3 port SwrProtectionController source-first TODOs (3M-1a)
- feat(tx): F.2 StepAttenuatorController TX-path activation (3M-1a)
- feat(tx): F.1 RadioModel onMoxHardwareFlipped slot (3M-1a)
- feat(tx): implement P2 sendTxIq with 24-bit BE SPSC ring (3M-1a E.6)
- feat(tx): P1 setWatchdogEnabled wire-byte emission (RUNSTOP pkt[3] bit 7) (3M-1a Task E.5)
- feat(tx): P1 setTrxRelay wire-byte emission (C3 bank 10 bit 7) (3M-1a Task E.4)
- feat(tx): P1 setMox wire-byte emission (C0 byte 3 bit 0) (3M-1a Task E.3)
- feat(tx): P1 sendTxIq wire format (EP2 zones, 16-bit I/Q) (3M-1a Task E.2)
- feat(tx): RadioConnection TX virtuals — sendTxIq + setTrxRelay (3M-1a Task E.1)
- feat(tx): TxMicRouter interface + NullMicSource stub (3M-1a Task D.1)
- feat(tx): TxChannel setRunning + 3M-1a active stages (3M-1a Task C.4)
- feat(tx): TxChannel setTuneTone via gen1 PostGen (3M-1a Task C.3)
- feat(tx): TxChannel skeleton — 31-stage TXA pipeline (3M-1a Task C.2)
- feat(tx): WdspEngine TX channel API (3M-1a Task C.1)
- feat(tx): MoxController setTune slot with manualMox flag (3M-1a Task B.5)
- feat(tx): MoxController 6 phase signals with Codex P1 boundary (3M-1a Task B.4)
- feat(tx): MoxController 6 QTimer chains (3M-1a Task B.3)
- feat(tx): MoxController skeleton with Codex P2 ordering (3M-1a Task B.2)
- feat(tx): port Thetis PTTMode enum as PttMode (3M-1a Task B.1)
- feat(spectrum): raise rewind cap to 16384 rows (~8 min default rewind)
- feat(setup): add waterfall rewind depth dropdown (E task 11)
- feat(spectrum): debounce history resize + period change (E task 10)
- feat(spectrum): pan/zoom reproject + largeShift clear (E task 9)
- feat(model): wire 3M-0 safety controllers into RadioModel + MainWindow
- feat(ui): status-bar TX Inhibit indicator + PA Status badge
- feat(setup): General → Options → prevent-different-band toggle
- feat(setup): General → Hardware Configuration group
- feat(setup): Block-TX antennas + Disable HF PA group boxes on Setup → Transmit
- feat(setup): External TX Inhibit group box on Setup → Transmit
- feat(setup): SWR Protection group box on Setup → Transmit
- feat(spectrum): static "HIGH SWR" overlay (inert until 3M-1a)
- feat(safety): per-board PA scaling table + MeterPoller telemetry routing
- feat(radio-model): paTripped() live state + Ganymede CAT trip handler
- feat(connection): setWatchdogEnabled(bool) stub on RadioConnection + P1/P2
- feat(safety): TxInhibitMonitor — GPIO poll + 4-source inhibit aggregator
- feat(safety): add SwrProtectionController — Phase 3M-0 Task 3
- feat(safety): BandPlanGuard with 60m channel tables + isValidTxFreq
- feat(caps): isRxOnlySku + canDriveGanymede for 3M-0 safety net
- feat(spectrum): paint timescale strip + LIVE button (E task 8)
- feat(spectrum): wire scrub gesture + LIVE click handlers (E task 7)
- feat(spectrum): add timescale + LIVE button rects (E task 6)
- feat(spectrum): add setWaterfallLive + paused-strip width (E task 5)
- feat(spectrum): wire scrollback into push + clear paths (E task 4)
- feat(spectrum): add ring buffer write paths (sub-epic E task 3)
- feat(spectrum): add scrollback math helpers + capacity tests (E task 2)
- feat(spectrum): add scrollback state + method decls (sub-epic E task 1)
- feat(view-menu): wire View → Band Plan to AetherSDR pattern
- feat(setup-display): bandplan region + label-size controls
- feat(mainwindow): wire BandPlanManager → SpectrumWidget
- feat(spectrum): port drawBandPlan from AetherSDR
- feat(spectrum): add BandPlanManager hookup + font-size knob
- feat(radiomodel): own BandPlanManager + load on init
- feat(bandplan): port BandPlanManager from AetherSDR
- feat(bandplan): add rac-canada bandplan JSON

### Fixes
- fix(p1/p2): gate WDSP CFIR on protocol — restore P2 ANAN TX path
- fix(p1/hl2): align WDSP TX path + bank-17/11/10 wire bytes with Thetis
- fix(p1/hl2): set bank 10 C2 bit 3 to enable HL2 PA on TX
- fix(p1/hl2): decimate radio-rate mic to 48 kHz before TxMicSource
- fix(hl2): address Codex P1 + P2 review on PR #160
- fix(hl2): N2ADR per-MAC persistence + signed S-ATT spinbox range
- fix(test): update parametric-edit test for direct-WDSP push contract
- fix(applet): TxEqDialog parametric WDSP push -- direct profile, not legacy scalars
- fix(applet): encode TX parametric EQ profile blobs
- fix(applet): wire parametric EQ to WDSP + sync from model on profile load
- fix(cmake): vendor zlib via FetchContent on Windows MinGW
- fix(applet): TxEqDialog parametric Reset button now actually resets
- fix(applet): apply project styles to TxCfc/TxEq dialog widgets
- fix(applet): drop unused <algorithm> include in TxEqDialog
- fix(widget): apply Task 5 code review feedback
- fix(widget): apply Task 4 code review feedback
- fix(test): drop unused <cmath> include in interaction test
- fix(widget): apply Task 2 code review feedback
- fix(test): drop unused <cmath> include in axis test
- fix(cpu): real cross-platform CPU counting on Linux + Windows
- fix(connection): address Codex P1 + P2 review on PR #158
- fix(diagnostics): RTT min-filter + PSU drop + Mbps fix + ship defaults + NYI honesty
- fix(hl2): address Codex P1 + P2 review comments on PR #157
- fix(chrome): partial layout-overflow fixes (WIP — ADC repositioning pending)
- fix(chrome): badge content clipping + audio-pip QChar — render fix follow-up
- fix(chrome): rendering issues — glyph fallbacks + min-widths + board code
- fix(widgets): RxDashboard re-entry guard — segfault on resize
- fix(hl2): signed −28..+32 dB step attenuator user range (mi0bot parity)
- fix(widgets): StationBlock — clean short-circuit, single applyStyle in ctor
- fix(widgets): StatusBadge — paint background + guard idempotent setters
- fix(settings): escape '+' and other non-NameChars in XML keys
- fix(applet): wire PhoneCw PROC, drop duplicate from TxApplet (3M-3a-ii H)
- fix(menu): Connect enablement reads connectionState, not isConnected (3Q polish)
- fix(connection): manual entries seed m_lastSeenMs so pill+Last Seen are accurate (3Q polish)
- fix(connection): force Disconnected state on teardown — strip + bar stuck "Connected" (3Q polish)
- fix(connection): seed-from-saved entries also populate m_discoveredRadios (3Q polish)
- fix(menu): Connect uses unicast probe — no listener leak on miss (3Q polish)
- fix(connection): live-test polish — pill widget + name preservation + seed-from-disk (3Q polish)
- fix(tx): push TX processing chain to WDSP on connect + profile activation
- fix(3m-1c): two-tone level — convert dB→linear before TXPostGen mag
- fix(3m-1c): marshal teardown setTxMicSource(nullptr) onto conn thread
- fix(3m-1c): P1 setTxDrive — port stub to working setter (HL2 bench triage)
- fix(3m-1c): refine C1 to sendPostedEvents + I3 invariant + test polish
- fix(3m-1c): arm m_lastMicAt at TxMicSource attach (I3)
- fix(3m-1c): zero-fill PC-mic short pulls in TxWorkerThread (I2)
- fix(3m-1c): add QCoreApplication::processEvents to TxWorkerThread::run (C1)
- fix(display): smooth spectrum repaint with timer-driven update loop
- fix(3m-1c): route MoxController→TxChannel lambdas via Queued dispatch
- fix(3m-1c): bump silence-drive threshold past mic-burst gap (SSB voice fix)
- fix(3m-1c): E.1 bench regression — restore TUN + SSB voice TX (no pump)
- fix(3m-1c): Phase L fixup — add 5 missing 2-tone signal connects + initial pushes
- fix(3m-1c): Phase K — initial-state-sync audit (TX monitor enable + volume)
- fix(3m-1c): correct chunk 1 attribution to AetherSDR + NereusSDR-native
- fix(3m-1c): HL2 TX step att — apply 31-N inversion in P1CodecHl2
- fix(3m-1c): HL2 PA scaling — add HermesLite case to paScalingFor
- fix(3n-5.5): use documented `list-keychains` (plural) for search-list write
- fix(3m-1b): address Codex P1+P1+P2 review on PR #149
- fix(tx): zero-fill partial mic-pull + skip fexchange2 on empty pull
- fix(tx): push initial micPreamp on TxChannel attach so SSB has gain
- fix(mox): gate onMicPttFromRadio(false) on PttMode==Mic to prevent un-key
- fix(ui): apply mic-gain to PhoneCwApplet level gauge so slider drives the meter
- fix(audio): pick hardware mic + clamp channels + permanent mic-permission key
- fix(audio): eager-open TX input mic on AudioEngine::start
- fix(3m-1b): E.4 RX-leak-during-MOX fold via activeSlice gate in rxBlockReady
- fix(connection): seed panel table from saved radios on construction (3Q polish)
- fix(connection): split Add Radio dialog into Probe (verify) + Save (3Q polish)
- fix(connection): Add Radio dialog verifies + saves; user connects from row (3Q polish)
- fix(connection): hold probing overlay for 700ms min on dialog probe success (3Q polish)
- fix(connection): live-test polish across Phase 3Q surface
- fix(3m-1b): close deskhpsdr discover script regex coverage gaps
- fix(3m-1b): add DL1YCF to deskhpsdr corpus via "by NAME" regex
- fix(discovery): clean up Task 2 — unused captures + PROVENANCE row (3Q-2 follow-up)
- fix(tx): wire TransmitModel::powerChanged → setTxDrive (Codex review)
- fix(tx): TxApplet per-band TUN-power slider + fwd-power gauge live data
- fix(tx): TX I/Q UDP destination port 1028 → 1029 (first RF on the air!)
- fix(tx): reorder TxChannel ctor init list to match declaration order
- fix(tx): TxChannel buffer size + P2 output rate (bench round 3)
- fix(tx): bench fixes — re-enable P2 TX consumer + RX mute on MOX
- fix(tx): wire TxChannel TX I/Q production loop to RadioConnection
- fix(safety): wire setAlexFwdLimit + setTunePowerSliderValue from setTune
- fix(ui): H.1-H.4 review fixups (QPointer + band wiring + ATT test)
- fix(tx): G.4 cold-off guard + ordering fidelity + missing test coverage
- fix(tx): G.3 drop s.save() from TransmitModel::save() (match AlexController)
- fix(tx): G.3 preserve //[2.10.3.5]MW0LGE author tag + test no-port-check
- fix(ui): G.2 sibling-field reset + named slot + 5th test
- fix(tx): G.1 prevent WDSP-init lambda accumulation across reconnect
- fix(safety): F.3 setTunePowerSliderValue clamp + setAlexFwdLimit doc
- fix(safety): F.3 preserve K2UE attribution at ANAN-8000D branch
- fix(tx): F.2 cross-thread dispatch + HPSDR setter wiring + cite fix
- fix(tx): F.1 cross-thread dispatch + public slots declaration
- fix(p2): E.7 setMox cleanup + drive-gate XVTR doc + m_mox threading note
- fix(p2): wire m_mox to byte 4 bit 1 + out-of-band drive gate (E.7)
- fix(tx): E.6 add keep-in-sync comment to P2 TX IQ drain loop
- fix(tx): E.5 stub-comment + drift-risk + HL2 stamp polish
- fix(tx): E.4 codec-path polarity propagation + flush priority docs
- fix(tx): E.2 critical fixups (atomic ring + cites + HL2 LSB workaround)
- fix(tx): remove unused <cstdint> include from P1RadioConnection.h (E.2 cleanup)
- fix(tx): E.1 fixups (naming alignment + idempotency + log level)
- fix(tx): C.4 fixups (explicit case arms + cite range + warning text)
- fix(tx): QFAIL -> QSKIP in C.3 HAVE_WDSP skeleton tests
- fix(tx): C.3 fixups (precision parity + attribution gaps)
- fix(tx): C.1 ownership + comments (code-review fixups)
- fix(tx): apply B.2 code-review fixups (CMakeLists + Q_DECLARE_METATYPE)
- fix(spectrum): guard GPU sentinels with NEREUS_GPU_SPECTRUM (CI #1)
- fix(spectrum): address Codex P1 + P2 on PR #140
- fix(spectrum): post-live-test fixes for sub-epic E
- fix(setup): use kMaxWaterfallHistoryRows constant (E task 11 review)
- fix(safety): address Codex P1+P2 review on PR #139
- fix(spectrum): wire debounce timer to resizeEvent (E task 10 review)
- fix(spectrum): invalidate GPU overlay on pause/scrub (E task 8 review)
- fix(safety): preserve verbatim Thetis inline author tags + thread-affinity doc
- fix(safety): close BandPlanGuard channel/range gaps + reviewer follow-ups
- fix(spectrum): wire scrollback flush to disconnect signal (E task 4 review)
- fix(spectrum): add m_wfTexFullUpload to viewport rebuild (E task 3 review)
- fix(dsp-menu): full RX DSP parity in top menu, drop overlay flyout
- fix(theme): suppress orange NYI badge — keep disable + tooltip only
- fix(theme): drop kSpinBoxStyle subcontrol override so app baseline arrows render
- fix(build): link Qt6::Svg so macdeployqt bundles libqsvg.dylib

### Docs
- docs(changelog): hermes-filter-debug HL2 N2ADR + S-ATT fixes
- docs(3M-3a-ii-followup): refresh PR-DRAFT + CHANGELOG for PR open
- docs(3M-3a-ii-followup): verification matrix + CHANGELOG (Batch 10)
- docs(widget): document Qt rect semantics divergence in paint notes
- docs(plan): apply Task 1 code review feedback to Tasks 3 + 5
- docs(plan): mark PROVENANCE rows as already-landed in Task 1
- docs(3M-3a-ii-followup): ParametricEq port plan (10 batches)
- docs(changelog): expand Phase 3Q section to cover full connect-flow rebuild
- docs(arch): shell-chrome redesign verification matrix — 20 scenarios
- docs(hl2): Phase 3L visibility brainstorm — design + lean implementation plan
- docs(arch): shell-chrome redesign implementation plan — 9 sub-PRs
- docs(arch): shell-chrome redesign spec — TitleBar/status-bar/STATION/segment
- docs(3M-3a-ii): mark complete + queue ParametricEq widget hand-off
- docs: CLAUDE.md mark 3M-3a-i Complete pending bench, 3M-3a-ii next
- docs(arch): 3M-3a-i verification matrix extension + commit summary (3M-3a-i I)
- docs(3m-3): pull forward — schedule swap with 3M-2 (HL2 audit gate)
- docs(3m-1c): verification matrix — TX pump v3 + Stage-2 fixes + HL2 + Codex
- docs(3m-1c): refresh v2-era doc comments to reflect v3 redesign (I1)
- docs(3m-1c): plan v3 supersedes v2 — Thetis-faithful semaphore-wake
- docs(3m-1c): correct TxChannel cross-thread audit doc-comments
- docs(3m-1c): TX pump redesign execution status + 720 misread corrections
- docs(3m-1c): TX pump architecture redesign plan + post-code review amendment
- docs(3m-1c): M.7 — post-code Thetis review
- docs(3m-1c): M.6 — verification matrix update (22 new rows)
- docs(3m-1c): C.1 finalize — update stale MoxController::moxChanged refs
- docs(3m-1c): implementation plan with TDD task list per chunk
- docs(3m-1c): pre-code Thetis review for chunks 1-9
- docs(3m-1c): chunk 0 — HL2 TX path desk-review vs mi0bot-Thetis
- docs(3m-1c): add chunk 0 — HL2 TX path desk-review vs mi0bot-Thetis
- docs(3m-1c): design spec — polish & persistence
- docs(build): fix Arch PipeWire package name (libpipewire → pipewire)
- docs(3m-1b): M.7 post-code Thetis review — Phase M autonomous tasks complete
- docs(3m-1b): M.6 verification matrix extension — 17 rows for 3M-1b
- docs(3m-1b): implementation plan for mic + SSB voice
- docs(3m-1b): Thetis pre-code review for mic + SSB voice
- docs(build): document missing Linux deps (qt6-svg, jack, alsa, pipewire)
- docs(captures): add Thetis TUN-engaged pcap findings + next-session prompt
- docs: execution handoff prompt for Phase 3Q
- docs: implementation plan for Phase 3Q (Connection Workflow Refactor)
- docs: introduce Phase 3Q (Connection Workflow Refactor) design + master plan placement
- docs(3m-1a): I.5 post-code Thetis review §2
- docs(3m-1a): I.4 verification matrix extension (TUNE-only First RF)
- docs(p2): E.8 — annotate setWatchdogEnabled stub with §7.8 deferral
- docs(3m-1a): add §7.11 — P1 16-bit vs P2 24-bit TX I/Q sample format
- docs(tx): D.1 doc clarifications (zero-return + alloc constraint + wire breadcrumb)
- docs(tx): C.2 fixups (stale Approach-A comments + 25→31 stage count)
- docs(tx): fix off-by-one cmaster.c line cites in TX slew constants (C.1 fixup)
- docs(tx): B.5 polish (typo + signal divider + contract notes)
- docs(tx): correct B.4 phase-signal docs + typos (B.4 review fixups)
- docs(3m-1a): disambiguate Thetis PttMode from existing PttSource
- docs(architecture): Phase 3M-1a TUNE-only First RF plan
- docs(architecture): Phase 3M-1a Thetis pre-code review
- docs: changelog entry for sub-epic E (E task 13)
- docs(phase3g): add sub-epic E verification matrix (E task 12)
- docs(plan): mark task 10 step 3 deferred (E task 10 review polish)
- docs(3m-0): verification matrix for PA safety foundation
- docs(plan): clarify task 8 wfRect-width contract for drawTimeScale
- docs(phase3g-rx-epic-e): add waterfall scrollback implementation plan
- docs(architecture): Phase 3M-0 PA Safety Foundation implementation plan
- docs(architecture): apply pre-code Thetis review corrections to 3M TX design
- docs(architecture): Phase 3M TX epic master design
- docs: audit phase status table — mark 3G-9b/c / 3G-13 / 3G-14 / 3P-H / 3P-I-b complete
- docs(verify): retarget matrix rows 6-9 to View → Band Plan
- docs(verify): 12-row manual matrix for bandplan port
- docs(vax): surface VAX in README + add alpha-guide step 15
- docs(plan): Phase 3G RX Epic sub-epic D — Bandplan overlay

### CI / Build
- chore(attribution): fix CI tag-preservation FAILs on PR #161
- chore(connection): strip 3Q diagnostic logging — root cause was duplicate app instances
- chore(attribution): regenerate contributor indexes after cfcomp PROVENANCE rows
- chore(attribution): finish cfcomp.{c,h} provenance for Batch B.1 sync
- chore(wdsp): partial sync cfcomp.{c,h} → Thetis v2.10.3.13 for Qg/Qe (3M-3a-ii B.1)
- chore: drop 3 unused includes (TxApplet/MainWindow/TransmitSetupPages)
- chore(profile): drop unused kProfileSubpath const
- chore(build): fix POST_BUILD PlistBuddy invocation for mic-permission gate
- chore(ui): drop unused <gui/StyleConstants.h> from PhoneCwApplet.cpp
- chore(3m-1b): L.1 cleanup — drop unused <memory> from ownership test
- chore(3m-1b): J.3 cleanup — drop unused <cmath> from tst_tx_applet_mon
- chore(3m-1b): J.2 cleanup — drop unused <functional> from VoxSettingsPopup.h
- chore(3m-1b): I.4 cleanup — fix kUnknown field-designator order + drop unused include
- chore(3m-1b): I.2 cleanup — remove unused <core/audio/CompositeTxMicRouter.h> in test
- chore(3m-1b): H.2 cleanup — move <cmath> from .h to .cpp + drop unused <limits> in test
- chore(3m-1b): F.2 cleanup — remove 2 unused includes
- chore(3m-1b): D.3 cleanup — move <cmath> to .cpp + wdsp_api history
- chore(3m-1b): D.2 move <stdexcept> include from TxChannel.h to .cpp
- chore(3m-1b): D.1 cleanup — remove redundant public: label + unused include
- chore(3m-1b): register deskhpsdr as recognised upstream
- chore(tx): strip 3M-1a bench-debug instrumentation
- chore(safety): add v-prefix to MW0LGE cite stamps
- chore(safety): add clsBandStackManager.cs + setup.designer.cs headers
- chore(caps): extend forBoard data table + spacing/comment polish
- ci: pin mi0bot-Thetis to c26a8a4 to fix tag-preservation drift
- ci: install Qt6::Svg dev headers across all platforms

### Other
- style(3m-1c): regroup AudioEngine public/public-slots blocks (I6)
- style(3m-1c): TxWorkerThread::run uses unique_ptr<QTimer> for RAII
- wip(tx): 5 wire-protocol fixes from first-RF debug session
- wip(tx): bench-debug checkpoint — first-RF path almost there
- revert(setup-display): remove Bandplan Overlay group box
- style(radiomodel): regroup BandPlanManager include with model headers

### Tests
- test(gui): tst_network_diagnostics_dialog — 4 cases for null-safety + reset
- test(widgets): tst_station_block — 5 cases for appearance + click signals
- test(widgets): tst_status_badge — 8 cases for variant + signal coverage
- test(3m-1c): add cross-thread queued-delivery regression tests for C1
- test(3m-1c): widen cross-thread race test to MoxController-routed setVoxRun
- test(3m-1c): RadioModel TxWorkerThread ownership test
- test(3m-1b): I.5 verify chk20dbMicBoost → VOX threshold scaling integration
- test(3m-1b): D.1 verify real mic-router drives fexchange2 with Q=0
- test(3m-1b): close 3 minor C.3 review items
- test(3m-1b): close idempotent-guard test coverage for C.2 mic-jack flags
- test(tx): assert no spurious moxStateChanged in rapid-toggle test (B.3 I-1)
- test(persistence): audit 15 AppSettings keys round-trip for 3M-0
- test(safety): add per-sample foldback test + TODO deferred conditions
- test(bandplan): failing tests for BandPlanManager loader

### Refactors
- refactor(3m-1c): TxWorkerThread semaphore-wake (no QTimer)
- refactor(3m-1c): TxChannel fexchange2→fexchange0 + interleaved double + 64-block
- refactor(3m-1c): TX pump architecture redesign — TxWorkerThread
- refactor(ui): relocate Mic Gain slider TxApplet → PhoneCwApplet + wire mic level gauge
- refactor(safety): name kPollIntervalMs constant per plan spec


## [Unreleased]

### Added (Phase 3Q — Connection Workflow Refactor / connect-flow rebuild)

Triggered by an April 2026 user report: an HL2 across a WireGuard
tunnel couldn't be reached through the existing manual-entry path,
and the disconnect state gave no feedback beyond a frozen spectrum.
This entry covers the connect / discover / disconnect flow rebuild
and the chrome layer that surrounds it.

- **Single ConnectionState state machine** — `Disconnected → Probing →
  Connecting → Connected → (Disconnected | LinkLost)` with broadcast
  scan and unicast probe as different *triggers* into the same path.
  Replaces the earlier broadcast-only-then-blind-connect flow.

- **Unicast probe** — `RadioDiscovery::probeAddress(addr, port, timeout)`
  with 1.5 s timeout, parallel P1 + P2 attempts, parses replies via the
  existing P1 / P2 reply helpers. Lets the Connect menu and the manual
  Add Radio dialog reach radios that aren't reachable via UDP broadcast
  (Layer-3 VPNs like WireGuard / ZeroTier / Tailscale).

- **Add Radio dialog rebuild** — replaces the 9-board picker with a
  16-SKU model dropdown organized by silicon family in `<optgroup>`s
  (Auto-detect first, then Atlas / Hermes (3) / Hermes II (2) /
  Angelia / Orion / Orion MkII (5) / Hermes Lite 2 / Saturn (2)).
  Two action buttons — `Probe and connect now` and `Save offline`.
  Failure path keeps the dialog open with form preserved + red
  error band; success path auto-closes and lands the row in the
  table tagged "(probe)".

- **ConnectionPanel polish.** Modal kept; status strip up top with
  inline Disconnect; state-pill column (🟢 Online <60 s · 🟡 Stale
  60 s–5 min · 🔴 Offline) replaces the bare `●`; Last Seen column
  replaces MAC; single ↻ Scan in the table header replaces
  Start/Stop Discovery; Auto-connect-on-launch checkbox added to the
  detail panel; auto-opens on launch + on disconnect; auto-closes
  1 s after Connected.

- **Radio menu rework.** `Connect (⌘K) · Disconnect (⌘⇧K) · Discover
  Now · Manage Radios… · Antenna Setup… (NYI) · Transverters… (NYI) ·
  Protocol Info` with state-aware enablement (Connect/Disconnect
  mutually exclusive; Protocol Info follows connection). Replaces
  the prior four-item-three-aliases set.

- **Spectrum disconnect overlay** — 800 ms fade to ~40 % opacity +
  DISCONNECTED label + click-anywhere-to-open-panel. Multi-cue
  feedback closes the loop on the "spectrum just freezes when the
  radio drops" complaint.

- **Stale policy change.** Saved radios *never* age out (previously
  dropped at 15 s); discovered-only radios age out at 60 s (raised
  from 15 s, long enough not to flap); the connected MAC stays
  exempt.

- **Auto-connect-on-launch** — uses the existing per-radio
  `AppSettings::autoConnect` flag; on failure the panel auto-opens
  with the target highlighted offline + a status-bar diagnostic.
  Multi-flag case picks most-recent-connected MAC + a one-time
  setup-bar warning.

- **macKey migration.** Offline entries saved with the synthetic
  `manual-<IP>-<port>` key get migrated under the real MAC on first
  probe success, preserving Name / Model / Auto-connect / Pin-to-MAC.

### Added (Phase 3Q — chrome layer)

- **Title-bar ConnectionSegment.** Collapses the old verbose connection
  text into a compact `[state dot] [▲ tx Mbps] [RTT ms] [▼ rx Mbps] [♪ audio]`
  segment that lives in the macOS title bar. Click opens the connection
  panel, right-click pops Reconnect / Disconnect / Manage Radios, hover
  shows a multi-line diagnostic tooltip with full IP / MAC / protocol /
  firmware / sample rate / throughput. The state dot color encodes
  connection state and pulses on each ep6 / DDC frame so the user can
  see "the radio is talking to me" at a glance.

- **RxDashboard with BadgePair drop ladder.** A 3-stage responsive
  dashboard in the status bar replacing the old free-text strip: at
  full width, three side-by-side `BadgePair`s show `[mode] [filter]
  [AGC] [NR] [NB] [APF]` plus a lone `SQL`; on a narrower window the
  pairs stack vertically (medium); on the narrowest, badges drop in
  priority order. Mode + filter never drop. A 30 px hysteresis
  deadband prevents the boundary-stack-flash that v0.2.3 testing
  surfaced.

- **StationBlock anchor.** The center "STATION" cell becomes a clickable
  block bearing the connected radio's name (or "Click to connect" with
  a dashed-red border when offline). Click opens the connection panel;
  right-click offers Disconnect / Edit radio… / Forget radio.

- **Right-side strip restyle.** CAT / TCI / PA voltage / CPU / PA-OK /
  TX status now use the new `MetricLabel` and `StatusBadge` widgets
  with consistent typography. Drop-priority shrinks the strip in a
  fixed order on narrow windows and surfaces dropped items via an
  `OverflowChip` (`…`) with a hover tooltip.

- **AdcOverloadBadge** — stacked `ADCx` / `OVERLOAD` two-row badge
  living between PA-OK and TX. Yellow on any ADC level > 0, red on
  any > 3 (Thetis severity rules), 2 s auto-hide.

- **SVG icon system on StatusBadge.** New `setSvgIcon(":/icons/...")`
  API renders SVG via `QSvgRenderer` with `CompositionMode_SourceIn`
  tinting; auto re-tints on `setVariant()`. Replaces the Unicode
  glyph prefixes that rendered inconsistently across font fallbacks.
  Nine SVGs ship: `badge-check`, `badge-dot`, `badge-mode` (sine
  wave), `badge-filter` (passband curve), `badge-agc` (lightning),
  `badge-nr` (smoothing wave), `badge-nb` (impulse spike), `badge-apf`
  (notch), `badge-sql` (threshold chevron).

- **CPU System / App toggle.** Right-click the CPU MetricLabel to
  switch between system-wide CPU usage and this-process-only.
  Mirrors Thetis's `m_bShowSystemCPUUsage`. Persisted as
  `CpuShowSystem` (defaults to System). 1 s tick rate, 0.8 / 0.2
  smoothing, integer percent display — all matching Thetis.

- **Min-filtered RTT.** The title-bar RTT readout now uses the
  2nd-smallest of a rolling 10-sample window instead of the mean.
  The previous mean-based smoother showed `~half_cadence` on
  sub-millisecond LAN connections (because the radio's status-packet
  cadence — P1 2.6 ms, P2 100 ms — adds bracket noise to every
  measurement) and showed random low values on WAN connections
  (because lucky-aligned samples pulled the mean down). Same min-filter
  technique TCP BBR uses.

### Changed (Phase 3Q — ship defaults aligned to live-radio testing)

- **Spectrum / waterfall ship defaults shifted down 12 dB** to match a
  typical residential HF noise floor: `DisplayGridMax -36→-48`,
  `DisplayGridMin -104→-116`, `DisplayWfHighLevel -50→-62`,
  `DisplayWfLowLevel -110→-122`, `DisplayWfBlackLevel 98→104`.
  Dynamic range (68 dB grid, 60 dB waterfall) is unchanged. Earlier
  defaults gave a noisy first-launch impression — band noise jammed
  the bottom of the panadapter and lit up the waterfall floor.

- **Clarity defaults to ON** for fresh installs. Auto-tuning the noise
  floor is the better first-launch experience than asking the user
  to find and toggle the setting.

- **PSU widget removed from status bar and Network Diagnostics dialog.**
  Source-first audit against Thetis [v2.10.3.13] confirmed
  `supply_volts` (AIN6) is dead data in Thetis —
  `computeHermesDCVoltage()` exists but has zero callers, and the
  only voltage status indicator (`toolStripStatusLabel_Volts`) reads
  `_MKIIPAVolts` which is `convertToVolts(getUserADC0)` — i.e. the
  PA drain voltage on AIN3. The PA volt label is now the sole supply
  indicator on MkII-class boards (Saturn / G2 / 8000D / 7000DLE /
  OrionMkII / Anvelina Pro 3), matching Thetis behaviour.

### Fixed (Phase 3Q — bench-caught bugs)

- **PA voltage formula correction.** `convertMkiiPaVolts` now uses
  5.0 V ADC reference and `(22+1)/1.1` divider per Thetis
  `convertToVolts` (`console.cs:24886-24892` [v2.10.3.13]). Prior
  formula used 3.3 V × 25.5 — wrong on both axes; the combined error
  was a 0.805 scaling factor (13.8 V actual displayed as 11.0 V).
  Verified live on ANAN-G2: now reads 13.3 V.

- **Network Diagnostics TX / RX rate.** Dialog was applying a redundant
  `* 8 / 1e6` conversion to values that were already in Mbps,
  producing 0.00 Mbps for any real throughput. Fixed and the
  `RadioConnection::txByteRate / rxByteRate` API doc strengthened
  to flag the misleading "ByteRate" name (returns Mbps).

- **Glyph encoding sweep.** Continuation of the v0.2.3 ebe9030 fix —
  on the macOS compile path, `\xe2\x96…` byte-escape strings inside
  `QString::asprintf` and `QStringLiteral` get misinterpreted as
  Latin-1 codepoints. Switched to `QChar(0x25B2)` / `QChar(0x25BC)` /
  `QChar(0x2014)` for: TitleBar painted Mbps `▲ ▼` glyphs, RTT
  em-dash placeholder, RadioModel connection tooltip, and four
  em-dash sites in `AntennaAlexAlex2Tab`.

- **Header initializer drift.** `m_refLevel`, `m_wfBlackLevel`,
  `m_wfHighThreshold`, `m_wfLowThreshold` member initializers in
  `SpectrumWidget.h` synced to the new ship defaults so any code
  path that reads these before `loadSettings()` runs sees the right
  values, not the stale `-50 / -110 / 98 / -36` from earlier work.

- **Network Diagnostics honesty.** Jitter / packet loss / packet gap
  rows previously showed hardcoded `0 ms` / `0.0%` / `—` placeholders
  that read as "perfect network" rather than "not measured". All
  three now show em-dash with a "Not yet measured" tooltip until
  proper protocol-level instrumentation lands.

### Added (Phase 3M-3a-ii follow-up — `ucParametricEq` Qt6 port)

- **`src/gui/widgets/ParametricEqWidget`** (NEW, ~3160 LOC h+cpp) — full
  Qt6 port of Thetis's `ucParametricEq.cs` (Richard Samphire MW0LGE,
  3396 LOC C# WinForms control). Five-batch port: skeleton + EqPoint /
  EqJsonState classes (B1), axis math + ordering + reset (B2),
  `paintEvent` + 10 draw helpers + 33 ms peak-hold bar-chart timer
  (B3), mouse + wheel + 6 Qt signals (B4), JSON marshal + 34-property
  public API (B5). All ports cite `ucParametricEq.cs [v2.10.3.13]`
  inline; GPLv2 + Samphire dual-license header preserved byte-for-byte.
  47 named test slots across 5 test files (4 + 10 + 7 + 10 + 16).

- **`src/core/ParaEqEnvelope`** (NEW, ~330 LOC h+cpp) — gzip + base64url
  envelope helper mirroring Thetis `Common.cs Compress_gzip` /
  `Decompress_gzip [v2.10.3.13]`. Byte-identical encode output with
  Thetis (verified via Python-fixture decode test). 9 test slots.

- **`src/models/TransmitModel`** — new `txEqParaEqData` field (mirror of
  existing `cfcParaEqData` pattern), with getter / setter / signal.

- **`src/core/MicProfileManager`** — new `TXParaEQData` bundle key
  wired into capture + apply paths (50 → 51 keys; 91 → 92 total bundle
  keys). 8 test slots in `tst_mic_profile_manager_para_eq_round_trip`.

- **`src/core/TxChannel::getCfcDisplayCompression`** (NEW) — WDSP
  wrapper for live CFC compression display data, used by `TxCfcDialog`'s
  50 ms bar-chart timer. 6 test slots.

- **`src/gui/applets/TxCfcDialog`** — full Thetis-verbatim rewrite.
  Drops the spartan profile combo (TxApplet hosts profile management).
  Embeds two cross-synced `ParametricEqWidget` instances (compression
  + post-EQ curves) with 50 ms live bar chart, 5/10/18-band radios,
  freq-range spinboxes, three checkboxes (`Use Q Factors` / `Live
  Update` / `Log scale`), two reset buttons, OG CFC Guide LinkLabel.
  Hide-on-close. 21 test slots (was 12).

- **`src/gui/applets/TxEqDialog`** — adds parametric panel behind
  `Legacy EQ` toggle (persists via AppSettings
  `TxEqDialog/UsingLegacyEQ`). Drops profile combo. Fixes legacy
  band-column slider/spinbox/header styling regression with
  `Style::sliderVStyle()` / `kSpinBoxStyle` / `kTextPrimary`. 11 test
  slots.

- **`CMakeLists`** — adds `find_package(ZLIB REQUIRED)` + `ZLIB::ZLIB`
  link.

- **Bench feedback (in-branch fixes before PR open):** dialog-level QSS
  block added to both `TxCfcDialog` and `TxEqDialog` constructors so the
  default Qt6 widgets (spinboxes / combos / radios / checkboxes /
  group-boxes / labels / buttons) pick up the project dark theme — the
  rewrites had been inheriting the system Qt6 default style and rendered
  dark-on-dark. `TxEqDialog`'s parametric Reset button was a no-op
  because it called `setBandCount(currentCount)` which early-returns at
  `ucParametricEq.cs:583 [v2.10.3.13]`; now synthesizes flat-default
  arrays inline (gain=0, q=4, evenly spaced freq) and calls
  `setPointsData` directly, mirroring `TxCfcDialog::onResetCompClicked`.

- **Total impact:** 9074 insertions / 1407 deletions across 31 files;
  102 new automated test slots; full ctest **280 / 280 green** at HEAD
  `e733fa6` (post-rebase onto current `main` at `f40f5a0`).

  Inline cite scan: 1739 cites validated against upstream sources
  (`verify-inline-tag-preservation.py`); 289 / 289 Thetis files pass
  header check; PROVENANCE registered for both `ParametricEqWidget`
  and `ParaEqEnvelope`. Closes the plan at
  [`docs/architecture/phase3m-3a-ii-followup-parametriceq-plan.md`](docs/architecture/phase3m-3a-ii-followup-parametriceq-plan.md).
  Verification matrix at
  [`docs/architecture/phase3m-3a-ii-followup-parametriceq-verification/README.md`](docs/architecture/phase3m-3a-ii-followup-parametriceq-verification/README.md).

### Fixed (hermes-filter-debug — HL2 N2ADR + S-ATT bench bugs)

Two HL2 bugs JJ caught while running on a Hermes Lite 2 the day after
the 3Q chrome layer landed. Both were silent (no warnings, no logs);
each had a non-obvious root cause that took source-first digging
through mi0bot to settle.

- **HL2 step-attenuator UI clamped at 0 dB despite the signed
  `-28..+32` range advertised in `BoardCapabilities`.** The
  `StepAttenuatorController` clamp at `setAttenuation` was correct,
  but three UI sites never received the negative bound:
  `RxApplet::setBoardCapabilities` only updated antenna-button
  visibility (not the spinbox range); `GeneralOptionsPage` re-ranged
  the RX1 / RX2 spinboxes from `m_ctrl->maxAttenuation()` but
  hardcoded `0` for the minimum; `RadioModel::connectToRadio` never
  pushed the connected board's `attenuator.{minDb, maxDb}` into the
  controller. Fix wires all three so HL2 sees `-28..+32`
  (mi0bot `setup.cs:16085-16086 [v2.10.3.13-beta2]`) end-to-end on
  every connect; legacy `0..31` boards unaffected.

- **N2ADR Filter board enable lost on app restart — and the Setup
  tab actively destroyed the persisted state.** Write side
  (`Hl2IoBoardTab::onN2adrToggled`) wrote to global
  `"hl2IoBoard/n2adrFilter"`; `HardwarePage::restoreSettings` read
  per-MAC `hardware/<mac>/hl2IoBoard/n2adrFilter`. Per-MAC was always
  empty, so restore defaulted `checked = false`, then unconditionally
  fired `onN2adrToggled(false)` — wiping the global key AND the
  OcMatrix every time the user opened Setup → HL2 I/O. Fix routes
  the write through `settingChanged()` → `HardwarePage::wire()` →
  `setHardwareValue(mac, ...)` (the path every other Hardware sub-tab
  uses). `RadioModel::connectToRadio` reads per-MAC.
  `restoreSettings` is no-op when the key is absent (a missing key
  means "no explicit user intent yet" — leave the matrix alone).
  One-shot migration `AppSettings::migrateLegacyN2adrFilter`
  (called from `main.cpp`) copies any pre-existing global value
  into `hardware/<mac>/...` for every saved HL2, then deletes the
  global; non-HL2 saved radios are skipped (`chkHERCULES` on a
  non-HERMESLITE radio is Apollo, not N2ADR — mi0bot
  `setup.cs:14369-14412 [@c26a8a4]`).

  **Why per-MAC and not mirror upstream global:** mi0bot's effective
  per-radio semantic comes from per-DB-file scoping
  (`database.cs:64 [@c26a8a4]`); multi-radio Thetis users swap files
  via `ImportDatabase` (`database.cs:11237 [@c26a8a4]`). NereusSDR
  achieves the same property via MAC scoping in one settings file —
  matches every other HL2 setting (OcMatrix, HL2 Options, Alex,
  calibration).

  22 new unit tests added: persistence round-trip + multi-MAC
  isolation + 5 migration cases + behavioural regression guard for
  the matrix-wipe-on-Setup-open scenario + controller signed range
  + RxApplet caps→spinbox propagation. **Codex review caught two
  follow-ups (PR #160) addressed in-branch: P1 — preserve the
  legacy global key when no HL2 saved radios exist yet so a future
  migration run can still pick it up; P2 — reset the N2ADR checkbox
  to false on missing-key restore so a re-used `Hl2IoBoardTab`
  instance doesn't show stale state from the previously selected
  radio.**

### Added (Phase 3M-3a-ii — TX Dynamics: CFC + CPDR + CESSB + Phase Rotator)

- **CFC (Continuous Frequency Compressor) — 10-band freq compander.** New
  `TxChannel` wrappers expose the WDSP CFC stage (`SetTXACFCOMPRun`,
  `Position`, `profile`, `Precomp`, `PeqRun`, `PrePeq`). The
  `cfcomp.c` source under `third_party/wdsp/` was surgically re-synced to
  Thetis v2.10.3.13 to pick up the 7-arg `SetTXACFCOMPprofile`
  (Qg/Qe gain-skirt and ceiling-Q arrays) added by MW0LGE/Samphire upstream;
  the original TAPR v1.29 5-arg signature was forward-compatible but lossy.
  GPL Samphire dual-license block + MW0LGE inline tags preserved verbatim;
  full attribution chain logged in `docs/attribution/WDSP-PROVENANCE.md` and
  `docs/attribution/REMEDIATION-LOG.md`.

- **CPDR (Compressor) — global enable + level dB.** TxChannel wrappers for
  `SetTXACompressorRun` / `SetTXACompressorGain`; level slider on
  PhoneCwApplet now maps `0..2` step → `0..20 dB` linear and drives the same
  `cpdrLevelDb` model property used by the new dialog and Setup page.

- **CESSB (Controlled-Envelope SSB) — overshoot envelope shaping.** TxChannel
  wrapper for `SetTXAosctrlRun`. Gating preserved: CESSB requires CPDR
  (`TXASetupBPFilters: bp2.run = compressor.run && osctrl.run`).

- **Phase Rotator (`phrot`).** Folded in from 3M-3a-i deferral — TxChannel
  wrappers for `SetTXAPhaseRotCorner` / `Nstages` / `Reverse`, fully
  controllable from the new CFC setup page.

- **TransmitModel — 15 new properties.** `phaseRotatorEnabled/Reverse/FreqHz/Stages`,
  `cfcEnabled/PostEqEnabled/PrecompDb/PostEqGainDb`, `cfcEqFreq[10]` /
  `cfcCompression[10]` / `cfcPostEqBandGain[10]`, opaque `cfcParaEqData` blob
  (for the future ParametricEq widget round-trip), global `cpdrOn`,
  `cpdrLevelDb`, `cessbOn`. All wired through `RadioModel::connectToRadio`
  with `pushTxProcessingChain` resync on connect / profile activation.

- **MicProfileManager — bundles +41 keys (was 50 → 91).** 19 of 21 factory
  profiles ported byte-for-byte from `database.cs:9282-9418 [v2.10.3.13]`
  with **155 verbatim overrides** (incl. AM 10k's unique `PhRotStages=9`
  override at `database.cs:9314`). Live capture/apply paths handle the new
  keys so [Save] in `TxCfcDialog` round-trips.

- **CfcSetupPage rewrite.** Setup → Transmit → CFC now exposes Phase Rotator
  group (Enable / Reverse / Corner / Stages), CFC group (Enable / Post-EQ /
  Precomp / Post-EQ Gain / Open Dialog button), and CESSB group
  (Enable / status text). The "Open Dialog" button raises `TxCfcDialog`
  modeless via the cross-thread `cfcDialogRequested` signal routed through
  `MainWindow::wireSetupDialog()`.

- **TxCfcDialog (modeless).** Profile combo + Save / SaveAs / Delete / Reset
  + Precomp / PostEQ-Gain global spinboxes + 10×3 per-band F / COMP / POST-EQ
  spinbox grid. Landed scalar-complete but visually spartan — full
  Thetis-faithful `ucParametricEq` widget port (3396-line C# WinForms
  UserControl, used by both CFC and EQ dialogs in Thetis) is queued as a
  separate sub-PR. Design hand-off doc:
  [`docs/architecture/phase3m-3a-ii-cfc-eq-parametriceq-handoff.md`](docs/architecture/phase3m-3a-ii-cfc-eq-parametriceq-handoff.md).

- **PhoneCwApplet PROC integration.** PROC button/slider now drives `cpdrOn`
  and `cpdrLevelDb`; numeric "X dB" label replaces the old NOR/DX/DX+ tick
  marks; NyiOverlay dropped. Duplicate `[PROC]` button on TxApplet (added
  during Batch 6 prep) was removed after JJ caught the redundancy — surfaced
  the feedback memory `feedback_survey_before_adding_controls.md` to keep
  future GUI batches from blindly adding controls without surveying.

- **SpeechProcessorPage live-status bindings.** Dashboard rows reflect
  CFC / CPDR / CESSB on/off + level + position in real time.

- **17 GPG-signed commits** stacked on 3M-3a-i; 7 new test executables
  (TxChannel CFC/CPDR/CESSB/PhRot setters; TM properties; profile round-trip;
  profile live-path; CfcSetupPage; TxCfcDialog; PhoneCwApplet PROC); 253/253
  ctest green. Verification matrix updated at
  `docs/architecture/phase3m-0-verification/README.md`.

- **Pre-emphasis** was de-scoped from this sub-PR to 3M-3b (FM-mode work) per
  master design §7.2 ("run as written").

### Added (Phase 3G RX Epic Sub-epic E — Waterfall rewind / scrollback)

- **Waterfall rewind / scrollback.** Drag up on the right-edge time-scale
  strip to pause the waterfall and scroll backward through up to ~8 minutes
  of history at default refresh, or 20+ minutes at any period ≥ 73 ms. A
  bright-red "LIVE" button appears when paused; one click returns to
  real-time. Tick labels switch from elapsed seconds (live) to absolute
  UTC timestamps (paused). Configurable depth via Setup → Display →
  Waterfall (60 s / 5 min / 15 min / 20 min, default 20 min, persisted as
  `DisplayWaterfallHistoryMs`).

  Ported from AetherSDR `main@0cd4559` for the live machinery and the
  unmerged PR `aetherclaude/issue-1478@2bb3b5c` (#1478) for the row cap
  and 250 ms resize debounce. NereusSDR raises the cap from upstream's
  4 096 rows to 16 384 rows (~128 MB at 2 000 px wide) so the default
  30 ms refresh delivers ~8 minutes of effective rewind instead of ~2.
  A disk-spool tier (to extend beyond the in-memory cap at the fastest
  refresh rates) is deferred to Phase 3M (Recording).

  Diverges from upstream by clearing history on band/zoom largeShift,
  keeping the rewind window coherent with the current band — see
  [`docs/architecture/phase3g-rx-epic-e-waterfall-scrollback-plan.md`](docs/architecture/phase3g-rx-epic-e-waterfall-scrollback-plan.md)
  §authoring-time #3.

  Closes the last item in the Phase 3G RX Experience Epic.

## [0.2.3] - 2026-04-24

This release rounds out the **Phase 3G RX experience epic** (AetherSDR-style
dBm scale strip, full Thetis Noise Blanker family port, cross-platform
7-filter Noise Reduction stack), brings **Alex antenna integration** to
feature-complete (3P-I-a + 3P-I-b — full `Alex.cs:310-413` composition,
RX-only antennas with SKU-driven labels, P1 bank0 / P2 Alex0 wire-locked),
and adds a **Linux PipeWire-native audio bridge** that supersedes the 0.2.2
pactl/PulseAudio fallback on PipeWire systems. v0.2.3 also addresses three
ANAN-G2 bench bugs caught after 0.2.2 shipped (P2 first-packet HPF/LPF
mismatch, band-crossing antenna-label desync, AlexController per-band
persistence).

### Added (Phase 3G RX Epic Sub-epic A — dBm scale strip)
- New AetherSDR-style **dBm scale strip** rendered on the right edge of the
  spectrum widget. Wheel inside the strip zooms the dynamic range live;
  arrow clicks at top/bottom nudge the floor or ceiling and persist back to
  `PanadapterModel`. Hover shows a cursor crosshair with the dBm value at
  the pointer. The strip honors the calibration offset, so on-screen labels
  reflect the actual signal level the user reads, not raw FFT bins. New
  `DisplayDbmScaleVisible` toggle in Setup → Display lets users hide the
  strip entirely. 17-row manual verification matrix at
  `docs/architecture/phase3g-rx-epic-a-verification.md`.
- Pure dBm-strip geometry helpers split out of the renderer with their own
  unit tests so layout changes are caught at build time.

### Added (Phase 3G RX Epic Sub-epic B — Noise Blanker family)
- Full port of Thetis's three-filter NB stack: **NB** (`nob.c`, Whitney),
  **NB2** (`nobII.c`, second-gen), and **SNB** (`snb.c`, spectral). New
  `NbFamily` wrapper on `RxChannel` owns the WDSP create/destroy lifecycle
  and tuning. The VFO Flag gets a cycling NB button (`Off → NB → NB2 → Off`)
  mirroring Thetis `chkNB` tri-state (`console.cs:43513-43560 [v2.10.3.13]`).
- RxApplet gains Threshold (0-100) + Lag (0-20 ms) sliders, scaled to match
  `setup.cs:8572, 16236 [v2.10.3.13]`. Setup → DSP → NB/SNB page is now
  interactive (previously greyed) and wires global defaults via AppSettings.
- `NbMode` + full `NbTuning` struct persist per-slice-per-band under
  `Slice<N>/Band<key>/Nb{Mode,Threshold,TauMs,LeadMs,LagMs}`; `SnbEnabled`
  session-level. Defaults pinned to Thetis `cmaster.c:43-68 [v2.10.3.13]`
  byte-for-byte (`nbThreshold=30.0`, times=0.1 ms, `backtau=0.05 s`,
  `nb2MaxImpMs=25.0`).
- New tests: `tst_nb_family`, `tst_slice_nb_persistence`, plus a rewritten
  `tst_rxchannel_nb2_polish` cover mode cycling, default parity, and
  per-band round-trip.

### Added (Phase 3G RX Epic Sub-epic C-1 — 7-filter NR stack)
- Full noise-reduction stack on the VFO flag DSP grid: **NR1** (ANR, WDSP),
  **NR2** (EMNR including post2 cascade), **NR3** (RNNR/rnnoise with
  user-loadable .bin models), **NR4** (SBNR/libspecbleach), **DFNR**
  (DeepFilterNet3 — cross-platform neural NR), **MNR** (Apple Accelerate
  MMSE-Wiener — macOS-only native), and **ANF**. BNR (NVIDIA Broadcast) is
  intentionally deferred (gRPC/NIM transport out of scope; tracked for
  follow-up).
- Setup → DSP → NR/ANF page with sub-tabs for NR1, NR2, NR3, NR4, DFNR,
  MNR, and ANF. NR1-NR4 sub-tabs mirror the Thetis `setup.designer.cs`
  layout byte-for-byte. The MNR sub-tab gains all 6 tuning knobs (Strength /
  Aggressiveness / Floor / Alpha / Bias / Gsmooth) with factory-default
  Reset buttons. The DFNR sub-tab uses AetherSDR-verbatim defaults
  (AttenLimit 100 dB, PostFilterBeta 0).
- Right-clicking any NR button on the VFO flag opens a `DspParamPopup` (port
  of AetherSDR's pattern) with 3-6 quick-control sliders plus a "More
  Settings…" entry point that deep-links into the matching Setup sub-tab.
- Per-slice NR independence via `SliceModel::setActiveNr` mutual exclusion
  (only one of NR1/NR2/NR3/NR4/DFNR/MNR active per slice at a time; ANF is
  independent and can stack).
- Vendored Xiph rnnoise (BSD-3) and libspecbleach (LGPL-2.1) into the WDSP
  build tree. Ported Thetis `rnnr.c` + `sbnr.c` (GPLv2+ + MW0LGE
  dual-license) into `third_party/wdsp/src/`.
- Bundled `Default_large.bin` (3.4 MB) and `Default_small.bin` (1.5 MB)
  rnnoise models plus `DeepFilterNet3_onnx.tar.gz` (7.6 MB) into every
  release artifact (Linux AppImage × 2 archs / macOS DMG / Windows ZIP +
  NSIS).
- New `ModelPaths` helper (`src/core/ModelPaths.{h,cpp}`) resolves rnnoise
  + DFNR model paths per platform install layout (Resources/ on macOS,
  share/NereusSDR/ on Linux, adjacent on Windows, plus dev-build fallbacks).
- New `setup-deepfilter.{sh,ps1}` scripts at the repo root build the
  DeepFilterNet3 Rust libdf for local development. CI workflows install
  Rust + invoke the appropriate script before `cmake -B build` so release
  artifacts always carry the runtime library.
- The 4×2 DSP toggle grid on the VFO Flag was rewritten as a 3×4 grid that
  mirrors AetherSDR's pattern exactly: NR mutex (NR1-4 + DFNR + MNR
  alternating off the same row) + NB mutex + ANF/SNB independent toggles.
  Buttons are 63×26 and fully fill the flag width.

### Added (Phase 3G RX Epic — band-button auto-mode, #118)
- New `RadioModel::onBandButtonClicked` handler funnels every band-button
  entry point (VFO Flag, RxApplet bands row, container `BandButtonItem`,
  spectrum overlay Band flyout) through one routing path. Closes #118.
- New `BandDefaults` seed table provides the per-band default mode + filter
  preset on the user's first visit to each band; subsequent visits load
  whatever the user last set there.
- `SliceModel::hasSettingsFor(Band)` probe + `Band::bandFromName` helper
  back the seed-vs-restore decision and round-trip with case-sensitivity
  pinning tests.

### Added (Phase 3P-I-a — Alex antenna integration core, #116)
- `AlexController`'s per-band antenna state now reaches the radio via
  `RadioConnection::setAntennaRouting`. Three triggers fire the pump:
  `AlexController::antennaChanged`, `PanadapterModel::bandChanged`, and
  `onConnectionStateChanged(Connected)`. All 5 writeable antenna surfaces
  (VFO Flag, RxApplet, Setup-grid, SpectrumOverlayPanel combos,
  AntennaButtonItem) funnel through `AlexController` as the single source
  of truth; `SliceModel` caches from the controller via
  `refreshAntennasFromAlex` so VFO/applet labels stay coherent on band
  changes. **Closes [#98](https://github.com/boydsoftprez/NereusSDR/issues/98).**
- `BoardCapabilities` gains `hasAlex2`, `hasRxBypassRelay`, and
  `rxOnlyAntennaCount` fields with cites to Thetis `setup.cs:6228` and
  `HPSDR/Alex.cs:377` (`//DH1KLM` / `//G8NJJ` author tags preserved
  verbatim).
- New `AntennaLabels` helper (`src/core/AntennaLabels.{h,cpp}`) is the
  single source for the ANT1/ANT2/ANT3 label list; returns empty on boards
  without Alex so UI sites can `setVisible(!empty)`. Replaces ~10
  hardcoded `QStringList{"ANT1","ANT2","ANT3"}` sites.
- New `PopupMenuStyle.h` defines the universal `kPopupMenu` dark-palette
  `QMenu` stylesheet. Every antenna popup (VFO Flag + RxApplet) now
  applies it, fixing Ubuntu 25.10 GNOME dark-on-dark menu rendering.
- `RadioConnection::setAntennaRouting(AntennaRouting)` pure-virtual
  replaces the deprecated `setAntenna(int)`. `AntennaRouting` carries
  RX/TX antenna numbers + `caps.hasAlex` so the protocol layer can zero
  the antenna bits on HL2/Atlas. Both P1 and P2 implementations updated
  with byte-for-byte wire-lock tests.
- VFO Flag, RxApplet, and SpectrumOverlayPanel antenna UI hidden on
  HL2 / Atlas (`!caps.hasAlex || antennaInputCount < 3`); these SKUs
  previously saw zombie ANT2/ANT3 buttons that wrote nothing. Matches
  Thetis's behavior of hiding antenna controls for boards with no
  Alex relay. `AntennaButtonItem` (meter) click is a silent no-op on
  `!caps.hasAlex`.
- New manual verification matrix at
  [`docs/architecture/antenna-routing-verification.md`](docs/architecture/antenna-routing-verification.md)
  covers per-SKU VFO Flag / RxApplet / Setup grid / spectrum overlay /
  AntennaButtonItem / band-change reapply / pcap verification.
- 17 new tests: `tst_antenna_routing_model` (4), `tst_alex_controller`
  (+4), `tst_ui_capability_gating` (5), `tst_popup_style_coverage` (1),
  plus expanded byte-lock cases on `tst_p1_codec_standard` and
  `tst_p2_codec_orionmkii`.

### Added (Phase 3P-I-b — RX-only antennas + SKU labels + XVTR, #117)
- New `SkuUiProfile` (`src/core/SkuUiProfile.{h,cpp}`) — per-`HPSDRModel`
  UI overlay describing RX-only labels + checkbox visibility. 14-case
  switch ports Thetis `setup.cs:19832-20375` exactly: Hermes/ANAN10 →
  "RX1/RX2/XVTR", ANAN100-class → "EXT2/EXT1/XVTR", 7000D/G2/etc. →
  "BYPS/EXT1/XVTR". Pure UI overlay; doesn't touch the wire.
- `AlexController` gains 6 flags (`rxOutOnTx` / `ext1OutOnTx` /
  `ext2OutOnTx` mutual-exclusion trio + `rxOutOverride` +
  `useTxAntForRx` + `xvtrActive`), ported from Thetis `Alex.cs:61-66`
  static fields. 5 persisted per-MAC; `xvtrActive` session-scoped.
  Mutual-exclusion matches Thetis `setup.cs:15420-16505` handlers.
- P1 bank0 C3 bits 5-7 now encode `AntennaRouting.rxOnlyAnt` + `rxOut`,
  byte-locked against Thetis `networkproto1.c:453-468` +
  `netInterface.c:479-481`. Both `P1CodecStandard` and `P1CodecHl2` (which
  has its own bank0, not inherited) updated.
- P2 Alex0 bits **8-11** (not 27-30 as the original plan said — bit 27 is
  `_TR_Relay`; corrected against authoritative Thetis `network.h:263-307`)
  encode rxOnlyAnt (bits 8/9/10 for XVTR_Rx_In / Rx_2_In / Rx_1_In) and
  rxOut (bit 11 = K36 RL17 RX-Bypass-Out relay).
- `RadioModel::applyAlexAntennaForBand(Band, bool isTx=false)` now ports
  the full Thetis `Alex.cs:310-413 UpdateAlexAntSelection` composition
  (minus MOX coupling + Aries clamp, both deferred to Phase 3M-1): isTx
  branch with Ext1/Ext2OnTx mapping, xvtrActive gating (derived from
  `band == Band::XVTR`), rx_out_override clamp. 6 new signal triggers
  wire flag changes to reapply composition.
- Setup → Antenna Control tab gains 5 new TX-bypass checkboxes (RX
  Bypass on TX, Ext 1 on TX, Ext 2 on TX, Disable RX Bypass relay,
  Use TX antenna for RX). SKU-driven visibility + per-SKU Ext2-on-TX
  tooltip variants. RX-only column sub-headers retargeted per SKU.
- Setup → Antenna → Alex-2 Filters sub-tab now gates on `caps.hasAlex2`
  (replaces the 3P-F hardcoded board check + hides the tab outright on
  non-BPF2 boards instead of leaving it gray).
- The VFO Flag gains an optional grey **BYPS** 3rd button between blue RX
  and red TX antenna buttons, double-gated on `caps.hasRxBypassRelay &&
  SkuUiProfile.hasRxOutOnTx`. Toggles `AlexController::rxOutOnTx` with
  bidirectional sync to the Setup checkbox.
- `AntennaButtonItem` meter gains `setHpsdrSku(HPSDRModel)` — button
  indices 3-5 (Thetis "Aux1/Aux2/XVTR" slots) now show SKU-specific
  labels from `SkuUiProfile.rxOnlyLabels`.
- 32 new tests across `tst_sku_ui_profile`, `tst_antenna_labels`,
  `tst_alex_controller` (flag mutual-exclusion), the codec byte-lock
  suites, and `tst_antenna_routing_model` integration cases.
- Verification doc: appended §7 per-SKU matrix (RX-only / XVTR /
  Ext-on-TX) + §8 authoritative P1 bank0 C3 and P2 Alex0 bit-layout
  reference.

### Added (Phase 3O — Linux PipeWire-native audio bridge)
- Native PipeWire audio bridge on Linux with live latency telemetry,
  separate sidetone / MON sinks, and per-slice output routing. Falls
  back to pactl / PulseAudio where PipeWire isn't present.
- New `Setup → Audio → Output` page (Primary, Sidetone, Monitor cards
  + per-slice routing section). Real PipeWire terminology throughout:
  `quantum`, `node.name`, `media.role`, `xruns`, `process-cb CPU`,
  `stream state` (no cosmetic relabeling).
- Setup → Audio → VAX page rebuilt: VAX is now a virtual source the
  system sees, not a device the user picks. Per-channel Rename + Copy
  node-name buttons.
- New `AudioBackendStrip` header on every Setup → Audio sub-page —
  shows the detected backend (PipeWire / Pactl / None) and Rescan +
  Open-logs buttons.
- `Help → Diagnose audio backend…` menu item opens the same
  detection-state dialog used at first launch when no audio backend
  is detected. The first-launch `VaxLinuxFirstRunDialog` walks the
  user through installing PipeWire / Pactl with copy-pasteable
  package commands.
- New optional build dependency on Linux: `libpipewire-0.3-dev`
  ≥ 0.3.50 (auto-detected via `pkg-config`). Without it the build
  still succeeds and the legacy LinuxPipeBus FIFO path is used.
- Lock-free SPSC byte ring (`AudioRingSpsc`) for cross-thread audio
  delivery, plus an RAII `PipeWireThreadLoop` wrapper over
  `pw_thread_loop` and a `QAudioSinkAdapter` that lets the unified
  `IAudioBus` interface front the QAudioSink fallback path.
- Manual verification matrix:
  `docs/architecture/linux-audio-verification/README.md`.

### Added — UI / theme polish
- App-wide dark `QPalette` + baseline QSS bootstrap installed at
  startup so every window — not just the spectrum / VFO chrome —
  renders against the correct palette on Linux. Fixes the gray-on-gray
  Setup dialog that some Ubuntu users saw on first launch.
- Setup → DSP → NR/ANF page: spinboxes replaced with sliders per the
  user-facing directive that NR controls should feel continuous.
  DFNR + MNR sliders use the shared `addSliderRow` /
  `addDoubleSliderRow` helpers so all NR sub-tabs render identically.

### Build
- Rust + cargo are now required for DFNR builds. CI installs
  `dtolnay/rust-toolchain@stable`. Local devs run
  `./setup-deepfilter.sh` (or `./setup-deepfilter.ps1` on Windows)
  before `cmake -B build`. `ENABLE_DFNR` auto-OFFs if the libdf is
  absent at configure time, so the build still succeeds without Rust
  — just without DFNR.
- `qt6-shadertools-dev` added to the Linux build prerequisites for
  Ubuntu 25.10 (Qt 6.8).
- CI installs ALSA + JACK + PipeWire dev headers on Linux so
  PortAudio's ALSA / JACK backends are compiled in (`PA_USE_ALSA` +
  `PA_USE_JACK` forced on).
- `NEREUS_HAVE_PIPEWIRE` is now propagated to `NereusSDRObjs` consumers
  so unit tests under `build/tests/` see the same compile-time gates as
  the app.
- Release artifacts grow ~12 MB per platform (DFNet3 model + rnnoise
  large/small models bundled).

### Fixed
- **Initial `CmdHighPriority` packet on P2 connect sent a DDC frequency
  that didn't match the HPF/LPF bits.** `P2RadioConnection::connectToRadio`
  unconditionally reset `m_rx[2].frequency` to 3865000 (80m LSB) after
  `RadioModel` had queued `setReceiverFrequency` with the persisted VFO.
  Worker-thread FIFO order made the hardcoded seed overwrite the real
  value, so the first packet told the radio to tune DDC2 to 80m while
  enabling 13 MHz HPF. Audio stayed silent until the user moved the
  panadapter. Fixed by only seeding the default when
  `m_rx[2].frequency == 0`. Caught on ANAN-G2 (Saturn) bench testing
  by KG4VCF.
- **Band-crossing reapplied the wire antenna but kept the old UI label.**
  `SliceModel::frequencyChanged` → `applyAlexAntennaForBand` was missing
  the `refreshAntennasFromAlex` call that the click-driven path had —
  so the relay switched but the VFO Flag / RxApplet buttons showed the
  previous band's antenna. Fixed + regression test
  `band_crossing_refreshes_slice_labels`.
- **`AlexController` per-band antenna selection didn't persist across
  app restart.** `AlexController::save()` had zero production call
  sites — only tests invoked it. User would pick ANT2 on 20m, quit,
  relaunch, and see ANT1 again. Hooked `antennaChanged` +
  `blockTxChanged` into the existing `scheduleSettingsSave()` coalescer
  via an `m_alexControllerDirty` flag so the 14-per-band emit burst
  during `load()` collapses to a single write. Also flushes on
  `teardownConnection()`.
- **SpectrumOverlayPanel RX Ant / TX Ant combos in the ANT flyout now
  actually change the antenna.** Previously the combos rendered but
  had no `currentTextChanged` handler — zombie controls. Wired through
  slice 0 via the same pattern as the VAX Ch combo with
  `m_updatingFromModel` echo guard.
- **Latent 3P-I-a bug: `AlexController::setRxOnlyAnt(band, 0)` was
  clamped to 1** by the shared `clampAnt(v)` helper, but Thetis
  `Alex.cs:58` uses 0 as "none selected" (required by the RX
  composition logic). New `clampRxOnlyAnt(0..3)` helper allows the 0
  state; constructor default changed from 1 to 0; `load()` defaults +
  clamp updated. 3P-I-b's full composition now works as Thetis intended.
- **Latent 3P-F bug: `setAntennasTo1` no longer touches rxOnlyAnt**
  (Thetis `Alex.cs:72-77` is explicit: "the various RX bypass
  unaffected"). 3P-F wrote all 3 arrays to 1; combined with 3P-I-b's
  new 0 semantic this would have silently activated the RX-bypass
  relay in external-ATU compat mode.
- **MNR's MMSE prior-SNR computation had a dimensional bug** in the
  post-filter application (caught while wiring the 6-knob tuning
  surface). Fixed in `src/core/MnrFilter.cpp`. Worth flagging upstream
  to AetherSDR — maintainers may want to backport.
- **`VfoWidget` removed raw `delete` calls** that bypassed Qt parent
  ownership. Closes #113.
- **AudioEngine fallback now honors the requested `deviceName`** and
  falls back through the `QMediaDevices` enumeration in order, instead
  of jumping straight to the system default. Closes #112.
- Three antenna/BPF bench bugs caught on ANAN-G2 during 3P-I-a
  shakedown (double-connect post-sweep, container `BandButtonItem`
  click routing, lock-state short-circuit on `onBandButtonClicked`).
- Several PipeWire shakedown bugs surfaced during Ubuntu 25.10 testing:
  non-blocking ring write on the PW data thread (was blocking and
  triggering Mutter's RT-thread SIGKILL); RT-thread `qCInfo` removed
  from `probeSchedOnce` (same Mutter SIGKILL path); null-guard on
  `on_process` buffer pointers (PipeWire PAUSED-state crash); a
  pre-existing SEGFAULT in `stop()` that skipped bus teardown when not
  running.
- Restored `//G8NJJ` author tag in the VfoWidget port from
  `setup.cs:6277` and preserved `//G8NJJ //MW0LGE //N1GP //DH1KLM`
  tags across the 3P-I-b ports — caught by the
  `verify-inline-tag-preservation.py` corpus drift gate.

### Changed
- `RadioModel` pushes the active slice's NR tuning state to its
  `RxChannel` whenever any of the 7 NR knobs changes (Task 19) — keeps
  WDSP/DFNR/MNR in sync with the model without requiring callers to
  reach into `RxChannel` directly.
- `RxChannel` consumes the `NbFamily` facade rather than juggling
  WDSP NB lifecycle directly. `WdspEngine` is no longer involved in
  per-channel NB create/destroy (it never should have been —
  duplicated lifecycle was a refactor leftover from 3-UI).

### Deprecated
- `RadioConnection::setAntenna(int)` — use
  `setAntennaRouting(AntennaRouting)`. Kept as a thin wrapper for one
  release cycle; scheduled for removal in 0.3.x.

### Internal
- New attribution corpus refresh (`thetis-author-tags.json` regenerated
  from upstream walk) + the standard `verify-inline-cites.py` baseline
  regression gate, `compliance-inventory.py --fail-on-unclassified`,
  and the cross-platform CI workflows that now build PipeWire +
  DeepFilterNet on Linux/macOS/Windows.

## [Unreleased]

## [0.2.2] - 2026-04-22

### Added
- New `--profile <name>` / `-p <name>` CLI flag for running multiple NereusSDR instances against different radios concurrently. Each profile gets its own isolated `NereusSDR.settings` + log directory under `profiles/<name>/`; profile names are validated against `[A-Za-z0-9_-]+` to prevent path traversal. FFTW wisdom stays machine-scoped (shared across profiles). Main window title gains a `[<profile>]` suffix. `SupportDialog` / `SupportBundle` now read the same profile-scoped log directory `main.cpp` writes to. Without `--profile`, behavior is byte-for-byte unchanged. Closes #100.
- New **Alex-1 Filters** per-row live-LED column mirroring Alex-2 — lights the matched HPF + LPF band for the current RX VFO. Ports Thetis `console.cs:setAlexHPF` / `setAlexLPF` first-match range logic including master-bypass and per-row bypass fallback. (Phase 3P-H)
- **ADC Overload** status-bar label on MainWindow (left of STATION). Text format matches Thetis verbatim: `"ADCi Overload"` with three-space separator, trimmed. Yellow at levels 1-3, red at level > 3, 2 s single-shot auto-hide timer restarts on each event (matches `ucInfoBar._warningTimer`). RxApplet's per-ADC OVL badges removed from the visible layout — the status-bar indicator is now authoritative. Ports `console.cs:21323, 21359-21389` + `ucInfoBar.cs:911-933`. (Phase 3P-H)
- New **attribution enforcement pipeline**: `scripts/discover-thetis-author-tags.py` walks upstream Thetis + mi0bot source, mechanically builds `docs/attribution/thetis-author-tags.json` (19 contributors discovered, with human-curated name/role fields). `scripts/verify-inline-tag-preservation.py` reads the corpus and fails commits that drop any developer-attribution tag (e.g. `//DH1KLM`, `//MW0LGE`) within ±10 port lines of a cite. `scripts/generate-contributor-indexes.py` regenerates `thetis-contributor-index.md` + `thetis-inline-mods-index.md` from the corpus + upstream walk — indexes now cover **151 upstream files, 2947 inline markers** (up from 30 files / 1458 markers). Old hand-curated indexes preserved as `-v020-snapshot.md`. CI runs `--drift` check so new upstream contributors block the PR until named. Pre-commit hook enforces strict mode locally. (Phase 3P-H attribution infra)
- **Attribution sweep commits** restore 74 historical `//DH1KLM` / `//MW0LGE` / `//G8NJJ` / `//W2PA` tags across 22 files (src + tests) that prior porting work had silently dropped. (Phase 3P-H sweep)
- New **Diagnostics → Radio Status** dashboard consolidating Thetis's piecemeal readouts (Front Console, PA Settings, main meter) into a single 5-card layout: PA Status (temp/current/voltage with bar meters), Forward/Reflected/SWR, PTT Source (pill row for MOX/VOX/CAT/Mic/CW/Tune/2-Tone + 8-event history), Connection Quality summary, Settings Hygiene actions. (Phase 3P-H)
- New **Diagnostics → Connection Quality**, **Settings Validation**, **Export / Import**, **Logs** sibling sub-tabs. Connection Quality shows live EP6/EP2 byte rates + throttle + sequence-gap counter from `HermesLiteBandwidthMonitor`. Settings Validation lists `SettingsHygiene::issues()` with per-row Reset / Forget / Re-validate; auto-refreshes on `issuesChanged()`. Export / Import round-trips the full AppSettings XML via QFile + QFileDialog. (Phase 3P-H)
- `RadioStatus` aggregator (`src/core/RadioStatus.{h,cpp}`) — PA temperature/current/forward-reflected/SWR/active PttSource + 8-event PTT history. Variant `multi-source`; data shapes from Thetis `console.cs` status handlers `[@501e3f5]`. Owned by `RadioModel`. (Phase 3P-H)
- `SettingsHygiene` (`src/core/SettingsHygiene.{h,cpp}`) — NereusSDR-original. Validates AppSettings against `BoardCapabilities` on connect and emits per-MAC mismatch records (severity Critical / Warning / Info). Powers the dashboard Reset / Forget actions. (Phase 3P-H)
- `PttSource` enum (`src/core/PttSource.h`) — NereusSDR-original. Tracks the trigger that asserted MOX so the Radio Status dashboard highlights the active source. (Phase 3P-H)
- `RadioModel` wires PA telemetry from both protocols into `RadioStatus` and runs `SettingsHygiene::validate()` on every `Connected` transition. P1 extracts PA fwd/rev/exciter/userADC0/userADC1/supply raws from `parseEp6Frame`'s C0 telemetry cases 0x08/0x10/0x18; P2 reads the same six fields from `processHighPriorityStatus` at documented offsets. Thetis per-board scaling (`computeAlexFwdPower` / `computeRefPower` / `convertToVolts` / `convertToAmps` `[@501e3f5]`) applied verbatim. (Phase 3P-H)
- **Live LED wire-up across earlier-phase pages**: Alex-2 Filters HPF + LPF LED rows highlight the active filter for the current RX frequency (mirrors Thetis `setAlex2HPF` / `setAlex2LPF` first-match ranges); OC Outputs pin-state LED row reflects `OcMatrix::maskFor(band, isTx)` and flips on MOX; HL2 I/O register state table polls `IoBoardHl2::registerValue(idx)` at 40 ms + bandwidth meter lit from `HermesLiteBandwidthMonitor`. (Phase 3P-H)
- Tests: `tst_ptt_source` (10), `tst_radio_status` (18), `tst_settings_hygiene` (11), `tst_p1_status_telemetry` (4), `tst_p2_status_telemetry` (3), `tst_radio_model_status_wiring` (4), `tst_alex2_live_leds` (5), `tst_oc_outputs_live_pins` (5), `tst_hl2_live_polling` (4). (Phase 3P-H)
- New **Hardware → Calibration** Setup page (renamed from PA Calibration). Hosts 5 Thetis-1:1 group boxes: **Freq Cal** (frequency spinbox + Start button + accuracy helptext), **Level Cal** (reference freq/level + Rx1/Rx2 6m LNA offsets + Start/Reset), **HPSDR Freq Cal Diagnostic** (correction factor with 9 decimal places + Using external 10 MHz ref toggle + 10 MHz factor), **TX Display Cal** (dB offset), and the existing **PA Current (A) calculation** group (preserved exactly). Per-MAC persistence under `hardware/<mac>/cal/`. (Phase 3P-G)
- `CalibrationController` model holds frequency correction factor (default 1.0), separate 10 MHz factor, using10MHzRef toggle, level offset, Rx1/Rx2 6m LNA offsets, TX display offset, PA current sensitivity/offset. `effectiveFreqCorrectionFactor()` returns factor10M when using10MHzRef, else factor. Per-MAC persistence. Ports `setup.cs:5137-5144; 14036-14050; 22690-22706; 14325; 17243; 18315` + `console.cs:9764-9844; 21022-21086` `[@501e3f5]`. (Phase 3P-G)
- New **Hardware → HL2 I/O** Setup page replacing the Phase 3I empty placeholder. Diagnostic surface for HL2 owners: connection status (LED + I2C address + last-probe timestamp), N2ADR Filter enable, register state table (8 principal registers from the IoBoardHl2 33-register set), 12-step state machine visualizer (current step highlighted), I2C transaction log (monospace listing of recent enqueue/dequeue with timestamps), bandwidth monitor mini (EP6/EP2 byte-rate progress bars + LAN PHY throttle indicator). Auto-hides for non-HL2 boards. (Phase 3P-E)
- `IoBoardHl2` model — 33-register enum + I2C TLV circular queue (32 slots per `network.h:MAX_I2C_QUEUE`) + 12-step UpdateIOBoard state machine with human-readable step descriptors. Closes the long-deferred Phase 3I-T12 work. (Phase 3P-E)
- `HermesLiteBandwidthMonitor` — HL2 LAN PHY throttle detection layered on a faithful port of mi0bot's `bandwidth_monitor.{c,h}` two-pointer byte-rate compute. Tracks ep6 ingress + ep2 egress, flags throttle when ep6 stays silent for 3 consecutive ticks while ep2 has traffic. (Phase 3P-E)
- New **Hardware → OC Outputs** Setup page with HF + SWL sub-sub-tabs (SWL placeholder). Hosts the full OC Outputs surface modeled 1:1 on Thetis: master toggles (Penny Ext Control, N2ADR Filter, Allow hot switching, Reset OC defaults), per-band RX matrix (14 × 7), per-band TX matrix (14 × 7), TX Pin Action mapping (7 pins × 7 actions per `enums.cs:443-457` TXPinActions), USB BCD output config, External PA control, and live OC pin state LED stubs (Phase H wires them to ep6 status). (Phase 3P-D)
- New `OcMatrix` model (per-band × per-pin × per-mode bit storage + TX pin action mapping) backs the OC Outputs page. Per-MAC persistence under `hardware/<mac>/oc/{rx,tx}/<band>/pin<n>` and `.../actions/pin<n>/<action>`. Owned by `RadioModel`. (Phase 3P-D)
- **RxApplet preamp combo now populates per board** from `BoardCapabilities::preampItemsForBoard()` at construction time instead of hardcoded items. HL2 / OrionMKII / Saturn / Angelia-no-Alex show the 4-item anan100d set (Off / -10 / -20 / -30 dB); Alex-equipped boards (Hermes, Angelia, Orion) show the 7-item on_off+alex set; Atlas no-Alex shows the 2-item on_off set. On connect the combo was already repopulated (PR #34); this closes the pre-connect gap. Matches Thetis `SetComboPreampForHPSDR` (`console.cs:40755-40825 [@501e3f5]`). (Phase 3P-C)
- **Hermes Lite 2 preamp combo corrected to 4 items** (anan100d set: 0dB / -10dB / -20dB / -30dB). Previously returned a 1-item table ("0dB" only). HL2 is not in Thetis's `SetComboPreampForHPSDR` switch (postdates it), but its LNA supports the same 4-level control as the anan100d set per mi0bot HL2 LNA design `[@c26a8a4]`. (Phase 3P-C)
- New `tst_preamp_combo` (17 assertions) locks per-board combo contents 1:1 with Thetis and verifies the 7-mode `PreampMode` enum has correct integer indices (0=Off, 1=On, 2–6=Minus10–50). Also verifies `RxApplet.preampComboItemCountForTest()` reflects board caps at construction. (Phase 3P-C)
- Hardware → Antenna / ALEX page split into three sub-sub-tabs — Antenna Control (placeholder for Phase F), **Alex-1 Filters**, and **Alex-2 Filters** — matching Thetis's General → Alex IA. Alex-1 page exposes per-band HPF/LPF bypass + edge editors and the **Saturn BPF1 panel** (gated on ANAN-G2 / G2-1K). Alex-2 page exposes per-band HPF/LPF for the RX2 board with live-LED indicator stubs (Phase H wires them). Per-MAC persistence under `hardware/<mac>/alex/...` and `.../alex2/...`. (Phase 3P-B)
- ADC OVL badge in RxApplet now splits into OVL₀ + OVL₁ for dual-ADC boards (Orion-MKII family — boards with `BoardCapabilities::p2PreampPerAdc=true`). Single-ADC boards (HL2, Hermes, Angelia) keep a single badge. (Phase 3P-B)
- Per-ADC RX1 preamp toggle exposed in RxApplet for OrionMKII-family boards; routes to byte 1403 bit 1 in P2 CmdHighPriority via the new `P2RadioConnection::setRx1Preamp(bool)`. (Phase 3P-B)
- ANAN-G2 / G2-1K can now use user-configured Saturn BPF1 band edges instead of Alex defaults via the new Hardware → Antenna/ALEX → Alex-1 Filters page; codec layer (`P2CodecSaturn`) reads the configured edges from `CodecContext.p2SaturnBpfHpfBits`. (Phase 3P-B)
- New **Hardware → Antenna / ALEX → Antenna Control** sub-sub-tab — third sub-sub-tab alongside Phase B's Alex-1/Alex-2 Filters. Per-band antenna assignment grid (14 bands × TX/RX1/RX-only ports 1-3) with Block-TX safety toggles. Backed by AlexController model. (Phase 3P-F)
- `AlexController` — per-band TX/RX/RX-only antenna arrays + Block-TX safety + SetAntennasTo1 force mode. Per-MAC persistence. Replaces Phase 3I Alex stubs. Ports `HPSDR/Alex.cs:30-106`. (Phase 3P-F)
- `ApolloController` — Apollo PA + ATU + LPF accessory state model (present/filterEnabled/tunerEnabled). Per-MAC persistence. Capability-gated on `BoardCapabilities::hasApollo` (only HPSDR-kit per Thetis source). Ports `setup.cs:15566-15590`. (Phase 3P-F)
- `PennyLaneController` — Penny external control master enable (companion to Phase D's OcMatrix which holds the per-pin/per-band masks). Per-MAC persistence. Ports `Penny.cs` + `console.cs:14899`. (Phase 3P-F)
- `BoardCapabilities` extended with `hasApollo` / `hasAlex` / `hasPennyLane` per-board enable rules per Thetis `setup.cs:19834-20205` board-model if-ladder. Source-first correction caught: only HPSDR-kit enables Apollo (NOT all ANAN family — every ANAN board explicitly disables it in Thetis). (Phase 3P-F)
- **VAX audio routing subsystem** — full port of AetherSDR's virtual-cable audio bus architecture as a NereusSDR-native multi-platform stack. Replaces the single-device QAudioSink output with `IAudioBus` abstract interface backed by 3 platform implementations: `PortAudioBus` (render + TX capture + device enumeration), `CoreAudioHalBus` (macOS HAL plugin bridge), `LinuxPipeBus` (pipewire/pulse module-pipe-source × 4 + module-pipe-sink × 1 under `nereussdr-vax-*` namespace). Per-slice VAX channel routing with up to 4 output channels + 1 TX input channel. (Phase 3O)
- `IAudioBus` abstract interface (`src/audio/IAudioBus.h`) — unified render / capture / enumerate API across platform backends. `AudioEngine` now holds `IAudioBus` instances rather than `QAudioSink` directly; platform selection at startup via `Q_OS_*`. (Phase 3O Sub-Phase 1 + 4)
- `MasterMixer` (`src/audio/MasterMixer.{h,cpp}`) — per-slice mute / volume / pan accumulation feeding the bus render path. Sits between slice audio output and the bus; master-mute API added to `AudioEngine`. (Phase 3O Sub-Phase 2)
- `PortAudioBus` (`src/audio/PortAudioBus.{h,cpp}`) — PortAudio v19.7.0 vendored via CMake FetchContent. Windows audio backend (the one that owns virtual-cable driver interop) plus fallback on macOS/Linux. Render-only minimal scaffold → host-API + device enumeration → TX capture (input stream + `pull()`). (Phase 3O Sub-Phase 3)
- `LinuxPipeBus` (`src/audio/LinuxPipeBus.{h,cpp}`) — PulseAudio / PipeWire named-pipe bus. Loads `module-pipe-source × 4` (RX destinations) + `module-pipe-sink × 1` (TX source) at startup under a `nereussdr-vax-*` namespace; render writes raw PCM to the pipes, capture reads from the sink's monitor. Auto-unloads modules on shutdown. (Phase 3O Sub-Phase 6)
- **macOS VAX HAL plugin** (`hal-plugin/`) — full port of AetherSDR's `NereusSDRVAX.driver` CoreAudio HAL plugin. Exposes 4 virtual output devices (`NereusSDR VAX 1..4`) + 1 TX input device to the macOS audio system; app process communicates with the plugin via a shared-memory ring (`VaxShmBlock`) for low-latency sample handoff. `CoreAudioHalBus` wraps the plugin on the app side. Includes `packaging/macos/hal-installer.sh` that builds + signs + packages the driver into a `productbuild` `.pkg` with a postinstall restart of `coreaudiod` (with macOS 14.4+ `killall` fallback when `launchctl kickstart` returns EPERM). **Note:** the release pipeline cannot produce a redistributable notarized installer until Apple Developer ID credentials are in place, so **v0.2.2 does NOT attach the HAL plugin `.pkg` to the GitHub Release**. macOS users who want VAX routing today can self-sign locally — see [docs/debugging/alpha-tester-hl2-smoke-test.md](docs/debugging/alpha-tester-hl2-smoke-test.md) §"macOS notes" for step-by-step instructions. (Phase 3O Sub-Phase 5)
- `VirtualCableDetector` (`src/audio/VirtualCableDetector.{h,cpp}`) — Windows-focused auto-detect of VB-Audio Virtual Cable, VAC, Voicemeeter, Dante Virtual Soundcard, and FlexRadio DAX virtual-cable families. Enumerates PortAudio devices against a family-signature table; reports detected cable pairs with suggested channel bindings. `rescan()` + rescan-diff helpers feed the VAX first-run wizard. (Phase 3O Sub-Phase 7)
- `VaxChannelSelector` widget (`src/widgets/VaxChannelSelector.{h,cpp}`) — compact per-slice VAX channel button row (1-4 + Off) embedded in the VFO flag. `setSlice()` rebinds cleanly on slice reshuffle (e.g. RX2 enable/disable); prior bindings are disconnected before new ones attach. Tooltips explain each button's destination. (Phase 3O Sub-Phase 8)
- `VaxFirstRunDialog` (`src/gui/VaxFirstRunDialog.{h,cpp}`) — launched on MainWindow startup when no VAX configuration has been persisted. Platform-specific: Windows runs `VirtualCableDetector` and offers to pre-fill channel bindings from detected virtual-cable pairs; macOS prompts for HAL-plugin install if absent; Linux checks for PulseAudio/PipeWire presence. "Apply suggested" maps detected pairs onto the first unassigned VAX slots. (Phase 3O Sub-Phase 10)
- `MasterOutputWidget` (`src/widgets/MasterOutputWidget.{h,cpp}`) hosted in the new `TitleBar` strip at the top of the main window — global speaker-volume slider + master-mute button + right-click → device picker + scroll-wheel fine-tune. One source of truth for output routing across the app. (Phase 3O Sub-Phase 11)
- `TitleBar` widget — thin strip above the main menu that hosts the menu bar, `MasterOutputWidget`, and the 💡 feature-request button (moved from menu corner). Keeps top chrome clean at narrow window widths. (Phase 3O Sub-Phase 11)
- **Setup → Audio sub-tabs** — `AudioDevicesPage` (per-device driver API / buffer / format with live-reconfig), `AudioVaxPage` (4 channel strips with meter + gain + mute + device picker + Auto-detect QMenu wiring `VirtualCableDetector`), `AudioTciPage` (placeholder for Phase 3J TCI), `AudioAdvancedPage` (reset-all-audio-settings with confirmation dialog + IVAC feedback parity controls). Full Audio-nav refactor from the legacy single-page Setup → Audio. (Phase 3O Sub-Phase 12 Tasks 12.1–12.5)
- `VaxApplet` (`src/gui/applets/VaxApplet.{h,cpp}`) — container-applet port from AetherSDR with per-channel RX gain slider + mute button + TX gain slider + level meter for all 4 VAX slots. `MeterSlider` widget (ported from AetherSDR) drives the sliders. (Phase 3O)
- `SliceModel::vaxChannel` property (`VaxChannel` enum: `Off` / 1-4) — per-slice VAX routing persisted under `slice<N>/vaxChannel`. New `SliceModel::slicePrefix()` helper unifies settings-key composition for VAX + future slice-scoped fields. Clamped on load so invalid persisted values don't poison the engine. (Phase 3O)
- `TransmitModel::txOwnerSlot` + `VaxSlot` enum — TX arbitration field so only the slice that armed TX actually transmits; `vaxSlotFromString` / `vaxSlotToString` round-trip with explicit `MicDirect` arm. Prevents ambiguous multi-slice MOX. (Phase 3O)
- `AppSettings` VAX schema migration helper — `main.cpp` runs a one-shot migration from pre-3O `daxChannel` keys to `vaxChannel` keys (covers the Phase 3O DAX→VAX rebrand). Idempotent; subsequent launches are no-ops. (Phase 3O)
- `SpectrumOverlayPanel` VAX Ch combo now wires to slice 0 directly; changes propagate through `SliceModel::vaxChannel` (was: unwired stub). (Phase 3O)
- Tests: `MasterOutputWidgetSignalRefreshTest` (with 50 ms timing assertion), audio-engine IAudioBus refactor coverage, VirtualCableDetector rescan helpers, `VaxFirstRunDialog` guard tests, `vaxSlotFromString` round-trip, SliceModel `VaxChannel` clamp-on-load. (Phase 3O)
- **macOS alpha-tester guidance for code signing + VAX HAL plugin** — `docs/debugging/alpha-tester-hl2-smoke-test.md` now documents the v0.2.x signing state (ad-hoc codesign, no Developer ID, no notarization), the Gatekeeper right-click → Open workflow, the `xattr -dr com.apple.quarantine` fallback, and a step-by-step self-sign procedure for the VAX HAL plugin so testers can enable VAX routing on macOS today without the notarized `.pkg`.

### Fixed
- **Setup-dialog checkboxes and radio buttons now visible on the dark theme.** `SetupPage` base class applies `kCheckBoxStyle` + `kRadioButtonStyle` at the page root; previously system defaults rendered black-on-dark and were invisible. (Phase 3P-H)
- **Alex-1 / Alex-2 Filters live LED now tracks VFO through CTUN-mode edge crossings.** Tabs were subscribing to `PanadapterModel::centerFrequencyChanged` but in CTUN mode the pan centre stays put while the slice tunes, so edge crossings were missed. Switched to `SliceModel::frequencyChanged` on every current + future slice (handles post-connect `addSlice` events). Added 250 ms polling fallback as a signal-path belt-and-suspenders. (Phase 3P-H)
- **ADC Overload status-bar label no longer shifts STATION** when firing. Fixed 180 px width + 12 px gap + empty-text-when-idle holds layout stable. (Phase 3P-H)
- **Silent shutdown crash on Windows after HL2 session, plus downstream Thetis startup hang.** `RadioConnectionTeardown` posted the worker-thread disconnect via a plain queued `invokeMethod` and immediately called `quit()`; on Windows' event dispatcher the quit could race the posted event out of the loop, leaving metis-stop unsent, the UDP socket open, and the `P1RadioConnection` + its three timers orphaned on a dead thread. Later Qt teardown then tripped a fast-fail abort and left the HL2 Winsock endpoint in a state that blocked Thetis from binding port 51188 until `netsh winsock reset` / reboot. Helper now uses a `QSemaphore` + `tryAcquire(1, 3000ms)` to ensure `disconnect()` completes on the worker before quit while keeping a bounded shutdown latency if the worker is wedged; `P1RadioConnection::disconnect()` now also closes its UDP socket explicitly, matching P2. Closes #83.
- **Hermes Lite 2 bandpass filter now switches on band/VFO change.** P1RadioConnection was emitting `m_alexHpfBits=0`/`m_alexLpfBits=0` permanently because the filter bits were never recomputed from frequency. P2 had the right code; lifted into shared `src/core/codec/AlexFilterMap` and called from P1's `setReceiverFrequency`/`setTxFrequency`. (Phase 3P-A)
- **Hermes Lite 2 step attenuator now actually attenuates.** P1's bank 11 C4 was using ramdor's 5-bit mask + 0x20 enable for every board; HL2 needs mi0bot's 6-bit mask + 0x40 enable + MOX TX/RX branch. Fixed via per-board codec subclasses (`P1CodecStandard` for Hermes/Orion, `P1CodecHl2` for HL2). RxApplet S-ATT slider range now widens to 0-63 dB on HL2 from `BoardCapabilities::attenuator.maxDb`. (Phase 3P-A)
- **VFO frequency entry no longer rejects valid thousand-grouped input.** The prior parser treated `7,200` as two tokens and silently dropped the comma group; full rewrite now handles decimal / comma-grouped thousands / unit-suffix (`k`, `M`, `G`, `Hz`, `kHz`, `MHz`, `GHz`) inputs consistently, including the single-comma + unit + 3-digit tail case (`7,200kHz`) which is treated as thousands. Closes #73.
- **RX-applet STEP ↑/↓ arrows now actually step the tuning ladder.** The arrows were rendering but their clicks were unwired; both now advance/retreat through the tuning-step ladder, and **500 Hz was added** between 100 Hz and 1 kHz to match Thetis's default ladder. Closes #69.
- **Receivers no longer leak across disconnect/reconnect cycles on the same radio.** `ReceiverManager` is now `reset()` on disconnect so the DDC-to-receiver map is clean when a second connect attempt runs; previously reconnecting the same rig would accumulate phantom receivers and crash WDSP channel allocation on the second attempt. Closes #75.
- **HL2 I2C read responses now persist into the `IoBoardHl2` model.** The HL2 I/O Setup page's register table was reading a model whose register slots were never updated from ep6 I2C read-response frames; the parse path now writes the received byte into the model and emits a change signal so the page stays in sync with the radio's live register state.
- **`FreqCorrectionFactor` now actually shifts the radio.** The Phase 3P-G Calibration page wrote the factor into the model correctly, but `P2CodecOrionMkII::hzToPhaseWord` never consulted it — the UI changed and the phase word didn't. Moved the multiplication into the codec so the dial on the Calibration page takes effect on the next tune.
- **`OcMatrix` state guarded with `QReadWriteLock` for cross-thread access.** Main-thread writes from the OC Outputs page were racing connection-thread reads at C&C compose time on boards with busy OC traffic; per-mask read/write locks remove the race. Default state remains byte-identical.
- **RX1 preamp toggle queued onto the connection thread.** The per-ADC RX1 preamp combo was calling `P2RadioConnection::setRx1Preamp()` synchronously from the GUI thread — could race the codec's next C&C compose. Now invoked via queued connection so the codec sees a stable value at compose time.
- **P1 bank scheduler ceiling now reads `codec->maxBank()`** instead of a hardcoded constant, so HL2 and Anvelina Pro 3 stop clipping at bank 10 and correctly reach bank 11 (S-ATT) / bank 17 (Anvelina Pro 3 extended) during compose cycles.
- **HAL plugin shared-memory layout aligned across app + plugin** — `VaxShmBlock` struct was diverging in field order between `hal-plugin/` and `src/audio/`, causing garbled samples on macOS when the app wrote the plugin's read in-flight; now driven from a single shared header. Also: HAL plugin RX ring backlog clamped to prevent laps-reading garble when the app stalls. (Phase 3O Sub-Phase 5)
- **Speakers-mutex scope narrowed to the push call only** in `AudioEngine` — previously held across format re-negotiation during live-reconfig, producing audible pops when the user switched output devices mid-stream.
- **Saved speakers device is applied to the engine at startup** — on first-launch-with-saved-config the `AudioEngine` was initialised before the speakers device was re-read from AppSettings; startup path now pulls the saved device before engine init.
- **VAX selector now lives inside the VAX tab** (was rendering below the tab bar due to a layout-parent mixup); **VAX channel tags refresh on `sliceRemoved`** and **rebind on slice-0 reshuffle** (RX2 enable/disable cycles no longer orphan the VAX combo).
- **Apply-suggested in the VAX first-run dialog now maps onto the first unassigned slots** instead of overwriting slot 0 every time.
- **Windows CMake now auto-fetches `libfftw3-3.dll`** via a FetchContent fallback when the system package isn't present, fixing cold-build failures on fresh Windows setups.
- **About dialog credits g0orx/wdsp** for the POSIX portability shim that made WDSP cross-platform — missed attribution surfaced by the Phase 3P-H discovery-driven corpus refresh.

### Changed
- **P2RadioConnection** `setReceiverFrequency` / `setTxFrequency` Hz→phase-word conversion now multiplies by `CalibrationController::effectiveFreqCorrectionFactor()` (defaults to 1.0 → byte-identical to pre-cal). Per-MAC freq correction lets users compensate per-radio reference oscillator drift. Ports `NetworkIO.cs:227-254` `FreqCorrectionFactor` + `Freq2PhaseWord()` `[@501e3f5]`. (Phase 3P-G)
- PA Calibration setup tab renamed to **Calibration**; the existing PA Current (A) group preserved exactly. (Phase 3P-G)
- `P1CodecHl2` gains I2C intercept mode — when `IoBoardHl2`'s I2C queue has pending transactions, the next C&C frame's 5 bytes are overwritten with the I2C TLV payload (chip-address + control + register address + data) and the txn dequeued. Normal bank compose runs when queue is empty. Per mi0bot `networkproto1.c:898-943`. ep6 read path extended to parse I2C response frames (C0 bit 7 marker) back into IoBoardHl2 register state. (Phase 3P-E)
- `P1RadioConnection`'s ep6/ep2 packet sizes are now recorded into `HermesLiteBandwidthMonitor`; watchdog tick drives the periodic rate evaluation. The legacy sequence-gap fallback throttle heuristic remains as a safety net when the monitor isn't wired (test seam path). Closes long-open `TODO(3I-T12)` markers at `P1RadioConnection.cpp:892, 939, 1416-1462`. (Phase 3P-E)
- `RadioModel` now owns the per-connection `IoBoardHl2` and `HermesLiteBandwidthMonitor` instances and pushes them to `P1RadioConnection` at connect time, mirroring the OcMatrix ownership pattern from Phase 3P-D. (Phase 3P-E)
- `RxApplet` antenna buttons (Ant 1/2/3) now read from `AlexController::txAnt(currentBand)` / `rxAnt(currentBand)` and re-populate on band change. Click-to-select calls the controller setter, respecting Block-TX safety guards. Was: static placeholder buttons. (Phase 3P-F)
- `RadioModel` now owns `AlexController`, `ApolloController`, `PennyLaneController` instances and pushes MAC + load on connect, mirroring Phase D's OcMatrix and Phase E's IoBoardHl2 + HermesLiteBandwidthMonitor ownership patterns. (Phase 3P-F)
- `P1RadioConnection` and `P2RadioConnection` now source the OC byte at C&C compose time from `OcMatrix::maskFor(currentBand, mox)` (when the matrix is wired via `setOcMatrix()` — `RadioModel` pushes its `m_ocMatrix` to the connections at connect time) instead of the legacy `m_ocOutput` field. Falls through to legacy when the matrix is unset (test seams). Default state byte-identical: empty matrix → `maskFor()==0` matching legacy `m_ocOutput==0`. Regression-freeze gates (P1 + P2) still PASS byte-for-byte. (Phase 3P-D)
- `P1RadioConnection`'s C&C compose layer refactored into per-board codec subclasses (`P1CodecStandard`, `P1CodecHl2`, `P1CodecAnvelinaPro3`, `P1CodecRedPitaya`) behind a stable `IP1Codec` interface. Behavior byte-identical for every non-HL2 board (regression-frozen via `tst_p1_regression_freeze` against a pre-refactor JSON baseline). Set `NEREUS_USE_LEGACY_P1_CODEC=1` to revert to the pre-refactor compose path for one release cycle as a rollback hatch.
- `P2RadioConnection` now calls the shared `AlexFilterMap::computeHpf/Lpf` helpers instead of its own inline copies; byte output unchanged.
- `BoardCapabilities::Attenuator` extended with `mask`, `enableBit`, and `moxBranchesAtt` fields capturing per-board ATT byte encoding parameters.
- `P2RadioConnection`'s C&C compose layer refactored into per-board codec subclasses (`P2CodecOrionMkII` for the OrionMKII / 7000D / 8000D / AnvelinaPro3 family, `P2CodecSaturn` extending it for ANAN-G2 / G2-1K with the G8NJJ BPF1 override) behind the new `IP2Codec` interface. Behavior byte-identical to pre-refactor for all captured tuples (`tst_p2_regression_freeze` with 36 tuples). `NEREUS_USE_LEGACY_P2_CODEC=1` env var reverts to the pre-refactor compose path for one release cycle as a rollback hatch. (Phase 3P-B)
- `BoardCapabilities` extended with `p2SaturnBpf1Edges` (per-band start/end MHz, empty default) and `p2PreampPerAdc` (true for OrionMKII family). `AlexFilterMap` shared between P1 and P2 codecs (was Phase A; Phase B is the first P2 consumer). (Phase 3P-B)
- `AudioEngine` backend dispatch refactored — the engine holds `IAudioBus` instances rather than a raw `QAudioSink`; platform selection via `Q_OS_*` at startup. Default path (no VAX configuration) falls through to the Qt multimedia output, byte-identical to v0.2.1 audio output. (Phase 3O Sub-Phase 4 + 8.5)
- **DAX → VAX UI rebrand (app-wide).** All user-facing "DAX" labels, enum values, settings keys, and doc strings renamed to "VAX". `AppSettings` first-launch migration covers persisted pre-rename keys. Internal identifiers (`VaxSlot`, `VaxChannel`, `vaxChannel`) follow the new name. (Phase 3O)
- **TitleBar relocation**: the 💡 feature-request button moved from the menu-bar corner into the new `TitleBar` strip alongside `MasterOutputWidget` for a consistent top-chrome layout. (Phase 3O Sub-Phase 11)
- `tune(display)` shipped current spectrum / waterfall defaults (smooth-fall value + Clarity Blue palette) as the out-of-box defaults; earlier v0.2.x installs can reset under Setup → Display.

## [0.2.1] - 2026-04-19

### Features
- (none)

### Fixes
- fix(radio-model): stop recalling bandstack on VFO-tune crossings
- fix(radio-model): guard per-band save/restore lambdas against reentrancy

### Docs
- (none)

### CI / Build
- (none)

### Other
- (none)

### Tests
- (none)

### Refactors
- (none)


## [0.2.0] - 2026-04-19

### Auto AGC-T (PR #53)

- `NoiseFloorTracker` — lerp-based noise-floor estimator feeding the
  Auto-threshold timer with MOX guard and `agcCalOffset`.
- AUTO button on the AGC-T slider row (VfoWidget + RxApplet) toggles
  auto-mode; periodic NF update keeps the threshold tracking.
- Right-click on the AGC-T slider opens Setup directly on the AGC/ALC
  page; Setup page controls wired to SliceModel.
- Thetis-source tooltips on all four AGC controls + attenuator
  fast-attack.

### Sample-rate wiring (PR #35)

- Per-MAC persistence of hardware sample rate + active-RX count under
  `hardware/<mac>/radioInfo/`.
- Full Thetis-parity rate lists: P1 = 48/96/192 kHz (plus 384 on
  RedPitaya); P2 = 48/96/192/384/768/1536 kHz. Default 192 kHz.
- RX2 sample-rate combo stub (disabled, for Phase 3F).
- Inline reconnect banner on RadioInfoTab when the selected rate
  differs from the active wire rate.

### Fixes

- **RxDspWorker thread-safety:** buffer-size fields now atomic; audio
  thread no longer races the main-thread control path.
- **RxDspWorker accumulator drain** scaled to per-rate `in_size` so
  DDC sample-rate changes don't starve the WDSP feed.
- **Setup dialog** no longer overrides `selectPage()` in `showEvent`,
  so right-click-to-page works as intended.

## [0.1.7] - 2026-04-16

RedPitaya / Hermes P1 protocol fixes driven by pcap analysis in issue #38,
plus Windows D3D11 container lifecycle fixes from issue #42.

### Fixed
- **P1 RedPitaya / Hermes family (#38):** restore DDC3 NCO command on the wire.
  The `kRxC0Address` lookup table was missing the `0x0A` address entry,
  which caused bank 6 (RX4 / DDC3 frequency) to be dropped and bank 9
  (RX7 / DDC6) to alias onto bank 10's `0x12` TX-drive slot. Verified
  byte-for-byte against Thetis `networkproto1.c` cases 6 and 9.
- **P1 EP2 send cadence (#38):** host→radio packets are now paced by a
  dedicated 2 ms `Qt::PreciseTimer` with a `QElapsedTimer`-driven catch-up
  loop, yielding ~200-400 pps (target: Thetis' 380.95 pps from its 48 kHz
  audio clock). Previously we ran ~40 pps, starving the radio's audio DAC
  and stretching the 17-bank C&C round-robin to ~213 ms per cycle.
- **Windows container float/dock rendering** — five interlocking issues in
  the meter container lifecycle on Windows D3D11 QRhi (#42):
  - HWND collision on reparent — `E_ACCESSDENIED` →
    `DXGI_ERROR_DEVICE_REMOVED` cascade from the old `MeterWidget`
    lingering under the parent HWND during `setParent()`. Old widget now
    detaches synchronously (`hide()` + `setParent(nullptr)`) before
    `deleteLater`; `ContainerManager` swaps the meter around each reparent
    so no `WA_NativeWindow` child is reparented.
  - First float landed at `(0,0)` behind the main window —
    `FloatingContainer::ensureVisiblePosition()` centers the form on the
    anchor's screen when saved geometry is missing, at origin, or
    off every connected screen.
  - Use-after-free in `MeterPoller` — raw `MeterWidget*` targets dangled
    when the reparent-swap destroyed them. Switched to
    `QVector<QPointer<MeterWidget>>` with null-guarded `poll()`.
  - Progressive stack compression across reparent cycles —
    `inferStackFromGeometry` merged touching row intervals into one
    cluster, collapsing N bar rows onto stack slot 0. Require strict
    overlap > 0.002 before merging.
  - Empty band below the meter stack on resizable containers — Thetis's
    fixed `kNormalRowHNorm = 0.05` assumes fixed-aspect containers; stack
    now shares `(1 − bandTop)` equally among rows. 24 px floor preserved
    for small widgets.

### Known issues
- **ANAN MM preset** still shows empty space below the needle panel when
  no bar rows are added. Thetis-faithful fix (per-container `AutoHeight`)
  is scoped in
  [`docs/architecture/meter-autoheight-plan.md`](docs/architecture/meter-autoheight-plan.md).
- Exit-time segfault (exit 139) reproducible on close; root cause still
  unknown, not implicated by the #42 changes.
- One `QRhiWidget: No QRhi` warning per meter install cycle; benign,
  under investigation.

## [0.1.7-rc1] - 2026-04-16

Radio model selector and P1 protocol completion for RedPitaya and non-standard
Hermes devices.

### Added
- **Radio model selector** — per-MAC model override in ConnectionPanel detail panel;
  users can select their actual radio model (e.g. "Red Pitaya") when the discovery
  board byte is ambiguous (e.g. reports "Hermes")
- **HardwareProfile engine** — port of Thetis clsHardwareSpecific.cs; maps 16
  HPSDRModel variants to correct ADC count, BPF, supply voltage, and board capabilities
- **P1 C&C full 17-bank round-robin** — port of Thetis networkproto1.c WriteMainLoop
  cases 0-17; was only sending 3 of 17 banks with zeros for dither/random/preamp/Alex filters
- **Radio Setup menu item** wired (was disabled NYI)
- Auto-reconnect loads persisted model override from AppSettings

### Fixed
- P1 bank 0 C3/C4 sent all zeros — dither, random, preamp, duplex now populated
- P1 reconnect log spam (30-80 duplicate "Reconnected" lines per cycle → 1 line)
- SaturnMKII board byte falls back to Saturn-family model instead of Hermes
- Null-guard on HardwareProfile caps pointer in P1/P2 connection setup

### Docs
- Phase 3I-RP design spec and implementation plan


## [0.1.6] - 2026-04-16

About dialog and built-in issue reporter.

### Added
- **About dialog** (Help → About NereusSDR) — version, build info, Qt/WDSP
  versions, heritage credits, license text, GPG fingerprint. Accessible from
  the menu bar and wired into the build.
- **AI-assisted issue reporter** — click the lightbulb (💡) in the menu bar
  corner to file a bug report or feature request directly from NereusSDR.
  Structured prompts guide you through the fields; submits to the GitHub
  issue tracker using the `bug_report.yml` and `feature_request.yml`
  templates.

### Tests
- AboutDialog content verification tests


## [0.1.5] - 2026-04-16

Major feature release: RX DSP parity, step attenuator, Clarity adaptive display.

### Phase 3G-10: RX DSP Parity + AetherSDR Flag Port (Complete)

- **10 WDSP feature slices wired** end-to-end through SliceModel → RadioModel → RxChannel → WDSP: AGC advanced (threshold/hang/slope/attack/decay), EMNR (NR2), SNB, APF (SPCW module), squelch (SSB/AM/FM 3-variant), mute/audio pan/binaural, NB2 advanced params, RIT/XIT client offset, frequency lock, mode containers (FM OPT/DIG/RTTY)
- **Per-slice-per-band persistence** via AppSettings (`Slice<N>/Band<key>/*` namespace) with legacy key migration and band-change save/restore cycle
- **VfoWidget rewrite** — 4-tab layout (Audio/DSP/Mode/X-RIT), 4×2 DSP toggle grid, AGC 5-button row, S-meter level bar with dBm readout and cyan→green gradient, mode containers (FM/DIG/RTTY), tooltip coverage test
- **AGC-T ↔ RF Gain bidirectional sync** — both control the same WDSP max_gain; `GetRXAAGCTop`/`GetRXAAGCThresh` readback prevents audio-breaking gain runaway
- **S-meter wired** to VfoWidget via MeterPoller `smeterUpdated` signal
- **Thetis-first tooltip sweep** — 18 controls updated with verbatim Thetis text and source-line citations
- 17 new test files, widget library (VfoLevelBar, ScrollableLabel, GuardedComboBox, TriBtn, ResetSlider, CenterMarkSlider)

### Phase 3G-13: Step Attenuator & ADC Overload (PR #34)

- **StepAttenuatorController** with Classic + Adaptive auto-attenuation modes, hysteresis, and per-MAC persistence
- **P1/P2 ADC overflow detection** — `adcOverflow` signal from frame parsers, OVL status badge in RxApplet
- **Setup → General → Options** page for step ATT configuration
- **RxApplet ATT/S-ATT row** with per-model preamp items from Thetis `SetComboPreampForHPSDR`
- 9 controller tests

### Phase 3G-9: Display Refactor

- **Clarity Blue waterfall palette** — full-spectrum rainbow with deep-black noise floor
- **ClarityController** with cadence, EWMA, deadband + NoiseFloorEstimator percentile estimator
- **Reset to Smooth Defaults** button on Setup → Display → Spectrum Defaults
- **Re-tune button + Clarity status badge** in spectrum overlay panel
- **Per-band Clarity memory** in BandGridSettings
- **Zoom persistence** — visible bandwidth saved/restored across restarts
- Thetis-cited tooltips on 47 Spectrum/Waterfall/Grid setup controls
- SliderRow/SliderRowD TDD factory helpers for setup pages

### Phase 3G-11: P1 Field Fixes

- **P1 VFO frequency encoding** — encode as raw Hz, not NCO phase word

### Fixes

- Bidirectional AGC-T ↔ RF Gain sync (prevents audio runaway)
- AGC-T slider direction inverted to match Thetis (right = more gain)
- RF Gain slider removed (redundant with AGC-T)
- S-meter continuous gradient cyan→green at S9 boundary
- S-meter dBm readout with clipping fix
- NYI overlays removed on live-wired FM/DIG/RTTY containers
- Mode tab layout matches AetherSDR (combo fills row)
- DSP grid equal-column stretch eliminates gap
- AGC-T right-click context menu → Setup dialog
- Windows linker ODR violation resolved (TriBtn consolidation)
- Waterfall AGC margin widened 3→12 dB

## [0.1.4] - 2026-04-14

Bug-fix release. Improves Hermes (Protocol 1) startup reliability and
fixes two Windows-only crashes seen on cold launch / shutdown. Also
relaxes an over-aggressive board firmware-version gate that rejected
some legitimate radios.

### Fixes
- **P1 Hermes DDC priming** — prime the DDC before sending `metis-start`
  on Hermes-class boards, declare `nddc=2`, and alternate the TX/RX1
  command banks. Resolves silent RX after connect on Hermes.
- **Windows startup/shutdown crashes** — fix two latent crashes that
  surfaced on Windows clean installs (cold-start init order and
  shutdown teardown ordering).
- **Connect: drop unattested firmware floors** — remove per-board
  firmware-version minimums that were never verified against
  real-world firmware ranges and were blocking valid radios.


## [0.1.3] - 2026-04-13

Hotfix for the v0.1.2 Windows artifacts. Linux and macOS builds are
functionally identical to v0.1.2 — only the Windows installer and
portable ZIP have changed.

### Fixes
- **Windows NSIS installer** — fixed missing Qt6 DLL packaging that
  caused startup crash on clean Windows installs.
- **Windows portable ZIP** — same DLL fix as installer.


## [0.1.2] - 2026-04-13

First full cross-platform alpha release with all 6 build artifacts.

### Added
- Linux AppImage (x86_64 + aarch64)
- macOS Apple Silicon DMG
- Windows portable ZIP + NSIS installer
- Consolidated `release.yml` CI pipeline
- `/release` skill for Claude Code

### Fixes
- Linux: added `qtshadertools` build dependency
- Linux aarch64: disabled GPU spectrum (no Vulkan in CI container)
- release.yml: fixed artifact upload paths


## [0.1.1] - 2026-04-12

Internal milestone — Phase 3I Radio Connector complete.

## [0.1.0] - 2026-04-10

Initial alpha — RX-only, Protocol 2 (ANAN-G2), spectrum + waterfall,
container meter system, 12 applets, 47-page setup dialog.
