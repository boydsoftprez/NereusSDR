// src/gui/instruments/FrequencyInstrument.h

// --- From src/gui/instruments/FrequencyInstrument.h ---
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

// =================================================================
// src/gui/instruments/FrequencyInstrument.h  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/gui/instruments/FrequencyInstrument.h
//   at revision 551576e30c42731c6d51ce005c5f23ec38be391c,
//   https://github.com/oe5sos/Longpath, primary author Martin Fischer
//   (OE5SOS). The upstream banners are copied above.
//
//   Upstream source has no top-of-file copyright or GPL header; the
//   project-level LICENSE (GPLv3) applies.
//
//   Namespace Longpath becomes NereusSDR.
//   Style:: resolves to the Longpath values in WorkspacePalette and
//   WorkspaceStyle through gui/instruments/InstrumentStyle.h, never to
//   NereusSDR::Style in gui/StyleConstants.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07 - Ported in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - setSliceLetters(): the VFO row labels come from the
//                 bound slices' letters; upstream hard-codes A and B.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-10-08 - Adds otherFrequency() and activeIsThis() for tests.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-20 - Display controls for Frequency (Custom): digit size,
//                 colour, centering, and each slice line on its own.
//                 Brian Sutton, AI-assisted via xAI Grok.
// =================================================================

#pragma once

#include <QColor>
#include <QList>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QLineEdit;
class QResizeEvent;
class QSpacerItem;
class QStackedWidget;

namespace NereusSDR {

// From Longpath src/gui/instruments/FrequencyInstrument.h:67 [@551576e3]
class FrequencyInstrument : public QWidget {
    Q_OBJECT

public:
    // From Longpath src/gui/instruments/FrequencyInstrument.h:71 [@551576e3]
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
    // NereusSDR: read back for tests.
    double otherFrequency() const { return m_otherHz; }
    bool activeIsThis() const { return m_activeIsThis; }

    // NereusSDR: the letters on the VFO row, this instrument's slice
    // first, then the other one. Upstream hard-codes "A" and "B", which
    // labels slice B as A when the instrument is bound to slice B. The
    // defaults keep the upstream labels.
    void setSliceLetters(QChar own, QChar other);
    QChar ownLetter() const { return m_ownLetter; }
    QChar otherLetter() const { return m_otherLetter; }

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

    // NereusSDR: Frequency (Custom). Defaults keep the ported instrument:
    // the digits fit the width from 12 px up to 26 px, the amber reading
    // colour, top-left alignment, and both slice lines. setDigitFont()
    // opts into an operator size. fitWidth still shrinks that size down
    // to 12 px; without it the size stays put and the minimum width
    // follows it. An invalid colour keeps the amber reading colours.
    void setDigitFont(int fontPx, bool fitWidth);
    bool digitFontLocked() const { return m_fontLocked; }
    int digitFontPx() const { return m_digitFontPx; }
    void setDigitColor(const QColor& color);
    QColor digitColor() const { return m_digitColor; }
    void setContentCentered(bool on);
    bool contentCentered() const { return m_centered; }
    // The two lines under the digits: this slice, then the other one.
    void setSliceLinesVisible(bool own, bool other);
    bool ownSliceLineVisible() const { return m_showOwn; }
    bool otherSliceLineVisible() const { return m_showOther; }

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
    // NereusSDR: Frequency (Custom). The ceiling is kMaxDigitFontPx, or the
    // operator size when that size may still shrink to the width.
    int fittingDigitFontPx(int availableWidth) const;
    QString digitBrightColor() const;
    QString digitDimColor() const;
    void applyEditStyle();
    void refreshDigits();
    void refreshVfoRow();
    void beginEdit();
    void commitEdit();

    /// Anteil der Frequenz im aktuellen Band, oder -1 ausserhalb.
    double bandFraction() const;

    double m_hz{0.0};
    double m_otherHz{0.0};
    bool   m_activeIsThis{true};
    // NereusSDR: see setSliceLetters().
    QChar  m_ownLetter{QLatin1Char('A')};
    QChar  m_otherLetter{QLatin1Char('B')};
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
    // NereusSDR: Frequency (Custom). Locked ignores the width. A ceiling
    // above zero replaces kMaxDigitFontPx while the width may still shrink
    // the digits. An invalid colour keeps the amber reading colours.
    bool m_fontLocked{false};
    int m_fontCeiling{0};
    QColor m_digitColor;
    bool m_centered{false};
    bool m_showOwn{true};
    bool m_showOther{true};
    // From Longpath src/gui/instruments/FrequencyInstrument.h:195 [@551576e3]
    // Betreiber, 2026-08-30, nach dem ersten Versuch: "das fenster mit
    // der frequenz sollte man noch kleiner ziehen können." 20px war zu
    // grosszuegig als Bodenwert -- 12px ist die Grenze, ab der die
    // Ziffernbreite (kDigitPadOfCell etc.) noch eine positive Zelle
    // ergibt; darunter reisst die Trefferflaeche pro Ziffer ab
    // (d->setFixedWidth(cell + digitPad) wuerde auf 0 oder negativ
    // fallen).
    static constexpr int kMinDigitFontPx = 12;
    // From Longpath src/gui/instruments/FrequencyInstrument.h:202 [@551576e3]
    // War 38 (== Style::kFontDisplay). Betreiber 2026-08-30, per
    // Entwurfsblatt entschieden ("C -- Ziffern kleiner, Streifen
    // bleibt" von drei Varianten, siehe FrequencyInstrument::sizeHint):
    // die Zahl darf kleiner sein als die grosse Kopfzeile eines
    // eigenstaendigen Zeigerinstruments -- hier steht sie in einem
    // knappen Schwebefenster, meist neben SWR/Leistung.
    static constexpr int kMaxDigitFontPx = 26;

    // From Longpath src/gui/instruments/FrequencyInstrument.h:205 [@551576e3]
    /// Acht Ziffern: 7.139.700 — zehn MHz bis Einer-Hertz.
    static constexpr int kDigitCount = 8;
};

} // namespace NereusSDR
