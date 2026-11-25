# Recordings, the voice keyer and the Digital Spectrum Recorder

The Core makes and keeps every recording. A desktop window, an iPhone or an iPad shows the recordings and controls them, but keeps none of its own. Every computer and phone signed in to the same Core sees the same list, the same recording in progress and the same voice messages.

There are three kinds of recording:

| Kind | Made by | Holds |
| --- | --- | --- |
| **Contact** | **Record** on a slice flag | What you hear on that slice, and your own voice while you transmit. |
| **Whole pan** | **SAVE** on the Digital Spectrum Recorder | A stretch of the whole band as the radio saw it. It replays like rewind, any station on it. |
| **Sound** | **Save sound only** | The sound of one slice over a saved stretch, as MP3 or WAV. |

A window or phone connected to a Core without recordings shows every control described here, grayed, with **This Core needs an update for recordings.** Update the Core to use them. A desktop window running its own radio (not through a Core) has recordings too.

## Record a contact

Each slice flag has a column of round buttons: close, lock, **Record**, **Play**, then the more button at the bottom. Slice A has no close button.

1. Press **Record** on the slice you are working. It blinks red with the running time beside it. On the phone the time is under it.
2. Transmit as usual. While you are keyed, **+ your voice** shows beside the time, and your transmitted voice goes into the recording. It sounds the way MON plays it.
3. Press **Record** again to stop. A **Kept** notice shows under the flag for about 6 seconds with the name, the length, why it stopped and **Play**.

Any device can stop a recording any device started. Each slice records on its own, so two slices recording at once make two files. If the disk fills, the recording stops, is kept up to that point, and the flag says **Stopped: the disk is full**. If the link to the Core drops, the Core keeps recording and the flag shows it again on reconnect.

### Start and stop by themselves

In Recording settings (below):

- **Start recording by itself when I transmit** (off at first). While it is on, **Record** shows a hollow red ring and the tip reads **Waits for you to transmit**. On the phone the switch reads **Start by itself when I transmit**. Keying starts a recording on the slice you transmit on. Pressing **Record** while it waits starts one at once.
- **Stop by itself after** a number of minutes **with nothing heard and no transmit** (on at first, after 2 minutes; 1 to 60). Quiet means you have not transmitted and the slice has heard nothing above its squelch. With squelch off, it means nothing more than 6 dB above the noise. The Kept notice says it stopped by itself.

### The name

A recording is named when it stops, for example `2026-11-15 1432Z 7.236.400 LSB K4ABC.mp3`: the date, the UTC time, the frequency, the mode and the other station's call. The call comes from Roundtable: the station heard longest in that slice's room while you recorded, leaving out your own call. With Roundtable off, or nobody named, there is no call. Nothing guesses a call from spots or decodes.

## Play a recording

- **Play** on a flag plays that slice's latest recording. Press it again to stop. While it plays, the desktop shows a blue stop square with a label (frequency, call, time). The phone and iPad show a blue playing bar under the flag with **Others** and **Stop**.
- **Right-click Play** on the desktop flag for the slice's last eight, **All recordings…**, **Show in Finder** (Explorer on Windows, folder on Linux), and **Recording settings…**. On the phone, **Others** on the playing bar gives the last eight, **All recordings…** and **Recording settings…**.
- A recording plays only through the speakers of the device that pressed **Play**. Other devices on the Core stay quiet.

## Send a recording on the air

Key with **MOX** or PTT while a recording plays on this device, and the recording goes out instead of your mic.

- It goes out from where it was playing, not from the start. It stops coming out of your speaker; you hear it through MON the way you hear your own voice.
- The desktop label turns red with **ON AIR** and the Play ring turns red. The phone and iPad playing bar turns red and reads, for example, **ON AIR · 0:12 / 3:25**.
- It goes out at a fixed level, whatever your mic gain is set to.
- Unkey and it comes back to your speaker and keeps playing from the new spot.
- If it ends while you are still keyed, your mic takes over. The radio stays keyed until you let go.
- **TUNE**, VOX and CW do not send it. The recording keeps playing on your speaker.
- Only the device that keys sends its own playback. If the phone is playing and you key from the desktop, the desktop's mic goes out and the phone keeps playing to its own speaker.

While a recording goes out, EQ, compressor, leveler, CFC and phase rotator are off unless you turn them on under **Playing back** in Recording settings. Your own transmit settings come back unchanged afterward.

## The voice keyer

The voice keyer holds eight messages, M1 to M8, each up to 2 minutes, kept on the Core.

### Record a message

On the desktop, open the **Voice keyer** panel (**Containers > Applets > Voice keyer**), or press **VOICE** in the transmit row beside TUNE and MOX and choose **Voice keyer panel ›**. On the phone or iPad, open **Tools > Voice Keyer**.

1. Press **Record**, then click the message to record into (on the phone, choose it on the Record card).
2. Speak, then press **Stop**.

Messages are recorded from the mic before processing, so each send gets your mic gain and your current EQ, compressor and other transmit settings, and sounds like your live voice. A message imported from a WAV file goes out at a fixed level, whatever the mic gain. On the desktop, recording needs this computer's mic as the transmit source. **Import WAV…** fills the first empty message from a WAV file. Right-click a message (the more button on the phone) for **Listen** (your speaker only), **Rename**, **Load WAV…**, **Save as WAV…** and **Clear**.

### Send a message

- Click a message in the panel, or press **VOICE** and choose one. The radio keys, sends it and unkeys at the end. While it goes out, **VOICE** turns red, shows the message and time (for example **M2 0:01**) and fills from left to right.
- **Repeat** with a wait sends, waits (**M2 again in 5 s**) and sends again.
- **VOICE**, Escape, PTT or **MOX** stops it and unkeys. Escape keeps its usual meaning in a text field or an open popup. On the phone, tap **VOICE** or PTT.
- **F1** to **F8** send M1 to M8 in voice modes and CW messages 1 to 8 in CW modes. **Ctrl+Shift+F1** to **F8** always send voice messages and **Ctrl+F1** to **F8** always send CW messages. The keys do nothing while you type in a text box. Change them in Setup's Keyboard page, where the message keys have their own color.
- An empty message opens the panel instead of keying. In CW a voice message is refused with **Voice messages go out in voice modes. The transmit slice is in CW.** and the radio does not key.
- When another station holds transmit, sending is grayed with **Take transmit**.

## Find, rename and delete recordings

- **Recordings panel** (desktop: **Containers > Applets > Recordings**): the six latest, a play button on each, a red row while a slice records. Right-click a row for **Play**, **Rename…**, **Save a copy here…**, **Show in Finder** and **Delete…**.
- **Recordings window** (**All recordings…**): totals for what is kept on the Core and its free space, a filter (**All**, **Contact**, **Whole pan**, **Sound**), search, and a table you can sort by any column. **Rename**, **Save sound only** (whole pan), **Save a copy here**, **Show in Finder** (only when the Core runs on this computer) and **Delete** for one or several.
- **Phone and iPad:** **Tools > Recordings**. Filter, search, sort, a more button on each row, swipe left, **Select** for several, and the share sheet. A large whole-pan copy warns before it starts. On the iPad, a Recordings panel can also be shown in the right column; turn on **Recordings panel in the column** in **Setup > Recording settings**. It is off at first.
- A rename changes the shown name and the file name on every device.
- **Delete** always asks first, and cannot be undone.
- **Save a copy here** downloads a copy to this computer or phone. The original stays on the Core.

## Recording settings

Desktop: **File > Settings... > Cores > Recordings**. Phone and iPad: **Setup > Recording settings**. The settings belong to the Core, so every device sees the same. With no Core connected, the page is grayed with a note.

| Group | Settings | At first |
| --- | --- | --- |
| What the Record button keeps | Your own voice when you transmit; **Put the two sides:** **Together, one after the other**, or **Apart: them on the left, you on the right** | On; together |
| Starting and stopping | Start recording by itself when I transmit; Stop by itself after a number of minutes with nothing heard and no transmit | Off; on, 2 minutes |
| Files | **Save as:** MP3 or WAV (the page shows the size per hour); a name example | MP3 |
| Where they are kept | Which disk; how many and how much space | The Core's own disk |
| Playing back | Keying while a recording plays sends it on the air; which of EQ, compressor, leveler, CFC and phase rotator stay on while it goes out | Each off |

An SD card is fine for recordings. If MP3 fails for any reason, the recording is kept as WAV and the Kept notice says **Kept as WAV: the MP3 encoder failed**. A Core without an MP3 encoder offers WAV only and says so.

**Show the VOICE button beside TUNE and MOX** on the desktop page, or **Show the VOICE button beside PTT** in the phone's **Tools > Voice Keyer**, takes VOICE off this device's main screen. The voice keyer panel and the hot keys still send messages.

## The Digital Spectrum Recorder

The Digital Spectrum Recorder keeps the band on the Core's disk in a loop, so you can go back in time on the pan, hear any signal that was there, and save a stretch. It works like a DVR for television. It is always erasing the oldest to make room; saved stretches are never erased.

It is on at first on a new Core, using up to 10 GB on the Core's own disk. It starts off in a desktop window running its own radio. If the Core's only disk is an SD card, it stays off and Setup says why in red: connect a USB SSD or NVMe drive (a thumb drive is not enough). How far back it reaches depends on the size limit and the sample rate: about 2.8, 5.5, 11 and 22 GB an hour per receiver at 192, 384, 768 and 1536 kHz.

### Go back in time

1. Drag the time strip beside the waterfall back and let go. The pan's slice plays from that moment on the frequency it is tuned to.
2. To hear another signal, tune the slice: click or tap the signal, use the dial, or type a frequency. It plays from the moment you are at.
3. The waterfall scrolls with the sound (its top row is the moment you hear), the spectrum shows the past, **PAST** shows on the pan (on the phone, on the slice flag), and the S-meter follows. Roundtable says **Paused while you listen to the past.**
4. Press the red **LIVE** to return to live sound on the frequency you were on before.

When it cannot play, the waterfall says why. **Nothing was kept at this frequency then.** means the recorder was running, but not over that frequency.

Further back than the window's own 20 minutes, the Core redraws the waterfall from its disk; the picture dims while it does. The hours map beside the time strip shows every kept hour, newest at the top, shaded by how busy the band was, with a white mark where you are. Click or drag it to jump. Click a time on the strip to type one. Your own transmissions are kept too and marked on the waterfall, so a replayed contact has both sides.

Keying while rewound takes every rewound slice live, on the frequency it was playing, so you can call the station you went back to.

### Save a stretch

1. Press **SAVE** above the hours map. A yellow bracket marks 10 minutes. Drag its ends or middle.
2. The box shows the times, the length and the sizes. Choose **Save stretch (whole pan)**, **Save sound only (MP3)** (or **(WAV)**, following the Files choice in Recording settings), or **Cancel**. On the phone, sound only saves the active slice, and the picture holds still while SAVE is open.

A whole-pan stretch appears in Recordings and opens on the waterfall to replay like rewind. **Play its sound only** plays just its sound.

### Recorder settings

Desktop: **File > Settings... > Cores > Digital Spectrum Recorder**. Phone and iPad: **Setup > Digital Spectrum Recorder**. On or off and its status; which disk; the size limit and how many hours it holds; a bar showing what is in use, where it is writing and the part erased next; and **Erase what is kept now**, which asks first and cannot be undone. If the disk is unplugged, the recorder stops, the strip shows only the window's own 20 minutes, and Setup says the disk is missing.

