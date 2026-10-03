#pragma once

#include "gui/SetupPage.h"

class QLineEdit;

namespace NereusSDR {

// ---------------------------------------------------------------------------
// General > Startup & Preferences
// ---------------------------------------------------------------------------
class StartupPrefsPage : public SetupPage {
    Q_OBJECT
public:
    explicit StartupPrefsPage(RadioModel* model, QWidget* parent = nullptr);

    // R-R3-21: callsign and grid are the station's operator identity
    // (User/Callsign, User/GridSquare: Core settings in a remote window),
    // so this is a Mixed page; they follow the Core's availability while
    // auto-connect, this computer's own, stays live.
    void setStationSettingsAvailable(bool available, const QString& reason) override;

private:
    QLineEdit* m_callsignEdit{nullptr};
    QLineEdit* m_gridEdit{nullptr};
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
