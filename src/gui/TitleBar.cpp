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

#include "StyleConstants.h"
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
    // Track the latest raw value so callers can read it back for
    // diagnostics; the painted value comes from smoothedRttMs().
    m_rttMs = ms;

    // Negative samples ("no rtt available") clear the smoothing queue
    // — re-attaching produces "— ms" until real samples flow again.
    if (ms < 0) {
        m_rttSamples.clear();
        update();
        return;
    }

    m_rttSamples.enqueue(ms);
    while (m_rttSamples.size() > kRttSmoothingWindow) {
        m_rttSamples.dequeue();
    }
    update();
}

int ConnectionSegment::smoothedRttMs() const noexcept
{
    if (m_rttSamples.isEmpty()) {
        return m_rttMs;   // -1 when there's been no real sample yet
    }

    // Min-filtered RTT — 2nd-smallest of the rolling window.
    //
    // The radio sends status packets on its own periodic cadence
    // (P1: 380.95 pps → 2.6 ms; P2: 100 ms HighPriority status)
    // independent of our outbound commands. Each measured sample is
    // therefore  true_RTT + uniform(0, cadence)  — the additive term
    // is bracket noise from how long the radio waited before its next
    // status emit. A rolling MEAN preserves that noise and produces
    // two pathological readings:
    //   - LAN with sub-ms RTT reads ~half the cadence (P1 floor ~1.3 ms)
    //   - WAN with cadence-sized RTT becomes random noise — a few
    //     "lucky" samples where status arrived right after our cmd
    //     can pull the mean way below true_RTT.
    //
    // The MINIMUM across a window approaches true_RTT (the lucky
    // sample where the radio's cadence noise was ~0). Same technique
    // TCP BBR uses for its RTT estimator. We use the 2nd-smallest
    // (rather than the absolute minimum) to winsorize a single
    // anomalously-low outlier — protects against a transient
    // sub-millisecond glitch pulling the display below the true RTT.
    if (m_rttSamples.size() == 1) {
        return m_rttSamples.first();
    }
    QList<int> sorted;
    sorted.reserve(m_rttSamples.size());
    for (int s : m_rttSamples) { sorted.append(s); }
    std::sort(sorted.begin(), sorted.end());
    return sorted.at(1);
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

    // ── 2. ▲ Mbps — NereusSDR → radio (commands; small, kbps territory) ──
    // Reads m_txMbps which is the call-site's "client→radio" byte rate
    // (RadioConnection::txByteRate, recorded per outbound packet at
    // RadioConnection.cpp:1914 [@HEAD]). Client perspective: ▲ = up
    // = uploading commands to the radio.
    //
    // Glyph is built via QChar(0x25B2) rather than a UTF-8 byte-escape
    // string passed to QString::asprintf — same fix as ebe9030 applied
    // to the audio pip. asprintf with a leading "\xe2\x96\xb2" prefix
    // gets misinterpreted as Latin-1 codepoints on the macOS compile
    // path, rendering as garbage rather than a triangle.
    p.setPen(QColor("#a0d8a0"));
    const QString tx = QChar(0x25B2) + QString::asprintf(" %.1f Mbps", m_txMbps);
    p.drawText(x, textY, tx);
    x += p.fontMetrics().horizontalAdvance(tx) + 10;

    // ── 3. RTT readout (smoothed) — clickable region ──────────────────────
    // Uses the rolling-mean smoothedRttMs() so the readout calms instead
    // of jumping per-ping. Color thresholds operate on the smoothed
    // value too, so the green/yellow/red transitions don't flicker.
    const int rttDisplay = smoothedRttMs();
    p.setPen(rttColor(rttDisplay));
    // QChar(0x2014) for the em-dash placeholder — same byte-escape
    // misinterpretation risk as the Mbps glyphs above; build via QChar.
    const QString rttText = (rttDisplay < 0)
        ? QChar(0x2014) + QStringLiteral(" ms")
        : QString::asprintf("%d ms", rttDisplay);
    p.drawText(x, textY, rttText);
    m_lastRttX1 = x;
    m_lastRttX2 = x + p.fontMetrics().horizontalAdvance(rttText);
    x = m_lastRttX2 + 10;

    // ── 4. ▼ Mbps — radio → NereusSDR (I/Q stream; large, Mbps) ──────────
    // Reads m_rxMbps which is the call-site's "radio→client" byte rate
    // (RadioConnection::rxByteRate, recorded per inbound packet at
    // RadioConnection.cpp:1272 [@HEAD]). Client perspective: ▼ = down
    // = downloading I/Q from the radio.
    //
    // QChar(0x25BC) for the same reason as the up-triangle above.
    p.setPen(QColor("#a0d8a0"));
    const QString rx = QChar(0x25BC) + QString::asprintf(" %.1f Mbps", m_rxMbps);
    p.drawText(x, textY, rx);
    x += p.fontMetrics().horizontalAdvance(rx) + 10;

    // ── 5. ● audio pip ────────────────────────────────────────────────────
    // Was: vertical separator "|" + ♪ (U+266A). ♪ is absent in Menlo (the
    // SF Mono fallback on macOS), rendering as garbage. Replaced with ●
    // (U+25CF BLACK CIRCLE), universally available in every monospace font.
    // The circle colour already encodes audio state — semantically cleaner.
    p.setPen(audioPipColor(m_audioFlow));
    // Use QChar(0x25CF) directly rather than a byte-escape QStringLiteral —
    // \xe2\x97\x8f gets misinterpreted as 3 Latin-1 codepoints (â + 2 control
    // chars) on some compile-paths, rendering as garbage. QChar(0x25CF) is
    // unambiguous: a single UTF-16 code unit pointing to ● (U+25CF).
    const QString pip(QChar(0x25CF));   // ● BLACK CIRCLE — color = audio state
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
    // Strip background + bottom border. From AetherSDR TitleBar.cpp:31.
    setStyleSheet(QStringLiteral("TitleBar { background: %1; border-bottom: 1px solid %2; }")
                  .arg(QLatin1String(Style::kStatusBarBg),
                       QLatin1String(Style::kBorderSubtle)));

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
    // App-name label. From AetherSDR TitleBar.cpp:102.
    appName->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 14px; font-weight: bold; }")
                           .arg(QLatin1String(Style::kAccent)));
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
    // NereusSDR-original — amber dark for the 💡 feature-request button.
    // One-off; no palette promotion warranted per §A2 design intent.
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
    // Menu-bar restyle. From AetherSDR TitleBar.cpp:285-290.
    mb->setStyleSheet(QStringLiteral(
        "QMenuBar { background: transparent; color: %1; font-size: 12px; }"
        "QMenuBar::item { padding: 4px 8px; }"
        "QMenuBar::item:selected { background: %2; color: #ffffff; }"
        "QMenu { background: %3; color: %5; border: 1px solid %4; }"
        "QMenu::item:selected { background: %6; }")
        .arg(QLatin1String(Style::kTitleText),
             QLatin1String(Style::kBorderSubtle),
             QLatin1String(Style::kAppBg),
             QLatin1String(Style::kOverlayBorder),
             QLatin1String(Style::kTextPrimary),
             QLatin1String(Style::kBlueBg)));
    mb->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    m_menuBar = mb;
    // Insert at position 0 (before the first stretch).
    m_hbox->insertWidget(0, mb);
}

} // namespace NereusSDR
