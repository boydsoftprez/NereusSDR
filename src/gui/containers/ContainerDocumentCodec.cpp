// no-port-check: NereusSDR-original lossless presentation JSON codec.
// Modification history (NereusSDR):
//   2026-10-25 - decodeEntry runs migrateEntryOnLoad on every entry it
//                 decodes. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - Unknown keys inside screen are kept, as they are inside
//                 home. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - A free home carries geometryPane, the pane its rect was
//                 placed in. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - home, written for floating containers only: where a
//                 popped-out container came from. An unreadable one is
//                 dropped for that container alone; unknown keys inside it
//                 are kept. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - locked is written only when true; a document without the
//                 key loads unlocked, as before. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-11 - screen, written for floating and popped-out containers
//                 only; an unreadable screen, or an unreadable geometry of
//                 a floating container, costs that container its place and
//                 nothing else. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-11 - geometryPane, written for free containers only and read
//                 leniently, by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-07 - The panel-docked edge and the workspace arrangement, each
//                 written only when not the default by J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
#include "ContainerDocumentCodec.h"
#include "LegacyContainerImporter.h"
#include "ContainerContentRegistry.h"
#include "ContentEntryMigration.h"
#include "core/LogCategories.h"
#include <QUuid>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>
#include <cmath>
#include <limits>

namespace NereusSDR {
namespace {
const QStringList kWorkspaceFields{"schemaVersion", "revision", "mainContainerId", "containers", "arrangement"};
const QStringList kContainerFields{"id", "name", "contents", "layout", "dockMode", "edge", "anchor", "header",
                                   "geometry", "geometryPane", "screen", "home", "visible", "locked", "autoHeight", "popOutShell",
                                   "config"};
const QStringList kScreenFields{"name", "serial", "geometry"};
const QStringList kHomeFields{"dockMode", "edge", "before", "after", "geometry", "geometryPane"};
const QStringList kArrangementFields{"strips", "gridAlignment"};
const QStringList kStripFields{"thickness", "extents"};

// Edge names as saved. Right is the default and is never written.
QString edgeName(DockEdge edge)
{
    switch (edge) {
    case DockEdge::Left:
        return QStringLiteral("Left");
    case DockEdge::Right:
        return QStringLiteral("Right");
    case DockEdge::Top:
        return QStringLiteral("Top");
    case DockEdge::Bottom:
        return QStringLiteral("Bottom");
    }
    return {};
}

bool edgeFromName(const QString& name, DockEdge& out)
{
    for (DockEdge edge : {DockEdge::Left, DockEdge::Right, DockEdge::Top, DockEdge::Bottom}) {
        if (name == edgeName(edge)) {
            out = edge;
            return true;
        }
    }
    return false;
}
const QStringList kContentFields{"id", "typeId", "name", "context", "config", "canvasRect", "paintOrder",
                                 "visible", "returnLocation"};
const QStringList kReturnFields{"containerId", "beforeId", "afterId"};

QJsonObject extensions(QJsonObject object, const QStringList& fields)
{
    for (const QString& field : fields) {
        object.remove(field);
    }
    return object;
}

bool collides(const QJsonObject& object, const QStringList& fields)
{
    for (const QString& field : fields) {
        if (object.contains(field)) {
            return true;
        }
    }
    return false;
}

bool integer(const QJsonValue& value, int& out)
{
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::trunc(number) != number
        || number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()) {
        return false;
    }
    out = static_cast<int>(number);
    return true;
}

bool revision(const QJsonValue& value, quint64& out)
{
    if (value.isString()) {
        const QString text = value.toString();
        if (text.isEmpty()) {
            return false;
        }
        for (const QChar ch : text) {
            if (ch < QLatin1Char('0') || ch > QLatin1Char('9')) {
                return false;
            }
        }
        bool ok = false;
        out = text.toULongLong(&ok);
        return ok;
    }
    // Accept older numeric writers only in the range with unambiguous JSON
    // double precision. Our writer uses decimal strings for all 64 bits.
    constexpr double kLargestExactJsonInteger = 9007199254740991.0;
    const double number = value.toDouble(-1);
    if (!value.isDouble() || number < 0 || number > kLargestExactJsonInteger
        || std::trunc(number) != number) {
        return false;
    }
    out = static_cast<quint64>(number);
    return true;
}

bool stringField(const QJsonObject& object, const QString& key, QString& out)
{
    if (!object.contains(key)) {
        return true;
    }
    if (!object[key].isString()) {
        return false;
    }
    out = object[key].toString();
    return true;
}

bool boolField(const QJsonObject& object, const QString& key, bool& out)
{
    if (!object.contains(key)) {
        return true;
    }
    if (!object[key].isBool()) {
        return false;
    }
    out = object[key].toBool();
    return true;
}

bool objectField(const QJsonObject& object, const QString& key, QJsonObject& out)
{
    if (!object.contains(key)) {
        return true;
    }
    if (!object[key].isObject()) {
        return false;
    }
    out = object[key].toObject();
    return true;
}

template<typename Enum>
bool enumField(const QJsonObject& object, const QString& key, Enum& out)
{
    if (!object.contains(key)) {
        return true;
    }
    int number = 0;
    if (!integer(object[key], number)) {
        return false;
    }
    out = static_cast<Enum>(number);
    return true;
}

bool rectField(const QJsonObject& object, const QString& key, QRectF& out)
{
    if (!object.contains(key)) {
        return true;
    }
    const QJsonValue value = object[key];
    if (!value.isArray()) {
        return false;
    }
    const QJsonArray array = value.toArray();
    if (array.size() != 4) {
        return false;
    }
    for (const QJsonValue coordinate : array) {
        if (!coordinate.isDouble() || !std::isfinite(coordinate.toDouble())) {
            return false;
        }
    }
    out = QRectF(array[0].toDouble(), array[1].toDouble(), array[2].toDouble(), array[3].toDouble());
    return true;
}

bool geometryField(const QJsonObject& object, QRect& out)
{
    if (!object.contains("geometry")) {
        return true;
    }
    const QJsonValue value = object["geometry"];
    if (!value.isArray() || value.toArray().size() != 4) {
        return false;
    }
    const QJsonArray array = value.toArray();
    int coordinates[4]{};
    for (int i = 0; i < 4; ++i) {
        if (!integer(array[i], coordinates[i])) {
            return false;
        }
    }
    // QRect stores the far edge as well; avoid overflowing it on hostile data.
    const qint64 right = qint64(coordinates[0]) + coordinates[2] - 1;
    const qint64 bottom = qint64(coordinates[1]) + coordinates[3] - 1;
    if (right < std::numeric_limits<int>::min() || right > std::numeric_limits<int>::max()
        || bottom < std::numeric_limits<int>::min() || bottom > std::numeric_limits<int>::max()) {
        return false;
    }
    out.setCoords(coordinates[0], coordinates[1], static_cast<int>(right), static_cast<int>(bottom));
    return true;
}

// [width, height] of the pane a free container's place was seen at.
bool paneSize(const QJsonValue& value, QSize& out)
{
    if (!value.isArray() || value.toArray().size() != 2) {
        return false;
    }
    const QJsonArray array = value.toArray();
    int width = 0;
    int height = 0;
    if (!integer(array[0], width) || !integer(array[1], height) || width <= 0 || height <= 0) {
        return false;
    }
    out = QSize(width, height);
    return true;
}

// {"name", "serial", "geometry": [x, y, w, h]} of the monitor a window was on.
bool savedScreen(const QJsonValue& value, SavedScreen& out)
{
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    if (!object["name"].isString() || !object["serial"].isString() || !object.contains("geometry")) {
        return false;
    }
    QRect geometry;
    if (!geometryField(object, geometry) || !geometry.isValid()) {
        return false;
    }
    out = {object["name"].toString(), object["serial"].toString(), geometry, extensions(object, kScreenFields)};
    return true;
}

bool nonNegativeInteger(const QJsonValue& value, int& out)
{
    return integer(value, out) && out >= 0;
}

// The arrangement is strict about the fields it knows and keeps any other
// member as an extension, like every other record.
bool decodeArrangement(const QJsonValue& value, WorkspaceArrangement& arrangement)
{
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    if (!boolField(object, "gridAlignment", arrangement.gridAlignment)) {
        return false;
    }
    if (object.contains("strips")) {
        if (!object["strips"].isObject()) {
            return false;
        }
        const QJsonObject strips = object["strips"].toObject();
        for (auto it = strips.constBegin(); it != strips.constEnd(); ++it) {
            DockEdge edge = DockEdge::Right;
            if (!edgeFromName(it.key(), edge) || !it.value().isObject()) {
                return false;
            }
            const QJsonObject stripObject = it.value().toObject();
            EdgeStripLayout strip;
            if (stripObject.contains("thickness")
                && !nonNegativeInteger(stripObject["thickness"], strip.thickness)) {
                return false;
            }
            if (stripObject.contains("extents")) {
                if (!stripObject["extents"].isObject()) {
                    return false;
                }
                const QJsonObject extents = stripObject["extents"].toObject();
                for (auto e = extents.constBegin(); e != extents.constEnd(); ++e) {
                    int extent = 0;
                    if (!nonNegativeInteger(e.value(), extent)) {
                        return false;
                    }
                    strip.extents.insert(e.key(), extent);
                }
            }
            strip.extensions = extensions(stripObject, kStripFields);
            arrangement.strips.insert(edge, strip);
        }
    }
    arrangement.extensions = extensions(object, kArrangementFields);
    return true;
}

QJsonObject encodeArrangement(const WorkspaceArrangement& arrangement)
{
    QJsonObject object = arrangement.extensions;
    if (!arrangement.strips.isEmpty()) {
        QJsonObject strips;
        for (auto it = arrangement.strips.constBegin(); it != arrangement.strips.constEnd(); ++it) {
            QJsonObject strip = it.value().extensions;
            strip["thickness"] = it.value().thickness;
            QJsonObject extents;
            for (auto e = it.value().extents.constBegin(); e != it.value().extents.constEnd(); ++e) {
                extents[e.key()] = e.value();
            }
            strip["extents"] = extents;
            strips[edgeName(it.key())] = strip;
        }
        object["strips"] = strips;
    }
    if (arrangement.gridAlignment) {
        object["gridAlignment"] = true;
    }
    return object;
}

bool decodeEntry(const QJsonObject& object, ContentEntry& entry)
{
    if (!stringField(object, "id", entry.id) || !stringField(object, "typeId", entry.typeId)
        || !stringField(object, "name", entry.name) || !objectField(object, "context", entry.context)
        || !objectField(object, "config", entry.config) || !rectField(object, "canvasRect", entry.canvasRect)
        || !boolField(object, "visible", entry.visible)) {
        return false;
    }
    if (object.contains("paintOrder") && !integer(object["paintOrder"], entry.paintOrder)) {
        return false;
    }
    if (object.contains("returnLocation")) {
        if (!object["returnLocation"].isObject()) {
            return false;
        }
        const QJsonObject locationObject = object["returnLocation"].toObject();
        ReturnLocation location;
        if (!stringField(locationObject, "containerId", location.containerId)
            || !stringField(locationObject, "beforeId", location.beforeId)
            || !stringField(locationObject, "afterId", location.afterId)) {
            return false;
        }
        location.extensions = extensions(locationObject, kReturnFields);
        entry.returnLocation = location;
    }
    entry.extensions = extensions(object, kContentFields);
    // Every load path (workspace store, profiles, imports, duplicates)
    // decodes its entries here.
    migrateEntryOnLoad(entry);
    return true;
}

// Where a popped-out container came from. A strip home names its edge; a
// free home has a rect with a size.
bool containerHome(const QJsonValue& value, ContainerHome& out)
{
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    int mode = -1;
    if (!integer(object["dockMode"], mode)
        || (mode != int(DockMode::PanelDocked) && mode != int(DockMode::OverlayDocked))) {
        return false;
    }
    out.dockMode = static_cast<DockMode>(mode);
    if (object.contains("edge")
        && (!object["edge"].isString() || !edgeFromName(object["edge"].toString(), out.edge))) {
        return false;
    }
    if (!stringField(object, "before", out.beforeId) || !stringField(object, "after", out.afterId)
        || !geometryField(object, out.geometry)) {
        return false;
    }
    if (out.dockMode == DockMode::OverlayDocked && !out.geometry.isValid()) {
        return false;
    }
    if (object.contains("geometryPane")) {
        QSize pane;
        if (!paneSize(object["geometryPane"], pane)) {
            return false;
        }
        out.geometryPane = pane;
    }
    out.extensions = extensions(object, kHomeFields);
    return true;
}

bool decodeContainer(const QJsonObject& object, ContainerDocument& container)
{
    if (!stringField(object, "id", container.id) || !stringField(object, "name", container.name)
        || !enumField(object, "layout", container.layout) || !enumField(object, "dockMode", container.dockMode)
        || !enumField(object, "anchor", container.anchor) || !enumField(object, "header", container.header)
        || !boolField(object, "visible", container.visible)
        || !boolField(object, "locked", container.locked) || !boolField(object, "autoHeight", container.autoHeight)
        || !boolField(object, "popOutShell", container.popOutShell) || !objectField(object, "config", container.config)
        || !object["contents"].isArray()) {
        return false;
    }
    if (object.contains("edge")
        && (!object["edge"].isString() || !edgeFromName(object["edge"].toString(), container.edge))) {
        return false;
    }
    if (!geometryField(object, container.geometry)) {
        if (container.dockMode != DockMode::Floating) {
            return false;
        }
        // Lenient for a window: it loses its place, not the whole layout,
        // and is shown at a default place until the operator moves it.
        container.geometry = QRect();
        qCWarning(lcContainer) << "Container" << container.id
                               << "has an unreadable geometry; it takes a default place";
    }
    if (object.contains("screen")) {
        // Lenient in the same way: without its monitor the window is placed
        // by its geometry alone.
        SavedScreen screen;
        if (savedScreen(object["screen"], screen)) {
            container.screen = screen;
        } else {
            qCWarning(lcContainer) << "Container" << container.id
                                   << "has an unreadable screen; it is ignored";
        }
    }
    if (object.contains("home")) {
        // Lenient: without it Return docks the container at the default
        // place, and nothing else is lost.
        ContainerHome home;
        if (containerHome(object["home"], home)) {
            container.home = home;
        } else {
            qCWarning(lcContainer) << "Container" << container.id
                                   << "has an unreadable home; it is ignored";
        }
    }
    if (object.contains("geometryPane")) {
        // Lenient: a value that does not read is dropped for this container
        // alone, which then takes the pane's size at start as its baseline.
        QSize pane;
        if (paneSize(object["geometryPane"], pane)) {
            container.geometryPane = pane;
        } else {
            qCWarning(lcContainer) << "Container" << container.id
                                   << "has an unreadable geometryPane; it is ignored";
        }
    }
    for (const QJsonValue value : object["contents"].toArray()) {
        ContentEntry entry;
        if (!value.isObject() || !decodeEntry(value.toObject(), entry)) {
            return false;
        }
        container.contents.append(entry);
    }
    container.extensions = extensions(object, kContainerFields);
    return true;
}

QJsonObject encodeEntry(const ContentEntry& entry)
{
    QJsonObject object = entry.extensions;
    object["id"] = entry.id;
    object["typeId"] = entry.typeId;
    object["name"] = entry.name;
    object["context"] = entry.context;
    object["config"] = entry.config;
    object["canvasRect"] = QJsonArray{entry.canvasRect.x(), entry.canvasRect.y(),
                                    entry.canvasRect.width(), entry.canvasRect.height()};
    object["paintOrder"] = entry.paintOrder;
    object["visible"] = entry.visible;
    if (entry.returnLocation) {
        const ReturnLocation& location = *entry.returnLocation;
        QJsonObject returnObject = location.extensions;
        returnObject["containerId"] = location.containerId;
        returnObject["beforeId"] = location.beforeId;
        returnObject["afterId"] = location.afterId;
        object["returnLocation"] = returnObject;
    }
    return object;
}

QJsonObject encodeContainer(const ContainerDocument& container)
{
    QJsonObject object = container.extensions;
    object["id"] = container.id;
    object["name"] = container.name;
    QJsonArray contents;
    for (const ContentEntry& entry : container.contents) {
        contents.append(encodeEntry(entry));
    }
    object["contents"] = contents;
    object["layout"] = int(container.layout);
    object["dockMode"] = int(container.dockMode);
    // Written only off the default, so existing documents keep their bytes.
    if (container.edge != DockEdge::Right) {
        object["edge"] = edgeName(container.edge);
    }
    object["anchor"] = int(container.anchor);
    object["header"] = int(container.header);
    object["geometry"] = QJsonArray{container.geometry.x(), container.geometry.y(),
                                  container.geometry.width(), container.geometry.height()};
    // Free containers only, and only once the operator has placed one, so
    // every other document keeps its bytes.
    if (container.geometryPane && container.dockMode == DockMode::OverlayDocked
        && container.geometryPane->width() > 0 && container.geometryPane->height() > 0) {
        object["geometryPane"] = QJsonArray{container.geometryPane->width(), container.geometryPane->height()};
    }
    // Windows only, and only once the operator has placed one.
    if (container.screen && (container.dockMode == DockMode::Floating || container.popOutShell)) {
        const QRect& on = container.screen->geometry;
        QJsonObject screen = container.screen->extensions;
        screen["name"] = container.screen->name;
        screen["serial"] = container.screen->serial;
        screen["geometry"] = QJsonArray{on.x(), on.y(), on.width(), on.height()};
        object["screen"] = screen;
    }
    // Only while it floats: where Return puts it back.
    if (container.home && container.dockMode == DockMode::Floating) {
        const ContainerHome& from = *container.home;
        QJsonObject home = from.extensions;
        home["dockMode"] = int(from.dockMode);
        if (from.dockMode == DockMode::PanelDocked) {
            home["edge"] = edgeName(from.edge);
            home["before"] = from.beforeId;
            home["after"] = from.afterId;
        } else {
            home["geometry"] = QJsonArray{from.geometry.x(), from.geometry.y(),
                                          from.geometry.width(), from.geometry.height()};
            // The pane that rect was placed in, when one was known.
            if (from.geometryPane && from.geometryPane->width() > 0 && from.geometryPane->height() > 0) {
                home["geometryPane"] = QJsonArray{from.geometryPane->width(), from.geometryPane->height()};
            }
        }
        object["home"] = home;
    }
    object["visible"] = container.visible;
    // Only when set: the workspace look has no Lock setting and writes none.
    if (container.locked) {
        object["locked"] = true;
    }
    object["autoHeight"] = container.autoHeight;
    object["popOutShell"] = container.popOutShell;
    object["config"] = container.config;
    return object;
}
} // namespace

DocumentResult ContainerDocumentCodec::decode(const QByteArray& json)
{
    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return {false, {}, QStringLiteral("Invalid workspace JSON: %1").arg(parseError.errorString())};
    }
    if (!parsed.isObject()) {
        return {false, {}, QStringLiteral("Workspace must be a JSON object")};
    }
    const QJsonObject root = parsed.object();
    WorkspaceDocument document;
    if (!integer(root["schemaVersion"], document.schemaVersion)) {
        return {false, {}, QStringLiteral("Workspace schemaVersion must be an integer")};
    }
    // Check before interpreting any other field. Future documents stay opaque.
    if (document.schemaVersion != 1) {
        return {false, {}, QStringLiteral("Unsupported workspace schema version %1; preserved read-only")
                               .arg(document.schemaVersion)};
    }
    if (!revision(root["revision"], document.revision) || !root["mainContainerId"].isString()
        || !root["containers"].isArray()) {
        return {false, {}, QStringLiteral("Invalid workspace revision, mainContainerId or containers")};
    }
    document.mainContainerId = root["mainContainerId"].toString();
    if (root.contains("arrangement") && !decodeArrangement(root["arrangement"], document.arrangement)) {
        return {false, {}, QStringLiteral("Invalid workspace arrangement")};
    }
    for (const QJsonValue value : root["containers"].toArray()) {
        ContainerDocument container;
        if (!value.isObject() || !decodeContainer(value.toObject(), container)) {
            return {false, {}, QStringLiteral("Invalid container or content field type")};
        }
        document.containers.append(container);
    }
    document.extensions = extensions(root, kWorkspaceFields);
    const QString error = validate(document);
    if (!error.isEmpty()) {
        return {false, {}, error};
    }
    return {true, document, {}};
}

QByteArray ContainerDocumentCodec::encode(const WorkspaceDocument& document)
{
    QJsonObject root = document.extensions;
    root["schemaVersion"] = document.schemaVersion;
    root["revision"] = QString::number(document.revision);
    root["mainContainerId"] = document.mainContainerId;
    QJsonArray containers;
    for (const ContainerDocument& container : document.containers) {
        containers.append(encodeContainer(container));
    }
    root["containers"] = containers;
    if (!document.arrangement.isEmpty()) {
        root["arrangement"] = encodeArrangement(document.arrangement);
    }
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QString ContainerDocumentCodec::validate(const WorkspaceDocument& document)
{
    if (document.schemaVersion != 1) {
        return QStringLiteral("Only workspace schema version 1 is editable");
    }
    if (collides(document.extensions, kWorkspaceFields)) {
        return QStringLiteral("Workspace extensions collide with known fields");
    }
    QSet<QString> containerIds, contentIds;
    for (const ContainerDocument& container : document.containers) {
        if (container.id.trimmed().isEmpty() || containerIds.contains(container.id)) {
            return QStringLiteral("Container IDs must be nonempty and unique: %1").arg(container.id);
        }
        containerIds.insert(container.id);
        if (int(container.layout) < 0 || int(container.layout) > int(ContentLayout::FreeCanvas)
            || int(container.dockMode) < 0 || int(container.dockMode) > int(DockMode::Floating)
            || int(container.anchor) < 0 || int(container.anchor) > int(AxisLock::BottomLeft)
            || int(container.header) < 0 || int(container.header) > int(HeaderMode::Hidden)
            || int(container.edge) < 0 || int(container.edge) > int(DockEdge::Bottom)) {
            return QStringLiteral("Invalid layout mode in container %1").arg(container.id);
        }
        if (container.geometry.width() < 0 || container.geometry.height() < 0) {
            return QStringLiteral("Invalid geometry in container %1").arg(container.id);
        }
        if (collides(container.extensions, kContainerFields)) {
            return QStringLiteral("Container extensions collide with known fields: %1").arg(container.id);
        }
        if (container.screen && collides(container.screen->extensions, kScreenFields)) {
            return QStringLiteral("Screen extensions collide with known fields: %1").arg(container.id);
        }
        if (container.home && collides(container.home->extensions, kHomeFields)) {
            return QStringLiteral("Home extensions collide with known fields: %1").arg(container.id);
        }
        for (const ContentEntry& entry : container.contents) {
            if (entry.id.trimmed().isEmpty() || contentIds.contains(entry.id)) {
                return QStringLiteral("Content IDs must be nonempty and globally unique: %1").arg(entry.id);
            }
            contentIds.insert(entry.id);
            if (entry.typeId.trimmed().isEmpty()) {
                return QStringLiteral("Content typeId must be nonempty: %1").arg(entry.id);
            }
            const QRectF& rect = entry.canvasRect;
            if (!std::isfinite(rect.x()) || !std::isfinite(rect.y()) || !std::isfinite(rect.width())
                || !std::isfinite(rect.height()) || rect.width() < 0 || rect.height() < 0) {
                return QStringLiteral("Invalid canvas rectangle: %1").arg(entry.id);
            }
            if (collides(entry.extensions, kContentFields)) {
                return QStringLiteral("Content extensions collide with known fields: %1").arg(entry.id);
            }
            if (entry.returnLocation && collides(entry.returnLocation->extensions, kReturnFields)) {
                return QStringLiteral("Return location extensions collide with known fields: %1").arg(entry.id);
            }
        }
    }
    for (auto it = document.arrangement.strips.constBegin(); it != document.arrangement.strips.constEnd(); ++it) {
        if (int(it.key()) < 0 || int(it.key()) > int(DockEdge::Bottom) || it.value().thickness < 0
            || collides(it.value().extensions, kStripFields)) {
            return QStringLiteral("Invalid edge strip in the workspace arrangement");
        }
        for (int extent : it.value().extents) {
            if (extent < 0) {
                return QStringLiteral("Invalid edge strip extent in the workspace arrangement");
            }
        }
    }
    if (collides(document.arrangement.extensions, kArrangementFields)) {
        return QStringLiteral("Workspace arrangement extensions collide with known fields");
    }
    if (!document.mainContainerId.isEmpty() && !containerIds.contains(document.mainContainerId)) {
        return QStringLiteral("mainContainerId does not name a container");
    }
    return {};
}

namespace {
QByteArray portable(const WorkspaceDocument& document, const QString& format)
{
    return QJsonDocument(QJsonObject{{"format",format},{"schemaVersion",1},
        {"workspace",QJsonDocument::fromJson(ContainerDocumentCodec::encode(document)).object()}}).toJson(QJsonDocument::Compact);
}
DocumentResult recoverAndRegenerate(DocumentResult result, const QByteArray& bytes, const QJsonObject& envelope = {})
{
    if (!result.ok) { return result; }
    QHash<QString,QString> containers, entries;
    for (const auto& c : result.document.containers) {
        containers[c.id]=QUuid::createUuid().toString(QUuid::WithoutBraces);
        for (const auto& e : c.contents) { entries[e.id]=QUuid::createUuid().toString(QUuid::WithoutBraces); }
    }
    QJsonObject unknownEnvelope=envelope;
    unknownEnvelope.remove("workspace"); unknownEnvelope.remove("format"); unknownEnvelope.remove("schemaVersion");
    const QJsonObject recovery{{"sourceBase64",QString::fromLatin1(bytes.toBase64())},
        {"workspaceExtensions",result.document.extensions},{"envelopeExtensions",unknownEnvelope}};
    const auto retain=[&](QJsonObject& extensions) {
        bool hasOriginal=false;
        for(auto it=extensions.begin();it!=extensions.end();++it) {
            const auto payload=it.value().toObject();
            if(payload["format"]=="nereus.portable-recovery" && payload["schemaVersion"]==1 && payload["records"].isArray()) {
                for(const auto& record:payload["records"].toArray()) {if(record.toObject()["sourceBase64"].isString()) {hasOriginal=true;break;}}
            }
        }
        if(!hasOriginal) {ContainerDocumentCodec::retainPortableRecovery(extensions,recovery);}
        else if(!unknownEnvelope.isEmpty()) {ContainerDocumentCodec::retainPortableRecovery(extensions,QJsonObject{{"envelopeExtensions",unknownEnvelope}});}
    };
    result.document.mainContainerId=containers.value(result.document.mainContainerId);
    for (auto& c : result.document.containers) {
        c.id=containers.value(c.id); retain(c.extensions);
        for (auto& e : c.contents) {
            e.id=entries.value(e.id); retain(e.extensions);
            if (e.returnLocation) {
                auto& r=*e.returnLocation;
                r.containerId=containers.value(r.containerId,r.containerId);
                r.beforeId=entries.value(r.beforeId,r.beforeId);
                r.afterId=entries.value(r.afterId,r.afterId);
            }
        }
    }
    return result;
}
DocumentResult importPortable(const QByteArray& bytes, const QString& expected)
{
    const QJsonDocument json=QJsonDocument::fromJson(bytes);
    const QJsonObject root=json.object();
    if (root.contains("format") || root.contains("workspace")) {
        if (!json.isObject() || root.value("format")!=expected || root.value("schemaVersion")!=1 || !root.value("workspace").isObject()) {
            return {false,{},QStringLiteral("Unsupported or malformed portable content envelope")};
        }
        auto result=ContainerDocumentCodec::decode(QJsonDocument(root["workspace"].toObject()).toJson(QJsonDocument::Compact));
        if (result.ok && result.document.containers.size()!=1) { return {false,{},QStringLiteral("Portable content requires exactly one container")}; }
        if(result.ok) {
            ContainerContentRegistry registry;
            for(const auto& entry:result.document.containers[0].contents) {const QString error=registry.validateEntry(entry);if(!error.isEmpty()) {return {false,{},error};}}
        }
        return recoverAndRegenerate(result,bytes,root);
    }
    return recoverAndRegenerate(LegacyContainerImporter::fromContainerFile(bytes),bytes);
}
}
void ContainerDocumentCodec::retainPortableRecovery(QJsonObject& extensions,const QJsonObject& record)
{
    QString key=QStringLiteral("nereusPortableRecovery");
    int suffix=0;
    for(;;) {
        if(!extensions.contains(key)) {extensions[key]=QJsonObject{{"format","nereus.portable-recovery"},{"schemaVersion",1},{"records",QJsonArray{record}}};return;}
        auto payload=extensions[key].toObject();
        if(payload["format"]=="nereus.portable-recovery" && payload["schemaVersion"]==1 && payload["records"].isArray()) {
            auto records=payload["records"].toArray();
            if(!records.contains(record)) {records.append(record);payload["records"]=records;extensions[key]=payload;}
            return;
        }
        key=QStringLiteral("nereusPortableRecovery%1").arg(++suffix);
    }
}
QByteArray ContainerDocumentCodec::exportContainer(const ContainerDocument& container)
{
    WorkspaceDocument d; d.mainContainerId=container.id; d.containers={container};
    return portable(d,QStringLiteral("nereus.container"));
}
DocumentResult ContainerDocumentCodec::importContainer(const QByteArray& bytes)
{
    return importPortable(bytes,QStringLiteral("nereus.container"));
}
QString ContainerDocumentCodec::exportEntries(const QVector<ContentEntry>& entries)
{
    ContainerDocument c; c.id=QUuid::createUuid().toString(QUuid::WithoutBraces); c.name=QStringLiteral("Imported contents"); c.contents=entries;
    WorkspaceDocument d; d.mainContainerId=c.id; d.containers={c};
    return QString::fromUtf8(portable(d,QStringLiteral("nereus.entries")));
}
DocumentResult ContainerDocumentCodec::importEntries(const QString& text)
{
    const QString trimmed=text.trimmed();
    if (trimmed.startsWith('{') || trimmed.startsWith('[')) { return importPortable(text.toUtf8(),QStringLiteral("nereus.entries")); }
    auto result=LegacyContainerImporter::fromClipboard(text);
    return recoverAndRegenerate(result,text.toUtf8());
}
} // namespace NereusSDR
