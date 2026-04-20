// =================================================================
// src/gui/widgets/MeterSlider.h  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR source:
//   src/gui/MeterSlider.h
//
// AetherSDR is licensed under the GNU General Public License v3; see
// https://github.com/ten9876/AetherSDR for the contributor list and
// project-level LICENSE. NereusSDR is also GPLv3. AetherSDR source
// files carry no per-file GPL header; attribution is at project level
// per AetherSDR convention.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Ported/adapted in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code. Rewrapped in the NereusSDR
//                 namespace; logic/visuals preserved verbatim from the
//                 AetherSDR source. Dependency of VaxApplet (Phase 3O
//                 Sub-Phase 9, Task 9.2).
// =================================================================

#pragma once

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>
#include <cmath>

namespace NereusSDR {

// Combined horizontal level meter + gain slider.
// Background shows RMS level, draggable thumb controls gain.
class MeterSlider : public QWidget {
    Q_OBJECT

public:
    explicit MeterSlider(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(16);
        setMinimumWidth(60);
        setCursor(Qt::PointingHandCursor);
    }

    float gain() const { return m_gain; }
    float level() const { return m_level; }

    void setGain(float g) {
        g = std::clamp(g, 0.0f, 1.0f);
        if (g != m_gain) {
            m_gain = g;
            update();
        }
    }

    void setLevel(float l) {
        l = std::clamp(l, 0.0f, 1.0f);
        if (l != m_level) {
            m_level = l;
            update();
        }
    }

signals:
    void gainChanged(float gain);  // 0.0–1.0

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const int w = width();
        const int h = height();
        const int margin = 1;
        const int barH = h - 2 * margin;
        const int barW = w - 2 * margin;

        // Background
        p.fillRect(rect(), QColor(0x0a, 0x0a, 0x18));
        p.setPen(QColor(0x1e, 0x2e, 0x3e));
        p.drawRect(rect().adjusted(0, 0, -1, -1));

        // Level meter fill (behind the slider)
        if (m_level > 0.0f) {
            int fillW = static_cast<int>(m_level * barW);
            QColor fillColor = m_level < 0.7f ? QColor(0x00, 0x80, 0xa0, 120)
                             : m_level < 0.9f ? QColor(0xa0, 0xa0, 0x20, 120)
                                              : QColor(0xc0, 0x30, 0x30, 120);
            p.fillRect(margin, margin, fillW, barH, fillColor);
        }

        // Gain thumb position
        int thumbX = margin + static_cast<int>(m_gain * barW);
        thumbX = std::clamp(thumbX, margin, margin + barW);

        // Gain fill (solid, up to thumb)
        if (m_gain > 0.0f) {
            int gainW = static_cast<int>(m_gain * barW);
            p.fillRect(margin, margin, gainW, barH, QColor(0x00, 0xb4, 0xd8, 60));
        }

        // Thumb line
        p.setPen(QPen(QColor(0x00, 0xb4, 0xd8), 2));
        p.drawLine(thumbX, margin, thumbX, margin + barH);

        // Thumb triangle (top)
        QPolygon tri;
        tri << QPoint(thumbX - 3, margin)
            << QPoint(thumbX + 3, margin)
            << QPoint(thumbX, margin + 4);
        p.setBrush(QColor(0x00, 0xb4, 0xd8));
        p.setPen(Qt::NoPen);
        p.drawPolygon(tri);
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            m_dragging = true;
            updateGainFromMouse(e->pos().x());
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (m_dragging) {
            updateGainFromMouse(e->pos().x());
        }
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            m_dragging = false;
        }
    }

private:
    void updateGainFromMouse(int x) {
        float g = static_cast<float>(x - 1) / static_cast<float>(width() - 2);
        g = std::clamp(g, 0.0f, 1.0f);
        if (g != m_gain) {
            m_gain = g;
            emit gainChanged(m_gain);
            update();
        }
    }

    float m_gain{0.5f};
    float m_level{0.0f};
    bool  m_dragging{false};
};

} // namespace NereusSDR
