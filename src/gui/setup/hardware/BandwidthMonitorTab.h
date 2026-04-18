#pragma once

// =================================================================
// src/gui/setup/hardware/BandwidthMonitorTab.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/ChannelMaster/bandwidth_monitor.h, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*  bandwidth_monitor.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2025 Richard Samphire, MW0LGE

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

// Developer note (NereusSDR Phase 3I): the .h exposes a low-level C byte-accounting
// API. NereusSDR wires only the static controls (throttle threshold, auto-pause
// toggle) and provides stub live labels (default text "— Mbps" / "0 events").
// The real-time feed from P1RadioConnection's bandwidth monitor is deferred to
// Phase 3L / Task 21.

#include <QVariant>
#include <QWidget>

class QCheckBox;
class QGroupBox;
class QLabel;
class QSpinBox;

namespace NereusSDR {

class RadioModel;
struct RadioInfo;
struct BoardCapabilities;

class BandwidthMonitorTab : public QWidget {
    Q_OBJECT
public:
    explicit BandwidthMonitorTab(RadioModel* model, QWidget* parent = nullptr);
    void populate(const RadioInfo& info, const BoardCapabilities& caps);
    void restoreSettings(const QMap<QString, QVariant>& settings);

signals:
    void settingChanged(const QString& key, const QVariant& value);

public slots:
    // Called by a future RadioModel / P1RadioConnection signal when live data
    // arrives. Phase 3I leaves this unconnected; Task 21 / Phase 3L wires it.
    void updateLiveStats(int currentMbps, int throttleEventCount);

private:
    RadioModel*  m_model{nullptr};

    // Live read-only labels
    QLabel*      m_currentRateLabel{nullptr};
    QLabel*      m_throttleEventLabel{nullptr};

    // User-adjustable controls
    QSpinBox*    m_throttleThresholdSpin{nullptr};
    QCheckBox*   m_autoPauseCheck{nullptr};
};

} // namespace NereusSDR
