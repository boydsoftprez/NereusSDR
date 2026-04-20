// =================================================================
// src/gui/meters/presets/AnanMultiMeterItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs (AddAnanMM, ~line 22461)
//   verbatim upstream header reproduced below; the .cpp carries the
//   same block in front of the calibration tables.
//
// =================================================================

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

// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Reimplemented in C++20/Qt6 for NereusSDR by
//                J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Claude Opus 4.7. First-class MeterItem subclass
//                collapsing the 7-needle ANAN Multi Meter preset into
//                a single entity; pivot/radius now anchor to the
//                background image's letterboxed draw rect to fix the
//                arc-drift bug at non-default container aspect ratios.
//
//                Note on binding identifiers: Thetis's AddAnanMM
//                tags each needle with `MMIOVariableIndex = 0..6` —
//                a per-preset slot ordinal used by the Thetis MMIO
//                exporter, NOT a runtime data-source identifier.
//                NereusSDR's runtime needle-to-WDSP routing uses the
//                C++ `MeterBinding::*` constants in
//                `src/gui/meters/MeterPoller.h` (e.g.
//                `MeterBinding::SignalAvg = 1`, `HwVolts = 200`,
//                `HwAmps = 201`, `TxPower = 100`, `TxSwr = 102`,
//                `TxAlcGain = 109`, `TxAlcGroup = 110`). The two
//                numbering schemes are unrelated; the Thetis 0..6
//                slot ordering is preserved only as the array index
//                inside `m_needles` to keep the assignment order
//                visually parallel to AddAnanMM.
//
//   2026-04-19 — NereusSDR-original face art replaced the Thetis
//                ananMM.png. Ported Thetis clsNeedleItem renderNeedle
//                math faithfully (MeterManager.cs:38808 [@501e3f5]):
//                NeedleOffset is an offset from the rect CENTER
//                (not topleft), scaled by rect size; radiusX and
//                radiusY both scale by (w/2) × LengthFactor ×
//                RadiusRatio; tip is computed via atan2 on the
//                ellipse-expanded pivot→calibration vector plus a
//                180° rotation. Hoisted the single shared pivot /
//                radiusRatio / lengthFactor off the item class onto
//                a per-Needle struct so the 3 distinct arc centres
//                on the new face (main center-bottom, bottom-left
//                ALC, bottom-right Volts) can be represented.
//                ALC and Volts calibration tables re-derived for
//                the new small arcs; Signal/Amps/Power/SWR/Comp
//                keep the Thetis calibration (verified by pixel
//                overlay to still align with the new main face arcs).
// =================================================================
#pragma once

#include "gui/meters/MeterItem.h"

#include <QColor>
#include <QImage>
#include <QMap>
#include <QPointF>
#include <QRect>
#include <QString>
#include <array>
#include <limits>

namespace NereusSDR {

// AnanMultiMeterItem — first-class ANAN Multi Meter preset.
//
// Encapsulates the 7-needle composite (Signal, Volts, Amps, Power,
// SWR, Compression, ALC) plus the ananMM.png background image as a
// single MeterItem. Replaces the 9+ child items emitted by the old
// `ItemGroup::createAnanMMPreset()` factory; one row in the in-use
// list, one property pane, one persistence blob.
//
// Arc-fix: pivot and radius are anchored to the painted image rect
// (`bgRect()`), not the container's normalized item rect. At
// non-natural aspect ratios the background letterboxes inside the
// item rect; the needles follow the image, keeping the arc glued to
// the meter face. Toggle off via `setAnchorToBgRect(false)` for
// pre-port behaviour.
class AnanMultiMeterItem : public MeterItem
{
    Q_OBJECT

public:
    static constexpr int kNeedleCount = 7;

    explicit AnanMultiMeterItem(QObject* parent = nullptr);

    // --- MeterItem overrides ---
    // Background -> MeterWidget calls paintForLayer() -> paint();
    // primitives (BarItem / NeedleItem / etc.) on Geometry /
    // OverlayDynamic paint on top. This matches the user's mental
    // model that a preset is the "background meter" the container
    // shows, with the in-use list's item order driving z-order
    // (top of list = behind, bottom = front). The 11 preset classes
    // only override paint() (QPainter), not emitVertices();
    // Layer::Geometry would route them through the empty GPU vertex
    // pipeline and the preset would never draw.
    Layer renderLayer() const override { return Layer::Background; }
    void  paint(QPainter& p, int widgetW, int widgetH) override;

    // Edit-container refactor Task 20 — live binding routing.
    // MeterPoller iterates every binding ID each tick and calls
    // pushBindingValue on every item. This override dispatches the
    // value to the matching internal needle (Signal=SignalAvg,
    // Volts=HwVolts, Amps=HwAmps, Power=TxPower, SWR=TxSwr,
    // Compression=TxAlcGain, ALC=TxAlcGroup) and stores it in
    // Needle::currentValue so paint() renders the live tip instead of
    // the calibration midpoint seed.
    void pushBindingValue(int bindingId, double v) override;

    QString serialize() const override;
    bool    deserialize(const QString& data) override;

    // --- Needle introspection ---
    int                 needleCount() const { return kNeedleCount; }
    QString             needleName(int i) const;
    QMap<float, QPointF> needleCalibration(int i) const;
    QColor              needleColor(int i) const;
    bool                needleVisible(int i) const;
    void                setNeedleVisible(int i, bool v);

    // --- Arc-fix: anchor pivot/radius to background image rect ---
    bool anchorToBgRect() const { return m_anchorToBgRect; }
    void setAnchorToBgRect(bool v) { m_anchorToBgRect = v; }

    // --- Debug overlay: paint coloured dots at every calibration point ---
    bool debugOverlay() const { return m_debugOverlay; }
    void setDebugOverlay(bool v) { m_debugOverlay = v; }

    // =========================================================
    // Edit-container refactor — Thetis ANAN-MM property-editor
    // parity (Phase 1..3). Each field below corresponds to one
    // Setup → Appearance → Meters/Gadgets control on Thetis's
    // ANAN Multi Meter tab. Storage for the full knob set is
    // introduced up front so the serialization schema stays
    // stable across phases; UI + paint-time wiring lands over
    // the 4-commit series described in
    // docs/architecture/phase3g-edit-container plan notes.
    // =========================================================

    // Signal-source selector for the Signal needle. Thetis lets
    // the user pick between the peak / average / max-bin S-meter
    // readings for the same needle.
    enum class SignalSource { Avg, Peak, MaxBin };

    SignalSource signalSource() const { return m_signalSource; }
    void         setSignalSource(SignalSource s);

    // --- Settings group ---
    int   updateMs() const { return m_updateMs; }
    void  setUpdateMs(int ms) { m_updateMs = ms; }

    float attackRatio() const { return m_attackRatio; }
    void  setAttackRatio(float a) { m_attackRatio = a; }

    float decayRatio() const { return m_decayRatio; }
    void  setDecayRatio(float d) { m_decayRatio = d; }

    // --- Colors group (scheme swatches) ---
    QColor backgroundColor() const { return m_backgroundColor; }
    void   setBackgroundColor(const QColor& c) { m_backgroundColor = c; }

    QColor lowColor() const { return m_lowColor; }
    void   setLowColor(const QColor& c) { m_lowColor = c; }

    QColor highColor() const { return m_highColor; }
    void   setHighColor(const QColor& c) { m_highColor = c; }

    QColor indicatorColor() const { return m_indicatorColor; }
    void   setIndicatorColor(const QColor& c) { m_indicatorColor = c; }

    QColor subColor() const { return m_subColor; }
    void   setSubColor(const QColor& c) { m_subColor = c; }

    // --- Per-needle colour accessors (writable, unlike needleColor above) ---
    void   setNeedleColor(int i, const QColor& c);

    // --- Toggles ---
    bool   fadeOnRx() const { return m_fadeOnRx; }
    void   setFadeOnRx(bool v) { m_fadeOnRx = v; }

    bool   fadeOnTx() const { return m_fadeOnTx; }
    void   setFadeOnTx(bool v) { m_fadeOnTx = v; }

    bool   darkMode() const { return m_darkMode; }
    void   setDarkMode(bool v) { m_darkMode = v; }

    // --- Meter Title ---
    bool   showMeterTitle() const { return m_showMeterTitle; }
    void   setShowMeterTitle(bool v) { m_showMeterTitle = v; }
    QColor meterTitleColor() const { return m_meterTitleColor; }
    void   setMeterTitleColor(const QColor& c) { m_meterTitleColor = c; }
    QString meterTitleText() const { return m_meterTitleText; }
    void   setMeterTitleText(const QString& t) { m_meterTitleText = t; }

    // --- Peak Value (numeric readout near each needle scale) ---
    bool   showPeakValue() const { return m_showPeakValue; }
    void   setShowPeakValue(bool v) { m_showPeakValue = v; }
    QColor peakValueColor() const { return m_peakValueColor; }
    void   setPeakValueColor(const QColor& c) { m_peakValueColor = c; }

    // --- Peak Hold (marker at decaying max) ---
    bool   showPeakHold() const { return m_showPeakHold; }
    void   setShowPeakHold(bool v) { m_showPeakHold = v; }
    QColor peakHoldColor() const { return m_peakHoldColor; }
    void   setPeakHoldColor(const QColor& c) { m_peakHoldColor = c; }

    // --- History (rolling trail behind the needle) ---
    bool   showHistory() const { return m_showHistory; }
    void   setShowHistory(bool v) { m_showHistory = v; }
    int    historyMs() const { return m_historyMs; }
    void   setHistoryMs(int ms) { m_historyMs = ms; }
    int    ignoreHistoryMs() const { return m_ignoreHistoryMs; }
    void   setIgnoreHistoryMs(int ms) { m_ignoreHistoryMs = ms; }
    QColor historyColor() const { return m_historyColor; }
    void   setHistoryColor(const QColor& c) { m_historyColor = c; }

    // --- Shadow decoration ---
    bool   showShadow() const { return m_showShadow; }
    void   setShowShadow(bool v) { m_showShadow = v; }
    double shadowLow() const { return m_shadowLow; }
    void   setShadowLow(double v) { m_shadowLow = v; }
    double shadowHigh() const { return m_shadowHigh; }
    void   setShadowHigh(double v) { m_shadowHigh = v; }
    QColor shadowColor() const { return m_shadowColor; }
    void   setShadowColor(const QColor& c) { m_shadowColor = c; }

    // --- Segmented decoration ---
    bool   showSegmented() const { return m_showSegmented; }
    void   setShowSegmented(bool v) { m_showSegmented = v; }
    double segmentedLow() const { return m_segmentedLow; }
    void   setSegmentedLow(double v) { m_segmentedLow = v; }
    double segmentedHigh() const { return m_segmentedHigh; }
    void   setSegmentedHigh(double v) { m_segmentedHigh = v; }
    QColor segmentedColor() const { return m_segmentedColor; }
    void   setSegmentedColor(const QColor& c) { m_segmentedColor = c; }

    // --- Solid decoration ---
    bool   showSolid() const { return m_showSolid; }
    void   setShowSolid(bool v) { m_showSolid = v; }
    QColor solidColor() const { return m_solidColor; }
    void   setSolidColor(const QColor& c) { m_solidColor = c; }

private:
    struct Needle {
        QString              name;
        int                  bindingId{-1};
        QMap<float, QPointF> calibration;
        QColor               color;
        bool                 visible{true};
        // Per-needle pivot — Thetis `clsNeedleItem.NeedleOffset`
        // (MeterManager.cs:38825 [@501e3f5]). Offset from the bg
        // rect's CENTER, measured as a fraction of the rect size
        // (not the normalized image coord). Thetis: startX = cX +
        // (w * NeedleOffset.X); startY = cY + (h * NeedleOffset.Y).
        // The original shared pivot (0.004, 0.736) places the
        // pivot just right of center and below the rect bottom —
        // serving the big sweeping main arcs. The new face places
        // ALC and Volts on small corner arcs with their own
        // pivots below those arcs.
        QPointF              pivot{0.004, 0.736};
        // Per-needle elliptical radius ratio — Thetis
        // `clsNeedleItem.RadiusRatio` (MeterManager.cs:38830..38831
        // [@501e3f5]). Final radiusX/Y = (w/2) * LengthFactor *
        // RadiusRatio.X/Y. The (1.0, 0.58) default comes from
        // AddAnanMM; both radii scale by (w/2), which — coupled
        // with the 1504:688 aspect of the face image — lets the
        // needle tip reach the calibration points correctly.
        QPointF              radiusRatio{1.0, 0.58};
        // Per-needle reach scaler — Thetis `clsNeedleItem.LengthFactor`
        // (MeterManager.cs:38830..38831 [@501e3f5]). Multiplies both
        // radii; larger values make the needle extend further from
        // pivot. Each of the 7 AddAnanMM needles has its own value
        // (Signal 1.65, Volts 0.75, Amps 1.15, Power 1.55, SWR 1.36,
        // Comp 0.96, ALC 0.75).
        float                lengthFactor{1.0f};
        // Edit-container refactor Task 20 — last value pushed through
        // pushBindingValue() for this needle's bindingId. NaN until the
        // poller delivers the first value; paint() treats NaN as "no
        // data" and falls back to the calibration midpoint so the
        // preview in the settings dialog still shows a needle.
        double               currentValue{std::numeric_limits<double>::quiet_NaN()};

        // Thetis property-editor parity Phase 2 — rolling per-needle
        // history trail. Sampled in pushBindingValue() and consumed by
        // paint() when `m_showHistory` is true. Bounded to the last
        // ~256 samples; the UI exposes `historyMs` as a time window
        // which paint() converts to a sample count via the current
        // MeterPoller tick (approximately `historyMs / updateMs`).
        QVector<double>      history;
    };

    void   initialiseNeedles();
    QRect  bgRect(int widgetW, int widgetH) const;
    QPointF calibratedPosition(const Needle& n, float value) const;
    void   paintNeedle(QPainter& p, const Needle& n, const QRect& bg) const;
    void   paintDebugOverlay(QPainter& p, const QRect& bg) const;

    QImage                       m_background;
    bool                         m_anchorToBgRect{true};
    bool                         m_debugOverlay{false};
    std::array<Needle, kNeedleCount> m_needles;

    // =========================================================
    // Thetis property-editor parity fields (storage for all 20+
    // knobs; defaults chosen to match Thetis ANAN-MM new preset
    // defaults). Paint-time behaviour is wired incrementally across
    // the 4-commit phase series; see the TODO(render-mode-*)
    // markers in paint() for the ones still UI-only.
    // =========================================================
    SignalSource m_signalSource{SignalSource::Avg};
    int          m_updateMs{100};              // Thetis default 100 ms poll
    float        m_attackRatio{0.8f};           // needle smoothing — UI-only
    float        m_decayRatio{0.2f};            // needle smoothing — UI-only

    QColor       m_backgroundColor{0, 0, 0, 0}; // transparent = let face show
    QColor       m_lowColor{Qt::white};
    QColor       m_highColor{255, 64, 64};      // Thetis "red zone" default
    QColor       m_indicatorColor{Qt::yellow};
    QColor       m_subColor{Qt::black};

    bool         m_fadeOnRx{false};
    bool         m_fadeOnTx{false};
    bool         m_darkMode{false};

    bool         m_showMeterTitle{false};
    QColor       m_meterTitleColor{Qt::white};
    QString      m_meterTitleText{QStringLiteral("ANAN MM")};

    bool         m_showPeakValue{false};
    QColor       m_peakValueColor{Qt::yellow};

    bool         m_showPeakHold{false};
    QColor       m_peakHoldColor{255, 128, 0};

    bool         m_showHistory{false};
    int          m_historyMs{4000};              // Thetis default
    int          m_ignoreHistoryMs{250};         // Thetis default
    QColor       m_historyColor{255, 0, 0, 128}; // Thetis default Red(128)

    bool         m_showShadow{false};
    double       m_shadowLow{-120.0};
    double       m_shadowHigh{-20.0};
    QColor       m_shadowColor{0, 0, 0, 96};

    bool         m_showSegmented{false};
    double       m_segmentedLow{-120.0};
    double       m_segmentedHigh{-20.0};
    QColor       m_segmentedColor{Qt::darkGray};

    bool         m_showSolid{false};
    QColor       m_solidColor{Qt::gray};

    // Peak-hold tracking (runtime only; not serialized): per-needle
    // decaying maximum used to paint the peak marker.
    std::array<double, kNeedleCount> m_peakHolds{};
};

} // namespace NereusSDR
