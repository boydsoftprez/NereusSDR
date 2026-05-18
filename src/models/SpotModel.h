// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - SpotModel: TCI-keyed spot sink. QMap<int, SpotData> keyed
// by monotonic spot index. The TCI-keyed applySpotStatus() update API
// recognises 12 keys (callsign, rx_freq, tx_freq, mode, color,
// background_color, source, spotter_callsign, comment, timestamp,
// lifetime_seconds, priority) and decodes the TCI 0x7F (DEL)
// wire-format quirk to a single ASCII space in callsign and comment.
//
// Ported from AetherSDR src/models/SpotModel.h [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task D1. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR". SpotData
//                                    struct (14 fields: index,
//                                    callsign, rxFreqMhz, txFreqMhz,
//                                    mode, color, backgroundColor,
//                                    source, spotterCallsign, comment,
//                                    timestamp, lifetimeSeconds,
//                                    priority, addedMs) preserved
//                                    verbatim from upstream. Public
//                                    surface (applySpotStatus,
//                                    removeSpot, clear, refresh,
//                                    spots) and the six signals
//                                    (spotAdded, spotUpdated,
//                                    spotRemoved, spotsCleared,
//                                    spotsRefreshed, spotTriggered)
//                                    preserved verbatim. The
//                                    TCI-keyed contract is the seam
//                                    the 3J-1 TCI worktree's
//                                    TciServer will hook into when it
//                                    lands. AI tooling: Anthropic
//                                    Claude Code.

#pragma once

#include <QObject>
#include <QMap>
#include <QHash>
#include <QString>
#include <QDateTime>
#include <QTimer>

namespace NereusSDR {

// From AetherSDR src/models/SpotModel.h:10-25 [@0cd4559]
struct SpotData {
    int index{-1};
    QString callsign;
    double rxFreqMhz{0.0};
    double txFreqMhz{0.0};
    QString mode;
    QString color;          // #AARRGGBB
    QString backgroundColor;
    QString source;
    QString spotterCallsign;
    QString comment;
    QDateTime timestamp;
    int lifetimeSeconds{1800};  // default 30 min
    int priority{0};
    qint64 addedMs{0};         // local wall-clock when first added (immutable)
    // Issue #263 review follow-up (2026-05-18): local wall-clock of the
    // most recent observation.  Refreshed by applySpotStatus on every
    // update AND by dedupIndexForAtTime when it reuses an existing
    // index, so a station re-emitted regularly inside its lifetime
    // does NOT expire mid-stream (which would flicker the label /
    // bounce the Spot List row).  Expiration checks against
    // lastSeenMs, not addedMs.
    qint64 lastSeenMs{0};
};

// From AetherSDR src/models/SpotModel.h:27-49 [@0cd4559]
class SpotModel : public QObject {
    Q_OBJECT
public:
    explicit SpotModel(QObject* parent = nullptr);
    ~SpotModel() override = default;

    const QMap<int, SpotData>& spots() const { return m_spots; }

    void applySpotStatus(int index, const QMap<QString, QString>& kvs);
    void removeSpot(int index);
    void clear();
    void refresh();

    // Phase 3J-1 closeout follow-up (2026-05-12): cross-source spot dedup.
    //
    // The 3J-2 port wired every per-source client (DxCluster / RBN /
    // WSJT-X / SpotCollector / POTA / FreeDV Reporter / PSK Reporter)
    // to bump `RadioModel::m_nextSpotIndex` for every incoming spot, so
    // SpotModel never saw a duplicate index and `spotAdded` fired on
    // EVERY spot, EVERY re-emit, from EVERY source.  Result: same
    // callsign appears 5-10 times on the spot list within seconds.
    //
    // Issue #263 (2026-05-17): the original 60 s fixed window expired
    // long before the typical 1800 s spot lifetime, so a re-emit of
    // CT3MD on the same kHz at +90 s minted a fresh index, and (because
    // there was no expiration sweeper either) the prior entry stayed in
    // m_spots.  Result: the panadapter overlay stacked N copies of the
    // same callsign.  Fix: dedupIndexFor now reuses the existing index
    // for as long as the previous spot is still alive (addedMs +
    // lifetimeSeconds * 1000 > nowMs).  expireOlderThan() (driven by an
    // internal QTimer) sweeps aged spots so the index can be recycled.
    //
    // dedupIndexFor(callsign, freqMhz) returns:
    //   - the existing index for (callsign, freqBucket) when the prior
    //     spot's lifetime hasn't elapsed -> caller passes it to
    //     applySpotStatus, which then emits spotUpdated (not spotAdded);
    //   - a freshly-minted monotonic index otherwise (and evicts the
    //     stale entry so the panadapter overlay loses it).
    //
    // freqBucket is the freqMhz rounded to the nearest 1 kHz so split-
    // operating spots (e.g. DX 14.205 vs 14.206) merge to one entry.
    //
    // Caller is RadioModel's on*SpotReceived family of handlers.
    // SpotModel itself remains TCI-keyed: TCI clients drive their own
    // index allocation and don't go through dedup.
    int dedupIndexFor(const QString& callsign, double freqMhz);

    // Clock-injectable test seam for dedup.  Production calls
    // dedupIndexFor() which delegates to this with the wall clock.
    int dedupIndexForAtTime(const QString& callsign, double freqMhz,
                            qint64 nowMs);

    // Issue #263: periodic sweeper.  Removes every spot whose
    // addedMs + lifetimeSeconds * 1000 < nowMs and emits spotRemoved
    // for each.  Production wires this to a 30 s QTimer in the ctor;
    // the parameter overload is the test seam.
    void expireOlderThan(qint64 nowMs);

signals:
    void spotAdded(const SpotData& spot);
    void spotUpdated(const SpotData& spot);
    void spotRemoved(int index);
    void spotsCleared();
    void spotsRefreshed();
    void spotTriggered(int index, const QString& panId);

private:
    QMap<int, SpotData> m_spots;

    // Phase 3J-1 closeout follow-up (2026-05-12) + issue #263 (2026-05-17).
    // Maps "<UPPER_CALLSIGN>|<freqBucketKHz>" to the live SpotData index.
    // An entry is valid only as long as m_spots still contains that
    // index; expireOlderThan() and removeSpot() prune both maps together.
    QHash<QString, int> m_dedupCache;
    int m_nextDedupIndex{0};
    QTimer m_expireTimer;
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::SpotData)
