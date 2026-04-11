// src/gui/widgets/FilterPassbandWidget.h
// Visual filter passband display with drag-to-adjust interaction.
// Ported from AetherSDR src/gui/FilterPassbandWidget.h
#pragma once

#include <QWidget>

namespace NereusSDR {

// Static trapezoid shape with numeric lo/hi/bandwidth labels.
// Horizontal drag shifts the passband, vertical drag adjusts width.
// From AetherSDR FilterPassbandWidget (SmartSDR RX panel filter widget).
class FilterPassbandWidget : public QWidget {
    Q_OBJECT

public:
    explicit FilterPassbandWidget(QWidget* parent = nullptr);

    void setFilter(int lo, int hi);
    void setMode(const QString& mode);

    int filterLo() const { return m_lo; }
    int filterHi() const { return m_hi; }

    QSize minimumSizeHint() const override { return {120, 36}; }
    QSize sizeHint() const override { return {200, 60}; }

signals:
    void filterChanged(int lo, int hi);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;

private:
    int m_lo{100};
    int m_hi{2800};
    QString m_mode{QStringLiteral("USB")};

    enum DragMode { DragNone, DragShift, DragLo, DragHi };
    DragMode m_dragMode{DragNone};
    QPoint m_dragStartPos;
    int m_dragStartLo{0};
    int m_dragStartHi{0};

    // From AetherSDR FilterPassbandWidget — minimum bandwidth 50 Hz
    static constexpr int kMinBw = 50;
};

} // namespace NereusSDR
