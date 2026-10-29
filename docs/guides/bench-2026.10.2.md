# NereusSDR 2026.10.2 combined build bench checklist

Use this with the combined candidate after it is installed. Start with the first 30 minutes and use the later sections when the equipment and features are available. Nothing on this sheet has been passed until you record it. PR #339 (XDMA) is excluded from this candidate.

Only the operator controls the radio and any transmit test, using the existing station procedure and a suitable load. Do not change the station's power, DSP, antenna, amplifier or safety settings just to make a test pass. CW tests are optional; paddles and a straight key are not required. Physical key takeover on the installed Hermes Lite 2 variant remains unverified.

## Before starting

- Date and operator: ____________________
- Candidate commit/build identity: ____________________  (fill in from the delivered build)
- STRIX desktop/Core build: ____________________  Zephyrus desktop/Core build: ____________________
- Mac build: ____________________  iPhone app version/build: ____________________
- Radio, firmware and protocol: ____________________  Local or remote Core for each client: ____________________
- Accessories or other software used: ____________________

Update the Core and desktop together; update the phone separately. Older clients and Cores can connect, but some newer display and remote CW features require matching updates. Before transmitting, review [the 2026.10.2 upgrade note](upgrading-to-2026.10.2.md), including the saved TUNE drive source, antenna table, G2E PA profile and SWR behavior. Saved custom PA profiles remain unchanged; an untouched G2E default profile migrates on connection. Previously edited filter presets stay saved; a reordered mode retains its old values until reset. The old per-slice antenna memory is not read, so check each band row. Saved CW frequencies retain their numbers, although CW alignment changed. Keep a backup of existing settings and profiles before upgrading; if returning to an older version, preserve the backup and record any settings changed during this run rather than assuming an older version can reverse every migration.

## First 30 minutes: daily operation

For each applicable device, record the result in the matrix below. If a feature is unavailable, mark N/A; if you choose not to try it, mark Skip.

1. **Start and connect.** Open the app, find the expected radio or Core and connect. The window stays responsive while searching; the radio/Core details, firmware and receiver count appear. Verify the build identity above and that no unexpected key or TUNE occurs.
2. **Receive.** Tune a familiar signal. Audio is clear and follows tuning; the spectrum and waterfall move steadily, and the S-meter is plausible and moves with received audio. Try a second band and return. Note any delay, silence, burst of noise, stuck meter or frozen display.
3. **Tune and filters.** Tune with the normal controls, drag a receiver inside its passband and drag its flag. The selected slice follows without an unexpected jump; filter edges still resize it. On a trackpad, two-finger tuning stops when fingers lift. With CTUN and zoom, tune inside and outside the view; the displayed frequency, receiver position and heard signal agree.
4. **Slices and history.** Open two slices on one pan, tune one into the other's coverage and switch between them. Both remain usable and the pan retains its history. Change sample rate if normally used: audio should briefly mute and return without prolonged noise or a stuck meter.
5. **Save and return.** Adjust a reversible frequency, mode, filter, AGC or layout setting; save it using the normal controls. On the next ordinary app restart, check it returns as saved. Restore the original value if desired. Check the radio's saved information remains populated.
6. **Remote and phone.** Connect Mac and iPhone to the intended Core. Tune from each and confirm the right slice responds, audio follows promptly and display rows continue smoothly. Switch between the available Cores using the normal controls; the selected station and slice are clear and audio/display recover. Disconnect and reconnect normally and confirm the appropriate slice and settings return. If audio is temporarily unavailable, allow the normal connection to recover and note whether audio resumes without a separate manual toggle.
7. **Phone touch and display.** Tune repeatedly while tapping normal band, mode and setup buttons. They respond and remain usable while the band updates. Try 2D and 3D waterfall, zoom and relaunch; rows move smoothly and zoom returns. In the flag menu try S-meter Peak and Average; the reading and label follow the selection.

## Results

Use **Pass**, **Fail**, **Skip** or **N/A** in each cell. Add device, Core and failure details in Notes; one device passing does not imply the others passed.

| Check | STRIX | Zephyrus | Mac | iPhone | Notes |
|---|---|---|---|---|---|
| Start, discovery and connect | | | | | |
| RX audio, spectrum and S-meter | | | | | |
| Tuning, filters and CTUN | | | | | |
| Slices, history and sample rate | | | | | |
| Save and restore | | | | | |
| Remote handover, reconnect and audio recovery | | | | | |
| Phone buttons, 3D, zoom and meter | N/A | N/A | N/A | | |
| Desktop containers and display | | | | N/A | |
| CAT / TCI | | | | N/A | |
| Roundtable | | | | | |
| PureSignal / optional TX | | | | | |
| Applicable radio and accessories | | | | | |
| Optional CWX | | | | | |

## Desktop and Core: follow-up checks

- **Display and receiver controls:** Try waterfall pause/history and return to live, resize or float a pan, then close and reopen it. Display remains usable and closing a menu or floating window does not crash the app. Check the remote waterfall for repeated rows or smears and compare remote meter motion to audio. Try the filter presets and, if normally used, sideband limit; the shown filter matches the selected setting.
- **Containers:** Add one RX row and one TX row from Available, apply, and compare their shown state with the corresponding applet. Try Frequency Custom, band buttons, color and visibility options, then save and reopen the workspace. Chosen options and values return. A refused save should explain why; an unchanged save should not claim new changes. Restore the original workspace if preferred.
- **CAT and TCI:** If used, reconnect the ordinary native application and check its frequency, mode, filter and AGC control/readback. A saved CAT slice binding remains valid after a normal restart. TCI audio remains continuous during ordinary tuning; note gaps or backlogs. Browser pages should not obtain an unsolicited TCI connection. Do not introduce a new keying test solely for this check.
- **Roundtable:** With the intended stations and accounts, open the room, check presence and incoming messages on both Cores and the phone, and switch stations normally. The active room and station remain understandable, messages arrive and controls respond. Use any transmit/PTT portion only as the operator's normal station procedure permits; record which participants and Cores were actually checked.
- **Remote recovery:** During normal receive, use an ordinary disconnect/reconnect or station handover. Audio, display and controls recover on the selected Core without stale data from the previous session. Note the actual route and what had to be touched.

## Phone follow-up

In the 2D waterfall, compare the same signal feature at 10 ms and 100 ms:
the 10 ms setting should scroll roughly ten times faster. This is a visible
time scale, while the Core's accepted FPS determines new RF observations.
Pause should hold the picture; after a normal display-rate change, rewind
should still reach the older history with its original time labels. Record
the accepted FPS, phone model and whether the result repeats.

Open **Tools > Roundtable**, start and stop the Core's connection there,
and check that the phone follows its reported state. If the first-run
visibility prompt appears, make the intended choice explicitly; a connection
start alone must not answer it. Skip public participation if it is not wanted.

Check the workspace look and Display settings, slice flags, S-meter Peak/Average/Max Bin availability, and normal setup controls. Change a display option, leave and return: the selected value remains visible. Tune a signal under normal display activity and listen for responsive audio; record an audible delay or backlog rather than estimating a pass. Check 3D rows at the usual viewing rate and look for repeated rows, ridges or stutter. If the Core reports audio unavailable, note whether the current wanted audio recovers after it becomes ready. Record device, app build, Core and connection used.

## Hardware and operator-controlled transmit, only when applicable

- **Antenna and radio:** Review the antenna table for the bands in use; when normally changing bands or receive-only antenna selections, confirm the correct row and displayed choice follow the radio. For an Anvelina, check only configured filter/open-collector outputs against the established station setup. Record model and actual equipment; skip absent hardware.
- **TUNE and tuner:** Before any TUNE, verify the chosen drive source in Setup. During an operator-controlled TUNE into the station's appropriate load, the selected source controls drive and the unused slider is grayed. If using a tuner, observe its normal completion and return to receive; stop and record unexpected behavior. Do not test safety trips by creating an unsafe antenna condition.
- **PureSignal:** If the station normally supports it, follow [the PureSignal IMD bench procedure](../development/puresignal-imd-bench-check.md). Check the displayed source, current/waiting state, correction and measurements against actual receiver feedback. Record the setup and outcome separately from the display; a graph alone does not prove RF coupling or calibration.
- **Optional CWX:** If you choose to use CW, send a short typed message or memory through the updated client and stop it using the normal station procedure. Stop should end it. A brief ordinary network interruption may pause and resume the unfinished letter; a stop or ended session should not restart the old message. Mark Skip if not tried. No physical paddle or straight-key check is required; physical takeover remains unverified.

## If something fails

Record the device and exact app/Core builds, time, radio/firmware/protocol, local or remote route, the steps just taken, expected and observed result, and whether it repeats. Save a screenshot or short recording and the relevant existing log if available. For an audio or display problem note which slice, mode, frequency and Core were active. Leave the result as Fail or Skip until retested; do not infer hardware success from a software-only result.

## Coverage notes

This checklist groups the changes since 2026.10.1 from [the changelog](../../CHANGELOG.md), the [upgrade note](upgrading-to-2026.10.2.md), and the combined work: remote transmit/CWX (#668, #680, #681), receive rate and audio (#654, #661, #687, #693–695), containers (#664, #666, #669, #671, #672, #677, #683), phone display (#653, #674), PureSignal (#662, #665, #682), tuner and radio (#670, #684, #689), and Roundtable (#657, #659). Applicable earlier changes on main, including tuning, filters, antenna, TCI/CAT and remote display, are included in the operating checks above.
