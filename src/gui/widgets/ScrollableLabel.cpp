// =================================================================
// src/gui/widgets/ScrollableLabel.cpp  (NereusSDR)
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

#include "ScrollableLabel.h"
#include "VfoStyles.h"

#include <QLabel>
#include <QLineEdit>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QEvent>
#include <algorithm>

namespace NereusSDR {

ScrollableLabel::ScrollableLabel(QWidget* parent)
    : QStackedWidget(parent)
    , m_label(new QLabel(this))
    , m_edit(new QLineEdit(this))
    , m_formatter([](int v) { return QString::number(v); })
{
    // Slot 0: display label
    addWidget(m_label);
    // Slot 1: inline editor
    addWidget(m_edit);
    setCurrentIndex(kLabelPage);

    m_label->setAlignment(Qt::AlignCenter);
    m_label->setStyleSheet(kLabelStyle.toString());

    m_edit->setAlignment(Qt::AlignCenter);
    // Minimal editor styling — inherits palette, override text color to match label
    m_edit->setStyleSheet(
        "QLineEdit { background: #1a2a3a; border: 1px solid #0090e0; "
        "color: #c8d8e8; font-size: 13px; padding: 0 2px; }");

    // Enter/focus-loss commits. returnPressed fires first, then editingFinished;
    // commitEdit is idempotent (guards on currentIndex == kEditPage) so both
    // firing is harmless. We connect only editingFinished to avoid double handling.
    connect(m_edit, &QLineEdit::editingFinished, this, &ScrollableLabel::commitEdit);

    // Esc handling: QLineEdit::editingFinished does not fire on Esc, so we
    // catch KeyPress via an event filter installed for the widget's lifetime.
    m_edit->installEventFilter(this);

    updateLabel();
}

void ScrollableLabel::setRange(int minVal, int maxVal) {
    m_min = minVal;
    m_max = maxVal;
    // Silently clamp stored value to the new range — setRange is a constraint
    // change, not a value change. Callers wanting a signal should call setValue()
    // after setRange() explicitly.
    const int clamped = std::clamp(m_value, m_min, m_max);
    if (clamped != m_value) {
        m_value = clamped;
        updateLabel();
    }
}

void ScrollableLabel::setStep(int step) {
    m_step = step;
}

void ScrollableLabel::setValue(int v) {
    const int clamped = std::clamp(v, m_min, m_max);
    if (clamped == m_value) {
        return;
    }
    m_value = clamped;
    updateLabel();
    emit valueChanged(m_value);
}

void ScrollableLabel::setFormat(std::function<QString(int)> fmt) {
    m_formatter = std::move(fmt);
    updateLabel();
}

std::optional<int> ScrollableLabel::parseValue(const QString& text) {
    bool ok = false;
    const int v = text.trimmed().toInt(&ok);
    if (!ok) {
        return std::nullopt;
    }
    return v;
}

void ScrollableLabel::wheelEvent(QWheelEvent* event) {
    const int delta = event->angleDelta().y();
    if (delta > 0) {
        setValue(m_value + m_step);
    } else if (delta < 0) {
        setValue(m_value - m_step);
    }
    event->accept();
}

void ScrollableLabel::mouseDoubleClickEvent(QMouseEvent* event) {
    if (currentIndex() == kLabelPage) {
        enterEditMode();
        event->accept();
    } else {
        QStackedWidget::mouseDoubleClickEvent(event);
    }
}

void ScrollableLabel::commitEdit() {
    if (currentIndex() != kEditPage) {
        return;
    }
    const auto parsed = parseValue(m_edit->text());
    if (parsed.has_value()) {
        setValue(parsed.value());
    }
    // Always return to label view regardless of parse success
    setCurrentIndex(kLabelPage);
}

void ScrollableLabel::cancelEdit() {
    setCurrentIndex(kLabelPage);
}

void ScrollableLabel::updateLabel() {
    m_label->setText(m_formatter(m_value));
}

void ScrollableLabel::enterEditMode() {
    // Pre-populate with raw number (no formatter — user types digits)
    m_edit->setText(QString::number(m_value));
    setCurrentIndex(kEditPage);
    m_edit->selectAll();
    m_edit->setFocus();

    // Event filter for Esc is installed once in the constructor for the widget's lifetime.
}

// Override eventFilter to handle Esc key in the line edit
bool ScrollableLabel::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == m_edit && ev->type() == QEvent::KeyPress) {
        const QKeyEvent* ke = static_cast<QKeyEvent*>(ev);
        if (ke->key() == Qt::Key_Escape) {
            cancelEdit();
            return true;
        }
    }
    return QStackedWidget::eventFilter(obj, ev);
}

} // namespace NereusSDR
