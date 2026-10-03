// =================================================================
// tests/tst_controls_that_work.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. It drives real local and remote
// MainWindows and Setup pages; no upstream logic is ported here.
//
// R3 controls that work, Task 1 (R-R3-21, R-R3-17): the controls the
// placeholder and dead-control audit found broken in the main window.
//   - No two menu actions or application shortcuts share a key, in a local
//     or a remote window; Ctrl+Shift+K is Radio > Disconnect's alone.
//   - Tools menu test entries exist only in developer builds.
//   - Saved Multimeter and high-resolution filter settings apply at startup.
//   - Band > HF restores the band's memory like the band buttons.
//   - DXCC colouring has its country table from a fresh install.
//   - Connection Quality's "EP6 sequence gaps" shows that count.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  R3 controls that work, Task 1.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QAction>
#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QMap>
#include <QPointer>
#include <QScopeGuard>
#include <QShortcut>
#include <QWidget>

#include "core/AppSettings.h"
#include "core/BuildIdentity.h"
#include "core/DxccColorProvider.h"
#include "core/DxccWorkedStatus.h"
#include "core/HermesLiteBandwidthMonitor.h"
#include "core/RadioDiscovery.h"
#include "core/WdspTypes.h"
#include "gui/GuiSessionCoordinator.h"
#include "gui/MainWindow.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/diagnostics/DiagnosticsPhaseHPages.h"
#include "gui/meters/FilterDisplayItem.h"
#include "gui/meters/HistoryGraphItem.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// Every key that two or more window- or application-wide QActions and
// QShortcuts under `root` share, one line each. Qt fires neither owner of
// an ambiguous chord (QAction::activated is replaced by
// QShortcut/QAction "ambiguous"), so any line here is a dead shortcut.
QStringList duplicateShortcuts(const QObject* root)
{
    const auto competes = [](Qt::ShortcutContext context) {
        return context == Qt::WindowShortcut || context == Qt::ApplicationShortcut;
    };
    QMap<QString, QStringList> owners;
    for (const QAction* action : root->findChildren<QAction*>()) {
        if (!competes(action->shortcutContext())) { continue; }
        const QString name = action->text().isEmpty() ? action->objectName() : action->text();
        for (const QKeySequence& seq : action->shortcuts()) {
            if (!seq.isEmpty()) {
                owners[seq.toString(QKeySequence::PortableText)].append(name);
            }
        }
    }
    for (const QShortcut* shortcut : root->findChildren<QShortcut*>()) {
        if (!competes(shortcut->context())) { continue; }
        const QString name = shortcut->objectName().isEmpty()
            ? QStringLiteral("(shortcut)") : shortcut->objectName();
        for (const QKeySequence& seq : shortcut->keys()) {
            if (!seq.isEmpty()) {
                owners[seq.toString(QKeySequence::PortableText)].append(name);
            }
        }
    }
    QStringList duplicates;
    for (auto it = owners.constBegin(); it != owners.constEnd(); ++it) {
        if (it.value().size() > 1) {
            duplicates.append(it.key() + QStringLiteral(": ")
                              + it.value().join(QStringLiteral(", ")));
        }
    }
    return duplicates;
}

QAction* actionByText(const QObject* root, const QString& text)
{
    for (QAction* action : root->findChildren<QAction*>()) {
        if (action->text() == text) { return action; }
    }
    return nullptr;
}

StationStartupSelection remoteCore()
{
    // Never dialled: replace(..., false) builds the remote window without
    // starting its connection.
    return {{QStringLiteral("ws://127.0.0.1:4433"), {}, {}, true}, QStringLiteral("core")};
}

MeterWidget* meterIn(ContainerWidget* container)
{
    if (container == nullptr) { return nullptr; }
    if (auto* meter = qobject_cast<MeterWidget*>(container->content())) { return meter; }
    return container->findChild<MeterWidget*>();
}

} // namespace

class TstControlsThatWork : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Windows save their settings; this run keeps a file of its own.
        AppSettings::setProfileOverride(QStringLiteral("controls-that-work-%1")
                                            .arg(QCoreApplication::applicationPid()));
    }

    void init()
    {
        QVERIFY(!AppSettings::instance().remoteBackend());
        AppSettings::instance().clear();
        // No VAX first-run dialog and no discovery broadcast onto the LAN
        // from the local windows (the tst_gui_session_coordinator setup).
        AppSettings::instance().setValue(QStringLiteral("audio/FirstRunComplete"),
                                         QStringLiteral("True"));
        RadioDiscovery::clearHoldOffForTest();
        RadioDiscovery discovery;
        discovery.holdOffScans(std::chrono::minutes{5});
        BuildIdentity::setBuildTag(QString());
    }

    void cleanup()
    {
        BuildIdentity::setBuildTag(QString());
        RadioDiscovery::clearHoldOffForTest();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // The uniqueness check below is only worth something if it fails on a
    // duplicate.
    void uniquenessCheckReportsADuplicate()
    {
        QWidget window;
        auto* first = new QAction(QStringLiteral("First"), &window);
        first->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));
        auto* shortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+K")), &window);
        shortcut->setObjectName(QStringLiteral("second"));
        const QStringList duplicates = duplicateShortcuts(&window);
        QCOMPARE(duplicates.size(), 1);
        QVERIFY2(duplicates.first().contains(QStringLiteral("First"))
                     && duplicates.first().contains(QStringLiteral("second")),
                 qPrintable(duplicates.join(QLatin1Char('\n'))));
    }

    // Ctrl+Shift+K was both Radio > Disconnect and an application-wide
    // clear-spots shortcut, so it did neither. Every shortcut in the local
    // and the remote window now has one owner, in both build kinds.
    void everyShortcutHasOneOwnerInLocalAndRemoteWindows()
    {
        for (const QString& tag : {QString(), QStringLiteral("test@0000000")}) {
            BuildIdentity::setBuildTag(tag);
            GuiSessionCoordinator sessions;
            for (bool remote : {false, true}) {
                QVERIFY(remote ? sessions.replace(remoteCore(), false)
                               : sessions.replace({}, false));
                MainWindow* window = sessions.window();
                QVERIFY(window != nullptr);
                QCOMPARE(window->radioModel()->ownsLocalDsp(), !remote);

                const QStringList duplicates = duplicateShortcuts(window);
                QVERIFY2(duplicates.isEmpty(),
                         qPrintable((remote ? QStringLiteral("remote: ")
                                            : QStringLiteral("local: "))
                                    + duplicates.join(QStringLiteral("; "))));

                QAction* disconnect = actionByText(window, QStringLiteral("&Disconnect"));
                QVERIFY(disconnect != nullptr);
                QCOMPARE(disconnect->shortcut(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));

                auto* clearSpots = window->findChild<QShortcut*>(QStringLiteral("clearSpotsShortcut"));
                QVERIFY(clearSpots != nullptr);
                QVERIFY(clearSpots->key() != disconnect->shortcut());
            }
            QVERIFY(sessions.replace({}, false));
        }
    }

    // Release builds carry no smoke-build tag (BuildIdentity.h) and show no
    // test entries in the Tools menu; developer builds keep them.
    void toolsTestEntriesOnlyInDeveloperBuilds()
    {
        GuiSessionCoordinator sessions;
        for (bool remote : {false, true}) {
            BuildIdentity::setBuildTag(QString());
            QVERIFY(remote ? sessions.replace(remoteCore(), false)
                           : sessions.replace({}, false));
            QVERIFY(sessions.window()->findChild<QAction*>(
                        QStringLiteral("toolsTestAntennaSwitchToast")) == nullptr);
            QVERIFY(sessions.window()->findChild<QAction*>(
                        QStringLiteral("toolsTestTxBoundReRoute")) == nullptr);
            QVERIFY(actionByText(sessions.window(),
                                 QStringLiteral("Test antenna switch &toast")) == nullptr);

            BuildIdentity::setBuildTag(QStringLiteral("codex/controls-that-work@0000000"));
            QVERIFY(remote ? sessions.replace(remoteCore(), false)
                           : sessions.replace({}, false));
            QVERIFY(sessions.window()->findChild<QAction*>(
                        QStringLiteral("toolsTestAntennaSwitchToast")) != nullptr);
            QVERIFY(sessions.window()->findChild<QAction*>(
                        QStringLiteral("toolsTestTxBoundReRoute")) != nullptr);
        }
        QVERIFY(sessions.replace({}, false));
    }

    // Setup > Multimeter's average, decimal, unit and history duration, and
    // DSP > Options' high-resolution filter graph, used to reach the meters
    // only when their Setup page opened. After a restart they apply without
    // opening Setup.
    void savedMeterSettingsApplyAtStartup()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        QString containerId;
        {
            ContainerManager* cm = sessions.window()->radioModel()->containerManager();
            QVERIFY(cm != nullptr);
            ContainerWidget* container = cm->createContainer(1, DockMode::OverlayDocked);
            QVERIFY(container != nullptr);
            container->setContent(new MeterWidget());
            MeterWidget* meter = meterIn(container);
            QVERIFY(meter != nullptr);
            meter->addItem(new FilterDisplayItem());
            meter->addItem(new HistoryGraphItem());
            containerId = container->id();
            cm->saveState();
        }

        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("MultimeterAverageWindow"), 7);
        s.setValue(QStringLiteral("MultimeterUnitMode"), QStringLiteral("S"));
        s.setValue(QStringLiteral("MultimeterShowDecimal"), QStringLiteral("False"));
        s.setValue(QStringLiteral("MultimeterSignalHistoryDurationMs"), 90000);
        s.setValue(QStringLiteral("DspOptionsHighResFilterCharacteristics"), QStringLiteral("True"));

        // The restart: a fresh window restores the saved containers.
        QVERIFY(sessions.replace({}, false));
        RadioModel* model = sessions.window()->radioModel();
        QVERIFY(model->meterPoller() != nullptr);
        QCOMPARE(model->meterPoller()->averageWindow(), 7);

        MeterWidget* meter = meterIn(model->containerManager()->container(containerId));
        QVERIFY2(meter != nullptr, "the saved container did not come back");
        FilterDisplayItem* filter = nullptr;
        HistoryGraphItem* history = nullptr;
        for (MeterItem* item : meter->items()) {
            if (auto* f = qobject_cast<FilterDisplayItem*>(item)) { filter = f; }
            if (auto* h = qobject_cast<HistoryGraphItem*>(item)) { history = h; }
        }
        QVERIFY(filter != nullptr);
        QVERIFY(history != nullptr);
        QVERIFY2(filter->highResolution(), "high-resolution filter setting not applied");
        QCOMPARE(history->durationMs(), 90000);

        int checked = 0;
        model->containerManager()->forEachMeterItem([&checked](MeterItem* item) {
            QVERIFY(item->unitMode() == MeterItem::MeterUnit::S);
            QVERIFY(!item->showDecimal());
            ++checked;
        });
        QVERIFY(checked >= 2);
        QVERIFY(sessions.replace({}, false));
    }

    // Band > HF used to set the listed frequency directly. It now goes
    // through the band buttons' path, so the band's saved frequency and
    // mode come back.
    void bandMenuHfRestoresTheBandMemory()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* window = sessions.window();
        RadioModel* model = window->radioModel();
        if (model->activeSlice() == nullptr) {
            model->addSlice();
            model->setActiveSlice(0);
        }
        SliceModel* slice = model->activeSlice();
        QVERIFY(slice != nullptr);
        const auto clearKeys = qScopeGuard([] {
            auto& s = AppSettings::instance();
            for (const QString& key : s.allKeys()) {
                if (key.contains(QStringLiteral("/Band40m/"))
                    || key.contains(QStringLiteral("/Band20m/"))) {
                    s.remove(key);
                }
            }
        });

        slice->setFrequency(7123000.0);
        slice->setDspMode(DSPMode::LSB);
        // Leaving 40m saves its memory, as a band button does.
        model->onBandButtonClicked(Band::Band20m);
        QVERIFY(bandFromFrequency(slice->frequency()) == Band::Band20m);

        QAction* band40 = actionByText(window, QStringLiteral("40m"));
        QVERIFY(band40 != nullptr);
        band40->trigger();
        QCOMPARE(slice->frequency(), 7123000.0);
        QCOMPARE(slice->dspMode(), DSPMode::LSB);
        QVERIFY(sessions.replace({}, false));
    }

    // cty.dat ships as the ":/cty.dat" resource DxccColorProvider loads by
    // default; before this change nothing bundled it, so every spot
    // resolved to no country.
    void dxccTableLoadsFromTheBundledResource()
    {
        DxccColorProvider provider;
        QVERIFY2(provider.loadCtyDat(), "the app's ':/cty.dat' resource is missing or empty");
        // No log imported: every known country is a new one.
        QVERIFY(provider.statusForSpot(QStringLiteral("JA1ABC"), 14.074, QStringLiteral("FT8"))
                == DxccStatus::NewDxcc);
        QCOMPARE(provider.colorForSpot(QStringLiteral("JA1ABC"), 14.074, QStringLiteral("FT8")),
                 provider.colorNewDxcc);
    }

    // And the window loads it at startup, local and remote.
    void windowsLoadTheDxccTableAtStartup()
    {
        GuiSessionCoordinator sessions;
        for (bool remote : {false, true}) {
            QVERIFY(remote ? sessions.replace(remoteCore(), false)
                           : sessions.replace({}, false));
            DxccColorProvider* dxcc = sessions.window()->radioModel()->dxccColorProvider();
            QVERIFY(dxcc != nullptr);
            QVERIFY(dxcc->statusForSpot(QStringLiteral("G4ABC"), 7.074, QStringLiteral("FT8"))
                    == DxccStatus::NewDxcc);
        }
        QVERIFY(sessions.replace({}, false));
    }

    // "EP6 sequence gaps" showed the LAN throttle event count.
    void connectionQualityShowsTheSequenceGapCount()
    {
        RadioModel model;
        for (int i = 0; i < 3; ++i) {
            model.bwMonitorMutable().recordEp6SequenceError();
        }
        QCOMPARE(model.bwMonitor().throttleEventCount(), 0);

        ConnectionQualityPage page(&model);
        QLabel* caption = nullptr;
        for (QLabel* label : page.findChildren<QLabel*>()) {
            if (label->text() == QStringLiteral("EP6 sequence gaps:")) { caption = label; }
        }
        QVERIFY(caption != nullptr);
        // The value sits beside its caption in the same row.
        QLabel* value = nullptr;
        page.resize(600, 400);
        page.show();
        QCoreApplication::processEvents();
        for (QLabel* label : page.findChildren<QLabel*>()) {
            if (label != caption && label->parentWidget() == caption->parentWidget()
                && qAbs(label->geometry().center().y() - caption->geometry().center().y()) < 4
                && label->geometry().x() > caption->geometry().x()) {
                value = label;
            }
        }
        QVERIFY(value != nullptr);
        QCOMPARE(value->text(), QStringLiteral("3"));
    }
};

QTEST_MAIN(TstControlsThatWork)
#include "tst_controls_that_work.moc"
