#pragma once

// =================================================================
// src/gui/setup/AsioSwitchAllDialog.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The prompt a Setup device card
// shows when a pick would put a second ASIO driver in use: NereusSDR runs
// one ASIO driver at a time, so it lists every role that moves with the
// pick and asks first (R-AUD-19).
//
// Design spec: docs/architecture/2026-10-08-native-audio-engines-design.md
// (R-AUD-19); mockup asio-setup-mockup.html ("One ASIO driver at a time").
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
//   2026-10-09: native audio plan Task 17 (R-AUD-19). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: Windows test fix (R-AUD-07, R-AUD-19): exported from the
//               GUI DLL, so a signal of it is found from outside the DLL
//               on Windows. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-09: native audio fix wave (R-AUD-19): askAsioSwitchAll(), the
//               prompt the Setup cards and the header's speakers menu
//               share. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#include "core/audio/AsioSession.h"
#include "core/audio/IAudioStreamHost.h"
#include "gui/NereusGuiExport.h"

#include <QDialog>
#include <QList>
#include <QPair>
#include <QString>

namespace NereusSDR {

// "NereusSDR can use one ASIO driver at a time. Switching <device> to
// <driver> also moves:", one line per move ("Speakers: Outputs 3-4"), the
// Cancel note, and the buttons "Switch all to <driver>" (objectName
// asioSwitchAllOk) and "Cancel" (asioSwitchAllCancel).  Each move is a
// name and the pair it moves to.
class NEREUS_GUI_EXPORT AsioSwitchAllDialog : public QDialog {
    Q_OBJECT
public:
    AsioSwitchAllDialog(const QString& device, const QString& driver,
                        const QList<QPair<QString, QString>>& moves, QWidget* parent);
};

// R-AUD-19: a pick that puts a second ASIO driver in use asks first. With
// `plan` moving other roles, shows the dialog for `role` switching to
// `driverName` (one line per move) and returns whether "Switch all" was
// pressed; with no move, true without asking. The caller applies the plan
// (AudioEngine::applyAsioSwitch) and saves its own pick; on false it
// writes nothing.
NEREUS_GUI_EXPORT bool askAsioSwitchAll(const AsioSwitchPlan& plan, AudioRole role,
                                        const QString& driverName, QWidget* parent);

} // namespace NereusSDR
