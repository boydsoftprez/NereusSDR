# Roundtable overlay redesign

Status: approved by JJ on the interactive board, 2026-11-03. Implementation plan:
[2026-11-03-roundtable-overlay-plan.md](2026-11-03-roundtable-overlay-plan.md).

The board is the reference for every look and behaviour here. Open
[board.html](2026-11-03-roundtable-overlay/board.html); its sources sit beside it (`overlay.js`,
`overlay.css`, `web.css`, `sections.html`, `make_board.py`). Where this document and the board
disagree, the board's Proposal look wins for appearance and this document wins for behaviour.

## Goal

Make the live overlay say who is on a frequency and how each listener hears them, truthfully and
at a glance, on the desktop, iPhone, iPad and the public web page. A double is unmistakable.
Anyone can look at another frequency without moving their radio.

The work starts from Codex's live frequency reports, committed unchanged as the first commit of
this branch, and fixes the defects its review found.

## Decisions

### Truth rules, shared by every client

* **D1. What a double is.** A frequency is a Double only when its open turn is `doubled`, or, for
  a frequency known only from the directory, when two or more keyed stations share one
  `sourceRoom`. Two keyed stations that merely sit within 5 kHz of each other in different source
  rooms are not a Double.
  Tags on a pan follow the source room, so a listener's frequency in a doubled room joins the one
  merged Double (D20); the web's list rows follow what is on that exact frequency, so a lone
  listener's row stays quiet while the room card says Double.
* **D2. Readings during a double are combined RF.** They are never credited to a transmitter.
  Distance and bearing are blank. Transmitters never appear among the listeners.
* **D3. Choosing the turn to show.** Use the exact-Hz turn if there is one, otherwise the nearest
  live turn within ±5 kHz in the same family, and on a tie the one that started most recently.
  This is what the web page does today, so returning to the remaining talker after a double works
  everywhere. Readings still belong to the turn the server attached them to.
* **D4. After a double.** The remaining talker's new turn starts with no readings; they arrive one
  by one. Combined readings never carry over. The line "Double ended 3s ago" shows for 8 seconds.
* **D5. Departed participants.** While a double is live, a participant who has gone is dropped.
  An ended double keeps every name, as history.
* **D6. A live talker is never captioned as past.** "Last talker" appears only when no turn is
  open.
* **D7. A dropped link never claims to be live.** Reconnecting and Offline freeze the card: no live
  colors, no rail stripes moving, no peak tick, meters grey, ages keep counting, and the caption
  reads "Last known". Reconnecting shows its amber note, Offline shows "Offline since HH:MM UTC".
  Hidden dims the card and says "You are hidden". Tags follow the same rule.
* **D8. One word.** "Double" everywhere a current double is named; "Last double" for an ended one.
  These go away: "Doubled", "DOUBLED", "TWO AT ONCE", "LAST OVERLAP", "Last overlap". The legend
  entry on the web page becomes "double". "Combined RF" stays.
* **D9. Present listeners only.** A reading row needs `present: true`. All three clients filter
  this; the web page does not today.

### The card

* **D10. Contents, top to bottom.** A 3 px rail in the state's color, striped for a double. A head
  line with the state glyph, the name (or "Double", "Unknown", "You · on the air"), a clock on a
  double's head, and the frequency. For a double, each keyed station on its own line or side by side
  when two fit. A caption: "Heard by", "Hearing you", "Combined RF" or "Last heard 6s ago", with
  the signal format at the right when the card is wide enough. Then one row per listener, nearest
  first.
* **D11. A reading row.** Call, signal in the chosen format, age, and beneath them a 3 px meter.
  This replaces the inline text-only table. The meter runs on the flag's S scale,
  `fraction = clamp((dbm + 121) / 88, 0, 1)`, with division marks at 13.6, 27.3, 40.9, 54.5 and
  77.3 percent. Color follows state: talking green, you cyan, unknown amber, a double striped red
  (`#e06c6e` on `#5d2c2e`), your own row striped cyan in a double. A 1 px white peak tick marks the
  listener's held peak.
* **D12. Peak hold.** Client side, per source room, listener and turn. It rises at once, falls
  2 dB per second, and resets on a new turn. It is not on the wire and is not sent anywhere.
* **D13. Old readings.** Past 20 seconds a row greys, its age turns amber and its tick goes.
* **D14. Movement.** Values, meters, ages and the clock update in place, without rebuilding the
  card, so nothing shifts or flickers.
* **D15. Colors.** Talking `#4caf6a`, unknown `#d8a55f`, double `#c25a5c` with text `#e06c6e`, you
  `#00d4ff`, quiet `#7e7e85`, card fill `rgba(10,10,12,0.92)`, rule `#2a2a2e`, dim text `#a6a6ac`.
  Nobody talking draws no card, only the compact tag.

### Placement

* **D16. Desktop.** Card 232 wide, anchored to the foot of the spectrum, on the far side of the
  active slice's flag, away from the stations around the dial. Left of about 57% of the pan's width
  the flag is to the right of the dial and the card sits past it. In the next 18% or so there is no
  room past the flag, and the card comes back beside the dial on the near side. On the last 25% or
  so the flag flips left and the card sits past it on the left. A dashed tick joins the card to
  the dial. Other slices' flags and the pan's own controls are keep-outs.
* **D17. iPhone.** Card 176 wide under the flag when the pan is tall enough. Otherwise it goes in
  the strip beside the flag, down to about 104 wide. Below 122 wide the row drops its age column
  and the head drops the frequency, which the flag already says. Below 150 wide the signal shows S
  units when the format is Both.
* **D18. iPad.** Card 232 wide under the flag, starting at the dial so it stays off a lower-sideband
  signal, clamped to the pan. Portrait uses the same rules with the width it has; JJ chose not to
  review that layout first, so it is checked on a device.
* **D19. Short pans.** Rows give way first, with "+N more ›". The Double head and the keyed names
  are the last things to go. In the inspector (D34), everyone not keyed folds to one "Also here"
  line first, and what still does not fit scrolls.
* **D20. Other frequencies' tags.** Along the foot, clear of the card and flags, at the nearest free
  spot, and left off when there is none. A double is red, with a striped top edge, the word Double
  and up to two names. A talker is green with its name and "+N". Quiet is grey with a name and
  "+N". One double spanning several exact frequencies in one source room is one tag. A frequency a
  flag already speaks for has no tag.

### Looking without tuning

* **D21. Clicking or tapping a tag views that frequency in the inspector (D34). It never tunes.**
  This supersedes the earlier rule that a tag tunes. Clicking the band itself still tunes, as always. Legacy servers
  that cannot read another frequency keep today's behaviour.
* **D22. What the view shows.** Head with state, clock and frequency. The Viewing line (D35). "On frequency · N": each station with its glyph, "keyed" when keyed, and its
  offset from the viewed frequency (`member.hz − viewed hz`), or "on freq" at zero. Then the
  readings as D11, adding distance and bearing unless D2 blanks them. A "Your pan" row is first when
  someone is on the air. Then **Tune slice A here**, disabled while you are keyed and when already
  there. Messages are read only until tuned, and only the viewed frequency's. On your own frequency
  the inspector drops the "Viewing" line and the Your pan row, and Tune stays in view, greyed, with
  the reason "Slice A is already here".
* **D23. Your pan.** The peak on this client's own pan in the family's sideband window, 2.7 kHz
  wide, on the sideband side of the viewed frequency. Blank when off the pan, before the first
  frame, or while transmitting. Calibration matches what the trace draws. Sampled about five times
  a second while a view is open, never per frame, and it adds no thread and no DSP.
* **D24. Where the view opens.** Desktop: the inspector, in your card's spot. iPhone: the bottom
  sheet. iPad: inside the Roundtable applet in the column, with no pop-up, and a popover over the
  pan when the column is hidden, for your own frequency as well. Web: the room pane, which already
  browses without touching a radio, so it has no Tune button.
* **D25. Clicking your own card** opens the inspector on your own frequency (D34). This replaces
  the earlier menu. Read reports and Signal move to the inspector's ⋯ (D36). Legacy servers keep
  their details dialog.
* **D26. Any slice's flag line** opens the inspector on that slice's frequency and leaves the active
  slice alone. The same line again closes it. The Messages button is what opens your own room.
* **D27. A refused or lost read** closes the view with the app's usual status message and leaves
  the slice alone. The Tune button closes the inspector and your card returns. A band click or a
  band drag tunes as always and leaves the inspector open on what it shows.

### The inspector

Settled on the board, section Desktop, "This proposal" (JJ, 2026-11-04: "build as shown"). The board
is the reference for every look in this section.

* **D34. One inspector.** Your own card, any tag, a fold's row and any flag's Roundtable line open
  the same inspector, on the frequency they name. It replaces the card while open, in the card's
  spot, 300 wide on the desktop, standing on the same foot line. The same thing clicked again, its
  close button or Escape closes it and your card returns. Another tag swaps it in place, with no
  close and reopen.
* **D35. What it shows.** The head (D22) with ⋯ and close. When it shows another frequency, one line
  under the head, "Viewing · +1.4 kHz" with Back to my slice, which returns to your own frequency in
  place. Then On frequency, readings, Tune and Messages. Keying and a dropped link behave as D22 and
  D7 say. A short pan folds unkeyed stations to "Also here", then scrolls with the wheel, fading at
  the cut. Tune and Messages never scroll away.
* **D36. Short menu.** Right-clicking a tag gives Open or Close inspector, Tune, Messages and Signal.
  The inspector's ⋯ has the same list, with Read reports. On iPhone and iPad the sheet's ⋯ has Signal
  and Cancel. The menu's Tune follows the same gates as the button, greyed and never hidden.
* **D37. Folds and placement.** A click on "+N" lists that fold's frequencies inside the inspector,
  and a row opens as D34. Tags stay clear of the open inspector, and near ones fold (D20). The spot
  follows the flag: if you tune far enough to flip it, the inspector moves with the card's spot. A
  pan that does not hold the active slice places it by its own slice's flag, or by the clicked tag
  when it holds no slice. While it shows your own frequency your compact tag is hidden; while it
  shows another, the compact tag stays and clicking it goes to your own.
* **D38. Band and phone.** D27 holds on the iPhone too: dragging the band leaves the sheet open.
  iPhone tapping the card, a tag or the flag line opens one sheet, the same on every frequency; the
  same thing again closes it; another tag swaps it without changing its height. iPad tags view in
  the column, and the same tag returns to your own.

### The flag's ⋯ and the S meter

* **D39. Flag ⋯ button.** The desktop flag gets a visible ⋯ button directly under the lock, the same
  size and style, holding exactly what right-clicking the flag holds. Right-click stays. It is part
  of the flag's region, so tags keep clear of it, and it follows the flag when it flips.
* **D40. S-meter item.** The flag's menu gains "S-meter: Signal ›" after Sample rate, listing Signal,
  Sig Avg, Signal Peak and Max Bin, with the current one marked. It drives the window's one existing
  S-meter choice (`SMeter_RxSelect`), so the panel's meter and every flag in the window follow, and
  no new setting exists. A listened flag still offers it, because it changes only this window. Max
  Bin greys with the reason when no pan shows that slice, as the iPhone does.
* **D41. The flag's level bar.** Max Bin prints MAX and Sig Avg prints AV above the readout in the
  bar's tick strip. Signal and Signal Peak print nothing, as the iPhone does. The value is the one
  the existing selector already sends to the flag, local and remote; nothing new is measured. Signal
  and Signal Peak read the same on the flag, as they do today, with no held peak added.

### iPad applet and Widen

* **D28.** The applet shows the frequency line ("7.236.400 LSB 6 stations"), the live line, a rail,
  then the same head, On frequency and readings as the view, then Earlier turns, Messages and the
  message box. Viewing adds Back to my slice at the top.
* **D29.** Widen keeps its current width and gesture. The left list shows every frequency; a double
  says Double with its names. A row views that frequency and never tunes. Your own frequency stays
  highlighted in cyan and the viewed one in the selection blue.

### Web page

* **D30.** A double's frequency group gets a red left edge and a "Double" tag on its label, and its
  keyed rows get the double glyph and red text. An unknown signal gets an amber "Unknown signal"
  tag. On the band map a double is red with a striped top and says Double with its names.
* **D31.** In the room, a rail above the caption, a clock beside "on the air", reading rows as D11
  with S and dBm on one line, and distance and bearing on one line at full width. At 760 px wide
  or less the page keeps its four columns (no offset) and stacks distance over bearing.
* **D32.** "On frequency" shows inside the room only at 760 px wide or less. At full width the list
  beside the room already says it.
* **D33.** Activity chips keep their names (`roundtable.js` passes the room). Nothing else on the
  page changes: bands, search, sort, quiet stations, distance, bearing, signal, tabs, mute, composer.

## Out of scope and unchanged

No server, wire protocol, I/Q, WDSP, audio or transmit change. `frequency.py` and
`test_frequency.py` stay as they are. No new settings: the signal format, distance units and
bearing preferences already exist. The Roundtable panel on the desktop keeps its layout and gains
only the wording (D8) and the keyed gate on Tune (D22). The earlier-turns and messages areas keep
their behaviour.

## Architecture

Every client derives one structured card from its existing model, so the words and numbers are
decided in one place per client and the drawing code only draws.

* **Desktop.** `RoundtableTagLayout` gains a structured `RoundtableCardModel` and a pure placement
  function. `SpectrumWidget` paints the card and tags on both the GPU tag layer and the CPU path,
  and keeps its rule that tag updates never invalidate the static overlay. The view is a new child
  widget, `RoundtableFrequencyPeek`, that does not inherit `SpectrumOverlayPanel`, so it never
  becomes a tag keep-out. It reads through the existing `readAt` path (one reading slot plus the
  active slot; the active card stays live). Until the stream arrives it shows the directory's
  stations.
* **iPhone and iPad.** One SwiftUI layer on both. `RoundtableTagLayer` gains per-item widths and
  a new `RoundtableMeterBar`; a new `RoundtableFrequencyView` is shared by the sheet, the applet and
  the popover. `BandCoverage` gains the reverse mapping that Your pan needs. The desktop's
  `RoundtableFrequencyPeek` becomes the inspector: it stays outside `SpectrumOverlayPanel`, and its
  rect reaches the pan so tags keep clear of it. New signals stay on the GUI thread:
  `SMeterWidget::rxModeChanged` to the flags, and the flag's menu request back through `MainWindow`.
* **Web.** `roundtable.js` and `roundtable.css` gain the additions in `web.css`, under the page's
  existing Content Security Policy and with no `innerHTML`.

Nothing under `src/core/` or `src/models/` may include a GUI header (Rule R1). All colors above are
constants. No ported code: new files carry `no-port-check: NereusSDR-original.`

## Errors and edge cases

* View of a frequency whose stream has not arrived: stations from the directory, "No readings".
* View while the link drops: D7, "Reconnecting · last known". Tune stays enabled only when the radio
  connection itself allows tuning.
* Viewed frequency goes quiet: head says "Nobody talking", the caption says "Last heard 3m ago".
* Keying while viewing: Tune disables at once; the view stays open.
* Your slice moves onto the viewed frequency: the inspector becomes your own, in place.
* Three or more keyed: the head says "Double · 3" and every name shows while room allows, then
  "+N keyed".
* A callsign that does not fit is cut, never overlapped; a row never wraps.

## Acceptance and evidence

| What | Evidence |
| --- | --- |
| D1 to D9 truth rules on desktop and iOS | Unit tests on the card model, with the cases named in the plan |
| D1, D3, D9, D30 to D33 on web | `frequency-unit.js`, `probes.py` with a real double on the test server |
| D10 to D20 desktop drawing | Offscreen widget tests of placement and hit rects; focused captures looked at, not just generated |
| No hitch | `tst_spectrum_overlay_dirty` and the 1000-updates, zero-invalidations test stay green |
| D21 to D27 desktop flow | `tst_roundtable_desktop_flow`: a tag click changes no slice |
| D17 to D29 iOS | App test suites on a simulator; screenshots on an iPhone and an iPad simulator, looked at |
| D34 to D37 desktop | `tst_roundtable_desktop_flow`, `tst_roundtable_frequency_peek`, `tst_roundtable_tag_layout`; offscreen captures compared with the board, looked at |
| D38 iOS | App and UI suites on an iPhone and an iPad simulator; screenshots looked at |
| D39 to D41 | `tst_vfo_level_bar`, flag position and menu tests, remote and local; a capture looked at |
| Real radios, a real Core, a real double over the air, iPad portrait | **Pending JJ's hardware.** Not claimed by any automated check |

## Changed during the build

Places where the build settled something the decisions above read differently. Each is
JJ's to veto on the hardware run.

* **D34, two pans.** Opened on a pan without your slice, your own inspector stays there, so its tag
  closes it and Back stays in place. Your slice's own pan keeps its card meanwhile, so that frequency
  shows in two boxes, one on each pan. The inspector moves only when your slice changes pans.
* **D34, your own card.** The inspector hides your card while it shows your own frequency, so
  there is no card to click again. Your flag line, the close button or Escape closes it.
* **D35, iPhone sideways.** On your own frequency, Tune scrolls with the reports instead of
  standing at the foot, because a foot left under 100 points for the reports. Viewing another
  frequency keeps it fixed, since there it is the action that counts.
* **D36, iPad.** The ⋯ is on the floating panel and not in the column's applet, as the board
  draws it. Signal stays reachable from the column through the Tools page.
* **D34 and D37, your own frequency while viewing another (desktop).** When your frequency has a
  talker or last-talker card, the inspector hides the card and the card's rule also drops your
  compact tag, so neither shows until the view closes. D37 keeps the compact tag only when your
  frequency has no such card.
* **D35, the Viewing line's words.** iPhone and iPad still read "Viewing, not tuned · −11.4 kHz
  from slice A · LSB", which is what the board draws. The desktop reads "Viewing · +1.4 kHz".
* **D22 and D38, keyed on your own sheet (iOS).** On a server with frequency reads, your own sheet
  has no TX line and no "Draft kept" while keyed, where a legacy server's sheet has both.

## Settled from earlier choices

These follow from what JJ chose or have one sensible answer. Each is his to veto.

* Nearest within ±5 kHz replaces the iPhone's exact-frequency rule, including its old note that
  nearby RF is not evidence. Readings still come from the server's attribution, so nothing is
  inferred from level.
* "Last overlap" becomes "Last double" (D8).
* Your pan is a peak across the sideband window, not one trace bin (D23).
* Peak tick falls 2 dB a second (D12), matching the board.
* Far side is measured from the active slice's flag; other flags are keep-outs (D16).
* Tag viewing also applies to the desktop's other-slice flag line (D26).
* The Roundtable panel's layout stays (see Out of scope); the board's small read view is the
  floating panel, and Messages leads to the panel.
