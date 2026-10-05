// no-port-check: NereusSDR-original atomic layout transactions; no radio/model access.
// Modification history (NereusSDR):
//   2026-10-02 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "ContainerArrangeController.h"
#include "ContainerWorkspaceStore.h"
#include "ContainerManager.h"
#include "ContainerDocumentCodec.h"
#include "ContainerContentRegistry.h"
#include "gui/meters/MeterItem.h"
#include <memory>
#include <cmath>
#include <QUuid>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <algorithm>
Q_LOGGING_CATEGORY(lcArrange, "nereus.container.arrange")
namespace NereusSDR
{
namespace
{
QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
int containerIndex(const WorkspaceDocument& d, const QString& id)
{
    for (int i = 0; i < d.containers.size(); ++i) {
        if (d.containers[i].id == id) {
            return i;
        }
    }
    return -1;
}
std::pair<int, int> locate(const WorkspaceDocument& d, const QString& id)
{
    for (int c = 0; c < d.containers.size(); ++c) {
        for (int e = 0; e < d.containers[c].contents.size(); ++e) {
            if (d.containers[c].contents[e].id == id) {
                return {c, e};
            }
        }
    }
    return {-1, -1};
}
ArrangeResult failure(const QString& error) { return {false, error}; }
void remember(ContentEntry& entry, const ContainerDocument& source, int index)
{
    // Temporary shell hops never replace a known normal home. Unknown fields
    // remain on the complete record, even after the known anchors are refreshed.
    if (source.popOutShell) {
        return;
    }
    QJsonObject extensions =
        entry.returnLocation ? entry.returnLocation->extensions : QJsonObject();
    entry.returnLocation = ReturnLocation{
        source.id, index > 0 ? source.contents[index - 1].id : QString(),
        index + 1 < source.contents.size() ? source.contents[index + 1].id : QString(), extensions};
}
// Stable neighbor evidence forms a small precedence graph. Survivors are a hard
// chain, return links precede historical live anchors, and stale cycles relax
// with diagnostics. Gap preference keeps linked returns adjacent when feasible.
QVector<ContentEntry> mergeReturns(const QVector<ContentEntry>& survivors,
                                   const QVector<ContentEntry>& returns, const QString& destination)
{
    QVector<ContentEntry> nodes = survivors;
    nodes += returns;
    const int count = nodes.size(), live = survivors.size();
    QVector<QVector<bool>> edges(count, QVector<bool>(count, false));
    QHash<QString, int> ids;
    for (int i = 0; i < count; ++i) {
        ids[nodes[i].id] = i;
    }
    const auto reachable = [&](int a, int b) {
        QVector<bool> seen(count, false);
        QVector<int> todo{a};
        while (!todo.isEmpty()) {
            int n = todo.takeLast();
            if (n == b) {
                return true;
            }
            if (seen[n]) {
                continue;
            }
            seen[n] = true;
            for (int j = 0; j < count; ++j) {
                if (edges[n][j]) {
                    todo.append(j);
                }
            }
        }
        return false;
    };
    const auto sameSource = [&](int a, int b) {
        return nodes[a].returnLocation && nodes[b].returnLocation &&
               nodes[a].returnLocation->containerId == nodes[b].returnLocation->containerId;
    };
    for (int i = live; i < count; ++i) {
        if (!nodes[i].returnLocation) {
            continue;
        }
        const auto& r = *nodes[i].returnLocation;
        const int pre = ids.value(r.beforeId, -1), post = ids.value(r.afterId, -1);
        if (pre == i || post == i) {
            qCWarning(lcArrange) << "Self return anchor ignored for" << nodes[i].id;
        }
        if ((pre < 0 && !r.beforeId.isEmpty()) || (post < 0 && !r.afterId.isEmpty())) {
            qCWarning(lcArrange) << "Missing return anchors ignored for" << nodes[i].id;
        }
        if (pre >= live && pre != i && sameSource(pre, i)) {
            edges[pre][i] = true;
        }
        if (post >= live && post != i && sameSource(post, i)) {
            edges[i][post] = true;
        }
    }
    // Cyclic linked histories cannot be chronological evidence. Linearize each
    // strongly connected component by stable identity while retaining outsiders.
    QVector<bool> handled(count, false);
    for (int i = live; i < count; ++i) {
        if (handled[i]) {
            continue;
        }
        QVector<int> component{i};
        for (int j = i + 1; j < count; ++j) {
            if (reachable(i, j) && reachable(j, i)) {
                component.append(j);
            }
        }
        for (int n : component) {
            handled[n] = true;
        }
        if (component.size() < 2) {
            continue;
        }
        qCWarning(lcArrange) << "Cyclic return anchors recovered in" << destination;
        std::sort(component.begin(), component.end(),
                  [&](int a, int b) { return nodes[a].id < nodes[b].id; });
        for (int a : component) {
            for (int b : component) {
                edges[a][b] = false;
            }
        }
        for (int j = 1; j < component.size(); ++j) {
            edges[component[j - 1]][component[j]] = true;
        }
    }
    // New shell entries have no historical ordering source; their shell order
    // is the user's current order and is retained when appending to main.
    int previousNew = -1;
    for (int i = live; i < count; ++i) {
        if (!nodes[i].returnLocation) {
            if (previousNew >= 0) {
                edges[previousNew][i] = true;
            }
            previousNew = i;
        }
    }
    for (int i = 1; i < live; ++i) {
        edges[i - 1][i] = true;
    }
    QVector<int> stable;
    for (int i = live; i < count; ++i) {
        stable.append(i);
    }
    std::sort(stable.begin(), stable.end(),
              [&](int a, int b) { return nodes[a].id < nodes[b].id; });
    const auto add = [&](int a, int b) {
        if (a == b || reachable(b, a)) {
            qCWarning(lcArrange) << "Relaxed stale return anchor" << nodes[a].id << nodes[b].id;
            return;
        }
        edges[a][b] = true;
    };
    for (bool predecessor : {true, false}) {
        for (int i : stable) {
            if (!nodes[i].returnLocation) {
                continue;
            }
            const auto& r = *nodes[i].returnLocation;
            // Surviving anchors are meaningful only in the remembered source.
            if (r.containerId != destination) {
                continue;
            }
            const int anchor = ids.value(predecessor ? r.beforeId : r.afterId, -1);
            if (anchor >= 0 && anchor < live) {
                if (predecessor) {
                    add(anchor, i);
                } else {
                    add(i, anchor);
                }
            }
        }
    }
    QVector<int> lower(count, 0), upper(count, live), gap(count, live);
    for (int i = live; i < count; ++i) {
        for (int j = 0; j < live; ++j) {
            if (reachable(j, i)) {
                lower[i] = qMax(lower[i], j + 1);
            }
            if (reachable(i, j)) {
                upper[i] = qMin(upper[i], j);
            }
        }
    }
    QVector<bool> grouped(count, false);
    QVector<QString> key(count);
    for (int i = live; i < count; ++i) {
        if (grouped[i]) {
            continue;
        }
        QVector<int> component, todo{i};
        while (!todo.isEmpty()) {
            int n = todo.takeLast();
            if (grouped[n]) {
                continue;
            }
            grouped[n] = true;
            component.append(n);
            for (int j = live; j < count; ++j) {
                if (edges[n][j] || edges[j][n]) {
                    todo.append(j);
                }
            }
        }
        int lo = 0, hi = live;
        QString smallest = nodes[i].id;
        bool hasPre = false, hasPost = false;
        for (int n : component) {
            lo = qMax(lo, lower[n]);
            hi = qMin(hi, upper[n]);
            smallest = qMin(smallest, nodes[n].id);
            hasPre |= lower[n] > 0;
            hasPost |= upper[n] < live;
        }
        for (int n : component) {
            key[n] = smallest;
            gap[n] = lo <= hi ? (hasPost ? hi : (hasPre ? lo : live))
                              : (upper[n] < live ? upper[n] : (lower[n] > 0 ? lower[n] : live));
        }
    }
    QVector<int> indegree(count, 0);
    for (int a = 0; a < count; ++a) {
        for (int b = 0; b < count; ++b) {
            if (edges[a][b]) {
                ++indegree[b];
            }
        }
    }
    QVector<ContentEntry> result;
    QVector<bool> used(count, false);
    for (int step = 0; step < count; ++step) {
        int best = -1;
        const auto rank = [&](int n) { return n < live ? 2 * n + 1 : 2 * gap[n]; };
        for (int i = 0; i < count; ++i) {
            if (used[i] || indegree[i]) {
                continue;
            }
            if (best < 0 || rank(i) < rank(best) ||
                (rank(i) == rank(best) &&
                 (key[i] < key[best] || (key[i] == key[best] && nodes[i].id < nodes[best].id)))) {
                best = i;
            }
        }
        Q_ASSERT(best >= 0);
        if (best < 0) {
            return {};
        }
        used[best] = true;
        result.append(nodes[best]);
        for (int j = 0; j < count; ++j) {
            if (edges[best][j]) {
                --indegree[j];
            }
        }
    }
    return result;
}
} // namespace
ContainerArrangeController::ContainerArrangeController(ContainerWorkspaceStore& store,
                                                       ContainerManager* manager, QObject* parent)
    : QObject(parent), m_store(store), m_manager(manager), m_identity(newId())
{
}
ArrangeResult ContainerArrangeController::commit(const WorkspaceDocument& document)
{
    const QString error = ContainerDocumentCodec::validate(document);
    if (!error.isEmpty()) {
        return failure(error);
    }
    const CommitResult result = m_manager ? m_manager->commitWorkspace(document, document.revision)
                                          : m_store.commit(document, document.revision);
    return {result.status == CommitStatus::Saved, result.error};
}
ArrangeResult ContainerArrangeController::moveDraft(WorkspaceDocument& d, const QString& id, const QString& destination,
                                               int insertion)
{
    const auto [source, index] = locate(d, id);
    const int target = containerIndex(d, destination);
    if (source < 0 || target < 0) {
        return failure(tr("Entry or destination is unavailable"));
    }
    if (d.containers[source].locked || d.containers[target].locked) {
        return failure(tr("Arrangement is locked"));
    }
    if (insertion < 0 || insertion > d.containers[target].contents.size()) {
        return failure(tr("Invalid insertion position"));
    }
    auto entry = d.containers[source].contents[index];
    if (source != target && d.containers[target].popOutShell) {
        remember(entry, d.containers[source], index);
    }
    d.containers[source].contents.removeAt(index);
    if (source == target && insertion > index) {
        --insertion;
    }
    d.containers[target].contents.insert(insertion, entry);
    d.containers[target].visible = true;
    return {true,{}};
}
ArrangeResult ContainerArrangeController::placeFreeCanvas(const QString& entryId,const QRectF& rect,const QRectF& original)
{
    auto document=m_store.snapshot();const auto [c,e]=locate(document,entryId);
    if(c<0 || document.containers[c].layout!=ContentLayout::FreeCanvas) {return failure(tr("The free Canvas placement is no longer available."));}
    auto& container=document.containers[c];auto& entry=container.contents[e];
    if(container.locked) {return failure(tr("Arrangement is locked."));}
    if(entry.freeCanvasRect().has_value() && entry.freeCanvasRect().value()!=original) {return failure(tr("The object placement changed during this gesture."));}
    if(!std::isfinite(rect.x()) || !std::isfinite(rect.y()) || !std::isfinite(rect.width()) || !std::isfinite(rect.height()) || rect.width()<0 || rect.height()<0) {return failure(tr("Invalid object geometry."));}
    entry.setFreeCanvasRect(rect);return commit(document);
}
ArrangeResult ContainerArrangeController::move(const QString& id,const QString& destination,int insertion)
{
    auto d=m_store.snapshot();const auto result=moveDraft(d,id,destination,insertion);
    return result.ok?commit(d):result;
}
ArrangeResult ContainerArrangeController::popOut(const QString& id)
{
    auto d = m_store.snapshot();
    const auto [source, index] = locate(d, id);
    if (source < 0) {
        return failure(tr("Entry is unavailable"));
    }
    if (d.containers[source].locked) {
        return failure(tr("Arrangement is locked"));
    }
    auto entry = d.containers[source].contents[index];
    remember(entry, d.containers[source], index);
    ContainerDocument shell;
    shell.id = newId();
    shell.name = entry.name;
    shell.layout = ContentLayout::VerticalStack;
    shell.dockMode = DockMode::Floating;
    shell.popOutShell = true;
    shell.autoHeight = true;
    // Preserve effective source defaults for inherited entries without changing
    // their explicit/inherited context choice. Ordinary Move still inherits its
    // destination; a newly created shell begins with its source's defaults.
    for (const QString& axis :
         {QStringLiteral("sessionId"), QStringLiteral("sliceId"), QStringLiteral("rxSource")}) {
        if (d.containers[source].config.contains(axis)) {
            shell.config[axis] = d.containers[source].config[axis];
        }
    }
    shell.contents = {entry};
    shell.geometry = QRect(140, 140, 360, 300);
    d.containers[source].contents.removeAt(index);
    d.containers.append(shell);
    return commit(d);
}
ArrangeResult ContainerArrangeController::returnBatch(WorkspaceDocument& d, int source,
                                                      const QVector<ContentEntry>& entries)
{
    QHash<int, QVector<ContentEntry>> batches;
    const int main = containerIndex(d, d.mainContainerId);
    if (main < 0) {
        return failure(tr("Main return destination is unavailable"));
    }
    for (const auto& entry : entries) {
        int target =
            entry.returnLocation ? containerIndex(d, entry.returnLocation->containerId) : -1;
        if (target < 0 || target == source || d.containers[target].popOutShell) {
            if (entry.returnLocation) {
                qCWarning(lcArrange)
                    << "Return destination unavailable; recovering" << entry.id << "to main";
            }
            target = main;
        }
        if (d.containers[target].locked) {
            return failure(tr("Return destination is locked"));
        }
        batches[target].append(entry);
    }
    for (auto it = batches.cbegin(); it != batches.cend(); ++it) {
        auto& destination = d.containers[it.key()];
        destination.contents = mergeReturns(destination.contents, it.value(), destination.id);
        destination.visible = true;
    }
    return {true, {}};
}
ArrangeResult ContainerArrangeController::returnEntry(const QString& id)
{
    auto d = m_store.snapshot();
    const auto [source, index] = locate(d, id);
    if (source < 0) {
        return failure(tr("Entry is unavailable"));
    }
    if (d.containers[source].locked) {
        return failure(tr("Arrangement is locked"));
    }
    const auto entry = d.containers[source].contents[index];
    if (entry.returnLocation && entry.returnLocation->containerId == d.containers[source].id) {
        return {true, {}};
    }
    const int remembered =
        entry.returnLocation ? containerIndex(d, entry.returnLocation->containerId) : -1;
    if (d.containers[source].id == d.mainContainerId &&
        (remembered < 0 || d.containers[remembered].popOutShell)) {
        return {true, {}};
    }
    d.containers[source].contents.removeAt(index);
    const auto result = returnBatch(d, source, {entry});
    if (!result.ok) {
        return result;
    }
    if (d.containers[source].popOutShell && d.containers[source].contents.isEmpty()) {
        d.containers.removeAt(source);
    }
    return commit(d);
}
ArrangeResult ContainerArrangeController::closeContainer(const QString& id)
{
    auto d = m_store.snapshot();
    const int source = containerIndex(d, id);
    if (source < 0) {
        return failure(tr("Container is unavailable"));
    }
    if (d.containers[source].popOutShell && d.containers[source].locked) {
        return failure(tr("Arrangement is locked"));
    }
    if (!d.containers[source].popOutShell) {
        d.containers[source].visible = false;
        return commit(d);
    }
    const auto result = returnBatch(d, source, d.containers[source].contents);
    if (!result.ok) {
        return result;
    }
    d.containers.removeAt(source);
    return commit(d);
}
ArrangeResult ContainerArrangeController::removeContainer(const QString& id)
{
    auto d = m_store.snapshot();
    const int source = containerIndex(d, id);
    if (source < 0 || id == d.mainContainerId) {
        return failure(tr("Cannot remove the main container"));
    }
    if (d.containers[source].locked) {
        return failure(tr("Arrangement is locked"));
    }
    // Preserve opaque entries too: deletion must never discard unavailable data.
    const auto result = returnBatch(d, source, d.containers[source].contents);
    if (!result.ok) {
        return result;
    }
    d.containers.removeAt(source);
    return commit(d);
}
ArrangeResult ContainerArrangeController::duplicateEntry(const QString& id,
                                                         const QString& destination, int insertion)
{
    auto d = m_store.snapshot();
    const auto [source, index] = locate(d, id);
    const int target = containerIndex(d, destination);
    if (source < 0 || target < 0) {
        return failure(tr("Entry or destination is unavailable"));
    }
    if (d.containers[source].locked || d.containers[target].locked) {
        return failure(tr("Arrangement is locked"));
    }
    auto entry = d.containers[source].contents[index];
    if (!canDuplicate(entry)) {
        return failure(tr("Singleton and unavailable objects cannot be duplicated"));
    }
    if (insertion < 0 || insertion > d.containers[target].contents.size()) {
        return failure(tr("Invalid insertion position"));
    }
    entry.id = newId();
    entry.returnLocation.reset();
    d.containers[target].contents.insert(insertion, entry);
    return commit(d);
}
bool ContainerArrangeController::canDuplicate(const ContentEntry& entry) const
{
    ContainerContentRegistry registry;
    for (const auto& descriptor : registry.descriptors()) {
        if (descriptor.typeId == entry.typeId && descriptor.singleton) {
            return false;
        }
    }
    const std::unique_ptr<MeterItem> item(
        registry.createMeterItem(entry, nullptr, ContentRenderMode::Validation));
    return bool(item);
}
WorkspaceDocument ContainerArrangeController::workspaceSnapshot() const
{
    return m_store.snapshot();
}
QByteArray ContainerArrangeController::mimeData(const QString& id) const
{
    return QJsonDocument(QJsonObject{{"workspace", m_identity},
                                     {"revision", QString::number(m_store.snapshot().revision)},
                                     {"entry", id}})
        .toJson(QJsonDocument::Compact);
}
ArrangeResult ContainerArrangeController::validateDrop(const QByteArray& data,
                                                       const QString& destination) const
{
    const auto object = QJsonDocument::fromJson(data).object();
    const auto d = m_store.snapshot();
    if (object.value("workspace").toString() != m_identity ||
        object.value("revision").toString() != QString::number(d.revision)) {
        return failure(tr("Stale or foreign workspace drag"));
    }
    const auto [source, index] = locate(d, object.value("entry").toString());
    Q_UNUSED(index);
    const int target = containerIndex(d, destination);
    if (source < 0 || target < 0) {
        return failure(tr("Entry or destination is unavailable"));
    }
    if (d.containers[source].locked || d.containers[target].locked) {
        return failure(tr("Arrangement is locked"));
    }
    return {true, {}};
}
ArrangeResult ContainerArrangeController::drop(const QByteArray& data, const QString& destination,
                                               int insertion)
{
    const auto result = validateDrop(data, destination);
    if (!result.ok) {
        return result;
    }
    return move(QJsonDocument::fromJson(data).object().value("entry").toString(), destination,
                insertion);
}
} // namespace NereusSDR
