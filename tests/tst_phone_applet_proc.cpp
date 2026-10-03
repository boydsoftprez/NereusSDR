// no-port-check: NereusSDR-original test file.  All Thetis source cites
// for the underlying TransmitModel properties live in TransmitModel.h.
// =================================================================
// tests/tst_phone_applet_proc.cpp  (NereusSDR)
// =================================================================
//
// Phase 3M-3a-ii post-bench cleanup — PhoneCwApplet PROC button + slider.
//
// PhoneCwApplet's PROC button (#7) and slider (#7 slider) sat un-wired and
// NyiOverlay-marked since 3I-3.  This file pins down the wiring after the
// 3M-3a-ii post-bench cleanup:
//
//   PROC button   — bidirectional ↔ TransmitModel::cpdrOn (objectName
//                   "PhoneCwProcButton").
//   PROC slider   — bidirectional ↔ TransmitModel::cpdrLevelDb (objectName
//                   "PhoneCwProcSlider", range 0..20 dB from
//                   console.Designer.cs:6042-6043 [v2.10.3.13]).
//   value label   — mirrors slider value as "X dB".
//
// The duplicate [PROC] previously placed on TxApplet was removed in the same
// commit (see tst_tx_applet_lev_eq_cfc.cpp).
//
// Tests:
//   1. PROC button exists by object-name "PhoneCwProcButton".
//   2. PROC slider exists by object-name "PhoneCwProcSlider".
//   3. Slider range is 0..20.
//   4. UI → Model: button toggle flips cpdrOn.
//   5. UI → Model: slider value flows to cpdrLevelDb.
//   6. Model → UI: cpdrOnChanged updates button checked state.
//   7. Model → UI: cpdrLevelDbChanged updates slider value.
//   8. Numeric value label shows "X dB" matching the current slider value.
//   9. NyiOverlay markers are absent (button + slider are enabled and the
//      tooltip does NOT contain "This control is not built").
//
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QSlider>

#include "core/AppSettings.h"
#include "gui/applets/PhoneCwApplet.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestPhoneAppletProc : public QObject {
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

    // ── 1. PROC button exists by object-name "PhoneCwProcButton" ──────────
    void procButton_existsByObjectName()
    {
        RadioModel rm;
        PhoneCwApplet applet(&rm);

        auto* btn = applet.findChild<QPushButton*>(QStringLiteral("PhoneCwProcButton"));
        QVERIFY(btn != nullptr);
        QVERIFY(btn->isCheckable());
    }

    // ── 2. PROC slider exists by object-name "PhoneCwProcSlider" ──────────
    void procSlider_existsByObjectName()
    {
        RadioModel rm;
        PhoneCwApplet applet(&rm);

        auto* sl = applet.findChild<QSlider*>(QStringLiteral("PhoneCwProcSlider"));
        QVERIFY(sl != nullptr);
    }

    // ── 3. Slider range is 0..20 (Thetis ptbCPDR Maximum=20, Minimum=0) ───
    void procSlider_rangeIsZeroToTwenty()
    {
        RadioModel rm;
        PhoneCwApplet applet(&rm);

        auto* sl = applet.findChild<QSlider*>(QStringLiteral("PhoneCwProcSlider"));
        QVERIFY(sl != nullptr);
        QCOMPARE(sl->minimum(), 0);
        QCOMPARE(sl->maximum(), 20);
    }

    // ── 4. UI → Model: button toggle flips cpdrOn ─────────────────────────
    void procButton_clickFlipsCpdrOn()
    {
        RadioModel rm;
        PhoneCwApplet applet(&rm);

        auto* btn = applet.findChild<QPushButton*>(QStringLiteral("PhoneCwProcButton"));
        QVERIFY(btn != nullptr);

        // Default cpdrOn is false (TransmitModel.h:1328 [v2.10.3.13]).
        QCOMPARE(rm.transmitModel().cpdrOn(), false);

        btn->click();
        QCOMPARE(rm.transmitModel().cpdrOn(), true);

        btn->click();
        QCOMPARE(rm.transmitModel().cpdrOn(), false);
    }

    // ── 5. UI → Model: slider value flows to cpdrLevelDb ──────────────────
    void procSlider_valueFlowsToCpdrLevelDb()
    {
        RadioModel rm;
        PhoneCwApplet applet(&rm);

        auto* sl = applet.findChild<QSlider*>(QStringLiteral("PhoneCwProcSlider"));
        QVERIFY(sl != nullptr);

        sl->setValue(12);
        QCOMPARE(rm.transmitModel().cpdrLevelDb(), 12);

        sl->setValue(20);
        QCOMPARE(rm.transmitModel().cpdrLevelDb(), 20);

        sl->setValue(0);
        QCOMPARE(rm.transmitModel().cpdrLevelDb(), 0);
    }

    // ── 6. Model → UI: cpdrOnChanged updates button checked state ─────────
    void setCpdrOn_updatesProcButton()
    {
        RadioModel rm;
        PhoneCwApplet applet(&rm);

        auto* btn = applet.findChild<QPushButton*>(QStringLiteral("PhoneCwProcButton"));
        QVERIFY(btn != nullptr);

        // Seed initial state from sync — default cpdrOn is false.
        applet.syncFromModel();
        QCOMPARE(btn->isChecked(), false);

        rm.transmitModel().setCpdrOn(true);
        QCOMPARE(btn->isChecked(), true);

        rm.transmitModel().setCpdrOn(false);
        QCOMPARE(btn->isChecked(), false);
    }

    // ── 7. Model → UI: cpdrLevelDbChanged updates slider value ────────────
    void setCpdrLevelDb_updatesProcSlider()
    {
        RadioModel rm;
        PhoneCwApplet applet(&rm);

        auto* sl = applet.findChild<QSlider*>(QStringLiteral("PhoneCwProcSlider"));
        QVERIFY(sl != nullptr);

        applet.syncFromModel();

        rm.transmitModel().setCpdrLevelDb(8);
        QCOMPARE(sl->value(), 8);

        rm.transmitModel().setCpdrLevelDb(15);
        QCOMPARE(sl->value(), 15);
    }

    // ── 8. Numeric value label shows "X dB" matching current slider value ─
    //      We don't have an objectName on the value label so we walk the
    //      applet's QLabels and look for the "12 dB" / "0 dB" pattern.
    void procValueLabel_mirrorsSliderValue()
    {
        RadioModel rm;
        PhoneCwApplet applet(&rm);

        auto* sl = applet.findChild<QSlider*>(QStringLiteral("PhoneCwProcSlider"));
        QVERIFY(sl != nullptr);

        // Push the slider to a known value and confirm at least one QLabel
        // child of the applet now reads "12 dB".
        sl->setValue(12);

        bool found12 = false;
        for (QLabel* lbl : applet.findChildren<QLabel*>()) {
            if (lbl->text() == QStringLiteral("12 dB")) {
                found12 = true;
                break;
            }
        }
        QVERIFY2(found12, "Numeric value label should read '12 dB' when slider is at 12");

        sl->setValue(0);
        bool found0 = false;
        for (QLabel* lbl : applet.findChildren<QLabel*>()) {
            if (lbl->text() == QStringLiteral("0 dB")) {
                found0 = true;
                break;
            }
        }
        QVERIFY2(found0, "Numeric value label should read '0 dB' when slider is at 0");
    }

    // ── 9. NyiOverlay markers absent: PROC button + slider are enabled and
    //      tooltip does NOT contain "This control is not built" (the marker
    //      signature applied by NyiOverlay::markNyi). ─────────────────────
    void procControls_haveNoNyiMarker()
    {
        RadioModel rm;
        PhoneCwApplet applet(&rm);

        auto* btn = applet.findChild<QPushButton*>(QStringLiteral("PhoneCwProcButton"));
        auto* sl  = applet.findChild<QSlider*>(QStringLiteral("PhoneCwProcSlider"));
        QVERIFY(btn != nullptr);
        QVERIFY(sl  != nullptr);

        QVERIFY(btn->isEnabled());
        QVERIFY(sl->isEnabled());

        QVERIFY(!btn->toolTip().contains(QStringLiteral("This control is not built")));
        QVERIFY(!sl->toolTip().contains(QStringLiteral("This control is not built")));
    }
};

QTEST_MAIN(TestPhoneAppletProc)
#include "tst_phone_applet_proc.moc"
