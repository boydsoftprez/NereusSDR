// no-port-check: NereusSDR-original stable-identity draft/rebase implementation.
// Modification history (NereusSDR):
//   2026-10-02 — Draft transactions by J.J. Boyd (KG4VCF), OpenAI Codex assisted.
#include "ContainerEditSession.h"
#include <QHash>
#include <QSet>
#include <algorithm>
namespace NereusSDR {
namespace {
const ContainerDocument* find(const WorkspaceDocument& doc, const QString& id)
{
    for (const auto& c : doc.containers) { if (c.id == id) { return &c; } }
    return nullptr;
}
bool equal(const ContainerDocument* a, const ContainerDocument* b)
{ return (!a && !b) || (a && b && *a == *b); }
QSet<QString> ids(const WorkspaceDocument& a, const WorkspaceDocument& b)
{
    QSet<QString> result;
    for (const auto& c : a.containers) { result.insert(c.id); }
    for (const auto& c : b.containers) { result.insert(c.id); }
    return result;
}
struct Placement { QString owner; ContentEntry entry; bool operator==(const Placement&) const = default; };
QHash<QString, Placement> placements(const WorkspaceDocument& doc)
{
    QHash<QString, Placement> result;
    for (const auto& c : doc.containers) { for (const auto& e : c.contents) { result[e.id] = {c.id,e}; } }
    return result;
}
void replace(WorkspaceDocument& doc, const QString& id, const ContainerDocument* current)
{
    for (int i=0; i<doc.containers.size(); ++i) {
        if (doc.containers[i].id != id) { continue; }
        if (current) { doc.containers[i] = *current; } else { doc.containers.removeAt(i); }
        return;
    }
    if (current) { doc.containers.append(*current); }
}
}
ContainerEditSession::ContainerEditSession(ContainerWorkspaceStore& store) : m_store(store)
{
    m_store.load(); m_base = m_store.snapshot(); m_draft = m_base;
}
QStringList ContainerEditSession::conflicts(const WorkspaceDocument& live) const
{
    QSet<QString> result;
    for (const auto& id : ids(m_base,m_draft)) {
        const auto* base=find(m_base,id); const auto* draft=find(m_draft,id);
        if (equal(base,draft)) { continue; }
        if (!equal(base,find(live,id))) { result.insert(id); }
        if (draft) {
            for (const auto& e : draft->contents) {
                if (e.returnLocation && !e.returnLocation->containerId.isEmpty()) {
                    const auto& ref=e.returnLocation->containerId;
                    if (!equal(find(m_base,ref),find(live,ref))) { result.insert(ref); result.insert(id); }
                }
            }
        }
    }
    const auto b=placements(m_base), d=placements(m_draft), l=placements(live);
    QSet<QString> entries;
    for (auto it=b.cbegin();it!=b.cend();++it) { entries.insert(it.key()); }
    for (auto it=d.cbegin();it!=d.cend();++it) { entries.insert(it.key()); }
    for (const auto& id : entries) {
        if (b.contains(id)==d.contains(id) && b.value(id)==d.value(id)) { continue; }
        if (b.contains(id)==l.contains(id) && b.value(id)==l.value(id)) { continue; }
        for (const auto& owner : {b.value(id).owner,d.value(id).owner,l.value(id).owner}) {
            if (!owner.isEmpty()) { result.insert(owner); }
        }
    }
    if ((m_draft.mainContainerId!=m_base.mainContainerId && live.mainContainerId!=m_base.mainContainerId)
        || (m_draft.extensions!=m_base.extensions && live.extensions!=m_base.extensions)) {
        result.insert(live.mainContainerId); result.insert(m_draft.mainContainerId);
    }
    QStringList ordered=result.values(); ordered.sort(); return ordered;
}
QStringList ContainerEditSession::conflictingContainers() const
{
    if (!m_store.load().ok) { return ids(m_base,m_draft).values(); }
    const auto current=conflicts(m_store.snapshot());
    return current.isEmpty() ? m_casConflicts : current;
}
CommitResult ContainerEditSession::apply()
{
    const auto loaded=m_store.load();
    if (!loaded.ok) { return {CommitStatus::Invalid,m_store.snapshot().revision,loaded.error}; }
    const auto live=m_store.snapshot(); const auto overlapping=conflicts(live);
    if (!overlapping.isEmpty()) {
        return {CommitStatus::Conflict,live.revision,QStringLiteral("Conflicting containers: %1").arg(overlapping.join(", "))};
    }
    WorkspaceDocument merged=live;
    for (const auto& id : ids(m_base,m_draft)) {
        if (!equal(find(m_base,id),find(m_draft,id))) { replace(merged,id,find(m_draft,id)); }
    }
    if (m_draft.mainContainerId!=m_base.mainContainerId) { merged.mainContainerId=m_draft.mainContainerId; }
    if (m_draft.extensions!=m_base.extensions) { merged.extensions=m_draft.extensions; }
    const auto result=m_store.commit(merged,live.revision);
    if (result.status==CommitStatus::Saved) { m_base=m_store.snapshot(); m_draft=m_base; m_casConflicts.clear(); }
    else if(result.status==CommitStatus::Conflict) {
        // The store is the final raw/revision arbiter. A race after our load
        // still names the drafts that need a fresh merge/retry.
        m_store.load(); m_casConflicts=conflicts(m_store.snapshot());
        if(m_casConflicts.isEmpty()) {
            for(const auto& id:ids(m_base,m_draft)) { if(!equal(find(m_base,id),find(m_draft,id))) { m_casConflicts.append(id); } }
            if(m_casConflicts.isEmpty()) { m_casConflicts.append(m_draft.mainContainerId); }
            m_casConflicts.sort();
        }
        return {CommitStatus::Conflict,m_store.snapshot().revision,QStringLiteral("Workspace changed during Apply; reload or retry containers: %1").arg(m_casConflicts.join(", "))};
    }
    return result;
}
void ContainerEditSession::cancel() { m_draft=m_base; m_casConflicts.clear(); }
void ContainerEditSession::reloadContainers(const QStringList& selected)
{
    if (!m_store.load().ok) { return; }
    const auto live=m_store.snapshot(); QSet<QString> linked;
    const bool reloadMetadata=selected.contains(m_base.mainContainerId) || selected.contains(live.mainContainerId);
    for (const auto& id : selected) {
        for (const auto* doc : {find(m_base,id),find(m_draft,id),find(live,id)}) {
            if (doc) { for (const auto& e : doc->contents) { linked.insert(e.id); } }
        }
        replace(m_base,id,find(live,id)); replace(m_draft,id,find(live,id));
    }
    // A selected container's adopted identity cannot remain in another draft
    // owner. Synchronize only these linked entries, retaining all other edits.
    for (auto* doc : {&m_base,&m_draft}) {
        for (auto& c : doc->containers) {
            if (selected.contains(c.id)) { continue; }
            c.contents.removeIf([&](const ContentEntry& e){ return linked.contains(e.id); });
            const auto* current=find(live,c.id);
            if (!current) { continue; }
            for (int i=0;i<current->contents.size();++i) {
                const auto& e=current->contents[i]; if (!linked.contains(e.id)) { continue; }
                int position=c.contents.size();
                for (int j=i+1;j<current->contents.size();++j) {
                    for (int k=0;k<c.contents.size();++k) {
                        if (c.contents[k].id==current->contents[j].id) { position=k; break; }
                    }
                    if (position<c.contents.size()) { break; }
                }
                c.contents.insert(position,e);
            }
        }
    }
    if (reloadMetadata) {
        m_base.mainContainerId=live.mainContainerId; m_draft.mainContainerId=live.mainContainerId;
    }
    // Workspace metadata conflicts are reported against the main container;
    // selecting that owner must resolve its metadata as well as its rows.
    if(reloadMetadata) {
        if(m_draft.extensions==m_base.extensions || live.extensions!=m_base.extensions) { m_draft.extensions=live.extensions; }
        m_base.extensions=live.extensions;
    }
    m_casConflicts.clear();
    m_base.revision=live.revision; m_draft.revision=live.revision;
}
}
