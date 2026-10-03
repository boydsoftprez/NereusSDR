#pragma once
// =================================================================
// src/core/session/MirrorPolicy.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 7.
//
// The direction table StateMirror consults before applying anything a
// remote GUI sends back. DEFAULT DENY: a property with no entry resolves
// to Outbound, so a Q_PROPERTY added by some future task is mirrored out
// to the GUI but is NOT remotely writable until someone deliberately says
// it is.
//
// The table is total over the mirrored surface rather than listing only
// the exceptions. That costs one line per property and buys the guarantee
// tst_mirror_schema enforces: a new Q_PROPERTY fails the golden-list
// guard BY NAME until it has been classified. Any task landing a
// Q_PROPERTY on a mirrored model regenerates this list.
//
// The guard asserts membership and never counts. The mirrored property
// total has already moved twice inside this plan's own execution window,
// and nine properties landed mid-declaration in four days in July 2026. A
// count assertion turns a routine feature commit into a mysterious
// failure; a membership assertion names the property that needs an entry.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 7: mirror
//                                    direction table. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
// =================================================================

#include <QByteArray>
#include <QList>

namespace NereusSDR {

enum class MirrorDirection {
    /// Daemon to GUI only. Anything a remote peer sends for this property
    /// is rejected. This is the default for anything unlisted.
    Outbound,

    /// Mirrored out, and a remote peer may write it back. The write is
    /// applied under StateMirror's re-entrancy guard so the resulting
    /// NOTIFY does not echo straight back down the link.
    Bidirectional,

    /// CONSTANT: no NOTIFY, so it can never be a delta. Travels in the
    /// connect-time snapshot only, and is never inbound.
    /// SliceModel::sliceIndex is the mirror's object identity and the
    /// reason this value exists.
    ConstantSnapshot,
};

class MirrorPolicy {
public:
    struct Entry {
        const char* className;
        const char* property;
        MirrorDirection direction;
    };

    /// Direction for one property. Returns Outbound for anything with no
    /// entry, which is the safe default for a network control plane.
    static MirrorDirection directionFor(const QByteArray& className,
                                        const QByteArray& property);

    /// May a remote peer write this property? True only for
    /// Bidirectional. Callers should prefer this over comparing the
    /// direction themselves.
    static bool inboundAllowed(const QByteArray& className,
                               const QByteArray& property);

    /// Does the table classify this property explicitly, as opposed to
    /// falling through to the Outbound default? The golden-list guard
    /// uses this to name properties nobody has classified yet.
    static bool hasExplicitEntry(const QByteArray& className,
                                 const QByteArray& property);

    /// The whole table, for the guard test.
    static const QList<Entry>& entries();

    /// A property the Core sends only to a peer whose hello declared
    /// `feature` at `minVersion` or higher; every other peer's schema,
    /// snapshot and deltas leave it out (StationServer::sendToPeer). A
    /// client that did not declare the feature must not count the missing
    /// property as schema skew (StationClient::compareSchema).
    struct FeatureGate {
        const char* className;
        const char* property;
        const char* feature;
        int minVersion;
    };
    static const QList<FeatureGate>& featureGates();
    /// The gate for one property, or nullptr when every peer gets it.
    static const FeatureGate* featureGateFor(const QByteArray& className,
                                             const QByteArray& property);
};

} // namespace NereusSDR
