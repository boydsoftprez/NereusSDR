#pragma once

// =================================================================
// src/gui/MainWindow.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Signal-routing hub, double-height status-bar layout, and
//                 TitleBar feature-request dialog ported from AetherSDR
//                 (ten9876/AetherSDR, GPLv3) src/gui/MainWindow.{h,cpp} and
//                 src/gui/TitleBar.{h,cpp}. AetherSDR has no per-file
//                 headers; project-level citation per docs/attribution/
//                 HOW-TO-PORT.md rule 6.
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
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
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

#include <QMainWindow>
#include <QLabel>
#include <QAction>
#include <QActionGroup>
#include <QTimer>

class QProgressDialog;
class QThread;
class QSplitter;
class QMenu;

namespace NereusSDR {

class RadioModel;
class ConnectionPanel;
class SupportDialog;
class WdspEngine;
class FFTEngine;
class SpectrumWidget;
class ClarityController;
class ContainerManager;
class MeterWidget;
class MeterPoller;
class TitleBar;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onConnectionStateChanged();
    void showConnectionPanel();
    void showSupportDialog();
    void showFeatureRequestDialog();
    void showFeatureRequestDialogImpl();

private:
    void buildUI();
    void buildMenuBar();
    void buildStatusBar();
    void applyDarkTheme();
    void tryAutoReconnect();
    void wireSliceToSpectrum();

    RadioModel* m_radioModel{nullptr};
    ConnectionPanel* m_connectionPanel{nullptr};
    SupportDialog* m_supportDialog{nullptr};

    // Status bar widgets (double-height AetherSDR design, 46px)
    QLabel* m_connStatusLabel{nullptr};
    QLabel* m_radioModelLabel{nullptr};
    QLabel* m_radioFwLabel{nullptr};
    QLabel* m_callsignLabel{nullptr};
    QLabel* m_utcTimeLabel{nullptr};
    QTimer* m_clockTimer{nullptr};
    QLabel* m_tnfLabel{nullptr};

    // Wisdom generation dialog (shown on first run)
    QProgressDialog* m_wisdomDialog{nullptr};

    // Spectrum display
    SpectrumWidget*     m_spectrumWidget{nullptr};
    FFTEngine*          m_fftEngine{nullptr};
    QThread*            m_fftThread{nullptr};
    ClarityController*  m_clarityController{nullptr};
    class StepAttenuatorController* m_stepAttController{nullptr};
    QLabel* m_adcOvlLabel{nullptr};

    // Re-entrancy guard: prevents centerChanged from firing a second
    // forceHardwareFrequency while frequencyChanged is already retuning the DDC
    bool m_handlingBandJump{false};

    // Task 17: auto-reconnect guard — prevents the background attempt from
    // interfering with a subsequent user-initiated Start Discovery.
    bool m_autoReconnectInProgress{false};

    // Container infrastructure (Phase 3G-1)
    ContainerManager* m_containerManager{nullptr};
    QSplitter* m_mainSplitter{nullptr};
    int m_hDelta{0};
    int m_vDelta{0};

    void createDefaultContainers();

    // Phase 3G-6 block 6: dynamic "Edit Container ▸" submenu,
    // populated from ContainerManager::allContainers() and rebuilt
    // whenever a container is added, removed, or retitled. Addresses
    // the block 4 review observation that there was no way to see
    // or manage already-created containers from the menu bar.
    QMenu* m_editContainerMenu{nullptr};
    void rebuildEditContainerSubmenu();
    void resetDefaultLayout();

    // Meter system (Phase 3G-2)
    MeterWidget* m_meterWidget{nullptr};
    MeterPoller* m_meterPoller{nullptr};
    void populateDefaultMeter();

    // Menu DSP actions (for overlay sync — Phase 3-UI)
    QAction* m_nrAction  = nullptr;
    QAction* m_nbAction  = nullptr;
    QAction* m_anfAction = nullptr;

    // Mode menu actions (12 modes, mutual exclusion via QActionGroup)
    QAction*      m_modeActions[12]  = {};
    QActionGroup* m_modeActionGroup  = nullptr;

    // AGC menu action group (Task 12)
    QActionGroup* m_agcGroup = nullptr;

    // Dark theme checkable action (Task 12)
    QAction* m_darkThemeAction = nullptr;

    // Status bar members (Task 13)
    QLabel*  m_cpuTopLabel{nullptr};   // "CPU: X.X%"
    QLabel*  m_cpuBotLabel{nullptr};   // "Mem: —"
    QTimer*  m_cpuTimer{nullptr};
    QVector<int> m_splitterSizesBeforeHide;  // saved splitter sizes for ☰ toggle

    // VFO flag widget (Phase 3E)
    class VfoWidget* m_vfoWidget{nullptr};

    // Applets (Phase 3-UI)
    class RxApplet* m_rxApplet{nullptr};
    class PhoneCwApplet* m_phoneCwApplet{nullptr};
    class EqApplet* m_eqApplet{nullptr};

    // Applets — Tasks 7-10 (NYI shells, hidden until Task 15 Container wiring)
    class DigitalApplet*    m_digitalApplet{nullptr};
    class PureSignalApplet* m_pureSignalApplet{nullptr};
    class DiversityApplet*  m_diversityApplet{nullptr};
    class CwxApplet*        m_cwxApplet{nullptr};
    class DvkApplet*        m_dvkApplet{nullptr};
    class CatApplet*        m_catApplet{nullptr};
    class TunerApplet*      m_tunerApplet{nullptr};

    // Spectrum overlay panel
    class SpectrumOverlayPanel* m_overlayPanel{nullptr};

    // Applet panel — scrollable content widget inside Container #0
    class AppletPanelWidget* m_appletPanel{nullptr};

    // Phase 3O Sub-Phase 10 Task 10c: host strip for the menu bar +
    // MasterOutputWidget. Owned by QMainWindow via setMenuWidget().
    TitleBar* m_titleBar{nullptr};
};

} // namespace NereusSDR
