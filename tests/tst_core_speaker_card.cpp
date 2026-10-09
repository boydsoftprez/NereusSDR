// no-port-check: test-only, NereusSDR-original. No upstream logic is
// ported here.
//
// =================================================================
// tests/tst_core_speaker_card.cpp  (NereusSDR)
// =================================================================
//
// Setup > Audio > Outputs' Core speaker card in a window connected to a
// Core (native audio plan Task 22: R-AUD-27, R-AUD-30, D24, settled calls 12
// and 35, V-UI-4).
//
//   1. Texts: the title, "A speaker or sound card plugged into <Core>.",
//      Volume and "Mute Core speaker", the Device list, the note, and Device
//      details folded with the Core's driver line, sample rate, channels,
//      buffer size, delay with its "Now" line and the negotiated format.
//   2. Unreachable: every control greyed with "Connect to the Core to change
//      these."; an older Core: greyed with its reason; the page's station
//      gate greys it too.
//   3. No card at the Core; the chosen card not connected or in use, with
//      and without another card.
//   4. A desktop box: "(none)" first and selected with R-AUD-30's first
//      sentence; after a pick, the second sentence with the card's name.
//   5. A card plugged in at the Core appears in the open list.
//   6. Writes reach the model; the Core's changes move the controls without
//      echo.
//   7. With NEREUS_AUDIO_SETUP_CAPTURE_DIR set, captures of each state.
//
// Modification history (NereusSDR):
//   2026-10-09 - Written for the native audio plan Task 22. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSlider>
#include <QStandardItemModel>
#include <QStyleFactory>
#include <QTextDocumentFragment>
#include <QToolButton>

#include "core/AppSettings.h"
#include "core/audio/CoreSpeakerJson.h"
#include "core/session/IStationLink.h"
#include "core/session/MirrorSchema.h"
#include "core/session/RemoteDevicesState.h"
#include "gui/setup/AudioOutputsPage.h"
#include "gui/setup/CoreSpeakerCard.h"
#include "gui/styles/AppTheme.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

const QString kCoreName = QStringLiteral("NereusCore-SkyHQ");
const QString kUsbId = QStringLiteral("USB,0");
const QString kUsb = QStringLiteral("USB Audio Device");
const QString kJackId = QStringLiteral("Headphones,0");
const QString kJack = QStringLiteral("bcm2835 Headphones");
const QString kHdmiId = QStringLiteral("vc4hdmi0,0");
const QString kHdmi = QStringLiteral("vc4-hdmi-0");

const QString kUnreachable = QStringLiteral("Connect to the Core to change these.");
const QString kOlderCore =
    QStringLiteral("This Core can't set its speaker from here. Update the Core.");
const QString kNote = QStringLiteral(
    "This is the speaker at the Core, for listening where the Core sits. Changes here reach "
    "every window and the phone. Each slice's AF level and mute still apply.");
const QString kDriverNote =
    QStringLiteral("The Core runs without a desktop, so it plays straight to the sound card.");
const QString kNoCard = QStringLiteral(
    "No sound card is plugged into the Core. Plug in a USB sound card or speaker and it shows "
    "up here by itself.");
const QString kDesktopPick = QStringLiteral(
    "The Core's computer runs a desktop, which uses its sound cards. Pick one here to play the "
    "Core speaker on it.");

// A Core link. `offers`: the Core offers the Core speaker (coreSpeakerVersion
// 1); `older`: the link knows the Core is older.
class CoreLink : public IStationLink {
public:
    bool offers{true};
    bool older{false};
    bool coreSpeakerAvailable() const override { return offers; }
    bool coreSpeakerNeedsNewerCore() const override { return older; }
    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
};

// A remote window of a Core called kCoreName.
struct Window {
    RadioModel model{RadioModel::Role::Remote};
    CoreLink link;
    RemoteDevicesState devices;

    Window()
    {
        devices.applyObject("devices",
                            {{0, "stationLabel", MirrorWireKind::Utf8, kCoreName}});
        model.setStationDevices(&devices);
        model.attachStation(&link);
        model.setStationConnectionState(ConnectionState::Connected);
        model.reportStationLinkStateChanged();
    }
    ~Window()
    {
        model.attachStation(nullptr);
        model.setStationDevices(nullptr);
    }

    void cards(const QList<CoreSpeakerCardInfo>& list)
    {
        QVERIFY(model.applyStationCoreSpeakerValue("coreSpeakerDevices",
                                                   coreSpeakerDevicesToJson(list)));
    }
    void state(CoreSpeakerStateKind kind, const QString& playing, const QString& chosen,
               bool desktop = false)
    {
        QVERIFY(model.applyStationCoreSpeakerValue(
            "coreSpeakerState",
            coreSpeakerStateToJson(CoreSpeakerState{kind, playing, chosen, desktop})));
    }
    void device(const QString& id, const QString& name)
    {
        model.setCoreSpeakerDevice(coreSpeakerDeviceToJson(id, name));
    }
    // The Core playing on its USB speaker, with two other cards.
    void playingUsb()
    {
        cards({{kJackId, kJack, AudioDeviceState::Present},
               {kUsbId, kUsb, AudioDeviceState::Present}});
        device(kUsbId, kUsb);
        state(CoreSpeakerStateKind::Playing, kUsb, kUsb);
        model.setCoreSpeakerVolume(40);
        CoreSpeakerDetails details;
        details.bufferFrames = 256;
        details.delayMs = 0;
        details.sampleRate = 48000;
        details.negotiated = QStringLiteral("48000 Hz · 2 ch · 256 samples");
        details.delayNowMs = 12.5;
        model.setCoreSpeakerDetails(coreSpeakerDetailsToJson(details));
    }
};

template <typename T>
T* child(QWidget* w, const char* name)
{
    T* found = w->findChild<T*>(QLatin1String(name));
    if (found == nullptr) {
        qWarning("no child %s", name);
    }
    return found;
}

QStringList items(QComboBox* combo)
{
    QStringList out;
    for (int i = 0; i < combo->count(); ++i) {
        out << combo->itemText(i);
    }
    return out;
}

QString plain(const QLabel* label)
{
    return QTextDocumentFragment::fromHtml(label->text()).toPlainText();
}

// The controls a reachable Core lets the window change.
QList<QWidget*> controlsOf(QWidget* card)
{
    QList<QWidget*> out;
    for (const char* name : {"coreSpeakerVolume", "coreSpeakerVolumeReadout", "coreSpeakerMute",
                             "coreSpeakerDevice", "coreSpeakerSampleRate", "coreSpeakerChannels",
                             "coreSpeakerBufferSize", "coreSpeakerDelay"}) {
        out << child<QWidget>(card, name);
    }
    return out;
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

class TstCoreSpeakerCard : public QObject {
    Q_OBJECT

private slots:
    // 1. The card's texts and Device details.
    void texts()
    {
        Window w;
        w.playingUsb();
        CoreSpeakerCard card(&w.model, nullptr);
        QCOMPARE(card.objectName(), QStringLiteral("coreSpeakerGroup"));
        QCOMPARE(card.title(), QStringLiteral("Core speaker"));
        QCOMPARE(plain(child<QLabel>(&card, "coreSpeakerWhere")),
                 QStringLiteral("A speaker or sound card plugged into NereusCore-SkyHQ."));
        QCOMPARE(child<QCheckBox>(&card, "coreSpeakerMute")->text(),
                 QStringLiteral("Mute Core speaker"));
        QCOMPARE(child<QSlider>(&card, "coreSpeakerVolume")->value(), 40);
        QCOMPARE(child<QLabel>(&card, "coreSpeakerVolumeReadout")->text(), QStringLiteral("40"));
        auto* device = child<QComboBox>(&card, "coreSpeakerDevice");
        QCOMPARE(items(device),
                 (QStringList{QStringLiteral("(the Core's default)"), kJack, kUsb}));
        QCOMPARE(device->currentText(), kUsb);
        QCOMPARE(child<QLabel>(&card, "coreSpeakerNote")->text(), kNote);
        QVERIFY(child<QLabel>(&card, "coreSpeakerStateNote")->isHidden());
        QVERIFY(child<QLabel>(&card, "coreSpeakerReason")->isHidden());
        QVERIFY(child<QLabel>(&card, "coreSpeakerDesktopNote")->isHidden());
        for (QWidget* control : controlsOf(&card)) {
            QVERIFY2(control->isEnabled(), qPrintable(control->objectName()));
        }

        // Device details, folded.
        auto* details = child<QWidget>(&card, "coreSpeakerDetails");
        QVERIFY(!card.detailsExpanded());
        QVERIFY(details->isHidden());
        card.setDetailsExpanded(true);
        QVERIFY(!details->isHidden());
        auto* driver = child<QComboBox>(&card, "coreSpeakerDriver");
        QCOMPARE(items(driver), QStringList{QStringLiteral("ALSA, direct")});
        QVERIFY(!driver->isEnabled());
        QCOMPARE(driver->toolTip(), kDriverNote);
        QCOMPARE(child<QLabel>(&card, "coreSpeakerDriverNote")->text(), kDriverNote);
        auto* rate = child<QComboBox>(&card, "coreSpeakerSampleRate");
        QCOMPARE(rate->currentText(), QStringLiteral("48000 Hz"));
        QCOMPARE(items(child<QComboBox>(&card, "coreSpeakerChannels")),
                 QStringList{QStringLiteral("2 (Stereo)")});
        auto* buffer = child<QComboBox>(&card, "coreSpeakerBufferSize");
        QCOMPARE(buffer->currentText(), QStringLiteral("256 samples"));
        QCOMPARE(child<QLabel>(&card, "coreSpeakerBufferMs")->text(), QStringLiteral("5.3 ms"));
        auto* delay = child<QComboBox>(&card, "coreSpeakerDelay");
        QCOMPARE(items(delay),
                 (QStringList{QStringLiteral("Automatic"), QStringLiteral("2 ms"),
                              QStringLiteral("3 ms"), QStringLiteral("5 ms"),
                              QStringLiteral("10 ms"), QStringLiteral("20 ms"),
                              QStringLiteral("40 ms")}));
        QCOMPARE(delay->currentText(), QStringLiteral("Automatic"));
        QCOMPARE(child<QLabel>(&card, "coreSpeakerDelayNow")->text(),
                 QStringLiteral("Now 13 ms from the radio to USB Audio Device"));
        QCOMPARE(child<QLabel>(&card, "coreSpeakerNegotiated")->text(),
                 QStringLiteral("48000 Hz · 2 ch · 256 samples"));
    }

    // The Core's name follows its devices object; unnamed, "the Core".
    void whereFollowsTheCoresName()
    {
        Window w;
        CoreSpeakerCard card(&w.model, nullptr);
        auto* where = child<QLabel>(&card, "coreSpeakerWhere");
        w.devices.applyObject("devices",
                              {{0, "stationLabel", MirrorWireKind::Utf8, QStringLiteral("Shack Pi")}});
        QCOMPARE(plain(where), QStringLiteral("A speaker or sound card plugged into Shack Pi."));
        w.devices.applyObject("devices", {{0, "stationLabel", MirrorWireKind::Utf8, QString()}});
        QCOMPARE(plain(where), QStringLiteral("A speaker or sound card plugged into the Core."));
    }

    // 2. Greyed with the reason: unreachable, an older Core, the page's gate.
    void greyedWithTheReason_data()
    {
        QTest::addColumn<bool>("offers");
        QTest::addColumn<bool>("older");
        QTest::addColumn<bool>("gated");
        QTest::addColumn<QString>("reason");
        QTest::newRow("unreachable") << false << false << false << kUnreachable;
        QTest::newRow("older-core") << false << true << false << kOlderCore;
        QTest::newRow("station-gate") << true << false << true << kUnreachable;
    }
    void greyedWithTheReason()
    {
        QFETCH(bool, offers);
        QFETCH(bool, older);
        QFETCH(bool, gated);
        QFETCH(QString, reason);
        Window w;
        w.playingUsb();
        w.link.offers = offers;
        w.link.older = older;
        w.model.reportStationLinkStateChanged();
        // On the page: greyed, never hidden.
        AudioOutputsPage page(&w.model);
        auto* found = page.findChild<CoreSpeakerCard*>();
        QVERIFY(found != nullptr);
        CoreSpeakerCard& card = *found;
        if (gated) {
            page.setStationSettingsAvailable(false, kUnreachable);
        }
        QVERIFY(!card.isHidden());
        for (QWidget* control : controlsOf(&card)) {
            QVERIFY2(!control->isEnabled(), qPrintable(control->objectName()));
            QCOMPARE(control->toolTip(), reason);
        }
        auto* why = child<QLabel>(&card, "coreSpeakerReason");
        QVERIFY(!why->isHidden());
        QCOMPARE(why->text(), reason);
        QCOMPARE(child<QLabel>(&card, "coreSpeakerNote")->text(), kNote);
        QVERIFY(child<QLabel>(&card, "coreSpeakerStateNote")->isHidden());

        // The link coming back lifts it (no signal of the card's own).
        w.link.offers = true;
        w.link.older = false;
        w.model.reportStationLinkStateChanged();
        page.setStationSettingsAvailable(true, QString());
        for (QWidget* control : controlsOf(&card)) {
            QVERIFY2(control->isEnabled(), qPrintable(control->objectName()));
            QVERIFY(control->toolTip().isEmpty());
        }
        QVERIFY(why->isHidden());
    }

    // 3. No card at the Core.
    void noCardAtTheCore()
    {
        Window w;
        w.cards({});
        w.device(QString(), QString());
        w.state(CoreSpeakerStateKind::NoCard, QString(), QString());
        AudioOutputsPage page(&w.model);
        auto* found = page.findChild<CoreSpeakerCard*>();
        QVERIFY(found != nullptr);
        CoreSpeakerCard& card = *found;
        QVERIFY(!card.isHidden());
        auto* note = child<QLabel>(&card, "coreSpeakerStateNote");
        QVERIFY(!note->isHidden());
        QCOMPARE(note->text(), kNoCard);
        auto* device = child<QComboBox>(&card, "coreSpeakerDevice");
        QCOMPARE(items(device), QStringList{QStringLiteral("(the Core's default)")});
        QVERIFY(device->isEnabled());
    }

    // 3. The chosen card not connected or in use (settled call 35).
    void chosenCardMissing_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addColumn<QString>("playing");
        QTest::addColumn<QString>("listed");
        QTest::addColumn<QString>("note");
        QTest::newRow("not-connected")
            << int(CoreSpeakerStateKind::NotConnected) << kJack
            << QStringLiteral("USB Audio Device (not connected)")
            << QStringLiteral("USB Audio Device is not connected at the Core. Playing on the "
                              "Core's default, bcm2835 Headphones, until it comes back.");
        QTest::newRow("not-connected-no-other")
            << int(CoreSpeakerStateKind::NotConnected) << QString()
            << QStringLiteral("USB Audio Device (not connected)")
            << QStringLiteral("USB Audio Device is not connected at the Core, and the Core has no "
                              "other sound card, so it is silent until it comes back.");
        QTest::newRow("in-use")
            << int(CoreSpeakerStateKind::InUse) << kJack
            << QStringLiteral("USB Audio Device (in use by another program)")
            << QStringLiteral("USB Audio Device is in use by another program at the Core. Playing "
                              "on the Core's default, bcm2835 Headphones, until it comes back.");
        QTest::newRow("in-use-no-other")
            << int(CoreSpeakerStateKind::InUse) << QString()
            << QStringLiteral("USB Audio Device (in use by another program)")
            << QStringLiteral("USB Audio Device is in use by another program at the Core, and the "
                              "Core has no other sound card, so it is silent until it comes back.");
    }
    void chosenCardMissing()
    {
        QFETCH(int, kind);
        QFETCH(QString, playing);
        QFETCH(QString, listed);
        QFETCH(QString, note);
        const auto k = static_cast<CoreSpeakerStateKind>(kind);
        Window w;
        QList<CoreSpeakerCardInfo> list;
        if (!playing.isEmpty()) {
            list.append({kJackId, kJack, AudioDeviceState::Present});
        }
        list.append({kUsbId, kUsb,
                     k == CoreSpeakerStateKind::InUse ? AudioDeviceState::InUse
                                                      : AudioDeviceState::NotConnected});
        w.cards(list);
        w.device(kUsbId, kUsb);
        w.state(k, playing, kUsb);
        CoreSpeakerCard card(&w.model, nullptr);
        auto* stateNote = child<QLabel>(&card, "coreSpeakerStateNote");
        QVERIFY(!stateNote->isHidden());
        QCOMPARE(stateNote->text(), note);
        auto* device = child<QComboBox>(&card, "coreSpeakerDevice");
        QCOMPARE(device->currentText(), listed);
    }

    // 4. A Core box that starts into a desktop (R-AUD-30, settled call 12).
    void desktopBox()
    {
        Window w;
        w.cards({{kJackId, kJack, AudioDeviceState::Present},
                 {kHdmiId, kHdmi, AudioDeviceState::Present},
                 {kUsbId, kUsb, AudioDeviceState::Present}});
        w.device(QStringLiteral("(none)"), QString());
        w.state(CoreSpeakerStateKind::WaitingForPick, QString(), QString(), true);
        CoreSpeakerCard card(&w.model, nullptr);
        auto* device = child<QComboBox>(&card, "coreSpeakerDevice");
        QCOMPARE(items(device), (QStringList{QStringLiteral("(none)"),
                                             QStringLiteral("(the Core's default)"), kJack, kHdmi,
                                             kUsb}));
        QCOMPARE(device->currentIndex(), 0);
        auto* desktop = child<QLabel>(&card, "coreSpeakerDesktopNote");
        QVERIFY(!desktop->isHidden());
        QCOMPARE(desktop->text(), kDesktopPick);
        QVERIFY(child<QLabel>(&card, "coreSpeakerStateNote")->isHidden());

        // A pick from the window; the Core then plays it and reports so.
        device->setCurrentIndex(device->findText(kUsb));
        QVERIFY(coreSpeakerDeviceFromJson(w.model.coreSpeakerDevice())
                == std::optional(qMakePair(kUsbId, kUsb)));
        w.state(CoreSpeakerStateKind::Playing, kUsb, kUsb, true);
        QCOMPARE(desktop->text(),
                 QStringLiteral("The desktop can't play through USB Audio Device while the Core "
                                "has it."));
        // "(none)" stays in the list (settled call 12).
        QCOMPARE(device->itemText(0), QStringLiteral("(none)"));
        QCOMPARE(device->currentText(), kUsb);

        // "(none)" picked again: the first sentence, and nothing plays.
        device->setCurrentIndex(0);
        QVERIFY(coreSpeakerDeviceFromJson(w.model.coreSpeakerDevice())
                == std::optional(qMakePair(QStringLiteral("(none)"), QString())));
        w.state(CoreSpeakerStateKind::NoCard, QString(), QString(), true);
        QCOMPARE(desktop->text(), kDesktopPick);
        QVERIFY(child<QLabel>(&card, "coreSpeakerStateNote")->isHidden());
    }

    // A box without a desktop offers no "(none)".
    void noDesktopNoNone()
    {
        Window w;
        w.playingUsb();
        CoreSpeakerCard card(&w.model, nullptr);
        QVERIFY(!items(child<QComboBox>(&card, "coreSpeakerDevice"))
                     .contains(QStringLiteral("(none)")));
    }

    // 5. A card plugged in at the Core appears in the open list.
    void hotPlugAppearsInTheOpenList()
    {
        Window w;
        w.playingUsb();
        AudioOutputsPage page(&w.model);
        auto* card = page.findChild<CoreSpeakerCard*>();
        QVERIFY(card != nullptr);
        auto* device = child<QComboBox>(card, "coreSpeakerDevice");
        QCOMPARE(device->count(), 3);
        QSignalSpy picks(&w.model, &RadioModel::coreSpeakerDeviceChanged);
        w.cards({{kJackId, kJack, AudioDeviceState::Present},
                 {kHdmiId, kHdmi, AudioDeviceState::Present},
                 {kUsbId, kUsb, AudioDeviceState::Present}});
        QCOMPARE(items(device), (QStringList{QStringLiteral("(the Core's default)"), kJack, kHdmi,
                                             kUsb}));
        QCOMPARE(device->currentText(), kUsb);
        // Refilling the list picks nothing.
        QCOMPARE(picks.count(), 0);
        // Unplugged: the Core lists it as not connected.
        w.cards({{kJackId, kJack, AudioDeviceState::Present},
                 {kUsbId, kUsb, AudioDeviceState::NotConnected}});
        QCOMPARE(items(device), (QStringList{QStringLiteral("(the Core's default)"), kJack,
                                             QStringLiteral("USB Audio Device (not connected)")}));
        QCOMPARE(picks.count(), 0);
    }

    // 6. Writes reach the model; the Core's changes move the controls
    // without echo.
    void writesAndFollowsWithoutEcho()
    {
        Window w;
        w.playingUsb();
        CoreSpeakerCard card(&w.model, nullptr);
        auto* volume = child<QSlider>(&card, "coreSpeakerVolume");
        auto* readout = child<QLabel>(&card, "coreSpeakerVolumeReadout");
        auto* mute = child<QCheckBox>(&card, "coreSpeakerMute");
        auto* device = child<QComboBox>(&card, "coreSpeakerDevice");
        auto* buffer = child<QComboBox>(&card, "coreSpeakerBufferSize");
        auto* delay = child<QComboBox>(&card, "coreSpeakerDelay");
        auto* rate = child<QComboBox>(&card, "coreSpeakerSampleRate");

        volume->setValue(72);
        QCOMPARE(w.model.coreSpeakerVolume(), 72);
        QCOMPARE(readout->text(), QStringLiteral("72"));
        mute->setChecked(true);
        QVERIFY(w.model.coreSpeakerMuted());
        device->setCurrentIndex(0);
        QCOMPARE(w.model.coreSpeakerDevice(), coreSpeakerDeviceToJson(QString(), QString()));
        device->setCurrentIndex(device->findText(kJack));
        QCOMPARE(w.model.coreSpeakerDevice(), coreSpeakerDeviceToJson(kJackId, kJack));
        buffer->setCurrentIndex(buffer->findData(512));
        delay->setCurrentIndex(delay->findData(20));
        rate->setCurrentIndex(rate->findData(96000));
        const std::optional<CoreSpeakerDetails> asked =
            coreSpeakerDetailsFromJson(w.model.coreSpeakerDetails());
        QVERIFY(asked.has_value());
        QCOMPARE(asked->bufferFrames, 512);
        QCOMPARE(asked->delayMs, 20);
        QCOMPARE(asked->sampleRate, 96000);
        // The Core's own fields go back as it sent them.
        QCOMPARE(asked->negotiated, QStringLiteral("48000 Hz · 2 ch · 256 samples"));
        QCOMPARE(asked->delayNowMs, 12.5);

        // The Core's changes: the controls follow, and the card writes
        // nothing back.
        QSignalSpy volumes(&w.model, &RadioModel::coreSpeakerVolumeChanged);
        QSignalSpy mutes(&w.model, &RadioModel::coreSpeakerMutedChanged);
        QSignalSpy devices(&w.model, &RadioModel::coreSpeakerDeviceChanged);
        QSignalSpy details(&w.model, &RadioModel::coreSpeakerDetailsChanged);
        w.model.setCoreSpeakerVolume(15);
        w.model.setCoreSpeakerMuted(false);
        w.device(kUsbId, kUsb);
        CoreSpeakerDetails fromCore = *asked;
        fromCore.bufferFrames = 128;
        fromCore.delayMs = 5;
        fromCore.sampleRate = 48000;
        fromCore.delayNowMs = 8.0;
        w.model.setCoreSpeakerDetails(coreSpeakerDetailsToJson(fromCore));
        QCOMPARE(volume->value(), 15);
        QCOMPARE(readout->text(), QStringLiteral("15"));
        QVERIFY(!mute->isChecked());
        QCOMPARE(device->currentText(), kUsb);
        QCOMPARE(buffer->currentData().toInt(), 128);
        QCOMPARE(delay->currentData().toInt(), 5);
        QCOMPARE(rate->currentData().toInt(), 48000);
        QCOMPARE(child<QLabel>(&card, "coreSpeakerDelayNow")->text(),
                 QStringLiteral("Now 8 ms from the radio to USB Audio Device"));
        QCOMPARE(volumes.count(), 1);
        QCOMPARE(mutes.count(), 1);
        QCOMPARE(devices.count(), 1);
        QCOMPARE(details.count(), 1);
    }

    // A window that runs the radio itself has no Core speaker card; a remote
    // window has it between Headphones and Radio speaker (D24).
    void placeOnOutputs()
    {
        {
            RadioModel local;
            AudioOutputsPage page(&local);
            QVERIFY(page.findChild<CoreSpeakerCard*>() == nullptr);
            QVERIFY(page.findChild<QWidget*>(QStringLiteral("coreSpeakerGroup")) == nullptr);
        }
        Window w;
        AudioOutputsPage page(&w.model);
        auto* headphones = page.findChild<QWidget*>(QStringLiteral("headphonesGroup"));
        auto* core = page.findChild<QWidget*>(QStringLiteral("coreSpeakerGroup"));
        auto* radio = page.findChild<QWidget*>(QStringLiteral("radioSpeakerGroup"));
        QVERIFY(headphones && core && radio);
        QLayout* layout = core->parentWidget()->layout();
        QVERIFY(layout != nullptr);
        QCOMPARE(layout->indexOf(core), layout->indexOf(headphones) + 1);
        QCOMPARE(layout->indexOf(radio), layout->indexOf(core) + 1);
        // The page's station gate reaches the card.
        page.setStationSettingsAvailable(false, kUnreachable);
        QVERIFY(!child<QSlider>(core, "coreSpeakerVolume")->isEnabled());
        page.setStationSettingsAvailable(true, QString());
        QVERIFY(child<QSlider>(core, "coreSpeakerVolume")->isEnabled());
    }

    // 7. Captures of each state (V-UI-4), compared by hand with
    // core-speaker-mockup.html.
    void captures()
    {
        if (qEnvironmentVariable("NEREUS_AUDIO_SETUP_CAPTURE_DIR").isEmpty()) {
            QSKIP("Set NEREUS_AUDIO_SETUP_CAPTURE_DIR to save the captures.");
        }
        // The app's own style, palette and baseline QSS (main.cpp).
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        applyDarkPalette(*qApp);
        applyAppBaselineQss(*qApp);
        auto shoot = [](RadioModel& model, const QString& stem, bool unfold) {
            AudioOutputsPage page(&model);
            page.resize(760, unfold ? 1300 : 1080);
            if (unfold) {
                page.findChild<CoreSpeakerCard*>()->setDetailsExpanded(true);
            }
            page.show();
            QApplication::processEvents();
            saveCapture(&page, stem);
        };
        auto shootCard = [](RadioModel& model, const QString& stem, bool unfold) {
            CoreSpeakerCard card(&model, nullptr);
            card.setDetailsExpanded(unfold);
            card.resize(740, card.sizeHint().height());
            card.setAutoFillBackground(true);
            card.show();
            QApplication::processEvents();
            card.resize(740, card.sizeHint().height());
            saveCapture(&card, stem);
        };
        {
            RadioModel local;
            shoot(local, QStringLiteral("core-speaker-local-window-absent"), false);
        }
        {
            Window w;
            w.playingUsb();
            shoot(w.model, QStringLiteral("core-speaker-remote-outputs"), false);
            shootCard(w.model, QStringLiteral("core-speaker-playing-unfolded"), true);
            w.link.offers = false;
            w.model.reportStationLinkStateChanged();
            shootCard(w.model, QStringLiteral("core-speaker-unreachable"), true);
            w.link.older = true;
            w.model.reportStationLinkStateChanged();
            shootCard(w.model, QStringLiteral("core-speaker-older-core"), false);
        }
        {
            Window w;
            w.cards({});
            w.state(CoreSpeakerStateKind::NoCard, QString(), QString());
            shootCard(w.model, QStringLiteral("core-speaker-no-card"), false);
        }
        {
            Window w;
            w.cards({{kJackId, kJack, AudioDeviceState::Present},
                     {kUsbId, kUsb, AudioDeviceState::NotConnected}});
            w.device(kUsbId, kUsb);
            w.state(CoreSpeakerStateKind::NotConnected, kJack, kUsb);
            shootCard(w.model, QStringLiteral("core-speaker-chosen-missing"), false);
            w.cards({{kUsbId, kUsb, AudioDeviceState::NotConnected}});
            w.state(CoreSpeakerStateKind::NotConnected, QString(), kUsb);
            shootCard(w.model, QStringLiteral("core-speaker-chosen-missing-no-other"), false);
            w.cards({{kJackId, kJack, AudioDeviceState::Present},
                     {kUsbId, kUsb, AudioDeviceState::InUse}});
            w.state(CoreSpeakerStateKind::InUse, kJack, kUsb);
            shootCard(w.model, QStringLiteral("core-speaker-chosen-in-use"), false);
        }
        {
            Window w;
            w.cards({{kJackId, kJack, AudioDeviceState::Present},
                     {kHdmiId, kHdmi, AudioDeviceState::Present},
                     {kUsbId, kUsb, AudioDeviceState::Present}});
            w.device(QStringLiteral("(none)"), QString());
            w.state(CoreSpeakerStateKind::WaitingForPick, QString(), QString(), true);
            shootCard(w.model, QStringLiteral("core-speaker-desktop-waiting"), false);
            w.device(kUsbId, kUsb);
            w.state(CoreSpeakerStateKind::Playing, kUsb, kUsb, true);
            shootCard(w.model, QStringLiteral("core-speaker-desktop-picked"), false);
        }
    }
};

QTEST_MAIN(TstCoreSpeakerCard)
#include "tst_core_speaker_card.moc"
