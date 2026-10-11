// =================================================================
// tests/tst_device_card_pairs_asio.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR test.
//
// Native audio plan Task 17 (R-AUD-07, R-AUD-19 to R-AUD-22, settled call
// 28, V-SW-7, V-UI-1): the Setup device cards over an engine on fake
// engine backends (Windows audio, ASIO, older drivers; Core Audio and
// PipeWire for the pairs) and fake ASIO caps.  No device, driver or
// helper is opened.
//
// Coverage:
//   1. An interface lists a heading of its name and its pairs indented
//      under it ("Outputs 1-2" to "Outputs 9-10"; a 5-channel one ends
//      with "Output 5"), on ASIO, Core Audio and PipeWire; picking a pair
//      saves FirstChannel; the output names its role to the engine.
//   2. Speakers and headphones on one pair both show the same-pair note;
//      on different pairs neither does.
//   3. The mic card's "Mic is on:" Left, Right, Both saves MicChannel; it
//      is never hidden and greyed with the mic off.
//   4. V-SW-7: a mic pick on a second ASIO driver opens the switch-all
//      dialog with every move; Cancel restores the selection and writes no
//      key; OK moves every role and its card shows the new pair.
//   5. The ASIO buffer sizes and rates come from the caps; a change in
//      one card changes every card on the driver; the shared note; a
//      driver with one size shows it greyed with its reason.
//   6. The ASIO control panel button, the restarted note for 5 s after a
//      reset, and the greyed pairs of a driver with an unusable format.
//      A reset's new buffer size and rate are saved.
//   7. The pure helpers: asioBufferChoices, asioSharedNote, the dialog.
//   8. V-UI-1 captures (NEREUS_AUDIO_SETUP_CAPTURE_DIR).
//
// Design spec: docs/architecture/2026-10-08-native-audio-engines-design.md
// Mockup: docs/architecture/2026-10-08-native-audio-engines-design/asio-setup-mockup.html
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 17 (R-AUD-07, R-AUD-19 to R-AUD-22).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan final fix wave (R-AUD-20, R-AUD-21):
//               resetSizeIsSaved.  J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QStandardItemModel>
#include <QStyleFactory>
#include <QTimer>
#include <QVBoxLayout>

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/audio/CaptureProtocol.h"
#include "core/audio/IAudioDeviceCatalog.h"
#include "gui/setup/AsioSwitchAllDialog.h"
#include "gui/setup/AudioDriverList.h"
#include "gui/setup/DeviceCard.h"
#include "gui/styles/AppTheme.h"

#include "fakes/FakeAudioEngineBackend.h"

#include <functional>
#include <memory>

using namespace NereusSDR;

namespace {

const QString kFocusrite = QStringLiteral("Focusrite USB ASIO");
const QString kMotu = QStringLiteral("MOTU M Series");
const QString kOldBox = QStringLiteral("Old Box ASIO");
constexpr char kUnwired[] = "a DeviceCard signal was not found (is DeviceCard exported from the GUI DLL?)";

AudioDeviceInfo device(AudioBackendId backend, AudioDeviceDirection direction, const QString& id,
                       const QString& name, int channels)
{
    AudioDeviceInfo info;
    info.backend = backend;
    info.direction = direction;
    info.id = id;
    info.name = name;
    info.channelCount = channels;
    return info;
}

AsioDriverCaps caps(const QString& name, int inputs, int outputs)
{
    AsioDriverCaps c;
    c.name = name;
    c.inputChannels = inputs;
    c.outputChannels = outputs;
    c.sampleType = AsioSampleType::Int32Lsb;
    c.minBufferFrames = 64;
    c.maxBufferFrames = 1024;
    c.preferredBufferFrames = 256;
    c.granularity = -1;
    c.sampleRates = {44100.0, 48000.0, 96000.0};
    c.currentRate = 48000.0;
    return c;
}

void save(const QString& prefix, const QString& engine, const QString& id, int firstChannel)
{
    AppSettings& s = AppSettings::instance();
    s.setValue(prefix + QStringLiteral("/Engine"), engine);
    s.setValue(prefix + QStringLiteral("/DeviceId"), id);
    s.setValue(prefix + QStringLiteral("/DeviceName"), id);
    s.setValue(prefix + QStringLiteral("/FirstChannel"), QString::number(firstChannel));
}

void saveAsio(const QString& prefix, const QString& driver, int firstChannel)
{
    save(prefix, audioEngineKey(AudioEngineKind::Asio), driver, firstChannel);
}

QString value(const QString& key)
{
    return AppSettings::instance().value(key).toString();
}

QStringList audioKeysAndValues()
{
    QStringList out;
    AppSettings& s = AppSettings::instance();
    for (const QString& k : s.allKeys()) {
        if (k.startsWith(QStringLiteral("audio/"))) {
            out << k + QLatin1Char('=') + s.value(k).toString();
        }
    }
    out.sort();
    return out;
}

// An engine on fake backends, never started: Windows audio, ASIO (two
// interfaces and a driver with an unusable format) and the older drivers,
// or one native engine with an interface of `channels` outputs and inputs.
struct Rig {
    std::shared_ptr<FakeAudioEngineBackend> native;
    std::shared_ptr<FakeAudioEngineBackend> asio;
    std::shared_ptr<FakeAudioEngineBackend> older =
        std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PortAudio);
    std::unique_ptr<AudioEngine> engine;

    static Rig windows()
    {
        Rig rig(AudioBackendId::Wasapi);
        rig.native->setDevices(
            {device(AudioBackendId::Wasapi, AudioDeviceDirection::Output, QStringLiteral("spk-uid"),
                    QStringLiteral("Desk speakers"), 2),
             device(AudioBackendId::Wasapi, AudioDeviceDirection::Input, QStringLiteral("mic-uid"),
                    QStringLiteral("USB Mic"), 2)});
        rig.asio = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::Asio);
        rig.asio->setHasControlPanel(true);
        rig.asio->setOpensOneStreamAtATime(true);
        rig.asio->setDevices(
            {device(AudioBackendId::Asio, AudioDeviceDirection::Output, kFocusrite, kFocusrite, 10),
             device(AudioBackendId::Asio, AudioDeviceDirection::Input, kFocusrite, kFocusrite, 2),
             device(AudioBackendId::Asio, AudioDeviceDirection::Output, kMotu, kMotu, 4),
             device(AudioBackendId::Asio, AudioDeviceDirection::Input, kMotu, kMotu, 4),
             device(AudioBackendId::Asio, AudioDeviceDirection::Output, kOldBox, kOldBox, 2)});
        return rig;
    }

    static Rig nativeRig(AudioBackendId id, int channels)
    {
        Rig rig(id);
        rig.native->setDevices(
            {device(id, AudioDeviceDirection::Output, QStringLiteral("iface-uid"),
                    QStringLiteral("Scarlett 18i20"), channels),
             device(id, AudioDeviceDirection::Output, QStringLiteral("built-in-uid"),
                    QStringLiteral("Built-in speakers"), 2),
             device(id, AudioDeviceDirection::Input, QStringLiteral("iface-uid"),
                    QStringLiteral("Scarlett 18i20"), channels)});
        rig.native->setDefault(AudioDeviceDirection::Output, QStringLiteral("built-in-uid"));
        return rig;
    }

    void prepare()
    {
        engine = std::make_unique<AudioEngine>();
        // A program that does not exist: no helper is ever started.
        CaptureSupervisor::Options options;
        options.program = QDir::temp().absoluteFilePath(QStringLiteral("no-such-capture-helper"));
        engine->setCaptureSupervisorOptionsForTest(options);
        engine->setVaxOutputsAllowed(false);
        std::vector<std::shared_ptr<IAudioEngineBackend>> backends{native};
        if (asio) {
            backends.push_back(asio);
        }
        backends.push_back(older);
        engine->setAudioBackendsForTest(backends);
        if (asio) {
            engine->setAsioDriverCapsForTest(kFocusrite, caps(kFocusrite, 2, 10));
            engine->setAsioDriverCapsForTest(kMotu, caps(kMotu, 4, 4));
            AsioDriverCaps old = caps(kOldBox, 0, 2);
            old.sampleType = AsioSampleType::Unsupported;
            engine->setAsioDriverCapsForTest(kOldBox, old);
        }
        QVERIFY(engine->catalogue() != nullptr);
    }

    // A card as the pages wire it: its edits reach the engine.  False when
    // a connect fails (on Windows a signal of a class the GUI DLL does not
    // export is not found from the test), so the test fails at once rather
    // than waiting out every QTRY for an engine that is never told.
    [[nodiscard]] bool card(const QString& prefix, std::unique_ptr<DeviceCard>& out)
    {
        const bool input = prefix == QLatin1String("audio/TxInput");
        const bool enable = prefix == QLatin1String("audio/Headphones");
        auto c = std::make_unique<DeviceCard>(prefix, input ? DeviceCard::Role::Input
                                                            : DeviceCard::Role::Output,
                                              enable);
        AudioEngine* e = engine.get();
        const std::optional<AudioRole> role = DeviceCard::roleForPrefix(prefix);
        bool wired = static_cast<bool>(QObject::connect(
            c.get(), &DeviceCard::configChanged, e, [e, role](const AudioDeviceConfig& cfg) {
                switch (*role) {
                case AudioRole::Speakers: e->setSpeakersConfig(cfg); break;
                case AudioRole::Headphones: e->setHeadphonesConfig(cfg); break;
                case AudioRole::TxInput: e->setTxInputConfig(cfg); break;
                default: e->setVaxConfig(1, cfg); break;
                }
            }));
        if (enable) {
            wired = static_cast<bool>(QObject::connect(c.get(), &DeviceCard::enabledChanged, e,
                                                       [e](bool on) { e->setHeadphonesEnabled(on); }))
                && wired;
        }
        c->setAudioEngine(e);
        out = std::move(c);
        return wired;
    }

private:
    explicit Rig(AudioBackendId nativeId)
        : native(std::make_shared<FakeAudioEngineBackend>(nativeId))
    {
        older->setTakesStereoMix(false);
    }
};

int indexOf(QComboBox* combo, const QString& text)
{
    for (int i = 0; i < combo->count(); ++i) {
        if (combo->itemText(i) == text) {
            return i;
        }
    }
    return -1;
}

bool itemEnabled(QComboBox* combo, int index)
{
    auto* model = qobject_cast<QStandardItemModel*>(combo->model());
    return model != nullptr && model->item(index) != nullptr && model->item(index)->isEnabled();
}

template <typename T>
T* child(QWidget& w, const char* name)
{
    return w.findChild<T*>(QLatin1String(name));
}

QStringList comboTexts(QComboBox* combo)
{
    QStringList out;
    for (int i = 0; i < combo->count(); ++i) {
        out << combo->itemText(i);
    }
    return out;
}

// Answers the next AsioSwitchAllDialog: records its texts, then presses
// OK or Cancel.
struct DialogAnswer {
    bool seen = false;
    QString title;
    QString text;
    QStringList moves;
    QString okText;
    QString cancelText;
};

void answerSwitchDialog(bool ok, DialogAnswer* answer, QWidget* captureFor = nullptr,
                        const QString& captureStem = {})
{
    auto poll = std::make_shared<std::function<void(int)>>();
    *poll = [=](int tries) {
        for (QWidget* top : QApplication::topLevelWidgets()) {
            auto* dialog = qobject_cast<AsioSwitchAllDialog*>(top);
            if (dialog == nullptr || !dialog->isVisible()) {
                continue;
            }
            answer->seen = true;
            answer->title = dialog->windowTitle();
            if (auto* t = dialog->findChild<QLabel*>(QStringLiteral("asioSwitchAllText"))) {
                answer->text = t->text();
            }
            for (QLabel* line : dialog->findChildren<QLabel*>(QStringLiteral("asioSwitchAllMove"))) {
                answer->moves << line->text();
            }
            auto* okButton = dialog->findChild<QPushButton*>(QStringLiteral("asioSwitchAllOk"));
            auto* cancelButton = dialog->findChild<QPushButton*>(QStringLiteral("asioSwitchAllCancel"));
            answer->okText = okButton ? okButton->text() : QString();
            answer->cancelText = cancelButton ? cancelButton->text() : QString();
            if (captureFor != nullptr || !captureStem.isEmpty()) {
                const QString dir = qEnvironmentVariable("NEREUS_AUDIO_SETUP_CAPTURE_DIR");
                if (!dir.isEmpty()) {
                    QDir().mkpath(dir);
                    const QPixmap shot = dialog->grab();
                    shot.save(QStringLiteral("%1/%2@%3x.png")
                                  .arg(dir, captureStem)
                                  .arg(qRound(shot.devicePixelRatio())));
                }
            }
            QPushButton* press = ok ? okButton : cancelButton;
            if (press != nullptr) {
                press->click();
            } else {
                dialog->reject();
            }
            return;
        }
        if (tries < 200) {
            QTimer::singleShot(10, qApp, [poll, tries]() { (*poll)(tries + 1); });
        }
    };
    QTimer::singleShot(0, qApp, [poll]() { (*poll)(0); });
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

class TstDeviceCardPairsAsio : public QObject {
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

    static void selectAsioPair(DeviceCard& card, const QString& label)
    {
        QComboBox* devices = card.deviceCombo();
        const int idx = indexOf(devices, label);
        QVERIFY2(idx > 0, qPrintable(label + QStringLiteral(" in ") + comboTexts(devices).join(QStringLiteral(" | "))));
        devices->setCurrentIndex(idx);
    }

private slots:
    void init() { clearAudioKeys(); }
    void cleanup() { clearAudioKeys(); }

    // ── 1. Pairs ──────────────────────────────────────────────────────────

    void pairsGroupUnderTheInterface_data()
    {
        QTest::addColumn<int>("backend");
        QTest::addColumn<int>("channels");
        QTest::addColumn<QStringList>("pairs");
        const QStringList ten{QStringLiteral("Outputs 1-2"), QStringLiteral("Outputs 3-4"),
                              QStringLiteral("Outputs 5-6"), QStringLiteral("Outputs 7-8"),
                              QStringLiteral("Outputs 9-10")};
        const QStringList five{QStringLiteral("Outputs 1-2"), QStringLiteral("Outputs 3-4"),
                               QStringLiteral("Output 5")};
        QTest::newRow("asio-10") << int(AudioBackendId::Asio) << 10 << ten;
        QTest::newRow("asio-5") << int(AudioBackendId::Asio) << 5 << five;
        QTest::newRow("coreaudio-10") << int(AudioBackendId::CoreAudio) << 10 << ten;
        QTest::newRow("coreaudio-5") << int(AudioBackendId::CoreAudio) << 5 << five;
        QTest::newRow("pipewire-10") << int(AudioBackendId::PipeWire) << 10 << ten;
        QTest::newRow("pipewire-5") << int(AudioBackendId::PipeWire) << 5 << five;
    }

    void pairsGroupUnderTheInterface()
    {
        QFETCH(int, backend);
        QFETCH(int, channels);
        QFETCH(QStringList, pairs);
        const auto id = static_cast<AudioBackendId>(backend);
        const bool onAsio = id == AudioBackendId::Asio;
        Rig rig = onAsio ? Rig::windows() : Rig::nativeRig(id, channels);
        if (onAsio) {
            rig.asio->setDevices(
                {device(AudioBackendId::Asio, AudioDeviceDirection::Output, kFocusrite, kFocusrite,
                        channels)});
        }
        const QString name = onAsio ? kFocusrite : QStringLiteral("Scarlett 18i20");
        const QString deviceId = onAsio ? kFocusrite : QStringLiteral("iface-uid");
        const AudioEngineKind engineKind = onAsio ? AudioEngineKind::Asio
            : id == AudioBackendId::CoreAudio ? AudioEngineKind::CoreAudio
                                              : AudioEngineKind::PipeWire;
        save(QStringLiteral("audio/Speakers"), audioEngineKey(engineKind), deviceId, 1);
        rig.prepare();
        if (onAsio) {
            rig.engine->setAsioDriverCapsForTest(kFocusrite, caps(kFocusrite, 0, channels));
        }
        std::unique_ptr<DeviceCard> card;
        QVERIFY2(rig.card(QStringLiteral("audio/Speakers"), card), kUnwired);
        QComboBox* devices = card->deviceCombo();

        // The heading: the interface's name, never picked.
        const int heading = indexOf(devices, name);
        QVERIFY2(heading > 0, qPrintable(comboTexts(devices).join(QStringLiteral(" | "))));
        QVERIFY(devices->itemData(heading, DeviceCard::kGroupHeadingRole).toBool());
        QVERIFY(!itemEnabled(devices, heading));
        // Its pairs, indented under it by their pair label; the closed
        // field keeps the full label.
        for (int n = 0; n < pairs.size(); ++n) {
            const int row = heading + 1 + n;
            QCOMPARE(devices->itemData(row, DeviceCard::kPopupTextRole).toString(), pairs.at(n));
            QCOMPARE(devices->itemText(row), name + QStringLiteral(" · ") + pairs.at(n));
            QVERIFY(itemEnabled(devices, row));
        }
        // Headings never count as devices.
        const int interfaces = 1;
        const int others = onAsio ? 0 : 1;   // Built-in speakers
        QCOMPARE(card->deviceCount(), pairs.size() + others + (interfaces - 1));

        // Picking a pair saves FirstChannel; the output names its role.
        devices->setCurrentIndex(heading + 2);
        QCOMPARE(value(QStringLiteral("audio/Speakers/FirstChannel")), QStringLiteral("3"));
        QCOMPARE(value(QStringLiteral("audio/Speakers/DeviceId")), deviceId);
        QCOMPARE(devices->currentText(), name + QStringLiteral(" · Outputs 3-4"));
        std::shared_ptr<FakeAudioEngineBackend> backendFake = onAsio ? rig.asio : rig.native;
        QTRY_VERIFY(!backendFake->outputRequests().empty());
        const AudioStreamRequest last = backendFake->outputRequests().back();
        QCOMPARE(last.pair.firstChannel, 3);
        QVERIFY(last.role.has_value());
        QCOMPARE(*last.role, AudioRole::Speakers);

        // A heading is never a choice: picking it keeps the pair.
        devices->setCurrentIndex(heading);
        QCOMPARE(devices->currentIndex(), heading + 2);
        QCOMPARE(value(QStringLiteral("audio/Speakers/FirstChannel")), QStringLiteral("3"));
    }

    // ── 2. The same-pair note ─────────────────────────────────────────────

    void samePairNoteOnBothCards()
    {
        Rig rig = Rig::nativeRig(AudioBackendId::CoreAudio, 10);
        const QString key = audioEngineKey(AudioEngineKind::CoreAudio);
        save(QStringLiteral("audio/Speakers"), key, QStringLiteral("iface-uid"), 3);
        save(QStringLiteral("audio/Headphones"), key, QStringLiteral("iface-uid"), 3);
        AppSettings::instance().setValue(QStringLiteral("audio/Headphones/Enabled"),
                                         QStringLiteral("True"));
        rig.prepare();
        std::unique_ptr<DeviceCard> speakers;
        QVERIFY2(rig.card(QStringLiteral("audio/Speakers"), speakers), kUnwired);
        std::unique_ptr<DeviceCard> headphones;
        QVERIFY2(rig.card(QStringLiteral("audio/Headphones"), headphones), kUnwired);
        const QString note =
            QStringLiteral("Speakers and headphones are on the same pair, so they play together.");
        auto* spkNote = child<QLabel>(*speakers, "samePairNote");
        auto* hpNote = child<QLabel>(*headphones, "samePairNote");
        QVERIFY(spkNote != nullptr && hpNote != nullptr);
        QCOMPARE(spkNote->text(), note);
        QVERIFY(!spkNote->isHidden());
        QCOMPARE(hpNote->text(), note);
        QVERIFY(!hpNote->isHidden());

        // Different pairs: neither card says anything.
        selectAsioPair(*headphones, QStringLiteral("Scarlett 18i20 · Outputs 5-6"));
        QTRY_VERIFY(spkNote->isHidden());
        QVERIFY(hpNote->isHidden());
        QVERIFY(spkNote->text().isEmpty());

        // Back on one pair, then the headphones off: no note.
        selectAsioPair(*headphones, QStringLiteral("Scarlett 18i20 · Outputs 3-4"));
        QTRY_VERIFY(!spkNote->isHidden());
        auto* enabled = headphones->findChild<QCheckBox*>();
        QVERIFY(enabled != nullptr);
        enabled->setChecked(false);
        QTRY_VERIFY(spkNote->isHidden());
        QVERIFY(hpNote->isHidden());
    }

    // ── 3. The mic's side ─────────────────────────────────────────────────

    void micIsOnLeftRightBoth()
    {
        Rig rig = Rig::nativeRig(AudioBackendId::CoreAudio, 4);
        save(QStringLiteral("audio/TxInput"), audioEngineKey(AudioEngineKind::CoreAudio),
             QStringLiteral("iface-uid"), 1);
        rig.prepare();
        std::unique_ptr<DeviceCard> mic;
        QVERIFY2(rig.card(QStringLiteral("audio/TxInput"), mic), kUnwired);
        auto* row = child<QWidget>(*mic, "micChannelRow");
        QVERIFY(row != nullptr);
        QVERIFY(!row->isHidden());
        QVERIFY(row->isEnabled());
        bool labelled = false;
        for (QLabel* l : row->findChildren<QLabel*>()) {
            labelled = labelled || l->text() == QStringLiteral("Mic is on:");
        }
        QVERIFY(labelled);
        auto* left = child<QRadioButton>(*mic, "micChannelLeft");
        auto* right = child<QRadioButton>(*mic, "micChannelRight");
        auto* both = child<QRadioButton>(*mic, "micChannelBoth");
        QVERIFY(left && right && both);
        QCOMPARE(left->text(), QStringLiteral("Left"));
        QCOMPARE(right->text(), QStringLiteral("Right"));
        QCOMPARE(both->text(), QStringLiteral("Both"));
        QVERIFY(left->isChecked());

        right->click();
        QCOMPARE(value(QStringLiteral("audio/TxInput/MicChannel")), QStringLiteral("Right"));
        both->click();
        QCOMPARE(value(QStringLiteral("audio/TxInput/MicChannel")), QStringLiteral("Both"));

        // Loaded back.
        std::unique_ptr<DeviceCard> again;
        QVERIFY2(rig.card(QStringLiteral("audio/TxInput"), again), kUnwired);
        QVERIFY(child<QRadioButton>(*again, "micChannelBoth")->isChecked());
    }

    void micSideGreyedWithTheMicOff()
    {
        Rig rig = Rig::nativeRig(AudioBackendId::CoreAudio, 4);
        save(QStringLiteral("audio/TxInput"), audioEngineKey(AudioEngineKind::CoreAudio),
             QString::fromLatin1(kAudioDeviceNone), 1);
        rig.prepare();
        std::unique_ptr<DeviceCard> mic;
        QVERIFY2(rig.card(QStringLiteral("audio/TxInput"), mic), kUnwired);
        auto* row = child<QWidget>(*mic, "micChannelRow");
        QVERIFY(row != nullptr);
        QCOMPARE(mic->deviceCombo()->currentText(), QString::fromLatin1(kAudioDeviceNone));
        QVERIFY(!row->isHidden());
        QVERIFY(!row->isEnabled());
        QVERIFY(!row->toolTip().isEmpty());
        // A pair turns it back on.
        selectAsioPair(*mic, QStringLiteral("Scarlett 18i20 · Inputs 1-2"));
        QVERIFY(row->isEnabled());
        QVERIFY(row->toolTip().isEmpty());
    }

    // ── 4. V-SW-7: one ASIO driver at a time ──────────────────────────────

    void planMovesEveryOtherRole()
    {
        Rig rig = Rig::windows();
        saveAsio(QStringLiteral("audio/Speakers"), kFocusrite, 3);
        saveAsio(QStringLiteral("audio/Vax1"), kFocusrite, 1);
        AppSettings::instance().setValue(QStringLiteral("audio/Vax1/Enabled"), QStringLiteral("True"));
        saveAsio(QStringLiteral("audio/TxInput"), kFocusrite, 1);
        rig.prepare();
        const AsioSwitchPlan plan =
            rig.engine->planAsioSwitchFor(AudioRole::TxInput, kMotu, AudioChannelPair{1, 2});
        QCOMPARE(plan.toDriver, kMotu);
        QCOMPARE(plan.moves.size(), 2);
        QCOMPARE(plan.moves.at(0).role, AudioRole::Speakers);
        QCOMPARE(plan.moves.at(0).pair, (AudioChannelPair{3, 2}));
        QCOMPARE(plan.moves.at(1).role, AudioRole::Vax1);
        QCOMPARE(plan.moves.at(1).pair, (AudioChannelPair{1, 2}));

        // A VAX channel that is off plays nothing and does not move.
        AppSettings::instance().setValue(QStringLiteral("audio/Vax1/Enabled"), QStringLiteral("False"));
        QCOMPARE(rig.engine->planAsioSwitchFor(AudioRole::TxInput, kMotu, AudioChannelPair{1, 2})
                     .moves.size(),
                 1);
        // Staying on the same driver moves nothing.
        QVERIFY(rig.engine->planAsioSwitchFor(AudioRole::TxInput, kFocusrite, AudioChannelPair{1, 2})
                    .moves.isEmpty());
    }

    void switchAllCancelKeepsEverything()
    {
        Rig rig = Rig::windows();
        saveAsio(QStringLiteral("audio/Speakers"), kFocusrite, 3);
        saveAsio(QStringLiteral("audio/Vax1"), kFocusrite, 1);
        AppSettings::instance().setValue(QStringLiteral("audio/Vax1/Enabled"), QStringLiteral("True"));
        saveAsio(QStringLiteral("audio/TxInput"), kFocusrite, 1);
        rig.prepare();
        std::unique_ptr<DeviceCard> mic;
        QVERIFY2(rig.card(QStringLiteral("audio/TxInput"), mic), kUnwired);
        const QString before = mic->deviceCombo()->currentText();
        QCOMPARE(before, kFocusrite);
        const QStringList keys = audioKeysAndValues();

        DialogAnswer answer;
        answerSwitchDialog(false, &answer);
        selectAsioPair(*mic, kMotu + QStringLiteral(" · Inputs 1-2"));
        QVERIFY(answer.seen);
        QCOMPARE(answer.title, QStringLiteral("One ASIO driver at a time"));
        QCOMPARE(answer.text,
                 QStringLiteral("NereusSDR can use one ASIO driver at a time. Switching Microphone "
                                "to MOTU M Series also moves:"));
        QCOMPARE(answer.moves,
                 (QStringList{QStringLiteral("Speakers: Outputs 3-4"),
                              QStringLiteral("VAX 1: Outputs 1-2")}));
        QCOMPARE(answer.okText, QStringLiteral("Switch all to MOTU M Series"));
        QCOMPARE(answer.cancelText, QStringLiteral("Cancel"));

        QCOMPARE(mic->deviceCombo()->currentText(), before);
        QCOMPARE(audioKeysAndValues(), keys);
    }

    void switchAllOkMovesEveryRole()
    {
        Rig rig = Rig::windows();
        saveAsio(QStringLiteral("audio/Speakers"), kFocusrite, 3);
        saveAsio(QStringLiteral("audio/Vax1"), kFocusrite, 1);
        AppSettings::instance().setValue(QStringLiteral("audio/Vax1/Enabled"), QStringLiteral("True"));
        saveAsio(QStringLiteral("audio/TxInput"), kFocusrite, 1);
        rig.prepare();
        std::unique_ptr<DeviceCard> speakers;
        QVERIFY2(rig.card(QStringLiteral("audio/Speakers"), speakers), kUnwired);
        std::unique_ptr<DeviceCard> mic;
        QVERIFY2(rig.card(QStringLiteral("audio/TxInput"), mic), kUnwired);
        QCOMPARE(speakers->deviceCombo()->currentText(), kFocusrite + QStringLiteral(" · Outputs 3-4"));
        AppSettings::instance().setValue(QStringLiteral("audio/Speakers/ExclusiveMode"),
                                         QStringLiteral("True"));

        DialogAnswer answer;
        answerSwitchDialog(true, &answer);
        selectAsioPair(*mic, kMotu + QStringLiteral(" · Inputs 3-4"));
        QVERIFY(answer.seen);

        QCOMPARE(value(QStringLiteral("audio/TxInput/DeviceId")), kMotu);
        QCOMPARE(value(QStringLiteral("audio/TxInput/FirstChannel")), QStringLiteral("3"));
        QCOMPARE(value(QStringLiteral("audio/Speakers/DeviceId")), kMotu);
        QCOMPARE(value(QStringLiteral("audio/Speakers/FirstChannel")), QStringLiteral("3"));
        QCOMPARE(value(QStringLiteral("audio/Vax1/DeviceId")), kMotu);
        QCOMPARE(value(QStringLiteral("audio/Vax1/FirstChannel")), QStringLiteral("1"));
        // The retired keys stay as they were.
        QCOMPARE(value(QStringLiteral("audio/Speakers/ExclusiveMode")), QStringLiteral("True"));
        // The moved card shows its new pair.
        QTRY_COMPARE(speakers->deviceCombo()->currentText(), kMotu + QStringLiteral(" · Outputs 3-4"));
        QCOMPARE(mic->deviceCombo()->currentText(), kMotu + QStringLiteral(" · Inputs 3-4"));
        const AsioStatus status = rig.engine->asioStatus();
        QCOMPARE(status.driver, kMotu);
        QCOMPARE(status.users,
                 (QList<AudioRole>{AudioRole::Speakers, AudioRole::TxInput, AudioRole::Vax1}));
    }

    // ── 5. Buffer size and sample rate ────────────────────────────────────

    void asioListsComeFromTheCaps()
    {
        Rig rig = Rig::windows();
        saveAsio(QStringLiteral("audio/Speakers"), kFocusrite, 1);
        rig.prepare();
        std::unique_ptr<DeviceCard> speakers;
        QVERIFY2(rig.card(QStringLiteral("audio/Speakers"), speakers), kUnwired);
        QComboBox* buffer = speakers->bufferSizeCombo();
        QCOMPARE(comboTexts(buffer),
                 (QStringList{QStringLiteral("64 samples"), QStringLiteral("128 samples"),
                              QStringLiteral("256 samples"), QStringLiteral("512 samples"),
                              QStringLiteral("1024 samples")}));
        QCOMPARE(buffer->currentData().toInt(), 256);   // the preferred size
        QComboBox* rate = nullptr;
        for (QComboBox* c : speakers->findChildren<QComboBox*>()) {
            if (c->count() > 0 && c->itemText(0).endsWith(QStringLiteral(" Hz"))) {
                rate = c;
            }
        }
        QVERIFY(rate != nullptr);
        QCOMPARE(comboTexts(rate), (QStringList{QStringLiteral("44100 Hz"), QStringLiteral("48000 Hz"),
                                                QStringLiteral("96000 Hz")}));
        QCOMPARE(rate->currentData().toInt(), 48000);

        // A driver stepping by 32.
        AsioDriverCaps stepped = caps(kFocusrite, 2, 10);
        stepped.maxBufferFrames = 256;
        stepped.granularity = 32;
        rig.engine->setAsioDriverCapsForTest(kFocusrite, stepped);
        QTRY_COMPARE(buffer->count(), 7);
        QCOMPARE(buffer->itemData(1).toInt(), 96);
        QCOMPARE(buffer->itemData(6).toInt(), 256);
        QVERIFY(buffer->isEnabled());
    }

    void oneCardChangesEveryCardOnTheDriver()
    {
        Rig rig = Rig::windows();
        saveAsio(QStringLiteral("audio/Speakers"), kFocusrite, 1);
        saveAsio(QStringLiteral("audio/Headphones"), kFocusrite, 3);
        AppSettings::instance().setValue(QStringLiteral("audio/Headphones/Enabled"),
                                         QStringLiteral("True"));
        saveAsio(QStringLiteral("audio/TxInput"), kFocusrite, 1);
        rig.prepare();
        std::unique_ptr<DeviceCard> speakers;
        QVERIFY2(rig.card(QStringLiteral("audio/Speakers"), speakers), kUnwired);
        std::unique_ptr<DeviceCard> headphones;
        QVERIFY2(rig.card(QStringLiteral("audio/Headphones"), headphones), kUnwired);
        std::unique_ptr<DeviceCard> mic;
        QVERIFY2(rig.card(QStringLiteral("audio/TxInput"), mic), kUnwired);

        auto* spkShared = child<QLabel>(*speakers, "asioSharedNote");
        auto* hpShared = child<QLabel>(*headphones, "asioSharedNote");
        auto* micShared = child<QLabel>(*mic, "asioSharedNote");
        QVERIFY(spkShared && hpShared && micShared);
        QCOMPARE(spkShared->text(), QStringLiteral("Buffer size and sample rate are shared with "
                                                   "Headphones and Microphone, on the same ASIO driver."));
        QVERIFY(!spkShared->isHidden());
        QCOMPARE(micShared->text(), QStringLiteral("Buffer size and sample rate are shared with "
                                                   "Speakers and Headphones, on the same ASIO driver."));

        const int idx = speakers->bufferSizeCombo()->findData(512);
        QVERIFY(idx >= 0);
        speakers->bufferSizeCombo()->setCurrentIndex(idx);   // through the 200 ms debounce
        QTRY_COMPARE(value(QStringLiteral("audio/Asio/BufferFrames")), QStringLiteral("512"));
        QTRY_COMPARE(headphones->bufferSizeCombo()->currentData().toInt(), 512);
        QCOMPARE(mic->bufferSizeCombo()->currentData().toInt(), 512);
        QCOMPARE(value(QStringLiteral("audio/Headphones/BufferSamples")), QStringLiteral("512"));
        QCOMPARE(value(QStringLiteral("audio/TxInput/BufferSamples")), QStringLiteral("512"));
        QCOMPARE(rig.engine->asioStatus().bufferFrames, 512);

        // A fourth role: three names.
        saveAsio(QStringLiteral("audio/Vax1"), kFocusrite, 5);
        AppSettings::instance().setValue(QStringLiteral("audio/Vax1/Enabled"), QStringLiteral("True"));
        rig.engine->setAsioBufferAndRate(512, 96000.0);
        QCOMPARE(spkShared->text(), QStringLiteral("Buffer size and sample rate are shared with "
                                                   "Headphones, Microphone and VAX 1, on the same "
                                                   "ASIO driver."));
        QCOMPARE(value(QStringLiteral("audio/Asio/SampleRate")), QStringLiteral("96000"));
        QCOMPARE(value(QStringLiteral("audio/Vax1/SampleRate")), QStringLiteral("96000"));
    }

    void oneBufferSizeIsGreyed()
    {
        Rig rig = Rig::windows();
        saveAsio(QStringLiteral("audio/Speakers"), kMotu, 1);
        rig.prepare();
        AsioDriverCaps fixed = caps(kMotu, 4, 4);
        fixed.minBufferFrames = 128;
        fixed.maxBufferFrames = 128;
        fixed.preferredBufferFrames = 128;
        fixed.granularity = 0;
        rig.engine->setAsioDriverCapsForTest(kMotu, fixed);
        std::unique_ptr<DeviceCard> speakers;
        QVERIFY2(rig.card(QStringLiteral("audio/Speakers"), speakers), kUnwired);
        QComboBox* buffer = speakers->bufferSizeCombo();
        QCOMPARE(comboTexts(buffer), QStringList{QStringLiteral("128 samples")});
        QVERIFY(!buffer->isHidden());
        QVERIFY(!buffer->isEnabled());
        QCOMPARE(buffer->toolTip(), QStringLiteral("Set in the ASIO control panel"));
        auto* note = child<QLabel>(*speakers, "asioBufferNote");
        QVERIFY(note != nullptr);
        QCOMPARE(note->text(), QStringLiteral("Set in the ASIO control panel"));
        QVERIFY(!note->isHidden());
        // No other role: no shared note.
        QVERIFY(child<QLabel>(*speakers, "asioSharedNote")->isHidden());
    }

    // ── 6. Control panel, restart, format ─────────────────────────────────

    void controlPanelButton()
    {
        Rig rig = Rig::windows();
        saveAsio(QStringLiteral("audio/Speakers"), kFocusrite, 1);
        save(QStringLiteral("audio/Headphones"), audioEngineKey(AudioEngineKind::WindowsShared),
             QStringLiteral("spk-uid"), 1);
        rig.prepare();
        std::unique_ptr<DeviceCard> speakers;
        QVERIFY2(rig.card(QStringLiteral("audio/Speakers"), speakers), kUnwired);
        std::unique_ptr<DeviceCard> headphones;
        QVERIFY2(rig.card(QStringLiteral("audio/Headphones"), headphones), kUnwired);
        auto* button = child<QPushButton>(*speakers, "asioControlPanel");
        QVERIFY(button != nullptr);
        QCOMPARE(button->text(), QStringLiteral("ASIO control panel"));
        QVERIFY(button->isEnabled());
        QCOMPARE(button->toolTip(), QStringLiteral("Opens the driver's own settings window"));
        button->click();
        QCOMPARE(rig.asio->controlPanelOpens(), std::vector<QString>{kFocusrite});

        auto* other = child<QPushButton>(*headphones, "asioControlPanel");
        QVERIFY(other != nullptr);
        QVERIFY(!other->isHidden());
        QVERIFY(!other->isEnabled());
#if defined(Q_OS_WIN)
        QCOMPARE(other->toolTip(), QStringLiteral("For ASIO drivers"));
#else
        QCOMPARE(other->toolTip(), QStringLiteral("ASIO drivers are Windows only."));
#endif
    }

    void controlPanelGreyedOnTheMac()
    {
        Rig rig = Rig::nativeRig(AudioBackendId::CoreAudio, 2);
        rig.prepare();
        std::unique_ptr<DeviceCard> speakers;
        QVERIFY2(rig.card(QStringLiteral("audio/Speakers"), speakers), kUnwired);
        auto* button = child<QPushButton>(*speakers, "asioControlPanel");
        QVERIFY(button != nullptr);
        QVERIFY(!button->isHidden());
        QVERIFY(!button->isEnabled());
        QVERIFY(!button->toolTip().isEmpty());
    }

    void restartedNoteForFiveSeconds()
    {
        QCOMPARE(AudioEngine::kAsioRestartNoteMs, 5000);
        Rig rig = Rig::windows();
        saveAsio(QStringLiteral("audio/Speakers"), kFocusrite, 1);
        rig.prepare();
        rig.engine->setAsioRestartNoteMsForTest(300);
        std::unique_ptr<DeviceCard> speakers;
        QVERIFY2(rig.card(QStringLiteral("audio/Speakers"), speakers), kUnwired);
        auto* note = child<QLabel>(*speakers, "asioRestartedNote");
        QVERIFY(note != nullptr);
        QCOMPARE(note->text(), QStringLiteral("Restarted with the driver's new settings."));
        QVERIFY(note->isHidden());

        // A restart for the mic coming or going says nothing.
        CaptureProtocol::AsioState state;
        state.state = CaptureProtocol::AsioStateKind::Restarted;
        state.driver = kFocusrite;
        state.bufferFrames = 256;
        state.rate = 48000.0;
        rig.engine->deliverAsioStateForTest(state);
        QVERIFY(note->isHidden());
        QVERIFY(!rig.engine->asioStatus().restartedRecently);

        // After the driver's reset: shown, then gone.
        state.detail = QString::fromLatin1(CaptureProtocol::kAsioResetDetail);
        rig.engine->deliverAsioStateForTest(state);
        QVERIFY(rig.engine->asioStatus().restartedRecently);
        QVERIFY(!note->isHidden());
        QTRY_VERIFY_WITH_TIMEOUT(note->isHidden(), 3000);
        QVERIFY(!rig.engine->asioStatus().restartedRecently);
    }

    // R-AUD-20, R-AUD-21: the driver's reset brought a new buffer size and
    // rate; they are saved, for the driver and for each role on it, so the
    // next open asks for them.  A restart for the mic saves nothing.
    void resetSizeIsSaved()
    {
        Rig rig = Rig::windows();
        saveAsio(QStringLiteral("audio/Speakers"), kFocusrite, 1);
        AppSettings& s = AppSettings::instance();
        s.setValue(QStringLiteral("audio/Asio/BufferFrames"), QStringLiteral("256"));
        s.setValue(QStringLiteral("audio/Asio/SampleRate"), QStringLiteral("48000"));
        s.setValue(QStringLiteral("audio/Speakers/BufferSamples"), QStringLiteral("256"));
        rig.prepare();
        QVERIFY(rig.engine->asioStatus().users.contains(AudioRole::Speakers));

        CaptureProtocol::AsioState state;
        state.state = CaptureProtocol::AsioStateKind::Restarted;
        state.driver = kFocusrite;
        state.bufferFrames = 512;
        state.rate = 96000.0;
        rig.engine->deliverAsioStateForTest(state);
        QCOMPARE(AudioEngine::savedAsioBufferFrames(), 256);
        QCOMPARE(AudioEngine::savedAsioSampleRate(), 48000.0);

        state.detail = QString::fromLatin1(CaptureProtocol::kAsioResetDetail);
        rig.engine->deliverAsioStateForTest(state);
        QCOMPARE(AudioEngine::savedAsioBufferFrames(), 512);
        QCOMPARE(AudioEngine::savedAsioSampleRate(), 96000.0);
        QCOMPARE(value(QStringLiteral("audio/Speakers/BufferSamples")), QStringLiteral("512"));
        QCOMPARE(value(QStringLiteral("audio/Speakers/SampleRate")), QStringLiteral("96000"));
    }

    void unusableFormatGreysItsPairs()
    {
        Rig rig = Rig::windows();
        saveAsio(QStringLiteral("audio/Speakers"), kFocusrite, 1);
        rig.prepare();
        std::unique_ptr<DeviceCard> speakers;
        QVERIFY2(rig.card(QStringLiteral("audio/Speakers"), speakers), kUnwired);
        QComboBox* devices = speakers->deviceCombo();
        const QString reason =
            QStringLiteral("Old Box ASIO uses a sample format NereusSDR can't play or record.");
        const int old = indexOf(devices, kOldBox);
        QVERIFY(old > 0);
        QVERIFY(!itemEnabled(devices, old));
        QCOMPARE(devices->itemData(old, Qt::ToolTipRole).toString(), reason);
        QVERIFY(itemEnabled(devices, indexOf(devices, kMotu + QStringLiteral(" · Outputs 1-2"))));
        auto* note = child<QLabel>(*speakers, "asioFormatNote");
        QVERIFY(note != nullptr);
        QCOMPARE(note->text(), reason);
        QVERIFY(!note->isHidden());

        // The engine's status says so for the driver in use.
        saveAsio(QStringLiteral("audio/Speakers"), kOldBox, 1);
        QVERIFY(rig.engine->asioStatus().formatUnsupported);
        saveAsio(QStringLiteral("audio/Speakers"), kFocusrite, 1);
        QVERIFY(!rig.engine->asioStatus().formatUnsupported);
    }

    // ── 7. Pure helpers ───────────────────────────────────────────────────

    void bufferChoices()
    {
        AsioDriverCaps c = caps(kFocusrite, 2, 2);
        QCOMPARE(asioBufferChoices(c), (QList<int>{64, 128, 256, 512, 1024}));
        c.granularity = 0;
        c.minBufferFrames = 128;
        c.maxBufferFrames = 128;
        QCOMPARE(asioBufferChoices(c), QList<int>{128});
        c.minBufferFrames = 64;
        c.maxBufferFrames = 2048;
        c.preferredBufferFrames = 480;
        QCOMPARE(asioBufferChoices(c), (QList<int>{64, 480, 2048}));
        c.granularity = 1;   // too many steps to list
        QCOMPARE(asioBufferChoices(c), (QList<int>{64, 128, 256, 480, 512, 1024, 2048}));
        c.minBufferFrames = 0;
        QVERIFY(asioBufferChoices(c).isEmpty());
    }

    void sharedNoteNames()
    {
        QVERIFY(asioSharedNote({}).isEmpty());
        QCOMPARE(asioSharedNote({AudioRole::Headphones}),
                 QStringLiteral("Buffer size and sample rate are shared with Headphones, on the same "
                                "ASIO driver."));
        QCOMPARE(asioSharedNote({AudioRole::Speakers, AudioRole::TxInput, AudioRole::Vax2}),
                 QStringLiteral("Buffer size and sample rate are shared with Speakers, Microphone and "
                                "VAX 2, on the same ASIO driver."));
    }

    void dialogAsTheBriefWritesIt()
    {
        AsioSwitchAllDialog dialog(QStringLiteral("Microphone"), kMotu,
                                   {{QStringLiteral("Speakers"), QStringLiteral("Outputs 3-4")},
                                    {QStringLiteral("VAX 1"), QStringLiteral("Inputs 1-2")}},
                                   nullptr);
        auto* text = dialog.findChild<QLabel*>(QStringLiteral("asioSwitchAllText"));
        QVERIFY(text != nullptr);
        QCOMPARE(text->text(), QStringLiteral("NereusSDR can use one ASIO driver at a time. Switching "
                                              "Microphone to MOTU M Series also moves:"));
        QStringList lines;
        for (QLabel* l : dialog.findChildren<QLabel*>(QStringLiteral("asioSwitchAllMove"))) {
            lines << l->text();
        }
        QCOMPARE(lines, (QStringList{QStringLiteral("Speakers: Outputs 3-4"),
                                     QStringLiteral("VAX 1: Inputs 1-2")}));
        QCOMPARE(dialog.findChild<QPushButton*>(QStringLiteral("asioSwitchAllOk"))->text(),
                 QStringLiteral("Switch all to MOTU M Series"));
        QCOMPARE(dialog.findChild<QPushButton*>(QStringLiteral("asioSwitchAllCancel"))->text(),
                 QStringLiteral("Cancel"));
    }

    // ── 8. V-UI-1 captures ────────────────────────────────────────────────

    void captures()
    {
        if (qEnvironmentVariable("NEREUS_AUDIO_SETUP_CAPTURE_DIR").isEmpty()) {
            QSKIP("Set NEREUS_AUDIO_SETUP_CAPTURE_DIR to save the captures.");
        }
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        applyDarkPalette(*qApp);
        applyAppBaselineQss(*qApp);

        Rig rig = Rig::windows();
        saveAsio(QStringLiteral("audio/Speakers"), kFocusrite, 3);
        saveAsio(QStringLiteral("audio/Headphones"), kFocusrite, 5);
        AppSettings::instance().setValue(QStringLiteral("audio/Headphones/Enabled"),
                                         QStringLiteral("True"));
        saveAsio(QStringLiteral("audio/Vax1"), kFocusrite, 1);
        AppSettings::instance().setValue(QStringLiteral("audio/Vax1/Enabled"), QStringLiteral("True"));
        saveAsio(QStringLiteral("audio/TxInput"), kFocusrite, 1);
        rig.prepare();
        std::unique_ptr<DeviceCard> speakers;
        QVERIFY2(rig.card(QStringLiteral("audio/Speakers"), speakers), kUnwired);
        speakers->setTitle(QStringLiteral("This computer"));
        speakers->resize(620, speakers->sizeHint().height());

        // The pairs: the card and its open Device list.
        QWidget pairs;
        auto* pairsLayout = new QVBoxLayout(&pairs);
        std::unique_ptr<DeviceCard> card;
        QVERIFY2(rig.card(QStringLiteral("audio/Speakers"), card), kUnwired);
        card->setTitle(QStringLiteral("This computer"));
        pairsLayout->addWidget(card.get());
        pairs.resize(620, pairs.sizeHint().height());
        pairs.show();
        card->deviceCombo()->showPopup();
        QApplication::processEvents();
        QAbstractItemView* view = card->deviceCombo()->view();
        QWidget* popup = view->window();
        saveCapture(popup, QStringLiteral("asio-pairs-popup"));
        card->deviceCombo()->hidePopup();
        saveCapture(&pairs, QStringLiteral("asio-pairs-card"));
        pairs.hide();

        // The details with the shared note.
        speakers->setDetailsExpanded(true);
        speakers->resize(620, speakers->sizeHint().height());
        speakers->show();
        saveCapture(speakers.get(), QStringLiteral("asio-details-shared"));
        speakers->hide();

        // The prompt.
        std::unique_ptr<DeviceCard> mic;
        QVERIFY2(rig.card(QStringLiteral("audio/TxInput"), mic), kUnwired);
        mic->setTitle(QStringLiteral("PC microphone"));
        DialogAnswer answer;
        answerSwitchDialog(false, &answer, mic.get(), QStringLiteral("asio-switch-all-prompt"));
        selectAsioPair(*mic, kMotu + QStringLiteral(" · Inputs 1-2"));
        QVERIFY(answer.seen);

        // A driver with one buffer size.
        AsioDriverCaps fixed = caps(kFocusrite, 2, 10);
        fixed.minBufferFrames = 128;
        fixed.maxBufferFrames = 128;
        fixed.preferredBufferFrames = 128;
        fixed.granularity = 0;
        rig.engine->setAsioDriverCapsForTest(kFocusrite, fixed);
        speakers->resize(620, speakers->sizeHint().height());
        speakers->show();
        saveCapture(speakers.get(), QStringLiteral("asio-buffer-fixed"));
        speakers->hide();
    }
};

QTEST_MAIN(TstDeviceCardPairsAsio)
#include "tst_device_card_pairs_asio.moc"
