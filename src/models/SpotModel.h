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
#include <utility>

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
    qint64 addedMs{0};         // local wall-clock when added
};

// From AetherSDR src/models/SpotModel.h:27-49 [@0cd4559]
class SpotModel : public QObject {
    Q_OBJECT
public:
    explicit SpotModel(QObject* parent = nullptr) : QObject(parent) {}

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
    // dedupIndexFor(callsign, freqMhz, windowMs) returns:
    //   - the existing index for (callsign, freqBucket) when within
    //     windowMs of the previous emit -> caller passes it to
    //     applySpotStatus, which then emits spotUpdated (not spotAdded);
    //   - a freshly-minted monotonic index otherwise.
    //
    // freqBucket is the freqMhz rounded to the nearest 1 kHz so split-
    // operating spots (e.g. DX 14.205 vs 14.206) merge to one entry.
    // Default windowMs of 60 s catches the typical "RBN sends every
    // CQ" rate without losing the user's ability to see the same
    // station spotted again hours later as a fresh entry.
    //
    // Caller is RadioModel's on*SpotReceived family of handlers.
    // SpotModel itself remains TCI-keyed: TCI clients drive their own
    // index allocation and don't go through dedup.
    int dedupIndexFor(const QString& callsign, double freqMhz,
                      qint64 windowMs = 60000);

signals:
    void spotAdded(const SpotData& spot);
    void spotUpdated(const SpotData& spot);
    void spotRemoved(int index);
    void spotsCleared();
    void spotsRefreshed();
    void spotTriggered(int index, const QString& panId);

private:
    QMap<int, SpotData> m_spots;

    // Phase 3J-1 closeout follow-up (2026-05-12): dedup state.
    // Key:   "<UPPER_CALLSIGN>|<freqBucketKHz>"
    // Value: <last spot index, last-seen ms epoch>
    QHash<QString, std::pair<int, qint64>> m_dedupCache;
    int m_nextDedupIndex{0};
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::SpotData)
