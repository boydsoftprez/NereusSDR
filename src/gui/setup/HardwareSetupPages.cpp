#include "HardwareSetupPages.h"

namespace NereusSDR {

// ---------------------------------------------------------------------------
// RadioSelectionPage
// ---------------------------------------------------------------------------

RadioSelectionPage::RadioSelectionPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Radio Selection"), model, parent)
{
    addSection(QStringLiteral("Discovery"));

    // Placeholder for the radio list (will be a QListWidget in a future phase)
    auto* radioListPlaceholder = addLabeledLabel(QStringLiteral("Detected radios"),
                                                  QStringLiteral("No radios found"));
    markNyi(radioListPlaceholder, QStringLiteral("Phase 3A"));

    auto* protocol = addLabeledCombo(QStringLiteral("Protocol"),
        {QStringLiteral("Auto"), QStringLiteral("P1"), QStringLiteral("P2")});
    markNyi(protocol, QStringLiteral("Phase 3A"));

    auto* mac = addLabeledEdit(QStringLiteral("Preferred MAC"),
                               QStringLiteral("e.g. 00:1C:C0:xx:xx:xx"));
    markNyi(mac, QStringLiteral("Phase 3A"));

    auto* autoDiscover = addLabeledToggle(QStringLiteral("Auto-discovery on startup"));
    markNyi(autoDiscover, QStringLiteral("Phase 3A"));

    auto* discoverBtn = addLabeledButton(QStringLiteral("Discover"),
                                         QStringLiteral("Discover Now"));
    markNyi(discoverBtn, QStringLiteral("Phase 3A"));

    auto* refreshBtn = addLabeledButton(QStringLiteral("Refresh"),
                                        QStringLiteral("Refresh List"));
    markNyi(refreshBtn, QStringLiteral("Phase 3A"));
}

// ---------------------------------------------------------------------------
// AdcDdcPage
// ---------------------------------------------------------------------------

AdcDdcPage::AdcDdcPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("ADC / DDC Configuration"), model, parent)
{
    addSection(QStringLiteral("ADC"));

    auto* attenuation = addLabeledCombo(QStringLiteral("Attenuation"),
        {QStringLiteral("0 dB"), QStringLiteral("-10 dB"),
         QStringLiteral("-20 dB"), QStringLiteral("-30 dB")});
    markNyi(attenuation, QStringLiteral("Phase 3A"));

    addSection(QStringLiteral("DDC"));

    auto* ddcAssign = addLabeledCombo(QStringLiteral("DDC Assignment (RX1)"),
        {QStringLiteral("DDC 0"), QStringLiteral("DDC 1"),
         QStringLiteral("DDC 2"), QStringLiteral("DDC 3")});
    markNyi(ddcAssign, QStringLiteral("Phase 3F"));

    auto* sampleRateLabel = addLabeledLabel(QStringLiteral("Sample Rate"),
                                             QStringLiteral("192 kHz"));
    markNyi(sampleRateLabel, QStringLiteral("Phase 3F"));
}

// ---------------------------------------------------------------------------
// CalibrationPage
// ---------------------------------------------------------------------------

CalibrationPage::CalibrationPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Calibration"), model, parent)
{
    addSection(QStringLiteral("Frequency"));

    auto* calFreq1 = addLabeledSpinner(QStringLiteral("Cal Frequency 1 (kHz)"),
                                       0, 30000, 10000);
    markNyi(calFreq1, QStringLiteral("Phase 3I-4"));

    auto* calFreq2 = addLabeledSpinner(QStringLiteral("Cal Frequency 2 (kHz)"),
                                       0, 30000, 14000);
    markNyi(calFreq2, QStringLiteral("Phase 3I-4"));

    auto* level = addLabeledSpinner(QStringLiteral("Reference Level (dBm)"),
                                    -120, 0, -73);
    markNyi(level, QStringLiteral("Phase 3I-4"));

    auto* startBtn = addLabeledButton(QStringLiteral("Calibration"),
                                      QStringLiteral("Start"));
    markNyi(startBtn, QStringLiteral("Phase 3I-4"));

    addSection(QStringLiteral("Correction"));

    auto* freqCorr = addLabeledSpinner(QStringLiteral("Frequency Correction (ppb)"),
                                       -10000, 10000, 0);
    markNyi(freqCorr, QStringLiteral("Phase 3A"));

    auto* resetBtn = addLabeledButton(QStringLiteral("Correction"),
                                      QStringLiteral("Reset to Zero"));
    markNyi(resetBtn, QStringLiteral("Phase 3A"));
}

// ---------------------------------------------------------------------------
// AlexFiltersPage
// ---------------------------------------------------------------------------

AlexFiltersPage::AlexFiltersPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Alex Filters"), model, parent)
{
    addSection(QStringLiteral("Filters"));

    auto* hpfEnable = addLabeledToggle(QStringLiteral("HPF Enable"));
    markNyi(hpfEnable, QStringLiteral("Phase 3A"));

    auto* lpfEnable = addLabeledToggle(QStringLiteral("LPF Enable"));
    markNyi(lpfEnable, QStringLiteral("Phase 3A"));

    auto* bypass = addLabeledToggle(QStringLiteral("Bypass All Filters"));
    markNyi(bypass, QStringLiteral("Phase 3A"));

    // Full per-band filter matrix is NYI
    auto* matrixNote = addLabeledLabel(QStringLiteral("Per-band filter matrix"),
                                        QStringLiteral("(full matrix NYI)"));
    markNyi(matrixNote, QStringLiteral("Phase 3A"));
}

// ---------------------------------------------------------------------------
// PennyHermesPage
// ---------------------------------------------------------------------------

PennyHermesPage::PennyHermesPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Penny / Hermes OC"), model, parent)
{
    addSection(QStringLiteral("Open Collector"));

    auto* assignNote = addLabeledLabel(QStringLiteral("OC Pin Assignment"),
                                        QStringLiteral("(full matrix NYI)"));
    markNyi(assignNote, QStringLiteral("Phase 3A"));

    auto* hotSwitch = addLabeledToggle(QStringLiteral("Allow hot-switching OC outputs"));
    markNyi(hotSwitch, QStringLiteral("Phase 3A"));
}

// ---------------------------------------------------------------------------
// FirmwarePage
// ---------------------------------------------------------------------------

FirmwarePage::FirmwarePage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Firmware"), model, parent)
{
    addSection(QStringLiteral("Current"));

    auto* version = addLabeledLabel(QStringLiteral("Firmware Version"),
                                     QStringLiteral("—"));
    markNyi(version, QStringLiteral("Phase 3A"));

    auto* boardId = addLabeledLabel(QStringLiteral("Board ID"),
                                     QStringLiteral("—"));
    markNyi(boardId, QStringLiteral("Phase 3A"));

    addSection(QStringLiteral("Update"));

    auto* checkBtn = addLabeledButton(QStringLiteral("Check"),
                                      QStringLiteral("Check for Update"));
    markNyi(checkBtn, QStringLiteral("Phase 3N"));

    auto* flashBtn = addLabeledButton(QStringLiteral("Flash"),
                                      QStringLiteral("Flash Firmware"));
    markNyi(flashBtn, QStringLiteral("Phase 3N"));

    // Progress bar (spans full width, placed below the labeled rows)
    // We add it manually rather than through a helper.
    // Re-use addLabeledLabel as a label + then add the bar.
    auto* progressLabel = addLabeledLabel(QStringLiteral("Progress"), {});
    markNyi(progressLabel, QStringLiteral("Phase 3N"));
}

// ---------------------------------------------------------------------------
// OtherHwPage
// ---------------------------------------------------------------------------

OtherHwPage::OtherHwPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Other H/W"), model, parent)
{
    addSection(QStringLiteral("External"));

    auto* temp = addLabeledLabel(QStringLiteral("PA Temperature"), QStringLiteral("—"));
    markNyi(temp, QStringLiteral("Phase 3A"));

    auto* serial = addLabeledLabel(QStringLiteral("Serial Number"), QStringLiteral("—"));
    markNyi(serial, QStringLiteral("Phase 3A"));

    auto* boardType = addLabeledLabel(QStringLiteral("Board Type"), QStringLiteral("—"));
    markNyi(boardType, QStringLiteral("Phase 3A"));
}

} // namespace NereusSDR
