// =================================================================
// src/gui/meters/presets/ClockPresetItem.h  (NereusSDR)
// =================================================================
//
// NereusSDR original — no Thetis upstream.
//
// no-port-check: NereusSDR-local preset. Collapsed from
// src/gui/meters/ItemGroup.cpp:1552-1572 (createClockPreset helper).
// Narrow strip (0..0.15 normalised): SolidColourItem backdrop
// (#0f0f1a) + ClockItem showing current UTC time. Ships under the
// project's GPLv3 terms (root LICENSE); no per-file verbatim upstream
// header required because no Thetis source code was consulted in
// writing this file.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Original implementation for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted scaffolding via Claude
//                Opus 4.7. First-class MeterItem subclass that
//                subsumes the createClockPreset() factory's two-item
//                composition (backdrop + ClockItem).
// =================================================================
#pragma once

#include "gui/meters/MeterItem.h"

#include <QColor>
#include <QString>

namespace NereusSDR {

// ClockPresetItem — NereusSDR-local clock strip preset.
//
// Collapses the SolidColourItem backdrop + ClockItem composition of
// `ItemGroup::createClockPreset()` into a single MeterItem. Paints
// a digital time readout centred in the rect.
class ClockPresetItem : public MeterItem
{
    Q_OBJECT

public:
    explicit ClockPresetItem(QObject* parent = nullptr);

    // --- MeterItem overrides ---
    // Background -> MeterWidget calls paintForLayer() -> paint();
    // primitives (BarItem / NeedleItem / etc.) on Geometry /
    // OverlayDynamic paint on top. This matches the user's mental
    // model that a preset is the "background meter" the container
    // shows, with the in-use list's item order driving z-order
    // (top of list = behind, bottom = front). Preset classes
    // override paint() only (not emitVertices()), so Layer::Geometry
    // would route through the empty GPU vertex path.
    Layer renderLayer() const override { return Layer::Background; }
    void  paint(QPainter& p, int widgetW, int widgetH) override;

    QString serialize() const override;
    bool    deserialize(const QString& data) override;

    bool hasClock() const { return true; }

    // --- UTC vs local toggle ---
    bool utc() const { return m_utc; }
    void setUtc(bool on) { m_utc = on; }

private:
    QColor m_backdropColor{0x0f, 0x0f, 0x1a};
    QColor m_textColor{0xc8, 0xd8, 0xe8};
    bool   m_utc{true};
};

} // namespace NereusSDR
