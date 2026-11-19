#pragma once

// =================================================================
// src/core/RufzeichenAuskunft.h  (Longpath)
// =================================================================
//
// Was über ein Rufzeichen bekannt ist, und wie weit die Abfrage ist.
//
// Longpath-original. no-port-check: Longpath-original.
//
// Die Rotor/Log-Karte fragt QRZ (Schalter, Entprellung, Cache, ein
// abgerechnetes Abo -- siehe den Kopf von QsoDetailPane.h). Das FT8-
// Feld zeigt seit 2026-11-18 Name, Ort und Klasse des Partners in der
// QSO-Karte. Es fragt dafuer NICHT selbst: zwei Stellen, die dasselbe
// Rufzeichen abfragen, hiessen zwei Anfragen je QSO. Die Karte meldet
// stattdessen, was sie weiss, und das FT8-Feld nimmt sich, was zu
// seinem Partner gehoert.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-18 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include "core/CallsignInfo.h"

#include <QString>

namespace Longpath {

struct RufzeichenAuskunft {
    enum class Stand {
        Unbekannt,      // niemand hat gefragt
        Aus,            // automatische Abfrage ausgeschaltet
        KeinKonto,      // kein QRZ-Konto eingetragen
        Laeuft,         // gefragt, Antwort steht aus
        Da,             // Antwort da (aus dem Netz oder dem Cache)
        NichtGefunden,  // QRZ kennt das Rufzeichen nicht
        Fehler,         // kein Netz, Anmeldung abgelehnt, ...
    };

    QString      rufzeichen;   // so, wie gefragt wurde (normalisiert)
    Stand        stand{Stand::Unbekannt};
    CallsignInfo info;         // nur bei Stand::Da gefuellt
};

} // namespace Longpath
