#pragma once

// =================================================================
// src/gui/meters/ItemGroup.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "MeterItem.h"

#include <QString>
#include <QVector>

namespace NereusSDR {

class MeterWidget;

class ItemGroup : public QObject {
    Q_OBJECT

public:
    explicit ItemGroup(const QString& name, QObject* parent = nullptr);
    ~ItemGroup() override;

    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    float x() const { return m_x; }
    float y() const { return m_y; }
    float groupWidth() const { return m_w; }
    float groupHeight() const { return m_h; }
    void setRect(float x, float y, float w, float h);

    void addItem(MeterItem* item);
    void removeItem(MeterItem* item);
    QVector<MeterItem*> items() const { return m_items; }

    QString serialize() const;
    static ItemGroup* deserialize(const QString& data, QObject* parent = nullptr);

    // Preset factory: creates a horizontal bar meter with scale + readout.
    // Layout within group: top 20% label+readout, mid 28% bar, bottom 46% scale.
    // Colors from AetherSDR: cyan bar (#00b4d8), red zone (#ff4444), dark bg (#0f0f1a).
    static ItemGroup* createHBarPreset(int bindingId, double minVal, double maxVal,
                                        const QString& name, QObject* parent = nullptr);

    // Compact HBar: single-line layout (label left, bar center, readout right).
    // No scale ticks. From AetherSDR HGauge pattern.
    static ItemGroup* createCompactHBarPreset(int bindingId, double minVal, double maxVal,
                                               const QString& name, QObject* parent = nullptr);

    // S-Meter preset: arc-style NeedleItem with full readout.
    // From AetherSDR SMeterWidget visual style. Default shape for
    // Container #0's fixed S-Meter header and the Presets menu
    // "S-Meter Only" entry. Phase 3G-9 briefly rebuilt this as a
    // Thetis-style bar (commit 4bba2c2) — reverted because the main
    // signal meter should stay a needle. Bar-composition port now
    // lives in createSMeterBarPreset below.
    static ItemGroup* createSMeterPreset(int bindingId, const QString& name,
                                          QObject* parent = nullptr);

    // Thetis addSMeterBar port: SolidColour backdrop + Line BarItem
    // with 3-point calibration (S0/S9/S9+60) + GeneralScale ScaleItem.
    // Opt-in — not wired into any UI call site by default. Add to a
    // new container manually to verify the Thetis calibrated bar.
    // From Thetis MeterManager.cs:21523-21616 addSMeterBar.
    static ItemGroup* createSMeterBarPreset(int bindingId, const QString& name,
                                             QObject* parent = nullptr);

    // Power/SWR preset: two stacked horizontal bars.
    // From Thetis MeterManager.cs AddPWRBar/AddSWRBar calibration.
    static ItemGroup* createPowerSwrPreset(const QString& name,
                                            QObject* parent = nullptr);

    // --- Phase E: Thetis-parity per-reading bar rows ---
    // Shared builder used by every create*BarRowPreset below.
    // Builds the canonical 5-item composition from Thetis
    // AddALCBar:23326-23411 (and every sibling Add*Bar that clones the
    // same layout):
    //   z=1  SolidColourItem dark gray(32,32,32), full rect
    //   z=2  BarItem Line style, ShowValue + ShowPeakValue + ShowHistory
    //         + ShowMarker + 3-point ScaleCalibration
    //   z=3  ScaleItem ShowType=true with the same bindingId so the row
    //         labels itself via readingName()
    // Different readings differ only in: bindingId, 3-point calibration
    // tuple, and historyColour.
    static ItemGroup* buildBarRow(int bindingId,
                                  double lowVal, double midVal, double highVal,
                                  float midX, float highX,
                                  const QColor& historyColour,
                                  QObject* parent);

    // ALC preset: horizontal bar for TX ALC level.
    // From Thetis MeterManager.cs AddALCBar:23326-23411.
    static ItemGroup* createAlcPreset(QObject* parent = nullptr);

    // Mic level preset: horizontal bar for TX mic level.
    // From Thetis MeterManager.cs MIC scale (-30 to 0 dB).
    static ItemGroup* createMicPreset(QObject* parent = nullptr);

    // Compressor level preset: horizontal bar for TX compressor.
    // From Thetis MeterManager.cs COMP scale (-25 to 0 dB).
    static ItemGroup* createCompPreset(QObject* parent = nullptr);

    // --- Phase 3G-4 bar presets (from Thetis MeterManager.cs AddMeter factory) ---
    static ItemGroup* createSignalBarPreset(QObject* parent = nullptr);
    static ItemGroup* createAvgSignalBarPreset(QObject* parent = nullptr);
    static ItemGroup* createMaxBinBarPreset(QObject* parent = nullptr);
    static ItemGroup* createAdcBarPreset(QObject* parent = nullptr);
    static ItemGroup* createAdcMaxMagPreset(QObject* parent = nullptr);
    static ItemGroup* createAgcBarPreset(QObject* parent = nullptr);
    static ItemGroup* createAgcGainBarPreset(QObject* parent = nullptr);
    static ItemGroup* createPbsnrBarPreset(QObject* parent = nullptr);
    static ItemGroup* createEqBarPreset(QObject* parent = nullptr);
    static ItemGroup* createLevelerBarPreset(QObject* parent = nullptr);
    static ItemGroup* createLevelerGainBarPreset(QObject* parent = nullptr);
    static ItemGroup* createAlcGainBarPreset(QObject* parent = nullptr);
    static ItemGroup* createAlcGroupBarPreset(QObject* parent = nullptr);
    static ItemGroup* createCfcBarPreset(QObject* parent = nullptr);
    static ItemGroup* createCfcGainBarPreset(QObject* parent = nullptr);
    static ItemGroup* createCustomBarPreset(int bindingId, double minVal, double maxVal,
                                             const QString& name, QObject* parent = nullptr);

    // --- Phase 3G-4 composite presets ---
    // ANAN Multi-Meter: 7-needle composite with background images
    // From Thetis AddAnanMM() (MeterManager.cs:22461-22815)
    static ItemGroup* createAnanMMPreset(QObject* parent = nullptr);

    // Cross-Needle: dual crossing needles (fwd/rev power)
    // From Thetis AddCrossNeedle() (MeterManager.cs:22817-23002)
    static ItemGroup* createCrossNeedlePreset(QObject* parent = nullptr);

    static ItemGroup* createMagicEyePreset(int bindingId, QObject* parent = nullptr);
    static ItemGroup* createSignalTextPreset(int bindingId, QObject* parent = nullptr);
    static ItemGroup* createHistoryPreset(int bindingId, QObject* parent = nullptr);
    static ItemGroup* createSpacerPreset(QObject* parent = nullptr);

    // --- Phase 3G-5 interactive presets ---
    static ItemGroup* createVfoDisplayPreset(QObject* parent = nullptr);
    static ItemGroup* createClockPreset(QObject* parent = nullptr);
    static ItemGroup* createContestPreset(QObject* parent = nullptr);

    // Install all items from this group into a MeterWidget, transforming
    // their normalized 0-1 positions into the given target rect.
    // Transfers item ownership to widget; clears this group's item list.
    void installInto(MeterWidget* widget, float gx, float gy, float gw, float gh);

private:
    QString m_name;
    float m_x{0.0f};
    float m_y{0.0f};
    float m_w{1.0f};
    float m_h{1.0f};
    QVector<MeterItem*> m_items;
};

} // namespace NereusSDR
