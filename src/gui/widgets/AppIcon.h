// src/gui/widgets/AppIcon.h
// no-port-check: NereusSDR-original GUI helper.
//
// AppIcon renders the app's own full-colour icon set (resources/icons/emoji/,
// copied byte-for-byte from the radio speaker and Audio Setup design's icon
// folder) in place of colour emoji text. Colour emoji fonts differ between
// macOS, Windows and Linux, and some Linux desktops have none, so a padlock,
// pin or bulb drawn as text could render as a box. These SVGs look the same
// everywhere (R-SPK-19, D7).
//
// Each name maps to :/icons/emoji/<name>.svg. The SVG is drawn through
// QSvgRenderer into a transparent image of logicalPx * dpr device pixels,
// with the pixmap's device pixel ratio set and no tint. Results are cached by
// name, size and dpr. An unknown name logs a warning and returns a null
// pixmap or icon. GUI thread only.
//
// A button that shows one of these icons also sets the dynamic property
// kIconProperty ("nereusIcon") to the icon name, so tests and captures can
// read which icon it shows.
//
// Modification history (NereusSDR):
//   2026-10-06 - Written for the radio speaker and Audio Setup plan, Task 5.
//                J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                Anthropic Claude Code.
#pragma once

#include <QIcon>
#include <QPixmap>
#include <QString>

class QAbstractButton;
class QWidget;

namespace NereusSDR::AppIcon {

// Dynamic property a button carries naming the icon it shows.
inline constexpr const char* kIconProperty = "nereusIcon";

// An icon holding the named SVG at logicalPx, rendered for 1x and 2x and,
// when forDpr is given, for that widget's device pixel ratio too.
QIcon icon(const QString& name, int logicalPx, const QWidget* forDpr = nullptr);

// The named SVG at logicalPx * dpr device pixels, device pixel ratio dpr.
QPixmap pixmap(const QString& name, int logicalPx, qreal dpr);

// Sets button's icon to the named SVG at logicalPx, its icon size to match,
// and its kIconProperty to name.
void apply(QAbstractButton* button, const QString& name, int logicalPx);

} // namespace NereusSDR::AppIcon
