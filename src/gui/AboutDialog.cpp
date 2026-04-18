// =================================================================
// src/gui/AboutDialog.cpp  (NereusSDR)
// =================================================================
//
// Design and layout inspired by AetherSDR (ten9876/AetherSDR, GPLv3):
//   src/gui/MainWindow.cpp (about-box section) and
//   src/gui/TitleBar.{h,cpp}. AetherSDR has no per-file headers;
//   project-level citation per docs/attribution/HOW-TO-PORT.md rule 6.
//
// No Thetis-derived code — Thetis's About is in console.cs and was not
// used as a source for this dialog.
//
// no-port-check: this file mentions Thetis sources (console.cs) and
// contributor callsigns only as data in the displayed contributor list;
// no Thetis code is ported here. AetherSDR design lineage is cited
// above per HOW-TO-PORT.md rule 6.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Implemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted authoring via Anthropic
//                 Claude Code. Contributor list, copyright string,
//                 and §5(d) notice content are NereusSDR-specific.
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

namespace NereusSDR {

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("About NereusSDR"));
    setFixedWidth(420);

    buildUI();

    // Size to content, prevent horizontal resize
    adjustSize();
    setFixedSize(size());
}

void AboutDialog::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(24, 20, 24, 20);

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

    struct Contributor {
        const char* name;
        const char* callsign;
        const char* project;
        const char* url;
    };

    static constexpr std::array kContributors = {
        Contributor{"Richie",              "MW0LGE",  "Thetis",
                    "https://github.com/ramdor/Thetis"},
        Contributor{"Jeremy",              "KK7GWY",  "AetherSDR",
                    "https://github.com/ten9876/AetherSDR"},
        Contributor{"Reid Campbell",       "MI0BOT",  "OpenHPSDR-Thetis (HL2)",
                    "https://github.com/mi0bot/OpenHPSDR-Thetis"},
        Contributor{"Dr. Warren C. Pratt", "NR0V",    "WDSP",
                    "https://github.com/TAPR/OpenHPSDR-wdsp"},
        Contributor{"John Melton",         "G0ORX",   "WDSP Linux Port",
                    "https://github.com/g0orx/wdsp"},
    };

    for (const auto& c : kContributors) {
        auto* row = new QLabel(
            QStringLiteral("%1, %2 — <a href=\"%3\">%4</a>")
                .arg(QString::fromUtf8(c.name),
                     QString::fromUtf8(c.callsign),
                     QString::fromUtf8(c.url),
                     QString::fromUtf8(c.project)),
            this);
        row->setOpenExternalLinks(true);
        row->setStyleSheet(QStringLiteral(
            "color: #99aabb; font-size: 12px; padding: 2px 0;"));
        mainLayout->addWidget(row);
    }

    // TAPR line (no individual callsign)
    auto* taprRow = new QLabel(
        QStringLiteral("TAPR / <a href=\"https://github.com/TAPR\">"
                       "OpenHPSDR</a> Community"),
        this);
    taprRow->setOpenExternalLinks(true);
    taprRow->setStyleSheet(QStringLiteral(
        "color: #99aabb; font-size: 12px; padding: 2px 0;"));
    mainLayout->addWidget(taprRow);

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
            "Copyright © 2004-2026 FlexRadio Systems, "
            "Bill Tracey (KD5TFD), Doug Wigley (W5WC), "
            "Richard Samphire (MW0LGE), Warren Pratt (NR0V), "
            "Phil Harman (VK6APH), Chris Codella (W2PA), "
            "Laurence Barker (G8NJJ), Reid Campbell (MI0BOT), DH1KLM, "
            "Jeremy (KK7GWY), other Thetis / mi0bot / AetherSDR / WDSP / "
            "OpenHPSDR contributors, and J.J. Boyd (KG4VCF). "
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
        QStringLiteral("This is free software, and you are welcome to "
                       "redistribute it under the terms of the "
                       "<a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">"
                       "GNU General Public License v3</a>; "
                       "see <a href=\"https://github.com/boydsoftprez/NereusSDR/blob/main/LICENSE\">LICENSE</a> "
                       "and <a href=\"https://github.com/boydsoftprez/NereusSDR/blob/main/LICENSE-DUAL-LICENSING\">LICENSE-DUAL-LICENSING</a> "
                       "for details."),
        this);
    license->setAlignment(Qt::AlignCenter);
    license->setOpenExternalLinks(true);
    license->setWordWrap(true);
    license->setStyleSheet(QStringLiteral("color: #667788; font-size: 11px;"));
    mainLayout->addWidget(license);

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
