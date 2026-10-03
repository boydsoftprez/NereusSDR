// no-port-check: NereusSDR-original raw-first legacy presentation importer.
#include "LegacyContainerImporter.h"
#include "ContainerContentRegistry.h"
#include "ContainerDocumentCodec.h"
#include "core/AppSettings.h"
#include "gui/meters/MeterItem.h"
#include <QJsonDocument>
#include <QUuid>
#include <algorithm>
#include <memory>
#include <cmath>
namespace NereusSDR {
namespace {
const QUuid kMigrationNamespace(QStringLiteral("59eaa11e-41b9-5430-b4dc-347c7d15fe83"));
QString stableId(const QString& owner, const QString& occurrence) {
    return QUuid::createUuidV5(kMigrationNamespace, (owner + QLatin1Char('/') + occurrence).toUtf8()).toString(QUuid::WithoutBraces);
}
bool truth(const QString& value) { return value.compare(QLatin1String("true"),Qt::CaseInsensitive) == 0; }
ContainerDocument container(const QString& id, const QString& metadata, const QString& payload) {
    ContainerDocument result; result.id = id; result.name = id;
    result.extensions.insert(QStringLiteral("legacyContainerData"),metadata);
    result.extensions.insert(QStringLiteral("legacyItemsPayload"),payload);
    const QStringList fields = metadata.split(QLatin1Char('|'));
    if (fields.size() >= 13) {
        bool valid = true;
        for (int i : {1,2,3,4,5,7,8}) { bool ok = false; fields[i].toInt(&ok); valid = valid && ok; }
        if (valid) {
            const int rx = std::clamp(fields[1].toInt(),1,4);
            result.geometry = QRect(fields[2].toInt(),fields[3].toInt(),fields[4].toInt(),fields[5].toInt());
            const QString mode = fields.value(23);
            result.dockMode = mode == QLatin1String("PANEL") ? DockMode::PanelDocked :
                (mode == QLatin1String("FLOATING") || (fields.size() <= 23 && truth(fields[6]))) ? DockMode::Floating : DockMode::OverlayDocked;
            const QStringList anchors = {QStringLiteral("LEFT"),QStringLiteral("TOPLEFT"),QStringLiteral("TOP"),QStringLiteral("TOPRIGHT"),QStringLiteral("RIGHT"),QStringLiteral("BOTTOMRIGHT"),QStringLiteral("BOTTOM"),QStringLiteral("BOTTOMLEFT")};
            const int anchor = anchors.indexOf(fields[9].toUpper()); result.anchor = static_cast<AxisLock>(anchor < 0 ? 1 : anchor);
            result.autoHeight = truth(fields.value(17)); result.locked = truth(fields.value(20));
            result.visible = (fields.size() <= 14 || truth(fields[14])) && !truth(fields.value(22));
            // Canonical header owns structured recovery; retain the old flag/raw
            // record but do not let it override later Always/Reveal edits.
            result.header = truth(fields.value(13)) || (fields.size() > 24 && !truth(fields[24])) ? HeaderMode::Hidden : HeaderMode::Always;
            result.config = {{QStringLiteral("rxSource"),rx},{QStringLiteral("sliceId"),rx-1},
                {QStringLiteral("deltaX"),fields[7].toInt()},{QStringLiteral("deltaY"),fields[8].toInt()},
                {QStringLiteral("pinOnTop"),truth(fields[10])},{QStringLiteral("border"),truth(fields[11])},
                {QStringLiteral("backgroundColor"),fields[12]},{QStringLiteral("noControls"),truth(fields.value(13))},
                {QStringLiteral("enabled"),fields.size() <= 14 || truth(fields[14])},{QStringLiteral("notes"),fields.value(15)},
                {QStringLiteral("containerMinimises"),truth(fields.value(16))},
                {QStringLiteral("showOnRx"),fields.size() <= 19 || truth(fields[18])},
                {QStringLiteral("showOnTx"),fields.size() <= 19 || truth(fields[19])},
                {QStringLiteral("hidesWhenRxNotUsed"),truth(fields.value(21))},{QStringLiteral("hiddenByMacro"),truth(fields.value(22))}};
        } else { result.extensions.insert(QStringLiteral("legacyMetadataError"),QStringLiteral("Invalid numeric container fields")); }
    } else if (!metadata.isEmpty()) { result.extensions.insert(QStringLiteral("legacyMetadataError"),QStringLiteral("Truncated container metadata")); }
    ContainerContentRegistry registry;
    const QStringList records = payload.split(QLatin1Char('\n'));
    for (int occurrence = 0; occurrence < records.size(); ++occurrence) {
        QString raw = records[occurrence];
        if (raw.isEmpty()) { continue; }
        QString tag = raw.section(QLatin1Char('|'),0,0);
        QJsonObject legacyJson;
        if (raw.trimmed().startsWith(QLatin1Char('{'))) {
            const QJsonDocument json = QJsonDocument::fromJson(raw.toUtf8());
            legacyJson = json.object();
            tag = legacyJson.value(QStringLiteral("kind")).toString();
            if (tag.isEmpty()) { tag = QStringLiteral("legacy:json"); }
        }
        // GROUP is a multiline legacy adapter, not a durable composite signature.
        // Preserve it as one named unavailable composition until a lossless face
        // representation exists; do not rebuild customized primitives from defaults.
        QString groupName;
        if (raw == QLatin1String("GROUP")) {
            groupName = records.value(occurrence+1);
            bool ok = false; const int count = records.value(occurrence+6).toInt(&ok);
            const int length = ok && count >= 0 && count <= records.size()-occurrence-7 ? 7+count : records.size()-occurrence;
            raw = records.mid(occurrence,length).join(QLatin1Char('\n'));
            occurrence += length-1;
        }
        ContentEntry entry;
        entry.id = stableId(id,QString::number(result.contents.size())); entry.typeId = tag;
        entry.name = groupName.isEmpty() ? tag : groupName;
        if (legacyJson.value(QStringLiteral("label")).isString()) { entry.name = legacyJson.value(QStringLiteral("label")).toString(); }
        bool jsonGeometry = !legacyJson.isEmpty();
        for (const QString& key : {QStringLiteral("x"),QStringLiteral("y"),QStringLiteral("w"),QStringLiteral("h")}) {
            const QJsonValue value = legacyJson.value(key);
            jsonGeometry = jsonGeometry && value.isDouble() && std::isfinite(value.toDouble());
        }
        if (jsonGeometry) { entry.canvasRect = QRectF(legacyJson.value(QStringLiteral("x")).toDouble(),legacyJson.value(QStringLiteral("y")).toDouble(),legacyJson.value(QStringLiteral("w")).toDouble(),legacyJson.value(QStringLiteral("h")).toDouble()); }
        entry.config.insert(QStringLiteral("legacyRecord"),raw);
        entry.context.insert(QStringLiteral("rxSource"),result.config.value(QStringLiteral("rxSource")));
        entry.context.insert(QStringLiteral("sliceId"),result.config.value(QStringLiteral("sliceId")));
        // Capture supported geometry/properties only after inert successful decode.
        // The complete raw record remains authoritative and is never rewritten.
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry,nullptr,ContentRenderMode::Validation));
        if (item) {
            // createMeterItem uses paintOrder; legacy z must first be obtained
            // from field 6, without treating it as contents ordering.
            const QStringList itemFields = raw.split(QLatin1Char('|'));
            bool ok = false; const int z = itemFields.value(6).toInt(&ok); if (ok) { item->setZOrder(z); }
            entry = registry.captureMeterItem(*item,entry);
        } else {
            const QString reason = tag == QLatin1String("DISCORDBTNS") ? QStringLiteral("This control was retired") : QStringLiteral("Unknown, unbuilt or malformed legacy content");
            if (!registry.validateEntry(entry).isEmpty()) { entry.extensions.insert(QStringLiteral("legacyMalformed"),true); }
            entry.extensions.insert(QStringLiteral("unavailableReason"),reason);
            entry.name += QStringLiteral(" (unavailable)");
        }
        result.contents.append(entry);
    }
    return result;
}
DocumentResult finish(WorkspaceDocument document) {
    const QString error = ContainerDocumentCodec::validate(document);
    return {error.isEmpty(),document,error};
}
const QList<QPair<QString,QString>> kAppletAliases = {
    {QStringLiteral("Rx"),QStringLiteral("rx")},{QStringLiteral("Display"),QStringLiteral("Display")},
    {QStringLiteral("Tx"),QStringLiteral("TX")},{QStringLiteral("PhoneCw"),QStringLiteral("PHCW")},
    {QStringLiteral("Rade"),QStringLiteral("RADE")},{QStringLiteral("Vax"),QStringLiteral("vax")},
    {QStringLiteral("PureSignal"),QStringLiteral("pure_signal")},{QStringLiteral("ModMon"),QStringLiteral("mod_monitor")},
    {QStringLiteral("Tci"),QStringLiteral("tci")},{QStringLiteral("ClientChain"),QStringLiteral("tci_clients")},
    {QStringLiteral("Amp"),QStringLiteral("amp")},{QStringLiteral("Tuner"),QStringLiteral("tuner")},
    {QStringLiteral("RfKit"),QStringLiteral("RfKit")}
};
}
DocumentResult LegacyContainerImporter::fromSettings(const AppSettings& settings) {
    if (settings.contains(QStringLiteral("ContainerWorkspace"))) { return ContainerDocumentCodec::decode(settings.value(QStringLiteral("ContainerWorkspace")).toString().toUtf8()); }
    WorkspaceDocument document; QJsonObject backup;
    QStringList ids = settings.value(QStringLiteral("ContainerIdList")).toString().split(QLatin1Char(','),Qt::SkipEmptyParts);
    for (const QString& key : settings.allKeys()) {
        const bool owned = key.startsWith(QLatin1String("ContainerData_")) || key.startsWith(QLatin1String("ContainerItems_")) || key.startsWith(QLatin1String("MeterDisplay_")) || key == QLatin1String("ContainerIdList") || key == QLatin1String("ContainerCount") || key == QLatin1String("MainSplitterSizes") || key.startsWith(QLatin1String("Applet"));
        if (owned) { backup.insert(key,settings.value(key).toString()); }
        for (const QString& prefix : {QStringLiteral("ContainerData_"),QStringLiteral("ContainerItems_")}) {
            if (key.startsWith(prefix) && !ids.contains(key.mid(prefix.size()))) { ids.append(key.mid(prefix.size())); }
        }
    }
    ids.removeDuplicates();
    for (const QString& id : ids) {
        document.containers.append(container(id,settings.value(QStringLiteral("ContainerData_")+id).toString(),settings.value(QStringLiteral("ContainerItems_")+id).toString()));
        const QString floatGeometry = settings.value(QStringLiteral("MeterDisplay_")+id+QStringLiteral("_Geometry")).toString();
        if (!floatGeometry.isEmpty()) { document.containers.last().config.insert(QStringLiteral("floatingGeometry"),floatGeometry); }
        if (document.mainContainerId.isEmpty() || document.containers.last().dockMode == DockMode::PanelDocked) { document.mainContainerId = id; }
    }
    for (const auto& alias : kAppletAliases) {
        const QString visibilityKey = QStringLiteral("Applet")+alias.first+QStringLiteral("Visible");
        const QString floatingKey = QStringLiteral("Applet")+alias.second+QStringLiteral("Floating");
        const QString geometryKey = QStringLiteral("Applet")+alias.second+QStringLiteral("FloatGeometry");
        if (!settings.contains(visibilityKey) && !settings.contains(floatingKey) && !settings.contains(geometryKey)) { continue; }
        if (document.mainContainerId.isEmpty()) {
            ContainerDocument main; main.id = stableId(QStringLiteral("workspace"),QStringLiteral("main")); main.name = QStringLiteral("Main applet area"); main.layout = ContentLayout::VerticalStack;
            document.mainContainerId = main.id; document.containers.append(main);
        }
        ContentEntry entry; entry.id = stableId(document.mainContainerId,QStringLiteral("applet:")+alias.second);
        entry.typeId = QStringLiteral("applet:")+alias.second; entry.name = alias.first;
        entry.visible = settings.value(visibilityKey,QStringLiteral("True")).toString() != QLatin1String("False");
        entry.config = {{QStringLiteral("floating"),settings.value(floatingKey).toString() == QLatin1String("True")},{QStringLiteral("floatGeometry"),settings.value(geometryKey).toString()}};
        entry.context = {{QStringLiteral("legacyVisibilityId"),alias.first},{QStringLiteral("legacyWidgetId"),alias.second}};
        for (auto& c : document.containers) { if (c.id == document.mainContainerId) { c.contents.append(entry); break; } }
    }
    if (!backup.isEmpty()) { document.extensions.insert(QStringLiteral("legacySettings"),backup); }
    return finish(document);
}
DocumentResult LegacyContainerImporter::fromContainerFile(const QByteArray& bytes) {
    QByteArray trimmed = bytes.trimmed();
    if (trimmed.startsWith(QByteArray::fromHex("efbbbf"))) { trimmed.remove(0,3); }
    if (trimmed.startsWith('{') || trimmed.startsWith('[')) {
        const QByteArray firstLine = trimmed.left(trimmed.indexOf('\n') < 0 ? trimmed.size() : trimmed.indexOf('\n'));
        const QJsonObject firstObject = QJsonDocument::fromJson(firstLine).object();
        // Historical composite items are compact kind-bearing JSON records.
        // A structured workspace (including malformed/future schema fields)
        // must always take the strict codec path, with no legacy fallback.
        if (firstObject.contains(QStringLiteral("schemaVersion")) || firstObject.value(QStringLiteral("kind")).toString().isEmpty()) { return ContainerDocumentCodec::decode(bytes); }
    }
    if (bytes.isEmpty()) { return {false,{},QStringLiteral("Empty container payload")}; }
    const QString text = QString::fromUtf8(bytes); const int newline = text.indexOf(QLatin1Char('\n'));
    const QString first = newline < 0 ? text : text.left(newline);
    WorkspaceDocument document;
    const bool metadata = (first.section(QLatin1Char('|'),6,6) == QLatin1String("true") || first.section(QLatin1Char('|'),6,6) == QLatin1String("false")) && first.section(QLatin1Char('|'),0,0) != QLatin1String("GROUP") && first.split(QLatin1Char('|')).size() >= 13;
    const QString id = metadata ? first.section(QLatin1Char('|'),0,0) : stableId(QStringLiteral("import"),QStringLiteral("canvas"));
    document.containers.append(container(id,metadata ? first : QString(),metadata ? (newline < 0 ? QString() : text.mid(newline+1)) : text));
    document.mainContainerId = id; document.extensions.insert(QStringLiteral("legacyPayloadBase64"),QString::fromLatin1(bytes.toBase64()));
    return finish(document);
}
DocumentResult LegacyContainerImporter::fromClipboard(const QString& text) {
    // Current clipboard export is Base64 of UTF-8 newline-delimited item
    // records; raw clipboard text remains accepted for older callers.
    const QString trimmed = text.trimmed();
    if (trimmed.startsWith(QLatin1String("GROUP\n")) || trimmed.contains(QLatin1Char('|')) || trimmed.startsWith(QLatin1Char('{')) || trimmed.startsWith(QLatin1Char('['))) { return fromContainerFile(text.toUtf8()); }
    const QByteArray encoded = trimmed.toLatin1();
    const auto decoded = QByteArray::fromBase64Encoding(encoded,QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || encoded.isEmpty() || decoded.decoded.isEmpty() || decoded.decoded.toBase64() != encoded || QString::fromUtf8(decoded.decoded).toUtf8() != decoded.decoded) {
        return {false,{},QStringLiteral("Invalid Base64 UTF-8 clipboard payload")};
    }
    DocumentResult result = fromContainerFile(decoded.decoded);
    if (result.ok) { result.document.extensions.insert(QStringLiteral("legacyClipboardText"),text); }
    return result;
}
} // namespace NereusSDR
