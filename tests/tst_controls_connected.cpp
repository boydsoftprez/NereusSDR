// =================================================================
// tests/tst_controls_connected.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. It drives real local and remote
// MainWindows and widgets; no upstream logic is ported here.
//
// R3 controls that work, Task 2 (R-R3-21): menu items, applet controls,
// container controls, pan overlay buttons and status badges that front a
// feature NereusSDR already has now reach it, in a local window and, where
// the feature works remotely, in a remote one (otherwise they say why).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  R3 controls that work, Task 2.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 2): the AM
//                                    carrier follows the transmit settings
//                                    gate. AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 3): the mic profile combo follows the
//                                    transmit settings gate.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QFile>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScopeGuard>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>

#include "OperatorWording.h"
#include "core/session/IStationLink.h"
#include "core/AppSettings.h"
#include "core/BuildIdentity.h"
#include "core/MicProfileManager.h"
#include "core/RadioDiscovery.h"
#include "core/StepAttenuatorFacade.h"
#include "core/WdspTypes.h"
#include "gui/DiversityDialog.h"
#include "gui/GuiSessionCoordinator.h"
#include "gui/HGauge.h"
#include "gui/MainWindow.h"
#include "gui/SetupDialog.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/PhoneCwApplet.h"
#include "gui/diagnostics/RadioStatusPage.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/AntennaButtonItem.h"
#include "gui/meters/FilterButtonItem.h"
#include "gui/meters/FilterDisplayItem.h"
#include "gui/meters/HistoryGraphItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/meters/TuneStepButtonItem.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/setup/HardwarePage.h"
#include "gui/widgets/RxDashboard.h"
#include "gui/widgets/StatusBadge.h"
#include "gui/widgets/StatusToast.h"
#include "gui/widgets/SystemTile.h"
#include "gui/widgets/VfoWidget.h"
#include "models/Band.h"
#include "models/FilterPresetStore.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

const QString kRemoteTransmitReason =
    QStringLiteral("Remote transmit controls are not available from this Core.");

QList<QAction*> actionsByText(const QObject* root, const QString& text)
{
    QList<QAction*> found;
    for (QAction* action : root->findChildren<QAction*>()) {
        if (action->text() == text) { found.append(action); }
    }
    return found;
}

bool toastShown(const QWidget* window, const QString& message)
{
    for (StatusToast* toast : window->findChildren<StatusToast*>()) {
        if (toast->message() == message) { return true; }
    }
    return false;
}

QAction* actionByText(const QObject* root, const QString& text)
{
    const QList<QAction*> found = actionsByText(root, text);
    return found.isEmpty() ? nullptr : found.first();
}

StationStartupSelection remoteCore()
{
    // Never dialled: replace(..., false) builds the remote window without
    // starting its connection.
    return {{QStringLiteral("ws://127.0.0.1:4433"), {}, {}, true}, QStringLiteral("core")};
}

// The Setup dialogs open under `window`, newest last.
QList<SetupDialog*> setupDialogs(QWidget* window)
{
    return window->findChildren<SetupDialog*>();
}

QString currentSetupPage(SetupDialog* dialog)
{
    auto* tree = dialog ? dialog->findChild<QTreeWidget*>() : nullptr;
    return tree && tree->currentItem() ? tree->currentItem()->text(0) : QString();
}

void closeSetupDialogs(QWidget* window)
{
    for (SetupDialog* dialog : setupDialogs(window)) {
        delete dialog;
    }
}

template <typename T>
T* childByAccessibleName(const QObject* root, const QString& name)
{
    for (T* w : root->findChildren<T*>()) {
        if (w->accessibleName() == name) { return w; }
    }
    return nullptr;
}

SliceModel* ensureSlice(RadioModel* model)
{
    if (model->activeSlice() == nullptr) {
        model->addSlice();
        model->setActiveSlice(0);
    }
    return model->activeSlice();
}

// Opens `widget`'s context menu and triggers the entry whose text starts
// with `prefix`; returns whether it was there and enabled.
bool chooseFromContextMenu(QWidget* widget, const QString& prefix, bool* enabled)
{
    bool found = false;
    *enabled = false;
    QTimer poll;
    poll.setInterval(10);
    QObject::connect(&poll, &QTimer::timeout, &poll, [&] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu) { return; }
        poll.stop();
        for (QAction* action : menu->actions()) {
            if (action->text().startsWith(prefix)) {
                found = true;
                *enabled = action->isEnabled();
                if (action->isEnabled()) {
                    menu->setActiveAction(action);
                    QTest::keyClick(menu, Qt::Key_Return);
                    return;
                }
            }
        }
        menu->close();
    });
    poll.start();
    const QPoint pos = widget->rect().center();
    QContextMenuEvent event(QContextMenuEvent::Mouse, pos, widget->mapToGlobal(pos));
    QApplication::sendEvent(widget, &event);
    poll.stop();
    return found;
}

} // namespace

// Records the URLs QDesktopServices hands it, instead of a browser.
class UrlCatcher : public QObject {
    Q_OBJECT
public:
    QList<QUrl> urls;
public slots:
    void open(const QUrl& url) { urls.append(url); }
};

class TstControlsConnected : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("controls-connected-%1")
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

    // File > Profiles, Tools > VAX Audio: each opens the Setup page that
    // does the job, in a local and a remote window (Setup applies its own
    // remote gating).
    void menuEntriesOpenTheirSetupPages()
    {
        const QList<std::pair<QString, QString>> entries = {
            {QStringLiteral("&TX Profiles..."), QStringLiteral("TX Profile")},
            {QStringLiteral("&Mic Profiles..."), QStringLiteral("TX Profile")},
            {QStringLiteral("&Import..."), QStringLiteral("Export / Import")},
            {QStringLiteral("&Export..."), QStringLiteral("Export / Import")},
            {QStringLiteral("&VAX Audio..."), QStringLiteral("VAX")},
            {QStringLiteral("&Antenna Setup…"), QStringLiteral("Hardware Config")},
        };
        GuiSessionCoordinator sessions;
        for (bool remote : {false, true}) {
            QVERIFY(remote ? sessions.replace(remoteCore(), false)
                           : sessions.replace({}, false));
            MainWindow* window = sessions.window();
            for (const auto& [text, page] : entries) {
                QAction* action = actionByText(window, text);
                QVERIFY2(action != nullptr, qPrintable(text));
                QVERIFY2(action->isEnabled(), qPrintable(text + QStringLiteral(" is greyed")));
                QVERIFY2(!action->toolTip().contains(QStringLiteral("NYI")), qPrintable(text));
                closeSetupDialogs(window);
                action->trigger();
                const QList<SetupDialog*> dialogs = setupDialogs(window);
                QVERIFY2(dialogs.size() == 1, qPrintable(text + QStringLiteral(" opened no Setup")));
                QCOMPARE(currentSetupPage(dialogs.first()), page);
                closeSetupDialogs(window);
            }
        }
        QVERIFY(sessions.replace({}, false));
    }

    // Radio > Antenna Setup lands on the Antenna / ALEX tab when the radio
    // has one.
    void hardwarePageShowsTheAntennaTab()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::OrionMKII);  // primes the caps pointer
        HardwarePage page(&model);
        RadioInfo info;
        info.boardType = HPSDRHW::OrionMKII;
        info.protocol = ProtocolVersion::Protocol2;
        info.macAddress = QStringLiteral("00:1c:c0:a2:13:dd");
        page.onCurrentRadioChanged(info);
        QVERIFY(page.showAntennaTab());
        QCOMPARE(page.currentTabText(), QStringLiteral("Antenna / ALEX"));
    }

    // DSP > Diversity and Tools > Diversity open the one Diversity dialog.
    void bothDiversityEntriesOpenTheDialog()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* window = sessions.window();
        const QList<QAction*> entries = actionsByText(window, QStringLiteral("&Diversity..."));
        QCOMPARE(entries.size(), 2);
        for (QAction* action : entries) {
            QVERIFY2(action->isEnabled(), "a Diversity entry is greyed");
            for (DiversityDialog* dialog : window->findChildren<DiversityDialog*>()) {
                dialog->hide();
            }
            action->trigger();
            const QList<DiversityDialog*> dialogs = window->findChildren<DiversityDialog*>();
            QCOMPARE(dialogs.size(), 1);
            QVERIFY(dialogs.first()->isVisible());
        }
        QVERIFY(sessions.replace({}, false));
    }

    // The VFO flag's right-click Diversity entry asks for the dialog.
    void vfoFlagDiversityEntryAsksForTheDialog()
    {
        VfoWidget flag;
        flag.resize(260, 120);
        flag.show();
        QSignalSpy spy(&flag, SIGNAL(diversityRequested()));
        QVERIFY2(spy.isValid(), "the flag has no Diversity request");
        bool enabled = false;
        QVERIFY(chooseFromContextMenu(&flag, QStringLiteral("Diversity"), &enabled));
        QVERIFY2(enabled, "the Diversity entry is greyed");
        QCOMPARE(spy.count(), 1);
    }

    // Band > GEN and Band > WWV go through the band buttons' path, so each
    // band's saved frequency comes back.
    void bandMenuGenAndWwvRestoreTheBandMemory()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* window = sessions.window();
        RadioModel* model = window->radioModel();
        SliceModel* slice = ensureSlice(model);
        QVERIFY(slice != nullptr);

        slice->setFrequency(5000000.0);   // WWV 5 MHz
        QVERIFY(bandFromFrequency(slice->frequency()) == Band::WWV);
        model->onBandButtonClicked(Band::Band20m);
        // Band entries are named by band only; each restores the band's
        // saved frequency, so no entry lists a frequency.
        QMenu* bandMenu = nullptr;
        for (QMenu* m : window->findChildren<QMenu*>()) {
            if (m->title() == QStringLiteral("&Band")) { bandMenu = m; }
        }
        QVERIFY(bandMenu != nullptr);
        QList<QMenu*> menus{bandMenu};
        int entries = 0;
        while (!menus.isEmpty()) {
            QMenu* m = menus.takeFirst();
            for (QAction* entry : m->actions()) {
                ++entries;
                QVERIFY2(!entry->text().contains(QStringLiteral("MHz")),
                         qPrintable(entry->text()));
                if (entry->menu()) { menus << entry->menu(); }
            }
        }
        QVERIFY(entries > 11);
        QAction* wwv = actionByText(window, QStringLiteral("&WWV"));
        QVERIFY(wwv != nullptr);
        wwv->trigger();
        QCOMPARE(slice->frequency(), 5000000.0);

        model->onBandButtonClicked(Band::GEN);
        slice->setFrequency(909000.0);
        QVERIFY(bandFromFrequency(slice->frequency()) == Band::GEN);
        model->onBandButtonClicked(Band::Band20m);
        QAction* gen = actionByText(window, QStringLiteral("&GEN"));
        QVERIFY(gen != nullptr);
        QVERIFY2(gen->menu() == nullptr, "Band > GEN is still an empty submenu");
        gen->trigger();
        QCOMPARE(slice->frequency(), 909000.0);
        QVERIFY(sessions.replace({}, false));
    }

    // Help > What's New opens the release notes the About dialog links to.
    void whatsNewOpensTheReleaseNotes()
    {
        UrlCatcher catcher;
        QDesktopServices::setUrlHandler(QStringLiteral("https"), &catcher, "open");
        const auto restore = qScopeGuard([] {
            QDesktopServices::unsetUrlHandler(QStringLiteral("https"));
        });
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        QAction* whatsNew = actionByText(sessions.window(), QStringLiteral("What's &New"));
        QVERIFY(whatsNew != nullptr);
        QVERIFY(whatsNew->isEnabled());
        whatsNew->trigger();
        QCOMPARE(catcher.urls.size(), 1);
        QCOMPARE(catcher.urls.first(),
                 QUrl(QStringLiteral("https://github.com/boydsoftprez/NereusSDR/releases")));
        QVERIFY(sessions.replace({}, false));
    }

    // Phone/CW applet: compression gauge, mic profile, mic source and AM
    // carrier reach the transmit settings in a local window.
    void phoneCwControlsReachTheTransmitSettings()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* window = sessions.window();
        RadioModel* model = window->radioModel();
        TransmitModel& tx = model->transmitModel();
        auto* applet = window->findChild<PhoneCwApplet*>();
        QVERIFY(applet != nullptr);

        // Compression: the meters' Compression reading reaches the gauge.
        auto* gauge = childByAccessibleName<HGauge>(applet, QStringLiteral("Compression gauge"));
        QVERIFY(gauge != nullptr);
        QVERIFY2(gauge->isEnabled(), "the compression gauge is greyed");
        QVERIFY(model->meterPoller() != nullptr);
        QVERIFY2(!gauge->isReversed(), "the compression gauge is drawn reversed");
        // At rest: an empty gauge.
        QCOMPARE(gauge->filledFraction(), 0.0);
        const auto feed = [model](double raw) {
            // What pollTxMeters() hands out for a raw TXA_COMP_AV value.
            return QMetaObject::invokeMethod(model->meterPoller(), "txMeterReading",
                                             Q_ARG(int, MeterBinding::TxComp),
                                             Q_ARG(double, MeterPoller::compressionReading(raw)));
        };
        // PROC off: WDSP's -400 reads -30; the gauge stays empty.
        QVERIFY2(feed(-400.0), "the meter poller hands out no transmit readings");
        QCOMPARE(gauge->filledFraction(), 0.0);
        // A -10 dB compressed level fills 15 of the 25 dB face.
        QVERIFY(feed(-10.0));
        QCOMPARE(gauge->value(), -10.0);
        QVERIFY(qAbs(gauge->filledFraction() - 0.6) < 1e-9);
        // Receive empties it again.
        tx.setMox(true);
        tx.setMox(false);
        QCOMPARE(gauge->filledFraction(), 0.0);

        // Mic profile: the MicProfileManager list, and a pick applies it.
        auto* profile = childByAccessibleName<QComboBox>(applet, QStringLiteral("Microphone profile"));
        QVERIFY(profile != nullptr);
        QVERIFY2(profile->isEnabled(), "the mic profile combo is greyed");
        MicProfileManager* mgr = model->micProfileManager();
        QVERIFY(mgr != nullptr);
        mgr->setMacAddress(QStringLiteral("00:1c:c0:a2:13:dd"));
        mgr->load();
        QStringList shown;
        for (int i = 0; i < profile->count(); ++i) { shown << profile->itemText(i); }
        QCOMPARE(shown, mgr->profileNames());
        QVERIFY(shown.size() > 1);
        const QString other = shown.first() == mgr->activeProfileName() ? shown.at(1) : shown.first();
        profile->setCurrentIndex(profile->findText(other));
        QCOMPARE(mgr->activeProfileName(), other);

        // Mic source: PC picks the PC microphone; a model change shows.
        auto* source = childByAccessibleName<QComboBox>(applet, QStringLiteral("Microphone source"));
        QVERIFY(source != nullptr);
        QVERIFY2(source->isEnabled(), "the mic source combo is greyed");
        tx.setMicSource(MicSource::Vax);
        source->setCurrentIndex(static_cast<int>(PhoneCwApplet::MicInput::Pc));
        emit source->activated(static_cast<int>(PhoneCwApplet::MicInput::Pc));
        QCOMPARE(tx.micSource(), MicSource::Pc);
        auto* items = qobject_cast<QStandardItemModel*>(source->model());
        QVERIFY(items != nullptr);
        QStandardItem* acc = items->item(static_cast<int>(PhoneCwApplet::MicInput::Accessory));
        QVERIFY(!acc->isEnabled());
        QVERIFY(!acc->toolTip().isEmpty());

        // AM carrier: both ways.
        auto* carrier = childByAccessibleName<QSlider>(applet, QStringLiteral("AM carrier level"));
        QVERIFY(carrier != nullptr);
        QVERIFY2(carrier->isEnabled(), "the AM carrier slider is greyed");
        carrier->setValue(40);
        QCOMPARE(tx.amCarrierLevel(), 40);
        tx.setAmCarrierLevel(70);
        QCOMPARE(carrier->value(), 70);
        QVERIFY(sessions.replace({}, false));
    }

    // In a remote window without transmit permission they show its reason,
    // as the applet's other transmit controls do.
    void phoneCwControlsFollowTheRemoteTransmitGate()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace(remoteCore(), false));
        auto* applet = sessions.window()->findChild<PhoneCwApplet*>();
        QVERIFY(applet != nullptr);
        auto* source = childByAccessibleName<QComboBox>(applet,
                                                        QStringLiteral("Microphone source"));
        QVERIFY(source != nullptr);
        QVERIFY(!source->isEnabled());
        QCOMPARE(source->toolTip(), kRemoteTransmitReason);
        // R-R3-49 (parity Task 3): the mic profile is a transmit setting too
        // (transmitSettingsVersion 3); with no Core taking them it says so.
        auto* profile = childByAccessibleName<QComboBox>(applet,
                                                         QStringLiteral("Microphone profile"));
        QVERIFY(profile != nullptr);
        QVERIFY(!profile->isEnabled());
        QCOMPARE(profile->toolTip(), IStationLink::transmitSettingsUnavailableReason());
        // R-R3-49 (parity Task 2): the AM carrier is a transmit setting; with
        // no Core taking them it says why in the transmit settings' words.
        auto* carrier = childByAccessibleName<QSlider>(applet, QStringLiteral("AM carrier level"));
        QVERIFY(carrier != nullptr);
        QVERIFY(!carrier->isEnabled());
        QCOMPARE(carrier->toolTip(), IStationLink::transmitSettingsUnavailableReason());
        QVERIFY(sessions.replace({}, false));
    }

    // A container's Mode, Filter, Antenna, Tune Step and VFO display act on
    // the active slice and show its state.
    void containerControlsActOnTheActiveSlice()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* window = sessions.window();
        RadioModel* model = window->radioModel();
        SliceModel* slice = ensureSlice(model);
        QVERIFY(slice != nullptr);

        ContainerManager* cm = model->containerManager();
        ContainerWidget* container = cm->createContainer(1, DockMode::OverlayDocked);
        QVERIFY(container != nullptr);
        auto* meter = new MeterWidget();
        container->setContent(meter);
        auto* mode = new ModeButtonItem();
        auto* filter = new FilterButtonItem();
        auto* step = new TuneStepButtonItem();
        auto* ant = new AntennaButtonItem();
        auto* vfo = new VfoDisplayItem();
        for (MeterItem* item : std::initializer_list<MeterItem*>{mode, filter, step, ant, vfo}) {
            meter->addItem(item);
        }

        slice->setFrequency(14074000.0);
        emit container->modeClicked(1);   // USB
        QCOMPARE(slice->dspMode(), DSPMode::USB);
        QCOMPARE(mode->activeMode(), 1);

        const QList<FilterPreset> presets =
            model->filterPresetStore()->presetsForMode(DSPMode::USB);
        QVERIFY(presets.size() > 2);
        emit container->filterClicked(2);
        QCOMPARE(slice->filterLow(), presets[2].low);
        QCOMPARE(slice->filterHigh(), presets[2].high);
        QCOMPARE(filter->activeFilter(), 2);

        emit container->tuneStepSelected(3);  // 1k
        QCOMPARE(slice->stepHz(), 1000);
        QCOMPARE(step->activeStep(), 3);

        emit container->antennaSelected(1);   // ANT2
        QCOMPARE(slice->rxAntenna(), QStringLiteral("ANT2"));

        emit container->frequencyChangeRequested(1000);
        QCOMPARE(slice->frequency(), 14075000.0);
        QCOMPARE(vfo->frequency(), int64_t{14075000});

        // A locked slice keeps its mode, as Thetis's mode buttons do.
        slice->setLocked(true);
        emit container->modeClicked(0);
        QCOMPARE(slice->dspMode(), DSPMode::USB);
        slice->setLocked(false);

        // Tuning moves the VFO display only; it does not relabel or
        // re-highlight the buttons.
        filter->setFilterLabel(0, QStringLiteral("mark"));
        mode->setActiveMode(5);
        slice->setFrequency(7074000.0);
        QCOMPARE(vfo->frequency(), int64_t{7074000});
        QCOMPARE(filter->filterLabel(0), QStringLiteral("mark"));
        QCOMPARE(mode->activeMode(), 5);
        slice->setDspMode(DSPMode::LSB);  // a full refresh puts them right
        QCOMPARE(filter->filterLabel(0),
                 model->filterPresetStore()->presetsForMode(DSPMode::LSB)[0].name);

        // A mode with fewer presets (FM has 3) leaves the rest blank, and
        // a click there changes nothing.
        QCOMPARE(filter->filterLabel(9), presets[9].name);
        slice->setDspMode(DSPMode::FM);
        const QList<FilterPreset> fm =
            model->filterPresetStore()->presetsForMode(DSPMode::FM);
        QCOMPARE(fm.size(), 3);
        QCOMPARE(filter->filterLabel(2), fm[2].name);
        for (int i = 3; i < 10; ++i) {
            QVERIFY2(filter->filterLabel(i).isEmpty(),
                     qPrintable(QStringLiteral("F%1 still shows %2")
                                    .arg(i + 1).arg(filter->filterLabel(i))));
        }
        const int low = slice->filterLow();
        const int high = slice->filterHigh();
        emit container->filterClicked(5);
        QCOMPARE(slice->filterLow(), low);
        QCOMPARE(slice->filterHigh(), high);
        QVERIFY(sessions.replace({}, false));
    }

    // A container's filter right-click offers what the VFO flag's and RX
    // applet's filter buttons offer: edit or reset the preset it names.
    // The VFO display's names the slice's current preset.
    void containerFilterRightClickEditsItsPreset()
    {
        QCOMPARE(MainWindow::containerFilterContextSlot(2, -1, 10), 2);
        QCOMPARE(MainWindow::containerFilterContextSlot(-1, 4, 10), 4);
        QCOMPARE(MainWindow::containerFilterContextSlot(-1, -1, 10), -1);
        QCOMPARE(MainWindow::containerFilterContextSlot(5, 0, 3), -1);

        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* window = sessions.window();
        RadioModel* model = window->radioModel();
        SliceModel* slice = ensureSlice(model);
        QVERIFY(slice != nullptr);
        ContainerManager* cm = model->containerManager();
        ContainerWidget* container = cm->createContainer(1, DockMode::OverlayDocked);
        QVERIFY(container != nullptr);
        auto* meter = new MeterWidget();
        container->setContent(meter);
        meter->addItem(new FilterButtonItem());
        slice->setDspMode(DSPMode::USB);
        FilterPresetStore* store = model->filterPresetStore();
        const FilterPreset original = store->presetsForMode(DSPMode::USB)[2];
        FilterPreset edited = original;
        edited.high = original.high + 300;
        store->setPreset(DSPMode::USB, 2, edited);
        QCOMPARE(store->presetsForMode(DSPMode::USB)[2].high, original.high + 300);

        // Right-click on F3: the menu's reset puts the preset back.
        QStringList seen;
        const auto answer = [&seen](const QString& pick) {
            auto* poll = new QTimer;
            poll->setInterval(10);
            QObject::connect(poll, &QTimer::timeout, poll, [poll, &seen, pick] {
                auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                if (!menu) { return; }
                poll->stop();
                poll->deleteLater();
                seen.clear();
                for (QAction* action : menu->actions()) {
                    seen << action->text();
                }
                for (QAction* action : menu->actions()) {
                    if (action->text() == pick) {
                        menu->setActiveAction(action);
                        QTest::keyClick(menu, Qt::Key_Return);
                        return;
                    }
                }
                menu->close();
            });
            poll->start();
        };
        // Let the new container's deferred show and activation settle: a
        // popup closes when the active window changes under it.
        QTest::qWait(300);
        answer(QStringLiteral("Reset this preset"));
        emit container->filterContextRequested(2);
        QCOMPARE(seen, (QStringList{QStringLiteral("Edit this preset…"),
                                    QStringLiteral("Reset this preset")}));
        QCOMPARE(store->presetsForMode(DSPMode::USB)[2].high, original.high);

        // The VFO display's right-click: the slice's current preset.
        slice->setFilter(store->presetsForMode(DSPMode::USB)[4].low,
                         store->presetsForMode(DSPMode::USB)[4].high);
        FilterPreset edited4 = store->presetsForMode(DSPMode::USB)[4];
        const int high4 = edited4.high;
        edited4.high += 200;
        store->setPreset(DSPMode::USB, 4, edited4);
        slice->setFilter(edited4.low, edited4.high);
        answer(QStringLiteral("Reset this preset"));
        emit container->vfoFilterContextRequested();
        QCOMPARE(store->presetsForMode(DSPMode::USB)[4].high, high4);

        // A filter that is no preset: no menu, only the reason.
        seen.clear();
        slice->setFilter(1234, 1500);
        answer(QString());  // closes a menu, should one open
        emit container->vfoFilterContextRequested();
        QVERIFY2(seen.isEmpty(), qPrintable(seen.join(QLatin1Char(','))));
        QVERIFY(QApplication::activePopupWidget() == nullptr);
        QVERIFY(toastShown(window, QStringLiteral(
            "This slice's filter is not one of this mode's presets, so there is no "
            "preset to edit.")));

        // Band stacking is not built: a band button's or the VFO display's
        // band-stack right-click says so rather than doing nothing.
        qDeleteAll(window->findChildren<StatusToast*>());
        emit container->bandStackRequested(5);
        QVERIFY(toastShown(window, MainWindow::containerBandStackReason()));
        QCOMPARE(MainWindow::containerBandStackReason(),
                 QStringLiteral("Band stacking is not ready."));
        store->resetPreset(DSPMode::USB, 2);
        store->resetPreset(DSPMode::USB, 4);
        QVERIFY(sessions.replace({}, false));
    }

    // Parity ruling C9: a remote window's CPU row names its source; with no
    // Core reading it shows this computer's CPU and says why. A local
    // window's row is unchanged.
    void remoteCpuRowShowsThisComputerWithTheReason()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace(remoteCore(), false));
        auto* tile = sessions.window()->findChild<SystemTile*>();
        QVERIFY(tile != nullptr);
        QTRY_COMPARE(tile->cpuRowToolTip(), QStringLiteral("The Core's CPU reading is not current."));
        QCOMPARE(tile->cpuRowLabel(), QStringLiteral("CPU"));
        QVERIFY(sessions.replace({}, false));
        auto* local = sessions.window()->findChild<SystemTile*>();
        QVERIFY(local != nullptr);
        QTest::qWait(1200);
        QCOMPARE(local->cpuRowLabel(), QStringLiteral("CPU"));
        QVERIFY(local->cpuRowToolTip().isEmpty());
        QVERIFY(sessions.replace({}, false));
    }

    // A meter item added while the window runs gets the saved Multimeter
    // unit, decimal and history duration, the saved high-resolution filter
    // graph, and the active slice's state, without opening Setup.
    void addedMeterItemsGetTheSavedSettings()
    {
        auto& s = AppSettings::instance();
        const QStringList keys = {QStringLiteral("MultimeterUnitMode"),
                                  QStringLiteral("MultimeterShowDecimal"),
                                  QStringLiteral("MultimeterSignalHistoryDurationMs"),
                                  QStringLiteral("DspOptionsHighResFilterCharacteristics")};
        const auto restore = qScopeGuard([&s, keys] {
            for (const QString& k : keys) { s.remove(k); }
        });
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* window = sessions.window();
        RadioModel* model = window->radioModel();
        SliceModel* slice = ensureSlice(model);
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::USB);

        ContainerManager* cm = model->containerManager();
        ContainerWidget* container = cm->createContainer(1, DockMode::OverlayDocked);
        QVERIFY(container != nullptr);
        auto* meter = new MeterWidget();
        container->setContent(meter);

        s.setValue(QStringLiteral("MultimeterUnitMode"), QStringLiteral("S"));
        s.setValue(QStringLiteral("MultimeterShowDecimal"), QStringLiteral("False"));
        s.setValue(QStringLiteral("MultimeterSignalHistoryDurationMs"), 30000);
        s.setValue(QStringLiteral("DspOptionsHighResFilterCharacteristics"), QStringLiteral("True"));

        auto* bar = new BarItem();
        auto* history = new HistoryGraphItem();
        auto* graph = new FilterDisplayItem();
        auto* mode = new ModeButtonItem();
        for (MeterItem* item : std::initializer_list<MeterItem*>{bar, history, graph, mode}) {
            meter->addItem(item);
        }
        QCOMPARE(bar->unitMode(), MeterItem::MeterUnit::S);
        QVERIFY(!bar->showDecimal());
        QCOMPARE(history->durationMs(), 30000);
        QVERIFY(graph->highResolution());
        QCOMPARE(mode->activeMode(), 1);  // USB
        QVERIFY(sessions.replace({}, false));
    }

    // The pan overlay's ATT opens the step attenuator; locally it changes
    // it, remotely (not connected) it says why it cannot.
    void overlayAttOpensTheStepAttenuator()
    {
        GuiSessionCoordinator sessions;
        for (bool remote : {false, true}) {
            QVERIFY(remote ? sessions.replace(remoteCore(), false)
                           : sessions.replace({}, false));
            MainWindow* window = sessions.window();
            window->resize(1400, 900);
            window->show();
            auto* panel = window->findChild<SpectrumOverlayPanel*>();
            QVERIFY(panel != nullptr);
            QPushButton* att = nullptr;
            for (QPushButton* b : panel->findChildren<QPushButton*>()) {
                if (b->text() == QStringLiteral("ATT")) { att = b; }
            }
            QVERIFY(att != nullptr);
            QVERIFY2(att->isEnabled(), "the ATT button is greyed");
            QVERIFY(!att->toolTip().contains(QStringLiteral("NYI")));
            att->click();
            auto* flyout = panel->parentWidget()->findChild<QWidget*>(QStringLiteral("attFlyout"));
            QVERIFY(flyout != nullptr);
            QVERIFY(flyout->isVisible());
            auto* enable = flyout->findChild<QCheckBox*>(QStringLiteral("attEnableCheck"));
            auto* spin = flyout->findChild<QSpinBox*>(QStringLiteral("attSpin"));
            QVERIFY(enable != nullptr);
            QVERIFY(spin != nullptr);
            StepAttenuatorFacade* facade = window->radioModel()->stepAttFacade();
            QVERIFY(facade != nullptr);
            if (!remote) {
                QVERIFY(enable->isEnabled());
                enable->setChecked(true);
                QVERIFY(facade->enabled());
                QVERIFY(spin->isEnabled());
                spin->setValue(spin->minimum() + 10);
                QCOMPARE(facade->attenuationDb(), spin->minimum() + 10);
            } else {
                QVERIFY(!enable->isEnabled());
                auto* reason = flyout->findChild<QLabel*>(QStringLiteral("attReason"));
                QVERIFY(reason != nullptr);
                QVERIFY(reason->isVisible());
                QCOMPARE(reason->text(), facade->windowUnavailableReason());
            }
        }
        QVERIFY(sessions.replace({}, false));
    }

    // The zoom buttons zoom the pan they are drawn on.
    void overlayZoomButtonsZoomTheirPan()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* window = sessions.window();
        auto* panel = window->findChild<SpectrumOverlayPanel*>();
        QVERIFY(panel != nullptr);
        auto* spectrum = qobject_cast<SpectrumWidget*>(panel->parentWidget());
        QVERIFY(spectrum != nullptr);
        spectrum->setSampleRate(1536000.0);
        spectrum->setFrequencyRange(14100000.0, 96000.0);
        spectrum->setVfoFrequency(14074000.0);

        emit panel->zoomOut();
        QCOMPARE(spectrum->bandwidth(), 144000.0);
        emit panel->zoomIn();
        QCOMPARE(spectrum->bandwidth(), 96000.0);

        emit panel->zoomBand();
        const double bandBw = spectrum->bandwidth();
        QVERIFY2(bandBw >= 300000.0, qPrintable(QString::number(bandBw)));
        QVERIFY(spectrum->centerFrequency() - bandBw / 2 <= 14074000.0);
        QVERIFY(spectrum->centerFrequency() + bandBw / 2 >= 14074000.0);

        emit panel->zoomSegment();
        const double segBw = spectrum->bandwidth();
        QVERIFY(segBw < bandBw);
        QVERIFY(spectrum->centerFrequency() - segBw / 2 <= 14074000.0);
        QVERIFY(spectrum->centerFrequency() + segBw / 2 >= 14074000.0);
        QVERIFY(sessions.replace({}, false));
    }

    // Status badges: the dashboard's open the flag tab holding the
    // setting; the rest have nothing to open and keep the arrow. The
    // dashboard's open something only while the window has a slice to
    // describe: with none (a fresh window) they are cleared and inert, as
    // "Clear RX banner when the desktop has no owned slice" (9925ed9a8)
    // made them, so they keep the arrow until a slice is active.
    void badgesShowTheHandOnlyWhereAClickOpensSomething()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* window = sessions.window();
        for (const char* name : {"paStatusBadge", "txStatusBadge"}) {
            auto* badge = window->findChild<StatusBadge*>(QLatin1String(name));
            QVERIFY2(badge != nullptr, name);
            QVERIFY2(badge->cursor().shape() != Qt::PointingHandCursor, name);
        }
        auto* dash = window->findChild<RxDashboard*>();
        QVERIFY(dash != nullptr);
        const QList<StatusBadge*> badges = dash->findChildren<StatusBadge*>();
        QCOMPARE(badges.size(), 7);
        QSignalSpy spy(dash, SIGNAL(badgeClicked(NereusSDR::RxDashboard::Badge)));
        QVERIFY2(spy.isValid(), "the dashboard reports no badge clicks");

        // No slice: nothing to open, the arrow, and a click does nothing.
        QVERIFY(window->radioModel()->activeSlice() == nullptr);
        QVERIFY(dash->slice() == nullptr);
        for (StatusBadge* badge : badges) {
            QCOMPARE(badge->cursor().shape(), Qt::ArrowCursor);
        }
        QTest::mouseClick(badges.first(), Qt::LeftButton);
        QCOMPARE(spy.count(), 0);

        // A slice: every dashboard badge opens its flag tab.
        SliceModel* slice = ensureSlice(window->radioModel());
        QVERIFY(slice != nullptr);
        QTRY_COMPARE(dash->slice(), slice);
        for (StatusBadge* badge : badges) {
            QCOMPARE(badge->cursor().shape(), Qt::PointingHandCursor);
        }
        QTest::mouseClick(badges.first(), Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
        QVERIFY(sessions.replace({}, false));
    }

    // Setup > Multimeter's update interval applies at startup, like the
    // four Multimeter values Task 1 fixed.
    void savedMeterIntervalAppliesAtStartup()
    {
        AppSettings::instance().setValue(QStringLiteral("MultimeterDelayMs"), 250);
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MeterPoller* poller = sessions.window()->radioModel()->meterPoller();
        QVERIFY(poller != nullptr);
        QCOMPARE(poller->intervalMs(), 250);
        QVERIFY(sessions.replace({}, false));
    }

    // Radio Status's "Seq gaps (EP6)" shows the EP6 sequence error count.
    void radioStatusShowsTheSequenceGapCount()
    {
        RadioModel model;
        for (int i = 0; i < 3; ++i) {
            model.bwMonitorMutable().recordEp6SequenceError();
        }
        RadioStatusPage page(&model);
        QLabel* caption = nullptr;
        for (QLabel* label : page.findChildren<QLabel*>()) {
            if (label->text() == QStringLiteral("Seq gaps (EP6)")) { caption = label; }
        }
        QVERIFY(caption != nullptr);
        // The value is the next label its row adds (RadioStatusPage makeRow).
        QLabel* value = nullptr;
        const QObjectList siblings = caption->parent()->children();
        for (qsizetype i = siblings.indexOf(caption) + 1; i < siblings.size(); ++i) {
            if (auto* label = qobject_cast<QLabel*>(siblings.at(i))) { value = label; break; }
        }
        QVERIFY(value != nullptr);
        QCOMPARE(value->text(), QStringLiteral("3"));
    }

    // The words these controls now show are plain operator words.
    void connectedControlsUsePlainWords()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* window = sessions.window();
        QStringList texts;
        for (const QString& text : {QStringLiteral("&TX Profiles..."),
                                    QStringLiteral("&Mic Profiles..."),
                                    QStringLiteral("&Import..."), QStringLiteral("&Export..."),
                                    QStringLiteral("&Antenna Setup…"),
                                    QStringLiteral("&Diversity..."), QStringLiteral("&GEN"),
                                    QStringLiteral("&VAX Audio..."),
                                    QStringLiteral("What's &New")}) {
            for (QAction* action : actionsByText(window, text)) {
                texts << action->toolTip();
            }
        }
        auto* applet = window->findChild<PhoneCwApplet*>();
        QVERIFY(applet != nullptr);
        for (const QString& name : {QStringLiteral("Compression gauge"),
                                    QStringLiteral("Microphone profile"),
                                    QStringLiteral("Microphone source"),
                                    QStringLiteral("AM carrier level")}) {
            auto* w = childByAccessibleName<QWidget>(applet, name);
            QVERIFY(w != nullptr);
            texts << w->toolTip();
        }
        auto* source = childByAccessibleName<QComboBox>(applet, QStringLiteral("Microphone source"));
        auto* items = qobject_cast<QStandardItemModel*>(source->model());
        for (int i = 0; i < items->rowCount(); ++i) {
            if (!items->item(i)->toolTip().isEmpty()) { texts << items->item(i)->toolTip(); }
        }
        auto* panel = window->findChild<SpectrumOverlayPanel*>();
        QVERIFY(panel != nullptr);
        for (QPushButton* b : panel->findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("ATT") || b->text() == QStringLiteral("VAX")) {
                texts << b->toolTip();
            }
        }
        QWidget* spectrum = panel->parentWidget();
        for (QPushButton* b : spectrum->findChildren<QPushButton*>()) {
            if (QStringList{QStringLiteral("S"), QStringLiteral("B"), QStringLiteral("-"),
                            QStringLiteral("+")}.contains(b->text())) {
                texts << b->toolTip();
            }
        }
        if (auto* flyout = spectrum->findChild<QWidget*>(QStringLiteral("attFlyout"))) {
            for (QWidget* w : flyout->findChildren<QWidget*>()) {
                if (!w->toolTip().isEmpty()) { texts << w->toolTip(); }
            }
        }
        QVERIFY(texts.size() >= 20);
        for (const QString& text : std::as_const(texts)) {
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
            QVERIFY2(!text.contains(QStringLiteral("NYI")), qPrintable(text));
            QVERIFY2(!text.contains(QChar(0x2014)), qPrintable(text));
        }
        QVERIFY(sessions.replace({}, false));
    }

    // The tab a badge asks for opens on the flag.
    void vfoFlagOpensTheAskedTab()
    {
        VfoWidget flag;
        flag.show();
        QCOMPARE(flag.activeTab(), -1);
        flag.showTab(VfoWidget::Tab::Dsp);
        QCOMPARE(flag.activeTab(), static_cast<int>(VfoWidget::Tab::Dsp));
        flag.showTab(VfoWidget::Tab::Dsp);  // stays open
        QCOMPARE(flag.activeTab(), static_cast<int>(VfoWidget::Tab::Dsp));
        flag.showTab(VfoWidget::Tab::Mode);
        QCOMPARE(flag.activeTab(), static_cast<int>(VfoWidget::Tab::Mode));
    }
};

QTEST_MAIN(TstControlsConnected)
#include "tst_controls_connected.moc"
