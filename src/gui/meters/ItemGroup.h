#pragma once

#include "MeterItem.h"

#include <QString>
#include <QVector>

namespace NereusSDR {

class ItemGroup : public QObject {
    Q_OBJECT

public:
    explicit ItemGroup(const QString& name, QObject* parent = nullptr);
    ~ItemGroup() override;

    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    float x() const { return m_x; }
    float y() const { return m_y; }
    float groupWidth() const { return m_w; }
    float groupHeight() const { return m_h; }
    void setRect(float x, float y, float w, float h);

    void addItem(MeterItem* item);
    void removeItem(MeterItem* item);
    QVector<MeterItem*> items() const { return m_items; }

    QString serialize() const;
    static ItemGroup* deserialize(const QString& data, QObject* parent = nullptr);

    // Preset factory: creates a horizontal bar meter with scale + readout.
    // Layout within group: top 20% label+readout, mid 28% bar, bottom 46% scale.
    // Colors from AetherSDR: cyan bar (#00b4d8), red zone (#ff4444), dark bg (#0f0f1a).
    static ItemGroup* createHBarPreset(int bindingId, double minVal, double maxVal,
                                        const QString& name, QObject* parent = nullptr);

private:
    QString m_name;
    float m_x{0.0f};
    float m_y{0.0f};
    float m_w{1.0f};
    float m_h{1.0f};
    QVector<MeterItem*> m_items;
};

} // namespace NereusSDR
