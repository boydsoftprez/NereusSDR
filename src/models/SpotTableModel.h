// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - SpotTableModel: QAbstractTableModel wrapping a bounded
// QVector<DxSpot> with 8 columns (Time, Freq, DxCall, Comment, Spotter,
// Band, Mode, Source). Newest spot at row 0; bounded at 500 (default).
// addSpot / addSpots / clear / setMaxSpots / freqAtRow API. Used by the
// DX cluster + spot-collector + RBN + WSJT-X + POTA + FreeDV + PSK
// dialogs (Phase F SpotHubDialog).
//
// Ported from AetherSDR src/gui/DxClusterDialog.h:33-58 [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task D2. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR". Both
//                                    SpotTableModel and BandFilterProxy
//                                    lived inline in AetherSDR's
//                                    DxClusterDialog.h:33-75 (with
//                                    implementations in
//                                    DxClusterDialog.cpp:75-226);
//                                    extracted to standalone src/models/
//                                    files so the SpotHubDialog (Phase F)
//                                    can reuse them. 8-column enum
//                                    (ColTime, ColFreq, ColDxCall,
//                                    ColComment, ColSpotter, ColBand,
//                                    ColMode, ColSource, ColCount)
//                                    preserved verbatim. Public surface
//                                    (addSpot, addSpots, clear,
//                                    setMaxSpots, freqAtRow, extractMode,
//                                    rowCount, columnCount, data,
//                                    headerData) preserved verbatim.
//                                    Default cap of 500 spots
//                                    (m_maxSpots{500}) preserved
//                                    verbatim. AI tooling: Anthropic
//                                    Claude Code.

#pragma once

#include <QAbstractTableModel>
#include <QVector>

#include "core/DxSpot.h"

namespace NereusSDR {

// From AetherSDR src/gui/DxClusterDialog.h:33-58 [@0cd4559]
class SpotTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { ColTime, ColFreq, ColDxCall, ColComment, ColSpotter, ColBand, ColMode, ColSource, ColCount };
    static QString extractMode(const QString& comment);

    explicit SpotTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex& = {}) const override { return m_spots.size(); }
    int columnCount(const QModelIndex& = {}) const override { return ColCount; }
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void addSpot(const DxSpot& spot);
    void addSpots(const QVector<DxSpot>& spots);
    void clear();
    void setMaxSpots(int max) { m_maxSpots = max; }
    double freqAtRow(int row) const;

private:
    static QString bandForFreq(double mhz);

    QVector<DxSpot> m_spots;
    int m_maxSpots{500};
};

} // namespace NereusSDR
