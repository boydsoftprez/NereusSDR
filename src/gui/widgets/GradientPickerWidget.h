// =================================================================
// src/gui/widgets/GradientPickerWidget.h  (NereusSDR)
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

#pragma once

#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>

#include "gui/widgets/GradientStop.h"

class QMouseEvent;
class QPaintEvent;
class QContextMenuEvent;

namespace NereusSDR {

/// Multi-stop linear gradient editor widget. C++20 / Qt6 port of Thetis
/// ucLGPicker.cs [v2.10.3.13+501e3f51].
///
/// The widget shows a horizontal gradient strip with draggable stop markers.
/// Click on the strip body adds a new stop at the midway point of the
/// surrounding pair (matching Thetis ucLGPicker.cs:387-446). Right-click on a
/// stop removes it (matching ucLGPicker.cs:582-597, gated by the end-cap
/// rule that prevents removing index 0 or index Count-1, giving an effective
/// minimum of 2 enabled stops). Double-click on a stop opens QColorDialog.
///
/// dBm scale runs from -150 (LOW) to +10 (HIGH) by default per
/// ucLGPicker.cs:57-59. Display can switch to a percent (0-100%) scale via
/// setShowAsPercent().
///
/// Encoded Text / EncodedText format is byte-for-byte compatible with
/// Thetis (pipe-delimited Text per ucLGPicker.cs:666-738; base64 wrapper
/// per ucLGPicker.cs:912-930) so existing Thetis skins can round-trip
/// through NereusSDR (Phase 3H skin import).
class GradientPickerWidget : public QWidget
{
    Q_OBJECT

public:
    // From Thetis ucLGPicker.cs:57-59 [v2.10.3.13+501e3f51]:
    //   private const int LOW = 150; // -150 dbm
    //   private const int HIGH = 10; // +10 dbm
    //   private const int SPAN = LOW + HIGH;
    static constexpr int kLowDbm  = -150;
    static constexpr int kHighDbm =   10;
    static constexpr int kSpanDbm = kHighDbm - kLowDbm;  // 160

    // From Thetis ucLGPicker.cs:62 [v2.10.3.13+501e3f51]:
    //   private const int m_nGrippers = 8; // must be at least 2
    static constexpr int kDefaultStopCount = 8;

    // From Thetis ucLGPicker.cs:61 [v2.10.3.13+501e3f51]:
    //   private const int m_nPadding = 16; // gaps around left/right side
    static constexpr int kPaddingPx = 16;

    explicit GradientPickerWidget(QWidget* parent = nullptr);
    ~GradientPickerWidget() override;

    // ---- Properties (mirror Thetis Low / High / ShowAsPercent /
    //      IncludeAlphaInPreview / ColourForSelectedGripper at
    //      ucLGPicker.cs:118-134, 552-565, 881-890) ----

    int  low()  const noexcept { return kLowDbm;  }
    int  high() const noexcept { return kHighDbm; }

    bool showAsPercent() const noexcept { return m_showAsPercent; }
    void setShowAsPercent(bool on);

    bool includeAlphaInPreview() const noexcept { return m_includeAlphaInPreview; }
    void setIncludeAlphaInPreview(bool on);

    QColor colorForSelectedStop() const;
    void   setColorForSelectedStop(const QColor& c);

    // ---- Serialization (byte-for-byte Thetis compatible) ----
    //
    // From Thetis ucLGPicker.cs:666-682 [v2.10.3.13+501e3f51] (Text getter):
    //   "<count>|<en0>|<perc0>|<argb0>|<en1>|<perc1>|<argb1>|...|"
    // From Thetis ucLGPicker.cs:684-738 [v2.10.3.13+501e3f51] (Text setter):
    //   Defensive parser. Malformed input is a no-op; m_dictColours is
    //   left untouched and no Changed event fires.
    QString text() const;
    void    setText(const QString& s);

    // From Thetis ucLGPicker.cs:912-930 [v2.10.3.13+501e3f51] (EncodedText):
    //   base64-of-UTF8(Text). try/catch around both directions; failure on
    //   getter returns empty string, failure on setter is a silent no-op.
    QString encodedText() const;
    void    setEncodedText(const QString& s);

    // ---- Color sampling ----
    //
    // From Thetis ucLGPicker.cs:770-792 [v2.10.3.13+501e3f51]
    // (GetColourAtPercent): linear interpolate ARGB component-wise between
    // the two adjacent enabled stops. Beyond the last enabled stop, returns
    // the endpoint color (right-clamp). Symmetric clamp at the left edge.
    QColor colorAtPercent(float perc) const;

    // From Thetis ucLGPicker.cs:764-769 [v2.10.3.13+501e3f51] (GetColourForDBM):
    //   returns colorAtPercent(percForDbm(dBm)).
    QColor colorForDbm(float dbm) const;

    // From Thetis setup.cs:33304-33312 [v2.10.3.13+501e3f51] (101-LUT consumer):
    //   for (int p = 0; p <= 100; p++)
    //       grad[p] = lgLinearGradient_waterfall.GetColourAtPercent(p / 100f);
    QVector<QColor> colorTable(int count = 101) const;

    // ---- Mutation ----
    //
    // From Thetis ucLGPicker.cs:599-623 [v2.10.3.13+501e3f51] (addGripper):
    //   finds first disabled slot, enables it at the requested percent /
    //   color. Returns the new index (-1 if no free slot remains).
    int  addStop(float perc);

    // From Thetis ucLGPicker.cs:582-597 [v2.10.3.13+501e3f51]
    // (RemoveSelectedGripper):
    //   no-op if no stop selected, or selected stop is the first / last
    //   index in the dictionary (end-cap protection ⇒ min 2 enabled).
    //   Returns true on successful removal.
    bool removeSelectedStop();

    // From Thetis ucLGPicker.cs:639-652 [v2.10.3.13+501e3f51]
    // (HighlightFirstGripper): selects the first enabled stop.
    void highlightFirstStop();

    // ---- Read-only data accessor ----
    QVector<GradientStop> gradientStops() const;

#ifdef NEREUS_BUILD_TESTS
    // Test seams under the NEREUS_BUILD_TESTS gate. Public API is
    // intentionally narrow; the tests need direct access to the underlying
    // storage so they can verify Thetis-verbatim default state and exercise
    // mutations the GUI normally drives via mouse events.
    QVector<GradientStop> stopsForTest() const;
    void setStopsForTest(const QVector<GradientStop>& stops);
    void setSelectedStopForTest(int index);
    int  selectedStopForTest() const;
#endif

signals:
    // From Thetis ucLGPicker.cs:91 [v2.10.3.13+501e3f51]:
    //   public event EventHandler Changed;
    void changed();

    // From Thetis ucLGPicker.cs:93-96 [v2.10.3.13+501e3f51]:
    //   GripperSelected / GripperDBMChanged / GripperMouseEnter /
    //   GripperMouseLeave event delegates.
    void stopSelected(const QColor& color);
    void stopDbmChanged(int dBm, float percent);
    void stopMouseEnter(int dBm, float percent);
    void stopMouseLeave(int dBm, float percent);

protected:
    // Paint + mouse event overrides (Step 2.8 / 2.9).
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    // ---- Helpers (mirror Thetis private members at ucLGPicker.cs:469-543) ----
    int  findStopIndexAtPixel(int x, int y) const;
    int  indexAtPercent(float perc) const;
    int  indexClosestLeft(float perc) const;
    int  indexClosestLeft(int index) const;
    int  indexClosestRight(float perc) const;
    int  indexClosestRight(int index) const;
    int  findFreeDisabledSlot() const;
    int  actualWidth() const;
    float percFromPixel(int x) const;
    int  percentToPixel(float perc) const;
    int  pixelToDbm(int x) const;
    int  percentToDbm(float perc) const;

    void rebuildSortedStops();
    void emitChangedAndUpdate();
    void drawScales(QPainter& painter) const;
    void drawTextCentered(QPainter& painter, const QString& s, int x) const;

    // ---- Data ----
    //
    // Thetis backs storage with Dictionary<int, GradColours> (sparse, keys
    // = original gripper index). NereusSDR uses a contiguous QVector<>
    // because there are at most 9 entries (kDefaultStopCount + 1 white
    // end-cap). The storage order matches the Thetis dictionary key order
    // so end-cap protection (index 0 / size-1) maps 1:1.
    QVector<GradientStop> m_stops;

    // Sorted-by-percent view of enabled stops, rebuilt on mutation.
    QVector<int> m_sortedEnabledIndices;

    int  m_selectedIndex {-1};
    bool m_dragSelected  {false};
    bool m_changedDuringDrag {false};
    int  m_mouseOverIndex {-1};
    int  m_highlightedIndex {-1};
    bool m_showAsPercent {false};
    bool m_includeAlphaInPreview {false};
};

} // namespace NereusSDR
