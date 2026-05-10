// tst_tx_waterfall_colormap.cpp
//
// Phase 3M-5b — TX Waterfall Colormap. Locks in 4 acceptance behaviors against
// Thetis-verbatim values. Tests reference the master plan at
// docs/architecture/tx-display-settings-master-plan.md §3M-5b.
//
// SHOW (Thetis sources read for this test):
//
// 1. Display.cs:1911-1937 [v2.10.3.13+501e3f51] — TXWFAmpMax / TXWFAmpMin:
//      private static int tx_wf_amp_max = 30;     // TXWFAmpMax default
//      private static int tx_wf_amp_min = -70;    // TXWFAmpMin default
//    These field initializers give the Thetis-verbatim defaults that
//    NereusSDR m_txWfHighLevel (30) and m_txWfLowLevel (-70) must match.
//
// 2. Display.cs:2516-2521 [v2.10.3.13+501e3f51] — waterfall_low_color_tx:
//      private static Color waterfall_low_color_tx = Color.Black;
//      public static Color WaterfallLowColorTX {
//          get { return waterfall_low_color_tx; }
//          set { waterfall_low_color_tx = value; }
//      }
//    Default is Color.Black, which maps to Qt::black / #FF000000.
//
// 3. Display.cs:428-437 [v2.10.3.13+501e3f51] — _tx_color_scheme field:
//      private static ColorScheme _tx_color_scheme = ColorScheme.enhanced;
//      public static ColorScheme TXColorScheme { ... }
//    Default palette is ColorScheme.enhanced, mapping to WfColorScheme::Enhanced.
//    (setup.cs:223 [v2.10.3.13+501e3f51] also confirms: comboColorPalette_tx.Text = "enhanced";)
//
// 4. display.cs:6506-6595 [v2.10.3.13+501e3f51] — per-frame MOX-conditional render path:
//    if (local_mox) {
//        low_threshold  = (float)TXWFAmpMin;    // TX static thresholds
//        high_threshold = (float)TXWFAmpMax;
//        cScheme        = _tx_color_scheme;      // TX palette
//        low_color      = waterfall_low_color_tx;
//    } else {
//        // ... use RX values (waterfall_high_threshold, _rx1_color_scheme, etc.)
//    }
//    No state machine, no save/restore on MOX edge. Per-pixel inline branch.
//    NereusSDR mirrors this in SpectrumWidget::dbmToRgb (m_moxActive flag).
//
// This test file is NereusSDR-original (test harness, not a Thetis port).

#include <QtTest/QtTest>
#include <QApplication>
#include <QColor>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

class TestTxWaterfallColormap : public QObject
{
    Q_OBJECT
private slots:

    // Verifies the four Thetis-verbatim defaults that 3M-5b ports.
    // Sources:
    //   Display.cs:1911-1937 [v2.10.3.13+501e3f51] — TXWFAmpMin / TXWFAmpMax defaults.
    //   Display.cs:428-437   [v2.10.3.13+501e3f51] — _tx_color_scheme default ColorScheme.enhanced.
    //   Display.cs:2516-2521 [v2.10.3.13+501e3f51] — waterfall_low_color_tx default Color.Black.
    void defaults_match_thetis()
    {
        SpectrumWidget w(nullptr);  // no AppSettings populated, pure defaults

        // From Thetis Display.cs:1925 [v2.10.3.13+501e3f51] — tx_wf_amp_min = -70.
        QCOMPARE(w.txWfLowLevel(), -70);

        // From Thetis Display.cs:1911 [v2.10.3.13+501e3f51] — tx_wf_amp_max = 30.
        QCOMPARE(w.txWfHighLevel(), 30);

        // From Thetis Display.cs:428 [v2.10.3.13+501e3f51] — _tx_color_scheme = ColorScheme.enhanced.
        QCOMPARE(static_cast<int>(w.txWfPalette()),
                 static_cast<int>(WfColorScheme::Enhanced));

        // From Thetis Display.cs:2516 [v2.10.3.13+501e3f51] — waterfall_low_color_tx = Color.Black.
        QCOMPARE(w.txWfLowColor(), QColor(Qt::black));
    }

    // Verifies that TX waterfall settings round-trip through AppSettings.
    // saveSettingsForTest() and loadSettingsForTest() are test seams that
    // will be added in 3M-5b Steps 1.5/1.6; expected to fail to compile until then.
    void settings_round_trip()
    {
        SpectrumWidget w(nullptr);
        w.setTxWfLowLevel(-100);
        w.setTxWfHighLevel(50);
        w.setTxWfPalette(WfColorScheme::ClarityBlue);
        w.setTxWfLowColor(QColor(QStringLiteral("#FF112233")));

        // 3M-5b Step 1.5/1.6 will add these test seams; expected to fail to compile until then.
        w.saveSettingsForTest();

        SpectrumWidget w2(nullptr);
        // 3M-5b Step 1.5/1.6 will add this test seam; expected to fail to compile until then.
        w2.loadSettingsForTest();

        QCOMPARE(w2.txWfLowLevel(), -100);
        QCOMPARE(w2.txWfHighLevel(), 50);
        QCOMPARE(static_cast<int>(w2.txWfPalette()),
                 static_cast<int>(WfColorScheme::ClarityBlue));
        QCOMPARE(w2.txWfLowColor(), QColor(QStringLiteral("#FF112233")));
    }

    // Verifies that dbmToRgb uses TX thresholds when MOX active, RX thresholds
    // when MOX off — mirroring display.cs:6506-6595 [v2.10.3.13+501e3f51] inline branch.
    //
    // setWfLowThresholdForTest() and setWfHighThresholdForTest() are test seams that
    // will be added in 3M-5b Steps 1.7/1.8; expected to fail to compile until then.
    // dbmToRgbForTest() is also a test seam added in Step 1.7.
    void mox_gate_thresholds()
    {
        SpectrumWidget w(nullptr);
        w.setTxWfLowLevel(-100);
        w.setTxWfHighLevel(20);

        // RX-side thresholds set to obviously different values so we can tell
        // which path dbmToRgb chose.
        // 3M-5b Step 1.7/1.8 will add these test seams; expected to fail to compile until then.
        w.setWfLowThresholdForTest(-200.0f);
        w.setWfHighThresholdForTest(-50.0f);

        // MOX off: dbmToRgb should use RX thresholds.
        w.setMoxOverlay(false);
        // 3M-5b Step 1.7 will add this test seam; expected to fail to compile until then.
        const QRgb rxColor = w.dbmToRgbForTest(-100.0f);

        // MOX on: dbmToRgb should use TX thresholds.
        w.setMoxOverlay(true);
        // 3M-5b Step 1.7 will add this test seam; expected to fail to compile until then.
        const QRgb txColor = w.dbmToRgbForTest(-100.0f);

        // Different threshold mappings must produce different colors for the same dBm input.
        QVERIFY(rxColor != txColor);
    }

    // Verifies that dbmToRgb uses TX palette when MOX active, RX palette when MOX off.
    // Mirrors display.cs:6516+6538+6570+6592 [v2.10.3.13+501e3f51] cScheme selection.
    //
    // dbmToRgbForTest() is a test seam added in Step 1.7.
    void mox_gate_palette()
    {
        SpectrumWidget w(nullptr);
        w.setWfColorScheme(WfColorScheme::Default);
        w.setTxWfPalette(WfColorScheme::ClarityBlue);

        w.setMoxOverlay(false);
        // 3M-5b Step 1.7 will add this test seam; expected to fail to compile until then.
        const QRgb rxColor = w.dbmToRgbForTest(-90.0f);  // Default palette

        w.setMoxOverlay(true);
        // 3M-5b Step 1.7 will add this test seam; expected to fail to compile until then.
        const QRgb txColor = w.dbmToRgbForTest(-90.0f);  // ClarityBlue palette

        QVERIFY(rxColor != txColor);
    }
};

// QApplication is required for SpectrumWidget (a QWidget subclass).
QTEST_MAIN(TestTxWaterfallColormap)
#include "tst_tx_waterfall_colormap.moc"
