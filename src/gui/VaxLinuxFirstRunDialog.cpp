// =================================================================
// src/gui/VaxLinuxFirstRunDialog.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original file — no Thetis port; no attribution-registry row.
//
// See VaxLinuxFirstRunDialog.h for the class overview.
//
// Design spec: docs/architecture/2026-04-23-linux-audio-pipewire-plan.md
// §8.2.
// =================================================================
//
//  Copyright (C) 2026 J.J. Boyd (KG4VCF)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program. If not, see <https://www.gnu.org/licenses/>.
// =================================================================
// Modification history (NereusSDR):
//   2026-04-24 — Written by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Anthropic Claude Code.
// =================================================================

#include "gui/VaxLinuxFirstRunDialog.h"
#include "StyleConstants.h"
#include "core/AppSettings.h"
#include "core/AudioEngine.h"

#include <QClipboard>
#include <QFile>
#include <QFont>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

// getuid() — POSIX; available on every Linux build.
#include <sys/types.h>
#include <unistd.h>

#if defined(Q_OS_LINUX)
#  include "core/audio/LinuxAudioBackend.h"
#endif

namespace NereusSDR {

namespace {

// Fixed dialog width — matches the spec §8.2 dialog shell width.
constexpr int kDialogWidth = 580;

// The three install command lines placed on the clipboard by "Copy commands".
// These are shell commands, deliberately NOT wrapped in tr() (they must not
// be translated — localized strings would produce invalid shell syntax).
const QString kCopyText =
    QStringLiteral("sudo apt install pipewire pipewire-pulse\n"
                   "sudo dnf install pipewire-pulseaudio\n"
                   "sudo pacman -S pipewire pipewire-pulse");

// ---------- shared style helpers -------------------------------------------

QString monoLabelStyle()
{
    return QStringLiteral(
        "QLabel { color: %1;"
        " font-family: 'ui-monospace','Menlo','Consolas',monospace;"
        " font-size: 11px; }")
        .arg(Style::kTextSecondary);
}

QString sectionLabelStyle()
{
    return QStringLiteral(
        "QLabel { color: %1; font-size: 12px; }")
        .arg(Style::kTextPrimary);
}

QString primaryButtonStyle()
{
    return QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2; color: %3;"
        " padding: 5px 14px; font-size: 11px; font-weight: bold;"
        " border-radius: 3px; }"
        "QPushButton:hover { background: %4; }")
        .arg(Style::kBlueBg, Style::kBlueBorder, Style::kBlueText,
             Style::kBlueHover);
}

QString neutralButtonStyle()
{
    return QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2; color: %3;"
        " padding: 5px 14px; font-size: 11px; font-weight: bold;"
        " border-radius: 3px; }"
        "QPushButton:hover { background: %4; }")
        .arg(Style::kButtonBg, Style::kBorder, Style::kTextPrimary,
             Style::kButtonAltHover);
}

}  // namespace

// ---------------------------------------------------------------------------
VaxLinuxFirstRunDialog::VaxLinuxFirstRunDialog(AudioEngine* eng,
                                               QWidget* parent)
    : QDialog(parent)
    , m_eng(eng)
{
    setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint
                   | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    setWindowTitle(tr("Linux audio bridge not available"));
    setFixedWidth(kDialogWidth);

    setStyleSheet(QStringLiteral(
        "QDialog { background: %1; border: 1px solid %2; }")
        .arg(Style::kAppBg, Style::kOverlayBorder));

    buildUi();

    layout()->setSizeConstraint(QLayout::SetFixedSize);
}

// ---------------------------------------------------------------------------
void VaxLinuxFirstRunDialog::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // ── Header bar ─────────────────────────────────────────────────────────
    {
        auto* header = new QFrame(this);
        header->setObjectName(QStringLiteral("dlgHdr"));
        header->setStyleSheet(QStringLiteral(
            "QFrame#dlgHdr {"
            " background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "  stop:0 %1, stop:0.5 %2, stop:1 %3);"
            " border-bottom: 1px solid %4; }")
            .arg(Style::kTitleGradTop, Style::kTitleGradMid,
                 Style::kTitleGradBot, Style::kTitleBorder));

        auto* headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(14, 8, 14, 8);
        headerLayout->setSpacing(8);

        auto* title = new QLabel(tr("Linux audio bridge not available"), header);
        title->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 13px; font-weight: bold; }")
            .arg(Style::kTextPrimary));
        headerLayout->addWidget(title);
        headerLayout->addStretch();

        // Platform badge
        auto* badge = new QLabel(QStringLiteral("LINUX"), header);
        badge->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; background: %2; border: 1px solid %1;"
            " padding: 1px 6px; border-radius: 2px;"
            " font-size: 10px; font-weight: bold; letter-spacing: 0.5px; }")
            .arg(Style::kAmberWarn, Style::kInsetBg));
        headerLayout->addWidget(badge);

        mainLayout->addWidget(header);
    }

    // ── Body ────────────────────────────────────────────────────────────────
    {
        auto* bodyFrame = new QFrame(this);
        auto* bodyLayout = new QVBoxLayout(bodyFrame);
        bodyLayout->setContentsMargins(18, 16, 18, 12);
        bodyLayout->setSpacing(10);

        // Intro paragraph
        auto* intro = new QLabel(
            tr("NereusSDR could not connect to a Linux audio daemon."),
            bodyFrame);
        intro->setWordWrap(true);
        intro->setStyleSheet(sectionLabelStyle());
        bodyLayout->addWidget(intro);

        // ── Detected state block ───────────────────────────────────────────
        {
            auto* stateFrame = new QFrame(bodyFrame);
            stateFrame->setStyleSheet(QStringLiteral(
                "QFrame { background: %1; border: 1px solid %2;"
                " border-left: 3px solid %3; border-radius: 3px; }")
                .arg(Style::kInsetBg, Style::kInsetBorder, Style::kAmberWarn));

            auto* stateLayout = new QVBoxLayout(stateFrame);
            stateLayout->setContentsMargins(12, 9, 12, 9);
            stateLayout->setSpacing(4);

            auto* stateTitle = new QLabel(tr("Detected state:"), stateFrame);
            stateTitle->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; font-size: 11px; font-weight: bold; }")
                .arg(Style::kTextPrimary));
            stateLayout->addWidget(stateTitle);

            // m_stateLabel carries the two dynamic detection lines.
            // refreshDetectionState() populates it. A fixed-width font is
            // used so the indented columns align cleanly.
            m_stateLabel = new QLabel(stateFrame);
            m_stateLabel->setTextFormat(Qt::RichText);
            m_stateLabel->setWordWrap(true);
            m_stateLabel->setStyleSheet(monoLabelStyle());
            stateLayout->addWidget(m_stateLabel);

            bodyLayout->addWidget(stateFrame);
        }

        // ── Install instructions block ─────────────────────────────────────
        {
            auto* installFrame = new QFrame(bodyFrame);
            installFrame->setStyleSheet(QStringLiteral(
                "QFrame { background: %1; border: 1px solid %2;"
                " border-radius: 3px; }")
                .arg(Style::kInsetBg, Style::kInsetBorder));

            auto* installLayout = new QVBoxLayout(installFrame);
            installLayout->setContentsMargins(12, 9, 12, 9);
            installLayout->setSpacing(6);

            auto* installTitle = new QLabel(
                tr("To enable VAX and audio output on Linux, install one of:"),
                installFrame);
            installTitle->setWordWrap(true);
            installTitle->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; font-size: 11px; }")
                .arg(Style::kTextPrimary));
            installLayout->addWidget(installTitle);

            // Distro install lines. Commands are NOT translated — they must
            // remain valid shell syntax in every locale.
            struct DistroRow {
                const char* label;
                const char* cmd1;
                const char* cmd2;
            };

            // clang-format off
            static const DistroRow kDistros[] = {
                {
                    "Ubuntu / Debian:",
                    "sudo apt install pipewire pipewire-pulse",
                    "(or) sudo apt install pulseaudio-utils"
                },
                {
                    "Fedora / RHEL:",
                    "sudo dnf install pipewire-pulseaudio",
                    "(or) sudo dnf install pulseaudio-utils"
                },
                {
                    "Arch:",
                    "sudo pacman -S pipewire pipewire-pulse",
                    nullptr
                },
            };
            // clang-format on

            for (const auto& row : kDistros) {
                // Distro label in amber accent color
                auto* distroLabel = new QLabel(
                    QString::fromUtf8(row.label), installFrame);
                distroLabel->setStyleSheet(QStringLiteral(
                    "QLabel { color: %1; font-size: 10px; font-weight: bold; }")
                    .arg(Style::kAmberText));
                installLayout->addWidget(distroLabel);

                // First command line
                auto* cmd1Label = new QLabel(
                    QStringLiteral("  ") + QString::fromUtf8(row.cmd1),
                    installFrame);
                cmd1Label->setStyleSheet(monoLabelStyle());
                installLayout->addWidget(cmd1Label);

                // Optional second command line
                if (row.cmd2) {
                    auto* cmd2Label = new QLabel(
                        QStringLiteral("  ") + QString::fromUtf8(row.cmd2),
                        installFrame);
                    cmd2Label->setStyleSheet(monoLabelStyle());
                    installLayout->addWidget(cmd2Label);
                }
            }

            bodyLayout->addWidget(installFrame);
        }

        // Closing note
        auto* note = new QLabel(
            tr("NereusSDR will run without audio routing in the meantime."),
            bodyFrame);
        note->setWordWrap(true);
        note->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; font-style: italic; }")
            .arg(Style::kTextSecondary));
        bodyLayout->addWidget(note);

        mainLayout->addWidget(bodyFrame);
    }

    // ── Footer (buttons) ────────────────────────────────────────────────────
    {
        auto* footer = new QFrame(this);
        footer->setObjectName(QStringLiteral("dlgFtr"));
        footer->setStyleSheet(QStringLiteral(
            "QFrame#dlgFtr { background: %1; border-top: 1px solid %2; }")
            .arg(Style::kPanelBg, Style::kInsetBorder));

        auto* footerLayout = new QHBoxLayout(footer);
        footerLayout->setContentsMargins(14, 10, 14, 10);
        footerLayout->setSpacing(8);

        footerLayout->addStretch();

        auto* copyBtn = new QPushButton(tr("Copy commands"), footer);
        copyBtn->setObjectName(QStringLiteral("btnCopyCommands"));
        copyBtn->setStyleSheet(neutralButtonStyle());
        connect(copyBtn, &QPushButton::clicked,
                this, &VaxLinuxFirstRunDialog::onCopyCommands);
        footerLayout->addWidget(copyBtn);

        auto* rescanBtn = new QPushButton(tr("Rescan after install"), footer);
        rescanBtn->setObjectName(QStringLiteral("btnRescan"));
        rescanBtn->setStyleSheet(neutralButtonStyle());
        connect(rescanBtn, &QPushButton::clicked,
                this, &VaxLinuxFirstRunDialog::onRescan);
        footerLayout->addWidget(rescanBtn);

        auto* dismissBtn = new QPushButton(tr("Dismiss"), footer);
        dismissBtn->setObjectName(QStringLiteral("btnDismiss"));
        dismissBtn->setStyleSheet(primaryButtonStyle());
        dismissBtn->setDefault(true);
        connect(dismissBtn, &QPushButton::clicked,
                this, &VaxLinuxFirstRunDialog::onDismiss);
        footerLayout->addWidget(dismissBtn);

        mainLayout->addWidget(footer);
    }

    // Populate detection state for the first time.
    refreshDetectionState();
}

// ---------------------------------------------------------------------------
void VaxLinuxFirstRunDialog::refreshDetectionState()
{
    if (!m_stateLabel) {
        return;
    }

#if defined(Q_OS_LINUX)
    // ── PipeWire socket ─────────────────────────────────────────────────
    const uid_t uid = getuid();
    const QString socketPath =
        QStringLiteral("/run/user/%1/pipewire-0").arg(static_cast<unsigned int>(uid));

    QFile socketFile(socketPath);
    const bool socketFound = socketFile.exists();

    const QString socketLine = socketFound
        ? QStringLiteral("  PipeWire socket: &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; "
                          "<span style='color:%1'>found at %2</span>")
              .arg(Style::kGreenText, socketPath.toHtmlEscaped())
        : QStringLiteral("  PipeWire socket: &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; "
                          "<span style='color:%1'>not found at %2</span>")
              .arg(Style::kAmberText, socketPath.toHtmlEscaped());

    // ── pactl binary ───────────────────────────────────────────────────
    const bool pactlFound =
        !QStandardPaths::findExecutable(QStringLiteral("pactl")).isEmpty();

    const QString pactlLine = pactlFound
        ? QStringLiteral("  pactl binary: &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
                          "<span style='color:%1'>found in $PATH</span>")
              .arg(Style::kGreenText)
        : QStringLiteral("  pactl binary: &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
                          "<span style='color:%1'>not in $PATH</span>")
              .arg(Style::kAmberText);

    m_stateLabel->setText(socketLine + QStringLiteral("<br/>") + pactlLine);

#else
    // Non-Linux build: show a static "not applicable" message so the dialog
    // at least renders correctly if opened by accident in a dev build.
    m_stateLabel->setText(
        QStringLiteral("  <span style='color:%1'>(Linux-only detection — "
                        "not applicable on this platform)</span>")
            .arg(Style::kTextSecondary));
#endif
}

// ---------------------------------------------------------------------------
void VaxLinuxFirstRunDialog::onCopyCommands()
{
    QGuiApplication::clipboard()->setText(kCopyText);
}

// ---------------------------------------------------------------------------
void VaxLinuxFirstRunDialog::onRescan()
{
#if defined(Q_OS_LINUX)
    if (m_eng) {
        m_eng->rescanLinuxBackend();

        // Refresh the detection-state section so the user sees updated probes.
        refreshDetectionState();

        // Auto-close if the rescan found a working backend.
        if (m_eng->linuxBackend() != LinuxAudioBackend::None) {
            accept();
            return;
        }
    }
#endif
    // Backend still None (or no engine) — stay open, updated state is visible.
}

// ---------------------------------------------------------------------------
void VaxLinuxFirstRunDialog::onDismiss()
{
    // Mark the first-run dialog as seen so Task 23's auto-open trigger
    // doesn't refire. Booleans in AppSettings are stored as "True"/"False".
    AppSettings::instance().setValue(
        QStringLiteral("Audio/LinuxFirstRunSeen"), QStringLiteral("True"));
    accept();
}

}  // namespace NereusSDR
