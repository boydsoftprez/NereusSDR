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
// =================================================================

#include "TitleBar.h"

#include "widgets/MasterOutputWidget.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenuBar>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QSize>
#include <QSizePolicy>

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

TitleBar::TitleBar(AudioEngine* audio, QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(kStripHeight);
    setStyleSheet(QLatin1String(kStripStyle));

    m_hbox = new QHBoxLayout(this);
    m_hbox->setContentsMargins(kMarginLeft, kMarginTop, kMarginRight, kMarginBottom);
    m_hbox->setSpacing(kSpacing);

    // Position 0 is reserved for the menu bar (inserted via setMenuBar()).
    // Until setMenuBar() runs, the layout is [stretch][app-name][stretch][master].

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
