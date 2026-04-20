// =================================================================
// src/gui/meters/presets/HistoryGraphPresetItem.h  (NereusSDR)
// =================================================================
//
// NereusSDR original — no Thetis upstream.
//
// no-port-check: NereusSDR-local preset. Collapsed from
// src/gui/meters/ItemGroup.cpp:1485-1520 (createHistoryPreset helper).
// Rolling history strip + current-value text + scale ticks, bound to a
// configurable MeterBinding::* value. Ships under the project's GPLv3
// terms (root LICENSE); no per-file verbatim upstream header required
// because no Thetis source code was consulted in writing this file.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Original implementation for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted scaffolding via Claude
//                Opus 4.7. First-class MeterItem subclass that
//                subsumes the createHistoryPreset() factory's
//                SolidColourItem + HistoryGraphItem + TextItem
//                composition.
// =================================================================
#pragma once

#include "gui/meters/MeterItem.h"

#include <QColor>
#include <QVector>

namespace NereusSDR {

// HistoryGraphPresetItem — first-class rolling history preset.
//
// NereusSDR-local. Collapses the SolidColourItem backdrop +
// HistoryGraphItem (top 80%) + TextItem readout (bottom 20%)
// composition of ItemGroup::createHistoryPreset into a single
// MeterItem. Supports any binding the caller assigns; default is
// MeterBinding::SignalAvg to match the factory's default call path.
class HistoryGraphPresetItem : public MeterItem
{
    Q_OBJECT

public:
    explicit HistoryGraphPresetItem(QObject* parent = nullptr);

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

    // --- Introspection ---
    bool hasGraph() const { return true; }
    bool hasScale() const { return true; }
    bool hasReadout() const { return m_showReadout; }

    // --- Readout toggle ---
    void setShowReadout(bool on) { m_showReadout = on; }
    bool showReadout() const { return m_showReadout; }

    // --- Ring-buffer capacity ---
    int  capacity() const { return m_capacity; }
    void setCapacity(int n);

    // --- Value ingestion (history strip feed) ---
    void setValue(double v) override;

private:
    QColor           m_backdropColor{0x0a, 0x0a, 0x18};
    QColor           m_lineColor{0x00, 0xb4, 0xd8};
    QColor           m_readoutColor{0xc8, 0xd8, 0xe8};
    int              m_capacity{300};
    QVector<double>  m_samples;
    bool             m_showReadout{true};
};

} // namespace NereusSDR
