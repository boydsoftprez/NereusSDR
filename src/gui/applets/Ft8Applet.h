// =================================================================
// src/gui/applets/Ft8Applet.h  (Longpath)
// =================================================================
//
// Longpath-original. Die Flaeche zum nativen FT8/FT4-Dekoder.
//
// Kein Port: weder Thetis noch AetherSDR dekodieren FT8 selbst (beide
// schicken den Ton an WSJT-X), es gibt also keine fremde Flaeche, von
// der hier etwas uebernommen werden koennte. Was dekodiert, liegt in
// Ft8Decoder/Ft8Stream; die Rechenarbeit macht ft8_lib (MIT, siehe
// third_party/ft8_lib/FT8LIB-PROVENANCE.md).
//
// ── Die Spaltenordnung ist geliehen ─────────────────────────────────
//
// Die Zeile heisst „HHMMSS  dB  DT  Freq  Nachricht" — dieselbe
// Reihenfolge, die WSJT-X in „Band Activity" zeigt. Nicht aus Code
// uebernommen, sondern weil Martin sie kennt und eine zweite
// Spaltenordnung fuer dieselbe Sache nur Suchzeit kostet. Dieselbe
// Begruendung wie bei N1MM/DXLog im Contestprogramm.
//
// ── Was hier NICHT ist ──────────────────────────────────────────────
//
// Senden. Keine Taste, keine Folgeschaltung, kein CQ. Ein Dekoder,
// der von selbst sendet, ist ein anderes Ding als einer, der zuhoert
// — und der Sendeweg gehoert nicht in ein Fenster, das man beim
// Aufraeumen des Layouts aus Versehen anklickt.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
//   2026-11-11 — Locator-Jagd: nimmFundAn() ist der EINE Weg, auf dem ein
//                 Fund angenommen wird (Doppelklick in der Liste und Klick
//                 auf der QSO-Karte), rufeVonDerKarte()/kartenSperre() sagen
//                 vorher, warum es nicht geht; gehoert() meldet jede Station
//                 mit Locator an die Karte. Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
//   2026-11-17 — Mehrere Plaetze: die Nachpruefung vor dem Senden packt
//                 die PLAETZE neu statt sie mit dem einzelnen Ablauf zu
//                 ueberschreiben; Doppelklick und „THEN" gehen im
//                 Mehrfachbetrieb auf einen Platz. Martin Fischer
//                 (OE5SOS), KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#pragma once

#include "AppletWidget.h"
#include "core/DigiFrequenzen.h"

#include "core/DigiFrequenzen.h"
#include "core/Ft8Decoder.h"
#include "core/Ft8Ablauf.h"
#include "models/LogEntry.h"
#include "core/Ft8Mehrfach.h"
#include "core/Ft8Warteschlange.h"
#include "core/GridsGearbeitet.h"
#include "Ft8PlatzStand.h"
#include "core/Ft8Tonwahl.h"
#include "core/Ft8Tx.h"
#include "core/Ft8Uhrversatz.h"
#include "core/LocatorJagd.h"
#include "core/RufzeichenAuskunft.h"
#include "core/WdspTypes.h"

#include <QDateTime>
#include <QPointer>
#include <QHash>
#include <QColor>
#include <QVector>

#include <memory>
#include <optional>

class QAction;
class QLabel;
class QPlainTextEdit;
class QSplitter;
class QSpinBox;
class QComboBox;

namespace Longpath { class Ft8Wasserfall; }
namespace Longpath { class Ft8Fensterbalken; }
namespace Longpath { class Ft8DannListe; }
namespace Longpath { class Ft8PartnerZeile; }
class QProgressBar;
class QLineEdit;
class QPushButton;
class QTimer;

namespace Longpath {

class AudioTapRing;
class Ft8Stream;
class RadioModel;
class SliceModel;

/// Nativer FT8/FT4-Dekoder, sichtbar in den Datenmodi (DIGU/DIGL).
///
/// Aufbau:
///   1. Kopfzeile   — FT8 / FT4, Fensteruhr, Fuellstand
///   2. Liste       — eine Zeile je Fund, Spalten wie in WSJT-X
///   3. Fusszeile   — Zahl der Funde im letzten Fenster, Leeren
class Ft8Applet : public AppletWidget {
    Q_OBJECT
public:
    explicit Ft8Applet(RadioModel* model, QWidget* parent = nullptr);
    ~Ft8Applet() override;

    QString appletId()    const override { return QStringLiteral("Ft8Decoder"); }
    QString appletTitle() const override { return QStringLiteral("FT8 Decoder"); }
    void    syncFromModel() override;

    /// Wie hoch das Fenster bei dieser Breite mindestens sein muss
    /// (2026-11-18). Seit die drei Reihen umbrechen, haengt die Hoehe
    /// an der Breite -- und der Rollbereich des schwebenden Fensters
    /// fragt danach. Qt antwortet von sich aus mit der WUNSCHhoehe; die
    /// Fundliste gaebe dann nie Platz ab, und `Stop` rutschte unter den
    /// Fensterrand, obwohl ueber ihm noch Luft ist. Siehe die .cpp.
    int heightForWidth(int breite) const override;

    /// Wie beim CW- und RTTY-Dekoder: eine Flaeche, die der jeweils
    /// aktiven Scheibe folgt.
    void setSlice(SliceModel* slice);
    /// Steht die Skala auf einer FT8- oder FT4-Anrufstelle, folgt der
    /// Dekoder ihr (2026-11-15). Oeffentlich fuer den Pruefstand.
    void folgeDerSkala(double skalaHz);
    /// Die Skala hat sich bewegt: bei einem anderen BAND die Liste
    /// leeren, dann `folgeDerSkala`.
    void skalaBewegt(double skalaHz);
    /// Band oder Verfahren (FT8/FT4) hat gewechselt: was in der Liste
    /// steht und wer wartet, wurde woanders gehoert und ist hier nicht
    /// mehr zu erreichen. Betreiber am 2026-11-17.
    void leereFuerNeuenOrt(const QString& grund);

    void setProtocol(Ft8Decoder::Protocol p);

    /// Was die Rotor/Log-Karte ueber ein Rufzeichen weiss (2026-11-18).
    /// Genommen wird nur, was zum laufenden Partner gehoert -- die
    /// Karte meldet auch, was der Betreiber selbst eintippt. Das FT8-
    /// Feld fragt QRZ nie selbst: siehe RufzeichenAuskunft.h.
    void setPartnerAuskunft(const Longpath::RufzeichenAuskunft& a);
    Ft8Decoder::Protocol protocol() const;

    // ── Senden (2026-10-15) ─────────────────────────────────────────
    //
    // Betreiber: „ft4-ft8 fertig machen."
    //
    // Was hier NICHT eingebaut ist: eine Folgeschaltung. Kein
    // automatisches Antworten, kein CQ in Schleife, kein Senden nach
    // einem Fund. Der Betreiber drueckt, das Programm sendet EIN
    // Fenster, dann ist Schluss. Ein Dekoder, der von selbst sendet,
    // ist ein anderes Ding — und einer, der es im Hintergrund tut,
    // ein gefaehrliches.
    //
    // Der Ablauf: Knopf -> die Nachricht wird auf den Ton gelegt ->
    // beim naechsten eigenen Fenster schaltet das Applet den
    // Mikrofonweg auf die FT8-Quelle, tastet, laesst das Fenster
    // laufen und nimmt beides wieder zurueck.
    /// Die Antwort auf eine Zeile der Fundliste, oder leer.
    ///
    /// `zeile` ist eine Zeile im Format der Liste (Spalten wie in
    /// WSJT-X „Band Activity": HHMMSS dB DT Freq Nachricht). Heraus
    /// kommt der Standardanruf „<sein Ruf> <mein Ruf> <mein Locator>"
    /// — das, was man auf ein CQ hin sendet.
    ///
    /// Leer, wenn die Zeile kein Rufzeichen hergibt, wenn das eigene
    /// Rufzeichen fehlt, oder wenn man sich selbst anklickt.
    ///
    /// Statisch und ohne Zustand, damit die Regel ohne Fenster und
    /// ohne Maus zu pruefen ist — der Doppelklick ist nur der Ausloeser.
    static QString antwortAuf(const QString& zeile,
                              const QString& meinRuf,
                              const QString& meinLocator);

    /// Die Nachricht aus einer Anzeigezeile — ohne die Landspalte.
    ///
    /// Seit der Landspalte (#311) steht hinter der Nachricht der
    /// Landesname. `simplified()` plus `section(' ', 4)` nimmt ihn
    /// mit, und `section(' ', -1)` liefert dann „Hungary" statt
    /// „JN86". Das traf den Doppelklick an zwei Stellen.
    ///
    /// Die Spalten stehen fest (siehe zeilentext()): vier Zahlenfelder,
    /// dann die Nachricht, dann — durch ZWEI Leerzeichen getrennt —
    /// das Land. Innerhalb einer FT8-Nachricht stehen nie zwei
    /// Leerzeichen hintereinander, also trennt das erste Doppelte
    /// sauber. Ohne Landspalte bleibt die Zeile unveraendert.
    static QString nachrichtAusZeile(const QString& zeile);
    /// Die Zeile ohne die Bandspalte nach der Uhrzeit (2026-11-19).
    static QString ohneBandmarke(const QString& zeile);
    /// Die Bandspalte einer angezeigten Zeile („15m"), leer ohne.
    static QString bandmarkeAusZeile(const QString& zeile);

    /// Der Locator der Gegenstation, oder leer. Geprueft wird die Form
    /// eines Maidenhead-Feldes (zwei Buchstaben, zwei Ziffern) —
    /// „JN86" ist einer, „R-09" und „+02" sind keine.
    static QString locatorAusNachricht(const QString& nachricht);

    /// Was ich gerade sende, in die rechte Spalte („Rx Frequency").
    /// Wie in WSJT-X: links steht das Band, rechts das eigene
    /// Gespraech — die Antworten an mich standen dort schon.
    void zeigeEigeneSendung(const QString& nachricht);

    /// Welcher Sorte ist eine Zeile? Die REIHENFOLGE ist die Aussage:
    ///
    ///   1. AnMich      -- jemand ruft DICH. Schlaegt alles.
    ///   2. NeuesLand   -- ein Land, das noch nie im Logbuch stand.
    ///   3. Cq          -- jemand ist rufbar.
    ///   4. Gewoehnlich -- ein fremdes QSO.
    ///
    /// „Neues Land" steht ueber „CQ", weil ein neues Land auch dann
    /// zaehlt, wenn die Station gerade kein CQ ruft -- man kann sie
    /// trotzdem im Auge behalten. Es steht UNTER „an mich", weil ein
    /// Ruf an die eigene Station immer das Dringendere ist.
    ///
    /// Statisch, damit die Reihenfolge ohne Fenster, ohne Logbuch und
    /// ohne cty.dat pruefbar ist.
    /// `NeuesGrid` steht zwischen Land und CQ: ein Land, das fehlt,
    /// ist mehr wert als ein Feld, das fehlt -- und beides mehr als
    /// ein beliebiges CQ.
    enum class Zeilenart { AnMich, NeuesLand, NeuesGrid, NeuesGridBand,
                           Cq, Gewoehnlich };
    static Zeilenart zeilenart(bool anMich, bool neuesLand, bool neuesGrid,
                               bool neuesGridBand, bool istCq);

    /// Die Spaltenueberschrift ueber der Fundliste. Aus DENSELBEN
    /// Breiten wie `zeilentext` -- zwei Stellen mit eigenen Zahlen
    /// liefen beim ersten Spaltenwechsel auseinander.
    static QString spaltenkopf();

    /// Eine Zeile der Liste, zusammengesetzt. Spalten wie in WSJT-X
    /// „Band Activity", dahinter das Land.
    ///
    /// Betreiber am 2026-10-21: „vielleicht kann man auch das land
    /// hinzufuegen." Der Auflaeser kannte es die ganze Zeit -- das
    /// Fenster benutzte den Namen nur, um eine Farbe zu waehlen, und
    /// warf ihn dann weg.
    ///
    /// Die Nachricht wird auf feste Breite gebracht, sonst stuende das
    /// Land bei jeder Zeile woanders und die Spalte waere keine. Ist
    /// sie laenger, gewinnt die Nachricht: lieber eine verrutschte
    /// Landspalte als eine abgeschnittene Nachricht.
    ///
    /// Statisch und ohne Zustand, damit die Spalten ohne Fenster,
    /// ohne Logbuch und ohne cty.dat pruefbar sind.
    static QString zeilentext(const QString& uhrzeit, int snrDb,
                              double dtSekunden, int hz,
                              const QString& nachricht,
                              const QString& land);

    /// Das Land zu einem Rufzeichen, oder leer. Leer heisst „nicht
    /// aufloesbar" und nicht „kein Land" -- dann bleibt die Spalte
    /// frei, statt etwas zu behaupten.
    QString landFuer(const QString& rufzeichen) const;
    bool istNeuesLandAuf(const QString& senderRuf, double hz) const;

    /// Das eigene Rufzeichen aus den Einstellungen. `User/Callsign`
    /// ist der kanonische Schluessel (SpotHubDialog schreibt ihn);
    /// `StationCallsign` ist der Rueckfall.
    /// Der Logbucheintrag zu einem fertigen Ablauf. Statisch und ohne
    /// Zustand, damit die Felder ohne Fenster zu pruefen sind — das
    /// Signal ist nur die Zustellung.
    /// Welcher Satz gehoert zu Tx1..Tx6? Die Zuordnung ist WSJT-X
    /// nachgebildet und steht als statische Regel da, damit sie ohne
    /// Fenster pruefbar ist. Gebaut wird der Satz selbst nicht hier,
    /// sondern von Ft8Ablauf::nachrichtFuer() -- eine Wahrheit.
    ///
    /// `partner` leer heisst: nur Tx6 (CQ) ergibt einen Satz, die
    /// uebrigen fuenf sprechen jemanden an, den es noch nicht gibt.
    /// Wer ist gerade das Gegenueber? Der laufende Ablauf, sonst das
    /// erste Wort im Textfeld (dort legt der Doppelklick es hin).
    QString aktuellerPartner() const;

    static QString txKnopfNachricht(int nummer,
                                    const QString& partner,
                                    const QString& meinRuf,
                                    const QString& meinLocator,
                                    int reportDb);

    /// `sendeTonHz` ist die Tonlage, auf der gesendet wurde -- seit
    /// 2026-10-18 waehlbar (siehe m_sendeTonHz). Sie geht in die
    /// gemeldete Frequenz ein, darum ein PARAMETER und keine
    /// Konstante: statisch bleibt die Funktion, damit die Rechnung
    /// ohne Fenster pruefbar ist.
    static Longpath::LogEntry logEintrag(const Ft8Ablauf& ablauf,
                                         double wahlHz,
                                         bool unteresSeitenband,
                                         Ft8Decoder::Protocol p,
                                         const QString& meinLocator,
                                         int sendeTonHz);

    /// Der eigene CQ-Ruf: „CQ <mein Ruf> <mein Locator>". Leer ohne
    /// Rufzeichen. Statisch, damit die Regel ohne Fenster zu pruefen
    /// ist — wie antwortAuf().
    static QString cqRuf(const QString& meinRuf, const QString& meinLocator);

    // ── Einen Fund annehmen (2026-11-11) ────────────────────────────
    //
    // Bis hierher stand das im Doppelklick selbst (eventFilter). Seit
    // die QSO-Karte gehoerte Stationen zeigt und ein Klick dort anrufen
    // soll, gibt es ZWEI Ausloeser -- und es darf nur EINEN Weg geben:
    // dieselbe Antwort ins Feld, derselbe Ablauf bis 73, dieselbe
    // Hound-/Contest-Behandlung. Ein zweiter Ablauf fuer die Karte
    // haette beim ersten Unterschied zwei Meinungen darueber, was ein
    // angenommener Anruf ist.

    /// Eine Zeile der Fundliste annehmen, als waere sie doppelgeklickt.
    /// Leer heisst angenommen; sonst steht da, warum nicht.
    QString nimmFundAn(const QString& zeile);

    /// Der Klick auf der Karte: erst kartenSperre(), dann nimmFundAn().
    /// Leer heisst angenommen.
    QString rufeVonDerKarte(const QString& zeile);

    /// Warum von der Karte aus gerade NICHT angerufen werden kann, oder
    /// leer. Die Liste braucht das nicht -- sie ist nur in FT8 zu sehen,
    /// und ihre Sendeversuche sagen am Ende des Weges, woran es lag. Die
    /// Karte ist ein anderes Fenster, oft auf dem anderen Schirm: ein
    /// Klick dort, der stumm nichts tut, saehe aus wie ein Fehler.
    QString kartenSperre() const;

    /// FT8/FT4 liegen in den Datenmodi -- beiden. Die EINE Regel fuer
    /// die Sichtbarkeit des Fensters (MainWindow) und fuer kartenSperre().
    static bool istFt8Modus(DSPMode modus);

    /// Das Band, unter dem ein QSO auf `hz` ins Logbuch ginge (mit
    /// Transverter das der Antenne). Dieselbe Rechnung wie logEintrag().
    static QString bandFuerLog(double hz);

    static QString eigenesRufzeichen();
    static QString eigenerLocator();

    void sendeNachricht(const QString& text);
    /// Mit mehreren Plaetzen: die Summe mehrerer Saetze, jeder auf
    /// seiner Tonlage (Multi Answering). `text` ist dann nur noch die
    /// Zeile fuer die Anzeige.
    void sendeNachricht(const QString& text,
                        const QVector<Ft8TxSource::Platz>& plaetze);
    void abbrechen();
    bool sendetGerade() const { return m_sendet; }

    /// Das Geraet hat nicht getastet oder die Sendung gebrochen
    /// (`RadioModel::flexNichtGetastet`). Haelt den Lauf an und sagt,
    /// woran es lag. Ohne eigene Sendung geschieht nichts.
    void sendenGescheitert(const QString& grund);

    // Fuer den Pruefstand.
    QPushButton* cqButtonForTest()  const { return m_cqBtn; }
    QPushButton* stopButtonForTest() const { return m_stopBtn; }
    /// Der „CQ"-Knopf, der den eigenen Ruf einsetzt. Nicht zu
    /// verwechseln mit cqButtonForTest() — das ist seit #244 der
    /// „Send"-Knopf (m_cqBtn), der Name ist dort eine Altlast.
    QPushButton* cqRufenButtonForTest() const { return m_cqRufenBtn; }
    const Ft8Ablauf& ablaufForTest() const { return m_ablauf; }
    /// Nur fuer Werkzeuge und Prueflaeufe: den Ablauf von aussen in
    /// einen Zustand bringen, um die Arbeitsleiste abzubilden.
    Ft8Ablauf& ablaufForTestSchreibend() { return m_ablauf; }
    void zeigeAblaufForTest() { zeigeAblauf(); }
    /// Der Zaehler fuer die CQ-Versuche, fuer den Pruefstand.
    class QSpinBox* cqVersucheSpinForTest() const { return m_cqVersucheSpin; }
    /// Einen Fund einspeisen, als waere er dekodiert worden — der
    /// einzige Weg, auf dem der Ablauf vorankommt, also auch der
    /// einzige, auf dem er sich pruefen laesst.
    void fundForTest(const Longpath::Ft8Decode& d);
    /// Eine GANZE Periode auf einmal -- `fundForTest` ist eine
    /// Periode mit einem einzigen Fund, und solche zaehlt der
    /// Uhrversatz bewusst nicht mit.
    void periodeForTest(const QVector<Longpath::Ft8Decode>& funde);

    // ── FT8 auf einem zweiten Band (2026-11-19) ──────────────────────
    //
    // Betreiber: „ich will 2 bänder … gleichzeitig empfangen umd ggf.
    // eine dxcc station zu hören." Ein zweiter Dekoder an einem
    // eigenen Abgriff (AudioEngine-Lauschplatz 0), gespeist von einer
    // zweiten Scheibe am Geraet (RadioModel::setLauschfrequenzHz).
    // Gesendet wird nur auf dem Hauptband.
    //
    // In die Liste kommt vom Lauschband nur, was ein NEUES DXCC-Land
    // ist, vorn mit grauer Bandmarke (Betreiber-Wahl 2B). Ein
    // Doppelklick darauf tauscht die Baender (3A): die Hauptscheibe
    // geht dorthin, das bisherige Hauptband wird Lauschband. Gesendet
    // wird danach NICHTS von selbst -- „aber nicht automatisch
    // senden … erst dann nach tunen."
    /// Das naheliegende zweite Band zu einem Hauptband. Belegt sind nur
    /// die beiden, die der Betreiber genannt hat („10 und 15 meter als
    /// beispiel"): 15 m -> 10 m und 10 m -> 15 m. Sonst keine Vorgabe.
    static std::optional<Band> naheliegendesLauschband(Band hauptband);
    /// Auf `hz` lauschen (Wahlfrequenz, DIGU); 0 schaltet ab.
    void setLauschfrequenzHz(double hz);
    double lauschfrequenzHz() const { return m_lauschHz; }
    /// „10m", leer wenn aus.
    QString lauschband() const { return m_lauschBand; }
    /// Wie viele Funde das Lauschband in der letzten Periode hatte.
    int lauschFundeZuletzt() const { return m_lauschZuletzt; }
    /// Eine Periode des Lauschbandes einspeisen, als waere sie dort
    /// dekodiert worden.
    void lauschPeriodeForTest(const QVector<Longpath::Ft8Decode>& funde);
    Ft8Stream* lauschStreamForTest() const { return m_lauschStream; }
    QComboBox* lauschFeldForTest() const { return m_lauschFeld; }
    /// Ohne Geraet mit zweiter Scheibe lauschen -- nur der Pruefstand
    /// speist die Funde selbst ein.
    void setLauschOhneGeraetForTest(bool an) { m_lauschOhneGeraet = an; }
    /// Der Doppelklick auf eine Zeile, wie ihn die Liste weiterreicht.
    QString doppelklickForTest(const QString& angezeigteZeile)
    { return nimmZeileAn(angezeigteZeile); }
    QLabel* uhrWarnungForTest() const { return m_uhrWarnung; }
    const Ft8Uhrversatz& uhrversatzForTest() const { return m_uhrversatz; }
    /// Ein gesendetes Fenster ist um. Im Betrieb kommt das von einem
    /// Zeitgeber nach bis zu 15 s — im Prueflauf waere das eine
    /// Viertelminute Warten je Zug.
    void fensterZuEndeForTest() { fensterZuEnde(); }
    /// Der Stand der Plaetze, wie ihn `Ft8PlaetzeApplet` zeigt.
    /// Oeffentlich fuer den Prueflauf -- sonst waere nur geprueft,
    /// DASS gerechnet wird, nicht WAS ankommt.
    Ft8PlaetzeStand plaetzeStandForTest() const { return baueStand(); }

    // ── Zwei reine Abbildungen ──────────────────────────────
    //
    // Zustand -> was auf der Kachel steht. Oeffentlich, weil sie ohne
    // Fenster, ohne Modell und ohne Geraet pruefbar sind -- und weil
    // genau diese Uebersetzung sonst im Panel noch einmal entstuende.
    /// Wie weit das QSO ist: 0..5, einer je Strich im Blatt.
    static int stufeFuer(Ft8Ablauf::Zustand z);
    /// Der Zug, wie das Fenster ihn nennt: „Tx2", „RR73", „CQ".
    static QString zugKurz(Ft8Ablauf::Zustand z);
    /// Die Warteschlange, damit ein Prueflauf nachsehen kann, ob ein
    /// Anruf wirklich ankommt. Ohne diesen Zugang waere nur die
    /// Einheit geprueft, nicht ihr Anschluss -- und genau so lag
    /// Multi Answering monatelang unerreichbar da (2026-10-30).
    const Ft8Warteschlange& schlangeForTest() const { return m_schlange; }
    Ft8Warteschlange& schlangeForTestSchreibend() { return m_schlange; }
    bool rufeNaechstenWartendenForTest() { return rufeNaechstenWartenden(); }
    /// „THEN" in der QSO-Karte und die Kapsel „+ Queue" in der
    /// Fundliste (2026-11-15).
    Longpath::Ft8DannListe* dannForTest() const { return m_dann; }
    /// Die Zeile unter „QSO with" (2026-11-18).
    Longpath::Ft8PartnerZeile* partnerZeileForTest() const { return m_arbeitWer; }
    QPushButton* queuePilleForTest() const { return m_queuePille; }
    /// Eine Zeile der Fundliste anstellen, wie ein Klick auf „+ Queue".
    /// Leer heisst angestellt; sonst steht da, warum nicht.
    QString stelleZeileAnForTest(const QString& zeile) { return stelleZeileAn(zeile); }
    /// Die Liste neu zeichnen, ohne auf den Sekundentakt zu warten.
    void zeigeDannForTest() { zeigeDann(); }
    QLineEdit*   textFeldForTest()  const { return m_textFeld; }

    // Fuer den Pruefstand.
    Ft8Stream*      streamForTest() const { return m_stream; }
    QPlainTextEdit* listForTest()   const { return m_list; }
    /// Der Zeitgeber, der auf den Beginn des eigenen Fensters wartet.
    /// Ein Prueflauf stellt ihn auf 0 und laesst ihn ablaufen — nur so
    /// wird der Sendeweg wirklich gefahren und nicht nur die Methode
    /// am Ende davon gerufen.
    QTimer* sendeTimerForTest() const { return m_sendeTimer; }
    QLabel*         statusForTest() const { return m_status; }
    /// Hiesz bis zum 2026-10-30 „last period: N" und stand in einer
    /// eigenen Fusszeile, am anderen Ende des Panels als die Zahl der
    /// Zeilen in der Liste. Beide stehen jetzt nebeneinander rechts
    /// neben den Pillen -- es sind zwei verschiedene Zahlen, auch wenn
    /// sie bei frischer Liste gleich aussehen.
    QLabel*         lastLabelForTest() const { return m_fundZahl; }
    /// Hiesz bis zum 2026-10-30 „CQ only" und war ein Haken in der
    /// Kopfzeile; jetzt ist es die Pille „CQ". Derselbe Zweck,
    /// derselbe Zugang.
    QPushButton*    nurCqButtonForTest() const { return m_sichtCq; }
    QPushButton*    nurNeueLaenderButtonForTest() const { return m_sichtNeuesLand; }
    /// Der Knopf „New grid" (2026-10-30). Er war als einziger der
    /// sechs Sichten ohne Pruefzugang -- und damit war der Weg
    /// Logbuch -> `istNeu` -> Zeilenart -> Anzeige nicht fahrbar.
    QPushButton*    sichtNeuesGridButtonForTest() const { return m_sichtNeuesGrid; }
    /// Die gearbeiteten Grossfelder, um im Lauf ein Logbuch zu
    /// stellen statt eine ADIF-Datei unterzuschieben.
    GridsGearbeitet& gridsForTest() { return m_grids; }
    Longpath::LocatorMosaik& mosaikForTest() { return m_mosaik; }
    /// Die Zeilenart der zuletzt gemerkten Fundzeile (2026-11-12).
    /// Ohne diesen Zugang laesst sich die zweite Stufe gar nicht
    /// belegen: im Text der Liste sehen „ganz neues Feld" und „auf
    /// diesem Band neu" gleich aus, sie unterscheiden sich nur in
    /// der Farbe -- und eine Farbe aus einem QPlainTextEdit
    /// zurueckzulesen waere eine Pruefung der Schriftart, nicht des
    /// Weges.
    Zeilenart letzteZeilenartForTest() const;
    QPushButton*    sichtAllesButtonForTest() const { return m_sichtAlles; }
    QPushButton*    sichtAnMichButtonForTest() const { return m_sichtAnMich; }
    QPushButton*    sichtFreqButtonForTest() const { return m_sichtFreq; }
    QSpinBox*       sendeTonFeldForTest() const { return m_tonFeld; }
    QPushButton*    houndButtonForTest() const { return m_houndBtn; }
    QPushButton*    contestButtonForTest() const { return m_contestBtn; }
    QSpinBox*       plaetzeSpinForTest() const { return m_plaetzeSpin; }
    QPushButton*    freieStelleButtonForTest() const { return m_freieStelleBtn; }
    QVector<int>    belegteToeneForTest() const { return belegteToene(); }
    bool            sucheFreieStelleForTest(bool nurWennBelegt)
                        { return sucheFreieStelle(nurWennBelegt); }
    const Ft8Mehrfach& mehrfachForTest() const { return m_mehrfach; }
    QSpinBox*       rundenFeldForTest() const { return m_rundenFeld; }
    Longpath::Ft8Wasserfall* wasserfallForTest() const { return m_wasserfall; }
    int             sendeTonHzForTest() const { return m_sendeTonHz; }
    QPushButton*    ft8ButtonForTest() const { return m_ft8Btn; }
    QPushButton*    ft4ButtonForTest() const { return m_ft4Btn; }
    int             lastPeriodCountForTest() const { return m_lastPeriodCount; }

    /// Der Rufzeichen-Absender aus einer FT8-Nachricht, oder leer.
    /// Oeffentlich, weil er sonst nicht einzeln pruefbar waere.
    static QString senderCallsign(const QString& message);

signals:
    /// Der Stand der Plaetze hat sich geaendert. `Ft8PlaetzeApplet`
    /// haengt daran -- das ist der EINZIGE Weg dorthin. Zwei Fenster,
    /// die beide in `Ft8Mehrfach` hineingreifen, haetten jedes seine
    /// eigene Meinung darueber, welcher Zug gerade dran ist.
    void plaetzeGeaendert(const Longpath::Ft8PlaetzeStand& stand);

    /// Ein QSO ist vollstaendig. Es geht denselben Weg wie eines aus
    /// WSJT-X (MainWindow -> RotorLogbookPanel::logExternalQso): eine
    /// Datei, eine Doppelt-Regel, ein Hochladen.
    /// Die Knoepfe FT8/FT4 IM Fenster gedrueckt.
    ///
    /// Betreiber am 2026-10-21: „bei ft4 und ft8 muss er glaube ich auch
    /// die frequenz anpassen und aendern." Es gab zwei Stellen, an denen
    /// man FT8 und FT4 waehlen konnte, und nur eine davon stellte das
    /// Geraet. Das Fenster kennt die Anrufstellen nicht und soll sie
    /// nicht kennen -- es meldet den Wunsch, MainWindow richtet her.
    void digiGewuenscht(Longpath::DigiFrequenzen::Digimodus modus);

    void qsoFertig(const Longpath::LogEntry& eintrag);

    /// Ein Fund wurde angenommen -- aus der Liste oder von der Karte.
    /// `zeile` ist die angenommene Zeile, `partner` das Rufzeichen.
    /// Fuer den Pruefstand: beide Ausloeser muessen hier dasselbe
    /// abliefern.
    void fundAngenommen(const QString& zeile, const QString& partner);

    /// Der QSO-Partner des Ablaufs hat gewechselt (2026-11-18) -- auf
    /// JEDEM Weg, nicht nur per Doppelklick oder Karte wie
    /// `fundAngenommen`: ein Antworter auf mein CQ, der naechste aus der
    /// Schlange, ein Doppelklick. Leer heiszt: keiner mehr (ein neues
    /// CQ). Gemeldet aus `meldeStand()` und nur bei echtem Wechsel.
    /// Die Rotor/Log-Karte haengt daran (MainWindow), damit Name, Ort
    /// und Foto des Partners neben dem FT8-Feld stehen, wie bei Zeus.
    /// Mehrfach-Betrieb meldet hier nichts: dort fuehrt `m_ablauf` nur
    /// den CQ-Ruf.
    void partnerGewechselt(const QString& partner);

    /// Jede gehoerte Station mit Rufzeichen, je Fund (2026-11-11). Die
    /// QSO-Karte haengt daran (ueber Ft8KartenBruecke).
    void gehoert(const Longpath::Ft8Gehoert& g);


protected:
    /// Doppelklick in die Fundliste: die Zeile wird zur Antwort im
    /// Eingabefeld. Gesendet wird dadurch NICHTS — der Druck auf den
    /// Knopf bleibt beim Betreiber.
    bool eventFilter(QObject* obj, QEvent* ev) override;

private slots:
    void onLauschDecoded(const QVector<Longpath::Ft8Decode>& funde,
                         const QDateTime& periodStart);
    void onDecoded(const QVector<Longpath::Ft8Decode>& funde,
                   const QDateTime& periodStart);

private:
    void buildUI();

    /// Das Blatt hinter dem ⚙ in der Kopfzeile: eigenes Rufzeichen und
    /// Locator. Betreiber 2026-10-21: „sollt in der taksleiste im
    /// decoder eine optionsmöglichkeit geben" -- und die Optionen
    /// gehoeren bei ihm immer rechts oben in den Panelkopf.
    void zeigeOptionen();
    void updateAudioTap();
    void updateStatus();
    /// Ein Land, das noch nie im Logbuch stand? EINE Stelle fuer die
    /// Faerbung der Liste und die Meldung an die Karte.
    bool istNeuesLand(const QString& senderRuf) const;

    QPointer<SliceModel> m_slice;
    Ft8Stream* m_stream{nullptr};
    std::unique_ptr<AudioTapRing> m_tapRing;
    // Das Lauschband (2026-11-19): eigener Dekoder, eigener Ring.
    Ft8Stream* m_lauschStream{nullptr};
    std::unique_ptr<AudioTapRing> m_lauschRing;
    double  m_lauschHz{0.0};
    QString m_lauschBand;
    int     m_lauschZuletzt{0};
    /// Die Skala vor dem letzten Bandwechsel: wohin das Lauschband
    /// geht, wenn das Hauptband auf seines wechselt.
    double  m_letzteSkalaHz{0.0};
    bool    m_lauschOhneGeraet{false};
    QComboBox* m_lauschFeld{nullptr};
    /// Das Feld „Also" neu fuellen: ohne das Hauptband, das
    /// naheliegende Band zuerst, die Wahl wie sie gilt.
    void fuelleLauschFeld();
    QTimer* m_pumpTimer{nullptr};
    int     m_lastPeriodCount{-1};
    int     m_lineCount{0};

    QLabel*         m_status{nullptr};
    // ── Der Zeilenspeicher (2026-10-30) ───────────────────
    //
    // Bis dahin ging jeder Fund direkt in die Textflaeche, und die
    // Filter wirkten nur auf NEUE Zeilen: wer „CQ only" einschaltete,
    // sah das Band unveraendert, bis die naechste Periode durch war.
    // Mit den Pillen aus Entwurf C faellt das sofort auf — also wird
    // behalten, was gefunden wurde, und die Liste bei jedem
    // Filterwechsel neu gezeichnet.
    //
    // Gespeichert wird das ERGEBNIS der Einordnung, nicht die
    // Nachricht allein: „neues Land" kostet eine Abfrage im
    // DXCC-Aufloeser, und die soll nicht bei jedem Pillendruck
    // hundertmal neu laufen.
    struct Fundzeile {
        QString text;        ///< die fertige Zeile, Spalten schon gesetzt
        QString nachricht;   ///< der reine FT8-Satz
        int     tonHz{0};
        bool    anMich{false};
        bool    istCq{false};
        bool    neuesLand{false};
        bool    neuesGrid{false};
        /// Das Feld steht im Logbuch, aber NICHT auf diesem Band
        /// (2026-11-12). Die schwaechere der beiden Stufen; siehe
        /// `Zeilenart::NeuesGridBand`.
        bool    neuesGridBand{false};
        QColor  landFarbe;
        bool    eigeneSendung{false};
        /// Vom Lauschband: dessen Bezeichnung („10m") und Wahlfrequenz.
        /// Leer bzw. 0 fuer das Hauptband.
        QString band;
        double  wahlHz{0.0};
    };
    QVector<Fundzeile> m_zeilen;
    /// Die Zeile, wie sie in der Liste steht: mit Bandmarke (Lauschband)
    /// oder, solange ein Lauschband laeuft, mit gleich breiter Luecke.
    QString anzeigeText(const Fundzeile& z) const;
    /// Der Doppelklick in der Fundliste: Lauschband-Zeile -> tauschen,
    /// sonst wie bisher nimmFundAn().
    QString nimmZeileAn(const QString& angezeigteZeile);
    /// Tauschen (3A). Sendet nichts.
    QString wechsleAufLauschband(const Fundzeile& z);
    static constexpr int kMaxZeilen = 400;

    /// Welche Zeilen zu sehen sind. Eine Pille, nicht vier Haken:
    /// zwei Filter gleichzeitig ergeben Faelle, die niemand im Kopf
    /// behaelt (und „CQ only" + „New DXCC only" zeigte frueher die
    /// Schnittmenge, ohne dass das irgendwo stand).
    enum class Sicht { Alles, AnMich, Cq, NeuesLand, NeuesGrid,
                       MeineFrequenz };
    Sicht m_sicht{Sicht::Alles};
    /// Die Liste aus dem Speicher neu zeichnen.
    void zeichneListe();
    /// Gehoert diese Zeile in die aktuelle Sicht?
    bool inSicht(const Fundzeile& z) const;
    /// Eine Zeile in den Speicher und, wenn sie passt, in die Liste.
    void merkeZeile(const Fundzeile& z);
    /// Eine Zeile unten an die Textflaeche haengen, richtig gefaerbt.
    void haengeAn(const Fundzeile& z);
    /// „10 Funde" bzw. „3 von 10" rechts neben den Pillen.
    void zeigeFundZahl();
    /// Die Uhr als Fehlerquelle sichtbar machen.
    ///
    /// Eine falsch gehende Rechneruhr sieht von innen aus wie ein
    /// totes Band: erst fallen die schwachen Funde weg, dann die
    /// halben Perioden. Das Schild steht neben der Fundzahl, weil
    /// dorthin sieht, wer sich fragt, ob ueberhaupt etwas ankommt.
    /// Es bleibt unsichtbar, solange die Lage in Ordnung ist.
    void zeigeUhrversatz();
    /// Die Arbeitsleiste nachziehen: mit wem, welcher Zug, was danach.
    void zeigeArbeit();
    /// Die Zeile unter „QSO with": Name, Ort, Klasse, Weg, Licht.
    void zeigeWer(const QString& wer);

    QPlainTextEdit* m_list{nullptr};
    // Zweite Spalte wie in WSJT-X: links alles („Band Activity"),
    // rechts nur, was auf der eigenen Stelle liegt oder an mich geht
    // („Rx Frequency"). Ohne sie geht die eigene Stelle in einem
    // vollen Band unter -- genau dort, wo man hinsehen muss.

    // Die Signalbreite von FT8 und FT4 (50 Hz). Siehe zeigeZeile().
    static constexpr int kRxFensterHz = 50;
    /// „CQ only": ein reiner ANSICHTS-Filter. Der Ablauf und die Funde
    /// fuer Panadapter/Logbuch laufen weiter ueber ALLE Zeilen -- wer
    /// dich ruft, darf nicht verschwinden, nur weil die Liste gerade
    /// aufgeraeumt ist. Bewusst nicht gesichert: er gehoert zum Blick
    /// auf das Band, nicht zur Einrichtung der Station.
    /// Haengt eine fertig gesetzte Zeile an die Liste -- gefiltert
    /// (siehe m_nurCq) und nach Dringlichkeit gefaerbt. `nachricht` ist
    /// der reine FT8-Text ohne die Spalten davor.
    // `tonHz` ist die Audiostelle des Fundes: sie entscheidet, ob die
    // Zeile auch rechts erscheint.
    void zeigeZeile(const QString& zeile, const QString& nachricht, int tonHz);

    /// Der zuletzt gehoerte Rauschabstand je Rufzeichen -- daraus
    /// bauen die Knoepfe Tx2/Tx3 ihren Rapport. Ohne ihn muesste man
    /// eine Zahl erfinden, und ein erfundener Rapport ist schlimmer
    /// als keiner.
    QHash<QString, int> m_letzterRapport;
    /// Die Tonlage im Durchlassbereich, auf der GESENDET wird.
    ///
    /// Betreiber 2026-10-18: „die sendestelle steht fest auf 1500 Hz"
    /// war der einzige Punkt der Luecken-Liste mit Folgen auf dem BAND
    /// statt am Schirm. Wer mit „Auto" auf einem vollen Band ruft,
    /// sendet sonst immer dorthin, wo schon jemand ist.
    ///
    /// Gesichert unter `Ft8/SendeTonHz` -- anders als „CQ only" gehoert
    /// das zur Einrichtung der Station und nicht zum Blick aufs Band:
    /// wer sich eine freie Stelle gesucht hat, will sie beim naechsten
    /// Start wiederhaben.
    QSpinBox* m_tonFeld{nullptr};
    /// Wie viele Runden der Dekoder ueber ein Fenster geht (1..3).
    /// Vorgabe 2; siehe core/Ft8Subtract.h.
    QSpinBox*    m_rundenFeld{nullptr};
    /// Der Streifen ueber der Fundliste. Zeigt den Durchlassbereich und
    /// nimmt per Klick die Sendestelle entgegen -- ohne ihn waere die
    /// waehlbare Sendestelle eine Zahl ohne Blick aufs Band.
    Longpath::Ft8Wasserfall* m_wasserfall{nullptr};
    int m_sendeTonHz{1500};

    QPushButton* m_sichtAlles{nullptr};
    QPushButton* m_sichtAnMich{nullptr};
    QPushButton* m_sichtCq{nullptr};
    QPushButton* m_sichtNeuesLand{nullptr};
    QPushButton* m_sichtFreq{nullptr};
    QLabel*      m_fundZahl{nullptr};
    QLabel*      m_uhrWarnung{nullptr};
    Ft8Uhrversatz m_uhrversatz;
    QLabel*      m_spaltenkopf{nullptr};

    // ── Die Arbeitsleiste (Entwurf C, 2026-10-30) ────────────
    //
    // Was beim automatischen Durchlauf zaehlt, steht unten, wo die
    // Hand ist: mit wem, welcher Zug, was danach, wie weit das
    // Fenster. Betreiber am 2026-10-30 zu Entwurf C: „c bitte".
    QWidget* m_arbeit{nullptr};
    QLabel*  m_arbeitPartner{nullptr};
    QLabel*  m_arbeitLand{nullptr};
    QLabel*  m_arbeitZustand{nullptr};
    QLabel*  m_arbeitZug{nullptr};
    QLabel*  m_arbeitDanach{nullptr};
    QLabel*  m_arbeitUhr{nullptr};
    Longpath::Ft8Fensterbalken* m_arbeitBalken{nullptr};
    /// Wer der Partner ist und wie es bei ihm aussieht -- die Zeile
    /// unter „QSO with" (2026-11-18, Entwurf A von drei).
    Longpath::Ft8PartnerZeile* m_arbeitWer{nullptr};
    /// Die letzte Auskunft der Rotor/Log-Karte. Gilt nur, solange sie
    /// zum Partner passt (verglichen wird bei jedem Zeichnen).
    Longpath::RufzeichenAuskunft m_auskunft;

    // ── „THEN": die Schlange in der QSO-Karte (2026-11-15) ──────────
    //
    // Betreiber nach drei Entwurfsblaettern: „C bauen". Die Schlange
    // gibt es seit dem 2026-10-30; zu sehen war von ihr nur
    // „2 waiting" im Kopf der Fundliste.
    Longpath::Ft8DannListe* m_dann{nullptr};
    /// frequencyChanged der Scheibe -> folgeDerSkala.
    QMetaObject::Connection m_skalaVerbindung;
    /// Das Band, auf dem die Zeilen der Liste gehoert wurden
    /// (`bandFuerLog`). Leer, solange noch keines bekannt ist.
    QString m_listenBand;
    /// Laesst das Alter jede Sekunde weiterlaufen -- nur solange
    /// jemand wartet.
    QTimer*      m_dannUhr{nullptr};
    /// „+ Queue" am rechten Rand der Fundzeile unter der Maus.
    QPushButton* m_queuePille{nullptr};
    QString      m_pilleZeile;
    int          m_pilleBlock{-1};
    void zeigeDann();
    /// Ein QSO mit `partner` ist fertig: sein Feld gilt ab jetzt als
    /// gearbeitet, und was davon schon BLAU in der Liste steht, wird
    /// sofort umgefaerbt. `locator` darf leer sein -- dann sucht die
    /// Funktion das Feld in den Zeilen, die der Partner selbst gesendet
    /// hat. Betreiber am 2026-11-18.
    void feldGearbeitet(const QString& partner, const QString& locator);
    /// Alles anhalten wie der Stop-Knopf (Ablauf, Plaetze, CQ-Automatik,
    /// laufende Sendung). `grund` bleibt in der Statuszeile stehen.
    void haltAllesAn(const QString& grund);
    void zeigePilleBei(const QPoint& posImViewport);
    void versteckePille();
    /// Leer heisst: diese Zeile laesst sich anstellen.
    QString warumNichtAnstellen(const QString& zeile) const;
    QString stelleZeileAn(const QString& zeile);
    void dannGewaehlt(const QString& partner);

    /// Der Wasserfall nahm ein Viertel der Hoehe und lebt erst, wenn
    /// Ton hereinkommt. Jetzt abschaltbar; die Wahl bleibt erhalten.
    /// Fox-Betrieb: ein Schalter, keine Erkennung. Ob eine Station im
    /// Fox-Betrieb laeuft, steht erst in ihrem 0.1-Satz, und den hoert
    /// man erst, wenn sie jemanden aufruft.
    QPushButton* m_houndBtn{nullptr};
    bool         m_houndBetrieb{false};

    /// Contestbetrieb: Schalter, Stand und die laufende Nummer.
    ///
    /// Die Nummer wird JE BAND gefuehrt und faengt dort wieder bei 1
    /// an -- IARU-Region-1-Regel („a serial number commencing with
    /// 001 for the first contact on each band"), und so haelt es auch
    /// das Contestprogramm (`ContestDefinition::serialScope()`,
    /// Vorgabe „band").
    QPushButton* m_contestBtn{nullptr};
    bool         m_contestAn{false};

    /// Den Contest-Stand aus den Einstellungen in den Ablauf tragen.
    void uebernimmContest();
    /// Die naechste Nummer in die Statuszeile.
    void zeigeContestStand();
    /// Der Schluessel, unter dem die Nummer dieses Bandes liegt.
    QString contestNummerSchluessel() const;
    /// Die naechste Nummer fuer das Band, auf dem die Scheibe steht.
    int naechsteContestNummer() const;

    // ── Multi Answering (2026-10-30 angeschlossen) ───────────
    //
    // `Ft8Mehrfach` und `Ft8TxSource::vorbereitenMehrere` lagen seit
    // dem Bau (#323/#333/#335) vollstaendig und geprueft im Baum --
    // und wurden von NIEMANDEM gerufen. Beide kamen nur in ihrer
    // eigenen Datei und im jeweiligen Prueflauf vor. In der Bilanz
    // gegen MSHV stand trotzdem „gebaut"; am 2026-10-30 fragte der
    // Betreiber „wie funktioniert mshv nun bei mir", und beim
    // Nachsehen war die Antwort: gar nicht.
    //
    // Ein Platz heisst eine Station, die gleichzeitig abgearbeitet
    // wird. Vorgabe 1 = aus, denn die Sendeleistung wird GETEILT:
    // zwei Plaetze sind −6 dB je Station, drei −9,5, sechs −15,6
    // (MSHV `gen_ft8.cpp:303`). Bei 5 W und drei Plaetzen bleiben
    // 1,7 W je Station.
    // ── Wo gerade jemand liegt (2026-10-30) ────────────────
    //
    // Die Tonlagen der zuletzt gehoerten Stationen. Daraus sucht
    // `Ft8Tonwahl::freieTonlage` eine Stelle, auf der man nicht in ein
    // laufendes QSO hineinruft.
    //
    // Nur die letzten Perioden: wer vor fuenf Minuten dort lag, ist
    // laengst weg, und eine Liste, die nie vergisst, haelt das ganze
    // Band fuer belegt.
    QVector<QVector<int>> m_toeneJePeriode;
    static constexpr int kPeriodenImGedaechtnis = 3;
    QVector<int> belegteToene() const;
    /// Die Grenzen, in denen gesendet werden darf -- beim Hound ab
    /// 1000 Hz (siehe `Ft8Ablauf::kHoundMinTonHz`).
    int tonUntergrenze() const;
    QPushButton* m_freieStelleBtn{nullptr};
    /// Eine freie Sendestelle suchen und setzen. Gibt true, wenn
    /// gewechselt wurde.
    bool sucheFreieStelle(bool nurWennBelegt);

    /// Wer mich gerufen hat, waehrend ich beschaeftigt war -- und
    /// wer gerade gearbeitet wurde.
    ///
    /// Gilt AUCH mit einem Platz, anders als die Schlange in
    /// `m_mehrfach`. Betreiber am 2026-10-30: „Antworten muessen
    /// immer automatisch sofort funktionieren, ohne meine Beihilfe."
    /// Welche Grossfelder schon im Logbuch stehen. Betreiber am
    /// 2026-10-30: „interessant waere zu sehen, welche grids ich
    /// noch nicht habe und diese dann zb blau zu markieren."
    GridsGearbeitet  m_grids;
    /// ── Dasselbe je Band (2026-11-12) ───────────────────────────
    ///
    /// Betreiber: die Locatorjagd „sollte optimiert werden und eher am
    /// programm ft8 und nicht im logprogramm zu finden sein."
    ///
    /// `m_grids` beantwortet nur „steht dieses Feld irgendwo im
    /// Logbuch". Das Mosaik beantwortet dieselbe Frage JE BAND --
    /// genau die Stufe, die es bei den Laendern mit DxccStatus
    /// (NewDxcc / NewBand) laengst gibt und bei den Feldern fehlte.
    /// Bis heute stand diese Rechnung nur hinter der Karte im
    /// Logbuchfenster, also dort, wo niemand FT8 fährt.
    Longpath::LocatorMosaik m_mosaik;
    QPushButton*     m_sichtNeuesGrid{nullptr};

    Ft8Warteschlange m_schlange;
    /// Den naechsten Wartenden anrufen. Gibt true, wenn einer da war.
    bool beginneNaechstenWartenden();
    bool rufeNaechstenWartenden();
    /// Einen gehoerten Anruf in die Schlange legen, wenn er gerade
    /// nicht bedient werden kann.
    void merkeAnrufFallsNoetig(const QString& nachricht, int snr,
                               int tonHz = -1);

    Ft8Mehrfach  m_mehrfach;
    QSpinBox*    m_plaetzeSpin{nullptr};
    int          m_plaetze{1};
    /// Mehrere Plaetze (2026-11-17 um zwei Ausnahmen ergaenzt).
    ///
    /// Mit Plaetzen ist `m_ablauf` NUR der CQ-Ruf; die QSOs fuehrt
    /// `m_mehrfach`, und nur dorthin reicht der Dekodier-Eingang die
    /// Funde. Damit das nie einem QSO den Boden wegzieht:
    ///   * nie als Hound -- wer eine Expedition ruft, fuehrt EIN QSO,
    ///     und das lebt in `m_ablauf`;
    ///   * nicht, solange in `m_ablauf` noch ein QSO steckt (die Zahl
    ///     der Plaetze wurde mitten im QSO erhoeht). Es laeuft auf dem
    ///     einfachen Weg zu Ende, danach gelten die Plaetze.
    bool mehrfachAn() const
    {
        if (m_plaetze <= 1 || m_houndBetrieb) { return false; }
        const bool qsoImEinzelnen =
            m_ablauf.laeuft()
            && m_ablauf.zustand() != Ft8Ablauf::Zustand::RufeCq;
        return !qsoImEinzelnen;
    }
    /// Was JETZT auf allen Plaetzen hinausginge: die laufenden QSOs
    /// und, solange ein Platz frei ist, der CQ-Ruf. Eine Stelle fuer
    /// das Packen nach einer Sendung UND fuer die Nachpruefung kurz
    /// vor der naechsten -- beide muessen dasselbe finden.
    QVector<Ft8TxSource::Platz> mehrfachPlaetze();
    /// Wer ruft mich in dieser Nachricht? Leer, wenn sie kein Anruf an
    /// mich ist: mein Rufzeichen steht nicht vorne, oder es ist ein
    /// Abschied. Eine Regel fuer den einfachen Weg und fuer die Plaetze.
    QString anruferAus(const QString& nachricht) const;
    /// Einen Anruf auf einen Platz nehmen, wenn er einer ist und der
    /// Anrufer weder schon dran noch eben gearbeitet noch abgewiesen
    /// ist. Gibt true, wenn er neu aufgenommen wurde.
    bool nimmAnruferAufPlatz(const QString& nachricht, int snr);
    /// Die fertigen QSOs der Plaetze ins Logbuch und in die Karenz.
    /// Gibt ihre Zahl.
    int meldeFertigePlaetze();
    /// Den Stand fuer das Plaetze-Panel bauen und melden.
    Ft8PlaetzeStand baueStand() const;
    void meldeStand();
    /// Wie weit ein QSO ist: 0..5, einer je Zug. Fuer die fuenf
    /// Striche im Blatt.

    /// Was in diesem Fenster auf allen Plaetzen hinausginge.
    void sendeMehrfach();
    /// Die Tonlage, die der Betreiber hatte, bevor Hound sie ueber
    /// 1000 Hz zog. 0 heisst: nichts zurueckzugeben.
    int          m_tonVorHound{0};

    QPushButton* m_wasserfallBtn{nullptr};
    bool         m_wasserfallAn{true};

    /// Die sechs Standardsaetze, jetzt hinter einem Knopf statt als
    /// sechs gleich aussehende Knoepfe in einer Reihe.
    QPushButton* m_handBtn{nullptr};
    QAction*     m_txAktionen[6]{};

    QPushButton*    m_ft8Btn{nullptr};
    QPushButton*    m_ft4Btn{nullptr};
    QPushButton*    m_clearBtn{nullptr};

    // Senden
    QLineEdit*   m_textFeld{nullptr};
    QPushButton* m_cqBtn{nullptr};
    QPushButton* m_stopBtn{nullptr};
    QTimer*      m_sendeTimer{nullptr};
    /// In welcher Haelfte der Minute diese Station sendet. „Gerade"
    /// ist die Vorgabe und entspricht WSJT-X' „Tx even/1st".
    Ft8Periode   m_periode{Ft8Periode::Gerade};
    bool         m_sendet{false};
    /// Warum zuletzt NICHT gesendet wurde. Bleibt in der Statuszeile
    /// stehen, bis wieder gesendet oder Stop gedrueckt wird — sonst
    /// schriebe der Sekundentakt „listening" darueber, und der Grund
    /// waere nach einer Sekunde weg (siehe `sendenGescheitert`).
    QString      m_festerHinweis;

    // ── Der QSO-Ablauf (2026-10-16) ──────────────────────────────────
    //
    // Ft8Ablauf rechnet nur: er sagt, was im naechsten Fenster zu
    // senden WAERE. Gesendet wird hier — mit Bandplanpruefung und
    // Tasten, wie bei jedem anderen Druck auf „Send" auch.
    //
    // Ein Doppelklick auf einen Fund nimmt das QSO an, und ab da
    // laeuft es bis 73 von selbst. Betreiber am 2026-10-22: „wichtig
    // ist auch, dass nach der annahme des qso das qso automatisch
    // anlaeuft bis 73."
    //
    // Bis dahin sass davor der Schalter „Auto", und er stand aus:
    // ein Doppelklick legte die Antwort nur ins Feld. Das war
    // derselbe Fehlgriff wie beim CQ-Knopf (#315) -- ein Knopf, der
    // nicht tut, was er verspricht. Der Schalter ist weg; angehalten
    // wird mit „Stop", und der haelt jetzt auch den Ablauf an.
    Ft8Ablauf    m_ablauf;
    /// Ein QSO wird genau EINMAL gemeldet. zeigeAblauf() laeuft bei
    /// jedem Schritt, und der Zustand bleibt auf Fertig stehen — ohne
    /// diese Wache stuende dasselbe QSO mehrfach im Logbuch.
    bool         m_qsoGemeldet{false};
    /// Der zuletzt mit `partnerGewechselt` gemeldete Partner. Ohne ihn
    /// ginge das Signal bei jedem Zug hinaus, nicht nur beim Wechsel.
    QString      m_gemeldeterPartner;
    /// Laeuft der Ruf vom CQ-Knopf? Dann geht es nach einem fertigen
    /// QSO von selbst weiter. Ein Doppelklick in die Fundliste ist
    /// eine Einzelentscheidung und setzt das zurueck, „Stop“ ebenso.
    bool         m_cqAutomatik{false};
    /// „CQ": ruft CQ und faengt damit den Ablauf als ANRUFER an — bis zum 2026-10-16 war diese Haelfte
    /// von Ft8Ablauf gebaut und geprueft, aber von nirgends
    /// erreichbar: starteAlsAnrufer() hatte keinen einzigen Aufrufer.
    QPushButton* m_cqRufenBtn{nullptr};

    /// Wie oft ein CQ-Ruf von selbst wiederholt wird, und wie viele
    /// Versuche noch offen sind.
    ///
    /// Betreiber am 2026-10-22: „der cq ruf funktioniert nicht" (er
    /// fuellte nur das Textfeld) und „man soll auch entscheiden
    /// koennen, wie oft er das automatisch probiert".
    class QSpinBox* m_cqVersucheSpin{nullptr};
    int m_cqVersucheOffen{0};
    void fensterZuEnde();

    /// CQ und Send zeigen, was gerade laeuft — nicht, was zuletzt
    /// geklickt wurde.
    void zeigeKnopfzustand();
    /// Einen CQ-Ruf anfangen. Eine Stelle fuer den Knopf UND fuer den
    /// Wiederanlauf nach einem fertigen QSO.
    void starteCqRuf();
    /// Derselbe Anfang, OHNE zu senden -- fuer den Aufrufer, der schon
    /// mitten in einem Sendefenster steht und den Zug selbst packt.
    void beginneCqRuf();
    /// Nach einem fertigen QSO auf einem Platz: der CQ-Ruf zaehlt von
    /// vorn. Sendet nicht.
    void frischeCqRufAuf();
    void meldeFertigesQso();
    void ablaufSchritt();
    void zeigeAblauf();
    int          m_quelleVorher{-1};   ///< MicSource, zum Zuruecknehmen
};

} // namespace Longpath
