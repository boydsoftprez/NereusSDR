#pragma once

// =================================================================
// src/gui/instruments/FrequencyInstrument.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Die Frequenz in der Handschrift der Instrumente ──────────────────
//
// OE5SOS, 2026-08-16, Entwurf ~/Downloads/frequenz-im-zeigerstil.html.
// Drei Varianten, im Widget umschaltbar:
//
//   A Bandstreifen — die Rille gerade gezogen, Verlauf bis zur
//     aktuellen Stelle im Band, Bandkanten als rote Striche, die
//     naechstgelegene Beschriftung hell.
//   B Flacher Bogen — derselbe Bogen wie die Instrumente, sehr weit
//     und flach, Marke auf der Frequenz.
//   C Nur die Zahl — Glut darunter, sonst nichts.
//
// Mulde, Verlauf und Glut kommen aus InstrumentPainter, die Geometrie
// aus LinearSpine bzw. ArcSpine. Dieselben drei Mittel wie beim Zeiger
// und beim Balken; hier steht nichts davon ein zweites Mal.
//
// ── Es ZEIGT nicht nur, es BEDIENT ──────────────────────────────────
//
// Auflage des Betreibers, 2026-08-17: „Das Frequenz-Widget muss Rad
// und Klick-Eingabe tragen, nicht nur anzeigen — sonst nimmt Punkt 3
// das Abstimmen weg."
//
// Also: jede Ziffer eine eigene Trefferflaeche. Mausrad darueber dreht
// DIESE Dekade. Doppelklick auf die Zahl oeffnet ein Eingabefeld.
// Die beiden Hertz-Stellen stehen matt, weil sie beim Abstimmen
// dauernd laufen.
//
// ── Bandkanten ──────────────────────────────────────────────────────
//
// Rot, und zwar kraeftig. CLAUDE.local.md: „Die Bandkante bleibt rot.
// Sie ist keine dezente Markierung und faellt nicht unter die
// Zwei-Prozent-Regel des Hausstils." Die Grenzen kommen aus
// AmateurBands::containing() — dieselbe Tabelle, die auch der
// SWR-Durchlauf benutzt.
//
// Der Guard selbst wird hier NICHT umgangen und auch nicht nachgebaut:
// dieses Widget aendert die Frequenz ueber dasselbe Signal wie jede
// andere Abstimmung. Stellenweises Drehen fuehrt so wenig ueber eine
// Bandkante wie das Rad auf der Flagge.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QList>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QLineEdit;
class QResizeEvent;
class QSpacerItem;
class QStackedWidget;

namespace Longpath {

class FrequencyInstrument : public QWidget {
    Q_OBJECT

public:
    enum class Form {
        BandStrip,   ///< A — Zahl mit Bandstreifen
        FlatArc,     ///< B — Zahl mit flachem Bogen
        NumberOnly,  ///< C — nur die Zahl, mit Glut
    };

    explicit FrequencyInstrument(QWidget* parent = nullptr);

    void setFrequency(double hz);
    double frequency() const { return m_hz; }

    /// Die zweite Scheibe und welche von beiden aktiv ist — damit Split
    /// ohne ein zweites Feld sichtbar ist.
    void setOtherFrequency(double hz);
    void setActiveIsThis(bool active);

    // ── RIT/XIT in der VFO-Zeile (2026-11-12) ────────────────────────
    //
    // Stellen kann man beide laengst: RxApplet hat seit S2.8 je eine
    // Reihe aus Ein/Aus, "0", Minus/Plus und einem "+0 Hz"-Schild
    // (RxApplet.cpp:926ff und 995ff), live am SliceModel. ABLESEN kann
    // man sie nur DORT -- wer das RX-Applet nicht offen hat, sieht
    // einen eingeschalteten Versatz nirgends. Genau das ist die Falle:
    // RIT bleibt an, die grossen Ziffern zeigen weiter die VFO-Zahl,
    // und der Empfang liegt dauerhaft daneben (SliceModel::
    // effectiveRxFrequency addiert den Versatz still).
    //
    // Darum steht er hier, wo die Frequenz steht -- aber NUR solange
    // er eingeschaltet ist. Ist beides aus, bleibt die Zeile genau so
    // lang wie bisher und kostet keine Breite.
    //
    // Ein Setter fuer alle vier Werte, nicht vier: sie kommen immer
    // zusammen aus demselben SliceModel und sollen die Zeile einmal
    // neu bauen, nicht viermal.
    void setRitXit(bool ritOn, int ritHz, bool xitOn, int xitHz);

    void setForm(Form f);
    Form form() const { return m_form; }

    /// Betreiber 2026-08-30: "A und B sollte man im Frequenzfeld auch
    /// anhacken können" -- dieselbe Ein-/Ausblenden-Idee wie schon bei
    /// den Leistungs-/SWR-Zusatzzeilen (FrequencyApplet::setShowPower/
    /// setShowSwr), nur fuer die VFO-Zeile selbst. Vorgabe true: bisher
    /// stand sie immer da, ein stiller Wechsel der Vorgabe waere fuer
    /// bestehende Profile ein unangekuendigtes Verschwinden.
    void setVfoRowVisible(bool on);
    bool vfoRowVisible() const;

    // ── Eingetippte Frequenz deuten ──────────────────────────────────
    //
    // Am 2026-08-18 aus VfoWidget herausgeloest, wo der Parser als
    // statische Methode stand und mit 24 Faellen geprueft war. Er MUSS
    // die Flagge ueberleben: commitEdit() rief bis dahin ein blosses
    // toDouble() und nahm MHz an — damit waere „7.230.000" still
    // verworfen worden (zwei Punkte) und „7230" als 7230 MHz gelandet.
    // Das ist wortwoertlich Fehlerbericht #73, den dieser Parser
    // seinerzeit geschlossen hat.
    //
    // Deutet europaeische und amerikanische Tausendertrennung,
    // Dezimalkomma und -punkt, ausgeschriebene und kurze Einheiten
    // (MHz/kHz/Hz, M/k), gross wie klein. Liefert Hz, oder -1 wenn
    // sich nichts Sinnvolles lesen laesst.
    //
    // KLEMMT NICHT: der Aufrufer entscheidet, was ausserhalb des
    // Bereichs geschieht. Ein Parser, der klemmt, kann nicht mehr
    // sagen, ob die Eingabe unsinnig oder nur ausserhalb war.
    static double parseUserFrequency(const QString& raw);

    /// Die Ziffernzeile als Text, aus den echten Schildern in
    /// Layoutfolge gelesen. Fuer den Test, der die GRUPPIERUNG
    /// festnagelt und nicht nur den Wert — die beiden liefen am
    /// 2026-08-18 auseinander.
    QString groupedText() const;

    /// Die VFO-Zeile als Klartext ("A 7.164.781  B 14.074.000", dahinter
    /// der Versatz, falls eingeschaltet). Derselbe Dienst wie
    /// groupedText() fuer die Ziffern: der Pruefstand soll die ZEILE
    /// nageln, nicht das HTML, in dem sie zufaellig gerade steckt.
    QString vfoRowText() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /// Neue Frequenz, vom Rad oder aus dem Eingabefeld. Der Empfaenger
    /// leitet sie auf denselben Weg wie jede andere Abstimmung —
    /// dieses Widget schreibt nichts selbst.
    void frequencyEdited(double hz);

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    bool eventFilter(QObject* watched, QEvent* ev) override;

private:
    void buildDigits();
    /// Wie breit die Ziffernzeile bei einer gegebenen Schriftgroesse
    /// wird -- dieselbe Rechnung wie buildDigits(), nur ohne Schilder
    /// zu bauen. Fuer resizeEvent() (welche Stufe passt?) und
    /// minimumSizeHint() (was braucht die kleinste Stufe wirklich?).
    static int digitRowWidthAt(int px);
    /// Die groesste Stufe zwischen kMinDigitFontPx und kMaxDigitFontPx,
    /// die noch in availableWidth passt.
    static int fittingDigitFontPx(int availableWidth);
    void refreshDigits();
    void refreshVfoRow();
    /// Baut den Klartext der VFO-Zeile; refreshVfoRow() faerbt ihn.
    QString buildVfoRowPlain() const;
    void beginEdit();
    void commitEdit();

    /// Anteil der Frequenz im aktuellen Band, oder -1 ausserhalb.
    double bandFraction() const;

    double m_hz{0.0};
    double m_otherHz{0.0};
    bool   m_activeIsThis{true};
    bool   m_ritOn{false};
    int    m_ritHz{0};
    bool   m_xitOn{false};
    int    m_xitHz{0};
    // War Form::BandStrip. Betreiber 2026-08-30, zum Streifen unter der
    // Zahl (der seit heute erst sichtbar wurde): "MHZ Balken benätige
    // ich nie, löschen!" -- die blosse Zahl ist die Vorgabe, keine
    // Bandlage-Grafik mehr darunter.
    Form   m_form{Form::NumberOnly};

    QWidget*        m_digitRow{nullptr};
    QHBoxLayout*    m_digitLayout{nullptr};
    QList<QLabel*>  m_digits;      ///< nur die Ziffern, in Anzeigefolge
    QList<double>   m_decades;     ///< Hz je Ziffer, gleiche Reihenfolge
    QStackedWidget* m_stack{nullptr};
    QLineEdit*      m_edit{nullptr};
    QLabel*         m_vfoRow{nullptr};
    /// Reserviert im eigenen Layout (root) den Platz fuer Streifen/
    /// Bogen -- Groesse haengt von m_form ab, siehe setForm() und
    /// stripReserveFor() in der .cpp. Eigentum bleibt bei root (Qt-
    /// Layouts loeschen ihre Items selbst); dieser Zeiger dient nur
    /// dazu, die Groesse spaeter noch aendern zu koennen.
    QSpacerItem*    m_stripSpacer{nullptr};

    /// Betreiber, 2026-08-30: „der inhalt im frequenz window sollte
    /// sich besser anpassen" -- ein abgeloestes Fenster liess sich
    /// schmaler ziehen, als die Ziffernzeile bei fester Schriftgroesse
    /// (Style::kFontDisplay) brauchte, und schnitt „MHz" zu „MH" ab.
    /// Diese Stufe faellt jetzt mit der Fensterbreite, haelt aber nie
    /// die Lesbarkeit der Uhr-Zone (siehe kMinDigitFontPx) und waechst
    /// nie ueber die urspruengliche Vorgabe hinaus (kMaxDigitFontPx).
    int m_digitFontPx{26};   // == kMaxDigitFontPx, siehe dort
    // Betreiber, 2026-08-30, nach dem ersten Versuch: "das fenster mit
    // der frequenz sollte man noch kleiner ziehen können." 20px war zu
    // grosszuegig als Bodenwert -- 12px ist die Grenze, ab der die
    // Ziffernbreite (kDigitPadOfCell etc.) noch eine positive Zelle
    // ergibt; darunter reisst die Trefferflaeche pro Ziffer ab
    // (d->setFixedWidth(cell + digitPad) wuerde auf 0 oder negativ
    // fallen).
    static constexpr int kMinDigitFontPx = 12;
    // War 38 (== Style::kFontDisplay). Betreiber 2026-08-30, per
    // Entwurfsblatt entschieden ("C -- Ziffern kleiner, Streifen
    // bleibt" von drei Varianten, siehe FrequencyInstrument::sizeHint):
    // die Zahl darf kleiner sein als die grosse Kopfzeile eines
    // eigenstaendigen Zeigerinstruments -- hier steht sie in einem
    // knappen Schwebefenster, meist neben SWR/Leistung.
    static constexpr int kMaxDigitFontPx = 26;

    /// Acht Ziffern: 7.139.700 — zehn MHz bis Einer-Hertz.
    static constexpr int kDigitCount = 8;
};

} // namespace Longpath
