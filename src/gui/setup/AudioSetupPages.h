#pragma once

#include "gui/SetupPage.h"

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Audio > Device Selection
// ---------------------------------------------------------------------------
class DeviceSelectionPage : public SetupPage {
    Q_OBJECT
public:
    explicit DeviceSelectionPage(RadioModel* model, QWidget* parent = nullptr);
};

// ---------------------------------------------------------------------------
// Audio > ASIO Configuration
// ---------------------------------------------------------------------------
class AsioConfigPage : public SetupPage {
    Q_OBJECT
public:
    explicit AsioConfigPage(RadioModel* model, QWidget* parent = nullptr);
};

// ---------------------------------------------------------------------------
// Audio > VAC 1
// ---------------------------------------------------------------------------
class Vac1Page : public SetupPage {
    Q_OBJECT
public:
    explicit Vac1Page(RadioModel* model, QWidget* parent = nullptr);
};

// ---------------------------------------------------------------------------
// Audio > VAC 2
// ---------------------------------------------------------------------------
class Vac2Page : public SetupPage {
    Q_OBJECT
public:
    explicit Vac2Page(RadioModel* model, QWidget* parent = nullptr);
};

// ---------------------------------------------------------------------------
// Audio > NereusVAX
// ---------------------------------------------------------------------------
class NereusVaxPage : public SetupPage {
    Q_OBJECT
public:
    explicit NereusVaxPage(RadioModel* model, QWidget* parent = nullptr);
};

// ---------------------------------------------------------------------------
// Audio > Recording
// ---------------------------------------------------------------------------
class RecordingPage : public SetupPage {
    Q_OBJECT
public:
    explicit RecordingPage(RadioModel* model, QWidget* parent = nullptr);
};

} // namespace NereusSDR
