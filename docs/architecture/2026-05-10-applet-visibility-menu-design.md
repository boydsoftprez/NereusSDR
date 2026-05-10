# Applet Visibility Menu — Design

> **Status:** Drafted 2026-05-10. Awaiting user review before implementation plan.
> **Branch:** TBD (likely off `main` after current epics merge)
> **Predecessor:** None (UX restoration of feature commented-out in `25597df`, 2026-05-02)
> **Successor:** Implementation plan via `superpowers:writing-plans`

## 1. Goal

Restore — and meaningfully extend — the ability to show or hide individual applets in the right-side `AppletPanelWidget`. The original feature lived under the top-bar **Containers** menu as a `addContainerToggle()` lambda (`src/gui/MainWindow.cpp:2773-2796`) and was disabled in commit `25597df` per `docs/superpowers/plans/2026-05-01-ui-polish-right-panel.md §Task 6` because the seven entries it carried were all ghost applets without functional wiring.

Since then, `PureSignalApplet` has shipped wired (`v0.4.0`, commit `2271f8a`), and the user wants the toggling capability back in two places:

1. The original top-bar location (Containers menu).
2. A new in-context entry point: a hamburger (`☰`) icon button on the right-side panel's banner.

Both menus stay in sync, share persistence, and only list applets that are actually wired and useful today.

## 2. Locked decisions (from brainstorm, 2026-05-10)

| # | Decision | Choice |
|---|---|---|
| 1 | Menu locations | Both — top menu (Containers) AND `☰` button on Container #0 banner |
| 2 | Applet inclusion | Only currently-wired applets (5: Rx, Tx, PhoneCw, Vax, PureSignal). Ghosts excluded until their phase ships. |
| 3 | Architecture | New `AppletVisibilityController` class — single source of truth shared by both menus and the panel. Fits existing controller pattern (Alex/Mox/Clarity/Antenna). |
| 4 | Order preservation | Hide/show via `QWidget::setVisible()` on the wrapper (not `removeApplet`/`addApplet`). Order stays stable across toggles. Requires new `AppletPanelWidget::setAppletVisible(applet, bool)` method. |
| 5 | Banner button gesture | Visible `☰` icon button. Discoverable. Right-click context menu rejected as too hidden. |
| 6 | Banner button location | A new ~22 px header row added to `AppletPanelWidget` itself (the panel has no banner today; not wrapped in `ContainerWidget`). Floating meter containers and other `ContainerWidget` instances don't get the button. |
| 7 | Persistence keys | `AppletRxVisible`, `AppletTxVisible`, `AppletPhoneCwVisible`, `AppletVaxVisible`, `AppletPureSignalVisible`. Stored as `True`/`False` strings per AppSettings convention. |
| 8 | Persistence scope | Global (not per-MAC). UI preference — survives radio swaps. |
| 9 | First-run defaults (shipped 5) | All 5 visible. Matches current behavior — no surprise for existing users. |
| 10 | Future-applet defaults | New applets added in later phases default to **visible** when their phase ships, so existing users discover the new surface on first launch of the new version. |
| 11 | Sync between menus | Both QActions update their `checked` state when controller emits `visibilityChanged`. `QSignalBlocker` prevents recursive toggle loops. |
| 12 | Phase 3F multi-pan readiness | Today: single global visibility map (only Container #0 hosts applets). At 3F: extend `AppletVisibilityController` to take a containerId. Persistence key schema becomes `Applet/<containerId>/<appletId>/Visible`. Out of scope for this design; called out as a known evolution path. |

## 3. UX summary

### 3.1 Top menu — Containers

Layout (additions in **bold**):

```
Containers
├─ Show Container...
├─ Edit Container ▸
├─ Reset Default Layout
├─ ─────────────── (existing separator, MainWindow.cpp:2764)
├─ Applets        (NEW section header — see Qt rendering note below)
├─ ✓ RX           (NEW)
├─ ✓ TX           (NEW)
├─ ✓ Phone / CW   (NEW)
├─ ✓ VAX          (NEW)
└─ ✓ PureSignal   (NEW)
```

Section-header rendering: prefer `QMenu::addSection("Applets")` (Qt 5.1+; styles vary across platforms but is the idiomatic choice). Fallback if the platform style ignores it: a disabled `QAction` with grey, slightly-smaller font.

Each of the 5 new entries is a checkable `QAction`. Toggling flips the check immediately and shows/hides the corresponding applet in the right-side panel. Display labels match the applet's `appletTitle()` virtual.

### 3.2 Banner button — AppletPanelWidget

**Important context.** The right-side AppletPanelWidget is *not* wrapped in a `ContainerWidget`. The `ContainerWidget` class (with its 4-button banner: axis-lock / pin / float / settings) is used for floating meter containers and overlay-docked containers — it is not the host of the applet panel. The applet panel sits directly in the main `QSplitter` with no banner header today.

Therefore: this design adds a **new thin header row** to `AppletPanelWidget` itself, above the existing MeterWidget header area. ~22 px tall, dark background matching the per-applet title bars (`Style::titleBarStyle()`), containing only the `☰` button right-aligned.

- Icon: `☰` (hamburger). 22×22 button.
- Tooltip: `"Show or hide applets."`
- Click: opens a `QMenu` aligned to the button's bottom-right corner.
- Menu contents: same 5 checkable items as the top-menu Applets section.

Floating meter containers and other `ContainerWidget` instances do not get the button (they don't host applets).

### 3.3 Behavior — both menus

- Toggling an entry persists immediately; survives quit/restart.
- Both menus stay synchronized: toggling via the `☰` updates the top menu's checkmark and vice versa.
- Hiding an applet hides the entire wrapper (title bar + body). The space collapses; remaining applets shift up.
- Showing an applet again restores it to its **original position in the stack** (RX always above TX, TX above Phone/CW, etc.) — never reshuffles.
- No animation in v1. Instant show/hide.

## 4. Architecture

### 4.1 New class — `AppletVisibilityController`

Files: `src/gui/applets/AppletVisibilityController.{h,cpp}`. Estimated ~80-120 lines.

Owned by `MainWindow`, constructed after `m_appletPanel`.

**Responsibilities:**
- Holds the registry of applet IDs, display names, and current visibility state.
- Reads initial state from `AppSettings` on first registration of each applet.
- Writes to `AppSettings` on every `setVisible` call (synchronous; small data; write-through is safe).
- Emits `visibilityChanged(QString id, bool visible)` whenever state flips.

**API sketch (final signatures TBD in implementation plan):**

```cpp
class AppletVisibilityController : public QObject {
    Q_OBJECT
public:
    explicit AppletVisibilityController(QObject* parent = nullptr);

    // Called once per applet at startup (typically from MainWindow).
    // defaultVisible is used only when no AppSettings key exists yet.
    void registerApplet(const QString& id,
                        const QString& displayName,
                        bool defaultVisible);

    // Public state accessors.
    bool isVisible(const QString& id) const;
    QStringList registeredIds() const;          // insertion order preserved
    QString displayName(const QString& id) const;

public slots:
    void setVisible(const QString& id, bool visible);

signals:
    void visibilityChanged(const QString& id, bool visible);

private:
    struct Entry { QString displayName; bool visible; };
    QMap<QString, Entry> m_entries;             // key = id
    QStringList m_order;                        // registration order
};
```

### 4.2 Modified — `AppletPanelWidget`

Add one method:

```cpp
void setAppletVisible(AppletWidget* applet, bool visible);
```

Implementation: looks up `m_wrappers[applet]`, calls `wrapper->setVisible(visible)`. No reparent, no destroy, no churn. Order in `m_stackLayout` is unchanged.

Existing `addApplet`/`removeApplet` API remains untouched for backward compatibility (other code paths may still use it).

### 4.3 Modified — `MainWindow`

In `buildDefaultContainerLayout()` (or equivalent): after the 5 wired applets are added to the panel, register each one with the controller:

```cpp
m_appletVis->registerApplet("Rx",         "RX",          true);
m_appletVis->registerApplet("Tx",         "TX",          true);
m_appletVis->registerApplet("PhoneCw",    "Phone / CW",  true);
m_appletVis->registerApplet("Vax",        "VAX",         true);
m_appletVis->registerApplet("PureSignal", "PureSignal",  true);
```

Maintain a side map `QHash<QString, AppletWidget*> m_appletsById` so the controller's signal can be routed to the right applet.

In the menu builder (the existing block around `MainWindow.cpp:2764`):
- After the existing `Reset Default Layout` action and separator, add a new separator.
- Add a disabled `"Applets"` section header QAction (greyed, non-clickable label).
- For each `id` in `m_appletVis->registeredIds()`, add a checkable QAction whose `toggled(bool)` slot calls `m_appletVis->setVisible(id, checked)`.
- Store these QActions in `QHash<QString, QAction*> m_appletMenuActions` for later checkmark sync.

In `AppletPanelWidget`:
- Add a `setBannerMenu(QMenu* menu)` API (or constructor-time `setBannerMenu` call from `MainWindow`) that installs the menu on the new ☰ button.
- Build the same 5 checkable actions parented to that menu, wired to `m_appletVis->setVisible(...)` via `toggled`.

In the controller's signal handler (a slot on `MainWindow`):
- Look up `applet = m_appletsById[id]`.
- Call `m_appletPanel->setAppletVisible(applet, visible)`.
- Sync both menus' QActions: `QSignalBlocker block(action); action->setChecked(visible);` for each menu's copy.

### 4.4 Data flow (recap)

```
User clicks toggle  (top menu OR banner ☰ menu)
    → QAction::toggled(bool)
    → AppletVisibilityController::setVisible(id, bool)
        → AppSettings::setValue("Applet<Id>Visible", "True"/"False")
        → emit visibilityChanged(id, bool)
            → MainWindow slot
                → m_appletPanel->setAppletVisible(applet, bool)  [wrapper hides/shows]
                → for each menu copy of the QAction:
                       QSignalBlocker → action->setChecked(bool)
```

## 5. Persistence

### 5.1 Keys

| Applet ID | AppSettings key |
|---|---|
| Rx | `AppletRxVisible` |
| Tx | `AppletTxVisible` |
| PhoneCw | `AppletPhoneCwVisible` |
| Vax | `AppletVaxVisible` |
| PureSignal | `AppletPureSignalVisible` |

Values: `"True"` / `"False"` strings, per `AppSettings` convention (CLAUDE.md §"Settings Persistence (AppSettings — NOT QSettings)").

### 5.2 Defaults

| Scenario | Behavior |
|---|---|
| Fresh install | All 5 keys absent → controller uses `defaultVisible=true` per registration → all 5 visible |
| Existing user, first launch with feature | Same — keys absent, all 5 visible (matches current behavior, zero surprise) |
| User has hidden Rx, restarts | `AppletRxVisible=False` persists, applied at startup, RX hidden until user re-enables |
| Future phase wires e.g. CwxApplet | Registration happens with `defaultVisible=true`. Existing users see "CW Keyer" appear in their panel + menu on first launch of new version. Can hide via menu if unwanted. |

### 5.3 Migration

None required. The feature has never persisted state before; absence of keys is the natural first-run condition.

## 6. Order preservation rationale

The original `addApplet`/`removeApplet` API destroys the wrapper on remove and creates a new one on re-add. This causes:

1. **Order loss.** A re-shown applet is appended at the end of the stack (`m_stackLayout->insertWidget(count - 1, wrapper)`), not restored to its original position.
2. **Widget churn.** Wrapper, title bar, label, grip dots — all destroyed and rebuilt. Cheap, but unnecessary.
3. **Latent reparent risk.** `AppletPanelWidget::clearHeaderWidget` carries a long comment (lines 117-128) explaining a Windows D3D11 swapchain bug triggered by reparenting `QRhiWidget` children. Applets don't currently host `QRhiWidget`, but adding `setVisible`-based hide/show eliminates the risk class entirely.

`setAppletVisible(applet, bool)` solves all three by simply calling `wrapper->setVisible(bool)` — wrapper stays in the layout, applet stays parented, and Qt's layout recomputes around the hidden wrapper (which reports zero `sizeHint()` when hidden).

## 7. Phase 3F multi-pan readiness

When 3F lands and additional containers can host their own applet sets, the visibility model needs a containerId axis. Path of least disruption:

1. `AppletVisibilityController::registerApplet` gains an optional `containerId` parameter (default = `"0"` for backward compatibility).
2. Persistence keys evolve to `Applet/<containerId>/<appletId>/Visible` (nested keys per AppSettings).
3. The `☰` button's menu reads only the current container's slice of the registry.
4. The top menu shows a per-container submenu structure: `Containers > Container #0 > Applets > ...`.

Out of scope for this design. Called out so the v1 schema doesn't paint into a corner. The single-container schema in §5.1 is forward-compatible: when 3F migration runs, a small one-time pass moves `AppletRxVisible` → `Applet/0/Rx/Visible`, etc.

## 8. Testing

### 8.1 Unit tests — `tst_applet_visibility_controller.cpp`

- Register one applet with `defaultVisible=true`, no AppSettings key → `isVisible(id) == true`.
- Register one applet with `defaultVisible=true`, AppSettings key `"False"` already present → `isVisible(id) == false` (persisted state wins).
- Call `setVisible(id, false)` → `isVisible(id) == false`, AppSettings key now `"False"`.
- Construct a fresh controller pointed at the same AppSettings → `isVisible(id) == false` (round-trip).
- Spy on `visibilityChanged` signal: verify exactly one emission per `setVisible` call with correct args. Verify no emission when value doesn't change (idempotent).
- Register two applets, check `registeredIds()` returns insertion order.

### 8.2 Panel test — `tst_applet_panel_set_visible.cpp`

- Build `AppletPanelWidget`, add 3 mock applets (Rx, Tx, PhoneCw equivalents).
- Call `setAppletVisible(tx, false)` → `m_wrappers[tx]->isVisible() == false`. Wrapper still in layout (`m_stackLayout->indexOf(wrapper) >= 0`).
- Call `setAppletVisible(tx, true)` → wrapper visible, layout index unchanged.
- Verify `setAppletVisible(nullptr, ...)` is a no-op (no crash).
- Verify `setAppletVisible(applet_not_in_panel, ...)` is a no-op.

### 8.3 Wire-up test — `tst_mainwindow_applet_menu.cpp`

- Build `MainWindow`, open Containers menu, locate the Applets section: assert 5 actions present with correct text, all checkable, all initially checked.
- Trigger toggle on TX action programmatically → assert TX applet wrapper is hidden in panel; assert AppSettings has `AppletTxVisible="False"`.
- Locate the `☰` banner button on Container #0; open its menu → assert same 5 actions present with TX showing unchecked (sync verified).
- Trigger toggle on TX via the banner menu → assert TX visible again; top menu's TX action is also checked.
- Verify `QSignalBlocker` prevents recursive emission (toggle once, controller emits once).

### 8.4 Verification (manual, at bench)

Not gated on this PR but documented for the change-log entry:

- Launch app, confirm all 5 applets visible.
- Toggle each via top menu, confirm panel updates.
- Toggle each via `☰` button, confirm top menu's checkmark mirrors.
- Quit, relaunch, confirm hidden state survives.
- Hide all 5, confirm panel collapses to header (S-Meter) only.
- Re-show all, confirm original order (Rx, Tx, PhoneCw, Vax, PureSignal top-to-bottom).

## 9. Out of scope

- Per-container visibility (deferred to Phase 3F — see §7).
- Drag-to-reorder applets within the panel (separate feature; if asked for, would need its own design).
- Animated show/hide transitions (could add later; not v1).
- Visibility toggling for ghost applets that aren't wired yet (excluded by design — they don't appear in either menu until their phase ships).
- Applet visibility as part of skin/layout import (Phase 3H concern; if a skin later persists these keys, the existing AppSettings layer handles it).
- Right-click context menu on the banner header (rejected; visible `☰` button preferred for discoverability).

## 10. Source attribution

This is a NereusSDR-original feature. No Thetis equivalent (Thetis exposes container-level show/hide via setup checkboxes — `setup.cs:24443-24657 [v2.10.3.13]` — but no per-applet-toggle pattern, since Thetis applets are static UserControl members of fixed container forms).

The `AppletPanelWidget` itself is an AetherSDR port (header at `AppletPanelWidget.cpp:1-24` credits Jeremy/KK7GWY); the new `setAppletVisible` method is a NereusSDR addition. Per CLAUDE.md style guide, the new method gets a comment noting it as NereusSDR-original; the existing AetherSDR header stays intact.

`AppletVisibilityController` is wholly NereusSDR-original. No upstream cite needed.

## 11. Implementation order (rough)

To be detailed in the implementation plan via `superpowers:writing-plans`. Sketch:

1. `AppletVisibilityController` class + unit tests.
2. `AppletPanelWidget::setAppletVisible` + panel test.
3. Wire `MainWindow`: construct controller, register 5 applets, bind controller signal to panel.
4. Top menu: add separator, section header, 5 checkable actions, bind to controller.
5. Banner button: add `☰` to Container #0, build menu, bind to controller. May need a small `ContainerWidget` API addition.
6. Two-way sync: controller signal → both menus' `setChecked` with `QSignalBlocker`.
7. Wire-up test + bench verification.
