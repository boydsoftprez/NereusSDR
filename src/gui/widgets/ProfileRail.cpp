// src/gui/widgets/ProfileRail.cpp

// --- From src/gui/widgets/ProfileRail.h ---
// =================================================================
// src/gui/widgets/ProfileRail.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Die Schiene ──────────────────────────────────────────────────────
//
// OE5SOS, 2026-08-15:
//
//   „Wenn dies alles erledigt ist, soll die Möglichkeit entstehen, ein
//    zweites Profil oder auch ein drittes anzulegen, welches ich mir
//    selbst wieder individuell gestalten kann."
//
// In der Vorlage sind das runde Abzeichen am linken Rand — ein Buchstabe je
// Arbeitsfläche, darunter ein gestricheltes Plus. Genau so hier.
//
// ── Warum ein Buchstabe und nicht der Name ───────────────────────────
//
// Die Schiene ist 44 Pixel breit; ein Name passt nicht hinein. Das
// Abzeichen trägt den ersten Buchstaben, der volle Name steht im
// Tooltip und im Rechtsklickmenü. Das ist kein Kompromiss, sondern der
// Punkt: die Schiene soll man mit dem Augenwinkel lesen, nicht
// studieren.
//
// ── Was die Schiene NICHT tut ────────────────────────────────────────
//
// Sie schaltet nicht von selbst um. LayoutProfiles::profileFor() kann
// ein Profil an Band und Modus binden, und die Verdrahtung wäre ein
// Signalanschluss — sie bleibt aus, weil der Betreiber es so
// entschieden hat (2026-08-15): „Nein, nur von Hand. Kein Fenster, das
// sich beim Bandwechsel unter dir verändert."
//
// Die Fenstergröße gehört ebenfalls nicht dazu, aus demselben Grund:
// ein Profilwechsel soll Panels tauschen und nicht das Fenster
// umspringen lassen.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

// --- From src/gui/widgets/ProfileRail.cpp ---
// =================================================================
// src/gui/widgets/ProfileRail.cpp  (Longpath)
// =================================================================
//
// Longpath-original. See ProfileRail.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

// =================================================================
// src/gui/widgets/ProfileRail.cpp  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/gui/widgets/ProfileRail.h
//   src/gui/widgets/ProfileRail.cpp
//   at revision 551576e30c42731c6d51ce005c5f23ec38be391c,
//   https://github.com/oe5sos/Longpath, primary author Martin Fischer
//   (OE5SOS). The upstream banners are copied above.
//
//   Upstream source has no top-of-file copyright or GPL header; the
//   project-level LICENSE (GPLv3) applies.
//
//   Namespace Longpath becomes NereusSDR. Style:: resolves to the
//   Longpath values in WorkspacePalette and WorkspaceStyle through
//   gui/instruments/InstrumentStyle.h, never to NereusSDR::Style in
//   gui/StyleConstants.h. CommandBar::pillStyle(), kPillHeight and
//   kPillRadius come from gui/widgets/PillStyle.h. Differences from
//   upstream:
//   - WorkspaceProfiles replaces LayoutProfiles; badges are keyed by
//     profile id and labelled from the profile name.
//   - User-visible strings (tooltips, menu items) are in English; the
//     German comments are kept verbatim.
//   - The context menu gains "Manage profiles..." (manageRequested), and
//     the "move the rail" item can be left out (setPlacementToggleOffered).
//   - The menu does not use ScopedChildWidget (src/gui/ScopedChildWidget.h,
//     ported later): it is a child of the main window held by QPointer
//     and deleted later, which keeps the same guarantee (see showMenuFor).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08 - Ported in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//                 Rewired to WorkspaceProfiles (ids, not names).
//   2026-10-08 - Horizontal rail: the hover remove cross no longer sits
//                 on the badge letter (shared Longpath defect, fixed in
//                 our copy; see kCloserReservePx). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Upstream's unmatched ProfileRail selectors restored, so
//                 the rail paints no fill or border of its own, as in
//                 Longpath.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The remove cross keeps its 14x14 size in the top bar
//                 (shared Longpath defect, fixed in our copy; see
//                 kCloserSidePx). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-22 - Comments no longer say ScopedChildWidget is absent.
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Claude Code.
// =================================================================

#include "gui/widgets/ProfileRail.h"

#include "gui/containers/WorkspaceProfiles.h"
#include "gui/instruments/InstrumentStyle.h"
#include "gui/widgets/PillStyle.h"

#include <QEvent>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QBoxLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <algorithm>

namespace NereusSDR {

// NereusSDR: the Longpath values, see gui/instruments/InstrumentStyle.h.
namespace Style = ::NereusSDR::InstrumentStyle;

namespace {

// NereusSDR: room on the right of a horizontal badge for the hover
// remove cross (14 px wide, 1 px from the edge, plus a 1 px gap), so the
// cross sits beside the letter, not on it. Upstream sizes the badge to
// the letter alone (horizontalAdvance + 20, about 29 px for one letter)
// and puts the cross at sizeHint().width() - 15, which covers the right
// half of the centred letter ("D" reads "D×"). Longpath has the same
// defect at 551576e3; ours differs to fix it. The vertical rail keeps
// its square badges and the upstream placement.
constexpr int kCloserReservePx = 16;

// The remove cross's side: a 14x14 disc (radius 7). From Longpath
// src/gui/widgets/ProfileRail.cpp:288 [@551576e3] (setFixedSize(14, 14)).
// NereusSDR: the cross also states it as min-/max-height in its style
// sheet. In the top bar its badge carries the pill style, whose
// `min-height: 27px; max-height: 27px` cascades to every child button
// that is silent about its height, and a style sheet height beats
// setFixedSize: the cross drew 14x27, the badge's full height. Longpath
// has the same defect at 551576e3; ours differs to fix it.
constexpr int kCloserSidePx = 14;

// Die Schiene ist so hoch wie eine Pille AUSSEN: Inhaltshoehe plus die
// beiden Raender. Siehe die Begruendung an kLeistenSeite.
// From Longpath src/gui/widgets/ProfileRail.cpp:31-34 [@551576e3]
static_assert(ProfileRail::kLeistenSeite == PillStyle::kPillHeight + 2,
              "Die Schiene muss so hoch sein wie eine Pille mit ihren Raendern.");

// From Longpath src/gui/widgets/ProfileRail.cpp:36-63 [@551576e3]
QString badgeStyle(bool active)
{
    // Das aktive Abzeichen ist gefüllt, die anderen sind Umrisse. Ein
    // Rand allein reicht nicht: bei fünf Profilen sucht man sonst, und
    // die Schiene soll man mit dem Augenwinkel lesen.
    const QString bg     = active ? QString::fromLatin1(Style::kBlueBg)
                                  : QString::fromLatin1(Style::kButtonBg);
    const QString border = active ? QString::fromLatin1(Style::kBlueBorder)
                                  : QString::fromLatin1(Style::kBorder);
    const QString text   = active ? QString::fromLatin1(Style::kBlueText)
                                  : QString::fromLatin1(Style::kTextSecondary);
    // `padding: 0` ist kein Schmuck. Style::kButtonStyle gilt app-weit
    // fuer jeden Knopf ohne eigene Angabe und bringt `padding: 4px 12px`
    // mit: von 30 Pixeln Kantenlaenge blieben damit 4 fuer den
    // Buchstaben, und aus dem „N" wurde ein Schraegstrich. Am
    // 2026-10-07 am Schirm gefunden -- die Schiene am linken Rand zeigte
    // das seit dem app-weiten Blatt vom 2026-09-18, also drei Wochen
    // lang. Dieselbe Falle wie bei den Zoomknoepfen am 2026-09-25
    // (tst_zoom_buttons_legible).
    return QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2; color: %3;"
        "  border-radius: %4px; font-size: 13px; font-weight: bold;"
        "  padding: 0; }"
        "QPushButton:hover { border: 1px solid %5; }")
        .arg(bg, border, text)
        .arg(ProfileRail::kBadgeSide / 2)
        .arg(QString::fromLatin1(Style::kBlueBorder));
}

// ── Zwei Formen fuer zwei Plaetze (2026-10-07) ──────────────────────
//
// Am linken Rand steht die Schiene fuer sich: dort ist die runde
// Scheibe richtig, sie gehoert zu nichts anderem und darf anders
// aussehen als der Fensterinhalt.
//
// In der Leiste steht sie in EINER Reihe mit BAND, MODE, FILTER,
// STEP, NR und RATE. Martin am 2026-10-07 zu genau diesem Bild: „die
// leiste oben passt aber noch immer nicht". Am Bildschirm nachgesehen:
// drei Kreise von 30 px zwischen lauter abgerundeten Rechtecken von
// 27 px, dazu Schrift 13 fett gegen 11/600 und eine flache blaue
// Fuellung gegen den Auswahlverlauf der Pillen. Vier Unterschiede in
// einer Reihe -- das liest sich als eingeklebt, nicht als Teil.
//
// Darum nimmt die Schiene in der Leiste KEINEN eigenen Stil mehr,
// sondern den der Pillen (CommandBar::pillStyle). Nicht nachgebaut,
// sondern derselbe Aufruf: ein nachgebauter Verlauf laeuft beim
// naechsten Stilblatt auseinander, und das faellt niemandem auf.
// From Longpath src/gui/widgets/ProfileRail.cpp:83-86 [@551576e3]
QString leistenStil()
{
    return PillStyle::pillStyle();
}

} // namespace

// From Longpath src/gui/widgets/ProfileRail.cpp:90-99 [@551576e3]
QString ProfileRail::initialFor(const QString& name)
{
    // Der erste Buchstabe, nicht das erste Zeichen. Ein Profil, das
    // „ CW" oder „(alt) SSB" heißt, bekäme sonst ein Leerzeichen oder
    // eine Klammer als Abzeichen.
    for (const QChar c : name) {
        if (c.isLetterOrNumber()) { return QString(c.toUpper()); }
    }
    return QStringLiteral("?");
}

// From Longpath src/gui/widgets/ProfileRail.cpp:101-188 [@551576e3]
ProfileRail::ProfileRail(WorkspaceProfiles* profiles,
                         Qt::Orientation richtung, QWidget* parent)
    : QWidget(parent), m_profiles(profiles), m_richtung(richtung)
{
    setAttribute(Qt::WA_StyledBackground, true);

    if (m_richtung == Qt::Vertical) {
        setFixedWidth(kWidth);
        // Die Trennlinie steht rechts, zum Fensterinhalt hin.
        // NereusSDR: upstream's selector is kept as written. A type
        // selector matches the class name with its namespace
        // ("NereusSDR--ProfileRail", Longpath--ProfileRail upstream), so
        // bare "ProfileRail" matches nothing and this rule paints nothing,
        // as in Longpath. Matching it would add a panel fill and a right
        // border that Longpath never shows.
        setStyleSheet(QStringLiteral(
            "ProfileRail { background: %1; border-right: 1px solid %2; }")
            .arg(QString::fromLatin1(Style::kPanelBg),
                 QString::fromLatin1(Style::kBorderSubtle)));
        m_column = new QVBoxLayout(this);
        m_column->setContentsMargins(7, 9, 7, 9);
    } else {
        // In der Leiste: keine eigene Flaeche und keine Trennlinie. Die
        // Abzeichen sollen wie die Pillen daneben auf dem Leistengrund
        // sitzen, sonst liegt ein Kasten im Kasten.
        setFixedHeight(kLeistenSeite);
        // NereusSDR: upstream's unmatched selector, kept as written (see
        // above).
        setStyleSheet(QStringLiteral("ProfileRail { background: transparent; }"));
        m_column = new QHBoxLayout(this);
        m_column->setContentsMargins(0, 0, 0, 0);
    }
    m_column->setSpacing(m_richtung == Qt::Vertical ? 7 : 5);

    m_plus = new QPushButton(QStringLiteral("+"), this);
    m_plus->setObjectName(QStringLiteral("profileRailNew"));
    setzeMasse(m_plus);
    m_plus->setCursor(Qt::PointingHandCursor);
    // NereusSDR: the upstream tooltip, in English. New offers Empty or
    // Current (the unified workspace design).
    m_plus->setToolTip(QStringLiteral(
        "New profile: empty, or from the current arrangement.\n\n"
        "The current profile keeps its own arrangement."));
    // Gestrichelt bleibt es an beiden Plaetzen -- das ist die Zusage
    // „hier kommt noch etwas hin" und gilt unabhaengig von der Form.
    // Nur die Rundung folgt dem Platz: Scheibe am Rand, Pillenradius
    // in der Leiste.
    //
    // ── Warum hier `padding: 0 11px` stehen MUSS ────────────────────
    //
    // Am 2026-10-07 am Schirm gemessen: das Plus war 37 Pixel hoch, die
    // Pillen daneben 29. Acht Pixel zu viel, und genau acht stehen in
    // Style::kButtonStyle: `padding: 4px 12px`. Das Blatt gilt fuer
    // jeden Knopf im Fenster, auch fuer einen mit eigenem Stilblatt,
    // solange dieses zum Innenabstand schweigt -- pillStyle() setzt
    // `padding: 0 11px` und ist deshalb 29; hier fehlte es.
    //
    // Die Hoehe steht ausdruecklich dabei, weil dieses Plus NICHT
    // pillStyle() traegt (es ist gestrichelt) und sie sonst niemand
    // liefert.
    m_plus->setStyleSheet(
        m_richtung == Qt::Vertical
            ? QStringLiteral(
                  "QPushButton { background: transparent; color: %1;"
                  "  border: 1px dashed %2; border-radius: %3px;"
                  "  font-size: 16px; padding: 0; }"
                  "QPushButton:hover { color: %4; border: 1px dashed %4; }")
                  .arg(QString::fromLatin1(Style::kTextScale),
                       QString::fromLatin1(Style::kBorder))
                  .arg(kBadgeSide / 2)
                  .arg(QString::fromLatin1(Style::kBlueBorder))
            : QStringLiteral(
                  "QPushButton { background: transparent; color: %1;"
                  "  border: 1px dashed %2; border-radius: %3px;"
                  // 13, nicht 14: die Schriftleiter kennt
                  // [7, 9, 11, 13, 16, 22, 38, 48], und eine Stufe
                  // daneben waere eine namenlose Groesse mehr
                  // (verify-style-drift). 11 waere die der Pillen --
                  // fuer ein Zeichen statt eines Wortes zu klein.
                  "  font-size: 13px; padding: 0 11px;"
                  "  min-height: %5px; max-height: %5px; }"
                  "QPushButton:hover { color: %4; border: 1px dashed %4; }")
                  .arg(QString::fromLatin1(Style::kTextScale),
                       QString::fromLatin1(Style::kBorder))
                  .arg(PillStyle::kPillRadius)
                  .arg(QString::fromLatin1(Style::kBlueBorder))
                  .arg(PillStyle::kPillHeight));
    connect(m_plus, &QPushButton::clicked,
            this, &ProfileRail::newProfileRequested);

    rebuild();

    if (m_profiles) {
        connect(m_profiles, &WorkspaceProfiles::profilesChanged,
                this, &ProfileRail::rebuild);
        connect(m_profiles, &WorkspaceProfiles::activeChanged,
                this, [this](const QString&) { rebuild(); });
    }
}

// ── Warum die Breite in der Leiste NICHT festgenagelt wird ──────────
//
// Erster Versuch am 2026-10-07 war setFixedSize(27, 27) -- quadratisch
// wie vorher die Scheibe, nur eckig. Am Bildschirm kam dabei aus dem
// „N" ein Schraegstrich, aus dem „B" ein „Ǝ" und aus dem „+" ein
// senkrechter Strich: alle drei an beiden Seiten abgeschnitten.
//
// Grund: Stilblaetter in Qt vererben sich. CommandBar setzt fuer ihre
// Pillen `padding: 0 11px`, und das gilt auch fuer jeden fremden Knopf,
// der in ihr sitzt -- auch fuer einen mit eigenem Stilblatt, solange
// dieses zum Innenabstand nichts sagt. Von 27 Pixeln blieben nach
// 2×11 Abstand und 2×1 Rand genau 3 fuer den Buchstaben.
//
// Die Leiste selbst kennt das Problem seit dem 2026-08-23 („aus ,10 Hz'
// wurde ,I0 Hz'") und loest es ueber die Schriftmetrik. Dieselbe Regel
// hier: die Hoehe steht fest, die Breite ergibt sich aus dem, was der
// Buchstabe braucht. Senkrecht am Rand bleibt es beim Quadrat -- eine
// Scheibe mit zwei verschiedenen Halbmessern waere ein Ei.
// From Longpath src/gui/widgets/ProfileRail.cpp:208-219 [@551576e3]
void ProfileRail::setzeMasse(QPushButton* b) const
{
    if (m_richtung == Qt::Vertical) {
        b->setFixedSize(kBadgeSide, kBadgeSide);
        return;
    }
    // Die Hoehe kommt aus dem Stilblatt der Pillen (min-height =
    // max-height), nicht von hier. Sie hier zu setzen hiesse, dieselbe
    // Zahl ein zweites Mal zu pflegen -- und beim naechsten Stilblatt
    // stuenden die Abzeichen zwei Pixel daneben, ohne dass es auffiele.
    b->setMinimumWidth(b->fontMetrics().horizontalAdvance(b->text()) + 20);
}

// From Longpath src/gui/widgets/ProfileRail.cpp:221-326 [@551576e3]
void ProfileRail::rebuild()
{
    // Abzeichen einsammeln und wegwerfen, das Plus bleibt. Es unten neu
    // anzulegen wäre einfacher und würde bei jedem Profilwechsel den
    // Knopf unter dem Mauszeiger austauschen.
    for (QPushButton* b : m_badges) {
        m_column->removeWidget(b);
        b->deleteLater();
    }
    m_badges.clear();
    m_closers.clear();   // Kinder der Abzeichen, sterben mit ihnen
    m_order.clear();
    m_column->removeWidget(m_plus);
    while (QLayoutItem* it = m_column->takeAt(0)) { delete it; }

    if (!m_profiles) {
        m_column->addWidget(m_plus, 0, mittig());
        m_column->addStretch(1);
        return;
    }

    const QString current = m_profiles->activeId();
    const QStringList ids = m_profiles->ids();
    for (const QString& id : ids) {
        const QString name = m_profiles->name(id);
        auto* b = new QPushButton(initialFor(name), this);
        b->setObjectName(QStringLiteral("profileRailBadge"));
        setzeMasse(b);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(name);
        if (m_richtung == Qt::Vertical) {
            b->setStyleSheet(badgeStyle(id == current));
        } else {
            // In der Leiste traegt das Abzeichen den Pillenstil, und das
            // aktive wird ueber :checked hervorgehoben -- derselbe Weg,
            // den CommandBar fuer BAND und MODE geht.
            //
            // autoExclusive: ohne das nimmt ein Klick auf das BEREITS
            // aktive Abzeichen ihm die Fuellung. LayoutProfiles meldet
            // bei activate() auf den schon laufenden Namen keine
            // Aenderung, also baut sich die Schiene nicht neu auf, und
            // der falsche Zustand bliebe stehen. Mit autoExclusive
            // laesst Qt das letzte gedrueckte Element gedrueckt.
            b->setCheckable(true);
            b->setAutoExclusive(true);
            b->setChecked(id == current);
            b->setStyleSheet(leistenStil());
        }
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(b, &QPushButton::clicked, this, [this, id]() {
            if (m_profiles) { m_profiles->switchTo(id); }
        });
        connect(b, &QWidget::customContextMenuRequested,
                this, [this, b, id](const QPoint& p) {
            showMenuFor(id, b->mapToGlobal(p));
        });
        // ── Das X am Abzeichen (2026-08-19) ─────────────────────────
        //
        // Auf Ansage des Betreibers: „man sollte die runden Buttons
        // (profile) auch mit einem X am Tab loeschen koennen."
        //
        // Loeschen gab es schon per Rechtsklick — aber ein Rechtsklick
        // ist kein Angebot: nichts auf dem Abzeichen sagt, dass es geht.
        // Dasselbe Argument wie beim Mausrad auf der Weltkugel.
        //
        // Es erscheint beim Ueberfahren, nicht dauernd: acht Abzeichen
        // mit acht Kreuzen sehen aus wie eine Warnung, und man loescht
        // ein Profil selten.
        if (ids.size() > 1) {
            auto* x = new QPushButton(QStringLiteral("×"), b);
            x->setFixedSize(kCloserSidePx, kCloserSidePx);
            x->setCursor(Qt::PointingHandCursor);
            // Waagrecht ist die Breite nicht mehr die Kantenlaenge
            // (siehe setzeMasse), darum an der tatsaechlichen Breite
            // ausrichten -- sonst saesse das Kreuz mitten im Buchstaben.
            if (m_richtung == Qt::Vertical) {
                x->move(b->sizeHint().width() - 15, 1);
            } else {
                // NereusSDR: widen the badge by kCloserReservePx and
                // place the cross against its real width (the minimum
                // width can exceed sizeHint), beside the letter. From
                // Longpath src/gui/widgets/ProfileRail.cpp:293 [@551576e3]
                // for the original line above.
                b->setMinimumWidth(b->minimumWidth() + kCloserReservePx);
                x->move(std::max(b->sizeHint().width(), b->minimumWidth()) - 15, 1);
            }
            x->setToolTip(QStringLiteral("Remove profile “%1”").arg(name));
            x->setStyleSheet(QStringLiteral(
                // `padding: 0` auch hier: 14 Pixel Kantenlaenge minus
                // der app-weite Innenabstand von 2x12 waere negativ --
                // das Kreuz haette gar keine Flaeche. Siehe HAUSSTIL.md,
                // „der geerbte Innenabstand".
                "QPushButton { background: %1; color: %2; border: none;"
                "  border-radius: 7px; font-size: 11px; font-weight: bold;"
                "  padding: 0;"
                // NereusSDR: see kCloserSidePx.
                "  min-height: %4px; max-height: %4px; }"
                "QPushButton:hover { background: %3; color: #ffffff; }")
                    .arg(QString::fromLatin1(Style::kButtonBg),
                         QString::fromLatin1(Style::kTextSecondary),
                         QString::fromLatin1(Style::kRedBg))
                    .arg(kCloserSidePx));
            x->hide();
            b->installEventFilter(this);
            m_closers.insert(id, x);

            connect(x, &QPushButton::clicked, this, [this, id]() {
                emit removeRequested(id);
            });
        }

        m_column->addWidget(b, 0, mittig());
        m_badges.insert(id, b);
        m_order << id;
    }

    m_column->addWidget(m_plus, 0, mittig());
    // Senkrecht schiebt die Dehnung die Abzeichen nach oben. Waagrecht
    // darf sie NICHT dazu: die Leiste setzt die Breite, und eine Dehnung
    // darin wuerde die Abzeichen auseinanderziehen.
    if (m_richtung == Qt::Vertical) { m_column->addStretch(1); }
}

// Das X erscheint, solange der Zeiger auf dem Abzeichen steht.
// From Longpath src/gui/widgets/ProfileRail.cpp:329-341 [@551576e3]
bool ProfileRail::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
        const bool show = (event->type() == QEvent::Enter);
        for (auto it = m_badges.cbegin(); it != m_badges.cend(); ++it) {
            if (it.value() != watched) { continue; }
            if (QPushButton* x = m_closers.value(it.key(), nullptr)) {
                x->setVisible(show);
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

// From Longpath src/gui/widgets/ProfileRail.cpp:343-406 [@551576e3]
void ProfileRail::showMenuFor(const QString& id, const QPoint& globalPos)
{
    // Am Hauptfenster, nicht an der Leiste (2026-09-27): die Leiste hat
    // ein eigenes natives Fenster, und ein Menue daran bekam von Qt keinen
    // Fensterbezug ("QWidgetWindow(... ProfileRailClassWindow) must be a
    // top level window." bei jedem Rechtsklick in Martins Protokoll) --
    // auf macOS kann es dann hinter schwebenden Werkzeugfenstern landen.
    // NereusSDR: a QPointer to a menu parented to the window, deleted
    // later, in place of ScopedChildWidget (same guarantee).  [original inline comment from
    // ProfileRail.cpp:350: "stirbt nicht mit dem Rahmen, siehe
    // ScopedChildWidget.h"]
    QPointer<QMenu> menuOwner = new QMenu(window());
    const QPointer<ProfileRail> self(this);
    QMenu& menu = *menuOwner;
    const QString name = m_profiles ? m_profiles->name(id) : QString();
    // Der volle Name als Überschrift. Auf dem Abzeichen steht nur ein
    // Buchstabe, und ein Menü mit „Löschen“ über einem „C“ ist zu wenig
    // Auskunft für etwas, das nicht rückgängig zu machen ist.
    QAction* title = menu.addAction(name);
    title->setEnabled(false);
    menu.addSeparator();

    QAction* save = menu.addAction(QStringLiteral("Save Now"));
    // captureIntoCurrent() sichert immer das AKTIVE Profil, gleich, auf
    // welches Abzeichen man rechtsklickt -- der Menuepunkt darf also nur
    // dort erscheinen, sonst waere "Jetzt sichern" auf einem anderen
    // Abzeichen ein Versprechen, das der Aufruf gar nicht einloest.
    save->setEnabled(m_profiles && m_profiles->activeId() == id);
    QAction* exportToDesktop = menu.addAction(QStringLiteral("Export…"));
    QAction* importFromDesktop = menu.addAction(QStringLiteral("Import…"));
    menu.addSeparator();
    QAction* ren = menu.addAction(QStringLiteral("Rename…"));
    QAction* dup = menu.addAction(QStringLiteral("Duplicate…"));
    menu.addSeparator();
    QAction* del = menu.addAction(QStringLiteral("Remove"));
    // Das letzte Profil lässt sich nicht löschen. Ohne Profil gehört
    // das Fenster keinem, und die nächste Umgestaltung landete nirgends.
    del->setEnabled(m_profiles && m_profiles->ids().size() > 1);
    // NereusSDR: the manage dialog.
    menu.addSeparator();
    QAction* manage = menu.addAction(QStringLiteral("Manage Profiles…"));

    // ── Wohin die Schiene gehoert, entscheidet der Betreiber ─────────
    //
    // 2026-10-06, nachdem sie von links in die Leiste gewandert war:
    // „macht sinn, dass man dies individuell verschieben und anpassen
    // kann". Also nicht neu festgenagelt, sondern hier umschaltbar --
    // an der Stelle, an der man ohnehin rechtsklickt, statt in einer
    // Einstellungsseite, die man erst finden muss.
    // NereusSDR: offered only when the receiver can move the rail.
    QAction* verschieben = nullptr;
    if (m_placementToggleOffered) {
        menu.addSeparator();
        verschieben = menu.addAction(
            m_richtung == Qt::Horizontal
                ? QStringLiteral("Move Rail to the Left Edge")
                : QStringLiteral("Move Rail into the Top Bar"));
    }

    QAction* chosen = menu.exec(globalPos);
    // Das Elternteil kann waehrend exec() gestorben sein — dann ist
    // auch das Menue weg und `this` eine Leiche. Siehe ScopedChildWidget.h.
    // Weil das Menue am Hauptfenster haengt, kann die Leiste auch allein
    // gestorben sein: dafuer `self`.
    // NereusSDR: the menu goes whichever way exec() ended.
    if (menuOwner) { menuOwner->deleteLater(); }
    if (!menuOwner || !self) { return; }
    if (chosen == save)      { emit saveRequested(id); }
    else if (chosen == exportToDesktop) { emit exportRequested(id); }
    else if (chosen == importFromDesktop) { emit importRequested(id); }
    else if (chosen == ren)  { emit renameRequested(id); }
    else if (chosen == dup)  { emit duplicateRequested(id); }
    else if (chosen == del)  { emit removeRequested(id); }
    else if (chosen == manage) { emit manageRequested(); }
    else if (verschieben && chosen == verschieben) {
        emit placementToggleRequested(m_richtung == Qt::Horizontal
                                          ? Qt::Vertical : Qt::Horizontal);
    }
}

// From Longpath src/gui/widgets/ProfileRail.cpp:408-421 [@551576e3]
QStringList ProfileRail::badges() const { return m_order; }

QString ProfileRail::activeBadge() const
{
    return m_profiles ? m_profiles->activeId() : QString();
}

bool ProfileRail::clickBadge(const QString& id)
{
    QPushButton* b = m_badges.value(id, nullptr);
    if (!b) { return false; }
    b->click();
    return true;
}

} // namespace NereusSDR
