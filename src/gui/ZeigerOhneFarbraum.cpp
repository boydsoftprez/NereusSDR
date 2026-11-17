// =================================================================
// src/gui/ZeigerOhneFarbraum.cpp  (Longpath)
// =================================================================
//
// Longpath-original -- see ZeigerOhneFarbraum.h.
// no-port-check: Longpath-original, umgeht einen Qt-Fehler (QTBUG-147602).
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Original von Martin Fischer (OE5SOS), KI-gestuetzt
//                 ueber Anthropic Claude.
// =================================================================

#include "gui/ZeigerOhneFarbraum.h"

#include <QColorSpace>
#include <QPixmap>

namespace Longpath {

QCursor zeigerAusBildOhneFarbraum(QImage bild, QPoint griff)
{
    // Ein ungueltiger Farbraum heisst fuer Qt "keiner": toCGImage nimmt
    // dann den sRGB-Systemfarbraum. Die Bildpunkte bleiben unberuehrt.
    bild.setColorSpace(QColorSpace());
    return QCursor(QPixmap::fromImage(bild), griff.x(), griff.y());
}

QCursor bewegenZeiger()
{
#ifdef Q_OS_MACOS
    // Dieselbe Quelle und derselbe Griffpunkt wie in Qt selbst
    // (QCocoaCursor::createCursorData, qcocoacursor.mm, Qt 6.11.1:
    // ":/qt-project.org/mac/cursors/images/sizeallcursor.png", QPoint(8, 8)).
    // Bei jedem Aufruf neu gebaut und nicht zwischengespeichert: ein
    // statisches QPixmap ueberlebt die Anwendung und stirbt nach ihr.
    const QImage bild(QStringLiteral(":/qt-project.org/mac/cursors/images/sizeallcursor.png"));
    if (!bild.isNull()) {
        return zeigerAusBildOhneFarbraum(bild, QPoint(8, 8));
    }
#endif
    return QCursor(Qt::SizeAllCursor);
}

} // namespace Longpath
