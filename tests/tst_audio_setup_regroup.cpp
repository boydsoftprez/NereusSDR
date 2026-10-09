// no-port-check: test-only, NereusSDR-original. No upstream logic is
// ported here.
//
// =================================================================
// tests/tst_audio_setup_regroup.cpp  (NereusSDR)
// =================================================================
//
// Setup > Audio > Outputs (R-SPK-21 Outputs, R-SPK-22, R-SPK-24, D13 to
// D15) and Setup > Audio > Microphone (R-SPK-21 Microphone, R-SPK-22).
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
//   8. Sound system texts for Mac, Windows and every Linux backend, and the
//      line from a fake catalogue naming the older drivers in use.
//   9. One "Rescan devices": it rescans the older drivers and says the
//      native lists update by themselves; greyed with the reason on the Mac.
//  10. With NEREUS_AUDIO_SETUP_CAPTURE_DIR set, captures of the page in its
//      states (run once plain and once with QT_SCALE_FACTOR=2).
//  11. Microphone follows Outputs, Mixed; Devices and TX Input are gone.
//  12. The PC microphone card, its status, its one Retry and its Device
//      details appear once across Setup, on Microphone.
//  13. All 13 audio.txInput nereusSetupIds are present exactly once.
//  14. The card writes the same audio/TxInput keys as both former places.
//  15. Sources not picked stay in view, greyed.
//  16. Radio mic follows radioMicSelectable() and its reasons, with the
//      board's group or the placeholder in view.
//  17. Mic gain is its own group, outside both sources.
//  18. Captures of the Microphone page (as 10).
//  19. Audio reads Outputs, Microphone, Digital modes, TX Profile,
//      Advanced; VAX and TCI are one Digital modes page, ThisComputer.
//  20. Each Digital modes and Advanced control appears once across Setup
//      (four times for a VAX card's own), on its page.
//  21. VAX cards on a Mac or Linux build: Device shows NereusSDR VAX n,
//      no picker; Used by and Activity.
//  22. VAX cards laid out for Windows: a cable picker, "On" greyed until a
//      cable is picked; no Rename.
//  23. No text or tooltip on the VAX section says PipeWire on a Mac or
//      Windows layout.
//  24. The VAX status line for each system.
//  25. TCI: the sentence replaces the Master Mute box; Audio stream and
//      Transmit keep every key; the Opus note still shows.
//  26. Advanced holds Logs, Feature Flags and Reset, the DSP group hidden;
//      the cables row is on Digital modes.
//  27. Captures of Digital modes and Advanced (as 10).
//
// Modification history (NereusSDR):
//   2026-10-06 - Written for the radio speaker and Audio Setup plan, Task 9.
//                J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                Anthropic Claude Code.
//   2026-10-06 - The Microphone page (Task 10): cases 11 to 18; case 2 no
//                longer looks for a Devices page. J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-10-06 - Digital modes and Advanced (Task 11): cases 19 to 27.
//                J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                Anthropic Claude Code.
//   2026-10-09 - Native audio plan Task 16 (R-AUD-01, R-AUD-06, V-UI-1):
//                cases 8 and 9 on fake catalogues; the Outputs and
//                Microphone captures for the Mac, Windows, Linux PipeWire
//                and Linux PulseAudio. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-10-09 - Task 16 fix round (R-AUD-01, R-AUD-06, R-AUD-09): Rescan
//                devices greyed with its Mac note without the lists; every
//                capture under the app's look; the missing-mic captures
//                unfolded; each open Driver and Device list, reason
//                tooltips, the Sound system line and the engine notes.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - Case 23: the VAX channel cards show the laid-out
//                system's engine on any test build (the Linux build showed
//                PipeWire on a Mac and a Windows layout). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <array>
#include <utility>
#include <memory>
#include <vector>

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyleFactory>
#include <QToolButton>
#include <QToolTip>
#include <QTreeWidget>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/session/IStationLink.h"
#include "core/audio/AudioDeviceTypes.h"
#include "core/audio/IAudioDeviceCatalog.h"
#include "core/audio/PortAudioBackend.h"
#include "fakes/FakeAudioEngineBackend.h"
#include "gui/HGauge.h"
#include "gui/SetupDialog.h"
#include "gui/setup/AudioAdvancedPage.h"
#include "gui/setup/AudioDigitalModesPage.h"
#include "gui/setup/AudioOutputsPage.h"
#include "gui/setup/AudioTciPage.h"
#include "gui/setup/AudioVaxPage.h"
#include "gui/setup/AudioTxInputPage.h"
#include "gui/setup/DeviceCard.h"
#include "gui/setup/SoundSystemLine.h"
#include "gui/styles/AppTheme.h"
#include "gui/widgets/AppIcon.h"
#include "gui/widgets/MasterOutputWidget.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

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
    // -1: IStationLink's default (an older Core when it offers nothing);
    // 0 or 1: what StationClient knows once signed in.
    int needsNewer{-1};
    bool radioSpeakerAvailable() const override { return offers; }
    bool radioSpeakerNeedsNewerCore() const override
    {
        return needsNewer < 0 ? !offers : needsNewer == 1;
    }
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

// Counts objects, not only widgets: two ids sit on QButtonGroups.
int countSetupIdObjects(QObject* root, const QString& id)
{
    int n = 0;
    for (QObject* o : root->findChildren<QObject*>()) {
        if (o->property("nereusSetupId").toString() == id) {
            ++n;
        }
    }
    return n;
}

const QStringList kTxInputSetupIds{
    QStringLiteral("audio.txInput.micGain"),
    QStringLiteral("audio.txInput.hermesLineIn"),
    QStringLiteral("audio.txInput.hermesMicBoost"),
    QStringLiteral("audio.txInput.hermesLineInGain"),
    QStringLiteral("audio.txInput.orionMicTipRing"),
    QStringLiteral("audio.txInput.orionMicBias"),
    QStringLiteral("audio.txInput.orionMicPttDisabled"),
    QStringLiteral("audio.txInput.orionMicBoost"),
    QStringLiteral("audio.txInput.saturnMicXlr"),
    QStringLiteral("audio.txInput.saturnMicTipRing"),
    QStringLiteral("audio.txInput.saturnMicPttDisabled"),
    QStringLiteral("audio.txInput.saturnMicBias"),
    QStringLiteral("audio.txInput.saturnMicBoost"),
};

QRadioButton* sourceButton(QWidget* page, const QString& text)
{
    for (QRadioButton* b : page->findChildren<QRadioButton*>()) {
        if (b->text() == text) {
            return b;
        }
    }
    return nullptr;
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

DetectedCable outputCable(const QString& name)
{
    return DetectedCable{VirtualCableProduct::VbCable, name, false, 0};
}

// Every text and tooltip a widget under `root` carries.
QStringList textsUnder(QWidget* root)
{
    QStringList out;
    QList<QWidget*> all = root->findChildren<QWidget*>();
    all.prepend(root);
    for (QWidget* w : all) {
        out << w->toolTip();
        if (auto* l = qobject_cast<QLabel*>(w)) {
            out << l->text();
        } else if (auto* b = qobject_cast<QAbstractButton*>(w)) {
            out << b->text();
        } else if (auto* g = qobject_cast<QGroupBox*>(w)) {
            out << g->title();
        } else if (auto* c = qobject_cast<QComboBox*>(w)) {
            for (int i = 0; i < c->count(); ++i) {
                out << c->itemText(i);
            }
        }
    }
    out.removeAll(QString());
    return out;
}

// Lays VAX sections out for `system` until the guard goes.
auto vaxSystem(SoundSystemLine::System system)
{
    AudioVaxPage::setSystemForTest(system);
    return qScopeGuard([] { AudioVaxPage::setSystemForTest(std::nullopt); });
}

// Native audio plan Task 16: a fake catalogue for each system, on the
// model's own engine. The fake's backends decide the system the cards and
// the Sound system line show; nothing opens a device.
enum class FakeOs { Mac, Windows, LinuxPipeWire, LinuxPulseAudio };

AudioDeviceInfo fakeDevice(AudioBackendId backend, AudioDeviceDirection direction,
                           const QString& id, const QString& name, const QString& hostApi = {})
{
    AudioDeviceInfo info;
    info.backend = backend;
    info.direction = direction;
    info.id = id;
    info.name = name;
    info.hostApi = hostApi;
    return info;
}

struct FakeSystem {
    std::shared_ptr<FakeAudioEngineBackend> native;
    std::shared_ptr<FakeAudioEngineBackend> older;
    std::shared_ptr<FakeAudioEngineBackend> stoppedPipeWire;
};

// Builds the fake system's devices: two outputs (the built-in one the
// default), one held by another program, a USB mic and a Bluetooth
// headset's mic; the older drivers list one output on their host API.
FakeSystem fakeSystem(FakeOs os)
{
    AudioBackendId id = AudioBackendId::CoreAudio;
    QString hostApi;
    QString headset = QStringLiteral("AirPods");
    switch (os) {
    case FakeOs::Mac:
        break;
    case FakeOs::Windows:
        id = AudioBackendId::Wasapi;
        hostApi = QStringLiteral("MME");
        headset = QStringLiteral("Headset");
        break;
    case FakeOs::LinuxPipeWire:
        id = AudioBackendId::PipeWire;
        hostApi = QStringLiteral("ALSA");
        headset = QStringLiteral("Headset");
        break;
    case FakeOs::LinuxPulseAudio:
        id = AudioBackendId::PulseAudio;
        hostApi = QStringLiteral("ALSA");
        headset = QStringLiteral("Headset");
        break;
    }
    FakeSystem fake;
    fake.native = std::make_shared<FakeAudioEngineBackend>(id);
    AudioDeviceInfo busy = fakeDevice(id, AudioDeviceDirection::Output, QStringLiteral("studio-uid"),
                                      QStringLiteral("Studio monitor"));
    busy.state = AudioDeviceState::InUse;
    AudioDeviceInfo bluetooth = fakeDevice(id, AudioDeviceDirection::Input,
                                           QStringLiteral("headset-uid"), headset);
    bluetooth.transport = AudioTransport::Bluetooth;
    fake.native->setDevices(
        {fakeDevice(id, AudioDeviceDirection::Output, QStringLiteral("desk-uid"),
                    QStringLiteral("Desk speakers")),
         fakeDevice(id, AudioDeviceDirection::Output, QStringLiteral("built-in-uid"),
                    QStringLiteral("Built-in speakers")),
         busy,
         fakeDevice(id, AudioDeviceDirection::Input, QStringLiteral("usb-mic-uid"),
                    QStringLiteral("USB Mic")),
         bluetooth});
    fake.native->setDefault(AudioDeviceDirection::Output, QStringLiteral("built-in-uid"));
    fake.native->setDefault(AudioDeviceDirection::Input, QStringLiteral("usb-mic-uid"));
    if (os == FakeOs::LinuxPulseAudio) {
        fake.stoppedPipeWire = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PipeWire);
        fake.stoppedPipeWire->setRunning(false);
    }
    if (!hostApi.isEmpty()) {
        fake.older = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PortAudio);
        fake.older->setTakesStereoMix(false);
        fake.older->setDevices({fakeDevice(AudioBackendId::PortAudio, AudioDeviceDirection::Output,
                                           portAudioDeviceId(hostApi, QStringLiteral("Speakers")),
                                           QStringLiteral("Speakers"), hostApi)});
    }
    return fake;
}

// Puts the fake system on the model's engine and starts it, so the cards
// find its catalogue as the page is built.
void startOnFakeSystem(RadioModel& model, const FakeSystem& fake)
{
    std::vector<std::shared_ptr<IAudioEngineBackend>> backends;
    if (fake.stoppedPipeWire) {
        backends.push_back(fake.stoppedPipeWire);
    }
    backends.push_back(fake.native);
    if (fake.older) {
        backends.push_back(fake.older);
    }
    AudioEngine* engine = model.localAudioDevices();
    engine->setVaxOutputsAllowed(false);
    engine->setAudioBackendsForTest(std::move(backends));
    engine->start();
    QVERIFY(engine->catalogue() != nullptr);
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

        // The speakers and headphones cards are on Outputs only; the
        // microphone card is on Microphone (R-SPK-21, case 12).
        int speakers = 0;
        int headphones = 0;
        for (DeviceCard* card : dialog.findChildren<DeviceCard*>()) {
            speakers += card->title() == QStringLiteral("This computer")
                || card->title() == QStringLiteral("Speakers");
            headphones += card->title() == QStringLiteral("Headphones");
        }
        QCOMPARE(speakers, 1);
        QCOMPARE(headphones, 1);
        QVERIFY(!dialog.pageLabelsForTest().contains(QStringLiteral("Devices")));
        QCOMPARE(countNamed(&dialog, QStringLiteral("radioSpeakerExplanation")), 0);
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
        QComboBox* channels = nullptr;
        for (QComboBox* combo : speakers->findChildren<QComboBox*>()) {
            if (combo->findText(QStringLiteral("1 (Mono)")) >= 0) {
                channels = combo;
            }
        }
        QVERIFY(channels != nullptr);
        channels->setCurrentIndex(channels->findData(1));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("audio/Speakers/Channels")).toString(),
                 QStringLiteral("1"));
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
        // The Core's capabilities have not named its radio: "the radio",
        // never the remote profile's default board.
        QCOMPARE(r.status->text(), QStringLiteral("Speaker output on the radio at the Core."));
        QVERIFY(r.volume->isEnabled());
        QVERIFY(r.readout->isEnabled());
        QCOMPARE(r.volume->toolTip(),
                 QStringLiteral("Radio speaker at the Core (shared with every window and the phone)"));
        r.volume->setValue(33);
        QCOMPARE(remote.radioSpeakerVolume(), 33);
        QVERIFY(remote.applyStationRadioSpeakerValue("speakerAmplifierAvailable", true));
        QVERIFY(r.amp->isEnabled());
        QCOMPARE(r.amp->toolTip(), AudioOutputsPage::speakerAmplifierToolTip());

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
        // The readout follows the slider.
        QVERIFY(!r.readout->isEnabled());
        QVERIFY(child<QSlider>(&page, "pcVolume")->isEnabled());
        // Rescan devices is this computer's: never the station's reason.
        // R-AUD-06: on the Mac it is greyed, as Core Audio needs no rescan.
        auto* rescan = child<QPushButton>(&page, "rescanDevices");
        QVERIFY(rescan->accessibleDescription() != kStationReason);
        QVERIFY(rescan->toolTip() != kStationReason);
#if defined(Q_OS_MAC)
        QVERIFY(!rescan->isEnabled());
#else
        QVERIFY(rescan->isEnabled());
#endif

        page.setStationSettingsAvailable(true, QString());
        QVERIFY(r.volume->isEnabled());
        QVERIFY(r.amp->isEnabled());
        QVERIFY(r.volume->accessibleDescription() != kStationReason);
    }

    // 7b. The Outputs status follows the link alone: an older Core with
    // no radio signing in changes the reason, nothing else.
    void outputsReasonFollowsTheLinkAlone()
    {
        RadioModel remote(RadioModel::Role::Remote);
        SpeakerLink link;
        link.needsNewer = 0;
        remote.attachStation(&link);
        const auto detach = qScopeGuard([&remote]() { remote.attachStation(nullptr); });
        remote.setStationConnectionState(ConnectionState::Connected);
        AudioOutputsPage page(&remote);
        const Radio r = radioOf(&page);
        QCOMPARE(r.status->text(), kNoRadio);

        link.needsNewer = 1;
        remote.reportStationLinkStateChanged();
        QCOMPARE(r.status->text(), kOlderCore);
        QCOMPARE(r.volume->toolTip(), kOlderCore);
        QCOMPARE(r.ampReason->text(), kOlderCore);
    }

    // 8. Sound system line texts on every system.
    void soundSystemTexts()
    {
        using S = SoundSystemLine::System;
        QCOMPARE(SoundSystemLine::describe(S::Mac, LinuxAudioBackend::None),
                 QStringLiteral("Core Audio"));
        QCOMPARE(SoundSystemLine::describe(S::Windows, LinuxAudioBackend::None),
                 QStringLiteral("Windows audio (WASAPI)"));
        QCOMPARE(SoundSystemLine::describe(S::Linux, LinuxAudioBackend::PipeWire),
                 QStringLiteral("PipeWire. NereusSDR talks to it directly."));
        QCOMPARE(SoundSystemLine::describe(S::Linux, LinuxAudioBackend::Pactl),
                 QStringLiteral("PulseAudio. PipeWire was not found, so NereusSDR talks to "
                                "PulseAudio directly."));
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
        QVERIFY(line->text().startsWith(QStringLiteral("Windows audio (WASAPI)")));
#else
        QCOMPARE(line->text(), SoundSystemLine::describe(
                                   S::Linux, model.localAudioDevices()->linuxBackend()));
#endif
        auto* text = line->findChild<QLabel*>(QStringLiteral("soundSystemText"));
        QVERIFY(text != nullptr && text->text().contains(QStringLiteral("Sound system:")));
    }

    // 9. One Rescan devices (R-AUD-06): it rescans the older drivers and
    // says the native lists update by themselves; on the Mac it is greyed
    // with the reason.
    void rescanDevices_data()
    {
        QTest::addColumn<int>("os");
        QTest::addColumn<QString>("note");
        QTest::newRow("windows")
            << int(FakeOs::Windows)
            << QStringLiteral("Only the older drivers need this. Windows audio and ASIO lists "
                              "update by themselves.");
        QTest::newRow("pipewire")
            << int(FakeOs::LinuxPipeWire)
            << QStringLiteral("Only the older drivers need this. PipeWire lists update by "
                              "themselves.");
        QTest::newRow("pulseaudio")
            << int(FakeOs::LinuxPulseAudio)
            << QStringLiteral("Only the older drivers need this. PulseAudio lists update by "
                              "themselves.");
    }
    void rescanDevices()
    {
        QFETCH(int, os);
        QFETCH(QString, note);
        RadioModel model;
        const FakeSystem fake = fakeSystem(static_cast<FakeOs>(os));
        startOnFakeSystem(model, fake);
        AudioOutputsPage page(&model);
        QCOMPARE(page.findChildren<QPushButton*>(QStringLiteral("rescanDevices")).size(), 1);
        auto* button = child<QPushButton>(&page, "rescanDevices");
        auto* result = child<QLabel>(&page, "rescanDevicesResult");
        QCOMPARE(button->text(), QStringLiteral("Rescan devices"));
        QVERIFY(button->isEnabled());
        QCOMPARE(result->text(), note);
        QSignalSpy rescanned(model.localAudioDevices()->catalogue(),
                             &IAudioDeviceCatalog::olderDriversRescanned);
        button->click();
        QVERIFY(rescanned.wait(5000));
        QCOMPARE(fake.older->rescanCount(), 1);
        QCOMPARE(fake.native->rescanCount(), 0);
        QCOMPARE(result->text(), note);
        model.localAudioDevices()->stop();
    }

    void rescanDevicesGreyedOnTheMac()
    {
        RadioModel model;
        const FakeSystem fake = fakeSystem(FakeOs::Mac);
        startOnFakeSystem(model, fake);
        AudioOutputsPage page(&model);
        auto* button = child<QPushButton>(&page, "rescanDevices");
        const QString reason =
            QStringLiteral("Core Audio lists update by themselves, so there is nothing to rescan.");
        QVERIFY(!button->isEnabled());
        QVERIFY(!button->isHidden());
        QCOMPARE(button->toolTip(), reason);
        QCOMPARE(child<QLabel>(&page, "rescanDevicesResult")->text(), reason);
        model.localAudioDevices()->stop();
    }

    // R-AUD-06: the button follows the same rule as its note, with or
    // without the device lists, and stays so once the page is shown.
    void rescanDevicesFollowsItsNoteWithoutTheLists()
    {
        RadioModel model;
        AudioOutputsPage page(&model);
        page.resize(760, 980);
        page.show();
        QApplication::processEvents();
        QApplication::processEvents();
        auto* button = child<QPushButton>(&page, "rescanDevices");
        const QString note = child<QLabel>(&page, "rescanDevicesResult")->text();
#if defined(Q_OS_MAC)
        QCOMPARE(note,
                 QStringLiteral("Core Audio lists update by themselves, so there is nothing to rescan."));
        QVERIFY(!button->isEnabled());
        QCOMPARE(button->toolTip(), note);
#else
        QVERIFY(button->isEnabled());
#endif
    }

    // 8. The Sound system line from the catalogue, naming the older
    // drivers the cards use.
    void soundSystemLineFollowsTheCatalogue_data()
    {
        QTest::addColumn<int>("os");
        QTest::addColumn<QString>("line");
        QTest::newRow("mac") << int(FakeOs::Mac) << QStringLiteral("Core Audio");
        QTest::newRow("windows") << int(FakeOs::Windows)
                                 << QStringLiteral("Windows audio (WASAPI). Older drivers in "
                                                   "use: MME.");
        QTest::newRow("pipewire") << int(FakeOs::LinuxPipeWire)
                                  << QStringLiteral("PipeWire. NereusSDR talks to it directly. "
                                                    "Older drivers in use: ALSA.");
        QTest::newRow("pulseaudio")
            << int(FakeOs::LinuxPulseAudio)
            << QStringLiteral("PulseAudio. PipeWire was not found, so NereusSDR talks to "
                              "PulseAudio directly. Older drivers in use: ALSA.");
    }
    void soundSystemLineFollowsTheCatalogue()
    {
        QFETCH(int, os);
        QFETCH(QString, line);
        const FakeSystem fake = fakeSystem(static_cast<FakeOs>(os));
        if (fake.older) {
            AudioDeviceConfig speakers;
            speakers.engine = AudioEngineKind::PortAudio;
            speakers.driverApi = fake.older->enumerate().front().hostApi;
            speakers.deviceId = fake.older->enumerate().front().id;
            speakers.deviceName = QStringLiteral("Speakers");
            speakers.saveToSettings(QStringLiteral("audio/Speakers"));
        }
        RadioModel model;
        startOnFakeSystem(model, fake);
        AudioOutputsPage page(&model);
        auto* sound = page.findChild<SoundSystemLine*>(QStringLiteral("soundSystemLine"));
        QVERIFY(sound != nullptr);
        QCOMPARE(sound->text(), line);
        QVERIFY(!sound->showsProblem());
        model.localAudioDevices()->stop();
    }

    // 11. Microphone follows Outputs, Mixed; Devices and TX Input are gone.
    void microphoneFollowsOutputsAndIsMixed()
    {
        RadioModel model;
        SetupDialog dialog(&model);
        QTreeWidgetItem* audio = audioCategory(dialog);
        QVERIFY(audio != nullptr);
        QCOMPARE(audio->child(0)->text(0), QStringLiteral("Outputs"));
        QCOMPARE(audio->child(1)->text(0), QStringLiteral("Microphone"));

        const QStringList labels = dialog.pageLabelsForTest();
        QCOMPARE(labels.count(QStringLiteral("Microphone")), 1);
        QVERIFY(!labels.contains(QStringLiteral("Devices")));
        QVERIFY(!labels.contains(QStringLiteral("TX Input")));
        const int mic = static_cast<int>(labels.indexOf(QStringLiteral("Microphone")));
        QCOMPARE(dialog.pageScopeAtForTest(mic), SetupScope::Mixed);

        // Audio factories return the page itself.
        dialog.selectPage(QStringLiteral("Microphone"));
        auto* page = qobject_cast<AudioTxInputPage*>(
            dialog.realizedPageForTest(QStringLiteral("Microphone")));
        QVERIFY(page != nullptr);
        QCOMPARE(page->pageTitle(), QStringLiteral("Microphone"));
    }

    // 12. The PC microphone card, its status, Retry and Device details,
    // once across Setup, on Microphone.
    void pcMicrophoneIsOnceAcrossSetup()
    {
        RadioModel model;
        SetupDialog dialog(&model);
        dialog.realizeAllPagesForTest();
        auto* page = qobject_cast<AudioTxInputPage*>(
            dialog.realizedPageForTest(QStringLiteral("Microphone")));
        QVERIFY(page != nullptr);

        for (const char* name : {"pcMicrophoneGroup", "captureStatus", "retryCapture",
                                 "micSourceNote", "radioMicSection", "radioMicPlaceholder",
                                 "micGainGroup"}) {
            QVERIFY2(countNamed(&dialog, QLatin1String(name)) == 1, name);
            QVERIFY2(countNamed(page, QLatin1String(name)) == 1, name);
        }
        int micCards = 0;
        for (DeviceCard* card : dialog.findChildren<DeviceCard*>()) {
            micCards += card->title() == QStringLiteral("PC microphone")
                || card->title() == QStringLiteral("TX Input (Microphone)");
        }
        QCOMPARE(micCards, 1);
        DeviceCard* card = page->pcMicCard();
        QVERIFY(card != nullptr);
        QCOMPARE(card->title(), QStringLiteral("PC microphone"));
        QCOMPARE(page->pcMicGroupBox(), static_cast<QGroupBox*>(card));
        QCOMPARE(countNamed(card, QStringLiteral("deviceDetailsToggle")), 1);
        QVERIFY(!card->detailsExpanded());

        // Everything the section holds is inside the one card.
        QVERIFY(card->isAncestorOf(page->deviceCombo()));
        QVERIFY(card->isAncestorOf(page->driverApiCombo()));
        QVERIFY(card->isAncestorOf(page->bufferSizeCombo()));
        QVERIFY(card->isAncestorOf(page->testMicButton()));
        QVERIFY(card->isAncestorOf(page->vuBar()));
        QVERIFY(card->isAncestorOf(page->captureStatusLabel()));
        QVERIFY(card->isAncestorOf(page->retryCaptureButton()));
        QCOMPARE(page->retryCaptureButton()->text(), QStringLiteral("Retry microphone"));
        int retries = 0;
        int monitors = 0;
        int tones = 0;
        for (QAbstractButton* b : dialog.findChildren<QAbstractButton*>()) {
            retries += b->text() == QStringLiteral("Retry microphone");
            monitors += b->text() == QStringLiteral("Monitor TX input during transmit");
            tones += b->text().startsWith(QStringLiteral("Enable tone check"));
        }
        QCOMPARE(retries, 1);
        QCOMPARE(monitors, 1);
        QCOMPARE(tones, 1);
        for (QAbstractButton* b : card->findChildren<QAbstractButton*>()) {
            if (b->text() == QStringLiteral("Monitor TX input during transmit")
                || b->text().startsWith(QStringLiteral("Enable tone check"))) {
                --monitors;
            }
        }
        QCOMPARE(monitors, -1);  // both rows are the card's own
    }

    // 13. All 13 existing audio.txInput ids, exactly once.
    void txInputSetupIdsArePresentOnce()
    {
        RadioModel model;
        SetupDialog dialog(&model);
        dialog.realizeAllPagesForTest();
        auto* page = dialog.realizedPageForTest(QStringLiteral("Microphone"));
        QVERIFY(page != nullptr);
        QCOMPARE(kTxInputSetupIds.size(), 13);
        for (const QString& id : kTxInputSetupIds) {
            QVERIFY2(countSetupIdObjects(&dialog, id) == 1, qPrintable(id));
            QVERIFY2(countSetupIdObjects(page, id) == 1, qPrintable(id));
        }
        // And no other audio.txInput id appeared.
        QSet<QString> seen;
        for (QObject* o : dialog.findChildren<QObject*>()) {
            const QString id = o->property("nereusSetupId").toString();
            if (id.startsWith(QStringLiteral("audio.txInput."))) {
                seen.insert(id);
            }
        }
        QCOMPARE(seen, QSet<QString>(kTxInputSetupIds.cbegin(), kTxInputSetupIds.cend()));

        // On every board family the ids stay on the same control.
        for (HPSDRModel board : {HPSDRModel::HERMES, HPSDRModel::ANAN7000D, HPSDRModel::ANAN_G2,
                                 HPSDRModel::HERMESLITE}) {
            RadioModel boardModel;
            boardModel.setHpsdrModelForTest(board);
            AudioTxInputPage boardPage(&boardModel);
            for (const QString& id : kTxInputSetupIds) {
                QVERIFY2(countSetupIdObjects(&boardPage, id) == 1, qPrintable(id));
            }
            QCOMPARE(boardPage.micGainSlider()->property("nereusSetupId").toString(),
                     QStringLiteral("audio.txInput.micGain"));
        }
    }

    // 14. The card writes the audio/TxInput keys both former places wrote.
    void pcMicrophoneKeepsItsKeys()
    {
        RadioModel model;
        AudioTxInputPage page(&model);
        QComboBox* buffer = page.bufferSizeCombo();
        QVERIFY(buffer != nullptr);
        const int next = buffer->findData(2048);
        QVERIFY(next >= 0);
        buffer->setCurrentIndex(next);  // debounced 200 ms
        QTRY_COMPARE(AppSettings::instance().value(QStringLiteral("audio/TxInput/BufferSamples"))
                         .toString(),
                     QStringLiteral("2048"));
        QCOMPARE(model.transmitModel().pcMicBufferSamples(), 2048);
        QCOMPARE(model.localAudioDevices()->txInputConfig().bufferSamples, 2048);
        QVERIFY(AppSettings::instance().contains(QStringLiteral("audio/TxInput/DriverApi")));
        QVERIFY(AppSettings::instance().contains(QStringLiteral("audio/TxInput/DeviceName")));
        for (const QString& k : AppSettings::instance().allKeys()) {
            if (k.startsWith(QStringLiteral("audio/"))) {
                QVERIFY2(k.startsWith(QStringLiteral("audio/TxInput/")), qPrintable(k));
            }
        }
        // The model's setter reaches the same card.
        model.transmitModel().setPcMicBufferSamples(512);
        QCOMPARE(buffer->currentData().toInt(), 512);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("audio/TxInput/BufferSamples"))
                     .toString(),
                 QStringLiteral("512"));
    }

    // 15. Sources not picked stay in view, greyed.
    void sourcesNotPickedStayInViewGreyed()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        AudioTxInputPage page(&model);
        QWidget* card = page.pcMicGroupBox();
        QWidget* radio = page.radioMicSection();
        QWidget* gain = page.micGainGroup();
        QVERIFY(card && radio && gain);
        QVERIFY(!page.saturnRadioMicGroup()->isHidden());
        QVERIFY(page.radioMicPlaceholder()->isHidden());

        struct Row { const char* button; bool pc; bool radio; MicSource source; };
        for (const Row& row : {Row{"Radio Mic", false, true, MicSource::Radio},
                               Row{"VAX TX (virtual device)", false, false, MicSource::Vax},
                               Row{"PC Mic", true, false, MicSource::Pc}}) {
            QRadioButton* b = sourceButton(&page, QLatin1String(row.button));
            QVERIFY2(b != nullptr && b->isEnabled(), row.button);
            b->click();
            QCOMPARE(model.transmitModel().micSource(), row.source);
            QVERIFY2(!card->isHidden() && !radio->isHidden() && !gain->isHidden(), row.button);
            QVERIFY2(!page.saturnRadioMicGroup()->isHidden(), row.button);
            QCOMPARE(card->isEnabled(), row.pc);
            QCOMPARE(radio->isEnabled(), row.radio);
            QCOMPARE(page.saturnRadioMicGroup()->isEnabled(), row.radio);
            QVERIFY2(gain->isEnabled(), row.button);
        }

        // A source set elsewhere (the Phone/CW applet) greys the same way.
        model.transmitModel().setMicSource(MicSource::Radio);
        QVERIFY(!card->isEnabled());
        QVERIFY(radio->isEnabled());
    }

    // 16. Radio mic follows radioMicSelectable() and its reasons.
    void radioMicFollowsSelectableAndReasons()
    {
        {
            // A stock board without a mic jack (the HL2 caps without the
            // add-on): Radio Mic disabled with its reason, the placeholder
            // in view.
            RadioModel model;
            model.setCapsHasMicJackForTest(false);
            model.setCapsHwForTest(HPSDRHW::HermesLite);
            AudioTxInputPage page(&model);
            QVERIFY(!model.boardCapabilities().radioMicSelectable());
            QRadioButton* radio = sourceButton(&page, QStringLiteral("Radio Mic"));
            QVERIFY(radio != nullptr);
            QVERIFY(!radio->isEnabled());
            QVERIFY(!radio->isHidden());
            QCOMPARE(radio->toolTip(), QStringLiteral("Radio mic jack not present on Hermes Lite 2"));
            QVERIFY(!page.radioMicPlaceholder()->isHidden());
            QCOMPARE(child<QLabel>(&page, "radioMicPlaceholderNote")->text(),
                     QStringLiteral("This radio has no mic jack."));
            QVERIFY(page.hermesRadioMicGroup()->isHidden());
        }
        {
            // The real HL2 row: selectable with the add-on note; its group
            // in view.
            RadioModel model;
            model.setHpsdrModelForTest(HPSDRModel::HERMESLITE);
            AudioTxInputPage page(&model);
            QVERIFY(model.boardCapabilities().radioMicSelectable());
            QRadioButton* radio = sourceButton(&page, QStringLiteral("Radio Mic"));
            QVERIFY(radio != nullptr && radio->isEnabled());
            QCOMPARE(radio->toolTip(), RadioModel::radioMicAddOnNote());
            QVERIFY(!page.hermesRadioMicGroup()->isHidden());
            QVERIFY(page.radioMicPlaceholder()->isHidden());
        }
        for (HPSDRModel board : {HPSDRModel::HERMES, HPSDRModel::ANAN7000D, HPSDRModel::ANAN_G2}) {
            RadioModel model;
            model.setHpsdrModelForTest(board);
            AudioTxInputPage page(&model);
            QRadioButton* radio = sourceButton(&page, QStringLiteral("Radio Mic"));
            QCOMPARE(radio->isEnabled(), model.boardCapabilities().radioMicSelectable());
            const int groups = int(!page.hermesRadioMicGroup()->isHidden())
                + int(!page.orionRadioMicGroup()->isHidden())
                + int(!page.saturnRadioMicGroup()->isHidden());
            QCOMPARE(groups, 1);
            QVERIFY(page.radioMicPlaceholder()->isHidden());
        }
    }

    // 16b. A page opened before the radio connected follows the radio when
    // it changes: the placeholder's note and the family group in view.
    void radioMicPlaceholderFollowsTheRadio()
    {
        RadioModel model;
        model.setCapsHwForTest(HPSDRHW::Unknown);
        AudioTxInputPage page(&model);
        QLabel* note = child<QLabel>(&page, "radioMicPlaceholderNote");
        QVERIFY(note != nullptr);
        QVERIFY(!page.radioMicPlaceholder()->isHidden());
        QCOMPARE(note->text(), QStringLiteral("Connect a radio to set up its mic jack."));

        model.setCapsHwForTest(HPSDRHW::Saturn);
        model.emitCurrentRadioChangedForTest();
        QVERIFY(page.radioMicPlaceholder()->isHidden());
        QVERIFY(!page.saturnRadioMicGroup()->isHidden());
        QVERIFY(page.hermesRadioMicGroup()->isHidden());

        model.setCapsHwForTest(HPSDRHW::HermesLite);
        model.setCapsHasMicJackForTest(false);
        model.emitCurrentRadioChangedForTest();
        QVERIFY(!page.radioMicPlaceholder()->isHidden());
        QVERIFY(page.saturnRadioMicGroup()->isHidden());
        QCOMPARE(note->text(), QStringLiteral("This radio has no mic jack."));
    }

    // 16c. A page opened before an HL2 connects takes the HL2's title, the
    // add-on notes on Radio Mic and the Hermes rows, and Radio Mic open.
    void microphonePageFollowsAnHl2Connecting()
    {
        RadioModel model;  // no radio: the Unknown board
        QCOMPARE(model.boardCapabilities().board, HPSDRHW::Unknown);
        AudioTxInputPage page(&model);
        QCOMPARE(page.hermesRadioMicGroup()->title(), QStringLiteral("Radio Mic (Hermes / Atlas)"));
        QLabel* addOn = child<QLabel>(&page, "radioMicAddOnNote");
        QVERIFY(addOn != nullptr);
        QVERIFY(addOn->isHidden());
        QRadioButton* radio = sourceButton(&page, QStringLiteral("Radio Mic"));
        QVERIFY(radio != nullptr);
        QVERIFY(radio->toolTip() != RadioModel::radioMicAddOnNote());

        // The real HL2 row: its add-on board takes the radio mic.
        model.setHpsdrModelForTest(HPSDRModel::HERMESLITE);
        QVERIFY(model.boardCapabilities().radioMicNeedsAddOn);
        model.emitCurrentRadioChangedForTest();
        const QString note = RadioModel::radioMicAddOnNote();
        QCOMPARE(page.hermesRadioMicGroup()->title(), QStringLiteral("Radio Mic (Hermes Lite 2)"));
        QVERIFY(!page.hermesRadioMicGroup()->isHidden());
        QVERIFY(!addOn->isHidden());
        QCOMPARE(addOn->text(), note);
        QVERIFY(radio->isEnabled());
        QCOMPARE(radio->toolTip(), note);
        const QList<QAbstractButton*> rows =
            page.hermesRadioMicGroup()->findChildren<QAbstractButton*>();
        QVERIFY(!rows.isEmpty());
        for (QAbstractButton* b : rows) {
            QCOMPARE(b->toolTip(), note);
        }
        QSlider* gain = page.hermesRadioMicGroup()->findChild<QSlider*>();
        QVERIFY(gain != nullptr);
        QCOMPARE(gain->toolTip(), note);

        // Back to no radio: the notes go.
        model.setCapsHwForTest(HPSDRHW::Unknown);
        model.emitCurrentRadioChangedForTest();
        QCOMPARE(page.hermesRadioMicGroup()->title(), QStringLiteral("Radio Mic (Hermes / Atlas)"));
        QVERIFY(addOn->isHidden());
        QVERIFY(gain->toolTip().isEmpty());
    }

    // 17. Mic gain is its own group, outside both sources.
    void micGainIsItsOwnGroup()
    {
        RadioModel model;
        AudioTxInputPage page(&model);
        QGroupBox* gain = page.micGainGroup();
        QVERIFY(gain != nullptr);
        QCOMPARE(gain->title(), QStringLiteral("Mic gain"));
        QVERIFY(gain->isAncestorOf(page.micGainSlider()));
        QVERIFY(!page.pcMicGroupBox()->isAncestorOf(page.micGainSlider()));
        QVERIFY(!page.radioMicSection()->isAncestorOf(page.micGainSlider()));
        QVERIFY(qobject_cast<QFormLayout*>(gain->layout()) != nullptr);
        int gainLabels = 0;
        for (QLabel* l : gain->findChildren<QLabel*>()) {
            gainLabels += l->text() == QStringLiteral("Mic Gain:");
        }
        QCOMPARE(gainLabels, 1);  // the row label the Setup description names
        // Live whichever source is picked.
        sourceButton(&page, QStringLiteral("VAX TX (virtual device)"))->click();
        QVERIFY(gain->isEnabled());
        QVERIFY(page.micGainSlider()->isEnabled());
    }

    // 19. The Audio pages and their order.
    void audioPagesAndOrder()
    {
        RadioModel model;
        SetupDialog dialog(&model);
        QTreeWidgetItem* audio = audioCategory(dialog);
        QVERIFY(audio != nullptr);
        QStringList order;
        for (int i = 0; i < audio->childCount(); ++i) {
            order << audio->child(i)->text(0);
        }
        QCOMPARE(order, (QStringList{QStringLiteral("Outputs"), QStringLiteral("Microphone"),
                                     QStringLiteral("Digital modes"),
                                     QStringLiteral("TX Profile"),
                                     QStringLiteral("Advanced")}));

        const QStringList labels = dialog.pageLabelsForTest();
        QVERIFY(!labels.contains(QStringLiteral("VAX")));
        QVERIFY(!labels.contains(QStringLiteral("TCI")));
        QCOMPARE(labels.count(QStringLiteral("Digital modes")), 1);
        const int digital = static_cast<int>(labels.indexOf(QStringLiteral("Digital modes")));
        QCOMPARE(dialog.pageScopeAtForTest(digital), SetupScope::ThisComputer);

        dialog.selectPage(QStringLiteral("Digital modes"));
        auto* page = qobject_cast<AudioDigitalModesPage*>(
            dialog.realizedPageForTest(QStringLiteral("Digital modes")));
        QVERIFY(page != nullptr);
        QCOMPARE(page->pageTitle(), QStringLiteral("Digital modes"));
        QVERIFY(page->vaxSection() != nullptr);
        QVERIFY(page->tciSection() != nullptr);

        // VAX first, then TCI.
        AudioDigitalModesPage shown(&model);
        auto* vaxHeading = child<QLabel>(&shown, "digitalModesVaxHeading");
        auto* tciHeading = child<QLabel>(&shown, "digitalModesTciHeading");
        QVERIFY(vaxHeading && tciHeading);
        shown.resize(760, 2400);
        shown.show();
        QApplication::processEvents();
        const auto yOf = [&shown](QWidget* w) { return w->mapTo(&shown, QPoint(0, 0)).y(); };
        QVERIFY(yOf(vaxHeading) < yOf(shown.vaxSection()));
        QVERIFY(yOf(shown.vaxSection()) < yOf(tciHeading));
        QVERIFY(yOf(tciHeading) < yOf(shown.tciSection()));
    }

    // 20. Every Digital modes and Advanced control once across Setup.
    void digitalModesAndAdvancedControlsArePresentOnce()
    {
        RadioModel model;
        SetupDialog dialog(&model);
        dialog.realizeAllPagesForTest();
        QWidget* digital = dialog.realizedPageForTest(QStringLiteral("Digital modes"));
        QWidget* advanced = dialog.realizedPageForTest(QStringLiteral("Advanced"));
        QVERIFY(digital && advanced);

        for (const char* name : {"vaxIntro", "vaxSystemStatus", "vaxSystemStatusDot",
                                 "vaxCompressedAudioNote", "detectedCablesLabel",
                                 "detectedCablesRescan", "tciSeparateNote",
                                 "tciAudioStreamGroup", "tciTransmitGroup",
                                 "tciSliceARateCombo", "tciStreamFormatCombo",
                                 "tciStreamChannelsCombo", "tciBlockSizeSpin",
                                 "tciTxChannelCombo", "tciTxBufferingSpin"}) {
            QVERIFY2(countNamed(&dialog, QLatin1String(name)) == 1, name);
            QVERIFY2(countNamed(digital, QLatin1String(name)) == 1, name);
        }
        QCOMPARE(static_cast<int>(dialog.findChildren<VaxChannelCard*>().size()), 4);
        for (const char* name : {"vaxEnable", "vaxFormat", "vaxConsumerLabel",
                                 "vaxLevelGauge", "vaxCardStatus", "vaxRename",
                                 "vaxCopyName"}) {
            QVERIFY2(countNamed(&dialog, QLatin1String(name)) == 4, name);
            QVERIFY2(countNamed(digital, QLatin1String(name)) == 4, name);
        }
        for (const char* name : {"audioAdvancedDspGroup", "audioAdvancedDspRate",
                                 "audioAdvancedDspBlockSize", "audioAdvancedLogsGroup",
                                 "openLogsFolder", "audioAdvancedFeatureFlagsGroup",
                                 "sendIqToVax", "txMonitorToVax",
                                 "muteVaxDuringTxOtherSlice", "audioAdvancedResetGroup",
                                 "resetAllAudio"}) {
            QVERIFY2(countNamed(&dialog, QLatin1String(name)) == 1, name);
            QVERIFY2(countNamed(advanced, QLatin1String(name)) == 1, name);
        }
        // The old Master Mute box and cables group are gone everywhere.
        for (QGroupBox* box : dialog.findChildren<QGroupBox*>()) {
            QVERIFY(box->title() != QStringLiteral("Master Mute Behavior"));
            QVERIFY(box->title() != QStringLiteral("Detected Virtual Cables"));
        }
    }

    // 21. A Mac or Linux layout names the channel's own device.
    void vaxCardsNameTheirDeviceOffWindows()
    {
        for (SoundSystemLine::System system :
             {SoundSystemLine::System::Mac, SoundSystemLine::System::Linux}) {
            const auto restore = vaxSystem(system);
            AudioVaxPage page(nullptr);
            QCOMPARE(countNamed(&page, QStringLiteral("vaxDevicePicker")), 0);
            for (int ch = 1; ch <= 4; ++ch) {
                VaxChannelCard* card = page.channelCard(ch);
                QVERIFY(card != nullptr);
                auto* device = child<QLabel>(card, "vaxDeviceName");
                QVERIFY(device != nullptr);
                QCOMPARE(device->text(), QStringLiteral("NereusSDR VAX %1").arg(ch));
                QVERIFY(child<QCheckBox>(card, "vaxEnable")->isEnabled());
                QVERIFY(child<QPushButton>(card, "vaxRename")->isEnabled());
                QVERIFY(child<QLabel>(card, "vaxCardStatus")->isHidden());
            }
            QStringList rows;
            for (QLabel* l : page.channelCard(1)->findChildren<QLabel*>()) {
                rows << l->text();
            }
            QVERIFY(rows.contains(QStringLiteral("Device:")));
            QVERIFY(rows.contains(QStringLiteral("Format:")));
            QVERIFY(rows.contains(QStringLiteral("Used by:")));
            QVERIFY(rows.contains(QStringLiteral("Activity:")));
            QVERIFY(!rows.contains(QStringLiteral("Level:")));
            QVERIFY(!rows.contains(QStringLiteral("Exposed to system as:")));
            QCOMPARE(page.introText(system), child<QLabel>(&page, "vaxIntro")->text());
        }
    }

    // 22. A Windows layout picks a cable per channel; "On" waits for it.
    void vaxCardsPickACableOnWindows()
    {
        const auto restore = vaxSystem(SoundSystemLine::System::Windows);
        AudioVaxPage page(nullptr);
        page.setDetectedCablesForTest({outputCable(QStringLiteral("CABLE Input")),
                                       outputCable(QStringLiteral("CABLE-A Input")),
                                       DetectedCable{VirtualCableProduct::VbCable,
                                                     QStringLiteral("CABLE Output"), true, 0}});
        VaxChannelCard* card = page.channelCard(1);
        QCOMPARE(countNamed(&page, QStringLiteral("vaxDeviceName")), 0);
        auto* picker = child<QComboBox>(card, "vaxDevicePicker");
        QVERIFY(picker != nullptr);
        // "(pick a cable)" and the two output cables; the input end is not offered.
        QCOMPARE(picker->count(), 3);
        QCOMPARE(picker->currentIndex(), 0);
        auto* on = child<QCheckBox>(card, "vaxEnable");
        QVERIFY(!on->isEnabled());
        QCOMPARE(on->toolTip(), QStringLiteral("Pick a cable first."));
        auto* line = child<QLabel>(card, "vaxCardStatus");
        QVERIFY(!line->isHidden());
        QCOMPARE(line->text(), QStringLiteral("Pick a cable first."));
        QVERIFY(!child<QPushButton>(card, "vaxRename")->isEnabled());
        QVERIFY(!child<QPushButton>(card, "vaxCopyName")->isEnabled());

        picker->setCurrentIndex(1);
        emit picker->activated(1);
        QCOMPARE(card->currentDeviceName(), QStringLiteral("CABLE Input"));
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("audio/Vax1/DeviceName")).toString(),
                 QStringLiteral("CABLE Input"));
        QVERIFY(on->isEnabled());
        QVERIFY(line->isHidden());
        QVERIFY(child<QPushButton>(card, "vaxCopyName")->isEnabled());
        QCOMPARE(picker->currentText(), QStringLiteral("CABLE Input"));

        // Back to no cable.
        picker->setCurrentIndex(0);
        emit picker->activated(0);
        QVERIFY(card->currentDeviceName().isEmpty());
        QVERIFY(!on->isEnabled());

        // The status line and cables row.
        QCOMPARE(page.statusLineText(), QStringLiteral("2 virtual cables found."));
        QVERIFY(!page.statusLineShowsProblem());
        QVERIFY(page.detectedCablesText().startsWith(QStringLiteral("Detected virtual cables: 3 cables")));
        page.setDetectedCablesForTest({});
        QCOMPARE(page.statusLineText(),
                 QStringLiteral("No virtual cable found. Install one, then click Rescan."));
        QVERIFY(page.statusLineShowsProblem());
        QCOMPARE(page.detectedCablesText(), QStringLiteral("Detected virtual cables: None."));
    }

    // 23. No PipeWire on a Mac or Windows layout.  The channel cards have
    // no device lists here, so their Driver list shows the laid-out
    // system's engine, whichever system the test build runs on.
    void vaxSectionSaysNoPipeWireOffLinux()
    {
        const std::array<std::pair<SoundSystemLine::System, AudioEngineKind>, 3> layouts{{
            {SoundSystemLine::System::Mac, AudioEngineKind::CoreAudio},
            {SoundSystemLine::System::Windows, AudioEngineKind::WindowsShared},
            {SoundSystemLine::System::Linux, AudioEngineKind::PipeWire},
        }};
        for (const auto& [system, engine] : layouts) {
            const auto restore = vaxSystem(system);
            AudioVaxPage page(nullptr);
            page.setDetectedCablesForTest({outputCable(QStringLiteral("CABLE Input"))});
            const QList<DeviceCard*> cards = page.findChildren<DeviceCard*>();
            QVERIFY(!cards.isEmpty());
            for (DeviceCard* card : cards) {
                QCOMPARE(card->driverApiCombo()->currentText(), audioEngineLabel(engine));
            }
            if (system == SoundSystemLine::System::Linux) {
                continue;
            }
            for (const QString& text : textsUnder(&page)) {
                QVERIFY2(!text.contains(QLatin1String("PipeWire"), Qt::CaseInsensitive),
                         qPrintable(text));
            }
        }
        // Linux names its own sound system.
        const auto restore = vaxSystem(SoundSystemLine::System::Linux);
        AudioVaxPage page(nullptr);
        QVERIFY(child<QPushButton>(page.channelCard(1), "vaxCopyName")
                    ->toolTip().contains(QLatin1String("nereussdr.vax-1")));
    }

    // 24. The status line for each system.
    void vaxStatusTexts_data()
    {
        QTest::addColumn<int>("system");
        QTest::addColumn<int>("backend");
        QTest::addColumn<int>("cables");
        QTest::addColumn<bool>("failed");
        QTest::addColumn<bool>("open");
        QTest::addColumn<bool>("anyOn");
        QTest::addColumn<QString>("text");
        QTest::addColumn<bool>("problem");
        const int mac = static_cast<int>(SoundSystemLine::System::Mac);
        const int win = static_cast<int>(SoundSystemLine::System::Windows);
        const int lin = static_cast<int>(SoundSystemLine::System::Linux);
        const int pw = static_cast<int>(LinuxAudioBackend::PipeWire);
        const int pactl = static_cast<int>(LinuxAudioBackend::Pactl);
        const int none = static_cast<int>(LinuxAudioBackend::None);
        QTest::newRow("mac off") << mac << none << 0 << false << false << false
                                 << QStringLiteral("No VAX channel is on.") << false;
        QTest::newRow("mac loaded") << mac << none << 0 << false << true << true
                                    << QStringLiteral("The NereusSDR VAX driver is loaded.") << false;
        QTest::newRow("mac other devices") << mac << none << 0 << false << false << true
            << QStringLiteral("The channels that are on use the devices shown below.") << false;
        QTest::newRow("mac failed") << mac << none << 0 << true << false << true
            << QStringLiteral("The NereusSDR VAX driver did not load. Allow it in System "
                              "Settings > Privacy & Security or reinstall NereusSDR, then "
                              "restart NereusSDR.") << true;
        QTest::newRow("win one") << win << none << 1 << false << false << false
                                 << QStringLiteral("1 virtual cable found.") << false;
        QTest::newRow("win none") << win << none << 0 << false << false << false
            << QStringLiteral("No virtual cable found. Install one, then click Rescan.") << true;
        QTest::newRow("linux pipewire") << lin << pw << 0 << false << true << true
            << QStringLiteral("VAX devices are made through PipeWire.") << false;
        QTest::newRow("linux pactl") << lin << pactl << 0 << false << false << false
            << QStringLiteral("VAX devices are made through PulseAudio (pactl).") << false;
        QTest::newRow("linux none") << lin << none << 0 << false << false << false
            << QStringLiteral("No sound system is running, so the VAX devices cannot be made.")
            << true;
        QTest::newRow("linux failed") << lin << pw << 0 << true << false << true
            << QStringLiteral("A VAX device could not be made. Check that PipeWire or "
                              "PulseAudio is running, then turn the channel off and on.")
            << true;
    }

    void vaxStatusTexts()
    {
        QFETCH(int, system);
        QFETCH(int, backend);
        QFETCH(int, cables);
        QFETCH(bool, failed);
        QFETCH(bool, open);
        QFETCH(bool, anyOn);
        QFETCH(QString, text);
        QFETCH(bool, problem);
        AudioVaxPage::StatusInputs in;
        in.system = static_cast<SoundSystemLine::System>(system);
        in.linuxBackend = static_cast<LinuxAudioBackend>(backend);
        in.cablesFound = cables;
        in.ownDeviceFailed = failed;
        in.ownDeviceOpen = open;
        in.anyOn = anyOn;
        QCOMPARE(AudioVaxPage::statusText(in), text);
        QCOMPARE(AudioVaxPage::statusIsProblem(in), problem);
        QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
        // A Mac card whose own device did not open says so on its line.
        if (in.system == SoundSystemLine::System::Mac && failed) {
            const auto restore = vaxSystem(SoundSystemLine::System::Mac);
            AudioVaxPage page(nullptr);
            VaxChannelCard* card = page.channelCard(1);
            child<QCheckBox>(card, "vaxEnable")->setChecked(true);
            card->setBusOpen(false);
            QCOMPARE(page.statusLineText(), text);
            QVERIFY(page.statusLineShowsProblem());
            QCOMPARE(child<QLabel>(card, "vaxCardStatus")->text(),
                     QStringLiteral("Not available until the VAX driver is allowed (see above)."));
            QVERIFY(!child<QLabel>(card, "vaxCardStatus")->isHidden());
            QCOMPARE(card->statusLineText(),
                     QStringLiteral("Not available until the VAX driver is allowed (see above)."));
        }
    }

    // 25. TCI: one sentence, two groups, every key, and the Opus note.
    void tciSectionKeepsItsKeys()
    {
        auto& s = AppSettings::instance();
        const QStringList keys{QStringLiteral("TciSliceA_OutputSampleRate"),
                               QStringLiteral("TciAudioStreamSampleType"),
                               QStringLiteral("TciAudioStreamChannels"),
                               QStringLiteral("TciAudioStreamSamples"),
                               QStringLiteral("TciTxChannel"),
                               QStringLiteral("TciTxStreamBufferingMs")};
        QMap<QString, QVariant> saved;
        for (const QString& k : keys) {
            if (s.contains(k)) {
                saved.insert(k, s.value(k));
            }
        }
        const auto putBack = qScopeGuard([&] {
            for (const QString& k : keys) {
                s.remove(k);
                if (saved.contains(k)) {
                    s.setValue(k, saved.value(k));
                }
            }
        });
        for (const QString& k : keys) {
            s.remove(k);
        }
        s.setValue(QStringLiteral("TciSliceA_OutputSampleRate"), QStringLiteral("96000"));
        s.setValue(QStringLiteral("TciAudioStreamSampleType"), QStringLiteral("Int16"));
        s.setValue(QStringLiteral("TciAudioStreamChannels"), 1);
        s.setValue(QStringLiteral("TciAudioStreamSamples"), 512);
        s.setValue(QStringLiteral("TciTxChannel"), QStringLiteral("Left"));
        s.setValue(QStringLiteral("TciTxStreamBufferingMs"), 80);

        RadioModel model;
        AudioTciPage tci(&model);
        auto* note = child<QLabel>(&tci, "tciSeparateNote");
        QVERIFY(note != nullptr);
        QCOMPARE(note->text(),
                 QStringLiteral("TCI audio is separate from the PC and radio speaker volumes; "
                                "muting either one does not mute TCI."));
        QVERIFY(OperatorWording::isPlain(note->text()));
        auto* stream = child<QGroupBox>(&tci, "tciAudioStreamGroup");
        auto* tx = child<QGroupBox>(&tci, "tciTransmitGroup");
        QVERIFY(stream && tx);
        QCOMPARE(stream->title(), QStringLiteral("Audio stream"));
        QCOMPARE(tx->title(), QStringLiteral("Transmit"));
        QCOMPARE(static_cast<int>(tci.findChildren<QGroupBox*>().size()), 2);

        auto* rate = child<QComboBox>(stream, "tciSliceARateCombo");
        auto* format = child<QComboBox>(stream, "tciStreamFormatCombo");
        auto* channels = child<QComboBox>(stream, "tciStreamChannelsCombo");
        auto* block = child<QSpinBox>(stream, "tciBlockSizeSpin");
        auto* txChannel = child<QComboBox>(tx, "tciTxChannelCombo");
        auto* buffering = child<QSpinBox>(tx, "tciTxBufferingSpin");
        QVERIFY(rate && format && channels && block && txChannel && buffering);
        QCOMPARE(rate->currentText(), QStringLiteral("96000"));
        QCOMPARE(format->currentText(), QStringLiteral("Int16"));
        QCOMPARE(channels->currentData().toInt(), 1);
        QCOMPARE(block->value(), 512);
        QCOMPARE(txChannel->currentText(), QStringLiteral("Left"));
        QCOMPARE(buffering->value(), 80);
        QStringList streamTexts = textsUnder(stream);
        QVERIFY(streamTexts.contains(QStringLiteral("Slices C and D are not available over TCI.")));

        rate->setCurrentIndex(rate->findText(QStringLiteral("192000")));
        format->setCurrentIndex(format->findText(QStringLiteral("Float32")));
        channels->setCurrentIndex(channels->findData(2));
        block->setValue(1024);
        txChannel->setCurrentIndex(txChannel->findText(QStringLiteral("Both")));
        buffering->setValue(120);
        QCOMPARE(s.value(QStringLiteral("TciSliceA_OutputSampleRate")).toString(),
                 QStringLiteral("192000"));
        QCOMPARE(s.value(QStringLiteral("TciAudioStreamSampleType")).toString(),
                 QStringLiteral("Float32"));
        QCOMPARE(s.value(QStringLiteral("TciAudioStreamChannels")).toInt(), 2);
        QCOMPARE(s.value(QStringLiteral("TciAudioStreamSamples")).toInt(), 1024);
        QCOMPARE(s.value(QStringLiteral("TciTxChannel")).toString(), QStringLiteral("Both"));
        QCOMPARE(s.value(QStringLiteral("TciTxStreamBufferingMs")).toInt(), 120);

        // The Opus note shows on Digital modes in a remote window.
        RadioModel remote(RadioModel::Role::Remote);
        SetupDialog dialog(&remote);
        dialog.setReceiverAudioNote(RemoteReceiverAudioNote::OpusChosen);
        dialog.selectPage(QStringLiteral("Digital modes"));
        QWidget* page = dialog.realizedPageForTest(QStringLiteral("Digital modes"));
        QVERIFY(page != nullptr);
        auto* opus = child<QLabel>(page, "vaxCompressedAudioNote");
        QVERIFY(opus != nullptr);
        QVERIFY(!opus->isHidden());
    }

    // 26. Advanced: Logs, Feature Flags, Reset; DSP hidden; no cables row.
    void advancedHoldsLogsFlagsAndReset()
    {
        RadioModel model;
        AudioAdvancedPage page(&model);
        page.resize(760, 900);
        page.show();
        QApplication::processEvents();
        auto* dsp = child<QGroupBox>(&page, "audioAdvancedDspGroup");
        auto* logs = child<QGroupBox>(&page, "audioAdvancedLogsGroup");
        auto* flags = child<QGroupBox>(&page, "audioAdvancedFeatureFlagsGroup");
        auto* reset = child<QGroupBox>(&page, "audioAdvancedResetGroup");
        QVERIFY(dsp && logs && flags && reset);
        QVERIFY(dsp->isHidden());
        QVERIFY(!logs->isHidden());
        QVERIFY(!reset->isHidden());
        auto* open = child<QPushButton>(logs, "openLogsFolder");
        QVERIFY(open != nullptr);
        QCOMPARE(open->text(), QStringLiteral("Open logs folder"));
        QVERIFY(open->isEnabled());
        QVERIFY(child<QPushButton>(reset, "resetAllAudio") != nullptr);
        QVERIFY(logs->mapTo(&page, QPoint(0, 0)).y() < reset->mapTo(&page, QPoint(0, 0)).y());
        QCOMPARE(countNamed(&page, QStringLiteral("detectedCablesLabel")), 0);
        QCOMPARE(countNamed(&page, QStringLiteral("detectedCablesRescan")), 0);
        for (QAbstractButton* b : page.findChildren<QAbstractButton*>()) {
            QVERIFY(b->text() != QStringLiteral("Rescan"));
        }
        for (const QString& text : textsUnder(&page)) {
            QVERIFY2(!text.contains(QLatin1String("cable"), Qt::CaseInsensitive)
                         || text.contains(QLatin1String("This will clear")),
                     qPrintable(text));
        }
    }

    // 10. Captures (NEREUS_AUDIO_SETUP_CAPTURE_DIR).
    void captures()
    {
        if (qEnvironmentVariable("NEREUS_AUDIO_SETUP_CAPTURE_DIR").isEmpty()) {
            QSKIP("Set NEREUS_AUDIO_SETUP_CAPTURE_DIR to save the captures.");
        }
        // Every capture under the app's own style, palette and baseline QSS
        // (main.cpp), so greyed and live text read as they do in the app.
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        applyDarkPalette(*qApp);
        applyAppBaselineQss(*qApp);
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
#if defined(Q_OS_MAC)
            // R-AUD-06: greyed on the Mac in every capture.
            if (child<QPushButton>(&page, "rescanDevices")->isEnabled()) {
                QFAIL(qPrintable(stem));
            }
#endif
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

        // 18. The Microphone page (the look is set above).
        auto shootMic = [](RadioModel& model, const QString& stem, bool unfold,
                           const QString& source) {
            AudioTxInputPage page(&model);
            page.resize(760, unfold ? 1400 : 1100);
            if (!source.isEmpty()) {
                if (QRadioButton* b = sourceButton(&page, source)) {
                    b->click();
                }
            }
            if (unfold) {
                page.pcMicCard()->setDetailsExpanded(true);
            }
            page.show();
            QApplication::processEvents();
            saveCapture(&page, stem);
        };
        {
            RadioModel model;
            model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
            shootMic(model, QStringLiteral("microphone-g2-pc-folded"), false, QString());
            shootMic(model, QStringLiteral("microphone-g2-pc-unfolded"), true, QString());
            shootMic(model, QStringLiteral("microphone-g2-radio-picked"), false,
                     QStringLiteral("Radio Mic"));
            model.transmitModel().setMicSource(MicSource::Pc);
        }
        {
            RadioModel model;
            model.setHpsdrModelForTest(HPSDRModel::HERMESLITE);
            shootMic(model, QStringLiteral("microphone-hl2-addon"), false, QString());
        }
        {
            RadioModel model;
            shootMic(model, QStringLiteral("microphone-no-radio"), false, QString());
        }
        {
            RadioModel remote(RadioModel::Role::Remote);
            SpeakerLink link;
            remote.attachStation(&link);
            const auto detach = qScopeGuard([&remote]() { remote.attachStation(nullptr); });
            remote.setStationConnectionState(ConnectionState::Connected);
            shootMic(remote, QStringLiteral("microphone-remote-folded"), false, QString());
            shootMic(remote, QStringLiteral("microphone-remote-unfolded"), true, QString());
        }
        // 27. Digital modes and Advanced, under the same style.
        auto shootDigital = [](RadioModel& model, const QString& stem,
                               const QVector<DetectedCable>* cables,
                               RemoteReceiverAudioNote note) {
            AudioDigitalModesPage page(&model);
            if (cables != nullptr) {
                page.vaxSection()->setDetectedCablesForTest(*cables);
            }
            page.setReceiverAudioNote(note);
            page.resize(760, 2300);
            page.show();
            QApplication::processEvents();
            saveCapture(&page, stem);
        };
        {
            RadioModel model;
            const QVector<DetectedCable> none;
            shootDigital(model, QStringLiteral("digital-modes-mac"), &none,
                         RemoteReceiverAudioNote::None);
        }
        {
            const auto restore = vaxSystem(SoundSystemLine::System::Mac);
            RadioModel model;
            AudioDigitalModesPage page(&model);
            VaxChannelCard* card = page.vaxSection()->channelCard(1);
            child<QCheckBox>(card, "vaxEnable")->setChecked(true);
            card->setBusOpen(false);
            page.vaxSection()->setDetectedCablesForTest({});
            page.resize(760, 2300);
            page.show();
            QApplication::processEvents();
            QCOMPARE(card->statusLineText(),
                     QStringLiteral("Not available until the VAX driver is allowed (see above)."));
            saveCapture(&page, QStringLiteral("digital-modes-mac-driver-failed"));
            child<QCheckBox>(card, "vaxEnable")->setChecked(false);
        }
        {
            const auto restore = vaxSystem(SoundSystemLine::System::Windows);
            RadioModel model;
            const QVector<DetectedCable> none;
            shootDigital(model, QStringLiteral("digital-modes-windows-no-cables"), &none,
                         RemoteReceiverAudioNote::None);
            const QVector<DetectedCable> two{outputCable(QStringLiteral("CABLE Input")),
                                             outputCable(QStringLiteral("CABLE-A Input"))};
            shootDigital(model, QStringLiteral("digital-modes-windows-cables"), &two,
                         RemoteReceiverAudioNote::None);
        }
        {
            const auto restore = vaxSystem(SoundSystemLine::System::Linux);
            RadioModel model;
            const QVector<DetectedCable> none;
            shootDigital(model, QStringLiteral("digital-modes-linux"), &none,
                         RemoteReceiverAudioNote::None);
        }
        {
            RadioModel remote(RadioModel::Role::Remote);
            const QVector<DetectedCable> none;
            shootDigital(remote, QStringLiteral("digital-modes-remote-opus"), &none,
                         RemoteReceiverAudioNote::OpusChosen);
        }
        {
            RadioModel model;
            AudioAdvancedPage page(&model);
            page.resize(760, 700);
            page.show();
            QApplication::processEvents();
            saveCapture(&page, QStringLiteral("advanced"));
        }

        // Native audio plan Task 16 (V-UI-1): Outputs and Microphone on a
        // fake catalogue for each system. Outputs: the speakers' device is
        // missing (playing on the default), the headphones' device is held
        // by another program; both cards unfolded with the Delay line.
        // Microphone: a Bluetooth headset's mic picked, and a missing mic.
        const std::array<std::pair<FakeOs, QString>, 4> systems{{
            {FakeOs::Mac, QStringLiteral("mac")},
            {FakeOs::Windows, QStringLiteral("windows")},
            {FakeOs::LinuxPipeWire, QStringLiteral("linux-pipewire")},
            {FakeOs::LinuxPulseAudio, QStringLiteral("linux-pulseaudio")},
        }};
        for (const auto& [os, name] : systems) {
            const AudioEngineKind engine = os == FakeOs::Mac       ? AudioEngineKind::CoreAudio
                : os == FakeOs::Windows                            ? AudioEngineKind::WindowsShared
                : os == FakeOs::LinuxPipeWire                      ? AudioEngineKind::PipeWire
                                                                   : AudioEngineKind::PulseAudio;
            auto choice = [engine](const QString& id, const QString& device) {
                AudioDeviceConfig cfg;
                cfg.engine = engine;
                cfg.deviceId = id;
                cfg.deviceName = device;
                return cfg;
            };
            const QString headset = os == FakeOs::Mac ? QStringLiteral("AirPods")
                                                      : QStringLiteral("Headset");
            {
                clearAudioKeys();
                choice(QStringLiteral("gone-uid"), QStringLiteral("Desk monitor"))
                    .saveToSettings(QStringLiteral("audio/Speakers"));
                choice(QStringLiteral("desk-uid"), QStringLiteral("Desk speakers"))
                    .saveToSettings(QStringLiteral("audio/Headphones"));
                AppSettings::instance().setValue(QStringLiteral("audio/Headphones/Enabled"),
                                                 QStringLiteral("True"));
                RadioModel model;
                const FakeSystem fake = fakeSystem(os);
                startOnFakeSystem(model, fake);
                AudioEngine* audio = model.localAudioDevices();
                QCOMPARE(fake.native->outputRequests().back().deviceId, QStringLiteral("desk-uid"));
                FakeMatcherAudioBus* headphonesBus = fake.native->lastOutput();
                QVERIFY(headphonesBus != nullptr);
                AudioOutputsPage page(&model);
                page.resize(760, 960);
                for (DeviceCard* card : page.findChildren<DeviceCard*>()) {
                    card->setDetailsExpanded(true);
                }
                AudioStreamEvent busy;
                busy.kind = AudioStreamEvent::Kind::DeviceBusy;
                headphonesBus->emitEventForTest(busy);
                auto* speakers = child<DeviceCard>(&page, "thisComputerGroup");
                auto* headphones = child<DeviceCard>(&page, "headphonesGroup");
                QTRY_COMPARE_WITH_TIMEOUT(
                    child<QLabel>(speakers, "deviceStateNote")->text(),
                    QStringLiteral("Desk monitor is not connected. Playing on the system default, "
                                   "Built-in speakers, until it comes back."),
                    5000);
                QTRY_VERIFY_WITH_TIMEOUT(child<QLabel>(headphones, "deviceStateNote")
                                             ->text()
                                             .contains(QStringLiteral("is in use by another program")),
                                         5000);
                QTRY_VERIFY_WITH_TIMEOUT(child<QLabel>(speakers, "deviceDelayNow")
                                             ->text()
                                             .endsWith(QStringLiteral(" ms from the radio to Built-in speakers")),
                                         2000);
                page.show();
                QApplication::processEvents();
                saveCapture(&page, QStringLiteral("outputs-%1-missing-in-use-unfolded").arg(name));
                audio->stop();
            }
            {
                clearAudioKeys();
                choice(QStringLiteral("headset-uid"), headset)
                    .saveToSettings(QStringLiteral("audio/TxInput"));
                RadioModel model;
                const FakeSystem fake = fakeSystem(os);
                startOnFakeSystem(model, fake);
                AudioTxInputPage page(&model);
                page.resize(760, 720);
                page.pcMicCard()->setDetailsExpanded(true);
                QCOMPARE(child<QLabel>(page.pcMicCard(), "deviceStateNote")->text(),
                         QStringLiteral("Bluetooth headsets switch to phone-call quality, for "
                                        "listening too, while they are your mic. For the best "
                                        "sound, listen on %1 and talk on a wired or built-in "
                                        "mic.")
                             .arg(headset));
                page.show();
                QApplication::processEvents();
                saveCapture(&page, QStringLiteral("microphone-%1-bluetooth-unfolded").arg(name));
                model.localAudioDevices()->stop();
            }
            {
                clearAudioKeys();
                choice(QStringLiteral("gone-mic-uid"), QStringLiteral("Desk mic"))
                    .saveToSettings(QStringLiteral("audio/TxInput"));
                RadioModel model;
                const FakeSystem fake = fakeSystem(os);
                startOnFakeSystem(model, fake);
                AudioTxInputPage page(&model);
                page.resize(760, 720);
                page.pcMicCard()->setDetailsExpanded(true);
                QTRY_COMPARE_WITH_TIMEOUT(
                    child<QLabel>(page.pcMicCard(), "deviceStateNote")->text(),
                    QStringLiteral("Desk mic is not connected. The mic stays silent until it "
                                   "comes back; NereusSDR never switches to another mic on its "
                                   "own."),
                    5000);
                QTRY_COMPARE_WITH_TIMEOUT(child<QLabel>(&page, "captureStatus")->text(),
                                          QStringLiteral("PC mic not connected"), 5000);
                page.show();
                QApplication::processEvents();
                saveCapture(&page, QStringLiteral("microphone-%1-missing-unfolded").arg(name));
                model.localAudioDevices()->stop();
            }
        }
        captureOpenLists();
        clearAudioKeys();
    }

private:
    // Task 16 fix round: each open list's view rendered on its own (an
    // offscreen popup is a separate window), a reason tooltip as Qt draws
    // it, the Sound system line and the engine notes. The engines here are
    // never started, so the lists come from the catalogue before a radio
    // connects and no device opens. (The mic's delay readout and in-use
    // captures need the fake capture helper: tst_audio_tx_input_pc_mic_group.)
    static void shootList(QComboBox* combo, const QString& stem)
    {
        QVERIFY2(combo != nullptr, qPrintable(stem));
        combo->showPopup();
        QApplication::processEvents();
        QVERIFY2(combo->view()->isVisible(), qPrintable(stem));
        saveCapture(combo->view(), stem);
        combo->hidePopup();
        QApplication::processEvents();
    }

    static void shootTip(QWidget* anchor, const QString& text, const QString& stem)
    {
        QVERIFY2(!text.isEmpty(), qPrintable(stem));
        QToolTip::showText(anchor->mapToGlobal(QPoint(8, 8)), text, anchor);
        QApplication::processEvents();
        QWidget* tip = nullptr;
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (w->inherits("QTipLabel") && w->isVisible()) {
                tip = w;
            }
        }
        QVERIFY2(tip != nullptr, qPrintable(stem));
        saveCapture(tip, stem);
        QToolTip::hideText();
        QApplication::processEvents();
    }

    static std::unique_ptr<AudioEngine> engineOn(
        std::vector<std::shared_ptr<IAudioEngineBackend>> backends)
    {
        auto engine = std::make_unique<AudioEngine>();
        engine->setVaxOutputsAllowed(false);
        engine->setAudioBackendsForTest(std::move(backends));
        return engine;
    }

    static std::unique_ptr<DeviceCard> cardOn(AudioEngine* engine, const QString& prefix,
                                              DeviceCard::Role role, bool unfold)
    {
        auto card = std::make_unique<DeviceCard>(prefix, role, false);
        card->setAudioEngine(engine);
        card->setDetailsExpanded(unfold);
        card->resize(700, card->sizeHint().height());
        card->show();
        QApplication::processEvents();
        return card;
    }

    static QComboBox* deviceCombo(DeviceCard* card)
    {
        for (QComboBox* combo : card->findChildren<QComboBox*>()) {
            if (combo->count() > 0 && combo->itemText(0) == QStringLiteral("(platform default)")) {
                return combo;
            }
        }
        return nullptr;
    }

    static void shootLine(AudioEngine* engine, const QString& expected, const QString& stem)
    {
        SoundSystemLine line(engine);
        line.refresh();
        QCOMPARE(line.text(), expected);
        line.resize(700, line.sizeHint().height());
        line.QWidget::show();
        QApplication::processEvents();
        saveCapture(&line, stem);
    }

    static AudioDeviceConfig savedAs(AudioEngineKind engine, const QString& id,
                                     const QString& name, const QString& driverApi = {})
    {
        AudioDeviceConfig cfg;
        cfg.engine = engine;
        cfg.deviceId = id;
        cfg.deviceName = name;
        cfg.driverApi = driverApi;
        return cfg;
    }

    static int opens(const std::shared_ptr<FakeAudioEngineBackend>& backend)
    {
        return backend ? int(backend->outputRequests().size()) : 0;
    }

    void captureOpenLists()
    {
        const QString speakers = QStringLiteral("audio/Speakers");
        // The Mac: Core Audio alone, greyed, with its reason.
        {
            clearAudioKeys();
            const FakeSystem fake = fakeSystem(FakeOs::Mac);
            auto engine = engineOn({fake.native});
            auto card = cardOn(engine.get(), speakers, DeviceCard::Role::Output, false);
            QComboBox* driver = child<QComboBox>(card.get(), "deviceDriverCombo");
            QCOMPARE(driver->count(), 1);
            QCOMPARE(driver->itemText(0), QStringLiteral("Core Audio"));
            shootList(driver, QStringLiteral("list-driver-mac-open"));
            shootTip(driver, driver->toolTip(), QStringLiteral("list-driver-mac-reason"));
            QCOMPARE(opens(fake.native), 0);
        }
        // Windows: in order, ASIO installed, the "Older drivers" heading;
        // the Sound system line with ASIO and with an older driver; the
        // exclusive and older-driver engine notes.
        {
            const FakeSystem fake = fakeSystem(FakeOs::Windows);
            auto asio = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::Asio);
            asio->setDevices({fakeDevice(AudioBackendId::Asio, AudioDeviceDirection::Output,
                                         QStringLiteral("focusrite-asio"),
                                         QStringLiteral("Focusrite USB ASIO"))});
            const std::vector<std::shared_ptr<IAudioEngineBackend>> backends{fake.native, asio,
                                                                             fake.older};
            {
                clearAudioKeys();
                auto engine = engineOn(backends);
                auto card = cardOn(engine.get(), speakers, DeviceCard::Role::Output, false);
                QComboBox* driver = child<QComboBox>(card.get(), "deviceDriverCombo");
                const QStringList expected{
                    QStringLiteral("Windows audio, shared"), QStringLiteral("Windows audio, exclusive"),
                    QStringLiteral("ASIO"), QStringLiteral("Older drivers")};
                for (int i = 0; i < expected.size(); ++i) {
                    QCOMPARE(driver->itemText(i), expected.at(i));
                }
                shootList(driver, QStringLiteral("list-driver-windows-open"));
            }
            {
                clearAudioKeys();
                savedAs(AudioEngineKind::Asio, QStringLiteral("focusrite-asio"),
                        QStringLiteral("Focusrite USB ASIO"))
                    .saveToSettings(speakers);
                auto engine = engineOn(backends);
                shootLine(engine.get(),
                          QStringLiteral("Windows audio (WASAPI) and ASIO (Focusrite USB ASIO)"),
                          QStringLiteral("line-sound-system-windows-asio"));
            }
            {
                clearAudioKeys();
                savedAs(AudioEngineKind::PortAudio,
                        portAudioDeviceId(QStringLiteral("MME"), QStringLiteral("Speakers")),
                        QStringLiteral("Speakers"), QStringLiteral("MME"))
                    .saveToSettings(speakers);
                auto engine = engineOn(backends);
                shootLine(engine.get(),
                          QStringLiteral("Windows audio (WASAPI). Older drivers in use: MME."),
                          QStringLiteral("line-sound-system-windows-older"));
                auto card = cardOn(engine.get(), speakers, DeviceCard::Role::Output, true);
                QCOMPARE(child<QLabel>(card.get(), "engineNote")->text(),
                         QStringLiteral("An older driver: more delay, and its list updates only "
                                        "with Rescan devices."));
                saveCapture(card.get(), QStringLiteral("card-windows-older-driver-note-unfolded"));
            }
            {
                clearAudioKeys();
                savedAs(AudioEngineKind::WindowsExclusive, QStringLiteral("desk-uid"),
                        QStringLiteral("Desk speakers"))
                    .saveToSettings(speakers);
                auto engine = engineOn(backends);
                auto card = cardOn(engine.get(), speakers, DeviceCard::Role::Output, true);
                QCOMPARE(child<QLabel>(card.get(), "engineNote")->text(),
                         QStringLiteral("Other apps cannot play through this device while "
                                        "NereusSDR has it."));
                saveCapture(card.get(), QStringLiteral("card-windows-exclusive-note-unfolded"));
            }
            QCOMPARE(opens(fake.native) + opens(asio) + opens(fake.older), 0);
        }
        // Linux: PipeWire; PulseAudio with "PipeWire (not running)"; neither.
        {
            clearAudioKeys();
            const FakeSystem fake = fakeSystem(FakeOs::LinuxPipeWire);
            auto engine = engineOn({fake.native, fake.older});
            auto card = cardOn(engine.get(), speakers, DeviceCard::Role::Output, false);
            QComboBox* driver = child<QComboBox>(card.get(), "deviceDriverCombo");
            QCOMPARE(driver->itemText(0), QStringLiteral("PipeWire"));
            shootList(driver, QStringLiteral("list-driver-linux-pipewire-open"));
            QCOMPARE(opens(fake.native) + opens(fake.older), 0);
        }
        {
            clearAudioKeys();
            const FakeSystem fake = fakeSystem(FakeOs::LinuxPulseAudio);
            auto engine = engineOn({fake.stoppedPipeWire, fake.native, fake.older});
            auto card = cardOn(engine.get(), speakers, DeviceCard::Role::Output, false);
            QComboBox* driver = child<QComboBox>(card.get(), "deviceDriverCombo");
            QCOMPARE(driver->itemText(0), QStringLiteral("PipeWire (not running)"));
            QCOMPARE(driver->itemText(1), QStringLiteral("PulseAudio"));
            shootList(driver, QStringLiteral("list-driver-linux-pulseaudio-open"));
            shootTip(driver, driver->itemData(0, Qt::ToolTipRole).toString(),
                     QStringLiteral("list-driver-linux-pulseaudio-pipewire-reason"));
            QCOMPARE(opens(fake.native) + opens(fake.older), 0);
        }
        {
            clearAudioKeys();
            const FakeSystem fake = fakeSystem(FakeOs::LinuxPulseAudio);
            fake.native->setRunning(false);
            auto engine = engineOn({fake.stoppedPipeWire, fake.native, fake.older});
            auto card = cardOn(engine.get(), speakers, DeviceCard::Role::Output, false);
            QComboBox* driver = child<QComboBox>(card.get(), "deviceDriverCombo");
            QCOMPARE(driver->itemText(0), QStringLiteral("PipeWire (not running)"));
            QCOMPARE(driver->itemText(1), QStringLiteral("PulseAudio (not running)"));
            shootList(driver, QStringLiteral("list-driver-linux-neither-open"));
            shootLine(engine.get(),
                      QStringLiteral("None found. Start PipeWire or PulseAudio, then click Rescan "
                                     "devices."),
                      QStringLiteral("line-sound-system-linux-neither"));
            QCOMPARE(opens(fake.native) + opens(fake.older), 0);
        }
        // The Core: "ALSA, direct" alone, greyed, with its reason.
        {
            clearAudioKeys();
            auto alsa = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::AlsaDirect);
            alsa->setDevices({fakeDevice(AudioBackendId::AlsaDirect, AudioDeviceDirection::Output,
                                         QStringLiteral("hw:1,0"), QStringLiteral("USB Audio")),
                              fakeDevice(AudioBackendId::AlsaDirect, AudioDeviceDirection::Output,
                                         QStringLiteral("hw:0,0"), QStringLiteral("Built-in Audio"))});
            alsa->setDefault(AudioDeviceDirection::Output, QStringLiteral("hw:0,0"));
            auto engine = engineOn({alsa});
            auto card = cardOn(engine.get(), speakers, DeviceCard::Role::Output, false);
            QComboBox* driver = child<QComboBox>(card.get(), "deviceDriverCombo");
            QCOMPARE(driver->count(), 1);
            QCOMPARE(driver->itemText(0), QStringLiteral("ALSA, direct"));
            shootList(driver, QStringLiteral("list-driver-core-open"));
            shootTip(driver, driver->toolTip(), QStringLiteral("list-driver-core-reason"));
            QCOMPARE(opens(alsa), 0);
        }
        // A Device list: "(platform default)" first, a saved "(none)", and
        // a Bluetooth headset's mic.
        {
            clearAudioKeys();
            savedAs(AudioEngineKind::CoreAudio, QString::fromLatin1(kAudioDeviceNone),
                    QString::fromLatin1(kAudioDeviceNone))
                .saveToSettings(QStringLiteral("audio/TxInput"));
            const FakeSystem fake = fakeSystem(FakeOs::Mac);
            auto engine = engineOn({fake.native});
            auto card = cardOn(engine.get(), QStringLiteral("audio/TxInput"),
                               DeviceCard::Role::Input, false);
            QComboBox* device = deviceCombo(card.get());
            QVERIFY(device != nullptr);
            QCOMPARE(device->itemText(1), QString::fromLatin1(kAudioDeviceNone));
            QVERIFY(device->findText(QStringLiteral("AirPods")) > 1);
            shootList(device, QStringLiteral("list-device-mac-mic-open"));
            QCOMPARE(opens(fake.native), 0);
        }
    }
};

QTEST_MAIN(TstAudioSetupRegroup)
#include "tst_audio_setup_regroup.moc"
