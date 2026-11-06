# Band Stack Hardware Run

**Date:** 2026-11-06
**Status:** Everything below is pending. Nothing here counts as seen until J.J. Boyd (KG4VCF) has
seen it on the real gear and written the result next to the scenario.
**Spec:** [2026-11-05-band-stack-design.md](2026-11-05-band-stack-design.md), section 12, items 17 to 21,
plus the extra checks that the build turned up.

Each scenario says what to do and what you should see. Mark each one Pass, Fail or Unsure, and
add a note for anything odd.

## Part 1. The spec's hardware items

### 1. Band presses and row clicks on a real radio (spec item 17)

Status: pending

Do: With a radio connected, press a band button, press the same one again, press it with Shift,
press it with Ctrl, and click a row in the stack.

See: Each one retunes the slice and restores its mode, filter, center and zoom as the spec describes,
and the radio follows by ear and on the display. A second press steps to the next spot; Shift goes
the other way; Ctrl does what the spec says for it. A row click goes straight to that spot.

### 2. Desktop and phone on one Core (spec item 18)

Status: pending

Do: Connect the desktop window and the phone to the same Core. Add a spot from the desktop, then
one from the phone. Then step through the stack on the phone. Then press a band on the desktop that
would move the phone's slice.

See: Each added spot shows up on the other device. Stepping on the phone leaves the desktop's place
alone. The press that would move the phone's slice asks first, and nothing moves until you answer.

### 3. Leaving a spot the phone is on (spec item 19)

Status: pending

Do: Put the phone's slice on a spot. On the desktop, tune a slice onto that same spot, change its
frequency, then leave the spot with a band press.

See: The spot keeps what was saved before. The phone's place is not disturbed. "Save current spot
here" writes at once, even onto a spot the phone is on, and does not move the phone.

### 4. Restarting the Core (spec item 20)

Status: pending

Do: Set up a few spots and leave Slice A and Slice B on different bands. Restart the Core and
reconnect.

See: The stack is intact, and each slice comes back to its own place.

### 5. An older Core or older phone (spec item 21)

Status: pending

Do: Connect the new phone to an older Core. Then connect an older phone to the new Core.

See: With the older Core, the panel, popup, dots, overlay and the phone's block are visible but
greyed, and they say "This Core does not have band stacks. Updating the Core may help." Band buttons
work as they did before. With the older phone on the new Core, its band buttons still work as before,
and its changes update the last frequency used without moving its slice's place.

## Part 2. Checks the plan added

### 6. Animation cost on real panadapters

Status: pending

Do: Open the panel and close it several times with real panadapters running, with more than one
panadapter showing.

See: The spectrum keeps moving smoothly while the panel animates. No stutter, no dropped audio.

### 7. Overlay ticks in GPU rendering on a native build

Status: pending

Do: On a native build with GPU rendering on, look at the saved-spot ticks on the spectrum. Click a
tick.

See: The ticks are drawn crisply in the right places. Clicking one tunes to that spot. A tick within
8 pixels of a slice's center line cannot be clicked, because the center line wins; that is the
intended rule, so check it feels right.

### 8. Right-click on real container buttons

Status: pending

Do: Right-click the band container buttons, including STACK+ and STACK-.

See: Each right-click opens the panel (the popup is not built to open from here yet). Say whether
this is what you expect.

### 9. Free containers stay put when the panel opens and closes

Status: pending

Do: Place free-floating containers, some anchored to the right side. Open and close the panel a few
times, including right after the window opens.

See: Every container stays where it was, and none jumps or ends up off its spot. A narrow pane
returns to its width when the panel closes.

### 10. Remote click tune: saved center versus what the Core's receiver can see

Status: pending

Do: From a remote window, save a spot with a center that is near the edge of what the Core's
receiver covers. Click a tick or a row for it.

See: The tune lands on the saved frequency, and the view shows it. If the saved center is outside
what the receiver can see, say what the screen does.

### 11. Remote arrival order

Status: pending

Do: Recall a spot from the Core's own window. Watch the phone and a remote window.

See: Both land on the saved center after the recall, not on a stale frequency that jumps a moment
later.

### 12. Another device's region change inside this window's 65 seconds

Status: pending

Do: Change the region on one device, and within about a minute look at a second device's window.

See: The second window picks up the change and does not ask a stale question or apply the old
region. Say whether the timing feels right.

### 13. Phone chips under a folded slice tag

Status: pending

Do: On the phone, fold the slice tag so it is compact, and look at the band stack chips.

See: The chips are readable and tappable, and none is hidden behind the tag.

### 14. iPad: Display pill scrolls under a real finger

Status: pending

Do: On the iPad, open the Display pill and drag with a finger to reach the Band stack switch.

See: The pill scrolls and the switch can be reached and flipped. The simulated drag did not scroll
it, so this is the real test.

### 15. iPad and phone: Rename, Reorder and Clear

Status: pending

Do: On a real iPad and a real phone, rename a spot inline, reorder spots, and use Clear.

See: Rename edits in place and keeps what you typed. Reorder moves the spot where you drop it. The
Clear question has no Cancel button, so say whether the way out is clear and whether you can back
out without losing the stack.

### 16. The Setup page's region prompt against a real Core

Status: pending

Do: In Setup on the phone, change the region while connected to a real Core, and wait for the
Core's 60 second question.

See: The prompt appears after the edit and says what it will do. The Core's own 60 second question
shows and times out as designed, and the answer you give is the one that applies.

### 17. Hand tuning into a band, then pressing elsewhere

Status: pending

Do: Tune by hand into a band you have a saved spot in. Then press a band button or a row somewhere
else.

See: Your hand-tuned frequency is saved over your place in that band, as spec section 5.3 says. Try
it and say whether that feels right, since it replaces what was saved there.

## Layout problems to look at

Status: pending

1. On the phone held sideways, the marker note sits behind Slice A's flag.
2. The PTT button overlaps the zoom box when the BAND menu is open.
3. A chip can sit under the column.

For each, say what you see and whether it needs fixing before release.
