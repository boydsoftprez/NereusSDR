#pragma once
// no-port-check: NereusSDR-original GUI presentation value records.
#include "ContainerTypes.h"
#include <QJsonObject>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QVector>
#include <optional>

namespace NereusSDR {

struct ReturnLocation {
    QString containerId, beforeId, afterId;
    QJsonObject extensions;
    bool operator==(const ReturnLocation&) const = default;
};

struct ContentEntry {
    QString id, typeId, name;
    QJsonObject context, config, extensions;
    QRectF canvasRect;
    int paintOrder = 0;
    bool visible = true;
    std::optional<ReturnLocation> returnLocation;
    std::optional<QRectF> freeCanvasRect() const;
    void setFreeCanvasRect(const QRectF&);
    bool operator==(const ContentEntry&) const = default;
};

struct ContainerDocument {
    QString id, name;
    QVector<ContentEntry> contents;
    ContentLayout layout = ContentLayout::LegacyCanvas;
    DockMode dockMode = DockMode::PanelDocked;
    AxisLock anchor = AxisLock::Left;
    HeaderMode header = HeaderMode::Always;
    QRect geometry;
    bool visible = true;
    bool locked = false;
    bool autoHeight = false;
    bool popOutShell = false;
    QJsonObject config, extensions;
    QSizeF freeCanvasExtent() const;
    void setFreeCanvasExtent(const QSizeF&);
    bool operator==(const ContainerDocument&) const = default;
};

struct WorkspaceDocument {
    int schemaVersion = 1;
    quint64 revision = 0;
    QString mainContainerId;
    QVector<ContainerDocument> containers;
    QJsonObject extensions;
    bool operator==(const WorkspaceDocument&) const = default;
};

struct DocumentResult {
    bool ok = false;
    WorkspaceDocument document;
    QString error;
};

enum class CommitStatus { Saved, Conflict, Invalid, StorageError };
struct CommitResult {
    CommitStatus status = CommitStatus::Invalid;
    quint64 revision = 0;
    QString error;
};

} // namespace NereusSDR
