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
//   2026-10-30: Approved frequency-first Roundtable desktop surfaces.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 14).
//   2026-10-24: Roundtable plan Task 15: roundtableTurnIsYours(), whose
//               turn it is from the talkers and your own row, not from
//               `mine` (wire spec 6.9: the name you gave). J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
//   2026-10-25: while you are hidden, the flag line says Hidden in place of
//               Roundtable and the tags and the line's glyph are dimmed (RoundtableTagDisplay::Dimmed,
//               RoundtableFlagLine::dimmed, roundtableDimmedColor), JJ's
//               answer of 2026-10-25. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "gui/NereusGuiExport.h"

#include <QColor>
#include <QJsonObject>
#include <QJsonArray>
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
    QJsonArray stations;     // unique scoped people at this exact context
    QStringList reportLines; // matching last talker, attributed reporter rows, or Read reports
};

/// Compact RF rows for this exact cochannel. A neighborhood's unrelated turn
/// never labels this tag; reading.turn is required for every reported sample.
NEREUS_GUI_EXPORT QStringList roundtableTagReportLines(const RoundtableTag& tag, const QJsonObject& view,
                                                       qint64 nowMs);

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

/// A stretch of the pan, in pixels from its left edge, that no tag may
/// cover (a VFO flag hanging over the tag row).
struct RoundtableKeepOut {
    double left = 0.0;
    double right = 0.0;
};

/// As above, with the tags kept off `keepOut`: they sit side by side in the
/// stretches between, each in the one nearest its frequency that has room,
/// in order of frequency; whether they fit counts only those stretches.
NEREUS_GUI_EXPORT QList<RoundtableTagPlacement> layoutRoundtableTags(const QList<RoundtableTag>& tags,
                                                                     double hzPerPixel, qint64 leftHz,
                                                                     double width, double tagWidth,
                                                                     double gap,
                                                                     const QList<RoundtableKeepOut>& keepOut);

/// How a pan shows its tags (design 11.1, U4): offline greys them as last
/// known, connecting hides them. Dimmed is live while you are hidden: the
/// rooms and talkers as when live, the state marking dimmed.
enum class RoundtableTagDisplay { Hidden, Live, Stale, Dimmed };

/// The tags for every room in `model`'s band list. `sliceIds` are this
/// station's slices: their rooms are mine. Empty without a model.
NEREUS_GUI_EXPORT QList<RoundtableTag> roundtableTagsFor(const RoundtableModel* model,
                                                         const QList<int>& sliceIds);
/// Hidden while Roundtable is off, unavailable or connecting; Stale while
/// offline; Dimmed while you are hidden (you still see everything, D21);
/// Live otherwise (live or reconnecting).
NEREUS_GUI_EXPORT RoundtableTagDisplay roundtableTagDisplayFor(const RoundtableModel* model);

/// What a VFO flag's Roundtable line shows.
struct RoundtableFlagLine {
    bool visible = false;
    int glyph = 0;     // a RoundtableGlyph
    QString text;      // "Roundtable · 9 · K4ABC"
    int mentions = 0;  // @N, drawn in cyan after the text
    bool dimmed = false; // you are hidden: "Hidden · 9 · K4ABC", the glyph dimmed
    bool operator==(const RoundtableFlagLine& other) const
    {
        return visible == other.visible && glyph == other.glyph && text == other.text
            && mentions == other.mentions && dimmed == other.dimmed;
    }
    bool operator!=(const RoundtableFlagLine& other) const { return !(*this == other); }
};

/// Whether a non-doubled turn explicitly matches any own scoped endpoint.
/// Frequency views never infer ownership from a different keyed own slice.
/// A legacy single RF room retains its room-scoped keyed/open fallback.
/// A turn's `mine` is the name this station gave the turn (wire spec 6.9),
/// never whose turn it is.
NEREUS_GUI_EXPORT bool roundtableTurnIsYours(const QJsonObject& room, const QJsonObject& turn, bool open);

/// The line for slice `sliceId`'s flag (design 11.2): the room's member
/// count and who is talking, "N hear you" while you transmit there, the
/// link state while Roundtable is not live, and the count of messages
/// naming you. While you are hidden, the room as when live with Hidden in
/// place of Roundtable, dimmed. Not visible while Roundtable is off or not
/// running here.
NEREUS_GUI_EXPORT RoundtableFlagLine roundtableFlagLineFor(const RoundtableModel* model, int sliceId,
                                                                            qint64 hz = 0, const QString& family = {});

/// The state colors (design 11): talking green, unknown amber, doubled
/// red, you cyan, quiet grey; waiting amber, offline red.
NEREUS_GUI_EXPORT QColor roundtableGlyphColor(int glyph);
/// Grey for a tag shown as last known.
NEREUS_GUI_EXPORT QColor roundtableStaleColor();
/// How much of a dimmed marking shows (you are hidden).
inline constexpr qreal kRoundtableDimmedAlpha = 0.45;
/// A state's marking while you are hidden: the state's color as the
/// workspace look's role (ok, warn, danger, accent, text-secondary; the
/// fixed color with the look off) at kRoundtableDimmedAlpha.
NEREUS_GUI_EXPORT QColor roundtableDimmedColor(int glyph);

/// Draws glyph `glyph` centered on `center`, `size` pixels across; `grey`
/// draws it in the last-known grey, `dimmed` in roundtableDimmedColor().
NEREUS_GUI_EXPORT void paintRoundtableGlyph(QPainter& p, const QPointF& center, qreal size, int glyph,
                                            bool grey = false, bool dimmed = false);

/// A room's frequency as the fold list shows it: MHz with three decimals,
/// more when the room is between kHz ("7.185", "7.0305").
NEREUS_GUI_EXPORT QString roundtableFrequencyText(qint64 hz);

} // namespace NereusSDR
