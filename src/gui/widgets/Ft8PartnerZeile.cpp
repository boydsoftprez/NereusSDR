// =================================================================
// src/gui/widgets/Ft8PartnerZeile.cpp  (Longpath)
// =================================================================
//
// Longpath-original. no-port-check: Longpath-original.
// Siehe Ft8PartnerZeile.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-18 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include "Ft8PartnerZeile.h"

#include "gui/StyleConstants.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Longpath {

namespace {

constexpr int kIcon      = 11;   // Kantenlaenge des Lichtzeichens
constexpr int kIconAbst  = 4;
constexpr int kWarteBalk = 22;   // der ruhige Strich „Abfrage laeuft"
constexpr int kWarteAbst = 6;

QColor farbe(const char* f) { return QColor(QString::fromLatin1(f)); }

QString lichtText(Grauzone::Licht l)
{
    switch (l) {
        case Grauzone::Licht::Tag:      return QStringLiteral("day there");
        case Grauzone::Licht::Grauzone: return QStringLiteral("greyline there");
        case Grauzone::Licht::Nacht:    return QStringLiteral("night there");
        case Grauzone::Licht::Unbekannt: break;
    }
    return {};
}

// Der Hinweis ohne Konto: drei Laeufe, der mittlere in Bernstein.
const QString kHinweisVor  = QStringLiteral("name and photo: ");
const QString kHinweisKern = QStringLiteral("QRZ account");
const QString kHinweisNach = QStringLiteral(" in Tools");

} // namespace

Ft8PartnerZeile::Ft8PartnerZeile(QWidget* parent)
    : QWidget(parent)
{
    // Dieselbe Stufe wie die Zeilen unter „THEN" (Ft8DannListe): eine
    // Stufe unter „QSO with", damit das Rufzeichen die Karte fuehrt.
    QFont f = font();
    f.setPointSize(Style::kFontCaption + 1);
    setFont(f);
    // Die QSO-Karte vererbt „QWidget { Rand; Messingkante }" an jedes
    // Kind (siehe Ft8DannListe). Dieses Bauteil malt selbst und ruft
    // PE_Widget nie -- es bekommt also weder Rand noch Flaeche.
    setAttribute(Qt::WA_StyledBackground, false);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
}

void Ft8PartnerZeile::setzeInhalt(const Inhalt& inhalt)
{
    if (inhalt == m_inhalt) { return; }
    m_inhalt = inhalt;

    // Woher was kommt, fuer den, der es wissen will.
    QStringList woher;
    if (m_inhalt.qrz == RufzeichenAuskunft::Stand::Da) {
        woher << QStringLiteral("Name, place and class: QRZ.com");
    }
    if (!m_inhalt.locator.isEmpty()) {
        woher << QStringLiteral("Grid %1, distance and bearing from your own grid")
                     .arg(m_inhalt.locator);
    }
    if (m_inhalt.licht != Grauzone::Licht::Unbekannt) {
        woher << QStringLiteral("Light: sun height at his end, now (greyline = within 6°)");
    }
    setToolTip(woher.join(QLatin1Char('\n')));
    updateGeometry();
    update();
}

QVector<Ft8PartnerZeile::Stueck> Ft8PartnerZeile::alleStuecke() const
{
    using S = RufzeichenAuskunft::Stand;
    QVector<Stueck> st;
    if (m_inhalt.rufzeichen.isEmpty()) { return st; }

    const auto text = [&st](const QString& t, const char* f, int rang,
                            bool fett = false) {
        if (t.trimmed().isEmpty()) { return; }
        st.append(Stueck{Art::Text, t, f, rang, fett});
    };

    if (m_inhalt.qrz == S::Laeuft) {
        st.append(Stueck{Art::Warten,
                         QStringLiteral("looking up %1…").arg(m_inhalt.rufzeichen),
                         Style::kTextScale, 7, false});
    }
    if (m_inhalt.qrz == S::Da) {
        text(m_inhalt.name, Style::kTextPrimary, 9, true);
        text(m_inhalt.ort, Style::kTextSecondary, 3);
        if (!m_inhalt.klasse.trimmed().isEmpty()) {
            text(QStringLiteral("class %1").arg(m_inhalt.klasse.trimmed()),
                 Style::kTextSecondary, 4);
        }
    }

    if (!m_inhalt.locator.isEmpty()) {
        QString weg = m_inhalt.locator;
        if (m_inhalt.km >= 0) {
            weg += QStringLiteral(" · %1 km").arg(m_inhalt.km);
        }
        if (m_inhalt.grad >= 0) {
            weg += QStringLiteral(" · %1°").arg(m_inhalt.grad);
        }
        text(weg, Style::kTextScale, 6);
    }

    if (m_inhalt.licht != Grauzone::Licht::Unbekannt) {
        st.append(Stueck{Art::Licht, lichtText(m_inhalt.licht),
                         Style::kTextSecondary, 8, false});
    }

    switch (m_inhalt.qrz) {
        case S::KeinKonto:
            st.append(Stueck{Art::Hinweis, kHinweisVor + kHinweisKern + kHinweisNach,
                             Style::kTextInactive, 1, false});
            break;
        case S::NichtGefunden:
            text(QStringLiteral("not in QRZ"), Style::kTextInactive, 2);
            break;
        case S::Fehler:
            text(QStringLiteral("QRZ: no answer"), Style::kTextInactive, 2);
            break;
        case S::Unbekannt:
        case S::Aus:
        case S::Laeuft:
        case S::Da:
            break;
    }
    return st;
}

int Ft8PartnerZeile::trennerBreite() const
{
    return QFontMetrics(font()).horizontalAdvance(QStringLiteral(" · "));
}

int Ft8PartnerZeile::breiteVon(const Stueck& s) const
{
    QFont f = font();
    if (s.fett) { f.setWeight(QFont::DemiBold); }
    const int t = QFontMetrics(f).horizontalAdvance(s.text);
    switch (s.art) {
        case Art::Licht:   return kIcon + kIconAbst + t;
        case Art::Warten:  return kWarteBalk + kWarteAbst + t;
        case Art::Text:
        case Art::Hinweis: break;
    }
    return t;
}

QVector<Ft8PartnerZeile::Stueck> Ft8PartnerZeile::passendeStuecke(int breite) const
{
    QVector<Stueck> st = alleStuecke();
    const int trenner = trennerBreite();
    const auto gesamt = [&]() {
        int w = 0;
        for (int i = 0; i < st.size(); ++i) {
            w += breiteVon(st[i]) + (i > 0 ? trenner : 0);
        }
        return w;
    };
    // Vom niedrigsten Rang her weglassen, bis es passt -- das letzte
    // Stueck bleibt immer (und wird notfalls abgeschnitten).
    while (st.size() > 1 && gesamt() > breite) {
        int weg = 0;
        for (int i = 1; i < st.size(); ++i) {
            if (st[i].rang < st[weg].rang) { weg = i; }
        }
        st.removeAt(weg);
    }
    return st;
}

QStringList Ft8PartnerZeile::sichtbareStueckeForTest() const
{
    QStringList r;
    for (const Stueck& s : passendeStuecke(width())) { r << s.text; }
    return r;
}

QSize Ft8PartnerZeile::sizeHint() const
{
    int w = 0;
    const QVector<Stueck> st = alleStuecke();
    for (int i = 0; i < st.size(); ++i) {
        w += breiteVon(st[i]) + (i > 0 ? trennerBreite() : 0);
    }
    return {std::max(w, 40), QFontMetrics(font()).height() + 2};
}

QSize Ft8PartnerZeile::minimumSizeHint() const
{
    return {40, QFontMetrics(font()).height() + 2};
}

void Ft8PartnerZeile::maleLicht(QPainter& p, const QRect& r) const
{
    const QPointF c = QRectF(r).center();
    const double rad = r.width() * 0.42;
    p.setRenderHint(QPainter::Antialiasing, true);
    switch (m_inhalt.licht) {
        case Grauzone::Licht::Nacht: {
            // Sichel: die Scheibe ohne eine verschobene zweite.
            QPainterPath scheibe;
            scheibe.addEllipse(c, rad, rad);
            QPainterPath schatten;
            schatten.addEllipse(c + QPointF(rad * 0.55, -rad * 0.45),
                                rad * 0.85, rad * 0.85);
            p.fillPath(scheibe.subtracted(schatten), farbe(Style::kTextSecondary));
            break;
        }
        case Grauzone::Licht::Grauzone: {
            // Halb hell, halb dunkel -- in Messing, nicht als Warnfarbe.
            const QRectF k(c.x() - rad, c.y() - rad, 2 * rad, 2 * rad);
            p.setPen(QPen(farbe(Style::kTxKeyBorder), 1.2));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(k);
            p.setPen(Qt::NoPen);
            p.setBrush(farbe(Style::kTxKeyBorder));
            p.drawPie(k, 90 * 16, -180 * 16);
            break;
        }
        case Grauzone::Licht::Tag: {
            p.setPen(Qt::NoPen);
            p.setBrush(farbe(Style::kAmberText));
            p.drawEllipse(c, rad * 0.55, rad * 0.55);
            p.setPen(QPen(farbe(Style::kAmberText), 1.1));
            for (int i = 0; i < 8; ++i) {
                const double a = i * std::numbers::pi / 4.0;
                const QPointF d(std::cos(a), std::sin(a));
                p.drawLine(c + d * rad * 0.8, c + d * rad * 1.15);
            }
            break;
        }
        case Grauzone::Licht::Unbekannt:
            break;
    }
    p.setRenderHint(QPainter::Antialiasing, false);
}

void Ft8PartnerZeile::paintEvent(QPaintEvent*)
{
    const QVector<Stueck> st = passendeStuecke(width());
    if (st.isEmpty()) { return; }

    QPainter p(this);
    const QFontMetrics fm(font());
    const int mitte  = height() / 2;
    const int grund  = mitte + (fm.ascent() - fm.descent()) / 2;
    int x = 0;

    const auto schreibe = [&](const QString& t, const char* f, bool fett,
                              int rest) {
        QFont fo = font();
        if (fett) { fo.setWeight(QFont::DemiBold); }
        p.setFont(fo);
        const QFontMetrics m(fo);
        const QString zeig = m.horizontalAdvance(t) > rest
                                 ? m.elidedText(t, Qt::ElideRight, std::max(rest, 0))
                                 : t;
        p.setPen(farbe(f));
        p.drawText(x, grund, zeig);
        x += m.horizontalAdvance(zeig);
    };

    for (int i = 0; i < st.size(); ++i) {
        const Stueck& s = st[i];
        if (i > 0) {
            schreibe(QStringLiteral(" · "), Style::kTextInactive, false,
                     width() - x);
        }
        if (x >= width()) { break; }
        switch (s.art) {
            case Art::Licht:
                maleLicht(p, QRect(x, mitte - kIcon / 2, kIcon, kIcon));
                x += kIcon + kIconAbst;
                schreibe(s.text, s.farbe, false, width() - x);
                break;
            case Art::Warten: {
                // Ein ruhiger Strich, kein Kreisel: die Antwort kommt in
                // einer Sekunde oder gar nicht, Bewegung waere Laerm.
                QLinearGradient g(x, 0, x + kWarteBalk, 0);
                g.setColorAt(0.0, farbe(Style::kInsetBorder));
                g.setColorAt(0.5, farbe(Style::kTextInactive));
                g.setColorAt(1.0, farbe(Style::kInsetBorder));
                p.fillRect(QRect(x, mitte - 1, kWarteBalk, 3), g);
                x += kWarteBalk + kWarteAbst;
                schreibe(s.text, s.farbe, false, width() - x);
                break;
            }
            case Art::Hinweis:
                schreibe(kHinweisVor, Style::kTextInactive, false, width() - x);
                schreibe(kHinweisKern, Style::kAmberText, false, width() - x);
                schreibe(kHinweisNach, Style::kTextInactive, false, width() - x);
                break;
            case Art::Text:
                schreibe(s.text, s.farbe, s.fett, width() - x);
                break;
        }
    }
}

} // namespace Longpath
