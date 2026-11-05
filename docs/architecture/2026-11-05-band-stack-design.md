# Band Stack Design

**Date:** 2026-11-05
**Status:** Design settled with J.J. Boyd (KG4VCF) on the mockups; implementation planned in
[2026-11-05-band-stack-plan.md](2026-11-05-band-stack-plan.md).
**Replaces:** the April draft (`docs/architecture/bandstack-design.md` at commit `4e85a5328`,
never on main). Its decisions stand unless section 3 says otherwise.
**Mockups:** [desktop](2026-11-05-band-stack-design/desktop.html),
[phone and iPad](2026-11-05-band-stack-design/phone-and-ipad.html),
[side by side on one Core](2026-11-05-band-stack-design/side-by-side.html). They are interactive
and were built from captures of the real UI. Where this text and a mockup disagree on behavior,
this text wins.

## 1. Goal

Each band keeps a list of saved spots: frequency, mode, filter, and how the pan looked. Pressing a
band button goes to your place on that band. Pressing it again saves any change to the spot you
are on and moves to the next one. The list lives on the Core, so the desktop, a remote window, the
phone and the iPad all see the same stack and each slice keeps its own place in it.

This is the feature J.J. calls "the memories". It is separate from `UnbuiltFeature::Memories`,
which stays out of scope.

## 2. What the references do, and what we chose

| Reference | What it does | What we take |
|---|---|---|
| Thetis BandStack2 (MW0LGE), `clsBandStackManager.cs`, `console.cs` `preBandSelect`, pin v2.10.3.15 `3759d09` | Sorted per-band list with a current index. Re-press writes back and advances, Shift goes back, Ctrl adds. Return modes First, LastVisited, Specific, Current. Region seeds. RX1 only. Stored in the client. | The cycle, return modes, lock and duplicate rules, region seed tables. |
| AetherSDR `BandStackPanel` (`a9a83e66`) | Panel beside the spectrum, opened from status-bar dots. A flat bookmark list; band stacking itself is delegated to Flex firmware. | The panel shell and dots. Not the data model, since OpenHPSDR radios have no firmware stack. |
| piHPSDR and deskHPSDR `band.c`, `bandstack.h` | A small fixed set of entries per band with a current entry, saved in the client's properties. | Nothing beyond confirming the same basic cycle. |

**Study, then choose.** This is a design on the Thetis basis, not a straight port, because NereusSDR
has up to four slices, several devices on one Core, and panadapters of their own. The seed tables
and the cycle's core rules are ported under the attribution rules in section 10.

### Where we differ from Thetis

1. **The stack is on the Core,** one per radio. Thetis keeps it in the client.
2. **Each slice has its own place** in one shared list. Thetis stacks RX1 only.
3. **A spot another slice is on is not saved over** when a slice leaves it. Thetis has one writer,
   and April's rule was last to leave writes. J.J.'s ruling, 2026-11-05. See section 5.3.
4. **Dragging can reorder a band.** Thetis always sorts by frequency
   (`GenerateFilteredList`, clsBandStackManager.cs:714-721 [v2.10.3.15]). New spots still go in
   frequency order.
5. **The first press on a band with no place yet lands on the first voice entry,** matching the
   choice in issue 118. Thetis uses its return mode's first entry, which is often CW.
6. **Duplicate means within 100 Hz.** Thetis compares exactly (clsBandStackManager.cs:372-386
   [v2.10.3.15]). Two spots 50 Hz apart are the same spot to an operator.
7. **Next and previous stay on the band.** Thetis's band keys walk off the end into the next band
   (console.cs:27007-27056 [v2.10.3.15]). See veto item V3.
8. **Entries do not store a sub-mode or C-Tune.** NereusSDR has no sub-mode, and C-Tune belongs to
   the pan.
9. **A press that would move a shared receiver asks first.** Thetis has no shared receivers.

## 3. Decisions

### J.J.'s decisions on the mockups, November 2026

| Topic | Decision |
|---|---|
| Where the stack lives | On the Core, one per radio, the same on every window and phone. Replaces April's client settings store. |
| Older Core | The stack shows greyed with the reason. Band buttons work as they do today. |
| Recall on a shared pan | Pan stays. Center and zoom come back only when the slice is alone on its pan. On a shared pan the view stays if the new frequency is on screen, otherwise it follows as today's band button does. |
| Right-click on a band button | A popup at the pointer with that band's rows, with the same rows and menus as the panel. Also the VFO display's long press and the flyout's right-click. |
| BAND flyout | Stays open after a press until you click elsewhere. |
| iPad | A Band Stack applet, alongside the BAND menu. |
| New spots | Go in frequency order, as Thetis. The new spot becomes your place, so the next press goes to the next higher one. On a dragged list it goes just before the first higher spot. Dwell saves do the same. |
| A press that would move a shared receiver | Asks first, with the existing "Confirm the change" dialog. A device that does not anchor takes a free receiver with no question. |
| Leaving a spot another slice is on | Does not save over it. Keeping the new frequency takes a deliberate save. |
| Keys (with the hot keys work) | M and N are the default keys for next and previous entry, plain keys so text fields keep them. Shift+M adds an entry and Shift+N opens the panel. A focused panel keeps its own keys. |
| Region change prompt | Not drawn, at J.J.'s direction ("use your judgment for that one"). Settled as designed in April; see V1. |

### April decisions that still stand

- The Thetis cycle: re-press writes back and advances, Shift goes back, Ctrl adds (Cmd on Mac).
- A grouped panel: current band open, the others collapsed with counts, sliding in from the left
  from the status-bar dots.
- Panel right-click menu with group by band, auto-expiry and auto-save dwell.
- All three return modes, per band, shared across slices.
- Spectrum overlay: ticks, lock icon, active highlight, hover tooltip, click to tune.
- Right-click, inline rename and drag-reorder, with no separate dialog.
- Duplicate protection on by default.
- Region-based seeds. A region change asks first, clears unlocked entries and re-seeds.
- Full-state recall. Today's per-band memory still restores what it holds, such as AGC, noise blanker
  and diversity. The sample rate and the tune step stay what you set on every band (J.J.'s rulings,
  2026-11-05; PR 747), so a recall changes neither.

## 4. Calls settled here, for J.J. to veto

Each of these follows from an earlier decision or has one sensible answer. Saying no to any of
them is cheap until the matching task runs.

- **V1. Region change.** When you change region in Setup on any device and that Core has band
  stacks, the device that made the change asks: "You changed the region to %1. Replace the unlocked
  entries with the defaults for %1? Your locked entries stay." with **Keep my entries** (default)
  and **Replace**. A device knows it made the change because it marks its own region edit, and asks
  when the document's `region` next changes (within 65 seconds, covering the 60 second question a
  region edit can raise). The region itself has already changed either way. Replace removes unlocked
  entries and seed entries you have not changed, then loads the new region's seeds. No slice moves.
  A slice whose place was removed has no place on that band until its next press. Other devices
  are not asked.
- **V2. Locking a seed by hand makes it yours.** It stops being a seed, so region changes keep it.
  The seeded locks on WWV and US 60 m remain seeds, so a region change replaces them.
- **V3. Next and previous wrap within the band,** like re-pressing the band button. This covers M
  and N, the STACK+ and STACK- container buttons, and the phone's arrows. Thetis's keys go on to
  the next band at the end of the list.
- **V4. Tuning across a band edge by hand** does not save into a spot and does not move your place.
  It updates that band's last frequency used.
- **V5. Dwell and expiry run on the Core,** so a Core with no window still does them. Locked and
  seeded entries never expire, and neither does a spot a slice is on. Both are off by default.
  The undo for an expiry is on the device that shows the note, for 5 seconds.
- **V6. Per device, not on the Core:** panel open or closed, panel width, group by band, show on
  the spectrum. On the Core: the stack, return modes, duplicate protection, expiry, dwell. Show on
  the spectrum is off by default.
- **V7. The panel acts on the window's selected slice,** named in its header. Ticks on a pan act on
  that pan's active slice.
- **V8. First run.** The region's seeds load. Then each slice's saved spot on each band joins the
  stack and becomes that slice's place, or takes the existing entry if one is within 100 Hz. Nothing
  moves on first connect.
- **V9. Older clients on a new Core.** `slice.selectBand` keeps its meaning, including "a band the
  slice is already on changes nothing". Only clients that know the stack can cycle. An older phone
  never starts cycling by surprise.
- **V10. Listener and another device's slice.** The rows can be read. Recall on a slice you only
  listen to is refused: "You are only listening to Slice %1. Take control of it to change its
  band." Pressing for another device's slice is refused with the Core's existing refusal text.
- **V11. While transmitting,** a press on the transmitting slice is ignored with "Band %1 ignored:
  Slice %2 is transmitting." The overlay is hidden on air. Other slices may still press.
- **V12. Transverter bands** are not stacked in this version and keep today's refusal, because
  XVTR has no seeds or frequency range in NereusSDR yet.
- **V13. Stack size.** No fixed limit. A list of a few hundred entries stays one small document.
- **V14. Pans without click tune.** A pan without click tune always sits on its slice, which is how
  tuning works today, so a recall centers it there. "Pan stays" and bringing back a saved center apply
  with click tune on. Zoom comes back on any pan the slice has to itself.
- **V15. The hosting desktop asks too.** Its own tuning does not ask today; it moves to a free receiver
  or is rolled back. A stack press from it goes through the Core's question, as J.J. chose. Its other
  tuning is not changed by this work.
- **V16. LMF seed rows go under GEN.** Thetis has an LMF band and NereusSDR does not.
- **V17. Spectrum ticks.** Right-click on a tick opens that entry's menu. A slice's marker line wins
  over a tick under it, so dragging still works.
- **V18. Phone's spectrum switch** is in the Display pill's "On this phone" and in the older Display
  sheet's "On the band", because a newer Core shows the pill and never that sheet. The phone's Setup
  page sits under General, after Options.

## 5. Behavior

### 5.1 Pressing a band button

Target slice: the slice the button already acts on today (the pan's active slice, or the window's
selected slice).

| Press | On another band | On this band |
|---|---|---|
| Plain | Save and leave (5.3), then go where the band's return mode says (5.2). | Save the spot (5.3), then go to the next entry, wrapping. |
| Shift | As plain. | Save the spot, then go to the previous entry, wrapping. |
| Ctrl (Cmd on Mac) | As plain. | Add the current spot (5.4). |

The cycle's basis is `preBandSelect`, console.cs:45621-45734 [v2.10.3.15].

### 5.2 Where a band returns you

Per band, shared by all slices, set from the band's right-click menu.

- **Current entry** (default): your place on that band. If you have none, the first voice entry
  (first LSB or USB), or the first entry if none is voice.
- **Last frequency used:** the exact frequency, mode and filter you left. It is not an entry, so
  your place on that band clears and the stack number shows "-". This is why a slice's last
  frequency per band is kept apart from the entries.
- **Specific entry:** the entry marked "Always return here". If it is deleted, the band goes back to
  Current entry.

An empty band tunes to the middle of the band with no entry created and says so: "%1: empty stack,
tuned to the middle of the band".

### 5.3 Saving when you leave a spot

Leaving means a band press, next, previous, a row click or a tick click. Your slice's current
frequency, mode, filter, center and zoom go into the spot you are on unless one of these holds:

1. The entry is locked.
2. **Another slice is on that spot.** "On" means its place on that band is this entry and it is
   currently on that band. The spot stays as saved. To keep your frequency, use "Save current spot
   here" (desktop), "Save this spot here" (phone), or add it as a new spot.
3. Duplicate protection is on and another entry on the band is within 100 Hz. Your place still
   moves; the frequency just is not written.

The last slice on a spot saves when it leaves, as usual. A saved change clears the seed mark, and
that entry stops being replaced by a region change.

"Save current spot here" writes at once, including onto a spot another slice is on. It does not
move the other slice.

### 5.4 Adding a spot

Add current spot, Ctrl on the band button and a dwell save all capture the same state. With duplicate
protection on, a spot within 100 Hz of an existing one is refused: "%1 is already in %2". The new
entry goes before the first entry with a higher frequency, or at the end, and becomes the slice's
place. A dwell save does not move your place and also skips a spot within 100 Hz in the same mode.

### 5.5 What a recall changes

1. Refuse if the slice is locked, transmitting, only listened to or another device's (section 11).
2. Save the old band's per-band memory (`materializePendingBandSettings`, `saveToSettings`).
3. Restore the new band's per-band memory with tuning left out (`restoreFromSettings(band, false)`,
   which already skips frequency, mode, filter and sample rate). Do not call
   `applyRestoredSampleRate`. The slice's tune step stays as it is: J.J. ruled on 2026-11-05 that
   there is one step per slice on every band, so a recall must not restore a step from per-band
   memory, whichever order this and the settings audit land in.
4. Set frequency first, then mode, then filter, in the order `onBandButtonClicked` documents.
5. Apply the pan rule (V14): alone on its pan, the slice brings back the entry's zoom, and with click
   tune on its center too, unless that would put the frequency off screen, in which case it centers
   on the frequency. On a shared pan with click tune on, the view stays if the frequency is on screen,
   otherwise it follows as a band change does.
6. Move the slice's place, update the entry's last-used time, and report "40m: 7.185 LSB, 5 of 6".

### 5.6 Shared receivers

A recall is a frequency change, and it takes the same path as any other:

- **You anchor the receiver and another device's slice would be left outside:** the Core holds the
  press and asks. Dialog title "Confirm the change", naming "Receiver %1: %2 to %3" and each slice it
  reaches, from the existing `ConfirmChangeDialog` on desktop and `SharedChangeSheet` on iOS. Cancel
  changes nothing, including the save into the spot you were on. Confirm runs the press. The question
  expires after 60 seconds like any other.
- **You do not anchor:** your slice takes a free receiver. Nobody is asked and the anchor is left
  alone.
- **Every receiver is in use:** the existing take-a-receiver question.

The target is worked out again when you confirm, because the stack may have changed while the
question was up.

### 5.7 Lock, expiry, region seeds

- A locked entry can be recalled, renamed, deleted and unlocked. It is never written by a leave
  or removed by expiry or by Clear unlocked.
- Seed lock rules from Thetis: WWV entries always, and 60 m entries in the United States
  (`addBSObjectToEntries`, clsBandStackManager.cs:2001-2002 [v2.10.3.15]).
- Region mapping, from `addStandardFrequencies` (clsBandStackManager.cs:1084-1158 [v2.10.3.15]). Every
  region also gets the shortwave list (`AddBandStackSWL`):

| NereusSDR region | Thetis routine |
|---|---|
| United States | `AddRegion2BandStack(true)` plus `AddUS_PlusBandStack` |
| Australia, IARU Region 2 | `AddRegion2BandStack` |
| Japan, IARU Region 3 | `AddRegion3BandStack` |
| United Kingdom | `AddUK_PlusBandStack` |
| Sweden | `AddSwedenBandStack` |
| Every other region (Europe, IARU Region 1 and the European national regions, India, Israel, Russia, Italy) | `AddRegion1BandStack` |

NereusSDR's `BandPlanRegion` index is not Thetis's `FRSRegion` number, so the mapping is by name.

## 6. Architecture

### 6.1 Where the logic lives

- **`src/models/bandstack/BandStackStore.{h,cpp}`:** plain data and rules with no Qt widgets and no
  link code: entries, per-band order, return modes, each slice's place. It decides what a press
  means and returns a result (`go to X`, `add`, `refuse`). It does not touch slices. Tested on its own.
- **`src/models/bandstack/BandStackSeeds.{h,cpp}`:** ported Thetis seed tables by region.
- **`src/models/bandstack/BandStackController.{h,cpp}`:** a child of the Core's `RadioModel`
  (`Role::Local`). Connects the store to slices: tracks last-used state, performs recalls (5.5), runs
  dwell and expiry, loads and saves.
- **`src/core/session/BandStackFacade.{h,cpp}`:** the `json` and `revision` document every peer mirrors.
  Follows `PaProfilesFacade`.
- **`src/models/bandstack/BandStackAccess.h`:** the interface the GUI talks to, so panels do not care
  whether the Core is local. `LocalBandStackAccess` calls the controller and `RemoteBandStackAccess`
  sends verbs. `RadioModel::bandStack()` returns the right one.

Rule R1 holds: nothing under `src/core/` or `src/models/` includes a GUI header. All of this runs on
the main thread. The stack never touches the I/Q, WDSP or audio path; a recall uses the same slice
setters a band button uses today.

### 6.2 Storage

One versioned JSON document, `hardware/<mac>/bandstack/Document`, in Station scope through the existing
`hardware/` prefix (`SettingsScope.cpp`). Saved at most once a second after a change and at shutdown.
A document that fails to parse is logged with `qCWarning`, set aside under `Document.bad`, and
replaced by fresh seeds.

The document, with bands named by `bandKeyName`:

```json
{
  "schema": 1,
  "region": "United States",
  "options": {"dupeProtection": true, "expiryMinutes": 0, "dwellSeconds": 0},
  "bands": {"40m": {"order": ["<id>"], "returnMode": "current", "specificId": ""}},
  "entries": [{"id": "<uuid>", "band": "40m", "frequencyHz": 7185000, "mode": "LSB",
               "filterLowHz": -2800, "filterHighHz": -100, "centerHz": 7185000, "spanHz": 192000,
               "locked": false, "seeded": true, "name": "", "lastUsedMs": 0}],
  "places": {"A": {"40m": "<id>"}},
  "lastExpiry": {"batchId": "", "count": 0},
  "lastRecall": {"seq": 0, "sliceId": -1, "applyView": false, "centerHz": 0, "spanHz": 0}
}
```

`returnMode` is `current`, `lastUsed` or `specific`. `expiryMinutes` is 0, 5, 15, 30 or 60 and
`dwellSeconds` is 0, 10, 30 or 60; both default to 0, and `dupeProtection` defaults to true. `spanHz` 0
means no saved zoom. `lastExpiry` and `lastRecall` are published but not saved. A slice's last frequency
per band comes from its existing per-band memory.

The Core has no pan object: a pan's view belongs to each window. So a device sends its own view with a
press, and the Core decides and publishes the outcome in `lastRecall`. The window or phone controlling
that slice applies it to its own view and every other device ignores it.

Seed rows name a filter as F1 to F10 as Thetis does. They resolve to low and high Hz through
NereusSDR's per-mode preset table when the seeds load.

### 6.3 The link

Capability `bandStackVersion` = 1, and a link feature `"bandStack": 1` so only peers that know the
document receive it. The object key is `bandStack`, carrying `json` and `revision`. Slice places are
in the document, so every device can draw every slice's marker.

| Verb | Arguments | Notes |
|---|---|---|
| `bandStack.press` | `sliceId`, `band` (key), `direction` (next, previous), view | Runs 5.1. May be held by 5.6. |
| `bandStack.step` | `sliceId`, `direction`, view | Next or previous on the slice's own band. May be held. |
| `bandStack.goTo` | `sliceId`, `entryId`, view | Row or tick. May be held. |
| `bandStack.add` | `sliceId`, view | Add current spot. |
| `bandStack.saveHere` | `sliceId`, `entryId`, view | Deliberate save. |
| `bandStack.edit` | `entryId`, optional `name`, `locked` | |
| `bandStack.delete` | `entryId` | |
| `bandStack.reorder` | `band`, `ids` | Ids must be exactly that band's entries. |
| `bandStack.setReturn` | `band`, `mode`, optional `entryId` | |
| `bandStack.clear` | optional `band` | Removes unlocked entries. |
| `bandStack.reseed` | none | Region reset, using the Core's current region. |
| `bandStack.options` | any of `dupeProtection`, `expiryMinutes`, `dwellSeconds` | |
| `bandStack.restoreExpired` | `batchId` | Undo. Valid for 5 seconds after the batch. |

"view" is the optional `viewCenterHz` and `viewSpanHz`. Where they are missing (a dwell save, an older
path), the entry keeps its stored values.

Results and refusals use `commandResult`. Expired-entry notes reach devices through the document's
`lastExpiry` field (`batchId`, `count`), so every device can show it. Slice-owning verbs use
`changeRefusal` and `mayChange`. `freezeRefusalFor` covers the on-air case.

`slice.selectBand` is unchanged. It is what an older client uses.

The shared-receiver question uses `checkPanMove`, `askPanMove` and the `panMove` confirm kind. A
recall works out its target frequency first without changing the store (`planRecall`), and only
proceeds once the check says apply or the question comes back confirmed. It is rechecked at proceed, so a
changed stack gets a fresh target. A retune the allocator refuses leaves the store unchanged.

The hosting desktop reaches the same check through `kHostVerbs` and `HostingSliceActions`.

The verb and capability tables in
[2026-09-23-station-link-v1.md](2026-09-23-station-link-v1.md) are generated; section 6.3 of that
document gets a prose entry, and `tests/data/link/v1/surface.json` is regenerated.

### 6.4 Older and newer

| Core | Client | Result |
|---|---|---|
| New | New | Everything here. |
| New | Older | `slice.selectBand` as today. Its changes update last frequency used, and its slice's place does not move. |
| Older | New | Band buttons as today. Panel, popup, dots, overlay and phone block are visible and greyed with "This Core does not have band stacks yet. Updating the Core adds them." |
| None (not connected) | New | Greyed: "Connect to a radio to use band stacks." |

## 7. Desktop

Mockup: [desktop.html](2026-11-05-band-stack-design/desktop.html).

- **Status bar.** `statusBandStackDots` opens and closes the panel. The stack number reads "40m 5/6",
  "-" with no place. Band menu "Band Stacking..." does the same as the dots.
  `UnbuiltFeature::BandStack` is retired, and the reasons in `containerBandStackReason` go with it.
- **Panel.** Opens left of the pan stack, right of any left dock strip. Header: "BAND STACK", the
  selected slice's flag, "on 40m", and a "..." menu. Bands are grouped, with the current band open and
  the others collapsed showing counts and locks. Rows show frequency, mode and filter width, name,
  lock, and the flag of any slice on that spot. Tint follows the band plan segment. "+ Add current
  spot" sits at the foot of the current band.
- **Row actions.** Click to go there. Double-click the name to rename. Drag within a band to reorder.
  F2 rename, Cmd/Ctrl+U lock, Delete deletes, arrows and Enter move and recall, Esc closes.
- **Row menu.** Go there, Save current spot here (disabled when locked or off the band), Rename,
  Lock/Unlock, Always return here, Delete.
- **Band menu.** Return to (Current entry, Last frequency used, Specific entry), Collapse/Expand,
  Clear unlocked %1 entries (asks).
- **Panel menu.** Group by band, Auto-expiry, Auto-save dwell, Don't save over a frequency already
  here, Show on the spectrum, Reset to region defaults... (asks), Clear unlocked entries... (asks).
- **Popup.** Same rows and menus, at the pointer, for one band. From a band button's right-click (in
  the flyout and in containers) and the VFO display's long press.
- **Flyout.** `SpectrumOverlayPanel`'s BAND flyout stays open after a press, as J.J. chose, and closes
  on a click elsewhere.
- **Overlay.** Per pan, from the shared per-window switch. A tick and a dot per entry in view, a lock
  mark, active highlighted, frequency and mode label, tooltip on hover ("frequency, mode and filter,
  N of M, name"). Clicks within 4 px recall; any other click tunes as today. Hidden on air. It draws
  in the overlay layer and does not change the spectrum shaders.
- **Container buttons.** `BandButtonItem` right-click opens the popup. `VfoDisplayItem` long press
  opens the popup. STACK+ and STACK- step.
- **Actions for hot keys.** `bandStackNext`, `bandStackPrevious`, `bandStackAdd`, `bandStackPanel` are
  public entry points on `MainWindow`. The hot keys work binds M, N, Shift+M and Shift+N to them.

A disabled control stays visible, with the reason as its tooltip. Slice names in strings are "Slice A",
never RX1.

## 8. iPhone and iPad

Mockup: [phone-and-ipad.html](2026-11-05-band-stack-design/phone-and-ipad.html).

- **Where.** A stack block under the band grid in the Pan sheet (`PanSheet`) and in the column's BAND
  menu (`BandColumnMenu`), which stays open. Sideways the grid and block sit side by side; on iPad
  stacked, plus a Band Stack applet.
- **Controls.** Tap the lit band for next. "‹" and "›" replace Shift. "40m ▾" picks a band's list, with
  a half-second hold on a band as an extra way in, never the only one. Each row's "..." menu: Save this
  spot here, Rename, Lock/Unlock, Always return here, Delete. "+ Add this spot". Reorder is a mode with
  grips, never a hidden gesture. A band "..." menu for return mode and clear.
- **Spectrum.** Marker chips at the spectrum foot, tap to go. A per-device Band stack button in the
  Display sheet's "On the band" group, default off.
- **Setup.** A page "Band Stack" marked as this Core's: Auto-save, Auto-expiry, duplicate protection,
  Reset to region defaults, Clear unlocked entries.
- **Older Core.** The grid works as today; a greyed "Needs a newer Core" row carries the reason.

The phone does not get hot keys in this work.

## 9. Interaction with the hot keys work

The hot-keys session owns the action list and default bindings. This work provides the four entry
points in section 7, whichever branch lands first. If hot keys is not on main when this lands, the
entry points remain and are reached by the panel, the band buttons and the container buttons.

## 10. Attribution

- `BandStackStore.{h,cpp}` carries the rules from `clsBandStackManager.cs` and `console.cs`.
  `BandStackSeeds.{h,cpp}` carries the tables from `clsBandStackManager.cs`. Each file gets the upstream
  headers byte for byte under `// --- From [filename] ---`, including the Samphire dual-licence block
  (clsBandStackManager.cs:28-39, console.cs:39-48 [v2.10.3.15]), plus a NereusSDR modification history.
- Inline `//` comments and developer tags inside ported logic are copied verbatim. Every block and
  constant carries `// From Thetis file:line [v2.10.3.15]`. `0` tolerance in Thetis versus our 100 Hz
  gets a note saying why ours differs.
- New provenance rows in `docs/attribution/THETIS-PROVENANCE.md` in the same commit.
- AetherSDR's panel is studied, not copied, so the panel code carries a study note and no port header.
- `scripts/verify-thetis-headers.py` and `scripts/verify-inline-tag-preservation.py` must pass.

## 11. Refusals and failures

| Situation | What happens |
|---|---|
| Slice locked | "Band %1 ignored: the slice is locked. Unlock it to change bands." |
| Slice transmitting | "Band %1 ignored: Slice %2 is transmitting." |
| Only listening | "You are only listening to Slice %1. Take control of it to change its band." |
| Another device's slice | The Core's `changeRefusal` text. |
| Duplicate on add | "%1 is already in %2" |
| Unknown entry (deleted on another device) | Refused without a change; the sender's mirror is already correct by then. |
| Older client or Core | Section 6.4. |
| Corrupt document | Section 6.2. |
| Link drops mid-question | The question lapses and nothing changes. |

A local window's refusal uses `bandClickIgnored`; a remote window's comes back through `commandResult`
and reaches the same place.

## 12. Acceptance

**Software (unit and integration on this machine):**

1. Store: next and previous wrap, for each return mode and with no place; first press lands on first
   voice entry.
2. Store: leaving saves; a locked entry does not; a spot another slice is on does not, in both orders;
   the last slice on the spot does; a duplicate within 100 Hz does not but the place still moves.
3. Store: deliberate save writes onto a shared spot and leaves the other slice's frequency alone.
4. Store: add goes in frequency order, on a dragged list too; reorder refuses a list that is not
   exactly the band's entries.
5. Seeds: row counts and sample rows per routine match the Thetis tables; WWV and US 60 m are locked;
   every NereusSDR region maps to a routine.
6. Controller: recall restores AGC, noise blanker and diversity from per-band memory, applies the
   entry's frequency, mode and filter, and leaves the sample rate and tune step unchanged.
7. Controller: first run keeps slices where they are and makes their saved spots their places.
8. Controller: dwell and expiry follow V5; undo restores the batch.
9. Link: all verbs through `SessionCommandDispatcher` with refusals; `tst_link_surface_manifest` and
   `render-link-tables.py --check` pass; a peer without the feature receives no document.
10. Link: a press leaving a shared window gets `needsConfirmation`, Cancel leaves slices and store
    untouched, Confirm runs it, a non-anchor is not asked.
11. `slice.selectBand` on a new Core is unchanged, including same-band no-op.
12. A new client on an older Core shows the greyed state and band buttons still work.
13. Persistence round trip under `hardware/<mac>/`, isolation between MACs, corrupt document handled.
14. Rule R1, `verify-thetis-headers.py`, `verify-inline-tag-preservation.py` pass.

**UI (offscreen captures and the iOS shot and UI tests):**

15. Panel, popup, menus, overlay, flyout staying open, and the greyed state, on desktop.
16. Stack block in the Pan sheet and BAND menu, sideways, iPad applet, marker chips, Setup page, Reorder
    mode and the older-Core row on iOS.

**Hardware, with J.J. at the radio. Not called ready until seen:**

17. On a real radio, band press, re-press, Shift, Ctrl and a row click retune and restore as designed,
    with the radio following.
18. Desktop and phone on one Core: add on each, see it on the other; step the phone with the desktop's
    place untouched; the confirm dialog on a press that would move the phone's slice.
19. Leave a spot the phone is on and see it stay as saved.
20. Restart the Core and find the stack and each slice's place intact.
21. An older Core or older phone behaves per section 6.4.

## 13. Out of scope

Transverter bands. Search in the panel. Multi-select. Import and export. Reading Thetis's `database.xml`.
A favorites star. Dragging between bands. A return mode per slice. Thetis's hide-on-select option.
`UnbuiltFeature::Memories`.
