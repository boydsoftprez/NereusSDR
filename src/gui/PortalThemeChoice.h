#pragma once
// no-port-check: NereusSDR-original choice of the Qt platform theme on Linux.
// Modification history (NereusSDR):
//   2026-11-06 - Issue 700: an AppImage keeps Qt's own dialog until someone has
//                 run one on Cinnamon. J.J. Boyd (KG4VCF), AI-assisted via
//                 Claude Code.
//   2026-11-05 - Issue 700: Cinnamon's file choosers go through the desktop
//                 portal unless the user picked a theme. J.J. Boyd (KG4VCF),
//                 AI-assisted via Claude Code.

#include <QString>

namespace NereusSDR {

// True when this run should ask Qt for the xdgdesktopportal platform theme.
// desktop is XDG_CURRENT_DESKTOP, which can be a colon-separated list and reads
// X-Cinnamon on Cinnamon itself. currentTheme is QT_QPA_PLATFORMTHEME; a theme
// the user chose stands. Limited to Cinnamon because that is the only desktop
// where the problem and this fix were verified. inAppImage says no: an AppImage
// ships no GTK plugin, so a broken portal would have nothing to fall back to,
// and its own theme already uses Qt's dialog.
inline bool shouldUsePortalThemeOnThisDesktop(const QString& desktop, const QString& currentTheme,
                                              bool inAppImage)
{
    return !inAppImage
        && currentTheme.isEmpty()
        && desktop.contains(QStringLiteral("Cinnamon"), Qt::CaseInsensitive);
}

} // namespace NereusSDR
