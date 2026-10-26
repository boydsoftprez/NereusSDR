// no-port-check: NereusSDR-original checks for added copies of the SIGNAL
// STRENGTH dial as container content (workspace preview).
// Modification history (NereusSDR):
//   2026-10-08 - Written for the workspace visual review by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The external amplifier, Setup's face, locked and editing,
//                 and the test-only plate limit. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The plate limit is the product: limited and centred with
//                 no seam, and the parent's background outside the bezel.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - The look has no Lock setting: a dial's container committed
//                 locked stays locked with the look off and is stored
//                 unlocked with it on, and its menu commits in both.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - The appended TX modes read their bindings and are saved
//                 by name, a saved Compression dial loads unchanged, the
//                 power limit property, and the VHF S9 rule of the copy's
//                 slice at 30 MHz. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
#include <QtTest>
#include <QApplication>
#include <QLabel>
#include <QSplitter>
#include <QComboBox>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QMenu>
#include <QScopeGuard>
#include <QTemporaryDir>
#include "core/AppSettings.h"
#include "gui/SMeterWidget.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/ContentPropertyEditor.h"
#include "gui/instruments/SMeterDialContent.h"
#include "gui/setup/AppearanceSetupPages.h"
#include "models/RadioModel.h"
#include "models/MainPowerMeter.h"
#include "models/AmplifierModel.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/AboveS9Frequency.h"
#include "models/SliceModel.h"
#include "gui/styles/WorkspacePreview.h"

using namespace NereusSDR;

namespace {

void settle()
{
    for (int pass = 0; pass < 6; ++pass) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

// The dials a host built for added copies (never the Main Panel's).
QList<SMeterWidget*> dialCopies(const QWidget& host)
{
    QList<SMeterWidget*> out;
    for (SMeterWidget* w : host.findChildren<SMeterWidget*>()) {
        if (w->settingsStore() == SMeterWidget::SettingsStore::Instance) { out.append(w); }
    }
    return out;
}

SMeterWidget* dialFor(const QWidget& host, const QString& entryId)
{
    for (SMeterWidget* w : dialCopies(host)) {
        if (w->property("containerEntryId").toString() == entryId) { return w; }
    }
    return nullptr;
}

bool showsDuplicateLabel(const QWidget& host)
{
    for (const QLabel* l : host.findChildren<QLabel*>()) {
        if (l->text().contains(QLatin1String("duplicate"))) { return true; }
    }
    return false;
}

// The Main Panel's dial keys, as AppSettings holds them now.
QStringList globalDialKeys()
{
    QStringList out;
    for (const char* key : {"SMeter_TxSelect", "SMeter_RxSelect", "PeakHoldEnabled", "PeakDecayRate",
                            "SMeter_FaceStyle"}) {
        const QString k = QString::fromLatin1(key);
        out.append(k + QLatin1Char('=')
                   + (AppSettings::instance().contains(k) ? AppSettings::instance().value(k).toString()
                                                          : QStringLiteral("<absent>")));
    }
    return out;
}

ContainerDocument stationContainer(const QVector<ContentEntry>& contents)
{
    ContainerDocument c;
    c.id = QStringLiteral("station");
    c.name = QStringLiteral("Station");
    c.layout = ContentLayout::VerticalStack;
    c.dockMode = DockMode::OverlayDocked;
    c.geometry = QRect(10, 10, 380, 800);
    c.contents = contents;
    return c;
}

// A station container of `entries` committed through a ContainerManager on
// an isolated store, shown and settled.
struct DialStation {
    DialStation(ContainerContentRegistry& registry, const QVector<ContentEntry>& entries, bool locked = false)
        : settings(directory.filePath(QStringLiteral("dial-station.xml")))
        , store(settings)
        , manager(&dock, &splitter)
    {
        dock.resize(900, 900);
        manager.setWorkspaceAdapter(&store, &registry);
        WorkspaceDocument document;
        document.containers.append(stationContainer(entries));
        document.containers.first().locked = locked;
        document.mainContainerId = QStringLiteral("station");
        committed = manager.commitWorkspace(document, 0);
        dock.show();
        exposed = QTest::qWaitForWindowExposed(&dock);
        settle();
    }
    ContentEntry entry(const QString& id) const
    {
        // The snapshot is held: a range-for over a temporary's member
        // would read a destroyed document.
        const WorkspaceDocument snapshot = store.snapshot();
        for (const auto& e : snapshot.containers.first().contents) {
            if (e.id == id) { return e; }
        }
        return {};
    }
    ContainerContentHost* host() { return manager.contentHost(QStringLiteral("station")); }

    QTemporaryDir directory;
    AppSettings settings;
    ContainerWorkspaceStore store;
    QWidget dock;
    QSplitter splitter;
    ContainerManager manager;
    bool exposed{false};
    CommitResult committed;
};

ContentEntry dialEntry(ContainerContentRegistry& registry, const QString& id, const QJsonObject& config = {})
{
    ContentEntry e = registry.makeEntry(QString::fromLatin1(SMeterDialContent::kDialType));
    e.id = id;
    e.config = config;
    return e;
}

// A Power Genius XL status report as RadioModel receives it from the amp's
// connection (RadioModel::onPgxlStatus): no device, no connection.
void reportPgxl(RadioModel& radio, const QMap<QString, QString>& kvs)
{
    radio.amplifierModel()->applyStatusFrame(kvs);
    using StatusMap = QMap<QString, QString>;
    QVERIFY(QMetaObject::invokeMethod(&radio, "onPgxlStatus", Qt::DirectConnection,
                                      Q_ARG(StatusMap, kvs)));
}

// The action labelled `label` in `menu` or its submenus.
QAction* menuAction(QMenu* menu, const QString& label)
{
    for (QAction* a : menu->actions()) {
        if (a->text() == label) { return a; }
        if (a->menu()) {
            if (QAction* inner = menuAction(a->menu(), label)) { return inner; }
        }
    }
    return nullptr;
}

// SMeter_FaceStyle is written below: only in a test home (ctest gives each
// test its own HOME), never the operator's settings.
bool settingsAreIsolated()
{
    return AppSettings::instance().filePath().contains(QLatin1String("/test-home/"));
}

} // namespace

class TstSMeterDialContent : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { WorkspacePreview::setEnabledForTest(true); }
    void cleanupTestCase() { WorkspacePreview::setEnabledForTest(false); }

    void dialIsListedAsAnAdditionalType()
    {
        ContainerContentRegistry registry;
        SMeterDialContent::registerDial(registry, {});
        bool found = false;
        for (const auto& d : registry.descriptors()) {
            if (d.typeId == QLatin1String(SMeterDialContent::kDialType)) {
                found = true;
                QVERIFY(!d.singleton);
                QVERIFY(d.available);
                QCOMPARE(d.title, QStringLiteral("S-Meter dial (additional)"));
            }
        }
        QVERIFY(found);
        // A new copy starts from today's defaults.
        const ContentEntry entry = registry.makeEntry(QString::fromLatin1(SMeterDialContent::kDialType));
        QVERIFY(registry.validateEntry(entry).isEmpty());
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
        auto* dial = qobject_cast<SMeterWidget*>(widget.get());
        QVERIFY(dial);
        QCOMPARE(dial->settingsStore(), SMeterWidget::SettingsStore::Instance);
        QCOMPARE(dial->txMode(), SMeterWidget::TxMode::Power);
        QCOMPARE(dial->rxMode(), SMeterWidget::RxMode::SMeter);
        QVERIFY(dial->peakHoldEnabled());
        QCOMPARE(dial->peakDecayRate(), SMeterWidget::DecayRate::Medium);
        QCOMPARE(dial->faceStyle(), SMeterWidget::FaceStyle::AgedCream);
        // No reading yet: "--", as a Longpath needle is offline.
        QCOMPARE(dial->sUnitsText(), QStringLiteral("--"));
    }

    void twoCopiesAreBothAlive()
    {
        MeterPoller poller;
        ContainerContentRegistry registry;
        SMeterWidget mainPanelDial;
        registry.attachSingleton(QStringLiteral("applet:s_meter"), &mainPanelDial);
        LongpathContent::Sources sources;
        sources.poller = &poller;
        SMeterDialContent::registerDial(registry, sources);

        QTemporaryDir directory;
        AppSettings settings{directory.filePath("dial-two.xml")};
        ContainerWorkspaceStore store{settings};
        QWidget dock;
        dock.resize(900, 900);
        QSplitter splitter;
        ContainerManager manager{&dock, &splitter};
        manager.setWorkspaceAdapter(&store, &registry);
        ContentEntry a = registry.makeEntry(QString::fromLatin1(SMeterDialContent::kDialType));
        ContentEntry b = registry.makeEntry(QString::fromLatin1(SMeterDialContent::kDialType));
        a.id = QStringLiteral("dial-a");
        b.id = QStringLiteral("dial-b");
        WorkspaceDocument document;
        document.containers.append(stationContainer({a, b}));
        document.mainContainerId = QStringLiteral("station");
        const auto committed = manager.commitWorkspace(document, 0);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        settle();
        auto* host = manager.contentHost(QStringLiteral("station"));
        QVERIFY(host);
        QCOMPARE(dialCopies(*host).size(), 2);
        SMeterWidget* first = dialFor(*host, a.id);
        SMeterWidget* second = dialFor(*host, b.id);
        QVERIFY(first && second && first != second);
        QVERIFY(first->isVisible() && second->isVisible());
        QVERIFY(first->height() >= 130 && second->height() >= 130);
        QVERIFY(!showsDuplicateLabel(*host));
        // The Main Panel's dial stays where it was.
        QVERIFY(!host->isAncestorOf(&mainPanelDial));
        QCOMPARE(poller.readingConsumerCountForTest(), 2);
    }

    void eachCopyFollowsItsOwnSource()
    {
        MeterPoller poller;
        ContainerContentRegistry registry;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        SMeterDialContent::registerDial(registry, sources);

        QTemporaryDir directory;
        AppSettings settings{directory.filePath("dial-source.xml")};
        ContainerWorkspaceStore store{settings};
        QWidget dock;
        dock.resize(900, 900);
        QSplitter splitter;
        ContainerManager manager{&dock, &splitter};
        manager.setWorkspaceAdapter(&store, &registry);
        ContentEntry a = registry.makeEntry(QString::fromLatin1(SMeterDialContent::kDialType));
        ContentEntry b = registry.makeEntry(QString::fromLatin1(SMeterDialContent::kDialType));
        a.id = QStringLiteral("dial-rx-a");
        b.id = QStringLiteral("dial-rx-b");
        a.context = QJsonObject{{QStringLiteral("rxSource"), 1}};
        b.context = QJsonObject{{QStringLiteral("rxSource"), 2}};
        WorkspaceDocument document;
        document.containers.append(stationContainer({a, b}));
        document.mainContainerId = QStringLiteral("station");
        auto committed = manager.commitWorkspace(document, 0);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        settle();
        auto* host = manager.contentHost(QStringLiteral("station"));
        QVERIFY(host);
        SMeterWidget* dialA = dialFor(*host, a.id);
        SMeterWidget* dialB = dialFor(*host, b.id);
        QVERIFY(dialA && dialB);
        const QJsonObject contextA = dialA->property("containerSourceContext").toJsonObject();
        const QJsonObject contextB = dialB->property("containerSourceContext").toJsonObject();
        QVERIFY(contextA != contextB);

        emit poller.readingUpdated(contextA, MeterBinding::SignalPeak, -63.0);
        emit poller.readingUpdated(contextB, MeterBinding::SignalPeak, -103.0);
        QCOMPARE(dialA->levelDbm(), -63.0f);
        QCOMPARE(dialB->levelDbm(), -103.0f);
        // A reading for a binding the RX mode does not read changes nothing.
        emit poller.readingUpdated(contextA, MeterBinding::SignalAvg, -20.0);
        QCOMPARE(dialA->levelDbm(), -63.0f);

        // The receiver behind A changed: A drops its value, B keeps its own.
        emit poller.rxSourceChanged(contextA);
        QCOMPARE(dialA->sUnitsText(), QStringLiteral("--"));
        QCOMPARE(dialB->levelDbm(), -103.0f);
        // A's receiver closed: no reading, "--".
        emit poller.readingUpdated(contextA, MeterBinding::SignalPeak, kNoMeterReadingDbm);
        QCOMPARE(dialA->sUnitsText(), QStringLiteral("--"));

        // Changing A's source moves it: B's context now reaches A.
        document.containers[0].contents[0].context = QJsonObject{{QStringLiteral("rxSource"), 2}};
        committed = manager.commitWorkspace(document, committed.revision);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        settle();
        host = manager.contentHost(QStringLiteral("station"));
        dialA = dialFor(*host, a.id);
        QVERIFY(dialA);
        QCOMPARE(dialA->property("containerSourceContext").toJsonObject(), contextB);
        emit poller.readingUpdated(contextB, MeterBinding::SignalPeak, -71.0);
        QCOMPARE(dialA->levelDbm(), -71.0f);
    }

    void settingsLiveInTheEntryNotTheGlobalKeys()
    {
        const QStringList globalBefore = globalDialKeys();
        MeterPoller poller;
        ContainerContentRegistry registry;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        SMeterDialContent::registerDial(registry, sources);

        QTemporaryDir directory;
        AppSettings settings{directory.filePath("dial-settings.xml")};
        ContainerWorkspaceStore store{settings};
        QWidget dock;
        dock.resize(900, 900);
        QSplitter splitter;
        ContainerManager manager{&dock, &splitter};
        manager.setWorkspaceAdapter(&store, &registry);
        ContentEntry a = registry.makeEntry(QString::fromLatin1(SMeterDialContent::kDialType));
        ContentEntry b = registry.makeEntry(QString::fromLatin1(SMeterDialContent::kDialType));
        a.id = QStringLiteral("dial-edit");
        b.id = QStringLiteral("dial-keep");
        WorkspaceDocument document;
        document.containers.append(stationContainer({a, b}));
        document.mainContainerId = QStringLiteral("station");
        const auto committed = manager.commitWorkspace(document, 0);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        settle();
        auto* host = manager.contentHost(QStringLiteral("station"));
        QVERIFY(host);
        SMeterWidget* dial = dialFor(*host, a.id);
        QVERIFY(dial);
        // One poll fills the poller's cache for the dial's context: Signal
        // Peak -60 dBm (shown) and Sig Avg -88 dBm (cached).
        poller.setRxReadingSource([](const QJsonObject&, int binding) {
            return binding == MeterBinding::SignalAvg ? -88.0 : -60.0;
        });
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(dial->levelDbm(), -60.0f);

        // The right-click menu's setters.
        dial->setTxMode(QStringLiteral("SWR"));
        dial->setRxMode(QStringLiteral("Sig Avg"));
        dial->setPeakHoldEnabled(false);
        dial->setPeakDecayRate(QStringLiteral("Slow"));
        dial->setFaceStyle(SMeterWidget::FaceStyle::VuAmber);
        settle();
        // The new RX mode shows its binding's cached reading at once.
        QCOMPARE(dial->levelDbm(), -88.0f);

        const auto findEntry = [](const WorkspaceDocument& d, const QString& id) {
            for (const auto& e : d.containers.first().contents) {
                if (e.id == id) { return e; }
            }
            return ContentEntry{};
        };
        const WorkspaceDocument saved = store.snapshot();
        const QJsonObject config = findEntry(saved, a.id).config;
        QCOMPARE(config.value(QLatin1String(SMeterDialContent::kTxModeKey)).toString(), QStringLiteral("SWR"));
        QCOMPARE(config.value(QLatin1String(SMeterDialContent::kRxModeKey)).toString(), QStringLiteral("Sig Avg"));
        QCOMPARE(config.value(QLatin1String(SMeterDialContent::kPeakHoldKey)), QJsonValue(false));
        QCOMPARE(config.value(QLatin1String(SMeterDialContent::kPeakDecayKey)).toString(), QStringLiteral("Slow"));
        QCOMPARE(config.value(QLatin1String(SMeterDialContent::kFaceKey)).toString(),
                 SMeterWidget::faceStyleKey(SMeterWidget::FaceStyle::VuAmber));
        // The other copy and the Main Panel's keys are untouched.
        QVERIFY(findEntry(saved, b.id).config.isEmpty());
        QCOMPARE(globalDialKeys(), globalBefore);
        // The edit did not rebuild the dial it came from.
        QCOMPARE(dialFor(*host, a.id), dial);

        // Reloaded from the stored document, a copy comes back as it was left.
        ContainerWorkspaceStore reloadStore{settings};
        const DocumentResult loaded = reloadStore.load();
        QVERIFY2(loaded.ok, qPrintable(loaded.error));
        const ContentEntry reloaded = findEntry(loaded.document, a.id);
        QCOMPARE(reloaded.config, config);
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(reloaded, &parent));
        auto* again = qobject_cast<SMeterWidget*>(widget.get());
        QVERIFY(again);
        QCOMPARE(again->txMode(), SMeterWidget::TxMode::SWR);
        QCOMPARE(again->rxMode(), SMeterWidget::RxMode::SignalAverage);
        QVERIFY(!again->peakHoldEnabled());
        QCOMPARE(again->peakDecayRate(), SMeterWidget::DecayRate::Slow);
        QCOMPARE(again->faceStyle(), SMeterWidget::FaceStyle::VuAmber);
        QCOMPARE(globalDialKeys(), globalBefore);
    }

    // A copy follows the external amplifier as the Main Panel's dial does
    // (MainWindow.cpp:9505-9612): the 2 kW scale and the amplifier's watts
    // while it amplifies, the radio's own power held back meanwhile, and
    // back to the radio's scale and power when it stands by.
    void copiesFollowTheExternalAmplifier()
    {
        MeterPoller poller;
        RadioModel radio;
        poller.setRadioStatus(&radio.radioStatus());
        poller.setMainPowerMeter(radio.mainPowerMeter());
        ContainerContentRegistry registry;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        sources.radio = &radio;
        SMeterDialContent::registerDial(registry, sources);
        DialStation station(registry, {dialEntry(registry, QStringLiteral("dial-amp"))});
        QVERIFY2(station.committed.status == CommitStatus::Saved, qPrintable(station.committed.error));
        QVERIFY(station.exposed);
        SMeterWidget* dial = dialFor(*station.host(), QStringLiteral("dial-amp"));
        QVERIFY(dial);
        const QJsonObject context = dial->property("containerSourceContext").toJsonObject();
        dial->setTransmitting(true);
        QVERIFY(!dial->txScaleIs2kWForTesting());
        radio.mainPowerMeter()->setHardwareModel(HPSDRModel::HERMESLITE);
        QCOMPARE(dial->testPowerScaleMax(), 6.0f);
        QCOMPARE(dial->testPowerRedStart(), 5.0f);
        radio.radioStatus().setPowerReadings(85.0, 0.0, 1.25);
        QCOMPARE(dial->testTxValue(), 85.0f);

        // The amp reports OPERATE and 1500 W (61.76 dBm), return loss 20 dB.
        reportPgxl(radio, {{QStringLiteral("state"), QStringLiteral("TRANSMIT_A")},
                           {QStringLiteral("peakfwd"), QStringLiteral("61.7609")},
                           {QStringLiteral("swr"), QStringLiteral("-20")}});
        QVERIFY(radio.isAnyExternalAmpInOperate());
        QVERIFY(dial->txScaleIs2kWForTesting());
        radio.mainPowerMeter()->setHardwareModel(HPSDRModel::ANAN200D);
        QCOMPARE(dial->testPowerScaleMax(), 2000.0f);
        QCOMPARE(dial->testPowerRedStart(), 1500.0f);
        QVERIFY2(qAbs(dial->testTxValue() - 1500.0f) < 1.0f, qPrintable(QString::number(dial->testTxValue())));
        // The radio's own forward power does not overwrite the amp's.
        radio.radioStatus().setPowerReadings(86.0, 0.0, 1.25);
        QVERIFY2(qAbs(dial->testTxValue() - 1500.0f) < 1.0f, qPrintable(QString::number(dial->testTxValue())));

        // A copy added while the amp amplifies starts on its scale.
        QWidget parent;
        std::unique_ptr<QWidget> later(registry.createWidget(dialEntry(registry, QStringLiteral("dial-later")), &parent));
        auto* laterDial = qobject_cast<SMeterWidget*>(later.get());
        QVERIFY(laterDial);
        QVERIFY(laterDial->txScaleIs2kWForTesting());

        // STANDBY: the radio's scale and its own power again.
        reportPgxl(radio, {{QStringLiteral("state"), QStringLiteral("STANDBY")}});
        QVERIFY(!radio.isAnyExternalAmpInOperate());
        QVERIFY(!dial->txScaleIs2kWForTesting());
        QVERIFY(!laterDial->txScaleIs2kWForTesting());
        QCOMPARE(dial->testPowerScaleMax(), 240.0f);
        QCOMPARE(dial->testPowerRedStart(), 200.0f);
        QCOMPARE(laterDial->testPowerScaleMax(), 240.0f);
        radio.radioStatus().setPowerReadings(90.0, 0.0, 1.25);
        QCOMPARE(dial->testTxValue(), 90.0f);
    }

    // A copy with no face key draws Setup's Meter Styles face and follows
    // it; once a face is picked from its own menu it keeps that face. A
    // stored face key is the copy's own. The Main Panel's dial is as before.
    void faceFollowsSetupUntilTheCopyPicksItsOwn()
    {
        if (!settingsAreIsolated()) {
            QSKIP("writes SMeter_FaceStyle: run through ctest (its own test home)");
        }
        AppSettings& global = AppSettings::instance();
        const QString faceKey = QStringLiteral("SMeter_FaceStyle");
        global.remove(faceKey);
        auto restore = qScopeGuard([&global, faceKey] { global.remove(faceKey); });

        MeterPoller poller;
        ContainerContentRegistry registry;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        SMeterDialContent::registerDial(registry, sources);
        SMeterWidget mainPanelDial;
        const QString ownFace = SMeterWidget::faceStyleKey(SMeterWidget::FaceStyle::Ice);
        DialStation station(registry, {dialEntry(registry, QStringLiteral("dial-follow")),
                                       dialEntry(registry, QStringLiteral("dial-picks")),
                                       dialEntry(registry, QStringLiteral("dial-stored"),
                                                 {{QLatin1String(SMeterDialContent::kFaceKey), ownFace}})});
        QVERIFY2(station.committed.status == CommitStatus::Saved, qPrintable(station.committed.error));
        QVERIFY(station.exposed);
        SMeterWidget* follow = dialFor(*station.host(), QStringLiteral("dial-follow"));
        SMeterWidget* picks = dialFor(*station.host(), QStringLiteral("dial-picks"));
        SMeterWidget* stored = dialFor(*station.host(), QStringLiteral("dial-stored"));
        QVERIFY(follow && picks && stored);
        QCOMPARE(follow->faceStyle(), SMeterWidget::FaceStyle::AgedCream);
        QCOMPARE(stored->faceStyle(), SMeterWidget::FaceStyle::Ice);

        // Setup > Appearance > Meter Styles: Face Blackface.
        MeterStylesPage page(nullptr);
        auto* combo = page.findChild<QComboBox*>(QStringLiteral("sMeterFaceCombo"));
        QVERIFY(combo);
        combo->setCurrentIndex(combo->findData(static_cast<int>(SMeterWidget::FaceStyle::Blackface)));
        QCOMPARE(global.value(faceKey).toString(), SMeterWidget::faceStyleKey(SMeterWidget::FaceStyle::Blackface));
        QCOMPARE(follow->faceStyle(), SMeterWidget::FaceStyle::Blackface);
        QCOMPARE(picks->faceStyle(), SMeterWidget::FaceStyle::Blackface);
        QCOMPARE(stored->faceStyle(), SMeterWidget::FaceStyle::Ice);

        // The second copy picks Carbon from its own menu.
        std::unique_ptr<QMenu> menu(picks->buildContextMenuForTesting());
        QAction* carbon = menuAction(menu.get(), SMeterWidget::faceStyleLabel(SMeterWidget::FaceStyle::Carbon));
        QVERIFY(carbon);
        carbon->trigger();
        settle();
        picks = dialFor(*station.host(), QStringLiteral("dial-picks"));
        QVERIFY(picks);
        QCOMPARE(picks->faceStyle(), SMeterWidget::FaceStyle::Carbon);
        QCOMPARE(station.entry(QStringLiteral("dial-picks")).config.value(QLatin1String(SMeterDialContent::kFaceKey)).toString(),
                 SMeterWidget::faceStyleKey(SMeterWidget::FaceStyle::Carbon));
        // Setup's face is not written by a copy.
        QCOMPARE(global.value(faceKey).toString(), SMeterWidget::faceStyleKey(SMeterWidget::FaceStyle::Blackface));

        // The first copy changes another setting: still no face key, still
        // following.
        follow->setTxMode(QStringLiteral("SWR"));
        settle();
        follow = dialFor(*station.host(), QStringLiteral("dial-follow"));
        QVERIFY(follow);
        const QJsonObject followConfig = station.entry(QStringLiteral("dial-follow")).config;
        QCOMPARE(followConfig.value(QLatin1String(SMeterDialContent::kTxModeKey)).toString(), QStringLiteral("SWR"));
        QVERIFY(!followConfig.contains(QLatin1String(SMeterDialContent::kFaceKey)));

        // The Main Panel's dial changes the face (its menu writes the key):
        // the follower follows; the copies with a face of their own do not.
        mainPanelDial.setFaceStyle(SMeterWidget::FaceStyle::VuAmber);
        QCOMPARE(global.value(faceKey).toString(), SMeterWidget::faceStyleKey(SMeterWidget::FaceStyle::VuAmber));
        QCOMPARE(follow->faceStyle(), SMeterWidget::FaceStyle::VuAmber);
        QCOMPARE(picks->faceStyle(), SMeterWidget::FaceStyle::Carbon);
        QCOMPARE(stored->faceStyle(), SMeterWidget::FaceStyle::Ice);
        QVERIFY(!station.entry(QStringLiteral("dial-follow")).config.contains(QLatin1String(SMeterDialContent::kFaceKey)));
    }

    // Locked and editing, as the Longpath needle: a locked container (the
    // workspace look off; the look has no Lock) locks
    // its arrangement, not its contents' settings (Container Settings'
    // property editor still edits a needle's properties), so the dial's
    // menu opens and its change commits; while Container Settings edits
    // the workspace (application-modal) its preview copies take no mouse
    // input and commit nothing, as the needle's preview.
    void lockedAndEditingMatchTheNeedle()
    {
        MeterPoller poller;
        ContainerContentRegistry registry;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        LongpathContent::registerAll(registry, sources);
        SMeterDialContent::registerDial(registry, sources);

        // The needle in a locked container: its properties stay editable.
        ContentEntry needle = registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType));
        needle.id = QStringLiteral("needle-locked");
        ContentPropertyEditor editor(registry);
        editor.setGeometryLocked(true);
        editor.setEntry(needle);
        QStringList needleEdits;
        connect(&editor, &ContentPropertyEditor::entryEdited, this,
                [&needleEdits](const ContentEntry& e) { needleEdits.append(e.name); });
        auto* name = editor.findChild<QLineEdit*>(QStringLiteral("contentName"));
        QVERIFY(name);
        QVERIFY(name->isEnabled());
        QTest::keyClicks(name, QStringLiteral("Peak"));
        QVERIFY(!needleEdits.isEmpty());
        QVERIFY(needleEdits.last().endsWith(QStringLiteral("Peak")));

        // The dial in a container committed locked: the menu is offered and
        // commits. With the workspace look off the container stays locked
        // through the commit. The look has no Lock setting
        // (ContainerManager::lockApplies), so there the same container is
        // stored unlocked, and the menu commits all the same.
        for (const bool look : {false, true}) {
            WorkspacePreview::setEnabledForTest(look);
            auto restore = qScopeGuard([] { WorkspacePreview::setEnabledForTest(true); });
            const char* const which = look ? "workspace look on" : "workspace look off";
            DialStation station(registry, {dialEntry(registry, QStringLiteral("dial-locked"))}, true);
            QVERIFY2(station.committed.status == CommitStatus::Saved, qPrintable(station.committed.error));
            QVERIFY2(station.exposed, which);
            QVERIFY2(station.store.snapshot().containers.first().locked == !look, which);
            SMeterWidget* dial = dialFor(*station.host(), QStringLiteral("dial-locked"));
            QVERIFY2(dial, which);
            std::unique_ptr<QMenu> menu(dial->buildContextMenuForTesting());
            QAction* swr = menuAction(menu.get(), QStringLiteral("SWR"));
            QVERIFY2(swr, which);
            QVERIFY2(swr->isEnabled(), which);
            swr->trigger();
            settle();
            QCOMPARE(station.entry(QStringLiteral("dial-locked")).config.value(QLatin1String(SMeterDialContent::kTxModeKey)).toString(),
                     QStringLiteral("SWR"));
            QVERIFY2(station.store.snapshot().containers.first().locked == !look, which);
        }

        // Container Settings' preview: neither takes the mouse, neither commits.
        QWidget previewParent;
        std::unique_ptr<QWidget> needlePreview(registry.createPreview(needle, &previewParent));
        std::unique_ptr<QWidget> dialPreview(registry.createPreview(dialEntry(registry, QStringLiteral("dial-preview")), &previewParent));
        QVERIFY(needlePreview && dialPreview);
        QVERIFY(qobject_cast<SMeterWidget*>(dialPreview.get()));
        QVERIFY(needlePreview->testAttribute(Qt::WA_TransparentForMouseEvents));
        QVERIFY(dialPreview->testAttribute(Qt::WA_TransparentForMouseEvents));
        QVERIFY(!needlePreview->findChild<ContentConfigNotifier*>());
        QVERIFY(!dialPreview->findChild<ContentConfigNotifier*>());
    }

    // The plate limit is the product (JJ's decision D20): a dial wider than
    // kPlateMaxAspect times its height gets a centred plate that wide; one
    // at the Main Panel's own size (298 x 140) keeps its whole rect. A
    // vintage face paints only its bezel, so it is never opaque; the Classic
    // face still fills its rect.
    void plateIsLimitedAndCentred()
    {
        QWidget parent;
        SMeterWidget mainSize(SMeterWidget::SettingsStore::Instance, &parent);
        SMeterWidget wide(SMeterWidget::SettingsStore::Instance, &parent);
        SMeterWidget middle(SMeterWidget::SettingsStore::Instance, &parent);
        mainSize.setGeometry(0, 0, 298, 140);
        wide.setGeometry(0, 150, 1000, 140);
        middle.setGeometry(0, 300, 600, 140);
        QCOMPARE(mainSize.plateRect(), QRectF(0, 0, 298, 140));
        QCOMPARE(wide.plateRect(), QRectF(351, 0, 298, 140));
        QCOMPARE(middle.plateRect(), QRectF(151, 0, 298, 140));
        QVERIFY(!mainSize.testAttribute(Qt::WA_OpaquePaintEvent));
        QVERIFY(!wide.testAttribute(Qt::WA_OpaquePaintEvent));
        wide.setFaceStyle(SMeterWidget::FaceStyle::Classic);
        QVERIFY(wide.testAttribute(Qt::WA_OpaquePaintEvent));
        wide.setFaceStyle(SMeterWidget::FaceStyle::AgedCream);
        QVERIFY(!wide.testAttribute(Qt::WA_OpaquePaintEvent));
    }

    // Outside the bezel a dial paints nothing: the parent's background
    // (here a user colour) shows at the plate's rounded corners and in the
    // bands beside a limited plate, with the workspace preview off and on.
    void parentBackgroundShowsOutsideTheBezel_data()
    {
        QTest::addColumn<bool>("preview");
        QTest::newRow("preview off") << false;
        QTest::newRow("preview on") << true;
    }
    void parentBackgroundShowsOutsideTheBezel()
    {
        QFETCH(bool, preview);
        WorkspacePreview::setEnabledForTest(preview);
        auto restore = qScopeGuard([] { WorkspacePreview::setEnabledForTest(false); });
        const QColor user(0x2a, 0x1e, 0x14);
        QWidget parent;
        parent.setAutoFillBackground(true);
        QPalette pal = parent.palette();
        pal.setColor(QPalette::Window, user);
        parent.setPalette(pal);
        parent.resize(1000, 290);
        SMeterWidget mainSize(SMeterWidget::SettingsStore::Instance, &parent);
        SMeterWidget wide(SMeterWidget::SettingsStore::Instance, &parent);
        mainSize.setGeometry(0, 0, 298, 140);
        wide.setGeometry(0, 150, 1000, 140);
        const QImage img = parent.grab().toImage().convertToFormat(QImage::Format_ARGB32);
        struct Probe { QPoint at; const char* where; };
        const Probe probes[] = {
            {{0, 0}, "Main Panel size, top-left corner"},
            {{2, 2}, "Main Panel size, inside the top-left corner"},
            {{297, 139}, "Main Panel size, bottom-right corner"},
            {{351, 150}, "wide copy, plate's top-left corner"},
            {{353, 152}, "wide copy, inside the plate's top-left corner"},
            {{10, 220}, "wide copy, band left of the plate"},
            {{990, 220}, "wide copy, band right of the plate"},
        };
        for (const Probe& probe : probes) {
            QVERIFY2(img.pixelColor(probe.at).rgb() == user.rgb(),
                     qPrintable(QStringLiteral("%1 at (%2,%3): %4, expected %5")
                                    .arg(QString::fromLatin1(probe.where))
                                    .arg(probe.at.x()).arg(probe.at.y())
                                    .arg(img.pixelColor(probe.at).name(), user.name())));
        }
        // The plate itself is painted.
        QVERIFY(img.pixelColor(149, 70).rgb() != user.rgb());
        QVERIFY(img.pixelColor(500, 220).rgb() != user.rgb());
    }

    void savedSingletonWorkspaceLoadsUnchanged()
    {
        // A workspace saved before the dial was addable: the Main Panel's
        // dial (applet:s_meter) in its container.
        QTemporaryDir directory;
        AppSettings settings{directory.filePath("dial-legacy.xml")};
        WorkspaceDocument original;
        {
            ContainerContentRegistry before;
            SMeterWidget unused;
            before.attachSingleton(QStringLiteral("applet:s_meter"), &unused);
            ContainerWorkspaceStore store{settings};
            ContentEntry singleton = before.makeEntry(QStringLiteral("applet:s_meter"));
            singleton.id = QStringLiteral("main-s-meter");
            original.containers.append(stationContainer({singleton}));
            original.mainContainerId = QStringLiteral("station");
            const auto saved = store.commit(original, 0);
            QVERIFY2(saved.status == CommitStatus::Saved, qPrintable(saved.error));
            original = store.snapshot();
        }
        ContainerWorkspaceStore probe{settings};
        const QString key = probe.workspaceKey();
        const QString rawBefore = settings.value(key).toString();
        QVERIFY(!rawBefore.isEmpty());

        // Loaded with the dial registered.
        MeterPoller poller;
        ContainerContentRegistry registry;
        SMeterWidget mainPanelDial;
        registry.attachSingleton(QStringLiteral("applet:s_meter"), &mainPanelDial);
        LongpathContent::Sources sources;
        sources.poller = &poller;
        SMeterDialContent::registerDial(registry, sources);
        ContainerWorkspaceStore store{settings};
        const DocumentResult loaded = store.load();
        QVERIFY2(loaded.ok, qPrintable(loaded.error));
        QCOMPARE(loaded.document, original);
        QWidget dock;
        dock.resize(900, 900);
        QSplitter splitter;
        ContainerManager manager{&dock, &splitter};
        manager.setWorkspaceAdapter(&store, &registry);
        manager.reconcileWorkspace(loaded.document);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        settle();
        auto* host = manager.contentHost(QStringLiteral("station"));
        QVERIFY(host);
        QVERIFY(host->isAncestorOf(&mainPanelDial));
        QCOMPARE(mainPanelDial.settingsStore(), SMeterWidget::SettingsStore::Global);
        QVERIFY(dialCopies(*host).isEmpty());
        QVERIFY(!showsDuplicateLabel(*host));
        QCOMPARE(store.snapshot(), original);
        QCOMPARE(settings.value(key).toString(), rawBefore);
    }

    // ALC Compression reads TxAlcGain, ALC Group TxAlcGroup, Volts HwVolts
    // and Amps HwAmps; each is kept in the entry by its menu name.
    void appendedTxModesReadTheirBindings()
    {
        MeterPoller poller;
        ContainerContentRegistry registry;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        SMeterDialContent::registerDial(registry, sources);
        ContentEntry entry = dialEntry(registry, QStringLiteral("dial-alc"),
                                       QJsonObject{{QLatin1String(SMeterDialContent::kTxModeKey),
                                                    QStringLiteral("ALC Compression")}});
        entry.context = QJsonObject{{QStringLiteral("rxSource"), 1}};
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
        auto* dial = qobject_cast<SMeterWidget*>(widget.get());
        QVERIFY(dial);
        QCOMPARE(dial->txMode(), SMeterWidget::TxMode::Alc);
        dial->setTransmitting(true);

        emit poller.readingUpdated(entry.context, MeterBinding::TxAlcGain, 12.0);
        emit poller.readingUpdated(entry.context, MeterBinding::TxAlcGroup, -6.0);
        emit poller.readingUpdated(entry.context, MeterBinding::HwVolts, 13.8);
        emit poller.readingUpdated(entry.context, MeterBinding::HwAmps, 10.0);
        // Another context's reading reaches nothing.
        emit poller.readingUpdated(QJsonObject{{QStringLiteral("rxSource"), 2}}, MeterBinding::TxAlcGain, 29.0);
        QCOMPARE(dial->testTxValue(), 12.0f);
        QCOMPARE(dial->testTxReadout(), QStringLiteral("12 dB"));
        QCOMPARE(dial->testNeedleTarget(), 0.4f);

        const auto savedTxMode = [dial] {
            return dial->property(ContentConfigNotifier::kAppliedConfigProperty).toJsonObject()
                .value(QLatin1String(SMeterDialContent::kTxModeKey)).toString();
        };
        dial->setTxMode(QStringLiteral("ALC Group"));
        QCOMPARE(dial->txMode(), SMeterWidget::TxMode::AlcGroup);
        QCOMPARE(dial->testTxValue(), -6.0f);
        QCOMPARE(savedTxMode(), QStringLiteral("ALC Group"));
        dial->setTxMode(QStringLiteral("Volts"));
        QCOMPARE(dial->txMode(), SMeterWidget::TxMode::Volts);
        QVERIFY(qAbs(dial->testTxValue() - 13.8f) < 1e-4f);
        QCOMPARE(dial->testTxReadout(), QStringLiteral("13.8 V"));
        QCOMPARE(savedTxMode(), QStringLiteral("Volts"));
        dial->setTxMode(QStringLiteral("Amps"));
        QCOMPARE(dial->testTxValue(), 10.0f);
        QCOMPARE(dial->testTxReadout(), QStringLiteral("10.0 A"));
        QCOMPARE(dial->testNeedleTarget(), 0.5f);
        QCOMPARE(savedTxMode(), QStringLiteral("Amps"));

        // No reading: the reading Thetis shows with none (0 A).
        emit poller.readingUpdated(entry.context, MeterBinding::HwAmps, kNoMeterReadingDbm);
        QCOMPARE(dial->testTxValue(), 0.0f);
        // A board without the reading: "--" with the reason.
        emit poller.bindingAvailabilityChanged(MeterBinding::HwAmps, QStringLiteral("No current reading"));
        QCOMPARE(dial->txModeUnavailableReason(SMeterWidget::TxMode::Amps), QStringLiteral("No current reading"));
        QCOMPARE(dial->testTxReadout(), QStringLiteral("--"));

        // The copy registers the four bindings with the poller.
        const QList<int> bindings = SMeterDialContent::registeredBindings();
        for (int b : {MeterBinding::TxAlcGain, MeterBinding::TxAlcGroup, MeterBinding::HwVolts,
                      MeterBinding::HwAmps}) {
            QVERIFY(bindings.contains(b));
        }
    }

    // The copy's right-click menu offers the appended modes in sections;
    // the Main Panel's dial keeps its four.
    void copyMenuOffersTheAppendedModes()
    {
        ContainerContentRegistry registry;
        SMeterDialContent::registerDial(registry, {});
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(dialEntry(registry, QStringLiteral("dial-menu")), &parent));
        auto* dial = qobject_cast<SMeterWidget*>(widget.get());
        QVERIFY(dial);
        std::unique_ptr<QMenu> menu(dial->buildContextMenuForTesting());
        QMenu* txMenu = menu->actions().at(0)->menu();
        QVERIFY(txMenu);
        QStringList labels;
        for (QAction* a : txMenu->actions()) {
            labels.append(a->isSeparator() ? QStringLiteral("|") : a->text());
        }
        QCOMPARE(labels, (QStringList{QStringLiteral("Power"), QStringLiteral("SWR"), QStringLiteral("|"),
                                      QStringLiteral("Level"), QStringLiteral("Compression"),
                                      QStringLiteral("ALC Compression"), QStringLiteral("|"),
                                      QStringLiteral("ALC Group"), QStringLiteral("|"),
                                      QStringLiteral("Volts"), QStringLiteral("Amps")}));
        QAction* volts = menuAction(menu.get(), QStringLiteral("Volts"));
        QVERIFY(volts);
        volts->trigger();
        QCOMPARE(dial->txMode(), SMeterWidget::TxMode::Volts);

        // The property editor offers the same names.
        const QVector<ContentProperty> properties =
            registry.declaredProperties(QString::fromLatin1(SMeterDialContent::kDialType));
        QStringList choices;
        for (const ContentProperty& p : properties) {
            if (p.key == QLatin1String(SMeterDialContent::kTxModeKey)) {
                for (const ContentPropertyChoice& c : p.choices) { choices.append(c.value.toString()); }
            }
        }
        QCOMPARE(choices, (QStringList{QStringLiteral("Power"), QStringLiteral("SWR"), QStringLiteral("Level"),
                                       QStringLiteral("Compression"), QStringLiteral("ALC Compression"),
                                       QStringLiteral("ALC Group"), QStringLiteral("Volts"), QStringLiteral("Amps")}));
    }

    // A dial saved with TX mode Compression, before the modes were
    // appended, loads as it was.
    void savedCompressionDialLoadsUnchanged()
    {
        QCOMPARE(static_cast<int>(SMeterWidget::TxMode::Power), 0);
        QCOMPARE(static_cast<int>(SMeterWidget::TxMode::SWR), 1);
        QCOMPARE(static_cast<int>(SMeterWidget::TxMode::Level), 2);
        QCOMPARE(static_cast<int>(SMeterWidget::TxMode::Compression), 3);
        MeterPoller poller;
        ContainerContentRegistry registry;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        SMeterDialContent::registerDial(registry, sources);
        const QJsonObject saved{{QLatin1String(SMeterDialContent::kTxModeKey), QStringLiteral("Compression")},
                                {QLatin1String(SMeterDialContent::kRxModeKey), QStringLiteral("Sig Avg")}};
        DialStation station(registry, {dialEntry(registry, QStringLiteral("dial-comp"), saved)});
        QVERIFY2(station.committed.status == CommitStatus::Saved, qPrintable(station.committed.error));
        QVERIFY(station.exposed);
        SMeterWidget* dial = dialFor(*station.host(), QStringLiteral("dial-comp"));
        QVERIFY(dial);
        QCOMPARE(dial->txMode(), SMeterWidget::TxMode::Compression);
        QCOMPARE(dial->rxMode(), SMeterWidget::RxMode::SignalAverage);
        QCOMPARE(station.entry(QStringLiteral("dial-comp")).config, saved);
    }

    // "Power limit" 50 W: red from 50 W and full scale at it, so 25 W sits
    // at mid-scale; the radio's scale no longer moves this copy's.
    void powerLimitPutsHalfOfItAtMidScale()
    {
        MeterPoller poller;
        RadioModel radio;
        ContainerContentRegistry registry;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        sources.radio = &radio;
        SMeterDialContent::registerDial(registry, sources);
        bool declared = false;
        for (const ContentProperty& p : registry.declaredProperties(QString::fromLatin1(SMeterDialContent::kDialType))) {
            if (p.key == QLatin1String(SMeterDialContent::kPowerLimitKey)) {
                declared = true;
                QCOMPARE(p.kind, ContentProperty::Kind::Integer);
                QCOMPARE(p.minimum, 0);
                QCOMPARE(p.maximum, 5000);
                QCOMPARE(p.defaultValue.toInt(), 0);
            }
        }
        QVERIFY(declared);

        ContentEntry entry = dialEntry(registry, QStringLiteral("dial-50w"),
                                       QJsonObject{{QLatin1String(SMeterDialContent::kPowerLimitKey), 50}});
        entry.context = QJsonObject{{QStringLiteral("rxSource"), 1}};
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
        auto* dial = qobject_cast<SMeterWidget*>(widget.get());
        QVERIFY(dial);
        QCOMPARE(dial->testPowerScaleMax(), 50.0f);
        QCOMPARE(dial->testPowerRedStart(), 50.0f);
        dial->setTransmitting(true);
        emit poller.readingUpdated(entry.context, MeterBinding::TxPower, 25.0);
        QCOMPARE(dial->testTxValue(), 25.0f);
        QCOMPARE(dial->testNeedleTarget(), 0.5f);
        // The radio's scale changes; this copy keeps its own.
        radio.mainPowerMeter()->setHardwareModel(HPSDRModel::HERMESLITE);
        QCOMPARE(dial->testPowerScaleMax(), 50.0f);

        // Absent (or 0), the copy follows the radio's scale.
        std::unique_ptr<QWidget> follows(registry.createWidget(dialEntry(registry, QStringLiteral("dial-radio")), &parent));
        auto* radioDial = qobject_cast<SMeterWidget*>(follows.get());
        QVERIFY(radioDial);
        QCOMPARE(radioDial->testPowerScaleMax(), static_cast<float>(radio.mainPowerMeter()->scaleTopW()));
    }

    // S9 is -93 dBm when the copy's slice is at or above 30 MHz, -73 dBm
    // below, and follows the slice across the boundary.
    void vhfS9FollowsTheCopysSliceAt30MHz()
    {
        // No radio connection: a slice's frequency change warns that the
        // TX frequency is not pushed. Expected, so kept out of the output.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.connection.warning=false"));
        const auto restoreRules = qScopeGuard([] { QLoggingCategory::setFilterRules(QString()); });
        RadioModel model;
        model.addSlice();
        SliceModel* slice = model.slices().at(0);
        slice->setFrequency(30000000.0);
        MeterPoller poller;
        ContainerContentRegistry registry;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        sources.radio = &model;
        sources.sliceForContext = [slice](const QJsonObject&) { return slice; };
        SMeterDialContent::registerDial(registry, sources);
        ContentEntry entry = dialEntry(registry, QStringLiteral("dial-vhf"));
        entry.context = QJsonObject{{QStringLiteral("rxSource"), 1}};
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
        auto* dial = qobject_cast<SMeterWidget*>(widget.get());
        QVERIFY(dial);
        QVERIFY(dial->aboveS9Frequency());
        emit poller.readingUpdated(entry.context, MeterBinding::SignalPeak, -93.0);
        QCOMPARE(dial->sUnitsText(), QStringLiteral("S9"));
        QCOMPARE(dial->testDbmReadout(), QStringLiteral("-93 dBm"));
        QCOMPARE(dial->testNeedleTarget(), 0.6f);

        slice->setFrequency(29999000.0);
        QVERIFY(!dial->aboveS9Frequency());
        QCOMPARE(dial->sUnitsText(), QStringLiteral("S6"));
        emit poller.readingUpdated(entry.context, MeterBinding::SignalPeak, -73.0);
        QCOMPARE(dial->sUnitsText(), QStringLiteral("S9"));
        QCOMPARE(dial->testNeedleTarget(), 0.6f);

        // An inert preview reads the slice once.
        slice->setFrequency(144200000.0);
        QVERIFY(dial->aboveS9Frequency());
        std::unique_ptr<QWidget> preview(registry.createWidget(entry, &parent, ContentRenderMode::Preview));
        auto* previewDial = qobject_cast<SMeterWidget*>(preview.get());
        QVERIFY(previewDial);
        QVERIFY(previewDial->aboveS9Frequency());
        slice->setFrequency(14200000.0);
        QVERIFY(previewDial->aboveS9Frequency());
        QVERIFY(!dial->aboveS9Frequency());
    }
};

QTEST_MAIN(TstSMeterDialContent)
#include "tst_smeter_dial_content.moc"
