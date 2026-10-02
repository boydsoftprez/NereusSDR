// no-port-check: NereusSDR-original opaque session-history helper.
#include "EqEditHistory.h"
namespace NereusSDR {
EqEditHistory::EqEditHistory(QObject* parent) : QObject(parent) {}

void EqEditHistory::reset(const QByteArray& state) {
    const bool oldUndo = canUndo();
    const bool oldRedo = canRedo();
    m_current = state;
    m_pendingBefore.reset();
    m_edits.clear();
    m_cursor = 0;
    emitAvailabilityChange(oldUndo, oldRedo);
}

void EqEditHistory::beginEdit(const QByteArray& before) {
    if (!m_pendingBefore) {
        m_pendingBefore = before;
    }
}

void EqEditHistory::commitEdit(const QByteArray& after) {
    const bool oldUndo = canUndo();
    const bool oldRedo = canRedo();
    const QByteArray before = m_pendingBefore.value_or(m_current);
    m_pendingBefore.reset();
    m_current = after;
    if (before == after) {
        return;
    }

    // A completed edit replaces only the redo branch. QByteArray's value
    // semantics retain exact bytes even if the caller later mutates its copy.
    m_edits.resize(m_cursor);
    m_edits.push_back({before, after});
    if (m_edits.size() > kMaximumEdits) {
        m_edits.erase(m_edits.begin());
    }
    m_cursor = m_edits.size();
    emitAvailabilityChange(oldUndo, oldRedo);
}

void EqEditHistory::cancelEdit() {
    m_pendingBefore.reset();
}

std::optional<QByteArray> EqEditHistory::undo() {
    if (!canUndo()) {
        return std::nullopt;
    }
    const bool oldUndo = canUndo();
    const bool oldRedo = canRedo();
    cancelEdit();
    m_current = m_edits[--m_cursor].before;
    emitAvailabilityChange(oldUndo, oldRedo);
    return m_current;
}

std::optional<QByteArray> EqEditHistory::redo() {
    if (!canRedo()) {
        return std::nullopt;
    }
    const bool oldUndo = canUndo();
    const bool oldRedo = canRedo();
    cancelEdit();
    m_current = m_edits[m_cursor++].after;
    emitAvailabilityChange(oldUndo, oldRedo);
    return m_current;
}

bool EqEditHistory::canUndo() const {
    return m_cursor > 0;
}

bool EqEditHistory::canRedo() const {
    return m_cursor < m_edits.size();
}

void EqEditHistory::emitAvailabilityChange(bool oldUndo, bool oldRedo) {
    if (oldUndo != canUndo() || oldRedo != canRedo()) {
        emit availabilityChanged(canUndo(), canRedo());
    }
}
} // namespace NereusSDR
