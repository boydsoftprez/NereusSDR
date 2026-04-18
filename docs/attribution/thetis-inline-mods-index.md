# Thetis Inline Mod Index

Catalog of inline modification-attribution markers found inside
Thetis source file BODIES (not headers; headers were preserved
verbatim in Pass 5). Populated in Pass 6a of NereusSDR's GPL
compliance work. Consumed by Pass 6b to reconcile against NereusSDR
ported code.

Scope boundary: only the 30 Thetis sources cited in
`THETIS-PROVENANCE.md` were scanned. All line numbers refer to the
Thetis source file, not to any NereusSDR derivative. The scanner
skipped each file's leading comment block (everything above the
first real code line) to avoid double-counting header attributions
already preserved in Pass 5.

Callsigns tracked (case-insensitive): MW0LGE, W2PA, G8NJJ, N1GP,
W4WMT, W5WC, W5SD, WD5Y, M0YGG, MI0BOT, NR0V, VK6APH, KD5TFD.
Name-style attributions matching these callsigns (e.g. "Codella",
"Samphire", "Barker") are also captured and mapped to the
corresponding callsign.

## Summary

- Total Thetis sources scanned: 30
- Total inline markers found: 1458
- Distinct contributors attributed inline: 8

## Scope inference note

For each marker, the likely "scope" (the lines the marker attaches
to) is one of the following, in decreasing strength of signal:

1. A marker immediately preceding a function/method declaration
   attaches to that function body.
2. A marker at the end of a code line attaches to that single line.
3. A marker inside a function body attaches to the statement(s)
   that follow, up to the next blank line, marker, or closing brace.
4. A MW0LGE `[version]` marker typically attaches to a named
   contiguous block of 1-30 lines below it.

Pass 6b will refine these by reading the surrounding code. For
now, we emit each marker line with its 1-indexed line number so
Pass 6b can jump directly to the source.

## Per-file inline markers

### `Project Files/Source/ChannelMaster/cmaster.c`

Total lines in file: 594. Header block ends at
line 29 (first non-comment code line).
Inline markers found: **0**.

No body-level inline markers for any tracked callsign.

### `Project Files/Source/ChannelMaster/netInterface.c`

Total lines in file: 1690. Header block ends at
line 28 (first non-comment code line).
Inline markers found: **6**.

Per-callsign counts: G8NJJ: 1, MW0LGE: 6

- **L239** [MW0LGE]: `//NOTE: these 4 user get fuctions are named for P1 //MW0LGE_22b`
- **L269** [MW0LGE]: `//NOTE: these 5 user get functions are named for P2 //MW0LGE_22b`
- **L301** [G8NJJ, MW0LGE]: `	return (prn->user_dig_in & 0x10) != 0; //[2.10.3.5]MW0LGE changed to 0x10 from 0x16, spotted by G8NJJ`
- **L1101** [MW0LGE]: `//MW0LGE_22b`
- **L1178** [MW0LGE]: `		if (listenSock != INVALID_SOCKET) //[2.10.3.6]MW0LGE high priority always`
- **L1443** [MW0LGE]: `		prn->pll_locked = 0; //MW0LGE_21d`

### `Project Files/Source/ChannelMaster/network.c`

Total lines in file: 1494. Header block ends at
line 37 (first non-comment code line).
Inline markers found: **12**.

Per-callsign counts: MW0LGE: 12

- **L47** [MW0LGE]: `int WSA_inited = 0; //MW0LGE_22b moved here`
- **L54** [MW0LGE]: `		if(WSA_inited) WSACleanup(); // MW0LGE_22b`
- **L57** [MW0LGE]: `	//WSAinitialized = 1;//MW0LGE_22b`
- **L75** [MW0LGE]: `	if (WSA_inited) { // MW0LGE_22b`
- **L279** [MW0LGE]: `//	//MW0LGE_22b`
- **L310** [MW0LGE]: `//			if(WSA_inited) WSACleanup(); //MW0LGE_22b`
- **L325** [MW0LGE]: `//	sndbufsize = 0xfa000; // MW0LGE [2.9.0.8] from Warren, changed from 0x10000`
- **L693** [MW0LGE]: `					prn->pll_locked = prn->ReadBufp[0] & 0x10; //MW0LGE_21d`
- **L708** [MW0LGE]: `						prn->adc[i].adc_overload = prn->adc[i].adc_overload || (((prn->ReadBufp[1] >> i) & 0x1) != 0); // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`
- **L1028** [MW0LGE]: `	packetbuf[1400] = xvtr_enable | (!audioamp_enable) << 1 | atu_tune << 2; //MW0LGE_22b  // user_dig_in was gettin overwritten by 1025 packet read`
- **L1457** [MW0LGE]: `	//CloseHandle(prn->hWriteThreadMain); //[2.10.3.7]MW0LGE moved below, this only gets created with USB, crash if connected to a p2, then moved to a p1 etc`
- **L1458** [MW0LGE]: `	//CloseHandle(prn->hWriteThreadInitSem); //[2.10.3.7]MW0LGE moved below, this only gets created with USB`

### `Project Files/Source/ChannelMaster/network.h`

Total lines in file: 474. Header block ends at
line 43 (first non-comment code line).
Inline markers found: **3**.

Per-callsign counts: G8NJJ: 1, MI0BOT: 1, MW0LGE: 1

- **L390** [MW0LGE]: `int audioamp_enable; // constrol audio amp on ?? board //MW0LGE_22b`
- **L422** [MI0BOT]: `	HermesLite = 6,  // MI0BOT: HL2 allocated number`
- **L423** [G8NJJ]: `	Saturn = 10,     // ANAN-G2: added G8NJJ`

### `Project Files/Source/ChannelMaster/networkproto1.c`

Total lines in file: 749. Header block ends at
line 26 (first non-comment code line).
Inline markers found: **6**.

Per-callsign counts: MW0LGE: 6

- **L99** [MW0LGE]: `	prop = NULL; // MW0LGE_21g a change to ForceReset in setup.cs will cause calls to SendStopToMetis`
- **L292** [MW0LGE]: `		//MW0LGE_21g WSAWaitForMultipleEvents(1, &prn->hDataEvent, FALSE, WSA_INFINITE, FALSE);`
- **L335** [MW0LGE]: `								prn->adc[0].adc_overload = prn->adc[0].adc_overload || ControlBytesIn[1] & 0x01; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`
- **L353** [MW0LGE]: `								prn->adc[0].adc_overload = prn->adc[0].adc_overload || ControlBytesIn[1] & 1; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`
- **L354** [MW0LGE]: `								prn->adc[1].adc_overload = prn->adc[1].adc_overload || (ControlBytesIn[2] & 1) << 1; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`
- **L355** [MW0LGE]: `								prn->adc[2].adc_overload = prn->adc[2].adc_overload || (ControlBytesIn[3] & 1) << 2; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`

### `Project Files/Source/Console/DiversityForm.cs`

Total lines in file: 2937. Header block ends at
line 42 (first non-comment code line).
Inline markers found: **35**.

Per-callsign counts: G8NJJ: 6, MW0LGE: 29

- **L61** [MW0LGE]: `        //private Point p = new Point(200, 200); //MW0LGE_21c`
- **L62** [MW0LGE]: `        //private const double m_SCALEFACTOR = 10.0; //MW0LGE_21f this now applied to udR1 and udR2`
- **L72** [MW0LGE]: `        private double m_dGainMulti = 1; //MW0LGE_21f`
- **L170** [MW0LGE]: `            udR1.Maximum = udGainMulti.Maximum; //[2.10.3.6]MW0LGE these need to be high so that restore form can recover values`
- **L188** [MW0LGE]: `            //[2.10.3.6]MW0LGE implement memories. A bit of a hack to store all this in a text box, but it is easy with the saveform/restoreform`
- **L1531** [MW0LGE]: `            pp = getControlHandlePoint(); // MW0LGE_21c`
- **L1593** [MW0LGE]: `            //Point pp = getControlHandlePoint(); // MW0LGE_21c`
- **L1668** [MW0LGE]: `                    //p = PolarToXY(locked_r, -angle); //MW0LGE_21c`
- **L1678** [MW0LGE]: `                    //p = PolarToXY(r, -locked_angle); //MW0LGE_21c`
- **L1691** [MW0LGE]: `                    //p = PolarToXY(r, -angle); //MW0LGE_21c`
- **L1891** [MW0LGE]: `            //MW0LGE_21c`
- **L2080** [MW0LGE]: `                if (udR2.Value != (decimal)locked_r) udR2.Value = (decimal)Math.Round(locked_r * m_dGainMulti, 3); // MW0LGE_21b only assign if different`
- **L2083** [MW0LGE]: `            udR.Value = (decimal)Math.Round(udR2.Value / (decimal)m_dGainMulti, 3); //MW0LGE_21c;`
- **L2150** [MW0LGE]: `                if ((decimal)locked_r != udR1.Value) udR1.Value = (decimal)Math.Round(locked_r * m_dGainMulti, 3); // MW0LGE_21b only assign if different`
- **L2153** [MW0LGE]: `            udR.Value = (decimal)Math.Round(udR1.Value / (decimal)m_dGainMulti, 3); //MW0LGE_21c`
- **L2219** [G8NJJ]: `            get             // added 31/3/2018 G8NJJ to allow access by CAT commands`
- **L2230** [G8NJJ]: `        public int DiversityRXSource         // added 31/3/2018 G8NJJ to allow access by CAT commands`
- **L2253** [G8NJJ]: `        // added 6/8/2019 G8NJJ to allow access by Andromeda. Sets the appropriate gain.`
- **L2259** [MW0LGE]: `                    //udR2.Value = value; //MW0LGE_[2.9.0.7]`
- **L2262** [MW0LGE]: `                    //udR1.Value = value; //MW0LGE_[2.9.0.7]`
- **L2268** [MW0LGE]: `                    //return udR2.Value; //MW0LGE_[2.9.0.7]`
- **L2271** [MW0LGE]: `                    //return udR1.Value; //MW0LGE_[2.9.0.7]`
- **L2280** [MW0LGE]: `                bool bOldLock = chkLockR.Checked; //[2.10.3.5]MW0LGE fixes #324`
- **L2284** [MW0LGE]: `                v = Math.Max(v, udR1.Minimum); //MW0LGE_[2.9.0.7] not really needed as min is 0, belts/braces`
- **L2292** [G8NJJ]: `            get { return udR1.Value; }      // added 31/3/2018 G8NJJ to allow access by CAT commands`
- **L2299** [MW0LGE]: `                bool bOldLock = chkLockR.Checked; //[2.10.3.5]MW0LGE fixes #324`
- **L2303** [MW0LGE]: `                v = Math.Max(v, udR2.Minimum); //MW0LGE_[2.9.0.7] not really needed as min is 0, belts/braces`
- **L2311** [G8NJJ]: `            get { return udR2.Value; }      // added 31/3/2018 G8NJJ to allow access by CAT commands`
- **L2318** [MW0LGE]: `                bool bOldLockAngle = chkLockAngle.Checked; //[2.10.3.5]MW0LGE fixes #324`
- **L2326** [G8NJJ]: `            get { return udFineNull.Value; }        // added 31/3/2018 G8NJJ to allow access by CAT commands`
- **L2461** [MW0LGE]: `                if ((decimal)dTmpCalc != udFineNull.Value) // MW0LGE_21a only do if different`
- **L2544** [MW0LGE]: `                if (!console.IsSetupFormNull) console.SetupForm.UpdateDDCTab(); //[2.10.3.0]MW0LGE`
- **L2558** [MW0LGE]: `                if (!console.IsSetupFormNull) console.SetupForm.UpdateDDCTab(); //[2.10.3.0]MW0LGE`
- **L2572** [MW0LGE]: `                if (!console.IsSetupFormNull) console.SetupForm.UpdateDDCTab(); //[2.10.3.0]MW0LGE`
- **L2818** [MW0LGE]: `        //[2.10.3.5]MW0LGE old code, kept for reference`

### `Project Files/Source/Console/HPSDR/IoBoardHl2.cs [mi0bot]`

Total lines in file: 204. Header block ends at
line 23 (first non-comment code line).
Inline markers found: **0**.

No body-level inline markers for any tracked callsign.

### `Project Files/Source/Console/HPSDR/NetworkIO.cs`

Total lines in file: 1173. Header block ends at
line 1 (first non-comment code line).
Inline markers found: **10**.

Per-callsign counts: KD5TFD: 1, MW0LGE: 8, W5WC: 1

- **L160** [MW0LGE]: `                //[2.10.3.9]MW0LGE added board check, issue icon shown in setup`
- **L279** [KD5TFD]: `//* Copyright (C) 2006 Bill Tracey, KD5TFD, bill@ewjt.com `
- **L280** [W5WC]: `//* Copyright (C) 2010-2020  Doug Wigley`
- **L364** [MW0LGE]: `//        private const short VERSION = 0x202;//2;  //MW0LGE_22b`
- **L432** [MW0LGE]: `//                if (localEndPoint != null) //[2.10.3.7]MW0LGE null check added, and changed to tryparse`
- **L562** [MW0LGE]: `//            //[2.10.3.9]MW0LGE added board check, issue icon shown in setup`
- **L901** [MW0LGE]: `//                        //[2.10.3.5]MW0LGE sigh, MAC address in P1 is NOT at data[5], but at data[3]`
- **L1120** [MW0LGE]: `//                return null;  //[2.10.3.7]MW0LGE added try catch`
- **L1162** [MW0LGE]: `//        public byte betaVersion;                // byte[23] from P2 discovery packet. MW0LGE_21d`
- **L1163** [MW0LGE]: `//        public byte protocolSupported;          // byte[12] from P2 discovery packet. MW0LGE_21d `

### `Project Files/Source/Console/HPSDR/clsRadioDiscovery.cs [mi0bot]`

Total lines in file: 1460. Header block ends at
line 41 (first non-comment code line).
Inline markers found: **9**.

Per-callsign counts: MI0BOT: 9

- **L514** [MI0BOT]: `        public string IpAddressFixedHL2 { get; set; }   // MI0BOT: Extra info from discovery for HL2`
- **L518** [MI0BOT]: `        public byte EeConfigHL2{ get; set; }            // MI0BOT: Extra info from discovery for HL2`
- **L519** [MI0BOT]: `        public byte EeConfigReservedHL2{ get; set; }    // MI0BOT: Extra info from discovery for HL2`
- **L1065** [MI0BOT]: `                        info.IpAddressFixedHL2 = parsed.FixedIpHL2;                 // MI0BOT: Extra info from discovery for HL2`
- **L1117** [MI0BOT]: `            public string FixedIpHL2 { get; set; }          // MI0BOT: Extra info from discovery for HL2`
- **L1118** [MI0BOT]: `            public byte EeConfigHL2 { get; set; }           // MI0BOT: Extra info from discovery for HL2`
- **L1119** [MI0BOT]: `            public byte EeConfigReservedHL2 { get; set; }   // MI0BOT: Extra info from discovery for HL2`
- **L1169** [MI0BOT]: `                        byte[] fixedIp = new byte[4];                   // MI0BOT: Extra info from discovery for HL2`
- **L1239** [MI0BOT]: `            if (boardId == 6) return HPSDRHW.HermesLite;    // MI0BOT: HL2 added`

### `Project Files/Source/Console/HPSDR/specHPSDR.cs`

Total lines in file: 981. Header block ends at
line 20 (first non-comment code line).
Inline markers found: **1**.

Per-callsign counts: MW0LGE: 1

- **L570** [MW0LGE]: `                        //MW0LGE_21a`

### `Project Files/Source/Console/MeterManager.cs`

Total lines in file: 42584. Header block ends at
line 41 (first non-comment code line).
Inline markers found: **18**.

Per-callsign counts: MW0LGE: 18

- **L1800** [MW0LGE]: `                if (bOk && tmp.Length >= 44) //[2.10.3.6]MW0LGE added for dev_6`
- **L4200** [MW0LGE]: `                bool updateVFOASub = _console.VFOASubInUse; //[2.10.1.0] MW0LGE needed because at init rx2 might not be enabled, and the init function will have been given -999.999 from console.vfoasubfreq`
- **L5695** [MW0LGE]: `            initAllConsoleData(); //[2.10.3.6]MW0LGE get all console info here, as everything will be at the correct state`
- **L6433** [MW0LGE]: `                                    //a.Add("meterIGSettings_" + ig.Value.ID, igs.ToString()); //[2.10.3.6]MW0LGE not used`
- **L6563** [MW0LGE]: `                    f.Dispose();//[2.10.3.7]MW0LGE // we have to dispose it because close() prevent this being freed up`
- **L6791** [MW0LGE]: `                //[2.10.3.9]MW0LGE order these once, pointless doing it every time we get a percentage !`
- **L6846** [MW0LGE]: `            public int FadeValue //[2.10.1.0] MW0LGE used for on rx/tx fading`
- **L6851** [MW0LGE]: `            public bool Disabled //[2.10.1.0] MW0LGE used when certain features turned off such as eq,leveler,cfc`
- **L6856** [MW0LGE]: `            //[2.10.30.9]MW0LGE this perc cache code totally refactored, and only caches to 2 decimal precision for the dB value, and is keyed on the int version of that`
- **L9861** [MW0LGE]: `                    setTXAntenna(index - 6, _tx_band);//[2.10.3.9]MW0LGE fix, was using _rx1_band`
- **L18046** [MW0LGE]: `                        if (ok) n.frequency_hz = new_freq; //[2.10.3.9]MW0LGE update the data, prevents loads of updates`
- **L29467** [MW0LGE]: `                    return _rx == 1 && _rx2Enabled && (_multiRxEnabled || _split) && _vfoSub >= 0; //[2.10.3.6]MW0LGE added m.vfosub >= 0`
- **L31618** [MW0LGE]: `            // [2.10.1.0] MW0LGE`
- **L33264** [MW0LGE]: `                                //[2.10.3.5]MW0LGE note these are reverse RGB, we normally expect BGRA #289`
- **L33473** [MW0LGE]: `                                //[2.10.3.5]MW0LGE note these are reverse RGB, we normally expect BGRA #289`
- **L33686** [MW0LGE]: `                                //[2.10.3.5]MW0LGE note these are reverse RGB, we normally expect BGRA #289`
- **L39132** [MW0LGE]: `                //[2.10.3.9]MW0LGE refactor for speed`
- **L39319** [MW0LGE]: `                //[2.10.3.6]MW0LGE refactored to use Windows Imaging Component (WIC)`

### `Project Files/Source/Console/PSForm.cs`

Total lines in file: 1164. Header block ends at
line 42 (first non-comment code line).
Inline markers found: **22**.

Per-callsign counts: MW0LGE: 21, W2PA: 1

- **L72** [MW0LGE]: `            console = c;    // MW0LGE moved above restore, so that we actaully have console when control events fire because of restore form`
- **L74** [MW0LGE]: `            Common.RestoreForm(this, "PureSignal", false); // will also restore txtPSpeak //MW0LGE_21k9rc5`
- **L76** [MW0LGE]: `            _advancedON = chkAdvancedViewHidden.Checked; //MW0LGE_[2.9.0.6]`
- **L83** [MW0LGE]: `            startPSThread(); // MW0LGE_21k8 removed the winform timers, now using dedicated thread`
- **L157** [MW0LGE]: `        public ToolTip ToolTip //[2.10.3.9]MW0LGE used by finder`
- **L391** [MW0LGE]: `        public void SetupForm()//EventArgs e)  //MW0LGE_[2.9.0.7]`
- **L404** [MW0LGE]: `            setAdvancedView();  //MW0LGE_[2.9.0.7]`
- **L409** [MW0LGE]: `            //[2.10.3.4]]MW0LGE leave it there until thetis closes`
- **L417** [MW0LGE]: `            //_advancedON = true;//MW0LGE_[2.9.0.7]`
- **L418** [MW0LGE]: `            //btnPSAdvanced_Click(this, e); //MW0LGE_[2.9.0.7]`
- **L480** [W2PA]: `        //-W2PA Adds capability for CAT control via console`
- **L602** [MW0LGE]: `                if (lblPSInfoFB.BackColor.R > 0 || lblPSInfoFB.BackColor.G > 0 || lblPSInfoFB.BackColor.B > 0) //MW0LGE_21k8`
- **L614** [MW0LGE]: `            // MW0LGE_21k9`
- **L703** [MW0LGE]: `                    if(!_autocal_enabled) _autoON = false; // only want to turn this off if autocal is off MW0LGE_21k9rc4`
- **L738** [MW0LGE]: `                        if (!console.ATTOnTX) AutoAttenuate = true; //MW0LGE`
- **L754** [MW0LGE]: `                        _deltadB = (int)Math.Round(ddB, MidpointRounding.AwayFromZero); //[2.10.3.12]MW0LGE use rounding, to fix Banker's rounding issue`
- **L775** [MW0LGE]: `                        // give some additional time for the network msg to get to the radio before switching back on MW0LGE_21k9d5`
- **L802** [MW0LGE]: `            pbWarningSetPk.Visible = _PShwpeak != HardwareSpecific.PSDefaultPeak; //[2.10.3.7]MW0LGE show a warning if the setpk is different to what we expect for this hardware`
- **L815** [MW0LGE]: `            AutoAttenuate = chkPSAutoAttenuate.Checked; //MW0LGE use property`
- **L888** [MW0LGE]: `        private bool _advancedON = false; //MW0LGE_[2.9.0.7]`
- **L907** [MW0LGE]: `            this.TopMost = _topmost; //MW0LGE`
- **L1078** [MW0LGE]: `            //make copy of old, used in HasInfoChanged & CalibrationAttemptsChanged MW0LGE`

### `Project Files/Source/Console/clsDiscoveredRadioPicker.cs`

Total lines in file: 603. Header block ends at
line 41 (first non-comment code line).
Inline markers found: **0**.

No body-level inline markers for any tracked callsign.

### `Project Files/Source/Console/clsHardwareSpecific.cs`

Total lines in file: 791. Header block ends at
line 45 (first non-comment code line).
Inline markers found: **1**.

Per-callsign counts: G8NJJ: 1

- **L164** [G8NJJ]: `                    case HPSDRModel.ANAN_G2_1K:             // G8NJJ: likely to need further changes for PA`

### `Project Files/Source/Console/cmaster.cs`

Total lines in file: 2325. Header block ends at
line 42 (first non-comment code line).
Inline markers found: **10**.

Per-callsign counts: MW0LGE: 10

- **L664** [MW0LGE]: `                                    0, 0, 0, 0, 0, 2, 0, 2      // DDC0+DDC1, port 1035, Call 1 Sends TX_freq data to both RX //MW0LGE_21d DUP on top panadaptor (Warren provided info)`
- **L669** [MW0LGE]: `                                    0, 0, 0, 0, 0, 2, 0, 2      // DDC0+DDC1, port 1035, Call 1 Sends TX_freq data to both RX //MW0LGE_21d DUP on top panadaptor (Warren provided info), // MW0LGE [2.9.0.8] from Warren, change 3's to 2's`
- **L676** [MW0LGE]: `                                    LoadRouterAll((void*)0, 0, 1, /*1*/2, 8, pstreams, pfunction, pcallid); //MW0LGE_21d DUP on top panadaptor (Warren provided info)`
- **L999** [MW0LGE]: `            WaveThing.SetWavePlayerRun(id, run ? 1 : 0); //[2.10.3.4]MW0LGE`
- **L1011** [MW0LGE]: `                    if (run) WaveThing.wave_file_writer[0].RecordGain = (float)Audio.console.radio.GetDSPRX(0, 0).RXOutputGain; //[2.10.3.5]MW0LGE`
- **L1017** [MW0LGE]: `                    if (run) WaveThing.wave_file_writer[1].RecordGain = (float)Audio.console.radio.GetDSPRX(1, 0).RXOutputGain; //[2.10.3.5]MW0LGE`
- **L1020** [MW0LGE]: `            WaveThing.SetWaveRecorderRun(id, run ? 1 : 0); //[2.10.3.4]MW0LGE`
- **L1105** [MW0LGE]: `            Scope.SetScopeRun(id, run ? 1 : 0); //[2.10.3.4]MW0LGE run on/off`
- **L1872** [MW0LGE]: `            if (wideband[adc].WindowState == FormWindowState.Minimized) wideband[adc].WindowState = FormWindowState.Normal; //MW0LGE`
- **L2293** [MW0LGE]: `                        int size = Audio.MOX ? Audio.OutCountTX : Audio.OutCount; //[2.10.3.4]MW0LGE use OutCountTX if moxing`

### `Project Files/Source/Console/console.cs`

Total lines in file: 53877. Header block ends at
line 52 (first non-comment code line).
Inline markers found: **939**.

Per-callsign counts: G8NJJ: 53, MW0LGE: 836, W2PA: 46, WD5Y: 8

- **L52** [W2PA]: `using Midi2Cat.Data; //-W2PA Necessary for Behringer MIDI changes`
- **L123** [MW0LGE]: `        public bool _pause_DisplayThread = true;             // MW0LGE_21d initally paused`
- **L141** [G8NJJ]: `        // G8NJJ`
- **L310** [MW0LGE]: `        //MW0LGE_21k9rc6 new resize implementation`
- **L492** [G8NJJ]: `        // G8NJJ`
- **L493** [G8NJJ]: `        private Point chk_RIT_basis = new Point(100, 100);//G8NJJ added`
- **L494** [G8NJJ]: `        private Point chk_XIT_basis = new Point(100, 100);//G8NJJ added`
- **L495** [G8NJJ]: `        private Point ud_RIT_basis = new Point(100, 100);//G8NJJ added`
- **L496** [G8NJJ]: `        private Point ud_XIT_basis = new Point(100, 100);//G8NJJ added`
- **L497** [G8NJJ]: `        private Point btn_RITReset_basis = new Point(100, 100);//G8NJJ added`
- **L498** [G8NJJ]: `        private Point btn_XITReset_basis = new Point(100, 100);//G8NJJ added`
- **L504** [G8NJJ]: `        // G8NJJ: Titlebar strings and button/encoder/menu definitions for Andromeda`
- **L559** [MW0LGE]: `                    m_frmCWXForm.StopEverything(chkPower.Checked); //[2.10.3]MW0LGE`
- **L614** [MW0LGE]: `            //MW0LGE`
- **L755** [MW0LGE]: `            gmh.MouseMove += new MouseEventHandler(gmh_MouseMove); //MW0LGE_21d3`
- **L787** [MW0LGE]: `            // MW0LGE_21k9`
- **L866** [MW0LGE]: `            // initialise expandedSize so that we have something as a minimum to come back to from collapsed state //MW0LGE`
- **L902** [MW0LGE]: `            //[2.10.3.4]MW0LGE shutdown log remove`
- **L908** [MW0LGE]: `            this.Text = BasicTitleBar;//TitleBar.GetString(); //MW0LGE_21b`
- **L914** [MW0LGE]: `            //MW0LGE [2.9.0.8]`
- **L950** [MW0LGE]: `            CpuUsage(); //[2.10.1.0] MW0LGE initial call to setup check marks in status bar as a minimum`
- **L963** [MW0LGE]: `            //_frmFinder.WriteXmlFinderFile(AppDataPath); // note: this will only happen if not already there //[2.10.3.12]MW0LGE moved to shutdown`
- **L969** [MW0LGE]: `            Splash.SplashForm.Owner = this;						// So that main form will show/focus when splash disappears //MW0LGE_21d done in show above`
- **L991** [MW0LGE]: `            //added rx2 //MW0LGE_22b`
- **L995** [MW0LGE]: `            //MW0LGE_21d fix isuse where controls that had been`
- **L999** [MW0LGE]: `            //MW0LGE_21d BandStack2`
- **L1044** [MW0LGE]: `            //[2.10.3.5]MW0LGE setup all status icon items`
- **L1067** [MW0LGE]: `                    Priority = m_tpDisplayThreadPriority, //MW0LGE now defaulted with m_tpDisplayThreadPriority, and updated by setupform`
- **L1068** [MW0LGE]: `                    IsBackground = false//true MW0LGE_21b rundisplay now stops nicely, ensuring dx gpu resources are released                    `
- **L1209** [MW0LGE]: `            // MW0LGE used because some aspects of thetis test for null.`
- **L1215** [MW0LGE]: `            // MW0LGE implement SetupForm as singleton, with some level of thread safety (which is probably not needed)`
- **L1265** [MW0LGE]: `        //MW0LGE`
- **L1479** [MW0LGE]: `            //[2.10.3.12]MW0LGE command line adaptor select`
- **L1568** [MW0LGE]: `                Common.SetLogPath(app_data_path); // init the logger MW0LGE`
- **L1582** [MW0LGE]: `                // could not find exception for column not found //MW0LGE_21k9rc6`
- **L1672** [MW0LGE]: `        //MW0LGE_21g`
- **L1677** [MW0LGE]: `                //[2.10.3.6]MW0LGE changed to use invoke if needed as CATTCPIPserver uses this from another thread`
- **L1701** [MW0LGE]: `        //MW0LGE_21b`
- **L1724** [MW0LGE]: `        private bool m_bCTUNputsZeroOnMouse = false; //MW0LGE_21k9d`
- **L1782** [MW0LGE]: `            ResetLevelCalibration(true); // MW0LGE_[2.9.0.6] removed code below, call reset so that code is in one place`
- **L1978** [MW0LGE]: `            m_nTuneStepsByMode = new int[(int)DSPMode.LAST]; //MW0LGE_21j`
- **L1987** [MW0LGE]: `            //MW0LGE_22b fixes start state issue for RX2 meter`
- **L2000** [MW0LGE]: `            rx1_preamp_offset[(int)PreampMode.SA_MINUS20] = 20.0f; //MW0LGE_21d step atten`
- **L2007** [MW0LGE]: `            rx2_preamp_offset[(int)PreampMode.HPSDR_MINUS20] = 20.0f;  //MW0LGE_21d step atten`
- **L2009** [MW0LGE]: `            rx2_preamp_offset[(int)PreampMode.SA_MINUS10] = 10.0f;  //MW0LGE_21d SA stuff`
- **L2015** [MW0LGE]: `            //MW0LGE_21d Display.Target = pnlDisplay;`
- **L2020** [MW0LGE]: `            // MW0LGE_[2.9.0.7] setup the multi meter`
- **L2034** [MW0LGE]: `            //[2.10.3.1]MW0LGE make sure it is created on this thread, as the following serial`
- **L2061** [MW0LGE]: `            BuildTXProfileCombos(); // MW0LGE_21k9rc4b build them, so that GetState can apply the combobox text`
- **L2107** [MW0LGE]: `            //MW0LGE_21d BandStack2`
- **L2116** [MW0LGE]: `            // MW0LGE certain things in setup need objects created in this instance, so we will`
- **L2133** [MW0LGE]: `            //[2.10.3.7]MW0LGE FM tx filter select, this was not being done at startup`
- **L2214** [MW0LGE]: `            chkTUN.Enabled = false; // MW0LGE_21a`
- **L2215** [MW0LGE]: `            chk2TONE.Enabled = false; // MW0LGE_21a`
- **L2224** [MW0LGE]: `            //MW0LGE duped from above Display.Target = pnlDisplay;`
- **L2227** [MW0LGE]: `            //[2.10.3.13]MW0LGE moved to after the console window is showing`
- **L2241** [MW0LGE]: `            non_qsk_agc_hang_thresh = SetupForm.AGCRX1HangThreshold;//SetupForm.AGCHangThreshold; //MW0LGE_21k8`
- **L3350** [MW0LGE]: `        //            //[2.10.3.7]MW0LGE control names to always save, some were being missed if disabled`
- **L3443** [MW0LGE]: `        //            a.Remove("udTXStepAttData/" + udTXStepAttData.Value.ToString()); //[2.3.6.10]MW0LGE            `
- **L3445** [MW0LGE]: `        //            a.Add("last_radio_protocol/" + Audio.LastRadioProtocol.ToString()); // MW0LGE [2.9.0.8] used incase protocol changes from last time. Used in audio.cs tp reset PS feedback level`
- **L3453** [MW0LGE]: `        //            a.Add("chkSquelch_checkstate/" + chkSquelch.CheckState.ToString()); //MW0LGE [2.9.0.8]`
- **L3456** [MW0LGE]: `        //            a.Add("rx1_display_cal_offset/" + _rx1_display_cal_offset.ToString()); //[2.10.3.9]MW0LGE maintaining max precision`
- **L3459** [MW0LGE]: `        //            a.Add("rx2_display_cal_offset/" + _rx2_display_cal_offset.ToString());  //[2.10.3.9]MW0LGE maintaining max precision`
- **L3484** [MW0LGE]: `        //            a.Add("VFOASubFreq/" + m_dVFOASubFreq);  // MW0LGE_21a`
- **L3779** [MW0LGE]: `        //            //MW0LGE_21k9d`
- **L3809** [MW0LGE]: `        //                s += (rx_meter_cal_offset_by_radio[i]).ToString() + "|"; //[2.10.3.9]MW0LGE maintaining max precision`
- **L3815** [MW0LGE]: `        //                s += (rx_display_cal_offset_by_radio[i]).ToString() + "|"; //[2.10.3.9]MW0LGE maintaining max precision`
- **L3831** [MW0LGE]: `        //            //MW0LGE_21j`
- **L3839** [MW0LGE]: `        //            a.Add("infoBar_flip/" + infoBar.CurrentFlip.ToString()); //MW0LGE_21k9rc4 info bar currentflip`
- **L3840** [MW0LGE]: `        //            a.Add("infoBar_button1/" + ((int)infoBar.Button1Action).ToString()); //MW0LGE_[2.9.0.6] change to in, to prevent getstate issues if enaum names change`
- **L3842** [MW0LGE]: `        //            a.Add("infoBar_splitter_ratio/" + infoBar.SplitterRatio.ToString("f4")); //MW0LGE_21k9c changed format`
- **L3844** [MW0LGE]: `        //            a.Add("auto_start_forms/" + getAutoStartData()); //[2.10.3.6]MW0LGE`
- **L3851** [MW0LGE]: `        //            //MW0LGE_21d drop shadow`
- **L3862** [MW0LGE]: `        //            if (this.WindowState != FormWindowState.Minimized)//[2.10.3.6]MW0LGE prevent garbage being stored if shutdown when minimsed`
- **L3869** [MW0LGE]: `        //            a.Add("console_state/" + ((int)this.WindowState).ToString()); //MW0LGE_21 window state`
- **L3874** [MW0LGE]: `        //                if (SetupForm.WindowState != FormWindowState.Minimized)//[2.10.3.6]MW0LGE prevent garbage being stored if shutdown when minimsed`
- **L3918** [MW0LGE]: `            // returns if an update is needed - MW0LGE_21a`
- **L3923** [MW0LGE]: `            bool bNeedUpdate = false; // MW0LGE used to rebuild main form collapsed/expanded, done at the very end`
- **L3927** [MW0LGE]: `            //[2.10.2.3]MW0LGE change to dictionary as controls will be unique`
- **L3947** [MW0LGE]: `            // MW0LGE_21a`
- **L3951** [MW0LGE]: `            double dVFOAFreq = 7.1; //MW0LGE_21c`
- **L4013** [MW0LGE]: `                        if (list.Length != (int)DSPMode.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4034** [MW0LGE]: `                            bool selected = bool.Parse(val); //[2.10.3.6]MW0LGE`
- **L4040** [MW0LGE]: `                            bool selected = bool.Parse(val); //[2.10.3.6]MW0LGE`
- **L4046** [MW0LGE]: `                            bool selected = bool.Parse(val); //[2.10.3.6]MW0LGE`
- **L4191** [MW0LGE]: `                        dVFOAFreq = double.Parse(val); // MW0LGE_21c need to do this at end, as we used center_freq etc`
- **L4201** [MW0LGE]: `                        dVFOBFreq = double.Parse(val); // MW0LGE_21c need to do this at end, as we used center_freq etc`
- **L4203** [MW0LGE]: `                    case "VFOASubFreq": // MW0LGE_21a`
- **L4680** [MW0LGE]: `                    case "infoBar_flip": //MW0LGE_21k9rc4`
- **L4705** [MW0LGE]: `                        setAutoStartData(val); //[2.10.3.6]MW0LGE`
- **L4782** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4787** [MW0LGE]: `                    // MW0LGE_21k9d - store/recall ZTB`
- **L4790** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4796** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4802** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4808** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4814** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4820** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4826** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4832** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4860** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4868** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4876** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4883** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4890** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4897** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4906** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4915** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4924** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4933** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4940** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4947** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4954** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4961** [MW0LGE]: `                        if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE`
- **L4969** [MW0LGE]: `                        //[2.10.3.7]MW0LGE changed to <= from == so that if a new radio model is added, we will still use the data for the existing radios.`
- **L4971** [W2PA]: `                        if (numVals <= (int)HPSDRModel.LAST)  //-W2PA  The number of rig types in the imported DB matches the number in this version`
- **L4977** [W2PA]: `                        }  //-W2PA  else the number has changed so don't import, leave the defaults alone`
- **L4983** [MW0LGE]: `                        //[2.10.3.7]MW0LGE changed to <= from == so that if a new radio model is added, we will still use the data for the existing radios.`
- **L4985** [W2PA]: `                        if (numVals <= (int)HPSDRModel.LAST)  //-W2PA  The number of rig types in the imported DB matches the number in this version`
- **L4991** [W2PA]: `                        }  //-W2PA  else the number has changed so don't import, leave the defaults alone`
- **L5001** [MW0LGE]: `            // MW0LGE_21a`
- **L5081** [MW0LGE]: `            //[2.10.1.12]MW0LGE - apply CTUN state, and done above CentreFrequency assignment below`
- **L5108** [MW0LGE]: `            //MW0LGE_21c`
- **L5153** [MW0LGE]: `                //MW0LGE_21d restore window state`
- **L5634** [MW0LGE]: `            comboMeterTXMode.Items.Add("ALC Group"); //MW0LGE`
- **L5869** [MW0LGE]: `            //MW0LGE_21d`
- **L5888** [MW0LGE]: `            //[2.10.3.6]MW0LGE moved after mode`
- **L5900** [MW0LGE]: `            //MW0LGE_21c`
- **L5959** [MW0LGE]: `            //MW0LGE_21d3 setting the background of buttons, much like the mode buttons, but was totally forgotten here`
- **L5968** [MW0LGE]: `            //MW0LGE_21d`
- **L6118** [MW0LGE]: `            //MW0LGE_21j`
- **L6124** [MW0LGE]: `            //MW0LGE_21j`
- **L6191** [MW0LGE]: `            // MW0LGE lots of changes in this function for BandStack2`
- **L6457** [MW0LGE]: `            //[2.10.3.6]MW0LGE no band change on TX fix`
- **L6504** [MW0LGE]: `            //[2.10.3.6]MW0LGE no band change on TX fix`
- **L6507** [MW0LGE]: `            if (disable_split_on_bandchange && !bIngoreBandChange) //[2.10.3.6]MW0LGE might need to ignore this is we are using extended and band is moved to a hamband`
- **L6601** [MW0LGE]: `            //[2.10.3.13]MW0LGE does nothing atm`
- **L6737** [G8NJJ]: `                case HPSDRModel.ANAN_G2:                // G8NJJ`
- **L6738** [G8NJJ]: `                case HPSDRModel.ANAN_G2_1K:             // G8NJJ`
- **L6777** [MW0LGE]: `            //MW0LGE_21d filter outside band, ignore option`
- **L6815** [MW0LGE]: `            //MW0LGE_21f`
- **L7361** [G8NJJ]: `        // G8NJJ like CATBandGroup but covering SWL too`
- **L7497** [G8NJJ]: `        // G8NJJ added to allow labelling of buttons in popup form`
- **L7507** [MW0LGE]: `        private bool m_bLimitFiltersToSidebands = false;    //MW0LGE_21k9`
- **L7513** [MW0LGE]: `        private int m_nLowOutRX1;  // bit of a work around instead of having to add ref to UpdateRX1Filters //MW0LGE_21k9d`
- **L7550** [MW0LGE]: `                case DSPMode.SPEC: //MW0LGE_21k9`
- **L7560** [MW0LGE]: `                    int halfBw = (int)(radio.GetDSPRX(0, 0).RXFMDeviation + radio.GetDSPRX(0, 0).RXFMHighCut);  //[2.10.3.4]MW0LGE`
- **L7570** [MW0LGE]: `            //MW0LGE_21k9`
- **L7634** [MW0LGE]: `                FilterEdgesChangedHandlers?.Invoke(1, rx1_filter, RX1Band, low, high, rx1_filters[(int)_rx1_dsp_mode].GetName(rx1_filter), _max_filter_width, _max_filter_shift); //MW0LGE [2.9.0.7]`
- **L7672** [MW0LGE]: `                case DSPMode.SPEC: //MW0LGE_21k9`
- **L7682** [MW0LGE]: `                    int halfBw = (int)(radio.GetDSPRX(1, 0).RXFMDeviation + radio.GetDSPRX(1, 0).RXFMHighCut);  //[2.10.3.4]MW0LGE`
- **L7690** [MW0LGE]: `            //MW0LGE_21k9`
- **L7732** [MW0LGE]: `                FilterEdgesChangedHandlers?.Invoke(2, rx2_filter, RX2Band, low, high, rx2_filters[(int)_rx2_dsp_mode].GetName(rx2_filter), _max_filter_width, _max_filter_shift); //MW0LGE [2.9.0.7]`
- **L7848** [MW0LGE]: `            m_dVFOAFreq = freqFromString(freq); //MW0LGE `
- **L7859** [MW0LGE]: `            //MW0LGE [2.10.3.6_dev4]`
- **L7918** [MW0LGE]: `            string cmd = "F" + vfo + freq.ToString("f6").Replace(separator, "").PadLeft(11, '0') + ";"; //MW0LGE_22a`
- **L8078** [MW0LGE]: `                    int halfBw = (int)(radio.GetDSPTX(0).TXFMDeviation + radio.GetDSPTX(0).TXFMHighCut); //[2.10.3.4]MW0LGE`
- **L8238** [MW0LGE]: `                            if (p1) Rate[0] = rx1_rate; // [2.10.3.13]MW0LGE p1 !`
- **L8251** [MW0LGE]: `                            if (p1) Rate[0] = rx1_rate; // [2.10.3.13]MW0LGE p1 !`
- **L8536** [MW0LGE]: `            //MW0LGE_21e`
- **L8559** [G8NJJ]: `                    case HPSDRHW.Saturn:        // ANAN-G2, G21K    (G8NJJ)`
- **L8609** [MW0LGE]: `                                rx1 = 0;   //MW0LGE_22b missed out`
- **L9076** [MW0LGE]: `                //MW0LGE override... XVTR? TODO`
- **L9395** [MW0LGE]: `                    //MW0LGE override... XVTR? TODO`
- **L9532** [MW0LGE]: `                    //MW0LGE override... XVTR? TODO`
- **L9681** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L9687** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L9693** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L9699** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L9705** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L9711** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L9717** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L9723** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L9729** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L9735** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L9741** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L9747** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L9753** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L9759** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L9776** [MW0LGE]: `            //MW0LGE_[2.9.0.6] turn off min grid`
- **L9791** [MW0LGE]: `            // [2.10.3.9]MW0LGE the time needs to be calculated especially if CTUN is off`
- **L9860** [MW0LGE]: `            //MW0LGE_[2.9.0.6] turn off min grid`
- **L9871** [MW0LGE]: `            int iterations = 50;                            // number of samples to average //[2.10.3.9]MW0LGE changed from 20 to 50`
- **L9896** [MW0LGE]: `            bool step_attnRX2 = SetupForm.RX2EnableAtt; //MW0LGE_[2.9.0.6]`
- **L9898** [MW0LGE]: `            SetupForm.RX2EnableAtt = false; //MW0LGE_[2.9.0.6]`
- **L9902** [MW0LGE]: `            RX2PreampMode = PreampMode.HPSDR_ON;        //MW0LGE_[2.9.0.6]`
- **L9909** [MW0LGE]: `            float old_multimeter_cal_rx2 = _rx2_meter_cal_offset; //MW0LGE_[2.9.0.6]`
- **L9910** [MW0LGE]: `            float old_display_cal_rx2 = _rx2_display_cal_offset; //MW0LGE_[2.9.0.6]`
- **L9977** [MW0LGE]: `            //MW0LGE_[2.9.0.6]`
- **L10165** [MW0LGE]: `            float diffRX2 = level - (avg + _rx2_meter_cal_offset + rx2_preamp_offset[(int)rx2_preamp_mode]); // MW0LGE_[2.9.0.6]`
- **L10169** [MW0LGE]: `            rx_meter_cal_offset_by_radio[HardwareSpecific.ModelInt] = _rx1_meter_cal_offset;  // MW0LGE_[2.9.0.7] re-instated`
- **L10173** [MW0LGE]: `            diffRX2 = level - (avg2 + _rx2_display_cal_offset + rx2_preamp_offset[(int)rx2_preamp_mode]); // MW0LGE_[2.9.0.6]`
- **L10176** [MW0LGE]: `            RX2DisplayCalOffset += diffRX2; // MW0LGE_[2.9.0.6]`
- **L10193** [MW0LGE]: `                RX2MeterCalOffset = old_multimeter_cal_rx2; //MW0LGE_[2.9.0.6]`
- **L10204** [MW0LGE]: `            RX2PreampMode = preampRX2;					        	// restore preamp value MW0LGE_[2.9.0.6]`
- **L10206** [MW0LGE]: `            SetupForm.RX2EnableAtt = step_attnRX2;   //MW0LGE_[2.9.0.6] `
- **L10217** [MW0LGE]: `            GridMinFollowsNFRX1 = bOldMinGridFollowNF; //MW0LGE_[2.9.0.6]`
- **L10220** [MW0LGE]: `            if (ret_val) //MW0LGE_[2.9.0.6]`
- **L10269** [MW0LGE]: `            Band[] bands = { Band.B160M, Band.B80M, Band.B60M, Band.B40M, Band.B30M, Band.B20M, Band.B17M, Band.B15M, Band.B12M, Band.B10M, Band.B6M }; //MW0LGE_22b`
- **L10272** [MW0LGE]: `            //MW0LGE_22b`
- **L10324** [MW0LGE]: `                            //MW0LGE_22b`
- **L10608** [MW0LGE]: `                _tx_attenuator_data = validateTXStepAttData(value); //[2.10.3.9]MW0LGE validated`
- **L10612** [MW0LGE]: `                    setTXstepAttenuatorForBand(_tx_band, _tx_attenuator_data); //[2.10.3.6]MW0LGE att_fixes #399`
- **L10615** [MW0LGE]: `                        NetworkIO.SetTxAttenData(_tx_attenuator_data); //[2.10.3.6]MW0LGE att_fixes`
- **L10616** [MW0LGE]: `                        Display.TXAttenuatorOffset = _tx_attenuator_data; //[2.10.3.6]MW0LGE att_fixes`
- **L10627** [MW0LGE]: `                    //[2.10.3.9]MW0LGE This is not ideal, but a bodge to get the PedPitaya to TX attenuate correctly`
- **L10749** [MW0LGE]: `                //[2.10.3.13]MW0LGE moved from above, to ensure correct preamp/attn settings after diversity change`
- **L10756** [MW0LGE]: `                UpdateDiversityMenuItem(); //MW0LGE_22b`
- **L10760** [MW0LGE]: `        // MW0LGE_21d changed to set/getter so can bse used in event delgate system to track changes`
- **L10766** [MW0LGE]: `                return Math.Round(m_dCentreFrequency, 6); //MW0LGE_21k8`
- **L10784** [MW0LGE]: `                return Math.Round(m_dCentreRX2Frequency, 6); //MW0LGE_21k8`
- **L10852** [MW0LGE]: `                if (bOld != _click_tune_display) CTUNChangedHandlers?.Invoke(1, bOld, _click_tune_display, RX1Band); //MW0LGE_21d`
- **L10864** [MW0LGE]: `                if (bOld != _click_tune_rx2_display) CTUNChangedHandlers?.Invoke(2, bOld, _click_tune_rx2_display, RX2Band); //MW0LGE_21d`
- **L10917** [MW0LGE]: `        //[2.10.3.9]MW0LGE validate step attenuator values, so many HL2 issues with these being out of range`
- **L10947** [MW0LGE]: `                    udRX1StepAttData.Value = validateRX1StepAttData(getRX1stepAttenuatorForBand(rx1_band)); //MW0LGE [2.10.3.6] added //[2.10.3.9]MW0LGE validated`
- **L10979** [MW0LGE]: `                if (_from_attenuatordata[0]) return; //[2.10.3.12]MW0LGE prevent recalling this from inside here when the combotext changes, such basic 101`
- **L11004** [MW0LGE]: `                _rx1_attenuator_data = validateRX1StepAttData(_rx1_attenuator_data); //[2.10.3.9]MW0LGE validated`
- **L11006** [MW0LGE]: `                //MW0LGE_22b step atten`
- **L11050** [MW0LGE]: `                if (!_mox) //[2.10.3.9]MW0LGE note, this is not technically required for all radios except for the RedPitaya. See BODGE`
- **L11085** [MW0LGE]: `        public bool DiversityAttLink //[2.10.3.4]MW0LGE used by diversity form`
- **L11102** [MW0LGE]: `                        udRX2StepAttData.Value = validateRX2StepAttData(getRX2stepAttenuatorForBand(rx2_band)); //[2.10.3.9]MW0LGE validated`
- **L11144** [MW0LGE]: `                if (_from_attenuatordata[1]) return; //[2.10.3.12]MW0LGE prevent recalling this from inside here when the combotext changes, such basic 101`
- **L11165** [MW0LGE]: `                    udRX2StepAttData.Maximum = (decimal)61; //MW0LGE_[2.9.0.7]  changed to udRX2`
- **L11169** [MW0LGE]: `                rx2_attenuator_data = validateRX2StepAttData(rx2_attenuator_data); //[2.10.3.9]MW0LGE validated`
- **L11171** [MW0LGE]: `                //MW0LGE_22b step atten`
- **L11212** [MW0LGE]: `                if (!_mox || (_mox && VFOATX)) //[2.10.3.9]MW0LGE we should be able to do this if txing on rx1`
- **L11224** [MW0LGE]: `                        bool bRX1RX2diversity = m_bDiversityAttLinkForRX1andRX2 && (diversityForm != null && Diversity2 && diversityForm.EXTDIVOutput == 2); // if using diversity, and both rx's are linked, then we need to attenuate both //MW0LGE_[2.9.0.6]`
- **L11246** [MW0LGE]: `        private int[] m_nTuneStepsByMode; //MW0LGE_21j`
- **L11267** [MW0LGE]: `                // store the step index against mode. This is recovered on mode change event MW0LGE_21j`
- **L11611** [G8NJJ]: `        public bool CATDiversityRXRefSource             // added G8NJJ`
- **L11630** [G8NJJ]: `        public int CATDiversityRXSource             // added G8NJJ`
- **L11678** [MW0LGE]: `                //[2.10.3.13]MW0LGE modified to use existing function to ensure all is setup correctly`
- **L11727** [MW0LGE]: `                    // so that update to SetTXVAC occurs if needed MW0LGE_21k9d`
- **L12079** [MW0LGE]: `                if (oldValue != tx_xvtr_index) TransverterIndexChangedHandlers?.Invoke(oldValue, tx_xvtr_index); //MW0LGE_[2.9.0.7]`
- **L12561** [MW0LGE, WD5Y]: `                lblRX2ModeBigLabel.BackColor = value; //[2.10.1.0]MW0LGE ty WD5Y`
- **L12563** [MW0LGE]: `                //MW0LGE_21d`
- **L12569** [MW0LGE]: `                //MW0LGE_21f`
- **L12748** [MW0LGE, WD5Y]: `                lblRX2ModeBigLabel.ForeColor = value;//[2.10.1.0]MW0LGE ty WD5Y`
- **L12756** [MW0LGE]: `                //MW0LGE_21f`
- **L12761** [MW0LGE]: `                //MW0LGE_22b`
- **L12862** [G8NJJ]: `        // added G8NJJ to allow scaling of VOX gain CAT command to Thetis range which is typ -80 to 0, not 0 to 1000`
- **L12978** [W2PA]: `        public bool QSKEnabled  // QSK - a.k.a. full-break-in - Possible with Protocol-2 v1.7 or later  -W2PA`
- **L12995** [MW0LGE]: `                    non_qsk_agc_hang_thresh = SetupForm.AGCRX1HangThreshold;//SetupForm.AGCHangThreshold; //MW0LGE_21k8`
- **L13006** [W2PA]: `                    SetupForm.ATTOnTX = 31; // W2PA_21a                 `
- **L13026** [MW0LGE]: `            //[2.10.3.6]MW0LGE changed to one location as code was everywhere previously`
- **L13340** [G8NJJ]: `        // G8NJJ: return the set of strings in the combo box`
- **L13359** [G8NJJ]: `        // G8NJJ: return the set of strings in the combo box`
- **L13365** [G8NJJ]: `        // added G8NJJ`
- **L13514** [MW0LGE]: `        //MW0LGE_21k9`
- **L13524** [MW0LGE]: `            //MW0LGE_21h`
- **L13554** [MW0LGE]: `        //[2.10,3.5]MW0LGE the following is an insane implementation, has no one ever heard of arrays?`
- **L14817** [G8NJJ]: `                case HPSDRModel.ANAN_G2_1K:                          // G8NJJ: likely to need further changes for PA`
- **L14831** [MW0LGE]: `            RX2PreampPresent = _rx2_preamp_present; //[2.10.3.11]MW0LGE we were setting the member var above, but this was not actually having any effect/update`
- **L14877** [MW0LGE]: `            CurrentModelChangedHandlers?.Invoke(HardwareSpecific.OldModel, HardwareSpecific.Model); //MW0LGE_[2.9.0.7]`
- **L14910** [MW0LGE]: `                    int bits = Penny.getPenny().ExtCtrlEnable(lo_band, lo_bandb, _mox, value, _tuning, SetupForm.TestIMD, chkExternalPA.Checked); // MW0LGE_21j`
- **L15056** [MW0LGE]: `                        //_rx2_step_att_enabled = true; //[2.10.3.11]MW0LGE replaced with property`
- **L15296** [MW0LGE]: `                chk2TONE.Enabled = !_rx_only; // MW0LGE_21a`
- **L15327** [MW0LGE]: `                chk2TONE.Enabled = !_tx_inhibit; //MW0LGE_21a`
- **L15349** [MW0LGE]: `                //MW0LGE_21d bandstack initalising`
- **L15357** [MW0LGE]: `                    updateBandstackOverlay(1); //MW0LGE_21h`
- **L15442** [MW0LGE]: `            if (MOX)//[2.10.3.13]MW0LGE`
- **L15535** [MW0LGE]: `                if (RX2Enabled)//[2.10.3.7]MW0LGE added`
- **L15575** [MW0LGE]: `                //MW0LGE_21d bandstack initalising`
- **L15583** [MW0LGE]: `                    updateBandstackOverlay(1); //MW0LGE_21h`
- **L15588** [MW0LGE]: `        //MW0LGE`
- **L15595** [MW0LGE]: `                //MW0LGE override, and dont update any setupform/band settings`
- **L15607** [MW0LGE]: `                //MW0LGE override, and dont update any setupform/band settings`
- **L15706** [MW0LGE]: `                //[2.10.3.6]MW0LGE fix #417`
- **L15726** [MW0LGE]: `                //[2.10.3.6]MW0LGE fix #417`
- **L15752** [W2PA]: `        //-W2PA Added three new functions to make CAT functions match behavior of equivalent console functions.`
- **L16007** [W2PA]: `        //-W2PA This specifies the number of MIDI messages that cause a single tune step increment`
- **L16370** [MW0LGE]: `                //case "2m": next = "WWV"; previous = "6m"; break; //MW0LGE remove 2m and this //MW0LGE21_h`
- **L16373** [MW0LGE]: `                case "VHF0"/*"VU 2m"*/: next = "VHF1"; previous = "WWV"; break;  // remove these VU 2m/70cm MW0LGE_21h`
- **L16433** [MW0LGE]: `                //MW0LGE_21h`
- **L16462** [G8NJJ]: `                    chkRX2NB.CheckState = CheckState.Unchecked;             // edited 30/3/2018 G8NJJ to access the NB control not SNB`
- **L16464** [G8NJJ]: `                    chkRX2NB.CheckState = CheckState.Indeterminate;         // edited 30/3/2018 G8NJJ to access the NB control not SNB`
- **L16491** [MW0LGE]: `                //[2.10.3.6]MW0LGE fix #417`
- **L16505** [MW0LGE]: `        //[2.10.3.12]MW0LGE changed so that it is filter width from 10 to max filter width`
- **L16710** [MW0LGE]: `        public int CATSquelch2 //[2.10.3.5]MW0LGE change to an int, same as CATSquelch, why implement it differenly in the first place? boggles my mind`
- **L16712** [MW0LGE]: `            //[2.10.3.5]MW0LGE tri state for vsql`
- **L16916** [G8NJJ]: `                            //InitialiseAndromedaIndicators(true);           // initialise the panel LEDs   g8njj now done by ZZZS response`
- **L17243** [MW0LGE]: `        private int twotone_tune_power; //MW0LGE_22b`
- **L17272** [MW0LGE]: `                //[2.10.3.6]MW0LGE no band change on TX fix`
- **L17294** [MW0LGE]: `                    SetupForm.ATTOnTX = getTXstepAttenuatorForBand(_tx_band); //[2.10.3.6]MW0LGE att_fixes`
- **L17297** [MW0LGE]: `                    //[2.10.3.6]MW0LGE this tmp is needed because RX1AGCMode causes an update to the setup form`
- **L17306** [MW0LGE]: `                //MW0LGE_21b`
- **L17351** [MW0LGE]: `                case Band.VHF0: ret = "VHF0"; break; //"VU 2m"; break; //MW0LGE_21h get rid of these VU 2m and 70cms`
- **L17404** [MW0LGE]: `                case "VHF0"/*"VU 2m"*/: b = Band.VHF0; break;// remove these VU 2m/70cm MW0LGE_21h`
- **L17429** [MW0LGE]: `                //[2.10.3.6]MW0LGE no band change on TX fix`
- **L17435** [MW0LGE]: `                //MW0LGE`
- **L17446** [MW0LGE]: `                    lo_band = BandByFreq(XVTRForm.TranslateFreq(VFOBFreq), rx2_xvtr_index, current_region);//MW0LGE use rx2_xvtr_index`
- **L17447** [MW0LGE]: `                                                                                                           //MW0LGE this was changed in RX1Band but not here`
- **L17463** [MW0LGE]: `                    int tmp = rx2_agct_by_band[(int)rx2_band]; //[2.10.3.6]MW0LGE see comment in RX1Band`
- **L17469** [MW0LGE]: `                    //MW0LGE_21b`
- **L17488** [MW0LGE]: `                //[2.10.3.6]MW0LGE no band change on TX fix`
- **L17492** [MW0LGE]: `                if (initializing) old_band = value; // we cant use tx_band, because it is unset (GEN), unless we save it out it is irrelevant MW0LGE`
- **L17506** [MW0LGE]: `                                                                              // initialisting, becase it is irrelevent, old_band will = value at this point MW0LGE`
- **L17511** [MW0LGE]: `                    ptbTune.LimitValue = limitTunePower_by_band[(int)value]; //MW0LGE_22b`
- **L17514** [MW0LGE]: `                    TunePWR = tunePower_by_band[(int)value]; //MW0LGE_22b`
- **L17521** [MW0LGE]: `                    FMTXOffsetMHz = fm_tx_offset_by_band_mhz[(int)value]; //MW0LGE_21k9`
- **L17550** [MW0LGE]: `                if (_tx_band != old_band) TXBandChangeHandlers?.Invoke(old_band, _tx_band, TXFreq); //MW0LGE_22b`
- **L17693** [MW0LGE]: `                //[2.10.3.6]MW0LGE no mode change on TX fix`
- **L17753** [MW0LGE]: `                //[2.10.3.6]MW0LGE no mode change on TX fix`
- **L18191** [W2PA]: `                if (cw_pitch <= 0) cw_pitch = 0;  //-W2PA`
- **L18196** [W2PA]: `                //-W2PA June 2017`
- **L18300** [MW0LGE]: `                return Math.Round(m_dVFOAFreq, 6);  // MW0LGE_21d so many lost focus calls fixed because of this, rounded to 6`
- **L18304** [MW0LGE]: `                if ((!_force_vfo_update && _vfoA_lock) || IsSetupFormNull) return; //[2.10.3.5]MW0LGE removed the state check //[2.10.3.7]MW0LGE always process if initialising`
- **L18320** [MW0LGE]: `            m_dVFOAFreq = Math.Round(freq, 6); // MW0LGE_21d rounded to 6`
- **L18326** [MW0LGE]: `            m_dVFOBFreq = Math.Round(freq, 6); // MW0LGE_21d rounded to 6`
- **L18334** [MW0LGE]: `            m_dVFOASubFreq = Math.Round(freq, 6);// MW0LGE_21d rounded to 6`
- **L18338** [MW0LGE]: `            //MW0LGE [2.9.0.7] also in UpdateVFOASub`
- **L18358** [MW0LGE]: `                if (!VFOASubInUse) return -999.999; // [2.10.1.0] MW0LGE changed as MeterManager makes use of this`
- **L18359** [MW0LGE]: `                return Math.Round(m_dVFOASubFreq, 6); // MW0LGE_21d rounded to 6`
- **L18364** [MW0LGE]: `                if ((!_force_vfo_update && _vfoA_lock) || IsSetupFormNull) return; //[2.10.3.6]MW0LGE removed the state check //[2.10.3.7]MW0LGE always process if initialising`
- **L18382** [MW0LGE]: `                return Math.Round(m_dVFOBFreq, 6); // MW0LGE_21d rounded to 6`
- **L18386** [MW0LGE]: `                if ((!_force_vfo_update && _vfoB_lock) || IsSetupFormNull) return; //[2.10.3.5]MW0LGE removed state check //[2.10.3.7]MW0LGE always process if initialising`
- **L18420** [MW0LGE]: `        public int TunePWR  //MW0LGE_22b`
- **L19056** [MW0LGE]: `                        NetworkIO.SetTxAttenData(txatt); //[2.10.3.6]MW0LGE att_fixes`
- **L19057** [MW0LGE]: `                        Display.TXAttenuatorOffset = txatt; //[2.10.3.6]MW0LGE att_fixes`
- **L19092** [MW0LGE]: `                if (oldValue != apollopresent) ApolloPresentChangedHandlers?.Invoke(oldValue, apollopresent); //MW0LGE_[2.9.0.7]`
- **L19147** [MW0LGE]: `                if (oldValue != alexpresent) AlexPresentChangedHandlers?.Invoke(oldValue, alexpresent); //MW0LGE_[2.9.0.7]`
- **L19181** [MW0LGE]: `                if (_from_preampmode[0]) return; //[2.10.3.12]MW0LGE prevent recalling this from inside here when the combotext changes, such basic 101`
- **L19257** [MW0LGE]: `                //MW0LGE_22b`
- **L19337** [MW0LGE]: `                        comboPreamp.Text = "-40db"; //MW0LGE_22b lower`
- **L19341** [MW0LGE]: `                        comboPreamp.Text = "-50db"; //MW0LGE_22b lower`
- **L19375** [MW0LGE]: `                UpdateRX1DisplayOffsets(); //MW0LGE_22b`
- **L19389** [MW0LGE]: `                if (_from_preampmode[1]) return; //[2.10.3.12]MW0LGE prevent recalling this from inside here when the combotext changes, such basic 101`
- **L19402** [MW0LGE]: `                //MW0LGE_22b`
- **L19419** [MW0LGE]: `                        comboRX2Preamp.Text = "-10db"; //MW0LGE_22b lower`
- **L19424** [MW0LGE]: `                        comboRX2Preamp.Text = "-20db";  //MW0LGE_22b lower`
- **L19429** [MW0LGE]: `                        comboRX2Preamp.Text = "-30db";  //MW0LGE_22b lower`
- **L19449** [MW0LGE]: `                //MW0LGE_22b`
- **L19456** [MW0LGE]: `                if (!_rx2_step_att_enabled && (HardwareSpecific.Model == HPSDRModel.ANAN100D ||  //MW0LGE_22b we dont want to do this if we are using SA`
- **L19503** [MW0LGE]: `                // [2.9.3.5]MW0LGE reverted back`
- **L19508** [MW0LGE]: `                // [2.9.3.5]MW0LGE reverted back to -160 to 0`
- **L19520** [MW0LGE]: `                // [2.9.3.5]MW0LGE reverted back to -160 to 0`
- **L19525** [MW0LGE]: `                // [2.9.3.5]MW0LGE reverted back to -160 to 0`
- **L19711** [MW0LGE]: `                DSPMode mode = RX2Enabled && VFOBTX ? _rx2_dsp_mode : _rx1_dsp_mode; //[2.10.3.7]MW0LGE use the correct mode, age old bug from before 27/4/2019`
- **L19727** [MW0LGE]: `                DSPMode mode = RX2Enabled && VFOBTX ? _rx2_dsp_mode : _rx1_dsp_mode; //[2.10.3.7]MW0LGE use the correct mode, age old bug from before 27/4/2019`
- **L19875** [MW0LGE]: `        private int sample_rate_rx1 = 0; //[2.10.2.3]MW0LGE change to 0 so that comboAudioSampleRate1_SelectedIndexChanged will do its thing is system is shutdown with 48000 selected`
- **L19916** [MW0LGE]: `        private int sample_rate_rx2 = 0;//[2.10.2.3]MW0LGE change to 0 so that comboAudioSampleRate1_SelectedIndexChanged will do its thing is system is shutdown with 48000 selected`
- **L19964** [MW0LGE]: `                Audio.OutRateTX = value; //[2.10.3.4]MW0LGE added`
- **L19965** [MW0LGE]: `                Audio.BlockSizeTX = cmaster.GetBuffSize(value); //[2.10.3.4]MW0LGE added`
- **L19967** [MW0LGE]: `                if (!IsSetupFormNull)  //[2.10.3.5]MW0LGE added`
- **L20039** [MW0LGE]: `        //[2.10.3.4]MW0LGE added for completeness`
- **L20161** [MW0LGE]: `                Display.CurrentFPS = _display_fps; //MW0LGE_21k8 pre init`
- **L20163** [MW0LGE]: `                int wdspFps = (int)Math.Max(1, _display_fps * 1.1f); //[2.10.3]MW0LGE add in 10% extra so frames are more often avaialble for use in RunDisplay()`
- **L20641** [MW0LGE]: `                if (oldValue != pa_present) PAPresentChangedHandlers?.Invoke(oldValue, pa_present); //MW0LGE_[2.9.0.7]`
- **L20684** [MW0LGE]: `                    _total_cpu_usage.Dispose(); //MW0LGE_21k8`
- **L20865** [MW0LGE]: `                if (value < (double)udFMOffset.Minimum || value > (double)udFMOffset.Maximum) return; //MW0LGE_21k9`
- **L21050** [MW0LGE]: `        // Added 6/11/05 BT to support CAT //[2.10.3.11]MW0LGE included setter`
- **L21062** [MW0LGE]: `        //Added 7/11/2010 BT to support CAT //[2.10.3.11]MW0LGE included setter`
- **L21690** [MW0LGE]: `                            if (!adcs_linked && har != null && _auto_att_undo_rx2) //[2.10.3.9]MW0LGE ignore if adcs linked, as will be maintained by rx1 data`
- **L21769** [MW0LGE]: `                //MW0LGE`
- **L21833** [MW0LGE]: `                if (_bInfoBarShowSEQErrors) infoBar.Warning("Sequence error : " + ooo.ToString() + " (" + s.Trim() + ")"); //MW0LGE_21k9c show/hide flag`
- **L21859** [MW0LGE]: `            float y = Display.MaxY; // MW0LGE_21a this is already in dBm, not pixels`
- **L21863** [MW0LGE]: `            //mostly copied from vfoa lost focus MW0LGE_21a`
- **L21908** [MW0LGE]: `                    if ((_click_tune_display && !_mox) || (_click_tune_display && _display_duplex))    // Correct Right hand peak frequency when CTUN on -G3OQD // MW0LGE_21a also when in CTD and DUP`
- **L22022** [MW0LGE]: `            //[2.10.1.0] MW0LGE re-implemented`
- **L22130** [MW0LGE]: `            //[2.10.1.0] MW0LGE re-implemented`
- **L22144** [MW0LGE]: `            //[2.10.1.0] MW0LGE re-implemented`
- **L22273** [MW0LGE]: `            //MW0LGE `
- **L22283** [MW0LGE]: `                bAboveS9Frequency = (VFOAFreq >= S9Frequency); //MW0LGE_21a`
- **L22288** [MW0LGE]: `                bAboveS9Frequency = (VFOBFreq >= S9Frequency); //MW0LGE_21a`
- **L22448** [MW0LGE]: `                        //MW0LGE combined ALC + ALCcomp display`
- **L23383** [MW0LGE]: `            if (m_objRX1HistoryDelayTimer.ElapsedMsec < Math.Max(dly, 2000)) //MW0LGE_21a`
- **L23400** [MW0LGE]: `            if (m_objRX2HistoryDelayTimer.ElapsedMsec < Math.Max(dly, 2000)) //MW0LGE_21a`
- **L23539** [MW0LGE]: `                        //MW0LGE to do, just draw lines atm`
- **L23591** [MW0LGE]: `                    //MW0LGE moved all code into common function, used by both edge and original meter`
- **L23798** [MW0LGE]: `                        // MW0LGE reworked size/heights`
- **L23847** [MW0LGE]: `                        //MW0LGE moved all code into common function, used by both edge and original meter`
- **L23966** [MW0LGE]: `            rx2_avg_num = Display.CLEAR_FLAG; // MW0LGE_21a`
- **L24192** [MW0LGE]: `                    //MW0LGE_21g`
- **L24298** [MW0LGE]: `                                    case DisplayMode.SCOPE:  //[2.10.3.4]MW0LGE not used anymore since scope was coded in cmaster.cs`
- **L24313** [MW0LGE]: `                                        if (Audio.phase_buf_l != null && Audio.phase_buf_r != null) // MW0LGE would be null if audio not running (ie not connected?)`
- **L24385** [MW0LGE]: `                                    case DisplayMode.PANAFALL:  // MW0LGE`
- **L24406** [MW0LGE]: `                                    case DisplayMode.SCOPE:  //[2.10.3.4]MW0LGE not used anymore since scope was coded in cmaster.cs`
- **L24421** [MW0LGE]: `                                        if (Audio.phase_buf_l != null && Audio.phase_buf_r != null) // MW0LGE would be null if audio not running (ie not connected?)`
- **L24466** [MW0LGE]: `                    //MW0LGE consider how long all the above took (reset at start of loop), and remove any inaccuarcy from Thread.Sleep`
- **L24617** [MW0LGE]: `                            case MeterTXMode.ALC_GROUP: //MW0LGE ALC_GROUP is the sum of ALC and ALC_G`
- **L24694** [MW0LGE]: `                    //MW0LGE_21d step atten`
- **L24759** [MW0LGE]: `            // MW0LGE_21k9c`
- **L24762** [MW0LGE]: `            // MW0LGE [2.9.0.7] changed volts to 150`
- **L24763** [G8NJJ]: `            //G8NJJ need similar code for Saturn here, but rates from Ssaturn will be different`
- **L24796** [MW0LGE]: `                // [2.10.1.0]MW0LGE log data to VALog.txt`
- **L24998** [MW0LGE]: `            if (MeterManager.RequiresUpdate(1, Reading.REV_VOLT)) _RX1MeterValues[Reading.REV_VOLT] = volts; //MW0LGE_[2.9.0.7]`
- **L25064** [MW0LGE]: `            if (MeterManager.RequiresUpdate(1, Reading.FWD_VOLT)) _RX1MeterValues[Reading.FWD_VOLT] = volts; //MW0LGE_[2.9.0.7]`
- **L25080** [MW0LGE]: `            if (MeterManager.RequiresUpdate(1, Reading.FWD_ADC)) _RX1MeterValues[Reading.FWD_ADC] = (float)power_int; //MW0LGE_[2.9.0.7]`
- **L25139** [MW0LGE]: `            if (MeterManager.RequiresUpdate(1, Reading.FWD_ADC)) _RX1MeterValues[Reading.FWD_ADC] = (float)power_int; //MW0LGE_[2.9.0.7]`
- **L25198** [MW0LGE]: `            if (MeterManager.RequiresUpdate(1, Reading.FWD_ADC)) _RX1MeterValues[Reading.FWD_ADC] = (float)power_int; //MW0LGE_[2.9.0.7]`
- **L25303** [MW0LGE]: `            if (MeterManager.RequiresUpdate(1, Reading.FWD_ADC)) _RX1MeterValues[Reading.FWD_ADC] = (float)power_int; //MW0LGE_[2.9.0.7]`
- **L25427** [MW0LGE]: `                    bool cw_ptt = CWInput.KeyerPTT && _current_breakin_mode == BreakIn.Semi; // CW serial PTT  //[2.10.3.9]MW0LGE only want to do this on semi breakin`
- **L25579** [MW0LGE]: `        //[2.10.3.12]MW0LGE changed from ints to bools`
- **L25633** [MW0LGE]: `                _old_cw_auto_mode = DSPMode.FIRST; //[2.10.3.12]MW0LGE fix issue where if you key when in cw it would return to old non cw mode`
- **L25655** [MW0LGE]: `            //[2.10.1.0]MW0LGE implements #70`
- **L25656** [MW0LGE]: `            //[2.10.3.12]MW0LGE modified to be called whenever dot/dash is present`
- **L25767** [MW0LGE]: `                        SetupForm.ATTOnRX2 = old_rx2_satt_data; //MW0LGE_21d atten`
- **L25792** [MW0LGE]: `                        old_rx2_satt_data = SetupForm.ATTOnRX2;// MW0LGE_21d atten          rx2_attenuator_data;// RX2AttenuatorData;`
- **L25806** [MW0LGE]: `                //MW0LGE_22b converted to protocol, so we use correctly named userI functions`
- **L25863** [MW0LGE]: `                catch // MW0LGE_21d`
- **L25917** [MW0LGE]: `                //MW0LGE_21d`
- **L25980** [MW0LGE]: `                    //[2.10.3.6]MW0LGE modifications to use setup config for swr and tune ignore power. Implements #221 (https://github.com/ramdor/Thetis/issues/221)`
- **L25987** [W2PA]: `                        //-W2PA Changed to allow 35w - some amplifier tuners need about 30w to reliably start working`
- **L25989** [MW0LGE]: `                        if (!chkTUN.Checked && (swrprotection && alex_fwd > 10.0f && (alex_fwd - alex_rev) < 1.0f //[2.10.3.6]MW0LGE ignored if tuning, and returned the 10.0f`
- **L26167** [MW0LGE]: `                computeMKIIPAVoltsAmps(); //MW0LGE_21k9c`
- **L26172** [MW0LGE]: `                //MW0LGE [2.9.0.7] added to prevent edge case flicker due to rounding`
- **L26266** [MW0LGE]: `                    btnHidden.Focus(); // only do this if vfo boxes dont have focus //MW0LGE`
- **L26278** [MW0LGE]: `        // MW0LGE`
- **L26295** [MW0LGE]: `        //MW0LGE_21d private static bool m_bControlKeyDown = false; // ke9ns add (used for an extra right click + CTRL function: add bandstacking and hyperlinking) // MW0LGE changed to bool`
- **L26447** [MW0LGE]: `                        SelectRX1VarFilter(false); //MW0LGE_21k8`
- **L26476** [MW0LGE]: `                        SelectRX1VarFilter(false); //MW0LGE_21k8`
- **L26574** [MW0LGE]: `                        SelectRX1VarFilter(false); //MW0LGE_21k8`
- **L26612** [MW0LGE]: `                        SelectRX1VarFilter(false); //MW0LGE_21k8`
- **L26712** [MW0LGE, WD5Y]: `                                    //[2.10.1.0]MW0LGE ideas from WD5Y`
- **L26753** [MW0LGE]: `                        //MW0LGE_218k moved K/L to cltr/alt J so not to conflict with anything user defined as L/K`
- **L27223** [MW0LGE]: `                    NetworkIO.SetTxAttenData(txatt); //[2.10.3.6]MW0LGE att_fixes`
- **L27232** [MW0LGE]: `                enableAudioAmplfier(); // MW0LGE_22b`
- **L27240** [MW0LGE]: `                if (!IsSetupFormNull) SetupForm.BoardWarning = NetworkIO.BoardMismatch; //[2.10.3.9]MW0LGE show warning in setup if board does not match expected`
- **L27242** [MW0LGE]: `                //MW0LGE_21k9 these two moved after the audio start`
- **L27249** [MW0LGE]: `                //multimeter2 MW0LGE_[2.9.0.7]`
- **L27266** [MW0LGE]: `                    //multimeter2 MW0LGE_[2.9.0.7]`
- **L27384** [MW0LGE]: `                        Priority = ThreadPriority.BelowNormal,// Normal, // MW0LGE_12k9c`
- **L27391** [MW0LGE]: `                    m_frmCWXForm.StopEverything(chkPower.Checked); //[2.10.3]MW0LGE`
- **L27397** [MW0LGE]: `                    chk2TONE.Enabled = true; //MW0LGE_21a`
- **L27402** [MW0LGE]: `                //HardwareSpecific.Hardware = NetworkIO.BoardID; // [2.10.3.9]MW0LGE check this, dont like it here ! //[2.10.3.9]MW0LGE - REMOVED !!!`
- **L27437** [MW0LGE]: `                    m_frmCWXForm.StopEverything(chkPower.Checked); //[2.10.3]MW0LGE`
- **L27443** [MW0LGE]: `                chk2TONE.Checked = false;  // MW0LGE_21a`
- **L27453** [MW0LGE]: `                chkVFOBLock.Enabled = false; //[2.10.3.7]MW0LGE`
- **L27485** [MW0LGE]: `                    if (!multimeter_thread.Join(/*500*/Math.Max(meter_delay, meter_dig_delay) + 50)) //MW0LGE change to meter delay`
- **L27490** [MW0LGE]: `                    if (!rx2_meter_thread.Join(/*500*/Math.Max(meter_delay, meter_dig_delay) + 50)) //MW0LGE change to meter delay`
- **L27493** [MW0LGE]: `                //MW0LGE_[2.9.0.7]`
- **L27557** [MW0LGE]: `                    if (!display_volts_amps_thead.Join(650)) // there is a sleep 600 in there MW0LGE`
- **L27571** [MW0LGE]: `            //MW0LGE_21d6`
- **L27581** [MW0LGE]: `        //MW0LGE [2.9.0.8] re-implemented by Warren`
- **L27839** [MW0LGE]: `                    //MW0LGE`
- **L27890** [MW0LGE]: `                    //MW0LGE`
- **L27972** [MW0LGE]: `            m_RX1agcMode = (AGCMode)comboAGC.SelectedIndex; // MW0LGE`
- **L28113** [MW0LGE]: `                    //[2.10.3.12]MW0LGE added incase anything needs to close down instantly irrespective of console closing`
- **L28163** [MW0LGE]: `            //[2.10.3.1]MW0LGE not being done anywhere`
- **L28176** [MW0LGE]: `            Thread.Sleep(200); //[2.10.3.12]MW0LGE give some time for power down, increased to 200ms as psform loops were not detecting power off fast enough`
- **L28181** [MW0LGE]: `                psform.CloseAmpView(); //[2.10.4.3]MW0LGE`
- **L28226** [MW0LGE]: `            if (draw_display_thread != null && draw_display_thread.IsAlive) draw_display_thread.Join(1100); // added 1100, slightly longer than 1fps MW0LGE [2.9.0.7]`
- **L28227** [MW0LGE]: `            Display.ShutdownDX2D(); // MW0LGE`
- **L28230** [MW0LGE]: `            //MW0LGE_21a un-register delegates`
- **L28232** [MW0LGE]: `            if (!IsSetupFormNull) SetupForm.RemoveDelegates(); // MW0LGE_22b`
- **L28238** [MW0LGE]: `            //MW0LGE`
- **L28247** [MW0LGE]: `                SetupForm.WaitForSaveLoad(10000); // MW0LGE [2.9.0.8] wait 10 seconds, should be enough?`
- **L28265** [MW0LGE]: `            shutdownLogStringToPath("Before MeterManager.Shutdown()"); //[2.10.3.6]MW0LGE moved from after hidewb so that listeners can save as part of setup`
- **L28275** [MW0LGE]: `            if (m_frmCWXForm != null && !m_frmCWXForm.IsDisposed) m_frmCWXForm.StopEverything(); //[2.10.3.1]MW0LGE`
- **L28285** [MW0LGE]: `            //MW0LGE_21d -- the db is updated with everything`
- **L28358** [MW0LGE]: `                    if (HardwareSpecific.Model == HPSDRModel.HPSDR) //MW0LGE_21d step atten`
- **L28377** [MW0LGE]: `                // NOTE: lower case db !!! not a very nice implemention //MW0LGE_22b commented`
- **L28425** [MW0LGE]: `                    if (HardwareSpecific.Model == HPSDRModel.HPSDR) //MW0LGE_21d step atten`
- **L28481** [MW0LGE]: `                ptbRX0Gain_Scroll(this, EventArgs.Empty); //MW0LGE_21k9`
- **L28490** [MW0LGE]: `                ptbRX2Gain_Scroll(this, EventArgs.Empty); //MW0LGE_21k9`
- **L28503** [MW0LGE]: `                    radio.GetDSPRX(0, 0).RXOutputGain = 0.0; //MW0LGE_21j`
- **L28504** [MW0LGE]: `                    radio.GetDSPRX(0, 1).RXOutputGain = 0.0; //MW0LGE_21j`
- **L28509** [WD5Y]: `                lblRX1MuteVFOA.Show(); //[2.10.1.0] from WD5Y`
- **L28516** [MW0LGE]: `                radio.GetDSPRX(0, 1).RXOutputGain = (double)ptbRX1Gain.Value / ptbRX1Gain.Maximum; //MW0LGE_21j do this always`
- **L28521** [WD5Y]: `                lblRX1MuteVFOA.Hide(); //[2.10.1.0] from WD5Y`
- **L28651** [W2PA]: `            double pct = Convert.ToDouble(new_pwr) / 100.0;  //-W2PA Send LED update back to Behringer`
- **L28653** [W2PA]: `            else if (pct < 1.0 / 15.0) pct = 1.0 / 15.0; //-W2PA Don't let the last LED go out until zero`
- **L28656** [W2PA]: `            //-W2PA Update LEDs on Behringer MIDI controller mini wheel`
- **L28663** [MW0LGE]: `            if (m_fDrivePower != new_pwr)  // MW0LGE_21k9d`
- **L28674** [MW0LGE]: `            //[2.10.1.0] MW0LGE added`
- **L28695** [MW0LGE]: `                    if (!m_bIgnoreAFChangeForMonitor) TXAF = ptbAF.Value; //MW0LGE_21k9d the if`
- **L28702** [MW0LGE]: `            else if (MOX && chkMON.Checked && !m_bIgnoreAFChangeForMonitor) TXAF = ptbAF.Value; //MW0LGE_21k9 ingore the monitor AF slider change when in mox //MW0LGE_22b added chMON, so only adjust txaf if monitoring`
- **L28704** [MW0LGE]: `            setLinkedAF(0, ptbAF.Value); //[2.10.1.0] MW0LGE`
- **L28739** [W2PA]: `            //-W2PA Update LEDs on Behringer MIDI controller`
- **L28794** [MW0LGE]: `                //[2.10.3.9]MW0LGE fix for when mic is disabled`
- **L28827** [W2PA]: `            //-W2PA Update LEDs on Behringer MIDI controller`
- **L28829** [W2PA]: `            if (pct < 1.0 / 15.0) pct = 1.0 / 15.0;  //-W2PA Don't let the last LED go out`
- **L29053** [MW0LGE]: `                if (penny_ext_ctrl_enabled) //MW0LGE_21k`
- **L29055** [MW0LGE]: `                    int bits = Penny.getPenny().UpdateExtCtrl(lo_band, lo_bandb, _mox, _tuning, SetupForm.TestIMD, chkExternalPA.Checked); //MW0LGE_21j`
- **L29068** [MW0LGE]: `                UpdateTRXAnt(); //[2.3.10.6]MW0LGE added`
- **L29104** [MW0LGE]: `                if (penny_ext_ctrl_enabled) //MW0LGE_21k`
- **L29106** [MW0LGE]: `                    int bits = Penny.getPenny().UpdateExtCtrl(lo_band, lo_bandb, _mox, _tuning, SetupForm.TestIMD, chkExternalPA.Checked); //MW0LGE_21j`
- **L29110** [MW0LGE]: `                UpdateTRXAnt(); //[2.3.10.6]MW0LGE added`
- **L29150** [MW0LGE]: `            chkVFOLock.Enabled = !chkMOX.Checked; //MW0LGE_21d3`
- **L29151** [MW0LGE]: `            chkVFOBLock.Enabled = !chkMOX.Checked; //MW0LGE_21d3`
- **L29170** [MW0LGE]: `                if (!VFOALock) //MW0LGE_21d only unlock them if the vfo is unlocked`
- **L29190** [MW0LGE]: `            //[2.10.3.7]MW0LGE reset`
- **L29200** [MW0LGE]: `            chkVFOLock.Enabled = !chkMOX.Checked; //MW0LGE_21d3`
- **L29201** [MW0LGE]: `            chkVFOBLock.Enabled = !chkMOX.Checked; //MW0LGE_21d3`
- **L29275** [MW0LGE]: `                //MW0LGE we need to move the meters to top, because in collasped mode the bring to fronts above`
- **L29285** [MW0LGE]: `        private bool _forceATTwhenPSAoff = true; //MW0LGE [2.9.0.7] added`
- **L29291** [MW0LGE]: `        private bool _forceATTwhenPowerChangesWhenPSAon = true; //MW0LGE [2.9.3.5] added`
- **L29322** [MW0LGE]: `            bool bOldMox = _mox; //MW0LGE_21b used for state change delgates at end of fn`
- **L29324** [MW0LGE]: `            MoxPreChangeHandlers?.Invoke(rx2_enabled && VFOBTX ? 2 : 1, _mox, chkMOX.Checked); // MW0LGE_21k8`
- **L29355** [MW0LGE]: `            //[2.10.1.0]MW0LGE changed`
- **L29400** [MW0LGE]: `                    //MW0LGE [2.9.0.7]`
- **L29520** [MW0LGE]: `            _pause_DisplayThread = true; // MW0LGE_21k8 turn display off whilst everything is being setup, prevents flashes of pixels etc`
- **L29525** [MW0LGE]: `                if (!chkTUN.Checked && !chk2TONE.Checked) ptbPWR_Scroll(this, EventArgs.Empty); //MW0LGE_22b need this here as we may have adjusted power via tune slider when not in mox`
- **L29561** [MW0LGE]: `                        //MW0LGE [2.9.0.7] added option to always apply 31 att from setup form when not in ps`
- **L29567** [MW0LGE]: `                        SetupForm.ATTOnRX1 = getRX1stepAttenuatorForBand(rx1_band); //[2.10.3.6]MW0LGE att_fixes`
- **L29568** [MW0LGE]: `                        SetupForm.ATTOnTX = txAtt; //[2.10.3.6]MW0LGE att_fixes NOTE: this will eventually call Display.TXAttenuatorOffset with the value                        `
- **L29576** [MW0LGE]: `                    Display.TXAttenuatorOffset = 0; //[2.10.3.6]MW0LGE att_fixes`
- **L29603** [MW0LGE]: `                    Thread.Sleep(space_mox_delay); // default 0 // from PSDR MW0LGE`
- **L29647** [MW0LGE]: `                        //comboRX2Preamp.Enabled = true; //[2.10.3.6]MW0LGE att_fixes`
- **L29648** [MW0LGE]: `                        //udRX2StepAttData.Enabled = true; //[2.10.3.6]MW0LGE att_fixes`
- **L29659** [MW0LGE]: `                    Display.TXAttenuatorOffset = 0; //[2.10.3.6]MW0LGE att_fixes`
- **L29675** [MW0LGE]: `            _pause_DisplayThread = false; //MW0LGE_21k8 re-enable`
- **L29677** [MW0LGE]: `            if (bOldMox != tx) MoxChangeHandlers?.Invoke(rx2_enabled && VFOBTX ? 2 : 1, bOldMox, tx); // MW0LGE_21a`
- **L29694** [MW0LGE]: `                if (chk2TONE.Checked) //MW0LGE_21a`
- **L29793** [MW0LGE]: `                    mode = MeterTXMode.ALC_GROUP; //MW0LGE`
- **L29919** [MW0LGE]: `                        txtVFOAFreq_LostFocus(this, EventArgs.Empty); //[2.10.1.0] MW0LGE added to fix issue when in spit single rx, rit/xit would not be applied correctly on mox`
- **L29980** [MW0LGE]: `            bool oldTune = _tuning; //MW0LGE_21k9d`
- **L29994** [MW0LGE]: `                //MW0LGE_21a`
- **L30043** [MW0LGE]: `                // remember old power //MW0LGE_22b`
- **L30083** [MW0LGE]: `                await Task.Delay(100); // MW0LGE_21k8`
- **L30090** [MW0LGE]: `                // MW0LGE_21k8 moved below mox`
- **L30129** [MW0LGE]: `                //MW0LGE_22b`
- **L30136** [MW0LGE]: `                if (current_meter_tx_mode != old_meter_tx_mode_before_tune) //MW0LGE_21j`
- **L30153** [G8NJJ]: `                SetAriesTuneState(chkTUN.Checked);              // G8NJJ tell ARIES that tune is active`
- **L30155** [MW0LGE]: `            setupTuneDriveSlider(); // MW0LGE_22b`
- **L30411** [MW0LGE]: `            //[2.10.3.6]MW0LGE reimplemented all this including repopulateForms`
- **L30470** [MW0LGE]: `            //MW0LGE_22b re-instated this. It is such a bad way to get the band bars to redraw, but no desire to rework compressed view`
- **L30483** [MW0LGE]: `            //MW0LGE_22b re-instated this. It is such a bad way to get the band bars to redraw, but no desire to rework compressed view`
- **L30496** [MW0LGE]: `            //MW0LGE_22b re-instated this. It is such a bad way to get the band bars to redraw, but no desire to rework compressed view`
- **L30534** [MW0LGE]: `        //[2.10.3.12]MW0LGE added this force so that the filter will get set if focus is lost, ie by the enter keypress being trapped in console.keypress`
- **L30626** [MW0LGE]: `        //MW0LGE_21j used by setup form whenever a TX profile is loaded`
- **L30714** [G8NJJ]: `                // G8NJJ: change so that it sets the correct VAC 1 or 2 sample rate in setup`
- **L30727** [MW0LGE]: `            //MW0LGE_21d`
- **L30741** [MW0LGE]: `            setupZTBButton(); //MW0LGE_21k9`
- **L30801** [W2PA]: `                if (chkCWAPFEnabled.Checked) _cat_apf_status = 1; //-W2PA Added to enable extended CAT control`
- **L30904** [MW0LGE]: `            else //MW0LGE_22b`
- **L31024** [MW0LGE]: `            // MW0LGE in collapsed mode this is only true if we are showing rx1, technically correct to add similar to vfoB, but not required as fall through will handle it`
- **L31072** [MW0LGE]: `                // MW0LGE Console_KeyPress(this, new KeyPressEventArgs((char)Keys.Enter));`
- **L31089** [MW0LGE]: `            // MW0LGE before all, handle the notch size change`
- **L31101** [MW0LGE]: `            if (Common.ShiftKeyDown && step >= 10) step /= 10; //MW0LGE`
- **L31208** [MW0LGE]: `                    // MW0LGE`
- **L31212** [MW0LGE]: `                    // MW0LGE this was other, but now we assume it is top of spectral display`
- **L31314** [MW0LGE]: `            //MW0LGE_21k8`
- **L31348** [W2PA]: `            // Lock the display //-W2PA Don't freeze display if we are zoomed in too far to fit the passband`
- **L31353** [MW0LGE]: `            bool bRitOk = !_mox || (_mox && VFOBTX && RX2Enabled); //[2.10.1.0] MW0LGE we can apply rit`
- **L31355** [MW0LGE]: `            if (_click_tune_display && bCanFitInView && ((_mox && VFOBTX && RX2Enabled) || !_mox || _display_duplex)) //[2.10.1.0] MW0LGE want if moxing rx2`
- **L31367** [MW0LGE]: `                if (!m_bIgnoreLimitsForZTB) // MW0LGE_21k9`
- **L31381** [W2PA]: `                            //-W2PA If we tune beyond the display limits, re-center or scroll display, and keep going.  Original code above just stops tuning at edges.`
- **L31428** [MW0LGE]: `                    if (!rx1_spectrum_tune_drag && bLimitToSpectral)  // MW0LGE_21k9 repos if limited to spectral`
- **L31472** [MW0LGE]: `            if (_click_tune_display && ((_mox && VFOBTX && RX2Enabled) || !_mox || _display_duplex)) //[2.10.1.0] MW0LGE want this to happen if moxing on rx2`
- **L31486** [MW0LGE]: `            //MW0LGE_21k8`
- **L31490** [MW0LGE]: `            if (chkTUN.Checked && chkVFOATX.Checked && !_display_duplex) // MW0LGE only if not display duplex`
- **L31515** [MW0LGE]: `            //MW0LGE_21k8`
- **L31635** [G8NJJ]: `            // added G8NJJ for Aries ATU: see if ARIES needs a new frequency update and antenna band update`
- **L31649** [MW0LGE]: `                if (penny_ext_ctrl_enabled) //MW0LGE_21k`
- **L31651** [MW0LGE]: `                    int bits = Penny.getPenny().UpdateExtCtrl(lo_band, lo_bandb, _mox, _tuning, SetupForm.TestIMD, chkExternalPA.Checked); //MW0LGE_21j`
- **L31706** [MW0LGE]: `                if (alexpresent && rx1_band == Band.B6M && // chksr button was hidden and always unchecked. This has become the 2TON button MW0LGE_21a`
- **L31845** [W2PA]: `                    if (_click_tune_display) //-W2PA This was preventing proper receiver adjustment`
- **L31847** [MW0LGE]: `                        // MW0LGE_21b changed this block to include CW shift with xvtr`
- **L31849** [W2PA]: `                        switch (RX1DSPMode)  //-W2PA Account for offset when in CW modes.`
- **L31861** [MW0LGE]: `                    if (chkEnableMultiRX.Checked && !_mox) //MW0LGE [2.7.0.9] only when RX'ing. Fixes issue where multirx would be outside sample area after a tx`
- **L31895** [MW0LGE]: `            //MW0LGE_21d`
- **L32025** [MW0LGE]: `                    m_bVFOAChangedByKeys = false; //MW0LGE_21a`
- **L32100** [MW0LGE]: `                freq = VFOASubFreq;// double.Parse(txtVFOABand.Text); //MW0LGE //[2.10.3.6]freq changes.`
- **L32271** [MW0LGE]: `            //MW0LGE_21d`
- **L32289** [MW0LGE]: `                //oldBand = RX1Band;  //[2.10.3.9]MW0LGE now done below for rx2 as RX1Band is obviously wrong`
- **L32323** [MW0LGE]: `                if (alexpresent && rx2_band == Band.B6M && // chkSR2 button was hidden and always unchecked. This has become the 2TON button MW0LGE_21a`
- **L32326** [MW0LGE]: `                else RX2_6mGainOffset = 0; // MW0LGE_21d doing this always !`
- **L32340** [W2PA]: `            // Lock the display //-W2PA Don't freeze display if we are zoomed in too far to fit the passband`
- **L32345** [MW0LGE]: `            bool bRitOk = !_mox || (_mox && VFOATX && RX2Enabled); //[2.10.1.0] MW0LGE we can apply rit to rx2`
- **L32349** [MW0LGE]: `                if (_click_tune_rx2_display && bCanFitInView && ((_mox && VFOATX && RX2Enabled) || !_mox)) //[2.10.1.0] MW0LGE want if moxing`
- **L32353** [MW0LGE]: `                    //MW0LGE`
- **L32362** [MW0LGE]: `                    if (!m_bIgnoreLimitsForZTB) // MW0LGE_21k9`
- **L32366** [W2PA]: `                            //-W2PA Original 3.4.1 code`
- **L32377** [W2PA]: `                                //-W2PA If we tune beyond the display limits, re-center or scroll display, and keep going.  Original code above just stops tuning at edges.`
- **L32422** [MW0LGE]: `                        if (!rx2_spectrum_tune_drag && bLimitToSpectral) // MW0LGE_21k9`
- **L32460** [MW0LGE]: `            if (chkEnableMultiRX.Checked && !rx2_enabled && !_mox)  //MW0LGE [2.7.0.9] only when RX'ing. Fixes issue where multirx would be outside sample area after a tx`
- **L32470** [MW0LGE]: `                else chkEnableMultiRX.Checked = false; // MW0LGE [2.9.0.7] same as vfoA lost focus`
- **L32473** [MW0LGE]: `            //[2.10.3.7]MW0LGE limits added`
- **L32489** [W2PA]: `                //-W2PA Freeze display unless we are zoomed in too far to fit the passband`
- **L32490** [MW0LGE]: `                if (_click_tune_rx2_display && ((_mox && VFOATX && RX2Enabled) || !_mox)) // [2.10.1.0] MW0LGE want if moxing`
- **L32497** [MW0LGE]: `                if (chkTUN.Checked && chkVFOBTX.Checked) // MW0LGE only if not duplex //MW0LGE_21k8 !display_duplex commented as always in dup off mode on rx2 when tx'ing tune`
- **L32575** [MW0LGE]: `                        Display.VFOASub = (long)(freq * 1e6); //MW0LGE_21k8`
- **L32576** [MW0LGE]: `                    else //MW0LGE_21k8`
- **L32582** [MW0LGE]: `                if (chkTUN.Checked && chkVFOBTX.Checked && !_display_duplex) // MW0LGE_21k8 only if not duplex`
- **L32590** [MW0LGE]: `                            Display.VFOASub += cw_pitch; // needed if only rx1 in use, and we are split. Display uses VFOASub to display split freq MW0LGE_21k8`
- **L32647** [MW0LGE]: `                if (penny_ext_ctrl_enabled) //MW0LGE_21k`
- **L32649** [MW0LGE]: `                    int bits = Penny.getPenny().UpdateExtCtrl(lo_banda, lo_band, _mox, _tuning, SetupForm.TestIMD, chkExternalPA.Checked); //MW0LGE_21j`
- **L32747** [MW0LGE]: `            //MW0LGE_21d should happen before the check below`
- **L32854** [W2PA]: `            if (_click_tune_rx2_display) //-W2PA This was preventing proper receiver adjustment`
- **L32856** [MW0LGE]: `                // MW0LGE_21i changed this block to include CW shift with xvtr`
- **L32858** [W2PA]: `                switch (RX2DSPMode)  //-W2PA Account for offset when in CW modes.`
- **L32880** [G8NJJ]: `            // added G8NJJ for Aries ATU: see if ARIES needs a new frequency update and antenna band update`
- **L32941** [MW0LGE]: `                    m_bVFOBChangedByKeys = false; //MW0LGE_21a`
- **L33229** [MW0LGE]: `        //NOTCH MW0LGE`
- **L33235** [MW0LGE]: `        private int m_nNotchRX = 0; //MW0LGE_21e`
- **L33317** [MW0LGE]: `                            (Display.RX2PreampOffset - Display.AlexPreampOffset) - Display.RX2FFTSizeOffset); //MW0LGE_21k5 change to rx2`
- **L33336** [MW0LGE]: `                        //[2.10.1.0] MW0LGE changes so that CTUN on works for filter drag`
- **L33344** [MW0LGE]: `                    //MW0LGE_21h changes so that CTUN on works for filter drag`
- **L33359** [MW0LGE]: `                    //MW0LGE_21h changes so that CTUN on works for filter drag`
- **L33447** [MW0LGE]: `            //MW0LGE returns the frequecny (Hz) at a given pixel`
- **L33595** [MW0LGE]: `                m_bIgnoreZoomCentre = true; //[2.10.3.5]MW0LGE used in ptbDisplayZoom_Scroll to ignore the shift key which might be held for RX2`
- **L33680** [MW0LGE]: `            if (!m_bIgnoreZoomCentre) //[2.10.3.5]MW0LGE fixes #345`
- **L33682** [MW0LGE]: `                // MW0LGE shift modifier`
- **L33685** [W2PA]: `                if ((ClickTuneDisplay && zoomingIn) && bCentre)  //-W2PA Force centering display when zooming in with CTUN on, to keep the vfo within the display`
- **L33690** [MW0LGE]: `                if (rx2_enabled && (ClickTuneRX2Display && zoomingIn) && bCentre)  //MW0LGE - we should do rx2 as well !`
- **L33704** [MW0LGE]: `            if (dOldZoomFactor != zoom_factor) ZoomFactorChangedHandlers?.Invoke(dOldZoomFactor, zoom_factor, ptbDisplayZoom.Value); //MW0LGE_21d`
- **L33768** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L33774** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L33780** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L33786** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L33792** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L33798** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L33804** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L33810** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L33816** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L33822** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L33828** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L33834** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L33840** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L33851** [MW0LGE]: `            //MW0LGE_21d new bandstack system`
- **L33874** [MW0LGE]: `        // MW0LGE_21d used to default colours of all button+radio controls, and inside other panels or groups`
- **L33902** [MW0LGE]: `        private bool _ignore_sidetone_change = false; //[2.10.3.10]MW0LGE used in chkMON_CheckedChanged to ignore making changes to cw sidetone as new dsp mode wont have been set yet, fixes #575`
- **L33907** [MW0LGE]: `            //MW0LGE_21d`
- **L34142** [MW0LGE]: `                    if (new_mode != DSPMode.DIGL) bRecallDigiModeSettings = true; //[2.10.3]MW0LGE done below after tx profile change`
- **L34264** [MW0LGE]: `                        if (old_mode != DSPMode.CWL && old_mode != DSPMode.CWU)//[2.10.3]MW0LGE fixes #59 mon issue, did not consider old_mode and igmored CWU`
- **L34385** [MW0LGE]: `                        bStoreDigiModeSettings = true; //[2.10.3]MW0LGE done after tx profile change at end as we will overwrite existng tx profile if auto save is on`
- **L34392** [MW0LGE]: `                    if (chkVFOATX.Checked || !rx2_enabled) //[2.10.3.7]MW0LGE added  || !rx2_enabled`
- **L34455** [MW0LGE]: `            SelectModeDependentPanel(); //MW0LGE_21k9d`
- **L34467** [MW0LGE]: `                    int halfBw = (int)(radio.GetDSPRX(0, 0).RXFMDeviation + radio.GetDSPRX(0, 0).RXFMHighCut); //[2.10.3.4]MW0LGE`
- **L34501** [W2PA]: `                // Although CWFWKeyer is mostly a deprecated flag, it's useful in the QSK-enabled firmware (1.7 or later) -W2PA`
- **L34532** [MW0LGE]: `            // MW0LGE from powersdr - selects tx profiles `
- **L34557** [MW0LGE]: `            //MW0LGE_21b`
- **L34614** [MW0LGE]: `            if (mode != DSPMode.FIRST) //MW0LGE_21k9d4`
- **L34636** [MW0LGE]: `            // MW0LGE_21j`
- **L34651** [MW0LGE]: `            Filter oldFilter = rx1_filter; //MW0LGE_21d`
- **L34842** [G8NJJ]: `            if (filterPopupForm != null) filterPopupForm.RepopulateForm();          // G8NJJ update popup`
- **L34940** [G8NJJ]: `            if (filterPopupForm != null) filterPopupForm.RepopulateForm();      // G8NJJ update popup`
- **L34942** [MW0LGE]: `            // MW0LGE`
- **L34950** [MW0LGE]: `            //MW0LGE_21d filter - work if mouse over as well`
- **L34971** [MW0LGE]: `            //MW0LGE_21d filter - work if mouse over as well`
- **L35043** [MW0LGE]: `                case DSPMode.SPEC: //MW0LGE_21k9`
- **L35138** [MW0LGE]: `            if (new_center == _oldFilterShiftCentre)  // if the new_center hasnt changed, ignore it MW0LGE_[2.9.0.6]`
- **L35145** [MW0LGE]: `            // stop filter moving over 0 MW0LGE_21k9`
- **L35283** [MW0LGE]: `            SelectRX1VarFilter(false, _filter_width_update_from_cat); //[2.10.3.12]MW0LGE we update below`
- **L35631** [MW0LGE]: `            //MW0LGE [2.9.0.7] also in VFOASubUpdate`
- **L35642** [MW0LGE]: `        private bool _bOldVFOSplit = false; //MW0LGE_22a`
- **L35677** [MW0LGE]: `            //[2.10.1.0] MW0LGE apply a quick shift`
- **L35753** [MW0LGE]: `            if (_bOldVFOSplit != chkVFOSplit.Checked) //MW0LGE_22a`
- **L35894** [MW0LGE]: `            updateVFOFreqs(_mox); //[2.10.1.0] MW0LGE we might need to update everything if tx'ing on sub, use std function`
- **L35922** [MW0LGE]: `            //[2.10.1.0] MW0LGE need this so that osc offset gets assigned if needed`
- **L35941** [MW0LGE]: `            //[2.10.1.0] MW0LGE need this so that osc offset gets assigned if needed`
- **L35948** [W2PA]: `            setRIT_LEDs();  //-W2PA Behringer LEDs`
- **L35950** [W2PA]: `            //-W2PA Sync XIT/XIT if selected`
- **L35970** [MW0LGE]: `                updateVFOFreqs(_mox); //[2.10.1.0] MW0LGE we might need to update everything if tx'ing on sub, use std function`
- **L35976** [W2PA]: `            setXIT_LEDs(); //-W2PA Behringer LEDs`
- **L35978** [W2PA]: `            //-W2PA Sync XIT/XIT if selected`
- **L36005** [W2PA]: `            //-W2PA Update LEDs on Behringer MIDI controller, within limits of +/- 2kHz.  Beyond that range the extreme L or R LED remains lit.`
- **L36006** [W2PA]: `            int IT_MIDIminimum = -2000; //-W2PA Change these two values to enable a broader range for the LEDs`
- **L36013** [W2PA]: `            //-W2PA Light the center LED (#8) only if exactly at zero RIT/XIT`
- **L36017** [W2PA]: `            //-W2PA Prevent the lowest LED from going out completely.`
- **L36024** [W2PA]: `            //-W2PA Update LEDs on Behringer MIDI controller, within limits of +/- 2kHz`
- **L36025** [W2PA]: `            int IT_MIDIminimum = -2000; //-W2PA Change these two values to enable a broader range for the LEDs`
- **L36032** [W2PA]: `            //-W2PA Light the center LED (#8) only if exactly at zero RIT/XIT`
- **L36036** [W2PA]: `            //-W2PA Prevent the lowest LED from going out completely.`
- **L36143** [MW0LGE]: `                if (_click_tune_display) //MW0LGE_21d`
- **L36153** [MW0LGE]: `                //MW0LGE_21d belts and braces`
- **L36177** [MW0LGE]: `                    //[2.10.2.3]MW0LGE timeout version used, with 10 times frame rate to give some additional time`
- **L36522** [MW0LGE]: `            //[2.10.3.5]MW0LGE`
- **L36523** [MW0LGE]: `            if (!initializing) radio.GetDSPRX(0, 1).Active = chkEnableMultiRX.Checked; //MW0LGE only set after init complete`
- **L36528** [MW0LGE]: `                cmaster.SetAAudioMixWhat((void*)0, 0, 1, !Audio.MuteRX1); //[2.10.3.9]MW0LGE now considers RX1 audio mute state`
- **L36567** [MW0LGE]: `                if (rx2_enabled)   // <-- rx2_enabled, so how can it be also in the else, bunch commented out MW0LGE [2.9.0.7]`
- **L36596** [MW0LGE]: `            // MW0LGE`
- **L36638** [MW0LGE]: `            //MWLGE_21k9 re-worked //[2.10.1.0] MW0LGE added eventargs empty`
- **L36641** [MW0LGE]: `            if (chkMUT.Checked && m_bMuteWillMuteVAC1) //MW0LGE_21k9`
- **L36653** [MW0LGE]: `            setLinkedAF(1, ptbRX1AF.Value); //[2.10.1.0] MW0LGE`
- **L36660** [MW0LGE]: `            //MW0LGE_21k9 moved here (now same as ptbRX2Gain_Scroll)`
- **L36661** [W2PA]: `            //-W2PA Update LEDs on Behringer MIDI controller`
- **L36678** [MW0LGE]: `            //[2.10.1.0] MW0LGE consider mute when on vac`
- **L36691** [MW0LGE]: `            setLinkedAF(2, ptbRX1Gain.Value); //[2.10.1.0] MW0LGE`
- **L36876** [MW0LGE]: `            // MW0LGE changes made to this function so that RX1 meter fills space to right of VFOB box, also delay repaint until all controls moved`
- **L36877** [MW0LGE]: `            SuspendDrawing(this); //MW0LGE`
- **L36906** [MW0LGE]: `                    moveModeSpecificPanels();// [2.10.3.4]MW0LGE  SelectModeDependentPanel will deal with this when collapsed`
- **L36913** [MW0LGE]: `                    //MW0LGE -- uses pad radio between meter and vfoB`
- **L36951** [MW0LGE]: `                panelRX2Mode.Location = new Point(gr_RX2Mode_basis_location.X + (int)(h_delta * 0.492), gr_RX2Mode_basis_location.Y + v_delta); // MW0LGE changed to gr_RX2Mode_basis_location`
- **L36984** [MW0LGE]: `            setPAProfileLabelPos();  //[2.10.1.0] MW0LGE`
- **L36986** [MW0LGE]: `            ResumeDrawing(this); //MW0LGE`
- **L37030** [MW0LGE]: `            // MW0LGE_21k9rc6 new resize implementation`
- **L37079** [MW0LGE]: `            //MW0LGE`
- **L37098** [MW0LGE]: `            chk_rx2_mut_basis = chkRX2Mute.Location; //MW0LGE`
- **L37200** [G8NJJ]: `            // G8NJJ - to allow RIT and XIT to show in collapsed view`
- **L37215** [MW0LGE]: `        private bool rx1_enabled = true; // always true for now MW0LGE_21a`
- **L37218** [MW0LGE]: `        //MW0LGE_21a added for completness at this stage`
- **L37268** [MW0LGE]: `                        //multimeter2 MW0LGE_[2.9.0.7]`
- **L37369** [MW0LGE]: `            _pause_DisplayThread = true; //MW0LGE_21k8 hide the changes`
- **L37377** [MW0LGE]: `            //[2.10.3.9]MW0LGE restore VAC on/off state for VAC2 if the TX profile is configured to do so`
- **L37443** [MW0LGE]: `            //MW0LGE_21d linear gradient rebuild`
- **L37447** [MW0LGE]: `                Display.FastAttackNoiseFloorRX2 = true; // MW0LGE_21k`
- **L37450** [MW0LGE]: `            //[2.10.3.7]MW0LGE force update for vfoB, as sometimes at start vfoB would be fine, but spectrum would be at 0mhz`
- **L37453** [MW0LGE]: `            // MW0LGE`
- **L37461** [MW0LGE]: `            if (!m_bResizeDX2Display && (oldRX2Enabled != RX2Enabled)) m_bResizeDX2Display = true; // MW0LGE_22b force resize is rx2 enabled state is changed, this may also be set by reisze calls above`
- **L37463** [MW0LGE]: `            _pause_DisplayThread = false; //MW0LGE_21k8`
- **L37467** [MW0LGE]: `                SetQuickSplit(); //[2.10.1.0] MW0LGE`
- **L37514** [MW0LGE]: `            updateVFOFreqs(_mox); //[2.10.1.0] MW0LGE replaced above`
- **L37592** [MW0LGE]: `            Band oldBand = RX2Band; //MW0LGE_21d`
- **L37916** [MW0LGE]: `                    if (chkVFOBTX.Checked && rx2_enabled)//[2.10.3.7]MW0LGE added rx2_enabled`
- **L37935** [MW0LGE]: `                    if (chkVFOBTX.Checked && rx2_enabled)//[2.10.3.7]MW0LGE added rx2_enabled`
- **L38036** [MW0LGE]: `                    int halfBw = (int)(radio.GetDSPRX(1, 0).RXFMDeviation + radio.GetDSPRX(1, 0).RXFMHighCut); //[2.10.3.4]MW0LGE`
- **L38070** [MW0LGE]: `            //MW0LGE_21b`
- **L38119** [MW0LGE]: `            //setRX2ModeLabels(radiobut); //MW0LGE_21j`
- **L38229** [MW0LGE]: `            Filter oldFilter = rx2_filter; //MW0LGE_21d`
- **L38320** [MW0LGE]: `            if (filterAndDspModeValid(2) && oldFilter != rx2_filter) FilterChangedHandlers?.Invoke(2, oldFilter, rx2_filter, RX2Band, rx2_filters[(int)_rx2_dsp_mode].GetLow(rx2_filter), rx2_filters[(int)_rx2_dsp_mode].GetHigh(rx2_filter), rx2_filters[(int)_rx2_dsp_mode].GetName(rx2_filter)); //MW...`
- **L38379** [MW0LGE]: `            //MW0LGE_21d filter`
- **L38406** [MW0LGE]: `            //MW0LGE_21d filter`
- **L38547** [W2PA]: `            //-W2PA Update LEDs on Behringer MIDI controller`
- **L38606** [MW0LGE]: `            //MWLGE_21k9 re-worked //[2.10.1.0] MW0LGE event args empty`
- **L38609** [MW0LGE]: `            if (chkRX2Mute.Checked && m_bMuteWillMuteVAC2) //MW0LGE_21k9`
- **L38621** [MW0LGE]: `            setLinkedAF(3, ptbRX2Gain.Value); //[2.10.1.0] MW0LGE`
- **L38628** [W2PA]: `            //-W2PA Update LEDs on Behringer MIDI controller`
- **L38653** [MW0LGE]: `                    radio.GetDSPRX(1, 0).RXOutputGain = 0.0; //MW0LGE_21j`
- **L38657** [WD5Y]: `                lblRX2MuteVFOB.Show(); //[2.10.1.0] from WD5Y`
- **L38663** [MW0LGE]: `                radio.GetDSPRX(1, 0).RXOutputGain = (double)ptbRX2Gain.Value / ptbRX2Gain.Maximum; //MW0LGE_21j`
- **L38667** [WD5Y]: `                lblRX2MuteVFOB.Hide(); //[2.10.1.0] from WD5Y`
- **L38786** [MW0LGE]: `            if (m_bDeferUpdateDSP) return; //MW0LGE_21k9d`
- **L38892** [MW0LGE]: `                    // MW0LGE_21k9`
- **L38904** [MW0LGE]: `                    filttypetx = dsp_filt_type_dig_tx; //rx; MW0LGE_21kd5`
- **L39302** [MW0LGE]: `            //[2.10.3.6]MW0LGE no band change on TX fix`
- **L39306** [MW0LGE]: `            //[2.10.3.6]MW0LGE added frequency_only so that it can be used as a way to select a band for rx1, vfob, in the vfo display system`
- **L39307** [MW0LGE]: `            //MW0LGE_21d BandStack2 ineresting... applies to rx2`
- **L39334** [MW0LGE]: `            // MW0LGE reinstated and moved block to another function to be called from here, and via CAT`
- **L39417** [MW0LGE]: `            //MW0LGE`
- **L39430** [MW0LGE]: `            h_delta = this.Width - console_basis_size.Width;                //MW0LGE_[2.9.0.7] might actually want to set the globals`
- **L39462** [MW0LGE]: `            m_RX2agcMode = (AGCMode)comboRX2AGC.SelectedIndex; // MW0LGE`
- **L39565** [MW0LGE]: `            if (diversityForm != null) //[2.10.3.5]MW0LGE`
- **L39674** [MW0LGE]: `            if (chkVFOATX.Checked) VFOTXChangedHandlers?.Invoke(false, m_bLastVFOATXsetting, true);  // MW0LGE_21k9c`
- **L39676** [MW0LGE]: `            m_bLastVFOATXsetting = chkVFOATX.Checked; // MW0LGE_21k9d rc3`
- **L39717** [MW0LGE]: `                //[2.10.1.0] MW0LGE moved here from below`
- **L39721** [MW0LGE]: `                updateVFOFreqs(_mox); //[2.10.1.0] MW0LGE changed to std update function`
- **L39779** [MW0LGE]: `            Penny.getPenny().VFOBTX = chkVFOBTX.Checked; // MW0LGE_21j `
- **L39784** [MW0LGE]: `            if (chkVFOBTX.Checked) VFOTXChangedHandlers?.Invoke(true, m_bLastVFOBTXsetting, true); // MW0LGE_21k9c`
- **L39786** [MW0LGE]: `            m_bLastVFOBTXsetting = chkVFOBTX.Checked; // MW0LGE_21k9d rc3`
- **L39998** [MW0LGE]: `            //MW0LGE_21e XVTR`
- **L40055** [MW0LGE]: `                //if (bSelected) SelectedNotch = MNotchDB.GetFirstNotchThatMatches(fcenter, fwidth, bActive); //MW0LGE [2.9.0.7] fix old bug, we need to find the notch for the updated freq`
- **L40176** [MW0LGE]: `            fFreqHZ = Math.Round(fFreqHZ); //[2.10.3.7]MW0LGE moved from below`
- **L40178** [MW0LGE]: `            //MW0LGE_21e XVTR`
- **L40275** [MW0LGE]: `            vfoHz += notchSidebandShift(rx); //MW0LGE_21k9rc4`
- **L40299** [MW0LGE]: `                //[2.10.3.9]MW0LGE fix for when mic is disabled`
- **L40348** [MW0LGE]: `                    int halfBw = (int)(radio.GetDSPRX(0, 0).RXFMDeviation + radio.GetDSPRX(0, 0).RXFMHighCut); //[2.10.3.4]MW0LGE`
- **L40353** [MW0LGE]: `                    int halfBw = (int)(radio.GetDSPRX(1, 0).RXFMDeviation + radio.GetDSPRX(1, 0).RXFMHighCut); //[2.10.3.4]MW0LGE`
- **L40358** [MW0LGE]: `                    int halfBw = (int)(radio.GetDSPTX(0).TXFMDeviation + radio.GetDSPTX(0).TXFMHighCut); //[2.10.3.4]MW0LGE`
- **L40377** [MW0LGE]: `                    int halfBw = (int)(radio.GetDSPRX(0, 0).RXFMDeviation + radio.GetDSPRX(0, 0).RXFMHighCut); //[2.10.3.4]MW0LGE`
- **L40382** [MW0LGE]: `                    int halfBw = (int)(radio.GetDSPRX(1, 0).RXFMDeviation + radio.GetDSPRX(1, 0).RXFMHighCut); //[2.10.3.4]MW0LGE`
- **L40387** [MW0LGE]: `                    int halfBw = (int)(radio.GetDSPTX(0).TXFMDeviation + radio.GetDSPTX(0).TXFMHighCut); //[2.10.3.4]MW0LGE`
- **L40395** [MW0LGE]: `            FMTXOffsetMHz = (double)udFMOffset.Value; //MW0LGE_21k9`
- **L40549** [MW0LGE]: `            TXFreq = record.TXFreq; //MW0LGE_21k9 moved here after the split, and done always`
- **L40551** [MW0LGE]: `            if (RF != record.AGCT && AutoAGCRX1) AutoAGCRX1 = false; // turn off 'auto agc' only if different MW0LGE_21k8`
- **L40922** [MW0LGE]: `                this.bandToolStripMenuItem.Visible = !m_bShowBandControls && IsCollapsedView; //[2.10.3.6]MW0LGE added && CollapsedDisplay. The item will be shown elsewhere if this property is changed whilst in expanded`
- **L40938** [MW0LGE]: `                this.modeToolStripMenuItem.Visible = !m_bShowModeControls && IsCollapsedView; //[2.10.3.6]MW0LGE added && CollapsedDisplay. The item will be shown elsewhere if this property is changed whilst in expanded`
- **L40979** [MW0LGE]: `                    panelButtonBar.Hide(); //[2.10.3.6]MW0LGE added so that it will hide if the setup/menu option is changed`
- **L41010** [MW0LGE]: `            //MW0LGE`
- **L41021** [G8NJJ]: `            // added G8NJJ`
- **L41059** [G8NJJ]: `            // G8NJJ`
- **L41086** [MW0LGE]: `            //MW0LGE`
- **L41126** [MW0LGE]: `            chkX2TR.Show();//MW0LGE`
- **L41127** [MW0LGE]: `            chkRX2Mute.Show();//MW0LGE`
- **L41146** [MW0LGE]: `            //MW0LGE -- uses pad radio between meter and vfoB`
- **L41169** [MW0LGE]: `            comboMeterRXMode.Parent = grpMultimeterMenus; //MW0LGE`
- **L41175** [MW0LGE]: `            comboMeterTXMode.Parent = grpMultimeterMenus; //MW0LGE`
- **L41186** [MW0LGE]: `            chkRX2Mute.Parent = panelRX2DSP; // MW0LGE`
- **L41187** [MW0LGE]: `            chkRX2Mute.Location = chk_rx2_mut_basis; // MW0LGE`
- **L41233** [G8NJJ]: `            // G8NJJ`
- **L41255** [MW0LGE]: `            setupHiddenButton();//grpVFOA); //MW0LGE_21a`
- **L41257** [G8NJJ]: `            // G8NJJ`
- **L41275** [MW0LGE]: `            //[2.10.3.6]MW0LGE changed the above to cope with legacy control dynamic removal, now based off left of the ztb button`
- **L41376** [MW0LGE]: `            // [2.10.1.0] MW0LGE`
- **L41391** [MW0LGE]: `            updateAttNudsCombos(); //[2.10.3.6]MW0LGE`
- **L41393** [MW0LGE]: `            updateLegacyMeterControls(true);// [2.10.1.0] MW0LGE`
- **L41395** [MW0LGE]: `            SelectModeDependentPanel(); //MW0LGE [2.9.0.7] moved here`
- **L41396** [MW0LGE]: `            setPAProfileLabelPos(); //[2.10.1.0] MW0LGE`
- **L41402** [MW0LGE]: `            this.Text = BasicTitleBar; //MW0LGE_21a moved here after expaned is true so that title text gets rebuild correctly`
- **L41449** [G8NJJ]: `        // modified G8NJJ to add alternate top/button controls for Andromeda`
- **L41468** [G8NJJ]: `            // added G8NJJ so initial tick on menu item matches the initial displayed state`
- **L41477** [MW0LGE]: `            this.Text = BasicTitleBar + "    " + TitleBarEncoder; //MW0LGE_22b`
- **L41578** [G8NJJ]: `            // G8NJJ: top display with both VFO controls`
- **L41643** [G8NJJ]: `                    // G8NJJ`
- **L41657** [G8NJJ]: `                    // G8NJJ`
- **L41686** [G8NJJ]: `                    // G8NJJ`
- **L41701** [MW0LGE]: `                    picMultiMeterDigital.Size = pic_multi_meter_size_basis;//MW0LGE`
- **L41704** [MW0LGE]: `                    txtMultiText.Size = txt_multi_text_size_basis;//MW0LGE`
- **L41709** [MW0LGE]: `                    chkRX2Mute.Hide(); // MW0LGE`
- **L41710** [MW0LGE]: `                    chkMUT.Parent = this; // MW0LGE`
- **L41711** [MW0LGE]: `                    chkMUT.Show(); // MW0LGE`
- **L41762** [G8NJJ]: `                    // changed G8NJJ to pick up RX1 or RX2 mode`
- **L41775** [G8NJJ]: `                    // G8NJJ`
- **L41797** [MW0LGE]: `                    chkRX2Mute.Parent = this; // MW0LGE`
- **L41799** [MW0LGE]: `                    chkRX2Mute.Show(); // MW0LGE`
- **L41861** [G8NJJ]: `                    // changed G8NJJ to pick up RX1 or RX2 mode`
- **L41881** [MW0LGE]: `                comboPreamp.Hide();//MW0LGE`
- **L41885** [G8NJJ]: `            // G8NJJ: if Andromeda button bar, show its button panel`
- **L41928** [MW0LGE]: `            updateLegacyMeterControls(false);// [2.10.1.0] MW0LGE`
- **L41930** [MW0LGE]: `            // [2.10.1.0] MW0LGE`
- **L41947** [MW0LGE]: `            SelectModeDependentPanel(); //MW0LGE [2.9.0.7] moved here`
- **L41948** [MW0LGE]: `            setPAProfileLabelPos(); //[2.10.1.0] MW0LGE`
- **L42004** [G8NJJ]: `                    // G8NJJ: deliberately set RX1 meter and txt size to RX2 ones size in Andromeda`
- **L42012** [MW0LGE]: `                    setupHiddenButton();// grpVFOA); //MW0LGE_21a`
- **L42029** [G8NJJ]: `                    // G8NJJ`
- **L42030** [MW0LGE]: `                    setupHiddenButton();// grpVFOA); //MW0LGE_21a`
- **L42033** [G8NJJ]: `            //            else if (m_bShowModeControls)         /// changed G8NJJ - wrong variable used?`
- **L42077** [MW0LGE]: `                    chkMUT.Location = new Point(grpVFOA.Location.X + grpVFOA.Width + 4, chkMON.Location.Y);  //MW0LGE -- move mute to right side of vfo box`
- **L42099** [MW0LGE]: `                    setupHiddenButton();// grpVFOA); //MW0LGE_21a`
- **L42129** [MW0LGE]: `                    chkRX2Mute.Location = new Point(grpVFOB.Location.X + grpVFOB.Width + 4, chkMON.Location.Y);  //MW0LGE -- move mute to right side of vfo box`
- **L42154** [MW0LGE]: `                    setupHiddenButton();// grpVFOB); //MW0LGE_21a`
- **L42191** [G8NJJ]: `                // G8NJJ`
- **L42238** [G8NJJ]: `            // G8NJJ to add new Andromeda button bar in place of band, mode controls`
- **L42692** [MW0LGE]: `            // MW0LGE_21a`
- **L42695** [MW0LGE]: `            btnHidden.Location = new Point(-1000, -1000); // [2.10.3.6]MW0LGE off screen`
- **L42902** [MW0LGE]: `            //[2.10.1.0] MW0LGE`
- **L42914** [MW0LGE]: `            //[2.10.1.0] MW0LGE`
- **L42940** [G8NJJ]: `            UpdateButtonBarButtons();               // G8NJJ - update the button bar`
- **L42961** [G8NJJ]: `            UpdateButtonBarButtons();               // G8NJJ - update the button bar`
- **L42987** [MW0LGE]: `            if (!IsSetupFormNull) SetupForm.ATTOnRX1 = (int)udRX1StepAttData.Value; //[2.10.3.6]MW0LGE att_fixes`
- **L42999** [MW0LGE]: `            if (!IsSetupFormNull) SetupForm.ATTOnRX2 = (int)udRX2StepAttData.Value; //[2.10.3.6]MW0LGE att_fixes`
- **L43011** [MW0LGE]: `                if (RX1RX2usingSameADC) SetupForm.RX2EnableAtt = SetupForm.RX1EnableAtt; //MW0LGE_22b`
- **L43020** [MW0LGE]: `                if (RX1RX2usingSameADC) SetupForm.RX1EnableAtt = SetupForm.RX2EnableAtt;//MW0LGE_22b`
- **L43086** [MW0LGE]: `            //MW0LGE_21d`
- **L43096** [MW0LGE]: `            setupZTBButton(); //MW0LGE_21k9`
- **L43102** [MW0LGE]: `            psform.SetupForm(); //MW0LGE_[2.9.0.7]`
- **L43163** [MW0LGE]: `                        //[2.10.3]MW0LGE only recover these if not coming from a TX profile as LoadedTXProfile uses this function`
- **L43472** [MW0LGE]: `        private bool _wb_caused_alex_hpf_bypass = false; //[2.10.3.7]MW0LGE fixes #529`
- **L43644** [MW0LGE]: `            // MW0LGE`
- **L43683** [MW0LGE]: `            Invoke(new MethodInvoker(BandStack2Form.Show)); //MW0LGE_21d`
- **L43691** [MW0LGE]: `            if (initializing) return; // MW0LGE`
- **L43698** [MW0LGE]: `            if (initializing) return; // MW0LGE`
- **L43721** [MW0LGE]: `                //MW0LGE [2.10.3.6] PS in autocal might have enabled AttOnTX even though setup form option is disabled, fix this case`
- **L43759** [MW0LGE]: `        //MW0LGE`
- **L43799** [MW0LGE]: `        //MW0LGE`
- **L43893** [MW0LGE]: `        // MW0LGE`
- **L43915** [MW0LGE]: `            m_objRawinput.KeyPressed += OnKeyPressedRaw; //[2.10.3.6]MW0LGE now send to MeterManager`
- **L44159** [MW0LGE]: `                    this.Size = s + DropShadowSize; // add in the drop shadow, so the edge to edge is actually what we want //MW0LGE_21a`
- **L44534** [MW0LGE]: `            // MW0LGE generate arrow key presses by passing them to console_keydown`
- **L44688** [MW0LGE]: `            updateResolutionStatusBarText(); //MW0LGE_21b need to call this here so that drop shadow sizes can be obtained`
- **L44695** [MW0LGE]: `            // MW0LGE_21e could not get the designer to keep widths of these`
- **L44759** [MW0LGE]: `            setupTuneDriveSlider(); // MW0LGE_22b`
- **L44770** [MW0LGE]: `            //MW0LGE_21d band stack 2`
- **L44774** [MW0LGE]: `        //MW0LGE_21d3`
- **L44843** [MW0LGE]: `        // MW0LGE_21b A move towards delegate/event based system`
- **L45308** [MW0LGE]: `                //MW0LGE_21g update title bar as we should now know FW`
- **L45349** [MW0LGE]: `                //MW0LGE_21h`
- **L45385** [MW0LGE]: `            //MW0LGE_21h`
- **L45401** [MW0LGE]: `                //MW0LGE_21h`
- **L45416** [MW0LGE]: `            //MW0LGE_21d3 select the now highlighted one`
- **L45420** [MW0LGE]: `            //MW0LGE_21h`
- **L45425** [MW0LGE]: `            //[2.10.3.13]MW0LGE removed as only done when frequencies are bein modified`
- **L45427** [MW0LGE]: `            ////MW0LGE_21h`
- **L45431** [MW0LGE]: `            //MW0LGE_21h`
- **L45467** [MW0LGE]: `            //MW0LGE_21h`
- **L45502** [MW0LGE]: `            //MW0LGE_21h`
- **L45544** [MW0LGE]: `            //[2.10.3.6]MW0LGE no band change on TX fix`
- **L45662** [MW0LGE]: `            //[2.10.3.6]MW0LGE no band change on TX fix`
- **L45695** [MW0LGE]: `            //reset smeter pixel history //MW0LGE_21a`
- **L45716** [MW0LGE]: `            //reset the cw auto mode return [2.10.3.12]MW0LGE`
- **L45722** [MW0LGE]: `            //reset smeter pixel history //MW0LGE_21a`
- **L45743** [MW0LGE]: `            //[2.10.3.13]MW0LGE removed as only done when frequencies are bein modified`
- **L45764** [MW0LGE]: `            //[2.10.3.13]MW0LGE removed as only done when frequencies are bein modified`
- **L45796** [MW0LGE]: `            //MW0LGE_21k disable xPA if not permitted to hot switch`
- **L45804** [MW0LGE]: `            //reset smeter pixel history //MW0LGE_21a`
- **L45864** [MW0LGE]: `            //MW0LGE_21j`
- **L45865** [MW0LGE]: `            //if (!chkPower.Checked) return;//[2.10.3.12]MW0LGE commented this out, so that we actualy call the delegates if this is on at start up`
- **L45874** [MW0LGE]: `            if (penny_ext_ctrl_enabled) //MW0LGE_21k`
- **L45964** [MW0LGE]: `            // MW0LGE_21k9d values are already offset as part of Display`
- **L45976** [MW0LGE]: `                size = (double)specRX.GetSpecRX(0).FFTSize; // MW0LGE_21k7`
- **L46081** [MW0LGE]: `                    float fDelta = (float)Math.Abs(SetupForm.DisplayGridMax - SetupForm.DisplayGridMin); // abs incase MW0LGE [2.9.0.7]`
- **L46097** [MW0LGE]: `                    float fDelta = (float)Math.Abs(SetupForm.RX2DisplayGridMax - SetupForm.RX2DisplayGridMin); // abs incase MW0LGE [2.9.0.7]`
- **L46336** [MW0LGE]: `        //MW0LGE_21k9 to fix issues where some GPU drivers do not allow xor mono cursor on top of DirectX render target`
- **L46546** [MW0LGE]: `            if (m_fTuneDrivePower != new_pwr)  // MW0LGE_21k9d`
- **L46740** [MW0LGE]: `            //[2.10.3.5]MW0LGE max tx attenuation when power is increased and PS is enabled`
- **L46849** [MW0LGE]: `                    ////[2.10.3.9]MW0LGE sub rx (future)`
- **L47219** [MW0LGE]: `                        //[2.10.3.5]MW0LGE convert to a 0-100 scale from a -160 to 0 scale`
- **L47231** [MW0LGE]: `                        //[2.10.3.5]MW0LGE reverted back to a -160 to 0 scale`
- **L47243** [MW0LGE]: `                    //[2.10.3.5]MW0LGE convert to a 0-100 scale from a -160 to 0 scale`
- **L47260** [MW0LGE]: `                //[2.10.3.5]MW0LGE`
- **L47351** [MW0LGE]: `                sliderForm.RX1SquelchState = chkSquelch.CheckState; //[2.10.3.5]MW0LGE`
- **L47531** [MW0LGE]: `                sliderForm.RX2SquelchState = chkRX2Squelch.CheckState; //[2.10.3.5]MW0LGE`
- **L47568** [MW0LGE]: `                        //[2.10.3.5]MW0LGE convert to a 0-100 scale from a -160 to 0 scale`
- **L47580** [MW0LGE]: `                        //[2.10.3.5]MW0LGE reverted back to a -160 to 0 scale`
- **L47592** [MW0LGE]: `                    //[2.10.3.5]MW0LGE convert to a 0-100 scale from a -160 to 0 scale`
- **L47609** [MW0LGE]: `                //[2.10.3.5]MW0LGE`
- **L47631** [MW0LGE]: `            //[2.10.1.0] MW0LGE`
- **L47736** [MW0LGE, WD5Y]: `        //[2.10.1.0] MW0LGE code/idea from WD5Y`
- **L47989** [MW0LGE]: `        //[2.10.3.6]MW0LGE moved all this to functions to make it easier to diagnose issues`
- **L48034** [MW0LGE]: `        public double S9Frequency //[2.10.3.6]MW0LGE implements #418`
- **L48632** [MW0LGE]: `            //[2.10.3.6]MW0LGE now in submenu, however do this if shift held, or right click,`
- **L48656** [MW0LGE]: `            //[2.10.3.7]MW0LGE show dropdown above fixes issue where the popup does not show if the window`
- **L48899** [MW0LGE]: `                    bool bOverRX1 = overRX(e.X, e.Y, 1, false);  //MW0LGE`
- **L48902** [MW0LGE]: `                    //NOTCH MW0LGE`
- **L48975** [MW0LGE]: `                            m_nNotchRX = nRX; // MW0LGE_21e`
- **L48994** [MW0LGE]: `                    //MIDDLE OF PANAFALL MOVEUPDOWN MW0LGE`
- **L49129** [MW0LGE]: `                        if (bOverRX1 && agc_knee_drag) AutoAGCRX1 = false; // MW0LGE_21k8 turn of auto agc if we click knee`
- **L49141** [MW0LGE]: `                            bool panafall_check = Display.CurrentDisplayMode == DisplayMode.PANAFALL && ((!rx2_enabled && e.Y < Display.PanafallSplitBarPos) || (rx2_enabled && e.Y < pnlDisplay.Height / 4)); //[2.10.3.6]MW0LGE fixes issue where you could try to qsy click on the waterfall`
- **L49197** [MW0LGE]: `                                        bShift = false; // the shift is already applied to the vfo, we dont want to do it again ! MW0LGE_21k9rc6`
- **L49233** [MW0LGE]: `                                        bShift = false; // the shift is already applied to the vfo, we dont want to do it again ! MW0LGE_21k9rc6`
- **L49259** [MW0LGE]: `                                // MW0LGE block below handles dragging top frequency bars`
- **L49287** [MW0LGE]: `                                                if (!(Common.ShiftKeyDown && (e.X > low_x && e.X < high_x))) // ignore if shift down, so that we move the filter, and not the frequency MW0LGE_21k9d`
- **L49312** [MW0LGE]: `                                                if (!(Common.ShiftKeyDown && (e.X > low_x && e.X < high_x))) // ignore if shift down, so that we move the filter, and not the frequency MW0LGE_21k9d`
- **L49335** [MW0LGE]: `                                            if (!(Common.ShiftKeyDown && (e.X > low_x && e.X < high_x))) //MW0LGE_21k9d do not set freq, so we can shift the filter instead`
- **L49350** [MW0LGE]: `                                                // shift filter MW0LGE_21k9d`
- **L49364** [MW0LGE]: `                                            if (!(Common.ShiftKeyDown && (e.X > low_x && e.X < high_x))) //MW0LGE_21k9d do not set freq, so we can shift the filter instead`
- **L49383** [MW0LGE]: `                                                // shift filter MW0LGE_21k9d`
- **L49510** [MW0LGE]: `                                else if (e.X > low_x && e.X < high_x && Common.ShiftKeyDown) // need shift held to drag the filter in ctun off mode MW0LGE_21k9d`
- **L49652** [MW0LGE]: `                            if (removeNotch(SelectedNotch)) SelectedNotch = null; // remove the notch, and if ok clear selected MW0LGE`
- **L49663** [MW0LGE]: `                        if (Common.ShiftKeyDown) ChangeTuneStepDown(); //MW0LGE`
- **L49687** [MW0LGE]: `            Display.HighlightedBandStackEntryIndex = -1; //MW0LGE_21h`
- **L49715** [MW0LGE]: `                //MW0LGE_21h`
- **L49728** [MW0LGE]: `                        //MW0LGE_21h changes so that CTUN on works for filter drag                        `
- **L49737** [MW0LGE]: `                        if (_display_duplex) //[2.10.1.0] MW0LGE support duplex`
- **L49750** [MW0LGE]: `                        //MW0LGE_21h changes so that CTUN on works for filter drag`
- **L49791** [MW0LGE]: `                //NOTCH MW0LGE`
- **L49854** [MW0LGE]: `                        //MW0LGE_21e XVTR`
- **L49888** [MW0LGE]: `                                ChangeNotchCentreFrequency(SelectedNotch, SelectedNotch.FCenter, m_nNotchRX); //MW0LGE [2.9.0.7] update on drag`
- **L49910** [MW0LGE]: `                        //MW0LGE_21e XVTR`
- **L49972** [MW0LGE]: `                    //BandstackOverlay highlight MW0LGE_21h`
- **L50030** [MW0LGE]: `                //MIDDLE OF PANAFALL MOVEUPDOWN MW0LGE`
- **L50074** [MW0LGE]: `                                //MW0LGE`
- **L50082** [MW0LGE]: `                                //MW0LGE_21d set rx2 grid - change to shift key`
- **L50116** [MW0LGE]: `                                //MW0LGE`
- **L50123** [MW0LGE]: `                                //MW0LGE_21d set rx2 grid - changed to shift key`
- **L50171** [MW0LGE]: `                                //MW0LGE`
- **L50179** [MW0LGE]: `                                //MW0LGE_21d set rx1 grid - changed to shift key`
- **L50212** [MW0LGE]: `                                //MW0LGE`
- **L50219** [MW0LGE]: `                                //MW0LGE_21d set rx1 grid - changed to shift key`
- **L50242** [MW0LGE]: `                //MW0LGE_21k9`
- **L50317** [MW0LGE]: `                                        double size = (double)specRX.GetSpecRX(1).FFTSize; // MW0LGE_21k7`
- **L50318** [MW0LGE]: `                                        WDSP.SetRXAAGCThresh(WDSP.id(2, 0), agc_rx2_thresh_point, size/*4096.0*/, sample_rate_rx2); //MW0LGE_21k5 was sample_rate_rx1`
- **L50345** [MW0LGE]: `                                        if (agc_thresh_point < -160.0) agc_thresh_point = -160.0; //[2.10.3.6]MW0LGE changed from -143`
- **L50349** [MW0LGE]: `                                        double size = (double)specRX.GetSpecRX(0).FFTSize; // MW0LGE_21k7`
- **L50424** [MW0LGE]: `                        bool bOkToChangeRX1 = bOverRX1 && rx1_enabled && !rx1_click_tune_drag && !rx1_spectrum_drag && (_rx1_dsp_mode != DSPMode.DRM && _rx1_dsp_mode != DSPMode.SPEC && _rx1_dsp_mode != DSPMode.FM) && !(_mox && (VFOATX || (RX2Enabled && VFOSplit))); //[2.10.1.0] MW0LGE prevent hig...`
- **L50444** [MW0LGE]: `                                    //MW0LGE_21h`
- **L50472** [MW0LGE]: `                                //MW0LGE_21k9 added the filter info onto the cursor info, also done below on the filter drags`
- **L50678** [MW0LGE]: `                //re-implemented cursor info MW0LGE_21k9`
- **L50702** [MW0LGE]: `                        double localVFOfreq = Display.VFOB * 1e-6; //[2.10.1.0] MW0LGE change to use the display VFO as that is what we are considering`
- **L50735** [MW0LGE]: `                        double localVFOfreq = Display.VFOA * 1e-6; //[2.10.1.0] MW0LGE change to use the display VFO as that is what we are considering`
- **L50798** [MW0LGE]: `                    bool localMox = _mox && ((RX2Enabled && (!bRx2 && VFOATX) || (bRx2 && VFOBTX)) || !RX2Enabled); //[2.10.1.0] MW0LGE consider if we are over the RX that is in mox`
- **L50799** [MW0LGE]: `                    if ((localClickTuneDisplay && !localMox) || (localClickTuneDisplay && (_display_duplex && !bRx2)))    // Correct cursor frequency when CTUN on -G3OQD  // MW0LGE_21a also when in CTD and DUP //[2.10.1.0] MW0LGE ignore rx2 if dup`
- **L50925** [MW0LGE]: `                if ((!rx2_enabled && Display.CurrentDisplayMode == DisplayMode.PANAFALL) && m_bDraggingPanafallSplit) //MW0LGE_21k9c changes to this and below`
- **L50984** [MW0LGE]: `                        //MW0LGE_21i`
- **L51005** [MW0LGE]: `                //BandStack overlay MW0LGE_21h`
- **L51029** [MW0LGE]: `                    // finished dragging a notch, let use change its frequency MW0LGE`
- **L51067** [MW0LGE]: `                if (SelectedNotch != null && !Common.CtrlKeyDown) //MW0LGE_21f only if ctrl not down, as was randomly showing when adding a new one`
- **L51092** [MW0LGE]: `                //MW0LGE_21h`

### `Project Files/Source/Console/console.resx`

Total lines in file: 20607. Header block ends at
line 1 (first non-comment code line).
Inline markers found: **0**.

No body-level inline markers for any tracked callsign.

### `Project Files/Source/Console/display.cs`

Total lines in file: 12201. Header block ends at
line 48 (first non-comment code line).
Inline markers found: **72**.

Per-callsign counts: MW0LGE: 72

- **L249** [MW0LGE]: `        private static SharpDX.Direct2D1.Bitmap _waterfall_bmp_dx2d = null;					// MW0LGE`
- **L1972** [MW0LGE]: `        private static Pen sub_rx_zero_line_pen = new Pen(sub_rx_zero_line_color, 2); // MW0LGE width 2`
- **L2038** [MW0LGE]: `        private static Pen grid_zero_pen = new Pen(grid_zero_color, 2); // MW0LGE width 2`
- **L2054** [MW0LGE]: `        private static Pen tx_grid_zero_pen = new Pen(Color.FromArgb(255, tx_grid_zero_color), 2); //MW0LGE width 2`
- **L2135** [MW0LGE]: `        //MW0LGE`
- **L2272** [MW0LGE]: `        private static Pen cw_zero_pen = new Pen(Color.FromArgb(255, display_filter_color), 2); // MW0LGE width 2`
- **L2368** [MW0LGE]: `        private static Pen tx_filter_pen = new Pen(display_filter_tx_color, 2); // width 2 MW0LGE`
- **L2415** [MW0LGE]: `        //MW0LGE`
- **L3008** [MW0LGE]: `        //MW0LGE - these properties auto AGC on the waterfall, so that`
- **L3187** [MW0LGE]: `        // directx mw0lge`
- **L3306** [MW0LGE]: `        //[2.10.3.12]MW0LGE adaptor info`
- **L3410** [MW0LGE]: `                    DeviceCreationFlags debug = DeviceCreationFlags.None;//.Debug;  //MW0LGE_21k9 enabled debug to obtain list of objects that are still referenced`
- **L3692** [MW0LGE]: `                    //[2.10.1.0] MW0LGE spectrum/bitmaps may be cleared or bad, so wait to settle`
- **L4213** [MW0LGE]: `                    //MW0LGE_21k8`
- **L4391** [MW0LGE]: `            // MW0LGE_21k8 used to pre-init fps, before rundisplay has had time to recalculate, called from console mostly when adjusting fps in setup window`
- **L4829** [MW0LGE]: `                            fOffset += rx1_display_cal_offset; //[2.10.1.0] MW0LGE fix issue #137`
- **L4830** [MW0LGE]: `                            fOffset += tx_attenuator_offset; //[2.10.3.6]MW0LGE att_fix // change fixes #482`
- **L4972** [MW0LGE]: `            //if (grid_control) //[2.10.3.9]MW0LGE raw grid control option now just turns off the grid, all other elements are shown`
- **L5109** [MW0LGE]: `            //MW0LGE not used, as filling vertically with lines is faster than a filled very detailed`
- **L5824** [MW0LGE]: `        //            bool bElapsed = (_high_perf_timer.ElapsedMsec - _fLastFastAttackEnabledTimeRX1) > tmpDelay; //[2.10.1.0] MW0LGE change to time related, instead of frame related`
- **L5859** [MW0LGE]: `        //            bool bElapsed = (_high_perf_timer.ElapsedMsec - _fLastFastAttackEnabledTimeRX2) > tmpDelay; //[2.10.1.0] MW0LGE change to time related, instead of frame related`
- **L5868** [MW0LGE]: `            //[2.10.3.9]MW0LGE refactor to use refs, simplifies the code, removes unnecessary branching, general speed improvements`
- **L6700** [MW0LGE]: `                    //MW0LGE [2.9.0.7]`
- **L6727** [MW0LGE]: `                            local_max_y = max_copy; //[2.10.3.9]MW0LGE changed from max`
- **L6734** [MW0LGE]: `                            local_min_y_w3sz = max_copy; //[2.10.3]MW0LGE use unmodified, not the notced data`
- **L6852** [MW0LGE]: `                                    if (waterfall_minimum > dataCopy[i] + fOffset) //[2.10.3]MW0LGE use non notched data`
- **L6944** [MW0LGE]: `                                    if (waterfall_minimum > dataCopy[i] + fOffset) //[2.10.3]MW0LGE use non notched data`
- **L7026** [MW0LGE]: `                                    if (waterfall_minimum > dataCopy[i] + fOffset) //[2.10.3]MW0LGE use non notched data`
- **L7065** [MW0LGE]: `                                    if (waterfall_minimum > dataCopy[i] + fOffset) //[2.10.3]MW0LGE use non notched data`
- **L7274** [MW0LGE]: `                                    if (waterfall_minimum > dataCopy[i] + fOffset) //[2.10.3]MW0LGE use non notched data`
- **L7278** [MW0LGE]: `                                    //[2.10.3.5]MW0LGE note these are reverse RGB, we normally expect BGRA #289`
- **L7480** [MW0LGE]: `                                    if (waterfall_minimum > dataCopy[i] + fOffset) //[2.10.3]MW0LGE use non notched data`
- **L7483** [MW0LGE]: `                                    //[2.10.3.5]MW0LGE note these are reverse RGB, we normally expect BGRA #289`
- **L7689** [MW0LGE]: `                                    //[2.10.3.5]MW0LGE note these are reverse RGB, we normally expect BGRA #289`
- **L7817** [MW0LGE]: `            // MW0LGE now draw any grid/labels/scales over the top of waterfall`
- **L7818** [MW0LGE]: `            //if (grid_control_major)  //[2.10.3.9]MW0LGE`
- **L7874** [MW0LGE]: `            BitmapProperties bitmapProperties = new BitmapProperties(new SDXPixelFormat(Format.B8G8R8A8_UNorm, ALPHA_MODE)); //was R8G8B8A8_UNorm  //MW0LGE_21k9`
- **L7887** [MW0LGE]: `                //[2.10.3.13]MW0LGE fix issue where stride can be -ve for bottom up bitmaps`
- **L8316** [MW0LGE]: `            //MW0LGE_21k9rc6 just assign to null, but they are disposed of in clearAllDynamicBrushes`
- **L8339** [MW0LGE]: `            //MW0LGE_21k9rc6 just assign to null, but they are disposed of in clearAllDynamicBrushes`
- **L8541** [MW0LGE]: `            // MW0LGE`
- **L8681** [MW0LGE]: `                m_stringSizeCache.Remove(oldKey); // [2.10.1.0] MW0LGE dictionary is not ordered`
- **L8916** [MW0LGE]: `            // MW0LGE`
- **L8937** [MW0LGE]: `            //MW0LGE`
- **L9070** [MW0LGE]: `            //MW0LGE`
- **L9078** [MW0LGE]: `            //-- [2.10.1.0] MW0LGE fix for when split and dup is off. Note dupe always off for rx2`
- **L9186** [MW0LGE]: `            //MW0LGE_21k8 reworked`
- **L9187** [MW0LGE]: `            if ((rx == 1 && rx1_dsp_mode != DSPMode.CWL && rx1_dsp_mode != DSPMode.CWU) || (rx == 2 && rx2_dsp_mode != DSPMode.CWL && rx2_dsp_mode != DSPMode.CWU)) //MW0LGE [2.9.0.7] +rx2`
- **L9220** [MW0LGE]: `                    else // MW0LGE_21k8`
- **L9245** [MW0LGE]: `            // draw 60m channels if in view - not on the waterfall //MW0LGE`
- **L9254** [MW0LGE]: `                    if (bottom || (current_display_mode_bottom == DisplayMode.PANAFALL && rx == 2)) // MW0LGE`
- **L9297** [MW0LGE]: `                        //MW0LGE`
- **L9305** [MW0LGE]: `            //MW0LGE_21h`
- **L9333** [MW0LGE]: `                handleNotches(rx, bottom, cwSideToneShift, Low, High, nVerticalShift, top, width, W, H, true);//, 50); //MW0LGE [2.9.0.7] moved to function so can be used by drawmpana/drawwater                `
- **L9382** [MW0LGE]: `                    if (rx == 2 && !local_mox && (_rx2_enabled && _tx_on_vfob) &&  //MW0LGE [2.9.0.7] txonb`
- **L9398** [MW0LGE]: `            //      MW0LGE`
- **L9403** [MW0LGE]: `                if (!split_enabled) //MW0LGE_21k8`
- **L9419** [MW0LGE]: `                (bIsWaterfall && ((m_bShowRXZeroLineOnWaterfall & !local_mox) || (m_bShowTXZeroLineOnWaterfall & local_mox)))) // MW0LGE`
- **L9483** [MW0LGE]: `            for (int i = -1; i < f_steps + 1; i++) // MW0LGE was from i=0, fixes inbetweenies not drawn if major is < 0`
- **L9495** [MW0LGE]: `                    //MW0LGE`
- **L9667** [MW0LGE]: `                //MW0LGE`
- **L9686** [MW0LGE]: `                        //MW0LGE`
- **L9773** [MW0LGE]: `                        // MW0LGE`
- **L9872** [MW0LGE]: `                    { // only show filter if option set MW0LGE`
- **L9893** [MW0LGE]: `            // MW0LGE all the code for F/G/H overlay line/grab boxes`
- **L9896** [MW0LGE]: `                //MW0LGE include bottom check`
- **L9991** [MW0LGE]: `                            double size = (double)console.specRX.GetSpecRX(0).FFTSize;//MW0LGE_21k7`
- **L10063** [MW0LGE]: `                            double size = (double)console.specRX.GetSpecRX(1).FFTSize;//MW0LGE_21k7`
- **L10079** [MW0LGE]: `                                rx2_agc_hang_y = dBToRX2Pixel((float)rx2_hang + rx2_cal_offset, H);// + rx2_fft_size_offset);  MW0LGE   NOT IN RX1 WHY?  TODO CHECK`
- **L10279** [MW0LGE]: `                if (rx == 1) max += rx1_preamp_offset - alex_preamp_offset;   //MW0LGE_21 change to rx==1`
- **L10304** [MW0LGE]: `                    if (rx == 1) max += rx1_preamp_offset - alex_preamp_offset;  //MW0LGE_21 change to rx==1`
- **L11200** [MW0LGE]: `        ////MW0LGE_21b the new peak detect code, here as reference`

### `Project Files/Source/Console/dsp.cs`

Total lines in file: 1061. Header block ends at
line 40 (first non-comment code line).
Inline markers found: **6**.

Per-callsign counts: MW0LGE: 6

- **L828** [MW0LGE]: `        // WDSP impulse cache - MW0LGE`
- **L881** [MW0LGE]: `            AGC_PK, // MW0LGE [2.9.0.7] added pk + av + last`
- **L883** [MW0LGE]: `            CFC_AV, // MW0LGE [2.9.0.7] added av`
- **L959** [MW0LGE]: `	        case MeterType.ADC_REAL:  // MW0LGE [2.9.0.7] not sure how these are real + imaginary values, they are ADC peak, and ADC average, according to rxa.c and rxa.h`
- **L960** [MW0LGE]: `                val = GetRXAMeter(channel, rxaMeterType.RXA_ADC_PK); // input peak MW0LGE [2.9.0.7]`
- **L964** [MW0LGE]: `                val = GetRXAMeter(channel, rxaMeterType.RXA_ADC_AV); // input average MW0LGE [2.9.0.7]`

### `Project Files/Source/Console/enums.cs`

Total lines in file: 501. Header block ends at
line 42 (first non-comment code line).
Inline markers found: **8**.

Per-callsign counts: G8NJJ: 3, MI0BOT: 2, MW0LGE: 3

- **L125** [G8NJJ]: `        ANAN_G2,        //G8NJJ`
- **L126** [G8NJJ]: `        ANAN_G2_1K,     //G8NJJ`
- **L128** [MI0BOT]: `        HERMESLITE,     //MI0BOT`
- **L246** [MW0LGE]: `        SA_MINUS20,  //MW0LGE_21d`
- **L277** [MW0LGE]: `        //B2200M, //MW0LGE_21k8 need fix this`
- **L396** [MI0BOT]: `        HermesLite = 6,     // MI0BOT`
- **L397** [G8NJJ]: `        Saturn = 10,        // ANAN-G2: added G8NJJ`
- **L399** [MW0LGE]: `        Unknown = 999,      // MW0LGE`

### `Project Files/Source/Console/frmAddCustomRadio.Designer.cs`

Total lines in file: 200. Header block ends at
line 1 (first non-comment code line).
Inline markers found: **0**.

No body-level inline markers for any tracked callsign.

### `Project Files/Source/Console/frmAddCustomRadio.cs`

Total lines in file: 87. Header block ends at
line 42 (first non-comment code line).
Inline markers found: **0**.

No body-level inline markers for any tracked callsign.

### `Project Files/Source/Console/frmMeterDisplay.cs`

Total lines in file: 208. Header block ends at
line 41 (first non-comment code line).
Inline markers found: **1**.

Per-callsign counts: MW0LGE: 1

- **L188** [MW0LGE]: `            //IMPORTANT NOTE *****   //[2.10.3.7]MW0LGE`

### `Project Files/Source/Console/radio.cs`

Total lines in file: 4386. Header block ends at
line 44 (first non-comment code line).
Inline markers found: **9**.

Per-callsign counts: MW0LGE: 9

- **L82** [MW0LGE]: `            RadioDSP.DestroyDSP(); //[2.10.3] MW0LGE`
- **L105** [MW0LGE]: `            //check for old wdspWisdom00 file - [2.10.3.9]MW0LGE`
- **L1090** [MW0LGE]: `                        //[2.10.3.5]MW0LGE wave recorder volume normalise`
- **L1183** [MW0LGE]: `        // MW0LGE [2.9.0.8]`
- **L4242** [MW0LGE]: `        private static Object _listLock = new Object(); //MW0LGE_21k8`
- **L4243** [MW0LGE]: `        //MW0LGE return a notch that matches`
- **L4258** [MW0LGE]: `        //MW0LGE check if notch close by`
- **L4272** [MW0LGE]: `        //MW0LGE return list of notches in given bandwidth`
- **L4294** [MW0LGE]: `        //MW0LGE return first notch found that surrounds a given frequency in the given bandwidth        `

### `Project Files/Source/Console/rxa.cs`

Total lines in file: 201. Header block ends at
line 1 (first non-comment code line).
Inline markers found: **0**.

No body-level inline markers for any tracked callsign.

### `Project Files/Source/Console/setup.cs`

Total lines in file: 37400. Header block ends at
line 46 (first non-comment code line).
Inline markers found: **286**.

Per-callsign counts: G8NJJ: 20, MI0BOT: 2, MW0LGE: 263, W2PA: 2, W4WMT: 1, W5WC: 1

- **L134** [MW0LGE]: `            //[2.10.3.9]MW0LGE atempt to get the model as soon as possile, before the getoptions, so that everything that relies on it at least has a chance`
- **L147** [MW0LGE]: `            //MW0LGE_21i`
- **L153** [MW0LGE]: `            //MW0LGE_21d used for selective cancel`
- **L170** [MW0LGE]: `            // MW0LGE note: this will allways cause the change event to fire, as the combobox does not contain any default value`
- **L176** [MW0LGE]: `            labelSavingLoading.Visible = false;// MW0LGE gets shown/hidden by save/cancel/apply`
- **L193** [MW0LGE]: `            //MW0LGE_21d some defaults`
- **L225** [MW0LGE]: `            //MW0LGE_21g comboDisplayDriver.Text = "DirectX";`
- **L227** [MW0LGE]: `            //MW0LGE_21h`
- **L243** [MW0LGE]: `            //MW0LGE_21j`
- **L280** [MW0LGE]: `            //OC tab //MW0LGE_21j`
- **L296** [MW0LGE]: `            //MW0LGE_21k8 initialise these`
- **L303** [MW0LGE]: `            console.DeferUpdateDSP = true; //MW0LGE_21k9d updated below`
- **L380** [MW0LGE]: `            // some default tci server states MW0LGE_21k9d`
- **L398** [MW0LGE]: `            //MW0LGE_22b PA Profiles`
- **L418** [MW0LGE]: `            //MW0LGE [2.9.0.7] setup amp/volts calibration`
- **L423** [MW0LGE]: `            //MW0LGE_21j`
- **L576** [MW0LGE]: `            chkConsoleDarkModeTitleBar.Visible = Common.IsWindows10OrGreater(); //MW0LGE [2.9.0.8]`
- **L604** [MW0LGE]: `            initializing = true; //MW0LGE_21d stop the lg from notifying changed events`
- **L637** [MW0LGE]: `            console.DeferUpdateDSP = false;  //MW0LGE_21k9d`
- **L642** [MW0LGE]: `            btnRX2PBsnr.Enabled = console.RX2Enabled; //MW0LGE [2.9.0.7]`
- **L644** [MW0LGE]: `            //MW0LGE_21e`
- **L646** [MW0LGE]: `            updateAttenuationInfo(); //MW0LGE [2.10.3.9]`
- **L652** [MW0LGE]: `            //MW0LGE_21h`
- **L881** [MW0LGE]: `            //MW0LGE_21j // mirrors btnVAC1AdvancedDefault and btnVAC2AdvancedDefault`
- **L946** [MW0LGE]: `            // MW0LGE in the case where we don't have a setting in the db, this function (initdisplaytab) is called, use console instead`
- **L949** [MW0LGE]: `            if (needsRecovering(recoveryList, "comboDisplayThreadPriority")) comboDisplayThreadPriority.SelectedIndex = (int)console.DisplayThreadPriority; // MW0LGE`
- **L1046** [MW0LGE]: `            if (needsRecovering(recoveryList, "clrbtnDataFill")) clrbtnDataFill.Color = Display.DataFillColor;  //MW0LGE`
- **L1047** [MW0LGE]: `            if (needsRecovering(recoveryList, "chkDisablePicDisplayBackgroundImage")) chkDisablePicDisplayBackgroundImage.Checked = console.DisableBackgroundImage;  //MW0LGE`
- **L1048** [MW0LGE]: `            if (needsRecovering(recoveryList, "chkShowRXFilterOnWaterfall")) chkShowRXFilterOnWaterfall.Checked = Display.ShowRXFilterOnWaterfall; //MW0LGE`
- **L1049** [MW0LGE]: `            if (needsRecovering(recoveryList, "chkShowTXFilterOnWaterfall")) chkShowTXFilterOnWaterfall.Checked = Display.ShowTXFilterOnWaterfall; //MW0LGE`
- **L1050** [MW0LGE]: `            if (needsRecovering(recoveryList, "chkShowRXZeroLineOnWaterfall")) chkShowRXZeroLineOnWaterfall.Checked = Display.ShowRXZeroLineOnWaterfall; //MW0LGE`
- **L1051** [MW0LGE]: `            if (needsRecovering(recoveryList, "chkShowTXZeroLineOnWaterfall")) chkShowTXZeroLineOnWaterfall.Checked = Display.ShowTXZeroLineOnWaterfall; //MW0LGE`
- **L1052** [MW0LGE]: `            if (needsRecovering(recoveryList, "chkShowTXFilterOnRXWaterfall")) chkShowTXFilterOnRXWaterfall.Checked = Display.ShowTXFilterOnRXWaterfall; //MW0LGE`
- **L1096** [MW0LGE]: `            chkATTOnTX_CheckedChanged(this, e); //MW0LGE [2.10.3.6]`
- **L1098** [G8NJJ, MW0LGE]: `            //MW0LGE_21h to make G8NJJ_21h change in each function work`
- **L1106** [MW0LGE]: `            //MW0LGE [2.9.0.7]`
- **L1177** [MW0LGE]: `                    MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]);`
- **L1196** [MW0LGE]: `                    MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]);`
- **L1291** [MW0LGE]: `        //MW0LGE_21d`
- **L1311** [MW0LGE]: `            if (this.WindowState != FormWindowState.Normal) this.WindowState = FormWindowState.Normal; //MW0LGE_21i`
- **L1369** [MW0LGE]: `            // also remove any that make no sense to track/update MW0LGE_21j`
- **L1439** [MW0LGE]: `            //[2.6.10.3]MW0LGE this had been removed, and was spotted after I diffed older version. I put it here to keep a record`
- **L1529** [MW0LGE]: `            //MW0LGE_21a moved to dictionary to use same code as get options`
- **L1540** [MW0LGE]: `            //MW0LGE_21k8 converted to dictionary as everything will be unique`
- **L1584** [MW0LGE]: `            //a.Add("chkRadioProtocolSelect_checkstate", chkRadioProtocolSelect.CheckState.ToString()); //[2.10.3.5]MW0LGE not used anymore`
- **L1624** [MW0LGE]: `            // remove any outdated options from the DB MW0LGE_22b`
- **L1636** [MW0LGE]: `            if (needsRecovering(recoveryList, "comboTXTUNMeter")) comboTXTUNMeter.Text = "Fwd SWR"; // this needs to be setup in the case of new database and we havent hit tx/tune yet // MW0LGE_21a`
- **L1659** [MW0LGE]: `            if (getDict.ContainsKey("chkRadioProtocolSelect_checkstate")) //[2.10.3.5]MW0LGE this is no longer used`
- **L1708** [MW0LGE]: `            //MW0LGE_21d`
- **L1712** [MW0LGE]: `            //MW0LGE moved to dictionary, as control names are unique, and does away with old loop through every control to find the one we want`
- **L1717** [MW0LGE]: `            //MW0LGE_21k8 converted to dictionary`
- **L1720** [MW0LGE]: `            // deal with out dated settings //MW0LGE_22b`
- **L1727** [MW0LGE]: `            //[2.10.3.12]MW0LGE this is bad, because many radios have tabs removed, alex-2 for example, and in those cases`
- **L1744** [MW0LGE]: `            if (sortedList.Contains("comboPAProfile")) sortedList.Remove("comboPAProfile"); // this is done after the PA profiles are recovered // MW0LGE_22b`
- **L1781** [MW0LGE]: `            //[2.10.3.9]MW0LGE special check for radio model not being supported, default to HERMES`
- **L1801** [MW0LGE]: `                if (needsRecovering(recoveryList, name)) //MW0LGE_21d selective recovery`
- **L1887** [MW0LGE]: `                    else if (name == "UsbBCDSerialNumber") // [2.10.3.5]MW0LGE recover this as the usbbcd combo box will not have any entries at this point`
- **L1918** [MW0LGE]: `            //MW0LGE_22b`
- **L1978** [MW0LGE]: `            if (recoveryList == null) // MW0LGE [2.9.0.8] ignore if we hit cancel, not possible to undo multimeter changes at this time`
- **L2018** [MW0LGE]: `            //MW0LGE We have overwritten controls with data that might not match the current profile`
- **L2203** [MW0LGE]: `            chkCTUNignore0beat_CheckedChanged(this, e); //MW0LGE_21k9d`
- **L2240** [MW0LGE]: `            // MW0LGE_21h`
- **L2249** [MW0LGE]: `            // MW0LGE_21j`
- **L2304** [MW0LGE]: `            chkUsing10MHzRef_CheckedChanged(this, e); //MW0LGE_21k9rc6`
- **L2308** [MW0LGE]: `            udOptMaxFilterWidth_ValueChanged(this, e); //[2.10.3.9]MW0LGE`
- **L2309** [MW0LGE]: `            udOptMaxFilterShift_ValueChanged(this, e); //[2.10.3.9]MW0LGE`
- **L2310** [MW0LGE]: `            udFilterDefaultLowCut_ValueChanged(this, e); //MW0LGE_21d5`
- **L2318** [MW0LGE]: `            udTestIMDPower_ValueChanged(this, e); //MW0LGE_22b`
- **L2319** [MW0LGE]: `            setupTuneAnd2ToneRadios(); //MW0LGE_22b`
- **L2350** [MW0LGE]: `            chkStopRX1WaterfallOnTx_CheckedChanged(this, e); //[2.10.3.5]MW0LGE`
- **L2367** [MW0LGE]: `            clrbtnSliderLimitBar_Changed(this, e); //MW0LGE_22b`
- **L2373** [MW0LGE]: `            //MW0LGE_21d`
- **L2380** [MW0LGE]: `            //MW0LGE_21k`
- **L2400** [MW0LGE]: `            chkConsoleDarkModeTitleBar_CheckedChanged(this, e); //MW0LGE [2.9.0.8]`
- **L2402** [MW0LGE]: `            //collapsed display related items [2.10.3.6]MW0LGE`
- **L2409** [MW0LGE]: `            //[2.10.3.8]MW0LGE`
- **L2677** [MW0LGE]: `            //[2.10.1.0]MW0LGE`
- **L2743** [MW0LGE]: `            //MW0LGE_21d n1mm            `
- **L2827** [MW0LGE]: `            chkUsbBCD_CheckedChanged(this, e); //[2.10.3.5]MW0LGE`
- **L2830** [MW0LGE]: `            comboPAProfile_SelectedIndexChanged(this, e); //MW0LGE_22b`
- **L2832** [MW0LGE]: `            chkForceATTwhenPSAoff_CheckedChanged(this, e); //MW0LGE [2.9.0.7]`
- **L2980** [MW0LGE]: `            //[2.10.3.6]MW0LGE fix for case sensitive tx profile names. For example, if you name one`
- **L2985** [MW0LGE]: `            DataRow[] rows = DB.ds.Tables["TXProfile"].Select("Name = '" + profile.Replace("'", "''") + "'");//MW0LGE_21k9rc6 replace ' for ''`
- **L3199** [MW0LGE]: `                DataRow[] rows = getDataRowsForTXProfile(_current_profile);// DB.ds.Tables["TXProfile"].Select("Name = '" + current_profile.Replace("'", "''") + "'"); //MW0LGE_21k9rc6 replace ' for ''`
- **L3785** [MW0LGE]: `                updateTXProfileInDB(dr); //MW0LGE_21a remove duplication`
- **L3793** [MW0LGE]: `                //MW0LGE_21e`
- **L3803** [MW0LGE]: `                //MW0LGE_21e`
- **L3936** [MW0LGE]: `                        udHermesStepAttenuatorDataRX2_ValueChanged(this, EventArgs.Empty); //[2.10.3.6]MW0LGE no event will fire if the same, so force it`
- **L3969** [MI0BOT, MW0LGE]: `                    if (value < 0) value = 0; //MW0LGE [2.9.0.7] added after mi0bot source review`
- **L3971** [MW0LGE]: `                    if (udATTOnTX.Value == value) //[2.10.3.6]MW0LGE no event will fire if the same, so force it`
- **L4582** [MW0LGE]: `                if (udAudioVACGainTX != null) udAudioVACGainTX.Value = Math.Min(udAudioVACGainTX.Maximum, Math.Max(udAudioVACGainTX.Minimum, value));//[2.10.3.8]MW0LGE to fix HL2 issues introduced by linking mic gain to vac gain`
- **L4595** [MW0LGE]: `                if (udVAC2GainTX != null) udVAC2GainTX.Value = Math.Min(udVAC2GainTX.Maximum, Math.Max(udVAC2GainTX.Minimum, value));//[2.10.3.8]MW0LGE to fix HL2 issues introduced by linking mic gain to vac gain`
- **L5057** [MW0LGE]: `                    udDSPAGCHangTime_ValueChanged(this, EventArgs.Empty); // MW0LGE_21f moved above the decay`
- **L5075** [MW0LGE]: `                    udDSPAGCRX2HangTime_ValueChanged(this, EventArgs.Empty); // MW0LGE_21f moved above the decay`
- **L5692** [MW0LGE]: `                //[2.10.3.5]MW0LGE no code here, TODO`
- **L5834** [MW0LGE]: `                //[2.10.3.5]MW0LGE no code here, TODO`
- **L6029** [G8NJJ]: `        // added G8NJJ for Andromeda`
- **L6036** [G8NJJ]: `        // added G8NJJ for Aries`
- **L6043** [G8NJJ]: `        // added G8NJJ for Aries`
- **L6050** [G8NJJ]: `        // added G8NJJ for Ganymede`
- **L6057** [G8NJJ]: `        // added G8NJJ for Ganymede`
- **L6244** [G8NJJ]: `                else if (HardwareSpecific.Model == HPSDRModel.ANAN_G2_1K)            // G8NJJ. will need more work ofr high power PA`
- **L6457** [MW0LGE]: `                    MessageBoxIcon.Warning, MessageBoxDefaultButton.Button2, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]);`
- **L6489** [MW0LGE]: `            if (dr == DialogResult.No) return; //MW0LGE_[2.9.0.6] double check we want to do this, prevents accidental click from changing config`
- **L6560** [MW0LGE]: `                    MessageBoxIcon.Warning, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]);`
- **L6690** [MW0LGE]: `            //bool old_val = console.VACEnabled;  //MW0LGE_21i not used`
- **L6736** [MW0LGE]: `            //[2.10.3]MW0LGE only enable if wasapi`
- **L6737** [MW0LGE]: `            if (comboAudioDriver2.SelectedIndex < PA19.PA_GetHostApiCount()) //[2.10.3.4]MW0LGE ignore the manually added HPSDR (PCM A/D)`
- **L6787** [MW0LGE]: `            //[2.10.3]MW0LGE only enable if wasapi`
- **L6788** [MW0LGE]: `            if (comboAudioDriver3.SelectedIndex < PA19.PA_GetHostApiCount()) //[2.10.3.4]MW0LGE ignore the manually added HPSDR (PCM A/D)`
- **L6921** [MW0LGE]: `            //                    [NOTE: this had an impact on SendStopToMetis in networkproto1.c   (MW0LGE_21g comment)]`
- **L6990** [MW0LGE]: `            bool ok = Int32.TryParse(comboAudioSampleRate1.Text, out int new_rate); //[2.10.3.7]MW0LGE repleaced line below with this`
- **L7001** [MW0LGE]: `            bool was_enabled = console.RX1Enabled;  //... was set to RX2 for some reason, it should be RX1 which always true. MW0LGE_21a`
- **L7161** [MW0LGE]: `                console.InitFFTFillTime(1);//[2.10.1.0]MW0LGE`
- **L7180** [MW0LGE]: `            bool ok = Int32.TryParse(comboAudioSampleRateRX2.Text, out int new_rate); //[2.10.3.7]MW0LGE repleaced line below with this`
- **L7192** [MI0BOT, MW0LGE]: `            // MW0LGE [2.9.07] always initialise rx2 even if P1. Thanks to Reid (Gi8TME/Mi0BOT) and DH1KLM`
- **L7241** [MW0LGE]: `            console.InitFFTFillTime(2);//[2.10.1.0]MW0LGE`
- **L7259** [MW0LGE]: `            bool ok = Int32.TryParse(comboAudioSampleRate2.Text, out int new_rate); //[2.10.3.7]MW0LGE repleaced line below with this`
- **L7293** [MW0LGE]: `            bool ok = Int32.TryParse(comboAudioSampleRate3.Text, out int new_rate); //[2.10.3.7]MW0LGE repleaced line below with this`
- **L7733** [MW0LGE]: `            //[2.10.3.6]MW0LGE limit`
- **L7801** [MW0LGE]: `            console.WaterfallUseRX1SpectrumMinMax = chkWaterfallUseRX1SpectrumMinMax.Checked; // MW0LGE_21d this will force an update`
- **L7811** [MW0LGE]: `            //[2.10.3.6]MW0LGE limit`
- **L7878** [MW0LGE]: `            console.UpdateDisplayGridLevelMinValues(true); //MW0LGE  //MW0LGE_21e`
- **L7879** [MW0LGE]: `            console.WaterfallUseRX1SpectrumMinMax = chkWaterfallUseRX1SpectrumMinMax.Checked; // MW0LGE_21d this will force an update`
- **L7895** [MW0LGE]: `            //[2.10.3.6]MW0LGE limit`
- **L7963** [MW0LGE]: `            console.UpdateDisplayGridLevelMaxValues(false); //MW0LGE  //MW0LGE_21e`
- **L7964** [MW0LGE]: `            console.WaterfallUseRX2SpectrumMinMax = chkWaterfallUseRX2SpectrumMinMax.Checked; // MW0LGE_21d this will force an update`
- **L7974** [MW0LGE]: `            //[2.10.3.6]MW0LGE limit`
- **L8041** [MW0LGE]: `            console.WaterfallUseRX2SpectrumMinMax = chkWaterfallUseRX2SpectrumMinMax.Checked; // MW0LGE_21d this will force an update`
- **L8141** [MW0LGE]: `            //[2.10.3.6]MW0LGE limit`
- **L8213** [MW0LGE]: `            //[2.10.3.6]MW0LGE limit`
- **L8304** [MW0LGE]: `            //[2.10.3.6]MW0LGE limit`
- **L8376** [MW0LGE]: `            //[2.10.3.6]MW0LGE limit`
- **L8449** [MW0LGE]: `            //[2.10.3.5]MW0LGE changed the above to keep calcs internal to Audio`
- **L8802** [G8NJJ]: `                    // G8NJJ Saturn has QSK capability in any version.`
- **L8871** [MW0LGE]: `                    MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L8887** [MW0LGE]: `                        MessageBoxIcon.Hand, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L8909** [MW0LGE]: `                    MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]);`
- **L9258** [MW0LGE]: `            DataRow[] rows = getDataRowsForTXProfile(sProfileName);// DB.ds.Tables["TXProfile"].Select("Name = '" + sProfileName.Replace("'", "''") + "'"); //MW0LGE_21k9rc6 replace ' for ''            `
- **L9275** [MW0LGE]: `                //[2.10.3.9]MW0LGE added`
- **L9279** [MW0LGE]: `                // diable the vacs, so we can make changes without them trying to re-init etc MW0LGE_21dk5`
- **L9304** [MW0LGE]: `            //console.DXLevel = (int)dr["DXLevel"]; //MW0LGE_22b`
- **L9311** [MW0LGE]: `            console.MicMute = (bool)dr["MicMute"]; //MW0LGE_21f // NOTE: although called MicMute, true = mic in use`
- **L9398** [MW0LGE]: `            console.DeferUpdateDSP = true; //MW0LGE_21k9d`
- **L9422** [MW0LGE]: `            if (!initializing) console.DeferUpdateDSP = false;  //MW0LGE_21k9d  we dont want to undo this if we are initalising`
- **L9450** [MW0LGE]: `            //[2.10.3.2]MW0LGE console.EQForm.NumBands is set above, so just use RXeq to get the array, then adjust it`
- **L9480** [MW0LGE]: `            chkAudioEnableVAC.Checked = (bool)dr["VAC1_On"];    // moved here after setting to off MW0LGE_21k9d`
- **L9481** [MW0LGE]: `            chkAudioVACAutoEnable.Checked = (bool)dr["VAC1_Auto_On"]; //[2.10.1.0] MW0LGE moved here`
- **L9482** [MW0LGE]: `            chkVAC2Enable.Checked = (bool)dr["VAC2_On"];    // moved here after setting to off MW0LGE_21k9d`
- **L9483** [MW0LGE]: `            chkVAC2AutoEnable.Checked = (bool)dr["VAC2_Auto_On"]; //[2.10.1.0] MW0LGE moved here`
- **L9516** [MW0LGE]: `                if (comboTXProfileName.Focused && checkTXProfileChanged2()) //MW0LGE_21k8 swap the order so focus is needed before check profile is called`
- **L9597** [MW0LGE]: `                updateTXProfileInDB(dr); //MW0LGE_21a remove duplication`
- **L9637** [MW0LGE]: `            DataRow[] rows = getDataRowsForTXProfile(comboTXProfileName.Text);// DB.ds.Tables["TXProfile"].Select("Name = '" + comboTXProfileName.Text.Replace("'", "''") + "'"); //MW0LGE_21k9rc6 replace ' for ''`
- **L9701** [G8NJJ]: `            // G8NJJ: all logic moved to the console properties code`
- **L9708** [G8NJJ]: `            // G8NJJ: all logic moved to the console properties code`
- **L9715** [G8NJJ]: `            // G8NJJ: all logic moved to the console properties code`
- **L9719** [G8NJJ]: `        // G8NJJ: setup control to select an Andromeda top bar when display is collapsed`
- **L9725** [G8NJJ]: `            // G8NJJ: all logic moved to the console properties code`
- **L9729** [G8NJJ]: `        // G8NJJ: setup control to select an Andromeda top bar when display is collapsed`
- **L9735** [G8NJJ]: `            // G8NJJ: all logic moved to the console properties code`
- **L9864** [MW0LGE]: `            Display.DataLineColor = Color.FromArgb(tbDataLineAlpha.Value, clrbtnDataLine.Color); // MW0LGE_21b`
- **L10172** [W2PA]: `            //-W2PA MIDI wheel as VFO sensitivity adjustments`
- **L10176** [MW0LGE]: `            if (Common.GetComPortNumber(comboCATPort.Text, out int port)) //[2.10.3.9]MW0LGE`
- **L10182** [MW0LGE]: `            if (Common.GetComPortNumber(comboCATPTTPort.Text, out port)) //[2.10.3.9]MW0LGE`
- **L10202** [MW0LGE]: `            if (Common.GetComPortNumber(comboCAT2Port.Text, out port)) //[2.10.3.9]MW0LGE`
- **L10210** [MW0LGE]: `            if (Common.GetComPortNumber(comboCAT3Port.Text, out port)) //[2.10.3.9]MW0LGE`
- **L10218** [MW0LGE]: `            if (Common.GetComPortNumber(comboCAT4Port.Text, out port)) //[2.10.3.9]MW0LGE`
- **L10226** [MW0LGE]: `            if (Common.GetComPortNumber(comboAndromedaCATPort.Text, out port)) //[2.10.3.9]MW0LGE`
- **L10230** [MW0LGE]: `            if (Common.GetComPortNumber(comboAriesCATPort.Text, out port)) //[2.10.3.9]MW0LGE`
- **L10234** [MW0LGE]: `            if (Common.GetComPortNumber(comboGanymedeCATPort.Text, out port)) //[2.10.3.9]MW0LGE`
- **L10331** [MW0LGE]: `                    MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L10353** [MW0LGE]: `                        MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L10363** [MW0LGE]: `                        MessageBoxIcon.Information, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L10372** [MW0LGE]: `                        MessageBoxIcon.Information, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L10408** [MW0LGE]: `                    MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L10430** [MW0LGE]: `                        MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L10464** [MW0LGE]: `                    MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L10486** [MW0LGE]: `                        MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L10520** [MW0LGE]: `                    MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L10542** [MW0LGE]: `                        MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L10578** [MW0LGE]: `                    MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L10599** [MW0LGE]: `                        MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L10609** [MW0LGE]: `                        MessageBoxIcon.Information, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L10618** [MW0LGE]: `                        MessageBoxIcon.Information, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L10714** [MW0LGE]: `                        MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L10784** [MW0LGE]: `            if (Common.GetComPortNumber(comboCATPort.Text, out int port)) //[2.10.3.9]MW0LGE`
- **L10804** [MW0LGE]: `            if (Common.GetComPortNumber(comboCAT2Port.Text, out int port)) //[2.10.3.9]MW0LGE`
- **L10824** [MW0LGE]: `            if (Common.GetComPortNumber(comboCAT3Port.Text, out int port)) //[2.10.3.9]MW0LGE`
- **L10844** [MW0LGE]: `            if (Common.GetComPortNumber(comboCAT4Port.Text, out int port)) //[2.10.3.9]MW0LGE`
- **L10865** [MW0LGE]: `            if (Common.GetComPortNumber(comboAndromedaCATPort.Text, out int port)) //[2.10.3.9]MW0LGE`
- **L10888** [MW0LGE]: `                        MessageBoxIcon.Hand, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L10907** [MW0LGE]: `            if (Common.GetComPortNumber(comboCATPTTPort.Text, out int port)) //[2.10.3.9]MW0LGE`
- **L11049** [MW0LGE]: `                udFreq2Delay.Enabled = false; //MW0LGE_21`
- **L11076** [MW0LGE]: `                    await Task.Delay(200); //MW0LGE_21a`
- **L11101** [MW0LGE]: `                    //MW0LGE_21a change to delay Freq2 output. Fixes problems with some Amps frequency counters`
- **L11109** [MW0LGE]: `                //MW0LGE_22b`
- **L11110** [MW0LGE]: `                // remember old power //MW0LGE_22b`
- **L11124** [MW0LGE]: `                console.TwoTone = true; // MW0LGE_21a`
- **L11147** [MW0LGE]: `                chkTestIMD.Text = "Stop"; //MW0LGE_22b`
- **L11152** [MW0LGE]: `                await Task.Delay(200); //MW0LGE_21a`
- **L11155** [MW0LGE]: `                console.TwoTone = false; // MW0LGE_21a`
- **L11157** [MW0LGE]: `                //MW0LGE_22b`
- **L11367** [MW0LGE]: `            this.Hide(); //MW0LGE_21d deadlock potential`
- **L11389** [MW0LGE]: `            listClickedControls(); //MW0LGE_21d for testing`
- **L11414** [MW0LGE]: `            clearUpdatedClickControlList(); //MW0LGE_21d`
- **L11737** [MW0LGE]: `            udTXFilterLow_ValueChanged(sender, e); //[2.10.3.5]MW0LGE we want to validate even value doesnt change`
- **L11742** [MW0LGE]: `            udTXFilterHigh_ValueChanged(sender, e); //[2.10.3.5]MW0LGE we want to validate even value doesnt change`
- **L12071** [MW0LGE]: `            //MW0LGE_22b`
- **L12089** [MW0LGE]: `                //MW0LGE_22b`
- **L12203** [MW0LGE]: `                    case MeterTXMode.SWR_POWER: // MW0LGE re-added `
- **L12226** [MW0LGE]: `                case "Fwd SWR": //MW0LGE re-added`
- **L12354** [MW0LGE]: `            DataRow[] rows = DB.ds.Tables["TxProfileDef"].Select("Name = '" + name.Replace("'", "''") + "'"); //MW0LGE_21k9rc6 replace ' for ''`
- **L12409** [W2PA]: `        //-W2PA Export a single TX Profile to send to someone else for importing.`
- **L12420** [MW0LGE]: `            //[2.10.3.9]MW0LGE now requests file location, incrementing filename functionality removed`
- **L12448** [MW0LGE]: `            //[2.10.3.6]MW0LGE drop all non important table schema`
- **L12492** [MW0LGE]: `                        chkAutoPACalibrate.Visible = true; //MW0LGE_22b`
- **L12524** [MW0LGE]: `            if (initializing || _skinChanging) return;  // prevents call on getoptions, need to force call after options loaded MW0LGE `
- **L12620** [MW0LGE]: `                grpAlexAntCtrl.Enabled = true; //MW0LGE_21a`
- **L13986** [MW0LGE]: `        //MW0LGE_21g`
- **L14154** [W5WC]: `        private void comboFRSRegion_SelectedIndexChanged(object sender, EventArgs e) //w5wc`
- **L14237** [MW0LGE]: `                //MW0LGE_21d`
- **L14247** [MW0LGE]: `                setButtonState(true, false);  // MW0LGE this causes an update of the whole DB, so disable buttons while happening                                `
- **L15690** [MW0LGE]: `                updatePAProfileCombo(_sPA_PROFILE_BYPASS); //MW0LGE_22b`
- **L15696** [MW0LGE]: `                updatePAProfileCombo(_sAutoCailbratePAOldSelectedProfile); //MW0LGE_22b`
- **L15699** [MW0LGE]: `        //MW0LGE_22b`
- **L15742** [MW0LGE]: `            if (chk != null) // only if we click it //MW0LGE [2.9.0.6]`
- **L15773** [MW0LGE]: `            //MW0LGE_21f`
- **L15801** [MW0LGE]: `            if (chk != null) // only if we click it //MW0LGE [2.9.0.6]`
- **L15831** [MW0LGE]: `            //MW0LGE_21f`
- **L16160** [MW0LGE]: `            console.InitFFTFillTime(1);//[2.10.1.0]MW0LGE`
- **L16184** [MW0LGE]: `            console.InitFFTFillTime(2);//[2.10.1.0]MW0LGE`
- **L16361** [G8NJJ]: `            switch (HardwareSpecific.Model)              // G8NJJ will need more work for ANAN_G2_1K (1KW PA)`
- **L16428** [MW0LGE]: `            //Display.UpdateMNFminWidth(); //[2.10.3.4]MW0LGE`
- **L16982** [MW0LGE]: `            chkCTLimitDragToSpectral.Enabled = chkClickTuneDrag.Checked; //MW0LGE_21k9`
- **L17689** [MW0LGE]: `            // returns true if in middle of edit or add MW0LGE`
- **L17737** [MW0LGE]: `            SaveNotchesToDatabase(); // aligns notch db with what has actually happened MW0LGE`
- **L17840** [MW0LGE]: `                btnMNFEdit.Enabled = false; //MW0LGE`
- **L17848** [MW0LGE]: `            SaveNotchesToDatabase(); // aligns notch db with what has actually happened MW0LGE`
- **L17935** [MW0LGE]: `                btnMNFEdit.Enabled = true; //MW0LGE`
- **L17939** [MW0LGE]: `                // MW0LGE added because udMNFFreq could be set to some value even though no MNotchDB entries`
- **L17966** [MW0LGE]: `            UpdateNotchDisplay(); // sets max limits, and selects first notch if one exists MW0LGE`
- **L17995** [MW0LGE]: `            //[2.10.3.5]MW0LGE note: see updateNormalizePan() in specHPSDR as it only applies to pan detector type 2,3,4`
- **L18040** [MW0LGE]: `            //[2.10.3.5]MW0LGE note: see updateNormalizePan() in specHPSDR as it only applies to pan detector type 2,3,4`
- **L18111** [MW0LGE]: `            //[2.10.3.5]MW0LGE note: see updateNormalizePan() in specHPSDR as it only applies to pan detector type 2,3,4`
- **L18488** [MW0LGE]: `        //[2.10.3.9]MW0LGE this attribue, together with the app.config change 'legacyCorruptedStateExceptionsPolicy' enables`
- **L18513** [MW0LGE]: `            try //[2.10.3.7]MW0LGE this can fail if the vac is turned on/off. Mostly seen when TCI server does it via line_out command, and when ZZVA is used via CAT`
- **L18527** [MW0LGE]: `                // front end isplay of overflow/underflow MW0LGE_21k9rc5`
- **L18564** [MW0LGE]: `            try //[2.10.3.7]MW0LGE this can fail if the vac is turned on/off. Mostly seen when TCI server does it via line_out command, and when ZZVA is used via CAT`
- **L18578** [MW0LGE]: `                // front end isplay of overflow/underflow MW0LGE_21k9rc5`
- **L18615** [MW0LGE]: `            try //[2.10.3.7]MW0LGE this can fail if the vac is turned on/off. Mostly seen when TCI server does it via line_out command, and when ZZVA is used via CAT`
- **L18629** [MW0LGE]: `                // front end isplay of overflow/underflow MW0LGE_21k9rc5`
- **L18665** [MW0LGE]: `            try //[2.10.3.7]MW0LGE this can fail if the vac is turned on/off. Mostly seen when TCI server does it via line_out command, and when ZZVA is used via CAT`
- **L18679** [MW0LGE]: `                // front end isplay of overflow/underflow MW0LGE_21k9rc5`
- **L18759** [G8NJJ]: `            console.AlexANT2RXOnly = chkBlockTxAnt2.Checked; // G8NJJ_21h`
- **L18782** [G8NJJ]: `            console.AlexANT3RXOnly = chkBlockTxAnt3.Checked; // G8NJJ_21h`
- **L18839** [MW0LGE]: `            if (!lblLED01.Visible) //MW0LGE who cares if we cant see it?`
- **L19010** [MW0LGE]: `        ///MW0LGE`
- **L19014** [MW0LGE]: `            if (initializing) return; // ignore until we force call this after init MW0LGE`
- **L19212** [MW0LGE]: `            //MW0LGE_21d`
- **L19348** [MW0LGE]: `            console.UseAccurateFramingTiming = false;// chkAccurateFrameTiming.Checked; //[2.10.3.9]MW0LGE disabled as primarily for testing`
- **L19513** [MW0LGE]: `            if (Common.GetComPortNumber(comboGanymedeCATPort.Text, out int port)) //[2.10.3.9]MW0LGE`
- **L19531** [MW0LGE]: `            if (Common.GetComPortNumber(comboAriesCATPort.Text, out int port)) //[2.10.3.9]MW0LGE`
- **L19558** [MW0LGE]: `                    MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L19578** [MW0LGE]: `                        MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L19608** [MW0LGE]: `                    MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L19628** [MW0LGE]: `                        MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]`
- **L19814** [MW0LGE]: `            if (initializing) return; // forceallevents will call this  // [2.10.1.0] MW0LGE renabled`
- **L20202** [G8NJJ]: `                case HPSDRModel.ANAN_G2:                 // added G8NJJ`
- **L20253** [G8NJJ]: `                case HPSDRModel.ANAN_G2_1K:              // added G8NJJ`
- **L20420** [MW0LGE]: `            updatePAProfileCombo("Default - " + HardwareSpecific.Model.ToString()); //MW0LGE_22b`
- **L21104** [MW0LGE]: `                        //[2.10.3.9]MW0LGE show the syncs first`
- **L21143** [MW0LGE]: `            //[2,10.3.9]MW0LGE update the led mirror, as at this stage we know protocol`
- **L21157** [MW0LGE]: `        //MW0LGE_21h resampler advanced settings`
- **L22859** [MW0LGE]: `                    radUseFixedDrive2Tone.Checked = true; //MW0LGE_[2.9.0.7] fixed, was setting incorrect radio button`
- **L22909** [MW0LGE]: `            if (initializing) return; //[2.10.1.0] MW0LGE only want to apply this at the forceallevents stage`
- **L23732** [MW0LGE]: `            lblTXattBand.Text = newBand.ToString(); //[2.3.10.6]MW0LGE added (also in ATTOnTX)`
- **L23744** [MW0LGE]: `                _adjustingBand = Band.FIRST; // MW0LGE_[2.9.0.7] reset`
- **L26843** [MW0LGE]: `                // ignore some things [2.10.1.0] MW0LGE - fixes issue where bar with change units is paste into new bar, and source bars have no sub indicator`
- **L27047** [MW0LGE]: `        //[2.10.3.5]MW0LGE re-write, fixes #222`
- **L27183** [MW0LGE]: `        private bool _timerCheckingTXProfile = false; //[2.10.3.5]MW0LGE used to bypass changed events caused`
- **L28453** [W4WMT]: `        //[2.10.3.5]W4WMT implements #87`
- **L28459** [MW0LGE]: `        //[2.10.3.5]MW0LGE implements #306`
- **L29563** [MW0LGE]: `        // Code for the multimeter IO - MW0LGE [2.10.3.6]`
- **L32820** [MW0LGE]: `            chkDiscordTimeStamp.Visible = show; // only let mw0lge see this for testing purposes`
- **L34656** [MW0LGE]: `        //[2.10.3.12]MW0LGE this is a mirror the the rnnoise parse code from parse_lpcnet_weights.c`
- **L37368** [MW0LGE]: `            //[2.10.3.6]MW0LGE fixes #414, note might have issues when first running up, but should be ok after a profile save`

### `Project Files/Source/Console/setup.designer.cs`

Total lines in file: 76406. Header block ends at
line 1 (first non-comment code line).
Inline markers found: **0**.

No body-level inline markers for any tracked callsign.

### `Project Files/Source/Console/ucMeter.cs`

Total lines in file: 1238. Header block ends at
line 41 (first non-comment code line).
Inline markers found: **4**.

Per-callsign counts: MW0LGE: 4

- **L660** [MW0LGE]: `            if (!_resizing && (!pbGrab.ClientRectangle.Contains(pbGrab.PointToClient(Control.MousePosition)) || !this.ClientRectangle.Contains(this.PointToClient(Control.MousePosition)))) //[2.10.3.4]MW0LGE added 'this' incase we are totally outside, fix issue where ui items get left visible`
- **L1059** [MW0LGE]: `                if (tmp.Length >= 13)// && tmp.Length <= 21)  //[2.10.3.6_rc4] MW0LGE removed so that clients going forward can use older data as long as 13 entries exist`
- **L1188** [MW0LGE]: `            if (!_dragging && (!pnlBar.ClientRectangle.Contains(pnlBar.PointToClient(Control.MousePosition)) || !this.ClientRectangle.Contains(this.PointToClient(Control.MousePosition)))) //[2.10.3.4]MW0LGE added 'this' incase we are totally outside, fix issue where ui items get left visible`
- **L1200** [MW0LGE]: `            bool no_controls = _no_controls && !Common.ShiftKeyDown; //[2.10.3.6]MW0LGE no title or resize grabber, override by holding shift`

### `Project Files/Source/Console/ucRadioList.cs`

Total lines in file: 2002. Header block ends at
line 41 (first non-comment code line).
Inline markers found: **0**.

No body-level inline markers for any tracked callsign.

### `Project Files/Source/Console/xvtr.cs`

Total lines in file: 6035. Header block ends at
line 42 (first non-comment code line).
Inline markers found: **0**.

No body-level inline markers for any tracked callsign.

## Per-contributor summary

| Callsign | Name | Marker count | Files where cited inline (count per file) |
| --- | --- | --- | --- |
| MW0LGE | Richard Samphire | 1306 | console.cs (836); setup.cs (263); display.cs (72); DiversityForm.cs (29); PSForm.cs (21); MeterManager.cs (18); network.c (12); cmaster.cs (10); radio.cs (9); NetworkIO.cs (8); netInterface.c (6); networkproto1.c (6); dsp.cs (6); ucMeter.cs (4); enums.cs (3); ... +3 more |
| G8NJJ | Laurence Barker | 85 | console.cs (53); setup.cs (20); DiversityForm.cs (6); enums.cs (3); netInterface.c (1); network.h (1); clsHardwareSpecific.cs (1) |
| W2PA | Chris Codella | 49 | console.cs (46); setup.cs (2); PSForm.cs (1) |
| MI0BOT | Reid Campbell / Simon Brown | 14 | clsRadioDiscovery.cs [mi0bot] (9); enums.cs (2); setup.cs (2); network.h (1) |
| WD5Y | Joe | 8 | console.cs (8) |
| W5WC | Doug Wigley | 2 | NetworkIO.cs (1); setup.cs (1) |
| KD5TFD | Bill Tracey | 1 | NetworkIO.cs (1) |
| W4WMT | Bryan Rambo | 1 | setup.cs (1) |

## Notes for Pass 6b

### Very dense files (require block-granularity judgment)

- `console.cs` and `setup.cs` carry the majority of MW0LGE markers
  via `[version]` tags. Pass 6b should read each marker's surrounding
  10-30 lines to decide whether the NereusSDR port needs a matching
  inline comment or whether the behaviour lives inside a function
  already attributed at block level.
- `MeterManager.cs` is predominantly MW0LGE-authored; many markers
  are self-references inside Samphire's own code rather than
  third-party modification attributions. Pass 6b may elect to skip
  these unless the marker indicates a specific version gate or
  bug-fix tag the port must preserve.
- `display.cs` is a hybrid (FlexRadio + Wigley + VK6APH + MW0LGE);
  waterfall-AGC markers should be treated distinctly from MW0LGE's
  DirectX conversion markers.
- `clsRadioDiscovery.cs` [mi0bot] and `IoBoardHl2.cs` [mi0bot]:
  scanned from the mi0bot fork at `/Users/j.j.boyd/mi0bot-Thetis/`.
  MI0BOT markers inside these sources identify the HL2 fork-only
  code blocks; these must travel with the ported code.

### Header-block vs. body attributions

This index deliberately excludes top-of-file Copyright lines — Pass
5 already preserved those verbatim. A small number of body hits
(e.g. the long commented-out historical KD5TFD block in
`NetworkIO.cs`) are retained here because they appear below the
first non-comment code line and therefore pass the header-end
heuristic, even though semantically they are header relics.

### Scope inference is approximate

The scanner does not parse C# / C syntax. It emits marker locations
only. Pass 6b must open each cited file, read the surrounding code,
and decide:
- single-line vs. block-level scope
- whether the ported NereusSDR code retains the modified behaviour
- whether the inline comment survives the C# → C++/Qt translation
  intact or needs rewording to match the translated construct
