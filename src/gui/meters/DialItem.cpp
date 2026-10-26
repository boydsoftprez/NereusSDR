// =================================================================
// src/gui/meters/DialItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Thetis dial tuning: Degrees, Pressed, undoTuneStep,
//                 Update, MouseUp and MouseWheel ported from clsDialDisplay
//                 (the marker drag and wheel tune the slice the dial
//                 follows, acceleration and its undo on release, the
//                 smoothed speed, the lock); the marker and pressed marker
//                 from renderDialDisplay; the top buttons step slices (they
//                 replace Thetis's VFOA / VFOB); appended settings and
//                 kDialThetisDefaults. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - Fix round 1 (ruling D5): paintThetis draws
//                 renderDialDisplay's look for new dials; a saved dial draws
//                 as before (no marker but while dragged, top-left lit, no
//                 centre letter) and starts a drag from a press and a move.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Screenshot fix: captionLayout(); ACCEL and LOCK that
//                 cannot fit in their corners at the readable text size
//                 sit under the dial, a space apart. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-03 — Responsive object text and measured role fitting by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
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

// --- From console.cs ---
//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
// ApacheLabs G2E support added throughout Thetis in various files, all changes marked  //N1GP G2E added
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
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Final modifictions by MW0LGE Richard Samphire - 19th April 2026
// Nothing further added by him after this date, and his repo is now in archive https://github.com/ramdor/Thetis
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

// Modification history (NereusSDR):
//   2026-10-12 - Apply the approved workspace-only 11 px text minimum.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

#include "gui/styles/WorkspacePreview.h"
#include "DialItem.h"
#include "ResponsiveText.h"
#include "models/SliceModel.h"

// From Thetis clsDialDisplay (MeterManager.cs:16719-17366 [v2.10.3.15])
// renderDialDisplay() (MeterManager.cs:35284-35460 [v2.10.3.15])

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>
#include <QGuiApplication>
#include <QStyleHints>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace NereusSDR {

namespace {

// From Thetis MeterManager.cs:16811 [v2.10.3.15]: UpdateInterval = 1000 / 60;
constexpr int kDialUpdateIntervalMs = 1000 / 60;
// From Thetis MeterManager.cs:17121 [v2.10.3.15]: UpdateInterval * 4 // over 4 frames
constexpr int kSpeedFrames = 4;
// From Thetis MeterManager.cs:17124 [v2.10.3.15]:
//   _smoothed_speed = (_smoothed_speed * 0.95f) + (degrees_per_second * 0.05f);
constexpr float kSpeedKeep = 0.95f;
constexpr float kSpeedNew = 0.05f;
// From Thetis MeterManager.cs:16835, :16844 [v2.10.3.15]: up and down stay
// 90 apart (_down_threshold + 90, _up_threshold - 90).
constexpr int kThresholdGap = 90;
// From Thetis MeterManager.cs:16915-16916 [v2.10.3.15]: deg_diff wraps
// at -180f / 180f by 360f.
constexpr float kHalfTurn = 180.0f;
constexpr float kFullTurn = 360.0f;
// From Thetis MeterManager.cs:16939 [v2.10.3.15]: if (turned >= 1)
constexpr float kMinTurned = 1.0f;
// From Thetis MeterManager.cs:27749-27760 [v2.10.3.15] (the setup
// ranges GetSetting clamps to).
constexpr float kVerticalRatioMin = 0.01f;
constexpr float kVerticalRatioMax = 1.0f;
constexpr float kFontScaleMin = 0.01f;
constexpr float kFontScaleMax = 1.1f;
constexpr int kThresholdMin = 90;
constexpr int kThresholdMax = 720;
constexpr int kIntervalMin = 1;
constexpr int kIntervalMax = 10;
constexpr int kMaxChangesMin = 1;
constexpr int kMaxChangesMax = 30;
constexpr int kDegreesMin = 1;
constexpr int kDegreesMax = 90;
// From Thetis MeterManager.cs:35355 [v2.10.3.15]:
//   float ring_radius = (w * 0.65f) / 2f;
constexpr float kRingDiameterRatio = 0.65f;
// From Thetis MeterManager.cs:35386 [v2.10.3.15]: float marker_radius = w * 0.125f;
constexpr float kMarkerRadiusRatio = 0.125f;
// NereusSDR: the centre disc (the speed dot and the slice letter) inside
// the ring's inner edge is not a button.
// From Thetis MeterManager.cs:35364 [v2.10.3.15]:
//   if (dial.Pressed && Math.Abs(dial.SmoothedSpeed) >= 0.1f)
constexpr float kSpeedRingMin = 0.1f;
// From Thetis MeterManager.cs:35377 [v2.10.3.15]: (w * 0.025f)
constexpr double kSpeedRingWidthRatio = 0.025;
// From Thetis MeterManager.cs:35381 [v2.10.3.15]: (w * 0.005f)
constexpr double kDottedRingWidthRatio = 0.005;
// From Thetis MeterManager.cs:35425-35426 [v2.10.3.15]: 38f * dial.FontScale,
// / 16f, * (rect.Width / 52f)
constexpr double kThetisFontSize = 38.0;
constexpr double kThetisFontEmDivisor = 16.0;
constexpr double kThetisFontWidthDivisor = 52.0;
constexpr float kInnerDiscRatio = kRingDiameterRatio / 2.0f - kMarkerRadiusRatio;
// The saved look's caption: 9 px on a 120 px dial.
constexpr double kSavedCaptionPixels = 9.0;
constexpr double kSavedCaptionDial = 120.0;
// NereusSDR: the gap between the dial and captions drawn under it.
constexpr int kCaptionGapPx = 2;

constexpr int kTopLeft = 0;
constexpr int kTopRight = 1;
constexpr int kBottomLeft = 2;
constexpr int kBottomRight = 3;

QString sliceLetter(int sliceIndex)
{
    return sliceIndex >= 0 ? QString(QChar(u'A' + sliceIndex)) : QString();
}

} // namespace

DialItem::DialItem(QObject* parent)
    : MeterItem(parent)
    , m_padPressedColour(QColor::fromRgb(kDialThetisDefaults.padPressedColour))
{
    m_clock.start();
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
void DialItem::setVerticalRatio(float ratio)
{
    m_verticalRatio = std::clamp(ratio, kVerticalRatioMin, kVerticalRatioMax);
}

void DialItem::setFontScale(float scale)
{
    m_fontScale = std::clamp(scale, kFontScaleMin, kFontScaleMax);
}

// From Thetis MeterManager.cs:16830-16838 [v2.10.3.15] (UpThreshold)
void DialItem::setUpThreshold(int degreesPerSecond)
{
    int value = std::clamp(degreesPerSecond, kThresholdMin, kThresholdMax);
    if (value < m_downThreshold + kThresholdGap) { value = m_downThreshold + kThresholdGap; }
    m_upThreshold = value;
}

// From Thetis MeterManager.cs:16839-16847 [v2.10.3.15] (DownThreshold)
void DialItem::setDownThreshold(int degreesPerSecond)
{
    int value = std::clamp(degreesPerSecond, kThresholdMin, kThresholdMax);
    if (value > m_upThreshold - kThresholdGap) { value = m_upThreshold - kThresholdGap; }
    m_downThreshold = value;
}

void DialItem::setTuneStepIntervalSeconds(int seconds)
{
    m_tuneStepIntervalSeconds = std::clamp(seconds, kIntervalMin, kIntervalMax);
}

void DialItem::setMaxTuneStepChanges(int changes)
{
    m_maxTuneStepChanges = std::clamp(changes, kMaxChangesMin, kMaxChangesMax);
}

void DialItem::setDegreesPerChange(int degrees)
{
    m_degreesPerChange = std::clamp(degrees, kDegreesMin, kDegreesMax);
}

void DialItem::setSliceSelect(SliceSelect select)
{
    m_sliceSelect = select == SliceSelect::AnySlice ? SliceSelect::AnySlice : SliceSelect::SourceSlice;
    syncLock();
}

// ---------------------------------------------------------------------------
// Slice feed
// ---------------------------------------------------------------------------
void DialItem::clearSliceStates()
{
    m_slices.clear();
    syncLock();
}

void DialItem::setSliceState(const DialSliceState& state)
{
    if (state.index < 0) { return; }
    m_slices.insert(state.index, state);
    syncLock();
}

DialSliceState DialItem::sliceState(int sliceIndex) const
{
    return m_slices.value(sliceIndex);
}

void DialItem::setSourceSliceIndex(int sliceIndex)
{
    m_sourceSliceIndex = sliceIndex;
    syncLock();
}

void DialItem::setActiveSliceIndex(int sliceIndex)
{
    m_activeSliceIndex = sliceIndex;
    syncLock();
}

void DialItem::setSourceFollowsSelection(bool follows)
{
    m_sourceFollowsSelection = follows;
}

int DialItem::tunedSlice() const
{
    const int slice = m_sliceSelect == SliceSelect::AnySlice ? m_activeSliceIndex : m_sourceSliceIndex;
    return m_slices.contains(slice) ? slice : -1;
}

int DialItem::previousSlice() const
{
    const int tuned = tunedSlice();
    if (tuned < 0) { return -1; }
    int best = -1;
    for (auto it = m_slices.cbegin(); it != m_slices.cend(); ++it) {
        if (it.key() < tuned && it.key() > best) { best = it.key(); }
    }
    return best;
}

int DialItem::nextSlice() const
{
    const int tuned = tunedSlice();
    if (tuned < 0) { return -1; }
    int best = -1;
    for (auto it = m_slices.cbegin(); it != m_slices.cend(); ++it) {
        if (it.key() > tuned && (best < 0 || it.key() < best)) { best = it.key(); }
    }
    return best;
}

bool DialItem::sliceStepEnabled() const
{
    // Selecting a slice moves the dial only when it tunes the active slice,
    // or its Source follows the selection.
    return m_sliceSelect == SliceSelect::AnySlice || m_sourceFollowsSelection;
}

QString DialItem::presentationKey() const
{
    return QStringLiteral("%1/%2/%3/%4/%5")
        .arg(tunedSlice()).arg(previousSlice()).arg(nextSlice())
        .arg(sliceStepEnabled() ? 1 : 0).arg(m_lockEnabled ? 1 : 0);
}

// From Thetis MeterManager.cs:17170-17173 [v2.10.3.15] (Lock): the tuned
// VFO's lock.
void DialItem::syncLock()
{
    const int slice = tunedSlice();
    setLockEnabled(slice >= 0 && m_slices.value(slice).locked);
}

// ---------------------------------------------------------------------------
// Tuning model
// ---------------------------------------------------------------------------
qint64 DialItem::tunedFrequencyHz(qint64 currentHz, qint64 deltaHz, bool align)
{
    // From Thetis MeterManager.cs:16951-16975 [v2.10.3.15]:
    //   freq = Math.Round((_console.VFOAFreq + freq) / (_console.CurrentTuneStepHz * 1e-6)) * (_console.CurrentTuneStepHz * 1e-6);
    // Math.Round rounds a half to even, as std::nearbyint does by default.
    const qint64 target = currentHz + deltaHz;
    const qint64 step = deltaHz < 0 ? -deltaHz : deltaHz;
    if (!align || step == 0) { return target; }
    const double steps = std::nearbyint(static_cast<double>(target) / static_cast<double>(step));
    return static_cast<qint64>(steps) * step;
}

// From Thetis console.cs:6124-6134 [v2.10.3.15] (ChangeTuneStepUp /
// ChangeTuneStepDown): the index wraps round the list.
void DialItem::changeTuneStep(int slice, int direction)
{
    if (slice < 0 || !m_slices.contains(slice)) { return; }
    const int index = tuneStepIndexForHz(m_slices.value(slice).stepHz);
    if (index < 0) { return; }
    //MW0LGE_21j
    const int next = (index + direction + kTuneStepListSize) % kTuneStepListSize;
    const int hz = kTuneStepList[next].stepHz;
    m_slices[slice].stepHz = hz;
    emit stepChangeRequested(slice, hz);
}

// From Thetis MeterManager.cs:16907-17030 [v2.10.3.15] (Degrees setter)
void DialItem::setDegrees(float value, qint64 nowMs)
{
    if (m_lockEnabled) { return; }
    if (m_degrees == value) { return; }

    float degDiff = value - m_degrees;
    if (degDiff < -kHalfTurn) { degDiff += kFullTurn; }
    if (degDiff > kHalfTurn) { degDiff -= kFullTurn; }

    m_degrees = value;

    if (m_ignoreNextVfoUpdate) {
        m_ignoreNextVfoUpdate = false;
        return;
    }

    degDiff = -degDiff; //-ve so that +ve clockwise
    m_degTurned += degDiff;

    m_degreesTotal += std::abs(degDiff);

    m_oldDirection = m_direction;
    if (degDiff == 0) {
        m_direction = 0;
    } else if (degDiff > 0) {
        m_direction = 1;
    } else {
        m_direction = -1;
    }

    // The slice a drag started on (Thetis has one console); the wheel's
    // is the tuned slice.
    const int slice = m_pressed ? m_pressedSlice : tunedSlice();

    const float turned = std::abs(m_degTurned);
    if (turned >= kMinTurned) {
        if (turned >= static_cast<float>(m_degreesPerChange)) {
            m_degTurned -= static_cast<float>(static_cast<int>(m_degTurned));

            const int steps = m_direction;

            if (slice >= 0 && m_slices.contains(slice)) {
                // The frequency itself (and Align) is applied by the
                // receiver of the request, on the slice's live frequency.
                const qint64 stepHz = m_slices.value(slice).stepHz;
                if (steps != 0 && stepHz > 0) {
                    emit tuneRequested(slice, static_cast<qint64>(steps) * stepHz);
                }
            }
        }

        // accelerate
        if (m_accelEnabled) {
            const float speed = std::abs(m_smoothedSpeed);
            const qint64 intervalMs = static_cast<qint64>(m_tuneStepIntervalSeconds) * 1000;
            const bool intervalPassed = !m_hasAccelerated || (nowMs - m_lastAcceleratedMs) >= intervalMs;
            if (speed >= static_cast<float>(m_upThreshold)) {
                if ((m_tuneStepChanges < m_maxTuneStepChanges) && intervalPassed) {
                    changeTuneStep(slice, 1);
                    m_lastAcceleratedMs = nowMs;
                    m_hasAccelerated = true;

                    m_tuneStepChanges++;
                }
                m_tuneStepDirection = 1;
            } else if (speed <= static_cast<float>(m_downThreshold)) {
                const int current = slice >= 0 ? tuneStepIndexForHz(m_slices.value(slice).stepHz) : -1;
                if (m_existingStepIndex != -1 && m_existingStepIndex < current) {
                    if (intervalPassed) {
                        changeTuneStep(slice, -1);
                        m_lastAcceleratedMs = nowMs;
                        m_hasAccelerated = true;
                    }

                    m_tuneStepChanges--;
                    if (m_tuneStepChanges < 0) { m_tuneStepChanges = 0; }
                }
                m_tuneStepDirection = -1;
            } else {
                m_tuneStepDirection = 0;
            }
            // NereusSDR: the existing speed indicator shows the direction
            // (Thetis's speed ring colours, MeterManager.cs:35359-35378 [v2.10.3.15]).
            setSpeedState(m_tuneStepDirection > 0 ? SpeedState::Fast
                          : m_tuneStepDirection < 0 ? SpeedState::Slow : SpeedState::Hold);
        }
    }
}

// From Thetis MeterManager.cs:17074-17098 [v2.10.3.15] (Pressed setter)
void DialItem::setPressed(bool pressed)
{
    if (m_pressed == pressed) { return; }
    m_pressed = pressed;
    m_smoothedSpeed = 0;
    m_degTurned = 0;
    m_degreesTotal = 0;
    m_ignoreNextVfoUpdate = true;
    m_tuneStepChanges = 0;

    if (m_pressed) {
        m_pressedSlice = tunedSlice();
        m_existingStepIndex = m_pressedSlice >= 0
            ? tuneStepIndexForHz(m_slices.value(m_pressedSlice).stepHz) : -1;
    } else {
        undoTuneStep();
        m_pressedSlice = -1;
    }
}

// From Thetis MeterManager.cs:17099-17111 [v2.10.3.15] (undoTuneStep)
void DialItem::undoTuneStep()
{
    if (m_existingStepIndex != -1) {
        const int step = m_existingStepIndex;
        const int slice = m_pressedSlice;
        if (slice >= 0 && m_slices.contains(slice)) {
            m_slices[slice].stepHz = kTuneStepList[step].stepHz;
            emit stepChangeRequested(slice, kTuneStepList[step].stepHz);
        }
        m_existingStepIndex = -1;
        m_hasAccelerated = false;
    }
}

// From Thetis MeterManager.cs:17116-17129 [v2.10.3.15] (Update)
void DialItem::updateSpeed(qint64 nowMs)
{
    const bool overFrames = !m_hasPreviousTime
        || (nowMs - m_previousTimeMs) > static_cast<qint64>(kDialUpdateIntervalMs) * kSpeedFrames;
    if (m_pressed && overFrames) // over 4 frames
    {
        // _previous_time starts at DateTime.MinValue: the first interval is
        // too long to give a speed.
        const float degreesPerSecond = m_hasPreviousTime
            ? m_degreesTotal / (static_cast<float>(nowMs - m_previousTimeMs) / 1000.0f) : 0.0f;
        m_smoothedSpeed = (m_smoothedSpeed * kSpeedKeep) + (degreesPerSecond * kSpeedNew);

        m_degreesTotal = 0;
        m_previousTimeMs = nowMs;
        m_hasPreviousTime = true;
    }
}

bool DialItem::advanceMeter(qint64 monotonicMs)
{
    Q_UNUSED(monotonicMs);
    // Thetis updates the speed every frame; the dial's own clock keeps one
    // time base with its mouse handlers. Nothing drawn changes here.
    updateSpeed(nowMs());
    return false;
}

qint64 DialItem::nowMs() const
{
    return m_clockForTest ? m_clockForTest() : m_clock.elapsed();
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
bool DialItem::interactive() const
{
    // A preview copy (its signals blocked) never acts.
    return !signalsBlocked();
}

bool DialItem::hitTest(const QPointF& pos, int widgetW, int widgetH) const
{
    // A held marker keeps the drag (and its release) wherever the pointer goes.
    return m_pressed || m_dragArmed || MeterItem::hitTest(pos, widgetW, widgetH);
}

float DialItem::angleAt(const QPointF& pos, const QRect& dialRect) const
{
    // From Thetis MeterManager.cs:35408-35413 [v2.10.3.15]
    const float w = static_cast<float>(dialRect.width());
    const float h = static_cast<float>(dialRect.height());
    if (w <= 0 || h <= 0) { return m_degrees; }
    const float mouseX = (static_cast<float>(pos.x()) - static_cast<float>(dialRect.left())) / w;
    const float mouseY = (static_cast<float>(pos.y()) - static_cast<float>(dialRect.top())) / h;
    const float relativeX = (mouseX - 0.5f) * 2.0f;
    const float relativeY = (0.5f - mouseY) * 2.0f;
    float degrees = std::atan2(relativeY, relativeX) * kHalfTurn / std::numbers::pi_v<float>;
    if (degrees < 0) { degrees += kFullTurn; }
    return degrees;
}

bool DialItem::onMarker(const QPointF& pos, const QRect& dialRect) const
{
    // From Thetis MeterManager.cs:35390-35398 [v2.10.3.15]
    const float w = static_cast<float>(dialRect.width());
    const float ringRadius = (w * kRingDiameterRatio) / 2.0f;
    const float markerRadius = w * kMarkerRadiusRatio;
    const float rad = m_degrees * std::numbers::pi_v<float> / kHalfTurn;
    const QPointF centre = QRectF(dialRect).center();
    const float markerX = static_cast<float>(centre.x()) + std::cos(rad) * ringRadius;
    const float markerY = static_cast<float>(centre.y()) - std::sin(rad) * ringRadius;
    const float dx = static_cast<float>(pos.x()) - markerX;
    const float dy = static_cast<float>(pos.y()) - markerY;
    return dx * dx + dy * dy <= markerRadius * markerRadius;
}

int DialItem::buttonAt(const QPointF& pos, const QRect& dialRect) const
{
    if (!QRectF(dialRect).contains(pos) || (m_thetisLook && onMarker(pos, dialRect))) { return -1; }
    const QPointF centre = QRectF(dialRect).center();
    const double dx = pos.x() - centre.x();
    const double dy = pos.y() - centre.y();
    const double inner = dialRect.width() * kInnerDiscRatio;
    if (dx * dx + dy * dy <= inner * inner) { return -1; }
    //corner buttons
    //quadrant,     0 1
    //              2 3
    const bool right = pos.x() >= centre.x();
    const bool bottom = pos.y() >= centre.y();
    return (bottom ? 2 : 0) + (right ? 1 : 0);
}

bool DialItem::buttonEnabled(int quadrant) const
{
    switch (quadrant) {
        case kTopLeft:     return sliceStepEnabled() && previousSlice() >= 0;
        case kTopRight:    return sliceStepEnabled() && nextSlice() >= 0;
        case kBottomLeft:  return true;
        case kBottomRight: return tunedSlice() >= 0;
        default:           return false;
    }
}

bool DialItem::handleMousePress(QMouseEvent* event, int widgetW, int widgetH)
{
    if (!event || event->button() != Qt::LeftButton || !interactive()) { return false; }
    const QRect dialRect = squareRect(widgetW, widgetH);
    const QPointF pos = event->position();
    // From Thetis MeterManager.cs:35395-35399 [v2.10.3.15]: the marker is
    // pressed when the button goes down on it.
    if (!m_thetisLook) {
        // The saved look shows no marker: a press anywhere on the dial arms
        // a drag, which starts once the pointer moves; a press and release
        // without moving is the quadrant's button.
        if (!QRectF(dialRect).contains(pos)) { return false; }
        m_dragArmed = true;
        m_pressPos = pos;
        m_pressQuadrant = buttonAt(pos, dialRect);
        return true;
    }
    if (onMarker(pos, dialRect)) {
        m_pressQuadrant = -1;
        m_hoverQuadrant = -1;
        setPressed(true);
        // The first update after a press only moves the marker
        // (_ignore_next_vfo_update).
        setDegrees(angleAt(pos, dialRect), nowMs());
        return true;
    }
    m_pressQuadrant = buttonAt(pos, dialRect);
    return m_pressQuadrant >= 0;
}

bool DialItem::handleMouseMove(QMouseEvent* event, int widgetW, int widgetH)
{
    if (!event || !interactive()) { return false; }
    const QRect dialRect = squareRect(widgetW, widgetH);
    const QPointF pos = event->position();
    if (m_dragArmed && !m_pressed) {
        const QPointF moved = pos - m_pressPos;
        if (moved.manhattanLength() < QGuiApplication::styleHints()->startDragDistance()) { return false; }
        m_pressQuadrant = -1;
        setPressed(true);
        // The first update after a press only moves the marker (to where
        // the drag started).
        setDegrees(angleAt(m_pressPos, dialRect), nowMs());
    }
    if (m_pressed) {
        const qint64 now = nowMs();
        updateSpeed(now);
        setDegrees(angleAt(pos, dialRect), now);
        return true;
    }
    // From Thetis MeterManager.cs:35335-35338 [v2.10.3.15]: a button
    // highlights under the mouse while the marker is not held (the Thetis
    // look; the saved look had no highlight).
    if (!m_thetisLook) { return false; }
    int hover = buttonAt(pos, dialRect);
    if (!buttonEnabled(hover)) { hover = -1; }
    if (hover == m_hoverQuadrant) { return false; }
    m_hoverQuadrant = hover;
    return true;
}

// From Thetis MeterManager.cs:17134-17156 [v2.10.3.15] (MouseUp)
bool DialItem::handleMouseRelease(QMouseEvent* event, int widgetW, int widgetH)
{
    if (!event || event->button() != Qt::LeftButton) { return false; }
    m_dragArmed = false;
    if (m_pressed) {
        // The render loop drops Pressed when the button is up
        // (MeterManager.cs:35418-35421 [v2.10.3.15]).
        setPressed(false);
        return true;
    }
    const int pressed = m_pressQuadrant;
    m_pressQuadrant = -1;
    if (pressed < 0 || !interactive()) { return false; }
    const int released = buttonAt(event->position(), squareRect(widgetW, widgetH));
    if (released != pressed || !buttonEnabled(released)) { return true; }
    switch (released) {
        case kTopLeft:
            emit sliceSelectRequested(previousSlice());
            break;
        case kTopRight:
            emit sliceSelectRequested(nextSlice());
            break;
        case kBottomLeft:
            m_accelEnabled = !m_accelEnabled;
            break;
        case kBottomRight:
            emit lockToggleRequested(tunedSlice());
            break;
        default:
            break;
    }
    return true;
}

// From Thetis MeterManager.cs:17157-17164 [v2.10.3.15] (MouseWheel)
bool DialItem::handleWheel(QWheelEvent* event, int widgetW, int widgetH)
{
    Q_UNUSED(widgetW); Q_UNUSED(widgetH);
    if (!event || !interactive()) { return false; }
    const int delta = event->angleDelta().y();
    if (delta == 0) { return false; }
    if (!m_pressed) {
        const int sign = delta > 0 ? 1 : -1;
        setDegrees(m_degrees - static_cast<float>(m_degreesPerChange * sign), nowMs());
    }
    return true;
}

bool DialItem::handleMouseLeave()
{
    if (m_hoverQuadrant < 0) { return false; }
    m_hoverQuadrant = -1;
    return true;
}

QString DialItem::reasonAt(const QPointF& pos, int widgetW, int widgetH) const
{
    const int quadrant = buttonAt(pos, squareRect(widgetW, widgetH));
    if ((quadrant == kTopLeft || quadrant == kTopRight) && !sliceStepEnabled()) {
        return tr("This dial tunes its Source slice. Set it to tune Any slice to step slices here.");
    }
    return {};
}

void DialItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    const DialItemThetisDefaults& d = kDialThetisDefaults;
    m_textColour       = QColor::fromRgb(d.textColour);
    m_circleColour     = QColor::fromRgb(d.circleColour);
    m_padColour        = QColor::fromRgb(d.padColour);
    m_padPressedColour = QColor::fromRgb(d.padPressedColour);
    m_btnOnColour      = QColor::fromRgb(d.buttonOnColour);
    m_btnOffColour     = QColor::fromRgb(d.buttonOffColour);
    m_btnHighlight     = QColor::fromRgb(d.buttonHighlightColour);
    m_ringColour       = QColor::fromRgb(d.ringColour);
    m_slowColour       = QColor::fromRgb(d.slowColour);
    m_holdColour       = QColor::fromRgb(d.holdColour);
    m_fastColour       = QColor::fromRgb(d.fastColour);
    m_maxTuneStepChanges      = d.maxTuneStepChanges;
    m_alignWithTuneStep       = d.alignWithTuneStep;
    m_upThreshold             = d.upThreshold;
    m_downThreshold           = d.downThreshold;
    m_tuneStepIntervalSeconds = d.tuneStepIntervalSeconds;
    m_degreesPerChange        = d.degreesPerChange;
    m_verticalRatio           = d.verticalRatio;
    m_fontScale               = d.fontScale;
    m_accelEnabled            = d.accelerate;
    m_thetisLook              = d.thetisLook;
}

// ---------------------------------------------------------------------------
// participatesIn()
// DialItem renders to OverlayStatic (main circle, dashed ring, bottom labels)
// and OverlayDynamic (buttons, slice labels, speed dot, marker).
// ---------------------------------------------------------------------------
bool DialItem::participatesIn(Layer layer) const
{
    return layer == Layer::OverlayStatic || layer == Layer::OverlayDynamic;
}

// ---------------------------------------------------------------------------
// squareRect()
// Compute a square centered within the item's pixel rect, scaled by the
// vertical ratio (Thetis dial.Size = (VScale, VScale), centred,
// MeterManager.cs:27776-27777 [v2.10.3.15]).
// ---------------------------------------------------------------------------
QRect DialItem::squareRect(int widgetW, int widgetH) const
{
    return captionLayout(widgetW, widgetH).dial;
}

// ---------------------------------------------------------------------------
// captionPixels()
// The Thetis look: MeterManager.cs:35425-35426 [v2.10.3.15]:
//   float font_size_scaled = 38f * dial.FontScale;
//   float fontSizeEmScaled = (font_size_scaled / 16f) * (rect.Width / 52f);
// rect is the whole meter's width, so the text does not shrink with the
// dial's size. The saved look scales 9 px with a 120 px dial.
// ---------------------------------------------------------------------------
double DialItem::captionPixels(const QRect& dialRect, int meterWidth) const
{
    if (m_thetisLook) {
        return (kThetisFontSize * static_cast<double>(m_fontScale) / kThetisFontEmDivisor)
            * (static_cast<double>(meterWidth) / kThetisFontWidthDivisor);
    }
    return kSavedCaptionPixels * objectTextScale(dialRect, {kSavedCaptionDial, kSavedCaptionDial})
        * static_cast<double>(m_fontScale);
}

// ---------------------------------------------------------------------------
// captionLayout()
// NereusSDR: the readable text size can make ACCEL and LOCK wider than
// their corners on a small dial (a 72 px stack row), where they ran into
// each other and over the ring. Then they sit under the dial instead.
// ---------------------------------------------------------------------------
DialItem::CaptionLayout DialItem::captionLayout(int widgetW, int widgetH) const
{
    const QRect pr = pixelRect(widgetW, widgetH);
    // Compute a square centered within `area`, scaled by the vertical
    // ratio (Thetis dial.Size = (VScale, VScale), centred,
    // MeterManager.cs:27776-27777 [v2.10.3.15]).
    const auto squareIn = [this](const QRect& area) {
        const int full = std::min(area.width(), area.height());
        const int side = m_verticalRatio >= kVerticalRatioMax
            ? full : std::max(1, static_cast<int>(std::lround(full * static_cast<double>(m_verticalRatio))));
        return QRect(area.left() + (area.width()  - side) / 2,
                     area.top()  + (area.height() - side) / 2,
                     side, side);
    };
    CaptionLayout layout;
    layout.dial = squareIn(pr);
    const QRect& d = layout.dial;
    const double size = captionPixels(d, pr.width());
    const QString accel = m_thetisLook ? QStringLiteral(" ACCEL ") : QStringLiteral("ACCEL");
    const QString lock = m_thetisLook ? QStringLiteral(" LOCK ") : QStringLiteral("LOCK");
    const int hw = d.width() / 2;
    const int hh = d.height() / 2;
    const QRectF accelZone = m_thetisLook ? QRectF(d.left(), d.top() + d.height() / 2.0, d.width() / 2.0, d.height() / 2.0)
                                          : QRectF(QRect(d.left(), d.top() + hh, hw, hh));
    const QRectF lockZone = m_thetisLook ? QRectF(d.left() + d.width() / 2.0, d.top() + d.height() / 2.0,
                                                  d.width() / 2.0, d.height() / 2.0)
                                         : QRectF(QRect(d.left() + hw, d.top() + hh, hw, hh));
    const int accelAlign = m_thetisLook ? int(Qt::AlignLeft | Qt::AlignBottom) : int(Qt::AlignCenter);
    const int lockAlign = m_thetisLook ? int(Qt::AlignRight | Qt::AlignBottom) : int(Qt::AlignCenter);
    // In its corner, as paintStatic / paintThetis draw it there.
    bool fits = true;
    const auto inCorner = [&](const QString& text, const QRectF& zone, int align) {
        layout.font = fitObjectText(QFont(), size, text, zone, false, Qt::TextSingleLine);
        const QFontMetricsF metrics(layout.font);
        const QRectF ink = metrics.boundingRect(zone, align | Qt::TextSingleLine, text);
        fits = fits && ink.width() <= zone.width() && metrics.height() <= zone.height();
        return ink;
    };
    layout.accel = inCorner(accel, accelZone, accelAlign);
    layout.lock = inCorner(lock, lockZone, lockAlign);
    if (fits) {
        return layout;
    }
    // Under the dial: no larger than the saved look's size for this dial
    // (the Thetis size follows the meter's width, too large for a dial
    // this small) and at least the readable size, a space either side of
    // the centre, and the dial shrunk by the caption's height.
    const double belowSize = std::min(size, kSavedCaptionPixels
        * objectTextScale(d, {kSavedCaptionDial, kSavedCaptionDial}) * static_cast<double>(m_fontScale));
    QFont font;
    font.setPixelSize(WorkspacePreview::readableTextPixelSize(std::max(1, static_cast<int>(std::floor(belowSize)))));
    const QFontMetricsF metrics(font);
    const int captionH = static_cast<int>(std::ceil(metrics.height()));
    const QRect area = pr.adjusted(0, 0, 0, -(captionH + kCaptionGapPx));
    if (area.height() < captionH) {
        return layout;
    }
    const QRect dial = squareIn(area);
    const double centre = dial.left() + dial.width() / 2.0;
    const double space = metrics.horizontalAdvance(QLatin1Char(' '));
    const double accelW = metrics.horizontalAdvance(accel.trimmed());
    const double lockW = metrics.horizontalAdvance(lock.trimmed());
    if (centre - space - accelW < pr.left() || centre + space + lockW > pr.left() + pr.width()) {
        return layout;
    }
    const double top = dial.bottom() + 1 + kCaptionGapPx;
    layout.dial = dial;
    layout.below = true;
    layout.font = font;
    layout.accel = QRectF(centre - space - accelW, top, accelW, captionH);
    layout.lock = QRectF(centre + space, top, lockW, captionH);
    return layout;
}

// ---------------------------------------------------------------------------
// paintStatic()
// From Thetis renderDialDisplay() (MeterManager.cs:35284-35460 [v2.10.3.15])
// Draws: main circle fill, dashed ring, quadrant dividers, ACCEL and LOCK.
// ---------------------------------------------------------------------------
void DialItem::paintStatic(QPainter& p, const CaptionLayout& layout)
{
    const QRect& dialRect = layout.dial;
    p.setRenderHint(QPainter::Antialiasing, true);

    //large circle
    // Step 1: fill main circle
    p.setBrush(m_circleColour);
    p.setPen(Qt::NoPen);
    p.drawEllipse(dialRect);

    //dotted ring
    // Step 2: dashed ring inset by 4px
    QPen ringPen(m_ringColour, 2, Qt::DashLine);
    p.setPen(ringPen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(dialRect.adjusted(4, 4, -4, -4));

    // Step 3: cross lines dividing circle into four quadrants
    // Thin lines through center in ring colour
    const QPoint center = dialRect.center();
    QPen linePen(m_ringColour, 1);
    p.setPen(linePen);
    p.drawLine(center.x(), dialRect.top(), center.x(), dialRect.bottom());
    p.drawLine(dialRect.left(), center.y(), dialRect.right(), center.y());

    //button text
    // Step 4: bottom labels; the font scale is Thetis's
    // (font_size_scaled = 38f * dial.FontScale, MeterManager.cs:35425 [v2.10.3.15]).
    QFont labelFont;
    const double size = captionPixels(dialRect, 0);
    labelFont.setPixelSize(WorkspacePreview::readableTextPixelSize(qMax(1, qRound(size))));
    p.setFont(labelFont);
    p.setPen(m_textColour);
    p.setBrush(Qt::NoBrush);

    if (layout.below) {
        p.setFont(layout.font);
        p.drawText(layout.accel, Qt::AlignCenter, QStringLiteral("ACCEL"));
        p.drawText(layout.lock, Qt::AlignCenter, QStringLiteral("LOCK"));
        return;
    }

    const int hw = dialRect.width()  / 2;
    const int hh = dialRect.height() / 2;
    const int l  = dialRect.left();
    const int t  = dialRect.top();

    // Bottom-left quadrant — "ACCEL"
    const QRect blRect(l, t + hh, hw, hh);
    drawObjectText(p, blRect, QStringLiteral("ACCEL"), size);

    // Bottom-right quadrant — "LOCK"
    const QRect brRect(l + hw, t + hh, hw, hh);
    drawObjectText(p, brRect, QStringLiteral("LOCK"), size);
}

// ---------------------------------------------------------------------------
// paintDynamic()
// The saved look (dials placed before the tuning port), as it was drawn:
// active quadrant, accel/lock tints, speed indicator dot. The top labels
// name slices (decision 2's wording) and sit under the fills as the old
// static labels did. The marker shows only while it is dragged.
// ---------------------------------------------------------------------------
void DialItem::paintDynamic(QPainter& p, const QRect& dialRect)
{
    p.setRenderHint(QPainter::Antialiasing, true);

    const QPoint center = dialRect.center();
    const int hw = dialRect.width()  / 2;
    const int hh = dialRect.height() / 2;
    const int l  = dialRect.left();
    const int t  = dialRect.top();

    // Helper lambda: fill a quadrant-rect clipped to the circle with given color+alpha
    auto fillQuadrant = [&](const QRect& qRect, const QColor& baseColor, int alpha) {
        QPainterPath circlePath;
        circlePath.addEllipse(dialRect);

        QPainterPath quadPath;
        quadPath.addRect(qRect);

        const QPainterPath clipped = circlePath.intersected(quadPath);

        QColor fillColor = baseColor;
        fillColor.setAlpha(alpha);
        p.setBrush(fillColor);
        p.setPen(Qt::NoPen);
        p.drawPath(clipped);
    };

    // Quadrant rect lookup
    // 0=top-left(previous slice), 1=top-right(next slice), 2=bottom-left(ACCEL), 3=bottom-right(LOCK)
    const QRect quadRects[4] = {
        QRect(l,      t,      hw, hh),   // 0: previous slice, top-left
        QRect(l + hw, t,      hw, hh),   // 1: next slice, top-right
        QRect(l,      t + hh, hw, hh),   // 2: ACCEL bottom-left
        QRect(l + hw, t + hh, hw, hh),   // 3: LOCK bottom-right
    };

    // Top labels: the slice each button selects ("<" / ">" when none).
    const double size = 9 * objectTextScale(dialRect, {120, 120}) * static_cast<double>(m_fontScale);
    QFont labelFont;
    labelFont.setPixelSize(WorkspacePreview::readableTextPixelSize(qMax(1, qRound(size))));
    p.setFont(labelFont);
    p.setPen(m_textColour);
    p.setBrush(Qt::NoBrush);
    const int previous = sliceStepEnabled() ? previousSlice() : -1;
    const int next = sliceStepEnabled() ? nextSlice() : -1;
    drawObjectText(p, quadRects[0],
                   previous >= 0 ? QStringLiteral("< ") + sliceLetter(previous) : QStringLiteral("<"), size);
    drawObjectText(p, quadRects[1],
                   next >= 0 ? sliceLetter(next) + QStringLiteral(" >") : QStringLiteral(">"), size);

    // Step 1: highlight active quadrant at alpha 0.3 (≈76/255)
    if (m_activeQuadrant >= 0 && m_activeQuadrant <= 3) {
        fillQuadrant(quadRects[m_activeQuadrant], m_btnOnColour, 76);
    }

    // Step 2: accel tint (bottom-left) at alpha 0.2 (≈51/255)
    if (m_accelEnabled) {
        fillQuadrant(quadRects[2], m_btnOnColour, 51);
    }

    // Step 3: lock tint (bottom-right) at alpha 0.2 (≈51/255)
    if (m_lockEnabled) {
        fillQuadrant(quadRects[3], m_btnOnColour, 51);
    }

    // Step 4: speed indicator dot at center
    QColor speedColor;
    switch (m_speedState) {
        case SpeedState::Slow: speedColor = m_slowColour; break;
        case SpeedState::Hold: speedColor = m_holdColour; break;
        case SpeedState::Fast: speedColor = m_fastColour; break;
    }

    const int dotR = dialRect.width() / 10;
    p.setBrush(speedColor);
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, dotR, dotR);

    if (m_pressed) { paintMarker(p, dialRect); }
}

// ---------------------------------------------------------------------------
// paintMarker()
// From Thetis MeterManager.cs:35435-35441 [v2.10.3.15]: the pad on the ring
// at the dial's angle, in the pressed colour while held.
// ---------------------------------------------------------------------------
void DialItem::paintMarker(QPainter& p, const QRect& dialRect)
{
    //marker circle
    const double w = dialRect.width();
    const double ringRadius = (w * kRingDiameterRatio) / 2.0;
    const double markerRadius = w * kMarkerRadiusRatio;
    const double radians = static_cast<double>(m_degrees) * std::numbers::pi / static_cast<double>(kHalfTurn);
    const QPointF centre = QRectF(dialRect).center();
    const QPointF marker(centre.x() + std::cos(radians) * ringRadius,
                         centre.y() - std::sin(radians) * ringRadius);
    p.setBrush(m_pressed ? m_padPressedColour : m_padColour);
    p.setPen(Qt::NoPen);
    p.drawEllipse(marker, markerRadius, markerRadius);
}

// ---------------------------------------------------------------------------
// paintThetis()
// From Thetis renderDialDisplay() (MeterManager.cs:35284-35460 [v2.10.3.15]):
// the four corner zones, the large circle, the speed ring while
// accelerating, the dotted ring, the corner labels and the marker.
// ---------------------------------------------------------------------------
void DialItem::paintThetis(QPainter& p, const CaptionLayout& layout, int widgetW, int widgetH)
{
    const QRect& dialRect = layout.dial;
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r(dialRect);
    const double x = r.left();
    const double y = r.top();
    const double w = r.width();
    const double h = r.height();
    const QPointF centre = r.center();

    //corner buttons
    //quadrant,     0 1
    //              2 3
    const QRectF zones[4] = {
        QRectF(x, y, w / 2, h / 2),
        QRectF(x + (w / 2), y, w / 2, h / 2),
        QRectF(x, y + (h / 2), w / 2, h / 2),
        QRectF(x + (w / 2), y + (h / 2), w / 2, h / 2),
    };
    // From Thetis MeterManager.cs:35341-35344 [v2.10.3.15]: a zone is on,
    // else highlighted under the mouse, else off. The slice buttons are
    // never "on": they select, they are not a state.
    const auto zoneColour = [&](int zone, bool on) {
        if (on) { return m_btnOnColour; }
        const bool highlighted = !m_pressed && m_hoverQuadrant == zone && buttonEnabled(zone);
        return highlighted ? m_btnHighlight : m_btnOffColour;
    };
    p.setPen(Qt::NoPen);
    p.fillRect(zones[0], zoneColour(0, false));
    p.fillRect(zones[1], zoneColour(1, false));
    p.fillRect(zones[2], zoneColour(2, m_accelEnabled));
    p.fillRect(zones[3], zoneColour(3, m_lockEnabled));

    //large circle
    p.setBrush(m_circleColour);
    p.drawEllipse(centre, w / 2.0, w / 2.0);

    const double ringRadius = (w * kRingDiameterRatio) / 2.0;

    //acceleration colour
    // From Thetis MeterManager.cs:35359-35378 [v2.10.3.15]
    if (m_accelEnabled) {
        if (m_pressed && std::abs(m_smoothedSpeed) >= kSpeedRingMin) {
            QColor speed;
            switch (m_tuneStepDirection) {
                case -1: speed = m_slowColour; break;
                case 1:  speed = m_fastColour; break;
                default: speed = m_holdColour; break;
            }
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(speed, std::max(1.0, w * kSpeedRingWidthRatio)));
            p.drawEllipse(centre, ringRadius, ringRadius);
        }
    }
    //dotted ring
    // From Thetis MeterManager.cs:35380-35381 [v2.10.3.15]
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(m_ringColour, std::max(1.0, w * kDottedRingWidthRatio), Qt::DotLine));
    p.drawEllipse(centre, ringRadius, ringRadius);

    //button text
    // From Thetis MeterManager.cs:35424-35433 [v2.10.3.15]:
    //   float font_size_scaled = 38f * dial.FontScale;
    //   float fontSizeEmScaled = (font_size_scaled / 16f) * (rect.Width / 52f);
    // rect is the whole meter's width, so the text does not shrink with the
    // dial's size.
    const double size = captionPixels(dialRect, pixelRect(widgetW, widgetH).width());
    QFont font;
    font.setPixelSize(WorkspacePreview::readableTextPixelSize(qMax(1, qRound(size))));
    p.setFont(font);
    const int previous = sliceStepEnabled() ? previousSlice() : -1;
    const int next = sliceStepEnabled() ? nextSlice() : -1;
    QColor dimText = m_textColour;
    dimText.setAlphaF(m_textColour.alphaF() * 0.5f);
    const auto label = [&](int zone, const QString& text, int alignment, bool enabled) {
        p.setPen(enabled ? m_textColour : dimText);
        drawObjectText(p, zones[zone], text, size, alignment);
    };
    label(0, previous >= 0 ? QStringLiteral(" < ") + sliceLetter(previous) + QStringLiteral(" ") : QStringLiteral(" < "),
          Qt::AlignLeft | Qt::AlignTop, previous >= 0);
    label(1, next >= 0 ? QStringLiteral(" ") + sliceLetter(next) + QStringLiteral(" > ") : QStringLiteral(" > "),
          Qt::AlignRight | Qt::AlignTop, next >= 0);
    if (layout.below) {
        // NereusSDR: under the dial (captionLayout), a space apart.
        p.setFont(layout.font);
        p.setPen(m_textColour);
        p.drawText(layout.accel, Qt::AlignCenter, QStringLiteral("ACCEL"));
        p.setPen(tunedSlice() >= 0 ? m_textColour : dimText);
        p.drawText(layout.lock, Qt::AlignCenter, QStringLiteral("LOCK"));
    } else {
        label(2, QStringLiteral(" ACCEL "), Qt::AlignLeft | Qt::AlignBottom, true);
        label(3, QStringLiteral(" LOCK "), Qt::AlignRight | Qt::AlignBottom, tunedSlice() >= 0);
    }

    paintMarker(p, dialRect);
}

// ---------------------------------------------------------------------------
// paintForLayer()
// Dispatch to static or dynamic paint based on pipeline layer. The Thetis
// look draws on the dynamic layer only (its zones follow the mouse).
// ---------------------------------------------------------------------------
void DialItem::paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer)
{
    const CaptionLayout layout = captionLayout(widgetW, widgetH);
    if (m_thetisLook) {
        if (layer == Layer::OverlayDynamic) { paintThetis(p, layout, widgetW, widgetH); }
        return;
    }
    if (layer == Layer::OverlayStatic) {
        paintStatic(p, layout);
    } else if (layer == Layer::OverlayDynamic) {
        paintDynamic(p, layout.dial);
    }
}

// ---------------------------------------------------------------------------
// paint()
// CPU fallback: render both layers in sequence.
// ---------------------------------------------------------------------------
void DialItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const CaptionLayout layout = captionLayout(widgetW, widgetH);
    if (m_thetisLook) {
        paintThetis(p, layout, widgetW, widgetH);
        return;
    }
    paintStatic(p, layout);
    paintDynamic(p, layout.dial);
}

// ---------------------------------------------------------------------------
// serialize()
// Format: DIAL|x|y|w|h|bindingId|zOrder|textColour|circleColour|padColour|
//         ringColour|btnOnColour|btnOffColour|btnHighlight|slowColour|holdColour|fastColour
//         |padPressedColour|verticalRatio|fontScale|upThreshold|downThreshold
//         |intervalSeconds|maxChanges|degreesPerChange|align|accelerate|sliceSelect
//         |thetisLook
//         [17-28], appended 2026-10-26; a record without them keeps the
//         constructor's values (and the saved look).
// ---------------------------------------------------------------------------
QString DialItem::serialize() const
{
    QStringList parts{
        QStringLiteral("DIAL"),
        QString::number(static_cast<double>(m_x)),
        QString::number(static_cast<double>(m_y)),
        QString::number(static_cast<double>(m_w)),
        QString::number(static_cast<double>(m_h)),
        QString::number(m_bindingId),
        QString::number(m_zOrder),
        m_textColour.name(QColor::HexArgb),
        m_circleColour.name(QColor::HexArgb),
        m_padColour.name(QColor::HexArgb),
        m_ringColour.name(QColor::HexArgb),
        m_btnOnColour.name(QColor::HexArgb),
        m_btnOffColour.name(QColor::HexArgb),
        m_btnHighlight.name(QColor::HexArgb),
        m_slowColour.name(QColor::HexArgb),
        m_holdColour.name(QColor::HexArgb),
        m_fastColour.name(QColor::HexArgb),
        m_padPressedColour.name(QColor::HexArgb),
        QString::number(static_cast<double>(m_verticalRatio)),
        QString::number(static_cast<double>(m_fontScale)),
        QString::number(m_upThreshold),
        QString::number(m_downThreshold),
        QString::number(m_tuneStepIntervalSeconds),
        QString::number(m_maxTuneStepChanges),
        QString::number(m_degreesPerChange),
        QString::number(m_alignWithTuneStep ? 1 : 0),
        QString::number(m_accelEnabled ? 1 : 0),
        QString::number(static_cast<int>(m_sliceSelect)),
        QString::number(m_thetisLook ? 1 : 0),
    };
    return parts.join(QLatin1Char('|'));
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected parts (17 or more):
// [0]=DIAL [1-6]=base fields [7-16]=colour fields [17-28]=appended settings
// ---------------------------------------------------------------------------
bool DialItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 17 || parts[0] != QLatin1String("DIAL")) {
        return false;
    }

    // Parse base fields (indices 1..6)
    const QString base = QStringList(parts.mid(1, 6)).join(QLatin1Char('|'));
    if (!MeterItem::deserialize(base)) {
        return false;
    }

    // Parse colour fields
    const QColor textColour(parts[7]);
    const QColor circleColour(parts[8]);
    const QColor padColour(parts[9]);
    const QColor ringColour(parts[10]);
    const QColor btnOnColour(parts[11]);
    const QColor btnOffColour(parts[12]);
    const QColor btnHighlight(parts[13]);
    const QColor slowColour(parts[14]);
    const QColor holdColour(parts[15]);
    const QColor fastColour(parts[16]);

    if (!textColour.isValid()   || !circleColour.isValid() ||
        !padColour.isValid()    || !ringColour.isValid()   ||
        !btnOnColour.isValid()  || !btnOffColour.isValid() ||
        !btnHighlight.isValid() || !slowColour.isValid()   ||
        !holdColour.isValid()   || !fastColour.isValid()) {
        return false;
    }

    m_textColour   = textColour;
    m_circleColour = circleColour;
    m_padColour    = padColour;
    m_ringColour   = ringColour;
    m_btnOnColour  = btnOnColour;
    m_btnOffColour = btnOffColour;
    m_btnHighlight = btnHighlight;
    m_slowColour   = slowColour;
    m_holdColour   = holdColour;
    m_fastColour   = fastColour;

    // Appended settings: each one present and readable is taken; a missing
    // or unreadable one keeps its current value.
    const auto intAt = [&parts](int index, int& out) {
        if (index >= parts.size()) { return false; }
        bool ok = false;
        const int value = parts[index].toInt(&ok);
        if (ok) { out = value; }
        return ok;
    };
    const auto floatAt = [&parts](int index, float& out) {
        if (index >= parts.size()) { return false; }
        bool ok = false;
        const float value = parts[index].toFloat(&ok);
        if (ok) { out = value; }
        return ok;
    };
    if (parts.size() > 17) {
        const QColor padPressed(parts[17]);
        if (padPressed.isValid()) { m_padPressedColour = padPressed; }
    }
    float ratio = m_verticalRatio;
    if (floatAt(18, ratio)) { setVerticalRatio(ratio); }
    float fontScale = m_fontScale;
    if (floatAt(19, fontScale)) { setFontScale(fontScale); }
    // The saved pair is taken as written (in range), then kept 90 apart by
    // lowering down, so a saved pair loads whatever the order of the setters.
    int up = m_upThreshold;
    int down = m_downThreshold;
    intAt(20, up);
    intAt(21, down);
    m_upThreshold = std::clamp(up, kThresholdMin + kThresholdGap, kThresholdMax);
    m_downThreshold = std::clamp(std::min(down, m_upThreshold - kThresholdGap), kThresholdMin, kThresholdMax);
    int value = 0;
    if (intAt(22, value)) { setTuneStepIntervalSeconds(value); }
    if (intAt(23, value)) { setMaxTuneStepChanges(value); }
    if (intAt(24, value)) { setDegreesPerChange(value); }
    if (intAt(25, value)) { m_alignWithTuneStep = value != 0; }
    if (intAt(26, value)) { m_accelEnabled = value != 0; }
    m_thetisLook = intAt(28, value) && value != 0;
    if (intAt(27, value)) {
        setSliceSelect(value == static_cast<int>(SliceSelect::AnySlice) ? SliceSelect::AnySlice
                                                                         : SliceSelect::SourceSlice);
    }

    return true;
}

} // namespace NereusSDR
