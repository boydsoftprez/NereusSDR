# Setup description v2: TNF table

This extends [Setup description v1](2026-09-23-setup-description-v1.md) for the
Core's existing TNF rows. The Core continues to own notch validation,
ownership, range checks, and shared-setting confirmation. No new notch
command or mirror ordinal is introduced.

## Negotiation and projection

A peer at protocol minor 11 or later that declares `setupDescription: 1`
receives `setupDescriptionVersion: 1` and category strings with `version: 1`.
A peer declaring 2 or a higher version receives capability and category
version 2. A peer without the declaration, or below minor 11, receives no
Setup schema, object, delta, or capability. The Core projects every outbound
category value, including initial object creation and later deltas, to that
peer's negotiated version. Projection removes controls whose
`requiresDescriptionVersion` exceeds the negotiated version, then empty
sections and pages. It sends an empty category string if no pages remain.
The v1 scalar DSP controls and their order match the v1 source. Other
categories carry their existing controls with a v2 category version for a v2
peer.

## TNF contract

`dsp.tnf.list` is a `kind:table` control with
`requiresDescriptionVersion:2`. Its one `binding.table` names
`notches.listJson` as `valueProperty` and `notches.revision` as
`revisionProperty`, with `format:"json-array"`, `rowKey:"id"`, and
`maxRows:1024`. The list is the Core's compact JSON array of records with
integer `id`, decimal `centreHz` and `widthHz`, and boolean `active`.
The columns match the desktop TNF table: Center Frequency (Hz),
100000..61440000; Width (Hz), 0..10000; Active; and Delete. The first two
use decimal values with step 1. Labels and tooltips, including the desktop's
spelling of the width tooltip, follow `MnfSetupPage`.

The row actions are exactly `notch.move(id,centreHz,widthHz)`,
`notch.setActive(id,active)`, and `notch.delete(id)`. Their argument sources
are single-marker objects: `{"$row":"id"}` or `{"$edit":"centreHz"}`,
`{"$edit":"widthHz"}`, and `{"$edit":"active"}` respectively. The move
action submits center and width together. The separate `dsp.tnf.add` button
uses `notch.addAtSlice(sliceId)` at `notchControlVersion>=2`; its sole source
is `{"$selectedOwnedSliceId":true}`. There is no mirrored `SliceModel.id`,
so this is an explicit ID source rather than a property lookup. The Core
accepts only this known table binding, these row fields and typed actions,
and this selected-slice marker for this Add command. Unknown sources,
fallbacks, extra markers, and expression-like forms are invalid.

## Client execution rules

Before enabling the table, a renderer must parse the entire `listJson`
array, reject more than 1024 rows, duplicate or invalid IDs, missing or
wrongly typed fields, out-of-range or non-finite numbers, unknown kinds, and
wrong source or argument types. It should show a plain reason when disabled.
It resolves the row, current list revision, and all edited values in one
live session and epoch immediately before sending a row action. An observed
revision change, row removal, session or epoch change cancels pending row
edits. Add resolves a current selected slice owned by this device and also
cancels when selection or ownership changes. It must never use a cached row
or slice ID from an earlier gesture.

Existing notch commands do not accept an expected revision. Client
cancellation prevents common stale sends, while the Core still refuses a
missing row, unowned slice, invalid range, or shared change according to its
existing rules. It does not promise compare-and-set rejection for a
concurrent edit of an existing row.

This slice describes TNF only. The phone has no generic Setup v2 renderer
yet. CFC/filter atomic endpoints, NNR pending groups, and dynamic asset
choices remain undescribed and incomplete.
