#pragma once

#include "gui/SetupPage.h"

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Hardware > Radio Selection
// ---------------------------------------------------------------------------
class RadioSelectionPage : public SetupPage {
    Q_OBJECT
public:
    explicit RadioSelectionPage(RadioModel* model, QWidget* parent = nullptr);
};

// ---------------------------------------------------------------------------
// Hardware > ADC / DDC Configuration
// ---------------------------------------------------------------------------
class AdcDdcPage : public SetupPage {
    Q_OBJECT
public:
    explicit AdcDdcPage(RadioModel* model, QWidget* parent = nullptr);
};

// ---------------------------------------------------------------------------
// Hardware > Calibration
// ---------------------------------------------------------------------------
class CalibrationPage : public SetupPage {
    Q_OBJECT
public:
    explicit CalibrationPage(RadioModel* model, QWidget* parent = nullptr);
};

// ---------------------------------------------------------------------------
// Hardware > Alex Filters
// ---------------------------------------------------------------------------
class AlexFiltersPage : public SetupPage {
    Q_OBJECT
public:
    explicit AlexFiltersPage(RadioModel* model, QWidget* parent = nullptr);
};

// ---------------------------------------------------------------------------
// Hardware > Penny / Hermes OC
// ---------------------------------------------------------------------------
class PennyHermesPage : public SetupPage {
    Q_OBJECT
public:
    explicit PennyHermesPage(RadioModel* model, QWidget* parent = nullptr);
};

// ---------------------------------------------------------------------------
// Hardware > Firmware
// ---------------------------------------------------------------------------
class FirmwarePage : public SetupPage {
    Q_OBJECT
public:
    explicit FirmwarePage(RadioModel* model, QWidget* parent = nullptr);
};

// ---------------------------------------------------------------------------
// Hardware > Other H/W
// ---------------------------------------------------------------------------
class OtherHwPage : public SetupPage {
    Q_OBJECT
public:
    explicit OtherHwPage(RadioModel* model, QWidget* parent = nullptr);
};

} // namespace NereusSDR
