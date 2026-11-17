#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/roundtable/RoundtableTagLayout.h  (NereusSDR)
// =================================================================
//
// Roundtable on the panadapter and the VFO flag (design 11.2, U4, U7, U8,
// U10; mockups placement.html and the tag and flag parts of board.html).
//
// Panadapter tags: one per room in the Roundtable strip under the frequency
// scale, each the state glyph, the talker's callsign (or Unknown, Double,
// Quiet) and the member count. layoutRoundtableTags() is the pure part: where each tag
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
//   2026-11-16: The details open over the waterfall, under the strip:
//               placeRoundtableDetails, RoundtableDetailsPlacement,
//               kRoundtableDetailsMinBelow. placeRoundtableInspector and
//               kRoundtableInspectorMinStretch are gone. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-11-16: Your frequency's line in the strip replaces the card on the
//               spectrum: placeRoundtableStripLine, paintRoundtableStripLine,
//               roundtableStripLineParts. placeRoundtableCard, paintRoundtableCard
//               and RoundtableCardPlacement are gone. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-11-16: The Roundtable strip between the frequency scale and the
//               waterfall: kRoundtableStripHeight, roundtableStripShownFor.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-11-03: The view of another frequency: RoundtableViewModel,
//               paintRoundtableView and its text (overlay design D22, D23).
//               RoundtableCardModel::onFrequency, RoundtableCardRow::label.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-11-03: The live card and tags on the pan: placeRoundtableCard,
//               paintRoundtableCard, roundtableMergeDoubleTags (overlay design
//               D10, D11, D13 to D16, D19, D20). The text-row card path and the
//               Stale display are gone. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-11-03: Overlay redesign truth rules and structured card model:
//               roundtableIsDouble, roundtableCardModelFor,
//               RoundtablePeakTracker, roundtableMeterFraction and the Frozen
//               display (design D1 to D9, D12, D13, D19). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-11-02: Expose passive card rows and current overlap participants.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
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
#include <QHash>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

class QPainter;
class QPaintDevice;
class QFontMetrics;

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
    QString label;           // the talker's callsign, You, Unknown, Double or Quiet
    int members = 0;
    bool mine = false;       // any of your slices' rooms
    bool active = false;     // selected listening context; independent of mine
    bool mentionsMe = false; // a message there names you
    QJsonArray stations;     // unique scoped people at this exact context
    QStringList keyed;       // who is on the air here; a Double lists each current participant
    QStringList reportLines; // the active frequency's report, as plain text
    QList<qint64> mergedHz;  // other exact frequencies roundtableMergeDoubleTags folded into this tag
    double width = 0.0;      // how wide it draws, to its own text; 0 takes the layout's tagWidth
};

/// The shared Roundtable panel/tag S-unit conversion.
NEREUS_GUI_EXPORT QString roundtableSUnitText(double dbm);

/// Supplied participants, using explicit current member key states for a
/// live double; ended overlaps retain their historical participants.
NEREUS_GUI_EXPORT QJsonArray roundtableTurnParticipants(const QJsonObject& view, const QJsonObject& turn,
    bool* identitiesComplete = nullptr);

/// Ignore numeric samples, ages and path measurements when deciding
/// whether a room change requires immediate structural presentation.
NEREUS_GUI_EXPORT QString roundtableRoomStructureSignature(QJsonObject room);

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
/// `leftHz`, at `hzPerPixel`. Each tag is its own `width` wide, or `tagWidth`
/// when that is 0; a fold is `tagWidth` wide. At least `gap` lies between
/// neighbors and from either edge. Tags whose
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

/// How a pan shows its tags (design 11.1, U4): connecting hides them. Dimmed is
/// live while you are hidden: the rooms and talkers as when live, the state
/// marking dimmed. Frozen is a dropped link, Reconnecting or Offline (overlay
/// design D7): last known, drawn grey, with no live color.
enum class RoundtableTagDisplay { Hidden, Live, Dimmed, Frozen };

/// One double across several exact frequencies in one source room is one tag
/// (overlay design D20). Doubled tags that share a source room and a keyed name
/// become the first of them, with the others' frequencies in mergedHz.
NEREUS_GUI_EXPORT QList<RoundtableTag> roundtableMergeDoubleTags(const QList<RoundtableTag>& tags);

/// The tags for every room in `model`'s band list. `sliceIds` are this
/// station's slices: their rooms are mine. Empty without a model.
NEREUS_GUI_EXPORT QList<RoundtableTag> roundtableTagsFor(const RoundtableModel* model,
                                                         const QList<int>& sliceIds, const QString& activeContext = {});
/// Hidden while Roundtable is off, unavailable or connecting; Frozen while
/// reconnecting or offline; Dimmed while you are hidden (you still see
/// everything, D21); Live otherwise.
NEREUS_GUI_EXPORT RoundtableTagDisplay roundtableTagDisplayFor(const RoundtableModel* model);

/// How tall the Roundtable strip is: the row between the frequency scale and
/// the waterfall that holds the card and the tags.
inline constexpr int kRoundtableStripHeight = 30;

/// Whether the pans show the Roundtable strip: Roundtable is available and its
/// link is anything but Off (Connecting, Reconnecting, Offline, Hidden and Live
/// all show it). False without a model.
NEREUS_GUI_EXPORT bool roundtableStripShownFor(const RoundtableModel* model);

/// Where a reading sits on a card's meter, the flag's S scale: -121 dBm is
/// empty and -33 dBm is full (overlay design D11).
NEREUS_GUI_EXPORT double roundtableMeterFraction(double dbm);

/// One listener's reading on the card (overlay design D11). In a double the
/// reading is combined RF, so km and bearingDeg stay empty (D2). A row past 20
/// seconds is old and has no peak tick (D13).
struct RoundtableCardRow {
    QString call;
    QString label;           // shown in place of the call: "Your pan"
    double dbm = 0.0;
    qint64 ageMs = 0;
    std::optional<int> km;
    std::optional<int> bearingDeg;
    int offsetHz = 0;
    bool you = false;
    bool old = false;
    double peakDbm = 0.0;
    bool hasPeak = false;
};

/// One station at a frequency, as a view of it lists them (overlay design D22).
struct RoundtableCardPerson {
    QString name;
    bool keyed = false;
    bool you = false;
    qint64 offsetHz = 0;  // the station's frequency less the viewed one
};

/// What the card for one frequency says.
struct RoundtableCardModel {
    enum class Kind { Talking, You, Double, Unknown, LastTalker, LastDouble, Quiet };
    Kind kind = Kind::Quiet;
    quint64 hz = 0;
    QStringList keyed;      // on the air, or every name for a LastDouble
    QString keyedYou;       // which of keyed is this station, in a double
    QStringList people;     // the stations at this exact frequency
    QVector<RoundtableCardPerson> onFrequency;  // the view's list, from the stream or else the directory
    qint64 clockMs = 0;     // how long a live turn has run; stops where a link dropped
    qint64 liveHz = 0;      // the live turn's exact frequency, a double's participant nearest the dial; 0 with none
    qint64 endedAgoMs = 0;  // how long ago a Last turn ended
    QVector<RoundtableCardRow> rows;
    bool frozen = false;    // Reconnecting or Offline (D7)
    QString note;           // the link's note, else "Double ended 3s ago" for 8 seconds
};

/// True when two cards differ only in values: readings, ages, the clock and the
/// words of a note. Such a change moves nothing and redraws in place (D14).
NEREUS_GUI_EXPORT bool roundtableCardSameStructure(const RoundtableCardModel& a, const RoundtableCardModel& b);

/// The inspector's width on the desktop pan (overlay design D34; strip design 5).
inline constexpr int kRoundtableInspectorWidth = 300;

/// How tall the fade over a cut body is: never more than half of it, so a short
/// body's rows stay readable.
NEREUS_GUI_EXPORT int roundtableViewFadeHeight(int bodyHeight);

/// The details' gap to the strip above and the waterfall's foot below.
inline constexpr int kRoundtableDetailsGap = 4;
/// Under this much room over the waterfall, the details may open upward.
inline constexpr int kRoundtableDetailsMinBelow = 140;

/// Where the details stand with the strip (design V4, V5). Pure. `below` is the
/// waterfall area the details may cover (already cut clear of the time strip on
/// its right edge); `above` is the spectrum from its top to the frequency
/// scale's top edge. `anchorLeft` is the left edge to line up with (your line's
/// left when it is drawn, else dialX + 8, or dialX - 8 - width when the flag
/// side says left). `height` is what the details want. They hang from
/// below.top() + 4, cut to below.bottom() - 4, 300 wide or the area's width if
/// narrower, left clamped into the area. When below has under 140 px of room
/// (below.height() - 8 < 140) and above has more room than below, they stand on
/// above's bottom edge instead, cut to above's height. `upward` reports which.
struct RoundtableDetailsPlacement {
    QRect rect;
    bool upward = false;
};
NEREUS_GUI_EXPORT RoundtableDetailsPlacement placeRoundtableDetails(
    const QRect& below, const QRect& above, int anchorLeft, int height);

/// A reading in the chosen format: "dBm", "S-units" or "Both".
NEREUS_GUI_EXPORT QString roundtableSignalText(double dbm, const QString& format);

/// The card's state color, for its border and the tick to the dial (D15).
NEREUS_GUI_EXPORT QColor roundtableCardColor(const RoundtableCardModel& card);

/// Where your frequency's line sits in the strip. line is null when the card
/// has nothing to show (Kind::Quiet with no note) or no side has room for the
/// head. readings is how many reading rows are drawn, moreReadings the rest.
struct RoundtableStripLinePlacement {
    QRect line;
    int readings = 0;
    int moreReadings = 0;
    bool operator==(const RoundtableStripLinePlacement&) const = default;
};

/// Pure (strip design 3, 4; the board's placeStrip). `strip` is the strip's
/// rect, `dialX` the active slice's dial, `flagFlipped` true when its flag hangs
/// left of the dial. The line is 24 px high, 3 px under the strip's top, beside
/// the dial on the flag's far side when it fits there or that side is the
/// roomier, else on the near side; readings drop from the end, behind
/// "+N more", until it fits. It depends only on the card's structure and the
/// widths of its texts in `device`'s fonts: each reading is its call's width
/// plus a value column as wide as the format's widest words (-121 dBm or
/// -33 dBm) and an age column for two digits and the unit, at least 78 px, and
/// the clock and a last talker's "Last heard" take their widest two digits, so
/// new readings, ages and clocks move nothing.
NEREUS_GUI_EXPORT RoundtableStripLinePlacement placeRoundtableStripLine(
    const RoundtableCardModel& card, const QRect& strip, int dialX, bool flagFlipped,
    const QString& format, const QPaintDevice* device);

/// Draws the line where `placed` says: the rail, the head, the clock, each
/// reading with its meter, then "+N more". `dimmed` is true while you are
/// hidden; `open` while the inspector shows your own frequency.
NEREUS_GUI_EXPORT void paintRoundtableStripLine(
    QPainter& p, const RoundtableCardModel& card, const RoundtableStripLinePlacement& placed,
    const QString& format, bool dimmed, bool open);

/// Where paintRoundtableStripLine draws each piece of a placed line, in the
/// line's coordinates. Null rects for what the line does not show. Each reading
/// is its text row and the meter under it; the meter is its bottom 3 px.
struct RoundtableStripLineParts {
    QRect head;               // the glyph and who
    QVector<QRect> names;     // a double's names, side by side
    QRect clock;
    QRect note;               // the link's note, a last talker's ended note
    QVector<QRect> readings;
    QRect more;               // "+N more"
};
NEREUS_GUI_EXPORT RoundtableStripLineParts roundtableStripLineParts(
    const RoundtableCardModel& card, const RoundtableStripLinePlacement& placed, const QString& format,
    const QPaintDevice* device);

/// Fills `rect` with `on`/`off` stripes at 135 degrees, as the board's
/// repeating gradients: `onWidth` and `offWidth` along the gradient, in pixels.
NEREUS_GUI_EXPORT void paintRoundtableStripes(QPainter& p, const QRectF& rect, const QColor& on, const QColor& off,
                                              qreal onWidth, qreal offWidth);

/// What the inspector shows (overlay design D22, D23, D35).
struct RoundtableViewModel {
    RoundtableCardModel card;
    bool mine = false;       // your own frequency: no Viewing line, no Your pan row
    QString family;          // of the viewed frequency
    quint64 sliceHz = 0;     // the active slice
    QString sliceFamily;
    QString sliceName = QStringLiteral("A");
    QString format = QStringLiteral("dBm");
    bool miles = true;
    bool bearing = true;
    std::optional<double> panDbm;  // this client's own pan; empty when it has no level
    double panPeakDbm = 0.0;
};

/// The view's head: the card's words, with "Nobody talking" when no turn is open
/// (the view's own wording, from the board's peekBody).
NEREUS_GUI_EXPORT QString roundtableViewHeadText(const RoundtableCardModel& card);
/// The same for the whole view: your own frequency keeps its last talker's name.
NEREUS_GUI_EXPORT QString roundtableViewHeadText(const RoundtableViewModel& view);
/// "‹ Back to my slice".
NEREUS_GUI_EXPORT QString roundtableViewBackText();
/// "Viewing · −11.4 kHz", or "Viewing" with no distance to give. Empty on your own
/// frequency.
NEREUS_GUI_EXPORT QString roundtableViewSubText(const RoundtableViewModel& view);
/// "Reconnecting · last known", "Offline since HH:MM UTC · last known", or the
/// card's other note.
NEREUS_GUI_EXPORT QString roundtableViewLinkText(const RoundtableCardModel& card);
/// A station's offset from the viewed frequency: "on freq" or "+20 Hz".
NEREUS_GUI_EXPORT QString roundtableViewOffsetText(qint64 offsetHz);
/// "1,240 mi 087°"; empty when the row has no path (D2 blanks it in a double).
NEREUS_GUI_EXPORT QString roundtableViewPathText(const RoundtableCardRow& row, bool miles, bool bearing);
/// The readings as the view lists them: Your pan first while someone is on the
/// air, then the card's rows.
NEREUS_GUI_EXPORT QVector<RoundtableCardRow> roundtableViewRows(const RoundtableViewModel& view);
/// How the view's words stack, from the rect's top, up to where its buttons go.
struct RoundtableViewLayout {
    int head = 0;        // what stays put, ending where the body starts
    int total = 0;       // the head and the whole body
    int viewingTop = -1; // the Viewing line's top, -1 without one
    int backWidth = 0;   // what Back's words need
    int body() const { return total - head; }
};
/// Measures the view. `foldPeople` folds everyone not keyed to one "Also here" line
/// (overlay design D19). The same walk as paintRoundtableView.
NEREUS_GUI_EXPORT RoundtableViewLayout roundtableViewLayout(const RoundtableViewModel& view, int width, bool foldPeople,
                                                            const QPaintDevice* device);
/// Draws the frame and the words of the view in `rect` and returns the same layout.
/// The body moves up by `scrollY` and is cut `bodyHeight` below the head, fading
/// while more lies below; a negative `bodyHeight` draws it whole.
NEREUS_GUI_EXPORT RoundtableViewLayout paintRoundtableView(QPainter& p, const RoundtableViewModel& view, const QRect& rect,
                                                           bool foldPeople = false, int scrollY = 0, int bodyHeight = -1);

/// One row of a fold's list in the inspector (overlay design D37).
struct RoundtableFoldRow {
    QString roomId;
    qint64 hz = 0;
    int state = 0;        // a RoundtableGlyph
    QString who;          // "VE3XYZ", or "Double · AA5ABC + K0ABC"
    bool frozen = false;  // a dropped link draws no live color (D7)
    bool dimmed = false;  // you are hidden
    bool operator==(const RoundtableFoldRow&) const = default;
};
/// How tall the list wants to be, head and rows.
NEREUS_GUI_EXPORT int roundtableFoldListHeight(int rows);
/// Where the rows start, under the fixed head.
NEREUS_GUI_EXPORT int roundtableFoldListBodyTop();
/// The row under `pos`, in `rect`'s coordinates, or -1.
NEREUS_GUI_EXPORT int roundtableFoldListRowAt(const QPoint& pos, const QRect& rect, int rows, int scrollY);
/// Draws "N frequencies" and its rows in `rect` (overlay.js renderIns, .ins-list).
NEREUS_GUI_EXPORT void paintRoundtableFoldList(QPainter& p, const QVector<RoundtableFoldRow>& rows, const QRect& rect,
                                               int hoverRow, int scrollY);

/// The held peak for each listener (overlay design D12). Client side only:
/// never persisted or sent. It rises at once, falls 2 dB a second and resets on
/// a new turn id, per source room and listener. The caller owns it.
class NEREUS_GUI_EXPORT RoundtablePeakTracker {
public:
    /// Takes a reading and returns the peak to show.
    double update(const QString& sourceRoom, const QString& ref, qint64 turnId, double dbm, qint64 nowMs);

private:
    struct Held {
        qint64 turnId = 0;
        double peakDbm = 0.0;
        qint64 atMs = 0;
    };
    QHash<QString, QHash<QString, Held>> m_rooms;
    int m_entries = 0;
};

/// Whether a frequency is a Double right now (overlay design D1). With a view
/// of that frequency, only when the turn it shows (D3) is an open doubled turn.
/// With no view, or one whose stream has not arrived, known only from `rooms`
/// (the directory's summaries): the source rooms of the stations at exactly `hz`
/// in `family` decide. Two or more of their keyed stations within 5 kHz, or a
/// summary whose state is doubled, which holds with one participant left.
NEREUS_GUI_EXPORT bool roundtableIsDouble(const QJsonObject& view, const QJsonArray& rooms, qint64 hz,
                                          const QString& family);

/// Whether a Tune to `hz` in `family` would do nothing for a slice at `sliceHz` in
/// `sliceFamily` (D22): already there, and no mode to write. A FreeDV view writes
/// no mode, so it counts as there from any family on the same frequency.
NEREUS_GUI_EXPORT bool roundtableTuneWritesNothing(qint64 hz, const QString& family, qint64 sliceHz,
                                                   const QString& sliceFamily);

/// The card for frequency `hz`, read from `model`'s view of `activeContext`
/// (with no context, the room of the first of `sliceIds`) and its directory.
/// `peaks` may be null: rows then have no peak.
NEREUS_GUI_EXPORT RoundtableCardModel roundtableCardModelFor(const RoundtableModel* model, const QList<int>& sliceIds,
                                                             const QString& activeContext, quint64 hz,
                                                             RoundtablePeakTracker* peaks);

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
