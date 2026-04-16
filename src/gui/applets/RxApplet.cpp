// src/gui/applets/RxApplet.cpp
// Per-slice RX controls applet — all 17 controls from spec.
//
// Layout adapted from AetherSDR RxApplet.cpp (buildUI()).
// Tier 1 wiring to SliceModel follows AetherSDR GUI↔model sync pattern.
// NYI controls marked with NyiOverlay::markNyi() pending Phase 3I.

#include "RxApplet.h"
#include "NyiOverlay.h"
#include "gui/ComboStyle.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/FilterPassbandWidget.h"
#include "models/SliceModel.h"

#include <QAction>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

namespace NereusSDR {

// ─── RxApplet ─────────────────────────────────────────────────────────────────

RxApplet::RxApplet(SliceModel* slice, RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
    , m_slice(slice)
{
    buildUi();
    syncFromModel();
}

void RxApplet::buildUi()
{
    // Outer layout: zero margins, zero spacing (title bar flush).
    // Inner body: 4px sides, 2px top/bottom, 2px row spacing.
    // From AetherSDR RxApplet::buildUI() outer/inner layout pattern.
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* body = new QWidget(this);
    body->setStyleSheet(QStringLiteral("background: %1;").arg(Style::kPanelBg));
    auto* root = new QVBoxLayout(body);
    root->setContentsMargins(4, 2, 4, 2);
    root->setSpacing(2);
    outer->addWidget(body);

    // ── Row 1: badge | lock | RX ant | TX ant | stretch | filter label ────
    // From AetherSDR RxApplet.cpp lines 243-336
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(3);

        // Control 1: Slice letter badge (A/B/C/D)
        // 20×20, bg #0070c0, white text, 3px radius
        m_sliceBadge = new QLabel(QStringLiteral("A"), this);
        m_sliceBadge->setFixedSize(20, 20);
        m_sliceBadge->setAlignment(Qt::AlignCenter);
        m_sliceBadge->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; color: %2;"
            " border-radius: 3px; font-weight: bold; font-size: 11px; }"
        ).arg(Style::kBlueBg, Style::kBlueText));
        row->addWidget(m_sliceBadge);

        // Control 2: Lock button (checkable, 20×20, emoji 🔓/🔒)
        // Checked color: #4488ff. NYI — SliceModel has no setFrequencyLocked yet.
        m_lockBtn = new QPushButton(QString::fromUtf8("\xF0\x9F\x94\x93"), this); // 🔓
        m_lockBtn->setCheckable(true);
        m_lockBtn->setFixedSize(20, 20);
        m_lockBtn->setFlat(true);
        m_lockBtn->setStyleSheet(QStringLiteral(
            "QPushButton { font-size: 13px; padding: 0; background: transparent; border: none; }"
            "QPushButton:checked { color: #4488ff; }"
        ));
        connect(m_lockBtn, &QPushButton::toggled, this, [this](bool locked) {
            m_lockBtn->setText(locked
                ? QString::fromUtf8("\xF0\x9F\x94\x92")   // 🔒
                : QString::fromUtf8("\xF0\x9F\x94\x93")); // 🔓
            // TODO Phase 3I: m_slice->setFrequencyLocked(locked);
        });
        row->addWidget(m_lockBtn);
        NyiOverlay::markNyi(m_lockBtn, QStringLiteral("Phase 3I"));

        // Control 3: RX antenna button (flat, color #4488ff, transparent bg)
        // From AetherSDR RxApplet.cpp lines 270-289
        m_rxAntBtn = new QPushButton(QStringLiteral("ANT1"), this);
        m_rxAntBtn->setFlat(true);
        m_rxAntBtn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  color: #4488ff; background: transparent; border: none;"
            "  font-size: 10px; font-weight: bold; padding: 0 2px;"
            "}"
            "QPushButton:hover { color: #66aaff; }"
        ));
        connect(m_rxAntBtn, &QPushButton::clicked, this, [this] {
            QMenu menu(this);
            const QString cur = m_slice ? m_slice->rxAntenna() : QString{};
            for (const QString& ant : m_antList) {
                QAction* act = menu.addAction(ant);
                act->setCheckable(true);
                act->setChecked(ant == cur);
            }
            const QAction* sel = menu.exec(
                m_rxAntBtn->mapToGlobal(QPoint(0, m_rxAntBtn->height())));
            if (sel && m_slice) {
                m_slice->setRxAntenna(sel->text());
            }
        });
        row->addWidget(m_rxAntBtn);

        // Control 4: TX antenna button (flat, color #ff4444, transparent bg)
        // From AetherSDR RxApplet.cpp lines 292-311
        m_txAntBtn = new QPushButton(QStringLiteral("ANT1"), this);
        m_txAntBtn->setFlat(true);
        m_txAntBtn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  color: #ff4444; background: transparent; border: none;"
            "  font-size: 10px; font-weight: bold; padding: 0 2px;"
            "}"
            "QPushButton:hover { color: #ff6666; }"
        ));
        connect(m_txAntBtn, &QPushButton::clicked, this, [this] {
            QMenu menu(this);
            const QString cur = m_slice ? m_slice->txAntenna() : QString{};
            for (const QString& ant : m_antList) {
                QAction* act = menu.addAction(ant);
                act->setCheckable(true);
                act->setChecked(ant == cur);
            }
            const QAction* sel = menu.exec(
                m_txAntBtn->mapToGlobal(QPoint(0, m_txAntBtn->height())));
            if (sel && m_slice) {
                m_slice->setTxAntenna(sel->text());
            }
        });
        row->addWidget(m_txAntBtn);

        row->addStretch(1);

        // Control 5: Filter width label (color #00c8ff, 11px bold)
        m_filterWidthLbl = new QLabel(QStringLiteral("2.9K"), this);
        m_filterWidthLbl->setAlignment(Qt::AlignCenter);
        m_filterWidthLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: #00c8ff; font-size: 11px; font-weight: bold; }"
        ));
        row->addWidget(m_filterWidthLbl);

        row->addStretch(1);

        root->addLayout(row);
    }

    // ── Control 6: Mode combo ─────────────────────────────────────────────
    // fixedHeight 20, applyComboStyle()
    // From AetherSDR RxApplet.cpp lines 359-381
    {
        m_modeCombo = new QComboBox(this);
        m_modeCombo->setFixedHeight(20);
        m_modeCombo->addItems({
            QStringLiteral("USB"), QStringLiteral("LSB"),
            QStringLiteral("CWU"), QStringLiteral("CWL"),
            QStringLiteral("AM"),  QStringLiteral("SAM"),
            QStringLiteral("FM"),  QStringLiteral("DSB"),
            QStringLiteral("DIGU"),QStringLiteral("DIGL"),
            QStringLiteral("DRM")
        });
        applyComboStyle(m_modeCombo);

        // Tier 1 wiring: mode combo → SliceModel::setDspMode()
        connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) {
            if (m_updatingFromModel || !m_slice) { return; }
            const QString name = m_modeCombo->currentText();
            m_slice->setDspMode(SliceModel::modeFromName(name));
        });
        root->addWidget(m_modeCombo);
    }

    // ── Two-column area ───────────────────────────────────────────────────
    // From AetherSDR RxApplet.cpp lines 443-878
    // left:right stretch = 2:3 (same as AetherSDR)
    auto* columns = new QHBoxLayout;
    columns->setSpacing(4);

    // ── Left column ───────────────────────────────────────────────────────
    auto* leftCol = new QVBoxLayout;
    leftCol->setSpacing(2);

    // Control 17: Step size row — STEP: [<] [value] [>]
    // NYI: SliceModel::setStepHz exists, but step cycling not yet wired
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(0);

        auto* lbl = new QLabel(QStringLiteral("STEP:"), this);
        lbl->setFixedWidth(34);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; }"
        ).arg(Style::kTextSecondary));
        row->addWidget(lbl);

        m_stepDown = new TriBtn(TriBtn::Left, this);
        row->addWidget(m_stepDown);

        m_stepLabel = new QLabel(QStringLiteral("100 Hz"), this);
        m_stepLabel->setAlignment(Qt::AlignCenter);
        m_stepLabel->setStyleSheet(Style::insetValueStyle());
        row->addWidget(m_stepLabel, 1);

        m_stepUp = new TriBtn(TriBtn::Right, this);
        row->addWidget(m_stepUp);

        leftCol->addLayout(row);

        NyiOverlay::markNyi(m_stepDown, QStringLiteral("Phase 3I"));
        NyiOverlay::markNyi(m_stepUp,   QStringLiteral("Phase 3I"));
    }

    // Control 7: Filter preset buttons — 10 buttons in 2×5 grid
    // Rows 0-1, cols 0-4. Spacing 2px. Blue active state.
    // Tier 1 wired → SliceModel::setFilter()
    // From AetherSDR RxApplet.cpp rebuildFilterButtons()
    {
        m_filterContainer = new QWidget(this);
        m_filterGrid = new QGridLayout(m_filterContainer);
        m_filterGrid->setContentsMargins(0, 0, 0, 0);
        m_filterGrid->setSpacing(2);
        rebuildFilterButtons();
        leftCol->addWidget(m_filterContainer);
    }

    // Control 8: FilterPassband — visual filter with drag-to-adjust
    // From AetherSDR RxApplet.cpp lines 504-513
    {
        m_filterPassband = new FilterPassbandWidget(this);
        m_filterPassband->setMinimumHeight(40);
        m_filterPassband->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(m_filterPassband, &FilterPassbandWidget::filterChanged,
                this, [this](int lo, int hi) {
            if (m_slice) { m_slice->setFilter(lo, hi); }
        });
        leftCol->addWidget(m_filterPassband);
    }

    columns->addLayout(leftCol, 2);

    // ── Right column ──────────────────────────────────────────────────────
    auto* rightCol = new QVBoxLayout;
    rightCol->setSpacing(2);

    // Controls 11 + 12: Mute + AF gain slider
    // From AetherSDR RxApplet.cpp lines 654-683
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // Control 12: Mute button (18×18, emoji 🔊/🔇)
        // NYI — SliceModel has no setMuted() yet
        m_muteBtn = new QPushButton(QString::fromUtf8("\xF0\x9F\x94\x8A"), this); // 🔊
        m_muteBtn->setCheckable(true);
        m_muteBtn->setFixedSize(18, 18);
        m_muteBtn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: transparent; border: none; font-size: 12px; padding: 0px;"
            "}"
            "QPushButton:hover { background: %1; border-radius: 3px; }"
        ).arg(Style::kButtonAltHover));
        connect(m_muteBtn, &QPushButton::toggled, this, [this](bool muted) {
            m_muteBtn->setText(muted
                ? QString::fromUtf8("\xF0\x9F\x94\x87")    // 🔇
                : QString::fromUtf8("\xF0\x9F\x94\x8A"));  // 🔊
            // TODO Phase 3I: m_slice->setMuted(muted);
        });
        row->addWidget(m_muteBtn);
        NyiOverlay::markNyi(m_muteBtn, QStringLiteral("Phase 3I"));

        // Control 11: AF gain slider (Tier 1 wired → SliceModel::setAfGain())
        m_afSlider = new QSlider(Qt::Horizontal, this);
        m_afSlider->setRange(0, 100);
        m_afSlider->setValue(50);
        m_afSlider->setFixedHeight(18);
        m_afSlider->setStyleSheet(Style::sliderHStyle());
        connect(m_afSlider, &QSlider::valueChanged, this, [this](int v) {
            if (m_updatingFromModel || !m_slice) { return; }
            m_slice->setAfGain(v);
        });
        row->addWidget(m_afSlider, 1);

        rightCol->addLayout(row);
    }

    // Control 13: Audio pan slider (NYI)
    // L ←→ R, center = 50
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lLbl = new QLabel(QStringLiteral("L"), this);
        lLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; }"
        ).arg(Style::kTextSecondary));
        row->addWidget(lLbl);

        m_panSlider = new QSlider(Qt::Horizontal, this);
        m_panSlider->setRange(0, 100);
        m_panSlider->setValue(50);
        m_panSlider->setFixedHeight(18);
        m_panSlider->setStyleSheet(Style::sliderHStyle());
        row->addWidget(m_panSlider, 1);

        auto* rLbl = new QLabel(QStringLiteral("R"), this);
        rLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; }"
        ).arg(Style::kTextSecondary));
        row->addWidget(rLbl);

        rightCol->addLayout(row);
        NyiOverlay::markNyi(m_panSlider, QStringLiteral("Phase 3I"));
    }

    // Control 14: Squelch toggle + slider (NYI)
    // greenToggle(fixedWidth 52) + QSlider
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_sqlBtn = greenToggle(QStringLiteral("SQL"), 52, 20);
        row->addWidget(m_sqlBtn);

        m_sqlSlider = new QSlider(Qt::Horizontal, this);
        m_sqlSlider->setRange(0, 100);
        m_sqlSlider->setValue(20);
        m_sqlSlider->setFixedHeight(18);
        m_sqlSlider->setStyleSheet(Style::sliderHStyle());
        row->addWidget(m_sqlSlider, 1);

        rightCol->addLayout(row);
        NyiOverlay::markNyi(m_sqlBtn,    QStringLiteral("Phase 3I"));
        NyiOverlay::markNyi(m_sqlSlider, QStringLiteral("Phase 3I"));
    }

    // Controls 9 + 10: AGC combo + AGC threshold slider
    // From AetherSDR RxApplet.cpp lines 738-768
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // Control 9: AGC combo (fixedWidth 52), items: Off/Slow/Med/Fast
        // Tier 1 wired → SliceModel::setAgcMode()
        m_agcCombo = new QComboBox(this);
        m_agcCombo->addItem(QStringLiteral("Off"),  static_cast<int>(AGCMode::Off));
        m_agcCombo->addItem(QStringLiteral("Slow"), static_cast<int>(AGCMode::Slow));
        m_agcCombo->addItem(QStringLiteral("Med"),  static_cast<int>(AGCMode::Med));
        m_agcCombo->addItem(QStringLiteral("Fast"), static_cast<int>(AGCMode::Fast));
        m_agcCombo->setFixedWidth(52);
        m_agcCombo->setFixedHeight(20);
        applyComboStyle(m_agcCombo);

        // Tier 1 wiring
        connect(m_agcCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) {
            if (m_updatingFromModel || !m_slice) { return; }
            const auto mode = static_cast<AGCMode>(
                m_agcCombo->itemData(idx).toInt());
            m_slice->setAgcMode(mode);
        });
        row->addWidget(m_agcCombo);

        // Control 10: AGC threshold slider
        // NYI — SliceModel has no setAgcThreshold() yet
        m_agcTSlider = new QSlider(Qt::Horizontal, this);
        m_agcTSlider->setRange(0, 100);
        m_agcTSlider->setValue(65);
        m_agcTSlider->setFixedHeight(18);
        m_agcTSlider->setStyleSheet(Style::sliderHStyle());
        connect(m_agcTSlider, &QSlider::valueChanged, this, [this](int v) {
            m_agcTSlider->setToolTip(
                QStringLiteral("AGC Threshold: %1").arg(v));
            // TODO Phase 3I: m_slice->setAgcThreshold(v);
        });
        row->addWidget(m_agcTSlider, 1);
        NyiOverlay::markNyi(m_agcTSlider, QStringLiteral("Phase 3I"));

        rightCol->addLayout(row);
    }

    rightCol->addStretch(1);

    // Control 15: RIT toggle + offset + zero
    // amberToggle + insetValue + QPushButton("0")
    // NYI — SliceModel has no setRit() yet
    // From AetherSDR RxApplet.cpp lines 773-823
    {
        auto* row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(0);

        m_ritOnBtn = amberToggle(QStringLiteral("RIT"), -1, 20);
        m_ritOnBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        row->addWidget(m_ritOnBtn);

        row->addSpacing(2);

        m_ritZero = styledButton(QStringLiteral("0"), -1, 20);
        m_ritZero->setCheckable(false);
        row->addWidget(m_ritZero);

        row->addSpacing(2);

        m_ritMinus = new TriBtn(TriBtn::Left, this);
        row->addWidget(m_ritMinus);

        m_ritLabel = new QLabel(QStringLiteral("+0 Hz"), this);
        m_ritLabel->setAlignment(Qt::AlignCenter);
        m_ritLabel->setStyleSheet(Style::insetValueStyle());
        row->addWidget(m_ritLabel, 1);

        m_ritPlus = new TriBtn(TriBtn::Right, this);
        row->addWidget(m_ritPlus);

        rightCol->addLayout(row);

        NyiOverlay::markNyi(m_ritOnBtn, QStringLiteral("Phase 3I"));
        NyiOverlay::markNyi(m_ritMinus, QStringLiteral("Phase 3I"));
        NyiOverlay::markNyi(m_ritPlus,  QStringLiteral("Phase 3I"));
    }

    // Control 16: XIT toggle + offset + zero
    // Same structure as RIT. NYI.
    // From AetherSDR RxApplet.cpp lines 825-876
    {
        auto* row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(0);

        m_xitOnBtn = amberToggle(QStringLiteral("XIT"), -1, 20);
        m_xitOnBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        row->addWidget(m_xitOnBtn);

        row->addSpacing(2);

        m_xitZero = styledButton(QStringLiteral("0"), -1, 20);
        m_xitZero->setCheckable(false);
        row->addWidget(m_xitZero);

        row->addSpacing(2);

        m_xitMinus = new TriBtn(TriBtn::Left, this);
        row->addWidget(m_xitMinus);

        m_xitLabel = new QLabel(QStringLiteral("+0 Hz"), this);
        m_xitLabel->setAlignment(Qt::AlignCenter);
        m_xitLabel->setStyleSheet(Style::insetValueStyle());
        row->addWidget(m_xitLabel, 1);

        m_xitPlus = new TriBtn(TriBtn::Right, this);
        row->addWidget(m_xitPlus);

        rightCol->addLayout(row);

        NyiOverlay::markNyi(m_xitOnBtn, QStringLiteral("Phase 3I"));
        NyiOverlay::markNyi(m_xitMinus, QStringLiteral("Phase 3I"));
        NyiOverlay::markNyi(m_xitPlus,  QStringLiteral("Phase 3I"));
    }

    columns->addLayout(rightCol, 3);
    root->addLayout(columns);

    // ── Tooltips ──────────────────────────────────────────────────────────
    m_sliceBadge->setToolTip(QStringLiteral("Slice identifier"));
    m_lockBtn->setToolTip(QStringLiteral("Lock VFO frequency to prevent accidental tuning"));
    m_rxAntBtn->setToolTip(QStringLiteral("Select the receive antenna port"));
    m_txAntBtn->setToolTip(QStringLiteral("Select the transmit antenna port"));
    m_filterWidthLbl->setToolTip(QStringLiteral("Current filter passband width"));
    m_modeCombo->setToolTip(QStringLiteral("Select operating mode"));
    m_muteBtn->setToolTip(QStringLiteral("Mute this slice audio output"));
    m_afSlider->setToolTip(QStringLiteral("Audio output volume for this slice"));
    m_agcCombo->setToolTip(QStringLiteral("AGC speed: Off/Slow/Med/Fast"));
    m_agcTSlider->setToolTip(QStringLiteral("AGC threshold (NYI)"));
    m_ritOnBtn->setToolTip(QStringLiteral("Receive Incremental Tuning (NYI)"));
    m_xitOnBtn->setToolTip(QStringLiteral("Transmit Incremental Tuning (NYI)"));
}

void RxApplet::rebuildFilterButtons()
{
    // Remove all existing buttons from grid
    for (QPushButton* btn : m_filterBtns) {
        m_filterGrid->removeWidget(btn);
        btn->deleteLater();
    }
    m_filterBtns.clear();

    // 10 filter presets in 3-column grid (matching AetherSDR layout)
    // Blue active state when this filter is selected
    // Tier 1 wired → SliceModel::setFilter()
    static constexpr int kCols = 3;
    const int count = qMin(m_filterWidths.size(), 10);

    for (int i = 0; i < count; ++i) {
        const int widthHz = m_filterWidths.value(i, 2700);
        QString label;
        if (widthHz >= 1000) {
            label = QStringLiteral("%1K").arg(widthHz / 1000.0, 0, 'g', 2);
        } else {
            label = QStringLiteral("%1").arg(widthHz);
        }

        auto* btn = blueToggle(label, -1, 20);
        btn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        btn->setCheckable(true);
        // Smaller padding than base style to fit narrow column
        // (matches AetherSDR kButtonBase: padding 1px 2px)
        btn->setStyleSheet(btn->styleSheet() + QStringLiteral(
            "QPushButton { padding: 1px 2px; }"));

        connect(btn, &QPushButton::clicked, this, [this, widthHz] {
            applyFilterPreset(widthHz);
        });

        m_filterBtns.append(btn);
        m_filterGrid->addWidget(btn, i / kCols, i % kCols);
    }
}

void RxApplet::applyFilterPreset(int widthHz)
{
    if (!m_slice) { return; }

    // Determine low/high from width + current mode
    // For USB/CWU: low = 100, high = low + width
    // For LSB/CWL: high = -100, low = high - width
    // For AM/SAM/DSB: symmetric ±half
    // This mirrors AetherSDR RxApplet::applyFilterPreset() logic.
    const DSPMode mode = m_slice->dspMode();
    int low  = 0;
    int high = 0;

    switch (mode) {
    case DSPMode::USB:
    case DSPMode::CWU:
    case DSPMode::DIGU:
        low  = 100;
        high = low + widthHz;
        break;
    case DSPMode::LSB:
    case DSPMode::CWL:
    case DSPMode::DIGL:
        high = -100;
        low  = high - widthHz;
        break;
    case DSPMode::AM:
    case DSPMode::SAM:
    case DSPMode::DSB:
    case DSPMode::FM:
    case DSPMode::DRM:
        low  = -(widthHz / 2);
        high =  (widthHz / 2);
        break;
    default:
        low  = 100;
        high = low + widthHz;
        break;
    }

    m_slice->setFilter(low, high);
}

void RxApplet::updateFilterButtons()
{
    if (!m_slice) { return; }

    const int lo = m_slice->filterLow();
    const int hi = m_slice->filterHigh();
    const int width = hi - lo;

    // Highlight the button matching current filter width (±50 Hz tolerance)
    for (int i = 0; i < m_filterBtns.size(); ++i) {
        QPushButton* btn = m_filterBtns[i];
        const int bw = m_filterWidths.value(i, 0);
        const bool match = qAbs(width - bw) <= 50;
        // Suppress toggled signal to avoid echo loop
        QSignalBlocker blocker(btn);
        btn->setChecked(match);
    }
}

void RxApplet::updateFilterLabel()
{
    if (!m_slice) {
        m_filterWidthLbl->setText(QStringLiteral("---"));
        return;
    }
    m_filterWidthLbl->setText(
        formatFilterWidth(m_slice->filterLow(), m_slice->filterHigh()));
}

QString RxApplet::formatFilterWidth(int low, int high)
{
    const int width = qAbs(high - low);
    if (width >= 1000) {
        return QStringLiteral("%1K").arg(width / 1000.0, 0, 'g', 3);
    }
    return QStringLiteral("%1").arg(width);
}

void RxApplet::setSlice(SliceModel* slice)
{
    if (m_slice == slice) { return; }
    disconnectSlice(m_slice);
    m_slice = slice;
    connectSlice(m_slice);
    syncFromModel();
}

void RxApplet::setSliceIndex(int idx)
{
    static const char* kLetters[] = {"A", "B", "C", "D"};
    if (idx >= 0 && idx < 4) {
        m_sliceBadge->setText(QString::fromLatin1(kLetters[idx]));
    }
}

void RxApplet::setAntennaList(const QStringList& ants)
{
    m_antList = ants;
    // Update button labels to show current selection if changed
    if (m_slice) {
        m_rxAntBtn->setText(m_slice->rxAntenna());
        m_txAntBtn->setText(m_slice->txAntenna());
    }
}

// ── Model → UI sync ───────────────────────────────────────────────────────────

void RxApplet::syncFromModel()
{
    if (!m_slice) { return; }

    m_updatingFromModel = true;

    // Mode combo
    {
        const QString name = SliceModel::modeName(m_slice->dspMode());
        const int idx = m_modeCombo->findText(name);
        if (idx >= 0) { m_modeCombo->setCurrentIndex(idx); }
    }

    // AGC combo
    {
        const int modeVal = static_cast<int>(m_slice->agcMode());
        for (int i = 0; i < m_agcCombo->count(); ++i) {
            if (m_agcCombo->itemData(i).toInt() == modeVal) {
                m_agcCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    // AF gain slider
    m_afSlider->setValue(m_slice->afGain());

    // Antenna buttons
    m_rxAntBtn->setText(m_slice->rxAntenna());
    m_txAntBtn->setText(m_slice->txAntenna());

    // Filter label + preset buttons + passband widget
    updateFilterLabel();
    updateFilterButtons();
    m_filterPassband->setFilter(m_slice->filterLow(), m_slice->filterHigh());
    m_filterPassband->setMode(SliceModel::modeName(m_slice->dspMode()));

    m_updatingFromModel = false;
}

void RxApplet::connectSlice(SliceModel* s)
{
    if (!s) { return; }

    // Mode change → update combo + passband widget
    connect(s, &SliceModel::dspModeChanged, this, [this](DSPMode mode) {
        m_updatingFromModel = true;
        const QString name = SliceModel::modeName(mode);
        const int idx = m_modeCombo->findText(name);
        if (idx >= 0) { m_modeCombo->setCurrentIndex(idx); }
        m_updatingFromModel = false;
        updateFilterLabel();
        updateFilterButtons();
        m_filterPassband->setMode(name);
    });

    // AGC change → update combo
    connect(s, &SliceModel::agcModeChanged, this, [this](AGCMode mode) {
        m_updatingFromModel = true;
        const int modeVal = static_cast<int>(mode);
        for (int i = 0; i < m_agcCombo->count(); ++i) {
            if (m_agcCombo->itemData(i).toInt() == modeVal) {
                m_agcCombo->setCurrentIndex(i);
                break;
            }
        }
        m_updatingFromModel = false;
    });

    // AF gain change → update slider
    connect(s, &SliceModel::afGainChanged, this, [this](int gain) {
        m_updatingFromModel = true;
        m_afSlider->setValue(gain);
        m_updatingFromModel = false;
    });

    // Filter change → update label + buttons + passband widget
    connect(s, &SliceModel::filterChanged, this, [this](int lo, int hi) {
        updateFilterLabel();
        updateFilterButtons();
        m_filterPassband->setFilter(lo, hi);
    });

    // Antenna changes → update button labels
    connect(s, &SliceModel::rxAntennaChanged, this, [this](const QString& ant) {
        m_rxAntBtn->setText(ant);
    });
    connect(s, &SliceModel::txAntennaChanged, this, [this](const QString& ant) {
        m_txAntBtn->setText(ant);
    });
}

void RxApplet::disconnectSlice(SliceModel* s)
{
    if (!s) { return; }
    s->disconnect(this);
}

} // namespace NereusSDR
