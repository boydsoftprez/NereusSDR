// =================================================================
// tests/tst_setup_controls_connected.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. It drives real Setup pages
// and a real MainWindow; no upstream logic is ported here.
//
// R3 controls that work, Task 2 (R-R3-21): Setup controls that front an
// existing setting or slice control now reach it: Startup & Preferences
// auto-connect, callsign and grid; DSP > NR/ANF Enable ANF, DSP > CW peak
// filter, DSP > AM/SAM and FM squelch; Appearance > Meter Styles S-meter
// group; Diagnostics > Logs; Audio > VAX level readout.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  R3 controls that work, Task 2.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-10-15  J.J. Boyd / KG4VCF  DSP > FM squelch readout shows the
//                                    0-100 number, AM keeps dB.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-26: CW transmit, the APF Center Freq range and value follow
//               the CW pitch (R-CW-27). J.J. Boyd (KG4VCF), AI-assisted via
//               Claude Code.
//   2026-10-26: CW transmit, the DSP > CW transmit controls and their
//               version 16 gate (R-CW-43). J.J. Boyd (KG4VCF), AI-assisted
//               via Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QPointer>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/BuildIdentity.h"
#include "core/RadioDiscovery.h"
#include "core/session/IStationLink.h"
#include "gui/GuiSessionCoordinator.h"
#include "gui/MainWindow.h"
#include "gui/SMeterWidget.h"
#include "gui/SetupDialog.h"
#include "gui/SupportDialog.h"
#include "gui/diagnostics/DiagnosticsPhaseHPages.h"
#include "gui/setup/AppearanceSetupPages.h"
#include "gui/setup/AudioVaxPage.h"
#include "gui/setup/DspSetupPages.h"
#include "gui/setup/GeneralSetupPages.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

SliceModel* ensureSlice(RadioModel& model)
{
    if (model.activeSlice() == nullptr) {
        model.addSlice();
        model.setActiveSlice(0);
    }
    return model.activeSlice();
}

QAction* actionByText(const QObject* root, const QString& text)
{
    for (QAction* action : root->findChildren<QAction*>()) {
        if (action->text() == text) { return action; }
    }
    return nullptr;
}

RadioInfo savedRadioInfo()
{
    RadioInfo info;
    info.macAddress = QStringLiteral("00:1c:c0:a2:13:dd");
    info.address = QHostAddress(QStringLiteral("192.168.1.20"));
    info.boardType = HPSDRHW::OrionMKII;
    info.protocol = ProtocolVersion::Protocol2;
    return info;
}

} // namespace

class TstSetupControlsConnected : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("setup-controls-connected-%1")
                                            .arg(QCoreApplication::applicationPid()));
    }

    void init()
    {
        QVERIFY(!AppSettings::instance().remoteBackend());
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("audio/FirstRunComplete"),
                                         QStringLiteral("True"));
        RadioDiscovery::clearHoldOffForTest();
        RadioDiscovery discovery;
        discovery.holdOffScans(std::chrono::minutes{5});
        BuildIdentity::setBuildTag(QString());
    }

    void cleanup()
    {
        RadioDiscovery::clearHoldOffForTest();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // Auto-connect to last radio is that radio's own auto-connect choice,
    // the one the Connection panel sets.
    void autoConnectSetsTheLastRadiosChoice()
    {
        auto& s = AppSettings::instance();
        const RadioInfo info = savedRadioInfo();
        s.saveRadio(info, /*pinToMac=*/true, /*autoConnect=*/false);
        s.setLastConnected(info.macAddress);
        // When the radio was last seen is not the toggle's to change.
        const QString lastSeenKey =
            QStringLiteral("radios/%1/lastSeen").arg(info.macAddress);
        const QString lastSeen = QStringLiteral("2020-01-02T03:04:05Z");
        s.setValue(lastSeenKey, lastSeen);

        StartupPrefsPage page(nullptr);
        auto* toggle = page.findChild<QPushButton*>(QStringLiteral("startupAutoConnect"));
        QVERIFY2(toggle != nullptr, "no auto-connect toggle");
        QVERIFY2(toggle->isEnabled(), "the auto-connect toggle is greyed");
        QVERIFY(!toggle->isChecked());
        toggle->setChecked(true);
        const auto saved = s.savedRadio(info.macAddress);
        QVERIFY(saved.has_value());
        QVERIFY(saved->autoConnect);
        QVERIFY(saved->pinToMac);  // the rest of the entry is kept
        QCOMPARE(s.value(lastSeenKey).toString(), lastSeen);
    }

    // With no radio to connect to, it says why it is off.
    void autoConnectWithoutARadioSaysWhy()
    {
        StartupPrefsPage page(nullptr);
        auto* toggle = page.findChild<QPushButton*>(QStringLiteral("startupAutoConnect"));
        QVERIFY(toggle != nullptr);
        QVERIFY(!toggle->isEnabled());
        QVERIFY(OperatorWording::isPlain(toggle->toolTip()));
        QVERIFY(!toggle->toolTip().contains(QStringLiteral("NYI")));
    }

    // Callsign and grid are the operator identity the Spot Hub edits.
    void callsignAndGridAreTheSpotIdentity()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("User/Callsign"), QStringLiteral("N0CALL"));
        s.setValue(QStringLiteral("User/GridSquare"), QStringLiteral("FN31"));
        RadioModel model;
        StartupPrefsPage page(&model);
        auto* call = page.findChild<QLineEdit*>(QStringLiteral("startupCallsign"));
        auto* grid = page.findChild<QLineEdit*>(QStringLiteral("startupGridSquare"));
        auto* error = page.findChild<QLabel*>(QStringLiteral("startupIdentityError"));
        QVERIFY2(call && grid && error, "no identity fields");
        QVERIFY(call->isEnabled());
        QVERIFY(grid->isEnabled());
        QCOMPARE(call->text(), QStringLiteral("N0CALL"));
        QCOMPARE(grid->text(), QStringLiteral("FN31"));

        grid->setText(QStringLiteral("EM7"));
        emit grid->editingFinished();
        QVERIFY(!error->isHidden());
        QCOMPARE(s.value(QStringLiteral("User/GridSquare")).toString(), QStringLiteral("FN31"));

        call->setText(QStringLiteral("kg4vcf"));
        grid->setText(QStringLiteral("em73"));
        emit call->editingFinished();
        QVERIFY(error->isHidden());
        QCOMPARE(s.value(QStringLiteral("User/Callsign")).toString(), QStringLiteral("KG4VCF"));
        QCOMPARE(s.value(QStringLiteral("User/GridSquare")).toString(), QStringLiteral("EM73"));
        // The spot sources' own copies follow, as the Spot Hub saves them.
        QCOMPARE(s.value(QStringLiteral("PskReporter/Callsign")).toString(), QStringLiteral("KG4VCF"));
        QCOMPARE(s.value(QStringLiteral("DxClusterCallsign")).toString(), QStringLiteral("KG4VCF"));
        QCOMPARE(s.value(QStringLiteral("FreeDvReporter/GridSquare")).toString(),
                 QStringLiteral("EM73"));
    }

    // DSP > NR/ANF > ANF: Enable ANF is the slice's ANF, both ways.
    void enableAnfIsTheSlicesAnf()
    {
        RadioModel model;
        SliceModel* slice = ensureSlice(model);
        QVERIFY(slice != nullptr);
        NrAnfSetupPage page(&model);
        auto* anf = page.findChild<QCheckBox*>(QStringLiteral("anfEnableCheck"));
        QVERIFY2(anf != nullptr, "no Enable ANF control");
        QVERIFY2(anf->isEnabled(), "Enable ANF is greyed");
        anf->setChecked(true);
        QVERIFY(slice->anfEnabled());
        slice->setAnfEnabled(false);
        QVERIFY(!anf->isChecked());
    }

    // DSP > CW > APF: Enable and Center Freq are the slice's APF and its
    // tune offset from the 600 Hz pitch.
    void cwPeakFilterIsTheSlicesApf()
    {
        RadioModel model;
        SliceModel* slice = ensureSlice(model);
        CwSetupPage page(&model);
        auto* enable = page.findChild<QPushButton*>(QStringLiteral("apfEnableButton"));
        auto* center = page.findChild<QSlider*>(QStringLiteral("apfCenterSlider"));
        QVERIFY2(enable && center, "no APF controls");
        QVERIFY2(enable->isEnabled() && enable->isEnabledTo(&page), "APF Enable is greyed");
        QVERIFY2(center->isEnabledTo(&page), "APF Center Freq is greyed");
        enable->click();
        QVERIFY(slice->apfEnabled());
        center->setValue(700);
        QCOMPARE(slice->apfTuneHz(), 100);
        slice->setApfTuneHz(-50);
        QCOMPARE(center->value(), 550);
    }

    // CW transmit (R-CW-43): the DSP > CW transmit controls carry their
    // Setup ids, reach their settings both ways, and grey with the transmit
    // settings' reason when the CW settings are not taken (version 16).
    void cwTransmitControlsReachTheirSettings()
    {
        RadioModel model;
        ensureSlice(model);
        TransmitModel& tx = model.transmitModel();
        CwSetupPage page(&model);
        const auto byId = [&page](const char* id) -> QWidget* {
            for (QWidget* w : page.findChildren<QWidget*>()) {
                if (w->property("nereusSetupId").toString() == QLatin1String(id)) { return w; }
            }
            return nullptr;
        };
        const char* ids[] = {"dsp.cw.keyer.mode", "dsp.cw.keyer.weight", "dsp.cw.keyer.speed",
                             "dsp.cw.keyer.pitch", "dsp.cw.keyer.reversePaddles",
                             "dsp.cw.keyer.strictSpacing", "dsp.cw.breakIn.mode",
                             "dsp.cw.breakIn.delay", "dsp.cw.breakIn.keyDownDelay",
                             "dsp.cw.breakIn.keyUpDelay", "dsp.cw.sidetone.radio",
                             "dsp.cw.sidetone.computer", "dsp.cw.sidetone.level",
                             "dsp.cw.serial.port", "dsp.cw.serial.paddles",
                             "dsp.cw.serial.keyLine", "dsp.cw.serial.pttLine",
                             "dsp.cw.options.autoModeSwitch", "dsp.cw.options.autoModeReturn",
                             "dsp.cw.options.autoModeReturnMs"};
        for (const char* id : ids) {
            QVERIFY2(byId(id) != nullptr, id);
        }
        auto* speed = qobject_cast<QSlider*>(byId("dsp.cw.keyer.speed"));
        auto* mode = qobject_cast<QComboBox*>(byId("dsp.cw.keyer.mode"));
        QVERIFY(speed && mode);
        QVERIFY(speed->isEnabled());
        speed->setValue(42);
        QCOMPARE(tx.cwKeyerSpeed(), 42);
        tx.setCwKeyerSpeed(12);
        QCOMPARE(speed->value(), 12);
        tx.setCwKeyerMode(1);
        QCOMPARE(mode->currentData().toInt(), 1);

        page.setTransmitSettingsPermittedAt(16, false, QString());
        QVERIFY(!speed->isEnabled());
        QVERIFY(!speed->isHidden());
        QCOMPARE(speed->toolTip(), IStationLink::transmitSettingsUnavailableReason());
        QVERIFY(OperatorWording::isPlain(speed->toolTip()));
        // An older version's gate does not reach the CW settings.
        page.setTransmitSettingsPermittedAt(16, true, QString());
        page.setTransmitSettingsPermittedAt(3, false, QString());
        QVERIFY(speed->isEnabled());
    }

    // DSP > AM/SAM and DSP > FM: the squelch thresholds are the slice's.
    void amAndFmSquelchAreTheSlicesSquelch()
    {
        RadioModel model;
        SliceModel* slice = ensureSlice(model);
        {
            AmSamSetupPage page(&model);
            auto* am = page.findChild<QSlider*>(QStringLiteral("amSquelchThresholdSlider"));
            QVERIFY2(am != nullptr, "no AM squelch control");
            QVERIFY2(am->isEnabledTo(&page), "AM squelch is greyed");
            am->setValue(-80);
            QCOMPARE(slice->amsqThresh(), -80.0);
            slice->setAmsqThresh(-100.0);
            QCOMPARE(am->value(), -100);
        }
        {
            FmSetupPage page(&model);
            auto* fm = page.findChild<QSlider*>(QStringLiteral("fmSquelchThresholdSlider"));
            QVERIFY2(fm != nullptr, "no FM squelch control");
            QVERIFY2(fm->isEnabledTo(&page), "FM squelch is greyed");
            fm->setValue(-70);
            QCOMPARE(slice->fmsqThresh(), -70.0);
        }
    }

    // DSP > FM shows the squelch as Thetis's 0-100 number, with no unit
    // (console.cs:47296-47318 [v2.10.3.15]); DSP > AM/SAM still shows dB.
    void fmSquelchReadoutIsTheZeroToHundredNumber()
    {
        RadioModel model;
        SliceModel* slice = ensureSlice(model);
        {
            FmSetupPage page(&model);
            auto* fm = page.findChild<QSlider*>(QStringLiteral("fmSquelchThresholdSlider"));
            auto* value = page.findChild<QLabel*>(QStringLiteral("fmSquelchThresholdValue"));
            QVERIFY2(fm != nullptr && value != nullptr, "no FM squelch readout");
            const QList<QPair<double, QString>> cases{
                {-150.0, QStringLiteral("6")},
                {-80.0, QStringLiteral("50")},
                {0.0, QStringLiteral("100")},
                {-160.0, QStringLiteral("0")},
            };
            for (const auto& [slider, shown] : cases) {
                slice->setFmsqThresh(slider);
                QCOMPARE(value->text(), shown);
            }
            fm->setValue(-150);
            QCOMPARE(value->text(), QStringLiteral("6"));
            fm->setValue(0);
            QCOMPARE(value->text(), QStringLiteral("100"));
            QCOMPARE(slice->fmsqThresh(), 0.0);
        }
        {
            AmSamSetupPage page(&model);
            auto* am = page.findChild<QSlider*>(QStringLiteral("amSquelchThresholdSlider"));
            auto* value = page.findChild<QLabel*>(QStringLiteral("amSquelchThresholdValue"));
            QVERIFY2(am != nullptr && value != nullptr, "no AM squelch readout");
            slice->setAmsqThresh(-80.0);
            QCOMPARE(value->text(), QStringLiteral("-80 dB"));
            am->setValue(-100);
            QCOMPARE(value->text(), QStringLiteral("-100 dB"));
        }
    }

    // With Setup open, the DSP controls follow a change of active slice:
    // they show the new slice's values and change that slice only.
    void dspControlsFollowTheActiveSlice()
    {
        RadioModel model;
        SliceModel* first = ensureSlice(model);
        QVERIFY(first != nullptr);
        NrAnfSetupPage anfPage(&model);
        CwSetupPage cwPage(&model);
        AmSamSetupPage amPage(&model);
        FmSetupPage fmPage(&model);
        QPointer<QCheckBox> anf = anfPage.findChild<QCheckBox*>(QStringLiteral("anfEnableCheck"));
        auto* apf = cwPage.findChild<QPushButton*>(QStringLiteral("apfEnableButton"));
        auto* center = cwPage.findChild<QSlider*>(QStringLiteral("apfCenterSlider"));
        auto* am = amPage.findChild<QSlider*>(QStringLiteral("amSquelchThresholdSlider"));
        auto* fm = fmPage.findChild<QSlider*>(QStringLiteral("fmSquelchThresholdSlider"));
        QVERIFY(anf && apf && center && am && fm);

        const int second = model.addSlice();
        QVERIFY(second >= 0);
        model.setActiveSlice(second);
        QVERIFY(anf.isNull()); // NR tabs discard the prior receiver's gesture widgets.
        anf = anfPage.findChild<QCheckBox*>(QStringLiteral("anfEnableCheck"));
        QVERIFY(anf != nullptr);
        SliceModel* active = model.activeSlice();
        QVERIFY(active != nullptr && active != first);
        active->setAnfEnabled(true);
        active->setApfTuneHz(120);
        active->setAmsqThresh(-90.0);
        active->setFmsqThresh(-60.0);
        QVERIFY(anf->isChecked());
        QCOMPARE(center->value(), model.cwPitchHz() + 120);
        // R-CW-27: the range is the pitch plus or minus 500 and follows a
        // pitch change; the tune offset stays.
        QCOMPARE(center->minimum(), model.cwPitchHz() - 500);
        QCOMPARE(center->maximum(), model.cwPitchHz() + 500);
        model.setCwPitchHz(700);
        QCOMPARE(center->minimum(), 200);
        QCOMPARE(center->maximum(), 1200);
        QCOMPARE(center->value(), 820);
        QCOMPARE(active->apfTuneHz(), 120);
        center->setValue(750);
        QCOMPARE(active->apfTuneHz(), 50);
        model.setCwPitchHz(600);
        QCOMPARE(center->value(), 650);
        QCOMPARE(am->value(), -90);
        QCOMPARE(fm->value(), -60);

        const bool firstAnf = first->anfEnabled();
        const bool firstApf = first->apfEnabled();
        const double firstAm = first->amsqThresh();
        anf->setChecked(false);
        apf->click();
        am->setValue(-70);
        QVERIFY(!active->anfEnabled());
        QVERIFY(active->apfEnabled());
        QCOMPARE(active->amsqThresh(), -70.0);
        QCOMPARE(first->anfEnabled(), firstAnf);
        QCOMPARE(first->apfEnabled(), firstApf);
        QCOMPARE(first->amsqThresh(), firstAm);
    }

    // DSP > NB/SNB follows the active slice, and removing the slice it is
    // bound to while the page is open leaves nothing pointing at it: a later
    // change reaches the slice that is active then, not the removed one.
    void noiseBlankerPageSurvivesRemovingItsSlice()
    {
        RadioModel model;
        SliceModel* first = ensureSlice(model);
        QVERIFY(first != nullptr);
        const int second = model.addSlice();
        QVERIFY(second >= 0);
        model.setActiveSlice(second);
        SliceModel* bound = model.activeSlice();
        QVERIFY(bound != nullptr && bound != first);
        bound->setNb1Threshold(77);

        NbSnbSetupPage page(&model);
        auto* thresh = page.findChild<QSlider*>(QStringLiteral("nb1ThresholdSlider"));
        QVERIFY(thresh != nullptr);
        QCOMPARE(thresh->value(), 77);
        thresh->setValue(88);
        QCOMPARE(bound->nb1Threshold(), 88);
        bound->setNb1Threshold(99);        // a change made elsewhere shows
        QCOMPARE(thresh->value(), 99);

        QPointer<SliceModel> gone(bound);
        model.removeSlice(bound->sliceIndex());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(gone.isNull());
        SliceModel* now = model.activeSlice();
        thresh->setValue(123);             // must not touch the removed slice
        if (now) {
            QCOMPARE(now->nb1Threshold(), 123);
        } else {
            QVERIFY(!thresh->isEnabledTo(&page));
        }
    }

    // Appearance > Meter Styles: the S-meter group is the S-meter's own
    // face, peak hold and decay, saved under its keys and applied to the
    // S-meter on screen.
    void meterStylesSetTheSMeter()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* window = sessions.window();
        auto* sMeter = window->findChild<SMeterWidget*>();
        QVERIFY(sMeter != nullptr);
        QAction* settings = actionByText(window, QStringLiteral("&Settings..."));
        QVERIFY(settings != nullptr);
        settings->trigger();
        auto* dialog = window->findChild<SetupDialog*>();
        QVERIFY(dialog != nullptr);
        dialog->selectPage(QStringLiteral("Meter Styles"));

        auto* face = dialog->findChild<QComboBox*>(QStringLiteral("sMeterFaceCombo"));
        auto* peak = dialog->findChild<QCheckBox*>(QStringLiteral("sMeterPeakHoldCheck"));
        auto* decay = dialog->findChild<QComboBox*>(QStringLiteral("sMeterDecayCombo"));
        QVERIFY2(face && peak && decay, "no S-meter controls");
        QVERIFY(face->isEnabled() && peak->isEnabled() && decay->isEnabled());

        const int classic = static_cast<int>(SMeterWidget::FaceStyle::Classic);
        face->setCurrentIndex(face->findData(classic));
        QCOMPARE(static_cast<int>(sMeter->faceStyle()), classic);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("SMeter_FaceStyle")).toString(),
                 SMeterWidget::faceStyleKey(SMeterWidget::FaceStyle::Classic));

        peak->setChecked(!peak->isChecked());
        QCOMPARE(AppSettings::instance().value(QStringLiteral("PeakHoldEnabled")).toString(),
                 peak->isChecked() ? QStringLiteral("True") : QStringLiteral("False"));

        decay->setCurrentIndex(decay->findData(QStringLiteral("Slow")));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("PeakDecayRate")).toString(),
                 QStringLiteral("Slow"));

        // A change from the S-meter's right-click menu shows on the page
        // while Setup is open.
        sMeter->setFaceStyle(SMeterWidget::FaceStyle::AgedCream);
        QCOMPARE(face->currentData().toInt(),
                 static_cast<int>(SMeterWidget::FaceStyle::AgedCream));
        const bool peakWas = peak->isChecked();
        sMeter->setPeakHoldEnabled(!peakWas);
        QCOMPARE(peak->isChecked(), !peakWas);
        sMeter->setPeakDecayRate(QStringLiteral("Fast"));
        QCOMPARE(decay->currentData().toString(), QStringLiteral("Fast"));
        delete dialog;
        QVERIFY(sessions.replace({}, false));
    }

    // Diagnostics > Logs shows the log file, as Help > Support does.
    void logsPageShowsTheLogFile()
    {
        LogsPage page;
        auto* view = page.findChild<QPlainTextEdit*>();
        QVERIFY(view != nullptr);
        QVERIFY2(view->isEnabled(), "the log view is greyed");
        QVERIFY(!view->toolTip().contains(QStringLiteral("NYI")));
        QCOMPARE(view->toPlainText(), SupportDialog::logTailText());
    }

    // Audio > VAX: each card's level shows the channel's audio level.
    void vaxCardShowsTheChannelLevel()
    {
        VaxChannelCard card(1);
        card.setLevel(0.5f);
        QVERIFY(qAbs(card.levelDbForTest() - (-6.0206)) < 0.01);
        card.setLevel(0.0f);
        QCOMPARE(card.levelDbForTest(), -60.0);
        auto* gauge = card.findChild<QWidget*>(QStringLiteral("vaxLevelGauge"));
        QVERIFY(gauge != nullptr);
        QVERIFY(OperatorWording::isPlain(gauge->toolTip()));
    }

    // The words the connected Setup controls show are plain.
    void connectedSetupControlsUsePlainWords()
    {
        RadioModel model;
        ensureSlice(model);
        QStringList texts;
        {
            StartupPrefsPage page(&model);
            for (const char* name : {"startupAutoConnect", "startupCallsign", "startupGridSquare"}) {
                auto* w = page.findChild<QWidget*>(QLatin1String(name));
                QVERIFY(w != nullptr);
                texts << w->toolTip();
            }
        }
        {
            NrAnfSetupPage page(&model);
            texts << page.findChild<QCheckBox*>(QStringLiteral("anfEnableCheck"))->toolTip();
        }
        {
            MeterStylesPage page(&model);
            for (const char* name : {"sMeterFaceCombo", "sMeterPeakHoldCheck", "sMeterDecayCombo"}) {
                auto* w = page.findChild<QWidget*>(QLatin1String(name));
                QVERIFY(w != nullptr);
                texts << w->toolTip();
            }
        }
        {
            LogsPage page;
            for (QWidget* w : page.findChildren<QWidget*>()) {
                if (!w->toolTip().isEmpty()) { texts << w->toolTip(); }
            }
        }
        for (const QString& text : std::as_const(texts)) {
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
            QVERIFY2(!text.contains(QStringLiteral("NYI")), qPrintable(text));
            QVERIFY2(!text.contains(QChar(0x2014)), qPrintable(text));
        }
    }
};

QTEST_MAIN(TstSetupControlsConnected)
#include "tst_setup_controls_connected.moc"
