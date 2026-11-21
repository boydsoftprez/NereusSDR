#pragma once
// no-port-check: NereusSDR-original GUI presentation value records.
// Modification history (NereusSDR):
//   2026-10-11 - SavedScreen::extensions: unknown keys inside "screen" are
//                 kept. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - ContainerHome::geometryPane: the pane a free home's rect
//                 was placed in, so Return reads it against that pane.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - ContainerHome and ContainerDocument::home: where a
//                 popped-out container came from, so Return puts it back.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - header is Shown or Hidden. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-11 - SavedScreen and ContainerDocument::screen: the monitor a
//                 floating or popped-out window was placed on, by J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - geometryPane: the pane size a free container's place was
//                 seen at, by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-07 - The panel-docked edge and the workspace arrangement
//                 (strip thickness, extents, grid alignment) by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
#include "ContainerTypes.h"
#include <QJsonObject>
#include <QMap>
#include <QRect>
#include <QRectF>
#include <QSize>
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

// The monitor a floating or popped-out window was on when the operator
// placed it. A present screen matches it by serial when that is not empty,
// else by name and geometry together.
struct SavedScreen {
    QString name;
    QString serial;
    QRect geometry;
    // Keys this build does not know, kept on rewrite. They play no part in
    // matching a screen.
    QJsonObject extensions;
    bool operator==(const SavedScreen&) const = default;
};

// Where a popped-out container came from (workspace look): the strip and
// its neighbours there, as ReturnLocation does for entries, or its free
// rect over the pane with the pane size that rect was placed in (the
// baseline its anchor works from; empty when none was known). Held only
// while the container floats.
struct ContainerHome {
    DockMode dockMode = DockMode::PanelDocked;
    DockEdge edge = DockEdge::Right;
    QString beforeId, afterId;
    QRect geometry;
    std::optional<QSize> geometryPane;
    QJsonObject extensions;
    bool operator==(const ContainerHome&) const = default;
};

struct ContainerDocument {
    QString id, name;
    QVector<ContentEntry> contents;
    ContentLayout layout = ContentLayout::LegacyCanvas;
    DockMode dockMode = DockMode::PanelDocked;
    // The strip a PanelDocked container sits in (workspace preview).
    DockEdge edge = DockEdge::Right;
    AxisLock anchor = AxisLock::Left;
    HeaderMode header = HeaderMode::Shown;
    QRect geometry;
    // Free (OverlayDocked) containers only: the size the pane had when the
    // operator last placed the container, so it comes back where it was
    // showing. Empty in documents from before 2026-10-11 and until the
    // operator moves the container.
    std::optional<QSize> geometryPane;
    // Floating and popped-out containers only: the monitor `geometry` was
    // chosen on. Empty in documents from before 2026-10-11 and until the
    // operator moves the window.
    std::optional<SavedScreen> screen;
    // Floating containers only (workspace look): the place the container
    // was popped out of. Empty for a container created as a window and in
    // documents from before 2026-10-11.
    std::optional<ContainerHome> home;
    bool visible = true;
    bool locked = false;
    // Helper-linked visibility, as Thetis's ucMeter.LinkedHFAuto /
    // LinkedWaveNode: the container is shown only while that helper is
    // enabled.
    bool linkedHfAuto = false;
    bool linkedWaveNode = false;
    // Lock item positions inside this container: the frame and its membership
    // stay editable, but objects cannot be dragged, nudged or resized. Thetis
    // keeps this separate from the container Lock (ucMeter.LockItemPositions).
    bool lockItemPositions = false;
    bool autoHeight = false;
    bool popOutShell = false;
    QJsonObject config, extensions;
    QSizeF freeCanvasExtent() const;
    void setFreeCanvasExtent(const QSizeF&);
    bool operator==(const ContainerDocument&) const = default;
};

// One edge strip's saved size: its thickness across the strip and each
// container's extent along it. Zero thickness means the edge default.
struct EdgeStripLayout {
    int thickness = 0;
    QMap<QString, int> extents;
    QJsonObject extensions;
    bool operator==(const EdgeStripLayout&) const = default;
};

// Workspace preview arrangement. Empty (the default) is not written, so
// a workspace saved with the preview off keeps its exact bytes.
struct WorkspaceArrangement {
    QMap<DockEdge, EdgeStripLayout> strips;
    bool gridAlignment = false;
    QJsonObject extensions;
    bool isEmpty() const { return strips.isEmpty() && !gridAlignment && extensions.isEmpty(); }
    bool operator==(const WorkspaceArrangement&) const = default;
};

struct WorkspaceDocument {
    int schemaVersion = 1;
    quint64 revision = 0;
    QString mainContainerId;
    QVector<ContainerDocument> containers;
    WorkspaceArrangement arrangement;
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
