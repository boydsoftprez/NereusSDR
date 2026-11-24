// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. Entwurf C „Mein Signal am Kiwi“
// (2026-11-24, Betreiber: einen KiwiSDR leicht hoeren, das eigene
// Signal dort hoeren, aufnehmen und schoen im Bild sehen).
//
//   2026-11-24 — Neu angelegt.
//
// Reine Logik, ohne Oberflaeche und ohne Netz, damit jede Zahl in einem
// Pruefstand steht:
//   * kiwiPreselect()  — das oeffentliche Verzeichnis nach Entfernung
//                        zum eigenen Locator und nach Band geordnet,
//   * KiwiHearMeMeter  — die ehrliche Messung "hoert der Kiwi mich?".
//
// Beides fasst keinen Kiwi an. Das Verzeichnis wird weiterhin nur auf
// einen Klick geholt (KiwiPublicDirectory::fetch), hier wird nur
// sortiert, was schon da ist.

#pragma once

#include "core/KiwiPublicDirectory.h"
#include "core/KiwiSdrManager.h"

#include <QString>
#include <QVector>

namespace Longpath {

// ── Verzeichnis-Vorauswahl ───────────────────────────────────────────

struct KiwiCandidate {
    KiwiPublicReceiver receiver;
    double km{-1.0};   // < 0: Standort des Empfaengers nicht lesbar
};

// Empfaenger, die der Betreiber sinnvoll anbieten darf, nach Entfernung
// zu ownGrid geordnet (nahe zuerst; ohne lesbaren Standort ans Ende).
//
// Ausgeschlossen werden:
//   * offline,
//   * ext_api nicht erlaubt (KiwiPublicReceiver::mayConnectViaApi —
//     unbekannte Regel gilt wie "nein", dieselbe Vorsicht wie im
//     Auswahldialog),
//   * voll belegt (users >= usersMax),
//   * Empfaenger, deren veroeffentlichter Bereich die Frequenz nicht
//     enthaelt. Ist "bands" nicht lesbar, bleibt der Empfaenger drin.
//
// frequencyHz <= 0 schaltet den Bandfilter ab. ownGrid ungueltig: keine
// Entfernung, Reihenfolge der Eingabe bleibt. maxCount <= 0: alle.
QVector<KiwiCandidate> kiwiPreselect(
    const QVector<KiwiPublicReceiver>& receivers,
    const QString& ownGrid, double frequencyHz, int maxCount = 0);

// Entfernung zum Empfaenger in km aus dessen Locator, ersatzweise aus
// "gps" ("(lat, lon)"). -1, wenn beides nicht lesbar.
double kiwiReceiverDistanceKm(const KiwiPublicReceiver& receiver,
                              const QString& ownGrid);

// Deckt "bands" ("0-30000000", Hz) die Frequenz ab? Nicht lesbar: true.
bool kiwiReceiverCoversFrequency(const KiwiPublicReceiver& receiver,
                                 double frequencyHz);

// ── Welche Scheibe, welche Frequenz ──────────────────────────────────

// Bleibt der Ton dieses Profils beim Senden hoerbar? Das Profil selbst
// kann es verlangen (keepAudioDuringTx); im Modus "Mein Signal" gilt es
// zusaetzlich fuer das Profil des Modus -- ohne das waere der ganze
// Modus stumm, genau dann, wenn man hinhoeren will.
inline bool kiwiKeepsAudioDuringTx(const KiwiSdrAntennaProfile& profile,
                                   bool isOwnSignalProfile)
{
    return profile.keepAudioDuringTx || isOwnSignalProfile;
}

// ── "Hoert der Kiwi mich?" ───────────────────────────────────────────
//
// Einfach und ehrlich: waehrend der Sendung wird in jeder Zeile des
// Kiwi-Wasserfalls der Pegel auf der Markenfrequenz gegen den Median
// der Zeile gehalten. Hoert er mich, steht dort ein Abstand; hoert er
// mich nicht, steht dort Rauschen.
//
// Was die Messung NICHT behauptet:
//   * "hoert nichts" nur, wenn genug Zeilen im Sendefenster ankamen UND
//     die Marke im Bild liegt. Kommt kein Bild, ist das "unbekannt" --
//     fehlende Daten sind kein Messergebnis.
//   * Die Zeilen, die waehrend der Sendung ankommen, zeigen noch die
//     Zeit VOR der Sendung (Stromverzug). Das Fenster beginnt darum erst
//     nach dem Verzug: [Tasten + Verzug, Loslassen + Verzug].
//   * Der Pegel ist der des Kiwi-Wasserfalls (dBm, nur so genau wie
//     dessen Eichung), als S-Wert gerundet. Eine Naeherung, kein Pegelmesser.
class KiwiHearMeMeter {
public:
    enum class Verdict {
        Waiting,   // noch nicht gesendet -- nichts zu messen
        Unknown,   // gesendet, aber zu wenig/kein verwertbares Bild
        Hears,     // der Kiwi hoert die Aussendung
        Nothing,   // genug Bild, nichts auf der Marke
    };

    struct Result {
        Verdict verdict{Verdict::Waiting};
        int rows{0};              // gewertete Zeilen im Fenster
        double peakDbm{0.0};      // Median der Spitzen auf der Marke
        double snrDb{0.0};        // Median des Abstands zum Zeilen-Median
        int sUnits{0};            // 0..9, nur bei Hears
        int overS9Db{0};          // dB ueber S9, nur bei Hears
        bool delayMeasured{false};
        int delayMs{0};           // gemessener oder angenommener Verzug
    };

    // Schwelle: mindestens so viel ueber dem Zeilen-Median gilt als
    // "gehoert". 6 dB = eine S-Stufe, deutlich ueber dem Rauschen eines
    // schmalen Bereichs, ohne dass jede Spitze zaehlt.
    static constexpr double kHearSnrDb = 6.0;
    // Mindestzahl Zeilen im Fenster, bevor "hoert nichts" gesagt wird.
    static constexpr int kMinRows = 3;
    // Verzug, solange keiner gemessen ist (derselbe Wert wie das
    // verzoegerte Aufheben der Sendesperre).
    static constexpr int kAssumedDelayMs = 1200;
    static constexpr int kMinDelayMs = 200;
    static constexpr int kMaxDelayMs = 8000;
    // Halbbreite um die Marke, in der die Spitze gesucht wird.
    static constexpr double kMarkHalfWidthHz = 1500.0;
    // Schmaler als dies wird kein Bin als Breite angesehen.
    static constexpr int kMaxKeptRows = 600;

    void setMarkHz(double hz) { m_markHz = hz; }
    double markHz() const { return m_markHz; }

    // Tastenzustand, mit der Zeit des Ereignisses in Millisekunden
    // (beliebiger, aber derselbe Ursprung wie bei feedRow).
    void txChanged(bool transmitting, qint64 nowMs);

    // Eine Wasserfallzeile des Kiwi. lowHz/highHz = Bereich der Bins.
    void feedRow(const QVector<float>& binsDbm, double lowHz, double highHz,
                 qint64 nowMs);

    Result result() const;
    bool transmitting() const { return m_tx; }

    // Verwirft Zeilen und Ergebnis (Kiwi gewechselt, Modus aus).
    void reset();

    // S-Wert aus dBm: S9 = -73 dBm, 6 dB je Stufe (HF-Vereinbarung).
    static void sUnitsFromDbm(double dbm, int& sUnits, int& overS9Db);

private:
    struct RowSample {
        qint64 atMs{0};
        double peakDbm{0.0};
        double snrDb{0.0};
        bool markInside{false};
    };

    bool m_tx{false};
    bool m_everTransmitted{false};
    qint64 m_txStartMs{0};
    qint64 m_txEndMs{0};
    double m_markHz{0.0};
    bool m_delayMeasured{false};
    bool m_preTxHot{false};
    bool m_lastRowHot{false};
    int m_delayMs{kAssumedDelayMs};
    QVector<RowSample> m_rows;
};

// "S7" oder "S9+12" -- dieselbe Schreibweise wie das S-Meter.
QString kiwiSUnitsText(int sUnits, int overS9Db);

} // namespace Longpath
