#pragma once

// =================================================================
// src/gui/applets/SectionParts.{h,cpp}  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original file.  The named parts of a TX section
// row, which a container copy of the row can hide and reorder.  It contains
// no Thetis code and derives from no Thetis source.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-25 - The parts of a TX section row: a container copy hides
//                 each part and orders them from its entry config, and the
//                 property editor offers a toggle per part and the order.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/containers/ContainerContentRegistry.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

class QWidget;

namespace NereusSDR {

// The named parts of a section row (the MON button, the monitor volume, the
// slice indicators, ...).
//
// A line part sits with other parts in one shared line (a row of buttons);
// a column part owns whole items of the section's column.  The section
// registers its parts in the order it builds them, which is the order they
// show in.
//
// The section applies nothing on its own: only a container copy calls
// apply(), with its entry config, so the applet's own panel never changes.
//
// Config keys:
//   hide<Id>  (the part's id with its first letter uppercased): true hides
//             the part.
//   order     an array of part ids.  The column follows the list; a shared
//             line takes the position of its first listed part and moves as
//             a unit; within a line the parts follow the list.  Unknown ids
//             are ignored; unlisted parts keep their built position after
//             the listed ones.
//
// Visibility the applet decides at run time (PS-A on a board without
// PureSignal, the monitor notice, the holder line) goes through
// setDynamicVisible, which ANDs it with the part's hide setting, so a hidden
// part stays hidden whatever the applet says.
class SectionParts
{
public:
    explicit SectionParts(QWidget* section = nullptr);

    // line: the shared line the part sits in (0, 1, ...), or -1 for a
    // column part.  widgets: every widget the part shows.
    void add(const QString& id, const QString& label, int line, QList<QWidget*> widgets);

    // Hides the parts and orders them from a container entry's config.
    void apply(const QJsonObject& config);

    bool isHidden(const QString& id) const;
    void setHidden(const QString& id, bool hidden);

    // Whether the applet allows the part (or one widget of a part) to show.
    // The widget shows when it is allowed and its part is not hidden.
    void setDynamicVisible(const QString& id, bool allowed);
    void setDynamicVisible(QWidget* widget, bool allowed);

    // The part ids in the order they were built.
    QStringList ids() const;
    QString label(const QString& id) const;
    int line(const QString& id) const;
    QList<QWidget*> widgets(const QString& id) const;

    // The config key that hides a part: "hide" plus the id with its first
    // letter uppercased.
    static QString hideKey(const QString& id);
    static QString orderKey();

    // The parts in the order `order` gives them, as apply() lays them out:
    // the listed known ids, then the unlisted ones in built order, with
    // the parts of a shared line kept together at the position of the
    // first one.  `builtParts` is each part's id and line, in built order.
    static QStringList effectiveOrder(const QVector<QPair<QString, int>>& builtParts,
                                      const QJsonArray& order);

    // The editor's properties for these parts: a "Hide <part label>"
    // toggle per part, then the Order list (ContentProperty::Kind::Order).
    static QVector<ContentProperty> properties(const SectionParts& parts);

private:
    struct Part {
        QString id;
        QString label;
        int line = -1;
        QList<QWidget*> widgets;
        bool hidden = false;
    };

    const Part* find(const QString& id) const;
    Part* find(const QString& id);
    QVector<QPair<QString, int>> builtParts() const;
    void refresh(const Part& part);
    void refreshWidget(QWidget* widget, bool partHidden);
    void reorder(const QStringList& order);

    QWidget* m_section = nullptr;
    QVector<Part> m_parts;
    // Per widget: whether the applet allows it to show.  Starts as the
    // widget's own state when the part is added.
    QHash<QWidget*, bool> m_allowed;
};

} // namespace NereusSDR
