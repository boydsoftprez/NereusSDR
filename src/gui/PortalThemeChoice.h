#pragma once
// no-port-check: NereusSDR-original choice of the Qt platform theme on Linux.
// Modification history (NereusSDR):
//   2026-11-05 - Issue 700: Cinnamon's file choosers go through the desktop
//                 portal unless the user picked a theme. J.J. Boyd (KG4VCF),
//                 AI-assisted via Claude Code.

#include <QString>

namespace NereusSDR {

// True when this run should ask Qt for the xdgdesktopportal platform theme.
// desktop is XDG_CURRENT_DESKTOP, which can be a colon-separated list and reads
// X-Cinnamon on Cinnamon itself. currentTheme is QT_QPA_PLATFORMTHEME; a theme
// the user chose stands. Limited to Cinnamon because that is the only desktop
// where the problem and this fix were verified.
inline bool shouldUsePortalThemeOnThisDesktop(const QString& desktop, const QString& currentTheme)
{
    return currentTheme.isEmpty()
        && desktop.contains(QStringLiteral("Cinnamon"), Qt::CaseInsensitive);
}

} // namespace NereusSDR
