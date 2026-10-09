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
// =================================================================

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
class AsioSwitchAllDialog : public QDialog {
    Q_OBJECT
public:
    AsioSwitchAllDialog(const QString& device, const QString& driver,
                        const QList<QPair<QString, QString>>& moves, QWidget* parent);
};

} // namespace NereusSDR
