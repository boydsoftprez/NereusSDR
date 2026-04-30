# Thetis Contributor Index

**Generated mechanically** by `scripts/generate-contributor-indexes.py` on `2026-04-30T12:51:05+00:00`
from corpus `docs/attribution/thetis-author-tags.json` (thetis@`501e3f51`, mi0bot@`c26a8a4`).

**Do NOT hand-edit this file.** To add or correct a contributor, edit `docs/attribution/thetis-author-tags.json` and re-run the generator.

Historical Pass 5/6a snapshot preserved at `thetis-contributor-index-v020-snapshot.md`.

## Callsign glossary

| Callsign | Name | Role |
|---|---|---|
| `MW0LGE` | Richard Samphire | Thetis lead maintainer; UI, various; goes by 'Richie' |
| `W2PA` | Chris Codella | Behringer MIDI, QSK for Protocol-2, DB import |
| `DH1KLM` | Sigi | MIDI, skins & UI improvements; RedPitaya enum slot contribution |
| `MI0BOT` | Reid Campbell | Hermes Lite 2 fork maintainer; HL2 I/O, bandwidth monitor, I2C TLV |
| `G8NJJ` | Laurence Barker | G2, Andromeda, protocols; ANAN_G2_1K PA wiring |
| `W4TME` | Ke Chen | FlexRadio Systems contributor; database/memory form tweaks |
| `W4WMT` | Bryan Rambo | Resampler, VAC & cmASIO |
| `MI0BIT` | Unknown | mi0bot fork HL2 contributions (TX latency, PTT hang, I2C, channel swap); identity unconfirmed — possible alt callsign for MI0BOT |
| `G7KLJ` | Unknown | console.cs PA init thread + setup-instance singleton pattern; identity to be researched |
| `VK6APH` | Phil Harman | HPSDR firmware and protocol co-originator (About dialog uses alternate callsign VK6PH) |
| `W5WC` | Doug Wigley | UI, ChannelMaster, various; naming of 'Thetis' |
| `WU2O` | Scott | Forum admin; WDSP SSQL default-threshold tuning |
| `IK4JPN` | Unknown | 2014 console.cs modification (region marked IK4JPN+/IK4JPN-); identity to be researched |
| `MW0GLE` | Richard Samphire | Typo of MW0LGE (Richie Samphire) in setup.cs:2740 version tag [2.10.3.6_dev4]; preserved verbatim |
| `DK1HLM` | Unknown | OrionMKII/Saturn PA handling contribution; identity to be researched |
| `K2UE` | George Donadio | EME/antenna authority; ANAN-8000D Hi-Z/Lo-Z load detection idea |
| `KE9NS` | Darrin | Various contributions including SunTracking and memory-form URL drag-drop |
| `MW0GE` | Richard Samphire | Typo of MW0LGE (Richie Samphire) in Midi2CatCommands.cs:264 version tag [2.10.3.6]; preserved verbatim |
| `W1CEG` | Unknown | console.cs contribution (region marked :W1CEG: / W1CEG:  End); identity to be researched |

## Per-file index

Scope: every `.cs/.c/.h/.cpp` in upstream Thetis + mi0bot fork. Each entry lists **Block** contributors (found in the file's top Copyright block) and **Inline** contributors (corpus callsigns found in the file body with line numbers).

### `thetis/Project Files/Source/ChannelMaster/cmasio.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 58, 64, 148
- **Inline:** `W4WMT` (Bryan Rambo) — lines 141, 145, 340

### `thetis/Project Files/Source/ChannelMaster/cmasio.h`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 86, 89
- **Inline:** `W4WMT` (Bryan Rambo) — line 84

### `thetis/Project Files/Source/ChannelMaster/ivac.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 138, 211, 290

### `thetis/Project Files/Source/ChannelMaster/netInterface.c`
- Block: (none matched)
- **Inline:** `G8NJJ` (Laurence Barker) — line 301
- **Inline:** `MW0LGE` (Richard Samphire) — line 1178

### `thetis/Project Files/Source/ChannelMaster/network.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 325, 708, 1457, 1458

### `thetis/Project Files/Source/ChannelMaster/network.h`
- Block: (none matched)
- **Inline:** `G8NJJ` (Laurence Barker) — line 423
- **Inline:** `MI0BOT` (Reid Campbell) — line 422

### `thetis/Project Files/Source/ChannelMaster/networkproto1.c`
- Block: (none matched)
- **Inline:** `DH1KLM` (Sigi) — line 612
- **Inline:** `MW0LGE` (Richard Samphire) — lines 335, 353, 354, 355

### `thetis/Project Files/Source/ChannelMaster/pipe.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 229

### `thetis/Project Files/Source/ChannelMaster/ring.c`
- Block: (none matched)
- **Inline:** `W4WMT` (Bryan Rambo) — line 53

### `thetis/Project Files/Source/ChannelMaster/ring.h`
- Block: (none matched)
- **Inline:** `W4WMT` (Bryan Rambo) — line 35

### `thetis/Project Files/Source/ChannelMaster/version.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 13

### `thetis/Project Files/Source/Console/AmpView.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 89, 122, 259, 397

### `thetis/Project Files/Source/Console/Andromeda/Andromeda.cs`
- Block: (none matched)
- **Inline:** `G8NJJ` (Laurence Barker) — lines 16, 850, 1027, 1147
- **Inline:** `MW0LGE` (Richard Samphire) — lines 2141, 4121, 4126, 4129, 4147

### `thetis/Project Files/Source/Console/Andromeda/SliderSettingsForm.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 1276

### `thetis/Project Files/Source/Console/Andromeda/displaysettingsform.cs`
- Block: (none matched)
- **Inline:** `G8NJJ` (Laurence Barker) — lines 326, 330, 334, 338, 342

### `thetis/Project Files/Source/Console/CAT/CATCommands.cs`
- Block: (none matched)
- **Inline:** `DH1KLM` (Sigi) — lines 7271, 7294
- **Inline:** `MW0LGE` (Richard Samphire) — lines 1638, 1909, 2216, 2665, 2673, 2706, 2716, 2724 (+12 more)
- **Inline:** `W2PA` (Chris Codella) — lines 1037, 1072, 1249, 1345, 2857, 2894, 5883, 5896 (+11 more)

### `thetis/Project Files/Source/Console/CAT/CATTester.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 64

### `thetis/Project Files/Source/Console/CAT/SDRSerialPortII.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 91, 236

### `thetis/Project Files/Source/Console/CAT/SIOListenerII.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 38

### `thetis/Project Files/Source/Console/CAT/TCPIPcatServer.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 277

### `thetis/Project Files/Source/Console/ColorButton.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 162

### `thetis/Project Files/Source/Console/DiversityForm.cs`
- Block: (none matched)
- **Inline:** `G8NJJ` (Laurence Barker) — lines 2219, 2230, 2253, 2292, 2311, 2326
- **Inline:** `MW0LGE` (Richard Samphire) — lines 170, 188, 2280, 2299, 2318, 2544, 2558, 2572 (+1 more)

### `thetis/Project Files/Source/Console/FilterForm.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 677, 696, 786, 808, 920
- **Inline:** `W4TME` (Ke Chen) — lines 936, 937, 940, 941

### `thetis/Project Files/Source/Console/HPSDR/Alex.cs`
- Block: (none matched)
- **Inline:** `G8NJJ` (Laurence Barker) — line 377
- **Inline:** `MW0LGE` (Richard Samphire) — lines 167, 181

### `thetis/Project Files/Source/Console/HPSDR/NetworkIO.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 160, 432, 562, 901, 1120

### `thetis/Project Files/Source/Console/Memory/MemoryForm.cs`
- Block: (none matched)
- **Inline:** `KE9NS` (Darrin) — line 662
- **Inline:** `MW0LGE` (Richard Samphire) — lines 935, 1389, 1448
- **Inline:** `W4TME` (Ke Chen) — lines 470, 504

### `thetis/Project Files/Source/Console/MeterManager.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 1800, 4200, 5695, 6433, 6563, 6791, 6846, 6851 (+10 more)

### `thetis/Project Files/Source/Console/Midi2CatCommands.cs`
- Block: (none matched)
- **Inline:** `DH1KLM` (Sigi) — lines 5524, 5536, 5548, 5560, 5572, 5595, 5618, 5635 (+24 more)
- **Inline:** `MW0GE` (Richard Samphire) — line 264
- **Inline:** `MW0LGE` (Richard Samphire) — lines 1166, 1718, 1933, 3119, 3163, 6465
- **Inline:** `W2PA` (Chris Codella) — lines 45, 93, 103, 191, 207, 224, 288, 290 (+82 more)

### `thetis/Project Files/Source/Console/N1MM.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 188

### `thetis/Project Files/Source/Console/PSForm.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 72, 157, 409, 738, 754, 802, 815, 907 (+1 more)
- **Inline:** `W2PA` (Chris Codella) — line 480

### `thetis/Project Files/Source/Console/Skin.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 1897, 1917, 1923, 1973

### `thetis/Project Files/Source/Console/TCIServer.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 2116, 2344, 3869, 4319, 6270, 7073, 7478

### `thetis/Project Files/Source/Console/audio.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 359, 718, 1370, 1484, 1494, 1543, 1812, 1907 (+3 more)

### `thetis/Project Files/Source/Console/clsDBMan.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 84

### `thetis/Project Files/Source/Console/clsHardwareSpecific.cs`
- Block: (none matched)
- **Inline:** `DH1KLM` (Sigi) — lines 178, 180, 403, 420
- **Inline:** `G8NJJ` (Laurence Barker) — line 164

### `thetis/Project Files/Source/Console/cmaster.cs`
- Block: (none matched)
- **Inline:** `DH1KLM` (Sigi) — lines 625, 708, 744, 838
- **Inline:** `MW0LGE` (Richard Samphire) — lines 669, 999, 1011, 1017, 1020, 1105, 1872, 2293

### `thetis/Project Files/Source/Console/common.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 86, 582, 604, 636, 679, 937, 1372, 1387 (+3 more)

### `thetis/Project Files/Source/Console/console.Designer.cs`
- Block: (none matched)
- **Inline:** `G8NJJ` (Laurence Barker) — line 474

### `thetis/Project Files/Source/Console/console.cs`
- Block: (none matched)
- **Inline:** `DH1KLM` (Sigi) — lines 6739, 8296, 10028, 10999, 11024, 11164, 11189, 11667 (+23 more)
- **Inline:** `DK1HLM` (Unknown) — line 6821
- **Inline:** `G7KLJ` (Unknown) — lines 725, 1221, 1225
- **Inline:** `G8NJJ` (Laurence Barker) — lines 141, 492, 493, 494, 495, 496, 497, 498 (+44 more)
- **Inline:** `IK4JPN` (Unknown) — lines 26345, 26372
- **Inline:** `K2UE` (George Donadio) — lines 25985, 26064
- **Inline:** `MW0LGE` (Richard Samphire) — lines 559, 614, 866, 902, 914, 950, 963, 1044 (+414 more)
- **Inline:** `W1CEG` (Unknown) — lines 37084, 42364
- **Inline:** `W2PA` (Chris Codella) — lines 52, 4971, 4977, 4985, 4991, 12978, 15752, 16007 (+37 more)
- **Inline:** `W4TME` (Ke Chen) — lines 14647, 14682, 15718

### `thetis/Project Files/Source/Console/cwx.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 314, 623, 813, 1728, 1748, 1765, 1790, 2032 (+2 more)
- **Inline:** `W2PA` (Chris Codella) — line 1660

### `thetis/Project Files/Source/Console/database.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 744, 3425, 9844, 9860, 9886, 9910, 10038, 10336 (+11 more)
- **Inline:** `W2PA` (Chris Codella) — lines 9469, 9475, 9987, 11164, 11197, 11228, 11235
- **Inline:** `W4TME` (Ke Chen) — lines 5005, 5235

### `thetis/Project Files/Source/Console/display.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 249, 1972, 2038, 2054, 2135, 2272, 2368, 2415 (+48 more)

### `thetis/Project Files/Source/Console/dsp.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 828, 881, 883, 959, 960, 964

### `thetis/Project Files/Source/Console/enums.cs`
- Block: (none matched)
- **Inline:** `DH1KLM` (Sigi) — line 129
- **Inline:** `G8NJJ` (Laurence Barker) — lines 125, 126, 397
- **Inline:** `MI0BOT` (Reid Campbell) — lines 128, 396
- **Inline:** `MW0LGE` (Richard Samphire) — line 399

### `thetis/Project Files/Source/Console/eqform.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 203

### `thetis/Project Files/Source/Console/frmMeterDisplay.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 188

### `thetis/Project Files/Source/Console/radio.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 82, 105, 1090, 1183, 4243, 4258, 4272, 4294

### `thetis/Project Files/Source/Console/setup.cs`
- Block: (none matched)
- **Inline:** `DH1KLM` (Sigi) — lines 847, 6162, 6164, 6223, 6228, 6309, 6356, 6394 (+10 more)
- **Inline:** `G7KLJ` (Unknown) — line 127
- **Inline:** `G8NJJ` (Laurence Barker) — lines 6029, 6036, 6043, 6050, 6057, 6244, 8802, 9701 (+9 more)
- **Inline:** `MW0GLE` (Richard Samphire) — line 2740
- **Inline:** `MW0LGE` (Richard Samphire) — lines 134, 170, 176, 418, 576, 642, 646, 946 (+121 more)
- **Inline:** `W2PA` (Chris Codella) — lines 10172, 12409
- **Inline:** `W4WMT` (Bryan Rambo) — line 28453

### `thetis/Project Files/Source/Console/splash.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 60, 493, 523

### `thetis/Project Files/Source/Console/titlebar.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 65

### `thetis/Project Files/Source/Console/ucInfoBar.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 667, 683, 699, 715, 719, 731, 735, 747 (+2 more)

### `thetis/Project Files/Source/Console/ucMeter.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 660, 1059, 1188, 1200

### `thetis/Project Files/Source/Console/wideband.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 40

### `thetis/Project Files/Source/Midi2Cat/Midi2Cat.Data/CatCmdDb.cs`
- Block: (none matched)
- **Inline:** `DH1KLM` (Sigi) — lines 516, 518
- **Inline:** `MW0LGE` (Richard Samphire) — lines 510, 520, 523, 525, 527, 529
- **Inline:** `W2PA` (Chris Codella) — lines 95, 380, 532

### `thetis/Project Files/Source/Midi2Cat/Midi2Cat.Data/ControllerMapping.cs`
- Block: (none matched)
- **Inline:** `W2PA` (Chris Codella) — line 31

### `thetis/Project Files/Source/Midi2Cat/Midi2Cat.Data/Database.cs`
- Block: (none matched)
- **Inline:** `W2PA` (Chris Codella) — line 252

### `thetis/Project Files/Source/Midi2Cat/Midi2Cat.IO/MidiDevice.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 59, 60, 61, 124, 648, 667, 805
- **Inline:** `W2PA` (Chris Codella) — lines 62, 650, 709, 713, 715, 718, 720, 725 (+4 more)

### `thetis/Project Files/Source/Midi2Cat/Midi2Cat.IO/MidiDeviceSetup.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 183, 190
- **Inline:** `W2PA` (Chris Codella) — lines 251, 255, 257, 260, 262, 267

### `thetis/Project Files/Source/Midi2Cat/MidiMessageManager.cs`
- Block: (none matched)
- **Inline:** `W2PA` (Chris Codella) — lines 102, 118, 142, 143, 144, 145, 146, 147 (+24 more)

### `thetis/Project Files/Source/cmASIO/hostsample.cpp`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 327, 532, 690
- **Inline:** `W4WMT` (Bryan Rambo) — line 506

### `thetis/Project Files/Source/cmASIO/version.cpp`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 13

### `thetis/Project Files/Source/wdsp/RXA.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 663

### `thetis/Project Files/Source/wdsp/analyzer.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 1111, 1532

### `thetis/Project Files/Source/wdsp/analyzer.h`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 74

### `thetis/Project Files/Source/wdsp/eq.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 176

### `thetis/Project Files/Source/wdsp/rmatch.c`
- Block: (none matched)
- **Inline:** `W4WMT` (Bryan Rambo) — lines 734, 735

### `thetis/Project Files/Source/wdsp/ssql.c`
- Block: (none matched)
- **Inline:** `WU2O` (Scott) — lines 342, 352, 364

### `thetis/Project Files/lib/portaudio-19.7.0/include/portaudio.h`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 491

### `thetis/Project Files/lib/portaudio-19.7.0/src/common/pa_converters.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 120, 182, 194, 204, 214, 224, 234, 244 (+36 more)

### `thetis/Project Files/lib/portaudio-19.7.0/src/common/pa_converters.h`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 139, 150, 176, 187, 197, 206, 213, 224 (+1 more)

### `thetis/Project Files/lib/portaudio-19.7.0/src/common/pa_front.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 810, 1817

### `thetis/Project Files/lib/portaudio-19.7.0/src/hostapi/asio/pa_asio.cpp`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 1274, 1304

### `thetis/Project Files/lib/portaudio-19.7.0/src/hostapi/wasapi/pa_win_wasapi.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 993, 2757, 2784

### `mi0bot/Project Files/Source/ChannelMaster/cmasio.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 58, 64, 148
- **Inline:** `W4WMT` (Bryan Rambo) — lines 141, 145, 340

### `mi0bot/Project Files/Source/ChannelMaster/cmasio.h`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 86, 89
- **Inline:** `W4WMT` (Bryan Rambo) — line 84

### `mi0bot/Project Files/Source/ChannelMaster/ivac.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 138, 211, 290

### `mi0bot/Project Files/Source/ChannelMaster/netInterface.c`
- Block: (none matched)
- **Inline:** `G8NJJ` (Laurence Barker) — line 301
- **Inline:** `MI0BOT` (Reid Campbell) — lines 629, 816, 825, 1215, 1458, 1464, 1470, 1501 (+8 more)
- **Inline:** `MW0LGE` (Richard Samphire) — line 1204

### `mi0bot/Project Files/Source/ChannelMaster/network.c`
- Block: (none matched)
- **Inline:** `MI0BOT` (Reid Campbell) — lines 82, 222, 1442
- **Inline:** `MW0LGE` (Richard Samphire) — lines 327, 695, 1456, 1457

### `mi0bot/Project Files/Source/ChannelMaster/network.h`
- Block: (none matched)
- **Inline:** `G8NJJ` (Laurence Barker) — line 455
- **Inline:** `MI0BOT` (Reid Campbell) — lines 40, 109, 110, 113, 264, 276, 277, 442 (+3 more)

### `mi0bot/Project Files/Source/ChannelMaster/networkproto1.c`
- Block: (none matched)
- **Inline:** `DH1KLM` (Sigi) — line 781
- **Inline:** `MI0BOT` (Reid Campbell) — lines 252, 1249, 1261
- **Inline:** `MW0LGE` (Richard Samphire) — lines 338, 356, 357, 358

### `mi0bot/Project Files/Source/ChannelMaster/pipe.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 229

### `mi0bot/Project Files/Source/ChannelMaster/ring.c`
- Block: (none matched)
- **Inline:** `W4WMT` (Bryan Rambo) — line 53

### `mi0bot/Project Files/Source/ChannelMaster/ring.h`
- Block: (none matched)
- **Inline:** `W4WMT` (Bryan Rambo) — line 35

### `mi0bot/Project Files/Source/ChannelMaster/version.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 13

### `mi0bot/Project Files/Source/Console/AmpView.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 89, 122, 259, 397

### `mi0bot/Project Files/Source/Console/Andromeda/Andromeda.cs`
- Block: (none matched)
- **Inline:** `G8NJJ` (Laurence Barker) — lines 16, 850, 1027, 1147
- **Inline:** `MI0BOT` (Reid Campbell) — line 4157
- **Inline:** `MW0LGE` (Richard Samphire) — lines 2141, 4121, 4126, 4129, 4147

### `mi0bot/Project Files/Source/Console/Andromeda/SliderSettingsForm.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 1276

### `mi0bot/Project Files/Source/Console/Andromeda/displaysettingsform.cs`
- Block: (none matched)
- **Inline:** `G8NJJ` (Laurence Barker) — lines 326, 330, 334, 338, 342

### `mi0bot/Project Files/Source/Console/CAT/CATCommands.cs`
- Block: (none matched)
- **Inline:** `DH1KLM` (Sigi) — lines 7285, 7308
- **Inline:** `MI0BOT` (Reid Campbell) — lines 366, 384, 595, 6897, 9969, 10010
- **Inline:** `MW0LGE` (Richard Samphire) — lines 1651, 1922, 2229, 2678, 2686, 2719, 2729, 2737 (+12 more)
- **Inline:** `W2PA` (Chris Codella) — lines 1050, 1085, 1262, 1358, 2870, 2907, 5896, 5909 (+11 more)

### `mi0bot/Project Files/Source/Console/CAT/CATParser.cs`
- Block: (none matched)
- **Inline:** `MI0BOT` (Reid Campbell) — lines 187, 788, 820, 831, 959

### `mi0bot/Project Files/Source/Console/CAT/CATTester.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 64

### `mi0bot/Project Files/Source/Console/CAT/SDRSerialPortII.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 91, 236

### `mi0bot/Project Files/Source/Console/CAT/SIOListenerII.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 38

### `mi0bot/Project Files/Source/Console/CAT/TCPIPcatServer.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 277

### `mi0bot/Project Files/Source/Console/ColorButton.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 162

### `mi0bot/Project Files/Source/Console/DiversityForm.cs`
- Block: (none matched)
- **Inline:** `G8NJJ` (Laurence Barker) — lines 2219, 2230, 2253, 2292, 2311, 2326
- **Inline:** `MW0LGE` (Richard Samphire) — lines 170, 188, 2280, 2299, 2318, 2544, 2558, 2572 (+1 more)

### `mi0bot/Project Files/Source/Console/FilterForm.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 677, 696, 786, 808, 920
- **Inline:** `W4TME` (Ke Chen) — lines 936, 937, 940, 941

### `mi0bot/Project Files/Source/Console/HPSDR/Alex.cs`
- Block: (none matched)
- **Inline:** `G8NJJ` (Laurence Barker) — line 408
- **Inline:** `MI0BOT` (Reid Campbell) — lines 366, 389, 426, 446
- **Inline:** `MW0LGE` (Richard Samphire) — lines 167, 181

### `mi0bot/Project Files/Source/Console/HPSDR/NetworkIO.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 160, 432, 562, 901, 1120

### `mi0bot/Project Files/Source/Console/HPSDR/NetworkIOImports.cs`
- Block: (none matched)
- **Inline:** `MI0BIT` (Unknown) — lines 384, 387, 390, 393, 396, 399, 402, 405
- **Inline:** `MI0BOT` (Reid Campbell) — line 321

### `mi0bot/Project Files/Source/Console/HPSDR/Penny.cs`
- Block: (none matched)
- **Inline:** `MI0BOT` (Reid Campbell) — lines 174, 185

### `mi0bot/Project Files/Source/Console/HPSDR/clsRadioDiscovery.cs`
- Block: (none matched)
- **Inline:** `MI0BOT` (Reid Campbell) — lines 514, 518, 519, 1065, 1117, 1118, 1119, 1169 (+1 more)

### `mi0bot/Project Files/Source/Console/Memory/MemoryForm.cs`
- Block: (none matched)
- **Inline:** `KE9NS` (Darrin) — line 662
- **Inline:** `MW0LGE` (Richard Samphire) — lines 935, 1389, 1448
- **Inline:** `W4TME` (Ke Chen) — lines 470, 504

### `mi0bot/Project Files/Source/Console/MeterManager.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 1800, 4200, 5695, 6433, 6563, 6791, 6846, 6851 (+10 more)

### `mi0bot/Project Files/Source/Console/Midi2CatCommands.cs`
- Block: (none matched)
- **Inline:** `DH1KLM` (Sigi) — lines 5524, 5536, 5548, 5560, 5572, 5595, 5618, 5635 (+24 more)
- **Inline:** `MI0BOT` (Reid Campbell) — lines 6381, 6395
- **Inline:** `MW0GE` (Richard Samphire) — line 264
- **Inline:** `MW0LGE` (Richard Samphire) — lines 1166, 1718, 1933, 3119, 3163, 6493
- **Inline:** `W2PA` (Chris Codella) — lines 45, 93, 103, 191, 207, 224, 288, 290 (+82 more)

### `mi0bot/Project Files/Source/Console/N1MM.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 188

### `mi0bot/Project Files/Source/Console/PSForm.cs`
- Block: (none matched)
- **Inline:** `MI0BOT` (Reid Campbell) — lines 758, 759, 760, 788, 1144
- **Inline:** `MW0LGE` (Richard Samphire) — lines 72, 157, 409, 740, 772, 830, 843, 935 (+1 more)
- **Inline:** `W2PA` (Chris Codella) — line 480

### `mi0bot/Project Files/Source/Console/Skin.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 1897, 1917, 1923, 1973

### `mi0bot/Project Files/Source/Console/TCIServer.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 2116, 2344, 3869, 4319, 6270, 7073, 7478

### `mi0bot/Project Files/Source/Console/audio.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 359, 717, 1369, 1483, 1493, 1542, 1811, 1906 (+3 more)

### `mi0bot/Project Files/Source/Console/clsDBMan.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 84

### `mi0bot/Project Files/Source/Console/clsHardwareSpecific.cs`
- Block: (none matched)
- **Inline:** `DH1KLM` (Sigi) — lines 185, 187, 424, 441
- **Inline:** `G8NJJ` (Laurence Barker) — line 171

### `mi0bot/Project Files/Source/Console/cmaster.cs`
- Block: (none matched)
- **Inline:** `DH1KLM` (Sigi) — lines 631, 715, 751, 846
- **Inline:** `MI0BOT` (Reid Campbell) — lines 536, 566, 595, 685, 807, 885
- **Inline:** `MW0LGE` (Richard Samphire) — lines 675, 1008, 1020, 1026, 1029, 1114, 1881, 2302

### `mi0bot/Project Files/Source/Console/common.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 86, 582, 604, 636, 679, 937, 1372, 1387 (+3 more)

### `mi0bot/Project Files/Source/Console/console.Designer.cs`
- Block: (none matched)
- **Inline:** `G8NJJ` (Laurence Barker) — line 474

### `mi0bot/Project Files/Source/Console/console.cs`
- Block: (none matched)
- **Inline:** `DH1KLM` (Sigi) — lines 6766, 8326, 10069, 11055, 11086, 11232, 11262, 11740 (+24 more)
- **Inline:** `DK1HLM` (Unknown) — line 6849
- **Inline:** `G7KLJ` (Unknown) — lines 728, 1217, 1221
- **Inline:** `G8NJJ` (Laurence Barker) — lines 144, 495, 496, 497, 498, 499, 500, 501 (+44 more)
- **Inline:** `IK4JPN` (Unknown) — lines 26935, 26962
- **Inline:** `K2UE` (George Donadio) — lines 26571, 26650
- **Inline:** `MI0BOT` (Reid Campbell) — lines 124, 2099, 2101, 2110, 2121, 6772, 8409, 8476 (+84 more)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 562, 617, 862, 898, 910, 946, 959, 1040 (+415 more)
- **Inline:** `W1CEG` (Unknown) — lines 37937, 43304
- **Inline:** `W2PA` (Chris Codella) — lines 52, 4998, 5004, 5012, 5018, 13051, 15845, 16100 (+37 more)
- **Inline:** `W4TME` (Ke Chen) — lines 14720, 14755, 15811

### `mi0bot/Project Files/Source/Console/cwx.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 317, 626, 816, 1731, 1751, 1768, 1793, 2035 (+2 more)
- **Inline:** `W2PA` (Chris Codella) — line 1663

### `mi0bot/Project Files/Source/Console/database.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 744, 3425, 9844, 9860, 9886, 9910, 10038, 10336 (+11 more)
- **Inline:** `W2PA` (Chris Codella) — lines 9469, 9475, 9987, 11165, 11198, 11229, 11236
- **Inline:** `W4TME` (Ke Chen) — lines 5005, 5235

### `mi0bot/Project Files/Source/Console/display.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 249, 1972, 2038, 2054, 2135, 2272, 2368, 2415 (+48 more)

### `mi0bot/Project Files/Source/Console/dsp.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 828, 881, 883, 959, 960, 964

### `mi0bot/Project Files/Source/Console/enums.cs`
- Block: (none matched)
- **Inline:** `DH1KLM` (Sigi) — line 129
- **Inline:** `G8NJJ` (Laurence Barker) — lines 125, 126, 397
- **Inline:** `MI0BOT` (Reid Campbell) — lines 128, 396
- **Inline:** `MW0LGE` (Richard Samphire) — line 399

### `mi0bot/Project Files/Source/Console/eqform.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 203

### `mi0bot/Project Files/Source/Console/frmMeterDisplay.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 188

### `mi0bot/Project Files/Source/Console/radio.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 82, 105, 1090, 1183, 4243, 4258, 4272, 4294

### `mi0bot/Project Files/Source/Console/setup.cs`
- Block: (none matched)
- **Inline:** `DH1KLM` (Sigi) — lines 847, 6316, 6318, 6377, 6382, 6474, 6521, 6559 (+10 more)
- **Inline:** `G7KLJ` (Unknown) — line 127
- **Inline:** `G8NJJ` (Laurence Barker) — lines 6093, 6100, 6107, 6114, 6121, 6398, 8969, 9877 (+9 more)
- **Inline:** `MI0BOT` (Reid Campbell) — lines 1080, 1082, 1096, 2846, 4003, 5307, 5463, 5484 (+40 more)
- **Inline:** `MW0GLE` (Richard Samphire) — line 2763
- **Inline:** `MW0LGE` (Richard Samphire) — lines 134, 170, 176, 418, 576, 642, 646, 950 (+122 more)
- **Inline:** `W2PA` (Chris Codella) — lines 10348, 12648
- **Inline:** `W4WMT` (Bryan Rambo) — line 29395

### `mi0bot/Project Files/Source/Console/splash.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 60, 493, 523

### `mi0bot/Project Files/Source/Console/titlebar.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 65

### `mi0bot/Project Files/Source/Console/ucInfoBar.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 667, 683, 699, 715, 719, 731, 735, 747 (+2 more)

### `mi0bot/Project Files/Source/Console/ucMeter.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 660, 1059, 1188, 1200

### `mi0bot/Project Files/Source/Console/wideband.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 40

### `mi0bot/Project Files/Source/Console/xvtr.cs`
- Block: (none matched)
- **Inline:** `MI0BOT` (Reid Campbell) — line 6054

### `mi0bot/Project Files/Source/Midi2Cat/Midi2Cat.Data/CatCmdDb.cs`
- Block: (none matched)
- **Inline:** `DH1KLM` (Sigi) — lines 516, 518
- **Inline:** `MI0BOT` (Reid Campbell) — lines 534, 536
- **Inline:** `MW0LGE` (Richard Samphire) — lines 510, 520, 523, 525, 527, 529
- **Inline:** `W2PA` (Chris Codella) — lines 95, 380, 532

### `mi0bot/Project Files/Source/Midi2Cat/Midi2Cat.Data/ControllerMapping.cs`
- Block: (none matched)
- **Inline:** `W2PA` (Chris Codella) — line 31

### `mi0bot/Project Files/Source/Midi2Cat/Midi2Cat.Data/Database.cs`
- Block: (none matched)
- **Inline:** `W2PA` (Chris Codella) — line 252

### `mi0bot/Project Files/Source/Midi2Cat/Midi2Cat.IO/MidiDevice.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 59, 60, 61, 124, 648, 667, 805
- **Inline:** `W2PA` (Chris Codella) — lines 62, 650, 709, 713, 715, 718, 720, 725 (+4 more)

### `mi0bot/Project Files/Source/Midi2Cat/Midi2Cat.IO/MidiDeviceSetup.cs`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 183, 190
- **Inline:** `W2PA` (Chris Codella) — lines 251, 255, 257, 260, 262, 267

### `mi0bot/Project Files/Source/Midi2Cat/MidiMessageManager.cs`
- Block: (none matched)
- **Inline:** `W2PA` (Chris Codella) — lines 102, 118, 142, 143, 144, 145, 146, 147 (+24 more)

### `mi0bot/Project Files/Source/cmASIO/hostsample.cpp`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 327, 532, 690
- **Inline:** `W4WMT` (Bryan Rambo) — line 506

### `mi0bot/Project Files/Source/cmASIO/version.cpp`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 13

### `mi0bot/Project Files/Source/wdsp/RXA.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 663

### `mi0bot/Project Files/Source/wdsp/analyzer.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 1111, 1532

### `mi0bot/Project Files/Source/wdsp/analyzer.h`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 74

### `mi0bot/Project Files/Source/wdsp/eq.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 176

### `mi0bot/Project Files/Source/wdsp/rmatch.c`
- Block: (none matched)
- **Inline:** `W4WMT` (Bryan Rambo) — lines 734, 735

### `mi0bot/Project Files/Source/wdsp/ssql.c`
- Block: (none matched)
- **Inline:** `WU2O` (Scott) — lines 342, 352, 364

### `mi0bot/Project Files/lib/portaudio-19.7.0/include/portaudio.h`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — line 491

### `mi0bot/Project Files/lib/portaudio-19.7.0/src/common/pa_converters.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 120, 182, 194, 204, 214, 224, 234, 244 (+36 more)

### `mi0bot/Project Files/lib/portaudio-19.7.0/src/common/pa_converters.h`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 139, 150, 176, 187, 197, 206, 213, 224 (+1 more)

### `mi0bot/Project Files/lib/portaudio-19.7.0/src/common/pa_front.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 810, 1817

### `mi0bot/Project Files/lib/portaudio-19.7.0/src/hostapi/asio/pa_asio.cpp`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 1274, 1304

### `mi0bot/Project Files/lib/portaudio-19.7.0/src/hostapi/wasapi/pa_win_wasapi.c`
- Block: (none matched)
- **Inline:** `MW0LGE` (Richard Samphire) — lines 993, 2757, 2784

