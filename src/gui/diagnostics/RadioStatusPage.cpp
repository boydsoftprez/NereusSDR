// =================================================================
// src/gui/diagnostics/RadioStatusPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Diagnostics → Radio Status dashboard.
// Thetis surfaces these readouts piecemeal across Front Console /
// PA Settings / main meter / etc. NereusSDR consolidates them into
// a single tab backed by Phase 3P-H Task 1 models (RadioStatus,
// SettingsHygiene) + Phase 3P-E HermesLiteBandwidthMonitor.
//
// No direct Thetis port at this layer; data shapes ported in Task 1.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-20 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-25 - R-R3-32 / R-R3-46 (parity Task 6): PA Temp, PA Current
//                and PA Voltage (set at last) from RadioModel::paReadings(),
//                the Core's in a remote window and said so; unavailable,
//                never 0, when absent. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-26 - R-R3-32 (remote-window parity Task 14): the Connection
//                Quality figures from RadioModel::hl2LinkFigures(), the
//                Core's HL2 link in a remote window and said so;
//                unavailable, never 0, when absent. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-49 / R-IOS-18: Setup description version 15 ids on
//                the readouts. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
// =================================================================

#include "RadioStatusPage.h"

#include "models/RadioModel.h"
#include "core/PaTempUnit.h"
#include "core/RadioStatus.h"
#include "core/SettingsHygiene.h"
#include "core/session/IStationLink.h"
#include "core/HermesLiteBandwidthMonitor.h"
#include "core/PttSource.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QListWidget>
#include <QMessageBox>
#include <QScrollArea>

namespace NereusSDR {

// ── bar meter limit constants ──────────────────────────────────────────────
// PA temp bar: 0–75°C (NereusSDR-original safe operating range for
// HL2/ANAN PA stages; no Thetis source for this specific limit value).
// no-port-check: NereusSDR-original constant.
static constexpr int kPaTempMaxC  = 75;

// PA current bar: 0–25 A
// no-port-check: NereusSDR-original constant.
static constexpr int kPaCurrentMax10 = 250;  // stored as ×10 for integer bar

// Forward power bar: 0–200 W
// no-port-check: NereusSDR-original constant.
static constexpr int kFwdPowerMaxW = 200;

// SWR bar: 1–3:1 (integer ×10 for 1.0..3.0)
// no-port-check: NereusSDR-original constant.
static constexpr int kSwrBarMin10 = 10;
static constexpr int kSwrBarMax10 = 30;

// BW poll interval: 250 ms for live rate updates
// no-port-check: NereusSDR-original constant.
static constexpr int kBwPollMs = 250;

// Uptime timer interval: 1000 ms
// no-port-check: NereusSDR-original constant.
static constexpr int kUptimeIntervalMs = 1000;

static QString cardStyle()
{
    return QStringLiteral(
        "QFrame {"
        "  background: #0a0a18;"
        "  border: 1px solid %1;"
        "  border-radius: 4px;"
        "  padding: 2px;"
        "}"
    ).arg(QLatin1String(Style::kBorder));
}

static QString progressBarStyle(const char* fill)
{
    return QStringLiteral(
        "QProgressBar {"
        "  background: #1a1a2a;"
        "  border: 1px solid %1;"
        "  border-radius: 3px;"
        "  height: 8px;"
        "  text-align: center;"
        "}"
        "QProgressBar::chunk {"
        "  background: %2;"
        "  border-radius: 2px;"
        "}"
    ).arg(QLatin1String(Style::kBorderSubtle), QLatin1String(fill));
}

RadioStatusPage::RadioStatusPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Radio Status"), model, parent)
    , m_model(model)
{
    // ── Outer layout (added inside SetupPage's contentLayout) ────────────
    auto* outer = new QVBoxLayout();
    outer->setSpacing(6);
    outer->setContentsMargins(6, 6, 6, 6);
    contentLayout()->addLayout(outer);

    // ── Top status bar ────────────────────────────────────────────────────
    auto* statusBar = new QFrame(this);
    statusBar->setFrameShape(QFrame::StyledPanel);
    statusBar->setStyleSheet(QStringLiteral(
        "QFrame {"
        "  background: #0a1a10;"
        "  border: 1px solid %1;"
        "  border-radius: 4px;"
        "}"
    ).arg(QLatin1String(Style::kGreenBorder)));
    statusBar->setFixedHeight(Style::kStatusBarH);
    buildStatusBar(statusBar);
    outer->addWidget(statusBar);

    // ── Scroll area for cards ─────────────────────────────────────────────
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));

    auto* cardsContainer = new QWidget;
    cardsContainer->setStyleSheet(QStringLiteral("QWidget { background: transparent; }"));
    auto* cardsLayout = new QVBoxLayout(cardsContainer);
    cardsLayout->setSpacing(6);
    cardsLayout->setContentsMargins(0, 0, 0, 0);

    // ── Top row: 3 cards ──────────────────────────────────────────────────
    auto* topRow = new QHBoxLayout;
    topRow->setSpacing(6);

    auto* paCard = new QFrame(this);
    paCard->setStyleSheet(cardStyle());
    buildPaStatusCard(paCard);
    topRow->addWidget(paCard, 1);

    auto* powerCard = new QFrame(this);
    powerCard->setStyleSheet(cardStyle());
    buildPowerCard(powerCard);
    topRow->addWidget(powerCard, 1);

    auto* pttCard = new QFrame(this);
    pttCard->setStyleSheet(cardStyle());
    buildPttCard(pttCard);
    topRow->addWidget(pttCard, 1);

    cardsLayout->addLayout(topRow);

    // ── Bottom row: 2 cards ───────────────────────────────────────────────
    auto* botRow = new QHBoxLayout;
    botRow->setSpacing(6);

    auto* connCard = new QFrame(this);
    connCard->setStyleSheet(cardStyle());
    buildConnectionCard(connCard);
    botRow->addWidget(connCard, 1);

    auto* hygCard = new QFrame(this);
    hygCard->setStyleSheet(cardStyle());
    buildHygieneCard(hygCard);
    botRow->addWidget(hygCard, 1);

    cardsLayout->addLayout(botRow);
    cardsLayout->addStretch();

    scroll->setWidget(cardsContainer);
    outer->addWidget(scroll);

    // Setup description version 15: the readouts' ids.
    const std::pair<QLabel*, const char*> readouts[] = {
        {m_radioLabel, "radio"}, {m_uptimeLabel, "uptime"}, {m_firmwareLabel, "firmware"},
        {m_modeLabel, "mode"}, {m_paTemperatureLabel, "paTemperature"},
        {m_paCurrentLabel, "paCurrent"}, {m_paVoltageLabel, "paVoltage"},
        {m_forwardLabel, "forward"}, {m_reflectedLabel, "reflected"}, {m_swrLabel, "swr"},
        {m_bwEp6Label, "ep6"}, {m_bwEp2Label, "ep2"}, {m_bwThrottleLabel, "throttle"},
        {m_bwSeqGapLabel, "sequenceGaps"}};
    for (const auto& [label, id] : readouts) {
        label->setProperty("nereusSetupId",
                           QStringLiteral("diagnostics.radioStatus.") + QLatin1String(id));
    }

    // ── Wire signals if model present ─────────────────────────────────────
    if (m_model) {
        RadioStatus& rs = m_model->radioStatus();
        // R-R3-32 / R-R3-46 (parity Task 6): PA temperature, current and
        // voltage come from the one PA reading source (the Core's in a
        // remote window).
        connect(m_model, &RadioModel::paReadingsChanged,
                this, &RadioStatusPage::refreshPaReadings);
        // Live re-format on °C / °F toggle. The progress bar stays in
        // °C; only the text label converts.
        connect(&PaTempUnitNotifier::instance(),
                &PaTempUnitNotifier::unitChanged, this,
                [this](PaTempUnit /*unit*/) {
            refreshPaReadings();
        });
        connect(&rs, &RadioStatus::powerChanged,
                this, &RadioStatusPage::onPowerChanged);
        connect(&rs, &RadioStatus::pttChanged,
                this, [this]() { onPttChanged(); });

        SettingsHygiene& sh = m_model->settingsHygiene();
        connect(&sh, &SettingsHygiene::issuesChanged,
                this, &RadioStatusPage::onIssuesChanged);

        // Populate initial state
        onIssuesChanged();
        refreshPttPills();
        refreshPaReadings();
    }

    // ── Uptime timer ──────────────────────────────────────────────────────
    connect(&m_uptimeTimer, &QTimer::timeout, this, &RadioStatusPage::onUptimeTick);
    if (m_model) {
        connect(m_model, &RadioModel::connectionStateChanged,
                this, &RadioStatusPage::onUptimeTick);
    }
    onUptimeTick();
    m_uptimeTimer.start(kUptimeIntervalMs);

    // ── BW poll timer ─────────────────────────────────────────────────────
    connect(&m_bwPollTimer, &QTimer::timeout, this, &RadioStatusPage::onBwPollTick);
    m_bwPollTimer.start(kBwPollMs);
}

// ── Status bar build ──────────────────────────────────────────────────────

void RadioStatusPage::buildStatusBar(QFrame* bar)
{
    auto* h = new QHBoxLayout(bar);
    h->setSpacing(16);
    h->setContentsMargins(10, 2, 10, 2);

    auto makeCol = [&](const QString& label) -> QLabel* {
        auto* lbl = new QLabel(label, bar);
        lbl->setStyleSheet(QStringLiteral("font-size: 10px; color: %1;")
                               .arg(QLatin1String(Style::kGreenText)));
        return lbl;
    };

    // Col 1: Connected radio
    {
        auto* v = new QVBoxLayout;
        v->setSpacing(0);
        auto* hdr = new QLabel(QStringLiteral("Connected radio"), bar);
        hdr->setStyleSheet(QStringLiteral("font-size: 9px; color: %1;")
                               .arg(QLatin1String(Style::kTextTertiary)));
        v->addWidget(hdr);
        m_radioLabel = makeCol(QStringLiteral("–"));
        v->addWidget(m_radioLabel);
        h->addLayout(v);
    }
    h->addStretch();

    // Col 2: Uptime
    {
        auto* v = new QVBoxLayout;
        v->setSpacing(0);
        auto* hdr = new QLabel(QStringLiteral("Uptime"), bar);
        hdr->setStyleSheet(QStringLiteral("font-size: 9px; color: %1;")
                               .arg(QLatin1String(Style::kTextTertiary)));
        v->addWidget(hdr);
        m_uptimeLabel = makeCol(QStringLiteral("00:00"));
        v->addWidget(m_uptimeLabel);
        h->addLayout(v);
    }
    h->addStretch();

    // Col 3: Firmware
    {
        auto* v = new QVBoxLayout;
        v->setSpacing(0);
        auto* hdr = new QLabel(QStringLiteral("Firmware"), bar);
        hdr->setStyleSheet(QStringLiteral("font-size: 9px; color: %1;")
                               .arg(QLatin1String(Style::kTextTertiary)));
        v->addWidget(hdr);
        m_firmwareLabel = makeCol(QStringLiteral("–"));
        v->addWidget(m_firmwareLabel);
        h->addLayout(v);
    }
    h->addStretch();

    // Col 4: Mode
    {
        auto* v = new QVBoxLayout;
        v->setSpacing(0);
        auto* hdr = new QLabel(QStringLiteral("Mode"), bar);
        hdr->setStyleSheet(QStringLiteral("font-size: 9px; color: %1;")
                               .arg(QLatin1String(Style::kTextTertiary)));
        v->addWidget(hdr);
        m_modeLabel = makeCol(QStringLiteral("RX (idle)"));
        v->addWidget(m_modeLabel);
        h->addLayout(v);
    }

    // Populate from model if available
    if (m_model) {
        QString radioStr = m_model->model();
        if (!m_model->name().isEmpty()) {
            radioStr = m_model->name();
        }
        m_radioLabel->setText(radioStr.isEmpty() ? QStringLiteral("–") : radioStr);
        m_firmwareLabel->setText(m_model->version().isEmpty()
                                     ? QStringLiteral("–") : m_model->version());
    }
}

// ── Card: PA Status ───────────────────────────────────────────────────────

void RadioStatusPage::buildPaStatusCard(QFrame* card)
{
    auto* v = new QVBoxLayout(card);
    v->setSpacing(4);
    v->setContentsMargins(8, 6, 8, 6);

    auto* title = new QLabel(QStringLiteral("PA Status"), card);
    title->setStyleSheet(QStringLiteral(
        "font-size: 10px; font-weight: bold; color: %1; border: none;"
    ).arg(QLatin1String(Style::kAccent)));
    v->addWidget(title);
    m_paTitleLabel = title;

    // ── Temperature row ───────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        auto* lbl = new QLabel(QStringLiteral("PA Temp"), card);
        lbl->setStyleSheet(QStringLiteral("font-size: 9px; color: %1; border: none;")
                               .arg(QLatin1String(Style::kTextSecondary)));
        row->addWidget(lbl);
        row->addStretch();
        m_paTemperatureLabel = new QLabel(QStringLiteral("0.0 °C"), card);
        m_paTemperatureLabel->setStyleSheet(QStringLiteral(
            "font-size: 10px; font-weight: bold; color: %1; border: none;"
        ).arg(QLatin1String(Style::kTextPrimary)));
        row->addWidget(m_paTemperatureLabel);
        v->addLayout(row);

        m_paTempBar = new QProgressBar(card);
        m_paTempBar->setRange(0, kPaTempMaxC);
        m_paTempBar->setValue(0);
        m_paTempBar->setTextVisible(false);
        m_paTempBar->setFixedHeight(8);
        m_paTempBar->setStyleSheet(progressBarStyle(Style::kGaugeNormal));
        v->addWidget(m_paTempBar);
    }

    // ── Current row ───────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        auto* lbl = new QLabel(QStringLiteral("PA Current"), card);
        lbl->setStyleSheet(QStringLiteral("font-size: 9px; color: %1; border: none;")
                               .arg(QLatin1String(Style::kTextSecondary)));
        row->addWidget(lbl);
        row->addStretch();
        m_paCurrentLabel = new QLabel(QStringLiteral("0.0 A"), card);
        m_paCurrentLabel->setStyleSheet(QStringLiteral(
            "font-size: 10px; font-weight: bold; color: %1; border: none;"
        ).arg(QLatin1String(Style::kTextPrimary)));
        row->addWidget(m_paCurrentLabel);
        v->addLayout(row);

        m_paCurrentBar = new QProgressBar(card);
        m_paCurrentBar->setRange(0, kPaCurrentMax10);
        m_paCurrentBar->setValue(0);
        m_paCurrentBar->setTextVisible(false);
        m_paCurrentBar->setFixedHeight(8);
        m_paCurrentBar->setStyleSheet(progressBarStyle(Style::kGaugeNormal));
        v->addWidget(m_paCurrentBar);
    }

    // ── Voltage row (not exposed by Thetis status fields) ────────────────
    {
        auto* row = new QHBoxLayout;
        auto* lbl = new QLabel(QStringLiteral("PA Voltage"), card);
        lbl->setStyleSheet(QStringLiteral("font-size: 9px; color: %1; border: none;")
                               .arg(QLatin1String(Style::kTextSecondary)));
        row->addWidget(lbl);
        row->addStretch();
        // PA voltage is not a separately-exposed status field in Thetis
        // (see RadioStatus.h header comment re: source-first deviation).
        m_paVoltageLabel = new QLabel(QStringLiteral("–"), card);
        m_paVoltageLabel->setStyleSheet(QStringLiteral(
            "font-size: 10px; font-weight: bold; color: %1; border: none;"
        ).arg(QLatin1String(Style::kTextInactive)));
        row->addWidget(m_paVoltageLabel);
        v->addLayout(row);
    }

    v->addStretch();
}

// ── Card: Forward/Reflected/SWR ───────────────────────────────────────────

void RadioStatusPage::buildPowerCard(QFrame* card)
{
    auto* v = new QVBoxLayout(card);
    v->setSpacing(4);
    v->setContentsMargins(8, 6, 8, 6);

    auto* title = new QLabel(QStringLiteral("Forward / Reflected / SWR"), card);
    title->setStyleSheet(QStringLiteral(
        "font-size: 10px; font-weight: bold; color: %1; border: none;"
    ).arg(QLatin1String(Style::kAccent)));
    v->addWidget(title);

    // Forward power
    {
        auto* row = new QHBoxLayout;
        auto* lbl = new QLabel(QStringLiteral("Forward"), card);
        lbl->setStyleSheet(QStringLiteral("font-size: 9px; color: %1; border: none;")
                               .arg(QLatin1String(Style::kTextSecondary)));
        row->addWidget(lbl);
        row->addStretch();
        m_forwardLabel = new QLabel(QStringLiteral("– W"), card);
        m_forwardLabel->setStyleSheet(QStringLiteral(
            "font-size: 10px; font-weight: bold; color: %1; border: none;"
        ).arg(QLatin1String(Style::kTextPrimary)));
        row->addWidget(m_forwardLabel);
        v->addLayout(row);

        m_forwardBar = new QProgressBar(card);
        m_forwardBar->setRange(0, kFwdPowerMaxW);
        m_forwardBar->setValue(0);
        m_forwardBar->setTextVisible(false);
        m_forwardBar->setFixedHeight(8);
        m_forwardBar->setStyleSheet(progressBarStyle(Style::kGaugeNormal));
        v->addWidget(m_forwardBar);
    }

    // Reflected power
    {
        auto* row = new QHBoxLayout;
        auto* lbl = new QLabel(QStringLiteral("Reflected"), card);
        lbl->setStyleSheet(QStringLiteral("font-size: 9px; color: %1; border: none;")
                               .arg(QLatin1String(Style::kTextSecondary)));
        row->addWidget(lbl);
        row->addStretch();
        m_reflectedLabel = new QLabel(QStringLiteral("– W"), card);
        m_reflectedLabel->setStyleSheet(QStringLiteral(
            "font-size: 10px; font-weight: bold; color: %1; border: none;"
        ).arg(QLatin1String(Style::kTextPrimary)));
        row->addWidget(m_reflectedLabel);
        v->addLayout(row);
    }

    // SWR
    {
        auto* row = new QHBoxLayout;
        auto* lbl = new QLabel(QStringLiteral("SWR"), card);
        lbl->setStyleSheet(QStringLiteral("font-size: 9px; color: %1; border: none;")
                               .arg(QLatin1String(Style::kTextSecondary)));
        row->addWidget(lbl);
        row->addStretch();
        m_swrLabel = new QLabel(QStringLiteral("1.0:1"), card);
        m_swrLabel->setStyleSheet(QStringLiteral(
            "font-size: 10px; font-weight: bold; color: %1; border: none;"
        ).arg(QLatin1String(Style::kTextPrimary)));
        row->addWidget(m_swrLabel);
        v->addLayout(row);

        m_swrBar = new QProgressBar(card);
        m_swrBar->setRange(kSwrBarMin10, kSwrBarMax10);
        m_swrBar->setValue(kSwrBarMin10);
        m_swrBar->setTextVisible(false);
        m_swrBar->setFixedHeight(8);
        m_swrBar->setStyleSheet(progressBarStyle(Style::kGaugeWarning));
        v->addWidget(m_swrBar);
    }

    v->addStretch();
}

// ── Card: PTT Source ──────────────────────────────────────────────────────

void RadioStatusPage::buildPttCard(QFrame* card)
{
    auto* v = new QVBoxLayout(card);
    v->setSpacing(4);
    v->setContentsMargins(8, 6, 8, 6);

    auto* title = new QLabel(QStringLiteral("PTT Source"), card);
    title->setStyleSheet(QStringLiteral(
        "font-size: 10px; font-weight: bold; color: %1; border: none;"
    ).arg(QLatin1String(Style::kAccent)));
    v->addWidget(title);

    m_pttActiveLabel = new QLabel(QStringLiteral("Active: none"), card);
    m_pttActiveLabel->setStyleSheet(QStringLiteral(
        "font-size: 9px; color: %1; border: none;"
    ).arg(QLatin1String(Style::kTextSecondary)));
    v->addWidget(m_pttActiveLabel);

    // Pill row — 7 sources
    const char* pillLabels[7] = { "MOX", "VOX", "CAT", "Mic PTT", "CW", "Tune", "2-Tone" };

    auto* pillRow = new QHBoxLayout;
    pillRow->setSpacing(3);
    for (int i = 0; i < 7; ++i) {
        auto* btn = new QPushButton(QString::fromLatin1(pillLabels[i]), card);
        btn->setCheckable(false);
        btn->setEnabled(false);
        btn->setFixedHeight(18);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: %1; border: 1px solid %2;"
            "  border-radius: 9px; color: %3;"
            "  font-size: 8px; font-weight: bold; padding: 1px 4px;"
            "}"
        ).arg(QLatin1String(Style::kButtonBg),
              QLatin1String(Style::kBorderSubtle),
              QLatin1String(Style::kTextInactive)));
        pillRow->addWidget(btn);
        m_pttPills[i] = btn;
    }
    pillRow->addStretch();
    v->addLayout(pillRow);

    // History list
    auto* histLbl = new QLabel(QStringLiteral("Recent events:"), card);
    histLbl->setStyleSheet(QStringLiteral("font-size: 9px; color: %1; border: none;")
                               .arg(QLatin1String(Style::kTextTertiary)));
    v->addWidget(histLbl);

    m_pttHistoryList = new QListWidget(card);
    m_pttHistoryList->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "  background: %1; border: 1px solid %2;"
        "  font-family: monospace; font-size: 9px; color: %3;"
        "}"
        "QListWidget::item { padding: 1px 2px; }"
    ).arg(QLatin1String(Style::kInsetBg),
          QLatin1String(Style::kInsetBorder),
          QLatin1String(Style::kTextPrimary)));
    m_pttHistoryList->setMaximumHeight(100);
    m_pttHistoryList->setSelectionMode(QAbstractItemView::NoSelection);
    v->addWidget(m_pttHistoryList);

    v->addStretch();
}

// ── Card: Connection Quality ──────────────────────────────────────────────

void RadioStatusPage::buildConnectionCard(QFrame* card)
{
    auto* v = new QVBoxLayout(card);
    v->setSpacing(4);
    v->setContentsMargins(8, 6, 8, 6);

    auto* title = new QLabel(QStringLiteral("Connection Quality"), card);
    m_connTitleLabel = title;
    title->setStyleSheet(QStringLiteral(
        "font-size: 10px; font-weight: bold; color: %1; border: none;"
    ).arg(QLatin1String(Style::kAccent)));
    v->addWidget(title);

    auto makeRow = [&](const QString& label, QLabel*& out) {
        auto* row = new QHBoxLayout;
        auto* lbl = new QLabel(label, card);
        lbl->setStyleSheet(QStringLiteral("font-size: 9px; color: %1; border: none;")
                               .arg(QLatin1String(Style::kTextSecondary)));
        row->addWidget(lbl);
        row->addStretch();
        out = new QLabel(QStringLiteral("–"), card);
        out->setStyleSheet(QStringLiteral(
            "font-size: 10px; font-weight: bold; color: %1; border: none;"
        ).arg(QLatin1String(Style::kTextPrimary)));
        row->addWidget(out);
        v->addLayout(row);
    };

    makeRow(QStringLiteral("EP6 ingress"), m_bwEp6Label);
    makeRow(QStringLiteral("EP2 egress"),  m_bwEp2Label);
    makeRow(QStringLiteral("Throttle events"), m_bwThrottleLabel);
    makeRow(QStringLiteral("Seq gaps (EP6)"),  m_bwSeqGapLabel);

    auto* hint = new QLabel(
        QStringLiteral("(Full 60s graph in Connection Quality tab)"), card);
    hint->setStyleSheet(QStringLiteral("font-size: 8px; color: %1; border: none;")
                            .arg(QLatin1String(Style::kTextTertiary)));
    v->addWidget(hint);

    v->addStretch();

    // Initialize with actual data if model present
    if (m_model) {
        onBwPollTick();
    }
}

// ── Card: Settings Hygiene ────────────────────────────────────────────────

void RadioStatusPage::buildHygieneCard(QFrame* card)
{
    auto* v = new QVBoxLayout(card);
    v->setSpacing(4);
    v->setContentsMargins(8, 6, 8, 6);

    auto* title = new QLabel(QStringLiteral("Settings Hygiene"), card);
    title->setStyleSheet(QStringLiteral(
        "font-size: 10px; font-weight: bold; color: %1; border: none;"
    ).arg(QLatin1String(Style::kAccent)));
    v->addWidget(title);

    m_issueList = new QListWidget(card);
    m_issueList->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "  background: %1; border: 1px solid %2;"
        "  font-size: 9px; color: %3;"
        "}"
        "QListWidget::item { padding: 2px 4px; border-bottom: 1px solid %2; }"
    ).arg(QLatin1String(Style::kInsetBg),
          QLatin1String(Style::kInsetBorder),
          QLatin1String(Style::kTextPrimary)));
    m_issueList->setMaximumHeight(120);
    m_issueList->setSelectionMode(QAbstractItemView::NoSelection);
    v->addWidget(m_issueList);

    // Action buttons
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(4);

    m_repairBtn = new QPushButton(QStringLiteral("Repair invalid settings"), card);
    m_repairBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2; border-radius: 3px;"
        " color: %3; font-size: 9px; padding: 2px 6px; }"
        "QPushButton:hover { background: %4; }"
    ).arg(QLatin1String(Style::kAmberBg),
          QLatin1String(Style::kAmberBorder),
          QLatin1String(Style::kAmberText),
          QLatin1String(Style::kButtonHover)));
    btnRow->addWidget(m_repairBtn);

    m_forgetBtn = new QPushButton(QStringLiteral("Forget this radio"), card);
    m_forgetBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2; border-radius: 3px;"
        " color: %3; font-size: 9px; padding: 2px 6px; }"
        "QPushButton:hover { background: %4; }"
    ).arg(QLatin1String(Style::kRedBg),
          QLatin1String(Style::kRedBorder),
          QLatin1String(Style::kRedText),
          QLatin1String(Style::kButtonHover)));
    btnRow->addWidget(m_forgetBtn);
    btnRow->addStretch();
    v->addLayout(btnRow);

    v->addStretch();

    // Wire buttons
    if (m_model) {
        connect(m_repairBtn, &QPushButton::clicked, this, [this]() {
            const QString mac = m_model->currentRadioMac();
            auto reply = QMessageBox::warning(this,
                QStringLiteral("Repair settings"),
                QStringLiteral("Repair the settings that are invalid for this radio? Values "
                               "outside its range are brought back into range, and settings "
                               "for hardware it does not have are removed."),
                QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
            if (reply == QMessageBox::Ok) {
                if (mac.isEmpty() || mac != m_model->currentRadioMac()) { return; }
                QString reason;
                if (m_model->stationOnAirRefusal(&reason)) { return; }
                if (m_model->ownsLocalDsp()) {
                    m_model->settingsHygiene().resetSettingsToDefaults(
                        mac, m_model->boardCapabilities());
                } else if (IStationLink* link = m_model->stationLink();
                           link && link->settingsRepairAvailable()) {
                    // G-38: the same repair, run on the Core.
                    link->requestSettingsHygiene("station.repairSettings", mac);
                }
            }
        });

        connect(m_forgetBtn, &QPushButton::clicked, this, [this]() {
            const QString mac = m_model->currentRadioMac();
            auto reply = QMessageBox::warning(this,
                QStringLiteral("Forget radio"),
                QStringLiteral("Remove all saved settings for this radio? This cannot be undone."),
                QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
            if (reply == QMessageBox::Ok) {
                if (mac.isEmpty() || mac != m_model->currentRadioMac()) { return; }
                QString reason;
                if (m_model->stationOnAirRefusal(&reason)) { return; }
                if (m_model->ownsLocalDsp()) {
                    m_model->settingsHygiene().forgetRadio(mac);
                } else if (IStationLink* link = m_model->stationLink();
                           link && link->settingsHygieneAvailable()) {
                    link->requestSettingsHygiene("station.forgetSettings", mac);
                }
            }
        });
    }
}

// ── Slots ─────────────────────────────────────────────────────────────────

void RadioStatusPage::onPaTemperatureChanged(double celsius)
{
    m_paTempLastCelsius = celsius;
    m_paTemperatureLabel->setText(PaTempUnitNotifier::format(celsius));
    m_paTempBar->setValue(static_cast<int>(celsius));

    // Color shift: green → amber → red based on temperature
    const char* fill = Style::kGaugeNormal;
    if (celsius >= 65.0) {
        fill = Style::kGaugeDanger;
    } else if (celsius >= 50.0) {
        fill = Style::kGaugeWarning;
    }
    m_paTempBar->setStyleSheet(progressBarStyle(fill));
}

void RadioStatusPage::onPaCurrentChanged(double amps)
{
    m_paCurrentLabel->setText(QStringLiteral("%1 A").arg(amps, 0, 'f', 1));
    m_paCurrentBar->setValue(static_cast<int>(amps * 10.0));

    const char* fill = Style::kGaugeNormal;
    if (amps >= 22.0) {
        fill = Style::kGaugeDanger;
    } else if (amps >= 18.0) {
        fill = Style::kGaugeWarning;
    }
    m_paCurrentBar->setStyleSheet(progressBarStyle(fill));
}

void RadioStatusPage::refreshPaReadings()
{
    if (!m_model || !m_paTemperatureLabel || !m_paCurrentLabel || !m_paVoltageLabel) { return; }
    const RadioModel::PaReadings readings = m_model->paReadings();
    const bool fromCore = m_model->paReadingsFromCore();
    // R-R3-32: a remote window's readings are the Core's, and say so.
    const QString source = fromCore ? tr("From the Core") : QString();
    if (m_paTitleLabel) {
        m_paTitleLabel->setText(fromCore ? tr("PA Status, from the Core") : tr("PA Status"));
    }
    const QString unavailable = tr("Unavailable");
    if (readings.paTemperatureCelsius) {
        onPaTemperatureChanged(*readings.paTemperatureCelsius);
    } else {
        m_paTemperatureLabel->setText(unavailable);
        m_paTempBar->setValue(0);
        m_paTempBar->setStyleSheet(progressBarStyle(Style::kGaugeNormal));
    }
    if (readings.paCurrentAmps) {
        onPaCurrentChanged(*readings.paCurrentAmps);
    } else {
        m_paCurrentLabel->setText(unavailable);
        m_paCurrentBar->setValue(0);
        m_paCurrentBar->setStyleSheet(progressBarStyle(Style::kGaugeNormal));
    }
    // PA Voltage: the PA row's volts as the System tile shows them (the
    // supply volts on the ANAN-G2E, the PA drain volts elsewhere).
    const RadioModel::PaRowVolts row = m_model->paRowVolts();
    m_paVoltageLabel->setText(row.volts ? QStringLiteral("%1 V").arg(*row.volts, 0, 'f', 1)
                                        : unavailable);
    m_paVoltageLabel->setStyleSheet(QStringLiteral(
        "font-size: 10px; font-weight: bold; color: %1; border: none;"
    ).arg(QLatin1String(row.volts ? Style::kTextPrimary : Style::kTextInactive)));
    for (QLabel* label : {m_paTemperatureLabel, m_paCurrentLabel, m_paVoltageLabel}) {
        label->setToolTip(source);
    }
}

void RadioStatusPage::refreshPower()
{
    if (!m_model) { return; }
    const RadioStatus& rs = m_model->radioStatus();
    onPowerChanged(rs.forwardPowerWatts(), rs.reflectedPowerWatts(), rs.swrRatio());
}

void RadioStatusPage::onPowerChanged(double forward, double reflected, double swr)
{
    if (!m_model) { return; }
    bool tx = m_model->radioStatus().isTransmitting();

    if (tx) {
        m_forwardLabel->setText(QStringLiteral("%1 W").arg(forward, 0, 'f', 1));
        m_reflectedLabel->setText(QStringLiteral("%1 W").arg(reflected, 0, 'f', 1));
        m_swrLabel->setText(QStringLiteral("%1:1").arg(swr, 0, 'f', 1));
    } else {
        m_forwardLabel->setText(QStringLiteral("– W"));
        m_reflectedLabel->setText(QStringLiteral("– W"));
        m_swrLabel->setText(QStringLiteral("1.0:1"));
    }

    m_forwardBar->setValue(static_cast<int>(forward));
    m_swrBar->setValue(static_cast<int>(swr * 10.0));

    // Update mode label
    m_modeLabel->setText(tx ? QStringLiteral("TX") : QStringLiteral("RX (idle)"));
}

void RadioStatusPage::onPttChanged()
{
    if (!m_model) { return; }
    refreshPttPills();

    const RadioStatus& rs = m_model->radioStatus();
    bool tx = rs.isTransmitting();
    PttSource src = rs.activePttSource();

    m_pttActiveLabel->setText(QStringLiteral("Active: %1").arg(pttSourceLabel(src)));
    m_modeLabel->setText(tx ? QStringLiteral("TX") : QStringLiteral("RX (idle)"));
    // The power readouts show only while keyed, so they follow the key too.
    refreshPower();

    // Refresh history list
    m_pttHistoryList->clear();
    const auto events = rs.recentPttEvents();
    for (const auto& ev : events) {
        const QString ts = QStringLiteral("[%1]").arg(ev.timestampMs / 1000);
        const QString action = ev.isStart ? QStringLiteral("TX start") : QStringLiteral("TX end");
        m_pttHistoryList->addItem(QStringLiteral("%1 %2 (%3)")
            .arg(ts, action, pttSourceLabel(ev.source)));
    }
}

void RadioStatusPage::onIssuesChanged()
{
    if (!m_model) { return; }
    refreshHygieneRows();
}

void RadioStatusPage::onUptimeTick()
{
    m_uptimeLabel->setText(m_model ? m_model->connectionUptimeText() : QStringLiteral("–"));
}

void RadioStatusPage::onBwPollTick()
{
    if (!m_model) { return; }

    // R-R3-32 (parity Task 14): this window's HL2 link, or in a remote
    // window the Core's; one the Core has not sent shows as unavailable.
    const RadioModel::Hl2LinkFigures figures = m_model->hl2LinkFigures();
    const bool fromCore = m_model->hl2LinkFiguresFromCore();
    const QString unavailable = tr("Unavailable");
    const QString source = fromCore ? tr("From the Core") : QString();
    if (m_connTitleLabel) {
        m_connTitleLabel->setText(fromCore ? tr("Connection Quality, from the Core")
                                           : tr("Connection Quality"));
    }
    const auto kilobytes = [&unavailable](std::optional<double> bps) {
        return bps ? QStringLiteral("%1 KB/s").arg(*bps / 1024.0, 0, 'f', 1) : unavailable;
    };
    m_bwEp6Label->setText(kilobytes(figures.rxBytesPerSecond));
    m_bwEp2Label->setText(kilobytes(figures.txBytesPerSecond));
    const bool throttled = figures.throttled.value_or(false);
    const QString active = throttled ? QStringLiteral(" (active)") : QString{};
    if (figures.throttleEvents) {
        m_bwThrottleLabel->setText(QStringLiteral("%1%2").arg(*figures.throttleEvents).arg(active));
    } else if (figures.throttled) {
        // The Core sends whether its link is throttled, not the count.
        m_bwThrottleLabel->setText(throttled ? tr("Active") : tr("None active"));
    } else {
        m_bwThrottleLabel->setText(unavailable);
    }
    for (QLabel* label : {m_bwEp6Label, m_bwEp2Label, m_bwThrottleLabel, m_bwSeqGapLabel}) {
        label->setToolTip(source);
    }

    if (throttled) {
        m_bwThrottleLabel->setStyleSheet(QStringLiteral(
            "font-size: 10px; font-weight: bold; color: %1; border: none;"
        ).arg(QLatin1String(Style::kAmberText)));
    } else {
        m_bwThrottleLabel->setStyleSheet(QStringLiteral(
            "font-size: 10px; font-weight: bold; color: %1; border: none;"
        ).arg(QLatin1String(Style::kTextPrimary)));
    }

    // R-R3-21: the EP6 sequence error count P1RadioConnection keeps (the
    // Connection Quality page shows the same number).
    m_bwSeqGapLabel->setText(figures.sequenceGaps ? QString::number(*figures.sequenceGaps)
                                                  : unavailable);
}

void RadioStatusPage::refreshPttPills()
{
    if (!m_model) { return; }

    PttSource active = m_model->radioStatus().activePttSource();

    // Map PttSource enum index (1..7) to pill index (0..6)
    const PttSource sources[7] = {
        PttSource::Mox, PttSource::Vox, PttSource::Cat,
        PttSource::MicPtt, PttSource::Cw, PttSource::Tune, PttSource::TwoTone
    };

    for (int i = 0; i < 7; ++i) {
        bool isActive = (active == sources[i]);
        m_pttPills[i]->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: %1; border: 1px solid %2;"
            "  border-radius: 9px; color: %3;"
            "  font-size: 8px; font-weight: bold; padding: 1px 4px;"
            "}"
        ).arg(isActive ? QLatin1String(Style::kGreenBg)     : QLatin1String(Style::kButtonBg),
              isActive ? QLatin1String(Style::kGreenBorder)  : QLatin1String(Style::kBorderSubtle),
              isActive ? QLatin1String(Style::kGreenText)    : QLatin1String(Style::kTextInactive)));
    }
}

void RadioStatusPage::refreshHygieneRows()
{
    if (!m_model) { return; }

    m_issueList->clear();
    const QString unavailable = m_model->settingsHygiene().remoteUnavailableReason();
    if (!unavailable.isEmpty()) {
        m_issueList->addItem(unavailable);
        return;
    }
    const QVector<SettingsHygiene::Issue> issues = m_model->settingsHygiene().issues();

    if (issues.isEmpty()) {
        auto* item = new QListWidgetItem(QStringLiteral("No issues found. Settings are valid."));
        item->setForeground(QColor(QLatin1String(Style::kGreenText)));
        m_issueList->addItem(item);
        return;
    }

    for (const auto& issue : issues) {
        auto* item = new QListWidgetItem(
            QStringLiteral("[%1] %2: %3")
                .arg(issue.severity == SettingsHygiene::Severity::Critical ? QStringLiteral("CRIT")
                     : issue.severity == SettingsHygiene::Severity::Warning ? QStringLiteral("WARN")
                     : QStringLiteral("INFO"),
                     issue.summary,
                     issue.detail));

        QColor col{QLatin1String(Style::kTextPrimary)};
        switch (issue.severity) {
            case SettingsHygiene::Severity::Critical:
                col = QColor{QLatin1String(Style::kGaugeDanger)};
                break;
            case SettingsHygiene::Severity::Warning:
                col = QColor{QLatin1String(Style::kAmberWarn)};
                break;
            case SettingsHygiene::Severity::Info:
                col = QColor{QLatin1String(Style::kAccent)};
                break;
        }
        item->setForeground(col);
        m_issueList->addItem(item);
    }
}

void RadioStatusPage::setStationSettingsAvailable(bool available, const QString& reason)
{
    const IStationLink* link = m_model ? m_model->stationLink() : nullptr;
    const bool hygiene = !m_model || m_model->ownsLocalDsp()
        || (link && link->settingsHygieneAvailable());
    const bool paired = !m_model || m_model->ownsLocalDsp()
        || (link && link->signedInWithDeviceKey());
    gateStationControls({m_forgetBtn}, available && hygiene && paired,
        !hygiene ? IStationLink::settingsHygieneUnavailableReason() : paired ? reason
            : QStringLiteral("Pair this computer with the Core to forget its radio settings."));
    // G-38: Repair runs on the Core from a remote window, with Forget's gates.
    const bool repair = !m_model || m_model->ownsLocalDsp()
        || (link && link->settingsRepairAvailable());
    gateStationControls({m_repairBtn}, available && repair && paired,
        !repair ? IStationLink::settingsRepairUnavailableReason() : paired ? reason
            : QStringLiteral("Pair this computer with the Core to repair its radio settings."));
}

} // namespace NereusSDR
