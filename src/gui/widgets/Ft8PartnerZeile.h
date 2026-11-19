#pragma once

// =================================================================
// src/gui/widgets/Ft8PartnerZeile.h  (Longpath)
// =================================================================
//
// Die Zeile unter „QSO with" in der QSO-Karte des FT8-Dekoders: wer
// der Partner ist und wie es bei ihm aussieht.
//
//   Jan Kowalski · Tarnów · class A · KO00 · 647 km · 61° · ☾ night there
//
// Longpath-original. Vorbild ist die Rufzeichenkarte neben Zeus' FT8-
// Feld (Video KB2UKA, 2026-11-15). Von drei Entwurfsblaettern (Zeile,
// kleines Foto, Spalte) ist es die Zeile geworden: das grosse Foto
// steht seit Stufe 1 ohnehin in der Rotor/Log-Karte, die dem Partner
// folgt, und die Zeile kostet die Fundliste nur eine Schriftzeile.
//
// ── Was ohne Netz dasteht ──────────────────────────────────────────
//
// Locator (aus seiner Nachricht), Entfernung, Richtung und das Licht
// bei ihm (Grauzone.h, gerechnet). Name, Ort und Klasse kommen von
// QRZ -- und zwar nicht von hier: siehe RufzeichenAuskunft.h.
//
// ── Wenn der Platz nicht reicht ────────────────────────────────────
//
// Es faellt in dieser Reihenfolge weg: der Hinweis auf das QRZ-Konto,
// der Ort, die Klasse, der Weg. Name und Licht bleiben am laengsten:
// das eine sagt, mit wem, das andere, ob die Verbindung gleich kippt.
// Abgeschnitten mit „…" wird erst, wenn auch dann noch nichts passt.
//
// Die Zeile bleibt auch ohne Partner stehen (leer). Sonst wuerde die
// Fundliste bei jedem CQ und jedem neuen QSO um eine Zeile springen.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-18 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include "core/Grauzone.h"
#include "core/RufzeichenAuskunft.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

namespace Longpath {

class Ft8PartnerZeile : public QWidget
{
    Q_OBJECT
public:
    struct Inhalt {
        QString rufzeichen;     // leer = kein Partner, die Zeile ist leer
        QString name;
        QString ort;
        QString klasse;
        QString locator;
        int     km{-1};
        int     grad{-1};
        Grauzone::Licht licht{Grauzone::Licht::Unbekannt};
        RufzeichenAuskunft::Stand qrz{RufzeichenAuskunft::Stand::Unbekannt};

        bool operator==(const Inhalt& o) const
        {
            return rufzeichen == o.rufzeichen && name == o.name
                && ort == o.ort && klasse == o.klasse
                && locator == o.locator && km == o.km && grad == o.grad
                && licht == o.licht && qrz == o.qrz;
        }
        bool operator!=(const Inhalt& o) const { return !(*this == o); }
    };

    explicit Ft8PartnerZeile(QWidget* parent = nullptr);

    void setzeInhalt(const Inhalt& inhalt);
    const Inhalt& inhalt() const { return m_inhalt; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    /// Was bei der jetzigen Breite WIRKLICH dasteht, Stueck fuer Stueck
    /// (ohne Trennpunkte). Fuer Pruefstaende: „steht der Name da" ist
    /// eine Frage an die gemalte Zeile, nicht an den Inhalt.
    QStringList sichtbareStueckeForTest() const;

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    enum class Art { Text, Warten, Licht, Hinweis };
    struct Stueck {
        Art         art{Art::Text};
        QString     text;
        const char* farbe{nullptr};
        int         rang{0};       // hoeher = bleibt laenger stehen
        bool        fett{false};
    };

    QVector<Stueck> alleStuecke() const;
    QVector<Stueck> passendeStuecke(int breite) const;
    int  breiteVon(const Stueck& s) const;
    int  trennerBreite() const;
    void maleLicht(class QPainter& p, const QRect& r) const;

    Inhalt m_inhalt;
};

} // namespace Longpath
