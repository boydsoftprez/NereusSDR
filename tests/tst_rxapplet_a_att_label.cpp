// no-port-check: NereusSDR-original test for issue #174 verification.
//
// Issue #174 (HL2 user): toggling Setup → General → Options →
// "Auto Attenuate RX1 Enable" must flip the RxApplet label from
// "S-ATT" to "A-ATT" so the user sees that auto-attenuate is armed.
// Inspired by mi0bot-Thetis console.cs:21342-21365 [v2.10.3.13-beta2]
// AutoAttRX1 property (HL2-only there).  NereusSDR widens to all
// boards because StepAttenuatorController auto-att is universal —
// non-HL2 users had the same blind-spot bug.
//
// What this test verifies:
//
//   step att | auto-att | label
//   ---------|----------|--------
//   on       | off      | "S-ATT"
//   on       | on       | "A-ATT"   ← issue #174 fix
//   off      | any      | "ATT"     (preamp mode, all boards)

#include <QtTest/QtTest>
#include <QApplication>

#include "core/HpsdrModel.h"
#include "core/StepAttenuatorController.h"
#include "gui/applets/RxApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestRxAppletAAttLabel : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int   argc = 0;
            static char* argv = nullptr;
            new QApplication(argc, &argv);
        }
    }

    // HL2 + step att on + auto off → "S-ATT" (default mi0bot behavior).
    // The label-update connect chain lives in RxApplet::connectSlice
    // (driven by setSlice after construction — same path MainWindow
    // takes when a radio comes up).
    void hl2_auto_off_shows_s_att()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        StepAttenuatorController ctrl;
        ctrl.setStepAttEnabled(true);
        ctrl.setAutoAttEnabled(false);
        model.setStepAttController(&ctrl);
        SliceModel slice;
        RxApplet applet(nullptr, &model);
        applet.setSlice(&slice);
        QCOMPARE(applet.attLabelTextForTest(), QStringLiteral("S-ATT"));
    }

    // HL2 + step att on + auto on → "A-ATT".  This is the strict mi0bot
    // port from console.cs:21342-21365 [v2.10.3.13-beta2] AutoAttRX1
    // setter: `if (HardwareSpecific.Model == HPSDRModel.HERMESLITE)
    //          if (_auto_att_rx1) lblPreamp.Text = "A-ATT";`
    void hl2_auto_on_shows_a_att()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        StepAttenuatorController ctrl;
        ctrl.setStepAttEnabled(true);
        ctrl.setAutoAttEnabled(true);
        model.setStepAttController(&ctrl);
        SliceModel slice;
        RxApplet applet(nullptr, &model);
        applet.setSlice(&slice);
        QCOMPARE(applet.attLabelTextForTest(), QStringLiteral("A-ATT"));
    }

    // Live toggle after construction also flips the label.  This is the
    // path the user hits in the field: open Settings, toggle the
    // "Enable" checkbox, watch the RxApplet label flip.
    void hl2_toggle_after_construct_flips_label()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        StepAttenuatorController ctrl;
        ctrl.setStepAttEnabled(true);
        model.setStepAttController(&ctrl);
        SliceModel slice;
        RxApplet applet(nullptr, &model);
        applet.setSlice(&slice);

        QCOMPARE(applet.attLabelTextForTest(), QStringLiteral("S-ATT"));

        ctrl.setAutoAttEnabled(true);
        QCOMPARE(applet.attLabelTextForTest(), QStringLiteral("A-ATT"));

        ctrl.setAutoAttEnabled(false);
        QCOMPARE(applet.attLabelTextForTest(), QStringLiteral("S-ATT"));
    }

    // Issue #174 widen: non-HL2 boards also flip to "A-ATT".  We
    // intentionally diverge from mi0bot's HL2-only gate because
    // StepAttenuatorController auto-att is available on every board in
    // NereusSDR — so every board has the same label-feedback need.
    void hermes_auto_on_shows_a_att()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Hermes);
        StepAttenuatorController ctrl;
        ctrl.setStepAttEnabled(true);
        ctrl.setAutoAttEnabled(true);
        model.setStepAttController(&ctrl);
        SliceModel slice;
        RxApplet applet(nullptr, &model);
        applet.setSlice(&slice);
        QCOMPARE(applet.attLabelTextForTest(), QStringLiteral("A-ATT"));
    }

    // Same widen check on a different non-HL2 board to make sure the
    // gate is truly board-agnostic, not just "HL2 + Hermes".
    void orionmkii_auto_on_shows_a_att()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::OrionMKII);
        StepAttenuatorController ctrl;
        ctrl.setStepAttEnabled(true);
        ctrl.setAutoAttEnabled(true);
        model.setStepAttController(&ctrl);
        SliceModel slice;
        RxApplet applet(nullptr, &model);
        applet.setSlice(&slice);
        QCOMPARE(applet.attLabelTextForTest(), QStringLiteral("A-ATT"));
    }

    // Step att off (preamp mode): label is "ATT" regardless of auto.
    void hl2_step_off_shows_att_even_with_auto_on()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        StepAttenuatorController ctrl;
        ctrl.setStepAttEnabled(false);
        ctrl.setAutoAttEnabled(true);
        model.setStepAttController(&ctrl);
        SliceModel slice;
        RxApplet applet(nullptr, &model);
        applet.setSlice(&slice);
        QCOMPARE(applet.attLabelTextForTest(), QStringLiteral("ATT"));
    }

    // Regression guard: setAutoAttEnabled emits autoAttEnabledChanged
    // exactly once per real state change (no-op on duplicate sets).
    void setAutoAttEnabled_emits_only_on_change()
    {
        StepAttenuatorController ctrl;
        QSignalSpy spy(&ctrl, &StepAttenuatorController::autoAttEnabledChanged);
        QVERIFY(spy.isValid());

        ctrl.setAutoAttEnabled(false);  // already false (default)
        QCOMPARE(spy.count(), 0);

        ctrl.setAutoAttEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last().at(0).toBool(), true);

        ctrl.setAutoAttEnabled(true);   // duplicate
        QCOMPARE(spy.count(), 1);

        ctrl.setAutoAttEnabled(false);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.last().at(0).toBool(), false);
    }
};

QTEST_MAIN(TestRxAppletAAttLabel)
#include "tst_rxapplet_a_att_label.moc"
