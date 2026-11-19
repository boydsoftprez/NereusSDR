// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/Ft8Ablauf.h  (Longpath)
// =================================================================
//
// Der Ablauf eines FT8/FT4-QSOs als reine Rechenvorschrift.
//
// ── Was diese Klasse NICHT tut ──────────────────────────────────────
//
// Sie sendet nicht, sie tastet nicht und sie kennt keine Uhr. Sie
// SAGT nur, was im naechsten Fenster zu senden WAERE. Ob das
// geschieht, entscheidet das Applet — und dort steht auch das Tasten
// (MoxController) und der Bandplan.
//
// Das ist dieselbe Trennung wie bei Ft8Tx (siehe dort): was Ton
// erzeugt oder tastet, laesst sich nicht mehr gefahrlos pruefen. Hier
// bleibt eine Klasse ohne Qt-Fenster, ohne Zeitgeber und ohne
// Seiteneffekte — jeder Zustandsuebergang ist eine Zeile im
// Prueflauf.
//
// ── Der Ablauf ──────────────────────────────────────────────────────
//
// Nach WSJT-X, mit dessen Nachrichtennummern:
//
//   Als ANRUFER (ich rufe CQ):
//     Tx6  CQ OE5SOS JN67
//          <- OE5SOS DL1ABC JO31      jemand antwortet
//     Tx2  DL1ABC OE5SOS -12          mein Rapport an ihn
//          <- OE5SOS DL1ABC R-09      sein Rapport, bestaetigt
//     Tx4  DL1ABC OE5SOS RR73         fertig, ins Logbuch
//
//   Als ANTWORTER (ich antworte auf ein CQ):
//     Tx1  DL1ABC OE5SOS JN67
//          <- OE5SOS DL1ABC -12       sein Rapport an mich
//     Tx3  DL1ABC OE5SOS R-09         mein Rapport, bestaetigt
//          <- OE5SOS DL1ABC RR73      er ist fertig
//     Tx5  DL1ABC OE5SOS 73           Hoeflichkeit, dann fertig
//
// ── Warum eine Wiederholungsgrenze ──────────────────────────────────
//
// Ein Ablauf, der ohne Antwort immer weiterruft, ist ein Sender, der
// sich selbst nicht abschaltet. `maxWiederholungen` begrenzt, wie oft
// dieselbe Nachricht ohne passende Antwort wiederholt wird; danach
// geht der Ablauf in `Abgebrochen` und sagt nichts mehr zu senden.
//
// Modification history (Longpath):
//   2026-10-16 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#pragma once

#include <QString>

namespace Longpath {

class Ft8Ablauf
{
public:
    enum class Rolle {
        Anrufer,     ///< ich rufe CQ und werde angerufen
        Antworter,   ///< ich antworte auf ein gehoertes CQ
        Hound        ///< ich rufe eine DX-Expedition im Fox-Betrieb
    };

    enum class Zustand {
        Ruht,           ///< nichts im Gange
        RufeCq,         ///< Tx6
        RufeAn,         ///< Tx1
        SendeReport,    ///< Tx2
        SendeRReport,   ///< Tx3
        SendeRR73,      ///< Tx4
        Sende73,        ///< Tx5
        Fertig,         ///< QSO vollstaendig — ab ins Logbuch
        Abgebrochen     ///< aufgegeben oder abgebrochen
    };

    /// Wie oft dieselbe Nachricht ohne passende Antwort wiederholt
    /// wird, bevor aufgegeben wird. WSJT-X nimmt hier voreingestellt
    /// ebenfalls eine kleine Zahl; mehr hilft selten und sendet lange.
    ///
    /// Gilt fuer die Zuege MITTEN im QSO. Fuer den CQ-Ruf selbst
    /// entscheidet der Betreiber -- siehe `setzeCqWiederholungen`.
    static constexpr int kMaxWiederholungen = 5;

    /// Wie oft ein CQ-Ruf sich wiederholt, bevor der Ablauf aufgibt.
    ///
    /// ── Warum das getrennt ist (2026-10-29) ─────────────────────────
    ///
    /// Es gab zwei Zaehler fuer dieselbe Sache, und der kleinere hat
    /// gewonnen -- der, den der Betreiber nicht sehen konnte. Im
    /// Fenster stand „x16"; der Ruf brach nach der FUENFTEN Sendung ab,
    /// weil `fensterVerstrichen()` hier starr `kMaxWiederholungen`
    /// nahm. Am Geraet gemessen (Protokoll 2026-10-29, 12:45:00 bis
    /// 12:47:12): genau fuenf Sendungen, danach kein „Senden
    /// vorbereitet" mehr. Betreiber, zweimal: „anschliessend wieder
    /// keine Leistung nach dem dritten oder vierten Durchgang".
    ///
    /// Die Zahl des Betreibers gilt jetzt fuer den Ruf. Die Fuenf
    /// bleibt, wo sie hingehoert: ein hoeflicher Zug im laufenden QSO
    /// wird nicht sechzehnmal wiederholt.
    void setzeCqWiederholungen(int n);
    int  cqWiederholungen() const { return m_cqWiederholungen; }

    /// Wie oft EINE STATION gerufen wird, die nicht antwortet
    /// (`Zustand::RufeAn`), bevor der Ablauf aufgibt.
    ///
    /// ── Dieselbe Geschichte, eine Stufe weiter (2026-11-17) ─────────
    ///
    /// Am Geraet, 15 m: TL8GD (Zentralafrika) fuenfmal gerufen, er war
    /// mit G6SBR beschaeftigt — „gave up TL8GD". Betreiber: „der call
    /// hängt glaube ich wieder". Er hing nicht, er hatte aufgegeben:
    /// die Fuenf von `kMaxWiederholungen` galt auch hier, und fuer
    /// seltenes DX sind fuenf Anrufe (zweieinhalb Minuten) nichts.
    ///
    /// Ein Anruf, auf den noch niemand geantwortet hat, ist kein Zug
    /// MITTEN im QSO. Er bekommt darum wie der CQ-Ruf die Zahl des
    /// Betreibers; die Fuenf bleibt fuer Rapport und Bestaetigung.
    void setzeAnrufWiederholungen(int n);
    int  anrufWiederholungen() const { return m_anrufWiederholungen; }

    /// Ich antworte auf ein gehoertes CQ.
    /// `snr` ist das, was ICH von ihm gemessen habe — es wird spaeter
    /// sein Rapport.
    void starteAlsAntworter(const QString& partner,
                            const QString& meinRuf,
                            const QString& meinLocator,
                            int snr,
                            const QString& seinLocator = QString());

    /// Jemand hat MICH gerufen, und ich steige mit dem Rapport ein.
    ///
    /// ── Warum das nicht `starteAlsAntworter` ist (2026-10-30) ───────
    ///
    /// `Ft8Mehrfach::nimmAnrufer` nahm bis heute `starteAlsAntworter`
    /// und sagte im Kommentar dazu, das sei „der Einstieg bei Tx2".
    /// War es nicht: `starteAlsAntworter` setzt `Zustand::RufeAn`,
    /// also Tx1. Auf „OE5SOS PD4RJ JO33" antwortete Longpath damit
    /// „PD4RJ OE5SOS JN67" — meinen Locator an einen, der schon
    /// meinen Rapport erwartet. Das QSO kam trotzdem zustande, kostete
    /// aber einen Durchgang mehr (30 s) und war nicht das, was die
    /// Gegenstation las.
    ///
    /// `snr` ist, was ICH von IHM gemessen habe — es wird sein
    /// Rapport, und er geht im naechsten Fenster hinaus.
    void starteMitAnrufer(const QString& partner,
                          const QString& meinRuf,
                          const QString& meinLocator,
                          int snr,
                          const QString& seinLocator = QString());

    /// Ich rufe CQ. Der Partner ergibt sich aus der ersten Antwort.
    void starteAlsAnrufer(const QString& meinRuf,
                          const QString& meinLocator);

    // ── Hound: eine DX-Expedition im Fox-Betrieb rufen ─────────
    //
    // From WSJT-X lib/DXped_Operations.txt [v2.7.0] — der Ablauf, Zug
    // fuer Zug, mit dem Beispiel von dort (Fox = KH1DX):
    //
    //     Fox                        Hound
    //     CQ KH1DX AJ10
    //                                KH1DX K1ABC FN42
    //     K1ABC KH1DX -13
    //                                KH1DX K1ABC R-11
    //     K1ABC RR73; W9XYZ <KH1DX> -17
    //                                (fertig, ins Logbuch)
    //
    // Drei Dinge sind anders als bei einem gewoehnlichen QSO, und
    // jedes davon hat hier seinen Grund:
    //
    //  1. **Der Fox darf gehasht ankommen.** Sein Rapport kann als
    //     „K1ABC <KH1DX> -13" kommen; die spitzen Klammern heiszen,
    //     dass im Paket nur ein Hash stand (siehe
    //     `ftx_message_decode_dxpedition` in ft8_lib).
    //  2. **Den Schluss macht der Fox**, nicht ich. Auf mein „R-11"
    //     folgt kein 73 von mir, sondern sein „K1ABC RR73; …" — und
    //     das ist der Augenblick fuer das Logbuch.
    //  3. **Ich sende ueber 1000 Hz.** From WSJT-X
    //     lib/DXped_Operations.txt:16-17 [v2.7.0] — „Hounds call in
    //     2nd sequence, 1000-5000 Hz above f0. Hounds transmitting
    //     below f0+1000 Hz will not be answered." Das ist nicht Sache
    //     dieser Klasse (sie kennt keine Frequenz), aber es gehoert
    //     zur Rolle und steht darum hier: wer darunter sendet, ruft
    //     ins Leere.
    void starteAlsHound(const QString& fox,
                        const QString& meinRuf,
                        const QString& meinLocator);

    bool istHound() const { return m_rolle == Rolle::Hound; }

    /// Die kleinste Tonlage, auf der ein Hound ueberhaupt beantwortet
    /// wird. Siehe oben; die Zahl steht in WSJT-X' eigener Vorschrift.
    static constexpr int kHoundMinTonHz = 1000;

    // ═══ Contestbetrieb (2026-11-04) ════════════════════════════════
    //
    // Im EU-VHF-Contest tritt an die Stelle des Rapports ein
    // AUSTAUSCH: Rapport, laufende Nummer und sechsstelliger
    // Feldkenner in einem. Das ist Nachrichtentyp 5, und er sieht so
    // aus:
    //
    //     <DL1ABC> <OE5SOS> 590001 JN78CT
    //     <DL1ABC> <OE5SOS> R 590001 JN78CT
    //
    // Betroffen sind NUR die beiden Austauschzuege (Tx2/Tx3). Anruf,
    // RR73, 73 und CQ bleiben gewoehnliche Saetze -- sie passen nicht
    // in Typ 5, und WSJT-X haelt es genauso.
    struct Contest {
        bool    an{false};        ///< Contestbetrieb eingeschaltet
        int     nummer{1};        ///< die laufende Nummer, die ICH gebe
        QString meinLocator6;     ///< mein Feldkenner, SECHSstellig

        /// Taugt das fuer eine Aussendung? Der Feldkenner muss sechs
        /// Stellen haben -- vier gehen in Typ 5 nicht hinein, und
        /// etwas anderes zu senden waere schlimmer als abzulehnen.
        bool brauchbar() const;
    };

    /// Der Rapport im Contest. Drei Bit tragen 52..59; gegeben wird
    /// im Contest durchweg 59, weil der wirkliche Stoerabstand
    /// ohnehin in seiner eigenen Spalte steht und niemand im
    /// FT8-Contest einen abweichenden Rapport erwartet.
    static constexpr int kContestRapport = 59;

    void setzeContest(const Contest& c);
    const Contest& contest() const { return m_contest; }

    /// Seine laufende Nummer, soweit sie in einem Austausch stand.
    /// 0, wenn nie eine kam -- dann bleibt sie im Logbuch leer,
    /// statt geraten zu werden.
    int empfangeneNummer() const { return m_nummerEmpfangen; }

    /// Sein sechsstelliger Feldkenner aus dem Austausch. Leer, wenn
    /// keiner kam. NICHT dasselbe wie `partnerLocator()`: der kommt
    /// aus dem gewoehnlichen Betrieb und hat oft nur vier Stellen.
    QString partnerLocator6() const { return m_partnerLocator6; }

    /// Was im naechsten Fenster zu senden waere, oder leer.
    QString naechsteNachricht() const;

    /// Derselbe Textbau, ohne Ablauf: welcher Satz gehoert zu welchem
    /// Zustand?
    ///
    /// Herausgezogen am 2026-10-18 fuer die Tx1-Tx6-Knoepfe im Fenster.
    /// Sie bauen die Saetze NICHT selbst -- sonst gaebe es zwei
    /// Wahrheiten, und die zweite waere die, die beim naechsten
    /// Protokollbesuch vergessen wird.
    static QString nachrichtFuer(Zustand z,
                                 const QString& partner,
                                 const QString& meinRuf,
                                 const QString& meinLocator,
                                 int reportDb);

    /// Dieselbe Frage im Contestbetrieb. Ist `c.an` falsch oder
    /// `c.brauchbar()` nein, faellt sie auf die Fassung darueber
    /// zurueck -- ein unbrauchbarer Austausch darf den gewoehnlichen
    /// Betrieb nicht lahmlegen.
    static QString nachrichtFuer(Zustand z,
                                 const QString& partner,
                                 const QString& meinRuf,
                                 const QString& meinLocator,
                                 int reportDb,
                                 const Contest& c);

    /// Eine gehoerte Nachricht einspeisen. Gibt true, wenn sie den
    /// Ablauf weitergebracht hat (dann ist der Zaehler zurueckgesetzt).
    /// Nachrichten, die nicht an mich gerichtet sind, werden still
    /// uebergangen.
    bool verarbeite(const QString& nachricht, int snr);

    /// Ein Fenster ist vergangen, ohne dass etwas Passendes kam.
    /// Nach kMaxWiederholungen geht der Ablauf in `Abgebrochen`.
    void fensterVerstrichen();

    void abbrechen();

    Zustand zustand()        const { return m_zustand; }
    bool    laeuft()         const;
    QString partner()        const { return m_partner; }
    int     wiederholungen() const { return m_wiederholungen; }

    /// Fuer das Logbuch: was ich ihm gegeben und was er mir gegeben
    /// hat. kKeinReport, solange es noch nicht feststeht.
    static constexpr int kKeinReport = -99;
    int gesendeterReport() const { return m_reportGesendet; }
    int empfangenerReport() const { return m_reportEmpfangen; }

    /// Sein Feldkenner, soweit er in einer Nachricht stand. Beim
    /// Antworter kommt er aus der angeklickten CQ-Zeile, beim Anrufer
    /// aus der Antwort auf das CQ. Leer, wenn nie einer kam — dann
    /// bleibt GRIDSQUARE im Logbuch leer, statt zu raten.
    QString partnerLocator() const { return m_partnerLocator; }

    /// Ist dieses Feld ein Schlusswort — „73", „RR73" oder „RRR"?
    ///
    /// Oeffentlich, weil das Applet dieselbe Frage stellt, bevor es
    /// einen Anrufer in die Warteschlange legt: wer sich
    /// verabschiedet, stellt sich nicht an. Zwei Stellen, eine Regel.
    static bool istAbschied(const QString& feld);

    /// Sieht das Feld aus wie ein Feldkenner (JN67, JN67VV)? Nicht
    /// jedes dritte Feld ist einer: „-12", „R-09", „RR73" und „73"
    /// stehen an derselben Stelle.
    static bool wieLocator(const QString& feld);

    /// Nur fuer Prueflaeufe und Anzeige: eine lesbare Form.
    static QString zustandName(Zustand z);

private:
    /// Ist die Nachricht an MICH gerichtet (erstes Feld = mein
    /// Rufzeichen)? Und von wem?
    bool anMich(const QString& nachricht, QString* vonWem) const;

    Rolle   m_rolle{Rolle::Antworter};
    Zustand m_zustand{Zustand::Ruht};
    QString m_partner;
    QString m_meinRuf;
    QString m_meinLocator;
    QString m_partnerLocator;
    int     m_reportGesendet{kKeinReport};
    int     m_reportEmpfangen{kKeinReport};
    int     m_wiederholungen{0};
    int     m_cqWiederholungen{kMaxWiederholungen};
    int     m_anrufWiederholungen{kMaxWiederholungen};
    Contest m_contest;
    int     m_nummerEmpfangen{0};
    QString m_partnerLocator6;
};

} // namespace Longpath
