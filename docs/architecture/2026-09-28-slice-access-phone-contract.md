# Slice access: contract note for the phone crew

This note is for the crew that builds the phone's side of slice control
and shared listening. It carries no phone code and no phone layout. The
wire is final on the Core branch, and every name below is the name in
`tests/data/link/v1/surface.json`. The authority for each is the
[station link](2026-09-23-station-link-v1.md) (sections 6.2, 6.3, 7.1, 7.5,
9.1 and 18) and the
[design](2026-09-28-slice-control-and-listening-design.md); this note says
where to look and what the phone must not get wrong.

## What the phone declares and receives

- **Hello feature.** The phone declares `sliceAccess` 3 in the `features` of
  its hello (2 before the Core's own slice could be taken, 1 before Take it
  back on `controlTaken`; a phone that still declares 1 or 2 keeps exactly
  the wire it had). It declares it only together
  with `sessionHolder` 1 (and so `deviceAuth` 1). Without both, the Core
  treats it as not declared.
- **Capability.** A phone that declared it is sent `sliceAccessVersion` in
  the Core's capabilities, appended after `radioAntennaRowsVersion` and
  before `coreBuildInfo`: the lower of the Core's version and the one the
  phone declared. 3 means the Core's own slice can be taken (below,
  "Taking the Core's own slice"); 2 means the `controlTaken` notice offers
  Take it back (below); 1 is a Core from before that. A Core that does not offer the
  feature sends nothing, and the phone reads 0.
- **Gate.** Two keys, both required (link 6.2): agreed protocol minor 11 or
  later, and `sliceAccessVersion` 1 or more. Every verb below carries
  `minMinor` 11. A phone that lacks either key sees exactly today's wire and
  never sends these verbs.
- **What the gate turns on.** The `SliceAccess` object per slice
  (`access:<id>`), the slices this device joined as `slice:<id>` and every
  other as `marker:<id>`, the five verbs, `setActiveSliceById` on any joined
  slice, and the `controlTaken` notice.

## The `SliceAccess` class

One object per live slice, key `access:<id>`, all properties `outbound`
except the first two, which are `constantSnapshot`:

| Property | Kind | Meaning |
| --- | --- | --- |
| `sliceId` | `i64` | Which slice |
| `incarnation` | `i64` | Never 0, never reused while the Core runs; a slice closed and made again gets a new one |
| `controllerDeviceId` | `utf8` | The controller's device id, empty for nobody, `station` for the Core's own operating position |
| `controlRevision` | `i64` | 1 when made, one more on each change of controller |
| `listenerDeviceIds` | `utf8` | JSON array of every joined device id, controller first, then in join order |
| `activeRxDeviceIds` | `utf8` | JSON array of devices whose active receive slice this is |
| `txSelected` | `bool` | The slice is the transmit slice and its owner holds transmit |
| `onAir` | `bool` | The slice is transmitting now |

A change of controller, a join or a leave reaches the phone as
`object.destroy` and `object.create` (`slice:<id>` to `marker:<id>` or back)
when the phone's own form changed, and as an `access:<id>` delta only when
it did not. Show state from these objects. Do not infer it from a command
result.

## The five verbs and the extended select

Every verb names the slice by `sliceId` and `incarnation`, both read from
`access:<id>`. All are in the link's section 9.1.

| Verb | Arguments | Result |
| --- | --- | --- |
| `slice.listen` | `sliceId`, `incarnation` | Joins the slice. Allocates no slice, receiver or DDC, so it works when every receiver and the slice cap are in use. `controlRevision` in `values` |
| `slice.stopListening` | `sliceId`, `incarnation` | Leaves the slice. Refused from the controller (use release) |
| `slice.takeControl` | `sliceId`, `incarnation`, `controlRevision` | Moves control in one change. The former controller stays a listener (an older app that cannot listen loses the slice). Refused only while the slice transmits. `controlRevision` in `values` |
| `slice.release` | `sliceId`, `incarnation`, `controlRevision` | Controller only. The controller is cleared and leaves. The slice stays for its other listeners, or closes when nobody is left |
| `slice.setListenLevel` | `sliceId`, `incarnation`, `level`, `muted` | This device's own level (`f64`, 0 to 1) and mute (`bool`) for a slice it listens to |

Take and release carry the `controlRevision` the phone saw. Of two devices
that saw the same one, the first is applied and the other is refused.

**Extended select.** `setActiveSliceById` from a phone with the feature makes
any slice it has joined its active receive slice. Only a slice it controls
also becomes its active slice for tuning. It never releases or leaves the
slices the phone joined earlier, and it does not change the phone's transmit
selection.

**Keys the phone must not send.** Nothing writes to a `marker:<id>` object.
A write to a listened slice's `slice:<id>` is refused with the controller's
name (see the refusal words below).

## The `controlTaken` notice

`notice` kind `controlTaken`, sent to a device whose slice another device took
while it stays listening. It carries who took it (`byDeviceId`, `byName`,
`byShortName`, `byKind`, `bySource`), the slice in `slices`, and this text
as sent:

> <taker's name> took control of slice <letter>. You are still listening.

The whole notice, as a phone at `sliceAccessVersion` 2 receives it (the values
are an example):

```json
{"type": "notice", "id": 41, "kind": "controlTaken",
 "reason": "iPad took control of slice A. You are still listening.",
 "secondsAgo": 0, "takeBack": true,
 "byDeviceId": "<taker's wire id>", "byName": "iPad", "byShortName": "iPad",
 "byKind": "tablet", "bySource": "device",
 "slices": [{"sliceId": 0, "letter": "A", "frequencyHz": 7074000,
             "mode": 1, "band": 5, "incarnation": 3,
             "controlRevision": 12}]}
```

**Take it back, one tap.** With `sliceAccessVersion` 2 and `takeBack` true,
show Take it back on the notice. The tap sends `notice.takeBack {id}` with
the notice's `id` (`sessionHolderVersion` 1, as for the other notices). The
Core runs `slice.takeControl` for the phone with the slice entry's
`sliceId`, `incarnation` and `controlRevision` (the revision after the
take), so a phone may equally send `slice.takeControl` with those three
values itself; the Core treats both the same. There is no question to
answer. The result:

- accepted, with the slice's new `controlRevision` in `values`
  (`controlRevision`, `i64`) and the objects it reached; the phone is the
  controller again and the device that had it is sent its own
  `controlTaken`, with Take it back;
- refused in `slice.takeControl`'s words when the slice is transmitting
  ("Slice <letter> is transmitting. Take control once it stops."; the tap
  may be tried again), was closed, or its control moved on since the notice
  (someone else took it). After a closed slice or moved-on control, and after
  a take-back that worked, the same `id` is refused with "That can no longer
  be taken back."

Keep the notice's card up after a refusal that may be tried again (the
slice transmitting): the slice is still the one the entry names, at the
entry's `controlRevision`, and the Core still holds the take-back. Take the
card down when the take-back works, and after any other refusal.

Transmit never moves with a take-back: the phone does not hold transmit or
choose the slice for it by taking control back, and its first key on the
slice is refused until it picks the slice with `tx.setTxSlice`, exactly as
after `slice.takeControl`. The device that had the slice loses its transmit
choice of it as on any take.

On a Core that sends `sliceAccessVersion` 1, `takeBack` is false on this
notice and `notice.takeBack` for it is refused. Show Take it back disabled,
never hidden, with the reason "This Core cannot give control back from
here. Updating the Core may help." A notice that arrives with `takeBack`
false from a Core at 2 (its take-back ended while the phone was away past
its 3 minutes) shows it disabled with "That can no longer be taken back."

The hosting desktop's own slices pass the same way: a phone may take a
slice the desktop controls (the desktop is told and can take it back),
under the same rules.

## Taking the Core's own slice (`sliceAccessVersion` 3)

JJ, 2026-09-30: with nobody at the Core's desktop, a device may take the
Core's own slice. The phone learns this from the Core, never by guessing:

- **The signal.** `sliceAccessVersion` in the Core's capabilities (`i64`,
  the lower of the Core's version and the one the phone declared). Per
  slice, the phone reads `controllerDeviceId` of `access:<id>`: `station`
  is the Core's own position.
- **At 3.** Take control is offered on every slice this phone does not
  control, a `station` slice included; the one refusal is while the slice
  is transmitting (`onAir` true on `access:<id>`: show Take control
  disabled with "Slice <letter> is transmitting. Take control once it
  stops."). The Core decides on the tap: with nobody at its desktop
  (a Core with no desktop window, such as the Rock's headless Core) the
  take goes through at once, with no question and nobody told; on a Core
  hosted by a desktop window, the desktop is the slice's controller and
  the take follows the take-over rules above (the desktop is told with
  `controlTaken` and can take it back in one tap). Every other rule holds:
  on the air it is refused ("Slice <letter> is transmitting. Take control
  once it stops."), transmit does not move with it, and the phone's first
  key on it needs `tx.setTxSlice`, as after any take.
- **Below 3.** The Core refuses the take of a `station` slice with nobody
  at its desktop: "Slice <letter> is run by the Core itself, so control of
  it cannot pass to this device." The phone shows Take control on a
  `station` slice disabled, never hidden, with those words. A `station`
  slice the Core keeps for an away device (its `marker:<id>` names that
  device) may be taken at any version.
- **Wire shape.** Nothing else is new: the same `slice.takeControl
  {sliceId, incarnation, controlRevision}` and the same result
  (`controlRevision` in `values`). After the take, `access:<id>` names the
  phone as `controllerDeviceId`, and `station` stays in
  `listenerDeviceIds`.

The `sliceClosed` notice (no Take it back) also now reaches a slice's
listeners when a take or a pan move closes it.

## Naming the station device

JJ, 2026-09-30: when a person is at the Core's desktop, every client names
the id `station` by the desktop's own device name, for example "Shack
desktop". "The Core" (or "the Core itself") is only for a headless Core.

- **The rule.** When `controllerDeviceId` of `access:<id>`, or an entry of
  its `listenerDeviceIds`, is `station`, show the `name` (or `shortName`)
  of the `connectedDevices` entry with `hostsCore: true`. With no such
  entry, say the Core.
- **Select it by `hostsCore`, never by id.** That entry's `deviceId` on the
  wire is base64url("station"), `c3RhdGlvbg`, not `station`, so looking the
  id `station` up in `connectedDevices` never matches.
- **The station marker.** A `marker:<id>` for a slice the station device
  holds sends no `ownerDeviceId`, only `ownerKind` `station`, and a slice
  nobody controls sends the same marker. The marker alone cannot tell the
  two apart; the `access:<id>` entry decides. When its `controllerDeviceId`
  is `station`, it is the Core's desktop, named the same way, on the
  spectrum marker as in the slice list. When it names no controller, or
  there is no entry, nobody controls the slice: "Available to control".
- **A slice kept for an away device.** Its `access:<id>` names `station`
  and its `marker:<id>` names the away device: name that device, never the
  Core.
- **The same words as the Core.** The Core's own refusals name the
  hosting desktop the same way (the device's name, and "the Core" only with
  nobody at its desktop).

Nothing on the wire changes, and no version moves.

## Capacity results

- **A refused `addSlice` or `addSliceOnPan`.** From a phone with the feature it
  carries `usableSlices` in `values` (`utf8`): a JSON array, one entry per live
  slice in id order, `{sliceId, incarnation, letter, controllerDeviceId}`
  (`controllerDeviceId` empty for a slice nobody controls). Offer these as
  "listen in" choices with `slice.listen`, next to the existing take question.
- **Questions.** In a `panMove`, `takeReceiver` or `takeSlice` question sent to
  a phone with the feature, every slice named, in `affected` and in a choice,
  also carries `listenerDeviceIds` (controller first). Show who else is
  listening, not only who controls.
- **Asked again.** When a proceed reaches a slice with a listener the question
  did not name, the Core asks again (`needsConfirmation`). A listener who left
  in the meantime is no reason to ask again.

## Local level: the phone's volume never writes shared state

This is settled. Slice AF is applied per listener in NereusSDR's own mixer,
and the receive channel's WDSP panel gain stays fixed at 1.0. The consequences
for the phone:

- The phone's volume and mute for a slice it listens to are sent with
  `slice.setListenLevel`. They change only what the phone hears. They never
  write the slice's shared AF gain or mute, which belong to its controller.
- Two listeners of one slice each hear it at their own level. Nobody's
  volume or mute changes anyone else's audio, the controller's included.
- A new listener, and a former controller that stays on as a listener, start
  at the slice's AF gain at that moment.
- The Core does not publish per-device levels. The phone holds its own level
  for each slice it listens to.
- From a device not listening to the slice, the verb is refused (below). A
  level outside 0 to 1, or not a number, is refused as unreadable.

Known gap: `slice.setListenLevel` has no session conformance fixture in
`tests/data/link/v1/`. The final review's fix wave adds it. Until it lands,
the phone has the link document and `surface.json` for this verb and no
recorded trace.

## Refusal words: show them as sent

The Core's refusal reasons are plain operator words. The phone shows the
`reason` string from the result as received. It does not rewrite them, add a
code table of its own or map them to other text. The holder is named in every
refusal that has one: the device's own name, else the plain word for its kind
("a phone", "a tablet", "a computer"), else "another device", and "the Core"
only when nobody or the Core's own position holds the slice. Each string
below is copied as sent (`StationServer.cpp`, `SliceAccessController.cpp`,
`TxRefusal.cpp`); `<letter>` and `<name>` are filled in by the Core.

A write or slice verb from a device that only listens:

- "Slice <letter> is controlled by <name>. Take control to change it."
- "Nobody controls slice <letter>. Take control to change it."
- An older window that does not share slices gets: "That slice belongs to
  <name>. It can be changed only there."

The attenuator and preamp (TX rulings, JJ 2026-09-30): the `stepAtt`
properties that set the receive level act on the slice a device is shown.
They are, for either ADC:

- the attenuator and its on/off: `enabled`, `attenuationDb`,
  `rx2StepAttEnabled`, `rx2AttenuationDb`;
- the preamp: `preampMode`, `rx1Preamp`, `rx2PreampMode`;
- auto-attenuation: `autoAttEnabled`, `autoAttMode`, `autoAttUndo`,
  `autoAttUndoDelayMs`, `autoAttHoldMs`, `rx2AutoAttEnabled`,
  `rx2AutoAttUndo`, `rx2AutoAttUndoDelayMs`.

A `stepAtt` write carries no slice id, so the Core judges the shown slice as
the device's single chosen receive slice (the one whose `activeRxDeviceIds`
lists it, set by `setActiveSliceById`). While that slice is one the device
only listens to, the Core refuses those writes with the listener words above,
naming that slice. The transmit settings (`attOnTxEnabled`, `attOnTxValue`,
`forceAttWhenPsOff`) are not the slice's and are not refused this way.

The phone greys these controls whenever the slice it shows is one it listens
to (it is in the slice's `listenerDeviceIds` and is not the controller), with
the same reason as its other held controls, and treats the Core's refusal the
same way. It keeps its chosen receive slice (`setActiveSliceById`) the slice it
shows, so the Core and the phone judge the same slice. Nothing new is sent on
the wire.

The verbs:

- `slice.stopListening` from the controller: "You control slice <letter>. Use
  Release to leave it."
- `slice.release` from anyone else: "Only the device that controls slice
  <letter> can release it."
- `slice.setListenLevel` when not listening: "You are not listening to slice
  <letter>. Listen in first."
- A slice closed and its letter made again: "That slice has closed. Choose it
  again from the list."
- Two devices took or released from the same revision: "Someone else changed
  who controls slice <letter>. Look again and try once more."
- A take while the slice transmits: "Slice <letter> is transmitting. Take
  control once it stops."
- A release while the slice transmits: "Slice <letter> is transmitting.
  Release it once it stops."
- A take whose controller cannot stay on as a listener (an older app) is no
  longer refused (JJ, 2026-09-30): the take goes through and that device
  loses the slice. The same holds for a controller that is gone.
- A take of the Core's own slice with nobody at the Core's desktop (a
  headless Core), from a device below `sliceAccessVersion` 3: "Slice
  <letter> is run by the Core itself, so control of it cannot pass to this
  device." With someone at the desktop the take goes through as any other.
- A peer without the feature sending a `slice.*` verb: "Update this app to
  listen to and take slices on this Core."; from a Core that cannot share:
  "This Core cannot share slices between devices."

Keying (link section 18):

- `noTransmitSlice`: "There is no slice to transmit on. Add a slice first."
  Also the answer to a key whose binding would land on another device's
  slice (for example the slice the phone lost to a take, or a slice where
  another device left the flag) while the phone has no slice of its own it
  may transmit on. With one, the Core moves the unkeyed flag there once the
  key is admitted, and the key goes ahead. A key in that case while the
  radio is still coming back to receive after a transmission (or while the
  radio's own PTT holds the flag) is refused `holderOnAir`, "The radio is on
  the air. Try again when it stops.", and may be tried again a moment later.
- The hosting desktop's footswitch and mic PTT (ruling 8.11 on a hosting
  desktop, JJ 2026-09-30): with the flag on another device's slice (the
  phone's included), they key the desktop's active slice, even a slice the
  phone controls: the flag moves there once the key is admitted and the radio
  transmits on it. With the flag on one of the desktop's own slices, a
  non-active one included (split transmit), they key that chosen transmit
  slice and the flag stays. The phone sees this in `txState` (`holderSource`
  `radioPtt`, `txSliceId`) as for any radio PTT; nothing new is sent.
- `chooseTransmitSlice`: "You took this slice from another device. Choose it
  for transmit first with its TX button." This refusal applies to a key whose
  binding would land on a slice this device took and has not chosen with
  `tx.setTxSlice`, while it has no other slice it may transmit on. A phone on
  its own slice is never refused for lack of `tx.setTxSlice`.

## Approved behaviors and journeys

From the design document. These are JJ's approved behaviors (his words in
the plan addendum G-118).

| Action or event | Required result |
| --- | --- |
| Listen in | Join the existing slice without allocating another slice or receiver. Its one controller keeps tuning. This device controls its own volume and mute |
| Take control | The existing slice moves; letter, color, frequency, mode and filter stay. The former controller stays a listener and is told. Every slice can be taken; the one refusal is while it transmits (JJ, 2026-09-30) |
| Release | This device gives up control and stops listening. Other listeners keep audio and the slice can be taken. With nobody left it closes, the Core's last slice included |
| Handoff while TX is selected but idle | The slice's TX selection clears. The new controller selects transmit explicitly |
| Handoff while transmitting | Refused until transmission stops, rechecked when applied |
| Missing device | The three-minute reconnect grace stays. At expiry the device's control and listening claims are removed. Slices stay for remaining listeners and close when nobody is left |

Journey, as the design gives it: a Mac controls A and the phone joins A with
Listen in. Both hear A, and the phone's volume and mute affect only the
phone. The phone's tuning controls say the Mac controls A and offer Take
control. Taking control keeps A's tuning and audio, updates both controller
labels and leaves the Mac listening. Selecting another slice on the phone
changes the phone's active receive slice and does not release the ones it
joined. When the last receiver or slice cap is in use, Listen in and Take
control still work, and New slice reports the real constraint and offers
existing slices.

## Decisions JJ has made that bind the phone

The ledger (`progress.md` of the 2026-09-28 slice plan) records these as his
rulings. They are stated for the desktop and remote windows, and the phone
receive-slice UI follows the same actions and states.

- **U1.** An unseen slice goes into the main window: an existing pan showing
  it comes forward, else an empty pan, else the main window grows to a layout
  that fits, and a named destination is asked only when no larger single-window
  layout fits. Placing a slice never opens a new floating pan.
- **U2.** A slice already showing in a floating pan brings that floater to the
  front and switches the main window's receive to it. Nothing moves and no
  second copy is made.
- **U3.** A pan background click keeps its display-only meaning. Slices are
  chosen by flag or tab.
- **U4.** Same-pan stacking keeps today's rule, the selected slice's flag on
  top. Listened slices keep their color with "Listening · controlled by ..."
  text.
- **U5.** The existing AF slider and mute on any slice change only what this
  device hears, labeled "Your volume" on a listened slice. Nobody's volume
  or mute changes anyone else's audio.
- **U6.** Flag text: "You control" (menu: Release); "Listening · controlled by
  <device>" (menu: Take control, Stop listening; tuning disabled with
  "<device> controls this slice"); "TX" as today, red on air.
- **U7.** Listening ends only when the operator ends it: Stop listening,
  Release, or a close (JJ, 2026-09-30, replacing the earlier "a device hears
  only slices it can see"). The phone does not send `slice.stopListening`
  when it leaves a slice's band; the edge marker and the Slice list row
  count as showing the slice. On the desktop, a listened slice whose pan a
  layout change removes moves onto a pan that remains and keeps listening.
  On that pan it is a marker only: its flag inside the span, or its own edge
  marker outside it, which stays while it is off the span whatever the
  other slices do. Its stream is not shown on that pan, and neither its
  tunes nor its DDC moves change that pan's view, VFO or DDC centre, with
  CTUN on or off; its demodulator shift comes from its own stream. Its
  slice chooser row keeps Stop listening. A reveal ("Show Slice X on", or
  a pan the window opens for it) shows the slice as before, centred on its
  stream. Controlled slices keep
  today's rehoming into a remaining pan. The Core never ends a listening
  claim on a pan, layout or band change; only the verbs, a claims drop or
  the slice closing end it.
- **U8.** JJ's words: "2 but for only slices tgat are activatyed show in the
  applet." The transmit applet shows a row of slice letter buttons, only for
  slices active on this device, meaning the slices it controls. Pressing one
  uses `tx.setTxSlice`, the same for every client (a keyed radio unkeys, then
  moves). The earlier "idle only; refused on air" wording was the
  controller's addition and is withdrawn.

## Phone parity gaps for the phone's PR

From the deep scout of the phone app. These are for the phone's dependent PR;
none is fixed by the Core branch.

1. **Take is reached only after a refusal.** The phone gets to Take only after
   a refused command (`ConfirmationLayer.swift:35-59`,
   `TakeReceiverSheet.swift:8-73`). With this contract the phone should offer
   Listen in, Take control, Release and Stop listening directly from the
   receive-slice UI, as the desktop's flag menu does.
2. **Close and select refusals are log-only** (`BandSlicesModel.swift:302-349`).
   Show the Core's reason to the operator, as sent.
3. **Foreign slice note wording** says only the owner may tune
   (`ForeignSliceLabel.swift:8-14`). Replace it with the controller's name and
   the Take control action, in U6's words.
4. **Pairing check.** Where the phone retires a token and revokes a device
   (the device-list flow), it should check for another paired device first
   (the ledger's note on the device-identity task).

## Open items

These are not settled. Do not build them as decided.

- Task 15 (the RX applet) items owed to JJ: menu placement and tooltip wording,
  and the disabled look for controls a listener cannot change. ATT and preamp
  are held on a listened slice (JJ, 2026-09-30).
- Task 16 (placement) items owed to JJ: the order the main window grows in
  (1 to 2v, 2 to 3v, 3 to 2x2, 4 to 3h2), the look and wording of the
  destination menu, floating pans re-docking when the layout grows, whether a
  listened slice follows its controller's pan move, and whether taking control
  makes this window's placement the slice's pan for every device.
- The phone's own layout for the receive-slice UI belongs to the phone crew and
  to JJ's review.
- The session conformance fixture for `slice.setListenLevel`, as noted above.
