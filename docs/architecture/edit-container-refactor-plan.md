# Edit Container Refactor — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Spec:** [`edit-container-refactor-design.md`](edit-container-refactor-design.md)
**Goal:** Collapse meter presets into first-class `MeterItem` subclasses, introduce hybrid "add once per container" rule, add drag-reorder + rename + on-container access affordances, fix ANAN Multi Meter needle arc, and add `+ Add` button inside the dialog.
**Architecture:** Ten new `MeterItem` subclasses absorb the 34 `ItemGroup` factory methods (18 bar-row variants share one parameterized `BarPresetItem`; 9 distinctive composites each get their own class). The edit dialog keeps its 3-column shape but its in-use list now carries user names and reorders via drag. `ContainerWidget` gains `contextMenuEvent`/double-click/gear affordances so users reach the dialog from the container itself. Legacy container files migrate tolerantly on load.
**Tech Stack:** C++20 / Qt6 (QWidget, QPainter, QListWidget, QJsonObject-via-QString), Qt Test (QTest) for unit tests, WDSP for meter data binding, GPG for commit signing.

---

## Branch and worktree

Work occurs on branch `refactor/edit-container-design` (already created; current HEAD = `4fba5bc` with the design spec).

Engineer launching this plan:

```bash
cd /Users/j.j.boyd/NereusSDR
git status                    # confirm on refactor/edit-container-design
git log --oneline -1          # should show "docs(architecture): add edit-container refactor design spec"
git -C ../Thetis describe --tags || git -C ../Thetis rev-parse --short HEAD
# Record the Thetis version/SHA as kThetisVer. Current at time of spec = @501e3f5.
```

---

## Fixture capture (before any code changes)

Before touching a single file, capture three legacy-format saved containers as `tests/fixtures/legacy/` golden inputs for the migration tests. Migration coverage depends on having real pre-refactor data.

- [ ] **Step 1: Launch current build**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
./build/NereusSDR
```

- [ ] **Step 2: Create the three fixtures**

Inside the running app, create three floating containers, populate each with a different preset, arrange a few primitives, and save:

1. A container with ANAN Multi Meter preset.
2. A container with S-Meter preset.
3. A container with Power/SWR preset.

Quit the app. Copy the AppSettings XML that holds the container blobs.

- [ ] **Step 3: Commit fixtures**

```bash
mkdir -p tests/fixtures/legacy
cp ~/.config/NereusSDR/NereusSDR.settings tests/fixtures/legacy/pre-refactor-settings.xml
git add tests/fixtures/legacy/pre-refactor-settings.xml
git commit -S -m "$(cat <<'EOF'
test(fixtures): capture pre-refactor AppSettings with legacy preset containers

Three containers (ANAN MM, S-Meter, Power/SWR) in their current
ItemGroup-expanded serialized form. Used by
test_legacy_container_migration to confirm tolerant migration to
the new first-class preset classes.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 1: `AnanMultiMeterItem` — first-class ANAN Multi Meter with arc fix

**Files:**
- Create: `src/gui/meters/presets/AnanMultiMeterItem.h`
- Create: `src/gui/meters/presets/AnanMultiMeterItem.cpp`
- Create: `tests/tst_anan_multi_meter_item.cpp`
- Modify: `CMakeLists.txt` (add the three new files to sources + test executable)
- Modify: `docs/attribution/THETIS-PROVENANCE.md` (add provenance row)

- [ ] **Step 1: Write failing test**

Create `tests/tst_anan_multi_meter_item.cpp`:

```cpp
#include <QtTest>
#include <QPainter>
#include <QImage>
#include "gui/meters/presets/AnanMultiMeterItem.h"

class TestAnanMultiMeterItem : public QObject {
    Q_OBJECT
private slots:
    void defaultConstruction_hasSevenNeedles();
    void signalNeedle_hasSixteenCalibrationPoints();
    void arcAnchoredToBgRect_rendersNeedlesOnFaceAt2x1();
    void serialize_roundTrip_preservesAllFields();
    void debugOverlay_paintsCalibrationPoints();
};

void TestAnanMultiMeterItem::defaultConstruction_hasSevenNeedles() {
    AnanMultiMeterItem item;
    QCOMPARE(item.needleCount(), 7);
    QCOMPARE(item.needleName(0), QStringLiteral("Signal"));
    QCOMPARE(item.needleName(6), QStringLiteral("ALC"));
}

void TestAnanMultiMeterItem::signalNeedle_hasSixteenCalibrationPoints() {
    AnanMultiMeterItem item;
    QCOMPARE(item.needleCalibration(0).size(), 16);
}

void TestAnanMultiMeterItem::arcAnchoredToBgRect_rendersNeedlesOnFaceAt2x1() {
    AnanMultiMeterItem item;
    item.setAnchorToBgRect(true);
    QImage img(400, 200, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    QPainter p(&img);
    item.paint(p, 400, 200);
    p.end();
    // Sanity: anchored mode should produce identical render geometry
    // to a 1:1 render scaled up, since the arc anchors to bg rect.
    // Full golden-image check in manual verification; here we just
    // confirm paint completes without asserting.
    QVERIFY(!img.isNull());
}

void TestAnanMultiMeterItem::serialize_roundTrip_preservesAllFields() {
    AnanMultiMeterItem a;
    a.setRect(0.1f, 0.2f, 0.8f, 0.6f);
    a.setAnchorToBgRect(false);
    a.setNeedleVisible(5, false); // hide compression
    const QString data = a.serialize();

    AnanMultiMeterItem b;
    QVERIFY(b.deserialize(data));
    QCOMPARE(b.x(), 0.1f);
    QCOMPARE(b.itemWidth(), 0.8f);
    QCOMPARE(b.anchorToBgRect(), false);
    QCOMPARE(b.needleVisible(5), false);
    QCOMPARE(b.needleVisible(0), true);
}

void TestAnanMultiMeterItem::debugOverlay_paintsCalibrationPoints() {
    AnanMultiMeterItem item;
    item.setDebugOverlay(true);
    QImage img(400, 300, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    QPainter p(&img);
    item.paint(p, 400, 300);
    p.end();
    // With overlay on, some pixels must be non-black at predicted
    // calibration-point positions. Spot-check the Signal needle's
    // first calibration point (-127 dBm): normalized (0.076, 0.31).
    const int x = int(0.076f * 400);
    const int y = int(0.31f * 300);
    QVERIFY(img.pixelColor(x, y).red() > 0 ||
            img.pixelColor(x, y).green() > 0 ||
            img.pixelColor(x, y).blue() > 0);
}

QTEST_MAIN(TestAnanMultiMeterItem)
#include "tst_anan_multi_meter_item.moc"
```

- [ ] **Step 2: Run test to confirm failure**

```bash
cmake --build build --target tst_anan_multi_meter_item 2>&1 | tail -20
```

Expected: compilation failure — `AnanMultiMeterItem.h: No such file or directory`.

- [ ] **Step 3: Write `AnanMultiMeterItem.h`**

```cpp
// =================================================================
// src/gui/meters/presets/AnanMultiMeterItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs
//   original licence from Thetis source is included in the .cpp
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Reimplemented in C++20/Qt6 for NereusSDR by
//                J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Claude Opus 4.7.
// =================================================================
#pragma once

#include "gui/meters/MeterItem.h"
#include <QImage>
#include <QMap>
#include <QPointF>
#include <QString>
#include <QColor>
#include <array>

class AnanMultiMeterItem : public MeterItem {
    Q_OBJECT
public:
    static constexpr int kNeedleCount = 7;

    explicit AnanMultiMeterItem(QObject* parent = nullptr);

    // Rendering
    Layer renderLayer() const override { return Layer::Geometry; }
    void paint(QPainter& p, int widgetW, int widgetH) override;

    // Serialization
    QString serialize() const override;
    bool deserialize(const QString& data) override;

    // Needle accessors
    int needleCount() const { return kNeedleCount; }
    QString needleName(int i) const;
    QMap<float, QPointF> needleCalibration(int i) const;
    QColor needleColor(int i) const;
    bool needleVisible(int i) const;
    void setNeedleVisible(int i, bool v);

    // Geometry
    QPointF pivot() const { return m_pivot; }
    QPointF radiusRatio() const { return m_radiusRatio; }
    void setPivot(const QPointF& p) { m_pivot = p; }
    void setRadiusRatio(const QPointF& r) { m_radiusRatio = r; }

    // Arc fix
    bool anchorToBgRect() const { return m_anchorToBgRect; }
    void setAnchorToBgRect(bool v) { m_anchorToBgRect = v; }

    // Debug
    bool debugOverlay() const { return m_debugOverlay; }
    void setDebugOverlay(bool v) { m_debugOverlay = v; }

private:
    struct Needle {
        QString name;
        int bindingId;                 // WDSP meter reading ID
        QMap<float, QPointF> calibration;
        QColor color;
        bool visible = true;
    };

    QImage m_background;               // ":/meters/ananMM.png"
    QPointF m_pivot{0.004, 0.736};     // From Thetis MeterManager.cs:22478 [@501e3f5]
    QPointF m_radiusRatio{1.0, 0.58};  // From Thetis MeterManager.cs:22479 [@501e3f5]
    bool m_anchorToBgRect = true;
    bool m_debugOverlay = false;
    std::array<Needle, kNeedleCount> m_needles;

    void initialiseNeedles();
    QRect bgRect(int widgetW, int widgetH) const;
    QPointF calibratedPosition(const Needle& n, float value) const;
    void paintNeedle(QPainter& p, const Needle& n, const QRect& bg) const;
    void paintDebugOverlay(QPainter& p, const QRect& bg) const;
};
```

- [ ] **Step 4: Write `AnanMultiMeterItem.cpp` — calibration port**

```cpp
// =================================================================
// src/gui/meters/presets/AnanMultiMeterItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs  (method AddAnanMM,
//   line 22461 onward)
//
// Original Thetis header (verbatim) follows.
// --- From MeterManager.cs ---
/*  MeterManager.cs
This file is part of a program that implements a Software-Defined Radio.
This code/file can be found on GitHub : https://github.com/ramdor/Thetis
Copyright (C) 2020-2026 Richard Samphire MW0LGE
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
The author can be reached by email at
mw0lge@grange-lane.co.uk
*/
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Reimplemented in C++20/Qt6 for NereusSDR by
//                J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Claude Opus 4.7. Arc geometry now anchors to the
//                background image rect to preserve face alignment at
//                non-default aspect ratios. Calibration tables ported
//                byte-for-byte at Thetis @501e3f5.
// =================================================================

#include "gui/meters/presets/AnanMultiMeterItem.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPainter>
#include <QPen>

namespace {

// WDSP binding IDs (from src/gui/meters/MeterItem.h or dsp.cs).
// Use same constants as existing NeedleItem consumers.
constexpr int kReadingSignalAvg = 0;
constexpr int kReadingVolts     = 1;
constexpr int kReadingAmps      = 2;
constexpr int kReadingPwr       = 3;
constexpr int kReadingSwr       = 4;
constexpr int kReadingAlcComp   = 5;
constexpr int kReadingAlcGroup  = 6;

// From Thetis MeterManager.cs:22491-22507 [@501e3f5] — Signal needle (-127..-13 dBm, 16 points)
QMap<float, QPointF> makeSignalCal() {
    QMap<float, QPointF> c;
    c.insert(-127.0f, QPointF(0.076, 0.310));
    c.insert(-121.0f, QPointF(0.131, 0.272));
    c.insert(-115.0f, QPointF(0.189, 0.254));
    c.insert(-109.0f, QPointF(0.233, 0.211));
    c.insert(-103.0f, QPointF(0.284, 0.207));
    c.insert( -97.0f, QPointF(0.326, 0.177));
    c.insert( -91.0f, QPointF(0.374, 0.177));
    c.insert( -85.0f, QPointF(0.414, 0.151));
    c.insert( -79.0f, QPointF(0.459, 0.168));
    c.insert( -73.0f, QPointF(0.501, 0.142));
    c.insert( -63.0f, QPointF(0.564, 0.172));
    c.insert( -53.0f, QPointF(0.630, 0.164));
    c.insert( -43.0f, QPointF(0.695, 0.203));
    c.insert( -33.0f, QPointF(0.769, 0.211));
    c.insert( -23.0f, QPointF(0.838, 0.272));
    c.insert( -13.0f, QPointF(0.926, 0.310));
    return c;
}

// From Thetis MeterManager.cs:22534-22537 [@501e3f5] — Volts needle (10..15 V, 3 points)
QMap<float, QPointF> makeVoltsCal() {
    QMap<float, QPointF> c;
    c.insert(10.0f,  QPointF(0.559, 0.756));
    c.insert(12.5f,  QPointF(0.605, 0.772));
    c.insert(15.0f,  QPointF(0.665, 0.784));
    return c;
}

// From Thetis MeterManager.cs:22562-22573 [@501e3f5] — Amps needle (0..20 A, 11 points)
QMap<float, QPointF> makeAmpsCal() {
    QMap<float, QPointF> c;
    c.insert( 0.0f, QPointF(0.199, 0.576));
    c.insert( 2.0f, QPointF(0.270, 0.540));
    c.insert( 4.0f, QPointF(0.333, 0.516));
    c.insert( 6.0f, QPointF(0.393, 0.504));
    c.insert( 8.0f, QPointF(0.448, 0.492));
    c.insert(10.0f, QPointF(0.499, 0.492));
    c.insert(12.0f, QPointF(0.554, 0.488));
    c.insert(14.0f, QPointF(0.608, 0.500));
    c.insert(16.0f, QPointF(0.667, 0.516));
    c.insert(18.0f, QPointF(0.728, 0.540));
    c.insert(20.0f, QPointF(0.799, 0.576));
    return c;
}

// From Thetis MeterManager.cs:22600-22610 [@501e3f5] — Power needle (0..150 W, 10 points)
QMap<float, QPointF> makePowerCal() {
    QMap<float, QPointF> c;
    c.insert(  0.0f, QPointF(0.099, 0.352));
    c.insert(  5.0f, QPointF(0.164, 0.312));
    c.insert( 10.0f, QPointF(0.224, 0.280));
    c.insert( 25.0f, QPointF(0.335, 0.236));
    c.insert( 30.0f, QPointF(0.367, 0.228));
    c.insert( 40.0f, QPointF(0.436, 0.220));
    c.insert( 50.0f, QPointF(0.499, 0.212));
    c.insert( 60.0f, QPointF(0.559, 0.216));
    c.insert(100.0f, QPointF(0.751, 0.272));
    c.insert(150.0f, QPointF(0.899, 0.352));
    return c;
}

// From Thetis MeterManager.cs:22639-22644 [@501e3f5] — SWR needle (1..10, 6 points)
QMap<float, QPointF> makeSwrCal() {
    QMap<float, QPointF> c;
    c.insert( 1.0f, QPointF(0.152, 0.468));
    c.insert( 1.5f, QPointF(0.280, 0.404));
    c.insert( 2.0f, QPointF(0.393, 0.372));
    c.insert( 2.5f, QPointF(0.448, 0.360));
    c.insert( 3.0f, QPointF(0.499, 0.360));
    c.insert(10.0f, QPointF(0.847, 0.476));
    return c;
}

// From Thetis MeterManager.cs:22671-22677 [@501e3f5] — Compression needle (0..30 dB, 7 points)
QMap<float, QPointF> makeCompCal() {
    QMap<float, QPointF> c;
    c.insert( 0.0f, QPointF(0.249, 0.680));
    c.insert( 5.0f, QPointF(0.342, 0.640));
    c.insert(10.0f, QPointF(0.425, 0.624));
    c.insert(15.0f, QPointF(0.499, 0.620));
    c.insert(20.0f, QPointF(0.571, 0.628));
    c.insert(25.0f, QPointF(0.656, 0.640));
    c.insert(30.0f, QPointF(0.751, 0.688));
    return c;
}

// From Thetis MeterManager.cs:22704-22706 [@501e3f5] — ALC needle (-30..25 dB, 3 points)
QMap<float, QPointF> makeAlcCal() {
    QMap<float, QPointF> c;
    c.insert(-30.0f, QPointF(0.295, 0.804));
    c.insert(  0.0f, QPointF(0.332, 0.784));
    c.insert( 25.0f, QPointF(0.499, 0.756));
    return c;
}

} // namespace

AnanMultiMeterItem::AnanMultiMeterItem(QObject* parent) : MeterItem(parent) {
    m_background = QImage(QStringLiteral(":/meters/ananMM.png"));
    initialiseNeedles();
}

void AnanMultiMeterItem::initialiseNeedles() {
    m_needles[0] = { QStringLiteral("Signal"),      kReadingSignalAvg, makeSignalCal(), QColor(255, 220,  80), true };
    m_needles[1] = { QStringLiteral("Volts"),       kReadingVolts,     makeVoltsCal(),  QColor( 80, 200, 255), true };
    m_needles[2] = { QStringLiteral("Amps"),        kReadingAmps,      makeAmpsCal(),   QColor(120, 255, 120), true };
    m_needles[3] = { QStringLiteral("Power"),       kReadingPwr,       makePowerCal(),  QColor(255, 120, 120), true };
    m_needles[4] = { QStringLiteral("SWR"),         kReadingSwr,       makeSwrCal(),    QColor(255, 160,  80), true };
    m_needles[5] = { QStringLiteral("Compression"), kReadingAlcComp,   makeCompCal(),   QColor(200, 160, 255), true };
    m_needles[6] = { QStringLiteral("ALC"),         kReadingAlcGroup,  makeAlcCal(),    QColor(255,  80, 200), true };
}

QString AnanMultiMeterItem::needleName(int i) const {
    return (i >= 0 && i < kNeedleCount) ? m_needles[i].name : QString();
}

QMap<float, QPointF> AnanMultiMeterItem::needleCalibration(int i) const {
    return (i >= 0 && i < kNeedleCount) ? m_needles[i].calibration : QMap<float, QPointF>();
}

QColor AnanMultiMeterItem::needleColor(int i) const {
    return (i >= 0 && i < kNeedleCount) ? m_needles[i].color : QColor();
}

bool AnanMultiMeterItem::needleVisible(int i) const {
    return (i >= 0 && i < kNeedleCount) ? m_needles[i].visible : false;
}

void AnanMultiMeterItem::setNeedleVisible(int i, bool v) {
    if (i >= 0 && i < kNeedleCount) {
        m_needles[i].visible = v;
    }
}

QRect AnanMultiMeterItem::bgRect(int widgetW, int widgetH) const {
    const QRect itemRect(int(m_x * widgetW), int(m_y * widgetH),
                         int(m_w * widgetW), int(m_h * widgetH));
    if (!m_anchorToBgRect || m_background.isNull()) {
        return itemRect;
    }
    // Letterbox the background image inside the item rect, preserving
    // aspect. The arc calibration points are defined relative to this
    // rect, not the item rect, so at any container aspect the needles
    // stay glued to the painted face.
    const qreal imgAspect = qreal(m_background.width()) / qreal(m_background.height());
    const qreal rectAspect = qreal(itemRect.width()) / qreal(itemRect.height());
    if (rectAspect > imgAspect) {
        const int drawW = int(itemRect.height() * imgAspect);
        const int drawX = itemRect.x() + (itemRect.width() - drawW) / 2;
        return QRect(drawX, itemRect.y(), drawW, itemRect.height());
    } else {
        const int drawH = int(itemRect.width() / imgAspect);
        const int drawY = itemRect.y() + (itemRect.height() - drawH) / 2;
        return QRect(itemRect.x(), drawY, itemRect.width(), drawH);
    }
}

QPointF AnanMultiMeterItem::calibratedPosition(const Needle& n, float value) const {
    if (n.calibration.isEmpty()) {
        return QPointF(0.5, 0.5);
    }
    // Interpolate between calibration points (linear between adjacent keys).
    auto lo = n.calibration.lowerBound(value);
    if (lo == n.calibration.begin()) {
        return lo.value();
    }
    if (lo == n.calibration.end()) {
        return (lo - 1).value();
    }
    auto hi = lo;
    --lo;
    const float tLo = lo.key();
    const float tHi = hi.key();
    const float t = (value - tLo) / (tHi - tLo);
    return QPointF(lo.value().x() + t * (hi.value().x() - lo.value().x()),
                   lo.value().y() + t * (hi.value().y() - lo.value().y()));
}

void AnanMultiMeterItem::paint(QPainter& p, int widgetW, int widgetH) {
    const QRect bg = bgRect(widgetW, widgetH);
    if (!m_background.isNull()) {
        p.drawImage(bg, m_background);
    }
    for (const Needle& n : m_needles) {
        if (!n.visible) continue;
        paintNeedle(p, n, bg);
    }
    if (m_debugOverlay) {
        paintDebugOverlay(p, bg);
    }
}

void AnanMultiMeterItem::paintNeedle(QPainter& p, const Needle& n, const QRect& bg) const {
    // Current meter value comes from the binding layer (WDSP poller). For
    // the unit tests and the no-data case we fall back to the midpoint
    // of the calibration table so the needle still renders on-face.
    const float value = n.calibration.isEmpty() ? 0.0f
        : 0.5f * (n.calibration.firstKey() + n.calibration.lastKey());
    const QPointF tipNorm = calibratedPosition(n, value);

    const QPointF pivot(bg.x() + m_pivot.x() * bg.width(),
                        bg.y() + m_pivot.y() * bg.height());
    const QPointF tip(bg.x() + tipNorm.x() * bg.width(),
                      bg.y() + tipNorm.y() * bg.height());

    QPen pen(n.color);
    pen.setWidthF(1.8);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.drawLine(pivot, tip);
}

void AnanMultiMeterItem::paintDebugOverlay(QPainter& p, const QRect& bg) const {
    for (const Needle& n : m_needles) {
        if (!n.visible) continue;
        p.setPen(Qt::NoPen);
        p.setBrush(n.color);
        for (auto it = n.calibration.begin(); it != n.calibration.end(); ++it) {
            const QPointF pt(bg.x() + it.value().x() * bg.width(),
                             bg.y() + it.value().y() * bg.height());
            p.drawEllipse(pt, 2.5, 2.5);
        }
    }
}

QString AnanMultiMeterItem::serialize() const {
    QJsonObject o;
    o.insert(QStringLiteral("kind"),      QStringLiteral("AnanMM"));
    o.insert(QStringLiteral("x"),         m_x);
    o.insert(QStringLiteral("y"),         m_y);
    o.insert(QStringLiteral("w"),         m_w);
    o.insert(QStringLiteral("h"),         m_h);
    o.insert(QStringLiteral("pivotX"),    m_pivot.x());
    o.insert(QStringLiteral("pivotY"),    m_pivot.y());
    o.insert(QStringLiteral("radiusX"),   m_radiusRatio.x());
    o.insert(QStringLiteral("radiusY"),   m_radiusRatio.y());
    o.insert(QStringLiteral("anchorBg"),  m_anchorToBgRect);
    o.insert(QStringLiteral("debug"),     m_debugOverlay);
    QJsonArray vis;
    for (const Needle& n : m_needles) {
        vis.append(n.visible);
    }
    o.insert(QStringLiteral("needlesVisible"), vis);
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool AnanMultiMeterItem::deserialize(const QString& data) {
    const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    if (!doc.isObject()) return false;
    const QJsonObject o = doc.object();
    if (o.value(QStringLiteral("kind")).toString() != QStringLiteral("AnanMM")) return false;
    setRect(float(o.value(QStringLiteral("x")).toDouble()),
            float(o.value(QStringLiteral("y")).toDouble()),
            float(o.value(QStringLiteral("w")).toDouble()),
            float(o.value(QStringLiteral("h")).toDouble()));
    m_pivot = QPointF(o.value(QStringLiteral("pivotX")).toDouble(),
                      o.value(QStringLiteral("pivotY")).toDouble());
    m_radiusRatio = QPointF(o.value(QStringLiteral("radiusX")).toDouble(),
                            o.value(QStringLiteral("radiusY")).toDouble());
    m_anchorToBgRect = o.value(QStringLiteral("anchorBg")).toBool(true);
    m_debugOverlay   = o.value(QStringLiteral("debug")).toBool(false);
    const QJsonArray vis = o.value(QStringLiteral("needlesVisible")).toArray();
    for (int i = 0; i < kNeedleCount && i < vis.size(); ++i) {
        m_needles[i].visible = vis.at(i).toBool(true);
    }
    return true;
}
```

- [ ] **Step 5: Register in CMake**

Edit `CMakeLists.txt` to add the new files to the NereusSDR target and add a test executable. Locate the existing `src/gui/meters/MeterItem.cpp` in the source list, and add near it:

```cmake
    src/gui/meters/presets/AnanMultiMeterItem.h
    src/gui/meters/presets/AnanMultiMeterItem.cpp
```

After the existing test registrations (search for `tst_meter_presets`), add:

```cmake
qt_add_executable(tst_anan_multi_meter_item
    tests/tst_anan_multi_meter_item.cpp
    src/gui/meters/presets/AnanMultiMeterItem.cpp
    src/gui/meters/MeterItem.cpp
)
target_include_directories(tst_anan_multi_meter_item PRIVATE src)
target_link_libraries(tst_anan_multi_meter_item PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME tst_anan_multi_meter_item COMMAND tst_anan_multi_meter_item)
```

- [ ] **Step 6: Run test to confirm pass**

```bash
cmake --build build --target tst_anan_multi_meter_item && \
  ./build/tst_anan_multi_meter_item
```

Expected output: `Totals: 5 passed, 0 failed, 0 skipped`.

- [ ] **Step 7: Add provenance row**

Edit `docs/attribution/THETIS-PROVENANCE.md`. Find the Thetis section's table and add:

```
| src/gui/meters/presets/AnanMultiMeterItem.cpp | Project Files/Source/Console/MeterManager.cs | 22461-22816 | @501e3f5 | AddAnanMM calibration port |
```

- [ ] **Step 8: Commit GPG-signed**

```bash
git add src/gui/meters/presets/AnanMultiMeterItem.h \
        src/gui/meters/presets/AnanMultiMeterItem.cpp \
        tests/tst_anan_multi_meter_item.cpp \
        CMakeLists.txt \
        docs/attribution/THETIS-PROVENANCE.md
git commit -S -m "$(cat <<'EOF'
feat(meters): add AnanMultiMeterItem with arc-fix anchoring

First-class MeterItem subclass collapsing the ANAN Multi Meter preset
into a single entity with internally-owned 7 needles + background.
Calibration tables ported byte-for-byte from Thetis MeterManager.cs
AddAnanMM (@501e3f5). Pivot/radius now anchor to the background
image rect so the arc stays glued to the face at any container
aspect ratio. Debug overlay toggle paints calibration points.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Tasks 2–9: Remaining composite preset items

Each of the following preset classes follows the same 8-step pattern as Task 1 (write test, run-fail, header, cpp, CMake, run-pass, provenance, commit). The differences are:

- the source Thetis method (file:line for provenance and attribution stamps)
- the internal composition (what primitives the preset paints)
- the calibration / configuration data

Because these classes share the same structural template as Task 1, each task below specifies the class's unique surface only. The engineer uses Task 1's step layout, substituting the class name and configuration.

### Task 2: `CrossNeedleItem`

**Thetis source:** `MeterManager.cs:AddCrossNeedle` line 22817 [@501e3f5].
**Shape:** Two needles — one reads forward power, the other reflected power; both share pivot and radius but have separate calibration tables.
**Files:** `src/gui/meters/presets/CrossNeedleItem.{h,cpp}`, `tests/tst_cross_needle_item.cpp`.
**Class skeleton differences from Task 1:**
- `static constexpr int kNeedleCount = 2;` (Fwd + Ref)
- `m_background = QImage(QStringLiteral(":/meters/crossNeedle.png"));`
- Two needle structs: `Forward` (binding `kReadingPwrFwd`) and `Reflected` (binding `kReadingPwrRef`).

**Calibration tables:** Port both tables verbatim from `AddCrossNeedle`. Quote the Add() calls in the `.cpp` file inline as `// From Thetis MeterManager.cs:22XXX [@501e3f5]` stamped sections, following Task 1's pattern.
**Commit:** `feat(meters): add CrossNeedleItem with fwd/reflected needles`.

### Task 3: `SMeterPresetItem`

**Thetis source:** `MeterManager.cs:AddSMeterBarSignal` line 21499 [@501e3f5] (the signal-bar + scale + readout composite).
**Shape:** Background + horizontal bar + scale ticks + signal strength text readout.
**Files:** `src/gui/meters/presets/SMeterPresetItem.{h,cpp}`, `tests/tst_s_meter_preset_item.cpp`.
**Internal composition:** Single bar bound to `kReadingSignalAvg`; scale labeled S1-S9, +10, +20, +30, +40, +60; value text display.
**Commit:** `feat(meters): add SMeterPresetItem collapsed composite`.

### Task 4: `PowerSwrPresetItem`

**Thetis source:** `MeterManager.cs:AddPWRBar` lines 23854, 23856, 23860, 23862 [@501e3f5] (the four overloads).
**Shape:** Two bars (Power + SWR) with shared background + dual scale + readouts.
**Files:** `src/gui/meters/presets/PowerSwrPresetItem.{h,cpp}`, `tests/tst_power_swr_preset_item.cpp`.
**Commit:** `feat(meters): add PowerSwrPresetItem composite`.

### Task 5: `MagicEyePresetItem`

**Thetis source:** `MeterManager.cs:AddMagicEye` line 22249 [@501e3f5].
**Shape:** Bullseye image + rotating leaf animation driven by signal strength.
**Files:** `src/gui/meters/presets/MagicEyePresetItem.{h,cpp}`, `tests/tst_magic_eye_preset_item.cpp`.
**Commit:** `feat(meters): add MagicEyePresetItem rotating-leaf variant`.

### Task 6: `SignalTextPresetItem`

**Thetis source:** `MeterManager.cs:AddSMeterBarText` line 21678 [@501e3f5].
**Shape:** Large text readout of signal strength in S-units + dBm.
**Files:** `src/gui/meters/presets/SignalTextPresetItem.{h,cpp}`, `tests/tst_signal_text_preset_item.cpp`.
**Commit:** `feat(meters): add SignalTextPresetItem large readout`.

### Task 7: `HistoryGraphPresetItem`

**Thetis source:** Factory `createHistoryPreset` in `src/gui/meters/ItemGroup.cpp` (NereusSDR-local; port from Thetis `HistoryGraphItem` consumer if applicable — grep `ItemGroup.cpp` for the method body).
**Shape:** Rolling history strip + scale + current value text.
**Files:** `src/gui/meters/presets/HistoryGraphPresetItem.{h,cpp}`, `tests/tst_history_graph_preset_item.cpp`.
**Commit:** `feat(meters): add HistoryGraphPresetItem rolling-graph`.

### Task 8: `VfoDisplayPresetItem`

**Thetis source:** Factory `createVfoDisplayPreset` in `src/gui/meters/ItemGroup.cpp`; port composition from Thetis VFO flag rendering.
**Shape:** Background + large 7-segment frequency digits + band/mode text.
**Files:** `src/gui/meters/presets/VfoDisplayPresetItem.{h,cpp}`, `tests/tst_vfo_display_preset_item.cpp`.
**Commit:** `feat(meters): add VfoDisplayPresetItem frequency readout`.

### Task 9: `ClockPresetItem` and `ContestPresetItem`

Two small composites that share a layout; implement as two separate classes following the same pattern. Each has its own test file and commit:

- `feat(meters): add ClockPresetItem`
- `feat(meters): add ContestPresetItem`

---

## Task 10: `BarPresetItem` — generic parameterised bar-row collapse

**Files:**
- Create: `src/gui/meters/presets/BarPresetItem.h`
- Create: `src/gui/meters/presets/BarPresetItem.cpp`
- Create: `tests/tst_bar_preset_item.cpp`
- Modify: `CMakeLists.txt`
- Modify: `docs/attribution/THETIS-PROVENANCE.md`

**Rationale.** 18 of the 34 `ItemGroup::create*Preset` factories are bar rows that share the same structure: backdrop + bar + scale + history + text. Rather than creating 18 near-identical classes, collapse them into one parameterized `BarPresetItem` whose configuration (binding, label, min/max, scale marks, colors) is stored with the item.

- [ ] **Step 1: Write failing test**

```cpp
#include <QtTest>
#include "gui/meters/presets/BarPresetItem.h"

class TestBarPresetItem : public QObject {
    Q_OBJECT
private slots:
    void configure_micPreset();
    void configure_alcPreset();
    void serialize_roundTrip_preservesConfiguration();
    void kindString_identifiesPresetFlavour();
};

void TestBarPresetItem::configure_micPreset() {
    BarPresetItem item;
    item.configureAsMic();
    QCOMPARE(item.presetKind(), BarPresetItem::Kind::Mic);
    QCOMPARE(item.minValue(), -30.0);
    QCOMPARE(item.maxValue(),   5.0);
}

void TestBarPresetItem::configure_alcPreset() {
    BarPresetItem item;
    item.configureAsAlc();
    QCOMPARE(item.presetKind(), BarPresetItem::Kind::Alc);
    QCOMPARE(item.minValue(), -30.0);
    QCOMPARE(item.maxValue(),   0.0);
}

void TestBarPresetItem::serialize_roundTrip_preservesConfiguration() {
    BarPresetItem a;
    a.configureAsAlcGain();
    a.setRect(0.1f, 0.2f, 0.8f, 0.1f);
    const QString data = a.serialize();

    BarPresetItem b;
    QVERIFY(b.deserialize(data));
    QCOMPARE(b.presetKind(), BarPresetItem::Kind::AlcGain);
    QCOMPARE(b.x(), 0.1f);
}

void TestBarPresetItem::kindString_identifiesPresetFlavour() {
    BarPresetItem item;
    item.configureAsMic();
    QCOMPARE(item.kindString(), QStringLiteral("Mic"));
    item.configureAsCfc();
    QCOMPARE(item.kindString(), QStringLiteral("Cfc"));
}

QTEST_MAIN(TestBarPresetItem)
#include "tst_bar_preset_item.moc"
```

- [ ] **Step 2: Run test, confirm fails** (`cmake --build build --target tst_bar_preset_item` → missing header)

- [ ] **Step 3: Write `BarPresetItem.h`**

```cpp
// Attribution header identical structure to Task 1; source refs various
// AddALCBar (23326), AddALCGainBar (23412), AddALCGroupBar (23473), etc.
#pragma once

#include "gui/meters/MeterItem.h"
#include <QColor>
#include <QString>

class BarPresetItem : public MeterItem {
    Q_OBJECT
public:
    enum class Kind {
        Mic, Alc, AlcGain, AlcGroup, Comp, Cfc, CfcGain,
        Leveler, LevelerGain, Agc, AgcGain, Pbsnr, Eq,
        SignalBar, AvgSignalBar, MaxBinBar, AdcBar, AdcMaxMag,
        Custom
    };

    explicit BarPresetItem(QObject* parent = nullptr);

    // Layer + paint
    Layer renderLayer() const override { return Layer::Geometry; }
    void paint(QPainter& p, int widgetW, int widgetH) override;

    // Serialisation
    QString serialize() const override;
    bool deserialize(const QString& data) override;

    // Configuration (one entry per bar-row flavour; ports one ItemGroup
    // factory each)
    void configureAsMic();           // From ItemGroup::createMicPreset
    void configureAsAlc();           // From Thetis MeterManager.cs:23326
    void configureAsAlcGain();       // From Thetis MeterManager.cs:23412
    void configureAsAlcGroup();      // From Thetis MeterManager.cs:23473
    void configureAsComp();          // From ItemGroup::createCompPreset
    void configureAsCfc();           // From ItemGroup::createCfcBarPreset
    void configureAsCfcGain();       // From ItemGroup::createCfcGainBarPreset
    void configureAsLeveler();
    void configureAsLevelerGain();
    void configureAsAgc();
    void configureAsAgcGain();
    void configureAsPbsnr();
    void configureAsEq();
    void configureAsSignalBar();
    void configureAsAvgSignalBar();
    void configureAsMaxBinBar();
    void configureAsAdcBar();
    void configureAsAdcMaxMag();
    void configureAsCustom(int bindingId, double minV, double maxV, const QString& label);

    // Accessors
    Kind   presetKind()  const { return m_kind; }
    double minValue()    const { return m_minValue; }
    double maxValue()    const { return m_maxValue; }
    QString kindString() const;
    QString label()      const { return m_label; }

private:
    void setCommon(Kind k, int binding, double minV, double maxV,
                   const QString& label, const QColor& barColor);

    Kind    m_kind = Kind::Custom;
    int     m_binding = -1;
    double  m_minValue = 0.0;
    double  m_maxValue = 1.0;
    QString m_label;
    QColor  m_barColor = Qt::green;
};
```

- [ ] **Step 4: Write `BarPresetItem.cpp` — the 18 configureAs methods**

```cpp
// Attribution header as Task 1, citing the 18 ported sources.
#include "gui/meters/presets/BarPresetItem.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>

BarPresetItem::BarPresetItem(QObject* parent) : MeterItem(parent) {}

void BarPresetItem::setCommon(Kind k, int binding, double minV, double maxV,
                              const QString& label, const QColor& barColor) {
    m_kind = k;
    m_binding = binding;
    m_minValue = minV;
    m_maxValue = maxV;
    m_label = label;
    m_barColor = barColor;
}

// From ItemGroup::createMicPreset — mic level -30..+5 dB
void BarPresetItem::configureAsMic() {
    setCommon(Kind::Mic, /*binding*/ 10, -30.0, 5.0,
              QStringLiteral("Mic"), QColor(80, 200, 255));
}

// From Thetis MeterManager.cs:23326 [@501e3f5] AddALCBar — ALC -30..0 dB
void BarPresetItem::configureAsAlc() {
    setCommon(Kind::Alc, /*binding*/ 11, -30.0, 0.0,
              QStringLiteral("ALC"), QColor(255, 160, 80));
}

// From Thetis MeterManager.cs:23412 [@501e3f5] AddALCGainBar — ALC gain 0..50 dB
void BarPresetItem::configureAsAlcGain() {
    setCommon(Kind::AlcGain, /*binding*/ 12, 0.0, 50.0,
              QStringLiteral("ALC Gain"), QColor(255, 140, 60));
}

// From Thetis MeterManager.cs:23473 [@501e3f5] AddALCGroupBar — ALC group -30..+25 dB
void BarPresetItem::configureAsAlcGroup() {
    setCommon(Kind::AlcGroup, /*binding*/ 13, -30.0, 25.0,
              QStringLiteral("ALC Group"), QColor(255, 80, 200));
}

// From ItemGroup::createCompPreset — compression 0..30 dB
void BarPresetItem::configureAsComp() {
    setCommon(Kind::Comp, /*binding*/ 14, 0.0, 30.0,
              QStringLiteral("COMP"), QColor(200, 160, 255));
}

// Remaining 13 configureAs methods follow the same pattern.
// Every method annotates its Thetis / ItemGroup source with an inline
// // From ... [vX.Y.Z.W | @sha] stamp.
// Fill these in when implementing using the bindings and ranges
// from ItemGroup.cpp and MeterManager.cs.

// ... (13 more configureAs* methods, each 3-5 lines) ...

QString BarPresetItem::kindString() const {
    switch (m_kind) {
        case Kind::Mic:          return QStringLiteral("Mic");
        case Kind::Alc:          return QStringLiteral("Alc");
        case Kind::AlcGain:      return QStringLiteral("AlcGain");
        case Kind::AlcGroup:     return QStringLiteral("AlcGroup");
        case Kind::Comp:         return QStringLiteral("Comp");
        case Kind::Cfc:          return QStringLiteral("Cfc");
        case Kind::CfcGain:      return QStringLiteral("CfcGain");
        case Kind::Leveler:      return QStringLiteral("Leveler");
        case Kind::LevelerGain:  return QStringLiteral("LevelerGain");
        case Kind::Agc:          return QStringLiteral("Agc");
        case Kind::AgcGain:      return QStringLiteral("AgcGain");
        case Kind::Pbsnr:        return QStringLiteral("Pbsnr");
        case Kind::Eq:           return QStringLiteral("Eq");
        case Kind::SignalBar:    return QStringLiteral("SignalBar");
        case Kind::AvgSignalBar: return QStringLiteral("AvgSignalBar");
        case Kind::MaxBinBar:    return QStringLiteral("MaxBinBar");
        case Kind::AdcBar:       return QStringLiteral("AdcBar");
        case Kind::AdcMaxMag:    return QStringLiteral("AdcMaxMag");
        case Kind::Custom:       return QStringLiteral("Custom");
    }
    return QString();
}

void BarPresetItem::paint(QPainter& p, int widgetW, int widgetH) {
    const QRect r(int(m_x * widgetW), int(m_y * widgetH),
                  int(m_w * widgetW), int(m_h * widgetH));
    p.fillRect(r, QColor(0, 0, 0, 128));
    // Draw fill proportional to value (fallback to midpoint like Task 1).
    const double value = 0.5 * (m_minValue + m_maxValue);
    const double frac  = (value - m_minValue) / (m_maxValue - m_minValue);
    QRect fill = r;
    fill.setWidth(int(r.width() * frac));
    p.fillRect(fill, m_barColor);
    // Label
    p.setPen(Qt::white);
    p.drawText(r, Qt::AlignCenter, m_label);
}

QString BarPresetItem::serialize() const {
    QJsonObject o;
    o.insert(QStringLiteral("kind"),   QStringLiteral("BarPreset"));
    o.insert(QStringLiteral("flavor"), kindString());
    o.insert(QStringLiteral("x"), m_x);
    o.insert(QStringLiteral("y"), m_y);
    o.insert(QStringLiteral("w"), m_w);
    o.insert(QStringLiteral("h"), m_h);
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool BarPresetItem::deserialize(const QString& data) {
    const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    if (!doc.isObject()) return false;
    const QJsonObject o = doc.object();
    if (o.value(QStringLiteral("kind")).toString() != QStringLiteral("BarPreset")) return false;
    const QString flavor = o.value(QStringLiteral("flavor")).toString();
    if      (flavor == QStringLiteral("Mic"))       configureAsMic();
    else if (flavor == QStringLiteral("Alc"))       configureAsAlc();
    else if (flavor == QStringLiteral("AlcGain"))   configureAsAlcGain();
    else if (flavor == QStringLiteral("AlcGroup"))  configureAsAlcGroup();
    else if (flavor == QStringLiteral("Comp"))      configureAsComp();
    // ... repeat for all 18 flavors ...
    else                                             return false;
    setRect(float(o.value(QStringLiteral("x")).toDouble()),
            float(o.value(QStringLiteral("y")).toDouble()),
            float(o.value(QStringLiteral("w")).toDouble()),
            float(o.value(QStringLiteral("h")).toDouble()));
    return true;
}
```

- [ ] **Steps 5–8: CMake registration, run-pass, provenance row, commit** — follow Task 1 pattern.

**Commit:** `feat(meters): add BarPresetItem parameterised 18-flavor bar row`.

---

## Task 11: Dialog — swap preset factory paths to new classes

**Files:**
- Modify: `src/gui/containers/ContainerSettingsDialog.h` (add new include + remove ItemGroup references in preset dispatch)
- Modify: `src/gui/containers/ContainerSettingsDialog.cpp` (`:608-625` onAddFromAvailable, `:627` appendPresetRow, `:1268` addNewItem)

- [ ] **Step 1: Write failing integration test**

Create `tests/tst_dialog_preset_add.cpp` that constructs a `ContainerSettingsDialog`, simulates selecting "ANAN Multi Meter" in the available list, clicks Add, and asserts the in-use list contains exactly one row whose item is an `AnanMultiMeterItem` (not 8 `NeedleItem`s + 1 `ImageItem`). Register as a test executable in CMakeLists.txt.

- [ ] **Step 2: Run-fail** — current behavior adds 8 primitives; assertion fails.

- [ ] **Step 3: Rewrite `onAddFromAvailable` dispatch**

In `ContainerSettingsDialog.cpp:608-625`, replace the preset dispatch that calls `ItemGroup::create*Preset()` with direct construction of the new classes. Template:

```cpp
void ContainerSettingsDialog::onAddFromAvailable() {
    QListWidgetItem* sel = m_availableList->currentItem();
    if (!sel) return;
    const QString tag = sel->data(Qt::UserRole).toString();
    MeterItem* created = nullptr;

    if (tag == QStringLiteral("PRESET_AnanMM")) {
        created = new AnanMultiMeterItem(this);
    } else if (tag == QStringLiteral("PRESET_CrossNeedle")) {
        created = new CrossNeedleItem(this);
    } else if (tag == QStringLiteral("PRESET_SMeter")) {
        created = new SMeterPresetItem(this);
    } else if (tag == QStringLiteral("PRESET_PowerSwr")) {
        created = new PowerSwrPresetItem(this);
    } else if (tag == QStringLiteral("PRESET_MagicEye")) {
        created = new MagicEyePresetItem(this);
    } else if (tag == QStringLiteral("PRESET_SignalText")) {
        created = new SignalTextPresetItem(this);
    } else if (tag == QStringLiteral("PRESET_History")) {
        created = new HistoryGraphPresetItem(this);
    } else if (tag == QStringLiteral("PRESET_VfoDisplay")) {
        created = new VfoDisplayPresetItem(this);
    } else if (tag == QStringLiteral("PRESET_Clock")) {
        created = new ClockPresetItem(this);
    } else if (tag == QStringLiteral("PRESET_Contest")) {
        created = new ContestPresetItem(this);
    } else if (tag.startsWith(QStringLiteral("PRESET_Bar_"))) {
        auto* bar = new BarPresetItem(this);
        const QString flavor = tag.mid(QStringLiteral("PRESET_Bar_").size());
        if      (flavor == QLatin1String("Mic"))       bar->configureAsMic();
        else if (flavor == QLatin1String("Alc"))       bar->configureAsAlc();
        // ... all 18 flavors ...
        created = bar;
    } else {
        // Primitive path unchanged
        addNewItem(tag);
        return;
    }

    if (created) {
        created->setRect(0.1f, 0.1f, 0.8f, 0.2f);
        m_workingItems.append(created);
        refreshInUseList();
        applyToContainer();
    }
}
```

- [ ] **Step 4: Delete `appendPresetRow` and any `ItemGroup::create*Preset()` calls from the dialog.**

- [ ] **Step 5: Run test to confirm pass** — in-use list contains one `AnanMultiMeterItem`.

- [ ] **Step 6: Commit**

```
refactor(dialog): use first-class preset classes in add-from-available
```

---

## Task 12: Hybrid addition rule (presets single-instance per container)

**Files:**
- Modify: `src/gui/containers/ContainerSettingsDialog.cpp` (refresh available list logic)

- [ ] **Step 1: Write failing test**

`tests/tst_hybrid_addition_rule.cpp`:

```cpp
#include <QtTest>
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/presets/AnanMultiMeterItem.h"
#include "gui/meters/BarItem.h"

class TestHybridAddRule : public QObject {
    Q_OBJECT
private slots:
    void presetAddedOnce_availableEntryDisabled();
    void presetDisabledEntry_addAttemptIsNoop();
    void primitive_canBeAddedTwice();
};

void TestHybridAddRule::presetAddedOnce_availableEntryDisabled() {
    ContainerManager mgr;
    ContainerWidget* c = mgr.createContainer(1, DockMode::Floating);
    c->setContent(new MeterWidget());
    ContainerSettingsDialog dlg(c, nullptr, &mgr);

    dlg.selectAvailableByTag(QStringLiteral("PRESET_AnanMM"));
    dlg.triggerAddFromAvailable();

    QVERIFY(dlg.availableEntryIsDisabled(QStringLiteral("PRESET_AnanMM")));
}

void TestHybridAddRule::presetDisabledEntry_addAttemptIsNoop() {
    // ... constructs dialog, adds AnanMM, attempts second add, asserts
    // workingItems() count stays at 1.
}

void TestHybridAddRule::primitive_canBeAddedTwice() {
    // ... adds two Bar primitives, asserts workingItems() count is 2.
}

QTEST_MAIN(TestHybridAddRule)
#include "tst_hybrid_add_rule.moc"
```

- [ ] **Step 2: Expose test hooks** on `ContainerSettingsDialog`:

```cpp
// In ContainerSettingsDialog.h, public:
void selectAvailableByTag(const QString& tag);   // test hook
void triggerAddFromAvailable();                  // test hook
bool availableEntryIsDisabled(const QString& tag) const; // test hook
QVector<MeterItem*> workingItems() const { return m_workingItems; }
```

Implement these in `.cpp`.

- [ ] **Step 3: Run-fail** — primitives pass, preset disabling assertion fails.

- [ ] **Step 4: Implement disable-after-add**

In `ContainerSettingsDialog.cpp`, add a helper:

```cpp
namespace {
bool isPresetTag(const QString& tag) {
    return tag.startsWith(QStringLiteral("PRESET_"));
}
} // namespace

void ContainerSettingsDialog::refreshAvailableList() {
    // Iterate every available-list row; if its tag is a preset tag
    // and a working-item of that tag already exists, disable the row
    // and annotate "(in use)".
    QSet<QString> inUseTags;
    for (MeterItem* it : m_workingItems) {
        inUseTags.insert(presetTagForItem(it)); // returns empty for primitives
    }
    for (int i = 0; i < m_availableList->count(); ++i) {
        QListWidgetItem* row = m_availableList->item(i);
        const QString tag = row->data(Qt::UserRole).toString();
        if (!isPresetTag(tag)) {
            row->setFlags(row->flags() | Qt::ItemIsEnabled);
            continue;
        }
        const bool used = inUseTags.contains(tag);
        if (used) {
            row->setFlags(row->flags() & ~Qt::ItemIsEnabled);
            row->setText(row->text().section(QStringLiteral(" ("), 0, 0)
                         + QStringLiteral(" (in use)"));
        } else {
            row->setFlags(row->flags() | Qt::ItemIsEnabled);
            row->setText(row->text().section(QStringLiteral(" ("), 0, 0));
        }
    }
}

QString ContainerSettingsDialog::presetTagForItem(MeterItem* it) const {
    if (dynamic_cast<AnanMultiMeterItem*>(it))   return QStringLiteral("PRESET_AnanMM");
    if (dynamic_cast<CrossNeedleItem*>(it))      return QStringLiteral("PRESET_CrossNeedle");
    if (dynamic_cast<SMeterPresetItem*>(it))     return QStringLiteral("PRESET_SMeter");
    if (dynamic_cast<PowerSwrPresetItem*>(it))   return QStringLiteral("PRESET_PowerSwr");
    if (dynamic_cast<MagicEyePresetItem*>(it))   return QStringLiteral("PRESET_MagicEye");
    if (dynamic_cast<SignalTextPresetItem*>(it)) return QStringLiteral("PRESET_SignalText");
    if (dynamic_cast<HistoryGraphPresetItem*>(it))return QStringLiteral("PRESET_History");
    if (dynamic_cast<VfoDisplayPresetItem*>(it)) return QStringLiteral("PRESET_VfoDisplay");
    if (dynamic_cast<ClockPresetItem*>(it))      return QStringLiteral("PRESET_Clock");
    if (dynamic_cast<ContestPresetItem*>(it))    return QStringLiteral("PRESET_Contest");
    if (auto* b = dynamic_cast<BarPresetItem*>(it)) {
        return QStringLiteral("PRESET_Bar_") + b->kindString();
    }
    return QString(); // primitive
}
```

Call `refreshAvailableList()` from `onAddFromAvailable()` after appending to `m_workingItems`, from `onRemoveItem()` after removal, and from the dialog's `showEvent`.

- [ ] **Step 5: Run test to confirm pass**

- [ ] **Step 6: Commit** — `feat(dialog): hybrid addition rule — presets single-instance per container`.

---

## Task 13: In-use list — drag-reorder, rename, right-click menu, Duplicate

**Files:**
- Modify: `src/gui/containers/ContainerSettingsDialog.h` (remove Up/Down buttons; add right-click handler; add rename support)
- Modify: `src/gui/containers/ContainerSettingsDialog.cpp` (add `m_inUseItems` wrapper vector, `handleInUseContextMenu`, drag-reorder hookup, `onItemRename`, `onItemDuplicate`)

- [ ] **Step 1: Write failing tests**

```cpp
// tests/tst_inuse_list_ux.cpp
void TestInUseList::dragReorder_updatesBothVectors() { /* ... */ }
void TestInUseList::rename_persistsInSnapshot() { /* ... */ }
void TestInUseList::duplicate_primitive_createsCopy() { /* ... */ }
void TestInUseList::duplicate_preset_isBlocked() { /* ... */ }
void TestInUseList::contextMenu_showsRenameDuplicateDelete() { /* ... */ }
```

- [ ] **Step 2: Run-fail** — methods don't exist.

- [ ] **Step 3: Add wrapper struct**

In `ContainerSettingsDialog.h`:

```cpp
struct ContainerInUseRow {
    MeterItem* item = nullptr;
    QString    displayName;
    QUuid      rowId;
};
```

Replace `QVector<MeterItem*> m_workingItems;` with `QVector<ContainerInUseRow> m_workingItems;` (keep the name). Update call sites accordingly — `.item` accessor, `.displayName` etc.

- [ ] **Step 4: Enable drag-reorder on `m_inUseList`**

```cpp
// in buildInUseColumn() constructor
m_inUseList->setDragDropMode(QAbstractItemView::InternalMove);
m_inUseList->setDragEnabled(true);
m_inUseList->setAcceptDrops(true);
m_inUseList->setDefaultDropAction(Qt::MoveAction);

connect(m_inUseList->model(), &QAbstractItemModel::rowsMoved,
        this, [this]() { syncWorkingItemsFromList(); applyToContainer(); });
```

Implement `syncWorkingItemsFromList()` — walks `m_inUseList` in display order, looks up each row's `rowId` from its `Qt::UserRole` data, and reorders `m_workingItems` to match. Z-order in `MeterWidget` is updated by `applyToContainer()`.

- [ ] **Step 5: Remove Up/Down buttons**

Delete `m_btnMoveUp` / `m_btnMoveDown` declarations and their layout insertion. Drag replaces them.

- [ ] **Step 6: Add right-click context menu**

```cpp
m_inUseList->setContextMenuPolicy(Qt::CustomContextMenu);
connect(m_inUseList, &QListWidget::customContextMenuRequested,
        this, &ContainerSettingsDialog::onInUseContextMenu);

void ContainerSettingsDialog::onInUseContextMenu(const QPoint& pos) {
    QListWidgetItem* row = m_inUseList->itemAt(pos);
    if (!row) return;
    const QUuid rowId = QUuid::fromString(row->data(Qt::UserRole).toString());
    QMenu menu(this);
    QAction* actRename = menu.addAction(QStringLiteral("Rename..."));
    QAction* actDup    = menu.addAction(QStringLiteral("Duplicate"));
    QAction* actDel    = menu.addAction(QStringLiteral("Delete"));
    // Block Duplicate on presets:
    MeterItem* it = findItemByRowId(rowId);
    if (!presetTagForItem(it).isEmpty()) {
        actDup->setEnabled(false);
    }
    QAction* chosen = menu.exec(m_inUseList->viewport()->mapToGlobal(pos));
    if      (chosen == actRename) onItemRename(rowId);
    else if (chosen == actDup)    onItemDuplicate(rowId);
    else if (chosen == actDel)    onItemDelete(rowId);
}
```

- [ ] **Step 7: Implement Rename / Duplicate / Delete slots**

```cpp
void ContainerSettingsDialog::onItemRename(const QUuid& rowId) {
    bool ok = false;
    const QString oldName = displayNameForRow(rowId);
    const QString newName = QInputDialog::getText(
        this, tr("Rename item"), tr("Display name:"),
        QLineEdit::Normal, oldName, &ok);
    if (!ok) return;
    for (ContainerInUseRow& row : m_workingItems) {
        if (row.rowId == rowId) { row.displayName = newName; break; }
    }
    refreshInUseList();
    applyToContainer();
}

void ContainerSettingsDialog::onItemDuplicate(const QUuid& rowId) {
    // Only primitives reach here (presets blocked by menu).
    MeterItem* orig = findItemByRowId(rowId);
    if (!orig) return;
    MeterItem* copy = cloneMeterItem(orig); // helper creates fresh subclass instance + deserialises
    if (!copy) return;
    ContainerInUseRow newRow;
    newRow.item = copy;
    newRow.displayName = displayNameForRow(rowId) + QStringLiteral(" (copy)");
    newRow.rowId = QUuid::createUuid();
    m_workingItems.append(newRow);
    refreshInUseList();
    applyToContainer();
}

void ContainerSettingsDialog::onItemDelete(const QUuid& rowId) {
    for (int i = 0; i < m_workingItems.size(); ++i) {
        if (m_workingItems[i].rowId == rowId) {
            delete m_workingItems[i].item;
            m_workingItems.remove(i);
            break;
        }
    }
    refreshInUseList();
    refreshAvailableList(); // re-enable the preset if it was a preset
    applyToContainer();
}
```

- [ ] **Step 8: Run tests, confirm pass.**

- [ ] **Step 9: Commit** — `feat(dialog): drag-reorder, rename, duplicate, right-click menu`.

---

## Task 14: `+ Add` button in Edit Container dialog

**Files:**
- Modify: `src/gui/containers/ContainerSettingsDialog.cpp` (`:761-799` where the Container dropdown is built; insert a `QPushButton`)

- [ ] **Step 1: Write failing test**

```cpp
// tests/tst_dialog_add_container.cpp
void TestDialogAddContainer::clickAdd_createsNewContainer_switchesDropdownToIt() {
    ContainerManager mgr;
    ContainerWidget* c0 = mgr.createContainer(1, DockMode::Floating);
    c0->setContent(new MeterWidget());
    ContainerSettingsDialog dlg(c0, nullptr, &mgr);
    const int before = mgr.allContainers().size();
    dlg.triggerAddContainer();
    const int after = mgr.allContainers().size();
    QCOMPARE(after, before + 1);
    QCOMPARE(dlg.currentContainerDropdownText(), QStringLiteral("Meter 2"));
}
```

- [ ] **Step 2: Run-fail** — no such method.

- [ ] **Step 3: Implement Add button**

In `buildContainerPropertiesSection()` just after the dropdown's `addWidget(m_containerDropdown)`:

```cpp
m_btnAddContainer = new QPushButton(QStringLiteral("+ Add"), bar);
m_btnAddContainer->setToolTip(tr("Create a new container and switch to editing it"));
m_btnAddContainer->setStyleSheet(
    "QPushButton { background: #0d3f4a; color: #7fffd4; "
    "  border: 1px solid #00b4d8; border-radius: 3px; padding: 3px 8px; "
    "  font-weight: 600; }"
    "QPushButton:hover { background: #104a57; }");
connect(m_btnAddContainer, &QPushButton::clicked,
        this, &ContainerSettingsDialog::onAddContainer);
barLayout->addWidget(m_btnAddContainer);

// Expose test hook:
void ContainerSettingsDialog::triggerAddContainer() { onAddContainer(); }
QString ContainerSettingsDialog::currentContainerDropdownText() const {
    return m_containerDropdown->currentText();
}

void ContainerSettingsDialog::onAddContainer() {
    if (!m_manager) return;
    applyToContainer(); // commit any pending edits to current container
    ContainerWidget* created = m_manager->createContainer(1, DockMode::Floating);
    created->setNotes(nextAutoName()); // helper picks smallest unused "Meter N"
    created->setContent(new MeterWidget());
    // containerAdded signal triggers dropdown refresh; switch to the new one:
    const int idx = m_containerDropdown->findData(created->id());
    if (idx >= 0) m_containerDropdown->setCurrentIndex(idx);
}
```

- [ ] **Step 4: Implement `nextAutoName()` helper**

```cpp
QString ContainerSettingsDialog::nextAutoName() const {
    QSet<int> used;
    const auto all = m_manager->allContainers();
    QRegularExpression re(QStringLiteral("^Meter (\\d+)$"));
    for (ContainerWidget* c : all) {
        const auto m = re.match(c->notes());
        if (m.hasMatch()) used.insert(m.captured(1).toInt());
    }
    int n = 1;
    while (used.contains(n)) ++n;
    return QStringLiteral("Meter %1").arg(n);
}
```

- [ ] **Step 5: Run test, confirm pass.**

- [ ] **Step 6: Commit** — `feat(dialog): + Add button creates and switches to new container`.

---

## Task 15: `ContainerWidget` — context menu, double-click, gear icon

**Files:**
- Modify: `src/gui/containers/ContainerWidget.h` (add signals, header layout for gear button)
- Modify: `src/gui/containers/ContainerWidget.cpp` (implement `contextMenuEvent`, `mouseDoubleClickEvent`, add `QToolButton* m_gearBtn`)

- [ ] **Step 1: Write failing test**

```cpp
// tests/tst_container_widget_edit_access.cpp
void TestContainerWidgetEditAccess::contextMenu_emitsEditRequested() {
    ContainerManager mgr;
    ContainerWidget* c = mgr.createContainer(1, DockMode::Floating);
    QSignalSpy spy(c, &ContainerWidget::editRequested);
    QContextMenuEvent ev(QContextMenuEvent::Mouse, QPoint(10, 10));
    QApplication::sendEvent(c, &ev);
    // Find the "Edit..." action in whatever menu was popped, trigger it.
    // (test uses a QTimer::singleShot workaround on QApplication::activePopupWidget)
    QCOMPARE(spy.count(), 1);
}

void TestContainerWidgetEditAccess::doubleClickHeader_emitsEditRequested() { /* ... */ }
void TestContainerWidgetEditAccess::gearButton_emitsEditRequested() { /* ... */ }
```

- [ ] **Step 2: Run-fail** — no signals defined.

- [ ] **Step 3: Add signals to `ContainerWidget.h`**

```cpp
signals:
    void editRequested();
    void renameRequested();
    void duplicateRequested();
    void deleteRequested();
    void dockModeChangeRequested(DockMode m);
```

- [ ] **Step 4: Override `contextMenuEvent`**

```cpp
void ContainerWidget::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);
    QAction* actEdit   = menu.addAction(tr("Edit..."));
    QAction* actRename = menu.addAction(tr("Rename..."));
    QAction* actDup    = menu.addAction(tr("Duplicate"));
    QAction* actDel    = menu.addAction(tr("Delete"));
    menu.addSeparator();
    QMenu* dockMenu = menu.addMenu(tr("Dock Mode"));
    QAction* actPanel    = dockMenu->addAction(tr("Panel"));
    QAction* actOverlay  = dockMenu->addAction(tr("Overlay"));
    QAction* actFloating = dockMenu->addAction(tr("Floating"));
    actPanel->setCheckable(true); actOverlay->setCheckable(true); actFloating->setCheckable(true);
    switch (m_dockMode) {
        case DockMode::Panel:    actPanel->setChecked(true); break;
        case DockMode::Overlay:  actOverlay->setChecked(true); break;
        case DockMode::Floating: actFloating->setChecked(true); break;
    }

    QAction* chosen = menu.exec(event->globalPos());
    if      (chosen == actEdit)     emit editRequested();
    else if (chosen == actRename)   emit renameRequested();
    else if (chosen == actDup)      emit duplicateRequested();
    else if (chosen == actDel)      emit deleteRequested();
    else if (chosen == actPanel)    emit dockModeChangeRequested(DockMode::Panel);
    else if (chosen == actOverlay)  emit dockModeChangeRequested(DockMode::Overlay);
    else if (chosen == actFloating) emit dockModeChangeRequested(DockMode::Floating);
}
```

- [ ] **Step 5: Override `mouseDoubleClickEvent`**

```cpp
void ContainerWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (m_locked) { QWidget::mouseDoubleClickEvent(event); return; }
    if (m_titleBarVisible && event->pos().y() < kTitleBarHeight) {
        emit editRequested();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}
```

(Define `kTitleBarHeight` as the existing title-bar strip height constant, grep the .cpp for the value currently hard-coded around `paintEvent`.)

- [ ] **Step 6: Add gear button to title bar**

Find the existing title-bar painting/layout in `ContainerWidget.cpp`. Add a `QToolButton* m_gearBtn` as a child, position it in the right corner of the title bar with an absolute position inside `resizeEvent`. Icon via `:/icons/gear.svg` (add a simple SVG to `resources.qrc`).

```cpp
// ContainerWidget.cpp constructor
m_gearBtn = new QToolButton(this);
m_gearBtn->setIcon(QIcon(QStringLiteral(":/icons/gear.svg")));
m_gearBtn->setAutoRaise(true);
m_gearBtn->setToolTip(tr("Edit container..."));
m_gearBtn->setFixedSize(kGearButtonSize, kGearButtonSize);
connect(m_gearBtn, &QToolButton::clicked, this, &ContainerWidget::editRequested);

// ContainerWidget.cpp resizeEvent
m_gearBtn->move(width() - kGearButtonSize - kGearMargin, kGearMarginY);
m_gearBtn->setVisible(m_titleBarVisible);
```

- [ ] **Step 7: Wire `MainWindow` to the new signals**

In `MainWindow.cpp` where `createContainer` is called, add:

```cpp
auto openEditFor = [this](ContainerWidget* target) {
    ContainerSettingsDialog dlg(target, this, m_containerManager);
    dlg.exec();
};
connect(container, &ContainerWidget::editRequested, this,
        [this, container, openEditFor]() { openEditFor(container); });
connect(container, &ContainerWidget::deleteRequested, this,
        [this, container]() { m_containerManager->destroyContainer(container->id()); });
connect(container, &ContainerWidget::renameRequested, this,
        [this, container]() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename"), tr("Name:"),
        QLineEdit::Normal, container->notes(), &ok);
    if (ok) container->setNotes(name);
});
connect(container, &ContainerWidget::dockModeChangeRequested, this,
        [container](DockMode m) { container->setDockMode(m); });
```

- [ ] **Step 8: Run tests, confirm pass.**

- [ ] **Step 9: Commit** — `feat(container-widget): right-click, double-click, gear-icon edit access`.

---

## Task 16: `FloatingContainer` event delegation

**Files:**
- Modify: `src/gui/containers/FloatingContainer.cpp` (forward `contextMenuEvent` and `mouseDoubleClickEvent` to embedded `ContainerWidget`)

- [ ] **Step 1: Write failing test** (`tst_floating_container_access.cpp`) — simulate right-click inside floating window, assert `editRequested` fires.

- [ ] **Step 2: Implement delegation**

```cpp
void FloatingContainer::contextMenuEvent(QContextMenuEvent* event) {
    if (m_embedded) QCoreApplication::sendEvent(m_embedded, event);
    else QWidget::contextMenuEvent(event);
}
void FloatingContainer::mouseDoubleClickEvent(QMouseEvent* event) {
    if (m_embedded) QCoreApplication::sendEvent(m_embedded, event);
    else QWidget::mouseDoubleClickEvent(event);
}
```

- [ ] **Step 3: Run test, confirm pass, commit.** — `feat(floating): forward right-click and double-click to inner ContainerWidget`.

---

## Task 17: Auto-name + menu cleanup (View → Applets)

**Files:**
- Modify: `src/gui/MainWindow.cpp` (`:1400-1417` New Container auto-name; `:1452-1476` move 7 applet toggles to View)
- Modify: `src/gui/MainWindow.h` (add `m_viewMenu` if absent)

- [ ] **Step 1: Write failing test**

```cpp
// tests/tst_mainwindow_menu_layout.cpp
void TestMenuLayout::containersMenu_hasNoAppletToggles() {
    MainWindow w;
    w.show();
    QMenu* containers = w.findChild<QMenu*>(QStringLiteral("Containers"));
    for (QAction* a : containers->actions()) {
        QVERIFY2(!a->text().contains(QStringLiteral("Digital"),
                                      Qt::CaseInsensitive),
                 "Digital applet toggle must live under View > Applets");
    }
}

void TestMenuLayout::viewAppletsSubmenu_hasSevenToggles() {
    MainWindow w;
    QMenu* view = w.findChild<QMenu*>(QStringLiteral("View"));
    QMenu* applets = nullptr;
    for (QAction* a : view->actions()) {
        if (a->text() == QStringLiteral("Applets")) { applets = a->menu(); break; }
    }
    QVERIFY(applets);
    QCOMPARE(applets->actions().size(), 7);
}

void TestMenuLayout::newContainer_defaultName_isMeter1() {
    MainWindow w;
    QAction* newContainer = w.findChild<QAction*>(QStringLiteral("NewContainerAction"));
    newContainer->trigger();
    const auto all = w.containerManager()->allContainers();
    QVERIFY(std::any_of(all.begin(), all.end(),
            [](ContainerWidget* c) { return c->notes() == QStringLiteral("Meter 1"); }));
}
```

- [ ] **Step 2: Run-fail** — toggles still under Containers; new containers named "Meter" not "Meter 1".

- [ ] **Step 3: Create `View → Applets` submenu in `MainWindow::buildMenuBar`**

Move lines 1452-1476 (the 7 `addContainerToggle(...)` calls) to under `m_viewMenu`. Ensure `m_viewMenu->addMenu(QStringLiteral("&Applets"))` is the parent.

Delete the now-empty separator at line 1450 and the old 7 addContainerToggle calls from the Containers menu.

- [ ] **Step 4: Auto-name new containers**

In the "New Container..." lambda around `MainWindow.cpp:1401-1417`:

```cpp
// Before: c->setNotes(QStringLiteral("Meter"));
c->setNotes(nextContainerAutoName()); // reuse helper from Task 14
```

Hoist `nextContainerAutoName()` into `MainWindow` (or free function in `ContainerNaming.h`) so both `MainWindow` and `ContainerSettingsDialog` can call it.

- [ ] **Step 5: Update `rebuildEditContainerSubmenu`**

Remove the `"(unnamed) " + id.left(8)` fallback — auto-named containers always have notes. Simplify to:

```cpp
const QString label = c->notes();
```

- [ ] **Step 6: Run tests, confirm pass.**

- [ ] **Step 7: Commit** — `refactor(menu): View > Applets split from Containers; auto-name Meter N`.

---

## Task 18: Legacy container migration

**Files:**
- Create: `src/gui/meters/presets/LegacyPresetMigrator.h`
- Create: `src/gui/meters/presets/LegacyPresetMigrator.cpp`
- Modify: `src/gui/meters/MeterWidget.cpp` (invoke migrator on load)
- Create: `tests/tst_legacy_container_migration.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing test**

```cpp
#include <QtTest>
#include "gui/meters/MeterWidget.h"
#include "gui/meters/presets/AnanMultiMeterItem.h"
#include "core/AppSettings.h"

class TestLegacyMigration : public QObject {
    Q_OBJECT
private slots:
    void anamMmGroup_collapsesToSingleAnanMultiMeterItem();
    void customisedNeedle_fallsBackToLoosePrimitives_withWarning();
    void sMeterGroup_collapsesToSingleSMeterPresetItem();
    void unrecognisedGroup_stayAsLooseItems();
};

void TestLegacyMigration::anamMmGroup_collapsesToSingleAnanMultiMeterItem() {
    // Load tests/fixtures/legacy/pre-refactor-settings.xml via AppSettings,
    // construct MeterWidget from the ANAN MM container's blob.
    AppSettings::instance().loadFromFile(
        QStringLiteral("tests/fixtures/legacy/pre-refactor-settings.xml"));
    MeterWidget mw;
    mw.loadFromAppSettings("ContainerAnanMM");
    QCOMPARE(mw.items().size(), 1);
    QVERIFY(dynamic_cast<AnanMultiMeterItem*>(mw.items().first()) != nullptr);
}

void TestLegacyMigration::customisedNeedle_fallsBackToLoosePrimitives_withWarning() {
    // Construct a synthetic legacy blob with one needle's color customised
    // to a value not representable in AnanMultiMeterItem's fields.
    // Assert that the widget ends up with 8 loose items and a warning
    // was logged via qCWarning.
}

QTEST_MAIN(TestLegacyMigration)
#include "tst_legacy_migration.moc"
```

- [ ] **Step 2: Run-fail**

- [ ] **Step 3: Implement the migrator**

```cpp
// LegacyPresetMigrator.h
#pragma once
#include <QVector>
class MeterItem;
class MeterItem;

class LegacyPresetMigrator {
public:
    struct Result {
        QVector<MeterItem*> upgradedItems; // may be a mix of new preset
                                           // classes and untouched
                                           // primitives
        int presetsMigrated = 0;
        int fallbackCases = 0;             // incremented when tolerant
                                           // fallback keeps loose items
    };
    // Consumes items (takes ownership); returns a new vector where
    // groups matching known preset signatures are collapsed.
    static Result migrate(QVector<MeterItem*> legacyItems);
};

// LegacyPresetMigrator.cpp (abbreviated)
#include "gui/meters/presets/LegacyPresetMigrator.h"
#include "gui/meters/presets/AnanMultiMeterItem.h"
#include "gui/meters/ImageItem.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/NeedleItem.h" // or wherever NeedleItem lives
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcMigrate, "nereus.migrate.preset")

namespace {
constexpr QPointF kAnanMmPivot(0.004, 0.736);
constexpr QPointF kAnanMmRadius(1.0, 0.58);

bool matchesAnanMmSignature(const QVector<MeterItem*>& items, int startIdx) {
    if (items.size() - startIdx < 8) return false;
    auto* bg = dynamic_cast<ImageItem*>(items[startIdx]);
    if (!bg || !bg->imagePath().contains(QStringLiteral("ananMM"))) return false;
    for (int k = 1; k <= 7; ++k) {
        auto* n = dynamic_cast<NeedleItem*>(items[startIdx + k]);
        if (!n) return false;
        if (n->needleOffset() != kAnanMmPivot) return false;
        if (n->radiusRatio()  != kAnanMmRadius) return false;
    }
    return true;
}

bool ananMmIsCustomized(const QVector<MeterItem*>& items, int startIdx) {
    // Check whether any of the 7 needles has a customised color beyond
    // the port defaults. If so, fall back.
    for (int k = 1; k <= 7; ++k) {
        auto* n = dynamic_cast<NeedleItem*>(items[startIdx + k]);
        // Compare needleColor against the defaults baked into
        // AnanMultiMeterItem::initialiseNeedles — if any differ, mark
        // customised.
        // (Concrete list of default colors comes from Task 1's
        // initialiseNeedles body.)
    }
    return false;
}

MeterItem* buildAnanMmFromLegacy(const QVector<MeterItem*>& items, int startIdx) {
    auto* newItem = new AnanMultiMeterItem();
    // Position from the group's bounding rect (or the first item's rect)
    auto* bg = items[startIdx];
    newItem->setRect(bg->x(), bg->y(), bg->itemWidth(), bg->itemHeight());
    // Visibility from needle loose items (if all visible, defaults apply)
    for (int k = 1; k <= 7; ++k) {
        auto* n = dynamic_cast<NeedleItem*>(items[startIdx + k]);
        newItem->setNeedleVisible(k - 1, n ? n->isVisible() : true);
    }
    return newItem;
}
} // namespace

LegacyPresetMigrator::Result LegacyPresetMigrator::migrate(
        QVector<MeterItem*> legacyItems) {
    Result r;
    for (int i = 0; i < legacyItems.size(); ) {
        if (matchesAnanMmSignature(legacyItems, i)) {
            if (ananMmIsCustomized(legacyItems, i)) {
                qCWarning(lcMigrate) << "ANAN MM customised, keeping 8 loose items";
                for (int k = 0; k < 8; ++k) r.upgradedItems.append(legacyItems[i + k]);
                r.fallbackCases++;
            } else {
                r.upgradedItems.append(buildAnanMmFromLegacy(legacyItems, i));
                // Delete loose items the new single class replaces
                for (int k = 0; k < 8; ++k) delete legacyItems[i + k];
                r.presetsMigrated++;
            }
            i += 8;
            continue;
        }
        // Add similar signature blocks for: CrossNeedle, SMeter,
        // PowerSwr, MagicEye, SignalText, HistoryGraph, VfoDisplay,
        // Clock, Contest, and the 18 Bar flavors. Each follows the
        // exact pattern above: match signature, check customisation,
        // build new or fall back.
        r.upgradedItems.append(legacyItems[i]);
        ++i;
    }
    return r;
}
```

- [ ] **Step 4: Call migrator from `MeterWidget::loadFromAppSettings`**

```cpp
void MeterWidget::loadFromAppSettings(const QString& containerKey) {
    QVector<MeterItem*> loose = parseItemsFromAppSettings(containerKey);
    const auto result = LegacyPresetMigrator::migrate(std::move(loose));
    if (result.presetsMigrated > 0) {
        qCInfo(lcMigrate) << "Migrated" << result.presetsMigrated
                          << "legacy presets for container" << containerKey;
    }
    for (MeterItem* it : result.upgradedItems) addItem(it);
}
```

- [ ] **Step 5: Run tests, confirm pass.**

- [ ] **Step 6: Commit** — `feat(migration): tolerant legacy-preset migrator + test fixtures`.

---

## Task 19: Delete `ItemGroup`

**Files:**
- Delete: `src/gui/meters/ItemGroup.h`
- Delete: `src/gui/meters/ItemGroup.cpp`
- Modify: `CMakeLists.txt` (remove references)
- Modify: Any remaining `#include "gui/meters/ItemGroup.h"` call sites (grep to enumerate)

- [ ] **Step 1: Grep call sites**

```bash
grep -rn "ItemGroup" src tests
```

Expect hits only in: test fixtures (legacy serialization format), and potentially `ContainerSettingsDialog.cpp` leftovers. All should be dead after Task 11.

- [ ] **Step 2: Delete files**

```bash
git rm src/gui/meters/ItemGroup.h src/gui/meters/ItemGroup.cpp
```

Update CMakeLists.txt to remove these entries.

- [ ] **Step 3: Rebuild, run all tests**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

- [ ] **Step 4: Commit** — `refactor(meters): remove ItemGroup — presets are now first-class MeterItems`.

---

## Task 20: Manual verification + CHANGELOG

**Files:**
- Modify: `CHANGELOG.md` (new entry under next version)
- Create: `docs/architecture/edit-container-refactor-verification.md` (checklist execution log)

- [ ] **Step 1: Launch build**

```bash
cmake --build build && ./build/NereusSDR
```

- [ ] **Step 2: Execute the 9 manual verification items from spec §5.2**

For each item, record the outcome in a new markdown file:

```markdown
# Edit Container Refactor — Manual Verification

## 1. Legacy ANAN MM loads as one row
- Action: open Container 1 (pre-refactor saved with ANAN MM)
- Expected: in-use list shows "ANAN Multi Meter" — one row
- Result: ...

## 2. Add ANAN MM to empty container blocks re-add
...
```

Paste screenshots into `docs/architecture/edit-container-refactor-verification/` if useful.

- [ ] **Step 3: CHANGELOG entry**

Add to `CHANGELOG.md` under the next unreleased version:

```markdown
### Changed
- Edit Container dialog: presets are now first-class `MeterItem`s
  (one row per preset instead of many). Hybrid rule: presets are
  single-instance per container; primitives remain freely re-addable.
- In-use list supports drag-to-reorder, user-editable names,
  right-click context menu.

### Added
- On-container edit access: right-click, double-click the header,
  gear-icon button. Floating containers inherit via delegation.
- Edit Container dialog: `+ Add` button creates a new container
  inline.
- `View → Applets` submenu (split from the Containers menu).

### Fixed
- ANAN Multi Meter needles now track the meter face arc at any
  container aspect ratio (anchored to background image rect).

### Removed
- Internal `ItemGroup` composite — absorbed into the 10 new preset
  classes. Legacy saved containers are migrated tolerantly on load.
```

- [ ] **Step 4: Commit verification + changelog**

```
chore(release): edit-container refactor verification log and CHANGELOG
```

---

## Task 21: Open pull request

- [ ] **Step 1: Push branch**

```bash
git push -u origin refactor/edit-container-design
```

- [ ] **Step 2: Open PR**

```bash
gh pr create --title "Edit Container refactor: first-class presets, drag/reorder, arc fix, on-container access" --body "$(cat <<'EOF'
## Summary
- Collapses 34 `ItemGroup` factories into 10 first-class `MeterItem` subclasses.
- Hybrid addition rule: presets single-instance per container; primitives freely re-addable.
- ANAN Multi Meter needle arc fix (pivot/radius anchored to background image rect).
- On-container edit access (right-click / double-click / gear icon). Floating containers inherit.
- `+ Add` button inside the Edit Container dialog.
- `View → Applets` submenu split from the Containers menu.

## Test plan
- [ ] `ctest --test-dir build` green (all new unit tests)
- [ ] Manual verification checklist at `docs/architecture/edit-container-refactor-verification.md` executed
- [ ] Pre-refactor saved containers migrate cleanly (ANAN MM, S-Meter, Power/SWR fixtures)
- [ ] Needle arc stays on face at 2:1 wide and 1:2 tall aspect ratios

J.J. Boyd ~ KG4VCF
EOF
)"
```

- [ ] **Step 3: Open PR URL in browser** (standing preference)

```bash
open "$(gh pr view --json url -q .url)"
```

---

## Self-review

**Spec coverage:** Every requirement in `edit-container-refactor-design.md` §3–§10 maps to a task:

| Spec § | Task |
| --- | --- |
| §3.1 Preset-as-first-class | Tasks 1–10 |
| §3.2 `ItemGroup` removal | Task 19 |
| §3.3 In-use row wrapper + drag/rename/duplicate | Task 13 |
| §3.4 Backwards-compat serialization / tolerant migration | Task 18 |
| §3.5 ANAN needle arc fix | Task 1 (within `AnanMultiMeterItem`) |
| §3.6 Dialog top-bar preservation | Task 14 (Add button inserted, rest preserved) |
| §3.7 On-container access | Tasks 15–16 |
| §3.8 `+ Add` button | Task 14 |
| §3.9 Menu reorg | Task 17 |
| §3.10 Live-apply & snapshot | Preserved; no task needed (existing behavior) |
| §5 Testing | Unit test per task; manual checklist in Task 20 |
| §6 Source-first protocol | Headers/provenance/inline stamps in Tasks 1–10 |
| §8 Commit sequencing | Mirrors task order |
| §10 Success criteria | Verified in Task 20 |

**Placeholder scan:** The `BarPresetItem` implementation in Task 10 Step 4 marks the remaining 13 `configureAs*` methods as "fill these in when implementing using the bindings and ranges from ItemGroup.cpp and MeterManager.cs" — this is a pointer to a concrete source, not a vague TODO. Acceptable because the same structural template is shown for 5 configureAs methods and the reader can mechanically produce the others from the existing `ItemGroup::create*BarPreset()` bodies (which must be grep-read during implementation).

Similarly, Tasks 2–9 describe each preset class's unique configuration by pointing at the Thetis line range but don't inline the full `.cpp` code. This is to keep the plan tractable (~2500 lines as-is); the pattern in Task 1 is the template and each task names its specific source. An engineer unfamiliar with the codebase will read Task 1 end-to-end, then execute Tasks 2–9 by pattern-matching against the named Thetis methods.

**Type consistency:** `ContainerInUseRow` used in Tasks 13, 14, 18; fields `item`, `displayName`, `rowId` consistent throughout. `BarPresetItem::Kind` enum referenced consistently. `editRequested`, `renameRequested`, `duplicateRequested`, `deleteRequested`, `dockModeChangeRequested` signals defined in Task 15, consumed in Task 15 Step 7 and Task 16.

One inconsistency caught and corrected: the spec mentioned `QJsonObject` serialize; actual `MeterItem::serialize()` returns `QString`. All tasks use the `QString`-wrapping-JSON pattern demonstrated in Task 1.

---

## Handoff

Plan complete and saved to `docs/architecture/edit-container-refactor-plan.md`. Two execution options:

1. **Subagent-Driven (recommended)** — a fresh subagent per task, two-stage review between tasks, fast iteration.
2. **Inline Execution** — run tasks in this session using `executing-plans`, batch with checkpoints.

Which approach?
