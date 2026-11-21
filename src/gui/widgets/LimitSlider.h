#pragma once

// =================================================================
// src/gui/widgets/LimitSlider.h  (NereusSDR)
// =================================================================
//
// Port of Thetis's PrettyTrackBar limit interaction, reduced to the part the
// power rows need:
//
//   Project Files/Source/Console/PrettyTrackBar.cs
//     - right-button press/drag anywhere in the trackbar sets the limit at
//       that position (there is no menu; the drag IS the interaction)
//     - a red bar is painted from the limit to the top of the groove
//     - LimitValue is clamped to the slider's own min/max
//
// Thetis paints the bar in LimitBarColor (red by default).  The drive is
// clamped elsewhere (TransmitModel::setPowerUsingTargetDbm, Thetis
// ConstrainAValue); this widget only sets and shows the limit.
//
// =================================================================

#include <QColor>
#include <QSlider>

#include <functional>

class QMouseEvent;
class QPaintEvent;

namespace NereusSDR {

class LimitSlider : public QSlider
{
    Q_OBJECT
public:
    explicit LimitSlider(Qt::Orientation orientation, QWidget* parent = nullptr);

    /// The limit, in this slider's own value units.
    int limitValue() const noexcept { return m_limitValue; }
    /// Set the limit.  Clamped to minimum()..maximum(), no signal (the drag
    /// handler emits; a programmatic seed must not echo back).
    void setLimitValue(int value);

    bool limitEnabled() const noexcept { return m_limitEnabled; }
    /// Hidden and inert when false (Thetis LimitEnabled; the two-tone test
    /// turns the PWR limit off around its fixed drive source).
    void setLimitEnabled(bool enabled);

    QColor limitBarColor() const { return m_limitBarColor; }
    void setLimitBarColor(const QColor& color);

    /// How the limit reads in the callout ("40 W", "-2.5 dB", ...). The
    /// section supplies it because the units depend on Use Max and the SKU.
    void setLimitTextFormatter(std::function<QString(int)> formatter);
    QString limitText(int value) const;

signals:
    /// A right-drag moved the limit.  Emitted for every step of the drag.
    void limitChanged(int value);
    /// The right-drag began/ended, so the section can show the limit in its
    /// reading cell and restore the drive reading on release.
    void limitDragStarted();
    void limitDragFinished();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    int valueForPosition(const QPoint& pos) const;

    int m_limitValue{100};
    bool m_limitEnabled{true};
    bool m_draggingLimit{false};
    QColor m_limitBarColor{255, 68, 68};  // Style::kRedBorder
    std::function<QString(int)> m_limitTextFormatter;
};

}  // namespace NereusSDR
