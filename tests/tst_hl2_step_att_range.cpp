// no-port-check: NereusSDR-original tests for hermes-filter-debug Bug 1.
//
// HL2 step-attenuator UI range:
//   * StepAttenuatorController defaults to 0..31; setMin/Max round-trip works.
//   * setAttenuation respects an active negative minimum (no clamp-to-zero).
//   * RxApplet::setBoardCapabilities(HL2 caps) re-ranges m_stepAttSpin to the
//     signed -28..+31 range advertised in BoardCapabilities (mirrors mi0bot
//     setup.cs:16085-16086 [v2.10.3.13-beta2]).
//
// hermes-filter-debug 2026-05-01 — JJ Boyd (KG4VCF), AI-assisted.

#include <QtTest/QtTest>
#include <QApplication>
#include <QSpinBox>

#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/StepAttenuatorController.h"
#include "gui/applets/RxApplet.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TstHl2StepAttRange : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    // Controller defaults match the legacy ANAN/Hermes range (0..31) so
    // pre-connect callers see a sane spin range; HL2 expands later.
    // ─────────────────────────────────────────────────────────────────────
    void controller_default_range_zero_to_31()
    {
        StepAttenuatorController c;
        QCOMPARE(c.minAttenuation(), 0);
        QCOMPARE(c.maxAttenuation(), 31);
    }

    // ─────────────────────────────────────────────────────────────────────
    // Set/get round-trip for both bounds.
    // ─────────────────────────────────────────────────────────────────────
    void controller_setMinMax_round_trip()
    {
        StepAttenuatorController c;
        c.setMinAttenuation(-28);
        c.setMaxAttenuation(31);
        QCOMPARE(c.minAttenuation(), -28);
        QCOMPARE(c.maxAttenuation(), 31);
    }

    // ─────────────────────────────────────────────────────────────────────
    // setAttenuation honours a negative minimum.  This guards
    // StepAttenuatorController.cpp:195-196 — a value of -10 must NOT be
    // clamped back to 0 once the controller has been told about HL2's
    // signed range.
    // ─────────────────────────────────────────────────────────────────────
    void setAttenuation_accepts_negative_when_min_negative()
    {
        StepAttenuatorController c;
        c.setMinAttenuation(-28);
        c.setMaxAttenuation(31);

        c.setAttenuation(-10);
        QCOMPARE(c.attenuatorDb(), -10);

        c.setAttenuation(-28);
        QCOMPARE(c.attenuatorDb(), -28);

        // Below floor — clamp to floor, not to zero.
        c.setAttenuation(-99);
        QCOMPARE(c.attenuatorDb(), -28);

        // Above ceiling — clamp to ceiling.
        c.setAttenuation(99);
        QCOMPARE(c.attenuatorDb(), 31);
    }

    // ─────────────────────────────────────────────────────────────────────
    // setAttenuation still clamps a negative value to 0 when the controller
    // is on the legacy (default) range — non-HL2 boards must keep working.
    // ─────────────────────────────────────────────────────────────────────
    void setAttenuation_clamps_negative_for_default_range()
    {
        StepAttenuatorController c;  // 0..31 defaults
        c.setAttenuation(-10);
        QCOMPARE(c.attenuatorDb(), 0);
    }

    // ─────────────────────────────────────────────────────────────────────
    // RxApplet's S-ATT spinbox picks up board caps via setBoardCapabilities.
    // This is the canonical board-driven hook (currentRadioChanged →
    // MainWindow::setBoardCapabilities); pre-fix only antenna visibility
    // updated here, leaving the spinbox stuck at the construction-time
    // 0..31 default.  Post-fix the range follows caps on every emission.
    // ─────────────────────────────────────────────────────────────────────
    void rxapplet_step_att_range_follows_hl2_caps()
    {
        RadioModel model;
        // We need a step-att controller for RxApplet's wiring path.
        StepAttenuatorController ctrl;
        model.setStepAttController(&ctrl);

        // Construct the applet first (this picks up whatever caps the model
        // has at construction time — typically default).
        RxApplet applet(/*slice=*/nullptr, &model, /*parent=*/nullptr);

        // Look up the spinbox by walking children.  RxApplet doesn't expose
        // m_stepAttSpin publicly so we find it by suffix " dB" and then
        // confirm range.
        QList<QSpinBox*> spins = applet.findChildren<QSpinBox*>();
        QSpinBox* stepAttSpin = nullptr;
        for (QSpinBox* sp : spins) {
            if (sp->suffix() == QStringLiteral(" dB")) {
                stepAttSpin = sp;
                break;
            }
        }
        QVERIFY2(stepAttSpin != nullptr, "RxApplet must own a 'dB'-suffixed QSpinBox");

        // Drive HL2 caps through the public hook.
        const BoardCapabilities& hl2 = BoardCapsTable::forBoard(HPSDRHW::HermesLite);
        applet.setBoardCapabilities(hl2);

        QCOMPARE(stepAttSpin->minimum(), -28);
        QCOMPARE(stepAttSpin->maximum(), 31);
    }
};

QTEST_APPLESS_MAIN(TstHl2StepAttRange)
#include "tst_hl2_step_att_range.moc"
