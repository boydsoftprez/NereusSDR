// =================================================================
// src/gui/meters/LedResultRegistry.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See LedResultRegistry.h.
//
// Modification history (NereusSDR):
//   2026-10-26 - Created for the text overlay's LED gates. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "LedResultRegistry.h"

#include <QMutexLocker>

namespace NereusSDR {

LedResultRegistry& LedResultRegistry::instance()
{
    static LedResultRegistry registry;
    return registry;
}

void LedResultRegistry::set(const QString& id, bool value)
{
    set(id, value, nullptr);
}

void LedResultRegistry::set(const QString& id, bool value, const void* owner)
{
    if (id.isEmpty()) {
        return;
    }
    const QMutexLocker locker(&m_mutex);
    m_results.insert(id, Entry{value, owner});
}

std::optional<bool> LedResultRegistry::get(const QString& id) const
{
    const QMutexLocker locker(&m_mutex);
    const auto it = m_results.constFind(id);
    if (it == m_results.cend()) {
        return std::nullopt;
    }
    return it->value;
}

void LedResultRegistry::remove(const QString& id, const void* owner)
{
    if (id.isEmpty()) {
        return;
    }
    const QMutexLocker locker(&m_mutex);
    const auto it = m_results.find(id);
    if (it != m_results.end() && it->owner == owner) {
        m_results.erase(it);
    }
}

void LedResultRegistry::clear()
{
    const QMutexLocker locker(&m_mutex);
    m_results.clear();
}

} // namespace NereusSDR
