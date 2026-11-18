// =================================================================
// src/gui/TitleBar.cpp  (Longpath)
// =================================================================
//
// Ported from AetherSDR source:
//   src/gui/TitleBar.cpp (especially lines 27-34, 94-104, 282-295)
//
// AetherSDR is licensed under the GNU General Public License v3; see
// https://github.com/ten9876/AetherSDR for the contributor list and
// project-level LICENSE. Longpath is also GPLv3. AetherSDR source
// files carry no per-file GPL header; attribution is at project level
// per docs/attribution/HOW-TO-PORT.md rule 6.
//
// Upstream reference: AetherSDR v0.8.16 (2026-04).
//
// =================================================================
// Modification history (Longpath):
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
//                 "AetherSDR" swapped to "Longpath", accent colour
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
//   2026-08-02 — Task A7 (bottom-banner cleanup): single-row UTC clock,
//                 inserted after MasterOutputWidget and before the 💡
//                 feature-request button. See TitleBar.h for rationale.
//   2026-09-21 — Longpath: die Version steht immer neben dem Namen
//                 (Betreiber: "die Version sollte man auch sehen!"),
//                 der Bau-Tag nur bei Entwicklungsbauten dahinter.
// =================================================================

#include "TitleBar.h"
#include "gui/styles/ThemeQss.h"

#include "core/BuildIdentity.h"

#include "StyleConstants.h"
#include "widgets/MasterOutputWidget.h"

#include <QDateTime>
#include <QEvent>
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
#include <QToolButton>

namespace Longpath {


namespace {

// Content-margins + spacing from AetherSDR TitleBar.cpp:34-35.
constexpr int kMarginLeft   = 4;
constexpr int kMarginTop    = 2;
constexpr int kMarginRight  = 8;
constexpr int kMarginBottom = 2;
constexpr int kSpacing      = 6;

// Fixed strip height. From AetherSDR TitleBar.cpp:30.
constexpr int kStripHeight = 32;


// Steht im Tooltip der Aktualisierungsanzeige, solange es keinen
// besonderen Grund gibt. An EINER Stelle, weil sie zweimal gebraucht
// wird: beim Anlegen und bei jedem Zustandswechsel ohne Grund.
QString kAktualisierungHinweis()
{
    return TitleBar::tr(
        "How this build compares with the latest published release. "
        "Checked at every start; Help → Check for Updates… checks "
        "right away.");
}

} // namespace

// =========================================================================
// ConnectionSegment implementation  (Phase 3Q Sub-PR-4 D.1)
// =========================================================================

ConnectionSegment::ConnectionSegment(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(30);
    // 200 -> 330 am 2026-08-22: mit der Verlustanzeige hinter den
    // Mbit/s reichten 200 Punkte nicht mehr, und der neue Text wurde
    // WORTLOS abgeschnitten. Das ist die zweite Falle desselben Tages —
    // erst kam die Verbindung wegen Qt::UniqueConnection an einem
    // Lambda nicht zustande, dann war das Ergebnis zu breit fuer die
    // Leiste. Beide Male sah der Betreiber schlicht nichts.
    setMinimumWidth(330);
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
    // ── Beim Verbindungswechsel keinen FREMDEN Wert stehenlassen ─────
    //
    // Die Anzeige haelt bewusst den schlechtesten Wert der letzten
    // Minute — sonst sieht man im ruhigen Moment 0,0 und glaubt, es
    // sei alles gut. Ueber einen GERAETEWECHSEL hinweg ist dasselbe
    // Verhalten aber eine Luege: der Betreiber sah am 2026-08-22 am
    // ANAN-10/100 den Verlustwert des Anvelina, weil Protokoll 1
    // damals gar nicht mass. Es misst jetzt — aber der Uebergang
    // gehoert trotzdem geloescht.
    if (s != m_state) {
        m_lossPct      = -1.0;
        m_lossWorstPct = -1.0;
        m_lossWorstMs  = 0;
    }

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


void ConnectionSegment::setRadioLinkKind(RadioLinkKind kind)
{
    if (m_linkKind == kind) { return; }
    m_linkKind = kind;
    update();
}

void ConnectionSegment::setPacketLoss(double lossPercent)
{
    m_lossPct = lossPercent;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (lossPercent > m_lossWorstPct || now - m_lossWorstMs > 60000) {
        m_lossWorstPct = lossPercent;
        m_lossWorstMs  = now;
    }
    update();
}

void ConnectionSegment::onIqPacketLoss(double lossPercent, quint32, quint32)
{
    setPacketLoss(lossPercent);
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
            return m_pulseOn ? QColor("#6fa384") : QColor("#33684c");
        case ConnectionState::Probing:
        case ConnectionState::Connecting:
            return m_pulseOn ? QColor("#4a7ba8") : QColor(Style::kBluePulseDim);
        case ConnectionState::LinkLost:
            return m_pulseOn ? QColor("#c2924f") : QColor("#6b5426");
        case ConnectionState::Disconnected:
            // ── Warum hier warn und beim DISCONNECTED-Schild nicht ──
            //
            // Beides heisst "keine Verbindung", und beide tragen
            // absichtlich verschiedene Toene:
            //
            //   LinkLost      warn   -- die Verbindung STAND und ist
            //                          weggebrochen. Etwas ist
            //                          passiert, der Betreiber hat es
            //                          nicht veranlasst, und das Geraet
            //                          versucht gerade wieder.
            //   DISCONNECTED  Skala  -- gar nicht verbunden. Ein
            //                          Ruhezustand, kein Vorfall; das
            //                          grosse Schild im Spektrum sagt
            //                          nur, dass nichts anliegt.
            //
            // Rot ist es in beiden Faellen nicht: es geht nichts kaputt,
            // und es verbindet sich wieder.
            //
            // Steht hier, damit die zwei Antworten nicht in einem
            // halben Jahr auseinanderlaufen. Wer eine davon aendert,
            // aendert die andere mit oder streicht diesen Absatz.
            return QColor("#a8853f");
    }
    return QColor(Style::kTextGroupHead);
}

QColor ConnectionSegment::rttColor(int rttMs) const
{
    if (rttMs < 0)    { return QColor(Style::kTextGroupHead); }
    if (rttMs < 50)   { return QColor("#6fa384"); }
    if (rttMs < 150)  { return QColor("#c2924f"); }
    return QColor("#a8853f");
}

QColor ConnectionSegment::audioPipColor(AudioEngine::FlowState s) const
{
    switch (s) {
        case AudioEngine::FlowState::Healthy:  return QColor("#4a7ba8");
        case AudioEngine::FlowState::Underrun: return QColor("#c2924f");
        case AudioEngine::FlowState::Stalled:  return QColor("#a8853f");
        case AudioEngine::FlowState::Dead:     return QColor("#2c2c31");
    }
    return QColor("#2c2c31");
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
    p.setBrush(QColor(Style::hexRole(Style::kAppBg)));   // war das rohe #08080a
    p.drawRoundedRect(rect(), 3, 3);

    // Vorher QFont("SF Mono", ...) mit nur diesem einen Namen -- und die
    // Schrift ist auf macOS nicht angemeldet, Qt nahm also irgendeine.
    p.setFont(Style::monoFontPt(10, QFont::DemiBold));

    // ── 1. State-encoding dot ──────────────────────────────────────────────
    const QRect dotRect(8, height() / 2 - 5, 10, 10);
    p.setBrush(stateDotColor());
    p.drawEllipse(dotRect);

    int x = dotRect.right() + 8;
    const int textY = height() / 2 + 4;

    if (m_state == ConnectionState::Disconnected) {
        p.setPen(QColor(Style::kTextGroupHead));
        p.drawText(x, textY, tr("Disconnected — click to connect"));
        m_lastRttX1 = m_lastRttX2 = 0;
        m_lastPipX1 = m_lastPipX2 = 0;
        return;
    }

    // ── 2. ▲ Mbps — Longpath → radio (commands; small, kbps territory) ──
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
    p.setPen(QColor("#6fa384"));
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

    // ── 4. ▼ Mbps — radio → Longpath (I/Q stream; large, Mbps) ──────────
    // Reads m_rxMbps which is the call-site's "radio→client" byte rate
    // (RadioConnection::rxByteRate, recorded per inbound packet at
    // RadioConnection.cpp:1272 [@HEAD]). Client perspective: ▼ = down
    // = downloading I/Q from the radio.
    //
    // QChar(0x25BC) for the same reason as the up-triangle above.
    p.setPen(QColor("#6fa384"));
    const QString rx = QChar(0x25BC) + QString::asprintf(" %.1f Mbps", m_rxMbps);
    p.drawText(x, textY, rx);
    x += p.fontMetrics().horizontalAdvance(rx) + 10;

    // ── 4b. Paketverlust im I/Q-Strom ────────────────────────────────
    //
    // Die Zahl, die dem Betreiber drei Tage gefehlt hat. Sie stand die
    // ganze Zeit im Protokoll (P2RadioConnection: "IQ seq audit"), nur
    // sah sie nie jemand — also blieb "es ruckt" eine Frage ohne
    // Messung, und der Verdacht fiel auf die App statt auf das WLAN.
    //
    // Gezeigt wird der SCHLECHTESTE Wert der letzten Minute, nicht der
    // augenblickliche: Verlust kommt in Stoessen, und wer im ruhigen
    // Moment hinsieht, sieht 0,0 und glaubt, es sei alles gut.
    //
    // Schwellen, absichtlich streng: Protokoll 2 wiederholt NICHTS.
    // Jedes verlorene Paket ist ein Loch im Ton — 0,1 % sind schon
    // hoerbar, 1 % ist unbrauchbar.
    if (m_lossWorstPct >= 0.0 && m_state == ConnectionState::Connected) {
        const double v = m_lossWorstPct;
        QColor c;
        if (v < 0.01)      { c = QColor(Style::kTextInactive); }   // still, unauffaellig
        else if (v < 0.10) { c = QColor("#6fa384"); }
        else if (v < 1.00) { c = QColor(Style::kAmberText); }
        else               { c = QColor(Style::kRedText); }
        p.setPen(c);
        // Das WORT traegt die Warnung, nicht die Farbe. Der Hausstil
        // haelt Farbe knapp (2 %), und gesaettigtes Rot ist der
        // Bandkante und dem SWR vorbehalten — beides Dinge, bei denen
        // es um die Endstufe geht. Ein nacktes "1.40 %" neben den
        // Mbit/s waere ausserdem mehrdeutig: Verlust wovon?
        QString loss = (v < 0.01)
            ? QStringLiteral("LOSS 0 %")
            : QString::asprintf("LOSS %.2f %%", v);

        // ── Woher der Verlust kommt, gleich daneben ─────────────────
        //
        // Nur wenn ueberhaupt etwas verloren geht UND der Weg
        // verlustanfaellig ist. Ein "WLAN" neben "LOSS 0 %" waere eine
        // Belehrung ohne Anlass; steht dort aber eine Zahl, ist es die
        // Antwort auf die Frage, die der Betreiber gerade stellt.
        if (v >= 0.01) {
            if (m_linkKind == RadioLinkKind::Wireless) {
                loss += QStringLiteral(" \u00b7 WLAN");
            } else if (m_linkKind == RadioLinkKind::Virtual) {
                loss += QStringLiteral(" \u00b7 Tunnel");
            }
        }
        p.drawText(x, textY, loss);
        x += p.fontMetrics().horizontalAdvance(loss) + 10;
    }

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
    // From AetherSDR TitleBar.cpp:101-104 — text swapped to "Longpath".
    auto* appName = new QLabel(QStringLiteral("Longpath"), this);
    // App-name label. From AetherSDR TitleBar.cpp:102.
    appName->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 16px; font-weight: bold; }")
                           .arg(QLatin1String(Style::kAccent)));
    appName->setAlignment(Qt::AlignCenter);
    m_hbox->addWidget(appName);

    // Version and build identity, beside the name.
    //
    // They are already in the window title, but the window title is not
    // visible full screen — and this session lost time twice to the
    // question "is that the build we just made or the installed copy".
    // A branch@sha you can read at a glance settles it. Betreiber
    // 2026-09-21 ("die Version sollte man auch sehen!"): on a release
    // build the tag is empty and nothing showed at all -- the version
    // now stands here always, the tag joins it on a development build.
    QString identity = QStringLiteral(LONGPATH_VERSION);
    const QString tag = BuildIdentity::buildTag();
    if (!tag.isEmpty()) {
        identity += QStringLiteral(" · %1").arg(tag);
    }
    {
        auto* build = new QLabel(identity, this);
        build->setObjectName(QStringLiteral("titleBarVersion"));
        build->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; }")
            .arg(QLatin1String(Style::kTextScale)));
        build->setToolTip(tag.isEmpty()
            ? QStringLiteral("Longpath version")
            : QStringLiteral("Version, branch and commit this binary was built from"));
        m_hbox->addSpacing(6);
        m_hbox->addWidget(build);
    }

    // ── Der Stand der Aktualisierung, immer sichtbar ─────────────────
    //
    // „checking for update sollte auch immer automatisch angezeigt
    //  werden nach dem neustart!"
    //
    // Die stille Prüfung beim Start gab es schon, aber sie meldete
    // sich NUR, wenn es etwas Neueres gab. „Nichts gesehen" hiess
    // damit zweierlei: alles aktuell, oder gar nicht nachgesehen --
    // und genau diese Unsicherheit hat heute früh eine Stunde
    // gekostet, als die Fassung im Dock eine andere war als die
    // gebaute.
    //
    // Also steht der Stand jetzt neben der Version: „aktuell",
    // „v0.7.0 verfügbar" oder „nicht erreichbar". Kein Fenster, das
    // bei jedem Start aufgeht -- eine Zeile, die man im Vorbeigehen
    // liest. Ein Dialog, der jedes Mal sagt „alles in Ordnung", wird
    // nach drei Tagen weggeklickt, ohne gelesen zu werden.
    m_updateState = new QLabel(this);
    m_updateState->setObjectName(QStringLiteral("titleBarUpdateState"));
    m_updateState->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
        .arg(QLatin1String(Style::kTextInactive)));
    m_updateState->setText(tr("· checking …"));
    m_updateState->setToolTip(kAktualisierungHinweis());
    m_hbox->addSpacing(6);
    m_hbox->addWidget(m_updateState);

    // ── Right stretch ──────────────────────────────────────────────────────
    m_hbox->addStretch(1);

    // ── UTC clock — Task A7 ─────────────────────────────────────────────────
    // Single-row UTC, moved here from the bottom banner. Every desktop OS
    // already puts a clock top-right; the one fact worth duplicating here
    // is UTC, since it's the one the OS clock doesn't give an operator for
    // logging. Dropping it from the banner frees that corner for alarms
    // alone.
    //
    // Placed BEFORE MasterOutputWidget, with a deliberate gap after it.
    // Sitting immediately after the slider it read as part of that control
    // and invited a mis-drag on a widget where an accidental grab changes
    // audio level (bench feedback, 2026-08-03).
    m_utcLabel = new QLabel(this);
    m_utcLabel->setToolTip(tr("UTC time"));
    m_utcLabel->setStyleSheet(Style::themed(QStringLiteral(
        "QLabel { color: %1; font-size: 11px;"
        " font-family: " LP_MONO_QSS "; }")
            .arg(QString::fromLatin1(Style::kTextSecondary))));
    m_hbox->addWidget(m_utcLabel);
    m_hbox->addSpacing(24);

    // ── MasterOutputWidget — Task 10b composite ────────────────────────────
    m_master = new MasterOutputWidget(audio, this);
    m_hbox->addWidget(m_master);
    m_hbox->addSpacing(10);

    auto tickUtc = [this]() {
        m_utcLabel->setText(QDateTime::currentDateTimeUtc()
                                .toString(QStringLiteral("hh:mm:ss UTC")));
    };
    tickUtc();  // populate before the first timer fire
    m_utcTimer = new QTimer(this);
    connect(m_utcTimer, &QTimer::timeout, this, tickUtc);
    m_utcTimer->start(1000);

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

    // Grau, nicht Bernstein: der Knopf ist Inventar ("Wunsch melden"),
    // kein Messwert und kein Zustand -- er war die einzige gesaettigte
    // Flaeche in der Kopfleiste (Foto vom 2026-09-17). Glas & Tiefe:
    // ein erhabener Knopf wie jeder andere, die Birne als Zeichnung.
    QIcon bulbIcon = makeBulbIcon(QColor(Style::kTextSecondary), QColor(Style::kTextScale));

    m_featureBtn = new QPushButton(this);
    m_featureBtn->setObjectName(QStringLiteral("featureButton"));
    m_featureBtn->setIcon(bulbIcon);
    m_featureBtn->setIconSize(QSize(22, 22));
    m_featureBtn->setFixedSize(28, 28);
    m_featureBtn->setToolTip(QStringLiteral("Submit a feature request or bug report"));
    m_featureBtn->setAccessibleName(QStringLiteral("Feature request"));
    m_featureBtn->setStyleSheet(Style::buttonBaseStyle()
                                + QStringLiteral("QPushButton { padding: 0; }"));
    connect(m_featureBtn, &QPushButton::clicked,
            this, &TitleBar::featureRequestClicked);
    m_hbox->addWidget(m_featureBtn);
}

void TitleBar::setLinkerRand(int punkte)
{
    if (m_hbox == nullptr) { return; }
    const QMargins m = m_hbox->contentsMargins();
    m_hbox->setContentsMargins(punkte, m.top(), m.right(), m.bottom());
}

void TitleBar::setUpdateState(const QString& text,
                              AktualisierungsStand art,
                              const QString& grund)
{
    if (m_updateState == nullptr) { return; }
    const char* farbe = Style::kTextInactive;
    switch (art) {
    case AktualisierungsStand::Aktuell:     farbe = Style::kGreenText;    break;
    case AktualisierungsStand::Neuer:       farbe = Style::kAmberText;    break;
    case AktualisierungsStand::Unbekannt:   farbe = Style::kTextInactive; break;
    }
    m_updateState->setText(text);
    m_updateState->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
        .arg(QLatin1String(farbe)));
    // Der Grund tritt an die Stelle der allgemeinen Erklaerung, aber
    // nur wenn es einen gibt -- sonst stuende nach einem „up to date"
    // gar nichts mehr im Tooltip.
    m_updateState->setToolTip(grund.isEmpty() ? kAktualisierungHinweis() : grund);
}

void TitleBar::setMenuBar(QMenuBar* mb)
{
    // Ported line-for-line from AetherSDR TitleBar.cpp:282-295.
    if (!mb) {
        return;
    }
    // Menu-bar restyle. From AetherSDR TitleBar.cpp:285-290.
    mb->setStyleSheet(QStringLiteral(
        "QMenuBar { background: transparent; color: %1; font-size: 13px; }"
        "QMenuBar::item { padding: 4px 8px; }"
        "QMenuBar::item:selected { background: %2; color: %7; }"
        "QMenu { background: %3; color: %5; border: 1px solid %4; }"
        "QMenu::item:selected { background: %6; }")
        .arg(QLatin1String(Style::kTitleText),
             QLatin1String(Style::kBorderSubtle),
             QLatin1String(Style::kAppBg),
             QLatin1String(Style::kOverlayBorder),
             QLatin1String(Style::kTextPrimary),
             QLatin1String(Style::kBlueBg))
        .arg(QLatin1String(Style::kBlueText)));
    mb->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    // Longpath: die „»"-Taste sichtbar machen (2026-11-17, siehe Kopf).
    faerbeMenueErweiterung(mb);
    m_menuBar = mb;
    // Insert at position 0 (before the first stretch).
    m_hbox->insertWidget(0, mb);
}

// ── Die „»"-Taste der Menueleiste (Longpath, 2026-11-17) ────────────
//
// Herleitung: TitleBar.h. Qt baut die Taste selbst (QMenuBarExtension,
// objectName „qt_menubar_ext_button") und gibt ihr das Symbol des
// Systemstils; faerben laesst sich ein solches Symbol nicht. Darum ein
// eigenes: zwei Winkel in der dunklen Schrift der Sendetaste, auf einer
// Flaeche im Orange des Plus -- dieselbe Taste wie das Plus, nur kleiner.
namespace {

QIcon doppelpfeil()
{
    // Qt gibt der Taste nur etwa zwoelf Punkte Breite (der Stil legt das
    // fest, ein Stilblatt aendert es nicht). Das Zeichen fuellt sie
    // darum fast ganz: 10 x 14, zwei Winkel ueber die volle Breite.
    constexpr int kBreite = 10;
    constexpr int kHoehe = 14;
    constexpr qreal kDichte = 3.0;   // scharf auch auf Retina
    QPixmap bild(QSize(kBreite, kHoehe) * kDichte);
    bild.setDevicePixelRatio(kDichte);
    bild.fill(Qt::transparent);
    QPainter g(&bild);
    g.setRenderHint(QPainter::Antialiasing, true);
    QPen stift(QColor(QString::fromLatin1(Style::kTxKeyText)), 1.8);
    stift.setCapStyle(Qt::RoundCap);
    stift.setJoinStyle(Qt::RoundJoin);
    g.setPen(stift);
    for (const qreal x : {1.5, 5.5}) {
        g.drawPolyline(QPolygonF{QPointF(x, 3.0), QPointF(x + 3.0, 7.0),
                                 QPointF(x, 11.0)});
    }
    return QIcon(bild);
}

// Qt setzt der Taste bei einem Stilwechsel wieder das Systemsymbol.
// Dieser kleine Waechter setzt unseres zurueck.
class PfeilWaechter : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject* ziel, QEvent* ereignis) override
    {
        if (ereignis->type() == QEvent::StyleChange) {
            if (auto* taste = qobject_cast<QToolButton*>(ziel)) {
                taste->setIcon(doppelpfeil());
            }
        }
        return QObject::eventFilter(ziel, ereignis);
    }
};

} // namespace

void TitleBar::faerbeMenueErweiterung(QMenuBar* mb)
{
    if (!mb) { return; }
    auto* taste = mb->findChild<QToolButton*>(QStringLiteral("qt_menubar_ext_button"));
    if (!taste) { return; }
    taste->setIcon(doppelpfeil());
    taste->setIconSize(QSize(10, 14));
    taste->setToolTip(tr("More menus"));
    // Schon orange: keine zweite Marke darueber (widgets/KlickMarke.h).
    taste->setProperty("klickmarke", false);
    taste->setStyleSheet(QStringLiteral(
        "QToolButton#qt_menubar_ext_button {"
        " background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2);"
        " border: 1px solid %3; border-radius: 4px; padding: 0px; }"
        "QToolButton#qt_menubar_ext_button::menu-indicator { image: none; }")
        .arg(QLatin1String(Style::kPlusTop), QLatin1String(Style::kPlusBottom),
             QLatin1String(Style::kPlusBorder)));
    // Genau ein Waechter je Taste, auch wenn diese Funktion zweimal
    // laeuft (eine Eigenschaft statt findChild: der Waechter traegt kein
    // Q_OBJECT und braucht keines).
    if (!taste->property("pfeilWaechter").toBool()) {
        taste->installEventFilter(new PfeilWaechter(taste));
        taste->setProperty("pfeilWaechter", true);
    }
}

QString TitleBar::utcText() const
{
    return m_utcLabel ? m_utcLabel->text() : QString();
}

} // namespace Longpath
