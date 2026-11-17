// =================================================================
// src/gui/widgets/Ft8DannListe.cpp  (Longpath)
// =================================================================
//
// Siehe Ft8DannListe.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-15 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include "Ft8DannListe.h"

#include "gui/StyleConstants.h"

#include <QEnterEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdlib>
#include <functional>

namespace Longpath {

namespace {

QString farbe(const char* c) { return QString::fromLatin1(c); }

QString rapport(int snr)
{
    // „+12", „-07" -- dieselbe Schreibweise wie die Rapporte, die
    // hinausgehen, damit man die Zahl wiedererkennt.
    return QStringLiteral("%1%2")
        .arg(snr < 0 ? QLatin1Char('-') : QLatin1Char('+'))
        .arg(std::abs(snr), 2, 10, QLatin1Char('0'));
}

QString alter(qint64 ms)
{
    const qint64 s = std::max<qint64>(0, ms / 1000);
    return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
}

}  // namespace

// ── Eine Zeile ──────────────────────────────────────────────────────
//
// Sie zeichnet ihren Grund selbst: die erste traegt den Messingstrich
// (sie kommt als naechste dran), die unter der Maus wird heller. Ein
// Stylesheet dafuer muesste bei jedem Wechsel neu angewendet werden,
// und die QSO-Karte darueber vererbt ihr eigenes -- mit Rand und
// Messingkante fuer JEDES QWidget darin.
class Ft8DannListe::Zeile : public QWidget
{
public:
    std::function<void(int)> knopf;      // 0 hoch, 1 runter, 2 weg
    std::function<void()>    doppel;

    explicit Zeile(QWidget* parent)
        : QWidget(parent)
    {
        setFixedHeight(21);
        setToolTip(QObject::tr("Double-click: call now — or next, if a QSO "
                               "is running"));
        auto* h = new QHBoxLayout(this);
        h->setContentsMargins(6, 0, 4, 0);
        h->setSpacing(6);

        const auto schild = [this](int breite, bool mono, bool rechts) {
            auto* l = new QLabel(this);
            // Die Schrift ohne feste Breite eine Stufe kleiner: bei
            // gleicher Punktzahl stand das Land groesser da als das
            // Rufzeichen daneben (Bild vom 2026-11-15).
            QFont f = mono ? Style::monoFont(font(), Style::kFontSmall) : font();
            if (!mono) { f.setPointSize(Style::kFontCaption + 1); }
            l->setFont(f);
            if (breite > 0) { l->setFixedWidth(breite); }
            if (rechts) { l->setAlignment(Qt::AlignRight | Qt::AlignVCenter); }
            return l;
        };
        m_nr  = schild(14, true, false);
        m_ruf = schild(84, true, false);
        QFont fett = m_ruf->font();
        fett.setBold(true);
        m_ruf->setFont(fett);
        m_land = schild(0, false, false);
        m_land->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        m_snr = schild(34, true, true);
        m_loc = schild(44, true, false);
        m_hz  = schild(62, true, true);

        h->addWidget(m_nr);
        h->addWidget(m_ruf);
        h->addWidget(m_land, 1);
        h->addWidget(m_snr);
        h->addWidget(m_loc);
        h->addWidget(m_hz);

        // Alter und Handgriffe teilen sich EINEN Platz: beim Ueberfahren
        // tauscht er, und nichts daneben rueckt.
        m_platz = new QStackedWidget(this);
        m_platz->setFixedWidth(62);
        m_alter = schild(0, true, true);
        m_platz->addWidget(m_alter);
        auto* griffe = new QWidget(m_platz);
        auto* g = new QHBoxLayout(griffe);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(3);
        g->addStretch(1);
        const QString zeichen[3] = {QStringLiteral("↑"),
                                    QStringLiteral("↓"),
                                    QStringLiteral("✕")};
        const QString hilfe[3] = {
            QObject::tr("Earlier"), QObject::tr("Later"),
            QObject::tr("Remove — he is not queued again for 5 min")};
        for (int i = 0; i < 3; ++i) {
            auto* k = new QToolButton(griffe);
            k->setText(zeichen[i]);
            k->setToolTip(hilfe[i]);
            k->setFixedSize(17, 17);
            k->setCursor(Qt::PointingHandCursor);
            k->setStyleSheet(QStringLiteral(
                "QToolButton { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                " stop:0 %1, stop:1 %2); border: 1px solid %3;"
                " border-radius: 4px; color: %4; padding: 0; }"
                "QToolButton:hover { color: %5; border-color: %6; }")
                .arg(farbe(Style::kGlassBtnTop), farbe(Style::kGlassBtnBot),
                     farbe(Style::kBorderMuted), farbe(Style::kTextSecondary),
                     farbe(Style::kTextPrimary), farbe(Style::kGlassSelBorder)));
            QObject::connect(k, &QToolButton::clicked, this,
                             [this, i] { if (knopf) { knopf(i); } });
            g->addWidget(k);
            m_knoepfe[i] = k;
        }
        m_platz->addWidget(griffe);
        h->addWidget(m_platz);
    }

    void setze(const Eintrag& e, int nr, bool erste)
    {
        partner = e.partner;
        m_nr->setText(QString::number(nr));
        m_ruf->setText(e.partner);
        m_land->setText(e.land);
        m_snr->setText(rapport(e.snr));
        m_loc->setText(e.locator);
        m_hz->setText(e.tonHz >= 0 ? QStringLiteral("%1 Hz").arg(e.tonHz)
                                   : QString());
        m_alter->setText(alter(e.alterMs));
        m_alter->setToolTip(e.vonHand
            ? QObject::tr("Added by hand — you call him with your locator")
            : QObject::tr("Last heard this long ago — a call counts for 5 min"));

        const bool blass = e.alterMs >= kBlassAbMs;
        if (blass != m_blass || erste != m_erste || !m_gefaerbt) {
            m_blass = blass;
            m_erste = erste;
            m_gefaerbt = true;
            // Blass heisst: die letzte Minute laeuft. Nicht rot -- es
            // ist kein Fehler, nur ein Anruf, der bald verfaellt.
            const QString leise = farbe(blass ? Style::kBorderMuted
                                              : Style::kTextInactive);
            const QString mitte = farbe(blass ? Style::kTextInactive
                                              : Style::kTextSecondary);
            const QString ruf   = blass ? farbe(Style::kTextInactive)
                                : erste ? farbe(Style::kAmberText)
                                        : farbe(Style::kTextPrimary);
            const auto f = [](QLabel* l, const QString& c) {
                l->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(c));
            };
            f(m_nr, leise);
            f(m_ruf, ruf);
            f(m_land, mitte);
            f(m_snr, mitte);
            f(m_loc, leise);
            f(m_hz, leise);
            f(m_alter, leise);
            update();
        }
    }

    QString      partner;
    QToolButton* knopfNr(int i) const { return m_knoepfe[i]; }

protected:
    void enterEvent(QEnterEvent* ev) override
    {
        m_ueber = true;
        m_platz->setCurrentIndex(1);
        update();
        QWidget::enterEvent(ev);
    }
    void leaveEvent(QEvent* ev) override
    {
        m_ueber = false;
        m_platz->setCurrentIndex(0);
        update();
        QWidget::leaveEvent(ev);
    }
    void mouseDoubleClickEvent(QMouseEvent* ev) override
    {
        if (ev->button() == Qt::LeftButton && doppel) {
            doppel();
            ev->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(ev);
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const QRect r = rect();
        if (m_ueber) {
            p.fillRect(r, QColor(farbe(Style::kTileActiveBg)));
        } else if (m_erste && !m_blass) {
            p.fillRect(r, QColor(farbe(Style::kFt8NaechsterBg)));
        }
        if (m_erste) {
            p.fillRect(QRect(r.left(), r.top(), 2, r.height()),
                       QColor(farbe(Style::kTxKeyBorder)));
        }
    }

private:
    QLabel*         m_nr{nullptr};
    QLabel*         m_ruf{nullptr};
    QLabel*         m_land{nullptr};
    QLabel*         m_snr{nullptr};
    QLabel*         m_loc{nullptr};
    QLabel*         m_hz{nullptr};
    QLabel*         m_alter{nullptr};
    QStackedWidget* m_platz{nullptr};
    QToolButton*    m_knoepfe[3]{nullptr, nullptr, nullptr};
    bool            m_ueber{false};
    bool            m_erste{false};
    bool            m_blass{false};
    bool            m_gefaerbt{false};
};

// ── Die Liste ───────────────────────────────────────────────────────

Ft8DannListe::Ft8DannListe(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ft8DannListe"));
    // Die QSO-Karte setzt „QWidget { Rand; Messingkante }" fuer alles
    // in ihr. Ein eigenes Blatt geht vor dem geerbten -- ohne das
    // trueege jede Zeile einen Rahmen.
    setStyleSheet(QStringLiteral(
        "QWidget { background: transparent; border: none; }"));

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 2, 0, 0);
    v->setSpacing(2);

    auto* strich = new QFrame(this);
    strich->setFixedHeight(1);
    strich->setStyleSheet(QStringLiteral("QFrame { background: %1; }")
                              .arg(farbe(Style::kBorderSubtle)));
    v->addWidget(strich);

    auto* kopf = new QHBoxLayout();
    kopf->setContentsMargins(0, 3, 0, 1);
    kopf->setSpacing(10);
    m_kopf = new QLabel(tr("THEN"), this);
    QFont kf = font();
    kf.setPointSize(Style::kFontCaption);
    kf.setBold(true);
    kf.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
    m_kopf->setFont(kf);
    m_kopf->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                              .arg(farbe(Style::kTextTertiary)));
    m_hinweis = new QLabel(this);
    QFont hf = font();
    hf.setPointSize(Style::kFontCaption + 1);
    m_hinweis->setFont(hf);
    m_hinweis->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                 .arg(farbe(Style::kTextInactive)));
    m_mehr = new QPushButton(this);
    m_mehr->setFlat(true);
    m_mehr->setCursor(Qt::PointingHandCursor);
    m_mehr->setFont(hf);
    m_mehr->setStyleSheet(QStringLiteral(
        "QPushButton { color: %1; background: transparent; border: none;"
        " padding: 0 2px; }"
        "QPushButton:hover { color: %2; }")
        .arg(farbe(Style::kTextTertiary), farbe(Style::kTextPrimary)));
    m_mehr->hide();
    connect(m_mehr, &QPushButton::clicked, this, [this] {
        m_ausgeklappt = !m_ausgeklappt;
        zeigeZeilen();
    });
    kopf->addWidget(m_kopf);
    kopf->addWidget(m_hinweis, 1);
    kopf->addWidget(m_mehr);
    v->addLayout(kopf);

    m_zeilenLayout = new QVBoxLayout();
    m_zeilenLayout->setContentsMargins(0, 0, 0, 0);
    m_zeilenLayout->setSpacing(0);
    v->addLayout(m_zeilenLayout);

    setzeEintraege({}, true);
}

Ft8DannListe::~Ft8DannListe() = default;

void Ft8DannListe::setzeEintraege(const QVector<Eintrag>& eintraege,
                                  bool automatisch)
{
    m_eintraege   = eintraege;
    m_automatisch = automatisch;
    if (m_eintraege.size() <= kSichtbar) { m_ausgeklappt = false; }
    zeigeZeilen();
}

void Ft8DannListe::zeigeZeilen()
{
    const int n = m_eintraege.size();
    if (n == 0) {
        m_hinweis->setText(tr("nobody waiting — stations that call you "
                              "line up here"));
    } else if (m_automatisch) {
        m_hinweis->setText(tr("automatically, in this order · a call "
                              "counts for 5 min"));
    } else {
        m_hinweis->setText(tr("double-click to call · a call counts "
                              "for 5 min"));
    }

    const int zeigen = m_ausgeklappt ? n : std::min(n, kSichtbar);
    m_mehr->setVisible(n > kSichtbar);
    m_mehr->setText(m_ausgeklappt ? tr("show less")
                                  : tr("+%1 more").arg(n - kSichtbar));

    // Vorhandene Zeilen behalten (siehe Kopf der .h), nur fehlende
    // anlegen und ueberzaehlige verstecken.
    while (m_zeilen.size() < zeigen) {
        auto* z = new Zeile(this);
        z->knopf = [this, z](int i) {
            const QString wer = z->partner;
            if (i == 0)      { emit hoch(wer); }
            else if (i == 1) { emit runter(wer); }
            else             { emit weg(wer); }
        };
        z->doppel = [this, z] { emit gewaehlt(z->partner); };
        m_zeilenLayout->addWidget(z);
        m_zeilen.append(z);
    }
    for (int i = 0; i < m_zeilen.size(); ++i) {
        Zeile* z = m_zeilen.at(i);
        if (i < zeigen) {
            z->setze(m_eintraege.at(i), i + 1, i == 0);
            z->show();
        } else {
            z->hide();
        }
    }
}

int Ft8DannListe::sichtbareZeilenForTest() const
{
    int n = 0;
    for (const Zeile* z : m_zeilen) {
        if (!z->isHidden()) { ++n; }
    }
    return n;
}

QString Ft8DannListe::rufInZeileForTest(int zeile) const
{
    if (zeile < 0 || zeile >= m_zeilen.size()) { return {}; }
    return m_zeilen.at(zeile)->isHidden() ? QString()
                                          : m_zeilen.at(zeile)->partner;
}

QWidget* Ft8DannListe::zeileForTest(int zeile) const
{
    if (zeile < 0 || zeile >= m_zeilen.size()) { return nullptr; }
    return m_zeilen.at(zeile);
}

QToolButton* Ft8DannListe::knopfForTest(int zeile, int welcher) const
{
    if (zeile < 0 || zeile >= m_zeilen.size() || welcher < 0 || welcher > 2) {
        return nullptr;
    }
    return m_zeilen.at(zeile)->knopfNr(welcher);
}

}  // namespace Longpath
