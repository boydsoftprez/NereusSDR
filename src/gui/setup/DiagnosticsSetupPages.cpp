#include "DiagnosticsSetupPages.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// DiagSignalGeneratorPage
// ---------------------------------------------------------------------------

DiagSignalGeneratorPage::DiagSignalGeneratorPage(QWidget* parent)
    : SetupPage(QStringLiteral("Signal Generator"), parent)
{
    buildUI();
}

void DiagSignalGeneratorPage::buildUI()
{
    setStyleSheet(QString::fromLatin1(Style::kPageStyle));

    // --- Tone section ---
    {
        auto* group = new QGroupBox(QStringLiteral("Tone"), this);
        group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));

        auto* grid = new QGridLayout(group);
        grid->setSpacing(6);

        auto* freqLabel = new QLabel(QStringLiteral("Frequency (Hz):"), group);
        freqLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
        grid->addWidget(freqLabel, 0, 0);

        m_toneFreqSpin = new QSpinBox(group);
        m_toneFreqSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
        m_toneFreqSpin->setRange(100, 20000);
        m_toneFreqSpin->setValue(1000);
        m_toneFreqSpin->setSuffix(QStringLiteral(" Hz"));
        m_toneFreqSpin->setDisabled(true);
        m_toneFreqSpin->setToolTip(QStringLiteral("NYI — tone generator frequency (100-20000 Hz)"));
        grid->addWidget(m_toneFreqSpin, 0, 1);

        auto* ampLabel = new QLabel(QStringLiteral("Amplitude (dBFS):"), group);
        ampLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
        grid->addWidget(ampLabel, 1, 0);

        m_toneAmpSlider = new QSlider(Qt::Horizontal, group);
        m_toneAmpSlider->setStyleSheet(QString::fromLatin1(Style::kSliderStyle));
        m_toneAmpSlider->setRange(-60, 0);
        m_toneAmpSlider->setValue(-20);
        m_toneAmpSlider->setDisabled(true);
        m_toneAmpSlider->setToolTip(QStringLiteral("NYI — tone amplitude (-60 to 0 dBFS)"));
        grid->addWidget(m_toneAmpSlider, 1, 1);

        m_toneEnableCheck = new QCheckBox(QStringLiteral("Enable Tone"), group);
        m_toneEnableCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
        m_toneEnableCheck->setDisabled(true);
        m_toneEnableCheck->setToolTip(QStringLiteral("NYI — enable test tone injection"));
        grid->addWidget(m_toneEnableCheck, 2, 0, 1, 2);

        contentLayout()->addWidget(group);
    }

    // --- Noise section ---
    {
        auto* group = new QGroupBox(QStringLiteral("Noise"), this);
        group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));

        auto* grid = new QGridLayout(group);
        grid->setSpacing(6);

        m_noiseEnableCheck = new QCheckBox(QStringLiteral("Enable Noise"), group);
        m_noiseEnableCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
        m_noiseEnableCheck->setDisabled(true);
        m_noiseEnableCheck->setToolTip(QStringLiteral("NYI — enable noise injection"));
        grid->addWidget(m_noiseEnableCheck, 0, 0, 1, 2);

        auto* levelLabel = new QLabel(QStringLiteral("Level:"), group);
        levelLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
        grid->addWidget(levelLabel, 1, 0);

        m_noiseLevelSlider = new QSlider(Qt::Horizontal, group);
        m_noiseLevelSlider->setStyleSheet(QString::fromLatin1(Style::kSliderStyle));
        m_noiseLevelSlider->setRange(0, 100);
        m_noiseLevelSlider->setValue(50);
        m_noiseLevelSlider->setDisabled(true);
        m_noiseLevelSlider->setToolTip(QStringLiteral("NYI — noise level"));
        grid->addWidget(m_noiseLevelSlider, 1, 1);

        contentLayout()->addWidget(group);
    }

    // --- Sweep section ---
    {
        auto* group = new QGroupBox(QStringLiteral("Sweep"), this);
        group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));

        auto* grid = new QGridLayout(group);
        grid->setSpacing(6);

        m_sweepEnableCheck = new QCheckBox(QStringLiteral("Enable Sweep"), group);
        m_sweepEnableCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
        m_sweepEnableCheck->setDisabled(true);
        m_sweepEnableCheck->setToolTip(QStringLiteral("NYI — enable frequency sweep"));
        grid->addWidget(m_sweepEnableCheck, 0, 0, 1, 2);

        m_sweepRangeLabel = new QLabel(
            QStringLiteral("Sweep range controls will appear here"), group);
        m_sweepRangeLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
        m_sweepRangeLabel->setAlignment(Qt::AlignCenter);
        grid->addWidget(m_sweepRangeLabel, 1, 0, 1, 2);

        contentLayout()->addWidget(group);
    }

    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// DiagHardwareTestsPage
// ---------------------------------------------------------------------------

DiagHardwareTestsPage::DiagHardwareTestsPage(QWidget* parent)
    : SetupPage(QStringLiteral("Hardware Tests"), parent)
{
    buildUI();
}

void DiagHardwareTestsPage::buildUI()
{
    setStyleSheet(QString::fromLatin1(Style::kPageStyle));

    auto* group = new QGroupBox(QStringLiteral("Tests"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));

    auto* vLayout = new QVBoxLayout(group);
    vLayout->setSpacing(6);

    m_adcTestButton = new QPushButton(QStringLiteral("Run ADC Test"), group);
    m_adcTestButton->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_adcTestButton->setDisabled(true);
    m_adcTestButton->setToolTip(QStringLiteral("NYI — ADC hardware self-test"));
    vLayout->addWidget(m_adcTestButton);

    m_resultLabel = new QLabel(QStringLiteral("Test result: —"), group);
    m_resultLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    vLayout->addWidget(m_resultLabel);

    m_ddcTestButton = new QPushButton(QStringLiteral("Run DDC Test"), group);
    m_ddcTestButton->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_ddcTestButton->setDisabled(true);
    m_ddcTestButton->setToolTip(QStringLiteral("NYI — DDC hardware self-test"));
    vLayout->addWidget(m_ddcTestButton);

    m_loopbackButton = new QPushButton(QStringLiteral("Run Loopback Test"), group);
    m_loopbackButton->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_loopbackButton->setDisabled(true);
    m_loopbackButton->setToolTip(QStringLiteral("NYI — TX/RX loopback test"));
    vLayout->addWidget(m_loopbackButton);

    contentLayout()->addWidget(group);
    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// DiagLoggingPage
// ---------------------------------------------------------------------------

DiagLoggingPage::DiagLoggingPage(QWidget* parent)
    : SetupPage(QStringLiteral("Logging"), parent)
{
    buildUI();
}

void DiagLoggingPage::buildUI()
{
    setStyleSheet(QString::fromLatin1(Style::kPageStyle));

    // --- Log section ---
    {
        auto* group = new QGroupBox(QStringLiteral("Log"), this);
        group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));

        auto* grid = new QGridLayout(group);
        grid->setSpacing(6);

        auto* levelLabel = new QLabel(QStringLiteral("Log Level:"), group);
        levelLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
        grid->addWidget(levelLabel, 0, 0);

        m_levelCombo = new QComboBox(group);
        m_levelCombo->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
        m_levelCombo->addItem(QStringLiteral("Debug"));
        m_levelCombo->addItem(QStringLiteral("Info"));
        m_levelCombo->addItem(QStringLiteral("Warning"));
        m_levelCombo->addItem(QStringLiteral("Error"));
        m_levelCombo->setCurrentIndex(1);  // Info default
        m_levelCombo->setDisabled(true);
        m_levelCombo->setToolTip(QStringLiteral("NYI — log level selection"));
        grid->addWidget(m_levelCombo, 0, 1);

        m_filePathLabel = new QLabel(QStringLiteral("Log file: —"), group);
        m_filePathLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
        grid->addWidget(m_filePathLabel, 1, 0, 1, 2);

        auto* btnRow = new QHBoxLayout();
        m_openLogButton = new QPushButton(QStringLiteral("Open Log"), group);
        m_openLogButton->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
        m_openLogButton->setDisabled(true);
        m_openLogButton->setToolTip(QStringLiteral("NYI — open log file in default viewer"));
        btnRow->addWidget(m_openLogButton);

        m_clearLogButton = new QPushButton(QStringLiteral("Clear Log"), group);
        m_clearLogButton->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
        m_clearLogButton->setDisabled(true);
        m_clearLogButton->setToolTip(QStringLiteral("NYI — clear log file"));
        btnRow->addWidget(m_clearLogButton);
        btnRow->addStretch();

        grid->addLayout(btnRow, 2, 0, 1, 2);

        contentLayout()->addWidget(group);
    }

    // --- Categories section ---
    {
        auto* group = new QGroupBox(QStringLiteral("Categories"), this);
        group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));

        auto* vLayout = new QVBoxLayout(group);

        m_filterLabel = new QLabel(QStringLiteral("Category filters"), group);
        m_filterLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
        m_filterLabel->setAlignment(Qt::AlignCenter);
        m_filterLabel->setMinimumHeight(60);
        vLayout->addWidget(m_filterLabel);

        contentLayout()->addWidget(group);
    }

    contentLayout()->addStretch();
}

} // namespace NereusSDR
