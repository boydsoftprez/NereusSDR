// =================================================================
// src/gui/AmpViewWindow.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/AmpView.cs
//   Project Files/Source/Console/AmpView.Designer.cs
// original licences from Thetis source are included below.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-06 — Phase 3M-4 Task 9: created by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude
//                 Code.  Source-first 1:1 port of the Thetis modeless
//                 AmpView dialog (AmpView.cs + AmpView.Designer.cs)
//                 [v2.10.3.13].  Title bar reads "AmpView 1.0"
//                 verbatim.  ClientSize 564x401, MinimumSize 440x380.
//                 5 named chart series (Ref / MagAmp / PhsAmp /
//                 MagCorr / PhsCorr) feed from PureSignal::
//                 getDispBuffers (forwards TxChannel::getPSDisp →
//                 calcc.c:1058 [v2.10.3.13]).  4 toolbar checkboxes
//                 at exact Thetis x positions (chkAVShowGain @ 7,378
//                 / chkAVPhaseZoom @ 242,378 / chkAVLowRes @ 404,378
//                 / chkStayOnTop @ 490,378).  Render uses NereusSDR-
//                 native AmpViewChart custom QPainter widget instead
//                 of the upstream System.Windows.Forms.DataVisuali-
//                 zation chart (no QtCharts dependency added — design
//                 decision per phase3m-4-puresignal-design.md §15 #14).
//   2026-09-21 — Migrated the data feed to bounded owning PS3 snapshots;
//                 widgets no longer receive raw WDSP display buffers.
// =================================================================

/*  AmpView.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2025 Original authors
Copyright (C) 2020-2025 Richard Samphire MW0LGE

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


#pragma once

#include <QDialog>
#include <QPointer>

#include <cstdint>

class QCheckBox;
class QCloseEvent;
class QHideEvent;
class QLabel;
class QShowEvent;
class QTimer;
class QWidget;

namespace NereusSDR {

class AmpViewChart;
class PureSignal;
class PureSignalSessionFacade;
class RadioModel;
struct Ps3Snapshot;

// Modeless PS3 display.  It consumes the session-neutral facade and never
// holds a TxChannel or calls WDSP.  The legacy constructor shape remains for
// PsForm and standalone coordinator tests.
class AmpViewWindow final : public QDialog {
    Q_OBJECT

public:
    explicit AmpViewWindow(RadioModel* radioModel = nullptr,
                           PureSignal* pureSignal = nullptr,
                           QWidget* parent = nullptr);
    ~AmpViewWindow() override;

    // AmpView.cs:501-519 [v2.10.3.13] FixOnTop behavior.
    void setStayOnTopFromParent(bool on);

protected:
    void closeEvent(QCloseEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onShowGainToggled(bool on);
    void onPhaseZoomToggled(bool on);
    void onLowResToggled(bool on);
    void onStayOnTopToggled(bool on);
    void updateFreshnessLabel();

private:
    void buildUi();
    void restorePreferences();
    void connectUi();
    void restoreAndRepairGeometry();
    void repairOffscreenPosition();
    void persistGeometry() const;
    void setSubscribed(bool subscribed);
    void acceptSnapshot(const Ps3Snapshot& snapshot);
    void invalidateDisplay();
    void refreshAvailability();

    QPointer<PureSignalSessionFacade> m_facade;
    AmpViewChart* m_chart{nullptr};
    QLabel* m_displayStatus{nullptr};
    QCheckBox* m_chkShowGain{nullptr};
    QCheckBox* m_chkPhaseZoom{nullptr};
    QCheckBox* m_chkLowRes{nullptr};
    QCheckBox* m_chkStayOnTop{nullptr};
    QCheckBox* m_chkReference{nullptr};
    QCheckBox* m_chkMeasuredMagnitude{nullptr};
    QCheckBox* m_chkMeasuredPhase{nullptr};
    QCheckBox* m_chkCorrectionMagnitude{nullptr};
    QCheckBox* m_chkCorrectionPhase{nullptr};
    QTimer* m_presentationTimer{nullptr};
    std::uint64_t m_displayGeneration{0};
    std::uint64_t m_lastSequence{0};
    qint64 m_lastCaptureMs{0};
    bool m_subscribed{false};
};

} // namespace NereusSDR
