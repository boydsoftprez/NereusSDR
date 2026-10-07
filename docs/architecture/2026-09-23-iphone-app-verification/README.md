# NereusSDR for iPhone: the verification matrix

This is the bench and device matrix for the iPhone app. Task 56b of the plan
(`../2026-09-23-iphone-app-plan.md`) wrote the listening rows (L1 to L10) and Task 70 the
rest (B1 to B19); the last section (D1 to D6) holds the other device checks the plan hands
to JJ. Every row has the requirement it traces, the setup, the steps, the expected
observation, and the result.

## How to read a result

- **pending**: nobody has seen it on a device yet.
- **passed (date, device)**: a line in the controller's ledger says JJ or the controller
  saw a device show it. The ledger's own words are quoted, with its date.
- **partial (date, device)**: part of the row is seen; the rest is named, and the row is
  not passed until all of it is.

A row is never marked passed from a unit test, a simulator run or a screenshot. "JJ's
iPhone" below means the physical phone used for that observation. Its model has not
been reconciled across the historical install records; the iPhone 18 Pro Max used in
simulator runs does not establish the physical model. Device names in historical
quotations are preserved as recorded. Record the confirmed model with each new device
observation. "The Pi 4 Core" is the NereusSDR Core on the Pi 4 beside the HL2.
"The Rock Core" is the Core on the Rock.

L1 to L10 are the plan's Task 56b rows 1 to 10 and B1 to B19 are Task 70's rows 1 to 19.

## Floors (the plan's)

No address, pairing code, key, UDID, serial or real IP goes into this file, a log or the
repository. The expected observations use the app's own words, quoted from the app.

Dates are 2026 and come from the ledger (`progress.md` in the crew folder).

---

## Listening rows (Task 56b)

Common setup unless a row says otherwise: JJ's iPhone with the build installed, the
Pi 4 Core running with the HL2 connected and remote access on.

### L1. JJ's one-time setup

- **Requirement:** R-IOS-16 (the signing and install path the rest depends on).
- **Setup:** a Mac with Xcode, JJ's iPhone, a cable.
- **Steps:** add JJ's Apple ID in Xcode's Settings, Accounts; turn Developer Mode on in
  the iPhone; connect the phone to the Mac once and trust it.
- **Expected:** the Mac lists the iPhone as a run destination and builds can be installed.
- **Result:** passed (2026-09-25, JJ's iPhone). Ledger: "Row 1 and 2 passed earlier"
  (the line recording rows 3 and 4).

### L2. Build with automatic signing, install

- **Requirement:** R-IOS-28 (the Push to Talk capability must provision on the app ID).
- **Setup:** the Mac, the cable, JJ's iPhone unlocked.
- **Steps:** build with `xcodebuild -allowProvisioningUpdates` under Task 51's team and
  install on the iPhone. If a declared capability (Push to Talk) cannot be provisioned,
  stop and bring JJ the error; the project keeps the entitlement.
- **Expected:** the build succeeds, installs and launches; the app opens with its five
  tabs and shows its build name at the foot of Setup.
- **Result:** passed (2026-09-24, JJ's iPhone). Ledger: "observed by JJ 2026-09-24: the
  app opens on his iPhone 18 Pro Max with the five tabs ... Signing, provisioning (Push
  to Talk), install and launch all work." Passed again for a later build on 2026-09-25
  ("row 2 passed again for this build: BUILD SUCCEEDED, installed and launched").

### L3. The Core is on the Pi 4 and JJ reads the pairing code

- **Requirement:** R-IOS-08 (the code is read at the Core and never sent anywhere), R-IOS-16.
- **Setup:** the Core/GUI session has put the build on the Pi 4 with JJ's go-ahead and given
  him the address and port.
- **Steps:** JJ reads the code on the Pi himself over SSH. A Core that still holds the R2
  access token counts as claimed, so its window starts closed: `sudo nereusd pairing open`
  opens it for one more device for 10 minutes, then `sudo nereusd pairing show` prints the
  code. On a packaged Core both need nothing but sudo (a Core started by hand takes its own
  `--config` and `--profile`). `sudo nereusd status` shows the pairing state ("Pairing: ...")
  and how many devices are paired.
- **Expected:** the code shows on the Pi and nowhere else; `status` counts the devices.
- **Result:** passed (2026-09-25, JJ's iPhone, the Pi 4 Core). Ledger: "rows 3 and 4 PASSED
  (JJ, 2026-09-25 ...): pairing opened on the Pi".

### L4. Type the address and the code, see the band

- **Requirement:** R-IOS-16 (connecting by a typed address, pairing by code), R-IOS-11.
- **Setup:** the Pi 4 Core's pairing window open; JJ has the Pi's IPv6 address and the code.
- **Steps:** on the phone type the address and the code.
- **Expected:** the phone pairs, signs in and shows the band from the HL2.
- **Result:** passed (2026-09-25, JJ's iPhone, the Pi 4 Core with the HL2). Ledger: "the
  phone paired by code over the Pi's public IPv6 and shows the band ('paired, band is
  showing!')".

### L5. Sound on the speaker, the earpiece and AirPods, and while locked

- **Requirement:** R-IOS-20 (the routes), spec section 4.7 (sound while locked).
- **Setup:** the phone on the band with sound on; AirPods charged.
- **Steps:** open the Sound panel from the speaker button; play on Speaker, then Earpiece,
  then AirPods; lock the phone; unplug or switch off AirPods mid-play.
- **Expected:** each route plays from where it says; locking the phone keeps the sound
  going; when headphones or AirPods disconnect the sound pauses with "Sound paused: your
  headphones disconnected. Tap to play on the speaker." and resumes on the speaker only
  after a tap.
- **Result:** partial. Seen: the speaker plays and, locked, the sound keeps playing
  (2026-09-25, JJ's iPhone: "sound plays on the speaker" and "locked, the sound keeps
  playing"); the band plays from the bottom loudspeaker after the route fix (2026-09-26,
  JJ's iPhone, TestFlight 2026.9.0 (2): "speaker route PASSED on device"). Pending: the
  earpiece, AirPods and the headphones-out pause (ledger 2026-09-25: "Earpiece, AirPods and
  the headphones-out pause: JJ tests later (pending)").

### L6. Tuning and the RX panel

- **Requirement:** R-IOS-11, the tuning gestures (spec section 5.1), and display-parity
  row 42 (the waterfall while dragging).
- **Setup:** the phone on the band, a signal on the HL2.
- **Steps:** drag the band (it pans), drag the flag or passband (it tunes that slice), tap,
  pinch to zoom, use the step choice and the number pad, change the RX panel's controls.
  While dragging and zooming, watch that new waterfall lines keep arriving and remain
  aligned with their frequencies.
- **Expected:** each does what it says and the sound follows; a precise frequency can be
  reached by drag, by step and by typing. The waterfall keeps updating through a drag
  and zoom and draws its lines where their frequencies fall.
- **Result:** partial (2026-09-26, JJ's iPhone, TestFlight 2026.9.0 (1)). Ledger: "row 6
  PASSED ... tuning feels good (drag pans, flag drag tunes, step, number pad)". Earlier
  findings (2026-09-25) were fixed first. Pending: itemised tap, pinch and RX-control
  observations, and the waterfall continuity/alignment check. These are not named in
  the quoted observation.

### L7. Offline and link lost

- **Requirement:** R-IOS-11, spec section 7 (the covers).
- **Setup:** the phone listening to the Pi 4 Core.
- **Steps:** (a) turn Airplane Mode on, then off. (b) With cellular on, turn Wi-Fi off,
  then on. (c) During LINK LOST, tap Cancel.
- **Expected:** (a) the cover says NO NETWORK with "This phone is offline." and the band,
  sound and moving display come back quickly when it is turned off. (b) LINK LOST shows
  with "Reconnecting, try N: next in S s" and Cancel, then the band is back on the air
  (over cellular where the carrier gives IPv6, else when Wi-Fi returns). (c) Cancel shows
  Reconnect and Back to Cores. No band scale or strip shows through the cover's words.
- **Result:** partial (2026-09-26, JJ's iPhone, build 3ef8feb9 and again on TestFlight
  2026.9.0 (1)). Ledger: "row 7 PASSED ...: Airplane Mode shows NO NETWORK and the band, the
  sound and the moving band come back quickly after it" and "row 7 PASSED again ...:
  Airplane Mode recovery quick". The cover's look was approved on the device on 2026-09-25
  ("the link-lost cover as built ... looks good"). The ledger lines name the Airplane Mode
  part; the Wi-Fi-off and Cancel parts remain pending because they are not quoted
  separately.

### L8. The band's frame time

- **Requirement:** R-IOS-11 (the band on a real iPhone), Task 52's device check.
- **Setup:** the phone on a wide band at 30 frames a second, Xcode's Instruments attached to
  the app on the phone. The app writes no frame-time log, so the Instruments Metal System
  Trace is the device measure.
- **Steps:** record the Metal System Trace for a minute and read the GPU time of each frame
  of the band's render pass.
- **Expected:** a 1179-pixel-wide band draws in under 8 ms a frame on the iPhone, as on the
  simulator's Metal device.
- **Result:** pending (no ledger line shows a device measurement).

### L9. Delete the app, install it again, keep the pairing

- **Requirement:** R-IOS-16 (decision D67), Task 15.
- **Setup:** a paired phone.
- **Steps:** delete the app, install it again, open it.
- **Expected:** the phone signs in to the Pi 4 Core with no code; the key and the paired
  Core read back from the Keychain (the unhosted tests cannot reach the real Keychain).
- **Result:** pending (the delete-and-reinstall itself has not been done). Related, seen: an update install
  kept the pairing (2026-09-25, "an update install kept the pairing"), and the TestFlight
  build opened straight to the Pi's Core with no code, the Keychain pairing carried over
  from the cable-installed build (2026-09-26, "row 9-adjacent PASSED"). Neither deleted
  the app first.

### L10. The Pan 1 and Display sheets against the Pi's Core as installed

- **Requirement:** R-IOS-11, decision D73, Task 54b.
- **Setup:** the phone on the band against the Pi's Core as installed.
- **Steps:** open the Pan 1 and Display sheets; try each control; quit and reopen the app.
- **Expected:** what the Core cannot do yet is greyed with "Needs a newer Core"; the rest
  acts on the band; the display settings survive reopening the app.
- **Result:** pending (the 2026-09-25 ledger planned it; no ledger line shows it observed).

---

## Bench rows (Task 70)

The numbering and order are the plan's, including 19 before 18. Each row stays pending
until a device shows it. The controller confirms which device answers before any step that
writes to it, and never restarts a box that could drop off its network without asking.
Keying rows go into a dummy load.

### B1. Keying on air from the phone

- **Requirement:** R-IOS-13, R-IOS-15.
- **Setup:** an ANAN-G2 and an HL2, each into a dummy load, one at a time; a Power Genius XL
  with the interlock on Block.
- **Steps:** key from the phone on each radio; then with the Power Genius XL in STANDBY, key
  again.
- **Expected:** the radio transmits and the band shows it; with the amp in STANDBY the
  interlock refuses the key and the phone offers "Operate amp".
- **Result:** partial. Seen (2026-09-26, JJ's iPhone, TestFlight 2026.9.0 (2), HL2 via the
  Pi 4 Core): "PTT from the iPhone keyed the HL2 ... the voice was heard clean on the
  ANAN-G2 at his home" and "TUNE from the phone put out RF ('looks like it did')". Not
  seen: a dummy-load run, the ANAN-G2 keyed from the phone, the interlock refusal.

### B2. Lock, switch apps, an incoming call, while keyed

- **Requirement:** R-IOS-14, R-IOS-13, decisions D24 and D25.
- **Setup:** keyed from the phone into a dummy load, a meter on the carrier.
- **Steps:** lock the phone; separately, switch to another app; separately, take a call.
- **Expected:** locking stops the carrier; switching apps keeps it; a call arriving
  mid-transmission stops it. Also measure the lock-to-unkey time.
- **Result:** partial. Seen (2026-09-26, JJ's iPhone, TestFlight 2026.9.0 (2), HL2 via the
  Pi 4 Core): "TUNE keyed at low power, locking the phone stopped transmit 'feels
  instant'". Not seen: the carrier on a meter, switching apps, the call.

### B3. The time-out on air

- **Requirement:** R-IOS-13.
- **Setup:** the transmit time-out set to 30 s, keyed into a dummy load.
- **Steps:** key and hold.
- **Expected:** the carrier stops at 30 s.
- **Result:** pending.

### B4. Taking transmit

- **Requirement:** R-IOS-17, decisions D51, D52 and D55.
- **Setup:** four devices on one Core, the Rock Core with the ANAN-G2.
- **Steps:** take transmit from a device that is keyed on air; press the radio's own PTT
  while the phone holds transmit; with four devices connected, a fifth tries to take a place.
- **Expected:** the holder keyed on air, the taker confirms, the carrier stops before
  transmit moves; the radio's PTT is handled as designed; the fifth device gets its
  question and takes a place.
- **Result:** pending. (A separate 2026-09-30 line has JJ meeting "Take transmit on this
  device first." on the flag; that was a finding, not this row.)

### B5. Link severed mid-transmission on each path

- **Requirement:** R-IOS-14, remote design section 12.1 (the mandatory severing test).
- **Setup:** the phone as the client, keyed into a dummy load.
- **Steps:** sever the link on each path: direct on the LAN, direct across NAT, TURN over
  UDP, the relay floor.
- **Expected:** the carrier stops each time within the deadline.
- **Result:** pending.

### B6. Microphone starvation mid-transmission

- **Requirement:** R-IOS-13.
- **Setup:** keyed from the phone, USB then FM.
- **Steps:** starve the microphone stream mid-transmission.
- **Expected:** in USB the radio stays keyed and goes silent; in FM it unkeys.
- **Result:** pending.

### B7. Amp and tuner from the phone

- **Requirement:** R-IOS-19.
- **Setup:** the Power Genius XL and Tuner Genius XL; the RF2K-S when one is available.
- **Steps:** OPERATE and STANDBY on the amp; TUNE, OPERATE, BYPASS and the antennas on the
  tuner.
- **Expected:** each acts on the device and the page follows what the device reports.
- **Result:** pending.

### B8. The phone on a cellular hotspot

- **Requirement:** R-IOS-02 to R-IOS-05, R-IOS-10.
- **Setup:** the phone on a cellular hotspot (T-Mobile and one other carrier), the Rock
  Core's station.
- **Steps:** connect by each rung: direct over IPv6, IPv4 hole punching, TURN over UDP, the
  relay floor (forced); then an upgrade from relay to direct.
- **Expected:** each rung connects; the upgrade happens without a heard glitch.
- **Result:** pending. Related, not this row: on 2026-09-29 JJ retested on 5G and the
  ledger records "JJ confirmed IPv6 direct after the firewall change; not independently
  verified". The IPv4 hole punch, TURN, the forced relay floor and the upgrade are not seen.

### B9. Pairing three ways, and revoking

- **Requirement:** R-IOS-16, R-IOS-08.
- **Setup:** a Core with its pairing window open; the desktop showing paired devices.
- **Steps:** pair on the LAN with one tap; pair by code through the relay; pair by typed
  address; then revoke the phone from the desktop mid-session.
- **Expected:** each pairs; the revoked phone drops.
- **Result:** partial. Seen: pairing by typed address and code (see L4, 2026-09-25, JJ's
  iPhone). Not seen: one tap on the LAN, by code through the relay, revoking mid-session.

### B10. The device matrix

- **Requirement:** R-IOS-22, R-IOS-24, R-IOS-15.
- **Setup:** two iPhone sizes (one with the Action button), an 11-inch iPad, AirPods, a
  Bluetooth PTT button.
- **Steps:** run the band in both orientations, locked and unlocked; run an eight-hour
  session.
- **Expected:** every screen fits and works on each device and orientation; the eight-hour
  session finishes without a fault.
- **Result:** pending.

### B11. The desktop's station switch

- **Requirement:** R-IOS-13, the station design's switch.
- **Setup:** the ANAN-G2 on the Mac with the phone listening.
- **Steps:** quit and reopen NereusSDR on the Mac, repeatedly.
- **Expected:** the phone comes back each time with transmit off.
- **Result:** pending.

### B12. A flashed card in a Pi 4

- **Requirement:** R-IOS-16.
- **Setup:** a flashed card in a Pi 4 beside a radio.
- **Steps:** claim it from the phone with one tap.
- **Expected:** one tap claims it and the band shows.
- **Result:** pending.

### B13. Versions

- **Requirement:** R-IOS-16, decisions D23 and the version rules (Tasks 4 and 8's debug
  overrides).
- **Setup:** the debug overrides; a station speaking major 1.
- **Steps:** (a) the phone speaks majors 1 and 2 against a station speaking 1; (b) the phone
  speaks 2 and 3 against a station speaking 1; (c) a station speaks 1 and 2 and the phone
  speaks 1.
- **Expected:** (a) connects, and the page names the Core as running an older NereusSDR
  ("<Core> runs an older NereusSDR.") with what it lacks greyed "Needs a newer Core";
  (b) the trouble screen "<Core> needs updating" names both sides ("The Core speaks",
  "This app speaks"); (c) the station serves the phone.
- **Result:** pending.

### B14. Every screen against the board's pictures

- **Requirement:** spec section 5, the `ui-verification` skill.
- **Setup:** the phone on the device, the board's pictures.
- **Steps:** take a screenshot of every screen in spec section 5 on the device and compare
  side by side.
- **Expected:** each matches its picture.
- **Result:** pending.

### B15. The station killed and its Ethernet pulled while keyed

- **Requirement:** R-IOS-14.
- **Setup:** keyed from the phone into a dummy load on an ANAN-G2, then an HL2.
- **Steps:** `kill -9 nereusd`; separately pull the Ethernet; time each.
- **Expected:** each radio's own watchdog stops the carrier; the time is recorded.
- **Result:** pending.

### B16. PureSignal while keyed

- **Requirement:** R-IOS-13.
- **Setup:** the ANAN-G2, keyed from the phone.
- **Steps:** calibrate and correct PureSignal.
- **Expected:** it calibrates and corrects while keyed from the phone.
- **Result:** pending.

### B17. TCI transmit through the NereusSDR window

- **Requirement:** R-IOS-17, decision D58.
- **Setup:** an app on the operator's computer using TCI through its NereusSDR window.
- **Steps:** transmit while that window holds transmit; again while another device holds it;
  again on a Core with no window.
- **Expected:** it transmits while the window holds transmit; it is refused with the
  window's reason while another device holds it; with no window no app transmits.
- **Result:** pending.

### B19. The several-devices design's bench rows

- **Requirement:** R-IOS-17, the several-devices design section 14.3.
- **Setup:** per that section, with the phone as one of the devices.
- **Steps:** run each row of section 14.3.
- **Expected:** as that section states.
- **Result:** pending.

### B18. A 30-minute keyed session

- **Requirement:** R-IOS-13, R-IOS-14.
- **Setup:** the time-out set to off for the run; keyed from the phone into a dummy load.
- **Steps:** stay keyed for 30 minutes.
- **Expected:** no false starvation from clock drift.
- **Result:** pending.

---

## Other device checks the plan assigns to JJ

### D1. MON in the phone's headphones (Task 55b)

- **Requirement:** R-IOS-20, R-IOS-13, decision D80.
- **Setup:** keyed on the HL2, AirPods on, a Core that sends the transmit monitor.
- **Steps:** turn MON on the TX panel; repeat with the loudspeaker.
- **Expected:** the voice is heard in the AirPods as it goes out; no feedback; on the
  loudspeaker MON is greyed with "Plug in headphones to hear your transmit. The loudspeaker
  would feed back into the microphone."; on a Core without the capability it is greyed with
  "This Core does not send the transmit monitor. Updating the Core may help."
- **Result:** pending. The MON build merged on 2026-10-01 (ledger); no device line yet.

### D2. Hardware PTT (Task 65)

- **Requirement:** R-IOS-15, decision D26.
- **Setup:** the PTT probe build on JJ's iPhone with the Action button; a wired headset and
  a Bluetooth PTT button; a dummy load. The ledger records JJ's 2026-09-30 ruling that the
  undocumented parts (an AirPods stem press, Camera Control without a camera session) are
  skipped.
- **Steps:** with the app joined to a Push to Talk channel: does station audio keep playing in
  the background; which presses begin and end a transmission (wired headset button,
  Bluetooth button through CoreBluetooth, Action button); can a press toggle, the first
  starting a transmission that stays on until the second, including locked. Then, in the
  built feature, each button locked and unlocked keying on air.
- **Expected:** the findings are recorded in `ptt-experiment.md`; each supported button keys
  and unkeys a locked phone; with the locked-phone switch off, a locked phone ignores the
  buttons. Until Task 65 wires it, the app draws that switch greyed and off, labelled
  "Buttons key a locked phone" (the plan's "Key a locked phone").
- **Result:** pending. The probe merged on 2026-09-30 (ledger: "Bench for JJ: steps in
  ptt-probe-task-report.md"); no device observation is recorded.

### D3. Measurements and the published figures (Task 68)

- **Requirement:** R-IOS-23, R-IOS-22, R-IOS-10.
- **Setup:** the Rock Core's station, one panadapter, receive only; the measurement log on.
- **Steps:** run each mode (Full, Balanced, Saver, Audio only) for one hour on Wi-Fi and one
  hour on cellular; an hour of talking; a multi-hour run logging battery and heat.
- **Expected:** measured cost per hour for each mode and network, battery per hour with the
  screen on and sound only, the thermal states reached; sound only shows only audio and
  telemetry traffic; conditions recorded (phone model, carrier, signal, station radio).
- **Result:** pending (the ledger notes the physical measurements remain pending).

### D4. The band plan strip matches the desktop's (Task 54d)

- **Requirement:** R-IOS-11, decision D79.
- **Setup:** the phone and the desktop on the same span.
- **Steps:** compare the strips; pick a plan on the phone.
- **Expected:** the phone's strip matches the desktop's; picking a plan on the phone moves
  the desktop's plan.
- **Result:** pending.

### D5. The lock screen, the island and eight hours (Task 64)

- **Requirement:** R-IOS-14, R-IOS-22, decisions D24 and D25.
- **Setup:** keyed into a dummy load; an iPhone with the island and one without.
- **Steps:** lock while keyed; switch to another app while keyed; leave it eight hours;
  force-quit while keyed; watch the card's update rate in the background; StandBy on a
  charger on its side.
- **Expected:** locked stops the carrier, another app keeps it; the card and island show the
  state and a lost link alerts; UNKEY, Mute and Cancel on the card run with one tap, Reconnect
  asks for Face ID; a killed app's card does not keep saying keyed; StandBy shows the card at
  twice its size.
- **Result:** partial. Seen: locking while keyed stopped the carrier "instantly" (see B2,
  2026-09-26). The rest is pending.

### D6. The first install and the cable path (Task 51)

- **Requirement:** R-IOS-28, R-IOS-29.
- **Result:** covered by L2 (passed 2026-09-24).

---

## Tally

35 rows: L1 to L10 (10), B1 to B19 (19) and D1 to D6 (6).

- Passed: L1, L2, L3, L4 (D6 is L2 again).
- Partial: L5, L6, L7, B1, B2, B9, D5.
- Pending: L8, L9, L10, B3 to B8, B10 to B19, D1 to D4.
