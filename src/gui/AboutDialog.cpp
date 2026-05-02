// =================================================================
// src/gui/AboutDialog.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/frmAbout.cs — original licence from
//     Thetis source is included below (paired file for the About
//     dialog class; carries the copyright / GPL / dual-licensing
//     header that governs this About-dialog content).
//   Project Files/Source/Console/frmAbout.Designer.cs:57-81 —
//     reproduces the lstContributors .Items.AddRange roster verbatim
//     in the kRoster array below. Upstream frmAbout.Designer.cs has
//     no top-of-file GPL header — project-level Thetis LICENSE
//     (GPLv2-or-later, Richard Samphire MW0LGE) applies via the
//     paired frmAbout.cs header reproduced below.
//
// Thetis version pin: [@501e3f5] (ramdor/Thetis archive commit
//   501e3f513f73f07742d7e1b85a0e9528bd14977d, "Archive project and
//   end active development"). Thetis has no tag at HEAD; cite uses
//   the seven-hex commit form per HOW-TO-PORT.md inline-cite rules.
//
// Design and layout patterns from AetherSDR (ten9876/AetherSDR,
//   GPLv3): src/gui/MainWindow.cpp (about-box section) and
//   src/gui/TitleBar.{h,cpp}. AetherSDR has no per-file headers;
//   project-level citation per HOW-TO-PORT.md rule 6.
//
// --- From Thetis Project Files/Source/Console/frmAbout.cs (verbatim header) ---
/*  frmAbout.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2025 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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
// --- End Thetis frmAbout.cs verbatim header ---
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Implemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted authoring via Anthropic
//                 Claude Code. Contributor list, copyright string,
//                 and §5(d) notice content are NereusSDR-specific.
//   2026-04-18 — Expanded contributor section to reproduce the full
//                 Thetis roster (from frmAbout.Designer.cs:57-81) in a
//                 scrollable list. Added Links section. Revised licence
//                 paragraph to reflect v2-or-later derived-file grant +
//                 v3 distribution. Added AI-assisted-translation
//                 disclosure. Layout fix: replaced adjustSize()+
//                 setFixedSize() with QLayout::SetFixedSize constraint
//                 so the QListWidget's 190px cap no longer clashes with
//                 following sections. Audit against Thetis source-file
//                 headers added DttSP authors (AB2KT, N4HY), Thomas
//                 Ascher, Pierre-Philippe Coupard, Richard Allen, and
//                 M0YGG to the copyright line, plus a DttSP→WDSP lineage
//                 sub-section in the roster. J.J. Boyd (KG4VCF),
//                 AI-assisted authoring via Anthropic Claude Code.
//   2026-04-18 — Promoted AboutDialog.cpp from the PROVENANCE
//                 "independently implemented" exclusion list into the
//                 derived-file table: the kRoster array reproduces
//                 Thetis's lstContributors expressive content verbatim
//                 (phrasings, order, blank-line separators, closing
//                 line), which is a creative-compilation carry-forward
//                 rather than plain factual attribution. Added Thetis
//                 frmAbout.cs verbatim header, version pin [@501e3f5],
//                 canonical inline cite above kRoster. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "AboutDialog.h"
#include "StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QApplication>
#include <QPixmap>
#include <QPushButton>
#include <QListWidget>

namespace NereusSDR {

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("About NereusSDR"));

    buildUI();

    // Let the layout dictate size; width is fixed via the layout constraint
    // inside buildUI(). QLayout::SetFixedSize lets the dialog grow vertically
    // to fit the roster and footer without the earlier adjustSize()+setFixedSize
    // race that clipped the QListWidget and overlapped Built With onto the list.
    layout()->setSizeConstraint(QLayout::SetFixedSize);
}

void AboutDialog::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    setFixedWidth(520);

    setStyleSheet(
        QStringLiteral("QDialog { background: %1; }"
                       "QLabel { color: %2; }"
                       "QLabel a { color: %3; text-decoration: none; }")
            .arg(Style::kAppBg, Style::kTextPrimary, Style::kAccent));

    // ── Header ──────────────────────────────────────────────────────────
    auto* icon = new QLabel(this);
    QPixmap pix(QStringLiteral(":/icons/NereusSDR.png"));
    if (!pix.isNull())
        icon->setPixmap(pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    icon->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(icon);
    mainLayout->addSpacing(8);

    auto* title = new QLabel(QStringLiteral("NereusSDR"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        QStringLiteral("font-size: 22px; font-weight: bold; color: %1;")
            .arg(Style::kAccent));
    mainLayout->addWidget(title);

    auto* version = new QLabel(
        QStringLiteral("v%1 — Cross-platform SDR Console")
            .arg(QCoreApplication::applicationVersion()),
        this);
    version->setAlignment(Qt::AlignCenter);
    version->setStyleSheet(
        QStringLiteral("color: %1; font-size: 13px;").arg(Style::kLabelMid));
    mainLayout->addWidget(version);

    auto* buildInfo = new QLabel(
        QStringLiteral("Qt %1 · C++20 · Built %2")
            .arg(QString::fromUtf8(qVersion()),
                 QStringLiteral(__DATE__)),
        this);
    buildInfo->setAlignment(Qt::AlignCenter);
    buildInfo->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px;").arg(Style::kLabelMid));
    mainLayout->addWidget(buildInfo);

    mainLayout->addSpacing(4);

    auto* author = new QLabel(QStringLiteral("Created by JJ Boyd · KG4VCF"), this);
    author->setAlignment(Qt::AlignCenter);
    // §D exception: #aabbcc — author byline; lighter blue-gray than kTitleText (#8aa8c0).
    author->setStyleSheet(QStringLiteral("color: #aabbcc; font-size: 12px;"));
    mainLayout->addWidget(author);

    // ── Divider ─────────────────────────────────────────────────────────
    auto* div1 = new QFrame(this);
    div1->setFrameShape(QFrame::HLine);
    // §D exception: #334455 — divider line; darker blue-gray than kBorderSubtle (#203040).
    div1->setStyleSheet(QStringLiteral(
        "QFrame { color: #334455; margin-top: 12px; margin-bottom: 12px; }"));
    mainLayout->addWidget(div1);

    // ── Standing on the Shoulders of Giants ─────────────────────────────
    auto* giantsHeading = new QLabel(
        QStringLiteral("Standing on the Shoulders of Giants"), this);
    giantsHeading->setStyleSheet(
        QStringLiteral("color: %1; font-size: 13px; font-weight: 600;")
            .arg(Style::kAccent));
    mainLayout->addWidget(giantsHeading);
    mainLayout->addSpacing(6);

    auto* origin = new QLabel(
        QStringLiteral(
            "NereusSDR descends from "
            "<a href=\"https://github.com/ramdor/Thetis\">Thetis</a> "
            "(MW0LGE's active fork of FlexRadio Systems' PowerSDR), "
            "adapts Qt6 / C++20 architecture from "
            "<a href=\"https://github.com/ten9876/AetherSDR\">AetherSDR</a> "
            "(KK7GWY), and embeds "
            "<a href=\"https://github.com/TAPR/OpenHPSDR-wdsp\">WDSP</a> "
            "(NR0V / TAPR). The roster below reproduces the Thetis "
            "upstream contributor list verbatim, with NereusSDR-specific "
            "attributions appended."),
        this);
    origin->setWordWrap(true);
    origin->setOpenExternalLinks(true);
    // §D exception: #99aabb — supplementary body text; midpoint between kTextSecondary
    // (#8090a0) and kTitleText (#8aa8c0), used for secondary prose in About sections.
    origin->setStyleSheet(QStringLiteral(
        "color: #99aabb; font-size: 11px; padding-bottom: 6px;"));
    mainLayout->addWidget(origin);

    // From Thetis Project Files/Source/Console/frmAbout.Designer.cs:57-81 [@501e3f5] —
    // lstContributors .Items.AddRange block, reproduced verbatim in kRoster
    // below (each entry prefixed with two spaces for NereusSDR presentation
    // hierarchy; content after the indent matches byte-for-byte). Thetis
    // frmAbout.cs verbatim header is at top of this file per HOW-TO-PORT.md
    // rule 2; frmAbout.Designer.cs itself has no per-file header.
    // NereusSDR-scoped contributors are appended after the upstream block,
    // separated by a blank line.
    auto* contribList = new QListWidget(this);
    contribList->setSelectionMode(QAbstractItemView::NoSelection);
    contribList->setFocusPolicy(Qt::NoFocus);
    contribList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    contribList->setFixedHeight(190);
    contribList->setStyleSheet(
        QStringLiteral(
            "QListWidget { background: %1; color: %2; border: 1px solid %3;"
            " border-radius: 4px; font-size: 11px; padding: 4px; }"
            "QListWidget::item { padding: 1px 4px; }")
            .arg(Style::kInsetBg, Style::kTextPrimary, Style::kBorderSubtle));

    static constexpr const char* kRoster[] = {
        // --- From Thetis frmAbout.Designer.cs:57-81 (verbatim) ---
        "Thetis / PowerSDR upstream (FlexRadio Systems, ramdor/Thetis):",
        "  NR0V, Warren (WDSP & too many other contributions to list)",
        "  G8NJJ, Laurence (G2, Andromeda & protocols)",
        "  N1GP, Rick (Firmware related changes)",
        "  W4WMT, Bryan (Resampler, VAC & cmASIO)",
        "  MI0BOT, Reid (Hermes Lite 2)",
        "  MW0LGE, Richie (UI & various)",
        "  W5WC, Doug (UI, ChannelMaster, various & Thetis naming)",
        "  W2PA, Chris (QSK & MIDI)",
        "  WD5Y, Joe (UI tweaks and fixes)",
        "  M0YGG, Andrew (MIDI & various)",
        "",
        "  VK6PH, Phil (Firmware, Protocols & other)",
        "  KD5TFD, Bill (Protocol 1 initial implementation, UI & various)",
        "  K5SO, Joe (Diversity Reception & firmware)",
        "  WA8YWQ, Dave (various)",
        "  KE9NS, Darrin (various)",
        "",
        "  WU2O, Scott (forum admin & ideas/feedback)",
        "  NC3Z, Gary (forum mod)",
        "  OE3IDE, Ernst (skins & primary tester)",
        "  W1AEX, Rob (skins & audio information)",
        "  DH1KLM, Sigi (midi, skins & UI improvements)",
        "",
        "  and indirectly, all the testers",
        // --- End verbatim Thetis block ---
        "",
        "AetherSDR (ten9876/AetherSDR):",
        "  KK7GWY, Jeremy (architecture, Qt6 patterns, GPU rendering)",
        "",
        "WDSP engine (descends from DttSP):",
        "  AB2KT, Frank Brickle (original DttSP DSP engine, 2004-2009)",
        "  N4HY, Bob McGwier (original DttSP DSP engine, 2004-2009)",
        "  NR0V, Warren (DttSP → WDSP evolution and current author)",
        "  G0ORX, John (Linux port / cross-platform shims)",
        "",
        "Hermes Lite 2 fork (mi0bot/OpenHPSDR-Thetis):",
        "  MI0BOT, Reid (HL2 integration, ChannelMaster variants)",
        "",
        "NereusSDR:",
        "  KG4VCF, J.J. Boyd (C++20 / Qt6 port, packaging, CI)",
    };

    for (const auto* entry : kRoster) {
        contribList->addItem(QString::fromUtf8(entry));
    }
    mainLayout->addWidget(contribList);

    // ── Divider ─────────────────────────────────────────────────────────
    auto* divLinks = new QFrame(this);
    divLinks->setFrameShape(QFrame::HLine);
    divLinks->setStyleSheet(QStringLiteral(
        "QFrame { color: #334455; margin-top: 12px; margin-bottom: 12px; }"));
    mainLayout->addWidget(divLinks);

    // ── Links ───────────────────────────────────────────────────────────
    auto* linksHeading = new QLabel(QStringLiteral("Links"), this);
    linksHeading->setStyleSheet(
        QStringLiteral("color: %1; font-size: 13px; font-weight: 600;")
            .arg(Style::kAccent));
    mainLayout->addWidget(linksHeading);
    mainLayout->addSpacing(6);

    auto* links = new QLabel(
        QStringLiteral(
            "<a href=\"https://github.com/boydsoftprez/NereusSDR/releases\">"
            "NereusSDR releases</a> &nbsp;·&nbsp; "
            "<a href=\"https://github.com/boydsoftprez/NereusSDR/tree/main/docs/attribution\">"
            "docs/attribution</a> &nbsp;·&nbsp; "
            "<a href=\"https://github.com/boydsoftprez/NereusSDR/issues\">Issues</a><br>"
            "<a href=\"https://github.com/ramdor/Thetis\">Thetis upstream</a> &nbsp;·&nbsp; "
            "<a href=\"https://github.com/mi0bot/OpenHPSDR-Thetis\">mi0bot fork (HL2)</a> &nbsp;·&nbsp; "
            "<a href=\"https://github.com/ten9876/AetherSDR\">AetherSDR</a><br>"
            "<a href=\"https://github.com/TAPR/OpenHPSDR-wdsp\">WDSP (TAPR)</a> &nbsp;·&nbsp; "
            "<a href=\"https://github.com/g0orx/wdsp\">g0orx/wdsp (POSIX shim)</a><br>"
            "<a href=\"https://github.com/TAPR/OpenHPSDR-Protocol1-Programmers\">OpenHPSDR Protocol 1</a> &nbsp;·&nbsp; "
            "<a href=\"https://github.com/TAPR/OpenHPSDR-Protocol2-Programmers\">OpenHPSDR Protocol 2</a><br>"
            "<a href=\"https://community.apache-labs.com/index.php\">Apache-Labs Community</a> &nbsp;·&nbsp; "
            "<a href=\"https://apache-labs.com/\">Apache-Labs Home</a>"),
        this);
    links->setOpenExternalLinks(true);
    links->setWordWrap(true);
    links->setAlignment(Qt::AlignLeft);
    links->setStyleSheet(QStringLiteral(
        "color: #99aabb; font-size: 11px; padding: 2px 0;"));  // §D exception: see origin label above
    mainLayout->addWidget(links);

    // ── Divider ─────────────────────────────────────────────────────────
    auto* div2 = new QFrame(this);
    div2->setFrameShape(QFrame::HLine);
    div2->setStyleSheet(QStringLiteral(
        "QFrame { color: #334455; margin-top: 12px; margin-bottom: 12px; }"));
    mainLayout->addWidget(div2);

    // ── Built With ──────────────────────────────────────────────────────
    auto* builtHeading = new QLabel(QStringLiteral("Built With"), this);
    builtHeading->setStyleSheet(
        QStringLiteral("color: %1; font-size: 13px; font-weight: 600;")
            .arg(Style::kAccent));
    mainLayout->addWidget(builtHeading);
    mainLayout->addSpacing(6);

    struct Library {
        const char* name;
        const char* desc;
    };

    static constexpr std::array kLibraries = {
        Library{"Qt 6",       "GUI, Networking & Audio"},
        Library{"FFTW3",      "Spectrum & Waterfall FFT"},
        Library{"WDSP v1.29", "DSP Engine"},
    };

    auto* cardRow = new QHBoxLayout();
    cardRow->setSpacing(10);

    for (const auto& lib : kLibraries) {
        // §D exception: #aabbcc — card subtitle blue-gray; lighter than kTitleText (#8aa8c0).
        auto* card = new QLabel(
            QStringLiteral("<b>%1</b><br><span style=\"color:#aabbcc\">%2</span>")  // §D exception
                .arg(QString::fromUtf8(lib.name),
                     QString::fromUtf8(lib.desc)),
            this);
        card->setAlignment(Qt::AlignCenter);
        card->setStyleSheet(
            QStringLiteral("background: %1; border-radius: 6px; padding: 10px 8px;"
                           " font-size: 11px; color: %2;")
                .arg(Style::kButtonBg, Style::kTextPrimary));
        card->setMinimumWidth(110);
        cardRow->addWidget(card);
    }

    mainLayout->addLayout(cardRow);

    // ── Divider ─────────────────────────────────────────────────────────
    auto* div3 = new QFrame(this);
    div3->setFrameShape(QFrame::HLine);
    div3->setStyleSheet(QStringLiteral(
        "QFrame { color: #334455; margin-top: 12px; margin-bottom: 12px; }"));
    mainLayout->addWidget(div3);

    // ── Footer ──────────────────────────────────────────────────────────
    // GPLv3 §5(d) "Appropriate Legal Notices" compliance: interactive
    // programs must display copyright, no-warranty, redistribute-under-
    // these-conditions, and a how-to-view-the-License notice. The four-
    // element block below satisfies §5(d) directly; it also satisfies
    // GPLv2 §2(c) for downstream recipients who elect the v2 grant
    // available via the "or later" clause in upstream Thetis source-file
    // headers. Hence the full four-element block below.
    // The copyright line names the principal copyright-holding individuals
    // whose code appears in the running binary. NereusSDR is a derivative
    // work; the full per-file contributor chain — including inline-mod
    // attributions — is carried in the source tree's file headers and
    // summarized in docs/attribution/ (THETIS-PROVENANCE.md,
    // aethersdr-contributor-index.md, WDSP-PROVENANCE.md, etc.). The
    // pointer routes interested readers to the authoritative chain.
    auto* copyright = new QLabel(
        QStringLiteral(
            "Copyright © 2004-2026 "
            "Frank Brickle (AB2KT), Bob McGwier (N4HY), "
            "FlexRadio Systems, Thomas Ascher, "
            "Pierre-Philippe Coupard, Richard Allen, "
            "Bill Tracey (KD5TFD), Doug Wigley (W5WC), "
            "Richard Samphire (MW0LGE), Warren Pratt (NR0V), "
            "John Melton (G0ORX/N6LYT), "
            "Phil Harman (VK6APH), Chris Codella (W2PA), "
            "Laurence Barker (G8NJJ), Reid Campbell (MI0BOT), "
            "Andrew Mansfield (M0YGG), Sigi (DH1KLM), "
            "Jeremy (KK7GWY), other Thetis / mi0bot / AetherSDR / WDSP / "
            "DttSP / OpenHPSDR contributors, and J.J. Boyd (KG4VCF). "
            "See source file headers and "
            "<a href=\"https://github.com/boydsoftprez/NereusSDR/tree/main/docs/attribution\">"
            "docs/attribution</a> for the full contributor chain."),
        this);
    copyright->setAlignment(Qt::AlignCenter);
    copyright->setWordWrap(true);
    copyright->setOpenExternalLinks(true);
    // §D exception: #667788 — legal footer text; intentionally subdued blue-gray,
    // darker than kTextInactive (#405060) and distinct from other text levels.
    // Used for all GPL notices, warranty, license links, and HPSDR credit below.
    copyright->setStyleSheet(QStringLiteral("color: #667788; font-size: 11px;"));
    mainLayout->addWidget(copyright);

    auto* warranty = new QLabel(
        QStringLiteral("This program comes with ABSOLUTELY NO WARRANTY; "
                       "see sections 15-16 of the License for details."),
        this);
    warranty->setAlignment(Qt::AlignCenter);
    warranty->setStyleSheet(QStringLiteral("color: #667788; font-size: 11px;"));  // §D exception: see copyright label above
    mainLayout->addWidget(warranty);

    auto* license = new QLabel(
        QStringLiteral("This is free software. Thetis-derived source files "
                       "carry the upstream "
                       "<a href=\"https://www.gnu.org/licenses/old-licenses/gpl-2.0.html\">"
                       "GNU General Public License version 2 or later</a> "
                       "grant; the combined NereusSDR work is distributed "
                       "under "
                       "<a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">GPL v3</a> "
                       "via the upstream \"or later\" clause. "
                       "See <a href=\"https://github.com/boydsoftprez/NereusSDR/blob/main/LICENSE\">LICENSE</a>, "
                       "<a href=\"https://github.com/boydsoftprez/NereusSDR/blob/main/LICENSE-DUAL-LICENSING\">LICENSE-DUAL-LICENSING</a> "
                       "(Samphire / MW0LGE contributions only), and per-file "
                       "headers for authoritative terms."),
        this);
    license->setAlignment(Qt::AlignCenter);
    license->setOpenExternalLinks(true);
    license->setWordWrap(true);
    license->setStyleSheet(QStringLiteral("color: #667788; font-size: 11px;"));  // §D exception: see copyright label above
    mainLayout->addWidget(license);

    auto* aiDisclosure = new QLabel(
        QStringLiteral("Portions of NereusSDR's source were translated from "
                       "upstream C# (Thetis) and Qt6 / C++ (AetherSDR) with "
                       "AI-assisted authoring (Anthropic Claude Code). "
                       "Per-file headers disclose which translations are "
                       "AI-assisted."),
        this);
    aiDisclosure->setAlignment(Qt::AlignCenter);
    aiDisclosure->setWordWrap(true);
    aiDisclosure->setStyleSheet(QStringLiteral("color: #667788; font-size: 11px;"));  // §D exception: see copyright label above
    mainLayout->addWidget(aiDisclosure);

    auto* repoLink = new QLabel(
        QStringLiteral("<a href=\"https://github.com/boydsoftprez/NereusSDR\">"
                       "github.com/boydsoftprez/NereusSDR</a>"),
        this);
    repoLink->setAlignment(Qt::AlignCenter);
    repoLink->setOpenExternalLinks(true);
    repoLink->setStyleSheet(QStringLiteral("color: #667788; font-size: 11px;"));  // §D exception: see copyright label above
    mainLayout->addWidget(repoLink);

    auto* hpsdr = new QLabel(
        QStringLiteral("HPSDR protocol © TAPR"), this);
    hpsdr->setAlignment(Qt::AlignCenter);
    hpsdr->setStyleSheet(QStringLiteral("color: #667788; font-size: 11px;"));  // §D exception: see copyright label above
    mainLayout->addWidget(hpsdr);

    mainLayout->addSpacing(12);

    // ── OK button ───────────────────────────────────────────────────────
    auto* okBtn = new QPushButton(QStringLiteral("OK"), this);
    okBtn->setDefault(true);
    okBtn->setAutoDefault(false);
    okBtn->setFixedWidth(80);
    // §D exception: hover #33c8e8 — brightened accent variant; no canonical match.
    okBtn->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; color: %2; border-radius: 4px;"
                       " padding: 6px 16px; font-weight: bold; }"
                       "QPushButton:hover { background: #33c8e8; }")  // §D exception: accent-light hover
            .arg(Style::kAccent, Style::kAppBg));
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);
}

} // namespace NereusSDR
