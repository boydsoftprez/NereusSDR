// =================================================================
// tests/tst_filter_button_item.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test of the container filter grid;
// it checks the grid's buttons and the click routing, and ports no
// upstream logic itself.
//
// Container item parity, Task 51: the filter grid has 12 buttons on
// every slice.
//   - Every grid has F1-F10, Var1 and Var2 (Thetis gives RX2 only 9);
//     Var1 and Var2 are not drawn while variable filters are not built,
//     and the column range is Thetis's 1-12 on any slice.
//   - The editor has one visibility row per button; the Var rows are
//     disabled with the not-built reason. Saved bits come back as saved,
//     and a record from before the bits loads with every button.
//   - In a window: a click sets the RX filter of the entry's slice only
//     and leaves TransmitModel unchanged; the grid lights from its slice;
//     Var1 changes nothing; an entry on another session's radio changes
//     nothing here.
//   - A preview copy's click never reaches the window's handler.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26  J.J. Boyd / KG4VCF  Container item parity Task 51.
//                                    AI-assisted via Anthropic Claude
//                                    Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QJsonObject>
#include <QSignalSpy>
#include <QSplitter>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/BuildIdentity.h"
#include "core/RadioDiscovery.h"
#include "gui/GuiSessionCoordinator.h"
#include "gui/MainWindow.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerPreviewWidget.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/meter_property_editors/FilterButtonItemEditor.h"
#include "gui/meters/FilterButtonItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "models/FilterPresetStore.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

constexpr int kSize = 600;

SliceModel* ensureSlice(RadioModel* model)
{
    if (model->activeSlice() == nullptr) {
        model->addSlice();
        model->setActiveSlice(0);
    }
    return model->activeSlice();
}

FilterButtonItem* firstGrid(const QList<MeterWidget*>& meters)
{
    for (MeterWidget* meter : meters) {
        for (MeterItem* item : meter->items()) {
            if (auto* filter = qobject_cast<FilterButtonItem*>(item)) { return filter; }
        }
    }
    return nullptr;
}

QStringList shownLabels(const FilterButtonItem& grid)
{
    QStringList shown;
    for (int i = 0; i < grid.buttonCount(); ++i) {
        if (grid.isButtonShown(i)) { shown << grid.filterLabel(i); }
    }
    return shown;
}

} // namespace

class TstFilterButtonItem : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("filter-button-item-%1")
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

    // Thetis MeterManager.cs:7718-7727 gives RX1 12 buttons and RX2 9;
    // every NereusSDR grid has the 12, and Var1/Var2 wait for their
    // feature (R-R3-49).
    void everyGridHasTwelveButtons()
    {
        QVERIFY(!UnbuiltFeatures::isBuilt(UnbuiltFeature::VariableFilters));
        FilterButtonItem grid;
        QCOMPARE(FilterButtonItem::kFilterCount, 12);
        QCOMPARE(grid.buttonCount(), FilterButtonItem::kFilterCount);
        QCOMPARE(grid.filterLabel(FilterButtonItem::kVar1Button), QStringLiteral("Var1"));
        QCOMPARE(grid.filterLabel(FilterButtonItem::kVar2Button), QStringLiteral("Var2"));
        QVERIFY(!grid.isButtonShown(FilterButtonItem::kVar1Button));
        QVERIFY(!grid.isButtonShown(FilterButtonItem::kVar2Button));
        QCOMPARE(grid.shownButtonCount(), 10);
        QCOMPARE(shownLabels(grid),
                 (QStringList{"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10"}));
        // From Thetis setup.cs:25826-25832: RX1's 1-12 columns, on any slice.
        QCOMPARE(grid.columnRange(), std::make_pair(1, 12));
    }

    // One row per button; Var1/Var2 are never drawn while not built, and
    // their rows say why. Unticking a row hides its button.
    void editorShowsEachButtonAndVarReason()
    {
        FilterButtonItem grid;
        const uint32_t bitsBefore = grid.visibleBits();
        FilterButtonItemEditor editor;
        editor.setItem(&grid);
        QSignalSpy changed(&editor, &BaseItemEditor::propertyChanged);
        for (int i = 0; i < FilterButtonItem::kFilterCount; ++i) {
            auto* check = editor.findChild<QCheckBox*>(QStringLiteral("filterVisible_%1").arg(i));
            QVERIFY2(check, qPrintable(QString::number(i)));
            QVERIFY(check->isChecked());
            const bool var = i >= FilterButtonItem::kVar1Button;
            QCOMPARE(check->isEnabled(), !var);
            QCOMPARE(check->toolTip(), var ? UnbuiltFeatures::notBuiltReason() : QString());
        }
        QVERIFY(!UnbuiltFeatures::notBuiltReason().isEmpty());

        QCOMPARE(changed.count(), 0);
        auto* f3 = editor.findChild<QCheckBox*>(QStringLiteral("filterVisible_2"));
        f3->setChecked(false);
        QVERIFY(!grid.isButtonShown(2));
        QCOMPARE(grid.shownButtonCount(), 9);
        QCOMPARE(changed.count(), 1);
        f3->setChecked(true);
        QVERIFY(grid.isButtonShown(2));
        QCOMPARE(grid.visibleBits(), bitsBefore);

        // Loading another grid's bits updates the rows without a change.
        FilterButtonItem other;
        other.setVisibleBits(other.visibleBits() & ~(1u << 4));
        editor.setItem(&other);
        QVERIFY(!editor.findChild<QCheckBox*>(QStringLiteral("filterVisible_4"))->isChecked());
        QCOMPARE(changed.count(), 2);
    }

    // D5: saved bits come back as saved; a record written before the
    // bits were saved loads with every built button shown.
    void savedGridsKeepTheirButtons()
    {
        FilterButtonItem grid;
        grid.setColumns(5);
        grid.setVisibleBits(grid.visibleBits() & ~(1u << 7));
        FilterButtonItem loaded;
        QVERIFY(loaded.deserialize(grid.serialize()));
        QCOMPARE(loaded.visibleBits(), grid.visibleBits());
        QCOMPARE(loaded.columns(), 5);
        QVERIFY(!loaded.isButtonShown(7));
        QCOMPARE(loaded.buttonCount(), FilterButtonItem::kFilterCount);

        FilterButtonItem old;
        QVERIFY(old.deserialize(QStringLiteral("FILTERBTNS|0|0|1|1|0|0|4|-1")));
        QCOMPARE(old.shownButtonCount(), 10);
        QCOMPARE(old.columns(), 4);
    }

    // In a window: the click sets the entry's slice's RX filter only; a
    // grid on Slice B shows the same 12 buttons as one on Slice A.
    void clickSetsTheSliceRxFilterOnlyOnEverySlice()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* window = sessions.window();
        RadioModel* model = window->radioModel();
        SliceModel* a = ensureSlice(model);
        QVERIFY(a != nullptr);
        model->addSlice();
        QCOMPARE(model->slices().size(), 2);
        SliceModel* b = model->slices().at(1);
        QVERIFY(b != nullptr && b != a);
        a->setDspMode(DSPMode::USB);
        b->setDspMode(DSPMode::USB);
        FilterPresetStore* store = model->filterPresetStore();
        QVERIFY(store != nullptr);
        const QList<FilterPreset> presets = store->presetsForMode(DSPMode::USB);
        QVERIFY(presets.size() >= 10);
        a->setFilter(presets[0].low, presets[0].high);
        b->setFilter(presets[0].low, presets[0].high);

        ContainerManager* cm = model->containerManager();
        ContainerWidget* container = cm->createContainer(1, DockMode::OverlayDocked);
        QVERIFY(container != nullptr);
        auto* meter = new MeterWidget();
        container->setContent(meter);
        auto* gridB = new FilterButtonItem();
        auto* gridA = new FilterButtonItem();
        meter->addItem(gridB);
        meter->addItem(gridA);
        container->wireInteractiveItem(gridB);
        container->wireInteractiveItem(gridA);
        gridB->setProperty("containerSourceContext", QJsonObject{{"sliceId", b->sliceIndex()}});
        gridA->setProperty("containerSourceContext", QJsonObject{{"sliceId", a->sliceIndex()}});

        // The same 12 buttons, the same shown set, on both slices.
        QCOMPARE(gridB->buttonCount(), gridA->buttonCount());
        QCOMPARE(gridB->buttonCount(), FilterButtonItem::kFilterCount);
        QCOMPARE(gridB->shownButtonCount(), gridA->shownButtonCount());
        QCOMPARE(gridB->columnRange(), gridA->columnRange());

        TransmitModel& tx = model->transmitModel();
        const int txLow = tx.filterLow();
        const int txHigh = tx.filterHigh();
        QSignalSpy txFilter(&tx, &TransmitModel::filterChanged);
        const int aLow = a->filterLow();
        const int aHigh = a->filterHigh();

        // F3 on Slice B's grid: B's RX filter only; A and TX unchanged.
        emit gridB->filterClicked(2);
        QCOMPARE(b->filterLow(), presets[2].low);
        QCOMPARE(b->filterHigh(), presets[2].high);
        QCOMPARE(a->filterLow(), aLow);
        QCOMPARE(a->filterHigh(), aHigh);
        QCOMPARE(gridB->activeFilter(), 2);
        QCOMPARE(gridA->activeFilter(), 0);

        // F10 on Slice B: the last preset applies on B too (no 9-button
        // limit as Thetis's RX2 has).
        emit gridB->filterClicked(9);
        QCOMPARE(b->filterLow(), presets[9].low);
        QCOMPARE(b->filterHigh(), presets[9].high);
        QCOMPARE(gridB->activeFilter(), 9);

        // F5 on Slice A's grid: A only.
        emit gridA->filterClicked(4);
        QCOMPARE(a->filterLow(), presets[4].low);
        QCOMPARE(a->filterHigh(), presets[4].high);
        QCOMPARE(b->filterLow(), presets[9].low);
        QCOMPARE(gridA->activeFilter(), 4);

        // Var1 has nothing behind it: nothing changes.
        emit gridB->filterClicked(FilterButtonItem::kVar1Button);
        QCOMPARE(b->filterLow(), presets[9].low);
        QCOMPARE(b->filterHigh(), presets[9].high);

        // An entry on another session's radio drives nothing here.
        gridB->setProperty("containerSourceContext",
                           QJsonObject{{"sessionId", "another-radio"}, {"sliceId", a->sliceIndex()}});
        emit gridB->filterClicked(1);
        QCOMPARE(a->filterLow(), presets[4].low);
        QCOMPARE(a->filterHigh(), presets[4].high);
        QCOMPARE(b->filterLow(), presets[9].low);
        QCOMPARE(b->filterHigh(), presets[9].high);

        // TransmitModel's filter is untouched by every click.
        QCOMPARE(txFilter.count(), 0);
        QCOMPARE(tx.filterLow(), txLow);
        QCOMPARE(tx.filterHigh(), txHigh);
        QVERIFY(!tx.isMox());
        QVERIFY(sessions.replace({}, false));
    }

    // A preview copy of the grid never reaches the window's handler.
    void aPreviewClickNeverActs()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings{directory.filePath(QStringLiteral("filters.xml"))};
        ContainerWorkspaceStore store{settings};
        ContainerContentRegistry registry;
        QWidget dock;
        QSplitter splitter;
        ContainerManager manager{&dock, &splitter};
        manager.setWorkspaceAdapter(&store, &registry);
        MeterPoller poller;
        manager.setPreviewPoller(&poller);
        dock.resize(1200, 900);

        FilterButtonItem shape;
        ContentEntry entry = registry.makeEntry(QStringLiteral("FILTERBTNS"));
        entry.id = QStringLiteral("filter-buttons");
        entry.config[QStringLiteral("legacyRecord")] = shape.serialize();
        ContainerDocument container;
        container.id = QStringLiteral("grid");
        container.name = QStringLiteral("grid");
        container.layout = ContentLayout::VerticalStack;
        container.geometry = QRect(10, 20, kSize, kSize);
        container.contents.append(entry);
        WorkspaceDocument document;
        document.containers.append(container);
        document.mainContainerId = container.id;
        QCOMPARE(manager.commitWorkspace(document, 0).status, CommitStatus::Saved);

        // MainWindow's wiring: a container's filter click goes to its handler.
        int clicked = 0;
        for (ContainerWidget* c : manager.allContainers()) {
            connect(c, &ContainerWidget::filterClicked, this, [&clicked](int) { ++clicked; });
        }
        ContainerContentHost* host = manager.contentHost(QStringLiteral("grid"));
        QVERIFY(host);
        QVERIFY(QTest::qWaitFor([&] { return firstGrid(host->meterSurfaces()) != nullptr; }, 5000));
        FilterButtonItem* live = firstGrid(host->meterSurfaces());
        manager.container(QStringLiteral("grid"))->wireInteractiveItem(live);

        ContainerSettingsDialog dialog(manager.container(QStringLiteral("grid")), nullptr, &manager);
        dialog.refreshDraftView();
        auto* preview = dialog.findChild<ContainerPreviewWidget*>();
        QVERIFY(preview);
        FilterButtonItem* copy = firstGrid(preview->findChildren<MeterWidget*>());
        QVERIFY(copy);
        QVERIFY(copy != live);
        for (MeterWidget* meter : preview->findChildren<MeterWidget*>()) {
            QVERIFY(meter->testAttribute(Qt::WA_TransparentForMouseEvents));
        }
        for (int i = 0; i < FilterButtonItem::kFilterCount; ++i) {
            emit copy->filterClicked(i);
        }
        QCOMPARE(clicked, 0);

        // The live grid's click reaches it (the wiring above is real).
        emit live->filterClicked(0);
        QCOMPARE(clicked, 1);
        dialog.reject();
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    TstFilterButtonItem test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "tst_filter_button_item.moc"
