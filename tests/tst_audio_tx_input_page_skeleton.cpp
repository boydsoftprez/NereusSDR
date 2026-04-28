// tests/tst_audio_tx_input_page_skeleton.cpp  (NereusSDR)
//
// Phase 3M-1b Task I.1 — AudioTxInputPage skeleton + TransmitModel::micSource
//
// no-port-check: test fixture — no Thetis attribution required.
//
// Verifies:
//   1. HL2 board (hasMicJack=false): PC Mic enabled, Radio Mic disabled.
//   2. HL2 board: Radio Mic has exact disabled-tooltip text.
//   3. G2 board (hasMicJack=true): both PC Mic and Radio Mic enabled.
//   4. G2 board: Radio Mic has no tooltip when enabled.
//   5. Default state: PC Mic checked, Radio Mic unchecked.
//   6. UI → Model round-trip: clicking Radio Mic → micSource == Radio.
//   7. UI → Model round-trip: clicking PC Mic after Radio → micSource == Pc.
//   8. Model → UI round-trip: setMicSource(Radio) → Radio Mic button checked.
//   9. Model → UI round-trip: setMicSource(Pc) → PC Mic button checked.
//  10. HL2 forced state: Radio Mic button is disabled even after setMicSource(Radio)
//      is called programmatically (button disabled, model still holds Radio).
//  11. TransmitModel signal emission: setMicSource emits micSourceChanged.
//  12. TransmitModel idempotency: setMicSource(Pc) twice → only one signal emit.

#include <QtTest/QtTest>
#include <QApplication>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QGroupBox>
#include <QRadioButton>

#include "core/AppSettings.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "gui/setup/AudioTxInputPage.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

// Helper: find the first child QRadioButton with the given text.
static QRadioButton* findRadioButton(QWidget* parent, const QString& text)
{
    const auto buttons = parent->findChildren<QRadioButton*>();
    for (QRadioButton* btn : buttons) {
        if (btn->text() == text) { return btn; }
    }
    return nullptr;
}

class TestAudioTxInputPageSkeleton : public QObject
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

    // ── 1. HL2 (hasMicJack=false): PC Mic enabled, Radio Mic disabled ─────────

    void hl2_pcMic_isEnabled()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(false);
        AudioTxInputPage page(&model);

        QRadioButton* btn = findRadioButton(&page, QStringLiteral("PC Mic"));
        QVERIFY2(btn, "PC Mic button not found");
        QVERIFY2(btn->isEnabled(), "PC Mic must be enabled on HL2");
    }

    void hl2_radioMic_isDisabled()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(false);
        AudioTxInputPage page(&model);

        QRadioButton* btn = findRadioButton(&page, QStringLiteral("Radio Mic"));
        QVERIFY2(btn, "Radio Mic button not found");
        QVERIFY2(!btn->isEnabled(), "Radio Mic must be disabled on HL2 (hasMicJack=false)");
    }

    // ── 2. HL2: disabled-tooltip exact text ──────────────────────────────────

    void hl2_radioMic_tooltipExactText()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(false);
        AudioTxInputPage page(&model);

        QRadioButton* btn = findRadioButton(&page, QStringLiteral("Radio Mic"));
        QVERIFY2(btn, "Radio Mic button not found");
        QCOMPARE(btn->toolTip(),
                 QStringLiteral("Radio mic jack not present on Hermes Lite 2"));
    }

    // ── 3. G2 (hasMicJack=true): both buttons enabled ─────────────────────────

    void g2_pcMic_isEnabled()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QRadioButton* btn = findRadioButton(&page, QStringLiteral("PC Mic"));
        QVERIFY2(btn, "PC Mic button not found");
        QVERIFY2(btn->isEnabled(), "PC Mic must be enabled on G2");
    }

    void g2_radioMic_isEnabled()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QRadioButton* btn = findRadioButton(&page, QStringLiteral("Radio Mic"));
        QVERIFY2(btn, "Radio Mic button not found");
        QVERIFY2(btn->isEnabled(), "Radio Mic must be enabled on G2 (hasMicJack=true)");
    }

    // ── 4. G2: Radio Mic has no tooltip when enabled ──────────────────────────

    void g2_radioMic_noTooltip()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QRadioButton* btn = findRadioButton(&page, QStringLiteral("Radio Mic"));
        QVERIFY2(btn, "Radio Mic button not found");
        QVERIFY2(btn->toolTip().isEmpty(),
                 "Radio Mic must have no tooltip on a board with hasMicJack=true");
    }

    // ── 5. Default state: PC Mic checked, Radio Mic unchecked ─────────────────

    void defaultState_pcMic_isChecked()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QRadioButton* pc = findRadioButton(&page, QStringLiteral("PC Mic"));
        QVERIFY2(pc, "PC Mic button not found");
        QVERIFY2(pc->isChecked(), "PC Mic must be checked by default");
    }

    void defaultState_radioMic_isUnchecked()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QRadioButton* radio = findRadioButton(&page, QStringLiteral("Radio Mic"));
        QVERIFY2(radio, "Radio Mic button not found");
        QVERIFY2(!radio->isChecked(), "Radio Mic must not be checked by default");
    }

    // ── 6. UI → Model: select Radio Mic → model.micSource() == Radio ──────────

    void uiToModel_selectRadioMic()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QRadioButton* radioBtn = findRadioButton(&page, QStringLiteral("Radio Mic"));
        QVERIFY2(radioBtn, "Radio Mic button not found");

        radioBtn->setChecked(true);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().micSource(), MicSource::Radio);
    }

    // ── 7. UI → Model: click PC Mic after Radio → model.micSource() == Pc ─────

    void uiToModel_selectPcMicAfterRadio()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        QRadioButton* radioBtn = findRadioButton(&page, QStringLiteral("Radio Mic"));
        QRadioButton* pcBtn    = findRadioButton(&page, QStringLiteral("PC Mic"));
        QVERIFY2(radioBtn, "Radio Mic button not found");
        QVERIFY2(pcBtn,    "PC Mic button not found");

        radioBtn->setChecked(true);
        QApplication::processEvents();
        pcBtn->setChecked(true);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().micSource(), MicSource::Pc);
    }

    // ── 8. Model → UI: setMicSource(Radio) → Radio Mic button checked ─────────

    void modelToUi_setRadio_buttonChecked()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        model.transmitModel().setMicSource(MicSource::Radio);
        QApplication::processEvents();

        QRadioButton* radioBtn = findRadioButton(&page, QStringLiteral("Radio Mic"));
        QVERIFY2(radioBtn, "Radio Mic button not found");
        QVERIFY2(radioBtn->isChecked(),
                 "Radio Mic button must be checked after setMicSource(Radio)");
    }

    // ── 9. Model → UI: setMicSource(Pc) → PC Mic button checked ──────────────

    void modelToUi_setPc_buttonChecked()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        AudioTxInputPage page(&model);

        // First go to Radio, then come back to Pc.
        model.transmitModel().setMicSource(MicSource::Radio);
        QApplication::processEvents();
        model.transmitModel().setMicSource(MicSource::Pc);
        QApplication::processEvents();

        QRadioButton* pcBtn = findRadioButton(&page, QStringLiteral("PC Mic"));
        QVERIFY2(pcBtn, "PC Mic button not found");
        QVERIFY2(pcBtn->isChecked(),
                 "PC Mic button must be checked after setMicSource(Pc)");
    }

    // ── 10. HL2 forced state: Radio Mic button disabled even if model holds Radio

    void hl2_radioMicButton_disabledEvenWhenModelHoldsRadio()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(false);
        AudioTxInputPage page(&model);

        // Programmatic setMicSource(Radio) is allowed at the model level;
        // HL2 force-Pc coercion arrives in Phase L.3. For I.1 the UI just
        // verifies the button is disabled regardless.
        model.transmitModel().setMicSource(MicSource::Radio);
        QApplication::processEvents();

        QRadioButton* radioBtn = findRadioButton(&page, QStringLiteral("Radio Mic"));
        QVERIFY2(radioBtn, "Radio Mic button not found");
        QVERIFY2(!radioBtn->isEnabled(),
                 "Radio Mic button must remain disabled on HL2 regardless of model value");
    }

    // ── 11. TransmitModel signal emission ─────────────────────────────────────

    void transmitModel_setMicSource_emitsSignal()
    {
        TransmitModel tx;
        QSignalSpy spy(&tx, &TransmitModel::micSourceChanged);

        tx.setMicSource(MicSource::Radio);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).value<MicSource>(), MicSource::Radio);
    }

    // ── 12. TransmitModel idempotency: same-value call emits no signal ────────

    void transmitModel_setMicSource_idempotent()
    {
        TransmitModel tx;
        // Default is Pc. Setting to Pc again must not emit.
        QSignalSpy spy(&tx, &TransmitModel::micSourceChanged);

        tx.setMicSource(MicSource::Pc);
        QCOMPARE(spy.count(), 0);

        // Now set to Radio, then set Radio again — only one emission total.
        tx.setMicSource(MicSource::Radio);
        QCOMPARE(spy.count(), 1);

        tx.setMicSource(MicSource::Radio);
        QCOMPARE(spy.count(), 1);  // must not have fired again
    }
};

QTEST_MAIN(TestAudioTxInputPageSkeleton)
#include "tst_audio_tx_input_page_skeleton.moc"
