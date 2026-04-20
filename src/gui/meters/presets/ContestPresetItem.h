// =================================================================
// src/gui/meters/presets/ContestPresetItem.h  (NereusSDR)
// =================================================================
//
// NereusSDR original — no Thetis upstream.
//
// no-port-check: NereusSDR-local preset. Collapsed from
// src/gui/meters/ItemGroup.cpp:1574-1599 (createContestPreset
// helper). Full-container composite: backdrop + VFO (top 30%) +
// band buttons (30..55%) + mode buttons (55..75%) + clock (bottom
// 25%). Ships under the project's GPLv3 terms (root LICENSE); no
// per-file verbatim upstream header required because no Thetis
// source code was consulted in writing this file.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Original implementation for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted scaffolding via Claude
//                Opus 4.7. First-class MeterItem subclass subsuming
//                the 5-child composition of createContestPreset().
// =================================================================
#pragma once

#include "gui/meters/MeterItem.h"

#include <QColor>
#include <QString>

namespace NereusSDR {

// ContestPresetItem — NereusSDR-local full-container contest layout.
//
// Collapses the backdrop + VFO display + band buttons + mode buttons
// + clock composition of `ItemGroup::createContestPreset()` into one
// MeterItem. Internally paints five stacked strips; ships placeholder
// buttons until the Phase 3G-* button infrastructure is wired behind
// first-class presets.
class ContestPresetItem : public MeterItem
{
    Q_OBJECT

public:
    explicit ContestPresetItem(QObject* parent = nullptr);

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

    // --- Composition introspection ---
    bool hasBackdrop()    const { return true; }
    bool hasVfo()         const { return true; }
    bool hasBandButtons() const { return true; }
    bool hasModeButtons() const { return true; }
    bool hasClock()       const { return true; }

    // --- Content (mirrors VfoDisplayPresetItem) ---
    double  frequencyHz() const { return m_freqHz; }
    void    setFrequencyHz(double hz) { m_freqHz = hz; }

    QString bandLabel() const { return m_bandLabel; }
    void    setBandLabel(const QString& s) { m_bandLabel = s; }

    QString modeLabel() const { return m_modeLabel; }
    void    setModeLabel(const QString& s) { m_modeLabel = s; }

private:
    QColor  m_backdropColor{0x0f, 0x0f, 0x1a};
    QColor  m_digitColor{0x00, 0xe0, 0xff};
    QColor  m_labelColor{0x80, 0x90, 0xa0};
    QColor  m_buttonColor{0x1a, 0x22, 0x2e};
    QColor  m_clockColor{0xc8, 0xd8, 0xe8};
    double  m_freqHz{14200000.0};
    QString m_bandLabel{QStringLiteral("20m")};
    QString m_modeLabel{QStringLiteral("USB")};
};

} // namespace NereusSDR
