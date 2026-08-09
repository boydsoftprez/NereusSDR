// no-port-check: NereusSDR-original test file. Cites Thetis line numbers as
// verification anchors (we assert that GradientPickerWidget defaults +
// serialization match Thetis ucLGPicker verbatim), not as port-from
// references. No verbatim Thetis source is reproduced here beyond brief
// default-value quotes used as test oracles.
//
// tst_gradient_picker.cpp
//
// Phase 3M-5c: Custom Gradient Picker. Locks in 10 acceptance behaviors
// against Thetis ucLGPicker.cs verbatim values. Tests reference the master
// plan at docs/architecture/tx-display-settings-master-plan.md section
// "Phase 2: Custom Gradient Picker widget".
//
// SHOW (Thetis sources read for this test, all from
// /Users/j.j.boyd/Thetis/Project Files/Source/Console/ucLGPicker.cs
// [v2.10.3.13+501e3f51]):
//
// 1. ucLGPicker.cs:55-130 (class scaffolding, constants, default ctor):
//      private const int LOW = 150; // -150 dbm
//      private const int HIGH = 10; // +10 dbm
//      private const int SPAN = LOW + HIGH; // 160
//      private const int m_nGrippers = 8; // must be at least 2
//
// 2. ucLGPicker.cs:75-80 (private GradColours struct):
//      private struct GradColours {
//          public float percent;
//          public Color color;
//          public bool enabled;
//          public bool highlighted;
//      }
//
// 3. ucLGPicker.cs:98-115 (default ctor 8-stop grayscale ramp):
//      m_dictColours = new Dictionary<int, GradColours>();
//      for (int i = 0; i < m_nGrippers; i++) {
//          addColour(i, i / (float)m_nGrippers,
//                    Color.FromArgb(255, i*(256/m_nGrippers - 1),
//                                        i*(256/m_nGrippers - 1),
//                                        i*(256/m_nGrippers - 1)),
//                    m_dictColours);
//      }
//      addColour(m_nGrippers, 1, Color.FromArgb(255, 255, 255, 255), m_dictColours);
//      for (int i = 1; i < m_nGrippers; i++) {
//          enableGripper(i, false);
//      }
//    Note: 256/8 = 32, 32-1 = 31. So the grayscale value at i is i*31 ⇒
//    {0, 31, 62, 93, 124, 155, 186, 217} for indices 0..7, then index 8
//    is (255,255,255,255). After the disable-loop only index 0 and index 8
//    remain enabled. Total 9 entries, 2 enabled.
//
// 4. ucLGPicker.cs:666-682 (Text getter, raw pipe-delimited format):
//      string sRet = m_dictColours.Count.ToString() + "|";
//      for (int i = 0; i < m_dictColours.Count; i++) {
//          if (m_dictColours[i].enabled) sRet += "1|";
//          else                          sRet += "0|";
//          sRet += m_dictColours[i].percent.ToString("0.000") + "|";
//          sRet += m_dictColours[i].color.ToArgb().ToString() + "|";
//      }
//    ⇒ format: "<count>|<en0>|<perc0>|<argb0>|<en1>|<perc1>|<argb1>|...|"
//    Trailing pipe is present because the loop appends "|" after every
//    field including the last argb. Color.ToArgb() returns a SIGNED int32
//    (e.g. (255,0,0,0) ⇒ 0xFF000000 ⇒ -16777216).
//
// 5. ucLGPicker.cs:684-738 (Text setter, malformed-input no-op):
//      string[] parts = value.Split('|');
//      bool bOk = false;
//      Dictionary<int, GradColours> lstTmp = new Dictionary<int, GradColours>();
//      int tot;
//      bOk = int.TryParse(parts[0], out tot);
//      if (!bOk) return;
//      bOk = parts.Length == (tot * 3) + 2; // count + 3 fields per entry + trailing empty
//      ...
//      if (bOk) { /* commit lstTmp into m_dictColours, OnChanged */ }
//      // else: m_dictColours untouched, no event fired.
//
// 6. ucLGPicker.cs:912-930 (EncodedText: Text wrapped in base64-of-UTF8):
//      get { return Convert.ToBase64String(Encoding.UTF8.GetBytes(this.Text)); }
//      set { this.Text = Encoding.UTF8.GetString(Convert.FromBase64String(value)); }
//    Both wrapped in try/catch returning "" / no-op on failure.
//
// 7. ucLGPicker.cs:770-792 (GetColourAtPercent — clamp / interp):
//      Color c = Color.Empty;
//      int iL = indexAtPerc(perc);
//      if (iL == -1) iL = indexOfClosestToLeft(perc);
//      int iR = indexOfClosestToRight(perc);
//      if (iR == -1) iR = iL; // off-end clamp on the right
//      if (iL != -1 && iR != -1) {
//          float percWidth = m_dictColours[iR].percent - m_dictColours[iL].percent;
//          float scale = percWidth == 0 ? 0 : 1 / percWidth;
//          float offset = perc - m_dictColours[iL].percent;
//          perc = (offset * scale);
//          c = ColorInterpolator.InterpolateBetween(m_dictColours[iL].color,
//                                                   m_dictColours[iR].color, perc);
//      }
//      return c;
//
// 8. ucLGPicker.cs:387-446 (LGPicker_MouseDown add-stop-on-empty-strip):
//      // when click hits empty space, add a new gripper at midway between
//      // closest-left and closest-right enabled, color = GetColourAtPercent(midWay).
//      float midWay = m_dictColours[indexLeft].percent + (percWidth / 2);
//      int newIndex = addGripper(midWay, GetColourAtPercent(midWay), false);
//
// 9. ucLGPicker.cs:582-597 (RemoveSelectedGripper — min 2 enforced):
//      if (m_nSelectedGripper == -1) return;
//      // prevent either end
//      if (m_nSelectedGripper == 0 || m_nSelectedGripper == m_dictColours.Count - 1) return;
//      enableGripper(m_nSelectedGripper, false);
//      // ⇒ end-cap stops can never be removed; min enabled count = 2.
//
// 10. setup.cs:33304-33312 (101-LUT consumer pattern from grad widget):
//       Color[] waterfall_grad = new Color[101];
//       for (int perc = 0; perc <= 100; perc++) {
//           Color c = lgLinearGradient_waterfall.GetColourAtPercent(perc / 100f);
//           waterfall_grad[perc] = c;
//       }
//
// This test file is NereusSDR-original (test harness, not a Thetis port).

#include <QtTest/QtTest>
#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QSignalSpy>
#include <QString>

#include "gui/widgets/GradientPickerWidget.h"
#include "gui/widgets/GradientStop.h"

using namespace NereusSDR;

namespace {

// Build the canonical default-ctor reference Text string from documented
// Thetis ARGB ints. This mirrors the math in ucLGPicker.cs:104-108 step by
// step, then assembles the "<count>|<en>|<perc>|<argb>|..." string per the
// Text getter at ucLGPicker.cs:666-682. Used as the byte-for-byte oracle in
// text_format_matches_thetis_byte_for_byte.
//
// Default ctor produces 9 entries:
//   i=0..7: percent = i/8, color = (255, i*31, i*31, i*31)
//   i=8:    percent = 1.0, color = (255,255,255,255)
// After enableGripper(i, false) for i in 1..7, only index 0 and index 8 are
// enabled. Color.ToArgb() returns a signed int32, sign bit set when alpha
// = 255 (top byte 0xFF).
QString thetisDefaultText()
{
    auto argbToSignedInt = [](int a, int r, int g, int b) -> qint32 {
        const quint32 u = (static_cast<quint32>(a) << 24)
                        | (static_cast<quint32>(r) << 16)
                        | (static_cast<quint32>(g) << 8)
                        |  static_cast<quint32>(b);
        return static_cast<qint32>(u);
    };

    QString s;
    s += QStringLiteral("9|");

    // Indices 0..7: enabled iff i==0; grayscale ramp value = i * 31.
    for (int i = 0; i < 8; ++i) {
        const bool en = (i == 0);
        const float perc = static_cast<float>(i) / 8.0f;
        const int gray = i * 31;
        const qint32 argb = argbToSignedInt(255, gray, gray, gray);
        s += en ? QStringLiteral("1|") : QStringLiteral("0|");
        s += QString::number(perc, 'f', 3) + QStringLiteral("|");
        s += QString::number(argb) + QStringLiteral("|");
    }
    // Index 8: percent=1.0, color=white, enabled.
    {
        const qint32 argb = argbToSignedInt(255, 255, 255, 255);
        s += QStringLiteral("1|");
        s += QString::number(1.0f, 'f', 3) + QStringLiteral("|");
        s += QString::number(argb) + QStringLiteral("|");
    }
    return s;
}

} // namespace

class TestGradientPicker : public QObject
{
    Q_OBJECT

private slots:

    // 1. From Thetis ucLGPicker.cs:98-115 [v2.10.3.13+501e3f51]:
    //    9 entries (8 grippers indices 0..7 + final addColour at index 8 with
    //    white per :108). Two enabled (indices 0 and 8). Grayscale at i = i*31
    //    (because 256/8 - 1 = 31).
    void default_constructor_has_8_grayscale_stops()
    {
        GradientPickerWidget w;
        const auto stops = w.stopsForTest();

        QCOMPARE(stops.size(), 9);

        // Indices 0..7: percent = i/8, RGB = (i*31, i*31, i*31), alpha = 255.
        for (int i = 0; i < 8; ++i) {
            const int gray = i * 31;
            QCOMPARE(stops[i].percent, static_cast<float>(i) / 8.0f);
            QCOMPARE(stops[i].color, QColor(gray, gray, gray, 255));
        }

        // Index 8: percent = 1, color = white.
        QCOMPARE(stops[8].percent, 1.0f);
        QCOMPARE(stops[8].color, QColor(255, 255, 255, 255));

        // Only index 0 and index 8 enabled (the post-construction disable loop
        // at ucLGPicker.cs:110-113 disables indices 1..7).
        int enabled = 0;
        for (const GradientStop& stop : stops) {
            if (stop.enabled) {
                ++enabled;
            }
        }
        QCOMPARE(enabled, 2);
        QVERIFY(stops[0].enabled);
        QVERIFY(stops[8].enabled);
    }

    // 2. From Thetis ucLGPicker.cs:666-682 [v2.10.3.13+501e3f51]:
    //    Text getter format is "<count>|<en>|<perc>|<argb>|..." with trailing
    //    pipe. percent is "0.000" (3 decimals), argb is signed int32 decimal.
    void text_format_matches_thetis_byte_for_byte()
    {
        GradientPickerWidget w;
        const QString actual = w.text();
        const QString expected = thetisDefaultText();
        QCOMPARE(actual, expected);
    }

    // 3. From Thetis ucLGPicker.cs:684-738 [v2.10.3.13+501e3f51]:
    //    Text setter parses "<count>|<en>|<perc>|<argb>|..." and replaces
    //    m_dictColours. setText(text()) is the identity.
    void text_round_trip_via_setText()
    {
        GradientPickerWidget w;
        const QString original = w.text();
        // Mutate the widget (enable an interior stop) so we know the round trip
        // doesn't just compare two default-constructed states. Guard against
        // the TDD-red stub that returns an empty stops vector (test would
        // otherwise segfault on out-of-bounds index access).
        QVector<GradientStop> mutated = w.stopsForTest();
        QVERIFY2(mutated.size() >= 4,
                 "default ctor must produce at least 4 stops (Thetis spec: 9)");
        mutated[3].enabled = true;
        mutated[3].color = QColor(11, 22, 33, 255);
        w.setStopsForTest(mutated);

        const QString afterMutation = w.text();
        QVERIFY(afterMutation != original);

        GradientPickerWidget w2;
        w2.setText(afterMutation);
        QCOMPARE(w2.text(), afterMutation);
    }

    // 4. From Thetis ucLGPicker.cs:912-930 [v2.10.3.13+501e3f51]:
    //    EncodedText is base64-of-UTF8(Text). setEncodedText(encodedText())
    //    is the identity.
    void encoded_text_round_trip()
    {
        GradientPickerWidget w;
        const QString encoded = w.encodedText();

        // Sanity: encoded form is base64 of the same Text string.
        const QByteArray decoded = QByteArray::fromBase64(encoded.toUtf8());
        QCOMPARE(QString::fromUtf8(decoded), w.text());

        GradientPickerWidget w2;
        // First diverge w2 so the assertion proves the setter did something.
        // Guard against the TDD-red stub returning an empty stops vector.
        QVector<GradientStop> mutated = w2.stopsForTest();
        QVERIFY2(mutated.size() >= 2,
                 "default ctor must produce at least 2 stops (Thetis spec: 9)");
        mutated[1].enabled = true;
        w2.setStopsForTest(mutated);
        QVERIFY(w2.encodedText() != encoded);

        w2.setEncodedText(encoded);
        QCOMPARE(w2.encodedText(), encoded);
    }

    // 5. From Thetis ucLGPicker.cs:684-738 [v2.10.3.13+501e3f51]:
    //    Text setter pre-checks bOk = int.TryParse(parts[0], out tot); on
    //    failure it returns. Empty input has no "<count>" and is rejected.
    void empty_string_to_setText_is_noop()
    {
        GradientPickerWidget w;
        // Pre-condition: default ctor populates 9 stops. Any test that
        // probes setText behaviour needs the widget to start in the real
        // default state, otherwise an "all-empty" stub would trivially
        // satisfy "before == after" without exercising the parser.
        QVERIFY2(w.stopsForTest().size() == 9,
                 "default ctor must populate 9 stops (Thetis spec at "
                 "ucLGPicker.cs:104-108)");
        const QString before = w.text();
        QSignalSpy spy(&w, &GradientPickerWidget::changed);

        w.setText(QString());

        QCOMPARE(w.text(), before);
        QCOMPARE(spy.count(), 0);
    }

    // 6. From Thetis ucLGPicker.cs:684-738 [v2.10.3.13+501e3f51]:
    //    Defensive parser: malformed input ⇒ m_dictColours untouched, no
    //    OnChanged. Cases: bad count, length mismatch, non-numeric percent.
    void malformed_text_to_setText_is_noop()
    {
        GradientPickerWidget w;
        // Pre-condition: default ctor populates 9 stops; otherwise an
        // all-empty stub would trivially pass without exercising the parser.
        QVERIFY2(w.stopsForTest().size() == 9,
                 "default ctor must populate 9 stops (Thetis spec at "
                 "ucLGPicker.cs:104-108)");
        const QString before = w.text();

        // Case 1: leading non-numeric token.
        {
            QSignalSpy spy(&w, &GradientPickerWidget::changed);
            w.setText(QStringLiteral("not-a-number|1|0.000|-16777216|"));
            QCOMPARE(w.text(), before);
            QCOMPARE(spy.count(), 0);
        }

        // Case 2: count says 9, but body has only 1 entry's worth of fields.
        // bOk = parts.Length == (tot * 3) + 2 fails, lstTmp not committed.
        {
            QSignalSpy spy(&w, &GradientPickerWidget::changed);
            w.setText(QStringLiteral("9|1|0.000|-16777216|"));
            QCOMPARE(w.text(), before);
            QCOMPARE(spy.count(), 0);
        }

        // Case 3: count is 1, length matches (1*3 + 2 = 5 parts), but the
        // percent field is non-numeric.
        {
            QSignalSpy spy(&w, &GradientPickerWidget::changed);
            w.setText(QStringLiteral("1|1|nope|-16777216|"));
            QCOMPARE(w.text(), before);
            QCOMPARE(spy.count(), 0);
        }
    }

    // 7. From Thetis ucLGPicker.cs:387-446 [v2.10.3.13+501e3f51]:
    //    Click on empty strip triggers addGripper(midWay, GetColourAtPercent(midWay)).
    //    The new stop's color equals GetColourAtPercent at the new percent
    //    AT THE TIME OF THE INSERT (i.e. before the insert mutates the
    //    interpolation table).
    void add_stop_at_percent_inserts_with_interpolated_color()
    {
        GradientPickerWidget w;
        const QColor expected = w.colorAtPercent(0.5f);

        const int idx = w.addStop(0.5f);
        QVERIFY2(idx >= 0, "addStop(0.5) must succeed (Thetis spec: free disabled slot exists)");

        const auto stops = w.stopsForTest();
        QVERIFY2(idx < stops.size(),
                 "addStop must return an index inside the stops vector");
        QVERIFY(stops[idx].enabled);
        QCOMPARE(stops[idx].percent, 0.5f);
        QCOMPARE(stops[idx].color, expected);
    }

    // 8. From Thetis ucLGPicker.cs:582-597 [v2.10.3.13+501e3f51]:
    //    RemoveSelectedGripper rejects index 0 / Count - 1 (end-caps).
    //    Together with the dictionary's fixed total entry count and the
    //    addGripper-into-disabled-slot mechanic, this gives min enabled = 2.
    void remove_stop_enforces_min_2()
    {
        GradientPickerWidget w;

        // Default: 2 enabled (indices 0 + 8). Removing the only removable
        // (interior) stop fails because there are no interior enabled stops.
        // First add an interior stop, then prove we can remove it; then prove
        // we CANNOT remove the end-caps no matter what we try.
        const int newIdx = w.addStop(0.5f);
        QVERIFY2(newIdx > 0, "addStop must place new stop inside (interior, not end-cap)");

        // Select the interior stop and remove it ⇒ back to 2 enabled.
        w.setSelectedStopForTest(newIdx);
        QVERIFY(w.removeSelectedStop());

        int enabled = 0;
        for (const GradientStop& stop : w.stopsForTest()) {
            if (stop.enabled) {
                ++enabled;
            }
        }
        QCOMPARE(enabled, 2);

        // Try to remove the leftmost (index 0) ⇒ rejected.
        w.setSelectedStopForTest(0);
        QVERIFY(!w.removeSelectedStop());

        // Try to remove the rightmost (last index) ⇒ rejected.
        const int storedSize = w.stopsForTest().size();
        QVERIFY2(storedSize >= 2,
                 "default ctor must produce at least 2 stops (Thetis spec: 9)");
        w.setSelectedStopForTest(storedSize - 1);
        QVERIFY(!w.removeSelectedStop());

        // Still 2 enabled.
        enabled = 0;
        for (const GradientStop& stop : w.stopsForTest()) {
            if (stop.enabled) {
                ++enabled;
            }
        }
        QCOMPARE(enabled, 2);
    }

    // 9. From Thetis ucLGPicker.cs:770-792 [v2.10.3.13+501e3f51]:
    //    GetColourAtPercent: if iR == -1 (right of last enabled), iR = iL ⇒
    //    percWidth = 0 ⇒ scale = 0 ⇒ perc = 0 ⇒ InterpolateBetween returns the
    //    left endpoint color. Symmetric behaviour at the left edge falls out
    //    of the indexAtPerc / indexOfClosestToLeft logic.
    void color_at_percent_clamps_to_endpoints()
    {
        GradientPickerWidget w;

        // Valid-color check first so an unimplemented stub returning
        // QColor() (invalid) fails cleanly. QColor() reports name == "#000000"
        // which would compare-equal to QColor(0,0,0,255) under QCOMPARE
        // because both stringify to the same hex. The isValid guard avoids
        // that false positive in the TDD red state.
        const QColor c0  = w.colorAtPercent(0.0f);
        const QColor c1  = w.colorAtPercent(1.0f);
        const QColor cLo = w.colorAtPercent(-0.5f);
        const QColor cHi = w.colorAtPercent(1.5f);
        QVERIFY2(c0.isValid(),  "colorAtPercent(0.0) must return a valid QColor");
        QVERIFY2(c1.isValid(),  "colorAtPercent(1.0) must return a valid QColor");
        QVERIFY2(cLo.isValid(), "colorAtPercent(-0.5) must return a valid QColor");
        QVERIFY2(cHi.isValid(), "colorAtPercent(1.5) must return a valid QColor");

        // First enabled stop has color black at percent 0.0; last enabled stop
        // has color white at percent 1.0.
        QCOMPARE(c0, QColor(0, 0, 0, 255));
        QCOMPARE(c1, QColor(255, 255, 255, 255));

        // Beyond the [0, 1] domain, clamp to the corresponding endpoint.
        QCOMPARE(cLo, QColor(0, 0, 0, 255));
        QCOMPARE(cHi, QColor(255, 255, 255, 255));
    }

    // 10. From Thetis setup.cs:33304-33312 [v2.10.3.13+501e3f51]:
    //     The 101-entry waterfall LUT is built by sampling
    //     GetColourAtPercent(perc / 100f) for perc in [0..100]. NereusSDR's
    //     colorTable() must produce the same sequence.
    void color_table_101_matches_thetis_LUT_build()
    {
        GradientPickerWidget w;
        const auto lut = w.colorTable(101);
        QCOMPARE(lut.size(), 101);

        for (int k = 0; k <= 100; ++k) {
            const QColor expected = w.colorAtPercent(static_cast<float>(k) / 100.0f);
            QCOMPARE(lut[k], expected);
        }
    }
};

// QApplication is required for GradientPickerWidget (a QWidget subclass).
QTEST_MAIN(TestGradientPicker)
#include "tst_gradient_picker.moc"
