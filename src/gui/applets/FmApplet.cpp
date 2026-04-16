#include "FmApplet.h"
#include "gui/widgets/TriBtn.h"
#include "NyiOverlay.h"

#include <QComboBox>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace NereusSDR {

// --------------------------------------------------------------------------
// Style constants
// --------------------------------------------------------------------------

// Blue active state (deviation buttons, stereo button)
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

// Green active state (CTCSS enable, Simplex)
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

// Inset value label (offset display)
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
// FmApplet
// --------------------------------------------------------------------------

FmApplet::FmApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
}

void FmApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    // Body margins: (4,2,4,2), spacing 2 — per task spec
    root->setContentsMargins(4, 2, 4, 2);
    root->setSpacing(2);

    // -----------------------------------------------------------------------
    // Control 1: FM Mic slider
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("FM Mic"), this);
        lbl->setStyleSheet(kSectionLabel);
        lbl->setFixedWidth(44);
        row->addWidget(lbl);

        m_micSlider = new QSlider(Qt::Horizontal, this);
        m_micSlider->setRange(0, 100);
        m_micSlider->setValue(50);
        m_micSlider->setFixedHeight(18);
        row->addWidget(m_micSlider, 1);

        m_micValueLbl = new QLabel(QStringLiteral("50"), this);
        m_micValueLbl->setStyleSheet(kInsetValue);
        m_micValueLbl->setFixedWidth(30);
        m_micValueLbl->setAlignment(Qt::AlignCenter);
        row->addWidget(m_micValueLbl);

        root->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // Control 2: Deviation buttons — "5.0k" + "2.5k", mutually exclusive (blue)
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Dev"), this);
        lbl->setStyleSheet(kSectionLabel);
        lbl->setFixedWidth(44);
        row->addWidget(lbl);

        m_dev5kBtn = new QPushButton(QStringLiteral("5.0k"), this);
        m_dev5kBtn->setCheckable(true);
        m_dev5kBtn->setChecked(true);
        m_dev5kBtn->setFixedHeight(22);
        m_dev5kBtn->setStyleSheet(kBlueActive);
        row->addWidget(m_dev5kBtn, 1);

        m_dev25kBtn = new QPushButton(QStringLiteral("2.5k"), this);
        m_dev25kBtn->setCheckable(true);
        m_dev25kBtn->setFixedHeight(22);
        m_dev25kBtn->setStyleSheet(kBlueActive);
        row->addWidget(m_dev25kBtn, 1);

        root->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // Control 3 + 4: CTCSS enable + tone combo
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_ctcssBtn = new QPushButton(QStringLiteral("CTCSS"), this);
        m_ctcssBtn->setCheckable(true);
        m_ctcssBtn->setFixedHeight(22);
        m_ctcssBtn->setFixedWidth(52);
        m_ctcssBtn->setStyleSheet(kGreenActive);
        row->addWidget(m_ctcssBtn);

        m_ctcssCombo = new QComboBox(this);
        m_ctcssCombo->setFixedHeight(22);
        // Standard CTCSS tones (Hz) — from CTCSS standard tone table
        const QStringList ctcssTones = {
            QStringLiteral("67.0"),  QStringLiteral("71.9"),  QStringLiteral("74.4"),
            QStringLiteral("77.0"),  QStringLiteral("79.7"),  QStringLiteral("82.5"),
            QStringLiteral("85.4"),  QStringLiteral("88.5"),  QStringLiteral("91.5"),
            QStringLiteral("94.8"),  QStringLiteral("97.4"),  QStringLiteral("100.0"),
            QStringLiteral("103.5"), QStringLiteral("107.2"), QStringLiteral("110.9"),
            QStringLiteral("114.8"), QStringLiteral("118.8"), QStringLiteral("123.0"),
            QStringLiteral("127.3"), QStringLiteral("131.8"), QStringLiteral("136.5"),
            QStringLiteral("141.3"), QStringLiteral("146.2"), QStringLiteral("151.4"),
            QStringLiteral("156.7"), QStringLiteral("162.2"), QStringLiteral("167.9"),
            QStringLiteral("173.8"), QStringLiteral("179.9"), QStringLiteral("186.2"),
            QStringLiteral("192.8"), QStringLiteral("203.5"), QStringLiteral("210.7"),
            QStringLiteral("218.1"), QStringLiteral("225.7"), QStringLiteral("233.6"),
            QStringLiteral("241.8"), QStringLiteral("250.3")
        };
        m_ctcssCombo->addItems(ctcssTones);
        row->addWidget(m_ctcssCombo, 1);

        root->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // Control 5: Simplex toggle
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_simplexBtn = new QPushButton(QStringLiteral("Simplex"), this);
        m_simplexBtn->setCheckable(true);
        m_simplexBtn->setFixedHeight(22);
        m_simplexBtn->setStyleSheet(kGreenActive);
        row->addWidget(m_simplexBtn, 1);

        root->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // Control 6: Repeater offset stepper — ◀ + inset value + ▶
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Offset"), this);
        lbl->setStyleSheet(kSectionLabel);
        lbl->setFixedWidth(44);
        row->addWidget(lbl);

        auto* stepLeft = new TriBtn(TriBtn::Left, this);
        row->addWidget(stepLeft);

        m_offsetLbl = new QLabel(QStringLiteral("0.600 MHz"), this);
        m_offsetLbl->setStyleSheet(kInsetValue);
        m_offsetLbl->setFixedWidth(60);
        m_offsetLbl->setAlignment(Qt::AlignCenter);
        row->addWidget(m_offsetLbl);

        auto* stepRight = new TriBtn(TriBtn::Right, this);
        row->addWidget(stepRight);

        root->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // Control 7: Offset direction — "-" + "+" + "Rev", mutually exclusive
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Dir"), this);
        lbl->setStyleSheet(kSectionLabel);
        lbl->setFixedWidth(44);
        row->addWidget(lbl);

        m_offsetNegBtn = new QPushButton(QStringLiteral("-"), this);
        m_offsetNegBtn->setCheckable(true);
        m_offsetNegBtn->setChecked(true);
        m_offsetNegBtn->setFixedHeight(22);
        m_offsetNegBtn->setStyleSheet(kBlueActive);
        row->addWidget(m_offsetNegBtn, 1);

        m_offsetPosBtn = new QPushButton(QStringLiteral("+"), this);
        m_offsetPosBtn->setCheckable(true);
        m_offsetPosBtn->setFixedHeight(22);
        m_offsetPosBtn->setStyleSheet(kBlueActive);
        row->addWidget(m_offsetPosBtn, 1);

        m_offsetRevBtn = new QPushButton(QStringLiteral("Rev"), this);
        m_offsetRevBtn->setCheckable(true);
        m_offsetRevBtn->setFixedHeight(22);
        m_offsetRevBtn->setStyleSheet(kBlueActive);
        row->addWidget(m_offsetRevBtn, 1);

        root->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // Control 8: FM TX Profile combo
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Profile"), this);
        lbl->setStyleSheet(kSectionLabel);
        lbl->setFixedWidth(44);
        row->addWidget(lbl);

        m_txProfileCombo = new QComboBox(this);
        m_txProfileCombo->setFixedHeight(22);
        m_txProfileCombo->addItem(QStringLiteral("Default"));
        row->addWidget(m_txProfileCombo, 1);

        root->addLayout(row);
    }

    root->addStretch();

    // -----------------------------------------------------------------------
    // Mark all controls NYI — Phase 3I-3
    // -----------------------------------------------------------------------
    const QString kPhase = QStringLiteral("Phase 3I-3");
    NyiOverlay::markNyi(m_micSlider,       kPhase);
    NyiOverlay::markNyi(m_dev5kBtn,        kPhase);
    NyiOverlay::markNyi(m_dev25kBtn,       kPhase);
    NyiOverlay::markNyi(m_ctcssBtn,        kPhase);
    NyiOverlay::markNyi(m_ctcssCombo,      kPhase);
    NyiOverlay::markNyi(m_simplexBtn,      kPhase);
    NyiOverlay::markNyi(m_offsetLbl,       kPhase);
    NyiOverlay::markNyi(m_offsetNegBtn,    kPhase);
    NyiOverlay::markNyi(m_offsetPosBtn,    kPhase);
    NyiOverlay::markNyi(m_offsetRevBtn,    kPhase);
    NyiOverlay::markNyi(m_txProfileCombo,  kPhase);
}

void FmApplet::syncFromModel()
{
    // NYI — no model wiring until Phase 3I-3
}

} // namespace NereusSDR
