#pragma once
#include <QWidget>
namespace NereusSDR {
class VfoLevelBar : public QWidget {
    Q_OBJECT
public:
    explicit VfoLevelBar(QWidget* parent = nullptr);
    void setValue(float dbm);   // slot-safe; schedules update()
    float value() const { return m_value; }
    double fillFraction() const;  // 0..1, clamped
    bool isAboveS9() const { return m_value >= kS9Dbm; }
    // 2026-05-12 (PR #238): +4 px height for the kTopPad padding
    // shoved into paintEvent so the tick strip doesn't sit flush
    // against the frequency-display border above this widget.
    QSize sizeHint() const override { return {230, 26}; }
    QSize minimumSizeHint() const override { return {200, 26}; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    float m_value{-130.0f};
    static constexpr float kFloorDbm   = -130.0f;
    static constexpr float kCeilingDbm =  -20.0f;
    static constexpr float kS9Dbm      =  -73.0f;
};
}
