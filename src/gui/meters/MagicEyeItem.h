#pragma once

// =================================================================
// src/gui/meters/MagicEyeItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Thetis magic eye options: attack and decay, history and
//                 ignore history, eye and bezel scale, scale calibration,
//                 kMagicEyeThetisDefaults for new items. Saved records keep
//                 the fixed smoothing and look. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
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
#include "MeterPoller.h"
#include <QColor>
#include <QImage>
#include <QMap>
#include <QVector>

namespace NereusSDR {

// From Thetis clsMagicEyeItem (MeterManager.cs:15855+)
// Vacuum tube magic eye — green phosphor arc opens/closes with signal.
class MagicEyeItem : public MeterItem {
    Q_OBJECT

public:
    explicit MagicEyeItem(QObject* parent = nullptr) : MeterItem(parent) {}

    // S-meter scale constants (from NeedleItem / AetherSDR)
    static constexpr float kS0Dbm  = -127.0f;
    static constexpr float kMaxDbm = -13.0f;
    static constexpr float kSmoothAlpha = 0.3f;

    // Shadow wedge range (degrees)
    static constexpr float kMaxShadowDeg = 120.0f; // no signal — wide open
    static constexpr float kMinShadowDeg = 5.0f;   // full signal — nearly closed

    // Eye and bezel size, 1.0 = the item's full width (Thetis's setup
    // range, MeterItem.h kMinMeterEyeScale..kMaxMeterEyeScale).
    static constexpr float kMinEyeScale = kMinMeterEyeScale;
    static constexpr float kMaxEyeScale = kMaxMeterEyeScale;

    void setGlowColor(const QColor& c) { m_glowColor = c; }
    QColor glowColor() const { return m_glowColor; }

    void setBezelImagePath(const QString& path);
    QString bezelImagePath() const { return m_bezelPath; }

    // Thetis AttackRatio / DecayRatio: a rising reading moves the eye by
    // attack, a falling one by decay (clsMagicEyeItem.Update,
    // MeterManager.cs:17247-17250 [v2.10.3.15]). Saved records keep
    // kSmoothAlpha for both, the fixed smoothing they always had.
    // Clamped to 0..1.
    void setAttackRatio(float a);
    float attackRatio() const { return m_attackRatio; }
    void setDecayRatio(float d);
    float decayRatio() const { return m_decayRatio; }

    // Thetis ShowHistory and its history fields. Thetis's renderEye draws
    // no history; the eye keeps it for MinHistory / MaxHistory.
    void setShowHistory(bool on) { m_showHistory = on; }
    bool showHistory() const { return m_showHistory; }
    // Clamped to kMinBarHistoryDurationMs..kMaxBarHistoryDurationMs.
    void setHistoryDurationMs(int ms);
    int historyDurationMs() const { return m_historyDurationMs; }
    void setHistoryColour(const QColor& c) { m_historyColour = c; }
    QColor historyColour() const { return m_historyColour; }
    // Clamped to kMinBarIgnoreHistoryMs..kMaxBarIgnoreHistoryMs.
    void setIgnoreHistoryMs(int ms);
    int ignoreHistoryMs() const { return m_ignoreHistoryMs; }
    void clearHistory() override;
    int ignoringReadings() const { return m_ignoringNext; }
    int historySize() const { return static_cast<int>(m_history.size()); }
    float minHistory() const;
    float maxHistory() const;

    // Eye and bezel size, centred in the item. Clamped to
    // kMinEyeScale..kMaxEyeScale.
    void setEyeScale(float s);
    float eyeScale() const { return m_eyeScale; }
    void setBezelScale(float s);
    float bezelScale() const { return m_bezelScale; }

    // Thetis ScaleCalibration: dBm -> eye position 0..1. Empty (a saved
    // record) keeps the linear S0..S9+60 scale.
    void addScaleCalibration(float dbm, float position) { m_scaleCalibration.insert(dbm, position); }
    void clearScaleCalibration() { m_scaleCalibration.clear(); }
    QMap<float, float> scaleCalibration() const { return m_scaleCalibration; }

    // The eye position 0..1 for a reading, after the VHF S9 rule.
    float scalePosition(float dbm) const;
    // The smoothed reading the eye shows.
    float displayValue() const { return m_smoothedDbm; }
    // The eye and bezel rectangles paint() uses, in widget pixels.
    QRect eyeRect(int widgetW, int widgetH) const;
    QRect bezelRect(int widgetW, int widgetH) const;

    // New items (makeEntry) take kMagicEyeThetisDefaults.
    void applyNewItemDefaults() override;

    void setValue(double v) override;

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    float dbmToFraction(float dbm) const;
    QRect scaledRect(int widgetW, int widgetH, float scale) const;
    int historyIntervalMs() const;

    QColor  m_glowColor{0x00, 0xff, 0x88};
    QString m_bezelPath;
    QImage  m_bezelImage;
    float   m_smoothedDbm{kS0Dbm};

    float   m_attackRatio{kSmoothAlpha};
    float   m_decayRatio{kSmoothAlpha};
    bool    m_showHistory{false};
    int     m_historyDurationMs{500};
    int     m_ignoreHistoryMs{2000};
    QColor  m_historyColour{0xff, 0xff, 0xff, 128};
    int     m_ignoringNext{0};
    QVector<float> m_history;
    float   m_eyeScale{1.0f};
    float   m_bezelScale{1.0f};
    QMap<float, float> m_scaleCalibration;
};

// New MAGICEYE items (makeEntry) take Thetis's magic eye.
// From Thetis MeterManager.cs:23572-23618 [v2.10.3.15] (AddMagicEye) and
// the clsMagicEyeItem constructor :17188-17207. Thetis's bezel is the
// image "eye-bezel" (:23602); NereusSDR bundles no such art, so a new eye
// starts with no bezel until one is picked.
struct MagicEyeItemThetisDefaults {
    float attackRatio;
    float decayRatio;
    QRgb  colour;
    int   bindingId;
    float calibrationDbm[3];
    float calibrationPosition[3];
    bool  showHistory;
    int   historyDurationMs;
    int   ignoreHistoryMs;
    QRgb  historyColour;
    float eyeScale;
    float bezelScale;
};
inline constexpr MagicEyeItemThetisDefaults kMagicEyeThetisDefaults{
    0.2f,                           // me.AttackRatio = 0.2f (:23585)
    0.05f,                          // me.DecayRatio = 0.05f (:23586)
    qRgb(0x00, 0xff, 0x00),         // me.Colour = Color.Lime (:23588)
    MeterBinding::SignalAvg,        // Reading.AVG_SIGNAL_STRENGTH (:23589)
    {-127.0f, -73.0f, -13.0f},      // ScaleCalibration keys (:23590-23592)
    {0.0f, 0.85f, 1.0f},            // ScaleCalibration X (:23590-23592)
    false,                          // _showHistory = false (:17193)
    500,                            // _msHistoryDuration = 500 (:17191)
    2000,                           // _msIgnoreHistoryDuration = 2000 (:17192)
    qRgba(0xff, 0xff, 0xff, 128),   // _historyColour, clsMeterItem :6763
    kDefaultMeterEyeScale,          // nudMeterItemEyeScale.Value (MeterItem.h)
    kDefaultMeterEyeScale,          // nudMeterItemEyeBezelScale.Value (MeterItem.h)
};

} // namespace NereusSDR
