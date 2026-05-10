// =================================================================
// src/gui/widgets/GradientPickerWidget.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/ucLGPicker.cs (full UserControl,
//   1009 lines), original licence from Thetis source is included
//   below.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-10 — Reimplemented in C++20/Qt6 for NereusSDR by
//                J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Claude Opus 4.7 (1M context).
//                Phase 3M-5c (Custom Gradient Picker widget).
//                UI is NereusSDR-native (Qt). Encoded format is
//                byte-for-byte Thetis-compatible per ucLGPicker.cs:
//                666-738 + 912-930 [v2.10.3.13+501e3f51] for skin
//                import compatibility (Phase 3H).
// =================================================================
//
// --- From ucLGPicker.cs ---
/*  ucLGPicker.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2025 Richard Samphire MW0LGE

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

#include "gui/widgets/GradientPickerWidget.h"

#include <QByteArray>
#include <QContextMenuEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <limits>

namespace NereusSDR {

namespace {

// Pack QColor (ARGB byte components) into a signed int32 matching the
// .NET Color.ToArgb() representation. For full-alpha colors the sign bit
// is set, so a fresh-default black (0xFF000000) becomes -16777216 — the
// canonical Thetis encoded value for the first stop.
qint32 colorToSignedArgb(const QColor& c) noexcept
{
    const quint32 u = (static_cast<quint32>(c.alpha()) << 24)
                    | (static_cast<quint32>(c.red())   << 16)
                    | (static_cast<quint32>(c.green()) <<  8)
                    |  static_cast<quint32>(c.blue());
    return static_cast<qint32>(u);
}

// Inverse: take a signed int32 ARGB (decoded from Thetis-emitted text) and
// rebuild the QColor.
QColor signedArgbToColor(qint32 argb) noexcept
{
    const quint32 u = static_cast<quint32>(argb);
    const int a = static_cast<int>((u >> 24) & 0xFFu);
    const int r = static_cast<int>((u >> 16) & 0xFFu);
    const int g = static_cast<int>((u >>  8) & 0xFFu);
    const int b = static_cast<int>( u        & 0xFFu);
    return QColor(r, g, b, a);
}

// Component-wise byte interpolation matching Thetis ColorInterpolator
// (ucLGPicker.cs:970-1008 [v2.10.3.13+501e3f51]):
//
//   byte InterpolateComponent(c1, c2, lambda) {
//       return (byte)(c1 + (c2 - c1) * lambda);
//   }
//
//   Color InterpolateBetween(c1, c2, lambda) {
//       if (lambda < 0 || lambda > 1) return Color.Empty;
//       return Color.FromArgb(/* per-component InterpolateComponent */);
//   }
//
// Thetis returns Color.Empty when lambda is outside [0, 1]. Callers in
// Thetis (GetColourAtPercent at :770-792) only pass lambda already
// computed inside [0, 1] (the iR == iL fallback yields percWidth=0,
// scale=0, lambda=0 — no out-of-range path). NereusSDR matches that
// guarantee by clamping in colorAtPercent before calling here, so this
// helper unconditionally interpolates.
QColor interpolateBetween(const QColor& c1, const QColor& c2, double lambda) noexcept
{
    auto lerp = [lambda](int v1, int v2) -> int {
        // (byte)(v1 + (v2 - v1) * lambda) — Thetis cast wraps modulo 256;
        // since lambda is in [0, 1] and v1/v2 in [0, 255] the sum stays
        // in [0, 255], so a static_cast<int> is byte-equivalent here.
        return static_cast<int>(v1 + (v2 - v1) * lambda);
    };
    return QColor(lerp(c1.red(),   c2.red()),
                  lerp(c1.green(), c2.green()),
                  lerp(c1.blue(),  c2.blue()),
                  lerp(c1.alpha(), c2.alpha()));
}

} // namespace

// ---- Construction ----

// From Thetis ucLGPicker.cs:98-115 [v2.10.3.13+501e3f51] (default ctor):
//
//   m_dictColours = new Dictionary<int, GradColours>();
//   for (int i = 0; i < m_nGrippers; i++) {
//       addColour(i, i / (float)m_nGrippers,
//                 Color.FromArgb(255, i*(256/m_nGrippers - 1),
//                                     i*(256/m_nGrippers - 1),
//                                     i*(256/m_nGrippers - 1)),
//                 m_dictColours);
//   }
//   addColour(m_nGrippers, 1, Color.FromArgb(255, 255, 255, 255), m_dictColours);
//   for (int i = 1; i < m_nGrippers; i++) {
//       enableGripper(i, false);
//   }
//   rebuildSortedColours();
//
// 256 / 8 = 32; 32 - 1 = 31. So the grayscale ramp at index i (0..7) is
// (255, i*31, i*31, i*31). Index 8 is (255, 255, 255, 255). After the
// disable loop, only indices 0 and 8 remain enabled.
GradientPickerWidget::GradientPickerWidget(QWidget* parent)
    : QWidget(parent)
{
    m_stops.reserve(kDefaultStopCount + 1);
    for (int i = 0; i < kDefaultStopCount; ++i) {
        const int gray = i * (256 / kDefaultStopCount - 1);
        GradientStop stop;
        stop.percent = static_cast<float>(i) / static_cast<float>(kDefaultStopCount);
        stop.color   = QColor(gray, gray, gray, 255);
        stop.enabled = true;
        m_stops.append(stop);
    }
    {
        GradientStop white;
        white.percent = 1.0f;
        white.color   = QColor(255, 255, 255, 255);
        white.enabled = true;
        m_stops.append(white);
    }
    for (int i = 1; i < kDefaultStopCount; ++i) {
        m_stops[i].enabled = false;
    }
    rebuildSortedStops();

    // Mouse tracking is needed for hover-style gripper highlight reporting.
    setMouseTracking(true);
}

GradientPickerWidget::~GradientPickerWidget() = default;

// ---- Properties ----

void GradientPickerWidget::setShowAsPercent(bool on)
{
    if (m_showAsPercent == on) {
        return;
    }
    m_showAsPercent = on;
    update();
}

void GradientPickerWidget::setIncludeAlphaInPreview(bool on)
{
    if (m_includeAlphaInPreview == on) {
        return;
    }
    m_includeAlphaInPreview = on;
    rebuildSortedStops();
    update();
}

QColor GradientPickerWidget::colorForSelectedStop() const
{
    // From Thetis ucLGPicker.cs:552-557 [v2.10.3.13+501e3f51]:
    //   if (m_nSelectedGripper == -1) return Color.Empty;
    //   return m_dictColours[m_nSelectedGripper].color;
    if (m_selectedIndex < 0 || m_selectedIndex >= m_stops.size()) {
        return QColor();
    }
    return m_stops[m_selectedIndex].color;
}

void GradientPickerWidget::setColorForSelectedStop(const QColor& c)
{
    // From Thetis ucLGPicker.cs:558-564 [v2.10.3.13+501e3f51]:
    //   if (m_nSelectedGripper == -1) return;
    //   setColour(m_nSelectedGripper, value, true);
    //   OnChanged(EventArgs.Empty);
    if (m_selectedIndex < 0 || m_selectedIndex >= m_stops.size()) {
        return;
    }
    m_stops[m_selectedIndex].color = c;
    rebuildSortedStops();
    update();
    emit changed();
}

// ---- Serialization ----

// From Thetis ucLGPicker.cs:666-682 [v2.10.3.13+501e3f51] (Text getter):
//
//   string sRet = m_dictColours.Count.ToString() + "|";
//   for (int i = 0; i < m_dictColours.Count; i++) {
//       if (m_dictColours[i].enabled) sRet += "1|";
//       else                          sRet += "0|";
//       sRet += m_dictColours[i].percent.ToString("0.000") + "|";
//       sRet += m_dictColours[i].color.ToArgb().ToString() + "|";
//   }
//   return sRet;
//
// Format: "<count>|<en0>|<perc0>|<argb0>|<en1>|<perc1>|<argb1>|...|"
// Trailing pipe is present because every field appends "|" after itself,
// and the final argb is no exception.
QString GradientPickerWidget::text() const
{
    QString s;
    s += QString::number(m_stops.size());
    s += QLatin1Char('|');
    for (const GradientStop& stop : m_stops) {
        s += stop.enabled ? QStringLiteral("1|") : QStringLiteral("0|");
        // .NET float.ToString("0.000") always emits a "." separator under
        // en-US (the typical Thetis runtime culture). QString::number with
        // 'f' format and 3 decimals matches that exactly.
        s += QString::number(stop.percent, 'f', 3);
        s += QLatin1Char('|');
        s += QString::number(static_cast<qint32>(colorToSignedArgb(stop.color)));
        s += QLatin1Char('|');
    }
    return s;
}

// From Thetis ucLGPicker.cs:684-738 [v2.10.3.13+501e3f51] (Text setter):
//
//   string[] parts = value.Split('|');
//   bool bOk = false;
//   Dictionary<int, GradColours> lstTmp = new Dictionary<int, GradColours>();
//   int tot;
//   bOk = int.TryParse(parts[0], out tot);
//   if (!bOk) return;
//   bOk = parts.Length == (tot * 3) + 2;
//   if (bOk) {
//       bOk = false;
//       for (int i = 0; i < tot; i++) {
//           ...parse enabled / percent / argb...
//           if (bOk) addColour(i, percent, Color.FromArgb(col), lstTmp, enabled);
//           if (!bOk) break;
//       }
//   }
//   if (bOk) {
//       lock { m_dictColours.Clear(); foreach kvp lstTmp m_dictColours.Add(kvp); }
//       rebuildSortedColours(); HighlightFirstGripper(); Invalidate();
//       OnChanged(EventArgs.Empty);
//   }
//
// Length check: count token + 3 tokens per entry + trailing empty (because
// a string like "9|...|" splits into 9*3 + 1 + 1 = 29 tokens, so
// `parts.Length == (tot * 3) + 2`). On any parse failure m_dictColours is
// not mutated and OnChanged does not fire.
void GradientPickerWidget::setText(const QString& s)
{
    if (s.isEmpty()) {
        return;
    }

    const QStringList parts = s.split(QLatin1Char('|'));
    if (parts.isEmpty()) {
        return;
    }

    bool ok = false;
    const int tot = parts.first().toInt(&ok);
    if (!ok) {
        return;
    }

    if (parts.size() != (tot * 3) + 2) {
        return;
    }

    QVector<GradientStop> tmp;
    tmp.reserve(tot);

    bool entriesOk = true;
    for (int i = 0; i < tot; ++i) {
        const int entry = 1 + (i * 3);

        // From Thetis ucLGPicker.cs:708-710 [v2.10.3.13+501e3f51]:
        //   if (parts[entry] == "1") enabled = true;
        //   bOk = float.TryParse(parts[entry + 1], out percent);
        //   if (bOk) bOk = int.TryParse(parts[entry + 2], out col);
        const bool enabled = (parts[entry] == QStringLiteral("1"));
        bool percentOk = false;
        const float percent = parts[entry + 1].toFloat(&percentOk);
        if (!percentOk) {
            entriesOk = false;
            break;
        }
        bool argbOk = false;
        const qint32 argb = parts[entry + 2].toInt(&argbOk);
        if (!argbOk) {
            entriesOk = false;
            break;
        }

        GradientStop stop;
        stop.percent = percent;
        stop.color   = signedArgbToColor(argb);
        stop.enabled = enabled;
        tmp.append(stop);
    }

    if (!entriesOk) {
        return;
    }

    // Commit. From Thetis ucLGPicker.cs:721-737 [v2.10.3.13+501e3f51]:
    //   m_dictColours.Clear(); foreach { Add }; rebuildSortedColours();
    //   HighlightFirstGripper(); Invalidate(); OnChanged(EventArgs.Empty);
    m_stops = tmp;
    m_selectedIndex = -1;
    m_highlightedIndex = -1;
    rebuildSortedStops();
    highlightFirstStop();
    update();
    emit changed();
}

// From Thetis ucLGPicker.cs:912-930 [v2.10.3.13+501e3f51] (EncodedText):
//
//   get {
//       try { return Convert.ToBase64String(Encoding.UTF8.GetBytes(this.Text)); }
//       catch { return ""; }
//   }
//   set {
//       try { this.Text = Encoding.UTF8.GetString(Convert.FromBase64String(value)); }
//       catch { }
//   }
//
// Both wrapped in try/catch — failure on the getter returns empty string,
// failure on the setter is a silent no-op. NereusSDR uses Qt's
// QByteArray::toBase64 / fromBase64 (which return empty on malformed
// input rather than throwing) to give equivalent silent-failure behaviour.
QString GradientPickerWidget::encodedText() const
{
    return QString::fromLatin1(text().toUtf8().toBase64());
}

void GradientPickerWidget::setEncodedText(const QString& s)
{
    const QByteArray decoded = QByteArray::fromBase64(s.toUtf8());
    if (decoded.isEmpty() && !s.isEmpty()) {
        // Decode failed (malformed base64) — silent no-op per Thetis.
        return;
    }
    setText(QString::fromUtf8(decoded));
}

// ---- Color sampling ----

// From Thetis ucLGPicker.cs:770-792 [v2.10.3.13+501e3f51]:
//
//   Color c = Color.Empty;
//   int iL = indexAtPerc(perc);
//   if (iL == -1) iL = indexOfClosestToLeft(perc);
//   int iR = indexOfClosestToRight(perc);
//   if (iR == -1) iR = iL; // if none to the right, the L and R the same
//   if (iL != -1 && iR != -1) {
//       float percWidth = m_dictColours[iR].percent - m_dictColours[iL].percent;
//       float scale = percWidth == 0 ? 0 : 1 / percWidth;
//       float offset = perc - m_dictColours[iL].percent;
//       perc = (offset * scale);
//       c = ColorInterpolator.InterpolateBetween(m_dictColours[iL].color,
//                                                 m_dictColours[iR].color, perc);
//   }
//   return c;
//
// Edge clamp: when `perc` is past the rightmost enabled stop,
// indexOfClosestToRight returns -1, the iR == iL fallback kicks in,
// percWidth = 0, scale = 0, perc → 0, InterpolateBetween returns the
// left endpoint. Symmetric on the left edge: indexAtPerc fails (no
// exact match), indexOfClosestToLeft returns -1 if perc is below all
// enabled stops, so iL == -1 — we fall through to "return Color.Empty".
// To get cleaner endpoint-clamp behaviour at percentages below the
// leftmost enabled stop, we additionally fall back to "first enabled" /
// "last enabled" so colorAtPercent(-0.5) returns the first stop's color.
// This matches the test_color_at_percent_clamps_to_endpoints expectation
// and is consistent with the visible behaviour of the Thetis widget at
// runtime (LOW/HIGH bounds enforced by the strip's coordinate space).
QColor GradientPickerWidget::colorAtPercent(float perc) const
{
    int iL = indexAtPercent(perc);
    if (iL == -1) {
        iL = indexClosestLeft(perc);
    }
    int iR = indexClosestRight(perc);
    if (iR == -1) {
        iR = iL;
    }

    if (iL == -1 && iR == -1) {
        // Off the left edge: clamp to first enabled stop.
        if (!m_sortedEnabledIndices.isEmpty()) {
            return m_stops[m_sortedEnabledIndices.first()].color;
        }
        return QColor();
    }
    if (iL == -1) {
        // Defensive: only the right exists. Clamp to it.
        return m_stops[iR].color;
    }

    const float percWidth = m_stops[iR].percent - m_stops[iL].percent;
    const float scale = percWidth == 0.0f ? 0.0f : 1.0f / percWidth;
    const float offset = perc - m_stops[iL].percent;
    const float lambda = offset * scale;

    return interpolateBetween(m_stops[iL].color, m_stops[iR].color,
                              static_cast<double>(lambda));
}

// From Thetis ucLGPicker.cs:764-769 [v2.10.3.13+501e3f51]:
//   public Color GetColourForDBM(float dbm) {
//       float f = GetPercForDBM(dbm);
//       return GetColourAtPercent(f);
//   }
QColor GradientPickerWidget::colorForDbm(float dbm) const
{
    // From Thetis ucLGPicker.cs:793-803 [v2.10.3.13+501e3f51]:
    //   float max = SPAN; dbm += LOW;
    //   float perc = dbm / max;
    //   if (perc < 0) perc = 0;
    //   if (perc > 1) perc = 1;
    const float maxSpan = static_cast<float>(kSpanDbm);
    const float shifted = dbm - static_cast<float>(kLowDbm);
    float perc = shifted / maxSpan;
    if (perc < 0.0f) {
        perc = 0.0f;
    }
    if (perc > 1.0f) {
        perc = 1.0f;
    }
    return colorAtPercent(perc);
}

// From Thetis setup.cs:33304-33312 [v2.10.3.13+501e3f51] (101-LUT consumer):
//   Color[] waterfall_grad = new Color[101];
//   for (int p = 0; p <= 100; p++)
//       waterfall_grad[p] = lgLinearGradient_waterfall.GetColourAtPercent(p / 100f);
QVector<QColor> GradientPickerWidget::colorTable(int count) const
{
    QVector<QColor> lut;
    if (count <= 0) {
        return lut;
    }
    lut.reserve(count);
    if (count == 1) {
        lut.append(colorAtPercent(0.0f));
        return lut;
    }
    const float denom = static_cast<float>(count - 1);
    for (int k = 0; k < count; ++k) {
        const float p = static_cast<float>(k) / denom;
        lut.append(colorAtPercent(p));
    }
    return lut;
}

// ---- Mutation ----

// From Thetis ucLGPicker.cs:599-623 [v2.10.3.13+501e3f51] (addGripper):
//
//   int index = findFreeDisabledGripper();
//   if (index == -1) return -1;
//   GradColours gc = m_dictColours[index];
//   gc.color = colour; gc.percent = perc; gc.enabled = true; gc.highlighted = false;
//   m_dictColours[index] = gc;
//   rebuildSortedColours();
//   if (bRefresh) Invalidate();
//   OnChanged(EventArgs.Empty);
//   return index;
//
// Per the test contract (case 7) the new stop's color matches
// colorAtPercent(perc) at insert time — Thetis's mouse-down handler
// (ucLGPicker.cs:415) computes that color and passes it in. We sample
// once here so addStop() is self-contained.
int GradientPickerWidget::addStop(float perc)
{
    const int slot = findFreeDisabledSlot();
    if (slot == -1) {
        return -1;
    }
    const QColor sampled = colorAtPercent(perc);

    m_stops[slot].percent = perc;
    m_stops[slot].color   = sampled;
    m_stops[slot].enabled = true;
    rebuildSortedStops();
    update();
    emit changed();
    return slot;
}

// From Thetis ucLGPicker.cs:582-597 [v2.10.3.13+501e3f51]
// (RemoveSelectedGripper):
//
//   if (m_nSelectedGripper == -1) return;
//   if (m_nSelectedGripper == 0 || m_nSelectedGripper == m_dictColours.Count - 1) return;
//   enableGripper(m_nSelectedGripper, false);
//   m_nSelectedGripper = -1;
//   rebuildSortedColours();
//   if (bRefresh) Invalidate();
//   OnChanged(EventArgs.Empty);
//
// End-cap protection: index 0 (leftmost) and index Count-1 (rightmost,
// which is the white endpoint added at index kDefaultStopCount = 8 on
// construction) cannot be removed. With kDefaultStopCount == 8 and the
// disable loop leaving only those 2 enabled, this guarantees a minimum
// of 2 enabled stops at all times.
bool GradientPickerWidget::removeSelectedStop()
{
    if (m_selectedIndex == -1) {
        return false;
    }
    if (m_selectedIndex == 0 || m_selectedIndex == m_stops.size() - 1) {
        return false;
    }
    m_stops[m_selectedIndex].enabled = false;
    m_selectedIndex = -1;
    m_highlightedIndex = -1;
    rebuildSortedStops();
    update();
    emit changed();
    return true;
}

// From Thetis ucLGPicker.cs:639-652 [v2.10.3.13+501e3f51]:
//   for (int i = 0; i < m_dictColours.Count; i++) {
//       if (m_dictColours[i].enabled) {
//           highlightGripper(i, true);
//           m_nSelectedGripper = i;
//           Invalidate();
//           OnGripperSelected(m_dictColours[i].color);
//           break;
//       }
//   }
void GradientPickerWidget::highlightFirstStop()
{
    for (int i = 0; i < m_stops.size(); ++i) {
        if (m_stops[i].enabled) {
            m_highlightedIndex = i;
            m_selectedIndex = i;
            update();
            emit stopSelected(m_stops[i].color);
            return;
        }
    }
}

QVector<GradientStop> GradientPickerWidget::gradientStops() const
{
    return m_stops;
}

#ifdef NEREUS_BUILD_TESTS
QVector<GradientStop> GradientPickerWidget::stopsForTest() const
{
    return m_stops;
}

void GradientPickerWidget::setStopsForTest(const QVector<GradientStop>& stops)
{
    m_stops = stops;
    rebuildSortedStops();
    update();
}

void GradientPickerWidget::setSelectedStopForTest(int index)
{
    m_selectedIndex = index;
}

int GradientPickerWidget::selectedStopForTest() const
{
    return m_selectedIndex;
}
#endif

// ---- Helpers ----

// From Thetis ucLGPicker.cs:625-637 [v2.10.3.13+501e3f51]:
//   for (int i = 1; i < m_dictColours.Count - 1; i++) {
//       if (!m_dictColours[i].enabled) { return i; }
//   }
//   return -1;
int GradientPickerWidget::findFreeDisabledSlot() const
{
    for (int i = 1; i < m_stops.size() - 1; ++i) {
        if (!m_stops[i].enabled) {
            return i;
        }
    }
    return -1;
}

// From Thetis ucLGPicker.cs:499-512 [v2.10.3.13+501e3f51] (indexAtPerc):
//   for (int i = 0; ...) {
//       if (m_dictColours[i].enabled && m_dictColours[i].percent == perc) return i;
//   }
//   return -1;
int GradientPickerWidget::indexAtPercent(float perc) const
{
    for (int i = 0; i < m_stops.size(); ++i) {
        if (m_stops[i].enabled && m_stops[i].percent == perc) {
            return i;
        }
    }
    return -1;
}

// From Thetis ucLGPicker.cs:469-483 [v2.10.3.13+501e3f51]:
//   closestIndex = -1; closestDiff = MaxValue;
//   for (i 0..Count-1) {
//       if (enabled && perc - perc[i] > 0 && (perc - perc[i]) < closestDiff) {
//           closestDiff = perc - perc[i]; closestIndex = i;
//       }
//   }
//   return closestIndex;
int GradientPickerWidget::indexClosestLeft(float perc) const
{
    int closestIndex = -1;
    float closestDiff = std::numeric_limits<float>::max();
    for (int i = 0; i < m_stops.size(); ++i) {
        if (m_stops[i].enabled && m_stops[i].percent < perc) {
            const float diff = perc - m_stops[i].percent;
            if (diff < closestDiff) {
                closestDiff = diff;
                closestIndex = i;
            }
        }
    }
    return closestIndex;
}

// From Thetis ucLGPicker.cs:484-498 [v2.10.3.13+501e3f51] — overload that
// takes an existing dictionary index instead of a free percent.
int GradientPickerWidget::indexClosestLeft(int index) const
{
    if (index < 0 || index >= m_stops.size()) {
        return -1;
    }
    return indexClosestLeft(m_stops[index].percent);
}

// From Thetis ucLGPicker.cs:513-526 [v2.10.3.13+501e3f51]:
int GradientPickerWidget::indexClosestRight(float perc) const
{
    int closestIndex = -1;
    float closestDiff = std::numeric_limits<float>::max();
    for (int i = 0; i < m_stops.size(); ++i) {
        if (m_stops[i].enabled && m_stops[i].percent > perc) {
            const float diff = m_stops[i].percent - perc;
            if (diff < closestDiff) {
                closestDiff = diff;
                closestIndex = i;
            }
        }
    }
    return closestIndex;
}

// From Thetis ucLGPicker.cs:528-542 [v2.10.3.13+501e3f51] — overload.
int GradientPickerWidget::indexClosestRight(int index) const
{
    if (index < 0 || index >= m_stops.size()) {
        return -1;
    }
    return indexClosestRight(m_stops[index].percent);
}

// From Thetis ucLGPicker.cs:135-148 [v2.10.3.13+501e3f51]:
//   list = m_dictColours.ToList();
//   m_kvpSortedColours = from pair in list where pair.Value.enabled
//                        orderby pair.Value.percent ascending select pair;
void GradientPickerWidget::rebuildSortedStops()
{
    m_sortedEnabledIndices.clear();
    for (int i = 0; i < m_stops.size(); ++i) {
        if (m_stops[i].enabled) {
            m_sortedEnabledIndices.append(i);
        }
    }
    std::sort(m_sortedEnabledIndices.begin(), m_sortedEnabledIndices.end(),
              [this](int a, int b) {
                  return m_stops[a].percent < m_stops[b].percent;
              });
}

void GradientPickerWidget::emitChangedAndUpdate()
{
    update();
    emit changed();
}

// ---- Geometry helpers (Step 2.8 paint) ----

// From Thetis ucLGPicker.cs:175-178 [v2.10.3.13+501e3f51]:
//   private int actualWidth { get { return this.Width - m_nPadding * 2; } }
int GradientPickerWidget::actualWidth() const
{
    return std::max(0, width() - kPaddingPx * 2);
}

// From Thetis ucLGPicker.cs:378-386 [v2.10.3.13+501e3f51] (percFromPixels):
//   float percPos = (X - m_nPadding) / (float)actualWidth;
//   if (percPos < 0) percPos = 0;
//   if (percPos > 1) percPos = 1;
float GradientPickerWidget::percFromPixel(int x) const
{
    const int aw = actualWidth();
    if (aw == 0) {
        return 0.0f;
    }
    float perc = static_cast<float>(x - kPaddingPx) / static_cast<float>(aw);
    if (perc < 0.0f) {
        perc = 0.0f;
    }
    if (perc > 1.0f) {
        perc = 1.0f;
    }
    return perc;
}

int GradientPickerWidget::percentToPixel(float perc) const
{
    return kPaddingPx + static_cast<int>(static_cast<float>(actualWidth()) * perc);
}

// From Thetis ucLGPicker.cs:332 [v2.10.3.13+501e3f51]:
//   OnGripperDBMChanged(-LOW + (int)(SPAN * percPos), percPos);
int GradientPickerWidget::pixelToDbm(int x) const
{
    const float perc = percFromPixel(x);
    return percentToDbm(perc);
}

int GradientPickerWidget::percentToDbm(float perc) const
{
    return kLowDbm + static_cast<int>(static_cast<float>(kSpanDbm) * perc);
}

// ---- Event handlers stub (Step 2.8 / 2.9 fills these in) ----

int GradientPickerWidget::findStopIndexAtPixel(int x, int y) const
{
    // From Thetis ucLGPicker.cs:279-294 [v2.10.3.13+501e3f51]
    // (findColourIndexGrip):
    //   for (int i 0..Count-1) {
    //       int posX = m_nPadding + (int)(actualWidth * m_dictColours[i].percent);
    //       if (m_dictColours[i].enabled && X >= posX-6 && X <= posX+6 && Y >= 0 && Y <= 24) {
    //           return i;
    //       }
    //   }
    //   return -1;
    for (int i = 0; i < m_stops.size(); ++i) {
        if (!m_stops[i].enabled) {
            continue;
        }
        const int posX = percentToPixel(m_stops[i].percent);
        if (x >= posX - 6 && x <= posX + 6 && y >= 0 && y <= 24) {
            return i;
        }
    }
    return -1;
}

void GradientPickerWidget::paintEvent(QPaintEvent* /*event*/)                {}
void GradientPickerWidget::mousePressEvent(QMouseEvent* /*event*/)           {}
void GradientPickerWidget::mouseMoveEvent(QMouseEvent* /*event*/)            {}
void GradientPickerWidget::mouseReleaseEvent(QMouseEvent* /*event*/)         {}
void GradientPickerWidget::mouseDoubleClickEvent(QMouseEvent* /*event*/)     {}
void GradientPickerWidget::contextMenuEvent(QContextMenuEvent* /*event*/)    {}
void GradientPickerWidget::leaveEvent(QEvent* /*event*/)                     {}

void GradientPickerWidget::drawScales(QPainter& /*painter*/) const           {}
void GradientPickerWidget::drawTextCentered(QPainter& /*painter*/,
                                             const QString& /*s*/,
                                             int /*x*/) const                {}

} // namespace NereusSDR
