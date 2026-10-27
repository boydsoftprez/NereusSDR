// =================================================================
// src/gui/meters/ItemColourLook.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. The table names the Thetis default
// colours the container items already carry (each item's own cited
// defaults); no Thetis code is ported here.
//
// The workspace look for container and item colours (JJ, 2026-10-26: "on
// the containers looks, i want all the colors reworked into our workspace
// style"). One table maps a colour field at its default colour to a
// workspace palette role (WorkspacePalette.h). The mapping happens when a
// colour is used:
//
//   - the workspace look on (WorkspacePreview::enabled()) and the stored
//     colour equal to one of the field's defaults: the role's colour, with
//     the stored alpha;
//   - any other stored colour, or one the user picked in an editor even
//     when it equals a default (MeterItem::colourUserSet, asked of the
//     item drawing it and of the composite face holding that item): that
//     colour;
//   - the look off: the stored colour.
//
// Records, serialization and the editors' stored values never change, so
// a saved layout round-trips byte for byte and turning the look off brings
// the classic colours back. The picks are saved beside the record, in the
// entry context (kUserColoursKey), only when there is one. An item saved
// without picks (every layout saved before they were recorded) treats a
// colour equal to a default as the default. A field whose default has no matching role
// keeps its classic colour; its row says why.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Written by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-26 - Contrast round: derived() for colours an item derives
//                 (off-state text, badge fills). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - PR 677 review fix wave (I6): a colour the user picked
//                 stays as picked even when it equals a default
//                 (userPicked(), kUserColoursKey; surface arguments on
//                 followsLook, derived and paintSwatch); swatches carry an
//                 accessible name (GUI M5). J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QColor>
#include <QString>
#include <QVector>

class QObject;
class QPushButton;

namespace NereusSDR {

class MeterItem;

namespace ItemColourLook {

// One row of the table: a colour field at one of its defaults.
struct Row {
    // "<Item>.<field>", for example "VfoDisplay.frequencyColour". Composite
    // faces use "<kind>.<config key>" and "<kind>.<channel>.<key>".
    const char* field;
    // The default, as qRgba (a default with alpha keeps that alpha).
    QRgb classic;
    // The workspace palette role; nullptr keeps the classic colour.
    const char* role;
    // A background: with the look on it shows the background the user set
    // on the container around it (WorkspacePreview::dataBackground()),
    // otherwise the role.
    bool backdrop;
    // Why the classic colour is kept (role == nullptr), in plain words.
    const char* kept;
};

// Every row, in inventory order.
const QVector<Row>& rows();

// The row for `field` whose default is `stored`, or nullptr when `stored`
// is no default of the field (a colour the user picked, or an unknown
// field).
const Row* defaultRow(const QString& field, const QColor& stored);

// True when the table lists `stored` as a default of `field`, mapped or
// kept.
bool listed(const QString& field, const QColor& stored);

// True when `surface` is an item that holds `field` as a colour the user
// picked (MeterItem::colourUserSet), or sits in a composite face that
// does (a VFO display or clock inside a face).
bool userPicked(const QString& field, const QObject* surface);

// True when the look is on and `stored` is a mapped default of `field`
// that the user did not pick (`surface`, as for effective()): the item
// draws the role's colour.
bool followsLook(const QString& field, const QColor& stored, const QObject* surface = nullptr);

// The colour drawn for `field` holding `stored`. `surface` is the item (or
// its widget): a colour the item says the user picked is drawn as stored,
// and for a backdrop row it finds the container's user background.
QColor effective(const char* field, const QColor& stored, const QObject* surface = nullptr);
QColor effective(const QString& field, const QColor& stored, const QObject* surface = nullptr);

// A colour an item derives from a field rather than stores (an off
// state's text, a badge's fill): `role`'s colour while `field` holding
// `stored` follows the look, otherwise `classic`.
QColor derived(const char* field, const QColor& stored, const char* role, const QColor& classic,
               const QObject* surface = nullptr);

// The editors' note for a field that follows the look.
QString followsLookNote();

// A container item editor's colour swatch: the effective colour, in the
// swatch sheet WorkspacePreview recognises as a user colour (so the live
// pass leaves it alone), and the note as its tool tip while the field
// follows the look. `surface` is the item edited (its picks count; when
// null, the kSwatchSurfaceProperty an ancestor of `button` names). The
// field's stored value is not touched.
void paintSwatch(QPushButton* button, const QString& field, const QColor& stored,
                 const QObject* surface = nullptr);

// One colour field of an item and the colour it stores.
struct FieldColour {
    QString field;
    QColor stored;
};

// Every colour field `item` stores, with its stored colour: the item's own
// fields, a button grid's per-button colours, a composite face's colour
// keys and channels, and the items a composite face holds. Invalid
// (unset optional) colours are left out.
QVector<FieldColour> colourFieldsOf(const MeterItem& item);

// The container's background (ContainerWidget::contentBackground()). It is
// not an item field but takes the same rows. The container's title bar and
// border store no colour; they draw workspace roles already.
inline constexpr auto kContainerBackground = "Container.background";

// The entry context key that saves an item's picked colours: an array of
// field names, written only when the item has a pick.
inline constexpr auto kUserColoursKey = "userColours";

// The dynamic property (a QObject*) an item editor sets on itself: the
// item its swatches are painted for, so paintSwatch() without a surface
// finds the item's picks.
inline constexpr auto kSwatchSurfaceProperty = "nereusColourSurface";

} // namespace ItemColourLook
} // namespace NereusSDR
