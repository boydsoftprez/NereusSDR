// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/applets/KombiApplet.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Siehe KombiApplet.h fuer das Warum.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-01 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/applets/KombiApplet.h"

#include "core/AppSettings.h"
#include "gui/StyleConstants.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace Longpath {

namespace {
QList<KombiApplet*> g_alle;
QString standSchluessel(const QString& kennung) { return kennung + QStringLiteral("/Inhalt"); }
QString seitenSchluessel(const QString& kennung) { return kennung + QStringLiteral("/Seite"); }
}  // namespace

KombiApplet::KombiApplet(RadioModel* model, QWidget* parent, const QString& kennung)
    : AppletWidget(model, parent)
    , m_kennung(kennung)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(Style::kPanelGapPx);

    baueReiter();

    // ── Schmal gestaltbar (2026-11-01) ──────────────────────────────
    //
    // Betreiber: „sollte ggf. eher schmal gestaltbar sein, weil
    // Spalte schmal und hoch ist." Ohne das bestimmt die BREITESTE
    // Seite die Breite des ganzen Feldes -- und damit die der Spalte.
    // Ein Rollbereich nimmt der breitesten Seite dieses Vorrecht: sie
    // darf breiter sein als das Feld, dann wird eben geschoben.
    //
    // Nur waagerecht. Senkrecht rollt die Spalte selbst; zwei
    // ineinander liegende senkrechte Rollbereiche sind eine Falle,
    // weil das Rad dann mal den einen und mal den anderen bewegt.
    m_stapel = new QStackedWidget;
    m_rollbereich = new QScrollArea(this);
    m_rollbereich->setWidget(m_stapel);
    m_rollbereich->setWidgetResizable(true);
    m_rollbereich->setFrameShape(QFrame::NoFrame);
    m_rollbereich->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_rollbereich->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    root->addWidget(m_rollbereich, 1);
    // Das Feld selbst darf so schmal werden, wie der Betreiber die
    // Spalte zieht.
    setMinimumWidth(0);
    // ── Und es nimmt die ganze Hoehe ────────────────────────────────
    //
    // Betreiber, 2026-11-01: „ich hatte nicht auf die kopfleiste
    // geklickt." Stimmt -- auf seinem Bild lag die Stelle UNTER dem
    // Feld: das Combined endete nach seinem Inhalt, und die schwarze
    // Flaeche darunter gehoerte dem Container. Dessen Menue kam
    // folgerichtig.
    //
    // Sein Ziel war ohnehin „das Panel ganz rechts von oben bis
    // unten". Also nimmt das Feld den Platz, den es bekommt; dann
    // gibt es unter ihm keine fremde Flaeche mehr, auf die man
    // danebenklicken kann.
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    m_aufklapp = new QWidget(this);
    auto* senkrecht = new QVBoxLayout(m_aufklapp);
    senkrecht->setContentsMargins(0, 0, 0, 0);
    senkrecht->setSpacing(Style::kPanelGapPx);
    m_aufklapp->setVisible(false);
    root->addWidget(m_aufklapp, 1);

    // Der leere Zustand sagt, was zu tun ist. Ein leeres Feld ohne ein
    // Wort sieht aus wie ein Fehler.
    m_hinweis = new QLabel(tr("Drag panels in here by their title bar"), this);
    m_hinweis->setAlignment(Qt::AlignCenter);
    m_hinweis->setWordWrap(true);
    m_hinweis->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px; padding: 12px;")
            .arg(QLatin1String(Style::kTextTertiary)));
    root->addWidget(m_hinweis, 1);

    reiterNeu();
    g_alle.append(this);

    // ── Die Liste MUSZ ueberall im Feld zu holen sein ───────────────
    //
    // Betreiber, 2026-11-01: „muss immer und überall funktionieren in
    // im combined". Ein Rechtsklick geht an das Widget UNTER dem
    // Zeiger; was ihn nicht selbst beantwortet, reicht ihn nach oben
    // weiter -- aber Rollbereich, Stapel und Hinweis beantworten ihn
    // mit einem leeren Menue und schlucken ihn damit.
    //
    // Darum haengt die Frage an jedem dieser Stuecke. Eine Seite, die
    // ein EIGENES Rechtsklickmenue hat (das RX-Applet mit seinen
    // DSP-Griffen), behaelt ihres -- dort kommt der Klick gar nicht
    // erst bis hierher, und das ist richtig so.
    for (QWidget* teil : {static_cast<QWidget*>(m_rollbereich),
                          static_cast<QWidget*>(m_rollbereich->viewport()),
                          static_cast<QWidget*>(m_stapel),
                          static_cast<QWidget*>(m_hinweis),
                          static_cast<QWidget*>(m_aufklapp)}) {
        if (!teil) { continue; }
        teil->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(teil, &QWidget::customContextMenuRequested, this,
                [this, teil](const QPoint& wo) {
            emit listeGewuenscht(teil->mapToGlobal(wo));
        });
    }

    // ── Und ein Fangnetz darunter ───────────────────────────────────
    //
    // „genau mitten im fenster" -- und trotzdem kam das Menue des
    // Containers. Der Rechtsklick geht an das Widget UNTER dem
    // Zeiger; reicht das ihn weiter, laeuft er die Elternkette hoch
    // und landet irgendwann beim Container. WELCHES Stueck ihn
    // durchgereicht hat, ist von auszen nicht zu sehen -- also wird
    // nicht geraten, sondern abgefangen: ein Filter auf der
    // Anwendung sieht JEDEN Rechtsklick und fragt nur, ob er
    // innerhalb DIESES Feldes liegt.
    //
    // Eine Seite mit eigenem Menue (RX-Applet, DSP-Griffe) behaelt
    // ihres: die beantwortet den Klick selbst, und dann kommt er hier
    // gar nicht an.
    qApp->installEventFilter(this);

    // Rechtsklick auf das Feld fragt nach der Liste. Auf dem Feld,
    // nicht nur auf dem leeren Hinweis: auch mit zwei Seiten darin
    // will man eine dritte dazulegen koennen.
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this,
            [this](const QPoint& wo) {
        emit listeGewuenscht(mapToGlobal(wo));
    });
}

KombiApplet::~KombiApplet()
{
    g_alle.removeAll(this);
}

QList<KombiApplet*> KombiApplet::alle()
{
    return g_alle;
}

// Das sichtbare Rechteck auf dem SCHIRM entscheidet -- nicht, in
// welchem Fenster das Feld haengt.
KombiApplet* KombiApplet::unter(const QPoint& globalPos)
{
    for (KombiApplet* k : g_alle) {
        if (!k || !k->isVisible()) { continue; }
        const QRect r(k->mapToGlobal(QPoint(0, 0)), k->size());
        if (r.contains(globalPos)) { return k; }
    }
    return nullptr;
}

void KombiApplet::baueReiter()
{
    // Oben, wie der Betreiber es gesagt hat („oben im Panel zum
    // umschalten"), und LINKS: rechts oben sitzt das Zahnrad, und das
    // ist seit dem 2026-09-20 seine feste Regel fuer Optionen.
    m_reiterleiste = new QWidget(this);
    m_reiterReihe = new QHBoxLayout(m_reiterleiste);
    m_reiterReihe->setContentsMargins(0, 0, 0, 0);
    m_reiterReihe->setSpacing(Style::kPanelGapPx);
    // KEIN Dehnfuge am Ende: die Reiter teilen sich die volle Breite.
    // Betreiber, 2026-11-01, zu Entwurf 2: „ziel des fenster ist es
    // ja, eines offen zu lassen und schnell umschalten zu können."
    // Schnell heiszt: nicht zielen muessen. Zwei halbe Panelbreiten
    // sind das groesztmoegliche Ziel, das hier zu haben ist.
    layout()->addWidget(m_reiterleiste);
}

QString KombiApplet::appletTitle() const
{
    // ── Kein ausgeschriebener Name, solange Pillen dastehen ─────────
    //
    // Betreiber, 2026-11-01: "also Combined muss ja nicht
    // ausgeschrieben sein" -- "dann könnte RX und TX eine eigene
    // pille nebeneinander sein" -- "wäre platz, eine dritte".
    //
    // Richtig: das Wort sagt nichts, was die Pillen nicht schon
    // zeigen, und es nimmt ihnen in einer schmalen Spalte den Platz
    // fuer die dritte. Leer bleibt es nur, solange etwas drin ist --
    // ein leeres Feld muss sagen, was es ist.
    if (!m_inhalte.isEmpty()) { return QString(); }

    // IMMER „Combined", nie der Inhalt.
    //
    // Betreiber, 2026-11-01: „das RX/TX (combined) panel sollte
    // generell combined heißen, sofern man hier dann alle widget
    // reinziehen kann!" -- und man kann. Ein Name, der sich mit dem
    // Inhalt aendert, haette das Feld jedes Mal zu etwas anderem
    // gemacht; dabei ist es immer dasselbe Ding. Was gerade drin ist,
    // steht ohnehin auf den Reitern.
    return m_kennung == QStringLiteral("Kombi2") ? tr("Combined 2") : tr("Combined");
}

void KombiApplet::nimmAuf(QWidget* panel)
{
    if (!panel || !m_stapel) { return; }
    for (int i = 0; i < m_inhalte.size(); ++i) {
        if (m_inhalte.at(i) != panel) { continue; }
        // Schon aufgenommen -- aber nur, wenn es auch wirklich noch im
        // Stapel liegt. Am 2026-11-17 gemessen: jemand hatte das
        // Rotor-Panel per Elternwechsel aus dem Feld genommen, ohne
        // gibFrei() zu rufen. Die Liste fuehrte es weiter, dieser
        // Vergleich sagte „ist schon da", und das Panel blieb ohne
        // Elternteil und unsichtbar liegen. Ein Eintrag, der etwas
        // behauptet, was nicht stimmt, ist kein Grund abzulehnen: er
        // fliegt hinaus, und das Panel wird neu aufgenommen.
        if (m_stapel->indexOf(panel) >= 0) { return; }
        m_inhalte.removeAt(i);
        break;
    }
    m_inhalte.append(panel);
    m_stapel->addWidget(panel);   // Elternwechsel, kein Neubau
    panel->show();
    reiterNeu();
    zeigeSeite(m_inhalte.size() - 1);
}

void KombiApplet::gibFrei(QWidget* panel)
{
    if (!panel || !m_stapel) { return; }
    int weg = -1;
    for (int i = 0; i < m_inhalte.size(); ++i) {
        if (m_inhalte.at(i) == panel) { weg = i; break; }
    }
    if (weg < 0) { return; }
    m_inhalte.removeAt(weg);
    if (m_stapel->indexOf(panel) >= 0) { m_stapel->removeWidget(panel); }
    // Ohne Elternteil zurueck: unversehrt, aber ohne Platz. Wer es
    // bekommt, legt es in ein eigenes Feld.
    panel->setParent(nullptr);
    reiterNeu();
    emit herausgeloest(panel);
}

QList<QWidget*> KombiApplet::gibAlleFrei()
{
    QList<QWidget*> raus;
    while (!m_inhalte.isEmpty()) {
        QWidget* w = m_inhalte.first().data();
        if (!w) { m_inhalte.removeFirst(); continue; }
        raus.append(w);
        gibFrei(w);
    }
    return raus;
}

// Die Reiterleiste wird neu gebaut, nicht nachgepflegt: bei zwei bis
// fuenf Knoepfen ist das billiger als jede Buchhaltung, und es gibt
// keinen Zustand, der auseinanderlaufen kann.
void KombiApplet::reiterNeu()
{
    for (QPushButton* b : m_reiter) { b->deleteLater(); }
    m_reiter.clear();

    // ── Schwarz, nicht blau (2026-11-01) ────────────────────────────
    //
    // Betreiber zum ersten Bau: „die farbe ist auch blau und nicht
    // schwarz". Zu Recht -- ein Reiter ueber die volle Panelbreite in
    // der Akzentfarbe ist eine grosze FLAECHE Farbe, und der Hausstil
    // sagt: Farbe bedeckt hoechstens zwei Prozent. Der aktive Reiter
    // bleibt jetzt dunkel wie das Panel; erkennbar ist er an der
    // helleren Schrift und an einem schmalen Strich darunter. Das ist
    // dieselbe Menge Farbe wie in Entwurf 4, nur ueber die ganze
    // Breite.
    // ── Schwarz. Ohne Blau. (2026-11-01) ────────────────────────────
    //
    // Betreiber zweimal zu diesem Reiter: „die farbe ist auch blau
    // und nicht schwarz" und dann „1. muss schwarz sein". Also kein
    // Akzent, auch nicht als Strich.
    //
    // Erkennbar bleibt der aktive Reiter an zwei Dingen, die keine
    // Farbe kosten: er liegt TIEFER (dunkel wie das Panel, waehrend
    // die anderen aufgesetzt sind) und seine Schrift ist hell. Das
    // ist dieselbe Sprache, die der Hausstil ueberall spricht --
    // Licht von oben, Vertiefung heiszt gewaehlt.
    const QString css =
        Style::buttonBaseStyle()
        + QStringLiteral(
              "QPushButton:checked {"
              "  background: %1; color: %2;"
              "  border: 1px solid %3;"
              "}"
              "QPushButton:!checked { color: %4; }")
              .arg(QLatin1String(Style::kPanelBg),
                   QLatin1String(Style::kTextPrimary),
                   QLatin1String(Style::kBorder),
                   QLatin1String(Style::kTextTertiary));
    for (int i = 0; i < m_inhalte.size(); ++i) {
        QWidget* w = m_inhalte.at(i).data();
        if (!w) { continue; }
        auto* b = new QPushButton(nameVon(w), m_reiterleiste);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(css);
        // Hoeher als ein gewoehnlicher Knopf (20) und ueber die ganze
        // Breite: dieser hier wird oft und im Vorbeigehen getroffen.
        b->setFixedHeight(24);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setToolTip(tr("Show this panel. Drag it out, or onto another "
                         "Combined. Right-click for more."));
        connect(b, &QPushButton::clicked, this,
                [this, i]() { zeigeSeite(i); });
        // Herausziehen: der Reiter selbst ist der Griff.
        b->installEventFilter(this);
        b->setProperty("kombiSeite", i);
        m_reiterReihe->insertWidget(i, b, 1);   // gleiche Breite fuer jeden
        m_reiter.append(b);
    }

    const bool leer = m_inhalte.isEmpty();
    if (m_reiterleiste) { m_reiterleiste->setVisible(!leer); }
    if (m_hinweis)      { m_hinweis->setVisible(leer); }
    if (m_stapel)       { m_stapel->setVisible(!leer); }
    if (m_rollbereich)  { m_rollbereich->setVisible(!leer && !m_aufgeklappt); }

    if (!leer) { zeigeSeite(qBound(0, seite(), m_inhalte.size() - 1)); }
    // Die feste Hoehe haengt an den Seiten -- kommt eine dazu oder
    // geht eine weg, wird sie neu bestimmt.
    if (leer) {
        setMinimumHeight(0);
        setMaximumHeight(QWIDGETSIZE_MAX);
    } else {
        ordneSeitenEin();
    }
    chipNachziehen();
    merkeStand();
}

// ── Ein Reiter als Griff ───────────────────────────────────────────
//
// Betreiber, 2026-11-01: „frequenz lässt sich reinziehen, soll sich
// aber wieder rausziehen lassen!" Hinein geht es durch Ziehen; hinaus
// ging es nur ueber einen Rechtsklick -- einen Weg, den man kennen
// musz. Was auf einem Weg hineingeht, musz denselben Weg hinaus.
//
// Erst ab einer Strecke wird aus dem Druck ein Zug (Qt's eigene
// Startstrecke), sonst waere jeder Klick ein halbes Herausziehen.
bool KombiApplet::eventFilter(QObject* watched, QEvent* ev)
{
    // Das Fangnetz (siehe Konstruktor): ein Rechtsklick irgendwo in
    // diesem Feld, den kein Stueck beantwortet hat.
    if (ev->type() == QEvent::ContextMenu) {
        auto* w = qobject_cast<QWidget*>(watched);
        if (w && (w == this || isAncestorOf(w))) {
            auto* ce = static_cast<QContextMenuEvent*>(ev);
            // Ein Reiter hat sein eigenes Menue. Dieses Netz fing den
            // Klick bisher vor ihm ab; das "Rechtsklick nimmt es heraus"
            // des Tooltips lief deshalb nie (2026-11-06).
            if (auto* reiter = qobject_cast<QPushButton*>(w);
                reiter && m_reiter.contains(reiter)) {
                const int i = reiter->property("kombiSeite").toInt();
                const QList<QWidget*> alle = seiten();
                if (i >= 0 && i < alle.size()) {
                    emit reiterMenueGewuenscht(alle.at(i), ce->globalPos());
                }
                return true;
            }
            emit listeGewuenscht(ce->globalPos());
            return true;        // nicht weiter nach oben
        }
        return false;
    }

    // Der Filter haengt an der Anwendung: nur die EIGENEN Reiter zaehlen.
    // Ohne diese Frage griff ein Druck auf irgendeinen Knopf die erste
    // Seite, und schon ein kleiner Mausweg riss sie aus dem Feld.
    auto* knopf = qobject_cast<QPushButton*>(watched);
    if (!knopf || !m_reiter.contains(knopf)) {
        return AppletWidget::eventFilter(watched, ev);
    }

    const int i = knopf->property("kombiSeite").toInt();
    const QList<QWidget*> alle = seiten();

    switch (ev->type()) {
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(ev);
        if (me->button() == Qt::LeftButton && i >= 0 && i < alle.size()) {
            m_zugStart = me->globalPosition().toPoint();
            m_zugPanel = alle.at(i);
        }
        break;
    }
    case QEvent::MouseMove: {
        auto* me = static_cast<QMouseEvent*>(ev);
        if (!m_zugPanel || m_zugStart.isNull()) { break; }
        if (!m_zugLaeuft) {
            const QPoint jetzt = me->globalPosition().toPoint();
            if ((jetzt - m_zugStart).manhattanLength()
                    < QApplication::startDragDistance()) {
                break;
            }
            m_zugLaeuft = true;
            knopf->setCursor(Qt::ClosedHandCursor);
        }
        return true;
    }
    case QEvent::MouseButtonDblClick:
        // Die Reiter fuellen die ganze Leiste -- eine freie Stelle
        // zum Doppelklicken gibt es nicht, also zaehlt der Reiter
        // selbst. Einfach klicken schaltet um, doppelt klappt auf.
        m_zugPanel = nullptr;
        m_zugStart = QPoint();
        m_zugLaeuft = false;
        setAufgeklappt(!m_aufgeklappt);
        return true;
    case QEvent::MouseButtonRelease: {
        // ── Entschieden wird beim Loslassen (2026-11-06) ─────────────
        //
        // Betreiber: "oder rueberziehen". Bis hierher loeste schon der
        // erste Mausweg das Panel; ein Ziel konnte es so nie geben.
        const bool zug = m_zugLaeuft;
        QWidget* raus = m_zugPanel.data();
        m_zugPanel = nullptr;
        m_zugStart = QPoint();
        m_zugLaeuft = false;
        if (!zug) { break; }
        knopf->setCursor(Qt::PointingHandCursor);
        knopf->setDown(false);
        const QPoint jetzt =
            static_cast<QMouseEvent*>(ev)->globalPosition().toPoint();
        // Es zaehlt, was oben liegt. Das Ziel steht VOR dem Freigeben
        // fest: danach schrumpft dieses Feld, und ein Nachbar in derselben
        // Spalte rutscht unter dem Zeiger weg.
        QWidget* oben = QApplication::widgetAt(jetzt);
        KombiApplet* ziel = nullptr;
        for (QWidget* w = oben; w && !ziel; w = w->parentWidget()) {
            ziel = qobject_cast<KombiApplet*>(w);
        }
        if (!oben && QRect(mapToGlobal(QPoint(0, 0)), size()).contains(jetzt)) {
            ziel = this;
        }
        // Im eigenen Feld losgelassen: es bleibt, wo es ist.
        if (raus && ziel != this) {
            // Erst aus dem Feld, dann melden: wer zuhoert, bekommt ein
            // Panel ohne Elternteil.
            gibFrei(raus);
            if (ziel) {
                emit herueberGezogen(raus, ziel);
            } else {
                emit herausgezogen(raus, jetzt);
            }
        }
        return true;
    }
    default:
        break;
    }
    return AppletWidget::eventFilter(watched, ev);
}

// ── Auf- und zuklappen ─────────────────────────────────────────────
//
// Zugeklappt ist das Feld ein Stapel mit Reitern und einer FESTEN
// Hoehe -- es springt beim Umschalten nicht, und das war der halbe
// Sinn der Sache („eine größe, die fix bleibt").
//
// Aufgeklappt stehen alle Seiten untereinander: man sieht die
// anderen Panels mit, ohne sie herauszuziehen. Beim Zuklappen
// bekommt das Feld seine feste Hoehe zurueck.
void KombiApplet::setAufgeklappt(bool an)
{
    if (m_aufgeklappt == an) { return; }
    m_aufgeklappt = an;
    ordneSeitenEin();
}

void KombiApplet::ordneSeitenEin()
{
    if (!m_stapel || !m_aufklapp) { return; }
    auto* senkrecht = qobject_cast<QVBoxLayout*>(m_aufklapp->layout());
    if (!senkrecht) { return; }

    if (m_aufgeklappt) {
        for (const QPointer<QWidget>& w : m_inhalte) {
            if (!w) { continue; }
            if (m_stapel->indexOf(w) >= 0) { m_stapel->removeWidget(w); }
            senkrecht->addWidget(w.data());
            w->show();
        }
        m_stapel->setVisible(false);
        if (m_rollbereich) { m_rollbereich->setVisible(false); }
        m_aufklapp->setVisible(true);
        setMaximumHeight(QWIDGETSIZE_MAX);
        setMinimumHeight(0);
    } else {
        for (const QPointer<QWidget>& w : m_inhalte) {
            if (!w) { continue; }
            senkrecht->removeWidget(w.data());
            if (m_stapel->indexOf(w) < 0) { m_stapel->addWidget(w.data()); }
        }
        m_aufklapp->setVisible(false);
        m_stapel->setVisible(!m_inhalte.isEmpty());
        if (m_rollbereich) { m_rollbereich->setVisible(!m_inhalte.isEmpty()); }
        zeigeSeite(seite());
        // ── KEINE feste Hoehe mehr (2026-11-01) ──────────────────────
        //
        // Betreiber: „beim drüber scrollen verschwindet einiges".
        // Gesehen in der laufenden App: das Feld war viel hoeher als
        // sein Inhalt, darunter eine groesze schwarze Flaeche, und in
        // der Spalte schob es die Nachbarn aus dem Bild.
        //
        // `setFixedHeight` war der falsche Griff. Die feste Groesze,
        // um die es ihm ging, macht der Stapel von selbst: ein
        // QStackedWidget nimmt die Hoehe der GROESZTEN Seite, nicht
        // die der sichtbaren -- das Feld springt beim Umschalten also
        // ohnehin nicht. Eine Untergrenze genuegt, damit es nicht
        // zusammenfaellt; nach oben entscheidet das Gehaeuse.
        setMaximumHeight(QWIDGETSIZE_MAX);
        setMinimumHeight(0);
    }
    updateGeometry();
}

// Der Umschalter in der Kopfleiste (Entwurf 3).
//
// EIN Knopf, kein Streifen: er zeigt, was gerade laeuft, und schaltet
// beim Klick auf die naechste Seite weiter. Der Gewinn ist Hoehe --
// Betreiber, 2026-11-01: "das TX panel ist oben so gross, das bei
// meinem mac das nicht otimal ist".
//
// Rechtsklick loest die gezeigte Seite wieder heraus, Doppelklick
// klappt auf und zu: dieselben Griffe wie vorher am Reiter.
QWidget* KombiApplet::kopfleistenWidget()
{
    if (!m_chip) {
        // ── Eine Pille je Seite, nebeneinander ──────────────────────
        //
        // Betreiber, 2026-11-01, nach dem Durchschalt-Griff: "wenn
        // 2-3 pillen oben stehen wäre es einfacher."
        //
        // Stimmt: mit zwei oder drei Seiten ist der direkte Weg immer
        // kuerzer als das Durchschalten -- man sieht, was da ist, und
        // trifft es mit einem Klick. (Sein frueheres "die pillen
        // gefallen mir nicht" galt dem EINEN gefuellten Rundknopf,
        // nicht der Reihe.)
        //
        // In der KOPFLEISTE, nicht als eigener Streifen: der kostete
        // Hoehe, und die Spalte ist schmal und hoch.
        m_chip = new QWidget(this);
        m_pillenReihe = new QHBoxLayout(m_chip);
        m_pillenReihe->setContentsMargins(0, 0, 0, 0);
        m_pillenReihe->setSpacing(3);
        // ── Rechtsklick auf die LEISTE zeigt die Liste ──────────────
        //
        // Betreiber, 2026-11-01, mit Bild: „rechte maustaste
        // funktioniert nicht". Er hatte auf die Kopfleiste geklickt --
        // und dort gehoerte das Menue dem Gehaeuse („Move freely /
        // Own window / Dock at the edge"). Meine Liste hing nur am
        // Koerper des Feldes, den man bei vollem Feld gar nicht mehr
        // trifft.
        //
        // Jetzt: Rechtsklick auf die Leiste neben den Pillen zeigt
        // die Liste; Rechtsklick auf eine PILLE nimmt genau die
        // heraus (das haengt an der Pille selbst, siehe unten).
        m_chip->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_chip, &QWidget::customContextMenuRequested, this,
                [this](const QPoint& wo) {
            emit listeGewuenscht(m_chip->mapToGlobal(wo));
        });
        m_chip->installEventFilter(this);
        m_chip->setProperty("kombiChip", true);
    }
    chipNachziehen();
    return m_chip;
}

// Die Reihe wird neu gebaut, nicht nachgepflegt: bei zwei bis vier
// Pillen ist das billiger als jede Buchhaltung.
void KombiApplet::chipNachziehen()
{
    if (!m_chip || !m_pillenReihe) { return; }
    for (QPushButton* b : m_pillen) { b->deleteLater(); }
    m_pillen.clear();

    const QList<QWidget*> s = seiten();
    const int aktiv = seite();

    // Dunkel statt Akzent: "muss schwarz sein" (2026-11-01). Die
    // aktive Pille liegt tiefer und hat helle Schrift -- dieselbe
    // Sprache wie ueberall im Haus, Licht von oben.
    const QString css =
        QStringLiteral(
            "QPushButton {"
            "  background: %1; color: %2;"
            "  border: 1px solid %3; border-radius: 7px;"
            "  font-size: 9px; font-weight: bold;"
            "  padding: 0px 7px;"
            "}"
            "QPushButton:hover { color: %4; }"
            "QPushButton:checked { background: %5; color: %4;"
            "  border: 1px solid %6; }")
            .arg(QLatin1String(Style::kButtonBg),
                 QLatin1String(Style::kTextTertiary),
                 QLatin1String(Style::kBorder),
                 QLatin1String(Style::kTextPrimary),
                 QLatin1String(Style::kPanelBg),
                 QLatin1String(Style::kBorder));

    for (int i = 0; i < s.size(); ++i) {
        QWidget* seite = s.at(i);
        // Lange Namen kuerzen: "Rotor / Log" neben zwei anderen
        // Pillen sprengt sonst eine schmale Spalte.
        QString text = nameVon(seite);
        if (text.size() > 11) { text = text.left(10) + QStringLiteral("…"); }
        auto* b = new QPushButton(text, m_chip);
        b->setCheckable(true);
        b->setChecked(i == aktiv);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(css);
        b->setFixedHeight(16);
        b->setToolTip(tr("%1 — right-click takes it out, "
                         "double-click unfolds them all.").arg(nameVon(seite)));
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(b, &QPushButton::clicked, this, [this, i]() { zeigeSeite(i); });
        connect(b, &QWidget::customContextMenuRequested, this,
                [this, seite](const QPoint&) { gibFrei(seite); });
        b->installEventFilter(this);
        b->setProperty("kombiSeite", i);
        m_pillenReihe->addWidget(b);
        m_pillen.append(b);
    }

    m_chip->setVisible(!s.isEmpty());
    // Der eigene Streifen ist dann ueberfluessig -- genau das war der
    // Punkt: er kostete Hoehe.
    if (m_reiterleiste) { m_reiterleiste->setVisible(false); }
}

// Der Name einer Seite. Ein Applet kennt seinen Titel selbst; alles
// andere (Rotor, Logbuch) traegt ihn als Fenstertitel.
QString KombiApplet::nameVon(const QWidget* w)
{
    if (!w) { return QString(); }
    if (auto* a = qobject_cast<const AppletWidget*>(w)) {
        return a->appletTitle();
    }
    if (!w->windowTitle().isEmpty()) { return w->windowTitle(); }
    return w->objectName().isEmpty() ? QStringLiteral("Panel")
                                     : w->objectName();
}


QList<QWidget*> KombiApplet::seiten() const
{
    QList<QWidget*> raus;
    for (const QPointer<QWidget>& w : m_inhalte) {
        if (w) { raus.append(w.data()); }
    }
    return raus;
}

int KombiApplet::seite() const
{
    // Die Nummer der Seite in seiten(), nicht der Stapelindex -- siehe
    // zeigeSeite(): die beiden Reihenfolgen muessen nicht gleich sein.
    if (!m_stapel) { return 0; }
    const int i = seiten().indexOf(m_stapel->currentWidget());
    return qMax(0, i);
}

QWidget* KombiApplet::aktuelleSeiteForTest() const
{
    return m_stapel ? m_stapel->currentWidget() : nullptr;
}

bool KombiApplet::istLeer() const
{
    return m_inhalte.isEmpty();
}

void KombiApplet::zeigeSeite(int welche)
{
    if (m_inhalte.isEmpty()) { return; }
    // ── Aufgeklappt faellt beim Umschalten zu (2026-11-01) ──────────
    //
    // Betreiber, mit Bild: „die pillen funktionieren nicht, kann
    // nicht umschalten". Auf dem Bild lagen Rotor UND TX
    // untereinander -- das Feld war aufgeklappt (Doppelklick). Dann
    // ist der Stapel gar nicht zu sehen, und ein Klick auf eine Pille
    // aendert nur etwas Unsichtbares.
    //
    // Wer eine Pille waehlt, meint diese EINE Seite. Also zuklappen
    // und sie zeigen.
    if (m_aufgeklappt) { setAufgeklappt(false); }
    // Betreiber 2026-11-04: "RX/TX -- entweder RX oder TX (Pille
    // schaltet nicht um)". Pille RX hell, Inhalt TX: die Pille zaehlt
    // ueber seiten() (die Reihenfolge von m_inhalte), der Stapel hatte
    // eine ANDERE Reihenfolge -- ein Panel, das im aufgeklappten
    // Zustand hereinkam, steht im Stapel vorn (nimmAuf: addWidget) und
    // in m_inhalte hinten; setAufgeklappt(false) haengt den Rest
    // dahinter. Index gegen Index ging dann daneben. Darum ueber das
    // Widget: die Seite, die die Pille meint, wird gezeigt -- und wer
    // nicht im Stapel ist, kommt hinein.
    const QList<QWidget*> s = seiten();
    if (s.isEmpty()) { return; }
    const int w = qBound(0, welche, s.size() - 1);
    if (m_stapel) {
        QWidget* ziel = s.at(w);
        if (m_stapel->indexOf(ziel) < 0) { m_stapel->addWidget(ziel); }
        m_stapel->setCurrentWidget(ziel);
    }
    for (int i = 0; i < m_reiter.size(); ++i) {
        m_reiter.at(i)->setChecked(i == w);
    }
    AppSettings::instance().setValue(seitenSchluessel(m_kennung), QString::number(w));
    chipNachziehen();
    emit seiteGewechselt(w);
}

// Was drin liegt, steht als Liste von Applet-Kennungen in den
// Einstellungen -- so findet der naechste Start dieselben Panels
// wieder zusammen.
void KombiApplet::merkeStand() const
{
    QStringList ids;
    for (const QPointer<QWidget>& w : m_inhalte) {
        if (auto* a = qobject_cast<AppletWidget*>(w.data())) {
            ids << a->appletId();
        } else if (w) {
            ids << w->objectName();
        }
    }
    AppSettings::instance().setValue(standSchluessel(m_kennung),
                                     ids.join(QLatin1Char(',')));
}

void KombiApplet::syncFromModel()
{
    // ALLE, nicht nur die sichtbare: wer umschaltet, soll keinen
    // veralteten Stand vorfinden.
    // Nur die Applets koennen das -- ein Rotorfenster hat keinen
    // Modellabgleich.
    for (const QPointer<QWidget>& w : m_inhalte) {
        if (auto* a = qobject_cast<AppletWidget*>(w.data())) {
            a->syncFromModel();
        }
    }
}

bool KombiApplet::hasExtendedSettings() const
{
    const QList<QWidget*> s = seiten();
    const int i = seite();
    if (i < 0 || i >= s.size()) { return false; }
    auto* a = qobject_cast<AppletWidget*>(s.at(i));
    return a && a->hasExtendedSettings();
}

void KombiApplet::openExtendedSettings()
{
    const QList<QWidget*> s = seiten();
    const int i = seite();
    if (i < 0 || i >= s.size()) { return; }
    if (auto* a = qobject_cast<AppletWidget*>(s.at(i))) {
        a->openExtendedSettings();
    }
}

} // namespace Longpath
