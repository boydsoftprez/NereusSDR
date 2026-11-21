#include "TxChainSection.h"

#include "gui/ComboStyle.h"
#include "gui/StyleConstants.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>

// =================================================================
// src/gui/applets/TxChainSection.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs - original licence from Thetis
//   source is included below.
//
// Extracted from src/gui/applets/TxApplet.h: the LEV, EQ and CFC buttons
// and the TX Profile combo.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-25 - The row's parts (SectionParts), which a container copy
//                 hides and orders from its entry config. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-20 - A copy of this row in a container is told the outer
//                 margins the applet's column gives the row (outerMargins);
//                 the applet's own row is told none.
//                 Brian Sutton, AI-assisted via OpenCode.
//   2026-10-19 - Extracted from TxApplet.{h,cpp} as part of splitting the
//                 TX applet into per-concern section objects. Behaviour,
//                 appearance, object names and tooltips are unchanged.
//                 Brian Sutton, AI-assisted via OpenAI Codex.
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines.
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

// =================================================================
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   Layout pattern from AetherSDR `src/gui/TxApplet.{h,cpp}`.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.

namespace NereusSDR {

namespace {

// A badge that behaves like a small button: either mouse button asks for
// that stage's adjustments, matching the EQ/CFC buttons' right-click.
class BadgeLabel : public QLabel
{
public:
    using QLabel::QLabel;
    std::function<void()> onClick;

protected:
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if ((event->button() == Qt::LeftButton
             || event->button() == Qt::RightButton)
            && rect().contains(event->position().toPoint())) {
            if (onClick) {
                onClick();
            }
            event->accept();
            return;
        }
        QLabel::mouseReleaseEvent(event);
    }
};

} // namespace

TxChainSection::TxChainSection(QWidget* parent, int rowSpacing,
                               const QMargins& outerMargins)
    : QWidget(parent)
{
    m_outerMargins = outerMargins;
    buildUI(rowSpacing);
}

void TxChainSection::buildUI(int rowSpacing)
{
    // The column the applet puts its rows in, with the applet's own
    // contents margins. The rows below are added to it unchanged.
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(m_outerMargins);
    vbox->setSpacing(rowSpacing);

    // ── 4e. TX-processing quick toggles: [LEV] [EQ] [CFC] ──────────────────
    // Phase 3M-3a-i Batch 2 (Task F): introduced the row of 3 (LEV/EQ/PROC).
    // Phase 3M-3a-ii Batch 6 (Task A + F, then post-bench cleanup):
    // PROC was promoted in Batch 6, then dropped here in the cleanup pass —
    // PhoneCwApplet already had a wired PROC button + slider sitting un-wired
    // since 3I-3 (NyiOverlay-marked).  Two PROC controls confused users.
    // Row is now 3 buttons (LEV / EQ / CFC); PROC lives on PhoneCwApplet.
    // All three share the same VOX/MON styling family (compact 22-px-tall,
    // expanding width, green-checked LED look).
    //
    //   LEV  — checkable, bidirectional with TransmitModel::txLevelerOn.
    //   EQ   — checkable, bidirectional with TransmitModel::txEqEnabled.
    //          Left-click toggles.  Right-click → TxEqDialog (3M-3a-i Batch 3).
    //   CFC  — checkable, bidirectional with TransmitModel::cfcEnabled.
    //          Left-click toggles.  Right-click → modeless TxCfcDialog
    //          (10-band per-band CFC editor; mirrors Thetis frmCFCConfig
    //          [v2.10.3.13]).
    //
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        const QString btnStyle = Style::buttonBaseStyle()
            + Style::greenCheckedStyle();

        m_levBtn = new QPushButton(QStringLiteral("LEV"), this);
        m_levBtn->setCheckable(true);
        m_levBtn->setFixedHeight(22);
        m_levBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_levBtn->setStyleSheet(btnStyle);
        m_levBtn->setAccessibleName(QStringLiteral("TX Leveler enable"));
        m_levBtn->setObjectName(QStringLiteral("TxLevButton"));
        m_levBtn->setToolTip(QStringLiteral(
            "TX Leveler: slow speech-leveling AGC. Improves intelligibility on weak speech."));
        row->addWidget(m_levBtn, 1);

        m_eqBtn = new QPushButton(QStringLiteral("EQ"), this);
        m_eqBtn->setCheckable(true);
        m_eqBtn->setFixedHeight(22);
        m_eqBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_eqBtn->setStyleSheet(btnStyle);
        m_eqBtn->setAccessibleName(QStringLiteral("TX EQ enable"));
        m_eqBtn->setObjectName(QStringLiteral("TxEqButton"));
        m_eqBtn->setToolTip(QStringLiteral(
            "TX 10-band graphic EQ.  Left-click to toggle.\n"
            "Right-click to open the EQ dialog."));
        // Custom context-menu policy so right-click hits a slot (not the
        // default menu).
        m_eqBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        row->addWidget(m_eqBtn, 1);

        m_cfcBtn = new QPushButton(QStringLiteral("CFC"), this);
        m_cfcBtn->setCheckable(true);
        m_cfcBtn->setFixedHeight(22);
        m_cfcBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_cfcBtn->setStyleSheet(btnStyle);
        m_cfcBtn->setAccessibleName(QStringLiteral("Continuous Frequency Compressor enable"));
        m_cfcBtn->setObjectName(QStringLiteral("TxCfcButton"));
        m_cfcBtn->setToolTip(QStringLiteral(
            "CFC: 10-band continuous frequency compressor. Left-click to "
            "toggle. Right-click to open the CFC dialog."));
        // Right-click → modeless TxCfcDialog (mirrors EQ button pattern).
        m_cfcBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        row->addWidget(m_cfcBtn, 1);

        // TX audio check (Thetis chkTXAudioCheck). TX/Y is normal transmit;
        // TX/N runs the TX processing chain with no RF so EQ/CFC/leveler can
        // be heard through MON. The persistent option is in Settings.
        m_txCheckBtn = new QPushButton(QStringLiteral("TX/Y"), this);
        m_txCheckBtn->setCheckable(true);
        m_txCheckBtn->setFixedHeight(22);
        m_txCheckBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_txCheckBtn->setStyleSheet(Style::buttonBaseStyle()
                                    + Style::redCheckedStyle());
        m_txCheckBtn->setObjectName(QStringLiteral("TxAudioCheckButton"));
        m_txCheckBtn->setAccessibleName(QStringLiteral("TX audio check"));
        m_txCheckBtn->setToolTip(QStringLiteral(
            "TX audio check.\n"
            "TX/Y = normal transmit allowed.\n"
            "TX/N = run mic > EQ > CFC > leveler > ALC locally with NO RF. "
            "MOX/PTT/VOX are disabled; MON stays off until you turn it on."));
        row->addWidget(m_txCheckBtn, 1);

        vbox->addLayout(row);
    }

    // ── Stage quality badges (LEV/COMP, EQ, CFC, ALC) ───────────────────────
    // One small pill under each stage, aligned with the buttons above. It
    // shows the live reading and turns yellow when the stage is under-driven,
    // green when it sits in its useful range, red when it is overdriven.
    // Thetis shows the same readings on its TX meters; these make them
    // visible beside the controls that set them.
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        const auto makeBadge = [this](Stage stage,
                                      const QString& name,
                                      const QString& title,
                                      const QString& tip) {
            auto* badge = new BadgeLabel(QStringLiteral("--"), this);
            badge->setObjectName(name);
            badge->setAlignment(Qt::AlignCenter);
            badge->setFixedHeight(15);
            badge->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            badge->setAccessibleName(title);
            badge->setToolTip(tip + tr("\n\nClick for adjustments."));
            badge->setCursor(Qt::PointingHandCursor);
            badge->onClick = [this, stage] { emit adjustRequested(stage); };
            badge->setProperty("stageLevel", 0);
            badge->setStyleSheet(QStringLiteral(
                "QLabel { background: #333333; color: #9a9a9a;"
                " border-radius: 3px; font-size: 7.5pt; font-weight: 600;"
                " padding: 0 2px; }"));
            return badge;
        };

        m_levBadge = makeBadge(
            Stage::Lev,
            QStringLiteral("TxLevBadge"), tr("Leveler gain reduction"),
            tr("Leveler gain reduction (COMP), live while transmitting.\n"
               "Yellow: under 1 dB (not levelling).\n"
               "Green: 1 to 10 dB.\n"
               "Red: over 10 dB (too much compression)."));
        m_eqBadge = makeBadge(
            Stage::Eq,
            QStringLiteral("TxEqBadge"), tr("Post-EQ level"),
            tr("Post-EQ peak level, live while transmitting.\n"
               "Yellow: below -30 dB (too little drive).\n"
               "Green: -30 to -3 dB.\n"
               "Red: above -3 dB (close to clipping)."));
        m_cfcBadge = makeBadge(
            Stage::Cfc,
            QStringLiteral("TxCfcBadge"), tr("CFC gain reduction"),
            tr("CFC gain reduction, live while transmitting.\n"
               "Yellow: under 1 dB (not compressing).\n"
               "Green: 1 to 8 dB.\n"
               "Red: over 8 dB (too much compression)."));
        m_alcBadge = makeBadge(
            Stage::Alc,
            QStringLiteral("TxAlcBadge"), tr("ALC gain reduction"),
            tr("ALC gain reduction, live while transmitting.\n"
               "Yellow: 0 dB (no control; drive is low).\n"
               "Green: 0 to -6 dB.\n"
               "Red: below -6 dB (ALC is holding the drive down)."));

        row->addWidget(m_levBadge, 1);
        row->addWidget(m_eqBadge, 1);
        row->addWidget(m_cfcBadge, 1);
        row->addWidget(m_alcBadge, 1);

        vbox->addLayout(row);
    }

    // ── Profile combo row (full width) ──────────────────────────────────────
    // Tune Mode combo removed (ATU phase, no plan yet).
    // (AetherSDR TxApplet.cpp:131–153)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        // ── Phase 3M-1c J.1 ─ TX Profile combo ─────────────────────────────────
        // Populated from MicProfileManager via setMicProfileManager().
        // Right-click → emit txProfileMenuRequested (mirrors Thetis
        // comboTXProfile_MouseDown at console.cs:44519-44522 [v2.10.3.13]).
        m_profileCombo = new QComboBox(this);
        m_profileCombo->addItem(QStringLiteral("Default"));
        m_profileCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_profileCombo->setFixedHeight(22);
        applyComboStyle(m_profileCombo);
        m_profileCombo->setAccessibleName(QStringLiteral("TX profile"));
        m_profileCombo->setToolTip(QStringLiteral(
            "TX Profile: left-click to switch.  Right-click to edit "
            "(Setup → Audio → TX Profile)."));
        // Custom context-menu policy so right-click emits
        // customContextMenuRequested instead of the default popup.
        m_profileCombo->setContextMenuPolicy(Qt::CustomContextMenu);
        row->addWidget(m_profileCombo, 1);

        vbox->addLayout(row);
    }

    // The parts a container copy hides and orders (SectionParts).
    m_parts.add(QStringLiteral("lev"), QStringLiteral("LEV button"), 0, {m_levBtn});
    m_parts.add(QStringLiteral("eq"), QStringLiteral("EQ button"), 0, {m_eqBtn});
    m_parts.add(QStringLiteral("cfc"), QStringLiteral("CFC button"), 0, {m_cfcBtn});
    m_parts.add(QStringLiteral("txcheck"), QStringLiteral("TX audio check"), 0, {m_txCheckBtn});
    m_parts.add(QStringLiteral("profile"), QStringLiteral("TX profile"), -1, {m_profileCombo});
}

// Colour band for a badge: 0 blank/idle, 1 low (yellow), 2 good (green),
// 3 overdriven (red). Styling happens only when the text or band changes,
// because this runs on the 100 ms meter tick.
namespace {

void paintBadge(QLabel* badge, const QString& prefix, const QString& value,
                int level)
{
    if (badge == nullptr) {
        return;
    }
    const QString text = value.isEmpty() ? prefix
                                         : prefix + QLatin1Char(' ') + value;
    if (badge->text() != text) {
        badge->setText(text);
    }
    if (badge->property("stageLevel").toInt() == level
        && badge->property("stageText").toString() == text) {
        return;
    }
    badge->setProperty("stageLevel", level);
    badge->setProperty("stageText", text);
    QString background;
    QString foreground;
    switch (level) {
    case 1:  background = QStringLiteral("#8a6d00"); foreground = QStringLiteral("#ffffff"); break;
    case 2:  background = QStringLiteral("#1f7a34"); foreground = QStringLiteral("#ffffff"); break;
    case 3:  background = QStringLiteral("#b3261e"); foreground = QStringLiteral("#ffffff"); break;
    default: background = QStringLiteral("#333333"); foreground = QStringLiteral("#9a9a9a"); break;
    }
    badge->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: %2; border-radius: 3px;"
        " font-size: 7.5pt; font-weight: 600; padding: 0 2px; }")
        .arg(background, foreground));
}

} // namespace

void TxChainSection::setStageReadings(const StageReadings& readings)
{
    constexpr double kNoReading = -300.0;
    const auto whole = [](double value) {
        return QString::number(qRound(value));
    };

    // LEV/COMP: the leveler's gain reduction, positive dB.
    {
        QString value;
        int level = 0;
        if (readings.keyed && readings.levOn
            && readings.compReductionDb > kNoReading) {
            value = whole(readings.compReductionDb);
            level = readings.compReductionDb > 10.0 ? 3
                  : readings.compReductionDb < 1.0  ? 1
                                                    : 2;
        }
        paintBadge(m_levBadge, QStringLiteral("LEV"), value, level);
    }

    // EQ: post-EQ peak level, dBFS (negative).
    {
        QString value;
        int level = 0;
        if (readings.keyed && readings.eqOn && readings.eqDb > kNoReading) {
            value = whole(readings.eqDb);
            level = readings.eqDb > -3.0  ? 3
                  : readings.eqDb < -30.0 ? 1
                                          : 2;
        }
        paintBadge(m_eqBadge, QStringLiteral("EQ"), value, level);
    }

    // CFC: gain reduction, positive dB.
    {
        QString value;
        int level = 0;
        if (readings.keyed && readings.cfcOn
            && readings.cfcReductionDb > kNoReading) {
            value = whole(readings.cfcReductionDb);
            level = readings.cfcReductionDb > 8.0 ? 3
                  : readings.cfcReductionDb < 1.0 ? 1
                                                  : 2;
        }
        paintBadge(m_cfcBadge, QStringLiteral("CFC"), value, level);
    }

    // ALC: gain reduction, negative dB (0 when the ALC is not acting).
    {
        QString value;
        int level = 0;
        if (readings.keyed && readings.alcReductionDb > kNoReading) {
            value = whole(readings.alcReductionDb);
            level = readings.alcReductionDb < -6.0 ? 3
                  : readings.alcReductionDb > -0.5 ? 1
                                                   : 2;
        }
        paintBadge(m_alcBadge, QStringLiteral("ALC"), value, level);
    }
}

} // namespace NereusSDR
