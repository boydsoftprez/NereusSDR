#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/roundtable/RoundtableTagLayout.h  (NereusSDR)
// =================================================================
//
// Roundtable on the panadapter and the VFO flag (design 11.2, U4, U7, U8,
// U10; mockups placement.html and the tag and flag parts of board.html).
//
// Panadapter tags: one per room along the foot of the spectrum, each the
// state glyph, the talker's callsign (or Unknown, Doubled, Quiet) and the
// member count. layoutRoundtableTags() is the pure part: where each tag
// goes across the pan and which ones fold.
//
//   - Tags that would overlap form a group. A group's tags that can sit
//     side by side, each within one tag's width of its own frequency,
//     move aside; otherwise the group's tags fold into one +N.
//   - A fold holds two or more. A lone neighbor only moves aside.
//   - Your slice's room (mine) and rooms whose messages name you
//     (mentionsMe) never fold.
//   - Folds widen only if the tags still don't fit across the pan: then
//     every overlapping group folds, and a group reaches further each
//     round.
//   - Zooming in (a smaller hzPerPixel) spreads the tags, so folds open by
//     themselves; there is no zoom setting.
//
// The VFO flag's Roundtable line: "Roundtable · 9 · K4ABC" with the state
// glyph, @N when a message names you, "9 hear you" while you transmit, and
// the link state when Roundtable is not live. roundtableFlagLineFor()
// builds it from the model; the flag only draws it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 14).
// =================================================================

#include "gui/NereusGuiExport.h"

#include <QColor>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>

class QPainter;

namespace NereusSDR {

class RoundtableModel;

/// The state glyphs (U7: shapes as well as colors; U8: you are cyan).
/// RoundtableTag::state holds one of these as an int.
enum class RoundtableGlyph : int {
    Quiet = 0,    // hollow ring
    Talking = 1,  // filled green dot
    Unknown = 2,  // amber ring with a dot
    Doubled = 3,  // striped red diamond
    You = 4,      // filled cyan dot
    Waiting = 5,  // an amber ring with a gap: connecting or reconnecting
    Offline = 6,  // a red ring: offline
};

/// One room as a tag on the pan.
struct RoundtableTag {
    QString roomId;
    qint64 hz = 0;           // the room's center
    int state = 0;           // a RoundtableGlyph
    QString label;           // the talker's callsign, You, Unknown, Doubled or Quiet
    int members = 0;
    bool mine = false;       // your slice's room
    bool mentionsMe = false; // a message there names you
};

/// Where one tag, or one fold of tags, sits. rect's x and width are pan
/// pixels from the left edge of the spectrum; its y and height are the
/// caller's (the tag row). roomIds lists the rooms in it by frequency.
struct RoundtableTagPlacement {
    QRectF rect;
    QStringList roomIds;
    bool folded = false;
};

/// Lays `tags` out across a pan `width` pixels wide whose left edge is
/// `leftHz`, at `hzPerPixel`. Every tag and fold is `tagWidth` wide, with
/// at least `gap` between neighbors and from either edge. Tags whose
/// frequency is off the pan are left out. The placements are in order of
/// frequency.
NEREUS_GUI_EXPORT QList<RoundtableTagPlacement> layoutRoundtableTags(const QList<RoundtableTag>& tags,
                                                                     double hzPerPixel, qint64 leftHz,
                                                                     double width, double tagWidth,
                                                                     double gap);

/// How a pan shows its tags (design 11.1, U4): offline greys them as last
/// known, connecting hides them.
enum class RoundtableTagDisplay { Hidden, Live, Stale };

/// The tags for every room in `model`'s band list. `sliceIds` are this
/// station's slices: their rooms are mine. Empty without a model.
NEREUS_GUI_EXPORT QList<RoundtableTag> roundtableTagsFor(const RoundtableModel* model,
                                                         const QList<int>& sliceIds);
/// Hidden while Roundtable is off, unavailable or connecting; Stale while
/// offline; Live otherwise (live, reconnecting, or hidden, when you still
/// see everything).
NEREUS_GUI_EXPORT RoundtableTagDisplay roundtableTagDisplayFor(const RoundtableModel* model);

/// A room's frequency as the fold list shows it: MHz with three decimals,
/// more when the room is between kHz ("7.185", "7.0305").
NEREUS_GUI_EXPORT QString roundtableFrequencyText(qint64 hz);

} // namespace NereusSDR
