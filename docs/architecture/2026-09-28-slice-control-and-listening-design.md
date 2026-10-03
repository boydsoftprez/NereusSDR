# Slice control and shared listening

Status: approved design, implementation in progress. JJ approved the six
behavior rulings and the bottom-RX chooser area on September 28, 2026, and
ruled the full-window behavior (flags, applets, multiple pans, placement,
layout changes and the TX applet letters, U1 to U8) the same day; see
"Full-window behavior (ruled)" below. Implementation follows
`2026-09-28-slice-control-and-listening-plan.md`. Its Task 1 (slice
incarnation and control revision) is complete on its lane in signed
`efb398303`, not yet merged to the trunk; nothing else in this document is
built. This document describes the required behavior; it does not claim
that the new commands or sharing support exist. The Core/GUI implementation
belongs in the current PR; the matching iPhone UI belongs in the phone's
dependent PR.

## What the operator should be able to do

From either desktop or phone, see every existing slice, identify its tuning
controller, hear it without disturbing anyone, take control of that same
slice, and leave without stranding radio resources. A slice's letter and
Aether color identify the slice. Separate text identifies who controls it,
whether this device is listening, and whether it is selected for transmit
or actually transmitting.

The current bottom RX banner describes this window's selected receive
slice. The proposed change makes its letter/status an entry to the slice
chooser. Existing mode/filter/DSP badges remain beside it. Selecting an
inventory row inspects it; it does not take control. The phone uses the same
actions and states through its receive-slice UI, with its actual layout to
be reviewed by the phone crew. TX selection remains in the TX controls.

The chooser shows the stable letter/color, frequency, mode/filter,
controller, present/away state, this-device listening state, and TX state.
It distinguishes this window and the desktop running Core from another
device with the same display name. A name is never an authorization key.
An empty window still offers Choose a slice and New slice.

## Approved behavior

| Action or event | Required result |
| --- | --- |
| Listen in | Join the existing slice without allocating another slice or hardware receiver. Its one controller retains tuning. This device controls its own volume and mute. |
| Take control | Transfer the existing slice, preserving letter, color, frequency, mode and filter. The previous controller remains a listener and receives clear notice. The notice offers Take it back: one tap takes control again under the same checks, and transmit does not move with it. A slice the hosting desktop controls transfers the same way; the desktop hears the notice and can take it back. With nobody at the Core's desktop (a Core with no desktop window), the Core's own slice transfers at once with nobody to tell (JJ, 2026-09-30; `sliceAccessVersion` 3; a device below 3 is refused as before). Every slice can be taken; the one refusal is while it transmits. A controller that cannot stay a listener (an older app) loses the slice (JJ, 2026-09-30). |
| Release | This device relinquishes control and stops listening. Other listeners retain audio and the slice becomes available to control. With nobody left, close it and free its resources, including the last physical Core slice. |
| Handoff while TX is selected but idle | Clear that slice's TX selection. Its new controller must select transmit explicitly. Taking RX control grants no transmit permission. A device that shares slices (the hosting desktop, or a device on sliceAccess) never keys on another device's slice, the one it lost included: its key moves the flag to a slice of its own once the key is admitted, or is refused with "There is no slice to transmit on. Add a slice first." when it has none. Any other keyer never keys on the slice it lost. A key that would move the flag while the radio is not back in receive, or while the flag is frozen, is refused with "The radio is on the air. Try again when it stops." The radio's own PTT (footswitch or mic) on a Core with no desktop transmits where the flag is (ruling 8.11). On a hosting desktop (JJ, 2026-09-30) it keys the desktop's active slice, wherever it is, even one another device controls: with the flag on another device's slice, the flag moves to the desktop's active slice once the key is admitted and keys there; the move is never made while the radio is on the air or the flag is frozen, and that key is refused rather than keying another slice. With the flag on one of the desktop's own slices, a non-active one included (split transmit), it keys that chosen transmit slice and the flag stays (JJ, 2026-09-30); with the flag on a slice nobody controls, it transmits where the flag is. |
| Handoff while transmitting | Refuse until transmission stops. Recheck when the action is applied. |
| Missing device | Preserve the existing three-minute reconnect grace. At expiry, remove the absent device's control/listening claims. Keep slices for remaining listeners; close those with nobody left. |

The approved phrases are recorded verbatim in G-118 of the plan addendum.
Any new participant may listen without an extra consent step from the
controller, consistent with JJ's approved paired-device Listen in flow.
Pairing, session admission limits and separate transmit authorization
continue to apply.

## Journeys

Mac controls A, phone joins A: the phone chooses Listen in. Both hear A;
the phone's volume/mute affect only the phone. Its tuning controls explain
that the Mac controls A and offer Take control. Taking control retains A's
tuning and audio, updates both controller labels, and leaves the Mac
listening. The Mac can then leave or take control again, in one tap from
its notice (Take it back, take-over parity 2026-09-30: `sliceAccessVersion`
2, the link document section 7.4). The same holds when the Mac is the
hosting desktop and A is its own slice.

Selecting another slice on this device changes the active receive focus
and the bottom banner. It does not release previously joined slices or
change the device's transmit selection. Release and Stop listening are
explicit actions; the one automatic stop is a layout change that removes
the only pan showing a listened slice (U7 below), because a device hears
only slices it can see. Controller-only edits are disabled for a listener, with
the controller named and Take control reachable.

When the last available slice or hardware receiver is in use, the chooser
still permits Listen in and intact Take control. Those actions use the
existing slice and do not need new capacity. New slice reports the actual
resource constraint and offers existing slices to use. Moving an entire
receiver or closing others to create a different slice remains a distinct
capacity action: the confirmation must name every affected slice and
listener, recheck the current set, and leave everything intact on refusal.

When a device disappears, show it as away during grace. Another device can
take control of an idle slice during grace. A subsequent reconnect restores
only claims the device still has; it cannot steal back transferred control.
Expiry acts on the matching absence generation, so an old timer cannot
release a replacement session's work. Explicit leave releases immediately.

## Full-window behavior (ruled September 28, 2026)

The September 28 scout distinguishes window RX focus from each pan's own
selected slice. Existing flag and RX-tab selection updates window RX;
pan-background selection updates display focus only. Spectrum actions
target the emitting pan's slice. The shared-listener change preserves an
explicit target for every control instead of making other pans silently
tune the bottom banner's selection.

JJ ruled each open window question on September 28, 2026. His words are
quoted exactly as the crew ledger records them; the "Ruled behavior" text
is the lead's recorded reading of each answer.

- **U1, placing an unseen slice.** JJ: "1 your recommendation but maybe
  not a floating pan but a layout that fits in the single window". Ruled
  behavior: an unseen slice goes into the main window. A main-window pan
  already showing it comes forward; else an empty main-window pan takes it;
  else the main window grows to a pan layout that fits in the single
  window, or asks for a named destination. Placing a slice never opens a
  new floating pan, never creates a physical receiver and never changes
  shared tuning. The lead confirmed on 2026-09-28: ask only when no larger
  single-window layout fits, and growing the layout to place a slice adds
  no new slice to any other empty pan.
- **U2, a slice already in a floating pan.** JJ: "1 your recommendation".
  Ruled behavior: that floater comes to the front and the main window's RX
  switches to the slice. Nothing moves and no second copy is made.
- **U3, pan background click.** JJ: "1 your recommendation". Ruled
  behavior: it keeps today's display-only meaning (keyboard and scroll
  focus). Slices are chosen by flag or tab.
- **U4, same-pan stacking.** JJ: "1 your recommendation". Ruled behavior:
  today's rule stays, the selected slice's flag on top. Listened slices keep
  their Aether color, with "Listening · controlled by ..." text.
- **U5, volume and mute.** JJ: "1 your recommendation". Ruled behavior: the
  existing AF slider and mute on any slice, controlled or listened, change
  only what this device hears; on a listened slice the slider is labeled
  "Your volume". Nobody's volume or mute changes anyone else's audio, the
  controller's included. The controller's AF therefore becomes per-device
  as well, not only a listener's.
- **U6, flag wording and actions.** JJ: "1 your wording sounds fine".
  Ruled behavior: a controlled flag reads "You control" (menu: Release); a
  listened flag reads "Listening · controlled by <device>" (menu: Take
  control, Stop listening), with tuning disabled and the reason "<device>
  controls this slice"; "TX" shows as today, red on air.
- **U7, layout changes.** JJ: "stop listening to slices, its confusinf to
  hear slices you have no visual referance to". Ruled behavior: a listened
  slice that loses its pan in a layout change stops being listened to on
  this device, with a plain notice. The principle: a device hears only
  slices it can see. Controlled slices keep today's rehoming into a
  remaining pan, so every slice a device hears stays visible.
- **U8, TX applet letters.** JJ: "2 but for only slices tgat are activatyed
  show in the applet". Ruled behavior: the TX applet shows a row of slice
  letter buttons, only for slices active on this device, read as the slices
  this device controls (the only ones it may transmit on). Pressing one
  selects it for transmit with the existing `tx.setTxSlice` behavior, the
  same for every client: while keyed it unkeys through the unkey gate, then
  moves (ruling 8.10). An earlier record of this ruling added "idle only;
  refused on air"; that was the lead's addition, not JJ's words, and is
  withdrawn (lead correction, 2026-09-28).

The resulting mapping:

- The bottom chooser inventories all slices. Inspecting a row performs no
  receive or transmit operation. A flag, joined RX tab or explicit Select
  RX focuses that slice, its visible pan (placed per U1 or raised per U2),
  the RX applet and the bottom bar.
- A joined flag retains its Aether letter and color and carries the U6
  text naming the current controller. A visible slice not joined here
  remains a distinguishable foreign marker. The same details and actions
  are reachable from the flag and the bottom chooser.
- The RX applet shows tabs for this device's joined slices. Under U7 every
  joined slice is visible in one of this device's pans, so there are no
  tabs for hidden joined slices. Listening disables shared tuning and DSP
  edits, with the controller named and Take control reachable. Volume and
  mute stay on the existing AF slider and mute (U5), which Task 14b made
  each listener's own level in the audio mixer; the RX applet has no volume
  or mute (the flag and title bar are the audio surfaces).
- TX stays explicitly bound to the selected transmit slice even when a
  different slice is selected for receive. The TX applet's letter row (U8)
  selects among this device's controlled slices through the existing
  transmit-slice behavior (while keyed it unkeys, then moves). The current
  active-RX-dependent TX applet bindings need a safety audit when this is
  implemented.
- If a slice is already visible, focus its existing pane, including a
  floating pane (U2: the floater comes forward and the slice becomes this
  window's RX; nothing moves). If unseen, it goes into the main window
  (U1): an empty main-window pane, else the window grows to the next layout
  that fits in the single window, else the operator picks a destination.
  Growing adds no slice to any other empty pane and never opens a floating
  pane. A new view of an existing slice does not create a physical receiver
  or change shared tuning. Replacing or hiding a view no longer retains
  listening: U7 supersedes the earlier proposal that listening continue
  until an explicit leave, and the earlier proposal of RX applet tabs for
  hidden joined slices.

The full-window interactive proposal, `nereus-multi-pan-slice-flow.html` in
this chat's visualization directory, was built from actual offscreen Qt
two-pan captures recorded in `core-gui-multi-pan-reference-report.md`. It
is design evidence, not product implementation, and it predates the
rulings: its placement of an unseen slice and its retained hidden audio are
superseded by U1 and U7, and it has no TX applet letter row (U8).

## Core boundaries and invariants

Keep tuning authority separate from listening membership. Each live slice
has at most one controller and may have multiple device listeners. The
controller is also a listener. A slice's running identity has an incarnation
distinct from its reusable numeric letter, and its control changes have a
revision. Stale commands and confirmations must not act on a new slice
that later reuses A or on a newer control assignment.

Control/listening changes are serialized on the model thread. Validate the
current authenticated device/session, target incarnation, expected control
revision, current TX gate and capacity effects before mutation. A successful
handoff publishes the new authority without an intermediate destroyed or
unowned slice. Do not rebuild its DSP channel or remove/recreate its audio
source just to change controller. Preserve the existing immutable audio
view and tap lifetime rules across actual slice retirement.

Keep three predicates separate: may see detailed slice state, may receive
its audio, and may change its tuning. Extending an existing ownsSlice check
globally would grant listeners unintended write or TX access. Listeners
need a current read-only mirror of shared tuning and an authoritative
per-device active RX selection; an owner's global active property cannot
represent every listener's independent choice.

Negotiate the new slice-access feature explicitly. Existing clients must
not receive unknown commands or be treated as supporting shared listening.
The hosting desktop must call the same validated Core operations as remote
clients. Its direct RadioModel shortcut must not bypass capacity questions,
TX protection, generation checks or listener cleanup.

Per-listener gain/mute cannot use the shared SliceModel's AF or muted
properties. The existing per-receiver audio tap is before slice mute/pan
and undoes AF gain; verify its zero-gain behavior and stream capacity before
using it for fan-out. Preserve bounded audio resources, clear admission
failures, reconnect/session fencing, and remote audio clock behavior. DSP
callback code must never traverse mutable listener collections or QObject
state. Host speakers and remote playback must obey the same per-device
listening policy without one device muting another. Under U5 this covers the
controller too: the controller's AF slider and mute set only the
controller's own hearing, and a handoff carries no device's level to
another. A listener's audio never depends on the controller's AF, including
AF at zero: the listener feed is taken before the controller's AF (before
WDSP's panel gain, or an equivalent that never divides by the AF), and the
controller's own path stays as it is today (lead ruling 2026-09-28, U5 over
the earlier Q2).

Zero physical slices is a supported idle Core state. Audit all last-slice
guards and first-slice indexing, active RX/TX fallbacks, saved layout,
display demand, DDC retirement, audio taps, RADE and external diversity.
Closing the last slice must leave valid empty state and a working New slice
path. Preserve tuning preferences without resurrecting an old control claim
or displacing a current user when an absent device returns.

## Verification required before delivery

- Three clients see the same slice identity and current tuning through
  listen, handoff, release and reconnect. Their active RX, volume and mute
  remain independent. The previous controller hears continuous audio after
  handoff and cannot continue writing tuning.
- A listener's forged property writes, tuning verbs and TX attempts are
  refused by Core. Old-session, reused-letter, double-take and stale-confirm
  cases preserve the current authority and all unrelated slices.
- Idle TX selection clears on transfer, on-air transfer refuses, and RX
  control never implicitly keys or grants transmit.
- Last listener leaving closes the slice; remaining listeners prevent its
  closure. Final physical removal reaches a valid zero-slice state and can
  create a new slice again. Device expiry and explicit leave follow the same
  membership rules; a reconnect within grace survives an old expiry event.
- Capacity exhaustion offers useful existing-slice actions. Failed creation,
  pan move and restoration preserve all victims and produce honest notices.
- Hosting desktop, remote desktop and phone have visible pending/refusal
  feedback and equivalent action semantics, including zero-owned states.
  Verify the real desktop layout offscreen before an authorized preview
  launch; the HTML mockup alone is not implementation evidence.
- The full-window rulings hold: an unseen slice is placed in the main
  window without a new floating pan, receiver or tuning change (U1); a
  floating slice's pan comes forward without moving (U2); a background
  click changes display focus only (U3); stacking and wording follow U4 and
  U6; every volume and mute is per-device, the controller's included (U5);
  a layout change that hides a listened slice stops listening there with a
  notice, and every slice a device hears is visible (U7); the TX applet
  letter row lists only this device's controlled slices and selects through
  the existing transmit-slice behavior (U8).

Use the current lane reports and addendum as the work record. G-125's
bounded Add preflight is a separate repair and does not establish general
transactionality for the remaining move/restore paths.
