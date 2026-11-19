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
#include <QObject>
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

    /// Der DAX-Kanal, den die Lauschscheibe belegt.
    static constexpr int kDaxKanal = 1;

    explicit FlexLauschscheibe(Sende sende, QObject* parent = nullptr);

    // ── Was die Klasse von RadioModel wissen muss ──
    void setHauptscheibe(const QString& id);
    void setPanadapter(const QString& panId);
    void setHauptfrequenzHz(double hz);
    /// Die Kennung, die die Maschine UNS als Client gegeben hat
    /// (`H<hex>` beim Verbinden). Daran erkennt sie eine Lauschscheibe,
    /// die von einer frueheren Sitzung uebrig ist.
    void setEigenerHandle(quint32 handle);

    /// Lauschen auf `hz` (Wahlfrequenz, DIGU); 0 heisst: abbauen.
    /// Steht die Verbindung noch nicht (Haupt­scheibe oder Panadapter
    /// unbekannt), wird es nachgeholt, sobald beide da sind.
    void setFrequenzHz(double hz);

    /// Strom und Scheibe auf der Maschine entfernen. Fuer das Trennen,
    /// solange der Befehlskanal noch steht.
    void abbauen();
    /// Alles vergessen, ohne zu senden (der Kanal ist weg).
    void vergiss();

    /// Gehoert diese Scheibenmeldung der Lauschscheibe?
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
    /// Ist das die Stromkennung des DAX-Tons der Lauschscheibe?
    bool istLauschstrom(quint32 streamId) const;
    /// Ton des DAX-Stroms ist angekommen. Schreibt EINMAL je Strom eine
    /// Zeile ins Protokoll: sie beantwortet am Geraet die offene Frage,
    /// ob DAX bei `audio_mute=1` weiter Ton liefert.
    void tonAngekommen(int werte);

    /// Eine Zustandszeile der Maschine. Gibt true zurueck, wenn sie der
    /// Lauschscheibe gehoerte -- dann darf sie KEIN anderer Weg sehen.
    bool nimmStatus(const QString& object, const QMap<QString, QString>& kv);

    Zustand zustand() const { return m_zustand; }
    int scheibe() const { return m_scheibe; }
    quint32 strom() const { return m_strom; }
    /// Die Frequenz, die die Maschine fuer die Lauschscheibe meldet
    /// (0, solange keine gemeldet ist).
    double gemeldeteFrequenzHz() const { return m_gemeldetHz; }
    /// Die Frequenz, auf die gelauscht werden soll (0 = aus).
    double gewuenschteFrequenzHz() const { return m_gewuenschtHz; }

signals:
    void zustandGeaendert();
    void frequenzGemeldet(double hz);
    /// Die Maschine hat das Anlegen verweigert (z. B. 0x50000003, „all
    /// slice receivers in use"). `grund` ist fuer das Protokoll.
    void abgelehnt(const QString& grund);

private:
    bool bereit() const;
    void legeAn();
    void richteEin();
    void stimme(double hz);
    void setZustand(Zustand z);

    Sende   m_sende;
    QString m_hauptId;
    QString m_panId;
    double  m_hauptHz{0.0};

    Zustand m_zustand{Zustand::Aus};
    int     m_scheibe{-1};
    quint32 m_strom{0};
    double  m_gewuenschtHz{0.0};
    double  m_gemeldetHz{0.0};
    bool    m_abbauNachAnlegen{false};
    quint32 m_tonGemeldetFuer{0};

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
