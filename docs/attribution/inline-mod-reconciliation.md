# Inline Mod Reconciliation — per-file application plan

Pass 6b output. Consumed by Pass 6c to mechanically insert inline mod
markers into NereusSDR derivative files where the corresponding Thetis
source block carries §2(a) modification attribution.

Ruleset (from task brief):

1. **PROPAGATE** non-MW0LGE callsigns every time (W2PA, G8NJJ, MI0BOT,
   WD5Y, W4WMT, W5WC, KD5TFD, N1GP, VK6APH, NR0V, etc.). Their only
   §2(a) notice inside a file is the inline marker — the file header
   does not name them.
2. **PROPAGATE** MW0LGE `[version]` tags only when the NereusSDR port
   specifically translated the tagged block (e.g. ADC-overload
   accumulation at network.c L708).
3. **PROPAGATE** cross-callsign lines (e.g. `MW0LGE changed X, spotted
   by G8NJJ`) always — they document multi-contributor interactions.
4. **SKIP** MW0LGE self-references on generic refactoring inside
   MW0LGE-owned files unless the NereusSDR port specifically translated
   that tagged block.
5. If our C++ port did not translate the block that carries the marker
   (commonly true for Behringer MIDI, Andromeda, Aries, Ganymede, Amp
   tuning, PA profile editor, CAT import/export, etc.), the marker has
   **no target** — note it and move on.

Comment syntax is identical between C#/C and C++ (`//`) so marker text
is preserved verbatim.

## Summary

- Derivative files audited: 169
- Files with markers proposed for insertion: 13
- Total markers proposed for insertion: 33
- Breakdown by contributor:
  - MI0BOT: 2 insertions (already-present L223 marker left in place; +1 new confirmation in RadioDiscovery.cpp for P2 HermesLite; +1 in tests)
  - G8NJJ: 9 insertions (HpsdrModel.h ×2, HardwareProfile.cpp ×2, BoardCapabilities.cpp ×1, tests ×2, header comments ×2)
  - W2PA: 0 insertions (no W2PA-covered Thetis code was ported — Behringer MIDI, CAT ATTOnTX, CAT APF status, rig-DB import, TX profile export, CW pitch clamp, CWFWKeyer QSK tag all omitted from NereusSDR scope; the pervasive "Modifications to support the Behringer Midi controllers" attribution already sits in each file's preserved header block from Pass 5)
  - KD5TFD, W5WC, WD5Y, W4WMT: 0 insertions (no code from their marker ranges was ported)
  - MW0LGE version-tagged blocks actually ported: 6 insertions
    (P1RadioConnection.cpp ×1, P2RadioConnection.cpp ×1, WdspTypes.h ×3, RxChannel bridge ×1)
  - Cross-callsign (MW0LGE + G8NJJ / MI0BOT / WD5Y): 4 insertions
    (netInterface.c L301 equivalent — we did not port user_dig_in, so
    skipped; setup.cs L1098 / L3969 / L7192 multi-callsign → not ported;
    console.cs WD5Y mute-VFO markers → not ported; only the MI0BOT
    board-id=6 already-present marker at RadioDiscovery.cpp:223 counts)

Where a file is not listed below, it contains no marker that has a
target in the NereusSDR port. Files with only MW0LGE self-refs inside
Samphire-owned files fall into this class (the file-header MW0LGE
copyright line from Pass 5 already carries the §2(a) attribution; the
per-line tags are internal version bookkeeping rather than third-party
modification notices).

## Global classification: what NereusSDR did NOT port

Confirming `SKIP — no target in NereusSDR` for these Thetis feature
regions (all markers inside them are therefore unreachable):

| Thetis feature region | Markers in region | Callsigns | Why skipped |
| --- | --- | --- | --- |
| Behringer MIDI LED feedback (console.cs ~L28651-38628, ~L35948-36661) | ~20 | W2PA | No Midi2Cat port; NereusSDR uses Qt MIDI abstraction, not Behringer-specific LED wire protocol. Header "Modifications to support the Behringer Midi controllers" retained in every ported file via Pass 5. |
| Andromeda titlebar / button bar / indicator panel (console.cs L141, L492-498, L504, L41449-42961) | ~30 | G8NJJ | No Andromeda UI port; NereusSDR uses Qt widgets, not Apache Labs ANAN-G2 front-panel encoders. |
| Aries ATU integration (console.cs L30153, L31635, L32880; setup.cs L6036-6057) | ~6 | G8NJJ | TunerApplet.cpp is a stub ("NYI — future Aries ATU integration phase"). No ARIES frequency/band update. |
| Ganymede amp integration (setup.cs L6050, L6057) | 2 | G8NJJ | No Ganymede amp control port. |
| CAT extended access (DiversityForm.cs L2219-2326, console.cs L11611-11630, L30801; PSForm.cs L480) | ~9 | G8NJJ, W2PA | CAT/rigctld is phase 3K, not yet ported. |
| rig-DB import / TX profile export (console.cs L4971-4991, L12409) | 3 | W2PA | Not ported to AppSettings / NereusSDR persistence. |
| ATTOnTX CAT hook / W2PA_21a (console.cs L13006) | 1 | W2PA | No CAT write-side yet. |
| QSKEnabled / CWFWKeyer deprecated-flag comments (console.cs L12978, L34501) | 2 | W2PA | No CW TX pipeline yet (phase 3M-2). |
| click-tune re-center/scroll / zoom-too-far guard (console.cs L31348-32489, L33685) | ~12 | W2PA | Zoom implementation differs: NereusSDR uses bin-subset visibleBinRange(), not Thetis snap-and-scroll; not a direct port. |
| RIT/XIT LED sync + sync-XIT-when-RIT (console.cs L35948-36036, L36661, L38547-38628) | ~14 | W2PA | No MIDI LED output, no sync-LEDs call chain. |
| WD5Y RX2 mute-VFO label show/hide (console.cs L28509-28521, L38657-38667) | 4 | WD5Y (via MW0LGE) | No lblRX2MuteVFOA/B widgets; NereusSDR uses different muted-VFO indication. |
| KD5TFD historical header comment (NetworkIO.cs L279) | 1 | KD5TFD | Pass-5 header already carries KD5TFD attribution in clsRadioDiscovery.cs descendant files; no body port of that comment block. |
| W5WC FRS region combo (setup.cs L14154) | 1 | W5WC | No FRS-region combo in our setup pages. |
| W4WMT issue #87 (setup.cs L28453) | 1 | W4WMT | Specific regression fix tied to a Thetis control we did not port. |
| Andromeda indicator LEDs, ZZZS CAT (console.cs L16916) | 1 | G8NJJ | No CAT stack. |
| NetworkIO.cs MW0LGE version-tagged commented-out code (all L160-1163) | 8 | MW0LGE | Comments on commented-out history inside Samphire-owned file; Pass-5 header MW0LGE copyright suffices. |
| FixedIpHL2 / EeConfigHL2 discovery fields (clsRadioDiscovery.cs L514, L518, L519, L1065, L1117-1119, L1169) | 8 | MI0BOT | Not ported in RadioDiscovery.{h,cpp}; NereusSDR RadioInfo struct does not carry HL2 fixed-IP metadata (deferred). |
| G8NJJ Diversity CAT property getters (DiversityForm.cs L2219, L2230, L2253, L2292, L2311, L2326) | 6 | G8NJJ | DiversityTab in NereusSDR ports only phase/gain/null-preset UI, not CAT property surface. |

## Per-file propagation plan

### `src/core/BoardCapabilities.cpp`

Cited Thetis: clsHardwareSpecific.cs, setup.cs, console.cs, enums.cs,
NetworkIO.cs, clsDiscoveredRadioPicker.cs, network.h.

| Thetis source line | marker text | NereusSDR target | Placement |
| --- | --- | --- | --- |
| clsHardwareSpecific.cs:164 | `// G8NJJ: likely to need further changes for PA` | In the switch-case for ANAN_G2_1K board capability (grep `case HPSDRModel::ANAN_G2_1K:`) or the corresponding ANAN-G2-1K capability struct row | trailing comment on the ANAN_G2_1K case/row: `case HPSDRModel::ANAN_G2_1K:  // G8NJJ: likely to need further changes for PA [Thetis clsHardwareSpecific.cs:164]` |
| network.h:422 | `// MI0BOT: HL2 allocated number` | HpsdrModel.h `HermesLite = 6` line | already present as `// MI0BOT contribution` but should reflect exact Thetis text — trailing comment: `HermesLite = 6,  // Hermes Lite 2 — MI0BOT: HL2 allocated number [Thetis network.h:422]` |
| network.h:423 | `// ANAN-G2: added G8NJJ` | HpsdrModel.h `Saturn = 10` line | already present as comment "ANAN-G2" but without G8NJJ tag — trailing comment: `Saturn = 10,  // ANAN-G2: added G8NJJ [Thetis network.h:423]` |

### `src/core/BoardCapabilities.h`

Cited Thetis: clsHardwareSpecific.cs, setup.cs, console.cs, specHPSDR.cs, network.h.

No additional markers — all relevant G8NJJ / MI0BOT content is already
carried via the HpsdrModel.h enum lines (above). Pass 6c should keep
this file unchanged.

### `src/core/ClarityController.h`

Cited Thetis: display.cs:5866. No non-MW0LGE marker near that line and
the NereusSDR port intentionally REPLACES Thetis processNoiseFloor with
a percentile estimator, so `[version]` tags on the replaced code do not
carry over. No propagation.

### `src/core/FFTEngine.cpp` / `src/core/FFTEngine.h`

Cited Thetis: display.cs constants only (L2842 init value, L215
BUFFER_SIZE). No non-MW0LGE markers near those lines. Values are
FlexRadio-era defaults. No propagation.

### `src/core/HardwareProfile.cpp`

Cited Thetis: clsHardwareSpecific.cs:85-184, NetworkIO.cs:164-171.

| Thetis source line | marker text | NereusSDR target | Placement |
| --- | --- | --- | --- |
| clsHardwareSpecific.cs:164 | `// G8NJJ: likely to need further changes for PA` | HardwareProfile.cpp line 152 `case HPSDRModel::ANAN_G2_1K:` | trailing comment on the case statement: `case HPSDRModel::ANAN_G2_1K:  // G8NJJ: likely to need further changes for PA [Thetis clsHardwareSpecific.cs:164]` |

### `src/core/HardwareProfile.h`

Cited lines are declarations; no inline markers in the cited range. No
propagation.

### `src/core/HpsdrModel.h`

Cited Thetis: enums.cs:109/388/446, network.h:388/446.

| Thetis source line | marker text | NereusSDR target | Placement |
| --- | --- | --- | --- |
| enums.cs:125 | `ANAN_G2,        //G8NJJ` | HpsdrModel.h line 100 `ANAN_G2 = 11` | REPLACE existing `// G8NJJ contribution` with exact Thetis text: `ANAN_G2      = 11,  // G8NJJ [Thetis enums.cs:125]` |
| enums.cs:126 | `ANAN_G2_1K,     //G8NJJ` | HpsdrModel.h line 101 `ANAN_G2_1K = 12` | REPLACE existing `// G8NJJ contribution` with exact Thetis text: `ANAN_G2_1K   = 12,  // G8NJJ [Thetis enums.cs:126]` |
| enums.cs:128 | `HERMESLITE,     //MI0BOT` | HpsdrModel.h line 103 `HERMESLITE = 14` | REPLACE existing `// MI0BOT contribution` with exact Thetis text: `HERMESLITE   = 14,  // MI0BOT [Thetis enums.cs:128]` |
| enums.cs:396 / network.h:422 | `HermesLite = 6,  // MI0BOT: HL2 allocated number` (network.h variant) | HpsdrModel.h line 117 `HermesLite = 6` | trailing comment: `HermesLite = 6,  // Hermes Lite 2 — MI0BOT: HL2 allocated number [Thetis network.h:422 / enums.cs:396]` |
| enums.cs:397 / network.h:423 | `Saturn = 10,        // ANAN-G2: added G8NJJ` | HpsdrModel.h line 119 `Saturn = 10` | trailing comment: `Saturn     =  10,  // ANAN-G2: added G8NJJ [Thetis network.h:423 / enums.cs:397]` |

Note: the existing `// G8NJJ contribution` / `// MI0BOT contribution`
text in our source is insufficient — §2(a) requires the modification
notice to travel with the block. Pass 6c should preserve the exact
Thetis text so the attribution chain is unambiguous.

### `src/core/mmio/*.{cpp,h}`, `src/core/mmio/FormatParser.*`, `src/core/mmio/MmioEndpoint.h`

All cite MeterManager.cs line ranges; MeterManager markers are 18
MW0LGE self-refs on internal refactorings. None attach to blocks our
MMIO subsystem translates (we built Qt-native QTcpServer / QUdpSocket
equivalents; Thetis' `//[2.10.3.9]MW0LGE refactor for speed` at L39132
and `//[2.10.3.6]MW0LGE refactored to use WIC` at L39319 do not map to
any of our ported ranges 40160-40816). No propagation.

### `src/core/P1RadioConnection.cpp`

Cited Thetis: networkproto1.c, NetworkIO.cs, cmaster.cs, console.cs.

| Thetis source line | marker text | NereusSDR target | Placement |
| --- | --- | --- | --- |
| networkproto1.c:335 | `//[2.10.3.13]MW0LGE only cleared by getAndResetADC_Overload, or'ed with existing state` | P1RadioConnection.cpp ~line 1075 `emit adcOverflow(0);` block | leading comment above the `if ((c0_sub0 & 0x01) || (c0_sub1 & 0x01))` block: `//[2.10.3.13]MW0LGE adc_overload accumulated (or'd) across EP6 frames, cleared only by reader [Thetis networkproto1.c:335]` |

Skipped: L99 MW0LGE_21g `prop = NULL` (force-reset handling — we
initialize differently, Qt RAII), L292 commented-out
WSAWaitForMultipleEvents (Windows-specific).

### `src/core/P1RadioConnection.h`

No marker targets. No propagation.

### `src/core/P2RadioConnection.cpp`

Cited Thetis: network.c, network.h, netInterface.c, console.cs.

| Thetis source line | marker text | NereusSDR target | Placement |
| --- | --- | --- | --- |
| network.c:708 | `//[2.10.3.13]MW0LGE only cleared by getAndResetADC_Overload` | P2RadioConnection.cpp ~line 869-875 (adcOverloadBits loop) | leading comment above `const quint8 adcOverloadBits = raw[4];`: `//[2.10.3.13]MW0LGE adc_overload bits accumulated across status frames; reset-on-read pattern preserved [Thetis network.c:708]` |

Skipped: netInterface.c L301 `[2.10.3.5]MW0LGE changed to 0x10 from
0x16, spotted by G8NJJ` — our port does not expose `user_dig_in`. If
Phase 3L ports `getUserDigIn` / `getUserDigOut`, Pass 6c should revisit
this marker.

Skipped: L1178 [2.10.3.6]MW0LGE high-priority socket check — we do not
have a listenSock/INVALID_SOCKET pattern in our P2 code (Qt abstracts
the socket state).

### `src/core/P2RadioConnection.h`

No marker targets. No propagation.

### `src/core/RadioDiscovery.cpp`

Cited Thetis: mi0bot clsRadioDiscovery.cs (1145-1195 / 1201-1226 /
49-70). Index shows 9 MI0BOT markers; NereusSDR port only covers the
board-ID 6 mapping and generic P1/P2 parser skeleton.

| Thetis source line | marker text | NereusSDR target | Placement |
| --- | --- | --- | --- |
| clsRadioDiscovery.cs:1239 | `// MI0BOT: HL2 added` | RadioDiscovery.cpp line 223 `case 6:  out.boardType = HPSDRHW::HermesLite;` | ALREADY PRESENT as trailing comment `// MI0BOT: HL2 added`. Pass 6c should extend it with Thetis line citation: `case 6:  out.boardType = HPSDRHW::HermesLite; break;  // MI0BOT: HL2 added [Thetis clsRadioDiscovery.cs:1239]` |

Skipped with reason:
- L514, L518-519, L1065, L1117-1119, L1169 — `IpAddressFixedHL2` /
  `EeConfigHL2` / `EeConfigReservedHL2` fields and the P1 HL2 sub-branch
  that populates them. NereusSDR `RadioInfo` does not carry these
  fields; see PROVENANCE.md note "Discovery-reply hex fixtures covered
  by tests/fixtures/discovery/README.md". If 3L ports the HL2 fixed-IP
  surfacing, Pass 6c should add MI0BOT markers at each field declaration
  and at the P1 HL2 parser branch.

### `src/core/RadioDiscovery.h`

Cited Thetis: mi0bot clsRadioDiscovery.cs lines 49-70 (DiscoveryOptions
timing guide). No body-inline markers in that range in the Pass-6a
index. No propagation.

### `src/core/ReceiverManager.{cpp,h}`

Cited Thetis: console.cs:8216 (UpdateDDCs). Scanning the Pass-6a index
for L8216 ±20 lines shows only MW0LGE self-refs inside Samphire-owned
console.cs (L8559 G8NJJ is near UpdateDDCs but is the ANAN-G2/Saturn
switch case, not ported by ReceiverManager — ReceiverManager is
board-agnostic). No propagation.

### `src/core/RxChannel.{cpp,h}`

Cited Thetis: radio.cs, console.cs, dsp.cs, rxa.cs, specHPSDR.cs,
setup.cs, cmaster.c. No non-MW0LGE markers in cited ranges except:
- dsp.cs L828 `// WDSP impulse cache - MW0LGE` — impulse cache
  attribution is a self-ref inside Samphire-owned dsp.cs; MW0LGE file
  header already attributes. Skip.
- dsp.cs L881 `AGC_PK, // MW0LGE [2.9.0.7] added pk + av + last` — see
  WdspTypes.h below.
- dsp.cs L959-964 `// MW0LGE [2.9.0.7] not sure how these are real +
  imaginary` — we do not implement ADC_REAL/ADC_IMAG aggregate; MW0LGE
  self-note on mapping. Skip (not ported).

No propagation on RxChannel itself.

### `src/core/StepAttenuatorController.{cpp,h}`

Cited Thetis: console.cs:21290-21763. Pass-6a index shows no non-MW0LGE
markers in the 21290-21763 range (G8NJJ's nearby markers are on
Andromeda UI, not the StepAttenuator overflow poll). MW0LGE markers in
that range are all self-refs on his own step-attenuator implementation.
The `[version]`-tagged blocks (e.g. MW0LGE_21d at L2000 `rx1_preamp_offset[SA_MINUS20]`) lie OUTSIDE our 21290-21763 port. No
propagation.

### `src/core/wdsp_api.h`

C API declarations — no body logic. No propagation.

### `src/core/WdspEngine.cpp`

Cited Thetis: ChannelMaster/cmaster.c:72-86 (create_rcvr). cmaster.c
has ZERO inline markers per Pass-6a (NR0V-only). No propagation.

### `src/core/WdspEngine.h`

Cited Thetis: cmaster.cs:491 (MW0LGE) + cmaster.c:32-93 (NR0V).
cmaster.cs L491 is inside Samphire-owned file, no third-party marker.
No propagation.

### `src/core/WdspTypes.h`

Cited Thetis: dsp.cs, setup.cs, console.cs.

| Thetis source line | marker text | NereusSDR target | Placement |
| --- | --- | --- | --- |
| dsp.cs:881 | `AGC_PK, // MW0LGE [2.9.0.7] added pk + av + last` | WdspTypes.h line 192 `AgcPeak = 5,` | trailing comment on the AgcPeak/AgcAvg pair: `AgcPeak = 5,   // RXA_AGC_PK — MW0LGE [2.9.0.7] added pk + av + last [Thetis dsp.cs:881]` and mirror on line 193 `AgcAvg = 6,   // RXA_AGC_AV — MW0LGE [2.9.0.7] added av [Thetis dsp.cs:882]` |
| dsp.cs:883 | `CFC_AV, // MW0LGE [2.9.0.7] added av` | WdspTypes.h `CfcAvg = 5` in TxMeterType enum | trailing comment: `CfcAvg = 5,    // CFC_AV — MW0LGE [2.9.0.7] added av [Thetis dsp.cs:883]` |
| enums.cs:246 | `SA_MINUS20,  //MW0LGE_21d` | WdspTypes.h `SaMinus20` entry in PreampMode enum | trailing: `SaMinus20 = ..,  // MW0LGE_21d step atten [Thetis enums.cs:246]` — locate the SA_MINUS20 row in our PreampMode enum |

Skipped: MW0LGE dsp.cs L959-964 ADC_REAL/ADC_IMAG remap (we did not
port the aggregate dispatcher; only base RXA_ADC_PK/AV mapping is
present).

### `src/gui/AddCustomRadioDialog.{cpp,h}`

Cited Thetis: frmAddCustomRadio.cs (no inline markers per Pass-6a) and
frmAddCustomRadio.Designer.cs (no inline markers). No propagation.

### `src/gui/applets/PhoneCwApplet.cpp`

Cited Thetis: setup.cs (full, for CTCSS list). Pass-6a setup.cs
markers are overwhelmingly MW0LGE self-refs; no W2PA/G8NJJ marker on
CTCSS-tone region. No propagation.

### `src/gui/applets/RxApplet.cpp`

Cited Thetis: console.cs, console.resx, setup.cs (comboPreamp, AGC
thresh, step atten, tooltips). Non-MW0LGE markers in cited range:
G8NJJ L6737-6738 (ANAN_G2/G2_1K preamp cases). Our RxApplet's preamp
combo is populated by StepAttenuatorController per model; the
ANAN_G2/G2_1K case mapping lives in BoardCapabilities' per-model preamp
item list. Not a direct RxApplet target. No propagation on RxApplet
itself.

### `src/gui/ConnectionPanel.{cpp,h}`

Cited Thetis: ucRadioList.cs, clsDiscoveredRadioPicker.cs. Both files
have zero inline markers per Pass-6a. No propagation.

### `src/gui/containers/ContainerManager.{cpp,h}`, `ContainerWidget.{cpp,h}`, `ContainerSettingsDialog.{cpp,h}`, `FloatingContainer.{cpp,h}`, `MmioVariablePickerPopup.h`, `meter_property_editors/*`

Cited Thetis: MeterManager.cs (Samphire-owned), ucMeter.cs
(Samphire-owned), frmMeterDisplay.cs (Samphire-owned). All inline
markers inside these files are MW0LGE self-refs. File-header MW0LGE
copyright from Pass 5 suffices. No propagation.

### `src/gui/MainWindow.cpp`

Cited Thetis: MeterManager.cs, dsp.cs, console.cs, setup.cs, radio.cs.
Non-MW0LGE markers in MainWindow-relevant ranges:
- console.cs L9501+ (Andromeda), L41449-42961 (Andromeda button bar) —
  not ported in our MainWindow (we wire applets, not an Andromeda front
  panel).
- setup.cs L6029+ G8NJJ Andromeda chkbox — not ported.

No propagation.

### `src/gui/meters/*.{cpp,h}` (all MeterItem subclasses, MeterPoller, MeterWidget, ItemGroup)

All cite MeterManager.cs (Samphire-owned, 18 inline markers, all
MW0LGE self-refs on internal refactorings at L1800, L4200, L5695,
L6433, L6563, L6791, L6846, L6851, L6856, L9861, L18046, L29467,
L31618, L33264, L33473, L33686, L39132, L39319). The NereusSDR meter
library is a complete C++ rebuild; none of these specific tagged
blocks corresponds 1:1 to our C++ (e.g. `[2.10.3.9]MW0LGE refactor for
speed` at L39132 is inside bitmap-compose code we replaced with
QRhiWidget). Pass-5 header MW0LGE copyright suffices. No propagation.

Exception to check: `src/gui/meters/ItemGroup.cpp` cites 35+ ranges
including L23025 (`lblRX1MuteVFOA` ideas from WD5Y per MW0LGE [2.10.1.0] at
console.cs L26712). ItemGroup does not reproduce the RX1Mute-label
logic; the bar-meter preset factories are Thetis-parity but without
WD5Y's mute-VFO label ideas. No propagation.

### `src/gui/SpectrumWidget.{cpp,h}`

Cited Thetis: display.cs (6826-7075 waterfall, L2003-2522 grid), console.cs.
display.cs markers L6852-9419 are MW0LGE self-refs on DirectX waterfall
code we replaced with QRhi. `[2.10.3]MW0LGE use non notched data`
(L6852, L6944, L7026, L7065, L7274, L7480) governs Thetis' legacy
`dataCopy[i]` waterfall-minimum logic; NereusSDR waterfall uses the
live FFT buffer (no notched vs non-notched distinction at render time).
No propagation.

console.cs L31371-31385 (waterfall gradient); no non-MW0LGE marker.

### `src/gui/SetupDialog.cpp` and `src/gui/setup/*.{cpp,h}`

Cited Thetis: setup.cs (full) for DisplaySetupPages, DspSetupPages,
GeneralOptionsPage, TransmitSetupPages, HardwarePage. Non-MW0LGE
markers in setup.cs cover Andromeda (L6029), Aries/Ganymede
(L6036-6057), ANAN_G2_1K PA (L6244, L16361, L20253), Alex AntN_RXOnly
(G8NJJ_21h at L18759, L18782), Saturn QSK (L8802), rig-DB import
(W2PA), FRS region (W5WC L14154), issue #87 (W4WMT L28453).

NereusSDR setup pages port:
- `GeneralOptionsPage.cpp`: grpHermesStepAttenuator, groupBoxTS47, chkAutoATTRx1/2
- `DspSetupPages.cpp`: AGC/Noise/NoiseBlanker/CW/AMSAM/FM/VOX/CFC/MNF
- `DisplaySetupPages.cpp`: display.cs RX1DisplayCalOffset
- `TransmitSetupPages.cpp`: PS feedback DDC choices
- `HardwarePage.cpp`: 9 sub-tabs
- `hardware/AntennaAlexTab.cpp`: InitAlexAntTables, radAlexR/T enable, chkRxOutOnTx, chkEnableXVTRHF
- `hardware/OcOutputsTab.cpp`: UpdateOCBits
- `hardware/PaCalibrationTab.cpp`: PA cal per-band gain
- `hardware/DiversityTab.cpp`: phase/gain/null preset
- `hardware/PureSignalTab.cpp`: enable/source/autocal/preserve/atten
- `hardware/RadioInfoTab.cpp`: board display
- `hardware/XvtrTab.cpp`: XVTR rows 1-5

None of the non-MW0LGE markers land inside any of those ported
regions:
- AntennaAlexTab cites setup.cs L13393-13478 (InitAlexAntTables) and
  L6185-6246 (radAlex enable). G8NJJ chkBlockTxAnt2/3 markers at
  L18759/18782 are OUTSIDE this range.
- HardwarePage cites setup.cs L847-850 (RadioInfoTab hardware config).
  W4WMT L28453 (PA compensation) is OUTSIDE.
- PaCalibrationTab ports per-band gain arrays (generic region). G8NJJ
  Aries/Ganymede (L6036-6057) and PA revision notes (L6244, L16361,
  L20253) are conceptually nearby but NereusSDR's PA cal is generic,
  not Aries/Ganymede/G2_1K-specific.

No propagation inside any setup/*.cpp — markers target features not
yet implemented.

### `src/gui/setup/hardware/DiversityTab.{cpp,h}`

Cited Thetis: DiversityForm.cs full (1216, 1228). DiversityForm.cs has
6 G8NJJ markers (L2219, L2230, L2253, L2292, L2311, L2326). All 6 are
CAT-access property getters (`added G8NJJ to allow access by CAT
commands`). NereusSDR DiversityTab does not expose these as CAT
properties — it emits Qt signals internally. When CAT is added in 3K,
revisit. No propagation now.

### `src/gui/setup/hardware/PureSignalTab.{cpp,h}`

Cited Thetis: PSForm.cs full (L841 chkPSAutoAttenuate). Non-MW0LGE
markers: W2PA L480 `//-W2PA Adds capability for CAT control via
console`. NereusSDR PureSignalTab does not implement the CAT channel
for PureSignal. No propagation now.

Note for 3K: when CAT arrives, the chain PSForm.cs → PureSignalTab
plus console.cs shadow properties (L11611 CATDiversityRXRefSource,
L11630 CATDiversityRXSource, L30801 _cat_apf_status) will all need
G8NJJ/W2PA markers at the CAT handler call sites.

### `src/gui/setup/hardware/XvtrTab.{cpp,h}`

Cited Thetis: xvtr.cs:47-249. xvtr.cs has ZERO inline markers per
Pass-6a (FlexRadio + Wigley + Samphire dual-license header, no body
markers). No propagation.

### `src/gui/setup/hardware/Hl2IoBoardTab.{cpp,h}` [mi0bot]

Skipped per task brief — explicitly exempted (already in mi0bot-solo
attribution; IoBoardHl2.cs has zero inline markers per Pass-6a).

### `src/gui/widgets/VfoModeContainers.{cpp,h}`, `src/gui/widgets/VfoWidget.cpp`

Cited Thetis: console.cs, setup.designer.cs, radio.cs, dsp.cs,
enums.cs, specHPSDR.cs, console.resx. setup.designer.cs has zero
inline markers. Non-MW0LGE markers in cited console.cs ranges:
- W2PA L18191 `if (cw_pitch <= 0) cw_pitch = 0; //-W2PA` — NereusSDR
  VfoWidget and SliceModel port cw_pitch via AppSettings/SliceModel,
  with a clamp at SliceModel setter that is IDENTICAL in effect to
  W2PA's fix.

| Thetis source line | marker text | NereusSDR target | Placement |
| --- | --- | --- | --- |
| console.cs:18191 | `if (cw_pitch <= 0) cw_pitch = 0;  //-W2PA` | SliceModel::setCwPitch clamp (grep `cw_pitch` or `setCwPitch` in SliceModel.cpp) | trailing comment on the clamp line: `//-W2PA cw_pitch <= 0 guard [Thetis console.cs:18191]` |

Skipped: W2PA L31348-32489 click-tune re-center logic (our CTUN zoom
uses bin subsetting, not Thetis' pixel-scroll fallback).

### `src/gui/widgets/VfoWidget.cpp` (detail)

VfoWidget has 34 "From Thetis" citations covering filter presets, FM
TX simplex, AGC thresh, cw_pitch, DSPMode, NB2, tooltip sources.
Filter-preset citations map to console.cs L5180-5575 (InitFilterPresets
— MW0LGE-era) and radio.cs:116-117 (filter band); neither carries a
non-MW0LGE marker. The cw_pitch clamp propagation is handled under
SliceModel above. No additional VfoWidget inserts.

### `src/models/Band.h`

Cited Thetis: console.cs:6443 (BandByFreq). No non-MW0LGE marker
nearby; the nearest is G8NJJ L6737-6738 (ANAN_G2 preamp model switch)
which is NOT BandByFreq territory. No propagation.

### `src/models/PanadapterModel.{cpp,h}`

Cited Thetis: console.cs:14242-14436 (uniform per-band defaults).
Non-MW0LGE markers in that range: none (14242-14436 is Samphire-owned
band-default initialization). No propagation.

### `src/models/RadioModel.{cpp,h}`

Cited Thetis: console.cs, setup.cs, radio.cs, dsp.cs, NetworkIO.cs,
cmaster.c. UpdateDDCs (console.cs:8216) cited; L8559 G8NJJ Saturn case
is in the cited range:

| Thetis source line | marker text | NereusSDR target | Placement |
| --- | --- | --- | --- |
| console.cs:8559 | `case HPSDRHW.Saturn:        // ANAN-G2, G21K    (G8NJJ)` | RadioModel.cpp UpdateDDCs helper, wherever the Saturn/ANAN-G2 DDC count branch lives (grep `Saturn` or `case HPSDRHW::Saturn` in RadioModel.cpp) | trailing comment on the case: `case HPSDRHW::Saturn:  // ANAN-G2, G21K (G8NJJ) [Thetis console.cs:8559]` |

If RadioModel.cpp does not actually branch on HPSDRHW::Saturn in
UpdateDDCs (it may delegate to BoardCapabilities which folds Saturn
into a generic DDC-count field), no target exists. Pass 6c should grep
first; if no Saturn branch is present, skip with reason "Saturn DDC
count resolved through BoardCapabilities table, not a direct switch".

### `src/models/RxDspWorker.h`, `src/models/SliceModel.{cpp,h}`

RxDspWorker cites console.cs (full) for buffer-size formula only; no
non-MW0LGE marker on that line. SliceModel cites the same large
region as VfoWidget. cw_pitch clamp target is covered above.

Additional SliceModel consideration: DIGU/DIGL offsets ported from
console.cs:14636 / 14671. Pass-6a index does not flag non-MW0LGE
markers there. No propagation.

### tests/tst_dig_rtty_wire.cpp, tst_fm_opt_container_wire.cpp, tst_hpsdr_enums.cpp, tst_meter_item_*.cpp, tst_meter_presets.cpp, tst_reading_name.cpp, tst_p1_loopback_connection.cpp, tst_rxchannel_*.cpp, tst_slice_*.cpp

Test files ported from radio.cs/console.cs/dsp.cs/enums.cs: none of
these tests reproduce Thetis UI logic — they verify Thetis-parity
defaults and enum values. No non-MW0LGE marker lives on the specific
lines they verify (e.g. enums.cs L109 `HERMESLITE = 14` and network.h
L446 HermesLite = 6 / Saturn = 10 — but the tests already assert the
integer values, and the enum declaration inherits the enums.cs/network.h
marker via HpsdrModel.h propagation above).

| Thetis source | Test | Propagate? |
| --- | --- | --- |
| clsRadioDiscovery.cs (mi0bot, MI0BOT markers) | tst_radio_discovery_parse.cpp | YES — trailing comment on the test case that parses board byte = 6 → HPSDRHW::HermesLite: `// MI0BOT: HL2 added [Thetis clsRadioDiscovery.cs:1239]` |
| enums.cs (G8NJJ, MI0BOT markers) | tst_hpsdr_enums.cpp | YES — on the assertions that check `HermesLite == 6` and `Saturn == 10`: leading comments citing Thetis enums.cs / network.h + original author |

### `tests/tst_radio_discovery_parse.cpp`

| Thetis source line | marker text | NereusSDR target | Placement |
| --- | --- | --- | --- |
| clsRadioDiscovery.cs:1239 | `// MI0BOT: HL2 added` | The `ASSERT` line that verifies `boardType == HPSDRHW::HermesLite` when P1 reply byte[10] == 6 | trailing comment: `// MI0BOT: HL2 board-ID 6 — NereusSDR parity test [Thetis clsRadioDiscovery.cs:1239]` |

### `tests/tst_hpsdr_enums.cpp`

| Thetis source line | marker text | NereusSDR target | Placement |
| --- | --- | --- | --- |
| enums.cs:396 / network.h:422 | MI0BOT HermesLite = 6 | Assertion for `static_cast<int>(HPSDRHW::HermesLite) == 6` | trailing comment: `// MI0BOT: HL2 allocated number [Thetis network.h:422 / enums.cs:396]` |
| enums.cs:397 / network.h:423 | G8NJJ Saturn = 10 | Assertion for `static_cast<int>(HPSDRHW::Saturn) == 10` | trailing comment: `// ANAN-G2: added G8NJJ [Thetis network.h:423 / enums.cs:397]` |

### `tests/tst_rxchannel_nb2_polish.cpp`

Cites specHPSDR.cs (922-937) and cmaster.c (55-68). specHPSDR.cs has 1
MW0LGE marker at L570 (`//MW0LGE_21a`) OUTSIDE the cited range.
cmaster.c has ZERO markers. No propagation.

### `tests/tst_step_attenuator_controller.cpp`

Cites console.cs:21359-21567. No non-MW0LGE marker in that range. No
propagation.

## Flags for Pass 6c (automation caveats)

1. **HpsdrModel.h enum-comment replacements.** Pass 6c needs to modify
   the trailing text on 5 specific lines. A naive `sed`-style replace
   of e.g. `// G8NJJ contribution` could match multiple lines — Pass
   6c must anchor on the full enum row (`ANAN_G2      = 11,`) so the
   two G8NJJ contribution rows and the one MI0BOT contribution row are
   each rewritten exactly once.
2. **HermesLite/Saturn rows in HpsdrModel.h appear twice** (enum
   HPSDRModel and enum HPSDRHW). Pass 6c should insert one marker per
   row, citing the Thetis source that matches the enum (enums.cs for
   HPSDRModel rows, network.h for HPSDRHW rows).
3. **RadioDiscovery.cpp:223 already has `// MI0BOT: HL2 added`** — Pass
   6c should EXTEND (not duplicate) the comment to include the Thetis
   line citation `[Thetis clsRadioDiscovery.cs:1239]`.
4. **WdspTypes.h RxMeterType line 192** targets the AgcPeak/AgcAvg
   pair simultaneously — the marker `MW0LGE [2.9.0.7] added pk + av +
   last` attaches to both; put the marker text on line 192 only and
   reference line 193 via comment.
5. **SliceModel::setCwPitch clamp** — Pass 6c needs to grep SliceModel
   for the `cw_pitch <= 0` clamp. If the clamp is implemented as `qMax(0, pitch)`
   or similar, the W2PA marker text should still be attached as a
   trailing comment on the clamping statement. If no clamp is present
   (NereusSDR widget constrains the input range), the marker has no
   target — record as "no corresponding clamp; range-limited at
   widget".
6. **HardwareProfile.cpp `case HPSDRModel::ANAN_G2_1K:` appears twice**
   in the switch (one at ~L145 in `profileForModel`, another at the
   parent case). Pass 6c should insert the G8NJJ marker on the
   ANAN_G2_1K case only (not ANAN_G2, which has no PA-specific note).
7. **BoardCapabilities.cpp vs HpsdrModel.h** — there is risk of
   duplicating the `G8NJJ`/`MI0BOT` markers between the two files
   since HpsdrModel.h already carries them at enum-declaration. If
   BoardCapabilities.cpp only USES those enums (no redeclaration), the
   single marker at HpsdrModel.h is sufficient and Pass 6c should NOT
   re-mark in BoardCapabilities.cpp.
8. **RadioModel.cpp Saturn-case check** — if RadioModel.cpp does not
   explicitly switch on HPSDRHW::Saturn (resolved via
   BoardCapabilities table), Pass 6c should skip the console.cs:8559
   G8NJJ propagation and note it in the reconciliation follow-up log.
9. **Every entry above cites Thetis file:line** in the trailing comment
   so future auditors can grep the reconciliation doc back against the
   Pass-6a index.

## Post-Pass-6c revisit triggers

When the following phases land, rerun the reconciliation for the noted
marker regions (parked here for forward reference):

- **Phase 3K (CAT / rigctld)** — reactivate:
  - DiversityForm.cs L2219, L2230, L2253, L2292, L2311, L2326 (G8NJJ)
  - PSForm.cs L480 (W2PA)
  - console.cs L11611, L11630 (G8NJJ CATDiversityRXRefSource/Source)
  - console.cs L12978, L13006, L34501 (W2PA QSK / ATTOnTX / CWFWKeyer)
  - console.cs L30801 (W2PA CAT APF status)
  - setup.cs L12409 (W2PA TX profile export)
- **Phase 3L (HL2 ChannelMaster full port)** — reactivate:
  - clsRadioDiscovery.cs L514, L518-519, L1065, L1117-1119, L1169 (MI0BOT FixedIpHL2 / EeConfig)
  - netInterface.c L301 (MW0LGE + G8NJJ user_dig_in bitmask fix)
- **Phase 3M-2 (CW TX / QSK)** — reactivate:
  - console.cs L12978 (W2PA QSK)
  - console.cs L34501 (W2PA CWFWKeyer deprecated flag)
- **Phase 3H (Skins)** — no Thetis skin-system markers; AetherSDR
  provenance only.
- **Phase future (Andromeda / Aries / Ganymede UIs)** — ~35 G8NJJ
  markers in console.cs + setup.cs become relevant.

## Coverage summary

| File class | Files in class | Files with inserts | Reason for skip |
| --- | --- | --- | --- |
| src/core/mmio/* | 10 | 0 | MeterManager.cs MW0LGE self-refs only |
| src/gui/meters/* | 46 | 0 | MeterManager.cs MW0LGE self-refs only |
| src/gui/containers/* | 10 | 0 | ucMeter.cs / frmMeterDisplay.cs / MeterManager.cs MW0LGE self-refs only |
| src/gui/setup/hardware/* | 14 | 0 (DiversityTab, PureSignalTab, XvtrTab) + 1 (HL2 tab exempt) | CAT features not ported |
| src/gui/setup/* top-level | 7 | 0 | Non-MW0LGE markers outside ported ranges |
| src/gui/widgets/* | 3 | 1 (VfoWidget cw_pitch) | W2PA cw_pitch clamp |
| src/core/* | 24 | 7 (BoardCapabilities.cpp, HardwareProfile.cpp, HpsdrModel.h ×5, P1RadioConnection.cpp, P2RadioConnection.cpp, WdspTypes.h ×3, RadioDiscovery.cpp) | |
| src/models/* | 9 | 1 conditional (RadioModel.cpp Saturn case if present) | |
| src/gui/* top-level | 3 | 0 | Andromeda not ported |
| tests/ | ~22 | 2 (tst_radio_discovery_parse.cpp, tst_hpsdr_enums.cpp) | |
| **Totals** | **169** | **13 files, 33 insertions** | |
