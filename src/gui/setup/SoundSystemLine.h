#pragma once

// =================================================================
// src/gui/setup/SoundSystemLine.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original "Sound system" status line at the top of
// Setup > Audio > Outputs. It replaces the AudioBackendStrip that sat on
// every Audio page (R-SPK-21, R-SPK-24, D15). No Thetis port.
//
// Design spec: docs/architecture/2026-10-05-radio-speaker-and-audio-setup-design.md
// (R-SPK-21 Outputs, R-SPK-24 Platforms); mockup audio-setup.html.
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
// =================================================================
// Modification history (NereusSDR):
//   2026-10-06 - Written for the radio speaker and Audio Setup plan, Task 9.
//                J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                Anthropic Claude Code.
// =================================================================

#include "core/audio/LinuxAudioBackend.h"

#include <QWidget>

class QLabel;

namespace NereusSDR {

class AudioEngine;

// SoundSystemLine (objectName "soundSystemLine")
//
//   [dot] Sound system: <text>
//
// Mac: "Core Audio". Windows: "Windows audio" and the driver the devices use
// unless one is picked. Linux: PipeWire, PulseAudio through pactl, or "None
// found" in red with what to start, following AudioEngine::linuxBackendChanged.
// The text label is "soundSystemText", the dot "soundSystemDot".
class SoundSystemLine : public QWidget {
    Q_OBJECT
public:
    enum class System { Mac, Windows, Linux };

    explicit SoundSystemLine(AudioEngine* engine, QWidget* parent = nullptr);

    // The system this build runs on (Q_OS_MAC / Q_OS_WIN / Q_OS_LINUX).
    static System thisSystem();
    // The words after "Sound system:" for a system (and, on Linux, the
    // backend found). Pure, so every platform's text is testable anywhere.
    static QString describe(System system, LinuxAudioBackend backend);
    // True when nothing was found (Linux, no backend): shown in red.
    static bool isProblem(System system, LinuxAudioBackend backend);

    // The words now shown after "Sound system:".
    QString text() const;
    bool showsProblem() const { return m_problem; }

public slots:
    // Re-reads the engine (on Linux, its detected backend).
    void refresh();

private:
    void show(System system, LinuxAudioBackend backend);

    AudioEngine* m_engine{nullptr};
    QLabel*      m_dot{nullptr};
    QLabel*      m_text{nullptr};
    QString      m_described;
    bool         m_problem{false};
};

} // namespace NereusSDR
