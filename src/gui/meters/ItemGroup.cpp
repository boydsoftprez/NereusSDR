#include "ItemGroup.h"

#include <QStringList>
#include <QtAlgorithms>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

ItemGroup::ItemGroup(const QString& name, QObject* parent)
    : QObject(parent)
    , m_name(name)
{
}

ItemGroup::~ItemGroup()
{
    qDeleteAll(m_items);
    m_items.clear();
}

// ---------------------------------------------------------------------------
// setRect
// ---------------------------------------------------------------------------

void ItemGroup::setRect(float x, float y, float w, float h)
{
    m_x = x;
    m_y = y;
    m_w = w;
    m_h = h;
}

// ---------------------------------------------------------------------------
// addItem / removeItem
// ---------------------------------------------------------------------------

void ItemGroup::addItem(MeterItem* item)
{
    if (!item) {
        return;
    }
    item->setParent(this);
    m_items.append(item);
}

void ItemGroup::removeItem(MeterItem* item)
{
    m_items.removeOne(item);
}

// ---------------------------------------------------------------------------
// serialize
// Format:
//   GROUP
//   name
//   x
//   y
//   w
//   h
//   itemCount
//   item1_serialized
//   item2_serialized
//   ...
// ---------------------------------------------------------------------------

QString ItemGroup::serialize() const
{
    QStringList lines;
    lines << QStringLiteral("GROUP");
    lines << m_name;
    lines << QString::number(static_cast<double>(m_x));
    lines << QString::number(static_cast<double>(m_y));
    lines << QString::number(static_cast<double>(m_w));
    lines << QString::number(static_cast<double>(m_h));
    lines << QString::number(m_items.size());
    for (const MeterItem* item : m_items) {
        lines << item->serialize();
    }
    return lines.join(QLatin1Char('\n'));
}

// ---------------------------------------------------------------------------
// deserialize
// ---------------------------------------------------------------------------

ItemGroup* ItemGroup::deserialize(const QString& data, QObject* parent)
{
    const QStringList lines = data.split(QLatin1Char('\n'));
    if (lines.size() < 7 || lines[0] != QLatin1String("GROUP")) {
        return nullptr;
    }

    const QString name = lines[1];
    bool ok = true;
    const float x = lines[2].toFloat(&ok); if (!ok) { return nullptr; }
    const float y = lines[3].toFloat(&ok); if (!ok) { return nullptr; }
    const float w = lines[4].toFloat(&ok); if (!ok) { return nullptr; }
    const float h = lines[5].toFloat(&ok); if (!ok) { return nullptr; }
    const int count = lines[6].toInt(&ok); if (!ok) { return nullptr; }

    if (lines.size() < 7 + count) {
        return nullptr;
    }

    ItemGroup* group = new ItemGroup(name, parent);
    group->setRect(x, y, w, h);

    for (int i = 0; i < count; ++i) {
        const QString& itemData = lines[7 + i];
        // Detect type from first pipe-delimited field
        const int pipeIdx = itemData.indexOf(QLatin1Char('|'));
        const QString typeTag = (pipeIdx >= 0) ? itemData.left(pipeIdx) : itemData;

        MeterItem* item = nullptr;
        if (typeTag == QLatin1String("BAR")) {
            BarItem* bar = new BarItem();
            if (bar->deserialize(itemData)) {
                item = bar;
            } else {
                delete bar;
            }
        } else if (typeTag == QLatin1String("SOLID")) {
            SolidColourItem* solid = new SolidColourItem();
            if (solid->deserialize(itemData)) {
                item = solid;
            } else {
                delete solid;
            }
        } else if (typeTag == QLatin1String("IMAGE")) {
            ImageItem* img = new ImageItem();
            if (img->deserialize(itemData)) {
                item = img;
            } else {
                delete img;
            }
        } else if (typeTag == QLatin1String("SCALE")) {
            ScaleItem* scale = new ScaleItem();
            if (scale->deserialize(itemData)) {
                item = scale;
            } else {
                delete scale;
            }
        } else if (typeTag == QLatin1String("TEXT")) {
            TextItem* text = new TextItem();
            if (text->deserialize(itemData)) {
                item = text;
            } else {
                delete text;
            }
        }

        if (item) {
            group->addItem(item);
        }
    }

    return group;
}

// ---------------------------------------------------------------------------
// createHBarPreset
// Factory: horizontal bar meter with scale + readout (AetherSDR styling).
// Layout:
//   top 20%  — label (left) + readout (right)
//   mid 28%  — bar (0.22 top, 0.28 height)
//   bottom 46% — scale (0.52 top, 0.46 height)
// Colors from AetherSDR: cyan bar (#00b4d8), red zone (#ff4444), dark bg (#0f0f1a).
// ---------------------------------------------------------------------------

ItemGroup* ItemGroup::createHBarPreset(int bindingId, double minVal, double maxVal,
                                        const QString& name, QObject* parent)
{
    ItemGroup* group = new ItemGroup(name, parent);

    // Background fill
    SolidColourItem* bg = new SolidColourItem();
    bg->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    bg->setColour(QColor(QStringLiteral("#0f0f1a")));
    bg->setZOrder(0);
    group->addItem(bg);

    // Label text (static, no binding)
    TextItem* label = new TextItem();
    label->setRect(0.02f, 0.0f, 0.5f, 0.2f);
    label->setLabel(name);
    label->setBindingId(-1);
    label->setTextColor(QColor(QStringLiteral("#8090a0")));
    label->setFontSize(10);
    label->setBold(false);
    label->setZOrder(10);
    group->addItem(label);

    // Readout text (dynamic, bound to bindingId)
    TextItem* readout = new TextItem();
    readout->setRect(0.5f, 0.0f, 0.48f, 0.2f);
    readout->setLabel(QString());
    readout->setBindingId(bindingId);
    readout->setTextColor(QColor(QStringLiteral("#c8d8e8")));
    readout->setFontSize(13);
    readout->setBold(true);
    readout->setSuffix(QStringLiteral(" dBm"));
    readout->setDecimals(1);
    readout->setZOrder(10);
    group->addItem(readout);

    // Bar meter
    BarItem* bar = new BarItem();
    bar->setRect(0.02f, 0.22f, 0.96f, 0.28f);
    bar->setOrientation(BarItem::Orientation::Horizontal);
    bar->setRange(minVal, maxVal);
    bar->setBindingId(bindingId);
    bar->setBarColor(QColor(QStringLiteral("#00b4d8")));
    bar->setBarRedColor(QColor(QStringLiteral("#ff4444")));
    bar->setRedThreshold(maxVal * 0.9);
    bar->setZOrder(5);
    group->addItem(bar);

    // Scale
    ScaleItem* scale = new ScaleItem();
    scale->setRect(0.02f, 0.52f, 0.96f, 0.46f);
    scale->setOrientation(ScaleItem::Orientation::Horizontal);
    scale->setRange(minVal, maxVal);
    scale->setMajorTicks(7);
    scale->setMinorTicks(5);
    scale->setTickColor(QColor(QStringLiteral("#c8d8e8")));
    scale->setLabelColor(QColor(QStringLiteral("#8090a0")));
    scale->setFontSize(9);
    scale->setZOrder(10);
    group->addItem(scale);

    return group;
}

} // namespace NereusSDR
