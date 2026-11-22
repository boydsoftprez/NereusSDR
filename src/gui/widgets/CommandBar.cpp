// =================================================================
// src/gui/widgets/CommandBar.cpp  (Longpath)
// =================================================================
//
// Longpath-original. See CommandBar.h for what this is and why the
// band and filter groups are deliberately absent.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-09-28 -- the filter "…" reads FilterPresetStore (Thetis names +
//                 the operator's own presets), Martin Fischer, OE5SOS,
//                 AI-assisted via Anthropic Claude.
//   2026-09-28 -- the running-filter pill says the preset name when the
//                 filter is a preset (same rule as the RX applet).
// =================================================================

#include "gui/widgets/CommandBar.h"

#include "gui/ScopedChildWidget.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/DspQuickPopups.h"
#include "core/AppSettings.h"
#include "models/FilterPresetStore.h"
#include "models/SliceModel.h"

#include <QHBoxLayout>
#include <QEvent>
#include <QPainter>
#include <QStyleOption>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

#include <cstdlib>

namespace Longpath {
namespace {

/// Die Versalzeile über einer Gruppe. 8 px, weite Laufweite,
/// Skalenfarbe — sie soll benennen, nicht mitreden.
QLabel* captionLabel(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text.toUpper(), parent);
    QFont f = l->font();
    f.setPixelSize(9);
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.6);
    l->setFont(f);
    l->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                         .arg(QString::fromLatin1(Style::kTextScale)));
    return l;
}

} // namespace

/// Das Aussehen einer Pille. Über Style::-Konstanten, damit die
/// Theme-Datei sie erreicht. Oeffentlich, weil die untere Leiste
/// dieselbe Optik traegt.
QString CommandBar::pillStyle()
{
    // Glas & Tiefe (2026-09-17, Stilblatt 3): die Pille ist ERHABEN —
    // Verlauf, Lichtkante oben, dunkle Unterkante — und die aktive
    // traegt den gedeckten Auswahlverlauf statt des flachen kBlueBg.
    // Das nimmt der Kopfleiste die Blau-Summe (fuenf leuchtende Knoepfe
    // in einer Reihe), ohne dass die Auswahl leiser wuerde.
    return QStringLiteral(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "               stop:0 %1, stop:1 %10);"
        "  border: 1px solid %2; border-radius: %6px;"
        "  border-top-color: %11; border-bottom-color: %12;"
        "  color: %3; font-size: 11px; font-weight: 600;"
        "  padding: 0 11px; min-height: %5px; max-height: %5px;"
        "}"
        "QPushButton:hover { background: %4; }"
        // Genau einer je Gruppe leuchtet. Der Rest ist fast unsichtbar —
        // das ist der Unterschied zwischen einer Leiste und einer Wand.
        "QPushButton:checked {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "               stop:0 %7, stop:1 %13);"
        "  border: 1px solid %8; border-top-color: %14; color: %9;"
        "}")
        .arg(QString::fromLatin1(Style::kGlassBtnTop),
             QString::fromLatin1(Style::kBorder),
             QString::fromLatin1(Style::kTextSecondary),
             QString::fromLatin1(Style::kButtonHover))
        .arg(CommandBar::kPillHeight)
        .arg(CommandBar::kPillRadius)
        .arg(QString::fromLatin1(Style::kGlassSelTop),
             QString::fromLatin1(Style::kGlassSelBorder),
             QString::fromLatin1(Style::kGlassSelText))
        .arg(QString::fromLatin1(Style::kGlassBtnBot),
             Style::shiftL(QColor(Style::kBorder), 12),
             Style::shiftL(QColor(Style::kBorder), -14),
             QString::fromLatin1(Style::kGlassSelBot),
             Style::shiftL(QColor(Style::kGlassSelBorder), 16));
}

CommandBar::CommandBar(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    // Leichtes Rendering (2026-09-26): dieselbe Platte wie die Panels
    // (Verlauf) und oben die Lichtkante aus HAUSSTIL.md "Weiche
    // Uebergaenge" -- vorher flach kPanelBg.
    setStyleSheet(QStringLiteral(
        "CommandBar { background: %1; border: 1px solid %2;"
        "             border-top-color: %3; border-radius: 6px; }")
        .arg(Style::glassPanelFill(),
             QString::fromLatin1(Style::kBorderSubtle),
             QLatin1String(Style::kGlassLight)));

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(13, 9, 13, 10);
    // Der Abstand ZWISCHEN den Gruppen ist das, was sie zu Gruppen
    // macht. Fünf Pixel innerhalb, zweiundzwanzig dazwischen.
    row->setSpacing(22);

    m_row = row;
    // ── BAND zuerst ─────────────────────────────────────────────────
    //
    // Der Betreiber am 2026-08-22: "bandwechsel sollte auch mit
    // buttons möglich sein, am besten in der leiste oben."
    //
    // Ganz links, weil das Band die groebste Wahl ist: erst das Band,
    // dann Betriebsart, Filter, Schrittweite. Dieselbe Reihenfolge wie
    // in der Vorlage und bei Thetis.
    buildBandGroup(row);
    buildModeGroup(row);
    buildDigiGroup(row);
    buildFilterGroup(row);
    buildStepGroup(row);
    buildNrGroup(row);
    buildRateGroup(row);
    verteileGruppen();
    zieheLeereGruppenZurueck();
}

// ── Die leise Pille (2026-10-07) ─────────────────────────────────────
//
// Gleiche Masse wie pillStyle(), damit beim Verbinden nichts springt:
// derselbe Innenabstand, dieselbe min-/max-Hoehe. Nur Fuellung, Rand
// und Schrift treten zurueck. Kein Verlauf -- "Glas & Tiefe" heisst
// erhaben, und etwas, das gerade nichts kann, soll nicht erhaben sein.
QString CommandBar::pillStyleGedaempft()
{
    return QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  color: %3; font-size: 11px; font-weight: 600;"
        "  padding: 0 11px; min-height: %4px; max-height: %4px;"
        "  border-radius: %5px;"
        "}"
        "QPushButton:hover { color: %6; border: 1px solid %7; }")
        .arg(QString::fromLatin1(Style::kPanelBg),
             QString::fromLatin1(Style::kBorderSubtle),
             QString::fromLatin1(Style::kTextScale))
        .arg(CommandBar::kPillHeight)
        .arg(CommandBar::kPillRadius)
        .arg(QString::fromLatin1(Style::kTextSecondary),
             QString::fromLatin1(Style::kBorder));
}

void CommandBar::addGroupWidget(const QString& caption, QWidget* w)
{
    if (!w || !m_row) { return; }
    // Wie addGroup(): Versalzeile oben, Inhalt darunter -- damit das
    // Bauteil auf derselben Linie sitzt wie die Pillen daneben und die
    // Ueberschrift mit BAND/MODE/FILTER/RATE fluchtet.
    auto* box = new QVBoxLayout;
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(5);
    box->addWidget(captionLabel(caption, this), 0, Qt::AlignLeft);
    box->addWidget(w, 0, Qt::AlignLeft);
    m_row->insertLayout(dehnungsPlatz(), box);
    m_gruppenWidgets.insert(w, box);
    verteileGruppen();
}

int CommandBar::dehnungsPlatz() const
{
    // Die Stelle der Dehnung SUCHEN, nicht raten.
    //
    // Hier stand m_row->count() - 1 mit der Begruendung "buildGroups()
    // haengt sie als letztes an". Das stimmt nur, solange niemand
    // addTrailing() gerufen hat -- das haengt WEITER hinten an. Danach
    // ist das letzte Element der Plus-Knopf, und die Gruppe landete
    // zwischen Dehnung und Plus, also ganz rechts statt neben der Rate.
    //
    // Am 2026-10-07 beim Gegenlesen gefunden: schon der erste Wechsel von
    // links zurueck in die Leiste setzte die Abzeichen an den rechten
    // Rand, weil das Plus beim Aufbau laengst angehaengt war.
    // 2026-10-07, zweite Fassung: seit verteileGruppen() steht zwischen
    // JEDEM Gruppenpaar eine Dehnung. "Die erste Dehnung" waere jetzt
    // die zwischen BAND und MODE -- die Profile landeten gleich hinter
    // dem Band. Also nicht mehr nach der Dehnung suchen, sondern nach
    // der letzten Gruppe: dort gehoert eine neue Gruppe hin, und das
    // bleibt richtig, wie viele Dehnungen auch dazwischenstehen.
    if (!m_row) { return 0; }
    for (int i = m_row->count() - 1; i >= 0; --i) {
        if (m_row->itemAt(i) && m_row->itemAt(i)->layout()) { return i + 1; }
    }
    // Keine Gruppe da: ganz nach vorne. NICHT ans Ende -- dort haengt
    // schon das Plus, und dahinter waere wieder der rechte Rand.
    return 0;
}

QVector<QLayout*> CommandBar::gruppenKaesten() const
{
    QVector<QLayout*> k;
    if (!m_row) { return k; }
    for (int i = 0; i < m_row->count(); ++i) {
        QLayoutItem* it = m_row->itemAt(i);
        if (it && it->layout()) { k.append(it->layout()); }
    }
    return k;
}

// ── Die Gruppen ueber die Breite verteilen ───────────────────────────
//
// Vorher: eine einzige Dehnung ganz hinten. Folge: alle Gruppen klebten
// links zusammen, und rechts standen 168 Pixel tot, in denen allein das
// Plus schwebte.
//
// Jetzt eine Dehnung hinter jeder Gruppe, die nicht das letzte Element
// der Reihe ist. Damit verteilt Qt den uebrigen Platz gleichmaessig --
// und wenn das Fenster schmal wird, schrumpfen alle Dehnungen auf null,
// und es bleibt beim Grundabstand von 22. Eine gerechnete Weite haette
// bei schmalem Fenster ueberlaufen; eine Dehnung kann das nicht.
//
// Die letzte Gruppe bekommt bewusst eine Dehnung, SOLANGE hinter ihr
// noch das Plus haengt -- das schiebt das Plus an den rechten Rand, wo
// es hingehoert. Haengt nichts dahinter, bekommt sie keine, sonst
// ruecke alles nach links und die Verteilung waere dahin.
void CommandBar::verteileGruppen()
{
    if (!m_row) { return; }

    for (int i = m_row->count() - 1; i >= 0; --i) {
        QLayoutItem* it = m_row->itemAt(i);
        if (it && it->spacerItem()) { delete m_row->takeAt(i); }
    }

    QVector<int> kaesten;
    for (int i = 0; i < m_row->count(); ++i) {
        QLayoutItem* it = m_row->itemAt(i);
        if (it && it->layout()) { kaesten.append(i); }
    }
    const int letztes = m_row->count() - 1;
    for (int k = kaesten.size() - 1; k >= 0; --k) {
        const int i = kaesten.at(k);
        if (i == letztes) { continue; }
        m_row->insertStretch(i + 1, 1);
    }
}

void CommandBar::removeGroupWidget(QWidget* w)
{
    // Das Gegenstueck zu addGroupWidget. Ohne das blieb die Versalzeile
    // ("PROFIL") samt leerem Teil-Layout in der Leiste stehen, wenn das
    // Bauteil wegging -- und beim naechsten Hinzufuegen kam eine zweite
    // dazu. Nach drei Wechseln standen drei Ueberschriften nebeneinander
    // (2026-10-07 offscreen nachgemessen).
    if (!w || !m_row) { return; }
    const auto it = m_gruppenWidgets.constFind(w);
    if (it == m_gruppenWidgets.constEnd()) { return; }
    QLayout* box = it.value();
    m_gruppenWidgets.erase(it);
    if (!box) { return; }
    // Das Bauteil selbst gehoert dem Aufrufer -- nur aus der Anordnung
    // nehmen, nicht loeschen. Alles andere darin (die Versalzeile) ist
    // unser und geht mit.
    box->removeWidget(w);
    while (QLayoutItem* item = box->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    m_row->removeItem(box);
    delete box;
    verteileGruppen();
}

// ── Die Trennstriche ─────────────────────────────────────────────────
//
// Jedes andere Bauteil im Fenster ist eingefasst: BANDWIDTH FILTER,
// PANADAPTER, S-METER, TX, FREQUENZ, ROTOR/LOG -- alle mit Rahmen und
// Versalzeile. Die Kommandoleiste war die einzige Stelle, an der
// Gruppen ueberhaupt keine Grenze hatten; vierundzwanzig fast gleiche
// Pillen in einer Reihe, und das Auge hat keinen Halt.
//
// Der Haarstrich ist die leiseste Form derselben Aussage. kGlassEdgeLight
// ist dafuer die Hausfarbe (Lichtkante auf kBorder) -- kein neuer Ton.
//
// Gezeichnet, nicht gebaut: ein Strich-Widget waere ein weiteres Element
// in der Reihe, an dem dehnungsPlatz(), addGroupWidget() und
// removeGroupWidget() vorbeizielen koennten.
void CommandBar::paintEvent(QPaintEvent* e)
{
    // Erst das Stilblatt zeichnen lassen -- die Leiste traegt
    // WA_StyledBackground, und ohne diesen Aufruf bliebe ihr Grund leer.
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QWidget::paintEvent(e);

    const QVector<QLayout*> k = gruppenKaesten();
    if (k.size() < 2) { return; }

    p.setPen(QPen(QColor(QString::fromLatin1(Style::kGlassEdgeLight)), 1));
    for (int i = 0; i + 1 < k.size(); ++i) {
        const QRect a = k.at(i)->geometry();
        const QRect b = k.at(i + 1)->geometry();
        if (!a.isValid() || !b.isValid()) { continue; }
        // Genau in die Mitte der Luecke. Am Rand einer Gruppe klebend
        // waere er ihr Rahmen und gehoerte dann auch ringsum.
        const int x = (a.right() + b.left()) / 2;
        const int oben  = qMin(a.top(), b.top());
        const int unten = qMax(a.bottom(), b.bottom());
        p.drawLine(x, oben, x, unten);
    }
}

// ── Keine alten Striche (2026-11-21) ─────────────────────────────────
//
// Betreiber: „wieder sind die trennlinien oben falsch - schaue oben bei
// ft8 und bei 192". Auf seinem Bild standen neben den richtigen Strichen
// lange an alten Stellen, halb hinter den Pillen. Die Striche malt die
// Leiste selbst (paintEvent); ruecken die Gruppen aber, ohne dass die
// Leiste ihre Groesse aendert -- andere Pillen-Texte nach dem Wechsel
// Flex -> Anvelina, eine Pille mehr oder weniger --, zeichnet Qt nur die
// verschobenen Pillen neu. Die alten Striche bleiben dann als Bildreste
// stehen. tst_command_bar hat 508 solche Punkte gezaehlt.
//
// Darum: jede neue Anordnung (LayoutRequest, nachdem Qt sie ausgefuehrt
// hat) zeichnet die ganze Leiste neu. Das kommt selten und kostet eine
// Leiste aus Pillen.
bool CommandBar::event(QEvent* e)
{
    const bool erledigt = QWidget::event(e);
    if (e->type() == QEvent::LayoutRequest) {
        update();
    }
    return erledigt;
}

void CommandBar::addTrailing(QWidget* w)
{
    if (!w || !m_row) { return; }
    // Rechtsbündig: die Dehnung steht schon davor, angehängt wird
    // dahinter. Die Ausrichtung unten hält das Plus auf einer Linie mit
    // den Pillen und nicht mit den Versalzeilen darüber.
    m_row->addWidget(w, 0, Qt::AlignBottom);
    // Erst jetzt steht fest, dass hinter der letzten Gruppe noch etwas
    // haengt -- sie braucht also doch eine Dehnung, damit das Plus an
    // den rechten Rand wandert. Siehe verteileGruppen().
    verteileGruppen();
}

// ── Gruppen bauen ────────────────────────────────────────────────────

CommandBar::Group& CommandBar::addGroup(const QString& caption,
                                        QHBoxLayout* row)
{
    auto* box = new QVBoxLayout;
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(5);
    QLabel* versal = captionLabel(caption, this);
    box->addWidget(versal, 0, Qt::AlignLeft);

    auto* pills = new QHBoxLayout;
    pills->setContentsMargins(0, 0, 0, 0);
    pills->setSpacing(5);
    box->addLayout(pills);
    row->addLayout(box);

    auto* g = new Group{caption, pills, {}, nullptr, versal};
    m_groups.append(g);
    return *g;
}

namespace {

// ── Die Pille darf nie schmaler sein als ihre Schrift ────────────────
//
// „advance + 20" stand hier seit dem 2026-08-23 und war damals
// richtig. Seit „Glas & Tiefe" (2026-09-17) tragen die Pillen aber
// Rand und Polsterung aus dem Stylesheet, und die zwanzig Punkte
// decken das nicht mehr. Gemessen am laufenden Programm (2026-10-15,
// Fenster 796 px): JEDE Pille stand genau auf ihrer gesetzten
// Mindestbreite, und die lag durchweg VIER Punkte unter dem, was Qt
// selbst als minimumSizeHint ausrechnet. Darum las sich „40m" als
// „40", „LSB" als „LSI" und „NR1" als „NR" — derselbe Fehler wie
// damals, nur vier Punkte statt vieler.
//
// Also nicht mehr selbst rechnen, sondern das Groessere von beidem
// nehmen: die eigene Schaetzung UND Qts Bedarf, der Rand und
// Polsterung des Stylesheets kennt. ensurePolished() davor, sonst
// antwortet der Hinweis noch ohne Stylesheet.
void setzeMindestbreite(QPushButton* b, const QString& label)
{
    if (!b) { return; }
    b->ensurePolished();

    // Mit DER Schrift rechnen, die das Stylesheet setzt, nicht mit der
    // des Widgets: pillStyle() sagt font-size: 11px und font-weight:
    // 600. b->fontMetrics() antwortet dagegen fuer die geerbte Schrift
    // und kommt zu klein heraus — der zweite Grund, warum die Pillen
    // zu schmal blieben.
    QFont schrift = b->font();
    schrift.setPixelSize(11);
    schrift.setWeight(QFont::DemiBold);

    // Und mit DEM Beiwerk, das dasselbe Stylesheet verlangt:
    // padding 0 11px auf beiden Seiten, dazu 1px Rand. Macht 24 statt
    // der alten 20. Zwei Punkte Luft obendrauf, damit Kantenglaettung
    // und Sperrung nicht an der Kante kratzen.
    const int ausDerSchrift =
        QFontMetrics(schrift).horizontalAdvance(label) + 24 + 2;
    const int ausDemStil = b->minimumSizeHint().width();
    b->setMinimumWidth(qMax(ausDerSchrift, ausDemStil));
}

} // namespace

QPushButton* CommandBar::addPill(Group& g, const QString& label)
{
    auto* b = new QPushButton(label, this);
    b->setCheckable(true);
    b->setStyleSheet(pillStyle());
    b->setCursor(Qt::PointingHandCursor);

    // ── Keine Pille schmaler als ihre Schrift ───────────────────────
    //
// Am 2026-08-23 live gesehen, nachdem die BAND-Gruppe dazukam:
    // aus "10 Hz" wurde "I0 Hz", aus "100 Hz" wurde "00 H". Die
    // Anordnung hatte keinen Platz mehr und hat die Pillen unter ihre
    // eigene Schrift gequetscht — eine halb gelesene Schrittweite ist
    // schlimmer als keine.
    //
    // Mindestbreite aus der Schriftmetrik plus Innenabstand: darunter
    // darf die Anordnung nicht. Wird es zu eng, muss sich etwas
    // anderes ruehren (die Leiste rollt), nicht die Beschriftung.
    setzeMindestbreite(b, label);
    g.pills.append(b);
    if (g.row) { g.row->addWidget(b); }
    return b;
}

// ── Ohne Funkgeraet tritt eine leere Gruppe zurueck ──────────────────
//
// Die Regel steht schon im Fenster: POWER, SWR und SIGNAL AVERAGE
// zeigen ihre "—" gedaempft. FILTER und RATE taten es nicht -- drei
// helle Pillen mit einem Strich darin sehen nicht nach "noch nichts"
// aus, sondern nach kaputt.
//
// Die Masse bleiben gleich (siehe pillStyleGedaempft), es springt also
// nichts, wenn das Geraet kommt.
bool CommandBar::gruppeIstLeer(const Group& g) const
{
    // RATE traegt seinen Wert in einem eigenen Schild, nicht in Pillen
    // -- ohne diese Abzweigung waere die Gruppe immer "leer", auch mit
    // "48 kHz" darin.
    if (g.pills.isEmpty()) {
        return !m_rateLabel || m_rateLabel->text() == QStringLiteral("—");
    }
    for (QPushButton* b : g.pills) {
        if (!b) { continue; }
        const QString t = b->text();
        // Das "…" ist kein Inhalt, es ist der Weg zu mehr davon.
        if (t == QStringLiteral("…") || t == QStringLiteral("—")) { continue; }
        return false;
    }
    return true;
}

void CommandBar::zieheLeereGruppenZurueck()
{
    for (Group* g : m_groups) {
        if (!g) { continue; }
        const bool leer = gruppeIstLeer(*g);
        for (QPushButton* b : g->pills) {
            if (b) { b->setStyleSheet(leer ? pillStyleGedaempft() : pillStyle()); }
        }
        if (g->caption) {
            // Zurueck auf kTextScale, nicht auf einen leeren Stil:
            // captionLabel() setzt die Farbe selbst, und ein leeres
            // Stilblatt haette die Versalzeile auf die Vorgabefarbe
            // gestellt -- heller als vorher statt gleich.
            g->caption->setStyleSheet(
                QStringLiteral("QLabel { color: %1; }")
                    .arg(QString::fromLatin1(leer ? Style::kBorderMuted
                                                  : Style::kTextScale)));
        }
    }
}

void CommandBar::setActive(Group& g, const QString& label)
{
    for (QPushButton* b : g.pills) {
        b->setChecked(b->text() == label);
    }
}

void CommandBar::buildModeGroup(QHBoxLayout* row)
{
    // Der vollständige Satz für das „…". SPEC und DRM bleiben draußen:
    // das eine ist kein Betriebsmodus, das andere hat in diesem
    // Programm keinen Decoder.
    m_allModes = {
        {QStringLiteral("LSB"),  DSPMode::LSB},
        {QStringLiteral("USB"),  DSPMode::USB},
        {QStringLiteral("CWL"),  DSPMode::CWL},
        {QStringLiteral("CWU"),  DSPMode::CWU},
        {QStringLiteral("AM"),   DSPMode::AM},
        {QStringLiteral("SAM"),  DSPMode::SAM},
        {QStringLiteral("FM"),   DSPMode::FM},
        {QStringLiteral("DIGL"), DSPMode::DIGL},
        {QStringLiteral("DIGU"), DSPMode::DIGU},
        {QStringLiteral("DSB"),  DSPMode::DSB},
    };

    Group& g = addGroup(QStringLiteral("Mode"), row);
    for (int i = 0; i < kVisiblePerGroup && i < m_allModes.size(); ++i) {
        const auto entry = m_allModes.at(i);
        QPushButton* b = addPill(g, entry.first);
        connect(b, &QPushButton::clicked, this, [this, entry]() {
            pushModeToModel(entry.second);
        });
    }

    // ── Das „…" ──────────────────────────────────────────────────────
    //
    // Drei vorne, der Rest im Menü. Wählt jemand dort etwas aus, tritt
    // es an die dritte Stelle — sonst wäre der eingeschaltete Modus
    // unsichtbar, sobald er nicht zu den ersten dreien gehört, und die
    // Leiste würde lügen.
    addOverflow(g, m_allModes,
                [this](DSPMode m) { pushModeToModel(m); });

}

// ── Digital: eine eigene Gruppe, nicht zwei Pillen mehr bei Mode ─────
//
// Betreiber am 2026-10-21, vor der Antenne stehend: „ich will, sobald
// ich auf ft8 klicke, dass alles automatisch funktioniert", „ich will
// nicht die freq suchen", „quasi wie jtdx".
//
// Zuerst standen FT8 und FT4 als zwei weitere Pillen bei Mode. Das
// fiel in der CI sofort auf, und zu Recht: `tst_command_bar` haelt
// fest, dass JEDE Gruppe genau kVisiblePerGroup Pillen zeigt -- „drei
// vorne, der Rest im Menue" ist der ganze Grund, warum diese Leiste
// schmaler ist als die Filterwand im RX-Panel.
//
// Eine eigene Gruppe ist ohnehin das Richtigere: es sind keine
// Betriebsarten des Geraets. Das Geraet steht dabei auf DIGU; was FT8
// von FT4 unterscheidet, ist die Anrufstelle und der Dekoder, nicht
// die Demodulation. Zwei Pillen, kein „…" -- mehr gibt es nicht.
// Nur diese eine Gruppe nachziehen.
//
// Eine eigene Funktion und nicht `pullFromModel()`, weil sie an
// `frequencyChanged` haengt: beim Drehen am Knopf kommt das Signal
// dutzendfach je Sekunde, und die ganze Leiste neu zu stellen waere
// dafuer zu teuer. Hier sind es zwei Knoepfe.
void CommandBar::aktualisiereDigiLampe()
{
    Group* g = group(QStringLiteral("Digi"));
    if (!g) { return; }
    if (!m_slice) { setActive(*g, QString()); return; }

    // ── Die Lampe zeigt den DEKODER, nicht die Frequenz ──────────
    //
    // Bis zum 2026-10-30 las sie `stehtSchonDort(frequenz)`. Der
    // Betreiber stand an dem Abend auf 7.1275 MHz -- keine
    // Anrufstelle, also beide Pillen grau -- waehrend der Dekoder
    // durchgehend FT4 fuhr. Er sagte dazu: „war aber auf ft8", und
    // konnte es nicht besser wissen: die einzige Stelle, an der FT4
    // stand, war der Knopf IM FT8-Fenster.
    //
    // Zwei Anzeigen, die dasselbe meinen und Verschiedenes sagen.
    // Jetzt sagt die Lampe, was der Dekoder tut -- danach richtet
    // sich, was ankommt. Wo die Scheibe steht, sagt die Skala.
    const DSPMode m = m_slice->dspMode();
    const bool datenmodus = (m == DSPMode::DIGU || m == DSPMode::DIGL);
    QString label;
    if (datenmodus) {
        using DigiFrequenzen::Digimodus;
        if (m_digiDekoderLaeuft) {
            // Der Dekoder hat Vorrang: was er faehrt, bestimmt, was
            // ankommt. Die Frequenz sagt nur, WO man steht, und das
            // sagt die Skala schon.
            label = (m_digiBetriebsart == Digimodus::Ft4)
                        ? QStringLiteral("FT4") : QStringLiteral("FT8");
        } else {
            // Ohne Dekoder bleibt die alte Auskunft: stehe ich auf
            // einer Anrufstelle? Sie darf NICHT schon bei DIGU
            // angehen -- in DIGU sitzt man auch auf PSK oder Olivia,
            // und ein Knopf, der dann leuchtet, luegt.
            const auto f = qint64(m_slice->frequency());
            if (DigiFrequenzen::stehtSchonDort(f, Digimodus::Ft8)) {
                label = QStringLiteral("FT8");
            } else if (DigiFrequenzen::stehtSchonDort(f, Digimodus::Ft4)) {
                label = QStringLiteral("FT4");
            }
        }
    }
    setActive(*g, label);
}

void CommandBar::setzeDigiDekoder(bool laeuft,
                                  DigiFrequenzen::Digimodus modus)
{
    if (m_digiDekoderLaeuft == laeuft && m_digiBetriebsart == modus) {
        return;
    }
    m_digiDekoderLaeuft = laeuft;
    m_digiBetriebsart   = modus;
    aktualisiereDigiLampe();
}

void CommandBar::buildDigiGroup(QHBoxLayout* row)
{
    Group& g = addGroup(QStringLiteral("Digi"), row);
    for (const auto& eintrag : {
             std::pair{QStringLiteral("FT8"), DigiFrequenzen::Digimodus::Ft8},
             std::pair{QStringLiteral("FT4"), DigiFrequenzen::Digimodus::Ft4}}) {
        QPushButton* b = addPill(g, eintrag.first);
        connect(b, &QPushButton::clicked, this,
                [this, modus = eintrag.second]() { emit digiRequested(modus); });
    }
}

void CommandBar::buildBandGroup(QHBoxLayout* row)
{
    // Vorne 40, 20, 15 — auf Ansage des Betreibers (2026-08-22):
    // "bänder sollten 40,20,15 ersichtlich sein, rest dann mit einem
    // fenster mit punkten ... 160, 80, 60, 30, 10, 6".
    //
    // Die Reihenfolge dahinter ist seine: erst die sechs, die er
    // genannt hat, dann der Rest. Was vorne steht, ist ohne Umweg
    // erreichbar; alles Weitere liegt hinter dem "…".
    m_allBands = {
        {QStringLiteral("40m"),  Band::Band40m},
        {QStringLiteral("20m"),  Band::Band20m},
        {QStringLiteral("15m"),  Band::Band15m},
        {QStringLiteral("160m"), Band::Band160m},
        {QStringLiteral("80m"),  Band::Band80m},
        {QStringLiteral("60m"),  Band::Band60m},
        {QStringLiteral("30m"),  Band::Band30m},
        {QStringLiteral("10m"),  Band::Band10m},
        {QStringLiteral("6m"),   Band::Band6m},
        {QStringLiteral("17m"),  Band::Band17m},
        {QStringLiteral("12m"),  Band::Band12m},
        {QStringLiteral("GEN"),  Band::GEN},
        {QStringLiteral("WWV"),  Band::WWV},
    };

    Group& g = addGroup(QStringLiteral("Band"), row);
    for (int i = 0; i < kVisiblePerGroup && i < m_allBands.size(); ++i) {
        const auto entry = m_allBands.at(i);
        QPushButton* b = addPill(g, entry.first);
        b->setProperty("bandValue", static_cast<int>(entry.second));
        connect(b, &QPushButton::clicked, this, [this, entry]() {
            pushBandToModel(entry.second);
        });
    }
    addOverflow(g, m_allBands,
                [this](Band b) { pushBandToModel(b); });
}

void CommandBar::buildStepGroup(QHBoxLayout* row)
{
    m_allSteps = {
        {QStringLiteral("10 Hz"),   10},
        {QStringLiteral("100 Hz"),  100},
        {QStringLiteral("1 kHz"),   1000},
        {QStringLiteral("50 Hz"),   50},
        {QStringLiteral("500 Hz"),  500},
        {QStringLiteral("5 kHz"),   5000},
        {QStringLiteral("9 kHz"),   9000},
        {QStringLiteral("10 kHz"),  10000},
    };

    Group& g = addGroup(QStringLiteral("Step"), row);
    for (int i = 0; i < kVisiblePerGroup && i < m_allSteps.size(); ++i) {
        const auto entry = m_allSteps.at(i);
        QPushButton* b = addPill(g, entry.first);
        connect(b, &QPushButton::clicked, this, [this, entry]() {
            pushStepToModel(entry.second);
        });
    }

    addOverflow(g, m_allSteps, [this](int hz) { pushStepToModel(hz); });
}

// Ein Helfer statt zweimal derselbe Block: der Ueberlauf ist fuer jede
// Gruppe dasselbe, und zwei Kopien waeren zwei Stellen, an denen der
// naechste Gruppentyp vergessen wird.
template <typename Entry, typename Apply>
void CommandBar::addOverflow(Group& g, const QVector<Entry>& all, Apply apply)
{
    auto* more = new QPushButton(QStringLiteral("…"), this);
    more->setStyleSheet(pillStyle());
    more->setCursor(Qt::PointingHandCursor);
    more->setToolTip(tr("Show all %1").arg(all.size()));
    g.overflow = more;
    if (g.row) { g.row->addWidget(more); }

    connect(more, &QPushButton::clicked, this, [this, all, apply]() {
        // Am Fenster, nicht an der Leiste: die Leiste (oder ihr Elternteil)
        // hat ein eigenes natives Fenster, und ein Menue daran bekam von Qt
        // keinen Fensterbezug -- „QWidgetClassWindow must be a top level
        // window." im Protokoll vom 2026-09-29, jedesmal kurz vor einem
        // Bandwechsel. Wie ProfileRail (2552eadc).
        ScopedChildWidget<QMenu> menuOwner(window());   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
        QMenu& m = *menuOwner.get();
        for (const auto& e : all) {
            QAction* a = m.addAction(e.first);
            connect(a, &QAction::triggered, this,
                    [apply, e]() { apply(e.second); });
        }
        m.exec(QCursor::pos());
    });
}

// ── Modellanbindung ──────────────────────────────────────────────────

void CommandBar::attach(SliceModel* slice)
{
    // Die alten Verbindungen zuerst lösen. Ohne das hängt die Leiste
    // nach einem Pan-Wechsel an beiden Ketten und meldet abwechselnd
    // deren Modus — ein Fehler, der erst beim zweiten Pan auffällt und
    // dann wie ein Wackelkontakt aussieht.
    for (const auto& c : m_links) { disconnect(c); }
    m_links.clear();

    m_slice = slice;
    if (!slice) { return; }

    m_links << connect(slice, &SliceModel::dspModeChanged,
                       this, [this](DSPMode) { pullFromModel(); });
    m_links << connect(slice, &SliceModel::stepHzChanged,
                       this, [this](int) { pullFromModel(); });
    m_links << connect(slice, &SliceModel::filterChanged,
                       this, [this](int, int) { pullFromModel(); });
    m_links << connect(slice, &SliceModel::activeNrChanged,
                       this, [this](NrSlot) { pullFromModel(); });
    m_links << connect(slice, &SliceModel::bandChanged,
                       this, [this](Longpath::Band) { pullFromModel(); });
    // Die Digi-Lampe haengt an der genauen Frequenz, nicht nur am
    // Band: innerhalb von 20 m entscheidet sie zwischen FT8 (14.074),
    // FT4 (14.080) und „keins von beiden". Darum hier das feine
    // Signal -- aber nur fuer diese Gruppe, siehe
    // aktualisiereDigiLampe().
    m_links << connect(slice, &SliceModel::frequencyChanged,
                       this, [this](double) { aktualisiereDigiLampe(); });
    pullFromModel();
}

// ── Filter ───────────────────────────────────────────────────────────
//
// Drei Breiten vorne, der Rest im „…", und darin auch eine eigene
// Breite zum Eintippen. Auf Ansage vom 2026-08-21, mit Vorlage:
// FILTER 2.7k · 2.9k · 3.3k · …
//
// Die Breiten haengen am MODUS — CW arbeitet mit anderen als SSB.
// Deshalb werden die Pillen nicht fest beschriftet, sondern bei jedem
// Abgleich neu (relabelFilterPills), und ein Klick greift ueber seine
// STELLE in die Liste des aktuellen Modus. Feste Beschriftungen haetten
// in CW die Breiten von SSB angeboten.
void CommandBar::buildFilterGroup(QHBoxLayout* row)
{
    Group& g = addGroup(QStringLiteral("Filter"), row);
    for (int i = 0; i < kVisiblePerGroup; ++i) {
        QPushButton* b = addPill(g, QStringLiteral("—"));
        // ── Die Nutzlast haengt am KNOPF, nicht an seiner Stelle ─────
        //
        // Der erste Entwurf griff ueber den Index in die Preset-Liste
        // des Modus. Das faellt in dem Moment auseinander, in dem eine
        // Pille nachrueckt: laeuft eine Breite, die nicht unter den
        // ersten dreien ist, wird die letzte Pille auf sie
        // umbeschriftet — und der Index zeigte dann weiter auf das
        // dritte Preset. Der Knopf haette „1.8k" gesagt und 3.3k
        // geschaltet.
        //
        // Der bestehende Test exactlyOnePillIsLitPerGroup hat den
        // Entwurf auffliegen lassen, nicht ich.
        b->setProperty("loHz", 0);
        b->setProperty("hiHz", 0);
        connect(b, &QPushButton::clicked, this, [this, b]() {
            if (!m_slice || !b->isEnabled()) { return; }
            pushFilterToModel(b->property("loHz").toInt(),
                              b->property("hiHz").toInt());
        });
    }

    auto* more = new QPushButton(QStringLiteral("…"), this);
    more->setStyleSheet(pillStyle());
    more->setCursor(Qt::PointingHandCursor);
    more->setToolTip(tr("All widths for this mode -- and one of "
                                    "your own to type in."));
    g.overflow = more;
    if (g.row) { g.row->addWidget(more); }

    connect(more, &QPushButton::clicked, this, [this]() {
        if (!m_slice) { return; }
        ScopedChildWidget<QMenu> menuOwner(window());   // am Fenster, siehe addOverflow()
        QMenu& m = *menuOwner.get();
        // Beschriftet mit dem NAMEN der Vorgabe, wie Thetis' Knoepfe —
        // nicht mit der aus den Kanten gerechneten Breite: CW F10 ist
        // ±13 Hz, also 26 Hz breit, und heisst in Thetis „25".
        for (const FilterMenuEntry& e : filterMenuEntries()) {
            const int low = e.low, high = e.high;
            QAction* a = m.addAction(e.name);
            connect(a, &QAction::triggered, this,
                    [this, low, high]() { pushFilterToModel(low, high); });
        }
        m.addSeparator();
        QAction* own = m.addAction(tr("Own width…"));
        connect(own, &QAction::triggered, this,
                [this]() { askForCustomFilter(); });
        m.exec(QCursor::pos());
    });
}

void CommandBar::setMaschinenNr(const std::function<QString(NrSlot)>& namen)
{
    m_maschinenNr = namen;
}

void CommandBar::setFilterPresetStore(FilterPresetStore* store)
{
    m_presetStore = store;
}

QVector<CommandBar::FilterMenuEntry> CommandBar::filterMenuEntries() const
{
    QVector<FilterMenuEntry> out;
    if (!m_slice) { return out; }
    const DSPMode mode = m_slice->dspMode();
    if (m_presetStore) {
        for (const FilterPreset& p : m_presetStore->presetsForMode(mode)) {
            out.append({p.name, p.low, p.high});
        }
        return out;
    }
    const auto edges = SliceModel::presetsForMode(mode);
    const QStringList names = SliceModel::presetNamesForMode(mode);
    for (int i = 0; i < edges.size(); ++i) {
        out.append({names.value(i, filterLabel(edges[i].first, edges[i].second)),
                    edges[i].first, edges[i].second});
    }
    return out;
}

/// Was die Pille des LAUFENDEN Filters sagt: der Name der Vorgabe, wenn er
/// eine ist, sonst die Breite. Dieselbe Regel wie im RX-Applet
/// (RxApplet::updateFilterLabel, dort mit dem Thetis-Bezug) — sonst
/// stuende oben „26" und rechts „25" fuer denselben Filter.
QString CommandBar::runningFilterLabel(int low, int high) const
{
    if (!m_slice) { return filterLabel(low, high); }
    const DSPMode mode = m_slice->dspMode();
    const QString name = m_presetStore
        ? m_presetStore->nameForEdges(mode, low, high)
        : FilterPresetStore::defaultNameForEdges(mode, low, high);
    return name.isEmpty() ? filterLabel(low, high) : name;
}

/// Wie eine Breite in der Leiste heisst: „2.9k" statt „-2900…-100".
/// Der Betrag zaehlt, nicht das Vorzeichen — LSB liegt unter Null und
/// ist trotzdem 2,9 kHz breit.
QString CommandBar::filterLabel(int low, int high)
{
    const int w = qAbs(high - low);
    if (w >= 1000) {
        return QStringLiteral("%1k").arg(w / 1000.0, 0, 'f',
                                         (w % 1000 == 0) ? 0 : 1);
    }
    return QStringLiteral("%1").arg(w);
}

// ── Welche drei vorne stehen ─────────────────────────────────────────
//
// Der Betreiber, 2026-08-21, zuerst „2,7; 2,9; 3,5", Minuten spaeter
// „2,9; 3,2; 3,8 bitte".
//
// Genau deshalb steht die Auswahl in den EINSTELLUNGEN und nicht im
// Quelltext: die erste Bitte enthielt zwei Breiten, die unsere
// Preset-Liste damals nicht kannte (sie fuehrte 5,0 / 4,4 / 3,8 / 3,2 /
// 2,9 / 2,6 / 2,3 / 1,7 / 1,1 / 0,5), die zweite nur bekannte. Seit
// 2026-09-28 fuehrt sie die Thetis-Liste 5,0 / 4,4 / 3,8 / 3,3 / 2,9 /
// 2,7 / 2,4 / 2,1 / 1,8 / 1,0 — die alte war erfunden (siehe
// SliceModel::presetsForMode); jetzt fehlt 3,5 aus der ersten Bitte
// und 3,2 aus der zweiten. Waere die Reihe fest verdrahtet, waere jede
// Meinungsaenderung eine Uebersetzung; so ist sie eine Zahl in einer
// Datei.
//
// Die drei vorderen sind darum keine Presets, sondern BREITEN. Das ist
// auch naeher an der Sache: wer „3,2" sagt, meint eine Breite und
// nicht den soundsovielten Eintrag einer Liste. Das „…" fuehrt
// weiterhin die echten Presets des Modus.
QVector<int> CommandBar::frontWidthsHz()
{
    const QString raw = AppSettings::instance()
        .value(QStringLiteral("CommandBarFilterFront"),
               QStringLiteral("2900,3200,3800")).toString();
    QVector<int> out;
    const QStringList parts = raw.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        bool ok = false;
        const int v = p.trimmed().toInt(&ok);
        if (ok && v >= 50 && v <= 20000) { out.append(v); }
    }
    if (out.isEmpty()) { out = {2900, 3200, 3800}; }
    return out;
}

/// Eine Breite auf die Flanken des laufenden Filters umrechnen: die
/// Mitte bleibt stehen. Sonst spraenge ein LSB-Filter auf die andere
/// Seite des Traegers, sobald man eine Breite waehlt.
QPair<int, int> CommandBar::edgesForWidth(int widthHz) const
{
    if (!m_slice) { return {-widthHz, 0}; }
    const double centre =
        (m_slice->filterLow() + m_slice->filterHigh()) / 2.0;
    return {qRound(centre - widthHz / 2.0), qRound(centre + widthHz / 2.0)};
}

void CommandBar::relabelFilterPills()
{
    Group* g = group(QStringLiteral("Filter"));
    if (!g || !m_slice) { return; }
    const QVector<int> front = frontWidthsHz();
    for (int i = 0; i < g->pills.size(); ++i) {
        QPushButton* b = g->pills.at(i);
        if (i < front.size()) {
            const QPair<int, int> e = edgesForWidth(front.at(i));
            const QString label = filterLabel(e.first, e.second);
            b->setText(label);
            // Dieselbe Mindestbreite wie addPill() beim Anlegen -- nur
            // dort neu berechnet, weil diese Pille mit dem einzelnen
            // Platzhalterzeichen "—" angelegt wird (buildFilterGroup)
            // und addPill()'s Mindestbreite sich an GENAU diesem
            // schmalen Text festgelegt hat. Ohne diese Zeile blieb die
            // Pille auf "—"-Breite gequetscht, sobald hier "2.9k"
            // hineingeschrieben wurde -- derselbe Fehler, den der
            // 2026-08-23-Kommentar in addPill() fuer BAND schon einmal
            // beschreibt ("aus '10 Hz' wurde 'I0 Hz'"), nur an der
            // Stelle, wo die Beschriftung sich NACH dem Anlegen aendert.
            setzeMindestbreite(b, label);
            b->setProperty("loHz", e.first);
            b->setProperty("hiHz", e.second);
            b->setEnabled(true);
            b->setToolTip(QStringLiteral("%1 Hz breit").arg(front.at(i)));
        } else {
            // Weniger als drei Breiten in diesem Modus: die Pille
            // bleibt stehen, aber leer und tot. Sie zu verstecken
            // liesse die Leiste bei jedem Moduswechsel springen.
            b->setText(QStringLiteral("—"));
            b->setEnabled(false);
            b->setToolTip(QString{});
        }
    }
    zieheLeereGruppenZurueck();
}

void CommandBar::pushFilterToModel(int low, int high)
{
    if (m_slice) { m_slice->setFilter(low, high); }
    pullFromModel();
}

// Eine eigene Breite: gefragt wird nach der BREITE, nicht nach zwei
// Flanken. Die Mitte des laufenden Filters bleibt stehen — sonst
// spraenge ein LSB-Filter beim Eintippen auf die andere Seite des
// Traegers, und das will niemand.
void CommandBar::askForCustomFilter()
{
    if (!m_slice) { return; }
    const int low  = m_slice->filterLow();
    const int high = m_slice->filterHigh();
    const int cur  = qAbs(high - low);
    const double centre = (low + high) / 2.0;

    bool ok = false;
    const int w = QInputDialog::getInt(
        this, QStringLiteral("Filterbreite"),
        tr("Width in Hz:"), cur, 50, 20000, 50, &ok);
    if (!ok) { return; }

    pushFilterToModel(qRound(centre - w / 2.0), qRound(centre + w / 2.0));
}

// ── Rauschminderung ──────────────────────────────────────────────────
//
// NR1 bis NR3 vorne, der Rest im „…". Sie schliessen einander aus; ein
// zweiter Klick auf die laufende schaltet sie ab. Das ist derselbe
// Vertrag wie im RX-Feld — dort steht er seit dem 2026-08-18 so, und
// zwei verschiedene Vertraege fuer denselben Schalter waeren die Art
// Unstimmigkeit, die man erst im Betrieb merkt.
void CommandBar::buildNrGroup(QHBoxLayout* row)
{
    // ── Nur, was in DIESEM Bau auch etwas tut ────────────────────────
    //
    // Der Betreiber am 2026-09-17: "bitte kontrollieren nicht nur nr1,
    // nr2 und nr3 sondern auch die anderen optionen im offenen menü."
    // Befund: DFNR (DeepFilterNet3) ist nur enthalten, wenn die
    // Bibliothek beim Bauen gefunden wurde (HAVE_DFNR, siehe
    // ./setup-deepfilter.sh) — in seinem Bau war sie es nicht. BNR
    // (NVIDIA Broadcast) gibt es nur unter Windows mit NVIDIA-Karte
    // (HAVE_BNR, derzeit nirgends gesetzt). MNR (Apple Accelerate) nur
    // auf dem Mac (HAVE_MNR). Trotzdem standen alle acht im „…"-Menue:
    // ein Klick auf DFNR oder BNR leuchtete den Knopf an, schaltete die
    // laufende Minderung AB (die Fahnen sind exklusiv) und tat sonst
    // nichts. Ein Knopf, der angeht und nichts tut, ist eine Luege mit
    // Rueckmeldung — dieselbe Regel, nach der das DSP-Menue in
    // MainWindow MNR/BNR schon lange versteckt. Was fehlt, steht auf
    // der Setup-Seite („DFNR is not enabled in this build"), nicht in
    // der Leiste.
    m_allNr = {
        {QStringLiteral("NR1"),  NrSlot::NR1},
        {QStringLiteral("NR2"),  NrSlot::NR2},
        {QStringLiteral("NR3"),  NrSlot::NR3},
        {QStringLiteral("NR4"),  NrSlot::NR4},
    };
#ifdef HAVE_DFNR
    m_allNr.append({QStringLiteral("DFNR"), NrSlot::DFNR});
#endif
#ifdef HAVE_BNR
    m_allNr.append({QStringLiteral("BNR"),  NrSlot::BNR});
#endif
#ifdef HAVE_MNR
    m_allNr.append({QStringLiteral("MNR"),  NrSlot::MNR});
#endif
    m_allNr.append({QStringLiteral("NNR"),  NrSlot::NNR});

    Group& g = addGroup(QStringLiteral("NR"), row);
    for (int i = 0; i < kVisiblePerGroup && i < m_allNr.size(); ++i) {
        const auto entry = m_allNr.at(i);
        QPushButton* b = addPill(g, entry.first);
        // Nutzlast am Knopf — siehe die Notiz bei den Filterpillen.
        b->setProperty("nrSlot", static_cast<int>(entry.second));
        connect(b, &QPushButton::clicked, this, [this, b]() {
            pushNrToModel(static_cast<NrSlot>(b->property("nrSlot").toInt()));
        });

        // ── Rechtsklick: die Schnellregler DIESER Rauschminderung ────
        //
        // Die Faehigkeit sass bisher auf den NR-Knoepfen im RX-Feld.
        // Weil die auf Ansage vom 2026-08-21 verschwinden ('dies
        // sollte nichts rechts in den widgets stehen'), zieht sie hier
        // mit — sonst waere sie danach unerreichbar, und das ist genau
        // die Art Verlust, die niemandem auffaellt, bis er das erste
        // Mal an einem Regler drehen will.
        //
        // Nicht die Einstellungsseite: auf der Flagge oeffnete der
        // Rechtsklick die drei bis fuenf Regler dieser Minderung, und
        // die Setup-Seite ist darin nur der Verweis ganz unten.
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(b, &QWidget::customContextMenuRequested, this,
                [this, b](const QPoint& pos) {
            if (!m_slice) { return; }
            const NrSlot slot =
                static_cast<NrSlot>(b->property("nrSlot").toInt());
            DspQuickPopup::showFor(this, m_slice, slot,
                                   b->mapToGlobal(pos),
                                   [this, slot]() {
                                       emit openNrSetupRequested(slot);
                                   },
                                   m_maschinenNr ? m_maschinenNr(slot)
                                                 : QString());
        });
    }

    // Das „…" liest die Liste beim KLICK, nicht beim Bau: die Flex-
    // Fassung ändert sie zur Laufzeit (setNrFlexModus). Sonst gleicher
    // Aufbau wie addOverflow().
    auto* more = new QPushButton(QStringLiteral("…"), this);
    more->setStyleSheet(pillStyle());
    more->setCursor(Qt::PointingHandCursor);
    more->setToolTip(tr("Show all %1").arg(m_allNr.size()));
    g.overflow = more;
    if (g.row) { g.row->addWidget(more); }
    connect(more, &QPushButton::clicked, this, [this]() {
        ScopedChildWidget<QMenu> menuOwner(window());   // am Fenster, siehe addOverflow()
        QMenu& m = *menuOwner.get();
        for (const auto& e : nrListe()) {
            QAction* a = m.addAction(e.first);
            connect(a, &QAction::triggered, this,
                    [this, slot = e.second]() { pushNrToModel(slot); });
        }
        m.exec(QCursor::pos());
    });
}

// ── Die Flex-Fassung der Rauschminderung (2026-11-20) ────────────────
//
// Betreiber am 2026-11-20: „bei flexnutzung sollte natürlich auch nur
// das angezeigt werden, was wir können. hier heissen diese glaube ich
// auch anders." Die Pillen stammen aus der Anvelina-Fassung; an der
// Flex stehen Verfahren darin, die wir dort nicht ausführen (DFNR und
// BNR rechnen auf I/Q und sind am Flex-Ton nicht hörbar, NNR ebenso),
// und die Flex nennt ihre Verfahren anders. Die Regeln selbst stehen
// bei RadioModel::flexNrAngeboten / flexNrAnzeige; hier nur die Leiste.
void CommandBar::setNrFlexRegeln(const std::function<bool(NrSlot)>& angeboten,
                                 const std::function<QString(NrSlot)>& anzeige)
{
    m_nrFlexAngeboten = angeboten;
    m_nrFlexAnzeige   = anzeige;
    if (m_nrFlex) { aktualisiereNrPillen(); }
}

void CommandBar::setNrFlexModus(bool flex)
{
    if (m_nrFlex == flex) { return; }
    m_nrFlex = flex;
    aktualisiereNrPillen();
}

QVector<QPair<QString, NrSlot>> CommandBar::nrListe() const
{
    if (!m_nrFlex) { return m_allNr; }
    QVector<QPair<QString, NrSlot>> out;
    for (const auto& e : m_allNr) {
        if (m_nrFlexAngeboten && !m_nrFlexAngeboten(e.second)) { continue; }
        const QString flexName = m_nrFlexAnzeige ? m_nrFlexAnzeige(e.second)
                                                 : QString();
        out.append({flexName.isEmpty() ? e.first : flexName, e.second});
    }
    return out;
}

// Die drei Pillen vorne bekommen die ersten drei der Liste. Die Liste
// hat auch an der Flex immer mindestens drei Einträge (NR1, NR2, NR3
// bleiben dort), daher braucht es kein Ausblenden.
void CommandBar::aktualisiereNrPillen()
{
    Group* g = group(QStringLiteral("NR"));
    if (!g) { return; }
    const auto liste = nrListe();
    for (int i = 0; i < g->pills.size() && i < liste.size(); ++i) {
        g->pills.at(i)->setText(liste.at(i).first);
        g->pills.at(i)->setProperty("nrSlot", static_cast<int>(liste.at(i).second));
    }
    if (g->overflow) {
        g->overflow->setToolTip(tr("Show all %1").arg(liste.size()));
    }
    // Ein Verfahren, das gerade läuft und hier nicht mehr angeboten
    // wird, bleibt in der Maschine; die Leiste zeigt es nicht. Der
    // Stand wird unten aus dem Modell nachgezogen.
    pullFromModel();
}

// ── RATE: was der Empfangskanal wirklich faehrt ──────────────────────
//
// Betreiber am 2026-09-24, nachdem der QRP-Kanal drei Tage lang auf
// 192 kHz lief, waehrend 48 kHz ankamen: "waere es nicht sinnvoll, diese
// rate oben in der taskleiste einzubauen? ... neben NR usw." Angezeigt
// wird die Rate des Kanals, nicht die eingestellte -- der Fehler war
// genau der Unterschied zwischen beiden.
void CommandBar::buildRateGroup(QHBoxLayout* row)
{
    Group& g = addGroup(QStringLiteral("Rate"), row);
    m_rateLabel = new QLabel(QStringLiteral("—"), this);
    m_rateLabel->setAlignment(Qt::AlignCenter);
    m_rateLabel->setToolTip(tr("Not connected"));
    if (g.row) { g.row->addWidget(m_rateLabel); }
    applyRateStyle(false);
}

void CommandBar::applyRateStyle(bool warn)
{
    if (!m_rateLabel) { return; }
    const QString top = QString::fromLatin1(warn ? Style::kAmberBg : Style::kGlassBtnTop);
    const QString bot = QString::fromLatin1(warn ? Style::kAmberBg : Style::kGlassBtnBot);
    const QString border = QString::fromLatin1(warn ? Style::kAmberBorder : Style::kBorder);
    const QString text = QString::fromLatin1(warn ? Style::kAmberText : Style::kTextSecondary);
    m_rateLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "               stop:0 %1, stop:1 %2);"
        "  border: 1px solid %3; border-radius: %5px;"
        "  color: %4; font-size: 11px; font-weight: 600;"
        "  padding: 0 11px; min-height: %6px; max-height: %6px;"
        "}")
        .arg(top, bot, border, text)
        .arg(kPillRadius)
        .arg(kPillHeight));
}

namespace {
QString rateText(int hz)
{
    if (hz % 1000 == 0) {
        return QStringLiteral("%1 kHz").arg(hz / 1000);
    }
    return QStringLiteral("%1 kHz").arg(hz / 1000.0, 0, 'f', 1);
}
} // namespace

void CommandBar::setRateReadout(int channelRateHz, int measuredRateHz)
{
    if (!m_rateLabel) { return; }
    if (channelRateHz <= 0) {
        m_rateLabel->setText(QStringLiteral("—"));
        m_rateLabel->setToolTip(tr("Not connected"));
        m_rateMismatchSeconds = 0;
        m_rateWarn = false;
        applyRateStyle(false);
        return;
    }
    m_rateLabel->setText(rateText(channelRateHz));

    // Nur eine gemessene, deutlich abweichende Rate zaehlt, und erst nach
    // zwei Sekunden in Folge: die erste Sekunde nach dem Verbinden ist
    // angebrochen, und UDP kommt gebuendelt.
    const bool mismatch = measuredRateHz > 0
        && std::abs(measuredRateHz - channelRateHz) > channelRateHz / 10;
    m_rateMismatchSeconds = mismatch ? m_rateMismatchSeconds + 1 : 0;
    const bool warn = m_rateMismatchSeconds >= 2;

    if (warn) {
        m_rateLabel->setToolTip(
            tr("The receive channel runs at %1 but %2 is arriving — that is "
                "why it sounds wrong.")
                .arg(rateText(channelRateHz), rateText(measuredRateHz)));
    } else if (measuredRateHz > 0) {
        m_rateLabel->setToolTip(tr("Empfangskanal %1 · gemessen %2")
                                    .arg(rateText(channelRateHz),
                                         rateText(measuredRateHz)));
    } else {
        m_rateLabel->setToolTip(tr("Empfangskanal %1").arg(rateText(channelRateHz)));
    }
    if (warn != m_rateWarn) {
        m_rateWarn = warn;
        applyRateStyle(warn);
    }
    zieheLeereGruppenZurueck();
}

QString CommandBar::rateReadoutText() const
{
    return m_rateLabel ? m_rateLabel->text() : QString();
}

void CommandBar::pushNrToModel(NrSlot slot)
{
    if (!m_slice) { return; }
    // Ein zweiter Klick auf die laufende schaltet ab — der einzige Weg
    // zu „keine", ohne einen achten Knopf „AUS".
    m_slice->setActiveNr(m_slice->activeNr() == slot ? NrSlot::Off : slot);
    pullFromModel();
}

void CommandBar::pushModeToModel(DSPMode m)
{
    // ── Ein Druck auf DIGU/DIGL ist ein Druck auf „Digimodus" ────────
    //
    // Betreiber 2026-10-28: „sobald ich auf digi klicke muss das digi
    // fenster automatisch aufgehen und die frequenz dort auch
    // hinspringen".
    //
    // Bis dahin tat dieser Knopf nur eines: die Betriebsart stellen.
    // Wer am Geraet auf DIGU drueckte, stand danach in DIGU -- auf der
    // Frequenz, auf der er vorher war, ohne Dekoderfenster. Die ganze
    // Herrichtung (Anrufstelle, Filter, Fenster nach vorne) hing allein
    // an den Pillen FT8/FT4 daneben, und das ist ein Unterschied, den
    // man dem Knopf nicht ansieht.
    //
    // Das Signal geht HINAUS, statt hier zu handeln: welche Anrufstelle
    // und welches Fenster, weiss das Hauptfenster
    // (MainWindow::aufDigiStellen) -- die Leiste kennt weder Baender
    // noch Applets.
    const bool digimodus = (m == DSPMode::DIGU || m == DSPMode::DIGL);

    if (m_slice) { m_slice->setDspMode(m); }
    if (digimodus) { emit digimodusGewaehlt(); }
    // Nicht selbst umschalten: das Modell meldet die Änderung zurück,
    // und wenn es sie ablehnt, soll die Leiste den alten Zustand
    // zeigen und nicht den gewünschten. Ein Knopf, der immer angeht,
    // egal was darunter passiert, ist eine Lüge mit Rückmeldung.
    pullFromModel();
}

void CommandBar::pushStepToModel(int hz)
{
    if (m_slice) { m_slice->setStepHz(hz); }
    pullFromModel();
}

void CommandBar::pushBandToModel(Band band)
{
    // Anders als Mode/Step: der eigentliche Bandwechsel (Sperr-Check,
    // Speichern/Wiederherstellen der Slice-Einstellungen) lebt in
    // RadioModel::onBandButtonClicked, nicht hier -- die Leiste kennt
    // die Regeln dafuer nicht und soll sie nicht duplizieren. Nur
    // durchreichen. bandRequested->onBandButtonClicked ist eine
    // Direktverbindung (selber Thread), das Modell steht also schon,
    // wenn pullFromModel() unten laeuft.
    emit bandRequested(band);
    pullFromModel();
}

void CommandBar::pullFromModel()
{
    if (!m_slice) {
        // Ohne Modell darf keine Pille etwas behaupten. Vorher liess diese
        // Wache den Checked-Zustand von VOR dem Verschwinden der Scheibe
        // stehen -- und ein Klick auf eine ANDERE Pille in derselben
        // Gruppe zeigte danach zwei gleichzeitig an: Qt schaltet den
        // eigenen Haken jedes checkable QPushButton beim Klick IMMER um
        // (kein QButtonGroup-Exklusivitaet hier), unabhaengig davon, ob
        // der verbundene Slot (pushModeToModel & Co., auch ueber diese
        // Funktion) mangels Modell etwas bewirkt.
        //
        // Live gegen ein angeschlossenes ANAN 10E gefunden (2026-09-07):
        // LSB und DIGL gleichzeitig als aktiv angezeigt, waehrend
        // SliceModel::dspMode() der echten, aktiven Scheibe durchgehend
        // DIGL blieb -- die Leiste hing an einer Scheibe, die verschwunden
        // war, ohne dass ein erneutes attach() das je bemerkt hatte.
        for (Group* g : m_groups) { setActive(*g, QString()); }
        return;
    }

    // ── Das eingeschaltete Band leuchtet ────────────────────────────
    //
    // Die Hausregel (tst_command_bar): in jeder Gruppe ist GENAU EINE
    // Pille an. Ohne das haette die Bandgruppe drei dunkle Knoepfe —
    // und die Leiste sagte nicht, wo man steht.
    //
    // Das Band kommt aus der FREQUENZ, nicht aus einem eigenen Feld:
    // so stimmt die Anzeige auch nach einem Klick in den Panadapter,
    // nach CAT oder nach einem Speicheraufruf.
    if (Group* g = group(QStringLiteral("Band"))) {
        const Band cur = bandFromFrequency(m_slice->frequency());
        QString label;
        for (const auto& e : m_allBands) {
            if (e.second == cur) { label = e.first; break; }
        }
        if (!label.isEmpty()) {
            bool visible = false;
            for (QPushButton* b : g->pills) {
                if (b->text() == label) { visible = true; break; }
            }
            // Wie bei Mode: ein Band ausserhalb der ersten drei rueckt
            // an die letzte sichtbare Stelle, sonst zeigt die Leiste
            // drei Pillen, von denen keine an ist.
            if (!visible && !g->pills.isEmpty()) {
                g->pills.last()->setText(label);
            }
            setActive(*g, label);
        }
    }

    // ── Digital leuchtet, wenn es WIRKLICH laeuft ───────────────────
    //
    // Anders als Band oder Mode hat diese Gruppe einen Aus-Zustand:
    // wer SSB faehrt, macht weder FT8 noch FT4, und dann darf hier
    // nichts leuchten. (Dieselbe Begruendung wie bei NR, und
    // tst_command_bar nimmt die Gruppe aus derselben Regel aus.)
    //
    // „Laeuft" heisst nicht „DIGU eingestellt": in DIGU sitzt man auch
    // auf PSK oder Olivia. Es heisst Datenmodus UND Skala auf der
    // Anrufstelle -- genau die zwei Dinge, die der Griff herstellt.
    aktualisiereDigiLampe();

    if (Group* g = group(QStringLiteral("Mode"))) {
        const DSPMode cur = m_slice->dspMode();
        QString label;
        for (const auto& e : m_allModes) {
            if (e.second == cur) { label = e.first; break; }
        }
        // Steht der aktuelle Modus nicht vorne, rückt er an die letzte
        // sichtbare Stelle. Sonst zeigt die Leiste drei Pillen, von
        // denen keine an ist, und der eingestellte Modus steht nirgends.
        if (!label.isEmpty()) {
            bool visible = false;
            for (QPushButton* b : g->pills) {
                if (b->text() == label) { visible = true; break; }
            }
            if (!visible && !g->pills.isEmpty()) {
                g->pills.last()->setText(label);
            }
            setActive(*g, label);
        }
    }

    if (Group* g = group(QStringLiteral("Step"))) {
        const int cur = m_slice->stepHz();
        QString label;
        for (const auto& e : m_allSteps) {
            if (e.second == cur) { label = e.first; break; }
        }
        if (!label.isEmpty()) {
            bool visible = false;
            for (QPushButton* b : g->pills) {
                if (b->text() == label) { visible = true; break; }
            }
            if (!visible && !g->pills.isEmpty()) {
                g->pills.last()->setText(label);
            }
            setActive(*g, label);
        }
    }

    // ── Filter ───────────────────────────────────────────────────────
    //
    // Erst neu beschriften (der Modus kann sich geaendert haben), dann
    // die passende Pille markieren. Umgekehrt waere die Markierung
    // einen Durchlauf alt.
    relabelFilterPills();
    if (Group* g = group(QStringLiteral("Filter"))) {
        const int lo = m_slice->filterLow();
        const int hi = m_slice->filterHigh();
        const QString label = runningFilterLabel(lo, hi);

        // Dieselbe Regel wie bei Modus und Schrittweite: was laeuft,
        // rueckt an die letzte sichtbare Stelle, wenn es nicht schon
        // vorne steht. Sonst zeigt die Leiste drei Pillen, von denen
        // keine an ist, und die eingestellte Breite steht nirgends.
        bool visible = false;
        for (QPushButton* b : g->pills) {
            if (b->text() == label) { visible = true; break; }
        }
        if (!visible && !g->pills.isEmpty()) {
            QPushButton* last = g->pills.last();
            last->setText(label);
            // Siehe relabelFilterPills(): dieselbe Pille kann von "—"
            // oder einer schmaleren Breite hierher gelangt sein; ohne
            // Neuberechnung bleibt sie auf der alten Mindestbreite
            // haengen und die neue, breitere Beschriftung wird an
            // beiden Raendern abgeschnitten.
            setzeMindestbreite(last, label);
            last->setProperty("loHz", lo);
            last->setProperty("hiHz", hi);
            last->setEnabled(true);
            last->setToolTip(QStringLiteral("%1 Hz bis %2 Hz").arg(lo).arg(hi));
        }
        setActive(*g, label);
    }

    // ── Rauschminderung ──────────────────────────────────────────────
    //
    // Wieder dieselbe Regel. Der erste Entwurf hatte hier eine eigene
    // erfunden — die Marke am „…" statt Nachruecken — und der
    // bestehende Test exactlyOnePillIsLitPerGroup hat sie sofort
    // verworfen. Zu Recht: zwei Regeln fuer dieselbe Leiste sind eine
    // zu viel.
    if (Group* g = group(QStringLiteral("NR"))) {
        const NrSlot cur = m_slice->activeNr();
        QString label;
        const auto liste = nrListe();
        for (const auto& e : liste) {
            if (e.second == cur) { label = e.first; break; }
        }
        if (!label.isEmpty()) {
            bool visible = false;
            for (QPushButton* b : g->pills) {
                if (b->text() == label) { visible = true; break; }
            }
            if (!visible && !g->pills.isEmpty()) {
                QPushButton* last = g->pills.last();
                last->setText(label);
                last->setProperty("nrSlot", static_cast<int>(cur));
            }
        }
        // Leeres Etikett heisst „keine laeuft" — dann darf auch keine
        // leuchten.
        setActive(*g, label);
    }
    zieheLeereGruppenZurueck();
}

CommandBar::~CommandBar() { qDeleteAll(m_groups); }

CommandBar::Group* CommandBar::group(const QString& name)
{
    for (Group* g : m_groups) {
        if (g->name.compare(name, Qt::CaseInsensitive) == 0) { return g; }
    }
    return nullptr;
}

const CommandBar::Group* CommandBar::group(const QString& name) const
{
    for (const Group* g : m_groups) {
        if (g->name.compare(name, Qt::CaseInsensitive) == 0) { return g; }
    }
    return nullptr;
}

QStringList CommandBar::groups() const
{
    QStringList out;
    for (const Group* g : m_groups) { out << g->name; }
    return out;
}

QStringList CommandBar::pillsIn(const QString& groupName) const
{
    QStringList out;
    if (const Group* g = group(groupName)) {
        for (QPushButton* b : g->pills) { out << b->text(); }
    }
    return out;
}

QStringList CommandBar::allNrLabels() const
{
    QStringList out;
    for (const auto& e : nrListe()) { out << e.first; }
    return out;
}

QString CommandBar::activePill(const QString& groupName) const
{
    if (const Group* g = group(groupName)) {
        for (QPushButton* b : g->pills) {
            if (b->isChecked()) { return b->text(); }
        }
    }
    return {};
}

bool CommandBar::clickPill(const QString& groupName, const QString& label)
{
    if (Group* g = group(groupName)) {
        for (QPushButton* b : g->pills) {
            if (b->text() == label) { b->click(); return true; }
        }
    }
    return false;
}

} // namespace Longpath
