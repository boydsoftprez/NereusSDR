// =================================================================
// tests/tst_tx_applet_mon_output.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// R-R3-45 (operator decision 2026-09-24): the SPEAKERS / PHONES choice for
// the transmit monitor, on the TX applet's MON row beside the MON button.
//
// Pins:
//   - captions and plain-word tooltips;
//   - speakers checked by default, from the engine's saved choice;
//   - a click sets and saves the engine's MON output, exclusively;
//   - an engine change shows on the buttons;
//   - with PHONES chosen and no headphones output open, a plain notice says
//     why MON is silent (the receiver flag's own words), and it hides again
//     on SPEAKERS or once the headphones open.
//
// No device is opened: the engine is never started, and the headphones
// are a fake bus through the NEREUS_BUILD_TESTS seam.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "gui/applets/TxApplet.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"

#include "fakes/FakeAudioBus.h"

#include <memory>

using namespace NereusSDR;

class TestTxAppletMonOutput : public QObject {
    Q_OBJECT

private:
    struct Parts {
        QPushButton* speakers{nullptr};
        QPushButton* phones{nullptr};
        QLabel* notice{nullptr};
    };

    static Parts partsOf(TxApplet& applet)
    {
        Parts p;
        p.speakers = applet.findChild<QPushButton*>(QStringLiteral("TxMonitorSpeakersButton"));
        p.phones = applet.findChild<QPushButton*>(QStringLiteral("TxMonitorHeadphonesButton"));
        p.notice = applet.findChild<QLabel*>(QStringLiteral("TxMonitorOutputNotice"));
        return p;
    }

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
        // The test device guard: with no fake device supplied, the engine
        // opens nothing real.
        QStandardPaths::setTestModeEnabled(true);
    }
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void captionsAndTooltips()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        const Parts p = partsOf(applet);
        QVERIFY(p.speakers && p.phones && p.notice);
        QCOMPARE(p.speakers->text(), QStringLiteral("SPEAKERS"));
        QCOMPARE(p.phones->text(), QStringLiteral("PHONES"));
        QCOMPARE(p.speakers->toolTip(),
                 QStringLiteral("Play your transmit monitor on the speakers"));
        QCOMPARE(p.phones->toolTip(),
                 QStringLiteral("Play your transmit monitor on the headphones"));
    }

    void speakersByDefault()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        const Parts p = partsOf(applet);
        QVERIFY(p.speakers->isChecked());
        QVERIFY(!p.phones->isChecked());
        QVERIFY(p.notice->isHidden());
    }

    void showsTheSavedChoice()
    {
        AppSettings::instance().setValue(QStringLiteral("audio/TxMonitor/Output"),
                                         QStringLiteral("Headphones"));
        RadioModel rm;
        TxApplet applet(&rm);
        const Parts p = partsOf(applet);
        QVERIFY(p.phones->isChecked());
        QVERIFY(!p.speakers->isChecked());
    }

    void clickSetsAndSavesTheChoice()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        const Parts p = partsOf(applet);

        p.phones->click();
        QCOMPARE(rm.audioEngine()->txMonitorOutput(), TxMonitorOutput::Headphones);
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("audio/TxMonitor/Output")).toString(),
                 QStringLiteral("Headphones"));
        QVERIFY(p.phones->isChecked());
        QVERIFY(!p.speakers->isChecked());

        // Clicking the checked one keeps it.
        p.phones->click();
        QVERIFY(p.phones->isChecked());
        QCOMPARE(rm.audioEngine()->txMonitorOutput(), TxMonitorOutput::Headphones);

        p.speakers->click();
        QCOMPARE(rm.audioEngine()->txMonitorOutput(), TxMonitorOutput::Speakers);
        QVERIFY(p.speakers->isChecked());
        QVERIFY(!p.phones->isChecked());
    }

    void engineChangeShowsOnTheButtons()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        const Parts p = partsOf(applet);
        rm.audioEngine()->setTxMonitorOutput(TxMonitorOutput::Headphones);
        QVERIFY(p.phones->isChecked());
        QVERIFY(!p.speakers->isChecked());
        rm.audioEngine()->setTxMonitorOutput(TxMonitorOutput::Speakers);
        QVERIFY(p.speakers->isChecked());
    }

    void saysWhyItIsSilentWithNoHeadphones()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        const Parts p = partsOf(applet);
        QVERIFY(!rm.audioEngine()->headphonesAvailable());

        p.phones->click();
        QVERIFY(!p.notice->isHidden());
        QCOMPARE(p.notice->text(), VfoWidget::headphonesMissingText());

        // Turned on in Setup but not opened: says so instead.
        rm.audioEngine()->setHeadphonesEnabled(true);
        QVERIFY(!rm.audioEngine()->headphonesAvailable());
        QVERIFY(!p.notice->isHidden());
        QCOMPARE(p.notice->text(), VfoWidget::headphonesNotOpenedText());

        // Once headphones are open, no notice.
        auto bus = std::make_unique<FakeAudioBus>(QStringLiteral("FakeHeadphones"));
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels = 2;
        fmt.sample = AudioFormat::Sample::Float32;
        bus->open(fmt);
        rm.audioEngine()->setHeadphonesBusForTest(std::move(bus));
        QVERIFY(rm.audioEngine()->headphonesAvailable());
        QVERIFY(p.notice->isHidden());

        // And none on the speakers.
        rm.audioEngine()->setHeadphonesBusForTest(nullptr);
        QVERIFY(!p.notice->isHidden());
        p.speakers->click();
        QVERIFY(p.notice->isHidden());
    }
};

QTEST_MAIN(TestTxAppletMonOutput)
#include "tst_tx_applet_mon_output.moc"
