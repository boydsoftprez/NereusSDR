#pragma once

// =================================================================
// src/core/RitXitAnzeige.h  (Longpath)
// =================================================================
//
// Welchen Versatz die VFO-Zeile zeigt, wenn es ZWEI Quellen gibt.
//
// Longpath kennt RIT und XIT an zwei voellig getrennten Stellen:
//
//   * die Scheibe (SliceModel::ritEnabled/ritHz/xitEnabled/xitHz) --
//     der eigene Empfaenger, gestellt in der RIT-Reihe des RX-Applets,
//     verrechnet in SliceModel::effectiveRxFrequency();
//   * das fremde Geraet (Ts590Cat::Zustand::ritAn/ritHz/xitAn) --
//     gestellt am TS-590 selbst oder im CAT-Applet, verrechnet IM
//     GERAET, wovon Longpath nichts merkt.
//
// Im CAT-Betrieb ist die Scheibe nur das Abbild des Geraets: die
// CatRigBruecke zieht dessen Frequenz hinein (CatRigBruecke.cpp,
// setFrequency). Der Versatz der Scheibe wird dort NICHT gesetzt --
// absichtlich, denn er wuerde das Empfangsfenster ein ZWEITES Mal
// verschieben, nachdem das Geraet schon verschoben hat. Darum muss die
// Zeile hier selbst entscheiden, welche der beiden Quellen gilt.
//
// Die Regel: laeuft der CAT-Weg und hat das Geraet geantwortet, gilt
// das Geraet -- es ist der Empfaenger, den der Betreiber hoert. Sonst
// gilt die Scheibe.
//
// Zwei Dinge, die hier bewusst NICHT passieren:
//
//   * Nichts wird addiert. Zwei Versaetze aus zwei Quellen sind zwei
//     verschiedene Empfaenger, keine Summe.
//   * Unbekannt wird nicht zu "aus" gemacht. Ein -1 im Zustand heisst,
//     dass das Geraet zu dieser Stelle noch nichts gesagt hat (eine zu
//     kurze IF-Antwort laesst xitAn auf -1, siehe Ts590Cat.h). Dann
//     zeigt die Zeile dazu nichts, statt "aus" zu behaupten.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-14 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/Ts590Cat.h"

namespace Longpath {

/// Was die VFO-Zeile anschreiben soll. Ein Wert, kein Verhalten.
struct RitXitAnzeige {
    bool ritAn{false};
    int  ritHz{0};
    bool xitAn{false};
    int  xitHz{0};

    bool operator==(const RitXitAnzeige&) const = default;
};

/// Die Quellenwahl. `catLaeuft` ist CompanionRigController::catBetrieb()
/// UND verbunden() -- ein eingestellter, aber toter CAT-Weg darf die
/// Scheibe nicht verdraengen.
inline RitXitAnzeige ritXitFuerDieZeile(bool catLaeuft,
                                        const Ts590Cat::Zustand& geraet,
                                        bool scheibeRitAn, int scheibeRitHz,
                                        bool scheibeXitAn, int scheibeXitHz)
{
    // Hat das Geraet zu KEINER der beiden Stellen etwas gesagt, ist es
    // als Quelle noch nicht da -- auch dann nicht, wenn der Weg laeuft.
    const bool geraetHatGesprochen = geraet.ritAn >= 0 || geraet.xitAn >= 0;
    if (catLaeuft && geraetHatGesprochen) {
        // Ein Versatzregister fuer beide, wie im TS-590 (Ts590Cat.h).
        return RitXitAnzeige{geraet.ritAn == 1, geraet.ritHz,
                             geraet.xitAn == 1, geraet.ritHz};
    }
    return RitXitAnzeige{scheibeRitAn, scheibeRitHz,
                         scheibeXitAn, scheibeXitHz};
}

}  // namespace Longpath
