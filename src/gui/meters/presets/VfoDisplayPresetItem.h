// =================================================================
// src/gui/meters/presets/VfoDisplayPresetItem.h  (NereusSDR)
// =================================================================
//
// NereusSDR original — no Thetis upstream.
//
// no-port-check: NereusSDR-local preset. Collapsed from
// src/gui/meters/ItemGroup.cpp:1537-1551 (createVfoDisplayPreset
// helper). Background + large 7-segment-style frequency digits +
// band / mode labels. Ships under the project's GPLv3 terms (root
// LICENSE); no per-file verbatim upstream header required because no
// Thetis source code was consulted in writing this file.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Original implementation for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted scaffolding via Claude
//                Opus 4.7. First-class MeterItem subclass that
//                subsumes the createVfoDisplayPreset() factory's
//                SolidColourItem + VfoDisplayItem composition.
// =================================================================
#pragma once

#include "gui/meters/MeterItem.h"

#include <QColor>
#include <QString>

namespace NereusSDR {

// VfoDisplayPresetItem — NereusSDR-local VFO display preset.
//
// Collapses the SolidColourItem backdrop + VfoDisplayItem (large
// 7-segment frequency digits) composition emitted by
// `ItemGroup::createVfoDisplayPreset()` into one MeterItem with
// integrated band/mode labels.
class VfoDisplayPresetItem : public MeterItem
{
    Q_OBJECT

public:
    explicit VfoDisplayPresetItem(QObject* parent = nullptr);

    // --- MeterItem overrides ---
    Layer renderLayer() const override { return Layer::Geometry; }
    void  paint(QPainter& p, int widgetW, int widgetH) override;

    QString serialize() const override;
    bool    deserialize(const QString& data) override;

    bool hasFrequencyDigits() const { return true; }
    bool hasBandLabel()       const { return true; }
    bool hasModeLabel()       const { return true; }

    // --- Content ---
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
    double  m_freqHz{14200000.0};
    QString m_bandLabel{QStringLiteral("20m")};
    QString m_modeLabel{QStringLiteral("USB")};
};

} // namespace NereusSDR
