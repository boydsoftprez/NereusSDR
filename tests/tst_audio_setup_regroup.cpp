// no-port-check: test-only, NereusSDR-original. No upstream logic is
// ported here.
//
// =================================================================
// tests/tst_audio_setup_regroup.cpp  (NereusSDR)
// =================================================================
//
// Setup > Audio > Outputs (R-SPK-21 Outputs, R-SPK-22, R-SPK-24, D13 to
// D15).
//
//   1. Outputs is the first Audio page, Mixed; no Setup page carries the
//      old backend strip, and the Sound system line is on Outputs only.
//   2. Each Outputs control is present once across Setup: the speakers
//      and headphones cards moved off Devices; the radio speaker ids.
//   3. Saved keys unchanged: audio/Master/Volume as "0.720",
//      audio/Master/Muted, audio/Speakers/*, audio/Headphones/Enabled.
//   4. This computer's Volume and Mute are the header's PC control: the
//      engine and the header's MasterOutputWidget follow, and back.
//   5. Headphones: greyed until Enabled, the PC volume note.
//   6. Radio speaker: Volume and Mute write and follow RadioModel, nothing
//      is written while the page builds; no radio, the HL2 add-on status,
//      and the amplifier choice enabled (G2 on Protocol 2), greyed with the
//      model's reason (HL2, ANAN-100D, ANAN-G2E), and its live status.
//   7. Remote: the Core's text, an older Core's reason, and the Core's
//      settings unavailable gating only the radio speaker controls.
//   8. Sound system texts for Mac, Windows and every Linux backend.
//   9. One "Rescan devices" that reports what it found.
//  10. With NEREUS_AUDIO_SETUP_CAPTURE_DIR set, captures of the page in its
//      states (run once plain and once with QT_SCALE_FACTOR=2).
//
// Modification history (NereusSDR):
//   2026-10-06 - Written for the radio speaker and Audio Setup plan, Task 9.
//                J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSlider>
#include <QStackedWidget>
#include <QToolButton>
#include <QTreeWidget>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/session/IStationLink.h"
#include "gui/SetupDialog.h"
#include "gui/setup/AudioOutputsPage.h"
#include "gui/setup/DeviceCard.h"
#include "gui/setup/SoundSystemLine.h"
#include "gui/widgets/AppIcon.h"
#include "gui/widgets/MasterOutputWidget.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

const QString kNoRadio = QStringLiteral("No radio connected");
const QString kOlderCore =
    QStringLiteral("This Core can't set the radio speaker. Update the Core.");
const QString kNoAmp = QStringLiteral("This radio has no switchable speaker amplifier.");
const QString kG2e = QStringLiteral("Not tested on the ANAN-G2E.");
const QString kStationReason = QStringLiteral("Connect to the Core to change these.");

// A connected radio that carries radio audio and does nothing else.
class SpeakerConnection : public RadioConnection {
    Q_OBJECT
public:
    int protocol{1};

    explicit SpeakerConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    int protocolVersion() const override { return protocol; }
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

void connectRadio(RadioModel& model, SpeakerConnection& conn, HPSDRModel board, int protocol)
{
    conn.protocol = protocol;
    model.setHpsdrModelForTest(board);
    model.injectConnectionForTest(&conn);
    model.setConnectionStateForTest(ConnectionState::Connected);
}

template <typename T>
T* child(QWidget* w, const char* name)
{
    return w->findChild<T*>(QLatin1String(name));
}

struct Radio {
    QSlider* volume{nullptr};
    QCheckBox* mute{nullptr};
    QPushButton* icon{nullptr};
    QLabel* readout{nullptr};
    QLabel* status{nullptr};
    QLabel* note{nullptr};
    QWidget* amp{nullptr};
    QRadioButton* normal{nullptr};
    QRadioButton* offOnTx{nullptr};
    QRadioButton* alwaysOff{nullptr};
    QLabel* ampReason{nullptr};
    QLabel* ampStatus{nullptr};
};

Radio radioOf(QWidget* page)
{
    Radio r;
    r.volume = child<QSlider>(page, "radioSpeakerVolume");
    r.mute = child<QCheckBox>(page, "radioSpeakerMute");
    r.icon = child<QPushButton>(page, "radioSpeakerMuteButton");
    r.readout = child<QLabel>(page, "radioSpeakerReadout");
    r.status = child<QLabel>(page, "radioSpeakerStatusLine");
    r.note = child<QLabel>(page, "radioSpeakerNote");
    r.amp = child<QWidget>(page, "speakerAmplifierChoice");
    r.normal = child<QRadioButton>(page, "ampNormal");
    r.offOnTx = child<QRadioButton>(page, "ampOffOnTx");
    r.alwaysOff = child<QRadioButton>(page, "ampAlwaysOff");
    r.ampReason = child<QLabel>(page, "speakerAmplifierReason");
    r.ampStatus = child<QLabel>(page, "speakerAmplifierStatus");
    return r;
}

QString iconOf(const QAbstractButton* b)
{
    return b->property(AppIcon::kIconProperty).toString();
}

QTreeWidgetItem* audioCategory(SetupDialog& dialog)
{
    auto* tree = dialog.findChild<QTreeWidget*>();
    if (tree == nullptr) {
        return nullptr;
    }
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        if (tree->topLevelItem(i)->text(0) == QStringLiteral("Audio")) {
            return tree->topLevelItem(i);
        }
    }
    return nullptr;
}

int countNamed(QWidget* root, const QString& name)
{
    return static_cast<int>(root->findChildren<QWidget*>(name).size());
}

int countSetupId(QWidget* root, const QString& id)
{
    int n = 0;
    for (QWidget* w : root->findChildren<QWidget*>()) {
        if (w->property("nereusSetupId").toString() == id) {
            ++n;
        }
    }
    return n;
}

void saveCapture(QWidget* w, const QString& stem)
{
    const QString dir = qEnvironmentVariable("NEREUS_AUDIO_SETUP_CAPTURE_DIR");
    if (dir.isEmpty() || w == nullptr) {
        return;
    }
    QDir().mkpath(dir);
    QApplication::processEvents();
    QApplication::processEvents();
    const QPixmap shot = w->grab();
    const int scale = qRound(shot.devicePixelRatio());
    const QString path = QStringLiteral("%1/%2@%3x.png").arg(dir, stem).arg(scale);
    QVERIFY2(shot.save(path), qPrintable(path));
}

} // namespace

class TstAudioSetupRegroup : public QObject {
    Q_OBJECT

private:
    static void clearAudioKeys()
    {
        auto& s = AppSettings::instance();
        for (const QString& k : s.allKeys()) {
            if (k.startsWith(QStringLiteral("audio/"))) {
                s.remove(k);
            }
        }
    }

private slots:
    void init() { clearAudioKeys(); }
    void cleanup() { clearAudioKeys(); }

    // 1. Outputs leads Audio, Mixed; no page shows the old strip.
    void outputsLeadsAudioAndNoPageShowsTheStrip()
    {
        RadioModel model;
        SetupDialog dialog(&model);
        QTreeWidgetItem* audio = audioCategory(dialog);
        QVERIFY(audio != nullptr);
        QCOMPARE(audio->child(0)->text(0), QStringLiteral("Outputs"));

        const QStringList labels = dialog.pageLabelsForTest();
        const int outputs = static_cast<int>(labels.indexOf(QStringLiteral("Outputs")));
        QVERIFY(outputs >= 0);
        QCOMPARE(labels.count(QStringLiteral("Outputs")), 1);
        QCOMPARE(dialog.pageScopeAtForTest(outputs), SetupScope::Mixed);

        dialog.realizeAllPagesForTest();
        for (QWidget* w : dialog.findChildren<QWidget*>()) {
            QVERIFY2(QLatin1String(w->metaObject()->className())
                         != QLatin1String("NereusSDR::AudioBackendStrip"),
                     qPrintable(w->objectName()));
        }
        QCOMPARE(countNamed(&dialog, QStringLiteral("soundSystemLine")), 1);
        auto* page = qobject_cast<AudioOutputsPage*>(dialog.realizedPageForTest(QStringLiteral("Outputs")));
        QVERIFY(page != nullptr);
        QVERIFY(page->findChild<SoundSystemLine*>() != nullptr);
    }

    // 2. Every Outputs control once across Setup.
    void outputsControlsArePresentOnce()
    {
        RadioModel model;
        SetupDialog dialog(&model);
        dialog.realizeAllPagesForTest();
        for (const char* name : {"soundSystemLine", "thisComputerGroup", "headphonesGroup",
                                 "radioSpeakerGroup", "radioSpeakerVolume", "radioSpeakerMute",
                                 "speakerAmplifierChoice", "ampNormal", "ampOffOnTx",
                                 "ampAlwaysOff", "speakerAmplifierStatus", "rescanDevices"}) {
            QVERIFY2(countNamed(&dialog, QLatin1String(name)) == 1, name);
        }
        for (const char* id : {"audio.outputs.radioSpeakerVolume",
                               "audio.outputs.radioSpeakerMuted",
                               "audio.outputs.speakerAmplifierMode"}) {
            QVERIFY2(countSetupId(&dialog, QLatin1String(id)) == 1, id);
        }
        QCOMPARE(dialog.pageLabelsForTest().count(QStringLiteral("Outputs")), 1);

        // The speakers and headphones cards are on Outputs only; Devices
        // keeps the microphone card.
        int speakers = 0;
        int headphones = 0;
        for (DeviceCard* card : dialog.findChildren<DeviceCard*>()) {
            speakers += card->title() == QStringLiteral("This computer")
                || card->title() == QStringLiteral("Speakers");
            headphones += card->title() == QStringLiteral("Headphones");
        }
        QCOMPARE(speakers, 1);
        QCOMPARE(headphones, 1);
        QWidget* devices = dialog.realizedPageForTest(QStringLiteral("Devices"));
        QVERIFY(devices != nullptr);
        QCOMPARE(devices->findChildren<DeviceCard*>().size(), 1);
        QCOMPARE(devices->findChildren<DeviceCard*>().first()->title(),
                 QStringLiteral("TX Input (Microphone)"));
        QVERIFY(devices->findChild<QLabel*>(QStringLiteral("radioSpeakerExplanation")) == nullptr);
    }

    // 3 and 4. The PC control: header and page are one control, saved keys
    // unchanged.
    void pcVolumeIsTheHeadersControlWithItsKeys()
    {
        RadioModel model;
        AudioEngine* engine = model.localAudioDevices();
        QVERIFY(engine != nullptr);
        engine->setVolume(0.5f);
        engine->setMasterMuted(false);
        AudioOutputsPage page(&model);
        MasterOutputWidget header(engine);
        auto* slider = child<QSlider>(&page, "pcVolume");
        auto* mute = child<QCheckBox>(&page, "pcMute");
        auto* icon = child<QPushButton>(&page, "pcMuteButton");
        QVERIFY(slider && mute && icon);
        auto* group = child<QWidget>(&page, "thisComputerGroup");
        QVERIFY(group != nullptr && group->isAncestorOf(slider));

        // Building the page saved nothing.
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("audio/Master/Volume")));

        slider->setValue(72);
        QCOMPARE(engine->volume(), 0.72f);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("audio/Master/Volume")).toString(),
                 QStringLiteral("0.720"));
        QCOMPARE(child<QLabel>(&page, "pcVolumeReadout")->text(), QStringLiteral("72"));

        mute->setChecked(true);
        QVERIFY(engine->masterMuted());
        QVERIFY(icon->isChecked());
        QCOMPARE(iconOf(icon), QStringLiteral("pc-muted"));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("audio/Master/Muted")).toString(),
                 QStringLiteral("True"));
        icon->click();
        QVERIFY(!engine->masterMuted());
        QVERIFY(!mute->isChecked());
        QCOMPARE(AppSettings::instance().value(QStringLiteral("audio/Master/Muted")).toString(),
                 QStringLiteral("False"));

        // From the header (or any other source) back to the page.
        auto* headerSlider = header.findChild<QSlider*>();
        QVERIFY(headerSlider != nullptr);
        QCOMPARE(headerSlider->value(), 72);
        headerSlider->setValue(30);
        QCOMPARE(slider->value(), 30);
        engine->setMasterMuted(true);
        QVERIFY(mute->isChecked());
        QCOMPARE(AppSettings::instance().value(QStringLiteral("audio/Master/Volume")).toString(),
                 QStringLiteral("0.300"));
    }

    // 3. The device cards keep their keys.
    void deviceCardsKeepTheirKeys()
    {
        // No engine: the cards save their keys without opening a
        // device (none in a test), so the run stays quiet.
        AudioOutputsPage page(nullptr);
        auto* speakers = child<DeviceCard>(&page, "thisComputerGroup");
        auto* headphones = child<DeviceCard>(&page, "headphonesGroup");
        QVERIFY(speakers && headphones);
        QCOMPARE(speakers->title(), QStringLiteral("This computer"));
        QCOMPARE(headphones->title(), QStringLiteral("Headphones"));

        // An edit on each card saves under the old prefixes.
        speakers->findChildren<QComboBox*>().first()->addItem(QStringLiteral("Other"),
                                                               QVariant::fromValue(55));
        speakers->findChildren<QComboBox*>().first()->setCurrentIndex(1);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("audio/Speakers/DriverApi")).toString(),
                 QStringLiteral("Other"));
        QCheckBox* enabled = nullptr;
        for (QCheckBox* box : headphones->findChildren<QCheckBox*>()) {
            if (box->text() == QStringLiteral("Enabled")) { enabled = box; }
        }
        QVERIFY(enabled != nullptr);
        enabled->setChecked(true);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("audio/Headphones/Enabled")).toString(),
                 QStringLiteral("True"));
        enabled->setChecked(false);
    }

    // 5. Headphones greyed until Enabled; Device details folded on both.
    void headphonesGreyedUntilEnabledAndDetailsFolded()
    {
        // No engine: the cards save their keys without opening a
        // device (none in a test), so the run stays quiet.
        AudioOutputsPage page(nullptr);
        auto* headphones = child<DeviceCard>(&page, "headphonesGroup");
        auto* body = headphones->findChild<QWidget*>(QStringLiteral("deviceCardBody"));
        QVERIFY(body != nullptr);
        QVERIFY(!body->isEnabled());
        QVERIFY(!body->isHidden());
        auto* note = child<QLabel>(&page, "headphonesNote");
        QVERIFY(note != nullptr);
        QCOMPARE(note->text(), QStringLiteral("The PC volume does not change the headphones."));
        QVERIFY(note->isEnabled());
        for (QCheckBox* box : headphones->findChildren<QCheckBox*>()) {
            if (box->text() == QStringLiteral("Enabled")) { box->setChecked(true); }
        }
        QVERIFY(body->isEnabled());

        for (DeviceCard* card : page.findChildren<DeviceCard*>()) {
            QVERIFY(!card->detailsExpanded());
        }
        QCOMPARE(page.findChildren<QToolButton*>(QStringLiteral("deviceDetailsToggle")).size(), 2);
    }

    // 6. No radio: every radio speaker control disabled, never hidden.
    void noRadio()
    {
        RadioModel model;
        AudioOutputsPage page(&model);
        const Radio r = radioOf(&page);
        QVERIFY(r.volume && r.mute && r.icon && r.readout && r.status && r.amp && r.normal
                && r.offOnTx && r.alwaysOff && r.ampReason && r.ampStatus && r.note);
        QCOMPARE(r.status->text(), kNoRadio);
        QCOMPARE(page.radioSpeakerStatusText(), kNoRadio);
        for (QWidget* w : {static_cast<QWidget*>(r.volume), static_cast<QWidget*>(r.mute),
                           static_cast<QWidget*>(r.icon), r.amp}) {
            QVERIFY(!w->isEnabled());
            QVERIFY(!w->isHidden());
        }
        QCOMPARE(r.readout->text(), QStringLiteral("--"));
        QCOMPARE(iconOf(r.icon), QStringLiteral("radio-none"));
        QCOMPARE(r.volume->toolTip(), kNoRadio);
        QCOMPARE(r.ampReason->text(), kNoRadio);
        QCOMPARE(r.amp->toolTip(), kNoRadio);
        QCOMPARE(r.mute->text(), QStringLiteral("Mute radio speaker"));
        QVERIFY(r.note->text().startsWith(QStringLiteral("Same control as RADIO")));
        QVERIFY(r.ampStatus->text().isEmpty());
    }

    // 6. Writes reach the model and the model's changes come back; the page
    // writes nothing while it builds.
    void radioSpeakerWritesAndFollowsTheModel()
    {
        RadioModel model;
        SpeakerConnection conn;
        const auto detach = qScopeGuard([&model]() { model.injectConnectionForTest(nullptr); });
        connectRadio(model, conn, HPSDRModel::ANAN_G2, 2);
        model.setRadioSpeakerVolume(40);
        model.setRadioSpeakerMuted(false);
        model.setSpeakerAmplifierMode(0);

        QSignalSpy volume(&model, &RadioModel::radioSpeakerVolumeChanged);
        QSignalSpy muted(&model, &RadioModel::radioSpeakerMutedChanged);
        QSignalSpy mode(&model, &RadioModel::speakerAmplifierModeChanged);
        AudioOutputsPage page(&model);
        QCOMPARE(volume.count(), 0);
        QCOMPARE(muted.count(), 0);
        QCOMPARE(mode.count(), 0);

        const Radio r = radioOf(&page);
        QCOMPARE(r.status->text(), QStringLiteral("Speaker output on the ANAN-G2."));
        QVERIFY(r.volume->isEnabled());
        QCOMPARE(r.volume->value(), 40);
        QCOMPARE(r.readout->text(), QStringLiteral("40"));
        QCOMPARE(iconOf(r.icon), QStringLiteral("radio-on"));

        r.volume->setValue(65);
        QCOMPARE(model.radioSpeakerVolume(), 65);
        QCOMPARE(volume.count(), 1);
        r.mute->setChecked(true);
        QVERIFY(model.radioSpeakerMuted());
        QVERIFY(r.icon->isChecked());
        QCOMPARE(iconOf(r.icon), QStringLiteral("radio-muted"));
        r.icon->click();
        QVERIFY(!model.radioSpeakerMuted());
        QVERIFY(!r.mute->isChecked());

        model.setRadioSpeakerVolume(12);
        QCOMPARE(r.volume->value(), 12);
        QCOMPARE(volume.count(), 2);  // the page did not write it back
        model.setRadioSpeakerMuted(true);
        QVERIFY(r.mute->isChecked());

        // Amplifier choice: G2 on Protocol 2 has it.
        QVERIFY(r.amp->isEnabled());
        QVERIFY(r.ampReason->text().isEmpty());
        QVERIFY(r.normal->isChecked());
        r.offOnTx->click();
        QCOMPARE(model.speakerAmplifierMode(), 1);
        r.alwaysOff->click();
        QCOMPARE(model.speakerAmplifierMode(), 2);
        QCOMPARE(r.ampStatus->text(), model.speakerAmplifierStatus());
        QVERIFY(!r.ampStatus->text().isEmpty());
        model.setSpeakerAmplifierMode(0);
        QVERIFY(r.normal->isChecked());
        model.setRadioSpeakerMuted(false);
        QCOMPARE(r.ampStatus->text(), model.speakerAmplifierStatus());
        QVERIFY(r.ampStatus->text().isEmpty());
        model.setRadioSpeakerMuted(true);
        QCOMPARE(r.ampStatus->text(), QStringLiteral("Amplifier is off now: radio speaker muted."));
    }

    // 6. Amplifier greyed with the model's reason; the HL2 add-on status.
    void amplifierGreyedWithTheModelsReason_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<int>("protocol");
        QTest::addColumn<QString>("reason");
        QTest::addColumn<QString>("status");
        QTest::newRow("hl2") << int(HPSDRModel::HERMESLITE) << 1 << kNoAmp
                             << QStringLiteral("Headphone output on the Hermes Lite 2. It needs "
                                               "the audio add-on board; the radio cannot report "
                                               "whether it has one.");
        QTest::newRow("anan100d") << int(HPSDRModel::ANAN100D) << 1 << kNoAmp
                                  << QStringLiteral("Speaker output on the ANAN-100D.");
        QTest::newRow("g2e") << int(HPSDRModel::ANAN_G2E) << 2 << kG2e
                             << QStringLiteral("Speaker output on the ANAN-G2E.");
    }

    void amplifierGreyedWithTheModelsReason()
    {
        QFETCH(int, board);
        QFETCH(int, protocol);
        QFETCH(QString, reason);
        QFETCH(QString, status);
        RadioModel model;
        SpeakerConnection conn;
        const auto detach = qScopeGuard([&model]() { model.injectConnectionForTest(nullptr); });
        connectRadio(model, conn, static_cast<HPSDRModel>(board), protocol);
        AudioOutputsPage page(&model);
        const Radio r = radioOf(&page);
        QCOMPARE(r.status->text(), status);
        QVERIFY(r.volume->isEnabled());
        QVERIFY(!r.amp->isEnabled());
        QVERIFY(!r.amp->isHidden());
        QCOMPARE(r.amp->toolTip(), reason);
        QCOMPARE(r.ampReason->text(), reason);
        QVERIFY(r.ampStatus->text().isEmpty());
    }

    // 7. Remote: the Core's text and reasons; the Core's settings
    // unavailable gate only the radio speaker controls.
    void remoteWindow()
    {
        RadioModel remote(RadioModel::Role::Remote);
        SpeakerLink link;
        remote.attachStation(&link);
        const auto detach = qScopeGuard([&remote]() { remote.attachStation(nullptr); });
        AudioOutputsPage page(&remote);
        const Radio r = radioOf(&page);
        QCOMPARE(r.note->text(),
                 QStringLiteral("This is the speaker at the Core. Changes here reach every window "
                                "and the phone. Each slice's AF level and mute still apply."));

        remote.setStationConnectionState(ConnectionState::Connected);
        QCOMPARE(r.status->text(), kOlderCore);
        QVERIFY(!r.volume->isEnabled());

        link.offers = true;
        QVERIFY(remote.applyStationRadioSpeakerValue("radioSpeakerAvailability",
                                                     RadioModel::kRadioSpeakerAvailable));
        QVERIFY(r.status->text().endsWith(QStringLiteral(" at the Core.")));
        QVERIFY(r.volume->isEnabled());
        QCOMPARE(r.volume->toolTip(),
                 QStringLiteral("Radio speaker at the Core (shared with every window and the phone)"));
        r.volume->setValue(33);
        QCOMPARE(remote.radioSpeakerVolume(), 33);
        QVERIFY(remote.applyStationRadioSpeakerValue("speakerAmplifierAvailable", true));
        QVERIFY(r.amp->isEnabled());

        page.setStationSettingsAvailable(false, kStationReason);
        int gated = 0;
        for (QWidget* w : page.findChildren<QWidget*>()) {
            if (w->accessibleDescription() == kStationReason) {
                ++gated;
                QVERIFY(!w->isEnabled());
                QCOMPARE(w->toolTip(), kStationReason);
            }
        }
        QCOMPARE(gated, 4);
        QVERIFY(child<QSlider>(&page, "pcVolume")->isEnabled());
        QVERIFY(child<QPushButton>(&page, "rescanDevices")->isEnabled());

        page.setStationSettingsAvailable(true, QString());
        QVERIFY(r.volume->isEnabled());
        QVERIFY(r.amp->isEnabled());
        QVERIFY(r.volume->accessibleDescription() != kStationReason);
    }

    // 8. Sound system line texts on every system.
    void soundSystemTexts()
    {
        using S = SoundSystemLine::System;
        QCOMPARE(SoundSystemLine::describe(S::Mac, LinuxAudioBackend::None),
                 QStringLiteral("Core Audio"));
        QVERIFY(SoundSystemLine::describe(S::Windows, LinuxAudioBackend::None)
                    .startsWith(QStringLiteral("Windows audio.")));
        QVERIFY(SoundSystemLine::describe(S::Windows, LinuxAudioBackend::None)
                    .contains(QStringLiteral("WASAPI")));
        QCOMPARE(SoundSystemLine::describe(S::Linux, LinuxAudioBackend::PipeWire),
                 QStringLiteral("PipeWire. NereusSDR talks to it directly."));
        QCOMPARE(SoundSystemLine::describe(S::Linux, LinuxAudioBackend::Pactl),
                 QStringLiteral("PulseAudio. PipeWire was not found, so NereusSDR uses the pactl "
                                "tool instead."));
        QCOMPARE(SoundSystemLine::describe(S::Linux, LinuxAudioBackend::None),
                 QStringLiteral("None found. Start PipeWire or PulseAudio, then click Rescan "
                                "devices."));
        QVERIFY(SoundSystemLine::isProblem(S::Linux, LinuxAudioBackend::None));
        QVERIFY(!SoundSystemLine::isProblem(S::Linux, LinuxAudioBackend::PipeWire));
        QVERIFY(!SoundSystemLine::isProblem(S::Mac, LinuxAudioBackend::None));
        QVERIFY(!SoundSystemLine::isProblem(S::Windows, LinuxAudioBackend::None));

        RadioModel model;
        AudioOutputsPage page(&model);
        auto* line = page.findChild<SoundSystemLine*>(QStringLiteral("soundSystemLine"));
        QVERIFY(line != nullptr);
#if defined(Q_OS_MAC)
        QCOMPARE(line->text(), QStringLiteral("Core Audio"));
        QVERIFY(!line->showsProblem());
#elif defined(Q_OS_WIN)
        QVERIFY(line->text().startsWith(QStringLiteral("Windows audio.")));
#else
        QCOMPARE(line->text(), SoundSystemLine::describe(
                                   S::Linux, model.localAudioDevices()->linuxBackend()));
#endif
        auto* text = line->findChild<QLabel*>(QStringLiteral("soundSystemText"));
        QVERIFY(text != nullptr && text->text().contains(QStringLiteral("Sound system:")));
    }

    // 9. One Rescan devices, reporting what it found.
    void rescanDevices()
    {
        RadioModel model;
        AudioOutputsPage page(&model);
        QCOMPARE(page.findChildren<QPushButton*>(QStringLiteral("rescanDevices")).size(), 1);
        auto* button = child<QPushButton>(&page, "rescanDevices");
        QCOMPARE(button->text(), QStringLiteral("Rescan devices"));
        button->click();
        QVERIFY(child<QLabel>(&page, "rescanDevicesResult")->text().startsWith(
            QStringLiteral("Found ")));
    }

    // 10. Captures (NEREUS_AUDIO_SETUP_CAPTURE_DIR).
    void captures()
    {
        if (qEnvironmentVariable("NEREUS_AUDIO_SETUP_CAPTURE_DIR").isEmpty()) {
            QSKIP("Set NEREUS_AUDIO_SETUP_CAPTURE_DIR to save the captures.");
        }
        auto shoot = [](RadioModel& model, const QString& stem, bool unfold) {
            AudioOutputsPage page(&model);
            page.resize(760, unfold ? 1500 : 980);
            if (unfold) {
                for (DeviceCard* card : page.findChildren<DeviceCard*>()) {
                    card->setDetailsExpanded(true);
                }
            }
            page.show();
            QApplication::processEvents();
            saveCapture(&page, stem);
        };

        {
            RadioModel model;
            shoot(model, QStringLiteral("outputs-no-radio-folded"), false);
            shoot(model, QStringLiteral("outputs-no-radio-unfolded"), true);
        }
        {
            RadioModel model;
            SpeakerConnection conn;
            const auto detach = qScopeGuard([&model]() { model.injectConnectionForTest(nullptr); });
            connectRadio(model, conn, HPSDRModel::ANAN_G2, 2);
            model.setRadioSpeakerVolume(60);
            shoot(model, QStringLiteral("outputs-g2-amp-enabled"), false);
            model.setSpeakerAmplifierMode(2);
            shoot(model, QStringLiteral("outputs-g2-amp-always-off"), false);
            model.setSpeakerAmplifierMode(0);
        }
        {
            RadioModel model;
            SpeakerConnection conn;
            const auto detach = qScopeGuard([&model]() { model.injectConnectionForTest(nullptr); });
            connectRadio(model, conn, HPSDRModel::HERMESLITE, 1);
            shoot(model, QStringLiteral("outputs-hl2-addon-amp-greyed"), false);
        }
        {
            RadioModel model;
            SpeakerConnection conn;
            const auto detach = qScopeGuard([&model]() { model.injectConnectionForTest(nullptr); });
            connectRadio(model, conn, HPSDRModel::ANAN_G2E, 2);
            shoot(model, QStringLiteral("outputs-g2e-amp-greyed"), false);
        }
        {
            RadioModel remote(RadioModel::Role::Remote);
            SpeakerLink link;
            link.offers = true;
            remote.attachStation(&link);
            const auto detach = qScopeGuard([&remote]() { remote.attachStation(nullptr); });
            remote.setStationConnectionState(ConnectionState::Connected);
            QVERIFY(remote.applyStationRadioSpeakerValue("radioSpeakerAvailability",
                                                         RadioModel::kRadioSpeakerAvailable));
            QVERIFY(remote.applyStationRadioSpeakerValue("speakerAmplifierAvailable", true));
            shoot(remote, QStringLiteral("outputs-remote-folded"), false);
            shoot(remote, QStringLiteral("outputs-remote-unfolded"), true);
        }
    }
};

QTEST_MAIN(TstAudioSetupRegroup)
#include "tst_audio_setup_regroup.moc"
