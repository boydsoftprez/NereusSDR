// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/widgets/FlowLayout.h  (Longpath)
// =================================================================
// Longpath-original. No Thetis port.
//
// Eine Zeile, die umbricht: Knoepfe und Felder laufen von links nach
// rechts und rutschen in die naechste Zeile, wenn der Platz ausgeht —
// wie Text. Der Betreiber (2026-09-21): das Logbuchfenster liess sich
// nicht schmaler machen als seine Werkzeugleiste, „diese kann aber
// anstatt 1-zeilig auch 2-zeilig werden".
//
// Zwei Dinge, die ein Werkzeugleisten-Layout braucht und ein einfacher
// Umbruch nicht haette:
//   * Was in seiner Zeile uebrig bleibt, bekommen die Elemente, die
//     sich waagrecht ausdehnen wollen (QSizePolicy::ExpandFlag —
//     Suchfeld, Datumsfelder) — und ein addStretch() als Luecke, mit der
//     ein „Clear" rechts bleibt, solange die Zeile es hergibt.
//   * Die Mindestbreite ist das breiteste EINZELNE Element, nicht die
//     Summe — genau darum geht es.
// Hoehe folgt der Breite (hasHeightForWidth), das umschliessende
// QVBoxLayout fragt danach.
//
// 2026-11-18, fuer die drei Reihen des FT8-Fensters (Betreiber: „hier
// finde ich nun keinen button mehr für CQ" -- er stand rechts ausserhalb
// des Fensters). Drei Dinge, die eine Reihe aus einem QHBoxLayout
// mitbringt und die hier fehlten:
//   * addSpacing(): ein fester Abstand ZWISCHEN zwei Elementen. Am
//     Anfang und am Ende einer Zeile zaehlt er nicht -- eine
//     umgebrochene Zeile faengt links an, nicht acht Punkt eingerueckt.
//   * insertWidget() und addStretch(int): damit eine Reihe, die als
//     QHBoxLayout geschrieben wurde, mit denselben Aufrufen umbricht.
//   * Ein Element, das breiter waere als die ganze Zeile, bekommt die
//     Zeilenbreite, soweit seine Mindestbreite es erlaubt. Eine
//     Beschriftung mit QSizePolicy::Ignored zaehlt beim Umbruch mit
//     ihrer Mindestbreite (nicht mit ihrem Text) und nimmt dann, was in
//     der Zeile uebrig ist -- so bricht die Reihe nicht um, weil ein
//     Text laenger wird.
// Unveraendert: ein verstecktes Element zaehlt weiter mit Breite null
// und seinem Abstand (Logbuch und Karte verlassen sich darauf).
// =================================================================
#pragma once

#include <QLayout>
#include <QList>
#include <QRect>

namespace Longpath {

class FlowLayout : public QLayout {
    Q_OBJECT
public:
    explicit FlowLayout(QWidget* parent = nullptr, int hSpacing = 6, int vSpacing = 6);
    ~FlowLayout() override;

    void addItem(QLayoutItem* item) override;
    // Eine dehnbare Luecke — wie QBoxLayout::addStretch(). Der Faktor
    // wird angenommen und nicht gebraucht (alle Luecken einer Zeile
    // teilen gleich): so bleibt `addStretch(1)` aus einer QHBoxLayout-
    // Reihe stehen, wie es ist.
    void addStretch(int faktor = 0);
    // Ein fester Abstand zwischen zwei Elementen — wie
    // QBoxLayout::addSpacing(). Am Zeilenanfang und -ende entfaellt er.
    void addSpacing(int breite);
    // Wie QBoxLayout::insertWidget(): an die Stelle `index`.
    void insertWidget(int index, QWidget* widget);
    // Die dehnbare Luecke geht mit dem Element, das auf sie folgt, in
    // dessen Zeile -- so bleibt „Clear" auch in einer umgebrochenen
    // Zeile am rechten Rand. Ab Werk aus: dann bleibt die Luecke in der
    // Zeile, in der sie eingehaengt wurde, und was danach umbricht,
    // faengt links an (so stehen Logbuch und Karte seit 2026-09-21).
    void setStretchFollowsNext(bool on) { m_stretchFollows = on; invalidate(); }

    int  count() const override { return m_items.size(); }
    QLayoutItem* itemAt(int i) const override { return m_items.value(i); }
    QLayoutItem* takeAt(int i) override;

    Qt::Orientations expandingDirections() const override { return Qt::Horizontal; }
    bool  hasHeightForWidth() const override { return true; }
    int   heightForWidth(int width) const override;
    // Die Mindesthoehe bei dieser Breite IST die umgebrochene Hoehe —
    // ohne das rechnet das umschliessende QVBoxLayout mit einer Zeile
    // und die Nachbarn rutschen unter die Leiste (Kartenspalte bei
    // 640 px, 2026-09-22).
    int   minimumHeightForWidth(int width) const override { return heightForWidth(width); }
    QSize sizeHint() const override;
    QSize minimumSize() const override;
    void  setGeometry(const QRect& rect) override;

    // Zeilen beim letzten setGeometry — fuer Pruefstaende.
    int rowsLaidOut() const { return m_rows; }

private:
    int layOut(const QRect& rect, bool apply) const;
    static bool wantsWidth(QLayoutItem* item);

    QList<QLayoutItem*> m_items;
    int m_hSpace;
    int m_vSpace;
    bool m_stretchFollows{false};
    mutable int m_rows{0};
};

} // namespace Longpath
