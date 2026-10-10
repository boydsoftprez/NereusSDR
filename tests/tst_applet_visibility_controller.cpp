// Verify AppletVisibilityController state, persistence, and signal emission.
// no-port-check: NereusSDR-original — no Thetis source.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "gui/applets/AppletVisibilityController.h"
#include "core/AppSettings.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include <QTemporaryDir>

using namespace NereusSDR;

namespace {

ContentEntry applet(const QString& type, bool visible = true)
{
    ContentEntry entry;
    entry.id = type + QStringLiteral("-id");
    entry.typeId = type;
    entry.name = type;
    entry.visible = visible;
    return entry;
}

// A workspace saved before applet:rotor existed: the right-hand panel
// holds the station accessory applets, and a second container holds
// meters only.
WorkspaceDocument workspaceWithoutRotor()
{
    WorkspaceDocument doc;
    ContainerDocument meters;
    meters.id = QStringLiteral("meters");
    meters.name = QStringLiteral("Meters");
    ContentEntry meter;
    meter.id = QStringLiteral("meter-one");
    meter.typeId = QStringLiteral("meter.custom");
    meter.name = QStringLiteral("Meter");
    meters.contents.append(meter);
    ContainerDocument panel;
    panel.id = QStringLiteral("panel");
    panel.name = QStringLiteral("Main Panel");
    panel.layout = ContentLayout::VerticalStack;
    panel.contents = {applet(QStringLiteral("applet:rx")), applet(QStringLiteral("applet:amp")),
                      applet(QStringLiteral("applet:tuner")),
                      applet(QStringLiteral("applet:RfKit"))};
    doc.mainContainerId = meters.id;
    doc.containers = {meters, panel};
    return doc;
}

void saveWorkspace(AppSettings& settings, const WorkspaceDocument& doc)
{
    settings.setValue(QStringLiteral("ContainerWorkspace"),
                      QString::fromUtf8(ContainerDocumentCodec::encode(doc)));
    QVERIFY(settings.save());
}

void registerStationApplets(AppletVisibilityController& c)
{
    c.registerApplet(QStringLiteral("Rx"), QStringLiteral("RX"), true);
    c.registerApplet(QStringLiteral("Amp"), QStringLiteral("Power Genius"), true);
    c.registerApplet(QStringLiteral("Tuner"), QStringLiteral("Tuner Genius"), true);
    c.registerApplet(QStringLiteral("Rotor"), QStringLiteral("Rotor"), true);
    c.registerApplet(QStringLiteral("RfKit"), QStringLiteral("RF-Kit RF2K-S"), true);
}

const ContentEntry* findType(const WorkspaceDocument& doc, const QString& type,
                             QString* containerId = nullptr, int* index = nullptr)
{
    for (const ContainerDocument& c : doc.containers) {
        for (int i = 0; i < c.contents.size(); ++i) {
            if (c.contents[i].typeId == type) {
                if (containerId) { *containerId = c.id; }
                if (index) { *index = i; }
                return &c.contents[i];
            }
        }
    }
    return nullptr;
}

} // namespace

class TstAppletVisibilityController : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()    { AppSettings::instance().clear(); }
    void cleanup()         { AppSettings::instance().clear(); }

    void default_visible_when_no_settings_key();
    void default_hidden_when_no_settings_key();
    void persisted_value_overrides_default();
    void setVisible_persists_and_emits();
    void setVisible_idempotent_no_emit_on_same_value();
    void registeredIds_returns_insertion_order();
    void displayName_round_trip();
    void registered_applet_missing_from_workspace_is_added_visible();
    void workspace_applet_already_hidden_stays_hidden();
    void setVisible_on_missing_workspace_entry_adds_it();
};

void TstAppletVisibilityController::default_visible_when_no_settings_key()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Rx"), QStringLiteral("RX"), true);
    QVERIFY(c.isVisible(QStringLiteral("Rx")));
}

void TstAppletVisibilityController::default_hidden_when_no_settings_key()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Cwx"), QStringLiteral("CW Keyer"), false);
    QVERIFY(!c.isVisible(QStringLiteral("Cwx")));
}

void TstAppletVisibilityController::persisted_value_overrides_default()
{
    AppSettings::instance().setValue(QStringLiteral("AppletRxVisible"),
                                     QStringLiteral("False"));
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Rx"), QStringLiteral("RX"), true);
    QVERIFY(!c.isVisible(QStringLiteral("Rx")));
}

void TstAppletVisibilityController::setVisible_persists_and_emits()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Tx"), QStringLiteral("TX"), true);
    QSignalSpy spy(&c, &AppletVisibilityController::visibilityChanged);

    c.setVisible(QStringLiteral("Tx"), false);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("Tx"));
    QCOMPARE(spy.first().at(1).toBool(), false);
    QCOMPARE(AppSettings::instance().value(QStringLiteral("AppletTxVisible"))
             .toString(), QStringLiteral("False"));

    AppletVisibilityController c2;
    c2.registerApplet(QStringLiteral("Tx"), QStringLiteral("TX"), true);
    QVERIFY(!c2.isVisible(QStringLiteral("Tx")));
}

void TstAppletVisibilityController::setVisible_idempotent_no_emit_on_same_value()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Vax"), QStringLiteral("VAX"), true);
    QSignalSpy spy(&c, &AppletVisibilityController::visibilityChanged);

    c.setVisible(QStringLiteral("Vax"), true);
    QCOMPARE(spy.count(), 0);

    c.setVisible(QStringLiteral("Vax"), false);
    QCOMPARE(spy.count(), 1);

    c.setVisible(QStringLiteral("Vax"), false);
    QCOMPARE(spy.count(), 1);
}

void TstAppletVisibilityController::registeredIds_returns_insertion_order()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Rx"),         QStringLiteral("RX"),          true);
    // 3D Stacked-Trace Spectrum Plan Task 22: MainWindow registers Display
    // immediately after Rx (see populateDefaultMeter()'s add order), so
    // this synthetic sequence mirrors that placement.
    c.registerApplet(QStringLiteral("Display"),    QStringLiteral("Display"),    true);
    c.registerApplet(QStringLiteral("Tx"),         QStringLiteral("TX"),          true);
    c.registerApplet(QStringLiteral("PhoneCw"),    QStringLiteral("Phone / CW"),  true);
    c.registerApplet(QStringLiteral("Vax"),        QStringLiteral("VAX"),         true);
    c.registerApplet(QStringLiteral("PureSignal"), QStringLiteral("PureSignal"),  true);

    QStringList expected{
        QStringLiteral("Rx"), QStringLiteral("Display"), QStringLiteral("Tx"),
        QStringLiteral("PhoneCw"), QStringLiteral("Vax"), QStringLiteral("PureSignal")
    };
    QCOMPARE(c.registeredIds(), expected);
}

void TstAppletVisibilityController::displayName_round_trip()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("PhoneCw"), QStringLiteral("Phone / CW"), true);
    QCOMPARE(c.displayName(QStringLiteral("PhoneCw")),
             QStringLiteral("Phone / CW"));
    QCOMPARE(c.displayName(QStringLiteral("Unknown")), QString{});
}

// Bench fix: JJ's workspace was saved before applet:rotor existed, so the
// Rotor applet had no content entry, never showed, and its menu toggle did
// nothing. A registered applet missing from a loaded workspace is added at
// the end of the panel holding the station accessory applets, with its
// default visibility, and saved once.
void TstAppletVisibilityController::registered_applet_missing_from_workspace_is_added_visible()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
    saveWorkspace(settings, workspaceWithoutRotor());
    ContainerWorkspaceStore store(settings);
    QVERIFY(store.loadError().isEmpty());
    const quint64 loadedRevision = store.snapshot().revision;
    ContainerContentRegistry registry;
    QSignalSpy commits(&store, &ContainerWorkspaceStore::committed);

    AppletVisibilityController c;
    c.setWorkspaceAdapter(&store, &registry);
    registerStationApplets(c);

    QCOMPARE(commits.count(), 1);
    QCOMPARE(store.snapshot().revision, loadedRevision + 1);
    QString container;
    int index = -1;
    const ContentEntry* rotor = findType(store.snapshot(), QStringLiteral("applet:rotor"),
                                         &container, &index);
    QVERIFY(rotor != nullptr);
    QVERIFY(rotor->visible);
    QCOMPARE(rotor->name, QStringLiteral("Rotor"));
    QCOMPARE(container, QStringLiteral("panel"));
    QCOMPARE(index, 4);
    QVERIFY(c.isVisible(QStringLiteral("Rotor")));

    // Toggling works, through the workspace.
    QSignalSpy changes(&c, &AppletVisibilityController::visibilityChanged);
    c.setVisible(QStringLiteral("Rotor"), false);
    QVERIFY(!findType(store.snapshot(), QStringLiteral("applet:rotor"))->visible);
    QVERIFY(!c.isVisible(QStringLiteral("Rotor")));
    c.setVisible(QStringLiteral("Rotor"), true);
    QVERIFY(findType(store.snapshot(), QStringLiteral("applet:rotor"))->visible);
    QVERIFY(c.isVisible(QStringLiteral("Rotor")));
    QCOMPARE(changes.count(), 2);

    // Saved once: the next start finds it and adds nothing.
    ContainerWorkspaceStore restarted(settings);
    QSignalSpy restartCommits(&restarted, &ContainerWorkspaceStore::committed);
    AppletVisibilityController again;
    again.setWorkspaceAdapter(&restarted, &registry);
    registerStationApplets(again);
    QCOMPARE(restartCommits.count(), 0);
    QVERIFY(again.isVisible(QStringLiteral("Rotor")));
}

void TstAppletVisibilityController::workspace_applet_already_hidden_stays_hidden()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
    WorkspaceDocument doc = workspaceWithoutRotor();
    doc.containers[1].contents.append(applet(QStringLiteral("applet:rotor"), false));
    saveWorkspace(settings, doc);
    ContainerWorkspaceStore store(settings);
    ContainerContentRegistry registry;
    QSignalSpy commits(&store, &ContainerWorkspaceStore::committed);

    AppletVisibilityController c;
    c.setWorkspaceAdapter(&store, &registry);
    registerStationApplets(c);

    QCOMPARE(commits.count(), 0);
    QVERIFY(!c.isVisible(QStringLiteral("Rotor")));
    QVERIFY(!findType(store.snapshot(), QStringLiteral("applet:rotor"))->visible);
}

void TstAppletVisibilityController::setVisible_on_missing_workspace_entry_adds_it()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
    saveWorkspace(settings, workspaceWithoutRotor());
    ContainerWorkspaceStore store(settings);
    ContainerContentRegistry registry;
    AppletVisibilityController c;
    c.setWorkspaceAdapter(&store, &registry);
    registerStationApplets(c);

    // The entry goes away (a layout edit, an import); the menu still works.
    WorkspaceDocument without = store.snapshot();
    without.containers[1].contents.removeLast();
    QVERIFY(findType(without, QStringLiteral("applet:rotor")) == nullptr);
    QCOMPARE(store.commit(without, without.revision).status, CommitStatus::Saved);

    c.setVisible(QStringLiteral("Rotor"), true);
    const ContentEntry* rotor = findType(store.snapshot(), QStringLiteral("applet:rotor"));
    QVERIFY(rotor != nullptr);
    QVERIFY(rotor->visible);
    QVERIFY(c.isVisible(QStringLiteral("Rotor")));
}

QTEST_MAIN(TstAppletVisibilityController)
#include "tst_applet_visibility_controller.moc"
