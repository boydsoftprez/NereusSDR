# Containers and objects: recovery and usability design

Date: 2026-10-02
Status: approved in conversation on 2026-10-02; implementation plan written for review.
Implementation has not started. This document does not approve a wholesale
merge of the historical refactor.

## Purpose and decisions from the conversation

Make containers and their contents usable as a normal part of arranging
NereusSDR. Recover the useful unfinished work, finish the settings experience,
and make meter indicators look and move correctly.

The maintainer requested:

- Keep Nereus's container approach and study the best parts of Thetis.
- Study current Aether, particularly the top-left dotted drag grip, reordering,
  placement, resizing and pop-out behavior.
- Let the user move the objects in the applet area into their own containers.
- Let containers hide their headers/controls until hover.
- Refactor Container Settings and its objects for easier use.
- Show proposed interfaces and moving meter faces before building them.

The Container Settings directions and Mic/ALC animated face study were accepted
as a visual direction. The mockups are design references, not evidence that
Qt rendering, persistence or radio integration works.

Approved direction in this document: a contents list with a prominent preview;
named applets and complete meter faces as the moveable units; flat containers
that may contain both kinds of object; and explicit header modes. These choices
were reviewed as the direction for the implementation plan.
Splitting individual sliders/buttons out of an applet is a separate design.

## Baseline and current project guidance

Use freshly fetched Nereus `origin/main` at `dd53da5af` as the integration base,
not the local checkout at `e342467fb`. Guidance comes from that main's
`CLAUDE.md`: **Study, then choose.** Read and summarize the references, then port
faithfully where appropriate or design for Nereus's slices, panadapters, remote
Core and clients. Protocol facts, signatures and hardware limits still require
sources. Translated code keeps the required attribution.

Reference snapshots:

| Reference | Snapshot | Use |
| --- | --- | --- |
| Nereus main | `dd53da5af` | Integration and preservation baseline |
| Old composite refactor | `refactor/edit-container-rebased`, `15de6fbecf` | Selective recovery material |
| April worktree | `.worktrees/edit-container-refactor`, `50c5246cdb` | Historical design and handoff |
| August applet spec | `claude/widget-popout-controls-391063` | Historical intent and known lifecycle pitfalls |
| Aether official main | `5766bb13ef9ec8df3cf714f2c2fae2e17a7465ab` | Qt drag, placement, pop-out and restore patterns |
| Thetis | `v2.10.3.15` | Meter composition, calibration and dynamics reference |

No DSP or protocol behavior change is part of this design. Layout operations
must not transmit, change radio settings, or create another DSP instance.
Nothing in `src/core/` or `src/models/` acquires a GUI dependency (rule R1).

## Evidence that affects recovery

The references below are against the snapshots above, not unstamped local
files. Historical tests and old handoff claims have not been rerun.

- Main has an applet floating shell and saved geometry. Its pop-out button and
  restore path are gated by `canFloat()`; Mod Monitor supplies the positive
  override. The dotted grip is a label, not a reorder implementation.
  See main `AppletPanelWidget.cpp:198–234, 239–325, 382–431`,
  `AppletWidget.h:57`, and `ModMonitorApplet.h:81`.
- Main's container reparent path extracts serialized meter content and creates
  a fresh meter surface. The design must preserve this platform workaround.
  See main `ContainerManager.cpp:137–188`.
- Main restores containers before constructing the applet panel; a registry
  must account for this ordering rather than assume a working applet factory.
  See main `MainWindow.cpp:7326–7354, 9337–9658`.
- Main's `ItemGroup::installInto()` transfers primitives into a meter surface;
  saved presets therefore do not provide durable named composite objects.
  See main `ItemGroup.cpp:520`.
- Main dispatches readings through an outer item's single binding. The old
  branch's `pushBindingValue()` supports composites' internal channels and is
  useful recovery material. See main `MeterWidget.cpp:208–235`, rebased
  `MeterWidget.cpp:199–252`.
- Both serializers have paths that discard unknown records. The old migrator
  rebuilds some customized objects from defaults and may leave old decorations
  beside new composite decorations. See main `MeterWidget.cpp:284–373`,
  rebased `LegacyPresetMigrator.cpp:131, 169, 187–214, 445–492`.
- The old branch stores edited names/UUIDs in dialog rows, then serializes only
  the underlying meter items. Reopening regenerates names and IDs. Its list
  reorder changes paint order but does not renumber stack positions.
  See rebased `ContainerSettingsDialog.cpp:1528–1555, 2160–2209, 2498–2534`.
- Main's preview hook is empty, and cloning hand-copies MMIO and layout fields.
  See main `ContainerSettingsDialog.cpp:1652–1682`.
- Main's power rescaling and TX reset know primitive types. Recovered
  composites must participate explicitly. See main `MeterWidget.cpp:238` and
  `MeterPoller.cpp:466–512`.

## Container and content model

Keep `ContainerManager` as the coordinator for Nereus containers. Give each
container a document describing its content independently of transient Qt
widgets. Keep this presentation document in the GUI layer.

A container has a stable ID, name, ordered content entries, layout policy,
placement, visibility policy and chrome settings. A content entry has a stable
ID, durable display name, kind, binding/context, layout information and typed
configuration. Internal meter paint layers are not content-order entries.

Two content categories share the same arranging commands:

1. **Meter objects:** complete named faces such as Mic, ALC, Power/SWR, cross
   needle or ANAN Multi Meter. Composite internals stay together when selected,
   resized, duplicated or moved. Primitive/custom compositions remain available
   for legacy/custom layouts.
2. **Applet objects:** the applet widget and its existing model/session
   connections. A catalog identifies which objects are singleton views and
   which may be duplicated. Moving a singleton never constructs a second copy.

A content host adapts these categories to the container. Meter-only legacy
containers retain their canvas layout. New mixed containers use an ordered
vertical layout; meter objects retain their own calibrated face geometry and
widgets retain their natural minimum sizes. Arbitrary nesting is deferred.

One registry and codec serve load/save, editor drafts, duplication, migration
and view reconstruction. Apply must not discard a property because a separate
clone path omitted it. Keep bindings, MMIO, visibility and placement in the
document rather than patching them around serialization by list index.

## Arrange, pop out and return

The top-left dotted grip is the explicit drag target. Dragging controls in an
applet does not initiate arrangement. The title-bar menu exposes equivalent
Move Up, Move Down, Move to Container, Pop Out and Return actions.

- A drag within a stack shows an insertion line and changes actual content
  order on a valid drop. Paint order within a meter face is unchanged.
- A drop into another container transfers the complete entry. The source and
  destination update together; cancel or an invalid target changes neither.
- Pop Out creates a Nereus container containing that object, using the same
  placement/resize controls as other containers. Retain a return destination
  and position identified by stable IDs; if that destination no longer exists,
  return to the main applet area through a visible fallback.
- Preserve the present floating-applet convention: closing a popped-out applet
  returns it to its remembered container. Hiding is a separate action and
  retains placement. Show a specific Return tooltip so the action is clear.
- Mark pop-out shells explicitly. Closing such a shell returns every contained
  object to its own remembered destination in one transaction, including objects
  moved into it later. Objects created in the shell, or whose old destination
  was removed, return to the main area. Remove the empty shell after the return
  succeeds. A normal user-created floating container closes by hiding, retaining
  all contents. Individual Return actions remain available in either kind.
- Singleton applets expose Move rather than Duplicate. Meter duplicates get
  new IDs and complete copied configuration.
- Deleting a container containing singleton applets returns those applets to
  the main area; it does not destroy their session/model connections. Removing
  a singleton from the contents list hides/returns its view through the same
  visibility coordinator. Removing a meter object deletes that presentation
  object only. These outcomes are stated in the relevant action labels.
- Hidden or unavailable siblings retain their positions. Layout lock blocks
  arrangement/resizing, while recovery and settings actions remain reachable.
- Floating and overlay geometry is clamped to an available screen/pane, respects
  minimum sizes, and restores across restart and screen changes.

The applet catalog is initialized before restore materializes content. If a
host or session is unavailable, retain the entry and defer view creation;
do not replace it with a blank meter or discard it. Capability and user
visibility gates apply consistently to stack, overlay and floating hosts.

## Header, resize and placement

Provide three explicit modes: Always visible, Reveal on hover/focus, and Hidden
with an explicit recovery gesture/menu. The last mode preserves a way to recover
controls without forcing every user into it. Reveal mode is the requested
everyday way to keep the face uncluttered.

Revealing chrome must not resize the meter viewport or move the controls under
the pointer. For the first implementation, reserve the header's layout extent
and reveal controls within it. A zero-space overlay header may be explored in
a later visual revision; it must not cover essential readings.

Show a resize affordance on hover/focus for manually sized floating/overlay
containers. In a stack, width follows the host and height follows the chosen
content sizing policy. Auto height, RX/TX visibility and placement anchors must
have real behavior before being presented as working settings.

## Container Settings

Retain a discoverable Available / Contents / Properties structure, with a
prominent preview of the edited container. Give the contents list names and
object-level actions; keep it usable for selecting overlapping custom elements.
The preview reflects the same document and renderer as the eventual container.

Organize properties into container layout/appearance, object appearance and
behavior, and advanced bindings. Show controls relevant to the selected object;
use meaningful names rather than exposing composite internals as unrelated
bars, scales and labels.

All changes belong to an editor draft until Apply. Preview live readings against
that draft without writing settings or operating radio controls. Applet previews
use inert presentation adapters with no radio actions or extra subscriptions;
never instantiate a second singleton or reparent the live one into the preview.
The real applet stays in its live host until Apply moves it. Apply validates
and commits the document, then reconciles the
views. Cancel discards the draft.
Switching containers retains their drafts during the same editing session.
Container creation/deletion or a move initiated in this dialog is part of the
same transaction; arrange actions made outside the dialog commit directly.
Track the live document revision when opening the draft. If the same content
changes outside the editor, Apply explains the conflict and offers reloading
that container; it never overwrites newer arrangement/configuration silently.

Unavailable objects remain named and selectable with an explanation. Offer
explicit removal rather than silently deleting them. Disable unsupported
properties with a short reason; do not expose a setting whose value has no
rendering or behavioral effect.

## Meter faces and dynamics

Use the accepted animated Mic/ALC study as the appearance baseline. Peak and
average readings are independent channels; history is a recent min/max range;
peak hold is an independently configurable indicator. Title color, row sizing
and header treatment remain editable choices, not implied global defaults.

For these faces, use the studied Thetis composition/calibration and dynamics
where they fit. Thetis `MeterManager.cs:24327–24403, 24649–24735` defines the two
bars, their channels, colors and -30 / 0 / +12 calibration at 0 / .665 / .99.
`21323–21385` smooths readings and retains history relative to update cadence;
`33393–33485, 33872–33980, 37424–37714` define scales and draw ordering.
The prototype's 50 ms synthetic sampling is not an approved production default.

Verify each additional face against its own source, including power/SWR,
analog/needle indicators and specialized meters. Do not apply Mic/ALC's scale
or one invented decay rule to every meter.

Presentation behavior must cover:

- Fan-out of all composite channels, correct source/session association and
  immediately seeding new/replaced views even when a reading is unchanged.
- TX/RX transitions, reset/history clearing, stale or missing data and PA power
  scaling through a common content contract rather than primitive-name casts.
- Cadence-aware smoothing/history, correct paint invalidation and equivalent
  appearance in CPU and GPU renderers.
- Layout transitions that do not duplicate polling/subscriptions. Recreate GPU
  views when needed while preserving the document and reading source.

Record deliberate departures from upstream behavior with their reason. This
allows a faithful indicator face with Nereus-specific container arrangement.

## Migration and compatibility

Read prior container data/items, splitter and applet float/geometry keys. Add a
versioned structured document without deleting the original records before a
validated replacement has been saved. Keep a recoverable original payload.

Legacy custom layouts open with their geometry and appearance intact. Promote
a primitive group to a composite only when its entire signature is unambiguous
and all stored properties can be represented without changing the result.
Otherwise retain it as a named legacy composition or primitive objects.

Preserve unknown kinds/fields as opaque unavailable entries, including raw
payload, order and placement. They survive saving, duplication and export.
Explicitly retired features follow the current retirement policy; preservation
of unknown records must not resurrect deliberately removed Discord controls.

Migration is idempotent and handles mixed old/new content. Apply/save/reload
cannot erase names, MMIO, bindings, visual customization or original placement.
Migration failure leaves the original document usable and explains the problem.

## Delivery boundaries and verification

Recover selected classes and tests, then integrate their contracts with fresh
main. The old migration, generic bar rendering and dialog assumptions need
repair before integration. Do not merge all 46 historical commits as one unit.

Suggested dependency order for the later implementation plan:

1. Document/codec and compatibility foundation, including lossless fixtures.
2. Complete composite meter contracts and deterministic rendering references.
3. Container content host and unified applet arrangement/pop-out lifecycle.
4. Transactional settings editor and accurate prominent preview.
5. Integrated visual review, persistence/recovery checks and release readiness.

Acceptance evidence must demonstrate:

- Dotted-grip reorder moves rows, whole objects transfer/pop out/return, and
  canceled drops preserve source/destination state. Mixed pop-out shells return
  their objects to the correct individual destinations on close.
- Rename, duplicate, Apply, Cancel, restart and recovery retain complete state.
- Customized, mixed and unknown legacy content round-trips without silent loss;
  repeated migration leaves the result unchanged.
- Newly applied/recreated meters show stable current readings immediately, with
  all channels, RX/TX gates and power scales working.
- Meter traces and rendered frames match the chosen reference across rise,
  release, history expiry, peak hold and minimum supported sizes. Capture CPU
  and GPU views rather than treating numeric tests as visual evidence.
- Hover controls do not move content; floating geometry and visibility recover
  after host/session/screen changes; no arrange action keys the radio.

Use the current fast-test-loop guidance and targeted tests. Windows GPU
reparenting and macOS/Linux native drag behavior need explicit runtime evidence
on those platforms before claiming cross-platform completion.

## Review and next step

The written design was accepted in conversation. Review the concrete
implementation plan against fresh main and select its execution method.
Keep further visual review at behavior changes and meter-family milestones.
