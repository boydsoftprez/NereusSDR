# Remote notch control version 1

This is the wire contract for R-R3-21 and R-R3-09: the Core owns the tracking
notch filter (TNF) list, and a remote window edits it with commands. It sits
beside [remote media control version 1](2026-09-20-remote-media-control-v1.md)
and uses the existing WSS session envelope (object snapshots, property
deltas, `command.invoke` and `command.result`).

## Negotiation

Capability `notchControlVersion=1` in `StationCapabilities`, on a session
whose agreed minor is at least `kDspControlSessionProtocolMinor` (5). The
session protocol minor itself is unchanged. The Core advertises version 1
whenever it runs; it does not depend on a radio or on WDSP.

- A window that negotiated version 1 puts its `NotchModel` in mirror mode:
  the list comes only from the Core's `notches` object, and every edit is a
  `notch.*` command. The window writes no notch settings except
  `NotchVisualEnabled`, which is one Core-wide setting: a Station key the
  settings proxy carries, so every window and the phone share one value.
- A window without version 1 (an older Core, or an older agreed minor) keeps
  today's behaviour: its notches live in its own settings. Leaving mirror mode
  clears the Core's list from the window and restores the window's own saved
  list.
- An older app is refused a write or remove of a Core-owned notch setting
  with the plain reason "This Core keeps its own notch list. Update this app
  to change notches." The Core-owned keys are exactly `NotchCount`,
  `Notch<N>Center`, `Notch<N>Width`, `Notch<N>Active` (N a whole number
  written without leading zeros), `NotchGlobalEnabled` and
  `NotchAutoIncrease`, compared without regard to case. `NotchVisualEnabled`
  is not Core-owned in this sense: any window may write it through the
  settings proxy, and the Core keeps the one value.
- A `notch.*` command on an agreed minor below 5 is refused with "Update this
  app to change notches on this Core."
- Version 2 (R-IOS-27, R-IOS-06) adds `notch.addAtSlice`, below. The Core
  sends 2; the other four commands need 1, so a window that compares the
  version as a minimum reads 2 as it read 1.
- A desktop window's +TNF (R-R3-21, R-IOS-27) sends `notch.addAtSlice` for
  its pan's slice (the Core's slice id) when the Core offers version 2 or
  more, so the Core's own slice decides the centre; against version 1 it
  sends `notch.add` with the centre composed from its mirrored slice. A
  refusal on either path reaches the window's "Notch not added" notice.

## The `notches` object

The Core watches `RadioModel::notchModel()` under object key `notches`, class
`NotchModel`. An older app holds no object for this key: it records the
schema as skew and drops the object's deltas. An app from this build on logs
that once per object for each connection; an app built before the R3 audio
and DSP control batch logs one line per delta, and only notch edits produce
them.

| Property | Direction | Type | Meaning |
| --- | --- | --- | --- |
| `listJson` | Core to window | string | The whole list, as below |
| `revision` | Core to window | uint32 | Advances with every list change; compared with serial-number arithmetic across wrap |
| `globalEnabled` | both ways | bool | The TNF master switch |
| `autoIncrease` | both ways | bool | Widen notches with the filter |

`listJson` is a JSON array of at most 1024 entries (WDSP's notch capacity),
in the Core's list order, which is the WDSP notch index order:

```json
[{"id":3,"centreHz":7074000,"widthHz":100,"active":true}]
```

Each entry has exactly these four keys. `id` is a whole number from 1 to
2147483647, unique within the list; ids are the Core's session-local keys,
never persisted, and new notches get ids above every id the window holds.
`centreHz` and `widthHz` are finite numbers, the width not negative;
`active` is a bool. A window refuses a list that breaks any of these rules
and keeps its previous list.

## Commands

Each is a `command.invoke` with exactly the arguments shown and exactly
these wire kinds; anything else is refused with "This notch change is not
one this Core understands." and changes nothing.

| Verb | Arguments | Effect |
| --- | --- | --- |
| `notch.add` | `sliceId` (int64), `centreHz` (float64), `widthHz` (float64) | Adds a notch where the window's receiver `sliceId` put it, by the Core's own add rules (rounding, tuning range, the 10 Hz duplicate window, the 1024 limit) |
| `notch.move` | `id` (int64), `centreHz` (float64), `widthHz` (float64) | Moves and resizes one notch in one change: both values are checked before either applies, so a refused move changes nothing |
| `notch.setActive` | `id` (int64), `active` (bool) | Turns one notch on or off |
| `notch.delete` | `id` (int64) | Removes one notch |
| `notch.addAtSlice` | `sliceId` (int64) | Version 2. The desktop's +TNF for receiver `sliceId`: the Core composes the notch itself, 200 Hz wide at the receiver's demodulated frequency (VFO, RIT and the DIGU/DIGL click-tune offset) moved by the middle of its receive filter (Thetis TNFAdd with notchSidebandShift), the same `RadioModel::addTnfForSlice` the desktop's button runs, and adds it as `notch.add` does |

An accepted command's `command.result` names affected key `notches` and
carries `revision`, the list revision after the change; `notch.add` and
`notch.addAtSlice` also carry `id`, the new notch's id. The window keeps its own view of an edit
until the mirrored revision reaches that value, so a delta already in flight
cannot undo it.

A refused command carries a plain reason and no values:

- "That notch is no longer on this Core." (unknown id; the window drops it)
- "The notch list is being edited on the Core. Try again when it is done."
- "That notch would be outside the radio's tuning range."
- The Core's own add refusals, for example "Maximum of 1024 notches reached"
  or "A notch already exists within 10 Hz".
- "That receiver is not on this Core" (`notch.add` and `notch.addAtSlice`
  naming an unknown `sliceId`).

A dragging window sends at most one `notch.move` per 100 ms
(`NotchModel::kRemoteMoveIntervalMs`), plus a final one when the drag ends.

## Evidence

`tst_notch_add_at_slice` (`notch.addAtSlice` lands the desktop +TNF's
notch in USB, LSB, DIGU and DIGL, and each refusal; a remote window's
+TNF sends `notch.addAtSlice` to a version 2 Core and `notch.add` to a
version 1 Core, with the Core's refusal on both), `tst_station_session` (mirror mode, add, move, toggle, delete, refusals, an
older app's refused writes), `tst_tnf_ui_wiring`, `tst_notch_channel_sync`
(every command reaches every bound WDSP channel once) and
`tst_settings_scope` (the exact Core-owned key forms). Hardware acceptance is
pending.
