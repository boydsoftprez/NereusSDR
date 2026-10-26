#pragma once

// =================================================================
// src/gui/meters/DialItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Thetis dial tuning (clsDialDisplay Degrees, Pressed,
//                 Update, MouseUp, MouseWheel): drag and wheel tune the
//                 slice the dial follows, acceleration with undo on
//                 release, smoothed speed, lock, slice select (Source slice
//                 or Any slice) and the top buttons stepping slices; the
//                 appended settings and kDialThetisDefaults for new dials.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Fix round 1 (ruling D5): new dials draw Thetis's look
//                 (renderDialDisplay); a saved dial keeps the look it had
//                 (the marker shows only while it is dragged, which starts
//                 from a press and a move). The look is saved.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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
#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QRgb>
#include <functional>

namespace NereusSDR {

// One open slice as the dial sees it. MainWindow feeds one per open slice
// (index is the slice id, 0 = Slice A); runtime only, never saved.
struct DialSliceState {
    int index{-1};
    int stepHz{0};
    bool locked{false};
};

// From Thetis clsDialDisplay (MeterManager.cs:16719-17366 [v2.10.3.15])
// Circular tuning dial: drag the marker round the ring, or use the wheel,
// to tune; the corner buttons step slices (top), turn acceleration on and
// off (bottom left) and lock the slice (bottom right).
class DialItem : public MeterItem {
    Q_OBJECT

public:
    enum class SpeedState { Slow, Hold, Fast };
    // Replaces Thetis's VFOA / VFOB choice and "always show VFOs": the dial
    // tunes the slice its entry's Source gives, or the active slice.
    enum class SliceSelect : int { SourceSlice = 0, AnySlice = 1 };

    explicit DialItem(QObject* parent = nullptr);

    // Colors (from Thetis clsDialDisplay properties)
    void setTextColour(const QColor& c) { m_textColour = c; }
    QColor textColour() const { return m_textColour; }

    void setCircleColour(const QColor& c) { m_circleColour = c; }
    QColor circleColour() const { return m_circleColour; }

    void setPadColour(const QColor& c) { m_padColour = c; }
    QColor padColour() const { return m_padColour; }

    // Thetis PadPressedColour (MeterManager.cs:16816 [v2.10.3.15]): the marker while
    // it is held.
    void setPadPressedColour(const QColor& c) { m_padPressedColour = c; }
    QColor padPressedColour() const { return m_padPressedColour; }

    void setRingColour(const QColor& c) { m_ringColour = c; }
    QColor ringColour() const { return m_ringColour; }

    void setButtonOnColour(const QColor& c) { m_btnOnColour = c; }
    QColor buttonOnColour() const { return m_btnOnColour; }

    void setButtonOffColour(const QColor& c) { m_btnOffColour = c; }
    QColor buttonOffColour() const { return m_btnOffColour; }

    void setButtonHighlightColour(const QColor& c) { m_btnHighlight = c; }
    QColor buttonHighlightColour() const { return m_btnHighlight; }

    void setSlowColour(const QColor& c) { m_slowColour = c; }
    QColor slowColour() const { return m_slowColour; }

    void setHoldColour(const QColor& c) { m_holdColour = c; }
    QColor holdColour() const { return m_holdColour; }

    void setFastColour(const QColor& c) { m_fastColour = c; }
    QColor fastColour() const { return m_fastColour; }

    // Settings (Thetis setup ranges, MeterManager.cs:27749-27760
    // [v2.10.3.15]); each setter clamps to its range.
    void setVerticalRatio(float ratio);
    float verticalRatio() const { return m_verticalRatio; }
    void setFontScale(float scale);
    float fontScale() const { return m_fontScale; }
    // UpThreshold / DownThreshold (MeterManager.cs:16830-16847 [v2.10.3.15]): the two
    // stay at least 90 degrees per second apart.
    void setUpThreshold(int degreesPerSecond);
    int upThreshold() const { return m_upThreshold; }
    void setDownThreshold(int degreesPerSecond);
    int downThreshold() const { return m_downThreshold; }
    void setTuneStepIntervalSeconds(int seconds);
    int tuneStepIntervalSeconds() const { return m_tuneStepIntervalSeconds; }
    void setMaxTuneStepChanges(int changes);
    int maxTuneStepChanges() const { return m_maxTuneStepChanges; }
    void setDegreesPerChange(int degrees);
    int degreesPerChange() const { return m_degreesPerChange; }
    void setAlignWithTuneStep(bool align) { m_alignWithTuneStep = align; }
    bool alignWithTuneStep() const { return m_alignWithTuneStep; }
    void setSliceSelect(SliceSelect select);
    SliceSelect sliceSelect() const { return m_sliceSelect; }

    // State
    void setActiveQuadrant(int q) { m_activeQuadrant = q; } // 0-3, -1 = none
    int activeQuadrant() const { return m_activeQuadrant; }

    void setSpeedState(SpeedState s) { m_speedState = s; }
    SpeedState speedState() const { return m_speedState; }

    // Thetis Accelerate (saved).
    void setAccelEnabled(bool e) { m_accelEnabled = e; }
    bool accelEnabled() const { return m_accelEnabled; }

    // Thetis Lock: the tuned slice's lock. The feed below sets it; while it
    // is on, turning the dial tunes nothing.
    void setLockEnabled(bool e) { m_lockEnabled = e; }
    bool lockEnabled() const { return m_lockEnabled; }

    // The slices (fed by MainWindow on every container refresh).
    void clearSliceStates();
    void setSliceState(const DialSliceState& state);
    DialSliceState sliceState(int sliceIndex) const;
    void setSourceSliceIndex(int sliceIndex);
    int sourceSliceIndex() const { return m_sourceSliceIndex; }
    void setActiveSliceIndex(int sliceIndex);
    int activeSliceIndex() const { return m_activeSliceIndex; }
    // Whether the entry's Source follows the selected slice, so selecting
    // another slice moves a Source-slice dial too.
    void setSourceFollowsSelection(bool follows);
    // The slice the dial tunes: its Source slice, or the active slice for
    // Any slice; -1 when that slice is not open.
    int tunedSlice() const;
    // The open slice before / after the tuned one (-1 at either end) and
    // whether the top buttons may select it.
    int previousSlice() const;
    int nextSlice() const;
    bool sliceStepEnabled() const;
    // What the dynamic layer shows from the feed (MainWindow repaints the
    // item when it changes).
    QString presentationKey() const;

    // Thetis clsDialDisplay's tuning model. nowMs is a monotonic time.
    // Degrees setter (MeterManager.cs:16907-17030 [v2.10.3.15]).
    void setDegrees(float value, qint64 nowMs);
    float degrees() const { return m_degrees; }
    // Pressed setter and undoTuneStep (MeterManager.cs:17074-17111 [v2.10.3.15]): a press records the
    // step, a release restores it.
    void setPressed(bool pressed);
    bool pressed() const { return m_pressed; }
    // Update (MeterManager.cs:17116-17129 [v2.10.3.15]): the smoothed speed.
    void updateSpeed(qint64 nowMs);
    float smoothedSpeed() const { return m_smoothedSpeed; }
    int tuneStepDirection() const { return m_tuneStepDirection; }
    // The frequency one change tunes to (deltaHz is one step either way):
    // with align, rounded to the step as Thetis's Math.Round does
    // (MeterManager.cs:16951-16975 [v2.10.3.15]).
    static qint64 tunedFrequencyHz(qint64 currentHz, qint64 deltaHz, bool align);
    // Tests: the clock the mouse handlers read.
    void setClockForTest(std::function<qint64()> clock) { m_clockForTest = std::move(clock); }

    // Which look the dial draws (ruling D5). Thetis: renderDialDisplay's
    // corner buttons, ring and marker; new dials take it. Saved: the look
    // dials had before the tuning port (a saved record without the field),
    // whose marker shows only while it is dragged.
    void setThetisLook(bool thetis) { m_thetisLook = thetis; }
    bool thetisLook() const { return m_thetisLook; }
    // Whether the marker is drawn now.
    bool drawsMarker() const { return m_thetisLook || m_pressed; }

    // Multi-layer
    bool participatesIn(Layer layer) const override;
    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer) override;
    void paint(QPainter& p, int widgetW, int widgetH) override;

    bool advanceMeter(qint64 monotonicMs) override;
    bool hitTest(const QPointF& pos, int widgetW, int widgetH) const override;
    bool handleMousePress(QMouseEvent* event, int widgetW, int widgetH) override;
    bool handleMouseRelease(QMouseEvent* event, int widgetW, int widgetH) override;
    bool handleMouseMove(QMouseEvent* event, int widgetW, int widgetH) override;
    bool handleWheel(QWheelEvent* event, int widgetW, int widgetH) override;
    bool handleMouseLeave() override;
    QString reasonAt(const QPointF& pos, int widgetW, int widgetH) const override;

    // New dials (makeEntry) take kDialThetisDefaults.
    void applyNewItemDefaults() override;

    QString serialize() const override;
    bool deserialize(const QString& data) override;

signals:
    // Each names the slice (its id, 0 = Slice A). Nothing here keys.
    void tuneRequested(int slice, qint64 deltaHz);
    void stepChangeRequested(int slice, int hz);
    void sliceSelectRequested(int slice);
    void lockToggleRequested(int slice);

private:
    void paintStatic(QPainter& p, const QRect& dialRect);
    void paintDynamic(QPainter& p, const QRect& dialRect);
    void paintThetis(QPainter& p, const QRect& dialRect, int widgetW, int widgetH);
    void paintMarker(QPainter& p, const QRect& dialRect);
    QRect squareRect(int widgetW, int widgetH) const;
    // 0 top left, 1 top right, 2 bottom left, 3 bottom right; -1 outside
    // the dial, inside its centre or on the marker.
    int buttonAt(const QPointF& pos, const QRect& dialRect) const;
    bool onMarker(const QPointF& pos, const QRect& dialRect) const;
    float angleAt(const QPointF& pos, const QRect& dialRect) const;
    bool buttonEnabled(int quadrant) const;
    bool interactive() const;
    qint64 nowMs() const;
    void syncLock();
    void changeTuneStep(int slice, int direction);
    void undoTuneStep();

    // Colors (Thetis defaults from clsDialDisplay)
    QColor m_textColour{0xc8, 0xd8, 0xe8};
    QColor m_circleColour{0x1a, 0x2a, 0x3a};
    QColor m_padColour{0x15, 0x20, 0x30};
    QColor m_padPressedColour;
    QColor m_ringColour{0x40, 0x50, 0x60};
    QColor m_btnOnColour{0x00, 0x70, 0xc0};
    QColor m_btnOffColour{0x30, 0x40, 0x50};
    QColor m_btnHighlight{0x00, 0x90, 0xe0};
    QColor m_slowColour{0x00, 0xff, 0x88};   // green
    QColor m_holdColour{0xff, 0xb8, 0x00};   // amber
    QColor m_fastColour{0xff, 0x44, 0x44};   // red

    // Settings (constructor values, MeterManager.cs:16764-16812 [v2.10.3.15])
    float m_verticalRatio{1.0f};
    float m_fontScale{1.0f};
    int m_upThreshold{540};
    int m_downThreshold{360};
    int m_tuneStepIntervalSeconds{2};
    int m_maxTuneStepChanges{3};
    int m_degreesPerChange{5};
    bool m_alignWithTuneStep{true};
    SliceSelect m_sliceSelect{SliceSelect::SourceSlice};

    // State
    int m_activeQuadrant{0};  // 0=top-left lit by default (the saved look)
    bool m_thetisLook{false};
    SpeedState m_speedState{SpeedState::Slow};
    bool m_accelEnabled{false};
    bool m_lockEnabled{false};

    // Slice feed
    QHash<int, DialSliceState> m_slices;
    int m_sourceSliceIndex{-1};
    int m_activeSliceIndex{-1};
    bool m_sourceFollowsSelection{false};

    // Thetis tuning state (clsDialDisplay fields, MeterManager.cs:16721-16762 [v2.10.3.15])
    float m_degrees{0.0f};
    float m_degTurned{0.0f};
    float m_degreesTotal{0.0f};
    float m_smoothedSpeed{0.0f};
    int m_direction{0};
    int m_oldDirection{0};
    int m_tuneStepDirection{0};
    int m_tuneStepChanges{0};
    int m_existingStepIndex{-1};
    bool m_ignoreNextVfoUpdate{false};
    bool m_pressed{false};
    int m_pressedSlice{-1};
    bool m_hasAccelerated{false};     // _last_accelerated != DateTime.MinValue
    qint64 m_lastAcceleratedMs{0};
    bool m_hasPreviousTime{false};    // _previous_time != DateTime.MinValue
    qint64 m_previousTimeMs{0};

    // Input
    int m_hoverQuadrant{-1};
    int m_pressQuadrant{-1};
    // Saved look: a press arms a drag that starts once the pointer moves.
    bool m_dragArmed{false};
    QPointF m_pressPos;
    QElapsedTimer m_clock;
    std::function<qint64()> m_clockForTest;
};

// New dials (makeEntry) take Thetis's dial.
// From Thetis MeterManager.cs:16764-16812 [v2.10.3.15] (clsDialDisplay
// constructor). The constructor's 3 changes is used, not the setup
// default of 4 (:27759).
struct DialItemThetisDefaults {
    QRgb textColour;
    QRgb circleColour;
    QRgb padColour;
    QRgb padPressedColour;
    QRgb buttonOnColour;
    QRgb buttonOffColour;
    QRgb buttonHighlightColour;
    QRgb ringColour;
    QRgb slowColour;
    QRgb holdColour;
    QRgb fastColour;
    int maxTuneStepChanges;
    bool alignWithTuneStep;
    int upThreshold;
    int downThreshold;
    int tuneStepIntervalSeconds;
    int degreesPerChange;
    float verticalRatio;
    float fontScale;
    bool accelerate;
    bool thetisLook;
};
inline constexpr DialItemThetisDefaults kDialThetisDefaults{
    qRgb(255, 255, 255),        // _text_colour = Color.White (:16797)
    qRgb(0, 0, 0),              // _circle_colour = Color.Black (:16798)
    qRgb(0, 0, 255),            // _pad_colour = Color.Blue (:16799)
    qRgb(255, 165, 0),          // _pad_pressed_colour = Color.Orange (:16800)
    qRgb(100, 149, 237),        // _button_on_colour = Color.CornflowerBlue (:16801)
    qRgb(0, 0, 0),              // _button_off_colour = Color.Black (:16802)
    qRgb(128, 128, 128),        // _button_highlight_colour = Color.Gray (:16803)
    qRgb(128, 128, 128),        // _ring_colour = Color.Gray (:16804)
    qRgb(0, 0, 255),            // _slow_colour = Color.Blue (:16805)
    qRgb(0, 128, 0),            // _hold_colour = Color.Green (:16806)
    qRgb(255, 0, 0),            // _fast_colour = Color.Red (:16807)
    3,                          // _max_tune_step_changes = 3 (:16789)
    true,                       // _align_with_tunestep = true (:16791)
    540,                        // _up_threshold = 540 (:16792)
    360,                        // _down_threshold = 360 (:16793)
    2,                          // _tune_step_interval_seconds = 2 (:16794)
    5,                          // _degrees_per_change = 5 (:16795)
    1.0f,                       // _vscale = 1f (:16780)
    1.0f,                       // _font_scale = 1f (:16781)
    false,                      // _accelerate = false (:16770)
    true,                       // renderDialDisplay's look (:35284-35460)
};

} // namespace NereusSDR
