// src/gui/applets/PhoneCwApplet.cpp
// Phone/CW/FM stacked applet — NYI shell with 30 controls.
// Phone=Phase 3I-1/3I-3, CW=Phase 3I-2, FM=Phase 3I-1.
#include "PhoneCwApplet.h"
#include "gui/HGauge.h"
#include "gui/ComboStyle.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/TriBtn.h"
#include "NyiOverlay.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace NereusSDR {

// ── Style constants (adapted from AetherSDR PhoneCwApplet.cpp) ────────────────

static constexpr const char* kSliderStyle =
    "QSlider::groove:horizontal { height: 4px; background: #203040; border-radius: 2px; }"
    "QSlider::handle:horizontal { width: 10px; height: 10px; margin: -3px 0;"
    "background: #00b4d8; border-radius: 5px; }";

static constexpr const char* kButtonBase =
    "QPushButton { background: #1a3a5a; border: 1px solid #205070; "
    "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
    "QPushButton:hover { background: #204060; }";

// Blue active style: used for DAX, Iambic, tab buttons
static constexpr const char* kBlueActive =
    "QPushButton:checked { background-color: #0070c0; color: #ffffff; "
    "border: 1px solid #0090e0; }";

// Green active style: used for ACC, PROC, MON, VOX, DEXP, Sidetone, QSK, Firmware keyer
static constexpr const char* kGreenActive =
    "QPushButton:checked { background-color: #006040; color: #00ff88; "
    "border: 1px solid #00a060; }";

static constexpr const char* kLabelStyle =
    "QLabel { color: #c8d8e8; font-size: 10px; }";

static constexpr const char* kDimLabelStyle =
    "QLabel { color: #8090a0; font-size: 10px; }";

static constexpr const char* kInsetValueStyle =
    "QLabel { font-size: 10px; background: #0a0a18; border: 1px solid #1e2e3e; "
    "border-radius: 3px; padding: 1px 2px; color: #c8d8e8; }";

static constexpr const char* kTickLabelStyle =
    "QLabel { color: #c8d8e8; font-size: 8px; }";

// CW column widths (from AetherSDR PhoneCwApplet.cpp)
static constexpr int kLeftColW = 70;
static constexpr int kValueW   = 36;
static constexpr int kGap      = 4;

// NYI phase tags
static const QString kNyiPhone  = QStringLiteral("Phase 3I-1");
static const QString kNyiCw     = QStringLiteral("Phase 3I-2");
static const QString kNyiProc   = QStringLiteral("Phase 3I-3");
static const QString kNyiDax    = QStringLiteral("Phase 3-DAX");
static const QString kNyiFm     = QStringLiteral("Phase 3I-1");

// CTCSS tones (standard 38-tone list from Thetis setup.cs)
static const QStringList kCtcssTones = {
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
    QStringLiteral("241.8"), QStringLiteral("254.1"),
};


// ── PhoneCwApplet ─────────────────────────────────────────────────────────────

PhoneCwApplet::PhoneCwApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    buildUI();
}

void PhoneCwApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Phone/CW/FM tab button row ───────────────────────────────────────────
    m_tabGroup = new QButtonGroup(this);
    m_tabGroup->setExclusive(true);
    {
        auto* tabRow = new QHBoxLayout;
        tabRow->setSpacing(2);
        tabRow->setContentsMargins(4, 2, 4, 2);

        m_phoneTabBtn = new QPushButton(QStringLiteral("Phone"), this);
        m_phoneTabBtn->setCheckable(true);
        m_phoneTabBtn->setChecked(true);
        m_phoneTabBtn->setFixedHeight(20);
        m_phoneTabBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_phoneTabBtn->setStyleSheet(QString(kButtonBase) + kBlueActive);
        m_tabGroup->addButton(m_phoneTabBtn, 0);
        tabRow->addWidget(m_phoneTabBtn);

        m_cwTabBtn = new QPushButton(QStringLiteral("CW"), this);
        m_cwTabBtn->setCheckable(true);
        m_cwTabBtn->setFixedHeight(20);
        m_cwTabBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_cwTabBtn->setStyleSheet(QString(kButtonBase) + kBlueActive);
        m_tabGroup->addButton(m_cwTabBtn, 1);
        tabRow->addWidget(m_cwTabBtn);

        // FM is a separate FmApplet per reconciled design spec

        root->addLayout(tabRow);
    }

    // ── Stacked widget: Phone (0) / CW (1) ──────────────────────────────────
    m_stack = new QStackedWidget(this);

    auto* phonePage = new QWidget(m_stack);
    buildPhonePage(phonePage);
    m_stack->addWidget(phonePage);  // index 0

    auto* cwPage = new QWidget(m_stack);
    buildCwPage(cwPage);
    m_stack->addWidget(cwPage);     // index 1

    auto* fmPage = new QWidget(m_stack);
    buildFmPage(fmPage);
    m_stack->addWidget(fmPage);     // index 2

    m_stack->setCurrentIndex(0);
    root->addWidget(m_stack);

    // ── Wire tab buttons via QButtonGroup ────────────────────────────────────
    connect(m_tabGroup, &QButtonGroup::idToggled, this,
            [this](int id, bool checked) {
        if (checked) {
            m_stack->setCurrentIndex(id);
        }
    });
}

// ── Phone page (13 controls) ──────────────────────────────────────────────────

void PhoneCwApplet::buildPhonePage(QWidget* page)
{
    // From AetherSDR PhoneCwApplet.cpp buildPhonePanel():
    // contentsMargins(4,2,4,2), spacing 2
    auto* vbox = new QVBoxLayout(page);
    vbox->setContentsMargins(4, 2, 4, 2);
    vbox->setSpacing(2);

    // ── Control 1: Mic level gauge ───────────────────────────────────────────
    // HGauge(-40, +10, redStart=0, yellowStart=-10)
    // Ticks: -40/-30/-20/-10/0/+5/+10
    m_levelGauge = new HGauge(page);
    m_levelGauge->setRange(-40.0, 10.0);
    m_levelGauge->setYellowStart(-10.0);
    m_levelGauge->setRedStart(0.0);
    m_levelGauge->setTitle(QStringLiteral("Level"));
    m_levelGauge->setUnit(QStringLiteral("dB"));
    m_levelGauge->setTickLabels({QStringLiteral("-40dB"), QStringLiteral("-30"),
                                  QStringLiteral("-20"),  QStringLiteral("-10"),
                                  QStringLiteral("0"),    QStringLiteral("+5"),
                                  QStringLiteral("+10")});
    m_levelGauge->setAccessibleName(QStringLiteral("Microphone level gauge"));
    vbox->addWidget(m_levelGauge);

    // ── Control 2: Compression gauge ─────────────────────────────────────────
    // HGauge(-25, 0, redStart=1, reversed=true)
    // Ticks: -25/-20/-15/-10/-5/0
    m_compGauge = new HGauge(page);
    m_compGauge->setRange(-25.0, 0.0);
    m_compGauge->setRedStart(1.0);
    m_compGauge->setReversed(true);
    m_compGauge->setTitle(QStringLiteral("Compression"));
    m_compGauge->setTickLabels({QStringLiteral("-25dB"), QStringLiteral("-20"),
                                 QStringLiteral("-15"),   QStringLiteral("-10"),
                                 QStringLiteral("-5"),    QStringLiteral("0")});
    m_compGauge->setAccessibleName(QStringLiteral("Compression gauge"));
    vbox->addWidget(m_compGauge);
    vbox->addSpacing(4);

    // ── Control 3: Mic profile combo ─────────────────────────────────────────
    m_micProfileCombo = new QComboBox(page);
    m_micProfileCombo->setFixedHeight(22);
    m_micProfileCombo->addItems({QStringLiteral("Default"), QStringLiteral("DX"),
                                 QStringLiteral("Contest"), QStringLiteral("Custom")});
    m_micProfileCombo->setAccessibleName(QStringLiteral("Microphone profile"));
    applyComboStyle(m_micProfileCombo);
    vbox->addWidget(m_micProfileCombo);

    // ── Control 4: Mic source + Control 5: Mic level slider + Control 6: +ACC ─
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // Control 4: Mic source combo (fixedWidth 55, fixedHeight 22)
        m_micSourceCombo = new QComboBox(page);
        m_micSourceCombo->setFixedWidth(55);
        m_micSourceCombo->setFixedHeight(22);
        m_micSourceCombo->addItems({QStringLiteral("MIC"), QStringLiteral("BAL"),
                                    QStringLiteral("LINE"), QStringLiteral("ACC"),
                                    QStringLiteral("PC")});
        m_micSourceCombo->setAccessibleName(QStringLiteral("Microphone source"));
        applyComboStyle(m_micSourceCombo);
        row->addWidget(m_micSourceCombo);

        // Control 5a: Mic level slider
        m_micLevelSlider = new QSlider(Qt::Horizontal, page);
        m_micLevelSlider->setRange(0, 100);
        m_micLevelSlider->setValue(50);
        m_micLevelSlider->setStyleSheet(kSliderStyle);
        m_micLevelSlider->setAccessibleName(QStringLiteral("Microphone level"));
        row->addWidget(m_micLevelSlider, 1);

        // Control 5b: Value label (fixedWidth 22)
        m_micLevelLabel = new QLabel(QStringLiteral("50"), page);
        m_micLevelLabel->setStyleSheet(kLabelStyle);
        m_micLevelLabel->setFixedWidth(22);
        m_micLevelLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(m_micLevelLabel);

        // Control 6: +ACC button (green, checkable, fixedWidth 48, fixedHeight 22)
        m_accBtn = new QPushButton(QStringLiteral("+ACC"), page);
        m_accBtn->setCheckable(true);
        m_accBtn->setFixedWidth(48);
        m_accBtn->setFixedHeight(22);
        m_accBtn->setStyleSheet(QString(kButtonBase) + kGreenActive);
        m_accBtn->setAccessibleName(QStringLiteral("Accessory mic input"));
        row->addWidget(m_accBtn);

        vbox->addLayout(row);
    }

    // ── Control 7: PROC + Control 8: PROC slider + Control 9: DAX ───────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // Control 7: PROC button (green, checkable, fixedWidth 48, fixedHeight 22)
        m_procBtn = new QPushButton(QStringLiteral("PROC"), page);
        m_procBtn->setCheckable(true);
        m_procBtn->setFixedWidth(48);
        m_procBtn->setFixedHeight(22);
        m_procBtn->setStyleSheet(QString(kButtonBase) + kGreenActive);
        m_procBtn->setAccessibleName(QStringLiteral("Speech processor"));
        row->addWidget(m_procBtn);

        // Control 8: 3-position PROC slider with NOR/DX/DX+ tick labels
        auto* procGroup = new QWidget(page);
        auto* procVbox = new QVBoxLayout(procGroup);
        procVbox->setContentsMargins(0, 0, 0, 0);
        procVbox->setSpacing(0);

        auto* labelsRow = new QHBoxLayout;
        labelsRow->setContentsMargins(0, 0, 0, 0);
        auto* norLbl   = new QLabel(QStringLiteral("NOR"),  procGroup);
        auto* dxLbl    = new QLabel(QStringLiteral("DX"),   procGroup);
        auto* dxPlusLbl = new QLabel(QStringLiteral("DX+"), procGroup);
        norLbl->setStyleSheet(kTickLabelStyle);
        dxLbl->setStyleSheet(kTickLabelStyle);
        dxPlusLbl->setStyleSheet(kTickLabelStyle);
        norLbl->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
        dxLbl->setAlignment(Qt::AlignCenter | Qt::AlignBottom);
        dxPlusLbl->setAlignment(Qt::AlignRight | Qt::AlignBottom);
        labelsRow->addWidget(norLbl);
        labelsRow->addWidget(dxLbl);
        labelsRow->addWidget(dxPlusLbl);
        procVbox->addLayout(labelsRow);

        m_procSlider = new QSlider(Qt::Horizontal, procGroup);
        m_procSlider->setRange(0, 2);
        m_procSlider->setTickInterval(1);
        m_procSlider->setTickPosition(QSlider::NoTicks);
        m_procSlider->setPageStep(1);
        m_procSlider->setFixedHeight(14);
        m_procSlider->setStyleSheet(kSliderStyle);
        m_procSlider->setAccessibleName(QStringLiteral("Processor level (NOR/DX/DX+)"));
        procVbox->addWidget(m_procSlider);

        row->addWidget(procGroup, 1);

        // Control 9: DAX button (blue, checkable, fixedWidth 48, fixedHeight 22)
        m_daxBtn = new QPushButton(QStringLiteral("DAX"), page);
        m_daxBtn->setCheckable(true);
        m_daxBtn->setFixedWidth(48);
        m_daxBtn->setFixedHeight(22);
        m_daxBtn->setStyleSheet(QString(kButtonBase) + kBlueActive);
        m_daxBtn->setAccessibleName(QStringLiteral("DAX digital audio"));
        row->addWidget(m_daxBtn);

        vbox->addLayout(row);
    }

    // ── Control 10: MON button + level slider ────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_monBtn = new QPushButton(QStringLiteral("MON"), page);
        m_monBtn->setCheckable(true);
        m_monBtn->setFixedWidth(48);
        m_monBtn->setFixedHeight(22);
        m_monBtn->setStyleSheet(QString(kButtonBase) + kGreenActive);
        m_monBtn->setAccessibleName(QStringLiteral("TX monitor"));
        row->addWidget(m_monBtn);

        m_monSlider = new QSlider(Qt::Horizontal, page);
        m_monSlider->setRange(0, 100);
        m_monSlider->setValue(50);
        m_monSlider->setStyleSheet(kSliderStyle);
        m_monSlider->setAccessibleName(QStringLiteral("Monitor level"));
        row->addWidget(m_monSlider, 1);

        auto* monLabel = new QLabel(QStringLiteral("50"), page);
        monLabel->setStyleSheet(kLabelStyle);
        monLabel->setFixedWidth(22);
        monLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(monLabel);

        vbox->addLayout(row);
    }

    // ── Control 10: VOX toggle + level slider + delay slider ────────────────
    // Spec: QPushButton green 36px + QSlider level + QSlider delay + 2 insets
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* voxLbl = new QLabel(QStringLiteral("VOX"), page);
        voxLbl->setStyleSheet(kDimLabelStyle);
        voxLbl->setFixedWidth(24);
        row->addWidget(voxLbl);

        m_voxBtn = new QPushButton(QStringLiteral("ON"), page);
        m_voxBtn->setCheckable(true);
        m_voxBtn->setFixedWidth(36);
        m_voxBtn->setFixedHeight(22);
        m_voxBtn->setStyleSheet(QString(kButtonBase) + kGreenActive);
        m_voxBtn->setAccessibleName(QStringLiteral("VOX voice-operated transmit"));
        row->addWidget(m_voxBtn);

        m_voxSlider = new QSlider(Qt::Horizontal, page);
        m_voxSlider->setRange(0, 100);
        m_voxSlider->setValue(50);
        m_voxSlider->setStyleSheet(kSliderStyle);
        m_voxSlider->setAccessibleName(QStringLiteral("VOX level"));
        row->addWidget(m_voxSlider, 1);

        m_voxLvlLabel = new QLabel(QStringLiteral("50"), page);
        m_voxLvlLabel->setStyleSheet(kInsetValueStyle);
        m_voxLvlLabel->setFixedWidth(22);
        m_voxLvlLabel->setAlignment(Qt::AlignCenter);
        row->addWidget(m_voxLvlLabel);

        m_voxDlySlider = new QSlider(Qt::Horizontal, page);
        m_voxDlySlider->setRange(0, 100);
        m_voxDlySlider->setValue(30);
        m_voxDlySlider->setStyleSheet(kSliderStyle);
        m_voxDlySlider->setAccessibleName(QStringLiteral("VOX delay"));
        row->addWidget(m_voxDlySlider, 1);

        m_voxDlyLabel = new QLabel(QStringLiteral("300"), page);
        m_voxDlyLabel->setStyleSheet(kInsetValueStyle);
        m_voxDlyLabel->setFixedWidth(26);
        m_voxDlyLabel->setAlignment(Qt::AlignCenter);
        row->addWidget(m_voxDlyLabel);

        vbox->addLayout(row);
    }

    // ── Control 12: DEXP toggle + level slider ───────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_dexpBtn = new QPushButton(QStringLiteral("DEXP"), page);
        m_dexpBtn->setCheckable(true);
        m_dexpBtn->setFixedWidth(48);
        m_dexpBtn->setFixedHeight(22);
        m_dexpBtn->setStyleSheet(QString(kButtonBase) + kGreenActive);
        m_dexpBtn->setAccessibleName(QStringLiteral("Downward expander / noise gate"));
        row->addWidget(m_dexpBtn);

        m_dexpSlider = new QSlider(Qt::Horizontal, page);
        m_dexpSlider->setRange(0, 100);
        m_dexpSlider->setValue(50);
        m_dexpSlider->setStyleSheet(kSliderStyle);
        m_dexpSlider->setAccessibleName(QStringLiteral("DEXP level"));
        row->addWidget(m_dexpSlider, 1);

        auto* dexpLabel = new QLabel(QStringLiteral("50"), page);
        dexpLabel->setStyleSheet(kLabelStyle);
        dexpLabel->setFixedWidth(22);
        dexpLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(dexpLabel);

        vbox->addLayout(row);
    }

    // ── Control 13: TX filter Low/High Cut sliders ───────────────────────────
    {
        // Low Cut row
        auto* lowRow = new QHBoxLayout;
        lowRow->setSpacing(4);
        auto* lowLbl = new QLabel(QStringLiteral("TX Lo:"), page);
        lowLbl->setStyleSheet(kDimLabelStyle);
        lowLbl->setFixedWidth(40);
        lowRow->addWidget(lowLbl);
        m_txFiltLowSlider = new QSlider(Qt::Horizontal, page);
        m_txFiltLowSlider->setRange(0, 500);
        m_txFiltLowSlider->setValue(100);
        m_txFiltLowSlider->setStyleSheet(kSliderStyle);
        m_txFiltLowSlider->setAccessibleName(QStringLiteral("TX filter low cut"));
        lowRow->addWidget(m_txFiltLowSlider, 1);
        auto* lowValLbl = new QLabel(QStringLiteral("100"), page);
        lowValLbl->setStyleSheet(kInsetValueStyle);
        lowValLbl->setFixedWidth(30);
        lowValLbl->setAlignment(Qt::AlignCenter);
        lowRow->addWidget(lowValLbl);
        vbox->addLayout(lowRow);

        // High Cut row
        auto* highRow = new QHBoxLayout;
        highRow->setSpacing(4);
        auto* highLbl = new QLabel(QStringLiteral("TX Hi:"), page);
        highLbl->setStyleSheet(kDimLabelStyle);
        highLbl->setFixedWidth(40);
        highRow->addWidget(highLbl);
        m_txFiltHighSlider = new QSlider(Qt::Horizontal, page);
        m_txFiltHighSlider->setRange(500, 5000);
        m_txFiltHighSlider->setValue(2800);
        m_txFiltHighSlider->setStyleSheet(kSliderStyle);
        m_txFiltHighSlider->setAccessibleName(QStringLiteral("TX filter high cut"));
        highRow->addWidget(m_txFiltHighSlider, 1);
        auto* highValLbl = new QLabel(QStringLiteral("2800"), page);
        highValLbl->setStyleSheet(kInsetValueStyle);
        highValLbl->setFixedWidth(30);
        highValLbl->setAlignment(Qt::AlignCenter);
        highRow->addWidget(highValLbl);
        vbox->addLayout(highRow);
    }

    // ── Control 13: AM Carrier level slider (0-100) + inset "25" ────────────
    // Spec: Phase 3I-3
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("AM Car:"), page);
        lbl->setStyleSheet(kDimLabelStyle);
        lbl->setFixedWidth(40);
        row->addWidget(lbl);

        m_amCarSlider = new QSlider(Qt::Horizontal, page);
        m_amCarSlider->setRange(0, 100);
        m_amCarSlider->setValue(25);
        m_amCarSlider->setStyleSheet(kSliderStyle);
        m_amCarSlider->setAccessibleName(QStringLiteral("AM carrier level"));
        row->addWidget(m_amCarSlider, 1);

        m_amCarLabel = new QLabel(QStringLiteral("25"), page);
        m_amCarLabel->setStyleSheet(kInsetValueStyle);
        m_amCarLabel->setFixedWidth(30);
        m_amCarLabel->setAlignment(Qt::AlignCenter);
        row->addWidget(m_amCarLabel);

        vbox->addLayout(row);
    }

    // ── Mark all Phone controls NYI ───────────────────────────────────────────
    NyiOverlay::markNyi(m_levelGauge,       kNyiPhone);   // #1
    NyiOverlay::markNyi(m_compGauge,        kNyiProc);    // #2 — Phase 3I-3
    NyiOverlay::markNyi(m_micProfileCombo,  kNyiPhone);   // #3
    NyiOverlay::markNyi(m_micSourceCombo,   kNyiPhone);   // #4
    NyiOverlay::markNyi(m_micLevelSlider,   kNyiPhone);   // #5
    NyiOverlay::markNyi(m_accBtn,           kNyiPhone);   // #6
    NyiOverlay::markNyi(m_procBtn,          kNyiProc);    // #7 — Phase 3I-3
    NyiOverlay::markNyi(m_procSlider,       kNyiProc);    // #7 slider
    NyiOverlay::markNyi(m_daxBtn,           kNyiDax);     // #8 — Phase 3-DAX
    NyiOverlay::markNyi(m_monBtn,           kNyiPhone);   // #9
    NyiOverlay::markNyi(m_monSlider,        kNyiPhone);   // #9 slider
    NyiOverlay::markNyi(m_voxBtn,           kNyiProc);    // #10 — Phase 3I-3
    NyiOverlay::markNyi(m_voxSlider,        kNyiProc);    // #10 level
    NyiOverlay::markNyi(m_voxDlySlider,     kNyiProc);    // #10 delay
    NyiOverlay::markNyi(m_dexpBtn,          kNyiProc);    // #11 — Phase 3I-3
    NyiOverlay::markNyi(m_dexpSlider,       kNyiProc);    // #11 slider
    NyiOverlay::markNyi(m_txFiltLowSlider,  kNyiProc);    // #12 — Phase 3I-3
    NyiOverlay::markNyi(m_txFiltHighSlider, kNyiProc);    // #12 hi
    NyiOverlay::markNyi(m_amCarSlider,      kNyiProc);    // #13 — Phase 3I-3
}

// ── CW page (9 controls) ──────────────────────────────────────────────────────

void PhoneCwApplet::buildCwPage(QWidget* page)
{
    // From AetherSDR PhoneCwApplet.cpp buildCwPanel():
    // contentsMargins(4,2,4,6), spacing 4
    auto* vbox = new QVBoxLayout(page);
    vbox->setContentsMargins(4, 2, 4, 6);
    vbox->setSpacing(4);

    // ── Control 1: ALC gauge (0–100, redStart 80) ────────────────────────────
    m_alcGauge = new HGauge(page);
    m_alcGauge->setRange(0.0, 100.0);
    m_alcGauge->setRedStart(80.0);
    m_alcGauge->setTitle(QStringLiteral("ALC"));
    m_alcGauge->setTickLabels({QStringLiteral("0"),  QStringLiteral("25"),
                                QStringLiteral("50"), QStringLiteral("75"),
                                QStringLiteral("100")});
    m_alcGauge->setAccessibleName(QStringLiteral("ALC gauge"));
    vbox->addWidget(m_alcGauge);
    vbox->addSpacing(2);

    // ── Control 2: CW speed slider (1–60 WPM) ───────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Speed:"), page);
        lbl->setStyleSheet(kDimLabelStyle);
        lbl->setFixedWidth(kLeftColW);
        row->addWidget(lbl);

        row->addSpacing(kGap);

        m_speedSlider = new QSlider(Qt::Horizontal, page);
        m_speedSlider->setRange(1, 60);
        m_speedSlider->setValue(20);
        m_speedSlider->setStyleSheet(kSliderStyle);
        m_speedSlider->setAccessibleName(QStringLiteral("CW speed (WPM)"));
        row->addWidget(m_speedSlider, 1);

        m_speedLabel = new QLabel(QStringLiteral("20"), page);
        m_speedLabel->setStyleSheet(kInsetValueStyle);
        m_speedLabel->setFixedWidth(kValueW);
        m_speedLabel->setAlignment(Qt::AlignCenter);
        row->addWidget(m_speedLabel);

        vbox->addLayout(row);
    }

    // ── Control 3: CW pitch + TriBtn steppers ───────────────────────────────
    // Pitch range 100–6000 Hz, step 10
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Pitch:"), page);
        lbl->setStyleSheet(kDimLabelStyle);
        lbl->setFixedWidth(kLeftColW);
        row->addWidget(lbl);

        row->addSpacing(kGap);

        m_pitchDown = new TriBtn(TriBtn::Left, page);
        m_pitchDown->setAccessibleName(QStringLiteral("CW pitch down"));
        row->addWidget(m_pitchDown);

        m_pitchLabel = new QLabel(QStringLiteral("700 Hz"), page);
        m_pitchLabel->setAlignment(Qt::AlignCenter);
        m_pitchLabel->setAccessibleName(QStringLiteral("CW pitch frequency"));
        m_pitchLabel->setStyleSheet(
            "QLabel { font-size: 10px; background: #0a0a18; border: 1px solid #1e2e3e; "
            "border-radius: 3px; padding: 1px 3px; color: #c8d8e8; }");
        row->addWidget(m_pitchLabel, 1);

        m_pitchUp = new TriBtn(TriBtn::Right, page);
        m_pitchUp->setAccessibleName(QStringLiteral("CW pitch up"));
        row->addWidget(m_pitchUp);

        row->addStretch();

        vbox->addLayout(row);
    }

    // ── Control 4: Delay slider ──────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Delay:"), page);
        lbl->setStyleSheet(kDimLabelStyle);
        lbl->setFixedWidth(kLeftColW);
        row->addWidget(lbl);

        row->addSpacing(kGap);

        m_delaySlider = new QSlider(Qt::Horizontal, page);
        m_delaySlider->setRange(0, 2000);
        m_delaySlider->setValue(500);
        m_delaySlider->setSingleStep(10);
        m_delaySlider->setPageStep(100);
        m_delaySlider->setStyleSheet(kSliderStyle);
        m_delaySlider->setAccessibleName(QStringLiteral("CW break-in delay (ms)"));
        row->addWidget(m_delaySlider, 1);

        m_delayLabel = new QLabel(QStringLiteral("500"), page);
        m_delayLabel->setStyleSheet(kInsetValueStyle);
        m_delayLabel->setFixedWidth(kValueW);
        m_delayLabel->setAlignment(Qt::AlignCenter);
        row->addWidget(m_delayLabel);

        vbox->addLayout(row);
    }

    // ── Control 5: Sidetone toggle + slider ─────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // QPushButton(green, "Sidetone", fixedWidth 70)
        m_sidetoneBtn = new QPushButton(QStringLiteral("Sidetone"), page);
        m_sidetoneBtn->setCheckable(true);
        m_sidetoneBtn->setFixedHeight(22);
        m_sidetoneBtn->setFixedWidth(kLeftColW);
        m_sidetoneBtn->setStyleSheet(QString(kButtonBase) + kGreenActive);
        m_sidetoneBtn->setAccessibleName(QStringLiteral("CW sidetone"));
        row->addWidget(m_sidetoneBtn);

        row->addSpacing(kGap);

        m_sidetoneSlider = new QSlider(Qt::Horizontal, page);
        m_sidetoneSlider->setRange(0, 100);
        m_sidetoneSlider->setValue(50);
        m_sidetoneSlider->setStyleSheet(kSliderStyle);
        m_sidetoneSlider->setAccessibleName(QStringLiteral("Sidetone volume"));
        row->addWidget(m_sidetoneSlider, 1);

        auto* stLabel = new QLabel(QStringLiteral("50"), page);
        stLabel->setStyleSheet(kInsetValueStyle);
        stLabel->setFixedWidth(kValueW);
        stLabel->setAlignment(Qt::AlignCenter);
        row->addWidget(stLabel);

        vbox->addLayout(row);
    }

    // ── Controls 6-8: QSK / Iambic / Firmware keyer toggles ─────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // Control 6: Break-in (QSK) toggle (green)
        m_breakinBtn = new QPushButton(QStringLiteral("QSK"), page);
        m_breakinBtn->setCheckable(true);
        m_breakinBtn->setFixedHeight(22);
        m_breakinBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_breakinBtn->setStyleSheet(QString(kButtonBase) + kGreenActive);
        m_breakinBtn->setAccessibleName(QStringLiteral("CW break-in / QSK"));
        row->addWidget(m_breakinBtn, 1);

        // Control 7: Iambic toggle (blue)
        m_iambicBtn = new QPushButton(QStringLiteral("Iambic"), page);
        m_iambicBtn->setCheckable(true);
        m_iambicBtn->setFixedHeight(22);
        m_iambicBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_iambicBtn->setStyleSheet(QString(kButtonBase) + kBlueActive);
        m_iambicBtn->setAccessibleName(QStringLiteral("Iambic paddle keyer"));
        row->addWidget(m_iambicBtn, 1);

        // Control 8: Firmware keyer toggle
        m_fwKeyerBtn = new QPushButton(QStringLiteral("FW Keyer"), page);
        m_fwKeyerBtn->setCheckable(true);
        m_fwKeyerBtn->setFixedHeight(22);
        m_fwKeyerBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_fwKeyerBtn->setStyleSheet(QString(kButtonBase) + kGreenActive);
        m_fwKeyerBtn->setAccessibleName(QStringLiteral("Firmware CW keyer"));
        row->addWidget(m_fwKeyerBtn, 1);

        vbox->addLayout(row);
    }

    // ── Control 9: CW pan slider (L–R) ──────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lLbl = new QLabel(QStringLiteral("L"), page);
        lLbl->setStyleSheet(kDimLabelStyle);
        row->addWidget(lLbl);

        m_cwPanSlider = new QSlider(Qt::Horizontal, page);
        m_cwPanSlider->setRange(0, 100);
        m_cwPanSlider->setValue(50);
        m_cwPanSlider->setStyleSheet(kSliderStyle);
        m_cwPanSlider->setAccessibleName(QStringLiteral("CW audio pan"));
        row->addWidget(m_cwPanSlider, 1);

        auto* rLbl = new QLabel(QStringLiteral("R"), page);
        rLbl->setStyleSheet(kDimLabelStyle);
        row->addWidget(rLbl);

        vbox->addLayout(row);
    }

    // ── Mark all CW controls NYI (Phase 3I-2) ────────────────────────────────
    NyiOverlay::markNyi(m_alcGauge,       kNyiCw);
    NyiOverlay::markNyi(m_speedSlider,    kNyiCw);
    NyiOverlay::markNyi(m_pitchDown,      kNyiCw);
    NyiOverlay::markNyi(m_pitchUp,        kNyiCw);
    NyiOverlay::markNyi(m_delaySlider,    kNyiCw);
    NyiOverlay::markNyi(m_sidetoneBtn,    kNyiCw);
    NyiOverlay::markNyi(m_sidetoneSlider, kNyiCw);
    NyiOverlay::markNyi(m_breakinBtn,     kNyiCw);
    NyiOverlay::markNyi(m_iambicBtn,      kNyiCw);
    NyiOverlay::markNyi(m_fwKeyerBtn,     kNyiCw);
    NyiOverlay::markNyi(m_cwPanSlider,    kNyiCw);
}

// ── FM page (8 controls) ──────────────────────────────────────────────────────

void PhoneCwApplet::buildFmPage(QWidget* page)
{
    auto* vbox = new QVBoxLayout(page);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(2);

    // ── Control 1: FM MIC slider (0-100) + inset value ───────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("FM MIC:"), page);
        lbl->setStyleSheet(kDimLabelStyle);
        lbl->setFixedWidth(kLeftColW);
        row->addWidget(lbl);

        m_fmMicSlider = new QSlider(Qt::Horizontal, page);
        m_fmMicSlider->setRange(0, 100);
        m_fmMicSlider->setValue(50);
        m_fmMicSlider->setStyleSheet(kSliderStyle);
        m_fmMicSlider->setAccessibleName(QStringLiteral("FM microphone level"));
        row->addWidget(m_fmMicSlider, 1);

        m_fmMicLabel = new QLabel(QStringLiteral("50"), page);
        m_fmMicLabel->setStyleSheet(kInsetValueStyle);
        m_fmMicLabel->setFixedWidth(kValueW);
        m_fmMicLabel->setAlignment(Qt::AlignCenter);
        row->addWidget(m_fmMicLabel);

        vbox->addLayout(row);
    }

    // ── Control 2: Deviation [5.0k] [2.5k] blue toggles ─────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Dev:"), page);
        lbl->setStyleSheet(kDimLabelStyle);
        lbl->setFixedWidth(28);
        row->addWidget(lbl);

        m_dev5kBtn = new QPushButton(QStringLiteral("5.0k"), page);
        m_dev5kBtn->setCheckable(true);
        m_dev5kBtn->setChecked(true);
        m_dev5kBtn->setFixedHeight(22);
        m_dev5kBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_dev5kBtn->setStyleSheet(QString(kButtonBase) + kBlueActive);
        m_dev5kBtn->setAccessibleName(QStringLiteral("5 kHz deviation"));
        row->addWidget(m_dev5kBtn, 1);

        m_dev25kBtn = new QPushButton(QStringLiteral("2.5k"), page);
        m_dev25kBtn->setCheckable(true);
        m_dev25kBtn->setFixedHeight(22);
        m_dev25kBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_dev25kBtn->setStyleSheet(QString(kButtonBase) + kBlueActive);
        m_dev25kBtn->setAccessibleName(QStringLiteral("2.5 kHz deviation (narrow FM)"));
        row->addWidget(m_dev25kBtn, 1);

        row->addStretch();
        vbox->addLayout(row);
    }

    // ── Control 3: CTCSS enable (green) + tone combo ─────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_ctcssBtn = new QPushButton(QStringLiteral("CTCSS"), page);
        m_ctcssBtn->setCheckable(true);
        m_ctcssBtn->setFixedHeight(22);
        m_ctcssBtn->setFixedWidth(52);
        m_ctcssBtn->setStyleSheet(QString(kButtonBase) + kGreenActive);
        m_ctcssBtn->setAccessibleName(QStringLiteral("CTCSS sub-audible tone squelch"));
        row->addWidget(m_ctcssBtn);

        m_ctcssCombo = new QComboBox(page);
        m_ctcssCombo->addItems(kCtcssTones);
        applyComboStyle(m_ctcssCombo);
        m_ctcssCombo->setAccessibleName(QStringLiteral("CTCSS tone frequency"));
        row->addWidget(m_ctcssCombo, 1);

        vbox->addLayout(row);
    }

    // ── Control 4: Simplex toggle ─────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_simplexBtn = new QPushButton(QStringLiteral("Simplex"), page);
        m_simplexBtn->setCheckable(true);
        m_simplexBtn->setFixedHeight(22);
        m_simplexBtn->setStyleSheet(QString(kButtonBase) + kGreenActive);
        m_simplexBtn->setAccessibleName(QStringLiteral("Simplex (no repeater offset)"));
        row->addWidget(m_simplexBtn);
        row->addStretch();

        vbox->addLayout(row);
    }

    vbox->addSpacing(4);

    // ── Control 5: Repeater offset slider (0-10000 kHz) + inset "600" ────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Offset:"), page);
        lbl->setStyleSheet(kDimLabelStyle);
        lbl->setFixedWidth(kLeftColW);
        row->addWidget(lbl);

        m_rptOffsetSlider = new QSlider(Qt::Horizontal, page);
        m_rptOffsetSlider->setRange(0, 10000);
        m_rptOffsetSlider->setValue(600);
        m_rptOffsetSlider->setStyleSheet(kSliderStyle);
        m_rptOffsetSlider->setAccessibleName(QStringLiteral("Repeater offset (kHz)"));
        row->addWidget(m_rptOffsetSlider, 1);

        m_rptOffsetLabel = new QLabel(QStringLiteral("600"), page);
        m_rptOffsetLabel->setStyleSheet(kInsetValueStyle);
        m_rptOffsetLabel->setFixedWidth(kValueW);
        m_rptOffsetLabel->setAlignment(Qt::AlignCenter);
        row->addWidget(m_rptOffsetLabel);

        vbox->addLayout(row);
    }

    // ── Control 6: Offset direction [-] [+] [Rev] ────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Dir:"), page);
        lbl->setStyleSheet(kDimLabelStyle);
        lbl->setFixedWidth(24);
        row->addWidget(lbl);

        m_offsetMinusBtn = new QPushButton(QStringLiteral("-"), page);
        m_offsetMinusBtn->setCheckable(true);
        m_offsetMinusBtn->setFixedHeight(22);
        m_offsetMinusBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_offsetMinusBtn->setStyleSheet(QString(kButtonBase) + kBlueActive);
        m_offsetMinusBtn->setAccessibleName(QStringLiteral("Negative repeater offset"));
        row->addWidget(m_offsetMinusBtn, 1);

        m_offsetPlusBtn = new QPushButton(QStringLiteral("+"), page);
        m_offsetPlusBtn->setCheckable(true);
        m_offsetPlusBtn->setFixedHeight(22);
        m_offsetPlusBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_offsetPlusBtn->setStyleSheet(QString(kButtonBase) + kBlueActive);
        m_offsetPlusBtn->setAccessibleName(QStringLiteral("Positive repeater offset"));
        row->addWidget(m_offsetPlusBtn, 1);

        m_offsetRevBtn = new QPushButton(QStringLiteral("Rev"), page);
        m_offsetRevBtn->setCheckable(true);
        m_offsetRevBtn->setFixedHeight(22);
        m_offsetRevBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_offsetRevBtn->setStyleSheet(QString(kButtonBase) + kBlueActive);
        m_offsetRevBtn->setAccessibleName(QStringLiteral("Reverse repeater offset"));
        row->addWidget(m_offsetRevBtn, 1);

        vbox->addLayout(row);
    }

    vbox->addSpacing(4);

    // ── Control 7: FM TX Profile combo ───────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Profile:"), page);
        lbl->setStyleSheet(kDimLabelStyle);
        lbl->setFixedWidth(kLeftColW);
        row->addWidget(lbl);

        m_fmProfileCombo = new QComboBox(page);
        m_fmProfileCombo->addItems({QStringLiteral("Default"),
                                    QStringLiteral("Narrow"),
                                    QStringLiteral("Wide")});
        applyComboStyle(m_fmProfileCombo);
        m_fmProfileCombo->setAccessibleName(QStringLiteral("FM TX profile"));
        row->addWidget(m_fmProfileCombo, 1);

        vbox->addLayout(row);
    }

    // ── Control 8: FM Memory combo + ◀ ▶ nav ────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Memory:"), page);
        lbl->setStyleSheet(kDimLabelStyle);
        lbl->setFixedWidth(kLeftColW);
        row->addWidget(lbl);

        m_fmMemCombo = new QComboBox(page);
        m_fmMemCombo->addItem(QStringLiteral("(none)"));
        applyComboStyle(m_fmMemCombo);
        m_fmMemCombo->setAccessibleName(QStringLiteral("FM memory channel"));
        row->addWidget(m_fmMemCombo, 1);

        m_fmMemPrev = new QPushButton(QStringLiteral("\u25c4"), page);
        m_fmMemPrev->setFixedSize(22, 22);
        m_fmMemPrev->setStyleSheet(QString(kButtonBase));
        m_fmMemPrev->setAccessibleName(QStringLiteral("Previous FM memory"));
        row->addWidget(m_fmMemPrev);

        m_fmMemNext = new QPushButton(QStringLiteral("\u25ba"), page);
        m_fmMemNext->setFixedSize(22, 22);
        m_fmMemNext->setStyleSheet(QString(kButtonBase));
        m_fmMemNext->setAccessibleName(QStringLiteral("Next FM memory"));
        row->addWidget(m_fmMemNext);

        vbox->addLayout(row);
    }

    vbox->addStretch();

    // ── Mark all FM controls NYI (Phase 3I-1) ────────────────────────────────
    NyiOverlay::markNyi(m_fmMicSlider,     kNyiFm);
    NyiOverlay::markNyi(m_fmMicLabel,      kNyiFm);
    NyiOverlay::markNyi(m_dev5kBtn,        kNyiFm);
    NyiOverlay::markNyi(m_dev25kBtn,       kNyiFm);
    NyiOverlay::markNyi(m_ctcssBtn,        kNyiFm);
    NyiOverlay::markNyi(m_ctcssCombo,      kNyiFm);
    NyiOverlay::markNyi(m_simplexBtn,      kNyiFm);
    NyiOverlay::markNyi(m_rptOffsetSlider, kNyiFm);
    NyiOverlay::markNyi(m_rptOffsetLabel,  kNyiFm);
    NyiOverlay::markNyi(m_offsetMinusBtn,  kNyiFm);
    NyiOverlay::markNyi(m_offsetPlusBtn,   kNyiFm);
    NyiOverlay::markNyi(m_offsetRevBtn,    kNyiFm);
    NyiOverlay::markNyi(m_fmProfileCombo,  kNyiFm);
    NyiOverlay::markNyi(m_fmMemCombo,      kNyiFm);
    NyiOverlay::markNyi(m_fmMemPrev,       kNyiFm);
    NyiOverlay::markNyi(m_fmMemNext,       kNyiFm);
}

// ── syncFromModel — NYI ───────────────────────────────────────────────────────

void PhoneCwApplet::syncFromModel()
{
    // NYI — wired in Phase 3I-1 (Phone/FM) / Phase 3I-2 (CW)
}

// ── showPage — switch stacked widget to the given page index ─────────────────

void PhoneCwApplet::showPage(int index)
{
    if (m_stack) {
        m_stack->setCurrentIndex(index);
    }
    // Sync the tab selector buttons so they reflect the active page
    if (m_tabGroup) {
        QAbstractButton* btn = m_tabGroup->button(index);
        if (btn && !btn->isChecked()) {
            btn->setChecked(true);
        }
    }
}

} // namespace NereusSDR
