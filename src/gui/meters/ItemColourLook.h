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
//   - any other stored colour (one the user picked): that colour;
//   - the look off: the stored colour.
//
// Records, serialization and the editors' stored values never change, so
// a saved layout round-trips byte for byte and turning the look off brings
// the classic colours back. A field whose default has no matching role
// keeps its classic colour; its row says why.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Written by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-26 - Contrast round: derived() for colours an item derives
//                 (off-state text, badge fills). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
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

// True when the look is on and `stored` is a mapped default of `field`:
// the item draws the role's colour.
bool followsLook(const QString& field, const QColor& stored);

// The colour drawn for `field` holding `stored`. `surface` is the item (or
// its widget) for a backdrop row; it finds the container's user background.
QColor effective(const char* field, const QColor& stored, const QObject* surface = nullptr);
QColor effective(const QString& field, const QColor& stored, const QObject* surface = nullptr);

// A colour an item derives from a field rather than stores (an off
// state's text, a badge's fill): `role`'s colour while `field` holding
// `stored` follows the look, otherwise `classic`.
QColor derived(const char* field, const QColor& stored, const char* role, const QColor& classic);

// The editors' note for a field that follows the look.
QString followsLookNote();

// A container item editor's colour swatch: the effective colour, in the
// swatch sheet WorkspacePreview recognises as a user colour (so the live
// pass leaves it alone), and the note as its tool tip while the field
// follows the look. The field's stored value is not touched.
void paintSwatch(QPushButton* button, const QString& field, const QColor& stored);

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

} // namespace ItemColourLook
} // namespace NereusSDR
