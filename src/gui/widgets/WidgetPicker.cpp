// =================================================================
// src/gui/widgets/WidgetPicker.cpp  (Longpath)
// =================================================================
//
// Longpath-original. See WidgetPicker.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-08-15 — Kategorien, Suche, Karten statt flacher Hakenliste.
// =================================================================

#include "gui/widgets/WidgetPicker.h"

#include "gui/SideAreaWindow.h"
#include "gui/StyleConstants.h"
#include "gui/applets/AppletVisibilityController.h"

#include <QApplication>
#include <QCursor>
#include <QDrag>
#include <QMimeData>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QAbstractButton>
#include <QEasingCurve>
#include <QEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QGuiApplication>
#include <QScreen>
#include <QScrollArea>

#include <algorithm>
#include <functional>
#include <QVBoxLayout>

namespace Longpath {
namespace {

/// Eine Karte, die auf einen Klick reagiert. QFrame kann das nicht von
/// sich aus, und ein Knopf mit zwei Textzeilen unterschiedlicher Größe
/// wäre ein Stylesheet-Ringkampf.
class CardFrame : public QWidget {
public:
    using QWidget::QWidget;
    std::function<void()> onClick;
    /// Kennung, die beim Ziehen mitgeht (kSidePageMimeType). Leer: die
    /// Karte laesst sich nicht ziehen.
    QString dragId;

protected:
    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton) {
            m_pressPos = e->position().toPoint();
            m_dragged = false;
        }
        QWidget::mousePressEvent(e);
    }

    // ── Ziehen statt Klicken ────────────────────────────────────────
    //
    // Betreiber 2026-09-25: "widget oeffnen und per drag and drop auf die
    // taskleiste". Wer die Karte ueber die Zieh-Schwelle hinaus bewegt,
    // zieht sie -- auf den Seitenbereich, der sie als Seite aufnimmt. Ein
    // Klick ohne Bewegung bleibt, was er war: ein-/ausschalten.
    void mouseMoveEvent(QMouseEvent* e) override
    {
        if ((e->buttons() & Qt::LeftButton) && !m_dragged && !dragId.isEmpty()
            && isEnabled()
            && (e->position().toPoint() - m_pressPos).manhattanLength()
                   >= QApplication::startDragDistance()) {
            m_dragged = true;
            auto* mime = new QMimeData;
            mime->setData(QLatin1String(kSidePageMimeType), dragId.toUtf8());
            auto* drag = new QDrag(this);
            drag->setMimeData(mime);
            const QPixmap shot = grab();
            drag->setPixmap(shot.scaledToWidth(qMin(shot.width(), 260),
                                               Qt::SmoothTransformation));
            drag->setHotSpot(QPoint(12, 12));
            drag->exec(Qt::CopyAction);
            return;
        }
        QWidget::mouseMoveEvent(e);
    }

    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton && isEnabled() && onClick && !m_dragged) {
            onClick();
        }
        m_dragged = false;
        QWidget::mouseReleaseEvent(e);
    }

private:
    QPoint m_pressPos;
    bool m_dragged{false};
};

} // namespace

QString WidgetPicker::allCategory() { return tr("All"); }

WidgetPicker::WidgetPicker(AppletVisibilityController* vis, QWidget* parent)
    : QWidget(parent), m_vis(vis), m_category(allCategory())
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(
        "WidgetPicker { background: %1; border: 1px solid %2;"
        "               border-radius: 6px; }")
        .arg(QString::fromLatin1(Style::kPanelBg),
             QString::fromLatin1(Style::kBorder)));
    // ── Wie groß ─────────────────────────────────────────────────────
    //
    // Die Obergrenze ist der Punkt. Ohne sie wächst adjustSize() auf
    // die Höhe ALLER Karten — bei dreizehn Widgets war das Fenster
    // 1100 Pixel hoch und verdeckte das halbe Programm, obwohl der
    // Rollbereich darunter genau dafür da ist. Ein Auswähler soll neben
    // dem stehen, worüber man entscheidet, nicht davor.
    setMinimumSize(620, 380);
    setMaximumHeight(560);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Hier stand eine Überschrift „WIDGET HINZUFÜGEN". Sie stammt aus
    // der Zeit, als das Ding ein rahmenloser Aufklapper war. Jetzt ist
    // es ein Fenster mit Titelleiste, und die sagt dasselbe — zweimal
    // derselbe Satz übereinander ist keine Gestaltung, sondern ein
    // vergessener Umbau.
    outer->addSpacing(10);

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    outer->addLayout(body, 1);

    // ── Kategorien links ─────────────────────────────────────────────
    m_categories = new QListWidget(this);
    m_categories->setFixedWidth(178);
    m_categories->setFrameShape(QFrame::NoFrame);
    m_categories->setStyleSheet(QStringLiteral(
        "QListWidget { background: transparent; border: none;"
        "  outline: none; padding: 4px 12px; }"
        "QListWidget::item { color: %1; padding: 9px 12px;"
        "  border-radius: 6px; }"
        "QListWidget::item:selected { background: %2; color: %3;"
        "  border: 1px solid %4; }")   // Mulde statt Blau, siehe refresh()
        .arg(QString::fromLatin1(Style::kTextSecondary),
             QString::fromLatin1(Style::kGlassSelTop),
             QString::fromLatin1(Style::kGlassSelText),
             QString::fromLatin1(Style::kGlassSelBorder)));
    body->addWidget(m_categories);
    connect(m_categories, &QListWidget::currentTextChanged,
            this, [this](const QString& t) { setCategory(t); });

    // ── Suche und Karten rechts ──────────────────────────────────────
    auto* right = new QVBoxLayout;
    right->setContentsMargins(6, 4, 16, 16);
    right->setSpacing(10);
    body->addLayout(right, 1);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search widgets…"));
    m_search->setStyleSheet(QStringLiteral(
        "QLineEdit { background: %1; border: 1px solid %2;"
        "  border-radius: 6px; color: %3; padding: 7px 11px;"
        "  font-size: 13px; }")
        .arg(QString::fromLatin1(Style::kInsetBg),
             QString::fromLatin1(Style::kBorder),
             QString::fromLatin1(Style::kTextPrimary)));
    right->addWidget(m_search);
    connect(m_search, &QLineEdit::textChanged,
            this, [this](const QString& t) { setSearch(t); });

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* holder = new QWidget(scroll);
    m_cards = new QVBoxLayout(holder);
    m_cards->setContentsMargins(0, 0, 0, 0);
    m_cards->setSpacing(8);
    m_cards->addStretch(1);
    scroll->setWidget(holder);
    right->addWidget(scroll, 1);

    rebuild();

    if (m_vis) {
        connect(m_vis, &AppletVisibilityController::availabilityChanged,
                this, [this](const QString&, bool) { refresh(); });
        connect(m_vis, &AppletVisibilityController::visibilityChanged,
                this, [this](const QString&, bool) { refresh(); });
    }
}

void WidgetPicker::rebuild()
{
    m_byId.clear();
    m_order.clear();
    while (m_cards->count() > 1) {                 // die Dehnung bleibt
        QLayoutItem* it = m_cards->takeAt(0);
        if (QWidget* w = it->widget()) { w->deleteLater(); }
        delete it;
    }
    m_categories->clear();
    if (!m_vis) { return; }

    m_categories->addItem(allCategory());
    for (const QString& c : m_vis->categories()) { m_categories->addItem(c); }
    for (const ZusatzEintrag& z : m_zusatz) {
        if (m_categories->findItems(z.kategorie, Qt::MatchExactly).isEmpty()) {
            m_categories->addItem(z.kategorie);
        }
    }
    m_categories->setCurrentRow(0);

    for (const QString& id : m_vis->registeredIds()) {
        const bool avail = m_vis->isAvailable(id);
        // Hinter einer Betriebsart: bedienbar, der Klick schaltet sie ein.
        const bool bedienbar = avail || m_einschalter.contains(id);

        auto* frame = new CardFrame(this);
        frame->setAttribute(Qt::WA_StyledBackground, true);
        frame->setCursor(bedienbar ? Qt::PointingHandCursor : Qt::ArrowCursor);
        frame->setEnabled(bedienbar);

        auto* col = new QVBoxLayout(frame);
        col->setContentsMargins(15, 11, 15, 12);
        col->setSpacing(4);

        auto* titleRow = new QHBoxLayout;
        titleRow->setContentsMargins(0, 0, 0, 0);
        titleRow->setSpacing(9);
        auto* name = new QLabel(m_vis->displayName(id), frame);
        {
            QFont f = name->font();
            f.setPixelSize(13);
            f.setBold(true);
            name->setFont(f);
        }
        titleRow->addWidget(name);
        // Die Vorlage schreibt DISABLED neben den Namen und den Grund
        // darunter. Sagen, wo man es einschaltet, ist der Unterschied
        // zwischen einer Auskunft und einem grauen Feld. Immer gebaut,
        // in refresh() gezeigt oder versteckt: mit einem Einschalter
        // aendert sich die Verfuegbarkeit, waehrend die Karte steht.
        auto* badge = new QLabel(tr("OFF"), frame);
        {
            QFont bf = badge->font();
            bf.setPixelSize(9);
            bf.setBold(true);
            bf.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
            badge->setFont(bf);
        }
        badge->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
            .arg(QString::fromLatin1(Style::kAmberText)));
        badge->setVisible(!avail);
        titleRow->addWidget(badge);
        titleRow->addStretch(1);
        col->addLayout(titleRow);

        auto* keys = new QLabel(unterzeileFuer(id, avail), frame);
        {
            QFont f = keys->font();
            f.setPixelSize(11);
            keys->setFont(f);
        }
        keys->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
            .arg(QString::fromLatin1(Style::kTextScale)));
        col->addWidget(keys);

        auto click = [this, id]() {
            if (!m_vis) { return; }
            if (!m_vis->isAvailable(id)) {
                const auto it = m_einschalter.constFind(id);
                if (it == m_einschalter.cend() || !it->schalte) { return; }
                it->schalte();
                if (m_vis->isAvailable(id) && !m_vis->isVisible(id)) {
                    m_vis->setVisible(id, true);
                }
                refresh();
                emit toggled(id, m_vis->isVisible(id));
                return;
            }
            const bool next = !m_vis->isVisible(id);
            m_vis->setVisible(id, next);
            emit toggled(id, next);
        };
        frame->onClick = click;
        frame->dragId = id;

        m_cards->insertWidget(m_cards->count() - 1, frame);
        m_byId.insert(id, Card{frame, id, bedienbar, m_vis->isVisible(id), click,
                               badge, keys});
        m_order << id;
    }

    // Die Zusatzeintraege (Panels des Contestprogramms): dieselbe Karte,
    // nur Zustand und Schalter kommen vom Anmelder, nicht vom Controller.
    for (const ZusatzEintrag& z : m_zusatz) {
        auto* frame = new CardFrame(this);
        frame->setAttribute(Qt::WA_StyledBackground, true);
        frame->setCursor(Qt::PointingHandCursor);
        auto* col = new QVBoxLayout(frame);
        col->setContentsMargins(15, 11, 15, 12);
        col->setSpacing(4);
        auto* name = new QLabel(z.titel, frame);
        {
            QFont f = name->font();
            f.setPixelSize(13);
            f.setBold(true);
            name->setFont(f);
        }
        col->addWidget(name);
        auto* keys = new QLabel(z.schlagworte.join(QStringLiteral(" · ")), frame);
        {
            QFont f = keys->font();
            f.setPixelSize(11);
            keys->setFont(f);
        }
        keys->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
            .arg(QString::fromLatin1(Style::kTextScale)));
        col->addWidget(keys);
        const QString id = z.id;
        auto click = [this, id]() {
            for (const ZusatzEintrag& e : m_zusatz) {
                if (e.id != id || !e.istAn || !e.schalte) { continue; }
                const bool next = !e.istAn();
                e.schalte(next);
                refresh();
                emit toggled(id, next);
                return;
            }
        };
        frame->onClick = click;
        frame->dragId = id;
        m_cards->insertWidget(m_cards->count() - 1, frame);
        m_byId.insert(id, Card{frame, id, true, z.istAn ? z.istAn() : false, click});
        m_order << id;
    }
    refresh();
}

void WidgetPicker::setZusatzEintraege(const QList<ZusatzEintrag>& liste)
{
    m_zusatz = liste;
    rebuild();
}

void WidgetPicker::setEinschalter(const QHash<QString, Einschalter>& e)
{
    m_einschalter = e;
    rebuild();
}

QString WidgetPicker::unterzeileFuer(const QString& id, bool verfuegbar) const
{
    if (verfuegbar) {
        return m_vis ? m_vis->keywords(id).join(QStringLiteral(" · ")) : QString();
    }
    const auto it = m_einschalter.constFind(id);
    if (it != m_einschalter.cend() && !it->hinweis.isEmpty()) { return it->hinweis; }
    return tr("The hardware or master switch behind it is off.");
}

void WidgetPicker::refresh()
{
    if (!m_vis) { return; }
    for (const QString& id : m_order) {
        Card& c = m_byId[id];
        const ZusatzEintrag* z = nullptr;
        for (const ZusatzEintrag& e : m_zusatz) { if (e.id == id) { z = &e; break; } }
        if (z) {
            c.checked = z->istAn ? z->istAn() : false;
            c.enabled = true;
        } else {
            const bool avail = m_vis->isAvailable(id);
            c.checked = m_vis->isVisible(id);
            c.enabled = avail || m_einschalter.contains(id);
            if (c.offMarke) { c.offMarke->setVisible(!avail); }
            if (c.unterzeile) { c.unterzeile->setText(unterzeileFuer(id, avail)); }
        }
        if (!c.frame) { continue; }
        c.frame->setEnabled(c.enabled);
        c.frame->setCursor(c.enabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
        // Eine gewählte Karte trägt den Auswahlrahmen — genau eine
        // Eigenschaft, zwei Zustände, kein Haken nötig. Angehakt = eine
        // Mulde (kGlassSel*), kein Blau: Betreiber 2026-11-02 "kein
        // Auswahl-Blau", und am 2026-11-04 am Auswaehler selbst: "blauer
        // hintergrund ganz rechts". Dieselbe Mulde wie an den
        // Kopfleisten-Pillen.
        c.frame->setStyleSheet(c.checked
            ? QStringLiteral(
                "CardFrame { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
                "            stop:0 %1, stop:1 %2); border: 1px solid %3;"
                "            border-radius: 6px; }")
                .arg(QString::fromLatin1(Style::kGlassSelTop),
                     QString::fromLatin1(Style::kGlassSelBot),
                     QString::fromLatin1(Style::kGlassSelBorder))
            : QStringLiteral(
                "CardFrame { background: %1; border: 1px solid %2;"
                "            border-radius: 6px; }")
                .arg(QString::fromLatin1(Style::kInsetBg),
                     QString::fromLatin1(Style::kBorderSubtle)));
    }
    applyFilter();
}

void WidgetPicker::applyFilter()
{
    if (!m_vis) { return; }
    for (const QString& id : m_order) {
        Card& c = m_byId[id];
        if (!c.frame) { continue; }
        const ZusatzEintrag* z = nullptr;
        for (const ZusatzEintrag& e : m_zusatz) { if (e.id == id) { z = &e; break; } }
        bool byCat = false;
        bool byText = false;
        if (z) {
            byCat = m_category == allCategory() || z->kategorie == m_category;
            const QString n = m_needle.trimmed().toLower();
            byText = n.isEmpty() || z->titel.toLower().contains(n)
                     || z->id.toLower().contains(n);
            for (const QString& k : z->schlagworte) {
                if (byText) { break; }
                byText = k.toLower().contains(n);
            }
        } else {
            byCat = m_category == allCategory()
                    || m_vis->category(id) == m_category;
            // "Was wir noch nicht haben, benoetigen wir nicht": ein
            // Applet hinter einem ausgeschalteten Hauptschalter steht
            // gar nicht in der Liste (AppletVisibilityController::isListed).
            byText = m_vis->matches(id, m_needle) && m_vis->isListed(id);
        }
        c.frame->setVisible(byCat && byText);
    }
}

void WidgetPicker::setCategory(const QString& category)
{
    if (category.isEmpty() || m_category == category) { return; }
    m_category = category;
    applyFilter();
}

void WidgetPicker::setSearch(const QString& needle)
{
    if (m_needle == needle) { return; }
    m_needle = needle;
    applyFilter();
}

QStringList WidgetPicker::categoryColumn() const
{
    QStringList out;
    for (int i = 0; i < m_categories->count(); ++i) {
        out << m_categories->item(i)->text();
    }
    return out;
}

QStringList WidgetPicker::entries() const
{
    QStringList out;
    for (const QString& id : m_order) {
        const Card* c = card(id);
        // isVisible() taugt hier nicht: solange der Dialog selbst nicht
        // gezeigt wurde, ist jedes Kind unsichtbar, und der Test saehe
        // eine leere Liste. isHidden() fragt nur, ob es AUSDRUECKLICH
        // versteckt wurde — und genau das macht der Filter.
        if (c && c->frame && !c->frame->isHidden()) { out << id; }
    }
    return out;
}

WidgetPicker::Card* WidgetPicker::card(const QString& id)
{
    auto it = m_byId.find(id);
    return it == m_byId.end() ? nullptr : &it.value();
}

const WidgetPicker::Card* WidgetPicker::card(const QString& id) const
{
    const auto it = m_byId.constFind(id);
    return it == m_byId.constEnd() ? nullptr : &it.value();
}

bool WidgetPicker::isChecked(const QString& id) const
{
    const Card* c = card(id);
    return c && c->checked;
}

bool WidgetPicker::isEnabled(const QString& id) const
{
    const Card* c = card(id);
    return c && c->enabled;
}

bool WidgetPicker::toggle(const QString& id)
{
    Card* c = card(id);
    if (!c || !c->enabled || !c->frame || c->frame->isHidden()) {
        return false;
    }
    if (!c->click) { return false; }
    c->click();
    return true;
}

// ── Das Plus ─────────────────────────────────────────────────────────


// ── Das Plus, von Hand gemalt ────────────────────────────────────────
//
// Es war ein QPushButton mit einem „+" darin und einem gestrichelten
// Rand aus dem Stilblatt. Der Wunsch vom 2026-10-09: „das PLUS, wo man
// windows hinzufuegen kann, sollte animiert sein, lass dir was
// einfallen."
//
// Was dabei NICHT entstanden ist: ein hupfendes, pulsierendes oder
// sich drehendes Zierstueck. Der Hausstil hat zu Bewegung genau einen
// Satz, und der ist eine Grenze: „Zustandswechsel: 150 ms ease.
// Nichts springt." Also bewegt sich hier nichts von selbst. Bewegt
// wird nur, was auch etwas AUSSAGT:
//
//   offen   — das Plus dreht sich in 180 ms um 45 Grad und steht als
//             Kreuz da. Derselbe Knopf schliesst den Auswaehler
//             wieder, und man sieht ihm an, dass er das jetzt tut.
//             Das ist Regel 5 in Bewegung: Zustand, immer an
//             derselben Stelle.
//   Zeiger  — 150 ms, in denen Rand und Zeichen zur blauen Kante
//             wandern und eine Flaeche darunter aufgeht. Vorher
//             sprang das per :hover um.
//   Druck   — 90 ms auf 94 Prozent und zurueck. Kurz genug, um nicht
//             aufzufallen, lang genug, dass die Hand eine Antwort
//             bekommt.
//
// Selbst gemalt statt per Stilblatt, aus zwei Gruenden. Drehen laesst
// sich ein Textzeichen in einem Stilblatt nicht. Und der geerbte
// Innenabstand aus Style::kButtonStyle, der diesem Knopf schon einmal
// das Zeichen weggefressen haette (HAUSSTIL.md, „der geerbte
// Innenabstand"), kann hier gar nicht mehr zuschlagen: ein eigener
// paintEvent kennt kein padding.
class PlusGlyph : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(qreal turn  READ turn  WRITE setTurn)
    Q_PROPERTY(qreal hover READ hover WRITE setHover)
    Q_PROPERTY(qreal press READ press WRITE setPress)
    // Das ruhige Atmen, damit man das Plus ueberhaupt findet.
    Q_PROPERTY(qreal puls  READ puls  WRITE setPuls)

public:
    explicit PlusGlyph(QWidget* parent = nullptr)
        : QAbstractButton(parent)
    {
        setFixedSize(AddWidgetButton::kSide, AddWidgetButton::kSide);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);

        m_turnAnim  = new QPropertyAnimation(this, "turn", this);
        m_turnAnim->setDuration(180);
        m_turnAnim->setEasingCurve(QEasingCurve::OutCubic);

        m_hoverAnim = new QPropertyAnimation(this, "hover", this);
        m_hoverAnim->setDuration(150);
        m_hoverAnim->setEasingCurve(QEasingCurve::InOutQuad);

        m_pressAnim = new QPropertyAnimation(this, "press", this);
        m_pressAnim->setDuration(90);
        m_pressAnim->setEasingCurve(QEasingCurve::OutQuad);

        // ── Warum es atmet ───────────────────────────────────────
        //
        // „das plus symbol sollte eine animation sein, dann kann man
        //  es leichter sehen."
        //
        // Die Drehung beim Oeffnen gab es schon -- die sieht aber
        // nur, wer das Plus bereits gefunden hat. Gesucht war das
        // Gegenteil: etwas, das den Blick AUF es lenkt, bevor man es
        // kennt.
        //
        // Drei Sekunden je Atemzug, Sinus, und die Amplitude
        // absichtlich klein: ein Rand, der zwischen ruhig und einem
        // Hauch Blau wandert, dazu ein weicher Schein. Keine
        // Groessenaenderung, kein Blinken. „Nichts springt"
        // (HAUSSTIL) gilt auch fuer etwas, das auffallen soll --
        // eine Ecke, die eine Stunde lang zuckt, schaltet man ab,
        // und dann hat man nichts gewonnen.
        //
        // Es haelt an, sobald der Zeiger darauf liegt oder der
        // Auswaehler offen ist: dann ist es gefunden, und das Atmen
        // haette seine Aufgabe erfuellt.
        m_pulsAnim = new QPropertyAnimation(this, "puls", this);
        m_pulsAnim->setDuration(3000);
        m_pulsAnim->setEasingCurve(QEasingCurve::InOutSine);
        m_pulsAnim->setKeyValueAt(0.0, 0.0);
        m_pulsAnim->setKeyValueAt(0.5, 1.0);
        m_pulsAnim->setKeyValueAt(1.0, 0.0);
        m_pulsAnim->setLoopCount(-1);
        m_pulsAnim->start();
    }

    qreal turn()  const { return m_turn; }
    qreal hover() const { return m_hover; }
    qreal press() const { return m_press; }
    qreal puls()  const { return m_puls; }
    void setTurn (qreal v) { m_turn  = v; update(); }
    void setHover(qreal v) { m_hover = v; update(); }
    void setPress(qreal v) { m_press = v; update(); }
    void setPuls (qreal v) { m_puls  = v; update(); }

    // Offen heisst Kreuz. Gerufen vom Auswaehler, nicht vom Klick --
    // er kann auch ueber sein eigenes Schliesskreuz zugehen, und dann
    // muss das Plus trotzdem zurueckdrehen.
    void setOpen(bool open)
    {
        if (m_open == open) { return; }
        m_open = open;
        m_turnAnim->stop();
        m_turnAnim->setStartValue(m_turn);
        m_turnAnim->setEndValue(open ? 1.0 : 0.0);
        m_turnAnim->start();
        atmen(!open);
    }

protected:
    void enterEvent(QEnterEvent*) override
    {
        glide(m_hoverAnim, m_hover, 1.0);
        atmen(false);
    }
    void leaveEvent(QEvent*) override
    {
        glide(m_hoverAnim, m_hover, 0.0);
        atmen(!m_open);
    }
    void mousePressEvent(QMouseEvent* e) override
    {
        glide(m_pressAnim, m_press, 1.0);
        QAbstractButton::mousePressEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        glide(m_pressAnim, m_press, 0.0);
        QAbstractButton::mouseReleaseEvent(e);
    }

    void atmen(bool an)
    {
        if (an) {
            if (m_pulsAnim->state() != QAbstractAnimation::Running) {
                m_pulsAnim->start();
            }
        } else {
            m_pulsAnim->stop();
            setPuls(0.0);
        }
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter g(this);
        g.setRenderHint(QPainter::Antialiasing, true);

        // ── Orange-gelbe Taste, schwarzes Zeichen (2026-11-12) ──────
        //
        // Betreiber: „den plus punkt fuer das adden von window bitte in
        // anderer farbe machen" und dann genauer: „der hintergrund
        // eigentlich orange gelb ist und das plus schwarz bleibt" --
        // „ev eine animation". Vorher: grauer, gestrichelter Rand mit
        // grauem Zeichen, das Atmen ein Hauch Blau im Rand; auf dem
        // Bildschirmfoto kaum zu finden. Jetzt eine gefuellte Taste in
        // der Familie der Sendetaste (Messing, dunkle Schrift), nur
        // heller und gelber, damit sie nicht mit ihr verwechselt wird.
        // Das Atmen hebt und senkt die Helligkeit der Flaeche, der
        // Zeiger hellt sie auf, der offene Auswaehler dunkelt sie ab.
        // Seit 2026-11-17 mit Namen (StyleConstants.h): die Klickmarke
        // braucht dieselben drei.
        const QColor obenRuhe(QString::fromLatin1(Style::kPlusTop));
        const QColor untenRuhe(QString::fromLatin1(Style::kPlusBottom));
        const QColor randRuhe(QString::fromLatin1(Style::kPlusBorder));
        const QColor zeichen(QString::fromLatin1(Style::kTxKeyText));

        const qreal wachheit = std::max(m_hover, m_turn);
        // Der Ruf: nur solange nichts anderes wach ist. Ein Atemzug
        // hebt die Flaeche um hoechstens 14 Prozent -- sichtbar, aber
        // kein Blinken.
        const qreal ruf = (1.0 - wachheit) * m_puls;
        const int heller = 100 + int(14.0 * ruf) + int(10.0 * m_hover);
        const int dunkler = 100 + int(18.0 * m_turn);
        auto ton = [&](QColor c) {
            c = c.lighter(heller);
            return m_turn > 0.01 ? c.darker(dunkler) : c;
        };

        const qreal schrumpf = 1.0 - 0.06 * m_press;
        const QRectF voll(rect());
        QRectF feld(0, 0, voll.width() * schrumpf, voll.height() * schrumpf);
        feld.moveCenter(voll.center());
        feld.adjust(0.5, 0.5, -0.5, -0.5);

        QLinearGradient flaeche(feld.topLeft(), feld.bottomLeft());
        flaeche.setColorAt(0.0, ton(obenRuhe));
        flaeche.setColorAt(1.0, ton(untenRuhe));
        g.setBrush(flaeche);
        g.setPen(QPen(ton(randRuhe), 1.0));
        g.drawRoundedRect(feld, 6.0, 6.0);

        // Zwei Balken um die Mitte. 45 Grad daraus machen ein Kreuz.
        g.save();
        g.translate(feld.center());
        g.rotate(45.0 * m_turn);
        g.setPen(Qt::NoPen);
        g.setBrush(zeichen);
        const qreal arm  = feld.width() * 0.28;
        const qreal dick = 2.2;   // schwarz auf Gelb: etwas kraeftiger
        g.drawRoundedRect(QRectF(-arm, -dick / 2.0, arm * 2.0, dick),
                          dick / 2.0, dick / 2.0);
        g.drawRoundedRect(QRectF(-dick / 2.0, -arm, dick, arm * 2.0),
                          dick / 2.0, dick / 2.0);
        g.restore();
    }

private:
    static QColor mix(const QColor& a, const QColor& b, qreal t)
    {
        t = std::clamp(t, 0.0, 1.0);
        return QColor::fromRgbF(a.redF()   + (b.redF()   - a.redF())   * t,
                                a.greenF() + (b.greenF() - a.greenF()) * t,
                                a.blueF()  + (b.blueF()  - a.blueF())  * t);
    }

    static void glide(QPropertyAnimation* a, qreal from, qreal to)
    {
        a->stop();
        a->setStartValue(from);
        a->setEndValue(to);
        a->start();
    }

    qreal m_turn{0.0}, m_hover{0.0}, m_press{0.0};
    qreal m_puls{0.0};
    bool  m_open{false};
    QPropertyAnimation* m_turnAnim{nullptr};
    QPropertyAnimation* m_hoverAnim{nullptr};
    QPropertyAnimation* m_pressAnim{nullptr};
    QPropertyAnimation* m_pulsAnim{nullptr};
};

AddWidgetButton::AddWidgetButton(AppletVisibilityController* vis,
                                 QWidget* parent)
    : QWidget(parent), m_vis(vis)
{
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);

    // ── Das Plus ist ein PlusGlyph, nicht ein QPushButton ───────────
    //
    // Gebaut und unerreichbar (gefunden 2026-10-20): `PlusGlyph` steht
    // weiter unten in dieser Datei mit allem, was der Betreiber
    // verlangt hat -- „das plus symbol sollte eine animation sein,
    // dann kann man es leichter sehen" -- Drehung ins Kreuz beim
    // Oeffnen, Zeiger- und Druckantwort, das ruhige Atmen. Nur ERZEUGT
    // hat es niemand: hier stand ein einfacher QPushButton mit
    // Stilvorlage, `m_glyph` blieb nullptr, und damit war
    // `setGlyphOpen()` ein Leerlauf und `PlusGlyph` toter Code. Der
    // CHANGELOG versprach das Atmen schon („Das Plus atmet").
    //
    // Aufgefallen ist es an `tst_plus_glyph_renders`: der Lauf fand
    // einen QPushButton statt des Glyphs, `setProperty("hover", 1.0)`
    // meldete `false` (kein solches Q_PROPERTY), und alle vier
    // Zustaende lieferten BILDGLEICHE Bilder -- gemessen 0 von 841
    // Bildpunkten Unterschied.
    //
    // Der gestrichelte Rand, die Rundung und die Farben stecken in
    // `PlusGlyph::paintEvent`; eine Stilvorlage braucht es nicht mehr,
    // und der geerbte Innenabstand aus `Style::kButtonStyle` kann
    // nichts mehr verdruecken.
    auto* plus = new PlusGlyph(this);
    // Keine Klickmarke: das Plus IST schon das Orange und antwortet auf
    // den Druck mit seiner eigenen Bewegung (KlickMarke.h).
    plus->setProperty("klickmarke", false);
    plus->setToolTip(tr("Add or remove a widget"));
    m_glyph = plus;
    col->addWidget(plus);

    // Ein Druck auf ein offenes Kreuz schliesst wieder. Vorher war das
    // Plus eine Einbahn: zweimal druecken hiess zweimal oeffnen, und
    // das Fenster stand schon da.
    connect(plus, &QAbstractButton::clicked, this, [this]() {
        if (m_picker && m_picker->isVisible()) { m_picker->close(); }
        else                                   { openPicker(); }
    });
}

void AddWidgetButton::setGlyphOpen(bool open)
{
    if (auto* g = qobject_cast<PlusGlyph*>(m_glyph)) { g->setOpen(open); }
}

// Der Auswaehler kann auch an uns vorbei zugehen -- ueber sein eigenes
// Schliesskreuz oder mit Esc. Dann muss das Plus trotzdem
// zurueckdrehen, sonst steht ein Kreuz ueber einem geschlossenen
// Fenster und luegt.
bool AddWidgetButton::eventFilter(QObject* watched, QEvent* e)
{
    if (watched == m_picker
        && (e->type() == QEvent::Close || e->type() == QEvent::Hide)) {
        setGlyphOpen(false);
    }
    return QWidget::eventFilter(watched, e);
}

void AddWidgetButton::setZusatzEintraege(const QList<WidgetPicker::ZusatzEintrag>& liste)
{
    m_zusatz = liste;
    if (m_picker) { m_picker->setZusatzEintraege(liste); }
}

void AddWidgetButton::setEinschalter(const QHash<QString, WidgetPicker::Einschalter>& e)
{
    m_einschalter = e;
    if (m_picker) { m_picker->setEinschalter(e); }
}

void AddWidgetButton::openPicker()
{
    if (!m_picker) {
        // ── Ein Fenster, kein Aufklapper ─────────────────────────────
        //
        // Erste Fassung war Qt::Popup an der Mausposition. Zwei Fehler
        // in drei Zeilen, beide am 2026-08-15 im Betrieb aufgefallen:
        //
        // Ein Popup schließt beim ersten Klick daneben. Wer sein
        // Fenster einrichtet, schaltet aber fünf Widgets nacheinander
        // ein und will dazwischen hinsehen — und ein Popup lässt sich
        // grundsätzlich nicht verschieben, also verdeckt es genau das,
        // worüber man gerade entscheidet.
        //
        // Qt::Tool: eigenes Fenster mit Titelleiste, verschiebbar,
        // bleibt offen, bleibt über dem Hauptfenster.
        m_picker = new WidgetPicker(m_vis, this);
        if (!m_zusatz.isEmpty()) { m_picker->setZusatzEintraege(m_zusatz); }
        if (!m_einschalter.isEmpty()) { m_picker->setEinschalter(m_einschalter); }
        m_picker->setWindowFlags(Qt::Tool);
        m_picker->setWindowTitle(tr("Add a widget"));
        connect(m_picker, &WidgetPicker::toggled,
                this, &AddWidgetButton::toggled);
        m_picker->installEventFilter(this);
    }
    m_picker->refresh();
    m_picker->adjustSize();
    m_picker->move(placeNear(m_picker->size()));
    m_picker->show();
    m_picker->raise();
    m_picker->activateWindow();
    setGlyphOpen(true);
}

QPoint AddWidgetButton::placeNear(const QSize& want) const
{
    // Unter dem Plus, rechtsbündig — und dann in den Bildschirm
    // geschoben.
    //
    // Vorher wurde stur an QCursor::pos() gesetzt. Das Plus sitzt am
    // rechten Ende der Kommandoleiste, also stand der Zeiger dort, also
    // öffnete ein 620 Pixel breites Fenster über den rechten Rand
    // hinaus: sichtbar blieb ein Streifen von zwei Zentimetern. Nicht
    // „schlecht platziert" — unbenutzbar.
    const QPoint anchor = mapToGlobal(QPoint(width(), height() + 6));
    QRect r(anchor.x() - want.width(), anchor.y(),
            want.width(), want.height());

    const QScreen* s = screen();
    if (!s) { s = QGuiApplication::primaryScreen(); }
    if (s) {
        const QRect avail = s->availableGeometry();
        // Erst rechts/unten hineinschieben, dann links/oben — in dieser
        // Reihenfolge, sonst schiebt der zweite Schritt den ersten
        // wieder hinaus, wenn das Fenster größer als der Schirm ist.
        if (r.right()  > avail.right())  { r.moveRight(avail.right() - 8); }
        if (r.bottom() > avail.bottom()) { r.moveBottom(avail.bottom() - 8); }
        if (r.left() < avail.left()) { r.moveLeft(avail.left() + 8); }
        if (r.top()  < avail.top())  { r.moveTop(avail.top() + 8); }
    }
    return r.topLeft();
}

} // namespace Longpath

// PlusGlyph traegt Q_OBJECT und lebt in dieser .cpp.
#include "WidgetPicker.moc"
