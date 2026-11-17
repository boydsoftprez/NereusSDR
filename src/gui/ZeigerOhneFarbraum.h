// =================================================================
// src/gui/ZeigerOhneFarbraum.h  (Longpath)
// =================================================================
//
// Longpath-original.
// no-port-check: Longpath-original, umgeht einen Qt-Fehler (QTBUG-147602).
//
// Mauszeiger, die Qt auf macOS aus einem Bild baut -- ohne das
// Farbprofil dieses Bildes.
//
// Absturz 2026-11-10 18:15 (SIGTRAP): BandwidthFilterPane setzte beim
// Ueberfahren der Filtermitte Qt::SizeAllCursor. macOS hat dafuer keinen
// eigenen Zeiger; Qt baut ihn aus seinem Bild sizeallcursor.png, und das
// traegt ein eingebettetes Monitorprofil ("SyncMaster Calibrated", 824
// Byte ICC). Qt 6.11.0/6.11.1 gibt beim Umwandeln eines Bildes mit
// eigenem Profil (QImage::toCGImage -> qt_mac_cgImageFormatForImage)
// den CoreGraphics-Farbraum frei, BEVOR CGImageCreate ihn liest
// (QTBUG-147602, behoben in qtbase a481ede673, enthalten ab 6.11.2 und
// 6.12.0). Ob es kracht, haengt davon ab, ob der Speicher schon neu
// vergeben ist -- darum nur manchmal, und nur beim ersten Gebrauch des
// Zeigers in einem Lauf.
//
// Ein Bild OHNE Profil nimmt in Qt den sRGB-Weg (ein Systemfarbraum, der
// nie freigegeben wird) und ist davon nicht betroffen. Die Release-Bauten
// mit Qt 6.8 sind es ohnehin nicht; die lokalen mit Homebrew-Qt 6.11.1
// (auch die automatisch installierte App) schon.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Original von Martin Fischer (OE5SOS), KI-gestuetzt
//                 ueber Anthropic Claude.
// =================================================================

#pragma once

#include <QCursor>
#include <QImage>
#include <QPoint>

namespace Longpath {

/// Ein Zeiger aus `bild` mit dem Griffpunkt `griff` -- das Farbprofil
/// des Bildes wird abgelegt, die Bildpunkte bleiben, wie sie sind.
QCursor zeigerAusBildOhneFarbraum(QImage bild, QPoint griff);

/// Der Zeiger fuer "ziehen/verschieben" (Qt::SizeAllCursor). Auf macOS
/// aus Qts eigenem Bild, mit demselben Griffpunkt wie bei Qt, nur ohne
/// Profil -- er sieht also aus wie bisher. Ist das Bild nicht da (etwa
/// auf der Offscreen-Plattform), oder auf Linux und Windows, die eigene
/// Zeiger haben: einfach Qt::SizeAllCursor.
QCursor bewegenZeiger();

} // namespace Longpath
