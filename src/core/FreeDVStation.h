// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - FreeDVStation: per-sid live state from qso.freedv.org
//
// Ported from freedv-gui src/gui/dialogs/freedv_reporter.h:367-417
// (the wxWidgets `ReporterData` inner struct) [@77e793a], restructured
// as a plain struct with Qt6 value types (QString / QDateTime / QColor)
// for use across FreeDVReporterClient and FreeDVStationModel (the
// QAbstractTableModel landing in Phase 3J-2 Task D3).
//
// License (upstream): freedv-gui carries an LGPLv2.1+ root license
// (`freedv-gui/COPYING`); the specific `freedv_reporter.h` file has no
// per-file Copyright header, so the project root header applies. LGPL
// is upgrade-compatible to GPL-3 when linked into a GPLv3 work (LGPL
// §3 conversion clause), which is the model NereusSDR uses.
//
// Modification history (NereusSDR):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B5. Created the
//                                    NereusSDR per-sid station struct
//                                    by porting the field list of
//                                    freedv-gui's wx-flavoured
//                                    `ReporterData` (freedv_reporter.h
//                                    :367-417). Translation:
//                                    `wxString` -> `QString`,
//                                    `wxDateTime` -> `QDateTime`,
//                                    `wxColour` -> `QColor`, the
//                                    upstream `isVisible` / `isPending
//                                    Delete` / `isPendingUpdate` / view
//                                    bookkeeping fields are dropped
//                                    because they belong to the
//                                    Qt-model layer (FreeDVStationModel
//                                    in Task D3), not the per-station
//                                    value type. Added Q_DECLARE_METATYPE
//                                    so the struct can flow through
//                                    QSignalSpy in the B5 unit test.
//                                    AI tooling: Anthropic Claude Code.

#pragma once

#include <QColor>
#include <QDateTime>
#include <QMetaType>
#include <QString>

namespace NereusSDR {

// From freedv-gui src/gui/dialogs/freedv_reporter.h:367-417 [@77e793a]
//
// Per-station live state keyed by Socket.IO session ID (`sid`).
// FreeDVReporterClient owns the QHash<QString, FreeDVStation> map;
// FreeDVStationModel (Task D3) presents this as a sortable table.
struct FreeDVStation {
    QString sid;
    QString callsign;
    QString gridSquare;
    QString version;
    quint64 frequencyHz{0};
    QString txMode;
    QString status;            // "Active" or "RX Only"
    bool    rxOnly{false};
    bool    transmitting{false};

    QString userMessage;
    QDateTime lastTxDate;
    QDateTime lastRxDate;
    QString lastRxCallsign;
    QString lastRxMode;
    int     snrVal{-99};        // -99 = unknown
    QString snrText;
    QDateTime lastUpdate;
    QDateTime connectTime;

    // Computed (set by FreeDVStationModel, not the client)
    double  distanceKm{0.0};
    double  headingDeg{0.0};
    QString headingCardinal;

    // UI hints (set by view layer, not the client)
    QColor foregroundColor;
    QColor backgroundColor;
};

}  // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::FreeDVStation)
