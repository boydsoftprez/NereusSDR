// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - BandFilterProxy implementation. setBandVisible toggles
// QSet<QString> membership; filterAcceptsRow reads SpotTableModel::ColBand
// DisplayRole to compare against m_hiddenBands.
//
// Ported from AetherSDR src/gui/DxClusterDialog.cpp:208-226 [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task D2. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR". Implementation
//                                    extracted from
//                                    DxClusterDialog.cpp:208-226 verbatim:
//                                    setBandVisible (toggles QSet
//                                    membership and reinvalidates the
//                                    filter), filterAcceptsRow (empty
//                                    m_hiddenBands -> always accept;
//                                    looks up the band string via
//                                    SpotTableModel::ColBand
//                                    DisplayRole; empty band always
//                                    shows; otherwise membership
//                                    check). AI tooling: Anthropic
//                                    Claude Code.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task F3. NereusSDR-
//                                    native extension: source filter
//                                    mirrors the band filter. New
//                                    setSourceVisible() toggles
//                                    m_hiddenSources QSet membership
//                                    and calls invalidateFilter().
//                                    filterAcceptsRow now applies
//                                    both predicates with AND
//                                    semantics: row must pass band
//                                    AND source to be visible. Source
//                                    column lookup uses
//                                    SpotTableModel::ColSource
//                                    DisplayRole; empty source string
//                                    always shows (matches band
//                                    convention for unknown values).
//                                    AI tooling: Anthropic Claude
//                                    Code.

#include "BandFilterProxy.h"
#include "SpotTableModel.h"

namespace NereusSDR {

// From AetherSDR src/gui/DxClusterDialog.cpp:208-215 [@0cd4559]
void BandFilterProxy::setBandVisible(const QString& band, bool visible)
{
    if (visible)
        m_hiddenBands.remove(band);
    else
        m_hiddenBands.insert(band);
    invalidateFilterCompat();
}

// NereusSDR Task F3 extension. Mirrors setBandVisible but populates
// m_hiddenSources. The Spot List tab toggles this from 7 source pills
// (Cluster / RBN / WSJT-X / SpotCollector / POTA / FreeDV / PSK).
void BandFilterProxy::setSourceVisible(const QString& source, bool visible)
{
    if (visible)
        m_hiddenSources.remove(source);
    else
        m_hiddenSources.insert(source);
    invalidateFilterCompat();
}

// From AetherSDR src/gui/DxClusterDialog.cpp:217-226 [@0cd4559] +
// NereusSDR Task F3 source-filter extension. AetherSDR upstream
// only checks the band predicate; NereusSDR additionally applies the
// source predicate with AND semantics (a row must pass BOTH the band
// and source filters to be visible). Empty band / source strings
// always show (unknown values, matches upstream convention).
bool BandFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (!m_hiddenBands.isEmpty()) {
        auto idx = sourceModel()->index(sourceRow, SpotTableModel::ColBand, sourceParent);
        QString band = sourceModel()->data(idx, Qt::DisplayRole).toString();
        // unknown band - always show (upstream behaviour)
        if (!band.isEmpty() && m_hiddenBands.contains(band))
            return false;
    }
    if (!m_hiddenSources.isEmpty()) {
        auto idx = sourceModel()->index(sourceRow, SpotTableModel::ColSource, sourceParent);
        QString source = sourceModel()->data(idx, Qt::DisplayRole).toString();
        // unknown source - always show (mirrors band convention)
        if (!source.isEmpty() && m_hiddenSources.contains(source))
            return false;
    }
    return true;
}

} // namespace NereusSDR
