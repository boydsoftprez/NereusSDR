# Edit Container Refactor — Design Spec

**Status:** Approved (brainstorming)
**Author:** J.J. Boyd (KG4VCF) with Claude Opus 4.7
**Date:** 2026-04-19
**Mockup:** `.superpowers/brainstorm/9196-1776638323/content/edit-container-mockup-v2.html`

## 1. Motivation

The current Edit Container dialog (`ContainerSettingsDialog`, ~2326 lines) has four related
problems that together make container editing feel disconnected from the containers themselves:

1. **Presets explode into flat item lists.** Selecting "ANAN Multi Meter" in the available list
   creates 8 discrete rows (1 image + 7 needles) in the in-use list, each with its own
   editor. Thetis's `clsMeter` exposes each preset as one logical unit; NereusSDR loses that
   identity the moment the `ItemGroup` installs.
2. **Available items can be added any number of times.** The same preset can be re-added to a
   container; the available list doesn't reflect what's already placed.
3. **ANAN Multi Meter needles don't track the face arc.** At non-default aspect ratios the
   7 needles drift off the painted meter face. Pivot and radius are normalized against the
   container rect, not the background image rect, so changing container aspect moves the arc
   off the face.
4. **There is no on-container path to edit.** Users must traverse the menu bar
   (`Containers → Edit Container ▸ → pick from UUID-labeled submenu`). `ContainerWidget` has
   no `contextMenuEvent`, no `mouseDoubleClickEvent`, no gear icon. Containers are spatial
   objects reached only through a linear menu tree.
5. **No way to add a container from inside the dialog.** Creating a new container requires
   closing the dialog, opening `Containers → New Container…`, waiting for the dialog to reopen.

## 2. Scope

Bundled as one phase. Tier 1 (required) + Tier 2 (high-value low-cost bundled UX).

**In scope:**

- Preset collapse into first-class `MeterItem` subclasses (10 new classes).
- Hybrid addition rule: presets single-instance per container; primitives freely re-addable.
- In-use list: drag-to-reorder (drives z-order), user-editable display names, right-click
  context menu (Rename / Duplicate / Delete), Duplicate button for primitives.
- On-container access: `contextMenuEvent`, header `mouseDoubleClickEvent`, gear-icon
  `QToolButton`, inline header rename. `FloatingContainer` inherits via delegation.
- Auto-name new containers `Meter N`.
- `+ Add` button in the Edit Container dialog beside the Container dropdown.
- ANAN-MM needle arc fix: re-port calibration byte-for-byte from Thetis v2.10.3.13; anchor
  pivot/radius to background image rect; debug overlay toggle.
- Menu cleanup: move 7 applet show/hide toggles from the Containers menu to
  `View → Applets`.

**Out of scope:**

- Live preview rendered inside the dialog.
- Search / filter box in the available list.
- Undo/redo inside the dialog.
- Cut/copy/paste of items between containers.
- Skin format changes (phase 3H owns those).
- New meter types beyond what preset collapse requires.
- Non-modal "inspector" rework of the dialog (keep the current modal shape).

## 3. Architecture

### 3.1 Preset-as-first-class MeterItem

Each preset becomes a concrete `MeterItem` subclass that owns its internal parts privately:

```cpp
class AnanMultiMeterItem : public MeterItem {
  QImage  m_background;            // ananMM.png
  QPointF m_pivot;                 // shared across all 7 needles
  QPointF m_radiusRatio;
  bool    m_anchorToBgRect = true; // NEW — fixes arc bug
  bool    m_debugOverlay   = false;// NEW — paints calibration points
  struct Needle {
    QMap<float, QPointF> cal;
    QColor  color;
    bool    visible;
  };
  std::array<Needle, 7> m_needles; // Signal / Volts / Amps / Pwr / SWR / Comp / ALC

  void draw(QPainter&, const QRect& itemRect) override;
  QJsonObject serialize() const override;
  void deserialize(const QJsonObject&) override;
};
```

Ten new classes, one per collapsed preset:

| Class | Replaces `ItemGroup` factory |
| --- | --- |
| `AnanMultiMeterItem` | `createAnanMMPreset()` |
| `CrossNeedleItem` | `createCrossNeedlePreset()` |
| `SMeterPresetItem` | `createSMeterPreset()` |
| `PowerSwrPresetItem` | `createPowerSwrPreset()` |
| `AlcPresetItem` | `createAlcPreset()` |
| `AdcBarPresetItem` | `createAdcBarPreset()` |
| `MagicEyePresetItem` | `createMagicEyePreset()` |
| `HistoryGraphPresetItem` | `createHistoryGraphPreset()` |
| `SignalTextPresetItem` | `createSignalTextPreset()` |
| `VfoDisplayPresetItem` | `createVfoDisplayPreset()` |

(Exact factory list confirmed against `ItemGroup.cpp` during implementation; classes added
1:1 with existing presets.)

Primitives (`BarItem`, `TextItem`, `NeedleItem`, `ScaleItem`, `SolidColourItem`,
`ImageItem`, `SpacerItem`, `FadeCoverItem`, `LEDItem`, `DialItem`, all `*ButtonItem`s,
`VoiceRecordPlayItem`, `DiscordButtonItem`, etc.) are unchanged.

### 3.2 `ItemGroup` removal

Once the 10 preset classes absorb the factory logic, `ItemGroup` is deleted. Callers
(`ContainerSettingsDialog.cpp`, `MeterWidget.cpp`) switch to constructing the new subclass
directly. The `installInto(MeterWidget)` unpacking path is gone — each preset is itself a
single `MeterItem*` in the widget's item vector.

### 3.3 In-use row wrapper

Each in-use row carries display metadata the raw `MeterItem` does not own:

```cpp
struct ContainerInUseItem {
  MeterItem* item;         // owning pointer, lives in MeterWidget
  QString    displayName;  // user-editable, e.g. "Main S-Meter"
  QUuid      rowId;        // stable across reorder/rename
};
```

`MeterWidget` still owns the `MeterItem*` vector (unchanged). The wrapper list is a parallel
vector of equal length, maintained in lockstep with the item vector. `rowId` identifies a
row across drag reorder so the UI (dialog list, property pane) can track selection.

Z-order = vector order, as today.

### 3.4 Backwards-compatible serialization

Existing saved containers use the flat `ItemGroup` form (8 separate item lines for ANAN MM).
On load, a tolerant migration pass detects the known preset signatures:

- **ANAN MM signature**: ImageItem bound to `ananMM.png`, followed by 7 NeedleItems with the
  shared pivot `(0.004, 0.736)` and radius `(1.0, 0.58)`.
- Similar signatures for each of the other 8 presets.

If the signature matches, the flat items are collapsed into the new single preset class.
If any field was customized beyond what the new class exposes, log a `qCWarning` with the
container name and field and fall back to loose primitives for that one container
(tolerant mode per approval).

Once written back out, the container persists as the compact new form. Legacy detection
code lives in `MeterWidget::loadLegacyFormat()` behind a feature check; scheduled for
removal two releases after this phase ships.

### 3.5 ANAN needle arc fix

Root cause candidates, all addressed:

1. **Calibration table drift.** Re-port each needle's calibration array byte-for-byte from
   `MeterManager.cs:AddAnanMM()` at Thetis v2.10.3.13. Each ported table gets inline
   `// From Thetis MeterManager.cs:NNNN [v2.10.3.13]` stamps per the source-first protocol.
2. **Aspect-ratio scaling.** `m_anchorToBgRect = true` (new default). Pivot and radius
   ratios are evaluated against the `QRect` of the rendered background image, not the
   container's item rect. When the container is resized, the background image's aspect is
   preserved (letterboxed if needed), and the arc stays glued to the painted face.
3. **Debug overlay.** `m_debugOverlay` toggle paints each needle's calibration points as
   colored dots on the face, so misalignments become visible. Property editor exposes the
   toggle.

### 3.6 Dialog changes

Top bar unchanged in structure. Two rows preserved:

- **Row 1**: Container dropdown · **[+ Add]** (new) · Title · Border · RX source ·
  Show on RX · Show on TX.
- **Row 2**: Lock · Hide title · Minimises · Auto height · Hide when RX unused ·
  Highlight · Duplicate · Delete.

Three columns unchanged in structure:

- **Available**: same RX Meters / TX Meters / Primitives categorization. Presets that
  already exist in the current container render disabled with `"in use"` annotation
  (hybrid rule C — primitives stay freely re-addable).
- **In use**: `QListWidget` with `setDragDropMode(InternalMove)`. Drag handles visible on
  each row. Up/Down buttons removed (replaced by drag). Rename / Duplicate / Delete
  available via right-click menu and action buttons below the list.
- **Properties**: one editor page per item. For presets, one page covers the whole preset
  (not 8 sub-item pages).

Footer hints at the three new on-container paths to reach this dialog.

### 3.7 On-container access

- `ContainerWidget::contextMenuEvent` → `QMenu` with Edit, Rename, Duplicate, Delete,
  Dock Mode ▸ (Panel / Overlay / Floating). `setContextMenuPolicy(DefaultContextMenu)`.
- `ContainerWidget::mouseDoubleClickEvent` on header strip → emits `editRequested()`
  if the container isn't locked.
- Gear `QToolButton` added to container header layout; connects to `editRequested()`.
- Inline rename: F2 on selected container header, or right-click → Rename. Drives
  `setNotes()`.
- `FloatingContainer` delegates header events to its embedded `ContainerWidget` so
  all three paths work for floating windows.
- `MainWindow` connects `editRequested()` to the existing
  `ContainerSettingsDialog` open slot.

### 3.8 Dialog `+ Add` button

`+ Add` next to the Container dropdown:

1. Calls `ContainerManager::createContainer()` with default `DockMode::Floating`.
2. Assigns next unused `Meter N` name.
3. Populates with an empty `MeterWidget`.
4. Appends to the dropdown; switches selection to the new container.
5. On dialog Cancel: matches the existing "new container created then cancelled" behavior
   at `MainWindow.cpp:1413-1416` — the new empty container is destroyed.

### 3.9 Menu reorg

`MainWindow::buildMenuBar` changes:

- `Containers` menu keeps: `New Container…`, `Edit Container ▸` (fallback discovery),
  `Reset Default Layout`.
- **Move** the 7 applet show/hide toggles to a new `View → Applets` submenu. They toggle
  content inside the fixed applet panel (Container #0), not containers themselves.

Auto-name: new containers get `"Meter N"` where N is the smallest unused positive integer
across existing container `notes()`. Replaces `"(unnamed) " + uuid.left(8)` in
`rebuildEditContainerSubmenu()`.

### 3.10 Live-apply & snapshot

Current live-apply + snapshot/revert semantics (`ContainerSettingsDialog.cpp:293, 1012-1025`)
are preserved and extended:

- Snapshot is taken lazily per container — the first time the dropdown switches to a
  given container in a dialog session, snapshot before any edits land.
- Apply commits working items into `MeterWidget` immediately.
- Cancel reverts **every** container touched in this session to its snapshot.
- OK commits all + closes.

## 4. Signals

New signals:

- `MeterWidget::itemsChanged()` — emitted on reorder / add / remove.
- `ContainerWidget::editRequested()` — emitted by context menu, header double-click,
  gear button.
- `ContainerWidget::renameRequested()` — emitted by F2 or context menu Rename.

Existing signals reused:

- `ContainerManager::containerAdded / containerRemoved / containerTitleChanged`
  (dropdown already listens).

## 5. Testing

### 5.1 Unit tests (`tests/`)

- `test_anan_multi_meter_item.cpp` — construct, set pivot/radius, verify bg-rect anchor math
  produces expected arc endpoints for each of the 7 needles at 3 aspect ratios (square,
  2:1 wide, 1:2 tall). Golden-pixel check against a reference render.
- `test_preset_items_serialization.cpp` — round-trip each of the 10 new preset classes
  through `serialize()` / `deserialize()`.
- `test_legacy_container_migration.cpp` — load fixture saved from the current
  `ItemGroup`-based format (one fixture per preset). Confirm tolerant migration produces
  the correct new class, preserves mappable customizations, logs a warning for
  unmappable ones, falls back to loose primitives only where required.
- `test_container_inuse_reorder.cpp` — drag-reorder three items, confirm both the wrapper
  vector and the `MeterWidget` item vector reshuffle in lockstep, rowIds stable.
- `test_hybrid_addition_rule.cpp` — assert presets can't double-add to a container;
  assert primitives can.

### 5.2 Manual verification checklist

1. Open edit dialog on a container with a legacy ANAN MM → one row, collapsed pane.
   Close and reopen → persists in new form.
2. Add ANAN MM to empty container → one row; available entry greys to "in use"; re-adding
   does nothing.
3. Resize container to 2:1 wide and 1:2 tall → all 7 needles stay glued to the face arc.
4. Toggle "Show calibration overlay" → calibration points visible; toggle off → clean.
5. Drag-reorder: move Background (image) to bottom → renders over needles; back to top
   → needles over image.
6. Rename "ANAN Multi Meter" → "Left Panel Meter" → persists across reopen.
7. Right-click container on screen → Edit. Double-click header → Edit. Gear icon → Edit.
8. `+ Add` in dialog → new container appears, dropdown switches to it; Cancel destroys
   the empty new container.
9. Cancel after editing A, switching to B, editing B → both revert.

## 6. Source-first protocol

Per `CLAUDE.md` READ → SHOW → TRANSLATE:

- Capture Thetis version once at start: `git -C ../Thetis describe --tags`. Stamp every
  new constant with `[vX.Y.Z.W]`.
- READ / SHOW each needle's calibration table from `MeterManager.cs:AddAnanMM()` before
  porting. The 7 arrays become the `std::array<Needle, 7>` initializer.
- License headers: `AnanMultiMeterItem.cpp` ports from `MeterManager.cs` — copy that
  file's header byte-for-byte; add NereusSDR modification-history block per
  `docs/attribution/HOW-TO-PORT.md`. Repeat per preset class.
- `docs/attribution/THETIS-PROVENANCE.md` — add a row per new source file.
- Preserve verbatim any `// MW0LGE`, `// -W2PA`, `// TODO`, `// FIXME` inline comments
  in the needle-table and preset-construction regions.
- `scripts/verify-thetis-headers.py` and `scripts/check-new-ports.py` run pre-push
  (installed via `scripts/install-hooks.sh`).

## 7. Rollout & risk

- PR will be large. Target a single phase similar in scope to 3G-6.
- If the total net diff exceeds ~3,500 lines, split the on-container access work
  (§3.7) into a follow-up PR; the preset-collapse and dialog refactor (§3.1–3.6, 3.8,
  3.10) remain the core.
- Capture migration fixtures **before** starting work: save three real containers
  (ANAN MM, S-Meter, Power/SWR) from the current build as golden inputs for the
  legacy-load test.
- `MeterWidget` GPU render loop is not modified. Each new preset's `draw()` uses the same
  `QRhi` / `QPainter` overlay primitives as existing items.
- GPG-signed commits throughout. Standard `J.J. Boyd ~ KG4VCF` + `Co-Authored-By` trailer.

## 8. Commit sequencing (draft)

1. Add `AnanMultiMeterItem` class with byte-for-byte calibration port, attribution
   headers, PROVENANCE row, and `test_anan_multi_meter_item.cpp` green.
2. `CrossNeedleItem`.
3. `SMeterPresetItem`.
4. `PowerSwrPresetItem`.
5. `AlcPresetItem`.
6. `AdcBarPresetItem`.
7. `MagicEyePresetItem`.
8. `HistoryGraphPresetItem`.
9. `SignalTextPresetItem`.
10. `VfoDisplayPresetItem`.
11. Dialog: swap factories for new classes, remove preset path from `ItemGroup`.
12. Hybrid addition rule: available-list disable / "in use" marker; rejection on re-add.
13. In-use wrapper + drag-reorder + rename + right-click + primitive duplicate.
14. `+ Add` button in dialog.
15. `ContainerWidget` context menu + double-click + gear icon + `editRequested()`.
16. `FloatingContainer` event delegation.
17. Auto-name `Meter N`; menu cleanup; `View → Applets` submenu.
18. Legacy migration path + fixtures + `test_legacy_container_migration.cpp`.
19. Delete `ItemGroup`.
20. Manual verification checklist execution; CHANGELOG entry.

## 9. Open questions (resolved during brainstorming)

- Preset architecture direction: **A (first-class MeterItem subclasses)**.
- Duplicate rule scope: **C (per-container, presets only)**.
- Menu/access refactor: **bundle in this phase**.
- Multi-container editing: **keep modal; add `+ Add` button; keep dropdown switching**.
- Legacy migration mode: **tolerant — log warning and fall back to loose primitives
  when customization doesn't fit new class**.

## 10. Success criteria

- ANAN Multi Meter appears as one line in the in-use list with one property pane.
- Adding a preset a second time to the same container is blocked; primitives can be
  re-added.
- ANAN MM needles track the painted face arc at any container size.
- Right-clicking a container opens Edit without traversing the menu bar.
- `+ Add` in the dialog creates a new container without closing the dialog.
- Legacy saved containers load with warnings only in the rare customized-beyond-mapping
  case; otherwise upgrade cleanly and persist forward in the new form.
