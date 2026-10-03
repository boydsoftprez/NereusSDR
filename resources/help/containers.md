# Containers and objects

Containers hold an ordered stack of meter faces and existing applet views. Open
**Containers > Edit Container** or the container's **Container Settings…** menu
to edit its name, appearance, sources and Contents.

## Arrange the workspace

Drag the dotted grip beside an object to reorder it or move it into another
unlocked container. The grip occupies its own gutter; the object's controls keep
their normal operation. The container context menu's **Contents** submenu offers
**Move Up**, **Move Down**, **Move to Container**, **Pop Out** and **Return to
remembered container**. Meter faces can be duplicated. Applet views are singletons:
choose **Move** to relocate their existing view rather than create a second one.

A pop-out shell remembers each object's original container and neighbors. Closing
it returns all objects together. If a remembered container was deleted, the main
container is the fallback. Locked destinations refuse the operation without a
partial move. Closing an ordinary floating container hides it and retains its
placement. **Hide container (retain placement)** also preserves its contents.
Use the Containers menu to show a hidden container again. Removing a container
in Settings returns its contents; removing a meter object explicitly deletes that
entry. **Return / hide view** retains the existing applet.

## Edit, Apply and Cancel

Settings edits a workspace draft. Switching between containers keeps your pending
edits; moving a singleton within the draft takes effect on **Apply**. The preview
uses cached readings and inert controls. It does not tune, transmit or start a
second receiver. **Apply** saves the workspace and updates the live arrangement.
**Cancel** or closing Settings discards unapplied changes.

If another change touches the same container while Settings is open, Apply reports
a conflict. **Reload conflicting containers** refreshes the conflicting containers; unrelated
pending container edits remain. A failed save leaves the live workspace and your
draft intact, so correct the storage problem before applying again.

## Headers, locks and sources

Headers can be **Always visible**, **Reveal on hover / focus**, or **Hidden**.
Header space stays reserved. Hold **Shift** or use **Reveal controls** in the context
menu to recover hidden controls, then change the Header setting. **Lock** prevents
arrangement, floating/docking and resizing; it does not disable the object's normal
radio controls. Auto height follows the stack's actual minimum heights.

A container provides an inherited source, and supported meters may select an
explicit slice/session in Properties. Missing, stale or foreign sources show an
unavailable reading rather than values from a different receiver or station.
Clock and MMIO sources remain independent. An unavailable applet or unsupported
legacy object keeps its place and configuration for later recovery.

## Portable exchange and recovery

**Save / Load** exchange a container; **Export / Import** exchange its entries
through the clipboard. Import stages a draft and validates before Apply. Imported
objects receive new identities; internal return links follow the imported objects.
An imported singleton moves the existing view and preserves its live identity.
Malformed or future structured files are rejected without replacing your draft.

Customized legacy geometry, raw records, unknown fields and MMIO GUID/variable
bindings are retained. Portable recovery records keep the exact original imported
bytes/text through export, Apply and restart, including unsupported objects.
Migration also retains the original legacy settings and creates
`ContainerWorkspaceBackup` on a successful save. Recovery uses the same workspace
codec and store as normal saves; avoid manually rewriting unknown payloads.
