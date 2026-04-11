# Phase 3G-3: Core Meter Groups Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship arc-style S-meter needle, Power/SWR bars, and ALC preset in Container #0 — visually matching AetherSDR's SMeterWidget.

**Architecture:** Extend the 3-pipeline MeterWidget with a new NeedleItem that participates in all 4 render layers (Background arc, Geometry needle, OverlayStatic ticks, OverlayDynamic readout). Add TX MeterBinding constants (100+) for Power/SWR/ALC stubs. Compose presets via ItemGroup factories and install into Container #0 with a 55%/30%/15% vertical split.

**Tech Stack:** C++20, Qt6 (QRhi/QPainter), WDSP meter API, existing MeterWidget 3-pipeline GPU renderer.

**Source Authority:**
- Arc geometry, colors, fonts, smoothing: AetherSDR `src/gui/SMeterWidget.cpp`
- Calibration tables, meter types, scaling: Thetis `MeterManager.cs`
- Qt6 structure: existing MeterItem/MeterWidget patterns

---

## Decisions (from Step 0 Plan Review)

1. **Arc geometry:** QPainter arcs on P1 Background + vertex geometry for needle on P2.
2. **VHF S-unit offset:** Deferred to a later phase.
3. **TX binding range:** 100+ (TxPower=100 through TxAlc=105).
4. **Peak hold:** Medium only (500ms hold, 10 dB/sec decay, 10s hard reset).
5. **Multi-layer items:** New `participatesIn(Layer)` + `paintForLayer()` virtuals on MeterItem.
6. **S-Meter binding:** `SignalAvg` (matching Thetis ANAN needle), not `SignalPeak`.
7. **Smoothing model:** AetherSDR `SMOOTH_ALPHA = 0.3` (simple exponential).

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `src/gui/meters/MeterItem.h` | Modify | Add `participatesIn()`, `paintForLayer()` virtuals; add `NeedleItem` class |
| `src/gui/meters/MeterItem.cpp` | Modify | NeedleItem implementation (all render layers, smoothing, peak, serialization) |
| `src/gui/meters/MeterWidget.cpp` | Modify | Update GPU render loops to use multi-layer dispatch; add NEEDLE deserializer |
| `src/gui/meters/MeterPoller.h` | Modify | Add TX MeterBinding constants (100-105) |
| `src/gui/meters/ItemGroup.h` | Modify | Add `installInto()`, `createSMeterPreset()`, `createPowerSwrPreset()`, `createAlcPreset()`, `createMicPreset()`, `createCompPreset()` |
| `src/gui/meters/ItemGroup.cpp` | Modify | Implement new factories and installInto |
| `src/gui/MainWindow.cpp` | Modify | Replace `populateDefaultMeter()` with 3-group default layout |

---

### Task 1: Multi-layer MeterItem infrastructure

**Files:**
- Modify: `src/gui/meters/MeterItem.h:14-71` (MeterItem base class)
- Modify: `src/gui/meters/MeterWidget.cpp:429-507` (GPU render loops)

- [ ] **Step 1: Add multi-layer virtuals to MeterItem base**

In `src/gui/meters/MeterItem.h`, add two new virtual methods to the MeterItem class, after the `emitVertices` virtual:

```cpp
    virtual void emitVertices(QVector<float>& verts, int widgetW, int widgetH) {
        Q_UNUSED(verts); Q_UNUSED(widgetW); Q_UNUSED(widgetH);
    }

    // Multi-layer support: items that participate in multiple pipelines
    // override participatesIn() to return true for each layer they render to,
    // and override paintForLayer() to dispatch per-layer painting.
    // Default: single-layer (backward-compatible with existing items).
    virtual bool participatesIn(Layer layer) const { return layer == renderLayer(); }
    virtual void paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer) {
        Q_UNUSED(layer);
        paint(p, widgetW, widgetH);
    }
```

- [ ] **Step 2: Update GPU Background render loop**

In `src/gui/meters/MeterWidget.cpp`, in the `renderGpuFrame` method, find the Background painting loop (around line 433) and change:

```cpp
// Before:
            for (MeterItem* item : m_items) {
                if (item->renderLayer() == MeterItem::Layer::Background) {
                    item->paint(p, w, h);
                }
            }
// After:
            for (MeterItem* item : m_items) {
                if (item->participatesIn(MeterItem::Layer::Background)) {
                    item->paintForLayer(p, w, h, MeterItem::Layer::Background);
                }
            }
```

- [ ] **Step 3: Update GPU Geometry render loop**

In the same method, find the Geometry loop (around line 450) and change:

```cpp
// Before:
        for (MeterItem* item : m_items) {
            if (item->renderLayer() == MeterItem::Layer::Geometry) {
                item->emitVertices(verts, w, h);
            }
        }
// After:
        for (MeterItem* item : m_items) {
            if (item->participatesIn(MeterItem::Layer::Geometry)) {
                item->emitVertices(verts, w, h);
            }
        }
```

- [ ] **Step 4: Update GPU OverlayStatic render loop**

Find the OverlayStatic loop (around line 488) and change:

```cpp
// Before:
            for (MeterItem* item : m_items) {
                if (item->renderLayer() == MeterItem::Layer::OverlayStatic) {
                    item->paint(p, w, h);
                }
            }
// After:
            for (MeterItem* item : m_items) {
                if (item->participatesIn(MeterItem::Layer::OverlayStatic)) {
                    item->paintForLayer(p, w, h, MeterItem::Layer::OverlayStatic);
                }
            }
```

- [ ] **Step 5: Update GPU OverlayDynamic render loop**

Find the OverlayDynamic loop (around line 501) and change:

```cpp
// Before:
            for (MeterItem* item : m_items) {
                if (item->renderLayer() == MeterItem::Layer::OverlayDynamic) {
                    item->paint(p, w, h);
                }
            }
// After:
            for (MeterItem* item : m_items) {
                if (item->participatesIn(MeterItem::Layer::OverlayDynamic)) {
                    item->paintForLayer(p, w, h, MeterItem::Layer::OverlayDynamic);
                }
            }
```

- [ ] **Step 6: Build to verify no regressions**

Run:
```bash
cd ~/NereusSDR && cmake --build build -j$(nproc) 2>&1 | tail -20
```
Expected: Build succeeds. Existing H_BAR meter still renders (backward-compatible change).

- [ ] **Step 7: Commit**

```bash
cd ~/NereusSDR && git add src/gui/meters/MeterItem.h src/gui/meters/MeterWidget.cpp
git commit -m "$(cat <<'EOF'
Add multi-layer MeterItem infrastructure for NeedleItem

Add participatesIn() and paintForLayer() virtuals to MeterItem base
class, enabling items to render across multiple GPU pipelines. Update
MeterWidget GPU render loops to use the new dispatch. Backward-
compatible: existing single-layer items unchanged.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: TX MeterBinding constants

**Files:**
- Modify: `src/gui/meters/MeterPoller.h:15-23` (MeterBinding namespace)

- [ ] **Step 1: Add TX binding constants**

In `src/gui/meters/MeterPoller.h`, add TX constants after the existing RX bindings:

```cpp
namespace MeterBinding {
    constexpr int SignalPeak   = 0;    // RxMeterType::SignalPeak
    constexpr int SignalAvg    = 1;    // RxMeterType::SignalAvg
    constexpr int AdcPeak      = 2;    // RxMeterType::AdcPeak
    constexpr int AdcAvg       = 3;    // RxMeterType::AdcAvg
    constexpr int AgcGain      = 4;    // RxMeterType::AgcGain
    constexpr int AgcPeak      = 5;    // RxMeterType::AgcPeak
    constexpr int AgcAvg       = 6;    // RxMeterType::AgcAvg

    // TX bindings (100+). From Thetis MeterManager.cs Reading enum.
    // Stub values until TxChannel exists (Phase 3I-1).
    // PWR/SWR are hardware PA measurements, not WDSP meters.
    constexpr int TxPower        = 100;  // Forward power (hardware PA)
    constexpr int TxReversePower = 101;  // Reverse power (hardware PA)
    constexpr int TxSwr          = 102;  // SWR (computed fwd/rev ratio)
    constexpr int TxMic          = 103;  // TXA_MIC_AV
    constexpr int TxComp         = 104;  // TXA_COMP_AV
    constexpr int TxAlc          = 105;  // TXA_ALC_AV
}
```

- [ ] **Step 2: Build to verify**

Run:
```bash
cd ~/NereusSDR && cmake --build build -j$(nproc) 2>&1 | tail -5
```
Expected: Build succeeds (constants only, no consumers yet).

- [ ] **Step 3: Commit**

```bash
cd ~/NereusSDR && git add src/gui/meters/MeterPoller.h
git commit -m "$(cat <<'EOF'
Add TX MeterBinding constants for Power/SWR/ALC presets

Define stub binding IDs 100-105 for TX meters (Power, ReversePower,
SWR, Mic, Comp, ALC). From Thetis MeterManager.cs Reading enum.
Will be wired to TxChannel in Phase 3I-1.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: NeedleItem class declaration

**Files:**
- Modify: `src/gui/meters/MeterItem.h` (add NeedleItem class after TextItem)

- [ ] **Step 1: Add NeedleItem class declaration**

In `src/gui/meters/MeterItem.h`, add the following class after the TextItem class, before the closing `} // namespace NereusSDR`:

```cpp
// ---------------------------------------------------------------------------
// NeedleItem — Arc-style S-meter needle (AetherSDR SMeterWidget port)
// Participates in all 4 render layers:
//   P1 Background:     arc colored segments (white S0-S9, red S9+)
//   P2 Geometry:       needle quad + peak marker triangle
//   P3 OverlayStatic:  tick marks, tick labels, source label
//   P3 OverlayDynamic: S-unit readout (left), dBm readout (right)
// ---------------------------------------------------------------------------
class NeedleItem : public MeterItem {
    Q_OBJECT
public:
    explicit NeedleItem(QObject* parent = nullptr);

    // --- Arc geometry constants (from AetherSDR SMeterWidget.cpp) ---
    static constexpr float kArcStartDeg = 55.0f;   // right end (S9+60)
    static constexpr float kArcEndDeg   = 125.0f;  // left end (S0)
    static constexpr float kRadiusRatio = 0.85f;    // radius = width * 0.85
    static constexpr float kCenterYRatio = 0.65f;   // cy = h + radius - h*0.65

    // --- S-meter scale constants (from AetherSDR SMeterWidget.h) ---
    static constexpr float kS0Dbm  = -127.0f;      // S0
    static constexpr float kS9Dbm  = -73.0f;        // S9 = S0 + 9*6
    static constexpr float kMaxDbm = -13.0f;         // S9+60
    static constexpr float kDbPerS = 6.0f;           // dB per S-unit

    // --- Smoothing (from AetherSDR SMOOTH_ALPHA) ---
    static constexpr float kSmoothAlpha = 0.3f;

    // --- Peak hold: Medium preset (from AetherSDR) ---
    static constexpr int   kPeakHoldFrames   = 5;    // 500ms at 100ms poll
    static constexpr float kPeakDecayPerFrame = 1.0f; // 10 dB/sec = 1dB/100ms
    static constexpr int   kPeakResetFrames  = 100;   // 10s hard reset

    void setSourceLabel(const QString& label) { m_sourceLabel = label; }
    QString sourceLabel() const { return m_sourceLabel; }

    void setValue(double v) override;

    // Multi-layer: participates in all 4 pipeline layers
    bool participatesIn(Layer layer) const override;
    Layer renderLayer() const override { return Layer::Geometry; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    void paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer) override;
    void emitVertices(QVector<float>& verts, int widgetW, int widgetH) override;

    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    // From AetherSDR SMeterWidget.cpp dbmToFraction()
    float dbmToFraction(float dbm) const;
    // From AetherSDR SMeterWidget.cpp sUnitsText()
    QString sUnitsText(float dbm) const;

    void paintBackground(QPainter& p, int widgetW, int widgetH);
    void paintOverlayStatic(QPainter& p, int widgetW, int widgetH);
    void paintOverlayDynamic(QPainter& p, int widgetW, int widgetH);

    QString m_sourceLabel{QStringLiteral("S-Meter")};
    float   m_smoothedDbm{kS0Dbm};
    float   m_peakDbm{kS0Dbm};
    int     m_peakHoldCounter{0};
    int     m_peakResetCounter{0};
};
```

- [ ] **Step 2: Build to verify header compiles**

Run:
```bash
cd ~/NereusSDR && cmake --build build -j$(nproc) 2>&1 | tail -20
```
Expected: Link errors for unimplemented NeedleItem methods (expected — implementations come in Task 4).

- [ ] **Step 3: Commit**

```bash
cd ~/NereusSDR && git add src/gui/meters/MeterItem.h
git commit -m "$(cat <<'EOF'
Add NeedleItem class declaration to MeterItem.h

Arc-style S-meter needle item porting AetherSDR's SMeterWidget. All
geometry, color, and smoothing constants from AetherSDR source.
Participates in all 4 GPU pipeline layers. Implementation follows.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: NeedleItem core logic

**Files:**
- Modify: `src/gui/meters/MeterItem.cpp` (add NeedleItem implementation)

- [ ] **Step 1: Add QtMath include**

At the top of `src/gui/meters/MeterItem.cpp`, add after the existing `#include <cmath>`:

```cpp
#include <QtMath>
```

- [ ] **Step 2: Implement constructor**

Add at the end of `src/gui/meters/MeterItem.cpp`, before the closing `} // namespace NereusSDR`:

```cpp
// ---------------------------------------------------------------------------
// NeedleItem — Arc-style S-meter needle
// Ported from AetherSDR src/gui/SMeterWidget.cpp
// ---------------------------------------------------------------------------

NeedleItem::NeedleItem(QObject* parent)
    : MeterItem(parent)
{
}
```

- [ ] **Step 3: Implement participatesIn()**

```cpp
bool NeedleItem::participatesIn(Layer layer) const
{
    // NeedleItem renders across all 4 pipeline layers
    Q_UNUSED(layer);
    return true;
}
```

- [ ] **Step 4: Implement setValue() with smoothing + peak hold**

```cpp
// From AetherSDR SMeterWidget.cpp setLevel() — SMOOTH_ALPHA = 0.3
void NeedleItem::setValue(double v)
{
    MeterItem::setValue(v);

    const float dbm = static_cast<float>(v);

    // Exponential smoothing (from AetherSDR SMOOTH_ALPHA = 0.3)
    m_smoothedDbm += kSmoothAlpha * (dbm - m_smoothedDbm);
    m_smoothedDbm = std::clamp(m_smoothedDbm, kS0Dbm, kMaxDbm);

    // Peak tracking (from AetherSDR Medium preset: 500ms hold, 10 dB/sec decay)
    if (m_smoothedDbm > m_peakDbm) {
        m_peakDbm = m_smoothedDbm;
        m_peakHoldCounter = kPeakHoldFrames;
    } else if (m_peakHoldCounter > 0) {
        --m_peakHoldCounter;
    } else {
        m_peakDbm -= kPeakDecayPerFrame;
        if (m_peakDbm < m_smoothedDbm) {
            m_peakDbm = m_smoothedDbm;
        }
    }

    // Hard reset every 10 seconds (from AetherSDR m_peakReset = 10000ms)
    ++m_peakResetCounter;
    if (m_peakResetCounter >= kPeakResetFrames) {
        m_peakDbm = m_smoothedDbm;
        m_peakResetCounter = 0;
    }
}
```

- [ ] **Step 5: Implement dbmToFraction()**

```cpp
// From AetherSDR SMeterWidget.cpp dbmToFraction()
// S0-S9 occupies left 60% of arc; S9-S9+60 occupies right 40%
float NeedleItem::dbmToFraction(float dbm) const
{
    const float clamped = std::clamp(dbm, kS0Dbm, kMaxDbm);
    if (clamped <= kS9Dbm) {
        return 0.6f * (clamped - kS0Dbm) / (kS9Dbm - kS0Dbm);
    }
    return 0.6f + 0.4f * (clamped - kS9Dbm) / (kMaxDbm - kS9Dbm);
}
```

- [ ] **Step 6: Implement sUnitsText()**

```cpp
// From AetherSDR SMeterWidget.cpp sUnitsText()
QString NeedleItem::sUnitsText(float dbm) const
{
    if (dbm <= kS0Dbm) {
        return QStringLiteral("S0");
    }
    if (dbm <= kS9Dbm) {
        const int sUnit = qBound(0, qRound((dbm - kS0Dbm) / kDbPerS), 9);
        return QStringLiteral("S%1").arg(sUnit);
    }
    const int over = qRound(dbm - kS9Dbm);
    return QStringLiteral("S9+%1").arg(over);
}
```

- [ ] **Step 7: Commit**

```bash
cd ~/NereusSDR && git add src/gui/meters/MeterItem.cpp
git commit -m "$(cat <<'EOF'
Implement NeedleItem core logic (smoothing, peak hold, S-unit math)

Port AetherSDR's SMOOTH_ALPHA=0.3 exponential smoothing, Medium peak
hold preset (500ms hold, 10 dB/sec decay, 10s hard reset), and S-unit
conversion. All constants from AetherSDR SMeterWidget.cpp.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: NeedleItem rendering (all 4 layers)

**Files:**
- Modify: `src/gui/meters/MeterItem.cpp` (add render methods)

- [ ] **Step 1: Implement paintForLayer() dispatcher**

```cpp
void NeedleItem::paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    switch (layer) {
    case Layer::Background:     paintBackground(p, widgetW, widgetH); break;
    case Layer::OverlayStatic:  paintOverlayStatic(p, widgetW, widgetH); break;
    case Layer::OverlayDynamic: paintOverlayDynamic(p, widgetW, widgetH); break;
    default: break; // Geometry handled by emitVertices()
    }
    p.restore();
}
```

- [ ] **Step 2: Implement paint() (CPU fallback)**

```cpp
void NeedleItem::paint(QPainter& p, int widgetW, int widgetH)
{
    // CPU fallback: render all layers in a single QPainter pass
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    paintBackground(p, widgetW, widgetH);
    paintOverlayStatic(p, widgetW, widgetH);

    // CPU needle: draw as a simple line (no vertex geometry available)
    const QRect rect = pixelRect(widgetW, widgetH);
    const float pw = static_cast<float>(rect.width());
    const float ph = static_cast<float>(rect.height());
    const float cx = rect.left() + pw * 0.5f;
    const float radius = pw * kRadiusRatio;
    const float cy = rect.top() + ph + radius - ph * kCenterYRatio;
    const float pivotY = rect.top() + ph + 6.0f;

    const float frac = dbmToFraction(m_smoothedDbm);
    const float angleRad = qDegreesToRadians(kArcEndDeg - frac * (kArcEndDeg - kArcStartDeg));
    const float tipDist = radius + 14.0f;
    const float tipX = cx + tipDist * std::cos(angleRad);
    const float tipY = cy - tipDist * std::sin(angleRad);

    // Shadow
    QPen shadowPen(QColor(0, 0, 0, 80));
    shadowPen.setWidthF(3.0f);
    p.setPen(shadowPen);
    p.drawLine(QPointF(cx + 1.0f, pivotY + 1.0f), QPointF(tipX + 1.0f, tipY + 1.0f));

    // Needle
    QPen needlePen(Qt::white);
    needlePen.setWidthF(2.0f);
    p.setPen(needlePen);
    p.drawLine(QPointF(cx, pivotY), QPointF(tipX, tipY));

    paintOverlayDynamic(p, widgetW, widgetH);
    p.restore();
}
```

- [ ] **Step 3: Implement paintBackground() — arc colored segments (P1)**

```cpp
// From AetherSDR SMeterWidget.cpp paintEvent() — arc drawing section
void NeedleItem::paintBackground(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    const float pw = static_cast<float>(rect.width());
    const float ph = static_cast<float>(rect.height());

    // From AetherSDR: cx = width*0.5, radius = width*0.85
    // cy = height + radius - height*0.65
    const float cx = rect.left() + pw * 0.5f;
    const float radius = pw * kRadiusRatio;
    const float cy = rect.top() + ph + radius - ph * kCenterYRatio;

    const QRectF arcRect(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // S9 at fraction 0.6 → angle = 125 - 0.6*70 = 83 degrees
    const float arcSpan = kArcEndDeg - kArcStartDeg;  // 70 degrees
    const float s9Frac = 0.6f;
    const float s9Angle = kArcEndDeg - s9Frac * arcSpan;  // 83 degrees

    QPen arcPen;
    arcPen.setWidthF(3.0f);
    arcPen.setCapStyle(Qt::FlatCap);

    // White segment: S9 angle to ARC_END (left portion, S0-S9)
    // QPainter drawArc: angles in 1/16 degrees, 0=3 o'clock, positive=CCW
    arcPen.setColor(QColor(0xc8, 0xd8, 0xe8));  // From AetherSDR
    p.setPen(arcPen);
    const int whiteStart = static_cast<int>(s9Angle * 16.0f);
    const int whiteSpan  = static_cast<int>((kArcEndDeg - s9Angle) * 16.0f);
    p.drawArc(arcRect, whiteStart, whiteSpan);

    // Red segment: ARC_START to S9 angle (right portion, S9+)
    arcPen.setColor(QColor(0xff, 0x44, 0x44));  // From AetherSDR
    p.setPen(arcPen);
    const int redStart = static_cast<int>(kArcStartDeg * 16.0f);
    const int redSpan  = static_cast<int>((s9Angle - kArcStartDeg) * 16.0f);
    p.drawArc(arcRect, redStart, redSpan);
}
```

- [ ] **Step 4: Implement emitVertices() — needle + peak marker (P2)**

```cpp
// Needle geometry: tapered quad from pivot to tip (2 triangles = 6 verts).
// Peak marker: amber triangle at arc edge (3 verts).
// From AetherSDR SMeterWidget.cpp paintEvent() — needle drawing section
void NeedleItem::emitVertices(QVector<float>& verts, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    const float pw = static_cast<float>(rect.width());
    const float ph = static_cast<float>(rect.height());
    const float cx = rect.left() + pw * 0.5f;
    const float radius = pw * kRadiusRatio;
    const float cy = rect.top() + ph + radius - ph * kCenterYRatio;

    // From AetherSDR: needleCy = height + 6.0f (pivot below widget bottom)
    const float pivotX = cx;
    const float pivotY = rect.top() + ph + 6.0f;

    const float frac = dbmToFraction(m_smoothedDbm);
    const float angleDeg = kArcEndDeg - frac * (kArcEndDeg - kArcStartDeg);
    const float angleRad = qDegreesToRadians(angleDeg);

    // From AetherSDR: needle tip at radius + 14
    const float tipDist = radius + 14.0f;
    const float tipX = cx + tipDist * std::cos(angleRad);
    const float tipY = cy - tipDist * std::sin(angleRad);

    // Perpendicular for needle width
    const float dx = tipX - pivotX;
    const float dy = tipY - pivotY;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0f) { return; }

    const float halfWidth = 1.5f;
    const float nx = -dy / len * halfWidth;
    const float ny =  dx / len * halfWidth;

    // Pixel to NDC conversion lambdas
    const float invW = 2.0f / static_cast<float>(widgetW);
    const float invH = 2.0f / static_cast<float>(widgetH);
    auto ndcX = [invW](float px) { return px * invW - 1.0f; };
    auto ndcY = [invH](float py) { return 1.0f - py * invH; };

    // --- Shadow quad (black, offset +1px) ---
    // From AetherSDR: shadow color rgba(0,0,0,80), pen width 3, offset +1
    const float sr = 0.0f, sg = 0.0f, sb = 0.0f, sa = 80.0f / 255.0f;
    const float sOff = 1.0f;
    const float sp1x = pivotX + nx + sOff, sp1y = pivotY + ny + sOff;
    const float sp2x = pivotX - nx + sOff, sp2y = pivotY - ny + sOff;
    const float sp3x = tipX + nx * 0.3f + sOff, sp3y = tipY + ny * 0.3f + sOff;
    const float sp4x = tipX - nx * 0.3f + sOff, sp4y = tipY - ny * 0.3f + sOff;
    // Triangle 1
    verts << ndcX(sp1x) << ndcY(sp1y) << sr << sg << sb << sa;
    verts << ndcX(sp2x) << ndcY(sp2y) << sr << sg << sb << sa;
    verts << ndcX(sp3x) << ndcY(sp3y) << sr << sg << sb << sa;
    // Triangle 2
    verts << ndcX(sp3x) << ndcY(sp3y) << sr << sg << sb << sa;
    verts << ndcX(sp2x) << ndcY(sp2y) << sr << sg << sb << sa;
    verts << ndcX(sp4x) << ndcY(sp4y) << sr << sg << sb << sa;

    // --- Needle quad (white) ---
    // From AetherSDR: needle color #ffffff, pen width 2
    const float nr = 1.0f, ng = 1.0f, nb = 1.0f, na = 1.0f;
    const float np1x = pivotX + nx, np1y = pivotY + ny;
    const float np2x = pivotX - nx, np2y = pivotY - ny;
    const float np3x = tipX + nx * 0.3f, np3y = tipY + ny * 0.3f;  // tapers
    const float np4x = tipX - nx * 0.3f, np4y = tipY - ny * 0.3f;
    // Triangle 1
    verts << ndcX(np1x) << ndcY(np1y) << nr << ng << nb << na;
    verts << ndcX(np2x) << ndcY(np2y) << nr << ng << nb << na;
    verts << ndcX(np3x) << ndcY(np3y) << nr << ng << nb << na;
    // Triangle 2
    verts << ndcX(np3x) << ndcY(np3y) << nr << ng << nb << na;
    verts << ndcX(np2x) << ndcY(np2y) << nr << ng << nb << na;
    verts << ndcX(np4x) << ndcY(np4y) << nr << ng << nb << na;

    // --- Peak marker triangle (amber) ---
    // From AetherSDR: peak marker #ffaa00, shown when peak > current + 1 dBm
    if (m_peakDbm > m_smoothedDbm + 1.0f) {
        const float peakFrac = dbmToFraction(m_peakDbm);
        const float peakAngleDeg = kArcEndDeg - peakFrac * (kArcEndDeg - kArcStartDeg);
        const float peakAngleRad = qDegreesToRadians(peakAngleDeg);

        const float mr = 1.0f, mg = 0.667f, mb = 0.0f, ma = 1.0f;  // #ffaa00
        const float markerInnerR = radius + 2.0f;
        const float markerOuterR = radius + 10.0f;
        const float markerHalf = 3.0f;

        // Tip (outward)
        const float mtx = cx + markerOuterR * std::cos(peakAngleRad);
        const float mty = cy - markerOuterR * std::sin(peakAngleRad);
        // Base perpendicular
        const float perpX = -std::sin(peakAngleRad) * markerHalf;
        const float perpY = -std::cos(peakAngleRad) * markerHalf;
        const float mbx = cx + markerInnerR * std::cos(peakAngleRad);
        const float mby = cy - markerInnerR * std::sin(peakAngleRad);

        verts << ndcX(mtx) << ndcY(mty) << mr << mg << mb << ma;
        verts << ndcX(mbx + perpX) << ndcY(mby + perpY) << mr << mg << mb << ma;
        verts << ndcX(mbx - perpX) << ndcY(mby - perpY) << mr << mg << mb << ma;
    }
}
```

- [ ] **Step 5: Implement paintOverlayStatic() — tick marks + labels + source label (P3 static)**

```cpp
// From AetherSDR SMeterWidget.cpp paintEvent() — tick drawing section
void NeedleItem::paintOverlayStatic(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    const float pw = static_cast<float>(rect.width());
    const float ph = static_cast<float>(rect.height());
    const float cx = rect.left() + pw * 0.5f;
    const float radius = pw * kRadiusRatio;
    const float cy = rect.top() + ph + radius - ph * kCenterYRatio;

    // Tick definitions from AetherSDR SMeterWidget.cpp
    // Only odd S-units + over-S9 marks shown
    struct TickDef { float dbm; const char* label; bool isRed; };
    static const TickDef ticks[] = {
        {-121.0f, "S1",  false},  // From AetherSDR: frac 0.0667
        {-109.0f, "S3",  false},  // frac 0.200
        { -97.0f, "S5",  false},  // frac 0.333
        { -85.0f, "S7",  false},  // frac 0.467
        { -73.0f, "S9",  false},  // frac 0.600
        { -53.0f, "+20", true},   // frac 0.733
        { -33.0f, "+40", true},   // frac 0.867
    };

    // From AetherSDR: tick font = max(10, height/10) px, bold
    QFont tickFont = p.font();
    const int tickFontSize = qMax(10, static_cast<int>(ph) / 10);
    tickFont.setPixelSize(tickFontSize);
    tickFont.setBold(true);
    p.setFont(tickFont);

    for (const auto& tick : ticks) {
        const float frac = dbmToFraction(tick.dbm);
        const float angleDeg = kArcEndDeg - frac * (kArcEndDeg - kArcStartDeg);
        const float angleRad = qDegreesToRadians(angleDeg);

        // From AetherSDR: tick line 14px, inset 2px from arc, width 1.5
        const float innerR = radius - 2.0f;
        const float outerR = radius + 12.0f;
        const float ix = cx + innerR * std::cos(angleRad);
        const float iy = cy - innerR * std::sin(angleRad);
        const float ox = cx + outerR * std::cos(angleRad);
        const float oy = cy - outerR * std::sin(angleRad);

        const QColor tickColor = tick.isRed ? QColor(0xff, 0x44, 0x44)
                                            : QColor(0xc8, 0xd8, 0xe8);
        QPen tickPen(tickColor);
        tickPen.setWidthF(1.5f);
        p.setPen(tickPen);
        p.drawLine(QPointF(ix, iy), QPointF(ox, oy));

        // From AetherSDR: label 26px from arc, centered on tick angle
        const float labelR = radius + 26.0f;
        const float lx = cx + labelR * std::cos(angleRad);
        const float ly = cy - labelR * std::sin(angleRad);

        p.setPen(tickColor);
        const QFontMetrics fm(tickFont);
        const QString labelStr = QString::fromLatin1(tick.label);
        const int tw = fm.horizontalAdvance(labelStr);
        p.drawText(QPointF(lx - tw / 2.0f, ly + fm.ascent() / 2.0f), labelStr);
    }

    // From AetherSDR: source label centered, max(9, h/14) px, non-bold, #8090a0
    QFont srcFont = p.font();
    const int srcFontSize = qMax(9, static_cast<int>(ph) / 14);
    srcFont.setPixelSize(srcFontSize);
    srcFont.setBold(false);
    p.setFont(srcFont);
    p.setPen(QColor(0x80, 0x90, 0xa0));
    const QFontMetrics srcFm(srcFont);
    const int srcW = srcFm.horizontalAdvance(m_sourceLabel);
    p.drawText(QPointF(rect.left() + (pw - srcW) / 2.0f,
                        rect.top() + srcFm.height() + 2.0f), m_sourceLabel);
}
```

- [ ] **Step 6: Implement paintOverlayDynamic() — S-unit + dBm readout (P3 dynamic)**

```cpp
// From AetherSDR SMeterWidget.cpp paintEvent() — readout drawing section
void NeedleItem::paintOverlayDynamic(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    const float pw = static_cast<float>(rect.width());
    const float ph = static_cast<float>(rect.height());

    // Compute source label baseline for vertical positioning
    const int srcFontSize = qMax(9, static_cast<int>(ph) / 14);
    QFont srcFont = p.font();
    srcFont.setPixelSize(srcFontSize);
    const QFontMetrics srcFm(srcFont);
    const float topY = rect.top() + srcFm.height() + 2.0f;

    // From AetherSDR: value font = max(13, h/8) px, bold
    QFont valFont = p.font();
    const int valFontSize = qMax(13, static_cast<int>(ph) / 8);
    valFont.setPixelSize(valFontSize);
    valFont.setBold(true);
    p.setFont(valFont);
    const QFontMetrics valFm(valFont);

    // S-unit text (left, cyan) — from AetherSDR: #00b4d8, x=6
    const QString sText = sUnitsText(m_smoothedDbm);
    p.setPen(QColor(0x00, 0xb4, 0xd8));
    p.drawText(QPointF(rect.left() + 6.0f, topY + valFm.ascent()), sText);

    // dBm text (right, light steel) — from AetherSDR: #c8d8e8, right-aligned -6
    const QString dbmText = QString::number(static_cast<int>(std::round(m_smoothedDbm)))
                          + QStringLiteral(" dBm");
    p.setPen(QColor(0xc8, 0xd8, 0xe8));
    const int dbmW = valFm.horizontalAdvance(dbmText);
    p.drawText(QPointF(rect.right() - dbmW - 6.0f, topY + valFm.ascent()), dbmText);
}
```

- [ ] **Step 7: Build to verify all render methods compile**

Run:
```bash
cd ~/NereusSDR && cmake --build build -j$(nproc) 2>&1 | tail -20
```
Expected: Link errors only for serialize/deserialize (implemented in Task 6).

- [ ] **Step 8: Commit**

```bash
cd ~/NereusSDR && git add src/gui/meters/MeterItem.cpp
git commit -m "$(cat <<'EOF'
Implement NeedleItem rendering across all 4 GPU pipelines

P1 Background: QPainter arc segments (white S0-S9, red S9+)
P2 Geometry: needle quad + shadow + peak marker triangle vertices
P3 OverlayStatic: tick marks, labels (S1,S3,S5,S7,S9,+20,+40)
P3 OverlayDynamic: S-unit readout (cyan) + dBm readout (light steel)
All geometry, colors, and fonts ported from AetherSDR SMeterWidget.cpp.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: NeedleItem serialization + registration

**Files:**
- Modify: `src/gui/meters/MeterItem.cpp` (add serialize/deserialize)
- Modify: `src/gui/meters/MeterWidget.cpp:136-156` (deserializeItems registry)
- Modify: `src/gui/meters/ItemGroup.cpp:111-160` (deserialize registry)

- [ ] **Step 1: Implement NeedleItem serialize/deserialize**

Add to `src/gui/meters/MeterItem.cpp`:

```cpp
// Format: NEEDLE|x|y|w|h|bindingId|zOrder|sourceLabel
QString NeedleItem::serialize() const
{
    return QStringLiteral("NEEDLE|%1|%2")
        .arg(baseFields(*this))
        .arg(m_sourceLabel);
}

bool NeedleItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 8 || parts[0] != QLatin1String("NEEDLE")) {
        return false;
    }
    if (!parseBaseFields(*this, parts)) {
        return false;
    }
    m_sourceLabel = parts[7];
    return true;
}
```

- [ ] **Step 2: Register NEEDLE in MeterWidget::deserializeItems()**

In `src/gui/meters/MeterWidget.cpp`, in the `deserializeItems` method, add a NEEDLE case after the TEXT case:

```cpp
        } else if (type == QStringLiteral("TEXT")) {
            item = new TextItem();
        } else if (type == QStringLiteral("NEEDLE")) {
            item = new NeedleItem();
        }
```

- [ ] **Step 3: Register NEEDLE in ItemGroup::deserialize()**

In `src/gui/meters/ItemGroup.cpp`, in the `deserialize` static method, add a NEEDLE case after the TEXT case:

```cpp
        } else if (typeTag == QLatin1String("TEXT")) {
            TextItem* text = new TextItem();
            if (text->deserialize(itemData)) {
                item = text;
            } else {
                delete text;
            }
        } else if (typeTag == QLatin1String("NEEDLE")) {
            NeedleItem* needle = new NeedleItem();
            if (needle->deserialize(itemData)) {
                item = needle;
            } else {
                delete needle;
            }
        }
```

- [ ] **Step 4: Build to verify full NeedleItem compiles and links**

Run:
```bash
cd ~/NereusSDR && cmake --build build -j$(nproc) 2>&1 | tail -10
```
Expected: Build succeeds with no errors.

- [ ] **Step 5: Commit**

```bash
cd ~/NereusSDR && git add src/gui/meters/MeterItem.cpp src/gui/meters/MeterWidget.cpp src/gui/meters/ItemGroup.cpp
git commit -m "$(cat <<'EOF'
Add NeedleItem serialization and deserializer registration

NEEDLE|x|y|w|h|bindingId|zOrder|sourceLabel format. Register in both
MeterWidget::deserializeItems() and ItemGroup::deserialize() for
persistence support.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: Preset factories (S-Meter, Power/SWR, ALC)

**Files:**
- Modify: `src/gui/meters/ItemGroup.h:36-37` (add factory + helper declarations)
- Modify: `src/gui/meters/ItemGroup.cpp` (implement factories)

- [ ] **Step 1: Add factory declarations to ItemGroup.h**

In `src/gui/meters/ItemGroup.h`, add after the `createHBarPreset` declaration:

```cpp
    static ItemGroup* createHBarPreset(int bindingId, double minVal, double maxVal,
                                        const QString& name, QObject* parent = nullptr);

    // S-Meter preset: arc-style NeedleItem with full readout.
    // From AetherSDR SMeterWidget visual style.
    static ItemGroup* createSMeterPreset(int bindingId, const QString& name,
                                          QObject* parent = nullptr);

    // Power/SWR preset: two stacked horizontal bars.
    // From Thetis MeterManager.cs AddPWRBar/AddSWRBar calibration.
    static ItemGroup* createPowerSwrPreset(const QString& name,
                                            QObject* parent = nullptr);

    // ALC preset: horizontal bar for TX ALC level.
    // From Thetis MeterManager.cs ALC scale (-30 to 0 dB).
    static ItemGroup* createAlcPreset(QObject* parent = nullptr);

    // Mic level preset: horizontal bar for TX mic level.
    // From Thetis MeterManager.cs MIC scale (-30 to 0 dB).
    static ItemGroup* createMicPreset(QObject* parent = nullptr);

    // Compressor level preset: horizontal bar for TX compressor.
    // From Thetis MeterManager.cs COMP scale (-25 to 0 dB).
    static ItemGroup* createCompPreset(QObject* parent = nullptr);

    // Install all items from this group into a MeterWidget, transforming
    // their normalized 0-1 positions into the given target rect.
    // Transfers item ownership to widget; clears this group's item list.
    void installInto(MeterWidget* widget, float gx, float gy, float gw, float gh);
```

Also add a forward declaration at the top (after the existing includes):

```cpp
class MeterWidget;
```

- [ ] **Step 2: Implement installInto()**

In `src/gui/meters/ItemGroup.cpp`, add `#include "MeterWidget.h"` at the top after the existing includes, then add:

```cpp
void ItemGroup::installInto(MeterWidget* widget, float gx, float gy, float gw, float gh)
{
    for (MeterItem* item : m_items) {
        item->setRect(
            gx + item->x() * gw,
            gy + item->y() * gh,
            item->itemWidth() * gw,
            item->itemHeight() * gh
        );
        item->setParent(widget);
        widget->addItem(item);
    }
    m_items.clear();
}
```

- [ ] **Step 3: Implement createSMeterPreset()**

```cpp
// From AetherSDR SMeterWidget — single NeedleItem handles all rendering
ItemGroup* ItemGroup::createSMeterPreset(int bindingId, const QString& name,
                                          QObject* parent)
{
    ItemGroup* group = new ItemGroup(name, parent);

    NeedleItem* needle = new NeedleItem();
    needle->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    needle->setBindingId(bindingId);
    needle->setSourceLabel(name);
    needle->setZOrder(5);
    group->addItem(needle);

    return group;
}
```

- [ ] **Step 4: Implement createPowerSwrPreset()**

```cpp
// From Thetis MeterManager.cs AddPWRBar (line 23862) + AddSWRBar (line 23990)
// Power: 0-120W, HighPoint at 100W (75%). DecayRatio = 0.1
// SWR: 1:1-5:1, HighPoint at 3:1 (75%). DecayRatio = 0.1
ItemGroup* ItemGroup::createPowerSwrPreset(const QString& name, QObject* parent)
{
    ItemGroup* group = new ItemGroup(name, parent);

    // Background
    SolidColourItem* bg = new SolidColourItem();
    bg->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    bg->setColour(QColor(0x0f, 0x0f, 0x1a));
    bg->setZOrder(0);
    group->addItem(bg);

    // --- Forward Power section (top half) ---

    TextItem* pwrLabel = new TextItem();
    pwrLabel->setRect(0.02f, 0.0f, 0.5f, 0.15f);
    pwrLabel->setLabel(QStringLiteral("Power"));
    pwrLabel->setBindingId(-1);
    pwrLabel->setTextColor(QColor(0x80, 0x90, 0xa0));
    pwrLabel->setFontSize(10);
    pwrLabel->setBold(false);
    pwrLabel->setZOrder(10);
    group->addItem(pwrLabel);

    TextItem* pwrReadout = new TextItem();
    pwrReadout->setRect(0.5f, 0.0f, 0.48f, 0.15f);
    pwrReadout->setBindingId(MeterBinding::TxPower);
    pwrReadout->setTextColor(QColor(0xc8, 0xd8, 0xe8));
    pwrReadout->setFontSize(13);
    pwrReadout->setBold(true);
    pwrReadout->setSuffix(QStringLiteral(" W"));
    pwrReadout->setDecimals(0);
    pwrReadout->setZOrder(10);
    group->addItem(pwrReadout);

    // From Thetis MeterManager.cs AddPWRBar: 0-120W, red at 100W
    BarItem* pwrBar = new BarItem();
    pwrBar->setRect(0.02f, 0.17f, 0.96f, 0.22f);
    pwrBar->setOrientation(BarItem::Orientation::Horizontal);
    pwrBar->setRange(0.0, 120.0);
    pwrBar->setBindingId(MeterBinding::TxPower);
    pwrBar->setBarColor(QColor(0x00, 0xb4, 0xd8));
    pwrBar->setBarRedColor(QColor(0xff, 0x44, 0x44));
    pwrBar->setRedThreshold(100.0);   // From Thetis: HighPoint = 100W
    pwrBar->setAttackRatio(0.8f);     // From Thetis MeterManager.cs
    pwrBar->setDecayRatio(0.1f);      // From Thetis: PWR DecayRatio = 0.1
    pwrBar->setZOrder(5);
    group->addItem(pwrBar);

    ScaleItem* pwrScale = new ScaleItem();
    pwrScale->setRect(0.02f, 0.40f, 0.96f, 0.12f);
    pwrScale->setOrientation(ScaleItem::Orientation::Horizontal);
    pwrScale->setRange(0.0, 120.0);
    pwrScale->setMajorTicks(7);       // 0, 20, 40, 60, 80, 100, 120
    pwrScale->setMinorTicks(3);
    pwrScale->setFontSize(8);
    pwrScale->setZOrder(10);
    group->addItem(pwrScale);

    // --- SWR section (bottom half) ---

    TextItem* swrLabel = new TextItem();
    swrLabel->setRect(0.02f, 0.55f, 0.5f, 0.12f);
    swrLabel->setLabel(QStringLiteral("SWR"));
    swrLabel->setBindingId(-1);
    swrLabel->setTextColor(QColor(0x80, 0x90, 0xa0));
    swrLabel->setFontSize(10);
    swrLabel->setBold(false);
    swrLabel->setZOrder(10);
    group->addItem(swrLabel);

    TextItem* swrReadout = new TextItem();
    swrReadout->setRect(0.5f, 0.55f, 0.48f, 0.12f);
    swrReadout->setBindingId(MeterBinding::TxSwr);
    swrReadout->setTextColor(QColor(0xc8, 0xd8, 0xe8));
    swrReadout->setFontSize(13);
    swrReadout->setBold(true);
    swrReadout->setSuffix(QStringLiteral(":1"));
    swrReadout->setDecimals(1);
    swrReadout->setZOrder(10);
    group->addItem(swrReadout);

    // From Thetis MeterManager.cs AddSWRBar: 1:1-5:1, red at 3:1
    BarItem* swrBar = new BarItem();
    swrBar->setRect(0.02f, 0.69f, 0.96f, 0.15f);
    swrBar->setOrientation(BarItem::Orientation::Horizontal);
    swrBar->setRange(1.0, 5.0);
    swrBar->setBindingId(MeterBinding::TxSwr);
    swrBar->setBarColor(QColor(0x00, 0xb4, 0xd8));
    swrBar->setBarRedColor(QColor(0xff, 0x44, 0x44));
    swrBar->setRedThreshold(3.0);     // From Thetis: HighPoint = SWR 3:1
    swrBar->setAttackRatio(0.8f);
    swrBar->setDecayRatio(0.1f);      // From Thetis: SWR DecayRatio = 0.1
    swrBar->setZOrder(5);
    group->addItem(swrBar);

    ScaleItem* swrScale = new ScaleItem();
    swrScale->setRect(0.02f, 0.86f, 0.96f, 0.12f);
    swrScale->setOrientation(ScaleItem::Orientation::Horizontal);
    swrScale->setRange(1.0, 5.0);
    swrScale->setMajorTicks(5);       // 1, 2, 3, 4, 5
    swrScale->setMinorTicks(1);
    swrScale->setFontSize(8);
    swrScale->setZOrder(10);
    group->addItem(swrScale);

    return group;
}
```

- [ ] **Step 5: Implement createAlcPreset()**

```cpp
// ALC preset: horizontal bar, -30 to 0 dB range.
// From Thetis MeterManager.cs ALC scale: 0 dB = 66.5% bar
ItemGroup* ItemGroup::createAlcPreset(QObject* parent)
{
    return createHBarPreset(MeterBinding::TxAlc, -30.0, 0.0,
                            QStringLiteral("ALC"), parent);
}
```

- [ ] **Step 6: Implement createMicPreset()**

```cpp
// Mic level preset: horizontal bar, -30 to 0 dB range.
// From Thetis MeterManager.cs MIC scale
ItemGroup* ItemGroup::createMicPreset(QObject* parent)
{
    return createHBarPreset(MeterBinding::TxMic, -30.0, 0.0,
                            QStringLiteral("Mic"), parent);
}
```

- [ ] **Step 7: Implement createCompPreset()**

```cpp
// Compressor level preset: horizontal bar, -25 to 0 dB range.
// From Thetis MeterManager.cs COMP scale
ItemGroup* ItemGroup::createCompPreset(QObject* parent)
{
    return createHBarPreset(MeterBinding::TxComp, -25.0, 0.0,
                            QStringLiteral("Comp"), parent);
}
```

- [ ] **Step 8: Build to verify**

Run:
```bash
cd ~/NereusSDR && cmake --build build -j$(nproc) 2>&1 | tail -10
```
Expected: Build succeeds.

- [ ] **Step 9: Commit**

```bash
cd ~/NereusSDR && git add src/gui/meters/ItemGroup.h src/gui/meters/ItemGroup.cpp
git commit -m "$(cat <<'EOF'
Add S-Meter, Power/SWR, ALC, Mic, Comp preset factories + installInto

createSMeterPreset: single NeedleItem with AetherSDR visual style.
createPowerSwrPreset: stacked bars with Thetis calibration (0-120W
red@100W, SWR 1-5:1 red@3:1, DecayRatio=0.1).
createAlcPreset/createMicPreset/createCompPreset: horizontal bars
reusing createHBarPreset pattern with TX binding stubs.
installInto: transforms group items into target rect and transfers
ownership to MeterWidget.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 8: Default Container #0 layout

**Files:**
- Modify: `src/gui/MainWindow.cpp:266-288` (populateDefaultMeter)

- [ ] **Step 1: Replace populateDefaultMeter()**

Replace the entire `populateDefaultMeter()` method in `src/gui/MainWindow.cpp`:

```cpp
void MainWindow::populateDefaultMeter()
{
    ContainerWidget* c0 = m_containerManager->panelContainer();
    if (!c0) {
        qCWarning(lcContainer) << "No panel container for meter widget";
        return;
    }

    m_meterWidget = new MeterWidget();

    // S-Meter: top 55% — arc needle bound to SignalAvg
    // From Thetis MeterManager.cs: ANAN needle uses AVG_SIGNAL_STRENGTH
    ItemGroup* smeter = ItemGroup::createSMeterPreset(
        MeterBinding::SignalAvg, QStringLiteral("S-Meter"), m_meterWidget);
    smeter->installInto(m_meterWidget, 0.0f, 0.0f, 1.0f, 0.55f);
    delete smeter;

    // Power/SWR: middle 30% — stacked bars (stub TX bindings)
    ItemGroup* pwrSwr = ItemGroup::createPowerSwrPreset(
        QStringLiteral("Power/SWR"), m_meterWidget);
    pwrSwr->installInto(m_meterWidget, 0.0f, 0.55f, 1.0f, 0.30f);
    delete pwrSwr;

    // ALC: bottom 15% — horizontal bar (stub TX binding)
    ItemGroup* alc = ItemGroup::createAlcPreset(m_meterWidget);
    alc->installInto(m_meterWidget, 0.0f, 0.85f, 1.0f, 0.15f);
    delete alc;

    c0->setContent(m_meterWidget);
    qCDebug(lcMeter) << "Installed default meter layout: S-Meter + Power/SWR + ALC";
}
```

- [ ] **Step 2: Build the full project**

Run:
```bash
cd ~/NereusSDR && cmake --build build -j$(nproc) 2>&1 | tail -10
```
Expected: Build succeeds with zero errors.

- [ ] **Step 3: Launch and visually verify**

Run:
```bash
cd ~/NereusSDR && ./build/NereusSDR &
```
Expected:
- Container #0 shows three sections stacked vertically
- Top: arc-style S-meter with white/red arc segments, tick marks (S1, S3, S5, S7, S9, +20, +40), "S-Meter" source label, "S0" and "-127 dBm" readout text
- Middle: Power bar + SWR bar (at minimum/zero since TX is not connected)
- Bottom: ALC bar (at minimum since TX is not connected)
- When connected to a radio: S-meter needle should track signal strength with smooth movement and peak hold marker

- [ ] **Step 4: Commit**

```bash
cd ~/NereusSDR && git add src/gui/MainWindow.cpp
git commit -m "$(cat <<'EOF'
Wire default Container #0 with S-Meter + Power/SWR + ALC layout

Replace single signal bar with 3-group layout: S-Meter needle (55%,
bound to SignalAvg), Power/SWR stacked bars (30%, stub TX bindings),
ALC bar (15%, stub TX binding). Proportions match AetherSDR's right
panel. TX presets show placeholder values until Phase 3I-1.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Verification Checklist

After all tasks complete, verify:

- [ ] Existing H_BAR meter items still render correctly (backward compatibility)
- [ ] S-Meter arc shows white segment (S0-S9) and red segment (S9+)
- [ ] Tick marks at S1, S3, S5, S7, S9, +20, +40 with correct colors
- [ ] Source label "S-Meter" centered at top
- [ ] S-unit readout (cyan, left) and dBm readout (light steel, right) update live
- [ ] Needle tracks signal with smooth movement (SMOOTH_ALPHA=0.3)
- [ ] Peak marker (amber triangle) appears and decays at 10 dB/sec
- [ ] Peak hard-resets every 10 seconds
- [ ] Power/SWR bars visible with correct scale labels
- [ ] ALC bar visible with correct scale
- [ ] TX presets show default minimum values (no crash, no garbage)
- [ ] Cross-platform: builds on both macOS (Metal) and Windows (D3D11)
