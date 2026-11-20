#pragma once

// =================================================================
// src/core/FlexLauschscheibe.h  (Longpath)
// =================================================================
// no-port-check: Longpath-eigen. Der Befehlsweg fuer den DAX-Ton ist
// AetherSDR abgeschaut (RadioModel.cpp:1522 [@d58e2b8a]:
// `stream create type=dax_rx dax_channel=<ch>`), die Logik hier ist
// eigen.
//
// FT8 auf zwei Baendern an der FLEX-8400 (2026-11-19).
//
// Betreiber: „ich will 2 bänder, mehr macht derzeit mein trx nicht,
// gleichzeitig empfangen umd ggf. eine dxcc station zu hören."
//
// Die Maschine hat zwei Scheiben (`max_slices=2` im Erkennungsruf,
// gemessen am 2026-11-19). Eine fuehrt der Betreiber, die zweite
// legt diese Klasse an: die LAUSCHSCHEIBE. Sie hoert nur, sie sendet
// nie, und Longpath zeigt sie nirgends als Scheibe -- ihr Ton geht
// allein in den zweiten FT8-Dekoder.
//
// ── Wie sie zu ihrem eigenen Ton kommt ───────────────────────────────
//
// Der Tonstrom, den Longpath sonst hoert (`remote_audio_rx`), ist der
// Mix ALLER Scheiben -- das, was aus dem Lautsprecher kaeme. Einen
// Strom je Scheibe gibt es nur ueber DAX:
//
//     slice set <n> dax=1                       (SmartSDR-API, TCPIP slice:
//                                                „DAX channel for this
//                                                slice; 0 = none")
//     stream create type=dax_rx dax_channel=1   (TCPIP stream)
//
// Die Pakete kommen am selben UDP-Port an wie der Mix, mit eigener
// Stromkennung und derselben Klasse 0x03E3 (float32 Stereo, 24 kHz).
// `istLauschstrom()` trennt sie.
//
// Damit die Lauschscheibe NICHT im Lautsprecher landet, steht sie auf
// `audio_mute=1`. Ob DAX dann weiter Ton liefert, ist am Geraet noch
// nicht belegt (erwartet: ja, DAX liegt vor dem Ton-Regler).
//
// ── Warum sie sich nie fuer die Hauptscheibe halten darf ─────────────
//
// RadioModel ordnete bisher JEDE Scheibenmeldung der einen Scheibe zu:
// die Frequenz von Scheibe 1 haette den VFO der Hauptscheibe auf
// 28,074 MHz gestellt. `istLauschscheibe()` ist die eine Pruefung, die
// RadioModel VOR allen anderen Scheibenwegen fragt. Waehrend des
// Anlegens, wenn die Nummer noch nicht zurueck ist, gilt jede Scheibe
// ausser der Hauptscheibe als Lauschscheibe -- die Maschine meldet
// den Zustand einer neuen Scheibe gern VOR der Antwort (gemessen am
// 2026-10-18 fuer die erste Scheibe).
//
// ── Die Maschine merkt sich die Lauschscheibe (2026-11-19, gemessen) ──
//
// Die Flex legt einem GUI-Client beim Anmelden seine Scheiben unter
// derselben Kennung wieder hin -- auch nach einem Neustart der
// Maschine, und auch, wenn beim Beenden `slice remove` geschickt wurde.
// Am Geraet: nach dem Neustart stand wieder `slice 1 RF=21.074
// audio_mute=1 tx=0`, und weil Longpath an diesem Tag noch JEDE
// Scheibenmeldung fuer die eigene hielt, zog sie den VFO und den
// Panadapter auf 21,074 MHz, waehrend die Sendescheibe 0 auf 14,074
// stand. Darum: eine Scheibe UNSERES Clients, die nicht die
// Hauptscheibe ist und nicht sendet, ist eine uebriggebliebene
// Lauschscheibe. Sie geht nie an den VFO; wird gelauscht, wird sie
// uebernommen, sonst entfernt.
//
// ── Warum sie auf der Frequenz der Hauptscheibe entsteht ─────────────
//
// `slice create` traegt eine Frequenz, und eine, die ausserhalb des
// Panadapters liegt, koennte ihn verschieben -- dann spraenge das
// Bild der Hauptscheibe auf 10 m. Also entsteht sie IM Panadapter (auf
// der Frequenz der Hauptscheibe) und wird danach mit `autopan=0`
// weggestimmt; `autopan=0` ist derselbe Zusatz, mit dem Longpath die
// Hauptscheibe stimmt (FlexRadioConnection.cpp: tuneSliceCommand).
// =================================================================

#include <QMap>
#include <QElapsedTimer>
#include <QObject>
#include <QVector>
#include <QSet>
#include <QString>

#include <functional>

namespace Longpath {

class FlexLauschscheibe : public QObject {
    Q_OBJECT

public:
    using Antwort = std::function<void(quint32 code, const QString& body)>;
    /// Schickt einen Befehl; `antwort` darf leer sein.
    using Sende = std::function<void(const QString& befehl, Antwort antwort)>;

    enum class Zustand { Aus, Anlegen, Laeuft };

    /// Hoechstens so viele Lauschbaender (2026-11-19). Die FLEX-8400 hat
    /// zwei Scheiben, also einen Platz; die 8600 hat vier, also drei.
    /// Wie viele gelten, sagt `setPlaetze` (aus `max_slices` - 1).
    static constexpr int kMaxPlaetze = 3;
    /// Platz k belegt DAX-Kanal k + 1.
    static int daxKanal(int platz) { return platz + 1; }

    explicit FlexLauschscheibe(Sende sende, QObject* parent = nullptr);

    // ── Was die Klasse von RadioModel wissen muss ──
    void setHauptscheibe(const QString& id);
    void setPanadapter(const QString& panId);
    void setHauptfrequenzHz(double hz);
    /// Woher die Mitte des Panadapters kommt, in Hertz (0 = unbekannt).
    /// Dort entsteht die Lauschscheibe -- nicht auf der zuletzt
    /// BESTAETIGTEN Frequenz der Hauptscheibe: die Maschine echot eigene
    /// Abstimmbefehle oft erst eine Minute spaeter (gemessen 2026-11-19
    /// 18:29:01 -> 18:29:57), und eine Scheibe ausserhalb des
    /// Panadapters koennte ihn mitziehen.
    void setPanMitteQuelle(std::function<double()> quelle);
    /// Die Kennung, die die Maschine UNS als Client gegeben hat
    /// (`H<hex>` beim Verbinden). Daran erkennt sie eine Lauschscheibe,
    /// die von einer frueheren Sitzung uebrig ist.
    void setEigenerHandle(quint32 handle);
    /// Wie viele Lauschplaetze das Geraet hergibt (0..kMaxPlaetze).
    void setPlaetze(int anzahl);
    int plaetze() const { return m_plaetzeAnzahl; }

    /// Auf Platz `platz` auf `hz` lauschen (Wahlfrequenz, DIGU); 0 heisst:
    /// abbauen. Steht die Verbindung noch nicht (Hauptscheibe oder
    /// Panadapter unbekannt), wird es nachgeholt, sobald beide da sind.
    void setFrequenzHz(int platz, double hz);
    /// Platz 0 -- die Form von #496, eine Lauschscheibe.
    void setFrequenzHz(double hz) { setFrequenzHz(0, hz); }

    /// Alle Stroeme und Scheiben auf der Maschine entfernen. Fuer das
    /// Trennen, solange der Befehlskanal noch steht.
    void abbauen();
    /// Alles vergessen, ohne zu senden (der Kanal ist weg).
    void vergiss();

    /// Gehoert diese Scheibenmeldung einer Lauschscheibe?
    bool istLauschscheibe(int sliceId) const;
    /// Lauschscheibe ODER eine uebriggebliebene aus einer frueheren
    /// Sitzung: in beiden Faellen gehoert ihre Frequenz nie an den VFO.
    bool istFremd(int sliceId) const;
    /// Darf die Frequenz dieser Scheibe an den VFO? Ist die Hauptscheibe
    /// bekannt, nur sie; vorher keine, neben der eine andere sendet.
    bool darfAnDenVfo(int sliceId) const;
    /// Kann diese Scheibe NICHT die Hauptscheibe sein, weil eine andere
    /// nachweislich sendet (oder weil sie eine Lausch-/Restscheibe ist)?
    bool istNichtHaupt(int sliceId) const;
    /// Zu welchem Platz gehoert dieser DAX-Strom? -1: zu keinem.
    int platzFuerStrom(quint32 streamId) const;
    bool istLauschstrom(quint32 streamId) const { return platzFuerStrom(streamId) >= 0; }
    /// Ton eines DAX-Stroms ist angekommen: einmal je Strom eine Zeile
    /// ins Protokoll, dazu einmal je Minute der Pegel.
    void tonAngekommen(int platz, QVector<float>& werte);
    void tonAngekommen(QVector<float>& werte) { tonAngekommen(0, werte); }
    /// Unbrauchbare Proben (NaN, unendlich, weit ueber Vollaussteuerung)
    /// auf 0 setzen. Gibt zurueck, wie viele es waren; `groesster` die
    /// groesste verworfene Betragszahl.
    ///
    /// Am Geraet, 2026-11-20 10:32: in der ersten Minute nach dem Anlegen
    /// meldete der Pegel „Spitze 708.3 dBFS" -- einzelne Werte um 1e35,
    /// danach wieder normal. Eine solche Probe wird beim Quadrieren in der
    /// FFT unendlich und kostet den Dekoder ein ganzes 15-s-Fenster.
    static int bereinigeTon(QVector<float>& werte, float* groesster = nullptr);
    /// Ab hier gilt eine Probe als unbrauchbar: +18 dBFS. Der Flex-Ton ist
    /// float32 um ±1.0; was darueber liegt, ist kein Ton.
    static constexpr float kTonGrenze = 8.0f;

    /// Eine Zustandszeile der Maschine. Gibt true zurueck, wenn sie einer
    /// Lauschscheibe gehoerte -- dann darf sie KEIN anderer Weg sehen.
    bool nimmStatus(const QString& object, const QMap<QString, QString>& kv);

    Zustand zustand(int platz = 0) const;
    int scheibe(int platz = 0) const;
    quint32 strom(int platz = 0) const;
    /// Die Frequenz, die die Maschine fuer die Lauschscheibe meldet
    /// (0, solange keine gemeldet ist).
    double gemeldeteFrequenzHz(int platz = 0) const;
    /// Die Frequenz, auf die gelauscht werden soll (0 = aus).
    double gewuenschteFrequenzHz(int platz = 0) const;

signals:
    void zustandGeaendert();
    void frequenzGemeldet(int platz, double hz);
    /// Die Maschine hat das Anlegen verweigert (z. B. 0x50000003, „all
    /// slice receivers in use"). `grund` ist fuer das Protokoll.
    void abgelehnt(int platz, const QString& grund);

private:
    struct Platz {
        Zustand zustand{Zustand::Aus};
        int     scheibe{-1};
        quint32 strom{0};
        double  gewuenschtHz{0.0};
        double  gemeldetHz{0.0};
        bool    abbauNachAnlegen{false};
        quint32 tonGemeldetFuer{0};
        // Pegel des DAX-Tons, einmal je Minute ins Protokoll: „0 heard"
        // heisst dann entweder „Band zu" (Pegel da) oder „Strom stumm".
        QElapsedTimer pegelUhr;
        float  pegelSpitze{0.0f};
        double pegelQuadrate{0.0};
        qint64 pegelWerte{0};
        qint64 verworfen{0};       ///< seit der letzten Pegelzeile
        float  verworfenGroesster{0.0f};
        qint64 werteSeitStart{0};  ///< wo im Strom die erste kaputte lag
        bool   kaputtGemeldet{false};
    };

    bool gueltig(int platz) const { return platz >= 0 && platz < kMaxPlaetze; }
    bool bereit() const;
    /// Legt den naechsten wartenden Platz an -- immer nur EINEN zur Zeit:
    /// waehrend des Anlegens ist die neue Scheibe nur daran zu erkennen,
    /// dass sie keine der bekannten ist.
    void legeNaechstenAn();
    void legeAn(int platz);
    void richteEin(int platz);
    void stimme(int platz, double hz);
    void baueAb(int platz);
    void setZustand(int platz, Zustand z);
    int platzFuerScheibe(int sliceId) const;

    Sende   m_sende;
    QString m_hauptId;
    QString m_panId;
    double  m_hauptHz{0.0};
    std::function<double()> m_panMitte;
    int     m_plaetzeAnzahl{1};
    int     m_anlegePlatz{-1};   ///< Platz, dessen Nummer noch aussteht
    Platz   m_platz[kMaxPlaetze];

    // ── Uebrig aus einer frueheren Sitzung (2026-11-19, am Geraet) ──
    quint32    m_eigenerHandle{0};
    QSet<int>  m_uebrige;        ///< erkannt, nie an den VFO
    QSet<int>  m_entferntGeschickt;
    QMap<int, bool> m_sendetJe;  ///< zuletzt gemeldetes `tx` je Scheibe
    QSet<int>  m_eigene;         ///< Scheiben unseres Clients, in Gebrauch
    bool istUebrigeKandidat(int id, const QMap<QString, QString>& kv);
    void behandleUebrige(int id);
};

} // namespace Longpath
