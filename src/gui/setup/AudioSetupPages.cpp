#include "AudioSetupPages.h"

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Helper: populate a VAC page (identical layout for VAC 1 and VAC 2)
// ---------------------------------------------------------------------------
static void buildVacPage(SetupPage* page, const QString& sectionName)
{
    page->addSection(sectionName);

    auto* enable = page->addLabeledToggle(QStringLiteral("Enable VAC"));
    SetupPage::markNyi(enable, QStringLiteral("Phase 3M"));

    auto* device = page->addLabeledCombo(QStringLiteral("Device"),
        {QStringLiteral("(none)")});
    SetupPage::markNyi(device, QStringLiteral("Phase 3M"));

    auto* sampleRate = page->addLabeledCombo(QStringLiteral("Sample Rate"),
        {QStringLiteral("44100"), QStringLiteral("48000"),
         QStringLiteral("96000"), QStringLiteral("192000")});
    SetupPage::markNyi(sampleRate, QStringLiteral("Phase 3M"));

    auto* stereo = page->addLabeledToggle(QStringLiteral("Stereo"));
    SetupPage::markNyi(stereo, QStringLiteral("Phase 3M"));

    auto* rxGain = page->addLabeledSlider(QStringLiteral("RX Gain"), 0, 100, 50);
    SetupPage::markNyi(rxGain, QStringLiteral("Phase 3M"));

    auto* txGain = page->addLabeledSlider(QStringLiteral("TX Gain"), 0, 100, 50);
    SetupPage::markNyi(txGain, QStringLiteral("Phase 3M"));

    auto* bypass = page->addLabeledToggle(QStringLiteral("Bypass (monitor mode)"));
    SetupPage::markNyi(bypass, QStringLiteral("Phase 3M"));
}

// ---------------------------------------------------------------------------
// DeviceSelectionPage
// ---------------------------------------------------------------------------

DeviceSelectionPage::DeviceSelectionPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Device Selection"), model, parent)
{
    addSection(QStringLiteral("Output"));

    auto* outDevice = addLabeledCombo(QStringLiteral("Output Device"),
        {QStringLiteral("Default")});
    markNyi(outDevice, QStringLiteral("Phase 3A"));

    auto* outRate = addLabeledCombo(QStringLiteral("Sample Rate"),
        {QStringLiteral("44100"), QStringLiteral("48000"),
         QStringLiteral("96000")});
    markNyi(outRate, QStringLiteral("Phase 3A"));

    addSection(QStringLiteral("Input"));

    auto* inDevice = addLabeledCombo(QStringLiteral("Input Device"),
        {QStringLiteral("Default")});
    markNyi(inDevice, QStringLiteral("Phase 3I-1"));

    auto* bufSize = addLabeledCombo(QStringLiteral("Buffer Size (frames)"),
        {QStringLiteral("64"), QStringLiteral("128"),
         QStringLiteral("256"), QStringLiteral("512"),
         QStringLiteral("1024")});
    markNyi(bufSize, QStringLiteral("Phase 3I-1"));
}

// ---------------------------------------------------------------------------
// AsioConfigPage
// ---------------------------------------------------------------------------

AsioConfigPage::AsioConfigPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("ASIO Configuration"), model, parent)
{
    addSection(QStringLiteral("ASIO"));

    auto* device = addLabeledCombo(QStringLiteral("ASIO Device"),
        {QStringLiteral("(none)")});
    markNyi(device, QStringLiteral("Phase 3I-1"));

    auto* channel = addLabeledCombo(QStringLiteral("Channel"),
        {QStringLiteral("1/2"), QStringLiteral("3/4"),
         QStringLiteral("5/6"), QStringLiteral("7/8")});
    markNyi(channel, QStringLiteral("Phase 3I-1"));

    auto* bufSize = addLabeledSpinner(QStringLiteral("Buffer Size (samples)"),
                                      64, 4096, 256);
    markNyi(bufSize, QStringLiteral("Phase 3I-1"));

    auto* lockMode = addLabeledToggle(QStringLiteral("Lock sample rate to ASIO clock"));
    markNyi(lockMode, QStringLiteral("Phase 3I-1"));

    auto* ctrlPanel = addLabeledButton(QStringLiteral("ASIO Control Panel"),
                                       QStringLiteral("Open"));
    markNyi(ctrlPanel, QStringLiteral("Phase 3I-1"));
}

// ---------------------------------------------------------------------------
// Vac1Page
// ---------------------------------------------------------------------------

Vac1Page::Vac1Page(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("VAC 1"), model, parent)
{
    buildVacPage(this, QStringLiteral("VAC 1"));
}

// ---------------------------------------------------------------------------
// Vac2Page
// ---------------------------------------------------------------------------

Vac2Page::Vac2Page(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("VAC 2"), model, parent)
{
    buildVacPage(this, QStringLiteral("VAC 2"));
}

// ---------------------------------------------------------------------------
// NereusVaxPage
// ---------------------------------------------------------------------------

NereusVaxPage::NereusVaxPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("NereusVAX"), model, parent)
{
    addSection(QStringLiteral("VAX Channels"));

    // 4 audio channels — each shows channel label, status, and device combo
    const QStringList channelNames = {
        QStringLiteral("VAX Audio 1"),
        QStringLiteral("VAX Audio 2"),
        QStringLiteral("VAX Audio 3"),
        QStringLiteral("VAX Audio 4"),
    };

    for (const QString& ch : channelNames) {
        auto* statusLbl = addLabeledLabel(ch, QStringLiteral("Inactive"));
        markNyi(statusLbl, QStringLiteral("Phase 3J"));

        auto* devCombo = addLabeledCombo(
            QStringLiteral("  Device"), {QStringLiteral("(none)")});
        markNyi(devCombo, QStringLiteral("Phase 3J"));
    }

    addSection(QStringLiteral("IQ"));

    auto* iq1 = addLabeledCombo(QStringLiteral("IQ Channel 1"),
        {QStringLiteral("(none)")});
    markNyi(iq1, QStringLiteral("Phase 3J"));

    auto* iq2 = addLabeledCombo(QStringLiteral("IQ Channel 2"),
        {QStringLiteral("(none)")});
    markNyi(iq2, QStringLiteral("Phase 3J"));
}

// ---------------------------------------------------------------------------
// RecordingPage
// ---------------------------------------------------------------------------

RecordingPage::RecordingPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Recording"), model, parent)
{
    addSection(QStringLiteral("Output"));

    auto* dirLabel = addLabeledLabel(QStringLiteral("Output Directory"),
                                      QStringLiteral("~/NereusSDR/Recordings"));
    markNyi(dirLabel, QStringLiteral("Phase 3M"));

    auto* browseBtn = addLabeledButton(QStringLiteral("Directory"),
                                       QStringLiteral("Browse..."));
    markNyi(browseBtn, QStringLiteral("Phase 3M"));

    auto* format = addLabeledCombo(QStringLiteral("File Format"),
        {QStringLiteral("WAV"), QStringLiteral("FLAC")});
    markNyi(format, QStringLiteral("Phase 3M"));

    addSection(QStringLiteral("Options"));

    auto* autoSplit = addLabeledToggle(QStringLiteral("Auto-split at file size limit"));
    markNyi(autoSplit, QStringLiteral("Phase 3M"));

    auto* maxSize = addLabeledSpinner(QStringLiteral("Max File Size (MB)"),
                                      10, 4096, 500);
    markNyi(maxSize, QStringLiteral("Phase 3M"));
}

} // namespace NereusSDR
