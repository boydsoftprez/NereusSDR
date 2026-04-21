# Thetis Provenance — NereusSDR derived-file inventory

This document catalogs every NereusSDR source file derived from, translated
from, or materially based on Thetis (ramdor/Thetis), its mi0bot/Thetis-HL2
fork, or WDSP (TAPR/OpenHPSDR-wdsp). Per-file license headers live in the
source files themselves; this index is the grep-able summary.

NereusSDR as a whole is distributed under GPLv3 (root `LICENSE`), which is
compatible with Thetis's GPLv2-or-later terms.

## Legend

Derivation type:
- `port`       — direct reimplementation in C++/Qt6 of a Thetis source file
- `reference`  — consulted for behavior during independent implementation
- `structural` — architectural template with substantive behavioral echo
- `wrapper`    — thin C++ wrapper around vendored C source (WDSP)

Template variant (see `HEADER-TEMPLATES.md`):
- `thetis-samphire`     — Thetis source contains Samphire contributions
- `thetis-no-samphire`  — Thetis source has no Samphire contributions
- `mi0bot`              — sourced from mi0bot/Thetis-HL2 fork
- `multi-source`        — synthesizes multiple Thetis sources

## Files derived from ramdor/Thetis

| NereusSDR file | Thetis source | Line ranges | Type | Variant | Notes |
| --- | --- | --- | --- | --- | --- |
| src/core/AppSettings.cpp | Project Files/Source/Console/database.cs | full | port | multi-source | XML key/value semantics (PascalCase keys, True/False string booleans, per-StationName nesting) port Thetis database.cs SaveVarsDictionary / RestoreVarsDictionary pattern; QXmlStream file I/O skeleton follows AetherSDR `src/core/AppSettings.{h,cpp}` (AetherSDR has no per-file header — project-level GPLv3 per https://github.com/ten9876/AetherSDR) |
| src/core/AppSettings.h | Project Files/Source/Console/database.cs | full | port | multi-source | header mirrors .cpp multi-source mix |
| src/core/BoardCapabilities.cpp | Project Files/Source/Console/clsHardwareSpecific.cs; Project Files/Source/Console/setup.cs; Project Files/Source/Console/console.cs; Project Files/Source/Console/enums.cs; Project Files/Source/Console/HPSDR/NetworkIO.cs; Project Files/Source/Console/clsDiscoveredRadioPicker.cs; Project Files/Source/ChannelMaster/network.h | full | port | multi-source | synthesizes Samphire-clean LGPL ChannelMaster with Samphire-heavy console files; explicitly cites mi0bot fork for HL2 specifics |
| src/core/BoardCapabilities.h | Project Files/Source/Console/clsHardwareSpecific.cs; Project Files/Source/Console/setup.cs; Project Files/Source/Console/console.cs; Project Files/Source/Console/HPSDR/specHPSDR.cs; Project Files/Source/ChannelMaster/network.h | full | port | multi-source | header mirrors .cpp multi-source mix |
| src/core/codec/AlexFilterMap.cpp | Project Files/Source/Console/console.cs | 6830-6942; 7168-7234 | port | thetis-samphire | freq→Alex HPF/LPF bits, lifted from P2RadioConnection inline copy into shared header so P1 can call it too |
| src/core/codec/AlexFilterMap.h | Project Files/Source/Console/console.cs | 6830-6942; 7168-7234 | port | thetis-samphire | header mirrors .cpp |
| src/core/codec/P1CodecStandard.cpp | Project Files/Source/ChannelMaster/networkproto1.c | 419-698 | port | thetis-no-samphire | ramdor WriteMainLoop port; lifted from P1RadioConnection::composeCcForBank |
| src/core/codec/P1CodecStandard.h | Project Files/Source/ChannelMaster/networkproto1.c | 419-698 | port | thetis-no-samphire | header mirrors .cpp |
| src/core/codec/P1CodecAnvelinaPro3.cpp | Project Files/Source/ChannelMaster/networkproto1.c | 668-674; 682 | port | thetis-no-samphire | extends P1CodecStandard with bank 17 extra OC; AnvelinaPro3-only |
| src/core/codec/P1CodecAnvelinaPro3.h | Project Files/Source/ChannelMaster/networkproto1.c | 668-674 | port | thetis-no-samphire | header mirrors .cpp |
| src/core/codec/P1CodecRedPitaya.cpp | Project Files/Source/ChannelMaster/networkproto1.c | 606-616 | port | thetis-no-samphire | extends P1CodecStandard with bank 12 ADC1 MOX carve-out (DH1KLM contribution) |
| src/core/codec/P1CodecRedPitaya.h | Project Files/Source/ChannelMaster/networkproto1.c | 606-616 | port | thetis-no-samphire | header mirrors .cpp |
| src/core/codec/P1CodecHl2.cpp | Project Files/Source/ChannelMaster/networkproto1.c | 869-1201 | port | mi0bot | mi0bot WriteMainLoop_HL2 port; HL2-only codec; bank 11 6-bit ATT (0x3F) + 0x40 enable + MOX branch fixes the reported S-ATT bug; banks 17-18 HL2-specific |
| src/core/codec/P1CodecHl2.h | Project Files/Source/ChannelMaster/networkproto1.c | 869-1201 | port | mi0bot | header mirrors .cpp |
| src/core/codec/P2CodecOrionMkII.cpp | Project Files/Source/ChannelMaster/network.c | 821-1248 | port | thetis-no-samphire | base P2 codec for OrionMKII / 7000D / 8000D / AnvelinaPro3; lifted from P2RadioConnection inline compose helpers |
| src/core/codec/P2CodecOrionMkII.h | Project Files/Source/ChannelMaster/network.c | 821-1248 | port | thetis-no-samphire | header mirrors .cpp |
| src/core/codec/P2CodecSaturn.cpp | Project Files/Source/ChannelMaster/network.c; Project Files/Source/Console/console.cs | 821-1248; 6944-7040 | port | multi-source | extends P2CodecOrionMkII with G8NJJ Saturn BPF1 band-edge override (setBPF1ForOrionIISaturn); console.cs has Samphire dual-license stanza |
| src/core/codec/P2CodecSaturn.h | Project Files/Source/ChannelMaster/network.c; Project Files/Source/Console/console.cs | 821-1248; 6944-7040 | port | multi-source | header mirrors .cpp |
| src/core/ClarityController.h | Project Files/Source/Console/display.cs | 5866 | port | thetis-samphire | replaces Thetis processNoiseFloor with percentile-based estimator; cites Thetis as lineage |
| src/core/FFTEngine.cpp | Project Files/Source/Console/display.cs | 2842 | port | thetis-samphire | constant reference only (-200 dBm initial value) |
| src/core/FFTEngine.h | Project Files/Source/Console/display.cs | 215 | port | thetis-samphire | BUFFER_SIZE = 16384 constant + enum mapping |
| src/core/HardwareProfile.cpp | Project Files/Source/Console/clsHardwareSpecific.cs; Project Files/Source/Console/HPSDR/NetworkIO.cs | 85-184; 164-171 | port | thetis-samphire | |
| src/core/HardwareProfile.h | Project Files/Source/Console/clsHardwareSpecific.cs; Project Files/Source/Console/HPSDR/NetworkIO.cs | 85-184; 164-171 | port | thetis-samphire | |
| src/core/HpsdrModel.h | Project Files/Source/Console/enums.cs; Project Files/Source/ChannelMaster/network.h | 109; 388; 446 | port | multi-source | verbatim enum integer values; mi0bot/Thetis@Hermes-Lite citation is author viewpoint — files exist in ramdor/Thetis proper |
| src/core/mmio/ExternalVariableEngine.cpp | Project Files/Source/Console/MeterManager.cs | 40831-40834 | port | thetis-samphire | Qt-native engine implementing the .h's MultiMeterIO registry interface |
| src/core/mmio/ExternalVariableEngine.h | Project Files/Source/Console/MeterManager.cs | 40831-40834 | port | thetis-samphire | Qt-native re-expression of MultiMeterIO static registry |
| src/core/mmio/FormatParser.cpp | Project Files/Source/Console/MeterManager.cs | 41344-41349 | port | thetis-samphire | RAW parser |
| src/core/mmio/FormatParser.h | Project Files/Source/Console/MeterManager.cs | 41325-41422 | port | thetis-samphire | JSON / XML / RAW parser family |
| src/core/mmio/MmioEndpoint.h | Project Files/Source/Console/MeterManager.cs | 40320-40353 | port | thetis-samphire | clsMMIO payload format enum |
| src/core/mmio/SerialEndpointWorker.cpp | Project Files/Source/Console/MeterManager.cs | 40589-40816 | port | thetis-samphire | SerialPortHandler, Open, DataReceived |
| src/core/mmio/SerialEndpointWorker.h | Project Files/Source/Console/MeterManager.cs | 40589-40816 | port | thetis-samphire | |
| src/core/mmio/TcpClientEndpointWorker.cpp | Project Files/Source/Console/MeterManager.cs | 40160-40403 | port | thetis-samphire | TcpClientHandler, reconnect delay, newline framing |
| src/core/mmio/TcpClientEndpointWorker.h | Project Files/Source/Console/MeterManager.cs | 40160-40403 | port | thetis-samphire | |
| src/core/mmio/TcpListenerEndpointWorker.cpp | Project Files/Source/Console/MeterManager.cs | 39899-40160 | port | thetis-samphire | TcpListener |
| src/core/mmio/TcpListenerEndpointWorker.h | Project Files/Source/Console/MeterManager.cs | 39899-40160 | port | thetis-samphire | |
| src/core/mmio/UdpEndpointWorker.cpp | Project Files/Source/Console/MeterManager.cs | 40419 | port | thetis-samphire | UdpListener bind pattern |
| src/core/mmio/UdpEndpointWorker.h | Project Files/Source/Console/MeterManager.cs | 40403-40588 | port | thetis-samphire | UdpListener |
| src/core/NoiseFloorEstimator.h | Project Files/Source/Console/display.cs | 5866 | port | thetis-samphire | inspired-by; replaces algorithm; cites Thetis for contrast |
| src/core/NoiseFloorTracker.cpp | Project Files/Source/Console/display.cs | 5866-5961 | port | thetis-samphire | lerp-based noise floor tracker; reimplements processNoiseFloor lerp approach |
| src/core/NoiseFloorTracker.h | Project Files/Source/Console/display.cs | 5866-5961 | port | thetis-samphire | header for NoiseFloorTracker |
| src/core/P1RadioConnection.cpp | Project Files/Source/ChannelMaster/networkproto1.c; Project Files/Source/Console/HPSDR/NetworkIO.cs; Project Files/Source/Console/cmaster.cs; Project Files/Source/Console/console.cs; Project Files/Source/ChannelMaster/bandwidth_monitor.c; Project Files/Source/ChannelMaster/bandwidth_monitor.h; Project Files/Source/Console/HPSDR/IoBoardHl2.cs (mi0bot/OpenHPSDR-Thetis fork) | full | port | multi-source | synthesizes LGPL ChannelMaster + Samphire-heavy console + bandwidth_monitor (Samphire) + IoBoardHl2 (Campbell MI0BOT, mi0bot fork); all 7 upstream verbatim headers preserved in file |
| src/core/P1RadioConnection.h | Project Files/Source/ChannelMaster/networkproto1.c; Project Files/Source/Console/HPSDR/NetworkIO.cs | full | port | multi-source | |
| src/core/P2RadioConnection.cpp | Project Files/Source/ChannelMaster/network.c; Project Files/Source/ChannelMaster/network.h; Project Files/Source/ChannelMaster/netInterface.c; Project Files/Source/Console/console.cs | full | port | multi-source | primary code from Samphire-clean LGPL ChannelMaster; console.cs for band-filter + DDC logic |
| src/core/P2RadioConnection.h | Project Files/Source/ChannelMaster/network.c; Project Files/Source/ChannelMaster/network.h; Project Files/Source/ChannelMaster/netInterface.c; Project Files/Source/Console/console.cs | full | port | multi-source | |
| src/core/ReceiverManager.cpp | Project Files/Source/Console/console.cs | 8216 | port | thetis-samphire | UpdateDDCs |
| src/core/ReceiverManager.h | Project Files/Source/Console/console.cs | 8216 | port | thetis-samphire | UpdateDDCs |
| src/core/RxChannel.cpp | Project Files/Source/Console/radio.cs; Project Files/Source/Console/console.cs; Project Files/Source/Console/dsp.cs; Project Files/Source/Console/rxa.cs; Project Files/Source/Console/HPSDR/specHPSDR.cs; Project Files/Source/Console/setup.cs; Project Files/Source/ChannelMaster/cmaster.c | full | port | multi-source | synthesizes Samphire-heavy radio/console/dsp with Wigley-only specHPSDR.cs and NR0V-only cmaster.c |
| src/core/RxChannel.h | Project Files/Source/Console/radio.cs; Project Files/Source/Console/console.cs; Project Files/Source/Console/dsp.cs; Project Files/Source/Console/HPSDR/specHPSDR.cs; Project Files/Source/ChannelMaster/cmaster.c | full | port | multi-source | |
| src/core/SampleRateCatalog.cpp | Project Files/Source/Console/setup.cs | 847-852; 866 | port | thetis-samphire | per-protocol rate-list constants + default; buffer-size formula treated as non-copyrightable fact (idea-merger) and documented inline rather than under a separate cmsetup.c citation |
| src/core/SampleRateCatalog.h | Project Files/Source/Console/setup.cs | 847-852; 866 | port | thetis-samphire | per-protocol rate-list constants + default; buffer-size formula treated as non-copyrightable fact (idea-merger) and documented inline rather than under a separate cmsetup.c citation |
| src/core/OcMatrix.cpp | Project Files/Source/Console/HPSDR/Penny.cs; Project Files/Source/Console/enums.cs | 33-150; 443-457 | port | thetis-no-samphire | per-band×per-pin×per-mode OC bit matrix (RXABitMasks[]/TXABitMasks[], setBandABitMask); TX pin action mapping (TXPinActions enum: MOX/TUNE/TWOTONE and combinations); per-MAC persistence via AppSettings |
| src/core/OcMatrix.h | Project Files/Source/Console/HPSDR/Penny.cs; Project Files/Source/Console/enums.cs | 33-150; 443-457 | port | thetis-no-samphire | header mirrors OcMatrix.cpp |
| src/core/StepAttenuatorController.cpp | Project Files/Source/Console/console.cs | 21290-21763 | port | thetis-samphire | handleOverload, pollOverloadSyncSeqErr, RX1AttenuatorData, comboPreamp_SelectedIndexChanged |
| src/core/StepAttenuatorController.h | Project Files/Source/Console/console.cs | 21290-21763 | port | thetis-samphire | PreampMode enum (21574-21586), overload level fields (21212-21224) |
| src/core/wdsp_api.h | Project Files/Source/Console/dsp.cs; Project Files/Source/Console/radio.cs; Project Files/Source/Console/HPSDR/specHPSDR.cs | full | port | multi-source | verbatim C API declarations |
| src/core/WdspEngine.cpp | Project Files/Source/ChannelMaster/cmaster.c | 72-86 | port | thetis-no-samphire | create_rcvr OpenChannel call; NR0V only, no Samphire |
| src/core/WdspEngine.h | Project Files/Source/Console/cmaster.cs; Project Files/Source/ChannelMaster/cmaster.c | 491; 32-93 | port | multi-source | CMCreateCMaster (cmaster.cs, Samphire) + create_rcvr (cmaster.c, NR0V) |
| src/core/WdspTypes.h | Project Files/Source/Console/dsp.cs; Project Files/Source/Console/setup.cs; Project Files/Source/Console/console.cs | full | port | multi-source | verbatim enum integer values; wdsp.cs citation is mi0bot filename but values match ramdor dsp.cs |
| src/gui/AboutDialog.cpp | Project Files/Source/Console/frmAbout.cs; Project Files/Source/Console/frmAbout.Designer.cs | full; 57-81 | port | thetis-samphire | kRoster reproduces lstContributors.Items.AddRange block verbatim (creative expression — specific phrasings, ordering, blank-line separators, "and indirectly, all the testers" closer). frmAbout.Designer.cs has no per-file header; paired frmAbout.cs header (Samphire MW0LGE, GPLv2-or-later + dual-licensing stanza) is preserved verbatim at top of the NereusSDR file. Layout patterns from AetherSDR MainWindow/TitleBar. Promoted 2026-04-18 from the PROVENANCE exclusion list once the roster reproduction went verbatim. |
| src/gui/AboutDialog.h | Project Files/Source/Console/frmAbout.cs; Project Files/Source/Console/frmAbout.Designer.cs | full | port | thetis-samphire | .h orphan pair with AboutDialog.cpp; carries the same Thetis port citation and frmAbout.cs verbatim header so the paired files are internally consistent. |
| src/gui/AddCustomRadioDialog.cpp | Project Files/Source/Console/frmAddCustomRadio.cs; Project Files/Source/Console/frmAddCustomRadio.Designer.cs | 60; 64; 81-82 | port | thetis-samphire | |
| src/gui/AddCustomRadioDialog.h | Project Files/Source/Console/frmAddCustomRadio.cs; Project Files/Source/Console/frmAddCustomRadio.Designer.cs | full | port | thetis-samphire | .h orphan pair; .cpp was already in outbound audit |
| src/gui/applets/DigitalApplet.cpp | Project Files/Source/Console/setup.cs; Project Files/Source/Console/console.cs | full | port | multi-source | layout ports Thetis setup.cs DIGx tab (digital-mode slider rows) + console.cs VAC wiring; all controls NYI |
| src/gui/applets/DigitalApplet.h | Project Files/Source/Console/setup.cs; Project Files/Source/Console/console.cs | full | port | multi-source | applet declaration; pairs with DigitalApplet.cpp |
| src/gui/applets/DiversityApplet.cpp | Project Files/Source/Console/DiversityForm.cs | full | port | thetis-samphire | DIV enable + RX1/RX2 source + Gain(-20..+20 dB) + Phase(0..360°) + ESC Off/Auto/Manual; all controls NYI |
| src/gui/applets/DiversityApplet.h | Project Files/Source/Console/DiversityForm.cs | full | port | thetis-samphire | applet declaration; pairs with DiversityApplet.cpp |
| src/gui/applets/FmApplet.cpp | Project Files/Source/Console/setup.cs; Project Files/Source/Console/console.cs | full | port | multi-source | 38-tone CTCSS list + 5.0k/2.5k deviation presets + Simplex button (Thetis setup.cs FM tab); console.cs FM mode wiring; all controls NYI |
| src/gui/applets/FmApplet.h | Project Files/Source/Console/setup.cs; Project Files/Source/Console/console.cs | full | port | multi-source | applet declaration; pairs with FmApplet.cpp |
| src/gui/applets/PhoneCwApplet.cpp | Project Files/Source/Console/setup.cs | full | port | thetis-samphire | 38-tone CTCSS standard list |
| src/gui/applets/PhoneCwApplet.h | Project Files/Source/Console/setup.cs | full | port | thetis-samphire | applet declaration; pairs with PhoneCwApplet.cpp |
| src/gui/applets/PureSignalApplet.cpp | Project Files/Source/Console/PSForm.cs | full | port | thetis-samphire | PureSignal feedback/correction control layout (Samphire-authored, /*…*/ header preserved); all controls NYI — wired in 3M-4 |
| src/gui/applets/PureSignalApplet.h | Project Files/Source/Console/PSForm.cs | full | port | thetis-samphire | applet declaration; pairs with PureSignalApplet.cpp |
| src/gui/applets/RxApplet.cpp | Project Files/Source/Console/console.cs; Project Files/Source/Console/console.resx; Project Files/Source/Console/setup.cs | full | port | thetis-samphire | comboPreamp, AGC thresh, step attenuator, tooltips |
| src/gui/applets/RxApplet.h | Project Files/Source/Console/console.cs; Project Files/Source/Console/console.resx; Project Files/Source/Console/setup.cs | full | port | multi-source | applet declaration; pairs with RxApplet.cpp |
| src/gui/ConnectionPanel.cpp | Project Files/Source/Console/ucRadioList.cs; Project Files/Source/Console/clsDiscoveredRadioPicker.cs | full | port | thetis-samphire | Thetis ucRadioList port — radio list UI; both sources are Samphire-maintained |
| src/gui/ConnectionPanel.h | Project Files/Source/Console/ucRadioList.cs | full | port | thetis-samphire | |
| src/gui/containers/ContainerManager.cpp | Project Files/Source/Console/MeterManager.cs | 5613-5673; 5812-5918; 6012-6105; 6391-6447; 6514-6579 | port | thetis-samphire | container lifecycle |
| src/gui/containers/ContainerManager.h | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | duplicate-container path |
| src/gui/containers/ContainerSettingsDialog.cpp | Project Files/Source/Console/setup.cs; Project Files/Source/Console/MeterManager.cs | 24522-24566; 24443; 24447; 21266; 22472 | port | thetis-samphire | |
| src/gui/containers/ContainerSettingsDialog.h | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | .h orphan pair; .cpp was already in outbound audit; 3-column Available/In-use/Properties layout |
| src/gui/containers/ContainerWidget.cpp | Project Files/Source/Console/ucMeter.cs | 49-59; 281-294; 319-374; 400-407; 489-518; 520-572; 574-593; 912-935; 974-993; 1198-1229 | port | thetis-samphire | |
| src/gui/containers/ContainerWidget.h | Project Files/Source/Console/ucMeter.cs; Project Files/Source/Console/setup.cs; Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | axis enum, container highlight, ContainerMinimised flag |
| src/gui/containers/FloatingContainer.cpp | Project Files/Source/Console/frmMeterDisplay.cs | 114-179 | port | thetis-samphire | lifecycle, close, console state |
| src/gui/containers/FloatingContainer.h | Project Files/Source/Console/frmMeterDisplay.cs; Project Files/Source/Console/MeterManager.cs | 114-179 | port | thetis-samphire | ContainerMinimised handler |
| src/gui/containers/meter_property_editors/BaseItemEditor.cpp | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | shared base class for the per-item property editors; Qt-native re-expression of clsItem property-binding model (binding combo, MMIO picker wiring) |
| src/gui/containers/meter_property_editors/BaseItemEditor.h | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | shared base class declaration; pairs with BaseItemEditor.cpp |
| src/gui/containers/meter_property_editors/NeedleItemEditor.cpp | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | clsNeedleItem per-needle setter implementation; pairs with NeedleItemEditor.h |
| src/gui/containers/meter_property_editors/NeedleItemEditor.h | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | clsNeedleItem per-needle setters; full Thetis parity |
| src/gui/containers/meter_property_editors/NeedleScalePwrItemEditor.cpp | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | clsNeedleScalePwrItem implementation; pairs with NeedleScalePwrItemEditor.h |
| src/gui/containers/meter_property_editors/NeedleScalePwrItemEditor.h | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | clsNeedleScalePwrItem; full Thetis parity |
| src/gui/containers/meter_property_editors/ScaleItemEditor.cpp | Project Files/Source/Console/MeterManager.cs | 14827 | port | thetis-samphire | clsScaleItem ShowType |
| src/gui/containers/meter_property_editors/ScaleItemEditor.h | Project Files/Source/Console/MeterManager.cs | 14827 | port | thetis-samphire | clsScaleItem.ShowType centered title mode |
| src/gui/containers/MmioEndpointsDialog.cpp | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | Qt-native UI port of Samphire's MMIO endpoint editor; transport enum mirrors clsMMIO Transport (UdpListener/TcpListener/TcpClient/Serial) |
| src/gui/containers/MmioEndpointsDialog.h | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | dialog declaration; pairs with MmioEndpointsDialog.cpp |
| src/gui/containers/MmioVariablePickerPopup.h | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | matches Thetis frmVariablePicker; --DEFAULT-- sentinel; Cancel-preserves-caller behavior |
| src/gui/containers/MmioVariablePickerPopup.cpp | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | frmVariablePicker implementation; pairs with MmioVariablePickerPopup.h |
| src/gui/MainWindow.cpp | Project Files/Source/Console/MeterManager.cs; Project Files/Source/Console/dsp.cs; Project Files/Source/Console/console.cs; Project Files/Source/Console/setup.cs; Project Files/Source/Console/radio.cs | full | port | multi-source | AVG_SIGNAL_STRENGTH, AGCMode/DSPMode, band defs, pbAutoAttWarningRX1, SetupForm attenuator init; mixed-lineage with AetherSDR signal-routing hub / status bar / TitleBar feature-request dialog (see aethersdr-reconciliation.md D.2) |
| src/gui/MainWindow.h | Project Files/Source/Console/console.cs | full | port | thetis-samphire | mixed-lineage with AetherSDR per .cpp row; class shape + signal slots ported from AetherSDR MainWindow.h (see aethersdr-reconciliation.md D.2) |
| src/gui/meters/AntennaButtonItem.cpp | Project Files/Source/Console/MeterManager.cs | 9502+ | port | thetis-samphire | clsAntennaButtonBox |
| src/gui/meters/AntennaButtonItem.h | Project Files/Source/Console/MeterManager.cs | 9502+ | port | thetis-samphire | clsAntennaButtonBox |
| src/gui/meters/BandButtonItem.cpp | Project Files/Source/Console/MeterManager.cs | 11482+ | port | thetis-samphire | clsBandButtonBox |
| src/gui/meters/BandButtonItem.h | Project Files/Source/Console/MeterManager.cs | 11482+; 11896 | port | thetis-samphire | clsBandButtonBox, PopupBandstack |
| src/gui/meters/ButtonBoxItem.cpp | Project Files/Source/Console/MeterManager.cs | 12307+ | port | thetis-samphire | clsButtonBox, layout constants |
| src/gui/meters/ButtonBoxItem.h | Project Files/Source/Console/MeterManager.cs | 12307+; 12309-12327 | port | thetis-samphire | clsButtonBox, IndicatorType enum |
| src/gui/meters/ClickBoxItem.h | Project Files/Source/Console/MeterManager.cs | 7571+ | port | thetis-samphire | clsClickBox |
| src/gui/meters/ClickBoxItem.cpp | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | clsClickBoxItem implementation; pairs with ClickBoxItem.h |
| src/gui/meters/ClockItem.h | Project Files/Source/Console/MeterManager.cs | 14075+ | port | thetis-samphire | clsClock |
| src/gui/meters/ClockItem.cpp | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | clsClockItem implementation; pairs with ClockItem.h |
| src/gui/meters/DataOutItem.h | Project Files/Source/Console/MeterManager.cs | 16047+ | port | thetis-samphire | clsDataOut |
| src/gui/meters/DataOutItem.cpp | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | clsDataOutItem implementation; pairs with DataOutItem.h |
| src/gui/meters/DialItem.cpp | Project Files/Source/Console/MeterManager.cs | 15399+; 33750-33899 | port | thetis-samphire | clsDialDisplay, renderDialDisplay |
| src/gui/meters/DialItem.h | Project Files/Source/Console/MeterManager.cs | 15399+ | port | thetis-samphire | clsDialDisplay |
| src/gui/meters/DiscordButtonItem.h | Project Files/Source/Console/MeterManager.cs | 11983+ | port | thetis-samphire | clsDiscordButtonBox |
| src/gui/meters/DiscordButtonItem.cpp | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | clsDiscordButtonItem implementation; pairs with DiscordButtonItem.h |
| src/gui/meters/FadeCoverItem.cpp | Project Files/Source/Console/MeterManager.cs | 7665+; 36292; 7887-7888; 1900 | port | thetis-samphire | clsFadeCover, renderFadeCover, FadeOnRx/Tx |
| src/gui/meters/FadeCoverItem.h | Project Files/Source/Console/MeterManager.cs | 7665+; 1900 | port | thetis-samphire | clsFadeCover, FadeOnRx/Tx |
| src/gui/meters/FilterButtonItem.cpp | Project Files/Source/Console/MeterManager.cs | 7674+ | port | thetis-samphire | clsFilterButtonBox |
| src/gui/meters/FilterButtonItem.h | Project Files/Source/Console/MeterManager.cs | 7674+; 7917 | port | thetis-samphire | clsFilterButtonBox, PopupFilterContextMenu |
| src/gui/meters/FilterDisplayItem.cpp | Project Files/Source/Console/MeterManager.cs | 16852+; 18296+; 16936; 16940+; 16865; 16951-16952; 16957; 17006 | port | thetis-samphire | clsFilterItem, FIDisplayMode, palette, notch color |
| src/gui/meters/FilterDisplayItem.h | Project Files/Source/Console/MeterManager.cs | 16852+ | port | thetis-samphire | clsFilterItem, enum definitions |
| src/gui/meters/HistoryGraphItem.cpp | Project Files/Source/Console/MeterManager.cs | 16149+; 16468+ | port | thetis-samphire | clsHistoryItem, addReading; NereusSDR ring buffer divergence noted |
| src/gui/meters/HistoryGraphItem.h | Project Files/Source/Console/MeterManager.cs | 16149+ | port | thetis-samphire | clsHistoryItem |
| src/gui/meters/ItemGroup.cpp | Project Files/Source/Console/MeterManager.cs | 21499-21616; 23862; 23990; 23326-23411; 23025; 23681; 21740; 21638-21640; 23091; 23179; 23265; 23412; 23473; 23534; 23620; 22461-22815; 22817-23002 | port | thetis-samphire | 35+ cited ranges; comprehensive preset factories |
| src/gui/meters/ItemGroup.h | Project Files/Source/Console/MeterManager.cs | 21523-21616; 23326-23411; 22461-22815; 22817-23002 | port | thetis-samphire | |
| src/gui/meters/LEDItem.cpp | Project Files/Source/Console/MeterManager.cs | 19448+ | port | thetis-samphire | clsLed, renderLed |
| src/gui/meters/LEDItem.h | Project Files/Source/Console/MeterManager.cs | 19448+ | port | thetis-samphire | clsLed, LedShape/LedStyle enums |
| src/gui/meters/MagicEyeItem.cpp | Project Files/Source/Console/MeterManager.cs | 15855+ | port | thetis-samphire | clsMagicEyeItem |
| src/gui/meters/MagicEyeItem.h | Project Files/Source/Console/MeterManager.cs | 15855+ | port | thetis-samphire | clsMagicEyeItem |
| src/gui/meters/MeterItem.cpp | Project Files/Source/Console/MeterManager.cs; Project Files/Source/Console/console.cs | 2258-2318; 19917-20278; 14827+; 32338+; 21499-21616; 35950-36140; 12612-12678 | port | multi-source | verbatim ReadingName switch; edge meter colors from console.cs |
| src/gui/meters/MeterItem.h | Project Files/Source/Console/MeterManager.cs; Project Files/Source/Console/console.cs | 2258-2318; 19917-21616; 14827+; 32338+; 12612-12678 | port | multi-source | |
| src/gui/meters/MeterPoller.cpp | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | 100ms UpdateInterval poll cadence; .h was in outbound audit; .cpp orphan pair |
| src/gui/meters/MeterPoller.h | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | Reading enum / TX meters |
| src/gui/meters/MeterWidget.cpp | Project Files/Source/Console/MeterManager.cs | 21266; 31366-31368 | port | thetis-samphire | container fixed-aspect, per-item render gate |
| src/gui/meters/MeterWidget.h | Project Files/Source/Console/MeterManager.cs | 31366-31368 | port | thetis-samphire | visibility filter |
| src/gui/meters/ModeButtonItem.cpp | Project Files/Source/Console/MeterManager.cs | 9951+ | port | thetis-samphire | clsModeButtonBox |
| src/gui/meters/ModeButtonItem.h | Project Files/Source/Console/MeterManager.cs | 9951+ | port | thetis-samphire | clsModeButtonBox |
| src/gui/meters/NeedleScalePwrItem.cpp | Project Files/Source/Console/MeterManager.cs | 14888+; 31645-31850 | port | thetis-samphire | clsNeedleScalePwrItem, renderNeedleScale |
| src/gui/meters/NeedleScalePwrItem.h | Project Files/Source/Console/MeterManager.cs | 14888+; 22817-23002; 31822-31823 | port | thetis-samphire | clsNeedleScalePwrItem, AddCrossNeedle |
| src/gui/meters/OtherButtonItem.cpp | Project Files/Source/Console/MeterManager.cs | 8225+ | port | thetis-samphire | clsOtherButtons |
| src/gui/meters/OtherButtonItem.h | Project Files/Source/Console/MeterManager.cs | 8225+ | port | thetis-samphire | clsOtherButtons |
| src/gui/meters/RotatorItem.cpp | Project Files/Source/Console/MeterManager.cs | 15042+; 35170-35569; 15290-15312 | port | thetis-samphire | clsRotatorItem, renderRotator, Update |
| src/gui/meters/RotatorItem.h | Project Files/Source/Console/MeterManager.cs | 15042+; 15290-15312 | port | thetis-samphire | clsRotatorItem, Update |
| src/gui/meters/SignalTextItem.cpp | Project Files/Source/Console/MeterManager.cs; Project Files/Source/Console/console.cs | 20286-20540; 20420+ | port | multi-source | clsSignalText, Common.UVfromDBM, dBm format helpers |
| src/gui/meters/SignalTextItem.h | Project Files/Source/Console/MeterManager.cs; Project Files/Source/Console/console.cs | 20286+ | port | multi-source | |
| src/gui/meters/SpacerItem.cpp | Project Files/Source/Console/MeterManager.cs | 16116; 35010 | port | thetis-samphire | clsSpacerItem, renderSpacer |
| src/gui/meters/SpacerItem.h | Project Files/Source/Console/MeterManager.cs | 16116; 16121-16143 | port | thetis-samphire | clsSpacerItem |
| src/gui/meters/TextOverlayItem.cpp | Project Files/Source/Console/MeterManager.cs | 18746+; 19267-19395; 18800+ | port | thetis-samphire | clsTextOverlay, parseText, render |
| src/gui/meters/TextOverlayItem.h | Project Files/Source/Console/MeterManager.cs | 18746+; 19267+ | port | thetis-samphire | clsTextOverlay, parseText |
| src/gui/meters/TuneStepButtonItem.cpp | Project Files/Source/Console/MeterManager.cs | 7999+ | port | thetis-samphire | TuneStepList |
| src/gui/meters/TuneStepButtonItem.h | Project Files/Source/Console/MeterManager.cs | 7999+ | port | thetis-samphire | clsTunestepButtons |
| src/gui/meters/VfoDisplayItem.cpp | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | clsVfoDisplay digit handling |
| src/gui/meters/VfoDisplayItem.h | Project Files/Source/Console/MeterManager.cs | 12881+ | port | thetis-samphire | clsVfoDisplay |
| src/gui/meters/VoiceRecordPlayItem.h | Project Files/Source/Console/MeterManager.cs | 10222+ | port | thetis-samphire | clsVoiceRecordPlay |
| src/gui/meters/VoiceRecordPlayItem.cpp | Project Files/Source/Console/MeterManager.cs | full | port | thetis-samphire | clsVoiceRecordPlayItem implementation; pairs with VoiceRecordPlayItem.h |
| src/gui/meters/WebImageItem.cpp | Project Files/Source/Console/MeterManager.cs | 14165+ | port | thetis-samphire | clsWebImage |
| src/gui/meters/WebImageItem.h | Project Files/Source/Console/MeterManager.cs | 14165+ | port | thetis-samphire | clsWebImage |
| src/gui/setup/DisplaySetupPages.cpp | Project Files/Source/Console/display.cs | 1372 | port | thetis-samphire | RX1DisplayCalOffset |
| src/gui/setup/DisplaySetupPages.h | Project Files/Source/Console/setup.cs | full | port | thetis-samphire | Display tab sections |
| src/gui/setup/DspSetupPages.cpp | Project Files/Source/Console/setup.cs | full | port | thetis-samphire | tabDSP AGC/Noise/NoiseBlanker/CW/AMSAM/FM/VOX/CFC/MNF controls |
| src/gui/setup/DspSetupPages.h | Project Files/Source/Console/setup.cs | full | port | thetis-samphire | DSP setup page declarations; pairs with DspSetupPages.cpp |
| src/gui/setup/GeneralOptionsPage.cpp | Project Files/Source/Console/setup.cs | full | port | thetis-samphire | grpHermesStepAttenuator, groupBoxTS47, chkAutoATTRx1/2 |
| src/gui/setup/GeneralOptionsPage.h | Project Files/Source/Console/setup.cs | full | port | thetis-samphire | grpHermesStepAttenuator, groupBoxTS47 |
| src/gui/setup/hardware/AntennaAlexTab.cpp | Project Files/Source/Console/setup.cs; Project Files/Source/Console/setup.designer.cs | 13393-13478; 6185-6246; 2892-2898; 18639; 23385-23395 | port | thetis-samphire | InitAlexAntTables(), radAlexR/T enable, chkRxOutOnTx, chkEnableXVTRHF; refactored into tcAlexControl parent QTabWidget |
| src/gui/setup/hardware/AntennaAlexAlex1Tab.cpp | Project Files/Source/Console/setup.designer.cs | 23385-25538 | port | thetis-samphire | Alex-1 Filters sub-sub-tab — HPF + LPF + Saturn BPF1 panels |
| src/gui/setup/hardware/AntennaAlexAlex1Tab.h | Project Files/Source/Console/setup.designer.cs | 23385-25538 | port | thetis-samphire | header mirrors .cpp |
| src/gui/setup/hardware/AntennaAlexAlex2Tab.cpp | Project Files/Source/Console/setup.designer.cs | 25539-26857 | port | thetis-samphire | Alex-2 Filters sub-sub-tab — RX2 board HPF + LPF panels with LED status stubs |
| src/gui/setup/hardware/AntennaAlexAlex2Tab.h | Project Files/Source/Console/setup.designer.cs | 25539-26857 | port | thetis-samphire | header mirrors .cpp |
| src/gui/setup/hardware/BandwidthMonitorTab.cpp | Project Files/Source/ChannelMaster/bandwidth_monitor.h | full | port | thetis-samphire | Qt sub-tab around Samphire's C byte-accounting API; wires static controls only — live feed deferred to Phase 3L |
| src/gui/setup/hardware/BandwidthMonitorTab.h | Project Files/Source/ChannelMaster/bandwidth_monitor.h | full | port | thetis-samphire | sub-tab declaration; pairs with BandwidthMonitorTab.cpp |
| src/gui/setup/hardware/AntennaAlexTab.h | Project Files/Source/Console/setup.cs; Project Files/Source/Console/setup.designer.cs | 13393; 2892-2898; 23385-23395 | port | thetis-samphire | InitAlexAntTables() + per-band row structure; refactored as tcAlexControl parent |
| src/gui/setup/hardware/DiversityTab.cpp | Project Files/Source/Console/DiversityForm.cs | full | port | thetis-samphire | |
| src/gui/setup/hardware/DiversityTab.h | Project Files/Source/Console/DiversityForm.cs | 1216; 1228 | port | thetis-samphire | chkLockAngle, chkLockR |
| src/gui/setup/hardware/OcOutputsHfTab.cpp | Project Files/Source/Console/setup.designer.cs | 13658-13670+ (tpOCHFControl + nested groupboxes for chkPenOCrcv/xmit, grpTransmitPinActionHF, grpUSBBCD, grpExtPAControlHF) | port | thetis-samphire | OC Outputs HF sub-sub-tab UI — RX/TX 14×7 matrix + pin actions + USB BCD + ext PA + master toggles + live OC pin state stubs |
| src/gui/setup/hardware/OcOutputsHfTab.h | Project Files/Source/Console/setup.designer.cs | 13658-13670+ | port | thetis-samphire | header mirrors OcOutputsHfTab.cpp |
| src/gui/setup/hardware/OcOutputsTab.cpp | Project Files/Source/Console/setup.designer.cs | tpOCHFControl + tpOCSWLControl (tcOCOutputs parent) | port | thetis-samphire | Parent QTabWidget hosting OC Outputs HF and SWL sub-sub-tabs; routes state via OcMatrix |
| src/gui/setup/hardware/OcOutputsTab.h | Project Files/Source/Console/setup.designer.cs | tpOCHFControl + tpOCSWLControl | port | thetis-samphire | Parent tab header |
| src/gui/setup/hardware/PaCalibrationTab.cpp | Project Files/Source/Console/setup.cs | full | port | thetis-samphire | PA calibration per-band gain arrays |
| src/gui/setup/hardware/PaCalibrationTab.h | Project Files/Source/Console/setup.cs | full | port | thetis-samphire | PA calibration page |
| src/gui/setup/hardware/PureSignalTab.cpp | Project Files/Source/Console/PSForm.cs | full | port | thetis-samphire | |
| src/gui/setup/hardware/PureSignalTab.h | Project Files/Source/Console/PSForm.cs | 841 | port | thetis-samphire | chkPSAutoAttenuate |
| src/gui/setup/hardware/RadioInfoTab.cpp | Project Files/Source/Console/setup.cs | 847; 848-850+ | port | thetis-samphire | Hardware Config / General, include_extra_p1_rate, numericUpDownNr |
| src/gui/setup/hardware/RadioInfoTab.h | Project Files/Source/Console/setup.cs | full | port | thetis-samphire | Hardware Config / General section board model display |
| src/gui/setup/hardware/XvtrTab.cpp | Project Files/Source/Console/xvtr.cs | 47-249 | port | multi-source | XVTRForm; xvtr.cs is FlexRadio + Wigley + Samphire dual-license |
| src/gui/setup/hardware/XvtrTab.h | Project Files/Source/Console/xvtr.cs | 47-249 | port | multi-source | XVTRForm class, 16→5 row reduction |
| src/gui/setup/HardwarePage.cpp | Project Files/Source/Console/setup.cs | full | port | thetis-samphire | 9 sub-tabs mirroring Thetis Hardware Config sub-tabs |
| src/gui/setup/HardwarePage.h | Project Files/Source/Console/setup.cs | full | port | thetis-samphire | Hardware Config nested QTabWidget layout |
| src/gui/setup/TransmitSetupPages.cpp | Project Files/Source/Console/setup.cs | full | port | thetis-samphire | PS feedback DDC choices (board-dependent) |
| src/gui/setup/TransmitSetupPages.h | Project Files/Source/Console/setup.cs | full | port | thetis-samphire | PA/Power tab |
| src/gui/SpectrumWidget.cpp | Project Files/Source/Console/display.cs; Project Files/Source/Console/console.cs | 6826-7075; 2003; 2069; 2102; 2184; 2522-2536; 7719-7729; 1372; 1743-1754; 31371-31385 | port | multi-source | waterfall color mapping, grid colors, data_line_color, waterfall defaults, band edge |
| src/gui/SpectrumWidget.h | Project Files/Source/Console/enums.cs; Project Files/Source/Console/setup.cs; Project Files/Source/Console/display.cs | full | port | multi-source | ColorScheme enum, setup params, band edge, waterfall defaults |
| src/gui/widgets/VfoModeContainers.cpp | Project Files/Source/Console/console.cs; Project Files/Source/Console/setup.designer.cs; Project Files/Source/Console/radio.cs | 40412; 40635; 2043-2044 | port | multi-source | FM TX simplex, RTTY mark default, rx_dolly defaults |
| src/gui/widgets/VfoModeContainers.h | Project Files/Source/Console/console.cs; Project Files/Source/Console/dsp.cs | full | port | multi-source | per-sideband DIGU/DIGL offset model; .cpp is in outbound audit |
| src/gui/widgets/VfoWidget.cpp | Project Files/Source/Console/console.cs; Project Files/Source/Console/console.resx; Project Files/Source/Console/display.cs; Project Files/Source/Console/enums.cs; Project Files/Source/Console/radio.cs; Project Files/Source/Console/dsp.cs; Project Files/Source/Console/HPSDR/specHPSDR.cs | full | port | multi-source | filter presets, AGC thresh, FM TX, many tooltips, cw_pitch, DSPMode, NB2 |
| src/gui/widgets/VfoWidget.h | Project Files/Source/Console/console.cs; Project Files/Source/Console/console.resx; Project Files/Source/Console/display.cs; Project Files/Source/Console/enums.cs; Project Files/Source/Console/radio.cs; Project Files/Source/Console/dsp.cs; Project Files/Source/Console/HPSDR/specHPSDR.cs | full | port | multi-source | VFO widget declaration; pairs with VfoWidget.cpp |
| src/models/Band.h | Project Files/Source/Console/console.cs | 6443 | port | thetis-samphire | BandByFreq; Band.cpp is independently implemented (IARU Region 2 spec) |
| src/models/Band.cpp | Project Files/Source/Console/console.cs | full | port | thetis-samphire | 14-band enum implementation; pairs with Band.h |
| src/models/PanadapterModel.cpp | Project Files/Source/Console/console.cs | 14242-14436 | port | thetis-samphire | uniform per-band defaults |
| src/models/PanadapterModel.h | Project Files/Source/Console/console.cs | 14242-14436 | port | thetis-samphire | per-band defaults |
| src/models/RadioModel.cpp | Project Files/Source/Console/console.cs; Project Files/Source/Console/setup.cs; Project Files/Source/Console/radio.cs; Project Files/Source/Console/dsp.cs; Project Files/Source/Console/HPSDR/NetworkIO.cs; Project Files/Source/ChannelMaster/cmaster.c | full | port | multi-source | UpdateDDCs, band defaults, CW pitch, AGC sync, filter presets, DIGL/DIGU offsets |
| src/models/RadioModel.h | Project Files/Source/Console/console.cs | 45960-46006 | port | thetis-samphire | bidirectional sync pattern |
| src/models/RxDspWorker.cpp | Project Files/Source/Console/console.cs | full | port | thetis-samphire | Qt worker plumbing implementing the .h's buffer-size formula and DSP wiring |
| src/models/RxDspWorker.h | Project Files/Source/Console/console.cs | full | port | thetis-samphire | buffer-size formula in_size = 64 * rate / 48000 |
| src/models/SliceModel.cpp | Project Files/Source/Console/console.cs; Project Files/Source/Console/display.cs | 5180-5575; 7559-7565; 14636; 14671; 1023 | port | multi-source | InitFilterPresets, CW pitch band filter, DIGU/DIGL offsets |
| src/models/SliceModel.h | Project Files/Source/Console/console.cs; Project Files/Source/Console/radio.cs; Project Files/Source/Console/setup.designer.cs | full | port | multi-source | InitFilterPresets, RIT, tune_step_list, ctcss_freq, FMTXMode, AGC defaults, dolly |
| tests/tst_dig_rtty_wire.cpp | Project Files/Source/Console/console.cs; Project Files/Source/Console/enums.cs; Project Files/Source/Console/radio.cs; Project Files/Source/Console/setup.designer.cs | 14637; 14672; 252-268; 2043-2044; 40635-40637 | port | multi-source | verifies ported DIGU/DIGL/RTTY defaults |
| tests/tst_fm_opt_container_wire.cpp | Project Files/Source/Console/console.cs | 40412 | port | thetis-samphire | chkFMTXSimplex_CheckedChanged |
| tests/tst_hpsdr_enums.cpp | Project Files/Source/Console/enums.cs; Project Files/Source/ChannelMaster/network.h | 109; 446 | port | multi-source | verbatim integer value parity; mi0bot/Thetis@Hermes-Lite prefix is author viewpoint |
| tests/tst_noise_floor_tracker.cpp | Project Files/Source/Console/display.cs | 5866-5961 | port | thetis-samphire | tests for NoiseFloorTracker lerp logic |
| tests/tst_meter_item_bar.cpp | Project Files/Source/Console/MeterManager.cs | 19917-20278; 35950-36140; 21499-21616 | port | thetis-samphire | clsBarItem, renderHBar, addSMeterBar parity |
| tests/tst_meter_item_scale.cpp | Project Files/Source/Console/MeterManager.cs | 14785-15853; 31879-31886; 32338+ | port | thetis-samphire | clsScaleItem, ShowType, generalScale |
| tests/tst_meter_presets.cpp | Project Files/Source/Console/MeterManager.cs | 21499-21616; 31911-31916 | port | thetis-samphire | addSMeterBar, renderScale dispatch |
| tests/tst_reading_name.cpp | Project Files/Source/Console/MeterManager.cs | 2258-2318 | port | thetis-samphire | verbatim label parity |
| tests/tst_rxchannel_agc_advanced.cpp | Project Files/Source/Console/radio.cs; Project Files/Source/Console/console.cs; Project Files/Source/Console/dsp.cs | 1037-1124; 45977; 116-117 | port | multi-source | default + idempotency verification |
| tests/tst_rxchannel_apf.cpp | Project Files/Source/Console/radio.cs | 1910-1927 | port | thetis-samphire | |
| tests/tst_rxchannel_audio_panel.cpp | Project Files/Source/Console/dsp.cs; Project Files/Source/Console/radio.cs | 393-394; 1386-1403; 1145-1162 | port | multi-source | |
| tests/tst_rxchannel_emnr.cpp | Project Files/Source/Console/radio.cs | 2216-2232 | port | thetis-samphire | |
| tests/tst_rxchannel_nb2_polish.cpp | Project Files/Source/Console/HPSDR/specHPSDR.cs; Project Files/Source/ChannelMaster/cmaster.c | 922-937; 55-68 | port | thetis-no-samphire | both cited sources exclude Samphire (Wigley-only specHPSDR.cs + NR0V-only cmaster.c) |
| tests/tst_rxchannel_snb.cpp | Project Files/Source/Console/console.cs; Project Files/Source/Console/dsp.cs | 36347; 692-693 | port | multi-source | |
| tests/tst_rxchannel_squelch.cpp | Project Files/Source/Console/radio.cs | 1185; 1293; 1312 | port | thetis-samphire | |
| tests/tst_slice_agc_advanced.cpp | Project Files/Source/Console/radio.cs; Project Files/Source/Console/console.cs | 1037-1124; 45977 | port | thetis-samphire | |
| tests/tst_slice_auto_agc.cpp | Project Files/Source/Console/console.cs; Project Files/Source/Console/setup.designer.cs | 45913-45954; 38630,39245,39320,39418 | port | thetis-samphire | setup.designer.cs has no GPL header — project-level LICENSE applies |
| tests/tst_slice_apf.cpp | Project Files/Source/Console/radio.cs | 1910-2008 | port | thetis-samphire | |
| tests/tst_slice_audio_panel.cpp | Project Files/Source/Console/dsp.cs; Project Files/Source/Console/radio.cs | 393-394; 1386-1403; 1145-1162 | port | multi-source | |
| tests/tst_slice_emnr.cpp | Project Files/Source/Console/radio.cs | 2216-2232 | port | thetis-samphire | |
| tests/tst_slice_rit_xit.cpp | Project Files/Source/Console/console.cs | full | port | thetis-samphire | RIT behaviour |
| tests/tst_slice_snb.cpp | Project Files/Source/Console/console.cs; Project Files/Source/Console/dsp.cs | 36347; 692-693 | port | multi-source | |
| tests/tst_slice_squelch.cpp | Project Files/Source/Console/radio.cs | 1185-1329; 1164-1178; 1274-1291; 1293-1329 | port | thetis-samphire | |
| tests/tst_step_attenuator_controller.cpp | Project Files/Source/Console/console.cs | 21359-21382; 21366; 21369; 21373-21375; 21378; 21548-21567 | port | thetis-samphire | |

## Files derived from mi0bot/Thetis-HL2

Discovery-reply hex fixtures are covered by `tests/fixtures/discovery/README.md` (added in Task 13), not by individual rows.

| NereusSDR file | mi0bot source | Line ranges | Type | Variant | Notes |
| --- | --- | --- | --- | --- | --- |
| src/core/RadioDiscovery.cpp | HPSDR/clsRadioDiscovery.cs | 1145-1195; 1201-1226; 49-70 | port | mi0bot | fork diff vs ramdor/Thetis adds 9 HL2-specific MI0BOT blocks (IpAddressFixedHL2/EeConfigHL2 fields, P1 discovery-reply HL2 parser, board-id 6 → HermesLite mapping) — our port carries those; Reid Campbell (MI0BOT) name verified from fork IoBoardHl2.cs header |
| src/core/RadioDiscovery.h | HPSDR/clsRadioDiscovery.cs | 49-70 | port | mi0bot | port of the RadioInfo additions mi0bot added for HL2 support |
| src/gui/setup/hardware/Hl2IoBoardTab.cpp | Project Files/Source/Console/setup.cs; Project Files/Source/Console/console.cs | 20234-20238; 25781-25945 | port | mi0bot | HL2 I/O Setup page UI — N2ADR enable (chkHERCULES), register state table, state machine viz, I2C transaction log, bandwidth monitor mini; setup.cs credits FlexRadio/Wigley/Samphire; console.cs credits FlexRadio/Wigley/W2PA/Samphire; both headers preserved verbatim |
| src/gui/setup/hardware/Hl2IoBoardTab.h | Project Files/Source/Console/setup.cs; Project Files/Source/Console/console.cs | 20234-20238; 25781-25945 | port | mi0bot | header mirrors Hl2IoBoardTab.cpp |
| tests/tst_radio_discovery_parse.cpp | HPSDR/clsRadioDiscovery.cs | 1145-1195; 1201-1226 | port | mi0bot | tests the mi0bot-added HL2 discovery parsing; attribution matches RadioDiscovery.cpp |
| src/core/IoBoardHl2.cpp | HPSDR/IoBoardHl2.cs; ChannelMaster/network.h; Console/console.cs | full; 112-148; 25781-25945 | port | mi0bot | IOBoard Registers enum (verbatim integer values), HardwareVersion enum, 256-byte register mirror, 32-slot I2C FIFO (MAX_I2C_QUEUE=32 from network.h), 12-step UpdateIOBoard state machine (switch(state++) from console.cs); closes Phase 3I-T12 |
| src/core/IoBoardHl2.h | HPSDR/IoBoardHl2.cs; ChannelMaster/network.h; Console/console.cs | full; 112-148; 25781-25945 | port | mi0bot | header mirrors IoBoardHl2.cpp |
| src/core/HermesLiteBandwidthMonitor.cpp | ChannelMaster/bandwidth_monitor.h; ChannelMaster/bandwidth_monitor.c | full | port | mi0bot | ports bandwidth_monitor_in/out/reset (byte accumulators) + compute_bps() (rolling byte-rate) from InterlockedAdd64/GetTickCount64 to std::atomic<int64_t>/std::chrono::steady_clock; adds NereusSDR throttle-detection layer (ep6 silent + ep2 active for N ticks); closes Phase 3I-T12 bandwidth-monitor piece [@c26a8a4] |
| src/core/HermesLiteBandwidthMonitor.h | ChannelMaster/bandwidth_monitor.h; ChannelMaster/bandwidth_monitor.c | full | port | mi0bot | header mirrors HermesLiteBandwidthMonitor.cpp |

## Files derived from TAPR WDSP

Vendored in `third_party/wdsp/` with upstream TAPR/OpenHPSDR-wdsp license
and Warren Pratt (NR0V) attribution preserved. No per-file NereusSDR
header required. See `third_party/wdsp/README.md` and
`third_party/wdsp/LICENSE`.

## Independently implemented — Thetis-like but not derived

Files whose behavior resembles Thetis but whose implementation was
written without consulting Thetis source. No per-file Thetis header
required. These rows are intentionally formatted with the file path in
column 2 (not column 1) so the header-verifier script does not scan them.

| Behavioral resemblance | NereusSDR file | Basis of implementation |
| --- | --- | --- |
| Band edges mirror Thetis BandStackManager HF definitions | src/models/Band.cpp | Ham-band boundaries from IARU Region 2 spec; no Thetis source consulted |
| Left overlay button strip — Thetis has a similar overlay | src/gui/SpectrumOverlayPanel.cpp | Ported from AetherSDR src/gui/SpectrumOverlayMenu.cpp; the single "Thetis" mention is a categorical feature-set label, not a source citation |
| Same as .cpp | src/gui/SpectrumOverlayPanel.h | AetherSDR-derived; categorical "Thetis feature set" mention in banner only |
| Mentions "Thetis-format skin" in a placeholder tooltip | src/gui/setup/AppearanceSetupPages.cpp | Tooltip text for a future Phase 3H feature; no derived code — pure Qt widget construction |
| Tests NoiseFloorEstimator math; one comment contrasts Thetis processNoiseFloor approach | tests/tst_noise_floor_estimator.cpp | Tests NereusSDR's percentile-based estimator; Thetis mention is a contrast, not a derivation |
| Notes Thetis connection-refused behavior in a single comment | tests/tst_p1_loopback_connection.cpp | Loopback tests against P1FakeRadio; Thetis mention explains why a test was removed |
| Factory that picks P1 vs P2 based on ProtocolVersion | src/core/RadioConnection.cpp | Thin factory — no Thetis logic; not on candidate list; checked as sanity probe |
| Verifies AboutDialog contains ramdor/Thetis and mi0bot/OpenHPSDR-Thetis strings | tests/tst_about_dialog.cpp | Test of attribution rendering, not of Thetis-derived code |
