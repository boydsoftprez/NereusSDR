#pragma once

// =================================================================
// src/gui/widgets/Ft8DannListe.h  (Longpath)
// =================================================================
//
// „THEN" in der QSO-Karte des FT8-Dekoders: wer nach dem laufenden
// QSO drankommt, in welcher Reihenfolge, und wie lange sein Anruf
// noch gilt.
//
// Longpath-original. Der Anlass ist ein Video von ZeusSDR (KB2UKA,
// 2026-11-15): dort steht die „Caller Queue" als eigene Spalte mit
// ↑ ↓ ✕. Longpath hatte die Schlange schon seit dem 2026-10-30
// (`Ft8Warteschlange`), sie arbeitete auch -- nur sah man sie nicht:
// im Kopf der Fundliste stand „2 waiting", in Schrift von acht
// Punkten. Betreiber nach drei Entwurfsblaettern: „C bauen" -- die
// Liste gehoert unter das laufende QSO, nicht daneben.
//
// ── Was hier NICHT entschieden wird ─────────────────────────────────
//
// Das Bauteil zeigt nur und meldet Handgriffe. Wer drankommt, wie
// lange ein Anruf gilt, was ein ✕ bedeutet: das steht in
// `Ft8Warteschlange`, und der Dekoder (`Ft8Applet`) verbindet beides.
// So kann es keine zweite Meinung ueber die Reihenfolge geben.
//
// ── Die Zeilen bleiben stehen ───────────────────────────────────────
//
// Das Alter laeuft jede Sekunde weiter. Wuerden die Zeilen dafuer
// neu gebaut, verloere die Zeile unter der Maus bei jedem Takt ihr
// ↑ ↓ ✕ -- man griffe ins Leere. Darum werden vorhandene Zeilen nur
// neu beschriftet.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-15 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;

namespace Longpath {

class Ft8DannListe : public QWidget
{
    Q_OBJECT

public:
    struct Eintrag {
        QString partner;
        QString land;
        int     snr{0};
        QString locator;
        int     tonHz{-1};
        qint64  alterMs{0};     ///< seit er zuletzt gehoert wurde
        bool    vonHand{false}; ///< aus der Fundliste angestellt
    };

    /// So viele Zeilen stehen ohne Aufklappen da. Mehr kosten der
    /// Fundliste Hoehe, und drei reichen fuer „wer kommt als
    /// naechstes" -- der Rest steht hinter „+N more".
    static constexpr int kSichtbar = 3;

    /// Ab hier wird ein Anruf blass: die letzte Minute seiner fuenf
    /// (`Ft8Warteschlange::kHaltbarkeitMs`).
    static constexpr qint64 kBlassAbMs = 4 * 60 * 1000;

    explicit Ft8DannListe(QWidget* parent = nullptr);
    ~Ft8DannListe() override;

    /// `automatisch`: nach dem QSO geht es von selbst weiter (der
    /// CQ-Knopf hat den Lauf begonnen). Sonst nimmt man den naechsten
    /// mit Doppelklick -- und der Hinweis sagt das.
    void setzeEintraege(const QVector<Eintrag>& eintraege, bool automatisch);

    // ── Fuer Pruefstaende ───────────────────────────────────────────
    int          sichtbareZeilenForTest() const;
    QString      rufInZeileForTest(int zeile) const;
    QWidget*     zeileForTest(int zeile) const;
    /// 0 = ↑, 1 = ↓, 2 = ✕
    QToolButton* knopfForTest(int zeile, int welcher) const;
    QPushButton* mehrKnopfForTest() const { return m_mehr; }
    QLabel*      hinweisForTest() const { return m_hinweis; }

signals:
    void hoch(const QString& partner);
    void runter(const QString& partner);
    void weg(const QString& partner);
    /// Doppelklick auf eine Zeile.
    void gewaehlt(const QString& partner);

private:
    class Zeile;

    void zeigeZeilen();

    QVBoxLayout*     m_zeilenLayout{nullptr};
    QLabel*          m_kopf{nullptr};
    QLabel*          m_hinweis{nullptr};
    QPushButton*     m_mehr{nullptr};
    QVector<Zeile*>  m_zeilen;
    QVector<Eintrag> m_eintraege;
    bool             m_automatisch{true};
    bool             m_ausgeklappt{false};
};

}  // namespace Longpath
