# Recording, playback, the voice keyer and the Digital Spectrum Recorder

Status: design, written 2026-11-15 when JJ said "write the spec", approved by JJ the same day. Built on branch
`claude/recording-voice-keyer-dsr` (draft PR 797); software and integration checks are in
[the evidence document](2026-11-15-recording-voice-keyer-dsr-evidence.md). Every hardware row is pending JJ.
Plan: [2026-11-15-recording-voice-keyer-dsr-plan.md](2026-11-15-recording-voice-keyer-dsr-plan.md); its
"Corrections to the spec, found while planning" table replaces this document where they differ.

Mockups: [2026-11-15-recording-voice-keyer-dsr-design/](2026-11-15-recording-voice-keyer-dsr-design/), one folder
per screen (section 12 lists them). Open each `index.html` in a browser. They are built on real captures of the
desktop, iPhone and iPad, and every one of them was approved by JJ by looking at it.

Read at `a241e848f` (branch `claude/brainstorm-dvk-rewind-4d7763`), Thetis v2.10.3.15 (`3759d09`), Longpath
`bb9f675c`, AetherSDR as studied on 2026-11-14.

## 1. Goal

An operator can:

1. **Record a contact in one tap** from the slice flag, on the desktop, iPhone or iPad, and play it back.
2. **Send any recording on the air** by keying while it plays, the way Thetis operators already do with Quick Play,
   but with the recording of their choice rather than only the last one.
3. **Send recorded voice messages** (M1 to M8) from a voice keyer, from the desktop or the phone.
4. **Go back in time on the pan.** The Core keeps the radio's signal on its disk for hours. Drag the waterfall back,
   tap any signal, and hear it, the way a DVR works for television. This is the **Digital Spectrum Recorder**.
5. **Keep and manage recordings** in one list shared by every computer and phone on the Core.

Everything is made and kept **on the Core**. The windows and phones show it and control it.

## 2. Veto list

Calls settled in this document without asking. Say the word and any of them changes.

| # | Call | Why |
|---|---|---|
| V1 | **Slices B to D each get their own Record and Play**, in the same flag column. One recording at a time per slice; several slices may record at once. | The flag is per slice; a contact can be on any slice. |
| V2 | **Contact recordings are 24 kHz, 16-bit.** MP3 at 64 kbit/s mono, 128 kbit/s when the two sides are kept apart (stereo). WAV is the same audio uncompressed. | AetherSDR records at 24 kHz; voice needs no more. These rates give the sizes the approved settings page shows (about 29 MB an hour MP3, 173 MB an hour WAV). |
| V3 | **Voice keyer messages are 48 kHz, 16-bit mono WAV, up to 2 minutes each.** | The transmit input runs at 48 kHz, so a message goes out with no resampling. Longpath caps at 30 s, which is short for a CQ with a pause. |
| V4 | **"Quiet" for the stop-by-itself rule** means: you have not transmitted, and the slice has heard nothing above its squelch level (or, with squelch off, nothing more than 6 dB above the noise floor) for the set number of minutes. | It needs a concrete rule to test. AetherSDR stops after 120 s idle; ours defaults to 2 minutes as approved. |
| V5 | **Keying picks up from where the recording is playing**, not from its start. If it ends while you are still keyed, your mic takes over. | Thetis keeps the same playback stream across MOX (clsAudioRecordPlayback.cs `GetPlayBuffer` switches the rate, not the position). |
| V6 | **Keying sends only what plays on the device you key from.** If the iPhone is playing and you key from the desktop, the desktop's mic goes out and the iPhone keeps playing to its own speaker. | Each device's playback is its own; the person keying is the one who chose what to send. Shown in the approved side-by-side mockup. |
| V7 | **The phase rotator joins EQ, compressor, leveler and CFC** in the "off while a recording goes on the air" switches, also off by default. | Thetis lists the phase rotator with the other four in its playback settings (clsAudioRecordPlayback.cs:270-283). |
| V8 | **F1 to F8 send voice messages in voice modes and CW messages in CW modes.** The existing defaults (Ctrl+F1 to F8 for CW, Ctrl+Shift+F1 to F4 for voice) stay as alternates, and the voice list grows from four messages to eight. | JJ chose F1 to F8 (AetherSDR's keys). The hot keys work owns the keys, so they can be changed in Setup. |
| V9 | **An older Core** that does not offer recordings leaves Record, Play, VOICE and the Digital Spectrum Recorder visible but disabled, saying "This Core needs an update for recordings." | Disabled, never hidden. |
| V10 | **MP3 playback** uses LAME's own decoder if each platform's LAME build includes it; if not, minimp3 (public domain). The plan checks this first. | One encoder library is already approved; decoding needs a source too. |

## 3. What JJ decided

| Topic | Decision |
|---|---|
| Record on the flag | One tap records the whole contact: what you hear on the slice, plus your own voice when you transmit. Kept on the Core. |
| Flag order | ×, lock, Record, Play, then ··· at the bottom, on desktop, iPhone and iPad. The desktop's ··· moves from under the lock to the bottom. |
| Desktop flag look | Round 20 pt buttons, the same as the lock. Record blinks red with the time beside it ("+ your voice" while transmitting). Play turns into a blue stop square with a label (frequency, call, time). |
| Phone flag look | Gray icons, no box, all inside the flag's height. Record blinks red with the time under it; Play turns into a blue stop square. A blue playing bar under the flag, with Others (last eight, All recordings…, Recording settings…) and Stop, shown only while playing. |
| Older recordings | Right-click Play on the desktop for the last eight, All recordings…, Show in Finder, Recording settings…. Others on the phone's playing bar. A small Recordings panel. A Recordings window. |
| Playback and the air | Play comes out of your own speaker. **Keying while a recording plays sends that recording on the air instead of your mic**, whichever recording you picked. Never keyed, only you hear it. |
| Processing on the air | When a recording goes on the air, EQ, compressor, leveler and CFC are off by default, each with its own switch to turn it back on. |
| Recording together | One recording per slice, on the Core. Every device's flag shows it at the same moment; any device can start or stop it. When it stops, each flag shows "Kept" with why it stopped and Play. |
| Start and stop by themselves | "Start by itself when I transmit" (off by default) and "Stop by itself when it goes quiet" (on, after 2 minutes). While armed, Record shows a hollow red ring on every device. |
| Recording settings | Setup > Cores > Recordings (desktop); Setup > Recording settings (phone and iPad). What the button keeps (your voice: on; the two sides together or apart), starting and stopping, files (MP3 default or WAV; names), where they are kept, playing back. |
| File names | `2026-11-15 1432Z 7.236.400 LSB K4ABC.mp3`: date, UTC time, frequency, mode, and the other station's call when the log knows it. |
| Recordings window | Opened from All recordings…. One sortable list of every kind, disk use, play, rename, delete several, Save a copy here, Show in Finder when the Core is on this Mac, Save sound only from a whole-pan stretch. |
| Recordings panel | A right-column applet (Containers > Applets): six latest, play per row, red row while recording, right-click Play / Rename / Save a copy here / Show in Finder / Delete, All recordings…, no settings of its own. |
| Voice keyer panel | Shaped like the CW memory panel: Record and Stop, M1 to M8 in a 2 by 4 grid, Repeat and its delay, mic level, right-click Listen / Rename / Load WAV / Save as WAV / Clear, and a visible Import WAV… that fills the first empty message. |
| Voice keyer sending | Clicking a message keys the radio by itself and returns to receive at the end. Stop, Escape, PTT or MOX cut it. No on/off setting. Messages are recorded from the mic before processing; processing runs on each send. |
| VOICE buttons | Desktop: a third button in the TX panel's TUNE / MOX row, opening M1 to M8, Repeat and "Voice keyer panel ›". Phone and iPad: a round VOICE button beside PTT opening the same. While sending it turns red and shows the message and progress. Dimmed with Take transmit when another station holds transmit. |
| Voice keys | F1 to F8 send M1 to M8 in voice modes; Escape stops. |
| Digital Spectrum Recorder | The Core keeps the pan's signal (receiver I/Q) on its disk in a loop with a size limit. On by default on a new Core, 10 GB on its own disk; stays off when the only disk is an SD card. A desktop window running its own radio starts with it off (JJ, 2026-11-16). |
| Rewind sound | Any signal on the pan for as far back as the disk holds. The slice itself plays the past: its frequency and speaker move to the clicked moment. Red LIVE returns to live audio at the frequency before rewinding. |
| Rewind picture | The waterfall moves with the sound (top row is the moment you hear). The spectrum shows the past too, with a PAST tag. The S-meter follows the moment played. The picture reaches back as far as the sound; past a window's own 20 minutes, the Core redraws it from the disk. |
| PTT while rewound | Takes slice A live on the frequency being played, to call the station you rewound to. LIVE still returns to the frequency before rewinding. |
| Moving through hours | A map of kept hours beside the time strip (newest at the top, shaded by band activity, a white mark where you are; click or drag to jump). Click a strip time to type a time. The strip drag stays fine (20 minutes per height). "Drawing" dims the picture while the Core redraws. |
| Your transmit in the recorder | Kept, so a replayed or saved contact has both sides. Those waterfall rows are marked as your transmit. |
| Saving a stretch | SAVE above the hours map; a yellow bracket (10 minutes by default; drag its ends or middle); a box with times, length, sizes and "Save stretch (whole pan)" / "Save sound only (MP3)" / Cancel. A whole-pan stretch replays like rewind, any station. |
| Recorder Setup | Setup > Cores > Digital Spectrum Recorder: on/off and status; disk, size limit, hours it holds, in-use bar with a moving write head and a red "erased next" zone; a plain "It is always erasing" note; saved stretches are never overwritten; erase what is kept now. A strong red SD card warning: connect a USB SSD or NVMe drive (thumb drives are not enough). |
| MP3 | JJ asked for MP3. It needs one new library, LAME (LGPL), on the Core only. |
| iPad | The phone's design on the pan (flag column, playing bar, VOICE beside PTT), plus an optional Recordings panel in the right column. Tools, Recordings and Recording settings are the phone's pages, wider. |

## 4. How the references do it

| Topic | Thetis v2.10.3.15 | AetherSDR | Longpath | Ours |
|---|---|---|---|---|
| Quick record | Quick Rec / Quick Play: one file, `SDRQuickAudio.wav`, overwritten each time (console.cs:36831-36865). Record and Play lock each other out. | Round ⏺ and ▶ on the flag; records slice audio plus your transmit into one 24 kHz WAV named date_timeZ_freq_mode_call. | Removed the flag buttons. QSO recorder applet: other station left, you right, one stereo file. | Flag ⏺ and ▶ on every device; a list of recordings, not one file; together or apart as a setting. |
| Playback and MOX | Playback feeds the transmitter while MOX is on (clsAudioRecordPlayback.cs `GetPlayBuffer`, ~3990). "Playback causes MOX" defaults on (`MoxOnPlayback = true`, :218). | Speaker only; mutes live receive. | Playback to transmit unfinished ("keying belongs in MoxController"). | Speaker until you key; keying sends what is playing, any recording. |
| Processing on playback | Per-item switches turn off TX EQ, compressor, CFC, phase rotator and leveler while playing (:270-283, :540-545). | n/a | n/a | Same five switches, all off by default. |
| Voice keyer | Voice record/play container item, 8 slots by default (up to 64), hold for repeat. | The Flex radio's own DVK, 12 slots, F1 to F12. | Voice keyer applet, 10 slots, mic before processing, 30 s cap. | 8 messages, CW-memory shape, mic before processing, keys by itself. |
| Going back in time | None. | None. | Recording applet records I/Q and plays it as a fake radio. | Digital Spectrum Recorder: a disk loop of I/Q on the Core, rewind on the pan itself. |

## 5. What exists today

* Flag ⏺ and ▶ already exist, hidden: `VfoWidget::m_recBtn` and `m_playBtn`, built in `buildFloatingButtons()`
  (src/gui/widgets/VfoWidget.cpp ~3832-3858) and kept out of the stack by `UnbuiltFeature::Voice`. Their order is
  close, lock, ···, record, play (`positionFloatingButtons()`), which this design changes.
* `DvkApplet` (src/gui/applets/DvkApplet.h): 4 slots, unwired, construction commented out in MainWindow. It is
  replaced by the voice keyer panel below.
* Hot keys: `voice.msg1` to `voice.msg4` (Ctrl+Shift+F1 to F4) and `voice.stop` (Ctrl+Shift+F10) in
  src/core/hotkeys/HotKeyActionList.cpp:106-110, refused while `UnbuiltFeature::Voice` is unbuilt.
* Waterfall rewind: the time strip and red LIVE exist. The window keeps colored rows only (default 20 minutes,
  `DisplayWaterfallHistoryMs`, capped at 16384 rows). No I/Q is kept anywhere.
* Receiver I/Q forks to the spectrum and TCI in `RadioModel::forkIqToTaps` (src/models/RadioModel.cpp:40910).
* Transmit input: `CompositeTxMicRouter` (src/core/audio/CompositeTxMicRouter.h) chooses the mic source. TX
  processing switches live in `TxChannel` (`SetTXAEQRun`, `SetTXACompressorRun`, `SetTXACFCOMPRun`,
  `SetTXAPHROTRun`, the leveler).
* Station capabilities carry a version per feature (src/core/session/StationCapabilities.h).
* No recorder, no audio file writer for recordings, no MP3 library.

## 6. Design

### 6.1 Parts

On the **Core**:

| Part | Does |
|---|---|
| **Recording store** | One folder of recordings on the chosen disk, with a small index (kind, slice, start, length, frequency, mode, call, name, size). Names, renames, deletes, reports disk use. Never deletes by itself. |
| **Contact recorder** | One per slice. Takes the slice's receive audio after demodulation and, while transmitting, the transmitted audio (as sent, after processing), at 24 kHz. Mixes them (together) or puts them left and right (apart). Writes WAV, or MP3 through LAME. Handles the start-by-itself and stop-by-itself rules. |
| **Player** | Plays one recording at a time for each client, sending the audio down that client's existing receive-audio path. Knows each client's position. |
| **On-air source** | A new source in `CompositeTxMicRouter`: while a client that is playing keys the radio, the transmit input comes from that client's playback instead of its mic, from the current position (V5, V6). Applies the five processing switches for as long as it is on the air, then restores them. |
| **Voice keyer** | Stores M1 to M8 per station (48 kHz WAV, V3), records them from the client's mic stream before processing, sends one on request through the same on-air source with normal processing, keys and unkeys, repeats with its delay. |
| **Spectrum recorder (DSR)** | Writes each active receiver's I/Q, as 16-bit, to one ring file on the chosen disk, with an index of times and frequencies and marks for your transmit. Overwrites the oldest at the size limit. Serves past audio, past spectrum and past waterfall rows on request. Copies a bracketed stretch out as a saved whole-pan recording. |

On each **client** (desktop window, iPhone, iPad): the flag buttons, the playing bar or label, the Recordings panel
and window or pages, Recording settings, the voice keyer panel and VOICE button, the DSR strip, hours map and
SAVE. Clients keep no recordings; "Save a copy here" downloads one.

Threads: the recorders and the DSR writer run on a new Core worker thread fed by queued signals from the audio
and connection threads. Nothing blocks the audio callback; disk writes are buffered on that worker.

### 6.2 Recording a contact

1. Record on any flag sends "start recording slice A" to the Core. The Core starts the contact recorder and tells
   every client; every flag blinks with the same running time.
2. While you transmit, your sent audio goes in too. The desktop label adds "+ your voice".
3. Record again, on any device, stops it. Each flag shows a "Kept" notice under it for a few seconds: the name,
   length, why it stopped ("Stopped on the iPhone", "Stopped by itself after two quiet minutes"), and Play (plus
   All recordings… on the desktop).
4. **Start by itself when I transmit**: while on, every Record shows a hollow red ring ("Waits for you to transmit"
   on the desktop, "AUTO" on the phone). Keying the transmit slice starts a recording on that slice. Tapping Record
   while armed starts one at once.
5. **Stop by itself when it goes quiet**: the rule in V4, default 2 minutes, settable 1 to 60.

### 6.3 Playing back, and sending it on the air

* Play on a flag plays the slice's latest recording; Others or right-click picks one of the last eight; any row in
  the panel, window or phone pages plays that one. A whole-pan stretch opens on the waterfall instead, the way
  rewind plays the past ("Play its sound only" plays just its sound).
* Playback goes to the speaker of the device that pressed Play, only. Other devices stay quiet (V6).
* **Keying from that device while it plays** sends the recording on the air instead of its mic (section 3). The
  desktop label turns red with ON AIR and the Play ring turns red; the phone and iPad playing bar turns red and
  reads "ON AIR · 0:12 / 3:25". Unkeying returns to the speaker and keeps playing.
* While a recording is on the air, the processing switches in Recording settings apply (all off by default: EQ,
  compressor, leveler, CFC, phase rotator). They return to your normal settings when it stops going out.
* The voice keyer's messages are not affected by these switches; they always get your normal processing.
* A voice keyer message's level on the air: one imported from a file goes out at the fixed recording level (the
  same 0 dB a recording uses), whatever the mic gain or mute; one recorded on the mic goes out at the mic gain,
  like your live voice. Recording, importing or clearing a message sets this afresh (JJ, 2026-11-16).

### 6.4 Recordings

* **Kinds**: Contact (from the flag), Whole pan (a saved DSR stretch, I/Q), Sound (sound only saved from the DSR or
  from a whole-pan stretch).
* **Names**: `YYYY-MM-DD HHMMZ <frequency> <mode> <call>.<mp3|wav>`, call only when the log knows it. A rename
  changes the shown name and the file name; every device sees it.
* **Recordings panel** (desktop applet, iPad column): as approved, section 3.
* **Recordings window** (desktop): "On the Core" totals (saved recordings, whole pan vs sound, free space; the DSR
  loop not counted), filter All / Contact / Whole pan / Sound, search, sortable table (play, when, kind, frequency,
  mode, call, name, length, size), Play, Rename, Save sound only (whole pan), Save a copy here, Show in Finder
  (Core on this computer only; disabled with the reason otherwise), Delete several with a confirm.
* **Phone and iPad**: Tools > Recordings (Core chip): filter, search, sort, a round more button per row, swipe
  left, Select for several, share sheet with a warning on big whole-pan copies.
* Delete always asks and says it cannot be undone.

### 6.5 Recording settings

Core-owned (every device sees the same), in Setup > Cores > Recordings on the desktop (under the Digital Spectrum
Recorder) and Setup > Recording settings on the phone and iPad:

| Group | Settings | Default |
|---|---|---|
| What the Record button keeps | What you hear on the slice (always); your own voice when you transmit; the two sides together or apart | on; together |
| Starting and stopping | Start by itself when I transmit; stop by itself when it goes quiet; quiet for | off; on; 2 min |
| Files | Save as MP3 or WAV (with size per hour); name example | MP3 |
| Where they are kept | Disk (choice), count and space used, Recordings ›. An SD card is allowed for these, with a note that they do not wear it like the recorder would. | the Core's own disk |
| Playing back | Keying while a recording plays sends it on the air. Use while it goes out: EQ, compressor, leveler, CFC, phase rotator | each off |

The same Files choice applies to "Save sound only" from the DSR. Not connected: every group dims, with a note.

Keys (Core AppSettings, PascalCase, booleans `"True"` / `"False"`): `RecordingsKeepMyVoice`,
`RecordingsSidesApart`, `RecordingsAutoStartOnTransmit`, `RecordingsAutoStopWhenQuiet`, `RecordingsQuietMinutes`,
`RecordingsFormat` (`Mp3` / `Wav`), `RecordingsDisk`, `RecordingsOnAirUseTxEq`, `RecordingsOnAirUseCompressor`,
`RecordingsOnAirUseLeveler`, `RecordingsOnAirUseCfc`, `RecordingsOnAirUsePhaseRotator`.

### 6.6 Voice keyer

* **Desktop panel** (right column, Containers > Applets; replaces `DvkApplet`): shape A as approved. Record lights;
  click a message to record into it; Stop ends. Right-click Listen (speaker only), Rename, Load WAV…, Save as
  WAV…, Clear. Import WAV… fills the first empty message.
* **Desktop VOICE**: third button in the TUNE / MOX row. Click opens a box just above the row: M1 to M8, Repeat,
  "Voice keyer panel ›". Sending closes it; VOICE turns red with "M2 0:01" and fills left to right; between repeats
  "M2 again in 5 s". VOICE, Escape or MOX stops. An empty message opens the panel.
* **Phone and iPad**: Tools > Voice Keyer (Core chip), like the phone CW page: a Record card (record into M#, phone
  mic level), M1 to M8 rows (tap sends; ··· for Send, Listen, Record again, Rename, Import WAV, Save as WAV, Clear),
  Import WAV…, Repeat and wait. VOICE beside PTT opens M1 to M8 over the pan; sending shows the number and a red
  progress ring; tap it or PTT to stop.
* **Keys**: F1 to F8 (V8); Escape stops.
* **Transmit held elsewhere**: sending dims, with Take transmit.
* Hot keys `voice.msg5` to `voice.msg8` are added; the unbuilt gate on `voice.*` lifts when this ships.

### 6.7 Digital Spectrum Recorder

* **Writing**: every active receiver's I/Q at its own rate, 16-bit, into one ring file. About 2.8, 5.5, 11 and 22
  GB per hour per receiver at 192, 384, 768 and 1536 kHz. At the size limit each new minute erases the oldest.
  Your transmitted audio is kept beside it, and those moments are marked.
* **Disk**: chosen in Setup; default the Core's own disk at 10 GB, on by default. If the only disk is an SD card it
  stays off and Setup says why, in red, and asks for a USB SSD or NVMe drive.
* **Rewind** (desktop, phone, iPad): drag the time strip; tap or click a signal; the slice moves to that frequency
  and plays from that moment; the waterfall scrolls with the sound; the spectrum shows the past with PAST; the
  S-meter follows. LIVE returns. PTT while rewound takes slice A live on the frequency being played.
* **This changes the core receive path.** While rewound, the slice's demodulator is fed from the ring file instead
  of the radio. It is the one place this design touches I/Q → WDSP → audio, and it is flagged for review in the
  plan. Live receivers and other slices keep running from the radio.
* **Hours map, typed time, drawing** as in section 3. Beyond a window's own 20 minutes, the Core draws the
  requested rows from the ring file with the pan's own FFT settings and sends them.
* **SAVE**: the bracket and box as in section 3. Whole pan copies the stretch out of the loop into the Recording
  store (never overwritten). Sound only demodulates slice A over the stretch into MP3 or WAV.
* **Setup page**: as in section 3. Keys: `DsrEnabled`, `DsrDisk`, `DsrSizeLimitGb` (default 10).

### 6.8 Phone and iPad

As in section 3 and the mockups. The phone shows slice A's flag with the column ×, lock, Record, Play, ···; the
playing bar under the flag; VOICE beside PTT; Tools > Recordings and Tools > Voice Keyer; Setup > Recording
settings and Setup > Digital Spectrum Recorder; the DSR strip, hours map, LIVE, SAVE and the clock on the pan. The
iPad uses the same, plus the optional Recordings panel in its right column (it pushes TX down; the column
scrolls). Away from home over the rendezvous server it all works the same; playback streams as receive audio
does today.

### 6.9 Station protocol

A new capability, `recordingsVersion` (1), in `StationCapabilities`. With it:

| Message | Direction | Carries |
|---|---|---|
| recording state | Core → all clients | per slice: idle, armed, recording (start time, who started it); a "kept" event with name, length, reason |
| record start / stop | client → Core | slice |
| recordings list, changed | Core → client | the index (paged), and changes |
| play / stop / seek | client → Core | recording id, position |
| rename / delete | client → Core | recording id, new name |
| copy download | client → Core, Core → client | recording id; the file in chunks over the data channel |
| recording settings | both ways | the keys in 6.5 |
| voice keyer state, messages, record, send, stop, import | both ways | M1 to M8, the clip audio for record and import |
| DSR state, past rows, past spectrum, rewind play, save stretch | both ways | times, frequencies, the bracket |

Without the capability, see V9. The station protocol version, settings schema and product version stay
independent, as CLAUDE.md requires.

## 7. Errors

| Case | What the operator sees |
|---|---|
| Disk full while recording | The recording stops and is kept up to that point; the flag says "Stopped: the disk is full". |
| DSR disk gone (unplugged) | The recorder stops; the strip shows only the window's own 20 minutes; Setup says the disk is missing. |
| MP3 encoder fails | The recording is kept as WAV; a note says so. |
| Link to the Core drops mid-recording | The Core keeps recording; the client shows it again on reconnect. |
| Another station has transmit | Voice keyer sends and on-air playback dim, with Take transmit. Recording itself is unaffected. |
| Not connected | Everything stays in place, dimmed, with a note. |
| Errors in the log | `qCWarning` under a new `lcRecordings` category on the Core. |

## 8. Acceptance

Evidence: **S** software tests on this machine, **I** integration (station loopback tests or the UI captures),
**H** on JJ's radios.

| # | Case | Evidence |
|---|---|---|
| A1 | Record on the desktop flag starts the Core recorder; the phone's flag shows it within one second; Stop on the phone stops it; both show Kept. | I, H |
| A2 | Your transmitted voice is in the recording; apart puts them left and you right. | S, H |
| A3 | Start by itself when I transmit starts on keying; stop by itself stops after the quiet rule (V4) and re-arms. | S |
| A4 | Names follow 6.4, including the call from the log; rename and delete reach every device. | S, I |
| A5 | Play sounds only on the pressing device. | I, H |
| A6 | Keying from the playing device sends the recording on the air from its position; the other device keying sends its mic; ending while keyed hands over to the mic. | S, H |
| A7 | While a recording goes out, the five processing switches apply and restore afterward. | S |
| A8 | Voice keyer: record M1 from the desktop mic and the phone mic; send keys and unkeys; Repeat; Stop / Escape / PTT / MOX cut it; F1 to F8 in voice modes. | S, H |
| A9 | Import WAV fills the first empty message; Save as WAV round-trips. | S |
| A10 | DSR writes, wraps at the limit, and serves past audio at the right time and frequency. | S |
| A11 | Rewind on the desktop and phone: tapping a past signal plays it on slice A; LIVE returns to the old frequency; PTT takes the played frequency live. | I, H |
| A12 | Hours beyond 20 minutes redraw from the disk; the hours map jumps. | I, H |
| A13 | SAVE whole pan and sound only produce the right lengths and kinds; whole pan replays like rewind. | S, H |
| A14 | SD-card-only Core: DSR stays off with the red warning; recordings still work. | I, H (Pi) |
| A15 | Older Core: every new control is visible, disabled, with the reason (V9). | I |
| A16 | UI captures for the flag (idle, recording, armed, playing, on air) on desktop, iPhone and iPad match the mockups. | I |
| A17 | Nothing in the audio callback blocks on disk (timing check on the Core under a full DSR load). | S |

## 9. Build order

One plan, landed in this order, each part usable on its own:

1. **Recordings on the Core, desktop first**: store, contact recorder, player, LAME, flag Record and Play,
   Recordings panel and window, Recording settings.
2. **On the air**: the on-air source and the processing switches.
3. **Voice keyer**: Core store and sender, desktop panel, VOICE, F1 to F8.
4. **Phone and iPad** for parts 1 to 3.
5. **Digital Spectrum Recorder on the Core and desktop**: ring file, rewind sound, past spectrum and rows, hours
   map, SAVE, Setup.
6. **Digital Spectrum Recorder on the phone and iPad.**

Features run on JJ's hardware before they merge.

## 10. New dependency

**LAME** (libmp3lame, LGPL) for MP3, on the Core only (clients never encode). JJ's request for MP3 is the go-ahead;
it is listed here because CLAUDE.md asks for dependencies to be flagged. The plan adds it to the Core's build on
Windows, macOS, Linux and the Pi, and to the third-party notices. See V10 for decoding.

## 11. Out of scope

* I/Q recording of anything other than the DSR loop and saved stretches; scheduled recordings.
* The Thetis voice record/play meter item (`VoiceRecordPlayItem`) stays hidden.
* Roundtable. (The same brainstorm chose the strip under the spectrum for the desktop Roundtable card; that belongs
  to the Roundtable overlay work, docs/architecture/2026-11-03-roundtable-overlay-design.md.)

## 12. Mockups

| Folder | Shows |
|---|---|
| [desktop-flag](2026-11-15-recording-voice-keyer-dsr-design/desktop-flag/index.html) | Desktop flag Record and Play, right-click list |
| [recording-together](2026-11-15-recording-voice-keyer-dsr-design/recording-together/index.html) | Desktop and iPhone together, Kept, start by itself, ON AIR |
| [recordings-panel](2026-11-15-recording-voice-keyer-dsr-design/recordings-panel/index.html) | Small Recordings panel |
| [recordings-window](2026-11-15-recording-voice-keyer-dsr-design/recordings-window/index.html) | Recordings window |
| [recording-settings](2026-11-15-recording-voice-keyer-dsr-design/recording-settings/index.html) | Desktop Recording settings |
| [voice-keyer-panel](2026-11-15-recording-voice-keyer-dsr-design/voice-keyer-panel/index.html) | Voice keyer panel shapes (A chosen) |
| [desktop-voice-button](2026-11-15-recording-voice-keyer-dsr-design/desktop-voice-button/index.html) | VOICE beside TUNE and MOX |
| [dsr-rewind](2026-11-15-recording-voice-keyer-dsr-design/dsr-rewind/index.html), [hours](2026-11-15-recording-voice-keyer-dsr-design/dsr-rewind/hours.html) | Rewind with sound; hours map and SAVE |
| [dsr-setup](2026-11-15-recording-voice-keyer-dsr-design/dsr-setup/index.html) | Digital Spectrum Recorder Setup |
| [phone](2026-11-15-recording-voice-keyer-dsr-design/phone/index.html) | iPhone: DSR, flag Record and Play, VOICE, Recordings, Voice Keyer, Recording settings |
| [ipad](2026-11-15-recording-voice-keyer-dsr-design/ipad/index.html) | iPad |

Where a mockup's sample text and this document differ, this document is right.
