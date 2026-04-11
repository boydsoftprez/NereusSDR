#pragma once

#include "ButtonBoxItem.h"

namespace NereusSDR {

// Tuning step buttons: 1Hz, 10Hz, 100Hz, 1kHz, 10kHz, 100kHz, 1MHz.
// Ported from Thetis clsTunestepButtons (MeterManager.cs:7999+).
class TuneStepButtonItem : public ButtonBoxItem {
    Q_OBJECT

public:
    explicit TuneStepButtonItem(QObject* parent = nullptr);

    void setActiveStep(int index);
    int activeStep() const { return m_activeStep; }

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    QString serialize() const override;
    bool deserialize(const QString& data) override;

signals:
    void tuneStepSelected(int stepIndex);

private:
    void onButtonClicked(int index, Qt::MouseButton button);
    int m_activeStep{-1};
    static constexpr int kStepCount = 7;
};

} // namespace NereusSDR
