// no-port-check: NereusSDR-original lossless legacy migration regression tests.
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QTcpServer>
#include <memory>
#include "core/AppSettings.h"
#include "gui/containers/LegacyContainerImporter.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/WebImageItem.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/ItemGroup.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include <QPushButton>
using namespace NereusSDR;
class TstContainerLegacyImport : public QObject {
    Q_OBJECT
private slots:
    void rawFirstAndDeterministic() {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings"));
        QFile fixture(QFINDTESTDATA("fixtures/containers/custom.nscontainer"));
        QVERIFY(fixture.open(QIODevice::ReadOnly)); const QByteArray bytes = fixture.readAll();
        const QString text = QString::fromUtf8(bytes);
        settings.setValue("ContainerIdList", "custom");
        settings.setValue("ContainerData_custom", text.section('\n',0,0));
        settings.setValue("ContainerItems_custom", text.section('\n',1));
        const auto result = LegacyContainerImporter::fromSettings(settings);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.document, LegacyContainerImporter::fromSettings(settings).document);
        QCOMPARE(result.document.containers.size(), 1);
        const auto& c = result.document.containers.first();
        QCOMPARE(c.geometry, QRect(17,29,341,227));
        QCOMPARE(c.name, QString("custom"));
        QVERIFY(c.locked); QCOMPARE(c.contents.size(), 6);
        QCOMPARE(c.contents[0].config.value("legacyRecord").toString(), text.section('\n',1,1));
        QCOMPARE(c.contents[1].typeId, QString("UNKNOWN"));
        QVERIFY(c.contents[1].extensions.contains("unavailableReason"));
        QCOMPARE(c.contents[3].typeId, QString("DISCORDBTNS"));
        ContainerContentRegistry registry;
        QVERIFY(!registry.createMeterItem(c.contents[1], nullptr, ContentRenderMode::Validation));
        QVERIFY(!registry.createMeterItem(c.contents[3], nullptr, ContentRenderMode::Validation));
        QCOMPARE(ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(result.document)).document, result.document);
        const auto file = LegacyContainerImporter::fromContainerFile(bytes);
        QVERIFY(file.ok); QCOMPARE(file.document.extensions.value("legacyPayloadBase64").toString(), QString::fromLatin1(bytes.toBase64()));
        QCOMPARE(LegacyContainerImporter::fromClipboard(text).document, file.document);
    }
    void aliasesAndIndependentPlacement() {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings"));
        const QList<QPair<QString,QString>> aliases = {{"Rx","rx"},{"Display","Display"},{"Tx","TX"},{"PhoneCw","PHCW"},{"Rade","RADE"},{"Vax","vax"},{"PureSignal","pure_signal"},{"ModMon","mod_monitor"},{"Tci","tci"},{"ClientChain","tci_clients"},{"Amp","amp"},{"Tuner","tuner"},{"RfKit","RfKit"}};
        for (const auto& alias : aliases) {
            settings.setValue("Applet"+alias.first+"Visible", "False");
            settings.setValue("Applet"+alias.second+"Floating", "True");
            settings.setValue("Applet"+alias.second+"FloatGeometry", "exact-geometry");
        }
        const auto result = LegacyContainerImporter::fromSettings(settings); QVERIFY(result.ok);
        const auto& entries = result.document.containers.first().contents;
        QCOMPARE(entries.size(), aliases.size());
        for (int i=0;i<aliases.size();++i) {
            QCOMPARE(entries[i].typeId, "applet:"+aliases[i].second);
            QVERIFY(!entries[i].visible); QCOMPARE(entries[i].config.value("floating").toBool(), true);
            QCOMPARE(entries[i].config.value("floatGeometry").toString(), QString("exact-geometry"));
        }
        ContainerContentRegistry registry; bool found = false;
        for (const auto& d : registry.descriptors()) { if(d.typeId == "applet:s_meter") { QVERIFY(d.singleton); found = true; } }
        QVERIFY(found);
        settings.clear(); settings.setValue("AppletrxVisible", "False");
        QVERIFY(LegacyContainerImporter::fromSettings(settings).document.containers.isEmpty());
    }
    void runtimeCaptureAndLosslessOverlays() {
        ContainerContentRegistry registry;
        auto entry = registry.makeEntry("BAR"); entry.name = "My calibrated bar";
        entry.extensions.insert("future", "keep");
        entry.config.insert("legacyRecord", entry.config.value("legacyRecord").toString()+"|unknown-tail");
        std::unique_ptr<MeterItem> meter(registry.createMeterItem(entry,nullptr,ContentRenderMode::Validation)); QVERIFY(meter);
        const QUuid guid("{11111111-2222-3333-4444-555555555555}");
        meter->setMmioBinding(guid,"peak"); meter->setStackSlot(3); meter->setSlotLocalY(.2f); meter->setSlotLocalH(.6f);
        meter->setOnlyWhenTx(true); meter->setDisplayGroup(2); meter->setBindingId(47);
        const auto captured = registry.captureMeterItem(*meter,entry);
        QCOMPARE(captured.id,entry.id); QCOMPARE(captured.name,entry.name); QCOMPARE(captured.extensions,entry.extensions);
        QCOMPARE(captured.config.value("legacyRecord"),entry.config.value("legacyRecord"));
        std::unique_ptr<MeterItem> clone(registry.createMeterItem(captured,nullptr,ContentRenderMode::Validation)); QVERIFY(clone);
        QCOMPARE(clone->mmioGuid(),guid); QCOMPARE(clone->mmioVariable(),QString("peak"));
        QCOMPARE(clone->stackSlot(),3); QCOMPARE(clone->slotLocalY(),.2f); QCOMPARE(clone->slotLocalH(),.6f);
        QVERIFY(clone->onlyWhenTx()); QCOMPARE(clone->displayGroup(),2); QCOMPARE(clone->bindingId(),47);
    }
    void inertWebHydration() {
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost)); QSignalSpy requests(&server,&QTcpServer::newConnection);
        WebImageItem source; source.setFetchEnabled(false); source.setUrl(QString("http://127.0.0.1:%1/image").arg(server.serverPort()));
        ContainerContentRegistry registry; const auto entry = registry.captureMeterItem(source);
        std::unique_ptr<MeterItem> preview(registry.createMeterItem(entry,nullptr,ContentRenderMode::Preview)); QVERIFY(preview);
        std::unique_ptr<MeterItem> validation(registry.createMeterItem(entry,nullptr,ContentRenderMode::Validation)); QVERIFY(validation);
        QCOMPARE(static_cast<WebImageItem*>(preview.get())->url(),source.url());
        QCoreApplication::processEvents(); QCOMPARE(requests.size(),0);
        std::unique_ptr<MeterItem> live(registry.createMeterItem(entry,nullptr)); QVERIFY(live);
        QTRY_COMPARE_WITH_TIMEOUT(requests.size(),1,1000);
    }
    void backupOnlyOnSuccessfulCommit() {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings"));
        settings.setValue("ContainerIdList","custom"); settings.setValue("ContainerData_custom","custom|1|17|29|341|227|true|0|0|TOPLEFT|false|true|#ff123456");
        settings.setValue("ContainerItems_custom","UNKNOWN|exact");
        ContainerWorkspaceStore store(settings); QVERIFY(!settings.contains("ContainerWorkspaceBackup"));
        const auto doc = store.snapshot(); QVERIFY(doc.extensions.contains("legacySettings"));
        QCOMPARE(store.commit(doc,0).status,CommitStatus::Saved);
        const QJsonObject backup = QJsonDocument::fromJson(settings.value("ContainerWorkspaceBackup").toString().toUtf8()).object();
        QCOMPARE(backup.value("ContainerItems_custom").toString(), QString("UNKNOWN|exact"));
        QCOMPARE(settings.value("ContainerItems_custom").toString(),QString("UNKNOWN|exact"));
        AppSettings bad(dir.filePath("missing/sub/settings"));
        QFile blocker(dir.filePath("missing")); QVERIFY(blocker.open(QIODevice::WriteOnly)); blocker.close();
        bad.setValue("ContainerItems_custom","UNKNOWN|exact"); ContainerWorkspaceStore failed(bad);
        const auto before = failed.snapshot(); QCOMPARE(failed.commit(before,0).status,CommitStatus::StorageError);
        QVERIFY(!bad.contains("ContainerWorkspaceBackup")); QVERIFY(!bad.contains("ContainerWorkspace")); QCOMPARE(failed.snapshot(),before);
    }
    void legacyAdapterRetainsUnknownOnReplacement() {
        ContainerContentRegistry registry;
        auto entry = registry.makeEntry("BAR");
        const QString source = entry.config.value("legacyRecord").toString()+"|opaque-tail\nUNKNOWN|exact\nDISCORDBTNS|inert";
        MeterWidget widget; QVERIFY(widget.deserializeItems(source)); QCOMPARE(widget.items().size(),1);
        QVERIFY(widget.serializeItems().contains("opaque-tail"));
        const auto captured = registry.captureMeterItem(*widget.items().first());
        QVERIFY(captured.config.value("legacyRecord").toString().endsWith("opaque-tail"));
        const QString before = widget.serializeItems();
        QVERIFY(!widget.deserializeItems("{broken")); QCOMPARE(widget.serializeItems(),before);
        QVERIFY(!widget.deserializeItems("BAR|broken")); QCOMPARE(widget.serializeItems(),before);
        ContainerWidget container;
        auto* live = new MeterWidget(); container.setContent(live); QVERIFY(live->deserializeItems(source));
        ContainerSettingsDialog dialog(&container);
        QPushButton* apply = nullptr;
        for (auto* button : dialog.findChildren<QPushButton*>()) { if (button->text() == "Apply") { apply = button; } }
        QVERIFY(apply); apply->click();
        QVERIFY(live->serializeItems().contains("UNKNOWN|exact"));
        QVERIFY(live->serializeItems().contains("opaque-tail"));
        QVERIFY(widget.serializeItems().contains("UNKNOWN|exact"));
        QVERIFY(widget.serializeItems().contains("DISCORDBTNS|inert"));
        QVERIFY(widget.serializeItems().contains("opaque-tail"));
    }
    void clipboardAndUnbuiltCapabilities() {
        ContainerContentRegistry registry; auto entry = registry.makeEntry("NEEDLE");
        const QByteArray payload = (entry.config.value("legacyRecord").toString()+"\nUNKNOWN|exact").toUtf8();
        const QString text = QString::fromLatin1(payload.toBase64());
        const auto decoded = LegacyContainerImporter::fromClipboard(text); QVERIFY(decoded.ok);
        QCOMPARE(decoded.document.containers.first().contents.size(),2);
        QCOMPARE(decoded.document.containers.first().contents.first().typeId,QString("NEEDLE"));
        QCOMPARE(decoded.document.extensions.value("legacyClipboardText").toString(),text);
        QVERIFY(!LegacyContainerImporter::fromClipboard(text+"!").ok);
        for (const auto& d : registry.descriptors()) {
            if (d.typeId == "CLICKBOX" || d.typeId == "VOICERECPLAY") { QVERIFY(!d.available); QVERIFY(!d.unavailableReason.isEmpty()); }
        }
    }
    void defaultsAndUnknownTailsRemainRaw() {
        const QString raw = "NEEDLE|0.1|0.2|0.8|0.5|0|9|Custom label";
        const auto imported = LegacyContainerImporter::fromClipboard(raw); QVERIFY(imported.ok);
        const auto entry = imported.document.containers.first().contents.first();
        QCOMPARE(entry.name,QString("NEEDLE")); QCOMPARE(entry.paintOrder,9);
        ContainerContentRegistry registry;
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry,nullptr,ContentRenderMode::Validation)); QVERIFY(item);
        const auto captured = registry.captureMeterItem(*item,entry);
        QCOMPARE(captured.config.value("legacyRecord").toString(),raw);
        QVERIFY(captured.config.value("overrides").toObject().isEmpty());
        const auto group = LegacyContainerImporter::fromClipboard("GROUP\nMy custom group\n0\n0\n1\n1\n2\n"+raw+"\nUNKNOWN|exact");
        QVERIFY(group.ok); QCOMPARE(group.document.containers.first().contents.size(),1);
        QVERIFY(group.document.containers.first().contents.first().name.contains("My custom group"));
        QVERIFY(LegacyContainerImporter::fromClipboard("GROUP\nEmpty custom group\n0\n0\n1\n1\n0").ok);
    }
    void malformedBaseNumbersRemainInert() {
        ContainerContentRegistry registry;
        const QString good = registry.makeEntry("CLOCK").config.value("legacyRecord").toString();
        for (const auto& bad : QList<QPair<int,QString>>{{1,"broken"},{2,"nan"},{3,"inf"},{4,"1e400"},{5,"2147483648"},{6,"garbage"}}) {
            QStringList fields = good.split('|'); fields[bad.first] = bad.second;
            const QString raw = fields.join('|');
            const auto result = LegacyContainerImporter::fromClipboard(good+"\n"+raw+"\n"+good); QVERIFY(result.ok);
            const auto& contents = result.document.containers.first().contents; QCOMPARE(contents.size(),3);
            QVERIFY(contents[1].extensions.contains("unavailableReason"));
            QCOMPARE(contents[1].config.value("legacyRecord").toString(),raw);
            QVERIFY(!registry.createMeterItem(contents[1],nullptr,ContentRenderMode::Validation));
            QVERIFY(!contents[0].extensions.contains("unavailableReason"));
            QVERIFY(!contents[2].extensions.contains("unavailableReason"));
        }
    }
    void historicalGroupAdapterRetainsRawSiblings() {
        QFile fixture(QFINDTESTDATA("fixtures/containers/historical-custom.group"));
        QVERIFY(fixture.open(QIODevice::ReadOnly)); const QByteArray bytes = fixture.readAll();
        const auto imported = LegacyContainerImporter::fromContainerFile(bytes); QVERIFY(imported.ok);
        QCOMPARE(imported.document.containers.first().contents.size(),1);
        QCOMPARE(imported.document.extensions.value("legacyPayloadBase64").toString(),QString::fromLatin1(bytes.toBase64()));
        std::unique_ptr<ItemGroup> group(ItemGroup::deserialize(QString::fromUtf8(bytes))); QVERIFY(group);
        const QString serialized = group->serialize();
        QVERIFY(serialized.contains("UNKNOWN|exact historical record"));
        QVERIFY(serialized.contains("DISCORDBTNS|"));
        QVERIFY(serialized.contains("future"));
        MeterWidget widget; group->installInto(&widget,0,0,1,1);
        QVERIFY(widget.serializeItems().contains("UNKNOWN|exact historical record"));
        QVERIFY(widget.serializeItems().contains("DISCORDBTNS|"));
    }
    void opaqueOnlyAdaptersRoundTrip() {
        const QString payload = "UNKNOWN|exact\nDISCORDBTNS|inert\nVOICERECPLAY|saved unbuilt";
        MeterWidget widget; QVERIFY(widget.deserializeItems(payload));
        QCOMPARE(widget.items().size(),0); QCOMPARE(widget.serializeItems(),payload);
        MeterWidget reload; QVERIFY(reload.deserializeItems(widget.serializeItems())); QCOMPARE(reload.serializeItems(),payload);
        std::unique_ptr<ItemGroup> group(ItemGroup::deserialize("GROUP\nOpaque face\n0\n0\n1\n1\n3\n"+payload)); QVERIFY(group);
        QCOMPARE(group->items().size(),0); QVERIFY(group->serialize().endsWith(payload));
        MeterWidget malformed; QVERIFY(malformed.deserializeItems("BAR|broken")); QCOMPARE(malformed.serializeItems(),QString("BAR|broken"));
    }
    void replacementCanReuseOwnedItem() {
        MeterWidget widget; auto* bar = new BarItem(); widget.addItem(bar);
        QPointer<MeterItem> guard(bar); widget.replaceItems({bar});
        QVERIFY(guard); QCOMPARE(widget.items().size(),1); QCOMPARE(widget.items().first(),bar);
        QCOMPARE(bar->parent(),&widget);
    }
    void historicalJsonRemainsNamedAndUnavailable() {
        QFile fixture(QFINDTESTDATA("fixtures/containers/historical-custom.json")); QVERIFY(fixture.open(QIODevice::ReadOnly));
        const QByteArray bytes = fixture.readAll();
        const auto result = LegacyContainerImporter::fromContainerFile(bytes); QVERIFY2(result.ok,qPrintable(result.error));
        const auto& entry = result.document.containers.first().contents.first();
        QCOMPARE(entry.typeId,QString("BarPreset")); QVERIFY(entry.name.contains("Custom Mic | peak"));
        QCOMPARE(entry.canvasRect,QRectF(.12,.15,.81,.23));
        QCOMPARE(entry.config.value("legacyRecord").toString().toUtf8(),bytes);
        ContainerContentRegistry registry; QVERIFY(!registry.createMeterItem(entry,nullptr));
        const QString primitive = registry.makeEntry("BAR").config.value("legacyRecord").toString();
        const auto mixed = LegacyContainerImporter::fromClipboard(primitive+"\n"+QString::fromUtf8(bytes)+"\nUNKNOWN|exact");
        QVERIFY(mixed.ok); QCOMPARE(mixed.document.containers.first().contents.size(),3);
        QCOMPARE(mixed.document.containers.first().contents[1].typeId,QString("BarPreset"));
    }
    void rejectsFutureAndMalformedStructuredPayload() {
        QVERIFY(!LegacyContainerImporter::fromContainerFile("{\"schemaVersion\":999}").ok);
        QVERIFY(!LegacyContainerImporter::fromClipboard("{broken").ok);
    }
};
QTEST_MAIN(TstContainerLegacyImport)
#include "tst_container_legacy_import.moc"
