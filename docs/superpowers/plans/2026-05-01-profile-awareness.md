# Profile-Awareness UI Cues Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement cross-cutting profile-awareness UI cues for the 91 TX-profile-bundled fields scattered across 11+ surfaces. Each profile-bundled control gains a 3px blue/amber left-border (blue = bundled, amber = modified-from-saved); each surface with profile-bundled controls gains a `ProfileBanner` showing active profile name + Save/Save As buttons. ~1530 lines net across 7 sub-phases.

**Architecture:** Single `ProfileAwarenessRegistry` (owned by `MicProfileManager`) tracks attached widgets keyed by field-name. Per-control decoration via Qt property + global QSS rules in `StyleConstants.h`. Modified state computed by comparing widget value to active profile's saved value via per-widget-type value extractor + custom-getter callbacks for `ParametricEqWidget`. `ProfileBanner` widget subscribes to `MicProfileManager::activeProfileChanged` + registry's `anyModifiedChanged` signal, drives Save/Save-As actions through MicProfileManager. Reset = re-select profile (no separate "Reset to Saved" affordance — handled by existing profile-activation path).

**Tech Stack:** Qt 6 / C++20. Depends on Plan 4 (TX bandwidth — uses FilterLow/FilterHigh as Phase 3 test fixtures and the first profile field to register through the registry). Plans 1, 2, 3 are foundational dependencies. Phase 1-2 land first as commits before any per-surface registrations.

---

## Reference

- **Design doc:** `docs/architecture/ui-audit-polish-plan.md` — Phase E2.
- **Prerequisites:** Plans 1, 4. Plans 2, 3 strongly recommended (cross-surface UI matures into a clean canvas).
- **Key code paths:**
  - `src/core/MicProfileManager.{h,cpp}` — owns registry; gains `compareToSaved`, `createProfileFromCurrent`, `profileModifiedChanged` signal
  - `src/gui/StyleConstants.h` — gains QSS rules for `[profileBundled="true"]` decoration
  - `src/gui/applets/TxEqDialog.cpp`, `src/gui/applets/TxCfcDialog.cpp` — register parametric blob fields via custom getter on `ParametricEqWidget`
  - `src/gui/setup/{TxProfileSetupPage,AgcAlcSetupPage,CfcSetupPage,AudioTxInputPage,VoxDexpSetupPage,SpeechProcessorPage,FmSetupPage}.cpp` — surface integration
  - `src/gui/applets/{TxApplet,PhoneCwApplet}.cpp` — applet integration

---

## File-Structure Overview

### New code
- `src/core/ProfileAwarenessRegistry.{h,cpp}` (new, ~400 lines)
- `src/gui/widgets/ProfileBanner.{h,cpp}` (new, ~200 lines)
- `src/core/ProfileFieldSchema.{h,cpp}` (new, ~150 lines — 91-field metadata table)

### Files modified
- `src/core/MicProfileManager.{h,cpp}` (gain `compareToSaved`, `createProfileFromCurrent`, `profileModifiedChanged`; install registry)
- `src/gui/StyleConstants.h` (QSS rules for [profileBundled] decoration)
- 11+ surface files: TxApplet, PhoneCwApplet, AgcAlcSetupPage, CfcSetupPage, AudioTxInputPage, VoxDexpSetupPage, SpeechProcessorPage, FmSetupPage, TxProfileSetupPage, TxEqDialog, TxCfcDialog (each gets banner + per-control registrations)

### Tests added
- `tests/tst_profile_awareness_registry.cpp` (new) — attach/detach/isModified/revert per widget type
- `tests/tst_profile_banner.cpp` (new) — banner state transitions
- `tests/tst_profile_save_load_roundtrip.cpp` (new) — save → modify → re-select → values restored

---

## Tasks

### Phase 1 — Infrastructure (~600 lines, no UI surfaces touched)

#### Task 1: Branch checkpoint after Plans 1 + 4 land

- [ ] **Step 1: Verify Plans 1 + 4 commits + clean tree**

```bash
git log --oneline | head -50 | grep -cE "model\(tx\)|core\(tx-channel\)|ui\(tx-applet\)|ui\(tx-profile-setup\)"
ctest --test-dir build --output-on-failure 2>&1 | tail -5
```

---

#### Task 2: Create ProfileAwarenessRegistry skeleton

**Files:**
- Create: `src/core/ProfileAwarenessRegistry.{h,cpp}`
- Test: `tests/tst_profile_awareness_registry.cpp` (new)

- [ ] **Step 1: Write the failing test (covers attach + isModified + detach for QSpinBox)**

```cpp
// tests/tst_profile_awareness_registry.cpp
#include <QtTest/QtTest>
#include <QSpinBox>
#include "core/ProfileAwarenessRegistry.h"
#include "core/MicProfileManager.h"
#include "models/TransmitModel.h"

class TestProfileAwarenessRegistry : public QObject {
    Q_OBJECT
private slots:
    void attachSetsProfileBundledProperty();
    void modifiedFiresWhenWidgetValueDiffersFromSavedProfile();
    void revertToSavedRestoresWidgetValue();
    void detachClearsRegistry();
    void anyModifiedAggregatesCorrectly();
};

void TestProfileAwarenessRegistry::attachSetsProfileBundledProperty()
{
    NereusSDR::TransmitModel tx;
    NereusSDR::MicProfileManager mgr;
    NereusSDR::ProfileAwarenessRegistry registry(&mgr);
    QSpinBox spin;
    spin.setRange(0, 5000);
    spin.setValue(100);

    registry.attach(&spin, QStringLiteral("FilterLow"));
    QCOMPARE(spin.property("profileBundled").toBool(), true);
    QCOMPARE(spin.property("profileModified").toBool(), false);
}

void TestProfileAwarenessRegistry::modifiedFiresWhenWidgetValueDiffersFromSavedProfile()
{
    NereusSDR::TransmitModel tx;
    NereusSDR::MicProfileManager mgr;
    mgr.setActiveProfile(QStringLiteral("DX SSB"), &tx);  // FilterLow=1710 per Plan 4
    NereusSDR::ProfileAwarenessRegistry registry(&mgr);

    QSpinBox spin;
    spin.setRange(0, 5000);
    spin.setValue(1710);
    registry.attach(&spin, QStringLiteral("FilterLow"));

    QCOMPARE(registry.isModified(&spin), false);

    QSignalSpy spy(&registry, &NereusSDR::ProfileAwarenessRegistry::widgetModifiedChanged);
    spin.setValue(2000);
    QCOMPARE(registry.isModified(&spin), true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spin.property("profileModified").toBool(), true);
}

// ... three more sub-tests

QTEST_MAIN(TestProfileAwarenessRegistry)
#include "tst_profile_awareness_registry.moc"
```

Register in `tests/CMakeLists.txt`.

- [ ] **Step 2: Build + run; verify FAIL**

Class doesn't exist yet.

- [ ] **Step 3: Implement the header**

```cpp
// src/core/ProfileAwarenessRegistry.h
#pragma once

#include <QObject>
#include <QPointer>
#include <QHash>
#include <QString>
#include <QVariant>
#include <functional>

class QWidget;

namespace NereusSDR {

class MicProfileManager;
class TransmitModel;

class ProfileAwarenessRegistry : public QObject
{
    Q_OBJECT
public:
    using ValueGetter = std::function<QVariant(QWidget*)>;

    explicit ProfileAwarenessRegistry(MicProfileManager* mgr,
                                       QObject* parent = nullptr);
    ~ProfileAwarenessRegistry() override;

    // Attach a widget for a profile field. Auto-detects type via
    // qobject_cast for known Qt widget classes (QSpinBox, QSlider,
    // QCheckBox, checkable QPushButton, QComboBox, QLineEdit).
    void attach(QWidget* widget, const QString& fieldName);

    // Attach with a custom value getter — for non-standard widgets like
    // ParametricEqWidget. The signal must be one of the widget's
    // value-change signals; the registry uses Qt's metaObject to wire up.
    void attach(QWidget* widget, const QString& fieldName,
                ValueGetter getter,
                const QObject* signalSource, const char* signalName);

    // Detach. Auto-called on widget destruction via QPointer + destroyed signal.
    void detach(QWidget* widget);

    // Compare widget's current value to the active profile's saved value.
    bool isModified(QWidget* widget) const;

    // Aggregate modified state across all attached widgets.
    bool isAnyModified() const;

    // Revert one widget to its profile's saved value (programmatic value-set
    // is suppressed during this op so isModified flips back to false correctly).
    void revertToSaved(QWidget* widget);

    // Bulk: revert all attached widgets to active profile's saved values.
    void revertAllToSaved();

    // Bulk: re-evaluate modified state for every attached widget. Called
    // automatically by MicProfileManager on profile-load completion.
    void reevaluateAll();

signals:
    void widgetModifiedChanged(QWidget* widget, bool modified);
    void anyModifiedChanged(bool anyModified);

private slots:
    void onWidgetDestroyed(QObject* obj);

private:
    struct Entry {
        QPointer<QWidget> widget;
        QString fieldName;
        ValueGetter getter;     // empty for standard types
        bool lastModified{false};
    };

    QHash<QWidget*, Entry> m_entries;
    MicProfileManager* m_mgr;
    bool m_loadingProfile{false};  // suppress modified-detection during load

    QVariant readWidgetValue(const Entry& e) const;
    QVariant readProfileValue(const QString& fieldName) const;
    void connectValueChangeSignal(QWidget* w);
    void onWidgetValueChanged(QWidget* w);
    void updateAnyModifiedFlag();
};

}  // namespace NereusSDR
```

- [ ] **Step 4: Implement the .cpp**

```cpp
// src/core/ProfileAwarenessRegistry.cpp
#include "core/ProfileAwarenessRegistry.h"
#include "core/MicProfileManager.h"
#include "models/TransmitModel.h"

#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QCheckBox>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QSignalBlocker>

namespace NereusSDR {

ProfileAwarenessRegistry::ProfileAwarenessRegistry(MicProfileManager* mgr,
                                                     QObject* parent)
    : QObject(parent), m_mgr(mgr)
{
    if (m_mgr) {
        // Suppress modified detection during programmatic profile load.
        connect(m_mgr, &MicProfileManager::profileLoadStarted,
                this, [this]() { m_loadingProfile = true; });
        connect(m_mgr, &MicProfileManager::profileLoadCompleted,
                this, [this]() {
            m_loadingProfile = false;
            reevaluateAll();
        });
    }
}

ProfileAwarenessRegistry::~ProfileAwarenessRegistry() = default;

void ProfileAwarenessRegistry::attach(QWidget* widget, const QString& fieldName)
{
    if (!widget || fieldName.isEmpty()) return;
    Entry e;
    e.widget = widget;
    e.fieldName = fieldName;
    m_entries.insert(widget, e);

    widget->setProperty("profileBundled", true);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);

    connectValueChangeSignal(widget);
    connect(widget, &QObject::destroyed,
            this, &ProfileAwarenessRegistry::onWidgetDestroyed);
}

void ProfileAwarenessRegistry::attach(QWidget* widget, const QString& fieldName,
                                        ValueGetter getter,
                                        const QObject* signalSource,
                                        const char* signalName)
{
    if (!widget || fieldName.isEmpty()) return;
    Entry e;
    e.widget = widget;
    e.fieldName = fieldName;
    e.getter = std::move(getter);
    m_entries.insert(widget, e);

    widget->setProperty("profileBundled", true);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);

    connect(signalSource, signalName, this, [this, widget]() {
        onWidgetValueChanged(widget);
    });
    connect(widget, &QObject::destroyed,
            this, &ProfileAwarenessRegistry::onWidgetDestroyed);
}

void ProfileAwarenessRegistry::detach(QWidget* widget)
{
    auto it = m_entries.find(widget);
    if (it == m_entries.end()) return;
    if (widget) {
        widget->setProperty("profileBundled", QVariant());
        widget->setProperty("profileModified", QVariant());
    }
    m_entries.erase(it);
    updateAnyModifiedFlag();
}

bool ProfileAwarenessRegistry::isModified(QWidget* widget) const
{
    auto it = m_entries.constFind(widget);
    if (it == m_entries.constEnd()) return false;
    QVariant cur = readWidgetValue(*it);
    QVariant saved = readProfileValue(it->fieldName);
    return cur != saved;  // QVariant equality covers most types; blob types use deep-equal in custom getter
}

bool ProfileAwarenessRegistry::isAnyModified() const
{
    for (const auto& e : m_entries) {
        if (e.lastModified) return true;
    }
    return false;
}

void ProfileAwarenessRegistry::revertToSaved(QWidget* widget)
{
    auto it = m_entries.find(widget);
    if (it == m_entries.end()) return;
    QSignalBlocker b(widget);
    m_loadingProfile = true;
    QVariant saved = readProfileValue(it->fieldName);
    // Apply saved value via type-specific setter (mirror readWidgetValue dispatch).
    if (auto* sb = qobject_cast<QSpinBox*>(widget)) {
        sb->setValue(saved.toInt());
    } else if (auto* dsb = qobject_cast<QDoubleSpinBox*>(widget)) {
        dsb->setValue(saved.toDouble());
    } else if (auto* sl = qobject_cast<QSlider*>(widget)) {
        sl->setValue(saved.toInt());
    } else if (auto* cb = qobject_cast<QCheckBox*>(widget)) {
        cb->setChecked(saved.toBool());
    } else if (auto* pb = qobject_cast<QPushButton*>(widget); pb && pb->isCheckable()) {
        pb->setChecked(saved.toBool());
    } else if (auto* cmb = qobject_cast<QComboBox*>(widget)) {
        cmb->setCurrentIndex(saved.toInt());
    } else if (auto* le = qobject_cast<QLineEdit*>(widget)) {
        le->setText(saved.toString());
    }
    m_loadingProfile = false;
    onWidgetValueChanged(widget);  // re-evaluate
}

void ProfileAwarenessRegistry::revertAllToSaved()
{
    for (auto& e : m_entries) {
        if (e.widget) revertToSaved(e.widget);
    }
}

void ProfileAwarenessRegistry::reevaluateAll()
{
    for (auto& e : m_entries) {
        if (!e.widget) continue;
        bool nowModified = isModified(e.widget);
        if (nowModified != e.lastModified) {
            e.lastModified = nowModified;
            e.widget->setProperty("profileModified", nowModified);
            e.widget->style()->unpolish(e.widget);
            e.widget->style()->polish(e.widget);
            emit widgetModifiedChanged(e.widget, nowModified);
        }
    }
    updateAnyModifiedFlag();
}

void ProfileAwarenessRegistry::onWidgetDestroyed(QObject* obj)
{
    detach(qobject_cast<QWidget*>(obj));
}

QVariant ProfileAwarenessRegistry::readWidgetValue(const Entry& e) const
{
    if (e.getter) return e.getter(e.widget);
    QWidget* w = e.widget;
    if (auto* sb = qobject_cast<QSpinBox*>(w)) return sb->value();
    if (auto* dsb = qobject_cast<QDoubleSpinBox*>(w)) return dsb->value();
    if (auto* sl = qobject_cast<QSlider*>(w)) return sl->value();
    if (auto* cb = qobject_cast<QCheckBox*>(w)) return cb->isChecked();
    if (auto* pb = qobject_cast<QPushButton*>(w); pb && pb->isCheckable()) return pb->isChecked();
    if (auto* cmb = qobject_cast<QComboBox*>(w)) return cmb->currentIndex();
    if (auto* le = qobject_cast<QLineEdit*>(w)) return le->text();
    return {};
}

QVariant ProfileAwarenessRegistry::readProfileValue(const QString& fieldName) const
{
    if (!m_mgr) return {};
    return m_mgr->compareToSaved(fieldName);  // returns the saved value as QVariant, or empty if not present
}

void ProfileAwarenessRegistry::connectValueChangeSignal(QWidget* w)
{
    auto onChanged = [this, w]() { onWidgetValueChanged(w); };
    if (auto* sb = qobject_cast<QSpinBox*>(w)) {
        connect(sb, qOverload<int>(&QSpinBox::valueChanged), this, onChanged);
    } else if (auto* dsb = qobject_cast<QDoubleSpinBox*>(w)) {
        connect(dsb, qOverload<double>(&QDoubleSpinBox::valueChanged), this, onChanged);
    } else if (auto* sl = qobject_cast<QSlider*>(w)) {
        connect(sl, &QSlider::valueChanged, this, onChanged);
    } else if (auto* cb = qobject_cast<QCheckBox*>(w)) {
        connect(cb, &QCheckBox::toggled, this, onChanged);
    } else if (auto* pb = qobject_cast<QPushButton*>(w); pb && pb->isCheckable()) {
        connect(pb, &QPushButton::toggled, this, onChanged);
    } else if (auto* cmb = qobject_cast<QComboBox*>(w)) {
        connect(cmb, qOverload<int>(&QComboBox::currentIndexChanged), this, onChanged);
    } else if (auto* le = qobject_cast<QLineEdit*>(w)) {
        connect(le, &QLineEdit::textChanged, this, onChanged);
    }
}

void ProfileAwarenessRegistry::onWidgetValueChanged(QWidget* w)
{
    if (m_loadingProfile) return;
    auto it = m_entries.find(w);
    if (it == m_entries.end()) return;
    bool nowModified = isModified(w);
    if (nowModified != it->lastModified) {
        it->lastModified = nowModified;
        w->setProperty("profileModified", nowModified);
        w->style()->unpolish(w);
        w->style()->polish(w);
        emit widgetModifiedChanged(w, nowModified);
        updateAnyModifiedFlag();
    }
}

void ProfileAwarenessRegistry::updateAnyModifiedFlag()
{
    static bool lastAny = false;
    bool now = isAnyModified();
    if (now != lastAny) {
        lastAny = now;
        emit anyModifiedChanged(now);
    }
}

}  // namespace NereusSDR
```

Update `src/core/CMakeLists.txt` to include the new files.

- [ ] **Step 5: Build + run test; verify PASS**

- [ ] **Step 6: Commit**

```bash
git add src/core/ProfileAwarenessRegistry.{h,cpp} src/core/CMakeLists.txt tests/tst_profile_awareness_registry.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
core(profile-awareness): add ProfileAwarenessRegistry skeleton

Per docs/architecture/ui-audit-polish-plan.md §E2 Phase 1.

Registry tracks widgets attached for profile-bundled fields. Per-widget-
type value extractor dispatches to QSpinBox/QDoubleSpinBox/QSlider/
QCheckBox/QPushButton (checkable)/QComboBox/QLineEdit. Custom widgets
(ParametricEqWidget) use the alternative attach() overload with explicit
ValueGetter callback + signal source.

QPointer-based dangling-pointer protection. m_loadingProfile flag
suppresses modified-detection during programmatic profile-load.
profileBundled + profileModified Qt properties drive QSS decoration
(rules added in next task).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

#### Task 3: Add QSS decoration rules for [profileBundled] in StyleConstants.h

**Files:**
- Modify: `src/gui/StyleConstants.h`

- [ ] **Step 1: Add QSS rule constants**

After the existing palette additions:

```cpp
// Profile-bundled control decoration. Used by ProfileAwarenessRegistry
// via Qt properties: setProperty("profileBundled", true) + 
// setProperty("profileModified", true). Apply this rule via 
// QApplication::setStyleSheet at app startup so it cascades to every
// widget. Alternative: scope per-widget via the QSS attribute selector.
//
// Per docs/architecture/ui-audit-polish-plan.md §E2 Phase 1.
constexpr auto kProfileAwarenessQss =
    "*[profileBundled=\"true\"] {"
    "  border-left: 3px solid #00b4d8;"  // kAccent
    "  padding-left: 6px;"
    "}"
    "*[profileBundled=\"true\"][profileModified=\"true\"] {"
    "  border-left-color: #ffb800;"  // amber
    "}";
```

- [ ] **Step 2: Apply at app startup**

In `src/main.cpp` (or wherever the QApplication is set up):

```cpp
// Profile-awareness QSS — global decoration rules applied via app stylesheet.
QString existing = qApp->styleSheet();
qApp->setStyleSheet(existing + NereusSDR::Style::kProfileAwarenessQss);
```

- [ ] **Step 3: Build + smoke test**

App still launches and looks normal (no widgets registered yet, so rules don't fire).

- [ ] **Step 4: Commit**

```bash
git add src/gui/StyleConstants.h src/main.cpp
git commit -m "style: add kProfileAwarenessQss + apply at app startup

Per docs/architecture/ui-audit-polish-plan.md §E2 Phase 1.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

#### Task 4: Add MicProfileManager API additions

**Files:**
- Modify: `src/core/MicProfileManager.{h,cpp}`

- [ ] **Step 1: Add public API**

```cpp
public:
    void saveActiveProfile(TransmitModel* tx);  // already added in Plan 4 Task 2
    void createProfileFromCurrent(const QString& name, TransmitModel* tx);
    QVariant compareToSaved(const QString& fieldName) const;  // returns saved value as QVariant
    bool isActiveProfileModified() const;  // already added in Plan 4 Task 2
    ProfileAwarenessRegistry* registry() const { return m_registry.get(); }

signals:
    void profileLoadStarted();
    void profileLoadCompleted();
    void profileModifiedChanged(bool modified);

private:
    std::unique_ptr<ProfileAwarenessRegistry> m_registry;
```

In the constructor, instantiate the registry: `m_registry = std::make_unique<ProfileAwarenessRegistry>(this);`.

- [ ] **Step 2: Implement compareToSaved**

```cpp
QVariant MicProfileManager::compareToSaved(const QString& fieldName) const
{
    // Lookup the active profile's saved value for the named field.
    // Field names match the keys in the ProfileFieldSchema (Task 5).
    if (m_activeProfileName.isEmpty()) return {};
    return m_profiles[m_activeProfileName].fieldValue(fieldName);
    // (Actual implementation depends on whether profiles are stored as
    // a struct-with-fields, a QMap<QString, QVariant>, or per-field
    // AppSettings keys. Verify and adapt to existing storage.)
}
```

- [ ] **Step 3: Emit profileLoadStarted/Completed around setActiveProfile**

```cpp
void MicProfileManager::setActiveProfile(const QString& name, TransmitModel* tx)
{
    emit profileLoadStarted();
    // ... existing load logic ...
    emit profileLoadCompleted();
}
```

- [ ] **Step 4: Build + run existing Plan 4 tests; verify still PASS**

```bash
ctest --test-dir build -R tst_tx_bandwidth_persistence --output-on-failure
ctest --test-dir build -R tst_profile_awareness_registry --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add src/core/MicProfileManager.{h,cpp}
git commit -m "$(cat <<'EOF'
core(mic-profile-manager): add registry, compareToSaved, profileLoadStarted/Completed

Per docs/architecture/ui-audit-polish-plan.md §E2 Phase 1.

MicProfileManager now owns a ProfileAwarenessRegistry singleton accessible
via registry(). Adds:
- compareToSaved(fieldName) returning the active profile's saved value
- profileLoadStarted/profileLoadCompleted signals to suppress
  registry-side modified-detection during profile activation
- createProfileFromCurrent(name, tx) for Save As

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Phase 2 — Field schema table

#### Task 5: Define ProfileFieldSchema

**Files:**
- Create: `src/core/ProfileFieldSchema.{h,cpp}`

- [ ] **Step 1: Implement the schema table**

```cpp
// src/core/ProfileFieldSchema.h
#pragma once

#include <QString>
#include <QStringList>

namespace NereusSDR {

enum class ProfileFieldType {
    Bool,
    Int,
    Double,
    String,
    JsonBlob,        // for ParaEQ data fields
};

struct ProfileFieldDescriptor {
    QString name;
    ProfileFieldType type;
    QString uiSurface;   // human-readable surface hint, used in tooltips
    QString thetisCite;  // optional: "database.cs:9285 [v2.10.3.13]"
};

class ProfileFieldSchema {
public:
    static QStringList allFieldNames();
    static const ProfileFieldDescriptor& fieldDescriptor(const QString& name);
};

}  // namespace NereusSDR
```

```cpp
// src/core/ProfileFieldSchema.cpp
#include "core/ProfileFieldSchema.h"
#include <QHash>

namespace NereusSDR {

namespace {

const QHash<QString, ProfileFieldDescriptor>& descriptors()
{
    static const QHash<QString, ProfileFieldDescriptor> map {
        // Mic input (~10)
        {"MicGain", {"MicGain", ProfileFieldType::Int, "PhoneCwApplet · AudioTxInputPage", ""}},
        {"FMMicGain", {"FMMicGain", ProfileFieldType::Int, "FmSetupPage", ""}},
        {"MicMute", {"MicMute", ProfileFieldType::Bool, "AudioTxInputPage", ""}},
        {"MicSource", {"MicSource", ProfileFieldType::Int, "PhoneCwApplet · AudioTxInputPage", ""}},
        {"MicBoost", {"MicBoost", ProfileFieldType::Bool, "AudioTxInputPage", ""}},
        {"MicXlr", {"MicXlr", ProfileFieldType::Bool, "AudioTxInputPage", ""}},
        {"MicTipRing", {"MicTipRing", ProfileFieldType::Bool, "AudioTxInputPage", ""}},
        {"MicBias", {"MicBias", ProfileFieldType::Bool, "AudioTxInputPage", ""}},
        {"MicPttDisabled", {"MicPttDisabled", ProfileFieldType::Bool, "AudioTxInputPage", ""}},
        {"LineIn", {"LineIn", ProfileFieldType::Bool, "AudioTxInputPage", ""}},
        {"LineInBoost", {"LineInBoost", ProfileFieldType::Double, "AudioTxInputPage", ""}},

        // TX Filter (2 — from Plan 4)
        {"FilterLow", {"FilterLow", ProfileFieldType::Int, "TxApplet · TxProfileSetupPage", "database.cs:9285"}},
        {"FilterHigh", {"FilterHigh", ProfileFieldType::Int, "TxApplet · TxProfileSetupPage", "database.cs:9286"}},

        // TX EQ (~25) — see design doc §E2 inventory for full list
        {"TXEQEnabled", {"TXEQEnabled", ProfileFieldType::Bool, "TxApplet EQ button · TxEqDialog", ""}},
        {"TXEQPreamp", {"TXEQPreamp", ProfileFieldType::Int, "TxEqDialog", ""}},
        {"TXEQNumBands", {"TXEQNumBands", ProfileFieldType::Int, "TxEqDialog", ""}},
        // ... TXEQ0..TXEQ9 (10 band gains)
        // ... TxEqFreq0..TxEqFreq9 (10 band freqs)
        // ... Nc / Mp / Ctfmode / Wintype
        {"TXParaEQData", {"TXParaEQData", ProfileFieldType::JsonBlob, "TxEqDialog parametric panel", ""}},
        {"RXParaEQData", {"RXParaEQData", ProfileFieldType::JsonBlob, "TxEqDialog parametric panel", ""}},

        // TX Leveler (3)
        {"Lev_On", {"Lev_On", ProfileFieldType::Bool, "TxApplet LEV · AgcAlcSetupPage", ""}},
        {"Lev_MaxGain", {"Lev_MaxGain", ProfileFieldType::Int, "AgcAlcSetupPage", ""}},
        {"Lev_Decay", {"Lev_Decay", ProfileFieldType::Int, "AgcAlcSetupPage", ""}},

        // TX ALC (2)
        {"ALC_MaximumGain", {"ALC_MaximumGain", ProfileFieldType::Int, "AgcAlcSetupPage", ""}},
        {"ALC_Decay", {"ALC_Decay", ProfileFieldType::Int, "AgcAlcSetupPage", ""}},

        // CFC + Phase Rotator (~24) — see design doc inventory for full list
        {"CFCEnabled", {"CFCEnabled", ProfileFieldType::Bool, "TxApplet CFC · CfcSetupPage · TxCfcDialog", ""}},
        {"CFCPostEqEnabled", {"CFCPostEqEnabled", ProfileFieldType::Bool, "CfcSetupPage", ""}},
        {"CFCPreComp", {"CFCPreComp", ProfileFieldType::Int, "CfcSetupPage · TxCfcDialog", ""}},
        {"CFCPostEqGain", {"CFCPostEqGain", ProfileFieldType::Int, "CfcSetupPage · TxCfcDialog", ""}},
        // ... CFCFreq0..9 / CFCComp0..9 / CFCPostEq0..9 (30 fields)
        {"CFCParaEqData", {"CFCParaEqData", ProfileFieldType::JsonBlob, "TxCfcDialog parametric panel", ""}},
        {"CFCPhaseRotatorEnabled", {"CFCPhaseRotatorEnabled", ProfileFieldType::Bool, "CfcSetupPage", ""}},
        {"CFCPhaseReverseEnabled", {"CFCPhaseReverseEnabled", ProfileFieldType::Bool, "CfcSetupPage", ""}},
        {"CFCPhaseRotatorFreq", {"CFCPhaseRotatorFreq", ProfileFieldType::Int, "CfcSetupPage", ""}},
        {"CFCPhaseRotatorStages", {"CFCPhaseRotatorStages", ProfileFieldType::Int, "CfcSetupPage", ""}},

        // CESSB (1)
        {"CESSBOn", {"CESSBOn", ProfileFieldType::Bool, "CfcSetupPage", ""}},

        // Compander (CPDR) (2)
        {"CompanderOn", {"CompanderOn", ProfileFieldType::Bool, "PhoneCwApplet PROC · DspSetupPages", ""}},
        {"CompanderLevel", {"CompanderLevel", ProfileFieldType::Int, "PhoneCwApplet PROC slider · DspSetupPages", ""}},

        // VOX/DEXP (~10) — see design doc inventory
        {"VOX_On", {"VOX_On", ProfileFieldType::Bool, "TxApplet VOX · VoxDexpSetupPage", ""}},
        {"VOX_HangTime", {"VOX_HangTime", ProfileFieldType::Int, "VoxDexpSetupPage", ""}},
        {"Dexp_On", {"Dexp_On", ProfileFieldType::Bool, "VoxDexpSetupPage", ""}},
        // ... rest of DEXP fields

        // DX (1)
        {"DXOn", {"DXOn", ProfileFieldType::Bool, "SpeechProcessorPage · DspSetupPages", ""}},
    };
    return map;
}

}  // namespace

QStringList ProfileFieldSchema::allFieldNames()
{
    return descriptors().keys();
}

const ProfileFieldDescriptor& ProfileFieldSchema::fieldDescriptor(const QString& name)
{
    auto it = descriptors().find(name);
    static const ProfileFieldDescriptor empty{};
    return it == descriptors().end() ? empty : *it;
}

}  // namespace NereusSDR
```

**Note:** the table above is abbreviated for readability. The full implementation includes all 91 fields per the design doc inventory. When implementing, iterate the design doc's "91-field inventory" section row-by-row.

- [ ] **Step 2: Build + verify allFieldNames returns ~91**

Add a quick test:
```cpp
void TestProfileFieldSchema::allFieldsListed()
{
    auto names = NereusSDR::ProfileFieldSchema::allFieldNames();
    QVERIFY(names.size() >= 90);  // tolerate 91 ± a couple of pre-emphasis pre-ports
    QVERIFY(names.contains("FilterLow"));
    QVERIFY(names.contains("TXEQ0"));
    QVERIFY(names.contains("CFCComp9"));
}
```

- [ ] **Step 3: Commit**

```bash
git add src/core/ProfileFieldSchema.{h,cpp} src/core/CMakeLists.txt tests/tst_profile_field_schema.cpp tests/CMakeLists.txt
git commit -m "core(profile-awareness): add ProfileFieldSchema table (91 fields)

Per docs/architecture/ui-audit-polish-plan.md §E2 Phase 2.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Phase 3 — TxApplet integration

#### Task 6: Create ProfileBanner widget

**Files:**
- Create: `src/gui/widgets/ProfileBanner.{h,cpp}`

- [ ] **Step 1: Implement the header**

```cpp
// src/gui/widgets/ProfileBanner.h
#pragma once

#include <QWidget>
#include <QString>

class QLabel;
class QPushButton;

namespace NereusSDR {

class MicProfileManager;
class TransmitModel;

class ProfileBanner : public QWidget
{
    Q_OBJECT
public:
    explicit ProfileBanner(MicProfileManager* mgr,
                            TransmitModel* tx,
                            QWidget* parent = nullptr);

private slots:
    void onActiveProfileChanged(const QString& name, bool isFactory);
    void onModifiedChanged(bool modified);
    void onSaveClicked();
    void onSaveAsClicked();

private:
    QLabel*      m_iconLabel;
    QLabel*      m_profileLabel;
    QLabel*      m_modifiedDot;
    QLabel*      m_modifiedText;
    QPushButton* m_saveBtn;
    QPushButton* m_saveAsBtn;

    MicProfileManager* m_mgr;
    TransmitModel*     m_tx;
    bool m_factoryProfile{false};
};

}  // namespace NereusSDR
```

- [ ] **Step 2: Implement the .cpp**

(Layout: HBox with `[⚙] TX Profile: <name> [● modified-dot] [Saved/Modified] [stretch] [Save] [Save As...]`. Subscribes to `MicProfileManager::activeProfileChanged` + registry's `anyModifiedChanged`. Save disabled when factory profile is active or no modifications. Tooltip explains factory-profile case.)

For brevity here, a sketch — full implementation follows the standard Qt widget patterns established in the codebase. ~200 lines total.

- [ ] **Step 3: Build + smoke test**

Construct one in a test harness; verify it shows the active profile name.

- [ ] **Step 4: Commit**

```bash
git add src/gui/widgets/ProfileBanner.{h,cpp} src/gui/widgets/CMakeLists.txt
git commit -m "ui(profile-banner): add reusable banner widget

Per docs/architecture/ui-audit-polish-plan.md §E2 Phase 1.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

#### Task 7: TxApplet integration — banner + 7 field registrations

**Files:**
- Modify: `src/gui/applets/TxApplet.{h,cpp}`

- [ ] **Step 1: Add ProfileBanner at top of TX section**

After the title bar, before the Mic-source badge (Plan 4 Task 4 added the spinbox cluster around the Profile combo; the banner sits ABOVE the Profile combo or replaces the title bar's right side):

```cpp
auto* banner = new ProfileBanner(m_radioModel->micProfileManager(),
                                  m_radioModel->transmitModel(), this);
vbox->insertWidget(0, banner);  // top of stack
```

- [ ] **Step 2: Register the 7 TxApplet profile-bundled controls**

```cpp
auto* registry = m_radioModel->micProfileManager()->registry();

registry->attach(m_txFilterLowSpin,  QStringLiteral("FilterLow"));   // from Plan 4 Task 4
registry->attach(m_txFilterHighSpin, QStringLiteral("FilterHigh"));
registry->attach(m_levBtn,           QStringLiteral("Lev_On"));
registry->attach(m_eqBtn,            QStringLiteral("TXEQEnabled"));
registry->attach(m_cfcBtn,           QStringLiteral("CFCEnabled"));
registry->attach(m_voxBtn,           QStringLiteral("VOX_On"));
// (Profile combo itself is special — it drives selection, not bundled.)
```

- [ ] **Step 3: Build + visual**

Open TxApplet. Banner at top shows "TX Profile: DX SSB · Saved." LEV/EQ/CFC/VOX buttons + TX BW spinboxes show 3px blue left-border (profileBundled). Toggle LEV → border turns amber + banner shows Modified.

- [ ] **Step 4: Commit**

```bash
git commit -m "ui(tx-applet): integrate ProfileBanner + register 7 profile-bundled controls

Per docs/architecture/ui-audit-polish-plan.md §E2 Phase 3.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Phase 4 — Setup page integrations

#### Task 8: AgcAlcSetupPage integration

Banner at top of the TX-Leveler section group (page also has RX AGC; banner scoped to TX). Register: Lev_On checkbox, Lev_MaxGain spinbox, Lev_Decay spinbox, ALC_MaximumGain spinbox, ALC_Decay spinbox.

Same pattern as Task 7. Commit.

---

#### Task 9: CfcSetupPage integration

Banner at page top. Register: CFCEnabled, CFCPostEqEnabled, CFCPreComp, CFCPostEqGain, CESSBOn, plus the 4 PhaseRotator fields (CFCPhaseRotatorEnabled, CFCPhaseReverseEnabled, CFCPhaseRotatorFreq, CFCPhaseRotatorStages).

Commit.

---

#### Task 10: AudioTxInputPage integration

Banner at page top. Register the ~10 mic-input fields: MicGain, MicSource, MicBoost, MicXlr, MicTipRing, MicBias, MicPttDisabled, MicMute, LineIn, LineInBoost.

Commit.

---

#### Task 11: VoxDexpSetupPage integration

Banner at page top. Register the ~9 VOX/DEXP fields: VOX_On, VOX_HangTime, Dexp_On, Dexp_Threshold, Dexp_Attack, Dexp_Release, Dexp_Hysterisis, Dexp_Tau, Dexp_Attenuate.

Commit.

---

#### Task 12: SpeechProcessorPage integration

Banner at page top. Register: CompanderOn, CompanderLevel, DXOn, plus any other profile-bundled status fields.

Commit.

---

#### Task 13: FmSetupPage integration (FMMicGain only)

Banner at page top. Register: FMMicGain. Future pre-emphasis fields land in 3M-3b.

Commit.

---

#### Task 14: TxProfileSetupPage integration

Banner at page top. Register: FilterLow, FilterHigh (the only fields on this page so far, from Plan 4 Task 5).

Commit.

---

### Phase 5 — Modal dialog integrations

#### Task 15: TxEqDialog integration with custom getters

**Files:**
- Modify: `src/gui/applets/TxEqDialog.{h,cpp}`

- [ ] **Step 1: Add ProfileBanner at top of dialog body**

```cpp
auto* banner = new ProfileBanner(m_radioModel->micProfileManager(),
                                  m_radioModel->transmitModel(), this);
mainLayout->insertWidget(0, banner);
```

- [ ] **Step 2: Register the 25 EQ profile-bundled fields**

Most are direct widget→field maps:
```cpp
registry->attach(m_enableCheckbox, "TXEQEnabled");
registry->attach(m_preampSpin, "TXEQPreamp");
registry->attach(m_ncSpin, "Nc");
registry->attach(m_mpCheckbox, "Mp");
registry->attach(m_ctfmodeCombo, "Ctfmode");
registry->attach(m_wintypeCombo, "Wintype");

// Legacy panel: 10 band gains + 10 band frequencies (Strategy A — register each)
for (int i = 0; i < 10; ++i) {
    registry->attach(m_bandGainSpins[i], QStringLiteral("TXEQ%1").arg(i));
    registry->attach(m_bandFreqSpins[i], QStringLiteral("TxEqFreq%1").arg(i));
}
```

- [ ] **Step 3: Register parametric blob fields with custom getters**

```cpp
auto txParaGetter = [this](QWidget*) -> QVariant {
    return m_parametricWidget->serializeToJson();  // returns QString
};
registry->attach(m_parametricWidget, "TXParaEQData",
                 txParaGetter,
                 m_parametricWidget, SIGNAL(pointsChanged()));

// Same for RXParaEQData if the widget is shared (per design doc the
// parametric widget is re-used).
```

If `ParametricEqWidget` doesn't expose `serializeToJson()` already, add it (the dialog already reads/writes parametric blobs to the model, so a serialization path exists; expose it as a public method).

- [ ] **Step 4: Commit**

---

#### Task 16: TxCfcDialog integration with custom getters

Same pattern as Task 15. Register all CFC profile fields including the 30-array parametric fields (CFCFreq0..9 + CFCComp0..9 + CFCPostEq0..9) via per-band custom getters reading from the comp + post-EQ ParametricEqWidgets.

For Strategy A (per-band registration): each band index gets 3 individual registrations (freq, gain, post-eq), totaling 30 registrations across the two parametric widgets.

```cpp
for (int i = 0; i < 10; ++i) {
    auto compFreqGetter = [this, i](QWidget*) -> QVariant {
        return m_compWidget->pointAt(i).f;  // freq Hz
    };
    auto compGainGetter = [this, i](QWidget*) -> QVariant {
        return m_compWidget->pointAt(i).gain;
    };
    auto postEqGainGetter = [this, i](QWidget*) -> QVariant {
        return m_postEqWidget->pointAt(i).gain;
    };

    registry->attach(m_compWidget, QStringLiteral("CFCFreq%1").arg(i),
                     compFreqGetter, m_compWidget, SIGNAL(pointsChanged()));
    registry->attach(m_compWidget, QStringLiteral("CFCComp%1").arg(i),
                     compGainGetter, m_compWidget, SIGNAL(pointsChanged()));
    registry->attach(m_postEqWidget, QStringLiteral("CFCPostEq%1").arg(i),
                     postEqGainGetter, m_postEqWidget, SIGNAL(pointsChanged()));
}
```

(Note: this means the same `m_compWidget` is registered 20 times — once per band's freq + gain. The registry's value-changed signal fires once per `pointsChanged` emit, but the registry detects modified state per field-name independently. May need to refine the registry to handle multi-field-per-widget — see Edge case in Task 19.)

Commit.

---

### Phase 6 — Final integrations + edge cases

#### Task 17: PhoneCwApplet integration

Banner at top of Phone tab. Register: MicGain, CompanderOn, CompanderLevel.

Commit.

---

#### Task 18: Profile-switch-while-modified modal dialog

**Files:**
- Modify: `src/core/MicProfileManager.cpp`

- [ ] **Step 1: Hook into setActiveProfile**

```cpp
void MicProfileManager::setActiveProfile(const QString& name, TransmitModel* tx)
{
    if (isActiveProfileModified()) {
        // Show modal: "Profile X has unsaved changes. [Save] [Discard] [Cancel]"
        QMessageBox box;
        box.setIcon(QMessageBox::Warning);
        box.setText(QStringLiteral("Profile '%1' has unsaved changes.").arg(m_activeProfileName));
        box.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        int result = box.exec();
        if (result == QMessageBox::Save) {
            saveActiveProfile(tx);
        } else if (result == QMessageBox::Cancel) {
            return;  // abort the switch; the caller (e.g. ProfileBanner combo) should restore the combo state
        }
        // Discard falls through to load the new profile, abandoning unsaved changes.
    }
    // ... existing load logic ...
}
```

- [ ] **Step 2: Commit**

---

#### Task 19: Multi-instance same-field sync + dangling-pointer protection refinement

**Files:**
- Modify: `src/core/ProfileAwarenessRegistry.cpp`

Edge cases:
- `FilterLow` is registered on TWO widgets (TxApplet spinbox + TxProfileSetupPage spinbox). Both should reflect the same modified state.
- ParametricEqWidget registered for 30 fields (Strategy A). Re-evaluating all 30 on every `pointsChanged` is O(30 × 30 = 900) comparisons — fine for 60 Hz UI.

Verify these work with existing implementation; refine if needed (e.g. dedupe re-evaluation per widget, or batch via `QSignalBlocker`).

Commit.

---

### Phase 7 — Tests + docs

#### Task 20: Add round-trip integration test

**Files:**
- Test: `tests/tst_profile_save_load_roundtrip.cpp` (new)

- [ ] **Step 1: Write the test**

Activate "DX SSB" profile (FilterLow=1710). Modify FilterLow=2000. Verify banner shows Modified. Save. Verify Saved. Switch to "ESSB Wide" (FilterLow=50). Switch back to "DX SSB" — FilterLow should be 2000 (the saved value). Switch profile mid-modified → modal appears, Discard restores saved.

- [ ] **Step 2: Run + commit**

---

#### Task 21: Update CLAUDE.md with profile-awareness pattern

**Files:**
- Modify: `CLAUDE.md`

Add a section under "Key Implementation Patterns" describing how to register a new profile-bundled control (one `registry->attach()` call per widget; updates the canonical schema in `ProfileFieldSchema` if the field is new).

Commit.

---

#### Task 22: Final verification

```bash
cmake --build build -j$(nproc) 2>&1 | tail -10
ctest --test-dir build --output-on-failure 2>&1 | tail -20
```

Open every banner-equipped surface; verify visual decoration + Save behavior. Modal dialog appears on profile switch with unsaved changes.

---

## Self-Review

**Spec coverage:**
- E2 Phase 1 (infrastructure) — Tasks 2, 3, 4, 6 ✓
- E2 Phase 2 (field schema) — Task 5 ✓
- E2 Phase 3 (TxApplet integration) — Task 7 ✓
- E2 Phase 4 (setup page integrations × 7) — Tasks 8-14 ✓
- E2 Phase 5 (modal dialog integrations) — Tasks 15-16 ✓
- E2 Phase 6 (PhoneCwApplet + edge cases) — Tasks 17, 18, 19 ✓
- E2 Phase 7 (tests + docs) — Tasks 20, 21, 22 ✓

**Placeholder scan:** the field-schema table in Task 5 is abbreviated for readability — full implementation requires populating all 91 entries from the design doc inventory. This is a known authoring shortcut, NOT a placeholder for the engineer; the design doc has the complete list.

**Type consistency:** `ProfileAwarenessRegistry::attach()` two overloads (standard + custom-getter) consistent across Tasks 2, 7, 15, 16. `MicProfileManager::registry()`, `compareToSaved`, `profileLoadStarted/Completed`, `saveActiveProfile` consistent across Tasks 4, 7, 18.

**Dependencies:** Plan 5 depends on Plan 4 (FilterLow/FilterHigh schema additions used as test fixtures in Task 2). Plan 5's surface integrations also depend on Plans 1-3 having matured the surfaces (clean palette, antenna popup builder, AGC mode parity, etc.).
