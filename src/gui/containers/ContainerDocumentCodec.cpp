// no-port-check: NereusSDR-original lossless presentation JSON codec.
#include "ContainerDocumentCodec.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>
#include <cmath>
#include <limits>

namespace NereusSDR {
namespace {
const QStringList kWorkspaceFields{"schemaVersion", "revision", "mainContainerId", "containers"};
const QStringList kContainerFields{"id", "name", "contents", "layout", "dockMode", "anchor", "header",
                                   "geometry", "visible", "locked", "autoHeight", "popOutShell", "config"};
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
    return true;
}

bool decodeContainer(const QJsonObject& object, ContainerDocument& container)
{
    if (!stringField(object, "id", container.id) || !stringField(object, "name", container.name)
        || !enumField(object, "layout", container.layout) || !enumField(object, "dockMode", container.dockMode)
        || !enumField(object, "anchor", container.anchor) || !enumField(object, "header", container.header)
        || !geometryField(object, container.geometry) || !boolField(object, "visible", container.visible)
        || !boolField(object, "locked", container.locked) || !boolField(object, "autoHeight", container.autoHeight)
        || !boolField(object, "popOutShell", container.popOutShell) || !objectField(object, "config", container.config)
        || !object["contents"].isArray()) {
        return false;
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
    object["anchor"] = int(container.anchor);
    object["header"] = int(container.header);
    object["geometry"] = QJsonArray{container.geometry.x(), container.geometry.y(),
                                  container.geometry.width(), container.geometry.height()};
    object["visible"] = container.visible;
    object["locked"] = container.locked;
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
        if (int(container.layout) < 0 || int(container.layout) > int(ContentLayout::VerticalStack)
            || int(container.dockMode) < 0 || int(container.dockMode) > int(DockMode::Floating)
            || int(container.anchor) < 0 || int(container.anchor) > int(AxisLock::BottomLeft)
            || int(container.header) < 0 || int(container.header) > int(HeaderMode::Hidden)) {
            return QStringLiteral("Invalid layout mode in container %1").arg(container.id);
        }
        if (container.geometry.width() < 0 || container.geometry.height() < 0) {
            return QStringLiteral("Invalid geometry in container %1").arg(container.id);
        }
        if (collides(container.extensions, kContainerFields)) {
            return QStringLiteral("Container extensions collide with known fields: %1").arg(container.id);
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
    if (!document.mainContainerId.isEmpty() && !containerIds.contains(document.mainContainerId)) {
        return QStringLiteral("mainContainerId does not name a container");
    }
    return {};
}
} // namespace NereusSDR
