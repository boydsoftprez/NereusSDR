// no-port-check: NereusSDR-original opaque session-history helper.
#pragma once
#include <QByteArray>
#include <QObject>
#include <QSet>
#include <optional>
#include <vector>

namespace NereusSDR {
// Callers capture exact audio state and restore returned snapshots. Selection
// and measured samples are excluded by callers, not interpreted here.
class EqEditHistory : public QObject {
    Q_OBJECT
public:
    explicit EqEditHistory(QObject* parent = nullptr);
    // Rebase on an authoritative state (e.g. a newly loaded profile).
    void reset(const QByteArray& state);
    // Repeated begins retain the first snapshot of a gesture.
    void beginEdit(const QByteArray& before);
    // Without a begin, compare against the last committed/restored state.
    void commitEdit(const QByteArray& after);
    void cancelEdit();
    std::optional<QByteArray> undo();
    std::optional<QByteArray> redo();
    bool canUndo() const;
    bool canRedo() const;
    // Snapshot keys for caller-owned bounded sidecars (e.g. selection).
    QSet<QByteArray> retainedStates() const;
signals:
    void availabilityChanged(bool canUndo, bool canRedo);

private:
    struct Edit {
        QByteArray before;
        QByteArray after;
    };
    static constexpr std::size_t kMaximumEdits = 100;
    void emitAvailabilityChange(bool oldUndo, bool oldRedo);

    QByteArray m_current;
    std::optional<QByteArray> m_pendingBefore;
    std::vector<Edit> m_edits;
    std::size_t m_cursor{0};
};
} // namespace NereusSDR
