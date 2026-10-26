// =================================================================
// src/gui/meters/VfoDisplayItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-03 — Fit VFO text to its actual row width by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-24 : paint draws the unavailable text in place of the
//                 frequency and labels (R-R3-49, R-R3-21). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - Saves and restores all six colours after the older fields,
//                 so they survive a reload. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-26 - Slices by letter: one column per slice shown (Source
//                 slice, a fixed slice, or All slices with the inactive ones
//                 at alpha 24); the renderVfoDisplay layout for new items
//                 (titles, split box, RX/TX boxes, band text, lock and sync
//                 icons, digit highlight); the new colours, band text and
//                 background saved after field 13; sliceX_* published
//                 readings. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
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

// Modification history (NereusSDR):
//   2026-10-12 - Apply the approved workspace-only 11 px text minimum.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

#include "gui/styles/WorkspacePreview.h"
#include "VfoDisplayItem.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFontMetrics>
#include <QFontMetricsF>

namespace NereusSDR {
namespace {
// NereusSDR correction: height-derived fonts must also fit the face width.
void fitTextWidth(QFont& font, const QStringList& texts, int width)
{
    int low=WorkspacePreview::readableTextPixelSize(1), high=font.pixelSize();
    while(low<high) {
        const int candidate=(low+high+1)/2; font.setPixelSize(WorkspacePreview::readableTextPixelSize(candidate));
        const QFontMetrics metrics(font); bool fits=true;
        for(const QString& text:texts) { fits=fits && metrics.horizontalAdvance(text)<=width; }
        if(fits) { low=candidate; } else { high=candidate-1; }
    }
    font.setPixelSize(WorkspacePreview::readableTextPixelSize(low));
}

// --- renderVfoDisplay geometry (Thetis layout) ---------------------------
// From Thetis MeterManager.cs:39871 [v2.10.3.15]:
// float x_multy = vfo.VFODispMode == clsVfoDisplay.VFODisplayMode.VFO_BOTH ? 1f : 2.07f; // 0.07 to take into consideration the gap that was between a + b
constexpr float kVfoBothXMulti = 1.0f;
constexpr float kVfoSingleXMulti = 2.07f;
// In VFO_BOTH the two displays share the width, each 0.50 of it
// (MeterManager.cs:40253-40261 [v2.10.3.15]); NereusSDR gives each slice
// shown a column of its own, so one column is half of Thetis's w.
constexpr float kVfoBothColumns = 2.0f;
// AddVFODisplay height: _fHeight + _fHeight * 0.75f, with _fHeight = 0.05f.
// From Thetis MeterManager.cs:22586, :25790 [v2.10.3.15]
constexpr float kMeterRowHeight = 0.05f;
constexpr float kVfoBodyHeight = kMeterRowHeight + kMeterRowHeight * 0.75f;
// From Thetis MeterManager.cs:28064 [v2.10.3.15]: if (vfo.ShowBandText) pad = 0.03f;
constexpr float kBandTextPad = 0.03f;
// Text sizes relative to vfo.FontSize: frequency 1.5f, small digits 1.2f,
// everything else 1f. From Thetis MeterManager.cs:40268-40269 [v2.10.3.15]
constexpr float kFrequencyFontScale = 1.5f;
constexpr float kSmallDigitFontScale = 1.2f;
// NereusSDR: Thetis sizes text from the container width (plotText em
// scale); here the 1f text fills three quarters of the 0.4 h boxes it sits
// in, then shrinks to fit its width.
constexpr float kBaseTextOfHeight = 0.30f;
// RX/TX box shades. From Thetis MeterManager.cs:40350-40354 [v2.10.3.15]
constexpr float kDimFactor = 0.6f;
constexpr float kDimmerFactor = 0.3f;
// From Thetis MeterManager.cs:40436, :40442 [v2.10.3.15]: an icon that is
// off draws at Math.Min(64, nFade).
constexpr int kIconOffAlpha = 64;
// From Thetis database.cs:9578 [v2.10.3.15]: outStr = "Out of Band";
constexpr const char* kOutOfBandText = "Out of Band";

int64_t powerOfTen(int exponent)
{
    int64_t value = 1;
    for (int i = 0; i < exponent; ++i) { value *= 10; }
    return value;
}

QColor scaled(const QColor& c, float factor)
{
    return QColor(static_cast<int>(c.red() * factor), static_cast<int>(c.green() * factor),
                  static_cast<int>(c.blue() * factor));
}

QString sliceTitle(int index)
{
    return VfoDisplayItem::tr("Slice %1").arg(QChar(QLatin1Char(static_cast<char>('A' + index))));
}

// A Thetis layout column: x = left + unit * fraction, y = top + h * fraction.
struct Frame {
    qreal left{0};
    qreal top{0};
    qreal unit{0};
    qreal h{0};
    qreal fullH{0};
    qreal x(qreal f) const { return left + unit * f; }
    qreal y(qreal f) const { return top + h * f; }
    QRectF box(qreal x0, qreal y0, qreal x1, qreal y1) const
    {
        return QRectF(x(x0), y(y0), unit * (x1 - x0), h * (y1 - y0));
    }
};

Frame thetisFrame(const QRect& column, int count, bool showBandText)
{
    Frame f;
    f.left = column.left();
    f.top = column.top();
    f.unit = count == 1 ? column.width() * kVfoSingleXMulti
                        : column.width() * kVfoBothColumns * kVfoBothXMulti;
    f.fullH = column.height();
    f.h = f.fullH;
    //subtract 0.03f from h if showing text, as the vfo expands, but everything below is based on non expanded
    // [original inline comment from MeterManager.cs:39891]
    if (showBandText) {
        f.h = f.fullH * (kVfoBodyHeight / (kVfoBodyHeight + kBandTextPad));
    }
    return f;
}

QFont fitted(QFont font, const QString& text, qreal width)
{
    fitTextWidth(font, {text}, qMax(1, static_cast<int>(width)));
    return font;
}

// Text whose top-left (or top-right) is the Thetis plotText point.
void plotText(QPainter& p, const QString& text, qreal x, qreal y, const QFont& font,
              const QColor& colour, bool alignRight = false)
{
    if (text.isEmpty()) { return; }
    p.setFont(font);
    p.setPen(colour);
    const QFontMetricsF metrics(font);
    const qreal width = metrics.horizontalAdvance(text) + 2;
    const QRectF r(alignRight ? x - width : x, y, width, metrics.height());
    p.drawText(r, (alignRight ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignTop, text);
}

// NereusSDR drawings of Thetis's "lock" and "vfo_sync" images.
void drawLockIcon(QPainter& p, const QRectF& r, const QColor& c)
{
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    const QRectF body(r.left() + r.width() * 0.15, r.top() + r.height() * 0.45,
                      r.width() * 0.7, r.height() * 0.55);
    p.drawRoundedRect(body, r.width() * 0.08, r.width() * 0.08);
    QPen pen(c, qMax<qreal>(1.0, r.width() * 0.12));
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const QRectF arc(r.left() + r.width() * 0.28, r.top() + r.height() * 0.05,
                     r.width() * 0.44, r.height() * 0.6);
    p.drawArc(arc, 0, 180 * 16);
    p.drawLine(QPointF(arc.left(), arc.center().y()), QPointF(arc.left(), body.top()));
    p.drawLine(QPointF(arc.right(), arc.center().y()), QPointF(arc.right(), body.top()));
    p.restore();
}

void drawSyncIcon(QPainter& p, const QRectF& r, const QColor& c)
{
    p.save();
    QPen pen(c, qMax<qreal>(1.0, r.width() * 0.12));
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const QRectF ring = r.adjusted(r.width() * 0.15, r.height() * 0.15, -r.width() * 0.15, -r.height() * 0.15);
    p.drawArc(ring, 20 * 16, 140 * 16);
    p.drawArc(ring, 200 * 16, 140 * 16);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    const qreal a = r.width() * 0.22;
    QPainterPath head1;
    head1.moveTo(ring.left() - a * 0.5, ring.center().y() - a * 0.2);
    head1.lineTo(ring.left() + a * 0.5, ring.center().y() - a * 0.2);
    head1.lineTo(ring.left(), ring.center().y() + a * 0.5);
    head1.closeSubpath();
    QPainterPath head2;
    head2.moveTo(ring.right() - a * 0.5, ring.center().y() + a * 0.2);
    head2.lineTo(ring.right() + a * 0.5, ring.center().y() + a * 0.2);
    head2.lineTo(ring.right(), ring.center().y() - a * 0.5);
    head2.closeSubpath();
    p.drawPath(head1);
    p.drawPath(head2);
    p.restore();
}
}


VfoDisplayItem::VfoDisplayItem(QObject* parent) : MeterItem(parent) {}

// ---------------------------------------------------------------------------
// Display modes
// ---------------------------------------------------------------------------

int VfoDisplayItem::fixedSliceIndex(int mode)
{
    if (mode == static_cast<int>(DisplayMode::LegacySliceB)) { return 1; }
    if (mode >= kFixedSliceModeBase && mode < kFixedSliceModeBase + kMaxSliceCount) {
        return mode - kFixedSliceModeBase;
    }
    return -1;
}

int VfoDisplayItem::fromThetisDisplayMode(int thetisMode)
{
    // From Thetis MeterManager.cs:14247-14252 [v2.10.3.15]:
    // VFO_BOTH = 0, VFO_A = 1, VFO_B = 2
    // From Thetis MeterManager.cs:28073 [v2.10.3.15]: out of range is VFO_BOTH.
    if (thetisMode < 0 || thetisMode > 2) { thetisMode = 0; }
    if (thetisMode == 1) { return fixedSliceMode(0); }
    if (thetisMode == 2) { return fixedSliceMode(1); }
    return static_cast<int>(DisplayMode::AllSlices);
}

int VfoDisplayItem::normalizedDisplayMode(int mode)
{
    if (mode == static_cast<int>(DisplayMode::LegacySliceB)) { return fixedSliceMode(1); }
    if (mode == static_cast<int>(DisplayMode::SourceSlice)
        || mode == static_cast<int>(DisplayMode::AllSlices)
        || fixedSliceIndex(mode) >= 0) {
        return mode;
    }
    return static_cast<int>(DisplayMode::SourceSlice);
}

QString VfoDisplayItem::splitUnavailableReason()
{
    return tr("Split is not available.");
}

QString VfoDisplayItem::syncUnavailableReason()
{
    return tr("Sync is not available.");
}

void VfoDisplayItem::setSliceState(int sliceIndex, const VfoSliceState& state)
{
    if (sliceIndex < 0) { return; }
    VfoSliceState s = state;
    s.index = sliceIndex;
    m_sliceStates.insert(sliceIndex, s);
}

VfoSliceState VfoDisplayItem::sliceState(int sliceIndex) const
{
    return stateFor(sliceIndex);
}

VfoSliceState VfoDisplayItem::stateFor(int sliceIndex) const
{
    const auto it = m_sliceStates.constFind(sliceIndex);
    if (it != m_sliceStates.cend()) { return it.value(); }
    VfoSliceState s;
    s.index = sliceIndex;
    if (!m_sliceStates.isEmpty()) { return s; }   // fed, and this slice is not open
    // Nothing fed (a record being read, a preview): the item's own values
    // as one open slice that transmit comes off.
    s.frequencyHz = m_frequencyHz;
    s.mode = m_modeLabel;
    s.filter = m_filterLabel;
    s.band = m_bandLabel;
    s.transmitting = true;
    s.active = true;
    s.open = true;
    return s;
}

bool VfoDisplayItem::interactive(const VfoSliceState& state) const
{
    if (!state.open) { return false; }
    if (m_sliceStates.isEmpty()) { return true; }
    return state.index == m_sourceSliceIndex;
}

QVector<int> VfoDisplayItem::displayedSlices() const
{
    const int fixed = fixedSliceIndex(m_displayMode);
    if (fixed >= 0) { return {fixed}; }
    const int source = m_sourceSliceIndex >= 0 ? m_sourceSliceIndex : 0;
    if (m_displayMode == static_cast<int>(DisplayMode::AllSlices)) {
        int last = -1;
        for (auto it = m_sliceStates.cbegin(); it != m_sliceStates.cend(); ++it) {
            if (it.value().open) { last = qMax(last, it.key()); }
        }
        if (last < 0) { return {source}; }
        QVector<int> slices;
        for (int i = 0; i <= last; ++i) { slices.append(i); }
        return slices;
    }
    return {source};
}

QString VfoDisplayItem::presentationKey() const
{
    QString key = QString::number(m_frequencyHz) + m_modeLabel + m_filterLabel + m_bandLabel
        + m_unavailableText + QString::number(m_sourceSliceIndex);
    for (int index : displayedSlices()) {
        const VfoSliceState s = stateFor(index);
        key += QLatin1Char('|') + QString::number(s.index) + QLatin1Char(',') + QString::number(s.frequencyHz)
            + QLatin1Char(',') + s.mode + QLatin1Char(',') + s.filter + QLatin1Char(',') + s.band
            + QLatin1Char(',') + s.bandText + QLatin1Char(',') + QString::number(s.stepHz)
            + QString::number(s.locked) + QString::number(s.transmitting)
            + QString::number(s.active) + QString::number(s.open);
    }
    return key;
}

void VfoDisplayItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    const VfoDisplayThetisDefaults& d = kVfoDisplayThetisDefaults;
    m_freqColour      = QColor::fromRgb(d.frequencyColour);
    m_freqSmallColour = QColor::fromRgb(d.frequencySmallColour);
    m_modeColour      = QColor::fromRgb(d.modeColour);
    m_filterColour    = QColor::fromRgb(d.filterColour);
    m_titleColour     = QColor::fromRgb(d.titleColour);
    m_splitBackColour = QColor::fromRgb(d.splitBackColour);
    m_splitColour     = QColor::fromRgb(d.splitColour);
    m_rxColour        = QColor::fromRgb(d.rxColour);
    m_txColour        = QColor::fromRgb(d.txColour);
    m_bandColour      = QColor::fromRgb(d.bandColour);
    m_digitHighlight  = QColor::fromRgb(d.digitHighlightColour);
    m_bandTextColour  = QColor::fromRgb(d.bandTextColour);
    m_lockColour      = QColor::fromRgb(d.lockColour);
    m_syncColour      = QColor::fromRgb(d.syncColour);
    m_backColour      = QColor::fromRgb(d.backColour);
    m_showBandText    = d.showBandText;
    m_layout          = d.layout;
    m_displayMode     = static_cast<int>(DisplayMode::SourceSlice);
}

QHash<QString, QVariant> VfoDisplayItem::publishedReadings() const
{
    // From Thetis MeterManager.cs:31101-31120 [v2.10.3.15]: the VFO display
    // copies its frequency ("f6" MHz), band, filter name and mode into the
    // readings. Named by slice letter here, for each open slice shown.
    QHash<QString, QVariant> readings;
    if (m_sliceStates.isEmpty()) { return readings; }
    for (int index : displayedSlices()) {
        const VfoSliceState s = stateFor(index);
        if (!s.open) { continue; }
        const QString prefix = QStringLiteral("slice") + QChar(QLatin1Char(static_cast<char>('A' + index)))
            + QLatin1Char('_');
        readings.insert(prefix + QStringLiteral("freq"), QString::number(s.frequencyHz / 1.0e6, 'f', 6));
        readings.insert(prefix + QStringLiteral("mode"), s.mode);
        readings.insert(prefix + QStringLiteral("filter"), s.filter);
        readings.insert(prefix + QStringLiteral("band"), s.band);
        readings.insert(prefix + QStringLiteral("step"), s.stepHz);
    }
    return readings;
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

QString VfoDisplayItem::formatFrequency(int64_t hz) const
{
    const int mhz = static_cast<int>(hz / 1000000);
    const int khz = static_cast<int>((hz / 1000) % 1000);
    const int h   = static_cast<int>(hz % 1000);
    return QStringLiteral("%1.%2.%3")
        .arg(mhz, 2).arg(khz, 3, 10, QLatin1Char('0')).arg(h, 3, 10, QLatin1Char('0'));
}

int64_t VfoDisplayItem::digitWeightAt(float xFraction) const
{
    // From Thetis clsVfoDisplay: 9 digit positions left-to-right
    static constexpr int64_t kDigitWeights[] = {
        10000000, 1000000, 100000, 10000, 1000, 100, 10, 1, 1
    };
    const int digitIndex = static_cast<int>(xFraction * 9.0f);
    if (digitIndex < 0) { return kDigitWeights[0]; }
    if (digitIndex > 8) { return kDigitWeights[8]; }
    return kDigitWeights[digitIndex];
}

QRect VfoDisplayItem::columnRect(const QRect& rect, int column, int count) const
{
    if (count <= 1) { return rect; }
    const int left = rect.left() + rect.width() * column / count;
    const int right = rect.left() + rect.width() * (column + 1) / count;
    return QRect(left, rect.top(), right - left, rect.height());
}

VfoDisplayItem::Hit VfoDisplayItem::thetisHitAt(const QPointF& pos, int widgetW, int widgetH) const
{
    Hit hit;
    if (m_layout != Layout::Thetis || !m_unavailableText.isEmpty()) { return hit; }
    const QRect rect = pixelRect(widgetW, widgetH);
    const QVector<int> slices = displayedSlices();
    const int count = slices.size();
    for (int column = 0; column < count; ++column) {
        const QRect col = columnRect(rect, column, count);
        if (!col.contains(pos.toPoint())) { continue; }
        const VfoSliceState s = stateFor(slices[column]);
        if (!s.open) { return hit; }
        hit.column = column;
        hit.sliceIndex = s.index;
        const Frame f = thetisFrame(col, count, m_showBandText);
        const qreal mx = (pos.x() - f.left) / f.unit;
        const qreal my = (pos.y() - f.top) / f.h;
        const auto set = [&](Region region, qreal x0, qreal y0, qreal x1, qreal y1) {
            hit.region = region;
            hit.box = f.box(x0, y0, x1, y1);
        };
        // From Thetis MeterManager.cs:39927-40153 [v2.10.3.15]
        // band
        if (my >= 0.555 && my <= 0.900 && mx >= 0.333 && mx <= 0.480) { set(Region::Band, 0.333, 0.555, 0.480, 0.900); }
        // mode
        if (my >= 0.555 && my <= 0.900 && mx >= 0.008 && mx <= 0.078) { set(Region::Mode, 0.008, 0.530, 0.078, 0.530 + (0.900 - 0.555)); }
        // filter
        if (my >= 0.555 && my <= 0.900 && mx >= 0.250 && mx <= 0.332) { set(Region::Filter, 0.250, 0.555, 0.332, 0.900); }
        //split vfoA only
        if (s.index == 0 && my >= 0.030 && my <= 0.430 && mx >= 0.100 && mx <= 0.200) { set(Region::Split, 0.100, 0.030, 0.200, 0.430); }
        //tx
        if (my >= 0.520 && my <= 0.920 && mx >= 0.152 && mx <= 0.200) { set(Region::Tx, 0.152, 0.520, 0.200, 0.920); }
        //lock
        if (my >= 0.520 && my <= 0.920 && mx >= 0.200 && mx <= 0.223) { set(Region::Lock, 0.200, 0.520, 0.223, 0.920); }
        //sync
        if (my >= 0.520 && my <= 0.920 && mx >= 0.224 && mx <= 0.250) { set(Region::Sync, 0.224, 0.520, 0.250, 0.920); }
        // large numbers
        if (my >= 0.100 && my <= 0.530) {
            //mhz
            if (mx >= 0.216 && mx <= 0.330) {
                constexpr int boxes = 5;
                constexpr qreal boxSize = (0.330 - 0.216) / boxes;
                const int box = qMin(boxes - 1, static_cast<int>((mx - 0.216) / boxSize));
                const int mhzDigits = QString::number(s.frequencyHz / 1000000).size();
                if (((boxes - 1) - box) <= mhzDigits - 1) {
                    set(Region::Digit, 0.216 + boxSize * box, 0.100, 0.216 + boxSize * (box + 1), 0.530);
                    hit.step = 1000000LL * powerOfTen((boxes - 1) - box);
                }
            }
            //khz
            if (mx >= 0.346 && mx <= 0.416) {
                constexpr int boxes = 3;
                constexpr qreal boxSize = (0.416 - 0.346) / boxes;
                const int box = qMin(boxes - 1, static_cast<int>((mx - 0.346) / boxSize));
                set(Region::Digit, 0.346 + boxSize * box, 0.100, 0.346 + boxSize * (box + 1), 0.530);
                hit.step = 1000LL * powerOfTen((boxes - 1) - box);
            }
        }
        //small numbers
        if (my >= 0.170 && my <= 0.530) {
            //hz
            if (mx >= 0.425 && mx <= 0.480) {
                constexpr int boxes = 3;
                constexpr qreal boxSize = (0.480 - 0.425) / boxes;
                const int box = qMin(boxes - 1, static_cast<int>((mx - 0.425) / boxSize));
                set(Region::Digit, 0.425 + boxSize * box, 0.170, 0.425 + boxSize * (box + 1), 0.530);
                hit.step = powerOfTen((boxes - 1) - box);
            }
        }
        return hit;
    }
    return hit;
}

QString VfoDisplayItem::reasonAt(const QPointF& pos, int widgetW, int widgetH) const
{
    const Hit hit = thetisHitAt(pos, widgetW, widgetH);
    if (hit.region == Region::Split) { return splitUnavailableReason(); }
    if (hit.region == Region::Sync) { return syncUnavailableReason(); }
    return {};
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

void VfoDisplayItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Background
    p.fillRect(rect, m_backColour);
    if (m_layout == Layout::Compact) {
        p.setPen(QPen(QColor(0x1e, 0x2e, 0x3e), 1));
        p.drawRect(rect);
    }

    // R-R3-49 / R-R3-21: the container's slice is not open. Say so in
    // place of the frequency and labels; a stale frequency would read as
    // live.
    if (!m_unavailableText.isEmpty()) {
        // RX/TX indicator bar at left
        const QColor stateColour = m_transmitting ? m_txColour : m_rxColour;
        p.fillRect(QRect(rect.left(), rect.top(), 3, rect.height()), stateColour);
        const QRect textRect = rect.adjusted(6, 2, -6, -2);
        QFont font = WorkspacePreview::readableFont(p.font(), p.device());
        font.setPixelSize(WorkspacePreview::readableTextPixelSize(qMax(10, textRect.height() / 4)));
        font.setBold(false);
        p.setFont(font);
        p.setPen(m_modeColour);
        p.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, m_unavailableText);
        return;
    }

    const QVector<int> slices = displayedSlices();
    const int count = slices.size();
    // From Thetis MeterManager.cs:39880-39889 [v2.10.3.15]: a VFO that is
    // not in use draws at alpha 24. In All slices mode that is every slice
    // but the active one.
    const bool fadeInactive = m_displayMode == static_cast<int>(DisplayMode::AllSlices) && count > 1;
    const Hit hover = m_hovering ? thetisHitAt(m_hoverPos, widgetW, widgetH) : Hit();
    for (int column = 0; column < count; ++column) {
        const QRect col = columnRect(rect, column, count);
        const VfoSliceState s = stateFor(slices[column]);
        const int alpha = fadeInactive && !s.active ? kInactiveSliceAlpha : 255;
        p.save();
        p.setOpacity(alpha / 255.0);
        if (!s.open) {
            p.fillRect(QRect(col.left(), col.top(), 3, col.height()), m_rxColour);
            const QRect textRect = col.adjusted(6, 2, -6, -2);
            QFont font = WorkspacePreview::readableFont(p.font(), p.device());
            font.setPixelSize(WorkspacePreview::readableTextPixelSize(qMax(10, textRect.height() / 4)));
            p.setFont(font);
            p.setPen(m_modeColour);
            p.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap,
                       tr("%1 is not open").arg(sliceTitle(s.index)));
        } else if (m_layout == Layout::Thetis) {
            paintThetisColumn(p, col, s, count, alpha, hover);
        } else {
            paintCompactColumn(p, col, s);
        }
        p.restore();
    }
}

void VfoDisplayItem::paintCompactColumn(QPainter& p, const QRect& rect, const VfoSliceState& s)
{
    // RX/TX indicator bar at left (the TX marker: lit on the slice
    // transmit comes off while transmitting).
    const QColor stateColour = (m_transmitting && s.transmitting) ? m_txColour : m_rxColour;
    p.fillRect(QRect(rect.left(), rect.top(), 3, rect.height()), stateColour);

    // Frequency text (top 60%)
    const QString frequencyText = formatFrequency(s.frequencyHz);
    const QRect freqRect(rect.left() + 6, rect.top() + 2, rect.width() - 12, rect.height() * 6 / 10);
    QFont freqFont = WorkspacePreview::readableFont(p.font(), p.device());
    freqFont.setPixelSize(WorkspacePreview::readableTextPixelSize(qMax(12, freqRect.height() - 4)));
    freqFont.setBold(true);
    fitTextWidth(freqFont,{frequencyText},freqRect.width());
    p.setFont(freqFont);
    p.setPen(m_freqColour);
    p.drawText(freqRect, Qt::AlignVCenter | Qt::AlignLeft, frequencyText);

    // Split indicator
    if (m_split) {
        p.setPen(m_splitColour);
        QFont splitFont = freqFont;
        splitFont.setPixelSize(WorkspacePreview::readableTextPixelSize(freqFont.pixelSize() / 2));
        p.setFont(splitFont);
        p.drawText(freqRect, Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("SPLIT"));
    }

    // Labels below frequency (bottom 40%)
    const QRect labelRect(rect.left() + 6, rect.top() + rect.height() * 6 / 10,
                          rect.width() - 12, rect.height() * 4 / 10);
    QFont labelFont = WorkspacePreview::readableFont(p.font(), p.device());
    labelFont.setPixelSize(WorkspacePreview::readableTextPixelSize(qMax(9, labelRect.height() - 4)));
    labelFont.setBold(false);
    fitTextWidth(labelFont,{s.mode,s.filter,s.band},labelRect.width()/3);
    p.setFont(labelFont);

    const int thirdW = labelRect.width() / 3;
    p.setPen(m_modeColour);
    p.drawText(QRect(labelRect.left(), labelRect.top(), thirdW, labelRect.height()), Qt::AlignCenter, s.mode);
    p.setPen(m_filterColour);
    p.drawText(QRect(labelRect.left() + thirdW, labelRect.top(), thirdW, labelRect.height()), Qt::AlignCenter, s.filter);
    p.setPen(m_bandColour);
    p.drawText(QRect(labelRect.left() + 2*thirdW, labelRect.top(), thirdW, labelRect.height()), Qt::AlignCenter, s.band);
}

void VfoDisplayItem::paintThetisColumn(QPainter& p, const QRect& col, const VfoSliceState& s,
                                       int count, int alpha, const Hit& hover)
{
    const Frame f = thetisFrame(col, count, m_showBandText);
    const bool canAct = interactive(s);

    // TX marker (NereusSDR): lit on the slice transmit comes off while
    // transmitting.
    const QColor stateColour = (m_transmitting && s.transmitting) ? m_txColour : m_rxColour;
    p.fillRect(QRect(col.left(), col.top(), 3, col.height()), stateColour);

    // From Thetis MeterManager.cs:40220-40224 [v2.10.3.15]: the box under
    // the mouse (a digit, the band or the filter) fills with the digit
    // highlight colour before the text.
    const bool hoverHere = canAct && hover.sliceIndex == s.index
        && (hover.region == Region::Digit || hover.region == Region::Band || hover.region == Region::Filter);
    if (hoverHere) {
        p.fillRect(hover.box, m_digitHighlight);
    }

    QFont base = WorkspacePreview::readableFont(p.font(), p.device());
    base.setBold(false);
    base.setPixelSize(WorkspacePreview::readableTextPixelSize(qMax(1, static_cast<int>(f.h * kBaseTextOfHeight))));

    // Titles read "Slice A" and so on in place of "VFO A" / "VFO Sub" /
    // "VFO B". From Thetis MeterManager.cs:40246-40262 [v2.10.3.15]
    const QString title = sliceTitle(s.index);
    const qreal titleRoom = (s.index == 0 ? 0.100 - 0.010 : 0.216 - 0.010) * f.unit;
    plotText(p, title, f.x(0.01), f.y(0.03), fitted(base, title, titleRoom), m_titleColour);

    // frequency
    // From Thetis MeterManager.cs:40264-40270 [v2.10.3.15]: MHz.kHz right
    // aligned at 0.415 in 1.5x text, Hz right aligned at 0.48 in 1.2x text.
    const qint64 hz = s.frequencyHz;
    const QString mhz = QString::number(hz / 1000000);
    const QString khz = QStringLiteral("%1").arg((hz / 1000) % 1000, 3, 10, QLatin1Char('0'));
    const QString small = QStringLiteral("%1").arg(hz % 1000, 3, 10, QLatin1Char('0'));
    const QString sVfo = mhz + QLatin1Char('.') + khz;// + "." + hz;
    QFont freqFont = base;
    freqFont.setPixelSize(WorkspacePreview::readableTextPixelSize(
        qMax(1, static_cast<int>(f.h * kBaseTextOfHeight * kFrequencyFontScale))));
    // The digit boxes span 0.216 to 0.416 (five MHz digits, the point and
    // three kHz digits); the text fits them.
    fitTextWidth(freqFont, {QStringLiteral("00000.000")}, static_cast<int>((0.416 - 0.216) * f.unit));
    QFont smallFont = base;
    smallFont.setPixelSize(WorkspacePreview::readableTextPixelSize(
        qMax(1, static_cast<int>(f.h * kBaseTextOfHeight * kSmallDigitFontScale))));
    fitTextWidth(smallFont, {QStringLiteral("000")}, static_cast<int>((0.480 - 0.425) * f.unit));
    const qreal freqTop = f.y(0.02);
    plotText(p, sVfo, f.x(0.415), freqTop, freqFont, m_freqColour, true);
    // NereusSDR: the small digits share the large digits' baseline (Thetis
    // places them at 0.11 h for its own font ratio).
    const qreal baseline = freqTop + QFontMetricsF(freqFont).ascent();
    plotText(p, small, f.x(0.48), baseline - QFontMetricsF(smallFont).ascent(), smallFont, m_freqSmallColour, true);

    // mode VFOA
    // From Thetis MeterManager.cs:40302-40305 [v2.10.3.15]
    plotText(p, s.mode, f.x(0.01), f.y(0.52), fitted(base, s.mode, (0.100 - 0.010) * f.unit), m_modeColour);

    //band VFOA
    // From Thetis MeterManager.cs:40314-40319 [v2.10.3.15]
    if (!s.band.isEmpty()) {
        const QString band = s.band + QStringLiteral(" band");
        plotText(p, band, f.x(0.48), f.y(0.54), fitted(base, band, (0.480 - 0.333) * f.unit), m_bandColour, true);
    }

    //split only on VFOA
    // From Thetis MeterManager.cs:40329-40336 [v2.10.3.15]. NereusSDR has
    // no split (RadioModel::split is always false), so the box draws as
    // Thetis's split-off state and its tooltip says why.
    if (s.index == 0) {
        const QRectF rectSplit = f.box(0.1, 0.03, 0.2, 0.43);
        p.fillRect(rectSplit, m_splitBackColour);
        const QColor splitColor = m_split ? m_splitColour : QColor(Qt::black);
        plotText(p, QStringLiteral("SPLIT"), rectSplit.x() + 0.015 * f.unit, rectSplit.y(),
                 fitted(base, QStringLiteral("SPLIT"), (0.1 - 0.015) * f.unit), splitColor);
    }

    //-- tx/rx state
    // From Thetis MeterManager.cs:40338-40343 [v2.10.3.15]: RX lights while
    // not transmitting; the TX box is bright on the slice that can
    // transmit and its text lights while it transmits.
    const bool canTx = s.transmitting;
    const bool rxOn = !m_transmitting;
    const bool txOn = m_transmitting && canTx;

    // tx/rx box colours
    const QColor cRx = m_rxColour;
    const QColor cDimRx = scaled(cRx, kDimFactor);
    const QColor cDimerRx = scaled(cRx, kDimmerFactor);
    const QColor cTx = m_txColour;
    const QColor cDimTx = scaled(cTx, kDimFactor);
    const QColor cDimerTx = scaled(cTx, kDimmerFactor);

    const QFont boxFont = fitted(base, QStringLiteral("RX"), (0.048 - 0.005) * f.unit);
    //rx box VFOA
    // From Thetis MeterManager.cs:40360-40365 [v2.10.3.15]
    QRectF rct = f.box(0.1, 0.52, 0.148, 0.92);
    p.fillRect(rct, rxOn ? cDimRx : cDimerRx);
    plotText(p, QStringLiteral("RX"), rct.x() + 0.005 * f.unit, rct.y(), boxFont, rxOn ? cRx : QColor(Qt::black));

    //tx box VFOA
    // From Thetis MeterManager.cs:40367-40372 [v2.10.3.15]
    rct = f.box(0.152, 0.52, 0.200, 0.92);
    p.fillRect(rct, canTx ? cDimTx : cDimerTx);
    plotText(p, QStringLiteral("TX"), rct.x() + 0.005 * f.unit, rct.y(), boxFont, txOn ? cTx : QColor(Qt::black));

    //filter VFOA
    // From Thetis MeterManager.cs:40393-40396 [v2.10.3.15]
    plotText(p, s.filter, f.x(0.25) + 0.005 * f.unit, f.y(0.54),
             fitted(base, s.filter, (0.333 - 0.255) * f.unit), m_filterColour);

    if (m_showBandText) {
        // band text, needs the full height, as we adjusted it above
        // From Thetis MeterManager.cs:40404-40413 [v2.10.3.15]: centred on
        // 0.25 of the width. NereusSDR: the text sits in the strip the band
        // text adds below the display.
        const qreal stripTop = f.top + f.h;
        const qreal stripH = f.fullH - f.h;
        const QString text = s.bandText.isEmpty() ? QString::fromLatin1(kOutOfBandText) : s.bandText;
        QFont bandTextFont = base;
        bandTextFont.setPixelSize(WorkspacePreview::readableTextPixelSize(qMax(1, static_cast<int>(stripH * 0.8))));
        fitTextWidth(bandTextFont, {text}, static_cast<int>(0.48 * f.unit));
        p.setFont(bandTextFont);
        p.setPen(m_bandTextColour);
        const qreal width = 0.48 * f.unit;
        p.drawText(QRectF(f.x(0.25) - width / 2.0, stripTop, width, stripH), Qt::AlignCenter, text);
    }

    //vfo lock / vfo sync icons
    // From Thetis MeterManager.cs:40425-40443 [v2.10.3.15]: the lock icon is
    // full while the slice is locked and at Math.Min(64, nFade) when not;
    // NereusSDR has no VFO sync, so the sync icon is always off.
    p.setOpacity(1.0);
    const qreal iconSize = 0.026 * f.unit;
    QColor lock = m_lockColour;
    lock.setAlpha(s.locked ? alpha : qMin(kIconOffAlpha, alpha));
    drawLockIcon(p, QRectF(f.x(0.199), f.y(0.58), iconSize, iconSize), lock);
    QColor sync = m_syncColour;
    sync.setAlpha(qMin(kIconOffAlpha, alpha));
    drawSyncIcon(p, QRectF(f.x(0.224), f.y(0.58), iconSize, iconSize), sync);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

bool VfoDisplayItem::handleWheel(QWheelEvent* event, int widgetW, int widgetH)
{
    const int direction = (event->angleDelta().y() > 0) ? 1 : -1;
    if (m_layout == Layout::Thetis) {
        const Hit hit = thetisHitAt(event->position(), widgetW, widgetH);
        if (hit.region != Region::Digit || !interactive(stateFor(hit.sliceIndex))) { return false; }
        emit frequencyChangeRequested(direction * hit.step);
        return true;
    }
    const QRect itemRect = pixelRect(widgetW, widgetH);
    const QVector<int> slices = displayedSlices();
    for (int column = 0; column < slices.size(); ++column) {
        const QRect rect = columnRect(itemRect, column, slices.size());
        if (!rect.contains(event->position().toPoint())) { continue; }
        if (!interactive(stateFor(slices[column]))) { return false; }
        const QRect freqRect(rect.left() + 6, rect.top(), rect.width() - 12, rect.height() * 6 / 10);
        if (!freqRect.contains(event->position().toPoint())) { return false; }

        const float xFrac = static_cast<float>(event->position().x() - freqRect.left()) / freqRect.width();
        const int64_t weight = digitWeightAt(xFrac);
        emit frequencyChangeRequested(direction * weight);
        return true;
    }
    return false;
}

bool VfoDisplayItem::handleMousePress(QMouseEvent* event, int widgetW, int widgetH)
{
    if (event->button() != Qt::RightButton) { return false; }
    if (m_layout == Layout::Thetis) {
        const Hit hit = thetisHitAt(event->position(), widgetW, widgetH);
        if (!interactive(stateFor(hit.sliceIndex))) { return false; }
        if (hit.region == Region::Band) { emit bandStackRequested(0); return true; }
        if (hit.region == Region::Filter) { emit filterContextRequested(0); return true; }
        return false;
    }
    const QRect itemRect = pixelRect(widgetW, widgetH);
    const QVector<int> slices = displayedSlices();
    for (int column = 0; column < slices.size(); ++column) {
        const QRect rect = columnRect(itemRect, column, slices.size());
        if (!rect.contains(event->position().toPoint())) { continue; }
        if (!interactive(stateFor(slices[column]))) { return false; }
        const QRect labelRect(rect.left() + 6, rect.top() + rect.height() * 6 / 10,
                              rect.width() - 12, rect.height() * 4 / 10);
        if (!labelRect.contains(event->position().toPoint())) { return false; }

        const int thirdW = labelRect.width() / 3;
        const int relX = event->position().toPoint().x() - labelRect.left();
        if (relX > 2 * thirdW) { emit bandStackRequested(0); return true; }
        else if (relX > thirdW) { emit filterContextRequested(0); return true; }
        return false;
    }
    return false;
}

bool VfoDisplayItem::handleMouseMove(QMouseEvent* event, int widgetW, int widgetH)
{
    if (m_layout != Layout::Thetis) { return false; }
    const QRectF before = m_hovering ? thetisHitAt(m_hoverPos, widgetW, widgetH).box : QRectF();
    m_hoverPos = event->position();
    m_hovering = true;
    return thetisHitAt(m_hoverPos, widgetW, widgetH).box != before;
}

bool VfoDisplayItem::handleMouseLeave()
{
    if (!m_hovering) { return false; }
    m_hovering = false;
    return m_layout == Layout::Thetis;
}

// ---------------------------------------------------------------------------
// Record
// ---------------------------------------------------------------------------

QString VfoDisplayItem::serialize() const
{
    return QStringLiteral("VFO|%1|%2|%3|%4|%5|%6|%7")
        .arg(m_x).arg(m_y).arg(m_w).arg(m_h)
        .arg(m_bindingId).arg(m_zOrder).arg(m_displayMode)
        // Colours follow the older fields, so a record saved before they
        // existed still loads (with the default colours).
        + QStringLiteral("|%1|%2|%3|%4|%5|%6")
              .arg(m_freqColour.name(QColor::HexArgb))
              .arg(m_modeColour.name(QColor::HexArgb))
              .arg(m_filterColour.name(QColor::HexArgb))
              .arg(m_bandColour.name(QColor::HexArgb))
              .arg(m_rxColour.name(QColor::HexArgb))
              .arg(m_txColour.name(QColor::HexArgb))
        // Fields 14-24 follow, so an older record loads with the
        // constructor's values (the #0a0a18 background, Compact layout).
        + QLatin1Char('|') + m_freqSmallColour.name(QColor::HexArgb)
        + QLatin1Char('|') + m_titleColour.name(QColor::HexArgb)
        + QLatin1Char('|') + m_splitBackColour.name(QColor::HexArgb)
        + QLatin1Char('|') + m_splitColour.name(QColor::HexArgb)
        + QLatin1Char('|') + m_digitHighlight.name(QColor::HexArgb)
        + QLatin1Char('|') + m_bandTextColour.name(QColor::HexArgb)
        + QLatin1Char('|') + m_lockColour.name(QColor::HexArgb)
        + QLatin1Char('|') + m_syncColour.name(QColor::HexArgb)
        + QLatin1Char('|') + m_backColour.name(QColor::HexArgb)
        + QLatin1Char('|') + QString::number(m_showBandText ? 1 : 0)
        + QLatin1Char('|') + QString::number(static_cast<int>(m_layout));
}

bool VfoDisplayItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 7 || parts[0] != QLatin1String("VFO")) { return false; }
    m_x = parts[1].toFloat(); m_y = parts[2].toFloat();
    m_w = parts[3].toFloat(); m_h = parts[4].toFloat();
    m_bindingId = parts[5].toInt(); m_zOrder = parts[6].toInt();
    // A saved VFO B (1) loads as fixed Slice B and re-saves as 4; a saved
    // Both (2) is All slices.
    if (parts.size() > 7) { setDisplayModeValue(parts[7].toInt()); }
    const auto readColour = [&](int index, QColor& colour) {
        if (index >= parts.size()) { return; }
        const QColor parsed(parts[index]);
        if (parsed.isValid()) { colour = parsed; }
    };
    readColour(8, m_freqColour);
    readColour(9, m_modeColour);
    readColour(10, m_filterColour);
    readColour(11, m_bandColour);
    readColour(12, m_rxColour);
    readColour(13, m_txColour);
    readColour(14, m_freqSmallColour);
    readColour(15, m_titleColour);
    readColour(16, m_splitBackColour);
    readColour(17, m_splitColour);
    readColour(18, m_digitHighlight);
    readColour(19, m_bandTextColour);
    readColour(20, m_lockColour);
    readColour(21, m_syncColour);
    readColour(22, m_backColour);
    if (parts.size() > 23) { m_showBandText = parts[23].toInt() != 0; }
    if (parts.size() > 24) {
        m_layout = parts[24].toInt() == static_cast<int>(Layout::Thetis) ? Layout::Thetis : Layout::Compact;
    }
    return true;
}

} // namespace NereusSDR
