// SPDX-License-Identifier: GPL-2.0-or-later
//
// NereusSDR - RxDecodeModel: bounded ring buffer of recent local
// decodes from MY radio's receivers (rade_text + WSJT-X UDP).
//
// NEW NereusSDR-native model. No upstream equivalent. Distinguishes
// "what my radio just heard" from "what spots are flowing in from
// cluster/network sources" (which live in SpotModel).
//
// Modification history (NereusSDR)
//   Created 2026-05-11 by JJ Boyd / KG4VCF
//   AI tooling: Claude Code

#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QVector>

namespace NereusSDR {

// One decoded packet from a local receiver (no network sourcing).
struct RxDecode {
    QString   callsign;
    double    freqMhz{0.0};
    int       snr{0};
    QString   mode;        // e.g. "FT8", "FT4", "RADE"
    QString   source;      // "rade_text" or "WSJT-X"
    QDateTime utcTime;
    QString   payload;     // raw decoded text
};

// NEW class - no upstream equivalent.
// Bounded ring buffer of RxDecode entries. Sources:
//   - Phase R: rade_text decodes (set source="rade_text")
//   - Phase B4: WSJT-X UDP decodes (set source="WSJT-X")
//
// When the buffer reaches maxSize, the oldest entry is dropped on the
// next addDecode() call (FIFO eviction via QVector::pop_front()).
class RxDecodeModel : public QObject {
    Q_OBJECT
public:
    explicit RxDecodeModel(int maxSize = 200, QObject* parent = nullptr);

    QVector<RxDecode> decodes() const { return m_decodes; }

public slots:
    void addDecode(const RxDecode& decode);
    void clear();

signals:
    void decodeAdded(const RxDecode& decode);
    void cleared();

private:
    int               m_maxSize;
    QVector<RxDecode> m_decodes;
};

}  // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::RxDecode)
