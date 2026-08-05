# VAX TX-Source Toggle Button Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire the existing-but-NYI VAX button on PhoneCwApplet so left-click toggles `MicSource::Vax` as the TX audio source and right-click jumps to Setup → Audio → TX Input. Toggle-off restores the user's previous non-VAX source.

**Architecture:** `TransmitModel` gains a tracked, per-MAC-persisted "previous non-VAX source" plus a `toggleVaxSource(bool)` slot. The PhoneCwApplet button binds to that slot bidirectionally with `m_updatingFromModel` echo guards (matching the existing PROC/DEXP pattern). The TxApplet mic-source badge extends from a 2-way to a 3-way switch.

**Tech Stack:** C++20, Qt6 (QPushButton, QSignalBlocker, customContextMenuRequested), AppSettings (NereusSDR XML persistence, NOT QSettings), QtTest.

**Spec:** [docs/architecture/2026-05-10-vax-tx-source-toggle-design.md](2026-05-10-vax-tx-source-toggle-design.md)

---

## File Structure

| Action | File | Responsibility |
| --- | --- | --- |
| Modify | `src/models/TransmitModel.h` | Declare `previousNonVaxMicSource()` getter, `toggleVaxSource(bool)` slot, `m_previousNonVaxMicSource` member |
| Modify | `src/models/TransmitModel.cpp` | Implement slot, capture-on-set hook in `setMicSource`, load + save `Mic_Source_PreVax` per-MAC + preconnect keys |
| Modify | `src/gui/applets/PhoneCwApplet.cpp` | Remove `NyiOverlay::markNyi(m_vaxBtn, kNyiVax)`; wire toggle + right-click; tooltip; `syncFromModel` initial state |
| Modify | `src/gui/applets/TxApplet.cpp` | Extend mic-source badge to 3-way (Pc/Radio/VAX) at the `micSourceChanged` lambda + `syncFromModel` block |
| Create | `tests/tst_transmit_model_vax_toggle.cpp` | Unit-test toggle behavior, previous-tracking, HL2 lock interaction, persistence round-trip |
| Create | `tests/tst_phone_applet_vax_toggle.cpp` | Widget test: click VAX, click again, right-click signal |
| Modify | `tests/CMakeLists.txt` | Register both new tests via `nereus_add_test(...)` |

No new headers, no new files in `src/`. The `openSetupRequested(category, page)` signal already exists on PhoneCwApplet and is already routed by MainWindow's existing lambda at [MainWindow.cpp:4234](../../src/gui/MainWindow.cpp:4234), so no MainWindow change is needed.

---

## Task 1: TransmitModel state, tracking, persistence, and toggle slot

**Files:**
- Modify: `src/models/TransmitModel.h` (add slot, getter, member; near the existing `setMicSource` declaration around line 1783)
- Modify: `src/models/TransmitModel.cpp` (extend `setMicSource` near 2329, extend load near 1408, extend save near 1714)
- Create: `tests/tst_transmit_model_vax_toggle.cpp`
- Modify: `tests/CMakeLists.txt` (register test, near the existing `tst_transmit_model_mic_source_preconnect` block at line 2949)

- [ ] **Step 1: Write the failing test**

Create `tests/tst_transmit_model_vax_toggle.cpp` with the full content below. Patterned on `tests/tst_transmit_model_mic_source_preconnect.cpp` (same `clearState` shape, unique MAC per case).

```cpp
// =================================================================
// tests/tst_transmit_model_vax_toggle.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test file. No Thetis port at this layer.
//
// Verifies the PhoneCwApplet VAX-button toggle plumbing on
// TransmitModel: previous non-VAX source tracking, toggleVaxSource
// slot, HL2 lock interaction, and Mic_Source_PreVax persistence
// (per-MAC + preconnect fallback).
//
// Coverage:
//   defaults_previousIsPc                - cold construct -> Pc default
//   setMicSource_radioTracksPrevious     - explicit Radio captured as previous
//   setMicSource_vaxDoesNotTrackPrevious - Vax write does NOT clobber previous
//   toggleOn_setsVax                     - toggleVaxSource(true) -> Vax
//   toggleOff_restoresPrevious           - toggleVaxSource(false) -> previous
//   toggleOff_hl2LockedRestoresPc        - lock active, previous=Radio coerced
//   persistence_perMacRoundTrip          - PreVax key round-trips per-MAC
//   persistence_preconnectFallback       - missing per-MAC falls back to preconnect
// =================================================================
//
// Modification history (NereusSDR):
//   2026-05-10 - Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude
//                 Code.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>

#include "models/TransmitModel.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TstTransmitModelVaxToggle : public QObject {
    Q_OBJECT

private:
    static QString preconnectMicKey()
    {
        return QStringLiteral("tx/preconnect/Mic_Source");
    }
    static QString preconnectPreVaxKey()
    {
        return QStringLiteral("tx/preconnect/Mic_Source_PreVax");
    }
    static QString perMacMicKey(const QString& mac)
    {
        return QStringLiteral("hardware/%1/tx/Mic_Source").arg(mac);
    }
    static QString perMacPreVaxKey(const QString& mac)
    {
        return QStringLiteral("hardware/%1/tx/Mic_Source_PreVax").arg(mac);
    }

    void clearState(const QString& mac)
    {
        auto& s = AppSettings::instance();
        s.remove(preconnectMicKey());
        s.remove(preconnectPreVaxKey());
        s.remove(perMacMicKey(mac));
        s.remove(perMacPreVaxKey(mac));
    }

private slots:

    void defaults_previousIsPc()
    {
        TransmitModel m;
        QCOMPARE(m.previousNonVaxMicSource(), MicSource::Pc);
    }

    void setMicSource_radioTracksPrevious()
    {
        const QString mac = QStringLiteral("tst-vaxtog-aa-bb-cc-dd-ee-01");
        clearState(mac);

        TransmitModel m;
        m.loadFromSettings(mac);
        m.setMicSource(MicSource::Radio);

        QCOMPARE(m.micSource(), MicSource::Radio);
        QCOMPARE(m.previousNonVaxMicSource(), MicSource::Radio);
    }

    void setMicSource_vaxDoesNotTrackPrevious()
    {
        const QString mac = QStringLiteral("tst-vaxtog-aa-bb-cc-dd-ee-02");
        clearState(mac);

        TransmitModel m;
        m.loadFromSettings(mac);
        m.setMicSource(MicSource::Radio);
        QCOMPARE(m.previousNonVaxMicSource(), MicSource::Radio);

        m.setMicSource(MicSource::Vax);
        // Previous stays Radio. Toggling off should land back on Radio.
        QCOMPARE(m.previousNonVaxMicSource(), MicSource::Radio);
    }

    void toggleOn_setsVax()
    {
        const QString mac = QStringLiteral("tst-vaxtog-aa-bb-cc-dd-ee-03");
        clearState(mac);

        TransmitModel m;
        m.loadFromSettings(mac);
        m.toggleVaxSource(true);
        QCOMPARE(m.micSource(), MicSource::Vax);
    }

    void toggleOff_restoresPrevious()
    {
        const QString mac = QStringLiteral("tst-vaxtog-aa-bb-cc-dd-ee-04");
        clearState(mac);

        TransmitModel m;
        m.loadFromSettings(mac);
        m.setMicSource(MicSource::Radio);
        m.toggleVaxSource(true);
        QCOMPARE(m.micSource(), MicSource::Vax);

        m.toggleVaxSource(false);
        QCOMPARE(m.micSource(), MicSource::Radio);
    }

    void toggleOff_hl2LockedRestoresPc()
    {
        const QString mac = QStringLiteral("tst-vaxtog-aa-bb-cc-dd-ee-05");
        clearState(mac);

        TransmitModel m;
        m.loadFromSettings(mac);
        // Pretend Radio was the previous source on a non-HL2 radio.
        m.setMicSource(MicSource::Radio);
        QCOMPARE(m.previousNonVaxMicSource(), MicSource::Radio);

        // Switch to HL2: lock engages, current Radio coerces to Pc.
        m.setMicSourceLocked(true);
        QCOMPARE(m.micSource(), MicSource::Pc);
        // The lock-coerce write also updated previous to Pc.
        QCOMPARE(m.previousNonVaxMicSource(), MicSource::Pc);

        m.toggleVaxSource(true);
        QCOMPARE(m.micSource(), MicSource::Vax);
        m.toggleVaxSource(false);
        QCOMPARE(m.micSource(), MicSource::Pc);
    }

    void persistence_perMacRoundTrip()
    {
        const QString mac = QStringLiteral("tst-vaxtog-aa-bb-cc-dd-ee-06");
        clearState(mac);

        {
            TransmitModel m;
            m.loadFromSettings(mac);
            m.setMicSource(MicSource::Radio);  // captures Radio as previous + persists
            QCOMPARE(AppSettings::instance().value(perMacPreVaxKey(mac)).toString(),
                     QStringLiteral("Radio"));
        }

        {
            TransmitModel m;
            m.loadFromSettings(mac);
            QCOMPARE(m.previousNonVaxMicSource(), MicSource::Radio);
        }
    }

    void persistence_preconnectFallback()
    {
        const QString mac = QStringLiteral("tst-vaxtog-aa-bb-cc-dd-ee-07");
        clearState(mac);

        // User picks Radio in Setup before connecting (no MAC bound).
        TransmitModel pre;
        pre.setMicSource(MicSource::Radio);
        QCOMPARE(AppSettings::instance().value(preconnectPreVaxKey()).toString(),
                 QStringLiteral("Radio"));

        // First connect to this radio: per-MAC PreVax absent, fall back to preconnect.
        TransmitModel m;
        m.loadFromSettings(mac);
        QCOMPARE(m.previousNonVaxMicSource(), MicSource::Radio);
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelVaxToggle)
#include "tst_transmit_model_vax_toggle.moc"
```

Add the registration in `tests/CMakeLists.txt` immediately after the `tst_transmit_model_mic_source_preconnect` block at line 2949:

```cmake
# ── Phase 3M-VAX-toggle: TransmitModel previousNonVaxMicSource + toggleVaxSource ──
# Verifies the model-layer plumbing for the PhoneCwApplet VAX button:
# previous-source tracking on every non-VAX setMicSource write, toggleVaxSource
# slot (Vax on -> sets Vax; off -> restores previous), HL2 lock interaction
# (Radio previous coerces to Pc), and Mic_Source_PreVax persistence (per-MAC
# + preconnect fallback, mirroring the existing Mic_Source two-key pattern).
nereus_add_test(tst_transmit_model_vax_toggle)
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build build --target tst_transmit_model_vax_toggle
```

Expected: FAIL with "no member named 'previousNonVaxMicSource' in 'NereusSDR::TransmitModel'" or "no member named 'toggleVaxSource'". Compile error is acceptable.

- [ ] **Step 3: Add header declarations**

In `src/models/TransmitModel.h`, locate the `setMicSource` declaration (around line 1783). Add immediately after it:

```cpp
    /// Returns the most recent non-VAX source the user has selected.
    /// Tracked automatically by setMicSource() whenever a non-VAX source
    /// is written. Default Pc on first run.
    MicSource previousNonVaxMicSource() const noexcept { return m_previousNonVaxMicSource; }
```

In the `public slots:` block (locate the existing setter slots, e.g. `setMicSource`), add:

```cpp
    /// Quick toggle wired to the PhoneCwApplet VAX button. on=true sets
    /// MicSource::Vax. on=false restores previousNonVaxMicSource() (which
    /// the lock guard in setMicSource will coerce to Pc on HL2).
    void toggleVaxSource(bool on);
```

In the private member section (locate `MicSource m_micSource{MicSource::Pc};` near line 2253), add immediately below it:

```cpp
    MicSource m_previousNonVaxMicSource{MicSource::Pc};
```

- [ ] **Step 4: Implement the slot, the tracking hook, and persistence in TransmitModel.cpp**

Edit `src/models/TransmitModel.cpp::setMicSource` (line 2329-2366) to capture non-Vax writes BEFORE the existing persistence block. Replace the function body with:

```cpp
void TransmitModel::setMicSource(MicSource source)
{
    if (source == MicSource::Radio && m_micSourceLocked) {
        source = MicSource::Pc;
    }

    if (source == m_micSource) { return; }
    m_micSource = source;

    // Capture every non-Vax write as the "previous" source so toggling
    // VAX off restores the user's most recent explicit choice. Updates
    // both the per-MAC and preconnect persistence keys to mirror the
    // Mic_Source two-key pattern.
    if (source != MicSource::Vax && source != m_previousNonVaxMicSource) {
        m_previousNonVaxMicSource = source;
        QString preVaxStr = (source == MicSource::Radio)
                                ? QStringLiteral("Radio")
                                : QStringLiteral("Pc");
        if (m_persistMac.isEmpty()) {
            AppSettings::instance().setValue(
                QStringLiteral("tx/preconnect/Mic_Source_PreVax"), preVaxStr);
        } else {
            persistOne(QStringLiteral("Mic_Source_PreVax"), preVaxStr);
        }
    }

    QString persistStr;
    switch (source) {
        case MicSource::Radio: persistStr = QStringLiteral("Radio"); break;
        case MicSource::Vax:   persistStr = QStringLiteral("Vax");   break;
        case MicSource::Pc:
        default:               persistStr = QStringLiteral("Pc");    break;
    }
    if (m_persistMac.isEmpty()) {
        AppSettings::instance().setValue(
            QStringLiteral("tx/preconnect/Mic_Source"), persistStr);
    } else {
        persistOne(QStringLiteral("Mic_Source"), persistStr);
    }
    emit micSourceChanged(source);
}
```

Add the slot implementation immediately below `setMicSource`:

```cpp
void TransmitModel::toggleVaxSource(bool on)
{
    if (on) {
        setMicSource(MicSource::Vax);
    } else {
        setMicSource(m_previousNonVaxMicSource);
    }
}
```

In the load block at `loadFromSettings` (line 1408-1438), add the PreVax read immediately after the existing Mic_Source block (around line 1438). Use the same per-MAC-then-preconnect-fallback shape:

```cpp
    // ── Mic source previous (PhoneCwApplet VAX-toggle restore target) ─────
    // Lookup order matches Mic_Source: per-MAC -> preconnect -> default Pc.
    const QString perMacPreVaxStr = s.value(pfx + QLatin1String("Mic_Source_PreVax"),
                                             QString()).toString();
    QString preVaxStr;
    if (perMacPreVaxStr.isEmpty()) {
        preVaxStr = AppSettings::instance().value(
            QStringLiteral("tx/preconnect/Mic_Source_PreVax"),
            QStringLiteral("Pc")).toString();
    } else {
        preVaxStr = perMacPreVaxStr;
    }
    m_previousNonVaxMicSource = (preVaxStr == QLatin1String("Radio"))
                                    ? MicSource::Radio
                                    : MicSource::Pc;
```

In the save block at `saveToSettings` (the `Mic_Source` write at line 1714-1724), add directly below it:

```cpp
    // Mic_Source_PreVax mirrors Mic_Source persistence; tracked by setMicSource.
    {
        QString preVaxStr = (m_previousNonVaxMicSource == MicSource::Radio)
                                ? QStringLiteral("Radio")
                                : QStringLiteral("Pc");
        s.setValue(pfx + QLatin1String("Mic_Source_PreVax"), preVaxStr);
    }
```

- [ ] **Step 5: Run test to verify it passes**

```bash
cmake --build build --target tst_transmit_model_vax_toggle && ctest --test-dir build -R "^tst_transmit_model_vax_toggle$" --output-on-failure
```

Expected: PASS, all 8 cases.

- [ ] **Step 6: Commit (GPG-signed)**

```bash
git add src/models/TransmitModel.h src/models/TransmitModel.cpp tests/tst_transmit_model_vax_toggle.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(tx): TransmitModel previousNonVaxMicSource + toggleVaxSource slot

Adds the model-layer plumbing for the PhoneCwApplet VAX button. Every
non-Vax write to setMicSource captures the source as previous, persisted
per-MAC (Mic_Source_PreVax) with a preconnect fallback that mirrors the
existing Mic_Source two-key pattern. toggleVaxSource(bool) is the slot
the button binds to.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: PhoneCwApplet wires the VAX button

**Files:**
- Modify: `src/gui/applets/PhoneCwApplet.cpp` (remove NYI mark at line 541; add wiring + right-click; tooltip; syncFromModel update)
- Create: `tests/tst_phone_applet_vax_toggle.cpp`
- Modify: `tests/CMakeLists.txt` (register test)

- [ ] **Step 1: Write the failing test**

Create `tests/tst_phone_applet_vax_toggle.cpp`:

```cpp
// =================================================================
// tests/tst_phone_applet_vax_toggle.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test file. No Thetis port at this layer.
//
// Verifies the PhoneCwApplet VAX button:
//   click_setsVax              - left-click toggles MicSource to Vax
//   secondClick_restoresPrevious - second click reverts to previous source
//   modelChange_syncsButton    - external micSource change updates checked state
//   rightClick_emitsSetupReq   - right-click emits openSetupRequested("Audio", "TX Input")
//   nyiMark_absent             - the NyiOverlay mark removed
// =================================================================
//
// Modification history (NereusSDR):
//   2026-05-10 - Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude
//                 Code.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>
#include <QPushButton>
#include <QSignalSpy>
#include <QApplication>

#include "gui/applets/PhoneCwApplet.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {
QPushButton* findVaxButton(PhoneCwApplet* applet)
{
    // Locate by accessibleName set in PhoneCwApplet::buildUI (line 401).
    const auto buttons = applet->findChildren<QPushButton*>();
    for (auto* b : buttons) {
        if (b->accessibleName() == QStringLiteral("VAX digital audio")) {
            return b;
        }
    }
    return nullptr;
}
}

class TstPhoneAppletVaxToggle : public QObject {
    Q_OBJECT

private slots:

    void click_setsVax()
    {
        RadioModel model;
        PhoneCwApplet applet(&model);
        auto* btn = findVaxButton(&applet);
        QVERIFY(btn != nullptr);

        QVERIFY(!btn->isChecked());
        btn->click();
        QCOMPARE(model.transmitModel().micSource(), MicSource::Vax);
        QVERIFY(btn->isChecked());
    }

    void secondClick_restoresPrevious()
    {
        RadioModel model;
        model.transmitModel().setMicSource(MicSource::Radio);

        PhoneCwApplet applet(&model);
        auto* btn = findVaxButton(&applet);
        QVERIFY(btn != nullptr);

        btn->click();  // -> Vax
        QCOMPARE(model.transmitModel().micSource(), MicSource::Vax);

        btn->click();  // -> Radio (previous)
        QCOMPARE(model.transmitModel().micSource(), MicSource::Radio);
        QVERIFY(!btn->isChecked());
    }

    void modelChange_syncsButton()
    {
        RadioModel model;
        PhoneCwApplet applet(&model);
        auto* btn = findVaxButton(&applet);
        QVERIFY(btn != nullptr);

        QVERIFY(!btn->isChecked());
        model.transmitModel().setMicSource(MicSource::Vax);
        QVERIFY(btn->isChecked());
        model.transmitModel().setMicSource(MicSource::Pc);
        QVERIFY(!btn->isChecked());
    }

    void rightClick_emitsSetupReq()
    {
        RadioModel model;
        PhoneCwApplet applet(&model);
        auto* btn = findVaxButton(&applet);
        QVERIFY(btn != nullptr);

        QSignalSpy spy(&applet, &PhoneCwApplet::openSetupRequested);

        // Trigger the customContextMenuRequested handler directly.
        emit btn->customContextMenuRequested(QPoint(0, 0));

        QCOMPARE(spy.count(), 1);
        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("Audio"));
        QCOMPARE(args.at(1).toString(), QStringLiteral("TX Input"));
    }

    void nyiMark_absent()
    {
        RadioModel model;
        PhoneCwApplet applet(&model);
        auto* btn = findVaxButton(&applet);
        QVERIFY(btn != nullptr);
        // NyiOverlay sets the property "nyiOverlay" on marked widgets;
        // the absence of the overlay is sufficient verification.
        QVERIFY(btn->property("nyiOverlay").isNull()
                || !btn->property("nyiOverlay").toBool());
    }
};

QTEST_MAIN(TstPhoneAppletVaxToggle)
#include "tst_phone_applet_vax_toggle.moc"
```

Register in `tests/CMakeLists.txt` near the existing `nereus_add_test(tst_phone_applet_proc)` at line 1831:

```cmake
# ── Phase 3M-VAX-toggle: PhoneCwApplet VAX button wiring ────────────────────
# Verifies the [VAX] button on the Phone tab now drives MicSource via
# TransmitModel::toggleVaxSource: left-click toggles Vax, second click
# restores the previous non-VAX source, external model changes sync the
# checked state, and right-click emits openSetupRequested("Audio", "TX Input")
# so MainWindow opens Setup -> Audio -> TX Input.
nereus_add_test(tst_phone_applet_vax_toggle)
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build build --target tst_phone_applet_vax_toggle && ctest --test-dir build -R "^tst_phone_applet_vax_toggle$" --output-on-failure
```

Expected: FAIL on `click_setsVax` (button is NYI-marked and has no signal binding) and `nyiMark_absent` (NYI property still set).

- [ ] **Step 3: Remove the NYI mark in PhoneCwApplet.cpp**

In `src/gui/applets/PhoneCwApplet.cpp` at line 541, delete this single line:

```cpp
    NyiOverlay::markNyi(m_vaxBtn,           kNyiVax);     // #8 — Phase 3-VAX
```

Replace with a one-line comment so the surrounding NYI-marker block stays readable:

```cpp
    // #8 m_vaxBtn: wired (Phase 3M-VAX-toggle)
```

- [ ] **Step 4: Add the button wiring in `wireControls()`**

Locate the PROC button wiring around line 893-912 in `src/gui/applets/PhoneCwApplet.cpp`. Immediately AFTER that block (before the DEXP block), add:

```cpp
    // ── #8 VAX button ↔ TransmitModel::toggleVaxSource ─────────────────────
    // Left-click toggles MicSource between Vax and the user's previous
    // non-VAX source (tracked by TransmitModel::previousNonVaxMicSource,
    // persisted per-MAC). Right-click opens Setup -> Audio -> TX Input.
    if (m_vaxBtn) {
        m_vaxBtn->setToolTip(QStringLiteral(
            "VAX digital audio input.\n"
            "Left-click: toggle VAX as the TX audio source.\n"
            "Right-click: open Setup → Audio → TX Input."));
        {
            QSignalBlocker b(m_vaxBtn);
            m_vaxBtn->setChecked(tx.micSource() == MicSource::Vax);
        }
        // UI → Model
        connect(m_vaxBtn, &QPushButton::toggled, this, [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.toggleVaxSource(on);
        });
        // Model → UI (mirrors button to model so external changes - profile
        // load, future combo wiring, MMIO - keep the checked state honest).
        connect(&tx, &TransmitModel::micSourceChanged, this, [this](MicSource src) {
            m_updatingFromModel = true;
            {
                QSignalBlocker b(m_vaxBtn);
                m_vaxBtn->setChecked(src == MicSource::Vax);
            }
            m_updatingFromModel = false;
        });
        // Right-click → Setup → Audio → TX Input.
        m_vaxBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_vaxBtn, &QPushButton::customContextMenuRequested, this,
                [this](const QPoint&) {
            emit openSetupRequested(QStringLiteral("Audio"),
                                    QStringLiteral("TX Input"));
        });
    }
```

- [ ] **Step 5: Update `syncFromModel()`**

Locate `PhoneCwApplet::syncFromModel()` (search for the function). Inside it, add the VAX button initial-state line in the same style as the existing `m_procBtn->setChecked(tx.cpdrOn())` mirror:

```cpp
    if (m_vaxBtn) {
        QSignalBlocker b(m_vaxBtn);
        m_vaxBtn->setChecked(tx.micSource() == MicSource::Vax);
    }
```

- [ ] **Step 6: Run the test to verify it passes**

```bash
cmake --build build --target tst_phone_applet_vax_toggle && ctest --test-dir build -R "^tst_phone_applet_vax_toggle$" --output-on-failure
```

Expected: PASS, all 5 cases.

- [ ] **Step 7: Commit (GPG-signed)**

```bash
git add src/gui/applets/PhoneCwApplet.cpp tests/tst_phone_applet_vax_toggle.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(phone): wire VAX button to TransmitModel.toggleVaxSource

Removes the Phase 3-VAX NyiOverlay mark and binds the [VAX] button:
left-click toggles MicSource between Vax and the previous non-VAX
source, right-click emits openSetupRequested("Audio", "TX Input")
which MainWindow's existing lambda routes to Setup -> Audio -> TX Input.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: TxApplet mic-source badge gains the VAX case

**Files:**
- Modify: `src/gui/applets/TxApplet.cpp` (extend the `micSourceChanged` lambda at 1303-1309 and the `syncFromModel` block at 1549-1555)

- [ ] **Step 1: Write the failing test**

Extend the existing `tst_phone_applet_vax_toggle.cpp` with a new badge-only check, OR create a thin sibling test. Use the sibling pattern to keep concerns separate. Append to `tests/tst_phone_applet_vax_toggle.cpp` an additional `private slots:` case is wrong because the badge lives on TxApplet, not PhoneCwApplet. Instead, create `tests/tst_tx_applet_mic_source_badge.cpp`:

```cpp
// =================================================================
// tests/tst_tx_applet_mic_source_badge.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test file. No Thetis port at this layer.
//
// Verifies the TxApplet mic-source badge text for all three MicSource
// values: Pc -> "PC mic", Radio -> "Radio mic", Vax -> "VAX". Covers
// both the live micSourceChanged path and the syncFromModel path.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-05-10 - Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude
//                 Code.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>
#include <QLabel>

#include "gui/applets/TxApplet.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {
QLabel* findBadge(TxApplet* applet)
{
    const auto labels = applet->findChildren<QLabel*>();
    for (auto* l : labels) {
        if (l->accessibleName() == QStringLiteral("Mic source indicator")) {
            return l;
        }
    }
    return nullptr;
}
}

class TstTxAppletMicSourceBadge : public QObject {
    Q_OBJECT

private slots:

    void liveChange_Pc()
    {
        RadioModel model;
        TxApplet applet(&model);
        auto* badge = findBadge(&applet);
        QVERIFY(badge != nullptr);

        model.transmitModel().setMicSource(MicSource::Radio);
        QCOMPARE(badge->text(), QStringLiteral("Radio mic"));
        model.transmitModel().setMicSource(MicSource::Pc);
        QCOMPARE(badge->text(), QStringLiteral("PC mic"));
    }

    void liveChange_Vax()
    {
        RadioModel model;
        TxApplet applet(&model);
        auto* badge = findBadge(&applet);
        QVERIFY(badge != nullptr);

        model.transmitModel().setMicSource(MicSource::Vax);
        QCOMPARE(badge->text(), QStringLiteral("VAX"));
    }

    void syncFromModel_Vax()
    {
        RadioModel model;
        model.transmitModel().setMicSource(MicSource::Vax);

        TxApplet applet(&model);
        auto* badge = findBadge(&applet);
        QVERIFY(badge != nullptr);
        applet.syncFromModel();
        QCOMPARE(badge->text(), QStringLiteral("VAX"));
    }
};

QTEST_MAIN(TstTxAppletMicSourceBadge)
#include "tst_tx_applet_mic_source_badge.moc"
```

Register in `tests/CMakeLists.txt` near the other TxApplet tests (search for `nereus_add_test(tst_tx_applet_lev_eq_cfc)` and add immediately after):

```cmake
# ── Phase 3M-VAX-toggle: TxApplet mic-source badge 3-way ────────────────────
# The badge above the gauges previously read "PC mic" or "Radio mic".
# Phase 3M-VAX-toggle extends it to a 3-way switch so MicSource::Vax
# renders "VAX" both on micSourceChanged and on syncFromModel.
nereus_add_test(tst_tx_applet_mic_source_badge)
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build build --target tst_tx_applet_mic_source_badge && ctest --test-dir build -R "^tst_tx_applet_mic_source_badge$" --output-on-failure
```

Expected: `liveChange_Vax` and `syncFromModel_Vax` FAIL. The current 2-way switch returns "PC mic" for Vax (the default branch).

- [ ] **Step 3: Extend the live-change lambda**

In `src/gui/applets/TxApplet.cpp` at lines 1303-1309, replace:

```cpp
    connect(&tx, &TransmitModel::micSourceChanged,
            this, [this](MicSource source) {
        m_micSourceBadge->setText(
            source == MicSource::Radio
                ? QStringLiteral("Radio mic")
                : QStringLiteral("PC mic"));
    });
```

with:

```cpp
    connect(&tx, &TransmitModel::micSourceChanged,
            this, [this](MicSource source) {
        QString text;
        switch (source) {
            case MicSource::Radio: text = QStringLiteral("Radio mic"); break;
            case MicSource::Vax:   text = QStringLiteral("VAX");       break;
            case MicSource::Pc:
            default:               text = QStringLiteral("PC mic");    break;
        }
        m_micSourceBadge->setText(text);
    });
```

- [ ] **Step 4: Extend `syncFromModel` block**

In `src/gui/applets/TxApplet.cpp` at lines 1549-1555, replace:

```cpp
    if (m_micSourceBadge) {
        m_micSourceBadge->setText(
            tx.micSource() == MicSource::Radio
                ? QStringLiteral("Radio mic")
                : QStringLiteral("PC mic"));
    }
```

with:

```cpp
    if (m_micSourceBadge) {
        QString text;
        switch (tx.micSource()) {
            case MicSource::Radio: text = QStringLiteral("Radio mic"); break;
            case MicSource::Vax:   text = QStringLiteral("VAX");       break;
            case MicSource::Pc:
            default:               text = QStringLiteral("PC mic");    break;
        }
        m_micSourceBadge->setText(text);
    }
```

- [ ] **Step 5: Run test to verify it passes**

```bash
cmake --build build --target tst_tx_applet_mic_source_badge && ctest --test-dir build -R "^tst_tx_applet_mic_source_badge$" --output-on-failure
```

Expected: PASS, all 3 cases.

- [ ] **Step 6: Commit (GPG-signed)**

```bash
git add src/gui/applets/TxApplet.cpp tests/tst_tx_applet_mic_source_badge.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(tx): mic-source badge renders VAX as third state

Extends the TxApplet mic-source badge from a 2-way (Radio/Pc) to a
3-way switch. MicSource::Vax now reads "VAX" both on the live
micSourceChanged path and via syncFromModel.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Final verification + manual bench

- [ ] **Step 1: Run the full test suite to catch regressions**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: PASS. No new failures elsewhere. Any pre-existing flakes are unrelated to this change.

- [ ] **Step 2: Build and launch the app**

```bash
cmake --build build && pkill -f NereusSDR 2>/dev/null; sleep 1; ./build/NereusSDR &
```

Expected: app launches cleanly.

- [ ] **Step 3: Manual bench checklist (Linux + ANAN-G2 if available)**

Walk through the spec §Verification matrix:

1. Connect to a radio. PhoneCwApplet [VAX] button is no longer overlaid with the NYI badge.
2. Click [VAX]. TxApplet mic-source badge changes to `VAX`. Mic input audio should reach the VAX TX virtual sink (verify via `pavucontrol` on Linux, Audio MIDI Setup on macOS, Loopback on Windows).
3. Click [VAX] again. Badge returns to `PC mic` (or `Radio mic` if Radio was the prior state).
4. Right-click [VAX]. Setup dialog opens at Audio → TX Input.
5. Quit and relaunch the app. The button reflects the persisted `Mic_Source`. Click [VAX] off (if it loaded on) and verify it lands on the persisted `Mic_Source_PreVax` value.
6. On HL2 (or with `setMicSourceLocked(true)` simulated): VAX still toggles on; toggle-off lands on `Pc` regardless of any prior Radio state.

Take screenshots of states 2, 3, and 4 for the PR description.

- [ ] **Step 4: Open the PR**

```bash
git push -u origin claude/hungry-rubin-bec84a
gh pr create --title "feat(phone,tx): wire VAX button as quick TX-source toggle" --body "$(cat <<'EOF'
## Summary
- PhoneCwApplet [VAX] button left-click toggles `MicSource::Vax`; second click restores the previous non-VAX source (Pc or Radio).
- Right-click [VAX] opens Setup → Audio → TX Input.
- TxApplet mic-source badge gains a `VAX` state.
- New per-MAC + preconnect persistence key `Mic_Source_PreVax` survives app restart.

## Test plan
- [ ] `ctest --test-dir build --output-on-failure` green (incl. 3 new test executables: `tst_transmit_model_vax_toggle`, `tst_phone_applet_vax_toggle`, `tst_tx_applet_mic_source_badge`).
- [ ] Manual bench on Linux + ANAN-G2: clicks 1→4 from the design spec §Verification matrix.
- [ ] App restart with VAX on: button reflects model, toggle-off restores prior source.
- [ ] HL2 (or simulated lock): toggle-off lands on Pc regardless.

Spec: `docs/architecture/2026-05-10-vax-tx-source-toggle-design.md`

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

---

## Self-review (post-write)

- **Spec coverage:** Goals 1-4 from the spec are covered by Tasks 1-3 (toggle, right-click, persistence, button-state mirror). Tests for every case in spec §Tests rows 1-8 are split between `tst_transmit_model_vax_toggle` (8 cases) and `tst_phone_applet_vax_toggle` (5 cases). Spec §Edge Cases entries map to test cases: HL2 lock → `toggleOff_hl2LockedRestoresPc`; profile load → `modelChange_syncsButton`; pre-connect → `persistence_preconnectFallback`; multi-MAC → unique MACs per test. The "External change to Vax via combo (future)" case is covered by `modelChange_syncsButton`.
- **Placeholder scan:** No TBDs, no "implement later", no "similar to Task N", no "appropriate error handling". All code blocks contain real code; all bash commands are concrete.
- **Type consistency:** `previousNonVaxMicSource()` getter, `toggleVaxSource(bool)` slot, `m_previousNonVaxMicSource` member, `Mic_Source_PreVax` AppSettings key (per-MAC + preconnect) used identically across header, .cpp, and tests. The badge accessibleName lookup `"Mic source indicator"` matches the existing `setAccessibleName` call at [TxApplet.cpp:239](../../src/gui/applets/TxApplet.cpp:239). The button accessibleName `"VAX digital audio"` matches the existing `setAccessibleName` at [PhoneCwApplet.cpp:401](../../src/gui/applets/PhoneCwApplet.cpp:401).
