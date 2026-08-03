#pragma once
// =================================================================
// src/core/spectrum/FftTopology.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Extracted from
// MainWindow::rebuildFftRouting (MainWindow.cpp), which resolved the pan
// each live slice actually feeds by walking PanadapterStack (a QWidget)
// and then mutated FFTRouter directly from that walk -- core routing
// state living inside a QWidget, so a headless daemon had no way to
// populate FFTRouter at all.
//
// FftTopology holds the resolved consumer-to-stream subscription set as
// plain, widget-free state and knows how to rebuild an FFTRouter from it.
// The walk that resolves which pan a slice belongs to stays in MainWindow
// (it needs PanadapterStack and SliceModel, neither of which core may
// depend on -- see tests/tst_core_has_no_gui_includes.cpp); only the
// algebra of turning a subscription set into router calls moves here.
//
// A consumer may hold several streams at once, not just one. Fix round 1
// (coordinator spec review, finding 1) widened this from an earlier,
// mistaken one-stream-per-consumer design: the review found
// tests/tst_fft_router_topology.cpp:56-69 already shipping and passing --
// it maps one pan to two receivers and asserts receiversForPan().size()
// == 2 -- plus FFTRouter::receiversForPan returning QList<int>
// deliberately, not incidentally, and TciClientSession already modelling
// one connection carrying several simultaneous stream subscriptions (the
// daemon would hit the same ceiling on day one of Task 9 otherwise).
// subscribe() therefore adds a stream rather than replacing one, and is
// idempotent for a repeated (consumer, stream) pair.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02  J.J. Boyd / KG4VCF  Remote daemon R1, extraction 5 of 9.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-02  J.J. Boyd / KG4VCF  Fix round 1: widened to many streams
//                                    per consumer (coordinator spec
//                                    review finding 1) and added the
//                                    per-stream unsubscribe overload.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include <QList>
#include <QMap>
#include <QString>

namespace NereusSDR {

class FFTRouter;

/// One consumer of a stream's FFT frames. In the GUI this is a pan; in the
/// daemon it is a remote endpoint. Design section 9.4: the daemon keys on
/// endpoint ids, not pan ids.
struct SpectrumSubscription {
    QString consumerId;
    int     streamIndex {0};
};

/// Holds the consumer -> streams subscription set (a consumer may hold
/// several streams at once) and rebuilds an FFTRouter from it on demand.
class FftTopology {
public:
    /// Adds streamIndex to consumerId's stream set. Idempotent: subscribing
    /// the same (consumerId, streamIndex) pair again does not duplicate it.
    /// A consumer already holding a different stream keeps it -- this adds
    /// to the set, it does not replace what was there.
    void subscribe(const QString& consumerId, int streamIndex);

    /// Drops every stream consumerId holds.
    void unsubscribe(const QString& consumerId);

    /// Drops just streamIndex from consumerId's stream set, leaving any
    /// other streams it holds untouched. A no-op if consumerId does not
    /// currently hold streamIndex.
    void unsubscribe(const QString& consumerId, int streamIndex);

    /// Full rebuild of router from this topology's current subscription
    /// set, not an incremental patch. FFTRouter exposes no "every consumer
    /// it currently knows" query (only per-receiver and per-consumer
    /// lookups), so this class remembers, itself, which consumers it last
    /// pushed and removes exactly that set before remapping the current
    /// one. FFTRouter::removePan clears every stream a consumer was mapped
    /// to in one call, so remembering consumer ids alone (not
    /// (consumer, stream) pairs) is enough even though a consumer may hold
    /// several streams. A consumer dropped since the previous call is in
    /// that remembered set but not in the current one, so it is removed
    /// here and never remapped -- gone from the router, not left behind as
    /// a stale mapping. Calling this twice with no subscribe() /
    /// unsubscribe() in between removes then re-adds the same set both
    /// times, leaving the router in the same state either way.
    void applyTo(FFTRouter& router) const;

    /// One SpectrumSubscription per (consumer, stream) pair currently held.
    QList<SpectrumSubscription> subscriptions() const;

private:
    QMap<QString, QList<int>> m_streamsByConsumer;

    // Bookkeeping only -- this topology's logical subscription state is
    // m_streamsByConsumer alone. Records which consumers applyTo() last
    // pushed to a router, so the next call knows which ones to remove even
    // for a consumer this topology no longer mentions at all. Just the
    // consumer ids, not their streams: FFTRouter::removePan(consumerId)
    // clears every stream that consumer held in one call, so the streams
    // themselves need not be remembered here. Mutable because applyTo() is
    // const: pushing this state to a router is not a change to this
    // object's own subscription set.
    mutable QList<QString> m_lastAppliedConsumers;
};

} // namespace NereusSDR
