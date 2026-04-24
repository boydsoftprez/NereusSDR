#pragma once

// =================================================================
// src/gui/VaxLinuxFirstRunDialog.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original file — no Thetis port; no attribution-registry row.
//
// Modal dialog shown on first launch (or from Help → Diagnose audio
// backend, Task 19) when Linux audio detection returns None — i.e. no
// PipeWire socket is reachable and the pactl binary is absent from
// $PATH. Explains the missing dependency and offers distro-specific
// install commands. Auto-closes when Rescan after install detects a
// working backend.
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

#include <QDialog>

class QLabel;

namespace NereusSDR {

class AudioEngine;

// ---------------------------------------------------------------------------
// VaxLinuxFirstRunDialog
//
// QDialog subclass. Ctor takes (AudioEngine* eng, QWidget* parent = nullptr).
//
// The "Detected state" section is dynamic and is re-read every time the
// dialog opens (and again after Rescan), so re-opening from Help → Diagnose
// (Task 19) always shows fresh values, not first-run state.
//
// Three buttons:
//   Copy commands     — copies apt / dnf / pacman install lines to clipboard.
//   Rescan after install — calls AudioEngine::rescanLinuxBackend(); if the
//                          new backend is no longer None, auto-closes (accept).
//   Dismiss           — sets AppSettings("Audio/LinuxFirstRunSeen","True")
//                        and closes (accept).
// ---------------------------------------------------------------------------
class VaxLinuxFirstRunDialog : public QDialog {
    Q_OBJECT

public:
    explicit VaxLinuxFirstRunDialog(AudioEngine* eng,
                                    QWidget* parent = nullptr);

private slots:
    void onCopyCommands();
    void onRescan();
    void onDismiss();

private:
    // Rebuild the detection-state text and push it into m_stateLabel.
    // Called once from buildUi() and again from onRescan() so the display
    // is always fresh.
    void refreshDetectionState();

    void buildUi();

    AudioEngine* m_eng{nullptr};
    QLabel*      m_stateLabel{nullptr};
};

}  // namespace NereusSDR
