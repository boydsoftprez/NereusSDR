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

#include "SectionParts.h"

#include <QBoxLayout>
#include <QLayout>
#include <QLayoutItem>
#include <QWidget>

#include <algorithm>

namespace NereusSDR {

namespace {

// Whether `item` is `widget` or a layout that holds it.
bool holds(QLayoutItem* item, const QWidget* widget)
{
    if (!item) {
        return false;
    }
    if (item->widget() == widget) {
        return true;
    }
    if (QLayout* layout = item->layout()) {
        for (int i = 0; i < layout->count(); ++i) {
            if (holds(layout->itemAt(i), widget)) {
                return true;
            }
        }
    }
    return false;
}

// The index of the item of `layout` that is or holds `widget`, or -1.
int indexHolding(QBoxLayout* layout, const QWidget* widget)
{
    for (int i = 0; i < layout->count(); ++i) {
        if (holds(layout->itemAt(i), widget)) {
            return i;
        }
    }
    return -1;
}

// Moves the items of `layout` so that the units come in the given order.
// Each unit is the set of item indices it owns.  The slots the units fill
// together are refilled in unit order; every other item stays where it is.
// Each item keeps its stretch factor and alignment.
void permute(QBoxLayout* layout, const QVector<QVector<int>>& units)
{
    QVector<int> fillSlots;
    QVector<int> sequence;
    for (const QVector<int>& unit : units) {
        for (int index : unit) {
            if (fillSlots.contains(index)) {
                // Two units claim one item: leave the layout as built.
                return;
            }
            fillSlots.append(index);
            sequence.append(index);
        }
    }
    std::sort(fillSlots.begin(), fillSlots.end());
    if (sequence == fillSlots) {
        return;
    }

    const int count = layout->count();
    QVector<int> finalOrder(count);
    for (int i = 0; i < count; ++i) {
        finalOrder[i] = i;
    }
    for (int p = 0; p < fillSlots.size(); ++p) {
        finalOrder[fillSlots.at(p)] = sequence.at(p);
    }

    struct Taken {
        QLayoutItem* item = nullptr;
        int stretch = 0;
    };
    QVector<Taken> items(count);
    for (int i = 0; i < count; ++i) {
        items[i].stretch = layout->stretch(i);
    }
    for (int i = 0; i < count; ++i) {
        items[i].item = layout->takeAt(0);
    }
    for (int i = 0; i < count; ++i) {
        const Taken& taken = items.at(finalOrder.at(i));
        if (QLayout* child = taken.item->layout()) {
            layout->insertLayout(i, child, taken.stretch);
        } else {
            layout->insertItem(i, taken.item);
            layout->setStretch(i, taken.stretch);
        }
    }
}

QString lineUnitKey(int line)
{
    return QStringLiteral("#line%1").arg(line);
}

QString partUnitKey(const QString& id)
{
    return QStringLiteral("#part ") + id;
}

} // namespace

SectionParts::SectionParts(QWidget* section)
    : m_section(section)
{
}

void SectionParts::add(const QString& id, const QString& label, int line, QList<QWidget*> widgets)
{
    widgets.removeAll(nullptr);
    for (QWidget* widget : std::as_const(widgets)) {
        m_allowed.insert(widget, !widget->isHidden());
    }
    Part part;
    part.id = id;
    part.label = label;
    part.line = line;
    part.widgets = widgets;
    m_parts.append(part);
}

void SectionParts::apply(const QJsonObject& config)
{
    for (Part& part : m_parts) {
        part.hidden = config.value(hideKey(part.id)).toBool(false);
        refresh(part);
    }
    const QStringList order = effectiveOrder(builtParts(), config.value(orderKey()).toArray());
    reorder(order);
}

bool SectionParts::isHidden(const QString& id) const
{
    const Part* part = find(id);
    return part && part->hidden;
}

void SectionParts::setHidden(const QString& id, bool hidden)
{
    if (Part* part = find(id)) {
        part->hidden = hidden;
        refresh(*part);
    }
}

void SectionParts::setDynamicVisible(const QString& id, bool allowed)
{
    if (Part* part = find(id)) {
        for (QWidget* widget : std::as_const(part->widgets)) {
            m_allowed.insert(widget, allowed);
        }
        refresh(*part);
    }
}

void SectionParts::setDynamicVisible(QWidget* widget, bool allowed)
{
    if (!widget) {
        return;
    }
    m_allowed.insert(widget, allowed);
    for (const Part& part : std::as_const(m_parts)) {
        if (part.widgets.contains(widget)) {
            refreshWidget(widget, part.hidden);
            return;
        }
    }
    // Not a part's widget: the applet's word alone decides.
    refreshWidget(widget, false);
}

QStringList SectionParts::ids() const
{
    QStringList result;
    for (const Part& part : m_parts) {
        result.append(part.id);
    }
    return result;
}

QString SectionParts::label(const QString& id) const
{
    const Part* part = find(id);
    return part ? part->label : QString();
}

int SectionParts::line(const QString& id) const
{
    const Part* part = find(id);
    return part ? part->line : -1;
}

QList<QWidget*> SectionParts::widgets(const QString& id) const
{
    const Part* part = find(id);
    return part ? part->widgets : QList<QWidget*>{};
}

QString SectionParts::hideKey(const QString& id)
{
    QString key = id;
    if (!key.isEmpty()) {
        key[0] = key.at(0).toUpper();
    }
    return QStringLiteral("hide") + key;
}

QString SectionParts::orderKey()
{
    return QStringLiteral("order");
}

QStringList SectionParts::effectiveOrder(const QVector<QPair<QString, int>>& builtParts,
                                         const QJsonArray& order)
{
    QStringList known;
    QHash<QString, int> lineOf;
    for (const auto& part : builtParts) {
        known.append(part.first);
        lineOf.insert(part.first, part.second);
    }

    QStringList listed;
    for (const QJsonValue& value : order) {
        const QString id = value.toString();
        if (known.contains(id) && !listed.contains(id)) {
            listed.append(id);
        }
    }
    for (const QString& id : std::as_const(known)) {
        if (!listed.contains(id)) {
            listed.append(id);
        }
    }

    QStringList unitKeys;
    QHash<QString, QStringList> unitParts;
    for (const QString& id : std::as_const(listed)) {
        const int line = lineOf.value(id, -1);
        const QString key = line >= 0 ? lineUnitKey(line) : partUnitKey(id);
        if (!unitKeys.contains(key)) {
            unitKeys.append(key);
        }
        unitParts[key].append(id);
    }

    QStringList result;
    for (const QString& key : std::as_const(unitKeys)) {
        result.append(unitParts.value(key));
    }
    return result;
}

QVector<ContentProperty> SectionParts::properties(const SectionParts& parts)
{
    QVector<ContentProperty> result;
    QJsonArray built;
    QVector<ContentPropertyChoice> choices;
    for (const Part& part : parts.m_parts) {
        ContentProperty hide;
        hide.key = hideKey(part.id);
        hide.label = QStringLiteral("Hide ") + part.label;
        hide.kind = ContentProperty::Kind::Toggle;
        hide.defaultValue = false;
        result.append(hide);

        built.append(part.id);
        ContentPropertyChoice choice;
        choice.label = part.label;
        choice.value = QJsonObject{{QStringLiteral("id"), part.id},
                                   {QStringLiteral("line"), part.line}};
        choices.append(choice);
    }
    if (!parts.m_parts.isEmpty()) {
        ContentProperty order;
        order.key = orderKey();
        order.label = QStringLiteral("Order");
        order.kind = ContentProperty::Kind::Order;
        order.choices = choices;
        order.defaultValue = built;
        result.append(order);
    }
    return result;
}

const SectionParts::Part* SectionParts::find(const QString& id) const
{
    for (const Part& part : m_parts) {
        if (part.id == id) {
            return &part;
        }
    }
    return nullptr;
}

SectionParts::Part* SectionParts::find(const QString& id)
{
    for (Part& part : m_parts) {
        if (part.id == id) {
            return &part;
        }
    }
    return nullptr;
}

QVector<QPair<QString, int>> SectionParts::builtParts() const
{
    QVector<QPair<QString, int>> result;
    for (const Part& part : m_parts) {
        result.append({part.id, part.line});
    }
    return result;
}

void SectionParts::refresh(const Part& part)
{
    for (QWidget* widget : part.widgets) {
        refreshWidget(widget, part.hidden);
    }
}

void SectionParts::refreshWidget(QWidget* widget, bool partHidden)
{
    const bool shown = m_allowed.value(widget, true) && !partHidden;
    // Only a change is applied, so a widget that already shows as it
    // should is left exactly as it was built.
    if (widget->isHidden() == shown) {
        widget->setVisible(shown);
    }
}

void SectionParts::reorder(const QStringList& order)
{
    auto* column = qobject_cast<QBoxLayout*>(m_section ? m_section->layout() : nullptr);
    if (!column) {
        return;
    }

    // The column: one unit per shared line and per column part.
    QStringList unitKeys;
    QHash<QString, QVector<int>> unitIndices;
    QVector<int> lines;
    for (const QString& id : order) {
        const Part* part = find(id);
        if (!part) {
            continue;
        }
        const QString key = part->line >= 0 ? lineUnitKey(part->line) : partUnitKey(id);
        if (!unitKeys.contains(key)) {
            unitKeys.append(key);
        }
        if (part->line >= 0 && !lines.contains(part->line)) {
            lines.append(part->line);
        }
        QVector<int>& indices = unitIndices[key];
        for (QWidget* widget : part->widgets) {
            const int index = indexHolding(column, widget);
            if (index >= 0 && !indices.contains(index)) {
                indices.append(index);
            }
        }
    }
    QVector<QVector<int>> columnUnits;
    for (const QString& key : std::as_const(unitKeys)) {
        QVector<int> indices = unitIndices.value(key);
        std::sort(indices.begin(), indices.end());
        if (!indices.isEmpty()) {
            columnUnits.append(indices);
        }
    }
    permute(column, columnUnits);

    // Within each shared line, the parts follow the order.
    for (int line : std::as_const(lines)) {
        QBoxLayout* lineLayout = nullptr;
        QVector<QVector<int>> lineUnits;
        for (const QString& id : order) {
            const Part* part = find(id);
            if (!part || part->line != line) {
                continue;
            }
            if (!lineLayout) {
                for (QWidget* widget : part->widgets) {
                    const int index = indexHolding(column, widget);
                    if (index >= 0) {
                        lineLayout = qobject_cast<QBoxLayout*>(column->itemAt(index)->layout());
                        break;
                    }
                }
            }
            if (!lineLayout) {
                break;
            }
            QVector<int> indices;
            for (QWidget* widget : part->widgets) {
                const int index = indexHolding(lineLayout, widget);
                if (index >= 0 && !indices.contains(index)) {
                    indices.append(index);
                }
            }
            std::sort(indices.begin(), indices.end());
            if (!indices.isEmpty()) {
                lineUnits.append(indices);
            }
        }
        if (lineLayout) {
            permute(lineLayout, lineUnits);
        }
    }
}

} // namespace NereusSDR
