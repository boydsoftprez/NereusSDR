#include "DigitalApplet.h"
#include "NyiOverlay.h"

#include <QComboBox>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace NereusSDR {

// --------------------------------------------------------------------------
// Style constants
// --------------------------------------------------------------------------

// Blue active state (Stereo button)
static const char* kBlueActive =
    "QPushButton {"
    "  background: #1a2a3a; color: #c8d8e8;"
    "  border: 1px solid #205070; border-radius: 3px;"
    "  padding: 2px 4px; font-size: 10px; font-weight: bold;"
    "}"
    "QPushButton:hover { background: #203040; }"
    "QPushButton:checked {"
    "  background: #0070c0; color: #ffffff; border: 1px solid #0090e0;"
    "}";

// Green active state (VAC 1, VAC 2 enable)
static const char* kGreenActive =
    "QPushButton {"
    "  background: #1a2a3a; color: #c8d8e8;"
    "  border: 1px solid #205070; border-radius: 3px;"
    "  padding: 2px 4px; font-size: 10px; font-weight: bold;"
    "}"
    "QPushButton:hover { background: #203040; }"
    "QPushButton:checked {"
    "  background: #006040; color: #00ff88; border: 1px solid #00a060;"
    "}";

// Inset value label
static const char* kInsetValue =
    "QLabel {"
    "  font-size: 10px;"
    "  background: #0a0a18; border: 1px solid #1e2e3e; border-radius: 3px;"
    "  padding: 1px 2px; color: #c8d8e8;"
    "}";

// Section label style
static const char* kSectionLabel =
    "QLabel { color: #8090a0; font-size: 10px; }";

// --------------------------------------------------------------------------
// DigitalApplet
// --------------------------------------------------------------------------

DigitalApplet::DigitalApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
}

void DigitalApplet::buildSliderRow(QVBoxLayout* root, const QString& label,
                                    QSlider*& sliderOut, QLabel*& valueLblOut)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(4);

    auto* lbl = new QLabel(label, this);
    lbl->setStyleSheet(kSectionLabel);
    lbl->setFixedWidth(50);
    row->addWidget(lbl);

    sliderOut = new QSlider(Qt::Horizontal, this);
    sliderOut->setRange(0, 100);
    sliderOut->setValue(50);
    sliderOut->setFixedHeight(18);
    row->addWidget(sliderOut, 1);

    valueLblOut = new QLabel(QStringLiteral("50"), this);
    valueLblOut->setStyleSheet(kInsetValue);
    valueLblOut->setFixedWidth(30);
    valueLblOut->setAlignment(Qt::AlignCenter);
    row->addWidget(valueLblOut);

    root->addLayout(row);
}

void DigitalApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    // Body margins: (4,2,4,2), spacing 2 — per task spec
    root->setContentsMargins(4, 2, 4, 2);
    root->setSpacing(2);

    // -----------------------------------------------------------------------
    // VAC 1 group: enable button + device combo
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_vac1Btn = new QPushButton(QStringLiteral("VAC 1"), this);
        m_vac1Btn->setCheckable(true);
        m_vac1Btn->setFixedHeight(22);
        m_vac1Btn->setFixedWidth(50);
        m_vac1Btn->setStyleSheet(kGreenActive);
        row->addWidget(m_vac1Btn);

        m_vac1DevCombo = new QComboBox(this);
        m_vac1DevCombo->setFixedHeight(22);
        m_vac1DevCombo->setToolTip(QStringLiteral("VAC 1 audio device"));
        row->addWidget(m_vac1DevCombo, 1);

        root->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // VAC 2 group: enable button + device combo
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_vac2Btn = new QPushButton(QStringLiteral("VAC 2"), this);
        m_vac2Btn->setCheckable(true);
        m_vac2Btn->setFixedHeight(22);
        m_vac2Btn->setFixedWidth(50);
        m_vac2Btn->setStyleSheet(kGreenActive);
        row->addWidget(m_vac2Btn);

        m_vac2DevCombo = new QComboBox(this);
        m_vac2DevCombo->setFixedHeight(22);
        m_vac2DevCombo->setToolTip(QStringLiteral("VAC 2 audio device"));
        row->addWidget(m_vac2DevCombo, 1);

        root->addLayout(row);
    }

    // Divider between VAC sections and shared settings
    {
        auto* divider = new QFrame(this);
        divider->setFrameShape(QFrame::HLine);
        divider->setFrameShadow(QFrame::Sunken);
        divider->setFixedHeight(2);
        divider->setStyleSheet(QStringLiteral("QFrame { color: #1e2e3e; }"));
        root->addWidget(divider);
    }

    // -----------------------------------------------------------------------
    // Control 5: Sample rate combo
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Rate"), this);
        lbl->setStyleSheet(kSectionLabel);
        lbl->setFixedWidth(50);
        row->addWidget(lbl);

        m_sampleRateCombo = new QComboBox(this);
        m_sampleRateCombo->setFixedHeight(22);
        m_sampleRateCombo->addItems({
            QStringLiteral("8000"),
            QStringLiteral("11025"),
            QStringLiteral("22050"),
            QStringLiteral("44100"),
            QStringLiteral("48000"),
            QStringLiteral("96000")
        });
        // Default to 48000
        m_sampleRateCombo->setCurrentIndex(4);
        row->addWidget(m_sampleRateCombo, 1);

        root->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // Control 6: Stereo/Mono toggle (blue)
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_stereoBtn = new QPushButton(QStringLiteral("Stereo"), this);
        m_stereoBtn->setCheckable(true);
        m_stereoBtn->setChecked(true);
        m_stereoBtn->setFixedHeight(22);
        m_stereoBtn->setStyleSheet(kBlueActive);
        row->addWidget(m_stereoBtn, 1);

        root->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // Control 7: RX Gain + TX Gain sliders
    // -----------------------------------------------------------------------
    buildSliderRow(root, QStringLiteral("RX Gain"), m_rxGainSlider, m_rxGainLbl);
    buildSliderRow(root, QStringLiteral("TX Gain"), m_txGainSlider, m_txGainLbl);

    root->addStretch();

    // -----------------------------------------------------------------------
    // Mark all controls NYI — Phase 3-DAX
    // -----------------------------------------------------------------------
    const QString kPhase = QStringLiteral("Phase 3-DAX");
    NyiOverlay::markNyi(m_vac1Btn,         kPhase);
    NyiOverlay::markNyi(m_vac1DevCombo,    kPhase);
    NyiOverlay::markNyi(m_vac2Btn,         kPhase);
    NyiOverlay::markNyi(m_vac2DevCombo,    kPhase);
    NyiOverlay::markNyi(m_sampleRateCombo, kPhase);
    NyiOverlay::markNyi(m_stereoBtn,       kPhase);
    NyiOverlay::markNyi(m_rxGainSlider,    kPhase);
    NyiOverlay::markNyi(m_txGainSlider,    kPhase);
}

void DigitalApplet::syncFromModel()
{
    // NYI — no model wiring until Phase 3-DAX
}

} // namespace NereusSDR
