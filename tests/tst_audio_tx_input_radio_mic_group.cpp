// tests/tst_audio_tx_input_radio_mic_group.cpp  (NereusSDR)
//
// Phase 3M-1b Task I.3 — Radio Mic settings group with per-family layout.
//
// no-port-check: test fixture — no Thetis attribution required.
//
// Visibility tests:
//   1.  HL2 (hasMicJack=false) — all 3 groups hidden regardless of MicSource.
//   2.  Saturn G2 + Radio Mic — only Saturn group visible; Hermes + Orion hidden.
//   3.  OrionMKII + Radio Mic — only Orion group visible; Hermes + Saturn hidden.
//   4.  HermesII + Radio Mic — only Hermes group visible; Orion + Saturn hidden.
//   5.  Switch from PC Mic to Radio Mic (Saturn) — Saturn group becomes visible.
//   6.  Switch from Radio Mic back to PC Mic (Saturn) — all groups hidden.
//
// Bidirectional round-trip tests (representative sample across all families):
//   7.  Hermes: Mic In/Line In radio (UI→Model) — click Line In → lineIn=true.
//   8.  Hermes: lineIn model→UI — setLineIn(false) → Mic In button selected.
//   9.  Hermes: +20 dB Mic Boost (UI→Model) — uncheck → micBoost=false.
//  10.  Hermes: micBoost model→UI — setMicBoost(false) → checkbox unchecked.
//  11.  Hermes: Line In Gain slider (UI→Model) — setValue(-21) → lineInBoost=-10.5
//       (the slider counts half decibels, radio codec lane).
//  12.  Hermes: lineInBoost model→UI — setLineInBoost(4.5) → slider at 9.
//  13.  Orion: Mic Tip-Ring checkbox (UI→Model) — uncheck → micTipRing=false.
//  14.  Orion: micPttDisabled model→UI — setMicPttDisabled(true) → checked.
//  15.  Saturn: 3.5mm/XLR radio (UI→Model) — click 3.5mm → micXlr=false.
//  16.  Saturn: micXlr model→UI — setMicXlr(false) → 3.5mm button selected.
//  17.  Saturn: Mic Bias (UI→Model) — check → micBias=true.
//  18.  No feedback loop — model setter triggers signal exactly once (no echo).
//
// Radio codec lane (2026-09-30, J.J. Boyd KG4VCF, AI-assisted via Anthropic
// Claude Code):
//  19.  Line In Gain moves in Thetis's 1.5 dB steps (udLineInBoost) and
//       shows one decimal; a value between steps snaps to the nearest.
//  20.  A real HL2 (the model's own row): Radio Mic enabled with the add-on
//       note, the Hermes group titled for the HL2 and shown with Radio Mic.
//  21.  Saturn G2 Mic Tip-Ring both ways, shared with the Orion group.
//  22.  Red Pitaya: the Orion group is disabled with its reason; another
//       Orion-MkII radio's is enabled.

#include <QtTest/QtTest>
#include <QApplication>
#include <QAbstractButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QSlider>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "gui/setup/AudioTxInputPage.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

// Helper: find the first QRadioButton with the given text inside a parent.
static QRadioButton* findRadioButton(QWidget* parent, const QString& text)
{
    const auto buttons = parent->findChildren<QRadioButton*>();
    for (QRadioButton* btn : buttons) {
        if (btn->text() == text) { return btn; }
    }
    return nullptr;
}

// Helper: find the first QCheckBox with the given text inside a parent.
static QCheckBox* findCheckBox(QWidget* parent, const QString& text)
{
    const auto boxes = parent->findChildren<QCheckBox*>();
    for (QCheckBox* cb : boxes) {
        if (cb->text() == text) { return cb; }
    }
    return nullptr;
}

class TestAudioTxInputRadioMicGroup : public QObject
{
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

    // ── 1. HL2 (hasMicJack=false) — all 3 groups hidden ─────────────────────

    void hl2_allRadioMicGroups_hidden()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(false);
        model.setCapsHwForTest(HPSDRHW::HermesLite);
        AudioTxInputPage page(&model);

        // Even if we could select Radio Mic (disabled, but test the group
        // directly), all three groups must be hidden.
        QVERIFY2(page.hermesRadioMicGroup() != nullptr, "Hermes group must exist");
        QVERIFY2(page.orionRadioMicGroup()  != nullptr, "Orion group must exist");
        QVERIFY2(page.saturnRadioMicGroup() != nullptr, "Saturn group must exist");

        QVERIFY2(page.hermesRadioMicGroup()->isHidden(),
                 "Hermes group must be hidden on HL2 (hasMicJack=false)");
        QVERIFY2(page.orionRadioMicGroup()->isHidden(),
                 "Orion group must be hidden on HL2 (hasMicJack=false)");
        QVERIFY2(page.saturnRadioMicGroup()->isHidden(),
                 "Saturn group must be hidden on HL2 (hasMicJack=false)");
    }

    // ── 2. Saturn G2 + Radio Mic → only Saturn group visible ─────────────────

    void saturn_radioMic_saturnGroupVisible()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::Saturn);
        AudioTxInputPage page(&model);

        // Select Radio Mic.
        QRadioButton* radioBtn = findRadioButton(&page, QStringLiteral("Radio Mic"));
        QVERIFY2(radioBtn, "Radio Mic button not found");
        radioBtn->setChecked(true);
        QApplication::processEvents();

        QVERIFY2(!page.saturnRadioMicGroup()->isHidden(),
                 "Saturn group must be visible on Saturn + Radio Mic");
        QVERIFY2(page.hermesRadioMicGroup()->isHidden(),
                 "Hermes group must be hidden on Saturn board");
        QVERIFY2(page.orionRadioMicGroup()->isHidden(),
                 "Orion group must be hidden on Saturn board");
    }

    // ── 3. OrionMKII + Radio Mic → only Orion group visible ──────────────────

    void orionMkii_radioMic_orionGroupVisible()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::OrionMKII);
        AudioTxInputPage page(&model);

        QRadioButton* radioBtn = findRadioButton(&page, QStringLiteral("Radio Mic"));
        QVERIFY2(radioBtn, "Radio Mic button not found");
        radioBtn->setChecked(true);
        QApplication::processEvents();

        QVERIFY2(!page.orionRadioMicGroup()->isHidden(),
                 "Orion group must be visible on OrionMKII + Radio Mic");
        QVERIFY2(page.hermesRadioMicGroup()->isHidden(),
                 "Hermes group must be hidden on OrionMKII board");
        QVERIFY2(page.saturnRadioMicGroup()->isHidden(),
                 "Saturn group must be hidden on OrionMKII board");
    }

    // ── 4. HermesII + Radio Mic → only Hermes group visible ──────────────────

    void hermesii_radioMic_hermesGroupVisible()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::HermesII);
        AudioTxInputPage page(&model);

        QRadioButton* radioBtn = findRadioButton(&page, QStringLiteral("Radio Mic"));
        QVERIFY2(radioBtn, "Radio Mic button not found");
        radioBtn->setChecked(true);
        QApplication::processEvents();

        QVERIFY2(!page.hermesRadioMicGroup()->isHidden(),
                 "Hermes group must be visible on HermesII + Radio Mic");
        QVERIFY2(page.orionRadioMicGroup()->isHidden(),
                 "Orion group must be hidden on HermesII board");
        QVERIFY2(page.saturnRadioMicGroup()->isHidden(),
                 "Saturn group must be hidden on HermesII board");
    }

    // ── 5. PC Mic → Radio Mic (Saturn) — Saturn group becomes visible ─────────

    void saturn_pcToRadio_saturnGroupAppears()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::Saturn);
        AudioTxInputPage page(&model);

        // Initially PC Mic → all Radio Mic groups hidden.
        QVERIFY2(page.saturnRadioMicGroup()->isHidden(),
                 "Saturn group must be hidden initially (PC Mic selected)");

        // Switch to Radio Mic.
        QRadioButton* radioBtn = findRadioButton(&page, QStringLiteral("Radio Mic"));
        QVERIFY2(radioBtn, "Radio Mic button not found");
        radioBtn->setChecked(true);
        QApplication::processEvents();

        QVERIFY2(!page.saturnRadioMicGroup()->isHidden(),
                 "Saturn group must appear after switching to Radio Mic");
    }

    // ── 6. Radio Mic → PC Mic (Saturn) — all groups hidden again ─────────────

    void saturn_radioToPc_groupsHidden()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::Saturn);
        AudioTxInputPage page(&model);

        // Go to Radio Mic.
        QRadioButton* radioBtn = findRadioButton(&page, QStringLiteral("Radio Mic"));
        QVERIFY2(radioBtn, "Radio Mic button not found");
        radioBtn->setChecked(true);
        QApplication::processEvents();

        // Switch back to PC Mic.
        QRadioButton* pcBtn = findRadioButton(&page, QStringLiteral("PC Mic"));
        QVERIFY2(pcBtn, "PC Mic button not found");
        pcBtn->setChecked(true);
        QApplication::processEvents();

        QVERIFY2(page.saturnRadioMicGroup()->isHidden(),
                 "Saturn group must be hidden after switching back to PC Mic");
        QVERIFY2(page.hermesRadioMicGroup()->isHidden(),
                 "Hermes group must be hidden after switching back to PC Mic");
        QVERIFY2(page.orionRadioMicGroup()->isHidden(),
                 "Orion group must be hidden after switching back to PC Mic");
    }

    // ── 7. Hermes: click Line In → lineIn=true (UI→Model) ────────────────────

    void hermes_lineInRadio_uiToModel()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::Hermes);
        AudioTxInputPage page(&model);

        QGroupBox* grp = page.hermesRadioMicGroup();
        QVERIFY2(grp, "Hermes group must exist");

        QRadioButton* lineInBtn = findRadioButton(grp, QStringLiteral("Line In"));
        QVERIFY2(lineInBtn, "Line In button not found in Hermes group");

        lineInBtn->setChecked(true);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().lineIn(), true);
    }

    // ── 8. Hermes: setLineIn(false) → Mic In button selected (Model→UI) ───────

    void hermes_lineIn_modelToUi()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::Hermes);
        AudioTxInputPage page(&model);

        // First set to Line In.
        model.transmitModel().setLineIn(true);
        QApplication::processEvents();

        // Now set back to Mic In.
        model.transmitModel().setLineIn(false);
        QApplication::processEvents();

        QGroupBox* grp = page.hermesRadioMicGroup();
        QRadioButton* micInBtn = findRadioButton(grp, QStringLiteral("Mic In"));
        QVERIFY2(micInBtn, "Mic In button not found");
        QVERIFY2(micInBtn->isChecked(),
                 "Mic In must be selected after setLineIn(false)");
    }

    // ── 9. Hermes: uncheck +20 dB Mic Boost → micBoost=false (UI→Model) ──────

    void hermes_micBoost_uiToModel()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::HermesII);
        AudioTxInputPage page(&model);

        QGroupBox* grp = page.hermesRadioMicGroup();
        QCheckBox* boostChk = findCheckBox(grp, QStringLiteral("+20 dB Mic Boost"));
        QVERIFY2(boostChk, "+20 dB Mic Boost checkbox not found in Hermes group");

        // Default is true; uncheck it.
        boostChk->setChecked(false);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().micBoost(), false);
    }

    // ── 10. Hermes: setMicBoost(false) → checkbox unchecked (Model→UI) ───────

    void hermes_micBoost_modelToUi()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::Angelia);
        AudioTxInputPage page(&model);

        model.transmitModel().setMicBoost(false);
        QApplication::processEvents();

        QGroupBox* grp = page.hermesRadioMicGroup();
        QCheckBox* boostChk = findCheckBox(grp, QStringLiteral("+20 dB Mic Boost"));
        QVERIFY2(boostChk, "+20 dB Mic Boost checkbox not found");
        QVERIFY2(!boostChk->isChecked(),
                 "Hermes Mic Boost checkbox must be unchecked after setMicBoost(false)");
    }

    // ── 11. Hermes: Line In Gain slider UI→Model ──────────────────────────────

    void hermes_lineInGainSlider_uiToModel()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::Hermes);
        AudioTxInputPage page(&model);

        QGroupBox* grp = page.hermesRadioMicGroup();
        const auto sliders = grp->findChildren<QSlider*>();
        QVERIFY2(!sliders.isEmpty(), "No slider found in Hermes group");
        QSlider* lineInSlider = sliders.first();

        lineInSlider->setValue(-21);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().lineInBoost(), -10.5);
    }

    // ── 12. Hermes: setLineInBoost(4.5) → slider at 9 (Model→UI) ─────────────

    void hermes_lineInBoost_modelToUi()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::Hermes);
        AudioTxInputPage page(&model);

        model.transmitModel().setLineInBoost(4.5);
        QApplication::processEvents();

        QGroupBox* grp = page.hermesRadioMicGroup();
        const auto sliders = grp->findChildren<QSlider*>();
        QVERIFY2(!sliders.isEmpty(), "No slider found in Hermes group");
        QCOMPARE(sliders.first()->value(), 9);
        QCOMPARE(page.hermesLineInGainLabel()->text(), QStringLiteral("4.5 dB"));
    }

    // ── 13. Orion: uncheck Mic Tip-Ring → micTipRing=false (UI→Model) ─────────

    void orion_micTipRing_uiToModel()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::OrionMKII);
        AudioTxInputPage page(&model);

        QGroupBox* grp = page.orionRadioMicGroup();
        QCheckBox* cb = findCheckBox(grp, QStringLiteral("Mic Tip-Ring (Tip is Mic)"));
        QVERIFY2(cb, "Mic Tip-Ring checkbox not found in Orion group");

        // Default is true; uncheck it.
        cb->setChecked(false);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().micTipRing(), false);
    }

    // ── 14. Orion: setMicPttDisabled(true) → checkbox checked (Model→UI) ──────

    void orion_micPttDisabled_modelToUi()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::Orion);
        AudioTxInputPage page(&model);

        model.transmitModel().setMicPttDisabled(true);
        QApplication::processEvents();

        QGroupBox* grp = page.orionRadioMicGroup();
        QCheckBox* cb = findCheckBox(grp, QStringLiteral("Mic PTT Disabled"));
        QVERIFY2(cb, "Mic PTT Disabled checkbox not found in Orion group");
        QVERIFY2(cb->isChecked(),
                 "Mic PTT Disabled must be checked after setMicPttDisabled(true)");
    }

    // ── 15. Saturn: click 3.5 mm Jack → micXlr=false (UI→Model) ─────────────

    void saturn_micXlr_uiToModel_3_5mm()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::Saturn);
        AudioTxInputPage page(&model);

        QGroupBox* grp = page.saturnRadioMicGroup();
        QRadioButton* jackBtn = findRadioButton(grp, QStringLiteral("3.5 mm Jack"));
        QVERIFY2(jackBtn, "3.5 mm Jack button not found in Saturn group");

        jackBtn->setChecked(true);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().micXlr(), false);
    }

    // ── 16. Saturn: setMicXlr(false) → 3.5mm button selected (Model→UI) ──────

    void saturn_micXlr_modelToUi()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::Saturn);
        AudioTxInputPage page(&model);

        // Default is micXlr=true (XLR); set to false (3.5mm).
        model.transmitModel().setMicXlr(false);
        QApplication::processEvents();

        QGroupBox* grp = page.saturnRadioMicGroup();
        QRadioButton* jackBtn = findRadioButton(grp, QStringLiteral("3.5 mm Jack"));
        QVERIFY2(jackBtn, "3.5 mm Jack button not found");
        QVERIFY2(jackBtn->isChecked(),
                 "3.5 mm Jack must be selected after setMicXlr(false)");
    }

    // ── 17. Saturn: check Mic Bias → micBias=true (UI→Model) ─────────────────

    void saturn_micBias_uiToModel()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::Saturn);
        AudioTxInputPage page(&model);

        QGroupBox* grp = page.saturnRadioMicGroup();
        QCheckBox* cb = findCheckBox(grp, QStringLiteral("Mic Bias"));
        QVERIFY2(cb, "Mic Bias checkbox not found in Saturn group");

        // Default is false; check it.
        cb->setChecked(true);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().micBias(), true);
    }

    // ── 18. No feedback loop — model setter emits micBoostChanged exactly once ─

    void micBoost_no_feedback_loop()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::Hermes);
        AudioTxInputPage page(&model);

        QSignalSpy spy(&model.transmitModel(), &TransmitModel::micBoostChanged);

        // Model setter: should drive UI; UI update must NOT re-trigger model.
        model.transmitModel().setMicBoost(false);
        QApplication::processEvents();

        // Exactly one emission from the initial set.
        QCOMPARE(spy.count(), 1);

        // Second distinct value — one more emission only.
        model.transmitModel().setMicBoost(true);
        QApplication::processEvents();
        QCOMPARE(spy.count(), 2);
    }

    // ── 19. Line In Gain in 1.5 dB steps ─────────────────────────────────────

    void hermes_lineInGain_movesInThetisSteps()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::Hermes);
        AudioTxInputPage page(&model);

        QSlider* slider = page.hermesLineInGainSlider();
        QVERIFY(slider);
        // udLineInBoost (setup.designer.cs:47006-47034 [v2.10.3.15]):
        // -34.5 to 12 in 1.5 dB steps, counted in half decibels.
        QCOMPARE(slider->property("nereusSetupScale").toDouble(), 2.0);
        QCOMPARE(slider->minimum(), -69);
        QCOMPARE(slider->maximum(), 24);
        QCOMPARE(slider->singleStep(), 3);
        QCOMPARE(slider->pageStep(), 3);

        slider->setValue(-69);
        QCOMPARE(model.transmitModel().lineInBoost(), -34.5);
        QCOMPARE(page.hermesLineInGainLabel()->text(), QStringLiteral("-34.5 dB"));
        slider->setValue(24);
        QCOMPARE(model.transmitModel().lineInBoost(), 12.0);
        QCOMPARE(page.hermesLineInGainLabel()->text(), QStringLiteral("12.0 dB"));

        // A drag between steps snaps to the nearest step: -20 half dB
        // (-10 dB) is nearer -10.5 than -9.
        slider->setValue(-20);
        QCOMPARE(slider->value(), -21);
        QCOMPARE(model.transmitModel().lineInBoost(), -10.5);
        QCOMPARE(page.hermesLineInGainLabel()->text(), QStringLiteral("-10.5 dB"));
    }

    // ── 20. A real HL2: Radio Mic open with the add-on note ──────────────────

    void hl2_radioMicOpenWithAddOnNote()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::HERMESLITE);
        QVERIFY(model.boardCapabilities().radioMicNeedsAddOn);
        AudioTxInputPage page(&model);

        const QString note = QStringLiteral(
            "Needs the Hermes Lite 2 audio add-on board. A stock Hermes Lite 2 sends no mic audio.");
        QVERIFY(page.radioMicButton()->isEnabled());
        QCOMPARE(page.radioMicButton()->toolTip(), note);
        QVERIFY(page.radioMicNoteLabel() != nullptr);
        QCOMPARE(page.radioMicNoteLabel()->text(), note);
        QVERIFY(!page.radioMicNoteLabel()->isHidden());
        QCOMPARE(page.hermesRadioMicGroup()->title(), QStringLiteral("Radio Mic (Hermes Lite 2)"));

        // Not locked to the PC mic: Radio Mic sticks and shows the group.
        page.radioMicButton()->setChecked(true);
        QApplication::processEvents();
        QCOMPARE(model.transmitModel().micSource(), MicSource::Radio);
        QVERIFY(!page.hermesRadioMicGroup()->isHidden());
        QVERIFY(page.orionRadioMicGroup()->isHidden());
        QVERIFY(page.saturnRadioMicGroup()->isHidden());

        // Line In reaches the model, as on a Hermes.
        QRadioButton* lineIn = findRadioButton(page.hermesRadioMicGroup(), QStringLiteral("Line In"));
        QVERIFY(lineIn);
        lineIn->setChecked(true);
        QVERIFY(model.transmitModel().lineIn());

        // A Hermes has no note.
        RadioModel hermes;
        hermes.setHpsdrModelForTest(HPSDRModel::HERMES);
        AudioTxInputPage hermesPage(&hermes);
        QVERIFY(hermesPage.radioMicNoteLabel() == nullptr);
        QVERIFY(hermesPage.radioMicButton()->toolTip().isEmpty());
        QCOMPARE(hermesPage.hermesRadioMicGroup()->title(),
                 QStringLiteral("Radio Mic (Hermes / Atlas)"));
    }

    // ── 21. Saturn G2 Mic Tip-Ring ────────────────────────────────────────────

    void saturn_micTipRing_bothWays()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.setCapsHwForTest(HPSDRHW::Saturn);
        AudioTxInputPage page(&model);

        QCheckBox* tipRing = page.saturnMicTipRingCheck();
        QVERIFY(tipRing);
        QCOMPARE(tipRing->parentWidget(), static_cast<QWidget*>(page.saturnRadioMicGroup()));
        QCOMPARE(tipRing->text(), QStringLiteral("Mic Tip-Ring (Tip is Mic)"));
        QCOMPARE(tipRing->property("nereusSetupId").toString(),
                 QStringLiteral("audio.txInput.saturnMicTipRing"));
        QCOMPARE(tipRing->isChecked(), model.transmitModel().micTipRing());

        QSignalSpy spy(&model.transmitModel(), &TransmitModel::micTipRingChanged);
        tipRing->setChecked(!tipRing->isChecked());
        QCOMPARE(model.transmitModel().micTipRing(), tipRing->isChecked());
        QCOMPARE(spy.count(), 1);

        model.transmitModel().setMicTipRing(!model.transmitModel().micTipRing());
        QCOMPARE(tipRing->isChecked(), model.transmitModel().micTipRing());
        QCOMPARE(spy.count(), 2);
    }

    // ── 22. Red Pitaya: the Orion group disabled with its reason ─────────────

    void redPitaya_orionGroupDisabledWithReason()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::REDPITAYA);
        QCOMPARE(model.boardCapabilities().board, HPSDRHW::OrionMKII);
        AudioTxInputPage page(&model);

        QRadioButton* radioBtn = findRadioButton(&page, QStringLiteral("Radio Mic"));
        QVERIFY(radioBtn && radioBtn->isEnabled());
        radioBtn->setChecked(true);
        QApplication::processEvents();

        // In view, disabled, with the reason.
        QVERIFY(!page.orionRadioMicGroup()->isHidden());
        QVERIFY(!page.orionRadioMicGroup()->isEnabled());
        QCOMPARE(page.orionRadioMicGroup()->toolTip(),
                 QStringLiteral("These mic settings do not apply to the Red Pitaya."));

        // The transmit gates put that state back, not an enabled group.
        page.setTransmitSettingsPermittedAt(3, false, QStringLiteral("On the air"));
        QCOMPARE(page.orionRadioMicGroup()->toolTip(), QStringLiteral("On the air"));
        page.setTransmitSettingsPermittedAt(3, true, QString());
        QVERIFY(!page.orionRadioMicGroup()->isEnabled());
        QCOMPARE(page.orionRadioMicGroup()->toolTip(),
                 QStringLiteral("These mic settings do not apply to the Red Pitaya."));

        RadioModel anan;
        anan.setHpsdrModelForTest(HPSDRModel::ANAN7000D);
        AudioTxInputPage ananPage(&anan);
        QVERIFY(ananPage.orionRadioMicGroup()->isEnabled());
        QVERIFY(ananPage.orionRadioMicGroup()->toolTip().isEmpty());
    }
};

QTEST_MAIN(TestAudioTxInputRadioMicGroup)
#include "tst_audio_tx_input_radio_mic_group.moc"
