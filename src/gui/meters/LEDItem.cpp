// =================================================================
// src/gui/meters/LEDItem.cpp  (NereusSDR)
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
//   2026-10-26 - Placed look ported from Thetis renderLed
//                 (MeterManager.cs:34010-34130 [v2.10.3.15]): offset and
//                 size inside a panel width x padding tall, the panel RX/TX
//                 colour with its flip fade (ContentFade::nextValue), and
//                 the false-to-true colour fade with blink and pulsate.
//                 Show true / show false on both looks; process when
//                 hidden; isTrue(); an update interval under 50 ms becomes
//                 100 ms (:27446); kLedThetisDefaults for new items. The
//                 options are appended to the record; absent, a saved LED
//                 keeps the fill look. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - Condition: evaluated with evaluateLedCondition (not C#)
//                 per reading and per update interval; %name% values from
//                 the hosting meter's slice readings, published clock
//                 values and meter data sources; saved base64 after the
//                 options. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
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

#include "LEDItem.h"
#include "ClockItem.h"
#include "MeterWidget.h"
#include "ThetisReadingNames.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/mmio/MmioEndpoint.h"
#include "gui/containers/ContentFade.h"

// From Thetis clsLed (MeterManager.cs:19448+)

#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QPainterPath>
#include <QDateTime>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

// From Thetis MeterManager.cs:19448 — clsLed constructor
LEDItem::LEDItem(QObject* parent)
    : MeterItem(parent)
{
}

void LEDItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    const LedItemThetisDefaults& d = kLedThetisDefaults;
    m_placed            = d.placed;
    m_trueColour        = QColor::fromRgb(d.trueColour);
    m_falseColour       = QColor::fromRgb(d.falseColour);
    m_panelBack1        = QColor::fromRgb(d.panelBack1);
    m_panelBack2        = QColor::fromRgb(d.panelBack2);
    m_showBackPanel     = d.showBackPanel;
    m_offsetX           = d.offsetX;
    m_offsetY           = d.offsetY;
    m_sizeX             = d.sizeX;
    m_sizeY             = d.sizeY;
    m_padding           = d.padding;
    m_shape             = d.shape;
    m_style             = d.style;
    m_blink             = d.blink;
    m_pulsate           = d.pulsate;
    m_showTrue          = d.showTrue;
    m_showFalse         = d.showFalse;
    m_processWhenHidden = d.processWhenHidden;
}

// From Thetis MeterManager.cs:27462-27476 [v2.10.3.15]
void LEDItem::setLightMode(LightMode mode)
{
    switch (mode) {
    case LightMode::OnOff:
        m_blink = false;
        m_pulsate = false;
        break;
    case LightMode::Blink:
        m_blink = true;
        m_pulsate = false;
        break;
    case LightMode::Pulsate:
        m_blink = false;
        m_pulsate = true;
        break;
    }
}

LEDItem::LightMode LEDItem::lightMode() const
{
    if (m_blink) {
        return LightMode::Blink;
    }
    return m_pulsate ? LightMode::Pulsate : LightMode::OnOff;
}

void LEDItem::setUpdateIntervalMs(int ms)
{
    // From Thetis MeterManager.cs:27446 [v2.10.3.15]. 0 is NereusSDR's
    // "every poll" for a saved item without an interval, and stays.
    if (ms > 0 && ms < kLedMinUpdateIntervalMs) {
        ms = kLedRaisedUpdateIntervalMs; // when it hasnt been set
    }
    MeterItem::setUpdateIntervalMs(ms);
}

// ---------------------------------------------------------------------------
// setValue()
// From Thetis clsLed — threshold-based LED activation logic.
// If v >= redThreshold  → red mode (always on)
// If v >= amberThreshold → amber mode (always on)
// If v >= greenThreshold → m_ledOn = true (green/true colour)
// Else                   → m_ledOn = false (false/off colour)
// ---------------------------------------------------------------------------
void LEDItem::setValue(double v)
{
    MeterItem::setValue(v);

    // A condition decides alone; the reading may be one of its names.
    if (hasCondition()) {
        evaluateCondition();
        return;
    }

    if (v >= m_redThreshold) {
        m_ledOn = true;   // color determined by currentColor()
    } else if (v >= m_amberThreshold) {
        m_ledOn = true;
    } else if (v >= m_greenThreshold) {
        m_ledOn = true;
    } else {
        m_ledOn = false;
    }
    // The fill look's blink toggles m_ledOn while painting; the true state
    // is kept apart.
    m_isTrue = m_ledOn;
}

// ---------------------------------------------------------------------------
// Condition
// Thetis compiles the condition as C# (clsMeterScriptEngine.cs:325-381
// [v2.10.3.15]) after expand_placeholders turns each %name% into a lookup
// (MeterManager.cs:21007-21066 [v2.10.3.15]); an invalid script leaves the
// LED false (Update, MeterManager.cs:21179 [v2.10.3.15]). NereusSDR
// evaluates its own expression language with the same names.
// ---------------------------------------------------------------------------
namespace {

// Clock values. From Thetis MeterManager.cs:31096-31099 [v2.10.3.15]
// (published by ClockItem::publishedReadings).
bool isClockName(const QString& name)
{
    static const QStringList kClockNames{QStringLiteral("time_utc"), QStringLiteral("time_loc"),
                                         QStringLiteral("date_utc"), QStringLiteral("date_loc")};
    return kClockNames.contains(name, Qt::CaseInsensitive);
}

} // namespace

bool LEDItem::hasCondition() const
{
    return !m_condition.trimmed().isEmpty();
}

void LEDItem::setCondition(const QString& expression)
{
    const bool had = hasCondition();
    m_condition = expression;
    m_lastConditionMs = -1;
    if (hasCondition()) {
        evaluateCondition();
        return;
    }
    m_conditionError.clear();
    // Back to the thresholds, from the latest reading.
    if (had) {
        setValue(value());
    }
}

MeterWidget* LEDItem::hostMeter() const
{
    for (QObject* p = parent(); p; p = p->parent()) {
        if (auto* meter = qobject_cast<MeterWidget*>(p)) {
            return meter;
        }
    }
    return nullptr;
}

std::optional<QVariant> LEDItem::conditionValue(const QString& name) const
{
    const QString key = name.trimmed();
    MeterWidget* meter = hostMeter();

    // Values other items publish (the clock's time_utc and the like), named
    // in lowercase as Thetis custom strings are (MeterManager.cs:21035-21039
    // [v2.10.3.15]).
    if (meter) {
        const QString lower = key.toLower();
        for (MeterItem* item : meter->items()) {
            if (!item || item == this) {
                continue;
            }
            const QHash<QString, QVariant> published = item->publishedReadings();
            const auto it = published.constFind(lower);
            if (it != published.cend()) {
                return it.value();
            }
        }
    }
    // A clock value with no clock in the meter: the 24 hour time and the date.
    if (isClockName(key)) {
        const bool utc = key.startsWith(QLatin1String("time_utc"), Qt::CaseInsensitive)
            || key.startsWith(QLatin1String("date_utc"), Qt::CaseInsensitive);
        const QDateTime now = utc ? QDateTime::currentDateTimeUtc() : QDateTime::currentDateTime();
        const bool isTime = key.startsWith(QLatin1String("time"), Qt::CaseInsensitive);
        return QVariant(isTime ? ClockItem::thetisTime(now, true) : ClockItem::thetisDate(now));
    }

    // A Thetis reading name, from the entry's slice readings.
    const int binding = bindingForThetisReadingName(key);
    if (binding >= 0) {
        if (meter) {
            const QHash<int, double> readings = meter->lastBindingValues();
            const auto it = readings.constFind(binding);
            if (it != readings.cend()) {
                return QVariant(it.value());
            }
        }
        return QVariant();
    }

    // A meter data source value: <4 character ID>.<value name>.
    const qsizetype dot = key.indexOf(QLatin1Char('.'));
    if (dot > 0 && dot + 1 < key.size()) {
        if (MmioEndpoint* ep = ExternalVariableEngine::instance().endpointByFourChar(key.left(dot))) {
            return ep->valueForName(key.mid(dot + 1));
        }
    }
    return std::nullopt;
}

LedConditionResult LEDItem::checkCondition(const QString& expression) const
{
    return evaluateLedCondition(expression, [this](const QString& name) -> std::optional<QVariant> {
        std::optional<QVariant> v = conditionValue(name);
        if (v && !v->isValid()) {
            return isClockName(name.trimmed()) ? QVariant(QString()) : QVariant(0.0);
        }
        return v;
    });
}

bool LEDItem::evaluateCondition()
{
    const bool before = m_isTrue;
    const QString errorBefore = m_conditionError;
    const LedConditionResult result = evaluateLedCondition(
        m_condition, [this](const QString& name) { return conditionValue(name); });
    m_isTrue = result.ok && result.value;
    m_conditionError = result.ok ? QString() : result.error;
    // The fill look's blink toggles m_ledOn while painting.
    m_ledOn = m_isTrue;
    return m_isTrue != before || m_conditionError != errorBefore;
}

bool LEDItem::stepCondition(qint64 monotonicMs)
{
    const int interval = updateIntervalMs() > 0 ? updateIntervalMs() : kDefaultUpdateIntervalMs;
    if (m_lastConditionMs >= 0 && monotonicMs >= m_lastConditionMs
        && monotonicMs - m_lastConditionMs < interval) {
        return false;
    }
    m_lastConditionMs = monotonicMs;
    return evaluateCondition();
}

void LEDItem::setTransmitting(bool tx)
{
    MeterItem::setTransmitting(tx);
    if (m_placed) {
        checkPanelFlip();
    }
}

// From Thetis MeterManager.cs:34019 [v2.10.3.15]
bool LEDItem::coverFadeActive() const
{
    const bool tx = effectiveTransmitting();
    return (fadeOnRx() && !tx) || (fadeOnTx() && tx);
}

// From Thetis MeterManager.cs:34020-34026 [v2.10.3.15]: on a MOX flip the
// panel fade restarts at 48.
bool LEDItem::checkPanelFlip()
{
    if (coverFadeActive() || m_panelBack1 == m_panelBack2) {
        return false;
    }
    const bool tx = effectiveTransmitting();
    if (tx == m_moxFadeToggle) {
        return false;
    }
    m_panelFadeValue = ContentFade::kDimmedLevel;
    m_moxFadeToggle = tx;
    return true;
}

// ---------------------------------------------------------------------------
// stepPlaced()
// One frame of Thetis renderLed (MeterManager.cs:34010-34122 [v2.10.3.15]).
// Thetis runs it once per frame at the container's quickest update
// interval; NereusSDR runs it once per the LED's own update interval.
// ---------------------------------------------------------------------------
bool LEDItem::stepPlaced(int updateIntervalMs)
{
    const float fadeBefore = m_colourFade;
    const int panelBefore = m_panelFadeValue;

    // Panel: from Thetis MeterManager.cs:34019-34038 [v2.10.3.15]
    if (!coverFadeActive() && m_panelBack1 != m_panelBack2 && !checkPanelFlip()) {
        // fade to take half a second (ContentFade::nextValue with no fade
        // flag set takes the 255-48 = 207 step of :34029-34036)
        m_panelFadeValue = ContentFade::nextValue(m_panelFadeValue, false, false,
                                                  effectiveTransmitting(), updateIntervalMs);
    }

    // Light: from Thetis MeterManager.cs:34050-34122 [v2.10.3.15]
    const bool result = m_isTrue;
    int intervalSpeed = kLedFadeSpeedMs;

    if (result != m_oldIsTrue) {
        if (result && m_blink) {
            m_blinkCount = 0;
            m_pulsateUp = true;
            intervalSpeed = kLedBlinkSpeedMs;
            m_colourFade = 0.0f;
        }
    }
    // Thetis sets OldConditionResult once per reading (Update, :21187);
    // NereusSDR takes the edge once per frame.
    m_oldIsTrue = result;

    if (m_blink || m_pulsate) {
        if (m_blink) {
            if (m_colourFade == 1.0f) {
                if (m_blinkCount < 2) {
                    m_pulsateUp = false;
                    ++m_blinkCount;
                    intervalSpeed = kLedBlinkSpeedMs;
                } else {
                    m_colourFade = 0.0f;
                    m_pulsateUp = false;
                }
            }
            if (m_colourFade == 0.0f && result && m_blinkCount < 2) {
                m_pulsateUp = true;
            }
        } else {
            if (m_colourFade == 1.0f) {
                m_pulsateUp = false;
            }
            if (m_colourFade == 0.0f && result) {
                m_pulsateUp = true;
            }
        }
    } else {
        m_pulsateUp = result;
    }

    int updateInterval = updateIntervalMs;// m.QuickestUpdateInterval(m.MOX, false);
    updateInterval = std::min(updateInterval, intervalSpeed);
    const int stepsNeeded = static_cast<int>(std::ceil(intervalSpeed / static_cast<float>(updateInterval)));
    const float stepSize = 1.0f / static_cast<float>(stepsNeeded);

    // Thetis ColorFade clamps to 0..1 in its setter (:21122-21130).
    if (m_pulsateUp) {
        m_colourFade = std::min(m_colourFade + stepSize, 1.0f);
    } else {
        m_colourFade = std::max(m_colourFade - stepSize, 0.0f);
    }

    return m_colourFade != fadeBefore || m_panelFadeValue != panelBefore;
}

bool LEDItem::advanceMeter(qint64 monotonicMs)
{
    bool changed = hasCondition() && stepCondition(monotonicMs);
    if (!m_placed) {
        return changed;
    }
    changed = checkPanelFlip() || changed;
    const int interval = updateIntervalMs() > 0 ? updateIntervalMs() : kDefaultUpdateIntervalMs;
    // The first call, or a clock that moved backwards, only takes the time.
    if (m_lastStepMs < 0 || monotonicMs < m_lastStepMs) {
        m_lastStepMs = monotonicMs;
        return changed;
    }
    // A long gap (a hidden container) catches up a few frames at most.
    constexpr int kMaxCatchUpFrames = 16;
    int frames = 0;
    while (monotonicMs - m_lastStepMs >= interval) {
        changed = stepPlaced(interval) || changed;
        m_lastStepMs += interval;
        if (++frames >= kMaxCatchUpFrames) {
            m_lastStepMs = monotonicMs;
            break;
        }
    }
    return changed;
}

QColor LEDItem::trueEndColour() const
{
    if (hasCondition()) {
        return m_trueColour;
    }
    const double v = value();
    if (v >= m_redThreshold) {
        return m_redColour;
    }
    if (v >= m_amberThreshold) {
        return m_amberColour;
    }
    return m_trueColour;
}

// From Thetis MeterManager.cs:34112-34121 [v2.10.3.15]
// (ColorInterpolator.InterpolateBetween(FalseColour, TrueColour, ColorFade):
// each channel from + (to - from) x fade, cut to a byte) drawn with
// getDXBrushForColour(c, 255).
QColor LEDItem::placedLightColour() const
{
    const QColor a = m_falseColour;
    const QColor b = trueEndColour();
    const double lambda = static_cast<double>(m_colourFade);
    auto component = [lambda](int from, int to) {
        return static_cast<int>(static_cast<unsigned char>(from + (to - from) * lambda));
    };
    return QColor(component(a.red(), b.red()), component(a.green(), b.green()),
                  component(a.blue(), b.blue()), 255);
}

// From Thetis MeterManager.cs:34045 [v2.10.3.15]:
//   getDXBrushForColour(m.MOX ? led.PanelBackColour2 : led.PanelBackColour1, mi.FadeValue)
// getDXBrushForColour replaces the alpha with the fade value, except for a
// transparent colour. While the fade on RX / TX veil is active,
// MeterWidget's veil dims the item, so the colour shows at full alpha.
QColor LEDItem::placedPanelColour() const
{
    QColor c = effectiveTransmitting() ? m_panelBack2 : m_panelBack1;
    if (c.alpha() == 0) {
        return c;
    }
    const bool flipFade = !coverFadeActive() && m_panelBack1 != m_panelBack2;
    c.setAlpha(flipFade ? m_panelFadeValue : ContentFade::kUndimmedLevel);
    return c;
}

// From Thetis MeterManager.cs:27481 [v2.10.3.15]:
//   led.Size = new SizeF(ig.Size.Width, igs.SpacerPadding);
// The panel is as tall as the width times the padding. Thetis grows the
// group to fit; a NereusSDR item keeps its rectangle, so the panel stops at
// the item's bottom edge.
QRect LEDItem::placedPanelRect(int widgetW, int widgetH) const
{
    QRect rect = pixelRect(widgetW, widgetH);
    const int height = qRound(static_cast<double>(rect.width()) * static_cast<double>(m_padding));
    rect.setHeight(std::clamp(height, 0, rect.height()));
    return rect;
}

// From Thetis MeterManager.cs:34125-34129 [v2.10.3.15]: size and offset
// are ratios of the width (targetWidth; XRatio and YRatio are 1), and the
// light is centred on the offset.
QRectF LEDItem::placedLightRect(int widgetW, int widgetH) const
{
    const QRect rect = pixelRect(widgetW, widgetH);
    const double width = rect.width();
    const double xSize = width * static_cast<double>(m_sizeX);
    const double ySize = width * static_cast<double>(m_sizeY);
    const double posX = rect.x() + static_cast<double>(m_offsetX) * width;
    const double posY = rect.y() + static_cast<double>(m_offsetY) * width;
    return QRectF(posX - (xSize / 2.0), posY - (ySize / 2.0), xSize, ySize).normalized();
}

// ---------------------------------------------------------------------------
// currentColor()
// Returns the color appropriate for the current value and state.
// ---------------------------------------------------------------------------
QColor LEDItem::currentColor() const
{
    if (hasCondition()) {
        return m_ledOn ? m_trueColour : m_falseColour;
    }
    const double v = value();
    if (v >= m_redThreshold) {
        return m_redColour;
    }
    if (v >= m_amberThreshold) {
        return m_amberColour;
    }
    if (m_ledOn) {
        return m_trueColour;
    }
    return m_falseColour;
}

// ---------------------------------------------------------------------------
// paint()
// From Thetis clsLed renderLed() (MeterManager.cs:19448+)
// Renders the LED shape with optional back panel, blink, and pulsate.
// ---------------------------------------------------------------------------
void LEDItem::paint(QPainter& p, int widgetW, int widgetH)
{
    if (m_placed) {
        paintPlaced(p, widgetW, widgetH);
    } else {
        paintFill(p, widgetW, widgetH);
    }
}

// ---------------------------------------------------------------------------
// paintPlaced()
// From Thetis renderLed (MeterManager.cs:34040-34130 [v2.10.3.15]): the
// panel, then the light when shown for its state. Thetis fills a rectangle;
// a square flat light does the same, other shapes and styles draw as the
// fill look does.
// ---------------------------------------------------------------------------
void LEDItem::paintPlaced(QPainter& p, int widgetW, int widgetH)
{
    if (m_showBackPanel) {
        p.fillRect(placedPanelRect(widgetW, widgetH), placedPanelColour());
    }
    // From Thetis MeterManager.cs:34123 [v2.10.3.15]
    if (!((m_showTrue && m_isTrue) || (m_showFalse && !m_isTrue))) {
        return;
    }
    const QRectF lightRect = placedLightRect(widgetW, widgetH);
    if (lightRect.isEmpty()) {
        return;
    }
    const QColor color = placedLightColour();
    if (m_shape == LedShape::Square && m_style == LedStyle::Flat) {
        p.fillRect(lightRect, color);
        return;
    }
    drawShape(p, lightRect.toRect(), color);
}

// ---------------------------------------------------------------------------
// paintFill()
// The fill look: a saved record without the placed option.
// ---------------------------------------------------------------------------
void LEDItem::paintFill(QPainter& p, int widgetW, int widgetH)
{
    const QRect fullRect = pixelRect(widgetW, widgetH);

    // Step 1: optional back panel fill
    if (m_showBackPanel) {
        p.fillRect(fullRect, m_panelBack1);
    }

    // Step 2: compute led rect with padding inset
    const int pad = static_cast<int>(m_padding);
    const QRect ledRect = fullRect.adjusted(pad, pad, -pad, -pad);
    if (ledRect.isEmpty()) {
        return;
    }

    // Step 3: blink — toggle m_ledOn every 5 paint calls (~500ms at 100ms rate)
    // From Thetis clsLed blink logic (MeterManager.cs:19448+)
    if (m_blink) {
        ++m_blinkCounter;
        if (m_blinkCounter >= 5) {
            m_blinkCounter = 0;
            m_ledOn = !m_ledOn;
        }
    }

    // Step 4: pulsate — cycle alpha between 0.3 and 1.0
    // From Thetis clsLed pulsate logic (MeterManager.cs:19448+)
    if (m_pulsate) {
        m_pulsateAlpha += m_pulsateDir;
        if (m_pulsateAlpha <= 0.3f) {
            m_pulsateAlpha = 0.3f;
            m_pulsateDir = 0.05f;
        } else if (m_pulsateAlpha >= 1.0f) {
            m_pulsateAlpha = 1.0f;
            m_pulsateDir = -0.05f;
        }
    }

    // Step 5: show true / show false (Thetis MeterManager.cs:34123
    // [v2.10.3.15]); both on in a record saved before the options.
    if (!((m_showTrue && m_isTrue) || (m_showFalse && !m_isTrue))) {
        return;
    }

    // Step 6: determine current color, apply pulsate alpha
    QColor color = currentColor();
    if (m_pulsate) {
        color.setAlphaF(m_pulsateAlpha);
    }

    drawShape(p, ledRect, color);
}

// ---------------------------------------------------------------------------
// drawShape()
// The fill look's shapes and styles.
// ---------------------------------------------------------------------------
void LEDItem::drawShape(QPainter& p, const QRect& ledRect, const QColor& color)
{
    p.setRenderHint(QPainter::Antialiasing, true);

    if (m_shape == LedShape::Round) {
        if (m_style == LedStyle::Flat) {
            // From Thetis clsLed — Round/Flat: solid fill + darker border
            p.setBrush(color);
            p.setPen(QPen(color.darker(150), 1));
            p.drawEllipse(ledRect);
        } else {
            // From Thetis clsLed — Round/ThreeD: radial gradient, lighter center
            QRadialGradient grad(ledRect.center(), ledRect.width() / 2.0);
            grad.setFocalPoint(ledRect.center());
            grad.setColorAt(0.0, color.lighter(150));
            grad.setColorAt(1.0, color);
            p.setBrush(grad);
            p.setPen(QPen(color.darker(150), 1));
            p.drawEllipse(ledRect);
        }
    } else if (m_shape == LedShape::Square) {
        if (m_style == LedStyle::Flat) {
            // From Thetis clsLed — Square/Flat: solid fill + darker border
            p.fillRect(ledRect, color);
            p.setPen(QPen(color.darker(150), 1));
            p.setBrush(Qt::NoBrush);
            p.drawRect(ledRect);
        } else {
            // From Thetis clsLed — Square/ThreeD: linear gradient top-to-bottom
            QLinearGradient grad(ledRect.topLeft(), ledRect.bottomLeft());
            grad.setColorAt(0.0, color.lighter(150));
            grad.setColorAt(1.0, color);
            p.setBrush(grad);
            p.setPen(QPen(color.darker(150), 1));
            p.drawRect(ledRect);
        }
    } else {
        // From Thetis clsLed — Triangle: top-center, bottom-left, bottom-right
        QPainterPath path;
        const QPointF top(ledRect.center().x(), ledRect.top());
        const QPointF botLeft(ledRect.left(), ledRect.bottom());
        const QPointF botRight(ledRect.right(), ledRect.bottom());
        path.moveTo(top);
        path.lineTo(botLeft);
        path.lineTo(botRight);
        path.closeSubpath();
        p.setBrush(color);
        p.setPen(QPen(color.darker(150), 1));
        p.drawPath(path);
    }
}

// ---------------------------------------------------------------------------
// serialize()
// Format: LED|x|y|w|h|bindingId|zOrder|shape|style|trueColour|falseColour|
//             panelBack1|panelBack2|showBackPanel|blink|pulsate|padding|
//             greenThresh|amberThresh|redThresh
//             |placed|offsetX|offsetY|sizeX|sizeY|showTrue|showFalse|
//             processWhenHidden   (appended at [20]; absent = fill look, Thetis
//             offset and size, shown true and false, not when hidden)
//             |condition          (base64 UTF-8, after the options; written
//             only when set; absent or empty = the thresholds decide)
// ---------------------------------------------------------------------------

static QString ledBaseFields(const MeterItem& item)
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(static_cast<double>(item.x()))
        .arg(static_cast<double>(item.y()))
        .arg(static_cast<double>(item.itemWidth()))
        .arg(static_cast<double>(item.itemHeight()))
        .arg(item.bindingId())
        .arg(item.zOrder());
}

static bool ledParseBaseFields(MeterItem& item, const QStringList& parts)
{
    if (parts.size() < 7) {
        return false;
    }
    const QString base = QStringList(parts.mid(1, 6)).join(QLatin1Char('|'));
    return item.MeterItem::deserialize(base);
}

QString LEDItem::serialize() const
{
    const int shape = static_cast<int>(m_shape);   // 0=Square, 1=Round, 2=Triangle
    const int style = static_cast<int>(m_style);   // 0=Flat, 1=ThreeD

    // 14 placeholders for the 14 values (the first carries the six base
    // fields). Records written before 2026-10-26 had %15|%16|%17 left
    // unfilled at [20]-[22]; deserialize() skips them.
    return QStringLiteral("LED|%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11|%12|%13|%14")
        .arg(ledBaseFields(*this))
        .arg(shape)
        .arg(style)
        .arg(m_trueColour.name(QColor::HexArgb))
        .arg(m_falseColour.name(QColor::HexArgb))
        .arg(m_panelBack1.name(QColor::HexArgb))
        .arg(m_panelBack2.name(QColor::HexArgb))
        .arg(m_showBackPanel ? 1 : 0)
        .arg(m_blink ? 1 : 0)
        .arg(m_pulsate ? 1 : 0)
        .arg(static_cast<double>(m_padding))
        .arg(m_greenThreshold)
        .arg(m_amberThreshold)
        .arg(m_redThreshold)
        + QStringLiteral("|%1|%2|%3|%4|%5|%6|%7|%8")
            .arg(m_placed ? 1 : 0)
            .arg(static_cast<double>(m_offsetX))
            .arg(static_cast<double>(m_offsetY))
            .arg(static_cast<double>(m_sizeX))
            .arg(static_cast<double>(m_sizeY))
            .arg(m_showTrue ? 1 : 0)
            .arg(m_showFalse ? 1 : 0)
            .arg(m_processWhenHidden ? 1 : 0)
        // The condition is user text: base64, joined rather than passed
        // through .arg(), so neither '|' nor '%' can change the record.
        // Written only when set, so an LED without one saves as before.
        + (hasCondition()
               ? QLatin1Char('|') + QString::fromLatin1(m_condition.toUtf8().toBase64())
               : QString());
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected parts:
// [0]=LED [1-6]=base fields [7]=shape [8]=style [9]=trueColour [10]=falseColour
// [11]=panelBack1 [12]=panelBack2 [13]=showBackPanel [14]=blink [15]=pulsate
// [16]=padding [17]=greenThresh [18]=amberThresh [19]=redThresh
// Records written before 2026-10-26 carry "%15|%16|%17" at [20]-[22]
// (unfilled placeholders), and are read as having no options.
// Appended, each optional: [20]=placed [21]=offsetX [22]=offsetY
// [23]=sizeX [24]=sizeY [25]=showTrue [26]=showFalse [27]=processWhenHidden
// [28]=condition (base64; at opt + 8 when the old placeholders shift opt)
// ---------------------------------------------------------------------------
bool LEDItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 20 || parts[0] != QLatin1String("LED")) {
        return false;
    }
    if (!ledParseBaseFields(*this, parts)) {
        return false;
    }

    bool ok = true;

    const int shape = parts[7].toInt(&ok);
    if (!ok || shape < 0 || shape > 2) {
        return false;
    }

    const int style = parts[8].toInt(&ok);
    if (!ok || style < 0 || style > 1) {
        return false;
    }

    const QColor trueColour(parts[9]);
    if (!trueColour.isValid()) { return false; }
    const QColor falseColour(parts[10]);
    if (!falseColour.isValid()) { return false; }
    const QColor panelBack1(parts[11]);
    if (!panelBack1.isValid()) { return false; }
    const QColor panelBack2(parts[12]);
    if (!panelBack2.isValid()) { return false; }

    const int showBackPanel = parts[13].toInt(&ok);
    if (!ok) { return false; }
    const int blink = parts[14].toInt(&ok);
    if (!ok) { return false; }
    const int pulsate = parts[15].toInt(&ok);
    if (!ok) { return false; }

    const float padding = parts[16].toFloat(&ok);
    if (!ok) { return false; }

    const double greenThresh = parts[17].toDouble(&ok);
    if (!ok) { return false; }
    const double amberThresh = parts[18].toDouble(&ok);
    if (!ok) { return false; }
    const double redThresh = parts[19].toDouble(&ok);
    if (!ok) { return false; }

    // Appended options; absent fields keep the values a record saved
    // before them restores to. A record written while the format string
    // had three unfilled placeholders carries "%15|%16|%17" at [20]-[22];
    // the options then follow at [23].
    const LedItemThetisDefaults& d = kLedThetisDefaults;
    int opt = 20;
    if (parts.size() > opt && parts[opt].startsWith(QLatin1Char('%'))) {
        opt += 3;
    }
    auto optFlag = [&parts](int index, bool absent, bool* out) {
        if (parts.size() <= index) { *out = absent; return true; }
        bool flagOk = false;
        const int v = parts[index].toInt(&flagOk);
        *out = (v != 0);
        return flagOk;
    };
    auto optFloat = [&parts](int index, float absent, float* out) {
        if (parts.size() <= index) { *out = absent; return true; }
        bool floatOk = false;
        *out = parts[index].toFloat(&floatOk);
        return floatOk;
    };
    bool placed = false;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float sizeX = 0.0f;
    float sizeY = 0.0f;
    bool showTrue = true;
    bool showFalse = true;
    bool processWhenHidden = false;
    if (!optFlag(opt, false, &placed)
        || !optFloat(opt + 1, d.offsetX, &offsetX) || !optFloat(opt + 2, d.offsetY, &offsetY)
        || !optFloat(opt + 3, d.sizeX, &sizeX) || !optFloat(opt + 4, d.sizeY, &sizeY)
        || !optFlag(opt + 5, true, &showTrue) || !optFlag(opt + 6, true, &showFalse)
        || !optFlag(opt + 7, false, &processWhenHidden)) {
        return false;
    }
    QString condition;
    if (parts.size() > opt + 8) {
        const auto decoded = QByteArray::fromBase64Encoding(parts[opt + 8].toLatin1(),
                                                            QByteArray::AbortOnBase64DecodingErrors);
        if (!decoded) {
            return false;
        }
        condition = QString::fromUtf8(*decoded);
    }

    m_shape         = static_cast<LedShape>(shape);
    m_style         = static_cast<LedStyle>(style);
    m_trueColour    = trueColour;
    m_falseColour   = falseColour;
    m_panelBack1    = panelBack1;
    m_panelBack2    = panelBack2;
    m_showBackPanel = (showBackPanel != 0);
    m_blink         = (blink != 0);
    m_pulsate       = (pulsate != 0);
    m_padding       = padding;
    m_greenThreshold = greenThresh;
    m_amberThreshold = amberThresh;
    m_redThreshold   = redThresh;
    m_placed            = placed;
    m_offsetX           = offsetX;
    m_offsetY           = offsetY;
    m_sizeX             = sizeX;
    m_sizeY             = sizeY;
    m_showTrue          = showTrue;
    m_showFalse         = showFalse;
    m_processWhenHidden = processWhenHidden;
    m_condition         = condition;
    m_conditionError.clear();
    m_lastConditionMs   = -1;
    if (hasCondition()) {
        evaluateCondition();
    }

    return true;
}

} // namespace NereusSDR
