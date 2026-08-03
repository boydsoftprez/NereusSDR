// no-port-check: AetherSDR-derived NereusSDR file. Painted pan-layout
// preview tile, structurally from AetherSDR PanLayoutDialog.cpp
// LayoutThumbnail [@c6481cb]. Registered in
// docs/attribution/aethersdr-contributor-index.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/widgets/LayoutThumbnail.h  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR PanLayoutDialog.cpp [@c6481cb].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// AetherSDR carries no per-file headers, so this is a project-level
// citation per docs/attribution/HOW-TO-PORT.md rule 6.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02  J.J. Boyd / KG4VCF  Bottom-banner + pan-menu epic.
//                                    Nine layouts rather than
//                                    AetherSDR's twelve; see design
//                                    §8.3. AI-assisted transformation
//                                    via Anthropic Claude Code.
// =================================================================
#pragma once

#include <QRect>
#include <QString>
#include <QVector>
#include <QWidget>

namespace NereusSDR {

/// One pan-layout arrangement. rows holds the column count per row, so
/// {2, 1} is two pans over one.
struct PanLayoutGeometry {
    QString      id;
    QString      label;
    int          panCount{0};
    QVector<int> rows;
};

/// Every layout NereusSDR ships, Single first.
///
/// AetherSDR ships twelve, up to eight pans. The three largest are omitted
/// because BoardCapabilities caps maxSlices at five on every supported
/// board, so those tiles could never light up. That is a client-side
/// allocation limit and not a hardware one: the gateware does eight
/// receivers (n1gp-Anvelina_PROIII Orion.v:958 [@8e86a61], NR = 8).
extern const QVector<PanLayoutGeometry> kPanLayouts;

/// Paints one layout's cell geometry with lettered pans, so an operator
/// recognises the arrangement instead of decoding an id string.
class LayoutThumbnail : public QWidget {
    Q_OBJECT

public:
    LayoutThumbnail(const PanLayoutGeometry& layout, bool isCurrent,
                    bool enabled, QWidget* parent = nullptr);

    /// Cell rectangles in paint order, A first. Exposed for testing.
    QVector<QRect> cellRects() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    PanLayoutGeometry m_layout;
    bool              m_current{false};
    bool              m_enabled{true};
};

} // namespace NereusSDR
