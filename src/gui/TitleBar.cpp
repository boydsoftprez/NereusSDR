// =================================================================
// src/gui/TitleBar.cpp  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR source:
//   src/gui/TitleBar.cpp (especially lines 27-34, 94-104, 282-295)
//
// AetherSDR is licensed under the GNU General Public License v3; see
// https://github.com/ten9876/AetherSDR for the contributor list and
// project-level LICENSE. NereusSDR is also GPLv3. AetherSDR source
// files carry no per-file GPL header; attribution is at project level
// per docs/attribution/HOW-TO-PORT.md rule 6.
//
// Upstream reference: AetherSDR v0.8.16 (2026-04).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Ported/adapted in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Phase 3O Sub-Phase 10 Task 10c.
//                 Scoped-down port: master-output strip only. AetherSDR's
//                 heartbeat / multiFLEX / PC-audio / headphone /
//                 minimal-mode / feature-request widgets intentionally
//                 omitted (deferred to separate phases).
//                 Constructor preserves AetherSDR's 32 px fixed height,
//                 dark background (#0a0a14) with bottom-border (#203040),
//                 and the [menu][stretch][app-name][stretch][master]
//                 layout pattern. `setMenuBar()` is a line-for-line port
//                 of AetherSDR TitleBar.cpp:282-295 (restyle QMenuBar,
//                 `m_hbox->insertWidget(0, mb)`). App-name label: text
//                 "AetherSDR" swapped to "NereusSDR", accent colour
//                 (#00b4d8), font (14 px bold), and QLabel::AlignCenter
//                 preserved verbatim.
//                 Design spec: docs/architecture/2026-04-19-vax-design.md
//                 §6.3 + §7.3.
//   2026-04-20 — Task 10d: added the 💡 feature-request button as the
//                 rightmost element (past MasterOutputWidget, 6 px
//                 spacing). Button construction (lightbulb painter,
//                 28×28 sizing, #3a2a00/#806020 dark-amber style) moved
//                 verbatim from the now-deleted featureBar QToolBar in
//                 MainWindow.cpp. Emits featureRequestClicked(); MainWindow
//                 wires that to showFeatureRequestDialog. Matches
//                 AetherSDR's pattern of the feature button being the
//                 rightmost strip element.
//   2026-04-27 — Phase 3Q-6: implemented ConnectionSegment — state dot,
//                 radio name/IP text, ▲▼ Mbps readout, and 10 Hz
//                 throttled activity LED. Inserted at position 1 in the
//                 hbox (just after the menu bar). Design §4.1.
//   2026-04-30 — Phase 3Q Sub-PR-4 D.1: replaced ConnectionSegment body
//                 per shell-chrome redesign spec §4.1. See TitleBar.h for
//                 the full change description.
// =================================================================

#include "TitleBar.h"

#include "widgets/MasterOutputWidget.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QSize>
#include <QSizePolicy>
#include <QTimer>

namespace NereusSDR {


namespace {

// Strip background + bottom border. From AetherSDR TitleBar.cpp:31.
constexpr auto kStripStyle =
    "TitleBar { background: #0a0a14; border-bottom: 1px solid #203040; }";

// App-name label. From AetherSDR TitleBar.cpp:102.
constexpr auto kAppNameStyle =
    "QLabel { color: #00b4d8; font-size: 14px; font-weight: bold; }";

// Menu-bar restyle. From AetherSDR TitleBar.cpp:285-290.
constexpr auto kMenuBarStyle =
    "QMenuBar { background: transparent; color: #8aa8c0; font-size: 12px; }"
    "QMenuBar::item { padding: 4px 8px; }"
    "QMenuBar::item:selected { background: #203040; color: #ffffff; }"
    "QMenu { background: #0f0f1a; color: #c8d8e8; border: 1px solid #304050; }"
    "QMenu::item:selected { background: #0070c0; }";

// Content-margins + spacing from AetherSDR TitleBar.cpp:34-35.
constexpr int kMarginLeft   = 4;
constexpr int kMarginTop    = 2;
constexpr int kMarginRight  = 8;
constexpr int kMarginBottom = 2;
constexpr int kSpacing      = 6;

// Fixed strip height. From AetherSDR TitleBar.cpp:30.
constexpr int kStripHeight = 32;

} // namespace

// =========================================================================
// ConnectionSegment implementation  (Phase 3Q Sub-PR-4 D.1)
// =========================================================================

ConnectionSegment::ConnectionSegment(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(30);
    setMinimumWidth(200);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
    setAttribute(Qt::WA_StyledBackground, true);

    // Pulse timer — drives the state-dot animation. 750 ms half-period gives
    // a 1.5 s full cycle: visible and calm for streaming (not frantic).
    m_pulseTimer.setInterval(750);
    connect(&m_pulseTimer, &QTimer::timeout, this, [this]() {
        m_pulseOn = !m_pulseOn;
        update();
    });
    m_pulseTimer.start();
}

void ConnectionSegment::setState(ConnectionState s)
{
    if (m_state == s) {
        return;
    }
    m_state = s;

    // Pulse while there is interesting transient state to show.
    if (s == ConnectionState::Connected   ||
        s == ConnectionState::Probing     ||
        s == ConnectionState::Connecting  ||
        s == ConnectionState::LinkLost) {
        m_pulseTimer.start();
    } else {
        m_pulseTimer.stop();
        m_pulseOn = false;
    }
    update();
}

void ConnectionSegment::setRates(double rxMbps, double txMbps)
{
    if (qFuzzyCompare(m_rxMbps + 1.0, rxMbps + 1.0) &&
        qFuzzyCompare(m_txMbps + 1.0, txMbps + 1.0)) {
        return;
    }
    m_rxMbps = rxMbps;
    m_txMbps = txMbps;
    update();
}

void ConnectionSegment::setRttMs(int ms)
{
    if (m_rttMs == ms) {
        return;
    }
    m_rttMs = ms;
    update();
}

void ConnectionSegment::setAudioFlowState(AudioEngine::FlowState s)
{
    if (m_audioFlow == s) {
        return;
    }
    m_audioFlow = s;
    update();
}

void ConnectionSegment::frameTick()
{
    // Throttled activity tick — for now just nudges a repaint so the
    // pulse looks "live". The pulse timer above already drives the
    // animation; this slot exists for future per-frame visual cues.
    update();
}

QColor ConnectionSegment::stateDotColor() const
{
    switch (m_state) {
        case ConnectionState::Connected:
            // m_pulseOn alternates → slow green pulse encoding streaming activity
            return m_pulseOn ? QColor("#5fff8a") : QColor("#3fcf6a");
        case ConnectionState::Probing:
        case ConnectionState::Connecting:
            return m_pulseOn ? QColor("#5fa8ff") : QColor("#3f78cf");
        case ConnectionState::LinkLost:
            return m_pulseOn ? QColor("#ff8c00") : QColor("#cf6c00");
        case ConnectionState::Disconnected:
            return QColor("#ff4040");
    }
    return QColor("#607080");
}

QColor ConnectionSegment::rttColor(int rttMs) const
{
    if (rttMs < 0)    { return QColor("#607080"); }
    if (rttMs < 50)   { return QColor("#5fff8a"); }
    if (rttMs < 150)  { return QColor("#ffd700"); }
    return QColor("#ff6060");
}

QColor ConnectionSegment::audioPipColor(AudioEngine::FlowState s) const
{
    switch (s) {
        case AudioEngine::FlowState::Healthy:  return QColor("#5fa8ff");
        case AudioEngine::FlowState::Underrun: return QColor("#ffd700");
        case AudioEngine::FlowState::Stalled:  return QColor("#ff6060");
        case AudioEngine::FlowState::Dead:     return QColor("#404858");
    }
    return QColor("#404858");
}

QRect ConnectionSegment::rttRect() const
{
    return QRect(m_lastRttX1, 0, m_lastRttX2 - m_lastRttX1, height());
}

QRect ConnectionSegment::audioPipRect() const
{
    return QRect(m_lastPipX1, 0, m_lastPipX2 - m_lastPipX1, height());
}

void ConnectionSegment::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#0f1420"));
    p.drawRoundedRect(rect(), 3, 3);

    p.setFont(QFont(QStringLiteral("SF Mono"), 10, QFont::DemiBold));

    // ── 1. State-encoding dot ──────────────────────────────────────────────
    const QRect dotRect(8, height() / 2 - 5, 10, 10);
    p.setBrush(stateDotColor());
    p.drawEllipse(dotRect);

    int x = dotRect.right() + 8;
    const int textY = height() / 2 + 4;

    if (m_state == ConnectionState::Disconnected) {
        p.setPen(QColor("#607080"));
        p.drawText(x, textY, tr("Disconnected — click to connect"));
        m_lastRttX1 = m_lastRttX2 = 0;
        m_lastPipX1 = m_lastPipX2 = 0;
        return;
    }

    // ── 2. ▲ Mbps (tx uplink) ─────────────────────────────────────────────
    p.setPen(QColor("#a0d8a0"));
    const QString tx = QString::asprintf("\xe2\x96\xb2 %.1f", m_txMbps);   // ▲
    p.drawText(x, textY, tx);
    x += p.fontMetrics().horizontalAdvance(tx) + 10;

    // ── 3. RTT readout — clickable region ─────────────────────────────────
    p.setPen(rttColor(m_rttMs));
    const QString rttText = (m_rttMs < 0)
        ? QStringLiteral("\xe2\x80\x94 ms")               // — ms
        : QString::asprintf("%d ms", m_rttMs);
    p.drawText(x, textY, rttText);
    m_lastRttX1 = x;
    m_lastRttX2 = x + p.fontMetrics().horizontalAdvance(rttText);
    x = m_lastRttX2 + 10;

    // ── 4. ▼ Mbps (rx downlink) ───────────────────────────────────────────
    p.setPen(QColor("#a0d8a0"));
    const QString rx = QString::asprintf("\xe2\x96\xbc %.1f", m_rxMbps);   // ▼
    p.drawText(x, textY, rx);
    x += p.fontMetrics().horizontalAdvance(rx) + 10;

    // ── 5. Vertical separator ─────────────────────────────────────────────
    p.setPen(QColor("#304050"));
    p.drawText(x, textY, QStringLiteral("|"));
    x += p.fontMetrics().horizontalAdvance("|") + 6;

    // ── 6. ♪ audio pip ────────────────────────────────────────────────────
    p.setPen(audioPipColor(m_audioFlow));
    // ♪ U+266A encoded inline as UTF-8 bytes to avoid MSVC narrow-string issues
    const QString pip = QStringLiteral("\xe2\x99\xaa");
    p.drawText(x, textY, pip);
    m_lastPipX1 = x;
    m_lastPipX2 = x + p.fontMetrics().horizontalAdvance(pip);
}

void ConnectionSegment::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        emit contextMenuRequested(event->globalPosition().toPoint());
        return;
    }
    if (event->button() == Qt::LeftButton) {
        if (rttRect().contains(event->pos())) {
            emit rttClicked();
            return;
        }
        if (audioPipRect().contains(event->pos())) {
            emit audioPipClicked();
            return;
        }
        // Disconnected-state: anywhere-click routes to rttClicked so the
        // host can wire both to the Connect / Diagnostics dialog.
        if (m_state == ConnectionState::Disconnected) {
            emit rttClicked();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

// =========================================================================
// TitleBar implementation
// =========================================================================

TitleBar::TitleBar(AudioEngine* audio, QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(kStripHeight);
    setStyleSheet(QLatin1String(kStripStyle));

    m_hbox = new QHBoxLayout(this);
    m_hbox->setContentsMargins(kMarginLeft, kMarginTop, kMarginRight, kMarginBottom);
    m_hbox->setSpacing(kSpacing);

    // Position 0 is reserved for the menu bar (inserted via setMenuBar()).
    // The ConnectionSegment sits at position 1 (or 0 before the menu is
    // inserted), between the menu bar and the centre label stretch.

    // ── ConnectionSegment — Phase 3Q-6 ─────────────────────────────────
    // Inserted as the first item. setMenuBar() will prepend the menu bar
    // at index 0 pushing this to index 1. Until setMenuBar() runs the
    // segment sits at index 0 — acceptable; it just moves right once the
    // menu arrives.
    m_connectionSegment = new ConnectionSegment(this);
    m_hbox->addWidget(m_connectionSegment);

    // ── Left stretch ───────────────────────────────────────────────────────
    m_hbox->addStretch(1);

    // ── App-name label ─────────────────────────────────────────────────────
    // From AetherSDR TitleBar.cpp:101-104 — text swapped to "NereusSDR".
    auto* appName = new QLabel(QStringLiteral("NereusSDR"), this);
    appName->setStyleSheet(QLatin1String(kAppNameStyle));
    appName->setAlignment(Qt::AlignCenter);
    m_hbox->addWidget(appName);

    // ── Right stretch ──────────────────────────────────────────────────────
    m_hbox->addStretch(1);

    // ── MasterOutputWidget — Task 10b composite ────────────────────────────
    m_master = new MasterOutputWidget(audio, this);
    m_hbox->addWidget(m_master);

    // ── 💡 Feature-request button — Task 10d ───────────────────────────────
    // Construction moved verbatim from the now-deleted featureBar QToolBar
    // in MainWindow.cpp (Phase 3G-14). The button lives at the far right
    // of the TitleBar strip, past the MasterOutputWidget — this matches
    // AetherSDR's pattern where the feature button is the rightmost strip
    // element.
    m_hbox->addSpacing(6);

    // Paint a lightbulb icon so it renders cleanly at any DPI.
    auto makeBulbIcon = [](QColor bulbColor, QColor baseColor) -> QIcon {
        constexpr int sz = 64;  // paint large, Qt scales down
        QPixmap pm(sz, sz);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);

        // Bulb (circle)
        p.setPen(Qt::NoPen);
        p.setBrush(bulbColor);
        p.drawEllipse(QRectF(14, 4, 36, 36));

        // Neck (trapezoid connecting bulb to base)
        QPolygonF neck;
        neck << QPointF(22, 36) << QPointF(42, 36)
             << QPointF(40, 44) << QPointF(24, 44);
        p.drawPolygon(neck);

        // Base (screw threads — 3 thin lines)
        p.setPen(QPen(baseColor, 2.5));
        p.drawLine(QPointF(24, 46), QPointF(40, 46));
        p.drawLine(QPointF(25, 50), QPointF(39, 50));
        p.drawLine(QPointF(27, 54), QPointF(37, 54));

        // Tip
        p.setPen(Qt::NoPen);
        p.setBrush(baseColor);
        p.drawEllipse(QRectF(29, 56, 6, 4));

        // Filament lines inside bulb
        p.setPen(QPen(baseColor, 1.5));
        p.drawLine(QPointF(28, 34), QPointF(28, 22));
        p.drawLine(QPointF(28, 22), QPointF(32, 16));
        p.drawLine(QPointF(32, 16), QPointF(36, 22));
        p.drawLine(QPointF(36, 22), QPointF(36, 34));

        p.end();
        return QIcon(pm);
    };

    QIcon bulbIcon = makeBulbIcon(QColor(0xFF, 0xD0, 0x60), QColor(0x80, 0x60, 0x20));

    m_featureBtn = new QPushButton(this);
    m_featureBtn->setObjectName(QStringLiteral("featureButton"));
    m_featureBtn->setIcon(bulbIcon);
    m_featureBtn->setIconSize(QSize(22, 22));
    m_featureBtn->setFixedSize(28, 28);
    m_featureBtn->setToolTip(QStringLiteral("Submit a feature request or bug report"));
    m_featureBtn->setAccessibleName(QStringLiteral("Feature request"));
    m_featureBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #3a2a00; border: 1px solid #806020; "
        "border-radius: 4px; padding: 0; }"
        "QPushButton:hover { background: #504000; border-color: #a08030; }"));
    connect(m_featureBtn, &QPushButton::clicked,
            this, &TitleBar::featureRequestClicked);
    m_hbox->addWidget(m_featureBtn);
}

void TitleBar::setMenuBar(QMenuBar* mb)
{
    // Ported line-for-line from AetherSDR TitleBar.cpp:282-295.
    if (!mb) {
        return;
    }
    mb->setStyleSheet(QLatin1String(kMenuBarStyle));
    mb->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    m_menuBar = mb;
    // Insert at position 0 (before the first stretch).
    m_hbox->insertWidget(0, mb);
}

} // namespace NereusSDR
