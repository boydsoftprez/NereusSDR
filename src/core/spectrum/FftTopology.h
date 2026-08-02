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
// One active stream per consumer, not a set: subscribe() replaces any
// previous stream for that consumerId rather than adding to it. That
// matches the relationship this models -- a pan, or in the daemon a
// remote endpoint, watches exactly one spectrum stream at a time. The
// reverse fan-out, one stream reaching many consumers at different zoom
// levels, is FFTRouter's own job and is unrestricted here: many
// SpectrumSubscription entries are free to name the same streamIndex.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02  J.J. Boyd / KG4VCF  Remote daemon R1, extraction 5 of 9.
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

/// Holds the consumer -> stream subscription set (a plain QMap<QString,
/// int>: one active stream per consumer) and rebuilds an FFTRouter from it
/// on demand.
class FftTopology {
public:
    void subscribe(const QString& consumerId, int streamIndex);
    void unsubscribe(const QString& consumerId);

    /// Full rebuild of router from this topology's current subscription
    /// set, not an incremental patch. FFTRouter exposes no "every consumer
    /// it currently knows" query (only per-receiver and per-consumer
    /// lookups), so this class remembers, itself, the subscription set it
    /// last pushed and removes exactly that set before remapping the
    /// current one. A consumer dropped by unsubscribe() since the previous
    /// call is in that remembered set but not in the current one, so it is
    /// removed here and never remapped -- gone from the router, not left
    /// behind as a stale mapping. Calling this twice with no subscribe() /
    /// unsubscribe() in between removes then re-adds the same set both
    /// times, leaving the router in the same state either way.
    void applyTo(FFTRouter& router) const;

    QList<SpectrumSubscription> subscriptions() const;

private:
    QMap<QString, int> m_streamByConsumer;

    // Bookkeeping only -- this topology's logical subscription state is
    // m_streamByConsumer alone. Records the consumer set applyTo() last
    // pushed to a router, so the next call knows what to remove even for
    // a consumer this topology no longer mentions at all. Mutable because
    // applyTo() is const: pushing that state to a router is not a change
    // to this object's own subscription set.
    mutable QMap<QString, int> m_lastApplied;
};

} // namespace NereusSDR
