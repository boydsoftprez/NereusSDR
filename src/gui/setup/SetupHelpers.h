#pragma once

// Setup page factory helpers — produce prewired slider+spinbox rows so every
// numeric control in the Setup dialog has a live readout and supports both
// mouse and keyboard entry without copy-pasted boilerplate.
//
// Bidirectional sync uses QSignalBlocker to prevent signal feedback loops.

#include <QString>

class QWidget;
class QSlider;
class QSpinBox;
class QDoubleSpinBox;

namespace NereusSDR {

// Integer slider + QSpinBox pair.
struct SliderRow {
    QSlider*  slider    = nullptr;
    QSpinBox* spin      = nullptr;
    QWidget*  container = nullptr;  // HBox with slider + spin, ready to drop into a form layout
};

// Double slider + QDoubleSpinBox pair. Slider operates in integer units
// scaled by 10^decimals under the hood.
struct SliderRowD {
    QSlider*        slider    = nullptr;
    QDoubleSpinBox* spin      = nullptr;
    QWidget*        container = nullptr;  // HBox with slider + spin, ready to drop into a form layout
};

// Build an int slider+spinbox row. The slider and spinbox are wired
// bidirectionally via QSignalBlocker so dragging the slider updates the
// spinbox value without retriggering the slider's valueChanged signal,
// and typing into the spinbox updates the slider without retriggering
// the spinbox's valueChanged signal. Consumers connect to slider->valueChanged
// (one source of truth) to drive model updates.
//
// Arguments:
//   min, max, initial — range and start value
//   suffix            — unit string appended in the spinbox (" dBm", "%", etc.)
//   parent            — ownership of the returned container widget
SliderRow makeSliderRow(int min, int max, int initial,
                        const QString& suffix = QString(),
                        QWidget* parent = nullptr);

// Double variant. decimals controls the spinbox's decimal places and the
// slider's internal integer granularity (step = 10^-decimals in user space).
SliderRowD makeDoubleSliderRow(double min, double max, double initial,
                               int decimals,
                               const QString& suffix = QString(),
                               QWidget* parent = nullptr);

} // namespace NereusSDR
