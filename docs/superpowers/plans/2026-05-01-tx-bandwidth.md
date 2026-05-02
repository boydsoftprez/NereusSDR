# TX Bandwidth Filtering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement TX bandwidth filtering as a per-profile field with full TX Profile integration: spinbox cluster on TxApplet, per-profile persistence, panadapter overlay (always-on), waterfall MOX-gated overlay, status text, and user-customisable filter overlay color/opacity for both TX and RX. ~590 lines net.

**Architecture:** TX BW values live on `MicProfile` (extending the existing 91-field schema with `FilterLow` + `FilterHigh`). Activating a profile pushes filter values into runtime via `MicProfileManager::setActiveProfile`; manual edits update runtime AND mark the profile as modified (cleared on Save — re-selecting profile reverts). `TxChannel::setTxFilter` reads from `TransmitModel` instead of using the existing hardcoded mode-based defaults at `TxChannel.cpp:520-528`. SpectrumWidget gains `setTxFilterRange(low, high)` that drives a translucent orange overlay on the panadapter (always visible) plus a vertical column on the waterfall (MOX-gated via `MoxController`).

**Tech Stack:** Qt 6 / C++20 / WDSP. Depends on Plan 1 (Foundation — canonical palette for new colors). Independent of Plans 2, 3 — can land in parallel after Foundation. Plan 5 (Profile-awareness) depends on this plan's `FilterLow`/`FilterHigh` schema additions.

---

## Reference

- **Design doc:** `docs/architecture/ui-audit-polish-plan.md` — Phase E1 (D1-D9 + D9b + D9c).
- **Prerequisite:** Plan 1 (Foundation).
- **Independent of:** Plans 2, 3 — these can run in parallel.
- **Blocks:** Plan 5 — profile-awareness uses FilterLow/FilterHigh as registry test fixtures.
- **Key code paths:**
  - `src/core/TxChannel.cpp:520-528` — current hardcoded mode-based bandpass at tune time
  - `src/core/TxChannel.cpp:951-960` — existing `setTxFilter(low, high)` wrapper for `SetTXABandpassFreqs`
  - `src/core/MicProfileManager.{h,cpp}` — profile schema + 20 factory profiles ported from Thetis `database.cs:9282-9418`
  - `src/models/TransmitModel.h` — currently 5 Q_PROPERTYs + ~50 raw member fields; needs `filterLow`/`filterHigh` + signal + persistence
  - `src/gui/SpectrumWidget.h:386-484` — existing `setShowTxFilterOnRxWaterfall` / `setTxFilterVisible` infrastructure (paint code stub at line 1584 `Q_UNUSED`)
  - `src/gui/setup/{TxProfileSetupPage,DisplaySetupPages}.cpp` — D6 + D9b setup-page entries

---

## File-Structure Overview

### New code
- `src/gui/applets/TxFilterEditDialog.{h,cpp}` (new, optional — only if a fine-tune dialog is desired beyond direct spinbox edit; per D2 the spinboxes live directly on TxApplet so this dialog is not strictly required. Keep this in scope as a stretch goal for power-user "open in dialog" via right-click on the spinbox row; defer if not needed.)

### Files modified
- `src/models/TransmitModel.{h,cpp}` (D1, D5, D8: add Q_PROPERTYs, signals, persistence)
- `src/core/MicProfileManager.{h,cpp}` (D1, D5: schema additions, profile activation pushes filter values, 20 factory profiles seeded with values from Thetis database.cs)
- `src/core/TxChannel.{h,cpp}` (D8: read filter from TransmitModel instead of hardcoded; connect to filterChanged for live update with debounce)
- `src/gui/applets/TxApplet.{h,cpp}` (D2, D3: spinbox cluster after Profile combo; D9: status text label)
- `src/gui/setup/TxProfileSetupPage.cpp` (D6: TX Filter group with Low/High spinboxes)
- `src/gui/SpectrumWidget.{h,cpp}` (D9: setTxFilterRange + paint code; D9b: m_txFilterColor/m_rxFilterColor + setters; D9c: split zero-line color, reset method, TNF/SubRX scaffolding)
- `src/gui/MainWindow.cpp` (D9: connects from TransmitModel/MoxController to SpectrumWidget)
- `src/gui/setup/DisplaySetupPages.cpp` (D9b, D9c: ColorSwatchButton call sites for TX/RX filter color, split zero-line color, "Reset all display colors" button)
- `src/gui/StyleConstants.h` (D9: kTxFilterOverlayFill / Border / Label palette additions)

### Tests added
- `tests/tst_tx_bandwidth_persistence.cpp` (new) — verifies FilterLow/FilterHigh round-trips through MicProfile save/load
- `tests/tst_tx_filter_offset_to_wdsp.cpp` (new) — verifies TransmitModel::setFilterLow → TxChannel::setTxFilter → SetTXABandpassFreqs path
- `tests/tst_filter_display_text.cpp` (new) — verifies the per-mode display string (asymmetric vs symmetric formats)
- `tests/tst_spectrum_tx_overlay.cpp` (new) — verifies setTxFilterRange triggers paint update with the right band coordinates

---

## Tasks

### Task 1: Branch checkpoint after Plan 1 lands

- [ ] **Step 1: Verify Plan 1 commits + clean tree**

```bash
git log --oneline | head -30 | grep -cE "ui\(|style\("
ctest --test-dir build --output-on-failure 2>&1 | tail -5
```

---

### Task 2: D1 — Add FilterLow/FilterHigh to MicProfile schema + signal in TransmitModel

**Files:**
- Modify: `src/models/TransmitModel.{h,cpp}`
- Modify: `src/core/MicProfileManager.{h,cpp}`
- Test: `tests/tst_tx_bandwidth_persistence.cpp` (new)

- [ ] **Step 1: Write the failing test**

```cpp
// tests/tst_tx_bandwidth_persistence.cpp
#include <QtTest/QtTest>
#include "core/MicProfileManager.h"
#include "models/TransmitModel.h"

class TestTxBandwidthPersistence : public QObject {
    Q_OBJECT
private slots:
    void filterValuesPersistAcrossProfileSave();
    void activatingProfilePushesFilterToTransmitModel();
    void modifiedFlagSetsAndClearsOnSave();
};

void TestTxBandwidthPersistence::filterValuesPersistAcrossProfileSave()
{
    NereusSDR::TransmitModel tx;
    NereusSDR::MicProfileManager mgr;

    // Activate "DX SSB" factory profile (per D4 ports: 1710/2710 from
    // database.cs:5240-5241).
    mgr.setActiveProfile("DX SSB", &tx);
    QCOMPARE(tx.filterLow(), 1710);
    QCOMPARE(tx.filterHigh(), 2710);

    // Modify and save.
    tx.setFilterLow(2000);
    tx.setFilterHigh(2900);
    mgr.saveActiveProfile(&tx);

    // Re-activate to round-trip.
    mgr.setActiveProfile("Default Voice", &tx);  // some other profile
    mgr.setActiveProfile("DX SSB", &tx);
    QCOMPARE(tx.filterLow(), 2000);
    QCOMPARE(tx.filterHigh(), 2900);
}

void TestTxBandwidthPersistence::activatingProfilePushesFilterToTransmitModel()
{
    NereusSDR::TransmitModel tx;
    NereusSDR::MicProfileManager mgr;

    QSignalSpy spy(&tx, &NereusSDR::TransmitModel::filterChanged);
    mgr.setActiveProfile("ESSB Wide", &tx);  // 50/3650 per database.cs:6843
    QVERIFY(spy.count() >= 1);
    QCOMPARE(tx.filterLow(), 50);
    QCOMPARE(tx.filterHigh(), 3650);
}

void TestTxBandwidthPersistence::modifiedFlagSetsAndClearsOnSave()
{
    NereusSDR::TransmitModel tx;
    NereusSDR::MicProfileManager mgr;
    mgr.setActiveProfile("DX SSB", &tx);
    QVERIFY(!mgr.isActiveProfileModified());

    tx.setFilterLow(1500);
    QVERIFY(mgr.isActiveProfileModified());

    mgr.saveActiveProfile(&tx);
    QVERIFY(!mgr.isActiveProfileModified());
}

QTEST_MAIN(TestTxBandwidthPersistence)
#include "tst_tx_bandwidth_persistence.moc"
```

Register in `tests/CMakeLists.txt`.

- [ ] **Step 2: Build + run; verify FAIL**

Test fails because `TransmitModel::filterLow/filterHigh/filterChanged` don't exist, and `MicProfileManager::saveActiveProfile/isActiveProfileModified` don't exist.

- [ ] **Step 3: Add Q_PROPERTYs and getters/setters to TransmitModel**

In `src/models/TransmitModel.h`:

```cpp
class TransmitModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(int filterLow  READ filterLow  WRITE setFilterLow  NOTIFY filterChanged)
    Q_PROPERTY(int filterHigh READ filterHigh WRITE setFilterHigh NOTIFY filterChanged)

public:
    int filterLow() const { return m_filterLow; }
    int filterHigh() const { return m_filterHigh; }

    // Setters apply (with debounce in TxChannel; see Task 9).
    // Per docs/architecture/ui-audit-polish-plan.md §E1.D8 + D7.
    void setFilterLow(int hz);
    void setFilterHigh(int hz);

    // Compose a per-mode display string for the status label.
    // Asymmetric modes (USB/LSB/DIGU/DIGL): "100-2900 Hz · 2.8k BW"
    // Symmetric modes (AM/SAM/DSB/FM): "±2500 Hz · 5.0k BW"
    QString filterDisplayText(DSPMode mode) const;

signals:
    void filterChanged(int low, int high);

private:
    // Default to USB voice (100-2900) — typical SSB. Per-profile activation
    // overrides via MicProfileManager (D1).
    int m_filterLow {100};
    int m_filterHigh{2900};
};
```

In `src/models/TransmitModel.cpp`:
```cpp
void TransmitModel::setFilterLow(int hz)
{
    // Per-mode validation (D7): swap inverted pairs on commit.
    if (hz > m_filterHigh) {
        std::swap(hz, m_filterHigh);
    }
    if (m_filterLow != hz) {
        m_filterLow = hz;
        emit filterChanged(m_filterLow, m_filterHigh);
    }
}

void TransmitModel::setFilterHigh(int hz)
{
    if (hz < m_filterLow) {
        std::swap(hz, m_filterLow);
    }
    if (m_filterHigh != hz) {
        m_filterHigh = hz;
        emit filterChanged(m_filterLow, m_filterHigh);
    }
}

QString TransmitModel::filterDisplayText(DSPMode mode) const
{
    auto isSymmetric = [](DSPMode m) {
        return m == DSPMode::AM || m == DSPMode::SAM || m == DSPMode::DSB
            || m == DSPMode::FM;
    };
    const int bw = m_filterHigh - m_filterLow;
    if (isSymmetric(mode)) {
        // For symmetric modes the spinboxes show the absolute upper edge;
        // the effective passband is -high..+high with low at 0.
        return QStringLiteral("±%1 Hz · %2k BW")
            .arg(m_filterHigh)
            .arg(QString::number(2.0 * m_filterHigh / 1000.0, 'f', 1));
    }
    return QStringLiteral("%1-%2 Hz · %3k BW")
        .arg(m_filterLow)
        .arg(m_filterHigh)
        .arg(QString::number(bw / 1000.0, 'f', 1));
}
```

- [ ] **Step 4: Add MicProfileManager schema + saveActiveProfile + isActiveProfileModified**

Read existing `src/core/MicProfileManager.h` to understand schema format. The class today manages profiles via TransmitModel state without an explicit field-schema table.

Add:

```cpp
// In MicProfileManager.h:

public:
    // Per docs/architecture/ui-audit-polish-plan.md §E1.D6.
    // Atomic snapshot of TransmitModel state into the active profile.
    void saveActiveProfile(TransmitModel* tx);

    // True if any profile-bundled field on TransmitModel diverges from
    // the active profile's saved value. Compared field-by-field.
    bool isActiveProfileModified() const;

    // Emitted when the modified state flips.
    Q_SIGNAL void profileModifiedChanged(bool modified);
```

Implement `saveActiveProfile` in MicProfileManager.cpp to write all profile fields (existing 91 + new FilterLow/FilterHigh) into the profile's stored representation (typically AppSettings keys under `mic-profile/<name>/...`).

Implement `isActiveProfileModified` by comparing each profile field on the TransmitModel against the active profile's saved value. For now, comparing FilterLow + FilterHigh is sufficient for the test fixtures; Plan 5 (Profile-awareness) will register the full 91 fields via the registry mechanism.

When `setActiveProfile` is called, push FilterLow + FilterHigh into the TransmitModel via `tx->setFilterLow(savedLow)` + `tx->setFilterHigh(savedHigh)`.

- [ ] **Step 5: Build + run test; verify PASS**

```bash
cmake --build build -j$(nproc) --target tst_tx_bandwidth_persistence
ctest --test-dir build -R tst_tx_bandwidth_persistence --output-on-failure
```

- [ ] **Step 6: Commit**

```bash
git add src/models/TransmitModel.{h,cpp} src/core/MicProfileManager.{h,cpp} tests/tst_tx_bandwidth_persistence.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
model(tx): add filterLow/filterHigh as per-profile fields with persistence

TransmitModel gains Q_PROPERTYs for filterLow / filterHigh + a
filterChanged(low, high) signal + a filterDisplayText(mode) helper
producing per-mode display strings.

MicProfileManager gains saveActiveProfile / isActiveProfileModified /
profileModifiedChanged for the per-profile architecture (per
docs/architecture/ui-audit-polish-plan.md §E1.D1, D5, D6). Activating a
profile pushes the saved FilterLow/FilterHigh into the TransmitModel.

Per-mode validation (low <= high enforced via swap-on-commit) lives in
TransmitModel setters, not the future spinbox UI bounds. DRM lock + AM
symmetric handling deferred to TxChannel application path (Task 9).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: D4 — Seed 20 factory profiles with FilterLow/FilterHigh from Thetis database.cs

**Files:**
- Modify: `src/core/MicProfileManager.cpp` (factory profile constants table)

- [ ] **Step 1: Locate the factory profile table**

```bash
grep -n "Default Voice\|DX SSB\|Ragchew\|ESSB" src/core/MicProfileManager.cpp | head -10
```

Expected: a table or per-profile struct populated at construction with the existing 91 fields (per CLAUDE.md "20 Thetis factory profiles ported verbatim from database.cs:9282-9418").

- [ ] **Step 2: For each factory profile, add FilterLow + FilterHigh values from Thetis**

Per D4 (port verbatim from `database.cs`), each profile's filter values come from its corresponding row. Reference table (with exact line numbers from grep results in design doc):

| Profile | Low | High | Thetis cite |
|---|---|---|---|
| Default Voice (or SSB default) | 100 | 3000 | `database.cs:4546-4547` |
| (next factory) | 200 | 3100 | `:4778-4779` |
| Communications-narrow | 1000 | 2000 | `:5010-5011` |
| DX SSB compressed | 1710 | 2710 | `:5240-5241` |
| AM-style symmetric | 0 | 4000 | `:5469-5470` |
| SSB 3.1k | 100 | 3100 | `:5698-5699` |
| SSB 3.5k DIGI/ESSB-narrow | 100 | 3500 | `:5927`, `:6156`, `:6385` |
| Ragchew | 250 | 3250 | `:6614-6615` |
| ESSB wide | 50 | 3650 | `:6843`, `:7530`, `:7759` |
| (etc.) | 100 | 3100 | `:7072-7073` |
| (etc.) | 100 | 3100 | `:7301-7302` |
| SSB 3.2k | 100 | 3200 | `:7988-7989` |
| (etc.) | 100 | 3200 | `:8217-8218` |

Add `// From Thetis database.cs:<line> [v2.10.3.13]` comment per assignment.

For any NereusSDR profile without a direct Thetis equivalent (rare per CLAUDE.md): use the closest Thetis profile's values as reference and cite as `// NereusSDR-original; modeled after <profile-name>`.

- [ ] **Step 3: Build + run baseline tests**

```bash
cmake --build build -j$(nproc) --target NereusSDR
ctest --test-dir build --output-on-failure 2>&1 | tail -5
```

Now Task 2's `tst_tx_bandwidth_persistence` test asserting `1710/2710` for "DX SSB" passes (was passing before because the test sets values explicitly; now also passes from factory defaults).

- [ ] **Step 4: Commit**

```bash
git add src/core/MicProfileManager.cpp
git commit -m "$(cat <<'EOF'
profile(mic): seed 20 factory profiles with FilterLow/FilterHigh from Thetis

Per docs/architecture/ui-audit-polish-plan.md §E1.D4. Each NereusSDR
factory profile gets its corresponding FilterLow/FilterHigh values
copied verbatim from Thetis database.cs:9282-9418, with inline cite
per assignment.

Source-first protocol: each value carries
// From Thetis database.cs:<line> [v2.10.3.13].

Thetis values are battle-tested across years and thousands of operators;
porting verbatim preserves each profile's distinct character (e.g.
\"DX SSB compressed\" 1710-2710 narrow for intelligibility, vs \"ESSB
wide\" 50-3650 wide for hi-fi voice).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: D2 + D3 — Add TX BW spinbox cluster to TxApplet after Profile combo

**Files:**
- Modify: `src/gui/applets/TxApplet.{h,cpp}`

- [ ] **Step 1: Add member fields to TxApplet.h**

```cpp
private:
    QSpinBox* m_txFilterLowSpin{nullptr};
    QSpinBox* m_txFilterHighSpin{nullptr};
    QLabel*   m_txFilterStatusLabel{nullptr};
```

- [ ] **Step 2: Construct the TX BW row immediately after the Profile combo row**

In `TxApplet.cpp`, find the Profile combo construction (line 549 per inventory). After it, add:

```cpp
// TX BW spinbox cluster — sits directly after Profile combo per
// docs/architecture/ui-audit-polish-plan.md §E1.D3.
{
    auto* row = new QHBoxLayout;
    row->setSpacing(4);

    auto* lbl = new QLabel(QStringLiteral("TX BW"), this);
    lbl->setFixedWidth(56);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; font-weight: bold; }"
    ).arg(NereusSDR::Style::kTitleText));
    row->addWidget(lbl);

    auto* loLbl = new QLabel(QStringLiteral("Lo"), this);
    loLbl->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 9.5px; }"
    ).arg(NereusSDR::Style::kTextSecondary));
    row->addWidget(loLbl);

    m_txFilterLowSpin = new QSpinBox(this);
    m_txFilterLowSpin->setRange(0, 5000);  // per D7
    m_txFilterLowSpin->setSuffix(QStringLiteral(" Hz"));
    m_txFilterLowSpin->setStyleSheet(NereusSDR::Style::kSpinBoxStyle);
    m_txFilterLowSpin->setMinimumWidth(72);
    row->addWidget(m_txFilterLowSpin);

    auto* hiLbl = new QLabel(QStringLiteral("Hi"), this);
    hiLbl->setStyleSheet(loLbl->styleSheet());
    row->addWidget(hiLbl);

    m_txFilterHighSpin = new QSpinBox(this);
    m_txFilterHighSpin->setRange(200, 10000);  // per D7
    m_txFilterHighSpin->setSuffix(QStringLiteral(" Hz"));
    m_txFilterHighSpin->setStyleSheet(NereusSDR::Style::kSpinBoxStyle);
    m_txFilterHighSpin->setMinimumWidth(72);
    row->addWidget(m_txFilterHighSpin);

    vbox->addLayout(row);
}

// Status label below the spinboxes — D9 visual feedback.
m_txFilterStatusLabel = new QLabel(this);
m_txFilterStatusLabel->setAlignment(Qt::AlignRight);
m_txFilterStatusLabel->setStyleSheet(QStringLiteral(
    "QLabel { color: %1; font-size: 9px; font-weight: bold; }"
).arg("#ffaa70"));  // matches Style::kTxFilterOverlayLabel — Task 13
vbox->addWidget(m_txFilterStatusLabel);
```

- [ ] **Step 3: Wire spinboxes to TransmitModel**

```cpp
auto& tx = *m_radioModel->transmitModel();

connect(m_txFilterLowSpin, qOverload<int>(&QSpinBox::valueChanged),
        this, [this, &tx](int v) {
    if (m_updatingFromModel) return;
    tx.setFilterLow(v);
});
connect(m_txFilterHighSpin, qOverload<int>(&QSpinBox::valueChanged),
        this, [this, &tx](int v) {
    if (m_updatingFromModel) return;
    tx.setFilterHigh(v);
});
connect(&tx, &TransmitModel::filterChanged, this,
        [this](int low, int high) {
    QSignalBlocker bLow(m_txFilterLowSpin);
    QSignalBlocker bHigh(m_txFilterHighSpin);
    m_txFilterLowSpin->setValue(low);
    m_txFilterHighSpin->setValue(high);
    auto mode = m_radioModel->currentSlice()->dspMode();
    m_txFilterStatusLabel->setText(
        m_radioModel->transmitModel()->filterDisplayText(mode));
});

// Sync from current state on construction.
m_txFilterLowSpin->setValue(tx.filterLow());
m_txFilterHighSpin->setValue(tx.filterHigh());
m_txFilterStatusLabel->setText(
    tx.filterDisplayText(m_radioModel->currentSlice()->dspMode()));
```

Also connect `dspModeChanged` to refresh the status label (mode change can flip asymmetric ↔ symmetric format).

- [ ] **Step 4: Build + visual**

```bash
cmake --build build -j$(nproc) --target NereusSDR
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
```

Open TxApplet. Profile combo + TX BW row + status label visible. Spinboxes show the active profile's filter values. Switching profile via combo updates spinboxes. Editing spinbox updates status text.

- [ ] **Step 5: Commit**

```bash
git add src/gui/applets/TxApplet.{h,cpp}
git commit -m "$(cat <<'EOF'
ui(tx-applet): add TX BW spinbox cluster after Profile combo (D2 + D3 + D9 status)

Two QSpinBox widgets (Low/High) sit directly below the Profile combo per
§E1.D3 (proximity to profile-row's modified indicator). Range
[0..5000] / [200..10000] per §E1.D7. Wired to TransmitModel::setFilter*
with m_updatingFromModel guard (cross-surface sync via filterChanged).

Status label below spinboxes shows
TransmitModel::filterDisplayText(mode) — asymmetric "100-2900 Hz · 2.8k
BW" or symmetric "±2500 Hz · 5.0k BW" per §E1.D9.

Profile combo's existing right-click → Setup → Audio → TX Profile path
remains the editor for full profile configuration; banner-driven Save
behavior comes in Plan 5 (E2 profile-awareness).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: D6 — Add TX Filter group to TxProfileSetupPage

**Files:**
- Modify: `src/gui/setup/TxProfileSetupPage.cpp`

- [ ] **Step 1: Read existing TxProfileSetupPage layout**

```bash
sed -n '1,80p' src/gui/setup/TxProfileSetupPage.cpp
```

Per inventory: 2 buttons + 1 combo today. Add a new "TX Filter" QGroupBox below the existing controls.

- [ ] **Step 2: Construct the TX Filter group**

```cpp
// TX Filter group — mirrors TxApplet's TX BW cluster per §E1.D6.
auto* filterGroup = new QGroupBox(QStringLiteral("TX Filter"), this);
filterGroup->setStyleSheet(NereusSDR::Style::kGroupBoxStyle);
auto* fl = new QFormLayout(filterGroup);

auto* lowSpin = new QSpinBox(filterGroup);
lowSpin->setRange(0, 5000);
lowSpin->setSuffix(QStringLiteral(" Hz"));
lowSpin->setStyleSheet(NereusSDR::Style::kSpinBoxStyle);
fl->addRow(QStringLiteral("Low cutoff"), lowSpin);

auto* highSpin = new QSpinBox(filterGroup);
highSpin->setRange(200, 10000);
highSpin->setSuffix(QStringLiteral(" Hz"));
highSpin->setStyleSheet(NereusSDR::Style::kSpinBoxStyle);
fl->addRow(QStringLiteral("High cutoff"), highSpin);

contentLayout()->addWidget(filterGroup);

// Wire to TransmitModel — same connect pattern as TxApplet (Task 4).
auto& tx = *m_radioModel->transmitModel();
connect(lowSpin, qOverload<int>(&QSpinBox::valueChanged),
        this, [&tx](int v) { tx.setFilterLow(v); });
connect(highSpin, qOverload<int>(&QSpinBox::valueChanged),
        this, [&tx](int v) { tx.setFilterHigh(v); });
connect(&tx, &TransmitModel::filterChanged, this,
        [lowSpin, highSpin](int lo, int hi) {
    QSignalBlocker b1(lowSpin);
    QSignalBlocker b2(highSpin);
    lowSpin->setValue(lo);
    highSpin->setValue(hi);
});

// Initial sync.
lowSpin->setValue(tx.filterLow());
highSpin->setValue(tx.filterHigh());
```

- [ ] **Step 3: Build + visual**

Open Setup → Audio → TX Profile. New "TX Filter" group with Low/High spinboxes. Editing here updates TxApplet spinboxes via shared model.

- [ ] **Step 4: Commit**

```bash
git add src/gui/setup/TxProfileSetupPage.cpp
git commit -m "ui(tx-profile-setup): add TX Filter group with Low/High spinboxes

Per docs/architecture/ui-audit-polish-plan.md §E1.D6. Mirrors TxApplet's
TX BW cluster on the same model setters; cross-surface sync via
TransmitModel::filterChanged.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: D8 — Wire TxChannel to read filter from TransmitModel with debounce

**Files:**
- Modify: `src/core/TxChannel.{h,cpp}`
- Modify: `src/models/RadioModel.cpp` (route filterChanged → TxChannel)
- Test: `tests/tst_tx_filter_offset_to_wdsp.cpp` (new)

- [ ] **Step 1: Write the failing test**

A test that mocks WDSP's `SetTXABandpassFreqs` (via a function pointer or test seam) and verifies that calling `TransmitModel::setFilterLow(2000)` results in `SetTXABandpassFreqs(channelId, +<low>, +<high>)` being called within 100 ms (post-debounce).

- [ ] **Step 2: Add the QTimer-based debounce in TxChannel**

```cpp
// In TxChannel.h:
private:
    QTimer m_filterDebounceTimer;
    int m_pendingFilterLow{0};
    int m_pendingFilterHigh{0};

public:
    void requestFilterChange(int low, int high);  // call from MainWindow signal connect

private slots:
    void applyPendingFilter();
```

```cpp
// In TxChannel.cpp constructor:
m_filterDebounceTimer.setSingleShot(true);
m_filterDebounceTimer.setInterval(50);  // ~50 ms per §E1.D8
connect(&m_filterDebounceTimer, &QTimer::timeout,
        this, &TxChannel::applyPendingFilter);

// Method:
void TxChannel::requestFilterChange(int low, int high)
{
    m_pendingFilterLow = low;
    m_pendingFilterHigh = high;
    m_filterDebounceTimer.start();  // restarts on each call
}

void TxChannel::applyPendingFilter()
{
    setTxFilter(m_pendingFilterLow, m_pendingFilterHigh);  // existing wrapper
}
```

- [ ] **Step 3: Replace hardcoded mode-based bandpass at TxChannel.cpp:520-528**

The existing `setTuneTone` logic hardcodes per-mode bandpass values:
```cpp
if (isLsb) {
    SetTXABandpassFreqs(m_channelId, -2850.0, -150.0);
} else {
    SetTXABandpassFreqs(m_channelId, +150.0, +2850.0);
}
```

Replace with a read from TransmitModel:
```cpp
// Use the active TransmitModel's per-profile filter values (per §E1.D1 +
// §E1.D8). For LSB-family modes the filter applies to negative-frequency
// space; for USB-family it applies to positive-frequency space; for
// symmetric modes (AM/SAM/DSB/FM) the filter spans -high..+high.
applyTxFilterForMode(currentDspMode());
```

Add a new helper `applyTxFilterForMode(DSPMode mode)` that does the per-mode mapping using `m_transmitModel->filterLow()` / `filterHigh()`:

```cpp
void TxChannel::applyTxFilterForMode(DSPMode mode)
{
    const int lo = m_transmitModel->filterLow();
    const int hi = m_transmitModel->filterHigh();
    auto isLsb = [](DSPMode m) { return m == DSPMode::LSB || m == DSPMode::DIGL || m == DSPMode::CWL; };
    auto isSymmetric = [](DSPMode m) { return m == DSPMode::AM || m == DSPMode::SAM || m == DSPMode::DSB || m == DSPMode::FM; };

    if (isLsb(mode)) {
        SetTXABandpassFreqs(m_channelId, -static_cast<double>(hi), -static_cast<double>(lo));
    } else if (isSymmetric(mode)) {
        // Symmetric: -high..+high (low expected to be 0 by D7 validation).
        SetTXABandpassFreqs(m_channelId, -static_cast<double>(hi), static_cast<double>(hi));
    } else {
        SetTXABandpassFreqs(m_channelId, static_cast<double>(lo), static_cast<double>(hi));
    }
}
```

- [ ] **Step 4: In RadioModel, connect TransmitModel::filterChanged → TxChannel::requestFilterChange**

```cpp
// In RadioModel where TxChannel is owned:
connect(m_transmitModel, &TransmitModel::filterChanged,
        m_txChannel, &TxChannel::requestFilterChange);
```

- [ ] **Step 5: Run test; verify PASS**

- [ ] **Step 6: Commit**

```bash
git add src/core/TxChannel.{h,cpp} src/models/RadioModel.cpp tests/tst_tx_filter_offset_to_wdsp.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
core(tx-channel): read filter from TransmitModel with 50ms debounce; remove hardcoded ±150/±2850

Replaces the hardcoded per-mode bandpass values at TxChannel.cpp:520-528
(USB +150/+2850, LSB -2850/-150, AM ±2850, FM ±3000) with a model-driven
applyTxFilterForMode() helper that reads from
TransmitModel::filterLow/High and applies per-mode sign/symmetry rules.

50 ms QTimer debounce prevents WDSP spam during spinbox arrow-clicks.
Connected from RadioModel: TransmitModel::filterChanged →
TxChannel::requestFilterChange.

Per docs/architecture/ui-audit-polish-plan.md §E1.D8.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: D9 — Add filter overlay palette to StyleConstants.h

**Files:**
- Modify: `src/gui/StyleConstants.h`

- [ ] **Step 1: Add three new constants**

After the existing palette additions (post-Task 3 from Plan 1):

```cpp
// TX filter overlay (panadapter + waterfall).
// Default — translucent orange. Customisable via ColorSwatchButton on
// Setup → Display → TX Display per docs/.../ui-audit-polish-plan.md §E1.D9b.
constexpr auto kTxFilterOverlayFill   = "rgba(255, 120, 60, 46)";
constexpr auto kTxFilterOverlayBorder = "#ff7833";
constexpr auto kTxFilterOverlayLabel  = "#ffaa70";

// RX filter overlay (panadapter).
// Default — translucent cyan matching kAccent. Customisable on
// Setup → Display → Spectrum Defaults.
constexpr auto kRxFilterOverlayFill   = "rgba(0, 180, 216, 80)";
constexpr auto kRxFilterOverlayBorder = "#00b4d8";
```

- [ ] **Step 2: Build (no test needed — additive)**

- [ ] **Step 3: Commit**

```bash
git add src/gui/StyleConstants.h
git commit -m "style(palette): add TX/RX filter overlay defaults for §E1.D9 + D9b

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 8: D9 — Implement panadapter + waterfall TX filter overlay paint code

**Files:**
- Modify: `src/gui/SpectrumWidget.{h,cpp}`
- Modify: `src/gui/MainWindow.cpp`
- Test: `tests/tst_spectrum_tx_overlay.cpp` (new)

- [ ] **Step 1: Add member fields and setter**

In `SpectrumWidget.h`:
```cpp
public:
    void setTxFilterRange(int low, int high);

private:
    int m_txFilterLow {100};
    int m_txFilterHigh{2900};
    QColor m_txFilterColor{255, 120, 60, 46};   // matches kTxFilterOverlayFill
```

In `SpectrumWidget.cpp`:
```cpp
void SpectrumWidget::setTxFilterRange(int low, int high)
{
    if (m_txFilterLow == low && m_txFilterHigh == high) return;
    m_txFilterLow = low;
    m_txFilterHigh = high;
    update();
}
```

- [ ] **Step 2: Implement panadapter overlay paint**

Find the existing paint loop in `SpectrumWidget.cpp`. Add a TX filter draw step that runs after the spectrum trace but before the waterfall:

```cpp
// Draw TX filter overlay band — per §E1.D9.
// Panadapter overlay always-on (RX or TX); waterfall column MOX-gated.
if (m_txFilterVisible) {  // existing flag from §B8 inventory
    drawTxFilterOverlay(p, specRect);
}
```

Implement `drawTxFilterOverlay`:
```cpp
void SpectrumWidget::drawTxFilterOverlay(QPainter& p, const QRect& specRect)
{
    // Map audio Hz to RF offset based on current DSP mode.
    auto mode = m_slice ? m_slice->dspMode() : DSPMode::USB;
    double offsetLowHz = 0.0;
    double offsetHighHz = 0.0;
    auto isLsb = [](DSPMode m) { return m == DSPMode::LSB || m == DSPMode::DIGL || m == DSPMode::CWL; };
    auto isSymmetric = [](DSPMode m) { return m == DSPMode::AM || m == DSPMode::SAM || m == DSPMode::DSB || m == DSPMode::FM; };

    if (isLsb(mode)) {
        offsetLowHz = -m_txFilterHigh;
        offsetHighHz = -m_txFilterLow;
    } else if (isSymmetric(mode)) {
        offsetLowHz = -m_txFilterHigh;
        offsetHighHz = +m_txFilterHigh;
    } else {  // USB family
        offsetLowHz = m_txFilterLow;
        offsetHighHz = m_txFilterHigh;
    }

    // Convert RF-offset Hz to pixel x using existing spectrum-mapping utility.
    // (Use whatever existing helper is on SpectrumWidget — likely
    // freqToPanX(double hzOffset) or similar.)
    int xLow  = freqToPanX(offsetLowHz, specRect);
    int xHigh = freqToPanX(offsetHighHz, specRect);

    QRect bandRect(xLow, specRect.top(), xHigh - xLow, specRect.height());
    p.fillRect(bandRect, m_txFilterColor);

    // Border
    p.setPen(QPen(QColor(0xff, 0x78, 0x33), 2));  // kTxFilterOverlayBorder
    p.drawLine(xLow, specRect.top(), xLow, specRect.bottom());
    p.drawLine(xHigh, specRect.top(), xHigh, specRect.bottom());

    // Label inside the band
    p.setPen(QColor(0xff, 0xaa, 0x70));  // kTxFilterOverlayLabel
    p.drawText(bandRect.adjusted(8, 4, -8, -4), Qt::AlignTop | Qt::AlignLeft,
        QStringLiteral("TX %1-%2 Hz").arg(m_txFilterLow).arg(m_txFilterHigh));
}
```

- [ ] **Step 3: Implement waterfall TX filter column paint (MOX-gated)**

Add a similar method for the waterfall, but only when `m_showTxFilterOnRxWaterfall` is true AND MOX is engaged (from the existing flag — this requires MainWindow to set a `m_moxOn` flag on SpectrumWidget when MoxController emits state changes).

Add to `SpectrumWidget.h`:
```cpp
public:
    void setMoxOn(bool on) { m_moxOn = on; update(); }
private:
    bool m_moxOn{false};
```

In the paint loop:
```cpp
if (m_showTxFilterOnRxWaterfall && m_moxOn) {
    drawTxFilterWaterfallColumn(p, waterfallRect);
}
```

- [ ] **Step 4: Wire MainWindow signals**

```cpp
// In MainWindow::buildUI or equivalent:
connect(m_radioModel->transmitModel(), &TransmitModel::filterChanged,
        m_spectrumWidget, &SpectrumWidget::setTxFilterRange);
connect(m_radioModel->moxController(), &MoxController::moxStateChanged,
        m_spectrumWidget, &SpectrumWidget::setMoxOn);

// Initial sync:
auto* tx = m_radioModel->transmitModel();
m_spectrumWidget->setTxFilterRange(tx->filterLow(), tx->filterHigh());
```

- [ ] **Step 5: Build + bench check**

```bash
cmake --build build -j$(nproc) --target NereusSDR
pkill -f NereusSDR 2>/dev/null || true
./build/NereusSDR &
sleep 3
```

With ANAN-G2 connected, USB mode at 14.200 MHz:
- Verify panadapter shows translucent orange band at +100 to +2900 Hz off VFO.
- Engage MOX → waterfall shows orange vertical column.
- Disengage MOX → column disappears (panadapter overlay stays).
- Switch to LSB → band flips to -2900 to -100.
- Switch to AM → band becomes symmetric -2900 to +2900.

- [ ] **Step 6: Commit**

```bash
git add src/gui/SpectrumWidget.{h,cpp} src/gui/MainWindow.cpp tests/tst_spectrum_tx_overlay.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
ui(spectrum-widget): paint TX filter overlay (panadapter always, waterfall MOX-gated)

Per docs/architecture/ui-audit-polish-plan.md §E1.D9.

setTxFilterRange(low, high) updates m_txFilterLow/High and triggers
repaint. Paint code maps audio Hz to RF offset via existing freqToPanX
helper, applying per-mode rules (USB family +low..+high, LSB family
-high..-low, symmetric -high..+high). Translucent orange fill +
solid orange borders + inline label.

Waterfall column gated by m_showTxFilterOnRxWaterfall (existing flag)
AND m_moxOn (new — set by MainWindow on MoxController::moxStateChanged).

Connected in MainWindow:
- TransmitModel::filterChanged → SpectrumWidget::setTxFilterRange
- MoxController::moxStateChanged → SpectrumWidget::setMoxOn

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 9: D9b — Add TX/RX filter overlay color customisation on Setup → Display

**Files:**
- Modify: `src/gui/SpectrumWidget.{h,cpp}` (add setTxFilterColor/setRxFilterColor)
- Modify: `src/gui/setup/DisplaySetupPages.cpp` (add ColorSwatchButton on TX Display + Spectrum Defaults pages)

- [ ] **Step 1: Add setters + persistence to SpectrumWidget**

```cpp
public:
    void setTxFilterColor(const QColor& c);
    QColor txFilterColor() const { return m_txFilterColor; }

    void setRxFilterColor(const QColor& c);
    QColor rxFilterColor() const { return m_rxFilterColor; }

private:
    QColor m_rxFilterColor{0x00, 0xb4, 0xd8, 80};  // kRxFilterOverlayFill default
```

In `setTxFilterColor` / `setRxFilterColor`: update member, emit a signal (e.g. `txFilterColorChanged`), call `update()`, save via existing `scheduleSettingsSave()` mechanism.

Add AppSettings keys: `display/txFilterColor` (`"#RRGGBBAA"`) and `display/rxFilterColor`. Load on construction.

Update the panadapter paint to use `m_txFilterColor` / `m_rxFilterColor` (and split out the border color from the alpha-zero variant — derive border from solid version of fill color).

- [ ] **Step 2: Add ColorSwatchButton on TX Display setup page**

In `DisplaySetupPages.cpp`, find the TX Display page constructor. Add a "TX Filter Overlay" group with a ColorSwatchButton:

```cpp
auto* txFilterGroup = new QGroupBox(QStringLiteral("TX Filter Overlay"), this);
auto* fl = new QFormLayout(txFilterGroup);

QColor initial = ColorSwatchButton::colorFromHex(
    AppSettings::instance().value("display/txFilterColor",
                                   "#FF7838BC").toString());
auto* txColorBtn = new ColorSwatchButton(initial, txFilterGroup);
fl->addRow(QStringLiteral("Color &amp; opacity"), txColorBtn);

connect(txColorBtn, &ColorSwatchButton::colorChanged, this,
        [this](const QColor& c) {
    AppSettings::instance().setValue("display/txFilterColor",
        ColorSwatchButton::colorToHex(c));
    if (auto* sw = m_radioModel->spectrumWidget()) {
        sw->setTxFilterColor(c);
    }
});

contentLayout()->addWidget(txFilterGroup);
```

- [ ] **Step 3: Add ColorSwatchButton on Spectrum Defaults page (RX filter)**

Same pattern, with `display/rxFilterColor` and `setRxFilterColor`.

- [ ] **Step 4: Build + visual**

Open Setup → Display → TX Display. New "TX Filter Overlay" section with color picker. Pick magenta with 50% alpha → spectrum panadapter overlay turns magenta translucent. Restart app → magenta persists.

Same for RX (Setup → Display → Spectrum Defaults).

- [ ] **Step 5: Commit**

```bash
git add src/gui/SpectrumWidget.{h,cpp} src/gui/setup/DisplaySetupPages.cpp
git commit -m "$(cat <<'EOF'
ui(spectrum,display-setup): user-customisable TX/RX filter overlay colors

Per docs/architecture/ui-audit-polish-plan.md §E1.D9b. SpectrumWidget
gains setTxFilterColor/setRxFilterColor + persistence under AppSettings
keys display/txFilterColor and display/rxFilterColor (\"#RRGGBBAA\").

Setup → Display → TX Display gains a \"TX Filter Overlay\" group with
ColorSwatchButton. Setup → Display → Spectrum Defaults gains an \"RX
Filter Overlay\" group with another ColorSwatchButton. Both use
QColorDialog with alpha enabled (existing ColorSwatchButton pattern).

Defaults match Thetis convention (TX orange ~18%, RX cyan ~31%).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 10: D9c-1 — Split zero-line color: m_rxZeroLineColor + m_txZeroLineColor

**Files:**
- Modify: `src/gui/SpectrumWidget.{h,cpp}`
- Modify: `src/gui/setup/DisplaySetupPages.cpp` (add 2 ColorSwatchButtons)

- [ ] **Step 1: Replace m_zeroLineColor with split**

In `SpectrumWidget.h:786`, replace:
```cpp
QColor m_zeroLineColor{255, 0, 0};
```
with:
```cpp
QColor m_rxZeroLineColor{255, 0, 0};                // red default (Thetis)
QColor m_txZeroLineColor{255, 184, 0};              // amber default (NereusSDR-original to distinguish)
```

Add setters + persistence keys `display/rxZeroLineColor`, `display/txZeroLineColor`.

Update the paint code that draws zero lines — use `m_rxZeroLineColor` for the RX zero line and `m_txZeroLineColor` for the TX zero line.

- [ ] **Step 2: Add 2 ColorSwatchButtons on Spectrum Defaults page**

In `DisplaySetupPages.cpp` Spectrum Defaults page, add two more rows in a new "Zero Line Colors" group:
- "RX Zero Line" ColorSwatchButton bound to `display/rxZeroLineColor`
- "TX Zero Line" ColorSwatchButton bound to `display/txZeroLineColor`

- [ ] **Step 3: Build + visual**

Engage MOX → TX zero line shows in amber. RX zero line stays in red. Both customisable via Setup → Display → Spectrum Defaults.

- [ ] **Step 4: Commit**

```bash
git add src/gui/SpectrumWidget.{h,cpp} src/gui/setup/DisplaySetupPages.cpp
git commit -m "ui(spectrum,display-setup): split zero-line color into RX and TX

Per docs/architecture/ui-audit-polish-plan.md §E1.D9c-1. RX defaults to
red (Thetis); TX defaults to amber (NereusSDR-original to distinguish
during split / X-IT operation).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 11: D9c-3 — Add "Reset all display colors to defaults" button

**Files:**
- Modify: `src/gui/SpectrumWidget.{h,cpp}` (add resetDisplayColorsToDefaults method)
- Modify: `src/gui/setup/DisplaySetupPages.cpp` (add button + connect)

- [ ] **Step 1: Add the reset method to SpectrumWidget**

```cpp
void SpectrumWidget::resetDisplayColorsToDefaults()
{
    setTxFilterColor(QColor(0xff, 0x78, 0x38, 0xbc));  // ~46/255 alpha
    setRxFilterColor(QColor(0x00, 0xb4, 0xd8, 0x50));
    m_rxZeroLineColor = QColor(255, 0, 0);
    m_txZeroLineColor = QColor(255, 184, 0);
    // Plus other display colors that exist (data line, fill, low color, grid colors, text colors, etc.)
    // Reset each to its compile-time default.
    update();
    scheduleSettingsSave();
}
```

- [ ] **Step 2: Add the button on Setup → Display → Spectrum Defaults**

```cpp
auto* resetBtn = new QPushButton(QStringLiteral("Reset all display colors to defaults"), this);
resetBtn->setStyleSheet(NereusSDR::Style::kButtonStyle);
contentLayout()->addWidget(resetBtn);

connect(resetBtn, &QPushButton::clicked, this, [this]() {
    QMessageBox::StandardButton res = QMessageBox::question(this,
        QStringLiteral("Reset display colors"),
        QStringLiteral("Reset all spectrum/waterfall display colors to factory defaults?"),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (res == QMessageBox::Yes) {
        if (auto* sw = m_radioModel->spectrumWidget()) {
            sw->resetDisplayColorsToDefaults();
        }
    }
});
```

- [ ] **Step 3: Build + test**

Customise several colors. Click "Reset all display colors". Confirm dialog. After confirm, all colors return to factory defaults.

- [ ] **Step 4: Commit**

```bash
git commit -m "ui(spectrum,display-setup): add 'Reset all display colors to defaults' button

Per docs/architecture/ui-audit-polish-plan.md §E1.D9c-3.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 12: D9c-4 — Reserve TNF + SubRX color scaffolding

**Files:**
- Modify: `src/gui/SpectrumWidget.{h,cpp}`

- [ ] **Step 1: Add member fields + setters (no paint yet)**

```cpp
public:
    void setTnfFilterColor(const QColor& c);
    QColor tnfFilterColor() const { return m_tnfFilterColor; }
    void setSubRxFilterColor(const QColor& c);
    QColor subRxFilterColor() const { return m_subRxFilterColor; }
private:
    QColor m_tnfFilterColor   {255, 80, 80, 80};  // red translucent placeholder
    QColor m_subRxFilterColor {180, 0, 220, 80};  // purple translucent placeholder
```

Add AppSettings keys `display/tnfFilterColor` and `display/subRxFilterColor`. Load on construction. NO setup-page UI yet — these come with the respective feature phases (TNF and 3F multi-RX).

- [ ] **Step 2: Document in code why these are reserved**

Add a comment block:
```cpp
// Per docs/architecture/ui-audit-polish-plan.md §E1.D9c-4 — forward-compat
// scaffolding. The members + setters + AppSettings keys are reserved
// now so user customisation persists across the version that adds the
// feature. Setup-page UI lands when:
// - TNF filter ships (Tracking Notch Filter feature, no plan yet)
// - SubRX ships (3F multi-panadapter / multi-RX phase)
// No paint code yet — these colors are unused until their features ship.
```

- [ ] **Step 3: Commit**

```bash
git add src/gui/SpectrumWidget.{h,cpp}
git commit -m "ui(spectrum-widget): reserve TNF + SubRX filter color scaffolding (D9c-4)

Members + setters + AppSettings keys for future TNF and SubRX filter
overlay colors. No setup-page UI yet — those land with their respective
feature phases. This forward-compat ensures user customisation persists
across the version that adds each feature.

Per docs/architecture/ui-audit-polish-plan.md §E1.D9c-4.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 13: Final verification + bench checks

**Files:**
- None modified

- [ ] **Step 1: Full build + tests**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -10
ctest --test-dir build --output-on-failure 2>&1 | tail -10
```

- [ ] **Step 2: Bench verification (REQUIRED — needs ANAN-G2)**

With radio connected:
1. Activate "DX SSB" profile → spinboxes show 1710/2710 (D4 verification).
2. Sweep High spinbox 2710 → 3500 → 1500 → 2710. Spectrum overlay band moves live (within ~5 ms, debounced). Status text updates.
3. Switch to "ESSB Wide" profile → spinboxes restore to 50/3650; band widens.
4. Manually edit spinbox; switch profile → modal "Profile has unsaved changes" should appear if Plan 5 (E2) is also landed; otherwise just gets discarded silently with current plan.
5. Engage MOX → waterfall column appears. Disengage → disappears.
6. Customise TX filter color (Setup → Display → TX Display → ColorSwatchButton) to bright magenta with 50% alpha. Verify panadapter band updates.
7. Restart app → all state persists.

- [ ] **Step 3: Plan summary**

Plan 4 complete. ~12 commits land:
- D1 (Task 2): TransmitModel filter properties + MicProfile schema
- D4 (Task 3): 20 factory profile filter values from Thetis
- D2/D3 (Task 4): TX BW spinbox cluster on TxApplet
- D6 (Task 5): TX Filter group on TxProfileSetupPage
- D8 (Task 6): TxChannel reads filter from model with debounce
- D9 (Tasks 7, 8): Palette additions + panadapter/waterfall paint
- D9b (Task 9): Color customisation on TX Display + Spectrum Defaults
- D9c-1 (Task 10): Split zero-line color
- D9c-3 (Task 11): Reset display colors button
- D9c-4 (Task 12): TNF + SubRX scaffolding

Net: ~590 lines added, ~10 lines removed (hardcoded mode-based bandpass).

---

## Self-Review

**Spec coverage:**
- D1 (per-profile storage) — Task 2 ✓
- D2 (spinbox idiom) — Task 4 ✓
- D3 (placement after Profile combo) — Task 4 ✓
- D4 (Thetis factory values) — Task 3 ✓
- D5 (per-profile persistence) — Task 2 + 3 ✓
- D6 (TxProfileSetupPage entry) — Task 5 ✓
- D7 (range limits) — Task 4 (spinbox ranges) + Task 2 (model validation) ✓
- D8 (live update with debounce) — Task 6 ✓
- D9 (status text + overlays) — Tasks 4 (status), 7 (palette), 8 (paint) ✓
- D9b (color customization) — Task 9 ✓
- D9c-1 (zero-line split) — Task 10 ✓
- D9c-3 (reset button) — Task 11 ✓
- D9c-4 (TNF/SubRX scaffolding) — Task 12 ✓

**Placeholder scan:** none.

**Type consistency:** `TransmitModel::filterLow/filterHigh/filterChanged/filterDisplayText` consistent across Tasks 2, 4, 5, 6, 8. `SpectrumWidget::setTxFilterRange/setMoxOn/setTxFilterColor/setRxFilterColor/resetDisplayColorsToDefaults` consistent across Tasks 8, 9, 10, 11.

**Dependencies:** This plan depends on Plan 1 (palette helpers in StyleConstants.h). Independent of Plans 2 + 3. Plan 5 (Profile-awareness) depends on the FilterLow/FilterHigh schema additions from Tasks 2+3 of this plan.
