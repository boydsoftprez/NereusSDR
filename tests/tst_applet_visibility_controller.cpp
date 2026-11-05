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
#include <algorithm>

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

const ContainerDocument* findContainer(const WorkspaceDocument& doc, const QString& id)
{
    for (const ContainerDocument& c : doc.containers) {
        if (c.id == id) { return &c; }
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
    void ssb_panel_starts_off_under_the_am_panel_in_an_existing_profile();
    void ssb_panel_saved_visible_stays_visible();
    void ssb_panel_first_launch_entry_is_off_and_under_the_am_panel();
    void setVisible_on_missing_workspace_entry_adds_it();
    void ticking_applet_shows_its_hidden_container_data();
    void ticking_applet_shows_its_hidden_container();
    void unticking_applet_leaves_its_container_alone();
    void unavailable_reason_reaches_the_registry();
    void ticking_during_transmit_shows_a_receive_only_container();
    void ticking_a_transmit_only_container_on_receive_and_transmit();
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

// The maintainer's call: the SSB Mod Monitor starts off and one click away,
// also for a profile saved before it existed, and it sits under the AM panel.
void TstAppletVisibilityController::ssb_panel_starts_off_under_the_am_panel_in_an_existing_profile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
    WorkspaceDocument doc = workspaceWithoutRotor();
    doc.containers[1].contents.insert(1, applet(QStringLiteral("applet:mod_monitor")));
    saveWorkspace(settings, doc);
    ContainerWorkspaceStore store(settings);
    ContainerContentRegistry registry;
    AppletVisibilityController c;
    c.setWorkspaceAdapter(&store, &registry);
    c.registerApplet(QStringLiteral("SsbModMon"), QStringLiteral("SSB Mod Monitor"), false);

    QString container;
    int index = -1;
    const ContentEntry* ssb = findType(store.snapshot(), QStringLiteral("applet:ssb_mod_monitor"),
                                       &container, &index);
    QVERIFY(ssb != nullptr);
    QVERIFY(!ssb->visible);
    QCOMPARE(container, QStringLiteral("panel"));
    QCOMPARE(index, 2);
    QVERIFY(!c.isVisible(QStringLiteral("SsbModMon")));
    QVERIFY(c.registeredIds().contains(QStringLiteral("SsbModMon")));

    // One click turns it on, where it was placed.
    c.setVisible(QStringLiteral("SsbModMon"), true);
    QVERIFY(c.isVisible(QStringLiteral("SsbModMon")));
    index = -1;
    QVERIFY(findType(store.snapshot(), QStringLiteral("applet:ssb_mod_monitor"), nullptr, &index)->visible);
    QCOMPARE(index, 2);
}

void TstAppletVisibilityController::ssb_panel_saved_visible_stays_visible()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
    WorkspaceDocument doc = workspaceWithoutRotor();
    ContentEntry shown = applet(QStringLiteral("applet:ssb_mod_monitor"));
    shown.visible = true;
    doc.containers[1].contents.append(shown);
    saveWorkspace(settings, doc);
    ContainerWorkspaceStore store(settings);
    ContainerContentRegistry registry;
    AppletVisibilityController c;
    c.setWorkspaceAdapter(&store, &registry);
    c.registerApplet(QStringLiteral("SsbModMon"), QStringLiteral("SSB Mod Monitor"), false);
    QVERIFY(c.isVisible(QStringLiteral("SsbModMon")));
    QVERIFY(findType(store.snapshot(), QStringLiteral("applet:ssb_mod_monitor"))->visible);
}

void TstAppletVisibilityController::ssb_panel_first_launch_entry_is_off_and_under_the_am_panel()
{
    ContainerContentRegistry registry;
    QVector<ContentEntry> contents;
    // The order the registry's descriptors place them in, the S-Meter last.
    for (const QString& type : {QStringLiteral("applet:rx"), QStringLiteral("applet:mod_monitor"),
                                QStringLiteral("applet:ssb_mod_monitor"),
                                QStringLiteral("applet:cat"), QStringLiteral("applet:s_meter")}) {
        ContainerContentRegistry::placeFirstLaunchApplet(
            contents, ContainerContentRegistry::firstLaunchEntry(registry.makeEntry(type)));
    }
    int am = -1;
    int ssb = -1;
    for (int i = 0; i < contents.size(); ++i) {
        if (contents[i].typeId == QLatin1String("applet:mod_monitor")) { am = i; }
        if (contents[i].typeId == QLatin1String("applet:ssb_mod_monitor")) { ssb = i; }
    }
    QVERIFY(am >= 0);
    QCOMPARE(ssb, am + 1);
    QVERIFY(contents[am].visible);
    QVERIFY(!contents[ssb].visible);
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

void TstAppletVisibilityController::ticking_applet_shows_its_hidden_container_data()
{
    QTest::addColumn<QString>("flag");
    QTest::newRow("hidden") << QStringLiteral("visible");
    QTest::newRow("disabled") << QStringLiteral("enabled");
    QTest::newRow("hidden on receive") << QStringLiteral("showOnRx");
    QTest::newRow("hidden by a macro") << QStringLiteral("hiddenByMacro");
}

// Bench bug 2026-10-16: the Rotor read ticked under Containers > Applets
// while the container holding it was off the screen. The tick reads off
// while that container is hidden, and ticking it shows the container.
void TstAppletVisibilityController::ticking_applet_shows_its_hidden_container()
{
    QFETCH(QString, flag);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
    WorkspaceDocument doc = workspaceWithoutRotor();
    doc.containers[1].contents.append(applet(QStringLiteral("applet:rotor"), true));
    ContainerDocument& panel = doc.containers[1];
    if (flag == QStringLiteral("visible")) {
        panel.visible = false;
    } else if (flag == QStringLiteral("hiddenByMacro")) {
        panel.config[flag] = true;
    } else {
        panel.config[flag] = false;
    }
    saveWorkspace(settings, doc);
    ContainerWorkspaceStore store(settings);
    ContainerContentRegistry registry;
    AppletVisibilityController c;
    c.setWorkspaceAdapter(&store, &registry);
    registerStationApplets(c);
    const ContainerDocument meters = *findContainer(store.snapshot(), QStringLiteral("meters"));

    QVERIFY2(!c.isVisible(QStringLiteral("Rotor")), "the tick reads off while its container is hidden");

    QSignalSpy bring(&c, &AppletVisibilityController::bringOnScreenRequested);
    c.setVisible(QStringLiteral("Rotor"), true);
    const WorkspaceDocument after = store.snapshot();
    const ContainerDocument* shown = findContainer(after, QStringLiteral("panel"));
    QVERIFY(shown != nullptr);
    QVERIFY(shown->visible);
    QVERIFY(shown->config.value(QStringLiteral("enabled")).toBool(true));
    QVERIFY(shown->config.value(QStringLiteral("showOnRx")).toBool(true));
    QVERIFY(!shown->config.value(QStringLiteral("hiddenByMacro")).toBool(false));
    QVERIFY(findType(after, QStringLiteral("applet:rotor"))->visible);
    QVERIFY(c.isVisible(QStringLiteral("Rotor")));
    QCOMPARE(bring.count(), 1);
    QCOMPARE(bring.first().first().toString(), QStringLiteral("applet:rotor"));
    // Nothing else changes.
    QCOMPARE(*findContainer(after, QStringLiteral("meters")), meters);
}

void TstAppletVisibilityController::unticking_applet_leaves_its_container_alone()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
    WorkspaceDocument doc = workspaceWithoutRotor();
    doc.containers[1].contents.append(applet(QStringLiteral("applet:rotor"), true));
    doc.containers[1].config[QStringLiteral("showOnTx")] = false;
    saveWorkspace(settings, doc);
    ContainerWorkspaceStore store(settings);
    ContainerContentRegistry registry;
    AppletVisibilityController c;
    c.setWorkspaceAdapter(&store, &registry);
    registerStationApplets(c);
    const ContainerDocument before = *findContainer(store.snapshot(), QStringLiteral("panel"));

    QSignalSpy bring(&c, &AppletVisibilityController::bringOnScreenRequested);
    c.setVisible(QStringLiteral("Rotor"), false);
    ContainerDocument expected = before;
    for (ContentEntry& entry : expected.contents) {
        if (entry.typeId == QStringLiteral("applet:rotor")) { entry.visible = false; }
    }
    QCOMPARE(*findContainer(store.snapshot(), QStringLiteral("panel")), expected);
    QVERIFY(!c.isVisible(QStringLiteral("Rotor")));
    QCOMPARE(bring.count(), 0);
}

// Disabled, never hidden: the container shows a grayed applet with the
// reason the window gives.
void TstAppletVisibilityController::unavailable_reason_reaches_the_registry()
{
    ContainerContentRegistry registry;
    AppletVisibilityController c;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
    saveWorkspace(settings, workspaceWithoutRotor());
    ContainerWorkspaceStore store(settings);
    c.setWorkspaceAdapter(&store, &registry);
    registerStationApplets(c);
    const QString reason = QStringLiteral("Turn on 4O3A in Setup to use the Power Genius.");
    c.setAvailable(QStringLiteral("Amp"), false, reason);
    QVERIFY(!registry.isAvailable(QStringLiteral("applet:amp")));
    QCOMPARE(registry.unavailableReason(QStringLiteral("applet:amp")), reason);
    c.setAvailable(QStringLiteral("Amp"), true);
    QVERIFY(registry.isAvailable(QStringLiteral("applet:amp")));
}

// While transmitting, the tick reads a container's showOnTx, as the
// containers themselves do (ContainerManager::effectiveVisible), and
// ticking clears that flag only; showOnRx stays as it is.
void TstAppletVisibilityController::ticking_during_transmit_shows_a_receive_only_container()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
    WorkspaceDocument doc = workspaceWithoutRotor();
    doc.containers[1].contents.append(applet(QStringLiteral("applet:rotor"), true));
    doc.containers[1].config[QStringLiteral("showOnTx")] = false;
    saveWorkspace(settings, doc);
    ContainerWorkspaceStore store(settings);
    ContainerContentRegistry registry;
    AppletVisibilityController c;
    c.setWorkspaceAdapter(&store, &registry);
    registerStationApplets(c);
    QVERIFY2(c.isVisible(QStringLiteral("Rotor")), "on receive the container shows");

    QSignalSpy changed(&c, &AppletVisibilityController::visibilityChanged);
    c.setTransmitting(true);
    QVERIFY2(!c.isVisible(QStringLiteral("Rotor")), "while transmitting its container is hidden");
    // Every applet in that container reads off, the Rotor once.
    const auto rotorChanges = std::count_if(changed.cbegin(), changed.cend(), [](const QList<QVariant>& args) {
        return args.at(0).toString() == QStringLiteral("Rotor") && !args.at(1).toBool();
    });
    QCOMPARE(static_cast<int>(rotorChanges), 1);

    QSignalSpy bring(&c, &AppletVisibilityController::bringOnScreenRequested);
    c.setVisible(QStringLiteral("Rotor"), true);
    const ContainerDocument* shown = findContainer(store.snapshot(), QStringLiteral("panel"));
    QVERIFY(shown != nullptr);
    QVERIFY(shown->config.value(QStringLiteral("showOnTx")).toBool(true));
    QVERIFY(!shown->config.contains(QStringLiteral("showOnRx")));
    QVERIFY(c.isVisible(QStringLiteral("Rotor")));
    QCOMPARE(bring.count(), 1);

    c.setTransmitting(false);
    QVERIFY(c.isVisible(QStringLiteral("Rotor")));
}

// A container shown only while transmitting: the tick reads off on
// receive and on while transmitting. Ticked on receive, it is shown on
// receive too; ticked while transmitting, nothing about it changes.
void TstAppletVisibilityController::ticking_a_transmit_only_container_on_receive_and_transmit()
{
    for (const bool transmitting : {false, true}) {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
        WorkspaceDocument doc = workspaceWithoutRotor();
        doc.containers[1].contents.append(applet(QStringLiteral("applet:rotor"), true));
        doc.containers[1].config[QStringLiteral("showOnRx")] = false;
        doc.containers[1].config[QStringLiteral("showOnTx")] = true;
        saveWorkspace(settings, doc);
        ContainerWorkspaceStore store(settings);
        ContainerContentRegistry registry;
        AppletVisibilityController c;
        c.setWorkspaceAdapter(&store, &registry);
        registerStationApplets(c);
        c.setTransmitting(transmitting);
        QCOMPARE(c.isVisible(QStringLiteral("Rotor")), transmitting);
        const WorkspaceDocument before = store.snapshot();

        QSignalSpy bring(&c, &AppletVisibilityController::bringOnScreenRequested);
        c.setVisible(QStringLiteral("Rotor"), true);
        const ContainerDocument* after = findContainer(store.snapshot(), QStringLiteral("panel"));
        QVERIFY(after != nullptr);
        QVERIFY(after->config.value(QStringLiteral("showOnTx")).toBool(true));
        if (transmitting) {
            QCOMPARE(store.snapshot().revision, before.revision);
            QVERIFY(!after->config.value(QStringLiteral("showOnRx")).toBool(true));
        } else {
            QVERIFY(after->config.value(QStringLiteral("showOnRx")).toBool(true));
        }
        QVERIFY(c.isVisible(QStringLiteral("Rotor")));
        QCOMPARE(bring.count(), 1);
    }
}

QTEST_MAIN(TstAppletVisibilityController)
#include "tst_applet_visibility_controller.moc"
