# Changelog

## [0.1.2] - 2026-04-13

### Features
- (none)

### Fixes
- fix(ci): ship real NereusSDR icon in AppImage instead of placeholder
- fix(wdsp): linux_port LinuxCreateSemaphore ignored initial_count
- fix(installer): add Windows Firewall inbound UDP allow for OpenHPSDR DDC streams
- fix(build): fail loudly when NEREUS_GPU_SPECTRUM cannot be enabled
- fix(radio): tear down RadioConnection on its worker thread

### Docs
- docs(architecture): record Linux GPU backend = OpenGL, not Vulkan
- docs: reframe intro as independent client informed by Thetis
- docs(alpha-test): expand alpha-tester guide to full OpenHPSDR family

### CI / Build
- ci: disable GPU spectrum on Linux/CodeQL (Ubuntu Qt 6.4.2 too old)

### Other
- (none)

### Tests
- test(meters): dump PNGs to QDir::temp() instead of hardcoded /tmp
- test: auto-sandbox every test via TestSandboxInit global ctor

### Refactors
- refactor(dsp): move RX I/Q processing to dedicated DSP thread


## [0.1.1] - 2026-04-13

**First tagged alpha release.** This is a debugger/developer-tester build. The
release pipeline (Phase 3N) was shipped in PR #10 and this is its first real
payload — every future release from this point forward will follow the same
format (GPG-signed SHA256SUMS, 2×AppImage + macOS DMG + Windows installer +
portable ZIP + source tarball, draft Release for human publish gate).

### What's in this build

- **Connection:** OpenHPSDR Protocol 2 connection to ANAN-G2 (Orion MkII),
  discovery, multi-port I/Q reception. Protocol 1 radio connector port
  complete through Phase 3I-21 (HardwarePage capability gating, saved-radio
  persistence, auto-reconnect on launch, HL2 init + bandwidth monitor).
- **DSP:** WDSP v1.29 integration, per-receiver channels, USB/LSB/AM/CW
  demodulation, AGC, NB1/NB2, bandpass filtering.
- **Audio:** 48 kHz stereo Int16 output via QAudioSink, FFTW wisdom caching
  (first run generates ~15 min worth, cached for subsequent launches).
- **Spectrum & waterfall:** GPU-accelerated via QRhi (Metal/Vulkan/D3D12),
  VFO tuning, CTUN panadapter with bin-subset zoom, 47 Display setup
  controls wired (Spectrum / Waterfall / Grid pages), per-band grid storage
  across 14 bands (160m–6m + GEN + WWV + XVTR).
- **UI:** Full 12-applet panel, 47-page SetupDialog across 10 categories,
  Thetis-parity container settings dialog (3-column Available / In-use /
  Properties), 31 per-item property editors, MMIO Multi-Meter I/O subsystem
  with 4 transport workers (UDP / TCP listener / TCP client / Serial),
  Thetis-style bar-row stacked meters with per-reading colours + history +
  peak hold.
- **Persistence:** `AppSettings` XML-backed settings (NOT QSettings),
  radio-authoritative vs client-authoritative policy, per-MAC hardware page
  state.

### Packaging

Every artifact is detached GPG-signed (key `KG4VCF`, fingerprint
`4A95F4D22AEE9271D8A3C01B20C284473F97D2B3`). Verify with:

```bash
gpg --keyserver keyserver.ubuntu.com --recv-keys KG4VCF
gpg --verify SHA256SUMS.txt.asc SHA256SUMS.txt
sha256sum -c SHA256SUMS.txt
```

### Known limitations

- **macOS Intel DMG not built** — GitHub Actions retired the `macos-13` free
  runner. Intel Mac users should build from source until the runner is
  restored or we cross-compile x86_64 on `macos-15` in Phase 3N+1.
- **Ad-hoc codesigning on macOS** — first launch requires right-click → Open
  until Apple Developer ID is obtained (Phase 3N+1).
- **Unsigned Windows installer** — SmartScreen will flag the binary on first
  run; click *More info → Run anyway*. Azure Trusted Signing deferred to
  Phase 3N+1.
- **Transmit (3I-1 through 3I-4) is not yet implemented.** This build is
  receive-only. Do not key up a radio with this binary.

### Reporting issues

Please file issues at <https://github.com/boydsoftprez/NereusSDR/issues>
with: OS, radio model, protocol version (P1 or P2), and log file
(`~/.config/NereusSDR/nereussdr.log` on Linux/macOS, `%APPDATA%\NereusSDR\`
on Windows).

JJ Boyd ~KG4VCF

## [Unreleased]

### Phase 3I — Radio Connector & Radio-Model Port (2026-04-13)

#### Added
- Full Protocol 1 support across the ANAN/Hermes family: Atlas/Metis (HPSDR),
  Hermes (ANAN-10/100), HermesII (ANAN-10E/100B), Angelia (ANAN-100D),
  Orion (ANAN-200D), and Hermes Lite 2. P1 radios now connect, stream I/Q,
  and feed the existing WDSP demod chain identically to Saturn on P2.
- `HPSDRModel` + `HPSDRHW` enums ported 1:1 from mi0bot/Thetis@Hermes-Lite
  `enums.cs`, preserving integer values including the 7..9 reserved gap for
  wire-format compatibility.
- `BoardCapabilities` constexpr registry — 10 entries (9 boards + Unknown),
  pure data, 20+ test invariants, drives both protocol classes and the
  Hardware setup UI.
- Discovery rewritten following mi0bot `clsRadioDiscovery.cs`: six tunable
  timing profiles (UltraFast → VeryTolerant), dual P1+P2 NIC walk with
  MAC-based de-duplication.
- `P1RadioConnection` — 24 unit-test slots locking ep2 compose + ep6 parse
  against Thetis `networkproto1.c`, loopback integration test with
  `P1FakeRadio`, 2-second watchdog + bounded reconnection state machine.
- `RadioConnectionError` enum with 9 structured error codes (design §6.1)
  replacing string-only `errorOccurred` signal.
- `ConnectionPanel` expanded to a full Thetis-equivalent radio list:
  sortable columns, color-coded state, right-click context menu,
  saved-radio persistence keyed by MAC, manual-add dialog ported from
  `frmAddCustomRadio.cs`, and auto-reconnect on launch via
  `DiscoveryProfile::Fast` against `radios/lastConnected`.
- `HardwarePage` with 9 capability-gated nested tabs mirroring Thetis
  Setup.cs "Hardware Config" sub-tabs: Radio Info, Antenna/ALEX, OC Outputs,
  XVTR, PureSignal, Diversity, PA Calibration, HL2 I/O Board, Bandwidth
  Monitor. Each control persists per-MAC under `hardware/<MAC>/*` in
  `AppSettings` with namespaced round-trip + radio-swap isolation tests.
- HL2-specific helpers on P1RadioConnection: `hl2SendIoBoardInit` (citation
  stub pending closed-source ChannelMaster.dll port in Phase 3L) and
  `hl2CheckBandwidthMonitor` sequence-gap heuristic with ep2 pause-on-throttle.

#### Changed
- `P2RadioConnection` audited to read from `BoardCapabilities`; now
  recognises `SaturnMKII`, `ANAN_G2_1K`, and `AnvelinaPro3` via the
  existing P2 wire path. Saturn regression preserved.
- `RadioDiscovery::parseP1Reply` / `parseP2Reply` exposed as public
  statics so they can be unit-tested against captured hex fixtures.
- Legacy `BoardType` enum removed; all callers migrated to `HPSDRHW`.
- `HardwareSetupPages` renamed to `HardwarePage` (single class) and
  rebuilt as a QTabWidget container.

#### Fixed
- `BoardType::Griffin=2` was a documentation-era mistake; corrected to
  `HPSDRHW::HermesII=2` matching mi0bot/Thetis `enums.cs:392`. Zero users
  were on Griffin=2 in practice so no AppSettings migration needed.
- P2 board sample-rate ceiling raised from 384 kHz to 1536 kHz per
  `Setup.cs:854`, which the original Phase 2A design doc had wrong.
- Hermes/HermesII sample-rate ceiling dropped from 384 kHz to 192 kHz —
  `Setup.cs:850-853` restricts 384k to HL2 among the single-ADC P1 family.

#### Known deferred / future phases
- **TX IQ producer** — ep2 TX sample slots carry silence. TX DSP wires in
  the dedicated TX phase.
- **PureSignal feedback DSP** — toggle persists, tab visible; feedback loop
  lands in the TX phase.
- **TCI protocol**, **RedPitaya** (DH1KLM), **sidetone generator**,
  **firmware flashing** — each is its own phase.
- **HL2 IoBoardHl2 I2C-over-ep2 wire encoding** — lives in closed
  `ChannelMaster.dll`; Phase 3L will extract the needed bytes from a live
  capture.
- **Bandwidth monitor full port** — currently a sequence-gap heuristic;
  real byte-rate accounting lands with Phase 3L.

---

### Phase 3G-9 — Thetis Meter Parity (complete, PR pending)

**Status:** Complete. Branch `test/meters-on-main` off
`origin/main` (`b9c4879`, Phase 3N tip). 20 GPG-signed
meter-parity commits plus 24 commits that reach main via a
merge of `feature/phase3g7-polish`. Not yet PR'd.

Closes the gap between NereusSDR bar meters and Thetis's
`Setup → Appearance → Meters/Gadgets` dialog — every per-reading
bar row Thetis ships can now be loaded into a NereusSDR container
with faithful composition, colour, calibration, and stacking.

See [`docs/architecture/phase3g9-meter-parity-handoff.md`](docs/architecture/phase3g9-meter-parity-handoff.md)
for the full handoff, polish gap list, and Thetis source line map.

**What landed:**

- **Phase A1–A4 — BarItem API expansion.** `ShowValue`,
  `ShowPeakValue`, `FontColour`, peak high-water with optional
  decay, history ring buffer (`ShowHistory`, `HistoryColour`,
  `HistoryDuration`), live + peak-hold markers, `BarStyle::Line`,
  non-linear `ScaleCalibration` waypoint map and
  `valueToNormalizedX()`. Every field is append-only in serialize
  so pre-A1 saved payloads load unchanged. Source:
  `MeterManager.cs:19927-20278` `clsBarItem`.

- **Phase B1–B4 — ScaleItem + readingName.** Free function
  `readingName(int bindingId)` ported verbatim from
  `MeterManager.cs:2258-2318`. ScaleItem gains `ShowType` +
  `TitleColour` centered red title and `ScaleStyle::GeneralScale`
  opt-in two-tone baseline renderer matching
  `MeterManager.cs:32338-32423` `generalScale()`. ScaleItemEditor
  surfaces ShowType checkbox + title colour swatch.

- **Phase C — persistence sweep.** `tst_meter_item_bar` gains
  three defensive cases covering full-phase round trip, pre-A1
  legacy payload, and garbled calibration tolerance.

- **Phase D1 — Thetis S-meter bar port (opt-in).** Initial commit
  4bba2c2 rebuilt `createSMeterPreset` from `addSMeterBar:21499-21616`,
  but that silently swapped the main Container #0 S-meter header
  from an arc needle to a bar row. **Reverted**
  (`revert(meters): restore needle S-Meter as default`):
  `createSMeterPreset` is back to its pre-D1 single-NeedleItem
  shape; the Thetis bar-row composition (SolidColour backdrop +
  Line BarItem with 3-point calibration + GeneralScale ScaleItem)
  moved to a new opt-in factory `createSMeterBarPreset` which is
  not wired into any UI call site — add it to a new container
  manually to run/verify. D1b rewrites `BarItem::paint()` to
  render every A-phase field, D1c overrides
  `participatesIn(Layer::OverlayDynamic)` so `MeterWidget` routes
  BarItem through QPainter instead of the GPU `emitVertices()`
  pipeline that bypassed all the new render paths — both stay
  (they're general BarItem improvements, not S-meter-specific).

- **Phase E1–E4 — per-reading factories + append UX.** 16 wrapper
  factories rewritten via shared `buildBarRow()` helper. Thetis-
  pinned colours (white bar low, red bar high, yellow indicator,
  red peak-hold marker, red peak text, red ShowType title). New
  "RX Meters (Thetis)" / "TX Meters (Thetis)" sections in the
  Available list with `PRESET_*` tags routed to a new
  `appendPresetRow()` that tags bar rows with a stack slot index
  and within-slot 0..1 local rect snapshot. `loadPresetByName`
  re-routes bar-row names to append mode while composite presets
  (ANAN MM, Cross Needle, etc.) still replace on load.

Every preset rewrite cites its `MeterManager.cs` line range in
both the commit body and the in-source factory comment per the
CLAUDE.md SOURCE-FIRST PORTING PROTOCOL. Full audit and line map
in [`docs/architecture/meter-parity-audit.md`](docs/architecture/meter-parity-audit.md).

**Pre-PR fixes** (8 commits landed at branch tip, all GPG-signed):

1. **`fix(meters): clamp GeneralScale tick row on short ScaleItems`**
   — closes polish gap #1. When `ShowType=true` and row height
   < 40 px, scale tick fractions by 0.5 so the red title text
   and the tick row stop colliding.

2. **`fix(meters): persist BarItem peakFontColour across serialize (E2)`**
   — closes polish gap #2. Append field 31 to the BAR format
   (empty slot = fall back to m_fontColour). New
   `tst_meter_item_bar::peakFontColour_roundtrip_preserves_explicit_override`.

3. **`fix(meters): render "--" for idle ShowPeakValue slot`**
   — closes polish gap #3. Non-finite peak values render a
   literal `"--"` placeholder instead of blank text so parked
   bindings still show a readout slot.

4. **`revert(meters): restore needle S-Meter as default; preserve bar opt-in`**
   — reverts commit 4bba2c2. Container #0's fixed S-Meter header
   and the `"S-Meter Only"` Presets menu entry return to the
   AetherSDR arc needle. Thetis `addSMeterBar` composition
   preserved in new opt-in factory `createSMeterBarPreset`
   (not wired to any UI call site). Six tests in
   `tst_meter_presets` retargeted at the new factory; a new
   `SMeter_preset_is_a_needle_by_default` guards the revert.

5. **`fix(mainwindow): Reset Default Layout also rebuilds panel meter`**
   — pre-existing bug surfaced during verification:
   `resetDefaultLayout()` destroyed non-panel floating containers
   but left Container #0's MeterWidget untouched, so a corrupt
   or unwanted panel meter state couldn't be cleared without
   hand-editing `~/.config/NereusSDR/NereusSDR.settings`. Fix
   clears and repopulates the panel meter's S-Meter / Power/SWR
   / ALC defaults alongside the floating-container teardown.

6. **`fix(main): stop quit-time crash in messageHandler redactPii`**
   — every app quit crashed with `EXC_BAD_ACCESS` in
   `QRegularExpression::pattern()`. Classic static-destruction-
   order fiasco: Qt's `QThreadDataDestroyer` emits warnings via
   `QMessageLogger` from `__cxa_finalize` after the function-
   local static regex objects in `redactPii()` had already been
   destroyed. Fix leaks the regex pointers intentionally so they
   survive teardown, plus `qInstallMessageHandler(nullptr)` right
   after `app.exec()` returns as belt-and-braces.

7. **`fix(dialog): preserve stack metadata through Apply + snapshot clones`**
   — serialize/deserialize drops runtime-only stack metadata, so
   the dialog's Apply path was landing cloned items on the target
   `MeterWidget` with `m_stackSlot == -1`, which caused
   `reflowStackedItems()` to no-op on them. Fix copies
   `m_stackSlot` / `m_slotLocalY/H` across the clone alongside
   the existing MMIO-binding copy, and kicks a reflow at the
   end of `applyToContainer()`. Same fix applied on the
   dialog-open `populateItemList` side.

8. **`feat(meters): Thetis-parity normalized stack + ANAN MM 0.441 band`**
   — the big one. Rewrites the Container Settings dialog's
   append-to-stack layout to match Thetis Default Multimeter
   exactly:
   * Composite presets are authored at their Thetis-nominal
     normalized size directly by the factory. ANAN MM occupies
     `(0, 0, 1, 0.441)` per Thetis
     `MeterManager.cs:22472 ni.Size = new SizeF(1f, 0.441f)`.
     No compress-to-fraction step runs anywhere.
   * Bar rows use Thetis `_fHeight = 0.05f` (normalized) from
     `MeterManager.cs:21266`, with a NereusSDR-only **24 px
     pixel floor** so rows stay readable in tight containers.
   * `MeterWidget::reflowStackedItems()` computes `slotHpx =
     max(0.05 * widgetH, 24)` and `bandTop = max(y + itemHeight)`
     over items with `itemHeight > 0.30` on every resize.
     Re-lays every stacked item from those values — no
     per-item bandTop field.
   * `ContainerSettingsDialog::appendPresetRow` loses its
     compress-to-0.70 block entirely. Composites stay at their
     factory size; bar rows just get a stack slot index and
     a within-slot 0..1 local rect snapshot; reflow does the
     rest.
   * `MeterItem::layoutInStackSlot` signature grows a `bandTop`
     parameter; `m_stackBandTop` field removed.
   * `MeterWidget::inferStackFromGeometry()` runs after
     `deserializeItems()` (MainWindow panel restore) so saved
     containers keep their reflow-on-resize behaviour without
     a persistence format bump.
   * Result: ANAN MM no longer stretches to 70% of container
     height; bar rows stack at Thetis-correct spacing with a
     minimum pixel floor; resizing taller reveals more rows;
     shorter clips rows off the bottom.

**Known polish gaps** (queued, not blockers):
1. Scale tick labels can duplicate at narrow widths
2. `m_peakHoldDecayRatio = 0.02f` may feel slow — consider `0.05f`
3. UI automation flakiness in QListWidget (affects regression
   test automation, not users)
4. Manual `AutoHeight` container mode (Thetis
   `ucMeter.cs:903`) — Thetis auto-grows the container when
   content would overflow; NereusSDR currently relies on the
   user to resize the container taller manually

~~Polish gap #4~~ (70/30 composite/row split) — **obsolete**,
the entire compress block was removed in commit #8 above.
~~Polish gap #5~~ (`nextStackYPos h > 0.7` edge case) — **obsolete**
for the same reason.

**Full test suite** 6/6 green on the merged tree (tst_smoke,
tst_container_persistence, tst_meter_item_bar,
tst_meter_item_scale, tst_reading_name, tst_meter_presets — 50+
assertions total). Verified live via Quartz screencapture on the
merged build.

### Phase 3G-8 — RX1 Display Parity (complete)

**Status:** Complete. Branch `feature/phase3g8-rx1-display-parity`
off `feature/phase3g7-polish`. 9 GPG-signed code commits plus
three doc-amend prep commits. Brings the Display → Spectrum
Defaults / Waterfall Defaults / Grid & Scales pages from "every
control disabled with NYI tooltip" to feature parity with Thetis
for RX1.

See [`docs/architecture/phase3g8-rx1-display-parity-plan.md`](docs/architecture/phase3g8-rx1-display-parity-plan.md)
for the design (plan §13 open questions resolved in commit
`0308b1b`, §5.3 architectural correction in `b8045cc`).

**What landed:**

- **Commit 1** — `ColorSwatchButton` reusable widget. Replaces
  the dead `makeColorSwatch` placeholder with a real
  `QPushButton + QColorDialog` pair. Used by 9 call sites across
  this phase and available to future TX Display / skin editor
  work.
- **Commit 2** — per-band grid storage on `PanadapterModel`.
  New `Band` enum (14 bands: 160m–6m + GEN + WWV + XVTR), new
  `BandGridSettings { dbMax, dbMin }` struct, per-band hash, 28
  persistence keys, `bandChanged` signal, and `setCenterFrequency`
  auto-derive. `BandButtonItem` expanded 12 → 14 buttons.
  Initialised to Thetis uniform -40 / -140 per plan §13 Q4.
- **Commits 3–5** — `SpectrumWidget` + `FFTEngine` renderer
  additions: averaging mode (None/Weighted/Log/TimeWindow),
  peak hold + decay, trace line width, fill + alpha, gradient,
  cal offset, waterfall AGC, reverse scroll, opacity, update
  period rate limiting, waterfall averaging, use-spectrum-min/max,
  filter/zero-line overlays, timestamp overlay, 3 new colour
  schemes (LinLog / LinRad / Custom, total now 7), configurable
  grid / grid-fine / h-grid / text / zero-line / band-edge
  colours, 5-mode frequency label alignment, FPS overlay. GPU
  line width and gradient shader wire-up deferred to a future
  polish pass.
- **Commits 6–8** — wire each Display setup page to the new
  state:
  - Spectrum Defaults: 17 controls including FFT size / window
    (routed to FFTEngine), FPS, averaging + time + decimation,
    fill + alpha, line width, gradient, cal offset, peak hold
    + delay, 3 colour pickers, thread priority.
  - Waterfall Defaults: 17 controls including high / low
    thresholds, AGC, low colour, use-spectrum-min/max, update
    period, reverse scroll, opacity, 7-scheme colour combo,
    WF averaging, 4 filter / zero-line overlay toggles,
    timestamp position + mode.
  - Grid & Scales: 13 controls including show grid, live
    per-band dB Max / Min with "Editing band: N" label that
    updates on PanadapterModel::bandChanged, global dB Step,
    5-mode frequency label align, show zero line, show FPS,
    6 colour pickers (grid / grid-fine / h-grid / text /
    zero-line / band-edge).
- **Commit 9** — this CHANGELOG entry + verification matrix
  checklist at
  [`docs/architecture/phase3g8-verification/README.md`](docs/architecture/phase3g8-verification/README.md).

**Architectural additions:**

- `RadioModel::spectrumWidget()` / `fftEngine()` non-owning
  view hooks, wired by `MainWindow` during construction, so
  setup pages can reach the renderer without depending on
  `MainWindow` directly.
- `src/models/Band.h` — first-class 14-band enum with label,
  key-name, frequency lookup (IARU Region 2), and UI-index
  mapping.

**Authorized divergences from Thetis (plan §10):**

- New per-band grid slots initialise to Thetis uniform
  -40 / -140 rather than NereusSDR's existing -20 / -160,
  per user decision 2026-04-12. Existing users see the grid
  shift on first launch after upgrade.
- Source-first protocol (`CLAUDE.md`) stays as written. This
  phase is a one-off exception, not a precedent.

**Known deferrals (tracked for future phases):**

- GPU path line width and gradient shader wiring. QPainter
  fallback path fully implemented.
- FFT decimation (S16) — UI is scaffolded; FFTEngine has no
  decimation setter yet.
- W12 / W14 TX filter / zero-line overlays — renderer call
  path in place, activates once the TX state model provides
  a TX VFO/filter pair (post-3I-1).
- Data Line Color / Data Fill Color share `m_fillColor` — UX
  polish to split these is deferred until verification
  screenshots show whether users actually need them separate.
- W10 Waterfall Low Color — persisted; runtime gradient
  rebuild waits for the Custom-scheme AppSettings parser.

### Phase 3G-7 — Polish (complete)

**Status:** Complete. Branch `feature/phase3g7-polish` off
`feature/phase3g6-oneshot`. 4 GPG-signed commits, all polish
landed in a tighter scope than the original handoff proposed.
See [`docs/architecture/phase3g7-polish-handoff.md`](docs/architecture/phase3g7-polish-handoff.md).

**Items shipped:**

- **B — MeterItem accessor gap fills** (`25a7819`). Five
  subclasses (TextOverlayItem, RotatorItem, FilterDisplayItem,
  ClockItem, VfoDisplayItem) had setters with no matching
  getters, so each item's property editor `setItem()`
  populated only a fraction of its widgets. Added 42 trivial
  inline getters and wired each editor to populate from them.
  Investigation showed the handoff's other "phantom field"
  candidates (`SignalText::showMarker`, `LEDItem::condition`,
  `HistoryGraph::keepFor`/`fadeRx`, `MagicEye::darkMode`/`eyeScale`,
  `DialItem::vfoClickBehavior`) don't exist on either the items
  or the editors today — they're future feature ports, not
  gap fills, and have been moved to deferred-features list.
- **A — MMIO binding clone-path side-channel** (`8774b7c`).
  The 3G-6 runtime bug — meter items losing their MMIO binding
  on dialog Apply — turned out to be a 4-site clone leak in
  `ContainerSettingsDialog.cpp`, not a 30-subclass serialize
  sweep. `populateItemList`, `applyToContainer`,
  `takeSnapshot`/`revertFromSnapshot`, and the preset clone
  loop now copy `(mmioGuid, mmioVariable)` directly around the
  text round-trip via a parallel
  `QVector<QPair<QUuid, QString>>` snapshot. ~50 LOC in one
  file, no subclass changes. Disk persistence remains
  deliberately deferred (block 5 design).
- **C — `NeedleItemEditor` `QGroupBox` grouping** (`41c7031`).
  The 17 needle-specific fields plus the calibration table now
  live in 5 group boxes (Needle / Geometry / History / Power /
  Calibration) instead of stacking flat under header labels.
  Member pointers and connect lambdas unchanged; layout-only.
- **F — MMIO smoke test** — *deferred*. User skipped at scope
  decision; the runtime fix in item A is the proof MMIO works
  end-to-end, smoke test doc tracked as separate future work.
- **D — Editor widget width sweep** — *deferred to future*.
  Cosmetic, block 4b's scroll area solved the load-bearing
  reachability problem.
- **E — `ButtonBox` per-button `ButtonState` sub-editor** —
  *deferred to future*. Architectural addition; no current
  workflow blocked on it.

**Investigation note:** Both items A and B turned out to be
narrower than the handoff proposed. Item B's "phantom fields"
mostly didn't exist on either side; item A's bug was a 4-site
clone leak in one dialog file rather than a 30-subclass
serialize sweep. The handoff's pessimistic scope estimate cost
zero session time to disprove and saved a lot of mechanical
edits.

### Phase 3G-6 (One-Shot) — Container Settings Dialog + Full Thetis Parity + MMIO

**Status:** Complete. Branch `feature/phase3g6-oneshot`, PR #2.
Plan: [`docs/architecture/phase3g6a-plan.md`](docs/architecture/phase3g6a-plan.md).

40 GPG-signed commits across 7 execution blocks landed the
biggest single phase in NereusSDR's history, covering the
complete user-facing surface for the meter system.

#### Block 1 — rendering plumbing + Thetis filter rule
- Meter PNGs registered as Qt resources (`:/meters/ananMM.png`,
  CrossNeedle stopgap placeholders).
- `onlyWhenRx` / `onlyWhenTx` / `displayGroup` moved to
  `MeterItem` base, default 0 per Thetis sentinel.
- Thetis filter rule (MeterManager.cs:31366-31368) ported into
  `MeterWidget::shouldRender()`, wired into all 5 paint paths.
- ANANMM `NeedleScalePwrItem` pinned to power/SWR group to fix
  RX label overlap.

#### Block 2 — container surface + NeedleItem calibration
- `m_titleBarVisible`, `m_minimised`, `m_highlighted` on
  `ContainerWidget` (the latter painting a 2px accent outline).
- `ContainerManager::duplicateContainer`, `containerTitleChanged`
  signal chain.
- `FloatingContainer` collapses on `minimisedChanged(true)`.
- `NeedleItem` rewritten for calibration-driven painting:
  serializes the full factory state (16-point calibration map,
  needle color, geometry, smoothing); paint routines early-return
  when calibration is non-empty so the background ImageItem
  shows through; emitVertices honors `m_radiusRatio` /
  `m_lengthFactor` and reads needle color from `m_needleColor`.

#### Block 3 — `ContainerSettingsDialog` rewrite
- Live preview panel removed.
- Thetis 3-column layout: Available / In-use / Properties.
- Container-switch dropdown populated from
  `ContainerManager::allContainers()` (auto-commit on switch
  landed in block 7).
- Snapshot + revert on Cancel.
- Categorized Available list (RX / TX / Special).
- Two-row container property bar with all 8 plan controls plus
  Duplicate / Delete buttons.
- Footer: Save / Load / MMIO Variables (last became real in
  block 5).

#### Block 4 — per-item property editors (parallel agents)
- `BaseItemEditor` shared base class with 9 common form rows.
- 30 subclass editors built in parallel by 4 subagents:
  primitives (5), needles (3 — `NeedleItemEditor` includes a
  fully-editable calibration `QTableWidget`), text/signal/history
  (6), interactive + button-box (17). 62 files, ~155 fields,
  zero manual fixups.
- Dialog factory wires the right editor per item type tag and
  pushes property changes live via
  `propertyChanged → applyToContainer`.
- Block 4b polish: editor pages wrapped in `QScrollArea`,
  dialog grown to 1100×750, splitter rebalanced, minimum field
  widths in `BaseItemEditor` helpers.

#### Block 5 — MMIO subsystem
- Started with a docs commit (`f090dcf`) that rewrote section
  6 of the plan after a Thetis Explore agent found the
  original draft's per-variable parse-rule taxonomy was
  imaginary. Real Thetis is endpoint-centric with format-driven
  (JSON / XML / RAW) discovery.
- Core scaffold: `ExternalVariableEngine` singleton on its own
  worker `QThread`, `MmioEndpoint` value-object + per-endpoint
  `QHash<QString, QVariant>` cache behind a `QReadWriteLock`,
  `FormatParser` free functions, `ITransportWorker` abstract
  base, `lcMmio` logging category.
- Four parallel-built transport workers:
  `UdpEndpointWorker`, `TcpListenerEndpointWorker`,
  `TcpClientEndpointWorker`, `SerialEndpointWorker` (the last
  gated on `HAVE_SERIALPORT`). All Qt event-driven instead of
  Thetis's 50ms polling loops.
- XML persistence under `AppSettings/MmioEndpoints/<guid>/*`
  instead of Thetis's opaque Base64 binary blob.
- `MmioEndpointsDialog` 3-column manager (endpoint list +
  property form + discovered-variables tree).
- `MmioVariablePickerPopup` tree-of-endpoints picker for the
  per-item editor's new "Variable…" button.
- `MeterItem::m_mmioGuid` / `m_mmioVariable` in-memory binding
  + `MeterPoller` branch that reads bound values and pushes
  them into items at 10 fps.
- App startup wires `ExternalVariableEngine::instance().init()`.

#### Block 6 — `Containers → Edit Container` submenu
- Replaces the static "Container Settings…" action with a
  dynamic submenu populated from
  `ContainerManager::allContainers()`, alphabetized by notes.
- Rebuild on `containerAdded` / `containerRemoved` /
  `containerTitleChanged`.
- `Reset Default Layout` finally functional (was disabled NYI).

#### Block 7 — polish + docs
- Dead-code cleanup: 720 lines of legacy
  `buildXItemEditor`/`buildCommonPropsPage`/etc. methods
  removed from `ContainerSettingsDialog.cpp`.
- `lcMmio` registered in `LogManager` runtime metadata.
- Copy / Paste item settings (plan commit 35).
- Container-dropdown auto-commit on switch (plan commit 13's
  deferred half).
- This CHANGELOG entry + phase-table flip + debug-handoff
  marked Resolved.

### Known limitations after 3G-7

Resolved in 3G-7:
- ~~MMIO bindings dropped on dialog Apply~~ — fixed via the
  4-site clone-path side-channel in `ContainerSettingsDialog.cpp`.
- ~~`NeedleItemEditor` flat field stack~~ — now wrapped in 5
  `QGroupBox`es.
- ~~Five item subclasses missing accessor getters~~ —
  TextOverlay, Rotator, FilterDisplay, Clock, VfoDisplay
  populate fully on dialog open.

Still outstanding:
- **MMIO binding disk persistence.** Bindings survive Apply
  and Cancel within a session, but `serialize()`/`deserialize()`
  on each item still does not include them. Save-to-file +
  reload, or app restart, drops bindings. Block 5's design
  explicitly deferred this; the path forward is the original
  handoff's "append `|guid|var` to every concrete subclass
  format" sweep.
- **MMIO end-to-end smoke test.** No external doc walking a
  user through "spin up a UDP listener, send JSON, see the
  meter move." Useful first-real-proof artifact when MMIO
  bindings get exercised in anger.
- **Editor `QLineEdit` / `QComboBox` / `QPushButton` width
  sweep.** Block 4b's `BaseItemEditor` helpers set
  `setMinimumWidth(140)` but the 30 subagent-produced editors
  hand-roll some controls that still shrink-wrap. Cosmetic.
- **`ButtonBoxItem` per-button `ButtonState` sub-editor.**
  Per-button color/style/font fields live on a `ButtonState`
  struct array; needs a per-button-index spinner UI in
  `ButtonBoxItemEditor`. Untested code path because no
  workflow currently exposes per-button customization.
- **Phantom fields not yet ported from Thetis** (would expand
  scope, not gap-fill). Each is a standalone Thetis port:
  - `LEDItem` — custom amber/red zone colors (fields exist,
    no setter); string-expression `condition` mode
  - `SignalTextItem::showMarker` — bar-style marker visibility
    toggle
  - `HistoryGraphItem` — `keepFor` (time-based retention),
    `ignoreHistory` (pause), manual axis range, `fadeRx/Tx`
  - `MagicEyeItem` — `darkMode`, `eyeScale`, `eyeBezelScale`
  - `DialItem::vfoClickBehavior` — quadrant click action remap
- **Real CrossNeedle PNG artwork** — current files are
  byte-identical copies of `ananMM.png`. Waiting on user.
- **TX wiring** from `RadioModel` / `TransmitModel` into
  `MeterWidget::setMox()` — phase 3I-1 territory.

### Added — Phase 3G-5: Interactive Meter Items

**Mouse event infrastructure:**
- MeterItem base class gains virtual `hitTest()`, `handleMousePress()`, `handleMouseRelease()`, `handleMouseMove()`, `handleWheel()` with default no-op implementations
- MeterWidget forwards mouse events to items in reverse z-order (top-first hit dispatch)

**ButtonBoxItem shared base class** (from Thetis clsButtonBox, MeterManager.cs:12307+):
- Configurable grid layout (rows/columns), rounded rectangle buttons
- 13 indicator types: Ring, Bar (L/R/B/T), Dot (L/R/B/T/TL/TR/BL/BR), TextIconColour
- Three-state colors per button (fill/hover/click), on/off state
- 100ms click highlight timer (from Thetis `setupClick` pattern)
- FadeOnRx/FadeOnTx visibility guards, visibility bitmask

**8 interactive button grid items:**
- `BandButtonItem` — 12 band buttons (160m-6m, GEN), left-click select + right-click bandstack popup (from Thetis clsBandButtonBox)
- `ModeButtonItem` — 10 mode buttons (LSB-DIGU), left-click select (from Thetis clsModeButtonBox)
- `FilterButtonItem` — 12 filter buttons (F1-F10, Var1/2), mode-dependent labels, right-click context menu (from Thetis clsFilterButtonBox)
- `AntennaButtonItem` — 10 color-coded buttons: Rx (green), Aux (orange), Tx (red), Toggle (yellow) (from Thetis clsAntennaButtonBox)
- `TuneStepButtonItem` — 7 step buttons (1Hz-1MHz) with active highlighting (from Thetis clsTunestepButtons)
- `OtherButtonItem` — 34 core control buttons + 31 macro slots with Toggle/CAT/ContainerVis modes (from Thetis clsOtherButtons)
- `VoiceRecordPlayItem` — 5 DVK buttons (REC/PLAY/STOP/NEXT/PREV) (from Thetis clsVoiceRecordPlay)
- `DiscordButtonItem` — 12 Discord Rich Presence buttons (from Thetis clsDiscordButtonBox)

**3 standalone interactive items:**
- `VfoDisplayItem` — frequency display in XX.XXX.XXX format with per-digit mouse wheel tuning, mode/filter/band labels, RX/TX indicator, split indicator (from Thetis clsVfoDisplay)
- `ClockItem` — dual Local + UTC time display with 1s auto-refresh, 24h/12h toggle (from Thetis clsClock)
- `ClickBoxItem` — invisible hit region for unit cycling overlays (from Thetis clsClickBox)

**1 data item:**
- `DataOutItem` — MMIO external data bridge with JSON/XML/RAW output formats and UDP/TCP/Serial transport (from Thetis clsDataOut)

**Signal-based interaction:** all interactive items emit Qt signals, never touch RadioModel directly. Signal chain: MeterItem → MeterWidget → ContainerWidget → MainWindow.

**ContainerWidget signal forwarding:** `wireInteractiveItem()` connects band/mode/filter/antenna/tuneStep/other/macro/voice/discord/VFO signals from MeterItems to container-level signals.

**ItemGroup extensions:** 13 new type tags in deserialize registry. 3 new presets: VFO Display, Clock, Contest (VFO + band buttons + mode buttons + clock).

### Added — Phase 3G-4: Advanced Meter Items

**12 new MeterItem types (passive/display):**
- `SpacerItem` — layout spacer with gradient fill (from Thetis clsSpacerItem)
- `FadeCoverItem` — RX/TX transition overlay (from Thetis clsFadeCover)
- `LEDItem` — LED indicator: 3 shapes (Square/Round/Triangle), 2 styles (Flat/ThreeD), blink/pulsate (from Thetis clsLed)
- `HistoryGraphItem` — scrolling time-series with dual-axis ring buffer, auto-scaling (from Thetis clsHistoryItem)
- `MagicEyeItem` — vacuum tube magic eye with green phosphor arc (from Thetis clsMagicEyeItem)
- `NeedleScalePwrItem` — non-linear power scale labels for needle arcs (from Thetis clsNeedleScalePwrItem)
- `SignalTextItem` — S-units/dBm/uV signal display with peak hold and bar styles (from Thetis clsSignalText)
- `DialItem` — circular dial with VFO/ACCEL/LOCK quadrant buttons (from Thetis clsDialDisplay)
- `TextOverlayItem` — two-line text with %VARIABLE% substitution parser (from Thetis clsTextOverlay)
- `WebImageItem` — async web-fetched image with periodic refresh (from Thetis clsWebImage)
- `FilterDisplayItem` — mini passband spectrum/waterfall with filter edges and notch overlays (from Thetis clsFilterItem)
- `RotatorItem` — antenna rotator compass dial with AZ/ELE/BOTH modes (from Thetis clsRotatorItem)

**Complex composite presets:**
- ANANMM 7-needle multi-meter (signal, volts, amps, power, SWR, compression, ALC) with exact Thetis calibration points
- CrossNeedle dual fwd/rev power meter with mirrored CounterClockwise geometry

**NeedleItem extensions:** scaleCalibration, needleOffset, radiusRatio, lengthFactor, direction (CW/CCW), onlyWhenRx/Tx, displayGroup, historyEnabled/Color, normaliseTo100W

**Edge meter display mode:** BarItem gains Filled/Edge style with 3-line indicator (from Thetis Edge meter)

**15+ bar preset factories:** Signal, AvgSignal, MaxBin, ADC, AGC, PBSNR, EQ, Leveler, ALC variants, CFC variants

**MeterBinding extensions:** SignalMaxBin(7), PbSnr(8), TxEq-TxCfcGain(106-112), HwVolts/Amps/Temp(200-202), RotatorAz/Ele(300-301)

### Fixed — Applet Panel Layout
- RxApplet: fix STEP row overflow (138px content in 99px column), change filter grid
  from 2×5 to 3-column layout matching AetherSDR, remove fixedWidth from RIT/XIT
  labels so they flex with panel width, add Expanding sizePolicy to RIT/XIT toggles
- TxApplet: add outer→body wrapping pattern, merge MOX/TUNE/ATU/MEM into single
  4-button row matching AetherSDR, fix value label styles and slider label colors
- PhoneCwApplet: add missing applyComboStyle() to mic combos, set Ignored sizePolicy
  on CW QSK/Iambic/FW Keyer buttons for even spacing, flex CW pitch label
- EqApplet: add outer→body wrapping, reduce scale/spacer widths for 10-band fit,
  reduce band label font to 9px, add applyComboStyle() to preset combo

### Added — Phase 3-UI: Full UI Skeleton

**Applets (12 total, 150+ control widgets):**
- `src/gui/applets/RxApplet.h/.cpp` — RX controls: mode, AGC, AF/RF gain, filter presets; Tier 1 wired to SliceModel
- `src/gui/applets/TxApplet.h/.cpp` — TX controls: mic gain, drive, tune power, compression, DEX
- `src/gui/applets/PhoneCwApplet.h/.cpp` — Voice/CW macro keys, VOX, CW speed/weight
- `src/gui/applets/EqApplet.h/.cpp` — 10-band RX/TX graphic equalizer
- `src/gui/applets/FmApplet.h/.cpp` — FM deviation, sub-tone, repeater offset
- `src/gui/applets/DigitalApplet.h/.cpp` — digital mode settings (baud, shift, encoding)
- `src/gui/applets/PureSignalApplet.h/.cpp` — PureSignal calibrate, feedback level indicator
- `src/gui/applets/DiversityApplet.h/.cpp` — diversity RX phase/gain controls
- `src/gui/applets/CwxApplet.h/.cpp` — CW message memory keyer
- `src/gui/applets/DvkApplet.h/.cpp` — digital voice keyer playback controls
- `src/gui/applets/CatApplet.h/.cpp` — CAT/rigctld port configuration
- `src/gui/applets/TunerApplet.h/.cpp` — antenna tuner control (tune, bypass, memory)

**Spectrum Overlay Panel:**
- `src/gui/SpectrumOverlayPanel.h/.cpp` — 10-button overlay panel with 5 flyout sub-panels (display, filter, noise, spots, tools); auto-close on outside click

**Menu Bar (9 menus, ~60 items):**
- File, Radio, View, DSP, Band, Mode, Containers, Tools, Help menus wired to MainWindow

**Status Bar:**
- Double-height status bar (46px): UTC clock, radio info, TX/RX indicators, signal level

**Setup Dialog:**
- `src/gui/SetupDialog.h/.cpp` — 47 pages across 10 categories (Radio, Audio, DSP, Display, CW, Digital, TX, Logging, Network, Misc); all pages have real controls

**Applet Panel:**
- `src/gui/AppletPanelWidget.h/.cpp` — fixed header (S-Meter) + scrollable applet body for Container #0

**Infrastructure:**
- `src/gui/StyleConstants.h` — shared color palette, font sizes, widget style constants
- `src/gui/widgets/HGauge.h/.cpp` — horizontal bar gauge widget
- `src/gui/widgets/ComboStyle.h/.cpp` — styled combo box shared across applets

**Container / Persistence fixes:**
- Content restored on restart (applet panel state persisted)
- Pin-on-top state persists across sessions
- Floating container position persists correctly
- Container minimum width 260px enforced in all dock modes

**S-Meter enhancements:**
- Dynamic resize with aspect-ratio scaling and full-width arc
- SMeterWidget ported directly from AetherSDR for pixel-identical fidelity

### Added — Phase 3G-3: Core Meter Groups (in progress)
- NeedleItem: arc-style S-meter needle participating in all 4 GPU render pipelines
  - P1 Background: QPainter arc segments (white S0-S9, red S9+, blue inner TX arc)
  - P2 Geometry: uniform-width needle quad + shadow + amber peak marker triangle
  - P3 OverlayStatic: tick marks and labels (1,3,5,7,9,+20,+40) via needleDir vector
  - P3 OverlayDynamic: S-unit readout (cyan) + dBm readout (light steel)
- Multi-layer MeterItem infrastructure: participatesIn() + paintForLayer() virtuals
- Exponential smoothing (SMOOTH_ALPHA=0.3 from AetherSDR) + peak hold (Medium preset)
- S-unit scaling from Thetis: S0=-127dBm, S9=-73dBm, 6dB/S-unit
- Aspect-ratio-locked meterRect() helper (2:1 matching AetherSDR sizeHint 280x140)
- TX MeterBinding constants 100-105 (Power, ReversePower, SWR, Mic, Comp, ALC)
- Preset factories: createSMeterPreset, createPowerSwrPreset, createAlcPreset, createMicPreset, createCompPreset
- ItemGroup::installInto() for positioning groups within MeterWidget sub-regions
- Default Container #0 layout: S-Meter (55%) + Power/SWR (30%) + ALC (15%)
- NEEDLE serialization format + deserializer registration
- NOTE: S-meter will be re-implemented as dedicated SMeterWidget (direct AetherSDR port)
  for pixel-identical fidelity — the MeterItem/NeedleItem approach introduces rendering
  artifacts from GPU pipeline splitting and texture-based text

### Added — Phase 3G-2: MeterWidget GPU Renderer
- MeterWidget (QRhiWidget): 3-pipeline GPU rendering engine for meters inside ContainerWidget
  - Pipeline 1 (textured quad): cached background textures and images
  - Pipeline 2 (vertex-colored geometry): animated bar fills with per-frame vertex updates
  - Pipeline 3 (QPainter overlay, split static/dynamic): tick marks, labels, value readouts
- MeterItem base class with normalized 0-1 positioning and data binding
- Concrete item types: BarItem (H/V), TextItem, ScaleItem (H/V), SolidColourItem, ImageItem
- BarItem exponential smoothing (attack 0.8 / decay 0.2, from Thetis MeterManager.cs)
- ItemGroup composite with createHBarPreset() factory (AetherSDR visual style)
- MeterPoller: QTimer-based WDSP meter polling at 100ms via RxChannel::getMeter()
- MeterBinding constants mapping to RxMeterType (SignalPeak through AgcAvg)
- Pipe-delimited item serialization compatible with ContainerWidget persistence
- Container #0 pre-populated with live horizontal signal strength bar meter
- Shaders: meter_textured.vert/.frag (Pipelines 1 & 3), meter_geometry.vert/.frag (Pipeline 2)
- lcMeter logging category

### Added — Phase 3G-1: Container Infrastructure
- ContainerWidget: dock/float/resize container shell with title bar, axis-lock (8 positions), content slot
- FloatingContainer: top-level window wrapper with pin-on-top, minimize-with-console, geometry persist
- ContainerManager: singleton lifecycle — 3 dock modes (PanelDocked/OverlayDocked/Floating)
- QSplitter-based MainWindow layout: spectrum left, Container #0 right panel
- Axis-lock repositioning: docked containers track main window resize per anchor position
- 24-field pipe-delimited serialization (Thetis-compatible + DockMode extension)
- Container + splitter state persistence to AppSettings
- Hover-reveal title bar with dock-mode-aware buttons (float/dock, axis cycle, pin, settings)
- Wider hover detection zone (40px) for reliable title bar reveal on high-DPI displays
- Frameless floating containers on macOS (no native title bar, container provides its own)
- Ctrl-snap to 10px grid during drag and resize
- Phase roadmap: AetherSDR-style AppletPanel planned for Container #0 in Phase 3G-AP

### Added — Phase 3E: VFO & Controls + CTUN Panadapter
- Floating VFO flag widget (AetherSDR pattern): frequency display, mode/filter/AGC tabs, antenna buttons
- SliceModel: DSPMode enum, AGCMode, per-mode filter defaults from Thetis, tuning step, AF/RF gain
- SmartSDR-style CTUN panadapter: independent pan center and VFO, WDSP shift offsets
- Off-screen VFO indicator with double-click to recenter
- I/Q pipeline rewired through ReceiverManager with DDC-aware routing
- Alex HPF/LPF/BPF filter registers ported from Thetis (frequency-based selection)
- Full-spectrum FFT output (all N bins, FFT-shift + mirror for correct sideband orientation)
- VFO settings persistence (frequency, mode, filter, AGC, step, gains, antennas)
- AetherSDR-style VFO control buttons and green toggle filter presets
- CTUN zoom: frequency scale bar drag and Ctrl+scroll zoom into FFT bin subsets
- Hybrid zoom: smooth bin subsetting during drag, FFT replan on mouse release
- visibleBinRange() maps display window to DDC bin indices (GPU + CPU + waterfall)
- DDC center frequency tracking (m_ddcCenterHz) for correct bin-to-frequency mapping
- Waterfall preserves existing rows across zoom changes (new rows at current scale)

### Added — Phase 3D: GPU Spectrum & Waterfall
- FFTEngine: FFTW3 float-precision on dedicated spectrum worker thread
- SpectrumWidget: QRhiWidget GPU rendering via Metal (macOS) / Vulkan / D3D12
- 3 GPU pipelines: waterfall (ring-buffer texture), spectrum (vertex-colored), overlay (QPainter→texture)
- 6 GLSL 4.40 shaders compiled to QSB via qt_add_shaders()
- 4096-point FFT, Blackman-Harris 4-term window, 30 FPS rate limiting
- VFO marker, filter passband overlay, cursor frequency readout
- Mouse interaction: scroll zoom, drag ref level, click-to-tune
- Right-click SpectrumOverlayMenu with display settings
- AetherSDR default + Thetis Enhanced/Spectran/BlackWhite color schemes

### Added — Phase 3C: macOS Build
- WDSP linux_port.h cross-platform compat (stdlib.h, string.h, fcntl.h, LPCRITICAL_SECTION)
- ARM64 flush-to-zero guard, AVRT stubs
- Fixed use-after-free crash: wisdom poll timer accessing deleted QThread

### Added — Phase 3B: WDSP Integration + Audio Pipeline
- WDSP v1.29 built as static library in third_party/wdsp/
- RxChannel wrapper: I/Q accumulation (238→1024), fexchange2(), NB1/NB2
- AudioEngine: QAudioSink 48kHz stereo Int16, 10ms timer drain
- FFTW wisdom generation with progress dialog, cached for subsequent launches
- Audio device selection and persistence via AppSettings

### Added — Phase 3A: Radio Connection (Protocol 2)
- P2RadioConnection faithfully ported from Thetis ChannelMaster/network.c
- P2 discovery (60-byte packet, byte[4]=0x02) on all network interfaces
- CmdGeneral/CmdRx/CmdTx/CmdHighPriority byte-for-byte from Thetis
- I/Q data streaming: DDC2, 1444 bytes/packet, 238 samples at 48kHz
- ConnectionPanel dialog with discovered radio list, connect/disconnect
- LogManager with runtime category toggles, AppSettings persistence
- SupportDialog with log viewer, category checkboxes, support bundle creation
- Auto-reconnect to last connected radio via saved MAC

## [0.1.0] - 2026-04-08

### Added
- Initial project scaffolding
- CMake build system with Qt6 and C++20
- Stub source files (AppSettings, RadioDiscovery, RadioConnection, WdspEngine, RadioModel, MainWindow)
- Documentation (README, CLAUDE.md, CONTRIBUTING, STYLEGUIDE)
- CI workflow (GitHub Actions)
- Phase 1 analysis document stubs
