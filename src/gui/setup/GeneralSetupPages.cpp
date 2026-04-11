#include "GeneralSetupPages.h"

namespace NereusSDR {

// ---------------------------------------------------------------------------
// StartupPrefsPage
// ---------------------------------------------------------------------------

StartupPrefsPage::StartupPrefsPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Startup & Preferences"), model, parent)
{
    // Section: Startup
    addSection(QStringLiteral("Startup"));

    auto* autoConnect = addLabeledToggle(QStringLiteral("Auto-connect to last radio"));
    markNyi(autoConnect, QStringLiteral("Phase 3A"));

    auto* restoreFreq = addLabeledToggle(QStringLiteral("Restore last frequency on connect"));
    markNyi(restoreFreq, QStringLiteral("Phase 3E"));

    auto* callsign = addLabeledEdit(QStringLiteral("Callsign"),
                                    QStringLiteral("e.g. KG4VCF"));
    markNyi(callsign, QStringLiteral("future"));

    auto* gridSquare = addLabeledEdit(QStringLiteral("Grid Square"),
                                      QStringLiteral("e.g. EM73"));
    markNyi(gridSquare, QStringLiteral("future"));

    // Section: Application
    addSection(QStringLiteral("Application"));

    auto* splash = addLabeledToggle(QStringLiteral("Show splash screen at startup"));
    markNyi(splash, QStringLiteral("Phase 3N"));

    auto* checkUpdates = addLabeledToggle(QStringLiteral("Check for updates on startup"));
    markNyi(checkUpdates, QStringLiteral("Phase 3N"));

    auto* regenWisdom = addLabeledButton(QStringLiteral("FFTW Wisdom"),
                                         QStringLiteral("Regenerate"));
    markNyi(regenWisdom, QStringLiteral("Phase 3C"));

    auto* priority = addLabeledCombo(QStringLiteral("Process Priority"),
        {QStringLiteral("Normal"), QStringLiteral("Above Normal"),
         QStringLiteral("High"), QStringLiteral("Realtime")});
    markNyi(priority, QStringLiteral("future"));
}

// ---------------------------------------------------------------------------
// UiScalePage
// ---------------------------------------------------------------------------

UiScalePage::UiScalePage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("UI Scale & Theme"), model, parent)
{
    // Section: Scale
    addSection(QStringLiteral("Scale"));

    auto* scale = addLabeledCombo(QStringLiteral("UI Scale"),
        {QStringLiteral("100%"), QStringLiteral("125%"),
         QStringLiteral("150%"), QStringLiteral("175%"),
         QStringLiteral("200%")});
    markNyi(scale, QStringLiteral("Phase 3H"));

    // Section: Theme
    addSection(QStringLiteral("Theme"));

    auto* darkLight = addLabeledToggle(QStringLiteral("Dark mode"));
    markNyi(darkLight, QStringLiteral("Phase 3H"));

    auto* fontSize = addLabeledCombo(QStringLiteral("Font Size"),
        {QStringLiteral("Small"), QStringLiteral("Medium"),
         QStringLiteral("Large")});
    markNyi(fontSize, QStringLiteral("Phase 3H"));
}

// ---------------------------------------------------------------------------
// NavigationPage
// ---------------------------------------------------------------------------

NavigationPage::NavigationPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Navigation"), model, parent)
{
    // Section: Mouse
    addSection(QStringLiteral("Mouse"));

    auto* wheelTune = addLabeledToggle(QStringLiteral("Mouse wheel tunes VFO"));
    markNyi(wheelTune, QStringLiteral("Phase 3E"));

    auto* clickTune = addLabeledToggle(QStringLiteral("Click-to-tune on panadapter"));
    markNyi(clickTune, QStringLiteral("Phase 3E"));

    auto* scrollZoom = addLabeledToggle(QStringLiteral("Scroll zoom on panadapter"));
    markNyi(scrollZoom, QStringLiteral("Phase 3E"));

    auto* dblClick = addLabeledCombo(QStringLiteral("Double-click action"),
        {QStringLiteral("Tune"), QStringLiteral("Center"),
         QStringLiteral("None")});
    markNyi(dblClick, QStringLiteral("Phase 3E"));

    // Section: Tuning
    addSection(QStringLiteral("Tuning"));

    auto* snapTune = addLabeledToggle(QStringLiteral("Snap click-tune to step"));
    markNyi(snapTune, QStringLiteral("Phase 3E"));

    auto* wheelOutside = addLabeledToggle(QStringLiteral("Wheel tunes outside spectral display"));
    markNyi(wheelOutside, QStringLiteral("Phase 3E"));

    auto* wheelReverse = addLabeledToggle(QStringLiteral("Reverse wheel direction"));
    markNyi(wheelReverse, QStringLiteral("Phase 3E"));
}

} // namespace NereusSDR
