#pragma once

// =================================================================
// src/core/Ft8Mehrfach.h  (Longpath)
// =================================================================
//
// Longpath-original. Mehrere FT8-QSOs gleichzeitig — MSHVs
// „Multi Answering", Stufe 2.
//
// ── Was Stufe 1 konnte und was hier dazukommt ───────────────────────
//
// Stufe 1 (`Ft8Tx::vorbereitenMehrere`, PR #323) legt mehrere
// Nachrichten in EIN Sendefenster: je Platz 60 Hz hoeher, alle Kurven
// summiert, Pegel durch die Zahl der Plaetze. Das ist die Sendeseite.
//
// Was fehlte, ist die Buchhaltung davor: WER steht auf welchem Platz,
// wie kommt er darauf, und wann geht er wieder herunter. Genau das ist
// diese Klasse. Sie sendet nichts und kennt kein Fenster — sie haelt
// bis zu sechs `Ft8Ablauf` und sagt, was im naechsten Fenster von wem
// hinausginge.
//
// ── Das Modell, aus MSHV gelesen ────────────────────────────────────
//
// `src/HvTxW/hvmultianswermodw.h` [MSHV 2.76.7, LZ2HV] haelt zwei
// Listen nebeneinander:
//
//   LsQueue   wer angerufen hat und wartet
//   LsNow     wer gerade abgearbeitet wird
//
// Dazu `#define MAXSL 6` (:137, Kommentar „2.71 old=5") als harte
// Obergrenze und ein einstellbarer Regler darunter (`SBslots`).
// Longpath uebernimmt beides: `kMaxPlaetze = 6`, voreingestellt
// `kVorgabePlaetze`.
//
// Warum eine Obergrenze ueberhaupt: der Pegel wird durch die Zahl der
// Plaetze geteilt (MSHV `gen_ft8.cpp:303`). Sechs Plaetze heissen
// −15,6 dB je Station. Mehr waere kein Gewinn, sondern nur leiser.
//
// ── Was diese Klasse NICHT tut ──────────────────────────────────────
//
// Tasten, senden, Zeit messen, ins Logbuch schreiben. Sie rechnet.
// Dieselbe Trennung wie bei `Ft8Ablauf` und aus demselben Grund: eine
// Klasse, die rechnet UND sendet, laesst sich nicht gefahrlos pruefen.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-24 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
//   2026-11-17 — nimmVonHand() und raeumeAuf(); wer aus der Schlange
//                 nachrueckt, bekommt den Zug, der zu ihm gehoert
//                 (Rapport fuer den Anrufer, nicht mein Locator).
//                 Martin Fischer (OE5SOS), KI-gestuetzt ueber Anthropic
//                 Claude.
// =================================================================

#include <QString>
#include <QVector>

#include "core/Ft8Ablauf.h"
#include "core/Ft8Tx.h"

namespace Longpath {

class Ft8Mehrfach
{
public:
    /// Aus MSHV `hvmultianswermodw.h:137` [2.76.7]: `#define MAXSL 6`
    /// (dort kommentiert mit „2.71 old=5").
    static constexpr int kMaxPlaetze = 6;

    /// Voreinstellung. Drei ist MSHVs eigene Vorgabe und ein
    /// vernuenftiger Anfang: −9,5 dB je Station, und auf einem
    /// belegten Band hat man selten mehr echte Anrufer zugleich.
    static constexpr int kVorgabePlaetze = 3;

    /// Ein Platz, wie ihn die Oberflaeche anzeigt.
    struct Belegung {
        QString              partner;
        Ft8Ablauf::Zustand   zustand{Ft8Ablauf::Zustand::Ruht};
        float                toneHz{0.0f};
    };

    void setEigeneDaten(const QString& meinRuf, const QString& meinLocator);
    QString meinRuf()     const { return m_meinRuf; }
    QString meinLocator() const { return m_meinLocator; }

    /// 1..kMaxPlaetze. Ausserhalb wird geklemmt, nicht abgelehnt: ein
    /// Regler, der stillschweigend nichts tut, ist schlimmer als einer,
    /// der an seiner Grenze stehen bleibt.
    void setPlaetze(int n);
    int  plaetze() const { return m_plaetze; }

    /// Die Grundfrequenz des ersten Platzes. Die uebrigen liegen je
    /// `Ft8TxSource::kPlatzAbstandHz` hoeher (MSHV `gen_ft8.cpp:262`).
    void setGrundtonHz(float hz);
    float grundtonHz() const { return m_grundtonHz; }

    /// Ueber dieser Tonlage soll kein Platz liegen, wenn es sich
    /// vermeiden laesst.
    ///
    /// ── Warum (2026-11-18) ──────────────────────────────────────────
    ///
    /// Am Geraet, 20 m: Grundton 2710 Hz, drei Plaetze -- also 2710,
    /// 2770 und 2830 Hz. 49 Sendungen, zwei QSOs. Betreiber: „leider
    /// bekomme ich fast keine qsos zusammen auf ft8". PSK Reporter
    /// derselben zwei Stunden: 68 Empfangsmeldungen bei 2700 Hz, drei
    /// bei 2800. Wer mit einem SSB-Filter von 300 bis 2700 Hz hoert --
    /// und das tun viele --, bekommt einen Platz bei 2770 oder 2830 Hz
    /// gar nicht mit. Der dritte lag sogar ueber der Grenze, die das
    /// Fenster fuer die Sendetonlage selbst setzt (2800 Hz).
    ///
    /// Die MSHV-Leiter (`Ft8TxSource::tonleiter`) stapelt immer nach
    /// OBEN. Passt der Stapel der eingestellten Platzzahl nicht unter
    /// diese Grenze, stapeln wir nach UNTEN: der Grundton bleibt, wo
    /// der Betreiber ihn hingestellt hat, die weiteren Plaetze liegen
    /// darunter statt darueber.
    static constexpr float kPlatzObergrenzeHz = 2500.0f;

    /// Die Tonlagen der ersten `anzahl` Plaetze. Die RICHTUNG haengt an
    /// der eingestellten Platzzahl (`plaetze()`), nicht an `anzahl` und
    /// nicht daran, wie viele gerade laufen -- sonst spraenge ein
    /// laufender Platz um, sobald ein anderer dazukommt.
    QVector<float> tonleiter(int anzahl) const;

    /// Jemand hat mich gerufen. Ist ein Platz frei, geht er sofort
    /// darauf; sonst in die Warteschlange. Ein Rufzeichen, das schon
    /// auf einem Platz oder in der Schlange steht, wird NICHT doppelt
    /// aufgenommen — sonst antwortete man demselben zweimal in
    /// demselben Fenster, auf zwei Toenen.
    /// Gibt true, wenn er neu aufgenommen wurde.
    bool nimmAnrufer(const QString& partner, int snr,
                     const QString& seinLocator = QString());

    /// Eine Station, die MICH nicht gerufen hat, auf einen Platz
    /// nehmen (2026-11-17): der Doppelklick auf ihr CQ, oder „+ Queue".
    /// Der Unterschied zu `nimmAnrufer` ist der erste Zug -- hier geht
    /// mein Ruf mit Locator hinaus, dort gleich der Rapport. Wer nie
    /// gefragt hat, bekommt keinen Rapport.
    /// Sonst dieselben Regeln: kein Rufzeichen doppelt, und ist alles
    /// belegt, stellt er sich an.
    bool nimmVonHand(const QString& partner, int snr,
                     const QString& seinLocator = QString());

    /// Eine gehoerte Nachricht an ALLE laufenden Ablaeufe reichen.
    /// Jeder prueft selbst, ob sie ihn angeht (`Ft8Ablauf::verarbeite`
    /// uebergeht still, was nicht an ihn gerichtet ist).
    /// Gibt die Zahl der Ablaeufe, die dadurch vorangekommen sind.
    int verarbeite(const QString& nachricht, int snr);

    /// Was im naechsten Fenster hinausginge — fertig fuer
    /// `Ft8Tx::vorbereitenMehrere()`. Leere Nachrichten fallen heraus,
    /// damit ein ruhender Platz keinen Ton belegt.
    QVector<Ft8TxSource::Platz> naechsteNachrichten() const;

    /// Ein Fenster ist vergangen. Reicht es an alle laufenden Ablaeufe
    /// weiter und raeumt danach auf: fertige und abgebrochene Plaetze
    /// werden frei, und aus der Warteschlange rueckt nach.
    void fensterVerstrichen();

    /// Nur aufraeumen, OHNE ein Fenster zu zaehlen (2026-11-17): fertige
    /// und abgebrochene Plaetze werden frei, die Schlange rueckt nach.
    /// Fuer den Fall, dass ein QSO zwischen zwei Sendungen zu Ende geht
    /// -- dann ist kein Fenster vergangen, aber der Platz ist frei.
    void raeumeAuf();

    /// Die fertigen QSOs HERAUSNEHMEN. Jedes wird genau einmal
    /// geliefert — der Aufrufer schreibt es ins Logbuch, und beim
    /// naechsten Aufruf ist es weg. Dieselbe Regel wie beim einzelnen
    /// Ablauf, wo ein QSO sonst bei jedem Schritt neu gemeldet wuerde.
    QVector<Ft8Ablauf> nimmFertige();

    QVector<Belegung> belegung() const;
    int  aktive()   const;
    int  wartende() const { return m_warteschlange.size(); }
    bool istVoll()  const { return aktive() >= m_plaetze; }

    /// Kennt einer der Plaetze oder die Schlange dieses Rufzeichen?
    bool kennt(const QString& partner) const;

    void abbrechen();

private:
    struct Wartend {
        QString partner;
        int     snr{0};
        QString locator;
        /// Von Hand angestellt: er hat mich NICHT gerufen. Entscheidet
        /// beim Nachruecken ueber den ersten Zug.
        bool    vonHand{false};
    };

    bool nimm(const QString& partner, int snr, const QString& seinLocator,
              bool vonHand);
    static Ft8Ablauf beginne(const Wartend& w, const QString& meinRuf,
                             const QString& meinLocator);

    void rueckeNach();

    QString            m_meinRuf;
    QString            m_meinLocator;
    int                m_plaetze{kVorgabePlaetze};
    float              m_grundtonHz{1500.0f};
    QVector<Ft8Ablauf> m_laufend;
    QVector<Ft8Ablauf> m_fertig;
    QVector<Wartend>   m_warteschlange;
};

}  // namespace Longpath
