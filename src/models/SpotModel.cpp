// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - SpotModel: TCI-keyed spot sink implementation.
//
// Ported from AetherSDR src/models/SpotModel.cpp [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-17  J.J. Boyd / KG4VCF  Issue #263 fix.  dedupIndexFor
//                                    no longer uses a fixed 60 s window
//                                    (which expired long before the
//                                    typical 1800 s lifetime, leaving
//                                    duplicates on the panadapter
//                                    overlay).  Instead, reuse the
//                                    existing index for as long as the
//                                    prior spot is still alive
//                                    (addedMs + lifetimeSeconds*1000 >
//                                    nowMs).  Added expireOlderThan()
//                                    sweeper + 30 s QTimer in the ctor
//                                    so aged spots actually leave m_spots
//                                    (previously addedMs + lifetimeSeconds
//                                    were stored but never enforced).
//                                    AI tooling: Anthropic Claude Code.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task D1. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR". applySpotStatus
//                                    isNew detection (first call for a
//                                    given index sets addedMs and emits
//                                    spotAdded; subsequent calls emit
//                                    spotUpdated) preserved verbatim.
//                                    All 12 TCI keys dispatched
//                                    verbatim. 0x7F (DEL) -> space
//                                    decoding on callsign and comment
//                                    preserved verbatim. timestamp
//                                    key parsed as seconds-since-epoch
//                                    (UTC) via QDateTime::fromSecsSinceEpoch
//                                    only when the toLongLong conversion
//                                    succeeds. removeSpot, clear, and
//                                    refresh preserved verbatim. AI
//                                    tooling: Anthropic Claude Code.

#include "SpotModel.h"
#include <QDateTime>
#include <cmath>

namespace NereusSDR {

namespace {
// Issue #263: 30 s sweeper cadence.  Cheap (the spot map is bounded at a
// few hundred entries) and catches WSJT-X 120 s lifetime spots well
// before they get a chance to duplicate.
constexpr int kExpireTimerIntervalMs = 30 * 1000;
}  // namespace

SpotModel::SpotModel(QObject* parent)
    : QObject(parent)
{
    m_expireTimer.setInterval(kExpireTimerIntervalMs);
    m_expireTimer.setTimerType(Qt::CoarseTimer);
    QObject::connect(&m_expireTimer, &QTimer::timeout, this, [this]() {
        expireOlderThan(QDateTime::currentMSecsSinceEpoch());
    });
    m_expireTimer.start();
}

// From AetherSDR src/models/SpotModel.cpp:6-52 [@0cd4559]
void SpotModel::applySpotStatus(int index, const QMap<QString, QString>& kvs)
{
    bool isNew = !m_spots.contains(index);
    auto& spot = m_spots[index];
    spot.index = index;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (isNew) {
        spot.addedMs = nowMs;
    }
    // Issue #263 review follow-up (2026-05-18): refresh lastSeenMs on
    // every applySpotStatus call so the expiration sweep (and the
    // dedup-window check inside dedupIndexForAtTime) treat this as a
    // fresh observation.  Without this, a steadily-active station
    // would expire `lifetimeSeconds` after its FIRST observation
    // rather than after its LAST, causing label flicker on the
    // panadapter / Spot List row bounce.
    spot.lastSeenMs = nowMs;

    for (auto it = kvs.constBegin(); it != kvs.constEnd(); ++it) {
        const QString& key = it.key();
        const QString& val = it.value();

        if (key == "callsign")
            spot.callsign = QString(val).replace(QChar(0x7f), ' ');
        else if (key == "rx_freq")
            spot.rxFreqMhz = val.toDouble();
        else if (key == "tx_freq")
            spot.txFreqMhz = val.toDouble();
        else if (key == "mode")
            spot.mode = val;
        else if (key == "color")
            spot.color = val;
        else if (key == "background_color")
            spot.backgroundColor = val;
        else if (key == "source")
            spot.source = val;
        else if (key == "spotter_callsign")
            spot.spotterCallsign = val;
        else if (key == "comment")
            spot.comment = QString(val).replace(QChar(0x7f), ' ');
        else if (key == "timestamp") {
            bool ok;
            qint64 ts = val.toLongLong(&ok);
            if (ok)
                spot.timestamp = QDateTime::fromSecsSinceEpoch(ts, Qt::UTC);
        }
        else if (key == "lifetime_seconds")
            spot.lifetimeSeconds = val.toInt();
        else if (key == "priority")
            spot.priority = val.toInt();
    }

    if (isNew)
        emit spotAdded(spot);
    else
        emit spotUpdated(spot);
}

// From AetherSDR src/models/SpotModel.cpp:54-58 [@0cd4559]
void SpotModel::removeSpot(int index)
{
    if (m_spots.remove(index)) {
        // Issue #263: drop the dedup cache slot too so the next emit
        // of the same callsign mints a fresh index rather than
        // returning a dangling one that no longer exists in m_spots.
        for (auto it = m_dedupCache.begin(); it != m_dedupCache.end(); ) {
            if (it.value() == index) {
                it = m_dedupCache.erase(it);
            } else {
                ++it;
            }
        }
        emit spotRemoved(index);
    }
}

// From AetherSDR src/models/SpotModel.cpp:60-64 [@0cd4559]
void SpotModel::clear()
{
    m_spots.clear();
    m_dedupCache.clear();  // Issue #263: cache must shed with the map.
    emit spotsCleared();
}

// From AetherSDR src/models/SpotModel.cpp:66-69 [@0cd4559]
void SpotModel::refresh()
{
    emit spotsRefreshed();
}

// Phase 3J-1 closeout follow-up (2026-05-12) + issue #263 (2026-05-17):
// cross-source spot dedup.  See SpotModel.h for design + rationale.
int SpotModel::dedupIndexFor(const QString& callsign, double freqMhz)
{
    return dedupIndexForAtTime(callsign, freqMhz,
                               QDateTime::currentMSecsSinceEpoch());
}

int SpotModel::dedupIndexForAtTime(const QString& callsign,
                                   double freqMhz,
                                   qint64 nowMs)
{
    // Bucket by 1 kHz so 14.205 / 14.2055 / 14.206 collapse to one spot.
    const long bucketKHz =
        static_cast<long>(std::llround(freqMhz * 1000.0));
    const QString key = callsign.toUpper()
                         + QLatin1Char('|')
                         + QString::number(bucketKHz);

    auto it = m_dedupCache.find(key);
    if (it != m_dedupCache.end()) {
        const int existingIdx = it.value();
        auto sit = m_spots.find(existingIdx);
        if (sit != m_spots.end()) {
            // Reuse the index for as long as the prior spot is still
            // alive.  lifetimeSeconds * 1000 may overflow qint64 for
            // pathologically huge values, but the field is clamped by
            // applySpotStatus's toInt() to INT_MAX = ~68 years, so the
            // multiplication is safe in any realistic case.
            //
            // Issue #263 review follow-up (2026-05-18): compare against
            // lastSeenMs (refreshed on every observation) rather than
            // addedMs (set once on first insert) so a steadily-active
            // station stays alive indefinitely.  Refresh lastSeenMs
            // here too — applySpotStatus will set it again on the next
            // line, but doing it here makes dedup-only call sites
            // (TCI clients, future callers) symmetric.
            const qint64 ttlMs =
                static_cast<qint64>(sit.value().lifetimeSeconds) * 1000;
            if (nowMs - sit.value().lastSeenMs < ttlMs) {
                sit.value().lastSeenMs = nowMs;
                return existingIdx;
            }
            // Prior spot has expired but the sweeper hasn't run yet.
            // Evict here so the panadapter overlay loses the stale
            // label before the re-emit lands.
            m_spots.erase(sit);
            emit spotRemoved(existingIdx);
        }
        // Either the spot is gone or it just expired.  Mint and rebind.
    }

    const int newIdx = ++m_nextDedupIndex;
    m_dedupCache.insert(key, newIdx);
    return newIdx;
}

void SpotModel::expireOlderThan(qint64 nowMs)
{
    QVector<int> expired;
    for (auto it = m_spots.constBegin(); it != m_spots.constEnd(); ++it) {
        const auto& s = it.value();
        if (s.lifetimeSeconds <= 0) { continue; }
        const qint64 ttlMs = static_cast<qint64>(s.lifetimeSeconds) * 1000;
        // Issue #263 review follow-up (2026-05-18): expire based on
        // lastSeenMs, not addedMs.  See dedupIndexForAtTime for rationale.
        if (nowMs - s.lastSeenMs >= ttlMs) {
            expired.append(it.key());
        }
    }
    if (expired.isEmpty()) { return; }

    // Build the cache keys to drop in lockstep with the spot evictions
    // so an immediate re-emit of the same callsign mints a fresh index
    // rather than returning a dangling one.
    QHash<int, QString> idxToKey;
    for (auto it = m_dedupCache.constBegin(); it != m_dedupCache.constEnd(); ++it) {
        idxToKey.insert(it.value(), it.key());
    }
    for (int idx : expired) {
        m_spots.remove(idx);
        auto kit = idxToKey.constFind(idx);
        if (kit != idxToKey.constEnd()) {
            m_dedupCache.remove(kit.value());
        }
        emit spotRemoved(idx);
    }
}

} // namespace NereusSDR
