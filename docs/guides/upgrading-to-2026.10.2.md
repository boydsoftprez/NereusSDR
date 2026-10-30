# Upgrading to NereusSDR 2026.10.2

Use this note when updating a station from 2026.10.1. It lists what changes
when you first connect, what to check before you transmit, and a short list of
things to try. If you are coming from an earlier release, read the
[2026.10.1 note](upgrading-to-2026.10.1.md) first. When reporting a problem,
include the installed version, operating system, radio model and firmware, and
whether you use a local or remote Core.

## Update the Core and the desktop together

The Core owns the radio, the station DSP and the accessories. The console runs
where you operate. Update both, and the iPhone and iPad app if you use it.
The phone app has its own version number and delivery through TestFlight;
desktop and Core packages do not update it. Check the mobile availability
announcement for the matching phone build. Two improvements need a 2026.10.2
Core and updated desktop or phone clients:

- **The new remote display format.** Every waterfall row and trace stands on
  its own, so a lost network message costs one row instead of smearing the
  rows after it. An older Core, window or phone app keeps the earlier format.
- **RX meters held to their audio.** A remote window's S-meter and receive
  meters move with what you hear. With an older Core they show readings as
  they arrive, as before.

Older Cores, windows and phone apps still connect to each other.

Remote CW start needs the updated Core and an updated desktop or phone client.
An older client can still stop CW and release its key. The Core now tracks a
remote CW message independently of MOX and microphone PTT, including supported
QSK. A brief link interruption pauses the message and computer sidetone;
recovery starts the unfinished letter again. Stop, session loss or a prolonged
dropout cancels the message, so a late response cannot restart it.

CW receive shift and display alignment now put the dial at the carrier
frequency. Saved CW frequencies keep their numerical values; check a known CW
signal and retune once if needed after updating. Radio-side key and paddle
behavior depends on the firmware and wiring. Physical paddle takeover has not
been verified on the installed Hermes Lite 2 variant; client and packet tests
do not establish that physical-input observation.

## Check before you transmit

- **TUNE uses only the drive source chosen in Setup.** Open **File >
  Settings... > Transmit > Power** and look at the **Tune** group: **Use Drive
  Slider**, **Use Tune Slider** or **Use Fixed Drive**. Moving the RF Power or
  Tune Power slider no longer changes that choice. Your radio keeps the source
  it had saved, which may be whichever slider you last moved in 2026.10.1, so
  pick the one you want before your first TUNE. During TUNE the slider that is
  not in use is grayed.
- **ANAN-G2E: less drive on every band.** The G2E has its own PA gain table,
  53.0 dB on 160 m through 6 m, so a given power setting makes less output than
  before. A G2E still on the untouched default PA profile moves to the new
  values on its next connect to the radio; an edited or custom profile is left
  alone, and **Reset Defaults** on the PA Gain page gives the new values. This
  is a safe starting point, not a calibration: check each band against a
  wattmeter into a dummy load.
- **SWR protection now lowers the drive.** In 2026.10.1 a high-SWR trip was
  seen but the radio kept full drive. Now the drive drops when protection
  trips, and returns to full when you unkey. An open antenna drops the key with
  **High SWR condition detected. Please check your antenna connection.**, and
  a microphone PTT or VOX key that is still held stays off until you release
  it. Check this into a dummy load before relying on it on the air.
- **Check your antenna table rows.** Setup's antenna table is now the only
  per-band antenna memory, and a slice no longer carries its own. Open
  **Hardware Config > Antenna / ALEX > Antenna Control** and check the rows for
  your bands before you transmit. A row that an older version changed by mistake
  stays as it is until you set it once.

## What else you will notice

- **Phone waterfall speed.** The selected update period now controls visible
  2D scrolling in logical screen points. A saved 10 ms period will look much
  faster than the earlier one-physical-pixel-per-received-row behavior,
  especially on a Retina screen. The received display frame rate still
  controls how often new RF observations arrive; the phone preserves those
  rows and their timestamps rather than requesting extra FFT work. Pause and
  rewind retain the actual history timing when the Core changes frame rate.
- **Start Roundtable from the phone.** Open **Tools > Roundtable** and use
  **Start** or **Stop** beside its connection state. The Core owns this
  connection. On first use, choose **Stay hidden** or **Go live** in the
  existing public-visibility prompt; starting the connection does not make
  that choice for you.
- **Roundtable follows people and frequency.** Desktop, phone and website show
  exact-frequency people and cochannel callsign stacks, with separately attributed
  last-talker reports. New chat is shared within 5 kHz in the same mode family;
  earlier received messages keep their frequency/time after tuning. Read or browse
  without tuning, and use the explicit Tune action when desired. Distance defaults
  to miles, with kilometres and true-bearing display options. Deleted, moderated
  and expired messages refresh in retained history; disconnected history is marked
  stale. Use matching Core, client and server updates for these frequency views;
  older servers are identified as legacy room servers. See the
  [combined bench checklist](bench-2026.10.2.md#roundtable-on-desktop-iphone-and-website).
- **Tuning on the pan.** You can grab a receiver anywhere inside its
  passband, or near its center line, and drag it. Dragging a VFO flag sideways
  tunes its slice. Two-finger trackpad tuning no longer coasts after your
  fingers lift.
- **Slice letters after a restart.** A paired window or phone gets its own
  slice letter back, with its step and AGC, when a Core inside a desktop window
  restarts, as a headless Core already did. That desktop window's own slice
  may now start on B or later instead of A.
- **Filter presets.** The F1 to F10 presets for the SSB, CW, digital, AM, SAM
  and DSB modes have new default values; CW F1, for example, is 1.0 kHz wide.
  Presets you edited keep what you saved. If you once reordered a mode's
  presets, it keeps the old values until you use **Reset All Rows for This
  Mode** in **Setup > DSP > Filter Presets**.
- **Filter limits.** A receive filter now stays between 10 Hz and 10 kHz wide,
  with no edge more than 10 kHz from the carrier, however it is set; FM is not
  limited. A new **Limit filter edges to sidebands** checkbox in **Setup >
  General > Filters** (off by default) also keeps filters from crossing the
  carrier.
- **Radio search.** The window stays usable while it searches for radios. The
  search pauses while a connection starts, so the Connection panel fills in
  about 3 seconds after you connect.
- **Alex receive-only antennas.** The three receive-only columns in
  **Hardware Config > Antenna / ALEX** are checkboxes again, and unticking one
  returns a band to none.
- **The phone and iPad** take the desktop's workspace look, and the band keeps
  your zoom across launches.
- **The phone's flag level shows the peak** by default, as on the desktop. To
  go back to the average, open the flag's **...** menu and choose **S-meter:
  Average**.

## What to try

Each item is a short check you can do in a few minutes. Note the result,
including anything that surprised you.

1. **Remote waterfall.** From a remote window on a 2026.10.2 Core, watch a
   busy band for a few minutes over Wi-Fi or remote access. Confirm the
   waterfall scrolls evenly, with no pauses, repeated rows or smears.
2. **Remote meters.** Tune a signal that fades in and out and confirm the
   S-meter moves with the audio rather than ahead of it.
3. **Receiver grab.** Drag a wide receiver from the middle of its passband and
   a narrow CW receiver anywhere inside it. Drag a receiver that is not
   selected and confirm it becomes selected and moves. Drag a VFO flag
   sideways. Confirm the filter edges still resize the filter.
4. **Trackpad.** Tune with two fingers, slowly and quickly, and confirm the
   frequency stops when your fingers lift. Try a Linux or Windows touchpad if
   you have one.
5. **TUNE drive source.** Pick each of the three Tune sources in turn, press
   TUNE into a dummy load, and confirm the output follows the chosen source on
   two bands.
6. **SWR protection.** Into a load with a known high SWR, key and confirm the
   drive drops and comes back to full on the next key. With an open antenna,
   hold PTT and confirm the key drops and stays off until you release it.
7. **Two slices, one pan.** With slices A and B on one pan, tune A into the
   range B's receiver covers. Click between A and B and confirm the pan keeps
   its history.
8. **TX rows in a container.** In Container Settings, add **TX Keying** or
   **TX Monitor** from **Available** to a container, apply, and confirm it
   works the same as the row in the TX applet.

## Reporting

Report problems on [GitHub](https://github.com/boydsoftprez/NereusSDR/issues)
or on [Discord](https://discord.gg/m35ERjwRe). Include the installed version,
operating system, radio model and firmware, and whether you use a local or
remote Core.
