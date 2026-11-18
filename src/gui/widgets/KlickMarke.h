#pragma once

// =================================================================
// src/gui/widgets/KlickMarke.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Ein Druck, den man sieht ─────────────────────────────────────────
//
// Betreiber, 2026-11-17: „Das klicken auf Buttons sollte besser farblich
// markiert sein, verwende das Orange wie beim PLUS Zeichen."
//
// Nachgezaehlt am selben Tag: im ganzen `src/gui` stehen 18
// `:pressed`-Regeln, verteilt auf 15 Dateien. Alle uebrigen Knoepfe --
// Band, Betriebsart, Filter, die Leisten, jedes Applet -- antworten auf
// einen Druck gar nicht sichtbar; man sieht erst die Folge, nicht den
// Griff. Bei einem Knopf, dessen Folge am Funkgeraet liegt und einen
// Augenblick braucht, weiss man so lange nicht, ob man getroffen hat.
//
// ── Warum EINE Stelle und kein Stilblatt ─────────────────────────────
//
// Eine anwendungsweite Regel `QPushButton:pressed { … }` waere der
// naheliegende Weg und greift nicht: fast jeder Knopf traegt sein
// eigenes Stilblatt, und in Qt gewinnt bei gleicher Eigenschaft das
// Blatt des Widgets gegen das der Anwendung, gleich wie genau der
// Selektor ist. Uebrig bliebe, an zweihundert Stellen eine Zeile
// nachzutragen -- und die zweihundertunderste zu vergessen.
//
// Darum malt die Marke UEBER den Knopf, statt ihn umzufaerben: ein
// durchsichtiges Kind, fuer die Maus durchlaessig, das beim Druck
// aufleuchtet und beim Loslassen ausklingt. Sie fragt nicht, wie der
// Knopf aussieht, und kein Stilblatt kann sie uebersteuern. Dasselbe
// Muster wie `KlickHoltNachVorne` (WindowPlacement.h): ein Filter auf
// der Anwendung, bei dem alles ausser dem Mausdruck in der ersten Zeile
// durchfaellt.
//
// ── Was eine Marke bekommt ───────────────────────────────────────────
//
// Druckknoepfe: QPushButton, QToolButton und eigene QAbstractButton-
// Klassen. Ankreuzfelder und Wahlknoepfe NICHT -- ihr Haken ist die
// Antwort, und eine Flaeche ueber Text und Kaestchen saehe aus wie eine
// Auswahl. Das Plus selbst auch nicht (Eigenschaft `klickmarke=false`):
// es ist schon orange und antwortet mit seiner eigenen Bewegung.
//
// Die Farbe ist das Orange des Plus (Style::kPlusTop/-Bottom/-Border),
// zu gut drei Vierteln deckend: klar das Orange des Plus, und die
// Beschriftung scheint noch durch.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QHash>
#include <QObject>
#include <QPointer>

class QAbstractButton;
class QWidget;

namespace Longpath {

class KlickMarke : public QObject {
    Q_OBJECT
public:
    /// Haengt sich als Filter an die Anwendung. Ein Stueck je Programm.
    explicit KlickMarke(QObject* eltern = nullptr);

    /// Bekommt dieser Knopf eine Marke? Reine Auskunft, fuer den
    /// Pruefstand und damit die Regel an EINER Stelle steht.
    static bool bekommtMarke(const QAbstractButton* knopf);

    /// Der Knopf, zu dem ein gedruecktes Widget gehoert -- es selbst
    /// oder der naechste Vorfahr (ein Druck landet oft auf einem Kind:
    /// einem Symbol, einer Beschriftung im Knopf). Nullptr, wenn keiner.
    static QAbstractButton* knopfZu(QWidget* getroffen);

    /// Wie lange die Marke nach dem Loslassen ausklingt.
    static constexpr int kAusklangMs = 220;

    /// Fuer Pruefstaende: die Marke, die gerade ueber `knopf` liegt.
    QWidget* markeFuerTest(const QAbstractButton* knopf) const;

protected:
    bool eventFilter(QObject* ziel, QEvent* ereignis) override;

private:
    void druecke(QAbstractButton* knopf);
    void lasseLos(QAbstractButton* knopf);

    // Je Knopf hoechstens eine Marke. QPointer, weil der Knopf (und mit
    // ihm die Marke als sein Kind) jederzeit verschwinden kann.
    QHash<const QAbstractButton*, QPointer<QWidget>> m_marken;
    QPointer<QAbstractButton> m_gedrueckt;
};

} // namespace Longpath
