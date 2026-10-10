// no-port-check: NereusSDR-original. A pick in the header's speakers menu
// reaches an open Outputs card through MainWindow.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tests/tst_main_window_header_speaker_pick.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port.
//
// R-AUD-23 / R-AUD-04: the header's PC speakers menu saves a pick as the
// Outputs card saves one, and MainWindow reloads an open Outputs card
// ("This computer", thisComputerGroup) so it shows the pick at once. The
// pick goes through the real window: its title bar's MasterOutputWidget,
// its local engine (on fake engine backends, no device) and its Setup
// dialog at Audio, Outputs.
//
// The design: docs/architecture/2026-10-08-native-audio-engines-design.md
// (R-AUD-23, D20).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-09: native audio fix wave (R-AUD-23, R-AUD-04). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QAction>
#include <QComboBox>
#include <QMenu>

#include <memory>
#include <vector>

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "fakes/FakeAudioEngineBackend.h"
#include "fakes/MainWindowTestSettings.h"
#include "gui/MainWindow.h"
#include "gui/setup/DeviceCard.h"
#include "gui/widgets/MasterOutputWidget.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

AudioDeviceInfo output(const QString& id, const QString& name)
{
    AudioDeviceInfo info;
    info.backend = AudioBackendId::CoreAudio;
    info.direction = AudioDeviceDirection::Output;
    info.id = id;
    info.name = name;
    info.channelCount = 2;
    return info;
}

} // namespace

class TstMainWindowHeaderSpeakerPick : public QObject {
    Q_OBJECT

private slots:
    void headerPickReloadsTheOpenOutputsCard()
    {
        AppSettings::instance().clear();
        Test::markAudioFirstRunDone();
        AudioDeviceConfig saved;
        saved.engine = AudioEngineKind::CoreAudio;
        saved.deviceId = QStringLiteral("desk-uid");
        saved.deviceName = QStringLiteral("Desk speakers");
        saved.saveToSettings(QStringLiteral("audio/Speakers"));

        MainWindow window(RemoteStationOptions{}, nullptr, MainWindow::ConnectionStartup::Deferred);
        AudioEngine* engine = window.radioModel()->localAudioDevices();
        QVERIFY(engine != nullptr);
        auto native = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::CoreAudio);
        native->setDevices({output(QStringLiteral("desk-uid"), QStringLiteral("Desk speakers")),
                            output(QStringLiteral("built-in-uid"),
                                   QStringLiteral("Built-in speakers"))});
        native->setDefault(AudioDeviceDirection::Output, QStringLiteral("built-in-uid"));
        auto older = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PortAudio);
        older->setTakesStereoMix(false);
        engine->setAudioBackendsForTest({native, older});
        QVERIFY(engine->catalogue() != nullptr);

        QVERIFY(QMetaObject::invokeMethod(&window, "openSetupAtPage",
                                          Q_ARG(QString, QStringLiteral("Outputs"))));
        auto* card = window.findChild<DeviceCard*>(QStringLiteral("thisComputerGroup"));
        QVERIFY(card != nullptr);
        QCOMPARE(card->currentConfig().deviceId, QStringLiteral("desk-uid"));

        auto* header = window.findChild<MasterOutputWidget*>();
        QVERIFY(header != nullptr);
        std::unique_ptr<QMenu> menu(header->buildSpeakerMenuForTest());
        QAction* pick = nullptr;
        for (QAction* a : menu->actions()) {
            if (a->text() == QStringLiteral("Built-in speakers")) {
                pick = a;
            }
        }
        QVERIFY(pick != nullptr);
        QVERIFY(pick->isEnabled());
        pick->trigger();

        // MainWindow reloaded the open card: it shows the pick now.
        QCOMPARE(card->currentConfig().deviceId, QStringLiteral("built-in-uid"));
        QCOMPARE(card->deviceCombo()->currentText(), QStringLiteral("Built-in speakers"));
    }
};

QTEST_MAIN(TstMainWindowHeaderSpeakerPick)
#include "tst_main_window_header_speaker_pick.moc"
