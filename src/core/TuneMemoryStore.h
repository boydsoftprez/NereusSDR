// =================================================================
// src/core/TuneMemoryStore.h  (NereusSDR)
// =================================================================
//
// NereusSDR-native per-(antenna, band) cache of TGXL relay positions.
// Each slot stores the three relay byte values (C1/L/C2) and a timestamp.
// Slots are persisted as individual JSON objects in AppSettings under
// TGXL_TuneMemory_Ant<N>_Band<M>.
//
// Design reference: docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-design.md §4.8
//
// AI tooling: Anthropic Claude Code.

#pragma once

#include "models/Band.h"

#include <QObject>
#include <QVector>
#include <optional>

namespace NereusSDR {

/// Relay position snapshot for a single (antenna, band) combination.
struct TuneMemory {
    int    antenna;      // 1..3  (NOTE: 1-based, not 0-based)
    Band   band;         // ham band / GEN / WWV / XVTR
    int    c1;           // relay byte 0..255
    int    l;            // relay byte 0..255
    int    c2;           // relay byte 0..255
    qint64 savedAtMs;    // epoch ms when this memory was stored
};

/// Per-(antenna, band) cache of TGXL relay positions, persisted to
/// AppSettings as one JSON object per slot.
///
/// `listAll()` returns all stored memories sorted by band (ascending enum
/// value), then by antenna (ascending). This ordering is stable across
/// platform runtimes because both keys are integral.
class TuneMemoryStore : public QObject {
    Q_OBJECT
public:
    explicit TuneMemoryStore(QObject* parent = nullptr);

    /// Retrieve the relay positions stored for (antenna, band), or
    /// std::nullopt if no memory has been saved for that slot.
    std::optional<TuneMemory> recall(int antenna, Band band) const;

    /// Persist mem to AppSettings and emit changed().
    void store(const TuneMemory& mem);

    /// Remove the AppSettings key for (antenna, band) and emit changed().
    /// No-op if the slot was not stored.
    void clear(int antenna, Band band);

    /// Remove all TGXL_TuneMemory_Ant*_Band* keys and emit changed().
    void clearAll();

    /// Return all stored memories, sorted by band then antenna.
    QVector<TuneMemory> listAll() const;

signals:
    void changed();

private:
    /// Key for a single (antenna, band) slot:
    ///   "TGXL_TuneMemory_Ant<N>_Band<M>"
    /// where M is Band::bandKeyName(band).
    QString slotKey(int antenna, Band band) const;
};

}  // namespace NereusSDR
