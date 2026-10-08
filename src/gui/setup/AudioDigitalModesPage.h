#pragma once

// =================================================================
// src/gui/setup/AudioDigitalModesPage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup > Audio > Digital modes page: audio for other
// apps, VAX then TCI (R-SPK-21). No Thetis port.
//
// Design spec: docs/architecture/2026-10-05-radio-speaker-and-audio-setup-design.md
// (R-SPK-21 Digital modes, R-SPK-22, R-SPK-24, D16); mockup audio-setup.html.
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
//   2026-10-06 - Written for the radio speaker and Audio Setup plan, Task 11
//                (R-SPK-21, R-SPK-22, R-SPK-24). J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "gui/RemoteReceiverAudioNote.h"
#include "gui/SetupPage.h"

class QLabel;

namespace NereusSDR {

class AudioTciPage;
class AudioVaxPage;

// ---------------------------------------------------------------------------
// Audio > Digital modes (SetupScope::ThisComputer)
//
//   VAX heading            (digitalModesVaxHeading)
//   VAX section            (AudioVaxPage: sentence, status line, Opus note,
//                           four channel cards, Detected virtual cables)
//   TCI heading            (digitalModesTciHeading)
//   TCI section            (AudioTciPage: sentence, Audio stream, Transmit)
//
// The page owns the title and the scroll area; the two sections are plain
// widgets it adopts.
// ---------------------------------------------------------------------------
class AudioDigitalModesPage : public SetupPage {
    Q_OBJECT
public:
    // Adopts `vax` and `tci` (they become this page's children).
    AudioDigitalModesPage(RadioModel* model, AudioVaxPage* vax, AudioTciPage* tci,
                          QWidget* parent = nullptr);
    // Builds both sections itself.
    explicit AudioDigitalModesPage(RadioModel* model, QWidget* parent = nullptr);

    AudioVaxPage* vaxSection() const { return m_vax; }
    AudioTciPage* tciSection() const { return m_tci; }

    // Forwards to the VAX section (R-R3-43 / R-R3-44).
    void setReceiverAudioNote(RemoteReceiverAudioNote note);

private:
    void buildPage();
    QLabel* makeHeading(const QString& text, const QString& objectName);

    AudioVaxPage* m_vax{nullptr};
    AudioTciPage* m_tci{nullptr};
};

} // namespace NereusSDR
