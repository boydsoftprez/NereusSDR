#pragma once
#include <QWidget>
namespace NereusSDR {
class VfoLevelBar : public QWidget {
    Q_OBJECT
public:
    explicit VfoLevelBar(QWidget* parent = nullptr);
    void setValue(float dbm);   // slot-safe; schedules update()
    float value() const { return m_value; }
    double fillFraction() const;  // 0..1, clamped; 0 with no reading
    bool isAboveS9() const { return !m_noReading && m_value >= kS9Dbm; }
    // No reading (R-R3-13): a level at or below kNoReadingDbm, or a
    // non-finite one.  The bar is empty and the readout shows "-- dBm".
    bool hasReading() const { return !m_noReading; }
    // The dBm readout drawn right of the bar ("-65 dBm", or "-- dBm").
    QString readoutText() const;
    // Which reading this is, off the default (overlay design D41): "MAX" or
    // "AV", printed above the readout in the tick strip. Empty prints nothing.
    // Only this bar repaints, and nothing it already draws moves.
    void setSourceTag(const QString& tag);
    QString sourceTag() const { return m_sourceTag; }
    // Where the tag and the readout are drawn.
    QRect sourceTagRect() const;
    QRect readoutRect() const;
    // Same sentinel MeterPoller feeds and SMeterWidget treats as no reading.
    static constexpr float kNoReadingDbm = -400.0f;
    // 2026-05-12 (PR #238): +4 px height for the kTopPad padding
    // shoved into paintEvent so the tick strip doesn't sit flush
    // against the frequency-display border above this widget.
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
protected:
    void paintEvent(QPaintEvent*) override;
private:
    struct Layout {
        int tickH;
        int dbmWidth;
        int barW;
        int barTop;
    };
    Layout barLayout() const;

    QString m_sourceTag;
    float m_value{-130.0f};
    bool  m_noReading{false};
    static constexpr float kFloorDbm   = -130.0f;
    static constexpr float kCeilingDbm =  -20.0f;
    static constexpr float kS9Dbm      =  -73.0f;
};
}
