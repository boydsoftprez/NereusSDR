// no-port-check: NereusSDR-original test file.  All Thetis source cites for
// the underlying TransmitModel properties live in TransmitModel.h.
// =================================================================
// tests/tst_tx_applet_vox_relocated.cpp  (NereusSDR)
// =================================================================
//
// Phase 3M-3a-iii bench polish (2026-05-04) — TxApplet VOX row wiring.
//
// The full VOX row (button + threshold slider + DexpPeakMeter strip + Hold
// slider + 100 ms peak-meter poller + right-click → Setup) was relocated
// from PhoneCwApplet (Phone tab Control #10) to TxApplet's right pane
// directly under TUNE/MOX (Option B - full row).  Operators wanted the
// VOX engage surface next to MOX/TUNE on the right pane where they engage
// TX, not buried on the Phone tab.  This file pins the new TxApplet
// wiring contracts:
//
//   VOX [ON]   — bidirectional ↔ TransmitModel::voxEnabled
//                 (objectName "TxVoxButton").
//   VOX Th     — bidirectional ↔ TransmitModel::voxThresholdDb
//                 (objectName "TxVoxThresholdSlider", range -80..0 dB).
//   VOX Hold   — bidirectional ↔ TransmitModel::voxHangTimeMs
//                 (objectName "TxVoxHoldSlider", range 1..2000 ms).
//   Right-click on [VOX] → emit openSetupRequested("Transmit", "DEXP/VOX").
//   Peak meter — DexpPeakMeter strip under threshold slider
//                 (objectName "TxVoxPeakMeter").
//   Polling     — 100 ms QTimer drives the VOX peak meter (matches Thetis
//                 UpdateNoiseGate Task.Delay(100), console.cs:25347
//                 [v2.10.3.13]).
//
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QTimer>

#include "core/AppSettings.h"
#include "gui/applets/TxApplet.h"
#include "gui/widgets/DexpPeakMeter.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestTxAppletVoxRelocated : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        AppSettings::instance().clear();
    }

    // ── 1. VOX [ON] toggle bidirectional with TM::voxEnabled ──────────────
    void voxButton_bidirectionalWithTm()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* btn = applet.findChild<QPushButton*>(QStringLiteral("TxVoxButton"));
        QVERIFY2(btn != nullptr, "VOX button not found on TxApplet (relocation regression?)");
        QVERIFY(btn->isCheckable());

        // Default voxEnabled is false (TransmitModel.h:583-591 [v2.10.3.13]).
        QCOMPARE(rm.transmitModel().voxEnabled(), false);
        QCOMPARE(btn->isChecked(), false);

        // UI → Model
        btn->click();
        QCOMPARE(rm.transmitModel().voxEnabled(), true);
        QCOMPARE(btn->isChecked(), true);

        btn->click();
        QCOMPARE(rm.transmitModel().voxEnabled(), false);
        QCOMPARE(btn->isChecked(), false);

        // Model → UI
        rm.transmitModel().setVoxEnabled(true);
        QCOMPARE(btn->isChecked(), true);

        rm.transmitModel().setVoxEnabled(false);
        QCOMPARE(btn->isChecked(), false);
    }

    // ── 2. VOX threshold slider bidirectional with TM::voxThresholdDb ────
    void voxThresholdSlider_bidirectionalWithTm()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* sl = applet.findChild<QSlider*>(QStringLiteral("TxVoxThresholdSlider"));
        QVERIFY2(sl != nullptr, "VOX threshold slider not found on TxApplet (relocation regression?)");

        // Range -80..0 from console.Designer.cs:6018-6019 [v2.10.3.13].
        QCOMPARE(sl->minimum(), -80);
        QCOMPARE(sl->maximum(),   0);

        // UI → Model
        sl->setValue(-30);
        QCOMPARE(rm.transmitModel().voxThresholdDb(), -30);

        sl->setValue(0);
        QCOMPARE(rm.transmitModel().voxThresholdDb(), 0);

        sl->setValue(-80);
        QCOMPARE(rm.transmitModel().voxThresholdDb(), -80);

        // Model → UI
        rm.transmitModel().setVoxThresholdDb(-25);
        QCOMPARE(sl->value(), -25);

        rm.transmitModel().setVoxThresholdDb(-60);
        QCOMPARE(sl->value(), -60);
    }

    // ── 3. VOX hold slider bidirectional with TM::voxHangTimeMs ──────────
    void voxHoldSlider_bidirectionalWithTm()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* sl = applet.findChild<QSlider*>(QStringLiteral("TxVoxHoldSlider"));
        QVERIFY2(sl != nullptr, "VOX hold slider not found on TxApplet (relocation regression?)");

        // Range 1..2000 from setup.designer.cs:45005-45013 [v2.10.3.13].
        QCOMPARE(sl->minimum(),    1);
        QCOMPARE(sl->maximum(), 2000);

        // UI → Model
        sl->setValue(750);
        QCOMPARE(rm.transmitModel().voxHangTimeMs(), 750);

        sl->setValue(1);
        QCOMPARE(rm.transmitModel().voxHangTimeMs(), 1);

        sl->setValue(2000);
        QCOMPARE(rm.transmitModel().voxHangTimeMs(), 2000);

        // Model → UI
        rm.transmitModel().setVoxHangTimeMs(1200);
        QCOMPARE(sl->value(), 1200);

        rm.transmitModel().setVoxHangTimeMs(500);
        QCOMPARE(sl->value(), 500);
    }

    // ── 4. Right-click on VOX button emits openSetupRequested("Transmit",
    //      "DEXP/VOX") ─────────────────────────────────────────────────────
    void voxButton_rightClickEmitsOpenSetupRequested()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* btn = applet.findChild<QPushButton*>(QStringLiteral("TxVoxButton"));
        QVERIFY(btn != nullptr);
        QCOMPARE(btn->contextMenuPolicy(), Qt::CustomContextMenu);

        QSignalSpy spy(&applet, &TxApplet::openSetupRequested);
        QVERIFY(spy.isValid());

        // CustomContextMenu policy means right-click emits
        // customContextMenuRequested.  Emit directly to exercise the
        // applet's connected slot.
        emit btn->customContextMenuRequested(QPoint(0, 0));

        QCOMPARE(spy.count(), 1);
        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("Transmit"));
        QCOMPARE(args.at(1).toString(), QStringLiteral("DEXP/VOX"));
    }

    // ── 5. m_voxPeakMeter exists (not null) ──────────────────────────────
    void voxPeakMeter_existsAfterConstruction()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* voxMeter = applet.findChild<DexpPeakMeter*>(QStringLiteral("TxVoxPeakMeter"));
        QVERIFY2(voxMeter != nullptr, "VOX peak meter not created on TxApplet");
    }

    // ── 6. m_voxMeterTimer interval is 100 ms ────────────────────────────
    //      Matches Thetis UpdateNoiseGate Task.Delay(100) at
    //      console.cs:25347 [v2.10.3.13].
    void voxMeterTimer_intervalIs100ms()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        // TxApplet now owns at least one QTimer (the VOX meter @ 100 ms;
        // there's also the existing 50 ms fwd-power refresh timer).  Walk
        // all QTimer children and confirm exactly one ticks at 100 ms.
        const auto timers = applet.findChildren<QTimer*>();
        bool found100 = false;
        for (QTimer* t : timers) {
            if (t->interval() == 100 && t->isActive()) {
                found100 = true;
                break;
            }
        }
        QVERIFY2(found100, "No active 100 ms QTimer found on TxApplet");
    }

    // ── 7. PhoneCwApplet must NOT have the VOX widgets anymore ──────────
    //      After the 2026-05-04 relocation, the PhoneCw* VOX object names
    //      should not appear on TxApplet (those are PhoneCw-namespaced).
    //      The TxVoxButton / TxVoxThresholdSlider / TxVoxHoldSlider /
    //      TxVoxPeakMeter checks above already pin the relocation; this
    //      explicit check just ensures we didn't accidentally put both
    //      sets on TxApplet during the move.
    void onlyTxVoxObjectNamesPresent()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        QVERIFY2(applet.findChild<QPushButton*>(QStringLiteral("PhoneCwVoxButton")) == nullptr,
                 "PhoneCw VOX button should not be on TxApplet");
        QVERIFY2(applet.findChild<QSlider*>(QStringLiteral("PhoneCwVoxThresholdSlider")) == nullptr,
                 "PhoneCw VOX threshold slider should not be on TxApplet");
        QVERIFY2(applet.findChild<QSlider*>(QStringLiteral("PhoneCwVoxHoldSlider")) == nullptr,
                 "PhoneCw VOX hold slider should not be on TxApplet");
        QVERIFY2(applet.findChild<DexpPeakMeter*>(QStringLiteral("PhoneCwVoxPeakMeter")) == nullptr,
                 "PhoneCw VOX peak meter should not be on TxApplet");
    }
};

QTEST_MAIN(TestTxAppletVoxRelocated)
#include "tst_tx_applet_vox_relocated.moc"
