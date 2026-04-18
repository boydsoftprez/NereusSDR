#pragma once

// =================================================================
// src/gui/widgets/ScrollableLabel.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Qt6 pattern informed by AetherSDR
//                 `src/gui/GuardedSlider.h:81-100`.
// =================================================================

// NereusSDR native widget; Qt skeleton patterns informed by AetherSDR's
// `ScrollableLabel` in `~/AetherSDR/src/gui/GuardedSlider.h:81-100`.
// AetherSDR's version is a ~20-line QLabel that emits scrolled(±1) on wheel.
// This version is a richer composite per user-approved source-first exception
// for control surfaces. See feedback_source_first_ui_vs_dsp.md for the rule.

#include <QStackedWidget>
#include <functional>
#include <optional>

class QLabel;
class QLineEdit;

namespace NereusSDR {

// ScrollableLabel — editable numeric display for RIT/XIT/DIG-offset/FM-offset.
// Slot 0: QLabel showing formatted value. Slot 1: QLineEdit for inline edit.
// Double-click → edit mode; Enter or focus-loss → commit; Esc → cancel.
// Wheel up = +step, wheel down = −step, clamped to [m_min, m_max].
class ScrollableLabel : public QStackedWidget {
    Q_OBJECT

public:
    explicit ScrollableLabel(QWidget* parent = nullptr);

    // Range (inclusive). Default: [0, 0]. Call before setValue.
    void setRange(int minVal, int maxVal);

    // Single-wheel-tick delta. Default: 1.
    void setStep(int step);

    // Set value; clamps to range; emits valueChanged if changed.
    void setValue(int v);

    int value() const { return m_value; }

    // Custom display formatter. Default: QString::number(v).
    void setFormat(std::function<QString(int)> fmt);

    // Parses "+120", "-3500", plain integers; returns empty on non-numeric.
    // Used internally by the inline-edit commit path and exposed for tests.
    static std::optional<int> parseValue(const QString& text);

    QSize sizeHint() const override { return {80, 22}; }
    QSize minimumSizeHint() const override { return {70, 22}; }

signals:
    void valueChanged(int newValue);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void wheelEvent(QWheelEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private slots:
    void commitEdit();
    void cancelEdit();

private:
    void updateLabel();
    void enterEditMode();

    QLabel*    m_label{nullptr};
    QLineEdit* m_edit{nullptr};

    int m_min{0};
    int m_max{0};
    int m_step{1};
    int m_value{0};

    std::function<QString(int)> m_formatter;

    static constexpr int kLabelPage = 0;
    static constexpr int kEditPage  = 1;
};

} // namespace NereusSDR
