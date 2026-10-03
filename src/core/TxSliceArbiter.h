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
//   2026-09-25 iPhone app plan Task 34 (R-IOS-03; remote design section
//              12.2): a handoff while keyed waits for the unkey-confirmed
//              gate (setUnkeyGate) before the flag moves, local handoffs
//              included, so the new slice's frequency never reaches the
//              radio before MOX off. J.J. Boyd (KG4VCF), AI-assisted via
//              Anthropic Claude Code.
//   2026-09-26 iPhone app plan Task 77 (R-IOS-02; the several-devices
//              design, rulings 8.10 to 8.13): an owner lookup beside the
//              slice list; requestHandoff for a requester's own slices;
//              bindForHolder; the first bind among the holder's slices;
//              the freeze while the station device is keyed. J.J. Boyd
//              (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 Slice control plan Task 2: the owner lookup becomes a
//              transmit access check (RadioModel sets it to
//              SliceAccessPolicy::mayTransmitOn), so a slice a device only
//              listens to never carries its transmit. J.J. Boyd (KG4VCF),
//              AI-assisted via Anthropic Claude Code.
//   2026-09-28 Slice control plan, fix wave for the Tasks 1-4 review
//              (Critical 1): a handoff waiting for the unkey gate is
//              checked again when the gate answers, against the holder and
//              the device that asked, and dropped when the slice is no
//              longer theirs; pendingHandoffSliceId names the waiting
//              target, and pendingHandoffChanged announces it. J.J. Boyd
//              (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 Slice control fix wave (whole-branch review, Critical 1):
//              releaseBinding unkeys through the unkey gate before the
//              binding ends, so the radio is never left keyed with no
//              transmit slice. J.J. Boyd (KG4VCF), AI-assisted via
//              Anthropic Claude Code.
// =================================================================
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

namespace NereusSDR {

class SliceModel;
class MoxController;
class UnkeyGate;

/// Enforces the single-TX invariant: exactly one slice is TX-bound at a time.
/// Performs RF-safe handoff (drops MOX before flipping). RadioModel owns one
/// instance; MoxController + AlexController + VfoWidget + persistence subscribe.
class TxSliceArbiter : public QObject {
    Q_OBJECT
    Q_PROPERTY(int txBoundSliceId READ txBoundSliceId NOTIFY txBoundSliceChanged)

public:
    explicit TxSliceArbiter(QObject* parent = nullptr);

    /// Stable SliceModel::sliceIndex() identity of the TX-bound slice.
    int txBoundSliceId() const { return m_txBoundSliceId; }

    /// The slice bound to the transmitter, or nullptr when no binding
    /// resolves (no slice list wired, empty list, index out of range).
    /// Guaranteed to be the slice carrying SliceModel::isTxSlice() whenever
    /// it is non-null: `arb.txBoundSlice() == s` and `s->isTxSlice()` are the
    /// same predicate. Design ref §6 (Public API).
    SliceModel* txBoundSlice() const;

    /// Inject the MoxController (called by RadioModel during construction wiring).
    /// Without an unkey gate, a handoff while keyed calls mox->setMox(false)
    /// and flips the txSlice flags at once (MoxController commits m_mox
    /// before setMox returns, but the hardware unkeys later, at the end of
    /// its TX-to-RX walk).
    void setMoxController(MoxController* mox);

    /// iPhone app plan Task 34 (R-IOS-03): with a gate set (RadioModel sets
    /// its own), a handoff while keyed unkeys through the gate and moves the
    /// flag only when it answers (Confirmed, or TimedOut with the emergency
    /// stop applied), so the new slice's transmit frequency never reaches
    /// the radio before MOX off. requestHandoff then returns true at once
    /// and the flag moves later; a second request while one waits replaces
    /// its target.
    void setUnkeyGate(UnkeyGate* gate) { m_unkeyGate = gate; }
    /// A handoff is waiting for the unkey gate.
    bool isHandoffPending() const { return m_pendingHandoffId >= 0; }
    /// The slice a handoff waiting for the unkey gate moves the flag to,
    /// or -1. The Core counts it as transmitting while it waits, so its
    /// control cannot pass before the flag lands (slice control fix wave,
    /// Critical 1).
    int pendingHandoffSliceId() const { return m_pendingHandoffId; }

    /// Inject the slice list owner (RadioModel) so arbiter can flip txSlice
    /// flags on SliceModel instances.
    void setSliceList(QVector<SliceModel*>* slices);

    /// iPhone app plan Task 77 (ruling 8.13), slice control plan Task 2:
    /// whether a device may carry transmit on a slice (RadioModel:
    /// SliceAccessPolicy::mayTransmitOn, whose slice it is, never a
    /// listener's) and each owner's active slice, beside the slice list
    /// (RadioModel sets both from its SliceOwnership); and who holds
    /// transmit (empty while unheld).
    using TransmitAccess = std::function<bool(const QByteArray& device, int sliceId)>;
    using ActiveLookup = std::function<int(const QByteArray& owner)>;
    using HolderLookup = std::function<QByteArray()>;
    void setTransmitAccess(TransmitAccess mayTransmit, ActiveLookup active);
    void setHolderLookup(HolderLookup holder) { m_holder = std::move(holder); }
    /// Ruling 8.11: true while the station device is keyed; the flag then
    /// never moves (the Core's session server sets it).
    using FrozenLookup = std::function<bool()>;
    void setFrozen(FrozenLookup frozen) { m_frozen = std::move(frozen); }
    bool isFrozen() const { return m_frozen && m_frozen(); }

    /// Ruling 8.10: tx.setTxSlice from `requester`, for its own slices only.
    /// Refused (false, handoffBlocked) for a slice another owner has, and
    /// while frozen.
    bool requestHandoff(int sliceId, const QByteArray& requester);
    /// Ruling 8.10: a transfer assigned `holder`: the flag moves to its
    /// chosen transmit slice (`preferredSliceId`) if it still exists and
    /// is the holder's, otherwise to the holder's active slice. Nothing
    /// when the holder owns no slice, or while frozen. True when the flag
    /// is on one of the holder's slices afterwards.
    bool bindForHolder(const QByteArray& holder, int preferredSliceId);

    /// Remote-daemon R2 Task 5: set by RadioModel at construction wiring
    /// time (RadioModel::role() == Role::Remote), next to the
    /// setMoxController / setSliceList injections above. TxSliceArbiter
    /// has no route to RadioModel::role() of its own -- it holds only a
    /// MoxController* and a QVector<SliceModel*>* -- so the role has to
    /// be handed in explicitly. When true, syncToSliceList() and
    /// requestHandoff() both no-op: which slice transmits is the
    /// daemon's decision, mirrored in by a later task, not a value a
    /// remote client should compute for itself. See design addendum
    /// docs/architecture/2026-08-03-remote-daemon-r2-r3-design-
    /// addendum.md section 4.1.
    void setRemote(bool remote) { m_remote = remote; }

    /// Set the MAC address used as the per-radio AppSettings scope key for
    /// save()/load(). When unset (empty), save()/load() are no-ops.
    void setMacAddress(const QString& mac) { m_mac = mac; }

    /// Restore the persisted stable slice ID for the current MAC. The legacy
    /// positional key is migrated once when no stable-ID key exists.
    void load();

    /// Persist the current stable ID under hardware/<mac>/TxBoundSliceId.
    /// No-op if MAC unset.
    void save();

    /// Re-establish the single-TX invariant against the current slice list.
    /// RadioModel calls this after every mutation of the list it handed to
    /// setSliceList (add and remove), and load() calls it after a restore.
    /// Idempotent, and cheap enough to call unconditionally.
    ///
    /// Three arms:
    ///   - one slice flagged: adopt its stable ID. The transmitter has not
    ///     moved, so nothing is emitted.
    ///   - none flagged: INITIAL BIND. Raises the flag on the persisted /
    ///     current stable ID when it resolves, else on slice A, and emits
    ///     txBoundSliceChanged(-1, target). This is the arm that makes a
    ///     session with no operator handoff have a transmitter at all.
    ///   - more than one flagged: defensive normalisation back to one.
    ///
    /// RF-SAFETY: the initial-bind arm drops MOX first, exactly as
    /// requestHandoff does. On the true first bind nothing can be keyed yet
    /// (there was no slice to transmit from), so it never fires there; the
    /// guard exists for the degenerate keyed-but-unbound state, which is the
    /// last state you want to raise a binding underneath.
    void syncToSliceList();

    /// Slice control plan Task 7: the last slice closed, so there is no
    /// slice to transmit on. Unlike syncToSliceList() on an empty list
    /// (which keeps a restored id for the first slice to come), this drops
    /// the binding and any waiting move, and announces the change with a
    /// new id of -1. Never keys. Keyed, it unkeys first: through the
    /// unkey gate when one is set, the -1 following once the radio is in
    /// receive (or the gate stopped transmit at once); without a gate MOX
    /// drops before the -1. A slice made on the same id, or a new binding,
    /// while the gate waits leaves the binding as it is then.
    void releaseBinding();

public slots:
    /// Request TX handoff to the slice with the stable sliceId.
    /// Returns true if handoff succeeded or was a no-op (already TX-bound).
    /// Returns false and emits handoffBlocked if requested slice doesn't exist.
    bool requestHandoff(int sliceId);

private:
    /// requestHandoff with the device that asked (empty for the Core's own
    /// window), remembered while the move waits for the unkey gate.
    bool requestHandoffFrom(int sliceId, const QByteArray& requester);
    /// Whether a waiting move to `sliceId` may still land when the gate
    /// answers: the slice is still the holder's and the asker's.
    bool pendingMayLand(int sliceId, const QByteArray& requester) const;
    /// Sets the waiting move's target; pendingHandoffChanged when it differs.
    void setPending(int sliceId, const QByteArray& requester);
    /// Moves the flag to `target` (the handoff's last step).
    void flipTo(SliceModel* target);
    SliceModel* sliceWithId(int sliceId) const;

signals:
    /// Emitted after handoff completes. oldId may be -1 on initial bind.
    void txBoundSliceChanged(int oldId, int newId);

    /// Slice control fix wave: pendingHandoffSliceId() changed (-1 when no
    /// move waits any more), so what reads it as transmitting refreshes.
    void pendingHandoffChanged(int sliceId);

    /// Emitted when a handoff request is rejected (slice doesn't exist, etc.).
    void handoffBlocked(int requestedId, QString reason);

private:
    int                       m_txBoundSliceId {-1};
    MoxController*            m_mox {nullptr};
    QVector<SliceModel*>*     m_slices {nullptr};  // non-owning pointer to RadioModel's list
    QString                   m_mac;               // per-radio AppSettings scope key
    bool                      m_remote {false};    // Remote-daemon R2 Task 5
    UnkeyGate*                m_unkeyGate {nullptr};   // Task 34
    int                       m_pendingHandoffId {-1}; // Task 34: waiting for the gate
    QByteArray                m_pendingRequester;      // who asked for the waiting move
    QByteArray                m_pendingHolder;         // who held transmit when it was asked
    TransmitAccess            m_mayTransmit;           // Task 77, slice control Task 2
    ActiveLookup              m_active;                // Task 77
    HolderLookup              m_holder;                // Task 77
    FrozenLookup              m_frozen;                // Task 77, ruling 8.11
};

} // namespace NereusSDR
