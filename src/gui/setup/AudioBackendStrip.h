#pragma once

// =================================================================
// src/gui/setup/AudioBackendStrip.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original diagnostic strip widget shown at the top of
// each Setup → Audio sub-page. No Thetis port, no attribution
// headers required (Qt widgets in Setup pages are NereusSDR-native).
//
// Task 17 (ubuntu-dev, 2026-04-24): Standalone widget displaying
// the detected Linux audio backend name (PipeWire / Pactl / None),
// plus Rescan and Open-logs action buttons. Task 22 wires this
// into SetupDialog's audio pages.
//
// Design spec: docs/architecture/2026-04-23-linux-audio-pipewire-plan.md
// §9.1 (Task 17).
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
//   2026-04-24 — Written by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Anthropic Claude Code.
// =================================================================

#include <QWidget>

class QLabel;
class QPushButton;

namespace NereusSDR { class AudioEngine; }

namespace NereusSDR {

// ---------------------------------------------------------------------------
// AudioBackendStrip
//
// Minimal horizontal strip that shows:
//   [bold backend name]  [version/socket details]  [Rescan]  [Open logs]
//
// On non-Linux builds the strip renders "No backend detected" and the
// Rescan button is disabled (the underlying AudioEngine APIs are
// Linux-only).
//
// Connect AudioEngine::linuxBackendChanged → refresh() so the strip
// auto-updates when a rescan finds a new backend.
// ---------------------------------------------------------------------------
class AudioBackendStrip : public QWidget {
    Q_OBJECT
public:
    explicit AudioBackendStrip(AudioEngine* eng, QWidget* parent = nullptr);

private slots:
    void refresh();
    void onRescanClicked();
    void onOpenLogsClicked();

private:
    AudioEngine*  m_eng;
    QLabel*       m_primaryLabel{nullptr};
    QLabel*       m_detailsLabel{nullptr};
    QPushButton*  m_rescan{nullptr};
    QPushButton*  m_openLogs{nullptr};
};

} // namespace NereusSDR
