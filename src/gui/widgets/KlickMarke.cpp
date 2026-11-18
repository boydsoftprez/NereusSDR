// =================================================================
// src/gui/widgets/KlickMarke.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Zweck und Begruendung: KlickMarke.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "gui/widgets/KlickMarke.h"

#include "gui/StyleConstants.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QRadioButton>
#include <QVariant>

namespace Longpath {

namespace {

// Die Flaeche selbst: ein Kind des Knopfes, so gross wie er, fuer die
// Maus durchlaessig. `staerke` laeuft von 1 (gedrueckt) nach 0.
class Markenflaeche : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal staerke READ staerke WRITE setStaerke)
public:
    explicit Markenflaeche(QWidget* knopf) : QWidget(knopf)
    {
        setObjectName(QStringLiteral("KlickMarke"));
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        setGeometry(knopf->rect());
    }

    qreal staerke() const { return m_staerke; }
    void setStaerke(qreal s)
    {
        m_staerke = qBound(0.0, s, 1.0);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (m_staerke <= 0.0) { return; }
        QPainter g(this);
        g.setRenderHint(QPainter::Antialiasing, true);

        // Das Orange des Plus, halb durchscheinend: die Beschriftung
        // darunter bleibt lesbar, und ein Knopf, der selbst schon Farbe
        // traegt (Sendetaste, eine eingerastete Wahl), wird getoent
        // statt uebermalt.
        QColor oben(QString::fromLatin1(Style::kPlusTop));
        QColor unten(QString::fromLatin1(Style::kPlusBottom));
        QColor rand(QString::fromLatin1(Style::kPlusBorder));
        oben.setAlphaF(0.78 * m_staerke);
        unten.setAlphaF(0.78 * m_staerke);
        rand.setAlphaF(0.95 * m_staerke);

        const QRectF feld = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QLinearGradient flaeche(feld.topLeft(), feld.bottomLeft());
        flaeche.setColorAt(0.0, oben);
        flaeche.setColorAt(1.0, unten);
        g.setBrush(flaeche);
        g.setPen(QPen(rand, 1.0));
        // Die Chip-Rundung des Hausstils (7). Die Knoepfe selbst tragen
        // verschiedene Radien (2, 4, 7, 10); bei einem eckigeren liegt
        // die Marke an den Ecken einen Hauch innen, bei einem runderen
        // schneidet sein Rand sie ab -- sie ist sein Kind.
        g.drawRoundedRect(feld, Style::kGlassChipRadius, Style::kGlassChipRadius);
    }

private:
    qreal m_staerke{1.0};
};

} // namespace

KlickMarke::KlickMarke(QObject* eltern) : QObject(eltern)
{
    if (qApp) { qApp->installEventFilter(this); }
}

bool KlickMarke::bekommtMarke(const QAbstractButton* knopf)
{
    if (!knopf || !knopf->isEnabled() || !knopf->isVisible()) { return false; }
    // Der Haken IST die Antwort (siehe Kopf).
    if (qobject_cast<const QCheckBox*>(knopf)) { return false; }
    if (qobject_cast<const QRadioButton*>(knopf)) { return false; }
    // Wer sich abmeldet, bleibt ohne: das Plus antwortet selbst.
    const QVariant wunsch = knopf->property("klickmarke");
    if (wunsch.isValid() && !wunsch.toBool()) { return false; }
    return true;
}

QAbstractButton* KlickMarke::knopfZu(QWidget* getroffen)
{
    for (QWidget* w = getroffen; w; w = w->parentWidget()) {
        if (auto* knopf = qobject_cast<QAbstractButton*>(w)) { return knopf; }
        // Nicht ueber eine Fenstergrenze hinaus: ein Knopf in einem
        // anderen Fenster ist nicht der, der gedrueckt wurde.
        if (w->isWindow()) { break; }
    }
    return nullptr;
}

QWidget* KlickMarke::markeFuerTest(const QAbstractButton* knopf) const
{
    return m_marken.value(knopf).data();
}

bool KlickMarke::eventFilter(QObject* ziel, QEvent* ereignis)
{
    // Alles ausser Druck und Loslassen faellt in dieser Zeile durch --
    // der Filter sieht jedes Ereignis des Programms.
    const QEvent::Type art = ereignis->type();
    if (art != QEvent::MouseButtonPress && art != QEvent::MouseButtonRelease) {
        return false;
    }
    auto* maus = static_cast<QMouseEvent*>(ereignis);
    if (maus->button() != Qt::LeftButton) { return false; }

    if (art == QEvent::MouseButtonPress) {
        auto* widget = qobject_cast<QWidget*>(ziel);
        QAbstractButton* knopf = widget ? knopfZu(widget) : nullptr;
        if (knopf && bekommtMarke(knopf)) { druecke(knopf); }
        return false;   // nie schlucken: die Marke sieht zu, sie greift nicht ein
    }

    // Loslassen: gleich wo die Maus inzwischen steht -- wer vom Knopf
    // herunterzieht und dort loslaesst, hat ihn nicht ausgeloest, aber
    // die Marke muss trotzdem gehen.
    if (m_gedrueckt) { lasseLos(m_gedrueckt); }
    return false;
}

void KlickMarke::druecke(QAbstractButton* knopf)
{
    // Ein zweiter Druck, waehrend die alte Marke noch ausklingt: sie
    // wird wieder voll, statt dass zwei uebereinanderliegen.
    QWidget* alt = m_marken.value(knopf).data();
    auto* marke = qobject_cast<Markenflaeche*>(alt);
    if (!marke) {
        marke = new Markenflaeche(knopf);
        m_marken.insert(knopf, marke);
        connect(knopf, &QObject::destroyed, this,
                [this, knopf]() { m_marken.remove(knopf); });
    }
    // Eine laufende Ausklang-Animation anhalten.
    for (QPropertyAnimation* a : marke->findChildren<QPropertyAnimation*>()) {
        a->stop();
        a->deleteLater();
    }
    marke->setGeometry(knopf->rect());
    marke->setStaerke(1.0);
    marke->raise();
    marke->show();
    m_gedrueckt = knopf;
}

void KlickMarke::lasseLos(QAbstractButton* knopf)
{
    m_gedrueckt.clear();
    auto* marke = qobject_cast<Markenflaeche*>(m_marken.value(knopf).data());
    if (!marke) { return; }

    auto* aus = new QPropertyAnimation(marke, "staerke", marke);
    aus->setDuration(kAusklangMs);
    aus->setStartValue(marke->staerke());
    aus->setEndValue(0.0);
    aus->setEasingCurve(QEasingCurve::OutCubic);
    connect(aus, &QPropertyAnimation::finished, marke, [this, knopf, marke]() {
        m_marken.remove(knopf);
        marke->deleteLater();
    });
    aus->start(QAbstractAnimation::DeleteWhenStopped);
}

} // namespace Longpath

#include "KlickMarke.moc"
