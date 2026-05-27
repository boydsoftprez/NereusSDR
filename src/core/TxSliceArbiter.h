// =================================================================
// src/core/TxSliceArbiter.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Enforces the single-TX-bound-slice
// invariant for Phase 3F multi-slice. Design ref:
// docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §6.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-26 Created in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//              with AI-assisted transformation via Anthropic Claude Code.
// =================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace NereusSDR {

class SliceModel;
class MoxController;

/// Enforces the single-TX invariant: exactly one slice is TX-bound at a time.
/// Performs RF-safe handoff (drops MOX before flipping). RadioModel owns one
/// instance; MoxController + AlexController + VfoWidget + persistence subscribe.
class TxSliceArbiter : public QObject {
    Q_OBJECT
    Q_PROPERTY(int txBoundSliceIndex READ txBoundSliceIndex NOTIFY txBoundSliceChanged)

public:
    explicit TxSliceArbiter(QObject* parent = nullptr);

    int txBoundSliceIndex() const { return m_txBoundIndex; }

    /// Inject the MoxController (called by RadioModel during construction wiring).
    /// Arbiter calls mox->setMox(false) and waits for moxChanged confirmation
    /// before flipping txSlice flags.
    void setMoxController(MoxController* mox);

    /// Inject the slice list owner (RadioModel) so arbiter can flip txSlice
    /// flags on SliceModel instances.
    void setSliceList(QVector<SliceModel*>* slices);

    /// Set the MAC address used as the per-radio AppSettings scope key for
    /// save()/load(). When unset (empty), save()/load() are no-ops.
    void setMacAddress(const QString& mac) { m_mac = mac; }

    /// Restore the persisted txBoundSliceIndex for the current MAC, performing
    /// a real handoff if the restored value differs from the current bound
    /// index. No-op if MAC unset or restored value is out of range.
    void load();

    /// Persist the current txBoundSliceIndex under hardware/<mac>/TxBoundSliceIndex.
    /// No-op if MAC unset.
    void save();

public slots:
    /// Request TX handoff to the slice at newSliceIndex. RF-safe (drops MOX first).
    /// Returns true if handoff succeeded or was a no-op (already TX-bound).
    /// Returns false and emits handoffBlocked if requested slice doesn't exist.
    bool requestHandoff(int newSliceIndex);

signals:
    /// Emitted after handoff completes. oldIndex may be -1 on initial bind.
    void txBoundSliceChanged(int oldIndex, int newIndex);

    /// Emitted when a handoff request is rejected (slice doesn't exist, etc.).
    void handoffBlocked(int requestedIndex, QString reason);

private:
    int                       m_txBoundIndex {0};
    MoxController*            m_mox {nullptr};
    QVector<SliceModel*>*     m_slices {nullptr};  // non-owning pointer to RadioModel's list
    QString                   m_mac;               // per-radio AppSettings scope key
};

} // namespace NereusSDR
