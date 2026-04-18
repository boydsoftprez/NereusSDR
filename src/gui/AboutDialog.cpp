// =================================================================
// src/gui/AboutDialog.cpp  (NereusSDR)
// =================================================================
//
// Design and layout inspired by AetherSDR (ten9876/AetherSDR, GPLv3):
//   src/gui/MainWindow.cpp (about-box section) and
//   src/gui/TitleBar.{h,cpp}. AetherSDR has no per-file headers;
//   project-level citation per docs/attribution/HOW-TO-PORT.md rule 6.
//
// Thetis lineage: the scrollable contributor list (kRoster, below)
// reproduces the roster from Thetis
// Project Files/Source/Console/frmAbout.Designer.cs:57-81 (the
// lstContributors .Items.AddRange block) verbatim. Reproducing a
// contributor roster is a data / attribution carry-forward, not a
// code port — the surrounding Qt6 dialog code is NereusSDR-original
// with layout patterns from AetherSDR (see citation above).
//
// no-port-check: contributor callsigns appear only as data in the
// displayed list; no Thetis code is translated here. AetherSDR
// design lineage is cited above per HOW-TO-PORT.md rule 6.
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
    version->setStyleSheet(QStringLiteral("color: #8899aa; font-size: 13px;"));
    mainLayout->addWidget(version);

    auto* buildInfo = new QLabel(
        QStringLiteral("Qt %1 · C++20 · Built %2")
            .arg(QString::fromUtf8(qVersion()),
                 QStringLiteral(__DATE__)),
        this);
    buildInfo->setAlignment(Qt::AlignCenter);
    buildInfo->setStyleSheet(QStringLiteral("color: #8899aa; font-size: 11px;"));
    mainLayout->addWidget(buildInfo);

    mainLayout->addSpacing(4);

    auto* author = new QLabel(QStringLiteral("Created by JJ Boyd · KG4VCF"), this);
    author->setAlignment(Qt::AlignCenter);
    author->setStyleSheet(QStringLiteral("color: #aabbcc; font-size: 12px;"));
    mainLayout->addWidget(author);

    // ── Divider ─────────────────────────────────────────────────────────
    auto* div1 = new QFrame(this);
    div1->setFrameShape(QFrame::HLine);
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
    origin->setStyleSheet(QStringLiteral(
        "color: #99aabb; font-size: 11px; padding-bottom: 6px;"));
    mainLayout->addWidget(origin);

    // Contributor list — reproduced verbatim from Thetis
    // Project Files/Source/Console/frmAbout.Designer.cs:57-81 (the
    // lstContributors .Items.AddRange block). Listing names is data, not
    // code; Thetis's own About presents the same roster. NereusSDR-scoped
    // contributors are appended after the upstream block, separated by a
    // blank line.
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
            "<a href=\"https://github.com/TAPR/OpenHPSDR-Protocol1-Programmers\">OpenHPSDR Protocol 1</a> &nbsp;·&nbsp; "
            "<a href=\"https://github.com/TAPR/OpenHPSDR-Protocol2-Programmers\">Protocol 2</a><br>"
            "<a href=\"https://community.apache-labs.com/index.php\">Apache-Labs Community</a> &nbsp;·&nbsp; "
            "<a href=\"https://apache-labs.com/\">Apache-Labs Home</a>"),
        this);
    links->setOpenExternalLinks(true);
    links->setWordWrap(true);
    links->setAlignment(Qt::AlignLeft);
    links->setStyleSheet(QStringLiteral(
        "color: #99aabb; font-size: 11px; padding: 2px 0;"));
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
        auto* card = new QLabel(
            QStringLiteral("<b>%1</b><br><span style=\"color:#aabbcc\">%2</span>")
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
    copyright->setStyleSheet(QStringLiteral("color: #667788; font-size: 11px;"));
    mainLayout->addWidget(copyright);

    auto* warranty = new QLabel(
        QStringLiteral("This program comes with ABSOLUTELY NO WARRANTY; "
                       "see sections 15-16 of the License for details."),
        this);
    warranty->setAlignment(Qt::AlignCenter);
    warranty->setStyleSheet(QStringLiteral("color: #667788; font-size: 11px;"));
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
    license->setStyleSheet(QStringLiteral("color: #667788; font-size: 11px;"));
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
    aiDisclosure->setStyleSheet(QStringLiteral("color: #667788; font-size: 11px;"));
    mainLayout->addWidget(aiDisclosure);

    auto* repoLink = new QLabel(
        QStringLiteral("<a href=\"https://github.com/boydsoftprez/NereusSDR\">"
                       "github.com/boydsoftprez/NereusSDR</a>"),
        this);
    repoLink->setAlignment(Qt::AlignCenter);
    repoLink->setOpenExternalLinks(true);
    repoLink->setStyleSheet(QStringLiteral("color: #667788; font-size: 11px;"));
    mainLayout->addWidget(repoLink);

    auto* hpsdr = new QLabel(
        QStringLiteral("HPSDR protocol © TAPR"), this);
    hpsdr->setAlignment(Qt::AlignCenter);
    hpsdr->setStyleSheet(QStringLiteral("color: #667788; font-size: 11px;"));
    mainLayout->addWidget(hpsdr);

    mainLayout->addSpacing(12);

    // ── OK button ───────────────────────────────────────────────────────
    auto* okBtn = new QPushButton(QStringLiteral("OK"), this);
    okBtn->setDefault(true);
    okBtn->setAutoDefault(false);
    okBtn->setFixedWidth(80);
    okBtn->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; color: %2; border-radius: 4px;"
                       " padding: 6px 16px; font-weight: bold; }"
                       "QPushButton:hover { background: #33c8e8; }")
            .arg(Style::kAccent, Style::kAppBg));
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);
}

} // namespace NereusSDR
