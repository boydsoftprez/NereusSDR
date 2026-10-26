// =================================================================
// src/gui/meters/MagicEyeItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Thetis magic eye options: attack and decay, history and
//                 ignore history, eye and bezel scale, scale calibration,
//                 new-item defaults. Saved records keep the fixed smoothing
//                 and look. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-25 - VHF S9 rule per slice: the eye's signal-strength scale
//                 moves 20 dB at or above 30 MHz (aboveS9Frequency).
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

#include "MagicEyeItem.h"
#include "AboveS9Frequency.h"

// From Thetis clsMagicEyeItem (MeterManager.cs:15855+)

#include <QPainter>
#include <QRadialGradient>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// setBezelImagePath()
// Store path and load image from disk or a bundled resource. Never fetched.
// ---------------------------------------------------------------------------
void MagicEyeItem::setBezelImagePath(const QString& path)
{
    m_bezelPath = path;
    m_bezelImage = QImage();
    if (!path.isEmpty()) {
        m_bezelImage.load(path);
    }
}

void MagicEyeItem::setAttackRatio(float a)
{
    m_attackRatio = std::clamp(a, 0.0f, 1.0f);
}

void MagicEyeItem::setDecayRatio(float d)
{
    m_decayRatio = std::clamp(d, 0.0f, 1.0f);
}

void MagicEyeItem::setHistoryDurationMs(int ms)
{
    m_historyDurationMs = std::clamp(ms, kMinBarHistoryDurationMs, kMaxBarHistoryDurationMs);
}

void MagicEyeItem::setIgnoreHistoryMs(int ms)
{
    m_ignoreHistoryMs = std::clamp(ms, kMinBarIgnoreHistoryMs, kMaxBarIgnoreHistoryMs);
}

void MagicEyeItem::setEyeScale(float s)
{
    m_eyeScale = std::clamp(s, kMinEyeScale, kMaxEyeScale);
}

void MagicEyeItem::setBezelScale(float s)
{
    m_bezelScale = std::clamp(s, kMinEyeScale, kMaxEyeScale);
}

// The reading interval the history is counted in: the item's update
// interval, or Thetis's 100 ms default for an item that takes every poll.
int MagicEyeItem::historyIntervalMs() const
{
    return updateIntervalMs() > 0 ? updateIntervalMs() : kDefaultUpdateIntervalMs;
}

void MagicEyeItem::clearHistory()
{
    // From Thetis MeterManager.cs:17319-17327 [v2.10.3.15] (ClearHistory)
    m_history.clear();
    m_ignoringNext = m_ignoreHistoryMs / historyIntervalMs();  // ignore next N readings to make up 2 second of ignore
}

// From Thetis MeterManager.cs:17295-17318 [v2.10.3.15] (MinHistory, MaxHistory)
float MagicEyeItem::minHistory() const
{
    if (m_history.isEmpty()) { return m_smoothedDbm; }
    return *std::min_element(m_history.cbegin(), m_history.cend());
}

float MagicEyeItem::maxHistory() const
{
    if (m_history.isEmpty()) { return m_smoothedDbm; }
    return *std::max_element(m_history.cbegin(), m_history.cend());
}

void MagicEyeItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    const MagicEyeItemThetisDefaults& d = kMagicEyeThetisDefaults;
    m_attackRatio = d.attackRatio;
    m_decayRatio  = d.decayRatio;
    m_glowColor   = QColor::fromRgb(d.colour);
    setBindingId(d.bindingId);
    m_scaleCalibration.clear();
    for (int i = 0; i < 3; ++i) {
        m_scaleCalibration.insert(d.calibrationDbm[i], d.calibrationPosition[i]);
    }
    m_showHistory       = d.showHistory;
    m_historyDurationMs = d.historyDurationMs;
    m_ignoreHistoryMs   = d.ignoreHistoryMs;
    m_historyColour     = QColor::fromRgba(d.historyColour);
    m_eyeScale          = d.eyeScale;
    m_bezelScale        = d.bezelScale;
    // me.Value = the lowest calibration key; 20 lower above the S9
    // frequency. From Thetis MeterManager.cs:23593-23594 [v2.10.3.15]
    m_smoothedDbm = m_scaleCalibration.firstKey()
        - (m_aboveS9Frequency ? static_cast<float>(kAboveS9OffsetDb) : 0.0f);
}

// ---------------------------------------------------------------------------
// setValue()
// From Thetis clsMagicEyeItem.Update (MeterManager.cs:17245-17264
// [v2.10.3.15]): attack for a rising reading, decay for a falling one,
// then the history. A saved record's attack and decay are both
// kSmoothAlpha, the exponential smoothing it always had.
// ---------------------------------------------------------------------------
void MagicEyeItem::setValue(double v)
{
    MeterItem::setValue(v);
    const bool calibrated = !m_scaleCalibration.isEmpty();
    if (calibrated && isNoReading(v)) {
        // No reading: the eye rests closed, as a new eye starts.
        m_smoothedDbm = m_scaleCalibration.firstKey()
            - (m_aboveS9Frequency ? static_cast<float>(kAboveS9OffsetDb) : 0.0f);
        m_history.clear();
        return;
    }
    const float reading = static_cast<float>(v);
    const float ratio = reading > m_smoothedDbm ? m_attackRatio : m_decayRatio;
    m_smoothedDbm += ratio * (reading - m_smoothedDbm);
    if (!calibrated) {
        m_smoothedDbm = std::clamp(m_smoothedDbm, kS0Dbm, kMaxDbm);
    }

    if (m_ignoringNext <= 0) {
        // signal history
        m_history.append(m_smoothedDbm); // adds to end of the list
        const int numberToRemove = static_cast<int>(m_history.size())
            - (m_historyDurationMs / historyIntervalMs());
        // the list is sized based on delay
        if (numberToRemove > 0) { m_history.remove(0, numberToRemove); } // remove the oldest, the head of the list
    } else {
        --m_ignoringNext;
    }
}

// ---------------------------------------------------------------------------
// dbmToFraction()
// Returns 0.0 at S0 (-127 dBm), 1.0 at S9+60 (-13 dBm).
// From Thetis clsMagicEyeItem (MeterManager.cs:15855+)
// ---------------------------------------------------------------------------
float MagicEyeItem::dbmToFraction(float dbm) const
{
    return std::clamp((dbm - kS0Dbm) / (kMaxDbm - kS0Dbm), 0.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// scalePosition()
// The VHF S9 rule, then the calibration (or the linear S0..S9+60 scale of a
// saved record).
// From Thetis MeterManager.cs:41041-41104 [v2.10.3.15] (getPerc)
// ---------------------------------------------------------------------------
float MagicEyeItem::scalePosition(float dbm) const
{
    const float value = static_cast<float>(aboveS9Adjusted(m_bindingId, dbm, m_aboveS9Frequency));
    if (m_scaleCalibration.isEmpty()) {
        return dbmToFraction(value);
    }
    //[2.10.3.9]MW0LGE refactor for speed
    const float rounded = std::round(value * 100.0f) / 100.0f;
    if (!std::isfinite(rounded) || rounded <= m_scaleCalibration.firstKey()) {
        return m_scaleCalibration.first();
    }
    if (rounded >= m_scaleCalibration.lastKey()) {
        return m_scaleCalibration.last();
    }
    auto high = m_scaleCalibration.upperBound(rounded);
    auto low = std::prev(high);
    const float t = (rounded - low.key()) / (high.key() - low.key());
    return low.value() + (high.value() - low.value()) * t;
}

// A square of `scale` times the item's shorter side, centred in the item.
// From Thetis MeterManager.cs:27807-27880 [v2.10.3.15] (eye and bezel
// placed centred, each sized by its own scale)
QRect MagicEyeItem::scaledRect(int widgetW, int widgetH, float scale) const
{
    const QRect pr = pixelRect(widgetW, widgetH);
    const int full = std::min(pr.width(), pr.height());
    const int side = static_cast<int>(std::lround(static_cast<double>(full) * scale));
    return QRect(pr.left() + (pr.width() - side) / 2,
                 pr.top()  + (pr.height() - side) / 2,
                 side, side);
}

QRect MagicEyeItem::eyeRect(int widgetW, int widgetH) const
{
    return scaledRect(widgetW, widgetH, m_eyeScale);
}

QRect MagicEyeItem::bezelRect(int widgetW, int widgetH) const
{
    return scaledRect(widgetW, widgetH, m_bezelScale);
}

// ---------------------------------------------------------------------------
// paint()
// From Thetis clsMagicEyeItem (MeterManager.cs:15855+)
// Renders a green phosphor disc with dark shadow wedge (EM84/6E5 tuning eye).
// Shadow opens wide at no signal, closes to a sliver at full signal.
// ---------------------------------------------------------------------------
void MagicEyeItem::paint(QPainter& p, int widgetW, int widgetH)
{
    // Step 1: compute square eye rect centered within item pixel rect
    // (scaled by the eye scale; 1.0, a saved record's, fills the item)
    const QRect eyeRect = this->eyeRect(widgetW, widgetH);
    const int side = eyeRect.width();

    // Step 2: enable antialiasing
    p.setRenderHint(QPainter::Antialiasing, true);

    // Step 3: dark background circle
    p.setBrush(QColor(0x0a, 0x0a, 0x0a));
    p.setPen(Qt::NoPen);
    p.drawEllipse(eyeRect);

    // Step 4: green phosphor glow via radial gradient
    QRadialGradient grad(eyeRect.center(), side / 2.0);
    grad.setColorAt(0.0, m_glowColor);
    grad.setColorAt(0.6, m_glowColor.darker(200));
    grad.setColorAt(1.0, QColor(0x00, 0x44, 0x00, 0x40));
    p.setBrush(grad);
    p.drawEllipse(eyeRect);

    // Step 5: shadow wedge — dark pie slice, symmetric about 12 o'clock
    // Shadow narrows as signal strength increases (fraction → 1.0)
    // From Thetis clsMagicEyeItem (MeterManager.cs:15855+)
    // VHF S9 rule: a signal-strength eye moves 20 dB at or above the S9
    // frequency, as the meter's other S-unit scales do.
    // From Thetis MeterManager.cs:37253, 41049-41052 [v2.10.3.15] (getPerc)
    float fraction  = scalePosition(m_smoothedDbm);
    float shadowDeg = kMaxShadowDeg - fraction * (kMaxShadowDeg - kMinShadowDeg);
    float halfShadow = shadowDeg / 2.0f;

    // QPainter angles: 0° = 3 o'clock, counter-clockwise positive.
    // Top (12 o'clock) = 90°. drawPie uses 1/16th-degree units.
    int startAngle = static_cast<int>((90.0f - halfShadow) * 16.0f);
    int spanAngle  = static_cast<int>(shadowDeg * 16.0f);

    p.setBrush(QColor(0x0a, 0x0a, 0x0a, 0xe0));
    p.setPen(Qt::NoPen);
    p.drawPie(eyeRect, startAngle, spanAngle);

    // Step 6: center dot (anode)
    int dotR = side / 12;
    p.setBrush(QColor(0x20, 0x20, 0x20));
    p.drawEllipse(eyeRect.center(), dotR, dotR);

    // Step 7: optional bezel overlay image (sized by the bezel scale)
    if (!m_bezelImage.isNull()) {
        p.drawImage(bezelRect(widgetW, widgetH), m_bezelImage);
    }

    // Step 8: disable antialiasing
    p.setRenderHint(QPainter::Antialiasing, false);
}

// ---------------------------------------------------------------------------
// serialize()
// Format: MAGICEYE|x|y|w|h|bindingId|zOrder|glowColor|bezelPath
//         |attack|decay|showHistory|historyMs|historyColour|ignoreHistoryMs
//         |eyeScale|bezelScale|calibration
// calibration: "dbm:position" pairs joined by ';' (empty = linear scale).
// ---------------------------------------------------------------------------
QString MagicEyeItem::serialize() const
{
    QStringList cal;
    for (auto it = m_scaleCalibration.constBegin(); it != m_scaleCalibration.constEnd(); ++it) {
        cal << QStringLiteral("%1:%2").arg(static_cast<double>(it.key()))
                                      .arg(static_cast<double>(it.value()));
    }
    return QStringLiteral("MAGICEYE|%1|%2|%3|%4|%5|%6|%7|%8")
        .arg(static_cast<double>(m_x))
        .arg(static_cast<double>(m_y))
        .arg(static_cast<double>(m_w))
        .arg(static_cast<double>(m_h))
        .arg(m_bindingId)
        .arg(m_zOrder)
        .arg(m_glowColor.name(QColor::HexArgb))
        .arg(m_bezelPath)
        + QStringLiteral("|%1|%2|%3|%4|%5|%6|%7|%8|%9")
        .arg(static_cast<double>(m_attackRatio))
        .arg(static_cast<double>(m_decayRatio))
        .arg(m_showHistory ? 1 : 0)
        .arg(m_historyDurationMs)
        .arg(m_historyColour.name(QColor::HexArgb))
        .arg(m_ignoreHistoryMs)
        .arg(static_cast<double>(m_eyeScale))
        .arg(static_cast<double>(m_bezelScale))
        .arg(cal.join(QLatin1Char(';')));
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected parts:
// [0]=MAGICEYE [1-6]=base fields [7]=glowColor [8]=bezelPath
// [9..17] (optional; a record without them keeps the fixed smoothing and
// the linear scale): attack, decay, showHistory, historyMs, historyColour,
// ignoreHistoryMs, eyeScale, bezelScale, calibration
// ---------------------------------------------------------------------------
bool MagicEyeItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 9 || parts[0] != QLatin1String("MAGICEYE")) {
        return false;
    }

    // Parse base fields (indices 1..6)
    const QString base = QStringList(parts.mid(1, 6)).join(QLatin1Char('|'));
    if (!MeterItem::deserialize(base)) {
        return false;
    }

    const QColor glowColor(parts[7]);
    if (!glowColor.isValid()) {
        return false;
    }

    m_glowColor = glowColor;
    setBezelImagePath(parts[8]);  // empty string is valid (no bezel)

    // Appended fields, each read only when present and well formed.
    const auto number = [&parts](int i, float& out) {
        if (i >= parts.size()) { return false; }
        bool ok = false;
        const float v = parts[i].toFloat(&ok);
        if (ok && std::isfinite(v)) { out = v; }
        return ok && std::isfinite(v);
    };
    float v = 0.0f;
    if (number(9, v))  { setAttackRatio(v); }
    if (number(10, v)) { setDecayRatio(v); }
    if (parts.size() > 11 && (parts[11] == QLatin1String("0") || parts[11] == QLatin1String("1"))) {
        m_showHistory = parts[11] == QLatin1String("1");
    }
    if (number(12, v)) { setHistoryDurationMs(static_cast<int>(v)); }
    if (parts.size() > 13) {
        const QColor history(parts[13]);
        if (history.isValid()) { m_historyColour = history; }
    }
    if (number(14, v)) { setIgnoreHistoryMs(static_cast<int>(v)); }
    if (number(15, v)) { setEyeScale(v); }
    if (number(16, v)) { setBezelScale(v); }
    if (parts.size() > 17) {
        QMap<float, float> cal;
        const QStringList pairs = parts[17].split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QString& pair : pairs) {
            const QStringList kv = pair.split(QLatin1Char(':'));
            bool okK = false;
            bool okV = false;
            const float key = kv.value(0).toFloat(&okK);
            const float pos = kv.value(1).toFloat(&okV);
            if (kv.size() == 2 && okK && okV && std::isfinite(key) && std::isfinite(pos)) {
                cal.insert(key, pos);
            }
        }
        m_scaleCalibration = cal;
        if (!m_scaleCalibration.isEmpty()) {
            m_smoothedDbm = m_scaleCalibration.firstKey();
        }
    }

    return true;
}

} // namespace NereusSDR
