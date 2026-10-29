// no-port-check: NereusSDR-original test. It drives the Phone/CW applet's
// CW page, the TX applet in CW and Setup > DSP > CW against a real
// RadioModel and a fake connection; no upstream logic is ported here and no
// test keys the radio.
// =================================================================
// tests/tst_cw_page.cpp  (NereusSDR)
// =================================================================
//
// CW transmit Task 12 (R-CW-41, R-CW-42, R-CW-43, R-CW-47):
//   - the CW page: each control writes its setting and follows it; Break-in
//     steps Off, Semi and QSK and skips an unavailable QSK with its reason;
//     Delay greys in Off and QSK with the reason; the pitch arrows step
//     10 Hz within 200 to 2250; the KEY lamp follows dot, dash and key-out;
//     the state line says ready, off with the reason, or the refusal words;
//   - the TX applet in CW: VOX, LEV, EQ, CFC and TX BW greyed with the
//     reason; leaving CW gives each control back its state; MON and its
//     level show and set cwSidetone and monitorVolume;
//   - Setup > DSP > CW: ranges and defaults, no placeholders, the hang
//     readout, the key-down note, the radio sidetone on the Hermes Lite 2.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the desktop CW screens (Task 12). J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItemModel>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/CwTypes.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/codec/CwWireState.h"
#include "gui/applets/PhoneCwApplet.h"
#include "gui/applets/TxApplet.h"
#include "gui/applets/TxChainSection.h"
#include "gui/applets/TxFilterSection.h"
#include "gui/applets/TxMonitorSection.h"
#include "gui/applets/TxVoxSection.h"
#include "gui/setup/DspSetupPages.h"
#include "gui/widgets/TriBtn.h"
#include "models/CwKeyerController.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

class CwPageConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit CwPageConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
    void reportKey(bool dot, bool dash, bool keyOut) { emit radioKeyStatus(dot, dash, keyOut); }
};

void pump(int passes = 6)
{
    for (int i = 0; i < passes; ++i) {
        QCoreApplication::processEvents();
    }
}

RadioInfo radioInfo(ProtocolVersion protocol, HPSDRHW board, int firmware)
{
    RadioInfo info;
    info.protocol = protocol;
    info.boardType = board;
    info.firmwareVersion = firmware;
    return info;
}

// A radio on a fake connection with one CW slice in band, so the keyer is
// allowed. An Orion MkII on Protocol 2 offers QSK; a Hermes on Protocol 1
// does not.
struct Rig {
    RadioModel model;
    CwPageConnection conn;
    SliceModel* slice{nullptr};

    explicit Rig(bool qskRadio = true)
    {
        AppSettings::instance().clear();
        model.setLastRadioInfoForTest(qskRadio
            ? radioInfo(ProtocolVersion::Protocol2, HPSDRHW::OrionMKII, 22)
            : radioInfo(ProtocolVersion::Protocol1, HPSDRHW::Hermes, 32));
        model.injectConnectionForTest(&conn);
        model.wireCwKeyerConnectionForTest();
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        slice = model.sliceById(model.addSlice());
        slice->setFrequency(14'050'000.0);
        slice->setDspMode(DSPMode::CWU);
        pump();
    }
    ~Rig()
    {
        model.injectConnectionForTest(nullptr);
        AppSettings::instance().clear();
    }
    CwKeyerController* keyer() { return model.cwKeyerController(); }
    void key(bool dot, bool dash, bool keyOut)
    {
        conn.reportKey(dot, dash, keyOut);
        pump();
    }
};

template <typename T>
T* child(QObject& root, const char* name)
{
    return root.findChild<T*>(QLatin1String(name));
}

bool lit(const QLabel* lamp)
{
    return lamp && lamp->property("lit").toBool();
}

} // namespace

class TestCwPage : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("cw-page-%1")
                                            .arg(QCoreApplication::applicationPid()));
    }
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── CW page ───────────────────────────────────────────────────────────

    void everyControlHasItsName()
    {
        Rig rig;
        PhoneCwApplet applet(&rig.model);
        QVERIFY(child<QSlider>(applet, "cwDelaySlider"));
        QVERIFY(child<QSlider>(applet, "cwSpeedSlider"));
        QVERIFY(child<QPushButton>(applet, "cwSidetoneButton"));
        QVERIFY(child<QSlider>(applet, "cwSidetoneSlider"));
        QVERIFY(child<QSlider>(applet, "cwPanSlider"));
        QVERIFY(child<QPushButton>(applet, "cwBreakInButton"));
        QVERIFY(child<QPushButton>(applet, "cwIambicButton"));
        QVERIFY(child<QLabel>(applet, "cwPitchValue"));
        QVERIFY(child<QLabel>(applet, "cwKeyLamp"));
        QVERIFY(child<QLabel>(applet, "cwTxLamp"));
        QVERIFY(child<QLabel>(applet, "cwStatusLine"));
        // R-CW-41: no ALC gauge, and the placeholder text is gone.
        for (const QLabel* label : applet.findChildren<QLabel*>()) {
            QVERIFY2(!label->text().contains(QStringLiteral("does not send CW")),
                     qPrintable(label->text()));
        }
    }

    void controlsWriteTheirSettings()
    {
        Rig rig;
        TransmitModel& tx = rig.model.transmitModel();
        PhoneCwApplet applet(&rig.model);
        auto* delay = child<QSlider>(applet, "cwDelaySlider");
        auto* speed = child<QSlider>(applet, "cwSpeedSlider");
        auto* sidetone = child<QPushButton>(applet, "cwSidetoneButton");
        auto* level = child<QSlider>(applet, "cwSidetoneSlider");
        auto* iambic = child<QPushButton>(applet, "cwIambicButton");

        // Defaults as R-CW-01, and Speed runs 1 to 60.
        QCOMPARE(delay->value(), 300);
        QCOMPARE(delay->minimum(), 0);
        QCOMPARE(delay->maximum(), 1000);
        QCOMPARE(speed->value(), 25);
        QCOMPARE(speed->minimum(), 1);
        QCOMPARE(speed->maximum(), 60);
        QVERIFY(sidetone->isChecked());
        QVERIFY(iambic->isChecked());

        delay->setValue(450);
        QCOMPARE(tx.cwBreakInDelayMs(), 450);
        speed->setValue(32);
        QCOMPARE(tx.cwKeyerSpeed(), 32);
        level->setValue(70);
        QCOMPARE(tx.monitorVolume(), 0.70f);
        sidetone->click();
        QVERIFY(!tx.cwSidetone());
        // Iambic off is the straight key; on again keeps the iambic letter.
        tx.setCwKeyerMode(static_cast<int>(CwKeyerMode::IambicA));
        iambic->click();
        QCOMPARE(tx.cwKeyerMode(), static_cast<int>(CwKeyerMode::Straight));
        iambic->click();
        QCOMPARE(tx.cwKeyerMode(), static_cast<int>(CwKeyerMode::IambicA));
    }

    void controlsFollowTheirSettingsWithoutWritingBack()
    {
        Rig rig;
        TransmitModel& tx = rig.model.transmitModel();
        PhoneCwApplet applet(&rig.model);
        QSignalSpy speedWrites(&tx, &TransmitModel::cwKeyerSpeedChanged);
        tx.setCwKeyerSpeed(40);
        QCOMPARE(child<QSlider>(applet, "cwSpeedSlider")->value(), 40);
        QCOMPARE(speedWrites.count(), 1);   // the page wrote nothing back
        tx.setCwBreakInDelayMs(120);
        QCOMPARE(child<QSlider>(applet, "cwDelaySlider")->value(), 120);
        tx.setCwSidetone(false);
        QVERIFY(!child<QPushButton>(applet, "cwSidetoneButton")->isChecked());
        // The level greys with Sidetone off, with the reason.
        auto* level = child<QSlider>(applet, "cwSidetoneSlider");
        QVERIFY(!level->isEnabled());
        QVERIFY(OperatorWording::isPlain(level->toolTip()));
        QCOMPARE(level->accessibleDescription(), level->toolTip());
        tx.setMonitorVolume(0.25f);
        QCOMPARE(level->value(), 25);
        tx.setCwKeyerMode(static_cast<int>(CwKeyerMode::Straight));
        QVERIFY(!child<QPushButton>(applet, "cwIambicButton")->isChecked());
        rig.model.setCwPitchHz(800);
        QCOMPARE(child<QLabel>(applet, "cwPitchValue")->text(), QStringLiteral("800"));
    }

    void pitchArrowsStepTenHertzWithinTheRange()
    {
        Rig rig;
        PhoneCwApplet applet(&rig.model);
        auto* pitch = child<QLabel>(applet, "cwPitchValue");
        auto* down = child<TriBtn>(applet, "cwPitchDownButton");
        auto* up = child<TriBtn>(applet, "cwPitchUpButton");
        QVERIFY(down && up);
        rig.model.setCwPitchHz(600);
        up->click();
        QCOMPARE(rig.model.cwPitchHz(), 610);
        QCOMPARE(pitch->text(), QStringLiteral("610"));
        down->click();
        down->click();
        QCOMPARE(rig.model.cwPitchHz(), 590);
        rig.model.setCwPitchHz(kCwPitchMaxHz);
        up->click();
        QCOMPARE(rig.model.cwPitchHz(), 2250);
        rig.model.setCwPitchHz(kCwPitchMinHz);
        down->click();
        QCOMPARE(rig.model.cwPitchHz(), 200);
    }

    void breakInStepsOffSemiQsk()
    {
        Rig rig(/*qskRadio=*/true);
        TransmitModel& tx = rig.model.transmitModel();
        PhoneCwApplet applet(&rig.model);
        auto* bk = child<QPushButton>(applet, "cwBreakInButton");
        QVERIFY(rig.keyer()->qskAvailable());
        QCOMPARE(bk->text(), QStringLiteral("Semi BK"));
        bk->click();
        pump();
        QCOMPARE(tx.cwBreakInMode(), static_cast<int>(CwBreakInMode::Qsk));
        QCOMPARE(bk->text(), QStringLiteral("QSK"));
        bk->click();
        pump();
        QCOMPARE(tx.cwBreakInMode(), static_cast<int>(CwBreakInMode::Off));
        QCOMPARE(bk->text(), QStringLiteral("BK Off"));
        QVERIFY(!bk->isChecked());
        bk->click();
        pump();
        QCOMPARE(tx.cwBreakInMode(), static_cast<int>(CwBreakInMode::Semi));
    }

    void breakInSkipsAnUnavailableQskAndSaysWhy()
    {
        Rig rig(/*qskRadio=*/false);
        TransmitModel& tx = rig.model.transmitModel();
        PhoneCwApplet applet(&rig.model);
        auto* bk = child<QPushButton>(applet, "cwBreakInButton");
        auto* line = child<QLabel>(applet, "cwStatusLine");
        QVERIFY(!rig.keyer()->qskAvailable());
        const QString why = rig.keyer()->qskUnavailableReason();
        QVERIFY(!why.isEmpty());
        QVERIFY2(bk->toolTip().contains(why), qPrintable(bk->toolTip()));
        QVERIFY2(line->text().contains(why), qPrintable(line->text()));
        bk->click();   // Semi to Off, past QSK
        pump();
        QCOMPARE(tx.cwBreakInMode(), static_cast<int>(CwBreakInMode::Off));
        bk->click();
        pump();
        QCOMPARE(tx.cwBreakInMode(), static_cast<int>(CwBreakInMode::Semi));
    }

    void delayGreysInOffAndQskWithTheReason()
    {
        Rig rig;
        TransmitModel& tx = rig.model.transmitModel();
        PhoneCwApplet applet(&rig.model);
        auto* delay = child<QSlider>(applet, "cwDelaySlider");
        QVERIFY(delay->isEnabled());
        tx.setCwBreakInMode(static_cast<int>(CwBreakInMode::Off));
        pump();
        QVERIFY(!delay->isEnabled());
        const QString offWhy = delay->toolTip();
        QVERIFY(OperatorWording::isPlain(offWhy));
        QCOMPARE(delay->accessibleDescription(), offWhy);
        tx.setCwBreakInMode(static_cast<int>(CwBreakInMode::Qsk));
        pump();
        QCOMPARE(rig.keyer()->effectiveBreakInMode(), CwBreakInMode::Qsk);
        QVERIFY(!delay->isEnabled());
        QVERIFY(delay->toolTip() != offWhy);
        QVERIFY(OperatorWording::isPlain(delay->toolTip()));
        tx.setCwBreakInMode(static_cast<int>(CwBreakInMode::Semi));
        pump();
        QVERIFY(delay->isEnabled());
    }

    void keyLampFollowsDotDashAndKeyOut()
    {
        Rig rig;
        PhoneCwApplet applet(&rig.model);
        auto* key = child<QLabel>(applet, "cwKeyLamp");
        auto* tx = child<QLabel>(applet, "cwTxLamp");
        QVERIFY(!lit(key));
        rig.key(true, false, false);
        QVERIFY(lit(key));
        rig.key(false, false, false);
        QVERIFY(!lit(key));
        rig.key(false, true, false);
        QVERIFY(lit(key));
        rig.key(false, false, true);
        QVERIFY(lit(key));
        QVERIFY(lit(tx));   // key-out: the radio transmits (QSK)
        rig.key(false, false, false);
        QVERIFY(!lit(key));
        QVERIFY(!lit(tx));
    }

    void stateLineSaysReadyOffOrTheRefusal()
    {
        Rig rig;
        PhoneCwApplet applet(&rig.model);
        auto* line = child<QLabel>(applet, "cwStatusLine");
        QVERIFY2(rig.keyer()->keyerAllowed(), qPrintable(rig.keyer()->refusal().text));
        QVERIFY2(line->text().startsWith(QStringLiteral("Keyer ready.")), qPrintable(line->text()));
        QVERIFY(line->text().contains(QStringLiteral("300 ms")));

        // Out of CW: the keyer is off, with the reason.
        rig.slice->setDspMode(DSPMode::USB);
        pump();
        QVERIFY(!rig.keyer()->keyerAllowed());
        const QString words = rig.keyer()->refusal().text;
        QCOMPARE(line->text(), QStringLiteral("Keyer off. ") + words);
        QCOMPARE(child<QLabel>(applet, "cwKeyerLamp")->text(), QStringLiteral("KEYER OFF"));

        // A paddle pressed while refused: the refusal words; KEY lit, TX dark.
        rig.key(true, false, false);
        QCOMPARE(line->text(), words);
        QVERIFY(lit(child<QLabel>(applet, "cwKeyLamp")));
        QVERIFY(!lit(child<QLabel>(applet, "cwTxLamp")));
        rig.key(false, false, false);
        QCOMPARE(line->text(), QStringLiteral("Keyer off. ") + words);
        for (const QString& text : {line->text(), words}) {
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
        }
    }

    void panIsShownDisabledWithItsReason()
    {
        Rig rig;
        PhoneCwApplet applet(&rig.model);
        auto* pan = child<QSlider>(applet, "cwPanSlider");
        QVERIFY(!pan->isHidden());
        QVERIFY(!pan->isEnabled());
        QVERIFY(OperatorWording::isPlain(pan->toolTip()));
        QCOMPARE(pan->accessibleDescription(), pan->toolTip());
    }

    void remoteCwSettingsGateGreysWithTheReason()
    {
        Rig rig;
        PhoneCwApplet applet(&rig.model);
        const QString why = QStringLiteral("Update the Core to change CW settings here.");
        applet.setCwSettingsPermitted(false, why);
        for (const char* name : {"cwDelaySlider", "cwSpeedSlider", "cwSidetoneSlider"}) {
            auto* control = child<QSlider>(applet, name);
            QVERIFY2(!control->isEnabled(), name);
            QCOMPARE(control->toolTip(), why);
        }
        for (const char* name : {"cwSidetoneButton", "cwBreakInButton", "cwIambicButton"}) {
            auto* control = child<QPushButton>(applet, name);
            QVERIFY2(!control->isEnabled(), name);
            QCOMPARE(control->accessibleDescription(), why);
        }
        applet.setCwSettingsPermitted(true);
        QVERIFY(child<QSlider>(applet, "cwSpeedSlider")->isEnabled());
        QVERIFY(child<QSlider>(applet, "cwSpeedSlider")->toolTip() != why);
    }

    // ── TX applet in CW (R-CW-42) ─────────────────────────────────────────

    void txAppletGreysTheMicrophoneChainInCwAndGivesItBack()
    {
        Rig rig;
        rig.slice->setDspMode(DSPMode::USB);
        TxApplet applet(&rig.model);
        auto* lev = child<QPushButton>(applet, "TxLevButton");
        auto* eq = child<QPushButton>(applet, "TxEqButton");
        auto* cfc = child<QPushButton>(applet, "TxCfcButton");
        QVERIFY(lev && eq && cfc && applet.voxButton());
        auto* filter = applet.findChild<TxFilterSection*>();
        QVERIFY(filter);
        QList<QWidget*> chain{applet.voxButton(), lev, eq, cfc, filter->filterLowSpin(),
                              filter->filterHighSpin()};
        QList<QString> tips;
        QList<bool> enabled;
        for (QWidget* w : chain) {
            tips << w->toolTip();
            enabled << w->isEnabled();
        }
        rig.slice->setDspMode(DSPMode::CWU);
        QVERIFY(applet.cwLayerActiveForTest());
        for (QWidget* w : chain) {
            QVERIFY2(!w->isEnabled(), qPrintable(w->objectName()));
            QCOMPARE(w->toolTip(), QStringLiteral("Not used in CW: the radio makes the signal."));
            QCOMPARE(w->accessibleDescription(), TxApplet::cwNotUsedReason());
            QVERIFY(!w->isHidden());
        }
        rig.slice->setDspMode(DSPMode::LSB);
        for (int i = 0; i < chain.size(); ++i) {
            QCOMPARE(chain[i]->isEnabled(), enabled[i]);
            QCOMPARE(chain[i]->toolTip(), tips[i]);
        }
    }

    void txAppletCwLayerStacksWithTheRemoteLayer()
    {
        Rig rig;
        rig.slice->setDspMode(DSPMode::USB);
        TxApplet applet(&rig.model);
        auto* lev = child<QPushButton>(applet, "TxLevButton");
        const QString levTip = lev->toolTip();
        const QString remoteWhy = QStringLiteral("The Core is on the air.");
        rig.slice->setDspMode(DSPMode::CWL);
        // A layer below changes while CW holds the button.
        applet.setTransmitChainSettingsPermitted(false, remoteWhy);
        QCOMPARE(lev->toolTip(), TxApplet::cwNotUsedReason());
        rig.slice->setDspMode(DSPMode::USB);
        QVERIFY(!lev->isEnabled());
        QCOMPARE(lev->toolTip(), remoteWhy);
        applet.setTransmitChainSettingsPermitted(true);
        QVERIFY(lev->isEnabled());
        QCOMPARE(lev->toolTip(), levTip);
    }

    void monShowsAndSetsTheCwSidetoneInCw()
    {
        Rig rig;
        rig.slice->setDspMode(DSPMode::USB);
        TransmitModel& tx = rig.model.transmitModel();
        TxApplet applet(&rig.model);
        auto* mon = applet.findChild<TxMonitorSection*>();
        QVERIFY(mon && mon->monButton() && mon->volumeSlider());
        QVERIFY(!tx.monEnabled());
        QVERIFY(tx.cwSidetone());
        QVERIFY(!mon->monButton()->isChecked());
        rig.slice->setDspMode(DSPMode::CWU);
        QVERIFY(mon->monButton()->isChecked());   // cwSidetone
        mon->monButton()->click();
        QVERIFY(!tx.cwSidetone());
        QVERIFY(!tx.monEnabled());
        tx.setCwSidetone(true);
        QVERIFY(mon->monButton()->isChecked());
        mon->volumeSlider()->setValue(30);
        QCOMPARE(tx.monitorVolume(), 0.30f);
        tx.setMonitorVolume(0.6f);
        QCOMPARE(mon->volumeSlider()->value(), 60);
        // Out of CW, MON is MON again.
        rig.slice->setDspMode(DSPMode::USB);
        QVERIFY(!mon->monButton()->isChecked());
        mon->monButton()->click();
        QVERIFY(tx.monEnabled());
        QVERIFY(tx.cwSidetone());
    }

    // ── Setup > DSP > CW (R-CW-43) ────────────────────────────────────────

    void setupHasTheRangesAndDefaultsAndNoPlaceholders()
    {
        Rig rig;
        CwSetupPage page(&rig.model);
        for (const QLabel* label : page.findChildren<QLabel*>()) {
            for (const char* gone : {"Letter Spacing", "Dot-Dash Ratio"}) {
                QVERIFY2(!label->text().contains(QLatin1String(gone)), gone);
            }
        }
        for (const QComboBox* combo : page.findChildren<QComboBox*>()) {
            QCOMPARE(combo->findText(QStringLiteral("Bug")), -1);
        }
        auto* mode = child<QComboBox>(page, "cwKeyerModeCombo");
        QCOMPARE(mode->count(), 3);
        QCOMPARE(mode->currentData().toInt(), static_cast<int>(CwKeyerMode::IambicB));
        auto* weight = child<QSpinBox>(page, "cwKeyerWeightSpin");
        QCOMPARE(weight->minimum(), 33);
        QCOMPARE(weight->maximum(), 66);
        QCOMPARE(weight->value(), 50);
        auto* speed = child<QSlider>(page, "cwKeyerSpeedSlider");
        QCOMPARE(speed->minimum(), 1);
        QCOMPARE(speed->maximum(), 60);
        QCOMPARE(speed->value(), 25);
        auto* pitch = child<QSlider>(page, "cwPitchSlider");
        QCOMPARE(pitch->minimum(), 200);
        QCOMPARE(pitch->maximum(), 2250);
        auto* keyDown = child<QSpinBox>(page, "cwKeyDownDelaySpin");
        QCOMPARE(keyDown->maximum(), 255);
        QCOMPARE(keyDown->value(), 7);
        auto* keyUp = child<QSpinBox>(page, "cwKeyUpDelaySpin");
        QCOMPARE(keyUp->maximum(), 500);
        QCOMPARE(keyUp->value(), 10);
        auto* delay = child<QSlider>(page, "cwBreakInDelaySlider");
        QCOMPARE(delay->maximum(), 1000);
        QCOMPARE(delay->value(), 300);
        auto* ret = child<QSpinBox>(page, "cwAutoModeReturnMsSpin");
        QCOMPARE(ret->minimum(), 1);
        QCOMPARE(ret->maximum(), 10000);
        QCOMPARE(ret->singleStep(), 10);
        QCOMPARE(ret->value(), 2000);
        QVERIFY(child<QCheckBox>(page, "cwRadioSidetoneCheck")->isChecked());
        QVERIFY(child<QCheckBox>(page, "cwComputerSidetoneCheck")->isChecked());
        QVERIFY(child<QCheckBox>(page, "cwComputerSidetoneCheck")->text()
                    .contains(QStringLiteral("serial")));
    }

    void setupControlsWriteAndFollowTheirSettings()
    {
        Rig rig;
        TransmitModel& tx = rig.model.transmitModel();
        CwSetupPage page(&rig.model);
        auto* mode = child<QComboBox>(page, "cwKeyerModeCombo");
        const int straight = mode->findData(static_cast<int>(CwKeyerMode::Straight));
        mode->setCurrentIndex(straight);
        emit mode->activated(straight);
        QCOMPARE(tx.cwKeyerMode(), static_cast<int>(CwKeyerMode::Straight));
        child<QSpinBox>(page, "cwKeyerWeightSpin")->setValue(40);
        QCOMPARE(tx.cwKeyerWeight(), 40);
        child<QCheckBox>(page, "cwReversePaddlesCheck")->setChecked(true);
        QVERIFY(tx.cwReversePaddles());
        child<QCheckBox>(page, "cwStrictSpacingCheck")->setChecked(true);
        QVERIFY(tx.cwStrictSpacing());
        child<QSpinBox>(page, "cwKeyUpDelaySpin")->setValue(25);
        QCOMPARE(tx.cwKeyUpDelayMs(), 25);
        child<QCheckBox>(page, "cwComputerSidetoneCheck")->setChecked(false);
        QVERIFY(!tx.cwComputerSidetone());
        child<QSlider>(page, "cwPitchSlider")->setValue(704);
        QCOMPARE(rig.model.cwPitchHz(), 700);

        tx.setCwKeyerSpeed(18);
        QCOMPARE(child<QSlider>(page, "cwKeyerSpeedSlider")->value(), 18);
        tx.setCwRadioSidetone(false);
        QVERIFY(!child<QCheckBox>(page, "cwRadioSidetoneCheck")->isChecked());

        // Options: the return follows the switch, greyed with the reason.
        auto* ret = child<QCheckBox>(page, "cwAutoModeReturnCheck");
        QVERIFY(!ret->isEnabled());
        QVERIFY(OperatorWording::isPlain(ret->toolTip()));
        child<QCheckBox>(page, "cwAutoModeSwitchCheck")->setChecked(true);
        QVERIFY(tx.cwAutoModeSwitch());
        QVERIFY(ret->isEnabled());
        // Serial lines need a port.
        auto* paddles = child<QCheckBox>(page, "cwSerialPaddlesCheck");
        QVERIFY(!paddles->isEnabled());
        QVERIFY(OperatorWording::isPlain(paddles->toolTip()));
    }

    void setupHangReadoutFollowsDelayAndKeyUp()
    {
        Rig rig;
        TransmitModel& tx = rig.model.transmitModel();
        CwSetupPage page(&rig.model);
        auto* hang = child<QLabel>(page, "cwRadioHangTime");
        QVERIFY2(hang->text().startsWith(QStringLiteral("310 ms")), qPrintable(hang->text()));
        tx.setCwBreakInDelayMs(500);
        QVERIFY2(hang->text().startsWith(QStringLiteral("510 ms")), qPrintable(hang->text()));
        tx.setCwKeyUpDelayMs(40);
        QVERIFY2(hang->text().startsWith(QStringLiteral("540 ms")), qPrintable(hang->text()));
        tx.setCwBreakInDelayMs(1000);
        tx.setCwKeyUpDelayMs(500);
        QVERIFY2(hang->text().startsWith(QStringLiteral("1023 ms")), qPrintable(hang->text()));
        tx.setCwBreakInMode(static_cast<int>(CwBreakInMode::Off));
        pump();
        QVERIFY2(hang->text().startsWith(QStringLiteral("0 ms")), qPrintable(hang->text()));
        // Delay greys in Off with the reason.
        auto* delay = child<QSlider>(page, "cwBreakInDelaySlider");
        QVERIFY(!delay->isEnabled());
        QVERIFY(OperatorWording::isPlain(delay->toolTip()));
        tx.setCwBreakInMode(static_cast<int>(CwBreakInMode::Qsk));
        pump();
        QVERIFY2(hang->text().startsWith(QStringLiteral("500 ms")), qPrintable(hang->text()));
        QVERIFY(!delay->isEnabled());
    }

    void setupKeyDownNoteShowsTheSpeedClamp()
    {
        Rig rig;
        TransmitModel& tx = rig.model.transmitModel();
        CwSetupPage page(&rig.model);
        auto* note = child<QLabel>(page, "cwKeyDownNote");
        QVERIFY(!note->text().contains(QStringLiteral("WPM")));
        tx.setCwKeyerSpeed(30);
        tx.setCwKeyDownDelayMs(50);
        QVERIFY2(note->text().contains(QStringLiteral("At 30 WPM the radio uses at most 30 ms.")),
                 qPrintable(note->text()));
        tx.setCwKeyDownDelayMs(30);
        QVERIFY(!note->text().contains(QStringLiteral("WPM")));
        QCOMPARE(CwSetupPage::keyDownClampNote(255, 60),
                 QStringLiteral("At 60 WPM the radio uses at most 15 ms."));
    }

    void setupUnavailableQskIsGreyedInTheListWithItsReason()
    {
        Rig rig(/*qskRadio=*/false);
        CwSetupPage page(&rig.model);
        auto* mode = child<QComboBox>(page, "cwBreakInModeCombo");
        const int qsk = mode->findData(static_cast<int>(CwBreakInMode::Qsk));
        QVERIFY(qsk >= 0);
        auto* items = qobject_cast<QStandardItemModel*>(mode->model());
        QVERIFY(!(items->item(qsk)->flags() & Qt::ItemIsEnabled));
        QVERIFY(mode->itemText(qsk).contains(rig.keyer()->qskUnavailableReason()));
        QCOMPARE(mode->toolTip(), rig.keyer()->qskUnavailableReason());
    }

    void setupHl2RadioSidetoneIsDisabledNotHiddenWithTheNotes()
    {
        Rig rig;
        rig.model.setLastRadioInfoForTest(
            radioInfo(ProtocolVersion::Protocol1, HPSDRHW::HermesLite, 72));
        CwSetupPage page(&rig.model);
        // The two Hermes Lite 2 notes show on the HL2 (R-CW-15, R-CW-40).
        QVERIFY(!child<QLabel>(page, "cwKeyerHl2Note")->isHidden());
        QVERIFY(!child<QLabel>(page, "cwSidetoneHl2Note")->isHidden());
        auto* radio = child<QCheckBox>(page, "cwRadioSidetoneCheck");
        QVERIFY(!radio->isHidden());
        QVERIFY(!page.radioSidetoneEnabledForTest());
        QVERIFY(OperatorWording::isPlain(radio->toolTip()));
        QCOMPARE(radio->accessibleDescription(), radio->toolTip());
        page.setHasSidetoneGenerator(true);
        QVERIFY(page.radioSidetoneEnabledForTest());
        QVERIFY(radio->toolTip().isEmpty());

        // Another radio: no notes, the radio sidetone enabled.
        rig.model.setLastRadioInfoForTest(
            radioInfo(ProtocolVersion::Protocol2, HPSDRHW::OrionMKII, 22));
        CwSetupPage orion(&rig.model);
        QVERIFY(child<QLabel>(orion, "cwKeyerHl2Note")->isHidden());
        QVERIFY(child<QLabel>(orion, "cwSidetoneHl2Note")->isHidden());
        QVERIFY(orion.radioSidetoneEnabledForTest());
    }
};

QTEST_MAIN(TestCwPage)
#include "tst_cw_page.moc"
