#include "DigitalApplet.h"
#include "NyiOverlay.h"
#include "gui/ComboStyle.h"
#include "gui/StyleConstants.h"

#include <QComboBox>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace NereusSDR {

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
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
    lbl->setFixedWidth(50);
    row->addWidget(lbl);

    sliderOut = new QSlider(Qt::Horizontal, this);
    sliderOut->setRange(0, 100);
    sliderOut->setValue(50);
    sliderOut->setFixedHeight(18);
    row->addWidget(sliderOut, 1);

    valueLblOut = insetValue(QStringLiteral("50"));
    row->addWidget(valueLblOut);

    root->addLayout(row);
}

void DigitalApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(appletTitleBar(QStringLiteral("Digital / VAC")));

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(2);

    // -----------------------------------------------------------------------
    // VAC 1 group: enable button + device combo
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_vac1Btn = greenToggle(QStringLiteral("VAC 1"));
        m_vac1Btn->setCheckable(true);
        m_vac1Btn->setFixedWidth(50);
        row->addWidget(m_vac1Btn);

        m_vac1DevCombo = new QComboBox(this);
        m_vac1DevCombo->setToolTip(QStringLiteral("VAC 1 audio device"));
        applyComboStyle(m_vac1DevCombo);
        row->addWidget(m_vac1DevCombo, 1);

        vbox->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // VAC 2 group: enable button + device combo
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_vac2Btn = greenToggle(QStringLiteral("VAC 2"));
        m_vac2Btn->setCheckable(true);
        m_vac2Btn->setFixedWidth(50);
        row->addWidget(m_vac2Btn);

        m_vac2DevCombo = new QComboBox(this);
        m_vac2DevCombo->setToolTip(QStringLiteral("VAC 2 audio device"));
        applyComboStyle(m_vac2DevCombo);
        row->addWidget(m_vac2DevCombo, 1);

        vbox->addLayout(row);
    }

    vbox->addWidget(divider());

    // -----------------------------------------------------------------------
    // Control 5: Sample rate combo — "48000", "96000", "192000"
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Rate"), this);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
        lbl->setFixedWidth(50);
        row->addWidget(lbl);

        m_sampleRateCombo = new QComboBox(this);
        m_sampleRateCombo->addItems({
            QStringLiteral("48000"),
            QStringLiteral("96000"),
            QStringLiteral("192000")
        });
        applyComboStyle(m_sampleRateCombo);
        row->addWidget(m_sampleRateCombo, 1);

        vbox->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // Control 6: Stereo/Mono toggle (blue)
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_stereoBtn = blueToggle(QStringLiteral("Stereo"));
        m_stereoBtn->setCheckable(true);
        m_stereoBtn->setChecked(true);
        row->addWidget(m_stereoBtn, 1);

        vbox->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // Control 7: Buffer size combo — "256", "512", "1024", "2048"
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Buffer"), this);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
        lbl->setFixedWidth(50);
        row->addWidget(lbl);

        m_bufferSizeCombo = new QComboBox(this);
        m_bufferSizeCombo->addItems({
            QStringLiteral("256"),
            QStringLiteral("512"),
            QStringLiteral("1024"),
            QStringLiteral("2048")
        });
        applyComboStyle(m_bufferSizeCombo);
        row->addWidget(m_bufferSizeCombo, 1);

        vbox->addLayout(row);
    }

    vbox->addWidget(divider());

    // -----------------------------------------------------------------------
    // RX Gain + TX Gain sliders
    // -----------------------------------------------------------------------
    buildSliderRow(vbox, QStringLiteral("RX Gain"), m_rxGainSlider, m_rxGainLbl);
    buildSliderRow(vbox, QStringLiteral("TX Gain"), m_txGainSlider, m_txGainLbl);

    // -----------------------------------------------------------------------
    // rigctld channel combo
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("rigctld"), this);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
        lbl->setFixedWidth(50);
        row->addWidget(lbl);

        m_rigctldCombo = new QComboBox(this);
        m_rigctldCombo->addItems({
            QStringLiteral("Ch 1"),
            QStringLiteral("Ch 2"),
            QStringLiteral("Ch 3"),
            QStringLiteral("Ch 4")
        });
        applyComboStyle(m_rigctldCombo);
        row->addWidget(m_rigctldCombo, 1);

        vbox->addLayout(row);
    }

    vbox->addStretch();
    root->addWidget(body);

    // -----------------------------------------------------------------------
    // Mark all controls NYI — Phase 3-DAX
    // -----------------------------------------------------------------------
    const QString kPhase = QStringLiteral("Phase 3-DAX");
    NyiOverlay::markNyi(m_vac1Btn,          kPhase);
    NyiOverlay::markNyi(m_vac1DevCombo,     kPhase);
    NyiOverlay::markNyi(m_vac2Btn,          kPhase);
    NyiOverlay::markNyi(m_vac2DevCombo,     kPhase);
    NyiOverlay::markNyi(m_sampleRateCombo,  kPhase);
    NyiOverlay::markNyi(m_stereoBtn,        kPhase);
    NyiOverlay::markNyi(m_bufferSizeCombo,  kPhase);
    NyiOverlay::markNyi(m_rxGainSlider,     kPhase);
    NyiOverlay::markNyi(m_txGainSlider,     kPhase);
    NyiOverlay::markNyi(m_rigctldCombo,     kPhase);
}

void DigitalApplet::syncFromModel()
{
    // NYI — no model wiring until Phase 3-DAX
}

} // namespace NereusSDR
