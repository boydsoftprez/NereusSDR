#pragma once

#include "ButtonBoxItem.h"

namespace NereusSDR {

// Band selection grid: 160m-6m + GEN + WWV + XVTR.
// Ported from Thetis clsBandButtonBox (MeterManager.cs:11482+).
//
// Button order matches NereusSDR::Band enum (src/models/Band.h): index 0 =
// 160m ... index 11 = GEN, index 12 = WWV, index 13 = XVTR. WWV and XVTR
// were added in Phase 3G-8 (commit 2) to match Thetis's 14-band set and
// to provide a home for the per-band grid storage on PanadapterModel.
// The bandClicked(int) signal carries the same enum index; consumers
// should use Band::bandFromUiIndex() to convert.
class BandButtonItem : public ButtonBoxItem {
    Q_OBJECT

public:
    explicit BandButtonItem(QObject* parent = nullptr);

    void setActiveBand(int index);
    int activeBand() const { return m_activeBand; }

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    QString serialize() const override;
    bool deserialize(const QString& data) override;

signals:
    void bandClicked(int bandIndex);
    // From Thetis PopupBandstack (MeterManager.cs:11896)
    void bandStackRequested(int bandIndex);

private:
    void onButtonClicked(int index, Qt::MouseButton button);
    int m_activeBand{-1};
    static constexpr int kBandCount = 14;
};

} // namespace NereusSDR
