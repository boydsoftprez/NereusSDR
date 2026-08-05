// SPDX-License-Identifier: GPL-2.0-or-later
//
// NereusSDR - FreeDVReporterDialog: standalone window listing every
// connected qso.freedv.org station in a sortable 14-column table.
// Implementation.
//
// Ported from freedv-gui src/gui/dialogs/freedv_reporter.{h,cpp}
// [@77e793a] - Qt6-native rewrite. Per-port cites inline next to each
// function body.
//
// License (upstream): freedv-gui carries an LGPLv2.1+ root license
// (`freedv-gui/COPYING`); the specific `freedv_reporter.cpp` file
// carries a per-file Copyright header (Mooneer Salem, GPLv2.1+)
// reproduced verbatim below. LGPL is upgrade-compatible to GPLv2-or-
// later (LGPL §3 conversion clause).
//
// --- From freedv-gui src/gui/dialogs/freedv_reporter.cpp:1-21 (verbatim header) ---
//
// ==========================================================================
// Name:            freedv_reporter.cpp
// Purpose:         Dialog that displays current FreeDV Reporter spots
// Created:         Jun 12, 2023
// Authors:         Mooneer Salem
//
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
// ==========================================================================
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task G1. Initial shell
//                                    port. Implementation. AI tooling:
//                                    Anthropic Claude Code.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task G2. Filter / QSY /
//                                    message bar implementation.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task G3. Menu bar + per-
//                                    column filters + idle sweep + row
//                                    context menu + double-click QSY.
//                                    Ports the Show / Filter / Idle
//                                    construction from upstream
//                                    freedv_reporter.cpp:395-448
//                                    [@77e793a]; the filter-operator
//                                    submenu shape from :1710-1775; the
//                                    callsign lookup menu from
//                                    :579-602; the row double-click
//                                    QSY trigger from :1463-1477;
//                                    isNumericColumn_ from :2538-2541;
//                                    the column-filter comparator
//                                    pipeline from :2603-2671.
//                                    Inserts a second QSortFilterProxy
//                                    Model (m_columnFilterProxy) on top
//                                    of m_bandProxy so the band filter
//                                    and the per-column filter compose
//                                    naturally without rewriting
//                                    m_bandProxy's filterAcceptsRow.
//                                    Idle sweep is a QTimer on the
//                                    dialog, not a per-station record
//                                    walked from a global 1 s tick the
//                                    way upstream does
//                                    (m_deleteTimer at :499-500
//                                    [@77e793a]); both shapes end up
//                                    calling the same FreeDVStationModel
//                                    ::onStationRemoved entry point.

#include "FreeDVReporterDialog.h"

#include "core/AppSettings.h"
#include "core/FreeDVReporterClient.h"
#include "core/FreeDVStation.h"
#include "models/Band.h"
#include "models/FreeDVStationModel.h"

#include <QAbstractTableModel>
#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDoubleValidator>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QModelIndex>
#include <QPainter>
#include <QPoint>
#include <QPushButton>
#include <QRadioButton>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QtGlobal>

namespace NereusSDR {

// =====================================================================
// Highlight color defaults
//
// TX background: #fc4500 ported verbatim from freedv-gui
//   src/config/ReportingConfiguration.cpp:91 [@77e793a]
//   (default value of freedvReporterTxRowBackgroundColor).
// Msg background: #E58BE5 ported verbatim from freedv-gui
//   src/config/ReportingConfiguration.cpp:95 [@77e793a]
//   (default value of freedvReporterMsgRowBackgroundColor).
// RX background: NereusSDR-architectural divergence. Upstream uses
//   #379baf (cyan-blue, more blue than green) as the default
//   value of freedvReporterRxRowBackgroundColor
//   (ReportingConfiguration.cpp:93 [@77e793a]). NereusSDR picks
//   a green-dominant tone here so the dialog visually distinguishes
//   "actively decoding a callsign right now" (green) from
//   "transmitting" (red); G3 will surface the upstream color knobs
//   to AppSettings and let the user restore upstream cyan if they
//   prefer.
// =====================================================================
// 2026-05-12 bench fix (PR #238): muted palette per user feedback.
// Upstream defaults at freedv-gui ReportingConfiguration.cpp:91-95
// [@77e793a] use saturated tones (TX=#fc4500, RX=#379baf,
// Msg=#E58BE5).  Iterated three times with the bench operator:
//   v1 - upstream-bright       (too neon)
//   v2 - mid-muted              (still too hot)
//   v3 - dim tint (current)     (target: barely-there hint at hue,
//                                 mostly the dark UI showing through)
// The v3 values sit ~30 % brightness and ~35 % saturation — enough
// hue intent that warm/cool/violet stay distinguishable, but the
// row looks tinted rather than filled.  Same priority chain as
// upstream: Msg > TX > RX, so even if Msg + TX overlap the user
// reads the latest event.
//   TX:  dim rust   (warm; "this station is keying")
//   RX:  dim slate  (cool; "this station is hearing someone")
//   Msg: dim mauve  (violet; "message updated")
static const QColor kTxBackgroundColor(0x5e, 0x39, 0x33);  // dim rust
static const QColor kRxBackgroundColor(0x2e, 0x47, 0x50);  // dim slate
static const QColor kMsgBackgroundColor(0x4d, 0x3d, 0x4d); // dim mauve

// Default highlight-clear interval. Production matches the task spec's
// hard-coded 6 s; the test seam drops this to 50 ms.
// TODO: Phase 3J-2 G3 makes this configurable via AppSettings, matching
// upstream's freedv_reporter_tx_rx_highlight_time setting.
static constexpr int kDefaultHighlightClearMs = 6000;

// 2026-05-12 (PR #238 follow-up): source-first from
// freedv-gui src/gui/dialogs/freedv_reporter.cpp:67-69 [@77e793a].
// Upstream uses two RX-coloring windows: a LONG window for
// "station decoded a peer callsign" (cyan stays visible for the full
// timeout) and a SHORT window for "station updated mode/freq but
// has no peer callsign yet" (briefer cyan flash so the row settles
// quickly when nothing is actually being decoded).  Messaging gets
// its own short timeout.
static constexpr int kRxColoringLongMs  = 20'000;  // upstream RX_COLORING_LONG_TIMEOUT_SEC
static constexpr int kRxColoringShortMs =  5'000;  // upstream RX_COLORING_SHORT_TIMEOUT_SEC
static constexpr int kMsgColoringMs     =  5'000;  // upstream MSG_COLORING_TIMEOUT_SEC

// Periodic-recheck cadence.  Upstream walks the full station table
// once per second from its render-update timer to age out the
// coloring (freedv_reporter.cpp:1289-1340).  We do the same here so
// a row whose lastRxDate / lastUpdate aged past the timeout cleans
// up even when no new rx_report / tx_report event arrives.
static constexpr int kColoringRecheckMs = 1'000;

// =====================================================================
// FreeDVReporterTableModel
//
// QAbstractTableModel adapter over FreeDVStationModel. Each row is a
// FreeDVStation snapshot; the column count is fixed at 14 (see
// FreeDVReporterColumn enum in the header).
//
// The mapping rules (header text, alignment, cell text formatting) are
// drawn from freedv-gui's createColumn_ switch (freedv_reporter.cpp
// :75-182 [@77e793a]) and the corresponding GetValue() switch
// (freedv_reporter.cpp:3087-3146 [@77e793a]); the QAbstractTableModel
// shape itself is NereusSDR-native since wxDataViewModel is wx-specific.
// =====================================================================
class FreeDVReporterTableModel : public QAbstractTableModel {
public:
    explicit FreeDVReporterTableModel(QObject* parent = nullptr)
        : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : m_rows.size();
    }
    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : kFreeDVReporterColumnCount;
    }

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override {
        if (orientation != Qt::Horizontal || section < 0 || section >= kFreeDVReporterColumnCount) {
            return {};
        }
        if (role == Qt::DisplayRole) {
            // From freedv-gui freedv_reporter.cpp:85-153 [@77e793a]
            // (createColumn_ switch column-name table). G1 hard-codes
            // MHz for col 5; the kHz/MHz toggle from the upstream
            // reportingFrequencyAsKhz setting lands in G3.
            switch (section) {
                case kCallsignCol:       return QStringLiteral("Callsign");
                case kGridSquareCol:     return QStringLiteral("Locator");
                case kDistanceCol:
                    // Phase 3R K-bench (bench feedback): switch header
                    // between km / mi based on the SpotHub FreeDV
                    // tab's "Display distances in miles" checkbox.
                    return AppSettings::instance()
                                   .value("FreeDvReporter/DistanceMiles",
                                          "False").toString() == "True"
                               ? QStringLiteral("mi")
                               : QStringLiteral("km");
                case kHeadingCol:        return QStringLiteral("Hdg");
                case kVersionCol:        return QStringLiteral("Version");
                case kFrequencyCol:
                    // From freedv-gui freedv_reporter.cpp:108 [@77e793a]
                    // (reportingFrequencyAsKhz column-name switch).
                    return AppSettings::instance()
                                   .value("FreeDvReporter/FrequencyAsKhz",
                                          "False").toString() == "True"
                               ? QStringLiteral("kHz")
                               : QStringLiteral("MHz");
                case kTxModeCol:         return QStringLiteral("Mode");
                case kStatusCol:         return QStringLiteral("Status");
                case kUserMessageCol:    return QStringLiteral("Msg");
                case kLastTxDateCol:     return QStringLiteral("Last TX");
                case kLastRxCallsignCol: return QStringLiteral("RX Call");
                case kLastRxModeCol:     return QStringLiteral("Mode (RX)");
                case kSnrCol:            return QStringLiteral("SNR");
                case kLastUpdateDateCol: return QStringLiteral("Last Update");
                default: return {};
            }
        }
        if (role == Qt::TextAlignmentRole) {
            // From freedv-gui freedv_reporter.cpp:79+93-153 [@77e793a]
            // (renderer SetAlignment per column). Most columns default
            // to wxALIGN_LEFT; DISTANCE / HEADING / FREQUENCY override
            // to wxALIGN_RIGHT; USER_MESSAGE builds with wxALIGN_CENTER.
            switch (section) {
                case kDistanceCol:
                case kHeadingCol:
                case kFrequencyCol:
                    return int(Qt::AlignRight | Qt::AlignVCenter);
                case kUserMessageCol:
                    return int(Qt::AlignHCenter | Qt::AlignVCenter);
                default:
                    return int(Qt::AlignLeft | Qt::AlignVCenter);
            }
        }
        return {};
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
            return {};
        }
        const FreeDVStation& s = m_rows.at(index.row());

        if (role == Qt::DisplayRole) {
            // From freedv-gui freedv_reporter.cpp:3087-3146 [@77e793a]
            // (GetValue switch). NereusSDR formats the per-cell strings
            // up front here instead of caching them on the row struct
            // the way upstream does (freedv-gui refreshes `freqString`
            // / `snr` / `heading` on the ReporterData record itself).
            switch (index.column()) {
                case kCallsignCol:       return s.callsign;
                case kGridSquareCol:     return s.gridSquare;
                case kDistanceCol:       return formatDistance(s.distanceKm);
                case kHeadingCol:        return formatHeading(s.headingDeg, s.headingCardinal);
                case kVersionCol:        return s.version;
                case kFrequencyCol:      return formatFrequencyMhz(s.frequencyHz);
                case kTxModeCol:         return s.txMode;
                case kStatusCol:         return s.status;
                case kUserMessageCol:    return s.userMessage;
                case kLastTxDateCol:     return formatTimestamp(s.lastTxDate);
                case kLastRxCallsignCol: return s.lastRxCallsign;
                case kLastRxModeCol:     return s.lastRxMode;
                case kSnrCol:            return formatSnr(s.snrVal);
                case kLastUpdateDateCol: return formatTimestamp(s.lastUpdate);
                default: return {};
            }
        }
        if (role == Qt::TextAlignmentRole) {
            // Per-cell alignment mirrors per-header alignment.
            return headerData(index.column(), Qt::Horizontal, Qt::TextAlignmentRole);
        }
        if (role == Qt::EditRole) {
            // G2: surface raw values for sort + filter.
            //   Col kFrequencyCol -> quint64 Hz (consumed by band filter
            //     proxy). Col kSnrCol -> int snrVal (consumed by sort).
            //   Other columns fall through to the display string so
            //   sort-by-display stays sensible.
            switch (index.column()) {
                case kFrequencyCol: return QVariant::fromValue(s.frequencyHz);
                case kSnrCol:       return s.snrVal;
                case kDistanceCol:  return s.distanceKm;
                default:            return data(index, Qt::DisplayRole);
            }
        }
        if (role == Qt::UserRole) {
            // G2: per-row sid lookup for the bottom-bar QSY button.
            return s.sid;
        }
        return {};
    }

    // FreeDVStationModel adapters. The dialog connects these to the
    // model's signals so the table stays in sync.
    void onStationAdded(const QString& sid, const FreeDVStation& info) {
        if (m_indexBySid.contains(sid)) {
            onStationUpdated(sid, info);
            return;
        }
        const int newRow = m_rows.size();
        beginInsertRows(QModelIndex(), newRow, newRow);
        m_rows.append(info);
        m_indexBySid.insert(sid, newRow);
        endInsertRows();
    }

    void onStationUpdated(const QString& sid, const FreeDVStation& info) {
        auto it = m_indexBySid.constFind(sid);
        if (it == m_indexBySid.constEnd()) {
            onStationAdded(sid, info);
            return;
        }
        const int row = it.value();
        m_rows[row] = info;
        emit dataChanged(index(row, 0), index(row, kFreeDVReporterColumnCount - 1));
    }

    void onStationRemoved(const QString& sid) {
        auto it = m_indexBySid.constFind(sid);
        if (it == m_indexBySid.constEnd()) {
            return;
        }
        const int row = it.value();
        beginRemoveRows(QModelIndex(), row, row);
        m_rows.removeAt(row);
        m_indexBySid.remove(sid);
        // Reindex the trailing rows.
        for (auto i = m_indexBySid.begin(); i != m_indexBySid.end(); ++i) {
            if (i.value() > row) {
                i.value() -= 1;
            }
        }
        endRemoveRows();
    }

    void onCleared() {
        beginResetModel();
        m_rows.clear();
        m_indexBySid.clear();
        endResetModel();
    }

    int rowForSid(const QString& sid) const {
        return m_indexBySid.value(sid, -1);
    }

    // 2026-05-12 bench fix (PR #238): the row-highlight delegate
    // state lives outside the model (in
    // FreeDVReporterRowHighlightDelegate::m_highlight) so changing
    // a highlight has no data-changed signal that the QTableView
    // can use to schedule a repaint.  Calling viewport()->update()
    // by itself wasn't enough on macOS Cocoa builds — the view
    // optimized the repaint away because no underlying data
    // changed, and the tints only appeared when the user clicked
    // a row (forcing a selection-change repaint).
    //
    // This hook lets the dialog explicitly emit dataChanged for a
    // sid's row after touching the highlight hash so Qt schedules
    // a guaranteed repaint of every cell in that row.  The role
    // hint stays empty (== "all roles") to be safe across delegate
    // styles that may key cache on different roles.
    void notifyRowChangedForSid(const QString& sid) {
        const int row = rowForSid(sid);
        if (row < 0 || row >= m_rows.size()) return;
        const QModelIndex topLeft     = index(row, 0);
        const QModelIndex bottomRight = index(row, columnCount() - 1);
        emit dataChanged(topLeft, bottomRight);
    }

private:
    // From freedv-gui freedv_reporter.cpp:3180-3194 [@77e793a].
    // Upstream branches on reportingFrequencyAsKhz: kHz mode renders
    // `freqHz / 1000.0` with 1 decimal place (e.g. 14236.0 kHz); MHz
    // mode renders `freqHz / 1.0e6` with 4 decimal places (e.g.
    // 14.2360 MHz). The toggle is exposed in the SpotHub FreeDV
    // Preferences group.
    static QString formatFrequencyMhz(quint64 frequencyHz) {
        if (frequencyHz == 0) {
            return QStringLiteral(" - ");
        }
        const bool asKhz =
            AppSettings::instance()
                .value("FreeDvReporter/FrequencyAsKhz", "False").toString()
            == "True";
        if (asKhz) {
            const double khz = static_cast<double>(frequencyHz) / 1.0e3;
            return QLocale::system().toString(khz, 'f', 1);
        }
        const double mhz = static_cast<double>(frequencyHz) / 1.0e6;
        return QLocale::system().toString(mhz, 'f', 4);
    }

    // SNR rendering. NereusSDR-architectural divergence from upstream:
    // upstream calls wxNumberFormatter::ToString(snr, 1) which yields
    // "12.0" / "-99.0" / etc (freedv_reporter.cpp:3692 [@77e793a]);
    // NereusSDR renders the integer with a forced sign prefix and
    // shows " - " for the unknown-SNR sentinel (-99) so the column
    // stays narrow at 60 px minimum width. G3 will surface this as a
    // per-radio formatter setting if upstream parity is requested.
    static QString formatSnr(int snrVal) {
        if (snrVal == -99) {
            return QStringLiteral(" - ");
        }
        if (snrVal >= 0) {
            return QStringLiteral("+%1").arg(snrVal);
        }
        return QString::number(snrVal);
    }

    // From freedv-gui freedv_reporter.cpp:3358-3363 [@77e793a]
    // (refreshAllRows distance/heading string assembly). Upstream uses
    // wxNumberFormatter::ToString(distance, 0); NereusSDR matches that.
    static QString formatDistance(double distanceKm) {
        if (distanceKm <= 0.0) {
            return QStringLiteral(" - ");
        }
        // Phase 3R K-bench (bench feedback): user-selectable
        // miles/km display, matching freedv-gui's `useMetricUnits`
        // setting. 1 km = 0.621371 mi.
        const bool miles =
            AppSettings::instance()
                .value("FreeDvReporter/DistanceMiles", "False").toString()
            == "True";
        const double value = miles ? distanceKm * 0.621371 : distanceKm;
        return QString::number(static_cast<int>(value + 0.5));
    }

    // From freedv-gui freedv_reporter.cpp:3196-3210 [@77e793a]
    // (refreshAllRows heading string). Upstream renders either
    // GetCardinalDirection_(headingVal) or ToString(headingVal, 0) per
    // user setting reportingDirectionAsCardinal. NereusSDR pre-stamps
    // the cardinal in FreeDVStationModel (Task D3 followup) and renders
    // "045 NE" combining both forms; when no grid is available the
    // upstream `parent_->UNKNOWN_STR` path produces " - ".
    static QString formatHeading(double headingDeg, const QString& cardinal) {
        if (headingDeg <= 0.0 && cardinal.isEmpty()) {
            return QStringLiteral(" - ");
        }
        // Phase 3R K-bench (bench feedback): switch render mode based
        // on the SpotHub FreeDV tab's "Cardinal" checkbox, mirroring
        // freedv-gui's reportingDirectionAsCardinal setting.
        //   cardinal-mode ON  -> "NE"        (just the compass label)
        //   cardinal-mode OFF -> "045°"      (numeric bearing, no card)
        //   default (cur)     -> "045° NE"   (both, as we had pre-fix)
        const bool cardinalMode =
            AppSettings::instance()
                .value(QStringLiteral("FreeDvReporter/DirectionAsCardinal"),
                       QStringLiteral("Combined"))
                .toString() == QStringLiteral("Cardinal");
        const bool numericMode =
            AppSettings::instance()
                .value(QStringLiteral("FreeDvReporter/DirectionAsCardinal"),
                       QStringLiteral("Combined"))
                .toString() == QStringLiteral("Numeric");
        const QString deg = QStringLiteral("%1").arg(
            static_cast<int>(headingDeg + 0.5),
            3, 10, QLatin1Char('0'));
        if (cardinalMode && !cardinal.isEmpty()) {
            return cardinal;
        }
        if (numericMode) {
            return deg + QStringLiteral("°");
        }
        if (cardinal.isEmpty()) {
            return deg + QStringLiteral("°");
        }
        return deg + QStringLiteral("° ") + cardinal;
    }

    // From freedv-gui freedv_reporter.cpp:2308 [@77e793a] (`return
    // parent_->UNKNOWN_STR`) - upstream formats datestamps via the
    // makeValidTime_() helper, which calls wxDateTime::Format("%x %X")
    // by default; NereusSDR renders local time with HH:mm:ss per the
    // G1 task spec ("upstream wxDateTime::Format("%H:%M:%S")"). The
    // wider %x %X format lands in G3 once the kHz/MHz toggle exposes
    // the format setting.
    static QString formatTimestamp(const QDateTime& dt) {
        if (!dt.isValid()) {
            return QStringLiteral(" - ");
        }
        return dt.toLocalTime().toString(QStringLiteral("HH:mm:ss"));
    }

    QList<FreeDVStation>     m_rows;
    QHash<QString, int>      m_indexBySid;
};

// =====================================================================
// FreeDVReporterRowHighlightDelegate
//
// Paints row backgrounds when a sid has an active TX / RX / Msg
// highlight. The highlight map is owned by the dialog (so a per-sid
// QTimer can clear it after kDefaultHighlightClearMs) and queried by
// the delegate at paint time.
//
// The delegate is a NereusSDR-architectural divergence from upstream's
// approach (upstream stamps `backgroundColor` directly onto the
// `ReporterData` row struct and the wxDataViewCtrl renderer reads it
// off the row; freedv_reporter.cpp:3026 [@77e793a]). Keeping the
// highlight state in a separate view-side QHash keeps the QAbstract
// TableModel data layer free of view bookkeeping.
// =====================================================================
class FreeDVReporterRowHighlightDelegate : public QStyledItemDelegate {
public:
    explicit FreeDVReporterRowHighlightDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        // Resolve the sid for this row via Qt::UserRole, which the
        // FreeDVReporterTableModel surfaces for every cell. Works
        // through the QSortFilterProxyModel too since the proxy
        // forwards data() calls to the source.
        const QString sid = index.data(Qt::UserRole).toString();
        const QColor bg = m_highlight.value(sid);

        if (bg.isValid()) {
            // 2026-05-12 bench fix (PR #238): force-fill the cell
            // rectangle BEFORE the base class paints so the tint is
            // unconditionally visible.  Earlier code only set
            // opt.backgroundBrush + opt.palette.Highlight, which on
            // some macOS Cocoa builds gets overdrawn by the system
            // style's own PE_PanelItemViewItem fill (the brush hint
            // is ignored on platforms where the native style paints
            // its own cell chrome).  Filling here gives a guaranteed
            // base layer; the brush + palette hints stay so any
            // style that DOES honor them paints consistently.
            painter->fillRect(opt.rect, bg);
            opt.backgroundBrush = QBrush(bg);
            opt.palette.setBrush(QPalette::Highlight, QBrush(bg));
            // Keep the highlighted-text color readable on the colored
            // background. The three highlight colors are bright enough
            // that white text reads well on all of them.
            opt.palette.setBrush(QPalette::HighlightedText,
                                 QBrush(Qt::white));
            // Override the normal-state text color too, since the
            // base paint will use QPalette::Text (not Highlight) on
            // unselected rows.  Without this the tint shows but text
            // disappears against a dark cyan/orange background on
            // some themes.
            opt.palette.setBrush(QPalette::Text, QBrush(Qt::white));
            opt.palette.setBrush(QPalette::WindowText, QBrush(Qt::white));
        }
        QStyledItemDelegate::paint(painter, opt, index);
    }

    // Highlight map mutators - the dialog drives these on stationUpdated.
    void setRowHighlight(const QString& sid, const QColor& bg) {
        if (bg.isValid()) {
            m_highlight.insert(sid, bg);
        } else {
            m_highlight.remove(sid);
        }
    }
    QColor highlightFor(const QString& sid) const {
        return m_highlight.value(sid);
    }

private:
    QHash<QString, QColor>       m_highlight;
};

// =====================================================================
// FreeDVReporterBandFilterProxy
//
// QSortFilterProxyModel sitting between the QTableView and the
// FreeDVReporterTableModel. Hides any row whose station frequency
// (col 5 EditRole, raw quint64 Hz) does not fall inside the band the
// user picked in m_bandFilter.
//
// NereusSDR-architectural divergence from upstream. freedv-gui sets
// `reportData->isVisible = false` on each per-sid record and re-emits
// row-changed events into the wxDataViewModel (freedv_reporter.cpp
// :3215-3217 [@77e793a]). Qt's MV separation lets us drop the
// per-record isVisible bookkeeping entirely and run the filter
// declaratively in filterAcceptsRow.
// =====================================================================
class FreeDVReporterBandFilterProxy : public QSortFilterProxyModel {
public:
    explicit FreeDVReporterBandFilterProxy(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent) {}

    void setSelectedBand(Band band) {
        if (m_band == band && m_haveBand) {
            return;
        }
        m_band = band;
        m_haveBand = true;
        m_exactMode = false;
        invalidateFilterCompat();
    }
    void setShowAll() {
        if (!m_haveBand && !m_exactMode) {
            return;
        }
        m_haveBand = false;
        m_exactMode = false;
        invalidateFilterCompat();
    }

    // Phase 3R K-bench (bench feedback): "Exact freq" filter mode.
    // Operator's VFO Hz + tolerance; only stations within +/- tolerance
    // of the VFO pass. Tolerance default 3 kHz matches freedv-gui's
    // EXACT_FREQ_TOLERANCE_HZ. Use setExactFrequency to enable;
    // setShowAll / setSelectedBand reverts.
    void setExactFrequency(quint64 freqHz, quint64 toleranceHz = 3000) {
        m_exactMode = true;
        m_haveBand = false;
        m_exactFreqHz = freqHz;
        m_exactTolHz = toleranceHz;
        invalidateFilterCompat();
    }

protected:
    // Qt 6.13 deprecated invalidateFilter() in favor of
    // beginFilterChange() / endFilterChange(Direction::Rows). Both APIs
    // exist in Qt 6.10+. NereusSDR has no explicit Qt minimum in
    // CMakeLists.txt, so we keep a fallback to invalidateFilter on
    // older Qt builds. NereusSDR-only compat shim; not an upstream port.
    void invalidateFilterCompat() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
        invalidateFilter();
#endif
    }

    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent)
        const override {
        if (!m_haveBand && !m_exactMode) {
            return true;
        }
        // Frequency lives in EditRole on the source model's col 5.
        const QModelIndex freqIdx =
            sourceModel()->index(sourceRow, kFrequencyCol, sourceParent);
        const quint64 hz = freqIdx.data(Qt::EditRole).toULongLong();
        if (hz == 0) {
            return false;  // Unknown freq: only "All" matches
        }
        if (m_exactMode) {
            // |station_freq - vfo_freq| < tolerance
            const quint64 diff = (hz > m_exactFreqHz)
                                     ? (hz - m_exactFreqHz)
                                     : (m_exactFreqHz - hz);
            return diff <= m_exactTolHz;
        }
        // Band::bandFromFrequency maps Hz to the enclosing amateur band;
        // see src/models/Band.cpp.
        const Band rowBand = bandFromFrequency(static_cast<double>(hz));
        return rowBand == m_band;
    }

private:
    Band     m_band{Band::GEN};
    bool     m_haveBand{false};
    bool     m_exactMode{false};
    quint64  m_exactFreqHz{0};
    quint64  m_exactTolHz{3000};
};

// =====================================================================
// FreeDVReporterColumnFilterProxy
//
// Second-tier proxy sitting on top of FreeDVReporterBandFilterProxy.
// Reads its filter dictionary from FreeDVReporterDialog::m_columnFilters
// and tests every row against the active per-column operator(s).
//
// Composes naturally with the band proxy: any row hidden by either layer
// stays hidden in the table view; a row must clear both layers to be
// visible.
//
// From freedv-gui freedv_reporter.cpp:2603-2671 [@77e793a]
//   (FreeDVReporterDataModel::isFiltered_ inner block that walks the
//   columnFilterOperators_ / columnFilterValues_ vectors).
// =====================================================================
class FreeDVReporterColumnFilterProxy : public QSortFilterProxyModel {
public:
    explicit FreeDVReporterColumnFilterProxy(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent) {}

    void setFilters(const QHash<int, QPair<int, QVariant>>* filters) {
        m_filters = filters;
        invalidateFilterCompat();
    }
    void refresh() { invalidateFilterCompat(); }

protected:
    // Qt 6.13 deprecated invalidateFilter() in favor of
    // beginFilterChange() / endFilterChange(Direction::Rows). Both APIs
    // exist in Qt 6.10+. NereusSDR has no explicit Qt minimum in
    // CMakeLists.txt, so we keep a fallback to invalidateFilter on
    // older Qt builds. NereusSDR-only compat shim; not an upstream port.
    void invalidateFilterCompat() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
        invalidateFilter();
#endif
    }

    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent)
        const override {
        if (!m_filters || m_filters->isEmpty()) {
            return true;
        }
        for (auto it = m_filters->constBegin(); it != m_filters->constEnd(); ++it) {
            const int col = it.key();
            const int op = it.value().first;
            const QVariant val = it.value().second;
            if (op == kFilterNone) {
                continue;
            }
            const QModelIndex cellIdx =
                sourceModel()->index(sourceRow, col, sourceParent);
            if (!cellIdx.isValid()) {
                continue;
            }

            int cmp = 0;
            if (isNumericColumn(col)) {
                // From freedv-gui freedv_reporter.cpp:2612-2650 [@77e793a]
                //   (parse filter value as a double, compare against the
                //   row's raw numeric).
                bool okFilter = false;
                const double filterNum = val.toString().toDouble(&okFilter);
                if (!okFilter) {
                    continue;
                }
                double rowNum = 0.0;
                switch (col) {
                    case kDistanceCol:
                        rowNum = cellIdx.data(Qt::EditRole).toDouble();
                        break;
                    case kHeadingCol:
                        rowNum = cellIdx.data(Qt::DisplayRole).toString()
                                     .left(3).toDouble();
                        break;
                    case kSnrCol:
                        rowNum = cellIdx.data(Qt::EditRole).toDouble();
                        break;
                    case kFrequencyCol:
                        // Display unit is MHz - match the upstream
                        //   "Filter value is entered in the same display
                        //   units as the column" comment.
                        rowNum = cellIdx.data(Qt::EditRole).toDouble() / 1.0e6;
                        break;
                    default:
                        break;
                }
                if (rowNum < filterNum) cmp = -1;
                else if (rowNum > filterNum) cmp = 1;
                else cmp = 0;
            } else {
                // Case-insensitive string compare on the display text.
                //   From freedv-gui freedv_reporter.cpp:2654-2655 [@77e793a]
                //   (wxString::CmpNoCase).
                const QString rowText = cellIdx.data(Qt::DisplayRole).toString();
                cmp = QString::compare(rowText, val.toString(),
                                       Qt::CaseInsensitive);
            }

            bool passes = false;
            switch (op) {
                case kFilterGte: passes = (cmp >= 0); break;
                case kFilterGt:  passes = (cmp > 0); break;
                case kFilterEq:  passes = (cmp == 0); break;
                case kFilterNeq: passes = (cmp != 0); break;
                case kFilterLt:  passes = (cmp < 0); break;
                case kFilterLte: passes = (cmp <= 0); break;
                default:         passes = true; break;
            }
            if (!passes) {
                return false;
            }
        }
        return true;
    }

private:
    // From freedv-gui freedv_reporter.cpp:2538-2541 [@77e793a]
    //   (isNumericColumn_ whitelist).
    static bool isNumericColumn(int col) {
        return col == kDistanceCol || col == kHeadingCol
            || col == kFrequencyCol || col == kSnrCol;
    }

    const QHash<int, QPair<int, QVariant>>* m_filters{nullptr};
};

// =====================================================================
// FreeDVReporterDialog
// =====================================================================

FreeDVReporterDialog::FreeDVReporterDialog(FreeDVStationModel* stationModel,
                                           FreeDVReporterClient* client,
                                           QWidget* parent)
    : QWidget(parent, Qt::Window),
      m_stationModel(stationModel),
      m_client(client),
      m_highlightClearMs(kDefaultHighlightClearMs) {
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowTitle(QStringLiteral("FreeDV Reporter"));
    buildUi();

    if (m_stationModel) {
        connect(m_stationModel, &FreeDVStationModel::stationAdded,
                this, &FreeDVReporterDialog::onStationAdded);
        connect(m_stationModel, &FreeDVStationModel::stationUpdated,
                this, &FreeDVReporterDialog::onStationUpdated);
        connect(m_stationModel, &FreeDVStationModel::stationRemoved,
                this, &FreeDVReporterDialog::onStationRemoved);
        connect(m_stationModel, &FreeDVStationModel::cleared,
                this, &FreeDVReporterDialog::onCleared);
        // Seed any stations the model already holds.
        const auto initial = m_stationModel->stations();
        for (auto it = initial.constBegin(); it != initial.constEnd(); ++it) {
            onStationAdded(it.key(), it.value());
        }
    }

    // 2026-05-12 (PR #238 follow-up): periodic coloring recheck.
    // Source-first from freedv-gui freedv_reporter.cpp:1289-1340
    // [@77e793a] — upstream's render-update timer walks every
    // station once per second to recompute the highlight from
    // current-time vs lastRxDate / lastUpdate.  Event-driven recompute
    // alone isn't enough: a row whose lastRxDate ages past the
    // timeout has no triggering event to clear its highlight.  The
    // periodic walk also picks up rows whose state changed via a
    // freq_change / message_update / hide_self event without
    // an accompanying rx_report.
    m_coloringTimer = new QTimer(this);
    m_coloringTimer->setInterval(kColoringRecheckMs);
    connect(m_coloringTimer, &QTimer::timeout, this, [this] {
        if (!m_stationModel) return;
        const auto current = m_stationModel->stations();
        for (auto it = current.constBegin(); it != current.constEnd(); ++it) {
            applyHighlightForStation(it.key(), it.value());
        }
    });
    m_coloringTimer->start();
}

FreeDVReporterDialog::~FreeDVReporterDialog() {
    // m_clearTimers and m_table / delegates are parented to the dialog;
    // Qt's parent ownership reclaims them automatically.
}

void FreeDVReporterDialog::buildUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // G3: Show / Filter / Idle-longer-than menus built by buildMenuBar.
    m_menuBar = new QMenuBar(this);
    outer->setMenuBar(m_menuBar);

    m_tableModel = new FreeDVReporterTableModel(this);
    m_bandProxy  = new FreeDVReporterBandFilterProxy(this);
    m_bandProxy->setSourceModel(m_tableModel);

    // G3: chain a second proxy on top of the band proxy so the
    //   per-column filters compose with the band filter without
    //   rewriting m_bandProxy's filterAcceptsRow. Composition order:
    //   tableModel -> bandProxy -> columnFilterProxy -> view.
    m_columnFilterProxy = new FreeDVReporterColumnFilterProxy(this);
    m_columnFilterProxy->setSourceModel(m_bandProxy);
    m_columnFilterProxy->setFilters(&m_columnFilters);

    // 2026-05-12 bench fix (PR #238): SNR / Frequency / Distance
    // columns surface raw numeric values via Qt::EditRole on the
    // source model (FreeDVReporterTableModel::data at the
    // role == EditRole branch); flipping the proxy's sort role
    // from the default DisplayRole (strings) to EditRole makes
    // those columns sort numerically.  Without this, "Sort by SNR"
    // ordered the string form: "-3" sorted ABOVE "+12" because
    // the literal "-" comes before the digit "1" in lexical
    // order.  EditRole-as-sort-role is a per-proxy setting and
    // string columns (callsign, mode, status, ...) fall back to
    // DisplayRole via the model's switch default at
    // FreeDVReporterTableModel::data:305, so they keep sorting
    // alphabetically.
    m_columnFilterProxy->setSortRole(Qt::EditRole);

    m_rowDelegate = new FreeDVReporterRowHighlightDelegate(this);

    m_table = new QTableView(this);
    m_table->setModel(m_columnFilterProxy);
    m_table->setItemDelegate(m_rowDelegate);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSortingEnabled(true);
    m_table->setAlternatingRowColors(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionsMovable(true);
    m_table->horizontalHeader()->setStretchLastSection(true);

    // G3: right-click on a column header opens the Filter submenu for
    //   that column. Mirrors freedv-gui's OnColumnHeaderRightClick
    //   wiring at freedv_reporter.cpp:619 [@77e793a].
    m_table->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table->horizontalHeader(),
            &QHeaderView::customContextMenuRequested,
            this, &FreeDVReporterDialog::onColumnHeaderRightClicked);

    // G3: right-click on a row opens the lookup / copy / QSY menu;
    //   mirrors freedv-gui's OnItemRightClick at :617 [@77e793a] but
    //   wired off the table view rather than the hover-driven
    //   tempCallsign_ state machine.
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QWidget::customContextMenuRequested,
            this, &FreeDVReporterDialog::onRowContextMenuRequested);

    // G3: double-click on a row emits tuneRequested(freqHz). From
    //   freedv-gui freedv_reporter.cpp:1463-1477 [@77e793a]
    //   (OnItemDoubleClick reads model->getFrequency and forwards to
    //   rigFrequencyController->setFrequency).
    connect(m_table, &QTableView::doubleClicked,
            this, &FreeDVReporterDialog::onRowDoubleClicked);

    // Per-column minimum widths from freedv-gui createColumn_
    //   (freedv_reporter.cpp:75-182 [@77e793a]). Upstream calls
    //   colObj->SetMinWidth(minWidth); Qt does the equivalent via
    //   the header's setMinimumSectionSize per-column.
    static constexpr int kMinWidths[kFreeDVReporterColumnCount] = {
        70,  // CALLSIGN_COL
        65,  // GRID_SQUARE_COL
        60,  // DISTANCE_COL
        60,  // HEADING_COL
        70,  // VERSION_COL
        60,  // FREQUENCY_COL
        65,  // TX_MODE_COL
        60,  // STATUS_COL
        130, // USER_MESSAGE_COL
        60,  // LAST_TX_DATE_COL
        65,  // LAST_RX_CALLSIGN_COL
        60,  // LAST_RX_MODE_COL
        60,  // SNR_COL
        100, // LAST_UPDATE_DATE_COL
    };
    auto* header = m_table->horizontalHeader();
    for (int c = 0; c < kFreeDVReporterColumnCount; ++c) {
        header->resizeSection(c, kMinWidths[c]);
    }

    // Restore persisted per-column widths over the defaults. Stored as a
    // comma-joined width-per-column string under
    // "FreeDvReporter/ColumnWidths". Mirrors freedv-gui's per-column
    // SetWidth restore in OnInitDialog (freedv_reporter.cpp:189-203
    // [@77e793a]).
    {
        const QString widthsCsv = AppSettings::instance()
            .value(QStringLiteral("FreeDvReporter/ColumnWidths"), QString())
            .toString();
        const QStringList widthsList =
            widthsCsv.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (int c = 0;
             c < kFreeDVReporterColumnCount && c < widthsList.size(); ++c) {
            bool ok = false;
            const int w = widthsList[c].toInt(&ok);
            if (ok && w > 10) {
                header->resizeSection(c, w);
            }
        }
    }

    // Restore persisted sort column + order. Default = FREQUENCY_COL
    // ascending, matching freedv-gui FreeDVConfiguration.cpp:52-53
    // [@77e793a] (reporterWindowCurrentSort default 5, currentSortDirection
    // default true).
    {
        const int savedSortCol = AppSettings::instance()
            .value(QStringLiteral("FreeDvReporter/SortColumn"),
                   QString::number(kFrequencyCol)).toInt();
        const bool savedSortAsc = AppSettings::instance()
            .value(QStringLiteral("FreeDvReporter/SortAscending"),
                   QStringLiteral("True")).toString()
            == QStringLiteral("True");
        if (savedSortCol >= 0 && savedSortCol < kFreeDVReporterColumnCount) {
            m_table->sortByColumn(savedSortCol,
                                  savedSortAsc ? Qt::AscendingOrder
                                               : Qt::DescendingOrder);
        }
    }

    // Wire the persist connections AFTER initial restore so the resize /
    // sort calls above don't loop back into AppSettings.
    //
    // Sort persistence: mirrors freedv-gui freedv_reporter.cpp:1057-1060
    // / :1109-1112 [@77e793a] (OnSortChanged stores GetModelColumn /
    // IsSortOrderAscending into reporterWindowCurrentSort{,Direction}).
    connect(header, &QHeaderView::sortIndicatorChanged, this,
            [](int logicalIndex, Qt::SortOrder order) {
                auto& s = AppSettings::instance();
                s.setValue(QStringLiteral("FreeDvReporter/SortColumn"),
                           QString::number(logicalIndex));
                s.setValue(QStringLiteral("FreeDvReporter/SortAscending"),
                           order == Qt::AscendingOrder ? QStringLiteral("True")
                                                       : QStringLiteral("False"));
                s.save();
            });

    // Column-width persistence: fires on user drag-resize. Loops once
    // through the header to capture all current widths as a CSV. (No
    // upstream parity cite -- freedv-gui's wxDataViewListCtrl
    // SetColumnsOrder/GetColumnsOrder handles its own per-column widths
    // through the wx config layer; Qt's QHeaderView::sectionResized is
    // the analogous hook.)
    connect(header, &QHeaderView::sectionResized, this,
            [this](int, int, int) {
                auto* hdr = m_table->horizontalHeader();
                QStringList widths;
                widths.reserve(kFreeDVReporterColumnCount);
                for (int c = 0; c < kFreeDVReporterColumnCount; ++c) {
                    widths << QString::number(hdr->sectionSize(c));
                }
                auto& s = AppSettings::instance();
                s.setValue(QStringLiteral("FreeDvReporter/ColumnWidths"),
                           widths.join(QLatin1Char(',')));
                s.save();
            });

    outer->addWidget(m_table, 1);

    // Bottom bar built by buildBottomBar(); the test seam findChild<>()
    // resolves widgets by objectName().
    buildBottomBar();
    outer->addWidget(m_bottomBarHost);

    // G3: build the menu bar after the table so menu actions can
    //   reference the table directly.
    buildMenuBar();

    // G3: load any persisted column filters AFTER the menu has been
    //   built so the per-column submenu check states are coherent.
    loadColumnFiltersFromSettings();

    // G3: apply persisted column visibility AFTER the table and the
    //   Show menu both exist.
    applyVisibleColumnsFromSettings();

    // G3: idle sweep timer. Default 30 s sweep interval. Threshold
    //   m_idleTimeoutMinutes is reloaded from AppSettings during
    //   buildMenuBar() when the idle-longer-than action group is
    //   constructed.
    m_idleTimer = new QTimer(this);
    connect(m_idleTimer, &QTimer::timeout,
            this, &FreeDVReporterDialog::onIdleSweepTick);
    m_idleTimer->start(m_idleSweepIntervalMs);

    // Connect table-selection signal so the QSY button enables only
    // when exactly one station is selected. Mirrors freedv-gui's
    //   m_buttonSendQSY->Enable(false) default + refreshQSYButtonState
    //   on selection change (freedv_reporter.cpp:1137-1158, 2192
    //   [@77e793a]).
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &FreeDVReporterDialog::onTableSelectionChanged);
}

void FreeDVReporterDialog::buildBottomBar() {
    // Container widget holds two horizontal rows stacked vertically:
    //   Row 1: band filter combo, track-frequency radios, QSY freq edit,
    //          Send QSY button, Open Website, OK.
    //   Row 2: message MRU dropdown, message edit, Send / Save / Clear.
    // From freedv-gui freedv_reporter.cpp:300-388 [@77e793a]
    //   (wxBoxSizer-based reportingSettingsSizer + statusMessageSizer).
    m_bottomBarHost = new QWidget(this);
    auto* hostLayout = new QVBoxLayout(m_bottomBarHost);
    hostLayout->setContentsMargins(4, 4, 4, 4);
    hostLayout->setSpacing(4);

    // -----------------------------------------------------------------
    // Row 1: filter + QSY + Open Website + OK
    // -----------------------------------------------------------------
    auto* row1 = new QHBoxLayout;
    row1->setContentsMargins(0, 0, 0, 0);

    row1->addWidget(new QLabel(QStringLiteral("Band:"), m_bottomBarHost));

    m_bandFilter = new QComboBox(m_bottomBarHost);
    m_bandFilter->setObjectName(QStringLiteral("bandFilterCombo"));
    // Band list per task spec: "All" + 12 ham bands. Upstream's combo
    //   has 13 entries too (freedv_reporter.cpp:324-338 [@77e793a]) but
    //   collapses 6m+ into ">= 6 m" + "Other". NereusSDR surfaces 6m
    //   and 2m explicitly since the radios in our target list (HL2,
    //   ANAN-G2) both reach those bands natively.
    static const char* const kBandNames[] = {
        "All", "160m", "80m", "60m", "40m", "30m", "20m", "17m",
        "15m", "12m", "10m", "6m", "2m"
    };
    for (const char* name : kBandNames) {
        m_bandFilter->addItem(QString::fromLatin1(name));
    }
    // Default = All ("FreeDvReporter/BandFilter" persisted preference).
    const QString savedBand = AppSettings::instance()
        .value(QStringLiteral("FreeDvReporter/BandFilter"),
               QStringLiteral("All")).toString();
    const int savedIdx = m_bandFilter->findText(savedBand);
    m_bandFilter->setCurrentIndex(savedIdx > 0 ? savedIdx : 0);
    onBandFilterChanged(m_bandFilter->currentIndex());
    connect(m_bandFilter,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FreeDVReporterDialog::onBandFilterChanged);
    row1->addWidget(m_bandFilter);

    row1->addWidget(new QLabel(QStringLiteral("Track frequency:"),
                               m_bottomBarHost));
    m_trackBandRadio = new QRadioButton(QStringLiteral("Band"), m_bottomBarHost);
    m_trackBandRadio->setObjectName(QStringLiteral("trackBandRadio"));
    // From freedv-gui freedv_reporter.cpp:353-358 [@77e793a]
    //   (m_trackFreqBand created with wxRB_GROUP + initial value not
    //   touched here, but setBandFilter() at :367 + ::247 default = Band).
    m_trackBandRadio->setChecked(true);
    row1->addWidget(m_trackBandRadio);

    m_trackFreqRadio = new QRadioButton(QStringLiteral("Exact freq"),
                                        m_bottomBarHost);
    m_trackFreqRadio->setObjectName(QStringLiteral("trackFreqRadio"));
    row1->addWidget(m_trackFreqRadio);

    // Phase 3R K-bench (bench feedback): wire the radio toggles so
    // switching between Band and Exact-freq immediately re-applies the
    // filter using the latest cached VFO frequency.
    auto reapplyFilterMode = [this](bool /*checked*/) {
        if (m_currentVfoHz != 0) {
            setActiveFrequency(m_currentVfoHz);
        } else if (m_trackFreqRadio && m_trackFreqRadio->isChecked()
                   && m_bandProxy) {
            // No VFO known but user picked exact-freq — block everything
            // until a setActiveFrequency call lands.
            m_bandProxy->setExactFrequency(0, 0);
        }
    };
    connect(m_trackBandRadio, &QRadioButton::toggled,
            this, reapplyFilterMode);
    connect(m_trackFreqRadio, &QRadioButton::toggled,
            this, reapplyFilterMode);

    row1->addSpacing(8);
    row1->addWidget(new QLabel(QStringLiteral("Frequency (MHz):"),
                               m_bottomBarHost));
    m_qsyFreq = new QLineEdit(m_bottomBarHost);
    m_qsyFreq->setObjectName(QStringLiteral("qsyFreqEdit"));
    m_qsyFreq->setMaximumWidth(120);
    // Accept up to 4 decimal places. Range 0 to 9,999.9999 MHz: sanity
    //   bound only; the real wire shape consumed by
    //   FreeDVReporterClient::requestQSY is quint64 Hz.
    auto* freqValidator = new QDoubleValidator(0.0, 9999.9999, 4, m_qsyFreq);
    freqValidator->setNotation(QDoubleValidator::StandardNotation);
    m_qsyFreq->setValidator(freqValidator);
    row1->addWidget(m_qsyFreq);

    m_qsySendButton = new QPushButton(QStringLiteral("Send QSY"),
                                      m_bottomBarHost);
    m_qsySendButton->setObjectName(QStringLiteral("qsySendButton"));
    // From freedv-gui freedv_reporter.cpp:312 [@77e793a]
    //   (m_buttonSendQSY->Enable(false) - disabled until a station row
    //   gets selected).
    m_qsySendButton->setEnabled(false);
    connect(m_qsySendButton, &QPushButton::clicked,
            this, &FreeDVReporterDialog::onQsySendClicked);
    row1->addWidget(m_qsySendButton);

    row1->addStretch(1);

    m_openWebsiteButton = new QPushButton(QStringLiteral("Open Website"),
                                          m_bottomBarHost);
    m_openWebsiteButton->setObjectName(QStringLiteral("openWebsiteButton"));
    connect(m_openWebsiteButton, &QPushButton::clicked,
            this, &FreeDVReporterDialog::onOpenWebsiteClicked);
    row1->addWidget(m_openWebsiteButton);

    m_closeButton = new QPushButton(QStringLiteral("OK"), m_bottomBarHost);
    m_closeButton->setObjectName(QStringLiteral("closeButton"));
    m_closeButton->setDefault(true);
    // Upstream labels this button "Close" with wxID_OK
    //   (freedv_reporter.cpp:306 [@77e793a]); NereusSDR uses "OK" per
    //   the task spec since both names map to the same close() action.
    connect(m_closeButton, &QPushButton::clicked, this, &QWidget::close);
    row1->addWidget(m_closeButton);

    hostLayout->addLayout(row1);

    // -----------------------------------------------------------------
    // Row 2: message MRU + edit + Send / Save / Clear
    // -----------------------------------------------------------------
    auto* row2 = new QHBoxLayout;
    row2->setContentsMargins(0, 0, 0, 0);

    row2->addWidget(new QLabel(QStringLiteral("Msg"), m_bottomBarHost));

    m_msgDropdown = new QComboBox(m_bottomBarHost);
    m_msgDropdown->setObjectName(QStringLiteral("msgDropdown"));
    m_msgDropdown->setMaximumWidth(28);
    // Down-arrow only - the user picks a stored MRU entry which copies
    // into m_msgEdit. From freedv-gui freedv_reporter.cpp:374-376
    //   [@77e793a] (m_statusMessage wxComboCtrl with a MsgListPopup
    //   that surfaces the saved list and forwards onto the text field).
    refreshMessageDropdown(loadSavedMessages());
    connect(m_msgDropdown,
            QOverload<int>::of(&QComboBox::activated),
            this, &FreeDVReporterDialog::onMessageDropdownActivated);
    row2->addWidget(m_msgDropdown);

    m_msgEdit = new QLineEdit(m_bottomBarHost);
    m_msgEdit->setObjectName(QStringLiteral("msgEdit"));
    m_msgEdit->setPlaceholderText(QStringLiteral("Status message"));
    // Phase 3R K-bench (bench feedback): pre-fill with the saved
    // message from AppSettings so the dialog always shows the
    // currently-broadcast message. Without this, users would see an
    // empty line edit even though the client is actively reporting
    // a stored message to qso.freedv.org.
    {
        const QString saved =
            AppSettings::instance()
                .value(QStringLiteral("FreeDvReporter/Message"), QString())
                .toString();
        if (!saved.isEmpty()) {
            m_msgEdit->setText(saved);
        }
    }
    row2->addWidget(m_msgEdit, 1);

    m_msgSendButton = new QPushButton(QStringLiteral("Send"), m_bottomBarHost);
    m_msgSendButton->setObjectName(QStringLiteral("msgSendButton"));
    connect(m_msgSendButton, &QPushButton::clicked,
            this, &FreeDVReporterDialog::onMessageSendClicked);
    row2->addWidget(m_msgSendButton);

    m_msgSaveButton = new QPushButton(QStringLiteral("Save"), m_bottomBarHost);
    m_msgSaveButton->setObjectName(QStringLiteral("msgSaveButton"));
    connect(m_msgSaveButton, &QPushButton::clicked,
            this, &FreeDVReporterDialog::onMessageSaveClicked);
    row2->addWidget(m_msgSaveButton);

    m_msgClearButton = new QPushButton(QStringLiteral("Clear"), m_bottomBarHost);
    m_msgClearButton->setObjectName(QStringLiteral("msgClearButton"));
    connect(m_msgClearButton, &QPushButton::clicked,
            this, &FreeDVReporterDialog::onMessageClearClicked);
    row2->addWidget(m_msgClearButton);

    hostLayout->addLayout(row2);
}

void FreeDVReporterDialog::onStationAdded(const QString& sid, const FreeDVStation& info) {
    m_tableModel->onStationAdded(sid, info);
    refreshDelegateSidOrder();
    // From freedv-gui freedv_reporter.cpp:1308-1322 [@77e793a]
    // (priority chain: msg > tx > rx). Apply on add too in case the
    // first event we receive already carries TX-active state.
    applyHighlightForStation(sid, info);
}

void FreeDVReporterDialog::onStationUpdated(const QString& sid, const FreeDVStation& info) {
    m_tableModel->onStationUpdated(sid, info);
    refreshDelegateSidOrder();
    // From freedv-gui freedv_reporter.cpp:1308-1322 [@77e793a]
    applyHighlightForStation(sid, info);
}

void FreeDVReporterDialog::onStationRemoved(const QString& sid) {
    if (auto* t = m_clearTimers.take(sid)) {
        t->stop();
        t->deleteLater();
    }
    m_rowDelegate->setRowHighlight(sid, QColor());
    m_tableModel->onStationRemoved(sid);
    refreshDelegateSidOrder();
}

void FreeDVReporterDialog::onCleared() {
    for (auto it = m_clearTimers.begin(); it != m_clearTimers.end(); ++it) {
        it.value()->stop();
        it.value()->deleteLater();
    }
    m_clearTimers.clear();
    m_tableModel->onCleared();
    refreshDelegateSidOrder();
}

void FreeDVReporterDialog::applyHighlightForStation(const QString& sid,
                                                    const FreeDVStation& info) {
    // Source-first from freedv-gui freedv_reporter.cpp:1289-1340
    // [@77e793a] — the four-branch priority chain:
    //   messaging > transmitting > receiving valid callsign >
    //   receiving without valid callsign > default.
    //
    // 2026-05-12 (PR #238 follow-up): restored the previously-dropped
    // SHORT_TIMEOUT branch ("isReceivingNotValidCallsign") so a row
    // that just received a mode-only rx_report (callsign="" but mode
    // set — the more common shape qso.freedv.org delivers in
    // practice) still lights up cyan for 5 s.  Without this, RX
    // highlighting effectively never fired during bench testing —
    // most stations were updating in a mode-only-no-callsign state
    // that was being filtered out as "not receiving" by the
    // !lastRxCallsign.isEmpty() guard.
    const QDateTime now = QDateTime::currentDateTime();

    const bool isTransmitting = info.transmitting;

    const bool isReceivingValidCallsign =
        info.lastRxDate.isValid()
        && !info.lastRxCallsign.isEmpty()
        && info.lastRxDate.msecsTo(now) < kRxColoringLongMs;

    const bool isReceivingNotValidCallsign =
        info.lastRxDate.isValid()
        && info.lastRxCallsign.isEmpty()
        && info.lastRxDate.msecsTo(now) < kRxColoringShortMs;

    const bool isMessaging =
        !info.userMessage.isEmpty()
        && info.lastUpdate.isValid()
        && info.lastUpdate.msecsTo(now) < kMsgColoringMs;

    QColor bg;
    if (isMessaging) {
        bg = kMsgBackgroundColor;
    } else if (isTransmitting) {
        bg = kTxBackgroundColor;
    } else if (isReceivingValidCallsign || isReceivingNotValidCallsign) {
        bg = kRxBackgroundColor;
    }
    setHighlight(sid, bg);
}

void FreeDVReporterDialog::setHighlight(const QString& sid, const QColor& bg) {
    if (bg.isValid()) {
        m_rowDelegate->setRowHighlight(sid, bg);
        // Schedule the clear timer. Replaces any prior clear timer for
        // this sid so a fresh TX event extends the highlight window.
        if (auto* prev = m_clearTimers.take(sid)) {
            prev->stop();
            prev->deleteLater();
        }
        auto* timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [this, sid] {
            m_rowDelegate->setRowHighlight(sid, QColor());
            if (auto* t = m_clearTimers.take(sid)) {
                t->deleteLater();
            }
            if (m_table) {
                m_table->viewport()->update();
            }
        });
        m_clearTimers.insert(sid, timer);
        timer->start(m_highlightClearMs);
    } else {
        m_rowDelegate->setRowHighlight(sid, QColor());
        if (auto* prev = m_clearTimers.take(sid)) {
            prev->stop();
            prev->deleteLater();
        }
    }
    if (m_table) {
        // 2026-05-12 bench fix (PR #238): emit dataChanged for the
        // affected row so the view schedules a guaranteed repaint.
        // viewport()->update() alone was being optimized away by the
        // Qt view framework on macOS — the user had to click a row
        // to force a selection-change repaint before the new tint
        // became visible.
        if (m_tableModel) {
            m_tableModel->notifyRowChangedForSid(sid);
        }
        m_table->viewport()->update();
    }
}

void FreeDVReporterDialog::refreshDelegateSidOrder() {
    // G2: replaced by Qt::UserRole sid lookup inside the delegate.
    //   Kept as an empty hook in case G3 wants to drive sort indicators
    //   or similar per-row metadata refreshes from add / update events.
}

void FreeDVReporterDialog::setHighlightClearMsForTest(int ms) {
    m_highlightClearMs = ms;
    // Restart any active clear timers so the test seam takes effect
    // immediately rather than waiting for the next TX / RX event.
    for (auto it = m_clearTimers.begin(); it != m_clearTimers.end(); ++it) {
        it.value()->stop();
        it.value()->start(m_highlightClearMs);
    }
}

QColor FreeDVReporterDialog::rowHighlightColorForTest(const QString& sid) const {
    if (!m_rowDelegate) {
        return {};
    }
    return m_rowDelegate->highlightFor(sid);
}

// =====================================================================
// G2: bottom-bar slot implementations
// =====================================================================

QUrl FreeDVReporterDialog::websiteUrl() const {
    // From freedv-gui freedv_reporter.cpp:1094-1099 [@77e793a]
    //   (OnOpenWebsite uses appConfiguration.freedvReporterHostname
    //   prefixed with "https://"; default hostname is qso.freedv.org
    //   per ReportingConfiguration.cpp:113 [@77e793a]).
    return QUrl(QStringLiteral("https://qso.freedv.org"));
}

void FreeDVReporterDialog::onBandFilterChanged(int index) {
    if (!m_bandProxy || !m_bandFilter) {
        return;
    }
    if (index <= 0) {
        // "All" -> proxy passes every row through.
        m_bandProxy->setShowAll();
    } else {
        // Item text is "160m" / "80m" / ... / "2m"; Band::bandFromName
        //   parses those into the corresponding Band enum (160m -> Band160m).
        const QString text = m_bandFilter->itemText(index);
        const Band b = bandFromName(text);
        m_bandProxy->setSelectedBand(b);
    }
    // Persist the choice. AppSettings stores strings so the combo item
    //   text is the natural key.
    AppSettings::instance().setValue(
        QStringLiteral("FreeDvReporter/BandFilter"),
        m_bandFilter->itemText(index));
}

void FreeDVReporterDialog::setActiveFrequency(quint64 freqHz) {
    if (!m_bandProxy) {
        return;
    }
    m_currentVfoHz = freqHz;
    if (m_trackFreqRadio && m_trackFreqRadio->isChecked()) {
        // Exact-freq mode: strict Hz equality, matching upstream
        // freedv-gui freedv_reporter.cpp:2578 [@77e793a]
        // (`freq != filteredFrequency_`).  Earlier 3 kHz tolerance
        // surfaced "other freqs leak in on the same band" during
        // bench testing — operators on 7.182 MHz showed up when
        // the VFO sat at 7.183 MHz (1 kHz delta).  qso.freedv.org
        // publishes the dial frequency as a uint64 so the
        // operator's own VFO and the reported frequency are both
        // in 1 Hz units and exact comparison is meaningful.
        m_bandProxy->setExactFrequency(freqHz, 0);
    } else if (m_trackBandRadio && m_trackBandRadio->isChecked()) {
        // Band-tracking mode: snap the band combo to the VFO's band.
        const Band b = bandFromFrequency(static_cast<double>(freqHz));
        const QString name = bandLabel(b);
        if (m_bandFilter) {
            const int idx = m_bandFilter->findText(name);
            if (idx > 0 && idx != m_bandFilter->currentIndex()) {
                m_bandFilter->setCurrentIndex(idx);
                // onBandFilterChanged is wired to the combo's
                // currentIndexChanged signal and runs setSelectedBand
                // on the proxy, so we don't need a direct call here.
            } else if (idx <= 0) {
                // Unknown band (XVTR / WWV) — fall back to "All".
                m_bandFilter->setCurrentIndex(0);
            }
        }
    }
    // Neither radio checked: leave the combo alone.
}

void FreeDVReporterDialog::onTableSelectionChanged(
    const QItemSelection& /*selected*/, const QItemSelection& /*deselected*/) {
    if (!m_qsySendButton || !m_table) {
        return;
    }
    const QModelIndexList rows = m_table->selectionModel()->selectedRows();
    // From freedv-gui freedv_reporter.cpp:2186-2192 [@77e793a]
    //   (refreshQSYButtonState enables only when a single row is
    //   selected AND we are connected to the server).
    m_qsySendButton->setEnabled(rows.size() == 1);
}

QString FreeDVReporterDialog::currentSelectedSid() const {
    if (!m_table) {
        return {};
    }
    const QModelIndexList rows = m_table->selectionModel()->selectedRows();
    if (rows.size() != 1) {
        return {};
    }
    return rows.first().data(Qt::UserRole).toString();
}

void FreeDVReporterDialog::onQsySendClicked() {
    if (!m_qsyFreq) {
        return;
    }
    const QString sid = currentSelectedSid();
    if (sid.isEmpty()) {
        return;
    }
    // Parse MHz from the line edit, convert to Hz. Empty or zero input
    //   leaves the request as a no-op; the upstream button always uses
    //   the radio's own reportingFrequency (freedv_reporter.cpp:1085
    //   [@77e793a]) so 0 Hz here would also be a sentinel for "use the
    //   current TX freq".
    bool ok = false;
    const double mhz = QLocale::c().toDouble(m_qsyFreq->text(), &ok);
    if (!ok || mhz <= 0.0) {
        return;
    }
    const quint64 freqHz = static_cast<quint64>(mhz * 1.0e6 + 0.5);
    emit qsyRequested(sid, freqHz, QString());
    emit tuneRequested(freqHz);
}

void FreeDVReporterDialog::onOpenWebsiteClicked() {
    QDesktopServices::openUrl(websiteUrl());
}

void FreeDVReporterDialog::onMessageSendClicked() {
    if (!m_msgEdit) {
        return;
    }
    emit messageSendRequested(m_msgEdit->text());
}

void FreeDVReporterDialog::onMessageSaveClicked() {
    if (!m_msgEdit) {
        return;
    }
    const QString text = m_msgEdit->text().trimmed();
    if (text.isEmpty()) {
        return;
    }
    QStringList msgs = loadSavedMessages();
    msgs.removeAll(text);    // dedup; most-recent goes to the front
    msgs.prepend(text);
    // From freedv-gui freedv_reporter.cpp - upstream caps the saved
    //   message list at the configured popup-list count. Task spec
    //   says N=10, so cap here.
    constexpr int kMruLimit = 10;
    while (msgs.size() > kMruLimit) {
        msgs.removeLast();
    }
    saveSavedMessages(msgs);
    refreshMessageDropdown(msgs);

    // Phase 3R K-bench (bench feedback): also persist this message as
    // the ACTIVE status message under FreeDvReporter/Message so it
    // survives across app reloads. setIdentity() reads this key at
    // startup; without this write the saved MRU entry was reachable
    // via the dropdown but the "currently broadcasting" message
    // reverted to whatever was there before.
    AppSettings::instance().setValue(
        QStringLiteral("FreeDvReporter/Message"), text);
    AppSettings::instance().save();
}

void FreeDVReporterDialog::onMessageClearClicked() {
    if (!m_msgEdit) {
        return;
    }
    m_msgEdit->clear();
    // Empty text = clear remote message per upstream
    //   (OnStatusTextClear in freedv_reporter.cpp; the wire-level
    //   wrapper FreeDVReporter::updateMessage sends an empty string).
    emit messageSendRequested(QString());
}

void FreeDVReporterDialog::onMessageDropdownActivated(int index) {
    if (!m_msgDropdown || !m_msgEdit) {
        return;
    }
    if (index < 0 || index >= m_msgDropdown->count()) {
        return;
    }
    m_msgEdit->setText(m_msgDropdown->itemText(index));
}

QStringList FreeDVReporterDialog::loadSavedMessages() const {
    const QString raw = AppSettings::instance()
        .value(QStringLiteral("FreeDvReporter/SavedMessages"),
               QString()).toString();
    if (raw.isEmpty()) {
        return {};
    }
    // Stored as newline-joined per the task-spec convention; an
    //   embedded comma in a message would otherwise corrupt the
    //   round-trip via QVariant.toString()'s default ", " join.
    return raw.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

void FreeDVReporterDialog::saveSavedMessages(const QStringList& msgs) const {
    AppSettings::instance().setValue(
        QStringLiteral("FreeDvReporter/SavedMessages"),
        msgs.join(QLatin1Char('\n')));
}

void FreeDVReporterDialog::refreshMessageDropdown(const QStringList& msgs) {
    if (!m_msgDropdown) {
        return;
    }
    m_msgDropdown->clear();
    for (const QString& m : msgs) {
        m_msgDropdown->addItem(m);
    }
    // No default selection - the user explicitly picks an MRU entry to
    //   copy it into m_msgEdit.
    m_msgDropdown->setCurrentIndex(-1);
}

// =====================================================================
// G3: menu-bar construction
//
// Three top-level menus on m_menuBar:
//   Show              - per-column visibility checkmarks
//   Filter            - per-column comparison operators
//   Idle longer than  - 30 min / 1 hour / 2 hours / Never
//
// Ported from freedv-gui freedv_reporter.cpp:391-448 [@77e793a]
//   (wxMenuBar construction). NereusSDR-architectural divergences:
//   1. Upstream nests "Idle more than (minutes)..." as a submenu of the
//      Filter menu (:431); the task spec promotes it to a top-level menu.
//   2. Upstream's Filter menu only contains the idle submenu plus
//      column-filter wiring driven from the column-header right-click.
//      NereusSDR mirrors the column right-click but also exposes a
//      Filter menu with 14 per-column submenus so menu users (no mouse,
//      keyboard-only) have the same access.
//   3. Upstream offers Disabled / 30 / 60 / 90 / 120 / Custom for the
//      idle threshold (:427); the task spec trims to 30 / 60 / 120 /
//      Never.
// =====================================================================

namespace {

// Per-column display labels for the Show menu and the Filter menu's
// 14 submenus. Ported verbatim from freedv-gui freedv_reporter.cpp
// :398-413 [@77e793a].
const char* const kColumnDisplayNames[kFreeDVReporterColumnCount] = {
    "Callsign",
    "Locator",
    "Distance",
    "Heading",
    "Version",
    "Frequency",
    "TX Mode",
    "Status",
    "User Message",
    "Last TX Date",
    "Last RX Callsign",
    "Last RX Mode",
    "SNR",
    "Last Update",
};

// Comparison-operator labels for the Filter submenus. From
// freedv-gui freedv_reporter.cpp:1729-1736 [@77e793a]
//   (opItems[] array). NereusSDR keeps the same ordering and labels.
struct OperatorEntry {
    int op;
    const char* label;
};
const OperatorEntry kFilterOperators[] = {
    {kFilterGte, ">= (Greater or equal to)"},
    {kFilterGt,  ">  (Greater than)"},
    {kFilterEq,  "=  (Equal to)"},
    {kFilterNeq, "!= (Not equal to)"},
    {kFilterLt,  "<  (Less than)"},
    {kFilterLte, "<= (Less or equal to)"},
};

}  // namespace

void FreeDVReporterDialog::buildMenuBar() {
    // -----------------------------------------------------------------
    // Show menu - 14 checkable actions, one per column.
    //
    // From freedv-gui freedv_reporter.cpp:395-425 [@77e793a]
    //   (showMenu_ wxMenu + per-column wxITEM_CHECK Append calls).
    // -----------------------------------------------------------------
    m_showMenu = m_menuBar->addMenu(QStringLiteral("Show"));
    m_showActions.clear();
    auto* showGroup = new QActionGroup(this);
    showGroup->setExclusive(false);
    for (int col = 0; col < kFreeDVReporterColumnCount; ++col) {
        auto* action = m_showMenu->addAction(
            QString::fromLatin1(kColumnDisplayNames[col]));
        action->setCheckable(true);
        action->setChecked(true);
        action->setData(col);
        showGroup->addAction(action);
        m_showActions.append(action);
    }
    connect(showGroup, &QActionGroup::triggered,
            this, &FreeDVReporterDialog::onShowColumnToggled);

    // -----------------------------------------------------------------
    // Filter menu - 14 submenus, one per column. Each submenu carries 6
    //   operator actions + a Clear filter action. Mirrors the upstream
    //   right-click popup at freedv_reporter.cpp:1710-1775 [@77e793a],
    //   but exposed from the menu bar too.
    // -----------------------------------------------------------------
    m_filterMenu = m_menuBar->addMenu(QStringLiteral("Filter"));
    m_filterSubmenus.clear();
    for (int col = 0; col < kFreeDVReporterColumnCount; ++col) {
        auto* sub = m_filterMenu->addMenu(
            QString::fromLatin1(kColumnDisplayNames[col]));
        sub->menuAction()->setData(col);
        m_filterSubmenus.append(sub);
        rebuildColumnFilterSubmenu(col);
    }

    // -----------------------------------------------------------------
    // Idle longer than menu - 4 exclusive actions. From task spec.
    //   30 min / 1 hour / 2 hours / Never. Default is 2 hours per
    //   upstream's freedvReporterMaxIdleMinutes default of 120
    //   (ReportingConfiguration.cpp [@77e793a]).
    // -----------------------------------------------------------------
    m_idleMenu = m_menuBar->addMenu(QStringLiteral("Idle longer than"));
    m_idleGroup = new QActionGroup(this);
    m_idleGroup->setExclusive(true);

    struct IdleEntry { int minutes; const char* label; };
    const IdleEntry kIdleEntries[] = {
        {30,  "30 minutes"},
        {60,  "1 hour"},
        {120, "2 hours"},
        {0,   "Never"},
    };

    // Load saved threshold (default 120 minutes per task spec).
    const int savedMinutes = AppSettings::instance()
        .value(QStringLiteral("FreeDvReporter/IdleTimeoutMinutes"),
               QStringLiteral("120")).toString().toInt();
    m_idleTimeoutMinutes = savedMinutes;

    for (const IdleEntry& e : kIdleEntries) {
        auto* action = m_idleMenu->addAction(QString::fromLatin1(e.label));
        action->setCheckable(true);
        action->setData(e.minutes);
        if (e.minutes == m_idleTimeoutMinutes) {
            action->setChecked(true);
        }
        m_idleGroup->addAction(action);
    }
    // If no entry matched (e.g. a third-party-edited settings file
    // contains 45), force the closest match to be the default 2-hour
    // bucket and rewrite the persisted value.
    if (m_idleGroup->checkedAction() == nullptr) {
        for (QAction* a : m_idleGroup->actions()) {
            if (a->data().toInt() == 120) {
                a->setChecked(true);
                m_idleTimeoutMinutes = 120;
                break;
            }
        }
    }
    connect(m_idleGroup, &QActionGroup::triggered,
            this, &FreeDVReporterDialog::onIdleLongerThanTriggered);
}

void FreeDVReporterDialog::rebuildColumnFilterSubmenu(int column) {
    // Re-populate the per-column submenu. Called once at construction
    // for every column, then re-called whenever the active filter for
    // that column changes (so the checked operator stays current).
    if (column < 0 || column >= m_filterSubmenus.size()) {
        return;
    }
    QMenu* sub = m_filterSubmenus.at(column);
    sub->clear();

    int currentOp = kFilterNone;
    if (m_columnFilters.contains(column)) {
        currentOp = m_columnFilters.value(column).first;
    }

    // From freedv-gui freedv_reporter.cpp:1738-1759 [@77e793a]
    //   (opItems for-loop building a check-style menu item per operator).
    for (const OperatorEntry& e : kFilterOperators) {
        auto* opAction = sub->addAction(QString::fromLatin1(e.label));
        opAction->setCheckable(true);
        opAction->setChecked(currentOp == e.op);
        // Capture by value: column index + operator code.
        const int col = column;
        const int op = e.op;
        connect(opAction, &QAction::triggered, this,
                [this, col, op]() { promptForColumnFilterValue(col, op); });
    }

    sub->addSeparator();
    auto* clearAction = sub->addAction(QStringLiteral("Clear filter"));
    clearAction->setEnabled(currentOp != kFilterNone);
    // From freedv-gui freedv_reporter.cpp:1762-1771 [@77e793a]
    //   (clearItem Bind clearing the column filter).
    connect(clearAction, &QAction::triggered, this,
            [this, column]() { clearColumnFilter(column); });
}

void FreeDVReporterDialog::promptForColumnFilterValue(int column, int op) {
    // From freedv-gui freedv_reporter.cpp:1747-1758 [@77e793a]
    //   (wxTextEntryDialog prompts for the comparison value).
    QString existingText;
    if (m_columnFilters.contains(column)) {
        existingText = m_columnFilters.value(column).second.toString();
    }
    const QString label = QStringLiteral("Filter %1: enter value")
        .arg(QString::fromLatin1(kColumnDisplayNames[column]));
    bool ok = false;
    const QString text = QInputDialog::getText(
        this,
        QStringLiteral("Set Column Filter"),
        label,
        QLineEdit::Normal,
        existingText,
        &ok);
    if (!ok) {
        // User cancelled; do not modify the filter map. Re-build the
        // submenu so the check stamp drops back to whatever was active
        // before (the user's click checked the action visually until
        // this point).
        rebuildColumnFilterSubmenu(column);
        return;
    }
    setColumnFilter(column, op, text);
}

void FreeDVReporterDialog::setColumnFilter(int column, int op,
                                           const QVariant& value) {
    if (op == kFilterNone) {
        clearColumnFilter(column);
        return;
    }
    m_columnFilters.insert(column, qMakePair(op, value));
    if (m_columnFilterProxy) {
        m_columnFilterProxy->refresh();
    }
    rebuildColumnFilterSubmenu(column);
    persistColumnFilters();
}

void FreeDVReporterDialog::clearColumnFilter(int column) {
    m_columnFilters.remove(column);
    if (m_columnFilterProxy) {
        m_columnFilterProxy->refresh();
    }
    rebuildColumnFilterSubmenu(column);
    persistColumnFilters();
}

void FreeDVReporterDialog::applyVisibleColumnsFromSettings() {
    // From the task spec: 14-bit bitmask under
    //   AppSettings/FreeDvReporter/VisibleColumns. Default 0x3FFF
    //   (all 14 bits set) to match upstream's default of every
    //   column visible (freedv_reporter.cpp:251-255 [@77e793a]).
    const QString raw = AppSettings::instance()
        .value(QStringLiteral("FreeDvReporter/VisibleColumns"),
               QStringLiteral("16383")).toString();
    bool ok = false;
    int mask = raw.toInt(&ok);
    if (!ok) {
        mask = 0x3FFF;
    }
    for (int col = 0; col < kFreeDVReporterColumnCount; ++col) {
        const bool visible = (mask & (1 << col)) != 0;
        if (col < m_showActions.size()) {
            m_showActions.at(col)->setChecked(visible);
        }
        if (m_table) {
            m_table->setColumnHidden(col, !visible);
        }
    }
}

void FreeDVReporterDialog::persistVisibleColumns() {
    int mask = 0;
    for (int col = 0; col < m_showActions.size(); ++col) {
        if (m_showActions.at(col)->isChecked()) {
            mask |= (1 << col);
        }
    }
    AppSettings::instance().setValue(
        QStringLiteral("FreeDvReporter/VisibleColumns"),
        QString::number(mask));
}

void FreeDVReporterDialog::persistColumnFilters() {
    // Encode as "<col>:<op>:<value-base64>\n..." so commas / colons /
    //   newlines in the value text round-trip safely. Future read path
    //   in loadColumnFiltersFromSettings() rebuilds the QHash.
    QStringList encoded;
    for (auto it = m_columnFilters.constBegin();
         it != m_columnFilters.constEnd(); ++it) {
        const int col = it.key();
        const int op = it.value().first;
        const QString val = it.value().second.toString();
        const QString valB64 = QString::fromLatin1(
            val.toUtf8().toBase64());
        encoded.append(QStringLiteral("%1:%2:%3").arg(col).arg(op).arg(valB64));
    }
    AppSettings::instance().setValue(
        QStringLiteral("FreeDvReporter/ColumnFilters"),
        encoded.join(QLatin1Char('\n')));
}

void FreeDVReporterDialog::loadColumnFiltersFromSettings() {
    const QString raw = AppSettings::instance()
        .value(QStringLiteral("FreeDvReporter/ColumnFilters"),
               QString()).toString();
    if (raw.isEmpty()) {
        return;
    }
    const QStringList rows = raw.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& row : rows) {
        const QStringList parts = row.split(QLatin1Char(':'));
        if (parts.size() < 3) {
            continue;
        }
        bool okCol = false;
        bool okOp = false;
        const int col = parts.at(0).toInt(&okCol);
        const int op = parts.at(1).toInt(&okOp);
        if (!okCol || !okOp) {
            continue;
        }
        const QByteArray valBytes = QByteArray::fromBase64(
            parts.at(2).toLatin1());
        const QString val = QString::fromUtf8(valBytes);
        m_columnFilters.insert(col, qMakePair(op, QVariant(val)));
        rebuildColumnFilterSubmenu(col);
    }
    if (m_columnFilterProxy) {
        m_columnFilterProxy->refresh();
    }
}

// =====================================================================
// G3: slot wiring
// =====================================================================

void FreeDVReporterDialog::onShowColumnToggled(QAction* action) {
    if (!action || !m_table) {
        return;
    }
    const int col = action->data().toInt();
    const bool visible = action->isChecked();
    m_table->setColumnHidden(col, !visible);
    persistVisibleColumns();
}

void FreeDVReporterDialog::onIdleLongerThanTriggered(QAction* action) {
    if (!action) {
        return;
    }
    const int minutes = action->data().toInt();
    m_idleTimeoutMinutes = minutes;
    AppSettings::instance().setValue(
        QStringLiteral("FreeDvReporter/IdleTimeoutMinutes"),
        QString::number(minutes));
    // Trigger an immediate sweep so the new threshold takes effect
    // without waiting for the next timer tick.
    onIdleSweepTick();
}

void FreeDVReporterDialog::onColumnHeaderRightClicked(const QPoint& pos) {
    if (!m_table) {
        return;
    }
    auto* header = m_table->horizontalHeader();
    if (!header) {
        return;
    }
    const int section = header->logicalIndexAt(pos);
    if (section < 0 || section >= kFreeDVReporterColumnCount) {
        return;
    }
    // Open the corresponding Filter submenu at the click position.
    //   From freedv-gui freedv_reporter.cpp:1710-1775 [@77e793a]
    //   (OnColumnHeaderRightClick builds a wxMenu inline and Popups it).
    if (section < m_filterSubmenus.size()) {
        QMenu* sub = m_filterSubmenus.at(section);
        sub->popup(header->mapToGlobal(pos));
    }
}

void FreeDVReporterDialog::onRowContextMenuRequested(const QPoint& pos) {
    if (!m_table) {
        return;
    }
    const QModelIndex idx = m_table->indexAt(pos);
    if (!idx.isValid()) {
        return;
    }
    QMenu* menu = buildRowContextMenuForTest(idx.row());
    if (!menu) {
        return;
    }
    // popup auto-deletes via the WA_DeleteOnClose attribute set in
    //   buildRowContextMenuForTest.
    menu->popup(m_table->viewport()->mapToGlobal(pos));
}

void FreeDVReporterDialog::onRowDoubleClicked(const QModelIndex& proxyIdx) {
    if (!proxyIdx.isValid() || !m_table) {
        return;
    }
    // Resolve the frequency column on the same proxy row.
    const QModelIndex freqIdx = m_table->model()->index(
        proxyIdx.row(), kFrequencyCol);
    const quint64 freqHz = freqIdx.data(Qt::EditRole).toULongLong();
    if (freqHz == 0) {
        return;
    }
    // From freedv-gui freedv_reporter.cpp:1463-1477 [@77e793a]
    //   (OnItemDoubleClick -> rigFrequencyController->setFrequency).
    //   NereusSDR emits tuneRequested instead of QSY broadcast - the
    //   task spec calls out that double-click is a local tune only.
    emit tuneRequested(freqHz);
}

void FreeDVReporterDialog::onIdleSweepTick() {
    if (!m_stationModel || m_idleTimeoutMinutes <= 0) {
        return;
    }
    // From freedv-gui freedv_reporter.cpp:2581-2601 [@77e793a]
    //   (isFiltered_ idle-time check). NereusSDR walks the model's
    //   QHash<sid, FreeDVStation> snapshot and calls onStationRemoved
    //   for each station that has not updated within the threshold;
    //   that path mirrors the wire-level remove_connection event.
    const QHash<QString, FreeDVStation> stations = m_stationModel->stations();
    const QDateTime cutoff = QDateTime::currentDateTime()
        .addSecs(-static_cast<qint64>(m_idleTimeoutMinutes) * 60);
    QStringList toRemove;
    for (auto it = stations.constBegin(); it != stations.constEnd(); ++it) {
        const FreeDVStation& s = it.value();
        // Pick the most recent activity timestamp.
        //   Upstream uses lastTxDate if valid, else connectTime
        //   (freedv_reporter.cpp:2590-2597 [@77e793a]). NereusSDR uses
        //   lastUpdate because that's what the FreeDVReporterClient
        //   stamps on every event (D2 client port).
        const QDateTime ref = s.lastUpdate.isValid()
            ? s.lastUpdate
            : s.connectTime;
        if (ref.isValid() && ref < cutoff) {
            toRemove.append(it.key());
        }
    }
    for (const QString& sid : toRemove) {
        m_stationModel->onStationRemoved(sid);
    }
}

// =====================================================================
// G3: test seams
// =====================================================================

void FreeDVReporterDialog::setColumnFilterForTest(int column, int op,
                                                   const QVariant& value) {
    setColumnFilter(column, op, value);
}

void FreeDVReporterDialog::clearColumnFilterForTest(int column) {
    clearColumnFilter(column);
}

void FreeDVReporterDialog::setIdleTimeoutMinutesForTest(int minutes) {
    m_idleTimeoutMinutes = minutes;
    if (m_idleGroup) {
        for (QAction* a : m_idleGroup->actions()) {
            a->setChecked(a->data().toInt() == minutes);
        }
    }
}

void FreeDVReporterDialog::setIdleSweepIntervalMsForTest(int ms) {
    m_idleSweepIntervalMs = ms;
    if (m_idleTimer) {
        m_idleTimer->stop();
        m_idleTimer->start(m_idleSweepIntervalMs);
    }
}

QMenu* FreeDVReporterDialog::buildRowContextMenuForTest(int proxyRow) {
    if (!m_table || proxyRow < 0) {
        return nullptr;
    }
    auto* model = m_table->model();
    if (!model || proxyRow >= model->rowCount()) {
        return nullptr;
    }
    const QString callsign = model->index(proxyRow, kCallsignCol)
        .data(Qt::DisplayRole).toString();
    const QString locator = model->index(proxyRow, kGridSquareCol)
        .data(Qt::DisplayRole).toString();
    const QString sid = model->index(proxyRow, 0).data(Qt::UserRole).toString();
    const quint64 freqHz = model->index(proxyRow, kFrequencyCol)
        .data(Qt::EditRole).toULongLong();

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    // From freedv-gui freedv_reporter.cpp:579-602 [@77e793a]
    //   (callsignPopupMenu_ with QRZ / HamQTH / HamCall lookup items).
    auto* qrzAction = menu->addAction(QStringLiteral("Look up on QRZ.com"));
    connect(qrzAction, &QAction::triggered, this, [callsign]() {
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://www.qrz.com/db/%1").arg(callsign)));
    });

    auto* hamQthAction = menu->addAction(QStringLiteral("Look up on HamQTH"));
    connect(hamQthAction, &QAction::triggered, this, [callsign]() {
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://www.hamqth.com/%1").arg(callsign)));
    });

    auto* hamCallAction = menu->addAction(QStringLiteral("Look up on HamCall"));
    connect(hamCallAction, &QAction::triggered, this, [callsign]() {
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://hamcall.net/call?callsign=%1")
                     .arg(callsign)));
    });

    menu->addSeparator();

    auto* copyCallAction = menu->addAction(QStringLiteral("Copy callsign"));
    connect(copyCallAction, &QAction::triggered, this, [callsign]() {
        QGuiApplication::clipboard()->setText(callsign);
    });

    auto* copyLocatorAction = menu->addAction(QStringLiteral("Copy locator"));
    connect(copyLocatorAction, &QAction::triggered, this, [locator]() {
        QGuiApplication::clipboard()->setText(locator);
    });

    menu->addSeparator();

    auto* qsyAction = menu->addAction(QStringLiteral("Send QSY to this station"));
    qsyAction->setEnabled(!sid.isEmpty() && freqHz > 0);
    connect(qsyAction, &QAction::triggered, this, [this, sid, freqHz]() {
        emit qsyRequested(sid, freqHz, QString());
    });

    return menu;
}

QString FreeDVReporterDialog::qrzUrlForCallsignForTest(
    const QString& callsign) const {
    // From freedv-gui freedv_reporter.cpp:1846-1851 [@77e793a]
    //   (OnQRZLookup formats "https://www.qrz.com/db/%s").
    return QStringLiteral("https://www.qrz.com/db/%1").arg(callsign);
}

}  // namespace NereusSDR
