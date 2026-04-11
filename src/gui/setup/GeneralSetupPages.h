#pragma once

#include "gui/SetupPage.h"

namespace NereusSDR {

// ---------------------------------------------------------------------------
// General > Startup & Preferences
// ---------------------------------------------------------------------------
class StartupPrefsPage : public SetupPage {
    Q_OBJECT
public:
    explicit StartupPrefsPage(RadioModel* model, QWidget* parent = nullptr);
};

// ---------------------------------------------------------------------------
// General > UI Scale & Theme
// ---------------------------------------------------------------------------
class UiScalePage : public SetupPage {
    Q_OBJECT
public:
    explicit UiScalePage(RadioModel* model, QWidget* parent = nullptr);
};

// ---------------------------------------------------------------------------
// General > Navigation
// ---------------------------------------------------------------------------
class NavigationPage : public SetupPage {
    Q_OBJECT
public:
    explicit NavigationPage(RadioModel* model, QWidget* parent = nullptr);
};

} // namespace NereusSDR
