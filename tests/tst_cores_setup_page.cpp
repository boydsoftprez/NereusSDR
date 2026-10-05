// no-port-check: NereusSDR-original fake-session UI checks; no Core/radio/network.
// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QTabBar>
#include <QScrollArea>
#include <QScrollBar>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include "gui/setup/CoresSetupPage.h"
#include "gui/setup/ThisCorePage.h"
#include "gui/SetupDialog.h"
#include "gui/ConnectionSelector.h"
#include "core/AppSettings.h"
#include "core/security/StationIdentity.h"
#include "core/session/RemoteDevicesState.h"
#include "fakes/LoopbackTransport.h"
#include "models/RadioModel.h"
using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;
namespace {
class OlderStationLink final : public IStationLink {
public:
    bool stationLinkReady() const override { return true; }
    bool signedInWithDeviceKey() const override { return true; }
    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
};
class HeldRung final : public PathRung {
public:
    explicit HeldRung(QPointer<LoopbackTransport>& far, const QByteArray& certificate)
        : m_far(far), m_certificate(certificate) {}
    void start() override {
        auto* near = new LoopbackTransport(QStringLiteral("UI fake"));
        auto* far = new LoopbackTransport(QStringLiteral("UI Core"), this);
        near->linkTo(far);
        near->setPeerCertificateSha256(m_certificate);
        m_far = far;
        emit opened(near);
    }
    void stop() override {}
    int rank() const override { return PathRacer::Direct; }
    PathKind kind() const override { return PathKind::Direct; }
    QString address() const override { return QStringLiteral("ui.test:41000"); }
private:
    QPointer<LoopbackTransport>& m_far;
    QByteArray m_certificate;
};
SessionMessage hello(const StationIdentity& identity, const QByteArray& certificate) {
    SessionMessage value = SessionMessages::hello(kSessionProtocolMajor, kSessionProtocolMinor,
                                                  0, QStringLiteral("Untrusted name"));
    value.stationIdentity = SessionStationIdentity{StationIdentity::toBase64Url(identity.publicKeySpki()),
        StationIdentity::toBase64Url(identity.certBinding(certificate))};
    return value;
}
SavedCoreTarget target(const QString& id, const QByteArray& identity) {
    SavedCoreTarget value;
    value.id = id;
    value.label = QStringLiteral("Legacy nickname must never be actual Core name");
    value.connection.url = QStringLiteral("wss://configured.test:41000");
    value.connection.identityFingerprint = identity;
    return value;
}
QPushButton* push(QWidget& page, const char* objectName) {
    return page.findChild<QPushButton*>(QString::fromLatin1(objectName));
}
QLabel* label(QWidget& page, const char* objectName) {
    return page.findChild<QLabel*>(QString::fromLatin1(objectName));
}
void addresses(QWidget& page) { page.findChild<QTabBar*>()->setCurrentIndex(1); }
void add(QWidget& page, const QString& host) {
    push(page, "addCoreAddress")->click();
    page.findChild<QLineEdit*>(QStringLiteral("coreAddressHost"))->setText(host);
    push(page, "submitCoreAddress")->click();
}
void capture(QWidget& widget, const QString& name) {
    const QString destination = qEnvironmentVariable("NEREUS_CORE_UI_CAPTURES");
    if (!destination.isEmpty()) {
        QDir().mkpath(destination);
        QVERIFY(widget.grab().save(destination + '/' + name + QStringLiteral(".png")));
        QFile geometry(destination + '/' + name + QStringLiteral(".geometry.txt"));
        QVERIFY(geometry.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream output(&geometry);
        for (QWidget* child : widget.findChildren<QWidget*>()) {
            if (!child->isVisibleTo(&widget)) { continue; }
            output << child->metaObject()->className() << ' ' << child->objectName() << ' '
                   << child->mapTo(&widget, QPoint()).x() << ',' << child->mapTo(&widget, QPoint()).y()
                   << ' ' << child->width() << 'x' << child->height() << " min "
                   << child->minimumSizeHint().width() << 'x' << child->minimumSizeHint().height() << '\n';
        }
    }
}
}
class TstCoresSetupPage final : public QObject {
    Q_OBJECT
private slots:
    void inspectionNotificationsTrackIdentityLeaseWithoutSelectingOrProbing() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        const SavedCoreTarget one = target(QStringLiteral("one"), QByteArray(32, 'a'));
        const SavedCoreTarget two = target(QStringLiteral("two"), QByteArray(32, 'b'));
        QVERIFY(store.upsert(one));
        QVERIFY(store.upsert(two));
        QVERIFY(store.select(two.id));
        int probes = 0;
        CoresSetupPage page(&store, nullptr, nullptr,
            [&probes](const QUrl&) -> PathRung* { ++probes; return nullptr; });
        QCOMPARE(page.inspectedId(), one.id);
        QCOMPARE(page.inspectedIncarnation(), store.targetIncarnation(one.id));
        QSignalSpy changes(&page, &CoresSetupPage::inspectedTargetChanged);
        connect(&page, &CoresSetupPage::inspectedTargetChanged, &page,
            [&page](const QString& id, quint64 incarnation) {
                QCOMPARE(page.inspectedId(), id);
                QCOMPARE(page.inspectedIncarnation(), incarnation);
            });
        page.inspectTarget(two.id);
        QCOMPARE(changes.size(), 1);
        QCOMPARE(changes.last()[0].toString(), two.id);
        QCOMPARE(changes.last()[1].toULongLong(), store.targetIncarnation(two.id));
        page.inspectTarget(two.id);
        CoreSettingsContext context;
        context.radio = QStringLiteral("Ordinary telemetry refresh");
        page.setContext(context);
        page.refreshTargets();
        QCOMPARE(changes.size(), 1);
        QCOMPARE(store.selectedId(), two.id);
        QCOMPARE(probes, 0);

        // Missing inspection loses writable authority; refreshing falls back
        // through the same observable transition instead of preassigning its ID.
        page.inspectTarget(QStringLiteral("missing"));
        QCOMPARE(changes.size(), 2);
        QCOMPARE(changes.last()[0].toString(), QStringLiteral("missing"));
        QCOMPARE(changes.last()[1].toULongLong(), quint64(0));
        page.refreshTargets();
        QCOMPARE(changes.size(), 3);
        QCOMPARE(changes.last()[0].toString(), one.id);
        const quint64 firstLease = page.inspectedIncarnation();
        QVERIFY(store.remove(one.id));
        page.inspectTarget(one.id);
        QCOMPARE(changes.size(), 4);
        QCOMPARE(changes.last()[0].toString(), one.id);
        QCOMPARE(changes.last()[1].toULongLong(), quint64(0));
        QVERIFY(store.upsert(one));
        page.inspectTarget(one.id);
        QCOMPARE(changes.size(), 5);
        QVERIFY(page.inspectedIncarnation() != firstLease);
        QCOMPARE(changes.last()[1].toULongLong(), store.targetIncarnation(one.id));
        QCOMPARE(store.selectedId(), two.id);
        QCOMPARE(probes, 0);
        QVERIFY(store.remove(one.id));
        QVERIFY(store.remove(two.id));
        page.refreshTargets();
        QCOMPARE(changes.size(), 6);
        QCOMPARE(changes.last()[0].toString(), QString());
        QCOMPARE(changes.last()[1].toULongLong(), quint64(0));
        page.refreshTargets();
        QCOMPARE(changes.size(), 6);
    }
    void inspectionNotifiesAfterPendingRenameRetirementAndIgnoresTelemetry() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        const QByteArray identity(32, 'a');
        QVERIFY(store.upsert(target(QStringLiteral("one"), identity)));
        QVERIFY(store.upsert(target(QStringLiteral("two"), QByteArray(32, 'b'))));
        CoresSetupPage page(&store);
        CoreSettingsContext context;
        context.renameTargetId = page.inspectedId();
        context.renamePairedIdentity = identity;
        context.renameIncarnation = page.inspectedIncarnation();
        context.renameEpoch = 8;
        context.renameAvailable = true;
        page.setContext(context);
        page.show();
        QSignalSpy requests(&page, &CoresSetupPage::renameRequested);
        QStringList events;
        connect(&page, &CoresSetupPage::renameCancelled, &page,
                [&events](quint64) { events.append(QStringLiteral("cancel")); });
        connect(&page, &CoresSetupPage::inspectedTargetChanged, &page,
                [&events, &page](const QString&, quint64) {
                    events.append(QStringLiteral("inspect"));
                    QVERIFY(!page.findChild<QLineEdit*>(QStringLiteral("coreNameInput"))->isVisible());
                });
        push(page, "renameCore")->click();
        page.findChild<QLineEdit*>(QStringLiteral("coreNameInput"))->setText(QStringLiteral("KG4VCF/shack"));
        push(page, "saveCoreName")->click();
        QCOMPARE(requests.size(), 1);
        const CoreRenameRequest request = qvariant_cast<CoreRenameRequest>(requests.first()[0]);
        context.radio = QStringLiteral("New radio status, same authority");
        page.setContext(context);
        page.inspectTarget(page.inspectedId());
        QVERIFY(events.isEmpty());
        QVERIFY(!push(page, "saveCoreName")->isEnabled());
        page.inspectTarget(QStringLiteral("two"));
        QCOMPARE(events, QStringList({QStringLiteral("cancel"), QStringLiteral("inspect")}));
        page.finishRename(request, true, QStringLiteral("Late result"));
        QVERIFY(label(page, "coreSettingsStatus")->text().isEmpty());
        QVERIFY(!store.target(QStringLiteral("one"))->lastKnownCoreName);
    }
    void coreDetailsUsesDedicatedDestinationAcrossPageReplacement() {
        QTemporaryDir dir;
        AppSettings firstSettings(dir.filePath(QStringLiteral("first.xml")));
        AppSettings secondSettings(dir.filePath(QStringLiteral("second.xml")));
        CoreTargetStore first(firstSettings);
        CoreTargetStore second(secondSettings);
        QVERIFY(first.load());
        QVERIFY(second.load());
        QVERIFY(first.upsert(target(QStringLiteral("one"), QByteArray(32, 'a'))));
        QVERIFY(second.upsert(target(QStringLiteral("two"), QByteArray(32, 'b'))));
        SetupDialog dialog(nullptr);
        dialog.setCoreTargets(&first);
        CoreSettingsContext context;
        context.connectionDetailsAvailable = true;
        dialog.setCoreSettingsContext(context);
        QSignalSpy details(&dialog, &SetupDialog::coreConnectionDetailsRequested);
        QSignalSpy connections(&dialog, &SetupDialog::connectionsRequested);
        dialog.inspectCoreTarget(QStringLiteral("one"));
        dialog.show();
        auto clickDetails = [](CoresSetupPage* page) {
            QVERIFY(page);
            for (QPushButton* button : page->findChildren<QPushButton*>()) {
                if (button->text() == QStringLiteral("Connection details…")) {
                    QVERIFY(button->isEnabled());
                    button->click();
                    return;
                }
            }
            QFAIL("Connection details button is missing");
        };
        CoresSetupPage* initial = dialog.findChild<CoresSetupPage*>();
        clickDetails(initial);
        QCOMPARE(details.size(), 1);
        QCOMPARE(connections.size(), 0);
        dialog.setCoreTargets(&second);
        dialog.inspectCoreTarget(QStringLiteral("two"));
        CoresSetupPage* replacement = nullptr;
        for (CoresSetupPage* page : dialog.findChildren<CoresSetupPage*>()) {
            if (page != initial) { replacement = page; }
        }
        clickDetails(replacement);
        QCOMPARE(details.size(), 2);
        QCOMPARE(connections.size(), 0);
        dialog.selectPage(QStringLiteral("Remote Access"));
        QPushButton* general = push(dialog, "remoteStationConnections");
        QVERIFY(general);
        QVERIFY(general->isEnabled());
        general->click();
        QCOMPARE(connections.size(), 1);
        QCOMPARE(details.size(), 2);
    }
    void multipleUnknownCoresRemainRecognizable_data() {
        QTest::addColumn<QSize>("size");
        QTest::newRow("900x650") << QSize(900, 650);
        QTest::newRow("820x600") << QSize(820, 600);
    }
    void multipleUnknownCoresRemainRecognizable() {
        QFETCH(QSize, size);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget one = target(QStringLiteral("one"), QByteArray(32, 'a'));
        one.connection.rendezvousId = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
        QVERIFY(store.upsert(one));
        SavedCoreTarget two = target(QStringLiteral("two"), QByteArray(32, 'b'));
        two.connection.url = QStringLiteral("wss://[2001:db8::2]:41000/private?token=hidden");
        QVERIFY(store.upsert(two));
        SavedCoreTarget three = target(QStringLiteral("three"), QByteArray(32, 'c'));
        three.connection.url = QStringLiteral("wss://shack.test:41000");
        QVERIFY(store.upsert(three));
        SavedCoreTarget four = target(QStringLiteral("four"), QByteArray(32, 'd'));
        four.connection.rendezvousId = one.connection.rendezvousId;
        QVERIFY(store.upsert(four));
        QVERIFY(store.select(one.id));
        SetupDialog dialog(nullptr);
        dialog.setCoreTargets(&store);
        dialog.inspectCoreTarget(one.id);
        dialog.resize(size);
        dialog.show();
        dialog.raise();
        dialog.activateWindow();
        QTRY_VERIFY(dialog.isActiveWindow());
        CoresSetupPage* page = dialog.findChild<CoresSetupPage*>();
        QVERIFY(page);
        QSet<QString> names;
        int entries = 0;
        for (QPushButton* entry : page->findChildren<QPushButton*>()) {
            if (!entry->property("coreTargetId").isValid() || !entry->isVisibleTo(page)) { continue; }
            ++entries;
            QStringList visibleText;
            for (QLabel* child : entry->findChildren<QLabel*>()) { visibleText.append(child->text()); }
            const QString shown = visibleText.join('\n');
            QVERIFY(shown.contains(QStringLiteral("Core name unknown")));
            QVERIFY(shown.contains(QStringLiteral("Not connected in this window")));
            QVERIFY(!shown.contains(QStringLiteral("Legacy")));
            QVERIFY(!shown.contains(QStringLiteral("hidden")));
            QCOMPARE(entry->accessibleName(), shown);
            QVERIFY(entry->toolTip().startsWith(shown));
            const auto saved = store.target(entry->property("coreTargetId").toString());
            QVERIFY(saved);
            QVERIFY(entry->toolTip().contains(QString::fromLatin1(saved->connection.identityFingerprint.toHex())));
            QCOMPARE(entry->accessibleDescription(), entry->toolTip());
            names.insert(shown);
            if (entry->property("coreTargetId").toString() == two.id) {
                QVERIFY(shown.contains(QStringLiteral("Configured address: [2001:db8::2]:41000")));
            } else if (entry->property("coreTargetId").toString() == three.id) {
                QVERIFY(shown.contains(QStringLiteral("Configured address: shack.test:41000")));
            } else {
                QVERIFY(shown.contains(QStringLiteral("Core ID: abcdefghijklmnopqrstuvwxyz")));
                QVERIFY(shown.contains(QStringLiteral("Core identity:")));
            }
        }
        QCOMPARE(entries, 4);
        QCOMPARE(names.size(), 4);
        QCOMPARE(store.selectedId(), one.id);
        QVERIFY(!page->findChild<ThisCorePage*>());
        auto* scroll = page->findChild<QScrollArea*>();
        QTRY_COMPARE(scroll->widget()->width(), scroll->viewport()->width());
        QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
        capture(dialog, QStringLiteral("unknown-cores-%1").arg(size.width()));
    }
    void inspectionIsSeparateFromConnectionAndActualName() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        const QByteArray identity(32, 'a');
        QVERIFY(store.upsert(target(QStringLiteral("one"), identity)));
        QVERIFY(store.upsert(target(QStringLiteral("two"), QByteArray(32, 'b'))));
        QVERIFY(store.select(QStringLiteral("one")));
        QVERIFY(store.rememberCoreName(QStringLiteral("two"), QByteArray(32, 'b'), QStringLiteral("KG4VCF/office")));
        int dials = 0;
        CoresSetupPage page(&store, nullptr, nullptr, [&](const QUrl&) -> PathRung* { ++dials; return nullptr; });
        CoreSettingsContext context;
        context.targetId = QStringLiteral("one");
        context.pairedIdentity = identity;
        context.epoch = 7;
        context.authenticated = true;
        context.coreName = QStringLiteral("KG4VCF/radxa");
        context.controls = QStringLiteral("Direct · introduced by the service");
        page.setContext(context);
        QCOMPARE(label(page, "actualCoreName")->text(), QStringLiteral("KG4VCF/radxa"));
        page.inspectTarget(QStringLiteral("two"));
        QCOMPARE(label(page, "actualCoreName")->text(), QStringLiteral("KG4VCF/office (last known)"));
        QCOMPARE(store.selectedId(), QStringLiteral("one"));
        QCOMPARE(dials, 0);
        QVERIFY(!page.findChild<ThisCorePage*>());
        page.inspectTarget(QStringLiteral("one"));
        context.authenticated = false;
        page.setContext(context);
        QCOMPARE(label(page, "actualCoreName")->text(), QStringLiteral("Core name unknown"));
        QVERIFY(!label(page, "actualCoreName")->text().contains(QStringLiteral("Legacy")));
    }
    void addAndAtomicEditRequireIdentityProof() {
        QTemporaryDir dir;
        StationIdentity identity = StationIdentity::loadOrCreate(dir.filePath(QStringLiteral("key")));
        StationIdentity wrong = StationIdentity::loadOrCreate(dir.filePath(QStringLiteral("wrong")));
        AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        QVERIFY(store.upsert(target(QStringLiteral("one"), identity.fingerprint())));
        QPointer<LoopbackTransport> far;
        const QByteArray certificate(32, 'c');
        CoresSetupPage page(&store, nullptr, nullptr,
            [&](const QUrl&) { return new HeldRung(far, certificate); }, 1000);
        page.resize(699, 650);
        page.show();
        addresses(page);
        add(page, QStringLiteral("[2001:db8::1234]"));
        QTRY_VERIFY(far);
        QVERIFY(!push(page, "submitCoreAddress")->isEnabled());
        QVERIFY(store.target(QStringLiteral("one"))->manualAddresses.isEmpty());
        capture(page, QStringLiteral("pending"));
        far->sendText(SessionMessages::encode(hello(identity, certificate)));
        QTRY_COMPARE(store.target(QStringLiteral("one"))->manualAddresses.size(), 1);
        const QString original = store.target(QStringLiteral("one"))->manualAddresses.first();
        QTRY_VERIFY(push(page, "editCoreAddress"));
        push(page, "editCoreAddress")->click();
        page.findChild<QLineEdit*>(QStringLiteral("coreAddressHost"))->setText(QStringLiteral("wrong.test"));
        far = nullptr;
        push(page, "submitCoreAddress")->click();
        QTRY_VERIFY(far);
        QCOMPARE(store.target(QStringLiteral("one"))->manualAddresses, QStringList{original});
        far->sendText(SessionMessages::encode(hello(wrong, certificate)));
        QTRY_VERIFY(!label(page, "coreAddressError")->text().isEmpty());
        QVERIFY(!label(page, "coreAddressError")->text().contains(QStringLiteral("could not be reached")));
        QVERIFY(label(page, "coreSettingsStatus")->text().isEmpty());
        QCOMPARE(store.target(QStringLiteral("one"))->manualAddresses, QStringList{original});
        QVERIFY(push(page, "submitCoreAddress")->isEnabled());
        capture(page, QStringLiteral("wrong-core-error"));
        far = nullptr;
        page.findChild<QLineEdit*>(QStringLiteral("coreAddressHost"))->setText(QStringLiteral("new.test"));
        push(page, "submitCoreAddress")->click();
        QTRY_VERIFY(far);
        far->sendText(SessionMessages::encode(hello(identity, certificate)));
        QTRY_COMPARE(store.target(QStringLiteral("one"))->manualAddresses, QStringList{QStringLiteral("wss://new.test:41000")});
    }
    void pendingCancellationAndStaleCompletion_data() {
        QTest::addColumn<QString>("action");
        for (const char* action : {"cancel", "tab", "hide", "dialog-close", "switch", "context", "recreate", "destroy"}) {
            QTest::newRow(action) << QString::fromLatin1(action);
        }
    }
    void pendingCancellationAndStaleCompletion() {
        QFETCH(QString, action);
        QTemporaryDir dir;
        StationIdentity identity = StationIdentity::loadOrCreate(dir.filePath(QStringLiteral("key")));
        AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget one = target(QStringLiteral("one"), identity.fingerprint());
        QVERIFY(store.upsert(one));
        QVERIFY(store.upsert(target(QStringLiteral("two"), QByteArray(32, 'b'))));
        QPointer<LoopbackTransport> far;
        const QByteArray certificate(32, 'c');
        QDialog shell;
        auto page = std::make_unique<CoresSetupPage>(&store, nullptr,
            action == "dialog-close" ? &shell : nullptr,
            [&](const QUrl&) { return new HeldRung(far, certificate); }, 1000);
        if (action == "dialog-close") {
            auto* layout = new QVBoxLayout(&shell);
            layout->addWidget(page.get());
            shell.show();
        }
        page->show();
        addresses(*page);
        add(*page, QStringLiteral("new.test"));
        QTRY_VERIFY(far);
        // Queue the actual proved hello before the UI retires the operation.
        far->sendText(SessionMessages::encode(hello(identity, certificate)));
        if (action == "cancel") { push(*page, "cancelCoreAddress")->click(); }
        if (action == "tab") { page->findChild<QTabBar*>()->setCurrentIndex(0); }
        if (action == "hide") { page->hide(); }
        if (action == "dialog-close") { shell.close(); }
        if (action == "switch") { page->inspectTarget(QStringLiteral("two")); }
        if (action == "context") { CoreSettingsContext changed; changed.epoch = 2; page->setContext(changed); }
        if (action == "recreate") {
            QVERIFY(store.remove(one.id));
            QVERIFY(store.upsert(one));
            page->refreshTargets();
        }
        if (action == "destroy") { page.reset(); }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        QVERIFY(store.target(one.id)->manualAddresses.isEmpty());
        QVERIFY(store.target(QStringLiteral("two"))->manualAddresses.isEmpty());
        if (page) { QVERIFY(label(*page, "coreAddressError")->text().isEmpty()); }
    }
    void telemetryRefreshPreservesDeliberateEdit() {
        QTemporaryDir dir;
        StationIdentity identity = StationIdentity::loadOrCreate(dir.filePath(QStringLiteral("key")));
        AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        QVERIFY(store.upsert(target(QStringLiteral("one"), identity.fingerprint())));
        QPointer<LoopbackTransport> far;
        const QByteArray certificate(32, 'c');
        CoresSetupPage page(&store, nullptr, nullptr, [&](const QUrl&) { return new HeldRung(far, certificate); }, 1000);
        CoreSettingsContext context;
        context.targetId = QStringLiteral("one");
        context.pairedIdentity = identity.fingerprint();
        context.authenticated = true;
        context.epoch = 7;
        context.renameAvailable = true;
        page.setContext(context);
        page.show();
        addresses(page);
        add(page, QStringLiteral("new.test"));
        QTRY_VERIFY(far);
        context.radio = QStringLiteral("Radio offline at the Core");
        context.renameAvailable = false;
        page.setContext(context);
        QVERIFY(!push(page, "submitCoreAddress")->isEnabled());
        far->sendText(SessionMessages::encode(hello(identity, certificate)));
        QTRY_COMPARE(store.target(QStringLiteral("one"))->manualAddresses.size(), 1);
    }
    void fullDuplicateAndMergedRemoval() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget one = target(QStringLiteral("one"), QByteArray(32, 'a'));
        one.manualAddresses = {QStringLiteral("wss://[2001:db8::1]:41000"), QStringLiteral("wss://two.test:41000"),
                               QStringLiteral("wss://three.test:41000"), QStringLiteral("wss://four.test:41000")};
        one.connection.cachedAddresses = {one.manualAddresses.first()};

        QVERIFY(store.upsert(one));
        QVERIFY(store.rememberCoreAddresses(one.id, one.connection.identityFingerprint, QStringLiteral("[\"[2001:db8::1]:41000\"]")));
        CoresSetupPage page(&store);
        page.show();
        addresses(page);
        QVERIFY(!push(page, "addCoreAddress")->isEnabled());
        push(page, "removeCoreAddress")->click();
        push(page, "confirmRemoveCoreAddress")->click();
        QCOMPARE(store.target(one.id)->manualAddresses.size(), 3);
        QCOMPARE(store.target(one.id)->connection.cachedAddresses, one.connection.cachedAddresses);
        QVERIFY(push(page, "addCoreAddress")->isEnabled());
        add(page, QStringLiteral("two.test"));
        QTRY_VERIFY(!label(page, "coreAddressError")->text().isEmpty());
        QCOMPARE(store.target(one.id)->manualAddresses.size(), 3);
    }
    void durableSaveFailurePreservesOriginalManualAddress() {
        QTemporaryDir dir;
        StationIdentity identity = StationIdentity::loadOrCreate(dir.filePath(QStringLiteral("key")));
        const QString path = dir.filePath(QStringLiteral("settings.xml"));
        AppSettings settings(path);
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget one = target(QStringLiteral("one"), identity.fingerprint());
        one.manualAddresses = {QStringLiteral("wss://old.test:41000")};
        QVERIFY(store.upsert(one));
        QPointer<LoopbackTransport> far;
        const QByteArray certificate(32, 'c');
        CoresSetupPage page(&store, nullptr, nullptr, [&](const QUrl&) { return new HeldRung(far, certificate); }, 1000);
        page.show();
        addresses(page);
        push(page, "editCoreAddress")->click();
        page.findChild<QLineEdit*>(QStringLiteral("coreAddressHost"))->setText(QStringLiteral("new.test"));
        push(page, "submitCoreAddress")->click();
        QTRY_VERIFY(far);
        QVERIFY(QFile::remove(path));
        QVERIFY(QDir().mkdir(path));
        far->sendText(SessionMessages::encode(hello(identity, certificate)));
        QTRY_VERIFY(!label(page, "coreAddressError")->text().isEmpty());
        QCOMPARE(store.target(one.id)->manualAddresses, one.manualAddresses);
        QVERIFY(push(page, "submitCoreAddress")->isEnabled());
        capture(page, QStringLiteral("save-error"));
    }
    void renameFencesDoNotWriteNicknameOrAnotherCore() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        const QByteArray identity(32, 'a');
        QVERIFY(store.upsert(target(QStringLiteral("one"), identity)));
        QVERIFY(store.upsert(target(QStringLiteral("two"), QByteArray(32, 'b'))));
        CoresSetupPage page(&store);
        CoreSettingsContext context;
        context.renameTargetId = QStringLiteral("one");
        context.renamePairedIdentity = identity;
        context.renameIncarnation = store.targetIncarnation(QStringLiteral("one"));
        context.renameEpoch = 8;
        context.renameAvailable = true;
        page.setContext(context);
        QSignalSpy requests(&page, &CoresSetupPage::renameRequested);
        QSignalSpy cancels(&page, &CoresSetupPage::renameCancelled);
        page.show();
        push(page, "renameCore")->click();
        page.findChild<QLineEdit*>(QStringLiteral("coreNameInput"))->setText(QStringLiteral("KG4VCF/shack"));
        push(page, "saveCoreName")->click();
        QCOMPARE(requests.size(), 1);
        const CoreRenameRequest operation = qvariant_cast<CoreRenameRequest>(requests.first()[0]);
        QCOMPARE(operation.pairedIdentity, identity);
        QCOMPARE(operation.incarnation, context.renameIncarnation);
        page.inspectTarget(QStringLiteral("two"));
        QCOMPARE(cancels.size(), 1);
        QVERIFY(!push(page, "renameCore")->isEnabled());
        page.finishRename(operation, true, {});
        QVERIFY(label(page, "coreSettingsStatus")->text().isEmpty());
        QVERIFY(!store.target(QStringLiteral("one"))->lastKnownCoreName);
        QCOMPARE(store.target(QStringLiteral("one"))->label, QStringLiteral("Legacy nickname must never be actual Core name"));
    }
    void losingRenameAuthorityRetiresPendingEditorWithoutNewEpoch() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        const QByteArray identity(32, 'a');
        QVERIFY(store.upsert(target(QStringLiteral("one"), identity)));
        CoresSetupPage page(&store);
        CoreSettingsContext context;
        context.renameTargetId = QStringLiteral("one");
        context.renamePairedIdentity = identity;
        context.renameIncarnation = store.targetIncarnation(QStringLiteral("one"));
        context.renameEpoch = 8;
        context.renameAvailable = true;
        page.setContext(context);
        page.show();
        QSignalSpy requests(&page, &CoresSetupPage::renameRequested);
        QSignalSpy cancels(&page, &CoresSetupPage::renameCancelled);
        push(page, "renameCore")->click();
        page.findChild<QLineEdit*>(QStringLiteral("coreNameInput"))->setText(QStringLiteral("KG4VCF/shack"));
        push(page, "saveCoreName")->click();
        QCOMPARE(requests.size(), 1);
        const CoreRenameRequest request = qvariant_cast<CoreRenameRequest>(requests.first()[0]);
        QVERIFY(!push(page, "saveCoreName")->isEnabled());
        context.renameAvailable = false;
        context.renameReason = QStringLiteral("Wait until this connection is made.");
        page.setContext(context);
        QCOMPARE(cancels.size(), 1);
        QCOMPARE(cancels.first()[0].toULongLong(), request.operationId);
        QVERIFY(!push(page, "renameCore")->isEnabled());
        QVERIFY(push(page, "saveCoreName")->isEnabled());
        QVERIFY(!page.findChild<QLineEdit*>(QStringLiteral("coreNameInput"))->isVisible());
        page.finishRename(request, true, {});
        QVERIFY(label(page, "coreSettingsStatus")->text().isEmpty());
    }
    void acceptedRenamePreservesHostWarningWithoutLocalMutation() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        const QByteArray identity(32, 'a');
        QVERIFY(store.upsert(target(QStringLiteral("one"), identity)));
        CoresSetupPage page(&store);
        CoreSettingsContext context;
        context.renameTargetId = QStringLiteral("one");
        context.renamePairedIdentity = identity;
        context.renameIncarnation = store.targetIncarnation(QStringLiteral("one"));
        context.renameEpoch = 8;
        context.renameAvailable = true;
        page.setContext(context);
        page.show();
        QSignalSpy requests(&page, &CoresSetupPage::renameRequested);
        QSignalSpy cancels(&page, &CoresSetupPage::renameCancelled);
        push(page, "renameCore")->click();
        page.findChild<QLineEdit*>(QStringLiteral("coreNameInput"))->setText(QStringLiteral("KG4VCF/shack"));
        push(page, "saveCoreName")->click();
        QCOMPARE(requests.size(), 1);
        const CoreRenameRequest request = qvariant_cast<CoreRenameRequest>(requests.first()[0]);
        const QString warning = QStringLiteral("The Core accepted its name, but its last-known name could not be saved on this computer.");
        page.finishRename(request, true, warning);
        QCOMPARE(label(page, "coreSettingsStatus")->text(), warning);
        QVERIFY(!page.findChild<QLineEdit*>(QStringLiteral("coreNameInput"))->isVisible());
        QCOMPARE(cancels.size(), 0);
        QVERIFY(!store.target(request.targetId)->lastKnownCoreName);
        QCOMPARE(store.target(request.targetId)->label, QStringLiteral("Legacy nickname must never be actual Core name"));
        page.finishRename(request, true, {});
        QCOMPARE(label(page, "coreSettingsStatus")->text(), warning);
    }
    void currentAndOtherCoreViews() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        const QByteArray identity(32, 'a');
        QVERIFY(store.upsert(target(QStringLiteral("one"), identity)));
        QVERIFY(store.upsert(target(QStringLiteral("two"), QByteArray(32, 'b'))));
        QVERIFY(store.rememberCoreName(QStringLiteral("two"), QByteArray(32, 'b'), QStringLiteral("KG4VCF/office")));
        SetupDialog dialog(nullptr);
        dialog.setCoreTargets(&store);
        CoreSettingsContext context;
        context.targetId = QStringLiteral("one");
        context.pairedIdentity = identity;
        context.epoch = 7;
        context.authenticated = true;
        context.coreName = QStringLiteral("KG4VCF/radxa");
        context.reachedThrough = QStringLiteral("Remote access service");
        context.controls = QStringLiteral("Direct · introduced by the service");
        context.audioAndDisplay = QStringLiteral("Through control connection");
        context.radio = QStringLiteral("ANAN-G2 (Saturn) · Connected");
        dialog.setCoreSettingsContext(context);
        dialog.inspectCoreTarget(QStringLiteral("one"));
        dialog.show();
        CoresSetupPage* page = dialog.findChild<CoresSetupPage*>();
        QScrollArea* scroll = page->findChild<QScrollArea*>();
        QTRY_COMPARE(scroll->widget()->width(), scroll->viewport()->width());
        capture(dialog, QStringLiteral("current-overview"));
        context.radio = QStringLiteral("Radio offline at the Core");
        dialog.setCoreSettingsContext(context);
        capture(dialog, QStringLiteral("radio-offline-overview"));
        dialog.inspectCoreTarget(QStringLiteral("two"));
        capture(dialog, QStringLiteral("another-core-overview"));
        QCOMPARE(label(*page, "actualCoreName")->text(), QStringLiteral("KG4VCF/office (last known)"));
        page->findChild<QTabBar*>()->setCurrentIndex(2);
        QCoreApplication::processEvents();
        capture(dialog, QStringLiteral("another-core-radio"));
        page->findChild<QTabBar*>()->setCurrentIndex(3);
        QCoreApplication::processEvents();
        capture(dialog, QStringLiteral("another-core-devices"));
        for (QPushButton* action : page->findChildren<QPushButton*>()) {
            if (action->text() == QStringLiteral("Use this radio")
                || action->text() == QStringLiteral("Add a device")
                || action->text() == QStringLiteral("I've backed it up")) {
                QVERIFY(!action->isEnabled());
            }
        }
        for (QLabel* fact : page->findChildren<QLabel*>()) {
            QVERIFY(fact->text() != QStringLiteral("Direct · introduced by the service"));
            QVERIFY(fact->text() != QStringLiteral("Radio offline at the Core"));
        }
    }
    void administrationRequiresMatchingAuthenticatedIdentity() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        const QByteArray identity(32, 'a');
        QVERIFY(store.upsert(target(QStringLiteral("one"), identity)));
        QVERIFY(store.upsert(target(QStringLiteral("two"), QByteArray(32, 'b'))));
        RadioModel model(RadioModel::Role::Remote);
        OlderStationLink link;
        model.attachStation(&link);
        RemoteDevicesState devices;
        devices.applyObject("devices", {
            {0, "stationLabel", MirrorWireKind::Utf8, QStringLiteral("KG4VCF/actual")},
            {0, "keyPath", MirrorWireKind::Utf8,
             QStringLiteral("/srv/cores/") + QStringLiteral("directory with a long descriptive name/").repeated(28) + QStringLiteral("identity.key")}
        });
        model.setStationDevices(&devices);
        CoresSetupPage page(&store, &model);
        page.resize(699, 650);
        CoreSettingsContext context;
        context.targetId = QStringLiteral("one");
        context.pairedIdentity = identity;
        context.epoch = 7;
        context.authenticated = true;
        context.coreName = QStringLiteral("Incorrect host fallback");
        context.stationSettingsAvailable = true;
        page.setContext(context);
        QCOMPARE(label(page, "actualCoreName")->text(), QStringLiteral("KG4VCF/actual"));
        QVERIFY(page.findChild<ThisCorePage*>());
        QCOMPARE(page.findChild<ThisCorePage*>()->devicesUnavailableReason(), IStationLink::deviceAdminUnavailableReason());
        QCOMPARE(page.findChild<ThisCorePage*>()->unavailableReason(), IStationLink::stationRadiosUnavailableReason());
        QVERIFY(!page.findChild<ThisCorePage*>()->useButton()->isEnabled());
        QVERIFY(!page.findChild<ThisCorePage*>()->addDeviceButton()->isEnabled());
        page.setStationSettingsAvailable(false, QStringLiteral("Awaiting current Core settings"));
        context.radio = QStringLiteral("Radio offline at the Core");
        page.setContext(context);
        QCOMPARE(page.findChild<ThisCorePage*>()->unavailableReason(), QStringLiteral("Awaiting current Core settings"));
        page.setStationSettingsAvailable(true, {});
        page.show();
        page.findChild<QTabBar*>()->setCurrentIndex(2);
        capture(page, QStringLiteral("older-core-radio"));
        page.findChild<QTabBar*>()->setCurrentIndex(3);
        capture(page, QStringLiteral("older-core-devices"));
        QScrollArea* scroll = page.findChild<QScrollArea*>();
        QTRY_VERIFY(scroll->verticalScrollBar()->maximum() > 0);
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        QCoreApplication::processEvents();
        capture(page, QStringLiteral("older-core-devices-bottom"));
        page.inspectTarget(QStringLiteral("two"));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!page.findChild<ThisCorePage*>());
        context.targetId = QStringLiteral("two");
        context.pairedIdentity = identity; // Wrong paired identity must not admit.
        page.setContext(context);
        QVERIFY(!page.findChild<ThisCorePage*>());
        context.pairedIdentity = QByteArray(32, 'b');
        context.authenticated = false;
        page.setContext(context);
        QVERIFY(!page.findChild<ThisCorePage*>());
        // Full dialog framing for the matching authenticated administration,
        // using the same older-Core fake (no live Core/radio/default inventory).
        context.targetId = QStringLiteral("one");
        context.pairedIdentity = identity;
        context.authenticated = true;
        SetupDialog dialog(&model);
        dialog.setCoreTargets(&store);
        dialog.setCoreSettingsContext(context);
        dialog.inspectCoreTarget(context.targetId);
        dialog.show();
        auto* current = dialog.findChild<CoresSetupPage*>();
        QVERIFY(current);
        QCOMPARE(label(*current, "actualCoreName")->text(), QStringLiteral("KG4VCF/actual"));
        current->findChild<QTabBar*>()->setCurrentIndex(2);
        QCoreApplication::processEvents();
        capture(dialog, QStringLiteral("current-older-core-radio"));
        current->findChild<QTabBar*>()->setCurrentIndex(3);
        QCoreApplication::processEvents();
        capture(dialog, QStringLiteral("current-older-core-devices"));
    }
    void manageEntryOnlyOpensBoundSavedCoreDestination() {
        ConnectionSelector selector;
        ConnectionTargetRow saved;
        saved.key = QStringLiteral("core:one");
        saved.kind = ConnectionTargetKind::SavedCore;
        saved.name = QStringLiteral("KG4VCF/shack");
        selector.setTargets({saved});
        selector.setSelectedKey(saved.key);
        selector.show();
        QSignalSpy manage(&selector, &ConnectionSelector::manageCoreRequested);
        QSignalSpy connectSpy(&selector, &ConnectionSelector::connectRequested);
        QPushButton* entry = push(selector, "connectionSelectorManageCore");
        QVERIFY(!entry->isVisible());
        selector.setCoreManagementAvailable(true);
        QVERIFY(entry->isVisible());
        entry->click();
        QCOMPARE(manage.size(), 1);
        QCOMPARE(manage.first()[0].toString(), saved.key);
        QCOMPARE(connectSpy.size(), 0);
        saved.kind = ConnectionTargetKind::LanCore;
        selector.setTargets({saved});
        selector.setSelectedKey(saved.key);
        QVERIFY(!entry->isVisible());
    }
    void geometryAndKeyboard_data() {
        QTest::addColumn<QSize>("size");
        QTest::newRow("900x650") << QSize(900, 650);
        QTest::newRow("820x600") << QSize(820, 600);
    }
    void geometryAndKeyboard() {
        QFETCH(QSize, size);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget one = target(QStringLiteral("one"), QByteArray(32, 'a'));
        one.manualAddresses = {QStringLiteral("wss://[2001:db8:1234:5678:1234:5678:1234:5678]:41000")};
        QVERIFY(store.upsert(one));
        QVERIFY(store.rememberCoreName(one.id, one.connection.identityFingerprint,
            QStringLiteral("KG4VCF/abcdefghijklmnopqrstuvwxyz123456")));
        QVERIFY(store.upsert(target(QStringLiteral("two"), QByteArray(32, 'b'))));
        SetupDialog dialog(nullptr);
        dialog.setCoreTargets(&store);
        dialog.inspectCoreTarget(one.id);
        dialog.resize(size);
        dialog.show();
        QTRY_VERIFY(dialog.isVisible());
        dialog.raise();
        dialog.activateWindow();
        QTRY_VERIFY(dialog.isActiveWindow());
        QCOMPARE(dialog.size(), size);
        CoresSetupPage* page = dialog.findChild<CoresSetupPage*>();
        QVERIFY(page);
        QCOMPARE(page->findChild<QTabBar*>()->count(), 4);
        QVERIFY(!page->findChild<ThisCorePage*>());
        const auto scroll = page->findChild<QScrollArea*>();
        QVERIFY(scroll);
        QTRY_COMPARE(scroll->widget()->width(), scroll->viewport()->width());
        QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
        capture(dialog, QStringLiteral("overview-%1").arg(size.width()));
        addresses(*page);
        QCoreApplication::processEvents();
        QCOMPARE(dialog.size(), size);
        capture(dialog, QStringLiteral("addresses-%1").arg(size.width()));
        push(*page, "addCoreAddress")->click();
        QLineEdit* host = page->findChild<QLineEdit*>(QStringLiteral("coreAddressHost"));
        QTRY_VERIFY(host->hasFocus());
        QTest::keyClicks(host, "[2001:db8::1]");
        QTest::keyClick(host, Qt::Key_Tab);
        QVERIFY(page->findChild<QSpinBox*>()->hasFocus());
        capture(dialog, QStringLiteral("add-%1").arg(size.width()));
        dialog.hide();
    }
    void emptyAndUnpaired() {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        CoresSetupPage page(&store);
        page.resize(699, 650);
        page.show();
        QVERIFY(!page.findChild<QTabBar*>()->isEnabled());
        capture(page, QStringLiteral("empty"));
        SavedCoreTarget unpaired = target(QStringLiteral("legacy"), {});
        unpaired.connection.allowUnpinned = true;
        QVERIFY(store.upsert(unpaired));
        page.refreshTargets();
        addresses(page);
        QVERIFY(!push(page, "addCoreAddress")->isEnabled());
        QVERIFY(!push(page, "renameCore")->isEnabled());
        capture(page, QStringLiteral("unpaired"));
    }
};
QTEST_MAIN(TstCoresSetupPage)
#include "tst_cores_setup_page.moc"
