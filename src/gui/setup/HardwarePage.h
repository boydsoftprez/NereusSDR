#pragma once

// =================================================================
// src/gui/setup/HardwarePage.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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

#include "gui/SetupPage.h"

#include <QMap>
#include <QString>
#include <QVariant>
#include <QWidget>

class QTabWidget;

namespace NereusSDR {

class RadioModel;
struct RadioInfo;
struct BoardCapabilities;

class RadioInfoTab;
class AntennaAlexTab;
class OcOutputsTab;
class XvtrTab;
class PureSignalTab;
class DiversityTab;
class CalibrationTab;
class Hl2IoBoardTab;
class Hl2OptionsTab;
class BandwidthMonitorTab;

// HardwarePage — top-level "Hardware Config" entry in SetupDialog.
//
// Contains a nested QTabWidget that mirrors Thetis's Setup.cs Hardware Config
// sub-tabs. Tab visibility is capability-gated: call onCurrentRadioChanged()
// whenever the connected radio (or its BoardCapabilities) change. Tasks 19/20
// populate the individual tab widgets; Task 21 adds per-MAC persistence.
class HardwarePage : public SetupPage {
    Q_OBJECT
public:
    explicit HardwarePage(RadioModel* model, QWidget* parent = nullptr);
    ~HardwarePage() override;

#ifdef NEREUS_BUILD_TESTS
    enum class Tab {
        RadioInfo, AntennaAlex, OcOutputs, Xvtr, PureSignal,
        Diversity, Calibration, Hl2Options, Hl2IoBoard, BandwidthMonitor
    };
    bool isTabVisibleForTest(Tab t) const;
    QString tabTextForTest(Tab t) const;
#endif

public slots:
    // Reconciles tab visibility from BoardCapabilities flags and restores
    // persisted values for the incoming radio's MAC.
    void onCurrentRadioChanged(const RadioInfo& info);

private slots:
    // Write-through slot: stores tab setting under hardware/<mac>/<tabKey>/<key>.
    void onTabSettingChanged(const QString& tabKey,
                             const QString& key,
                             const QVariant& value);

private:
    // Extract entries whose key starts with prefix and return them with the
    // prefix stripped.
    static QMap<QString, QVariant> filterPrefix(const QMap<QString, QVariant>& map,
                                                 const QString& prefix);

    RadioModel*  m_model{nullptr};
    QTabWidget*  m_tabs{nullptr};

    // MAC address of the currently displayed radio; empty if none.
    QString      m_currentMac;

    RadioInfoTab*        m_radioInfoTab{nullptr};
    AntennaAlexTab*      m_antennaAlexTab{nullptr};
    OcOutputsTab*        m_ocOutputsTab{nullptr};
    XvtrTab*             m_xvtrTab{nullptr};
    PureSignalTab*       m_pureSignalTab{nullptr};
    DiversityTab*        m_diversityTab{nullptr};
    CalibrationTab*      m_paCalTab{nullptr};
    Hl2OptionsTab*       m_hl2OptionsTab{nullptr};
    Hl2IoBoardTab*       m_hl2IoTab{nullptr};
    BandwidthMonitorTab* m_bwMonitorTab{nullptr};

    int m_radioInfoIdx{-1};
    int m_antennaAlexIdx{-1};
    int m_ocOutputsIdx{-1};
    int m_xvtrIdx{-1};
    int m_pureSignalIdx{-1};
    int m_diversityIdx{-1};
    int m_paCalIdx{-1};
    int m_hl2OptionsIdx{-1};
    int m_hl2IoIdx{-1};
    int m_bwMonitorIdx{-1};
};

} // namespace NereusSDR
