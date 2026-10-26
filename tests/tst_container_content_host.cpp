// no-port-check: NereusSDR-original mixed host integration invariants.
// Modification history (NereusSDR):
//   2026-10-26 - Fix wave round 2 (R2): a signal text unit chosen in the
//                 editor, then changed by a click box, keeps the clicked unit
//                 through the save and rebuild. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-16 - An unavailable applet is grayed with its reason, never
//                 hidden, by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-04 — Cover safe legacy rounding and unsafe integer boundaries by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QSplitter>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <QAction>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <limits>
#include "gui/meters/MeterItem.h"
#include "core/AppSettings.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/ContainerEditSession.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerSourceAdapter.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/applets/AppletWidget.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/ClickBoxItem.h"
#include "gui/meters/SignalTextItem.h"
#include "gui/containers/ContentPropertyEditor.h"
#include <QComboBox>
#include "gui/SMeterWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
using namespace NereusSDR;
namespace {
// An applet with the root column every station applet has.
class ProbeApplet : public AppletWidget {
public:
    explicit ProbeApplet(QWidget* parent) : AppletWidget(nullptr, parent)
    {
        auto* root = new QVBoxLayout(this);
        root->addWidget(new QPushButton(QStringLiteral("Operate"), this));
    }
    QString appletId() const override { return QStringLiteral("probe"); }
    QString appletTitle() const override { return QStringLiteral("Probe"); }
    void syncFromModel() override {}
};
// An applet with a title bar over a box layout, a grid layout, or none.
class LayoutApplet : public AppletWidget {
public:
    LayoutApplet(const QString& kind, QWidget* parent) : AppletWidget(nullptr, parent)
    {
        if (kind == QStringLiteral("box")) {
            auto* root = new QVBoxLayout(this);
            root->addWidget(appletTitleBar(QStringLiteral("Layout")));
            root->addWidget(new QPushButton(QStringLiteral("Operate"), this));
        } else if (kind == QStringLiteral("grid")) {
            auto* root = new QGridLayout(this);
            root->addWidget(new QPushButton(QStringLiteral("Operate"), this), 0, 0);
            root->addWidget(new QPushButton(QStringLiteral("Tune"), this), 0, 1);
        } else {
            (new QPushButton(QStringLiteral("Operate"), this))->setGeometry(0, 40, 80, 22);
        }
    }
    QString appletId() const override { return QStringLiteral("layout"); }
    QString appletTitle() const override { return QStringLiteral("Layout"); }
    void syncFromModel() override {}
};
} // namespace
class TstContainerContentHost : public QObject {
    Q_OBJECT
private slots:
    void legacyBoundaryKeepsFloatRoundingAndChecksIntegerProjection_data()
    {
        QTest::addColumn<QRectF>("geometry");
        QTest::addColumn<QRect>("expected");
        QTest::newRow("fractional-float-rounding") << QRectF(.15,-.15,.15,.15) << QRect(3,-3,3,3);
        QTest::newRow("negative-position") << QRectF(-3.25,-2.5,.5,.25) << QRect(-65,-50,10,5);
        QTest::newRow("negative-dimension") << QRectF(-.1,.2,-.15,.15) << QRect(-2,4,-3,3);
        QTest::newRow("zero-width") << QRectF(.15,.15,0,.15) << QRect(3,3,0,3);
        QTest::newRow("zero-height") << QRectF(.15,.15,.15,0) << QRect(3,3,3,0);
        QTest::newRow("safe-int-min-left") << QRectF(-107374182.4,0,.15,.15)
            << QRect(std::numeric_limits<int>::min(),0,3,3);
        QTest::newRow("safe-large-width") << QRectF(0,0,107374176,.15) << QRect(0,0,2147483520,3);
        QTest::newRow("finite-float-overflow") << QRectF(0,0,3e38,.15) << QRect();
        QTest::newRow("float-rounding-past-int-max") << QRectF(107374182.35,0,.15,.15) << QRect();
        QTest::newRow("far-edge-past-int-max") << QRectF(107374176,0,12.8,.15) << QRect();
        QTest::newRow("positive-span-overflow") << QRectF(-50000000,0,107374182.4,.15) << QRect();
        QTest::newRow("negative-span-overflow") << QRectF(50000000,0,-150000000,.15) << QRect();
        QTest::newRow("int-min-exclusive-right") << QRectF(-107374182.4,0,0,.15) << QRect();
        QTest::newRow("int-min-negative-span") << QRectF(50000000,0,-107374182.4,.15) << QRect();
        QTest::newRow("vertical-span-overflow") << QRectF(0,-50000000,.15,107374182.4) << QRect();
        QTest::newRow("int-min-exclusive-bottom") << QRectF(0,-107374182.4,.15,0) << QRect();
    }
    void legacyBoundaryKeepsFloatRoundingAndChecksIntegerProjection()
    {
        QFETCH(QRectF,geometry);
        QFETCH(QRect,expected);
        ContainerContentRegistry registry; ContainerContentHost host(registry);
        ContainerDocument document; document.id="legacy"; document.layout=ContentLayout::LegacyCanvas;
        auto entry=registry.makeEntry("TEXT"); entry.canvasRect=geometry;
        entry.extensions["originalImport"]=QStringLiteral("retain complete record");
        document.contents={entry}; host.reconcile(document);
        QCOMPARE(host.entryRows().size(),1); QVERIFY(host.entryRows().first().item);
        QWidget* surface=host.entryRows().first().widget;
        // Keep the host hidden: the public boundary query must be safe before
        // painting, without exercising MeterItem's separate pixelRect path.
        surface->setFixedSize(20,20);
        const QPoint origin=surface->mapTo(&host,QPoint());
        surface->move(surface->pos()-origin);
        QCOMPARE(surface->mapTo(&host,QPoint()),QPoint());
        const auto before=host.captureDocument();
        const QByteArray capturedBytes=ContainerDocumentCodec::exportContainer(before);
        QCOMPARE(host.entryBoundary(entry.id),expected);
        QCOMPARE(host.captureDocument(),before);
        QCOMPARE(ContainerDocumentCodec::exportContainer(host.captureDocument()),capturedBytes);
        QCOMPARE(host.captureDocument().contents.first().config["legacyRecord"],entry.config["legacyRecord"]);
        QCOMPARE(host.captureDocument().contents.first().canvasRect.x(),geometry.x());
        QCOMPARE(host.captureDocument().contents.first().canvasRect.y(),geometry.y());
        QCOMPARE(host.captureDocument().contents.first().canvasRect.width(),geometry.width());
        QCOMPARE(host.captureDocument().contents.first().canvasRect.height(),geometry.height());
    }
    void legacyBoundaryLeavesImportedRecordsAndUnchangedApplyIntact()
    {
        QTemporaryDir directory; AppSettings settings(directory.filePath("settings.xml"));
        ContainerWorkspaceStore store(settings); ContainerContentRegistry registry;
        ContainerDocument container; container.id="legacy"; container.layout=ContentLayout::LegacyCanvas;
        auto precise=registry.makeEntry("TEXT");
        precise.canvasRect=QRectF(-3.250000000000001,.15000000000000002,.15000000000000002,0);
        QStringList legacyFields=precise.config["legacyRecord"].toString().split('|');
        legacyFields[1]="-3.250000"; legacyFields[2]="0.150000";
        legacyFields[3]="0.150000"; legacyFields[4]="0.000000";
        precise.config["legacyRecord"]=legacyFields.join('|');
        auto oversized=registry.makeEntry("TEXT"); oversized.canvasRect=QRectF(0,0,3e38,.15);
        for(ContentEntry* entry:{&precise,&oversized}) {
            entry->extensions["opaqueOriginal"]=QJsonObject{{"keep",true},{"precision","original bytes"}};
        }
        container.contents={precise,oversized};
        WorkspaceDocument document; document.mainContainerId=container.id; document.containers={container};
        QCOMPARE(store.commit(document,0).status,CommitStatus::Saved);
        const auto saved=store.snapshot();
        const QByteArray savedEntryBytes=ContainerDocumentCodec::exportContainer(saved.containers.first());
        ContainerContentHost host(registry); host.reconcile(saved.containers.first());
        const auto captured=host.captureDocument();
        for(const ContentEntry& entry:saved.containers.first().contents) {
            host.entryBoundary(entry.id);
        }
        QCOMPARE(host.captureDocument(),captured);
        for(int i=0;i<container.contents.size();++i) {
            const auto& original=container.contents[i]; const auto& retained=captured.contents[i];
            QCOMPARE(retained.config["legacyRecord"],original.config["legacyRecord"]);
            QCOMPARE(retained.extensions,original.extensions);
            QCOMPARE(retained.canvasRect.x(),original.canvasRect.x());
            QCOMPARE(retained.canvasRect.y(),original.canvasRect.y());
            QCOMPARE(retained.canvasRect.width(),original.canvasRect.width());
            QCOMPARE(retained.canvasRect.height(),original.canvasRect.height());
        }
        ContainerEditSession session(store); QVERIFY(!session.hasPendingChanges());
        QCOMPARE(session.apply().status,CommitStatus::Saved);
        QCOMPARE(store.snapshot().containers,saved.containers);
        QCOMPARE(ContainerDocumentCodec::exportContainer(store.snapshot().containers.first()),savedEntryBytes);
    }
    void orderedRunsKeepContextGeometryAndHiddenBoundaries()
    {
        ContainerContentRegistry registry;
        ContainerContentHost host(registry);
        ContainerDocument d; d.id = "main"; d.layout = ContentLayout::VerticalStack;
        auto a = registry.makeEntry("meter.mic"); a.id = "a"; a.canvasRect = QRectF(.13,.24,.61,.18); a.context["sliceId"] = 2;
        auto b = registry.makeEntry("meter.alc"); b.id = "b"; b.context["sliceId"] = 2;
        auto c = registry.makeEntry("meter.mic"); c.id = "c"; c.context["sliceId"] = 7;
        auto hidden = registry.makeEntry("meter.alc"); hidden.id = "hidden"; hidden.visible = false;
        auto late = registry.makeEntry("applet:rx"); late.id = "late";
        d.contents = {a,b,c,hidden,late};
        host.reconcile(d);
        QCOMPARE(host.meterSurfaces().size(), 2);
        QCOMPARE(host.meterSurfaces()[0]->items().size(), 2);
        QCOMPARE(host.sourceContext(host.meterSurfaces()[1]).value("sliceId").toInt(), 7);
        QCOMPARE(host.entryRows().size(), 5);
        QCOMPARE(host.entryRows()[3].entryId, QString("hidden"));
        QVERIFY(!host.entryRows()[3].effectiveVisible);
        QCOMPARE(host.captureDocument().contents[0].canvasRect, a.canvasRect);
        const auto* original = host.meterSurfaces()[0];
        host.reconcile(d); QCOMPARE(host.meterSurfaces()[0], original);
        auto* widget = new QWidget(&host); widget->setObjectName("one live model view");
        registry.attachSingleton("applet:rx", widget); host.reconcile(d);
        QCOMPARE(host.entryRows()[4].widget.data(), widget);
        registry.setAvailable("applet:rx", false); host.reconcile(d);
        QCOMPARE(host.entryRows()[4].widget.data(), widget);
        // Disabled, never hidden (2026-10-16).
        QVERIFY(!widget->isHidden()); QVERIFY(!widget->isEnabled()); QVERIFY(d.contents[4].visible);
        registry.setAvailable("applet:rx", true); host.reconcile(d);
        QCOMPARE(registry.singletonView("applet:rx"), widget);
    }
    void committedMoveKeepsSingletonAndReplacesTargetsSynchronously()
    {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings.xml"));
        ContainerWorkspaceStore store(settings); ContainerContentRegistry registry;
        QWidget dock; QSplitter splitter; ContainerManager manager(&dock, &splitter);
        MeterPoller poller;
        manager.setWorkspaceAdapter(&store, &registry);
        connect(&manager, &ContainerManager::meterContextReady, &poller, &MeterPoller::setTargetContext);
        WorkspaceDocument document; document.mainContainerId = "main";
        ContainerDocument main; main.id = "main"; main.layout = ContentLayout::VerticalStack;
        auto sm = registry.makeEntry("applet:s_meter"); auto bar = registry.makeEntry("meter.mic");
        main.contents = {sm,bar};
        ContainerDocument second; second.id = "second"; second.layout = ContentLayout::VerticalStack; second.dockMode = DockMode::Floating;
        document.containers = {main,second};
        QCOMPARE(manager.commitWorkspace(document, 0).status, CommitStatus::Saved);
        auto* meter = new SMeterWidget(); registry.attachSingleton("applet:s_meter", meter); poller.setSMeter(meter);
        QPointer<SMeterWidget> stable(meter);
        QPointer<MeterWidget> old(manager.contentHost("main")->meterSurfaces().first());
        QCOMPARE(poller.targetCountForTest(), 1);
        document = store.snapshot(); document.containers[0].contents.removeAt(0); document.containers[1].contents.append(sm);
        QCOMPARE(manager.commitWorkspace(document, document.revision).status, CommitStatus::Saved);
        QVERIFY(stable); QCOMPARE(registry.singletonView("applet:s_meter"), stable.data());
        QVERIFY(manager.contentHost("second")->isAncestorOf(stable));
        QVERIFY(old.isNull()); QCOMPARE(poller.targetCountForTest(), 1);
        manager.reconcileWorkspace(store.snapshot()); QCOMPARE(poller.targetCountForTest(), 1);
        manager.floatContainer("main"); QCOMPARE(poller.targetCountForTest(), 1);
        manager.panelDockContainer("main"); QCOMPARE(poller.targetCountForTest(), 1);
        document = store.snapshot(); document.containers[0].contents.append(sm); document.containers[0].contents.last().id = "duplicate";
        QCOMPARE(manager.commitWorkspace(document, document.revision).status, CommitStatus::Invalid);
        QVERIFY(stable);
    }
    void structuredVisibilityWinsLegacyAndFailedSaveIsInert()
    {
        QTemporaryDir dir; const QString path = dir.filePath("settings.xml"); AppSettings settings(path);
        ContainerWorkspaceStore store(settings); ContainerContentRegistry registry;
        WorkspaceDocument d; d.mainContainerId = "main"; ContainerDocument c; c.id = "main";
        auto a = registry.makeEntry("applet:rx"); a.visible = false; c.contents = {a}; d.containers = {c};
        QCOMPARE(store.commit(d,0).status, CommitStatus::Saved);
        auto& global = AppSettings::instance();
        const bool hadLegacy = global.contains("AppletRxVisible"); const QVariant legacy = global.value("AppletRxVisible");
        const auto restoreLegacy = qScopeGuard([&] { if (hadLegacy) { global.setValue("AppletRxVisible",legacy); } else { global.remove("AppletRxVisible"); } });
        global.setValue("AppletRxVisible", "True");
        AppletVisibilityController visibility; visibility.setWorkspaceAdapter(&store, &registry);
        visibility.registerApplet("Rx","RX",true); QVERIFY(!visibility.isVisible("Rx"));
        visibility.setAvailable("Rx",false); visibility.setVisible("Rx",true);
        QVERIFY(visibility.isVisible("Rx")); QVERIFY(!visibility.isEffectivelyVisible("Rx"));
        QCOMPARE(store.snapshot().containers[0].contents[0].visible, true);
        visibility.setAvailable("Rx",true);
        // A future/corrupt workspace remains read-only, with original bytes intact.
        settings.setValue("ContainerWorkspace","{\"schemaVersion\":99}"); QVERIFY(!store.load().ok);
        const auto before = store.snapshot(); visibility.setVisible("Rx",false);
        QVERIFY(visibility.isVisible("Rx")); QCOMPARE(store.snapshot(),before);
        QCOMPARE(settings.value("ContainerWorkspace").toString(),QString("{\"schemaVersion\":99}"));
        QVERIFY(!visibility.storageError().isEmpty());
    }
    void saveFailureKeepsCommittedUiAndBytes()
    {
        QTemporaryDir dir; QVERIFY(QDir(dir.path()).mkdir("profile"));
        const QString profile = dir.filePath("profile"); const QString path = profile + "/settings.xml";
        AppSettings settings(path); ContainerWorkspaceStore store(settings); ContainerContentRegistry registry;
        QWidget owner, dock; QSplitter splitter; ContainerManager manager(&dock,&splitter);
        manager.setWorkspaceAdapter(&store,&registry);
        auto* view = new QWidget(&owner); registry.attachSingleton("applet:rx",view);
        WorkspaceDocument d; d.mainContainerId="main"; ContainerDocument c; c.id="main";
        auto entry=registry.makeEntry("applet:rx"); c.contents={entry}; d.containers={c};
        QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);
        AppletVisibilityController controller; controller.setWorkspaceAdapter(&store,&registry); controller.registerApplet("Rx","RX",true);
        QAction menu; menu.setCheckable(true); menu.setChecked(controller.isVisible("Rx"));
        connect(&menu,&QAction::toggled,&controller,[&](bool v){controller.setVisible("Rx",v);});
        connect(&controller,&AppletVisibilityController::visibilityChanged,&menu,[&](const QString&,bool v){menu.setChecked(v);});
        const auto before=store.snapshot(); const QVariant raw=settings.value("ContainerWorkspace");
        const auto parent=view->parentWidget(); const bool hidden=view->isHidden();
        QFile original(path); QVERIFY(original.open(QIODevice::ReadOnly)); const QByteArray bytes=original.readAll(); original.close();
        QVERIFY(QDir().rename(profile,dir.filePath("saved-profile")));
        QFile obstruction(profile); QVERIFY(obstruction.open(QIODevice::WriteOnly)); obstruction.write("obstruction"); obstruction.close();
        auto draft=before; draft.containers[0].contents[0].visible=false;
        QCOMPARE(store.commit(draft,before.revision).status,CommitStatus::StorageError);
        QSignalSpy failures(&controller,&AppletVisibilityController::persistenceFailed);
        menu.setChecked(false);
        QCOMPARE(failures.count(),1); QVERIFY(controller.isVisible("Rx")); QVERIFY(menu.isChecked());
        QCOMPARE(view->parentWidget(),parent); QCOMPARE(view->isHidden(),hidden);
        QCOMPARE(store.snapshot(),before); QCOMPARE(settings.value("ContainerWorkspace"),raw);
        QFile preserved(dir.filePath("saved-profile/settings.xml")); QVERIFY(preserved.open(QIODevice::ReadOnly)); QCOMPARE(preserved.readAll(),bytes);
    }
    void selectiveReconcilePreservesHistoryAndDestroyedSingletonPlaceholder()
    {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings.xml"));
        ContainerWorkspaceStore store(settings); ContainerContentRegistry registry;
        QWidget owner,dock; QSplitter splitter; ContainerManager manager(&dock,&splitter); MeterPoller poller;
        manager.setWorkspaceAdapter(&store,&registry);
        connect(&manager,&ContainerManager::meterContextReady,&poller,&MeterPoller::setTargetContext);
        WorkspaceDocument d; d.mainContainerId="main";
        ContainerDocument a; a.id="main"; a.layout=ContentLayout::VerticalStack;
        auto face=registry.makeEntry("meter.mic"); auto properties=face.config["properties"].toObject(); properties["ignoreHistoryMs"]=0; face.config["properties"]=properties; auto applet=registry.makeEntry("applet:rx"); a.contents={face,applet};
        ContainerDocument b; b.id="other"; b.layout=ContentLayout::VerticalStack; b.contents={registry.makeEntry("meter.alc")}; d.containers={a,b};
        QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);
        QPointer<MeterWidget> first=manager.contentHost("main")->meterSurfaces().first(), other=manager.contentHost("other")->meterSurfaces().first();
        auto* item=qobject_cast<BarPresetItem*>(first->items().first()); QVERIFY(item);
        item->setValue(-25); first->advanceMeters(0); item->setValue(-10); first->advanceMeters(100); const double history=item->minimumHistory();
        QVERIFY(item->hasPrimaryReading()); QVERIFY(history>-30); QVERIFY(history<item->peakValue());
        const auto contents=store.snapshot().containers[0].contents;
        d=store.snapshot(); d.containers[0].geometry=QRect(12,18,400,350); d.containers[0].name="new name"; d.containers[1].visible=false;
        QCOMPARE(manager.commitWorkspace(d,d.revision).status,CommitStatus::Saved);
        QCOMPARE(manager.contentHost("main")->meterSurfaces().first(),first.data()); QCOMPARE(manager.contentHost("other")->meterSurfaces().first(),other.data());
        QCOMPARE(item->minimumHistory(),history); QCOMPARE(poller.targetCountForTest(),2);
        manager.saveState(); QCOMPARE(store.snapshot().containers[0].contents,contents); QCOMPARE(manager.contentHost("main")->meterSurfaces().first(),first.data());
        auto* view=new QWidget(&owner); registry.attachSingleton("applet:rx",view);
        QCOMPARE(manager.contentHost("main")->entryRows()[1].widget.data(),view);
        d=store.snapshot(); d.containers[0].contents[1].visible=false;
        QCOMPARE(manager.commitWorkspace(d,d.revision).status,CommitStatus::Saved);
        QCOMPARE(manager.contentHost("main")->meterSurfaces().first(),first.data());
        d=store.snapshot(); d.containers[0].contents[1].visible=true;
        QCOMPARE(manager.commitWorkspace(d,d.revision).status,CommitStatus::Saved);
        registry.setAvailable("applet:rx",false); registry.setAvailable("applet:rx",true);
        QCOMPARE(manager.contentHost("main")->meterSurfaces().first(),first.data()); QCOMPARE(item->minimumHistory(),history);
        delete view; QVERIFY(qobject_cast<QLabel*>(manager.contentHost("main")->entryRows()[1].widget.data()));
        auto* replacement=new QWidget(&owner); registry.attachSingleton("applet:rx",replacement);
        QCOMPARE(manager.contentHost("main")->entryRows()[1].widget.data(),replacement);
        registry.attachSingleton("applet:rx",nullptr); QCOMPARE(replacement->parentWidget(),&owner);
        auto* finalView=new QWidget(&owner); registry.attachSingleton("applet:rx",finalView); delete replacement;
        QCOMPARE(registry.singletonView("applet:rx"),finalView); QCOMPARE(manager.contentHost("main")->entryRows()[1].widget.data(),finalView);
        QCOMPARE(first->items().first(),item); QCOMPARE(item->minimumHistory(),history); QCOMPARE(poller.targetCountForTest(),2);
    }
    void retainedDuplicateUsesFirstPlacementPreference()
    {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings.xml")); ContainerWorkspaceStore store(settings); ContainerContentRegistry registry;
        WorkspaceDocument d; d.mainContainerId="main"; ContainerDocument a,b; a.id="main"; b.id="duplicate-container";
        auto first=registry.makeEntry("applet:TX"), duplicate=first; duplicate.id="retained-duplicate"; duplicate.visible=false; duplicate.extensions["opaque"]="retain";
        a.contents={first}; b.contents={duplicate}; d.containers={a,b}; QCOMPARE(store.commit(d,0).status,CommitStatus::Saved);
        AppletVisibilityController controller; controller.setWorkspaceAdapter(&store,&registry); controller.registerApplet("Tx","TX",false);
        QVERIFY(controller.isVisible("Tx")); controller.setVisible("Tx",false); QVERIFY(!controller.isVisible("Tx"));
        QCOMPARE(store.snapshot().containers[1].contents[0],duplicate);
        controller.setVisible("Tx",true); QVERIFY(store.snapshot().containers[0].contents[0].visible); QCOMPARE(store.snapshot().containers[1].contents[0],duplicate);
    }
    void minimumWidthAndLegacyMixedSourcePreservation()
    {
        ContainerContentRegistry registry; ContainerContentHost host(registry); ContainerDocument d; d.id="main"; d.layout=ContentLayout::VerticalStack;
        d.contents={registry.makeEntry("meter.ananMulti"),registry.makeEntry("meter.mic")}; host.reconcile(d);
        QCOMPARE(host.meterSurfaces().size(),1); QVERIFY(host.meterSurfaces().first()->minimumWidth()>=360);
        d.layout=ContentLayout::LegacyCanvas; d.contents[0].context["sliceId"]=7; d.contents[1].context["sliceId"]=23;
        d.contents[0].canvasRect=QRectF(.2,.15,.4,.3); d.contents[1].canvasRect=QRectF(.1,.62,.7,.11); host.reconcile(d);
        QCOMPARE(host.meterSurfaces().size(),1); QVERIFY(!host.entryRows()[0].item->property("containerUnsupportedSource").toBool());
        QVERIFY(host.entryRows()[1].item->property("containerUnsupportedSource").toBool());
        QCOMPARE(host.captureDocument().contents[0].canvasRect,d.contents[0].canvasRect); QCOMPARE(host.captureDocument().contents[1].canvasRect,d.contents[1].canvasRect);
        host.meterSurfaces().first()->updateMeterValue(MeterBinding::TxMic,-20); QVERIFY(!qobject_cast<BarPresetItem*>(host.entryRows()[1].item.data())->hasPrimaryReading());
    }
    void malformedKnownEntryCreatesOnlyExplanationAndValidRun()
    {
        ContainerContentRegistry registry; ContainerContentHost host(registry); MeterPoller poller;
        int ready=0; connect(&host,&ContainerContentHost::meterSurfaceReady,this,[&](MeterWidget*,const QJsonObject&){++ready;});
        connect(&host,&ContainerContentHost::meterSurfaceReady,&poller,&MeterPoller::setTargetContext);
        ContainerDocument d; d.id="main"; d.layout=ContentLayout::VerticalStack;
        auto broken=registry.makeEntry("meter.mic"); broken.config["legacyRecord"]="retained future or damaged record"; broken.context["sliceId"]=2;
        auto valid=registry.makeEntry("meter.alc"); valid.context["sliceId"]=7; d.contents={broken,valid};
        host.reconcile(d); QCOMPARE(host.meterSurfaces().size(),1); QCOMPARE(ready,1); QCOMPARE(poller.targetCountForTest(),1);
        QCOMPARE(host.entryRows().size(),2); QVERIFY(qobject_cast<QLabel*>(host.entryRows()[0].widget.data())); QVERIFY(!host.entryRows()[0].item);
        QVERIFY(host.entryRows()[1].item); QCOMPARE(host.sourceContext(host.meterSurfaces().first()).value("sliceId").toInt(),7);
        QCOMPARE(host.captureDocument().contents[0],broken);
    }
    void borrowedStackViewReturnsToConstructionOwner()
    {
        QWidget owner; QWidget stackView(&owner); QPointer<QWidget> stable=&stackView;
        {
            ContainerContentRegistry registry;
            {
                ContainerContentHost host(registry); registry.attachSingleton("applet:rx",&stackView);
                ContainerDocument d; d.id="main"; d.layout=ContentLayout::VerticalStack; d.contents={registry.makeEntry("applet:rx")};
                host.reconcile(d); QVERIFY(host.isAncestorOf(&stackView));
            }
            QVERIFY(stable); QCOMPARE(registry.singletonView("applet:rx"),&stackView);
        }
        QVERIFY(stable); QCOMPARE(stackView.parentWidget(),&owner);
    }
    void detachedHeaderKeepsSmeterLookupAndPointer()
    {
        AppletPanelWidget panel; QPointer<SMeterWidget> sm(panel.smeterWidget());
        panel.clearHeaderWidget(); QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QVERIFY(sm); QCOMPARE(panel.smeterWidget(),sm.data());
        ContainerContentRegistry registry; registry.attachSingleton("applet:s_meter",sm);
        ContainerContentHost host(registry); ContainerDocument d; d.id="main"; d.layout=ContentLayout::VerticalStack; d.contents={registry.makeEntry("applet:s_meter")};
        host.reconcile(d); QCOMPARE(panel.smeterWidget(),sm.data());
        host.releaseViews(); QVERIFY(sm); QCOMPARE(panel.smeterWidget(),sm.data());
    }
    void stableSourceNeverFallsBackAndReadsOnlyCache()
    {
        RadioModel model(RadioModel::Role::Remote); model.setStationConnectionState(ConnectionState::Connected);
        QVERIFY(model.addSliceWithStationId(7)>=0); QVERIFY(model.addSliceWithStationId(23)>=0);
        auto* first=model.sliceById(7); auto* second=model.sliceById(23); first->setStreamIndex(0); second->setStreamIndex(1);
        first->setSignalPeakDbm(-65); second->setSignalPeakDbm(-43);
        const QJsonObject explicitSource{{"sliceId",23}};
        QCOMPARE(ContainerSourceAdapter::slice(&model,explicitSource,first),second);
        QCOMPARE(ContainerSourceAdapter::reading(&model,explicitSource,first,MeterBinding::SignalPeak,true,true,{}),-43.0);
        QCOMPARE(ContainerSourceAdapter::reading(&model,{{"sliceId",99}},first,MeterBinding::SignalPeak,true,true,{}),kNoMeterReadingDbm);
        QCOMPARE(ContainerSourceAdapter::reading(&model,{},first,MeterBinding::SignalPeak,false,true,{}),kNoMeterReadingDbm);
        QCOMPARE(ContainerSourceAdapter::reading(&model,explicitSource,first,MeterBinding::PbSnr,true,true,{}),kNoMeterReadingDbm);
        QCOMPARE(ContainerSourceAdapter::reading(&model,explicitSource,first,MeterBinding::AdcPeak,true,false,{}),kNoMeterReadingDbm);
        const auto maxBin=[](const SliceModel* source){return source->sliceIndex()==23?-41.0:-71.0;};
        QCOMPARE(ContainerSourceAdapter::reading(&model,explicitSource,first,MeterBinding::SignalMaxBin,true,true,maxBin),-41.0);
        QCOMPARE(ContainerSourceAdapter::slice(&model,{{"sessionId","station"},{"sliceId",23}},first,"station"),second);
        QVERIFY(!ContainerSourceAdapter::slice(&model,{{"sessionId","elsewhere"},{"sliceId",23}},first,"station"));
        MeterPoller poller; QSignalSpy frames(&poller,&MeterPoller::frameAdvanced); poller.start();
        QTRY_VERIFY_WITH_TIMEOUT(frames.count()>0,500); poller.stop();
    }
    // Disabled, never hidden (2026-10-16): an applet whose feature is off
    // (Power Genius, Tuner Genius, RF-Kit, RADE) is shown grayed with its
    // reason, as the Rotor applet grays itself, in a column and on a free
    // canvas alike. An entry the operator hid stays hidden.
    void unavailableAppletIsGrayedWithItsReason_data()
    {
        QTest::addColumn<int>("layout");
        QTest::newRow("column") << int(ContentLayout::VerticalStack);
        QTest::newRow("free canvas") << int(ContentLayout::FreeCanvas);
    }
    void unavailableAppletIsGrayedWithItsReason()
    {
        QFETCH(int, layout);
        ContainerContentRegistry registry;
        QWidget owner;
        auto* applet = new ProbeApplet(&owner);
        registry.attachSingleton("applet:amp", applet);
        ContainerContentHost host(registry);
        host.resize(400, 400);
        host.show();
        ContainerDocument d; d.id = "main"; d.layout = ContentLayout(layout);
        auto entry = registry.makeEntry("applet:amp"); entry.id = "amp";
        if (d.layout == ContentLayout::FreeCanvas) { entry.setFreeCanvasRect(QRectF(0, 0, 300, 200)); }
        d.contents = {entry};
        const QString reason = QStringLiteral("Turn on 4O3A in Setup to use the Power Genius.");
        const auto shown = [&] {
            return d.layout == ContentLayout::FreeCanvas
                ? applet->property("freeCanvasEffectiveVisible").toBool() && !applet->isHidden()
                : !applet->isHidden();
        };
        registry.setAvailable("applet:amp", false, reason);
        host.reconcile(d);
        QCOMPARE(host.entryRows()[0].widget.data(), applet);
        QVERIFY2(shown(), "an unavailable applet stays on the screen");
        QVERIFY(!applet->isEnabled());
        QCOMPARE(applet->unavailableReason(), reason);
        auto* label = applet->findChild<QLabel*>(QStringLiteral("appletUnavailableReason"));
        QVERIFY(label != nullptr);
        QCOMPARE(label->text(), reason);
        QVERIFY(label->isVisibleTo(applet));

        registry.setAvailable("applet:amp", true);
        host.reconcile(d);
        QVERIFY(shown());
        QVERIFY(applet->isEnabled());
        QVERIFY(applet->unavailableReason().isEmpty());
        QVERIFY(!label->isVisibleTo(applet));

        registry.setAvailable("applet:amp", false, reason);
        d.contents[0].visible = false;
        host.reconcile(d);
        QVERIFY2(!shown(), "an entry the operator hid stays hidden");
    }
    // The reason stands in for a grayed view's tooltip only while it is
    // grayed: available again, the view's own tooltip comes back.
    void grayingKeepsTheViewsOwnToolTip_data()
    {
        QTest::addColumn<bool>("isApplet");
        QTest::newRow("applet") << true;
        QTest::newRow("plain view") << false;
    }
    void grayingKeepsTheViewsOwnToolTip()
    {
        QFETCH(bool, isApplet);
        ContainerContentRegistry registry;
        QWidget owner;
        QWidget* view = isApplet ? static_cast<QWidget*>(new ProbeApplet(&owner)) : new QWidget(&owner);
        view->setToolTip(QStringLiteral("Its own tip"));
        registry.attachSingleton("applet:amp", view);
        ContainerContentHost host(registry);
        host.resize(400, 400);
        host.show();
        ContainerDocument d; d.id = "main"; d.layout = ContentLayout::VerticalStack;
        auto entry = registry.makeEntry("applet:amp"); entry.id = "amp";
        d.contents = {entry};
        const QString reason = QStringLiteral("Turn on 4O3A in Setup to use the Power Genius.");
        registry.setAvailable("applet:amp", false, reason);
        host.reconcile(d);
        QVERIFY(!view->isEnabled());
        QCOMPARE(view->toolTip(), reason);
        // Reconciled again while unavailable, its own tip is still kept.
        host.reconcile(d);
        QCOMPARE(view->toolTip(), reason);

        registry.setAvailable("applet:amp", true);
        host.reconcile(d);
        QVERIFY(view->isEnabled());
        QCOMPARE(view->toolTip(), QStringLiteral("Its own tip"));
    }
    // The reason line is placed whatever layout the applet has: under its
    // title bar in a box layout, in a grid's next row, and across the top
    // (kept as wide as the applet) with no layout at all.
    void reasonLineIsPlacedWhateverTheLayout_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::newRow("box") << QStringLiteral("box");
        QTest::newRow("grid") << QStringLiteral("grid");
        QTest::newRow("none") << QStringLiteral("none");
    }
    void reasonLineIsPlacedWhateverTheLayout()
    {
        QFETCH(QString, kind);
        QWidget owner;
        auto* applet = new LayoutApplet(kind, &owner);
        applet->resize(300, 160);
        owner.show();
        QVERIFY(QTest::qWaitForWindowExposed(&owner));
        applet->setUnavailableReason(QStringLiteral("Switch the active slice to a RADE mode to use RADE."));
        auto* label = applet->findChild<QLabel*>(QStringLiteral("appletUnavailableReason"));
        QVERIFY(label != nullptr);
        QVERIFY(label->isVisibleTo(applet));
        if (kind == QStringLiteral("none")) {
            QVERIFY(applet->layout() == nullptr);
            QCOMPARE(label->geometry().topLeft(), QPoint(0, 0));
            QCOMPARE(label->width(), applet->width());
            QVERIFY(label->height() > 0);
            applet->resize(420, 160);
            QCOMPARE(label->width(), 420);
        } else {
            const int index = applet->layout()->indexOf(label);
            QVERIFY2(index >= 0, "the reason line is in the applet's layout");
            if (kind == QStringLiteral("box")) {
                QCOMPARE(index, 1);
            }
            QTRY_VERIFY(label->width() > 0);
            QVERIFY(label->geometry().top() > 0 || kind == QStringLiteral("grid"));
        }
        applet->setUnavailableReason(QString());
        QVERIFY(!label->isVisibleTo(applet));
    }
    // Containers > Hidden lists every container that is off the screen for
    // its own flags, as effectiveVisible judges them: hidden on receive
    // (or on transmit while transmitting) and hidden by a macro too. Its
    // entry shows the container again.
    void hiddenMenuListsReceiveAndMacroHiddenContainers()
    {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings.xml"));
        ContainerWorkspaceStore store(settings); ContainerContentRegistry registry;
        QWidget dock; QSplitter splitter; ContainerManager manager(&dock, &splitter);
        manager.setWorkspaceAdapter(&store, &registry);
        const auto container = [](const QString& id) {
            ContainerDocument c; c.id = id; c.name = id; c.layout = ContentLayout::VerticalStack;
            c.dockMode = DockMode::OverlayDocked; c.geometry = QRect(10, 10, 200, 120);
            return c;
        };
        WorkspaceDocument d; d.mainContainerId = "main";
        ContainerDocument rx = container("rx"); rx.config["showOnRx"] = false;
        ContainerDocument macro = container("macro"); macro.config["hiddenByMacro"] = true;
        ContainerDocument tx = container("tx"); tx.config["showOnTx"] = false;
        d.containers = {container("main"), rx, macro, tx};
        QCOMPARE(manager.commitWorkspace(d, 0).status, CommitStatus::Saved);
        const auto hiddenContainers = [&manager] {
            QStringList ids;
            for (const auto& item : manager.hiddenItems()) {
                if (item.entryId.isEmpty()) { ids.append(item.containerId); }
            }
            return ids;
        };
        QCOMPARE(hiddenContainers(), (QStringList{"rx", "macro"}));
        manager.setTransmitting(true);
        QCOMPARE(hiddenContainers(), (QStringList{"macro", "tx"}));
        manager.setTransmitting(false);

        QVERIFY(manager.showHiddenContainer("rx"));
        QVERIFY(manager.showHiddenContainer("macro"));
        QCOMPARE(hiddenContainers(), QStringList{});
        for (const auto& c : store.snapshot().containers) {
            QVERIFY2(c.config.value("showOnRx").toBool(true), qPrintable(c.id));
            QVERIFY2(!c.config.value("hiddenByMacro").toBool(false), qPrintable(c.id));
        }
        // Shown on receive: its transmit choice is the operator's still.
        QVERIFY(!store.snapshot().containers[3].config.value("showOnTx").toBool(true));
    }
    void clickedSignalUnitsSurviveSaveAndRebuild()
    {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings.xml"));
        ContainerWorkspaceStore store(settings); ContainerContentRegistry registry;
        QWidget root; QSplitter splitter(&root); ContainerManager manager(&root,&splitter);
        manager.setWorkspaceAdapter(&store,&registry);
        // The editor sets S units: the entry keeps its record and takes
        // overrides for the units and the set-here flag.
        ContentEntry text;
        {
            ContentPropertyEditor editor(registry);
            ContentEntry edited; connect(&editor,&ContentPropertyEditor::entryEdited,this,[&](const ContentEntry& e){ edited=e; });
            editor.setEntry(registry.makeEntry("SIGNALTEXT"));
            auto* units=editor.findChild<QComboBox*>("signalUnits"); QVERIFY(units);
            units->setCurrentIndex(units->findData(int(SignalTextItem::Units::SUnits)));
            QVERIFY(!edited.id.isEmpty()); text=edited;
        }
        const QJsonObject editorOverrides=text.config["overrides"].toObject();
        QVERIFY(editorOverrides.contains("7")); QVERIFY(editorOverrides.contains("26"));
        text.canvasRect=QRectF(0,0,1,1);
        // A click box over it that steps the units.
        auto box=registry.makeEntry("CLICKBOX");
        {
            std::unique_ptr<MeterItem> item(registry.createMeterItem(box,nullptr));
            auto* click=qobject_cast<ClickBoxItem*>(item.get()); QVERIFY(click);
            click->setAction(ClickBoxItem::Action::CycleUnits); click->setRect(0,0,1,1);
            box=registry.captureMeterItem(*click,box);
        }
        box.canvasRect=QRectF(0,0,1,1);
        ContainerDocument container; container.id="A"; container.layout=ContentLayout::LegacyCanvas;
        container.contents={text,box};
        WorkspaceDocument document; document.mainContainerId=container.id; document.containers={container};
        QCOMPARE(manager.commitWorkspace(document,0).status,CommitStatus::Saved);

        const auto liveText=[&manager,&text]() -> SignalTextItem* {
            ContainerContentHost* host=manager.contentHost("A");
            if(!host) { return nullptr; }
            for(const auto& row:host->entryRows()) {
                if(row.entryId==text.id) { return qobject_cast<SignalTextItem*>(row.item.data()); }
            }
            return nullptr;
        };
        SignalTextItem* item=liveText(); QVERIFY(item);
        QCOMPARE(item->units(),SignalTextItem::Units::SUnits); QVERIFY(item->unitsSetHere());
        auto* meter=qobject_cast<MeterWidget*>(item->parent()); QVERIFY(meter);
        meter->resize(200,100);
        const QPointF centre(100,50);
        QMouseEvent press(QEvent::MouseButtonPress,centre,meter->mapToGlobal(centre),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QCoreApplication::sendEvent(meter,&press);
        // One click, one step (the host's rebuild re-added the box).
        QCOMPARE(item->units(),SignalTextItem::Units::Uv);

        // The queued save commits and the host rebuilds: the clicked unit stays.
        QTRY_VERIFY(store.snapshot().containers.first().contents.first().config["overrides"].toObject().isEmpty());
        QTRY_VERIFY(liveText() && liveText()->units()==SignalTextItem::Units::Uv);
        QVERIFY(liveText()->unitsSetHere());
        // And from the saved file.
        ContainerWorkspaceStore reloaded(settings); const auto loaded=reloaded.load(); QVERIFY(loaded.ok);
        std::unique_ptr<MeterItem> saved(registry.createMeterItem(loaded.document.containers.first().contents.first(),nullptr));
        auto* savedText=qobject_cast<SignalTextItem*>(saved.get()); QVERIFY(savedText);
        QCOMPARE(savedText->units(),SignalTextItem::Units::Uv); QVERIFY(savedText->unitsSetHere());
        QCOMPARE(saved->x(),0.0f); QCOMPARE(saved->itemWidth(),1.0f);
    }
};
QTEST_MAIN(TstContainerContentHost)
#include "tst_container_content_host.moc"
