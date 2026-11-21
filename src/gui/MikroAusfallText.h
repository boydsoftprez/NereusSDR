#pragma once

// =================================================================
// src/gui/MikroAusfallText.h  (Longpath)
// =================================================================
//
// Ein Text für den Ausfall des PC-Mikrofons, an beiden Stellen, an denen
// er steht: in der Fußzeile des TX-Felds und am Platz „MIC" in der
// unteren Leiste (Betreiber, 2026-11-20: Entwurf A + B). Eine Stelle für
// den Wortlaut, damit die beiden nicht auseinanderlaufen.
//
// Warum dort „restart Longpath" steht: PortAudio 19.7 zählt die Geräte
// nur beim Start (pa_mac_core.c gatherDeviceInfo, aufgerufen allein aus
// PaMacCore_Initialize). Ein wieder eingestecktes Mikrofon kennt Longpath
// erst nach einem Neustart. Selbst neu öffnen ist ein eigener Schritt.
// =================================================================
// Modification history (Longpath):
//   2026-11-20 — Angelegt. KI-gestützt via Anthropic Claude Code.
// =================================================================

#include <QDateTime>
#include <QString>

namespace Longpath::MikroAusfall {

inline QString geraet(const QString& name)
{
    return name.isEmpty() ? QStringLiteral("The system default input")
                          : QStringLiteral("“%1”").arg(name);
}

/// Tooltip, solange das PC-Mikrofon nichts liefert.
inline QString tooltip(const QString& name, const QDateTime& seit)
{
    const QString wann = seit.isValid()
        ? QStringLiteral(" at %1").arg(seit.toString(QStringLiteral("HH:mm:ss")))
        : QString();
    return QStringLiteral(
               "%1 stopped delivering audio%2.\n"
               "Transmitting in a voice mode would send silence.\n"
               "Plug the microphone back in and restart Longpath,\n"
               "or pick another input in Setup → Audio → TX Input.")
        .arg(geraet(name), wann);
}

}  // namespace Longpath::MikroAusfall
