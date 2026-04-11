#pragma once

#include "MeterItem.h"

#include <QString>
#include <QVector>

namespace NereusSDR {

class MeterWidget;

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

    // Compact HBar: single-line layout (label left, bar center, readout right).
    // No scale ticks. From AetherSDR HGauge pattern.
    static ItemGroup* createCompactHBarPreset(int bindingId, double minVal, double maxVal,
                                               const QString& name, QObject* parent = nullptr);

    // S-Meter preset: arc-style NeedleItem with full readout.
    // From AetherSDR SMeterWidget visual style.
    static ItemGroup* createSMeterPreset(int bindingId, const QString& name,
                                          QObject* parent = nullptr);

    // Power/SWR preset: two stacked horizontal bars.
    // From Thetis MeterManager.cs AddPWRBar/AddSWRBar calibration.
    static ItemGroup* createPowerSwrPreset(const QString& name,
                                            QObject* parent = nullptr);

    // ALC preset: horizontal bar for TX ALC level.
    // From Thetis MeterManager.cs ALC scale (-30 to 0 dB).
    static ItemGroup* createAlcPreset(QObject* parent = nullptr);

    // Mic level preset: horizontal bar for TX mic level.
    // From Thetis MeterManager.cs MIC scale (-30 to 0 dB).
    static ItemGroup* createMicPreset(QObject* parent = nullptr);

    // Compressor level preset: horizontal bar for TX compressor.
    // From Thetis MeterManager.cs COMP scale (-25 to 0 dB).
    static ItemGroup* createCompPreset(QObject* parent = nullptr);

    // --- Phase 3G-4 bar presets (from Thetis MeterManager.cs AddMeter factory) ---
    static ItemGroup* createSignalBarPreset(QObject* parent = nullptr);
    static ItemGroup* createAvgSignalBarPreset(QObject* parent = nullptr);
    static ItemGroup* createMaxBinBarPreset(QObject* parent = nullptr);
    static ItemGroup* createAdcBarPreset(QObject* parent = nullptr);
    static ItemGroup* createAdcMaxMagPreset(QObject* parent = nullptr);
    static ItemGroup* createAgcBarPreset(QObject* parent = nullptr);
    static ItemGroup* createAgcGainBarPreset(QObject* parent = nullptr);
    static ItemGroup* createPbsnrBarPreset(QObject* parent = nullptr);
    static ItemGroup* createEqBarPreset(QObject* parent = nullptr);
    static ItemGroup* createLevelerBarPreset(QObject* parent = nullptr);
    static ItemGroup* createLevelerGainBarPreset(QObject* parent = nullptr);
    static ItemGroup* createAlcGainBarPreset(QObject* parent = nullptr);
    static ItemGroup* createAlcGroupBarPreset(QObject* parent = nullptr);
    static ItemGroup* createCfcBarPreset(QObject* parent = nullptr);
    static ItemGroup* createCfcGainBarPreset(QObject* parent = nullptr);
    static ItemGroup* createCustomBarPreset(int bindingId, double minVal, double maxVal,
                                             const QString& name, QObject* parent = nullptr);

    // --- Phase 3G-4 composite presets ---
    // ANAN Multi-Meter: 7-needle composite with background images
    // From Thetis AddAnanMM() (MeterManager.cs:22461-22815)
    static ItemGroup* createAnanMMPreset(QObject* parent = nullptr);

    // Cross-Needle: dual crossing needles (fwd/rev power)
    // From Thetis AddCrossNeedle() (MeterManager.cs:22817-23002)
    static ItemGroup* createCrossNeedlePreset(QObject* parent = nullptr);

    static ItemGroup* createMagicEyePreset(int bindingId, QObject* parent = nullptr);
    static ItemGroup* createSignalTextPreset(int bindingId, QObject* parent = nullptr);
    static ItemGroup* createHistoryPreset(int bindingId, QObject* parent = nullptr);
    static ItemGroup* createSpacerPreset(QObject* parent = nullptr);

    // Install all items from this group into a MeterWidget, transforming
    // their normalized 0-1 positions into the given target rect.
    // Transfers item ownership to widget; clears this group's item list.
    void installInto(MeterWidget* widget, float gx, float gy, float gw, float gh);

private:
    QString m_name;
    float m_x{0.0f};
    float m_y{0.0f};
    float m_w{1.0f};
    float m_h{1.0f};
    QVector<MeterItem*> m_items;
};

} // namespace NereusSDR
