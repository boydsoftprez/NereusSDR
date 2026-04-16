#include "VfoWidget.h"
#include "gui/applets/NyiOverlay.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMenu>
#include <QFontDatabase>
#include <QSignalBlocker>

#include <cmath>
#include <algorithm>

namespace NereusSDR {

// Styles — matching dark theme from CONTRIBUTING.md
static const char* kFlatBtn =
    "QPushButton {"
    "  background: transparent; border: none;"
    "  padding: 1px 4px; font-size: 11px; font-weight: bold;"
    "}";

static const char* kTabBtn =
    "QPushButton {"
    "  background: transparent; border: none;"
    "  color: #6888a0; font-size: 12px; font-weight: bold;"
    "  padding: 2px 6px;"
    "}"
    "QPushButton:checked {"
    "  color: #00b4d8;"
    "  border-bottom: 2px solid #00b4d8;"
    "}";

// From AetherSDR VfoWidget.cpp:158-162 — kDspToggle style
static const char* kDspToggle =
    "QPushButton {"
    "  background: #1a2a3a; border: 1px solid #304050;"
    "  border-radius: 2px; color: #c8d8e8;"
    "  font-size: 13px; font-weight: bold;"
    "  padding: 2px 4px; min-width: 32px;"
    "}"
    "QPushButton:checked {"
    "  background: #1a6030; color: #ffffff;"
    "  border: 1px solid #20a040;"
    "}"
    "QPushButton:hover {"
    "  border: 1px solid #0090e0;"
    "}";

// From VfoStyles.h kModeBtn — blue-checked mode/filter button (AetherSDR pattern)
static const char* kModeBtn =
    "QPushButton {"
    "  background: #1a2a3a; border: 1px solid #304050; border-radius: 2px;"
    "  color: #c8d8e8; font-size: 13px; font-weight: bold; padding: 3px;"
    "}"
    "QPushButton:checked {"
    "  background: #0070c0; color: #ffffff; border: 1px solid #0090e0;"
    "}"
    "QPushButton:hover { border: 1px solid #0090e0; }";

// ---- Construction ----

VfoWidget::VfoWidget(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(kWidgetW);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setMouseTracking(true);

    buildUI();
}

VfoWidget::~VfoWidget()
{
    // Floating buttons are parented to parentWidget() (SpectrumWidget),
    // not to this widget, so Qt's parent chain won't auto-delete them.
    // AetherSDR VfoWidget.cpp:223-233 pattern: explicit delete on destruction.
    // All four pointers are initialized to nullptr in the header, so deletes
    // are safe even if buildFloatingButtons() was never called.
    delete m_closeBtn;
    delete m_lockBtn;
    delete m_recBtn;
    delete m_playBtn;
}

void VfoWidget::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    // From AetherSDR VfoWidget.cpp:237-238 — margins (6, 2, 6, 0)
    mainLayout->setContentsMargins(6, 2, 6, 0);
    mainLayout->setSpacing(2);

    buildHeaderRow();
    buildFrequencyRow();
    buildSmeterRow();
    buildTabBar();

    // Tab content stacked widget — HIDDEN by default (compact flag)
    // From AetherSDR VfoWidget.cpp:545 — m_tabStack->hide()
    m_tabStack = new QStackedWidget(this);
    buildAudioTab();
    buildDspTab();
    buildModeTab();

    buildXRitTab();

    // Stub DAX tab
    auto* daxWidget = new QWidget;
    auto* daxLayout = new QVBoxLayout(daxWidget);
    daxLayout->setContentsMargins(4, 4, 4, 4);
    auto* daxLabel = new QLabel(QStringLiteral("DAX"), daxWidget);
    daxLabel->setStyleSheet(QStringLiteral("color: #6888a0; font-size: 11px;"));
    daxLayout->addWidget(daxLabel);
    m_tabStack->addWidget(daxWidget);

    mainLayout->addWidget(m_tabStack);
    m_tabStack->hide();  // Hidden by default — click tab to expand
    m_activeTab = -1;    // No tab active initially

    setLayout(mainLayout);
    adjustSize();

    // Floating buttons are children of our PARENT (SpectrumWidget)
    // so they render outside the VFO flag bounds. Deferred until
    // first updatePosition() when parentWidget() is available.
}

void VfoWidget::buildHeaderRow()
{
    auto* hdr = new QHBoxLayout;
    hdr->setSpacing(2);
    hdr->setContentsMargins(0, 0, 0, 0);

    // RX antenna button (blue)
    m_rxAntBtn = new QPushButton(QStringLiteral("ANT1"), this);
    m_rxAntBtn->setStyleSheet(QString(kFlatBtn) +
        QStringLiteral("QPushButton { color: #4488ff; }"));
    m_rxAntBtn->setFixedHeight(18);
    m_rxAntBtn->setToolTip(QStringLiteral("Select RX antenna"));
    connect(m_rxAntBtn, &QPushButton::clicked, this, [this]() {
        QMenu menu(this);
        for (const QString& ant : m_antennaList) {
            QAction* act = menu.addAction(ant);
            act->setCheckable(true);
            act->setChecked(ant == m_rxAntBtn->text());
        }
        QAction* sel = menu.exec(m_rxAntBtn->mapToGlobal(
            QPoint(0, m_rxAntBtn->height())));
        if (sel) {
            m_rxAntBtn->setText(sel->text());
            emit rxAntennaChanged(sel->text());
        }
    });
    hdr->addWidget(m_rxAntBtn);

    // TX antenna button (red)
    m_txAntBtn = new QPushButton(QStringLiteral("ANT1"), this);
    m_txAntBtn->setStyleSheet(QString(kFlatBtn) +
        QStringLiteral("QPushButton { color: #ff4444; }"));
    m_txAntBtn->setFixedHeight(18);
    m_txAntBtn->setToolTip(QStringLiteral("Select TX antenna"));
    connect(m_txAntBtn, &QPushButton::clicked, this, [this]() {
        QMenu menu(this);
        for (const QString& ant : m_antennaList) {
            QAction* act = menu.addAction(ant);
            act->setCheckable(true);
            act->setChecked(ant == m_txAntBtn->text());
        }
        QAction* sel = menu.exec(m_txAntBtn->mapToGlobal(
            QPoint(0, m_txAntBtn->height())));
        if (sel) {
            m_txAntBtn->setText(sel->text());
            emit txAntennaChanged(sel->text());
        }
    });
    hdr->addWidget(m_txAntBtn);

    // Filter width label (cyan)
    m_filterWidthLbl = new QLabel(QStringLiteral("2.9K"), this);
    m_filterWidthLbl->setStyleSheet(
        QStringLiteral("color: #00c8ff; font-size: 11px; font-weight: bold;"));
    m_filterWidthLbl->setFixedHeight(18);
    hdr->addWidget(m_filterWidthLbl);

    hdr->addStretch();

    // TX badge
    m_txBadge = new QPushButton(QStringLiteral("TX"), this);
    m_txBadge->setFixedSize(28, 18);
    m_txBadge->setCheckable(true);
    m_txBadge->setStyleSheet(
        QStringLiteral("QPushButton { background: #1a2a3a; border: 1px solid #304050;"
                        "border-radius: 3px; color: #6888a0; font-size: 10px; font-weight: bold; }"
                        "QPushButton:checked { background: #6a3030; border-color: #ff4444; color: #ff8080; }"));
    m_txBadge->setToolTip(QStringLiteral("Indicates this slice is the TX slice"));
    hdr->addWidget(m_txBadge);

    // Split badge — hidden in Stage 1; wired in Stage 2 when split semantics land
    m_splitBadge = new QLabel(QStringLiteral("SPLIT"), this);
    m_splitBadge->setFixedSize(36, 18);
    m_splitBadge->setAlignment(Qt::AlignCenter);
    m_splitBadge->setStyleSheet(
        QStringLiteral("background: #1a2a3a; border: 1px solid #304050;"
                        "border-radius: 3px; color: #6888a0; font-size: 10px; font-weight: bold;"));
    m_splitBadge->setVisible(false);
    hdr->addWidget(m_splitBadge);

    // Slice letter badge
    m_sliceBadge = new QLabel(QStringLiteral("A"), this);
    m_sliceBadge->setFixedSize(18, 18);
    m_sliceBadge->setAlignment(Qt::AlignCenter);
    m_sliceBadge->setStyleSheet(
        QStringLiteral("background: #0070c0; color: white; font-size: 11px;"
                        "font-weight: bold; border-radius: 3px;"));
    hdr->addWidget(m_sliceBadge);

    static_cast<QVBoxLayout*>(layout())->addLayout(hdr);
}

void VfoWidget::buildFrequencyRow()
{
    m_freqStack = new QStackedWidget(this);
    m_freqStack->setFixedHeight(30);

    // Display label
    m_freqLabel = new QLabel(this);
    m_freqLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_freqLabel->setStyleSheet(
        QStringLiteral("color: #00e5ff; font-size: 24px; font-weight: bold;"
                        "font-family: 'Consolas', 'Menlo', monospace;"
                        "background: transparent;"
                        "border: 1px solid rgba(255,255,255,50);"
                        "border-radius: 3px; padding: 0 4px;"));
    updateFreqLabel();
    m_freqStack->addWidget(m_freqLabel);

    // Edit field
    m_freqEdit = new QLineEdit(this);
    m_freqEdit->setAlignment(Qt::AlignRight);
    m_freqEdit->setStyleSheet(
        QStringLiteral("color: #00e5ff; font-size: 20px; font-weight: bold;"
                        "font-family: 'Consolas', 'Menlo', monospace;"
                        "background: #0a0a18; border: 1px solid #00b4d8;"
                        "border-radius: 3px; padding: 0 4px;"));
    connect(m_freqEdit, &QLineEdit::returnPressed, this, [this]() {
        QString text = m_freqEdit->text().replace(QLatin1Char('.'), QString());
        bool ok = false;
        double mhz = text.toDouble(&ok);
        if (ok && mhz > 0.0) {
            // If user typed a small number, assume MHz; otherwise Hz
            double hz = (mhz < 1000.0) ? mhz * 1e6 : mhz;
            hz = std::clamp(hz, 100000.0, 61440000.0);
            m_frequency = hz;
            updateFreqLabel();
            emit frequencyChanged(hz);
        }
        m_freqStack->setCurrentIndex(0);
    });
    connect(m_freqEdit, &QLineEdit::editingFinished, this, [this]() {
        m_freqStack->setCurrentIndex(0);
    });
    m_freqStack->addWidget(m_freqEdit);

    // Double-click on label → edit
    m_freqLabel->installEventFilter(this);

    static_cast<QVBoxLayout*>(layout())->addWidget(m_freqStack);
}

void VfoWidget::buildSmeterRow()
{
    m_levelBar = new VfoLevelBar(this);
    m_levelBar->setValue(float(m_smeterDbm));  // seed with cached value (default -127)
    static_cast<QVBoxLayout*>(layout())->addWidget(m_levelBar);
}

void VfoWidget::buildTabBar()
{
    auto* tabLayout = new QHBoxLayout;
    tabLayout->setSpacing(0);
    tabLayout->setContentsMargins(0, 0, 0, 0);

    // Tab labels — from AetherSDR VfoWidget.cpp:522
    // [🔊] | DSP | USB | X/RIT | DAX
    QStringList tabLabels = {
        QString::fromUtf8("\xF0\x9F\x94\x8A"),  // 🔊 speaker
        QStringLiteral("DSP"),
        SliceModel::modeName(m_currentMode),
        QStringLiteral("X/RIT"),
        QStringLiteral("DAX")
    };

    // Tooltips for each tab button (order matches tabLabels)
    static const char* kTabTooltips[] = {
        "Show/hide audio controls (AF gain, AGC, pan, mute, squelch)",
        "Show/hide DSP controls (NB, NR, ANF, SNB, APF)",
        "Show/hide mode and filter controls",
        "Show/hide RIT/XIT and frequency-lock controls",
        "Show/hide DAX audio routing controls"
    };

    for (int i = 0; i < tabLabels.size(); ++i) {
        // Add separator before each tab except the first
        // From AetherSDR VfoWidget.cpp:523-530
        if (i > 0) {
            auto* sep = new QLabel(QStringLiteral("|"), this);
            sep->setStyleSheet(QStringLiteral(
                "QLabel { background: transparent; border: none;"
                "color: rgba(255,255,255,80); font-size: 13px; padding: 0; }"));
            sep->setFixedWidth(6);
            tabLayout->addWidget(sep);
        }

        auto* btn = new QPushButton(tabLabels[i], this);
        btn->setCheckable(true);
        btn->setStyleSheet(kTabBtn);
        btn->setFixedHeight(24);  // 24px from AetherSDR
        btn->setToolTip(QString::fromLatin1(kTabTooltips[i]));
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            if (m_activeTab == i) {
                // Toggle: clicking active tab hides content
                m_tabStack->hide();
                m_activeTab = -1;
                for (auto* b : m_tabButtons) { b->setChecked(false); }
            } else {
                m_activeTab = i;
                m_tabStack->setCurrentIndex(i);
                m_tabStack->show();
                for (int j = 0; j < m_tabButtons.size(); ++j) {
                    m_tabButtons[j]->setChecked(j == i);
                }
            }
            adjustSize();
            // Notify parent to reposition
            if (parentWidget()) {
                parentWidget()->update();
            }
        });
        tabLayout->addWidget(btn, 1);  // stretch equally
        m_tabButtons.append(btn);
    }

    static_cast<QVBoxLayout*>(layout())->addLayout(tabLayout);
}

void VfoWidget::buildAudioTab()
{
    auto* audioWidget = new QWidget;
    auto* audioLayout = new QVBoxLayout(audioWidget);
    audioLayout->setContentsMargins(4, 4, 4, 4);
    audioLayout->setSpacing(4);

    // 1. AF Gain slider (preserved exactly as-is — already live-wired)
    {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(QStringLiteral("AF"), audioWidget);
        label->setStyleSheet(QStringLiteral("color: #8899aa; font-size: 11px;"));
        label->setFixedWidth(24);
        row->addWidget(label);

        m_afGainSlider = new QSlider(Qt::Horizontal, audioWidget);
        m_afGainSlider->setRange(0, 100);
        m_afGainSlider->setValue(50);
        m_afGainSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
                            "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -3px 0; border-radius: 6px; }"));
        m_afGainSlider->setToolTip(QStringLiteral("Adjust AF gain (audio volume)"));
        row->addWidget(m_afGainSlider);

        m_afGainLabel = new QLabel(QStringLiteral("50"), audioWidget);
        m_afGainLabel->setStyleSheet(QStringLiteral("color: #c8d8e8; font-size: 11px;"));
        m_afGainLabel->setFixedWidth(24);
        m_afGainLabel->setAlignment(Qt::AlignRight);
        row->addWidget(m_afGainLabel);

        connect(m_afGainSlider, &QSlider::valueChanged, this, [this](int val) {
            m_afGainLabel->setText(QString::number(val));
            if (!m_updatingFromModel) {
                emit afGainChanged(val);
            }
        });
        audioLayout->addLayout(row);
    }

    // 2. AGC 5-button row — replaces m_agcCmb (live-wired, no NYI badge)
    {
        static const char* kAgcLabels[] = { "Off", "Long", "Slow", "Med", "Fast" };
        static const char* kAgcTooltips[] = {
            "Select AGC mode: Off (no automatic gain control)",
            "Select AGC mode: Long attack/decay",
            "Select AGC mode: Slow attack/decay",
            "Select AGC mode: Medium attack/decay",
            "Select AGC mode: Fast attack/decay"
        };
        auto* row = new QHBoxLayout;
        row->setSpacing(2);
        row->setContentsMargins(0, 0, 0, 0);
        for (int i = 0; i < 5; ++i) {
            m_agcBtns[i] = new QPushButton(
                QString::fromLatin1(kAgcLabels[i]), audioWidget);
            m_agcBtns[i]->setCheckable(true);
            m_agcBtns[i]->setStyleSheet(kDspToggle);
            m_agcBtns[i]->setToolTip(QString::fromLatin1(kAgcTooltips[i]));
            row->addWidget(m_agcBtns[i]);
        }
        // Default: Med (index 3) — matches AGCMode::Med
        m_agcBtns[3]->setChecked(true);

        // Exclusive toggle: clicking one un-checks the others, emits agcModeChanged
        for (int i = 0; i < 5; ++i) {
            connect(m_agcBtns[i], &QPushButton::clicked, this, [this, i](bool checked) {
                if (!checked) {
                    // Don't allow unchecking; keep it checked
                    m_agcBtns[i]->setChecked(true);
                    return;
                }
                // Uncheck siblings
                for (int j = 0; j < 5; ++j) {
                    if (j != i) {
                        m_agcBtns[j]->setChecked(false);
                    }
                }
                if (!m_updatingFromModel) {
                    emit agcModeChanged(static_cast<AGCMode>(i));
                }
            });
        }
        audioLayout->addLayout(row);
    }

    // 3. Audio pan slider row (NYI)
    {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(QStringLiteral("Pan"), audioWidget);
        label->setStyleSheet(QStringLiteral("color: #8899aa; font-size: 11px;"));
        label->setFixedWidth(24);
        row->addWidget(label);

        m_panSlider = new QSlider(Qt::Horizontal, audioWidget);
        m_panSlider->setRange(-100, 100);
        m_panSlider->setSingleStep(1);
        m_panSlider->setValue(0);
        m_panSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
                            "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -3px 0; border-radius: 6px; }"));
        m_panSlider->setToolTip(QStringLiteral("Audio pan: left/right stereo balance (−100 = full left, 0 = center, +100 = full right)\nFrom Thetis radio.cs:1386 — WDSP patchpanel.c:159"));
        row->addWidget(m_panSlider);

        m_panLabel = new QLabel(QStringLiteral("0"), audioWidget);
        m_panLabel->setStyleSheet(QStringLiteral("color: #c8d8e8; font-size: 11px;"));
        m_panLabel->setFixedWidth(24);
        m_panLabel->setAlignment(Qt::AlignRight);
        row->addWidget(m_panLabel);

        connect(m_panSlider, &QSlider::valueChanged, this, [this](int val) {
            m_panLabel->setText(QString::number(val));
            if (!m_updatingFromModel) {
                emit panChanged(val / 100.0);
            }
        });
        audioLayout->addLayout(row);
    }

    // 4. Mute + BIN row (NYI)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_muteBtn = new QPushButton(QStringLiteral("Mute"), audioWidget);
        m_muteBtn->setCheckable(true);
        m_muteBtn->setStyleSheet(kDspToggle);
        m_muteBtn->setToolTip(QStringLiteral("Mute RX audio output (SetRXAPanelRun)\nFrom Thetis dsp.cs:393 — WDSP patchpanel.c:126"));
        row->addWidget(m_muteBtn);

        m_binBtn = new QPushButton(QStringLiteral("BIN"), audioWidget);
        m_binBtn->setCheckable(true);
        m_binBtn->setStyleSheet(kDspToggle);
        m_binBtn->setToolTip(QStringLiteral("Binaural audio: I/Q channels separate for headphone stereo image (SetRXAPanelBinaural)\nFrom Thetis radio.cs:1145 — WDSP patchpanel.c:187"));
        row->addWidget(m_binBtn);

        row->addStretch();

        connect(m_muteBtn, &QPushButton::toggled, this, [this](bool on) {
            if (!m_updatingFromModel) {
                emit muteChanged(on);
            }
        });
        connect(m_binBtn, &QPushButton::toggled, this, [this](bool on) {
            if (!m_updatingFromModel) {
                emit binauralChanged(on);
            }
        });
        audioLayout->addLayout(row);
    }

    // 5. Squelch row — SQL toggle + SQL threshold slider (NYI)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_sqlBtn = new QPushButton(QStringLiteral("SQL"), audioWidget);
        m_sqlBtn->setCheckable(true);
        m_sqlBtn->setStyleSheet(kDspToggle);
        m_sqlBtn->setFixedWidth(40);
        m_sqlBtn->setToolTip(QStringLiteral("Toggle squelch. SSB uses syllabic squelch; AM/FM use separate squelch engines."));
        row->addWidget(m_sqlBtn);

        m_sqlSlider = new QSlider(Qt::Horizontal, audioWidget);
        m_sqlSlider->setRange(0, 100);
        m_sqlSlider->setSingleStep(1);
        m_sqlSlider->setValue(0);
        m_sqlSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
                            "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -3px 0; border-radius: 6px; }"));
        m_sqlSlider->setToolTip(QStringLiteral("Squelch threshold. SSB: 0–100 maps to 0.0–1.0 linear. AM: dB scale. FM: linear 0–1."));
        row->addWidget(m_sqlSlider);

        connect(m_sqlBtn, &QPushButton::toggled, this, [this](bool on) {
            if (!m_updatingFromModel) {
                emit squelchEnabledChanged(on);
            }
        });
        connect(m_sqlSlider, &QSlider::valueChanged, this, [this](int val) {
            if (!m_updatingFromModel) {
                emit squelchThreshChanged(val);
            }
        });
        audioLayout->addLayout(row);
        // Squelch button and slider are live-wired — no NYI badge
    }

    // 6. AGC threshold slider row
    // From Thetis Project Files/Source/Console/console.cs:45977 — agc_thresh_point, range -160..0
    {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(QStringLiteral("AGC-T"), audioWidget);
        label->setStyleSheet(QStringLiteral("color: #8899aa; font-size: 11px;"));
        label->setFixedWidth(40);
        row->addWidget(label);

        m_agcTSlider = new QSlider(Qt::Horizontal, audioWidget);
        m_agcTSlider->setRange(-160, 0);
        m_agcTSlider->setSingleStep(1);
        m_agcTSlider->setValue(-20);
        m_agcTSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
                            "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -3px 0; border-radius: 6px; }"));
        m_agcTSlider->setToolTip(QStringLiteral("AGC threshold (dBu) — adjusts the level at which AGC begins to act"));
        row->addWidget(m_agcTSlider);

        m_agcTLabel = new QLabel(QStringLiteral("-20"), audioWidget);
        m_agcTLabel->setStyleSheet(QStringLiteral("color: #c8d8e8; font-size: 11px;"));
        m_agcTLabel->setFixedWidth(32);
        m_agcTLabel->setAlignment(Qt::AlignRight);
        row->addWidget(m_agcTLabel);

        connect(m_agcTSlider, &QSlider::valueChanged, this, [this](int val) {
            m_agcTLabel->setText(QString::number(val));
            if (!m_updatingFromModel) {
                emit agcThreshChanged(val);
            }
        });
        audioLayout->addLayout(row);
    }

    audioLayout->addStretch();
    m_tabStack->addWidget(audioWidget);
}

void VfoWidget::buildDspTab()
{
    auto* dspWidget = new QWidget;
    auto* dspLayout = new QVBoxLayout(dspWidget);
    dspLayout->setContentsMargins(4, 4, 4, 4);
    dspLayout->setSpacing(4);

    // 4×2 grid of DSP toggles
    auto* grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(2);

    auto makeToggle = [dspWidget](const QString& label) -> QPushButton* {
        auto* btn = new QPushButton(label, dspWidget);
        btn->setCheckable(true);
        btn->setStyleSheet(kDspToggle);
        return btn;
    };

    // Row 0: NB | NB2 | NR | NR2
    m_nb1Toggle = makeToggle(QStringLiteral("NB"));
    m_nb1Toggle->setToolTip(QStringLiteral("Toggle noise blanker (NB1)"));
    m_nb2Toggle = makeToggle(QStringLiteral("NB2"));
    m_nb2Toggle->setToolTip(QStringLiteral("NB2: Impulse Noise Blanker (xnobEXTF — pre-WDSP chain)\nFrom Thetis specHPSDR.cs:937 — WDSP nobII.c:649"));
    m_nrToggle  = makeToggle(QStringLiteral("NR"));
    m_nrToggle->setToolTip(QStringLiteral("Toggle noise reduction (NR)"));
    m_nr2Toggle = makeToggle(QStringLiteral("NR2"));
    m_nr2Toggle->setToolTip(QStringLiteral("Toggle enhanced multiband noise reduction (NR2/EMNR)"));
    grid->addWidget(m_nb1Toggle, 0, 0);
    grid->addWidget(m_nb2Toggle, 0, 1);
    grid->addWidget(m_nrToggle,  0, 2);
    grid->addWidget(m_nr2Toggle, 0, 3);

    // Row 1: ANF | SNB | APF | (spacer — col 3 intentionally empty per plan §S1.8.4)
    m_anfToggle = makeToggle(QStringLiteral("ANF"));
    m_anfToggle->setToolTip(QStringLiteral("Toggle automatic notch filter (ANF)"));
    m_snbToggle = makeToggle(QStringLiteral("SNB"));
    m_snbToggle->setToolTip(QStringLiteral("Toggle spectral noise blanker (SNB)"));
    m_apfToggle = makeToggle(QStringLiteral("APF"));
    m_apfToggle->setToolTip(QStringLiteral("Toggle audio peak filter (APF) — CW narrow-peak filter"));
    grid->addWidget(m_anfToggle, 1, 0);
    grid->addWidget(m_snbToggle, 1, 1);
    grid->addWidget(m_apfToggle, 1, 2);

    dspLayout->addLayout(grid);

    // APF tune slider row — below the grid
    {
        auto* apfRow = new QHBoxLayout;
        apfRow->setSpacing(4);

        m_apfLabel = new QLabel(QStringLiteral("APF"), dspWidget);
        m_apfLabel->setStyleSheet(QStringLiteral("color: #8899aa; font-size: 11px;"));
        m_apfLabel->setFixedWidth(24);
        apfRow->addWidget(m_apfLabel);

        m_apfTuneSlider = new QSlider(Qt::Horizontal, dspWidget);
        m_apfTuneSlider->setRange(-500, 500);
        m_apfTuneSlider->setSingleStep(1);
        m_apfTuneSlider->setValue(0);
        m_apfTuneSlider->setToolTip(QStringLiteral("APF tune offset from CW pitch (600 Hz ± offset). From Thetis setup.cs:17068"));
        apfRow->addWidget(m_apfTuneSlider);

        m_apfTuneLabel = new QLabel(QStringLiteral("0 Hz"), dspWidget);
        m_apfTuneLabel->setStyleSheet(QStringLiteral("color: #8899aa; font-size: 11px;"));
        m_apfTuneLabel->setFixedWidth(44);
        m_apfTuneLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        apfRow->addWidget(m_apfTuneLabel);

        dspLayout->addLayout(apfRow);
    }

    // Mode containers — embedded, hidden by default (S1.9 wires visibility)
    m_fmContainer = new FmOptContainer(dspWidget);
    m_fmContainer->setVisible(false);
    dspLayout->addWidget(m_fmContainer);

    m_digContainer = new DigOffsetContainer(dspWidget);
    m_digContainer->setVisible(false);
    dspLayout->addWidget(m_digContainer);

    m_rttyContainer = new RttyMarkShiftContainer(dspWidget);
    m_rttyContainer->setVisible(false);
    dspLayout->addWidget(m_rttyContainer);

    // If m_slice was set before buildUI ran, forward it to the containers now.
    if (m_slice) {
        m_fmContainer->setSlice(m_slice.data());
        m_digContainer->setSlice(m_slice.data());
        m_rttyContainer->setSlice(m_slice.data());
    }

    // Signal wiring for the 7 toggles
    connect(m_nb1Toggle, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) { emit nb1Changed(on); }
    });
    connect(m_nb2Toggle, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) { emit nb2Changed(on); }
    });
    connect(m_nrToggle, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) { emit nrChanged(on); }
    });
    connect(m_nr2Toggle, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) { emit nr2Changed(on); }
    });
    connect(m_anfToggle, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) { emit anfChanged(on); }
    });
    connect(m_snbToggle, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) { emit snbChanged(on); }
    });
    connect(m_apfToggle, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) { emit apfChanged(on); }
        // Re-evaluate APF slider visibility regardless of source (S1.9)
        applyModeVisibility(m_currentMode);
    });

    // APF tune slider — label updates always; emit only when user-driven
    connect(m_apfTuneSlider, &QSlider::valueChanged, this, [this](int hz) {
        m_apfTuneLabel->setText(QString::number(hz) + QStringLiteral(" Hz"));
        if (!m_updatingFromModel) { emit apfTuneHzChanged(hz); }
    });

    // NYI badges — NB1, NB2, NR, ANF, NR2, SNB, APF are live-wired (no badge); remaining controls get badges
    NyiOverlay::markNyi(m_fmContainer,    QStringLiteral("phase3g10-stage2"));
    NyiOverlay::markNyi(m_digContainer,   QStringLiteral("phase3g10-stage2"));
    NyiOverlay::markNyi(m_rttyContainer,  QStringLiteral("phase3g10-stage2"));

    m_tabStack->addWidget(dspWidget);
}

void VfoWidget::buildModeTab()
{
    auto* modeWidget = new QWidget;
    auto* modeLayout = new QVBoxLayout(modeWidget);
    modeLayout->setContentsMargins(4, 4, 4, 4);
    modeLayout->setSpacing(4);

    // Mode combo
    {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(QStringLiteral("Mode"), modeWidget);
        label->setStyleSheet(QStringLiteral("color: #8899aa; font-size: 11px;"));
        label->setFixedWidth(32);
        row->addWidget(label);

        m_modeCmb = new QComboBox(modeWidget);
        m_modeCmb->setToolTip(QStringLiteral("Select demodulation mode"));
        // From Thetis enums.cs DSPMode — common modes
        m_modeCmb->addItems({
            QStringLiteral("LSB"), QStringLiteral("USB"),
            QStringLiteral("AM"), QStringLiteral("CWL"),
            QStringLiteral("CWU"), QStringLiteral("FM"),
            QStringLiteral("DIGU"), QStringLiteral("DIGL"),
            QStringLiteral("SAM")
        });
        m_modeCmb->setCurrentText(QStringLiteral("USB"));
        m_modeCmb->setStyleSheet(
            QStringLiteral("QComboBox { background: #1a2a3a; color: #c8d8e8;"
                            "border: 1px solid #304050; border-radius: 3px;"
                            "padding: 1px 4px; font-size: 11px; }"
                            "QComboBox::drop-down { border: none; }"
                            "QComboBox QAbstractItemView { background: #1a2a3a; color: #c8d8e8;"
                            "selection-background-color: #0070c0; }"));
        connect(m_modeCmb, &QComboBox::currentTextChanged,
                this, [this](const QString& text) {
            if (!m_updatingFromModel) {
                DSPMode mode = SliceModel::modeFromName(text);
                m_currentMode = mode;
                applyModeVisibility(mode);    // S1.9 — user-driven mode change
                rebuildFilterButtons(mode);
                // Update mode tab label
                if (m_tabButtons.size() > 2) {
                    m_tabButtons[2]->setText(text);
                }
                emit modeChanged(mode);
            }
        });
        row->addWidget(m_modeCmb);
        modeLayout->addLayout(row);
    }

    // Quick-mode shortcut buttons (NYI — Stage 2 maps index → configurable DSPMode)
    {
        static const char* kQmLabels[] = { "USB", "CW", "DIG" };
        static const char* kQmTooltips[] = {
            "Quick-select USB mode — not yet implemented",
            "Quick-select CW mode — not yet implemented",
            "Quick-select DIG mode — not yet implemented"
        };
        auto* row = new QHBoxLayout;
        row->setSpacing(2);
        row->setContentsMargins(0, 0, 0, 0);
        for (int i = 0; i < 3; ++i) {
            m_quickModeBtns[i] = new QPushButton(
                QString::fromLatin1(kQmLabels[i]), modeWidget);
            m_quickModeBtns[i]->setCheckable(true);
            m_quickModeBtns[i]->setStyleSheet(kModeBtn);
            m_quickModeBtns[i]->setToolTip(QString::fromLatin1(kQmTooltips[i]));
            connect(m_quickModeBtns[i], &QPushButton::clicked, this, [this, i]() {
                if (!m_updatingFromModel) {
                    emit quickModeRequested(i);
                }
            });
            row->addWidget(m_quickModeBtns[i]);
            NyiOverlay::markNyi(m_quickModeBtns[i], QStringLiteral("phase3g10-stage2"));
        }
        modeLayout->addLayout(row);
    }

    // Filter preset buttons (dynamic per mode)
    m_filterBtnContainer = new QWidget(modeWidget);
    modeLayout->addWidget(m_filterBtnContainer);
    rebuildFilterButtons(DSPMode::USB);

    // RF Gain slider
    {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(QStringLiteral("RF"), modeWidget);
        label->setStyleSheet(QStringLiteral("color: #8899aa; font-size: 11px;"));
        label->setFixedWidth(24);
        row->addWidget(label);

        m_rfGainSlider = new QSlider(Qt::Horizontal, modeWidget);
        m_rfGainSlider->setRange(0, 100);
        m_rfGainSlider->setValue(80);
        m_rfGainSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
                            "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -3px 0; border-radius: 6px; }"));
        m_rfGainSlider->setToolTip(QStringLiteral("Adjust RF gain"));
        row->addWidget(m_rfGainSlider);

        m_rfGainLabel = new QLabel(QStringLiteral("80"), modeWidget);
        m_rfGainLabel->setStyleSheet(QStringLiteral("color: #c8d8e8; font-size: 11px;"));
        m_rfGainLabel->setFixedWidth(24);
        m_rfGainLabel->setAlignment(Qt::AlignRight);
        row->addWidget(m_rfGainLabel);

        connect(m_rfGainSlider, &QSlider::valueChanged, this, [this](int val) {
            m_rfGainLabel->setText(QString::number(val));
            if (!m_updatingFromModel) {
                emit rfGainChanged(val);
            }
        });
        modeLayout->addLayout(row);
    }

    modeLayout->addStretch();
    m_tabStack->addWidget(modeWidget);
}

void VfoWidget::buildXRitTab()
{
    // NereusSDR native X/RIT tab — AetherSDR pattern, control surfaces are native
    // (per feedback_source_first_ui_vs_dsp.md). DSP state is all stubs from S1.6;
    // Stage 2 wires the WDSP/SliceModel calls and removes NYI badges.
    auto* ritWidget = new QWidget;
    auto* vbox = new QVBoxLayout(ritWidget);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(2);

    static const char* kZeroBtn =
        "QPushButton {"
        "  background: #1a2a3a; border: 1px solid #304050; border-radius: 2px;"
        "  color: #c8d8e8; font-size: 12px; font-weight: bold; padding: 1px;"
        "}"
        "QPushButton:hover { border: 1px solid #0090e0; }";

    // --- RIT row ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_ritBtn = new QPushButton(QStringLiteral("RIT"), ritWidget);
        m_ritBtn->setCheckable(true);
        m_ritBtn->setStyleSheet(kDspToggle);
        m_ritBtn->setFixedHeight(22);
        m_ritBtn->setToolTip(QStringLiteral("RIT — Receive Incremental Tuning: shifts demodulation frequency without retuning hardware VFO"));
        row->addWidget(m_ritBtn);

        m_ritLabel = new ScrollableLabel(ritWidget);
        m_ritLabel->setRange(-10000, 10000);
        m_ritLabel->setStep(m_stepHz);
        m_ritLabel->setValue(0);
        m_ritLabel->setFormat([](int v) {
            return QString::asprintf("%+d Hz", v);
        });
        row->addWidget(m_ritLabel, 1);

        m_ritZeroBtn = new QPushButton(QStringLiteral("0"), ritWidget);
        m_ritZeroBtn->setFixedWidth(20);
        m_ritZeroBtn->setFlat(true);
        m_ritZeroBtn->setStyleSheet(kZeroBtn);
        m_ritZeroBtn->setToolTip(QStringLiteral("Zero RIT offset"));
        row->addWidget(m_ritZeroBtn);

        vbox->addLayout(row);
    }

    // --- XIT row ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_xitBtn = new QPushButton(QStringLiteral("XIT"), ritWidget);
        m_xitBtn->setCheckable(true);
        m_xitBtn->setStyleSheet(kDspToggle);
        m_xitBtn->setFixedHeight(22);
        m_xitBtn->setToolTip(QStringLiteral("XIT — not yet implemented"));
        row->addWidget(m_xitBtn);

        m_xitLabel = new ScrollableLabel(ritWidget);
        m_xitLabel->setRange(-10000, 10000);
        m_xitLabel->setStep(m_stepHz);
        m_xitLabel->setValue(0);
        m_xitLabel->setFormat([](int v) {
            return QString::asprintf("%+d Hz", v);
        });
        row->addWidget(m_xitLabel, 1);

        m_xitZeroBtn = new QPushButton(QStringLiteral("0"), ritWidget);
        m_xitZeroBtn->setFixedWidth(20);
        m_xitZeroBtn->setFlat(true);
        m_xitZeroBtn->setStyleSheet(kZeroBtn);
        m_xitZeroBtn->setToolTip(QStringLiteral("Zero XIT offset — not yet implemented"));
        row->addWidget(m_xitZeroBtn);

        vbox->addLayout(row);
    }

    // --- Bottom row: LOCK + STEP cycle ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_xritLockBtn = new QPushButton(QStringLiteral("LOCK"), ritWidget);
        m_xritLockBtn->setCheckable(true);
        m_xritLockBtn->setStyleSheet(kDspToggle);
        m_xritLockBtn->setFixedHeight(22);
        m_xritLockBtn->setToolTip(QStringLiteral("Lock VFO frequency to prevent accidental tuning"));
        row->addWidget(m_xritLockBtn);

        // Step cycle button — NOT NYI (wires to live SliceModel::setStepHz)
        m_stepCycleBtn = new QPushButton(
            QStringLiteral("%1 Hz").arg(m_stepHz), ritWidget);
        m_stepCycleBtn->setFlat(true);
        m_stepCycleBtn->setStyleSheet(
            QStringLiteral("QPushButton {"
                           "  background: #1a2a3a; border: 1px solid #304050; border-radius: 2px;"
                           "  color: #c8d8e8; font-size: 11px; font-weight: bold; padding: 2px 4px;"
                           "}"
                           "QPushButton:hover { border: 1px solid #0090e0; }"));
        m_stepCycleBtn->setFixedHeight(22);
        m_stepCycleBtn->setToolTip(QStringLiteral("Cycle tuning step size (click to advance to next step)"));
        row->addWidget(m_stepCycleBtn, 1);

        vbox->addLayout(row);
    }

    vbox->addStretch();

    // --- Signal wiring ---
    connect(m_ritBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) {
            emit ritEnabledChanged(on);
        }
    });

    connect(m_ritLabel, &ScrollableLabel::valueChanged, this, [this](int hz) {
        if (!m_updatingFromModel) {
            emit ritHzChanged(hz);
        }
    });

    connect(m_ritZeroBtn, &QPushButton::clicked, this, [this]() {
        m_ritLabel->setValue(0);
        if (!m_updatingFromModel) {
            emit ritHzChanged(0);
        }
    });

    connect(m_xitBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) {
            emit xitEnabledChanged(on);
        }
    });

    connect(m_xitLabel, &ScrollableLabel::valueChanged, this, [this](int hz) {
        if (!m_updatingFromModel) {
            emit xitHzChanged(hz);
        }
    });

    connect(m_xitZeroBtn, &QPushButton::clicked, this, [this]() {
        m_xitLabel->setValue(0);
        if (!m_updatingFromModel) {
            emit xitHzChanged(0);
        }
    });

    connect(m_xritLockBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) {
            applyLockedState(on);
        }
    });

    connect(m_stepCycleBtn, &QPushButton::clicked, this, [this]() {
        emit stepCycleRequested();
    });

    // RIT controls are live — no NYI badge.
    // XIT stored for 3M-1 (TX phase); keep NYI badge with TX note.
    NyiOverlay::markNyi(m_xitBtn,      QStringLiteral("XIT — TX gated by Phase 3M-1"));
    NyiOverlay::markNyi(m_xitLabel,    QStringLiteral("XIT — TX gated by Phase 3M-1"));
    NyiOverlay::markNyi(m_xitZeroBtn,  QStringLiteral("XIT — TX gated by Phase 3M-1"));
    // LOCK is live in S2.9 — no NYI badge.

    m_tabStack->addWidget(ritWidget);
}

void VfoWidget::rebuildFilterButtons(DSPMode mode)
{
    // Remove old layout and buttons
    if (m_filterBtnContainer->layout()) {
        QLayoutItem* item;
        while ((item = m_filterBtnContainer->layout()->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete m_filterBtnContainer->layout();
    }

    auto* grid = new QHBoxLayout(m_filterBtnContainer);
    grid->setSpacing(2);
    grid->setContentsMargins(0, 0, 0, 0);

    // Per-mode filter presets — ported from Thetis console.cs:5180-5575
    // Showing a selection of useful widths for each mode family
    struct Preset { const char* label; int low; int high; };

    QVector<Preset> presets;
    switch (mode) {
    case DSPMode::LSB:
        // From Thetis console.cs:5191-5231 (LSB F1-F10)
        presets = {{"5.0K",-5100,-100}, {"3.8K",-3900,-100}, {"2.9K",-3000,-100},
                   {"2.7K",-2800,-100}, {"2.4K",-2500,-100}, {"1.8K",-1900,-100}};
        break;
    case DSPMode::USB:
        // From Thetis console.cs:5233-5273 (USB F1-F10)
        presets = {{"5.0K",100,5100}, {"3.8K",100,3900}, {"2.9K",100,3000},
                   {"2.7K",100,2800}, {"2.4K",100,2500}, {"1.8K",100,1900}};
        break;
    case DSPMode::CWL:
    case DSPMode::CWU: {
        // From Thetis console.cs:5359-5441 — centered on cw_pitch (600 Hz)
        int sign = (mode == DSPMode::CWL) ? -1 : 1;
        int p = 600;  // From Thetis display.cs:1023
        presets = {{"1.0K", sign*(p-500), sign*(p+500)},
                   {"500",  sign*(p-250), sign*(p+250)},
                   {"400",  sign*(p-200), sign*(p+200)},
                   {"250",  sign*(p-125), sign*(p+125)},
                   {"100",  sign*(p-50),  sign*(p+50)}};
        break;
    }
    case DSPMode::AM:
    case DSPMode::SAM:
        // From Thetis console.cs:5443-5525 (AM/SAM F1-F10)
        presets = {{"20K",-10000,10000}, {"10K",-5000,5000}, {"8.0K",-4000,4000},
                   {"6.0K",-3000,3000}, {"5.0K",-2500,2500}};
        break;
    case DSPMode::FM:
        presets = {{"16K",-8000,8000}, {"12K",-6000,6000}, {"8.0K",-4000,4000}};
        break;
    case DSPMode::DIGU: {
        // From Thetis console.cs:5317-5357, offset=1500
        int o = 1500;
        presets = {{"3.0K",o-1500,o+1500}, {"2.0K",o-1000,o+1000},
                   {"1.0K",o-500,o+500}, {"600",o-300,o+300}};
        break;
    }
    case DSPMode::DIGL: {
        // From Thetis console.cs:5275-5315, offset=2210
        int o = 2210;
        presets = {{"3.0K",-(o+1500),-(o-1500)}, {"2.0K",-(o+1000),-(o-1000)},
                   {"1.0K",-(o+500),-(o-500)}, {"600",-(o+300),-(o-300)}};
        break;
    }
    default:
        presets = {{"10K",-5000,5000}, {"6.0K",-3000,3000}};
        break;
    }

    auto [defLow, defHigh] = SliceModel::defaultFilterForMode(mode);

    for (const auto& p : presets) {
        auto* btn = new QPushButton(QString::fromLatin1(p.label), m_filterBtnContainer);
        btn->setCheckable(true);
        btn->setStyleSheet(kModeBtn);
        btn->setFixedHeight(26);
        int low = p.low;
        int high = p.high;
        btn->setProperty("filterLow", low);
        btn->setProperty("filterHigh", high);
        // Tooltip: show the filter edges so the user knows what they're selecting
        btn->setToolTip(QStringLiteral("Select filter preset: %1 Hz to %2 Hz")
            .arg(low).arg(high));
        // Check if this matches current filter
        if (low == defLow && high == defHigh) {
            btn->setChecked(true);
        }
        // Exclusive toggle: click selects this preset, emits filterChanged
        connect(btn, &QPushButton::clicked, this, [this, low, high, btn](bool checked) {
            if (!checked) {
                // Don't allow unchecking the active preset — keep it toggled on
                btn->setChecked(true);
                return;
            }
            // Uncheck all other filter buttons (exclusive group)
            for (auto* child : m_filterBtnContainer->findChildren<QPushButton*>()) {
                if (child != btn) {
                    child->setChecked(false);
                }
            }
            if (!m_updatingFromModel) {
                emit filterChanged(low, high);
            }
        });
        grid->addWidget(btn);
    }

    m_filterBtnContainer->setLayout(grid);
}

// ---- State setters (guarded) ----

void VfoWidget::setFrequency(double hz)
{
    m_updatingFromModel = true;
    m_frequency = hz;
    updateFreqLabel();
    m_updatingFromModel = false;
}

void VfoWidget::setMode(DSPMode mode)
{
    m_updatingFromModel = true;
    m_currentMode = mode;
    QString name = SliceModel::modeName(mode);
    m_modeCmb->setCurrentText(name);
    if (m_tabButtons.size() > 2) {
        m_tabButtons[2]->setText(name);
    }
    rebuildFilterButtons(mode);
    applyModeVisibility(mode);    // S1.9 — model-driven mode change
    m_updatingFromModel = false;
}

void VfoWidget::setFilter(int low, int high)
{
    m_updatingFromModel = true;
    m_filterWidthLbl->setText(formatFilterWidth(low, high));
    // Update checked state of filter buttons — match by stored property
    for (auto* btn : m_filterBtnContainer->findChildren<QPushButton*>()) {
        int bLow = btn->property("filterLow").toInt();
        int bHigh = btn->property("filterHigh").toInt();
        btn->setChecked(bLow == low && bHigh == high);
    }
    m_updatingFromModel = false;
}

void VfoWidget::setAgcMode(AGCMode mode)
{
    m_updatingFromModel = true;
    int idx = static_cast<int>(mode);
    for (int i = 0; i < 5; ++i) {
        if (m_agcBtns[i]) {
            m_agcBtns[i]->setChecked(i == idx);
        }
    }
    m_updatingFromModel = false;
}

void VfoWidget::setAfGain(int gain)
{
    m_updatingFromModel = true;
    m_afGainSlider->setValue(gain);
    m_afGainLabel->setText(QString::number(gain));
    m_updatingFromModel = false;
}

void VfoWidget::setRfGain(int gain)
{
    m_updatingFromModel = true;
    m_rfGainSlider->setValue(gain);
    m_rfGainLabel->setText(QString::number(gain));
    m_updatingFromModel = false;
}

void VfoWidget::setRxAntenna(const QString& ant)
{
    m_updatingFromModel = true;
    m_rxAntBtn->setText(ant);
    m_updatingFromModel = false;
}

void VfoWidget::setTxAntenna(const QString& ant)
{
    m_updatingFromModel = true;
    m_txAntBtn->setText(ant);
    m_updatingFromModel = false;
}

void VfoWidget::setStepHz(int hz)
{
    m_stepHz = hz;
    if (m_ritLabel) {
        m_ritLabel->setStep(hz);
    }
    if (m_xitLabel) {
        m_xitLabel->setStep(hz);
    }
    if (m_stepCycleBtn) {
        m_stepCycleBtn->setText(QStringLiteral("%1 Hz").arg(hz));
    }
}

void VfoWidget::setSliceIndex(int index)
{
    m_sliceIndex = index;
    static const QChar letters[] = {'A', 'B', 'C', 'D'};
    if (index >= 0 && index < 4) {
        m_sliceBadge->setText(QString(letters[index]));
        QColor c = sliceColor(index);
        m_sliceBadge->setStyleSheet(
            QStringLiteral("background: %1; color: white; font-size: 11px;"
                            "font-weight: bold; border-radius: 3px;").arg(c.name()));
    }
}

void VfoWidget::setTxSlice(bool isTx)
{
    m_txBadge->setChecked(isTx);
}

void VfoWidget::setAntennaList(const QStringList& ants)
{
    m_antennaList = ants;
}

void VfoWidget::setSmeter(double dbm)
{
    m_smeterDbm = dbm;
    if (m_levelBar) {
        m_levelBar->setValue(float(dbm));
    }
}

void VfoWidget::setRitEnabled(bool v)
{
    if (m_ritBtn && m_ritBtn->isChecked() != v) {
        m_updatingFromModel = true;
        m_ritBtn->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setRitHz(int hz)
{
    if (m_ritLabel && m_ritLabel->value() != hz) {
        m_updatingFromModel = true;
        m_ritLabel->setValue(hz);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setXitEnabled(bool v)
{
    if (m_xitBtn && m_xitBtn->isChecked() != v) {
        m_updatingFromModel = true;
        m_xitBtn->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setXitHz(int hz)
{
    if (m_xitLabel && m_xitLabel->value() != hz) {
        m_updatingFromModel = true;
        m_xitLabel->setValue(hz);
        m_updatingFromModel = false;
    }
}

// ---- DSP tab state setters (S1.8b) ----

void VfoWidget::setNb2Enabled(bool v)
{
    if (m_nb2Toggle && m_nb2Toggle->isChecked() != v) {
        m_updatingFromModel = true;
        m_nb2Toggle->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setNr2Enabled(bool v)
{
    if (m_nr2Toggle && m_nr2Toggle->isChecked() != v) {
        m_updatingFromModel = true;
        m_nr2Toggle->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setSnbEnabled(bool v)
{
    if (m_snbToggle && m_snbToggle->isChecked() != v) {
        m_updatingFromModel = true;
        m_snbToggle->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setApfEnabled(bool v)
{
    if (m_apfToggle && m_apfToggle->isChecked() != v) {
        m_updatingFromModel = true;
        m_apfToggle->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setApfTuneHz(int hz)
{
    if (m_apfTuneSlider && m_apfTuneSlider->value() != hz) {
        m_updatingFromModel = true;
        m_apfTuneSlider->setValue(hz);
        m_updatingFromModel = false;
    }
}

// ---- Mode container visibility (S1.9) ----

void VfoWidget::applyModeVisibility(DSPMode mode)
{
    // Mode containers embedded in DspTab — show only the one matching
    // the active demodulation mode.
    if (m_fmContainer) {
        m_fmContainer->setVisible(mode == DSPMode::FM);
    }
    if (m_digContainer) {
        m_digContainer->setVisible(mode == DSPMode::DIGL || mode == DSPMode::DIGU);
    }
    if (m_rttyContainer) {
        // RTTY is a DIGL sub-mode — mark/shift controls shown alongside DIG offset
        m_rttyContainer->setVisible(mode == DSPMode::DIGL);
    }

    // APF tune slider + row label — visible only when APF is enabled AND mode is CW
    bool apfVisible = (m_apfToggle && m_apfToggle->isChecked())
                      && (mode == DSPMode::CWL || mode == DSPMode::CWU);
    if (m_apfLabel) {
        m_apfLabel->setVisible(apfVisible);
    }
    if (m_apfTuneSlider) {
        m_apfTuneSlider->setVisible(apfVisible);
    }
    if (m_apfTuneLabel) {
        m_apfTuneLabel->setVisible(apfVisible);
    }
}

// ---- Audio tab state setters (S1.8c — guarded against re-emit) ----

void VfoWidget::setMuted(bool v)
{
    if (m_muteBtn && m_muteBtn->isChecked() != v) {
        m_updatingFromModel = true;
        m_muteBtn->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setAudioPan(double pan)
{
    if (m_panSlider) {
        int val = static_cast<int>(std::round(pan * 100.0));
        if (m_panSlider->value() != val) {
            m_updatingFromModel = true;
            m_panSlider->setValue(val);
            if (m_panLabel) {
                m_panLabel->setText(QString::number(val));
            }
            m_updatingFromModel = false;
        }
    }
}

void VfoWidget::setSsqlEnabled(bool v)
{
    if (m_sqlBtn && m_sqlBtn->isChecked() != v) {
        m_updatingFromModel = true;
        m_sqlBtn->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setSsqlThresh(double dB)
{
    if (m_sqlSlider) {
        int val = static_cast<int>(std::round(dB));
        val = std::max(0, std::min(100, val));
        if (m_sqlSlider->value() != val) {
            m_updatingFromModel = true;
            m_sqlSlider->setValue(val);
            m_updatingFromModel = false;
        }
    }
}

void VfoWidget::setAgcThreshold(int dBu)
{
    if (m_agcTSlider) {
        int val = std::max(-160, std::min(0, dBu));
        if (m_agcTSlider->value() != val) {
            m_updatingFromModel = true;
            m_agcTSlider->setValue(val);
            if (m_agcTLabel) {
                m_agcTLabel->setText(QString::number(val));
            }
            m_updatingFromModel = false;
        }
    }
}

void VfoWidget::setBinauralEnabled(bool v)
{
    if (m_binBtn && m_binBtn->isChecked() != v) {
        m_updatingFromModel = true;
        m_binBtn->setChecked(v);
        m_updatingFromModel = false;
    }
}

// ---- Slice coupling (for mode container binding only) ----

void VfoWidget::setSlice(SliceModel* slice)
{
    m_slice = QPointer<SliceModel>(slice);
    if (m_fmContainer) {
        m_fmContainer->setSlice(slice);
    }
    if (m_digContainer) {
        m_digContainer->setSlice(slice);
    }
    if (m_rttyContainer) {
        m_rttyContainer->setSlice(slice);
    }
}

// ---- Floating control buttons (AetherSDR pattern) ----
// Close, Lock, Record, Play — rendered on parent SpectrumWidget

static const char* kFloatingBtn =
    "QPushButton {"
    "  background: rgba(255,255,255,15); border: none;"
    "  border-radius: 10px; color: #c8d8e8; font-size: 11px; padding: 0;"
    "}"
    "QPushButton:hover {"
    "  background: rgba(255,255,255,40);"
    "}";

static const char* kFloatingBtnClose =
    "QPushButton {"
    "  background: rgba(255,255,255,15); border: none;"
    "  border-radius: 10px; color: #c8d8e8; font-size: 11px; padding: 0;"
    "}"
    "QPushButton:hover {"
    "  background: rgba(204,32,32,180); color: #ffffff;"
    "}";

void VfoWidget::buildFloatingButtons()
{
    QWidget* parent = parentWidget();
    if (!parent || m_closeBtn) {
        return;  // Already built or no parent
    }

    auto makeBtn = [&](const QString& text, const char* style) -> QPushButton* {
        auto* btn = new QPushButton(text, parent);
        btn->setFixedSize(20, 20);
        btn->setStyleSheet(style);
        btn->show();
        return btn;
    };

    // Close button — wired
    m_closeBtn = makeBtn(QStringLiteral("\u2715"), kFloatingBtnClose);
    m_closeBtn->setToolTip(QStringLiteral("Close slice"));
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        emit closeRequested(m_sliceIndex);
    });

    // Lock button — wired
    m_lockBtn = makeBtn(QStringLiteral("\U0001F513"), kFloatingBtn);
    m_lockBtn->setToolTip(QStringLiteral("Lock VFO frequency"));
    m_lockBtn->setCheckable(true);
    connect(m_lockBtn, &QPushButton::toggled, this, [this](bool locked) {
        if (!m_updatingFromModel) {
            applyLockedState(locked);
        }
    });

    // Record button — checkable, NYI-badged (no consumer in Stage 1)
    m_recBtn = makeBtn(QStringLiteral("\u23FA"), kFloatingBtn);
    m_recBtn->setToolTip(QStringLiteral("Record audio"));
    m_recBtn->setCheckable(true);
    connect(m_recBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) {
            emit recordToggled(on);
        }
    });
    NyiOverlay::markNyi(m_recBtn, QStringLiteral("phase3g10-stage2"));

    // Play button — checkable, NYI-badged (no consumer in Stage 1)
    m_playBtn = makeBtn(QStringLiteral("\u25B6"), kFloatingBtn);
    m_playBtn->setToolTip(QStringLiteral("Play recording"));
    m_playBtn->setCheckable(true);
    connect(m_playBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) {
            emit playToggled(on);
        }
    });
    NyiOverlay::markNyi(m_playBtn, QStringLiteral("phase3g10-stage2"));
}

// ---- Lock state: applyLockedState + setLocked (S1.8a review — I3) ----
// applyLockedState is the single path for all lock changes — called by both
// the floating m_lockBtn toggled lambda and the X/RIT m_xritLockBtn toggled
// lambda.  setLocked is the inbound edge driven by SliceModel::lockedChanged.

void VfoWidget::applyLockedState(bool on)
{
    // Snapshot the incoming guard state so we can restore it around each
    // button update and correctly decide whether to emit at the end.
    const bool wasUpdating = m_updatingFromModel;

    // Update state
    m_locked = on;

    // Drive floating lock button — set guard while calling setChecked so its
    // toggled signal does not re-enter applyLockedState.
    if (m_lockBtn) {
        m_updatingFromModel = true;
        m_lockBtn->setChecked(on);
        m_lockBtn->setText(on ? QStringLiteral("\U0001F512") : QStringLiteral("\U0001F513"));
        if (on) {
            m_lockBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background: rgba(255,100,100,80); border: none;"
                "  border-radius: 10px; color: #c8d8e8; font-size: 11px; padding: 0; }"
                "QPushButton:hover { background: rgba(255,100,100,120); }"));
        } else {
            m_lockBtn->setStyleSheet(kFloatingBtn);
        }
        m_updatingFromModel = wasUpdating;
    }

    // Drive X/RIT lock button — same pattern.
    if (m_xritLockBtn) {
        m_updatingFromModel = true;
        m_xritLockBtn->setChecked(on);
        m_updatingFromModel = wasUpdating;
    }

    // Only emit lockChanged when the change originates from a user action
    // (i.e., guard was false when this call began).  When called from
    // setLocked() the guard is set true and we skip the emit, preventing
    // a model → widget → model feedback loop.
    if (!wasUpdating) {
        emit lockChanged(on);
    }
}

void VfoWidget::setLocked(bool v)
{
    if (m_locked == v) {
        return;
    }
    // Guard true → applyLockedState will update both buttons but will NOT
    // emit lockChanged back toward the model.
    m_updatingFromModel = true;
    applyLockedState(v);
    m_updatingFromModel = false;
}

void VfoWidget::positionFloatingButtons()
{
    if (!m_closeBtn) {
        return;
    }

    // Stack vertically on the opposite side of the flag from the VFO marker
    // From AetherSDR VfoWidget.cpp:1724-1749
    int btnX;
    if (m_onLeft) {
        // Flag is on the left of marker → buttons on right side of flag
        btnX = x() + width() + 2;
    } else {
        // Flag is on the right of marker → buttons on left side of flag
        btnX = x() - 22;
    }

    // Clamp to parent bounds
    if (parentWidget()) {
        btnX = std::clamp(btnX, 0, parentWidget()->width() - 20);
    }

    int btnY = y();
    QPushButton* btns[] = {m_closeBtn, m_lockBtn, m_recBtn, m_playBtn};
    for (QPushButton* btn : btns) {
        btn->move(btnX, btnY);
        btn->setVisible(isVisible());
        if (isVisible()) {
            btn->raise();
        }
        btnY += 22;
    }
}

// ---- Positioning ----

void VfoWidget::updatePosition(int vfoX, int specTop, FlagDir dir)
{
    // Build floating buttons on first call (parent is now available)
    if (!m_closeBtn && parentWidget()) {
        buildFloatingButtons();
    }

    int flagW = width();
    int parentW = parentWidget() ? parentWidget()->width() : 2000;
    bool onLeft = false;

    if (dir == FlagDir::ForceLeft) {
        onLeft = true;
    } else if (dir == FlagDir::ForceRight) {
        onLeft = false;
    } else {
        // Auto: flag goes OPPOSITE side of passband so it doesn't cover signals.
        // From AetherSDR VfoWidget.cpp:1696 — onLeft = !lowerSideband
        bool lowerSideband = (m_currentMode == DSPMode::LSB ||
                              m_currentMode == DSPMode::DIGL ||
                              m_currentMode == DSPMode::CWL);
        onLeft = !lowerSideband;
    }

    int x;
    if (onLeft) {
        x = vfoX - flagW;
        // Flip to right if clipped off left edge
        if (x < 0) {
            x = vfoX;
            onLeft = false;
        }
    } else {
        x = vfoX;
        // Flip to left if clipped off right edge
        if (x + flagW > parentW) {
            x = vfoX - flagW;
            onLeft = true;
        }
    }

    // Final clamp to stay on screen
    x = std::clamp(x, 0, std::max(0, parentW - flagW));

    m_onLeft = onLeft;
    move(x, specTop);
    positionFloatingButtons();
}

// ---- Painting ----

void VfoWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Dark panel background — from AetherSDR VfoWidget::paintEvent
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x0a, 0x0a, 0x14, 230));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4);

    // Subtle border
    p.setPen(QColor(255, 255, 255, 30));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4);

    // Colored top border matching slice color
    QColor c = sliceColor(m_sliceIndex);
    p.setPen(QPen(c, 2));
    p.drawLine(2, 1, width() - 3, 1);
}

void VfoWidget::mousePressEvent(QMouseEvent* event)
{
    event->accept();
    emit sliceActivationRequested(m_sliceIndex);

    // Double-click on frequency area → enter edit mode
    if (event->type() == QEvent::MouseButtonDblClick) {
        QRect freqRect = m_freqStack->geometry();
        if (freqRect.contains(event->pos())) {
            // Format current frequency as MHz for editing
            double mhz = m_frequency / 1e6;
            m_freqEdit->setText(QString::number(mhz, 'f', 6));
            m_freqEdit->selectAll();
            m_freqStack->setCurrentIndex(1);
            m_freqEdit->setFocus();
        }
    }
}

void VfoWidget::wheelEvent(QWheelEvent* event)
{
    event->accept();
    if (m_locked) {
        return;
    }
    int delta = event->angleDelta().y();
    if (delta == 0) {
        return;
    }
    int steps = (delta > 0) ? 1 : -1;
    double newFreq = m_frequency + steps * m_stepHz;
    newFreq = std::clamp(newFreq, 100000.0, 61440000.0);

    if (!qFuzzyCompare(newFreq, m_frequency)) {
        m_frequency = newFreq;
        updateFreqLabel();
        emit frequencyChanged(newFreq);
    }
}

// ---- Helpers ----

void VfoWidget::updateFreqLabel()
{
    // Format: "14.225.000" (MHz with period separators every 3 digits after decimal)
    // From Thetis txtVFOAFreq format: freq.ToString("f6")
    double mhz = m_frequency / 1e6;
    int intPart = static_cast<int>(mhz);
    int fracPart = static_cast<int>(std::round((mhz - intPart) * 1e6));
    int khz = fracPart / 1000;
    int hz = fracPart % 1000;

    m_freqLabel->setText(QStringLiteral("%1.%2.%3")
        .arg(intPart)
        .arg(khz, 3, 10, QLatin1Char('0'))
        .arg(hz, 3, 10, QLatin1Char('0')));

    // Also update filter width display
    // (will be properly synced from setFilter)
}

QString VfoWidget::formatFilterWidth(int low, int high) const
{
    int width = std::abs(high - low);
    if (width >= 1000) {
        return QStringLiteral("%1K").arg(width / 1000.0, 0, 'f', 1);
    }
    return QString::number(width);
}

QColor VfoWidget::sliceColor(int index)
{
    // From AetherSDR SliceColors.h
    switch (index) {
    case 0: return QColor(0x00, 0xd4, 0xff);  // cyan
    case 1: return QColor(0xff, 0x40, 0xff);  // magenta
    case 2: return QColor(0x40, 0xff, 0x40);  // green
    case 3: return QColor(0xff, 0xff, 0x00);  // yellow
    default: return QColor(0x00, 0xd4, 0xff);
    }
}

} // namespace NereusSDR
