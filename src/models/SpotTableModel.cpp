// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - SpotTableModel implementation. 8-column QAbstractTableModel
// over a bounded QVector<DxSpot>; bandForFreq() maps MHz to band labels
// using the IARU Region 2 amateur band edges (160m..2m).
//
// Ported from AetherSDR src/gui/DxClusterDialog.cpp:75-204 [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task D2. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR". Implementation
//                                    extracted from
//                                    DxClusterDialog.cpp:75-204 verbatim:
//                                    extractMode (known mode-token set
//                                    of 20 entries, first-or-last
//                                    word match), data (DisplayRole +
//                                    TextAlignmentRole + ForegroundRole
//                                    + UserRole-on-ColFreq for sort key),
//                                    headerData (8 column labels), addSpot
//                                    (prepend + cap-trim), addSpots
//                                    (reverse-prepend keeps newest first
//                                    + cap-trim), freqAtRow (bounds-checked
//                                    accessor), clear (model-reset), and
//                                    bandForFreq (160m..2m IARU lookup).
//                                    Foreground colours (DxCall accent
//                                    #00B4D8 cyan, Freq #E0D060
//                                    yellow-ish) preserved verbatim.
//                                    AI tooling: Anthropic Claude Code.
//   2026-10-08  J.J. Boyd / KG4VCF  Rotor control plan Task 8: the Bearing
//                                    column (whole degrees, "330°"; the long
//                                    path in its tooltip; sorts by number),
//                                    setBearingResolver, bearingAtRow.
//                                    AI-assisted via Anthropic Claude Code.

#include "SpotTableModel.h"

#include "core/GreatCircle.h"
#include "models/SpotBeamTurner.h"

#include <QColor>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTime>

#include <cmath>

namespace NereusSDR {

namespace {

// Rotor control plan Task 8: 0 to under 360 is a bearing; -1 is none.
bool isBearing(double deg)
{
    return std::isfinite(deg) && deg >= 0.0 && deg < 360.0;
}

QString wholeDegrees(double deg)
{
    return SpotBeamTurner::degreesText(deg);
}

} // namespace

// From AetherSDR src/gui/DxClusterDialog.cpp:75-88 [@0cd4559]
QString SpotTableModel::extractMode(const QString& comment)
{
    static const QSet<QString> known = {
        "CW", "SSB", "USB", "LSB", "AM", "FM", "FT8", "FT4",
        "JS8", "RTTY", "PSK31", "PSK63", "PSK", "OLIVIA",
        "JT65", "JT9", "SAM", "NFM", "DIGU", "DIGL"
    };
    QStringList words = comment.split(' ', Qt::SkipEmptyParts);
    if (!words.isEmpty() && known.contains(words.first().toUpper()))
        return words.first().toUpper();
    if (!words.isEmpty() && known.contains(words.last().toUpper()))
        return words.last().toUpper();
    return {};
}

// From AetherSDR src/gui/DxClusterDialog.cpp:90-126 [@0cd4559]
QVariant SpotTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_spots.size())
        return {};

    const auto& spot = m_spots[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColTime:    return spot.utcTime.toString("HH:mm");
        case ColFreq:    return QString::number(spot.freqMhz * 1000.0, 'f', 1);
        case ColDxCall:  return spot.dxCall;
        case ColMode:    return extractMode(spot.comment);
        case ColComment: return spot.comment;
        case ColSpotter: return spot.spotterCall;
        case ColBand:    return bandForFreq(spot.freqMhz);
        case ColSource:  return spot.source;
        case ColBearing: return wholeDegrees(bearingAtRow(index.row()));
        }
    }
    // Rotor control plan Task 8: the long path beside the short.
    if (role == Qt::ToolTipRole && index.column() == ColBearing) {
        const double deg = bearingAtRow(index.row());
        if (!isBearing(deg)) {
            return {};
        }
        return QStringLiteral("%1 short path, %2 long path")
            .arg(wholeDegrees(deg), wholeDegrees(GreatCircle::longPathBearingDeg(deg)));
    }
    if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColFreq)
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        if (index.column() == ColTime)
            return QVariant(Qt::AlignCenter);
        if (index.column() == ColBearing) {
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        }
    }
    if (role == Qt::ForegroundRole) {
        if (index.column() == ColDxCall)
            return QColor(0x00, 0xb4, 0xd8);  // accent
        if (index.column() == ColFreq)
            return QColor(0xe0, 0xd0, 0x60);  // yellow-ish
    }
    // Store freq in UserRole for sorting
    if (role == Qt::UserRole && index.column() == ColFreq)
        return spot.freqMhz;
    if (role == Qt::UserRole && index.column() == ColBearing) {
        return bearingAtRow(index.row());
    }

    return {};
}

// From AetherSDR src/gui/DxClusterDialog.cpp:128-143 [@0cd4559]
QVariant SpotTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case ColTime:    return "Time";
    case ColFreq:    return "Freq (kHz)";
    case ColDxCall:  return "DX Call";
    case ColMode:    return "Mode";
    case ColComment: return "Comment";
    case ColSpotter: return "Spotter";
    case ColBand:    return "Band";
    case ColSource:  return "Source";
    case ColBearing: return "Bearing";
    }
    return {};
}

// From AetherSDR src/gui/DxClusterDialog.cpp:145-156 [@0cd4559]
void SpotTableModel::addSpot(const DxSpot& spot)
{
    beginInsertRows({}, 0, 0);
    m_spots.prepend(spot);
    endInsertRows();

    if (m_spots.size() > m_maxSpots) {
        beginRemoveRows({}, m_maxSpots, m_spots.size() - 1);
        m_spots.resize(m_maxSpots);
        endRemoveRows();
    }
}

// From AetherSDR src/gui/DxClusterDialog.cpp:158-173 [@0cd4559]
void SpotTableModel::addSpots(const QVector<DxSpot>& spots)
{
    if (spots.isEmpty()) return;
    int count = spots.size();
    beginInsertRows({}, 0, count - 1);
    // Prepend in reverse so newest is at index 0
    for (int i = count - 1; i >= 0; --i)
        m_spots.prepend(spots[i]);
    endInsertRows();

    if (m_spots.size() > m_maxSpots) {
        beginRemoveRows({}, m_maxSpots, m_spots.size() - 1);
        m_spots.resize(m_maxSpots);
        endRemoveRows();
    }
}

// From AetherSDR src/gui/DxClusterDialog.cpp:175-180 [@0cd4559]
double SpotTableModel::freqAtRow(int row) const
{
    if (row >= 0 && row < m_spots.size())
        return m_spots[row].freqMhz;
    return 0.0;
}

// From AetherSDR src/gui/DxClusterDialog.cpp:182-187 [@0cd4559]
void SpotTableModel::setBearingResolver(BearingResolver resolver)
{
    m_bearingResolver = std::move(resolver);
    if (!m_spots.isEmpty()) {
        emit dataChanged(index(0, ColBearing), index(m_spots.size() - 1, ColBearing));
    }
}

double SpotTableModel::servedBearingAtRow(int row) const
{
    if (row < 0 || row >= m_spots.size()) {
        return -1.0;
    }
    const double served = m_spots[row].bearingDeg;
    return isBearing(served) ? served : -1.0;
}

double SpotTableModel::bearingAtRow(int row) const
{
    if (row < 0 || row >= m_spots.size()) {
        return -1.0;
    }
    const double served = servedBearingAtRow(row);
    if (isBearing(served)) {
        return served;
    }
    if (!m_bearingResolver) {
        return -1.0;
    }
    const double local = m_bearingResolver(m_spots[row].dxCall);
    return isBearing(local) ? local : -1.0;
}

void SpotTableModel::clear()
{
    beginResetModel();
    m_spots.clear();
    endResetModel();
}

// From AetherSDR src/gui/DxClusterDialog.cpp:189-204 [@0cd4559]
QString SpotTableModel::bandForFreq(double mhz)
{
    if (mhz >= 1.8   && mhz <= 2.0)    return "160m";
    if (mhz >= 3.5   && mhz <= 4.0)    return "80m";
    if (mhz >= 5.0   && mhz <= 5.5)    return "60m";
    if (mhz >= 7.0   && mhz <= 7.3)    return "40m";
    if (mhz >= 10.1  && mhz <= 10.15)  return "30m";
    if (mhz >= 14.0  && mhz <= 14.35)  return "20m";
    if (mhz >= 18.068 && mhz <= 18.168) return "17m";
    if (mhz >= 21.0  && mhz <= 21.45)  return "15m";
    if (mhz >= 24.89 && mhz <= 24.99)  return "12m";
    if (mhz >= 28.0  && mhz <= 29.7)   return "10m";
    if (mhz >= 50.0  && mhz <= 54.0)   return "6m";
    if (mhz >= 144.0 && mhz <= 148.0)  return "2m";
    return "";
}

} // namespace NereusSDR
