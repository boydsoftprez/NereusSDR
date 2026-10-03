// =================================================================
// src/gui/VintageModMeterWidget.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original file.
//
// Vintage illuminated modulation meter, in the style of the direct-
// illumination broadcast meters of the 1940s-60s (and of the AMM-SD1
// analog display): cream/amber face with warm glow, black percentage
// arc 0..140 with a red zone above 100, a red VU decibel row beneath
// (dB = 20*log10(pct/100)), a black needle with peak-reading ballistics,
// and a blue peak-hold dot on the arc.
//
// AsymmetryBarWidget is the narrow vertical "Pos / Neg" bar that sits
// between the two meters and shows (positive - negative) peak percent.
//
// Modification history (NereusSDR):
//   2026-09-08 — Created for the AM Mod Monitor applet (Lee, AI-assisted
//                 via Anthropic Claude Code).
// =================================================================
#pragma once

#include <QElapsedTimer>
#include <QWidget>

namespace NereusSDR {

class VintageModMeterWidget : public QWidget {
    Q_OBJECT
public:
    explicit VintageModMeterWidget(QWidget* parent = nullptr);

    void setCaption(const QString& text);       // "POSITIVE PEAKS"
    void setRange(double minPct, double maxPct); // default 0..140
    void setRedZoneStart(double pct);           // default 100
    /// Target reading (percent).  The needle follows with ballistics.
    void setValue(double pct);
    /// Peak-hold marker (percent); < 0 hides the dot.
    void setPeakValue(double pct);
    /// Meter-lamp on/off (dark face when off, as with the carrier absent).
    void setLit(bool lit);

    double value() const noexcept { return m_target; }

    QSize sizeHint() const override { return {150, 100}; }
    QSize minimumSizeHint() const override { return {96, 64}; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    double angleFor(double pct) const;   // degrees, 0 = straight up
    void   advanceNeedle();

    QString m_caption{QStringLiteral("PEAKS")};
    double  m_min{0.0};
    double  m_max{140.0};
    double  m_redStart{100.0};
    double  m_target{0.0};
    double  m_needle{0.0};
    double  m_peak{-1.0};
    bool    m_lit{true};
    QElapsedTimer m_clock;
    qint64  m_lastMs{0};
};

class AsymmetryBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit AsymmetryBarWidget(QWidget* parent = nullptr);
    /// Positive-minus-negative peak percent; clamped to +/- range.
    void setValue(double pctPoints);
    void setRange(double pctPoints);   // default 25
    QSize sizeHint() const override { return {30, 100}; }
    QSize minimumSizeHint() const override { return {22, 60}; }
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    double m_value{0.0};
    double m_range{25.0};
};

} // namespace NereusSDR
