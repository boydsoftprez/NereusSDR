# Applet Visibility Menu Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore per-applet show/hide toggling with two access points — the top-bar **Containers** menu and a new ☰ button on the right-side `AppletPanelWidget` — backed by a small `AppletVisibilityController` that owns persistence and keeps both menus in sync.

**Architecture:** A new `AppletVisibilityController` is the single source of truth. Both menus call into it; it persists each change to AppSettings and emits `visibilityChanged`. `MainWindow` connects that signal to a slot that hides/shows the applet wrapper in the panel and updates the checked state on both menu copies (via `QSignalBlocker`). `AppletPanelWidget` gains two changes: a new `setAppletVisible(applet, bool)` method that toggles wrapper visibility (preserving order) and a thin top banner row that hosts the ☰ button.

**Tech Stack:** C++20 / Qt6 (QObject, QMenu, QAction, QPushButton, QSignalBlocker), QtTest, AppSettings (`src/core/AppSettings.h` — never `QSettings`), CMake / Ninja.

**Spec:** `docs/architecture/2026-05-10-applet-visibility-menu-design.md`

**User-visible strings rule:** All tooltip / button / menu text in this plan is plain English. No source cites (`Thetis X:N`, `[v2.10.3.13]`, etc.) appear inside any QString — those go in `//` comments adjacent to the QString line, never inside the user-visible text.

---

## File Structure

| Path | Action | Responsibility |
|---|---|---|
| `src/gui/applets/AppletVisibilityController.h` | CREATE | Public class header — registry, state accessors, `setVisible` slot, `visibilityChanged` signal |
| `src/gui/applets/AppletVisibilityController.cpp` | CREATE | AppSettings round-trip, signal emission, default handling |
| `src/gui/applets/AppletPanelWidget.h` | MODIFY | New public `setAppletVisible(AppletWidget*, bool)`; new `setBannerMenu(QMenu*)`; private members for the banner row + ☰ button |
| `src/gui/applets/AppletPanelWidget.cpp` | MODIFY | Implement above; add a 22 px top banner row (hidden until `setBannerMenu` called) |
| `src/gui/MainWindow.h` | MODIFY | Add `m_appletVis` (owned), `m_appletsById` (QHash), `m_topMenuAppletActions` + `m_bannerAppletActions` (QHash<QString,QAction*>); add `m_appletsMenu` (the QMenu used by both surfaces, or two menus if needed) |
| `src/gui/MainWindow.cpp` | MODIFY | Construct controller after panel; register 5 applets; build top-menu Applets section; build banner menu; wire `visibilityChanged` slot |
| `tests/tst_applet_visibility_controller.cpp` | CREATE | Unit tests: defaults, persistence round-trip, signal emission, idempotent setVisible, registration order |
| `tests/tst_applet_panel_set_visible.cpp` | CREATE | Panel test: setAppletVisible toggles wrapper visibility while preserving layout index; null-safety |
| `tests/tst_applet_visibility_menu_wiring.cpp` | CREATE | Wiring test: MainWindow's top-menu Applets actions + banner-menu actions both flip the controller AND mirror each other's checked state |
| `CMakeLists.txt` | MODIFY | Add `src/gui/applets/AppletVisibilityController.cpp` to source list near existing applet sources (around line 707) |
| `tests/CMakeLists.txt` | MODIFY | Three `nereus_add_test(...)` registrations for the new test files |

---

## Task 1: AppletPanelWidget gains `setAppletVisible(applet, bool)`

This is the prerequisite for everything else: a way to hide/show an applet without losing its position in the stack.

**Files:**
- Modify: `src/gui/applets/AppletPanelWidget.h`
- Modify: `src/gui/applets/AppletPanelWidget.cpp`
- Create: `tests/tst_applet_panel_set_visible.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/tst_applet_panel_set_visible.cpp`:

```cpp
// Verify AppletPanelWidget::setAppletVisible toggles wrapper visibility
// while keeping the applet's position in the stack layout intact.

#include <QApplication>
#include <QtTest/QtTest>
#include <QVBoxLayout>
#include <QScrollArea>
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/AppletWidget.h"

using namespace NereusSDR;

namespace {
// Minimal AppletWidget concrete subclass for tests. Real applets pull in
// half the radio model; this stub avoids that.
class FakeApplet : public AppletWidget {
public:
    explicit FakeApplet(const QString& title) : m_title(title) {}
    QString appletTitle() const override { return m_title; }
private:
    QString m_title;
};
} // anon

class TstAppletPanelSetVisible : public QObject {
    Q_OBJECT
private slots:
    void hides_wrapper_without_changing_layout_index();
    void shows_wrapper_again_at_same_index();
    void null_applet_is_noop();
    void unknown_applet_is_noop();
};

// Helper: get the wrapper QWidget for an applet by walking the scroll area's
// inner stack layout and finding the wrapper whose child is the applet.
static QWidget* wrapperFor(AppletPanelWidget& panel, AppletWidget* applet)
{
    auto* scroll = panel.findChild<QScrollArea*>();
    if (!scroll) { return nullptr; }
    auto* stackWidget = scroll->widget();
    if (!stackWidget) { return nullptr; }
    auto* layout = qobject_cast<QVBoxLayout*>(stackWidget->layout());
    if (!layout) { return nullptr; }
    for (int i = 0; i < layout->count(); ++i) {
        auto* w = layout->itemAt(i)->widget();
        if (w && w->isAncestorOf(applet)) { return w; }
    }
    return nullptr;
}

static int indexOf(AppletPanelWidget& panel, QWidget* wrapper)
{
    auto* scroll = panel.findChild<QScrollArea*>();
    auto* stackWidget = scroll->widget();
    auto* layout = qobject_cast<QVBoxLayout*>(stackWidget->layout());
    return layout->indexOf(wrapper);
}

void TstAppletPanelSetVisible::hides_wrapper_without_changing_layout_index()
{
    AppletPanelWidget panel;
    auto* a = new FakeApplet(QStringLiteral("A"));
    auto* b = new FakeApplet(QStringLiteral("B"));
    panel.addApplet(a);
    panel.addApplet(b);

    QWidget* wrapperB = wrapperFor(panel, b);
    QVERIFY(wrapperB);
    int idxBefore = indexOf(panel, wrapperB);

    panel.setAppletVisible(b, false);

    QVERIFY(!wrapperB->isVisible());
    QCOMPARE(indexOf(panel, wrapperB), idxBefore);
}

void TstAppletPanelSetVisible::shows_wrapper_again_at_same_index()
{
    AppletPanelWidget panel;
    panel.show();  // needed so isVisible() returns true after setVisible(true)
    auto* a = new FakeApplet(QStringLiteral("A"));
    auto* b = new FakeApplet(QStringLiteral("B"));
    panel.addApplet(a);
    panel.addApplet(b);

    QWidget* wrapperB = wrapperFor(panel, b);
    int idxBefore = indexOf(panel, wrapperB);

    panel.setAppletVisible(b, false);
    panel.setAppletVisible(b, true);

    QVERIFY(wrapperB->isVisible());
    QCOMPARE(indexOf(panel, wrapperB), idxBefore);
}

void TstAppletPanelSetVisible::null_applet_is_noop()
{
    AppletPanelWidget panel;
    panel.setAppletVisible(nullptr, false);  // must not crash
    panel.setAppletVisible(nullptr, true);
}

void TstAppletPanelSetVisible::unknown_applet_is_noop()
{
    AppletPanelWidget panel;
    auto* orphan = new FakeApplet(QStringLiteral("Orphan"));
    panel.setAppletVisible(orphan, false);  // not added; must not crash
    delete orphan;
}

QTEST_MAIN(TstAppletPanelSetVisible)
#include "tst_applet_panel_set_visible.moc"
```

- [ ] **Step 2: Register the test in CMake**

Edit `tests/CMakeLists.txt`. Find the existing `nereus_add_test(tst_applet_panel_gutter)` line (around line ~80; grep for it). Add immediately after:

```cmake
# -- AppletPanelWidget setAppletVisible: order-preserving show/hide --
nereus_add_test(tst_applet_panel_set_visible)
```

- [ ] **Step 3: Build and run the test to verify it fails**

```bash
cmake --build build -j --target tst_applet_panel_set_visible
ctest --test-dir build -R tst_applet_panel_set_visible -V
```

Expected: build fails with `error: 'class NereusSDR::AppletPanelWidget' has no member named 'setAppletVisible'` (or similar).

- [ ] **Step 4: Add `setAppletVisible` to `AppletPanelWidget.h`**

Open `src/gui/applets/AppletPanelWidget.h`. Find the existing `void removeApplet(AppletWidget* applet);` declaration. Add immediately after it:

```cpp
    // Toggle visibility of an already-added applet without removing it
    // from the layout. Preserves stack position when re-shown. No-op for
    // null or unknown applets. NereusSDR-original (no Thetis equivalent).
    void setAppletVisible(AppletWidget* applet, bool visible);
```

- [ ] **Step 5: Implement `setAppletVisible` in `AppletPanelWidget.cpp`**

Open `src/gui/applets/AppletPanelWidget.cpp`. Find the existing `removeApplet` implementation (lines 163-178). Add immediately after the closing brace:

```cpp
void AppletPanelWidget::setAppletVisible(AppletWidget* applet, bool visible)
{
    if (!applet) { return; }
    QWidget* wrapper = m_wrappers.value(applet, nullptr);
    if (!wrapper) { return; }  // applet not in this panel
    wrapper->setVisible(visible);
}
```

- [ ] **Step 6: Build and run the test to verify it passes**

```bash
cmake --build build -j --target tst_applet_panel_set_visible
ctest --test-dir build -R tst_applet_panel_set_visible -V
```

Expected: PASS, all 4 cases green.

- [ ] **Step 7: Commit**

```bash
git add src/gui/applets/AppletPanelWidget.h \
        src/gui/applets/AppletPanelWidget.cpp \
        tests/tst_applet_panel_set_visible.cpp \
        tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(applet-panel): setAppletVisible preserves stack order

Adds an order-preserving alternative to addApplet/removeApplet for
the upcoming applet visibility menu. setVisible() on the wrapper
keeps it in the layout (zero sizeHint when hidden) so re-shows
restore to the original position.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: AppletVisibilityController class

The bookkeeper. Owns the visibility map, AppSettings round-trip, and change signal.

**Files:**
- Create: `src/gui/applets/AppletVisibilityController.h`
- Create: `src/gui/applets/AppletVisibilityController.cpp`
- Create: `tests/tst_applet_visibility_controller.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/tst_applet_visibility_controller.cpp`:

```cpp
// Verify AppletVisibilityController state, persistence, and signal emission.
// no-port-check: NereusSDR-original — no Thetis source.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "gui/applets/AppletVisibilityController.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TstAppletVisibilityController : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()    { AppSettings::instance().clear(); }
    void cleanup()         { AppSettings::instance().clear(); }

    void default_visible_when_no_settings_key();
    void default_hidden_when_no_settings_key();
    void persisted_value_overrides_default();
    void setVisible_persists_and_emits();
    void setVisible_idempotent_no_emit_on_same_value();
    void registeredIds_returns_insertion_order();
    void displayName_round_trip();
};

void TstAppletVisibilityController::default_visible_when_no_settings_key()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Rx"), QStringLiteral("RX"), true);
    QVERIFY(c.isVisible(QStringLiteral("Rx")));
}

void TstAppletVisibilityController::default_hidden_when_no_settings_key()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Cwx"), QStringLiteral("CW Keyer"), false);
    QVERIFY(!c.isVisible(QStringLiteral("Cwx")));
}

void TstAppletVisibilityController::persisted_value_overrides_default()
{
    AppSettings::instance().setValue(QStringLiteral("AppletRxVisible"),
                                     QStringLiteral("False"));
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Rx"), QStringLiteral("RX"), true);
    QVERIFY(!c.isVisible(QStringLiteral("Rx")));  // persisted False wins
}

void TstAppletVisibilityController::setVisible_persists_and_emits()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Tx"), QStringLiteral("TX"), true);
    QSignalSpy spy(&c, &AppletVisibilityController::visibilityChanged);

    c.setVisible(QStringLiteral("Tx"), false);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("Tx"));
    QCOMPARE(spy.first().at(1).toBool(), false);
    QCOMPARE(AppSettings::instance().value(QStringLiteral("AppletTxVisible"))
             .toString(), QStringLiteral("False"));

    // Round-trip: a fresh controller picks up the persisted False.
    AppletVisibilityController c2;
    c2.registerApplet(QStringLiteral("Tx"), QStringLiteral("TX"), true);
    QVERIFY(!c2.isVisible(QStringLiteral("Tx")));
}

void TstAppletVisibilityController::setVisible_idempotent_no_emit_on_same_value()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Vax"), QStringLiteral("VAX"), true);
    QSignalSpy spy(&c, &AppletVisibilityController::visibilityChanged);

    c.setVisible(QStringLiteral("Vax"), true);   // already true → no emit
    QCOMPARE(spy.count(), 0);

    c.setVisible(QStringLiteral("Vax"), false);  // change → emit
    QCOMPARE(spy.count(), 1);

    c.setVisible(QStringLiteral("Vax"), false);  // unchanged → no emit
    QCOMPARE(spy.count(), 1);
}

void TstAppletVisibilityController::registeredIds_returns_insertion_order()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Rx"),         QStringLiteral("RX"),          true);
    c.registerApplet(QStringLiteral("Tx"),         QStringLiteral("TX"),          true);
    c.registerApplet(QStringLiteral("PhoneCw"),    QStringLiteral("Phone / CW"),  true);
    c.registerApplet(QStringLiteral("Vax"),        QStringLiteral("VAX"),         true);
    c.registerApplet(QStringLiteral("PureSignal"), QStringLiteral("PureSignal"),  true);

    QStringList expected{
        QStringLiteral("Rx"), QStringLiteral("Tx"), QStringLiteral("PhoneCw"),
        QStringLiteral("Vax"), QStringLiteral("PureSignal")
    };
    QCOMPARE(c.registeredIds(), expected);
}

void TstAppletVisibilityController::displayName_round_trip()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("PhoneCw"), QStringLiteral("Phone / CW"), true);
    QCOMPARE(c.displayName(QStringLiteral("PhoneCw")),
             QStringLiteral("Phone / CW"));
    QCOMPARE(c.displayName(QStringLiteral("Unknown")), QString{});
}

QTEST_MAIN(TstAppletVisibilityController)
#include "tst_applet_visibility_controller.moc"
```

- [ ] **Step 2: Register the test in CMake**

In `tests/CMakeLists.txt`, add near the other applet tests:

```cmake
# -- AppletVisibilityController: state, persistence, signal emission --
nereus_add_test(tst_applet_visibility_controller)
```

- [ ] **Step 3: Build and run — verify failure**

```bash
cmake --build build -j --target tst_applet_visibility_controller
```

Expected: build fails with "AppletVisibilityController.h: No such file or directory".

- [ ] **Step 4: Create `AppletVisibilityController.h`**

Create `src/gui/applets/AppletVisibilityController.h`:

```cpp
#pragma once

// =================================================================
// src/gui/applets/AppletVisibilityController.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. No Thetis equivalent — Thetis exposes
// container-level show/hide via setup checkboxes, not per-applet
// toggles. AetherSDR has no equivalent.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-10 — Created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code. Backs the Containers >
//                 Applets menu and the right-side panel ☰ menu.
// =================================================================

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>

namespace NereusSDR {

class AppletVisibilityController : public QObject {
    Q_OBJECT
public:
    explicit AppletVisibilityController(QObject* parent = nullptr);

    // Register an applet's id, display name, and default visibility.
    // If an AppSettings key for this id already exists, the persisted
    // value wins over defaultVisible. Idempotent on the same id; later
    // calls overwrite the display name but preserve current state.
    void registerApplet(const QString& id,
                        const QString& displayName,
                        bool defaultVisible);

    bool isVisible(const QString& id) const;
    QStringList registeredIds() const;       // insertion order preserved
    QString displayName(const QString& id) const;

public slots:
    void setVisible(const QString& id, bool visible);

signals:
    // Emitted only when the value actually changes.
    void visibilityChanged(const QString& id, bool visible);

private:
    static QString settingsKey(const QString& id);  // "AppletRxVisible" etc.

    struct Entry {
        QString displayName;
        bool visible{true};
    };
    QHash<QString, Entry> m_entries;   // id -> entry
    QStringList m_order;               // registration order
};

} // namespace NereusSDR
```

- [ ] **Step 5: Create `AppletVisibilityController.cpp`**

Create `src/gui/applets/AppletVisibilityController.cpp`:

```cpp
// =================================================================
// src/gui/applets/AppletVisibilityController.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See header for attribution.
//
// =================================================================

#include "AppletVisibilityController.h"
#include "core/AppSettings.h"

namespace NereusSDR {

AppletVisibilityController::AppletVisibilityController(QObject* parent)
    : QObject(parent)
{
}

QString AppletVisibilityController::settingsKey(const QString& id)
{
    return QStringLiteral("Applet") + id + QStringLiteral("Visible");
}

void AppletVisibilityController::registerApplet(const QString& id,
                                                 const QString& displayName,
                                                 bool defaultVisible)
{
    if (id.isEmpty()) { return; }

    if (!m_entries.contains(id)) {
        m_order.append(id);
    }

    Entry& e = m_entries[id];
    e.displayName = displayName;

    // Pick up persisted value if present, else use defaultVisible.
    const QString stored = AppSettings::instance()
        .value(settingsKey(id), QString{}).toString();
    if (stored == QStringLiteral("True")) {
        e.visible = true;
    } else if (stored == QStringLiteral("False")) {
        e.visible = false;
    } else {
        e.visible = defaultVisible;
    }
}

bool AppletVisibilityController::isVisible(const QString& id) const
{
    auto it = m_entries.find(id);
    return it != m_entries.end() ? it->visible : false;
}

QStringList AppletVisibilityController::registeredIds() const
{
    return m_order;
}

QString AppletVisibilityController::displayName(const QString& id) const
{
    auto it = m_entries.find(id);
    return it != m_entries.end() ? it->displayName : QString{};
}

void AppletVisibilityController::setVisible(const QString& id, bool visible)
{
    auto it = m_entries.find(id);
    if (it == m_entries.end()) { return; }
    if (it->visible == visible) { return; }   // idempotent no-op

    it->visible = visible;
    AppSettings::instance().setValue(
        settingsKey(id),
        visible ? QStringLiteral("True") : QStringLiteral("False"));
    emit visibilityChanged(id, visible);
}

} // namespace NereusSDR
```

- [ ] **Step 6: Add the new source file to the main build**

Edit the top-level `CMakeLists.txt`. Find the line `src/gui/applets/AppletPanelWidget.cpp` (around line 707). Add immediately after:

```cmake
    src/gui/applets/AppletVisibilityController.cpp
```

- [ ] **Step 7: Build and run the test — verify pass**

```bash
cmake --build build -j --target tst_applet_visibility_controller
ctest --test-dir build -R tst_applet_visibility_controller -V
```

Expected: PASS, all 7 cases green.

- [ ] **Step 8: Commit**

```bash
git add src/gui/applets/AppletVisibilityController.h \
        src/gui/applets/AppletVisibilityController.cpp \
        tests/tst_applet_visibility_controller.cpp \
        CMakeLists.txt \
        tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(applets): AppletVisibilityController + AppSettings round-trip

Bookkeeper class that owns per-applet visibility state, persists
each change to AppSettings (key format AppletXxxVisible), and emits
visibilityChanged on actual changes. Backs the upcoming Containers
> Applets menu and the right-side panel hamburger menu.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: AppletPanelWidget gains a top banner row + ☰ button

The right-side applet panel has no banner today. Add a thin (22 px) row at the top with one button on the right. The button is hidden until `setBannerMenu(QMenu*)` installs a menu on it (defensive — keeps the panel usable from any test that doesn't exercise the menu).

**Files:**
- Modify: `src/gui/applets/AppletPanelWidget.h`
- Modify: `src/gui/applets/AppletPanelWidget.cpp`

This task has no new test file — it's UI scaffolding that will be exercised by Task 7's wiring test. We do verify the button appears once a menu is set, via a quick check appended to the existing `tst_applet_panel_gutter.cpp` (or a small standalone file if you prefer). For brevity here, we extend the existing gutter test.

- [ ] **Step 1: Add a check to existing gutter test**

In `tests/tst_applet_panel_gutter.cpp`, add a new private slot declaration and implementation:

```cpp
// In the class:
    void bannerButtonHiddenUntilMenuSet();
    void bannerButtonShownAndOpensMenuOnceSet();

// Implementations (append at end of file, before QTEST_MAIN):
#include <QMenu>
#include <QPushButton>

void TstAppletPanelGutter::bannerButtonHiddenUntilMenuSet()
{
    AppletPanelWidget panel;
    auto* btn = panel.findChild<QPushButton*>(
        QStringLiteral("appletPanelBannerButton"));
    QVERIFY(btn != nullptr);     // exists in widget tree
    QVERIFY(!btn->isVisible());  // hidden by default
}

void TstAppletPanelGutter::bannerButtonShownAndOpensMenuOnceSet()
{
    AppletPanelWidget panel;
    panel.show();
    auto* menu = new QMenu(&panel);
    menu->addAction(QStringLiteral("dummy"));
    panel.setBannerMenu(menu);

    auto* btn = panel.findChild<QPushButton*>(
        QStringLiteral("appletPanelBannerButton"));
    QVERIFY(btn != nullptr);
    QVERIFY(btn->isVisible());
    QCOMPARE(btn->menu(), menu);
}
```

- [ ] **Step 2: Build and run — verify failure**

```bash
cmake --build build -j --target tst_applet_panel_gutter
ctest --test-dir build -R tst_applet_panel_gutter -V
```

Expected: FAIL with "no member named setBannerMenu" or similar.

- [ ] **Step 3: Add banner row + button to `AppletPanelWidget.h`**

Open `src/gui/applets/AppletPanelWidget.h`. Find the `private:` section near the bottom. Add:

```cpp
public:
    // Install a menu on the panel's top-right ☰ button. Until this is
    // called, the button is hidden. Pass nullptr to remove the menu and
    // re-hide the button.
    void setBannerMenu(QMenu* menu);

private:
    // ... existing private members ...
    QWidget*     m_bannerRow{nullptr};      // 22 px top header row
    QPushButton* m_bannerMenuButton{nullptr};  // ☰ button (right-aligned)
```

Also add forward declarations at the top of the file (after existing forward decls):

```cpp
class QPushButton;
class QMenu;
```

- [ ] **Step 4: Implement banner row in `AppletPanelWidget.cpp`**

Open `src/gui/applets/AppletPanelWidget.cpp`. Add to the `#include` block:

```cpp
#include <QPushButton>
#include <QMenu>
```

In the constructor (`AppletPanelWidget::AppletPanelWidget`), find the line `m_rootLayout->addLayout(m_headerLayout);` (around line 56). Insert BEFORE that line:

```cpp
    // Banner row at the very top — hosts the ☰ menu button (hidden
    // until setBannerMenu installs a menu).
    m_bannerRow = new QWidget(this);
    m_bannerRow->setFixedHeight(22);
    m_bannerRow->setStyleSheet(Style::titleBarStyle());
    auto* bannerLayout = new QHBoxLayout(m_bannerRow);
    bannerLayout->setContentsMargins(2, 0, 4, 0);
    bannerLayout->setSpacing(4);
    bannerLayout->addStretch();

    m_bannerMenuButton = new QPushButton(QStringLiteral("☰"), m_bannerRow);
    m_bannerMenuButton->setObjectName(QStringLiteral("appletPanelBannerButton"));
    m_bannerMenuButton->setFixedSize(22, 18);
    // User-visible tooltip — plain English, no source cites.
    m_bannerMenuButton->setToolTip(QStringLiteral("Show or hide applets"));
    m_bannerMenuButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: transparent; color: %1;"
        "  border: none; font-size: 12px;"
        "}"
        "QPushButton:hover { color: %2; }"
    ).arg(Style::kTitleText, Style::kAccent));
    m_bannerMenuButton->hide();   // hidden until setBannerMenu called
    bannerLayout->addWidget(m_bannerMenuButton);

    m_rootLayout->addWidget(m_bannerRow);
```

Then add the `setBannerMenu` implementation near `addApplet` / `removeApplet`:

```cpp
void AppletPanelWidget::setBannerMenu(QMenu* menu)
{
    if (!m_bannerMenuButton) { return; }
    m_bannerMenuButton->setMenu(menu);
    m_bannerMenuButton->setVisible(menu != nullptr);
}
```

(If `Style::kAccent` does not exist, substitute the project's accent colour constant; grep `Style::k` in `src/gui/StyleConstants.h` for the available list. `Style::kTitleText` is verified present at `AppletPanelWidget.cpp:218`.)

- [ ] **Step 5: Build and run the gutter test — verify pass**

```bash
cmake --build build -j --target tst_applet_panel_gutter
ctest --test-dir build -R tst_applet_panel_gutter -V
```

Expected: PASS, including the two new banner cases plus the original gutter case.

- [ ] **Step 6: Commit**

```bash
git add src/gui/applets/AppletPanelWidget.h \
        src/gui/applets/AppletPanelWidget.cpp \
        tests/tst_applet_panel_gutter.cpp
git commit -m "$(cat <<'EOF'
feat(applet-panel): top banner row with hamburger menu button

22 px header row above the existing MeterWidget header. Hosts a
single right-aligned ☰ button, hidden until setBannerMenu installs
a menu. Backs the in-context applet show/hide menu wired in a
follow-up commit.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: MainWindow constructs the controller and registers the 5 wired applets

Wire the controller into MainWindow's startup flow. This task only registers applets and connects the controller's signal to the panel — the menus come in Tasks 5 and 6.

**Files:**
- Modify: `src/gui/MainWindow.h`
- Modify: `src/gui/MainWindow.cpp`

- [ ] **Step 1: Add member declarations to `MainWindow.h`**

Open `src/gui/MainWindow.h`. Add forward declarations near other forward decls:

```cpp
class AppletVisibilityController;
class AppletWidget;
```

In the `private:` section, add:

```cpp
    AppletVisibilityController* m_appletVis{nullptr};
    QHash<QString, AppletWidget*> m_appletsById;
```

Add the include in `MainWindow.cpp` (don't include in header — forward declare suffices):

```cpp
#include "gui/applets/AppletVisibilityController.h"
```

- [ ] **Step 2: Construct controller and register applets**

Open `src/gui/MainWindow.cpp`. Find line 1993 (the `panel->addApplet(m_pureSignalApplet);` call — the last applet add). Insert AFTER that line, before the next blank line / `}`:

```cpp
    // ── Applet visibility controller (Containers > Applets + ☰ menus) ──
    // NereusSDR-original. Backs the show/hide menu surfaces. Registration
    // order matches the order the applets were added above; toggle
    // defaults are all true (existing layout preserved on first launch).
    m_appletVis = new AppletVisibilityController(this);

    m_appletsById[QStringLiteral("Rx")]         = m_rxApplet;
    m_appletsById[QStringLiteral("Tx")]         = txApplet;       // local
    m_appletsById[QStringLiteral("PhoneCw")]    = m_phoneCwApplet;
    m_appletsById[QStringLiteral("Vax")]        = m_vaxApplet;
    m_appletsById[QStringLiteral("PureSignal")] = m_pureSignalApplet;

    // Display names match each applet's appletTitle() — keep in sync if
    // an applet's title changes.
    m_appletVis->registerApplet(QStringLiteral("Rx"),
                                QStringLiteral("RX"),         true);
    m_appletVis->registerApplet(QStringLiteral("Tx"),
                                QStringLiteral("TX"),         true);
    m_appletVis->registerApplet(QStringLiteral("PhoneCw"),
                                QStringLiteral("Phone / CW"), true);
    m_appletVis->registerApplet(QStringLiteral("Vax"),
                                QStringLiteral("VAX"),        true);
    m_appletVis->registerApplet(QStringLiteral("PureSignal"),
                                QStringLiteral("PureSignal"), true);

    // Apply initial visibility state from the controller (in case
    // AppSettings already had values from a prior session).
    for (const QString& id : m_appletVis->registeredIds()) {
        if (auto* a = m_appletsById.value(id, nullptr)) {
            panel->setAppletVisible(a, m_appletVis->isVisible(id));
        }
    }

    // Pump future visibility changes from the controller into the panel.
    connect(m_appletVis, &AppletVisibilityController::visibilityChanged,
            this, [this](const QString& id, bool visible) {
        if (auto* a = m_appletsById.value(id, nullptr)) {
            if (m_appletPanel) {
                m_appletPanel->setAppletVisible(a, visible);
            }
        }
    });
```

(Note: `txApplet` is the local name at line 1859; `m_txApplet` is the cached pointer. Use whichever is in scope at the insertion point. Both refer to the same widget. Prefer `m_txApplet` if it's already set — verify by reading lines 1859-1861.)

- [ ] **Step 3: Build the app — verify it still compiles and runs**

```bash
cmake --build build -j
./build/NereusSDR &
sleep 3
pkill -f NereusSDR
```

Expected: clean build, app launches, all 5 applets visible (no functional change yet — the controller exists but no menus consume it).

- [ ] **Step 4: Commit**

```bash
git add src/gui/MainWindow.h src/gui/MainWindow.cpp
git commit -m "$(cat <<'EOF'
feat(main-window): register 5 wired applets with visibility controller

Constructs AppletVisibilityController after the panel is built,
registers Rx / Tx / PhoneCw / Vax / PureSignal with their display
names, applies any persisted visibility state at startup, and
pumps later changes from the controller into the panel.

No menu surfaces yet — those come in the next two commits.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Top-menu Applets section under Containers

Restore the Containers > Applets section that was disabled in `25597df`, now driven by the controller.

**Files:**
- Modify: `src/gui/MainWindow.h` (add `m_topMenuAppletActions` map)
- Modify: `src/gui/MainWindow.cpp`

- [ ] **Step 1: Add action map declaration**

In `src/gui/MainWindow.h`, near the `m_appletsById` line added in Task 4, add:

```cpp
    QHash<QString, QAction*> m_topMenuAppletActions;
```

Add the include for QAction at top of MainWindow.h if not already present. (It almost certainly is — MainWindow has many menus.)

- [ ] **Step 2: Replace the dead toggle block in `MainWindow.cpp`**

Open `src/gui/MainWindow.cpp`. Find the dead block at lines 2766-2796 (the commented-out `addContainerToggle` lambda and the 7 ghost-applet entries). Replace the entire block with:

```cpp
    // ── Containers > Applets section ─────────────────────────────────────
    // Show/hide toggles for each currently-wired applet. Backed by
    // m_appletVis (AppletVisibilityController). Two-way sync with the
    // ☰ menu on AppletPanelWidget happens via the controller's
    // visibilityChanged signal.
    //
    // Predecessor: dead lambda at MainWindow.cpp 2766-2796 was disabled
    // in 25597df because its 7 entries were all ghost applets. The new
    // section ships only currently-wired applets. Add new entries here
    // as additional applets ship (default visible per design §5.2).
    if (m_appletVis) {
        // Section header. addSection is the idiomatic Qt API; falls back
        // gracefully on platforms where it renders as a plain label.
        containersMenu->addSection(QStringLiteral("Applets"));

        for (const QString& id : m_appletVis->registeredIds()) {
            QAction* act = containersMenu->addAction(
                m_appletVis->displayName(id));
            act->setCheckable(true);
            act->setChecked(m_appletVis->isVisible(id));
            // User-visible tooltip — plain English, no source cites.
            act->setToolTip(QStringLiteral("Show or hide the %1 applet")
                            .arg(m_appletVis->displayName(id)));

            connect(act, &QAction::toggled, this, [this, id](bool checked) {
                if (m_appletVis) { m_appletVis->setVisible(id, checked); }
            });
            m_topMenuAppletActions.insert(id, act);
        }

        // Sync checkmark when the controller's state changes (e.g. via
        // the banner ☰ menu). QSignalBlocker prevents recursive toggle.
        connect(m_appletVis, &AppletVisibilityController::visibilityChanged,
                this, [this](const QString& id, bool visible) {
            if (auto* act = m_topMenuAppletActions.value(id, nullptr)) {
                QSignalBlocker block(act);
                act->setChecked(visible);
            }
        });
    }
```

(Note: the existing separator at line 2764 — `containersMenu->addSeparator();` — stays in place. It precedes our new `addSection` call.)

- [ ] **Step 3: Build and smoke-test**

```bash
cmake --build build -j
./build/NereusSDR &
sleep 3
```

Manually verify (or have an agent screenshot): open Containers menu — see "Applets" section header followed by 5 checkable items, all checked. Toggle TX — TX applet hides. Re-toggle — TX returns to its original spot.

```bash
pkill -f NereusSDR
```

- [ ] **Step 4: Commit**

```bash
git add src/gui/MainWindow.h src/gui/MainWindow.cpp
git commit -m "$(cat <<'EOF'
feat(menu): restore Containers > Applets show/hide section

Replaces the dead commented-out addContainerToggle block from
25597df with a controller-driven Applets section. Ships with
the 5 currently-wired applets (Rx / Tx / PhoneCw / Vax /
PureSignal); future applets register and appear automatically
as their phases ship.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Banner ☰ menu on AppletPanelWidget

Build the same 5 toggles as a separate `QMenu`, install it on the panel's ☰ button, and wire two-way sync.

**Files:**
- Modify: `src/gui/MainWindow.h` (add `m_bannerAppletActions` + `m_bannerAppletsMenu`)
- Modify: `src/gui/MainWindow.cpp`

- [ ] **Step 1: Add member declarations**

In `src/gui/MainWindow.h`, add:

```cpp
    QMenu* m_bannerAppletsMenu{nullptr};
    QHash<QString, QAction*> m_bannerAppletActions;
```

Add `class QMenu;` forward decl if not already present.

- [ ] **Step 2: Build the banner menu in `MainWindow.cpp`**

Open `src/gui/MainWindow.cpp`. Find the controller-construction block from Task 4. Insert AFTER the `connect(m_appletVis, ...visibilityChanged ...)` lambda from Task 4 (the panel-pump connection), so the banner setup runs before the menu-bar build later in the file:

```cpp
    // ── Banner ☰ menu on AppletPanelWidget ──────────────────────────────
    if (m_appletVis && m_appletPanel) {
        m_bannerAppletsMenu = new QMenu(this);

        for (const QString& id : m_appletVis->registeredIds()) {
            QAction* act = m_bannerAppletsMenu->addAction(
                m_appletVis->displayName(id));
            act->setCheckable(true);
            act->setChecked(m_appletVis->isVisible(id));
            // User-visible tooltip — plain English.
            act->setToolTip(QStringLiteral("Show or hide the %1 applet")
                            .arg(m_appletVis->displayName(id)));

            connect(act, &QAction::toggled, this, [this, id](bool checked) {
                if (m_appletVis) { m_appletVis->setVisible(id, checked); }
            });
            m_bannerAppletActions.insert(id, act);
        }

        // Sync banner checkmarks when state changes elsewhere (top menu).
        connect(m_appletVis, &AppletVisibilityController::visibilityChanged,
                this, [this](const QString& id, bool visible) {
            if (auto* act = m_bannerAppletActions.value(id, nullptr)) {
                QSignalBlocker block(act);
                act->setChecked(visible);
            }
        });

        m_appletPanel->setBannerMenu(m_bannerAppletsMenu);
    }
```

- [ ] **Step 3: Build and smoke-test**

```bash
cmake --build build -j
./build/NereusSDR &
sleep 3
```

Manually verify (or screenshot): the right-side panel now has a ☰ button at the top-right of a thin dark bar above the meter widget. Click it — same 5 toggles appear, all checked. Toggle TX via the ☰ menu — TX hides AND the Containers > Applets > TX checkmark also clears.

```bash
pkill -f NereusSDR
```

- [ ] **Step 4: Commit**

```bash
git add src/gui/MainWindow.h src/gui/MainWindow.cpp
git commit -m "$(cat <<'EOF'
feat(applet-panel): hamburger menu on banner mirrors Containers menu

Builds a second QMenu of the same 5 applet toggles, installs it
on AppletPanelWidget's ☰ button, and wires it to the same
controller as the top-menu section. QSignalBlocker on each side
prevents toggle echo loops.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: End-to-end wiring test

A focused integration test that proves both menu surfaces share state via the controller.

**Files:**
- Create: `tests/tst_applet_visibility_menu_wiring.cpp`
- Modify: `tests/CMakeLists.txt`

This test does NOT spin up a full MainWindow (too heavy and fragile in CI). Instead it constructs a controller directly, builds the same two QMenu structures the production code builds, and verifies they stay in sync via the controller's signal. This is a faithful proxy for the production wiring without the rest of the app.

- [ ] **Step 1: Create the test**

Create `tests/tst_applet_visibility_menu_wiring.cpp`:

```cpp
// Verify the two QMenu structures (top menu + banner menu) wired in
// MainWindow stay in sync via AppletVisibilityController. This test
// builds the menu structures the same way MainWindow does, without
// instantiating MainWindow itself.
// no-port-check: NereusSDR-original — no Thetis source.

#include <QtTest/QtTest>
#include <QApplication>
#include <QMenu>
#include <QAction>
#include <QHash>
#include "gui/applets/AppletVisibilityController.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TstAppletVisibilityMenuWiring : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    void both_menus_have_5_actions_initially_checked();
    void toggling_top_menu_action_updates_banner_menu();
    void toggling_banner_action_updates_top_menu();
    void no_recursive_emit_loop();
};

namespace {
struct Wired {
    AppletVisibilityController* ctl;
    QMenu* topMenu;
    QMenu* bannerMenu;
    QHash<QString, QAction*> topActions;
    QHash<QString, QAction*> bannerActions;
};

// Mirror of the MainWindow setup, in test scope.
Wired buildMenus(QObject* parent)
{
    Wired w;
    w.ctl = new AppletVisibilityController(parent);
    w.ctl->registerApplet(QStringLiteral("Rx"),
                          QStringLiteral("RX"), true);
    w.ctl->registerApplet(QStringLiteral("Tx"),
                          QStringLiteral("TX"), true);
    w.ctl->registerApplet(QStringLiteral("PhoneCw"),
                          QStringLiteral("Phone / CW"), true);
    w.ctl->registerApplet(QStringLiteral("Vax"),
                          QStringLiteral("VAX"), true);
    w.ctl->registerApplet(QStringLiteral("PureSignal"),
                          QStringLiteral("PureSignal"), true);

    w.topMenu = new QMenu(qobject_cast<QWidget*>(parent));
    w.bannerMenu = new QMenu(qobject_cast<QWidget*>(parent));

    auto wireMenu = [&w](QMenu* menu, QHash<QString, QAction*>& sink) {
        for (const QString& id : w.ctl->registeredIds()) {
            QAction* act = menu->addAction(w.ctl->displayName(id));
            act->setCheckable(true);
            act->setChecked(w.ctl->isVisible(id));
            QObject::connect(act, &QAction::toggled,
                             [w, id](bool checked) {
                w.ctl->setVisible(id, checked);
            });
            sink.insert(id, act);
        }
    };
    wireMenu(w.topMenu,    w.topActions);
    wireMenu(w.bannerMenu, w.bannerActions);

    auto syncMenu = [&w](QHash<QString, QAction*>& sink) {
        QObject::connect(w.ctl, &AppletVisibilityController::visibilityChanged,
                         [sink](const QString& id, bool v) {
            if (auto* act = sink.value(id, nullptr)) {
                QSignalBlocker block(act);
                act->setChecked(v);
            }
        });
    };
    syncMenu(w.topActions);
    syncMenu(w.bannerActions);

    return w;
}
} // anon

void TstAppletVisibilityMenuWiring::both_menus_have_5_actions_initially_checked()
{
    QObject parent;
    Wired w = buildMenus(&parent);
    QCOMPARE(w.topMenu->actions().size(),    5);
    QCOMPARE(w.bannerMenu->actions().size(), 5);
    for (QAction* a : w.topMenu->actions())    { QVERIFY(a->isChecked()); }
    for (QAction* a : w.bannerMenu->actions()) { QVERIFY(a->isChecked()); }
}

void TstAppletVisibilityMenuWiring::toggling_top_menu_action_updates_banner_menu()
{
    QObject parent;
    Wired w = buildMenus(&parent);
    QAction* topTx = w.topActions.value(QStringLiteral("Tx"));
    QAction* banTx = w.bannerActions.value(QStringLiteral("Tx"));

    topTx->toggle();   // checked -> unchecked
    QVERIFY(!topTx->isChecked());
    QVERIFY(!banTx->isChecked());     // mirrored
    QVERIFY(!w.ctl->isVisible(QStringLiteral("Tx")));
}

void TstAppletVisibilityMenuWiring::toggling_banner_action_updates_top_menu()
{
    QObject parent;
    Wired w = buildMenus(&parent);
    QAction* topPs = w.topActions.value(QStringLiteral("PureSignal"));
    QAction* banPs = w.bannerActions.value(QStringLiteral("PureSignal"));

    banPs->toggle();
    QVERIFY(!banPs->isChecked());
    QVERIFY(!topPs->isChecked());
    QVERIFY(!w.ctl->isVisible(QStringLiteral("PureSignal")));
}

void TstAppletVisibilityMenuWiring::no_recursive_emit_loop()
{
    QObject parent;
    Wired w = buildMenus(&parent);
    QSignalSpy spy(w.ctl, &AppletVisibilityController::visibilityChanged);

    w.topActions.value(QStringLiteral("Vax"))->toggle();
    QCOMPARE(spy.count(), 1);     // exactly one emission, no echo loop

    w.bannerActions.value(QStringLiteral("Vax"))->toggle();
    QCOMPARE(spy.count(), 2);
}

QTEST_MAIN(TstAppletVisibilityMenuWiring)
#include "tst_applet_visibility_menu_wiring.moc"
```

- [ ] **Step 2: Register the test in CMake**

In `tests/CMakeLists.txt`, near the other applet tests:

```cmake
# -- Two-way sync between top-menu and banner-menu applet toggles --
nereus_add_test(tst_applet_visibility_menu_wiring)
```

- [ ] **Step 3: Build and run — verify pass**

```bash
cmake --build build -j --target tst_applet_visibility_menu_wiring
ctest --test-dir build -R tst_applet_visibility_menu_wiring -V
```

Expected: PASS, all 4 cases green.

- [ ] **Step 4: Run the full applet test family one more time**

```bash
ctest --test-dir build -R "applet" -V
```

Expected: every test matching `applet` is green (the new ones from this plan plus the existing `tst_applet_panel_gutter`, `tst_applet_ps_wiring`).

- [ ] **Step 5: Commit**

```bash
git add tests/tst_applet_visibility_menu_wiring.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
test(applets): two-way menu sync for visibility controller

Faithful proxy of the MainWindow wiring: top menu and banner
menu both bind to the same controller, toggling either updates
the other's checked state, and no recursive emit loops occur.
Tests the wiring shape without instantiating MainWindow.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 8: Bench-verify and tidy

Manual verification at the bench, plus a final pass to make sure nothing was missed.

- [ ] **Step 1: Build and launch the app**

```bash
cmake --build build -j
./build/NereusSDR &
```

- [ ] **Step 2: Walk the manual verification matrix from spec §8.4**

Verify each item:

1. App launches — all 5 applets visible (RX, TX, Phone/CW, VAX, PureSignal top-to-bottom).
2. Containers > Applets section present, all 5 items checked.
3. ☰ button visible top-right of the panel; click opens menu with same 5 items, all checked.
4. Toggle TX from top menu → TX hides in panel; ☰ menu's TX is now unchecked.
5. Toggle TX back from ☰ menu → TX returns to its original spot (not bottom); top-menu TX is now checked.
6. Hide all 5 → panel collapses to header (banner row + meter widget) only.
7. Re-show all → original order: RX, TX, Phone/CW, VAX, PureSignal.
8. Quit, relaunch → hidden states from before the quit are restored.
9. Open `~/.config/NereusSDR/NereusSDR.settings`, verify `AppletXxxVisible` keys are present and match.

- [ ] **Step 3: Kill the app**

```bash
pkill -f NereusSDR
```

- [ ] **Step 4: Run the full test suite once**

```bash
ctest --test-dir build --output-on-failure
```

Expected: all tests green. Investigate any unrelated failures (might be pre-existing flakes — check `git diff` to confirm none of this plan's edits touched the failing area).

- [ ] **Step 5: Final commit if any tidy was needed**

If verification surfaced any small fix (e.g. a missing tooltip, a stylesheet typo), fix and commit:

```bash
git add <fixed files>
git commit -m "fix(applet-vis): <one-line description>

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

If no tidy needed, skip this step.

---

## Done

The feature is complete when:

- [ ] All 4 test executables green (`tst_applet_panel_set_visible`, `tst_applet_visibility_controller`, `tst_applet_panel_gutter` (extended), `tst_applet_visibility_menu_wiring`)
- [ ] Manual verification matrix passes
- [ ] Full ctest suite green
- [ ] All commits GPG-signed
- [ ] Pre-commit hooks pass on every commit (Thetis header check, port attribution, inline cite check, compliance inventory, tag preservation)
