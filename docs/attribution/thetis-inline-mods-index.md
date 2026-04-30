# Thetis Inline Mod Index

**Generated mechanically** by `scripts/generate-contributor-indexes.py` on `2026-04-30T12:51:05+00:00`
from corpus `docs/attribution/thetis-author-tags.json` (thetis@`501e3f51`, mi0bot@`c26a8a4`).

**Do NOT hand-edit this file.** To add or correct a contributor, edit `docs/attribution/thetis-author-tags.json` and re-run the generator.

Historical Pass 6a snapshot preserved at `thetis-inline-mods-index-v020-snapshot.md`.

## Summary

- Upstream sources scanned: 151
- Files with ≥1 inline marker: 151
- Total inline markers: 2947
- Distinct callsigns inline: 17

## Per-file markers

### `thetis/Project Files/Source/ChannelMaster/cmasio.c`

- **`MW0LGE`** (Richard Samphire): 3 markers
  - L58: `//[2.10.3.13]MW0LGE get explicit base channel indices for the stereo pair, default to 0 if none in registry`
  - L64: `//[2.10.3.13]MW0LGE the input mode, left = ch1, right = ch2, both = stereo`
  - L148: `//[2.10.3.13]MW0LGE added input mode, so can use ch1(L), ch2(R), or both for input`
- **`W4WMT`** (Bryan Rambo): 3 markers
  - L141: `// W4WMT cmASIO via Protocol 1`
  - L145: `// W4WMT cmASIO via Protocol 1`
  - L340: `// W4WMT cmASIO via Protocol 1`

### `thetis/Project Files/Source/ChannelMaster/cmasio.h`

- **`MW0LGE`** (Richard Samphire): 2 markers
  - L86: `//[2.10.3.13]MW0LGE added explicit base channel indices`
  - L89: `//[2.10.3.13]MW0LGE added input mode, so would use ch1, ch2, or both for input`
- **`W4WMT`** (Bryan Rambo): 1 marker
  - L84: `// W4WMT cmASIO via Protocol 1`

### `thetis/Project Files/Source/ChannelMaster/ivac.c`

- **`MW0LGE`** (Richard Samphire): 3 markers
  - L138: `// , 3); //[2.10.3.6]MW0LGE new 17.11.0 version of VS started complaining about this 3, has been there 8 months`
  - L211: `// [2.10.3.12]MW0LGE handle mono input devices`
  - L290: `//[2.10.3.12]MW0LGE ignore handling of output channels for now, always use 2`

### `thetis/Project Files/Source/ChannelMaster/netInterface.c`

- **`G8NJJ`** (Laurence Barker): 1 marker
  - L301: `//[2.10.3.5]MW0LGE changed to 0x10 from 0x16, spotted by G8NJJ`
- **`MW0LGE`** (Richard Samphire): 1 marker
  - L1178: `//[2.10.3.6]MW0LGE high priority always`

### `thetis/Project Files/Source/ChannelMaster/network.c`

- **`MW0LGE`** (Richard Samphire): 4 markers
  - L325: `//	sndbufsize = 0xfa000; // MW0LGE [2.9.0.8] from Warren, changed from 0x10000`
  - L708: `// only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`
  - L1457: `//CloseHandle(prn->hWriteThreadMain); //[2.10.3.7]MW0LGE moved below, this only gets created with USB, crash if connected to a p2, then m...`
  - L1458: `//CloseHandle(prn->hWriteThreadInitSem); //[2.10.3.7]MW0LGE moved below, this only gets created with USB`

### `thetis/Project Files/Source/ChannelMaster/network.h`

- **`G8NJJ`** (Laurence Barker): 1 marker
  - L423: `// ANAN-G2: added G8NJJ`
- **`MI0BOT`** (Reid Campbell): 1 marker
  - L422: `// MI0BOT: HL2 allocated number`

### `thetis/Project Files/Source/ChannelMaster/networkproto1.c`

- **`DH1KLM`** (Sigi): 1 marker
  - L612: `//[2.10.3.9]DH1KLM  //model needed as board type (prn->discovery.BoardType) is an OrionII`
- **`MW0LGE`** (Richard Samphire): 4 markers
  - L335: `// only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`
  - L353: `// only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`
  - L354: `// only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`
  - L355: `// only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`

### `thetis/Project Files/Source/ChannelMaster/pipe.c`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L229: `// wav recorder 0 //[2.10.3.6]MW0LGE moved after vac`

### `thetis/Project Files/Source/ChannelMaster/ring.c`

- **`W4WMT`** (Bryan Rambo): 1 marker
  - L53: `//W4WMT set this flag after writes & reset it after reads`

### `thetis/Project Files/Source/ChannelMaster/ring.h`

- **`W4WMT`** (Bryan Rambo): 1 marker
  - L35: `//W4WMT added flag to distinguish between full/empty when pointers are equal`

### `thetis/Project Files/Source/ChannelMaster/version.c`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L13: `// MW0LGE version number now stored in Thetis->Versions.cs file, to keep shared`

### `thetis/Project Files/Source/Console/AmpView.cs`

- **`MW0LGE`** (Richard Samphire): 4 markers
  - L89: `//[2.10.3.5]MW0LGE  #292`
  - L122: `// MW0LGE [2.9.0.8] re-factored to use fixed set of chart points, which get adjusted, these poins are re-init under certain conditions`
  - L259: `// MW0LGE [2.9.0.8] kept for code record`
  - L397: `//disp_data(); // MW0LGE [2.9.0.8] changed to an add once, update points method.`

### `thetis/Project Files/Source/Console/Andromeda/Andromeda.cs`

- **`G8NJJ`** (Laurence Barker): 4 markers
  - L16: `// G8NJJ: handlers for ARIES ATU`
  - L850: `// G8NJJ: handlers for Ganymeda 500W PA protection`
  - L1027: `// G8NJJ: define the Andromeda button bar menu`
  - L1147: `// G8NJJ: define the actons an Andromeda encoder can have`
- **`MW0LGE`** (Richard Samphire): 5 markers
  - L2141: `// MW0LGE`
  - L4121: `// MW0LGE [2.10.1.0]`
  - L4126: `// MW0LGE [2.9.0.7] in collapsed view, hide them all`
  - L4129: `// MW0LGE [2.10.1.0] andromeda mode dependant form fixes`
  - L4147: `//[2.10.3.5]MW0LGE we are expanded, so ok to move them always`

### `thetis/Project Files/Source/Console/Andromeda/SliderSettingsForm.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L1276: `//[2.10.3.5]MW0LGE`

### `thetis/Project Files/Source/Console/Andromeda/displaysettingsform.cs`

- **`G8NJJ`** (Laurence Barker): 5 markers
  - L326: `// G8NJJ: clear old list before re-adding!`
  - L330: `// G8NJJ: clear old list before re-adding!`
  - L334: `// G8NJJ: clear old list before re-adding!`
  - L338: `// G8NJJ: clear old list before re-adding!`
  - L342: `// G8NJJ: clear old list before re-adding!`

### `thetis/Project Files/Source/Console/CAT/CATCommands.cs`

- **`DH1KLM`** (Sigi): 2 markers
  - L7271: `//Reads or sets the Quick Play button status // DH1KLM`
  - L7294: `//Reads or sets the Quick Rec button status // DH1KLM`
- **`MW0LGE`** (Richard Samphire): 20 markers
  - L1638: `//[2.10.3.6]MW0LGE Fixes #460 - needed as Midi is from another thread`
  - L1909: `//[2.10.3.6]MW0LGE was 0`
  - L2216: `//[2.10.3.13]MW0LGE the above is wrong. The enum does not match the ZZDM input values`
  - L2665: `// [2.10.3.12]MW0LGE -- addleadingzeros uses parser.nAns which will be 0 as we might not be`
  - L2673: `// MW0LGE changed to take into consideration the flag`
  - L2706: `//[2.10.3.12]MW0LGE might be a good idea to use RX2 mode if RX2 enabled.`
  - L2716: `// [2.10.3.12]MW0LGE -- addleadingzeros uses parser.nAns which will be 0, as we might not be`
  - L2724: `// MW0LGE changed to take into consideration the flag`
  - L2734: `//[2.10.3.12]MW0LGE as above`
  - L3568: `//[2.10.3.4]MW0LGE put in the property WPM where it should be`
  - L4189: `//[2.10.3.6]MW0LGE can also have -. Could have changed catsructs but not sure on cat msg formats from other sources other than midi so le...`
  - L5236: `//Sets or reads the RX1 antenna //[2.3.10.6]MW0LGE https://github.com/ramdor/Thetis/issues/385`
  - L5272: `//Sets or reads the TX antenna //[2.3.10.6]MW0LGE https://github.com/ramdor/Thetis/issues/385`
  - L6136: `//MW0LGE [2.10.1.0]`
  - L6390: `//[2.10.3.5]MW0LGE 2 is vsql`
  - L6493: `//[2.10.3.5]MW0LGE 2 is vsql`
  - L8682: `//[2.10.1.0]MW0LGE enable/disable quick split mode`
  - L8704: `//[2.10.1.0]MW0LGE enable/disable quick split and turn split on/off at same time`
  - L8764: `//[2.10.3.9]MW0LGE refcator for speed`
  - L10388: `//[2.10.3.9]MW0LGE refactored, and tweaked for special cases to match original`
- **`W2PA`** (Chris Codella): 19 markers
  - L1037: `//-W2PA Sets or reads the APF gain (A for amplitude since G is taken)`
  - L1072: `//-W2PA Sets or reads the APF bandwidth`
  - L1249: `//-W2PA Sets or reads the APF button on/off status`
  - L1345: `//-W2PA Sets or reads the APF tune`
  - L2857: `//-W2PA Transfer focus to VAR1`
  - L2894: `//-W2PA Transfer focus to VAR1`
  - L5883: `// && console.RITOn)  //-W2PA Want to be able to change RIT value even if it's off`
  - L5896: `//-W2PA Changed to be same step in all modes.`
  - L6113: `// && console.RITOn)  //-W2PA Want to be able to change RIT value even if it's off`
  - L6127: `//-W2PA Changed to operate in all modes.`
  - L7043: `//-W2PA  Initiate PS Single Cal`
  - L7049: `//-W2PA  Toggle two tone test`
  - L7511: `//-W2PA  Out of alphabetical order a bit, but related to ZZVL above.`
  - L7514: `//-W2PA  Lock VFOA`
  - L7545: `//-W2PA  Lock VFOB`
  - L8187: `// && console.RITOn)  //-W2PA Want to be able to change RIT value even if it's off`
  - L8200: `//-W2PA Changed to be same step in all modes.`
  - L8399: `// && console.RITOn)  //-W2PA Want to be able to change RIT value even if it's off`
  - L8412: `//-W2PA Changed to be same step in all modes.`

### `thetis/Project Files/Source/Console/CAT/CATTester.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L64: `//MW0LGE [2.9.0.7] added .Tables[0]`

### `thetis/Project Files/Source/Console/CAT/SDRSerialPortII.cs`

- **`MW0LGE`** (Richard Samphire): 2 markers
  - L91: `//[2.10.3.9]MW0LGE`
  - L236: `//refactored for MAX performance [2.10.3.9]MW0LGE`

### `thetis/Project Files/Source/Console/CAT/SIOListenerII.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L38: `//[2.10.3]MW0LGE console.Activated += new EventHandler(console_Activated);  // why try to init on form activation? er...`

### `thetis/Project Files/Source/Console/CAT/TCPIPcatServer.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L277: `//[2.10.3.9]MW0LGE fixed to handle multiple messages ending in ;`

### `thetis/Project Files/Source/Console/ColorButton.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L162: `//[2.10.3.7]MW0LGE fixed, as it is not a good idea to use the clip rect to position the triangle`

### `thetis/Project Files/Source/Console/DiversityForm.cs`

- **`G8NJJ`** (Laurence Barker): 6 markers
  - L2219: `// added 31/3/2018 G8NJJ to allow access by CAT commands`
  - L2230: `// added 31/3/2018 G8NJJ to allow access by CAT commands`
  - L2253: `// added 6/8/2019 G8NJJ to allow access by Andromeda. Sets the appropriate gain.`
  - L2292: `// added 31/3/2018 G8NJJ to allow access by CAT commands`
  - L2311: `// added 31/3/2018 G8NJJ to allow access by CAT commands`
  - L2326: `// added 31/3/2018 G8NJJ to allow access by CAT commands`
- **`MW0LGE`** (Richard Samphire): 9 markers
  - L170: `//[2.10.3.6]MW0LGE these need to be high so that restore form can recover values`
  - L188: `//[2.10.3.6]MW0LGE implement memories. A bit of a hack to store all this in a text box, but it is easy with the saveform/restoreform`
  - L2280: `//[2.10.3.5]MW0LGE fixes #324`
  - L2299: `//[2.10.3.5]MW0LGE fixes #324`
  - L2318: `//[2.10.3.5]MW0LGE fixes #324`
  - L2544: `//[2.10.3.0]MW0LGE`
  - L2558: `//[2.10.3.0]MW0LGE`
  - L2572: `//[2.10.3.0]MW0LGE`
  - L2818: `//[2.10.3.5]MW0LGE old code, kept for reference`

### `thetis/Project Files/Source/Console/FilterForm.cs`

- **`MW0LGE`** (Richard Samphire): 5 markers
  - L677: `//[2.10.3.9]MW0LGE prevent update if already happening`
  - L696: `//[2.10.3.12]MW0LGE only call if changed, as the events will call us back, and we would get a stack overflow`
  - L786: `//[2.10.3.12]MW0LGE prevent update if changes happening from UpdateFilter. UpdateFilter will call this directly`
  - L808: `//[2.10.3.12]MW0LGE prevent update if changes happening from UpdateFilter. UpdateFilter will call this directly`
  - L920: `//[2.10.3.12]MW0LGE prevent update if changes happening from UpdateFilter. UpdateFilter will call this directly`
- **`W4TME`** (Ke Chen): 4 markers
  - L936: `//W4TME`
  - L937: `//W4TME`
  - L940: `//W4TME`
  - L941: `//W4TME`

### `thetis/Project Files/Source/Console/HPSDR/Alex.cs`

- **`G8NJJ`** (Laurence Barker): 1 marker
  - L377: `// G8NJJ support for external Aries ATU on antenna port 1`
- **`MW0LGE`** (Richard Samphire): 2 markers
  - L167: `//[2.10.3.6]MW0LGE`
  - L181: `//[2.10.3.6]MW0LGE else freq = Console.getConsole().VFOAFreq;`

### `thetis/Project Files/Source/Console/HPSDR/NetworkIO.cs`

- **`MW0LGE`** (Richard Samphire): 5 markers
  - L160: `//[2.10.3.9]MW0LGE added board check, issue icon shown in setup`
  - L432: `//                if (localEndPoint != null) //[2.10.3.7]MW0LGE null check added, and changed to tryparse`
  - L562: `//            //[2.10.3.9]MW0LGE added board check, issue icon shown in setup`
  - L901: `//                        //[2.10.3.5]MW0LGE sigh, MAC address in P1 is NOT at data[5], but at data[3]`
  - L1120: `//                return null;  //[2.10.3.7]MW0LGE added try catch`

### `thetis/Project Files/Source/Console/Memory/MemoryForm.cs`

- **`KE9NS`** (Darrin): 1 marker
  - L662: `//KE9NS ADD below is used to determine the URL from a drag and drop onto the memory form`
- **`MW0LGE`** (Richard Samphire): 3 markers
  - L935: `//[2.10.3.9]MW0LGE`
  - L1389: `//[2.10.3.6]MW0LGE uncommented so that the recording folder is shown. Fixes #457`
  - L1448: `//[2.10.3.5]MW0LGE it looks like MP3 support has been removed and commented out, above, about 5 years ago.`
- **`W4TME`** (Ke Chen): 2 markers
  - L470: `//W4TME`
  - L504: `//W4TME`

### `thetis/Project Files/Source/Console/MeterManager.cs`

- **`MW0LGE`** (Richard Samphire): 18 markers
  - L1800: `//[2.10.3.6]MW0LGE added for dev_6`
  - L4200: `//[2.10.1.0] MW0LGE needed because at init rx2 might not be enabled, and the init function will have been given -999.999 from console.vfo...`
  - L5695: `//[2.10.3.6]MW0LGE get all console info here, as everything will be at the correct state`
  - L6433: `//a.Add("meterIGSettings_" + ig.Value.ID, igs.ToString()); //[2.10.3.6]MW0LGE not used`
  - L6563: `//[2.10.3.7]MW0LGE // we have to dispose it because close() prevent this being freed up`
  - L6791: `//[2.10.3.9]MW0LGE order these once, pointless doing it every time we get a percentage !`
  - L6846: `//[2.10.1.0] MW0LGE used for on rx/tx fading`
  - L6851: `//[2.10.1.0] MW0LGE used when certain features turned off such as eq,leveler,cfc`
  - L6856: `//[2.10.30.9]MW0LGE this perc cache code totally refactored, and only caches to 2 decimal precision for the dB value, and is keyed on the...`
  - L9861: `//[2.10.3.9]MW0LGE fix, was using _rx1_band`
  - L18046: `//[2.10.3.9]MW0LGE update the data, prevents loads of updates`
  - L29467: `//[2.10.3.6]MW0LGE added m.vfosub >= 0`
  - L31618: `// [2.10.1.0] MW0LGE`
  - L33264: `//[2.10.3.5]MW0LGE note these are reverse RGB, we normally expect BGRA #289`
  - L33473: `//[2.10.3.5]MW0LGE note these are reverse RGB, we normally expect BGRA #289`
  - L33686: `//[2.10.3.5]MW0LGE note these are reverse RGB, we normally expect BGRA #289`
  - L39132: `//[2.10.3.9]MW0LGE refactor for speed`
  - L39319: `//[2.10.3.6]MW0LGE refactored to use Windows Imaging Component (WIC)`

### `thetis/Project Files/Source/Console/Midi2CatCommands.cs`

- **`DH1KLM`** (Sigi): 32 markers
  - L5524: `// DH1KLM`
  - L5536: `// DH1KLM`
  - L5548: `// DH1KLM`
  - L5560: `// DH1KLM`
  - L5572: `// DH1KLM`
  - L5595: `// DH1KLM`
  - L5618: `// DH1KLM`
  - L5635: `// DH1KLM`
  - L5675: `// DH1KLM`
  - L5700: `// DH1KLM`
  - L5725: `// DH1KLM`
  - L5748: `// DH1KLM`
  - L5807: `// DH1KLM`
  - L5825: `// DH1KLM`
  - L5844: `// DH1KLM`
  - L5863: `// DH1KLM`
  - L5882: `// DH1KLM`
  - L5901: `// DH1KLM`
  - L5920: `// DH1KLM`
  - L5939: `// DH1KLM`
  - *(+12 more)*
- **`MW0GE`** (Richard Samphire): 1 marker
  - L264: `//[2.10.3.6]MW0GE reimplemented`
- **`MW0LGE`** (Richard Samphire): 6 markers
  - L1166: `//[2.10.3.9]MW0LGE refactor for speed, as other implemation was just a complete mess`
  - L1718: `//[2.10.3.9]MW0LGE refactor for speed`
  - L1933: `//[2.10.3.9]MW0LGE refactor for speed`
  - L3119: `//[2.10.3.6]MW0LGE changed`
  - L3163: `//[2.10.3.6]MW0LGE seriously 0.078?????? crazy`
  - L6465: `// MW0LGE [2.9.0.7]`
- **`W2PA`** (Chris Codella): 90 markers
  - L45: `//-W2PA Necessary for changes to support Behringer PL-1 (and others)`
  - L93: `//-W2PA* Use the MidiMessageManager to send an update to the proper device/control LEDs`
  - L103: `//-W2PA Added device parameter to all commands to support return messages to devices with LEDs such as the Behringers`
  - L191: `//-W2PA This makes the function match its equivalent console function (e.g. mode gets copied)`
  - L207: `//-W2PA This makes the function match its equivalent console function (e.g. mode gets copied)`
  - L224: `//-W2PA This makes the function match its equivalent console function (e.g. mode gets copied)`
  - L288: `//-W2PA special handling for Behringer wheel style knobs`
  - L290: `//-W2PA for Behringer PL-1 type knob/wheel push button, to zero the setting`
  - L294: `//-W2PA for Behringer PL-1 knob/wheel`
  - L298: `//-W2PA for Behringer PL-1 knob/wheel`
  - L303: `//-W2PA Original code in Midi2Cat`
  - L318: `//-W2PA Rewritten to use a mini-wheel like the ones on the Behringer PL-1`
  - L319: `//-W2PA XIT_inc is different from RIT_inc because the CAT commands are different in CATCommands.cs`
  - L328: `//-W2PA special handling for Behringer wheel style knobs`
  - L330: `//-W2PA for Behringer PL-1 type knob/wheel push button, to zero the setting`
  - L338: `//-W2PA Changed to operate in all modes.`
  - L345: `//-W2PA Changed to operate in all modes.`
  - L350: `//-W2PA Original code in Midi2Cat`
  - L436: `//-W2PA Incremental volume control for Behringer PL-1 or similar knobs as wheels. Also added an item for Wheel in CatCmdDb.cs`
  - L446: `//-W2PA Ignore knob click presses`
  - *(+70 more)*

### `thetis/Project Files/Source/Console/N1MM.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L188: `// MW0LGE [2.9.0.7] fix issue where spectrum is offset by cwpitch`

### `thetis/Project Files/Source/Console/PSForm.cs`

- **`MW0LGE`** (Richard Samphire): 9 markers
  - L72: `// MW0LGE moved above restore, so that we actaully have console when control events fire because of restore form`
  - L157: `//[2.10.3.9]MW0LGE used by finder`
  - L409: `//[2.10.3.4]]MW0LGE leave it there until thetis closes`
  - L738: `//MW0LGE`
  - L754: `//[2.10.3.12]MW0LGE use rounding, to fix Banker's rounding issue`
  - L802: `//[2.10.3.7]MW0LGE show a warning if the setpk is different to what we expect for this hardware`
  - L815: `//MW0LGE use property`
  - L907: `//MW0LGE`
  - L1078: `//make copy of old, used in HasInfoChanged & CalibrationAttemptsChanged MW0LGE`
- **`W2PA`** (Chris Codella): 1 marker
  - L480: `//-W2PA Adds capability for CAT control via console`

### `thetis/Project Files/Source/Console/Skin.cs`

- **`MW0LGE`** (Richard Samphire): 4 markers
  - L1897: `// [2.10.3.9]MW0LGE`
  - L1917: `//[2.10.3.6]MW0LGE cache based on hash of image`
  - L1923: `//[2.10.2.2] MW0LGE`
  - L1973: `//[2.10.3.9]MW0LGE change to md5`

### `thetis/Project Files/Source/Console/TCIServer.cs`

- **`MW0LGE`** (Richard Samphire): 7 markers
  - L2116: `//MW0LGE [2.9.0.7] note we invert with -`
  - L2344: `//MW0LGE [2.9.0.7]`
  - L3869: `//change if needed [2.10.3.6]MW0LGE fixes #365`
  - L4319: `//[2.10.3.6]MW0LGE rumlog fills arg5 with Nil - spotted buy GW3JVB`
  - L6270: `//[2.10.3.9]MW0LGE fixes issue #559`
  - L7073: `// also send legacy command (EESDR3 does this)	MW0LGE [2.9.0.8]`
  - L7478: `//[2.10.3.9]MW0LGE also send out RX_CLICKED_ON_SPOT defaults to rx1 and vfoA`

### `thetis/Project Files/Source/Console/audio.cs`

- **`MW0LGE`** (Richard Samphire): 11 markers
  - L359: `//[2.10.0.4]MW0LGE fix issue with no RX2 audio when tx'ing on rx1`
  - L718: `//[2.10.3.4]MW0LGE added`
  - L1370: `//[2.10.3.4]MW0LGE changed to use tx block size`
  - L1484: `//a.Add("HPSDR (USB/UDP)"); //[2.10.3.4]MW0LGE removed`
  - L1494: `//a.Add(new PADeviceInfo("HPSDR (PCM A/D)", 0)); //[2.10.3.4]MW0LGE removed`
  - L1543: `//a.Add(new PADeviceInfo("HPSDR (PWM D/A)", 0)); //[2.10.3.4]MW0LGE removed`
  - L1812: `////MW0LGE [2.9.0.8] fix if protocol is changed at some point`
  - L1907: `//[2.10.3.5]MW0LGE resolves #338`
  - L1997: `//[2.10.3.5]MW0LGE added`
  - L2001: `//MW0LGE added all other scope modes`
  - L2083: `//[2.10.3.5]MW0LGE added`

### `thetis/Project Files/Source/Console/clsDBMan.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L84: `//[2.10.3.8]MW0LGE a custom string converter, that will default to HERMES if the model is not in the enum`

### `thetis/Project Files/Source/Console/clsHardwareSpecific.cs`

- **`DH1KLM`** (Sigi): 4 markers
  - L178: `//DH1KLM`
  - L180: `// DH1KLM: changed for compatibility reasons for OpenHPSDR compat. DIY PA/Filter boards`
  - L403: `//DH1KLM`
  - L420: `//DH1KLM`
- **`G8NJJ`** (Laurence Barker): 1 marker
  - L164: `// G8NJJ: likely to need further changes for PA`

### `thetis/Project Files/Source/Console/cmaster.cs`

- **`DH1KLM`** (Sigi): 4 markers
  - L625: `//DH1KLM`
  - L708: `//DH1KLM`
  - L744: `//DH1KLM`
  - L838: `//DH1KLM`
- **`MW0LGE`** (Richard Samphire): 8 markers
  - L669: `// DDC0+DDC1, port 1035, Call 1 Sends TX_freq data to both RX //MW0LGE_21d DUP on top panadaptor (Warren provided info), // MW0LGE [2.9.0...`
  - L999: `//[2.10.3.4]MW0LGE`
  - L1011: `//[2.10.3.5]MW0LGE`
  - L1017: `//[2.10.3.5]MW0LGE`
  - L1020: `//[2.10.3.4]MW0LGE`
  - L1105: `//[2.10.3.4]MW0LGE run on/off`
  - L1872: `//MW0LGE`
  - L2293: `//[2.10.3.4]MW0LGE use OutCountTX if moxing`

### `thetis/Project Files/Source/Console/common.cs`

- **`MW0LGE`** (Richard Samphire): 11 markers
  - L86: `// extend contains to be able to ignore case etc MW0LGE`
  - L582: `// MW0LGE very simple logger`
  - L604: `// MW0LGE very simple logger`
  - L636: `// MW0LGE moved here from titlebar.cs, and used by console.cs and others`
  - L679: `//MW0LGE build version number string once and return that`
  - L937: `//MW0LGE [2.9.0.8]`
  - L1372: `//[2.10.3.9]MW0LGE performance related`
  - L1387: `//[2.10.3.9]MW0LGE cpu/memory details`
  - L1478: `//[2.10.3.9]MW0LGE form scaling`
  - L1533: `//[2.10.3.9]MW0LGE cpu usage for this process`
  - L1561: `//[2.10.3.9]MW0LGE screensave/powersave prevention`

### `thetis/Project Files/Source/Console/console.Designer.cs`

- **`G8NJJ`** (Laurence Barker): 1 marker
  - L474: `// G8NJJ`

### `thetis/Project Files/Source/Console/console.cs`

- **`DH1KLM`** (Sigi): 31 markers
  - L6739: `//DH1KLM`
  - L8296: `//DH1KLM`
  - L10028: `//DH1KLM`
  - L10999: `//DH1KLM`
  - L11024: `//DH1KLM`
  - L11164: `//DH1KLM`
  - L11189: `//DH1KLM`
  - L11667: `//        HardwareSpecific.Model != HPSDRModel.REDPITAYA) return; //DH1KLM`
  - L11897: `// DH1KLM`
  - L14825: `//DH1KLM`
  - L14850: `//DH1KLM`
  - L15413: `//DH1KLM`
  - L18702: `//DH1KLM`
  - L19289: `//DH1KLM`
  - L19464: `//DH1KLM`
  - L21009: `//DH1KLM`
  - L21035: `//DH1KLM`
  - L22506: `//DH1KLM`
  - L24965: `//DH1KLM`
  - L25038: `//DH1KLM`
  - *(+11 more)*
- **`DK1HLM`** (Unknown): 1 marker
  - L6821: `//DK1HLM`
- **`G7KLJ`** (Unknown): 3 markers
  - L725: `// PA init thread - from G7KLJ changes - done as early as possible`
  - L1221: `// this should not happen, ever !  // G7KLJ's idea/implementation`
  - L1225: `// G7KLJ's idea/implementation`
- **`G8NJJ`** (Laurence Barker): 52 markers
  - L141: `// G8NJJ`
  - L492: `// G8NJJ`
  - L493: `//G8NJJ added`
  - L494: `//G8NJJ added`
  - L495: `//G8NJJ added`
  - L496: `//G8NJJ added`
  - L497: `//G8NJJ added`
  - L498: `//G8NJJ added`
  - L504: `// G8NJJ: Titlebar strings and button/encoder/menu definitions for Andromeda`
  - L6737: `// G8NJJ`
  - L6738: `// G8NJJ`
  - L7361: `// G8NJJ like CATBandGroup but covering SWL too`
  - L7497: `// G8NJJ added to allow labelling of buttons in popup form`
  - L8559: `// ANAN-G2, G21K    (G8NJJ)`
  - L11611: `// added G8NJJ`
  - L11630: `// added G8NJJ`
  - L12862: `// added G8NJJ to allow scaling of VOX gain CAT command to Thetis range which is typ -80 to 0, not 0 to 1000`
  - L13340: `// G8NJJ: return the set of strings in the combo box`
  - L13359: `// G8NJJ: return the set of strings in the combo box`
  - L13365: `// added G8NJJ`
  - *(+32 more)*
- **`IK4JPN`** (Unknown): 2 markers
  - L26345: `// IK4JPN+ 9/11/2014`
  - L26372: `// IK4JPN-`
- **`K2UE`** (George Donadio): 2 markers
  - L25985: `// in following 'if', K2UE recommends not checking open antenna for the 8000 model`
  - L26064: `// K2UE idea:  try to determine if Hi-Z or Lo-Z load`
- **`MW0LGE`** (Richard Samphire): 422 markers
  - L559: `//[2.10.3]MW0LGE`
  - L614: `//MW0LGE`
  - L866: `// initialise expandedSize so that we have something as a minimum to come back to from collapsed state //MW0LGE`
  - L902: `//[2.10.3.4]MW0LGE shutdown log remove`
  - L914: `//MW0LGE [2.9.0.8]`
  - L950: `//[2.10.1.0] MW0LGE initial call to setup check marks in status bar as a minimum`
  - L963: `//_frmFinder.WriteXmlFinderFile(AppDataPath); // note: this will only happen if not already there //[2.10.3.12]MW0LGE moved to shutdown`
  - L1044: `//[2.10.3.5]MW0LGE setup all status icon items`
  - L1067: `//MW0LGE now defaulted with m_tpDisplayThreadPriority, and updated by setupform`
  - L1209: `// MW0LGE used because some aspects of thetis test for null.`
  - L1215: `// MW0LGE implement SetupForm as singleton, with some level of thread safety (which is probably not needed)`
  - L1265: `//MW0LGE`
  - L1479: `//[2.10.3.12]MW0LGE command line adaptor select`
  - L1568: `// init the logger MW0LGE`
  - L1677: `//[2.10.3.6]MW0LGE changed to use invoke if needed as CATTCPIPserver uses this from another thread`
  - L2034: `//[2.10.3.1]MW0LGE make sure it is created on this thread, as the following serial`
  - L2116: `// MW0LGE certain things in setup need objects created in this instance, so we will`
  - L2133: `//[2.10.3.7]MW0LGE FM tx filter select, this was not being done at startup`
  - L2224: `//MW0LGE duped from above Display.Target = pnlDisplay;`
  - L2227: `//[2.10.3.13]MW0LGE moved to after the console window is showing`
  - *(+402 more)*
- **`W1CEG`** (Unknown): 2 markers
  - L37084: `// :W1CEG:`
  - L42364: `// W1CEG:  End`
- **`W2PA`** (Chris Codella): 45 markers
  - L52: `//-W2PA Necessary for Behringer MIDI changes`
  - L4971: `//-W2PA  The number of rig types in the imported DB matches the number in this version`
  - L4977: `//-W2PA  else the number has changed so don't import, leave the defaults alone`
  - L4985: `//-W2PA  The number of rig types in the imported DB matches the number in this version`
  - L4991: `//-W2PA  else the number has changed so don't import, leave the defaults alone`
  - L12978: `// QSK - a.k.a. full-break-in - Possible with Protocol-2 v1.7 or later  -W2PA`
  - L15752: `//-W2PA Added three new functions to make CAT functions match behavior of equivalent console functions.`
  - L16007: `//-W2PA This specifies the number of MIDI messages that cause a single tune step increment`
  - L18191: `//-W2PA`
  - L18196: `//-W2PA June 2017`
  - L25987: `//-W2PA Changed to allow 35w - some amplifier tuners need about 30w to reliably start working`
  - L28651: `//-W2PA Send LED update back to Behringer`
  - L28653: `//-W2PA Don't let the last LED go out until zero`
  - L28656: `//-W2PA Update LEDs on Behringer MIDI controller mini wheel`
  - L28739: `//-W2PA Update LEDs on Behringer MIDI controller`
  - L28827: `//-W2PA Update LEDs on Behringer MIDI controller`
  - L28829: `//-W2PA Don't let the last LED go out`
  - L30801: `//-W2PA Added to enable extended CAT control`
  - L31348: `// Lock the display //-W2PA Don't freeze display if we are zoomed in too far to fit the passband`
  - L31381: `//-W2PA If we tune beyond the display limits, re-center or scroll display, and keep going.  Original code above just stops tuning at edges.`
  - *(+25 more)*
- **`W4TME`** (Ke Chen): 3 markers
  - L14647: `//reset preset filter's center frequency - W4TME`
  - L14682: `//reset preset filter's center frequency - W4TME`
  - L15718: `// W4TME`

### `thetis/Project Files/Source/Console/cwx.cs`

- **`MW0LGE`** (Richard Samphire): 10 markers
  - L314: `//[2.10.3]MW0LGE swap`
  - L623: `////MW0LGE`
  - L813: `//[2.10.3.6]MW0LGE fixes #400`
  - L1728: `//[2.10.1.0] MW0LGE fixes #205`
  - L1748: `//MW0LGE moved here  from loadalpha`
  - L1765: `//[2.10.3.6]MW0LGE`
  - L1790: `//[2.10.3.6]MW0LGE fixes #400`
  - L2032: `//MW0LGE`
  - L2270: `//[2.10.3]MW0LGE swap`
  - L2544: `//[2.10.3.1]MW0LGE added to stop this form from being destroyed and the reference in console.cs being lost`
- **`W2PA`** (Chris Codella): 1 marker
  - L1660: `//W2PA Let F keys activate messages directly`

### `thetis/Project Files/Source/Console/database.cs`

- **`MW0LGE`** (Richard Samphire): 19 markers
  - L744: `//MW0LGE`
  - L3425: `//         // MW0LGE`
  - L9844: `//[2.10.1.0] MW0LGE added bSaveEmptyValues. All entries need to be in the database even if empty, because`
  - L9860: `//MW0LGE converted to Rows.Find because it is insanely faster, needs a primary key though`
  - L9886: `//[2.10.1.0] MW0LGE added bSaveEmptyValues. All entries need to be in the database even if empty, because`
  - L9910: `//MW0LGE converted to Rows.Find because it is insanely faster, needs a primary key though`
  - L10038: `//    // MW0LGE [2.9.0.8]`
  - L10336: `//                        //MW0LGE this db contains comboRadioModel, we need to pull over old radio selection from radio button implement...`
  - L10360: `//                    else if (thisKey.Contains("mnotchdb")) //[2.10.3]MW0LGE let defaul else import any`
  - L10443: `//                //[2.10.3]MW0LGE merge from old, any notches, fixes #236`
  - L10539: `//    _importedDS = manualImport;  // Prevents overwriting the new database file on next exit // [2.10.1.0] MW0LGE added flag manualImpor...`
  - L10555: `//[2.10.3.6]MW0LGE modified for new DB manager system`
  - L10594: `// MW0LGE [2.9.0.8]`
  - L10896: `//MW0LGE this db contains comboRadioModel, we need to pull over old radio selection from radio button implementation`
  - L10920: `//[2.10.3]MW0LGE let defaul else import any`
  - L10952: `// [2.10.3.6]MW0LGE changed, previously it would drop all meters from existing setup, and import`
  - L11023: `//[2.10.3]MW0LGE merge from old, any notches, fixes #236`
  - L11147: `//--MW0LGE`
  - L11582: `// MW0LGE added region1,2,3`
- **`W2PA`** (Chris Codella): 7 markers
  - L9469: `//-W2PA Write the database to a specific file`
  - L9475: `//-W2PA Write specific dataset to a file`
  - L9987: `////-W2PA New version of ImportDatabase to merge an old database or partly corruped one with a new default one`
  - L11164: `//-W2PA Basic validity checks of imported DataSet xml file`
  - L11197: `//-W2PA Expand an old TxProfile table into a newer one with more colunms. Fill in missing ones with default values.`
  - L11228: `//-W2PA Write a message to the ImportLog file during the import process`
  - L11235: `//-W2PA Original version of ImportDatabase`
- **`W4TME`** (Ke Chen): 2 markers
  - L5005: `// W4TME`
  - L5235: `// W4TME`

### `thetis/Project Files/Source/Console/display.cs`

- **`MW0LGE`** (Richard Samphire): 56 markers
  - L249: `// MW0LGE`
  - L1972: `// MW0LGE width 2`
  - L2038: `// MW0LGE width 2`
  - L2054: `//MW0LGE width 2`
  - L2135: `//MW0LGE`
  - L2272: `// MW0LGE width 2`
  - L2368: `// width 2 MW0LGE`
  - L2415: `//MW0LGE`
  - L3008: `//MW0LGE - these properties auto AGC on the waterfall, so that`
  - L3306: `//[2.10.3.12]MW0LGE adaptor info`
  - L3692: `//[2.10.1.0] MW0LGE spectrum/bitmaps may be cleared or bad, so wait to settle`
  - L4829: `//[2.10.1.0] MW0LGE fix issue #137`
  - L4830: `//[2.10.3.6]MW0LGE att_fix // change fixes #482`
  - L4972: `//if (grid_control) //[2.10.3.9]MW0LGE raw grid control option now just turns off the grid, all other elements are shown`
  - L5109: `//MW0LGE not used, as filling vertically with lines is faster than a filled very detailed`
  - L5824: `//            bool bElapsed = (_high_perf_timer.ElapsedMsec - _fLastFastAttackEnabledTimeRX1) > tmpDelay; //[2.10.1.0] MW0LGE change to t...`
  - L5859: `//            bool bElapsed = (_high_perf_timer.ElapsedMsec - _fLastFastAttackEnabledTimeRX2) > tmpDelay; //[2.10.1.0] MW0LGE change to t...`
  - L5868: `//[2.10.3.9]MW0LGE refactor to use refs, simplifies the code, removes unnecessary branching, general speed improvements`
  - L6700: `//MW0LGE [2.9.0.7]`
  - L6727: `//[2.10.3.9]MW0LGE changed from max`
  - *(+36 more)*

### `thetis/Project Files/Source/Console/dsp.cs`

- **`MW0LGE`** (Richard Samphire): 6 markers
  - L828: `// WDSP impulse cache - MW0LGE`
  - L881: `// MW0LGE [2.9.0.7] added pk + av + last`
  - L883: `// MW0LGE [2.9.0.7] added av`
  - L959: `// MW0LGE [2.9.0.7] not sure how these are real + imaginary values, they are ADC peak, and ADC average, according to rxa.c and rxa.h`
  - L960: `// input peak MW0LGE [2.9.0.7]`
  - L964: `// input average MW0LGE [2.9.0.7]`

### `thetis/Project Files/Source/Console/enums.cs`

- **`DH1KLM`** (Sigi): 1 marker
  - L129: `//DH1KLM`
- **`G8NJJ`** (Laurence Barker): 3 markers
  - L125: `//G8NJJ`
  - L126: `//G8NJJ`
  - L397: `// ANAN-G2: added G8NJJ`
- **`MI0BOT`** (Reid Campbell): 2 markers
  - L128: `//MI0BOT`
  - L396: `// MI0BOT`
- **`MW0LGE`** (Richard Samphire): 1 marker
  - L399: `// MW0LGE`

### `thetis/Project Files/Source/Console/eqform.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L203: `//MW0LGE [2.9.0.7]`

### `thetis/Project Files/Source/Console/frmMeterDisplay.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L188: `//IMPORTANT NOTE *****   //[2.10.3.7]MW0LGE`

### `thetis/Project Files/Source/Console/radio.cs`

- **`MW0LGE`** (Richard Samphire): 8 markers
  - L82: `//[2.10.3] MW0LGE`
  - L105: `//check for old wdspWisdom00 file - [2.10.3.9]MW0LGE`
  - L1090: `//[2.10.3.5]MW0LGE wave recorder volume normalise`
  - L1183: `// MW0LGE [2.9.0.8]`
  - L4243: `//MW0LGE return a notch that matches`
  - L4258: `//MW0LGE check if notch close by`
  - L4272: `//MW0LGE return list of notches in given bandwidth`
  - L4294: `//MW0LGE return first notch found that surrounds a given frequency in the given bandwidth`

### `thetis/Project Files/Source/Console/setup.cs`

- **`DH1KLM`** (Sigi): 18 markers
  - L847: `//DH1KLM`
  - L6162: `//DH1KLM`
  - L6164: `//DH1KLM`
  - L6223: `//DH1KLM`
  - L6228: `//DH1KLM`
  - L6309: `//DH1KLM`
  - L6356: `//DH1KLM`
  - L6394: `//DH1KLM`
  - L6426: `//DH1KLM`
  - L7102: `//DH1KLM`
  - L7192: `// MW0LGE [2.9.07] always initialise rx2 even if P1. Thanks to Reid (Gi8TME/Mi0BOT) and DH1KLM`
  - L15547: `//DH1KLM`
  - L15783: `//DH1KLM`
  - L15841: `//DH1KLM`
  - L20355: `//DH1KLM`
  - L20400: `//DH1KLM, not possible for Red Pitaya since ADC overflow pin not implement in Hard and Firmware`
  - L20401: `//DH1KLM, not possible for Red Pitaya since ADC overflow pin not implement in Hard and Firmware`
  - L23698: `//DH1KLM`
- **`G7KLJ`** (Unknown): 1 marker
  - L127: `//everything here moved to AfterConstructor, which is called during singleton instance // G7KLJ's idea/implementation`
- **`G8NJJ`** (Laurence Barker): 17 markers
  - L6029: `// added G8NJJ for Andromeda`
  - L6036: `// added G8NJJ for Aries`
  - L6043: `// added G8NJJ for Aries`
  - L6050: `// added G8NJJ for Ganymede`
  - L6057: `// added G8NJJ for Ganymede`
  - L6244: `// G8NJJ. will need more work ofr high power PA`
  - L8802: `// G8NJJ Saturn has QSK capability in any version.`
  - L9701: `// G8NJJ: all logic moved to the console properties code`
  - L9708: `// G8NJJ: all logic moved to the console properties code`
  - L9715: `// G8NJJ: all logic moved to the console properties code`
  - L9719: `// G8NJJ: setup control to select an Andromeda top bar when display is collapsed`
  - L9725: `// G8NJJ: all logic moved to the console properties code`
  - L9729: `// G8NJJ: setup control to select an Andromeda top bar when display is collapsed`
  - L9735: `// G8NJJ: all logic moved to the console properties code`
  - L16361: `// G8NJJ will need more work for ANAN_G2_1K (1KW PA)`
  - L20202: `// added G8NJJ`
  - L20253: `// added G8NJJ`
- **`MW0GLE`** (Richard Samphire): 1 marker
  - L2740: `//MW0GLE [2.10.3.6_dev4]`
- **`MW0LGE`** (Richard Samphire): 129 markers
  - L134: `//[2.10.3.9]MW0LGE atempt to get the model as soon as possile, before the getoptions, so that everything that relies on it at least has a...`
  - L170: `// MW0LGE note: this will allways cause the change event to fire, as the combobox does not contain any default value`
  - L176: `// MW0LGE gets shown/hidden by save/cancel/apply`
  - L418: `//MW0LGE [2.9.0.7] setup amp/volts calibration`
  - L576: `//MW0LGE [2.9.0.8]`
  - L642: `//MW0LGE [2.9.0.7]`
  - L646: `//MW0LGE [2.10.3.9]`
  - L946: `// MW0LGE in the case where we don't have a setting in the db, this function (initdisplaytab) is called, use console instead`
  - L949: `// MW0LGE`
  - L1046: `//MW0LGE`
  - L1047: `//MW0LGE`
  - L1048: `//MW0LGE`
  - L1049: `//MW0LGE`
  - L1050: `//MW0LGE`
  - L1051: `//MW0LGE`
  - L1052: `//MW0LGE`
  - L1096: `//MW0LGE [2.10.3.6]`
  - L1106: `//MW0LGE [2.9.0.7]`
  - L1439: `//[2.6.10.3]MW0LGE this had been removed, and was spotted after I diffed older version. I put it here to keep a record`
  - L1584: `//a.Add("chkRadioProtocolSelect_checkstate", chkRadioProtocolSelect.CheckState.ToString()); //[2.10.3.5]MW0LGE not used anymore`
  - *(+109 more)*
- **`W2PA`** (Chris Codella): 2 markers
  - L10172: `//-W2PA MIDI wheel as VFO sensitivity adjustments`
  - L12409: `//-W2PA Export a single TX Profile to send to someone else for importing.`
- **`W4WMT`** (Bryan Rambo): 1 marker
  - L28453: `//[2.10.3.5]W4WMT implements #87`

### `thetis/Project Files/Source/Console/splash.cs`

- **`MW0LGE`** (Richard Samphire): 3 markers
  - L60: `//MW0LGE`
  - L493: `//MW0LGE interesting, but removed`
  - L523: `//MW0LGE pnlStatus.Invalidate(m_rProgress);`

### `thetis/Project Files/Source/Console/titlebar.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L65: `//[2.10.2.2]MW0LGE use the auto generated class from pre build event for the BuildDate`

### `thetis/Project Files/Source/Console/ucInfoBar.cs`

- **`MW0LGE`** (Richard Samphire): 10 markers
  - L667: `//MW0LGE [2.9.0.7]`
  - L683: `//MW0LGE [2.9.0.7]`
  - L699: `//MW0LGE [2.9.0.7]`
  - L715: `//MW0LGE [2.9.0.7]`
  - L719: `// MW0LGE [2.9.0.7]`
  - L731: `//MW0LGE [2.9.0.7]`
  - L735: `// MW0LGE [2.9.0.7]`
  - L747: `//MW0LGE [2.9.0.7]`
  - L751: `// MW0LGE [2.9.0.7]`
  - L1189: `// return if any control is null, this should not happen  // MW0LGE [2.9.0.7]`

### `thetis/Project Files/Source/Console/ucMeter.cs`

- **`MW0LGE`** (Richard Samphire): 4 markers
  - L660: `//[2.10.3.4]MW0LGE added 'this' incase we are totally outside, fix issue where ui items get left visible`
  - L1059: `// && tmp.Length <= 21)  //[2.10.3.6_rc4] MW0LGE removed so that clients going forward can use older data as long as 13 entries exist`
  - L1188: `//[2.10.3.4]MW0LGE added 'this' incase we are totally outside, fix issue where ui items get left visible`
  - L1200: `//[2.10.3.6]MW0LGE no title or resize grabber, override by holding shift`

### `thetis/Project Files/Source/Console/wideband.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L40: `// MW0LGE pause the display thread and return`

### `thetis/Project Files/Source/Midi2Cat/Midi2Cat.Data/CatCmdDb.cs`

- **`DH1KLM`** (Sigi): 2 markers
  - L516: `// DH1KLM`
  - L518: `// DH1KLM`
- **`MW0LGE`** (Richard Samphire): 6 markers
  - L510: `// MW0LGE [2.9.0.7]`
  - L520: `// [2.10.3.6]MW0LGE`
  - L523: `// [2.10.3.12]MW0LGE`
  - L525: `// [2.10.3.12]MW0LGE`
  - L527: `// [2.10.3.12]MW0LGE`
  - L529: `// [2.10.3.12]MW0LGE`
- **`W2PA`** (Chris Codella): 3 markers
  - L95: `//-W2PA Renamed function to reflect actual meaning`
  - L380: `//-W2PA Added Wheel versions for Behrihger CMD PL-1 and others`
  - L532: `//-W2PA Added a toggle between A/B for main wheel`

### `thetis/Project Files/Source/Midi2Cat/Midi2Cat.Data/ControllerMapping.cs`

- **`W2PA`** (Chris Codella): 1 marker
  - L31: `//-W2PA Added device parameter for msg flow to MIDI device`

### `thetis/Project Files/Source/Midi2Cat/Midi2Cat.Data/Database.cs`

- **`W2PA`** (Chris Codella): 1 marker
  - L252: `//-W2PA To allow flowing commands back to MIDI devices`

### `thetis/Project Files/Source/Midi2Cat/Midi2Cat.IO/MidiDevice.cs`

- **`MW0LGE`** (Richard Samphire): 7 markers
  - L59: `//[2.10.3.4]MW0LGE`
  - L60: `//[2.10.3.4]MW0LGE`
  - L61: `//[2.10.3.4]MW0LGE`
  - L124: `//[2.10.3.5]MW0LGE`
  - L648: `//[2.10.3.4]MW0LGE added filter/mapper`
  - L667: `//[2.10.3.4]MW0LGE ignore LSB Controller message for 0-31 until we code up 14 bit support`
  - L805: `// undo controlID changes [2.10.3.4]MW0LGE`
- **`W2PA`** (Chris Codella): 12 markers
  - L62: `//-W2PA for switching Behringer PL-1 main wheel between VFO A and B.  Used in inDevice_ChannelMessageReceived() below and in MidiDeviceSe...`
  - L650: `//-W2PA Disambiguate messages from Behringer controllers`
  - L709: `//-W2PA Test for DeviceName is a Behringer type, and disambiguate the messages if necessary`
  - L713: `//-W2PA Trap Status E0 from Behringer PL-1 slider, change the ID to something that doesn't conflict with other controls`
  - L715: `//-W2PA Since PL-1 sliders send a variety of IDs, fix it at something unused: 73 (it uses 10 as its ID for LEDs)`
  - L718: `//-W2PA Trap PL-1 main wheel, change the ID to something that doesn't conflict with other controls, indicating VFO number (1=A, 2=B)`
  - L720: `//-W2PA This isn't the ID of any other control on the PL-1, so use for VFOB`
  - L725: `//-W2PA Trap Status E0 from Behringer CMD Micro controls, change the ID to something that doesn't conflict with buttons`
  - L893: `//-W2PA Set PL1 button light: 0=default, 1=alternate, 2=blink.   Used by Midi2CatCommands.`
  - L904: `//-W2PA Set PL1 button light: 0=default, 1=alternate, 2=blink, for PL1 button whose ID=inCtlID  Used by the UI console.`
  - L914: `//-W2PA Light LED number n for PL1 knob/wheel whose ID=inCtlID.  Used by the UI console.`
  - L921: `//-W2PA This is necessary for the PL-1 slider, which can cause a deluge of messages while also receiving them.`

### `thetis/Project Files/Source/Midi2Cat/Midi2Cat.IO/MidiDeviceSetup.cs`

- **`MW0LGE`** (Richard Samphire): 2 markers
  - L183: `//ControlId = FixBehringerCtlID(ControlId, Status); //-W2PA Disambiguate messages from Behringer controllers  //[2.10.3.5]MW0LGE lets not...`
  - L190: `//[2.10.3.5]MW0LGE so we can get the original control id`
- **`W2PA`** (Chris Codella): 6 markers
  - L251: `//private int FixBehringerCtlID(int ControlId, int Status) //-W2PA Test for DeviceName is a Behringer type, and disambiguate the messages...`
  - L255: `//        if (Status == 0xE0) //-W2PA Trap Status E0 from Behringer PL-1 slider, change the ID to something that doesn't conflict with ot...`
  - L257: `//            ControlId = 73;  //-W2PA I don't think this corresponds to the ID of any other control on the PL-1`
  - L260: `//        if (ControlId == 0x1F && MidiDevice.VFOSelect == 2) //-W2PA Trap PL-1 main wheel, change the ID to something that doesn't confl...`
  - L262: `//            ControlId = 77;  //-W2PA I don't think this corresponds to the ID of any other control on the PL-1, so use for VFOB`
  - L267: `//        if (Status == 0xB0) //-W2PA Trap Status E0 from Behringer CMD Micro controls, change the ID to something that doesn't conflict ...`

### `thetis/Project Files/Source/Midi2Cat/MidiMessageManager.cs`

- **`W2PA`** (Chris Codella): 32 markers
  - L102: `//-W2PA Used to get the index of the PL-1.  Assumes only one is connected.`
  - L118: `//-W2PA Send Pl-1 knob or slider LED update message (CMD Micro doesn't have such LEDs)`
  - L142: `//-W2PA Channel, Value, Status, ControlID, message bytes - but apparently it only pays attention to the string, others can all be zero`
  - L143: `//-W2PA CUE button`
  - L144: `//-W2PA play/pause button`
  - L145: `//-W2PA - button`
  - L146: `//-W2PA + button`
  - L147: `//-W2PA Rew button`
  - L148: `//-W2PA FF button`
  - L149: `//-W2PA SYNC button`
  - L150: `//-W2PA TAP button`
  - L151: `//-W2PA SCRATCH button`
  - L152: `//-W2PA 1 button`
  - L153: `//-W2PA 2 button`
  - L154: `//-W2PA 3 button`
  - L155: `//-W2PA 4 button`
  - L156: `//-W2PA 5 button`
  - L157: `//-W2PA 6 button`
  - L158: `//-W2PA 7 button`
  - L159: `//-W2PA 8 button`
  - *(+12 more)*

### `thetis/Project Files/Source/cmASIO/hostsample.cpp`

- **`MW0LGE`** (Richard Samphire): 3 markers
  - L327: `//[2.10.3.13]MW0LGE added explicit channel indices for input/output (0-based)`
  - L532: `//[2.10.3.13]MW0LGE pass explicit channel indices for input/output (0-based)`
  - L690: `//[2.10.3.13]MW0LGE get base channel numbers for input and output, and input mode`
- **`W4WMT`** (Bryan Rambo): 1 marker
  - L506: `//W4WMT`

### `thetis/Project Files/Source/cmASIO/version.cpp`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L13: `// MW0LGE version number now stored in Thetis->Versions.cs file, to keep shared`

### `thetis/Project Files/Source/wdsp/RXA.c`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L663: `// [2.10.3.13]MW0LGE carrier removal before AGC`

### `thetis/Project Files/Source/wdsp/analyzer.c`

- **`MW0LGE`** (Richard Samphire): 2 markers
  - L1111: `//[2.10.2]MW0LGE reset all the pixel and average buffers`
  - L1532: `// [2.10.3.13]MW0LGE`

### `thetis/Project Files/Source/wdsp/analyzer.h`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L74: `// [2.10.3.13]MW0LGE`

### `thetis/Project Files/Source/wdsp/eq.c`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L176: `// parametric eq with Q factor - Richard Samphire (c) 2026 - MW0LGE`

### `thetis/Project Files/Source/wdsp/rmatch.c`

- **`W4WMT`** (Bryan Rambo): 2 markers
  - L734: `// proportional feedback gain  ***W4WMT - reduce loop gain a bit for PowerSDR to help Primary buffers > 512`
  - L735: `// linearly interpolate cvar by sample  ***W4WMT - set varmode = 0 for PowerSDR (doesn't work otherwise!?!)`

### `thetis/Project Files/Source/wdsp/ssql.c`

- **`WU2O`** (Scott): 3 markers
  - L342: `// WU2O testing:  0.16 is a good default for 'threshold'; => 0.08 for 'wthresh'`
  - L352: `// WU2O testing:  0.1 is good default value`
  - L364: `// WU2O testing:  0.1 is good default value`

### `thetis/Project Files/lib/portaudio-19.7.0/include/portaudio.h`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L491: `//[2.10.3.11]MW0LGE portaudio note, new vals given to these, see commented code below`

### `thetis/Project Files/lib/portaudio-19.7.0/src/common/pa_converters.c`

- **`MW0LGE`** (Richard Samphire): 44 markers
  - L120: `//[2.10.3.11]MW0LGE portaudio`
  - L182: `//[2.10.3.11]MW0LGE portaudio`
  - L194: `//[2.10.3.11]MW0LGE portaudio`
  - L204: `//[2.10.3.11]MW0LGE portaudio`
  - L214: `//[2.10.3.11]MW0LGE portaudio`
  - L224: `//[2.10.3.11]MW0LGE portaudio`
  - L234: `//[2.10.3.11]MW0LGE portaudio`
  - L244: `//[2.10.3.11]MW0LGE portaudio`
  - L262: `//[2.10.3.11]MW0LGE portaudio start`
  - L272: `//[2.10.3.11]MW0LGE portaudio end`
  - L298: `//[2.10.3.11]MW0LGE portaudio`
  - L309: `//[2.10.3.11]MW0LGE portaudio`
  - L319: `//[2.10.3.11]MW0LGE portaudio`
  - L328: `//[2.10.3.11]MW0LGE portaudio`
  - L335: `//[2.10.3.11]MW0LGE portaudio`
  - L346: `//[2.10.3.11]MW0LGE portaudio`
  - L366: `//[2.10.3.11]MW0LGE portaudio`
  - L387: `//[2.10.3.11]MW0LGE portaudio`
  - L414: `//[2.10.3.11]MW0LGE portaudio`
  - L448: `//[2.10.3.11]MW0LGE portaudio`
  - *(+24 more)*

### `thetis/Project Files/lib/portaudio-19.7.0/src/common/pa_converters.h`

- **`MW0LGE`** (Richard Samphire): 9 markers
  - L139: `//[2.10.3]MW0LGE start`
  - L150: `//[2.10.3]MW0LGE end`
  - L176: `//[2.10.3]MW0LGE`
  - L187: `//[2.10.3]MW0LGE`
  - L197: `//[2.10.3]MW0LGE`
  - L206: `//[2.10.3]MW0LGE`
  - L213: `//[2.10.3]MW0LGE`
  - L224: `//[2.10.3]MW0LGE`
  - L257: `//[2.10.3]MW0LGE`

### `thetis/Project Files/lib/portaudio-19.7.0/src/common/pa_front.c`

- **`MW0LGE`** (Richard Samphire): 2 markers
  - L810: `//[2.10.3.11]MW0LGE portaudio`
  - L1817: `//[2.10.3.11]MW0LGE portaudio`

### `thetis/Project Files/lib/portaudio-19.7.0/src/hostapi/asio/pa_asio.cpp`

- **`MW0LGE`** (Richard Samphire): 2 markers
  - L1274: `//[2.10.3.11]MW0LGE portaudio W4WMT`
  - L1304: `//driver that cmASIO is using, if there is one //[2.10.3.11]MW0LGE portaudio W4WMT`

### `thetis/Project Files/lib/portaudio-19.7.0/src/hostapi/wasapi/pa_win_wasapi.c`

- **`MW0LGE`** (Richard Samphire): 3 markers
  - L993: `//[2.10.3.11]MW0LGE portaudio`
  - L2757: `//[2.10.3.11]MW0LGE portaudio`
  - L2784: `//[2.10.3.11]MW0LGE portaudio`

### `mi0bot/Project Files/Source/ChannelMaster/cmasio.c`

- **`MW0LGE`** (Richard Samphire): 3 markers
  - L58: `//[2.10.3.13]MW0LGE get explicit base channel indices for the stereo pair, default to 0 if none in registry`
  - L64: `//[2.10.3.13]MW0LGE the input mode, left = ch1, right = ch2, both = stereo`
  - L148: `//[2.10.3.13]MW0LGE added input mode, so can use ch1(L), ch2(R), or both for input`
- **`W4WMT`** (Bryan Rambo): 3 markers
  - L141: `// W4WMT cmASIO via Protocol 1`
  - L145: `// W4WMT cmASIO via Protocol 1`
  - L340: `// W4WMT cmASIO via Protocol 1`

### `mi0bot/Project Files/Source/ChannelMaster/cmasio.h`

- **`MW0LGE`** (Richard Samphire): 2 markers
  - L86: `//[2.10.3.13]MW0LGE added explicit base channel indices`
  - L89: `//[2.10.3.13]MW0LGE added input mode, so would use ch1, ch2, or both for input`
- **`W4WMT`** (Bryan Rambo): 1 marker
  - L84: `// W4WMT cmASIO via Protocol 1`

### `mi0bot/Project Files/Source/ChannelMaster/ivac.c`

- **`MW0LGE`** (Richard Samphire): 3 markers
  - L138: `// , 3); //[2.10.3.6]MW0LGE new 17.11.0 version of VS started complaining about this 3, has been there 8 months`
  - L211: `// [2.10.3.12]MW0LGE handle mono input devices`
  - L290: `//[2.10.3.12]MW0LGE ignore handling of output channels for now, always use 2`

### `mi0bot/Project Files/Source/ChannelMaster/netInterface.c`

- **`G8NJJ`** (Laurence Barker): 1 marker
  - L301: `//[2.10.3.5]MW0LGE changed to 0x10 from 0x16, spotted by G8NJJ`
- **`MI0BOT`** (Reid Campbell): 16 markers
  - L629: `// MI0BOT: This call used on HL2 to enable/disable PA`
  - L816: `// MI0BOT: Causes a HL2 to perform a reset on disconnect`
  - L825: `// MI0BOT: Control to swap the left and right audio channels send over P1`
  - L1215: `// MI0BOT: On the HL2 the CWX protocol has been updated to pass PTT in Bit 3`
  - L1458: `// MI0BOT: Controls the delay for PTT to Tx power out for HL2`
  - L1464: `// MI0BOT: Determines the delay until Tx/Rx change over after Tx buffer empties for the HL2`
  - L1470: `// MI0BOT: Initialises for a read of the I2C on the HL2`
  - L1501: `// MI0BOT: Initialises for a write of the I2C on the HL2 where a return is expected`
  - L1535: `// MI0BOT: Write to the I2C on the HL2 when a return is not expected`
  - L1566: `// MI0BOT: Handles the I2C responses for the HL2`
  - L1622: `// MI0BOT: HL2 I2C variables`
  - L1700: `// MI0BOT: HL2 control of PTT via CWX protocol`
  - L1709: `// MI0BOT: HL2`
  - L1710: `// MI0BOT: HL2`
  - L1724: `// MI0BOT: Intialised to not reset on software disconnect`
  - L1725: `// MI0BOT: Control to swap the left and right audio channels send over P1`
- **`MW0LGE`** (Richard Samphire): 1 marker
  - L1204: `//[2.10.3.6]MW0LGE high priority always`

### `mi0bot/Project Files/Source/ChannelMaster/network.c`

- **`MI0BOT`** (Reid Campbell): 3 markers
  - L82: `// MI0BOT: Added remotePort to allow remote access to several HL2s by different port number`
  - L222: `//RemotePort = remotePort;	// MI0BOT: Remote access over WAN using different port`
  - L1442: `// MI0BOT: Thread locking up, so timeout added.`
- **`MW0LGE`** (Richard Samphire): 4 markers
  - L327: `//	sndbufsize = 0xfa000; // MW0LGE [2.9.0.8] from Warren, changed from 0x10000`
  - L695: `// only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`
  - L1456: `//CloseHandle(prn->hWriteThreadMain); //[2.10.3.7]MW0LGE moved below, this only gets created with USB, crash if connected to a p2, then m...`
  - L1457: `//CloseHandle(prn->hWriteThreadInitSem); //[2.10.3.7]MW0LGE moved below, this only gets created with USB`

### `mi0bot/Project Files/Source/ChannelMaster/network.h`

- **`G8NJJ`** (Laurence Barker): 1 marker
  - L455: `// ANAN-G2: added G8NJJ`
- **`MI0BOT`** (Reid Campbell): 11 markers
  - L40: `// MI0BOT: For I2C use on HL2`
  - L109: `// MI0BOT: Reset on software disconnect`
  - L110: `// MI0BOT: Control to swap the left and right audio channels send over P1`
  - L113: `// MI0BOT: I2C data structure for HL2`
  - L264: `// MI0BOT: CWX enhancement for PTT on HL2`
  - L276: `// MI0BOT: Delay in TX buffer`
  - L277: `// MI0BOT: Delay before PTT drops out after TX buffer empties`
  - L442: `// MI0BOT: Allows different remote port for WAN access`
  - L454: `// MI0BOT: HL2 allocated number`
  - L488: `// MI0BOT: Different write loop for HL2`
  - L490: `// MI0BOT: Different read loop for HL2`

### `mi0bot/Project Files/Source/ChannelMaster/networkproto1.c`

- **`DH1KLM`** (Sigi): 1 marker
  - L781: `//[2.10.3.9]DH1KLM  //model needed as board type (prn->discovery.BoardType) is an OrionII`
- **`MI0BOT`** (Reid Campbell): 3 markers
  - L252: `// MI0BOT: Different read loop for HL2`
  - L1249: `// MI0BOT: Bit 3 in HL2 is used to signal PTT for CWX`
  - L1261: `// MI0BOT: Different write loop for HL2`
- **`MW0LGE`** (Richard Samphire): 4 markers
  - L338: `// only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`
  - L356: `// only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`
  - L357: `// only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`
  - L358: `// only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`

### `mi0bot/Project Files/Source/ChannelMaster/pipe.c`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L229: `// wav recorder 0 //[2.10.3.6]MW0LGE moved after vac`

### `mi0bot/Project Files/Source/ChannelMaster/ring.c`

- **`W4WMT`** (Bryan Rambo): 1 marker
  - L53: `//W4WMT set this flag after writes & reset it after reads`

### `mi0bot/Project Files/Source/ChannelMaster/ring.h`

- **`W4WMT`** (Bryan Rambo): 1 marker
  - L35: `//W4WMT added flag to distinguish between full/empty when pointers are equal`

### `mi0bot/Project Files/Source/ChannelMaster/version.c`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L13: `// MW0LGE version number now stored in Thetis->Versions.cs file, to keep shared`

### `mi0bot/Project Files/Source/Console/AmpView.cs`

- **`MW0LGE`** (Richard Samphire): 4 markers
  - L89: `//[2.10.3.5]MW0LGE  #292`
  - L122: `// MW0LGE [2.9.0.8] re-factored to use fixed set of chart points, which get adjusted, these poins are re-init under certain conditions`
  - L259: `// MW0LGE [2.9.0.8] kept for code record`
  - L397: `//disp_data(); // MW0LGE [2.9.0.8] changed to an add once, update points method.`

### `mi0bot/Project Files/Source/Console/Andromeda/Andromeda.cs`

- **`G8NJJ`** (Laurence Barker): 4 markers
  - L16: `// G8NJJ: handlers for ARIES ATU`
  - L850: `// G8NJJ: handlers for Ganymeda 500W PA protection`
  - L1027: `// G8NJJ: define the Andromeda button bar menu`
  - L1147: `// G8NJJ: define the actons an Andromeda encoder can have`
- **`MI0BOT`** (Reid Campbell): 1 marker
  - L4157: `// MI0BOT: Make the panel based on mode of the current transmit VFO`
- **`MW0LGE`** (Richard Samphire): 5 markers
  - L2141: `// MW0LGE`
  - L4121: `// MW0LGE [2.10.1.0]`
  - L4126: `// MW0LGE [2.9.0.7] in collapsed view, hide them all`
  - L4129: `// MW0LGE [2.10.1.0] andromeda mode dependant form fixes`
  - L4147: `//[2.10.3.5]MW0LGE we are expanded, so ok to move them always`

### `mi0bot/Project Files/Source/Console/Andromeda/SliderSettingsForm.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L1276: `//[2.10.3.5]MW0LGE`

### `mi0bot/Project Files/Source/Console/Andromeda/displaysettingsform.cs`

- **`G8NJJ`** (Laurence Barker): 5 markers
  - L326: `// G8NJJ: clear old list before re-adding!`
  - L330: `// G8NJJ: clear old list before re-adding!`
  - L334: `// G8NJJ: clear old list before re-adding!`
  - L338: `// G8NJJ: clear old list before re-adding!`
  - L342: `// G8NJJ: clear old list before re-adding!`

### `mi0bot/Project Files/Source/Console/CAT/CATCommands.cs`

- **`DH1KLM`** (Sigi): 2 markers
  - L7285: `//Reads or sets the Quick Play button status // DH1KLM`
  - L7308: `//Reads or sets the Quick Rec button status // DH1KLM`
- **`MI0BOT`** (Reid Campbell): 6 markers
  - L366: `// MI0BOT: Redirect CAT to VFO B`
  - L384: `// MI0BOT: Redirect CAT to VFO B`
  - L595: `// MI0BOT: Redirect CAT to VFO B`
  - L6897: `// MI0BOT: HL2`
  - L9969: `// MI0BOT: Redirect CAT to VFO B`
  - L10010: `// MI0BOT: Redirect CAT to VFO B`
- **`MW0LGE`** (Richard Samphire): 20 markers
  - L1651: `//[2.10.3.6]MW0LGE Fixes #460 - needed as Midi is from another thread`
  - L1922: `//[2.10.3.6]MW0LGE was 0`
  - L2229: `//[2.10.3.13]MW0LGE the above is wrong. The enum does not match the ZZDM input values`
  - L2678: `// [2.10.3.12]MW0LGE -- addleadingzeros uses parser.nAns which will be 0 as we might not be`
  - L2686: `// MW0LGE changed to take into consideration the flag`
  - L2719: `//[2.10.3.12]MW0LGE might be a good idea to use RX2 mode if RX2 enabled.`
  - L2729: `// [2.10.3.12]MW0LGE -- addleadingzeros uses parser.nAns which will be 0, as we might not be`
  - L2737: `// MW0LGE changed to take into consideration the flag`
  - L2747: `//[2.10.3.12]MW0LGE as above`
  - L3581: `//[2.10.3.4]MW0LGE put in the property WPM where it should be`
  - L4202: `//[2.10.3.6]MW0LGE can also have -. Could have changed catsructs but not sure on cat msg formats from other sources other than midi so le...`
  - L5249: `//Sets or reads the RX1 antenna //[2.3.10.6]MW0LGE https://github.com/ramdor/Thetis/issues/385`
  - L5285: `//Sets or reads the TX antenna //[2.3.10.6]MW0LGE https://github.com/ramdor/Thetis/issues/385`
  - L6149: `//MW0LGE [2.10.1.0]`
  - L6403: `//[2.10.3.5]MW0LGE 2 is vsql`
  - L6506: `//[2.10.3.5]MW0LGE 2 is vsql`
  - L8696: `//[2.10.1.0]MW0LGE enable/disable quick split mode`
  - L8718: `//[2.10.1.0]MW0LGE enable/disable quick split and turn split on/off at same time`
  - L8778: `//[2.10.3.9]MW0LGE refcator for speed`
  - L10410: `//[2.10.3.9]MW0LGE refactored, and tweaked for special cases to match original`
- **`W2PA`** (Chris Codella): 19 markers
  - L1050: `//-W2PA Sets or reads the APF gain (A for amplitude since G is taken)`
  - L1085: `//-W2PA Sets or reads the APF bandwidth`
  - L1262: `//-W2PA Sets or reads the APF button on/off status`
  - L1358: `//-W2PA Sets or reads the APF tune`
  - L2870: `//-W2PA Transfer focus to VAR1`
  - L2907: `//-W2PA Transfer focus to VAR1`
  - L5896: `// && console.RITOn)  //-W2PA Want to be able to change RIT value even if it's off`
  - L5909: `//-W2PA Changed to be same step in all modes.`
  - L6126: `// && console.RITOn)  //-W2PA Want to be able to change RIT value even if it's off`
  - L6140: `//-W2PA Changed to operate in all modes.`
  - L7057: `//-W2PA  Initiate PS Single Cal`
  - L7063: `//-W2PA  Toggle two tone test`
  - L7525: `//-W2PA  Out of alphabetical order a bit, but related to ZZVL above.`
  - L7528: `//-W2PA  Lock VFOA`
  - L7559: `//-W2PA  Lock VFOB`
  - L8201: `// && console.RITOn)  //-W2PA Want to be able to change RIT value even if it's off`
  - L8214: `//-W2PA Changed to be same step in all modes.`
  - L8413: `// && console.RITOn)  //-W2PA Want to be able to change RIT value even if it's off`
  - L8426: `//-W2PA Changed to be same step in all modes.`

### `mi0bot/Project Files/Source/Console/CAT/CATParser.cs`

- **`MI0BOT`** (Reid Campbell): 5 markers
  - L187: `// MI0BOT: Redirect CAT to VFO B`
  - L788: `// MI0BOT: Redirect CAT to VFO B`
  - L820: `// MI0BOT: Redirect CAT to VFO B`
  - L831: `// MI0BOT: Redirect CAT to VFO B`
  - L959: `// MI0BOT: Redirect CAT to VFO B`

### `mi0bot/Project Files/Source/Console/CAT/CATTester.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L64: `//MW0LGE [2.9.0.7] added .Tables[0]`

### `mi0bot/Project Files/Source/Console/CAT/SDRSerialPortII.cs`

- **`MW0LGE`** (Richard Samphire): 2 markers
  - L91: `//[2.10.3.9]MW0LGE`
  - L236: `//refactored for MAX performance [2.10.3.9]MW0LGE`

### `mi0bot/Project Files/Source/Console/CAT/SIOListenerII.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L38: `//[2.10.3]MW0LGE console.Activated += new EventHandler(console_Activated);  // why try to init on form activation? er...`

### `mi0bot/Project Files/Source/Console/CAT/TCPIPcatServer.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L277: `//[2.10.3.9]MW0LGE fixed to handle multiple messages ending in ;`

### `mi0bot/Project Files/Source/Console/ColorButton.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L162: `//[2.10.3.7]MW0LGE fixed, as it is not a good idea to use the clip rect to position the triangle`

### `mi0bot/Project Files/Source/Console/DiversityForm.cs`

- **`G8NJJ`** (Laurence Barker): 6 markers
  - L2219: `// added 31/3/2018 G8NJJ to allow access by CAT commands`
  - L2230: `// added 31/3/2018 G8NJJ to allow access by CAT commands`
  - L2253: `// added 6/8/2019 G8NJJ to allow access by Andromeda. Sets the appropriate gain.`
  - L2292: `// added 31/3/2018 G8NJJ to allow access by CAT commands`
  - L2311: `// added 31/3/2018 G8NJJ to allow access by CAT commands`
  - L2326: `// added 31/3/2018 G8NJJ to allow access by CAT commands`
- **`MW0LGE`** (Richard Samphire): 9 markers
  - L170: `//[2.10.3.6]MW0LGE these need to be high so that restore form can recover values`
  - L188: `//[2.10.3.6]MW0LGE implement memories. A bit of a hack to store all this in a text box, but it is easy with the saveform/restoreform`
  - L2280: `//[2.10.3.5]MW0LGE fixes #324`
  - L2299: `//[2.10.3.5]MW0LGE fixes #324`
  - L2318: `//[2.10.3.5]MW0LGE fixes #324`
  - L2544: `//[2.10.3.0]MW0LGE`
  - L2558: `//[2.10.3.0]MW0LGE`
  - L2572: `//[2.10.3.0]MW0LGE`
  - L2818: `//[2.10.3.5]MW0LGE old code, kept for reference`

### `mi0bot/Project Files/Source/Console/FilterForm.cs`

- **`MW0LGE`** (Richard Samphire): 5 markers
  - L677: `//[2.10.3.9]MW0LGE prevent update if already happening`
  - L696: `//[2.10.3.12]MW0LGE only call if changed, as the events will call us back, and we would get a stack overflow`
  - L786: `//[2.10.3.12]MW0LGE prevent update if changes happening from UpdateFilter. UpdateFilter will call this directly`
  - L808: `//[2.10.3.12]MW0LGE prevent update if changes happening from UpdateFilter. UpdateFilter will call this directly`
  - L920: `//[2.10.3.12]MW0LGE prevent update if changes happening from UpdateFilter. UpdateFilter will call this directly`
- **`W4TME`** (Ke Chen): 4 markers
  - L936: `//W4TME`
  - L937: `//W4TME`
  - L940: `//W4TME`
  - L941: `//W4TME`

### `mi0bot/Project Files/Source/Console/HPSDR/Alex.cs`

- **`G8NJJ`** (Laurence Barker): 1 marker
  - L408: `// G8NJJ support for external Aries ATU on antenna port 1`
- **`MI0BOT`** (Reid Campbell): 4 markers
  - L366: `// MI0BOT: Alt RX has been requested or TX and Rx are not the same`
  - L389: `// MI0BOT: Antenna not the same is valid`
  - L426: `// MI0BOT: Transmit antenna is being used for reception in split aerial operation`
  - L446: `// MI0BOT: Sets the aerial controls on the I/O board`
- **`MW0LGE`** (Richard Samphire): 2 markers
  - L167: `//[2.10.3.6]MW0LGE`
  - L181: `//[2.10.3.6]MW0LGE else freq = Console.getConsole().VFOAFreq;`

### `mi0bot/Project Files/Source/Console/HPSDR/NetworkIO.cs`

- **`MW0LGE`** (Richard Samphire): 5 markers
  - L160: `//[2.10.3.9]MW0LGE added board check, issue icon shown in setup`
  - L432: `//                if (localEndPoint != null) //[2.10.3.7]MW0LGE null check added, and changed to tryparse`
  - L562: `//            //[2.10.3.9]MW0LGE added board check, issue icon shown in setup`
  - L901: `//                        //[2.10.3.5]MW0LGE sigh, MAC address in P1 is NOT at data[5], but at data[3]`
  - L1120: `//                return null;  //[2.10.3.7]MW0LGE added try catch`

### `mi0bot/Project Files/Source/Console/HPSDR/NetworkIOImports.cs`

- **`MI0BIT`** (Unknown): 8 markers
  - L384: `// MI0BIT: Pass hardware TX latency to HL2`
  - L387: `// MI0BIT: Pass hardware PTT hang to HL2`
  - L390: `// MI0BIT: Control reset on network disconnect`
  - L393: `// MI0BIT: Control to swap the left and right audio channels send over P1`
  - L396: `// MI0BIT: I2C read start for HL2`
  - L399: `// MI0BIT: I2C write start for HL2`
  - L402: `// MI0BIT: I2C write for HL2`
  - L405: `// MI0BIT: I2C read response for HL2`
- **`MI0BOT`** (Reid Campbell): 1 marker
  - L321: `// MI0BOT: Pass PTT for CWX`

### `mi0bot/Project Files/Source/Console/HPSDR/Penny.cs`

- **`MI0BOT`** (Reid Campbell): 2 markers
  - L174: `// MI0BOT: Select correct LPF for 2 receivers`
  - L185: `// MI0BOT: Select the filter for the high band`

### `mi0bot/Project Files/Source/Console/HPSDR/clsRadioDiscovery.cs`

- **`MI0BOT`** (Reid Campbell): 9 markers
  - L514: `// MI0BOT: Extra info from discovery for HL2`
  - L518: `// MI0BOT: Extra info from discovery for HL2`
  - L519: `// MI0BOT: Extra info from discovery for HL2`
  - L1065: `// MI0BOT: Extra info from discovery for HL2`
  - L1117: `// MI0BOT: Extra info from discovery for HL2`
  - L1118: `// MI0BOT: Extra info from discovery for HL2`
  - L1119: `// MI0BOT: Extra info from discovery for HL2`
  - L1169: `// MI0BOT: Extra info from discovery for HL2`
  - L1239: `// MI0BOT: HL2 added`

### `mi0bot/Project Files/Source/Console/Memory/MemoryForm.cs`

- **`KE9NS`** (Darrin): 1 marker
  - L662: `//KE9NS ADD below is used to determine the URL from a drag and drop onto the memory form`
- **`MW0LGE`** (Richard Samphire): 3 markers
  - L935: `//[2.10.3.9]MW0LGE`
  - L1389: `//[2.10.3.6]MW0LGE uncommented so that the recording folder is shown. Fixes #457`
  - L1448: `//[2.10.3.5]MW0LGE it looks like MP3 support has been removed and commented out, above, about 5 years ago.`
- **`W4TME`** (Ke Chen): 2 markers
  - L470: `//W4TME`
  - L504: `//W4TME`

### `mi0bot/Project Files/Source/Console/MeterManager.cs`

- **`MW0LGE`** (Richard Samphire): 18 markers
  - L1800: `//[2.10.3.6]MW0LGE added for dev_6`
  - L4200: `//[2.10.1.0] MW0LGE needed because at init rx2 might not be enabled, and the init function will have been given -999.999 from console.vfo...`
  - L5695: `//[2.10.3.6]MW0LGE get all console info here, as everything will be at the correct state`
  - L6433: `//a.Add("meterIGSettings_" + ig.Value.ID, igs.ToString()); //[2.10.3.6]MW0LGE not used`
  - L6563: `//[2.10.3.7]MW0LGE // we have to dispose it because close() prevent this being freed up`
  - L6791: `//[2.10.3.9]MW0LGE order these once, pointless doing it every time we get a percentage !`
  - L6846: `//[2.10.1.0] MW0LGE used for on rx/tx fading`
  - L6851: `//[2.10.1.0] MW0LGE used when certain features turned off such as eq,leveler,cfc`
  - L6856: `//[2.10.30.9]MW0LGE this perc cache code totally refactored, and only caches to 2 decimal precision for the dB value, and is keyed on the...`
  - L9861: `//[2.10.3.9]MW0LGE fix, was using _rx1_band`
  - L18046: `//[2.10.3.9]MW0LGE update the data, prevents loads of updates`
  - L29467: `//[2.10.3.6]MW0LGE added m.vfosub >= 0`
  - L31618: `// [2.10.1.0] MW0LGE`
  - L33264: `//[2.10.3.5]MW0LGE note these are reverse RGB, we normally expect BGRA #289`
  - L33473: `//[2.10.3.5]MW0LGE note these are reverse RGB, we normally expect BGRA #289`
  - L33686: `//[2.10.3.5]MW0LGE note these are reverse RGB, we normally expect BGRA #289`
  - L39132: `//[2.10.3.9]MW0LGE refactor for speed`
  - L39319: `//[2.10.3.6]MW0LGE refactored to use Windows Imaging Component (WIC)`

### `mi0bot/Project Files/Source/Console/Midi2CatCommands.cs`

- **`DH1KLM`** (Sigi): 32 markers
  - L5524: `// DH1KLM`
  - L5536: `// DH1KLM`
  - L5548: `// DH1KLM`
  - L5560: `// DH1KLM`
  - L5572: `// DH1KLM`
  - L5595: `// DH1KLM`
  - L5618: `// DH1KLM`
  - L5635: `// DH1KLM`
  - L5675: `// DH1KLM`
  - L5700: `// DH1KLM`
  - L5725: `// DH1KLM`
  - L5748: `// DH1KLM`
  - L5807: `// DH1KLM`
  - L5825: `// DH1KLM`
  - L5844: `// DH1KLM`
  - L5863: `// DH1KLM`
  - L5882: `// DH1KLM`
  - L5901: `// DH1KLM`
  - L5920: `// DH1KLM`
  - L5939: `// DH1KLM`
  - *(+12 more)*
- **`MI0BOT`** (Reid Campbell): 2 markers
  - L6381: `//MI0BOT: CW keying via MIDI`
  - L6395: `//MI0BOT: CW PTT via MIDI`
- **`MW0GE`** (Richard Samphire): 1 marker
  - L264: `//[2.10.3.6]MW0GE reimplemented`
- **`MW0LGE`** (Richard Samphire): 6 markers
  - L1166: `//[2.10.3.9]MW0LGE refactor for speed, as other implemation was just a complete mess`
  - L1718: `//[2.10.3.9]MW0LGE refactor for speed`
  - L1933: `//[2.10.3.9]MW0LGE refactor for speed`
  - L3119: `//[2.10.3.6]MW0LGE changed`
  - L3163: `//[2.10.3.6]MW0LGE seriously 0.078?????? crazy`
  - L6493: `// MW0LGE [2.9.0.7]`
- **`W2PA`** (Chris Codella): 90 markers
  - L45: `//-W2PA Necessary for changes to support Behringer PL-1 (and others)`
  - L93: `//-W2PA* Use the MidiMessageManager to send an update to the proper device/control LEDs`
  - L103: `//-W2PA Added device parameter to all commands to support return messages to devices with LEDs such as the Behringers`
  - L191: `//-W2PA This makes the function match its equivalent console function (e.g. mode gets copied)`
  - L207: `//-W2PA This makes the function match its equivalent console function (e.g. mode gets copied)`
  - L224: `//-W2PA This makes the function match its equivalent console function (e.g. mode gets copied)`
  - L288: `//-W2PA special handling for Behringer wheel style knobs`
  - L290: `//-W2PA for Behringer PL-1 type knob/wheel push button, to zero the setting`
  - L294: `//-W2PA for Behringer PL-1 knob/wheel`
  - L298: `//-W2PA for Behringer PL-1 knob/wheel`
  - L303: `//-W2PA Original code in Midi2Cat`
  - L318: `//-W2PA Rewritten to use a mini-wheel like the ones on the Behringer PL-1`
  - L319: `//-W2PA XIT_inc is different from RIT_inc because the CAT commands are different in CATCommands.cs`
  - L328: `//-W2PA special handling for Behringer wheel style knobs`
  - L330: `//-W2PA for Behringer PL-1 type knob/wheel push button, to zero the setting`
  - L338: `//-W2PA Changed to operate in all modes.`
  - L345: `//-W2PA Changed to operate in all modes.`
  - L350: `//-W2PA Original code in Midi2Cat`
  - L436: `//-W2PA Incremental volume control for Behringer PL-1 or similar knobs as wheels. Also added an item for Wheel in CatCmdDb.cs`
  - L446: `//-W2PA Ignore knob click presses`
  - *(+70 more)*

### `mi0bot/Project Files/Source/Console/N1MM.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L188: `// MW0LGE [2.9.0.7] fix issue where spectrum is offset by cwpitch`

### `mi0bot/Project Files/Source/Console/PSForm.cs`

- **`MI0BOT`** (Reid Campbell): 5 markers
  - L758: `// MI0BOT: Handle the Not A Number situation`
  - L759: `// MI0BOT: Handle - infinity`
  - L760: `// MI0BOT: Handle + infinity`
  - L788: `//MI0BOT: HL2 can handle negative up to -28, just let it be handled in ATTOnTx section`
  - L1144: `// MI0BOT: Needed seperate function for HL2 as`
- **`MW0LGE`** (Richard Samphire): 9 markers
  - L72: `// MW0LGE moved above restore, so that we actaully have console when control events fire because of restore form`
  - L157: `//[2.10.3.9]MW0LGE used by finder`
  - L409: `//[2.10.3.4]]MW0LGE leave it there until thetis closes`
  - L740: `//MW0LGE`
  - L772: `//[2.10.3.12]MW0LGE use rounding, to fix Banker's rounding issue`
  - L830: `//[2.10.3.7]MW0LGE show a warning if the setpk is different to what we expect for this hardware`
  - L843: `//MW0LGE use property`
  - L935: `//MW0LGE`
  - L1106: `//make copy of old, used in HasInfoChanged & CalibrationAttemptsChanged MW0LGE`
- **`W2PA`** (Chris Codella): 1 marker
  - L480: `//-W2PA Adds capability for CAT control via console`

### `mi0bot/Project Files/Source/Console/Skin.cs`

- **`MW0LGE`** (Richard Samphire): 4 markers
  - L1897: `// [2.10.3.9]MW0LGE`
  - L1917: `//[2.10.3.6]MW0LGE cache based on hash of image`
  - L1923: `//[2.10.2.2] MW0LGE`
  - L1973: `//[2.10.3.9]MW0LGE change to md5`

### `mi0bot/Project Files/Source/Console/TCIServer.cs`

- **`MW0LGE`** (Richard Samphire): 7 markers
  - L2116: `//MW0LGE [2.9.0.7] note we invert with -`
  - L2344: `//MW0LGE [2.9.0.7]`
  - L3869: `//change if needed [2.10.3.6]MW0LGE fixes #365`
  - L4319: `//[2.10.3.6]MW0LGE rumlog fills arg5 with Nil - spotted buy GW3JVB`
  - L6270: `//[2.10.3.9]MW0LGE fixes issue #559`
  - L7073: `// also send legacy command (EESDR3 does this)	MW0LGE [2.9.0.8]`
  - L7478: `//[2.10.3.9]MW0LGE also send out RX_CLICKED_ON_SPOT defaults to rx1 and vfoA`

### `mi0bot/Project Files/Source/Console/audio.cs`

- **`MW0LGE`** (Richard Samphire): 11 markers
  - L359: `//[2.10.0.4]MW0LGE fix issue with no RX2 audio when tx'ing on rx1`
  - L717: `//[2.10.3.4]MW0LGE added`
  - L1369: `//[2.10.3.4]MW0LGE changed to use tx block size`
  - L1483: `//a.Add("HPSDR (USB/UDP)"); //[2.10.3.4]MW0LGE removed`
  - L1493: `//a.Add(new PADeviceInfo("HPSDR (PCM A/D)", 0)); //[2.10.3.4]MW0LGE removed`
  - L1542: `//a.Add(new PADeviceInfo("HPSDR (PWM D/A)", 0)); //[2.10.3.4]MW0LGE removed`
  - L1811: `////MW0LGE [2.9.0.8] fix if protocol is changed at some point`
  - L1906: `//[2.10.3.5]MW0LGE resolves #338`
  - L1996: `//[2.10.3.5]MW0LGE added`
  - L2000: `//MW0LGE added all other scope modes`
  - L2082: `//[2.10.3.5]MW0LGE added`

### `mi0bot/Project Files/Source/Console/clsDBMan.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L84: `//[2.10.3.8]MW0LGE a custom string converter, that will default to HERMES if the model is not in the enum`

### `mi0bot/Project Files/Source/Console/clsHardwareSpecific.cs`

- **`DH1KLM`** (Sigi): 4 markers
  - L185: `//DH1KLM`
  - L187: `// DH1KLM: changed for compatibility reasons for OpenHPSDR compat. DIY PA/Filter boards`
  - L424: `//DH1KLM`
  - L441: `//DH1KLM`
- **`G8NJJ`** (Laurence Barker): 1 marker
  - L171: `// G8NJJ: likely to need further changes for PA`

### `mi0bot/Project Files/Source/Console/cmaster.cs`

- **`DH1KLM`** (Sigi): 4 markers
  - L631: `//DH1KLM`
  - L715: `//DH1KLM`
  - L751: `//DH1KLM`
  - L846: `//DH1KLM`
- **`MI0BOT`** (Reid Campbell): 6 markers
  - L536: `//puresignal.SetPSHWPeak(txch, 0.2899);     // MI0BOT: Corrected for in CMLoadRouterAll()`
  - L566: `// MI0BOT: Correct for correct PS value`
  - L595: `// MI0BOT: HL2`
  - L685: `// MI0BOT: HL2`
  - L807: `// MI0BOT: HL2`
  - L885: `// MI0BOT: HL2`
- **`MW0LGE`** (Richard Samphire): 8 markers
  - L675: `// DDC0+DDC1, port 1035, Call 1 Sends TX_freq data to both RX //MW0LGE_21d DUP on top panadaptor (Warren provided info), // MW0LGE [2.9.0...`
  - L1008: `//[2.10.3.4]MW0LGE`
  - L1020: `//[2.10.3.5]MW0LGE`
  - L1026: `//[2.10.3.5]MW0LGE`
  - L1029: `//[2.10.3.4]MW0LGE`
  - L1114: `//[2.10.3.4]MW0LGE run on/off`
  - L1881: `//MW0LGE`
  - L2302: `//[2.10.3.4]MW0LGE use OutCountTX if moxing`

### `mi0bot/Project Files/Source/Console/common.cs`

- **`MW0LGE`** (Richard Samphire): 11 markers
  - L86: `// extend contains to be able to ignore case etc MW0LGE`
  - L582: `// MW0LGE very simple logger`
  - L604: `// MW0LGE very simple logger`
  - L636: `// MW0LGE moved here from titlebar.cs, and used by console.cs and others`
  - L679: `//MW0LGE build version number string once and return that`
  - L937: `//MW0LGE [2.9.0.8]`
  - L1372: `//[2.10.3.9]MW0LGE performance related`
  - L1387: `//[2.10.3.9]MW0LGE cpu/memory details`
  - L1478: `//[2.10.3.9]MW0LGE form scaling`
  - L1533: `//[2.10.3.9]MW0LGE cpu usage for this process`
  - L1561: `//[2.10.3.9]MW0LGE screensave/powersave prevention`

### `mi0bot/Project Files/Source/Console/console.Designer.cs`

- **`G8NJJ`** (Laurence Barker): 1 marker
  - L474: `// G8NJJ`

### `mi0bot/Project Files/Source/Console/console.cs`

- **`DH1KLM`** (Sigi): 32 markers
  - L6766: `//DH1KLM`
  - L8326: `//DH1KLM`
  - L10069: `//DH1KLM`
  - L11055: `//DH1KLM`
  - L11086: `//DH1KLM`
  - L11232: `//DH1KLM`
  - L11262: `//DH1KLM`
  - L11740: `//        HardwareSpecific.Model != HPSDRModel.REDPITAYA) return; //DH1KLM`
  - L11970: `// DH1KLM`
  - L14903: `//DH1KLM`
  - L14929: `//DH1KLM`
  - L15505: `//DH1KLM`
  - L18809: `//DH1KLM`
  - L19402: `//DH1KLM`
  - L19577: `//DH1KLM`
  - L21122: `//DH1KLM`
  - L21148: `//DH1KLM`
  - L21677: `//DH1KLM`
  - L22666: `//DH1KLM`
  - L25180: `//DH1KLM`
  - *(+12 more)*
- **`DK1HLM`** (Unknown): 1 marker
  - L6849: `//DK1HLM`
- **`G7KLJ`** (Unknown): 3 markers
  - L728: `// PA init thread - from G7KLJ changes - done as early as possible`
  - L1217: `// this should not happen, ever !  // G7KLJ's idea/implementation`
  - L1221: `// G7KLJ's idea/implementation`
- **`G8NJJ`** (Laurence Barker): 52 markers
  - L144: `// G8NJJ`
  - L495: `// G8NJJ`
  - L496: `//G8NJJ added`
  - L497: `//G8NJJ added`
  - L498: `//G8NJJ added`
  - L499: `//G8NJJ added`
  - L500: `//G8NJJ added`
  - L501: `//G8NJJ added`
  - L507: `// G8NJJ: Titlebar strings and button/encoder/menu definitions for Andromeda`
  - L6764: `// G8NJJ`
  - L6765: `// G8NJJ`
  - L7389: `// G8NJJ like CATBandGroup but covering SWL too`
  - L7525: `// G8NJJ added to allow labelling of buttons in popup form`
  - L8598: `// ANAN-G2, G21K    (G8NJJ)`
  - L11684: `// added G8NJJ`
  - L11703: `// added G8NJJ`
  - L12935: `// added G8NJJ to allow scaling of VOX gain CAT command to Thetis range which is typ -80 to 0, not 0 to 1000`
  - L13413: `// G8NJJ: return the set of strings in the combo box`
  - L13432: `// G8NJJ: return the set of strings in the combo box`
  - L13438: `// added G8NJJ`
  - *(+32 more)*
- **`IK4JPN`** (Unknown): 2 markers
  - L26935: `// IK4JPN+ 9/11/2014`
  - L26962: `// IK4JPN-`
- **`K2UE`** (George Donadio): 2 markers
  - L26571: `// in following 'if', K2UE recommends not checking open antenna for the 8000 model`
  - L26650: `// K2UE idea:  try to determine if Hi-Z or Lo-Z load`
- **`MI0BOT`** (Reid Campbell): 92 markers
  - L124: `// updates the HL2 I/O board (MI0BOT)`
  - L2099: `// MI0BOT: Need an early indication of hardware type due to HL2 rx attenuator can be negative`
  - L2101: `// MI0BOT: Changes for HL2 only having a 16 step output attenuator`
  - L2110: `// MI0BOT: Changes for HL2 having a greater range of LNA`
  - L2121: `// MI0BOT: Remove items from main menu that are currently not used`
  - L6772: `// MI0BOT: HL2`
  - L8409: `// MI0BOT: HL2`
  - L8476: `// MI0BOT: HL2 can work at a high sample rate`
  - L8734: `// MI0BOT: Hermes Lite 2`
  - L10658: `// MI0BOT: Greater range for HL2`
  - L11041: `// MI0BOT: HL2 LNA has wider range`
  - L11072: `// MI0BOT: HL2 wider  LNA range`
  - L11218: `// MI0BOT: HL2 LNA has wider range`
  - L11248: `// MI0BOT: HL2 wider  LNA range`
  - L14846: `// MI0BOT: HL2`
  - L14916: `// MI0BOT: HL2`
  - L15027: `// MI0BOT:  Just return out to preserve the state of the rx antenna over reboots`
  - L15474: `// MI0BOT: HL2`
  - L15513: `// MI0BOT: HL2`
  - L16990: `// MI0BOT: Flag used to identify that CAT commands should operate on VFO B`
  - *(+72 more)*
- **`MW0LGE`** (Richard Samphire): 423 markers
  - L562: `//[2.10.3]MW0LGE`
  - L617: `//MW0LGE`
  - L862: `// initialise expandedSize so that we have something as a minimum to come back to from collapsed state //MW0LGE`
  - L898: `//[2.10.3.4]MW0LGE shutdown log remove`
  - L910: `//MW0LGE [2.9.0.8]`
  - L946: `//[2.10.1.0] MW0LGE initial call to setup check marks in status bar as a minimum`
  - L959: `//_frmFinder.WriteXmlFinderFile(AppDataPath); // note: this will only happen if not already there //[2.10.3.12]MW0LGE moved to shutdown`
  - L1040: `//[2.10.3.5]MW0LGE setup all status icon items`
  - L1063: `//MW0LGE now defaulted with m_tpDisplayThreadPriority, and updated by setupform`
  - L1205: `// MW0LGE used because some aspects of thetis test for null.`
  - L1211: `// MW0LGE implement SetupForm as singleton, with some level of thread safety (which is probably not needed)`
  - L1261: `//MW0LGE`
  - L1475: `//[2.10.3.12]MW0LGE command line adaptor select`
  - L1564: `// init the logger MW0LGE`
  - L1673: `//[2.10.3.6]MW0LGE changed to use invoke if needed as CATTCPIPserver uses this from another thread`
  - L2030: `//[2.10.3.1]MW0LGE make sure it is created on this thread, as the following serial`
  - L2143: `// MW0LGE certain things in setup need objects created in this instance, so we will`
  - L2160: `//[2.10.3.7]MW0LGE FM tx filter select, this was not being done at startup`
  - L2251: `//MW0LGE duped from above Display.Target = pnlDisplay;`
  - L2254: `//[2.10.3.13]MW0LGE moved to after the console window is showing`
  - *(+403 more)*
- **`W1CEG`** (Unknown): 2 markers
  - L37937: `// :W1CEG:`
  - L43304: `// W1CEG:  End`
- **`W2PA`** (Chris Codella): 45 markers
  - L52: `//-W2PA Necessary for Behringer MIDI changes`
  - L4998: `//-W2PA  The number of rig types in the imported DB matches the number in this version`
  - L5004: `//-W2PA  else the number has changed so don't import, leave the defaults alone`
  - L5012: `//-W2PA  The number of rig types in the imported DB matches the number in this version`
  - L5018: `//-W2PA  else the number has changed so don't import, leave the defaults alone`
  - L13051: `// QSK - a.k.a. full-break-in - Possible with Protocol-2 v1.7 or later  -W2PA`
  - L15845: `//-W2PA Added three new functions to make CAT functions match behavior of equivalent console functions.`
  - L16100: `//-W2PA This specifies the number of MIDI messages that cause a single tune step increment`
  - L18293: `//-W2PA`
  - L18298: `//-W2PA June 2017`
  - L26573: `//-W2PA Changed to allow 35w - some amplifier tuners need about 30w to reliably start working`
  - L29327: `//-W2PA Send LED update back to Behringer`
  - L29329: `//-W2PA Don't let the last LED go out until zero`
  - L29332: `//-W2PA Update LEDs on Behringer MIDI controller mini wheel`
  - L29415: `//-W2PA Update LEDs on Behringer MIDI controller`
  - L29543: `//-W2PA Update LEDs on Behringer MIDI controller`
  - L29545: `//-W2PA Don't let the last LED go out`
  - L31579: `//-W2PA Added to enable extended CAT control`
  - L32126: `// Lock the display //-W2PA Don't freeze display if we are zoomed in too far to fit the passband`
  - L32159: `//-W2PA If we tune beyond the display limits, re-center or scroll display, and keep going.  Original code above just stops tuning at edges.`
  - *(+25 more)*
- **`W4TME`** (Ke Chen): 3 markers
  - L14720: `//reset preset filter's center frequency - W4TME`
  - L14755: `//reset preset filter's center frequency - W4TME`
  - L15811: `// W4TME`

### `mi0bot/Project Files/Source/Console/cwx.cs`

- **`MW0LGE`** (Richard Samphire): 10 markers
  - L317: `//[2.10.3]MW0LGE swap`
  - L626: `////MW0LGE`
  - L816: `//[2.10.3.6]MW0LGE fixes #400`
  - L1731: `//[2.10.1.0] MW0LGE fixes #205`
  - L1751: `//MW0LGE moved here  from loadalpha`
  - L1768: `//[2.10.3.6]MW0LGE`
  - L1793: `//[2.10.3.6]MW0LGE fixes #400`
  - L2035: `//MW0LGE`
  - L2273: `//[2.10.3]MW0LGE swap`
  - L2547: `//[2.10.3.1]MW0LGE added to stop this form from being destroyed and the reference in console.cs being lost`
- **`W2PA`** (Chris Codella): 1 marker
  - L1663: `//W2PA Let F keys activate messages directly`

### `mi0bot/Project Files/Source/Console/database.cs`

- **`MW0LGE`** (Richard Samphire): 19 markers
  - L744: `//MW0LGE`
  - L3425: `//         // MW0LGE`
  - L9844: `//[2.10.1.0] MW0LGE added bSaveEmptyValues. All entries need to be in the database even if empty, because`
  - L9860: `//MW0LGE converted to Rows.Find because it is insanely faster, needs a primary key though`
  - L9886: `//[2.10.1.0] MW0LGE added bSaveEmptyValues. All entries need to be in the database even if empty, because`
  - L9910: `//MW0LGE converted to Rows.Find because it is insanely faster, needs a primary key though`
  - L10038: `//    // MW0LGE [2.9.0.8]`
  - L10336: `//                        //MW0LGE this db contains comboRadioModel, we need to pull over old radio selection from radio button implement...`
  - L10360: `//                    else if (thisKey.Contains("mnotchdb")) //[2.10.3]MW0LGE let defaul else import any`
  - L10443: `//                //[2.10.3]MW0LGE merge from old, any notches, fixes #236`
  - L10539: `//    _importedDS = manualImport;  // Prevents overwriting the new database file on next exit // [2.10.1.0] MW0LGE added flag manualImpor...`
  - L10555: `//[2.10.3.6]MW0LGE modified for new DB manager system`
  - L10594: `// MW0LGE [2.9.0.8]`
  - L10896: `//MW0LGE this db contains comboRadioModel, we need to pull over old radio selection from radio button implementation`
  - L10921: `//[2.10.3]MW0LGE let defaul else import any`
  - L10953: `// [2.10.3.6]MW0LGE changed, previously it would drop all meters from existing setup, and import`
  - L11024: `//[2.10.3]MW0LGE merge from old, any notches, fixes #236`
  - L11148: `//--MW0LGE`
  - L11583: `// MW0LGE added region1,2,3`
- **`W2PA`** (Chris Codella): 7 markers
  - L9469: `//-W2PA Write the database to a specific file`
  - L9475: `//-W2PA Write specific dataset to a file`
  - L9987: `////-W2PA New version of ImportDatabase to merge an old database or partly corruped one with a new default one`
  - L11165: `//-W2PA Basic validity checks of imported DataSet xml file`
  - L11198: `//-W2PA Expand an old TxProfile table into a newer one with more colunms. Fill in missing ones with default values.`
  - L11229: `//-W2PA Write a message to the ImportLog file during the import process`
  - L11236: `//-W2PA Original version of ImportDatabase`
- **`W4TME`** (Ke Chen): 2 markers
  - L5005: `// W4TME`
  - L5235: `// W4TME`

### `mi0bot/Project Files/Source/Console/display.cs`

- **`MW0LGE`** (Richard Samphire): 56 markers
  - L249: `// MW0LGE`
  - L1972: `// MW0LGE width 2`
  - L2038: `// MW0LGE width 2`
  - L2054: `//MW0LGE width 2`
  - L2135: `//MW0LGE`
  - L2272: `// MW0LGE width 2`
  - L2368: `// width 2 MW0LGE`
  - L2415: `//MW0LGE`
  - L3008: `//MW0LGE - these properties auto AGC on the waterfall, so that`
  - L3306: `//[2.10.3.12]MW0LGE adaptor info`
  - L3692: `//[2.10.1.0] MW0LGE spectrum/bitmaps may be cleared or bad, so wait to settle`
  - L4829: `//[2.10.1.0] MW0LGE fix issue #137`
  - L4830: `//[2.10.3.6]MW0LGE att_fix // change fixes #482`
  - L4972: `//if (grid_control) //[2.10.3.9]MW0LGE raw grid control option now just turns off the grid, all other elements are shown`
  - L5109: `//MW0LGE not used, as filling vertically with lines is faster than a filled very detailed`
  - L5824: `//            bool bElapsed = (_high_perf_timer.ElapsedMsec - _fLastFastAttackEnabledTimeRX1) > tmpDelay; //[2.10.1.0] MW0LGE change to t...`
  - L5859: `//            bool bElapsed = (_high_perf_timer.ElapsedMsec - _fLastFastAttackEnabledTimeRX2) > tmpDelay; //[2.10.1.0] MW0LGE change to t...`
  - L5868: `//[2.10.3.9]MW0LGE refactor to use refs, simplifies the code, removes unnecessary branching, general speed improvements`
  - L6700: `//MW0LGE [2.9.0.7]`
  - L6727: `//[2.10.3.9]MW0LGE changed from max`
  - *(+36 more)*

### `mi0bot/Project Files/Source/Console/dsp.cs`

- **`MW0LGE`** (Richard Samphire): 6 markers
  - L828: `// WDSP impulse cache - MW0LGE`
  - L881: `// MW0LGE [2.9.0.7] added pk + av + last`
  - L883: `// MW0LGE [2.9.0.7] added av`
  - L959: `// MW0LGE [2.9.0.7] not sure how these are real + imaginary values, they are ADC peak, and ADC average, according to rxa.c and rxa.h`
  - L960: `// input peak MW0LGE [2.9.0.7]`
  - L964: `// input average MW0LGE [2.9.0.7]`

### `mi0bot/Project Files/Source/Console/enums.cs`

- **`DH1KLM`** (Sigi): 1 marker
  - L129: `//DH1KLM`
- **`G8NJJ`** (Laurence Barker): 3 markers
  - L125: `//G8NJJ`
  - L126: `//G8NJJ`
  - L397: `// ANAN-G2: added G8NJJ`
- **`MI0BOT`** (Reid Campbell): 2 markers
  - L128: `//MI0BOT`
  - L396: `// MI0BOT`
- **`MW0LGE`** (Richard Samphire): 1 marker
  - L399: `// MW0LGE`

### `mi0bot/Project Files/Source/Console/eqform.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L203: `//MW0LGE [2.9.0.7]`

### `mi0bot/Project Files/Source/Console/frmMeterDisplay.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L188: `//IMPORTANT NOTE *****   //[2.10.3.7]MW0LGE`

### `mi0bot/Project Files/Source/Console/radio.cs`

- **`MW0LGE`** (Richard Samphire): 8 markers
  - L82: `//[2.10.3] MW0LGE`
  - L105: `//check for old wdspWisdom00 file - [2.10.3.9]MW0LGE`
  - L1090: `//[2.10.3.5]MW0LGE wave recorder volume normalise`
  - L1183: `// MW0LGE [2.9.0.8]`
  - L4243: `//MW0LGE return a notch that matches`
  - L4258: `//MW0LGE check if notch close by`
  - L4272: `//MW0LGE return list of notches in given bandwidth`
  - L4294: `//MW0LGE return first notch found that surrounds a given frequency in the given bandwidth`

### `mi0bot/Project Files/Source/Console/setup.cs`

- **`DH1KLM`** (Sigi): 18 markers
  - L847: `//DH1KLM`
  - L6316: `//DH1KLM`
  - L6318: `//DH1KLM`
  - L6377: `//DH1KLM`
  - L6382: `//DH1KLM`
  - L6474: `//DH1KLM`
  - L6521: `//DH1KLM`
  - L6559: `//DH1KLM`
  - L6592: `//DH1KLM`
  - L7269: `//DH1KLM`
  - L7359: `// MW0LGE [2.9.07] always initialise rx2 even if P1. Thanks to Reid (Gi8TME/Mi0BOT) and DH1KLM`
  - L15857: `//DH1KLM`
  - L16097: `//DH1KLM`
  - L16160: `//DH1KLM`
  - L20880: `//DH1KLM`
  - L20925: `//DH1KLM, not possible for Red Pitaya since ADC overflow pin not implement in Hard and Firmware`
  - L20926: `//DH1KLM, not possible for Red Pitaya since ADC overflow pin not implement in Hard and Firmware`
  - L24611: `//DH1KLM`
- **`G7KLJ`** (Unknown): 1 marker
  - L127: `//everything here moved to AfterConstructor, which is called during singleton instance // G7KLJ's idea/implementation`
- **`G8NJJ`** (Laurence Barker): 17 markers
  - L6093: `// added G8NJJ for Andromeda`
  - L6100: `// added G8NJJ for Aries`
  - L6107: `// added G8NJJ for Aries`
  - L6114: `// added G8NJJ for Ganymede`
  - L6121: `// added G8NJJ for Ganymede`
  - L6398: `// G8NJJ. will need more work ofr high power PA`
  - L8969: `// G8NJJ Saturn has QSK capability in any version.`
  - L9877: `// G8NJJ: all logic moved to the console properties code`
  - L9884: `// G8NJJ: all logic moved to the console properties code`
  - L9891: `// G8NJJ: all logic moved to the console properties code`
  - L9895: `// G8NJJ: setup control to select an Andromeda top bar when display is collapsed`
  - L9901: `// G8NJJ: all logic moved to the console properties code`
  - L9905: `// G8NJJ: setup control to select an Andromeda top bar when display is collapsed`
  - L9911: `// G8NJJ: all logic moved to the console properties code`
  - L16679: `// G8NJJ will need more work for ANAN_G2_1K (1KW PA)`
  - L20727: `// added G8NJJ`
  - L20778: `// added G8NJJ`
- **`MI0BOT`** (Reid Campbell): 48 markers
  - L1080: `// MI0BOT: Make sure the correct stuff is enabled`
  - L1082: `// MI0BOT: Changes for HL2 only having a 16 step output attenuator`
  - L1096: `// MI0BOT: For non HL2, minimum 0`
  - L2846: `// MI0BOT: HL2 option page now doesn't share ditter and random`
  - L4003: `//MI0BOT: HL2 has a greater range and can go negative`
  - L5307: `// MI0BOT: Now only has a -16.5 to 0 range in HL2 for Tune power`
  - L5463: `// MI0BOT: HL2`
  - L5484: `// MI0BOT: HL2`
  - L5504: `// MI0BOT: HL2`
  - L5524: `// MI0BOT: HL2`
  - L5544: `// MI0BOT: HL2`
  - L5564: `// MI0BOT: HL2`
  - L5584: `// MI0BOT: HL2`
  - L5604: `// MI0BOT: HL2`
  - L5624: `// MI0BOT: HL2`
  - L5644: `// MI0BOT: HL2`
  - L5664: `// MI0BOT: HL2`
  - L5684: `// MI0BOT: HL2`
  - L5704: `// MI0BOT: HL2`
  - L5724: `// MI0BOT: HL2`
  - *(+28 more)*
- **`MW0GLE`** (Richard Samphire): 1 marker
  - L2763: `//MW0GLE [2.10.3.6_dev4]`
- **`MW0LGE`** (Richard Samphire): 130 markers
  - L134: `//[2.10.3.9]MW0LGE atempt to get the model as soon as possile, before the getoptions, so that everything that relies on it at least has a...`
  - L170: `// MW0LGE note: this will allways cause the change event to fire, as the combobox does not contain any default value`
  - L176: `// MW0LGE gets shown/hidden by save/cancel/apply`
  - L418: `//MW0LGE [2.9.0.7] setup amp/volts calibration`
  - L576: `//MW0LGE [2.9.0.8]`
  - L642: `//MW0LGE [2.9.0.7]`
  - L646: `//MW0LGE [2.10.3.9]`
  - L950: `// MW0LGE in the case where we don't have a setting in the db, this function (initdisplaytab) is called, use console instead`
  - L953: `// MW0LGE`
  - L1050: `//MW0LGE`
  - L1051: `//MW0LGE`
  - L1052: `//MW0LGE`
  - L1053: `//MW0LGE`
  - L1054: `//MW0LGE`
  - L1055: `//MW0LGE`
  - L1056: `//MW0LGE`
  - L1119: `//MW0LGE [2.10.3.6]`
  - L1129: `//MW0LGE [2.9.0.7]`
  - L1462: `//[2.6.10.3]MW0LGE this had been removed, and was spotted after I diffed older version. I put it here to keep a record`
  - L1607: `//a.Add("chkRadioProtocolSelect_checkstate", chkRadioProtocolSelect.CheckState.ToString()); //[2.10.3.5]MW0LGE not used anymore`
  - *(+110 more)*
- **`W2PA`** (Chris Codella): 2 markers
  - L10348: `//-W2PA MIDI wheel as VFO sensitivity adjustments`
  - L12648: `//-W2PA Export a single TX Profile to send to someone else for importing.`
- **`W4WMT`** (Bryan Rambo): 1 marker
  - L29395: `//[2.10.3.5]W4WMT implements #87`

### `mi0bot/Project Files/Source/Console/splash.cs`

- **`MW0LGE`** (Richard Samphire): 3 markers
  - L60: `//MW0LGE`
  - L493: `//MW0LGE interesting, but removed`
  - L523: `//MW0LGE pnlStatus.Invalidate(m_rProgress);`

### `mi0bot/Project Files/Source/Console/titlebar.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L65: `//[2.10.2.2]MW0LGE use the auto generated class from pre build event for the BuildDate`

### `mi0bot/Project Files/Source/Console/ucInfoBar.cs`

- **`MW0LGE`** (Richard Samphire): 10 markers
  - L667: `//MW0LGE [2.9.0.7]`
  - L683: `//MW0LGE [2.9.0.7]`
  - L699: `//MW0LGE [2.9.0.7]`
  - L715: `//MW0LGE [2.9.0.7]`
  - L719: `// MW0LGE [2.9.0.7]`
  - L731: `//MW0LGE [2.9.0.7]`
  - L735: `// MW0LGE [2.9.0.7]`
  - L747: `//MW0LGE [2.9.0.7]`
  - L751: `// MW0LGE [2.9.0.7]`
  - L1189: `// return if any control is null, this should not happen  // MW0LGE [2.9.0.7]`

### `mi0bot/Project Files/Source/Console/ucMeter.cs`

- **`MW0LGE`** (Richard Samphire): 4 markers
  - L660: `//[2.10.3.4]MW0LGE added 'this' incase we are totally outside, fix issue where ui items get left visible`
  - L1059: `// && tmp.Length <= 21)  //[2.10.3.6_rc4] MW0LGE removed so that clients going forward can use older data as long as 13 entries exist`
  - L1188: `//[2.10.3.4]MW0LGE added 'this' incase we are totally outside, fix issue where ui items get left visible`
  - L1200: `//[2.10.3.6]MW0LGE no title or resize grabber, override by holding shift`

### `mi0bot/Project Files/Source/Console/wideband.cs`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L40: `// MW0LGE pause the display thread and return`

### `mi0bot/Project Files/Source/Console/xvtr.cs`

- **`MI0BOT`** (Reid Campbell): 1 marker
  - L6054: `// MI0BOT: Make Alt rx option available only for the HL2 with I/O Board`

### `mi0bot/Project Files/Source/Midi2Cat/Midi2Cat.Data/CatCmdDb.cs`

- **`DH1KLM`** (Sigi): 2 markers
  - L516: `// DH1KLM`
  - L518: `// DH1KLM`
- **`MI0BOT`** (Reid Campbell): 2 markers
  - L534: `//MI0BOT: Added ability to key via MIDI`
  - L536: `//MI0BOT: Added ability to PTT via MIDI`
- **`MW0LGE`** (Richard Samphire): 6 markers
  - L510: `// MW0LGE [2.9.0.7]`
  - L520: `// [2.10.3.6]MW0LGE`
  - L523: `// [2.10.3.12]MW0LGE`
  - L525: `// [2.10.3.12]MW0LGE`
  - L527: `// [2.10.3.12]MW0LGE`
  - L529: `// [2.10.3.12]MW0LGE`
- **`W2PA`** (Chris Codella): 3 markers
  - L95: `//-W2PA Renamed function to reflect actual meaning`
  - L380: `//-W2PA Added Wheel versions for Behrihger CMD PL-1 and others`
  - L532: `//-W2PA Added a toggle between A/B for main wheel`

### `mi0bot/Project Files/Source/Midi2Cat/Midi2Cat.Data/ControllerMapping.cs`

- **`W2PA`** (Chris Codella): 1 marker
  - L31: `//-W2PA Added device parameter for msg flow to MIDI device`

### `mi0bot/Project Files/Source/Midi2Cat/Midi2Cat.Data/Database.cs`

- **`W2PA`** (Chris Codella): 1 marker
  - L252: `//-W2PA To allow flowing commands back to MIDI devices`

### `mi0bot/Project Files/Source/Midi2Cat/Midi2Cat.IO/MidiDevice.cs`

- **`MW0LGE`** (Richard Samphire): 7 markers
  - L59: `//[2.10.3.4]MW0LGE`
  - L60: `//[2.10.3.4]MW0LGE`
  - L61: `//[2.10.3.4]MW0LGE`
  - L124: `//[2.10.3.5]MW0LGE`
  - L648: `//[2.10.3.4]MW0LGE added filter/mapper`
  - L667: `//[2.10.3.4]MW0LGE ignore LSB Controller message for 0-31 until we code up 14 bit support`
  - L805: `// undo controlID changes [2.10.3.4]MW0LGE`
- **`W2PA`** (Chris Codella): 12 markers
  - L62: `//-W2PA for switching Behringer PL-1 main wheel between VFO A and B.  Used in inDevice_ChannelMessageReceived() below and in MidiDeviceSe...`
  - L650: `//-W2PA Disambiguate messages from Behringer controllers`
  - L709: `//-W2PA Test for DeviceName is a Behringer type, and disambiguate the messages if necessary`
  - L713: `//-W2PA Trap Status E0 from Behringer PL-1 slider, change the ID to something that doesn't conflict with other controls`
  - L715: `//-W2PA Since PL-1 sliders send a variety of IDs, fix it at something unused: 73 (it uses 10 as its ID for LEDs)`
  - L718: `//-W2PA Trap PL-1 main wheel, change the ID to something that doesn't conflict with other controls, indicating VFO number (1=A, 2=B)`
  - L720: `//-W2PA This isn't the ID of any other control on the PL-1, so use for VFOB`
  - L725: `//-W2PA Trap Status E0 from Behringer CMD Micro controls, change the ID to something that doesn't conflict with buttons`
  - L893: `//-W2PA Set PL1 button light: 0=default, 1=alternate, 2=blink.   Used by Midi2CatCommands.`
  - L904: `//-W2PA Set PL1 button light: 0=default, 1=alternate, 2=blink, for PL1 button whose ID=inCtlID  Used by the UI console.`
  - L914: `//-W2PA Light LED number n for PL1 knob/wheel whose ID=inCtlID.  Used by the UI console.`
  - L921: `//-W2PA This is necessary for the PL-1 slider, which can cause a deluge of messages while also receiving them.`

### `mi0bot/Project Files/Source/Midi2Cat/Midi2Cat.IO/MidiDeviceSetup.cs`

- **`MW0LGE`** (Richard Samphire): 2 markers
  - L183: `//ControlId = FixBehringerCtlID(ControlId, Status); //-W2PA Disambiguate messages from Behringer controllers  //[2.10.3.5]MW0LGE lets not...`
  - L190: `//[2.10.3.5]MW0LGE so we can get the original control id`
- **`W2PA`** (Chris Codella): 6 markers
  - L251: `//private int FixBehringerCtlID(int ControlId, int Status) //-W2PA Test for DeviceName is a Behringer type, and disambiguate the messages...`
  - L255: `//        if (Status == 0xE0) //-W2PA Trap Status E0 from Behringer PL-1 slider, change the ID to something that doesn't conflict with ot...`
  - L257: `//            ControlId = 73;  //-W2PA I don't think this corresponds to the ID of any other control on the PL-1`
  - L260: `//        if (ControlId == 0x1F && MidiDevice.VFOSelect == 2) //-W2PA Trap PL-1 main wheel, change the ID to something that doesn't confl...`
  - L262: `//            ControlId = 77;  //-W2PA I don't think this corresponds to the ID of any other control on the PL-1, so use for VFOB`
  - L267: `//        if (Status == 0xB0) //-W2PA Trap Status E0 from Behringer CMD Micro controls, change the ID to something that doesn't conflict ...`

### `mi0bot/Project Files/Source/Midi2Cat/MidiMessageManager.cs`

- **`W2PA`** (Chris Codella): 32 markers
  - L102: `//-W2PA Used to get the index of the PL-1.  Assumes only one is connected.`
  - L118: `//-W2PA Send Pl-1 knob or slider LED update message (CMD Micro doesn't have such LEDs)`
  - L142: `//-W2PA Channel, Value, Status, ControlID, message bytes - but apparently it only pays attention to the string, others can all be zero`
  - L143: `//-W2PA CUE button`
  - L144: `//-W2PA play/pause button`
  - L145: `//-W2PA - button`
  - L146: `//-W2PA + button`
  - L147: `//-W2PA Rew button`
  - L148: `//-W2PA FF button`
  - L149: `//-W2PA SYNC button`
  - L150: `//-W2PA TAP button`
  - L151: `//-W2PA SCRATCH button`
  - L152: `//-W2PA 1 button`
  - L153: `//-W2PA 2 button`
  - L154: `//-W2PA 3 button`
  - L155: `//-W2PA 4 button`
  - L156: `//-W2PA 5 button`
  - L157: `//-W2PA 6 button`
  - L158: `//-W2PA 7 button`
  - L159: `//-W2PA 8 button`
  - *(+12 more)*

### `mi0bot/Project Files/Source/cmASIO/hostsample.cpp`

- **`MW0LGE`** (Richard Samphire): 3 markers
  - L327: `//[2.10.3.13]MW0LGE added explicit channel indices for input/output (0-based)`
  - L532: `//[2.10.3.13]MW0LGE pass explicit channel indices for input/output (0-based)`
  - L690: `//[2.10.3.13]MW0LGE get base channel numbers for input and output, and input mode`
- **`W4WMT`** (Bryan Rambo): 1 marker
  - L506: `//W4WMT`

### `mi0bot/Project Files/Source/cmASIO/version.cpp`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L13: `// MW0LGE version number now stored in Thetis->Versions.cs file, to keep shared`

### `mi0bot/Project Files/Source/wdsp/RXA.c`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L663: `// [2.10.3.13]MW0LGE carrier removal before AGC`

### `mi0bot/Project Files/Source/wdsp/analyzer.c`

- **`MW0LGE`** (Richard Samphire): 2 markers
  - L1111: `//[2.10.2]MW0LGE reset all the pixel and average buffers`
  - L1532: `// [2.10.3.13]MW0LGE`

### `mi0bot/Project Files/Source/wdsp/analyzer.h`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L74: `// [2.10.3.13]MW0LGE`

### `mi0bot/Project Files/Source/wdsp/eq.c`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L176: `// parametric eq with Q factor - Richard Samphire (c) 2026 - MW0LGE`

### `mi0bot/Project Files/Source/wdsp/rmatch.c`

- **`W4WMT`** (Bryan Rambo): 2 markers
  - L734: `// proportional feedback gain  ***W4WMT - reduce loop gain a bit for PowerSDR to help Primary buffers > 512`
  - L735: `// linearly interpolate cvar by sample  ***W4WMT - set varmode = 0 for PowerSDR (doesn't work otherwise!?!)`

### `mi0bot/Project Files/Source/wdsp/ssql.c`

- **`WU2O`** (Scott): 3 markers
  - L342: `// WU2O testing:  0.16 is a good default for 'threshold'; => 0.08 for 'wthresh'`
  - L352: `// WU2O testing:  0.1 is good default value`
  - L364: `// WU2O testing:  0.1 is good default value`

### `mi0bot/Project Files/lib/portaudio-19.7.0/include/portaudio.h`

- **`MW0LGE`** (Richard Samphire): 1 marker
  - L491: `//[2.10.3.11]MW0LGE portaudio note, new vals given to these, see commented code below`

### `mi0bot/Project Files/lib/portaudio-19.7.0/src/common/pa_converters.c`

- **`MW0LGE`** (Richard Samphire): 44 markers
  - L120: `//[2.10.3.11]MW0LGE portaudio`
  - L182: `//[2.10.3.11]MW0LGE portaudio`
  - L194: `//[2.10.3.11]MW0LGE portaudio`
  - L204: `//[2.10.3.11]MW0LGE portaudio`
  - L214: `//[2.10.3.11]MW0LGE portaudio`
  - L224: `//[2.10.3.11]MW0LGE portaudio`
  - L234: `//[2.10.3.11]MW0LGE portaudio`
  - L244: `//[2.10.3.11]MW0LGE portaudio`
  - L262: `//[2.10.3.11]MW0LGE portaudio start`
  - L272: `//[2.10.3.11]MW0LGE portaudio end`
  - L298: `//[2.10.3.11]MW0LGE portaudio`
  - L309: `//[2.10.3.11]MW0LGE portaudio`
  - L319: `//[2.10.3.11]MW0LGE portaudio`
  - L328: `//[2.10.3.11]MW0LGE portaudio`
  - L335: `//[2.10.3.11]MW0LGE portaudio`
  - L346: `//[2.10.3.11]MW0LGE portaudio`
  - L366: `//[2.10.3.11]MW0LGE portaudio`
  - L387: `//[2.10.3.11]MW0LGE portaudio`
  - L414: `//[2.10.3.11]MW0LGE portaudio`
  - L448: `//[2.10.3.11]MW0LGE portaudio`
  - *(+24 more)*

### `mi0bot/Project Files/lib/portaudio-19.7.0/src/common/pa_converters.h`

- **`MW0LGE`** (Richard Samphire): 9 markers
  - L139: `//[2.10.3]MW0LGE start`
  - L150: `//[2.10.3]MW0LGE end`
  - L176: `//[2.10.3]MW0LGE`
  - L187: `//[2.10.3]MW0LGE`
  - L197: `//[2.10.3]MW0LGE`
  - L206: `//[2.10.3]MW0LGE`
  - L213: `//[2.10.3]MW0LGE`
  - L224: `//[2.10.3]MW0LGE`
  - L257: `//[2.10.3]MW0LGE`

### `mi0bot/Project Files/lib/portaudio-19.7.0/src/common/pa_front.c`

- **`MW0LGE`** (Richard Samphire): 2 markers
  - L810: `//[2.10.3.11]MW0LGE portaudio`
  - L1817: `//[2.10.3.11]MW0LGE portaudio`

### `mi0bot/Project Files/lib/portaudio-19.7.0/src/hostapi/asio/pa_asio.cpp`

- **`MW0LGE`** (Richard Samphire): 2 markers
  - L1274: `//[2.10.3.11]MW0LGE portaudio W4WMT`
  - L1304: `//driver that cmASIO is using, if there is one //[2.10.3.11]MW0LGE portaudio W4WMT`

### `mi0bot/Project Files/lib/portaudio-19.7.0/src/hostapi/wasapi/pa_win_wasapi.c`

- **`MW0LGE`** (Richard Samphire): 3 markers
  - L993: `//[2.10.3.11]MW0LGE portaudio`
  - L2757: `//[2.10.3.11]MW0LGE portaudio`
  - L2784: `//[2.10.3.11]MW0LGE portaudio`

