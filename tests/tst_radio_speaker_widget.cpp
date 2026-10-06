// no-port-check: test-only, NereusSDR-original. No upstream logic is
// ported here.
//
// =================================================================
// tests/tst_radio_speaker_widget.cpp  (NereusSDR)
// =================================================================
//
// The header's RADIO group, RadioSpeakerWidget (R-SPK-06, R-SPK-07,
// R-SPK-16, R-SPK-17, D1, D5, D10; V-UI-1).
//
//   1. The slider and the icon write RadioModel (level, mute); model
//      changes from any source update the widget without writing back.
//   2. Clicking either header icon toggles its own mute and the icon shows
//      it (D5), PC included.
//   3. RADIO and PC are independent: both groups in one title bar on the
//      model's real AudioEngine.
//   4. Availability: no radio and an older Core disable the button and
//      slider (never hidden, D10) with icon radio-none, readout "--" and
//      the reason as tooltip; a Hermes Lite 2 stays enabled with the add-on
//      note; a remote window names the speaker at the Core (R-SPK-16).
//   5. With NEREUS_HEADER_CAPTURE_DIR set, captures of the header in its
//      states are saved in both forms, side by side and stacked (run once
//      plain and once with QT_SCALE_FACTOR=2 for 1x and 2x).
//   6. The stacked form (layout C, R-SPK-17): both icons still mute, both
//      sliders still write, and the handles keep the side-by-side size.
//
// Modification history (NereusSDR):
//   2026-10-06 - Written for the radio speaker and Audio Setup plan, Task 6.
//                J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPalette>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/session/IStationLink.h"
#include "gui/TitleBar.h"
#include "gui/widgets/AppIcon.h"
#include "gui/widgets/MasterOutputWidget.h"
#include "gui/widgets/RadioSpeakerWidget.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

const QString kNoRadio = QStringLiteral("No radio connected");
const QString kOlderCore =
    QStringLiteral("This Core can't set the radio speaker. Update the Core.");
const QString kRemoteTip =
    QStringLiteral("Radio speaker at the Core (shared with every window and the phone)");

// A connected radio that carries radio audio and does nothing else.
class SpeakerConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit SpeakerConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    int protocolVersion() const override { return 1; }
    bool carriesRadioAudio() const noexcept override { return true; }

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
    void setMox(bool) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
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
};

// A Core link; `offers` says whether the Core advertised the radio speaker.
class SpeakerLink : public IStationLink {
public:
    bool offers{false};
    bool radioSpeakerAvailable() const override { return offers; }
    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
};

struct Parts {
    QPushButton* button{nullptr};
    QLabel* label{nullptr};
    QSlider* slider{nullptr};
    QLabel* value{nullptr};
};

Parts partsOf(QWidget* w)
{
    return {w->findChild<QPushButton*>(QStringLiteral("radioSpeakerBtn")),
            w->findChild<QLabel*>(QStringLiteral("radioLabel")),
            w->findChild<QSlider*>(QStringLiteral("radioSlider")),
            w->findChild<QLabel*>(QStringLiteral("radioValueLabel"))};
}

QString iconOf(const QAbstractButton* b)
{
    return b->property(AppIcon::kIconProperty).toString();
}

void connectLocal(RadioModel& model, SpeakerConnection& conn, HPSDRHW board)
{
    model.setBoardForTest(board);
    model.injectConnectionForTest(&conn);
    model.setConnectionStateForTest(ConnectionState::Connected);
}

// The four remote readouts a remote window shows; at 1440 px they need
// the room the side-by-side volume groups would take.
void showRemoteReadouts(TitleBar& bar)
{
    ConnectionSegment* seg = bar.connectionSegment();
    seg->setState(ConnectionState::Connected);
    seg->setRemoteStatusText(QStringLiteral("Core connected"));
    seg->setRemoteMetrics({QStringLiteral("Traffic \u219312.4 \u21910.8 Mbps"),
                           QStringLiteral("Audio 96.0 kbps (playing)"),
                           QStringLiteral("Radio \u219312.4 \u21910.8 Mbps"),
                           QStringLiteral("Core RTT 18 ms")});
}

QSize handleSize(QSlider* slider)
{
    QStyleOptionSlider opt;
    opt.initFrom(slider);
    opt.orientation = slider->orientation();
    opt.minimum = slider->minimum();
    opt.maximum = slider->maximum();
    opt.sliderPosition = slider->sliderPosition();
    opt.sliderValue = slider->value();
    opt.subControls = QStyle::SC_SliderHandle | QStyle::SC_SliderGroove;
    return slider->style()
        ->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, slider)
        .size();
}

void saveCapture(QWidget* w, const QString& stem)
{
    const QString dir = qEnvironmentVariable("NEREUS_HEADER_CAPTURE_DIR");
    if (dir.isEmpty() || !w) {
        return;
    }
    QDir().mkpath(dir);
    QApplication::processEvents();
    const QPixmap shot = w->grab();
    const int scale = qRound(shot.devicePixelRatio());
    const QString path = QStringLiteral("%1/%2@%3x.png").arg(dir, stem).arg(scale);
    QVERIFY2(shot.save(path), qPrintable(path));
}

} // namespace

class TstRadioSpeakerWidget : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        auto& s = AppSettings::instance();
        s.remove(QStringLiteral("audio/Master/Volume"));
        s.remove(QStringLiteral("audio/Master/Muted"));
        s.remove(QStringLiteral("audio/Speakers/DeviceName"));
    }

    // The shape of the group: the four named children, their sizes and
    // ranges, and the RADIO word.
    void shape()
    {
        RadioSpeakerWidget w(nullptr);
        const Parts p = partsOf(&w);
        QVERIFY(p.button && p.label && p.slider && p.value);
        QVERIFY(p.button->isCheckable());
        QCOMPARE(p.button->size(), QSize(20, 20));
        QVERIFY(p.button->text().isEmpty());
        QCOMPARE(p.label->text(), QStringLiteral("RADIO"));
        QCOMPARE(p.slider->minimum(), 0);
        QCOMPARE(p.slider->maximum(), 100);
        QCOMPARE(p.slider->minimumWidth(), 100);
        QCOMPARE(p.slider->maximumWidth(), 100);
        QCOMPARE(p.slider->minimumHeight(), 16);
        QCOMPARE(p.slider->maximumHeight(), 16);
        QVERIFY(p.slider->styleSheet().contains(QStringLiteral("#e0a030")));
        QCOMPARE(p.value->styleSheet(), QString::fromLatin1(HeaderVolumeStyle::kReadout));
    }

    // 1. Slider and icon write the model; the model's own changes come back
    // without a second write.
    void writesModelAndFollowsItWithoutEcho()
    {
        RadioModel model;
        SpeakerConnection conn;
        const auto detach = qScopeGuard([&model]() { model.injectConnectionForTest(nullptr); });
        connectLocal(model, conn, HPSDRHW::Hermes);
        model.setRadioSpeakerVolume(50);
        model.setRadioSpeakerMuted(false);

        RadioSpeakerWidget w(&model);
        const Parts p = partsOf(&w);
        QCOMPARE(p.slider->value(), 50);
        QCOMPARE(p.value->text(), QStringLiteral("50"));

        QSignalSpy volume(&model, &RadioModel::radioSpeakerVolumeChanged);
        QSignalSpy muted(&model, &RadioModel::radioSpeakerMutedChanged);

        p.slider->setValue(64);
        QCOMPARE(model.radioSpeakerVolume(), 64);
        QCOMPARE(volume.count(), 1);
        QCOMPARE(p.value->text(), QStringLiteral("64"));
        QVERIFY(std::abs(model.localAudioDevices()->radioSpeakerVolume() - 0.64f) < 1e-3f);

        p.button->click();
        QVERIFY(model.radioSpeakerMuted());
        QCOMPARE(muted.count(), 1);
        QCOMPARE(iconOf(p.button), QStringLiteral("radio-muted"));

        // From another source (Setup, a remote window, the phone).
        volume.clear();
        muted.clear();
        model.setRadioSpeakerVolume(30);
        QCOMPARE(p.slider->value(), 30);
        QCOMPARE(p.value->text(), QStringLiteral("30"));
        QCOMPARE(volume.count(), 1);
        model.setRadioSpeakerMuted(false);
        QVERIFY(!p.button->isChecked());
        QCOMPARE(iconOf(p.button), QStringLiteral("radio-on"));
        QCOMPARE(muted.count(), 1);
        QCOMPARE(model.radioSpeakerVolume(), 30);
    }

    // 2. D5: each icon toggles its own mute, and shows it.
    void iconsToggleTheirMute()
    {
        RadioModel model;
        SpeakerConnection conn;
        const auto detach = qScopeGuard([&model]() { model.injectConnectionForTest(nullptr); });
        connectLocal(model, conn, HPSDRHW::Hermes);
        model.setRadioSpeakerMuted(false);

        TitleBar bar(model.localAudioDevices());
        bar.setRadioModel(&model);
        auto* pc = bar.findChild<QPushButton*>(QStringLiteral("speakerBtn"));
        auto* radio = bar.findChild<QPushButton*>(QStringLiteral("radioSpeakerBtn"));
        QVERIFY(pc && radio);
        QCOMPARE(iconOf(pc), QStringLiteral("pc-on"));
        QCOMPARE(iconOf(radio), QStringLiteral("radio-on"));
        QVERIFY(!pc->icon().isNull());
        QVERIFY(!radio->icon().isNull());

        radio->click();
        QVERIFY(model.radioSpeakerMuted());
        QCOMPARE(iconOf(radio), QStringLiteral("radio-muted"));
        QVERIFY(!model.localAudioDevices()->masterMuted());
        QCOMPARE(iconOf(pc), QStringLiteral("pc-on"));

        pc->click();
        QVERIFY(model.localAudioDevices()->masterMuted());
        QCOMPARE(iconOf(pc), QStringLiteral("pc-muted"));
        QVERIFY(model.radioSpeakerMuted());

        radio->click();
        pc->click();
        QVERIFY(!model.radioSpeakerMuted());
        QVERIFY(!model.localAudioDevices()->masterMuted());
        QCOMPARE(iconOf(radio), QStringLiteral("radio-on"));
        QCOMPARE(iconOf(pc), QStringLiteral("pc-on"));
    }

    // 3. Moving RADIO never changes PC, and the reverse.
    void pcAndRadioAreIndependent()
    {
        RadioModel model;
        SpeakerConnection conn;
        const auto detach = qScopeGuard([&model]() { model.injectConnectionForTest(nullptr); });
        connectLocal(model, conn, HPSDRHW::Hermes);
        model.setRadioSpeakerVolume(40);

        AudioEngine* engine = model.localAudioDevices();
        QVERIFY(engine);
        TitleBar bar(engine);
        bar.setRadioModel(&model);
        auto* pcSlider = bar.findChild<QSlider*>(QStringLiteral("masterSlider"));
        auto* radioSlider = bar.findChild<QSlider*>(QStringLiteral("radioSlider"));
        QVERIFY(pcSlider && radioSlider);
        pcSlider->setValue(72);
        QVERIFY(std::abs(engine->volume() - 0.72f) < 1e-3f);

        QSignalSpy pcVolume(engine, &AudioEngine::volumeChanged);
        radioSlider->setValue(15);
        QCOMPARE(model.radioSpeakerVolume(), 15);
        QVERIFY(std::abs(engine->radioSpeakerVolume() - 0.15f) < 1e-3f);
        QVERIFY(std::abs(engine->volume() - 0.72f) < 1e-3f);
        QCOMPARE(pcSlider->value(), 72);
        QCOMPARE(pcVolume.count(), 0);

        QSignalSpy radioVolume(&model, &RadioModel::radioSpeakerVolumeChanged);
        pcSlider->setValue(20);
        QVERIFY(std::abs(engine->volume() - 0.20f) < 1e-3f);
        QCOMPARE(model.radioSpeakerVolume(), 15);
        QVERIFY(std::abs(engine->radioSpeakerVolume() - 0.15f) < 1e-3f);
        QCOMPARE(radioSlider->value(), 15);
        QCOMPARE(radioVolume.count(), 0);
    }

    // 4a. No radio: disabled, not hidden, icon radio-none, "--", reason.
    void noRadioIsDisabledWithReason()
    {
        RadioSpeakerWidget none(nullptr);
        Parts p = partsOf(&none);
        QVERIFY(!p.button->isEnabled());
        QVERIFY(!p.slider->isEnabled());
        QVERIFY(!p.button->isHidden() && !p.slider->isHidden() && !p.label->isHidden()
                && !p.value->isHidden());
        QCOMPARE(iconOf(p.button), QStringLiteral("radio-none"));
        QCOMPARE(p.value->text(), QStringLiteral("--"));
        QCOMPARE(none.toolTip(), kNoRadio);
        QCOMPARE(p.slider->toolTip(), kNoRadio);
        QCOMPARE(p.button->toolTip(), kNoRadio);

        RadioModel model;
        RadioSpeakerWidget w(&model);
        p = partsOf(&w);
        QVERIFY(!p.button->isEnabled());
        QVERIFY(!p.slider->isEnabled());
        QCOMPARE(iconOf(p.button), QStringLiteral("radio-none"));
        QCOMPARE(p.value->text(), QStringLiteral("--"));
        QCOMPARE(p.button->toolTip(), kNoRadio);

        // A radio arrives, then goes.
        SpeakerConnection conn;
        const auto detach = qScopeGuard([&model]() { model.injectConnectionForTest(nullptr); });
        connectLocal(model, conn, HPSDRHW::Hermes);
        QVERIFY(p.button->isEnabled());
        QVERIFY(p.slider->isEnabled());
        QCOMPARE(iconOf(p.button), QStringLiteral("radio-on"));
        QCOMPARE(p.value->text(), QString::number(model.radioSpeakerVolume()));
        QCOMPARE(p.slider->toolTip(), QStringLiteral("Radio speaker"));

        model.injectConnectionForTest(nullptr);
        QVERIFY(!p.button->isEnabled());
        QVERIFY(!p.slider->isEnabled());
        QCOMPARE(iconOf(p.button), QStringLiteral("radio-none"));
        QCOMPARE(p.value->text(), QStringLiteral("--"));
        QCOMPARE(p.slider->toolTip(), kNoRadio);
    }

    // 4b. A Hermes Lite 2 (availability 2) stays enabled; the tooltip
    // names the audio add-on board (R-SPK-07).
    void hermesLiteNamesTheAddOn()
    {
        RadioModel model;
        SpeakerConnection conn;
        const auto detach = qScopeGuard([&model]() { model.injectConnectionForTest(nullptr); });
        connectLocal(model, conn, HPSDRHW::HermesLite);
        QCOMPARE(model.radioSpeakerAvailability(), int(RadioModel::kRadioSpeakerNeedsAddOn));

        RadioSpeakerWidget w(&model);
        const Parts p = partsOf(&w);
        QVERIFY(p.button->isEnabled());
        QVERIFY(p.slider->isEnabled());
        QCOMPARE(iconOf(p.button), QStringLiteral("radio-on"));
        QVERIFY(p.slider->toolTip().contains(QStringLiteral("audio add-on board")));
        QCOMPARE(p.slider->toolTip(), model.radioSpeakerToolTip());
    }

    // 4c. Remote windows: the speaker at the Core, or the update reason
    // from an older Core.
    void remoteWindowToolTips()
    {
        RadioModel remote(RadioModel::Role::Remote);
        SpeakerLink link;
        remote.attachStation(&link);
        const auto detach = qScopeGuard([&remote]() { remote.attachStation(nullptr); });
        RadioSpeakerWidget w(&remote);
        const Parts p = partsOf(&w);

        // An older Core: the reason changes with no availability change;
        // the tooltip is current when it shows.
        remote.setStationConnectionState(ConnectionState::Connected);
        QVERIFY(!p.slider->isEnabled());
        QCOMPARE(p.value->text(), QStringLiteral("--"));
        QCOMPARE(iconOf(p.button), QStringLiteral("radio-none"));
        QCOMPARE(p.slider->toolTip(), kOlderCore);

        // A Core that offers it, with a radio.
        link.offers = true;
        QVERIFY(remote.applyStationRadioSpeakerValue("radioSpeakerAvailability",
                                                     RadioModel::kRadioSpeakerAvailable));
        QVERIFY(p.slider->isEnabled());
        QVERIFY(p.button->isEnabled());
        QCOMPARE(p.slider->toolTip(), kRemoteTip);
        QCOMPARE(p.button->toolTip(), kRemoteTip);

        // The Core's value arrives, and the window writes through.
        QSignalSpy volume(&remote, &RadioModel::radioSpeakerVolumeChanged);
        remote.setRadioSpeakerVolume(81);
        QCOMPARE(p.slider->value(), 81);
        QCOMPARE(p.value->text(), QStringLiteral("81"));
        p.slider->setValue(12);
        QCOMPARE(remote.radioSpeakerVolume(), 12);
        QCOMPARE(volume.count(), 2);

        // The Core's radio goes.
        QVERIFY(remote.applyStationRadioSpeakerValue("radioSpeakerAvailability",
                                                     RadioModel::kRadioSpeakerNoRadio));
        QVERIFY(!p.slider->isEnabled());
        QCOMPARE(p.slider->toolTip(), kNoRadio);
    }

    // 5. V-UI-1 captures: the header in its states.
    void headerCaptures()
    {
        if (qEnvironmentVariable("NEREUS_HEADER_CAPTURE_DIR").isEmpty()) {
            QSKIP("NEREUS_HEADER_CAPTURE_DIR not set");
        }
        RadioModel model;
        SpeakerConnection conn;
        const auto detach = qScopeGuard([&model]() { model.injectConnectionForTest(nullptr); });

        TitleBar bar(model.localAudioDevices());
        auto* menu = new QMenuBar(&bar);
        menu->addMenu(QStringLiteral("File"));
        menu->addMenu(QStringLiteral("Radio"));
        menu->addMenu(QStringLiteral("Setup"));
        menu->addMenu(QStringLiteral("Help"));
        bar.setMenuBar(menu);
        bar.setRadioModel(&model);
        // The app's own stylesheet paints the strip dark; a bare test has
        // none, so fill it with the strip colour for the comparison with
        // header-layouts.html.
        bar.setAutoFillBackground(true);
        QPalette pal = bar.palette();
        pal.setColor(QPalette::Window, QColor(QStringLiteral("#0a0a14")));
        bar.setPalette(pal);
        bar.resize(1440, 32);
        bar.show();
        QApplication::processEvents();

        auto* pcSlider = bar.findChild<QSlider*>(QStringLiteral("masterSlider"));
        auto* pcBtn = bar.findChild<QPushButton*>(QStringLiteral("speakerBtn"));
        QVERIFY(pcSlider && pcBtn);
        pcSlider->setValue(72);

        // The volume area alone, from the UTC clock to the bulb.
        auto shoot = [&bar](const QString& stem) {
            saveCapture(&bar, QStringLiteral("header-") + stem);
            const QWidget* master = bar.masterOutput();
            const QRect area(master->x() - 110, 0, bar.width() - (master->x() - 110), bar.height());
            const QString dir = qEnvironmentVariable("NEREUS_HEADER_CAPTURE_DIR");
            const QPixmap crop = bar.grab(area);
            const QString path = QStringLiteral("%1/header-volume-%2@%3x.png")
                .arg(dir, stem).arg(qRound(crop.devicePixelRatio()));
            QVERIFY2(crop.save(path), qPrintable(path));
        };

        auto states = [&](const QString& form) {
            model.injectConnectionForTest(nullptr);
            shoot(form + QStringLiteral("radio-unavailable"));

            connectLocal(model, conn, HPSDRHW::Hermes);
            model.setRadioSpeakerVolume(40);
            model.setRadioSpeakerMuted(false);
            shoot(form + QStringLiteral("pc-on-radio-on"));

            model.setRadioSpeakerMuted(true);
            shoot(form + QStringLiteral("radio-muted"));

            model.setRadioSpeakerMuted(false);
            pcBtn->click();
            shoot(form + QStringLiteral("pc-muted"));
            pcBtn->click();
        };

        QVERIFY(!bar.volumeStacked());
        states(QString());

        // Layout C: the same bar with the remote readouts, which need the
        // room at this width.
        showRemoteReadouts(bar);
        QApplication::processEvents();
        QVERIFY(bar.volumeStacked());
        states(QStringLiteral("stacked-"));
    }

    // 6. The stacked form still mutes and writes, with full-size handles.
    void stackedFormStillWorks()
    {
        RadioModel model;
        SpeakerConnection conn;
        const auto detach = qScopeGuard([&model]() { model.injectConnectionForTest(nullptr); });
        connectLocal(model, conn, HPSDRHW::Hermes);
        model.setRadioSpeakerVolume(50);
        model.setRadioSpeakerMuted(false);

        AudioEngine* engine = model.localAudioDevices();
        TitleBar bar(engine);
        auto* menu = new QMenuBar(&bar);
        menu->addMenu(QStringLiteral("Radio"));
        menu->addMenu(QStringLiteral("Setup"));
        menu->addMenu(QStringLiteral("Help"));
        bar.setMenuBar(menu);
        bar.setRadioModel(&model);
        auto* pcSlider = bar.findChild<QSlider*>(QStringLiteral("masterSlider"));
        const QSize sideBySideHandle = handleSize(pcSlider);
        showRemoteReadouts(bar);
        bar.resize(1440, 32);
        bar.show();
        QApplication::processEvents();
        QVERIFY(bar.volumeStacked());

        const Parts p = partsOf(&bar);
        auto* pcBtn = bar.findChild<QPushButton*>(QStringLiteral("speakerBtn"));
        QVERIFY(p.button && p.slider && p.value && pcBtn && pcSlider);
        QVERIFY(p.slider->isEnabled());

        // Thin sliders, same handle as side by side (10 px).
        QVERIFY(sideBySideHandle.width() >= 10 && sideBySideHandle.height() >= 10);
        for (QSlider* slider : {pcSlider, p.slider}) {
            const QSize handle = handleSize(slider);
            QVERIFY2(handle.width() >= sideBySideHandle.width()
                         && handle.height() >= sideBySideHandle.height(),
                     qPrintable(QStringLiteral("%1 handle %2x%3")
                                    .arg(slider->objectName())
                                    .arg(handle.width()).arg(handle.height())));
        }

        p.slider->setValue(64);
        QCOMPARE(model.radioSpeakerVolume(), 64);
        QCOMPARE(p.value->text(), QStringLiteral("64"));
        QTest::mouseClick(p.button, Qt::LeftButton);
        QVERIFY(model.radioSpeakerMuted());
        QCOMPARE(iconOf(p.button), QStringLiteral("radio-muted"));
        QVERIFY(!engine->masterMuted());

        pcSlider->setValue(30);
        QVERIFY(std::abs(engine->volume() - 0.30f) < 1e-3f);
        QCOMPARE(model.radioSpeakerVolume(), 64);
        QTest::mouseClick(pcBtn, Qt::LeftButton);
        QVERIFY(engine->masterMuted());
        QCOMPARE(iconOf(pcBtn), QStringLiteral("pc-muted"));

        // From another source, while stacked.
        model.setRadioSpeakerMuted(false);
        model.setRadioSpeakerVolume(20);
        QCOMPARE(p.slider->value(), 20);
        QCOMPARE(iconOf(p.button), QStringLiteral("radio-on"));
        QTest::mouseClick(pcBtn, Qt::LeftButton);
        QVERIFY(!engine->masterMuted());

        // Disabled, never hidden, in the stacked form too.
        model.injectConnectionForTest(nullptr);
        QApplication::processEvents();
        QVERIFY(p.slider->isVisible());
        QVERIFY(!p.slider->isEnabled());
        QCOMPARE(p.slider->toolTip(), kNoRadio);
        QCOMPARE(iconOf(p.button), QStringLiteral("radio-none"));
    }
};

QTEST_MAIN(TstRadioSpeakerWidget)
#include "tst_radio_speaker_widget.moc"
